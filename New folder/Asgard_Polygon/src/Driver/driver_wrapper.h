#ifndef DRIVER_WRAPPER_H
#define DRIVER_WRAPPER_H

#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <winternl.h>
#include <thread>
#include <chrono>

#define STATUS_SUCCESS  ((NTSTATUS)0x00000000L)

typedef NTSTATUS(
    NTAPI*
    NtDeviceIoControlFile_t)(
        IN HANDLE FileHandle,
        IN HANDLE Event OPTIONAL,
        IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
        IN PVOID ApcContext OPTIONAL,
        OUT PIO_STATUS_BLOCK IoStatusBlock,
        IN ULONG IoControlCode,
        IN PVOID InputBuffer OPTIONAL,
        IN ULONG InputBufferLength,
        OUT PVOID OutputBuffer OPTIONAL,
        IN ULONG OutputBufferLength
        );

struct SystemRequest
{
    PVOID Address;
    PVOID Buffer;
    SIZE_T BufferSize;
    INT Process;
    enum _CALL
    {
        read,
        write,
        cache
    } CALL;
};

const ULONG DRIVER_CALL = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS);

class DriverWrapper
{
private:
    std::thread caching;
    bool isCaching = false;
    NtDeviceIoControlFile_t NtDeviceIoControlFileImport = nullptr;

public:
    HANDLE kernelHandle = INVALID_HANDLE_VALUE;
    INT processHandle = 0;

    bool CacheProcessDirectoryTableBase();
    void CacheThread();
    bool Attach(const wchar_t* ProcessName);
    void Detach();
    bool ReadVirtualMemory(uintptr_t Address, void* Buffer, SIZE_T Size);
    bool WriteVirtualMemory(uintptr_t Address, void* Buffer, SIZE_T Size);
    uintptr_t GetModuleBase(const wchar_t* ModuleName);

    template<typename T>
    T read(uintptr_t Address)
    {
        T Buffer{};
        this->ReadVirtualMemory(Address, &Buffer, sizeof(T));
        return Buffer;
    }

    template<typename T>
    void write(uintptr_t Address, const T& Buffer)
    {
        this->WriteVirtualMemory(Address, (void*)&Buffer, sizeof(T));
    }
};

// Global driver instance
extern DriverWrapper driver;

#endif // DRIVER_WRAPPER_H