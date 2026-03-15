#pragma once
#include "../sdk/memory.h"
#include "../sdk/sdk.h"
#include "../sdk/offsets.h"
#include <vector>
#include <string>
#include <chrono>
#include <Windows.h>

struct Player;

class WorldMods
{
public:
    void Initialize(Memory* mem, uintptr_t base) {
        memory = mem;
        game_base = base;
    }
    
    void Run(const std::vector<Player>& players, float screen_w, float screen_h);
    
    void TeleportToPlayer(const Vector3& target_pos);
    
    void RunTriggerbot(const std::vector<Player>& players, float screen_w, float screen_h) {}
    
private:
    Memory* memory = nullptr;
    uintptr_t game_base = 0;
    
    // Cached addresses
    uintptr_t cached_local_player = 0;
    uintptr_t cached_character = 0;
    uintptr_t cached_mouse_service = 0;
    uintptr_t cached_root_part = 0;
    uintptr_t cached_root_primitive = 0;
    uintptr_t cached_camera = 0;
    std::chrono::high_resolution_clock::time_point last_cache_update;
    
    void RunFly();
    void RunAntiAim();
    void UpdateCache();
    
    uintptr_t FindFirstChild(uintptr_t instance, const std::string& name);
    uintptr_t FindFirstChildByClass(uintptr_t instance, const std::string& class_name);
    std::string GetInstanceName(uintptr_t instance);
    std::string GetInstanceClassName(uintptr_t instance);
    std::string ReadString(uintptr_t address);
    std::string LengthReadString(uintptr_t str);
    Vector3 GetCameraLookVector();
};

extern WorldMods g_WorldMods;
