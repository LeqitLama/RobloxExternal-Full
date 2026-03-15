#pragma once
#include "../sdk/memory.h"
#include "../sdk/sdk.h"
#include "esp.h"
#include <vector>
#include <cmath>
#include <string>

class Aimbot
{
public:
    bool enabled = true;
    float fov = 100.0f;
    float smoothness = 6.0f;
    
    void Run(const std::vector<Player>& players, float screen_width, float screen_height);
    
private:
    uintptr_t locked_target_address = 0;
    
    bool IsKeyDown();
    bool IsTargetVisible(const Player& player, float screen_width, float screen_height);
};

extern Aimbot g_Aimbot;
