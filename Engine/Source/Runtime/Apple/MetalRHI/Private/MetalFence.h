// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#pragma once

#import <Metal/Metal.h>

#pragma clang diagnostic ignored "-Wnullability-completeness"

@class FMetalDebugCommandEncoder;

@interface FMetalDebugFence : FApplePlatformObject<MTLFence>
{
	TLockFreePointerListLIFO<FMetalDebugCommandEncoder> UpdatingEncoders;
	TLockFreePointerListLIFO<FMetalDebugCommandEncoder> WaitingEncoders;
	NSString* Label;
}
@property (retain) id<MTLFence> Inner;
-(void)updatingEncoder:(FMetalDebugCommandEncoder*)Encoder;
-(void)waitingEncoder:(FMetalDebugCommandEncoder*)Encoder;
-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)updatingEncoders;
-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)waitingEncoders;
-(void)validate;
@end

@protocol MTLDeviceExtensions <MTLDevice>
/*!
 @method newFence
 @abstract Create a new MTLFence object
 */
- (id <MTLFence>)newFence;
@end

@protocol MTLBlitCommandEncoderExtensions <MTLBlitCommandEncoder>
/*!
 @abstract Update the event to capture all GPU work so far enqueued by this encoder. */
-(void) updateFence:(id <MTLFence>)fence;
/*!
 @abstract Prevent further GPU work until the event is reached. */
-(void) waitForFence:(id <MTLFence>)fence;
@end
@protocol MTLComputeCommandEncoderExtensions <MTLComputeCommandEncoder>
/*!
 @abstract Update the event to capture all GPU work so far enqueued by this encoder. */
-(void) updateFence:(id <MTLFence>)fence;
/*!
 @abstract Prevent further GPU work until the event is reached. */
-(void) waitForFence:(id <MTLFence>)fence;
@end
@protocol MTLRenderCommandEncoderExtensions <MTLRenderCommandEncoder>
/*!
 @abstract Update the event to capture all GPU work so far enqueued by this encoder for the given
 stages.
 @discussion Unlike <st>updateFence:</st>, this method will update the event when the given stage(s) complete, allowing for commands to overlap in execution.
 */
-(void) updateFence:(id <MTLFence>)fence afterStages:(MTLRenderStages)stages;
/*!
 @abstract Prevent further GPU work until the event is reached for the given stages.
 @discussion Unlike <st>waitForFence:</st>, this method will only block commands assoicated with the given stage(s), allowing for commands to overlap in execution.
 */
-(void) waitForFence:(id <MTLFence>)fence beforeStages:(MTLRenderStages)stages;
@end

class FMetalFence
{
public:
	FMetalFence()
	{
		
	}
	
	explicit FMetalFence(mtlpp::Fence Obj)
	: Object(Obj)
	{
		
	}
	
	explicit FMetalFence(FMetalFence const& Other)
	{
		operator=(Other);
	}
	
	~FMetalFence()
	{
		SafeReleaseMetalFence(Object);
	}
	
	FMetalFence& operator=(FMetalFence const& Other)
	{
		if (&Other != this)
		{
			operator=(Other.Object);
		}
		return *this;
	}
	
	FMetalFence& operator=(mtlpp::Fence const& Other)
	{
		if (Other.GetPtr() != Object.GetPtr())
		{
			METAL_DEBUG_OPTION(Validate());
			SafeReleaseMetalFence(Object);
			Object = Other;
		}
		return *this;
	}
	
	operator mtlpp::Fence() const
	{
		return Object;
	}
	
	mtlpp::Fence operator*() const
	{
		return Object;
	}
	
#if METAL_DEBUG_OPTIONS
	void Validate(void) const;
#endif
	
	void Reset(void)
	{
		Object = nil;
	}
	
	bool IsValid() const
	{
		return Object.GetPtr() != nil;
	}
	
private:
	mtlpp::Fence Object;
};
