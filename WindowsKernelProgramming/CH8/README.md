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

Typically, driver will call
this API with `FALSE` in its `DriverEntry` routine and call the same API with `TRUE` in its unload
routine.

### Arguments to process notification routine
1. Process: process object of the newly created process or the process being destroyed.
2. Process Id: unique process ID of process. Although it’s declared with type `HANDLE`, it’s in fact an ID.
3. CreateInfo: contains detailed information on process being created. If process is being destroyed, this argument is `NULL`.

For process creation, driver’s callback routine is executed by creating thread. 

For process exit, the callback is executed by the last thread to exit the process. 

In both cases, the callback is called inside a critical region.

### structure provided for process creation 
```C++
typedef struct _PS_CREATE_NOTIFY_INFO {
  _In_ SIZE_T Size;
  union {
    _In_ ULONG Flags;
    struct {
      _In_ ULONG FileOpenNameAvailable : 1;
      _In_ ULONG IsSubsystemProcess : 1;
      _In_ ULONG Reserved : 30;
    };
  };
  _In_ HANDLE ParentProcessId;
  _In_ CLIENT_ID CreatingThreadId;
  _Inout_ struct _FILE_OBJECT *FileObject;
  _In_ PCUNICODE_STRING ImageFileName;
  _In_opt_ PCUNICODE_STRING CommandLine;
  _Inout_ NTSTATUS CreationStatus;
} PS_CREATE_NOTIFY_INFO, *PPS_CREATE_NOTIFY_INFO;
```

- CreatingThreadId: combination of thread and process Id of the caller to process creation function.
- ParentProcessId: parent process ID (not handle). This process may be the same provided by `CreateThreadId.UniqueProcess`, but may be different, as it’s possible as part of process creation to pass in different parent to inherit some properties from.
- ImageFileName: image file name of executable, available if the flag `FileOpenNameAvailable` is set.
- CommandLine: full command line used to create the process.
- IsSubsystemProcess: set if this process is a Pico process. This can only be if the driver registered with `PsSetCreateProcessNotifyRoutineEx2`.
- CreationStatus: would return to the caller. This is where the driver can stop the process from being created by placing some failure status.

## Implementing Process Notifications
- To demonstrate process notifications, will build a driver that gathers information on process
creation and destruction and allow this information to be consumed by user mode client. 
- Similar to Process Monitor which uses process/thread notifications for reporting process/thread activity. 
- Driver will store all process creation/destruction information in a linked list.
- Since this linked list may be accessed concurrently by multiple threads, need to protect it by
fast mutex.
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

Since these `LIST_ENTRY` objects
should not be exposed to user mode, will define extended structures containing these entries in
a different file, that is not shared with user mode.

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

## The DriverEntry Routine
```C++
Globals g_Globals;
extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING) {
  auto status = STATUS_SUCCESS;

  InitializeListHead(&g_Globals.ItemsHead);
  g_Globals.Mutex.Init();

  PDEVICE_OBJECT DeviceObject = nullptr;
  UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\sysmon");
  bool symLinkCreated = false;

  do {
    UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\sysmon");
    status = IoCreateDevice(DriverObject, 0, &devName,
      FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
    if (!NT_SUCCESS(status)) {
      KdPrint((DRIVER_PREFIX "failed to create device (0x%08X)\n", status));
      break;
    }
    DeviceObject->Flags |= DO_DIRECT_IO;

    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status)) {
      KdPrint((DRIVER_PREFIX "failed to create sym link (0x%08X)\n", status));
      break;
    }
    symLinkCreated = true;

    // register for process notifications
    status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
    if (!NT_SUCCESS(status)) {
      KdPrint((DRIVER_PREFIX "failed to register process callback (0x%08X)\n", status));
      break;
    }
  } while (false);

  if (!NT_SUCCESS(status)) {
    if (symLinkCreated)
      IoDeleteSymbolicLink(&symLink);
    if (DeviceObject)
      IoDeleteDevice(DeviceObject);
  }

  DriverObject->DriverUnload = SysMonUnload;
  DriverObject->MajorFunction[IRP_MJ_CREATE] =
  DriverObject->MajorFunction[IRP_MJ_CLOSE] = SysMonCreateClose;
  DriverObject->MajorFunction[IRP_MJ_READ] = SysMonRead;

  return status;
}
```

## Handling Process Exit Notifications
The process notification function in code above is `OnProcessNotify` and has the prototype
outlined earlier. 

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

Fill generic information: time, item type and size:
```C++
auto& item = info->Data;
KeQuerySystemTimePrecise(&item.Time);
item.Type = ItemType::ProcessExit;
item.ProcessId = HandleToULong(ProcessId);
item.Size = sizeof(ProcessExitInfo);

PushItem(&info->Entry);
```

```C++
void PushItem(LIST_ENTRY* entry) {
AutoLock<FastMutex> lock(g_Globals.Mutex);
  if (g_Globals.ItemCount > 1024) {
    // too many items, remove oldest one
    auto head = RemoveHeadList(&g_Globals.ItemsHead);
    g_Globals.ItemCount--;
    auto item = CONTAINING_RECORD(head, FullItem<ItemHeader>, Entry);
    ExFreePool(item);
  }
  InsertTailList(&g_Globals.ItemsHead, entry);
  g_Globals.ItemCount++;
}
```

The code first acquires fast mutex, as multiple threads may call this function at the same time.

Everything after that is done under the protection of the fast mutex.

Next, driver limits the number of items in the linked list. This is a necessary precaution, as there
is no guarantee that a client will consume these events promptly. 

The driver should never let data
be consumed without limit. 

The value 1024 chosen here is completely arbitrary. It’s better to have this number read from the registry in the driver’s service key.

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

```C++
USHORT allocSize = sizeof(FullItem<ProcessCreateInfo>);
USHORT commandLineSize = 0;
if (CreateInfo->CommandLine) {
  commandLineSize = CreateInfo->CommandLine->Length;
  allocSize += commandLineSize;
}
auto info = (FullItem<ProcessCreateInfo>*)ExAllocatePoolWithTag(PagedPool,allocSize, DRIVER_TAG);
if (info == nullptr) {
  KdPrint((DRIVER_PREFIX "failed allocation\n"));
  return;
}
```

The total size for allocation is based on the command line length. Fill in non-changing information, the header and the process and parent IDs:
```C++
auto& item = info->Data;
KeQuerySystemTimePrecise(&item.Time);
item.Type = ItemType::ProcessCreate;
item.Size = sizeof(ProcessCreateInfo) + commandLineSize;
item.ProcessId = HandleToULong(ProcessId);
item.ParentProcessId = HandleToULong(CreateInfo->ParentProcessId);
```

The item size must be calculated to include the base structure and the command line length.
Next, copy the command line to the address of the end of the base structure and update the length and offset:

```C++
if (commandLineSize > 0) {
  ::memcpy((UCHAR*)&item + sizeof(item), CreateInfo->CommandLine->Buffer,commandLineSize);
  item.CommandLineLength = commandLineSize / sizeof(WCHAR); // length in WCHARs
  item.CommandLineOffset = sizeof(item);
}
else {
  item.CommandLineLength = 0;
}
PushItem(&info->Entry);
```

## Providing Data to User Mode
In this driver, let client poll the driver for information using a read request. 

The driver will fill user-provided buffer with as many events as possible, until either the buffer is exhausted or there are no more events in the queue.

Start read request by obtaining the address of user’s buffer with Direct I/O (set up in `DriverEntry`):
```C++
NTSTATUS SysMonRead(PDEVICE_OBJECT, PIRP Irp) {
  auto stack = IoGetCurrentIrpStackLocation(Irp);
  auto len = stack->Parameters.Read.Length;
  auto status = STATUS_SUCCESS;
  auto count = 0;
  NT_ASSERT(Irp->MdlAddress); // we're using Direct I/O

  auto buffer = (UCHAR*)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
  if (!buffer) {
    status = STATUS_INSUFFICIENT_RESOURCES;
  }
  else {
    ...
```

Access linked list and pull items from its head:
```C++
AutoLock lock(g_Globals.Mutex); // C++ 17
while (true) {
  if (IsListEmpty(&g_Globals.ItemsHead)) // can also check g_Globals.ItemCount
    break;
  auto entry = RemoveHeadList(&g_Globals.ItemsHead);
  auto info = CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry);
  auto size = info->Data.Size;
  if (len < size) {
    // user's buffer is full, insert item back
    InsertHeadList(&g_Globals.ItemsHead, entry);
    break;
  }
  g_Globals.ItemCount--;
  ::memcpy(buffer, &info->Data, size);
  len -= size;
  buffer += size;
  count += size;
  // free data after copy
  ExFreePool(info);
}
```

First obtain the fast mutex, as process notifications can continue to arrive. 

If the list is empty, there is nothing to do and will break out of the loop. 

Pull the head item, and if it’s not
larger than the remaining user buffer size - copy its contents (`LIST_ENTRY` field). 

The loop continues pulling items from the head until either the list is empty or the user’s buffer is full.

Finally, complete the request with whatever the status is and set Information to the count variable:

```C++
Irp->IoStatus.Status = status;
Irp->IoStatus.Information = count;
IoCompleteRequest(Irp, 0);
return status;
```

If there are items in the linked list, they must be freed explicitly.

```C++
void SysMonUnload(PDRIVER_OBJECT DriverObject) {
  // unregister process notifications
  PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);

  UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\sysmon");
  IoDeleteSymbolicLink(&symLink);
  IoDeleteDevice(DriverObject->DeviceObject);

  // free remaining items
  while (!IsListEmpty(&g_Globals.ItemsHead)) {
    auto entry = RemoveHeadList(&g_Globals.ItemsHead);
    ExFreePool(CONTAINING_RECORD(entry, FullItem<ItemHeader>, Entry));
  }
}
```

## The User Mode Client
The main function calls `ReadFile` in a loop, sleeping a bit so that the thread is not always consuming
CPU. 

Once some data arrives, it’s sent for display purposes:

```C++
int main() {
  auto hFile = ::CreateFile(L"\\\\.\\SysMon", GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
  if (hFile == INVALID_HANDLE_VALUE)
    return Error("Failed to open file");

  BYTE buffer[1 << 16]; // 64KB buffer

  while (true) {
    DWORD bytes;
    if (!::ReadFile(hFile, buffer, sizeof(buffer), &bytes, nullptr))
      return Error("Failed to read");
    if (bytes != 0)
      DisplayInfo(buffer, bytes);
    ::Sleep(200);
  }
}
```

`DisplayInfo` function must make sense of the buffer it’s been given. 

Since all events start with
a common header, the function distinguishes various events based on `ItemType`. 

After the event has been dealt with, the Size field in the header indicates where the next event starts:
```C++
void DisplayInfo(BYTE* buffer, DWORD size) {
  auto count = size;
  while (count > 0) {
    auto header = (ItemHeader*)buffer;

    switch (header->Type) {
      case ItemType::ProcessExit:
      {
        DisplayTime(header->Time);
        auto info = (ProcessExitInfo*)buffer;
        printf("Process %d Exited\n", info->ProcessId);
        break;
      }
      case ItemType::ProcessCreate:
      {
        DisplayTime(header->Time);
        auto info = (ProcessCreateInfo*)buffer;
        std::wstring commandline((WCHAR*)(buffer + info->CommandLineOffset), info->CommandLineLength);
        printf("Process %d Created. Command line: %ws\n", info->ProcessId, commandline.c_str());
        break;
      }
      default:
        break;
    }
    buffer += header->Size;
    count -= header->Size;
  }
}
```

To extract the command line properly,  wstring can build a string based on pointer and string length. The `DisplayTime` formats
the time in human-readable way:
```C++
void DisplayTime(const LARGE_INTEGER& time) {
  SYSTEMTIME st;
  ::FileTimeToSystemTime((FILETIME*)&time, &st);
  printf("%02d:%02d:%02d.%03d: ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}
```

## Thread Notifications
The kernel provides thread creation and destruction callbacks, similarly to process callbacks. 

`PsSetCreateThreadNotifyRoutine` is for registration, and `PsRemoveCreateThreadNotifyRoutine` is unregistration. 

Arguments provided to the callback routine are process ID, thread ID and whether the thread is being created or destroyed.

Extend the existing SysMon driver to receive thread notifications as well as process notifications.
```C++
enum class ItemType : short {
  None,
  ProcessCreate,
  ProcessExit,
  ThreadCreate,
  ThreadExit
};

struct ThreadCreateExitInfo : ItemHeader {
  ULONG ThreadId;
  ULONG ProcessId;
};
```

Add proper registration to `DriverEntry` after registering for process notifications:
```C++
status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
if (!NT_SUCCESS(status)) {
  KdPrint((DRIVER_PREFIX "failed to set thread callbacks (status=%08X)\n", status)\);
  break;
}
```

Callback routine itself is rather simple, since the event structure has constant size. 
```C++
void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create) {
  auto size = sizeof(FullItem<ThreadCreateExitInfo>);
  auto info = (FullItem<ThreadCreateExitInfo>*)ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
  if (info == nullptr) {
    KdPrint((DRIVER_PREFIX "Failed to allocate memory\n"));
    return;
  }
  auto& item = info->Data;
  KeQuerySystemTimePrecise(&item.Time);
  item.Size = sizeof(item);
  item.Type = Create ? ItemType::ThreadCreate : ItemType::ThreadExit;
  item.ProcessId = HandleToULong(ProcessId);
  item.ThreadId = HandleToULong(ThreadId);

  PushItem(&info->Entry);
}
```

Add code to client that knows how to display thread creation and destruction (in `DisplayInfo`):
```C++
case ItemType::ThreadCreate:
{
  DisplayTime(header->Time);
  auto info = (ThreadCreateExitInfo*)buffer;
  printf("Thread %d Created in process %d\n", info->ThreadId, info->ProcessId);
  break;
}

case ItemType::ThreadExit:
{
  DisplayTime(header->Time);
  auto info = (ThreadCreateExitInfo*)buffer;
  printf("Thread %d Exited from process %d\n", info->ThreadId, info->ProcessId);
  break;
}
```

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
For drivers (kernel images),
this value is zero.

The ImageInfo argument contains additional information on the image, declared as follows:
```C++
#define IMAGE_ADDRESSING_MODE_32BIT 3

typedef struct _IMAGE_INFO {
  union {
    ULONG Properties;
    struct {
      ULONG ImageAddressingMode : 8; // Code addressing mode
      ULONG SystemModeImage : 1; // System mode image
      ULONG ImageMappedToAllPids : 1; // Image mapped into all processes
      ULONG ExtendedInfoPresent : 1; // IMAGE_INFO_EX available
      ULONG MachineTypeMismatch : 1; // Architecture type mismatch
      ULONG ImageSignatureLevel : 4; // Signature level
      ULONG ImageSignatureType : 3; // Signature type
      ULONG ImagePartialMap : 1; // Nonzero if entire image is not mapped
      ULONG Reserved : 12;
  };
};
PVOID ImageBase;
ULONG ImageSelector;
SIZE_T ImageSize;
ULONG ImageSectionNumber;
} IMAGE_INFO, *PIMAGE_INFO;
```

### Rundown
- SystemModeImage: set for kernel image, and unset for user mode image.
- ImageSignatureLevel: signing level. See `SE_SIGNING_LEVEL_` constants in WDK.
- ImageSignatureType: signature type. See `SE_IMAGE_SIGNATURE_TYPE` enumeration in WDK.
- ImageBase: virtual address into which the image is loaded.
- ImageSize: size of image.
- ExtendedInfoPresent: if this flag is set, then `IMAGE_INFO` is part of a larger structure, `IMAGE_-INFO_EX`.
```C++
typedef struct _IMAGE_INFO_EX {
  SIZE_T Size;
  IMAGE_INFO ImageInfo;
  struct _FILE_OBJECT *FileObject;
} IMAGE_INFO_EX, *PIMAGE_INFO_EX;
```

To access this larger structure,  driver uses `CONTAINING_RECORD`:
```C++
if (ImageInfo->ExtendedInfoPresent) {
  auto exinfo = CONTAINING_RECORD(ImageInfo, IMAGE_INFO_EX, ImageInfo);
  // access FileObject
}
```

The extended structure adds just one meaningful member - the file object used to manage the image.
The driver can add a reference to the object (ObReferenceObject) and use it in other functions as
needed.





