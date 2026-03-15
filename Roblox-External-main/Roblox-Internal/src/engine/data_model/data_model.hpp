#pragma once
#include <cstdint>
#include <memory>

namespace roblox_engine
{
    class data_model_t
    {
    public:
        uintptr_t get();
        uintptr_t get_service(const char* service_name);
        uintptr_t find_first_child(uintptr_t instance, const char* name);
        uintptr_t get_children(uintptr_t instance);
        const char* get_name(uintptr_t instance);
        const char* get_class_name(uintptr_t instance);
    };

    inline auto data_model = std::make_unique<data_model_t>();
}
