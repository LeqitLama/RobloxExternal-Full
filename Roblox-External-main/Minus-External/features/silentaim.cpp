#include "silentaim.h"
#include "settings.h"
#include "esp.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cfloat>

SilentAim g_SilentAim;

// Constants for resolver
constexpr int RESOLVER_HISTORY_SIZE = 30;
constexpr float JITTER_THRESHOLD = 15.0f;       // Degrees per tick that indicates jitter
constexpr float SPIN_THRESHOLD = 180.0f;        // Degrees per second for spin detection
constexpr float DIRECTION_CHANGE_THRESHOLD = 5; // Number of direction changes for jitter
constexpr float RESOLVER_CONFIDENCE_DECAY = 0.95f;

float SilentAim::CalculateDistance(const Vector3& a, const Vector3& b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

float SilentAim::CalculateAngle(const Vector3& from, const Vector3& to)
{
    float dx = to.x - from.x;
    float dz = to.z - from.z;
    return atan2f(dz, dx) * (180.0f / 3.14159265f);
}

void SilentAim::UpdateResolver(const std::vector<Player>& players)
{
    auto now = std::chrono::high_resolution_clock::now();
    
    for (const auto& player : players)
    {
        if (!player.valid) continue;
        
        auto& data = resolver_data[player.address];
        
        // Initialize if first time seeing this player
        if (data.history.empty())
        {
            data.resolved_position = player.head_position;
            data.predicted_velocity = { 0, 0, 0 };
            data.avg_rotation_speed = 0.0f;
            data.is_using_antiaim = false;
            data.antiaim_type = 0;
            data.confidence = 0.0f;
            data.last_update = now;
        }
        
        // Analyze the movement
        AnalyzeMovement(data, player.position, player.head_position);
        
        // Create new record
        ResolverRecord record;
        record.position = player.position;
        record.head_position = player.head_position;
        record.timestamp = now;
        record.is_jittering = false;
        record.is_spinning = false;
        record.direction_changes = 0;
        
        // Calculate velocity from last record
        if (!data.history.empty())
        {
            auto& last = data.history.back();
            auto dt = std::chrono::duration<float, std::milli>(now - last.timestamp).count();
            
            if (dt > 0.0f && dt < 500.0f) // Reasonable time delta
            {
                float dt_sec = dt / 1000.0f;
                record.velocity.x = (player.position.x - last.position.x) / dt_sec;
                record.velocity.y = (player.position.y - last.position.y) / dt_sec;
                record.velocity.z = (player.position.z - last.position.z) / dt_sec;
                
                // Calculate rotation delta (yaw change)
                float last_angle = CalculateAngle(last.position, last.head_position);
                float curr_angle = CalculateAngle(player.position, player.head_position);
                record.rotation_delta = fabsf(curr_angle - last_angle);
                
                // Normalize rotation delta
                if (record.rotation_delta > 180.0f)
                    record.rotation_delta = 360.0f - record.rotation_delta;
            }
            else
            {
                record.velocity = { 0, 0, 0 };
                record.rotation_delta = 0.0f;
            }
        }
        else
        {
            record.velocity = { 0, 0, 0 };
            record.rotation_delta = 0.0f;
        }
        
        // Add to history
        data.history.push_back(record);
        
        // Limit history size
        while (data.history.size() > RESOLVER_HISTORY_SIZE)
        {
            data.history.pop_front();
        }
        
        // Detect anti-aim type
        DetectAntiAimType(data);
        
        data.last_update = now;
    }
    
    // Clean up old resolver data for players that no longer exist
    for (auto it = resolver_data.begin(); it != resolver_data.end();)
    {
        auto age = std::chrono::duration<float, std::milli>(now - it->second.last_update).count();
        if (age > 5000.0f) // 5 seconds without update
        {
            it = resolver_data.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void SilentAim::AnalyzeMovement(ResolverData& data, const Vector3& new_pos, const Vector3& new_head)
{
    if (data.history.size() < 3) return;
    
    // Calculate average velocity over recent history
    Vector3 avg_velocity = { 0, 0, 0 };
    int count = 0;
    
    for (const auto& record : data.history)
    {
        avg_velocity.x += record.velocity.x;
        avg_velocity.y += record.velocity.y;
        avg_velocity.z += record.velocity.z;
        count++;
    }
    
    if (count > 0)
    {
        avg_velocity.x /= count;
        avg_velocity.y /= count;
        avg_velocity.z /= count;
    }
    
    data.predicted_velocity = avg_velocity;
    
    // Calculate resolved position (smooth out jitter)
    if (data.is_using_antiaim && data.antiaim_type == 2) // Jitter
    {
        // For jitter, use average of recent positions
        Vector3 avg_pos = { 0, 0, 0 };
        int pos_count = 0;
        int samples = (std::min)((int)data.history.size(), 5);
        
        auto it = data.history.rbegin();
        for (int i = 0; i < samples && it != data.history.rend(); ++i, ++it)
        {
            avg_pos.x += it->head_position.x;
            avg_pos.y += it->head_position.y;
            avg_pos.z += it->head_position.z;
            pos_count++;
        }
        
        if (pos_count > 0)
        {
            avg_pos.x /= pos_count;
            avg_pos.y /= pos_count;
            avg_pos.z /= pos_count;
            data.resolved_position = avg_pos;
        }
    }
    else if (data.is_using_antiaim && data.antiaim_type == 1) // Spin
    {
        // For spin, predict where they'll be based on rotation speed
        data.resolved_position = new_head;
    }
    else
    {
        // No anti-aim, use current position with slight prediction
        data.resolved_position = new_head;
        
        // Add small velocity prediction (16ms ahead)
        data.resolved_position.x += avg_velocity.x * 0.016f;
        data.resolved_position.y += avg_velocity.y * 0.016f;
        data.resolved_position.z += avg_velocity.z * 0.016f;
    }
}

void SilentAim::DetectAntiAimType(ResolverData& data)
{
    if (data.history.size() < 5) return;
    
    float total_rotation = 0.0f;
    int direction_changes = 0;
    float last_rotation_delta = 0.0f;
    int high_rotation_ticks = 0;
    
    for (const auto& record : data.history)
    {
        total_rotation += record.rotation_delta;
        
        // Count direction changes (sign flip in rotation)
        if (last_rotation_delta != 0.0f)
        {
            if ((record.rotation_delta > JITTER_THRESHOLD) != (last_rotation_delta > JITTER_THRESHOLD))
            {
                direction_changes++;
            }
        }
        
        // Count high rotation ticks
        if (record.rotation_delta > JITTER_THRESHOLD)
        {
            high_rotation_ticks++;
        }
        
        last_rotation_delta = record.rotation_delta;
    }
    
    data.avg_rotation_speed = total_rotation / data.history.size();
    
    // Determine anti-aim type
    data.is_using_antiaim = false;
    data.antiaim_type = 0;
    
    // Spin detection: consistent high rotation speed
    if (data.avg_rotation_speed > SPIN_THRESHOLD / 60.0f) // Per tick
    {
        data.is_using_antiaim = true;
        data.antiaim_type = 1; // Spin
        data.confidence = (std::min)(1.0f, data.avg_rotation_speed / 10.0f);
    }
    // Jitter detection: frequent direction changes
    else if (direction_changes >= DIRECTION_CHANGE_THRESHOLD)
    {
        data.is_using_antiaim = true;
        data.antiaim_type = 2; // Jitter
        data.confidence = (std::min)(1.0f, (float)direction_changes / 10.0f);
    }
    // Random detection: inconsistent high rotation
    else if (high_rotation_ticks > data.history.size() / 3)
    {
        data.is_using_antiaim = true;
        data.antiaim_type = 3; // Random
        data.confidence = (std::min)(1.0f, (float)high_rotation_ticks / (float)data.history.size());
    }
    
    // Decay confidence over time
    data.confidence *= RESOLVER_CONFIDENCE_DECAY;
}

Vector3 SilentAim::PredictPosition(const ResolverData& data, float time_ahead_ms)
{
    Vector3 predicted = data.resolved_position;
    
    float time_sec = time_ahead_ms / 1000.0f;
    predicted.x += data.predicted_velocity.x * time_sec;
    predicted.y += data.predicted_velocity.y * time_sec;
    predicted.z += data.predicted_velocity.z * time_sec;
    
    return predicted;
}

const ResolverData* SilentAim::GetResolverData(uintptr_t player_addr) const
{
    auto it = resolver_data.find(player_addr);
    if (it != resolver_data.end())
    {
        return &it->second;
    }
    return nullptr;
}

bool SilentAim::IsUsingAntiAim(uintptr_t player_addr) const
{
    auto data = GetResolverData(player_addr);
    return data && data->is_using_antiaim;
}

Vector3 SilentAim::GetResolvedPosition(uintptr_t player_addr, const Vector3& current_head) const
{
    auto data = GetResolverData(player_addr);
    if (data && data->is_using_antiaim && data->confidence > 0.3f)
    {
        return data->resolved_position;
    }
    return current_head;
}

std::string SilentAim::ReadString(uintptr_t address)
{
    std::string result;
    result.reserve(64);
    
    char character = 0;
    int offset = 0;

    while (offset < 200)
    {
        character = memory->ReadMemory<char>(address + offset);
        if (character == 0) break;
        offset++;
        result.push_back(character);
    }

    return result;
}

std::string SilentAim::LengthReadString(uintptr_t str)
{
    const auto length = memory->ReadMemory<int>(str + Offsets::StringLength);
    if (length >= 16u)
    {
        const auto _new = memory->ReadMemory<uintptr_t>(str);
        return ReadString(_new);
    }
    return ReadString(str);
}

std::string SilentAim::GetInstanceName(uintptr_t instance)
{
    const auto _get = memory->ReadMemory<uintptr_t>(instance + Offsets::Name);
    if (_get) return LengthReadString(_get);
    return "???";
}

uintptr_t SilentAim::FindFirstChild(uintptr_t instance, const std::string& name)
{
    if (!instance) return 0;

    auto start = memory->ReadMemory<uintptr_t>(instance + Offsets::Children);
    if (!start) return 0;

    auto end = memory->ReadMemory<uintptr_t>(start + Offsets::ChildrenEnd);
    auto childArray = memory->ReadMemory<uintptr_t>(start);
    if (!childArray || childArray >= end) return 0;

    for (uintptr_t current = childArray; current < end; current += 16)
    {
        auto child_instance = memory->ReadMemory<uintptr_t>(current);
        if (!child_instance) continue;
        
        if (GetInstanceName(child_instance) == name)
            return child_instance;
    }

    return 0;
}

bool SilentAim::WriteMouseTarget(const Vector3& target_pos)
{
    if (!memory || !game_base) return false;
    
    // Get Players service
    uintptr_t fake_datamodel = memory->ReadMemory<uintptr_t>(game_base + Offsets::FakeDataModelPointer);
    if (!fake_datamodel) return false;
    
    uintptr_t datamodel = memory->ReadMemory<uintptr_t>(fake_datamodel + Offsets::FakeDataModelToDataModel);
    if (!datamodel) return false;
    
    // Find Players service
    auto start = memory->ReadMemory<uintptr_t>(datamodel + Offsets::Children);
    if (!start) return false;
    auto end = memory->ReadMemory<uintptr_t>(start + Offsets::ChildrenEnd);
    
    uintptr_t players_service = 0;
    for (auto instances = memory->ReadMemory<uintptr_t>(start); instances != end; instances += 16)
    {
        uintptr_t child = memory->ReadMemory<uintptr_t>(instances);
        if (!child) continue;
        
        auto ptr = memory->ReadMemory<uintptr_t>(child + Offsets::ClassDescriptor);
        auto ptr2 = memory->ReadMemory<uintptr_t>(ptr + Offsets::ChildrenEnd);
        if (ptr2 && ReadString(ptr2) == "Players")
        {
            players_service = child;
            break;
        }
    }
    
    if (!players_service) return false;
    
    // Get LocalPlayer
    uintptr_t local_player = memory->ReadMemory<uintptr_t>(players_service + Offsets::LocalPlayer);
    if (!local_player) return false;
    
    // Get PlayerMouse
    uintptr_t player_mouse = memory->ReadMemory<uintptr_t>(local_player + Offsets::PlayerMouse);
    if (!player_mouse) return false;
    
    // Write target position to MousePosition offset
    // This makes the game think the mouse is pointing at the target
    Vector2 target_2d;
    target_2d.x = target_pos.x;
    target_2d.y = target_pos.y;
    
    // Write to Hit position (this is where the raycast thinks it hit)
    memory->WriteMemory<Vector3>(player_mouse + Offsets::MousePosition, target_pos);
    
    return true;
}

bool SilentAim::GetBestTarget(const std::vector<Player>& players, const Vector3& local_pos,
                               float screen_w, float screen_h, Vector3& out_target_pos, uintptr_t& out_target_addr)
{
    if (!g_Settings.rage_silent_aim) return false;
    
    float center_x = screen_w / 2.0f;
    float center_y = screen_h / 2.0f;
    
    float best_distance = FLT_MAX;
    bool found = false;
    
    for (const auto& player : players)
    {
        if (!player.valid) continue;
        
        // Check if within FOV
        float dx = player.screen_pos.x - center_x;
        float dy = player.screen_pos.y - center_y;
        float screen_dist = sqrtf(dx * dx + dy * dy);
        
        if (screen_dist > g_Settings.aimbot_fov) continue;
        
        // Get resolved position (accounts for anti-aim)
        Vector3 target_head = GetResolvedPosition(player.address, player.head_position);
        
        // Check if this is the best target
        float world_dist = CalculateDistance(local_pos, target_head);
        
        // Prioritize closer targets on screen
        float priority = screen_dist + (world_dist * 0.1f);
        
        if (priority < best_distance)
        {
            best_distance = priority;
            out_target_pos = target_head;
            out_target_addr = player.address;
            found = true;
        }
    }
    
    return found;
}

void SilentAim::Run(const std::vector<Player>& players, const Vector3& local_pos, float screen_w, float screen_h)
{
    // Always update resolver for tracking
    UpdateResolver(players);
    
    // Check if silent aim is enabled
    if (!g_Settings.rage_enabled || !g_Settings.rage_silent_aim) return;
    
    // Get PlayerMouse object
    if (!memory || !game_base) return;
    
    uintptr_t fake_datamodel = memory->ReadMemory<uintptr_t>(game_base + Offsets::FakeDataModelPointer);
    if (!fake_datamodel) return;
    
    uintptr_t datamodel = memory->ReadMemory<uintptr_t>(fake_datamodel + Offsets::FakeDataModelToDataModel);
    if (!datamodel) return;
    
    // Find Players service
    auto start = memory->ReadMemory<uintptr_t>(datamodel + Offsets::Children);
    if (!start) return;
    auto end = memory->ReadMemory<uintptr_t>(start + Offsets::ChildrenEnd);
    
    uintptr_t players_service = 0;
    for (auto instances = memory->ReadMemory<uintptr_t>(start); instances != end; instances += 16)
    {
        uintptr_t child = memory->ReadMemory<uintptr_t>(instances);
        if (!child) continue;
        
        auto ptr = memory->ReadMemory<uintptr_t>(child + Offsets::ClassDescriptor);
        auto ptr2 = memory->ReadMemory<uintptr_t>(ptr + Offsets::ChildrenEnd);
        if (ptr2 && ReadString(ptr2) == "Players")
        {
            players_service = child;
            break;
        }
    }
    
    if (!players_service) return;
    
    uintptr_t local_player = memory->ReadMemory<uintptr_t>(players_service + Offsets::LocalPlayer);
    if (!local_player) return;
    
    uintptr_t mouse_obj = memory->ReadMemory<uintptr_t>(local_player + Offsets::PlayerMouse);
    if (!mouse_obj) return;
    
    // Detect click (rising edge - only on the frame you click)
    static bool last_lmb = false;
    bool current_lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool just_clicked = current_lmb && !last_lmb;
    last_lmb = current_lmb;
    
    // Also check if aimbot key is held (to enable silent aim)
    bool hotkey_held = (GetAsyncKeyState(g_Settings.aimbot_key) & 0x8000) || 
                       (GetAsyncKeyState(g_Settings.aimbot_key2) & 0x8000);
    
    // Only activate on the exact frame of clicking while hotkey is held
    if (!just_clicked || !hotkey_held) return;
    
    // Find best target
    Vector3 target_pos;
    uintptr_t target_addr;
    if (!GetBestTarget(players, local_pos, screen_w, screen_h, target_pos, target_addr)) return;
    
    // Apply prediction if enabled
    if (g_Settings.aimbot_prediction)
    {
        auto data = GetResolverData(target_addr);
        if (data)
        {
            target_pos = PredictPosition(*data, 32.0f * g_Settings.aimbot_prediction_scale);
        }
    }
    
    // Save original mouse position
    Vector3 original_pos = memory->ReadMemory<Vector3>(mouse_obj + Offsets::MousePosition);
    
    // Write target position and immediately restore (no delay)
    memory->WriteMemory<Vector3>(mouse_obj + Offsets::MousePosition, target_pos);
    memory->WriteMemory<Vector3>(mouse_obj + Offsets::MousePosition, original_pos);
}