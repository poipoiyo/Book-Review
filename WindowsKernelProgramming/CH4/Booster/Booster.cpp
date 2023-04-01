#include <iostream>
#include <windows.h>
#include <stdio.h>
#include "..\PriorityBooster\PriorityBoosterCommon.h"


int main(int argc, const char* argv[]) {
  if (argc < 3) {
    printf("Usage: Booster <threadid> <priority>\n");
    return 0;
  }

  HANDLE hDevice = CreateFile(
    L"\\\\.\\PriorityBooster", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr
  );

  if (hDevice == INVALID_HANDLE_VALUE) {
    printf("%s (error=%d)\n", "Failed to open device", GetLastError());
  }

  ThreadData data;
  data.ThreadId = atoi(argv[1]); // command line first argument
  data.Priority = atoi(argv[2]); // command line second argument

  DWORD returned;
  BOOL success = DeviceIoControl(
    hDevice, IOCTL_PRIORITY_BOOSTER_SET_PRIORITY, &data, sizeof(data), nullptr, 0, &returned, nullptr
  );

  if (success) {
    printf("Priority change succeeded!\n");
  }
  else {
    printf("%s (error=%d)\n", "Priority change failed!", GetLastError());
  }

  CloseHandle(hDevice);
}

