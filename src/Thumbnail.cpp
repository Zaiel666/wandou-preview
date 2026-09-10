#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thumbcache.h>
#include <propsys.h>
#include <shlobj.h>
#include <string>
#include <new>

// Only this small adapter is distributed. Maxon's DLL is loaded from the user's installation.
const CLSID CLSID_Preview={0x3d15370f,0x5744,0x41bf,{0x85,0xd8,0x8d,0x49,0x85,0x50,0x9c,0xee}};
const CLSID CLSID_Maxon={0x2baa7283,0xb8ca,0x4993,{0x91,0xf7,0xce,0x75,0xb7,0x80,0xe1,0xf0}};
static LONG objects=0;
// Explorer only launches our bounded worker; Maxon's parser runs outside Explorer.
HRESULT ExtractInWorker(const wchar_t* file, HBITMAP* bitmap) {
    wchar_t dll[32768];DWORD bytes=sizeof(dll);
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\C4DQuickPreview",L"Backend",RRF_RT_REG_SZ,nullptr,dll,&bytes)!=ERROR_SUCCESS)return E_FAIL;
    HMODULE self=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&ExtractInWorker),&self);
    wchar_t module[32768];if(!GetModuleFileNameW(self,module,32768))return E_FAIL;
    std::wstring exe(module);exe=exe.substr(0,exe.find_last_of(L"\\/"))+L"\\C4DQuickPreview.exe";
    wchar_t tempDir[MAX_PATH],tempFile[MAX_PATH];if(!GetTempPathW(MAX_PATH,tempDir)||!GetTempFileNameW(tempDir,L"cqp",0,tempFile))return E_FAIL;
    std::wstring command=L"\""+exe+L"\" --extract-bmp \""+dll+L"\" \""+file+L"\" \""+tempFile+L"\"";
    STARTUPINFOW startup={sizeof(startup)};PROCESS_INFORMATION process={};
    HRESULT hr=E_FAIL;
    HANDLE job=CreateJobObjectW(nullptr,nullptr);
    if(!job){DeleteFileW(tempFile);return E_FAIL;}
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_PROCESS_MEMORY;limits.ProcessMemoryLimit=512ull*1024*1024;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))){CloseHandle(job);DeleteFileW(tempFile);return E_FAIL;}
    if(CreateProcessW(exe.c_str(),&command[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&startup,&process)){
        if(AssignProcessToJobObject(job,process.hProcess)){
            ResumeThread(process.hThread);
            if(WaitForSingleObject(process.hProcess,12000)==WAIT_OBJECT_0){DWORD code=1;GetExitCodeProcess(process.hProcess,&code);if(!code){*bitmap=static_cast<HBITMAP>(LoadImageW(nullptr,tempFile,IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION));if(*bitmap)hr=S_OK;}}
        }
        // Closing the job also terminates a timed-out worker.
        TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,2000);
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
    }
    CloseHandle(job);DeleteFileW(tempFile);DeleteFileW((std::wstring(tempFile)+L".error.txt").c_str());return hr;
}
void Badge(HBITMAP bitmap) {
    BITMAP info={}; if(!GetObjectW(bitmap,sizeof(info),&info)||info.bmWidth<48||info.bmHeight<32)return;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return;
    HGDIOBJ old=SelectObject(dc,bitmap);
    int h=max(12,min(24,info.bmHeight/8)),w=h*2,pad=max(3,h/4);
    RECT rect={pad,info.bmHeight-pad-h,pad+w,info.bmHeight-pad};
    HBRUSH brush=CreateSolidBrush(RGB(40,84,165));FillRect(dc,&rect,brush);DeleteObject(brush);
    HFONT font=CreateFontW(-h*3/4,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    HGDIOBJ prev=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    DrawTextW(dc,L"C4D",3,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(dc,prev);DeleteObject(font);SelectObject(dc,old);DeleteDC(dc);
}
void Fit(HBITMAP* bitmap,UINT size){
    BITMAP info={};if(!GetObjectW(*bitmap,sizeof(info),&info)||!size)return;
    int longest=max(info.bmWidth,info.bmHeight);if(longest<=static_cast<int>(size))return;
    int w=max(1,static_cast<int>(static_cast<long long>(info.bmWidth)*size/longest)),h=max(1,static_cast<int>(static_cast<long long>(info.bmHeight)*size/longest));
    BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;HBITMAP scaled=CreateDIBSection(nullptr,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!scaled)return;
    HDC src=CreateCompatibleDC(nullptr),dst=CreateCompatibleDC(nullptr);if(!src||!dst){if(src)DeleteDC(src);if(dst)DeleteDC(dst);DeleteObject(scaled);return;}
    HGDIOBJ a=SelectObject(src,*bitmap),b=SelectObject(dst,scaled);SetStretchBltMode(dst,HALFTONE);SetBrushOrgEx(dst,0,0,nullptr);
    BOOL ok=StretchBlt(dst,0,0,w,h,src,0,0,info.bmWidth,info.bmHeight,SRCCOPY);SelectObject(src,a);SelectObject(dst,b);DeleteDC(src);DeleteDC(dst);
    if(ok){DeleteObject(*bitmap);*bitmap=scaled;}else DeleteObject(scaled);
}
class Thumbnail final:public IThumbnailProvider,public IInitializeWithItem,public IInitializeWithFile {
    LONG refs=1;std::wstring file;
public:
    Thumbnail(){InterlockedIncrement(&objects);}~Thumbnail(){InterlockedDecrement(&objects);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** value) override {
        if(!value)return E_POINTER;*value=nullptr;
        if(iid==IID_IUnknown||iid==__uuidof(IThumbnailProvider))*value=static_cast<IThumbnailProvider*>(this);
        else if(iid==__uuidof(IInitializeWithItem))*value=static_cast<IInitializeWithItem*>(this);
        else if(iid==__uuidof(IInitializeWithFile))*value=static_cast<IInitializeWithFile*>(this);
        else return E_NOINTERFACE;AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release() override{LONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE Initialize(LPCWSTR value,DWORD)override{if(!value)return E_INVALIDARG;if(!file.empty())return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);file=value;return S_OK;}
    HRESULT STDMETHODCALLTYPE Initialize(IShellItem* item,DWORD mode)override{if(!item)return E_INVALIDARG;PWSTR path=nullptr;HRESULT hr=item->GetDisplayName(SIGDN_FILESYSPATH,&path);if(SUCCEEDED(hr)){hr=Initialize(path,mode);CoTaskMemFree(path);}return hr;}
    HRESULT STDMETHODCALLTYPE GetThumbnail(UINT size,HBITMAP* bitmap,WTS_ALPHATYPE* alpha) override {
        if(!bitmap||!alpha)return E_POINTER;*bitmap=nullptr;*alpha=WTSAT_UNKNOWN;if(file.empty())return E_UNEXPECTED;
        HRESULT hr=ExtractInWorker(file.c_str(),bitmap);
        if(SUCCEEDED(hr)&&*bitmap){Fit(bitmap,min(size,1024u));Badge(*bitmap);*alpha=WTSAT_RGB;}return hr;
    }
};
class Factory final:public IClassFactory {
    LONG refs=1;
public:
    Factory(){InterlockedIncrement(&objects);}~Factory(){InterlockedDecrement(&objects);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** value)override{if(!value)return E_POINTER;*value=nullptr;if(iid!=IID_IUnknown&&iid!=IID_IClassFactory)return E_NOINTERFACE;*value=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release()override{LONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID iid,void** value)override{if(!value)return E_POINTER;*value=nullptr;if(outer)return CLASS_E_NOAGGREGATION;auto obj=new(std::nothrow) Thumbnail();if(!obj)return E_OUTOFMEMORY;HRESULT hr=obj->QueryInterface(iid,value);obj->Release();return hr;}
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock)override{if(lock)InterlockedIncrement(&objects);else InterlockedDecrement(&objects);return S_OK;}
};
extern "C" __declspec(dllexport) HRESULT STDAPICALLTYPE PreviewGetClassObject(REFCLSID clsid,REFIID iid,void** value){if(!value)return E_POINTER;*value=nullptr;if(clsid!=CLSID_Preview)return CLASS_E_CLASSNOTAVAILABLE;auto f=new(std::nothrow) Factory();if(!f)return E_OUTOFMEMORY;HRESULT hr=f->QueryInterface(iid,value);f->Release();return hr;}
extern "C" __declspec(dllexport) HRESULT STDAPICALLTYPE PreviewCanUnloadNow(){return objects==0?S_OK:S_FALSE;}
