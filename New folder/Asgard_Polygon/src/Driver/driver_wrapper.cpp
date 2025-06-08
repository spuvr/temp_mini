#include "driver_wrapper.h"

// Global driver instance
DriverWrapper driver;

bool DriverWrapper::CacheProcessDirectoryTableBase()
{
    if (kernelHandle == INVALID_HANDLE_VALUE)
        return false;

    SystemRequest Request{};
    Request.Process = processHandle;
    Request.CALL = SystemRequest::_CALL::cache;

    IO_STATUS_BLOCK ioStatus;
    NTSTATUS status = NtDeviceIoControlFileImport(
        kernelHandle,
        NULL,
        NULL,
        NULL,
        &ioStatus,
        DRIVER_CALL,
        &Request,
        sizeof(Request),
        &Request,
        sizeof(Request)
    );

    return NT_SUCCESS(ioStatus.Status);
}

void DriverWrapper::CacheThread()
{
    while (isCaching)
    {
        CacheProcessDirectoryTableBase();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

bool DriverWrapper::Attach(const wchar_t* ProcessName)
{
    HMODULE NTDLL = GetModuleHandleA("ntdll.dll");
    if (!NTDLL) return false;

    NtDeviceIoControlFileImport = (NtDeviceIoControlFile_t)GetProcAddress(NTDLL, "NtDeviceIoControlFile");
    if (!NtDeviceIoControlFileImport)
        return false;

    HANDLE SnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (SnapShot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W Entry{};
    Entry.dwSize = sizeof(PROCESSENTRY32W);

    BOOL success = Process32FirstW(SnapShot, &Entry);
    while (success)
    {
        if (_wcsicmp(Entry.szExeFile, ProcessName) == 0)
        {
            processHandle = Entry.th32ProcessID;
            break;
        }
        success = Process32NextW(SnapShot, &Entry);
    }
    CloseHandle(SnapShot);

    if (processHandle == 0)
        return false;

    kernelHandle = CreateFileW(
        L"\\\\.\\sigmadriverr",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (kernelHandle == INVALID_HANDLE_VALUE)
        return false;

    isCaching = true;
    caching = std::thread(&DriverWrapper::CacheThread, this);
    caching.detach();

    return true;
}

void DriverWrapper::Detach()
{
    if (kernelHandle != INVALID_HANDLE_VALUE)
    {
        isCaching = false;
        CloseHandle(kernelHandle);
        kernelHandle = INVALID_HANDLE_VALUE;
    }
    processHandle = 0;
}

bool DriverWrapper::ReadVirtualMemory(uintptr_t Address, void* Buffer, SIZE_T Size)
{
    if (kernelHandle == INVALID_HANDLE_VALUE)
        return false;

    SystemRequest Request{};
    Request.Address = (PVOID)Address;
    Request.Buffer = Buffer;
    Request.BufferSize = Size;
    Request.Process = processHandle;
    Request.CALL = SystemRequest::_CALL::read;

    IO_STATUS_BLOCK ioStatus;
    NTSTATUS status = NtDeviceIoControlFileImport(
        kernelHandle,
        NULL,
        NULL,
        NULL,
        &ioStatus,
        DRIVER_CALL,
        &Request,
        sizeof(Request),
        &Request,
        sizeof(Request)
    );

    return NT_SUCCESS(ioStatus.Status);
}

bool DriverWrapper::WriteVirtualMemory(uintptr_t Address, void* Buffer, SIZE_T Size)
{
    if (kernelHandle == INVALID_HANDLE_VALUE)
        return false;

    SystemRequest Request{};
    Request.Address = (PVOID)Address;
    Request.Buffer = Buffer;
    Request.BufferSize = Size;
    Request.Process = processHandle;
    Request.CALL = SystemRequest::_CALL::write;

    IO_STATUS_BLOCK ioStatus;
    NTSTATUS status = NtDeviceIoControlFileImport(
        kernelHandle,
        NULL,
        NULL,
        NULL,
        &ioStatus,
        DRIVER_CALL,
        &Request,
        sizeof(Request),
        &Request,
        sizeof(Request)
    );

    return NT_SUCCESS(ioStatus.Status);
}

uintptr_t DriverWrapper::GetModuleBase(const wchar_t* ModuleName)
{
    if (!processHandle)
        return 0;

    HANDLE SnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processHandle);
    if (SnapShot == INVALID_HANDLE_VALUE)
        return 0;

    MODULEENTRY32W ModuleEntry{};
    ModuleEntry.dwSize = sizeof(MODULEENTRY32W);

    BOOL success = Module32FirstW(SnapShot, &ModuleEntry);
    while (success)
    {
        if (_wcsicmp(ModuleEntry.szModule, ModuleName) == 0)
        {
            CloseHandle(SnapShot);
            return (uintptr_t)ModuleEntry.modBaseAddr;
        }
        success = Module32NextW(SnapShot, &ModuleEntry);
    }
    CloseHandle(SnapShot);
    return 0;
}