// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeVirtualTextureEnum.generated.h"

/** 
 * Enumeration of virtual texture stack layouts to support. 
 * Extend this enumeration with other layouts as required. For example we will probably want to add a displacement texture option.
 * This "fixed function" approach will probably break down if we end up needing to support some complex set of attribute combinations but it is OK to begin with.
 */
UENUM()
enum class ERuntimeVirtualTextureMaterialType
{
	BaseColor UMETA(DisplayName = "Base Color"),
	BaseColor_Normal UMETA(DisplayName = "Base Color, Normal"),
	BaseColor_Normal_Specular UMETA(DisplayName = "Base Color, Normal, Roughness, Specular"),
	
	Count UMETA(Hidden),
};

enum { ERuntimeVirtualTextureMaterialType_NumBits = 2 };
static_assert((uint32)ERuntimeVirtualTextureMaterialType::Count <= (1 << (uint32)ERuntimeVirtualTextureMaterialType_NumBits), "NumBits is too small");

/** Enumeration of main pass behaviors when rendering to a runtime virtual texture. */
UENUM()
enum class ERuntimeVirtualTextureMainPassType
{
	/** If there is no valid virtual texture target we will not render at all. Use this for items that we don't mind removing if there is no virtual texture support. */
	Never UMETA(DisplayName = "Virtual Texture Only"),
	/** If and only if there is no valid virtual texture target we will render to the main pass. Use this for items that we must have whether virtual texture is supported or not. */
	Exclusive UMETA(DisplayName = "Virtual Texture OR Main Pass"),
	/** We will render to any valid virtual texture target AND the main pass. Use this for items that need to both read and write the virtual texture. For example, some landscape setups need this. */
	Always UMETA(DisplayName = "Virtual Texture AND Main Pass"),
};
