// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"

struct CORE_API FWindowsPlatformCrashContext : public FGenericCrashContext
{
	FWindowsPlatformCrashContext(ECrashContextType InType, const TCHAR* InErrorMessage)
		: FGenericCrashContext(InType, InErrorMessage)
		, CrashedThreadId(-1)
	{
	}

	virtual void SetPortableCallStack(const uint64* StackFrames, int32 NumStackFrames);

	virtual void AddPlatformSpecificProperties() const override;

	void CaptureAllThreadContexts() { AddAllThreadContexts(CrashedThreadId, AllThreadContexts); }

	void SetCrashedThreadId(uint32 InId) { CrashedThreadId = InId; }

protected:
	virtual bool GetPlatformAllThreadContextsString(FString& OutStr) const override;

private:
	// Helpers
	typedef TArray<void*, TInlineAllocator<128>> FModuleHandleArray;
	
	static void GetProcModuleHandles(FModuleHandleArray& OutHandles);

	static void ConvertProgramCountersToStackFrames(
		const FModuleHandleArray& SortedModuleHandles,
		const uint64* ProgramCounters,
		int32 NumPCs,
		TArray<FCrashStackFrame>& OutStackFrames);

	static void AddIsCrashed(bool bIsCrashed, FString& OutStr);
	static void AddThreadId(uint32 ThreadId, FString& OutStr);
	static void AddThreadName(const TCHAR* ThreadName, FString& OutStr);
	static void AddThreadContext(
		const FModuleHandleArray& ProcModuleHandles,
		uint32 CrashedThreadId,
		uint32 ThreadId,
		const FString& ThreadName,
		const uint64* StackTrace,
		int32 Depth,
		FString& OutStr);
	static void AddAllThreadContexts(uint32 CrashedThreadId, FString& OutStr);

	/**
	* <Thread>
	*   <CallStack>...</CallStack>
	*   <IsCrashed>...</IsCrashed>
	*   <Registers>...</Registers>
	*   <ThreadID>...</ThreadID>
	*   <ThreadName>...</ThreadName>
	* </Thead>
	* <Thread>...</Thread>
	* ...
	*/
	FString AllThreadContexts;

	// ID of the crashed thread
	uint32 CrashedThreadId;
};

typedef FWindowsPlatformCrashContext FPlatformCrashContext;
