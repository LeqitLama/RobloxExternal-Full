#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <Psapi.h>

// NT function definitions
typedef LONG NTSTATUS;
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, * PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES, * POBJECT_ATTRIBUTES;

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID, * PCLIENT_ID;

typedef struct _PS_ATTRIBUTE {
    ULONG Attribute;
    SIZE_T Size;
    union {
        ULONG Value;
        PVOID ValuePtr;
    };
    PSIZE_T ReturnLength;
} PS_ATTRIBUTE, * PPS_ATTRIBUTE;

typedef struct _PS_ATTRIBUTE_LIST {
    SIZE_T TotalLength;
    PS_ATTRIBUTE Attributes[1];
} PS_ATTRIBUTE_LIST, * PPS_ATTRIBUTE_LIST;

// NT function types
typedef NTSTATUS(NTAPI* pNtCreateThreadEx)(
    PHANDLE ThreadHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    HANDLE ProcessHandle,
    PVOID StartRoutine,
    PVOID Argument,
    ULONG CreateFlags,
    SIZE_T ZeroBits,
    SIZE_T StackSize,
    SIZE_T MaximumStackSize,
    PPS_ATTRIBUTE_LIST AttributeList
);

typedef NTSTATUS(NTAPI* pNtWriteVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID BaseAddress,
    PVOID Buffer,
    SIZE_T NumberOfBytesToWrite,
    PSIZE_T NumberOfBytesWritten
);

typedef NTSTATUS(NTAPI* pNtAllocateVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID* BaseAddress,
    ULONG_PTR ZeroBits,
    PSIZE_T RegionSize,
    ULONG AllocationType,
    ULONG Protect
);

typedef NTSTATUS(NTAPI* pNtProtectVirtualMemory)(
    HANDLE ProcessHandle,
    PVOID* BaseAddress,
    PSIZE_T RegionSize,
    ULONG NewProtect,
    PULONG OldProtect
);

typedef NTSTATUS(NTAPI* pNtOpenProcess)(
    PHANDLE ProcessHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PCLIENT_ID ClientId
);

typedef NTSTATUS(NTAPI* pNtClose)(
    HANDLE Handle
);

typedef NTSTATUS(NTAPI* pNtQueueApcThread)(
    HANDLE ThreadHandle,
    PVOID ApcRoutine,
    PVOID ApcArgument1,
    PVOID ApcArgument2,
    PVOID ApcArgument3
);

typedef NTSTATUS(NTAPI* pNtResumeThread)(
    HANDLE ThreadHandle,
    PULONG SuspendCount
);

typedef NTSTATUS(NTAPI* pNtSuspendThread)(
    HANDLE ThreadHandle,
    PULONG PreviousSuspendCount
);

typedef NTSTATUS(NTAPI* pLdrLoadDll)(
    PWCHAR PathToFile,
    ULONG Flags,
    PUNICODE_STRING ModuleFileName,
    PHANDLE ModuleHandle
);

// Injection methods enum
enum class InjectionMethod {
    LoadLibrary,
    NtCreateThreadEx,
    ManualMap,
    ThreadHijack,
    QueueUserAPC,
    LdrLoadDll
};

// Helper class for NT functions
class NtFunctions {
public:
    pNtCreateThreadEx NtCreateThreadEx = nullptr;
    pNtWriteVirtualMemory NtWriteVirtualMemory = nullptr;
    pNtAllocateVirtualMemory NtAllocateVirtualMemory = nullptr;
    pNtProtectVirtualMemory NtProtectVirtualMemory = nullptr;
    pNtOpenProcess NtOpenProcess = nullptr;
    pNtClose NtClose = nullptr;
    pNtQueueApcThread NtQueueApcThread = nullptr;
    pNtResumeThread NtResumeThread = nullptr;
    pNtSuspendThread NtSuspendThread = nullptr;
    pLdrLoadDll LdrLoadDll = nullptr;

    bool Initialize() {
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll) return false;

        NtCreateThreadEx = (pNtCreateThreadEx)GetProcAddress(ntdll, "NtCreateThreadEx");
        NtWriteVirtualMemory = (pNtWriteVirtualMemory)GetProcAddress(ntdll, "NtWriteVirtualMemory");
        NtAllocateVirtualMemory = (pNtAllocateVirtualMemory)GetProcAddress(ntdll, "NtAllocateVirtualMemory");
        NtProtectVirtualMemory = (pNtProtectVirtualMemory)GetProcAddress(ntdll, "NtProtectVirtualMemory");
        NtOpenProcess = (pNtOpenProcess)GetProcAddress(ntdll, "NtOpenProcess");
        NtClose = (pNtClose)GetProcAddress(ntdll, "NtClose");
        NtQueueApcThread = (pNtQueueApcThread)GetProcAddress(ntdll, "NtQueueApcThread");
        NtResumeThread = (pNtResumeThread)GetProcAddress(ntdll, "NtResumeThread");
        NtSuspendThread = (pNtSuspendThread)GetProcAddress(ntdll, "NtSuspendThread");
        LdrLoadDll = (pLdrLoadDll)GetProcAddress(ntdll, "LdrLoadDll");

        return NtCreateThreadEx && NtWriteVirtualMemory && NtAllocateVirtualMemory;
    }
};

inline NtFunctions g_NtFuncs;
