// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Engine/EngineTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"


class IDetailsView;
class SMediaFrameworkCaptureCameraViewportWidget;
class SMediaFrameworkCaptureRenderTargetWidget;
class UMediaFrameworkWorldSettingsAssetUserData;

namespace MediaFrameworkUtilities
{
	class SCaptureVerticalBox;
}

/*
 * SMediaFrameworkCapture
 */
class SMediaFrameworkCapture : public SCompoundWidget
{
public:
	static void RegisterNomadTabSpawner();
	static void UnregisterNomadTabSpawner();

public:
	SLATE_BEGIN_ARGS(SMediaFrameworkCapture){}
	SLATE_END_ARGS()

	~SMediaFrameworkCapture();

	void Construct(const FArguments& InArgs);

private:
	void OnMapChange(uint32 InMapFlags);
	void OnLevelActorsRemoved(AActor* InActor);
	void OnAssetsDeleted(const TArray<UClass*>& DeletedAssetClasses);
	void OnObjectPreEditChange(UObject* Object, const FEditPropertyChain& PropertyChain);

	bool CanEnableViewport() const;

	TSharedRef<class SWidget> MakeToolBar();
	void EnabledCapture(bool bEnabled);
	UMediaFrameworkWorldSettingsAssetUserData* FindMediaFrameworkAssetUserData() const;
	UMediaFrameworkWorldSettingsAssetUserData* FindOrAddMediaFrameworkAssetUserData();

	TSharedPtr<IDetailsView> DetailView;
	TSharedPtr<MediaFrameworkUtilities::SCaptureVerticalBox> CaptureBoxes;
	bool bIsCapturing;

	TArray<TSharedPtr<SMediaFrameworkCaptureCameraViewportWidget>> CaptureCameraViewports;
	TArray<TSharedPtr<SMediaFrameworkCaptureRenderTargetWidget>> CaptureRenderTargets;
};
