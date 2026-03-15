#pragma once
#include <Windows.h>
#include <dwmapi.h>
#include <string>
#include <thread>

#pragma comment(lib, "dwmapi.lib")

class Overlay {
private:
    HWND m_OverlayWindow = nullptr;
    HWND m_RobloxWindow = nullptr;
    bool m_Running = false;
    std::thread m_RenderThread;
    std::string m_Status = "Loaded";

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    HWND FindRobloxWindow() {
        HWND hwnd = FindWindowA(nullptr, "Roblox");
        if (!hwnd) hwnd = FindWindowA("WINDOWSCLIENT", nullptr);
        return hwnd;
    }

    void RenderLoop() {
        // Register window class
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "RobloxInternalOverlay";
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClassExA(&wc);

        // Get screen dimensions
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        // Create overlay window
        m_OverlayWindow = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            "RobloxInternalOverlay",
            "Overlay",
            WS_POPUP,
            0, 0, screenWidth, screenHeight,
            nullptr, nullptr, wc.hInstance, nullptr
        );

        if (!m_OverlayWindow) return;

        // Set layered window attributes
        SetLayeredWindowAttributes(m_OverlayWindow, RGB(0, 0, 0), 0, LWA_COLORKEY);
        
        // Enable blur behind (optional)
        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE;
        bb.fEnable = FALSE;
        DwmEnableBlurBehindWindow(m_OverlayWindow, &bb);

        ShowWindow(m_OverlayWindow, SW_SHOW);
        UpdateWindow(m_OverlayWindow);

        // Create font
        HFONT hFont = CreateFontA(
            16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas"
        );

        // Main loop
        MSG msg;
        while (m_Running) {
            while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    m_Running = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }

            // Find Roblox window
            m_RobloxWindow = FindRobloxWindow();

            if (m_RobloxWindow && IsWindow(m_RobloxWindow)) {
                RECT rect;
                GetWindowRect(m_RobloxWindow, &rect);
                
                // Move overlay to match Roblox
                SetWindowPos(m_OverlayWindow, HWND_TOPMOST, 
                    rect.left, rect.top, 
                    rect.right - rect.left, rect.bottom - rect.top,
                    SWP_NOACTIVATE);
            }

            // Render
            HDC hdc = GetDC(m_OverlayWindow);
            if (hdc) {
                RECT clientRect;
                GetClientRect(m_OverlayWindow, &clientRect);

                // Create double buffer
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
                HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

                // Clear with transparent color
                HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(memDC, &clientRect, blackBrush);
                DeleteObject(blackBrush);

                // Set text properties
                SelectObject(memDC, hFont);
                SetBkMode(memDC, TRANSPARENT);
                SetTextColor(memDC, RGB(0, 255, 100)); // Green

                // Draw watermark
                std::string watermark = "[RobloxInternal] " + m_Status;
                TextOutA(memDC, 10, 10, watermark.c_str(), (int)watermark.length());

                // Copy to screen
                BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

                // Cleanup
                SelectObject(memDC, oldBitmap);
                DeleteObject(memBitmap);
                DeleteDC(memDC);
                ReleaseDC(m_OverlayWindow, hdc);
            }

            Sleep(16); // ~60 FPS
        }

        DeleteObject(hFont);
        DestroyWindow(m_OverlayWindow);
        UnregisterClassA("RobloxInternalOverlay", wc.hInstance);
    }

public:
    void Start() {
        if (m_Running) return;
        m_Running = true;
        m_RenderThread = std::thread(&Overlay::RenderLoop, this);
    }

    void Stop() {
        m_Running = false;
        if (m_RenderThread.joinable()) {
            m_RenderThread.join();
        }
    }

    void SetStatus(const std::string& status) {
        m_Status = status;
    }

    bool IsRunning() const { return m_Running; }
};

inline Overlay g_Overlay;
