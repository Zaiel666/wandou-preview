#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thumbcache.h>
#include <propsys.h>
#include <shlobj.h>
#include <new>
#include <algorithm>
#include <string>

const CLSID CLSID_ModelThumbnail={0x4c238e90,0xc239,0x4fe9,{0xad,0x0f,0x67,0x50,0x78,0x43,0x79,0xb2}};
static LONG objects=0;

static std::wstring Extension(const std::wstring& path){size_t dot=path.find_last_of(L'.');if(dot==std::wstring::npos)return L"3D";std::wstring e=path.substr(dot+1);for(auto& c:e)c=(wchar_t)towupper(c);return e.size()>4?L"3D":e;}
static void Badge(HBITMAP bitmap,const std::wstring& label){
    BITMAP info={};if(!GetObjectW(bitmap,sizeof(info),&info)||info.bmWidth<48||info.bmHeight<32)return;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return;HGDIOBJ old=SelectObject(dc,bitmap);
    int h=std::max(13,std::min(26,info.bmHeight/8)),w=std::max(h*2,int(label.size()*h*.62f)),pad=std::max(3,h/4);RECT rect={pad,info.bmHeight-pad-h,pad+w,info.bmHeight-pad};
    HBRUSH brush=CreateSolidBrush(RGB(45,52,64));FillRect(dc,&rect,brush);DeleteObject(brush);
    HPEN pen=CreatePen(PS_SOLID,1,RGB(158,176,198));HGDIOBJ oldPen=SelectObject(dc,pen);HGDIOBJ oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));Rectangle(dc,rect.left,rect.top,rect.right,rect.bottom);SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(pen);
    HFONT font=CreateFontW(-h*3/4,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");HGDIOBJ prev=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(235,241,248));DrawTextW(dc,label.c_str(),int(label.size()),&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(dc,prev);DeleteObject(font);SelectObject(dc,old);DeleteDC(dc);
}
static void Fit(HBITMAP* bitmap,UINT size){
    BITMAP info={};if(!GetObjectW(*bitmap,sizeof(info),&info)||!size)return;int longest=std::max(info.bmWidth,info.bmHeight);if(longest<=int(size))return;
    int w=std::max(1,int((long long)info.bmWidth*size/longest)),h=std::max(1,int((long long)info.bmHeight*size/longest));BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void* bits=nullptr;HBITMAP scaled=CreateDIBSection(nullptr,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!scaled)return;
    HDC src=CreateCompatibleDC(nullptr),dst=CreateCompatibleDC(nullptr);if(!src||!dst){if(src)DeleteDC(src);if(dst)DeleteDC(dst);DeleteObject(scaled);return;}HGDIOBJ a=SelectObject(src,*bitmap),b=SelectObject(dst,scaled);SetStretchBltMode(dst,HALFTONE);BOOL ok=StretchBlt(dst,0,0,w,h,src,0,0,info.bmWidth,info.bmHeight,SRCCOPY);SelectObject(src,a);SelectObject(dst,b);DeleteDC(src);DeleteDC(dst);if(ok){DeleteObject(*bitmap);*bitmap=scaled;}else DeleteObject(scaled);
}
static HRESULT RenderWorker(const std::wstring& file,UINT size,HBITMAP* bitmap){
    HMODULE self=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&RenderWorker),&self);wchar_t module[32768];if(!GetModuleFileNameW(self,module,32768))return E_FAIL;std::wstring exe(module);exe=exe.substr(0,exe.find_last_of(L"\\/"))+L"\\ModelQuickPreview.exe";
    wchar_t tempDir[MAX_PATH],tempFile[MAX_PATH];if(!GetTempPathW(MAX_PATH,tempDir)||!GetTempFileNameW(tempDir,L"mqp",0,tempFile))return E_FAIL;std::wstring command=L"\""+exe+L"\" --render \""+file+L"\" \""+tempFile+L"\" "+std::to_wstring(std::max(128u,std::min(768u,size)));
    STARTUPINFOW startup={sizeof(startup)};PROCESS_INFORMATION process={};HANDLE job=CreateJobObjectW(nullptr,nullptr);if(!job){DeleteFileW(tempFile);return E_FAIL;}JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_PROCESS_MEMORY;limits.ProcessMemoryLimit=1024ull*1024*1024;HRESULT hr=E_FAIL;
    if(SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))&&CreateProcessW(exe.c_str(),&command[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&startup,&process)){
        if(AssignProcessToJobObject(job,process.hProcess)){ResumeThread(process.hThread);if(WaitForSingleObject(process.hProcess,20000)==WAIT_OBJECT_0){DWORD code=1;GetExitCodeProcess(process.hProcess,&code);if(!code){*bitmap=(HBITMAP)LoadImageW(nullptr,tempFile,IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION);if(*bitmap)hr=S_OK;}}}
        TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,1500);CloseHandle(process.hThread);CloseHandle(process.hProcess);
    }CloseHandle(job);DeleteFileW(tempFile);return hr;
}
class Thumbnail final:public IThumbnailProvider,public IInitializeWithFile,public IInitializeWithItem{
    LONG refs=1;std::wstring file;
public:
    Thumbnail(){InterlockedIncrement(&objects);}~Thumbnail(){InterlockedDecrement(&objects);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** value)override{if(!value)return E_POINTER;*value=nullptr;if(iid==IID_IUnknown||iid==__uuidof(IThumbnailProvider))*value=static_cast<IThumbnailProvider*>(this);else if(iid==__uuidof(IInitializeWithFile))*value=static_cast<IInitializeWithFile*>(this);else if(iid==__uuidof(IInitializeWithItem))*value=static_cast<IInitializeWithItem*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&refs);}ULONG STDMETHODCALLTYPE Release()override{LONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE Initialize(LPCWSTR path,DWORD)override{if(!path)return E_INVALIDARG;if(!file.empty())return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);file=path;return S_OK;}
    HRESULT STDMETHODCALLTYPE Initialize(IShellItem* item,DWORD mode)override{if(!item)return E_INVALIDARG;PWSTR path=nullptr;HRESULT hr=item->GetDisplayName(SIGDN_FILESYSPATH,&path);if(SUCCEEDED(hr)){hr=Initialize(path,mode);CoTaskMemFree(path);}return hr;}
    HRESULT STDMETHODCALLTYPE GetThumbnail(UINT size,HBITMAP* bitmap,WTS_ALPHATYPE* alpha)override{if(!bitmap||!alpha)return E_POINTER;*bitmap=nullptr;*alpha=WTSAT_UNKNOWN;if(file.empty())return E_UNEXPECTED;HRESULT hr=RenderWorker(file,size,bitmap);if(SUCCEEDED(hr)&&*bitmap){Fit(bitmap,std::min(size,1024u));Badge(*bitmap,Extension(file));*alpha=WTSAT_RGB;}return hr;}
};
class Factory final:public IClassFactory{LONG refs=1;public:Factory(){InterlockedIncrement(&objects);}~Factory(){InterlockedDecrement(&objects);}HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** value)override{if(!value)return E_POINTER;*value=nullptr;if(iid!=IID_IUnknown&&iid!=IID_IClassFactory)return E_NOINTERFACE;*value=this;AddRef();return S_OK;}ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&refs);}ULONG STDMETHODCALLTYPE Release()override{LONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID iid,void** value)override{if(outer)return CLASS_E_NOAGGREGATION;auto object=new(std::nothrow)Thumbnail();if(!object)return E_OUTOFMEMORY;HRESULT hr=object->QueryInterface(iid,value);object->Release();return hr;}HRESULT STDMETHODCALLTYPE LockServer(BOOL lock)override{if(lock)InterlockedIncrement(&objects);else InterlockedDecrement(&objects);return S_OK;}};
extern "C" __declspec(dllexport) HRESULT STDAPICALLTYPE ModelGetClassObject(REFCLSID clsid,REFIID iid,void** value){if(!value)return E_POINTER;*value=nullptr;if(clsid!=CLSID_ModelThumbnail)return CLASS_E_CLASSNOTAVAILABLE;auto factory=new(std::nothrow)Factory();if(!factory)return E_OUTOFMEMORY;HRESULT hr=factory->QueryInterface(iid,value);factory->Release();return hr;}
extern "C" __declspec(dllexport) HRESULT STDAPICALLTYPE ModelCanUnloadNow(){return objects==0?S_OK:S_FALSE;}
