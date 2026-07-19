#include "tray_icon.h"
#include <shellapi.h>
#include <CommCtrl.h>
#include <string>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comctl32.lib")

namespace Tray {
namespace {

static UINT getTrayMsg() {
  static UINT msg = RegisterWindowMessageW(L"OVsonLoaderTrayMessage");
  return msg;
}
constexpr UINT  ID_SHOW    = 4001;
constexpr UINT  ID_QUIT    = 4002;
constexpr UINT  TRAY_UID   = 1;

HWND                    g_owner    = nullptr;
bool                    g_installed = false;
std::function<void()>   g_onShow;
std::function<void()>   g_onQuit;
NOTIFYICONDATAW         g_nid{};

LRESULT CALLBACK trayProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                           LPARAM lParam, UINT_PTR, DWORD_PTR);

void buildNid() {
  g_nid = {};
  g_nid.cbSize           = sizeof(g_nid);
  g_nid.hWnd             = g_owner;
  g_nid.uID              = TRAY_UID;
  g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  g_nid.uCallbackMessage = getTrayMsg();
  g_nid.hIcon            = (HICON)LoadImageW(
      GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON,
      GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
      LR_DEFAULTCOLOR);
  if (!g_nid.hIcon) {
    g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  }
  g_nid.hBalloonIcon = (HICON)LoadImageW(
      GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON,
      32, 32, LR_DEFAULTCOLOR);
  if (!g_nid.hBalloonIcon) {
    g_nid.hBalloonIcon = g_nid.hIcon;
  }
  wcscpy_s(g_nid.szTip, L"OVson Loader");
}

void showContextMenu() {
  POINT pt;
  GetCursorPos(&pt);
  HMENU menu = CreatePopupMenu();
  if (!menu) return;
  AppendMenuW(menu, MF_STRING, ID_SHOW, L"Show OVson");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, ID_QUIT, L"Quit");
  SetForegroundWindow(g_owner);
  UINT cmd = TrackPopupMenu(menu,
                            TPM_RETURNCMD | TPM_RIGHTBUTTON
                              | TPM_NONOTIFY,
                            pt.x, pt.y, 0, g_owner, nullptr);
  DestroyMenu(menu);
  if (cmd == ID_SHOW && g_onShow) g_onShow();
  else if (cmd == ID_QUIT && g_onQuit) g_onQuit();
}

LRESULT CALLBACK trayProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                          LPARAM lParam, UINT_PTR, DWORD_PTR) {
  if (uMsg == getTrayMsg()) {
    UINT ev = LOWORD(lParam);
    if (ev == WM_LBUTTONUP || ev == WM_LBUTTONDBLCLK) {
      if (g_onShow) g_onShow();
    } else if (ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU) {
      showContextMenu();
    }
    return 0;
  }
  static const UINT WM_TASKBARCREATED =
      RegisterWindowMessageW(L"TaskbarCreated");
  if (uMsg == WM_TASKBARCREATED) {
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    return 0;
  }
  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

} // namespace

void install(HWND owner,
             std::function<void()> onShow,
             std::function<void()> onQuit) {
  if (g_installed || !owner) return;
  g_owner   = owner;
  g_onShow  = std::move(onShow);
  g_onQuit  = std::move(onQuit);

  UINT trayMsg = getTrayMsg();
  ChangeWindowMessageFilterEx(g_owner, trayMsg, MSGFLT_ALLOW, nullptr);

  static const UINT WM_TASKBARCREATED =
      RegisterWindowMessageW(L"TaskbarCreated");
  ChangeWindowMessageFilterEx(g_owner, WM_TASKBARCREATED, MSGFLT_ALLOW, nullptr);

  buildNid();
  SetWindowSubclass(g_owner, trayProc, 7, 0);
  Shell_NotifyIconW(NIM_ADD, &g_nid);
  g_installed = true;
}

void notify(const wchar_t *title, const wchar_t *body) {
  if (!g_installed) return;
  NOTIFYICONDATAW n = g_nid;
  n.uFlags      = NIF_INFO | NIF_ICON;
  n.dwInfoFlags = NIIF_USER | NIIF_NOSOUND | NIIF_LARGE_ICON;
  wcscpy_s(n.szInfoTitle, title ? title : L"OVson");
  wcsncpy_s(n.szInfo, body ? body : L"", _TRUNCATE);
  Shell_NotifyIconW(NIM_MODIFY, &n);
}

void uninstall() {
  if (!g_installed) return;
  Shell_NotifyIconW(NIM_DELETE, &g_nid);
  if (g_owner) {
    RemoveWindowSubclass(g_owner, trayProc, 7);
  }
  if (g_nid.hBalloonIcon && g_nid.hBalloonIcon != g_nid.hIcon) {
    DestroyIcon(g_nid.hBalloonIcon);
  }
  if (g_nid.hIcon) DestroyIcon(g_nid.hIcon);
  g_installed = false;
  g_owner = nullptr;
}

} // namespace Tray
