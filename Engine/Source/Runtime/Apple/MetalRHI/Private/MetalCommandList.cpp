// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandList.cpp: Metal command buffer list wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandList.h"
#include "MetalCommandQueue.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "ns.hpp"

#pragma mark - Public C++ Boilerplate -

#if PLATFORM_IOS
extern bool GIsSuspended;
#endif

FMetalCommandList::FMetalCommandList(FMetalCommandQueue& InCommandQueue, bool const bInImmediate)
: CommandQueue(InCommandQueue)
, Index(0)
, Num(0)
, bImmediate(bInImmediate)
{
}

FMetalCommandList::~FMetalCommandList(void)
{
}
	
#pragma mark - Public Command List Mutators -

static void ReportMetalCommandBufferFailure(mtlpp::CommandBuffer const& CompletedBuffer, TCHAR const* ErrorType, bool bDoCheck=true)
{
	NSString* Label = CompletedBuffer.GetLabel();
	int32 Code = CompletedBuffer.GetError().GetCode();
	NSString* Domain = CompletedBuffer.GetError().GetDomain();
	NSString* ErrorDesc = CompletedBuffer.GetError().GetLocalizedDescription();
	NSString* FailureDesc = CompletedBuffer.GetError().GetLocalizedFailureReason();
	NSString* RecoveryDesc = CompletedBuffer.GetError().GetLocalizedRecoverySuggestion();
	
	FString LabelString = Label ? FString(Label) : FString(TEXT("Unknown"));
	FString DomainString = Domain ? FString(Domain) : FString(TEXT("Unknown"));
	FString ErrorString = ErrorDesc ? FString(ErrorDesc) : FString(TEXT("Unknown"));
	FString FailureString = FailureDesc ? FString(FailureDesc) : FString(TEXT("Unknown"));
	FString RecoveryString = RecoveryDesc ? FString(RecoveryDesc) : FString(TEXT("Unknown"));
	
	if (GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() == EMetalDebugLevelLogDebugGroups)
	{
		NSMutableString* DescString = [NSMutableString new];
		[DescString appendFormat:@"Command Buffer %p %@:", CompletedBuffer.GetPtr(), Label ? Label : @"Unknown"];

		for (NSString* String in ((NSObject<MTLCommandBuffer>*)CompletedBuffer.GetPtr()).debugGroups)
		{
			[DescString appendFormat:@"\n\tDebugGroup: %@", String];
		}
		
		UE_LOG(LogMetal, Warning, TEXT("Command Buffer %p %s:%s"), CompletedBuffer.GetPtr(), *LabelString, *FString(DescString));
	}
	else
	{
		NSString* Desc = CompletedBuffer.GetPtr().debugDescription;
		UE_LOG(LogMetal, Warning, TEXT("%s"), *FString(Desc));
	}
	
#if PLATFORM_IOS
    if (bDoCheck && !GIsSuspended && !GIsRenderingThreadSuspended)
#endif
    {
		UE_LOG(LogMetal, Fatal, TEXT("Command Buffer %s Failed with %s Error! Error Domain: %s Code: %d Description %s %s %s"), *LabelString, ErrorType, *DomainString, Code, *ErrorString, *FailureString, *RecoveryString);
    }
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureInternal(mtlpp::CommandBuffer const& CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Internal"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureTimeout(mtlpp::CommandBuffer const& CompletedBuffer)
{
    ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Timeout"), PLATFORM_IOS);
}

static __attribute__ ((optnone)) void MetalCommandBufferFailurePageFault(mtlpp::CommandBuffer const& CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("PageFault"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureBlacklisted(mtlpp::CommandBuffer const& CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Blacklisted"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureNotPermitted(mtlpp::CommandBuffer const& CompletedBuffer)
{
	// when iOS goes into the background, it can get a delayed NotPermitted error, so we can't crash in this case, just allow it to not be submitted
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("NotPermitted"), !PLATFORM_IOS);
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureOutOfMemory(mtlpp::CommandBuffer const& CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("OutOfMemory"));
}

static __attribute__ ((optnone)) void MetalCommandBufferFailureInvalidResource(mtlpp::CommandBuffer const& CompletedBuffer)
{
	ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("InvalidResource"));
}

static void HandleMetalCommandBufferError(mtlpp::CommandBuffer const& CompletedBuffer)
{
	MTLCommandBufferError Code = (MTLCommandBufferError)CompletedBuffer.GetError().GetCode();
	switch(Code)
	{
		case MTLCommandBufferErrorInternal:
			MetalCommandBufferFailureInternal(CompletedBuffer);
			break;
		case MTLCommandBufferErrorTimeout:
			MetalCommandBufferFailureTimeout(CompletedBuffer);
			break;
		case MTLCommandBufferErrorPageFault:
			MetalCommandBufferFailurePageFault(CompletedBuffer);
			break;
		case MTLCommandBufferErrorBlacklisted:
			MetalCommandBufferFailureBlacklisted(CompletedBuffer);
			break;
		case MTLCommandBufferErrorNotPermitted:
			MetalCommandBufferFailureNotPermitted(CompletedBuffer);
			break;
		case MTLCommandBufferErrorOutOfMemory:
			MetalCommandBufferFailureOutOfMemory(CompletedBuffer);
			break;
		case MTLCommandBufferErrorInvalidResource:
			MetalCommandBufferFailureInvalidResource(CompletedBuffer);
			break;
		case MTLCommandBufferErrorNone:
			// No error
			break;
		default:
			ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Unknown"));
			break;
	}
}

static __attribute__ ((optnone)) void HandleAMDMetalCommandBufferError(mtlpp::CommandBuffer const& CompletedBuffer)
{
	HandleMetalCommandBufferError(CompletedBuffer);
}

static __attribute__ ((optnone)) void HandleNVIDIAMetalCommandBufferError(mtlpp::CommandBuffer const& CompletedBuffer)
{
	HandleMetalCommandBufferError(CompletedBuffer);
}

static void HandleIntelMetalCommandBufferError(mtlpp::CommandBuffer const& CompletedBuffer)
{
	HandleMetalCommandBufferError(CompletedBuffer);
}

void FMetalCommandList::HandleMetalCommandBufferFailure(mtlpp::CommandBuffer const& CompletedBuffer)
{
	if (CompletedBuffer.GetError().GetDomain() == MTLCommandBufferErrorDomain || [CompletedBuffer.GetError().GetDomain() isEqualToString:MTLCommandBufferErrorDomain])
	{
		if (GRHIVendorId && IsRHIDeviceAMD())
		{
			HandleAMDMetalCommandBufferError(CompletedBuffer);
		}
		else if (GRHIVendorId && IsRHIDeviceNVIDIA())
		{
			HandleNVIDIAMetalCommandBufferError(CompletedBuffer);
		}
		else if (GRHIVendorId && IsRHIDeviceIntel())
		{
			HandleIntelMetalCommandBufferError(CompletedBuffer);
		}
		else
		{
			HandleMetalCommandBufferError(CompletedBuffer);
		}
	}
	else
	{
		ReportMetalCommandBufferFailure(CompletedBuffer, TEXT("Unknown"));
	}
}

void FMetalCommandList::SetParallelIndex(uint32 InIndex, uint32 InNum)
{
	if (!IsImmediate())
	{
		Index = InIndex;
		Num = InNum;
	}
}

void FMetalCommandList::Commit(mtlpp::CommandBuffer& Buffer, TArray<ns::Object<mtlpp::CommandBufferHandler>> CompletionHandlers, bool const bWait, bool const bIsLastCommandBuffer)
{
	check(Buffer);
	
	Buffer.AddCompletedHandler([CompletionHandlers, bIsLastCommandBuffer](mtlpp::CommandBuffer const& CompletedBuffer)
	{
		if (CompletedBuffer.GetStatus() == mtlpp::CommandBufferStatus::Error)
		{
			HandleMetalCommandBufferFailure(CompletedBuffer);
		}
		if (CompletionHandlers.Num())
		{
			for (ns::Object<mtlpp::CommandBufferHandler> Handler : CompletionHandlers)
			{
				Handler.GetPtr()(CompletedBuffer);
			}
		}
		
		FMetalGPUProfiler::RecordCommandBuffer(CompletedBuffer);
		
		// The final command buffer in a frame will publish its frame
		// stats and reset the counters for the next frame.
		if(bIsLastCommandBuffer)
		{
			FMetalGPUProfiler::RecordFrame();
		}
	});

	if (bImmediate)
	{
		CommandQueue.CommitCommandBuffer(Buffer);
		if (bWait)
		{
			Buffer.WaitUntilCompleted();
		}
	}
	else
	{
		check(!bWait);
		SubmittedBuffers.Add(Buffer);
	}
}

void FMetalCommandList::Submit(uint32 InIndex, uint32 Count)
{
	// Only deferred contexts should call Submit, the immediate context commits directly to the command-queue.
	check(!bImmediate);

	// Command queue takes ownership of the array
	CommandQueue.SubmitCommandBuffers(SubmittedBuffers, InIndex, Count);
	SubmittedBuffers.Empty();
}
