#include "viewer.h"
static HINSTANCE module;
BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) { module = h; DisableThreadLibraryCalls(h); }
  return TRUE;
}
extern "C" __declspec(dllexport) BOOL DVP_Init() { acs::Initialize(module); return TRUE; }
extern "C" __declspec(dllexport) void DVP_Uninit() {}
extern "C" __declspec(dllexport) BOOL DVP_IdentifyW(LPVIEWERPLUGININFOW p) {
  if (!p || p->cbSize < VIEWERPLUGININFOW_V1_SIZE) return FALSE;
  p->dwFlags = DVPFIF_ExtensionsOnly | DVPFIF_NoThumbnails | DVPFIF_NoProperties | DVPFIF_NoFileInformation;
  p->dwVersionHigh = MAKELONG(1,0); p->dwVersionLow = 0;
  lstrcpynW(p->lpszHandleExts,L".h5p",p->cchHandleExtsMax);
  lstrcpynW(p->lpszName,L"ACS H5P Viewer",p->cchNameMax);
  lstrcpynW(p->lpszDescription,L"Interactive local H5P viewer (WebView2)",p->cchDescriptionMax);
  lstrcpynW(p->lpszCopyright,L"ACS H5P Viewer contributors, 2026",p->cchCopyrightMax);
  lstrcpynW(p->lpszURL,L"",p->cchURLMax);
  p->dwlMinFileSize = 0; p->dwlMaxFileSize = 0;
  p->dwlMinPreviewFileSize = 0; p->dwlMaxPreviewFileSize = 0;
  p->uiMajorFileType = DVPMajorType_Other;
  p->idPlugin = {0x1a6af74c,0x5363,0x4a73,{0x99,0x12,0x8f,0x2a,0x61,0xba,0x55,0x01}};
  return TRUE;
}
extern "C" __declspec(dllexport) BOOL DVP_IdentifyFileW(HWND, LPWSTR path, LPVIEWERPLUGINFILEINFOW info, HANDLE) {
  if (!acs::IsH5P(path)) return FALSE;
  acs::FileInfo(info); return TRUE;
}
extern "C" __declspec(dllexport) HWND DVP_CreateViewer(HWND parent, LPRECT rect, DWORD flags) {
  acs::Initialize(module); return acs::CreateViewer(parent,*rect,flags);
}
