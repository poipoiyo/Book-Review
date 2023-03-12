# Chapter 3: Kernel Programming Basics

## General Kernel Programming Guidelines

Developing kernel drivers requires the Windows Driver Kit (WDK)

||User Mode| Kernel Mode|
| ---- | ---- | ---- |
|Unhandled Exception|Crashes the process|Crashes the system|
|Termination|All private memory are freed automatically|Leak will only be resolved in next boot|
|Return values|Sometimes ignored|Should never ignore|
|IRQL|Always PASSIVE_LEVEL (0)|DISPATCH_LEVEL (2) or higher|
|Bad coding|Typically localized to the process|Can have system-wide effect|
|Testing and Debugging|Done on the developer’s machine|Must be done with another machine|
|Libraries|Can use almost and C/C++ library|Most standard libraries cannot be used|
|Exception Handling|Can use C++ exceptions or SEH|Only SEH can be used|
|C++ Usage|Full C++ runtime available|No C++ runtime|

## Unhandled Exceptions
user mode: cause the process to terminate prematurely

kernel mode: BSOD, protective mechanism

## Termination
process terminate: all private memory is freed,
all handles are closed

kernel: these resources will not be freed
automatically, only released at the next system boot

freed automatically: too dangerous to achieve, might casue system crash

## C++ Usage

Kernel mode: started supporting C++ with Visual Studio 2012 and WDK 8.

Resource Acquisition Is Initialization (RAII): to make sure don't leak resources.

### C++ features cannot be used:
- `new` and `delete`: allocate from a user-mode heap
- global variables: non-default constructors will not be called, create some Init function instead 
- `try`, `catch`, `throw`: kernel can only be done using SEH
- standard C++ libraries: but C++ templates are fine
- `nullptr`
- `auto`

## Debug and Release Builds
Debug build uses no optimizations by default, but easier to debug.

## Kernel API
|Prefix|Meaning|
| ---- | ---- |
|Ex|general executive functions|
|Ke|general kernel functions|
|Mm|memory manager|
|Rtl|general runtime library|
|FsRtl|file system runtime library|
|Flt|file system mini-filter library|
|Ob|object manager|
|Io|I/O manager|
|Se|security|
|Ps|process structure|
|Po|power manager|
|Wmi|Windows management instrumentation|
|Zw|native API wrappers|
|Hal|hardware abstraction layer|
|Cm|configuration manager (registry)|

## Functions and Error Codes
Return `NTSTATUS` as operation success or failure.

Most code paths don’t care about the exact nature of the error, and so testing the most significant bit
is enough. 

This can be done with the NT_SUCCESS macro. 

https://www.osr.com/wp-content/uploads/NTtoDos.pdf

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH3/Win32%20error%20mapping.png" width="60%" />

```C++
NTSTATUS DoWork() {
  NTSTATUS status = CallSomeKernelFunction();
  if(!NT_SUCCESS(Statue)) {
    KdPirnt((L"Error occurred: 0x%08X\n", status));
    return status;
  return STATUS_SUCCESS;
}
```

## Strings
most functions dealing with strings expect a structure of type UNICODE_STRING.

The UNICODE_STRING structure represents a string with its length and maximum length known.

```C++
typedef struct _UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWCH Buffer;
} UNICODE_STRING;
```

Common UNICODE_STRING functions: `RtlInitUnicodeString `, `RtlCopyUnicodeString `, `RtlCompareUnicodeString`, `RtlEqualUnicodeString`, `RtlAppendUnicodeStringToString`

Some well-known string functions: `wcscpy`, `wcscat`, `wcslen`, `wcscpy_s`, `wcschr`, `strcpy`, `strcpy_s`

## Dynamic Memory Allocation
Drivers often need to allocate memory dynamically.

The kernel provides two general memory pools for drivers to use.

- Paged pool - memory pool that can be paged out if required.

- Non Paged Pool - memory pool that is never paged out and is guaranteed to remain in RAM.

Page: is a fixed-length contiguous block of virtual memory, described by a single entry in the page table.

Non-paged pool is a “better” memory pool as it can never incur a page fault.

Useful functions: `ExAllocatePool `, `ExAllocatePoolWithTag`, `ExAllocatePoolWithQuotaTag`, `ExFreePool`

View pool allocations: Poolmon WDK tool

## Lists
The kernel uses circular doubly linked lists in many of its internal data structures.

```C++
typedef struct _LIST_ENTRY {
  struct _LIST_ENTRY *Flink;
  struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY;
```

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH3/3-2%20Circular%20linked%20list.png" width="80%" />

### EPROCESS
- connected in a circular doubly linked list 
- head: stored the kernel variable  `PsActiveProcessHead`).
- member: `ActiveProcessLinks` is of type `LIST_ENTRY`, pointing to the next and previous `LIST_ENTRY` objects
- `CONTAINING_RECORD` macro: to get pointer to actual structure of address of a `LIST_ENTRY`

```C++
struct MyDataItem {
  // some data members
  LIST_ENTRY Link;
  // more data members
};

MyDataItem* GetItem(LIST_ENTRY* pEntry) {
  return CONTAINING_RECORD(pEntry, MyDataItem, Link);
}
```

## The Driver Object
DriverEntry function accepts two arguments.

```C++
DriverEntry(
  _In_ PDRIVER_OBJECT DriverObject, 
  _In_ PUNICODE_STRING RegistryPath
 )
```

- Driver object: Semi-documented, is allocated by the kernel and partially initialized. 
- Semi-documented: some of its members are documented for driver’s use and some are not.
- Dispatch Routines: member of `DriverObject` is an array of function pointers, specifies particular operations, such as Create, Read, Write
- In DriverEntry, only needs to initialize the actual operations it supports, leaving other default.
- A driver must at least support `IRP_MJ_CREATE`, `IRP_MJ_CLOSE` operations, to allow opening a handle to one the device objects for the driver.

## Device Object
- Actual communication endpoints for clients to talk to drivers. 
- Are instances of the semi-documented DEVICE_OBJECT structure. 
- At least one should be created and given a name, so that it may be contacted by clients.

[Introduction to Device Objects](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/introduction-to-device-objects)

### CreateFile
- First argument “file name”(file object) should point to a device object’s name.
- Opening a handle to a file or device creates a instance of the kernel structure `FILE_OBJECT`. (Semi-documented)
- Accepts a symbolic link, a kernel object that knows how to point to another kernel object. (file system shortcut)
- Symbolic linksare located in Object Manager directory named ?? (check by WinObj)
- The names in ?? directory are not accessible by user mode, but can be accessed by kernel. (by `IoGetDeviceObjectPointer`)

[FILE_OBJECT](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_file_object)

###  Process Explorer
- Install a driver after launched with administrator rights.
- Driver give Process Explorer powers beyond those that can be obtained by user mode APIs.
- Driver installed by Process Explorer creates a single device object so that Process Explorer is able to open device handle.
- Device object must be named and have a symbolic link in ?? directory

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH3/3-4%20Process%20Explorer%E2%80%99s%20symbolic%20link%20in%20WinObj.png" width="80%" />

Open a handle to its device:
```C+++
HANDLE hDevice = CreateFile(L"\\\\.\\PROCEXP152",
  GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
```

Driver creates a device object using `IoCreateDevice` function. 

IoCreateDevice: allocates and initializes device object and returns its pointer. 

Device object instance is stored `DRIVER_OBJECT`
Driver and multi devices:

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH3/3-5%20Driver%20and%20Device%20objects.png" width="80%" />



