#pragma once
#include <Windows.h>
#include <Psapi.h>
#include <vector>
#include <string>
#include <cstdint>
#include <optional>

namespace utils
{
    class pattern_scanner
    {
    public:
        // Scan for a pattern in a module
        static uintptr_t find_pattern(HMODULE module, const char* pattern, const char* mask)
        {
            MODULEINFO module_info;
            if (!GetModuleInformation(GetCurrentProcess(), module, &module_info, sizeof(module_info)))
                return 0;

            uintptr_t start = reinterpret_cast<uintptr_t>(module_info.lpBaseOfDll);
            uintptr_t end = start + module_info.SizeOfImage;

            return find_pattern(start, end, pattern, mask);
        }

        // Scan for a pattern in a memory range
        static uintptr_t find_pattern(uintptr_t start, uintptr_t end, const char* pattern, const char* mask)
        {
            size_t pattern_length = strlen(mask);

            for (uintptr_t current = start; current < end - pattern_length; current++)
            {
                bool found = true;

                for (size_t i = 0; i < pattern_length; i++)
                {
                    if (mask[i] != '?' && pattern[i] != *reinterpret_cast<char*>(current + i))
                    {
                        found = false;
                        break;
                    }
                }

                if (found)
                    return current;
            }

            return 0;
        }

        // Parse IDA-style pattern string (e.g., "48 8B 05 ? ? ? ? 48 85 C0")
        static uintptr_t find_pattern_ida(HMODULE module, const std::string& ida_pattern)
        {
            std::vector<uint8_t> pattern;
            std::string mask;

            std::string current;
            for (size_t i = 0; i <= ida_pattern.length(); i++)
            {
                if (i == ida_pattern.length() || ida_pattern[i] == ' ')
                {
                    if (!current.empty())
                    {
                        if (current == "?" || current == "??")
                        {
                            pattern.push_back(0);
                            mask += '?';
                        }
                        else
                        {
                            pattern.push_back(static_cast<uint8_t>(std::stoi(current, nullptr, 16)));
                            mask += 'x';
                        }
                        current.clear();
                    }
                }
                else
                {
                    current += ida_pattern[i];
                }
            }

            return find_pattern(module, reinterpret_cast<const char*>(pattern.data()), mask.c_str());
        }

        // Get address from a relative call/jmp instruction
        static uintptr_t resolve_relative(uintptr_t address, int offset = 1, int instruction_size = 5)
        {
            if (!address)
                return 0;

            int32_t relative = *reinterpret_cast<int32_t*>(address + offset);
            return address + instruction_size + relative;
        }

        // Follow a chain of pointers
        static uintptr_t follow_pointers(uintptr_t base, const std::vector<uintptr_t>& offsets)
        {
            uintptr_t current = base;

            for (size_t i = 0; i < offsets.size(); i++)
            {
                if (!current)
                    return 0;

                current = *reinterpret_cast<uintptr_t*>(current);
                if (!current)
                    return 0;

                current += offsets[i];
            }

            return current;
        }
    };

    // String utilities
    class string_utils
    {
    public:
        static std::string to_lower(std::string str)
        {
            for (char& c : str)
                c = static_cast<char>(tolower(c));
            return str;
        }

        static std::wstring to_wstring(const std::string& str)
        {
            return std::wstring(str.begin(), str.end());
        }

        static std::string to_string(const std::wstring& wstr)
        {
            return std::string(wstr.begin(), wstr.end());
        }
    };

    // Memory utilities
    class memory_utils
    {
    public:
        template<typename T>
        static T read(uintptr_t address)
        {
            if (!address)
                return T{};

            __try
            {
                return *reinterpret_cast<T*>(address);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return T{};
            }
        }

        template<typename T>
        static bool write(uintptr_t address, const T& value)
        {
            if (!address)
                return false;

            __try
            {
                DWORD old_protect;
                VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), PAGE_EXECUTE_READWRITE, &old_protect);
                *reinterpret_cast<T*>(address) = value;
                VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), old_protect, &old_protect);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        static bool is_valid_pointer(uintptr_t address)
        {
            if (!address)
                return false;

            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)))
                return false;

            return (mbi.State == MEM_COMMIT) &&
                   !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                   (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));
        }
    };
}
