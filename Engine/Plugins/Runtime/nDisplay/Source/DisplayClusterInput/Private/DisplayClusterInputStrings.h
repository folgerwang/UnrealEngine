// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


namespace DisplayClusterInputStrings
{
	namespace cfg
	{
		namespace input
		{
			namespace keyboard
			{
				static constexpr auto TokenReflect    = TEXT("reflect=");
				static constexpr auto ReflectNdisplay = TEXT("ndisplay");
				static constexpr auto ReflectUE4      = TEXT("ue4");
				static constexpr auto ReflectBoth     = TEXT("both");
				static constexpr auto ReflectNone     = TEXT("none");
			}
		}
	}
};
