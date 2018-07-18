// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaBundle.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "UObject/ConstructorHelpers.h"
#endif

#include "Engine/TextureRenderTarget2D.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "MediaSource.h"
#include "MediaTexture.h"


/* UMediaBundle
 *****************************************************************************/
UMediaBundle::UMediaBundle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR && WITH_EDITORONLY_DATA
	static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterialFinder(TEXT("/MediaFrameworkUtilities/M_DefaultMedia"));
	static ConstructorHelpers::FObjectFinder<UTexture> DefaultFailedTextureFinder(TEXT("/MediaFrameworkUtilities/T_VideoInputFailed"));
	static ConstructorHelpers::FClassFinder<AMediaBundleActorBase> DefaultActorClassFinder(TEXT("/MediaFrameworkUtilities/BP_MediaBundle_Plane_16-9"));

	DefaultMaterial = DefaultMaterialFinder.Object;
	DefaultFailedTexture = DefaultFailedTextureFinder.Object;
	DefaultActorClass = DefaultActorClassFinder.Class;
#endif //WITH_EDITOR && WITH_EDITORONLY_DATA
}

bool UMediaBundle::OpenMediaSource()
{
	bool bResult = false;
	if (MediaSource && MediaPlayer)
	{
		bResult = true;

		// Only play once
		const EMediaState MediaState = MediaPlayer->GetPlayerFacade()->GetPlayer().IsValid() ? MediaPlayer->GetPlayerFacade()->GetPlayer()->GetControls().GetState() : EMediaState::Closed;
		if (MediaState == EMediaState::Closed || MediaState == EMediaState::Error)
		{
			bResult = MediaPlayer->OpenSource(MediaSource);
			MediaPlayer->SetLooping(bLoopMediaSource);
		}

		if (bResult)
		{
			MediaPlayer->OnMediaClosed.AddUniqueDynamic(this, &UMediaBundle::OnMediaClosed);
			MediaPlayer->OnMediaOpened.AddUniqueDynamic(this, &UMediaBundle::OnMediaOpenOpened);
			MediaPlayer->OnMediaOpenFailed.AddUniqueDynamic(this, &UMediaBundle::OnMediaOpenFailed);
			++ReferenceCount;
		}
	}
	return bResult;
}

void UMediaBundle::CloseMediaSource()
{
	--ReferenceCount;
	if (ReferenceCount == 0 && MediaPlayer)
	{
		MediaPlayer->Close();
	}
}

void UMediaBundle::SetIsValidMaterialParameter(bool bIsValid)
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Material))
		{
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(MediaBundleMaterialParametersName::IsValidMediaName), bIsValid ? 1.0f : 0.0f);
			Instance->PostEditChange();
		}
	}
#endif
}

void UMediaBundle::OnMediaClosed()
{
	const EMediaState MediaState = MediaPlayer->GetPlayerFacade()->GetPlayer().IsValid() ? MediaPlayer->GetPlayerFacade()->GetPlayer()->GetControls().GetState() : EMediaState::Closed;
	if (MediaState == EMediaState::Closed || MediaState == EMediaState::Error)
	{
		SetIsValidMaterialParameter(false);
	}
}

void UMediaBundle::OnMediaOpenOpened(FString DeviceUrl)
{
	SetIsValidMaterialParameter(true);
}

void UMediaBundle::OnMediaOpenFailed(FString DeviceUrl)
{
	SetIsValidMaterialParameter(false);
}

void UMediaBundle::RefreshLensDisplacementMap()
{
	if (FApp::CanEverRender())
	{
		if (LensDisplacementMap)
		{
			CurrentLensParameters = LensParameters;

			UTexture2D* PreComputed = CurrentLensParameters.CreateUndistortUVDisplacementMap(FIntPoint(256, 256), 0.0f, UndistortedCameraViewInfo);

			if (PreComputed != nullptr)
			{
				UWorld* World = GetWorld();
				if (World == nullptr)
				{
#if WITH_EDITOR
					World = GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
#endif
				}

				if (World != nullptr)
				{
					FOpenCVLensDistortionParameters::DrawDisplacementMapToRenderTarget(World, LensDisplacementMap, PreComputed);
				}
			}
		}
	}
}

void UMediaBundle::CreateInternalsEditor()
{
#if WITH_EDITOR && WITH_EDITORONLY_DATA
	if (GIsEditor)
	{
		IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		const FString ParentName = GetOuter()->GetName() + "_InnerAssets/";
		FString OutAssetName;
		FString OutPackageName;

		//Create MediaPlayer
		AssetTools.CreateUniqueAssetName(*(ParentName + TEXT("/MediaP_") + GetName()), TEXT(""), OutPackageName, OutAssetName);
		MediaPlayer = Cast<UMediaPlayer>(AssetTools.CreateAsset(OutAssetName, ParentName, UMediaPlayer::StaticClass(), nullptr));
		MediaPlayer->AffectedByPIEHandling = false;

		//Create MediaTexture 
		AssetTools.CreateUniqueAssetName(*(ParentName + TEXT("/T_") + GetName() + TEXT("_BC")), TEXT(""), OutPackageName, OutAssetName);
		MediaTexture = Cast<UMediaTexture>(AssetTools.CreateAsset(OutAssetName, ParentName, UMediaTexture::StaticClass(), nullptr));
		MediaTexture->SetDefaultMediaPlayer(MediaPlayer);
		MediaTexture->SetMediaPlayer(MediaPlayer);
		MediaTexture->UpdateResource();

		//Create LensDisplacementMap RenderTarget
		AssetTools.CreateUniqueAssetName(*(ParentName + TEXT("/RT_") + GetName() + TEXT("_LensDisplacement")), TEXT(""), OutPackageName, OutAssetName);
		LensDisplacementMap = Cast<UTextureRenderTarget2D>(AssetTools.CreateAsset(OutAssetName, ParentName, UTextureRenderTarget2D::StaticClass(), nullptr));
		LensDisplacementMap->RenderTargetFormat = RTF_RGBA16f;
		LensDisplacementMap->InitAutoFormat(256, 256);
		LensDisplacementMap->UpdateResource();

		//Create MaterialInstanceConstant
		UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
		Factory->InitialParent = DefaultMaterial;

		AssetTools.CreateUniqueAssetName(*(ParentName + TEXT("/MI_") + GetName()), TEXT(""), OutPackageName, OutAssetName);
		UMaterialInstanceConstant* NewMaterial = Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(OutAssetName, ParentName, UMaterialInstanceConstant::StaticClass(), Factory));
		NewMaterial->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(MediaBundleMaterialParametersName::MediaTextureName), MediaTexture);
		NewMaterial->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(MediaBundleMaterialParametersName::FailedTextureName), DefaultFailedTexture);
		NewMaterial->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(MediaBundleMaterialParametersName::LensDisplacementMapTextureName), LensDisplacementMap);
		NewMaterial->PostEditChange();
		Material = NewMaterial;

		//If we are creating a new object, set the default actor class. Duplicates won't change this.
		if (MediaBundleActorClass == nullptr)
		{
			MediaBundleActorClass = DefaultActorClass;
		}
	}
#endif // WITH_EDITOR && WITH_EDITORONLY_DATA
}

void UMediaBundle::PostLoad()
{
	Super::PostLoad();

	if (LensDisplacementMap != nullptr)
	{
		//Handle DisplacementMap PostLoad on our own to avoid our texture being reset
		LensDisplacementMap->ConditionalPostLoad();

		if (!HasAnyFlags(RF_ClassDefaultObject))
		{
			//No need to clear render target. We will generate it right after.
			const bool bClearRenderTarget = false;
			LensDisplacementMap->UpdateResourceImmediate(bClearRenderTarget);

			RefreshLensDisplacementMap();
		}
	}
}

void UMediaBundle::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	CreateInternalsEditor();
}

#if WITH_EDITOR
void UMediaBundle::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMediaBundle, MediaSource))
	{
		if (MediaPlayer)
		{
			MediaPlayer->Close();
			if (MediaSource && ReferenceCount > 0)
			{
				MediaPlayer->OpenSource(MediaSource);
			}
		}
	}
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMediaBundle, bLoopMediaSource))
	{
		if (MediaPlayer)
		{
			MediaPlayer->SetLooping(bLoopMediaSource);
			const EMediaState MediaState = MediaPlayer->GetPlayerFacade()->GetPlayer().IsValid() ? MediaPlayer->GetPlayerFacade()->GetPlayer()->GetControls().GetState() : EMediaState::Closed;
			if (MediaState == EMediaState::Stopped && ReferenceCount > 0)
			{
				MediaPlayer->OpenSource(MediaSource);
			}
		}
	}
	else if (CurrentLensParameters != LensParameters)
	{
		//Usage of internal current value to be able to regenerate displacement map
		//after undo/redo of a lens parameter
		RefreshLensDisplacementMap();
	}
}
#endif
