#pragma once
#include "nt_defs.hpp"
#include "manual_map.hpp"

class Injector {
private:
    DWORD m_ProcessId = 0;
    HANDLE m_ProcessHandle = nullptr;
    std::string m_DllPath;

    // Find Roblox process
    DWORD FindRobloxProcess();
    
    // Get process handle using NT functions
    HANDLE OpenProcessNt(DWORD pid, ACCESS_MASK access);
    
    // Enable debug privilege
    bool EnableDebugPrivilege();
    
    // Injection methods
    bool InjectLoadLibrary();
    bool InjectNtCreateThreadEx();
    bool InjectManualMap();
    bool InjectThreadHijack();
    bool InjectQueueUserAPC();
    bool InjectLdrLoadDll();

    // Helper functions
    HANDLE FindAlertableThread();
    std::vector<DWORD> GetThreadIds();

public:
    Injector() = default;
    ~Injector();

    bool Initialize(const std::string& dllPath);
    bool WaitForRoblox(int timeoutSeconds = 60);
    bool Inject(InjectionMethod method = InjectionMethod::NtCreateThreadEx);
    bool TryAllMethods();
    
    DWORD GetProcessId() const { return m_ProcessId; }
};

// Implementation
inline Injector::~Injector() {
    if (m_ProcessHandle) {
        CloseHandle(m_ProcessHandle);
    }
}

inline bool Injector::EnableDebugPrivilege() {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    LUID luid;
    if (!LookupPrivilegeValueA(nullptr, "SeDebugPrivilege", &luid)) {
        CloseHandle(hToken);
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    bool result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    CloseHandle(hToken);
    
    return result && GetLastError() == ERROR_SUCCESS;
}

inline DWORD Injector::FindRobloxProcess() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);

    if (Process32FirstW(snapshot, &pe32)) {
        do {
            if (wcscmp(pe32.szExeFile, L"RobloxPlayerBeta.exe") == 0 ||
                wcscmp(pe32.szExeFile, L"RobloxPlayer.exe") == 0 ||
                wcscmp(pe32.szExeFile, L"Windows10Universal.exe") == 0) {
                CloseHandle(snapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &pe32));
    }

    CloseHandle(snapshot);
    return 0;
}

inline HANDLE Injector::OpenProcessNt(DWORD pid, ACCESS_MASK access) {
    if (!g_NtFuncs.NtOpenProcess) {
        return OpenProcess(access, FALSE, pid);
    }

    HANDLE handle = nullptr;
    OBJECT_ATTRIBUTES oa = { sizeof(oa), nullptr, nullptr, 0, nullptr, nullptr };
    CLIENT_ID cid = { (HANDLE)(ULONG_PTR)pid, nullptr };

    NTSTATUS status = g_NtFuncs.NtOpenProcess(&handle, access, &oa, &cid);
    
    if (!NT_SUCCESS(status)) {
        // Fallback to normal OpenProcess
        return OpenProcess(access, FALSE, pid);
    }

    return handle;
}

inline bool Injector::Initialize(const std::string& dllPath) {
    m_DllPath = dllPath;

    // Check if DLL exists
    if (GetFileAttributesA(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::cout << "[!] DLL file not found: " << dllPath << "\n";
        return false;
    }

    // Initialize NT functions
    if (!g_NtFuncs.Initialize()) {
        std::cout << "[!] Failed to initialize NT functions\n";
        return false;
    }

    // Enable debug privilege
    if (EnableDebugPrivilege()) {
        std::cout << "[+] Debug privilege enabled\n";
    }

    return true;
}

inline bool Injector::WaitForRoblox(int timeoutSeconds) {
    std::cout << "[*] Waiting for Roblox...\n";
    
    for (int i = 0; i < timeoutSeconds; i++) {
        m_ProcessId = FindRobloxProcess();
        if (m_ProcessId) {
            std::cout << "[+] Found Roblox! PID: " << m_ProcessId << "\n";
            
            // Wait a bit for the process to fully initialize
            Sleep(2000);
            
            // Open process handle
            m_ProcessHandle = OpenProcessNt(m_ProcessId, PROCESS_ALL_ACCESS);
            if (!m_ProcessHandle) {
                std::cout << "[!] Failed to open process\n";
                return false;
            }
            
            std::cout << "[+] Process handle acquired\n";
            return true;
        }
        Sleep(1000);
        std::cout << ".";
    }

    std::cout << "\n[!] Timeout waiting for Roblox\n";
    return false;
}

inline std::vector<DWORD> Injector::GetThreadIds() {
    std::vector<DWORD> threads;
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return threads;

    THREADENTRY32 te32;
    te32.dwSize = sizeof(te32);

    if (Thread32First(snapshot, &te32)) {
        do {
            if (te32.th32OwnerProcessID == m_ProcessId) {
                threads.push_back(te32.th32ThreadID);
            }
        } while (Thread32Next(snapshot, &te32));
    }

    CloseHandle(snapshot);
    return threads;
}

inline HANDLE Injector::FindAlertableThread() {
    auto threads = GetThreadIds();
    for (DWORD tid : threads) {
        HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
        if (hThread) {
            return hThread;
        }
    }
    return nullptr;
}

inline bool Injector::InjectLoadLibrary() {
    std::cout << "[*] Trying LoadLibrary injection...\n";

    // Allocate memory for DLL path
    size_t pathSize = m_DllPath.size() + 1;
    LPVOID remotePath = VirtualAllocEx(m_ProcessHandle, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        std::cout << "[!] Failed to allocate remote memory\n";
        return false;
    }

    // Write DLL path
    if (!WriteProcessMemory(m_ProcessHandle, remotePath, m_DllPath.c_str(), pathSize, nullptr)) {
        std::cout << "[!] Failed to write DLL path\n";
        VirtualFreeEx(m_ProcessHandle, remotePath, 0, MEM_RELEASE);
        return false;
    }

    // Get LoadLibraryA address
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC loadLibrary = GetProcAddress(kernel32, "LoadLibraryA");

    // Create remote thread
    HANDLE hThread = CreateRemoteThread(
        m_ProcessHandle,
        nullptr,
        0,
        (LPTHREAD_START_ROUTINE)loadLibrary,
        remotePath,
        0,
        nullptr
    );

    if (!hThread) {
        std::cout << "[!] CreateRemoteThread failed: " << GetLastError() << "\n";
        VirtualFreeEx(m_ProcessHandle, remotePath, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread, 5000);
    
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    VirtualFreeEx(m_ProcessHandle, remotePath, 0, MEM_RELEASE);

    if (exitCode == 0) {
        std::cout << "[!] LoadLibrary returned NULL\n";
        return false;
    }

    std::cout << "[+] LoadLibrary injection successful!\n";
    return true;
}

inline bool Injector::InjectNtCreateThreadEx() {
    std::cout << "[*] Trying NtCreateThreadEx injection...\n";

    // Allocate memory for DLL path
    size_t pathSize = m_DllPath.size() + 1;
    PVOID remotePath = nullptr;
    SIZE_T regionSize = pathSize;

    NTSTATUS status = g_NtFuncs.NtAllocateVirtualMemory(
        m_ProcessHandle,
        &remotePath,
        0,
        &regionSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (!NT_SUCCESS(status) || !remotePath) {
        std::cout << "[!] NtAllocateVirtualMemory failed: 0x" << std::hex << status << "\n";
        return false;
    }

    // Write DLL path
    SIZE_T written = 0;
    status = g_NtFuncs.NtWriteVirtualMemory(
        m_ProcessHandle,
        remotePath,
        (PVOID)m_DllPath.c_str(),
        pathSize,
        &written
    );

    if (!NT_SUCCESS(status)) {
        std::cout << "[!] NtWriteVirtualMemory failed: 0x" << std::hex << status << "\n";
        VirtualFreeEx(m_ProcessHandle, remotePath, 0, MEM_RELEASE);
        return false;
    }

    // Get LoadLibraryA address
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC loadLibrary = GetProcAddress(kernel32, "LoadLibraryA");

    // Create remote thread using NtCreateThreadEx
    HANDLE hThread = nullptr;
    status = g_NtFuncs.NtCreateThreadEx(
        &hThread,
        THREAD_ALL_ACCESS,
        nullptr,
        m_ProcessHandle,
        loadLibrary,
        remotePath,
        0x00000004,  // THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER
        0,
        0,
        0,
        nullptr
    );

    if (!NT_SUCCESS(status) || !hThread) {
        std::cout << "[!] NtCreateThreadEx failed: 0x" << std::hex << status << "\n";
        VirtualFreeEx(m_ProcessHandle, remotePath, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);

    VirtualFreeEx(m_ProcessHandle, remotePath, 0, MEM_RELEASE);

    std::cout << "[+] NtCreateThreadEx injection successful!\n";
    return true;
}

inline bool Injector::InjectManualMap() {
    std::cout << "[*] Trying Manual Map injection...\n";
    
    ManualMapper mapper(m_ProcessHandle);
    return mapper.Map(m_DllPath);
}

inline bool Injector::InjectThreadHijack() {
    std::cout << "[*] Trying Thread Hijack injection...\n";

    // Get a thread to hijack
    auto threads = GetThreadIds();
    if (threads.empty()) {
        std::cout << "[!] No threads found\n";
        return false;
    }

    HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, threads[0]);
    if (!hThread) {
        std::cout << "[!] Failed to open thread\n";
        return false;
    }

    // Suspend the thread
    if (SuspendThread(hThread) == (DWORD)-1) {
        std::cout << "[!] Failed to suspend thread\n";
        CloseHandle(hThread);
        return false;
    }

    // Get thread context
    CONTEXT ctx;
    ctx.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(hThread, &ctx)) {
        std::cout << "[!] Failed to get thread context\n";
        ResumeThread(hThread);
        CloseHandle(hThread);
        return false;
    }

    // Allocate memory for shellcode and path
    size_t pathSize = m_DllPath.size() + 1;
    
    // Shellcode to call LoadLibraryA and return to original RIP
    BYTE shellcode[] = {
        0x48, 0x83, 0xEC, 0x28,                         // sub rsp, 0x28
        0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rcx, <path address>
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, <LoadLibraryA>
        0xFF, 0xD0,                                     // call rax
        0x48, 0x83, 0xC4, 0x28,                         // add rsp, 0x28
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, <original RIP>
        0xFF, 0xE0                                      // jmp rax
    };

    SIZE_T totalSize = sizeof(shellcode) + pathSize;
    PVOID remoteBuffer = VirtualAllocEx(m_ProcessHandle, nullptr, totalSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteBuffer) {
        std::cout << "[!] Failed to allocate remote memory\n";
        ResumeThread(hThread);
        CloseHandle(hThread);
        return false;
    }

    // Fill in shellcode addresses
    PVOID pathAddress = (PBYTE)remoteBuffer + sizeof(shellcode);
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC loadLibrary = GetProcAddress(kernel32, "LoadLibraryA");

    *(PVOID*)&shellcode[6] = pathAddress;
    *(PVOID*)&shellcode[16] = loadLibrary;
    *(PVOID*)&shellcode[32] = (PVOID)ctx.Rip;

    // Write shellcode and path
    WriteProcessMemory(m_ProcessHandle, remoteBuffer, shellcode, sizeof(shellcode), nullptr);
    WriteProcessMemory(m_ProcessHandle, pathAddress, m_DllPath.c_str(), pathSize, nullptr);

    // Redirect RIP to shellcode
    ctx.Rip = (DWORD64)remoteBuffer;
    SetThreadContext(hThread, &ctx);

    // Resume thread
    ResumeThread(hThread);
    CloseHandle(hThread);

    std::cout << "[+] Thread hijack injection successful!\n";
    return true;
}

inline bool Injector::InjectQueueUserAPC() {
    std::cout << "[*] Trying QueueUserAPC injection...\n";

    // Allocate memory for DLL path
    size_t pathSize = m_DllPath.size() + 1;
    LPVOID remotePath = VirtualAllocEx(m_ProcessHandle, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        std::cout << "[!] Failed to allocate remote memory\n";
        return false;
    }

    // Write DLL path
    if (!WriteProcessMemory(m_ProcessHandle, remotePath, m_DllPath.c_str(), pathSize, nullptr)) {
        std::cout << "[!] Failed to write DLL path\n";
        VirtualFreeEx(m_ProcessHandle, remotePath, 0, MEM_RELEASE);
        return false;
    }

    // Get LoadLibraryA address
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC loadLibrary = GetProcAddress(kernel32, "LoadLibraryA");

    // Queue APC to all threads
    auto threads = GetThreadIds();
    bool success = false;

    for (DWORD tid : threads) {
        HANDLE hThread = OpenThread(THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, tid);
        if (hThread) {
            if (g_NtFuncs.NtQueueApcThread) {
                NTSTATUS status = g_NtFuncs.NtQueueApcThread(hThread, loadLibrary, remotePath, nullptr, nullptr);
                if (NT_SUCCESS(status)) {
                    success = true;
                }
            } else {
                if (QueueUserAPC((PAPCFUNC)loadLibrary, hThread, (ULONG_PTR)remotePath)) {
                    success = true;
                }
            }
            CloseHandle(hThread);
        }
    }

    if (!success) {
        std::cout << "[!] Failed to queue APC to any thread\n";
        VirtualFreeEx(m_ProcessHandle, remotePath, 0, MEM_RELEASE);
        return false;
    }

    std::cout << "[+] QueueUserAPC injection queued! Waiting for thread to become alertable...\n";
    Sleep(3000);  // Wait for APC to be processed
    
    return true;
}

inline bool Injector::InjectLdrLoadDll() {
    std::cout << "[*] Trying LdrLoadDll injection...\n";

    if (!g_NtFuncs.LdrLoadDll) {
        std::cout << "[!] LdrLoadDll not available\n";
        return false;
    }

    // Convert path to wide string
    std::wstring widePath(m_DllPath.begin(), m_DllPath.end());

    // Create UNICODE_STRING structure
    size_t structSize = sizeof(UNICODE_STRING) + (widePath.size() + 1) * sizeof(wchar_t);
    
    PVOID remoteBuffer = VirtualAllocEx(m_ProcessHandle, nullptr, structSize + sizeof(HANDLE), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteBuffer) {
        std::cout << "[!] Failed to allocate remote memory\n";
        return false;
    }

    // Prepare UNICODE_STRING locally
    UNICODE_STRING uniStr;
    uniStr.Length = (USHORT)(widePath.size() * sizeof(wchar_t));
    uniStr.MaximumLength = (USHORT)((widePath.size() + 1) * sizeof(wchar_t));
    uniStr.Buffer = (PWSTR)((PBYTE)remoteBuffer + sizeof(UNICODE_STRING));

    // Write structure
    WriteProcessMemory(m_ProcessHandle, remoteBuffer, &uniStr, sizeof(uniStr), nullptr);
    WriteProcessMemory(m_ProcessHandle, (PBYTE)remoteBuffer + sizeof(UNICODE_STRING), widePath.c_str(), (widePath.size() + 1) * sizeof(wchar_t), nullptr);

    // Shellcode to call LdrLoadDll
    BYTE shellcode[] = {
        0x48, 0x83, 0xEC, 0x38,                                     // sub rsp, 0x38
        0x48, 0x31, 0xC9,                                           // xor rcx, rcx (PathToFile = NULL)
        0x48, 0x31, 0xD2,                                           // xor rdx, rdx (Flags = 0)
        0x49, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov r8, <UNICODE_STRING ptr>
        0x49, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov r9, <ModuleHandle ptr>
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, <LdrLoadDll>
        0xFF, 0xD0,                                                 // call rax
        0x48, 0x83, 0xC4, 0x38,                                     // add rsp, 0x38
        0xC3                                                        // ret
    };

    PVOID handlePtr = (PBYTE)remoteBuffer + structSize;
    *(PVOID*)&shellcode[12] = remoteBuffer;
    *(PVOID*)&shellcode[22] = handlePtr;
    *(PVOID*)&shellcode[32] = g_NtFuncs.LdrLoadDll;

    // Allocate and write shellcode
    PVOID codeBuffer = VirtualAllocEx(m_ProcessHandle, nullptr, sizeof(shellcode), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!codeBuffer) {
        VirtualFreeEx(m_ProcessHandle, remoteBuffer, 0, MEM_RELEASE);
        return false;
    }

    WriteProcessMemory(m_ProcessHandle, codeBuffer, shellcode, sizeof(shellcode), nullptr);

    // Create remote thread
    HANDLE hThread = nullptr;
    NTSTATUS status = g_NtFuncs.NtCreateThreadEx(
        &hThread,
        THREAD_ALL_ACCESS,
        nullptr,
        m_ProcessHandle,
        codeBuffer,
        nullptr,
        0x00000004,
        0, 0, 0,
        nullptr
    );

    if (!NT_SUCCESS(status) || !hThread) {
        VirtualFreeEx(m_ProcessHandle, remoteBuffer, 0, MEM_RELEASE);
        VirtualFreeEx(m_ProcessHandle, codeBuffer, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);

    VirtualFreeEx(m_ProcessHandle, codeBuffer, 0, MEM_RELEASE);
    
    std::cout << "[+] LdrLoadDll injection successful!\n";
    return true;
}

inline bool Injector::Inject(InjectionMethod method) {
    switch (method) {
        case InjectionMethod::LoadLibrary:
            return InjectLoadLibrary();
        case InjectionMethod::NtCreateThreadEx:
            return InjectNtCreateThreadEx();
        case InjectionMethod::ManualMap:
            return InjectManualMap();
        case InjectionMethod::ThreadHijack:
            return InjectThreadHijack();
        case InjectionMethod::QueueUserAPC:
            return InjectQueueUserAPC();
        case InjectionMethod::LdrLoadDll:
            return InjectLdrLoadDll();
        default:
            return false;
    }
}

inline bool Injector::TryAllMethods() {
    std::cout << "\n[*] Trying all injection methods...\n\n";

    // Try methods from most to least stealthy
    if (InjectManualMap()) return true;
    std::cout << "\n";
    
    if (InjectNtCreateThreadEx()) return true;
    std::cout << "\n";
    
    if (InjectLdrLoadDll()) return true;
    std::cout << "\n";
    
    if (InjectThreadHijack()) return true;
    std::cout << "\n";
    
    if (InjectQueueUserAPC()) return true;
    std::cout << "\n";
    
    if (InjectLoadLibrary()) return true;

    std::cout << "\n[!] All injection methods failed!\n";
    return false;
}
