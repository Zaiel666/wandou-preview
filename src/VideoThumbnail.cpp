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

template<class T> static void Release(T*& value){if(value){value->Release();value=nullptr;}}

static bool SaveBmp(const std::wstring& path,IMFSample* sample,IMFMediaType* type){
    UINT32 width=0,height=0;if(FAILED(MFGetAttributeSize(type,MF_MT_FRAME_SIZE,&width,&height))||!width||!height||width>16384||height>16384)return false;
    IMFMediaBuffer* buffer=nullptr;if(FAILED(sample->ConvertToContiguousBuffer(&buffer)))return false;
    const size_t rowBytes=size_t(width)*4,total=rowBytes*height;if(total>1024ull*1024*1024){Release(buffer);return false;}std::vector<BYTE> pixels(total);bool copied=false;
    IMF2DBuffer* twoD=nullptr;if(SUCCEEDED(buffer->QueryInterface(IID_PPV_ARGS(&twoD)))){
        BYTE* scan=nullptr;LONG pitch=0;if(SUCCEEDED(twoD->Lock2D(&scan,&pitch))){for(UINT32 y=0;y<height;y++)memcpy(pixels.data()+size_t(y)*rowBytes,scan+ptrdiff_t(y)*pitch,rowBytes);twoD->Unlock2D();copied=true;}Release(twoD);
    }
    if(!copied){BYTE* data=nullptr;DWORD maximum=0,current=0;if(SUCCEEDED(buffer->Lock(&data,&maximum,&current))){LONG stride=LONG(rowBytes);UINT32 stored=0;if(SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE,&stored)))stride=LONG(stored);BYTE* first=stride<0?data+size_t(-stride)*(height-1):data;if(size_t(std::abs(stride))*height<=maximum){for(UINT32 y=0;y<height;y++)memcpy(pixels.data()+size_t(y)*rowBytes,first+ptrdiff_t(y)*stride,rowBytes);copied=true;}buffer->Unlock();}}
    Release(buffer);if(!copied)return false;
    BITMAPFILEHEADER fileHeader={};BITMAPINFOHEADER info={};info.biSize=sizeof(info);info.biWidth=LONG(width);info.biHeight=-LONG(height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=DWORD(total);fileHeader.bfType=0x4d42;fileHeader.bfOffBits=sizeof(fileHeader)+sizeof(info);fileHeader.bfSize=fileHeader.bfOffBits+DWORD(total);
    std::ofstream output(path,std::ios::binary);if(!output)return false;output.write(reinterpret_cast<const char*>(&fileHeader),sizeof(fileHeader));output.write(reinterpret_cast<const char*>(&info),sizeof(info));output.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());return bool(output);
}

static HRESULT ExtractFrame(const std::wstring& input,const std::wstring& output){
    IMFAttributes* attributes=nullptr;IMFSourceReader* reader=nullptr;IMFMediaType* requested=nullptr;IMFMediaType* current=nullptr;IMFSample* sample=nullptr;HRESULT hr=MFCreateAttributes(&attributes,2);
    if(SUCCEEDED(hr))hr=attributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING,TRUE);
    if(SUCCEEDED(hr))hr=attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,TRUE);
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
    HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);bool com=SUCCEEDED(hr);if(hr==RPC_E_CHANGED_MODE)hr=S_OK;if(SUCCEEDED(hr))hr=MFStartup(MF_VERSION,MFSTARTUP_LITE);bool media=SUCCEEDED(hr);if(SUCCEEDED(hr))hr=ExtractFrame(args[1],args[2]);if(media)MFShutdown();if(com)CoUninitialize();LocalFree(args);return SUCCEEDED(hr)?0:1;
}
