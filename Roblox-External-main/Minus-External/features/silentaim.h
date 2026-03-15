#pragma once

// Prevent Windows min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../sdk/memory.h"
#include "../sdk/sdk.h"
#include "../sdk/offsets.h"
#include <vector>
#include <chrono>
#include <unordered_map>
#include <deque>

struct Player;

// Resolver - Tracks player movement to predict real position through anti-aim
struct ResolverRecord {
    Vector3 position;
    Vector3 head_position;
    Vector3 velocity;
    float rotation_delta;
    std::chrono::high_resolution_clock::time_point timestamp;
    bool is_jittering;
    bool is_spinning;
    int direction_changes;
};

struct ResolverData {
    std::deque<ResolverRecord> history;
    Vector3 resolved_position;
    Vector3 predicted_velocity;
    float avg_rotation_speed;
    bool is_using_antiaim;
    int antiaim_type; // 0 = None, 1 = Spin, 2 = Jitter, 3 = Random
    float confidence;
    std::chrono::high_resolution_clock::time_point last_update;
};

class SilentAim
{
public:
    SilentAim() : memory(nullptr), game_base(0) {}
    
    void Initialize(Memory* mem, uintptr_t base) {
        memory = mem;
        game_base = base;
    }
    
    // Main silent aim function - writes target position to PlayerMouse
    void Run(const std::vector<Player>& players, const Vector3& local_pos, float screen_w, float screen_h);
    
    // Get the best target considering resolver data
    bool GetBestTarget(const std::vector<Player>& players, const Vector3& local_pos, 
                       float screen_w, float screen_h, Vector3& out_target_pos, uintptr_t& out_target_addr);
    
    // Update resolver with current player positions
    void UpdateResolver(const std::vector<Player>& players);
    
    // Get resolver data for visualization
    const ResolverData* GetResolverData(uintptr_t player_addr) const;
    
    // Check if a player is using anti-aim
    bool IsUsingAntiAim(uintptr_t player_addr) const;
    
    // Get resolved position for a player
    Vector3 GetResolvedPosition(uintptr_t player_addr, const Vector3& current_head) const;

private:
    Memory* memory;
    uintptr_t game_base;
    
    // Resolver data per player
    std::unordered_map<uintptr_t, ResolverData> resolver_data;
    
    // Cached addresses
    uintptr_t cached_local_player = 0;
    uintptr_t cached_players_service = 0;
    uintptr_t cached_player_mouse = 0;
    
    // Timing
    std::chrono::high_resolution_clock::time_point last_write_time;
    
    // Helper functions
    void AnalyzeMovement(ResolverData& data, const Vector3& new_pos, const Vector3& new_head);
    void DetectAntiAimType(ResolverData& data);
    Vector3 PredictPosition(const ResolverData& data, float time_ahead_ms);
    float CalculateDistance(const Vector3& a, const Vector3& b);
    float CalculateAngle(const Vector3& from, const Vector3& to);
    
    // Memory helpers
    uintptr_t FindFirstChild(uintptr_t instance, const std::string& name);
    uintptr_t FindFirstChildByClass(uintptr_t instance, const std::string& class_name);
    std::string ReadString(uintptr_t address);
    std::string LengthReadString(uintptr_t str);
    std::string GetInstanceName(uintptr_t instance);
    
    // Write target to mouse
    bool WriteMouseTarget(const Vector3& target_pos);
};

extern SilentAim g_SilentAim;
