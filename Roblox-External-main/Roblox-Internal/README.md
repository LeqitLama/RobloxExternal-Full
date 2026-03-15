# Roblox Internal Executor Base

An internal Roblox Lua executor base inspired by [UNintpointer/robloxinternal](https://github.com/UNintpointer/robloxinternal).

## Features

- **Script Execution**: Compile and execute Lua scripts using Luau
- **Script Scheduler**: Thread-safe queue for script execution
- **Script Environment**: Custom Lua library registration system
- **Pointer Encryption**: Handle Roblox's encrypted pointers (XOR/ADD/SUB)
- **Named Pipe Communication**: IPC with external UI tools
- **DataModel Navigation**: Access Roblox services and instances

## Project Structure

```
Roblox-Internal/
├── src/
│   ├── core/
│   │   ├── globals.hpp         # Global state variables
│   │   ├── configuration.hpp   # Configuration constants
│   │   ├── offsets.hpp         # Roblox memory offsets
│   │   ├── entry_point.hpp     # DLL entry declarations
│   │   └── entry_point.cpp     # DllMain and initialization
│   │
│   ├── engine/
│   │   ├── pointer_encryption/ # Encrypted pointer handling
│   │   ├── data_model/         # DataModel access
│   │   └── graphics/           # RenderView/VisualEngine
│   │
│   ├── script_execution/       # Luau compilation & execution
│   ├── script_environment/     # Custom Lua libraries
│   ├── script_scheduler/       # Script queue management
│   └── communication/          # Named pipe IPC
│
├── dependencies/               # External libraries (add manually)
│   ├── luau/                   # Roblox Luau compiler
│   ├── zstd/                   # Compression library
│   └── xxhash/                 # Hash library
│
├── RobloxInternal.sln          # Visual Studio solution
└── RobloxInternal.vcxproj      # Visual Studio project
```

## Dependencies

You need to add the following dependencies to the `dependencies/` folder:

1. **Luau** - Roblox's Lua implementation
   - https://github.com/Roblox/luau
   - Compile and place headers in `dependencies/luau/include`
   - Place static libraries in `dependencies/luau/lib`

2. **zstd** - Compression library
   - https://github.com/facebook/zstd
   - Place headers in `dependencies/zstd/include`
   - Place static libraries in `dependencies/zstd/lib`

3. **xxHash** - Fast hash library
   - https://github.com/Cyan4973/xxHash
   - Place `xxhash.h` and `xxhash.c` in `dependencies/xxhash`

## Building

1. Open `RobloxInternal.sln` in Visual Studio 2022
2. Add dependencies as described above
3. Build in Release|x64 configuration
4. Output DLL will be in `build/Release/`

## Updating Offsets

Offsets need to be updated after each Roblox update. Edit `src/core/offsets.hpp`:

```cpp
namespace offsets
{
    // Update these values after each Roblox update
    constexpr uintptr_t render_view_to_data_model = 0x118;
    constexpr uintptr_t data_model_to_ptr = 0x198;
    // ... more offsets
}
```

## Usage

### Injection
Use a DLL injector to inject the compiled DLL into Roblox.

### Communication
Send scripts via named pipe: `\\.\pipe\RobloxInternalPipe`

Example (C++):
```cpp
HANDLE pipe = CreateFileA(
    "\\\\.\\pipe\\RobloxInternalPipe",
    GENERIC_WRITE,
    0, nullptr, OPEN_EXISTING, 0, nullptr
);

const char* script = "print('Hello World!')";
DWORD written;
WriteFile(pipe, script, strlen(script), &written, nullptr);
CloseHandle(pipe);
```

## Custom Libraries

Add custom Lua functions by creating a library class:

```cpp
class my_library : public library_t
{
public:
    void initialize(lua_State* L) override
    {
        // Register your functions here
    }
    
    const char* get_name() const override
    {
        return "MyLibrary";
    }
};
```

Then register it in `script_environment.cpp`:
```cpp
libraries.push_back(std::make_unique<my_library>());
```

## ⚠️ Disclaimer

This project is for **educational purposes only**. Using this software may violate Roblox's Terms of Service. The authors are not responsible for any consequences of using this software.

## Credits

- Inspired by [UNintpointer/robloxinternal](https://github.com/UNintpointer/robloxinternal)
- Uses [Roblox/luau](https://github.com/Roblox/luau) for script compilation
