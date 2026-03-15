#include "worldmods.h"
#include "settings.h"
#include "esp.h"
#include <cmath>
#include <iostream>
#include <random>
#include <chrono>

WorldMods g_WorldMods;

// Anti-aim state
static float antiaim_angle = 0.0f;
static std::chrono::high_resolution_clock::time_point last_aa_update;
static std::random_device aa_rd;
static std::mt19937 aa_gen(aa_rd());

float AARandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(aa_gen);
}

void WorldMods::UpdateCache()
{
    auto now = std::chrono::high_resolution_clock::now();
    
    if ((float)std::chrono::duration_cast<std::chrono::milliseconds>(now - last_cache_update).count() < 200.0f)
        return;
    
    last_cache_update = now;
    
    cached_local_player = 0;
    cached_character = 0;
    cached_root_part = 0;
    cached_root_primitive = 0;
    cached_camera = 0;
    
    uintptr_t fake_datamodel = memory->ReadMemory<uintptr_t>(game_base + Offsets::FakeDataModelPointer);
    if (!fake_datamodel) return;
    
    uintptr_t datamodel = memory->ReadMemory<uintptr_t>(fake_datamodel + Offsets::FakeDataModelToDataModel);
    if (!datamodel) return;
    
    uintptr_t players_service = FindFirstChildByClass(datamodel, "Players");
    if (!players_service) return;
    
    cached_mouse_service = FindFirstChildByClass(datamodel, "MouseService");

    cached_local_player = memory->ReadMemory<uintptr_t>(players_service + Offsets::LocalPlayer);
    if (!cached_local_player) return;
    
    cached_character = memory->ReadMemory<uintptr_t>(cached_local_player + Offsets::ModelInstance);
    if (!cached_character) return;
    
    cached_root_part = FindFirstChild(cached_character, "HumanoidRootPart");
    if (cached_root_part) {
        cached_root_primitive = memory->ReadMemory<uintptr_t>(cached_root_part + Offsets::Primitive);
    }
    
    uintptr_t workspace = FindFirstChildByClass(datamodel, "Workspace");
    if (workspace) {
        cached_camera = memory->ReadMemory<uintptr_t>(workspace + Offsets::Camera);
    }
}

Vector3 WorldMods::GetCameraLookVector()
{
    Vector3 look = {0.0f, 0.0f, -1.0f};
    
    if (!cached_camera) return look;
    
    float rotation[9];
    for (int i = 0; i < 9; i++) {
        rotation[i] = memory->ReadMemory<float>(cached_camera + Offsets::CameraRotation + (i * 4));
    }
    
    look.x = -rotation[2];
    look.y = -rotation[5];
    look.z = -rotation[8];
    
    float len = std::sqrt(look.x*look.x + look.y*look.y + look.z*look.z);
    if (len > 0.001f) {
        look.x /= len;
        look.y /= len;
        look.z /= len;
    }
    
    return look;
}

void WorldMods::RunFly()
{
    if (!g_Settings.fly_enabled) return;
    
    bool fly_active = (GetAsyncKeyState(g_Settings.fly_key) & 0x8000) != 0;
    if (!fly_active) return;
    
    UpdateCache();
    
    if (!cached_root_primitive) return;
    
    bool w = (GetAsyncKeyState('W') & 0x8000) != 0;
    bool s = (GetAsyncKeyState('S') & 0x8000) != 0;
    bool a = (GetAsyncKeyState('A') & 0x8000) != 0;
    bool d = (GetAsyncKeyState('D') & 0x8000) != 0;
    bool space = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
    bool ctrl = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;
    
    Vector3 zero = {0.0f, 0.0f, 0.0f};
    memory->WriteMemory<Vector3>(cached_root_primitive + Offsets::Velocity, zero);
    
    if (!w && !s && !a && !d && !space && !ctrl) return;
    
    Vector3 pos = memory->ReadMemory<Vector3>(cached_root_primitive + Offsets::Position);
    Vector3 look = GetCameraLookVector();
    
    Vector3 forward = {look.x, 0.0f, look.z};
    float fwd_len = std::sqrt(forward.x*forward.x + forward.z*forward.z);
    if (fwd_len > 0.001f) {
        forward.x /= fwd_len;
        forward.z /= fwd_len;
    }
    
    Vector3 right = {forward.z, 0.0f, -forward.x};
    
    float mf = 0, mr = 0, mu = 0;
    if (w) mf += 1.0f;
    if (s) mf -= 1.0f;
    if (a) mr -= 1.0f;
    if (d) mr += 1.0f;
    if (space) mu += 1.0f;
    if (ctrl) mu -= 1.0f;
    
    float speed = g_Settings.fly_speed;
    pos.x += (forward.x * mf + right.x * mr) * speed;
    pos.y += mu * speed;
    pos.z += (forward.z * mf + right.z * mr) * speed;
    
    memory->WriteMemory<Vector3>(cached_root_primitive + Offsets::Position, pos);
}

void WorldMods::RunAntiAim()
{
    if (!g_Settings.antiaim_enabled) return;
    
    UpdateCache();
    
    if (!cached_character) return;
    
    // Find Head or HumanoidRootPart to apply rotation
    uintptr_t head = FindFirstChild(cached_character, "Head");
    if (!head) return;
    
    uintptr_t head_primitive = memory->ReadMemory<uintptr_t>(head + Offsets::Primitive);
    if (!head_primitive) return;
    
    auto now = std::chrono::high_resolution_clock::now();
    float delta_time = std::chrono::duration<float>(now - last_aa_update).count();
    last_aa_update = now;
    
    // Anti-Aim Types
    switch (g_Settings.antiaim_type)
    {
        case 0: // Spin
        {
            antiaim_angle += g_Settings.antiaim_speed * delta_time * 360.0f;
            if (antiaim_angle > 360.0f) antiaim_angle -= 360.0f;
            break;
        }
        case 1: // Jitter
        {
            static bool jitter_side = false;
            jitter_side = !jitter_side;
            antiaim_angle = jitter_side ? 90.0f : -90.0f;
            break;
        }
        case 2: // Random
        {
            antiaim_angle = AARandomFloat(-180.0f, 180.0f);
            break;
        }
    }
    
    // Convert angle to radians
    float rad = antiaim_angle * 3.14159f / 180.0f;
    
    // Build rotation matrix (Y-axis rotation for looking left/right)
    // CFrame rotation matrix format: 
    // [r00 r01 r02]   [cos  0  sin]
    // [r10 r11 r12] = [0    1  0  ]
    // [r20 r21 r22]   [-sin 0  cos]
    
    float cos_a = std::cos(rad);
    float sin_a = std::sin(rad);
    
    // Write rotation to head primitive
    // Note: This writes to the primitive's rotation, which may or may not work
    // depending on how Roblox handles character orientation
    memory->WriteMemory<float>(head_primitive + Offsets::Rotation, cos_a);      // r00
    memory->WriteMemory<float>(head_primitive + Offsets::Rotation + 4, 0.0f);   // r01
    memory->WriteMemory<float>(head_primitive + Offsets::Rotation + 8, sin_a);  // r02
    memory->WriteMemory<float>(head_primitive + Offsets::Rotation + 12, 0.0f);  // r10
    memory->WriteMemory<float>(head_primitive + Offsets::Rotation + 16, 1.0f);  // r11
    memory->WriteMemory<float>(head_primitive + Offsets::Rotation + 20, 0.0f);  // r12
    memory->WriteMemory<float>(head_primitive + Offsets::Rotation + 24, -sin_a);// r20
    memory->WriteMemory<float>(head_primitive + Offsets::Rotation + 28, 0.0f);  // r21
    memory->WriteMemory<float>(head_primitive + Offsets::Rotation + 32, cos_a); // r22
}

void WorldMods::TeleportToPlayer(const Vector3& target_pos)
{
    UpdateCache();
    
    if (!cached_root_primitive) return;
    
    Vector3 tp_pos = target_pos;
    tp_pos.y += 3.0f;
    
    memory->WriteMemory<Vector3>(cached_root_primitive + Offsets::Position, tp_pos);
    
    Vector3 zero = {0.0f, 0.0f, 0.0f};
    memory->WriteMemory<Vector3>(cached_root_primitive + Offsets::Velocity, zero);
}

uintptr_t WorldMods::FindFirstChild(uintptr_t instance, const std::string& name)
{
    if (!instance) return 0;
    
    auto start = memory->ReadMemory<uintptr_t>(instance + Offsets::Children);
    if (!start) return 0;
    
    auto end = memory->ReadMemory<uintptr_t>(start + Offsets::ChildrenEnd);
    auto childArray = memory->ReadMemory<uintptr_t>(start);
    if (!childArray || childArray >= end) return 0;
    
    for (uintptr_t current = childArray; current < end; current += 16) {
        uintptr_t child = memory->ReadMemory<uintptr_t>(current);
        if (child && GetInstanceName(child) == name) return child;
    }
    return 0;
}

uintptr_t WorldMods::FindFirstChildByClass(uintptr_t instance, const std::string& class_name)
{
    if (!instance) return 0;
    
    auto start = memory->ReadMemory<uintptr_t>(instance + Offsets::Children);
    if (!start) return 0;
    
    auto end = memory->ReadMemory<uintptr_t>(start + Offsets::ChildrenEnd);
    auto childArray = memory->ReadMemory<uintptr_t>(start);
    if (!childArray || childArray >= end) return 0;
    
    for (uintptr_t current = childArray; current < end; current += 16) {
        uintptr_t child = memory->ReadMemory<uintptr_t>(current);
        if (child && GetInstanceClassName(child) == class_name) return child;
    }
    return 0;
}

std::string WorldMods::GetInstanceName(uintptr_t instance)
{
    const auto ptr = memory->ReadMemory<uintptr_t>(instance + Offsets::Name);
    return ptr ? LengthReadString(ptr) : "???";
}

std::string WorldMods::GetInstanceClassName(uintptr_t instance)
{
    const auto ptr = memory->ReadMemory<uintptr_t>(instance + Offsets::ClassDescriptor);
    const auto ptr2 = memory->ReadMemory<uintptr_t>(ptr + Offsets::ChildrenEnd);
    return ptr2 ? ReadString(ptr2) : "???";
}

std::string WorldMods::ReadString(uintptr_t address)
{
    std::string result;
    result.reserve(64);
    for (int i = 0; i < 200; i++) {
        char c = memory->ReadMemory<char>(address + i);
        if (c == 0) break;
        result.push_back(c);
    }
    return result;
}

std::string WorldMods::LengthReadString(uintptr_t str)
{
    const auto length = memory->ReadMemory<int>(str + Offsets::StringLength);
    if (length >= 16u) {
        return ReadString(memory->ReadMemory<uintptr_t>(str));
    }
    return ReadString(str);
}

void WorldMods::Run(const std::vector<Player>& players, float screen_w, float screen_h)
{
    RunFly();
    RunAntiAim();
}