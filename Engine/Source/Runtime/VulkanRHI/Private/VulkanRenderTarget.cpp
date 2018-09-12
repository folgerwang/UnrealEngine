// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanRenderTarget.cpp: Vulkan render target implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "ScreenRendering.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "SceneUtils.h"

static int32 GSubmitOnCopyToResolve = 0;
static FAutoConsoleVariableRef CVarVulkanSubmitOnCopyToResolve(
	TEXT("r.Vulkan.SubmitOnCopyToResolve"),
	GSubmitOnCopyToResolve,
	TEXT("Submits the Queue to the GPU on every RHICopyToResolveTarget call.\n")
	TEXT(" 0: Do not submit (default)\n")
	TEXT(" 1: Submit"),
	ECVF_Default
	);

static int32 GIgnoreCPUReads = 0;
static FAutoConsoleVariableRef CVarVulkanIgnoreCPUReads(
	TEXT("r.Vulkan.IgnoreCPUReads"),
	GIgnoreCPUReads,
	TEXT("Debugging utility for GPU->CPU reads.\n")
	TEXT(" 0 will read from the GPU (default).\n")
	TEXT(" 1 will read from GPU but fill the buffer instead of copying from a texture.\n")
	TEXT(" 2 will NOT read from the GPU and fill with zeros.\n"),
	ECVF_Default
	);

static FCriticalSection GStagingMapLock;
static TMap<FVulkanTextureBase*, VulkanRHI::FStagingBuffer*> GPendingLockedStagingBuffers;

void FTransitionAndLayoutManager::Destroy(FVulkanDevice& InDevice, FTransitionAndLayoutManager* Immediate)
{
	check(!GIsRHIInitialized);

	if (Immediate)
	{
		Immediate->RenderPasses.Append(RenderPasses);
		Immediate->Framebuffers.Append(Framebuffers);
	}
	else
	{
		for (auto& Pair : RenderPasses)
		{
			delete Pair.Value;
		}

		for (auto& Pair : Framebuffers)
		{
			FFramebufferList* List = Pair.Value;
			for (int32 Index = List->Framebuffer.Num() - 1; Index >= 0; --Index)
			{
				List->Framebuffer[Index]->Destroy(InDevice);
				delete List->Framebuffer[Index];
			}
			delete List;
		}
	}

	RenderPasses.Reset();
	Framebuffers.Reset();
}

FVulkanFramebuffer* FTransitionAndLayoutManager::GetOrCreateFramebuffer(FVulkanDevice& InDevice, const FRHISetRenderTargetsInfo& RenderTargetsInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass)
{
	uint32 RTLayoutHash = RTLayout.GetRenderPassCompatibleHash();

	uint64 MipsAndSlicesValues[MaxSimultaneousRenderTargets];
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		MipsAndSlicesValues[Index] = ((uint64)RenderTargetsInfo.ColorRenderTarget[Index].ArraySliceIndex << (uint64)32) | (uint64)RenderTargetsInfo.ColorRenderTarget[Index].MipIndex;
	}
	RTLayoutHash = FCrc::MemCrc32(MipsAndSlicesValues, sizeof(MipsAndSlicesValues), RTLayoutHash);

	FFramebufferList** FoundFramebufferList = Framebuffers.Find(RTLayoutHash);
	FFramebufferList* FramebufferList = nullptr;
	if (FoundFramebufferList)
	{
		FramebufferList = *FoundFramebufferList;

		for (int32 Index = 0; Index < FramebufferList->Framebuffer.Num(); ++Index)
		{
			if (FramebufferList->Framebuffer[Index]->Matches(RenderTargetsInfo))
			{
				return FramebufferList->Framebuffer[Index];
			}
		}
	}
	else
	{
		FramebufferList = new FFramebufferList;
		Framebuffers.Add(RTLayoutHash, FramebufferList);
	}

	FVulkanFramebuffer* Framebuffer = new FVulkanFramebuffer(InDevice, RenderTargetsInfo, RTLayout, *RenderPass);
	FramebufferList->Framebuffer.Add(Framebuffer);
	return Framebuffer;
}

FVulkanRenderPass* FVulkanCommandListContext::PrepareRenderPassForPSOCreation(const FGraphicsPipelineStateInitializer& Initializer, const TArray<FInputAttachmentData>& InputAttachmentData)
{
	FVulkanRenderTargetLayout RTLayout(Initializer, InputAttachmentData);
	return PrepareRenderPassForPSOCreation(RTLayout);
}

FVulkanRenderPass* FVulkanCommandListContext::PrepareRenderPassForPSOCreation(const FVulkanRenderTargetLayout& RTLayout)
{
	FVulkanRenderPass* RenderPass = nullptr;
	RenderPass = TransitionAndLayoutManager.GetOrCreateRenderPass(*Device, RTLayout);
	return RenderPass;
}

void FTransitionAndLayoutManager::BeginEmulatedRenderPass(FVulkanCommandListContext& Context, FVulkanDevice& InDevice, FVulkanCmdBuffer* CmdBuffer, const FRHISetRenderTargetsInfo& RenderTargetsInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass, FVulkanFramebuffer* Framebuffer)
{
	check(!CurrentRenderPass);
	VkClearValue ClearValues[MaxSimultaneousRenderTargets + 1];
	FMemory::Memzero(ClearValues);

	int32 Index = 0;
	for (Index = 0; Index < RenderTargetsInfo.NumColorRenderTargets; ++Index)
	{
		FTextureRHIParamRef Texture = RenderTargetsInfo.ColorRenderTarget[Index].Texture;
		if (Texture)
		{
			FVulkanSurface& Surface = FVulkanTextureBase::Cast(Texture)->Surface;
			VkImage Image = Surface.Image;

			VkImageLayout* Found = Layouts.Find(Image);
			if (!Found)
			{
				Found = &Layouts.Add(Image, VK_IMAGE_LAYOUT_UNDEFINED);
			}

			if (*Found != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
			{
				if (*Found == VK_IMAGE_LAYOUT_UNDEFINED)
				{
					VulkanRHI::ImagePipelineBarrier(CmdBuffer->GetHandle(), Image, EImageLayoutBarrier::Undefined, EImageLayoutBarrier::ColorAttachment, SetupImageSubresourceRange());
				}
				else
				{
					Context.RHITransitionResources(EResourceTransitionAccess::EWritable, &Texture, 1);
				}
			}

			const FLinearColor& ClearColor = Texture->HasClearValue() ? Texture->GetClearColor() : FLinearColor::Black;
			ClearValues[Index].color.float32[0] = ClearColor.R;
			ClearValues[Index].color.float32[1] = ClearColor.G;
			ClearValues[Index].color.float32[2] = ClearColor.B;
			ClearValues[Index].color.float32[3] = ClearColor.A;

			*Found = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}

	if (RenderTargetsInfo.DepthStencilRenderTarget.Texture)
	{
		FTextureRHIParamRef DSTexture = RenderTargetsInfo.DepthStencilRenderTarget.Texture;
		FVulkanSurface& Surface = FVulkanTextureBase::Cast(DSTexture)->Surface;
		VkImageLayout& DSLayout = Layouts.FindOrAdd(Surface.Image);
		FExclusiveDepthStencil RequestedDSAccess = RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess();
		VkImageLayout FinalLayout = VulkanRHI::GetDepthStencilLayout(RequestedDSAccess, InDevice);

		// Check if we need to transition the depth stencil texture(s) based on the current layout and the requested access mode for the render target
		if (DSLayout != FinalLayout)
		{
			VulkanRHI::FPendingBarrier Barrier;
			int32 BarrierIndex = Barrier.AddImageBarrier(Surface.Image, Surface.GetFullAspectMask(), 1);
			VulkanRHI::EImageLayoutBarrier SrcLayout = VulkanRHI::GetImageLayoutFromVulkanLayout(DSLayout);
			VulkanRHI::EImageLayoutBarrier DstLayout = VulkanRHI::GetImageLayoutFromVulkanLayout(FinalLayout);
			Barrier.SetTransition(BarrierIndex, SrcLayout, DstLayout);
			Barrier.Execute(CmdBuffer);
			DSLayout = FinalLayout;
		}

		if (DSTexture->HasClearValue())
		{
			float Depth = 0;
			uint32 Stencil = 0;
			DSTexture->GetDepthStencilClearValue(Depth, Stencil);
			ClearValues[RenderTargetsInfo.NumColorRenderTargets].depthStencil.depth = Depth;
			ClearValues[RenderTargetsInfo.NumColorRenderTargets].depthStencil.stencil = Stencil;
		}
	}
	
	CmdBuffer->BeginRenderPass(RenderPass->GetLayout(), RenderPass, Framebuffer, ClearValues);

	{
		const VkExtent3D& Extents = RTLayout.GetExtent3D();
		Context.GetPendingGfxState()->SetViewport(0, 0, 0, Extents.width, Extents.height, 1);
	}

	CurrentFramebuffer = Framebuffer;
	CurrentRenderPass = RenderPass;
}

void FTransitionAndLayoutManager::EndEmulatedRenderPass(FVulkanCmdBuffer* CmdBuffer)
{
	check(CurrentRenderPass);
	check(!bInsideRealRenderPass);
	CmdBuffer->EndRenderPass();
	CurrentRenderPass = nullptr;
}

void FTransitionAndLayoutManager::BeginRealRenderPass(FVulkanCommandListContext& Context, FVulkanDevice& InDevice, FVulkanCmdBuffer* CmdBuffer, const FRHIRenderPassInfo& RPInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass, FVulkanFramebuffer* Framebuffer)
{
	check(!CurrentRenderPass);
	check(!bInsideRealRenderPass);
	// (NumRT + 1 [Depth] ) * 2 [surface + resolve]
	VkClearValue ClearValues[(MaxSimultaneousRenderTargets + 1) * 2];
	uint32 ClearValueIndex = 0;
	bool bNeedsClearValues = RenderPass->GetNumUsedClearValues() > 0;
	FMemory::Memzero(ClearValues);

	int32 NumColorTargets = RPInfo.GetNumColorRenderTargets();
	int32 Index = 0;
	FPendingBarrier Barrier;
	if (RPInfo.bGeneratingMips)
	{
		GenerateMipsInfo.NumRenderTargets = NumColorTargets;
	}

	for (Index = 0; Index < NumColorTargets; ++Index)
	{
		FTextureRHIParamRef Texture = RPInfo.ColorRenderTargets[Index].RenderTarget;
		CA_ASSUME(Texture);
		FVulkanSurface& Surface = FVulkanTextureBase::Cast(Texture)->Surface;
		VkImageLayout* Found = Layouts.Find(Surface.Image);
		check(Found);

		if (RPInfo.bGeneratingMips)
		{
			int32 NumMips = Surface.GetNumMips();
			if (!GenerateMipsInfo.bInsideGenerateMips)
			{
#if !USING_CODE_ANALYSIS
				// This condition triggers static analysis as it doesn't have side effects.
				ensure(*Found == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL || *Found == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#endif
				int32 NumSlices = Surface.GetNumberOfArrayLevels();
				GenerateMipsInfo.bInsideGenerateMips = true;
				GenerateMipsInfo.Target[Index].CurrentImage = Surface.Image;

				GenerateMipsInfo.Target[Index].Layouts.Reset(0);
				for (int32 SliceIndex = 0; SliceIndex < NumSlices; ++SliceIndex)
				{
					GenerateMipsInfo.Target[Index].Layouts.AddDefaulted();
					for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
					{
						GenerateMipsInfo.Target[Index].Layouts[SliceIndex].Add(*Found);
					}
				}

				if (*Found != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				{
					*Found = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}
			}

			ensure(GenerateMipsInfo.Target[Index].CurrentImage == Surface.Image);

			int32 SliceIndex = (uint32)FMath::Max<int32>(RPInfo.ColorRenderTargets[Index].ArraySlice, 0);
			int32 RTMipIndex = RPInfo.ColorRenderTargets[Index].MipIndex;
			check(RTMipIndex > 0);
			GenerateMipsInfo.CurrentSlice = SliceIndex;
			GenerateMipsInfo.CurrentMip = RTMipIndex;
			GenerateMipsInfo.bLastMip = (RTMipIndex == (NumMips - 1));
			if (GenerateMipsInfo.Target[Index].Layouts[SliceIndex][RTMipIndex - 1] != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			{
				// Transition to readable
				int32 BarrierIndex = Barrier.AddImageBarrier(Surface.Image, VK_IMAGE_ASPECT_COLOR_BIT, 1);
				VkImageSubresourceRange& Range = Barrier.GetSubresource(BarrierIndex);
				Range.baseMipLevel = RTMipIndex - 1;
				Range.baseArrayLayer = SliceIndex;
#if !USING_CODE_ANALYSIS
				ensure(GenerateMipsInfo.Target[Index].Layouts[SliceIndex][RTMipIndex - 1] == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
#endif
				Barrier.SetTransition(BarrierIndex, EImageLayoutBarrier::ColorAttachment, EImageLayoutBarrier::PixelShaderRead);
				GenerateMipsInfo.Target[Index].Layouts[SliceIndex][RTMipIndex - 1] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			if (GenerateMipsInfo.Target[Index].Layouts[SliceIndex][RTMipIndex] != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
			{
				// Transition to writeable
				int32 BarrierIndex = Barrier.AddImageBarrier(Surface.Image, VK_IMAGE_ASPECT_COLOR_BIT, 1);
				VkImageSubresourceRange& Range = Barrier.GetSubresource(BarrierIndex);
				Range.baseMipLevel = RTMipIndex;
				Range.baseArrayLayer = SliceIndex;
#if !USING_CODE_ANALYSIS
				ensure(GenerateMipsInfo.Target[Index].Layouts[SliceIndex][RTMipIndex] == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#endif
				Barrier.SetTransition(BarrierIndex, EImageLayoutBarrier::PixelShaderRead, EImageLayoutBarrier::ColorAttachment);
				GenerateMipsInfo.Target[Index].Layouts[SliceIndex][RTMipIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
		}
		else
		{
			GenerateMipsInfo.Reset();
			if (*Found == VK_IMAGE_LAYOUT_UNDEFINED)
			{
				VulkanRHI::ImagePipelineBarrier(CmdBuffer->GetHandle(), Surface.Image, EImageLayoutBarrier::Undefined, EImageLayoutBarrier::ColorAttachment, SetupImageSubresourceRange());
			}
			else
			{
				Context.RHITransitionResources(EResourceTransitionAccess::EWritable, &Texture, 1);
			}

			*Found = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		if (bNeedsClearValues)
		{
			const FLinearColor& ClearColor = Texture->HasClearValue() ? Texture->GetClearColor() : FLinearColor::Black;
			ClearValues[ClearValueIndex].color.float32[0] = ClearColor.R;
			ClearValues[ClearValueIndex].color.float32[1] = ClearColor.G;
			ClearValues[ClearValueIndex].color.float32[2] = ClearColor.B;
			ClearValues[ClearValueIndex].color.float32[3] = ClearColor.A;
			++ClearValueIndex;
			if (Surface.GetNumSamples() > 1)
			{
				++ClearValueIndex;
			}
		}
	}

	FTextureRHIParamRef DSTexture = RPInfo.DepthStencilRenderTarget.DepthStencilTarget;
	if (DSTexture)
	{
		FVulkanSurface& Surface = FVulkanTextureBase::Cast(DSTexture)->Surface;
		VkImageLayout& DSLayout = Layouts.FindOrAdd(Surface.Image);
		FExclusiveDepthStencil RequestedDSAccess = RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil;
		VkImageLayout FinalLayout = VulkanRHI::GetDepthStencilLayout(RequestedDSAccess, InDevice);

		// Check if we need to transition the depth stencil texture(s) based on the current layout and the requested access mode for the render target
		if (DSLayout != FinalLayout)
		{
			int32 BarrierIndex = Barrier.AddImageBarrier(Surface.Image, Surface.GetFullAspectMask(), 1);
			VulkanRHI::EImageLayoutBarrier SrcLayout = VulkanRHI::GetImageLayoutFromVulkanLayout(DSLayout);
			VulkanRHI::EImageLayoutBarrier DstLayout = VulkanRHI::GetImageLayoutFromVulkanLayout(FinalLayout);
			Barrier.SetTransition(BarrierIndex, SrcLayout, DstLayout);
			DSLayout = FinalLayout;
		}

		if (DSTexture->HasClearValue() && bNeedsClearValues)
		{
			float Depth = 0;
			uint32 Stencil = 0;
			DSTexture->GetDepthStencilClearValue(Depth, Stencil);
			ClearValues[ClearValueIndex].depthStencil.depth = Depth;
			ClearValues[ClearValueIndex].depthStencil.stencil = Stencil;
			++ClearValueIndex;
			if (Surface.GetNumSamples() > 1)
			{
				++ClearValueIndex;
			}
		}
	}

	ensure(ClearValueIndex <= RenderPass->GetNumUsedClearValues());

	Barrier.Execute(CmdBuffer);

	CmdBuffer->BeginRenderPass(RenderPass->GetLayout(), RenderPass, Framebuffer, ClearValues);

	{
		const VkExtent3D& Extents = RTLayout.GetExtent3D();
		Context.GetPendingGfxState()->SetViewport(0, 0, 0, Extents.width, Extents.height, 1);
	}

	CurrentFramebuffer = Framebuffer;
	CurrentRenderPass = RenderPass;
	bInsideRealRenderPass = true;
}

void FTransitionAndLayoutManager::EndRealRenderPass(FVulkanCmdBuffer* CmdBuffer)
{
	check(CurrentRenderPass);
	check(bInsideRealRenderPass);
	CmdBuffer->EndRenderPass();

	if (GenerateMipsInfo.bInsideGenerateMips)
	{
		if (GenerateMipsInfo.bLastMip)
		{
			FPendingBarrier Barrier;
			for (int32 Index = 0; Index < GenerateMipsInfo.NumRenderTargets; ++Index)
			{
				ensure(GenerateMipsInfo.Target[Index].Layouts[GenerateMipsInfo.CurrentSlice][GenerateMipsInfo.CurrentMip] == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

				// Transition to readable
				int32 BarrierIndex = Barrier.AddImageBarrier(GenerateMipsInfo.Target[Index].CurrentImage, VK_IMAGE_ASPECT_COLOR_BIT, 1);
				VkImageSubresourceRange& Range = Barrier.GetSubresource(BarrierIndex);
				Range.baseMipLevel = GenerateMipsInfo.CurrentMip;
				Range.baseArrayLayer = GenerateMipsInfo.CurrentSlice;
				Barrier.SetTransition(BarrierIndex, EImageLayoutBarrier::ColorAttachment, EImageLayoutBarrier::PixelShaderRead);
				// This could really be ignored...
				GenerateMipsInfo.Target[Index].Layouts[GenerateMipsInfo.CurrentSlice][GenerateMipsInfo.CurrentMip] = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			Barrier.Execute(CmdBuffer);
		}
	}

	CurrentRenderPass = nullptr;
	bInsideRealRenderPass = false;
}

void FTransitionAndLayoutManager::NotifyDeletedRenderTarget(FVulkanDevice& InDevice, VkImage Image)
{
	for (auto It = Framebuffers.CreateIterator(); It; ++It)
	{
		FFramebufferList* List = It->Value;
		for (int32 Index = List->Framebuffer.Num() - 1; Index >= 0; --Index)
		{
			FVulkanFramebuffer* Framebuffer = List->Framebuffer[Index];
			if (Framebuffer->ContainsRenderTarget(Image))
			{
				List->Framebuffer.RemoveAtSwap(Index, 1, false);
				Framebuffer->Destroy(InDevice);

				if (Framebuffer == CurrentFramebuffer)
				{
					CurrentFramebuffer = nullptr;
				}

				delete Framebuffer;
			}
		}

		if (List->Framebuffer.Num() == 0)
		{
			delete List;
			It.RemoveCurrent();
		}
	}
}

void FTransitionAndLayoutManager::TransitionResource(FVulkanCmdBuffer* CmdBuffer, FVulkanSurface& Surface, VulkanRHI::EImageLayoutBarrier DestLayout)
{
	VkImageLayout* FoundLayout = Layouts.Find(Surface.Image);
	VkImageLayout VulkanDestLayout = VulkanRHI::GetImageLayout(DestLayout);
	if (FoundLayout)
	{
		if (*FoundLayout != VulkanDestLayout)
		{
			VulkanRHI::EImageLayoutBarrier SourceLayout = GetImageLayoutFromVulkanLayout(*FoundLayout);
			VulkanRHI::ImagePipelineBarrier(CmdBuffer->GetHandle(), Surface.Image, SourceLayout, DestLayout, VulkanRHI::SetupImageSubresourceRange(Surface.GetFullAspectMask()));
			*FoundLayout = VulkanDestLayout;
		}
	}
	else
	{
		VulkanRHI::ImagePipelineBarrier(CmdBuffer->GetHandle(), Surface.Image, EImageLayoutBarrier::Undefined, DestLayout, VulkanRHI::SetupImageSubresourceRange(Surface.GetFullAspectMask()));
		Layouts.Add(Surface.Image, VulkanDestLayout);
	}
}

void FVulkanCommandListContext::RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs)
{
	FRHIDepthRenderTargetView DepthView;
	if (NewDepthStencilTarget)
	{
		DepthView = *NewDepthStencilTarget;
	}
	else
	{
		DepthView = FRHIDepthRenderTargetView(FTextureRHIParamRef(), ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction, ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction);
	}

	if (NumSimultaneousRenderTargets == 1 && (!NewRenderTargets || !NewRenderTargets->Texture))
	{
		--NumSimultaneousRenderTargets;
	}

	FRHISetRenderTargetsInfo RenderTargetsInfo(NumSimultaneousRenderTargets, NewRenderTargets, DepthView);
	RHISetRenderTargetsAndClear(RenderTargetsInfo);

	// Yuck - Bind pending pixel shader UAVs from SetRenderTargets
	{
		PendingPixelUAVs.Reset();
		for (uint32 UAVIndex = 0; UAVIndex < NumUAVs; ++UAVIndex)
		{
			FVulkanUnorderedAccessView* UAV = ResourceCast(UAVs[UAVIndex]);
			if (UAV)
			{
				PendingPixelUAVs.Add({UAV, UAVIndex});
			}
		}
	}
}

void FVulkanCommandListContext::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	FVulkanRenderTargetLayout RTLayout(*Device, RenderTargetsInfo);

	TransitionAndLayoutManager.GenerateMipsInfo.Reset();

	FVulkanRenderPass* RenderPass = nullptr;
	FVulkanFramebuffer* Framebuffer = nullptr;

	if (RTLayout.GetExtent2D().width != 0 && RTLayout.GetExtent2D().height != 0)
	{
		RenderPass = TransitionAndLayoutManager.GetOrCreateRenderPass(*Device, RTLayout);
		Framebuffer = TransitionAndLayoutManager.GetOrCreateFramebuffer(*Device, RenderTargetsInfo, RTLayout, RenderPass);
	}

	if (Framebuffer == TransitionAndLayoutManager.CurrentFramebuffer && RenderPass == TransitionAndLayoutManager.CurrentRenderPass)
	{
		return;
	}

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer->IsInsideRenderPass())
	{
		TransitionAndLayoutManager.EndEmulatedRenderPass(CmdBuffer);

		if (GVulkanSubmitAfterEveryEndRenderPass)
		{
			CommandBufferManager->SubmitActiveCmdBuffer();
			CommandBufferManager->PrepareForNewActiveCommandBuffer();
			CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		}
	}

	if (SafePointSubmit())
	{
		CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	}

	if (RenderPass != nullptr && Framebuffer != nullptr)
	{
		if (RenderTargetsInfo.DepthStencilRenderTarget.Texture ||
			RenderTargetsInfo.NumColorRenderTargets > 1 ||
			((RenderTargetsInfo.NumColorRenderTargets == 1) && RenderTargetsInfo.ColorRenderTarget[0].Texture))
		{
			TransitionAndLayoutManager.BeginEmulatedRenderPass(*this, *Device, CmdBuffer, RenderTargetsInfo, RTLayout, RenderPass, Framebuffer);
		}
		else
		{
			ensureMsgf(0, TEXT("RenderPass not started! Bad combination of values? Depth %p #Color %d Color0 %p"), (void*)RenderTargetsInfo.DepthStencilRenderTarget.Texture, RenderTargetsInfo.NumColorRenderTargets, (void*)RenderTargetsInfo.ColorRenderTarget[0].Texture);
		}
	}
}

void FVulkanCommandListContext::RHICopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, const FResolveParams& InResolveParams)
{
	//FRCLog::Printf(FString::Printf(TEXT("RHICopyToResolveTarget")));
	if (!SourceTextureRHI || !DestTextureRHI)
	{
		// no need to do anything (silently ignored)
		return;
	}

	RHITransitionResources(EResourceTransitionAccess::EReadable, &SourceTextureRHI, 1);

	auto CopyImage = [](FTransitionAndLayoutManager& InRenderPassState, FVulkanCmdBuffer* InCmdBuffer, FVulkanSurface& SrcSurface, FVulkanSurface& DstSurface, uint32 SrcNumLayers, uint32 DstNumLayers, const FResolveParams& ResolveParams)
	{
		VkImageLayout SrcLayout = InRenderPassState.FindLayoutChecked(SrcSurface.Image);
		bool bIsDepth = DstSurface.IsDepthOrStencilAspect();
		VkImageLayout& DstLayout = InRenderPassState.FindOrAddLayoutRW(DstSurface.Image, VK_IMAGE_LAYOUT_UNDEFINED);
		bool bCopyIntoCPUReadable = (DstSurface.UEFlags & TexCreate_CPUReadback) == TexCreate_CPUReadback;

		check(InCmdBuffer->IsOutsideRenderPass());
		VkCommandBuffer CmdBuffer = InCmdBuffer->GetHandle();

		VkImageSubresourceRange SrcRange;
		SrcRange.aspectMask = SrcSurface.GetFullAspectMask();
		SrcRange.baseMipLevel = ResolveParams.MipIndex;
		SrcRange.levelCount = 1;
		SrcRange.baseArrayLayer = ResolveParams.SourceArrayIndex * SrcNumLayers + (SrcNumLayers == 6 ? ResolveParams.CubeFace : 0);
		SrcRange.layerCount = 1;

		VkImageSubresourceRange DstRange;
		DstRange.aspectMask = DstSurface.GetFullAspectMask();
		DstRange.baseMipLevel = ResolveParams.MipIndex;
		DstRange.levelCount = 1;
		DstRange.baseArrayLayer = ResolveParams.DestArrayIndex * DstNumLayers + (DstNumLayers == 6 ? ResolveParams.CubeFace : 0);
		DstRange.layerCount = 1;

		VulkanSetImageLayout(CmdBuffer, SrcSurface.Image, SrcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SrcRange);
		VulkanSetImageLayout(CmdBuffer, DstSurface.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, DstRange);

		VkImageCopy Region;
		FMemory::Memzero(Region);
		ensure(SrcSurface.Width == DstSurface.Width && SrcSurface.Height == DstSurface.Height);
		Region.extent.width = FMath::Max(1u, SrcSurface.Width>> ResolveParams.MipIndex);
		Region.extent.height = FMath::Max(1u, SrcSurface.Height >> ResolveParams.MipIndex);
		Region.extent.depth = 1;
		Region.srcSubresource.aspectMask = SrcSurface.GetFullAspectMask();
		Region.srcSubresource.baseArrayLayer = SrcRange.baseArrayLayer;
		Region.srcSubresource.layerCount = 1;
		Region.srcSubresource.mipLevel = ResolveParams.MipIndex;
		Region.dstSubresource.aspectMask = DstSurface.GetFullAspectMask();
		Region.dstSubresource.baseArrayLayer = DstRange.baseArrayLayer;
		Region.dstSubresource.layerCount = 1;
		Region.dstSubresource.mipLevel = ResolveParams.MipIndex;
		VulkanRHI::vkCmdCopyImage(CmdBuffer,
			SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &Region);

		VulkanSetImageLayout(CmdBuffer, SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SrcLayout, SrcRange);
		if (bCopyIntoCPUReadable)
		{
			VulkanSetImageLayout(CmdBuffer, DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, DstRange);
			DstLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
		else
		{
			DstLayout = bIsDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			VulkanSetImageLayout(CmdBuffer, DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, DstLayout, DstRange);
		}
	};

	FRHITexture2D* SourceTexture2D = SourceTextureRHI->GetTexture2D();
	FRHITexture3D* SourceTexture3D = SourceTextureRHI->GetTexture3D();
	FRHITextureCube* SourceTextureCube = SourceTextureRHI->GetTextureCube();
	FRHITexture2D* DestTexture2D = DestTextureRHI->GetTexture2D();
	FRHITexture3D* DestTexture3D = DestTextureRHI->GetTexture3D();
	FRHITextureCube* DestTextureCube = DestTextureRHI->GetTextureCube();
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();

	if (SourceTexture2D && DestTexture2D)
	{
		FVulkanTexture2D* VulkanSourceTexture = (FVulkanTexture2D*)SourceTexture2D;
		FVulkanTexture2D* VulkanDestTexture = (FVulkanTexture2D*)DestTexture2D;
		if (VulkanSourceTexture->Surface.Image != VulkanDestTexture->Surface.Image) 
		{
			CopyImage(TransitionAndLayoutManager, CmdBuffer, VulkanSourceTexture->Surface, VulkanDestTexture->Surface, 1, 1, InResolveParams);
		}
	}
	else if (SourceTextureCube && DestTextureCube) 
	{
		FVulkanTextureCube* VulkanSourceTexture = (FVulkanTextureCube*)SourceTextureCube;
		FVulkanTextureCube* VulkanDestTexture = (FVulkanTextureCube*)DestTextureCube;
		if (VulkanSourceTexture->Surface.Image != VulkanDestTexture->Surface.Image) 
		{
			CopyImage(TransitionAndLayoutManager, CmdBuffer, VulkanSourceTexture->Surface, VulkanDestTexture->Surface, 6, 6, InResolveParams);
		}
	}
	else if (SourceTexture2D && DestTextureCube) 
	{
		FVulkanTexture2D* VulkanSourceTexture = (FVulkanTexture2D*)SourceTexture2D;
		FVulkanTextureCube* VulkanDestTexture = (FVulkanTextureCube*)DestTextureCube;
		if (VulkanSourceTexture->Surface.Image != VulkanDestTexture->Surface.Image) 
		{
			CopyImage(TransitionAndLayoutManager, CmdBuffer, VulkanSourceTexture->Surface, VulkanDestTexture->Surface, 1, 6, InResolveParams);
		}
	}
	else if (SourceTexture3D && DestTexture3D) 
	{
		FVulkanTexture3D* VulkanSourceTexture = (FVulkanTexture3D*)SourceTexture3D;
		FVulkanTexture3D* VulkanDestTexture = (FVulkanTexture3D*)DestTexture3D;
		if (VulkanSourceTexture->Surface.Image != VulkanDestTexture->Surface.Image)
		{
			CopyImage(TransitionAndLayoutManager, CmdBuffer, VulkanSourceTexture->Surface, VulkanDestTexture->Surface, 1, 1, InResolveParams);
		}
	}
	else 
	{
		checkf(false, TEXT("Using unsupported Resolve combination"));
	}
}

void FVulkanDynamicRHI::RHIReadSurfaceData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
	check(TextureRHI2D);
	FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)TextureRHI2D;
	uint32 NumPixels = TextureRHI2D->GetSizeX() * TextureRHI2D->GetSizeY();

	if (GIgnoreCPUReads == 2)
	{
		OutData.Empty(0);
		OutData.AddZeroed(NumPixels);
		return;
	}

	Device->PrepareForCPURead();

	FVulkanCommandListContext& ImmediateContext = Device->GetImmediateContext();

	FVulkanCmdBuffer* CmdBuffer = ImmediateContext.GetCommandBufferManager()->GetUploadCmdBuffer();

	ensure(Texture2D->Surface.StorageFormat == VK_FORMAT_R8G8B8A8_UNORM || Texture2D->Surface.StorageFormat == VK_FORMAT_B8G8R8A8_UNORM || Texture2D->Surface.StorageFormat == VK_FORMAT_R16G16B16A16_SFLOAT || Texture2D->Surface.StorageFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
	const bool bIs8Bpp = (Texture2D->Surface.StorageFormat == VK_FORMAT_R16G16B16A16_SFLOAT);
	const uint32 Size = NumPixels * sizeof(FColor) * (bIs8Bpp ? 2 : 1);
	VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
	if (GIgnoreCPUReads == 0)
	{
		VkBufferImageCopy CopyRegion;
		FMemory::Memzero(CopyRegion);
		//Region.bufferOffset = 0;
		CopyRegion.bufferRowLength = TextureRHI2D->GetSizeX();
		CopyRegion.bufferImageHeight = TextureRHI2D->GetSizeY();
		CopyRegion.imageSubresource.aspectMask = Texture2D->Surface.GetFullAspectMask();
		//Region.imageSubresource.mipLevel = 0;
		//Region.imageSubresource.baseArrayLayer = 0;
		CopyRegion.imageSubresource.layerCount = 1;
		CopyRegion.imageExtent.width = TextureRHI2D->GetSizeX();
		CopyRegion.imageExtent.height = TextureRHI2D->GetSizeY();
		CopyRegion.imageExtent.depth = 1;

		//#todo-rco: Multithreaded!
		VkImageLayout& CurrentLayout = Device->GetImmediateContext().FindOrAddLayoutRW(Texture2D->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED);
		bool bHadLayout = (CurrentLayout != VK_IMAGE_LAYOUT_UNDEFINED);
		if (CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Texture2D->Surface.Image, CurrentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		}

		VulkanRHI::vkCmdCopyImageToBuffer(CmdBuffer->GetHandle(), Texture2D->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);
		if (bHadLayout && CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Texture2D->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, CurrentLayout);
		}
		else
		{
			CurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		}
	}
	else
	{
		VulkanRHI::vkCmdFillBuffer(CmdBuffer->GetHandle(), StagingBuffer->GetHandle(), 0, Size, (uint32)0xffffffff);
	}

	VkBufferMemoryBarrier Barrier;
	ensure(StagingBuffer->GetSize() >= Size);
	//#todo-rco: Change offset if reusing a buffer suballocation
	VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, StagingBuffer->GetHandle(), 0/*StagingBuffer->GetOffset()*/, Size);
	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &Barrier, 0, nullptr);

	// Force upload
	ImmediateContext.GetCommandBufferManager()->SubmitUploadCmdBuffer();
	Device->WaitUntilIdle();

/*
	VkMappedMemoryRange MappedRange;
	ZeroVulkanStruct(MappedRange, VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
	MappedRange.memory = StagingBuffer->GetDeviceMemoryHandle();
	MappedRange.offset = StagingBuffer->GetAllocationOffset();
	MappedRange.size = Size;
	VulkanRHI::vkInvalidateMappedMemoryRanges(Device->GetInstanceHandle(), 1, &MappedRange);
*/
	StagingBuffer->InvalidateMappedMemory();

	OutData.SetNum(NumPixels);
	FColor* Dest = OutData.GetData();

	if (Texture2D->Surface.StorageFormat == VK_FORMAT_R16G16B16A16_SFLOAT)
	{
		for (int32 Row = Rect.Min.Y; Row < Rect.Max.Y; ++Row)
		{
			FFloat16Color* Src = (FFloat16Color*)StagingBuffer->GetMappedPointer() + Row * TextureRHI2D->GetSizeX() + Rect.Min.X;
			for (int32 Col = Rect.Min.X; Col < Rect.Max.X; ++Col)
			{
				Dest->R = (uint8)(uint32)FMath::Clamp<int32>((int32)(Src->R.GetFloat() * 255.0f), 0, 255);
				Dest->G = (uint8)(uint32)FMath::Clamp<int32>((int32)(Src->G.GetFloat() * 255.0f), 0, 255);
				Dest->B = (uint8)(uint32)FMath::Clamp<int32>((int32)(Src->B.GetFloat() * 255.0f), 0, 255);
				Dest->A = (uint8)(uint32)FMath::Clamp<int32>((int32)(Src->A.GetFloat() * 255.0f), 0, 255);
				Dest++;
				Src++;
			}
		}
	}
	else if (Texture2D->Surface.StorageFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
	{
		struct FR10G10B10A2
		{
			uint32 R : 10;
			uint32 G : 10;
			uint32 B : 10;
			uint32 A : 2;
		};
		for (int32 Row = Rect.Min.Y; Row < Rect.Max.Y; ++Row)
		{
			FR10G10B10A2* Src = (FR10G10B10A2*)StagingBuffer->GetMappedPointer() + Row * TextureRHI2D->GetSizeX() + Rect.Min.X;
			for (int32 Col = Rect.Min.X; Col < Rect.Max.X; ++Col)
			{
				*Dest = FLinearColor(
					(float)Src->R / 1023.0f,
					(float)Src->G / 1023.0f,
					(float)Src->B / 1023.0f,
					(float)Src->A / 3.0f
				).Quantize();
				++Dest;
				++Src;
			}
		}
	}
	else if (Texture2D->Surface.StorageFormat == VK_FORMAT_R8G8B8A8_UNORM)
	{
		for (int32 Row = Rect.Min.Y; Row < Rect.Max.Y; ++Row)
		{
			FColor* Src = (FColor*)StagingBuffer->GetMappedPointer() + Row * TextureRHI2D->GetSizeX() + Rect.Min.X;
			for (int32 Col = Rect.Min.X; Col < Rect.Max.X; ++Col)
			{
				Dest->R = Src->B;
				Dest->G = Src->G;
				Dest->B = Src->R;
				Dest->A = Src->A;
				Dest++;
				Src++;
			}
		}
	}
	else if (Texture2D->Surface.StorageFormat == VK_FORMAT_B8G8R8A8_UNORM)
	{
		FColor* Src = (FColor*)StagingBuffer->GetMappedPointer() + Rect.Min.Y * TextureRHI2D->GetSizeX() + Rect.Min.X;
		for (int32 Row = Rect.Min.Y; Row < Rect.Max.Y; ++Row)
		{
			const int32 NumCols = Rect.Max.X - Rect.Min.X;
			FMemory::Memcpy(Dest, Src, NumCols * sizeof(FColor));
			Src += TextureRHI2D->GetSizeX();
			Dest += NumCols;
		}
	}

	Device->GetStagingManager().ReleaseBuffer(CmdBuffer, StagingBuffer);
	ImmediateContext.GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
}

void FVulkanDynamicRHI::RHIMapStagingSurface(FTextureRHIParamRef TextureRHI,void*& OutData,int32& OutWidth,int32& OutHeight)
{
	FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
	check(TextureRHI2D);
	FVulkanTexture2D* Texture2D = ResourceCast(TextureRHI2D);

	VulkanRHI::FStagingBuffer** StagingBufferPtr = nullptr;
	{
		FScopeLock Lock(&GStagingMapLock);
		StagingBufferPtr = &GPendingLockedStagingBuffers.FindOrAdd(Texture2D);
		checkf(!*StagingBufferPtr, TEXT("Can't map the same texture twice!"));
	}

	OutWidth = Texture2D->GetSizeX();
	OutHeight = Texture2D->GetSizeY();

	uint32 BufferSize = OutWidth * OutHeight * VulkanRHI::GetNumBitsPerPixel(Texture2D->Surface.ViewFormat) / 8;
	VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(BufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
	*StagingBufferPtr = StagingBuffer;

	Device->PrepareForCPURead(); //make sure the results are ready 
	FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();

	// Transition texture to source copy layout
	VkImageLayout& CurrentLayout = Device->GetImmediateContext().TransitionAndLayoutManager.FindOrAddLayoutRW(Texture2D->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED);
	bool bHadLayout = (CurrentLayout != VK_IMAGE_LAYOUT_UNDEFINED);
	if (CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Texture2D->Surface.Image, CurrentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	}

	VkBufferImageCopy CopyRegion;
	FMemory::Memzero(CopyRegion);
	//Region.bufferOffset = 0;
	CopyRegion.bufferRowLength = OutWidth;
	CopyRegion.bufferImageHeight = OutHeight;
	CopyRegion.imageSubresource.aspectMask = Texture2D->Surface.GetFullAspectMask();
	//CopyRegion.imageSubresource.mipLevel = InMipIndex;
	//CopyRegion.imageSubresource.baseArrayLayer = SrcBaseArrayLayer;
	CopyRegion.imageSubresource.layerCount = 1;
	CopyRegion.imageExtent.width = OutWidth;
	CopyRegion.imageExtent.height = OutHeight;
	CopyRegion.imageExtent.depth = 1;

	VulkanRHI::vkCmdCopyImageToBuffer(CmdBuffer->GetHandle(), Texture2D->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);
	// Transition back to original layout
	if (bHadLayout && CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Texture2D->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, CurrentLayout);
	}
	else
	{
		CurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}

	VkBufferMemoryBarrier Barrier;
	ensure(StagingBuffer->GetSize() >= BufferSize);
	//#todo-rco: Change offset if reusing a buffer suballocation
	VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, StagingBuffer->GetHandle(), 0/*StagingBuffer->GetOffset()*/, BufferSize);
	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &Barrier, 0, nullptr);

	Device->GetImmediateContext().GetCommandBufferManager()->SubmitUploadCmdBuffer();

	OutData = StagingBuffer->GetMappedPointer();
	StagingBuffer->InvalidateMappedMemory();
}

void FVulkanDynamicRHI::RHIUnmapStagingSurface(FTextureRHIParamRef TextureRHI)
{
	FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
	check(TextureRHI2D);
	FVulkanTexture2D* Texture2D = ResourceCast(TextureRHI2D);

	VulkanRHI::FStagingBuffer* StagingBuffer = nullptr;
	{
		FScopeLock Lock(&GStagingMapLock);
		bool bFound = GPendingLockedStagingBuffers.RemoveAndCopyValue(Texture2D, StagingBuffer);
		checkf(bFound, TEXT("Texture was not mapped!"));
	}

	ensure(!Device->GetImmediateContext().GetCommandBufferManager()->HasPendingUploadCmdBuffer());

	Device->GetImmediateContext().GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
	Device->GetStagingManager().ReleaseBuffer(nullptr, StagingBuffer);
}

void FVulkanDynamicRHI::RHIReadSurfaceFloatData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex)
{
	auto DoCopyFloat = [](FVulkanDevice* InDevice, FVulkanCmdBuffer* InCmdBuffer, const FVulkanSurface& Surface, uint32 InMipIndex, uint32 SrcBaseArrayLayer, FIntRect InRect, TArray<FFloat16Color>& OutputData)
	{
		ensure(Surface.StorageFormat == VK_FORMAT_R16G16B16A16_SFLOAT);

		uint32 NumPixels = (Surface.Width >> InMipIndex) * (Surface.Height >> InMipIndex);
		const uint32 Size = NumPixels * sizeof(FFloat16Color);
		VulkanRHI::FStagingBuffer* StagingBuffer = InDevice->GetStagingManager().AcquireBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);

		if (GIgnoreCPUReads == 0)
		{
			VkBufferImageCopy CopyRegion;
			FMemory::Memzero(CopyRegion);
			//Region.bufferOffset = 0;
			CopyRegion.bufferRowLength = Surface.Width >> InMipIndex;
			CopyRegion.bufferImageHeight = Surface.Height >> InMipIndex;
			CopyRegion.imageSubresource.aspectMask = Surface.GetFullAspectMask();
			CopyRegion.imageSubresource.mipLevel = InMipIndex;
			CopyRegion.imageSubresource.baseArrayLayer = SrcBaseArrayLayer;
			CopyRegion.imageSubresource.layerCount = 1;
			CopyRegion.imageExtent.width = Surface.Width >> InMipIndex;
			CopyRegion.imageExtent.height = Surface.Height >> InMipIndex;
			CopyRegion.imageExtent.depth = 1;

			//#todo-rco: Multithreaded!
			VkImageLayout& CurrentLayout = InDevice->GetImmediateContext().FindOrAddLayoutRW(Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED);
			bool bHadLayout = (CurrentLayout != VK_IMAGE_LAYOUT_UNDEFINED);
			if (CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				VulkanSetImageLayoutSimple(InCmdBuffer->GetHandle(), Surface.Image, CurrentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			}

			VulkanRHI::vkCmdCopyImageToBuffer(InCmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);

			if (bHadLayout && CurrentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				VulkanSetImageLayoutSimple(InCmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, CurrentLayout);
			}
			else
			{
				CurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			}
		}
		else
		{
			VulkanRHI::vkCmdFillBuffer(InCmdBuffer->GetHandle(), StagingBuffer->GetHandle(), 0, Size, (FFloat16(1.0).Encoded << 16) + FFloat16(1.0).Encoded);
		}

		VkBufferMemoryBarrier Barrier;
		// the staging buffer size may be bigger then the size due to alignment, etc. but it must not be smaller!
		ensure(StagingBuffer->GetSize() >= Size);
		//#todo-rco: Change offset if reusing a buffer suballocation
		VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, StagingBuffer->GetHandle(), 0/*StagingBuffer->GetOffset()*/, StagingBuffer->GetSize());
		VulkanRHI::vkCmdPipelineBarrier(InCmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &Barrier, 0, nullptr);

		// Force upload
		InDevice->GetImmediateContext().GetCommandBufferManager()->SubmitUploadCmdBuffer();
		InDevice->WaitUntilIdle();

		StagingBuffer->InvalidateMappedMemory();
/*
		VkMappedMemoryRange MappedRange;
		ZeroVulkanStruct(MappedRange, VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
		MappedRange.memory = StagingBuffer->GetDeviceMemoryHandle();
		MappedRange.offset = StagingBuffer->GetAllocationOffset();
		MappedRange.size = Size;
		VulkanRHI::vkInvalidateMappedMemoryRanges(InDevice->GetInstanceHandle(), 1, &MappedRange);
*/

		OutputData.SetNum(NumPixels);
		FFloat16Color* Dest = OutputData.GetData();
		for (int32 Row = InRect.Min.Y; Row < InRect.Max.Y; ++Row)
		{
			FFloat16Color* Src = (FFloat16Color*)StagingBuffer->GetMappedPointer() + Row * (Surface.Width >> InMipIndex) + InRect.Min.X;
			for (int32 Col = InRect.Min.X; Col < InRect.Max.X; ++Col)
			{
				*Dest++ = *Src++;
			}
		}
		InDevice->GetStagingManager().ReleaseBuffer(InCmdBuffer, StagingBuffer);
	};

	if (GIgnoreCPUReads == 2)
	{
		// FIll with CPU
		uint32 NumPixels = 0;
		if (TextureRHI->GetTextureCube())
		{
			FRHITextureCube* TextureRHICube = TextureRHI->GetTextureCube();
			FVulkanTextureCube* TextureCube = (FVulkanTextureCube*)TextureRHICube;
			NumPixels = (TextureCube->Surface.Width >> MipIndex) * (TextureCube->Surface.Height >> MipIndex);
		}
		else
		{
			FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
			check(TextureRHI2D);
			FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)TextureRHI2D;
			NumPixels = (Texture2D->Surface.Width >> MipIndex) * (Texture2D->Surface.Height >> MipIndex);
		}

		OutData.Empty(0);
		OutData.AddZeroed(NumPixels);
	}
	else
	{
		Device->PrepareForCPURead();

		FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();
		if (TextureRHI->GetTextureCube())
		{
			FRHITextureCube* TextureRHICube = TextureRHI->GetTextureCube();
			FVulkanTextureCube* TextureCube = (FVulkanTextureCube*)TextureRHICube;
			DoCopyFloat(Device, CmdBuffer, TextureCube->Surface, MipIndex, CubeFace, Rect, OutData);
		}
		else
		{
			FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
			check(TextureRHI2D);
			FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)TextureRHI2D;
			DoCopyFloat(Device, CmdBuffer, Texture2D->Surface, MipIndex, 0, Rect, OutData);
		}
		Device->GetImmediateContext().GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
	}
}

void FVulkanDynamicRHI::RHIRead3DSurfaceFloatData(FTextureRHIParamRef TextureRHI,FIntRect InRect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
{
	Device->PrepareForCPURead();

	VULKAN_SIGNAL_UNIMPLEMENTED();

	Device->GetImmediateContext().GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();

}

void FVulkanCommandListContext::RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFenceRHI)
{
	FPendingTransition PendingTransition;
	if (NumUAVs > 0)
	{
		for (int32 Index = 0; Index < NumUAVs; ++Index)
		{
			if (InUAVs[Index])
			{
				PendingTransition.UAVs.Add(InUAVs[Index]);
			}
		}

		if (PendingTransition.UAVs.Num() > 0)
		{
			PendingTransition.TransitionType = TransitionType;
			PendingTransition.TransitionPipeline = TransitionPipeline;
			PendingTransition.WriteComputeFenceRHI = WriteComputeFenceRHI;
			TransitionResources(PendingTransition);
		}
	}
}

void FVulkanCommandListContext::RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures)
{
	if (NumTextures > 0)
	{
		FPendingTransition PendingTransition;
		for (int32 Index = 0; Index < NumTextures; ++Index)
		{
			if (InTextures[Index])
			{
				PendingTransition.Textures.Add(InTextures[Index]);
			}
		}

		if (PendingTransition.Textures.Num() > 0)
		{
			PendingTransition.TransitionType = TransitionType;
			TransitionResources(PendingTransition);
		}
	}
}


bool FVulkanCommandListContext::FPendingTransition::GatherBarriers(FTransitionAndLayoutManager& InTransitionAndLayoutManager, TArray<VkBufferMemoryBarrier>& OutBufferBarriers, TArray<VkImageMemoryBarrier>& OutImageBarriers) const
{
	bool bEmpty = true;
	for (int32 Index = 0; Index < UAVs.Num(); ++Index)
	{
		FVulkanUnorderedAccessView* UAV = ResourceCast(UAVs[Index]);

		VkAccessFlags SrcAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, DestAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		switch (TransitionType)
		{
		case EResourceTransitionAccess::EWritable:
			SrcAccess = VK_ACCESS_SHADER_READ_BIT;
			DestAccess = VK_ACCESS_SHADER_WRITE_BIT;
			break;
		case EResourceTransitionAccess::EReadable:
			SrcAccess = VK_ACCESS_SHADER_WRITE_BIT;
			DestAccess = VK_ACCESS_SHADER_READ_BIT;
			break;
		case EResourceTransitionAccess::ERWBarrier:
			SrcAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			DestAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			break;
		case EResourceTransitionAccess::ERWNoBarrier:
			//#todo-rco: Skip for now
			continue;
		default:
			ensure(0);
			break;
		}

		if (!UAV)
		{
			continue;
		}

		if (UAV->SourceVertexBuffer)
		{
			VkBufferMemoryBarrier& Barrier = OutBufferBarriers[OutBufferBarriers.AddUninitialized()];
			VulkanRHI::SetupAndZeroBufferBarrier(Barrier, SrcAccess, DestAccess, UAV->SourceVertexBuffer->GetHandle(), UAV->SourceVertexBuffer->GetOffset(), UAV->SourceVertexBuffer->GetSize());
			bEmpty = false;
		}
		else if (UAV->SourceTexture)
		{
			VkImageMemoryBarrier& Barrier = OutImageBarriers[OutImageBarriers.AddUninitialized()];
			FVulkanTextureBase* VulkanTexture = FVulkanTextureBase::Cast(UAV->SourceTexture);
			VkImageLayout DestLayout = (TransitionPipeline == EResourceTransitionPipeline::EComputeToGfx || TransitionPipeline == EResourceTransitionPipeline::EGfxToGfx)
				? (VulkanTexture->Surface.IsDepthOrStencilAspect() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				: VK_IMAGE_LAYOUT_GENERAL;

			VkImageLayout& Layout = InTransitionAndLayoutManager.FindOrAddLayoutRW(VulkanTexture->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED);
			VulkanRHI::SetupAndZeroImageBarrierOLD(Barrier, VulkanTexture->Surface, SrcAccess, Layout, DestAccess, DestLayout);
			Layout = DestLayout;
			bEmpty = false;
		}
		else if (UAV->SourceStructuredBuffer)
		{
			VkBufferMemoryBarrier& Barrier = OutBufferBarriers[OutBufferBarriers.AddUninitialized()];
			VulkanRHI::SetupAndZeroBufferBarrier(Barrier, SrcAccess, DestAccess, UAV->SourceStructuredBuffer->GetHandle(), UAV->SourceStructuredBuffer->GetOffset(), UAV->SourceStructuredBuffer->GetSize());
			bEmpty = false;
		}
		else if (UAV->SourceIndexBuffer)
		{
			VkBufferMemoryBarrier& Barrier = OutBufferBarriers[OutBufferBarriers.AddUninitialized()];
			VulkanRHI::SetupAndZeroBufferBarrier(Barrier, SrcAccess, DestAccess, UAV->SourceIndexBuffer->GetHandle(), UAV->SourceIndexBuffer->GetOffset(), UAV->SourceIndexBuffer->GetSize());
			bEmpty = false;
		}
		else
		{
			ensure(0);
		}
	}

	return !bEmpty;
}


void FVulkanCommandListContext::TransitionResources(const FPendingTransition& PendingTransition)
{
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;

	if (PendingTransition.Textures.Num() > 0)
	{
		ensure(IsImmediate() || Device->IsRealAsyncComputeContext(this));

		SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResources, bShowTransitionEvents, TEXT("TransitionTo: %s: %i Textures"), *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)PendingTransition.TransitionType], PendingTransition.Textures.Num());

		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		check(CmdBuffer->HasBegun());

		//#todo-rco: Metadata is kind of a hack as decals do not have a read transition yet
		if (PendingTransition.TransitionType == EResourceTransitionAccess::EReadable || PendingTransition.TransitionType == EResourceTransitionAccess::EMetaData)
		{
			if (TransitionAndLayoutManager.CurrentRenderPass)
			{
				// If any of the textures are in the current render pass, we need to end it
				uint32 TexturesInsideRenderPass = 0;
				for (int32 Index = 0; Index < PendingTransition.Textures.Num(); ++Index)
				{
					FVulkanTextureBase* VulkanTexture = FVulkanTextureBase::Cast(PendingTransition.Textures[Index]);
					VkImage Image = VulkanTexture->Surface.Image;
					if (TransitionAndLayoutManager.CurrentFramebuffer->ContainsRenderTarget(Image))
					{
						++TexturesInsideRenderPass;
						bool bIsDepthStencil = VulkanTexture->Surface.IsDepthOrStencilAspect();
						VkImageLayout FoundLayout = TransitionAndLayoutManager.FindOrAddLayout(Image, VK_IMAGE_LAYOUT_UNDEFINED);
						VkImageLayout EnsureLayout = (bIsDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
						if (FoundLayout != VK_IMAGE_LAYOUT_UNDEFINED)
						{
							ensure(FoundLayout == EnsureLayout);
						}
					}
					else
					{
						//ensureMsgf(0, TEXT("Unable to transition texture as we're inside a render pass!"));
					}
				}

				if (TexturesInsideRenderPass > 0)
				{
					TransitionAndLayoutManager.EndEmulatedRenderPass(CmdBuffer);

					if (GVulkanSubmitAfterEveryEndRenderPass)
					{
						CommandBufferManager->SubmitActiveCmdBuffer();
						CommandBufferManager->PrepareForNewActiveCommandBuffer();
						CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
					}
				}
			}

			if (bShowTransitionEvents)
			{
				for (int32 Index = 0; Index < PendingTransition.Textures.Num(); ++Index)
				{
					SCOPED_RHI_DRAW_EVENTF(*this, RHITransitionResourcesLoop, TEXT("To:%i - %s"), Index, *PendingTransition.Textures[Index]->GetName().ToString());
				}
			}

			VulkanRHI::FPendingBarrier Barrier;
			for (int32 Index = 0; Index < PendingTransition.Textures.Num(); ++Index)
			{
				FVulkanTextureBase* VulkanTexture = FVulkanTextureBase::Cast(PendingTransition.Textures[Index]);
				VkImageLayout& SrcLayout = TransitionAndLayoutManager.FindOrAddLayoutRW(VulkanTexture->Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED);
				bool bIsDepthStencil = VulkanTexture->Surface.IsDepthOrStencilAspect();
				// During HMD rendering we get a frame where nothing is rendered into the depth buffer, but CopyToTexture is still called...
				// ensure(SrcLayout != VK_IMAGE_LAYOUT_UNDEFINED || bIsDepthStencil);
				VkImageLayout DstLayout = bIsDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				int32 BarrierIndex = Barrier.AddImageBarrier(VulkanTexture->Surface.Image, VulkanTexture->Surface.GetFullAspectMask(), VulkanTexture->Surface.GetNumMips(), VulkanTexture->Surface.GetNumberOfArrayLevels());
				Barrier.SetTransition(BarrierIndex, VulkanRHI::GetImageLayoutFromVulkanLayout(SrcLayout), VulkanRHI::GetImageLayoutFromVulkanLayout(DstLayout));

				SrcLayout = DstLayout;
			}
			//#todo-rco: Temp ensure disabled
			Barrier.Execute(CmdBuffer, false);
		}
		else if (PendingTransition.TransitionType == EResourceTransitionAccess::EWritable)
		{
			//#todo-rco: Until render passes come online, assume writable means end render pass
			if (TransitionAndLayoutManager.CurrentRenderPass)
			{
				TransitionAndLayoutManager.EndEmulatedRenderPass(CmdBuffer);
				if (GVulkanSubmitAfterEveryEndRenderPass)
				{
					CommandBufferManager->SubmitActiveCmdBuffer();
					CommandBufferManager->PrepareForNewActiveCommandBuffer();
					CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
				}
			}

			if (bShowTransitionEvents)
			{
				for (int32 i = 0; i < PendingTransition.Textures.Num(); ++i)
				{
					FRHITexture* RHITexture = PendingTransition.Textures[i];
					SCOPED_RHI_DRAW_EVENTF(*this, RHITransitionResourcesLoop, TEXT("To:%i - %s"), i, *PendingTransition.Textures[i]->GetName().ToString());
				}
			}

			VulkanRHI::FPendingBarrier Barrier;

			for (int32 Index = 0; Index < PendingTransition.Textures.Num(); ++Index)
			{
				FVulkanSurface& Surface = FVulkanTextureBase::Cast(PendingTransition.Textures[Index])->Surface;

				const VkImageAspectFlags AspectMask = Surface.GetFullAspectMask();
				VkImageSubresourceRange SubresourceRange = {AspectMask, 0, Surface.GetNumMips(), 0, Surface.GetNumberOfArrayLevels()};

				VkImageLayout& SrcLayout = TransitionAndLayoutManager.FindOrAddLayoutRW(Surface.Image, VK_IMAGE_LAYOUT_UNDEFINED);

				//int32 BarrierIndex = Barrier.AddImageBarrier(Surface.Image, Surface.GetFullAspectMask(), Surface.GetNumMips(), Surface.GetNumberOfArrayLevels());

				if ((AspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0)
				{
					VkImageLayout FinalLayout = (Surface.UEFlags & TexCreate_RenderTargetable) ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
					if (SrcLayout != FinalLayout)
					{
						//#todo-rco: Switch to pending barrier
						VulkanSetImageLayout(CmdBuffer->GetHandle(), Surface.Image, SrcLayout, FinalLayout, SubresourceRange);
						SrcLayout = FinalLayout;
					}
				}
				else
				{
					if (SrcLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
					{
						check(Surface.IsDepthOrStencilAspect());
						//#todo-rco: Switch to pending barrier
						VulkanSetImageLayout(CmdBuffer->GetHandle(), Surface.Image, SrcLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, SubresourceRange);
						SrcLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					}
				}
			}

			Barrier.Execute(CmdBuffer);
		}
		else if (PendingTransition.TransitionType == EResourceTransitionAccess::ERWSubResBarrier)
		{
			// This mode is only used for generating mipmaps only - old style
			if (CmdBuffer->IsInsideRenderPass())
			{
				check(PendingTransition.Textures.Num() == 1);
				TransitionAndLayoutManager.EndEmulatedRenderPass(CmdBuffer);

				if (GVulkanSubmitAfterEveryEndRenderPass)
				{
					CommandBufferManager->SubmitActiveCmdBuffer();
					CommandBufferManager->PrepareForNewActiveCommandBuffer();
					CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
				}
			}
		}
		else if (PendingTransition.TransitionType == EResourceTransitionAccess::EMetaData)
		{
			// Nothing to do here
		}
		else
		{
			ensure(0);
		}

		if (CommandBufferManager->GetActiveCmdBuffer()->IsOutsideRenderPass())
		{
			if (SafePointSubmit())
			{
				CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
			}
		}
	}
	else
	{
		const bool bIsRealAsyncComputeContext = Device->IsRealAsyncComputeContext(this);
		ensure(IsImmediate() || bIsRealAsyncComputeContext);
		check(PendingTransition.UAVs.Num() > 0);
		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		TArray<VkBufferMemoryBarrier> BufferBarriers;
		TArray<VkImageMemoryBarrier> ImageBarriers;
		if (PendingTransition.GatherBarriers(TransitionAndLayoutManager, BufferBarriers, ImageBarriers))
		{
			// If we can support async compute, add this if writing a fence from the gfx context, or transitioning queues (as it requires transferring ownership of resources)
			if (Device->HasAsyncComputeQueue() &&
				(this == &Device->GetImmediateComputeContext() ||
					(PendingTransition.WriteComputeFenceRHI && (PendingTransition.TransitionPipeline == EResourceTransitionPipeline::EComputeToGfx || PendingTransition.TransitionPipeline == EResourceTransitionPipeline::EGfxToCompute))))
			{
				TransitionUAVResourcesTransferringOwnership(Device->GetImmediateContext(), Device->GetImmediateComputeContext(), PendingTransition.TransitionPipeline, BufferBarriers, ImageBarriers);
			}
			else
			{
				// 'Vanilla' transitions within the same queue
				VkPipelineStageFlags SourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, DestStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
				switch (PendingTransition.TransitionPipeline)
				{
				case EResourceTransitionPipeline::EGfxToCompute:
					SourceStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
					DestStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
					break;
				case EResourceTransitionPipeline::EComputeToGfx:
					SourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
					DestStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
					break;
				case EResourceTransitionPipeline::EComputeToCompute:
					SourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
					DestStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
					break;
				default:
					ensure(0);
					break;
				}

				if (BufferBarriers.Num() && TransitionAndLayoutManager.CurrentRenderPass != nullptr)
				{
					TransitionAndLayoutManager.EndEmulatedRenderPass(CmdBuffer);

					if (GVulkanSubmitAfterEveryEndRenderPass)
					{
						CommandBufferManager->SubmitActiveCmdBuffer();
						CommandBufferManager->PrepareForNewActiveCommandBuffer();
						CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
					}
				}

				VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), SourceStage, DestStage, 0, 0, nullptr, BufferBarriers.Num(), BufferBarriers.GetData(), ImageBarriers.Num(), ImageBarriers.GetData());
			}
		}

		if (PendingTransition.WriteComputeFenceRHI)
		{
			// Can't do events between queues
			FVulkanComputeFence* Fence = ResourceCast(PendingTransition.WriteComputeFenceRHI);
			Fence->WriteCmd(CmdBuffer->GetHandle(), !bIsRealAsyncComputeContext);
		}
	}
}


void FVulkanCommandListContext::TransitionUAVResourcesTransferringOwnership(FVulkanCommandListContext& GfxContext, FVulkanCommandListContext& ComputeContext, 
	EResourceTransitionPipeline Pipeline, const TArray<VkBufferMemoryBarrier>& InBufferBarriers, const TArray<VkImageMemoryBarrier>& InImageBarriers)
{
	auto DoBarriers = [&InImageBarriers, &InBufferBarriers](uint32 SrcQueueIndex, uint32 DestQueueIndex, FVulkanCmdBuffer* SrcCmdBuffer, FVulkanCmdBuffer* DstCmdBuffer, VkPipelineStageFlags SrcStageFlags, VkPipelineStageFlags DestStageFlags)
	{
		TArray<VkBufferMemoryBarrier> BufferBarriers = InBufferBarriers;
		TArray<VkImageMemoryBarrier> ImageBarriers = InImageBarriers;

		// Release resources
		for (VkBufferMemoryBarrier& Barrier : BufferBarriers)
		{
			Barrier.dstAccessMask = 0;
			Barrier.srcQueueFamilyIndex = SrcQueueIndex;
			Barrier.dstQueueFamilyIndex = DestQueueIndex;
		}

		for (VkImageMemoryBarrier& Barrier : ImageBarriers)
		{
			Barrier.dstAccessMask = 0;
			Barrier.srcQueueFamilyIndex = SrcQueueIndex;
			Barrier.dstQueueFamilyIndex = DestQueueIndex;
		}

		VulkanRHI::vkCmdPipelineBarrier(SrcCmdBuffer->GetHandle(), SrcStageFlags, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, BufferBarriers.Num(), BufferBarriers.GetData(), ImageBarriers.Num(), ImageBarriers.GetData());

		// Now acquire and restore dstAccessMask
		for (VkBufferMemoryBarrier& Barrier : BufferBarriers)
		{
			Barrier.srcAccessMask = 0;
			size_t Index = &Barrier - &BufferBarriers[0];
			Barrier.dstAccessMask = InBufferBarriers[Index].dstAccessMask;
		}

		for (VkImageMemoryBarrier& Barrier : ImageBarriers)
		{
			Barrier.srcAccessMask = 0;
			size_t Index = &Barrier - &ImageBarriers[0];
			Barrier.dstAccessMask = ImageBarriers[Index].dstAccessMask;
		}

		VulkanRHI::vkCmdPipelineBarrier(DstCmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, DestStageFlags, 0, 0, nullptr, BufferBarriers.Num(), BufferBarriers.GetData(), ImageBarriers.Num(), ImageBarriers.GetData());
	};

	bool bComputeToGfx = Pipeline == EResourceTransitionPipeline::EComputeToGfx;
	ensure(bComputeToGfx || Pipeline == EResourceTransitionPipeline::EGfxToCompute);
	uint32 GfxQueueIndex = GfxContext.Device->GetGraphicsQueue()->GetFamilyIndex();
	uint32 ComputeQueueIndex = ComputeContext.Device->GetComputeQueue()->GetFamilyIndex();
	FVulkanCmdBuffer* GfxCmdBuffer = GfxContext.GetCommandBufferManager()->GetActiveCmdBuffer();
	if (!ComputeContext.GetCommandBufferManager()->HasPendingActiveCmdBuffer())
	{
		ComputeContext.GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
	}
	FVulkanCmdBuffer* ComputeCmdBuffer = ComputeContext.GetCommandBufferManager()->GetActiveCmdBuffer();
	if (bComputeToGfx)
	{
		DoBarriers(ComputeQueueIndex, GfxQueueIndex, ComputeCmdBuffer, GfxCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
	}
	else
	{
		DoBarriers(GfxQueueIndex, ComputeQueueIndex, GfxCmdBuffer, ComputeCmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}
}


void FVulkanCommandListContext::RHIBeginRenderPass(const FRHIRenderPassInfo& InInfo, const TCHAR* InName)
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (TransitionAndLayoutManager.CurrentRenderPass)
	{
		checkf(!TransitionAndLayoutManager.bInsideRealRenderPass, TEXT("Didn't call RHIEndRenderPass()!"));
		TransitionAndLayoutManager.EndEmulatedRenderPass(CmdBuffer);
	}

	TransitionAndLayoutManager.bInsideRealRenderPass = false;

	if (GVulkanSubmitAfterEveryEndRenderPass)
	{
		CommandBufferManager->SubmitActiveCmdBuffer();
		CommandBufferManager->PrepareForNewActiveCommandBuffer();
		CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	}
	else if (SafePointSubmit())
	{
		CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	}

	RenderPassInfo = InInfo;
	RHIPushEvent(InName ? InName : TEXT("<unnamed RenderPass>"), FColor::Green);
	if (InInfo.bOcclusionQueries)
	{
		BeginOcclusionQueryBatch(CmdBuffer, InInfo.NumOcclusionQueries);
	}
	FVulkanRenderTargetLayout RTLayout(*Device, InInfo);
	check(RTLayout.GetExtent2D().width != 0 && RTLayout.GetExtent2D().height != 0);
	FVulkanRenderPass* RenderPass = TransitionAndLayoutManager.GetOrCreateRenderPass(*Device, RTLayout);
	FRHISetRenderTargetsInfo RTInfo;
	InInfo.ConvertToRenderTargetsInfo(RTInfo);
	FVulkanFramebuffer* Framebuffer = TransitionAndLayoutManager.GetOrCreateFramebuffer(*Device, RTInfo, RTLayout, RenderPass);
	checkf(RenderPass != nullptr && Framebuffer != nullptr, TEXT("RenderPass not started! Bad combination of values? Depth %p #Color %d Color0 %p"), (void*)InInfo.DepthStencilRenderTarget.DepthStencilTarget, InInfo.GetNumColorRenderTargets(), (void*)InInfo.ColorRenderTargets[0].RenderTarget);
	TransitionAndLayoutManager.BeginRealRenderPass(*this, *Device, CmdBuffer, InInfo, RTLayout, RenderPass, Framebuffer);
}


void FVulkanCommandListContext::RHIEndRenderPass()
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (RenderPassInfo.bOcclusionQueries)
	{
		EndOcclusionQueryBatch(CmdBuffer);
	}
	else
	{
		TransitionAndLayoutManager.EndRealRenderPass(CmdBuffer);
	}
	RHIPopEvent();
}

// Need a separate struct so we can memzero/remove dependencies on reference counts
struct FRenderPassCompatibleHashableStruct
{
	FRenderPassCompatibleHashableStruct()
	{
		FMemory::Memzero(*this);
	}

	uint8						NumAttachments;
	uint8						NumSamples;
	// +1 for DepthStencil
	VkFormat					Formats[MaxSimultaneousRenderTargets + 1];
};

// Need a separate struct so we can memzero/remove dependencies on reference counts
struct FRenderPassFullHashableStruct
{
	FRenderPassFullHashableStruct()
	{
		FMemory::Memzero(*this);
	}

	// +1 for Depth, +1 for Stencil
	TEnumAsByte<VkAttachmentLoadOp>		LoadOps[MaxSimultaneousRenderTargets + 2];
	TEnumAsByte<VkAttachmentStoreOp>	StoreOps[MaxSimultaneousRenderTargets + 2];
};


FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(FVulkanDevice& InDevice, const FRHISetRenderTargetsInfo& RTInfo)
	: NumAttachmentDescriptions(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, NumSamples(0)
	, NumUsedClearValues(0)
{
	FMemory::Memzero(ColorReferences);
	FMemory::Memzero(DepthStencilReference);
	FMemory::Memzero(ResolveReferences);
	FMemory::Memzero(InputAttachments);
	FMemory::Memzero(Desc);
	FMemory::Memzero(Extent);

	FRenderPassCompatibleHashableStruct CompatibleHashInfo;
	FRenderPassFullHashableStruct FullHashInfo;

	bool bSetExtent = false;
	bool bFoundClearOp = false;
	for (int32 Index = 0; Index < RTInfo.NumColorRenderTargets; ++Index)
	{
		const FRHIRenderTargetView& RTView = RTInfo.ColorRenderTarget[Index];
		if (RTView.Texture)
		{
			FVulkanTextureBase* Texture = FVulkanTextureBase::Cast(RTView.Texture);
			check(Texture);
	
			if (bSetExtent)
			{
				ensure(Extent.Extent3D.width == FMath::Max(1u, Texture->Surface.Width >> RTView.MipIndex));
				ensure(Extent.Extent3D.height == FMath::Max(1u, Texture->Surface.Height >> RTView.MipIndex));
				ensure(Extent.Extent3D.depth == Texture->Surface.Depth);
			}
			else
			{
				bSetExtent = true;
				Extent.Extent3D.width = FMath::Max(1u, Texture->Surface.Width >> RTView.MipIndex);
				Extent.Extent3D.height = FMath::Max(1u, Texture->Surface.Height >> RTView.MipIndex);
				Extent.Extent3D.depth = Texture->Surface.Depth;
			}

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
			FVulkanSurface* Surface = Texture->MSAASurface ? Texture->MSAASurface : &Texture->Surface;
#else
			FVulkanSurface* Surface = &Texture->Surface;
#endif

			ensure(!NumSamples || NumSamples == Surface->GetNumSamples());
			NumSamples = Surface->GetNumSamples();
		
			VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
			CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
			CurrDesc.format = UEToVkFormat(RTView.Texture->GetFormat(), (Texture->Surface.UEFlags & TexCreate_SRGB) == TexCreate_SRGB);
			CurrDesc.loadOp = RenderTargetLoadActionToVulkan(RTView.LoadAction);
			bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
			CurrDesc.storeOp = RenderTargetStoreActionToVulkan(RTView.StoreAction);
			CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			CurrDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			ColorReferences[NumColorAttachments].attachment = NumAttachmentDescriptions;
			ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
			{
				Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
				Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
				ResolveReferences[NumColorAttachments].attachment = NumAttachmentDescriptions + 1;
				ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_GENERAL;
				++NumAttachmentDescriptions;
				bHasResolveAttachments = true;
			}

			CompatibleHashInfo.Formats[NumColorAttachments] = CurrDesc.format;
			FullHashInfo.LoadOps[NumColorAttachments] = CurrDesc.loadOp;
			FullHashInfo.StoreOps[NumColorAttachments] = CurrDesc.storeOp;
			++CompatibleHashInfo.NumAttachments;

			++NumAttachmentDescriptions;
			++NumColorAttachments;
		}
	}

	VkImageLayout DepthStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (RTInfo.DepthStencilRenderTarget.Texture)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);
		FVulkanTextureBase* Texture = FVulkanTextureBase::Cast(RTInfo.DepthStencilRenderTarget.Texture);
		check(Texture);

#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
		FVulkanSurface* Surface = Texture->MSAASurface ? Texture->MSAASurface : &Texture->Surface;
#else
		FVulkanSurface* Surface = &Texture->Surface;
#endif
		ensure(!NumSamples || NumSamples == Surface->GetNumSamples());
		NumSamples = Surface->GetNumSamples();

		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
		CurrDesc.format = UEToVkFormat(RTInfo.DepthStencilRenderTarget.Texture->GetFormat(), false);
		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(RTInfo.DepthStencilRenderTarget.DepthLoadAction);
		CurrDesc.stencilLoadOp = RenderTargetLoadActionToVulkan(RTInfo.DepthStencilRenderTarget.StencilLoadAction);
		bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
		if (CurrDesc.samples == VK_SAMPLE_COUNT_1_BIT)
		{
			CurrDesc.storeOp = RenderTargetStoreActionToVulkan(RTInfo.DepthStencilRenderTarget.DepthStoreAction);
			CurrDesc.stencilStoreOp = RenderTargetStoreActionToVulkan(RTInfo.DepthStencilRenderTarget.GetStencilStoreAction());
		}
		else
		{
			// Never want to store MSAA depth/stencil
			CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}

		DepthStencilLayout = VulkanRHI::GetDepthStencilLayout(RTInfo.DepthStencilRenderTarget.GetDepthStencilAccess(), InDevice);
		CurrDesc.initialLayout = DepthStencilLayout;
		CurrDesc.finalLayout = DepthStencilLayout;
		DepthStencilReference.attachment = NumAttachmentDescriptions;
		DepthStencilReference.layout = DepthStencilLayout;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets] = CurrDesc.loadOp;
		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets] = CurrDesc.storeOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilStoreOp;
		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets] = CurrDesc.format;

		++NumAttachmentDescriptions;
/*
		if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
		{
			Desc[NumAttachments + 1] = Desc[NumAttachments];
			Desc[NumAttachments + 1].samples = VK_SAMPLE_COUNT_1_BIT;
			ResolveReferences[NumColorAttachments].attachment = NumAttachments + 1;
			ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			++NumAttachments;
			bHasResolveAttachments = true;
		}*/

		bHasDepthStencil = true;

		if (bSetExtent)
		{
			// Depth can be greater or equal to color
			ensure(Texture->Surface.Width >= Extent.Extent3D.width);
			ensure(Texture->Surface.Height >= Extent.Extent3D.height);
		}
		else
		{
			bSetExtent = true;
			Extent.Extent3D.width = Texture->Surface.Width;
			Extent.Extent3D.height = Texture->Surface.Height;
			Extent.Extent3D.depth = 1;
		}
	}

	CompatibleHashInfo.NumSamples = NumSamples;

	RenderPassCompatibleHash = FCrc::MemCrc32(&CompatibleHashInfo, sizeof(CompatibleHashInfo));
	RenderPassFullHash = FCrc::MemCrc32(&FullHashInfo, sizeof(FullHashInfo), RenderPassCompatibleHash);
	NumUsedClearValues = bFoundClearOp ? NumAttachmentDescriptions : 0;
	bCalculatedHash = true;
}


FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(FVulkanDevice& InDevice, const FRHIRenderPassInfo& RPInfo)
	: NumAttachmentDescriptions(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, NumSamples(0)
	, NumUsedClearValues(0)
{
	FMemory::Memzero(ColorReferences);
	FMemory::Memzero(DepthStencilReference);
	FMemory::Memzero(ResolveReferences);
	FMemory::Memzero(InputAttachments);
	FMemory::Memzero(Desc);
	FMemory::Memzero(Extent);

	FRenderPassCompatibleHashableStruct CompatibleHashInfo;
	FRenderPassFullHashableStruct FullHashInfo;

	bool bSetExtent = false;
	bool bFoundClearOp = false;
	int32 NumColorRenderTargets = RPInfo.GetNumColorRenderTargets();
	for (int32 Index = 0; Index < NumColorRenderTargets; ++Index)
	{
		const FRHIRenderPassInfo::FColorEntry& ColorEntry = RPInfo.ColorRenderTargets[Index];
		FVulkanTextureBase* Texture = FVulkanTextureBase::Cast(ColorEntry.RenderTarget);
		check(Texture);

		if (bSetExtent)
		{
			ensure(Extent.Extent3D.width == FMath::Max(1u, Texture->Surface.Width >> ColorEntry.MipIndex));
			ensure(Extent.Extent3D.height == FMath::Max(1u, Texture->Surface.Height >> ColorEntry.MipIndex));
			ensure(Extent.Extent3D.depth == Texture->Surface.Depth);
		}
		else
		{
			bSetExtent = true;
			Extent.Extent3D.width = FMath::Max(1u, Texture->Surface.Width >> ColorEntry.MipIndex);
			Extent.Extent3D.height = FMath::Max(1u, Texture->Surface.Height >> ColorEntry.MipIndex);
			Extent.Extent3D.depth = Texture->Surface.Depth;
		}

		ensure(!NumSamples || NumSamples == ColorEntry.RenderTarget->GetNumSamples());
		NumSamples = ColorEntry.RenderTarget->GetNumSamples();

		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
		CurrDesc.format = UEToVkFormat(ColorEntry.RenderTarget->GetFormat(), (Texture->Surface.UEFlags & TexCreate_SRGB) == TexCreate_SRGB);
		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(GetLoadAction(ColorEntry.Action));
		bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
		CurrDesc.storeOp = RenderTargetStoreActionToVulkan(GetStoreAction(ColorEntry.Action), true);
		CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		CurrDesc.initialLayout = /*RPInfo.bGeneratingMips ? VK_IMAGE_LAYOUT_GENERAL : */VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		CurrDesc.finalLayout = /*RPInfo.bGeneratingMips ? VK_IMAGE_LAYOUT_GENERAL : */VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		ColorReferences[NumColorAttachments].attachment = NumAttachmentDescriptions;
		ColorReferences[NumColorAttachments].layout = /*RPInfo.bGeneratingMips ? VK_IMAGE_LAYOUT_GENERAL : */VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
		{
			Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
			Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
			ResolveReferences[NumColorAttachments].attachment = NumAttachmentDescriptions + 1;
			ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_GENERAL;
			++NumAttachmentDescriptions;
			bHasResolveAttachments = true;
		}

		CompatibleHashInfo.Formats[NumColorAttachments] = CurrDesc.format;
		FullHashInfo.LoadOps[NumColorAttachments] = CurrDesc.loadOp;
		FullHashInfo.StoreOps[NumColorAttachments] = CurrDesc.storeOp;
		++CompatibleHashInfo.NumAttachments;

		++NumAttachmentDescriptions;
		++NumColorAttachments;
	}

	VkImageLayout DepthStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (RPInfo.DepthStencilRenderTarget.DepthStencilTarget)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);
		FVulkanTextureBase* Texture = FVulkanTextureBase::Cast(RPInfo.DepthStencilRenderTarget.DepthStencilTarget);
		check(Texture);

		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RPInfo.DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples());
		ensure(!NumSamples || CurrDesc.samples == NumSamples);
		NumSamples = CurrDesc.samples;
		CurrDesc.format = UEToVkFormat(RPInfo.DepthStencilRenderTarget.DepthStencilTarget->GetFormat(), false);
		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(GetLoadAction(GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)));
		CurrDesc.stencilLoadOp = RenderTargetLoadActionToVulkan(GetLoadAction(GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)));
		bFoundClearOp = bFoundClearOp || (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);

		if (CurrDesc.samples != VK_SAMPLE_COUNT_1_BIT)
		{
			// Can't resolve MSAA depth/stencil
			ensure(GetStoreAction(GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)) != ERenderTargetStoreAction::EMultisampleResolve);
			ensure(GetStoreAction(GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)) != ERenderTargetStoreAction::EMultisampleResolve);
		}

		CurrDesc.storeOp = RenderTargetStoreActionToVulkan(GetStoreAction(GetDepthActions(RPInfo.DepthStencilRenderTarget.Action)), true);
		CurrDesc.stencilStoreOp = RenderTargetStoreActionToVulkan(GetStoreAction(GetStencilActions(RPInfo.DepthStencilRenderTarget.Action)), true);

		DepthStencilLayout = VulkanRHI::GetDepthStencilLayout(RPInfo.DepthStencilRenderTarget.ExclusiveDepthStencil, InDevice);
		CurrDesc.initialLayout = DepthStencilLayout;
		CurrDesc.finalLayout = DepthStencilLayout;
		DepthStencilReference.attachment = NumAttachmentDescriptions;
		DepthStencilReference.layout = DepthStencilLayout;

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets] = CurrDesc.loadOp;
		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets] = CurrDesc.storeOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilStoreOp;
		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets] = CurrDesc.format;

		++NumAttachmentDescriptions;
		/*
		if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
		{
		Desc[NumAttachments + 1] = Desc[NumAttachments];
		Desc[NumAttachments + 1].samples = VK_SAMPLE_COUNT_1_BIT;
		ResolveReferences[NumColorAttachments].attachment = NumAttachments + 1;
		ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		++NumAttachments;
		bHasResolveAttachments = true;
		}*/

		bHasDepthStencil = true;

		if (bSetExtent)
		{
			// Depth can be greater or equal to color
			ensure(Texture->Surface.Width >= Extent.Extent3D.width);
			ensure(Texture->Surface.Height >= Extent.Extent3D.height);
		}
		else
		{
			bSetExtent = true;
			Extent.Extent3D.width = Texture->Surface.Width;
			Extent.Extent3D.height = Texture->Surface.Height;
			Extent.Extent3D.depth = 1;
		}
	}

	CompatibleHashInfo.NumSamples = NumSamples;

	RenderPassCompatibleHash = FCrc::MemCrc32(&CompatibleHashInfo, sizeof(CompatibleHashInfo));
	RenderPassFullHash = FCrc::MemCrc32(&FullHashInfo, sizeof(FullHashInfo), RenderPassCompatibleHash);
	NumUsedClearValues = bFoundClearOp ? NumAttachmentDescriptions : 0;
	bCalculatedHash = true;
}

FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(const FGraphicsPipelineStateInitializer& Initializer, const TArray<FInputAttachmentData>& InputAttachmentData)
	: NumAttachmentDescriptions(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, NumSamples(0)
	, NumUsedClearValues(0)
{
	FMemory::Memzero(ColorReferences);
	FMemory::Memzero(DepthStencilReference);
	FMemory::Memzero(ResolveReferences);
	FMemory::Memzero(InputAttachments);
	FMemory::Memzero(Desc);
	FMemory::Memzero(Extent);

	FRenderPassCompatibleHashableStruct CompatibleHashInfo;
	FRenderPassFullHashableStruct FullHashInfo;

	bool bSetExtent = false;
	bool bFoundClearOp = false;
	NumSamples = Initializer.NumSamples;
	for (uint32 Index = 0; Index < Initializer.RenderTargetsEnabled; ++Index)
	{
		EPixelFormat UEFormat = Initializer.RenderTargetFormats[Index];
		if (UEFormat != PF_Unknown)
		{
			VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
			CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
			CurrDesc.format = UEToVkFormat(UEFormat, (Initializer.RenderTargetFlags[Index] & TexCreate_SRGB) == TexCreate_SRGB);
			CurrDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			CurrDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			ColorReferences[NumColorAttachments].attachment = NumAttachmentDescriptions;
			ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
			{
				Desc[NumAttachmentDescriptions + 1] = Desc[NumAttachmentDescriptions];
				Desc[NumAttachmentDescriptions + 1].samples = VK_SAMPLE_COUNT_1_BIT;
				ResolveReferences[NumColorAttachments].attachment = NumAttachmentDescriptions + 1;
				ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_GENERAL;
				++NumAttachmentDescriptions;
				bHasResolveAttachments = true;
			}

			CompatibleHashInfo.Formats[NumColorAttachments] = CurrDesc.format;
			FullHashInfo.LoadOps[NumColorAttachments] = CurrDesc.loadOp;
			FullHashInfo.StoreOps[NumColorAttachments] = CurrDesc.storeOp;
			++CompatibleHashInfo.NumAttachments;

			++NumAttachmentDescriptions;
			++NumColorAttachments;
		}
	}

	if (Initializer.DepthStencilTargetFormat != PF_Unknown)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachmentDescriptions];
		FMemory::Memzero(CurrDesc);

		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
		CurrDesc.format = UEToVkFormat(Initializer.DepthStencilTargetFormat, false);
		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(Initializer.DepthTargetLoadAction);
		CurrDesc.stencilLoadOp = RenderTargetLoadActionToVulkan(Initializer.StencilTargetLoadAction);
		if (CurrDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR || CurrDesc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
		{
			bFoundClearOp = true;
		}
		if (CurrDesc.samples == VK_SAMPLE_COUNT_1_BIT)
		{
			CurrDesc.storeOp = RenderTargetStoreActionToVulkan(Initializer.StencilTargetStoreAction);
			CurrDesc.stencilStoreOp = RenderTargetStoreActionToVulkan(Initializer.StencilTargetStoreAction);
		}
		else
		{
			// Never want to store MSAA depth/stencil
			CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
		CurrDesc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		CurrDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		DepthStencilReference.attachment = NumAttachmentDescriptions;
		DepthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		/*
		if (CurrDesc.samples > VK_SAMPLE_COUNT_1_BIT)
		{
		Desc[NumAttachments + 1] = Desc[NumAttachments];
		Desc[NumAttachments + 1].samples = VK_SAMPLE_COUNT_1_BIT;
		ResolveReferences[NumColorAttachments].attachment = NumAttachments + 1;
		ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		++NumAttachments;
		bHasResolveAttachments = true;
		}*/

		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets] = CurrDesc.loadOp;
		FullHashInfo.LoadOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilLoadOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets] = CurrDesc.storeOp;
		FullHashInfo.StoreOps[MaxSimultaneousRenderTargets + 1] = CurrDesc.stencilStoreOp;
		CompatibleHashInfo.Formats[MaxSimultaneousRenderTargets] = CurrDesc.format;

		++NumAttachmentDescriptions;
		bHasDepthStencil = true;
	}

	CompatibleHashInfo.NumSamples = NumSamples;

	RenderPassCompatibleHash = FCrc::MemCrc32(&CompatibleHashInfo, sizeof(CompatibleHashInfo));
	RenderPassFullHash = FCrc::MemCrc32(&FullHashInfo, sizeof(FullHashInfo), RenderPassCompatibleHash);
	NumUsedClearValues = bFoundClearOp ? NumAttachmentDescriptions : 0;
	bCalculatedHash = true;
}

uint16 FVulkanRenderTargetLayout::SetupSubpasses(VkSubpassDescription* OutDescs, uint32 MaxDescs, VkSubpassDependency* OutDeps, uint32 MaxDeps, uint32& OutNumDependencies) const
{
	check(MaxDescs > 0);
	FMemory::Memzero(OutDescs, sizeof(OutDescs[0]) * MaxDescs);
	OutDescs[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	//OutDesc.flags = 0;
	//OutDesc.inputAttachmentCount = 0;
	OutDescs[0].colorAttachmentCount = GetNumColorAttachments();
	OutDescs[0].pColorAttachments = GetColorAttachmentReferences();
	OutDescs[0].pResolveAttachments = GetResolveAttachmentReferences();
	OutDescs[0].pDepthStencilAttachment = GetDepthStencilAttachmentReference();
	//OutDesc.preserveAttachmentCount = 0;

	OutNumDependencies = 0;
	return 1;
}
