// Pure Windows Player
#include "../nekoav/elements/videosink.hpp"
#include "../nekoav/elements/filters.hpp"
#include "../nekoav/player.hpp"

#include <Windows.h>
#include <CommCtrl.h>
#include <ShellScalingApi.h>
#include <algorithm>
#include <cstdio>
#include <cwchar>

#pragma comment(linker,"\"/manifestdependency:type='win32' \
    name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
    processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using namespace NEKO_NAMESPACE;

class MainWindow {
public:
    static constexpr UINT_PTR MenuOpenId = 0;
    static constexpr UINT_PTR MenuPauseId = 1;
    static constexpr UINT_PTR MenuFilterEdgeDetect = 100;
    static constexpr UINT_PTR MenuFilterSharpen = 101;
    static constexpr UINT     WM_PLAYER_UPDATE_CLOCK = WM_USER;
    MainWindow() {
        WNDCLASSEXW wx {};
        wx.cbSize = sizeof(WNDCLASSEXW);
        wx.style = CS_VREDRAW | CS_HREDRAW;
        wx.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            MainWindow *win = reinterpret_cast<MainWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (win) {
                return win->_wndProc(hwnd, msg, wParam, lParam);
            }
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        };
        wx.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
        wx.lpszClassName = L"MainWindow";
        ::RegisterClassExW(&wx);

        HMENU menu = ::CreateMenu();
        HMENU submenu = ::CreatePopupMenu();

        // Add Menu
        ::AppendMenuW(menu, MF_STRING, MenuOpenId, L"Open");
        ::AppendMenuW(menu, MF_STRING, MenuPauseId, L"Pause");
        ::AppendMenuW(menu, MF_STRING | MF_POPUP, UINT_PTR(submenu), L"Filters");

        ::AppendMenuW(submenu, MF_STRING, MenuFilterEdgeDetect, L"Edge Detect");
        ::AppendMenuW(submenu, MF_STRING, MenuFilterSharpen, L"Sharpen");

        mHwnd = ::CreateWindowExW(
            0,
            L"MainWindow",
            L"NekoAV Player",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
            nullptr,
            menu,
            ::GetModuleHandleW(nullptr),
            nullptr
        );
        ::SetWindowLongPtrW(mHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        mRenderer = CreateD2DRenderer(mHwnd);
        mPlayer.setVideoRenderer(mRenderer.get());
        mPlayer.setPositionCallback([&](double pos) {
            ::SendMessageW(mHwnd, WM_PLAYER_UPDATE_CLOCK, 0, pos);
        });

        ::ShowWindow(mHwnd, SW_SHOW);
    }
    ~MainWindow() {
        mPlayer.stop();
        mRenderer.reset();
        ::DestroyWindow(mHwnd);
    }

    LRESULT _wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_PAINT: {
                mRenderer->paint();
                return 0;
            }
            case WM_CLOSE: {
                ::PostQuitMessage(0);
                return 0;
            }
            case WM_COMMAND: {
                switch (LOWORD(wParam)) {
                    case MenuOpenId: {
                        _openUrl();
                        return 0;
                    }
                    case MenuPauseId: {
                        _pause();
                        return 0;
                    }
                    case MenuFilterEdgeDetect: {
                        _addEdgeDetectFilter();
                        return 0;
                    }
                    case MenuFilterSharpen: {
                        _addSharpenFilter();
                        return 0;
                    }
                }
                return 0;
            }
            case WM_KEYDOWN: {
                switch (wParam) {
                    case ' ': {
                        _pause();
                        return 0;
                    }
                    // Forward
                    case VK_RIGHT: {
                        _applyOffset(1);
                        return 0;
                    }
                    // Rewind
                    case VK_LEFT: {
                        _applyOffset(-1);
                        return 0;
                    }
                }
                return 0;
            }
            case WM_PLAYER_UPDATE_CLOCK: {
                _updateTitle();
            }
        }
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    void _openUrl() {
        // Open a select
        wchar_t path[MAX_PATH] {0};
        OPENFILENAMEW filename {};
        filename.lStructSize = sizeof(OPENFILENAMEW);
        filename.lpstrFile = path;
        filename.nMaxFile = MAX_PATH;
        filename.lpstrTitle = L"Open media file to play";
        filename.hwndOwner = mHwnd;
        if (!::GetOpenFileNameW(&filename)) {
            return;
        }
        // Convert Utf8
        char buffer[1024] {0};
        ::WideCharToMultiByte(CP_UTF8, 0, path, -1, buffer, sizeof(buffer), nullptr, nullptr);
        
        mPlayer.setUrl(buffer);
        mPlayer.play();
    }
    void _pause() {
        if (mPlayer.state() == State::Paused) {
            mPlayer.play();
        }
        else if (mPlayer.state() == State::Running) {
            mPlayer.pause();
        }
        _updateTitle();
    }
    void _applyOffset(double f) {
        if (mPlayer.state() == State::Null) {
            return;
        }
        mPlayer.setPosition(std::clamp(mPlayer.position() + f, 0.0, mPlayer.duration()));
    }
    void _updateTitle() {
        wchar_t buffer[1024] {0};
        ::swprintf(buffer, L"NekoAV Player %d : %d", int(mPlayer.position()), int(mPlayer.duration()));
        switch (mPlayer.state()) {
            case State::Running: {
                wcscat(buffer, L" Playing");
                break;
            }
            case State::Paused: {
                wcscat(buffer, L" Paused");
                break;
            }
        }
        ::SetWindowTextW(mHwnd, buffer);
    }
    void _addEdgeDetectFilter() {
        Filter filter(typeid(KernelFilter), [](Element &elem) {
            static_cast<KernelFilter&>(elem).setEdgeDetectKernel();
        });
        mPlayer.addFilter(filter);
    }
    void _addSharpenFilter() {
        Filter filter(typeid(KernelFilter), [](Element &elem) {
            static_cast<KernelFilter&>(elem).setSharpenKernel();
        });
        mPlayer.addFilter(filter);
    }
private:
    Box<D2DRenderer> mRenderer;
    Player mPlayer;
    HWND mHwnd;
};

int main() {
    // Set HI DPI
    ::SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    MainWindow win;
    // auto renderer = NekoAV::CreateD2DRenderer(gwbd);
    MSG msg;
    while (::GetMessageW(&msg, nullptr, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
}