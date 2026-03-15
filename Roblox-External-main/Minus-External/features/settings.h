#pragma once
#include <string>

struct Settings {
    // ========== AIMBOT ==========
    bool aimbot_enabled = true;
    bool aimbot_team_check = true;
    float aimbot_fov = 100.0f;
    float aimbot_smoothness = 5.0f;
    int aimbot_key = 0x05; // VK_XBUTTON1 (Mouse4)
    int aimbot_key2 = 0x06; // VK_XBUTTON2 (Mouse5)
    bool aimbot_prediction = false;
    float aimbot_prediction_scale = 1.0f;
    int aimbot_target_bone = 0; // 0 = Head, 1 = Torso
    
    // Legit Mode
    bool aimbot_legit_mode = false; // Human-like aim for clips
    float aimbot_legit_reaction = 150.0f; // Reaction time in ms

    // ========== RAGE MODE ==========
    bool rage_enabled = false;
    bool rage_auto_shoot = false;
    bool rage_silent_aim = false; // Silent aim (memory write to PlayerMouse)
    
    // Resolver (Anti-aim counter)
    bool resolver_enabled = true;         // Enable anti-aim detection
    bool resolver_visualize = false;      // Show resolver info on ESP
    bool resolver_prediction = true;      // Use velocity prediction
    
    // ========== BACKTRACK ==========
    bool backtrack_enabled = false;
    float backtrack_time_ms = 100.0f;
    bool backtrack_visualize = false;
    
    // ========== VISUALS ==========
    bool esp_enabled = true;
    bool esp_box = true;
    bool esp_names = true;
    bool esp_distance = true;
    bool esp_health_bar = true;
    bool esp_team_check = true;
    bool esp_tracers = false;
    bool show_fov_circle = true;
    bool show_crosshair = false;
    float crosshair_size = 10.0f;
    
    // ========== COLORS ==========
    float esp_color_enemy[3] = { 1.0f, 0.3f, 0.3f };
    float esp_color_team[3] = { 0.3f, 1.0f, 0.3f };
    
    // ========== BLATANT ==========
    bool fly_enabled = false;
    float fly_speed = 1.0f;
    int fly_key = 'F';
    
    // Teleport
    int teleport_selected_index = -1;
    bool teleport_to_closest = false;
    
    // ========== HVH ==========
    bool antiaim_enabled = false;
    int antiaim_type = 0; // 0 = Spin, 1 = Jitter, 2 = Random
    float antiaim_speed = 10.0f;
    
    // For keybind UI
    bool waiting_for_key = false;
    bool waiting_for_fly_key = false;
};

inline const char* GetKeyName(int vk) {
    switch(vk) {
        case 0x01: return "LMB";
        case 0x02: return "RMB";
        case 0x04: return "MMB";
        case 0x05: return "Mouse4";
        case 0x06: return "Mouse5";
        case 0x10: return "Shift";
        case 0x11: return "Ctrl";
        case 0x12: return "Alt";
        case 0x20: return "Space";
        case 'A': return "A"; case 'B': return "B"; case 'C': return "C";
        case 'D': return "D"; case 'E': return "E"; case 'F': return "F";
        case 'G': return "G"; case 'H': return "H"; case 'I': return "I";
        case 'J': return "J"; case 'K': return "K"; case 'L': return "L";
        case 'M': return "M"; case 'N': return "N"; case 'O': return "O";
        case 'P': return "P"; case 'Q': return "Q"; case 'R': return "R";
        case 'S': return "S"; case 'T': return "T"; case 'U': return "U";
        case 'V': return "V"; case 'W': return "W"; case 'X': return "X";
        case 'Y': return "Y"; case 'Z': return "Z";
        default: return "???";
    }
}

extern Settings g_Settings;
