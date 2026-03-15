#include "esp.h"
#include "settings.h"
#include "aimbot.cpp"
#include "silentaim.h"
#include "worldmods.h"
#include "backtrack.h"
#include <iostream>
#include <chrono>
#include <unordered_map>
#include <immintrin.h> 
#include "../imgui/imgui.h"
#include "../overlay/overlay.h"
#include "../sdk/offsets.h"

static std::vector<Player> cached_players;
static std::chrono::high_resolution_clock::time_point last_player_update;
static std::string temp_string;

ActorLoopClass::ActorLoopClass() 
{
    memory = std::make_unique<Memory>();
    game_base = 0;
    local_player = 0;
    local_player_team = 0;
    local_player_position = {0, 0, 0};
    screen_width = 1920.0f;
    screen_height = 1080.0f;
    
    cached_players.reserve(16);
}

bool ActorLoopClass::Initialize() 
{
    if (!memory->AttachToProcess("RobloxPlayerBeta.exe"))
    {
        std::cout << "Failed to attach to Roblox process" << std::endl;
        return false;
    }
   
    game_base = memory->GetBaseAddress();
    if (!game_base) 
    {
        std::cout << "Failed to get game base address" << std::endl;
        return false;
    }
    
    return true;
}

std::string ActorLoopClass::ReadString(uintptr_t address)
{
    temp_string.clear();
    temp_string.reserve(64);
    
    char character = 0;
    int offset = 0;

    while (offset < 200)
    {
        character = memory->ReadMemory<char>(address + offset);

        if (character == 0)
            break;

        offset++;
        temp_string.push_back(character);
    }

    return temp_string;
}

std::string ActorLoopClass::LengthReadString(uintptr_t string)
{
    const auto length = memory->ReadMemory<int>(string + Offsets::StringLength);

    if (length >= 16u)
    {
        const auto _new = memory->ReadMemory<uintptr_t>(string);
        return ReadString(_new);
    }
    else
    {
        return ReadString(string);
    }
}

std::string ActorLoopClass::GetInstanceName(uintptr_t instance_address)
{
    const auto _get = memory->ReadMemory<uintptr_t>(instance_address + Offsets::Name);

    if (_get)
        return LengthReadString(_get);

    return "???";
}

std::string ActorLoopClass::GetInstanceClassName(uintptr_t instance_address)
{
    const auto ptr = memory->ReadMemory<uintptr_t>(instance_address + Offsets::ClassDescriptor);
    const auto ptr2 = memory->ReadMemory<uintptr_t>(ptr + Offsets::ChildrenEnd);

    if (ptr2)
        return ReadString(ptr2);

    return "???";
}

uintptr_t ActorLoopClass::FindFirstChild(uintptr_t instance_address, const std::string& child_name)
{
    if (!instance_address)
        return 0;

    static std::unordered_map<uintptr_t, std::vector<std::pair<uintptr_t, std::string>>> cache;
    static std::unordered_map<uintptr_t, std::chrono::high_resolution_clock::time_point> last_update;

    auto now = std::chrono::high_resolution_clock::now();
    auto& children = cache[instance_address];
    auto& update_time = last_update[instance_address];

    if (children.empty() || now - update_time > std::chrono::seconds(1))
    {
        children.clear();
        
        auto start = memory->ReadMemory<uintptr_t>(instance_address + Offsets::Children);
        if (!start)
            return 0;

        auto end = memory->ReadMemory<uintptr_t>(start + Offsets::ChildrenEnd);
        auto childArray = memory->ReadMemory<uintptr_t>(start);
        if (!childArray || childArray >= end)
            return 0;

        children.reserve(32);

        for (uintptr_t current = childArray; current < end; current += 16)
        {
            auto child_instance = memory->ReadMemory<uintptr_t>(current);
            if (!child_instance)
                continue;
            std::string name = GetInstanceName(child_instance);
            children.emplace_back(child_instance, std::move(name));
        }
        update_time = now;
    }

    for (const auto& [child_instance, name] : children)
    {
        if (name == child_name)
            return child_instance;
    }

    return 0;
}

uintptr_t ActorLoopClass::FindFirstChildByClass(uintptr_t instance_address, const std::string& class_name)
{
    if (!instance_address)
        return 0;

    static std::unordered_map<uintptr_t, std::vector<std::pair<uintptr_t, std::string>>> cache;
    static std::unordered_map<uintptr_t, std::chrono::high_resolution_clock::time_point> last_update;

    auto now = std::chrono::high_resolution_clock::now();
    auto& children = cache[instance_address];
    auto& update_time = last_update[instance_address];

    if (children.empty() || now - update_time > std::chrono::seconds(1))
    {
        children.clear();
        children.reserve(32);
        
        auto start = memory->ReadMemory<uintptr_t>(instance_address + Offsets::Children);
        if (!start)
            return 0;

        auto end = memory->ReadMemory<uintptr_t>(start + Offsets::ChildrenEnd);
        auto childArray = memory->ReadMemory<uintptr_t>(start);
        if (!childArray || childArray >= end)
            return 0;

        for (uintptr_t current = childArray; current < end; current += 16)
        {
            auto child_instance = memory->ReadMemory<uintptr_t>(current);
            if (!child_instance)
                continue;
            std::string classname = GetInstanceClassName(child_instance);
            children.emplace_back(child_instance, std::move(classname));
        }
        update_time = now;
    }

    for (const auto& [child_instance, classname] : children)
    {
        if (classname == class_name)
            return child_instance;
    }

    return 0;
}

std::vector<Player> ActorLoopClass::GetPlayers()
{
    auto now = std::chrono::high_resolution_clock::now();
    
    static std::vector<uintptr_t> cached_player_addresses;
    static std::chrono::high_resolution_clock::time_point last_structure_update;
    
    bool need_structure_update = cached_player_addresses.empty() || 
                                now - last_structure_update > std::chrono::milliseconds(500);
    
    if (need_structure_update)
    {
        cached_player_addresses.clear();
        
        uintptr_t fake_datamodel = memory->ReadMemory<uintptr_t>(game_base + Offsets::FakeDataModelPointer);
        if (!fake_datamodel) return cached_players;
        
        uintptr_t datamodel = memory->ReadMemory<uintptr_t>(fake_datamodel + Offsets::FakeDataModelToDataModel);
        if (!datamodel) return cached_players;
        
        uintptr_t players_service = FindFirstChildByClass(datamodel, "Players");
        if (!players_service) return cached_players;
        
        local_player = memory->ReadMemory<uintptr_t>(players_service + Offsets::LocalPlayer);
        
        if (local_player)
        {
            local_player_team = memory->ReadMemory<uintptr_t>(local_player + Offsets::Team);
        }
        
        auto start = memory->ReadMemory<uintptr_t>(players_service + Offsets::Children);
        if (!start) return cached_players;
        auto end = memory->ReadMemory<uintptr_t>(start + Offsets::ChildrenEnd);
        
        static std::vector<uintptr_t> all_children;
        all_children.clear();
        all_children.reserve(16);
        
        for (auto instances = memory->ReadMemory<uintptr_t>(start); instances != end; instances += 16)
        {
            uintptr_t child = memory->ReadMemory<uintptr_t>(instances);
            if (child) all_children.push_back(child);
        }
        
        for (const auto& child : all_children)
        {
            std::string class_name = GetInstanceClassName(child);
            if (class_name == "Player" && child != local_player)
            {
                cached_player_addresses.push_back(child);
            }
        }
        
        last_structure_update = now;
    }
    
    cached_players.clear();
    cached_players.reserve(cached_player_addresses.size());
    
    int local_team = 0;
    if (local_player)
    {
        local_team = memory->ReadMemory<int>(local_player + Offsets::Team);
        
        // Get local player position for distance calculation
        uintptr_t local_char = memory->ReadMemory<uintptr_t>(local_player + Offsets::ModelInstance);
        if (local_char)
        {
            uintptr_t local_root = FindFirstChild(local_char, "HumanoidRootPart");
            if (local_root)
            {
                uintptr_t local_prim = memory->ReadMemory<uintptr_t>(local_root + Offsets::Primitive);
                if (local_prim)
                {
                    local_player_position = memory->ReadMemory<Vector3>(local_prim + Offsets::Position);
                }
            }
        }
    }
    
    for (const auto& player_addr : cached_player_addresses)
    {
        Player player;
        player.address = player_addr;
        player.valid = false;
        player.health = 100.0f;
        player.maxHealth = 100.0f;
        player.team = 0;
        player.distance = 0.0f;
        
        player.name = GetInstanceName(player_addr);
        
        int player_team = memory->ReadMemory<int>(player_addr + Offsets::Team);
        player.team = player_team;
        
        // Teammates Check
        if (g_Settings.esp_team_check && local_team != 0 && player_team == local_team)
        {
            continue;
        }
        
        uintptr_t character = memory->ReadMemory<uintptr_t>(player_addr + Offsets::ModelInstance);
        if (!character) continue;
        
        uintptr_t root_part = FindFirstChild(character, "HumanoidRootPart");
        if (!root_part) continue;
        
        uintptr_t head_part = FindFirstChild(character, "Head");
        uintptr_t humanoid = FindFirstChildByClass(character, "Humanoid");
        
        uintptr_t primitive = memory->ReadMemory<uintptr_t>(root_part + Offsets::Primitive);
        if (!primitive) continue;
        
        player.position = memory->ReadMemory<Vector3>(primitive + Offsets::Position);
        
        // Calculate distance from local player
        float dx = player.position.x - local_player_position.x;
        float dy = player.position.y - local_player_position.y;
        float dz = player.position.z - local_player_position.z;
        player.distance = sqrtf(dx*dx + dy*dy + dz*dz);
        
        if (head_part)
        {
            uintptr_t head_primitive = memory->ReadMemory<uintptr_t>(head_part + Offsets::Primitive);
            if (head_primitive)
            {
                player.head_position = memory->ReadMemory<Vector3>(head_primitive + Offsets::Position);
            }
            else
            {
                player.head_position = player.position;
                player.head_position.y += 2.5f;
            }
        }
        else
        {
            player.head_position = player.position;
            player.head_position.y += 2.5f;
        }
        
        if (humanoid)
        {
            player.health = memory->ReadMemory<float>(humanoid + Offsets::Health);
            player.maxHealth = memory->ReadMemory<float>(humanoid + Offsets::MaxHealth);
            
            if (player.maxHealth <= 0.0f) player.maxHealth = 100.0f;
            if (player.health < 0.0f) player.health = 0.0f;
            if (player.health > player.maxHealth) player.health = player.maxHealth;
        }
        
        if (player.health <= 0.0f)
        {
            continue;
        }
        
        player.valid = true;
        cached_players.push_back(std::move(player));
    }
    
    return cached_players;
}

void ActorLoopClass::Render()
{
    if (!memory) return;
    
    GetScreenDimensions(screen_width, screen_height);
    
    auto view_matrix = GetViewMatrix();
    auto players = GetPlayers();
    
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    
    for (auto& player : players)
    {
        if (!player.valid) continue;
        
        CalculateBox(player, view_matrix);
        
        if (player.box_width > 0 && player.box_height > 0)
        {
            draw_list->AddRect(
                ImVec2(player.box_top_left.x, player.box_top_left.y),
                ImVec2(player.box_bottom_right.x, player.box_bottom_right.y),
                IM_COL32(255, 0, 0, 255),
                0.0f,
                0,
                2.0f
            );
            
            float text_offset = player.name.length() * 3.5f;
            ImVec2 name_pos(player.box_top_left.x + (player.box_width / 2.0f) - text_offset, player.box_top_left.y - 20);
            draw_list->AddText(name_pos, IM_COL32(255, 255, 255, 255), player.name.c_str());
        }
    }
}

void ActorLoopClass::Render(Overlay* overlay)
{
    if (!memory || !overlay) return;
    
    GetScreenDimensions(screen_width, screen_height);
    
    auto view_matrix = GetViewMatrix();
    auto players = GetPlayers();
    
    if (players.empty())
    {
        overlay->DrawPlayerCount(0);
        return;
    }
    
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    
    ImFont* minecraft_font = overlay->GetMinecraftFont();
    
    int valid_player_count = 0;
    
    for (auto& player : players)
    {
        if (!player.valid) continue;
        
        valid_player_count++;
        
        CalculateBox(player, view_matrix);
        
        if (player.box_width > 0 && player.box_height > 0)
        {
            float x = player.box_top_left.x;
            float y = player.box_top_left.y;
            float w = player.box_width;
            float h = player.box_height;
            
            // Corner size (proportional)
            float c = h * 0.25f;
            c = fmaxf(5.0f, fminf(c, 18.0f));
            
            ImU32 esp_color = IM_COL32(
                (int)(g_Settings.esp_color_enemy[0] * 255),
                (int)(g_Settings.esp_color_enemy[1] * 255),
                (int)(g_Settings.esp_color_enemy[2] * 255),
                255
            );
            ImU32 outline = IM_COL32(0, 0, 0, 200);
            ImU32 white = IM_COL32(255, 255, 255, 255);
            
            // Box with corners (drawn with outline for cleaner look)
            if (g_Settings.esp_box) {
                float t = 2.0f; // thickness
                
                // Draw outline first (slightly offset)
                // Top-left
                draw_list->AddLine(ImVec2(x-1, y), ImVec2(x + c+1, y), outline, t+2);
                draw_list->AddLine(ImVec2(x, y-1), ImVec2(x, y + c+1), outline, t+2);
                // Top-right
                draw_list->AddLine(ImVec2(x + w+1, y), ImVec2(x + w - c-1, y), outline, t+2);
                draw_list->AddLine(ImVec2(x + w, y-1), ImVec2(x + w, y + c+1), outline, t+2);
                // Bottom-left
                draw_list->AddLine(ImVec2(x-1, y + h), ImVec2(x + c+1, y + h), outline, t+2);
                draw_list->AddLine(ImVec2(x, y + h+1), ImVec2(x, y + h - c-1), outline, t+2);
                // Bottom-right
                draw_list->AddLine(ImVec2(x + w+1, y + h), ImVec2(x + w - c-1, y + h), outline, t+2);
                draw_list->AddLine(ImVec2(x + w, y + h+1), ImVec2(x + w, y + h - c-1), outline, t+2);
                
                // Draw colored corners
                // Top-left
                draw_list->AddLine(ImVec2(x, y), ImVec2(x + c, y), esp_color, t);
                draw_list->AddLine(ImVec2(x, y), ImVec2(x, y + c), esp_color, t);
                // Top-right
                draw_list->AddLine(ImVec2(x + w, y), ImVec2(x + w - c, y), esp_color, t);
                draw_list->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + c), esp_color, t);
                // Bottom-left
                draw_list->AddLine(ImVec2(x, y + h), ImVec2(x + c, y + h), esp_color, t);
                draw_list->AddLine(ImVec2(x, y + h), ImVec2(x, y + h - c), esp_color, t);
                // Bottom-right
                draw_list->AddLine(ImVec2(x + w, y + h), ImVec2(x + w - c, y + h), esp_color, t);
                draw_list->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - c), esp_color, t);
            }
            
            // Health bar (left side, gradient)
            if (g_Settings.esp_health_bar) {
                float hp = player.health / player.maxHealth;
                hp = fmaxf(0.0f, fminf(hp, 1.0f));
                
                float bar_x = x - 6.0f;
                float bar_w = 3.0f;
                float bar_h = h * hp;
                
                // Health color (green to red gradient)
                int r = (int)((1.0f - hp) * 255);
                int g = (int)(hp * 255);
                ImU32 hp_color = IM_COL32(r, g, 50, 255);
                
                // Background
                draw_list->AddRectFilled(ImVec2(bar_x - 1, y - 1), ImVec2(bar_x + bar_w + 1, y + h + 1), IM_COL32(0, 0, 0, 180));
                // Health fill
                draw_list->AddRectFilled(ImVec2(bar_x, y + h - bar_h), ImVec2(bar_x + bar_w, y + h), hp_color);
            }
            
            if (minecraft_font) ImGui::PushFont(minecraft_font);
            
            // Name (centered above box)
            if (g_Settings.esp_names) {
                ImVec2 ns = ImGui::CalcTextSize(player.name.c_str());
                float nx = x + (w - ns.x) * 0.5f;
                float ny = y - ns.y - 3.0f;
                
                // Drop shadow
                draw_list->AddText(ImVec2(nx + 1, ny + 1), IM_COL32(0, 0, 0, 200), player.name.c_str());
                // Main text
                draw_list->AddText(ImVec2(nx, ny), white, player.name.c_str());
            }
            
            // Resolver indicator (shows anti-aim type if detected)
            if (g_Settings.resolver_enabled && g_Settings.resolver_visualize) {
                auto resolver_data = g_SilentAim.GetResolverData(player.address);
                if (resolver_data && resolver_data->is_using_antiaim && resolver_data->confidence > 0.3f) {
                    const char* aa_type = "AA";
                    ImU32 aa_color = IM_COL32(255, 100, 100, 255);
                    
                    switch (resolver_data->antiaim_type) {
                        case 1: aa_type = "[SPIN]"; aa_color = IM_COL32(255, 150, 50, 255); break;
                        case 2: aa_type = "[JITTER]"; aa_color = IM_COL32(255, 50, 150, 255); break;
                        case 3: aa_type = "[RANDOM]"; aa_color = IM_COL32(150, 50, 255, 255); break;
                    }
                    
                    ImVec2 aa_size = ImGui::CalcTextSize(aa_type);
                    float aax = x + (w - aa_size.x) * 0.5f;
                    float aay = y - aa_size.y - 20.0f;
                    
                    // Drop shadow
                    draw_list->AddText(ImVec2(aax + 1, aay + 1), IM_COL32(0, 0, 0, 200), aa_type);
                    // Colored text
                    draw_list->AddText(ImVec2(aax, aay), aa_color, aa_type);
                }
            }
            
            // Distance (centered below box)
            if (g_Settings.esp_distance) {
                char dt[16];
                sprintf_s(dt, "%.0fm", player.distance);
                ImVec2 ds = ImGui::CalcTextSize(dt);
                float ddx = x + (w - ds.x) * 0.5f;
                float ddy = y + h + 3.0f;
                
                // Drop shadow
                draw_list->AddText(ImVec2(ddx + 1, ddy + 1), IM_COL32(0, 0, 0, 200), dt);
                // Main text (soft yellow)
                draw_list->AddText(ImVec2(ddx, ddy), IM_COL32(255, 230, 140, 255), dt);
            }
            
            if (minecraft_font) ImGui::PopFont();
        }

        // Tracers (gradient from bottom of screen to player)
        if (g_Settings.esp_tracers)
        {
             draw_list->AddLine(
                ImVec2(screen_width / 2, screen_height),
                ImVec2(player.box_top_left.x + player.box_width / 2, player.box_top_left.y + player.box_height),
                IM_COL32(255, 255, 255, 120),
                1.5f
            );
        }
    }

    // Crosshair
    if (g_Settings.show_crosshair)
    {
        float cx = screen_width / 2.0f;
        float cy = screen_height / 2.0f;
        float cs = g_Settings.crosshair_size;
        draw_list->AddLine(ImVec2(cx - cs, cy), ImVec2(cx + cs, cy), IM_COL32(255, 0, 0, 255), 2.0f);
        draw_list->AddLine(ImVec2(cx, cy - cs), ImVec2(cx, cy + cs), IM_COL32(255, 0, 0, 255), 2.0f);
    }
    
    // Update Backtrack with current player positions
    g_Backtrack.Update(players, screen_width, screen_height);
    
    // Visualize backtrack positions if enabled
    if (g_Settings.backtrack_enabled && g_Settings.backtrack_visualize)
    {
        for (const auto& player : players)
        {
            if (!player.valid) continue;
            
            const auto& records = g_Backtrack.GetRecords(player.address);
            int record_idx = 0;
            for (const auto& record : records)
            {
                if (!record.valid || record.box_width <= 0) continue;
                
                // Draw small circles at historical head positions
                float aim_x = record.box_top_left.x + (record.box_width * 0.5f);
                float aim_y = record.box_top_left.y + (record.box_height * 0.1f);
                
                // Fade out older positions
                int alpha = 200 - (record_idx * 15);
                if (alpha < 30) alpha = 30;
                
                draw_list->AddCircleFilled(
                    ImVec2(aim_x, aim_y),
                    3.0f,
                    IM_COL32(255, 100, 100, alpha)
                );
                
                record_idx++;
                if (record_idx > 12) break; // Limit visualization
            }
        }
    }
    
    // Run aimbot (mouse movement)
    g_Aimbot.Run(players, screen_width, screen_height);
    
    // Initialize Backtrack, World Mods, and Silent Aim
    static bool backtrack_initialized = false;
    static bool world_mods_initialized = false;
    static bool silent_aim_initialized = false;
    if (!backtrack_initialized) {
        g_Backtrack.Initialize(memory.get(), game_base);
        backtrack_initialized = true;
    }
    if (!world_mods_initialized) {
        g_WorldMods.Initialize(memory.get(), game_base);
        world_mods_initialized = true;
    }
    if (!silent_aim_initialized) {
        g_SilentAim.Initialize(memory.get(), game_base);
        silent_aim_initialized = true;
    }
    
    // Run Silent Aim (writes target position to PlayerMouse)
    g_SilentAim.Run(players, local_player_position, screen_width, screen_height);
    
    // Run Triggerbot and Movement Mods (Fly/Noclip)
    g_WorldMods.Run(players, screen_width, screen_height);
    g_WorldMods.RunTriggerbot(players, screen_width, screen_height);
    
    // Teleport to Closest Logic
    if (g_Settings.teleport_to_closest)
    {
        float min_dist = 999999.0f;
        Vector3 target_pos = { 0, 0, 0 };
        bool found = false;
        
        for (const auto& player : players)
        {
            if (!player.valid) continue;
            
            if (player.distance < min_dist)
            {
                min_dist = player.distance;
                target_pos = player.position;
                found = true;
            }
        }
        
        if (found)
        {
            g_WorldMods.TeleportToPlayer(target_pos);
        }
        
        g_Settings.teleport_to_closest = false;
    }
    
    overlay->DrawPlayerCount(valid_player_count);
}

Matrix4x4 ActorLoopClass::GetViewMatrix()
{
    Matrix4x4 view_matrix{};
    
    uintptr_t visual_engine = memory->ReadMemory<uintptr_t>(game_base + Offsets::VisualEnginePointer);
    if (!visual_engine) return view_matrix;
    
    view_matrix = memory->ReadMemory<Matrix4x4>(visual_engine + Offsets::viewmatrix);
    return view_matrix;
}

bool ActorLoopClass::WorldToScreen(const Vector3& world_pos, Vector2D& screen_pos, const Matrix4x4& view_matrix) 
{
    const float* m = &view_matrix.m[0][0];

    _mm_prefetch((const char*)m, _MM_HINT_T0);

    float w = m[12] * world_pos.x + m[13] * world_pos.y + m[14] * world_pos.z + m[15];

    if (w <= 0.001f)
        return false;

    float inv_w = 1.0f / w;

    screen_pos.x = (m[0] * world_pos.x + m[1] * world_pos.y + m[2] * world_pos.z + m[3]) * inv_w;
    screen_pos.y = (m[4] * world_pos.x + m[5] * world_pos.y + m[6] * world_pos.z + m[7]) * inv_w;

    float screen_width_half = screen_width * 0.5f;
    float screen_height_half = screen_height * 0.5f;

    screen_pos.x = screen_width_half * (screen_pos.x + 1.0f);
    screen_pos.y = screen_height_half * (1.0f - screen_pos.y);

    return true;
}

void ActorLoopClass::GetScreenDimensions(float& width, float& height)
{
    RECT desktop;
    const HWND hDesktop = GetDesktopWindow();
    GetWindowRect(hDesktop, &desktop);
    width = static_cast<float>(desktop.right);
    height = static_cast<float>(desktop.bottom);
}

void ActorLoopClass::CalculateBox(Player& player, const Matrix4x4& view_matrix)
{
    Vector3 bottom_pos = player.position;
    bottom_pos.y -= 3.5f; 
    
    Vector3 top_pos = player.position;
    top_pos.y += 2.5f; 
    
    Vector2D screen_bottom, screen_top;
    
    if (!WorldToScreen(bottom_pos, screen_bottom, view_matrix))
    {
        player.box_width = 0;
        player.box_height = 0;
        return;
    }
    
    if (!WorldToScreen(top_pos, screen_top, view_matrix))
    {
        player.box_width = 0;
        player.box_height = 0;
        return;
    }
    
    float height = screen_bottom.y - screen_top.y;
    
    if (height < 2.0f) height = 2.0f;
    
    float width = height * 0.65f;
    
    float center_x = (screen_bottom.x + screen_top.x) / 2.0f;
    
    player.box_top_left.x = center_x - (width / 2.0f);
    player.box_top_left.y = screen_top.y;
    player.box_bottom_right.x = center_x + (width / 2.0f);
    player.box_bottom_right.y = screen_bottom.y;
    player.box_width = width;
    player.box_height = height;
} 