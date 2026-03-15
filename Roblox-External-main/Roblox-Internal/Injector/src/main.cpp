#include <iostream>
#include <string>
#include <filesystem>
#include "injector.hpp"

void PrintBanner() {
    std::cout << R"(
  ____       _     _             _____        _           _             
 |  _ \ ___ | |__ | | _____  __ |_   _|      (_) ___  ___| |_ ___  _ __ 
 | |_) / _ \| '_ \| |/ _ \ \/ /   | || '_ \  | |/ _ \/ __| __/ _ \| '__|
 |  _ < (_) | |_) | | (_) >  <    | || | | | | |  __/ (__| || (_) | |   
 |_| \_\___/|_.__/|_|\___/_/\_\   |_||_| |_|_/ |\___|\___|\__\___/|_|   
                                           |__/                         
    )" << "\n";
    std::cout << "           Internal DLL Injector - Multi-Method\n";
    std::cout << "    ================================================\n\n";
}

void PrintMenu() {
    std::cout << "Injection Methods:\n";
    std::cout << "  [1] Manual Map (Most Stealthy)\n";
    std::cout << "  [2] NtCreateThreadEx\n";
    std::cout << "  [3] LdrLoadDll\n";
    std::cout << "  [4] Thread Hijack\n";
    std::cout << "  [5] QueueUserAPC\n";
    std::cout << "  [6] LoadLibrary (Detected)\n";
    std::cout << "  [7] Try All Methods\n";
    std::cout << "  [0] Exit\n\n";
}

int main(int argc, char* argv[]) {
    SetConsoleTitleA("Roblox Internal Injector");
    
    // Enable colors
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hConsole, &mode);
    SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    PrintBanner();

    // Get DLL path
    std::string dllPath;
    
    if (argc > 1) {
        dllPath = argv[1];
    } else {
        // Try to find DLL in same directory
        std::filesystem::path exePath = std::filesystem::path(argv[0]).parent_path();
        std::filesystem::path defaultDll = exePath / "RobloxInternal.dll";
        
        if (std::filesystem::exists(defaultDll)) {
            dllPath = defaultDll.string();
            std::cout << "[+] Found DLL: " << dllPath << "\n";
        } else {
            // Try relative path
            defaultDll = std::filesystem::current_path().parent_path() / "build" / "Release" / "RobloxInternal.dll";
            if (std::filesystem::exists(defaultDll)) {
                dllPath = defaultDll.string();
                std::cout << "[+] Found DLL: " << dllPath << "\n";
            } else {
                std::cout << "[?] Enter DLL path: ";
                std::getline(std::cin, dllPath);
            }
        }
    }

    // Create injector
    Injector injector;
    
    if (!injector.Initialize(dllPath)) {
        std::cout << "\n[!] Failed to initialize injector!\n";
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::cout << "[+] Injector initialized\n";
    std::cout << "[+] DLL: " << dllPath << "\n\n";

    // Wait for Roblox
    std::cout << "[*] Start Roblox to inject, or if already running we'll connect...\n";
    
    if (!injector.WaitForRoblox(120)) {
        std::cout << "\n[!] Roblox not found!\n";
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 1;
    }

    std::cout << "\n";
    PrintMenu();

    int choice;
    std::cout << "[?] Select method: ";
    std::cin >> choice;

    bool success = false;

    switch (choice) {
        case 0:
            return 0;
        case 1:
            success = injector.Inject(InjectionMethod::ManualMap);
            break;
        case 2:
            success = injector.Inject(InjectionMethod::NtCreateThreadEx);
            break;
        case 3:
            success = injector.Inject(InjectionMethod::LdrLoadDll);
            break;
        case 4:
            success = injector.Inject(InjectionMethod::ThreadHijack);
            break;
        case 5:
            success = injector.Inject(InjectionMethod::QueueUserAPC);
            break;
        case 6:
            success = injector.Inject(InjectionMethod::LoadLibrary);
            break;
        case 7:
            success = injector.TryAllMethods();
            break;
        default:
            std::cout << "[!] Invalid choice!\n";
            break;
    }

    std::cout << "\n";
    if (success) {
        std::cout << "========================================\n";
        std::cout << "    INJECTION SUCCESSFUL!\n";
        std::cout << "========================================\n";
    } else {
        std::cout << "========================================\n";
        std::cout << "    INJECTION FAILED\n";
        std::cout << "========================================\n";
        std::cout << "\nNote: Byfron actively blocks injections.\n";
        std::cout << "You may need to use more advanced techniques.\n";
    }

    std::cout << "\nPress Enter to exit...";
    std::cin.ignore();
    std::cin.get();
    
    return success ? 0 : 1;
}
