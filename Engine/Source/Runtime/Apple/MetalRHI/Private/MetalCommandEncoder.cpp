// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandEncoder.cpp: Metal command encoder wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandEncoder.h"
#include "MetalCommandBuffer.h"
#include "MetalComputeCommandEncoder.h"
#include "MetalRenderCommandEncoder.h"
#include "MetalProfiler.h"
#include "MetalShaderResources.h"

const uint32 EncoderRingBufferSize = 1024 * 1024;

#if METAL_DEBUG_OPTIONS
extern int32 GMetalBufferScribble;
#endif

#pragma mark - Public C++ Boilerplate -

FMetalCommandEncoder::FMetalCommandEncoder(FMetalCommandList& CmdList)
: CommandList(CmdList)
, bSupportsMetalFeaturesSetBytes(CmdList.GetCommandQueue().SupportsFeature(EMetalFeaturesSetBytes))
, RingBuffer(EncoderRingBufferSize, BufferOffsetAlignment, CmdList.GetCommandQueue().GetCompatibleResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::HazardTrackingModeUntracked | BUFFER_RESOURCE_STORAGE_MANAGED)))
, RenderPassDesc(nil)
, EncoderFence(nil)
#if ENABLE_METAL_GPUPROFILE
, CommandBufferStats(nullptr)
#endif
, DebugGroups([NSMutableArray new])
{
	for (uint32 Frequency = 0; Frequency < uint32(mtlpp::FunctionType::Kernel)+1; Frequency++)
	{
		FMemory::Memzero(ShaderBuffers[Frequency].Bytes);
		FMemory::Memzero(ShaderBuffers[Frequency].Offsets);
		FMemory::Memzero(ShaderBuffers[Frequency].Lengths);
		ShaderBuffers[Frequency].Bound = 0;
	}
	
	for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
	{
		ColorStoreActions[i] = mtlpp::StoreAction::Unknown;
	}
	DepthStoreAction = mtlpp::StoreAction::Unknown;
	StencilStoreAction = mtlpp::StoreAction::Unknown;
}

FMetalCommandEncoder::~FMetalCommandEncoder(void)
{
	if(CommandBuffer)
	{
		EndEncoding();
		CommitCommandBuffer(false);
	}
	
	check(!IsRenderCommandEncoderActive());
	check(!IsComputeCommandEncoderActive());
	check(!IsBlitCommandEncoderActive());
	
	RenderPassDesc = nil;

	if(DebugGroups)
	{
		[DebugGroups release];
	}
	
	for (uint32 Frequency = 0; Frequency < uint32(mtlpp::FunctionType::Kernel)+1; Frequency++)
	{
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			ShaderBuffers[Frequency].Buffers[i] = nil;
		}
		FMemory::Memzero(ShaderBuffers[Frequency].Bytes);
		FMemory::Memzero(ShaderBuffers[Frequency].Offsets);
		FMemory::Memzero(ShaderBuffers[Frequency].Lengths);
		ShaderBuffers[Frequency].Bound = 0;
	}
}

void FMetalCommandEncoder::Reset(void)
{
    check(!CommandBuffer);
    check(IsRenderCommandEncoderActive() == false
          && IsComputeCommandEncoderActive() == false
          && IsBlitCommandEncoderActive() == false);
	
	if(RenderPassDesc)
	{
		RenderPassDesc = nil;
	}
	
	static bool bDeferredStoreActions = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
	if (bDeferredStoreActions)
	{
		for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
		{
			ColorStoreActions[i] = mtlpp::StoreAction::Unknown;
		}
		DepthStoreAction = mtlpp::StoreAction::Unknown;
		StencilStoreAction = mtlpp::StoreAction::Unknown;
	}
	
	for (uint32 Frequency = 0; Frequency < uint32(mtlpp::FunctionType::Kernel)+1; Frequency++)
	{
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			ShaderBuffers[Frequency].Buffers[i] = nil;
		}
    	FMemory::Memzero(ShaderBuffers[Frequency].Bytes);
		FMemory::Memzero(ShaderBuffers[Frequency].Offsets);
		FMemory::Memzero(ShaderBuffers[Frequency].Lengths);
		ShaderBuffers[Frequency].Bound = 0;
	}
	
	[DebugGroups removeAllObjects];
}

#pragma mark - Public Command Buffer Mutators -

void FMetalCommandEncoder::StartCommandBuffer(void)
{
	check(!CommandBuffer);
	check(IsRenderCommandEncoderActive() == false
          && IsComputeCommandEncoderActive() == false
          && IsBlitCommandEncoderActive() == false);

	CommandBuffer = CommandList.GetCommandQueue().CreateCommandBuffer();
    METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, CommandBufferDebug = FMetalCommandBufferDebugging::Get(CommandBuffer));
    
	if ([DebugGroups count] > 0)
	{
		CommandBuffer.SetLabel([DebugGroups lastObject]);
	}
	
#if ENABLE_METAL_GPUPROFILE
	FMetalProfiler* Profiler = FMetalProfiler::GetProfiler();
	if (Profiler)
	{
		CommandBufferStats = Profiler->AllocateCommandBuffer(CommandBuffer, 0);
	}
#endif
}
	
void FMetalCommandEncoder::CommitCommandBuffer(uint32 const Flags)
{
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false
          && IsComputeCommandEncoderActive() == false
          && IsBlitCommandEncoderActive() == false);

	if(CommandBuffer.GetLabel() == nil && [DebugGroups count] > 0)
	{
		CommandBuffer.SetLabel([DebugGroups lastObject]);
	}
	
	bool const bWait = (Flags & EMetalSubmitFlagsWaitOnCommandBuffer);
	if (!(Flags & EMetalSubmitFlagsBreakCommandBuffer))
	{
		RingBuffer.Commit(CommandBuffer);
	}
	else
	{
		RingBuffer.Submit();
	}
    
#if METAL_DEBUG_OPTIONS
    if(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
    {
        for (FMetalBuffer const& Buffer : ActiveBuffers)
        {
            GetMetalDeviceContext().AddActiveBuffer(Buffer);
        }
        
        TSet<ns::AutoReleased<FMetalBuffer>> NewActiveBuffers = MoveTemp(ActiveBuffers);
        AddCompletionHandler([NewActiveBuffers](mtlpp::CommandBuffer const&)
                             {
                                 for (FMetalBuffer const& Buffer : NewActiveBuffers)
                                 {
                                     GetMetalDeviceContext().RemoveActiveBuffer(Buffer);
                                 }
                             });
    }
#endif
#if ENABLE_METAL_GPUPROFILE
	CommandBufferStats->End(CommandBuffer);
	CommandBufferStats = nullptr;
#endif

	CommandList.Commit(CommandBuffer, MoveTemp(CompletionHandlers), bWait);
	
	CommandBuffer = nil;
	if (Flags & EMetalSubmitFlagsCreateCommandBuffer)
	{
		StartCommandBuffer();
		check(CommandBuffer);
	}
	
	BufferBindingHistory.Empty();
	TextureBindingHistory.Empty();
}

#pragma mark - Public Command Encoder Accessors -
	
bool FMetalCommandEncoder::IsRenderCommandEncoderActive(void) const
{
	return RenderCommandEncoder.GetPtr() != nil;
}

bool FMetalCommandEncoder::IsComputeCommandEncoderActive(void) const
{
	return ComputeCommandEncoder.GetPtr() != nil;
}

bool FMetalCommandEncoder::IsBlitCommandEncoderActive(void) const
{
	return BlitCommandEncoder.GetPtr() != nil;
}

bool FMetalCommandEncoder::IsImmediate(void) const
{
	return CommandList.IsImmediate();
}

bool FMetalCommandEncoder::IsRenderPassDescriptorValid(void) const
{
	return (RenderPassDesc != nil);
}

mtlpp::RenderCommandEncoder& FMetalCommandEncoder::GetRenderCommandEncoder(void)
{
	check(IsRenderCommandEncoderActive());
	return RenderCommandEncoder;
}

mtlpp::ComputeCommandEncoder& FMetalCommandEncoder::GetComputeCommandEncoder(void)
{
	check(IsComputeCommandEncoderActive());
	return ComputeCommandEncoder;
}

mtlpp::BlitCommandEncoder& FMetalCommandEncoder::GetBlitCommandEncoder(void)
{
	check(IsBlitCommandEncoderActive());
	return BlitCommandEncoder;
}

mtlpp::Fence FMetalCommandEncoder::GetEncoderFence(void) const
{
	return EncoderFence;
}
	
#pragma mark - Public Command Encoder Mutators -

void FMetalCommandEncoder::BeginRenderCommandEncoding(void)
{
	check(RenderPassDesc);
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	RenderCommandEncoder = MTLPP_VALIDATE(mtlpp::CommandBuffer, CommandBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, RenderCommandEncoder(RenderPassDesc));
	METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug = FMetalRenderCommandEncoderDebugging(RenderCommandEncoder, RenderPassDesc, CommandBufferDebug));
	
	check(!EncoderFence.IsValid());
	NSString* Label = nil;
	
	if(GetEmitDrawEvents())
	{
		Label = [NSString stringWithFormat:@"RenderEncoder: %@", [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass")];
		RenderCommandEncoder.SetLabel(Label);
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() == EMetalDebugLevelLogDebugGroups)
				{
					[((NSObject<MTLCommandBuffer>*)CommandBuffer.GetPtr()).debugGroups addObject:Group];
				}
				RenderCommandEncoder.PushDebugGroup(Group);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.PushDebugGroup(Group));
			}
		}
	}
	METAL_STATISTIC(FMetalProfiler::GetProfiler()->BeginEncoder(CommandBufferStats, RenderCommandEncoder));
	
	EncoderFence = FMetalFence(CommandList.GetCommandQueue().CreateFence(Label));
}

void FMetalCommandEncoder::BeginComputeCommandEncoding(void)
{
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	ComputeCommandEncoder = MTLPP_VALIDATE(mtlpp::CommandBuffer, CommandBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, ComputeCommandEncoder());
    METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug = FMetalComputeCommandEncoderDebugging(ComputeCommandEncoder, CommandBufferDebug));

	check(!EncoderFence.IsValid());
	NSString* Label = nil;
	
	if(GetEmitDrawEvents())
	{
		Label = [NSString stringWithFormat:@"ComputeEncoder: %@", [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass")];
		ComputeCommandEncoder.SetLabel(Label);
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() == EMetalDebugLevelLogDebugGroups)
				{
					[((NSObject<MTLCommandBuffer>*)CommandBuffer.GetPtr()).debugGroups addObject:Group];
				}
				ComputeCommandEncoder.PushDebugGroup(Group);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.PushDebugGroup(Group));
			}
		}
	}
	METAL_STATISTIC(FMetalProfiler::GetProfiler()->BeginEncoder(CommandBufferStats, ComputeCommandEncoder));
	EncoderFence = FMetalFence(CommandList.GetCommandQueue().CreateFence(Label));
}

void FMetalCommandEncoder::BeginBlitCommandEncoding(void)
{
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	BlitCommandEncoder = MTLPP_VALIDATE(mtlpp::CommandBuffer, CommandBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, BlitCommandEncoder());
	METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug = FMetalBlitCommandEncoderDebugging(BlitCommandEncoder, CommandBufferDebug));
	
	check(!EncoderFence.IsValid());
	NSString* Label = nil;
	
	if(GetEmitDrawEvents())
	{
		Label = [NSString stringWithFormat:@"BlitEncoder: %@", [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass")];
		BlitCommandEncoder.SetLabel(Label);
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() == EMetalDebugLevelLogDebugGroups)
				{
					[((NSObject<MTLCommandBuffer>*)CommandBuffer.GetPtr()).debugGroups addObject:Group];
				}
				BlitCommandEncoder.PushDebugGroup(Group);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.PushDebugGroup(Group));
			}
		}
	}
	METAL_STATISTIC(FMetalProfiler::GetProfiler()->BeginEncoder(CommandBufferStats, BlitCommandEncoder));
	EncoderFence = FMetalFence(CommandList.GetCommandQueue().CreateFence(Label));
}

mtlpp::Fence FMetalCommandEncoder::EndEncoding(void)
{
	static bool bSupportsFences = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesFences);
	mtlpp::Fence Fence;
	@autoreleasepool
	{
		if(IsRenderCommandEncoderActive())
		{
			check(!bSupportsFences || EncoderFence.IsValid());
			static bool bDeferredStoreActions = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
			if (bDeferredStoreActions)
			{
				check(RenderPassDesc);
				
				ns::Array<mtlpp::RenderPassColorAttachmentDescriptor> ColorAttachments = RenderPassDesc.GetColorAttachments();
				for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
				{
					if (ColorAttachments[i].GetTexture() && ColorAttachments[i].GetStoreAction() == mtlpp::StoreAction::Unknown)
					{
						mtlpp::StoreAction Action = ColorStoreActions[i];
						check(Action != mtlpp::StoreAction::Unknown);
						RenderCommandEncoder.SetColorStoreAction((mtlpp::StoreAction)Action, i);
					}
				}
				if (RenderPassDesc.GetDepthAttachment().GetTexture() && RenderPassDesc.GetDepthAttachment().GetStoreAction() == mtlpp::StoreAction::Unknown)
				{
					mtlpp::StoreAction Action = DepthStoreAction;
					check(Action != mtlpp::StoreAction::Unknown);
					RenderCommandEncoder.SetDepthStoreAction((mtlpp::StoreAction)Action);
				}
				if (RenderPassDesc.GetStencilAttachment().GetTexture() && RenderPassDesc.GetStencilAttachment().GetStoreAction() == mtlpp::StoreAction::Unknown)
				{
					mtlpp::StoreAction Action = StencilStoreAction;
					check(Action != mtlpp::StoreAction::Unknown);
					RenderCommandEncoder.SetStencilStoreAction((mtlpp::StoreAction)Action);
				}
			}
			
			Fence = EncoderFence;
			UpdateFence(Fence);
			
			METAL_STATISTIC(FMetalProfiler::GetProfiler()->EndEncoder(CommandBufferStats, RenderCommandEncoder));
			RenderCommandEncoder.EndEncoding();
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.EndEncoder());
			RenderCommandEncoder = nil;
			EncoderFence.Reset();
		}
		else if(IsComputeCommandEncoderActive())
		{
			check(!bSupportsFences || EncoderFence.IsValid());
			Fence = EncoderFence;
			UpdateFence(Fence);
			
			METAL_STATISTIC(FMetalProfiler::GetProfiler()->EndEncoder(CommandBufferStats, ComputeCommandEncoder));
			ComputeCommandEncoder.EndEncoding();
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.EndEncoder());
			ComputeCommandEncoder = nil;
			EncoderFence.Reset();
		}
		else if(IsBlitCommandEncoderActive())
		{
			check(!bSupportsFences || EncoderFence.IsValid());
			Fence = EncoderFence;
			UpdateFence(Fence);
			
			METAL_STATISTIC(FMetalProfiler::GetProfiler()->EndEncoder(CommandBufferStats, RenderCommandEncoder));
			BlitCommandEncoder.EndEncoding();
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.EndEncoder());
			BlitCommandEncoder = nil;
			EncoderFence.Reset();
		}
	}
	
	for (uint32 Frequency = 0; Frequency < uint32(mtlpp::FunctionType::Kernel)+1; Frequency++)
	{
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			ShaderBuffers[Frequency].Buffers[i] = nil;
		}
		FMemory::Memzero(ShaderBuffers[Frequency].Bytes);
		FMemory::Memzero(ShaderBuffers[Frequency].Offsets);
		FMemory::Memzero(ShaderBuffers[Frequency].Lengths);
		ShaderBuffers[Frequency].Bound = 0;
	}
    return Fence;
}

void FMetalCommandEncoder::InsertCommandBufferFence(FMetalCommandBufferFence& Fence, mtlpp::CommandBufferHandler Handler)
{
	check(CommandBuffer);
	
	Fence.CommandBufferFence = CommandBuffer.GetCompletionFence();
	
	if (Handler)
	{
		AddCompletionHandler(Handler);
	}
}

void FMetalCommandEncoder::AddCompletionHandler(mtlpp::CommandBufferHandler Handler)
{
	check(Handler);
	
	mtlpp::CommandBufferHandler HeapHandler = Block_copy(Handler);
	CompletionHandlers.Add(HeapHandler);
	Block_release(HeapHandler);
}

void FMetalCommandEncoder::UpdateFence(mtlpp::Fence Fence)
{
	check(IsRenderCommandEncoderActive() || IsComputeCommandEncoderActive() || IsBlitCommandEncoderActive());
	static bool bSupportsFences = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesFences);
	if (bSupportsFences METAL_DEBUG_OPTION(|| CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation))
	{
		check(Fence);
		mtlpp::Fence InnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)Fence.GetPtr()).Inner) :) Fence;
		if (RenderCommandEncoder)
		{
			RenderCommandEncoder.UpdateFence(InnerFence, (mtlpp::RenderStages)(mtlpp::RenderStages::Vertex | mtlpp::RenderStages::Fragment));
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddUpdateFence(Fence));
		}
		else if (ComputeCommandEncoder)
		{
			ComputeCommandEncoder.UpdateFence(InnerFence);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.AddUpdateFence(Fence));
		}
		else if (BlitCommandEncoder)
		{
			BlitCommandEncoder.UpdateFence(InnerFence);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.AddUpdateFence(Fence));
		}
	}
}

void FMetalCommandEncoder::WaitForFence(mtlpp::Fence Fence)
{
	check(IsRenderCommandEncoderActive() || IsComputeCommandEncoderActive() || IsBlitCommandEncoderActive());
	static bool bSupportsFences = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesFences);
	if (bSupportsFences METAL_DEBUG_OPTION(|| CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation))
	{
		check(Fence);
		mtlpp::Fence InnerFence = METAL_DEBUG_OPTION(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation ? mtlpp::Fence(((FMetalDebugFence*)Fence.GetPtr()).Inner) :) Fence;
		if (RenderCommandEncoder)
		{
			RenderCommandEncoder.WaitForFence(InnerFence, (mtlpp::RenderStages)(mtlpp::RenderStages::Vertex | mtlpp::RenderStages::Fragment));
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.AddWaitFence(Fence));
		}
		else if (ComputeCommandEncoder)
		{
			ComputeCommandEncoder.WaitForFence(InnerFence);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.AddWaitFence(Fence));
		}
		else if (BlitCommandEncoder)
		{
			BlitCommandEncoder.WaitForFence(InnerFence);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.AddWaitFence(Fence));
		}
	}
}

#pragma mark - Public Debug Support -

void FMetalCommandEncoder::InsertDebugSignpost(ns::String const& String)
{
	if (String)
	{
		if (CommandBuffer && CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() == EMetalDebugLevelLogDebugGroups)
		{
			[((NSObject<MTLCommandBuffer>*)CommandBuffer.GetPtr()).debugGroups addObject:String];
		}
		if (RenderCommandEncoder)
		{
			RenderCommandEncoder.InsertDebugSignpost(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.InsertDebugSignpost(String));
		}
		if (ComputeCommandEncoder)
		{
			ComputeCommandEncoder.InsertDebugSignpost(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.InsertDebugSignpost(String));
		}
		if (BlitCommandEncoder)
		{
			BlitCommandEncoder.InsertDebugSignpost(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.InsertDebugSignpost(String));
		}
	}
}

void FMetalCommandEncoder::PushDebugGroup(ns::String const& String)
{
	if (String)
	{
		if (CommandBuffer && CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() == EMetalDebugLevelLogDebugGroups)
		{
			[((NSObject<MTLCommandBuffer>*)CommandBuffer.GetPtr()).debugGroups addObject:String];
		}
		[DebugGroups addObject:String];
		if (RenderCommandEncoder)
		{
			RenderCommandEncoder.PushDebugGroup(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.PushDebugGroup(String));
		}
		else if (ComputeCommandEncoder)
		{
			ComputeCommandEncoder.PushDebugGroup(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.PushDebugGroup(String));
		}
		else if (BlitCommandEncoder)
		{
			BlitCommandEncoder.PushDebugGroup(String);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.PushDebugGroup(String));
		}
	}
}

void FMetalCommandEncoder::PopDebugGroup(void)
{
	if (DebugGroups.count > 0)
	{
		[DebugGroups removeLastObject];
		if (RenderCommandEncoder)
		{
			RenderCommandEncoder.PopDebugGroup();
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.PopDebugGroup());
		}
		else if (ComputeCommandEncoder)
		{
			ComputeCommandEncoder.PopDebugGroup();
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.PopDebugGroup());
		}
		else if (BlitCommandEncoder)
		{
			BlitCommandEncoder.PopDebugGroup();
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, BlitEncoderDebug.PopDebugGroup());
		}
	}
}

#if ENABLE_METAL_GPUPROFILE
FMetalCommandBufferStats* FMetalCommandEncoder::GetCommandBufferStats(void)
{
	return CommandBufferStats;
}
#endif

#pragma mark - Public Render State Mutators -

void FMetalCommandEncoder::SetRenderPassDescriptor(mtlpp::RenderPassDescriptor RenderPass)
{
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	check(RenderPass);
	
	if(RenderPass.GetPtr() != RenderPassDesc.GetPtr())
	{
		RenderPassDesc = RenderPass;
		
		static bool bDeferredStoreActions = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
		if (bDeferredStoreActions)
		{
			for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
			{
				ColorStoreActions[i] = mtlpp::StoreAction::Unknown;
			}
			DepthStoreAction = mtlpp::StoreAction::Unknown;
			StencilStoreAction = mtlpp::StoreAction::Unknown;
		}
	}
	check(RenderPassDesc);
	
	for (uint32 Frequency = 0; Frequency < uint32(mtlpp::FunctionType::Kernel)+1; Frequency++)
	{
		for (uint32 i = 0; i < ML_MaxBuffers; i++)
		{
			ShaderBuffers[Frequency].Buffers[i] = nil;
		}
		FMemory::Memzero(ShaderBuffers[Frequency].Bytes);
		FMemory::Memzero(ShaderBuffers[Frequency].Offsets);
		FMemory::Memzero(ShaderBuffers[Frequency].Lengths);
		ShaderBuffers[Frequency].Bound = 0;
	}
}

void FMetalCommandEncoder::SetRenderPassStoreActions(mtlpp::StoreAction const* const ColorStore, mtlpp::StoreAction const DepthStore, mtlpp::StoreAction const StencilStore)
{
	check(RenderPassDesc);
	static bool bDeferredStoreActions = CommandList.GetCommandQueue().SupportsFeature(EMetalFeaturesDeferredStoreActions);
	if (bDeferredStoreActions)
	{
		for (uint32 i = 0; i < MaxSimultaneousRenderTargets; i++)
		{
			ColorStoreActions[i] = ColorStore[i];
		}
		DepthStoreAction = DepthStore;
		StencilStoreAction = StencilStore;
	}
}

void FMetalCommandEncoder::SetRenderPipelineState(FMetalShaderPipeline* PipelineState)
{
	check (RenderCommandEncoder);
	{
		METAL_STATISTIC(FMetalProfiler::GetProfiler()->EncodePipeline(CommandBufferStats, PipelineState));
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetPipeline(PipelineState));
		RenderCommandEncoder.SetRenderPipelineState(PipelineState->RenderPipelineState);
	}
}

void FMetalCommandEncoder::SetViewport(mtlpp::Viewport const Viewport[], uint32 NumActive)
{
	check(RenderCommandEncoder);
	check(NumActive >= 1 && NumActive < ML_MaxViewports);
	if (NumActive == 1)
	{
		RenderCommandEncoder.SetViewport(Viewport[0]);
	}
#if PLATFORM_MAC
	else
	{
		check(FMetalCommandQueue::SupportsFeature(EMetalFeaturesMultipleViewports));
		RenderCommandEncoder.SetViewports(Viewport, NumActive);
	}
#endif
}

void FMetalCommandEncoder::SetFrontFacingWinding(mtlpp::Winding const InFrontFacingWinding)
{
    check (RenderCommandEncoder);
	{
		RenderCommandEncoder.SetFrontFacingWinding(InFrontFacingWinding);
	}
}

void FMetalCommandEncoder::SetCullMode(mtlpp::CullMode const InCullMode)
{
    check (RenderCommandEncoder);
	{
		RenderCommandEncoder.SetCullMode(InCullMode);
	}
}

void FMetalCommandEncoder::SetDepthBias(float const InDepthBias, float const InSlopeScale, float const InClamp)
{
    check (RenderCommandEncoder);
	{
		RenderCommandEncoder.SetDepthBias(InDepthBias, InSlopeScale, InClamp);
	}
}

void FMetalCommandEncoder::SetScissorRect(mtlpp::ScissorRect const Rect[], uint32 NumActive)
{
    check(RenderCommandEncoder);
	check(NumActive >= 1 && NumActive < ML_MaxViewports);
	if (NumActive == 1)
	{
		RenderCommandEncoder.SetScissorRect(Rect[0]);
	}
#if PLATFORM_MAC
	else
	{
		check(FMetalCommandQueue::SupportsFeature(EMetalFeaturesMultipleViewports));
		RenderCommandEncoder.SetScissorRects(Rect, NumActive);
	}
#endif
}

void FMetalCommandEncoder::SetTriangleFillMode(mtlpp::TriangleFillMode const InFillMode)
{
    check(RenderCommandEncoder);
	{
		RenderCommandEncoder.SetTriangleFillMode(InFillMode);
	}
}

void FMetalCommandEncoder::SetBlendColor(float const Red, float const Green, float const Blue, float const Alpha)
{
	check(RenderCommandEncoder);
	{
		RenderCommandEncoder.SetBlendColor(Red, Green, Blue, Alpha);
	}
}

void FMetalCommandEncoder::SetDepthStencilState(mtlpp::DepthStencilState const& InDepthStencilState)
{
    check (RenderCommandEncoder);
	{
		RenderCommandEncoder.SetDepthStencilState(InDepthStencilState);
		METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetDepthStencilState(InDepthStencilState));
	}
}

void FMetalCommandEncoder::SetStencilReferenceValue(uint32 const ReferenceValue)
{
    check (RenderCommandEncoder);
	{
		RenderCommandEncoder.SetStencilReferenceValue(ReferenceValue);
	}
}

void FMetalCommandEncoder::SetVisibilityResultMode(mtlpp::VisibilityResultMode const Mode, NSUInteger const Offset)
{
    check (RenderCommandEncoder);
	{
		check(Mode == mtlpp::VisibilityResultMode::Disabled || RenderPassDesc.GetVisibilityResultBuffer());
		RenderCommandEncoder.SetVisibilityResultMode(Mode, Offset);
	}
}
	
#pragma mark - Public Shader Resource Mutators -

void FMetalCommandEncoder::SetShaderBuffer(mtlpp::FunctionType const FunctionType, FMetalBuffer const& Buffer, NSUInteger const Offset, NSUInteger const Length, NSUInteger index, EPixelFormat const Format)
{
	check(index < ML_MaxBuffers);
    if(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesSetBufferOffset) && Buffer && (ShaderBuffers[uint32(FunctionType)].Bound & (1 << index)) && ShaderBuffers[uint32(FunctionType)].Buffers[index] == Buffer)
    {
		SetShaderBufferOffset(FunctionType, Offset, Length, index);
		ShaderBuffers[uint32(FunctionType)].Lengths[index+ML_MaxBuffers] = GMetalBufferFormats[Format].DataFormat;
    }
    else
    {
		if(Buffer)
		{
			ShaderBuffers[uint32(FunctionType)].Bound |= (1 << index);
		}
		else
		{
			ShaderBuffers[uint32(FunctionType)].Bound &= ~(1 << index);
		}
		ShaderBuffers[uint32(FunctionType)].Buffers[index] = Buffer;
		ShaderBuffers[uint32(FunctionType)].Bytes[index] = nil;
		ShaderBuffers[uint32(FunctionType)].Offsets[index] = Offset;
		ShaderBuffers[uint32(FunctionType)].Lengths[index] = Length;
		ShaderBuffers[uint32(FunctionType)].Lengths[index+ML_MaxBuffers] = GMetalBufferFormats[Format].DataFormat;
		
		SetShaderBufferInternal(FunctionType, index);
    }
    
	if (Buffer)
	{
		BufferBindingHistory.Add(ns::AutoReleased<FMetalBuffer>(Buffer));
	}
}

void FMetalCommandEncoder::SetShaderData(mtlpp::FunctionType const FunctionType, FMetalBufferData* Data, NSUInteger const Offset, NSUInteger const Index, EPixelFormat const Format)
{
	check(Index < ML_MaxBuffers);
	
#if METAL_DEBUG_OPTIONS
	if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() > EMetalDebugLevelResetOnBind)
	{
		SetShaderBuffer(FunctionType, nil, 0, 0, Index);
	}
#endif
	
	if(Data)
	{
		ShaderBuffers[uint32(FunctionType)].Bound |= (1 << Index);
	}
	else
	{
		ShaderBuffers[uint32(FunctionType)].Bound &= ~(1 << Index);
	}
	
	ShaderBuffers[uint32(FunctionType)].Buffers[Index] = nil;
	ShaderBuffers[uint32(FunctionType)].Bytes[Index] = Data;
	ShaderBuffers[uint32(FunctionType)].Offsets[Index] = Offset;
	ShaderBuffers[uint32(FunctionType)].Lengths[Index] = Data ? (Data->Len - Offset) : 0;
	ShaderBuffers[uint32(FunctionType)].Lengths[Index+ML_MaxBuffers] = GMetalBufferFormats[Format].DataFormat;
	
	SetShaderBufferInternal(FunctionType, Index);
}

void FMetalCommandEncoder::SetShaderBytes(mtlpp::FunctionType const FunctionType, uint8 const* Bytes, NSUInteger const Length, NSUInteger const Index)
{
	check(Index < ML_MaxBuffers);
	
#if METAL_DEBUG_OPTIONS
	if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() > EMetalDebugLevelResetOnBind)
	{
		SetShaderBuffer(FunctionType, nil, 0, 0, Index);
	}
#endif
	
	if(Bytes && Length)
	{
		ShaderBuffers[uint32(FunctionType)].Bound |= (1 << Index);

		FMetalBuffer Buffer = RingBuffer.NewBuffer(Length, BufferOffsetAlignment);
		FMemory::Memcpy(((uint8*)Buffer.GetContents()), Bytes, Length);
		
		ShaderBuffers[uint32(FunctionType)].Buffers[Index] = Buffer;
		ShaderBuffers[uint32(FunctionType)].Bytes[Index] = nil;
		ShaderBuffers[uint32(FunctionType)].Offsets[Index] = 0;
		ShaderBuffers[uint32(FunctionType)].Lengths[Index] = Length;
		ShaderBuffers[uint32(FunctionType)].Lengths[Index+ML_MaxBuffers] = GMetalBufferFormats[PF_Unknown].DataFormat;
	}
	else
	{
		ShaderBuffers[uint32(FunctionType)].Bound &= ~(1 << Index);
		
		ShaderBuffers[uint32(FunctionType)].Buffers[Index] = nil;
		ShaderBuffers[uint32(FunctionType)].Bytes[Index] = nil;
		ShaderBuffers[uint32(FunctionType)].Offsets[Index] = 0;
		ShaderBuffers[uint32(FunctionType)].Lengths[Index] = 0;
		ShaderBuffers[uint32(FunctionType)].Lengths[Index+ML_MaxBuffers] = GMetalBufferFormats[PF_Unknown].DataFormat;
	}
	
	SetShaderBufferInternal(FunctionType, Index);
}

void FMetalCommandEncoder::SetShaderBufferOffset(mtlpp::FunctionType FunctionType, NSUInteger const Offset, NSUInteger const Length, NSUInteger const index)
{
	check(index < ML_MaxBuffers);
    checkf(ShaderBuffers[uint32(FunctionType)].Buffers[index] && (ShaderBuffers[uint32(FunctionType)].Bound & (1 << index)), TEXT("Buffer must already be bound"));
	check(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesSetBufferOffset));
	ShaderBuffers[uint32(FunctionType)].Offsets[index] = Offset;
	ShaderBuffers[uint32(FunctionType)].Lengths[index] = Length;
	ShaderBuffers[uint32(FunctionType)].Lengths[index+ML_MaxBuffers] = GMetalBufferFormats[PF_Unknown].DataFormat;
	switch (FunctionType)
	{
		case mtlpp::FunctionType::Vertex:
			check (RenderCommandEncoder);
			RenderCommandEncoder.SetVertexBufferOffset(Offset + ShaderBuffers[uint32(FunctionType)].Buffers[index].GetOffset(), index);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBufferOffset(EMetalShaderVertex, Offset + ShaderBuffers[uint32(FunctionType)].Buffers[index].GetOffset(), index));
			break;
		case mtlpp::FunctionType::Fragment:
			check(RenderCommandEncoder);
			RenderCommandEncoder.SetFragmentBufferOffset(Offset + ShaderBuffers[uint32(FunctionType)].Buffers[index].GetOffset(), index);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBufferOffset(EMetalShaderFragment, Offset + ShaderBuffers[uint32(FunctionType)].Buffers[index].GetOffset(), index));
			break;
		case mtlpp::FunctionType::Kernel:
			check (ComputeCommandEncoder);
			ComputeCommandEncoder.SetBufferOffset(Offset + ShaderBuffers[uint32(FunctionType)].Buffers[index].GetOffset(), index);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetBufferOffset(Offset + ShaderBuffers[uint32(FunctionType)].Buffers[index].GetOffset(), index));
			break;
		default:
			check(false);
			break;
	}
}

void FMetalCommandEncoder::SetShaderTexture(mtlpp::FunctionType FunctionType, FMetalTexture const& Texture, NSUInteger index)
{
	check(index < ML_MaxTextures);
	switch (FunctionType)
	{
		case mtlpp::FunctionType::Vertex:
			check (RenderCommandEncoder);
			// MTLPP_VALIDATE(mtlpp::RenderCommandEncoder, RenderCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Texture, mtlpp::ResourceUsage::Read));
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetTexture(EMetalShaderVertex, Texture, index));
			RenderCommandEncoder.SetVertexTexture(Texture, index);
			break;
		case mtlpp::FunctionType::Fragment:
			check(RenderCommandEncoder);
			// MTLPP_VALIDATE(mtlpp::RenderCommandEncoder, RenderCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Texture, mtlpp::ResourceUsage::Read));
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetTexture(EMetalShaderFragment, Texture, index));
			RenderCommandEncoder.SetFragmentTexture(Texture, index);
			break;
		case mtlpp::FunctionType::Kernel:
			check (ComputeCommandEncoder);
			// MTLPP_VALIDATE(mtlpp::ComputeCommandEncoder, ComputeCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Texture, mtlpp::ResourceUsage::Read));
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetTexture(Texture, index));
			ComputeCommandEncoder.SetTexture(Texture, index);
			break;
		default:
			check(false);
			break;
	}
	
	if (Texture)
	{
		TextureBindingHistory.Add(ns::AutoReleased<FMetalTexture>(Texture));
	}
}

void FMetalCommandEncoder::SetShaderSamplerState(mtlpp::FunctionType FunctionType, mtlpp::SamplerState const& Sampler, NSUInteger index)
{
	check(index < ML_MaxSamplers);
	switch (FunctionType)
	{
		case mtlpp::FunctionType::Vertex:
       		check (RenderCommandEncoder);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetSamplerState(EMetalShaderVertex, Sampler, index));
			RenderCommandEncoder.SetVertexSamplerState(Sampler, index);
			break;
		case mtlpp::FunctionType::Fragment:
			check (RenderCommandEncoder);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetSamplerState(EMetalShaderFragment, Sampler, index));
			RenderCommandEncoder.SetFragmentSamplerState(Sampler, index);
			break;
		case mtlpp::FunctionType::Kernel:
			check (ComputeCommandEncoder);
			METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetSamplerState(Sampler, index));
			ComputeCommandEncoder.SetSamplerState(Sampler, index);
			break;
		default:
			check(false);
			break;
	}
}

void FMetalCommandEncoder::SetShaderSideTable(mtlpp::FunctionType const FunctionType, NSUInteger const Index)
{
	if (Index < ML_MaxBuffers)
	{
		SetShaderBytes(FunctionType, (uint8*)ShaderBuffers[uint32(FunctionType)].Lengths, sizeof(ShaderBuffers[uint32(FunctionType)].Lengths), Index);
	}
}

#pragma mark - Public Compute State Mutators -

void FMetalCommandEncoder::SetComputePipelineState(FMetalShaderPipeline* State)
{
	check (ComputeCommandEncoder);
	{
		METAL_STATISTIC(FMetalProfiler::GetProfiler()->EncodePipeline(CommandBufferStats, State));
        METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetPipeline(State));
		ComputeCommandEncoder.SetComputePipelineState(State->ComputePipelineState);
	}
}

#pragma mark - Public Ring-Buffer Accessor -
	
FMetalSubBufferRing& FMetalCommandEncoder::GetRingBuffer(void)
{
	return RingBuffer;
}

#pragma mark - Public Resource query Access -

bool FMetalCommandEncoder::HasTextureBindingHistory(FMetalTexture const& Texture) const
{
	return TextureBindingHistory.Contains(ns::AutoReleased<FMetalTexture>(Texture));
}

bool FMetalCommandEncoder::HasBufferBindingHistory(FMetalBuffer const& Buffer) const
{
	return BufferBindingHistory.Contains(ns::AutoReleased<FMetalBuffer>(Buffer));
}

#pragma mark - Private Functions -

void FMetalCommandEncoder::SetShaderBufferInternal(mtlpp::FunctionType Function, uint32 Index)
{
	NSUInteger Offset = ShaderBuffers[uint32(Function)].Offsets[Index];
	bool bBufferHasBytes = ShaderBuffers[uint32(Function)].Bytes[Index] != nil;
	if (!ShaderBuffers[uint32(Function)].Buffers[Index] && bBufferHasBytes && !bSupportsMetalFeaturesSetBytes)
	{
		uint8 const* Bytes = (((uint8 const*)ShaderBuffers[uint32(Function)].Bytes[Index]->Data) + ShaderBuffers[uint32(Function)].Offsets[Index]);
		uint32 Len = ShaderBuffers[uint32(Function)].Bytes[Index]->Len - ShaderBuffers[uint32(Function)].Offsets[Index];
		
		Offset = 0;
		ShaderBuffers[uint32(Function)].Buffers[Index] = RingBuffer.NewBuffer(Len, BufferOffsetAlignment);
		
		FMemory::Memcpy(((uint8*)ShaderBuffers[uint32(Function)].Buffers[Index].GetContents()) + Offset, Bytes, Len);
	}
	
	ns::AutoReleased<FMetalBuffer>& Buffer = ShaderBuffers[uint32(Function)].Buffers[Index];
	if (Buffer)
	{
#if METAL_DEBUG_OPTIONS
		if(CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
		{
			ActiveBuffers.Add(Buffer);
		}
#endif
        
		switch (Function)
		{
			case mtlpp::FunctionType::Vertex:
				ShaderBuffers[uint32(Function)].Bound |= (1 << Index);
				check(RenderCommandEncoder);
				// MTLPP_VALIDATE(mtlpp::RenderCommandEncoder, RenderCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Buffer, mtlpp::ResourceUsage::Read));
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBuffer(EMetalShaderVertex, Buffer, Offset, Index));
				RenderCommandEncoder.SetVertexBuffer(Buffer, Offset, Index);
				break;
			case mtlpp::FunctionType::Fragment:
				ShaderBuffers[uint32(Function)].Bound |= (1 << Index);
				check(RenderCommandEncoder);
				// MTLPP_VALIDATE(mtlpp::RenderCommandEncoder, RenderCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Buffer, mtlpp::ResourceUsage::Read));
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBuffer(EMetalShaderFragment, Buffer, Offset, Index));
				RenderCommandEncoder.SetFragmentBuffer(Buffer, Offset, Index);
				break;
			case mtlpp::FunctionType::Kernel:
				ShaderBuffers[uint32(Function)].Bound |= (1 << Index);
				check(ComputeCommandEncoder);
				// MTLPP_VALIDATE(mtlpp::ComputeCommandEncoder, ComputeCommandEncoder, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, UseResource(Buffer, mtlpp::ResourceUsage::Read));
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetBuffer(Buffer, Offset, Index));
				ComputeCommandEncoder.SetBuffer(Buffer, Offset, Index);
				break;
			default:
				check(false);
				break;
		}
		
		if (Buffer.IsSingleUse())
		{
			ShaderBuffers[uint32(Function)].Offsets[Index] = 0;
			ShaderBuffers[uint32(Function)].Buffers[Index] = nil;
			ShaderBuffers[uint32(Function)].Bound &= ~(1 << Index);
		}
	}
	else if (bBufferHasBytes && bSupportsMetalFeaturesSetBytes)
	{
		uint8 const* Bytes = (((uint8 const*)ShaderBuffers[uint32(Function)].Bytes[Index]->Data) + ShaderBuffers[uint32(Function)].Offsets[Index]);
		uint32 Len = ShaderBuffers[uint32(Function)].Bytes[Index]->Len - ShaderBuffers[uint32(Function)].Offsets[Index];
		
		switch (Function)
		{
			case mtlpp::FunctionType::Vertex:
				ShaderBuffers[uint32(Function)].Bound |= (1 << Index);
				check(RenderCommandEncoder);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBytes(EMetalShaderVertex, Bytes, Len, Index));
				RenderCommandEncoder.SetVertexData(Bytes, Len, Index);
				break;
			case mtlpp::FunctionType::Fragment:
				ShaderBuffers[uint32(Function)].Bound |= (1 << Index);
				check(RenderCommandEncoder);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, RenderEncoderDebug.SetBytes(EMetalShaderFragment, Bytes, Len, Index));
				RenderCommandEncoder.SetFragmentData(Bytes, Len, Index);
				break;
			case mtlpp::FunctionType::Kernel:
				ShaderBuffers[uint32(Function)].Bound |= (1 << Index);
				check(ComputeCommandEncoder);
				METAL_DEBUG_LAYER(EMetalDebugLevelFastValidation, ComputeEncoderDebug.SetBytes(Bytes, Len, Index));
				ComputeCommandEncoder.SetBytes(Bytes, Len, Index);
				break;
			default:
				check(false);
				break;
		}
	}
}
