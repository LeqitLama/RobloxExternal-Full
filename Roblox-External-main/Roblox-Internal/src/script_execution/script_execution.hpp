#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

#include "../core/globals.hpp"
#include "../core/offsets.hpp"

// Luau includes
#include <Luau/BytecodeBuilder.h>
#include <Luau/Compiler.h>
#include <zstd.h>

#define XXH_INLINE_ALL
#include <xxhash.h>

class script_execution_t
{
private:
    // Compress bytecode for Roblox's format
    std::string compress_bytecode(const std::string& bytecode);
    
    // Custom bytecode encoder for Roblox
    class roblox_bytecode_encoder : public Luau::BytecodeEncoder
    {
    public:
        void encode(uint32_t* data, size_t count) override;
    };

public:
    // Execute a Lua script
    bool execute_script(const std::string& script);
    
    // Execute compiled bytecode directly
    bool execute_bytecode(const std::string& bytecode);
    
    // Initialize the execution environment
    bool initialize();
    
    // Get lua state from ScriptContext
    lua_State* get_lua_state();
};

inline auto script_execution = std::make_unique<script_execution_t>();
