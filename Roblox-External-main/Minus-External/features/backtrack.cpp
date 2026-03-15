#include "backtrack.h"
#include "settings.h"
#include "../sdk/offsets.h"
#include <cmath>
#include <algorithm>

Backtrack g_Backtrack;

const std::deque<BacktrackRecord> Backtrack::empty_records;

void Backtrack::Initialize(Memory* mem, uintptr_t base)
{
    memory = mem;
    game_base = base;
}

void Backtrack::Update(const std::vector<Player>& players, float screen_width, float screen_height)
{
    if (!g_Settings.backtrack_enabled)
    {
        // Clear all records when disabled
        if (!player_records.empty())
            player_records.clear();
        return;
    }
    
    auto now = std::chrono::high_resolution_clock::now();
    current_tick++;
    
    // Update records for each player
    for (const auto& player : players)
    {
        if (!player.valid || player.health <= 0)
            continue;
        
        BacktrackRecord record;
        
        // Store WORLD positions (this is the key for real backtrack)
        record.position = player.position;
        record.head_position = player.head_position;
        
        // Also store screen positions for visualization
        record.screen_pos = player.screen_pos;
        record.box_top_left = player.box_top_left;
        record.box_width = player.box_width;
        record.box_height = player.box_height;
        
        // Timing info
        record.timestamp = now;
        record.tick_count = current_tick;
        
        // Validity
        record.valid = player.box_width > 0;
        record.health = player.health;
        record.distance = player.distance;
        
        // Add to front of deque (newest first)
        player_records[player.address].push_front(record);
    }
    
    // Cleanup old records
    CleanupOldRecords();
}

void Backtrack::CleanupOldRecords()
{
    auto now = std::chrono::high_resolution_clock::now();
    int max_time_ms = static_cast<int>(g_Settings.backtrack_time_ms);
    
    // Iterate through all players
    for (auto it = player_records.begin(); it != player_records.end(); )
    {
        auto& records = it->second;
        
        // Remove old records from the back
        while (!records.empty())
        {
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - records.back().timestamp).count();
            
            if (age > max_time_ms)
            {
                records.pop_back();
            }
            else
            {
                break;
            }
        }
        
        // Remove players with no records
        if (records.empty())
        {
            it = player_records.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

float Backtrack::AngleBetween(const Vector3& from, const Vector3& to1, const Vector3& to2)
{
    // Calculate vectors from camera to both positions
    Vector3 dir1 = { to1.x - from.x, to1.y - from.y, to1.z - from.z };
    Vector3 dir2 = { to2.x - from.x, to2.y - from.y, to2.z - from.z };
    
    // Normalize
    float len1 = std::sqrt(dir1.x*dir1.x + dir1.y*dir1.y + dir1.z*dir1.z);
    float len2 = std::sqrt(dir2.x*dir2.x + dir2.y*dir2.y + dir2.z*dir2.z);
    
    if (len1 < 0.001f || len2 < 0.001f) return 0.0f;
    
    dir1.x /= len1; dir1.y /= len1; dir1.z /= len1;
    dir2.x /= len2; dir2.y /= len2; dir2.z /= len2;
    
    // Dot product gives cosine of angle
    float dot = dir1.x*dir2.x + dir1.y*dir2.y + dir1.z*dir2.z;
    // Clamp to [-1, 1] to avoid NaN from acos
    if (dot < -1.0f) dot = -1.0f;
    if (dot > 1.0f) dot = 1.0f;
    
    // Convert to degrees
    return std::acos(dot) * (180.0f / 3.14159265f);
}

bool Backtrack::GetBestPosition(uintptr_t player_address, float screen_cx, float screen_cy, float fov,
                                 Vector2D& out_aim_pos, float& out_distance, Vector3& out_world_pos)
{
    if (!g_Settings.backtrack_enabled)
        return false;
    
    auto it = player_records.find(player_address);
    if (it == player_records.end() || it->second.empty())
        return false;
    
    const auto& records = it->second;
    
    float best_dist = fov;
    bool found = false;
    
    // Check all historical positions
    for (const auto& record : records)
    {
        if (!record.valid || record.box_width <= 0)
            continue;
        
        // Calculate aim point (center of head area) using SCREEN position
        float aim_x = record.box_top_left.x + (record.box_width * 0.5f);
        float aim_y = record.box_top_left.y + (record.box_height * (g_Settings.aimbot_target_bone == 1 ? 0.4f : 0.1f));
        
        float dist = std::hypot(aim_x - screen_cx, aim_y - screen_cy);
        
        if (dist < best_dist)
        {
            best_dist = dist;
            out_aim_pos.x = aim_x;
            out_aim_pos.y = aim_y;
            out_distance = dist;
            
            // Return the WORLD position for this record
            if (g_Settings.aimbot_target_bone == 1) {
                // Torso - between head and root
                out_world_pos.x = (record.head_position.x + record.position.x) * 0.5f;
                out_world_pos.y = (record.head_position.y + record.position.y) * 0.5f;
                out_world_pos.z = (record.head_position.z + record.position.z) * 0.5f;
            } else {
                // Head
                out_world_pos = record.head_position;
            }
            
            found = true;
        }
    }
    
    return found;
}

bool Backtrack::GetBestWorldPosition(uintptr_t player_address, const Vector3& camera_pos, float fov_degrees,
                                      Vector3& out_world_pos, float& out_age_ms)
{
    if (!g_Settings.backtrack_enabled)
        return false;
    
    auto it = player_records.find(player_address);
    if (it == player_records.end() || it->second.empty())
        return false;
    
    const auto& records = it->second;
    auto now = std::chrono::high_resolution_clock::now();
    
    // Get current position (newest record)
    if (records.empty() || !records.front().valid)
        return false;
    
    Vector3 current_head = records.front().head_position;
    
    float best_angle = fov_degrees;
    bool found = false;
    
    // Find the historical position with the smallest angle from current aim direction
    for (const auto& record : records)
    {
        if (!record.valid)
            continue;
        
        Vector3 target_pos = (g_Settings.aimbot_target_bone == 1) 
            ? Vector3{
                (record.head_position.x + record.position.x) * 0.5f,
                (record.head_position.y + record.position.y) * 0.5f,
                (record.head_position.z + record.position.z) * 0.5f
              }
            : record.head_position;
        
        float angle = AngleBetween(camera_pos, current_head, target_pos);
        
        if (angle < best_angle)
        {
            best_angle = angle;
            out_world_pos = target_pos;
            out_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - record.timestamp).count();
            found = true;
        }
    }
    
    return found;
}

const std::deque<BacktrackRecord>& Backtrack::GetRecords(uintptr_t player_address) const
{
    auto it = player_records.find(player_address);
    if (it == player_records.end())
        return empty_records;
    return it->second;
}

bool Backtrack::HasRecords(uintptr_t player_address) const
{
    auto it = player_records.find(player_address);
    return it != player_records.end() && !it->second.empty();
}

void Backtrack::ClearPlayer(uintptr_t player_address)
{
    player_records.erase(player_address);
}

void Backtrack::ClearAll()
{
    player_records.clear();
}

int Backtrack::GetTickCount(uintptr_t player_address) const
{
    auto it = player_records.find(player_address);
    if (it == player_records.end())
        return 0;
    return static_cast<int>(it->second.size());
}

float Backtrack::GetOldestRecordAge(uintptr_t player_address) const
{
    auto it = player_records.find(player_address);
    if (it == player_records.end() || it->second.empty())
        return 0.0f;
    
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - it->second.back().timestamp).count();
}
