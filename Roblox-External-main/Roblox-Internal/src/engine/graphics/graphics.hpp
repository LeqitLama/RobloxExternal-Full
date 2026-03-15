#pragma once
#include <cstdint>
#include <memory>

namespace roblox_engine
{
    class graphics_t
    {
    public:
        uintptr_t get_render_view();
        uintptr_t get_visual_engine();
        bool initialize();
    };

    inline auto graphics = std::make_unique<graphics_t>();
}
