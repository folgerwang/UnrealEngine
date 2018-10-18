// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
//
#include "MaterialStatsCommon.h"
#include "EngineGlobals.h"
#include "MaterialStats.h"
#include "LocalVertexFactory.h"
#include "GPUSkinVertexFactory.h"
#include "MaterialEditorSettings.h"

/***********************************************************************************************************************/
/*begin FMaterialResourceStats functions*/

void FMaterialResourceStats::SetupExtaCompilationSettings(const EShaderPlatform Platform, FExtraShaderCompilerSettings& Settings) const
{
	Settings.bExtractShaderSource = true;
	Settings.OfflineCompilerPath = FMaterialStatsUtils::GetPlatformOfflineCompilerPath(Platform);
}

/*end FMaterialResourceStats functions*/
/***********************************************************************************************************************/

/***********************************************************************************************************************/
/*begin FMaterialStatsUtils */
const FLinearColor FMaterialStatsUtils::BlueColor(0.1851f, 1.0f, 0.940258f);
const FLinearColor FMaterialStatsUtils::YellowColor(1.0f, 0.934216f, 0.199542f);
const FLinearColor FMaterialStatsUtils::GreenColor(0.540805f, 1.0f, 0.321716f);
const FLinearColor FMaterialStatsUtils::OrangeColor(1.0f, 0.316738f, 0.095488f);
const FLinearColor FMaterialStatsUtils::DefaultGridTextColor(0.244819f, 0.301351f, 0.390625f);

TSharedPtr<FMaterialStats> FMaterialStatsUtils::CreateMaterialStats(class IMaterialEditor* MaterialEditor)
{
	TSharedPtr<FMaterialStats> MaterialStats = MakeShareable(new FMaterialStats());
	MaterialStats->Initialize(MaterialEditor);

	return MaterialStats;
}

FString FMaterialStatsUtils::MaterialQualityToString(const EMaterialQualityLevel::Type Quality)
{
	FString StrQuality;

	switch (Quality)
	{
		case EMaterialQualityLevel::High:
			StrQuality = TEXT("High Quality");
		break;
		case EMaterialQualityLevel::Medium:
			StrQuality = TEXT("Medium Quality");
		break;
		case EMaterialQualityLevel::Low:
			StrQuality = TEXT("Low Quality");
		break;
	}

	return StrQuality;
}

FString FMaterialStatsUtils::MaterialQualityToShortString(const EMaterialQualityLevel::Type Quality)
{
	FString StrQuality;

	switch (Quality)
	{
		case EMaterialQualityLevel::High:
			StrQuality = TEXT("High");
		break;
		case EMaterialQualityLevel::Medium:
			StrQuality = TEXT("Medium");
		break;
		case EMaterialQualityLevel::Low:
			StrQuality = TEXT("Low");
		break;
	}

	return StrQuality;
}

EMaterialQualityLevel::Type FMaterialStatsUtils::StringToMaterialQuality(const FString& StrQuality)
{
	if (StrQuality.Equals(TEXT("High Quality")))
	{
		return EMaterialQualityLevel::High;
	}		
	else if (StrQuality.Equals(TEXT("Medium Quality")))
	{
		return EMaterialQualityLevel::Medium;
	}
	else if (StrQuality.Equals(TEXT("Low Quality")))
	{
		return EMaterialQualityLevel::Low;
	}

	return EMaterialQualityLevel::Num;
}

FString FMaterialStatsUtils::GetPlatformTypeName(const EPlatformCategoryType InEnumValue)
{
	FString PlatformName;

	switch (InEnumValue)
	{
		case EPlatformCategoryType::Desktop:
			PlatformName = FString("Desktop");
		break;
		case EPlatformCategoryType::Android:
			PlatformName = FString("Android");
		break;
		case EPlatformCategoryType::IOS:
			PlatformName = FString("IOS");
		break;
	}

	return PlatformName;
}

FString FMaterialStatsUtils::ShaderPlatformTypeName(const EShaderPlatform PlatformID)
{
	switch (PlatformID)
	{
		case SP_PCD3D_SM5:
			return FString("PCD3D_SM5");
		break;
		case SP_OPENGL_SM4:
			return FString("OPENGL_SM4");
		break;
		case SP_PS4:
			return FString("OPENGL_SM4");
		break;
		case SP_OPENGL_PCES2:
			return FString("OPENGL_PCES2");
		break;
		case SP_XBOXONE_D3D12:
			return FString("XBOXONE_D3D12");
		break;
		case SP_PCD3D_SM4:
			return FString("PCD3D_SM4");
		break;
		case SP_OPENGL_SM5:
			return FString("OPENGL_SM5");
		break;
		case SP_PCD3D_ES2:
			return FString("PCD3D_ES2");
		break;
		case SP_OPENGL_ES2_ANDROID:
			return FString("OPENGL_ES2_ANDROID");
		break;
		case SP_OPENGL_ES2_WEBGL:
			return FString("OPENGL_ES2_WEBGL");
		break;
		case SP_OPENGL_ES2_IOS:
			return FString("OPENGL_ES2_IOS");
		break;
		case SP_METAL:
			return FString("METAL");
		break;
		case SP_METAL_MRT:
			return FString("METAL_MRT");
		break;
		case SP_METAL_TVOS:
			return FString("METAL_TVOS");
		break;
		case SP_METAL_MRT_TVOS:
			return FString("METAL_MRT_TVOS");
		break;
		case SP_OPENGL_ES31_EXT:
			return FString("OPENGL_ES31_EXT");
		break;
		case SP_PCD3D_ES3_1:
			return FString("PCD3D_ES3_1");
		break;
		case SP_OPENGL_PCES3_1:
			return FString("OPENGL_PCES3_1");
		break;
		case SP_METAL_SM5:
			return FString("METAL_SM5");
		break;
		case SP_VULKAN_PCES3_1:
			return FString("VULKAN_PCES3_1");
		break;
		case SP_METAL_SM5_NOTESS:
			return FString("METAL_SM5_NOTESS");
		break;
		case SP_VULKAN_SM4:
			return FString("VULKAN_SM4");
		break;
		case SP_VULKAN_SM5:
			return FString("VULKAN_SM5");
		break;
		case SP_VULKAN_ES3_1_ANDROID:
			return FString("VULKAN_ES3_1_ANDROID");
		break;
		case SP_METAL_MACES3_1:
			return FString("METAL_MACES3_1");
		break;
		case SP_METAL_MACES2:
			return FString("METAL_MACES2");
		break;
		case SP_OPENGL_ES3_1_ANDROID:
			return FString("OPENGL_ES3_1_ANDROID");
		break;
		case SP_SWITCH:
			return FString("SWITCH");
		break;
		case SP_SWITCH_FORWARD:
			return FString("SWITCH_FORWARD");
		break;
		case SP_METAL_MRT_MAC:
			return FString("METAL_MRT_MAC");
		break;

		default:
			return FString("!Unknown platform!");
		break;
	}

	return FString("!Unknown platform!");
}

FString FMaterialStatsUtils::GetPlatformOfflineCompilerPath(const EShaderPlatform ShaderPlatform)
{
	switch (ShaderPlatform)
	{
		case SP_OPENGL_ES2_ANDROID:
		case SP_OPENGL_ES3_1_ANDROID:
		case SP_VULKAN_ES3_1_ANDROID:
		case SP_OPENGL_ES2_IOS:
			return FPaths::ConvertRelativePathToFull(GetDefault<UMaterialEditorSettings>()->MaliOfflineCompilerPath.FilePath);
		break;

		default:
			return FString();
		break;
	}

	return FString();
}

bool FMaterialStatsUtils::IsPlatformOfflineCompilerAvailable(const EShaderPlatform ShaderPlatform)
{
	FString CompilerPath = GetPlatformOfflineCompilerPath(ShaderPlatform);

	bool bCompilerExists = FPaths::FileExists(CompilerPath);

	return bCompilerExists;
}

bool FMaterialStatsUtils::PlatformNeedsOfflineCompiler(const EShaderPlatform ShaderPlatform)
{
	switch (ShaderPlatform)
	{
		case SP_OPENGL_SM4:
		case SP_PS4:
		case SP_OPENGL_PCES2:
		case SP_OPENGL_SM5:
		case SP_OPENGL_ES2_ANDROID:
		case SP_OPENGL_ES31_EXT:
		case SP_OPENGL_PCES3_1:
		case SP_OPENGL_ES2_WEBGL:
		case SP_OPENGL_ES2_IOS:
		case SP_VULKAN_PCES3_1:
		case SP_VULKAN_SM4:
		case SP_VULKAN_SM5:
		case SP_VULKAN_ES3_1_ANDROID:
		case SP_OPENGL_ES3_1_ANDROID:
			return true;
		break;


		case SP_PCD3D_SM5:
		case SP_XBOXONE_D3D12:
		case SP_PCD3D_SM4:
		case SP_PCD3D_ES2:
		case SP_METAL:
		case SP_METAL_MRT:
		case SP_METAL_TVOS:
		case SP_METAL_MRT_TVOS:
		case SP_PCD3D_ES3_1:
		case SP_METAL_SM5:
		case SP_METAL_SM5_NOTESS:
		case SP_METAL_MACES3_1:
		case SP_METAL_MACES2:
		case SP_SWITCH:
		case SP_SWITCH_FORWARD:
		case SP_METAL_MRT_MAC:
			return false;
		break;

		default:
			return false;
		break;	
	}

	return false;
}

FString FMaterialStatsUtils::RepresentativeShaderTypeToString(const ERepresentativeShader ShaderType)
{
	switch (ShaderType)
	{
		case ERepresentativeShader::StationarySurface:
			return TEXT("Stationary surface");
		break;

		case ERepresentativeShader::StationarySurfaceCSM:
			return TEXT("Stationary surface + CSM");
		break;

		case ERepresentativeShader::StationarySurface1PointLight:
		case ERepresentativeShader::StationarySurfaceNPointLights:
			return TEXT("Stationary surface + Point Lights");
		break;

		case ERepresentativeShader::DynamicallyLitObject:
			return TEXT("Dynamically lit object");
		break;

		case ERepresentativeShader::StaticMesh:
			return TEXT("Static Mesh");
		break;

		case ERepresentativeShader::SkeletalMesh:
			return TEXT("Skeletal Mesh");
		break;

		case ERepresentativeShader::UIDefaultFragmentShader:
			return TEXT("UI Pixel Shader");
		break;

		case ERepresentativeShader::UIDefaultVertexShader:
			return TEXT("UI Vertex Shader");
		break;

		case ERepresentativeShader::UIInstancedVertexShader:
			return TEXT("UI Instanced Vertex Shader");
		break;

		default:
			return TEXT("Unknown shader name");
		break;
	}
}

FLinearColor FMaterialStatsUtils::PlatformTypeColor(EPlatformCategoryType PlatformType)
{
	FLinearColor Color(FLinearColor::Blue);

	switch (PlatformType)
	{
		case EPlatformCategoryType::Desktop:
			Color = BlueColor;
		break;
		case EPlatformCategoryType::Android:
			Color = GreenColor;
		break;
		case EPlatformCategoryType::IOS:
			Color = FLinearColor::Gray;
		break;

		default:
			return Color;
		break;
	}

	return Color;
}

FLinearColor FMaterialStatsUtils::QualitySettingColor(const EMaterialQualityLevel::Type QualityType)
{
	switch (QualityType)
	{
		case EMaterialQualityLevel::Low:
			return GreenColor;
		break;
		case EMaterialQualityLevel::High:
			return OrangeColor;
		break;
		case EMaterialQualityLevel::Medium:
			return YellowColor;
		break;

		default:
			return FLinearColor::Black;
		break;
	}

	return FLinearColor::Black;
}

void FMaterialStatsUtils::GetRepresentativeShaderTypesAndDescriptions(TMap<FName, TArray<FRepresentativeShaderInfo>>& ShaderTypeNamesAndDescriptions, const FMaterial* TargetMaterial)
{
	static auto* MobileHDR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	bool bMobileHDR = MobileHDR && MobileHDR->GetValueOnAnyThread() == 1;

	static const FName FLocalVertexFactoryName = FLocalVertexFactory::StaticType.GetFName();
	static const FName FGPUFactoryName = TGPUSkinVertexFactory<true>::StaticType.GetFName();

	if (TargetMaterial->IsUIMaterial())
	{
		static FName TSlateMaterialShaderPSDefaultfalseName = TEXT("TSlateMaterialShaderPSDefaultfalse");
		ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
			.Add(FRepresentativeShaderInfo(ERepresentativeShader::UIDefaultFragmentShader, TSlateMaterialShaderPSDefaultfalseName, TEXT("Default UI Pixel Shader")));

		static FName TSlateMaterialShaderVSfalseName = TEXT("TSlateMaterialShaderVSfalse");
		ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
			.Add(FRepresentativeShaderInfo(ERepresentativeShader::UIDefaultVertexShader, TSlateMaterialShaderVSfalseName, TEXT("Default UI Vertex Shader")));

		static FName TSlateMaterialShaderVStrueName = TEXT("TSlateMaterialShaderVStrue");
		ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
			.Add(FRepresentativeShaderInfo(ERepresentativeShader::UIInstancedVertexShader, TSlateMaterialShaderVStrueName, TEXT("Instanced UI Vertex Shader")));
	}
	else if (TargetMaterial->GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		if (TargetMaterial->GetShadingModel() == MSM_Unlit)
		{
			//unlit materials are never lightmapped
			static FName TBasePassPSFNoLightMapPolicyName = TEXT("TBasePassPSFNoLightMapPolicy");
			ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
				.Add(FRepresentativeShaderInfo(ERepresentativeShader::StationarySurface, TBasePassPSFNoLightMapPolicyName, TEXT("Base pass shader without light map")));
		}
		else
		{
			//also show a dynamically lit shader
			static FName TBasePassPSFNoLightMapPolicyName = TEXT("TBasePassPSFNoLightMapPolicy");
			ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
				.Add(FRepresentativeShaderInfo(ERepresentativeShader::DynamicallyLitObject, TBasePassPSFNoLightMapPolicyName, TEXT("Base pass shader")));

			static auto* CVarAllowStaticLighting = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
			const bool bAllowStaticLighting = CVarAllowStaticLighting->GetValueOnAnyThread() != 0;

			if (bAllowStaticLighting)
			{
				if (TargetMaterial->IsUsedWithStaticLighting())
				{
					static FName TBasePassPSTLightMapPolicyName = TEXT("TBasePassPSTDistanceFieldShadowsAndLightMapPolicyHQ");
					ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
						.Add(FRepresentativeShaderInfo(ERepresentativeShader::StationarySurface, TBasePassPSTLightMapPolicyName, TEXT("Base pass shader with Surface Lightmap")));
				}

				static FName TBasePassPSFPrecomputedVolumetricLightmapLightingPolicyName = TEXT("TBasePassPSFPrecomputedVolumetricLightmapLightingPolicy");
				ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
					.Add(FRepresentativeShaderInfo(ERepresentativeShader::DynamicallyLitObject, TBasePassPSFPrecomputedVolumetricLightmapLightingPolicyName, TEXT("Base pass shader with Volumetric Lightmap")));
			}
		}

		static FName TBasePassVSFNoLightMapPolicyName = TEXT("TBasePassVSFNoLightMapPolicy");
		ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
			.Add(FRepresentativeShaderInfo(ERepresentativeShader::StaticMesh, TBasePassVSFNoLightMapPolicyName, TEXT("Base pass vertex shader")));

		ShaderTypeNamesAndDescriptions.FindOrAdd(FGPUFactoryName)
			.Add(FRepresentativeShaderInfo(ERepresentativeShader::SkeletalMesh, TBasePassVSFNoLightMapPolicyName, TEXT("Base pass vertex shader")));
	}
	else
	{
		const TCHAR* DescSuffix = bMobileHDR ? TEXT(" (HDR)") : TEXT(" (LDR)");

		if (TargetMaterial->GetShadingModel() == MSM_Unlit)
		{
			//unlit materials are never lightmapped
			static FName Name_HDRLinear64 = TEXT("TMobileBasePassPSFNoLightMapPolicy0HDRLinear64");
			static FName Name_LDRGamma32 = TEXT("TMobileBasePassPSFNoLightMapPolicy0LDRGamma32");

			const FName TBasePassForForwardShadingPSFNoLightMapPolicy0Name = bMobileHDR ? Name_HDRLinear64 : Name_LDRGamma32;

			const FString Description = FString::Printf(TEXT("Mobile base pass shader without light map%s"), DescSuffix);
			ShaderTypeNamesAndDescriptions.Add(FLocalVertexFactoryName)
				.Add(FRepresentativeShaderInfo(ERepresentativeShader::StationarySurface, TBasePassForForwardShadingPSFNoLightMapPolicy0Name, Description));
		}
		else
		{
			static auto* CVarAllowDistanceFieldShadows = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowDistanceFieldShadows"));
			const bool bAllowDistanceFieldShadows = CVarAllowDistanceFieldShadows->GetValueOnAnyThread() != 0;

			static auto* CVarPointLights = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileNumDynamicPointLights"));
			const bool bPointLights = CVarPointLights->GetValueOnAnyThread() > 0;

			static auto* CVarPointLightsStaticBranch = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileDynamicPointLightsUseStaticBranch"));
			const bool bPointLightsStaticBranch = CVarPointLightsStaticBranch->GetValueOnAnyThread() != 0;

			static auto* CVarMobileSkyLightPermutation = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.SkyLightPermutation"));
			const bool bOnlySkyPermutation = CVarMobileSkyLightPermutation->GetValueOnAnyThread() == 2;

			const int32 NumPointLights = bPointLightsStaticBranch ? CVarPointLights->GetValueOnAnyThread() : 1;

			if (TargetMaterial->IsUsedWithStaticLighting())
			{
				if (bAllowDistanceFieldShadows)// distance field shadows
				{
					// distance field shadows only shaders
					{
						static const FName Name_HDRLinear64 = bOnlySkyPermutation ? 
							TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsAndLQLightMapPolicy0HDRLinear64Skylight") : 
							TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsAndLQLightMapPolicy0HDRLinear64");
						static const FName Name_LDRGamma32 = bOnlySkyPermutation ? 
							TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsAndLQLightMapPolicy0LDRGamma32Skylight") : 
							TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsAndLQLightMapPolicy0LDRGamma32");
						static const FName ShaderName = bMobileHDR ? Name_HDRLinear64 : Name_LDRGamma32;

						const FString Description = FString::Printf(TEXT("Mobile base pass shader with distance field shadows%s"), DescSuffix);
						ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
							.Add(FRepresentativeShaderInfo(ERepresentativeShader::StationarySurface, ShaderName, Description));
					}

					static auto* CVarAllowDistanceFieldShadowsAndCSM = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableStaticAndCSMShadowReceivers"));
					const bool bAllowDistanceFieldShadowsAndCSM = CVarAllowDistanceFieldShadowsAndCSM->GetValueOnAnyThread() != 0;
					if (bAllowDistanceFieldShadowsAndCSM)
					{
						// distance field shadows & CSM shaders
						{
							static const FName Name_HDRLinear64 = bOnlySkyPermutation ? 
								TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy0HDRLinear64Skylight") : 
								TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy0HDRLinear64");
							static const FName Name_LDRGamma32 = bOnlySkyPermutation ? 
								TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy0LDRGamma32Skylight") : 
								TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy0LDRGamma32");
							static const FName ShaderName = bMobileHDR ? Name_HDRLinear64 : Name_LDRGamma32;

							const FString Description = FString::Printf(TEXT("Mobile base pass shader with distance field shadows and CSM%s"), DescSuffix);
							ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
								.Add(FRepresentativeShaderInfo(ERepresentativeShader::StationarySurfaceCSM, ShaderName, Description));
						}

						if (bPointLights) // add point lights shaders + distance field shadows
						{
							static const FName Name_HDRLinear64_OneLight = bOnlySkyPermutation ? 
								TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy1HDRLinear64Skylight") :
								TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy1HDRLinear64");
							static const FName Name_LDRGamma32_OneLight = bOnlySkyPermutation ? 
								TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy1LDRGamma32Skylight") : 
								TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicy1LDRGamma32");

							static const FName Name_HDRLinear64_NLights = bOnlySkyPermutation ? 
								TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicyINT32_MAXHDRLinear64Skylight") : 
								TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicyINT32_MAXHDRLinear64");
							static const FName Name_LDRGamma32_NLights = bOnlySkyPermutation ? 
								TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicyINT32_MAXLDRGamma32Skylight") :
								TEXT("TMobileBasePassPSFMobileDistanceFieldShadowsLightMapAndCSMLightingPolicyINT32_MAXLDRGamma32");

							static const FName ShaderName = bMobileHDR ?
								(bPointLightsStaticBranch ? Name_HDRLinear64_NLights : Name_HDRLinear64_OneLight) :
								(bPointLightsStaticBranch ? Name_LDRGamma32_NLights : Name_LDRGamma32_OneLight);

							const FString Description = FString::Printf(TEXT("Mobile base pass shader with distance field shadows, CSM and %s point light(s) %s"), bPointLightsStaticBranch ? TEXT("N") : TEXT("1"), DescSuffix);

							FRepresentativeShaderInfo ShaderInfo = bPointLightsStaticBranch ?
								FRepresentativeShaderInfo(ERepresentativeShader::StationarySurfaceNPointLights, ShaderName, Description) :
								FRepresentativeShaderInfo(ERepresentativeShader::StationarySurface1PointLight, ShaderName, Description);

							ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName).Add(ShaderInfo);
								
						}
					}
				}
				else //no shadows & lightmapped
				{
					static const FName Name_HDRLinear64 = bOnlySkyPermutation ? 
						TEXT("TMobileBasePassPSTLightMapPolicyLQ0HDRLinear64Skylight") :
						TEXT("TMobileBasePassPSTLightMapPolicyLQ0HDRLinear64");
					static const FName Name_LDRGamma32 = bOnlySkyPermutation ? 
						TEXT("TMobileBasePassPSTLightMapPolicyLQ0LDRGamma32Skylight") : 
						TEXT("TMobileBasePassPSTLightMapPolicyLQ0LDRGamma32");
					static const FName TSlateMaterialShaderVStrueName = bMobileHDR ? Name_HDRLinear64 : Name_LDRGamma32;

					ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
						.Add(FRepresentativeShaderInfo(ERepresentativeShader::StationarySurface, TSlateMaterialShaderVStrueName,
							FString::Printf(TEXT("Mobile base pass shader with static lighting%s"), DescSuffix)));

					if (bPointLights) // add point lights + lightmap
					{
						static const FName Name_HDRLinear64_OneLight = bOnlySkyPermutation ? 
							TEXT("TMobileBasePassPSTLightMapPolicyLQ1HDRLinear64Skylight") : 
							TEXT("TMobileBasePassPSTLightMapPolicyLQ1HDRLinear64");
						static const FName Name_LDRGamma32_OneLight = bOnlySkyPermutation ? 
							TEXT("TMobileBasePassPSTLightMapPolicyLQ1LDRGamma32Skylight") : 
							TEXT("TMobileBasePassPSTLightMapPolicyLQ1LDRGamma32");

						static const FName Name_HDRLinear64_NLights = bOnlySkyPermutation ? 
							TEXT("TMobileBasePassPSTLightMapPolicyLQINT32_MAXHDRLinear64Skylight") :
							TEXT("TMobileBasePassPSTLightMapPolicyLQINT32_MAXHDRLinear64");
						static const FName Name_LDRGamma32_NLights = bOnlySkyPermutation ?  
							TEXT("TMobileBasePassPSTLightMapPolicyLQINT32_MAXLDRGamma32Skylight") :
							TEXT("TMobileBasePassPSTLightMapPolicyLQINT32_MAXLDRGamma32");

						static const FName ShaderName = bMobileHDR ?
							(bPointLightsStaticBranch ? Name_HDRLinear64_NLights : Name_HDRLinear64_OneLight) :
							(bPointLightsStaticBranch ? Name_LDRGamma32_NLights : Name_LDRGamma32_OneLight);

						const FString Description = FString::Printf(TEXT("Mobile base pass shader with static lighting and %s point light(s) %s"), bPointLightsStaticBranch ? TEXT("N") : TEXT("1"), DescSuffix);

						FRepresentativeShaderInfo ShaderInfo = bPointLightsStaticBranch ?
							FRepresentativeShaderInfo(ERepresentativeShader::StationarySurfaceNPointLights, ShaderName, Description) :
							FRepresentativeShaderInfo(ERepresentativeShader::StationarySurface1PointLight, ShaderName, Description);

						ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName).Add(ShaderInfo);
					}
				}
			}

			// dynamically lit shader			
			static const FName Name_HDRLinear64 = bOnlySkyPermutation ? 
				TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMLightingPolicy0HDRLinear64Skylight") : 
				TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMLightingPolicy0HDRLinear64");
			static const FName Name_LDRGamma32 = bOnlySkyPermutation ? 
				TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMLightingPolicy0LDRGamma32Skylight") : 
				TEXT("TMobileBasePassPSFMobileMovableDirectionalLightCSMLightingPolicy0LDRGamma32");
			static const FName TBasePassForForwardShadingPSFSimpleDirectionalLightAndSHIndirectPolicy0Name = bMobileHDR ? Name_HDRLinear64 : Name_LDRGamma32;
			
			ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
				.Add(FRepresentativeShaderInfo(ERepresentativeShader::DynamicallyLitObject, TBasePassForForwardShadingPSFSimpleDirectionalLightAndSHIndirectPolicy0Name,
				FString::Printf(TEXT("Mobile base pass shader with only dynamic lighting%s"), DescSuffix)));

			static const FName Name_NoLM_HDRLinear64 = bOnlySkyPermutation ? 
				TEXT("TMobileBasePassVSFNoLightMapPolicyHDRLinear64Skylight") : 
				TEXT("TMobileBasePassVSFNoLightMapPolicyHDRLinear64");
			static const FName Name_NoLM_LDRGamma32 = bOnlySkyPermutation ? 
				TEXT("TMobileBasePassVSFNoLightMapPolicyLDRGamma32Skylight") : 
				TEXT("TMobileBasePassVSFNoLightMapPolicyLDRGamma32");
			static const FName TBasePassForForwardShadingVSFNoLightMapPolicyName = bMobileHDR ? Name_NoLM_HDRLinear64 : Name_NoLM_LDRGamma32;

			ShaderTypeNamesAndDescriptions.FindOrAdd(FLocalVertexFactoryName)
				.Add(FRepresentativeShaderInfo(ERepresentativeShader::StaticMesh, TBasePassForForwardShadingVSFNoLightMapPolicyName,
				FString::Printf(TEXT("Mobile base pass vertex shader%s"), DescSuffix)));

			ShaderTypeNamesAndDescriptions.FindOrAdd(FGPUFactoryName)
				.Add(FRepresentativeShaderInfo(ERepresentativeShader::SkeletalMesh, TBasePassForForwardShadingVSFNoLightMapPolicyName,
				FString::Printf(TEXT("Mobile base pass vertex shader%s"), DescSuffix)));
		}
	}
}

/**
* Gets instruction counts that best represent the likely usage of this material based on shading model and other factors.
* @param Results - an array of descriptions to be populated
*/
void FMaterialStatsUtils::GetRepresentativeInstructionCounts(TArray<FShaderInstructionsInfo>& Results, const FMaterialResource* Target)
{
	TMap<FName, TArray<FRepresentativeShaderInfo>> ShaderTypeNamesAndDescriptions;
	Results.Empty();

	//when adding a shader type here be sure to update FPreviewMaterial::ShouldCache()
	//so the shader type will get compiled with preview materials
	const FMaterialShaderMap* MaterialShaderMap = Target->GetGameThreadShaderMap();
	if (MaterialShaderMap && MaterialShaderMap->IsCompilationFinalized())
	{
		GetRepresentativeShaderTypesAndDescriptions(ShaderTypeNamesAndDescriptions, Target);

		if (Target->IsUIMaterial())
		{
			//for (const TPair<FName, FRepresentativeShaderInfo>& ShaderTypePair : ShaderTypeNamesAndDescriptions)
			for (auto DescriptionPair : ShaderTypeNamesAndDescriptions)
			{
				auto& DescriptionArray = DescriptionPair.Value;
				for (int32 i = 0; i < DescriptionArray.Num(); ++i)
				{
					const FRepresentativeShaderInfo& ShaderInfo = DescriptionArray[i];

					FShaderType* ShaderType = FindShaderTypeByName(ShaderInfo.ShaderName);
					const int32 NumInstructions = MaterialShaderMap->GetMaxNumInstructionsForShader(ShaderType);

					FShaderInstructionsInfo Info;
					Info.ShaderType = ShaderInfo.ShaderType;
					Info.ShaderDescription = ShaderInfo.ShaderDescription;
					Info.InstructionCount = NumInstructions;

					Results.Push(Info);
				}
			}
		}
		else
		{
			for (auto DescriptionPair : ShaderTypeNamesAndDescriptions)
			{
				FVertexFactoryType* FactoryType = FindVertexFactoryType(DescriptionPair.Key);
				const FMeshMaterialShaderMap* MeshShaderMap = MaterialShaderMap->GetMeshShaderMap(FactoryType);
				if (MeshShaderMap)
				{
					TMap<FName, FShader*> ShaderMap;
					MeshShaderMap->GetShaderList(ShaderMap);

					auto& DescriptionArray = DescriptionPair.Value;

					for (int32 i = 0; i < DescriptionArray.Num(); ++i)
					{
						const FRepresentativeShaderInfo& ShaderInfo = DescriptionArray[i];

						FShader** ShaderEntry = ShaderMap.Find(ShaderInfo.ShaderName);
						if (ShaderEntry != nullptr)
						{
							FShaderType* ShaderType = (*ShaderEntry)->GetType();
							{
								const int32 NumInstructions = MeshShaderMap->GetMaxNumInstructionsForShader(ShaderType);

								FShaderInstructionsInfo Info;
								Info.ShaderType = ShaderInfo.ShaderType;
								Info.ShaderDescription = ShaderInfo.ShaderDescription;
								Info.InstructionCount = NumInstructions;

								Results.Push(Info);
							}
						}
					}
				}
			}
		}
	}
}

void FMaterialStatsUtils::ExtractMatertialStatsInfo(FShaderStatsInfo& OutInfo, const FMaterialResource* MaterialResource)
{
	// extract potential errors
	const ERHIFeatureLevel::Type MaterialFeatureLevel = MaterialResource->GetFeatureLevel();
	FString FeatureLevelName;
	GetFeatureLevelName(MaterialFeatureLevel, FeatureLevelName);

	OutInfo.Empty();
	TArray<FString> CompileErrors = MaterialResource->GetCompileErrors();
	for (int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
	{
		OutInfo.StrShaderErrors += FString::Printf(TEXT("[%s] %s\n"), *FeatureLevelName, *CompileErrors[ErrorIndex]);
	}

	bool bNoErrors = OutInfo.StrShaderErrors.Len() == 0;

	if (bNoErrors)
	{
		// extract instructions info
		TArray<FMaterialStatsUtils::FShaderInstructionsInfo> ShaderInstructionInfo;
		GetRepresentativeInstructionCounts(ShaderInstructionInfo, MaterialResource);

		for (int32 InstructionIndex = 0; InstructionIndex < ShaderInstructionInfo.Num(); InstructionIndex++)
		{
			FShaderStatsInfo::FContent Content;

			Content.StrDescription = ShaderInstructionInfo[InstructionIndex].InstructionCount > 0 ? FString::Printf(TEXT("%u"), ShaderInstructionInfo[InstructionIndex].InstructionCount) : TEXT("n/a");
			Content.StrDescriptionLong = ShaderInstructionInfo[InstructionIndex].InstructionCount > 0 ?
				FString::Printf(TEXT("%s: %u instructions"), *ShaderInstructionInfo[InstructionIndex].ShaderDescription, ShaderInstructionInfo[InstructionIndex].InstructionCount) :
				TEXT("Offline shader compiler not available or an error was encountered!");

			OutInfo.ShaderInstructionCount.Add(ShaderInstructionInfo[InstructionIndex].ShaderType, Content);
		}

		// extract samplers info
		const int32 SamplersUsed = FMath::Max(MaterialResource->GetSamplerUsage(), 0);
		const int32 MaxSamplers = GetExpectedFeatureLevelMaxTextureSamplers(MaterialResource->GetFeatureLevel());
		OutInfo.SamplersCount.StrDescription = FString::Printf(TEXT("%u/%u"), SamplersUsed, MaxSamplers);
		OutInfo.SamplersCount.StrDescriptionLong = FString::Printf(TEXT("%s samplers: %u/%u"), TEXT("Texture"), SamplersUsed, MaxSamplers);

		// extract esimated sample info
		uint32 NumVSTextureSamples = 0, NumPSTextureSamples = 0;
		MaterialResource->GetEstimatedNumTextureSamples(NumVSTextureSamples, NumPSTextureSamples);

		OutInfo.TextureSampleCount.StrDescription = FString::Printf(TEXT("VS(%u), PS(%u)"), NumVSTextureSamples, NumPSTextureSamples);
		OutInfo.TextureSampleCount.StrDescriptionLong = FString::Printf(TEXT("Texture Lookups (Est.): Vertex(%u), Pixel(%u)"), NumVSTextureSamples, NumPSTextureSamples);

		// extract interpolators info
		uint32 UVScalarsUsed, CustomInterpolatorScalarsUsed;
		MaterialResource->GetUserInterpolatorUsage(UVScalarsUsed, CustomInterpolatorScalarsUsed);

		const uint32 TotalScalars = UVScalarsUsed + CustomInterpolatorScalarsUsed;
		const uint32 MaxScalars = FMath::DivideAndRoundUp(TotalScalars, 4u) * 4;

		OutInfo.InterpolatorsCount.StrDescription = FString::Printf(TEXT("%u/%u"), TotalScalars, MaxScalars);
		OutInfo.InterpolatorsCount.StrDescriptionLong = FString::Printf(TEXT("User interpolators: %u/%u Scalars (%u/4 Vectors) (TexCoords: %i, Custom: %i)"),
			TotalScalars, MaxScalars, MaxScalars / 4, UVScalarsUsed, CustomInterpolatorScalarsUsed);
	}
}

/*end FMaterialStatsUtils */
/***********************************************************************************************************************/
