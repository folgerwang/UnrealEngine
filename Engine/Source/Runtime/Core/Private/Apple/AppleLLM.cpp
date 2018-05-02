// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Apple/AppleLLM.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER

#include <objc/runtime.h>
#if PLATFORM_MAC
#include "rd_route.h"
#endif

struct FLLMTagInfoApple
{
	const TCHAR* Name;
	FName StatName;				// shows in the LLMFULL stat group
	FName SummaryStatName;		// shows in the LLM summary stat group
};

DECLARE_LLM_MEMORY_STAT(TEXT("Objective-C"), STAT_ObjectiveCLLM, STATGROUP_LLMPlatform);

// *** order must match ELLMTagApple enum ***
const FLLMTagInfoApple ELLMTagNamesApple[] =
{
	// csv name						// stat name								// summary stat name						// enum value
	{ TEXT("Objective-C"),				GET_STATFNAME(STAT_ObjectiveCLLM),		NAME_None },									// ELLMTagApple::ObjectiveC
};

static IMP AllocWithZoneOriginal = nullptr;
static IMP DeallocOriginal = nullptr;

static id AllocWithZoneInterposer(id Obj, SEL Sel, struct _NSZone * Zone)
{
	id Result = AllocWithZoneOriginal(Obj, Sel, Zone);
	
	if (Result)
	{
		LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, Result, class_getInstanceSize([Result class]), (ELLMTag)ELLMTagApple::ObjectiveC, ELLMAllocType::System));
	}
	
	return Result;
}

static void DeallocInterposer(id Obj, SEL Sel)
{
	if (Obj)
	{
		LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default, Obj, ELLMAllocType::System));
	}
	DeallocOriginal(Obj, Sel);
}

static id (*NSAllocateObjectPtr)(Class aClass, NSUInteger extraBytes, NSZone *zone) = nullptr;

id NSAllocateObjectPtrInterposer(Class aClass, NSUInteger extraBytes, NSZone *zone)
{
	id Result = NSAllocateObjectPtr(aClass, extraBytes, zone);
	if (Result)
	{
		LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, Result, class_getInstanceSize([Result class]), (ELLMTag)ELLMTagApple::ObjectiveC, ELLMAllocType::System));
	}
	
	return Result;
}

static id (*_os_object_alloc_realizedPtr)(Class aClass, size_t size) = nullptr;
id _os_object_alloc_realizedInterposer(Class aClass, size_t size)
{
	id Result = _os_object_alloc_realizedPtr(aClass, size);
	if (Result)
	{
		LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, Result, class_getInstanceSize([Result class]), (ELLMTag)ELLMTagApple::ObjectiveC, ELLMAllocType::System));
	}
	
	return Result;
}

static void (*NSDeallocateObjectPtr)(id Obj) = nullptr;

void NSDeallocateObjectInterposer(id Obj)
{
	if (Obj)
	{
		LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default, Obj, ELLMAllocType::System));
	}
	NSDeallocateObjectPtr(Obj);
}

/*
 * Register Apple tags with LLM and setup the NSObject +alloc:, -dealloc interposers, some Objective-C allocations will already have been made, but there's not much I can do about that.
 */
void AppleLLM::Initialise()
{
	int32 TagCount = sizeof(ELLMTagApple) / sizeof(FLLMTagInfoApple);

	for (int32 Index = 0; Index < TagCount; ++Index)
	{
		int32 Tag = (int32)ELLMTag::PlatformTagStart + Index;
		const FLLMTagInfoApple& TagInfo = ELLMTagNamesApple[Index];

		FLowLevelMemTracker::Get().RegisterPlatformTag(Tag, TagInfo.Name, TagInfo.StatName, TagInfo.SummaryStatName);
	}
	
#if PLATFORM_MAC
	// Use rd_route to hook NSAllocateObject/_os_object_alloc_realized/NSDeallocateObject that are used in custom allocators within Apple's libraries
	// Necessary to avoid crashes because we otherwise can't track these allocations.
	// For this to work on iOS we would need to get TPS approval for libevil which performs the same function and handle the function-name -> ptr search available separately.
	int err = rd_route_byname("NSAllocateObject", nullptr, (void*)&NSAllocateObjectPtrInterposer, (void**)&NSAllocateObjectPtr);
	check(err == 0);
	
	err = rd_route_byname("_os_object_alloc_realized", nullptr, (void*)&_os_object_alloc_realizedInterposer, (void**)&_os_object_alloc_realizedPtr);
	check(err == 0);
	
	err = rd_route_byname("NSDeallocateObject", nullptr, (void*)&NSDeallocateObjectInterposer, (void**)&NSDeallocateObjectPtr);
	check(err == 0);
	
	Method AllocZone = class_getClassMethod([NSObject class], @selector(allocWithZone:));
	Method Dealloc = class_getInstanceMethod([NSObject class], @selector(dealloc));
	
	AllocWithZoneOriginal = method_getImplementation(AllocZone);
	DeallocOriginal = method_getImplementation(Dealloc);
	
	check(AllocWithZoneOriginal);
	check(DeallocOriginal);
	
	method_setImplementation(AllocZone, (IMP)&AllocWithZoneInterposer);
	method_setImplementation(Dealloc, (IMP)&DeallocInterposer);
#endif
}

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

