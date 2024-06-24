# Chapter 4: Driver from Start to Finish

## Introduction
Simple driver to fix inflexibility of setting thread priorities using the Windows API
- Thread priority: determined by combination of its process Priority Class (user mode)
- `SetPriorityClass`: change a process priority class
- `SetThreadPriority`: change a particular thread’s priority

Only six levels are available:

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH4/4-0%20Legal%20values%20for%20thread%20priorities%20with%20the%20Windows%20APIs.png" width="60%" />


## Driver Initialization
Need to do in `DriverEntry`(to take request):
- Set an Unload routine
- Set dispatch routines the driver supports
- Create a device object
- Create a symbolic link to the device object

### Step 1. 
`DriverEntry` with Unload routine:

### Step 2.
All drivers must
support `IRP_MJ_CREATE` and `IRP_MJ_CLOSE`, to open handle to
device.

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

## Passing Information to the Driver
Need to tell driver which thread and to what value to set priority

- User mode basic functions: `WriteFile`, `ReadFile` and `DeviceIoControl`
- sends control code directly to specified device driver, causing corresponding device to perform corresponding operation.
- is preferred to pass data to and from driver(flexible)
- important pieces: control code, input buffer, output buffer
- corresponds to `IRP_MJ_DEVICE_CONTROL` major function

### Add to initialization
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


### Create symbolic link and connect to device object:
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
- Every dispatch routine accepts target device object and an I/O Request Packet(IRP), which is more important.
- Driver’s purpose is to handle IRP, which means looking at the details of the request and doing what needs to be done to complete it.
- Every request to driver always wrapped in IRP, whether Create, Close, Read...
- IRP never arrives alone, accompanied by one or more structures of type `IO_STACK_LOCATION`. 
- [IO_STACK_LOCATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_io_stack_location): is an entry in the I/O stack that is associated with each IRP
- In case of Create and Close, just need to set status of IRP in its `IoStatus` member (of type `IO_STATUS_BLOCK`)

### [IoCompleteRequest](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iocompleterequest)
- Caller has completed all processing for a given I/O request and is returning the given IRP to the I/O manager
- Second argument is a temporary priority boost value that a driver can provide to its client. 
- `IO_NO_INCREMENT`(zero) is the best, because the request completed synchronously, no reason to get a priority boost.

## The DeviceIoControl Dispatch Routine
Actual work of setting a given thread to a requested priority.

### Check control code
- Typical drivers may support many control codes.
- Fail the request if control code is not recognized

### [IoGetCurrentIrpStackLocation](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iogetcurrentirpstacklocation) 
returns a pointer to caller's I/O stack location in specified IRP

### [PsLookupThreadByThreadId](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-pslookupthreadbythreadid)
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

















