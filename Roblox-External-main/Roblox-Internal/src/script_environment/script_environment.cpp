#include "script_environment.hpp"

void script_environment_t::initialize()
{
    if (!globals::lua_state)
        return;

    // Register all custom libraries
    /*
    Example libraries to add:
    
    libraries.push_back(std::make_unique<cache_library>());
    libraries.push_back(std::make_unique<closure_library>());
    libraries.push_back(std::make_unique<crypt_library>());
    libraries.push_back(std::make_unique<drawing_library>());
    libraries.push_back(std::make_unique<filesystem_library>());
    libraries.push_back(std::make_unique<http_library>());
    libraries.push_back(std::make_unique<input_library>());
    libraries.push_back(std::make_unique<metatable_library>());
    libraries.push_back(std::make_unique<websocket_library>());
    */

    // Initialize each library
    for (auto& library : libraries)
    {
        library->initialize(globals::lua_state);
    }

    // Push global environment modifications
    push_globals(globals::lua_state);
}

void script_environment_t::register_library(std::unique_ptr<library_t> library)
{
    libraries.push_back(std::move(library));
}

void script_environment_t::push_globals(lua_State* L)
{
    if (!L)
        return;

    /*
    Example global functions to register:
    
    // getgenv
    lua_pushfunction(L, [](lua_State* L) -> int {
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        return 1;
    });
    lua_setglobal(L, "getgenv");
    
    // getrenv
    lua_pushfunction(L, [](lua_State* L) -> int {
        // Return the Roblox environment
        return 1;
    });
    lua_setglobal(L, "getrenv");
    
    // identifyexecutor
    lua_pushfunction(L, [](lua_State* L) -> int {
        lua_pushstring(L, configuration::name);
        lua_pushstring(L, configuration::version);
        return 2;
    });
    lua_setglobal(L, "identifyexecutor");
    */
}
