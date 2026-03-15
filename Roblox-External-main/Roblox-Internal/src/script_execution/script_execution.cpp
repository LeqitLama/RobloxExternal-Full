#include "script_execution.hpp"
#include <lbytecode.h>

// Roblox bytecode encoder - multiplies opcodes by 227
void script_execution_t::roblox_bytecode_encoder::encode(uint32_t* data, size_t count)
{
    // Simple opcode encoder that Roblox uses
    for (size_t i = 0; i < count;)
    {
        auto& opcode = *reinterpret_cast<uint8_t*>(data + i);
        // Basic increment - opcodes have varying lengths but we process one at a time
        i++;
        opcode *= 227;  // Roblox's opcode encoding
    }
}

std::string script_execution_t::compress_bytecode(const std::string& bytecode)
{
    const size_t data_size = bytecode.size();
    const size_t max_size = ZSTD_compressBound(data_size);
    std::vector<char> buffer(max_size + 8);

    // RSB1 header (Roblox Signed Bytecode version 1)
    memcpy(&buffer[0], "RSB1", 4);
    memcpy(&buffer[4], &data_size, sizeof(uint32_t));

    const size_t compressed_size = ZSTD_compress(&buffer[8], max_size, bytecode.data(), data_size, ZSTD_maxCLevel());
    if (ZSTD_isError(compressed_size))
    {
        printf("[ERROR] Failed to compress bytecode: %s\n", ZSTD_getErrorName(compressed_size));
        return "";
    }

    const size_t size = compressed_size + 8;
    const uint32_t key = XXH32(buffer.data(), size, 42u);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&key);

    // XOR encryption with key
    for (size_t i = 0; i < size; ++i)
        buffer[i] ^= bytes[i % 4] + static_cast<uint8_t>(i * 41u);

    return std::string(buffer.data(), size);
}

bool script_execution_t::execute_script(const std::string& script)
{
    lua_State* L = get_lua_state();
    if (!L)
    {
        printf("[ERROR] No lua state available!\n");
        return false;
    }

    // Compile the script using Luau
    static roblox_bytecode_encoder encoder;
    
    Luau::CompileOptions options;
    options.optimizationLevel = 1;
    options.debugLevel = 1;
    
    std::string bytecode = Luau::compile(script, options, {}, &encoder);
    
    if (bytecode.empty() || bytecode[0] == '\0')
    {
        printf("[ERROR] Failed to compile script!\n");
        return false;
    }

    // Compress the bytecode
    std::string compressed = compress_bytecode(bytecode);
    if (compressed.empty())
        return false;

    return execute_bytecode(compressed);
}

bool script_execution_t::execute_bytecode(const std::string& bytecode)
{
    lua_State* L = get_lua_state();
    if (!L)
        return false;

    // Use Roblox's LuaVM_Load to load the bytecode
    std::string bytecode_copy = bytecode;
    
    // Call the internal LuaVM_Load function
    // Signature: uintptr_t LuaVMLoad(int64_t L, std::string* bytecode, const char* chunkname, int env)
    auto result = Signatures::LuaVMLoad(
        reinterpret_cast<int64_t>(L),
        &bytecode_copy,
        "@Script",
        0
    );

    if (result != 0)
    {
        printf("[ERROR] LuaVM_Load failed with code: %llu\n", result);
        return false;
    }

    // Defer the execution using task.defer
    int defer_result = Signatures::TaskDefer(L);
    
    printf("[SUCCESS] Script executed successfully!\n");
    return true;
}

lua_State* script_execution_t::get_lua_state()
{
    if (globals::lua_state)
        return globals::lua_state;

    // Try to get lua state from Roblox
    // This requires finding ScriptContext and calling GetGlobalState
    
    uintptr_t data_model = *reinterpret_cast<uintptr_t*>(Offsets::FakeDataModelPointer);
    if (!data_model)
    {
        printf("[ERROR] DataModel not found!\n");
        return nullptr;
    }

    printf("[INFO] DataModel: 0x%llX\n", data_model);
    
    // You need to traverse to ScriptContext and get the lua_State
    // This is game-specific and may require additional offsets
    
    return globals::lua_state;
}

bool script_execution_t::initialize()
{
    printf("[INFO] Initializing script execution...\n");
    
    // Get the lua state
    lua_State* L = get_lua_state();
    if (!L)
    {
        printf("[WARNING] Could not get lua state during init, will retry later\n");
    }
    else
    {
        printf("[SUCCESS] Got lua state: 0x%p\n", L);
        globals::lua_state = L;
    }
    
    return true;
}
