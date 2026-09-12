#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <shellapi.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

template<class T> static void Release(T*& value){if(value){value->Release();value=nullptr;}}

static bool WriteBmp(const std::wstring& path,const BYTE* pixels,UINT32 width,UINT32 height){
    std::vector<BYTE> reduced;UINT32 longest=std::max(width,height);if(longest>768){UINT32 sourceWidth=width,sourceHeight=height;double ratio=768.0/longest;width=std::max(1u,UINT32(sourceWidth*ratio+.5));height=std::max(1u,UINT32(sourceHeight*ratio+.5));reduced.resize(size_t(width)*height*4);for(UINT32 y=0;y<height;y++){UINT32 sy=std::min(sourceHeight-1,UINT32(uint64_t(y)*sourceHeight/height));for(UINT32 x=0;x<width;x++){UINT32 sx=std::min(sourceWidth-1,UINT32(uint64_t(x)*sourceWidth/width));memcpy(reduced.data()+(size_t(y)*width+x)*4,pixels+(size_t(sy)*sourceWidth+sx)*4,4);}}pixels=reduced.data();}
    const size_t rowBytes=size_t(width)*4,total=rowBytes*height;BITMAPFILEHEADER fileHeader={};BITMAPINFOHEADER info={};info.biSize=sizeof(info);info.biWidth=LONG(width);info.biHeight=LONG(height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=DWORD(total);fileHeader.bfType=0x4d42;fileHeader.bfOffBits=sizeof(fileHeader)+sizeof(info);fileHeader.bfSize=fileHeader.bfOffBits+DWORD(total);
    std::ofstream output(path,std::ios::binary);if(!output)return false;output.write(reinterpret_cast<const char*>(&fileHeader),sizeof(fileHeader));output.write(reinterpret_cast<const char*>(&info),sizeof(info));for(UINT32 y=height;y>0;y--)output.write(reinterpret_cast<const char*>(pixels+size_t(y-1)*rowBytes),rowBytes);return bool(output);
}

static bool SaveBmp(const std::wstring& path,IMFSample* sample,IMFMediaType* type){
    UINT32 width=0,height=0;if(FAILED(MFGetAttributeSize(type,MF_MT_FRAME_SIZE,&width,&height))||!width||!height||width>16384||height>16384)return false;
    IMFMediaBuffer* buffer=nullptr;if(FAILED(sample->ConvertToContiguousBuffer(&buffer)))return false;
    const size_t rowBytes=size_t(width)*4,total=rowBytes*height;if(total>1024ull*1024*1024){Release(buffer);return false;}std::vector<BYTE> pixels(total);bool copied=false;
    IMF2DBuffer* twoD=nullptr;if(SUCCEEDED(buffer->QueryInterface(IID_PPV_ARGS(&twoD)))){
        BYTE* scan=nullptr;LONG pitch=0;if(SUCCEEDED(twoD->Lock2D(&scan,&pitch))){for(UINT32 y=0;y<height;y++)memcpy(pixels.data()+size_t(y)*rowBytes,scan+ptrdiff_t(y)*pitch,rowBytes);twoD->Unlock2D();copied=true;}Release(twoD);
    }
    if(!copied){BYTE* data=nullptr;DWORD maximum=0,current=0;if(SUCCEEDED(buffer->Lock(&data,&maximum,&current))){LONG stride=LONG(rowBytes);UINT32 stored=0;if(SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE,&stored)))stride=LONG(stored);BYTE* first=stride<0?data+size_t(-stride)*(height-1):data;if(size_t(std::abs(stride))*height<=maximum){for(UINT32 y=0;y<height;y++)memcpy(pixels.data()+size_t(y)*rowBytes,first+ptrdiff_t(y)*stride,rowBytes);copied=true;}buffer->Unlock();}}
    Release(buffer);if(!copied)return false;
    return WriteBmp(path,pixels.data(),width,height);
}

static std::string Utf8(const std::wstring& value){int count=WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,nullptr,0,nullptr,nullptr);std::string result(std::max(0,count),'\0');if(count>1){WideCharToMultiByte(CP_UTF8,0,value.c_str(),-1,&result[0],count,nullptr,nullptr);result.resize(count-1);}return result;}

static bool ExtractFrameFfmpeg(const std::wstring& input,const std::wstring& output){
    av_log_set_level(AV_LOG_QUIET);AVFormatContext* format=nullptr;AVCodecContext* codec=nullptr;AVPacket* packet=nullptr;AVFrame* frame=nullptr;SwsContext* scaler=nullptr;AVStream* stream=nullptr;const AVCodec* decoder=nullptr;int streamIndex=-1;bool saved=false;std::string path=Utf8(input);
    if(avformat_open_input(&format,path.c_str(),nullptr,nullptr)<0)goto done;
    if(avformat_find_stream_info(format,nullptr)<0)goto done;
    streamIndex=av_find_best_stream(format,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);if(streamIndex<0)goto done;stream=format->streams[streamIndex];decoder=avcodec_find_decoder(stream->codecpar->codec_id);if(!decoder)goto done;
    codec=avcodec_alloc_context3(decoder);if(!codec||avcodec_parameters_to_context(codec,stream->codecpar)<0||avcodec_open2(codec,decoder,nullptr)<0)goto done;
    {int64_t target=0;if(stream->duration!=AV_NOPTS_VALUE&&stream->duration>0)target=stream->duration/10;else if(format->duration!=AV_NOPTS_VALUE&&format->duration>0)target=av_rescale_q(format->duration/10,AV_TIME_BASE_Q,stream->time_base);if(target>0){av_seek_frame(format,streamIndex,target,AVSEEK_FLAG_BACKWARD);avcodec_flush_buffers(codec);}}
    packet=av_packet_alloc();frame=av_frame_alloc();if(!packet||!frame)goto done;
    for(int attempts=0;attempts<3000&&av_read_frame(format,packet)>=0;attempts++){
        if(packet->stream_index==streamIndex&&avcodec_send_packet(codec,packet)>=0){int decoded=avcodec_receive_frame(codec,frame);if(decoded>=0){int longest=std::max(frame->width,frame->height);double scale=longest>768?768.0/longest:1.0;int width=std::max(1,int(frame->width*scale+.5)),height=std::max(1,int(frame->height*scale+.5));std::vector<BYTE> pixels(size_t(width)*height*4);uint8_t* planes[4]={pixels.data(),nullptr,nullptr,nullptr};int strides[4]={width*4,0,0,0};scaler=sws_getContext(frame->width,frame->height,(AVPixelFormat)frame->format,width,height,AV_PIX_FMT_BGRA,SWS_BICUBIC,nullptr,nullptr,nullptr);if(scaler&&sws_scale(scaler,frame->data,frame->linesize,0,frame->height,planes,strides)>0)saved=WriteBmp(output,pixels.data(),width,height);break;}}
        av_packet_unref(packet);
    }
done:
    if(scaler)sws_freeContext(scaler);if(frame)av_frame_free(&frame);if(packet)av_packet_free(&packet);if(codec)avcodec_free_context(&codec);if(format)avformat_close_input(&format);return saved;
}

static HRESULT ExtractFrame(const std::wstring& input,const std::wstring& output){
    IMFAttributes* attributes=nullptr;IMFSourceReader* reader=nullptr;IMFMediaType* requested=nullptr;IMFMediaType* current=nullptr;IMFSample* sample=nullptr;HRESULT hr=MFCreateAttributes(&attributes,2);
    if(SUCCEEDED(hr))hr=attributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING,TRUE);
    if(SUCCEEDED(hr))hr=MFCreateSourceReaderFromURL(input.c_str(),attributes,&reader);
    if(SUCCEEDED(hr))hr=reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE);
    if(SUCCEEDED(hr))hr=reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM,TRUE);
    if(SUCCEEDED(hr))hr=MFCreateMediaType(&requested);
    if(SUCCEEDED(hr))hr=requested->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video);
    if(SUCCEEDED(hr))hr=requested->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_RGB32);
    if(SUCCEEDED(hr))hr=reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,nullptr,requested);
    if(SUCCEEDED(hr)){
        PROPVARIANT duration;PropVariantInit(&duration);if(SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,MF_PD_DURATION,&duration))&&duration.vt==VT_UI8&&duration.uhVal.QuadPart>0){PROPVARIANT position;PropVariantInit(&position);position.vt=VT_I8;position.hVal.QuadPart=std::min<ULONGLONG>(duration.uhVal.QuadPart/10,10ull*10000000);reader->SetCurrentPosition(GUID_NULL,position);}PropVariantClear(&duration);
    }
    for(int attempt=0;SUCCEEDED(hr)&&attempt<240&&!sample;attempt++){DWORD stream=0,flags=0;LONGLONG timestamp=0;hr=reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,&stream,&flags,&timestamp,&sample);if(flags&(MF_SOURCE_READERF_ERROR|MF_SOURCE_READERF_ENDOFSTREAM)){if(!sample)hr=MF_E_END_OF_STREAM;break;}}
    if(SUCCEEDED(hr))hr=reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,&current);
    if(SUCCEEDED(hr)&&!SaveBmp(output,sample,current))hr=E_FAIL;
    Release(sample);Release(current);Release(requested);Release(reader);Release(attributes);return hr;
}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    int count=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);if(count!=3){MessageBoxW(nullptr,L"这是豌豆预览的视频缩略图后台组件。",L"豌豆预览 0.2.0",MB_ICONINFORMATION);if(args)LocalFree(args);return 0;}
    HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);bool com=SUCCEEDED(hr);if(hr==RPC_E_CHANGED_MODE)hr=S_OK;if(SUCCEEDED(hr))hr=MFStartup(MF_VERSION,MFSTARTUP_LITE);bool media=SUCCEEDED(hr);if(SUCCEEDED(hr))hr=ExtractFrame(args[1],args[2]);if(media)MFShutdown();if(com)CoUninitialize();bool ok=SUCCEEDED(hr)||ExtractFrameFfmpeg(args[1],args[2]);LocalFree(args);return ok?0:1;
}
