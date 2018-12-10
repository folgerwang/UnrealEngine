// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanState.cpp: Vulkan state implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"

static FCriticalSection GSamplerHashLock;

inline VkSamplerMipmapMode TranslateMipFilterMode(ESamplerFilter Filter)
{
	VkSamplerMipmapMode OutFilter = VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;

	switch (Filter)
	{
		case SF_Point:				OutFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;	break;
		case SF_Bilinear:			OutFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;	break;
		case SF_Trilinear:			OutFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR;	break;
		case SF_AnisotropicPoint:	OutFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR;	break;
		default:																break;
	}

	// Check for missing translation
	check(OutFilter != VK_SAMPLER_MIPMAP_MODE_MAX_ENUM);
	return OutFilter;
}

inline VkFilter TranslateMagFilterMode(ESamplerFilter InFilter)
{
	VkFilter OutFilter = VK_FILTER_MAX_ENUM;

	switch (InFilter)
	{
		case SF_Point:				OutFilter = VK_FILTER_NEAREST;	break;
		case SF_Bilinear:			OutFilter = VK_FILTER_LINEAR;	break;
		case SF_Trilinear:			OutFilter = VK_FILTER_LINEAR;	break;
		case SF_AnisotropicPoint:	OutFilter = VK_FILTER_LINEAR;	break;
		default:													break;
	}

	// Check for missing translation
	check(OutFilter != VK_FILTER_MAX_ENUM);
	return OutFilter;
}

inline VkFilter TranslateMinFilterMode(ESamplerFilter InFilter)
{
	VkFilter OutFilter = VK_FILTER_MAX_ENUM;

	switch (InFilter)
	{
		case SF_Point:				OutFilter = VK_FILTER_NEAREST;	break;
		case SF_Bilinear:			OutFilter = VK_FILTER_LINEAR;	break;
		case SF_Trilinear:			OutFilter = VK_FILTER_LINEAR;	break;
		case SF_AnisotropicPoint:	OutFilter = VK_FILTER_LINEAR;	break;
		default:													break;
	}

	// Check for missing translation
	check(OutFilter != VK_FILTER_MAX_ENUM);
	return OutFilter;
}

inline VkSamplerAddressMode TranslateWrapMode(ESamplerAddressMode InAddressMode)
{
	VkSamplerAddressMode OutAddressMode = VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;

	switch (InAddressMode)
	{
		case AM_Wrap:		OutAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;			break;
		case AM_Clamp:		OutAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;		break;
		case AM_Mirror:		OutAddressMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;	break;
		case AM_Border:		OutAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;	break;
		default:
			break;
	}

	// Check for missing translation
	check(OutAddressMode != VK_SAMPLER_ADDRESS_MODE_MAX_ENUM);
	return OutAddressMode;
}

inline VkCompareOp TranslateSamplerCompareFunction(ESamplerCompareFunction InSamplerComparisonFunction)
{
	VkCompareOp OutSamplerComparisonFunction = VK_COMPARE_OP_MAX_ENUM;

	switch (InSamplerComparisonFunction)
	{
		case SCF_Less:	OutSamplerComparisonFunction = VK_COMPARE_OP_LESS;	break;
		case SCF_Never:	OutSamplerComparisonFunction = VK_COMPARE_OP_NEVER;	break;
		default:															break;
	};

	// Check for missing translation
	check(OutSamplerComparisonFunction != VK_COMPARE_OP_MAX_ENUM);
	return OutSamplerComparisonFunction;
}

static inline VkBlendOp BlendOpToVulkan(EBlendOperation InOp)
{
	VkBlendOp OutOp = VK_BLEND_OP_MAX_ENUM;

	switch (InOp)
	{
		case BO_Add:				OutOp = VK_BLEND_OP_ADD;				break;
		case BO_Subtract:			OutOp = VK_BLEND_OP_SUBTRACT;			break;
		case BO_Min:				OutOp = VK_BLEND_OP_MIN;				break;
		case BO_Max:				OutOp = VK_BLEND_OP_MAX;				break;
		case BO_ReverseSubtract:	OutOp = VK_BLEND_OP_REVERSE_SUBTRACT;	break;
		default:															break;
	}

	// Check for missing translation
	check(OutOp != VK_BLEND_OP_MAX_ENUM);
	return OutOp;
}

static inline VkBlendFactor BlendFactorToVulkan(EBlendFactor InFactor)
{
	VkBlendFactor BlendMode = VK_BLEND_FACTOR_MAX_ENUM;

	switch (InFactor)
	{
		case BF_Zero:						BlendMode = VK_BLEND_FACTOR_ZERO;						break;
		case BF_One:						BlendMode = VK_BLEND_FACTOR_ONE;						break;
		case BF_SourceColor:				BlendMode = VK_BLEND_FACTOR_SRC_COLOR;					break;
		case BF_InverseSourceColor:			BlendMode = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;		break;
		case BF_SourceAlpha:				BlendMode = VK_BLEND_FACTOR_SRC_ALPHA;					break;
		case BF_InverseSourceAlpha:			BlendMode = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;		break;
		case BF_DestAlpha:					BlendMode = VK_BLEND_FACTOR_DST_ALPHA;					break;
		case BF_InverseDestAlpha:			BlendMode = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;		break;
		case BF_DestColor:					BlendMode = VK_BLEND_FACTOR_DST_COLOR;					break;
		case BF_InverseDestColor:			BlendMode = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;		break;
		case BF_ConstantBlendFactor:		BlendMode = VK_BLEND_FACTOR_CONSTANT_COLOR;				break;
		case BF_InverseConstantBlendFactor:	BlendMode = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;	break;
		default:																					break;
	}

	// Check for missing translation
	check(BlendMode != VK_BLEND_FACTOR_MAX_ENUM);
	return BlendMode;
}

static inline VkCompareOp CompareOpToVulkan(ECompareFunction InOp)
{
	VkCompareOp OutOp = VK_COMPARE_OP_MAX_ENUM;

	switch (InOp)
	{
		case CF_Less:			OutOp = VK_COMPARE_OP_LESS;				break;
		case CF_LessEqual:		OutOp = VK_COMPARE_OP_LESS_OR_EQUAL;	break;
		case CF_Greater:		OutOp = VK_COMPARE_OP_GREATER;			break;
		case CF_GreaterEqual:	OutOp = VK_COMPARE_OP_GREATER_OR_EQUAL;	break;
		case CF_Equal:			OutOp = VK_COMPARE_OP_EQUAL;			break;
		case CF_NotEqual:		OutOp = VK_COMPARE_OP_NOT_EQUAL;		break;
		case CF_Never:			OutOp = VK_COMPARE_OP_NEVER;			break;
		case CF_Always:			OutOp = VK_COMPARE_OP_ALWAYS;			break;
		default:														break;
	}

	// Check for missing translation
	check(OutOp != VK_COMPARE_OP_MAX_ENUM);
	return OutOp;
}

static inline VkStencilOp StencilOpToVulkan(EStencilOp InOp)
{
	VkStencilOp OutOp = VK_STENCIL_OP_MAX_ENUM;

	switch (InOp)
	{
		case SO_Keep:					OutOp = VK_STENCIL_OP_KEEP;					break;
		case SO_Zero:					OutOp = VK_STENCIL_OP_ZERO;					break;
		case SO_Replace:				OutOp = VK_STENCIL_OP_REPLACE;				break;
		case SO_SaturatedIncrement:		OutOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;	break;
		case SO_SaturatedDecrement:		OutOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP;	break;
		case SO_Invert:					OutOp = VK_STENCIL_OP_INVERT;				break;
		case SO_Increment:				OutOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;	break;
		case SO_Decrement:				OutOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;	break;
		default:																	break;
	}

	check(OutOp != VK_STENCIL_OP_MAX_ENUM);
	return OutOp;
}

static inline VkPolygonMode RasterizerFillModeToVulkan(ERasterizerFillMode InFillMode)
{
	VkPolygonMode OutFillMode = VK_POLYGON_MODE_MAX_ENUM;

	switch (InFillMode)
	{
		case FM_Point:			OutFillMode = VK_POLYGON_MODE_POINT;	break;
		case FM_Wireframe:		OutFillMode = VK_POLYGON_MODE_LINE;		break;
		case FM_Solid:			OutFillMode = VK_POLYGON_MODE_FILL;		break;
		default:														break;
	}

	// Check for missing translation
	check(OutFillMode != VK_POLYGON_MODE_MAX_ENUM);
	return OutFillMode;
}

static inline VkCullModeFlags RasterizerCullModeToVulkan(ERasterizerCullMode InCullMode)
{
	VkCullModeFlags outCullMode = VK_CULL_MODE_NONE;

	switch (InCullMode)
	{
		case CM_None:	outCullMode = VK_CULL_MODE_NONE;		break;
		case CM_CW:		outCullMode = VK_CULL_MODE_FRONT_BIT;	break;
		case CM_CCW:	outCullMode = VK_CULL_MODE_BACK_BIT;	break;
			// Check for missing translation
		default:		check(false);							break;
	}

	return outCullMode;
}


void FVulkanSamplerState::SetupSamplerCreateInfo(const FSamplerStateInitializerRHI& Initializer, FVulkanDevice& InDevice, VkSamplerCreateInfo& OutSamplerInfo)
{
	ZeroVulkanStruct(OutSamplerInfo, VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);

	OutSamplerInfo.magFilter = TranslateMagFilterMode(Initializer.Filter);
	OutSamplerInfo.minFilter = TranslateMinFilterMode(Initializer.Filter);
	OutSamplerInfo.mipmapMode = TranslateMipFilterMode(Initializer.Filter);
	OutSamplerInfo.addressModeU = TranslateWrapMode(Initializer.AddressU);
	OutSamplerInfo.addressModeV = TranslateWrapMode(Initializer.AddressV);
	OutSamplerInfo.addressModeW = TranslateWrapMode(Initializer.AddressW);

	OutSamplerInfo.mipLodBias = Initializer.MipBias;
	OutSamplerInfo.maxAnisotropy = FMath::Clamp((float)ComputeAnisotropyRT(Initializer.MaxAnisotropy), 1.0f, InDevice.GetLimits().maxSamplerAnisotropy);
	OutSamplerInfo.anisotropyEnable = OutSamplerInfo.maxAnisotropy > 1;

	OutSamplerInfo.compareEnable = Initializer.SamplerComparisonFunction != SCF_Never ? VK_TRUE : VK_FALSE;
	OutSamplerInfo.compareOp = TranslateSamplerCompareFunction(Initializer.SamplerComparisonFunction);
	OutSamplerInfo.minLod = Initializer.MinMipLevel;
	OutSamplerInfo.maxLod = Initializer.MaxMipLevel;
	OutSamplerInfo.borderColor = Initializer.BorderColor == 0 ? VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK : VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
}


FVulkanSamplerState::FVulkanSamplerState(const VkSamplerCreateInfo& InInfo, FVulkanDevice& InDevice, const bool bInIsImmutable)
	: Sampler(VK_NULL_HANDLE)
	, bIsImmutable(bInIsImmutable)
{
	VERIFYVULKANRESULT(VulkanRHI::vkCreateSampler(InDevice.GetInstanceHandle(), &InInfo, VULKAN_CPU_ALLOCATOR, &Sampler));
}

FVulkanRasterizerState::FVulkanRasterizerState(const FRasterizerStateInitializerRHI& InInitializer)
{
	Initializer = InInitializer;
	FVulkanRasterizerState::ResetCreateInfo(RasterizerState);

	// @todo vulkan: I'm assuming that Solid and Wireframe wouldn't ever be mixed within the same BoundShaderState, so we are ignoring the fill mode as a unique identifier
	//checkf(Initializer.FillMode == FM_Solid, TEXT("PIPELINE KEY: Only FM_Solid is supported fill mode [got %d]"), (int32)Initializer.FillMode);

	RasterizerState.polygonMode = RasterizerFillModeToVulkan(Initializer.FillMode);
	RasterizerState.cullMode = RasterizerCullModeToVulkan(Initializer.CullMode);

	//RasterizerState.depthClampEnable = VK_FALSE;
	RasterizerState.depthBiasEnable = Initializer.DepthBias != 0.0f ? VK_TRUE : VK_FALSE;
	//RasterizerState.rasterizerDiscardEnable = VK_FALSE;

	RasterizerState.depthBiasSlopeFactor = Initializer.SlopeScaleDepthBias;
	RasterizerState.depthBiasConstantFactor = Initializer.DepthBias;

	//RasterizerState.lineWidth = 1.0f;
}

void FVulkanDepthStencilState::SetupCreateInfo(const FGraphicsPipelineStateInitializer& GfxPSOInit, VkPipelineDepthStencilStateCreateInfo& OutDepthStencilState)
{
	ZeroVulkanStruct(OutDepthStencilState, VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);

	OutDepthStencilState.depthTestEnable = (Initializer.DepthTest != CF_Always || Initializer.bEnableDepthWrite) ? VK_TRUE : VK_FALSE;
	OutDepthStencilState.depthCompareOp = CompareOpToVulkan(Initializer.DepthTest);
	OutDepthStencilState.depthWriteEnable = Initializer.bEnableDepthWrite ? VK_TRUE : VK_FALSE;

	{
		// This will be filled in from the PSO
		OutDepthStencilState.depthBoundsTestEnable = GfxPSOInit.bDepthBounds;
		OutDepthStencilState.minDepthBounds = 0.0f;
		OutDepthStencilState.maxDepthBounds = 1.0f;
	}

	OutDepthStencilState.stencilTestEnable = (Initializer.bEnableFrontFaceStencil || Initializer.bEnableBackFaceStencil) ? VK_TRUE : VK_FALSE;

	// Front
	OutDepthStencilState.back.failOp = StencilOpToVulkan(Initializer.FrontFaceStencilFailStencilOp);
	OutDepthStencilState.back.passOp = StencilOpToVulkan(Initializer.FrontFacePassStencilOp);
	OutDepthStencilState.back.depthFailOp = StencilOpToVulkan(Initializer.FrontFaceDepthFailStencilOp);
	OutDepthStencilState.back.compareOp = CompareOpToVulkan(Initializer.FrontFaceStencilTest);
	OutDepthStencilState.back.compareMask = Initializer.StencilReadMask;
	OutDepthStencilState.back.writeMask = Initializer.StencilWriteMask;
	OutDepthStencilState.back.reference = 0;

	if (Initializer.bEnableBackFaceStencil)
	{
		// Back
		OutDepthStencilState.front.failOp = StencilOpToVulkan(Initializer.BackFaceStencilFailStencilOp);
		OutDepthStencilState.front.passOp = StencilOpToVulkan(Initializer.BackFacePassStencilOp);
		OutDepthStencilState.front.depthFailOp = StencilOpToVulkan(Initializer.BackFaceDepthFailStencilOp);
		OutDepthStencilState.front.compareOp = CompareOpToVulkan(Initializer.BackFaceStencilTest);
		OutDepthStencilState.front.compareMask = Initializer.StencilReadMask;
		OutDepthStencilState.front.writeMask = Initializer.StencilWriteMask;
		OutDepthStencilState.front.reference = 0;
	}
	else
	{
		OutDepthStencilState.front = OutDepthStencilState.back;
	}
}

FVulkanBlendState::FVulkanBlendState(const FBlendStateInitializerRHI& InInitializer)
{
	Initializer = InInitializer;
	for (uint32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		const FBlendStateInitializerRHI::FRenderTarget& ColorTarget = Initializer.RenderTargets[Index];
		VkPipelineColorBlendAttachmentState& BlendState = BlendStates[Index];
		FMemory::Memzero(BlendState);

		BlendState.colorBlendOp = BlendOpToVulkan(ColorTarget.ColorBlendOp);
		BlendState.alphaBlendOp = BlendOpToVulkan(ColorTarget.AlphaBlendOp);

		BlendState.dstColorBlendFactor = BlendFactorToVulkan(ColorTarget.ColorDestBlend);
		BlendState.dstAlphaBlendFactor = BlendFactorToVulkan(ColorTarget.AlphaDestBlend);

		BlendState.srcColorBlendFactor = BlendFactorToVulkan(ColorTarget.ColorSrcBlend);
		BlendState.srcAlphaBlendFactor = BlendFactorToVulkan(ColorTarget.AlphaSrcBlend);

		BlendState.blendEnable =
			(ColorTarget.ColorBlendOp != BO_Add || ColorTarget.ColorDestBlend != BF_Zero || ColorTarget.ColorSrcBlend != BF_One ||
			ColorTarget.AlphaBlendOp != BO_Add || ColorTarget.AlphaDestBlend != BF_Zero || ColorTarget.AlphaSrcBlend != BF_One) ? VK_TRUE : VK_FALSE;

		BlendState.colorWriteMask = (ColorTarget.ColorWriteMask & CW_RED) ? VK_COLOR_COMPONENT_R_BIT : 0;
		BlendState.colorWriteMask |= (ColorTarget.ColorWriteMask & CW_GREEN) ? VK_COLOR_COMPONENT_G_BIT : 0;
		BlendState.colorWriteMask |= (ColorTarget.ColorWriteMask & CW_BLUE) ? VK_COLOR_COMPONENT_B_BIT : 0;
		BlendState.colorWriteMask |= (ColorTarget.ColorWriteMask & CW_ALPHA) ? VK_COLOR_COMPONENT_A_BIT : 0;
	}
}

FSamplerStateRHIRef FVulkanDynamicRHI::RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	VkSamplerCreateInfo SamplerInfo;
	FVulkanSamplerState::SetupSamplerCreateInfo(Initializer, *Device, SamplerInfo);

	uint32 CRC = FCrc::MemCrc32(&SamplerInfo, sizeof(SamplerInfo));

	{
		FScopeLock ScopeLock(&GSamplerHashLock);
		TMap<uint32, FSamplerStateRHIRef>& SamplerMap = Device->GetSamplerMap();
		FSamplerStateRHIRef* Found = SamplerMap.Find(CRC);
		if (Found)
		{
			return *Found;
		}

		FSamplerStateRHIRef New = new FVulkanSamplerState(SamplerInfo, *Device);
		SamplerMap.Add(CRC, New);
		return New;
	}
}

#if VULKAN_SUPPORTS_COLOR_CONVERSIONS
FSamplerStateRHIRef FVulkanDynamicRHI::RHICreateSamplerState(
	const FSamplerStateInitializerRHI& Initializer, 
	const FSamplerYcbcrConversionInitializer& ConversionInitializer)
{
	VkSamplerYcbcrConversionCreateInfo ConversionCreateInfo;
	FMemory::Memzero(&ConversionCreateInfo, sizeof(VkSamplerYcbcrConversionCreateInfo));
	ConversionCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
	ConversionCreateInfo.format = ConversionInitializer.Format;
	
	ConversionCreateInfo.components.a = ConversionInitializer.Components.a;
	ConversionCreateInfo.components.r = ConversionInitializer.Components.r;
	ConversionCreateInfo.components.g = ConversionInitializer.Components.g;
	ConversionCreateInfo.components.b = ConversionInitializer.Components.b;
	
	ConversionCreateInfo.ycbcrModel = ConversionInitializer.Model;
	ConversionCreateInfo.ycbcrRange = ConversionInitializer.Range;
	ConversionCreateInfo.xChromaOffset = ConversionInitializer.XOffset;
	ConversionCreateInfo.yChromaOffset = ConversionInitializer.YOffset;
	ConversionCreateInfo.chromaFilter = VK_FILTER_NEAREST;
	ConversionCreateInfo.forceExplicitReconstruction = VK_FALSE;

	check(ConversionInitializer.Format != VK_FORMAT_UNDEFINED); // No support for VkExternalFormatANDROID yet.

	VkSamplerYcbcrConversionInfo ConversionInfo;
	FMemory::Memzero(&ConversionInfo, sizeof(VkSamplerYcbcrConversionInfo));
	ConversionInfo.conversion = Device->CreateSamplerColorConversion(ConversionCreateInfo);
	ConversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO;

	VkSamplerCreateInfo SamplerInfo;
	FVulkanSamplerState::SetupSamplerCreateInfo(Initializer, *Device, SamplerInfo);
	SamplerInfo.pNext = &ConversionInfo;

	return new FVulkanSamplerState(SamplerInfo, *Device, true);
}
#endif

FRasterizerStateRHIRef FVulkanDynamicRHI::RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	return new FVulkanRasterizerState(Initializer);
}


FDepthStencilStateRHIRef FVulkanDynamicRHI::RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	return new FVulkanDepthStencilState(Initializer);
}


FBlendStateRHIRef FVulkanDynamicRHI::RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	return new FVulkanBlendState(Initializer);
}
