#pragma once
#include "../sdk/memory.h"
#include "../sdk/sdk.h"
#include <vector>
#include <string>
#include <memory>

class Overlay;

struct Player {
    uintptr_t address = 0;
    std::string name = "";
    Vector3 position = { 0, 0, 0 };
    Vector3 head_position = { 0, 0, 0 };
    bool valid = false;
    Vector2D screen_pos = { 0, 0 };
    Vector2D box_top_left = { 0, 0 };
    Vector2D box_bottom_right = { 0, 0 };
    float box_height = 0.0f;
    float box_width = 0.0f;
    float health = 0.0f;
    float maxHealth = 100.0f;
    uintptr_t team = 0;
    float distance = 0.0f;
};

class ActorLoopClass
{
public:
    ActorLoopClass();
    bool Initialize();
   
    void Render();                  
    void Render(Overlay* overlay);

    std::vector<Player> GetPlayers();
    Matrix4x4 GetViewMatrix();

    bool WorldToScreen(const Vector3& world_pos, Vector2D& screen_pos, const Matrix4x4& view_matrix);
    void GetScreenDimensions(float& width, float& height);
    
    // Get memory instance
    Memory* GetMemory() const { return memory.get(); }

private:
    std::unique_ptr<Memory> memory;
    uintptr_t game_base;
    uintptr_t local_player;
    uintptr_t local_player_team;
    Vector3 local_player_position;
    float screen_width;
    float screen_height;

    std::string ReadString(uintptr_t address);
    std::string LengthReadString(uintptr_t string);
    std::string GetInstanceName(uintptr_t instance_address);
    std::string GetInstanceClassName(uintptr_t instance_address);
    
    uintptr_t FindFirstChild(uintptr_t instance_address, const std::string& child_name);
    uintptr_t FindFirstChildByClass(uintptr_t instance_address, const std::string& class_name);
    
    void CalculateBox(Player& player, const Matrix4x4& view_matrix);
};

extern std::unique_ptr<ActorLoopClass> ActorLoop;