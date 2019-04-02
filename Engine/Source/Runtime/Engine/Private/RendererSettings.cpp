// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Engine/RendererSettings.h"
#include "PixelFormat.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "Misc/MessageDialog.h"
#include "UnrealEdMisc.h"
#include "Misc/ConfigCacheIni.h"

/** The editor object. */
extern UNREALED_API class UEditorEngine* GEditor;
#endif // #if WITH_EDITOR

#define LOCTEXT_NAMESPACE "RendererSettings"

namespace EAlphaChannelMode
{
	EAlphaChannelMode::Type FromInt(int32 InAlphaChannelMode)
	{
		return static_cast<EAlphaChannelMode::Type>(FMath::Clamp(InAlphaChannelMode, (int32)Disabled, (int32)AllowThroughTonemapper));
	}
}

namespace EDefaultBackBufferPixelFormat
{
	EPixelFormat Convert2PixelFormat(EDefaultBackBufferPixelFormat::Type InDefaultBackBufferPixelFormat)
	{
		const int32 ValidIndex = FMath::Clamp((int32)InDefaultBackBufferPixelFormat, 0, (int32)DBBPF_MAX - 1);
		static EPixelFormat SPixelFormat[] = { PF_B8G8R8A8, PF_B8G8R8A8, PF_FloatRGBA, PF_FloatRGBA, PF_A2B10G10R10 };
		return SPixelFormat[ValidIndex];
	}

	int32 NumberOfBitForAlpha(EDefaultBackBufferPixelFormat::Type InDefaultBackBufferPixelFormat)
	{
		switch (InDefaultBackBufferPixelFormat)
		{
			case DBBPF_A16B16G16R16_DEPRECATED:
			case DBBPF_B8G8R8A8:
			case DBBPF_FloatRGB_DEPRECATED:
			case DBBPF_FloatRGBA:
				return 8;
			case DBBPF_A2B10G10R10:
				return 2;
			default:
				return 0;
		}
		return 0;
	}

	EDefaultBackBufferPixelFormat::Type FromInt(int32 InDefaultBackBufferPixelFormat)
	{
		const int32 ValidIndex = FMath::Clamp(InDefaultBackBufferPixelFormat, 0, (int32)DBBPF_MAX - 1);
		static EDefaultBackBufferPixelFormat::Type SPixelFormat[] = { DBBPF_B8G8R8A8, DBBPF_B8G8R8A8, DBBPF_FloatRGBA, DBBPF_FloatRGBA, DBBPF_A2B10G10R10 };
		return SPixelFormat[ValidIndex];
	}
}

URendererSettings::URendererSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectionName = TEXT("Rendering");
	TranslucentSortAxis = FVector(0.0f, -1.0f, 0.0f);
	bSupportStationarySkylight = true;
	bSupportPointLightWholeSceneShadows = true;
	bSupportAtmosphericFog = true;
	bSupportSkinCacheShaders = false;
	bSupportMaterialLayers = false;
	GPUSimulationTextureSizeX = 1024;
	GPUSimulationTextureSizeY = 1024;
	bEnableRayTracing = 0;
}

void URendererSettings::PostInitProperties()
{
	Super::PostInitProperties();
	
	SanatizeReflectionCaptureResolution();

#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif // #if WITH_EDITOR
}

#if WITH_EDITOR
void URendererSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SanatizeReflectionCaptureResolution();

	if (PropertyChangedEvent.Property)
	{
		// round up GPU sim texture sizes to nearest power of two, and constrain to sensible values
		if ( PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URendererSettings, GPUSimulationTextureSizeX) 
			|| PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URendererSettings, GPUSimulationTextureSizeY) ) 
		{
			static const int32 MinGPUSimTextureSize = 32;
			static const int32 MaxGPUSimTextureSize = 8192;
			GPUSimulationTextureSizeX = FMath::RoundUpToPowerOfTwo( FMath::Clamp(GPUSimulationTextureSizeX, MinGPUSimTextureSize, MaxGPUSimTextureSize) );
			GPUSimulationTextureSizeY = FMath::RoundUpToPowerOfTwo( FMath::Clamp(GPUSimulationTextureSizeY, MinGPUSimTextureSize, MaxGPUSimTextureSize) );
		}

		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URendererSettings, bEnableRayTracing) 
			&& bEnableRayTracing 
			&& !bSupportSkinCacheShaders)
		{
			if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("Skin Cache Disabled", "Ray Tracing requires enabling skin cache. Do you want to automatically enable skin cache now?")) == EAppReturnType::Yes)
			{
				bSupportSkinCacheShaders = 1;

				for (TFieldIterator<UProperty> PropIt(GetClass()); PropIt; ++PropIt)
				{
					UProperty* Property = *PropIt;
					if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(URendererSettings, bSupportSkinCacheShaders))
					{
						UpdateSinglePropertyInConfigFile(Property, GetDefaultConfigFilename());
					}
				}
			}
			else
			{
				bEnableRayTracing = 0;

				for (TFieldIterator<UProperty> PropIt(GetClass()); PropIt; ++PropIt)
				{
					UProperty* Property = *PropIt;
					if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(URendererSettings, bEnableRayTracing))
					{
						UpdateSinglePropertyInConfigFile(Property, GetDefaultConfigFilename());
					}
				}
			}
		}

		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);

		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(URendererSettings, ReflectionCaptureResolution) && 
			PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
		{
			GEditor->BuildReflectionCaptures();
		}
	}
}

bool URendererSettings::CanEditChange(const UProperty* InProperty) const
{
	const bool ParentVal = Super::CanEditChange(InProperty);

	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(URendererSettings, bSupportSkinCacheShaders)))
	{
		//only allow DISABLE of skincache shaders if raytracing is also disabled as skincache is a dependency of raytracing.
		return ParentVal && (!bSupportSkinCacheShaders || !bEnableRayTracing);
	}

	return ParentVal;
}
#endif // #if WITH_EDITOR

void URendererSettings::SanatizeReflectionCaptureResolution()
{
	static const int32 MaxReflectionCaptureResolution = 1024;
	static const int32 MinReflectionCaptureResolution = 64;
	ReflectionCaptureResolution = FMath::Clamp(int32(FMath::RoundUpToPowerOfTwo(ReflectionCaptureResolution)), MinReflectionCaptureResolution, MaxReflectionCaptureResolution);
}

URendererOverrideSettings::URendererOverrideSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectionName = TEXT("Rendering Overrides");	
}

void URendererOverrideSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif // #if WITH_EDITOR
}

#if WITH_EDITOR
void URendererOverrideSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);	

	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);		
	}
}
#endif // #if WITH_EDITOR

#undef LOCTEXT_NAMESPACE
