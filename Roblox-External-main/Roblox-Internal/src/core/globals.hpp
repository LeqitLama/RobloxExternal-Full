#pragma once
#include <cstdint>
#include <memory>
#include <Windows.h>

// Forward declare lua_State (defined in Luau headers)
struct lua_State;

namespace globals
{
    inline uintptr_t roblox_base = 0;
    inline uintptr_t data_model = 0;
    inline uintptr_t render_view = 0;
    inline uintptr_t visual_engine = 0;
    inline uintptr_t script_context = 0;
    inline lua_State* lua_state = nullptr;
    inline bool initialized = false;
    inline HMODULE dll_module = nullptr;
}
