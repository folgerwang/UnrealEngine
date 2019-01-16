// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHIShaderPlatformDefinitions.h: Localizable Friendly Names for Shader Platforms
=============================================================================*/

#pragma once

static const FText& GetFriendlyShaderPlatformName(const EShaderPlatform InShaderPlatform)
{
	switch (InShaderPlatform)
	{
	case SP_PCD3D_SM5:
	case SP_OPENGL_SM5:
	case SP_METAL_SM5:
	case SP_VULKAN_SM5:
	case SP_OPENGL_ES31_EXT:
	{
		static FText Description = LOCTEXT("Generic_SM5_loc", "SM5");
		return Description;
	}
	break;

	case SP_METAL_SM5_NOTESS:
	case SP_METAL_MRT_MAC:
	case SP_METAL_MRT:
	{
		static FText Description = LOCTEXT("Metal_SM5_loc", "Metal SM5");
		return Description;
	}
	break;

	case SP_PCD3D_SM4:
	case SP_OPENGL_SM4:
	case SP_VULKAN_SM4:
	{
		static FText Description = LOCTEXT("Generic_SM4_loc", "SM4");
		return Description;
	}
	break;

	case SP_PS4:
	{
		static FText Description = LOCTEXT("Playstation_4_loc", "PlayStation 4");
		return Description;
	}
	break;

	case SP_XBOXONE_D3D12:
	{
		static FText Description = LOCTEXT("XBox_One_loc", "XBox One");
		return Description;
	}
	break;

	case SP_SWITCH:
	{
		static FText Description = LOCTEXT("Switch_loc", "Switch");
		return Description;
	}
	break;

	case SP_SWITCH_FORWARD:
	{
		static FText Description = LOCTEXT("Switch_Forward_loc", "Switch Forward");
		return Description;
	}
	break;

	case SP_PCD3D_ES3_1:
	case SP_VULKAN_PCES3_1:
	case SP_OPENGL_PCES3_1:
	{
		static FText Description = LOCTEXT("Generic_ES31_loc", "ES31");
		return Description;
	}
	break;

	case SP_VULKAN_ES3_1_ANDROID:
	{
		static FText Description = LOCTEXT("Android_Vulkan_ES31_loc", "Android Vulkan ES31");
		return Description;
	}
	break;

	case SP_OPENGL_ES3_1_ANDROID:
	{
		static FText Description = LOCTEXT("Android_ES31_loc", "Android ES31");
		return Description;
	}
	break;

	case SP_VULKAN_ES3_1_LUMIN:
	{
		static FText Description = LOCTEXT("Lumin_Vulkan_ES31_loc", "Lumin Vulkan ES31");
		return Description;
	}
	break;

	case SP_METAL:
	case SP_METAL_MACES3_1:
	{
		static FText Description = LOCTEXT("iOS_Metal_Mace_31_loc", "Metal ES31");
		return Description;
	}
	break;

	case SP_VULKAN_SM5_LUMIN:
	{
		static FText Description = LOCTEXT("Lumin_Vulkan_SM5_loc", "Lumin Vulkan SM5");
		return Description;
	}
	break;

	case SP_PCD3D_ES2:
	case SP_OPENGL_PCES2:
	{
		static FText Description = LOCTEXT("Generic_ES2_loc", "ES2");
		return Description;
	}
	break;

	case SP_OPENGL_ES2_ANDROID:
	{
		static FText Description = LOCTEXT("Android_ES2_loc", "Android ES2");
		return Description;
	}
	break;

	case SP_OPENGL_ES2_WEBGL:
	{
		static FText Description = LOCTEXT("WebGL_ES2_loc", "WebGL ES2");
		return Description;
	}
	break;

	case SP_OPENGL_ES2_IOS:
	{
		static FText Description = LOCTEXT("iOS_ES2_loc", "iOS ES2");
		return Description;
	}
	break;

	case SP_METAL_MACES2:
	{
		static FText Description = LOCTEXT("iOS_Metal_Mace_2_loc", "iOS Metal ES2");
		return Description;
	}
	break;

	default:
		break;
	};

	static FText Description = LOCTEXT("", "");
	return Description;
}

