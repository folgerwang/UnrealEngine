// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#include "MagicLeapHelperVulkan.h"
#include "IMagicLeapHelperVulkanPlugin.h"
#include "Engine/Engine.h"

#if !PLATFORM_MAC
#include "VulkanRHIPrivate.h"
#include "ScreenRendering.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "VulkanUtil.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMagicLeapHelperVulkan, Display, All);

class FMagicLeapHelperVulkanPlugin : public IMagicLeapHelperVulkanPlugin
{};

IMPLEMENT_MODULE(FMagicLeapHelperVulkanPlugin, MagicLeapHelperVulkan);

//////////////////////////////////////////////////////////////////////////

void FMagicLeapHelperVulkan::BlitImage(uint64 SrcName, int32 SrcLevel, int32 SrcX, int32 SrcY, int32 SrcZ, int32 SrcWidth, int32 SrcHeight, int32 SrcDepth, uint64 DstName, int32 DstLevel, int32 DstX, int32 DstY, int32 DstZ, int32 DstWidth, int32 DstHeight, int32 DstDepth)
{
#if !PLATFORM_MAC
	VkImage Src = (VkImage)SrcName;
	VkImage Dst = (VkImage)DstName;

	FVulkanDynamicRHI* RHI = (FVulkanDynamicRHI*)GDynamicRHI;

	FVulkanCommandBufferManager* CmdBufferMgr = RHI->GetDevice()->GetImmediateContext().GetCommandBufferManager();
	FVulkanCmdBuffer* CmdBuffer = CmdBufferMgr->GetUploadCmdBuffer();

	/*{
		VkClearColorValue Color;
		Color.float32[0] = 0;
		Color.float32[1] = 0;
		Color.float32[2] = 1;
		Color.float32[3] = 1;
		VkImageSubresourceRange Range;
		Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Range.baseMipLevel = 0;
		Range.levelCount = 1;
		Range.baseArrayLayer = 0;
		Range.layerCount = 1;
		VulkanRHI::vkCmdClearColorImage(CmdBuffer->GetHandle(), Src, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, &Color, 1, &Range);
	}*/

	VkImageBlit Region;
	FMemory::Memzero(Region);
	Region.srcOffsets[0].x = SrcX;
	Region.srcOffsets[0].y = SrcY;
	Region.srcOffsets[0].z = SrcZ;
	Region.srcOffsets[1].x = SrcX + SrcWidth;
	Region.srcOffsets[1].y = SrcY + SrcHeight;
	Region.srcOffsets[1].z = SrcZ + SrcDepth;
	Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.srcSubresource.layerCount = 1;
	Region.dstOffsets[0].x = DstX;
	Region.dstOffsets[0].y = DstY;
	Region.dstOffsets[0].z = DstZ;
	Region.dstOffsets[1].x = DstX + DstWidth;
	Region.dstOffsets[1].y = DstY + DstHeight;
	Region.dstOffsets[1].z = DstZ + DstDepth;
	Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Region.dstSubresource.baseArrayLayer = DstLevel;
	Region.dstSubresource.layerCount = 1;
	VulkanRHI::vkCmdBlitImage(CmdBuffer->GetHandle(), Src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Dst, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 1, &Region, VK_FILTER_LINEAR);
#endif
}


void FMagicLeapHelperVulkan::TestClear(uint64 DstName)
{
#if !PLATFORM_MAC
	VkImage Dst = (VkImage)DstName;

	FVulkanDynamicRHI* RHI = (FVulkanDynamicRHI*)GDynamicRHI;

	FVulkanCommandBufferManager* CmdBufferMgr = RHI->GetDevice()->GetImmediateContext().GetCommandBufferManager();
	FVulkanCmdBuffer* CmdBuffer = CmdBufferMgr->GetUploadCmdBuffer();

	VkClearColorValue Color;
	Color.float32[0] = 0;
	Color.float32[1] = 0;
	Color.float32[2] = 1;
	Color.float32[3] = 1;
	VkImageSubresourceRange Range;
	Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Range.baseMipLevel = 0;
	Range.levelCount = 1;
	Range.baseArrayLayer = 0;
	Range.layerCount = 2;
	VulkanRHI::vkCmdClearColorImage(CmdBuffer->GetHandle(), Dst, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, &Color, 1, &Range);
#endif
}


void FMagicLeapHelperVulkan::SignalObjects(uint64 SignalObject0, uint64 SignalObject1)
{
#if !PLATFORM_MAC
	FVulkanDynamicRHI* RHI = (FVulkanDynamicRHI*)GDynamicRHI;

	FVulkanCommandBufferManager* CmdBufferMgr = RHI->GetDevice()->GetImmediateContext().GetCommandBufferManager();
	FVulkanCmdBuffer* CmdBuffer = CmdBufferMgr->GetUploadCmdBuffer();

	VkSemaphore Semaphores[2] =
	{
		(VkSemaphore)SignalObject0,
		(VkSemaphore)SignalObject1
	};

	CmdBufferMgr->SubmitUploadCmdBuffer(2, Semaphores);
#endif
}

uint64 FMagicLeapHelperVulkan::AliasImageSRGB(const uint64 Allocation, const uint32 Width, const uint32 Height)
{
#if !PLATFORM_MAC
	VkImageCreateInfo ImageCreateInfo;

	// This must match the RenderTargetTexture image other than format, which we are aliasing as srgb to match the output of the tonemapper.
	FMemory::Memzero(ImageCreateInfo);
	ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	ImageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	ImageCreateInfo.extent.width = Width;
	ImageCreateInfo.extent.height = Height;
	ImageCreateInfo.extent.depth = 1;
	ImageCreateInfo.mipLevels = 1;
	ImageCreateInfo.arrayLayers = 1;
	ImageCreateInfo.flags = 0;
	ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	ImageCreateInfo.usage = 0;
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	ImageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ImageCreateInfo.queueFamilyIndexCount = 0;
	ImageCreateInfo.pQueueFamilyIndices = nullptr;
	ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

	FVulkanDynamicRHI* const RHI = (FVulkanDynamicRHI*)GDynamicRHI;
	
	VkImage Result = VK_NULL_HANDLE;
	VERIFYVULKANRESULT(VulkanRHI::vkCreateImage(RHI->GetDevice()->GetInstanceHandle(), &ImageCreateInfo, nullptr, &Result));

	// Assuming the offset is 0, we're not currently querying the surface for this.
	const VkDeviceSize Offset = 0;

	VERIFYVULKANRESULT(VulkanRHI::vkBindImageMemory(RHI->GetDevice()->GetInstanceHandle(), Result, (VkDeviceMemory)Allocation, Offset));

	return (uint64)Result;
#else
	return 0;
#endif
}
