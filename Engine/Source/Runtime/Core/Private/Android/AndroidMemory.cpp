// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidMemory.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/MallocBinned.h"
#include "HAL/MallocBinned2.h"
#include "HAL/MallocAnsi.h"
#include "unistd.h"
#include <jni.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>

#define JNI_CURRENT_VERSION JNI_VERSION_1_6
extern JavaVM* GJavaVM;

static int64 GetNativeHeapAllocatedSize()
{
	int64 AllocatedSize = 0;
#if 0 // TODO: this works but sometimes crashes?
	JNIEnv* Env = NULL;
	GJavaVM->GetEnv((void **)&Env, JNI_CURRENT_VERSION);
	jint AttachThreadResult = GJavaVM->AttachCurrentThread(&Env, NULL);

	if(AttachThreadResult != JNI_ERR)
	{
		jclass Class = Env->FindClass("android/os/Debug");
		if (Class)
		{
			jmethodID MethodID = Env->GetStaticMethodID(Class, "getNativeHeapAllocatedSize", "()J");
			if (MethodID)
			{
				AllocatedSize = Env->CallStaticLongMethod(Class, MethodID);
			}
		}
	}
#endif
	return AllocatedSize;
}

void FAndroidPlatformMemory::Init()
{
	FGenericPlatformMemory::Init();

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	FPlatformMemoryStats MemoryStats = GetStats();
	UE_LOG(LogInit, Log, TEXT("Memory total: Physical=%.2fMB (%dGB approx) Available=%.2fMB PageSize=%.1fKB"), 
		float(MemoryConstants.TotalPhysical/1024.0/1024.0),
		MemoryConstants.TotalPhysicalGB, 
		float(MemoryStats.AvailablePhysical/1024.0/1024.0),
		float(MemoryConstants.PageSize/1024.0)
		);
}

namespace AndroidPlatformMemory
{
	/**
	* @brief Returns value in bytes from a status line
	* @param Line in format "Blah:  10000 kB" - needs to be writable as it will modify it
	* @return value in bytes (10240000, i.e. 10000 * 1024 for the above example)
	*/
	uint64 GetBytesFromStatusLine(char * Line)
	{
		check(Line);
		int Len = strlen(Line);

		// Len should be long enough to hold at least " kB\n"
		const int kSuffixLength = 4;	// " kB\n"
		if (Len <= kSuffixLength)
		{
			return 0;
		}

		// let's check that this is indeed "kB"
		char * Suffix = &Line[Len - kSuffixLength];
		if (strcmp(Suffix, " kB\n") != 0)
		{
			// Linux the kernel changed the format, huh?
			return 0;
		}

		// kill the kB
		*Suffix = 0;

		// find the beginning of the number
		for (const char * NumberBegin = Suffix; NumberBegin >= Line; --NumberBegin)
		{
			if (*NumberBegin == ' ')
			{
				return static_cast< uint64 >(atol(NumberBegin + 1)) * 1024ULL;
			}
		}

		// we were unable to find whitespace in front of the number
		return 0;
	}
}

extern int32 AndroidThunkCpp_GetMetaDataInt(const FString& Key);

FPlatformMemoryStats FAndroidPlatformMemory::GetStats()
{
	FPlatformMemoryStats MemoryStats;	// will init from constants

										// open to all kind of overflows, thanks to Linux approach of exposing system stats via /proc and lack of proper C API
										// And no, sysinfo() isn't useful for this (cannot get the same value for MemAvailable through it for example).

	if (FILE* FileGlobalMemStats = fopen("/proc/meminfo", "r"))
	{
		int FieldsSetSuccessfully = 0;
		uint64 MemFree = 0, Cached = 0;
		do
		{
			char LineBuffer[256] = { 0 };
			char *Line = fgets(LineBuffer, ARRAY_COUNT(LineBuffer), FileGlobalMemStats);
			if (Line == nullptr)
			{
				break;	// eof or an error
			}

			// if we have MemAvailable, favor that (see http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/commit/?id=34e431b0ae398fc54ea69ff85ec700722c9da773)
			if (strstr(Line, "MemAvailable:") == Line)
			{
				MemoryStats.AvailablePhysical = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "SwapFree:") == Line)
			{
				MemoryStats.AvailableVirtual = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "MemFree:") == Line)
			{
				MemFree = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "Cached:") == Line)
			{
				Cached = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
		} while (FieldsSetSuccessfully < 4);

		// if we didn't have MemAvailable (kernels < 3.14 or CentOS 6.x), use free + cached as a (bad) approximation
		if (MemoryStats.AvailablePhysical == 0)
		{
			MemoryStats.AvailablePhysical = FMath::Min(MemFree + Cached, MemoryStats.TotalPhysical);
		}

		fclose(FileGlobalMemStats);
	}

	// again /proc "API" :/
	if (FILE* ProcMemStats = fopen("/proc/self/status", "r"))
	{
		int FieldsSetSuccessfully = 0;
		do
		{
			char LineBuffer[256] = { 0 };
			char *Line = fgets(LineBuffer, ARRAY_COUNT(LineBuffer), ProcMemStats);
			if (Line == nullptr)
			{
				break;	// eof or an error
			}

			if (strstr(Line, "VmPeak:") == Line)
			{
				MemoryStats.PeakUsedVirtual = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmSize:") == Line)
			{
				MemoryStats.UsedVirtual = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmHWM:") == Line)
			{
				MemoryStats.PeakUsedPhysical = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
			else if (strstr(Line, "VmRSS:") == Line)
			{
				MemoryStats.UsedPhysical = AndroidPlatformMemory::GetBytesFromStatusLine(Line);
				++FieldsSetSuccessfully;
			}
		} while (FieldsSetSuccessfully < 4);

		fclose(ProcMemStats);
	}


	// sanitize stats as sometimes peak < used for some reason
	MemoryStats.PeakUsedVirtual = FMath::Max(MemoryStats.PeakUsedVirtual, MemoryStats.UsedVirtual);
	MemoryStats.PeakUsedPhysical = FMath::Max(MemoryStats.PeakUsedPhysical, MemoryStats.UsedPhysical);

	// get this value from Java instead (DO NOT INTEGRATE at this time) - skip this if JavaVM not set up yet!
#if USE_ANDROID_JNI
	if (GJavaVM)
	{
		MemoryStats.UsedPhysical = static_cast<uint64>(AndroidThunkCpp_GetMetaDataInt(TEXT("ue4.getUsedMemory"))) * 1024ULL;
	}
#endif

	return MemoryStats;
}

uint64 FAndroidPlatformMemory::GetMemoryUsedFast()
{
	// get this value from Java instead (DO NOT INTEGRATE at this time) - skip this if JavaVM not set up yet!
#if USE_ANDROID_JNI
	if (GJavaVM)
	{
		return static_cast<uint64>(AndroidThunkCpp_GetMetaDataInt(TEXT("ue4.getUsedMemory"))) * 1024ULL;
	}
#endif

	// minimal code to get Used memory
	if (FILE* ProcMemStats = fopen("/proc/self/status", "r"))
	{
		while (1)
		{
			char LineBuffer[256] = { 0 };
			char *Line = fgets(LineBuffer, ARRAY_COUNT(LineBuffer), ProcMemStats);
			if (Line == nullptr)
			{
				break;	// eof or an error
			}
			else if (strstr(Line, "VmRSS:") == Line)
			{
				fclose(ProcMemStats);
				return AndroidPlatformMemory::GetBytesFromStatusLine(Line);
			}
		} 
		fclose(ProcMemStats);
	}

	return 0;
}


const FPlatformMemoryConstants& FAndroidPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if (MemoryConstants.TotalPhysical == 0)
	{
		// Gather platform memory stats.
		struct sysinfo SysInfo;
		unsigned long long MaxPhysicalRAMBytes = 0;
		unsigned long long MaxVirtualRAMBytes = 0;

		if (0 == sysinfo(&SysInfo))
		{
			MaxPhysicalRAMBytes = static_cast< unsigned long long >(SysInfo.mem_unit) * static_cast< unsigned long long >(SysInfo.totalram);
			MaxVirtualRAMBytes = static_cast< unsigned long long >(SysInfo.mem_unit) * static_cast< unsigned long long >(SysInfo.totalswap);
		}

		MemoryConstants.TotalPhysical = MaxPhysicalRAMBytes;
		MemoryConstants.TotalVirtual = MaxVirtualRAMBytes;
		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
		MemoryConstants.PageSize = sysconf(_SC_PAGESIZE);
		MemoryConstants.BinnedPageSize = FMath::Max((SIZE_T)65536, MemoryConstants.PageSize);
		MemoryConstants.OsAllocationGranularity = MemoryConstants.PageSize;
#if PLATFORM_32BITS
		MemoryConstants.AddressLimit = DECLARE_UINT64(4) * 1024 * 1024 * 1024;
#else
		MemoryConstants.AddressLimit = FPlatformMath::RoundUpToPowerOfTwo64(MemoryConstants.TotalPhysical);
#endif
	}

	return MemoryConstants;
}

EPlatformMemorySizeBucket FAndroidPlatformMemory::GetMemorySizeBucket()
{
	// @todo android - if running in ES2 mode, or at least without the extensions for texture streaming, we will load all of the textures
	// so we better look like a low memory device
	return FGenericPlatformMemory::GetMemorySizeBucket();
}

// Set rather to use BinnedMalloc2 for binned malloc, can be overridden below
#define USE_MALLOC_BINNED2 PLATFORM_ANDROID_ARM64

FMalloc* FAndroidPlatformMemory::BaseAllocator()
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	// make sure LLM is using UsedPhysical for program size, instead of Availble-Free
	FPlatformMemoryStats Stats = FAndroidPlatformMemory::GetStats();
	FLowLevelMemTracker::Get().SetProgramSize(Stats.UsedPhysical);
#endif

#if USE_MALLOC_BINNED2
	return new FMallocBinned2();
#else
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	// 1 << FMath::CeilLogTwo(MemoryConstants.TotalPhysical) should really be FMath::RoundUpToPowerOfTwo,
	// but that overflows to 0 when MemoryConstants.TotalPhysical is close to 4GB, since CeilLogTwo returns 32
	// this then causes the MemoryLimit to be 0 and crashing the app
	uint64 MemoryLimit = FMath::Min<uint64>(uint64(1) << FMath::CeilLogTwo(MemoryConstants.TotalPhysical), 0x100000000);

	// todo: Verify MallocBinned2 on 32bit
	// [RCL] 2017-03-06 FIXME: perhaps BinnedPageSize should be used here, but leaving this change to the Android platform owner.
	return new FMallocBinned(MemoryConstants.PageSize, MemoryLimit);
#endif
}


void* FAndroidPlatformMemory::BinnedAllocFromOS(SIZE_T Size)
{
	void* Ptr;
	if (USE_MALLOC_BINNED2)
	{
		static FCriticalSection CriticalSection;
		FScopeLock Lock(&CriticalSection);

		const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();

		static uint8* Base = nullptr;
		static uint8* End = nullptr;

		static const SIZE_T MinAllocSize = 4 * 1024 * 1024; // we will allocate chunks of 4MB, that means the amount we will need to unmap, assuming a lot of 64k blocks, will be small.

		if (End - Base < Size)
		{
			if (Base)
			{
				if (Base < End)
				{
					if (munmap(Base, End - Base) != 0)
					{
						const int ErrNo = errno;
						UE_LOG(LogHAL, Fatal, TEXT("munmap (for trim) (addr=%p, len=%llu) failed with errno = %d (%s)"), Base, End - Base,
							ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
					}
				}
				Base = nullptr;
				End = nullptr;
			}

			SIZE_T SizeToAlloc = FMath::Max<SIZE_T>(MinAllocSize, Align(Size, MemoryConstants.PageSize) + MemoryConstants.BinnedPageSize);
			uint8* UnalignedBase = (uint8*)mmap(nullptr, SizeToAlloc, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
			End = UnalignedBase + SizeToAlloc;
			Base = Align(UnalignedBase, MemoryConstants.BinnedPageSize);

			if (Base > UnalignedBase)
			{
				if (munmap(UnalignedBase, Base - UnalignedBase) != 0)
				{
					const int ErrNo = errno;
					UE_LOG(LogHAL, Fatal, TEXT("munmap (for align) (addr=%p, len=%llu) failed with errno = %d (%s)"), UnalignedBase, Base - UnalignedBase,
						ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
				}
			}


		}
		Ptr = Base;
		uint8* UnalignedBase = Align(Base + Size, MemoryConstants.PageSize);
		Base = Align(UnalignedBase, MemoryConstants.BinnedPageSize);

		if (Base > End)
		{
			Base = End;
		}

		if (Base > UnalignedBase)
		{
			if (munmap(UnalignedBase, Base - UnalignedBase) != 0)
			{
				const int ErrNo = errno;
				UE_LOG(LogHAL, Fatal, TEXT("munmap (for tail align) (addr=%p, len=%llu) failed with errno = %d (%s)"), UnalignedBase, Base - UnalignedBase,
					ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
			}
		}
	}
	else
	{
		Ptr = mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	}

	LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Ptr, Size));
	return Ptr;
}

void FAndroidPlatformMemory::BinnedFreeToOS(void* Ptr, SIZE_T Size)
{
	LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr));
	if (munmap(Ptr, Size) != 0)
	{
		const int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu) failed with errno = %d (%s)"), Ptr, Size,
			ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
	}
}

/**
* LLM uses these low level functions (LLMAlloc and LLMFree) to allocate memory. It grabs
* the function pointers by calling FPlatformMemory::GetLLMAllocFunctions. If these functions
* are not implemented GetLLMAllocFunctions should return false and LLM will be disabled.
*/

#if ENABLE_LOW_LEVEL_MEM_TRACKER

int64 LLMMallocTotal = 0;

void* LLMAlloc(size_t Size)
{
	void* Ptr = mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

	LLMMallocTotal += Size;

	return Ptr;
}

void LLMFree(void* Addr, size_t Size)
{
	LLMMallocTotal -= Size;
	if (Addr != nullptr && munmap(Addr, Size) != 0)
	{
		const int ErrNo = errno;
		UE_LOG(LogHAL, Fatal, TEXT("munmap(addr=%p, len=%llu) failed with errno = %d (%s)"), Addr, Size,
			ErrNo, StringCast< TCHAR >(strerror(ErrNo)).Get());
	}
}

#endif

bool FAndroidPlatformMemory::GetLLMAllocFunctions(void*(*&OutAllocFunction)(size_t), void(*&OutFreeFunction)(void*, size_t), int32& OutAlignment)
{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	OutAllocFunction = LLMAlloc;
	OutFreeFunction = LLMFree;
	OutAlignment = sysconf(_SC_PAGESIZE);
	return true;
#else
	return false;
#endif
}
