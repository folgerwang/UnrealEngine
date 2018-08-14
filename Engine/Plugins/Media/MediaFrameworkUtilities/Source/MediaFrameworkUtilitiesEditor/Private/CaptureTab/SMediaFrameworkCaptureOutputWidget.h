// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "Camera/CameraActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "MediaCapture.h"
#include "MediaOutput.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UObject/StrongObjectPtr.h"

class FLevelEditorViewportClient;
class FSceneViewport;
struct FSlateBrush;
class SViewport;
class SMediaFrameworkCapture;

namespace MediaFrameworkUtilities
{
	class FMediaFrameworkCaptureLevelEditorViewportClient;
}

/*
 * SMediaFrameworkCaptureOutputWidget definition
 */
class SMediaFrameworkCaptureOutputWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMediaFrameworkCaptureOutputWidget) {}
	SLATE_ARGUMENT(TWeakPtr<SMediaFrameworkCapture>, Owner)
	SLATE_ARGUMENT(TWeakObjectPtr<UMediaOutput>, MediaOutput)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMediaFrameworkCaptureOutputWidget();

	virtual void StopOutput();

	TSharedRef<SWidget> BuildBaseWidget(TSharedRef<SWidget> InnerWidget);

	FSlateColor HandleIconColorAndOpacity() const;
	const FSlateBrush* HandleIconImage() const;
	EVisibility HandleThrobberVisibility() const;
	virtual bool IsValid() const;

	TWeakPtr<SMediaFrameworkCapture> Owner;
	TWeakObjectPtr<UMediaOutput> MediaOutput;

	TStrongObjectPtr<UMediaCapture> MediaCapture;
};

/*
 * SMediaFrameworkCaptureCameraViewportWidget definition
 */
class SMediaFrameworkCaptureCameraViewportWidget : public SMediaFrameworkCaptureOutputWidget
{
private:
	using Super = SMediaFrameworkCaptureOutputWidget;

public:
	SLATE_BEGIN_ARGS(SMediaFrameworkCaptureCameraViewportWidget) { }
	SLATE_ARGUMENT(TWeakPtr<SMediaFrameworkCapture>, Owner)
	SLATE_ARGUMENT(TWeakObjectPtr<UMediaOutput>, MediaOutput)
	SLATE_ARGUMENT(TArray<TWeakObjectPtr<AActor>>, PreviewActors)
	SLATE_ARGUMENT(EViewModeIndex, ViewMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SMediaFrameworkCaptureCameraViewportWidget();

	void StartOutput();

private:
	void OnPostPIEStarted(bool bWasSimulatingInEditor);
	void OnPrePIEEnded(bool bWasSimulatingInEditor);

	void SetActorLock(int32 CameraIndex);
	void UpdateActivePreviewList(bool bIsPIE);

	TSharedPtr<MediaFrameworkUtilities::FMediaFrameworkCaptureLevelEditorViewportClient> LevelViewportClient;
	TSharedPtr<SViewport> ViewportWidget;
	TSharedPtr<FSceneViewport> SceneViewport;

	TArray<TWeakObjectPtr<AActor>> PreviewActors;
	TArray<TWeakObjectPtr<AActor>> ActivePreviewActors;
	EViewModeIndex ViewMode;
	int32 CurrentLockCameraIndex;
};

/*
 * SMediaFrameworkCaptureRenderTargetWidget definition
 */
class SMediaFrameworkCaptureRenderTargetWidget : public SMediaFrameworkCaptureOutputWidget
{
private:
	using Super = SMediaFrameworkCaptureOutputWidget;

public:
	SLATE_BEGIN_ARGS(SMediaFrameworkCaptureRenderTargetWidget) {}
	SLATE_ARGUMENT(TWeakPtr<SMediaFrameworkCapture>, Owner)
	SLATE_ARGUMENT(TWeakObjectPtr<UMediaOutput>, MediaOutput)
	SLATE_ARGUMENT(TWeakObjectPtr<UTextureRenderTarget2D>, RenderTarget)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMediaFrameworkCaptureRenderTargetWidget();

	void StartOutput();

	virtual bool IsValid() const override;

private:
	TWeakObjectPtr<UTextureRenderTarget2D> RenderTarget;
	TStrongObjectPtr<UMaterial> ImageMaterial;
	TSharedPtr<FSlateBrush> ImageMaterialBrush;
};
