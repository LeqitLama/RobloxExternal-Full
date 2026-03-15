#include "memory.h"
#include "sdk.h"

Memory::Memory() : process_handle(nullptr), process_id(0), base_address(0) {}

Memory::~Memory()
{
    if (process_handle) CloseHandle(process_handle);
}

DWORD Memory::GetProcessId(const std::string& process_name)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &entry))
    {
        do
        {
#ifdef UNICODE
            std::wstring wProcessName(entry.szExeFile);
            std::string currentProcessName;
            currentProcessName.reserve(wProcessName.length());
            for(wchar_t c : wProcessName) currentProcessName.push_back((char)c);
#else
            std::string currentProcessName(entry.szExeFile);
#endif
            
            if (process_name == currentProcessName)
            {
                CloseHandle(snapshot);
                return entry.th32ProcessID;
            }
        } while (Process32Next(snapshot, &entry));
    }
    
    CloseHandle(snapshot);
    return 0;
}

uintptr_t Memory::GetModuleBase(const std::string& module_name)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, process_id);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    
    MODULEENTRY32 entry;
    entry.dwSize = sizeof(MODULEENTRY32);
    
    if (Module32First(snapshot, &entry))
    {
        do
        {
#ifdef UNICODE
            std::wstring wModuleName(entry.szModule);
            std::string currentModuleName;
            currentModuleName.reserve(wModuleName.length());
            for(wchar_t c : wModuleName) currentModuleName.push_back((char)c);
#else
            std::string currentModuleName(entry.szModule);
#endif
            
            if (module_name == currentModuleName)
            {
                CloseHandle(snapshot);
                return (uintptr_t)entry.modBaseAddr;
            }
        } while (Module32Next(snapshot, &entry));
    }
    
    CloseHandle(snapshot);
    return 0;
}

bool Memory::AttachToProcess(const std::string& process_name)
{
    process_id = GetProcessId(process_name);
    if (process_id == 0) return false;
    
    // Request read AND write access for silent aim
    process_handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, process_id);
    if (!process_handle) return false;
    
    base_address = GetModuleBase(process_name);
    if (base_address == 0) return false;
    
    return true;
}

template<typename T>
T Memory::ReadMemory(uintptr_t address)
{
    T value = {};
    ReadProcessMemory(process_handle, (LPCVOID)address, &value, sizeof(T), nullptr);
    return value;
}

template<typename T>
bool Memory::WriteMemory(uintptr_t address, const T& value)
{
    SIZE_T bytes_written = 0;
    return WriteProcessMemory(process_handle, (LPVOID)address, &value, sizeof(T), &bytes_written) && bytes_written == sizeof(T);
}

std::string Memory::ReadString(uintptr_t address, size_t max_length)
{
    char buffer[256] = {};
    ReadProcessMemory(process_handle, (LPCVOID)address, buffer, min(max_length, sizeof(buffer) - 1), nullptr);
    return std::string(buffer);
}

uintptr_t Memory::GetBaseAddress() const 
{
    return base_address; 
}

bool Memory::IsValid() const
{
    return process_handle != nullptr; 
}

// Template instantiations
template int Memory::ReadMemory<int>(uintptr_t address);
template char Memory::ReadMemory<char>(uintptr_t address);
template float Memory::ReadMemory<float>(uintptr_t address);
template uintptr_t Memory::ReadMemory<uintptr_t>(uintptr_t address);
template struct Vector3 Memory::ReadMemory<struct Vector3>(uintptr_t address);
template struct Vector2 Memory::ReadMemory<struct Vector2>(uintptr_t address);
template struct Matrix4x4 Memory::ReadMemory<struct Matrix4x4>(uintptr_t address);
template uint8_t Memory::ReadMemory<uint8_t>(uintptr_t address);

template bool Memory::WriteMemory<float>(uintptr_t address, const float& value);
template bool Memory::WriteMemory<int>(uintptr_t address, const int& value);
template bool Memory::WriteMemory<Vector3>(uintptr_t address, const Vector3& value);
template bool Memory::WriteMemory<Vector2>(uintptr_t address, const Vector2& value);
template bool Memory::WriteMemory<uint8_t>(uintptr_t address, const uint8_t& value);
template bool Memory::WriteMemory<uintptr_t>(uintptr_t address, const uintptr_t& value);