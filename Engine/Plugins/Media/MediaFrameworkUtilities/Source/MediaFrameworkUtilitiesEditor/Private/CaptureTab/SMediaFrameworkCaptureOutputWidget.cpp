// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CaptureTab/SMediaFrameworkCaptureOutputWidget.h"

#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Visibility.h"
#include "LevelEditorViewport.h"
#include "Slate/SceneViewport.h"
#include "SlateOptMacros.h"
#include "Textures/SlateIcon.h"
#include "UI/MediaBundleEditorStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SViewport.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"

#define LOCTEXT_NAMESPACE "MediaFrameworkUtilities"

/*
 * MediaFrameworkUtilities namespace
 */
namespace MediaFrameworkUtilities
{
	const float Padding = 4.f;
	const float ViewportBoxDesiredSizeY = 200.f;
	float GetWidthOverride(TWeakObjectPtr<UMediaOutput> MediaOutput)
	{
		float Ratio = 1.77777777f;
		UMediaOutput* MediaOutputPtr = MediaOutput.Get();
		if (MediaOutputPtr)
		{
			FIntPoint TargetSize = MediaOutputPtr->GetRequestedSize();
			Ratio = (float)TargetSize.X / (float)TargetSize.Y;
		}
		return ViewportBoxDesiredSizeY * Ratio;
	}
}

/*
 * SMediaFrameworkCaptureOutputWidget implementation
 */
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMediaFrameworkCaptureOutputWidget::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	MediaOutput = InArgs._MediaOutput;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SMediaFrameworkCaptureOutputWidget::~SMediaFrameworkCaptureOutputWidget()
{
	if (MediaCapture.IsValid())
	{
		MediaCapture->StopCapture(false);
		MediaCapture.Reset();
	}
}

TSharedRef<SWidget> SMediaFrameworkCaptureOutputWidget::BuildBaseWidget(TSharedRef<SWidget> InnerWidget)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(0.0f, 0.0f)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.MaxHeight(10)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SProgressBar)
				.ToolTipText(LOCTEXT("PreparingTooltip", "Preparing..."))
				.Visibility_Lambda([this]() -> EVisibility
				{
					return (IsValid() && MediaCapture->GetState() == EMediaCaptureState::Preparing) ? EVisibility::Visible : EVisibility::Hidden;
				})
			]
			+ SOverlay::Slot()
			[
				SNew(SColorBlock)
				.Color(this, &SMediaFrameworkCaptureOutputWidget::GetProgressColor)
				.IgnoreAlpha(true)
				.Visibility_Lambda([this]() -> EVisibility
				{
				return (IsValid() && MediaCapture->GetState() != EMediaCaptureState::Preparing) ? EVisibility::Visible : EVisibility::Hidden;
				})
			]
		]
		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.AutoHeight()
		[
			// Live view of the Source
			SNew(SBox)
			.Visibility(EVisibility::HitTestInvisible)
			.WidthOverride(MakeAttributeLambda([this]
			{
				return FOptionalSize(MediaFrameworkUtilities::GetWidthOverride(MediaOutput));
			}))
			.HeightOverride(MediaFrameworkUtilities::ViewportBoxDesiredSizeY)
			[
				InnerWidget
			]
		];
}

FLinearColor SMediaFrameworkCaptureOutputWidget::GetProgressColor() const
{
	if (MediaCapture.IsValid())
	{
		EMediaCaptureState State = MediaCapture->GetState();
		switch (State)
		{
		case EMediaCaptureState::Error:
			return FColor::Red;
		case EMediaCaptureState::Stopped:
			return FColor::Red;
		case EMediaCaptureState::Capturing:
			return FColor::Green;
		case EMediaCaptureState::Preparing:
		case EMediaCaptureState::StopRequested:
			return FColor::Yellow;
		}
	}
	return FColor::Black;
}

bool SMediaFrameworkCaptureOutputWidget::IsValid() const
{
	return MediaOutput.IsValid() && MediaCapture.IsValid();
}

/*
 * SMediaFrameworkCaptureCameraViewportWidget implementation
 */
SMediaFrameworkCaptureCameraViewportWidget::~SMediaFrameworkCaptureCameraViewportWidget()
{
	if (LevelViewportClient.IsValid())
	{
		LevelViewportClient->Viewport = nullptr;
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMediaFrameworkCaptureCameraViewportWidget::Construct(const FArguments& InArgs)
{
	PreviewActors = InArgs._PreviewActors;
	CurrentLockCameraIndex = 0;

	SMediaFrameworkCaptureOutputWidget::FArguments BaseArguments;
	BaseArguments._Owner = InArgs._Owner;
	BaseArguments._MediaOutput = InArgs._MediaOutput;
	SMediaFrameworkCaptureOutputWidget::Construct(BaseArguments);

	LevelViewportClient = MakeShareable(new FLevelEditorViewportClient(TSharedPtr<class SLevelViewport>()));
	{
		LevelViewportClient->ViewportType = LVT_Perspective;

		// Preview viewports never be a listener
		LevelViewportClient->bSetListenerPosition = false;

		// Never draw the axes indicator in these small viewports
		LevelViewportClient->bDrawAxes = false;

		// Default to "game" show flags for camera previews
		// Still draw selection highlight though
		LevelViewportClient->EngineShowFlags = FEngineShowFlags(ESFIM_Game);
		LevelViewportClient->EngineShowFlags.SetSelection(true);
		LevelViewportClient->LastEngineShowFlags = FEngineShowFlags(ESFIM_Editor);

		// We don't use view modes for preview viewports
		LevelViewportClient->SetViewMode(VMI_Unknown);

		// User should never be able to interact with this viewport
		LevelViewportClient->bDisableInput = true;

		// Never allow Matinee to possess these views
		LevelViewportClient->SetAllowCinematicPreview(false);

		// Our viewport is always visible
		LevelViewportClient->VisibilityDelegate.BindLambda([] {return true; });

		// Push actor transform to view.  From here on out, this will happen automatically in FLevelEditorViewportClient::Tick.
		// The reason we allow the viewport client to update this is to avoid off-by-one-frame issues when dragging actors around.
		if (PreviewActors.Num())
		{
			LevelViewportClient->SetActorLock(PreviewActors[0].Get());
		}
		LevelViewportClient->UpdateViewForLockedActor();
		LevelViewportClient->SetRealtime(true);
	}

	ViewportWidget = SNew(SViewport)
		.RenderDirectlyToWindow(false)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
		.EnableGammaCorrection(false)		// Scene rendering handles gamma correction
		.EnableBlending(false);

	SceneViewport = MakeShareable(new FSceneViewport(LevelViewportClient.Get(), ViewportWidget));
	{
		LevelViewportClient->Viewport = SceneViewport.Get();
		ViewportWidget->SetViewportInterface(SceneViewport.ToSharedRef());
	}

	TSharedPtr<SGridPanel> GridPanel = SNew(SGridPanel);
	{
		const int32 CameraNum = PreviewActors.Num();
		const int32 MaxCameraPerRow = 5;

		int32 CameraIndexY = 0;
		int32 CameraCounter = 0;
		while (CameraCounter < CameraNum)
		{
			for (int32 IndexX = 0; IndexX < MaxCameraPerRow && CameraCounter < CameraNum; ++CameraCounter, ++IndexX)
			{
				GridPanel->AddSlot(IndexX, CameraIndexY)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Text(FText::FromString(PreviewActors[CameraCounter]->GetActorLabel()))
					.OnClicked(FOnClicked::CreateLambda([this, CameraCounter]()
					{
						SetActorLock(CameraCounter);
						return FReply::Handled();
					}))
				];
			}
			++CameraIndexY;
		}
	}

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(MediaFrameworkUtilities::Padding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				GridPanel.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				BuildBaseWidget(ViewportWidget.ToSharedRef())
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMediaFrameworkCaptureCameraViewportWidget::StartOutput()
{
	UMediaOutput* MediaOutputPtr = MediaOutput.Get();
	if (MediaOutputPtr && SceneViewport.IsValid())
	{
		MediaCapture.Reset(MediaOutputPtr->CreateMediaCapture());
		if (MediaCapture.IsValid())
		{
			FIntPoint TargetSize = MediaOutputPtr->GetRequestedSize();
			SceneViewport->SetFixedViewportSize(TargetSize.X, TargetSize.Y);
			MediaCapture->CaptureSceneViewport(SceneViewport);
		}
	}
}

void SMediaFrameworkCaptureCameraViewportWidget::SetActorLock(int32 CameraIndex)
{
	LevelViewportClient->SetActorLock(PreviewActors[CameraIndex].Get());
}

/*
 * SMediaFrameworkCaptureRenderTargetWidget implementation
 */
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMediaFrameworkCaptureRenderTargetWidget::Construct(const FArguments& InArgs)
{
	RenderTarget = InArgs._RenderTarget;

	SMediaFrameworkCaptureOutputWidget::FArguments BaseArguments;
	BaseArguments._Owner = InArgs._Owner;
	BaseArguments._MediaOutput = InArgs._MediaOutput;
	SMediaFrameworkCaptureOutputWidget::Construct(BaseArguments);

	// create material
	UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
	UMaterialExpressionTextureSample* TextureSampler = NewObject<UMaterialExpressionTextureSample>(Material);
	{
		TextureSampler->Texture = RenderTarget.Get();
		TextureSampler->AutoSetSampleType();
	}

	FExpressionOutput& Output = TextureSampler->GetOutputs()[0];
	FExpressionInput& Input = Material->EmissiveColor;
	{
		Input.Expression = TextureSampler;
		Input.Mask = Output.Mask;
		Input.MaskR = Output.MaskR;
		Input.MaskG = Output.MaskG;
		Input.MaskB = Output.MaskB;
		Input.MaskA = Output.MaskA;
	}

	Material->Expressions.Add(TextureSampler);
	Material->MaterialDomain = EMaterialDomain::MD_UI;
	Material->PostEditChange();

	// create Slate brush
	MaterialBrush = MakeShareable(new FSlateBrush());
	{
		MaterialBrush->SetResourceObject(Material);
	}

	TSharedRef<SWidget> PictureBox = SNew(SImage)
		.Image(MaterialBrush.IsValid() ? MaterialBrush.Get() : FEditorStyle::GetBrush("WhiteTexture"));

	this->ChildSlot
	[
		SNew(SBorder)
		.Padding(MediaFrameworkUtilities::Padding)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				BuildBaseWidget(PictureBox)
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMediaFrameworkCaptureRenderTargetWidget::StartOutput()
{
	UMediaOutput* MediaOutputPtr = MediaOutput.Get();
	UTextureRenderTarget2D* RenderTargetPtr = RenderTarget.Get();
	if (MediaOutputPtr && RenderTargetPtr)
	{
		MediaCapture.Reset(MediaOutputPtr->CreateMediaCapture());
		if (MediaCapture.IsValid())
		{
			MediaCapture->CaptureTextureRenderTarget2D(RenderTargetPtr);
		}
	}
}

SMediaFrameworkCaptureRenderTargetWidget::~SMediaFrameworkCaptureRenderTargetWidget()
{
	if (MediaCapture.IsValid())
	{
		MediaCapture->StopCapture(false);
		MediaCapture.Reset();
	}
}

bool SMediaFrameworkCaptureRenderTargetWidget::IsValid() const
{
	return RenderTarget.IsValid();
}

#undef LOCTEXT_NAMESPACE
