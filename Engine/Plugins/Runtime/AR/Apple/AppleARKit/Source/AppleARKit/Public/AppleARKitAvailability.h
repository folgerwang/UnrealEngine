// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#if PLATFORM_IOS && !PLATFORM_TVOS
	#include "Availability.h"
#endif

#if PLATFORM_IOS && !PLATFORM_TVOS

	// Check for ARKit 1.0
	#ifdef __IPHONE_11_0
		#define SUPPORTS_ARKIT_1_0 1
	#else
		#define SUPPORTS_ARKIT_1_0 0
	#endif

	// Check for ARKit 1.5
	#ifdef __IPHONE_11_3
		#define SUPPORTS_ARKIT_1_5 1
	#else
		#define SUPPORTS_ARKIT_1_5 0
	#endif

	// Check for ARKit 2.0
	#ifdef __IPHONE_12_0
		#define SUPPORTS_ARKIT_2_0 1
	#else
		#define SUPPORTS_ARKIT_2_0 0
	#endif

#else

	// No ARKit support
	#define SUPPORTS_ARKIT_1_0 0
	#define SUPPORTS_ARKIT_1_5 0
	#define SUPPORTS_ARKIT_2_0 0

#endif

class FAppleARKitAvailability
{
public:
	static bool SupportsARKit10()
	{
		static bool bSupportsARKit10 = false;
#if SUPPORTS_ARKIT_1_0
		static bool bSupportChecked = false;
		if (!bSupportChecked)
		{
			bSupportChecked = true;
			// This call is slow, so cache the result
			if (@available(iOS 11.0, *))
			{
				bSupportsARKit10 = true;
			}
		}
#endif
		return bSupportsARKit10;
	}

	static bool SupportsARKit15()
	{
		static bool bSupportsARKit15 = false;
#if SUPPORTS_ARKIT_1_5
		static bool bSupportChecked = false;
		if (!bSupportChecked)
		{
			bSupportChecked = true;
			// This call is slow, so cache the result
			if (@available(iOS 11.3, *))
			{
				bSupportsARKit15 = true;
			}
		}
#endif
		return bSupportsARKit15;
	}

	static bool SupportsARKit20()
	{
		static bool bSupportsARKit20 = false;
#if SUPPORTS_ARKIT_2_0
		static bool bSupportChecked = false;
		if (!bSupportChecked)
		{
			bSupportChecked = true;
			// This call is slow, so cache the result
			if (@available(iOS 12.0, *))
			{
				bSupportsARKit20 = true;
			}
		}
#endif
		return bSupportsARKit20;
	}
};
