# Chapter 8: Process and Thread Notifications

## Process Notifications
Whenever process is created or destroyed, interested drivers can be notified by kernel. 

This allows drivers to keep track of processes, possibly associating some data with these processes. 

At the very minimum, these allows drivers to monitor process creation/destruction in real-time. 

Notifications are sent **in-line**, as part of process creation.
Driver cannot miss any processes that may be created and destroyed quickly.

For process creations, drivers also have power to stop process from being created, returning error to caller initiating process creation. This type of power can only be achieved from kernel mode.

### Registering for process notifications
[PsSetCreateProcessNotifyRoutineEx](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetcreateprocessnotifyroutineex)
```C++
NTSTATUS
PsSetCreateProcessNotifyRoutineEx (
  _In_ PCREATE_PROCESS_NOTIFY_ROUTINE_EX NotifyRoutine,
  _In_ BOOLEAN Remove
);
```

First argument is driver’s callback routine.
```C++
typedef void(*PCREATE_PROCESS_NOTIFY_ROUTINE_EX) (
  _Inout_ PEPROCESS Process,
  _In_ HANDLE ProcessId,
  _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
);
```

Second argument indicates whether driver is registering or unregistering the callback. 

Typically, driver will call this API with `FALSE` in its `DriverEntry` routine and call the same API with `TRUE` in its unload
routine.

### Arguments to process notification routine
1. Process: process object of the newly created process or the process being destroyed.
2. Process Id: unique process ID of process. Although it’s declared with type `HANDLE`, it’s in fact an ID.
3. CreateInfo: contains detailed information on process being created. If process is being destroyed, this argument is `NULL`.

For process creation, driver’s callback routine is executed by creating thread. 

For process exit, the callback is executed by the last thread to exit the process. 

In both cases, the callback is called inside a critical region.

## Implementing Process Notifications
- To demonstrate process notifications, will build a driver that gathers information on process creation and destruction and allow this information to be consumed by user mode client. 
- Similar to Process Monitor which uses process/thread notifications for reporting process/thread activity. 
- Driver will store all process creation/destruction information in a linked list.
- Since this linked list may be accessed concurrently by multiple threads, need to protect it by fast mutex.
- The data will eventually find its way to user mode, so declare common
structures that the driver builds and user mode client receives. 

### All information structures defined
```C++
enum class ItemType : short {
  None,
  ProcessCreate,
  ProcessExit
};
struct ItemHeader {
  ItemType Type;
  USHORT Size;
  LARGE_INTEGER Time;
};
```

`ItemHeader` holds information common to all event types: type and time of event and size of payload. 

The size is important, as each event has its own information. 

If need to pack an array of these events and provide them to user mode client, the client needs to know where each event ends and the next one begins.

### Process exit:
```C++
struct ProcessExitInfo : ItemHeader {
  ULONG ProcessId;
};

struct ExitProcessInfo {
  ItemHeader Header;
  ULONG ProcessId;
};
```

Since need to store every structure as part of linked list, each data structure must contain `LIST_ENTRY` instance that points to the next and previous items. 

Since these `LIST_ENTRY` objects should not be exposed to user mode, will define extended structures containing these entries in a different file, that is not shared with user mode.

In SysMon.h, add a generic structure that holds `LIST_ENTRY` together with actual data structure:
```C++
template<typename T>
struct FullItem {
  LIST_ENTRY Entry;
  T Data;
};
```

A templated class is used to avoid creating a multitude of types, one for each specific event type.

For example, create structure specifically for a process exit event:
```C++
struct FullProcessExitInfo {
  LIST_ENTRY Entry;
  ProcessExitInfo Data;
};
```

The head of linked list must be stored somewhere. Create a data structure that will hold all global state of driver, instead of creating separate global variables. 

```C++
struct Globals {
  LIST_ENTRY ItemsHead;
  int ItemCount;
  FastMutex Mutex;
};
```
## Handling Process Exit Notifications
The process notification function in code above is `OnProcessNotify` and has the prototype outlined earlier. 

This callback handles process creations and exits. 

```C++
void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId,
  PPS_CREATE_NOTIFY_INFO CreateInfo) {
  if (CreateInfo) {
    // process create
  }
  else {
    // process exit
  }
}
```

Allocate storage for the full item representing this event:
```C++
auto info = (FullItem<ProcessExitInfo>*)ExAllocatePoolWithTag(PagedPool,
  sizeof(FullItem<ProcessExitInfo>), DRIVER_TAG);
if (info == nullptr) {
  KdPrint((DRIVER_PREFIX "failed allocation\n"));
  return;
}
```

If allocation fails, there is really nothing the driver can do, so it just returns from callback.

Next, driver limits the number of items in the linked list. This is a necessary precaution, as there is no guarantee that a client will consume these events promptly. 

The driver should never let data be consumed without limit. 

If the item count is above limit, the code removes the oldest item, essentially treating the linked list as a queue (`RemoveHeadList`). 

If the item is removed, its memory must be freed. The pointer to
the actual entry is not necessarily the pointer that was originally allocated (in this case it actually is because the `LIST_ENTRY` object is the first in `FullItem<>`), so `CONTAINING_RECORD` macro is used to get to the beginning of `FullItem<>` object. 

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH8/8-2%20FullItem%20layout.png" width="80%" />

Finally, the driver calls `InsertTailList` to add item to the end of the list and the item count is incremented.

## Handling Process Create Notifications
Process create notifications are more complex because the amount of information varies. 

```C++
struct ProcessCreateInfo : ItemHeader {
  ULONG ProcessId;
  ULONG ParentProcessId;
  USHORT CommandLineLength;
  USHORT CommandLineOffset;
};
```

Store command line length and its offset from the beginning. 
The actual characters of command line will follow this structure in memory. 

In this way, command line length is not limited and are not wasting memory for short command lines.

## Thread Notifications
The kernel provides thread creation and destruction callbacks, similarly to process callbacks. 

`PsSetCreateThreadNotifyRoutine` is for registration, and `PsRemoveCreateThreadNotifyRoutine` is unregistration. 

Arguments provided to the callback routine are process ID, thread ID and whether the thread is being created or destroyed.

## Image Load Notifications
Whenever an image (EXE, DLL, driver) file loads, the driver can receive a notification.

`PsSetLoadImageNotifyRoutineAPI` registers for these notifications, and `PsRemoveImageNotifyRoutine` is used for unregistering. 

```C++
typedef void (*PLOAD_IMAGE_NOTIFY_ROUTINE)(
  _In_opt_ PUNICODE_STRING FullImageName,
  _In_ HANDLE ProcessId, // pid into which image is being mapped
  _In_ PIMAGE_INFO ImageInfo);
```

`FullImageName` argument is somewhat tricky. As indicated by SAL annotation, it’s optional and can be NULL. 

Even if it’s not NULL, it doesn’t always produce the correct image file name.

ProcessId argument is the process ID into which the image is loaded. 
For drivers (kernel images), this value is zero.







