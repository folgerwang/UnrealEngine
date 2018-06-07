// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaBundleFactoryNew.h"

#include "AssetTypeCategories.h"
#include "Engine/Blueprint.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "MediaBundle.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "MediaBundleFactoryNew"


/* UMediaBundleFactoryNew structors
 *****************************************************************************/

UMediaBundleFactoryNew::UMediaBundleFactoryNew( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterialFinder(TEXT("/MediaFrameworkUtilities/M_DefaultMedia"));
	static ConstructorHelpers::FClassFinder<AMediaBundleActorBase> DefaultActorClassFinder(TEXT("/MediaFrameworkUtilities/BP_MediaBundle_Plane_16-9"));

	DefaultMaterial = DefaultMaterialFinder.Object;
	DefaultActorClass = DefaultActorClassFinder.Class;

	SupportedClass = UMediaBundle::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UMediaBundleFactoryNew UFactory interface
 *****************************************************************************/

UObject* UMediaBundleFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UMediaBundle* NewMediaBundle = NewObject<UMediaBundle>(InParent, InClass, InName, Flags);

	NewMediaBundle->MediaPlayer = NewObject<UMediaPlayer>(NewMediaBundle, UMediaPlayer::StaticClass(), *(TEXT("MediaP_") + InName.ToString()), Flags);
	NewMediaBundle->MediaPlayer->AffectedByPIEHandling = false;

	NewMediaBundle->MediaTexture = NewObject<UMediaTexture>(NewMediaBundle, UMediaTexture::StaticClass(), *(+TEXT("T_") + InName.ToString() + TEXT("_BC")), Flags);
	NewMediaBundle->MediaTexture->SetDefaultMediaPlayer(NewMediaBundle->MediaPlayer);

	UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
	Factory->InitialParent = DefaultMaterial;
	UMaterialInstanceConstant* NewMaterial = Cast<UMaterialInstanceConstant>(Factory->FactoryCreateNew(UMaterialInstanceConstant::StaticClass(), NewMediaBundle, *(TEXT("MI_") + InName.ToString()), Flags, Context, Warn));
	NewMaterial->SetTextureParameterValueEditorOnly(FMaterialParameterInfo("MediaTexture"), NewMediaBundle->MediaTexture);
	NewMaterial->PostEditChange();

	NewMediaBundle->MediaBundleActorClass = DefaultActorClass;

	NewMediaBundle->Material = NewMaterial;

	return NewMediaBundle;
}

uint32 UMediaBundleFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}

bool UMediaBundleFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}

/* UActorFactoryMediaBundle structors
*****************************************************************************/

UActorFactoryMediaBundle::UActorFactoryMediaBundle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("FactoryMediaBundleDisplayName", "Media Bundle Actor");
	NewActorClass = AMediaBundleActorBase::StaticClass();
	bUseSurfaceOrientation = false;
}


/* UActorFactoryMediaBundle UFactory interface
*****************************************************************************/
bool UActorFactoryMediaBundle::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() || !AssetData.GetClass()->IsChildOf(UMediaBundle::StaticClass()))
	{
		OutErrorMsg = LOCTEXT("NoMediaBundle", "A valid Media Bundle must be specified.");
		return false;
	}

	return true;
}

void UActorFactoryMediaBundle::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	UMediaBundle* MediaBundle = CastChecked<UMediaBundle>(Asset);
	AMediaBundleActorBase* MediaBundleActor = CastChecked<AMediaBundleActorBase>(NewActor);

	UProperty* MediaBundleProperty = FindFieldChecked<UProperty>(AMediaBundleActorBase::StaticClass(), "MediaBundle");
	FEditPropertyChain PropertyChain;
	PropertyChain.AddHead(MediaBundleProperty);
	static_cast<UObject*>(NewActor)->PreEditChange(PropertyChain);

	MediaBundleActor->MediaBundle = MediaBundle;

	FPropertyChangedEvent PropertyEvent(MediaBundleProperty);
	NewActor->PostEditChangeProperty(PropertyEvent);
}

UObject* UActorFactoryMediaBundle::GetAssetFromActorInstance(AActor* ActorInstance)
{
	check(ActorInstance->IsA(NewActorClass));
	AMediaBundleActorBase* MBA = CastChecked<AMediaBundleActorBase>(ActorInstance);

	return MBA->GetMediaBundle();
}

AActor* UActorFactoryMediaBundle::GetDefaultActor(const FAssetData& AssetData)
{
	if (UMediaBundle* Bundle = Cast<UMediaBundle>(AssetData.GetAsset()))
	{
		if (UClass* ActorClass = Bundle->MediaBundleActorClass.Get())
		{
			return ActorClass->GetDefaultObject<AActor>();
		}
	}
	return Super::GetDefaultActor(AssetData);
}

#undef LOCTEXT_NAMESPACE
