#include <iostream>
#include <Windows.h>
#include <dwmapi.h>
#include <mmsystem.h>
#include <memory>
#include "overlay/overlay.h"
#include "features/esp.h"
#include "features/settings.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winmm.lib")
#pragma warning(disable: 4995)

std::unique_ptr<ActorLoopClass> ActorLoop = std::make_unique<ActorLoopClass>();
std::unique_ptr<Overlay> OverlayInstance = std::make_unique<Overlay>();

int main()
{
    SetConsoleTitleA("Free Lama External | leqit.de");
    
    std::cout << "========================================" << std::endl;
    std::cout << "    Free Lama External | leqit.de" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    std::cout << "[*] Initializing..." << std::endl;
    
    if (!ActorLoop->Initialize())
    {
        std::cout << "[!] Failed - make sure Roblox is running" << std::endl;
        std::cin.get();
        return 1;
    }
    
    std::cout << "[+] Game found!" << std::endl;

    if (!OverlayInstance->Initialize())
    {
        std::cout << "[!] Failed to initialize overlay" << std::endl;
        std::cin.get();
        return 1;
    }
    
    std::cout << "[+] Overlay ready!" << std::endl;
    std::cout << std::endl;
    std::cout << "[*] Menu: INSERT to toggle" << std::endl;
    std::cout << "[*] Aimbot: Side Mouse Buttons" << std::endl;
    std::cout << "[*] ESP: Always On (configurable in menu)" << std::endl;
    std::cout << std::endl;
    std::cout << "[+] Running! Enjoy :)" << std::endl;

    bool insert_pressed = false;
    
    while (OverlayInstance->IsRunning())
    {
        // Insert toggle for Menu
        if (GetAsyncKeyState(VK_INSERT) & 0x8000)
        {
            if (!insert_pressed)
            {
                OverlayInstance->ToggleMenu();
                insert_pressed = true;
            }
        }
        else
        {
            insert_pressed = false;
        }
        
        OverlayInstance->BeginFrame();
        ActorLoop->Render(OverlayInstance.get());
        OverlayInstance->EndFrame();
    }

    return 0;
} 