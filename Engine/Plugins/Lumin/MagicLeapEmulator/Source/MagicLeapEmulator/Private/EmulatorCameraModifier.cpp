// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "EmulatorCameraModifier.h"
#include "Camera/PlayerCameraManager.h"
#include "MagicLeapEmulatorSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"

UEmulatorCameraModifier::UEmulatorCameraModifier()
{
	// do this last
	Priority = 255;
}

/** Internal helper */
template<class T>
T* GetObjectFromStringAsset(FStringAssetReference const& AssetRef)
{
	UObject* AlreadyLoadedObj = AssetRef.ResolveObject();
	if (AlreadyLoadedObj)
	{
		return Cast<T>(AlreadyLoadedObj);
	}

	UObject* NewlyLoadedObj = AssetRef.TryLoad();
	if (NewlyLoadedObj)
	{
		return Cast<T>(NewlyLoadedObj);
	}

	return nullptr;
}

static const FName NAME_CapturedTex_LeftOrFull(TEXT("CapturedTex_LeftOrFull"));
static const FName NAME_CapturedTex_Right(TEXT("CapturedTex_Right"));

void UEmulatorCameraModifier::InitForEmulation(UTextureRenderTarget2D* BGRenderTarget_LeftOrFull, UTextureRenderTarget2D* BGRenderTarget_Right)
{
	UMagicLeapEmulatorSettings const* const Settings = GetDefault<UMagicLeapEmulatorSettings>();
	if (Settings)
	{
		// set up post process material to composite foreground additively onto background
		UMaterialInterface* const CompositingMat = GetObjectFromStringAsset<UMaterialInterface>(Settings->EmulatorCompositingMaterial);
		if (CompositingMat)
		{
			CompositingMatInst = UMaterialInstanceDynamic::Create(CompositingMat, this);
			if (CompositingMatInst)
			{
				CompositingMatInst->SetTextureParameterValue(NAME_CapturedTex_LeftOrFull, BGRenderTarget_LeftOrFull);
				CompositingMatInst->SetTextureParameterValue(NAME_CapturedTex_Right, BGRenderTarget_Right);
				EmulatorPPSettings.AddBlendable(CompositingMatInst, 1.f);
			}
		}
	}
}

bool UEmulatorCameraModifier::ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);

	if (CameraOwner)
	{
		CameraOwner->AddCachedPPBlend(EmulatorPPSettings, 1.f);
	}

	return false;
}
