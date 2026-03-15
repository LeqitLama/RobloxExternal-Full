#include "aimbot.h"
#include "backtrack.h"
#include <Windows.h>
#include <cmath>
#include <unordered_map>
#include <random>
#include <chrono>
#include "settings.h"

Aimbot g_Aimbot;

static std::unordered_map<uintptr_t, Vector2D> previous_positions;
static std::unordered_map<uintptr_t, Vector2D> velocity_cache;

// Legit aim state
static std::chrono::high_resolution_clock::time_point last_target_switch;
static bool waiting_for_reaction = false;

bool Aimbot::IsKeyDown()
{
    // Rage mode ignores keybind if you want it to be "Always On" while toggle is enabled
    // But usually safer to still require key usage. 
    // Let's stick to keybind for now, unless user asks for "Always On" specifically.
    return (GetAsyncKeyState(g_Settings.aimbot_key) & 0x8000) || 
           (GetAsyncKeyState(g_Settings.aimbot_key2) & 0x8000);
}

void Aimbot::Run(const std::vector<Player>& players, float screen_width, float screen_height)
{
    if (!g_Settings.aimbot_enabled) return;

    // Check keybind
    if (!IsKeyDown())
    {
        locked_target_address = 0;
        waiting_for_reaction = false;
        return;
    }

    float screen_cx = screen_width / 2.0f;
    float screen_cy = screen_height / 2.0f;
    
    const Player* target = nullptr;

    // Validate Existing Lock
    if (locked_target_address != 0)
    {
        for (const auto& p : players)
        {
            if (p.address == locked_target_address)
            {
                if (!p.valid || p.health <= 0 || p.box_width <= 0) {
                    locked_target_address = 0;
                    break;
                }

                float aim_x = p.box_top_left.x + (p.box_width * 0.5f);
                float aim_y = p.box_top_left.y + (p.box_height * (g_Settings.aimbot_target_bone == 1 ? 0.4f : 0.1f));
                
                float dist = std::hypot(aim_x - screen_cx, aim_y - screen_cy);

                // In Rage Mode, use large but finite FOV to prevent locking on off-screen/invalid coords
                // 2500.0f covers entire screen + buffer, but ignores weird "behind" coordinates
                float active_fov = g_Settings.rage_enabled ? 2500.0f : g_Settings.aimbot_fov;

                if (dist <= active_fov) 
                {
                    target = &p;
                }
                else 
                {
                    locked_target_address = 0;
                }
                break;
            }
        }
    }

    // Find closest enemy if no locked target
    const Player* backtrack_target = nullptr;
    Vector2D backtrack_aim_pos = {0, 0};
    bool use_backtrack = false;
    
    if (!target)
    {
        // Use large but safe distance for Rage
        float best_dist = g_Settings.rage_enabled ? 2500.0f : g_Settings.aimbot_fov;

        for (const auto& p : players)
        {
            if (!p.valid || p.box_width <= 0 || p.health <= 0) continue;

            float aim_x = p.box_top_left.x + (p.box_width * 0.5f);
            float aim_y = p.box_top_left.y + (p.box_height * (g_Settings.aimbot_target_bone == 1 ? 0.4f : 0.1f));

            float dist = std::hypot(aim_x - screen_cx, aim_y - screen_cy);

            if (dist < best_dist)
            {
                best_dist = dist;
                target = &p;
                use_backtrack = false;
            }
            
            // Check backtrack positions if enabled (Backtrack usually legit feature, but helps rage too)
            if (g_Settings.backtrack_enabled)
            {
                Vector2D bt_aim_pos;
                float bt_dist;
                Vector3 bt_world_pos;
                if (g_Backtrack.GetBestPosition(p.address, screen_cx, screen_cy, best_dist, bt_aim_pos, bt_dist, bt_world_pos))
                {
                    if (bt_dist < best_dist)
                    {
                        best_dist = bt_dist;
                        target = &p;
                        backtrack_aim_pos = bt_aim_pos;
                        use_backtrack = true;
                    }
                }
            }
        }

        if (target) {
            // New target acquired
            if (target->address != locked_target_address) {
                locked_target_address = target->address;
                
                // Reset legit mode state for new target
                if (g_Settings.aimbot_legit_mode && !g_Settings.rage_enabled) {
                    last_target_switch = std::chrono::high_resolution_clock::now();
                    waiting_for_reaction = true;
                }
            }
        }
    }

    if (!target) return;

    // Calculate aim point
    float aim_x, aim_y;
    
    if (use_backtrack && g_Settings.backtrack_enabled)
    {
        aim_x = backtrack_aim_pos.x;
        aim_y = backtrack_aim_pos.y;
    }
    else
    {
        aim_x = target->box_top_left.x + (target->box_width * 0.5f);
        aim_y = target->box_top_left.y + (target->box_height * (g_Settings.aimbot_target_bone == 1 ? 0.4f : 0.1f));
    }
    
    // Prediction (Disabled for immediate snap in Rage usually, but can be helpful)
    if (g_Settings.aimbot_prediction)
    {
        Vector2D current_pos = { aim_x, aim_y };
        
        auto prev_it = previous_positions.find(target->address);
        if (prev_it != previous_positions.end())
        {
            float vel_x = current_pos.x - prev_it->second.x;
            float vel_y = current_pos.y - prev_it->second.y;
            
            aim_x += vel_x * g_Settings.aimbot_prediction_scale * 2.0f;
            aim_y += vel_y * g_Settings.aimbot_prediction_scale * 2.0f;
        }
        
        previous_positions[target->address] = current_pos;
    }

    // ========== RAGE MODE ==========
    if (g_Settings.rage_enabled)
    {
        float delta_x = aim_x - screen_cx;
        float delta_y = aim_y - screen_cy;
        
        // Clamp deltas to prevent spinning/glitching
        // 800px per frame is still excessively fast (instant) but safe for most engines
        const float max_rage_delta = 800.0f;
        if (delta_x > max_rage_delta) delta_x = max_rage_delta;
        if (delta_x < -max_rage_delta) delta_x = -max_rage_delta;
        if (delta_y > max_rage_delta) delta_y = max_rage_delta;
        if (delta_y < -max_rage_delta) delta_y = -max_rage_delta;
        
        // Instant Snap (No smoothing)
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        input.mi.dx = static_cast<LONG>(delta_x);
        input.mi.dy = static_cast<LONG>(delta_y);
        SendInput(1, &input, sizeof(INPUT));
        
        // Auto-Shoot
        if (g_Settings.rage_auto_shoot)
        {
            // Simple triggerbot: if close enough to center, click
            float dist_to_center = std::hypot(aim_x - screen_cx, aim_y - screen_cy); // True distance check
            if (dist_to_center < 30.0f) // Tolerance
            {
                // Click
                INPUT inputs[2] = {};
                // ... click logic ... (kept simple for brevity in replacement, will use full block)
                inputs[0].type = INPUT_MOUSE;
                inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                inputs[1].type = INPUT_MOUSE;
                inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                SendInput(2, inputs, sizeof(INPUT));
            }
        }
        return;
    }

    // ========== LEGIT MODE ==========
    if (g_Settings.aimbot_legit_mode)
    {
        auto now = std::chrono::high_resolution_clock::now();
        
        // Reaction time delay
        if (waiting_for_reaction)
        {
            float elapsed_ms = std::chrono::duration<float, std::milli>(now - last_target_switch).count();
            float reaction_time = g_Settings.aimbot_legit_reaction;
            
            if (elapsed_ms < reaction_time) {
                return; // Still "reacting"
            }
            waiting_for_reaction = false;
        }
        
        // Calculate raw delta to target
        float raw_dx = aim_x - screen_cx;
        float raw_dy = aim_y - screen_cy;
        float dist = std::hypot(raw_dx, raw_dy);
        
        if (dist < 2.0f) return;
        
        // Dynamic smoothing
        float smooth = g_Settings.aimbot_smoothness * 4.0f;
        
        if (dist < 50.0f) smooth *= 0.8f;
        if (dist < 10.0f) smooth *= 0.5f;
        
        // Apply smoothing
        float delta_x = raw_dx / smooth;
        float delta_y = raw_dy / smooth;
        
        // Accumulate sub-pixel movements
        static float acc_x = 0.0f;
        static float acc_y = 0.0f;
        
        acc_x += delta_x;
        acc_y += delta_y;
        
        int move_x = static_cast<int>(acc_x);
        int move_y = static_cast<int>(acc_y);
        
        acc_x -= move_x;
        acc_y -= move_y;
        
        if (move_x == 0 && move_y == 0) return;
        
        // Send smooth mouse input
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        input.mi.dx = static_cast<LONG>(move_x);
        input.mi.dy = static_cast<LONG>(move_y);
        SendInput(1, &input, sizeof(INPUT));
        return;
    }

    // ========== NORMAL MODE (Legacy) ==========
    float delta_x = aim_x - screen_cx;
    float delta_y = aim_y - screen_cy;
    
    // Smoothness with Speed Limit
    float smooth_factor = g_Settings.aimbot_smoothness;
    if (smooth_factor > 0.0f) {
        delta_x /= smooth_factor;
        delta_y /= smooth_factor;
    }
    
    // Clamp max dragging speed for Normal Mode to prevent instant snaps at high FOV
    // This makes it look like a very fast drag instead of a glitchy teleport
    const float max_normal_speed = 40.0f; // Pixels per frame cap
    float speed = std::hypot(delta_x, delta_y);
    if (speed > max_normal_speed) {
        float scale = max_normal_speed / speed;
        delta_x *= scale;
        delta_y *= scale;
    }
    
    if (std::abs(delta_x) < 0.5f && std::abs(delta_y) < 0.5f) {
        return;
    }

    // Accumulate sub-pixel movements (Fixes stutter/lag)
    static float norm_acc_x = 0.0f;
    static float norm_acc_y = 0.0f;
    
    norm_acc_x += delta_x;
    norm_acc_y += delta_y;
    
    int move_x = static_cast<int>(norm_acc_x);
    int move_y = static_cast<int>(norm_acc_y);
    
    norm_acc_x -= move_x;
    norm_acc_y -= move_y;
    
    if (move_x == 0 && move_y == 0) return;
    
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = static_cast<LONG>(move_x);
    input.mi.dy = static_cast<LONG>(move_y);
    SendInput(1, &input, sizeof(INPUT));
}