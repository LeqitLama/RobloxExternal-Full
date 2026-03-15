#pragma once
#include "../sdk/sdk.h"
#include "../sdk/memory.h"
#include "esp.h"
#include <vector>
#include <deque>
#include <chrono>
#include <unordered_map>

// Maximum time to store player positions (in milliseconds)
constexpr int BACKTRACK_MAX_TIME_MS = 200;

// Store full player state with timestamp for real backtrack
struct BacktrackRecord {
    // World positions (3D)
    Vector3 position;           // Root position in world
    Vector3 head_position;      // Head position in world
    
    // Screen positions (2D) - calculated for visualization
    Vector2D screen_pos;
    Vector2D box_top_left;
    float box_width;
    float box_height;
    
    // Timing
    std::chrono::high_resolution_clock::time_point timestamp;
    int tick_count;             // Simulation tick
    
    // Validity
    bool valid;
    float health;
    
    // Distance from local player at the time
    float distance;
};

class Backtrack {
public:
    Backtrack() : memory(nullptr), game_base(0) {}
    
    // Initialize with memory access
    void Initialize(Memory* mem, uintptr_t base);
    
    // Call this every frame with current player data to record positions
    void Update(const std::vector<Player>& players, float screen_width, float screen_height);
    
    // Get the best backtrack record for a player (closest to crosshair within FOV)
    // Returns true if a valid backtrack position was found
    bool GetBestPosition(uintptr_t player_address, float screen_cx, float screen_cy, float fov,
                         Vector2D& out_aim_pos, float& out_distance, Vector3& out_world_pos);
    
    // Get the best world position for backtrack aiming
    bool GetBestWorldPosition(uintptr_t player_address, const Vector3& camera_pos, float fov_degrees,
                              Vector3& out_world_pos, float& out_age_ms);
    
    // Get all backtrack records for a player (for visualization)
    const std::deque<BacktrackRecord>& GetRecords(uintptr_t player_address) const;
    
    // Check if we have any backtrack data for a player
    bool HasRecords(uintptr_t player_address) const;
    
    // Clear all backtrack data (call when target dies or becomes invalid)
    void ClearPlayer(uintptr_t player_address);
    
    // Clear all data
    void ClearAll();
    
    // Get number of valid ticks stored for a player
    int GetTickCount(uintptr_t player_address) const;
    
    // Get the oldest valid record age in ms
    float GetOldestRecordAge(uintptr_t player_address) const;
    
private:
    Memory* memory;
    uintptr_t game_base;
    
    // Store historical positions per player
    std::unordered_map<uintptr_t, std::deque<BacktrackRecord>> player_records;
    
    // Global tick counter for ordering records
    int current_tick = 0;
    
    // Clean old records outside the time window
    void CleanupOldRecords();
    
    // Empty record list for when player doesn't exist
    static const std::deque<BacktrackRecord> empty_records;
    
    // Calculate angle between two vectors
    float AngleBetween(const Vector3& from, const Vector3& to1, const Vector3& to2);
};

extern Backtrack g_Backtrack;
