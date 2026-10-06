#pragma once
#include <windows.h>
#include <commctrl.h>
#include <objidl.h>
#include <string>
#include "viewer plugins.h"
namespace acs {
void Initialize(HINSTANCE module);
HWND CreateViewer(HWND parent, const RECT& bounds, DWORD flags, unsigned debugPort = 0);
bool IsH5P(const wchar_t* path);
void FileInfo(LPVIEWERPLUGINFILEINFOW info);
}
