# Roblox Internal Injector

A multi-method DLL injector designed with anti-cheat bypass techniques in mind.

## Features

- **Multiple Injection Methods**:
  1. **Manual Map** - Maps DLL without LoadLibrary, avoiding common hooks
  2. **NtCreateThreadEx** - Uses NT syscalls with hide-from-debugger flag
  3. **LdrLoadDll** - Direct ntdll loader call
  4. **Thread Hijack** - Hijacks existing thread context
  5. **QueueUserAPC** - APC-based injection
  6. **LoadLibrary** - Standard injection (easily detected)

- **Automatic detection** of Roblox process
- **Debug privilege** escalation
- **Run as Administrator** required

## Usage

1. Place `Injector.exe` and `RobloxInternal.dll` in the same folder
2. Run `Injector.exe` as Administrator
3. Start Roblox (or if already running, it will connect)
4. Select an injection method
5. Wait for injection result

### Command Line

```bash
Injector.exe                          # Auto-find DLL in same folder
Injector.exe C:\path\to\your.dll      # Specify DLL path
```

## Injection Methods Explained

### 1. Manual Map (Recommended)
- Manually maps PE sections into target process
- Resolves imports and processes relocations
- Calls DllMain via NtCreateThreadEx with hidden flag
- **Pros**: Avoids LoadLibrary hooks, no module in PEB
- **Cons**: Complex, may fail if relocations fail

### 2. NtCreateThreadEx
- Uses native API instead of CreateRemoteThread
- Applies `THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER`
- **Pros**: Bypasses some CreateRemoteThread hooks
- **Cons**: Still uses LoadLibrary, module visible in PEB

### 3. LdrLoadDll
- Calls ntdll's internal loader directly via shellcode
- **Pros**: Bypasses kernel32 LoadLibrary hooks
- **Cons**: Module still visible in PEB

### 4. Thread Hijack
- Suspends existing thread
- Modifies RIP to execute shellcode
- Resumes thread
- **Pros**: No new thread creation
- **Cons**: May cause instability

### 5. QueueUserAPC
- Queues APC to all process threads
- Executes when thread becomes alertable
- **Pros**: No direct thread creation
- **Cons**: Waits for alertable state

### 6. LoadLibrary (Detected)
- Standard CreateRemoteThread + LoadLibrary
- **Pros**: Simple and reliable
- **Cons**: Easily detected by anti-cheat

## ⚠️ Byfron Notice

**Byfron (Hyperion) actively monitors and blocks DLL injections.**

Even with these techniques, injection may fail because:
- Kernel-mode driver monitoring
- Syscall hooking
- Process handle stripping
- Memory protection
- Behavior detection

For educational purposes only. Use at your own risk.

## Building

1. Open `Injector.sln` in Visual Studio 2022
2. Build in Release|x64 configuration
3. Output: `build\Release\Injector.exe`

## Files

```
Injector/
├── src/
│   ├── main.cpp           # Entry point with UI
│   ├── injector.hpp       # Main injector class
│   ├── manual_map.hpp     # Manual mapping implementation
│   └── nt_defs.hpp        # NT function definitions
├── Injector.sln
├── Injector.vcxproj
└── app.manifest           # UAC admin requirement
```

## Output Location

Both `Injector.exe` and `RobloxInternal.dll` build to:
```
Roblox-Internal/build/Release/
```
