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

### [IRQL](https://en.wikipedia.org/wiki/IRQL_(Windows))
The interrupt controller sends interrupt request(IRQ) to CPU with a certain priority level, and CPU sets mask that causes any other interrupts with a lower priority to be put into a pending state, until CPU releases control back to interrupt controller.

### [SEH](https://learn.microsoft.com/en-us/cpp/cpp/structured-exception-handling-c-cpp?view=msvc-170)
Microsoft extension to C and C++ to handle certain exceptional code situations, such as hardware faults

## Unhandled Exceptions
user mode: cause the process to terminate prematurely

kernel mode: BSOD, protective mechanism

## Termination
process terminate: all private memory is freed,
all handles are closed

kernel: these resources will not be freed
automatically, only released at the next system boot

freed automatically: too dangerous to achieve, might casue system crash

### C++ features cannot be used:
- `new` and `delete`: allocate from a user-mode heap
- global variables: non-default constructors will not be called, create some Init function instead 
- `try`, `catch`, `throw`: kernel can only be done using SEH
- standard C++ libraries: but C++ templates are fine
- `nullptr`
- `auto`


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

## Dynamic Memory Allocation
Drivers often need to allocate memory dynamically.

The kernel provides two general memory pools for drivers to use.

- Paged pool - memory pool that can be paged out if required.

- Non Paged Pool - memory pool that is never paged out and is guaranteed to remain in RAM.

Page: is a fixed-length contiguous block of virtual memory, described by a single entry in the page table.

Non-paged pool is a “better” memory pool as it can never incur a page fault.

Example: [Dynamic Memory Allocation](https://github.com/poipoiyo/Book-Review/blob/main/WindowsKernelProgramming/CH3/Memory%20Allocation.md)

## Lists
The kernel uses circular doubly linked lists in many of its internal data structures.

```C++
typedef struct _LIST_ENTRY {
  struct _LIST_ENTRY *Flink;
  struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY;
```

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH3/3-2%20Circular%20linked%20list.png" width="80%" />

### [EPROCESS](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/eprocess)
- connected in circular doubly linked list 
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

## [Driver Object](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_driver_object)
- Each driver object represents image of a loaded kernel-mode driver. 
- A pointer to driver object is an input parameter to driver's `DriverEntry`, `AddDevice`, `Reinitialize` `Unload`.
- Semi-documented, is allocated by the kernel and partially initialized. 
- Semi-documented: some of its members are documented for driver’s use and some are not.

### DriverEntry
```C++
DriverEntry(
  _In_ PDRIVER_OBJECT DriverObject, 
  _In_ PUNICODE_STRING RegistryPath
 )
```
- Dispatch Routines: member of `DriverObject` is an array of function pointers, specifies particular operations, such as Create, Read, Write
- In DriverEntry, only needs to initialize actual operations it supports, leaving other default.
- A driver must at least support `IRP_MJ_CREATE`, `IRP_MJ_CLOSE` operations, to allow opening a handle to device objects for driver.

### Major function code
|Major function| Description|
| ---- | ---- |
| IRP_MJ_CREATE (0) | Create operation. Typically invoked for `CreateFile` or `ZwCreateFile` calls |
| IRP_MJ_CLOSE (2)  | Close operation. Normally invoked for `CloseHandle` or `ZwClose` calls |
| IRP_MJ_READ (3)  | Read operation. Typically invoked for `ReadFile`, `ZwReadFile` and similar read APIs |
| IRP_MJ_DEVICE_CONTROL (14) | Generic call to a driver, invoked because of DeviceIoControl or ZwDeviceIoControlFile calls |
| IRP_MJ_INTERNAL_DEVICE_CONTROL (15) |  Similar to the previous one, but only available for kernel mode callers |
| IRP_MJ_PNP (31) | Plug and play callback invoked by the Plug and Play Manager. Generally interesting for hardware-based drivers or filters to such drivers |
| IRP_MJ_POWER (22) | Power callback invoked by the Power Manager. Generally interesting for hardware-based drivers filters to such drivers |


## [Device Object](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/introduction-to-device-objects)
- Actual communication endpoints for clients to talk to drivers. 
- Are instances of the semi-documented `DEVICE_OBJECT` structure. 
- At least one should be created and given a name, so that it may be contacted by clients.

### FILE_OBJECT 
- To user-mode, represents an open instance of a file, device, directory, or volume. 
- To device and intermediate drivers, represents device object. 
- To drivers in file system stack, represents a directory or file.

### Symbolic links
- located in Object Manager directory named ?? (check by [WinObj](https://learn.microsoft.com/en-us/sysinternals/downloads/winobj))


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

### [IoCreateDevice](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iocreatedevice)
Creates a device object for use by a driver

Driver and multi devices:

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH3/3-5%20Driver%20and%20Device%20objects.png" width="80%" />



