// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanState.h: Vulkan state definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"

class FVulkanDevice;

class FVulkanSamplerState : public FRHISamplerState
{
public:
	FVulkanSamplerState(const VkSamplerCreateInfo& InInfo, FVulkanDevice& InDevice);

	VkSampler Sampler;

	static void SetupSamplerCreateInfo(const FSamplerStateInitializerRHI& Initializer, FVulkanDevice& InDevice, VkSamplerCreateInfo& OutSamplerInfo);
};

class FVulkanRasterizerState : public FRHIRasterizerState
{
public:
	FVulkanRasterizerState(const FRasterizerStateInitializerRHI& Initializer);

	static void ResetCreateInfo(VkPipelineRasterizationStateCreateInfo& OutInfo)
	{
		ZeroVulkanStruct(OutInfo, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
		OutInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		OutInfo.lineWidth = 1.0f;
	}

	VkPipelineRasterizationStateCreateInfo RasterizerState;
};

class FVulkanDepthStencilState : public FRHIDepthStencilState
{
public:
	FVulkanDepthStencilState(const FDepthStencilStateInitializerRHI& InInitializer)
	{
		Initializer = InInitializer;
	}

	void SetupCreateInfo(const FGraphicsPipelineStateInitializer& GfxPSOInit, VkPipelineDepthStencilStateCreateInfo& OutDepthStencilState);

	FDepthStencilStateInitializerRHI Initializer;
};

class FVulkanBlendState : public FRHIBlendState
{
public:
	FVulkanBlendState(const FBlendStateInitializerRHI& Initializer);

	// array the pipeline state can point right to
	VkPipelineColorBlendAttachmentState BlendStates[MaxSimultaneousRenderTargets];
};
