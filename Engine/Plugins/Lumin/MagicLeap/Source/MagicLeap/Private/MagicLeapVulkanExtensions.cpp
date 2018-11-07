// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 201x Magic Leap, Inc. (COMPANY) All Rights Reserved.
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

#include "MagicLeapVulkanExtensions.h"

#if !PLATFORM_LUMIN

#include "MagicLeapGraphics.h"

#if !PLATFORM_MAC
#include <vulkan.h>
#endif // !PLATFORM_MAC
#if WITH_MLSDK
#include <ml_remote.h>
#endif // WITH_MLSDK
#include "AppFramework.h"

#endif // !PLATFORM_LUMIN

#include "MagicLeapHelperVulkan.h"

struct FMagicLeapVulkanExtensions::Implementation
{
#if PLATFORM_WINDOWS
	TArray<VkExtensionProperties> InstanceExtensions;
	TArray<VkExtensionProperties> DeviceExtensions;
#endif
};

FMagicLeapVulkanExtensions::FMagicLeapVulkanExtensions() {}
FMagicLeapVulkanExtensions::~FMagicLeapVulkanExtensions() {}

bool FMagicLeapVulkanExtensions::GetVulkanInstanceExtensionsRequired(TArray<const ANSICHAR*>& Out)
{
	if (!ImpPtr.IsValid())
	{
		ImpPtr.Reset(new Implementation);
	}
#if (PLATFORM_WINDOWS && WITH_MLSDK)
	// Interrogate the extensions we need for MLRemote.
	TArray<VkExtensionProperties> Extensions;
	{
		MLResult Result = MLResult_Ok;
		uint32_t Count = 0;
		Result = MLRemoteEnumerateRequiredVkInstanceExtensions(nullptr, &Count);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLRemoteEnumerateRequiredVkInstanceExtensions failed with status %d"), Result);
			return false;
		}
		ImpPtr->InstanceExtensions.Empty();
		if (Count > 0)
		{
			ImpPtr->InstanceExtensions.AddDefaulted(Count);
			Result = MLRemoteEnumerateRequiredVkInstanceExtensions(ImpPtr->InstanceExtensions.GetData(), &Count);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLRemoteEnumerateRequiredVkInstanceExtensions failed with status %d"), Result);
				return false;
			}
		}
	}
	for (auto & Extension : ImpPtr->InstanceExtensions)
	{
		Out.Add(Extension.extensionName);
	}
#endif //(PLATFORM_WINDOWS && WITH_MLSDK)
	return true;
}

bool FMagicLeapVulkanExtensions::GetVulkanDeviceExtensionsRequired(struct VkPhysicalDevice_T *pPhysicalDevice, TArray<const ANSICHAR*>& Out)
{
	if (!ImpPtr.IsValid())
	{
		ImpPtr.Reset(new Implementation);
	}
#if PLATFORM_LUMIN
	return FMagicLeapHelperVulkan::GetVulkanDeviceExtensionsRequired(pPhysicalDevice, Out);
#else
#if (PLATFORM_WINDOWS && WITH_MLSDK)
	// Interrogate the extensions we need for MLRemote.
	TArray<VkExtensionProperties> Extensions;
	{
		MLResult Result = MLResult_Ok;
		uint32_t Count = 0;
		Result = MLRemoteEnumerateRequiredVkDeviceExtensions(nullptr, &Count);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLRemoteEnumerateRequiredVkDeviceExtensions failed with status %d"), Result);
			return false;
		}
		ImpPtr->DeviceExtensions.Empty();
		if (Count > 0)
		{
			ImpPtr->DeviceExtensions.AddDefaulted(Count);
			Result = MLRemoteEnumerateRequiredVkDeviceExtensions(ImpPtr->DeviceExtensions.GetData(), &Count);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLRemoteEnumerateRequiredVkDeviceExtensions failed with status %d"), Result);
				return false;
			}
		}
}
	for (auto & Extension : ImpPtr->DeviceExtensions)
	{
		Out.Add(Extension.extensionName);
	}
#endif // (PLATFORM_WINDOWS && WITH_MLSDK)
	return true;
#endif // PLATFORM_LUMIN
}
