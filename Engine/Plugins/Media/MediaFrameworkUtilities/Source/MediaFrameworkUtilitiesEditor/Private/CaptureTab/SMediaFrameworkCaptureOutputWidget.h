// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Engine/EngineTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "Camera/CameraActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MediaCapture.h"
#include "MediaOutput.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UObject/StrongObjectPtr.h"

class FLevelEditorViewportClient;
class FSceneViewport;
struct FSlateBrush;
class SViewport;
class SMediaFrameworkCapture;

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

	TSharedRef<SWidget> BuildBaseWidget(TSharedRef<SWidget> InnerWidget);

	FLinearColor GetProgressColor() const;
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
public:

	SLATE_BEGIN_ARGS(SMediaFrameworkCaptureCameraViewportWidget) { }
	SLATE_ARGUMENT(TWeakPtr<SMediaFrameworkCapture>, Owner)
	SLATE_ARGUMENT(TWeakObjectPtr<UMediaOutput>, MediaOutput)
	SLATE_ARGUMENT(TArray<TWeakObjectPtr<ACameraActor>>, PreviewActors)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void StartOutput();
	virtual ~SMediaFrameworkCaptureCameraViewportWidget();

	void SetActorLock(int32 CameraIndex);

	TSharedPtr<FLevelEditorViewportClient> LevelViewportClient;
	TSharedPtr<SViewport> ViewportWidget;
	TSharedPtr<FSceneViewport> SceneViewport;

	TArray<TWeakObjectPtr<ACameraActor>> PreviewActors;

	int32 CurrentLockCameraIndex;
};

/*
 * SMediaFrameworkCaptureRenderTargetWidget definition
 */
class SMediaFrameworkCaptureRenderTargetWidget : public SMediaFrameworkCaptureOutputWidget
{
public:
	SLATE_BEGIN_ARGS(SMediaFrameworkCaptureRenderTargetWidget) {}
	SLATE_ARGUMENT(TWeakPtr<SMediaFrameworkCapture>, Owner)
	SLATE_ARGUMENT(TWeakObjectPtr<UMediaOutput>, MediaOutput)
	SLATE_ARGUMENT(TWeakObjectPtr<UTextureRenderTarget2D>, RenderTarget)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void StartOutput();
	virtual ~SMediaFrameworkCaptureRenderTargetWidget();

	virtual bool IsValid() const override;

	TWeakObjectPtr<UTextureRenderTarget2D> RenderTarget;
	TSharedPtr<FSlateBrush> MaterialBrush;
};
