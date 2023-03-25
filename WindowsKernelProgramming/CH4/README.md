# Chapter 4: Driver from Start to Finish

## Introduction
- Solve: simple driver to fix inflexibility of setting thread priorities using the Windows API
- Thread priority: determined by combination of its process Priority Class (user mode)
- `SetPriorityClass`: change a process priority class
- `SetThreadPriority`: change a particular thread’s priority

Only six levels are available:

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH4/4-0%20Legal%20values%20for%20thread%20priorities%20with%20the%20Windows%20APIs.png" width="60%" />

The values acceptable to SetThreadPriority specify the offset. 
Five levels correspond to the offsets
-2 to +2: 
- `THREAD_PRIORITY_LOWEST` (-2)
- `THREAD_PRIORITY_BELOW_NORMAL`(-1)
- `THREAD_PRIORITY_NORMAL` (0)
- `THREAD_PRIORITY_ABOVE_NORMAL`(+1)
- `THREAD_PRIORITY_HIGHEST` (+2)
  
Saturation levels, two extremes levels:
- `THREAD_PRIORITY_IDLE` (-Sat) 
- `THREAD_PRIORITY_TIME_CRITICAL` (+Sat)
  
## Driver Initialization
Need to do in `DriverEntry`(to take request):
- Set an Unload routine
- Set dispatch routines the driver supports
- Create a device object
- Create a symbolic link to the device object

### Step 1. 
`DriverEntry` with Unload routine:
```C++
void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject);

extern "C" NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
  DriverObject->DriverUnload = PriorityBoosterUnload;
  return STATUS_SUCCESS;
}

void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject) {
}
```

### Step 2.
All drivers must
support `IRP_MJ_CREATE` and `IRP_MJ_CLOSE`, to open handle to
device.

```C++
DriverObject->MajorFunction[IRP_MJ_CREATE] = PriorityBoosterCreateClose;
DriverObject->MajorFunction[IRP_MJ_CLOSE] = PriorityBoosterCreateClose;
```

[IRP_MJ_CREATE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/irp-mj-create): 
- A new file or directory is being created.
- An existing file, device, directory, or volume is being opened.

[IRP_MJ_CLOSE](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-mj-close):
- Request indicates that the last handle of file object that is associated with target device object has been closed and released. 
- All outstanding I/O requests have been completed or canceled.

### Step 3.
All major functions have the same prototype. Add a prototype:
```C++
NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
```
Return `NTSTATUS` and accepts a pointer to device object and a pointer to an I/O
Request Packet (IRP). 

[IRP](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_irp): the primary object to store all types requerst  

## Passing Information to the Driver
Need to tell driver which thread and to what value to set priority

- User mode basic functions: `WriteFile`, `ReadFile` and `DeviceIoControl`

### [DeviceIoControl](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-deviceiocontrol)
```C++
BOOL WINAPI DeviceIoControl(
  _In_ HANDLE hDevice,
  _In_ DWORD dwIoControlCode,
  _In_reads_bytes_opt_(nInBufferSize) LPVOID lpInBuffer,
  _In_ DWORD nInBufferSize,
  _Out_writes_bytes_to_opt_(nOutBufferSize,*lpBytesReturned) LPVOID lpOutBuffer,
  _In_ DWORD nOutBufferSize,
  _Out_opt_ LPDWORD lpBytesReturned,
  _Inout_opt_ LPOVERLAPPED lpOverlapped
);
```
- sends control code directly to specified device driver, causing corresponding device to perform corresponding operation.

- is preferred to pass data to and from driver(flexible)

- important pieces: control code, input buffer, output buffer

- corresponds to `IRP_MJ_DEVICE_CONTROL` major function

### Add to initialization
```C++
DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PriorityBoosterDeviceControl;
```

[IRP_MJ_DEVICE_CONTROL](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-mj-device-control)
- If IOCTLs exists, every driver whose device objects belong to a particular device type is required to support this request in `DispatchDeviceControl` routine.

[IOCTLs](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/introduction-to-i-o-control-codes)
- used for communication between user-mode applications and drivers, or for communication internally among drivers in a stack.

## Client / Driver Communication Protocol
Need a control code and input buffer, because of using `DeviceIoControl`.

### Buffer 
- thread id 
- priority to set for it
- must be usable both by driver and client
- definitions must be in separate file that
must be included by driver and client code


Add `PriorityBoosterCommon.h`:
```C++
struct ThreadData {
  ULONG ThreadId;
  int Priority;
};
```

### Define control code
```C++
#define CTL_CODE( DeviceType, Function, Method, Access ) ( \
((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))

#define PRIORITY_BOOSTER_DEVICE 0x8000

#define IOCTL_PRIORITY_BOOSTER_SET_PRIORITY CTL_CODE(PRIORITY_BOOSTER_DEVICE, \
0x800, METHOD_NEITHER, FILE_ANY_ACCESS)
```

- DeviceType: Identifies a type of device, mostly for hardware

- Function: An ascending number indicating a specific operation. 
This number must be different between different control codes for the same driver.

- Method: Indicates how input and output buffers provided by client pass to driver. 

- Access: Indicates whether this operation is to driver. Typical just use `FILE_ANY_ACCESS` and deal with  actual request in `IRP_MJ_DEVICE_CONTROL`.

## Creating Device Object
Need one device object, with a symbolic link pointing to it. (for user mode to obtain handle)

### [IoCreateDevice](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iocreatedevice)
Creates device object for use by  driver
```C++
NTSTATUS IoCreateDevice(
  _In_ PDRIVER_OBJECT DriverObject, 
  _In_ ULONG DeviceExtensionSize, // for associating some data structure with device
  _In_opt_ PUNICODE_STRING DeviceName,
  _In_ DEVICE_TYPE DeviceType, // should be FILE_DEVICE_UNKNOWN
  _In_ ULONG DeviceCharacteristics,  // a set of flags
  _In_ BOOLEAN Exclusive, // should multi file object allow to open same device
  _Outptr_ PDEVICE_OBJECT *DeviceObject // returned pointer
);
```

### Hold internal device name
Create `UNICODE_STRING`:
```C++
UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\PriorityBooster");
// RtlInitUnicodeString(&devName, L"\\Device\\ThreadBoost");
```

### Call `IoCreateDevice` function
```C++
PDEVICE_OBJECT DeviceObject;
NTSTATUS status = IoCreateDevice(
  DriverObject // our driver object,
  0 // no need for extra bytes,
  &devName // the device name,
  FILE_DEVICE_UNKNOWN // device type,
  0 // characteristics flags,
  FALSE // not exclusive,
  &DeviceObject // the resulting pointer
);

if (!NT_SUCCESS(status)) {
  KdPrint(("Failed to create device object (0x%08X)\n", status));
  return status;
}
```

### Create symbolic link and connect to device object:
```C++
UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
status = IoCreateSymbolicLink(&symLink, &devName);
if (!NT_SUCCESS(status)) {
  KdPrint(("Failed to create symbolic link (0x%08X)\n", status));
  IoDeleteDevice(DeviceObject);
  return status;
}
```
- If creation fails, must undo everything done, in this case call `IoDeleteDevice`.
- If `DriverEntry` returns any failure status, the Unload routine is not called. 
- Once there is symbolic link, `DriverEntry` can return success and driver is ready to accept requests.
- In this case, there are two things: device object creation and symbolic link creation.

### Undo them in reverse order
```C++
void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject) {
  UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
  // delete symbolic link
  IoDeleteSymbolicLink(&symLink);
  
  // delete device object
  IoDeleteDevice(DriverObject->DeviceObject);
}
```

## Client Code
```C++
#include <windows.h>
#include <stdio.h>
#include "..\PriorityBooster\PriorityBoosterCommon.h"

int main(int argc, const char* argv[]) {
  if (argc < 3) {
    printf("Usage: Booster <threadid> <priority>\n");
    return 0;
  }

  HANDLE hDevice = CreateFile(L"\\\\.\\PriorityBooster", GENERIC_WRITE,
FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  if (hDevice == INVALID_HANDLE_VALUE)
    return Error("Failed to open device");

  ThreadData data;
  data.ThreadId = atoi(argv[1]); // command line first argument
  data.Priority = atoi(argv[2]); // command line second argument

  DWORD returned;
  BOOL success = DeviceIoControl(hDevice,
    IOCTL_PRIORITY_BOOSTER_SET_PRIORITY, // control code
    &data, sizeof(data), // input buffer and length
    nullptr, 0, // output buffer and length
    &returned, nullptr);

  if (success)
    printf("Priority change succeeded!\n");
  else
    Error("Priority change failed!");
  CloseHandle(hDevice);
}

int Error(const char* message) {
  printf("%s (error=%d)\n", message, GetLastError());
  return 1;
}
```

## The Create and Close Dispatch Routines
Create/Close dispatch routine implementation:
```C++
_Use_decl_annotations_
NTSTATUS PriorityBoosterCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  UNREFERENCED_PARAMETER(DeviceObject);
  Irp->IoStatus.Status = STATUS_SUCCESS;
  Irp->IoStatus.Information = 0;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return STATUS_SUCCESS;
}
```
- Every dispatch routine accepts target device object and an I/O Request Packet(IRP), which is more important.
- Driver’s purpose is to handle IRP, which means looking at the details of the request and doing what needs to be done to complete it.
- Every request to driver always wrapped in IRP, whether Create, Close, Read...
- IRP never arrives alone, accompanied by one or more structures of type `IO_STACK_LOCATION`. 
- [IO_STACK_LOCATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_io_stack_location): is an entry in the I/O stack that is associated with each IRP
- In case of Create and Close, just need to set status of IRP in its `IoStatus` member (of type `IO_STATUS_BLOCK`)

### [IO_STATUS_BLOCK](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_io_status_block)
request status, and pointer meaning different things
```C++
typedef struct _IO_STATUS_BLOCK {
  union {
    NTSTATUS Status;
    PVOID    Pointer;
  };
  ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;
```

### [IoCompleteRequest](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iocompleterequest)
```C++
void IofCompleteRequest(
  PIRP  Irp,
  CCHAR PriorityBoost
); 
```
- Caller has completed all processing for a given I/O request and is returning the given IRP to the I/O manager
- Second argument is a temporary priority boost value that a driver can provide to its client. 
- `IO_NO_INCREMENT`(zero) is the best, because the request completed synchronously, no reason to get a priority boost.

## The DeviceIoControl Dispatch Routine
Actual work of setting a given thread to a requested priority.

### Check control code
- Typical drivers may support many control codes.
- Fail the request if control code is not recognized

```C++
_Use_decl_annotations_
NTSTATUS PriorityBoosterDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
  // get our IO_STACK_LOCATION
  auto stack = IoGetCurrentIrpStackLocation(Irp); // IO_STACK_LOCATION*
  auto status = STATUS_SUCCESS;
  switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_PRIORITY_BOOSTER_SET_PRIORITY:
      // do the work
      break;
    default:
      status = STATUS_INVALID_DEVICE_REQUEST;
      break;
  }
```

### [IoGetCurrentIrpStackLocation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iogetcurrentirpstacklocation) 
returns a pointer to caller's I/O stack location in specified IRP

### Complete IRP after switch block:
```C++
Irp->IoStatus.Status = status;
Irp->IoStatus.Information = 0;
IoCompleteRequest(Irp, IO_NO_INCREMENT);
return status;
```

 ### Check received buffer is large enough 
 should be able to contain `ThreadData` object. 

```C++
if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ThreadData)) {
  status = STATUS_BUFFER_TOO_SMALL;
  break;
}

auto data = (ThreadData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;

if (data == nullptr) {
  status = STATUS_INVALID_PARAMETER;
  break;
}
```

### check legal range
```C++
if (data->Priority < 1 || data->Priority > 31) {
  status = STATUS_INVALID_PARAMETER;
  break;
}
```

### Get thread id and set priority:
```C++
#include <ntifs.h>

PETHREAD Thread;
status = PsLookupThreadByThreadId(ULongToHandle(data->ThreadId), &Thread);
if (!NT_SUCCESS(status))
  break;
```

### [PsLookupThreadByThreadId](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-pslookupthreadbythreadid)
```C++
NTSTATUS PsLookupThreadByThreadId(
  [in]  HANDLE   ThreadId,
  [out] PETHREAD *Thread
);
```
- Accepts thread ID of a thread and returns a referenced pointer to `ETHREAD` structure of thread.
- Accepts HANDLE rather than ID, because of the way process and thread IDs generated. 
- These are generated from global private kernel handle table, so handle “values” are actual IDs.

### [KeSetPriorityThread](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kesetprioritythread)
```C++
KeSetPriorityThread((PKTHREAD)Thread, data->Priority);
```
- Sets run-time priority of driver-created thread
- Accepts a `PKTHREAD` rather than `PETHREAD`. These are the same, because the first member of an `ETHREAD` is a `KTHREAD`(`Tcb`).
-  The thread won't be terminated,  before setting new priority? (Won't be dangle pointer)

`ObDereferenceObject`:　decrease thread object’s reference to prevent from leaking

### Complete IRP_MJ_DEVICE_CONTROL handler:
```C++
_Use_decl_annotations_
NTSTATUS PriorityBoosterDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
// get our IO_STACK_LOCATION
  auto stack = IoGetCurrentIrpStackLocation(Irp); // IO_STACK_LOCATION*
  auto status = STATUS_SUCCESS;

  switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_PRIORITY_BOOSTER_SET_PRIORITY: {
      // do the work
      auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
      if (len < sizeof(ThreadData)) {
        status = STATUS_BUFFER_TOO_SMALL;
        break;
      }

      auto data = (ThreadData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
      if (data == nullptr) {
        status = STATUS_INVALID_PARAMETER;
        break;
      }

      if (data->Priority < 1 || data->Priority > 31) {
        status = STATUS_INVALID_PARAMETER;
        break;
      }

      PETHREAD Thread;
      status = PsLookupThreadByThreadId(ULongToHandle(data->ThreadId), &Thread);
      if (!NT_SUCCESS(status))
        break;

      KeSetPriorityThread((PKTHREAD)Thread, data->Priority);
      ObDereferenceObject(Thread);
      KdPrint(("Thread Priority change for %d to %d succeeded!\n",
data->ThreadId, data->Priority));
      break;
    }

    default:
      status = STATUS_INVALID_DEVICE_REQUEST;
      break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}
```

## Installing and Testing
using the sc.exe tool:
`$sc create booster type= kernel binPath= c:\Test\PriorityBooster.sys`

load the driver: `$sc start booster`

open WinObj and
look for device name and symbolic link:

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH4/4-1%20Symbolic%20Link%20in%20WinObj.png" width="60%" />

run the client: `$booster 768 25`

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH4/4-2%20Original%20thread%20priority.png" width="30%" />

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH4/4-3%20Modified%20thread%20priority.png" width="30%" />

















