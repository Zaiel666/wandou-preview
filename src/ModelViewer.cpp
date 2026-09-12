#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <gl/GL.h>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

struct Vertex { float x,y,z,nx,ny,nz; };
struct Mesh { std::vector<Vertex> vertices; float color[4]={0.62f,0.66f,0.72f,1}; };
struct Viewer {
    HWND window=nullptr; HDC dc=nullptr; HGLRC gl=nullptr;
    std::vector<Mesh> meshes; std::wstring file; std::string error;
    float yaw=35, pitch=22, zoom=1, panX=0, panY=0;
    POINT last{}; int drag=0; bool ready=false;
} g;

static std::wstring BaseName(const std::wstring& p){size_t n=p.find_last_of(L"\\/");return n==std::wstring::npos?p:p.substr(n+1);}
static std::string Narrow(const std::wstring& w){int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),-1,nullptr,0,nullptr,nullptr);std::string s(n? n:1,'\0');if(n>1)WideCharToMultiByte(CP_UTF8,0,w.c_str(),-1,&s[0],n,nullptr,nullptr);if(!s.empty()&&s.back()=='\0')s.pop_back();return s;}
static void ResetView(){g.yaw=35;g.pitch=22;g.zoom=1;g.panX=g.panY=0;}

static bool LoadModel(const std::wstring& path){
    Assimp::Importer importer;
    const aiScene* scene=importer.ReadFile(Narrow(path),aiProcess_Triangulate|aiProcess_JoinIdenticalVertices|aiProcess_GenSmoothNormals|aiProcess_ImproveCacheLocality|aiProcess_PreTransformVertices|aiProcess_SortByPType|aiProcess_ValidateDataStructure);
    if(!scene||!scene->HasMeshes()){g.error=importer.GetErrorString();return false;}
    aiVector3D lo(1e30f),hi(-1e30f);
    for(unsigned m=0;m<scene->mNumMeshes;m++)for(unsigned v=0;v<scene->mMeshes[m]->mNumVertices;v++){auto p=scene->mMeshes[m]->mVertices[v];lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);}
    aiVector3D center=(lo+hi)*.5f;float extent=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});if(!(extent>1e-12f)){g.error="The model has no visible extent.";return false;}float scale=2.0f/extent;
    g.meshes.clear();g.meshes.reserve(scene->mNumMeshes);
    for(unsigned m=0;m<scene->mNumMeshes;m++){
        const aiMesh* src=scene->mMeshes[m];Mesh out;
        if(scene->HasMaterials()&&src->mMaterialIndex<scene->mNumMaterials){aiColor4D c;if(aiGetMaterialColor(scene->mMaterials[src->mMaterialIndex],AI_MATKEY_COLOR_DIFFUSE,&c)==AI_SUCCESS){out.color[0]=c.r;out.color[1]=c.g;out.color[2]=c.b;out.color[3]=std::max(.25f,c.a);}}
        for(unsigned f=0;f<src->mNumFaces;f++){const aiFace& face=src->mFaces[f];if(face.mNumIndices!=3)continue;for(unsigned k=0;k<3;k++){unsigned i=face.mIndices[k];aiVector3D p=(src->mVertices[i]-center)*scale;aiVector3D n=src->HasNormals()?src->mNormals[i]:aiVector3D(0,1,0);out.vertices.push_back({p.x,p.y,p.z,n.x,n.y,n.z});}}
        if(!out.vertices.empty())g.meshes.push_back(std::move(out));
    }
    if(g.meshes.empty()){g.error="The file contains no triangle geometry.";return false;}return true;
}

static bool SetupGL(HWND window){
    g.dc=GetDC(window);PIXELFORMATDESCRIPTOR pfd={sizeof(pfd),1,PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,PFD_TYPE_RGBA,32,0,0,0,0,0,0,0,0,0,0,0,24,8,0,PFD_MAIN_PLANE,0,0,0,0};
    int format=ChoosePixelFormat(g.dc,&pfd);if(!format||!SetPixelFormat(g.dc,format,&pfd))return false;g.gl=wglCreateContext(g.dc);return g.gl&&wglMakeCurrent(g.dc,g.gl);
}
static void Perspective(float fov,float aspect,float nearZ,float farZ){double top=nearZ*std::tan(fov*3.141592653589793/360.0),right=top*aspect;glFrustum(-right,right,-top,top,nearZ,farZ);}
static void DrawScene(int width,int height,bool swap=true){
    if(height<1)height=1;glViewport(0,0,width,height);glClearColor(.055f,.065f,.085f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glEnable(GL_NORMALIZE);glEnable(GL_MULTISAMPLE);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();Perspective(42.0f,float(width)/height,.05f,100);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glTranslatef(g.panX,g.panY,-3.6f/g.zoom);glRotatef(g.pitch,1,0,0);glRotatef(g.yaw,0,1,0);
    GLfloat ambient[]={.23f,.25f,.3f,1},light0[]={4,5,6,1},light1[]={-4,1,-3,1},white[]={.9f,.95f,1,1},cool[]={.28f,.38f,.55f,1};
    glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_LIGHT1);glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);glLightfv(GL_LIGHT0,GL_POSITION,light0);glLightfv(GL_LIGHT0,GL_DIFFUSE,white);glLightfv(GL_LIGHT1,GL_POSITION,light1);glLightfv(GL_LIGHT1,GL_DIFFUSE,cool);
    glEnable(GL_COLOR_MATERIAL);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);GLfloat spec[]={.45f,.48f,.52f,1};glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,spec);glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,42);
    for(const auto& mesh:g.meshes){glColor4fv(mesh.color);glBegin(GL_TRIANGLES);for(const auto& v:mesh.vertices){glNormal3f(v.nx,v.ny,v.nz);glVertex3f(v.x,v.y,v.z);}glEnd();}
    glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glColor4f(.3f,.38f,.48f,.22f);glBegin(GL_LINES);for(int i=-5;i<=5;i++){float n=i*.25f;glVertex3f(-1.25f,-1.02f,n);glVertex3f(1.25f,-1.02f,n);glVertex3f(n,-1.02f,-1.25f);glVertex3f(n,-1.02f,1.25f);}glEnd();glFlush();if(swap)SwapBuffers(g.dc);
}

static bool SaveBmp(const std::wstring& path,int size){
    std::vector<unsigned char> pixels(size*size*4);DrawScene(size,size,false);glPixelStorei(GL_PACK_ALIGNMENT,4);glReadPixels(0,0,size,size,0x80E1,GL_UNSIGNED_BYTE,pixels.data());
    BITMAPFILEHEADER fileHeader{};BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=size;info.biHeight=size;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=DWORD(pixels.size());fileHeader.bfType=0x4d42;fileHeader.bfOffBits=sizeof(fileHeader)+sizeof(info);fileHeader.bfSize=fileHeader.bfOffBits+info.biSizeImage;
    std::ofstream out(path,std::ios::binary);if(!out)return false;out.write((char*)&fileHeader,sizeof(fileHeader));out.write((char*)&info,sizeof(info));out.write((char*)pixels.data(),pixels.size());return bool(out);
}
static LRESULT CALLBACK WindowProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
    case WM_CREATE:if(!SetupGL(h))return -1;return 0;
    case WM_SIZE:if(g.ready){wglMakeCurrent(g.dc,g.gl);DrawScene(LOWORD(l),HIWORD(l));}return 0;
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);if(g.ready){RECT r;GetClientRect(h,&r);wglMakeCurrent(g.dc,g.gl);DrawScene(r.right,r.bottom);}EndPaint(h,&ps);return 0;}
    case WM_LBUTTONDOWN:case WM_MBUTTONDOWN:case WM_RBUTTONDOWN:g.drag=msg==WM_LBUTTONDOWN?1:(msg==WM_MBUTTONDOWN?2:3);g.last={GET_X_LPARAM(l),GET_Y_LPARAM(l)};SetCapture(h);return 0;
    case WM_MOUSEMOVE:if(g.drag){POINT p={GET_X_LPARAM(l),GET_Y_LPARAM(l)};int dx=p.x-g.last.x,dy=p.y-g.last.y;g.last=p;if(g.drag==1){g.yaw+=dx*.55f;g.pitch=std::max(-89.f,std::min(89.f,g.pitch+dy*.55f));}else{g.panX+=dx*.0035f/g.zoom;g.panY-=dy*.0035f/g.zoom;}InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_LBUTTONUP:case WM_MBUTTONUP:case WM_RBUTTONUP:g.drag=0;ReleaseCapture();return 0;
    case WM_MOUSEWHEEL:g.zoom*=GET_WHEEL_DELTA_WPARAM(w)>0?1.14f:1/1.14f;g.zoom=std::max(.08f,std::min(30.f,g.zoom));InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_LBUTTONDBLCLK:ResetView();InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_KEYDOWN:if(w==VK_HOME||w=='R'){ResetView();InvalidateRect(h,nullptr,FALSE);}else if(w==VK_ESCAPE)DestroyWindow(h);return 0;
    case WM_DESTROY:if(g.gl){wglMakeCurrent(nullptr,nullptr);wglDeleteContext(g.gl);}if(g.dc)ReleaseDC(h,g.dc);PostQuitMessage(0);return 0;
    }return DefWindowProc(h,msg,w,l);
}
static HWND MakeWindow(HINSTANCE instance,bool hidden,int size){WNDCLASSEXW wc={sizeof(wc),CS_OWNDC|CS_DBLCLKS,WindowProc,0,0,instance,LoadIconW(instance,MAKEINTRESOURCEW(101)),LoadCursor(nullptr,IDC_ARROW),(HBRUSH)(COLOR_WINDOW+1),nullptr,L"WandouModelPreview",LoadIconW(instance,MAKEINTRESOURCEW(101))};RegisterClassExW(&wc);DWORD style=hidden?WS_POPUP:WS_OVERLAPPEDWINDOW;return CreateWindowExW(0,wc.lpszClassName,(L"豌豆预览 · "+BaseName(g.file)).c_str(),style,CW_USEDEFAULT,CW_USEDEFAULT,size,size,nullptr,nullptr,instance,nullptr);}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    int count=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);if(count<5||std::wstring(args[1])!=L"--render"){MessageBoxW(nullptr,L"这是豌豆预览的资源管理器缩略图后台组件。安装后请在文件夹中使用大图标查看模型。",L"豌豆预览 0.1.0",MB_ICONINFORMATION);if(args)LocalFree(args);return 0;}
    g.file=args[2];std::wstring output=args[3];int size=std::max(64,std::min(1024,_wtoi(args[4])));
    if(!LoadModel(g.file)){LocalFree(args);return 2;}
    HWND window=MakeWindow(instance,true,size);if(!window){LocalFree(args);return 3;}g.window=window;g.ready=true;wglMakeCurrent(g.dc,g.gl);
    bool ok=SaveBmp(output,size);DestroyWindow(window);LocalFree(args);return ok?0:4;
}
