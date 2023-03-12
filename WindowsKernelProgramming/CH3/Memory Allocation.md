# Chapter 3 
## Dynamic Memory Allocation

Example shows memory allocation and string copying to save the registry path
passed to DriverEntry, and freeing that string in the Unload routine

```C++
#define DRIVER_TAG 'dcba'

UNICODE_STRING g_RegistryPath;

extern "C" NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
  DriverObject->DriverUnload = SampleUnload;

  g_RegistryPath.Buffer = (WCHAR*)ExAllocatePoolWithTag(PagedPool,
  RegistryPath->Length, DRIVER_TAG);
  if (g_RegistryPath.Buffer == nullptr) {
    KdPrint(("Failed to allocate memory\n"));
    return STATUS_INSUFFICIENT_RESOURCES;
  }

  g_RegistryPath.MaximumLength = RegistryPath->Length;
  RtlCopyUnicodeString(&g_RegistryPath, (PCUNICODE_STRING)RegistryPath);

  // %wZ is for UNICODE_STRING objects
  KdPrint(("Copied registry path: %wZ\n", &g_RegistryPath));
  //...
  return STATUS_SUCCESS;
}

void SampleUnload(_In_ PDRIVER_OBJECT DriverObject) {
  UNREFERENCED_PARAMETER(DriverObject);
  
  ExFreePool(g_RegistryPath.Buffer);
  KdPrint(("Sample driver Unload called\n"));
}
```