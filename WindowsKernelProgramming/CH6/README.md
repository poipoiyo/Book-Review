# Chapter 6: Kernel Mechanisms
## Interrupt Request Level
Example:
1. An I/O operation is carried out by a disk drive 
2. Once operation completes, disk drive notifies completion by requesting an interrupt. 
3. This interrupt is connected to Interrupt Controller hardware that sends request to a processor for handling. 
- Every hardware interrupt is associated with IRQL, determined by HAL. 
- The basic rule is that processor executes code with the highest IRQL.

Example:
1. CPU’s IRQL is zero at some point, and an interrupt with an associated IRQL of 5 comes in.
2. CPU will save its state (context) in the current thread’s kernel stack, raise its IRQL to 5 and then execute.
3. Once ISR completes, IRQL will drop to its previous level, resuming previous executed code as though interrupt didn’t exist.

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH6/6-1%20Basic%20interrupt%20dispatching.png" width="80%" />

The important IRQLs:
- `PASSIVE_LEVEL in WDK (0)`: normal IRQL for  CPU. User mode code always
runs at this level.
- `APC_LEVEL (1)`: used for special kernel APCs.
- `DISPATCH_LEVEL (2)`: things change radically. The scheduler cannot wake up on this CPU. Paged memory access is not allowed - such access causes a system crash. Since the scheduler cannot interfere, waiting on kernel objects is not allowed (causes a system crash if used).
- `Device IRQL`: a range of levels used for hardware interrupts (3~11 on x64, 3~26 on x86). All rules from IRQL 2 apply here as well.
- `Highest level (HIGH_LEVEL)`: mask all interrupts. Used by some
APIs dealing with linked list manipulation.

When IRQL is raised to 2 or higher, certain restrictions apply on executing code:
- Accessing memory not in physical memory is fatal and causes system crash. 
- Accessing data from non-paged pool is always safe
- Accessing data from paged pool or from user-supplied buffers is not safe and should be avoided.
- Waiting for any mutex or event causes system crash, unless the wait timeout is zero. 

## Raising and Lowering IRQL
In kernel mode, IRQL can be raised with [KeRaiseIrql](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-keraiseirql) and lowered back with [KeLowerIrql](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kelowerirql). 

### Example
```C++
// assuming current IRQL <= DISPATCH_LEVEL

KIRQL oldIrql; // typedefed as UCHAR
KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);

NT_ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);

// do work at IRQL DISPATCH_LEVEL

KeLowerIrql(oldIrql);
```

## Thread Priorities vs. IRQLs
- Thread priorities only have meaning at IRQL < 2. 
- Spending a lot of time at IRQL >= 2 is not a good thing, user mode code is not running for sure. 

## Deferred Procedure Calls
<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH6/6-2%20Typical%20IO%20request%20processing.png" width="80%" />

1. User mode thread opens a handle to a file, and issues read operation using `ReadFile` function. 
2. Driver receives request, and calls NTFS, which may call other drivers below it, until request reaches disk driver, which initiates the operation on actual hardware. 
3. When hardware is done with read operation, it issues an interrupt. 
4. This causes Interrupt Service Routine associated with interrupt to execute at Device IRQL.
5. Complete the initial request.

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH6/6-3%20Typical%20IO%20request%20processing.png" width="80%" />

1. Driver registered ISR prepares a DPC in advance, by allocating KDPC structure from non-paged pool and initializing with [KeInitializeDp](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-keinitializedpc).
2. When ISR is called before exiting function, ISR requests DPC to execute by queuing it using [KeInsertQueueDpc](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-keinsertqueuedpc).
3. When DPC function executes, it calls [IoCompleteRequest](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iocompleterequest). 
4. DPC serves as a compromise - running at IRQL `DISPATCH_LEVEL`, no scheduling can occur, no paged memory access, but not high enough to prevent hardware interrupts from coming in and being served on the same processor.

### IoCompleteRequest
- `IoCompleteRequest` can only be called at IRQL <= DISPATCH_LEVEL(2). 
- ISR cannot call `IoCompleteRequest` or it will crash the system. 
- The mechanism that allows ISR to call `IoCompleteRequest` as soon as possible is using a Deferred Procedure Call (DPC).
- A DPC is an object encapsulating function that is to be called at IRQL `DISPATCH_LEVEL`. 
- At this IRQL, calling `IoCompleteRequest` is permitted.

### KeInsertQueueDpc
- Queues DPC to current processor’s DPC queue. 
- When ISR returns, before IRQL drop back to zero, a check is made to see whethere DPCs exist in processor’s queue. 
- Processor drops to IRQL DISPATCH_LEVEL and DPCs in queue in a First In First Out manner, calling respective functions, until empty. 
- Only processor’s IRQL drop to zero, and resume executing original code that was disturbed.

## Using DPC with a Timer
- A kernel timer, represented by `KTIMER` structure allows setting up a timer to expire some time in the future. 
- This timer is a dispatcher object and so can be waited upon with `KeWaitForSingleObject`. 
- Waiting is inconvenient for a timer.
- A simpler approach is to call some callback when expires, which is what kernel timer provides using a DPC as its callback.

### Example
- Shows how to configure timer and DPC.
- When timer expires, DPC is inserted into DPC queue and executes. 
- Using DPC is more powerful than zero IRQL based callback, since it is guaranteed to execute before any user mode code.

```C
KTIMER Timer;
KDPC TimerDpc;

void InitializeAndStartTimer(ULONG msec) {
  KeInitializeTimer(&Timer);
  KeInitializeDpc(&TimerDpc, OnTimerExpired, nullptr); 

  // relative interval is in 100nsec units (and must be negative)
  // convert to msec by multiplying by 10000

  LARGE_INTEGER interval;
  interval.QuadPart = -10000LL * msec;
  KeSetTimer(&Timer, interval, &TimerDpc);
}

void OnTimerExpired(KDPC* Dpc, PVOID context, PVOID, PVOID) {
  UNREFERENCED_PARAMETER(Dpc);
  UNREFERENCED_PARAMETER(context);

  NT_ASSERT(KeGetCurrentIrql() == DISPATCH_LEVEL);

  // handle timer expiration
}
```

## Asynchronous Procedure Calls
- APCs are also data structures that encapsulate a function to be called. 
- Contrary to DPC, an APC is targeted towards a particular thread, so only that thread can execute the function. 
- Means each thread has an APC queue associated with it.
- APC API is undocumented in kernel mode, so drivers don’t usually use APCs directly

### Three types:
#### 1. User mode APCs
- Execute in user mode only when  thread goes into alertable state. 
- This is typically accomplished by calling `SleepEx`, `WaitForSingleObjectEx`, `WaitForMultipleObjectsEx`. 
- The last argument can be `TRUE` to put thread in alertable state. If APC queue is not empty, APCs will execute until empty.
#### 2. Normal kernel mode APCs
- Execute in kernel mode at IRQL `PASSIVE_LEVEL` and preempt user mode code and user mode APCs.
#### 3. Special kernel APCs 
- Execute in kernel mode at IRQL `APC_LEVEL` (1) and preempt user
mode code, normal kernel APCs, and user mode APCs. 
- These APCs are used by I/O system
to complete I/O operations.

## Critical Regions and Guarded Regions
### Critical Region 
- Prevents user mode and normal kernel APCs from executing.
- A thread enters critical region with `KeEnterCriticalRegion` and leaves it
with `KeLeaveCriticalRegion`. 
- Some kernel functions require being inside critical region, especially when working with executive resources.

### Guarded Region 
- Prevents all APCs from executing. 
- Call `KeEnterGuardedRegion` to enter guarded region and `KeLeaveGuardedRegion` to leave it. 

## Structured Exception Handling
### Difference
Exception is synchronous and technically reproducible under the same conditions, whereas interrupt is asynchronous and can arrive at any time. 

### Structured Exception Handling (SEH)
If an exception occurs, the kernel catches this and allows code to handle the exception, if possible.

### Interrupt Dispatch Table (IDT)
The kernel exception handlers are called based on the IDT, the same one
holding mapping between interrupt vectors and ISRs. 

Using `$ !idt` command shows all these mappings.

### Common examples
- Division by zero (0)
- Breakpoint (3) - kernel handles this transparently, passing control to an attached debugger.
- Invalid opcode (6) - raised by CPU if encounters an unknown instruction.
- Page fault (14) - raised by CPU if the page table entry used for translating virtual to physical addresses has the Valid bit set to zero, indicating that the page is not resident in physical memory.

### Keywords for working with SEH
<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH6/6-4%20Keywords%20for%20working%20with%20SEH.png" width="80%" />

## Using __try/__except
For example, user mode code could free buffer, before driver accesses it. 

Should wrap in a `__try/__except` block to make sure bad buffer does not crash driver.

Here is part of revised `IRP_MJ_DEVICE_CONTROL` handler using an exception handler:
```C
case IOCTL_PRIORITY_BOOSTER_SET_PRIORITY:
{
  if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ThreadData)) {
    status = STATUS_BUFFER_TOO_SMALL;
    break;
  }
  auto data = (ThreadData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
  if (data == nullptr) {
    status = STATUS_INVALID_PARAMETER;
    break;
  }
  __try {
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
    KdPrint(("Thread Priority change for %d to %d succeeded!\n", data->ThreadId, data->Priority));
  }  
  __except (EXCEPTION_EXECUTE_HANDLER) {
    // something wrong with the buffer
    status = STATUS_ACCESS_VIOLATION;
  }
  break;
}
```

It can be more selective by calling `GetExceptionCode` and looking at actual exception. 
```C
  __except (GetExceptionCode() == STATUS_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
  // handle exception
}
```

### Catch all exception in kernel
Access violation can only be caught if the violated address is in user
space.

If it’s in kernel space, it will not be caught and still cause system crash, because something bad has happened and the kernel will not let driver get away with it.

User mode addresses are not at driver control, so such exceptions can be caught and handled.

## Using __try/__finally
```C
void foo() {
  void* p = ExAllocatePool(PagedPool, 1024);
  // do work with p
  ExFreePool(p);
}
```

### Several issues
- If exception is thrown between allocation and release, handler will be searched but memory not freed.
- If return statement used in some conditional between allocation and release, buffer will not be freed.

### Make sure buffer is freed
```C
void foo() {
  void* p = ExAllocatePool(PagedPool, 1024);
  __try {
    // do work with p
  }
  __finally {
    ExFreePool(p);
  }
}
```

## Using C++ RAII Instead of __try / __finally
Example that manages buffer allocation with RAII class:
```C++
template<typename T = void>
struct kunique_ptr {
  kunique_ptr(T* p = nullptr) : _p(p) {}
  
  // remove copy ctor and copy = (single owner)
  kunique_ptr(const kunique_ptr&) = delete;
  kunique_ptr& operator=(const kunique_ptr&) = delete;

  // allow ownership transfer
  kunique_ptr(kunique_ptr&& other) : _p(other._p) {
    other._p = nullptr;
  }

  kunique_ptr& operator=(kunique_ptr&& other) {
    if (&other != this) {
      Release();
      _p = other._p;
      other._p = nullptr;
    }
    return *this;
  }

  ~kunique_ptr() {
    Release();
  }

  operator bool() const {
    return _p != nullptr;
  }

  T* operator->() const {
    return _p;
  }

  T& operator*() const {
    return *_p;
  }

  void Release() {
    if (_p)
      ExFreePool(_p);
  }

private:
  T* _p;
};

struct MyData {
  ULONG Data1;
  HANDLE Data2;
};

void foo() {
  // take charge of the allocation
  kunique_ptr<MyData> data((MyData*)ExAllocatePool(PagedPool, sizeof(MyData)));
  // use the pointer
  data->Data1 = 10;
  // when the object goes out of scope, the destructor frees the buffer
}
```

## System Crash
- Dump is not written to target file at crash time, but written to the first page file. 
- Only when system restarts kernel notices there is dump information in the page file does it copy data to target file. 
- Reason that it may be too dangerous to write something to a new file at system crash
- Downside is that the page file must be large enough to contain the dump, otherwise dump file will not be written.

### dump type
- Small memory dump: containing basic system information and
crash information. Usually is too little to determine what happened.
- Kernel memory dump: captures all kernel memory but no user mode memory. Usually good enough to determine.
- Complete memory dump: provides dump of all memory, user mode and kernel mode. Downside is the size of the dump, is gigantic depending on system RAM and currently used memory.
- Automatic memory dump (Windows 8+): same as kernel memory dump, but kernel resizes page file on boot to 
guarantees with high probability.
- Active memory dump (Windows 10+): similar to complete memory dump, except that if crashed system is hosting guest virtual machines, using memory at the time is not captured. This helps in reducing dump file size on server systems that may be hosting many VMs.

## System Hang
- If the system is still responsive to some extent, Sysinternals NotMyFault tool can force system crash and so force a dump file to be generated.
- If system is completely unresponsive, can attach kernel debugger, then debug normally or generate dump file by `$.dump`.

## Thread Synchronization
### Example
- Driver using linked list to gather data items. 
- Driver can be invoked by multiple clients, coming from many threads in one or more processes. 
- Manipulating linked list must be done atomically to avoid corrupted. 
- If multiple threads access the same memory where at least one is a writer making changes, this is referred to data race. 
- If data race is within a driver,  system crash occurs sooner
- While one thread manipulates the linked list, other threads wait for the first thread to finish its work

## Interlocked Operations
The Interlocked set of functions provide convenient operations to use hardware, and no software objects are involved. 

If using these functions, they will work as efficient as they possibly can.

### Example
Incrementing a value by 1 done from two threads ends up with result of 1 instead of 2

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH6/6-5%20%20Concurrent%20increment.png" width="80%" />

## Dispatcher Objects
- Also called Waitable Objects.
- These objects have a state called signaled and non-signaled
- A thread can wait on such objects until they become signaled. 
- While waiting the thread does not consume cpu cycles as it’s in a waiting state.

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH6/6-6%20Object%20Types%20and%20signaled%20meaning.png" width="80%" />

## Mutex
A mutex is signaled when it’s free.

Once a thread calls a wait function and the wait is satisfied, the mutex becomes non-signaled and the thread becomes the owner of the mutex. 

Ownership is very important for a mutex. 

### Ownership
- If a thread is the owner, it’s the only one that can release the mutex.
- A mutex can be acquired more than once by the same thread. The second attempt succeeds automatically since the thread is the current owner of the mutex. 
- This also means the thread
needs to release the mutex the same number of times it was acquired.

### Example
Use a mutex to access some shared data so that only a single thread does so at a time:
```C++
KMUTEX MyMutex;
LIST_ENTRY DataHead;

void Init() {
  KeInitializeMutex(&MyMutex, 0);
}

void DoWork() {
  // wait for the mutex to be available
  
  KeWaitForSingleObject(&MyMutex, Executive, KernelMode, FALSE, nullptr);
  __try {
    // access DataHead freely
  }
  __finally {
    // once done, release the mutex
    KeReleaseMutex(&MyMutex, FALSE);
  }
}
```

### Create a mutex wrapper 
```C++
struct Mutex {
  void Init() {
    KeInitializeMutex(&_mutex, 0);
  }

  void Lock() {
    KeWaitForSingleObject(&_mutex, Executive, KernelMode, FALSE, nullptr);
  }

  void Unlock() {
    KeReleaseMutex(&_mutex, FALSE);
  }

private:
  KMUTEX _mutex;
};
```

### Create generic RAII wrapper 
```C
template<typename TLock>
struct AutoLock {
  AutoLock(TLock& lock) : _lock(lock) {
    lock.Lock();
  }

  ~AutoLock() {
    _lock.Unlock();
  }

private:
  TLock& _lock;
};

Mutex MyMutex;

void Init() {
  MyMutex.Init();
}

void DoWork() {
  AutoLock<Mutex> locker(MyMutex);
  
  // access DataHead freely
}
```

## Fast Mutex
- An alternative to classic mutex, providing better performance.
- Has its own API for acquiring and releasing mutex.
- Most drivers requires a fast mutex

### Comparison
- A fast mutex can’t be acquired recursively. Doing so causes a deadlock.
- When a fast mutex is acquired the CPU IRQL is raised to APC_LEVEL (1), prevents any delivery of APCs to that thread.
- A fast mutex can only be waited on indefinitely 
- There is no way to specify a timeout.

### Example
```C++
// fastmutex.h
class FastMutex {
public:
  void Init();

  void Lock();
  void Unlock();

private:
  FAST_MUTEX _mutex;
};

// fastmutex.cpp

#include "FastMutex.h"

void FastMutex::Init() {
  ExInitializeFastMutex(&_mutex);
}

void FastMutex::Lock() {
  ExAcquireFastMutex(&_mutex);
}

void FastMutex::Unlock() {
  ExReleaseFastMutex(&_mutex);
}
```

## Semaphore
- Goal is to limit something, such as the length of a queue.
- Initialized with its maximum and initial count by calling
`KeInitializeSemaphore`. 
- While its internal count is greater than zero, the semaphore is signaled.
- A thread that calls `KeWaitForSingleObject` has its wait satisfied and the semaphore count drops by one. 
- Semaphore becomes nonsignaled until the count reaches zero.

## Event
- An event encapsulates a boolean flag: signaled or non-signaled. 
- Purpose is to signal something has happened, to provide flow synchronization. 

### Example
If some condition becomes true, an event can be set, and a bunch of threads can be released from waiting and continue working on some data that perhaps is now ready for processing.

### Types
1. Notification event (manual reset): when this event is set, it releases any number of waiting threads, and the event state remains signaled until explicitly reset.
2. Synchronization event (auto reset): when this event is set, it releases at most one thread (no matter how many are waiting for the event), and once released the event goes back to non-signaled state automatically.

## Executive Resource
- A thread that requires access to the shared resource can declare its intentions: read or write. 
- If it declares read, other threads declaring read can do so concurrently, improving performance. 
- This is especially useful if the shared data changes slowly.

### Usage
- Initializing by allocating an `ERESOURCE` structure from non-paged pool and calling `ExInitializeResourceLite`. 
- Threads can acquire either exclusive lock by `ExAcquireResourceExclusiveLite` or shared lock by `ExAcquireResourceSharedLite`. 
- Releases executive resource with
`ExReleaseResourceLite`. 
- Use `KeEnterCtriticalRegion` before the acquire call, and then `KeLeaveCriticalRegion` after the release call.

### Example
```C++
void WriteData() {
  ExEnterCriticalRegionAndAcquireResourceExclusive(&resource);

  // Write to the data

  ExReleaseResourceAndLeaveCriticalRegion(&resource);
}
```

## High IRQL Synchronization
### Scenario
- A driver has a timer, set up with `KeSetTimer` and uses a DPC
to execute code when the timer expires. 
- At the same time, other functions in the driver, `IRP_MJ_DEVICE_CONTROL` may execute at the same time (IRQL 0). 
- If both functions need to access a shared resource, they must synchronize access to prevent data
corruption.

### Issue
DPC cannot call `KeWaitForSingleObject` or any other waiting function: calling any of these is fatal. 

How can these functions synchronize access?

### Manipulating IRQL
The system has a single CPU. 

When accessing the shared resource, low IRQL function just needs to raise IRQL to `DISPATCH_LEVEL` and then access the resource. 

During that time a DPC cannot interfere since IRQL is already 2. 

Once the code is done , it can lower the IRQL back to zero. 

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH6/6-7%20High-IRQL%20synchronization%20by%20manipulating%20IRQL.png" width="60%" />

In standard systems, where there is more than one CPU, this synchronization method is not enough, because the IRQL is a CPU’s property, not a system-wide property. 

If one CPU’s IRQL is raised to
2 and a DPC needs to execute, it can interfere another CPU whose IRQL may be zero. 

In this case, it’s possible both functions will execute at the same time, accessing the shared resource, causing a data race.

## The Spin Lock
- When CPU tries to acquire busy spin, CPU keeps spinning on spin lock, busy waiting for it to be released by another CPU (cannot be done at IRQL DISPATCH_LEVEL or higher).
- Spin lock would need to be allocated and initialized
- Requires access to shared data needs to raise IRQL to 2, acquire the spin lock, perform the work on  shared data and finally release spin lock and lower IRQL back.

<img src="https://github.com/poipoiyo/Demo-image/blob/main/Book-Review/WindowsKernelProgramming/CH6/6-8%20High-IRQL%20synchronization%20with%20a%20Spin%20Lock.png" width="80%" />

### steps
1. Raise IRQL to proper level, which is the highest level of any function trying to synchronize access to a shared resource. 
2. Acquire the spin lock. These two steps are combined by using the appropriate API.

### API
|IRQL|Acquire|Release|Remark|
|---|---|---|---|
|2|KeAcquireSpinLock|KeReleaseSpinLock||
|2|KeAcquireSpinLockAtDpcLevel|KeReleaseSpinLockFromDpcLevel|(a)|
|Device IRQL|KeAcquireInterruptSpinLock|KeReleaseInterruptSpinLock|(b)|
|HIGH_LEVEL|ExInterlockedXxx||(c)|

(a) Can be called at IRQL 2 only. Provides an optimization that just acquires the spin lock without
changing IRQLs. Canonical scenario is in a DPC routine.

(b) Useful for synchronizing an ISR with any other function. Hardware based drivers with an interrupt source use these routines. 

(c) Use provided spin lock and raise IRQL to HIGH_LEVEL. Because of the high IRQL, these routines can be used
in any situation, since raising IRQL is always safe.

## Work Items
- One way to run code on different thread is to create a thread explicitly to run code. 
- Create a separate thread of execution: 
`PsCreateSystemThread` and
`IoCreateSystemThread`. 
- Work items is the term used to describe functions queued to the system thread pool. 
- A driver can allocate and initialize a work item, pointing to the function the driver wishes to execute, and then the work item can be queued to the pool.
- Difference between DPC is work items always execute at IRQL PASSIVE_LEVEL.
- This mechanism can be used
to perform operations at IRQL 0 from functions running at IRQL 2.

### Methods
1. Initialize with `IoAllocateWorkItem` which return a
pointer to `IO_WORKITEM`. Freed with
`IoFreeWorkItem`.
2. Allocate an `IO_WORKITEM` structure dynamically with size provided by `IoSizeofWorkItem`.
Then call `IoInitializeWorkItem`. Finally call `IoUninitializeWorkItem`.



