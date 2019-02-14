// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOColorTransform.h"

#include "IOpenColorIOModule.h"
#include "Materials/MaterialInterface.h"
#include "Math/PackedVector.h"
#include "Modules/ModuleManager.h"
#include "OpenColorIOConfiguration.h"
#include "UObject/UObjectIterator.h"


#if WITH_EDITOR
#include "DerivedDataCacheInterface.h"
#include "Editor.h"
#include "Interfaces/ITargetPlatform.h"
#include "OpenColorIODerivedDataVersion.h"
#include "OpenColorIOShader.h"

#if WITH_OCIO
#include "OpenColorIO/OpenColorIO.h"
#include <vector>
#endif

#endif //WITH_EDITOR




void UOpenColorIOColorTransform::SerializeOpenColorIOShaderMaps(const TMap<const ITargetPlatform*, TArray<FOpenColorIOTransformResource*>>* PlatformColorTransformResourcesToSavePtr, FArchive& Ar, TArray<FOpenColorIOTransformResource>&  OutLoadedResources)
{
	if (Ar.IsSaving())
	{
		int32 NumResourcesToSave = 0;
		const TArray<FOpenColorIOTransformResource*>* ColorTransformResourcesToSavePtr = nullptr;
		if (Ar.IsCooking())
		{
			check(PlatformColorTransformResourcesToSavePtr);
			auto& PlatformColorTransformResourcesToSave = *PlatformColorTransformResourcesToSavePtr;

			ColorTransformResourcesToSavePtr = PlatformColorTransformResourcesToSave.Find(Ar.CookingTarget());
			check(ColorTransformResourcesToSavePtr != nullptr || (Ar.GetLinker() == nullptr));
			if (ColorTransformResourcesToSavePtr != nullptr)
			{
				NumResourcesToSave = ColorTransformResourcesToSavePtr->Num();
			}
		}

		Ar << NumResourcesToSave;

		if (ColorTransformResourcesToSavePtr)
		{
			const TArray<FOpenColorIOTransformResource*> &ColorTransformResourcesToSave = *ColorTransformResourcesToSavePtr;
			for (int32 ResourceIndex = 0; ResourceIndex < NumResourcesToSave; ResourceIndex++)
			{
				ColorTransformResourcesToSave[ResourceIndex]->SerializeShaderMap(Ar);
			}
		}

	}
	else if (Ar.IsLoading())
	{
		int32 NumLoadedResources = 0;
		Ar << NumLoadedResources;
		OutLoadedResources.Empty(NumLoadedResources);

		for (int32 ResourceIndex = 0; ResourceIndex < NumLoadedResources; ResourceIndex++)
		{
			FOpenColorIOTransformResource LoadedResource;
			LoadedResource.SerializeShaderMap(Ar);
			OutLoadedResources.Add(LoadedResource);
		}
	}
}

void UOpenColorIOColorTransform::ProcessSerializedShaderMaps(UOpenColorIOColorTransform* Owner, TArray<FOpenColorIOTransformResource>& LoadedResources, FOpenColorIOTransformResource* (&OutColorTransformResourcesLoaded)[ERHIFeatureLevel::Num])
{
	check(IsInGameThread());

	for (FOpenColorIOTransformResource& Resource : LoadedResources)
	{
		Resource.RegisterShaderMap();
	}

	for (int32 ResourceIndex = 0; ResourceIndex < LoadedResources.Num(); ResourceIndex++)
	{
		FOpenColorIOTransformResource& LoadedResource = LoadedResources[ResourceIndex];
		FOpenColorIOShaderMap* LoadedShaderMap = LoadedResource.GetGameThreadShaderMap();

		if (LoadedShaderMap && LoadedShaderMap->GetShaderPlatform() == GMaxRHIShaderPlatform)
		{
			ERHIFeatureLevel::Type LoadedFeatureLevel = LoadedShaderMap->GetShaderMapId().FeatureLevel;
			if (!OutColorTransformResourcesLoaded[LoadedFeatureLevel])
			{
				OutColorTransformResourcesLoaded[LoadedFeatureLevel] = Owner->AllocateResource();
			}

			OutColorTransformResourcesLoaded[LoadedFeatureLevel]->SetInlineShaderMap(LoadedShaderMap);
		}
	}
}

void UOpenColorIOColorTransform::GetOpenColorIOLUTKeyGuid(const FString& InLutIdentifier, FGuid& OutLutGuid)
{
#if WITH_EDITOR
	FString DDCKey = FDerivedDataCacheInterface::BuildCacheKey(TEXT("OCIOLUT"), OPENCOLORIO_DERIVEDDATA_VER, *InLutIdentifier);

#if WITH_OCIO
	//Keep library version in the DDC key to invalidate it once we move to a new library
	DDCKey += TEXT("OCIOVersion");
	DDCKey += TEXT(OCIO_VERSION);
#endif //WITH_OCIO

	const uint32 KeyLength = DDCKey.Len() * sizeof(DDCKey[0]);
	uint32 Hash[5];
	FSHA1::HashBuffer(*DDCKey, KeyLength, reinterpret_cast<uint8*>(Hash));
	OutLutGuid = FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);
#endif
}

UOpenColorIOColorTransform::UOpenColorIOColorTransform(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UOpenColorIOColorTransform::Initialize(UOpenColorIOConfiguration* InOwner, const FString& InSourceColorSpace, const FString& InDestinationColorSpace)
{
	check(InOwner);
	ConfigurationOwner = InOwner;
	return GenerateColorTransformData(InSourceColorSpace, InDestinationColorSpace);
}

void UOpenColorIOColorTransform::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	SerializeOpenColorIOShaderMaps(&CachedColorTransformResourcesForCooking, Ar, LoadedTransformResources);
#else
	SerializeOpenColorIOShaderMaps(nullptr, Ar, LoadedTransformResources);
#endif

	SerializeLuts(Ar);
}


void UOpenColorIOColorTransform::CacheResourceShadersForCooking(EShaderPlatform InShaderPlatform, const FString& InShaderHash, const FString& InShaderCode, TArray<FOpenColorIOTransformResource*>& OutCachedResources)
{
	const ERHIFeatureLevel::Type TargetFeatureLevel = GetMaxSupportedFeatureLevel(InShaderPlatform);

	FOpenColorIOTransformResource* NewResource = AllocateResource();
	NewResource->SetupResource((ERHIFeatureLevel::Type)TargetFeatureLevel, InShaderHash, InShaderCode, GetTransformFriendlyName());

	const bool bApplyCompletedShaderMap = false;
	const bool bIsCooking = true;
	CacheShadersForResources(InShaderPlatform, NewResource, bApplyCompletedShaderMap, bIsCooking);

	OutCachedResources.Add(NewResource);
}

void UOpenColorIOColorTransform::SerializeLuts(FArchive& Ar)
{

	if (Ar.IsSaving())
	{
		UVolumeTexture* Lut3dTexturePtr = Lut3dTexture.Get();
		int32 Num3dLutsToSave = 0;
		if (Ar.IsCooking())
		{
			if (Lut3dTexturePtr != nullptr)
			{
				Num3dLutsToSave = 1;
			}
		}

		Ar << Num3dLutsToSave;

		if (Num3dLutsToSave > 0)
		{
			Ar << Lut3dTexturePtr;
		}
	}
	else if (Ar.IsLoading())
	{
		int32 NumLoaded3dLuts = 0;
		Ar << NumLoaded3dLuts;

		if (NumLoaded3dLuts > 0)
		{
			//Will only happen on cooked data
			UVolumeTexture* TempTexture = nullptr;
			Ar << TempTexture;
			Lut3dTexture.Reset(TempTexture);
		}
	}
}

void UOpenColorIOColorTransform::CacheResourceTextures()
{
	if (!Lut3dTexture.IsValid())
	{
#if WITH_EDITOR && WITH_OCIO
		OCIO_NAMESPACE::ConstConfigRcPtr CurrentConfig = ConfigurationOwner->GetLoadedConfigurationFile();
		if (CurrentConfig)
		{
#if !PLATFORM_EXCEPTIONS_DISABLED
			try
#endif
			{
				OCIO_NAMESPACE::ConstProcessorRcPtr TransformProcessor = CurrentConfig->getProcessor(StringCast<ANSICHAR>(*SourceColorSpace).Get(), StringCast<ANSICHAR>(*DestinationColorSpace).Get());
				if (TransformProcessor)
				{
					OCIO_NAMESPACE::GpuShaderDesc ShaderDescription;
					ShaderDescription.setLanguage(OCIO_NAMESPACE::GPU_LANGUAGE_CG);
					ShaderDescription.setFunctionName(StringCast<ANSICHAR>(OpenColorIOShader::OpenColorIOShaderFunctionName).Get());
					ShaderDescription.setLut3DEdgeLen(OpenColorIOShader::Lut3dEdgeLength);

					FString Lut3dIdentifier = StringCast<TCHAR>(TransformProcessor->getGpuLut3DCacheID(ShaderDescription)).Get();
					if (Lut3dIdentifier != TEXT("<NULL>"))
					{
						std::vector<float> Lut3dData;
						const uint32 LutLength = OpenColorIOShader::Lut3dEdgeLength;
						const uint32 TotalItemCount = LutLength * LutLength * LutLength;
						Lut3dData.resize(3 * TotalItemCount);
						TransformProcessor->getGpuLut3D(&Lut3dData[0], ShaderDescription);

						//In editor, it will use what's on DDC if there's something corresponding to the actual data or use that raw data
						//that OCIO library has on board. The texture will be serialized only when cooking.
						Update3dLutTexture(Lut3dIdentifier, &Lut3dData[0]);
					}
				}
				else
				{
					UE_LOG(LogOpenColorIO, Error, TEXT("Failed to cache 3dLUT for color transform %s. Transform processor was unusable."), *GetTransformFriendlyName());
				}
			}
#if !PLATFORM_EXCEPTIONS_DISABLED
			catch (OCIO_NAMESPACE::Exception& exception)
			{
				UE_LOG(LogOpenColorIO, Error, TEXT("Failed to cache 3dLUT for color transform %s. Error message: %s."), *GetTransformFriendlyName(), StringCast<TCHAR>(exception.what()).Get());
			}
#endif
		}
		else
		{
			UE_LOG(LogOpenColorIO, Error, TEXT("Failed to cache 3dLUT for color transform %s. Configuration file was invalid."), *GetTransformFriendlyName());
		}
#endif
	}
	else
	{
		//This is the path for cooked data where the 3dLut is serialized in the transform asset.
		Lut3dTexture->UpdateResource();
	}
}

void UOpenColorIOColorTransform::CacheResourceShadersForRendering(bool bRegenerateId)
{
	if (bRegenerateId)
	{
		FlushResourceShaderMaps();
	}

	if (FApp::CanEverRender())
	{
		//Update shader hash to fetch pre-compiled shader from DDC and grab shader code to be able to compile it on the fly if it's missing
		FString ShaderCodeHash;
		FString ShaderCode;
		if (UpdateShaderInfo(ShaderCodeHash, ShaderCode))
		{
			//OCIO shaders are simple, we should be compatible with any feature levels. Use the levels required for materials.
			uint32 FeatureLevelsToCompile = UMaterialInterface::GetFeatureLevelsToCompileForAllMaterials();
			while (FeatureLevelsToCompile != 0)
			{
				ERHIFeatureLevel::Type CacheFeatureLevel = (ERHIFeatureLevel::Type)FBitSet::GetAndClearNextBit(FeatureLevelsToCompile);
				const EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[CacheFeatureLevel];

				FOpenColorIOTransformResource*& TransformResource = ColorTransformResources[CacheFeatureLevel];
				if (TransformResource == nullptr)
				{
					TransformResource = AllocateResource();
				}

				TransformResource->SetupResource(CacheFeatureLevel, ShaderCodeHash, ShaderCode, GetTransformFriendlyName());

				const bool bApplyCompletedShaderMap = true;
				const bool bIsCooking = false;
				CacheShadersForResources(ShaderPlatform, TransformResource, bApplyCompletedShaderMap, bIsCooking);
			}
		}
	}
}

void UOpenColorIOColorTransform::CacheShadersForResources(EShaderPlatform InShaderPlatform, FOpenColorIOTransformResource* InResourceToCache, bool bApplyCompletedShaderMapForRendering, bool bIsCooking)
{
	const bool bSuccess = InResourceToCache->CacheShaders(InShaderPlatform, bApplyCompletedShaderMapForRendering, bIsCooking);

	if (!bSuccess)
	{
		UE_ASSET_LOG(LogOpenColorIO, Warning, this, TEXT("Failed to compile OCIO ColorSpace transform %s shader for platform %s.")
			, *LegacyShaderPlatformToShaderFormat(InShaderPlatform).ToString()
			, *InResourceToCache->GetFriendlyName());

		const TArray<FString>& CompileErrors = InResourceToCache->GetCompileErrors();
		for (int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
		{
			UE_LOG(LogOpenColorIO, Warning, TEXT("	%s"), *CompileErrors[ErrorIndex]);
		}
	}
}

FOpenColorIOTransformResource* UOpenColorIOColorTransform::AllocateResource()
{
	return new FOpenColorIOTransformResource();
}

bool UOpenColorIOColorTransform::GetShaderAndLUTResouces(ERHIFeatureLevel::Type InFeatureLevel, FOpenColorIOTransformResource*& OutShaderResource, FTextureResource*& OutLUT3dResource)
{
	OutShaderResource = ColorTransformResources[InFeatureLevel];
	if (OutShaderResource)
	{
		//Some color transform will only require shader code with no LUT involved.
		UVolumeTexture* Lut3dTexturePtr = Lut3dTexture.Get();
		if (Lut3dTexturePtr != nullptr)
		{
			OutLUT3dResource = Lut3dTexturePtr->Resource;
		}

		return true;
	}
	else
	{
		UE_LOG(LogOpenColorIO, Warning, TEXT("Shader resource was invalid for color transform %s. Were there errors during loading?"), *GetTransformFriendlyName());
		return false;
	}
}

bool UOpenColorIOColorTransform::IsTransform(const FString& InSourceColorSpace, const FString& InDestinationColorSpace) const
{
	return SourceColorSpace == InSourceColorSpace && DestinationColorSpace == InDestinationColorSpace;
}

void UOpenColorIOColorTransform::AllColorTransformsCacheResourceShadersForRendering()
{
	for (TObjectIterator<UOpenColorIOColorTransform> It; It; ++It)
	{
		UOpenColorIOColorTransform* Transform = *It;

		Transform->CacheResourceShadersForRendering(false);
	}
}

bool UOpenColorIOColorTransform::GenerateColorTransformData(const FString& InSourceColorSpace, const FString& InDestinationColorSpace)
{
#if WITH_EDITOR && WITH_OCIO
	if (InSourceColorSpace.IsEmpty() || InDestinationColorSpace.IsEmpty())
	{
		return false;
	}

	SourceColorSpace = InSourceColorSpace;
	DestinationColorSpace = InDestinationColorSpace;

	CacheResourceTextures();
	CacheResourceShadersForRendering(true);

	return true;
#endif //WITH_EDITOR
	return false;
}

FString UOpenColorIOColorTransform::GetTransformFriendlyName()
{
	return SourceColorSpace + TEXT(" to ") + DestinationColorSpace;
}

bool UOpenColorIOColorTransform::UpdateShaderInfo(FString& OutShaderCodeHash, FString& OutShaderCode)
{
#if WITH_EDITOR
#if WITH_OCIO
	OCIO_NAMESPACE::ConstConfigRcPtr CurrentConfig = ConfigurationOwner->GetLoadedConfigurationFile();
	if (CurrentConfig)
	{
#if !PLATFORM_EXCEPTIONS_DISABLED
		try
#endif
		{
			OCIO_NAMESPACE::ConstProcessorRcPtr TransformProcessor = CurrentConfig->getProcessor(StringCast<ANSICHAR>(*SourceColorSpace).Get(), StringCast<ANSICHAR>(*DestinationColorSpace).Get());
			if (TransformProcessor)
			{
				OCIO_NAMESPACE::GpuShaderDesc ShaderDescription;
				ShaderDescription.setLanguage(OCIO_NAMESPACE::GPU_LANGUAGE_CG);
				ShaderDescription.setFunctionName(StringCast<ANSICHAR>(OpenColorIOShader::OpenColorIOShaderFunctionName).Get());
				ShaderDescription.setLut3DEdgeLen(OpenColorIOShader::Lut3dEdgeLength);

				OutShaderCodeHash = StringCast<TCHAR>(TransformProcessor->getGpuShaderTextCacheID(ShaderDescription)).Get();
				FString GLSLShaderCode = StringCast<TCHAR>(TransformProcessor->getGpuShaderText(ShaderDescription)).Get();

				//CG language works with HLSL. Just update texture sampling to work with newest method
				const FString SamplerString = FString::Printf(TEXT("%s.Sample"), OpenColorIOShader::OCIOLut3dName);
				GLSLShaderCode = GLSLShaderCode.Replace(TEXT("tex3D"), *SamplerString, ESearchCase::CaseSensitive);
				GLSLShaderCode = GLSLShaderCode.Replace(TEXT("sampler3D"), TEXT("SamplerState"), ESearchCase::CaseSensitive);

				OutShaderCode = GLSLShaderCode;
				return true;
			}
			else
			{
				UE_LOG(LogOpenColorIO, Error, TEXT("Failed to fetch shader info for color transform %s. Transform processor was unusable."), *GetTransformFriendlyName());
			}
		}
#if !PLATFORM_EXCEPTIONS_DISABLED
		catch (OCIO_NAMESPACE::Exception& exception)
		{
			UE_LOG(LogOpenColorIO, Error, TEXT("Failed to fetch shader info for color transform %s. Error message: %s."), *GetTransformFriendlyName(), StringCast<TCHAR>(exception.what()).Get());
		}
#endif
	}
	else
	{
		UE_LOG(LogOpenColorIO, Error, TEXT("Failed to fetch shader info for color transform %s. Configuration file was invalid."), *GetTransformFriendlyName());
	}

	return false;
#else
	//Avoid triggering errors when building maps on build machine.
	if (!GIsBuildMachine)
	{
		UE_LOG(LogOpenColorIO, Error, TEXT("Can't update shader, OCIO library isn't present."));
	}
	return false;
#endif //WITH_OCIO
#else
	return true; //When not in editor, shaders have been cooked so we're not relying on the library data anymore.
#endif
}

void UOpenColorIOColorTransform::Update3dLutTexture(const FString& InLutIdentifier, const float* InSourceData)
{
#if WITH_EDITOR && WITH_OCIO
	check(InSourceData);

	Lut3dTexture.Reset(NewObject<UVolumeTexture>(this, NAME_None, RF_NoFlags));
	UVolumeTexture* Lut3dTexturePtr = Lut3dTexture.Get();

	//Initializes source data with the raw LUT. If it's found in DDC, the resulting platform data will be fetched from there. 
	//If not, the source data will be used to generate the platform data.
	Lut3dTexturePtr->MipGenSettings = TMGS_NoMipmaps;
	Lut3dTexturePtr->CompressionNone = true;
	Lut3dTexturePtr->Source.Init(OpenColorIOShader::Lut3dEdgeLength, OpenColorIOShader::Lut3dEdgeLength, OpenColorIOShader::Lut3dEdgeLength, /*NumMips=*/ 1, TSF_RGBA16F, nullptr);

	FFloat16Color* MipData = reinterpret_cast<FFloat16Color*>(Lut3dTexturePtr->Source.LockMip(0));
	const uint32 LutLength = OpenColorIOShader::Lut3dEdgeLength;
	for (uint32 Z = 0; Z < LutLength; ++Z)
	{
		for (uint32 Y = 0; Y < LutLength; Y++)
		{
			FFloat16Color* Row = &MipData[Y * LutLength + Z * LutLength * LutLength];
			const float* Source = &InSourceData[Y * LutLength * 3 + Z * LutLength * LutLength * 3];
			for (uint32 X = 0; X < LutLength; X++)
			{
				Row[X] = FFloat16Color(FLinearColor(Source[X * 3 + 0], Source[X * 3 + 1], Source[X * 3 + 2]));
			}
		}
	}
	Lut3dTexturePtr->Source.UnlockMip(0);

	//Generate a Guid from the identifier received from the library and our DDC version.
	FGuid LutGuid;
	GetOpenColorIOLUTKeyGuid(InLutIdentifier, LutGuid);
	Lut3dTexturePtr->Source.SetId(LutGuid, true);

	//Process our new texture to be usable in rendering pipeline.
	Lut3dTexturePtr->UpdateResource();
#endif
}

void UOpenColorIOColorTransform::FlushResourceShaderMaps()
{
	if (FApp::CanEverRender())
	{
		for (int32 Index = 0; Index < ERHIFeatureLevel::Num; Index++)
		{
			if (ColorTransformResources[Index])
			{
				ColorTransformResources[Index]->ReleaseShaderMap();
				ColorTransformResources[Index] = nullptr;
			}
		}
	}
}

void UOpenColorIOColorTransform::PostLoad()
{
	Super::PostLoad();

	if (FApp::CanEverRender())
	{
		ProcessSerializedShaderMaps(this, LoadedTransformResources, ColorTransformResources);
	}
	else
	{
		// Discard all loaded material resources
		for (FOpenColorIOTransformResource& Resource : LoadedTransformResources)
		{
			Resource.DiscardShaderMap();
		}
	}

	//To be able to fetch OCIO data, make sure our config owner has been postloaded.
	if (ConfigurationOwner)
	{
		ConfigurationOwner->ConditionalPostLoad();
	}

	// Empty the list of loaded resources, we don't need it anymore
	LoadedTransformResources.Empty();

	CacheResourceTextures();
	CacheResourceShadersForRendering(false);
}

void UOpenColorIOColorTransform::BeginDestroy()
{
	Super::BeginDestroy();

	ReleaseFence.BeginFence();
}

bool UOpenColorIOColorTransform::IsReadyForFinishDestroy()
{
	bool bReady = Super::IsReadyForFinishDestroy();

	return bReady && ReleaseFence.IsFenceComplete();
}

void UOpenColorIOColorTransform::FinishDestroy()
{
	ReleaseResources();

	Super::FinishDestroy();
}

#if WITH_EDITOR

void UOpenColorIOColorTransform::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

	TArray<FOpenColorIOTransformResource*>* CachedColorTransformResourceForPlatformPtr = CachedColorTransformResourcesForCooking.Find(TargetPlatform);

	if (DesiredShaderFormats.Num() > 0 && CachedColorTransformResourceForPlatformPtr == nullptr)
	{
		CachedColorTransformResourcesForCooking.Add(TargetPlatform);
		CachedColorTransformResourceForPlatformPtr = CachedColorTransformResourcesForCooking.Find(TargetPlatform);

		check(CachedColorTransformResourceForPlatformPtr != nullptr);

		//Need to re-update shader data when cooking. They won't have been previously fetched.
		FString ShaderCodeHash;
		FString ShaderCode;
		if (UpdateShaderInfo(ShaderCodeHash, ShaderCode))
		{
			// Cache for all the shader formats that the cooking target requires
			for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
			{
				const EShaderPlatform LegacyShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);

				// Begin caching shaders for the target platform and store the FOpenColorIOTransformResource being compiled into CachedColorTransformResourcesForCooking
				CacheResourceShadersForCooking(LegacyShaderPlatform, ShaderCodeHash, ShaderCode, *CachedColorTransformResourceForPlatformPtr);
			}
		}
	}
}

bool UOpenColorIOColorTransform::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{
	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

	const TArray<FOpenColorIOTransformResource*>* CachedColorTransformResourcesForPlatform = CachedColorTransformResourcesForCooking.Find(TargetPlatform);

	if (CachedColorTransformResourcesForPlatform != nullptr) // this should always succeed if BeginCacheForCookedPlatformData is called first
	{
		for (const FOpenColorIOTransformResource* const& TransformResource : *CachedColorTransformResourcesForPlatform)
		{
			if (TransformResource->IsCompilationFinished() == false)
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

void UOpenColorIOColorTransform::ClearCachedCookedPlatformData(const ITargetPlatform *TargetPlatform)
{
	TArray<FOpenColorIOTransformResource*>* CachedColorTransformResourcesForPlatform = CachedColorTransformResourcesForCooking.Find(TargetPlatform);
	if (CachedColorTransformResourcesForPlatform != nullptr)
	{
		for (const FOpenColorIOTransformResource* const& TransformResource : *CachedColorTransformResourcesForPlatform)
		{
			delete TransformResource;
		}
	}
	CachedColorTransformResourcesForCooking.Remove(TargetPlatform);
}

void UOpenColorIOColorTransform::ClearAllCachedCookedPlatformData()
{
	for (auto It : CachedColorTransformResourcesForCooking)
	{
		TArray<FOpenColorIOTransformResource*>& CachedColorTransformResourcesForPlatform = It.Value;
		for (int32 CachedResourceIndex = 0; CachedResourceIndex < CachedColorTransformResourcesForPlatform.Num(); CachedResourceIndex++)
		{
			delete CachedColorTransformResourcesForPlatform[CachedResourceIndex];
		}
	}

	CachedColorTransformResourcesForCooking.Empty();
}

#endif //WITH_EDITOR

void UOpenColorIOColorTransform::ReleaseResources()
{
	for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
	{
		FOpenColorIOTransformResource*& CurrentResource = ColorTransformResources[FeatureLevelIndex];
		if (CurrentResource)
		{
			delete CurrentResource;
			CurrentResource = nullptr;
		}
	}

#if WITH_EDITOR
	if (!GExitPurge)
	{
		ClearAllCachedCookedPlatformData();
	}
#endif
}
