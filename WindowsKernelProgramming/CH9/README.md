# Chapter 9: Object and Registry
## Object Notifications
The kernel provides a mechanism to notify interested drivers when attempts to open or duplicate a handle to certain object types(process and thread). 

The registration API is [ObRegisterCallbacks](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-obregistercallbacks):
```C++
NTSTATUS ObRegisterCallbacks (
  _In_ POB_CALLBACK_REGISTRATION CallbackRegistration,
  _Outptr_ PVOID *RegistrationHandle);
```

Before registration, [OB_CALLBACK_REGISTRATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_ob_callback_registration) must be initialized, which provides
necessary details about what the driver is registering for. 

`RegistrationHandle` is the return value upon a successful registration, which is just an opaque pointer used for unregistration by calling [ObUnRegisterCallbacks](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-obunregistercallbacks).

```C++
typedef struct _OB_CALLBACK_REGISTRATION {
  _In_ USHORT Version;
  _In_ USHORT OperationRegistrationCount;
  _In_ UNICODE_STRING Altitude;
  _In_ PVOID RegistrationContext;
  _In_ OB_OPERATION_REGISTRATION *OperationRegistration;
} OB_CALLBACK_REGISTRATION, *POB_CALLBACK_REGISTRATION;
```

- Version must be set to `OB_FLT_REGISTRATION_VERSION`.
- Number of operations that are being registered is specified by `OperationRegistrationCount`. This determines the number of `OB_OPERATION_REGISTRATION` that are pointed to by `OperationRegistration`, and provides information on process, thread or desktop.
- Altitude specifies a number (in string form) that affects the order of callbacks invocations for this driver. This is necessary because other drivers may have their own
callbacks and the question of which driver is invoked first is answered by altitude: the higher the earlier.
- The altitude provided must not collide with altitudes specified by previously registered drivers. The altitude does not have to be an integer number. In fact, it’s an infinite precision decimal number,
and this is why it’s specified as a string. 
- To avoid collision, the altitude should be set to something with random numbers after a decimal point, such as “12345.1762389”. 
The chances of collision in this
case are slim. The driver can even truly generate random digits to avoid collisions. 
- If registration fails with a status of `STATUS_FLT_INSTANCE_ALTITUDE_COLLISION`, this means altitude collision,
so the careful driver can adjust its altitude and try again.
- `RegistrationContext` is a driver defined value that is passed in as-is to callback routine(s).
- [OB_OPERATION_REGISTRATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_ob_operation_registration) is where the driver sets up its callbacks, determines which object types and operations are of interest. 

```C++
typedef struct _OB_OPERATION_REGISTRATION {
  _In_ POBJECT_TYPE *ObjectType;
  _In_ OB_OPERATION Operations;
  _In_ POB_PRE_OPERATION_CALLBACK PreOperation;
  _In_ POB_POST_OPERATION_CALLBACK PostOperation;
} OB_OPERATION_REGISTRATION, *POB_OPERATION_REGISTRATION;
```

- `ObjectType` is a pointer to instance registration type: process, thread or desktop. These pointers are exported as global kernel variables: `PsProcessType`, `PsThreadType` and `ExDesktopObjectType`.
- `Operations` is an bit flags enumeration selecting create/open `OB_OPERATION_HANDLE_CREATE` and/or duplicate `OB_OPERATION_HANDLE_DUPLICATE`.

- `OB_OPERATION_HANDLE_CREATE` refers to calls to user mode functions such as `CreateProcess`, `OpenProcess`, `CreateThread`, `OpenThread`, `CreateDesktop`, `OpenDesktop` ...
- `OB_OPERATION_HANDLE_DUPLICATE` refers to handle duplication for these objects.

Any time one of these calls is made from kernel, one or two callbacks
can be registered: a pre-operation callback `PreOperation` and a post-operation callback `PostOperation`.

## Pre-Operation Callback
Pre-operation callback is invoked before the actual create/open/duplicate operation completes,
giving chance to driver to make changes to operation’s result. 

Pre-operation callback
receives [OB_PRE_OPERATION_INFORMATION](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_ob_pre_operation_information):
```C++
typedef struct _OB_PRE_OPERATION_INFORMATION {
  _In_ OB_OPERATION Operation;
  union {
    _In_ ULONG Flags;
    struct {
      _In_ ULONG KernelHandle:1;
      _In_ ULONG Reserved:31;
    };
  };
  _In_ PVOID Object;
  _In_ POBJECT_TYPE ObjectType;
  _Out_ PVOID CallContext;
  _In_ POB_PRE_OPERATION_PARAMETERS Parameters;
} OB_PRE_OPERATION_INFORMATION, *POB_PRE_OPERATION_INFORMATION;
```

### Rundown
- Operation: indicates `OB_OPERATION_HANDLE_CREATE` or `OB_OPERATION_HANDLE_DUPLICATE`.
- KernelHandle: indicates this is a kernel handle. Kernel handles can only be created and used by kernel code. This allows driver to perhaps ignore kernel requests.
- Object: pointer to actual object for which a handle is being created/opened/duplicated. For processes, this is `EPROCESS` address, for thread it’s `PETHREAD` address.
- ObjectType: points to object type `*PsProcessType`, `*PsThreadType` or `*ExDesktopObjectType`.
- CallContext: a driver-defined value, is propagated to post-callback for this instance
(if exists).
- Parameters: a union specifying additional information based on `Operation`. 
```C++
typedef union _OB_PRE_OPERATION_PARAMETERS {
  _Inout_ OB_PRE_CREATE_HANDLE_INFORMATION CreateHandleInformation;
  _Inout_ OB_PRE_DUPLICATE_HANDLE_INFORMATION DuplicateHandleInformation;
} OB_PRE_OPERATION_PARAMETERS, *POB_PRE_OPERATION_PARAMETERS;
```

For Create operations, the
driver receives:
```C++
typedef struct _OB_PRE_CREATE_HANDLE_INFORMATION {
  _Inout_ ACCESS_MASK DesiredAccess;
  _In_ ACCESS_MASK OriginalDesiredAccess;
} OB_PRE_CREATE_HANDLE_INFORMATION, *POB_PRE_CREATE_HANDLE_INFORMATION;

HANDLE OpenHandleToProcess(DWORD pid) {
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if(!hProcess) {
    // failed to open a handle
  }
  return hProcess;
}
```

In this example, client tries to obtain a handle to a process with the specified access mask,
indicating what are its **intentions** towards that process.

The driver’s pre-operation callback receives this value in `OriginalDesiredAccess`. This value is also copied to `DesiredAccess`. 

Normally, kernel will determine, based on client’s security context and the process’s security descriptor whether client can be granted the access it desires.

The driver can, based on its own logic, modify `DesiredAccess` for example by removing some of the
access requested by client:
```C++
OB_PREOP_CALLBACK_STATUS OnPreOpenProcess(PVOID /* RegistrationContext */,
POB_PRE_OPERATION_INFORMATION Info) {

  if(/* some logic */) {
    Info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
  }
  return OB_PREOP_SUCCESS;
}
```

The above code snippet removes  `PROCESS_VM_READ` access mask before letting the operation continue normally. 

If it eventually succeeds, client will get back a valid handle, but only with `PROCESS_QUERY_INFORMATION` access mask.

```C++
typedef struct _OB_PRE_DUPLICATE_HANDLE_INFORMATION {
  _Inout_ ACCESS_MASK DesiredAccess;
  _In_ ACCESS_MASK OriginalDesiredAccess;
  _In_ PVOID SourceProcess;
  _In_ PVOID TargetProcess;
} OB_PRE_DUPLICATE_HANDLE_INFORMATION, *POB_PRE_DUPLICATE_HANDLE_INFORMATION;
```

`DesiredAccess` can be modified as before. The extra information provided are the source
process (from which a handle is being duplicated) and the target process (the process the new handle
will be duplicated into). 

This allows the driver to query various properties of these processes before making a decision on how to modify (if at all) the desired access mask.

## Post-Operation Callback
Post-operation callbacks are invoked after the operation completes. 

At this point the driver cannot
make any modifications, it can only look at the results.

```C++
typedef struct _OB_POST_OPERATION_INFORMATION {
  _In_ OB_OPERATION Operation;
  union {
    _In_ ULONG Flags;
    struct {
      _In_ ULONG KernelHandle:1;
      _In_ ULONG Reserved:31;
    };
  };
  _In_ PVOID Object;
  _In_ POBJECT_TYPE ObjectType;
  _In_ PVOID CallContext;
  _In_ NTSTATUS ReturnStatus;
  _In_ POB_POST_OPERATION_PARAMETERS Parameters;
} OB_POST_OPERATION_INFORMATION,*POB_POST_OPERATION_INFORMATION;
```

This looks similar to the pre-operation callback information, except:
- The final status returned in `ReturnStatus`. If successful, it means client will get back a valid handle (possibly with a reduced access mask).
- `Parameters` provided has just one piece of information: the access mask granted to client (assuming the status is successful).

## The Process Protector Driver
Process Protector driver is an example for using object callbacks.

Its purpose is to protect certain
processes from termination by denying `PROCESS_TERMINATE` access mask from any client that requests it for these **protected** processes.

The driver should keep a list of protected processes. 

In this driver, there is a limited array holding the process IDs under the driver’s protection. 
```C++
#define DRIVER_PREFIX "ProcessProtect: "
#define PROCESS_TERMINATE 1
#include "FastMutex.h"

const int MaxPids = 256;
struct Globals {
  int PidsCount; // currently protected process count
  ULONG Pids[MaxPids]; // protected PIDs
  FastMutex Lock;
  PVOID RegHandle; // object registration cookie
  void Init() {
    Lock.Init();
  }
};
```

## Object Notification Registration
`DriverEntry` routine for the process protector driver must include registration to object
callbacks for processes.

Structures for registration:
```C++
OB_OPERATION_REGISTRATION operations[] = {
  {
    PsProcessType, // object type
    OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE,
    OnPreOpenProcess, nullptr // pre, post
  }
};
  OB_CALLBACK_REGISTRATION reg = {
  OB_FLT_REGISTRATION_VERSION,
  1, // operation count
  RTL_CONSTANT_STRING(L"12345.6171"), // altitude
  nullptr, // context
  operations
};
```
The registration is for process objects only, with a pre-callback provided. 

This callback should remove
`PROCESS_TERMINATE` from the desired access requested by whatever client.

```C++
do {
  status = ObRegisterCallbacks(&reg, &g_Data.RegHandle);
  if (!NT_SUCCESS(status)) {
    break;
  }
...
```

## Managing Protected Processes
The driver maintains an array of process IDs for processes under its protection. 

The driver exposes three I/O control codes to allow adding and removing PIDs as well as clearing the entire list. 

ProcessProtectCommon.h:
```C++
#define PROCESS_PROTECT_NAME L"ProcessProtect"

#define IOCTL_PROCESS_PROTECT_BY_PID \
CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_PROCESS_UNPROTECT_BY_PID \
CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_PROCESS_PROTECT_CLEAR \
CTL_CODE(0x8000, 0x802, METHOD_NEITHER, FILE_ANY_ACCESS)
```

For protecting and unprotecting processes, the handler for `IRP_MJ_DEVICE_CONTROL` accepts an
array of PIDs (not necessarily just one). 
```C++
NTSTATUS ProcessProtectDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
  auto stack = IoGetCurrentIrpStackLocation(Irp);
  auto status = STATUS_SUCCESS;
  auto len = 0;

  switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_PROCESS_PROTECT_BY_PID:
      //...
      break;
    case IOCTL_PROCESS_UNPROTECT_BY_PID:
      //...
      break;
    case IOCTL_PROCESS_PROTECT_CLEAR:
      //...
      break;

    default:
      status = STATUS_INVALID_DEVICE_REQUEST;
      break;
  }

  // complete the request
  Irp->IoStatus.Status = status;
  Irp->IoStatus.Information = len;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return status;
}
```

To help with adding and removing PIDs, create two helper functions:
```C++
bool AddProcess(ULONG pid) {
  for(int i = 0; i < MaxPids; i++)
    if (g_Data.Pids[i] == 0) {
      // empty slot
      g_Data.Pids[i] = pid;
      g_Data.PidsCount++;
      return true;
    }
  return false;
}

bool RemoveProcess(ULONG pid) {
  for (int i = 0; i < MaxPids; i++)
    if (g_Data.Pids[i] == pid) {
      g_Data.Pids[i] = 0;
      g_Data.PidsCount--;
      return true;
    }
  return false;
}
```
Notice that fast mutex is not acquired in these functions, which means the caller must acquire
fast mutex before calling `AddProcess` or `RemoveProcess`.

Searches for process ID in the array and returns true if found:
```C++
bool FindProcess(ULONG pid) {
  for (int i = 0; i < MaxPids; i++)
    if (g_Data.Pids[i] == pid)
      return true;
  return false;
}
```

Find an empty **slot** in process ID array and store requested PID:
```C++
case IOCTL_PROCESS_PROTECT_BY_PID:
{
  auto size = stack->Parameters.DeviceIoControl.InputBufferLength;
  if (size % sizeof(ULONG) != 0) {
    status = STATUS_INVALID_BUFFER_SIZE;
    break;
  }
  
  auto data = (ULONG*)Irp->AssociatedIrp.SystemBuffer;

  AutoLock locker(g_Data.Lock);
  
  for (int i = 0; i < size / sizeof(ULONG); i++) {
    auto pid = data[i];
    if (pid == 0) {
      status = STATUS_INVALID_PARAMETER;
      break;
    }
    if (FindProcess(pid))
      continue;

    if (g_Data.PidsCount == MaxPids) {
      status = STATUS_TOO_MANY_CONTEXT_IDS;
      break;
    }

    if (!AddProcess(pid)) {
      status = STATUS_UNSUCCESSFUL;
      break;
    }

    len += sizeof(ULONG);
    }

  break;
  }
```

First checks the buffer size which must be a multiple of four bytes (PIDs) and not zero.

Next, the pointer to system buffer is retrieved (the control code uses `METHOD_BUFFERED`). 

Fast mutex is acquired and a loop begins.

The loop goes over all PIDs provided in the request and if all the following is true, adds the PID to
array:
- The PID is not zero (always an illegal PID, reserved for the Idle process).
- The PID is not already in the array (`FindProcess` determines that).
- The number of managed PIDs has not exceeded `MaxPids`.

Find PID and **remove** it by placing a zero in that slot (this
is a task for `RemoveProcess`):
```C++
case IOCTL_PROCESS_UNPROTECT_BY_PID:
{
  auto size = stack->Parameters.DeviceIoControl.InputBufferLength;
  if (size % sizeof(ULONG) != 0) {
    status = STATUS_INVALID_BUFFER_SIZE;
    break;
  }

  auto data = (ULONG*)Irp->AssociatedIrp.SystemBuffer;
  
  AutoLock locker(g_Data.Lock);

  for (int i = 0; i < size / sizeof(ULONG); i++) {
    auto pid = data[i];
    if (pid == 0) {
      status = STATUS_INVALID_PARAMETER;
      break;
    }
    if (!RemoveProcess(pid))
      continue;
    
    len += sizeof(ULONG);
    
    if (g_Data.PidsCount == 0)
      break;
  }
  
  break;
}
```

## The Pre-Callback
The most important piece is removing `PROCESS_TERMINATE` for PIDs that are
currently being protected from termination:
```C++
OB_PREOP_CALLBACK_STATUS
OnPreOpenProcess(PVOID, POB_PRE_OPERATION_INFORMATION Info) {
  if(Info->KernelHandle)
    return OB_PREOP_SUCCESS;

  auto process = (PEPROCESS)Info->Object;
  auto pid = HandleToULong(PsGetProcessId(process));
  AutoLock locker(g_Data.Lock);
  if (FindProcess(pid)) {
    // found in list, remove terminate access
    Info->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
  }

  return OB_PREOP_SUCCESS;
}
```

If the handle is a kernel handle, let the operation continue normally to not stop kernel code.

Getting callback PID is simple with `PsGetProcessId`. It accepts
`PEPROCESS` and returns its ID.

The last part is calling `FindProcess` under the protection of the lock. If found, remove `PROCESS_TERMINATE` access mask.

## The Client Application
The client application should be able to add, remove and clear processes by issuing correct
`DeviceIoControl` calls. 

Command line:
```bash
Protect.exe add 1200 2820 (protect PIDs 1200 and 2820)
Protect.exe remove 2820 (remove protection from PID 2820)
Protect.exe clear (remove all PIDs from protection)
```

Main:
```C++
int wmain(int argc, const wchar_t* argv[]) {

  if(argc < 2)
    return PrintUsage();

  enum class Options {
    Unknown,
    Add, Remove, Clear
  };
  Options option;
  if (::_wcsicmp(argv[1], L"add") == 0)
    option = Options::Add;
  else if (::_wcsicmp(argv[1], L"remove") == 0)
    option = Options::Remove;
  else if (::_wcsicmp(argv[1], L"clear") == 0)
    option = Options::Clear;
  else {
    printf("Unknown option.\n");
    return PrintUsage();
  }

  HANDLE hFile = ::CreateFile(L"\\\\.\\" PROCESS_PROTECT_NAME,GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
  if (hFile == INVALID_HANDLE_VALUE)
    return Error("Failed to open device");

  std::vector<DWORD> pids;
  BOOL success = FALSE;
  DWORD bytes;
  switch (option) {
    case Options::Add:
      pids = ParsePids(argv + 2, argc - 2);
      success = ::DeviceIoControl(hFile, 
        IOCTL_PROCESS_PROTECT_BY_PID,
        pids.data(), 
        static_cast<DWORD>(pids.size ()) * sizeof(DWORD), 
        nullptr, 
        0, 
        &bytes, 
        nullptr);
      break;
    case Options::Remove:
      pids = ParsePids(argv + 2, argc - 2);
      success = ::DeviceIoControl(hFile,
        IOCTL_PROCESS_UNPROTECT_BY_PI,
        pids.data(),
        static_cast<DWORD>(pids.size()) * sizeof(DWORD), 
        nullptr, 
        0, 
        &bytes, 
        nullptr);
      break;
    case Options::Clear:
      success = ::DeviceIoControl(hFile,
        IOCTL_PROCESS_PROTECT_CLEAR,
        nullptr, 
        0, 
        nullptr, 
        0, 
        &bytes, 
        nullptr);
      break;
  }

  if (!success)
    return Error("Failed in DeviceIoControl");
  
  printf("Operation succeeded.\n");
  
  ::CloseHandle(hFile);

  return 0;
}
```
`ParsePids` parses process IDs and returns them as `std::vector<DWORD>` that it’s easy to pass as an array by using the data() method on `std::vector<T>`:
```C++
std::vector<DWORD> ParsePids(const wchar_t* buffer[], int count) {
  std::vector<DWORD> pids;
  for (int i = 0; i < count; i++)
    pids.push_back(::_wtoi(buffer[i]));
  return pids;
}
```

## Registry Notifications
Somewhat similar to object notifications, Configuration Manager can be used to register for notifications when registry keys are accesses.

`CmRegisterCallbackEx` is used for registering to such notifications.
```C++
NTSTATUS CmRegisterCallbackEx (
  _In_ PEX_CALLBACK_FUNCTION Function,
  _In_ PCUNICODE_STRING Altitude,
  _In_ PVOID Driver, // PDRIVER_OBJECT
  _In_opt_ PVOID Context,
  _Out_ PLARGE_INTEGER Cookie,
  _Reserved_ PVOID Reserved
```
- Altitude is driver’s callback
altitude, which essentially has the same meaning as with object callbacks. 
- Driver argument should be driver object available in `DriverEntry`.
- Context is a driver-defined value passed as-is to callback. 
- Cookie is the result of registration if successful. This cookie should be passed to `CmUnregisterCallback` to unregister.

The callback function is fairly generic:
```C++
NTSTATUS RegistryCallback (
_In_ PVOID CallbackContext,
_In_opt_ PVOID Argument1,
_In_opt_ PVOID Argument2);
```

- `CallbackContext` is Context argument passed to `CmRegisterCallbackEx`. 
- The first generic argument is in fact an enumeration, `REG_NOTIFY_CLASS`, describing the operation for which the callback is invoked, and whether it’s pre or post notification. 
- The second argument is a pointer
to a specific structure relevant to this type of notification. 

A driver will typically switch on the
notification type:
```C++
NTSTATUS OnRegistryNotify(PVOID, PVOID Argument1, PVOID Argument2) {
  switch ((REG_NOTIFY_CLASS)(ULONG_PTR)Argument1) {
    //...
  }
```

## Handling Pre-Notifications
The callback is called for pre operations before these are carried out by Configuration Manager.

The driver has following options:
- Returning `STATUS_SUCCESS` from  callback instructs Configuration Manager to continue processing the operation normally.
- Return some failure status from the callback. In this case, Configuration Manager returns
to the caller with that status and the post operation will not be invoked.
- Handle the request in some way, and then return `STATUS_CALLBACK_BYPASS` from the callback. 
- Configuration Manager returns success to the caller and does not invoke the post operation. 
- The driver must take care to set proper values in `REG_xxx_KEY_INFORMATION` provided in the callback.

## Handling Post-Operations
After the operation is complete, and assuming the driver did not prevent the post operation from occurring, the callback is invoked after Configuration Manager performed the operation. 

The structure provided for post operations: 
```C++
typedef struct _REG_POST_OPERATION_INFORMATION {
  PVOID Object; // input
  NTSTATUS Status; // input
  PVOID PreInformation; // The pre information
  NTSTATUS ReturnStatus; // callback can change the outcome of the operation
  PVOID CallContext;
  PVOID ObjectContext;
  PVOID Reserved;
} REG_POST_OPERATION_INFORMATION,*PREG_POST_OPERATION_INFORMATION;
```
The callback has the following options for a post-operation:
- Look at the operation result and do something benign (log it, for instance).
- Modify the return status by setting a new status value in `ReturnStatus` of the post operation structure, and then returning `STATUS_CALLBACK_BYPASS`. Configuration Manager returns this new status to the caller.
- Modify the output parameters in `REG_xxx_KEY_INFORMATION` and return `STATUS_SUCCESS`. Configuration Manager returns this new data to the caller.

## Performance Considerations
The registry callback is invoked for every registry operation; there is no a-priori way to filter to
certain operations only. 

This means the callback needs to be as quick as possible since the caller is waiting. 

Also, there may be more than one driver in the chain of callbacks.

Some registry operations, especially read operations happen in large quantities, so it’s better for a driver to avoid processing read operations, if possible. 

If it must process read operations, it should al least limit its processing to certain keys of interest, such as anything under
`HKLM\System\CurrentControlSet`.

Write and create operations are used much less often, so in these cases the driver can do more if needed.

## Implementing Registry Notifications
Extended SysMon driver from chapter 8 to add notifications for writes to somewhere under `HKEY_LOCAL_MACHINE`.

Define the reported information:
```C++
struct RegistrySetValueInfo : ItemHeader {
  ULONG ProcessId;
  ULONG ThreadId;
  WCHAR KeyName[256]; // full key name
  WCHAR ValueName[64]; // value name
  ULONG DataType; // REG_xxx
  UCHAR Data[128]; // data
  ULONG DataSize; // size of data
};
```
Use fixed-size arrays for the reported information for simplicity. 

In a production-level driver,
it’s better to make this dynamic to save memory and provide complete information where needed.

The Data array is the actual written data. Naturally need to limit in some way, as it can be almost arbitrarily large.

`DataType` is one of `REG_xxx` type constants, such as `REG_SZ`, `REG_DWORD`, `REG_BINARY`, etc. These values are the same in user mode and kernel mode.

Add new event type:
```C++
enum class ItemType : short {
  None,
  ProcessCreate,
  ProcessExit,
  ThreadCreate,
  ThreadExit,
  ImageLoad,
  RegistrySetValue // new value
};
```

In `DriverEntry`, add registry callback registration as part of the do/while(false) block. The returned cookie representing the registration is stored in Globals structure:
```C++
UNICODE_STRING altitude = RTL_CONSTANT_STRING(L"7657.124");
status = CmRegisterCallbackEx(OnRegistryNotify, &altitude, DriverObject, nullptr, &g_Globals.RegCookie, nullptr);

if(!NT_SUCCESS(status)) {
  KdPrint((DRIVER_PREFIX "failed to set registry callback (%08X)\n",status));
  break;
}
```

Unload routine:
```C++
CmUnRegisterCallback(g_Globals.RegCookie);
```

## Handling Registry Callback
Switch on the operation of interest:
```C++
NTSTATUS OnRegistryNotify(PVOID context, PVOID arg1, PVOID arg2) {
UNREFERENCED_PARAMETER(context);
  switch ((REG_NOTIFY_CLASS)(ULONG_PTR)arg1) {
    case RegNtPostSetValueKey:
    //...
  }
  return STATUS_SUCCESS;
}
```

This driver don’t care about any other operation, so simply return
successful status after switch. 

Next, inside the caring case, cast the second argument to the post operation data and check if the operation succeeded:
```C++
auto args = (REG_POST_OPERATION_INFORMATION*)arg2;
if (!NT_SUCCESS(args->Status))
  break;
```

Next, need to check if the key in question is under `HKLM`. If not, just skip this key. 

The internal registry paths as viewed by the kernel always start with `\REGISTRY\` as the root. 

After that comes `MACHINE\` for the local machine hive: the same as `HKEY_LOCAL_MACHINE` in user mode code. 

This means need to check if the key in question starts with `\REGISTRY\MACHINE\`.

The key path is not stored in the post-structure and not even stored in the pre-structure directly.

Instead, the registry key object itself is provided as part of the post-information structure. 

Then extract the key name with `CmCallbackGetKeyObjectIDEx` and see if it’s starting with `\REGISTRY\MACHINE\`:
```C++
static const WCHAR machine[] = L"\\REGISTRY\\MACHINE\\";

PCUNICODE_STRING name;
if (NT_SUCCESS(CmCallbackGetKeyObjectIDEx(&g_Globals.RegCookie, args->Object,
nullptr, &name, 0))) {
  // filter out none-HKLM writes
  if (::wcsncmp(name->Buffer, machine, ARRAYSIZE(machine) - 1) == 0) {
```
If the condition holds, then capture the information of the operation into notification structure and push it onto the queue. 

That information (data type, value name, actual value, etc.) is
provided with the pre-information structure that is luckily available as part of the post-information
structure.

```C++
auto preInfo = (REG_SET_VALUE_KEY_INFORMATION*)args->PreInformation;
NT_ASSERT(preInfo);

auto size = sizeof(FullItem<RegistrySetValueInfo>);
auto info = (FullItem<RegistrySetValueInfo>*)ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG);
if (info == nullptr)
  break;

// zero out structure to make sure strings are null-terminated when copied
RtlZeroMemory(info, size);

// fill standard data
auto& item = info->Data;
KeQuerySystemTimePrecise(&item.Time);
item.Size = sizeof(item);
item.Type = ItemType::RegistrySetValue;

// get client PID/TID (this is our caller)
item.ProcessId = HandleToULong(PsGetCurrentProcessId());
item.ThreadId = HandleToULong(PsGetCurrentThreadId());

// get specific key/value data
::wcsncpy_s(item.KeyName, name->Buffer, name->Length / sizeof(WCHAR) - 1);
::wcsncpy_s(item.ValueName, preInfo->ValueName->Buffer, preInfo->ValueName->Length / sizeof(WCHAR) - 1);
item.DataType = preInfo->Type;
item.DataSize = preInfo->DataSize;
::memcpy(item.Data, preInfo->Data, min(item.DataSize, sizeof(item.Data)));
PushItem(&info->Entry);
```

The code is careful not to copy too much so as to overflow the statically-allocated buffers.

Finally, if `CmCallbackGetKeyObjectIDEx` succeeds, the resulting key name must be explicitly
freed:

```C++
CmCallbackReleaseKeyObjectIDEx(name);
```

## Modified Client Code
The client application must be modified to support this new event type. Here is one possible
implementation:
```C++
case ItemType::RegistrySetValue:
{
  DisplayTime(header->Time);
  auto info = (RegistrySetValueInfo*)buffer;
  printf("Registry write PID=%d: %ws\\%ws type: %d size: %d data: ",info->ProcessId, info->KeyName, info->ValueName, info->DataType, info->DataSize);

  switch (info->DataType) {
    case REG_DWORD:
      printf("0x%08X\n", *(DWORD*)info->Data);
      break;

    case REG_SZ:
    case REG_EXPAND_SZ:
      printf("%ws\n", (WCHAR*)info->Data);
      break;

    case REG_BINARY:
      DisplayBinary(info->Data, min(info->DataSize, sizeof(info->Data)));
      break;
    
    // add other cases... (REG_QWORD, REG_LINK, etc.)
    default:
      DisplayBinary(info->Data, min(info->DataSize, sizeof(info->Data)));
      break;
  }
  break;
}
```

`DisplayBinary` is a simple helper function that shows binary data as a series of hex values shown here for completeness:
```C++
void DisplayBinary(const UCHAR* buffer, DWORD size) {
  for (DWORD i = 0; i < size; i++)
    printf("%02X ", buffer[i]);
  printf("\n");
}
```
p240
