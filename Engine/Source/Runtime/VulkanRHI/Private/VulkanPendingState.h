// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPendingState.h: Private VulkanPendingState definitions.
=============================================================================*/

#pragma once

// Dependencies
#include "VulkanRHI.h"
#include "VulkanPipeline.h"
#include "VulkanGlobalUniformBuffer.h"
#include "VulkanPipelineState.h"

// All the current compute pipeline states in use
class FVulkanPendingComputeState : public VulkanRHI::FDeviceChild
{
public:
	FVulkanPendingComputeState(FVulkanDevice* InDevice, FVulkanCommandListContext& InContext)
		: VulkanRHI::FDeviceChild(InDevice)
		, Context(InContext)
	{
	}

	~FVulkanPendingComputeState();

	void SetComputePipeline(FVulkanComputePipeline* InComputePipeline)
	{
		if (InComputePipeline != CurrentPipeline)
		{
			CurrentPipeline = InComputePipeline;
			FVulkanComputePipelineDescriptorState** Found = PipelineStates.Find(InComputePipeline);
			if (Found)
			{
				CurrentState = *Found;
				check(CurrentState->ComputePipeline == InComputePipeline);
			}
			else
			{
				CurrentState = new FVulkanComputePipelineDescriptorState(Device, InComputePipeline);
				PipelineStates.Add(CurrentPipeline, CurrentState);
			}

			CurrentState->Reset();
		}
	}

	void PrepareForDispatch(FVulkanCmdBuffer* CmdBuffer);

	inline const FVulkanComputeShader* GetCurrentShader() const
	{
		return CurrentPipeline ? CurrentPipeline->GetShader() : nullptr;
	}

	inline void AddUAVForAutoFlush(FVulkanUnorderedAccessView* UAV)
	{
		UAVListForAutoFlush.Add(UAV);
	}

	void SetUAV(uint32 UAVIndex, FVulkanUnorderedAccessView* UAV);

	inline void SetTexture(uint32 BindPoint, const FVulkanTextureBase* TextureBase, VkImageLayout Layout)
	{
		CurrentState->SetTexture(BindPoint, TextureBase, Layout);
	}

	void SetSRV(uint32 BindIndex, FVulkanShaderResourceView* SRV);

	inline void SetShaderParameter(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	{
		CurrentState->SetShaderParameter(BufferIndex, ByteOffset, NumBytes, NewValue);
	}

	inline void SetUniformBufferConstantData(uint32 BindPoint, const TArray<uint8>& ConstantData)
	{
		CurrentState->SetUniformBufferConstantData(BindPoint, ConstantData);
	}

	inline void SetSamplerState(uint32 BindPoint, FVulkanSamplerState* Sampler)
	{
		CurrentState->SetSamplerState(BindPoint, Sampler);
	}

	void NotifyDeletedPipeline(FVulkanComputePipeline* Pipeline)
	{
		PipelineStates.Remove(Pipeline);
	}

protected:
	TArray<FVulkanUnorderedAccessView*> UAVListForAutoFlush;

	FVulkanComputePipeline* CurrentPipeline;
	FVulkanComputePipelineDescriptorState* CurrentState;

	TMap<FVulkanComputePipeline*, FVulkanComputePipelineDescriptorState*> PipelineStates;

	FVulkanCommandListContext& Context;

	friend class FVulkanCommandListContext;
};

// All the current gfx pipeline states in use
class FVulkanPendingGfxState : public VulkanRHI::FDeviceChild
{
public:
	FVulkanPendingGfxState(FVulkanDevice* InDevice, FVulkanCommandListContext& InContext)
		: VulkanRHI::FDeviceChild(InDevice)
		, Context(InContext)
	{
		Reset();
	}

	~FVulkanPendingGfxState();

	void Reset()
	{
		FMemory::Memzero(Scissor);
		FMemory::Memzero(Viewport);
		StencilRef = 0;
		bScissorEnable = false;

		CurrentPipeline = nullptr;
		CurrentState = nullptr;
		CurrentBSS = nullptr;
		bDirtyVertexStreams = true;

		//#todo-rco: Would this cause issues?
		//FMemory::Memzero(PendingStreams);
	}

	void SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
	{
		FMemory::Memzero(Viewport);

		Viewport.x = MinX;
		Viewport.y = MinY;
		Viewport.width = MaxX - MinX;
		Viewport.height = MaxY - MinY;
		Viewport.minDepth = MinZ;
		if (MinZ == MaxZ)
		{
			// Engine pases in some cases MaxZ as 0.0
			Viewport.maxDepth = MinZ + 1.0f;
		}
		else
		{
			Viewport.maxDepth = MaxZ;
		}

		SetScissorRect(MinX, MinY, MaxX - MinX, MaxY - MinY);
		bScissorEnable = false;
	}

	inline void SetScissor(bool bInEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
	{
		if (bInEnable)
		{
			SetScissorRect(MinX, MinY, MaxX - MinX, MaxY - MinY);
		}
		else
		{
			SetScissorRect(Viewport.x, Viewport.y, Viewport.width, Viewport.height);
		}

		bScissorEnable = bInEnable;
	}

	inline void SetScissorRect(uint32 MinX, uint32 MinY, uint32 Width, uint32 Height)
	{
		FMemory::Memzero(Scissor);

		Scissor.offset.x = MinX;
		Scissor.offset.y = MinY;
		Scissor.extent.width = Width;
		Scissor.extent.height = Height;
	}

	inline void SetStreamSource(uint32 StreamIndex, VkBuffer VertexBuffer, uint32 Offset)
	{
		PendingStreams[StreamIndex].Stream = VertexBuffer;
		PendingStreams[StreamIndex].BufferOffset = Offset;
		bDirtyVertexStreams = true;
	}

	inline void SetTexture(DescriptorSet::EStage Stage, uint32 BindPoint, const FVulkanTextureBase* TextureBase, VkImageLayout Layout)
	{
		CurrentState->SetTexture(Stage, BindPoint, TextureBase, Layout);
	}

	inline void SetUniformBufferConstantData(DescriptorSet::EStage Stage, uint32 BindPoint, const TArray<uint8>& ConstantData)
	{
		CurrentState->SetUniformBufferConstantData(Stage, BindPoint, ConstantData);
	}

	inline void SetUniformBuffer(DescriptorSet::EStage Stage, uint32 BindPoint, const FVulkanUniformBuffer* UniformBuffer)
	{
		CurrentState->SetUniformBuffer(Stage, BindPoint, UniformBuffer);
	}

	void SetUAV(DescriptorSet::EStage Stage, uint32 UAVIndex, FVulkanUnorderedAccessView* UAV);

	void SetSRV(DescriptorSet::EStage Stage, uint32 BindIndex, FVulkanShaderResourceView* SRV);

	inline void SetSamplerState(DescriptorSet::EStage Stage, uint32 BindPoint, FVulkanSamplerState* Sampler)
	{
		CurrentState->SetSamplerState(Stage, BindPoint, Sampler);
	}

	inline void SetShaderParameter(DescriptorSet::EStage Stage, uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	{
		CurrentState->SetShaderParameter(Stage, BufferIndex, ByteOffset, NumBytes, NewValue);
	}

	void PrepareForDraw(FVulkanCmdBuffer* CmdBuffer);

	bool SetGfxPipeline(FVulkanRHIGraphicsPipelineState* InGfxPipeline)
	{
		if (InGfxPipeline != CurrentPipeline)
		{
			// note: BSS objects are cached so this should only be a lookup
			CurrentBSS = ResourceCast(
				RHICreateBoundShaderState(
					InGfxPipeline->PipelineStateInitializer.BoundShaderState.VertexDeclarationRHI,
					InGfxPipeline->PipelineStateInitializer.BoundShaderState.VertexShaderRHI,
					InGfxPipeline->PipelineStateInitializer.BoundShaderState.HullShaderRHI,
					InGfxPipeline->PipelineStateInitializer.BoundShaderState.DomainShaderRHI,
					InGfxPipeline->PipelineStateInitializer.BoundShaderState.PixelShaderRHI,
					InGfxPipeline->PipelineStateInitializer.BoundShaderState.GeometryShaderRHI
				).GetReference()
			);

			CurrentPipeline = InGfxPipeline;
			FVulkanGraphicsPipelineDescriptorState** Found = PipelineStates.Find(InGfxPipeline);
			if (Found)
			{
				CurrentState = *Found;
				check(CurrentState->GfxPipeline == InGfxPipeline);
			}
			else
			{
				CurrentState = new FVulkanGraphicsPipelineDescriptorState(Device, InGfxPipeline, CurrentBSS);
				PipelineStates.Add(CurrentPipeline, CurrentState);
			}

			CurrentState->Reset();
			return true;
		}

		return false;
	}

	inline void UpdateDynamicStates(FVulkanCmdBuffer* Cmd)
	{
		InternalUpdateDynamicStates(Cmd);
	}

	inline void SetStencilRef(uint32 InStencilRef)
	{
		if (InStencilRef != StencilRef)
		{
			StencilRef = InStencilRef;
		}
	}

	void NotifyDeletedPipeline(FVulkanRHIGraphicsPipelineState* Pipeline)
	{
		PipelineStates.Remove(Pipeline);
	}

	inline void MarkNeedsDynamicStates()
	{
	}

protected:
	VkViewport Viewport;
	uint32 StencilRef;
	bool bScissorEnable;
	VkRect2D Scissor;

	bool bNeedToClear;

	FVulkanRHIGraphicsPipelineState* CurrentPipeline;
	FVulkanGraphicsPipelineDescriptorState* CurrentState;
	FVulkanBoundShaderState* CurrentBSS;

	TMap<FVulkanRHIGraphicsPipelineState*, FVulkanGraphicsPipelineDescriptorState*> PipelineStates;

	struct FVertexStream
	{
		FVertexStream() :
			Stream(VK_NULL_HANDLE),
			BufferOffset(0)
		{
		}

		VkBuffer Stream;
		uint32 BufferOffset;
	};
	FVertexStream PendingStreams[MaxVertexElementCount];
	bool bDirtyVertexStreams;

	void InternalUpdateDynamicStates(FVulkanCmdBuffer* Cmd);

	FVulkanCommandListContext& Context;

	friend class FVulkanCommandListContext;
};
