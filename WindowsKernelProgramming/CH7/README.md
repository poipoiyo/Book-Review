# Chapter 7: The I/O Request Packet
After a typical driver completes its initialization in `DriverEntry`, its primary job is to handle requests. 

These requests are packaged as the semi-documented I/O Request Packet (IRP) structure.

## Introduction to IRPs
- A structure allocated from non-paged pool typically one of the **managers** in Executive (I/O Manager, PnP Manager, Power Manager)
- Can also be allocated by driver
- Is responsible for freeing it
- Never allocated alone, always accompanied by one or more [IO_STACK_LOCATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_io_stack_location)
- While allocated, caller must specify how many I/O stack locations need to be allocated, and these stack locations follow IRP
directly in memory.
- The number of I/O stack locations is the number of device objects in device stack.
- When a driver receives an IRP, it gets a pointer to IRP structure itself, knowing it’s followed by a set of I/O stack location, one of it is for driver’s use. 
- Call `IoGetCurrentIrpStackLocation` to get correct I/O stack location

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-1%20IRP%20and%20its%20IO%20stack%20locations.png" width="80%" />

## Device Nodes
The I/O system in Windows is device-centric, rather than driver-centric.
- Device objects can be named and handles to device objects can be opened. `CreateFile` accepts symbolic link that lead to device object name. `CreateFile` cannot accept driver’s name as argument.
- Windows supports device layering
- One device can be layered on top of another. Request for lower device will first reach uppermost layer.

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-2%20Layered%20devices.png" width="80%" />  

Different device object that comprise device node (devnode) layers are named according to their role in devnode. 

### Rundown of meaning
- PDO (Physical Device Object): created by bus driver that is in charge of the particular bus (PCI, USB, ...). Represents the fact that there is some device on that bus.
- FDO (Functional Device Object): created by real driver, which provided by hardware’s vendor that understands the details of device
intimately.
- FiDO (Filter Device Object): optional filter devices created by filter drivers

### PnP manager
- In this case, responsible for loading appropriate drivers, starting from the bottom. 
- Suppose the devnode in figure represents a set of drivers that manage a PCI network card. 

Events to devnode creation:
1. PCI bus driver (pci.sys) creates a PDO (`IoCreateDevice`). The bus driver has no idea whether this a network card, video card or something else, only knows there is something there and can extract basic information from its controller, such as Vendor ID, Device ID 
2. PCI bus driver notifies P&P manager that it has changes on its bus.
3. P&P manager requests a list of PDOs managed by bus driver. It receives back a list of PDOs, in which this new PDO is included.
4. Now P&P manager’s job is to find and load proper driver for that new PDO. It issues a query to bus driver to request full hardware device ID.
5. With hardware ID in hand, P&P manager looks in registry at `HKLM\System\CurrentControlSet\Enum\PCI\`. If the driver has been loaded before, it will be registered there, and P&P manager will load it. 
6. The driver loads and creates the FDO, but adds an additional call to `IoAttachDeviceToDeviceStack`, thus attaching itself over the previous layer (typically PDO).

### Filter device objects
- Are loaded as well, if they are registered correctly in registry. 
- Lower filters (below FDO) load in order, from the bottom. 
- Each filter driver loaded creates its own device object and attached it on top of the previous layer.
-  Upper filters work the same way but are loaded after FDO. 
-  There are at least two layers: PDO and FDO, but there could be more if filters are involved.

- Lower filters(LowerFilters) are searched in two locations: hardware ID key and in corresponding class based on ClassGuid value listed under `HKLMSystemCurrentControlSetControlClasses`. 
- Upper filters are searched in a similar manner with value name UpperFilters. 

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-3%20The%20DiskDrive%20class%20key.png" width="80%" />

## IRP Flow
- The manager initializes main IRP structure and the first I/O stack location only. Then it passes the IRP’s pointer to the uppermost layer.
- A driver receives IRP in its appropriate dispatch routine. 
- For example, if this is a Read IRP, then the driver will be called in its `IRP_MJ_READ` index of its MajorFunction 

## Options when dealing IRP
### 1. Pass the request down 
- If driver’s device is not the last device in devnode, driver can
pass the request along if it’s not interesting for the driver. 
- Typically done by a filter driver that receives a request that it’s not interested in
- In order not to hurt functionality of the device (since the request is actually destined to a lower-layer device), the driver can pass it down.
- This must be done with two calls:
1. Call `IoSkipCurrentIrpStackLocation` to make sure next device in line is going to see the same information given to this device which should see the same I/O stack location.
2. Call `IoCallDriver` passing lower device object (which the driver received at the time it called `IoAttachDeviceToDeviceStack`) and the IRP.

### 2. Handle the IRP fully 
- The driver receiving IRP can just handle IRP without propagating it down by eventually calling `IoCompleteRequest`. 
- Any lower devices will never see the request.

### Do a combination of (1) and (2) 
- Driver can examine IRP, do something (log request), and pass it down. 
- Or it can make some changes to next I/O stack location, and pass request down.

### Pass request down and be notified when request completes by a lower layer device 
- Any layer (except the lowest one) can set up an I/O completion routine by calling `IoSetCompletionRoutine` before passing request down. 
- When one of lower layers completes the request, driver’s completion routine will be called.

### Start some asynchronous IRP handling 
- Driver may want to handle request, but if it is lengthy (typical hardware driver), driver may mark IRP as pending by calling `IoMarkIrpPending` and return `STATUS_PENDING` from its dispatch routine. 

Once some layer calls `IoCompleteRequest`, IRP turns around and starts “climbing” back towards the originator of IRP (typically on of the Managers). 

If completion routines have been registered, they will be invoked in reverse order of registration, such as from bottom to top.

## IPR and I/O Stack Location
<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-4%20Important%20fields%20of%20the%20IRP%20structure.png" width="80%" />

### Rundown
#### 1. IoStatus
contains `NT_STATUS` and an Information field. The Information field is polymorphic, typed as `ULONG_PTR`, but its meaning depends on the type of IRP. 
#### 2. UserBuffer
contains raw buffer pointer to user’s buffer for relevant IRPs. Read and Write IRPs, for instance, store it in this field.
#### 3. UserEvent
pointer to `KEVENT` that was provided by client if the call
is asynchronous and the event was supplied. From user mode, this event can be provided with HANDLE in a OVERLAPPED structure that is mandatory for invoking I/O operations asynchronously
#### 4. AssociatedIrp
holds three members, only one of which is valid:
- SystemBuffer: the most often used member. This points to a system-allocated non-paged pool buffer used for Buffered I/O operations. 
- MasterIrp: A pointer to a master IRP, if this IRP is an associated IRP. This idea is supported by I/O manager, where one IRP is a master that may have several **associated** IRPs. Once all associated IRPs complete, the master IRP is completed automatically. MasterIrp is valid for an associated IRP 
- IrpCount: for the master IRP itself, this field indicates the number of associated IRPs associated with this master IRP.
#### 5. Cancel Routine 
Pointer to a cancel routine that is invoked if the operation
is asked to be canceled, such as with the user mode functions `CancelIo` and `CancelIoEx`.
#### 6. MdlAddress
Points to an optional Memory Descriptor List (MDL). An MDL is a kernel data
structure that knows how to describe a buffer in RAM. MdlAddress is used primarily with Direct I/O.

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-5%20Important%20fields%20of%20the%20IO_STACK_LOCATION%20structure.png" width="80%" />

### Rundown
#### 1. MajorFunction
Major function of the IRP (`IRP_MJ_CREATE`, `IRP_MJ_READ` ...).
This field is sometimes useful if the driver points more than one major function code to the same handling routine. In that routine, the driver may want to distinguish the exact function using this field.
#### 2. MinorFunction
Some IRP types have minor functions. These are `IRP_MJ_PNP`, `IRP_MJ_POWER` and `IRP_MJ_SYSTEM_CONTROL` (WMI). Typical code for these handlers has a switch statement based on the MinorFunction. 
#### 3. FileObject 
`FILE_OBJECT` associated with this IRP. Not needed in most cases, but is available for dispatch routines that do.
#### 4. DeviceObject
Device object associated with this IRP. Dispatch routines receive a pointer
to this, so typically accessing this field is not needed.
#### 5. CompletionRoutine
Completion routine that is set for previous (upper) layer (set with `IoSetCompletionRoutine`).
#### 6.  Context
Argument to pass to the completion routine (if any).
#### 7. Parameters
This monstrous union contains multiple structures, each valid for a particular
operation. For example, in `IRP_MJ_READ` operation, the `Parameters.Read` structure field should be used to get more information about the Read operation.

## Viewing IRP Information
- While debugging or analyzing kernel dumps, a couple of commands may be useful for searching or examining IRPs.
- The `!irpfind` command can be used to find IRPs - either all IRPs, or IRPs that meet certain criteria.
- Using `!irpfind` without any arguments searches the non-paged pool(s) for all IRPs. 
- Command !irp examines the specific IRP, providing a nice overview of its
data. Command `dt` can be used with  _IRP type to look at entire IRP structure.

## Dispatch Routines
All dispatch routines have the same prototype, for convenience using  `DRIVER_-DISPATCH` typedef from WDK:

```C++
typedef NTSTATUS DRIVER_DISPATCH (
  _In_ PDEVICE_OBJECT DeviceObject,
  _Inout_ PIRP Irp);
```

The relevant dispatch routine is the first routine in a driver that
sees the request. 

Normally, it’s called by requesting thread context, e.g. ReadFile in IRQL PASSIVE_LEVEL (0). 

However, it’s possible that a filter driver sitting on top of this device sent the request down in a different context: it may be some other thread unrelated to the original requestor and even in higher IRQL.

Robust drivers need to be ready to deal with this kind of situation, even though for software drivers is rare.

### All dispatch routines follow a certain set of operations
1. Check for errors: For example, read and write operations contain buffers - do these buffers have appropriate sizes?
For DeviceIoControl, there is a control code in addition to potentially two buffers. The driver needs to make sure the control code is something it recognizes. If any error is identified, the IRP is completed immediately with appropriate status.
2. Handle the request appropriately.

### Most common dispatch routines 
1. `IRP_MJ_CREATE`: corresponds to `CreateFile` or `ZwCreateFile`. This major function is essentially mandatory, otherwise no client will be able to
open handle to device controlled. Most drivers just complete IRP with success status.
2. `IRP_MJ_CLOSE`: opposite of `IRP_MJ_CREATE`. Called by `CloseHandle` or `ZwClose` when the last handle to the file object is about to be closed.
Most drivers just complete request successfully, unless something meaningful was done in `IRP_MJ_CREATE`.
3. `IRP_MJ_READ`: corresponds to read operation, typically invoked by `ReadFile` or `ZwReadFile`.
4. `IRP_MJ_WRITE`: corresponds to write operation, typically invoked by
`WriteFile` or `ZwWriteFile`.
5. `IRP_MJ_DEVICE_CONTROL`: corresponds to `DeviceIoControl` or `ZwDeviceIoControlFile`.
6. `IRP_MJ_INTERNAL_DEVICE_CONTROL`: similar to `IRP_MJ_DEVICE_CONTROL`, but only available for kernel callers.

## Completing a Request
- Once driver decides to handle an IRP (not passing to another driver), it must
eventually complete it.
- Otherwise, the requesting thread cannot really terminate and by extension its containing process will linger on as well, resulting in a “zombie process”.
- Completing request means calling `IoCompleteRequest` after filling-in the request status and extra information. 
- If the completion is done in the dispatch routine itself, the routine must return the same status that was placed in the IRP.

```C++
NTSTATUS MyDispatchRoutine(PDEVICE_OBJECT, PIRP Irp) {
//...
  Irp->IoStatus.Status = STATUS_XXX;
  Irp->IoStatus.Information = NumberOfBytesTransfered; // depends on request type

  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return STATUS_XXX;
}
```

`IoCompleteRequest` accepts two arguments: IRP itself and an optional value to temporarily increment the original thread’s priority (the thread that initiated the request in the first place). 

In most cases, the thread in question is the executing thread, so a thread boost is
inappropriate. 
The value `IO_NO_INCREMENT` as defined as zero, so no increment in the above code
snippet.
However, the driver may choose to give the thread a boost, regardless whether it’s the calling thread or not.

In this case, the thread’s priority jumps with the given boost, and then it’s allowed to execute one quantum with that new priority before the priority decreases by one, it can then get another quantum with the reduced priority, and so on, until its priority returns to its original level. 

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-6%20Thread%20priority%20boost%20and%20decay.png" width="80%" />

## Accessing User Buffers
A given dispatch routine is the first to see IRP. 

Some dispatch routines, mainly `IRP_MJ_READ`, `IRP_MJ_WRITE` and `IRP_MJ_DEVICE_CONTROL` accept buffers provided by user mode. 

Typically, a dispatch routine is called in IRQL 0 and in the requesting thread context, which means buffers pointers provided by user mode are trivially accessible: IRQL is 0 so page faults are handled normally, and the thread is the requestor, so pointers are valid in this process context.

It’s possible for another thread in client’s process to free the passed-in buffer(s), before the driver gets a chance to examine them, and so cause an access violation.

If code is running at IRQL 2, it cannot safely access user’s buffers at this context. 

### Issues
1. IRQL is 2, meaning no page fault handling can occur.
2. The thread executing DPC is an arbitrary one, so pointer itself has no meaning in whatever process happens to be the current on this processor.

Using exception handling in such a case will not work correctly, because access some memory location that is essentially invalid in this random process context.

Even if the access succeeds, it will be accessing random memory, and certainly not the original buffer provided to request.

All this means that there must be some way to access the original user’s buffer in an inconvenient context. 

In fact, there are two ways provided by the kernel for this purpose, called Buffered I/O and Direct I/O. 

## Buffered I/O
Buffered I/O is the simplest of the two ways. 
To get support for Buffered I/O for Read and Write operations:

```C++
DeviceObject->Flags |= DO_BUFFERED_IO; // DO = Device Object
```

### Steps
1. I/O Manager allocates a buffer from non-paged pool with the same size as user’s buffer. It stores the pointer to this new buffer in  `AssociatedIrp->SystemBuffer` member
of the IRP. 
2. For a write request, I/O Manager copies user’s buffer to system buffer.
3. Now driver’s dispatch routine is called. Driver can use system buffer pointer directly without any checks, because the buffer is in system space, and in any IRQL, because the buffer is allocated from
non-paged pool, so it cannot be paged out.
4. Once driver completes IRP, I/O manager copies system buffer back to user’s buffer.
5. Finally, I/O Manager frees system buffer.

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-8a%20Buffered%20IO%20initial%20state.png" width="80%" />

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-8b%20Buffered%20IO%20system%20buffer%20allocated.png" width="80%" />

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-8c%20Buffered%20IO%20driver%20accesses%20system%20buffer.png" width="80%" />

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-8d%20Buffered%20IO%20on%20IRP%20completion.png" width="80%" />

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-8e%20Buffered%20IO%20final%20state.png" width="80%" />

Buffered I/O characteristics:
- Easy to use: just specify the flag in device object and everything else is taken care by
I/O Manager.
- Always involves a copy: best used for small buffers. Large buffers may be expensive to copy. In this case, the other option, Direct I/O, should be used instead.

## Direct I/O
Purpose is to allow access to user’s buffer in any IRQL and any thread but without any copying going around.
For read and write requests, selecting Direct I/O is done with different flag of the device object:
```C++
DeviceObject->Flags |= DO_DIRECT_IO;
```

### Steps
1. I/O Manager first makes sure user’s buffer is valid and faults it into physical memory.
2. It locks the buffer in memory, so it cannot be paged out until further notice. This solves
page faults cannot happen, so accessing buffer in any IRQL is safe.
3. I/O Manager builds a Memory Descriptor List (MDL) which knows how buffer mapped to RAM. Address of MDL is stored in MdlAddress field of the IRP.
4. Driver gets call to its dispatch routine. User’s buffer is locked in RAM, cannot be accessed from an arbitrary thread. When driver requires access to the buffer, it must call a function that maps the same user buffer to a system
address, which by definition is valid in any process context. There are two mappings to the same buffer. One is from the original address (valid only in the context of the requestor process) and the other in system space, which is always valid. `MmGetSystemAddressForMdlSafe`, passes MDL built by I/O Manager,  and return value is system address.
5. Once driver completes the request, I/O Manager removes the second mapping (to system space), frees MDL and unlocks user’s buffer, so it can be paged normally just like any other user mode memory

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-9a%20Direct%20IO%20initial%20state.png" width="80%" />

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-9b%20IO%20manager%20faults%20buffer%E2%80%99s%20pages%20to%20RAM%20and%20locks%20them.png" width="80%" />

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-9c%20the%20MDL%20describing%20the%20buffer%20is%20stored%20in%20the%20IRP.png" width="80%" />

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-9d%20the%20driver%20double-maps%20the%20buffer%20to%20a%20system%20address.png" width="80%" />

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-9e%20the%20driver%20accesses%20the%20buffer%20using%20the%20system%20address.png" width="80%" />

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-9f%20when%20the%20IRP%20is%20completed.png" width="80%" />

- There is no copying at all. Driver just reads/writes to user’s buffer directly by using system address.
- `MmGetSystemAddressForMdlSafe` accepts MDL and a page priority `MM_PAGE_PRIORITY`. 
- Most drivers specify `NormalPagePriority`, but there is also `LowPagePriority` and
`HighPagePriority`. 
- This priority gives hint to the system of the importance of the mapping.
- If `MmGetSystemAddressForMdlSafe` fails, it returns NULL. This means the system is out of system page tables or very low on system page tables. This should
be a rare occurrence, but can happen in very low memory conditions. 
- If NULL is returned, the driver should complete the IRP with the status `STATUS_INSUFFICIENT_RESOURCES`.

## User Buffers for IRP_MJ_DEVICE_CONTROL
For `IRP_MJ_DEVICE_CONTROL`, the buffering access method is supplied on a control code basis. 
```C++
BOOL DeviceIoControl(
  HANDLE hDevice, // handle to device or file
  DWORD dwIoControlCode, // IOCTL code (see <winioctl.h>)
  PVOID lpInBuffer, // input buffer
  DWORD nInBufferSize, // size of input buffer
  PVOID lpOutBuffer, // output buffer
  DWORD nOutBufferSize, // size of output buffer
  PDWORD lpdwBytesReturned, // # of bytes actually returned
  LPOVERLAPPED lpOverlapped); // for async. operation
```

There are three arguments here:
1. I/O control code
2. optional two buffers designated **input** and **output**. 

The way these buffers are accessed depends on the control code.
```
#define CTL_CODE( DeviceType, Function, Method, Access ) ( \
((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
```

### Options to access input output buffers
#### METHOD_NEITHER
- No help is required of I/O manager, so the driver is left dealing with the buffers on its own. 
- For instance, if the code does not require any buffer, the control code itself is all the information needed, it’s best to let I/O manager know that it does not need to do any additional work.
- In this case, the pointer to user’s input buffer is store in current I/O stack location’s `Paramaters.DeviceIoControl.Type3InputBuffer` field and the output buffer is stored in IRP’s UserBuffer field.

#### METHOD_BUFFERED 
- Indicates Buffered I/O for both input and output buffer. 
- When the request starts, I/O manager allocates system buffer from non-paged pool with the size that is the maximum of the lengths of input and output buffers. 
- It then copies input buffer to system buffer. Only now `IRP_MJ_DEVICE_CONTROL` dispatch routine is invoked. When the request completes, I/O manager copies the number of bytes indicated
with `IoStatus.Information` in IRP to user’s output buffer.
– The system buffer pointer is at the usual location: `AssociatedIrp.SystemBuffer` inside IRP structure.

#### METHOD_IN_DIRECT and METHOD_OUT_DIRECT 
- The only difference between these two values is whether the output buffer can be read `METHOD_IN_DIRECT` or written `METHOD_OUT_DIRECT`.

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH7/7-10%20Buffering%20method%20based%20on%20control%20code%20Method%20argument.png" width="80%" />

## Putting it All Together: The Zero Driver
The driver is named `Zero` and has the following characteristics:
- For read requests, it zeros out the provided buffer.
- For write requests, it just consumes the provided buffer, similar to a classic null device.

The driver will use Direct I/O so as not to incur the overhead of copies, as the buffers provided by client can potentially be very large.

## Using a Precompiled Header
- Add a new header file to the project and call it `pch.h`. This file will serve as the precompiled
header. Add all rarely-changing `#includes` here:
```C++
#pragma once
#include <ntddk.h>
#include "pch.h"
```

## The DriverEntry Routine
```C++
#define DRIVER_PREFIX "Zero: "
// DriverEntry
extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
  UNREFERENCED_PARAMETER(RegistryPath);
  DriverObject->DriverUnload = ZeroUnload;
  DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_\
CLOSE] = ZeroCreateClose;
  DriverObject->MajorFunction[IRP_MJ_READ] = ZeroRead;
  DriverObject->MajorFunction[IRP_MJ_WRITE] = ZeroWrite;
  UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\Zero");
  
  UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\Zero");
  PDEVICE_OBJECT DeviceObject = nullptr;
  auto status = STATUS_SUCCESS;

  do {
    status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
    if (!NT_SUCCESS(status)) {
      KdPrint((DRIVER_PREFIX "failed to create device (0x%08X)\n", status));
      break;
    }
    // set up Direct I/O
    DeviceObject->Flags |= DO_DIRECT_IO;

    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status)) {
      KdPrint((DRIVER_PREFIX "failed to create symbolic link (0x%08X)\n", status));
      break;
    }
  } while (false);

  if (!NT_SUCCESS(status)) {
    if (DeviceObject)
      IoDeleteDevice(DeviceObject);
  }
  return status;
}
```

The pattern is simple: if an error occurs in any call, just break out of the loop. 

Outside the loop, check the status, and if it’s a failure, undo any operations done so far.

With this scheme in hand, it’s easy to add more initializations, while keeping cleanup code localized and appearing just once.

## The Read Dispatch Routine
```C++
NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0) {
  Irp->IoStatus.Status = status;
  Irp->IoStatus.Information = info;
  IoCompleteRequest(Irp, 0);
  return status;
}
```
```C++
NTSTATUS ZeroRead(PDEVICE_OBJECT, PIRP Irp) {
  auto stack = IoGetCurrentIrpStackLocation(Irp);
  auto len = stack->Parameters.Read.Length;
  if (len == 0)
    return CompleteIrp(Irp, STATUS_INVALID_BUFFER_SIZE);
  auto buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
  if (!buffer)
    return CompleteIrp(Irp, STATUS_INSUFFICIENT_RESOURCES);
  memset(buffer, 0, len);

  return CompleteIrp(Irp, STATUS_SUCCESS, len);
}
```

It’s important to set `Information` field to the length of the buffer. This indicates to the client
the number of bytes consumed in the operation (in the second to last argument to `ReadFile`). 

## The Write Dispatch Routine
All it needs to do is just complete the request with the buffer length provided by client (essentially swallowing the buffer):
```C++
NTSTATUS ZeroWrite(PDEVICE_OBJECT, PIRP Irp) {
  auto stack = IoGetCurrentIrpStackLocation(Irp);
  auto len = stack->Parameters.Write.Length;
  return CompleteIrp(Irp, STATUS_SUCCESS, len);
}
```
Don’t even calling `MmGetSystemAddressForMdlSafe`, as don’t need to access the actual buffer. 

This is also the reason this call is not made beforehand by I/O manager: the driver may not even need it, or perhaps need it in certain conditions only; so I/O manager prepares everything (the MDL) and lets the driver decide when and if to do the actual mapping.

