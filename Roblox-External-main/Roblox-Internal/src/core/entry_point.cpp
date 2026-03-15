#include "entry_point.hpp"
#include <sstream>

namespace entry_point
{
    void main_thread(HMODULE dll_module)
    {
        globals::dll_module = dll_module;

        // IMMEDIATE feedback - uncomment this to test if DLL is running
        MessageBoxA(nullptr, "RobloxInternal DLL Loaded!\n\nIf you see this, injection worked.", "Success", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);

        // Allocate console for debug output
        AllocConsole();
        FILE* f;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        
        printf("[+] RobloxInternal DLL Loaded!\n");
        printf("[+] Module Base: 0x%p\n", dll_module);

        // Start the overlay
        printf("[*] Starting overlay...\n");
        g_Overlay.SetStatus("Initializing...");
        g_Overlay.Start();

        // Wait for overlay to start
        Sleep(1000);
        
        if (!g_Overlay.IsRunning()) {
            printf("[!] Overlay failed to start!\n");
            MessageBoxA(nullptr, "Overlay failed to start!", "Error", MB_OK | MB_ICONERROR);
        } else {
            printf("[+] Overlay started successfully!\n");
        }

        // Wait for Roblox to fully load
        Sleep(2000);
        g_Overlay.SetStatus("Waiting for game...");
        printf("[*] Waiting for game window...\n");

        // Wait for game window
        HWND hwnd = nullptr;
        for (int i = 0; i < 30; i++) {
            hwnd = FindWindowA(nullptr, "Roblox");
            if (hwnd) {
                printf("[+] Found Roblox window: 0x%p\n", hwnd);
                break;
            }
            Sleep(500);
        }

        if (!hwnd) {
            printf("[!] Could not find Roblox window!\n");
        }

        g_Overlay.SetStatus("Initializing engine...");
        printf("[*] Initializing engine...\n");

        // Initialize the Roblox engine interface
        if (!initialize_engine())
        {
            g_Overlay.SetStatus("Engine init failed!");
            printf("[!] Engine initialization failed!\n");
        } else {
            printf("[+] Engine initialized!\n");
        }

        g_Overlay.SetStatus("Starting services...");
        printf("[*] Starting services...\n");

        // Start all services
        start_services();

        g_Overlay.SetStatus("Ready - Connected!");
        printf("[+] All systems ready!\n");
        printf("[*] Listening for scripts on pipe: %s\n", configuration::pipe_name);

        // Keep running
        while (true)
        {
            Sleep(100);
            
            // Check if we should exit (e.g., if Roblox closes)
            if (!FindWindowA(nullptr, "Roblox")) {
                printf("[*] Roblox window closed, exiting...\n");
                break;
            }
        }

        // Cleanup
        g_Overlay.Stop();
        FreeConsole();
    }

    bool initialize_engine()
    {
        try {
            // Initialize graphics/rendering system
            if (!roblox_engine::graphics->initialize())
            {
                printf("[!] Graphics init failed (non-critical)\n");
            }

            // Initialize script execution
            if (!script_execution->initialize())
            {
                printf("[!] Script execution init failed (non-critical)\n");
            }

            globals::initialized = true;
            return true;
        }
        catch (...) {
            printf("[!] Exception during engine init!\n");
            return false;
        }
    }

    void start_services()
    {
        try {
            // Initialize script environment (custom functions)
            script_environment->initialize();
            printf("[+] Script environment initialized\n");

            // Start the script scheduler
            script_scheduler->initialize();
            printf("[+] Script scheduler initialized\n");

            // Setup communication
            if (communication->setup())
            {
                std::thread([]() {
                    printf("[+] Communication thread started\n");
                    communication->receive_data();
                }).detach();

                g_Overlay.SetStatus("Ready - Pipe active");
                printf("[+] Named pipe ready\n");
            } else {
                printf("[!] Failed to setup communication pipe\n");
            }
        }
        catch (...) {
            printf("[!] Exception during service startup!\n");
        }
    }
}

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)entry_point::main_thread, hModule, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        // Cleanup
        g_Overlay.Stop();
        script_scheduler->shutdown();
        communication->shutdown();
        break;
    }
    return TRUE;
}
