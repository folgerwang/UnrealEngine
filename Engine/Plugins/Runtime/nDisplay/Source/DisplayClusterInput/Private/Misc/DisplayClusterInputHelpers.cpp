// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Misc/DisplayClusterInputHelpers.h"
#include "XRMotionControllerBase.h"
#include "InputCoreTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"


bool DisplayClusterInputHelpers::KeyNameToVrpnScancode(const FString& KeyName, int32& VrpnScanCode)
{
	static TArray<FKey> AllDefinedKeys;
	if (AllDefinedKeys.Num() == 0)
	{
		EKeys::GetAllKeys(AllDefinedKeys);
	}

	if (AllDefinedKeys.Num())
	{
		// key = "key-name"
		int32 KeyScanCode = -1;
		for (int ScanCode = 0; ScanCode < 256; ++ScanCode)
		{
			const uint32 VirtualKeyCode = MapVirtualKey(ScanCode, MAPVK_VSC_TO_VK);
			const uint32 CharCode = MapVirtualKey(VirtualKeyCode, MAPVK_VK_TO_CHAR);
			FKey Key = FInputKeyManager::Get().GetKeyFromCodes(VirtualKeyCode, CharCode);

			if (Key != EKeys::Invalid)
			{
				bool bIsDisplayName = Key.GetDisplayName().ToString().Compare(KeyName, ESearchCase::IgnoreCase) == 0;
				bool bIsInternalName = !bIsDisplayName && Key.GetFName().ToString().Compare(KeyName, ESearchCase::IgnoreCase) == 0;
				if (bIsDisplayName || bIsInternalName)
				{
					//Found!
					KeyScanCode = ScanCode; break;
				}
			}
		}

		if (KeyScanCode >= 0)
		{
			VrpnScanCode = KeyScanCode;
			return true;
		}
	}

	return false;
}
#include "Windows/HideWindowsPlatformTypes.h"
#endif

