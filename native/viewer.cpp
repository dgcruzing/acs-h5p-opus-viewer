#include "viewer.h"
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <wrl.h>
#include <shlwapi.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <memory>
#include <atomic>
#include "json.hpp"
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Callback;
using json=nlohmann::json;
namespace fs=std::filesystem;
namespace acs {
static HINSTANCE instance;
static constexpr UINT PollMessage=WM_APP+120;
static constexpr wchar_t ClassName[]=L"ACSH5PViewerWindow";
static fs::path runtimeRoot;
static std::once_flag initFlag;
static std::string Utf8(const std::wstring& s) {
  if(s.empty()) return {};
  int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
  std::string out(n,0); WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr); return out;
}
static std::wstring Wide(const std::string& s) {
  if(s.empty()) return {};
  int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
  std::wstring out(n,0); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n); return out;
}
static std::wstring Quote(const std::wstring& s) {
  std::wstring out=L"\""; unsigned slashes=0;
  for(auto c:s) { if(c==L'\\') { slashes++; continue; } out.append(slashes*(c==L'"'?2:1),L'\\'); slashes=0; if(c==L'"') out+=L'\\'; out+=c; }
  out.append(slashes*2,L'\\'); return out+L'"';
}
static fs::path UserRoot() {
  wchar_t local[32768]; DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
  if(!n || n>=32768) throw std::runtime_error("LOCALAPPDATA unavailable");
  return fs::path(local)/L"ACS"/L"H5PViewer";
}
static std::string Id() { GUID g; CoCreateGuid(&g); wchar_t b[40]; StringFromGUID2(g,b,40); auto s=Utf8(b); s.erase(0,1); s.pop_back(); return s; }
struct Viewer : std::enable_shared_from_this<Viewer> {
  HWND hwnd{}; DWORD flags{}; unsigned debugPort{}; bool closed{},comInitialized{},helperReady{},browserStarting{};
  int generation{}; std::wstring selected,status=L"Starting local H5P viewer…",pendingUrl,origin;
  fs::path profile; HANDLE lease{INVALID_HANDLE_VALUE},child{},job{},input{},output{};
  std::thread reader,writer; std::mutex gate; std::condition_variable wake; bool stopping{};
  std::deque<std::string> incoming,outgoing;
  ComPtr<ICoreWebView2Environment> environment;
  ComPtr<ICoreWebView2Controller> controller;
  ComPtr<ICoreWebView2> web;
  std::ofstream log;
  void Record(const std::string& event,json details=json::object()) {
    if(log) { details["event"]=event; details["tick"]=GetTickCount64(); details["generation"]=generation; log<<details.dump()<<"\n"; log.flush(); }
  }
  void Status(const std::wstring& text) { status=text; InvalidateRect(hwnd,nullptr,TRUE); Record("status",{{"text",Utf8(text)}}); }
  void Fail(const std::wstring& text) { if(web) { web->Stop(); web->Navigate(L"about:blank"); } if(controller) controller->put_IsVisible(FALSE); Status(text); }
  void Send(json data) { { std::lock_guard lock(gate); if(stopping) return; outgoing.push_back(data.dump()+"\n"); } wake.notify_one(); }
  void SetupProfile() {
    auto root=UserRoot(); fs::create_directories(root/L"logs"); fs::create_directories(root/L"profiles");
    // Only our immediate non-reparse directories with an unlocked ownership marker qualify.
    for(const auto& e:fs::directory_iterator(root/L"profiles")) {
      auto attrs=GetFileAttributesW(e.path().c_str());
      if(attrs==INVALID_FILE_ATTRIBUTES || !(attrs&FILE_ATTRIBUTE_DIRECTORY) || (attrs&FILE_ATTRIBUTE_REPARSE_POINT)) continue;
      auto marker=e.path()/L"acs-viewer.lease";
      HANDLE h=CreateFileW(marker.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
      if(h==INVALID_HANDLE_VALUE) continue;
      bool cleared=true;
      if(e.path().parent_path()==root/L"profiles") {
        std::error_code ec;
        for(const auto& child:fs::directory_iterator(e.path(),ec)) {
          if(child.path()==marker) continue;
          fs::remove_all(child.path(),ec); if(ec) { cleared=false; ec.clear(); }
        }
        if(ec) cleared=false;
      } else cleared=false;
      CloseHandle(h);
      // Keep the marker if a browser process still owns any files, so cleanup retries.
      if(cleared) { std::error_code ec; fs::remove(marker,ec); if(!ec) fs::remove(e.path(),ec); }
    }
    auto id=std::to_string(GetCurrentProcessId())+"-"+Id();
    profile=root/L"profiles"/Wide(id); fs::create_directory(profile);
    lease=CreateFileW((profile/L"acs-viewer.lease").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);
    if(lease==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot lock viewer profile");
    log.open(root/L"logs"/(Wide(id)+L".jsonl")); Record("created",{{"hwnd",reinterpret_cast<uintptr_t>(hwnd)},{"pid",GetCurrentProcessId()}});
  }
  bool StartHelper() {
    std::ifstream cf(runtimeRoot/L"runtime.json"); json cfg; cf>>cfg;
    auto node=Wide(cfg.at("node").get<std::string>());
    if(!fs::is_regular_file(node)) throw std::runtime_error("Configured Node runtime is missing");
    auto cmd=Quote(node)+L" "+Quote((runtimeRoot/L"helper"/L"server.mjs").wstring());
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE}; HANDLE childInput{},childOutput{};
    if(!CreatePipe(&childInput,&input,&sa,262144)) return false;
    if(!CreatePipe(&output,&childOutput,&sa,262144)) { CloseHandle(childInput); return false; }
    SetHandleInformation(input,HANDLE_FLAG_INHERIT,0); SetHandleInformation(output,HANDLE_FLAG_INHERIT,0);
    HANDLE err=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
    SIZE_T bytes=0; InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    std::vector<unsigned char> attributes(bytes); auto attrs=reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    InitializeProcThreadAttributeList(attrs,1,0,&bytes); HANDLE inherited[]={childInput,childOutput,err};
    UpdateProcThreadAttribute(attrs,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr);
    STARTUPINFOEXW si{}; si.StartupInfo.cb=sizeof(si); si.StartupInfo.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
    si.StartupInfo.wShowWindow=SW_HIDE; si.StartupInfo.hStdInput=childInput; si.StartupInfo.hStdOutput=childOutput; si.StartupInfo.hStdError=err; si.lpAttributeList=attrs;
    PROCESS_INFORMATION pi{};
    job=CreateJobObjectW(nullptr,nullptr); JOBOBJECT_EXTENDED_LIMIT_INFORMATION ji{}; ji.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    bool ok=job && SetInformationJobObject(job,JobObjectExtendedLimitInformation,&ji,sizeof(ji));
    if(ok) ok=CreateProcessW(node.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,runtimeRoot.c_str(),&si.StartupInfo,&pi)!=FALSE;
    DeleteProcThreadAttributeList(attrs); CloseHandle(childInput); CloseHandle(childOutput); CloseHandle(err);
    if(!ok) return false;
    child=pi.hProcess;
    if(!AssignProcessToJobObject(job,child)) { TerminateProcess(child,1); CloseHandle(pi.hThread); return false; }
    ResumeThread(pi.hThread); CloseHandle(pi.hThread); Record("helper_started",{{"pid",pi.dwProcessId}});
    reader=std::thread([this] {
      std::string buffer; char data[4096]; DWORD n;
      while(ReadFile(output,data,sizeof(data),&n,nullptr) && n) {
        buffer.append(data,n); if(buffer.size()>1024*1024) break;
        size_t pos; while((pos=buffer.find('\n'))!=std::string::npos) {
          { std::lock_guard lock(gate); incoming.push_back(buffer.substr(0,pos)); } buffer.erase(0,pos+1); PostMessageW(hwnd,PollMessage,0,0);
        }
      }
      { std::lock_guard lock(gate); incoming.push_back("{\"type\":\"exited\"}"); } PostMessageW(hwnd,PollMessage,0,0);
    });
    writer=std::thread([this] {
      for(;;) { std::string line; { std::unique_lock lock(gate); wake.wait(lock,[this]{return stopping || !outgoing.empty();}); if(stopping) break; line=std::move(outgoing.front()); outgoing.pop_front(); }
        DWORD wrote; if(!WriteFile(input,line.data(),static_cast<DWORD>(line.size()),&wrote,nullptr) || wrote!=line.size()) break;
      }
    });
    return true;
  }
  void Start() {
    try {
      auto hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED); comInitialized=SUCCEEDED(hr);
      if(FAILED(hr)) throw std::runtime_error("Viewer requires an STA window thread");
      SetupProfile();
      if(!debugPort) { std::ifstream dev(UserRoot()/L"development.json"); if(dev) { json d; dev>>d; debugPort=d.value("debugPort",0u); } }
      if(!StartHelper()) throw std::runtime_error("Unable to launch local player helper");
      SetTimer(hwnd,1,20000,nullptr);
    } catch(const std::exception& e) { Fail(L"H5P viewer: "+Wide(e.what())); }
  }
  bool Allowed(const std::wstring& url) const {
    return !origin.empty() && url.rfind(origin+L"/",0)==0;
  }
  void NavigatePending() { if(web && !pendingUrl.empty()) { controller->put_IsVisible(TRUE); web->Navigate(pendingUrl.c_str()); Record("navigate"); } }
  void Browser() {
    if(browserStarting || closed) return; browserStarting=true;
    auto weak=weak_from_this(); auto options=Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    std::wstring args=L"--autoplay-policy=document-user-activation-required --disable-background-networking --disable-component-update";
    if(debugPort) args+=L" --remote-debugging-address=127.0.0.1 --remote-debugging-port="+std::to_wstring(debugPort);
    options->put_AdditionalBrowserArguments(args.c_str());
    auto hr=CreateCoreWebView2EnvironmentWithOptions(nullptr,profile.c_str(),options.Get(),Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([weak](HRESULT result,ICoreWebView2Environment* env)->HRESULT {
      auto s=weak.lock(); if(!s || s->closed) return S_OK;
      if(FAILED(result) || !env) { s->Fail(L"WebView2 could not start. Check the installed Edge WebView2 runtime."); return S_OK; }
      s->environment=env;
      env->CreateCoreWebView2Controller(s->hwnd,Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([weak](HRESULT result,ICoreWebView2Controller* ctrl)->HRESULT {
        auto s=weak.lock(); if(!s || s->closed) { if(ctrl) ctrl->Close(); return S_OK; }
        if(FAILED(result) || !ctrl) { s->Fail(L"WebView2 could not create the viewer panel."); return S_OK; }
        s->controller=ctrl; ctrl->get_CoreWebView2(&s->web);
        ComPtr<ICoreWebView2Settings> settings; s->web->get_Settings(&settings);
        settings->put_AreDevToolsEnabled(s->debugPort!=0); settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_IsWebMessageEnabled(FALSE); settings->put_AreDefaultScriptDialogsEnabled(FALSE); settings->put_AreHostObjectsAllowed(FALSE);
        ComPtr<ICoreWebView2_22> secure; if(FAILED(s->web.As(&secure))) { s->Fail(L"Update WebView2: worker request filtering is required."); return S_OK; }
        secure->AddWebResourceRequestedFilterWithRequestSourceKinds(L"*",COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL,COREWEBVIEW2_WEB_RESOURCE_REQUEST_SOURCE_KINDS_ALL);
        EventRegistrationToken token;
        s->web->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>([weak](ICoreWebView2*,ICoreWebView2WebResourceRequestedEventArgs* e)->HRESULT {
          auto s=weak.lock(); if(!s || s->closed) return S_OK;
          ComPtr<ICoreWebView2WebResourceRequest> request; e->get_Request(&request); LPWSTR raw{}; request->get_Uri(&raw); std::wstring uri=raw?raw:L""; CoTaskMemFree(raw);
          if(!s->Allowed(uri) && uri.rfind(L"data:",0)!=0 && uri.rfind(L"blob:"+s->origin+L"/",0)!=0) {
            ComPtr<ICoreWebView2WebResourceResponse> response; s->environment->CreateWebResourceResponse(nullptr,403,L"External requests disabled",L"Content-Type: text/plain",&response); e->put_Response(response.Get()); s->Record("request_blocked");
          } return S_OK;
        }).Get(),&token);
        s->web->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>([weak](ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs* e)->HRESULT {
          auto s=weak.lock(); if(!s || s->closed) return S_OK; LPWSTR raw{}; e->get_Uri(&raw); std::wstring uri=raw?raw:L""; CoTaskMemFree(raw);
          if(uri!=L"about:blank" && !s->Allowed(uri)) e->put_Cancel(TRUE); return S_OK;
        }).Get(),&token);
        s->web->add_FrameNavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>([weak](ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs* e)->HRESULT {
          auto s=weak.lock(); if(!s || s->closed) return S_OK; LPWSTR raw{}; e->get_Uri(&raw); std::wstring uri=raw?raw:L""; CoTaskMemFree(raw);
          if(uri!=L"about:blank" && !s->Allowed(uri) && uri.rfind(L"blob:"+s->origin+L"/",0)!=0) e->put_Cancel(TRUE); return S_OK;
        }).Get(),&token);
        s->web->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>([](ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs* e)->HRESULT { e->put_Handled(TRUE); return S_OK; }).Get(),&token);
        s->web->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>([](ICoreWebView2*,ICoreWebView2PermissionRequestedEventArgs* e)->HRESULT { e->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY); return S_OK; }).Get(),&token);
        ComPtr<ICoreWebView2_4> v4; if(SUCCEEDED(s->web.As(&v4))) v4->add_DownloadStarting(Callback<ICoreWebView2DownloadStartingEventHandler>([](ICoreWebView2*,ICoreWebView2DownloadStartingEventArgs* e)->HRESULT { e->put_Cancel(TRUE); return S_OK; }).Get(),&token);
        s->web->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>([weak](ICoreWebView2*,ICoreWebView2ProcessFailedEventArgs*)->HRESULT { if(auto s=weak.lock();s && !s->closed) s->Fail(L"The browser process stopped. Select the lesson again or reopen the viewer."); return S_OK; }).Get(),&token);
        s->web->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>([weak](ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs* e)->HRESULT { if(auto s=weak.lock();s && !s->closed) { BOOL ok; e->get_IsSuccess(&ok); s->Record("navigation_complete",{{"success",ok!=FALSE}}); } return S_OK; }).Get(),&token);
        s->controller->add_MoveFocusRequested(Callback<ICoreWebView2MoveFocusRequestedEventHandler>([weak](ICoreWebView2Controller*,ICoreWebView2MoveFocusRequestedEventArgs* e)->HRESULT {
          if(auto s=weak.lock();s && !s->closed) { if(s->flags&DVPCVF_ReturnTabs) { NMKEY k{}; k.hdr.hwndFrom=s->hwnd; k.hdr.idFrom=GetDlgCtrlID(s->hwnd); k.hdr.code=NM_KEYDOWN; k.nVKey=VK_TAB; SendMessageW(GetParent(s->hwnd),WM_NOTIFY,k.hdr.idFrom,reinterpret_cast<LPARAM>(&k)); } else SetFocus(GetParent(s->hwnd)); e->put_Handled(TRUE); } return S_OK;
        }).Get(),&token);
        RECT r; GetClientRect(s->hwnd,&r); ctrl->put_Bounds(r); s->Record("webview_ready",{{"debugPort",s->debugPort}}); s->NavigatePending(); return S_OK;
      }).Get()); return S_OK;
    }).Get());
    if(FAILED(hr)) Fail(L"WebView2 initialization failed.");
  }
  void Poll() {
    std::deque<std::string> messages; { std::lock_guard lock(gate); messages.swap(incoming); }
    for(const auto& line:messages) { try {
      auto m=json::parse(line); auto type=m.value("type","");
      if(type=="ready") { KillTimer(hwnd,1); helperReady=true; auto url=Wide(m.at("url").get<std::string>()); origin=url.substr(0,url.find(L'/',7)); Record("helper_ready"); Browser(); if(!selected.empty()) Send({{"cmd","load"},{"path",Utf8(selected)},{"generation",generation}}); else { pendingUrl=url; NavigatePending(); } }
      else if(type=="loaded" && m.value("generation",-1)==generation) { pendingUrl=Wide(m.at("url").get<std::string>()); Record("package_loaded"); NavigatePending(); }
      else if(type=="error" && m.value("generation",generation)==generation) Fail(L"H5P: "+Wide(m.value("message","Cannot load package")));
      else if(type=="exited" && !closed) Fail(L"Local player helper stopped. Reopen the viewer to retry.");
    } catch(const std::exception&) { Fail(L"Invalid response from local player helper."); } }
  }
  bool Load(const std::wstring& path) {
    ++generation; selected=path; pendingUrl.clear();
    if(web) { web->Stop(); web->Navigate(L"about:blank"); } if(controller) controller->put_IsVisible(FALSE);
    Status(L"Loading "+fs::path(path).filename().wstring()+L"…");
    if(helperReady) Send({{"cmd","load"},{"path",Utf8(path)},{"generation",generation}});
    return true;
  }
  void Clear() { ++generation; selected.clear(); pendingUrl.clear(); if(web) { web->Stop(); web->Navigate(L"about:blank"); } if(controller) controller->put_IsVisible(FALSE); if(helperReady) Send({{"cmd","clear"},{"generation",generation}}); Status(L"Select an H5P lesson."); }
  void Close() {
    if(closed) return; closed=true; KillTimer(hwnd,1); Record("closing");
    if(controller) controller->Close(); web.Reset(); controller.Reset(); environment.Reset();
    { std::lock_guard lock(gate); stopping=true; } wake.notify_all();
    if(job) { CloseHandle(job); job=nullptr; } // Kills only this viewer's helper job.
    if(writer.joinable()) { CancelSynchronousIo(writer.native_handle()); writer.join(); }
    if(reader.joinable()) { CancelSynchronousIo(reader.native_handle()); reader.join(); }
    if(input) CloseHandle(input); if(output) CloseHandle(output); if(child) CloseHandle(child);
    if(lease!=INVALID_HANDLE_VALUE) { CloseHandle(lease); lease=INVALID_HANDLE_VALUE; }
    Record("closed"); log.close(); if(comInitialized) { CoUninitialize(); comInitialized=false; }
    // Browser processes may still hold files; next launch reclaims unlocked owned profiles.
  }
  ~Viewer() { Close(); }
};
LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  auto holder=reinterpret_cast<std::shared_ptr<Viewer>*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
  auto s=holder?*holder:std::shared_ptr<Viewer>{};
  if(msg==WM_NCCREATE) { auto cs=reinterpret_cast<CREATESTRUCTW*>(lp); holder=new std::shared_ptr<Viewer>(*static_cast<std::shared_ptr<Viewer>*>(cs->lpCreateParams)); SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(holder)); (*holder)->hwnd=hwnd; return TRUE; }
  if(!s) return DefWindowProcW(hwnd,msg,wp,lp);
  switch(msg) {
    case WM_CREATE: s->Start(); return 0;
    case PollMessage: s->Poll(); return 0;
    case WM_TIMER: if(wp==1) { KillTimer(hwnd,1); if(!s->helperReady) s->Fail(L"Local H5P helper did not become ready. Reopen the viewer to retry."); } return 0;
    case WM_SIZE: if(s->controller) { RECT r; GetClientRect(hwnd,&r); s->controller->put_Bounds(r); s->controller->NotifyParentWindowPositionChanged(); } return 0;
    case WM_SETFOCUS: if(s->controller) s->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC); return 0;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps); RECT r; GetClientRect(hwnd,&r); FillRect(dc,&r,GetSysColorBrush(COLOR_WINDOW)); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,GetSysColor(COLOR_WINDOWTEXT)); InflateRect(&r,-20,-20); DrawTextW(dc,s->status.c_str(),-1,&r,DT_WORDBREAK); EndPaint(hwnd,&ps); return 0; }
    case DVPLUGINMSG_LOADW: return lp && IsH5P(reinterpret_cast<wchar_t*>(lp)) && s->Load(reinterpret_cast<wchar_t*>(lp));
    case DVPLUGINMSG_CLEAR: s->Clear(); return TRUE;
    case DVPLUGINMSG_GETIMAGEINFOW: FileInfo(reinterpret_cast<LPVIEWERPLUGINFILEINFOW>(lp)); return TRUE;
    case DVPLUGINMSG_GETCAPABILITIES: return VPCAPABILITY_WANTFOCUS|VPCAPABILITY_WANTMOUSEWHEEL;
    case DVPLUGINMSG_RESIZE: MoveWindow(hwnd,static_cast<short>(LOWORD(wp)),static_cast<short>(HIWORD(wp)),LOWORD(lp),HIWORD(lp),TRUE); return TRUE;
    case DVPLUGINMSG_PREVENTAUTOSIZE: case DVPLUGINMSG_PREVENTFRAME: return TRUE;
    case DVPLUGINMSG_REDRAW: InvalidateRect(hwnd,nullptr,TRUE); return TRUE;
    case WM_DESTROY: s->Close(); return 0;
    case WM_NCDESTROY: SetWindowLongPtrW(hwnd,GWLP_USERDATA,0); delete holder; return DefWindowProcW(hwnd,msg,wp,lp);
  }
  return DefWindowProcW(hwnd,msg,wp,lp);
}
void Initialize(HINSTANCE module) {
  std::call_once(initFlag,[module]{ instance=module; wchar_t path[32768]; GetModuleFileNameW(module,path,32768); runtimeRoot=fs::path(path).parent_path()/L"ACSH5PViewer_assets";
    // Async COM callbacks may outlive the last viewer. Keep DLL code resident until Opus exits.
    HMODULE pinned; GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&Initialize),&pinned);
    WNDCLASSW wc{}; wc.hInstance=instance; wc.lpfnWndProc=WindowProc; wc.lpszClassName=ClassName; wc.hCursor=LoadCursor(nullptr,IDC_ARROW); wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW); RegisterClassW(&wc);
  });
}
HWND CreateViewer(HWND parent,const RECT& rect,DWORD flags,unsigned debugPort) {
  auto state=std::make_shared<Viewer>(); state->flags=flags; state->debugPort=debugPort;
  return CreateWindowExW((flags&DVPCVF_Border)?WS_EX_CLIENTEDGE:0,ClassName,L"ACS H5P Viewer",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|WS_TABSTOP,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,parent,nullptr,instance,&state);
}
bool IsH5P(const wchar_t* path) { return path && _wcsicmp(PathFindExtensionW(path),L".h5p")==0; }
void FileInfo(LPVIEWERPLUGINFILEINFOW p) { if(!p) return; p->dwFlags=DVPFIF_CanReturnViewer; p->wMajorType=DVPMajorType_Other; p->wMinorType=0; p->szImageSize={960,540}; p->iNumBits=0; if(p->lpszInfo) lstrcpynW(p->lpszInfo,L"Interactive H5P lesson",p->cchInfoMax); }
}
