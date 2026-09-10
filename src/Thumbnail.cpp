#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thumbcache.h>
#include <propsys.h>
#include <new>

// Only this small adapter is distributed. Maxon's DLL is loaded from the user's installation.
const CLSID CLSID_Preview={0x3d15370f,0x5744,0x41bf,{0x85,0xd8,0x8d,0x49,0x85,0x50,0x9c,0xee}};
const CLSID CLSID_Maxon={0x2baa7283,0xb8ca,0x4993,{0x91,0xf7,0xce,0x75,0xb7,0x80,0xe1,0xf0}};
static LONG objects=0;
static HMODULE backend=nullptr;
static INIT_ONCE once=INIT_ONCE_STATIC_INIT;
BOOL CALLBACK LoadBackend(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t path[32768]; DWORD size=sizeof(path);
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\C4DQuickPreview",L"Backend",RRF_RT_REG_SZ,nullptr,path,&size)==ERROR_SUCCESS)
        backend=LoadLibraryExW(path,nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    return TRUE;
}
HRESULT NewBackend(IThumbnailProvider** result,IStream* stream) {
    InitOnceExecuteOnce(&once,LoadBackend,nullptr,nullptr);
    if(!backend)return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    auto get=reinterpret_cast<HRESULT(STDAPICALLTYPE*)(REFCLSID,REFIID,void**)>(GetProcAddress(backend,"DllGetClassObject"));
    if(!get)return E_NOINTERFACE;
    IClassFactory* factory=nullptr;
    HRESULT hr=get(CLSID_Maxon,IID_PPV_ARGS(&factory));
    if(FAILED(hr))return hr;
    IInitializeWithStream* init=nullptr;
    hr=factory->CreateInstance(nullptr,IID_PPV_ARGS(&init));factory->Release();
    if(FAILED(hr))return hr;
    LARGE_INTEGER start={};hr=stream->Seek(start,STREAM_SEEK_SET,nullptr);
    if(SUCCEEDED(hr))hr=init->Initialize(stream,STGM_READ);
    if(SUCCEEDED(hr))hr=init->QueryInterface(IID_PPV_ARGS(result));
    init->Release();return hr;
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
class Thumbnail final:public IThumbnailProvider,public IInitializeWithStream {
    LONG refs=1;IStream* stream=nullptr;
public:
    Thumbnail(){InterlockedIncrement(&objects);}~Thumbnail(){if(stream)stream->Release();InterlockedDecrement(&objects);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** value) override {
        if(!value)return E_POINTER;*value=nullptr;
        if(iid==IID_IUnknown||iid==__uuidof(IThumbnailProvider))*value=static_cast<IThumbnailProvider*>(this);
        else if(iid==__uuidof(IInitializeWithStream))*value=static_cast<IInitializeWithStream*>(this);
        else return E_NOINTERFACE;AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release() override{LONG n=InterlockedDecrement(&refs);if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE Initialize(IStream* value,DWORD) override{if(!value)return E_INVALIDARG;if(stream)return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);stream=value;stream->AddRef();return S_OK;}
    HRESULT STDMETHODCALLTYPE GetThumbnail(UINT size,HBITMAP* bitmap,WTS_ALPHATYPE* alpha) override {
        if(!bitmap||!alpha)return E_POINTER;*bitmap=nullptr;*alpha=WTSAT_UNKNOWN;if(!stream)return E_UNEXPECTED;
        IThumbnailProvider* provider=nullptr;HRESULT hr=NewBackend(&provider,stream);if(FAILED(hr))return hr;
        hr=provider->GetThumbnail(min(size,1024u),bitmap,alpha);provider->Release();
        if(SUCCEEDED(hr)&&*bitmap){Badge(*bitmap);*alpha=WTSAT_RGB;}return hr;
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
