# Chapter 10: Introduction to File System Mini-Filters

File systems are targets for I/O operations to access files. 

Windows supports several file systems, most notably NTFS, its native file system. 

File system filtering is the mechanism by which drivers can intercept calls destined to the file system. 

A newer model called file system mini-filters was developed to replace the legacy filter mechanism. 

Mini-filters are easier to write in many respects, and are the preferred way to develop file system filtering drivers.

## Introduction
Legacy filters cannot be unloaded while the system is running which means the system had to be restarted to load an update version of the driver. 

With the mini-filter model, drivers can be loaded and unloaded dynamically.

Internally, legacy filter provided by Windows called the Filter Manager is tasked with managing mini-filters. 

Each mini-filter has its own Altitude, which determines its relative position in the device stack. 

Filter manager is the one receiving IRPs just like any other legacy filter and then calls upon the mini-filters it’s managing, in descending order of altitude.

## Loading and Unloading
Mini-filter drivers must be loaded just like any other driver. 

User mode API to use is [FilterLoad](https://learn.microsoft.com/en-us/windows/win32/api/fltuser/nf-fltuser-filterload), passing driver’s name (its key in the registry `atHKLM\System\CurrentControlSet\Services\drivername`).

Internally, [FltLoadFilter](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/fltkernel/nf-fltkernel-fltloadfilter) is invoked with the same semantics. 

Just like any other driver, `SeLoadDriverPrivilege` must be present in caller’s token if called from user mode.

By default, it’s present in admin-level tokens, but not in standard users tokens.

Unloading a mini-filter is accomplished with `FilterUnload` in user mode, or `FltUnloadFilter` in kernel mode. 

This operation requires the same privilege as for loads, but is not guaranteed to succeed, because mini-filter’s Filter unload callback is called, which can fail the request so that driver remains in the system.

Loading driver with fltmc.exe is done with the load option, like so: `fltmc load myfilter`

Unloading is done with the unload command line option: `fltmc unload myfilter`

## Initialization
A file system mini-filter driver has `DriverEntry` routine, just like any other driver. 

The driver must register itself as a mini-filter with filter manager, specifying various settings, such as what operations it wishes to intercept. 

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

This abstraction provided by the filter manager helps to isolate the mini-filter from knowing the exact source of the operation: it could be a real IRP or it could be another operation that is abstracted as an IRP. 

Furthermore, file systems support yet another mechanism for receiving requests, known as Fast I/O. 

Fast I/O is used for synchronous I/O with cached files. 

Fast I/O requests transfer data between user buffers and the system cache directly, bypassing the file system and storage driver stack, thus avoiding unnecessary overhead.

The NTFS file system driver, as a canonical example, supports Fast I/O.

The filter manager abstracts I/O operations, regardless of whether they are IRP-based or fast I/O based. 

Mini-filters can intercept any such request. 

If the driver is not interested in fast I/O, for example, it can query the actual request type provided by the filter manager with `FLT_IS_FASTIO_OPERATION` and/or `FLT_IS_IRP_OPERATION` macros

The second field in `FLT_OPERATION_REGISTRATION` is a set of flags which can be zero or a combination of one of the following flags affecting read and write operations:
- `FLTFL_OPERATION_REGISTRATION_SKIP_CACHED_IO`: do not invoke the callback(s) if it’s cached I/O (such as fast I/O operations, which are always cached).
- `FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO`: do not invoke the callback(s) for paging I/O (IRP-based operations only).
- `FLTFL_OPERATION_REGISTRATION_SKIP_NON_DASD_IO`: do not invoke the callback(s) for DAX volumes.

## The Altitude
File system mini-filters must have an altitude, indicating their relative **position** within the file system filters hierarchy.

First, altitude value is not provided as part of mini-filter’s registration, but is read from the registry. 

When the driver is installed, its altitude is written in the proper location in the registry.

Here is an example that should clarify why altitude matters. 

Suppose there is a mini-filter at altitude 10000 whose job is to encrypt data when written, and decrypt when read. 

Now suppose another minifilter whose job is to check data for malicious activity is at altitude 9000.

The encryption driver encrypts incoming data to be written, which is then passed on to the antivirus driver. 

The anti-virus driver is in a problem, as it sees the encrypted data with no viable way of decrypting it. 

In such a case, the anti-virus driver must have an altitude higher than the encryption driver. 

To rectify this situation, Microsoft has defined ranges of altitudes for drivers based on their requirements. 

In order to obtain a proper altitude, the driver publisher must send an email to Microsoft and ask an altitude be allocated for that driver based on its intended target. 

## Installation
The **proper** way to install a file system mini-filter is to use an INF file.

## INF Files
INF files are the classic mechanism used to install hardware based device drivers, but these can be
used to install any type of driver.

**File System Mini-Filter** project templates provided by WDK creates such an INF file, which is almost ready for installation.

INF files use the old INI file syntax, where there are sections in square brackets, and underneath a section there are entries in the form **key=value**. 

These entries are instructions to the installer that parses the file, essentially instructing the installer to do two types of operations: copy files to specific
locations and making changes to the registry.

## Installing the Driver
Once the INF file is properly modified, and the driver code compiles, it is ready to be installed. 

The simplest way to install is to copy the driver package (SYS, INF and CAT files) to the target system, and then right click the INF file in Explorer and select Install. 

This will **run** the INF, executing the required operations.

At this point, the mini-filter is installed, and can be loaded with the fltmc command line tool:
`c:\>fltmc load <name>`

## Processing I/O Operations
The main function of a file system mini-filter is processing I/O operations by implementing pre and/or post callbacks for the operations of interest. 

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
1. `FLTFL_CALLBACK_DATA_DIRTY`: indicates the driver has made changes to the structure and then called `FltSetCallbackDataDirty`. Every member of the structure can be modified except Thread and RequestorMode.
2. `FLTFL_CALLBACK_DATA_FAST_IO_OPERATION`: indicates this is a fast I/O operation.
3. `FLTFL_CALLBACK_DATA_IRP_OPERATION`: indicates this is an IRP-based operation.
4. `FLTFL_CALLBACK_DATA_GENERATED_IO`: indicates this is an operation generated by another mini-filter.
5. `FLTFL_CALLBACK_DATA_POST_OPERATION`: indicates this is a post-operation, rather than a pre-operation.
- Thread is an opaque pointer to the thread requesting this operation.
- `IoStatus` is the status of the request. A pre-operation can set this value and then indicate the operation is complete by returning `FLT_PREOP_COMPLETE`. A post-operation can look at the final status of the operation.
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
1. TargetFileObject is the file object that is the target of this operation; it’s useful to have when invoking some APIs.
2. Parameters is a monstrous union providing the actual data for the specific information (similar to Paramters member of `IO_STACK_LOCATION`). The driver looks at the proper structure within this union to get to the information it needs.

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

The post-operation function is called at `IRQL <= DISPATCH_LEVEL` in an arbitrary thread context, unless the pre-callback routine returned `FLT_PREOP_SYNCHRONIZE`, in which case the filter manager guarantees the post-callback is invoked at `IRQL < DISPATCH_LEVEL` on the same thread that executed the pre-callback.

In the former case, the driver cannot perform certain types of operations because IRQL is too high:
- Cannot access paged memory.
- Cannot use kernel APIs that only work at `IRQL < DISPATCH_LEVEL`.
- Cannot acquire synchronization primitives such as mutexes, fast mutexes, executive resources, semaphores, events, etc.
- Cannot set, get or delete contexts, but it can release contexts

If the driver needs to do any of the above, it somehow must defer its execution to another routine called at `IRQL < DISPATCH_LEVEL`. This can be done in one of two ways:
1. The driver calls `FltDoCompletionProcessingWhenSafe` which sets up a callback function that is invoked by a system worker thread at `IRQL < DISPATCH_LEVEL` (if the post-operation was called at `IRQL = DISPATCH_LEVEL`).
2. The driver posts a work item by calling `FltQueueDeferredIoWorkItem`, which queues a work item that will eventually execute by a system worker thread at `IRQL = PASSIVE_LEVEL`. In the work item callback, the driver will eventually call `FltCompletePendedPostOperation` to signal the filter manager that the post-operation is complete.

Although using `FltDoCompletionProcessingWhenSafe` is easier, it has some limitations that prevent it from being used in some scenarios:
- Cannot be used for `IRP_MJ_READ`, `IRP_MJ_WRITE` or `IRP_MJ_FLUSH_BUFFERS` because it can cause deadlock if these operations are completed synchronously by lower layer.
- Can only be called for IRP-based operations (can check with `FLT_IS_IRP_OPERATION`).

The returned value from pos-callback is usually `FLT_POSTOP_FINISHED_PROCESSING` to indicate the driver is finished with this operation. 

However, if the driver needs to perform work in a work item (high IRQL), the driver can return `FLT_POSTOP_MORE_PROCESSING_REQUIRED` to tell filter manager the operation is still pending completion, and in the work item call `FltCompletePendedPostOperation` to let filter manager know it can continue processing this request.

## The Delete Protector Driver
Create driver to protect certain files from deletion by certain processes. 

DriverEntry provided by the project template already has the mini-filter registration code in place. 

Need to tweak the callbacks to indicate which are the ones interested in. 

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

## Testing the Modified Driver
There are three ways to delete a file with user mode APIs:
1. Call `DeleteFile`.
2. Call `CreateFile` with `FILE_FLAG_DELETE_ON_CLOSE`.
3. Call `SetFileInformationByHandle` on an open file

Internally, there are only two ways to delete a file: `IRP_MJ_CREATE` with the `FILE_DELETE_ON_CLOSE` flag and `IRP_MJ_SET_INFORMATION` with `FileDispositionInformation`. 

Clearly, in the above list, item (2) corresponds to the first option and item (3) corresponds to the second option. 

The only mystery left is `DeleteFile`: how does it delete a file?

From the driver’s perspective it does not matter at all, since it must map to one of the two options the driver handles. 

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

To offset this issues, the filter manager provides `FltGetFileNameInformation` that can return the correct file name when needed. 

This function is prototyped as follows:
```C++
NTSTATUS FltGetFileNameInformation (
  _In_ PFLT_CALLBACK_DATA CallbackData,
  _In_ FLT_FILE_NAME_OPTIONS NameOptions,
  _Outptr_ PFLT_FILE_NAME_INFORMATION *FileNameInformation);
```

`CallbackData` parameter is the one provided by filter manager in any callback. 

`NameOptions` parameter is a set of flags that specify the requested file format.

Typical value used by most drivers is `FLT_FILE_NAME_NORMALIZED` (full path name) ORed with `FLT_FILE_NAME_QUERY_DEFAULT` (locate the name in a cache, otherwise query the file system).

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

The main ingredients are the several `UNICODE_STRING` structures that should hold the various components of a file name.

Initially, only Name field is initialized to the full file name (depending on the flags used to query the file name information, “full” may be a partial name). 

If the request specified the flag `FLT_FILE_NAME_NORMALIZED`, then Name points to the full path name, in device form. 

Device form means that file such as `c:\mydir\myfile.txt` is stored with the internal device name to which **C:** maps to, such as `\Device\HarddiskVolume3\mydir\myfile.txt`. 

This makes the driver’s job a bit more complicated if it somehow depends on paths provided by user mode.

Since only the full name is provided by default (Name field), it’s often necessary to split the full path to its constituents.

Fortunately, the filter manager provides such a service with
`FltParseFileNameInformation`. 

This one takes `FLT_FILE_NAME_INFORMATION` object and fills in the other `UNICODE_STRING` fields in the structure.

## File Name Parts
As can be seen from `FLT_FILE_NAME_INFORMATION` declaration, there are several components that make up a full file name. 

Here is an example for the local file `c:\mydir1\mydir2\myfile.txt`:
The volume is the actual device name for which the symbolic link **C:** maps to. 

The share string is empty for local files (Length is zero). 

ParentDir is set to the directory only. 

In driver example that would be \mydir1\mydir2\ (not the trailing backslash). 

`FinalComponent` field stores the file name and stream name (if not using the default stream). 

The Stream component bares some explanation. 

Some file systems (most notable NTFS) provide the ability to have multiple data **streams** in a single file. 

Essentially, this means several files can be stored into a single **physical** file. 

In NTFS, for instance, file’s data is in fact one of its streams named **$DATA**, which is considered the default stream. 

But it’s possible to create/open another stream, that is stored in the same file, so to speak. 

Tools such as Windows Explorer do not look for these streams, and the sizes of any alternate streams are not shown or returned by standard APIs such as `GetFileSize`. 

Stream names are specified with a colon after the file name before the stream name itself. 

## Contexts
In some scenarios it is desirable to attach some data to file system entities such as volumes and files.

The filter manager provides this capability through contexts. 

A context is a data structure provided by the mini-filter driver that can be set and retrieved for any file system object. 

These contexts are connected to the objects they are set on, for as long as these objects are alive.

To use contexts, the driver must declare before hand what contexts it may require and for what type of objects. 

This is done as part of the registration structure `FLT_REGISTRATION`. 

The ContextRegistration field may point to an array of `FLT_CONTEXT_REGISTRATION` structures, each of which defines information for a single context.

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

The last value is a sentinel for indicating this is the end of the list of context definitions.

The aside **Context Types** contains more information on the various context types.

## Context Types
The filter manager supports several types of contexts:
- Volume contexts are attached to volumes, such as a disk partition (C:, D:, etc.).
- Instance contexts are attached to filter instances. A mini-filter can have several instances running, each attached to a different volume.
- File contexts can be attached to files in general (and not a specific file stream).
- Stream contexts can be attached to file streams, supported by some file systems, such as NTFS. File systems that support a single stream per file (such as FAT) treat stream contexts as file contexts.
- Stream handle contexts can be attached to a stream on a per `FILE_OBJECT`.
- Transaction contexts can be attached to a transaction that is in progress. Specifically, the NTFS file system supports transactions, and such so a context can be attached to a running transaction.
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

The next two fields are optional callbacks where the driver provides the allocation and deallocation functions. 

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

In this file case it’s the instance (actually needed in any set context function) and the file object representing the file that should carry this context. 

The Operation parameter can be either `FLT_SET_CONTEXT_REPLACE_IF_EXISTS` or `FLT_SET_CONTEXT_KEEP_IF_EXISTS`, which are pretty self explanatory.




310




