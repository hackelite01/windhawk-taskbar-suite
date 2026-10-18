// ==WindhawkMod==
// @id              taskbar-now-playing-widget
// @name            Taskbar Now Playing Widget
// @description     A small glass-style now-playing widget on the taskbar with title, artist and previous/play/next buttons
// @version         0.1.0
// @author          hackelite01
// @github          https://github.com/hackelite01
// @include         windhawk.exe
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -lgdi32 -luser32
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Taskbar Now Playing Widget

Shows what is playing (Spotify, browsers, media players - anything that
registers with the Windows media session) in a small widget placed over the
left side of the taskbar, with previous / play-pause / next buttons.

The widget lives in its own window, so it does not hook or modify Explorer.
Combine it with the Windows 11 Taskbar Styler if you want the taskbar itself
to look glassy; this mod only draws the widget.

## Settings
- **Width / Height**: widget size in pixels (at 100% scaling).
- **Offset X / Offset Y**: distance from the left edge of the taskbar, and
  vertical nudge from the taskbar's center.
- **Opacity**: 0 (invisible) to 255 (solid).
- **Hide when nothing is playing**: hide the widget when no media session exists.

## Notes
- Primary taskbar only.
- The widget hides while the taskbar is hidden (for example in fullscreen apps).
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- width: 320
  $name: Width
- height: 44
  $name: Height
- offsetX: 12
  $name: Offset X
  $description: Distance from the left edge of the taskbar
- offsetY: 0
  $name: Offset Y
  $description: Vertical nudge from the taskbar's center
- opacity: 235
  $name: Opacity
  $description: 0 to 255
- hideWhenIdle: true
  $name: Hide when nothing is playing
*/
// ==/WindhawkModSettings==

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <thread>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

using namespace winrt::Windows::Media::Control;

namespace {

constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 500;
constexpr const wchar_t* kClassName = L"HackeliteNowPlayingWidget";

struct Settings {
    int width = 320;
    int height = 44;
    int offsetX = 12;
    int offsetY = 0;
    int opacity = 235;
    bool hideWhenIdle = true;
};

struct MediaState {
    std::wstring title;
    std::wstring artist;
    bool hasSession = false;
    bool playing = false;

    bool operator==(const MediaState& o) const {
        return title == o.title && artist == o.artist &&
               hasSession == o.hasSession && playing == o.playing;
    }
};

Settings g_settings;
std::thread g_thread;
std::atomic<bool> g_stop{false};
std::atomic<DWORD> g_threadId{0};

// Everything below is only touched from the UI thread.
MediaState g_media;
GlobalSystemMediaTransportControlsSessionManager g_manager{nullptr};
HFONT g_fontTitle = nullptr;
HFONT g_fontArtist = nullptr;
HFONT g_fontIcon = nullptr;
double g_scale = 1.0;
int g_lastW = 0;
int g_lastH = 0;

void LoadSettings() {
    Settings s;
    s.width = std::clamp(Wh_GetIntSetting(L"width"), 160, 800);
    s.height = std::clamp(Wh_GetIntSetting(L"height"), 28, 120);
    s.offsetX = std::clamp(Wh_GetIntSetting(L"offsetX"), 0, 4000);
    s.offsetY = std::clamp(Wh_GetIntSetting(L"offsetY"), -200, 200);
    s.opacity = std::clamp(Wh_GetIntSetting(L"opacity"), 0, 255);
    s.hideWhenIdle = Wh_GetIntSetting(L"hideWhenIdle") != 0;
    g_settings = s;
}

int Px(int v) { return static_cast<int>(v * g_scale + 0.5); }

HFONT MakeFont(int heightPx, int weight, const wchar_t* face) {
    return CreateFontW(-heightPx, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

void RefreshMedia(HWND hwnd) {
    MediaState next;
    try {
        if (!g_manager) {
            g_manager = GlobalSystemMediaTransportControlsSessionManager::
                            RequestAsync().get();
        }
        auto session = g_manager.GetCurrentSession();
        if (session) {
            auto props = session.TryGetMediaPropertiesAsync().get();
            next.title = props.Title().c_str();
            next.artist = props.Artist().c_str();
            next.playing =
                session.GetPlaybackInfo().PlaybackStatus() ==
                GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
            next.hasSession = true;
        }
    } catch (...) {
        g_manager = nullptr;  // try to reconnect on the next tick
    }

    if (!(next == g_media)) {
        g_media = std::move(next);
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

// 0 = previous, 1 = play/pause, 2 = next
void SendMediaCommand(int cmd) {
    try {
        if (!g_manager) return;
        auto session = g_manager.GetCurrentSession();
        if (!session) return;
        if (cmd == 0) {
            session.TrySkipPreviousAsync();
        } else if (cmd == 1) {
            session.TryTogglePlayPauseAsync();
        } else {
            session.TrySkipNextAsync();
        }
    } catch (...) {
    }
}

void Reposition(HWND hwnd) {
    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    RECT r;
    bool show = tray && IsWindowVisible(tray) && GetWindowRect(tray, &r) &&
                !(g_settings.hideWhenIdle && !g_media.hasSession);

    if (!show) {
        ShowWindow(hwnd, SW_HIDE);
        return;
    }

    int w = Px(g_settings.width);
    int h = Px(g_settings.height);
    int x = r.left + Px(g_settings.offsetX);
    int y = r.top + (r.bottom - r.top - h) / 2 + Px(g_settings.offsetY);

    SetWindowPos(hwnd, HWND_TOPMOST, x, y, w, h,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetLayeredWindowAttributes(hwnd, 0, static_cast<BYTE>(g_settings.opacity),
                               LWA_ALPHA);

    if (w != g_lastW || h != g_lastH) {
        g_lastW = w;
        g_lastH = h;
        SetWindowRgn(hwnd, CreateRoundRectRgn(0, 0, w + 1, h + 1, Px(16), Px(16)),
                     TRUE);
    }
}

RECT ButtonRect(const RECT& client, int index) {
    int bw = Px(32);
    int left = client.right - Px(8) - bw * (3 - index);
    return RECT{left, client.top, left + bw, client.bottom};
}

void Paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);

    HBRUSH bg = CreateSolidBrush(RGB(24, 28, 36));
    FillRect(mem, &rc, bg);
    DeleteObject(bg);
    HBRUSH border = CreateSolidBrush(RGB(88, 98, 114));
    FrameRect(mem, &rc, border);
    DeleteObject(border);

    SetBkMode(mem, TRANSPARENT);

    RECT firstBtn = ButtonRect(rc, 0);
    int textLeft = Px(14);
    int textRight = firstBtn.left - Px(8);

    const wchar_t* title = g_media.hasSession && !g_media.title.empty()
                               ? g_media.title.c_str()
                               : L"Nothing playing";
    RECT titleRc{textLeft, Px(5), textRight, h / 2 + Px(2)};
    SelectObject(mem, g_fontTitle);
    SetTextColor(mem, RGB(255, 255, 255));
    DrawTextW(mem, title, -1, &titleRc,
              DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS | DT_VCENTER);

    if (g_media.hasSession && !g_media.artist.empty()) {
        RECT artistRc{textLeft, h / 2, textRight, h - Px(5)};
        SelectObject(mem, g_fontArtist);
        SetTextColor(mem, RGB(77, 163, 255));
        DrawTextW(mem, g_media.artist.c_str(), -1, &artistRc,
                  DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS | DT_VCENTER);
    }

    // Segoe Fluent Icons: E892 previous, E768 play, E769 pause, E893 next.
    const wchar_t* glyphs[3] = {L"\xE892", g_media.playing ? L"\xE769" : L"\xE768",
                                L"\xE893"};
    SelectObject(mem, g_fontIcon);
    for (int i = 0; i < 3; i++) {
        RECT br = ButtonRect(rc, i);
        SetTextColor(mem, i == 1 ? RGB(77, 163, 255) : RGB(255, 255, 255));
        DrawTextW(mem, glyphs[i], -1, &br,
                  DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    }

    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER:
            if (wParam == kTimerId) {
                SetTimer(hwnd, kTimerId, kTimerMs, nullptr);  // back to normal rate
                RefreshMedia(hwnd);
                Reposition(hwnd);
            }
            return 0;
        case WM_PAINT:
            Paint(hwnd);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_LBUTTONUP: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            POINT pt{static_cast<short>(LOWORD(lParam)),
                     static_cast<short>(HIWORD(lParam))};
            for (int i = 0; i < 3; i++) {
                RECT br = ButtonRect(rc, i);
                if (PtInRect(&br, pt)) {
                    SendMediaCommand(i);
                    // Show the new state quickly instead of waiting for the timer.
                    SetTimer(hwnd, kTimerId, 150, nullptr);
                    return 0;
                }
            }
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, kTimerId);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void UiThread() {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    g_scale = GetDpiForSystem() / 96.0;
    g_fontTitle = MakeFont(Px(13), FW_SEMIBOLD, L"Segoe UI Variable");
    g_fontArtist = MakeFont(Px(11), FW_NORMAL, L"Segoe UI Variable");
    g_fontIcon = MakeFont(Px(16), FW_NORMAL, L"Segoe Fluent Icons");

    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        kClassName, L"", WS_POPUP, 0, 0, Px(g_settings.width),
        Px(g_settings.height), nullptr, nullptr, inst, nullptr);

    // Make sure this thread has a message queue before Wh_ModUninit can post to it.
    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    g_threadId = GetCurrentThreadId();

    if (hwnd && !g_stop) {
        SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
        RefreshMedia(hwnd);
        Reposition(hwnd);
        SetTimer(hwnd, kTimerId, kTimerMs, nullptr);

        while (!g_stop && GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hwnd) DestroyWindow(hwnd);
    UnregisterClassW(kClassName, inst);
    DeleteObject(g_fontTitle);
    DeleteObject(g_fontArtist);
    DeleteObject(g_fontIcon);
    g_manager = nullptr;
    winrt::uninit_apartment();
}

}  // namespace

BOOL Wh_ModInit() {
    Wh_Log(L"Taskbar Now Playing Widget: init");
    LoadSettings();
    g_stop = false;
    g_threadId = 0;
    g_thread = std::thread(UiThread);
    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();
    g_lastW = 0;  // force the rounded region to be rebuilt on the next tick
}

void Wh_ModUninit() {
    Wh_Log(L"Taskbar Now Playing Widget: uninit");
    g_stop = true;
    DWORD tid = g_threadId;
    if (tid) PostThreadMessageW(tid, WM_QUIT, 0, 0);
    if (g_thread.joinable()) g_thread.join();
}
