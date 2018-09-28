// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"

#include "MetalFence.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandQueue.h"
#include "MetalContext.h"

@implementation FMetalDebugFence
@synthesize Inner;

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugFence)

-(instancetype)init
{
	id Self = [super init];
	if (Self)
	{
		Label = nil;
	}
	return Self;
}

-(void)dealloc
{
	[self validate];
	FMetalDebugCommandEncoder* Encoder = nil;
	while ((Encoder = UpdatingEncoders.Pop()))
	{
		[Encoder release];
	}
	Encoder = nil;
	while ((Encoder = WaitingEncoders.Pop()))
	{
		[Encoder release];
	}
	[Label release];
	[super dealloc];
}

-(id <MTLDevice>) device
{
	if (Inner)
	{
		return Inner.device;
	}
	else
	{
		return nil;
	}
}

-(NSString *_Nullable)label
{
	return Label;
}

-(void)setLabel:(NSString *_Nullable)Text
{
	[Text retain];
	[Label release];
	Label = Text;
	if(Inner)
	{
		Inner.label = Text;
	}
}

-(void)validate
{
	UE_CLOG(UpdatingEncoders.IsEmpty() != WaitingEncoders.IsEmpty(), LogMetal, Fatal, TEXT("Fence with unmatched updates/waits destructed - there's a gap in fence (%p) %s"), self, Label ? *FString(Label) : TEXT("Null"));
}

-(void)updatingEncoder:(FMetalDebugCommandEncoder*)Encoder
{
	check(Encoder);
	UpdatingEncoders.Push([Encoder retain]);
}

-(void)waitingEncoder:(FMetalDebugCommandEncoder*)Encoder
{
	check(Encoder);
	WaitingEncoders.Push([Encoder retain]);
}

-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)updatingEncoders
{
	return &UpdatingEncoders;
}

-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)waitingEncoders
{
	return &WaitingEncoders;
}
@end

#if METAL_DEBUG_OPTIONS
void FMetalFence::Validate(void) const
{
	if (GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation && GetPtr())
	{
		[(FMetalDebugFence*)GetPtr() validate];
	}
}
#endif

FMetalFencePool FMetalFencePool::sSelf;
#if METAL_DEBUG_OPTIONS
extern int32 GMetalRuntimeDebugLevel;
#endif

void FMetalFencePool::Initialise(mtlpp::Device const& InDevice)
{
	Device = InDevice;
	for (int32 i = 0; i < FMetalFencePool::NumFences; i++)
	{
#if METAL_DEBUG_OPTIONS
		if (GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation)
		{
			FMetalDebugFence* Fence = [[FMetalDebugFence new] autorelease];
			Fence.Inner = Device.NewFence();
			FMetalFence* F = new FMetalFence(Fence);
			Fences.Add(F);
			Lifo.Push(F);
		}
		else
#endif
		{
			FMetalFence* F = new FMetalFence(Device.NewFence());
#if METAL_DEBUG_OPTIONS
			if (GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation)
			{
				Fences.Add(F);
			}
#endif
			Lifo.Push(F);
		}
	}
	Count = FMetalFencePool::NumFences;
}

FMetalFence* FMetalFencePool::AllocateFence()
{
	FMetalFence* Fence = Lifo.Pop();
	if (Fence)
	{
		FPlatformAtomics::InterlockedDecrement(&Count);
#if METAL_DEBUG_OPTIONS
		if (GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation)
		{
			FScopeLock Lock(&Mutex);
			check(Fences.Contains(Fence));
			Fences.Remove(Fence);
		}
#endif
	}
	check(Fence);
	Fence->Reset();
	return Fence;
}

void FMetalFence::ValidateUsage(FMetalFence* InFence)
{
	if (InFence)
	{
		if (InFence->NumWrites(mtlpp::RenderStages::Vertex) != InFence->NumWaits(mtlpp::RenderStages::Vertex))
		{
			UE_LOG(LogMetal, Warning, TEXT("%p (%s) writes %d waits %d"), InFence, *FString(InFence->GetLabel()), (uint32)InFence->NumWrites(mtlpp::RenderStages::Vertex), (uint32)InFence->NumWaits(mtlpp::RenderStages::Vertex));
		}
		if (InFence->NumWrites(mtlpp::RenderStages::Fragment) != InFence->NumWaits(mtlpp::RenderStages::Fragment))
		{
			UE_LOG(LogMetal, Warning, TEXT("%p (%s) writes %d waits %d"), InFence, *FString(InFence->GetLabel()), (uint32)InFence->NumWrites(mtlpp::RenderStages::Fragment), (uint32)InFence->NumWaits(mtlpp::RenderStages::Fragment));
		}
	}
}

void FMetalFencePool::ReleaseFence(FMetalFence* const InFence)
{
	if (InFence)
	{
#if METAL_DEBUG_OPTIONS
		if (GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation)
		{
			FScopeLock Lock(&Mutex);
			FMetalFence::ValidateUsage(InFence);
			check(!Fences.Contains(InFence));
			Fences.Add(InFence);
		}
#endif
		FPlatformAtomics::InterlockedIncrement(&Count);
		check(Count <= FMetalFencePool::NumFences);
		Lifo.Push(InFence);
	}
}
