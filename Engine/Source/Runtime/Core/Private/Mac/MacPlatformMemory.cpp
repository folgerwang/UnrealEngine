// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MacPlatformMemory.cpp: Mac platform memory functions
=============================================================================*/

#include "Mac/MacPlatformMemory.h"
#include "HAL/PlatformMemory.h"
#include "HAL/MallocTBB.h"
#include "HAL/MallocAnsi.h"
#include "HAL/MallocBinned.h"
#include "HAL/MallocBinned2.h"
#include "HAL/MallocStomp.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CoreStats.h"
#include "CoreGlobals.h"

#include <sys/param.h>
#include <sys/mount.h>

#if WITH_MALLOC_STOMP
extern "C"
{
	#include <crt_externs.h> // Needed for _NSGetArgc & _NSGetArgv
}
#endif

#include "rd_route.h"

// Set rather to use BinnedMalloc2 for binned malloc, can be overridden below
#define USE_MALLOC_BINNED2 (1)

void* CFNetwork_CFAllocatorOperatorNew_Replacement(unsigned long Size, CFAllocatorRef Alloc)
{
	if (Alloc)
	{
		return CFAllocatorAllocate(Alloc, Size, 0);
	}
	else
	{
		return FMemory::Malloc(Size);
	}
}

FMalloc* FMacPlatformMemory::BaseAllocator()
{
	//c++filt __ZnwmPK13__CFAllocator => "operator new(unsigned long, __CFAllocator const*)"
	int err = rd_route_byname("_ZnwmPK13__CFAllocator", "/System/Library/Frameworks/CFNetwork.framework/Versions/A/CFNetwork", (void*)&CFNetwork_CFAllocatorOperatorNew_Replacement, nullptr);
	check(err == 0);
	
	bool bIsMavericks = false;

	char OSRelease[PATH_MAX] = {};
	size_t OSReleaseBufferSize = PATH_MAX;
	if (sysctlbyname("kern.osrelease", OSRelease, &OSReleaseBufferSize, NULL, 0) == 0)
	{
		int32 OSVersionMajor = 0;
		if (sscanf(OSRelease, "%d", &OSVersionMajor) == 1)
		{
			bIsMavericks = OSVersionMajor <= 13;
		}
	}

	if (FORCE_ANSI_ALLOCATOR || IS_PROGRAM)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Ansi;
	}
	else if ((WITH_EDITORONLY_DATA || IS_PROGRAM) && TBB_ALLOCATOR_ALLOWED)
	{
		AllocatorToUse = EMemoryAllocatorToUse::TBB;
	}
	else if (USE_MALLOC_BINNED2)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Binned2;
	}
	else
	{
		AllocatorToUse = EMemoryAllocatorToUse::Binned;
	}

	// Force ansi malloc in some cases
	if(getenv("UE4_FORCE_MALLOC_ANSI") != nullptr || bIsMavericks)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Ansi;
	}

#if defined(__has_feature)
	#if __has_feature(thread_sanitizer)
		AllocatorToUse = EMemoryAllocatorToUse::Ansi;
	#endif
#endif

#if WITH_MALLOC_STOMP
	if (_NSGetArgc() && _NSGetArgv())
	{
		int Argc = *_NSGetArgc();
		char** Argv = *_NSGetArgv();
		for (int i = 1; i < Argc; ++i)
		{
			const char* Arg = Argv[i];
			if (FCStringAnsi::Stricmp(Arg, "-stompmalloc") == 0)
			{
				AllocatorToUse = EMemoryAllocatorToUse::Stomp;
				break;
			}
		}
	}
#endif

	switch (AllocatorToUse)
	{
	case EMemoryAllocatorToUse::Ansi:
		return new FMallocAnsi();
#if WITH_MALLOC_STOMP
	case EMemoryAllocatorToUse::Stomp:
		return new FMallocStomp();
#endif
#if TBB_ALLOCATOR_ALLOWED
	case EMemoryAllocatorToUse::TBB:
		return new FMallocTBB();
#endif
	case EMemoryAllocatorToUse::Binned2:
		return new FMallocBinned2();

	default:	// intentional fall-through
	case EMemoryAllocatorToUse::Binned:
		// [RCL] 2017-03-06 FIXME: perhaps BinnedPageSize should be used here, but leaving this change to the Mac platform owner.
		return new FMallocBinned((uint32)(GetConstants().PageSize&MAX_uint32), 0x100000000);
	}

}

FPlatformMemoryStats FMacPlatformMemory::GetStats()
{
	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();

	FPlatformMemoryStats MemoryStats;

	// Gather platform memory stats.
	vm_statistics Stats;
	mach_msg_type_number_t StatsSize = sizeof(Stats);
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&Stats, &StatsSize);
	uint64_t FreeMem = Stats.free_count * MemoryConstants.PageSize;
	MemoryStats.AvailablePhysical = FreeMem;
	
	// Get swap file info
	xsw_usage SwapUsage;
	SIZE_T Size = sizeof(SwapUsage);
	sysctlbyname("vm.swapusage", &SwapUsage, &Size, NULL, 0);
	MemoryStats.AvailableVirtual = FreeMem + SwapUsage.xsu_avail;

	// Just get memory information for the process and report the working set instead
	mach_task_basic_info_data_t TaskInfo;
	mach_msg_type_number_t TaskInfoCount = MACH_TASK_BASIC_INFO_COUNT;
	task_info( mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&TaskInfo, &TaskInfoCount );
	MemoryStats.UsedPhysical = TaskInfo.resident_size;
	if(MemoryStats.UsedPhysical > MemoryStats.PeakUsedPhysical)
	{
		MemoryStats.PeakUsedPhysical = MemoryStats.UsedPhysical;
	}
	MemoryStats.UsedVirtual = TaskInfo.virtual_size;
	if(MemoryStats.UsedVirtual > MemoryStats.PeakUsedVirtual)
	{
		MemoryStats.PeakUsedVirtual = MemoryStats.UsedVirtual;
	}
	

	return MemoryStats;
}

const FPlatformMemoryConstants& FMacPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if( MemoryConstants.TotalPhysical == 0 )
	{
		// Gather platform memory constants.

		// Get page size.
		vm_size_t PageSize;
		host_page_size(mach_host_self(), &PageSize);

		// Get swap file info
		xsw_usage SwapUsage;
		SIZE_T Size = sizeof(SwapUsage);
		sysctlbyname("vm.swapusage", &SwapUsage, &Size, NULL, 0);

		// Get memory.
		int64 AvailablePhysical = 0;
		int Mib[] = {CTL_HW, HW_MEMSIZE};
		size_t Length = sizeof(int64);
		sysctl(Mib, 2, &AvailablePhysical, &Length, NULL, 0);
		
		MemoryConstants.TotalPhysical = AvailablePhysical;
		MemoryConstants.TotalVirtual = AvailablePhysical + SwapUsage.xsu_total;
		MemoryConstants.PageSize = (uint32)PageSize;
		MemoryConstants.OsAllocationGranularity = (uint32)PageSize;
		MemoryConstants.BinnedPageSize = FMath::Max((SIZE_T)65536, (SIZE_T)PageSize);

		MemoryConstants.TotalPhysicalGB = (MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024;
		MemoryConstants.AddressLimit = FPlatformMath::RoundUpToPowerOfTwo64(MemoryConstants.TotalPhysical);
	}

	return MemoryConstants;	
}
