#include "data_model.hpp"
#include "../../core/globals.hpp"
#include "../../core/offsets.hpp"
#include "../pointer_encryption/pointer_encryption.hpp"
#include <string>

namespace roblox_engine
{
    uintptr_t data_model_t::get()
    {
        if (globals::data_model)
            return globals::data_model;

        // Try to get from FakeDataModelPointer
        uintptr_t dm_ptr = *reinterpret_cast<uintptr_t*>(Offsets::FakeDataModelPointer);
        if (dm_ptr)
        {
            globals::data_model = dm_ptr;
            return dm_ptr;
        }

        return 0;
    }

    uintptr_t data_model_t::get_service(const char* service_name)
    {
        uintptr_t dm = get();
        if (!dm)
            return 0;

        return find_first_child(dm, service_name);
    }

    uintptr_t data_model_t::find_first_child(uintptr_t instance, const char* name)
    {
        if (!instance || !name)
            return 0;

        // Instance children offset - you need to update this
        constexpr uintptr_t INSTANCE_CHILDREN = 0x50;
        
        uintptr_t children_ptr = *reinterpret_cast<uintptr_t*>(instance + INSTANCE_CHILDREN);
        if (!children_ptr)
            return 0;

        uintptr_t children_start = *reinterpret_cast<uintptr_t*>(children_ptr);
        uintptr_t children_end = *reinterpret_cast<uintptr_t*>(children_ptr + sizeof(uintptr_t));

        for (uintptr_t child = children_start; child < children_end; child += sizeof(uintptr_t))
        {
            uintptr_t child_instance = *reinterpret_cast<uintptr_t*>(child);
            if (!child_instance)
                continue;

            const char* child_name = get_name(child_instance);
            if (child_name && strcmp(child_name, name) == 0)
                return child_instance;
        }

        return 0;
    }

    uintptr_t data_model_t::get_children(uintptr_t instance)
    {
        if (!instance)
            return 0;

        constexpr uintptr_t INSTANCE_CHILDREN = 0x50;
        return *reinterpret_cast<uintptr_t*>(instance + INSTANCE_CHILDREN);
    }

    const char* data_model_t::get_name(uintptr_t instance)
    {
        if (!instance)
            return nullptr;

        constexpr uintptr_t INSTANCE_NAME = 0x48;
        uintptr_t name_ptr = *reinterpret_cast<uintptr_t*>(instance + INSTANCE_NAME);
        if (!name_ptr)
            return nullptr;

        return reinterpret_cast<const char*>(name_ptr);
    }

    const char* data_model_t::get_class_name(uintptr_t instance)
    {
        if (!instance)
            return nullptr;

        constexpr uintptr_t INSTANCE_CLASS_DESCRIPTOR = 0x18;
        uintptr_t class_descriptor = *reinterpret_cast<uintptr_t*>(instance + INSTANCE_CLASS_DESCRIPTOR);
        if (!class_descriptor)
            return nullptr;

        uintptr_t class_name_ptr = *reinterpret_cast<uintptr_t*>(class_descriptor + 0x8);
        if (!class_name_ptr)
            return nullptr;

        return reinterpret_cast<const char*>(class_name_ptr);
    }
}
