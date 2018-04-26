#include "common/os_handle.h"

int GetOSPageSize()
{
#if defined(_WIN32) || defined(_WIN64)
  // Not sure of another way, but just assume 4096 page sizes
  return 0x1000;
#else
  return getpagesize();
#endif
}

int GetOSHandleSize(OSHandle os_handle)
{
#if defined(_WIN32) || defined(_WIN64)
  return GetFileSize(os_handle, 0);
#else
  struct stat st;
  if (fstat(os_handle, &st) != 0)
  {
    return 0;
  }

  return st.st_size;
#endif
}

void CloseOSHandle(OSHandle os_handle)
{
#if defined(_WIN32) || defined(_WIN64)
  CloseHandle(os_handle);
  os_handle = INVALID_HANDLE_VALUE;
#else
  close(os_handle);
  os_handle = -1;
#endif
}

size_t ReadOSHandle(const OSHandle os_handle, void* buf, size_t size, size_t nmemb, size_t offset)
{
#if defined(_WIN32) || defined(_WIN64)
  DWORD BytesRead = 0;
  bool ret = ReadFile(os_handle, buf, size, &BytesRead, NULL);

  if (ret)
  {
    return BytesRead;
  }

  return 0;
#else
  return pread(os_handle, buf, size * nmemb, offset);
#endif
}

void* CreateOSMapping(void* addr, size_t length, int prot, int flags, OSHandle os_handle, size_t offset)
{
#if defined(_WIN32) || defined(_WIN64)
  // TODO This will only handle DWORD size mappings
  OSHandle Mapping = CreateFileMapping(os_handle, 0, prot, 0, length, 0);
  return MapViewOfFile(Mapping, flags, offset, 0, 0);
#else
  return mmap(addr, length, prot, flags, os_handle, offset);
#endif
}

void CloseOSMapping(void* addr, size_t size)
{
#if defined(_WIN32) || defined(_WIN64)
  UnmapViewOfFile(addr);
#else
  munmap(addr, size);
#endif
}
