// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "VulkanRHIPrivate.h"
#include "VulkanGenericPlatform.h"

void FVulkanGenericPlatform::SetupFeatureLevels()
{
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_VULKAN_PCES3_1;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_VULKAN_PCES3_1;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = SP_VULKAN_SM4;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_VULKAN_SM5;
}
