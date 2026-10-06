#include "viewer.h"
#include <shellapi.h>
static HWND viewer;
static unsigned debugPort;
static PFNDVPCREATEVIEWER pluginCreate;
LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if(msg == WM_CREATE) { RECT r; GetClientRect(hwnd,&r); viewer=pluginCreate?pluginCreate(hwnd,&r,0):acs::CreateViewer(hwnd,r,0,debugPort); return viewer?0:-1; }
  if(msg == WM_SIZE && viewer) { MoveWindow(viewer,0,0,LOWORD(lp),HIWORD(lp),TRUE); return 0; }
  if(msg == WM_SETFOCUS && viewer) { SetFocus(viewer); return 0; }
  if(msg == WM_COPYDATA) {
    auto data=reinterpret_cast<COPYDATASTRUCT*>(lp);
    if(data->dwData==1 && data->cbData>=sizeof(wchar_t) && data->cbData<65536 && static_cast<wchar_t*>(data->lpData)[data->cbData/sizeof(wchar_t)-1]==0)
      return SendMessageW(viewer,DVPLUGINMSG_LOADW,0,reinterpret_cast<LPARAM>(data->lpData));
  }
  if(msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcW(hwnd,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  int argc=0; auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
  std::wstring file; bool usePlugin=false;
  for(int i=1;i<argc;i++) { if(std::wstring(argv[i])==L"--debug-port" && i+1<argc) debugPort=std::stoul(argv[++i]); else if(std::wstring(argv[i])==L"--plugin") usePlugin=true; else file=argv[i]; }
  LocalFree(argv);
  if(usePlugin) {
    wchar_t path[32768]; GetModuleFileNameW(instance,path,32768); std::wstring dll=path; dll=dll.substr(0,dll.find_last_of(L"\\/"))+L"\\ACSH5PViewer.dll";
    auto module=LoadLibraryW(dll.c_str()); if(!module) return 10;
    auto init=reinterpret_cast<PFNDVPINIT>(GetProcAddress(module,"DVP_Init"));
    auto identify=reinterpret_cast<PFNDVPIDENTIFYW>(GetProcAddress(module,"DVP_IdentifyW"));
    auto identifyFile=reinterpret_cast<PFNDVPIDENTIFYFILEW>(GetProcAddress(module,"DVP_IdentifyFileW"));
    pluginCreate=reinterpret_cast<PFNDVPCREATEVIEWER>(GetProcAddress(module,"DVP_CreateViewer"));
    if(!init||!identify||!identifyFile||!pluginCreate||!init()) return 11;
    VIEWERPLUGININFOW info{}; info.cbSize=sizeof(info); wchar_t ext[80],name[100],desc[200],copy[100],url[100];
    info.lpszHandleExts=ext;info.cchHandleExtsMax=80;info.lpszName=name;info.cchNameMax=100;info.lpszDescription=desc;info.cchDescriptionMax=200;info.lpszCopyright=copy;info.cchCopyrightMax=100;info.lpszURL=url;info.cchURLMax=100;
    if(!identify(&info)||std::wstring(ext)!=L".h5p") return 12;
    VIEWERPLUGINFILEINFOW fi{};fi.cbSize=sizeof(fi);wchar_t good[]=L"test.h5p",bad[]=L"test.md";
    if(!identifyFile(nullptr,good,&fi,nullptr)||identifyFile(nullptr,bad,&fi,nullptr)) return 13;
  } else acs::Initialize(instance);
  WNDCLASSW wc{}; wc.hInstance=instance; wc.lpfnWndProc=HostProc; wc.lpszClassName=L"ACSH5PTestHost"; wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
  RegisterClassW(&wc);
  auto hwnd=CreateWindowExW(0,wc.lpszClassName,L"ACS H5P Viewer - Test Host",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,80,80,1180,850,nullptr,nullptr,instance,nullptr);
  if(!hwnd) return 1;
  ShowWindow(hwnd,show); UpdateWindow(hwnd);
  if(!file.empty()) SendMessageW(viewer,DVPLUGINMSG_LOADW,0,reinterpret_cast<LPARAM>(file.c_str()));
  MSG msg; while(GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
  return static_cast<int>(msg.wParam);
}
