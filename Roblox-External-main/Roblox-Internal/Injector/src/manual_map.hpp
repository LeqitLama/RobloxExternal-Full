#pragma once
#include "nt_defs.hpp"

class ManualMapper {
private:
    HANDLE m_ProcessHandle = nullptr;

    // PE structures
    typedef struct _IMAGE_RELOC {
        WORD offset : 12;
        WORD type : 4;
    } IMAGE_RELOC, * PIMAGE_RELOC;

    // Shellcode for calling DllMain
    bool CallDllMain(void* remoteBase, PIMAGE_NT_HEADERS ntHeaders);
    
    // Process relocations
    bool ProcessRelocations(PBYTE localImage, void* remoteBase, PIMAGE_NT_HEADERS ntHeaders);
    
    // Resolve imports
    bool ResolveImports(PBYTE localImage, PIMAGE_NT_HEADERS ntHeaders);
    
    // Get remote module handle - FIXED to enumerate remote process modules
    HMODULE GetRemoteModuleHandle(const char* moduleName);
    
    // Get remote proc address by parsing remote export table
    FARPROC GetRemoteProcAddress(HMODULE remoteModule, const char* procName);

public:
    ManualMapper(HANDLE processHandle) : m_ProcessHandle(processHandle) {}
    
    bool Map(const std::string& dllPath);
    bool MapFromMemory(const std::vector<BYTE>& dllData);
};

// Implementation
inline bool ManualMapper::Map(const std::string& dllPath) {
    // Read DLL file
    std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "[!] Failed to open DLL file\n";
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<BYTE> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cout << "[!] Failed to read DLL file\n";
        return false;
    }

    return MapFromMemory(buffer);
}

inline bool ManualMapper::MapFromMemory(const std::vector<BYTE>& dllData) {
    // Get DOS header
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)dllData.data();
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        std::cout << "[!] Invalid DOS signature\n";
        return false;
    }

    // Get NT headers
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(dllData.data() + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        std::cout << "[!] Invalid NT signature\n";
        return false;
    }

    // Allocate memory in target process
    SIZE_T imageSize = ntHeaders->OptionalHeader.SizeOfImage;
    PVOID remoteBase = nullptr;
    
    NTSTATUS status = g_NtFuncs.NtAllocateVirtualMemory(
        m_ProcessHandle,
        &remoteBase,
        0,
        &imageSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );

    if (!NT_SUCCESS(status) || !remoteBase) {
        std::cout << "[!] Failed to allocate remote memory: 0x" << std::hex << status << "\n";
        return false;
    }

    std::cout << "[+] Allocated remote memory at: 0x" << std::hex << remoteBase << "\n";

    // Create local copy of image
    std::vector<BYTE> localImage(ntHeaders->OptionalHeader.SizeOfImage);
    
    // Copy headers
    memcpy(localImage.data(), dllData.data(), ntHeaders->OptionalHeader.SizeOfHeaders);

    // Copy sections
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++, section++) {
        if (section->SizeOfRawData > 0) {
            memcpy(
                localImage.data() + section->VirtualAddress,
                dllData.data() + section->PointerToRawData,
                section->SizeOfRawData
            );
        }
    }

    // Get local NT headers pointer
    PIMAGE_NT_HEADERS localNtHeaders = (PIMAGE_NT_HEADERS)(localImage.data() + dosHeader->e_lfanew);

    // Process relocations
    if (!ProcessRelocations(localImage.data(), remoteBase, localNtHeaders)) {
        std::cout << "[!] Failed to process relocations\n";
        VirtualFreeEx(m_ProcessHandle, remoteBase, 0, MEM_RELEASE);
        return false;
    }

    // Resolve imports
    if (!ResolveImports(localImage.data(), localNtHeaders)) {
        std::cout << "[!] Failed to resolve imports\n";
        VirtualFreeEx(m_ProcessHandle, remoteBase, 0, MEM_RELEASE);
        return false;
    }

    // Write image to remote process
    SIZE_T written = 0;
    status = g_NtFuncs.NtWriteVirtualMemory(
        m_ProcessHandle,
        remoteBase,
        localImage.data(),
        localImage.size(),
        &written
    );

    if (!NT_SUCCESS(status)) {
        std::cout << "[!] Failed to write image: 0x" << std::hex << status << "\n";
        VirtualFreeEx(m_ProcessHandle, remoteBase, 0, MEM_RELEASE);
        return false;
    }

    std::cout << "[+] Written " << std::hex << written << " bytes to remote process\n";

    // Call DllMain
    if (!CallDllMain(remoteBase, localNtHeaders)) {
        std::cout << "[!] Failed to call DllMain\n";
        return false;
    }

    std::cout << "[+] Manual mapping completed successfully!\n";
    return true;
}

inline bool ManualMapper::ProcessRelocations(PBYTE localImage, void* remoteBase, PIMAGE_NT_HEADERS ntHeaders) {
    DWORD64 delta = (DWORD64)remoteBase - ntHeaders->OptionalHeader.ImageBase;
    if (delta == 0) return true; // No relocation needed

    DWORD relocRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
    if (relocRVA == 0) return true; // No relocations

    PIMAGE_BASE_RELOCATION reloc = (PIMAGE_BASE_RELOCATION)(localImage + relocRVA);
    DWORD relocSize = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
    PBYTE relocEnd = (PBYTE)reloc + relocSize;

    while ((PBYTE)reloc < relocEnd && reloc->SizeOfBlock > 0) {
        DWORD count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        PWORD list = (PWORD)((PBYTE)reloc + sizeof(IMAGE_BASE_RELOCATION));

        for (DWORD i = 0; i < count; i++) {
            int type = list[i] >> 12;
            int offset = list[i] & 0xFFF;

            if (type == IMAGE_REL_BASED_DIR64) {
                PDWORD64 patchAddr = (PDWORD64)(localImage + reloc->VirtualAddress + offset);
                *patchAddr += delta;
            }
            else if (type == IMAGE_REL_BASED_HIGHLOW) {
                PDWORD patchAddr = (PDWORD)(localImage + reloc->VirtualAddress + offset);
                *patchAddr += (DWORD)delta;
            }
        }

        reloc = (PIMAGE_BASE_RELOCATION)((PBYTE)reloc + reloc->SizeOfBlock);
    }

    return true;
}

inline bool ManualMapper::ResolveImports(PBYTE localImage, PIMAGE_NT_HEADERS ntHeaders) {
    DWORD importRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (importRVA == 0) return true; // No imports

    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)(localImage + importRVA);

    while (importDesc->Name) {
        char* moduleName = (char*)(localImage + importDesc->Name);
        
        // Get or load the module in target process
        HMODULE remoteModule = GetRemoteModuleHandle(moduleName);
        if (!remoteModule) {
            std::cout << "[!] Failed to find module: " << moduleName << "\n";
            return false;
        }

        std::cout << "[+] Found remote module: " << moduleName << " at 0x" << std::hex << remoteModule << "\n";

        PIMAGE_THUNK_DATA originalThunk = (PIMAGE_THUNK_DATA)(localImage + importDesc->OriginalFirstThunk);
        PIMAGE_THUNK_DATA firstThunk = (PIMAGE_THUNK_DATA)(localImage + importDesc->FirstThunk);

        while (originalThunk->u1.AddressOfData) {
            FARPROC funcAddr = nullptr;

            if (IMAGE_SNAP_BY_ORDINAL(originalThunk->u1.Ordinal)) {
                funcAddr = GetRemoteProcAddress(remoteModule, (LPCSTR)IMAGE_ORDINAL(originalThunk->u1.Ordinal));
            }
            else {
                PIMAGE_IMPORT_BY_NAME importByName = (PIMAGE_IMPORT_BY_NAME)(localImage + originalThunk->u1.AddressOfData);
                funcAddr = GetRemoteProcAddress(remoteModule, importByName->Name);
            }

            if (!funcAddr) {
                if (!IMAGE_SNAP_BY_ORDINAL(originalThunk->u1.Ordinal)) {
                    PIMAGE_IMPORT_BY_NAME importByName = (PIMAGE_IMPORT_BY_NAME)(localImage + originalThunk->u1.AddressOfData);
                    std::cout << "[!] Failed to resolve: " << importByName->Name << "\n";
                }
                return false;
            }

            firstThunk->u1.Function = (ULONGLONG)funcAddr;

            originalThunk++;
            firstThunk++;
        }

        importDesc++;
    }

    return true;
}

// FIXED: Properly enumerate modules in the REMOTE process
inline HMODULE ManualMapper::GetRemoteModuleHandle(const char* moduleName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetProcessId(m_ProcessHandle));
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::cout << "[!] Failed to create snapshot: " << GetLastError() << "\n";
        return nullptr;
    }

    MODULEENTRY32W me32;
    me32.dwSize = sizeof(me32);

    // Convert module name to wide string for comparison
    std::wstring wideModuleName;
    int len = MultiByteToWideChar(CP_ACP, 0, moduleName, -1, nullptr, 0);
    wideModuleName.resize(len);
    MultiByteToWideChar(CP_ACP, 0, moduleName, -1, &wideModuleName[0], len);

    HMODULE result = nullptr;

    if (Module32FirstW(snapshot, &me32)) {
        do {
            // Case-insensitive comparison
            if (_wcsicmp(me32.szModule, wideModuleName.c_str()) == 0) {
                result = me32.hModule;
                break;
            }
        } while (Module32NextW(snapshot, &me32));
    }

    CloseHandle(snapshot);
    return result;
}

// Get function address from remote module's export table
inline FARPROC ManualMapper::GetRemoteProcAddress(HMODULE remoteModule, const char* procName) {
    // Read the remote module's headers
    BYTE headerBuffer[0x1000];
    SIZE_T bytesRead = 0;
    
    if (!ReadProcessMemory(m_ProcessHandle, remoteModule, headerBuffer, sizeof(headerBuffer), &bytesRead)) {
        return nullptr;
    }

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)headerBuffer;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return nullptr;
    }

    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(headerBuffer + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return nullptr;
    }

    DWORD exportRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    DWORD exportSize = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;

    if (exportRVA == 0) {
        return nullptr;
    }

    // Read export directory
    std::vector<BYTE> exportBuffer(exportSize);
    if (!ReadProcessMemory(m_ProcessHandle, (PBYTE)remoteModule + exportRVA, exportBuffer.data(), exportSize, &bytesRead)) {
        return nullptr;
    }

    PIMAGE_EXPORT_DIRECTORY exportDir = (PIMAGE_EXPORT_DIRECTORY)exportBuffer.data();

    // Calculate offsets within our buffer
    DWORD* functions = (DWORD*)(exportBuffer.data() + (exportDir->AddressOfFunctions - exportRVA));
    DWORD* names = (DWORD*)(exportBuffer.data() + (exportDir->AddressOfNames - exportRVA));
    WORD* ordinals = (WORD*)(exportBuffer.data() + (exportDir->AddressOfNameOrdinals - exportRVA));

    // Check if searching by ordinal
    if ((ULONG_PTR)procName <= 0xFFFF) {
        WORD ordinal = (WORD)((ULONG_PTR)procName - exportDir->Base);
        if (ordinal < exportDir->NumberOfFunctions) {
            DWORD funcRVA = functions[ordinal];
            return (FARPROC)((PBYTE)remoteModule + funcRVA);
        }
        return nullptr;
    }

    // Search by name
    for (DWORD i = 0; i < exportDir->NumberOfNames; i++) {
        char* funcName = (char*)(exportBuffer.data() + (names[i] - exportRVA));
        if (strcmp(funcName, procName) == 0) {
            WORD ordinal = ordinals[i];
            DWORD funcRVA = functions[ordinal];
            
            // Check for forwarded export
            if (funcRVA >= exportRVA && funcRVA < exportRVA + exportSize) {
                // This is a forwarded export, we need to resolve it
                // For now, skip forwarded exports
                return nullptr;
            }
            
            return (FARPROC)((PBYTE)remoteModule + funcRVA);
        }
    }

    return nullptr;
}

inline bool ManualMapper::CallDllMain(void* remoteBase, PIMAGE_NT_HEADERS ntHeaders) {
    // Calculate DllMain address
    DWORD64 entryPoint = (DWORD64)remoteBase + ntHeaders->OptionalHeader.AddressOfEntryPoint;
    
    std::cout << "[+] Calling DllMain at: 0x" << std::hex << entryPoint << "\n";

    // Shellcode to call DllMain(hModule, DLL_PROCESS_ATTACH, 0)
    // DllMain signature: BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
    BYTE shellcode[] = {
        0x48, 0x83, 0xEC, 0x28,                                         // sub rsp, 0x28 (shadow space)
        0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // mov rcx, <hModule>
        0xBA, 0x01, 0x00, 0x00, 0x00,                                   // mov edx, DLL_PROCESS_ATTACH (1)
        0x4D, 0x31, 0xC0,                                               // xor r8, r8 (lpvReserved = 0)
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // mov rax, <DllMain address>
        0xFF, 0xD0,                                                     // call rax
        0x48, 0x83, 0xC4, 0x28,                                         // add rsp, 0x28
        0xC3                                                            // ret
    };

    // Fill in addresses
    *(PVOID*)&shellcode[6] = remoteBase;        // hModule
    *(PVOID*)&shellcode[24] = (PVOID)entryPoint; // DllMain

    // Allocate and write shellcode
    SIZE_T codeSize = sizeof(shellcode);
    PVOID codeBuffer = nullptr;
    
    NTSTATUS status = g_NtFuncs.NtAllocateVirtualMemory(
        m_ProcessHandle,
        &codeBuffer,
        0,
        &codeSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );

    if (!NT_SUCCESS(status) || !codeBuffer) {
        std::cout << "[!] Failed to allocate shellcode memory\n";
        return false;
    }

    SIZE_T written = 0;
    g_NtFuncs.NtWriteVirtualMemory(m_ProcessHandle, codeBuffer, shellcode, sizeof(shellcode), &written);

    // Create remote thread to execute shellcode
    HANDLE hThread = nullptr;
    status = g_NtFuncs.NtCreateThreadEx(
        &hThread,
        THREAD_ALL_ACCESS,
        nullptr,
        m_ProcessHandle,
        codeBuffer,
        nullptr,
        0x00000004,  // THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER
        0, 0, 0,
        nullptr
    );

    if (!NT_SUCCESS(status) || !hThread) {
        std::cout << "[!] Failed to create remote thread: 0x" << std::hex << status << "\n";
        VirtualFreeEx(m_ProcessHandle, codeBuffer, 0, MEM_RELEASE);
        return false;
    }

    std::cout << "[+] Waiting for DllMain to complete...\n";
    
    DWORD waitResult = WaitForSingleObject(hThread, 10000);
    if (waitResult == WAIT_TIMEOUT) {
        std::cout << "[!] DllMain timed out\n";
    }
    
    CloseHandle(hThread);
    VirtualFreeEx(m_ProcessHandle, codeBuffer, 0, MEM_RELEASE);
    
    return true;
}
