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
#include "LightingTrackingComponent.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/TextureCube.h"
#include "RenderUtils.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/PostProcessComponent.h"
#if WITH_MLSDK
#include "ml_lighting_tracking.h"
#endif //WITH_MLSDK
#include <limits>

DEFINE_LOG_CATEGORY(LogLightingTracking);

#define MAX_NITS 15.0f

class LightingTrackingImpl
{
public:
	LightingTrackingImpl(ULightingTrackingComponent* InOwner)
		: Owner(InOwner)
#if WITH_MLSDK
		, Tracker(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
		, PostProcessor(nullptr)
		, AmbientCubeMap(nullptr)
		, LastAmbientIntensityTimeStamp(0)
		, LastAmbientCubeMapTimeStamp(0)
	{
#if WITH_MLSDK
		MLResult Result = MLLightingTrackingCreate(&Tracker);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogLightingTracking, Error, TEXT("MLLightingTrackingCreate failed with error %d"), Result);
		}
#endif //WITH_MLSDK

		for (TActorIterator<AActor> ActorItr(Owner->GetWorld()); ActorItr; ++ActorItr)
		{
			PostProcessor = (*ActorItr)->FindComponentByClass<UPostProcessComponent>();
			if (PostProcessor)
			{
				break;
			}
		}

		if (!PostProcessor)
		{
			PostProcessor = NewObject<UPostProcessComponent>(Owner->GetOwner());
			PostProcessor->RegisterComponent();
		}

		PostProcessor->bUnbound = true;
		PostProcessor->Settings.bOverride_AmbientCubemapIntensity = Owner->UseGlobalAmbience;
		PostProcessor->Settings.bOverride_WhiteTemp = Owner->UseColorTemp;

		/*const int32 MinDim = static_cast<int32>(FMath::Min(MLLightingTracking_GridWidth, MLLightingTracking_GridHeight));
		const int32 NumCubeMapTextures = 6;

		AmbientCubeMap = NewObject<UTextureCube>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient
		);
		AmbientCubeMap->AddToRoot();
		AmbientCubeMap->PlatformData = new FTexturePlatformData();
		AmbientCubeMap->PlatformData->SizeX = MinDim * NumCubeMapTextures;
		AmbientCubeMap->PlatformData->SizeY = MinDim;
		AmbientCubeMap->PlatformData->PixelFormat = EPixelFormat::PF_FloatRGBA;

		// Allocate first mipmap.
		int32 NumBlocksX = (MinDim * NumCubeMapTextures) / GPixelFormats[EPixelFormat::PF_FloatRGBA].BlockSizeX;
		int32 NumBlocksY = MinDim / GPixelFormats[EPixelFormat::PF_FloatRGBA].BlockSizeY;
		FTexture2DMipMap* Mip = new FTexture2DMipMap();// (AmbientCubeMap->PlatformData->Mips) FTexture2DMipMap();
		Mip->SizeX = MinDim * 6;
		Mip->SizeY = MinDim;
		Mip->BulkData.Lock(LOCK_READ_WRITE);
		uint16* Data = static_cast<uint16*>(Mip->BulkData.Realloc(NumBlocksX * NumBlocksY * GPixelFormats[EPixelFormat::PF_FloatRGBA].BlockBytes));
		for (int32 i = 0; i < Mip->BulkData.GetBulkDataSize() / 2; i+=2, ++Data)
		{
			*((FFloat16*)Data) = 1.0f;
		}
		Mip->BulkData.Unlock();
		AmbientCubeMap->PlatformData->Mips.Add(Mip);
		AmbientCubeMap->UpdateResource();

		PostProcessor->Settings.AmbientCubemap = AmbientCubeMap;*/
	}

	void RefreshGlobalAmbience()
	{
#if WITH_MLSDK
		PostProcessor->Settings.bOverride_AmbientCubemapIntensity = Owner->UseGlobalAmbience;

		if (!Owner->UseGlobalAmbience)
		{
			return;
		}

		MLLightingTrackingAmbientGlobalState AmbientGlobalState;
		MLResult Result = MLLightingTrackingGetAmbientGlobalState(Tracker, &AmbientGlobalState);
		if (Result == MLResult_Ok)
		{
			if (AmbientGlobalState.timestamp_ns > LastAmbientIntensityTimeStamp)
			{
				LastAmbientIntensityTimeStamp = AmbientGlobalState.timestamp_ns;
				uint16 LuminanceSum = 0;
				for (uint16 Index = 0; Index < (uint16)MLLightingTrackingCamera_Count; ++Index)
				{
					LuminanceSum += AmbientGlobalState.als_global[Index];
				}
				checkf((uint16)MLLightingTrackingCamera_Count > 0, TEXT("Invalid value for MLLightingTrackingCamera_Count!"));
				PostProcessor->Settings.AmbientCubemapIntensity = (float)(LuminanceSum / MLLightingTrackingCamera_Count) / MAX_NITS;// (float)std::numeric_limits<uint16>::max();
			}
		}
		else
		{
			UE_LOG(LogLightingTracking, Error, TEXT("MLLightingTrackingGetAmbientGlobalState failed with error %d"), Result);
		}
#endif //WITH_MLSDK
	}

	void RefreshColorTemperature()
	{
#if WITH_MLSDK
		PostProcessor->Settings.bOverride_WhiteTemp = Owner->UseColorTemp;

		if (!Owner->UseColorTemp)
		{
			return;
		}

		MLLightingTrackingColorTemperatureState ColorTemperatureState;
		MLResult Result = MLLightingTrackingGetColorTemperatureState(Tracker, &ColorTemperatureState);
		if (Result == MLResult_Ok)
		{
			PostProcessor->Settings.WhiteTemp = (float)ColorTemperatureState.color_temp;
		}
		else
		{
			UE_LOG(LogLightingTracking, Error, TEXT("MLLightingTrackingGetColorTemperatureState failed with error %d"), Result);
		}
#endif //WITH_MLSDK
	}

	void RefreshAmbientCubeMap()
	{
		/*if (!AmbientCubeMap)
		{
			UE_LOG(LogLightingTracking, Error, TEXT("AmbientCubeMap has not been set!"));
		}

		MLLightingTrackingAmbientGridState AmbientGridState;
		MLResult Result = MLLightingTrackingGetAmbientGridState(Tracker, &AmbientGridState);
		if (Result == MLResult_Ok)
		{
			if (AmbientGridState.timestamp_ns > LastAmbientCubeMapTimeStamp)
			{
				LastAmbientCubeMapTimeStamp = AmbientGridState.timestamp_ns;
				ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER
				(
					RefreshAmbientMapCommand,
					UTextureCube*,
					_AmbientCubeMap,
					AmbientCubeMap,
					MLLightingTrackingAmbientGridState,
					_AmbientGridState,
					AmbientGridState,
					LightingTrackingImpl*,
					_LightingTrackingImpl,
					this,
					{
						const uint32 TextureSizeInBytes = MLLightingTracking_GridHeight * MLLightingTracking_GridHeight * sizeof(FFloat16Color);
						FTexture2DMipMap& Mip = _AmbientCubeMap->PlatformData->Mips[0];
						void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
						_LightingTrackingImpl->CalcRight(_AmbientGridState.als_grid[MLLightingTrackingCamera_FarRight], static_cast<FFloat16Color*>(Data));
						Data += TextureSizeInBytes;
						_LightingTrackingImpl->CalcLeft(_AmbientGridState.als_grid[MLLightingTrackingCamera_FarLeft], static_cast<FFloat16Color*>(Data));
						Data += TextureSizeInBytes;
						_LightingTrackingImpl->CalcFront(_AmbientGridState.als_grid[MLLightingTrackingCamera_Left], static_cast<FFloat16Color*>(Data));
						Data += TextureSizeInBytes;
						_LightingTrackingImpl->CalcBack(_AmbientGridState.als_grid[MLLightingTrackingCamera_Left], static_cast<FFloat16Color*>(Data));
						Data += TextureSizeInBytes;
						_LightingTrackingImpl->CalcUp(_AmbientGridState.als_grid[MLLightingTrackingCamera_Left], static_cast<FFloat16Color*>(Data));
						Data += TextureSizeInBytes;
						_LightingTrackingImpl->CalcDown(_AmbientGridState.als_grid[MLLightingTrackingCamera_Left], static_cast<FFloat16Color*>(Data));
						Mip.BulkData.Unlock();
					}
				);

				AmbientCubeMap->UpdateResource();
			}
		}
		else
		{
			UE_LOG(LogLightingTracking, Error, TEXT("MLLightingTrackingGetColorTemperatureState failed with error %d"), Result);
		}*/
	}
#if WITH_MLSDK
	void CalcRight(uint16_t InLumins[MLLightingTracking_GridHeight][MLLightingTracking_GridWidth], FFloat16Color* OutColors)
	{
		uint32 ColorCount = 0;
		for (int32 YIndex = 0; YIndex < MLLightingTracking_GridHeight; ++YIndex)
		{
			for (int32 XIndex = MLLightingTracking_GridHeight; XIndex > 0; --XIndex)
			{
				const uint16 Lumins = InLumins[YIndex][XIndex];
				FFloat16Color* Color = &OutColors[ColorCount++];
				Color->R = Lumins;
				Color->G = Lumins;
				Color->B = Lumins;
				Color->A = 1.0f;
			}
		}
	}

	void CalcLeft(uint16_t InLumins[MLLightingTracking_GridHeight][MLLightingTracking_GridWidth], FFloat16Color* OutColors)
	{
		uint32 ColorCount = 0;
		for (int32 XIndex = 1; XIndex < MLLightingTracking_GridHeight; ++XIndex)
		{
			for (int32 YIndex = MLLightingTracking_GridHeight - 1; YIndex > -1; --YIndex)
			{
				const uint16 Lumins = InLumins[YIndex][XIndex];
				FFloat16Color* Color = &OutColors[ColorCount++];
				Color->R = Lumins;
				Color->G = Lumins;
				Color->B = Lumins;
				Color->A = 1.0f;
			}
		}
	}

	void CalcFront(uint16 InLumins[MLLightingTracking_GridHeight][MLLightingTracking_GridWidth], FFloat16Color* OutColors)
	{
		uint32 ColorCount = 0;
		for (int32 YIndex = MLLightingTracking_GridHeight - 1; YIndex > -1; --YIndex)
		{
			for (int32 XIndex = MLLightingTracking_GridWidth - 2; XIndex > 0; ++XIndex)
			{
				const uint16 Lumins = InLumins[YIndex][XIndex];
				FFloat16Color* Color = &OutColors[ColorCount++];
				Color->R = Lumins;
				Color->G = Lumins;
				Color->B = Lumins;
				Color->A = 1.0f;
			}
		}
	}

	void CalcBack(uint16 InLumins[MLLightingTracking_GridHeight][MLLightingTracking_GridWidth], FFloat16Color* OutColors)
	{
		(void)InLumins;
		for (uint32 Index = 0; Index < MLLightingTracking_GridHeight * MLLightingTracking_GridHeight; ++Index)
		{
			FFloat16Color* Color = &OutColors[Index];
			Color->R = 0.5f;
			Color->G = 0.5f;
			Color->B = 0.5f;
			Color->A = 1.0f;
		}
	}

	void CalcUp(uint16 InLumins[MLLightingTracking_GridHeight][MLLightingTracking_GridWidth], FFloat16Color* OutColors)
	{
		(void)InLumins;
		for (uint32 Index = 0; Index < MLLightingTracking_GridHeight * MLLightingTracking_GridHeight; ++Index)
		{
			FFloat16Color* Color = &OutColors[Index];
			Color->R = 0.5f;
			Color->G = 0.5f;
			Color->B = 0.5f;
			Color->A = 1.0f;
		}
	}

	void CalcDown(uint16 InLumins[MLLightingTracking_GridHeight][MLLightingTracking_GridWidth], FFloat16Color* OutColors)
	{
		(void)InLumins;
		for (uint32 Index = 0; Index < MLLightingTracking_GridHeight * MLLightingTracking_GridHeight; ++Index)
		{
			FFloat16Color* Color = &OutColors[Index];
			Color->R = 0.5f;
			Color->G = 0.5f;
			Color->B = 0.5f;
			Color->A = 1.0f;
		}
	}

	MLHandle Tracker;
#endif //WITH_MLSDK
	ULightingTrackingComponent* Owner;
	UPostProcessComponent* PostProcessor;
	UTextureCube* AmbientCubeMap;
	uint64 LastAmbientIntensityTimeStamp;
	uint64 LastAmbientCubeMapTimeStamp;
};

ULightingTrackingComponent::ULightingTrackingComponent()
	: UseGlobalAmbience(false)
	, UseColorTemp(false)
	, Impl(nullptr)
	//, UseDynamicAmbientCubeMap(false)
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = true;
}

void ULightingTrackingComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	Impl = new LightingTrackingImpl(this);
}

void ULightingTrackingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UActorComponent::EndPlay(EndPlayReason);
#if WITH_MLSDK
	if (MLHandleIsValid(Impl->Tracker))
	{
		MLResult Result = MLLightingTrackingDestroy(Impl->Tracker);
		if (Result != MLResult_Ok)
		{
			UE_LOG(LogLightingTracking, Error, TEXT("MLLightingTrackingDestroy failed with error %d"), Result);
		}
	}
#endif //WITH_MLSDK
	delete Impl;
	Impl = nullptr;
}

void ULightingTrackingComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Impl->RefreshGlobalAmbience();

	Impl->RefreshColorTemperature();

	//Impl->RefreshAmbientCubeMap();
}