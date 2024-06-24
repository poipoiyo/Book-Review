# Chapter 10: Introduction to File System Mini-Filters

File systems are targets for I/O operations to access files. 

Windows supports several file systems, most notably NTFS, its native file system. 

File system filtering is the mechanism by which drivers can intercept calls destined to the file system. 

This is useful for many type of software, such as anti-viruses, backups, encryption and many more.

Windows supported for a long time a filtering model known as file system filters, which is now referred to as file system legacy filters. 

A newer model called file system mini-filters was developed to replace the legacy filter mechanism. 

Mini-filters are easier to write in many respects, and are the preferred way to develop file system filtering drivers.

## Introduction
Legacy file system filters are notoriously difficult to write.
The driver writer has to take care of an assortment of little details. 

Legacy filters cannot be unloaded while the system is running which means the system had to be restarted to load an update version of the driver. 

With the mini-filter model, drivers can be loaded and unloaded dynamically.

Internally, legacy filter provided by Windows called the Filter Manager is tasked with managing mini-filters. 

Typical filter layering:
<img src="" width="80%" />  

Each mini-filter has its own Altitude, which determines its relative position in the device stack. 

Filter manager is the one receiving IRPs just like any other legacy filter and then calls upon the
mini-filters it’s managing, in descending order of altitude.

In some unusual cases, there may be another legacy filter in the hierarchy, that may cause a minifilter **split**, where some are higher in altitude than the legacy filter and some lower. 

In such a case, more than one instance of the filter manager will load, each managing its own mini-filters. 

Every such filter manager instance is referred to as Frame.
<img src="" width="80%" /> 

## Loading and Unloading
Mini-filter drivers must be loaded just like any other driver. 

User mode API to use is [FilterLoad](https://learn.microsoft.com/en-us/windows/win32/api/fltuser/nf-fltuser-filterload), passing driver’s name (its key in the registry `atHKLM\System\CurrentControlSet\Services\drivername`).

Internally, [FltLoadFilter](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/fltkernel/nf-fltkernel-fltloadfilter) is invoked with the same semantics. 

Just like any other driver, `SeLoadDriverPrivilege` must be present in caller’s token if called from user mode.

By default, it’s present in admin-level tokens, but not in standard users tokens.

Unloading a mini-filter is accomplished with `FilterUnload` in user mode, or `FltUnloadFilter` in kernel mode. 

This operation requires the same privilege as for loads, but is not guaranteed to succeed, because mini-filter’s Filter unload callback is called, which can fail the request so that driver remains in the system.

Although using APIs to load and unload filters has its uses, during development it’s usually easier to
use a built-in tool that can accomplish that called fltmc.exe. 

Invoking it without arguments lists the currently loaded mini-filters. 

```cmd
C:\WINDOWS\system32>fltmc
Filter Name Num Instances Altitude Frame
------------------------------ ------------- ------------ -----
bindflt 1 409800 0
FsDepends 9 407000 0
WdFilter 10 328010 0
storqosflt 1 244000 0
wcifs 3 189900 0
PrjFlt 1 189800 0
CldFlt 2 180451 0
FileCrypt 0 141100 0
luafv 1 135000 0
npsvctrig 1 46000 0
Wof 8 40700 0
FileInfo 10 40500 0
```

For each filter, output shows driver’s name, number of instances each filter has currently
running, its altitude and the filter manager frame it’s part of.

The short answer for drivers with different number of instances
is that it’s up to the driver to decide whether to attach to a given volume or not.

Loading driver with fltmc.exe is done with the load option, like so: `fltmc load myfilter`

Unloading is done with the unload command line option: `fltmc unload myfilter`

File system drivers and filters are created in FileSystem directory of  Object Manager namespace. 
<img src="" width="80%" /> 

## Initialization
A file system mini-filter driver has `DriverEntry` routine, just like any other driver. 

The driver must register itself as a mini-filter with filter manager, specifying various settings, such as
what operations it wishes to intercept. 

The driver sets up appropriate structures and then calls
[FltRegisterFilter](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/fltkernel/nf-fltkernel-fltregisterfilter) to register. 

If successful, the driver can do further initializations as needed
and finally call [FltStartFiltering](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/fltkernel/nf-fltkernel-fltstartfiltering) to actually start filtering operations. 

Note that the driver does not need to set up dispatch routines on its own (`IRP_MJ_READ`, `IRP_MJ_WRITE`, etc.). 

This is because the driver is not directly in the I/O path; the filter manager is.

```C++
NTSTATUS FltRegisterFilter (
  _In_ PDRIVER_OBJECT Driver,
  _In_ const FLT_REGISTRATION *Registration,
  _Outptr_ PFLT_FILTER *RetFilte);
```

```C++
typedef struct _FLT_REGISTRATION {
  USHORT Size;
  USHORT Version;
  
  FLT_REGISTRATION_FLAGS Flags;
  
  const FLT_CONTEXT_REGISTRATION *ContextRegistration;
  const FLT_OPERATION_REGISTRATION *OperationRegistration;
  
  PFLT_FILTER_UNLOAD_CALLBACK FilterUnloadCallback;
  PFLT_INSTANCE_SETUP_CALLBACK InstanceSetupCallback;
  PFLT_INSTANCE_QUERY_TEARDOWN_CALLBACK InstanceQueryTeardownCallback;
  PFLT_INSTANCE_TEARDOWN_CALLBACK InstanceTeardownStartCallback;
  PFLT_INSTANCE_TEARDOWN_CALLBACK InstanceTeardownCompleteCallback;
  
  PFLT_GENERATE_FILE_NAME GenerateFileNameCallback;
  PFLT_NORMALIZE_NAME_COMPONENT NormalizeNameComponentCallback;
  PFLT_NORMALIZE_CONTEXT_CLEANUP NormalizeContextCleanupCallback;
  
  PFLT_TRANSACTION_NOTIFICATION_CALLBACK TransactionNotificationCallback;
  PFLT_NORMALIZE_NAME_COMPONENT_EX NormalizeNameComponentExCallback;

#if FLT_MGR_WIN8
  PFLT_SECTION_CONFLICT_NOTIFICATION_CALLBACK SectionNotificationCallback;
#endif
} FLT_REGISTRATION, *PFLT_REGISTRATION;
```

### Important information
- Size must be set to the size of the structure, which may depend on the target Windows version
(set in the project’s properties). Drivers typically just specify sizeof(FLT_REGISTRATION).
- Version is also based on the target Windows version. Drivers use `FLT_REGISTRATION_VERSION`.
- Flags can be zero or a combination of the following values:
1. `FLTFL_REGISTRATION_DO_NOT_SUPPORT_SERVICE_STOP`: the driver does not support a stop request, regardless of other settings.
2. `FLTFL_REGISTRATION_SUPPORT_NPFS_MSFS`: the driver is aware of named pipes and mailslots and wishes to filter requests to these file systems as well.
3. `FLTFL_REGISTRATION_SUPPORT_DAX_VOLUME`: the driver will support attaching to Direct Access Volume (DAX), if volume is available.
- `OperationRegistration`: by far the most important field. This is a pointer to an array of
`FLT_OPERATION_REGISTRATION` structures, each specifying interest operation and pre/post callback the driver wishes to be called upon.
- `FilterUnloadCallback`: specifies a function to be called when the driver is about to be unloaded. If NULL is specified, the driver cannot be unloaded. If the driver sets a callback and returns a successful status, the driver is unloaded; in that case the driver must call `FltUnregisterFilter` to unregister itself before being unloaded. Returning a non-success status does not unload the driver.
- `InstanceSetupCallback`: this callback allows the driver to be notified when an instance is about
to be attached to a new volume. The driver may return `STATUS_SUCCESS` to attach or `STATUS_FLT_DO_NOT_ATTACH` if the driver does not wish to attach to this volume.
- `InstanceQueryTeardownCallback`: an optional callback invoked before detaching from a volume. This can happen because of an explicit request to detach using `FltDetachVolume` in kernel mode or `FilterDetach` in user mode. If NULL is specified by the callback, the detach operation is aborted.
- `InstanceTeardownStartCallback`: an optional callback invoked when teardown of an instance has started. The driver should complete any pended operations so that instance teardown can complete. Specifying NULL for this callback does not prevent instance teardown (prevention can be achieved with the previous query teardown callback).
- `InstanceTeardownCompleteCallback`: an optional callback invoked after all the pending I/O operations complete or canceled.

### Pipes and Mailslots
Named pipes is a uni- or bi-directional communication mechanism from a server to one or more clients, implemented as a file system (npfs.sys). 

`CreateNamedPipe` can be used to create a named pipe server, to which clients can connect using  `CreateFile` with **file name** in this form: `\\<server>\pipe\<pipename>`.

Mailslots is a uni-directional communication mechanism, implemented as a file system (msfs.sys), where a server process opens a mailslot, to which messages can
be sent by clients. 

`CreateMailslot` creates the mailslot and clients connect with CreateFile with a file name in the form `\\<server>\mailslot\<mailslotname>`.

### Direct Access Volume (DAX or DAS)
Provides support for a new kind of storage based on direct access to the underlying byte data. 

This is supported by new type of storage hardware referred to as Storage Class Memory: a non-volatile storage medium with RAM-like performance. 

## Operations Callback Registration
A mini-filter driver must indicate which operations it’s interested in.

This is provided at mini-filter
registration time with an array of `FLT_OPERATION_REGISTRATION`:
```C++
typedef struct _FLT_OPERATION_REGISTRATION {
  UCHAR MajorFunction;
  FLT_OPERATION_REGISTRATION_FLAGS Flags;
  PFLT_PRE_OPERATION_CALLBACK PreOperation;
  PFLT_POST_OPERATION_CALLBACK PostOperation;

  PVOID Reserved1; // reserved
} FLT_OPERATION_REGISTRATION, *PFLT_OPERATION_REGISTRATION;
```

The operation itself is identified by a major function code: `IRP_MJ_CREATE`, `IRP_MJ_READ`, `IRP_MJ_WRITE` and so on. 

However, there are other operations identified with a major function that do not have a real major function dispatch routine.

This abstraction provided by the filter manager helps to isolate the mini-filter from knowing the exact source of the operation: it could be a real IRP or it could be another
operation that is abstracted as an IRP. 

Furthermore, file systems support yet another mechanism for receiving requests, known as Fast I/O. 

Fast I/O is used for synchronous I/O with cached files. 

Fast I/O requests transfer data between user buffers and the system cache directly, bypassing the file
system and storage driver stack, thus avoiding unnecessary overhead.

The NTFS file system driver,
as a canonical example, supports Fast I/O.

The filter manager abstracts I/O operations, regardless of whether they are IRP-based or fast I/O
based. 

Mini-filters can intercept any such request. 

If the driver is not interested in fast I/O, for example, it can query the actual request type provided by the filter manager with `FLT_IS_FASTIO_OPERATION` and/or `FLT_IS_IRP_OPERATION` macros

<img src="" width="80%" /> 

The second field in `FLT_OPERATION_REGISTRATION` is a set of flags which can be zero or a
combination of one of the following flags affecting read and write operations:
- `FLTFL_OPERATION_REGISTRATION_SKIP_CACHED_IO`: do not invoke the callback(s) if it’s cached I/O (such as fast I/O operations, which are always cached).
- `FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO`: do not invoke the callback(s) for paging I/O (IRP-based operations only).
- `FLTFL_OPERATION_REGISTRATION_SKIP_NON_DASD_IO`: do not invoke the callback(s) for DAX volumes.

The next two fields are pre and post operation callbacks, where at least one must be non-NULL. Here is an example of initializing an array of `FLT_OPERATION_REGISTRATION`:
```C++
const FLT_OPERATION_REGISTRATION Callbacks[] = {
  { IRP_MJ_CREATE, 0, nullptr, SamplePostCreateOperation },
  { IRP_MJ_WRITE, FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO,
  SamplePreWriteOperation, nullptr },
  { IRP_MJ_CLOSE, 0, nullptr, SamplePostCloseOperation },
  { IRP_MJ_OPERATION_END }
};
```

With this array, registration for a driver that does not require any contexts could be done with the following code:

```C++
const FLT_REGISTRATION FilterRegistration = {
  sizeof(FLT_REGISTRATION),
  FLT_REGISTRATION_VERSION,
  0, // Flags
  nullptr, // Context
  Callbacks, // Operation callbacks
  ProtectorUnload, // MiniFilterUnload
  SampleInstanceSetup, // InstanceSetup
  SampleInstanceQueryTeardown, // InstanceQueryTeardown
  SampleInstanceTeardownStart, // InstanceTeardownStart
  SampleInstanceTeardownComplete, // InstanceTeardownComplete
};

PFLT_FILTER FilterHandle;

NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
  NTSTATUS status;
  //... some code
  status = FltRegisterFilter(DriverObject, &FilterRegistration, &FilterHandle);
  if(NT_SUCCESS(status)) {
    // actually start I/O filtering
    status = FltStartFiltering(FilterHandle);
    if(!NT_SUCCESS(status))
      FltUnregisterFilter(FilterHandle);
  }
  return status;
}
```

## The Altitude
File system mini-filters must have an altitude, indicating their relative **position** within the file system filters hierarchy.

First, altitude value is not provided as part of mini-filter’s registration, but is read from
the registry. 

When the driver is installed, its altitude is written in the proper location in the registry.

Images show that registry entry for the built-in Fileinfo mini-filter driver; Altitude is clearly visible, and is the same value shown earlier with the fltmc.exe tool.

<img src="" width="80%" /> 

Here is an example that should clarify why altitude matters. 

Suppose there is a mini-filter at altitude 10000 whose job is to encrypt data when written, and decrypt when read. 

Now suppose another minifilter whose job is to check data for malicious activity is at altitude 9000.

<img src="" width="80%" />

The encryption driver encrypts incoming data to be written, which is then passed on to the antivirus driver. 

The anti-virus driver is in a problem, as it sees the encrypted data with no viable way of decrypting it. 

In such a case, the anti-virus driver must have an altitude higher than the encryption driver. 

How can such a driver guarantee this is in fact the case?

To rectify this situation, Microsoft has defined ranges of altitudes for drivers based on their requirements. 

In order to obtain a proper altitude, the driver publisher must send an email to Microsoft and ask an altitude be allocated for that driver based on its intended target. 

## Installation
The **proper** way to install a file system mini-filter is to use an INF file.

## INF Files
INF files are the classic mechanism used to install hardware based device drivers, but these can be
used to install any type of driver.

**File System Mini-Filter** project templates provided by WDK creates such an INF file, which is almost ready for installation.

INF files use the old INI file syntax, where there are sections in square brackets, and underneath a
section there are entries in the form **key=value**. 

These entries are instructions to the installer that parses the file, essentially instructing the installer to do two types of operations: copy files to specific
locations and making changes to the registry.

### The Version Section
The Version section is mandatory in an INF. 

The following is generated by the WDK project wizard (slightly modified for readability):

```ini
[Version]
Signature = "$Windows NT$"
; TODO - Change the Class and ClassGuid to match the Load Order Group value,
; see https://msdn.microsoft.com/en-us/windows/hardware/gg462963
; Class = "ActivityMonitor"
;This is determined by the work this filter driver does
; ClassGuid = {b86dff51-a31e-4bac-b3cf-e8cfe75c9fc2}
;This value is determined by the Load Order Group value
Class = "_TODO_Change_Class_appropriately_"
ClassGuid = {_TODO_Change_ClassGuid_appropriately_}
Provider = %ManufacturerName%
DriverVer =
CatalogFile = Sample.cat
```

Signature directive must be set to the magic string `$Windows NT$`. 

Class and ClassGuid directives are mandatory and specify the class (type or group) to which this
driver belongs to. 

The generated INF contains an example class, ActivityMonitor and its associated GUID, both in comments.

The simplest solution is to uncomment these to lines and remove the dummy Class and ClassGuid
resulting in the following:
```ini
Class = "ActivityMonitor"
ClassGuid = {b86dff51-a31e-4bac-b3cf-e8cfe75c9fc2}
```

**Class** is one a set of predefined groups of devices, used mostly by hardware based drivers, but also by mini-filters. 

The complete table list of classes is stored in the registry under the key
`HKLM\System\CurrentControlSet\Control\Class`. 

Each class is uniquely identified by a GUID; the string name is just a human-readable helper. 

ActivityMonitor class in the registry:
<img src="" width="80%" />

Notice the GUID is the key name. The class name itself is provided in the Class value. 

The other values within this key are not important from a practical perspective. 

The entry that makes this class
**eligible** for file system mini-filters is the value `FSFilterClass` having the value of 1.

All the existing classes on a system is available in `FSClass.exe` tool.

Back to the Version section in the INF: the Provider directive is the name of the driver publisher.

It doesn’t mean much in practical terms, but might appear in some UI, so should be something meaningful.

The value set by the WDK template is `%ManufacturerName%`. 

Anything within percent symbols is sort of a macro: to be replaced by the actual value specified in another section called Strings. 

Here is part of this section:
```ini
[Strings]
; TODO - Add your manufacturer
ManufacturerName = "Template"
ServiceDescription = "Sample Mini-Filter Driver"
ServiceName = "Sample"
```

Here, `ManufacturerName` is replaced with **Template**. The driver writer should replace **Template** with a proper company or product name.

`DriverVer` directive specifies the date/time of the driver and the version. 

`CatalogFile` directive points to a catalog file, storing the digital signatures of the driver package (the driver package contains the output files for the driver: SYS, INF and CAT files).

## The DefaultInstall Section
indicating what operations should execute as part of **running**
this INF. 

By having this section, a driver can be installed using Windows Explorer, by right clicking INF and selecting Install.

The WDK generated wizard created this:
```ini
[DefaultInstall]
OptionDesc = %ServiceDescription%
CopyFiles = MiniFilter.DriverFiles
```

`OptionDesc` directive provides a simple description that is used in case the user install the
driver using the Plug & Play driver installation wizard (uncommon for file-system mini-filters). 

CopyFiles points to another section (with the given name) that should indicate what files should be copied and to where.

For example, `CopyFile` directive points to `MiniFilter.DriverFiles`:
```ini
[MiniFilter.DriverFiles]
%DriverName%.sys
```

%DriverName% points to Sample.sys, the driver output file. 
`DestinationDirs` section:
```ini
[DestinationDirs]
DefaultDestDir = 12
MiniFilter.DriverFiles = 12 ;%windir%\system32\drivers
```

`DefaultDestDir` is the default directory to copy to if not explicitly specified. 

The value here, 12 is pointing to the system’s drivers directory, shown in the comment for the second directive. 

`System32\Drivers` directory is the canonical location to place drivers
in. 

In previous chapters, drivers are placed anywhere, but drivers should be placed in that drivers folder, at least for protection purposes, as this folder is a system one, and so does not allow full access for standard users.

## The Service Section
Installation in the services registry key is similarly to what `CreateService` does. 

This step is mandatory for any driver. Definition:
```ini
[DefaultInstall.Services]
AddService = %ServiceName%,,MiniFilter.Service
```

`DefaultInstall` is appended with **.Services** and such a section is automatically searched. 

If found, `AddService` directive points to another section that indicates what information to write to the registry in the service key named `%ServiceName%`.

The extra comma is a placeholder for a set of flags, zero in this case. 

The wizard-generated code follows:
```ini
[MiniFilter.Service]
DisplayName = %ServiceName%
Description = %ServiceDescription%
ServiceBinary = %12%\%DriverName%.sys ;%windir%\system32\drivers\
Dependencies = "FltMgr"
ServiceType = 2 ;SERVICE_FILE_SYSTEM_DRIVER
StartType = 3 ;SERVICE_DEMAND_START
ErrorControl = 1 ;SERVICE_ERROR_NORMAL
; TODO - Change the Load Order Group value
; LoadOrderGroup = "FSFilter Activity Monitor"
LoadOrderGroup = "_TODO_Change_LoadOrderGroup_appropriately_"
AddReg = MiniFilter.AddRegistry
```

`ServiceType` is 2 for a file system related driver (as opposed to 1 for **standard** drivers). 

`Dependencies` is something we haven’t met before: this is a list of services/drivers this service/driver depends on.

In the file system mini-filter case, this is Filter Manager itself.

`LoadOrderGroup` should be specified based on the mini-filter group names. 

Finally, `AddReg` directive points to another section with instructions for adding more registry entries.

Fixed MiniFilter.Service section:
```ini
[MiniFilter.Service]
DisplayName = %ServiceName%
Description = %ServiceDescription%
ServiceBinary = %12%\%DriverName%.sys ;%windir%\system32\drivers\
Dependencies = "FltMgr"
ServiceType = 2 ;SERVICE_FILE_SYSTEM_DRIVER
StartType = 3 ;SERVICE_DEMAND_START
ErrorControl = 1 ;SERVICE_ERROR_NORMAL
LoadOrderGroup = "FSFilter Activity Monitor"
AddReg = MiniFilter.AddRegistry
```

## AddReg Sections
This section is used to add custom registry entries for any and all purposes. 

The wizard generated INF contains the following additions to the registry:
```ini
[MiniFilter.AddRegistry]
HKR,,"DebugFlags",0x00010001 ,0x0
HKR,,"SupportedFeatures",0x00010001,0x3
HKR,"Instances","DefaultInstance",0x00000000,%DefaultInstance%
HKR,"Instances\"%Instance1.Name%,"Altitude",0x00000000,%Instance1.Altitude%
HKR,"Instances\"%Instance1.Name%,"Flags",0x00010001,%Instance1.Flags%
```

- root key: one of HKLM, HKCU (current user), HKCR (classes root), HKU (users) or HKR (relative to the calling section). In our case, HKR is the service subkey(`HKLMSystemCurrentControlSetServicesSample`).
- subkey from the root key 
- value name to set
- flags: many are defined, default is zero which indicates writing of a REG_SZ value. Some other flags:
1. 0x100000: write `REG_MULTI_SZ`
2. 0x100001: write `REG_DWORD`
3. 0x000001: write a binary value (`REG_BINARY`)
4. 0x000002: no clobber. Do not overwrite an existing value
5. 0x000008: append a value. The existing value must be `REG_MULTI_SZ`
- Actual value or values to write/append

The code snippet above sets some defaults for file system mini-filters. 

The most important value is the altitude, taken from the `%Instance1.Altitude%` in the strings section.

## Finalizing the INF
Modify is altitude in Strings section:
```ini
[Strings]
; other entries
;Instances specific information.
DefaultInstance = "Sample Instance"
Instance1.Name = "Sample Instance"
Instance1.Altitude = "360100"
Instance1.Flags = 0x0 ; Allow all attachments
```

Altitude was chosen from the Activity Monitor group altitude range. 

In a real driver, that number will be returned by Microsoft.

Lastly, the flags value indicates the driver is fine with attaching to any volume, but in actuality the
driver will be queried in its Instance Setup callback, where it can allow or reject attachments.

## Installing the Driver
Once the INF file is properly modified, and the driver code compiles, it is ready to be installed. 

The simplest way to install is to copy the driver package (SYS, INF and CAT files) to the target system,
and then right click the INF file in Explorer and select Install. 

This will **run** the INF, executing the required operations.

At this point, the mini-filter is installed, and can be loaded with the fltmc command line tool:
`c:\>fltmc load <name>`

## Processing I/O Operations
The main function of a file system mini-filter is processing I/O operations by implementing pre
and/or post callbacks for the operations of interest. 

Pre operations allow a mini-filter to reject an operation completely, while post operation allows looking at the result of the operation, and in some cases: making changes to the returned information.

## Pre Operation Callbacks
```C++
FLT_PREOP_CALLBACK_STATUS SomePreOperation (
  _Inout_ PFLT_CALLBACK_DATA Data,
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _Outptr_ PVOID *CompletionContext);
```

Here are common return values to use:
- `FLT_PREOP_COMPLETE`: indicates the driver is completing the operation. The filter manager
does not call the post-operation callback and does not forward the request to lower-layer mini-filters.
- `FLT_PREOP_SUCCESS_NO_CALLBACK`: indicates the pre-operation is done with request and lets it continue flowing to the next filter. The driver does not want its post-operation callback to be called for this operation.
- `FLT_PREOP_SUCCESS_WITH_CALLBACK`: indicates the driver allows the filter manager to propagate the request to lower-layer filters, but it wants its post-callback invoked for this operation.
- `FLT_PREOP_PENDING`: indicates the driver is pending the operation. The filter manager does not continue processing the request until the driver calls `FltCompletePendedPreOperation`
to let filter manager know it can continue processing this request.
- `FLT_PREOP_SYNCHRONIZE`: similar to `FLT_PREOP_SUCCESS_WITH_CALLBACK`, but the driver asks filter manager to invoke its post-callback on the same thread at `IRQL <= APC_LEVEL` (normally the post-operation callback can be invoked at `IRQL <= DISPATCH_LEVEL` by an arbitrary thread).

The Data argument provides all the information related to the I/O operation itself, as  `FLT_CALLBACK_DATA`:
```C++
typedef struct _FLT_CALLBACK_DATA {
  FLT_CALLBACK_DATA_FLAGS Flags;
  PETHREAD CONST Thread;
  PFLT_IO_PARAMETER_BLOCK CONST Iopb;
  IO_STATUS_BLOCK IoStatus;
  
  struct _FLT_TAG_DATA_BUFFER *TagData;

  union {
    struct {
      LIST_ENTRY QueueLinks;
      PVOID QueueContext[2];
    };
    PVOID FilterContext[4];
  };
  KPROCESSOR_MODE RequestorMode;
} FLT_CALLBACK_DATA, *PFLT_CALLBACK_DATA;
```

### Rundown
- Flags may contain zero or a combination of flags, some of which are listed below:
1. `FLTFL_CALLBACK_DATA_DIRTY`: indicates the driver has made changes to the structure and
then called `FltSetCallbackDataDirty`. Every member of the structure can be modified except Thread and RequestorMode.
2. `FLTFL_CALLBACK_DATA_FAST_IO_OPERATION`: indicates this is a fast I/O operation.
3. `FLTFL_CALLBACK_DATA_IRP_OPERATION`: indicates this is an IRP-based operation.
4. `FLTFL_CALLBACK_DATA_GENERATED_IO`: indicates this is an operation generated by another mini-filter.
5. `FLTFL_CALLBACK_DATA_POST_OPERATION`: indicates this is a post-operation, rather than a pre-operation.
- Thread is an opaque pointer to the thread requesting this operation.
- `IoStatus` is the status of the request. A pre-operation can set this value and then indicate the
operation is complete by returning `FLT_PREOP_COMPLETE`. A post-operation can look at the
final status of the operation.
- `RequestorMode` indicates whether the requestor of the operation is from user mode or kernel mode.
- `Iopb` is in itself a structure holding the detailed parameters of the request, defined like so:
```C++
typedef struct _FLT_IO_PARAMETER_BLOCK {
  ULONG          IrpFlags;
  UCHAR          MajorFunction;
  UCHAR          MinorFunction;
  UCHAR          OperationFlags;
  UCHAR          Reserved;
  PFILE_OBJECT   TargetFileObject;
  PFLT_INSTANCE  TargetInstance;
  FLT_PARAMETERS Parameters;
} FLT_IO_PARAMETER_BLOCK, *PFLT_IO_PARAMETER_BLOCK;
```
1. TargetFileObject is the file object that is the target of this operation; it’s useful to have when
invoking some APIs.
2. Parameters is a monstrous union providing the actual data for the specific information (similar
to Paramters member of `IO_STACK_LOCATION`). The driver looks at the proper structure within this union to get to the information it needs.

`FLT_RELATED_OBJECTS` contains opaque handles to the current filter, instance and volume, which are useful in some APIs.
```C++
typedef struct _FLT_RELATED_OBJECTS {
  USHORT CONST Size;
  USHORT CONST TransactionContext;
  PFLT_FILTER CONST Filter;
  PFLT_VOLUME CONST Volume;
  PFLT_INSTANCE CONST Instance;
  PFILE_OBJECT CONST FileObject;
  PKTRANSACTION CONST Transaction;
} FLT_RELATED_OBJECTS, *PFLT_RELATED_OBJECTS;
```

FileObject is the same one accessed through the I/O parameter block’s TargetFileObject field.

The last argument to the pre-callback is a context value that can be set by the driver. 

If set, this value is propagated to the post-callback routine for the same request (default is NULL).

## Post Operation Callbacks
```C++
FLT_POSTOP_CALLBACK_STATUS SomePostOperation (
  _Inout_ PFLT_CALLBACK_DATA Data,
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _In_opt_ PVOID CompletionContext,
  _In_ FLT_POST_OPERATION_FLAGS Flags);
```

The post-operation function is called at `IRQL <= DISPATCH_LEVEL` in an arbitrary thread context,
unless the pre-callback routine returned `FLT_PREOP_SYNCHRONIZE`, in which case the filter manager
guarantees the post-callback is invoked at `IRQL < DISPATCH_LEVEL` on the same thread that executed
the pre-callback.

In the former case, the driver cannot perform certain types of operations because IRQL is too
high:
- Cannot access paged memory.
- Cannot use kernel APIs that only work at `IRQL < DISPATCH_LEVEL`.
- Cannot acquire synchronization primitives such as mutexes, fast mutexes, executive resources,
semaphores, events, etc.
- Cannot set, get or delete contexts, but it can release contexts

If the driver needs to do any of the above, it somehow must defer its execution to another routine
called at `IRQL < DISPATCH_LEVEL`. This can be done in one of two ways:
1. The driver calls `FltDoCompletionProcessingWhenSafe` which sets up a callback function
that is invoked by a system worker thread at `IRQL < DISPATCH_LEVEL` (if the post-operation was called at `IRQL = DISPATCH_LEVEL`).
2. The driver posts a work item by calling `FltQueueDeferredIoWorkItem`, which queues a work item that will eventually execute by a system worker thread at `IRQL = PASSIVE_LEVEL`. In the work item callback, the driver will eventually call `FltCompletePendedPostOperation` to signal the filter manager that the post-operation is complete.

Although using `FltDoCompletionProcessingWhenSafe` is easier, it has some limitations that prevent it from being used in some scenarios:
- Cannot be used for `IRP_MJ_READ`, `IRP_MJ_WRITE` or `IRP_MJ_FLUSH_BUFFERS` because it can cause deadlock if these operations are completed synchronously by lower layer.
- Can only be called for IRP-based operations (can check with `FLT_IS_IRP_OPERATION`).

The returned value from pos-callback is usually `FLT_POSTOP_FINISHED_PROCESSING` to indicate the driver is finished with this operation. 

However, if the driver needs to perform work in a work item (high IRQL), the driver can return `FLT_POSTOP_MORE_PROCESSING_REQUIRED` to tell filter manager the operation is still pending completion, and in the work item call `FltCompletePendedPostOperation` to let filter manager know it can continue processing this request.

## The Delete Protector Driver
Create driver to protect certain files from deletion by certain processes. 

Built based on WDK-provided project template.

Started by creating a new File System Mini-Filter project named DelProtect.

The changed sections in INF:
```ini
[Version]
Signature = "$Windows NT$"
Class = "Undelete"
ClassGuid = {fe8f1572-c67a-48c0-bbac-0b5c6d66cafb}
Provider = %ManufacturerName%
DriverVer =
CatalogFile = DelProtect.cat

[MiniFilter.Service]
DisplayName = %ServiceName%
Description = %ServiceDescription%
ServiceBinary = %12%\%DriverName%.sys ;%windir%\system32\drivers\
Dependencies = "FltMgr"
ServiceType = 2 ;SERVICE_FILE_SYSTEM_DRIVER
StartType = 3 ;SERVICE_DEMAND_START
ErrorControl = 1 ;SERVICE_ERROR_NORMAL
LoadOrderGroup = "FS Undelete filters"
AddReg = MiniFilter.AddRegistry

[Strings]
ManufacturerName = "WindowsDriversBook"
ServiceDescription = "DelProtect Mini-Filter Driver"
ServiceName = "DelProtect"
DriverName = "DelProtect"
DiskId1 = "DelProtect Device Installation Disk"
;Instances specific information.
DefaultInstance = "DelProtect Instance"
Instance1.Name = "DelProtect Instance"
Instance1.Altitude = "345101" ; in the range of the undelete group
Instance1.Flags = 0x0 ; Allow all attachments
```

DriverEntry provided by the project template already has the mini-filter registration code in place. 

Need to tweak the callbacks to indicate which are the ones interested in. 

What major functions are involved in file deletion?

It turns out there are two way to delete a file. 
- Use `IRP_MJ_SET_INFORMATION` to  provide a bag of operations, delete just being one of them. 
- Delete a file is to open file with `FILE_DELETE_ON_CLOSE` flag. The file then is deleted as soon as that last handle to it is closed.

For the driver, support both options for deletion to cover all our bases.

`FLT_OPERATION_REGISTRATION` array must be modified to support these two options:
```C++
CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
  { IRP_MJ_CREATE, 0, DelProtectPreCreate, nullptr },
  { IRP_MJ_SET_INFORMATION, 0, DelProtectPreSetInformation, nullptr },
  { IRP_MJ_OPERATION_END }
};
```

## Handling Pre-Create
Start with pre-create function. 

Its prototype is the same as
any pre-callback:
```C++
FLT_PREOP_CALLBACK_STATUS DelProtectPreCreate(
  _Inout_ PFLT_CALLBACK_DATA Data,
  _In_ PCFLT_RELATED_OBJECTS FltObjects,
  _Flt_CompletionContext_Outptr_ PVOID *CompletionContext);
```

Check if operation is originating from kernel mode, and if so, just let it continueun interrupted:
```C++
UNREFERENCED_PARAMETER(CompletionContext);
UNREFERENCED_PARAMETER(FltObjects);

if (Data->RequestorMode == KernelMode)
  return FLT_PREOP_SUCCESS_NO_CALLBACK;
```

Next, check if `FILE_DELETE_ON_CLOSE` exists in creation request. 
The structure to look at is the Create field under the Paramaters inside Iopb like so:
```C++
const auto& params = Data->Iopb->Parameters.Create;
if (params.Options & FILE_DELETE_ON_CLOSE) {
  // delete operation
}
// otherwise, just carry on
return FLT_PREOP_SUCCESS_NO_CALLBACK;
```

The above params variable references the Create structure defined like so:
```C++
struct {
  PIO_SECURITY_CONTEXT SecurityContext;
  //
  // The low 24 bits contains CreateOptions flag values.
  // The high 8 bits contains the CreateDisposition values.
  //
  ULONG Options;

  USHORT POINTER_ALIGNMENT FileAttributes;
  USHORT ShareAccess;
  ULONG POINTER_ALIGNMENT EaLength;

  PVOID EaBuffer; //Not in IO_STACK_LOCATION parameters list
  LARGE_INTEGER AllocationSize; //Not in IO_STACK_LOCATION parameters list
} Create;
```

Generally, for any I/O operation, the documentation must be consulted to understand what’s available and how to use it. 

In this case, Options field is a combination of flags documented
under `FltCreateFile`.

The code checks to see if this flag exists, and if so, it means a delete operation is being initiated.

This driver will block delete operations coming from `cmd.exe`. 

Need to get the image path of the calling process. 

Since a create operation is invoked synchronously, the caller is the process attempting to delete something. 

How to get the image path of the current process?

One way would be to use  `NtQueryInformationProcess` (or `ZwQueryInformationProcess`). 

It’s semi-documented, and its prototype available in the user model header <wintrnl.h>. 

Copy its declaration and change to Zw:
```C++
extern "C" NTSTATUS ZwQueryInformationProcess(
  _In_ HANDLE ProcessHandle,
  _In_ PROCESSINFOCLASS ProcessInformationClass,
  _Out_ PVOID ProcessInformation,
  _In_ ULONG ProcessInformationLength,
  _Out_opt_ PULONG ReturnLength);
```

`PROCESSINFOCLASS` is actually mostly available in <ntddk.h>, and it does not provide all supported values.

Driver get the full path of a process’ image file by using `ProcessImageFileName` from `PROCESSINFOCLASS`.

The documentation for `NtQueryInformationProcess` indicates that for `ProcessImageFileName`,
the returned data is `UNICODE_STRING`, that must be allocated by the caller:
```C++
auto size = 300; // some arbitrary size enough for cmd.exe image path
auto processName = (UNICODE_STRING*)ExAllocatePool(PagedPool, size);
if (processName == nullptr)
  return FLT_PREOP_SUCCESS_NO_CALLBACK;
RtlZeroMemory(processName, size); // ensure string will be NULL-terminated
```

Allocate a contiguous buffer and the API itself is going to place the actual characters just after the structure itself in memory and point to it from its internal buffer. 

API call:
```C++
auto status = ZwQueryInformationProcess(NtCurrentProcess(), ProcessImageFileName,
processName, size - sizeof(WCHAR), nullptr);
```

`NtCurrentProcess` returns a pseudo-handle that refers to the current process (practically the same as `GetCurrentProcess`).

If the call succeeds, need to compare the process image file name to cmd.exe. 
Simple way to do:
```C++
if (NT_SUCCESS(status)) {
  if (wcsstr(processName->Buffer, L"\\System32\\cmd.exe") != nullptr ||
    wcsstr(processName->Buffer, L"\\SysWOW64\\cmd.exe") != nullptr) {
    // do something
  }
}
```

The actual path returned from `ZwQueryInformationProcess` call is the native path, something like `\Device\HarddiskVolume3\Windows\System32\cmd.exe`.

If this is indeed cmd.exe, need to prevent operation from succeeding. 

The standard way of doing this is changing the operation status (`Data->IoStatus.Status`) to an appropriate failure status and return from the callback `FLT_PREOP_COMPLETE` to tell the filter manager not to continue
with the request.

Here is the entire pre-create callback, slightly modified:
```C++
_Use_decl_annotations_
FLT_PREOP_CALLBACK_STATUS DelProtectPreCreate(
  PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID*) {
  UNREFERENCED_PARAMETER(FltObjects);

  if (Data->RequestorMode == KernelMode)
    return FLT_PREOP_SUCCESS_NO_CALLBACK;

  auto& params = Data->Iopb->Parameters.Create;
  auto returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

  if (params.Options & FILE_DELETE_ON_CLOSE) {
    // delete operation
    KdPrint(("Delete on close: %wZ\n", &Data->Iopb->TargetFileObject->FileName));
    auto size = 300; // some arbitrary size enough for cmd.exe image path
    auto processName = (UNICODE_STRING*)ExAllocatePool(PagedPool, size);
    if (processName == nullptr)
      return FLT_PREOP_SUCCESS_NO_CALLBACK;

    RtlZeroMemory(processName, size); // ensure string will be NULL-terminated
    auto status = ZwQueryInformationProcess(NtCurrentProcess(), ProcessImageFileName, processName, size - sizeof(WCHAR), nullptr);

    if (NT_SUCCESS(status)) {
      KdPrint(("Delete operation from %wZ\n", processName));

      if (wcsstr(processName->Buffer, L"\\System32\\cmd.exe") != nullptr || wcsstr(processName->Buffer, L"\\SysWOW64\\cmd.exe") != nullptr) {
        // fail request
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        returnStatus = FLT_PREOP_COMPLETE;
        KdPrint(("Prevent delete from IRP_MJ_CREATE by cmd.exe\n"));
      }
    }
    ExFreePool(processName);
  }
  return returnStatus;
} 
```

Provide something for `IRP_MJ_SET_INFORMATION` pre-callback.
Simple allow-all implementation:
```C++
FLT_PREOP_CALLBACK_STATUS DelProtectPreSetInformation(
  _Inout_ PFLT_CALLBACK_DATA Data, _In_ PCFLT_RELATED_OBJECTS FltObjects, PVOID*) {
  UNREFERENCED_PARAMETER(FltObjects);
  UNREFERENCED_PARAMETER(Data);
  return FLT_PREOP_SUCCESS_NO_CALLBACK;
}
```

## Handling Pre-Set Information
The second way file deletion is implemented by file systems. 
Start by ignoring kernel callers as
with `IRP_MJ_CREATE`:
```C++
_Use_decl_annotations_
FLT_PREOP_CALLBACK_STATUS DelProtectPreSetInformation(
  PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID*) {
  UNREFERENCED_PARAMETER(FltObjects);

  if (Data->RequestorMode == KernelMode)
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
```

Since `IRP_MJ_SET_INFORMATION` is the way to do several types of operations, need to check if
this is in fact a delete operation.

The driver must first access the proper structure in the parameters
union, declared like so:
```C++
struct {
  ULONG Length;
  FILE_INFORMATION_CLASS POINTER_ALIGNMENT FileInformationClass;
  PFILE_OBJECT ParentOfTarget;
  union {
    struct {
      BOOLEAN ReplaceIfExists;
      BOOLEAN AdvanceOnly;
    };
    ULONG ClusterCount;
    HANDLE DeleteHandle;
  };
  PVOID InfoBuffer;
} SetFileInformation;
```

`FileInformationClass` indicates which type of operation this instance represents and so need
to check whether this is a delete operation:
```C++
auto& params = Data->Iopb->Parameters.SetFileInformation;

if (params.FileInformationClass != FileDispositionInformation &&
  params.FileInformationClass != FileDispositionInformationEx) {
  // not a delete operation
  return FLT_PREOP_SUCCESS_NO_CALLBACK;
}
```

`FileDispositionInformation` indicates a delete operation, and is similar and undocumented, but is used internally by the user mode
`DeleteFile`.

If it is a delete operation, there is yet another check to do, but looking at the information buffer
which is of type `FILE_DISPOSITION_INFORMATION` for delete operations and checking the boolean stored there:
```C++
auto info = (FILE_DISPOSITION_INFORMATION*)params.InfoBuffer;
if (!info->DeleteFile)
  return FLT_PREOP_SUCCESS_NO_CALLBACK;
```

In `IRP_MJ_CREATE` case, the callback is called by the requesting thread, so can just access the current process to find out which image file is used to make the call.

In all other major functions, this is not necessary the case and must look at Thread field in the provided data for the original caller. 

Can get the pointer to the process from this thread:
```C++
// what process did this originate from?
auto process = PsGetThreadProcess(Data->Thread);
NT_ASSERT(process); // cannot really fail
```

Goal is to call `ZwQueryInformationProcess`, but need a handle for that. 

This is where `ObOpenObjectByPointer` comes in. 

It allows obtaining a handle to an object. 
```C++
NTSTATUS ObOpenObjectByPointer(
  _In_ PVOID Object,
  _In_ ULONG HandleAttributes,
  _In_opt_ PACCESS_STATE PassedAccessState,
  _In_ ACCESS_MASK DesiredAccess,
  _In_opt_ POBJECT_TYPE ObjectType,
  _In_ KPROCESSOR_MODE AccessMode,
  _Out_ PHANDLE Handle);
```

`ObOpenObjectByPointer` arguments:
- `Object`: is the object for which a handle is needed. It can be any type of kernel object.
- `HandleAttributes`: is a set of optional flags. The most useful flag is `OBJ_KERNEL_HANDLE`. This flag makes the returned kernel handle, which is unusable by user mode code and can be used from any process context.
- `PassedAccessState`: is an optional pointer to `ACCESS_STATE`, not typically useful for drivers, set to NULL.
- `DesiredAccess`: is the access mask the handle should be opened with. If the AccessMode argument is KernelMode, then this can be zero and the returned handle will be all-powerful.
- `ObjectType`: is an optional object type the function can compare the Object to, such as
`*PsProcessType`, `*PsThreadType` and other exported type objects. Specifying NULL does not force any check on the passed in object.
- `AccessMode`: can be `UserMode` or `KernelMode`. Drivers usually specify `KernelMode` to indicate
the request is not on behalf of a user mode process. With KernelMode, no access check is made.
- `Handle`: is the pointer to the returned handle.

Given the above function, opening a handle to the process follows:
```C++
HANDLE hProcess;
auto status = ObOpenObjectByPointer(process, OBJ_KERNEL_HANDLE, nullptr, 0, nullptr, KernelMode, &hProcess);
if (!NT_SUCCESS(status))
  return FLT_PREOP_SUCCESS_NO_CALLBACK;
```

Once getting a handle to the process, query for the process’ image file name, and see if it’s cmd.exe:
```C++
  auto returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
  auto size = 300;
  auto processName = (UNICODE_STRING*)ExAllocatePool(PagedPool, size);
  if (processName) {
    RtlZeroMemory(processName, size); // ensure string will be NULL-terminated
    status = ZwQueryInformationProcess(hProcess, ProcessImageFileName,processName, size - sizeof(WCHAR), nullptr);

    if (NT_SUCCESS(status)) {
      KdPrint(("Delete operation from %wZ\n", processName));

      if (wcsstr(processName->Buffer, L"\\System32\\cmd.exe") != nullptr || wcsstr(processName->Buffer, L"\\SysWOW64\\cmd.exe") != nullptr) {
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        returnStatus = FLT_PREOP_COMPLETE;
        KdPrint(("Prevent delete from IRP_MJ_SET_INFORMATION by cmd.exe\n"));
      }
    }
    ExFreePool(processName);
  }
  ZwClose(hProcess);
  return returnStatus;
}
```

## Some Refactoring
Extract the code to open a process handle, get an image file and comparing to cmd.exe to a separate function:
```C++
bool IsDeleteAllowed(const PEPROCESS Process) {
  bool currentProcess = PsGetCurrentProcess() == Process;
  HANDLE hProcess;
  if (currentProcess)
    hProcess = NtCurrentProcess();
  else {
    auto status = ObOpenObjectByPointer(Process, OBJ_KERNEL_HANDLE, nullptr, 0, nullptr, KernelMode, &hProcess);
    if (!NT_SUCCESS(status))
      return true;
  }

  auto size = 300;
  bool allowDelete = true;
  auto processName = (UNICODE_STRING*)ExAllocatePool(PagedPool, size);

  if (processName) {
    RtlZeroMemory(processName, size);
    auto status = ZwQueryInformationProcess(hProcess, ProcessImageFileName,processName, size - sizeof(WCHAR), nullptr);

    if (NT_SUCCESS(status)) {
      KdPrint(("Delete operation from %wZ\n", processName));

      if (wcsstr(processName->Buffer, L"\\System32\\cmd.exe") != nullptr || wcsstr(processName->Buffer, L"\\SysWOW64\\cmd.exe") != nullptr) {
        allowDelete = false;
      }
    }
    ExFreePool(processName);
  }
  if (!currentProcess)
    ZwClose(hProcess);

  return allowDelete;
}
```

The function accepts an opaque pointer to the process attempting to delete a file. 

If the process address is the current process `PsGetCurrentProcess` then using a full-fledged open is just a waste
of time and the pseudo-handle `NtCurrentProcess` can be used instead. 

Otherwise, a full process
open is required. 

Remember to free the image file buffer and close the process handle.

```C++
Use_decl_annotations_
FLT_PREOP_CALLBACK_STATUS DelProtectPreCreate(
  PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID*) {
  UNREFERENCED_PARAMETER(FltObjects);

  if (Data->RequestorMode == KernelMode)
    return FLT_PREOP_SUCCESS_NO_CALLBACK;

  auto& params = Data->Iopb->Parameters.Create;
  auto returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

  if (params.Options & FILE_DELETE_ON_CLOSE) {
    // delete operation
    KdPrint(("Delete on close: %wZ\n", &Data->Iopb->TargetFileObject->FileName));

    if (!IsDeleteAllowed(PsGetCurrentProcess())) {
      Data->IoStatus.Status = STATUS_ACCESS_DENIED;
      returnStatus = FLT_PREOP_COMPLETE;
      KdPrint(("Prevent delete from IRP_MJ_CREATE by cmd.exe\n"));
    }
  }
  return returnStatus;
}
```

And the revised `IRP_MJ_SET_INFORMATION` pre-callback:
```C++
FLT_PREOP_CALLBACK_STATUS DelProtectPreSetInformation(
  _Inout_ PFLT_CALLBACK_DATA Data, _In_ PCFLT_RELATED_OBJECTS FltObjects, PVOID*) {
  UNREFERENCED_PARAMETER(FltObjects);
  UNREFERENCED_PARAMETER(Data);

  auto& params = Data->Iopb->Parameters.SetFileInformation;

  if (params.FileInformationClass != FileDispositionInformation && params.FileInformationClass != FileDispositionInformationEx) {
    // not a delete operation
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
  }

  auto info = (FILE_DISPOSITION_INFORMATION*)params.InfoBuffer;
  if (!info->DeleteFile)
    return FLT_PREOP_SUCCESS_NO_CALLBACK;

  auto returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

  // what process did this originate from?
  auto process = PsGetThreadProcess(Data->Thread);
  NT_ASSERT(process);

  if (!IsDeleteAllowed(process)) {
    Data->IoStatus.Status = STATUS_ACCESS_DENIED;
    returnStatus = FLT_PREOP_COMPLETE;
    KdPrint(("Prevent delete from IRP_MJ_SET_INFORMATION by cmd.exe\n"));
  }

  return returnStatus;
}
```

## Generalizing the Driver
The current driver only checks for delete operations from cmd.exe. 

Register executable names from which delete operations can be prevented.

To that end, there will be a classic device object and symbolic link, just as was done in earlier
chapters. 

This is not a problem, and the driver can serve double-duty: a file system mini-filter and
expose a Control Device Object (CDO).

Manage the executable names in a fixed size array for simplicity, have a fast mutex protecting
the array, just as was done in previous chapters. 

Here are the added global variables:
```C++
const int MaxExecutables = 32;

WCHAR* ExeNames[MaxExecutables];
int ExeNamesCount;
FastMutex ExeNamesLock;
```

The revised DriverEntry has now the additional duty of creating the device object and symbolic link and setting dispatch routines, as well as registering as a mini-filter:
```C++
PDEVICE_OBJECT DeviceObject = nullptr;
UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\device\\delprotect");
UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\delprotect");
auto symLinkCreated = false;

do {
  status = IoCreateDevice(DriverObject, 0, &devName,FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
  if (!NT_SUCCESS(status))
    break;

  status = IoCreateSymbolicLink(&symLink, &devName);
  if (!NT_SUCCESS(status))
    break;

  symLinkCreated = true;
  
  status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);

  FLT_ASSERT(NT_SUCCESS(status));
  if (!NT_SUCCESS(status))
    break;

  DriverObject->DriverUnload = DelProtectUnloadDriver;
  DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverObject->MajorFunction[IRP_MJ_CLOSE] = DelProtectCreateClose;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DelProtectDeviceControl;
  ExeNamesLock.Init();

  status = FltStartFiltering(gFilterHandle);
} while (false);

if (!NT_SUCCESS(status)) {
  if (gFilterHandle)
    FltUnregisterFilter(gFilterHandle);
  if (symLinkCreated)
    IoDeleteSymbolicLink(&symLink);
  if (DeviceObject)
    IoDeleteDevice(DeviceObject);
}

return status;
```

Define a few I/O control codes for adding, removing and clearing the list of executable names (in a new file called DelProtectCommon.h):
```C++
#define IOCTL_DELPROTECT_ADD_EXE \
CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DELPROTECT_REMOVE_EXE \
CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DELPROTECT_CLEAR \
CTL_CODE(0x8000, 0x802, METHOD_NEITHER, FILE_ANY_ACCESS)
```

Handling these kinds of control codes is nothing new: here is the complete code for the `IRP_MJ_DEVICE_CONTROL` dispatch routine:
```C++
NTSTATUS DelProtectDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
  auto stack = IoGetCurrentIrpStackLocation(Irp);
  auto status = STATUS_SUCCESS;

  switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_DELPROTECT_ADD_EXE:
    {
      auto name = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;
      if (!name) {
        status = STATUS_INVALID_PARAMETER;
        break;
      }

      if (FindExecutable(name)) {
        break;
      }

      AutoLock locker(ExeNamesLock);
      if (ExeNamesCount == MaxExecutables) {
        status = STATUS_TOO_MANY_NAMES;
        break;
      }

      for (int i = 0; i < MaxExecutables; i++) {
        if (ExeNames[i] == nullptr) {
          auto len = (::wcslen(name) + 1) * sizeof(WCHAR);
          auto buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, len,DRIVER_TAG);
          if (!buffer) {
            status = STATUS_INSUFFICIENT_RESOURCES;
            break;
          }
          ::wcscpy_s(buffer, len / sizeof(WCHAR), name);
          ExeNames[i] = buffer;
          ++ExeNamesCount;
          break;
        }
      }
      break;
    }

    case IOCTL_DELPROTECT_REMOVE_EXE:
    {
      auto name = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;
      if (!name) {
        status = STATUS_INVALID_PARAMETER;
        break;
      }

      AutoLock locker(ExeNamesLock);
      auto found = false;
      for (int i = 0; i < MaxExecutables; i++) {
        if (::_wcsicmp(ExeNames[i], name) == 0) {
          ExFreePool(ExeNames[i]);
          ExeNames[i] = nullptr;
          --ExeNamesCount;
          found = true;
          break;
        }
      }
      if (!found)
        status = STATUS_NOT_FOUND;
      break;
    }

    case IOCTL_DELPROTECT_CLEAR:
      ClearAll();
      break;

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

The missing pieces from the above code are `FindExecutable` and `ClearAll` functions:
```C++
bool FindExecutable(PCWSTR name) {
  AutoLock locker(ExeNamesLock);
  if (ExeNamesCount == 0)
    return false;
  
  for (int i = 0; i < MaxExecutables; i++)
    if (ExeNames[i] && ::_wcsicmp(ExeNames[i], name) == 0)
      return true;
  return false;
}

void ClearAll() {
  AutoLock locker(ExeNamesLock);
  for (int i = 0; i < MaxExecutables; i++) {
    if (ExeNames[i]) {
      ExFreePool(ExeNames[i]);
      ExeNames[i] = nullptr;
    }
  }
  ExeNamesCount = 0;
}
```

Need to make changes to the pre-create callback to search for
executable names in the managing array.
```C++
_Use_decl_annotations_
FLT_PREOP_CALLBACK_STATUS DelProtectPreCreate(
  PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID*) {
  if (Data->RequestorMode == KernelMode)
    return FLT_PREOP_SUCCESS_NO_CALLBACK;

  auto& params = Data->Iopb->Parameters.Create;
  auto returnStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

  if (params.Options & FILE_DELETE_ON_CLOSE) {
    // delete operation
    KdPrint(("Delete on close: %wZ\n", &FltObjects->FileObject->FileName));
    auto size = 512; // some arbitrary size
    auto processName = (UNICODE_STRING*)ExAllocatePool(PagedPool, size);
    if (processName == nullptr)
      return FLT_PREOP_SUCCESS_NO_CALLBACK;

    RtlZeroMemory(processName, size);
    auto status = ZwQueryInformationProcess(NtCurrentProcess(), ProcessImageFileName,processName, size - sizeof(WCHAR), nullptr);

    if (NT_SUCCESS(status)) {
      KdPrint(("Delete operation from %wZ\n", processName));
      
      auto exeName = ::wcsrchr(processName->Buffer, L'\\');
      NT_ASSERT(exeName);

      if (exeName && FindExecutable(exeName + 1)) { // skip backslash
        Data->IoStatus.Status = STATUS_ACCESS_DENIED;
        KdPrint(("Prevented delete in IRP_MJ_CREATE\n"));
        returnStatus = FLT_PREOP_COMPLETE;
      }
    }
    ExFreePool(processName);
  }
  return returnStatus;
}
```

The main modification in the above code is calling `FindExecutable` to find out if the current process
image executable name is one of the values stored in the array. 

If it is, set an **access denied**
status and return `FLT_PREOP_COMPLETE`.

## Testing the Modified Driver
There are three ways to delete a file with user mode APIs:
1. Call `DeleteFile`.
2. Call `CreateFile` with `FILE_FLAG_DELETE_ON_CLOSE`.
3. Call `SetFileInformationByHandle` on an open file

Internally, there are only two ways to delete a file: `IRP_MJ_CREATE` with the `FILE_DELETE_ON_CLOSE` flag and `IRP_MJ_SET_INFORMATION` with `FileDispositionInformation`. 

Clearly, in the above list, item (2) corresponds to the first option and item (3) corresponds to the second option. 

The only mystery left is `DeleteFile`: how does it delete a file?

From the driver’s perspective it does not matter at all, since it must map to one of the two options
the driver handles. 

From a curiosity point of view, `DeleteFile` uses `IRP_MJ_SET_INFORMATION`.

Create a console application project named `DelTest`, for which the usage text should be something like this:
```cmd
c:\book>deltest
Usage: deltest.exe <method> <filename>
Method: 1=DeleteFile, 2=delete on close, 3=SetFileInformation.
```

Examine user mode code for each of these methods (assuming filename is a variable pointing to the file name provided in the command line).
```C++
BOOL success = ::DeleteFile(filename);
```

Opening the file with the delete-on-close flag can be achieved with the following:
```C++
HANDLE hFile = ::CreateFile(filename, DELETE, 0, nullptr, OPEN_EXISTING,
FILE_FLAG_DELETE_ON_CLOSE, nullptr);
::CloseHandle(hFile);
```

When the handle is closed, the file should be deleted.
Lastly:
```C++
FILE_DISPOSITION_INFO info;
info.DeleteFile = TRUE;
HANDLE hFile = ::CreateFile(filename, DELETE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
BOOL success = ::SetFileInformationByHandle(hFile, FileDispositionInfo, &info, sizeof(info));
::CloseHandle(hFile);
```

## File Names
In some mini-filter callbacks, file name being accessed is needed in the majority of cases.

At first, this seems like an easy enough detail to find: `FILE_OBJECT` has `FileName` member.

Files may be opened with a full path or a relative one;
rename operations on the same file may be occurring at the same time; some file name information is cached. 

For these and other internal reasons, `FileName` field in the file object is not be trusted.

In fact, it’s only guaranteed to be valid in `IRP_MJ_CREATE` pre-operation callback, and even there it’s not necessarily in the format the driver needs.

To offset this issues, the filter manager provides `FltGetFileNameInformation` that can
return the correct file name when needed. 

This function is prototyped as follows:
```C++
NTSTATUS FltGetFileNameInformation (
  _In_ PFLT_CALLBACK_DATA CallbackData,
  _In_ FLT_FILE_NAME_OPTIONS NameOptions,
  _Outptr_ PFLT_FILE_NAME_INFORMATION *FileNameInformation);
```

`CallbackData` parameter is the one provided by filter manager in any callback. 

`NameOptions` parameter is a set of flags that specify the requested file format.

Typical value used by most drivers is `FLT_FILE_NAME_NORMALIZED` (full path name) ORed with
`FLT_FILE_NAME_QUERY_DEFAULT` (locate the name in a cache, otherwise query the file system).

The result from the call is provided by the last parameter, `FileNameInformation`. 

The result is an allocated structure that needs to be properly freed by calling `FltReleaseFileNameInformation`.
```C++
typedef struct _FLT_FILE_NAME_INFORMATION {
  USHORT Size;
  FLT_FILE_NAME_PARSED_FLAGS NamesParsed;
  FLT_FILE_NAME_OPTIONS Format;
  UNICODE_STRING Name;
  UNICODE_STRING Volume;
  UNICODE_STRING Share;
  UNICODE_STRING Extension;
  UNICODE_STRING Stream;
  UNICODE_STRING FinalComponent;
  UNICODE_STRING ParentDir;
} FLT_FILE_NAME_INFORMATION, *PFLT_FILE_NAME_INFORMATION;
```

The main ingredients are the several `UNICODE_STRING` structures that should hold the various
components of a file name.

Initially, only Name field is initialized to the full file name (depending on the flags used to query the file name information, “full” may be a partial name). 

If the request specified the flag `FLT_FILE_NAME_NORMALIZED`, then Name points to the full path name, in device form. 

Device form means that file such as `c:\mydir\myfile.txt` is stored with the internal device name
to which **C:** maps to, such as `\Device\HarddiskVolume3\mydir\myfile.txt`. 

This makes the driver’s job a bit more complicated if it somehow depends on paths provided by user mode.

Since only the full name is provided by default (Name field), it’s often necessary to split the
full path to its constituents.

Fortunately, the filter manager provides such a service with
`FltParseFileNameInformation`. 

This one takes `FLT_FILE_NAME_INFORMATION` object
and fills in the other `UNICODE_STRING` fields in the structure.

## File Name Parts
As can be seen from `FLT_FILE_NAME_INFORMATION` declaration, there are several components that make up a full file name. 

Here is an example for the local file `c:\mydir1\mydir2\myfile.txt`:
The volume is the actual device name for which the symbolic link **C:** maps to. 

Images shows WinObj showing the C: symbolic link and its target, which is \Device\HarddiskVolume3 on that
machine.

<img src="" width="80%" />

The share string is empty for local files (Length is zero). 

ParentDir is set to the directory only. 

In driver example that would be \mydir1\mydir2\ (not the trailing backslash). 

`FinalComponent` field stores the file name and stream name (if not using the default stream). 

The Stream component bares some explanation. 

Some file systems (most notable NTFS) provide the ability to have multiple data **streams** in a single file. 

Essentially, this means several files can be stored into a single **physical** file. 

In NTFS, for instance, file’s data
is in fact one of its streams named **$DATA**, which is considered the default stream. 

But it’s possible to create/open another stream, that is stored in the same file, so to speak. 

Tools such as Windows Explorer do not look for these streams, and the sizes of any alternate streams are not shown or returned by standard APIs such as `GetFileSize`. 

Stream names are specified with a colon after the file name before the stream name itself. 

For example, the file name **myfile.txt:mystream** points to an alternate stream named **mystream** within the file **myfile.txt**.

Alternate streams can be created
with the command interpreter as the following example shows:
```cmd
C:\temp>echo hello > hello.txt:mystream

C:\Temp>dir hello.txt
  Volume in drive C is OS
  Volume Serial Number is 1707-9837

  Directory of C:\Temp

22-May-19 11:33 0 hello.txt 1 File(s) 0 bytes
```

Notice the zero size file. Trying to use the type command fails:
```cmd
C:\Temp>type hello.txt:mystream
The filename, directory name, or volume label syntax is incorrect.
```

The type command interpreter does not recognize stream names. 

Can use the SysInternals tool
Streams.exe to list the names and sizes of alternate streams in files. 
Here is the command with hello.txt file:
```cmd
C:\Temp>streams -nobanner hello.txt
C:\Temp\hello.txt:
:mystream:$DATA 8
```

The alternate stream content is not shown. 

Of course alternate streams can be created programmatically by passing the stream name at the end
of the filename after a colon, to  `CreateFile`. 

Here is an example (error handling omitted):
```C++
HANDLE hFile = ::CreateFile(L"c:\\temp\\myfile.txt:stream1",GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, 0, nullptr);

char data[] = "Hello, from a stream";
DWORD bytes;
::WriteFile(hFile, data, sizeof(data), &bytes, nullptr);
::CloseHandle(hFile);
```

## RAII FLT_FILE_NAME_INFORMATION wrapper
As discussed in the previous section, calling `FltGetFileNameInformation` requires calling its opposite function, `FltReleaseFileNameInformation`.

This naturally leads to the possibility of having a RAII wrapper to take care of this, making the surrounding code simpler and less error prone. 

Here is one possible declaration for such a wrapper:
```C++
enum class FileNameOptions {
  Normalized = FLT_FILE_NAME_NORMALIZED,
  Opened = FLT_FILE_NAME_OPENED,
  Short = FLT_FILE_NAME_SHORT,

  QueryDefault = FLT_FILE_NAME_QUERY_DEFAULT,
  QueryCacheOnly = FLT_FILE_NAME_QUERY_CACHE_ONLY,
  QueryFileSystemOnly = FLT_FILE_NAME_QUERY_FILESYSTEM_ONLY,

  RequestFromCurrentProvider = FLT_FILE_NAME_REQUEST_FROM_CURRENT_PROVIDER,
  DoNotCache = FLT_FILE_NAME_DO_NOT_CACHE,
  AllowQueryOnReparse = FLT_FILE_NAME_ALLOW_QUERY_ON_REPARSE
};
DEFINE_ENUM_FLAG_OPERATORS(FileNameOptions);

struct FilterFileNameInformation {
  FilterFileNameInformation(PFLT_CALLBACK_DATA data, FileNameOptions options = FileNameOptions::QueryDefault | FileNameOptions::Normalized);

  ~FilterFileNameInformation();

  operator bool() const {
    return _info != nullptr;
  }

  operator PFLT_FILE_NAME_INFORMATION() const {
    return Get();
  }

  PFLT_FILE_NAME_INFORMATION operator->() {
    return _info;
  }
  
  NTSTATUS Parse();

private:
  PFLT_FILE_NAME_INFORMATION _info;
};
```

The non-inline functions are defined below:
```C++
FilterFileNameInformation::FilterFileNameInformation(
  PFLT_CALLBACK_DATA data, FileNameOptions options) {
  auto status = FltGetFileNameInformation(data,(FLT_FILE_NAME_OPTIONS)options, &_info);

  if (!NT_SUCCESS(status))
    _info = nullptr;
} 

FilterFileNameInformation::~FilterFileNameInformation() {
  if (_info)
    FltReleaseFileNameInformation(_info);
}

NTSTATUS FilterFileNameInformation::Parse() {
  return FltParseFileNameInformation(_info);
}
```

Using this wrapper can be something like the following:
```C++
FilterFileNameInformation nameInfo(Data);
if(nameInfo) { // operator bool()
  if(NT_SUCCESS(nameInfo.Parse())) {
    KdPrint(("Final component: %wZ\n", &nameInfo->FinalComponent));
  }
}
```

## The Alternate Delete Protector Driver
Create an alternative Delete Protector driver, that will be protecting file deletions from certain directories (regardless of the calling process), rather than basing the decision on the caller process or image file.

First, manage directories to protect (rather than process image file names as in the earlier driver). 

A complication arises because user mode client will use directories in the form **c:\somedir**; that is, paths based on symbolic links. 

Need to somehow convert DOS-style
names to NT-style names (another common referral to internal
device names).

To this end, list of protected directories would have each directory in the two forms, ready to
be used where appropriate. 

Here is the structure definition:
```C++
struct DirectoryEntry {
  UNICODE_STRING DosName;
  UNICODE_STRING NtName;

  void Free() {
    if (DosName.Buffer) {
      ExFreePool(DosName.Buffer);
      DosName.Buffer = nullptr;
    }

    if (NtName.Buffer) {
      ExFreePool(NtName.Buffer);
      NtName.Buffer = nullptr;
    }
  }
};
```

Since driver will allocate these strings dynamically, need to free them eventually. 

The above code adds a Free method that frees the internal string buffers. 

The choice of using `UNICODE_STRING`
rather than raw C-strings or even a constant size string is somewhat arbitrary, but it should be
appropriate for the driver’s requirements. 

In this case, driver go with `UNICODE_STRING` because the strings themselves can be allocated dynamically and some APIs work with `UNICODE_STRING` directly.

Store an array of these structures and manage it in a similar way as was done in the previous driver:
```C++
const int MaxDirectories = 32;

DirectoryEntry DirNames[MaxDirectories];
int DirNamesCount;
FastMutex DirNamesLock;
```

The I/O control codes of the previous driver have changed meaning: they should allow adding and removing directories, which are also strings of course. 
Here are the updated definitions:
```C++
#define IOCTL_DELPROTECT_ADD_DIR \
CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DELPROTECT_REMOVE_DIR \
CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DELPROTECT_CLEAR \
CTL_CODE(0x8000, 0x802, METHOD_NEITHER, FILE_ANY_ACCESS)
```

Next, need to implement these add/remove/clear operations, starting with add. 

The first part is making some sanity checks for the input string:
```C++
case IOCTL_DELPROTECT_ADD_DIR:
{
  auto name = (WCHAR*)Irp->AssociatedIrp.SystemBuffer;
  if (!name) {
    status = STATUS_INVALID_PARAMETER;
    break;
  }

  auto bufferLen = stack->Parameters.DeviceIoControl.InputBufferLength;
  if (bufferLen > 1024) {
    // just too long for a directory
    status = STATUS_INVALID_PARAMETER;
    break;
  }

  // make sure there is a NULL terminator somewhere
  name[bufferLen / sizeof(WCHAR) - 1] = L'\0';
  auto dosNameLen = ::wcslen(name);
  if (dosNameLen < 3) {
    status = STATUS_BUFFER_TOO_SMALL;
    break;
  }
}
```

Check buffer if it already exists in array, and if so, no need to add it again. 

Create a helper function to do this lookup:
```C++
int FindDirectory(PCUNICODE_STRING name, bool dosName) {
  if (DirNamesCount == 0)
    return -1;

  for (int i = 0; i < MaxDirectories; i++) {
    const auto& dir = dosName ? DirNames[i].DosName : DirNames[i].NtName;
    if (dir.Buffer && RtlEqualUnicodeString(name, &dir, TRUE))
      return i;
  }
  return -1;
}
```

The function goes over the array, looking for a match with the input string. 

The boolean parameter indicates whether the routine compares the DOS name or the NT name. 

Use `RtlEqualUnicodeString` to check for equality, specifying case insensitive test. 

The function returns the index in which the string was found, or -1 otherwise. 

Note the function does not acquire
any lock, so it’s up to the caller to call the function with proper synchronization.

Directory handler can now search for the input directory string and just move on if found:
```C++
AutoLock locker(DirNamesLock);

UNICODE_STRING strName;
RtlInitUnicodeString(&strName, name);
if (FindDirectory(&strName, true) >= 0) {
  // found it, just continue and return success
  break;
}
```

With the fast mutex acquired, directories array can be safely  accessed. 

If the string is not found, there is a new directory to add to the array. 

First, make sure the array is not exhausted:
```C++
if (DirNamesCount == MaxDirectories) {
  status = STATUS_TOO_MANY_NAMES;
  break;
}
```

Traverse the array and look for an empty slot (where the DOS string’s buffer pointer is NULL). 

Once find it, add the DOS name and do something to convert it to an NT
name.

```C++
for (int i = 0; i < MaxDirectories; i++) {
  if (DirNames[i].DosName.Buffer == nullptr) {
    // allow space for trailing backslash and NULL terminator
    auto len = (dosNameLen + 2) * sizeof(WCHAR);
    auto buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool, len, DRIVER_TAG);
    if (!buffer) {
      status = STATUS_INSUFFICIENT_RESOURCES;
      break;
    }
    ::wcscpy_s(buffer, len / sizeof(WCHAR), name);

    // append a backslash if it's missing
    if (name[dosNameLen - 1] != L'\\')
      ::wcscat_s(buffer, dosNameLen + 2, L"\\");

    status = ConvertDosNameToNtName(buffer, &DirNames[i].NtName);

    if (!NT_SUCCESS(status)) {
      ExFreePool(buffer);
      break;
    }

    RtlInitUnicodeString(&DirNames[i].DosName, buffer);
    KdPrint(("Add: %wZ <=> %wZ\n", &DirNames[i].DosName, &DirNames[i].NtName));
    ++DirNamesCount;
    break;
  }
}
```

The code is fairly straightforward with the exception of the call to `ConvertDosNameToNtName`. 

This is not a built-in function, but something we need to implement ourselves. 

Here is its declaration:
```C++
  NTSTATUS ConvertDosNameToNtName(_In_ PCWSTR dosName, _Out_ PUNICODE_STRING ntName);
```

How to convert the DOS name to NT name? 

Since **C:** and similar are the symbolic links, one approach would be to lookup the symbolic link and find its target, which is the NT name. 

Basic checks:
```C++
ntName->Buffer = nullptr; // in case of failure
auto dosNameLen = ::wcslen(dosName);

if (dosNameLen < 3)
  return STATUS_BUFFER_TOO_SMALL;

// make sure we have a driver letter
if (dosName[2] != L'\\' || dosName[1] != L':')
  return STATUS_INVALID_PARAMETER;
```

Expect a directory in the form “X:\…”, meaning a drive letter, colon, backslash, and the rest of the path. 

Need to build the symbolic link, residing under the \??\ Object Manager directory. 

Use string manipulation functions to create the full string. 

Following code snippet use a type called kstring, which is a string wrapper, similar in concept to the standard C++ std::wstring. 

Its API is not the same, but fairly readable. 

Start with the base symbolic link directory and add the drive letter provided:
```C++
kstring symLink(L"\\??\\");
symLink.Append(dosName, 2); // driver letter and colon
```

Now need to open the symbolic link using `ZwOpenSymbolicLinkObject`.

For this purpose, need to prepare an `OBJECT_ATTRIBUTES` structure common to many open-style APIs where some kind of name is required:
```C++
UNICODE_STRING symLinkFull;
symLink.GetUnicodeString(&symLinkFull);
OBJECT_ATTRIBUTES symLinkAttr;
InitializeObjectAttributes(&symLinkAttr, &symLinkFull,OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, nullptr, nullptr);
```

`GetUnicodeString` is a kstring helper that initializes a UNICODE_STRING based on a kstring.

This is necessary because `OBJECT_ATTRIBUTES` requires a UNICODE_STRING for the name. 

Initializing `OBJECT_ATTRIBUTES` is accomplished with the `InitializeObjectAttributes` macro,
requiring the following arguments (in order):
- pointer to `OBJECT_ATTRIBUTES` structure to initialize
- name of the object
- set of flags. In this case the handle returned would be a kernel handle and the lookup will be
case insensitive
- optional handle to a root directory in case the name is relative, rather than absolute NULL
- optional security descriptor to apply to the object (if created and not opened).

Once the structure is initialized, it is ready to call `ZwOpenSymbolicLinkObject`:
```C++
HANDLE hSymLink = nullptr;
auto status = STATUS_SUCCESS;
do {
  // open symbolic link
  status = ZwOpenSymbolicLinkObject(&hSymLink, GENERIC_READ, &symLinkAttr);
  if (!NT_SUCCESS(status))
    break;
```

Use the well-known do/while(false) scheme to cleanup the handle if it’s valid.

`ZwOpenSymbolicLinkObject` accepts an output HANDLE, an access mask (`GENERIC_READ` means to read information) and the attributes we prepared earlier. 

This call can certainly fail, if
the provided drive letter does not exist, for instance.

If the call succeeds, need to read the target this symbolic link object points to. 

The function to get this info is `ZwQuerySymbolicLinkObject`. 

Need to prepare a UNICODE_STRING large enough to hold the result (this is also our output parameter from the conversion function):
```C++
  USHORT maxLen = 1024; // arbitrary
  ntName->Buffer = (WCHAR*)ExAllocatePool(PagedPool, maxLen);
  if (!ntName->Buffer) {
    status = STATUS_INSUFFICIENT_RESOURCES;
    break;
  }
  ntName->MaximumLength = maxLen;

  // read target of symbolic link
  status = ZwQuerySymbolicLinkObject(hSymLink, ntName, nullptr);
  if (!NT_SUCCESS(status))
    break;
} while (false);
```

Once the do/while block is exited, need to free the allocated buffer if anything failed. 

Otherwise, append the rest of the input directory to the target NT name:
```C++
if (!NT_SUCCESS(status)) {
  if (ntName->Buffer) {
    ExFreePool(ntName->Buffer);
    ntName->Buffer = nullptr;
  }
}
else {
  RtlAppendUnicodeToString(ntName, dosName + 2); // directory part
}
```

Finally, close the symbolic link handle if opened successfully:
```C++
if (hSymLink)
  ZwClose(hSymLink);
  return status;
}
```

Make similar checks on the provided path and then look it up by its DOS name to remove a protected directory. 

If found, remove it from the array:
```C++
AutoLock locker(DirNamesLock);
UNICODE_STRING strName;
RtlInitUnicodeString(&strName, name);
int found = FindDirectory(&strName, true);
if (found >= 0) {
  DirNames[found].Free();
  DirNamesCount--;
}
else {
  status = STATUS_NOT_FOUND;
}
break;
```

## Handling Pre-Create and Pre-Set Information
With above infrastructure in place, it is able to implement the
pre-callbacks, so that any file in one of the protected directories would not be deleted, regardless
of the calling process. 

In both callbacks to get the file name to be deleted and locate its
directory in our array of directories. 

Create a helper function for this purpose, declared like so:
```C++
bool IsDeleteAllowed(_In_ PFLT_CALLBACK_DATA Data);
```

The first thing to do is get the file name (in this code we will not be using the wrapper introduced earlier to make the API calls more apparent):
```C++
PFLT_FILE_NAME_INFORMATION nameInfo = nullptr;
auto allow = true;
do {
  auto status = FltGetFileNameInformation(Data,FLT_FILE_NAME_QUERY_DEFAULT | FLT_FILE_NAME_NORMALIZED, &nameInfo);

  if (!NT_SUCCESS(status))
    break;

  status = FltParseFileNameInformation(nameInfo);
  if (!NT_SUCCESS(status))
    break;
```

Get the file name information and then parse it, since the volume and parent directory are required
only. 

Build a UNICODE_STRING that concatenates these three factors:
```C++
// concatenate volume+share+directory
UNICODE_STRING path;
path.Length = path.MaximumLength = nameInfo->Volume.Length + nameInfo->Share.Length + nameInfo->ParentDir.Length;
path.Buffer = nameInfo->Volume.Buffer;
```

Since the full file path is contiguous in memory, the buffer pointer starts at the first component(volume) and the length must be calculated appropriately.

All that’s left to do now is call
FindDirectory to locate (or fail to locate) this directory:
```C++
  AutoLock locker(DirNamesLock);
  if (FindDirectory(&path, false) >= 0) {
    allow = false;
    KdPrint(("File not allowed to delete: %wZ\n", &nameInfo->Name));
  }
} while (false);
```

Finally, release the file name information:
```C++
  if (nameInfo)
    FltReleaseFileNameInformation(nameInfo);
  return allow;
}
```

Back to pre-callbacks:
```C++
_Use_decl_annotations_
FLT_PREOP_CALLBACK_STATUS DelProtectPreCreate(PFLT_CALLBACK_DATA Data,
  PCFLT_RELATED_OBJECTS, PVOID*) {
  if (Data->RequestorMode == KernelMode)
    return FLT_PREOP_SUCCESS_NO_CALLBACK;

  auto& params = Data->Iopb->Parameters.Create;

  if (params.Options & FILE_DELETE_ON_CLOSE) {
    // delete operation
    KdPrint(("Delete on close: %wZ\n", &FltObjects->FileObject->FileName));

    if (!IsDeleteAllowed(Data)) {
      Data->IoStatus.Status = STATUS_ACCESS_DENIED;
      return FLT_PREOP_COMPLETE;
    }
  }
  return FLT_PREOP_SUCCESS_NO_CALLBACK;
}
```

And pre-set information which is very similar:
```C++
_Use_decl_annotations_
FLT_PREOP_CALLBACK_STATUS DelProtectPreSetInformation(PFLT_CALLBACK_DATA Data,
PCFLT_RELATED_OBJECTS, PVOID*) {

  if (Data->RequestorMode == KernelMode)
    return FLT_PREOP_SUCCESS_NO_CALLBACK;

  auto& params = Data->Iopb->Parameters.SetFileInformation;

  if (params.FileInformationClass != FileDispositionInformation &&
params.FileInformationClass != FileDispositionInformationEx) {
    // not a delete operation
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
  }

  auto info = (FILE_DISPOSITION_INFORMATION*)params.InfoBuffer;
  if (!info->DeleteFile)
    return FLT_PREOP_SUCCESS_NO_CALLBACK;

  if (IsDeleteAllowed(Data))
    return FLT_PREOP_SUCCESS_NO_CALLBACK;

  Data->IoStatus.Status = STATUS_ACCESS_DENIED;
  return FLT_PREOP_COMPLETE;
}
```

## Testing the Driver
The configuration client has been updated to send the updated control codes, although the code is
very similar to the earlier one since sending strings in add/remove is just like before (the project is
named DelProtectConfig3 in the source). 

Here are some tests:
```cmd
c:\book>fltmc load delprotect3
c:\book>delprotectconfig3 add c:\users\pavel\pictures
Success!
c:\book>del c:\users\pavel\pictures\pic1.jpg
c:\users\pavel\pictures\pic1.jpg
Access is denied.
```

## Contexts
In some scenarios it is desirable to attach some data to file system entities such as volumes and files.

The filter manager provides this capability through contexts. 

A context is a data structure provided by the mini-filter driver that can be set and retrieved for any file system object. 

These contexts are connected to the objects they are set on, for as long as these objects are alive.

To use contexts, the driver must declare before hand what contexts it may require and for what type of objects. 

This is done as part of the registration structure `FLT_REGISTRATION`. 

The ContextRegistration field may point to an array of `FLT_CONTEXT_REGISTRATION` structures,
each of which defines information for a single context.

`FLT_CONTEXT_REGISTRATION` is declared as follows:
```C++
typedef struct _FLT_CONTEXT_REGISTRATION {
  FLT_CONTEXT_TYPE ContextType;
  FLT_CONTEXT_REGISTRATION_FLAGS Flags;
  PFLT_CONTEXT_CLEANUP_CALLBACK ContextCleanupCallback;
  SIZE_T Size;
  ULONG PoolTag;
  PFLT_CONTEXT_ALLOCATE_CALLBACK ContextAllocateCallback;
  PFLT_CONTEXT_FREE_CALLBACK ContextFreeCallback;
  PVOID Reserved1;
} FLT_CONTEXT_REGISTRATION, *PFLT_CONTEXT_REGISTRATION;
```

Here is a description of the above fields:
- `ContextType` identifies the object type this context would be attached to. The `FLT_CONTEXT_TYPE` is typedefed as USHORT and can have one of the following values:
```C++
#define FLT_VOLUME_CONTEXT 0x0001
#define FLT_INSTANCE_CONTEXT 0x0002
#define FLT_FILE_CONTEXT 0x0004
#define FLT_STREAM_CONTEXT 0x0008
#define FLT_STREAMHANDLE_CONTEXT 0x0010
#define FLT_TRANSACTION_CONTEXT 0x0020
#if FLT_MGR_WIN8
#define FLT_SECTION_CONTEXT 0x0040
#endif // FLT_MGR_WIN8
#define FLT_CONTEXT_END 0xffff
```

As can be seen from the above definitions, a context can be attached to a volume, filter instance, file, stream, stream handle, transaction and section. 

The last value is a sentinel
for indicating this is the end of the list of context definitions.

The aside **Context Types** contains
more information on the various context types.

## Context Types
The filter manager supports several types of contexts:
- Volume contexts are attached to volumes, such as a disk partition (C:, D:, etc.).
- Instance contexts are attached to filter instances. A mini-filter can have several instances running, each attached to a different volume.
- File contexts can be attached to files in general (and not a specific file stream).
- Stream contexts can be attached to file streams, supported by some file systems, such as NTFS.
File systems that support a single stream per file (such as FAT) treat stream contexts as file
contexts.
- Stream handle contexts can be attached to a stream on a per `FILE_OBJECT`.
- Transaction contexts can be attached to a transaction that is in progress. Specifically, the
NTFS file system supports transactions, and such so a context can be attached to a running
transaction.
- Section contexts can be attached to section (file mapping) objects created with
`FltCreateSectionForDataScan`.

Context size can be fixed or variable. 

If fixed size is desired, it’s specified in the Size field of
`FLT_CONTEXT_REGISTRATION`. 

For a variable sized context, a driver specifies the special value
`FLT_VARIABLE_SIZED_CONTEXTS` (-1).

Using fixed-size contexts is more efficient, because the filter manager can use lookaside lists for managing allocations and deallocations.

The pool tag is specified with the PoolTag field of `FLT_CONTEXT_REGISTRATION`. 

This is the tag the filter manager will use when actually allocating the context. 

The next two fields are optional
callbacks where the driver provides the allocation and deallocation functions. 

If these are non-NULL, then the PoolTag and Size fields are meaningless and not used.

Here is an example of building an array of context registration structure:
```C++
struct FileContext {
  //...
};

const FLT_CONTEXT_REGISTRATION ContextRegistration[] = {
  { FLT_FILE_CONTEXT, 0, nullptr, sizeof(FileContext), 'torP',
  nullptr, nullptr, nullptr },
  { FLT_CONTEXT_END }
};
```

## Managing Contexts
To actually use a context, a driver first needs to allocate it by calling `FltAllocateContext`, defined like so:
```C++
NTSTATUS FltAllocateContext (
  _In_ PFLT_FILTER Filter,
  _In_ FLT_CONTEXT_TYPE ContextType,
  _In_ SIZE_T ContextSize,
  _In_ POOL_TYPE PoolType,
  _Outptr_ PFLT_CONTEXT *ReturnedContext);
```

The Filter parameter is the filter’s opaque pointer returned by FltRegisterFilter but also available in the `FLT_RELATED_OBJECTS` structure provided to all callbacks. 

ContextType is one of the
supported context macros shown earlier, such as `FLT_FILE_CONTEXT`. 

ContextSize is the requested
context size in bytes (must be greater than zero). 

PoolType can be PagedPool or NonPagedPool, depending on what IRQL the driver is planning to access the context (for volume contexts, NonPagedPool must be specified). 

Finally, the ReturnedContext field stores the returned allocated
context; PFLT_CONTEXT is typedefed as PVOID.

Once the context has been allocated, the driver can store in that data buffer anything it wishes.

Then it must attach the context to an object (this is the reason to create the context in the first place) using one of several functions named `FltSetXxxContext` where “Xxx” is one of File, Instance, Volume, Stream, StreamHandle, or Transaction. 

The only exception is a section context which is set with `FltCreateSectionForDataScan`. 

Each of the `FltSetXxxContext` functions has the same
generic makeup, shown here for the File case:
```C++
NTSTATUS FltAllocateContext (
  _In_ PFLT_FILTER Filter,
  _In_ FLT_CONTEXT_TYPE ContextType,
  _In_ SIZE_T ContextSize,
  _In_ POOL_TYPE PoolType,
  _Outptr_ PFLT_CONTEXT *ReturnedContext);
```

The Filter parameter is the filter’s opaque pointer returned by FltRegisterFilter but also
available in `FLT_RELATED_OBJECTS` structure provided to all callbacks. 

ContextType is one of the
supported context macros shown earlier, such as `FLT_FILE_CONTEXT`. 

ContextSize is the requested
context size in bytes (must be greater than zero). 

PoolType can be PagedPool or NonPagedPool, depending on what IRQL the driver is planning to access the context (for volume contexts, NonPagedPool must be specified). 

Finally, the ReturnedContext field stores the returned allocated
context; PFLT_CONTEXT is typedefed as PVOID.

Once the context has been allocated, the driver can store in that data buffer anything it wishes.

Then it must attach the context to an object (this is the reason to create the context in the first place) using one of several functions named `FltSetXxxContext` where “Xxx” is one of File, Instance, Volume, Stream, StreamHandle, or Transaction. 

The only exception is a section context which is set with `FltCreateSectionForDataScan`. 

Each of the `FltSetXxxContext` functions has the same generic makeup, shown here for the File case:
```C++
NTSTATUS FltSetFileContext (
  _In_ PFLT_INSTANCE Instance,
  _In_ PFILE_OBJECT FileObject,
  _In_ FLT_SET_CONTEXT_OPERATION Operation,
  _In_ PFLT_CONTEXT NewContext,
  _Outptr_ PFLT_CONTEXT *OldContext);
```

The function accepts the required parameters for the context at hand.

In this file case it’s the instance
(actually needed in any set context function) and the file object representing the file that should carry this context. 

The Operation parameter can be either `FLT_SET_CONTEXT_REPLACE_IF_EXISTS` or `FLT_SET_CONTEXT_KEEP_IF_EXISTS`, which are pretty self explanatory.






310




