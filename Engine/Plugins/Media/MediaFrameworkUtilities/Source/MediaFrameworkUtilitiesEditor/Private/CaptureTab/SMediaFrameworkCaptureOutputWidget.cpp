// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CaptureTab/SMediaFrameworkCaptureOutputWidget.h"

#include "EditorStyleSet.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Visibility.h"
#include "LevelEditorViewport.h"
#include "Slate/SceneViewport.h"
#include "SlateOptMacros.h"
#include "Textures/SlateIcon.h"
#include "UI/MediaFrameworkUtilitiesEditorStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"

#include "Editor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "MediaCapture.h"

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

	/*
	 * FMediaFrameworkCaptureLevelEditorViewportClient
	 * Like FLevelEditorViewportClient but always use the PlayWorld if available. Do not support bSimulated
	 */
	class FMediaFrameworkCaptureLevelEditorViewportClient : public FLevelEditorViewportClient
	{
	private:
		using Super = FLevelEditorViewportClient;

	public:
		FMediaFrameworkCaptureLevelEditorViewportClient(const TSharedPtr<class SLevelViewport>& InLevelViewport, EViewModeIndex InViewModeIndex)
			: FLevelEditorViewportClient(InLevelViewport)
			, ViewModeIndex(InViewModeIndex)
		{}
		~FMediaFrameworkCaptureLevelEditorViewportClient()
		{
			if (GEditor)
			{
				FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
				if (PIEWorldContext)
				{
					RemoveReferenceToWorldContext(*PIEWorldContext);
				}
			}
		}

		virtual UWorld* GetWorld() const override
		{
			if (GEditor->PlayWorld)
			{
				return GEditor->PlayWorld;
			}
			return Super::GetWorld();
		}

		void SetPIE(bool bInIsPIE)
		{
			FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
			if (bInIsPIE)
			{
				if (PIEWorldContext)
				{
					RemoveReferenceToWorldContext(GEditor->GetEditorWorldContext());
					SetReferenceToWorldContext(*PIEWorldContext);
				}
			}
			else
			{
				if (PIEWorldContext)
				{
					RemoveReferenceToWorldContext(*PIEWorldContext);
				}
				SetReferenceToWorldContext(GEditor->GetEditorWorldContext());
			}
			SetViewMode(ViewModeIndex);
			SetRealtime(true, false);
			SetRealtime(true, true); // Save that setting for RestoreRealtime
		}

		void AutoSetPIE()
		{
			if (GEditor->PlayWorld)
			{
				SetPIE(true);
			}
			else
			{
				SetViewMode(ViewModeIndex);
				SetRealtime(true, false);
				SetRealtime(true, true); // Save that setting for RestoreRealtime
			}
		}

	private:
		EViewModeIndex ViewModeIndex;
	};
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
	StopOutput();
}

void SMediaFrameworkCaptureOutputWidget::StopOutput()
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
		.FillHeight(1.0f)
		.VAlign(EVerticalAlignment::VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SThrobber)
					.Animate(SThrobber::VerticalAndOpacity)
					.NumPieces(1)
					.Visibility(this, &SMediaFrameworkCaptureOutputWidget::HandleThrobberVisibility)
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.ColorAndOpacity(this, &SMediaFrameworkCaptureOutputWidget::HandleIconColorAndOpacity)
					.Image(this, &SMediaFrameworkCaptureOutputWidget::HandleIconImage)
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(MediaOutput->GetName()))
				.Justification(ETextJustify::Left)
				.TextStyle(FEditorStyle::Get(), "LargeText")
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

FSlateColor SMediaFrameworkCaptureOutputWidget::HandleIconColorAndOpacity() const
{
	FSlateColor Result = FSlateColor::UseForeground();
	if (MediaCapture.IsValid())
	{
		EMediaCaptureState State = MediaCapture->GetState();
		switch (State)
		{
		case EMediaCaptureState::Error:
		case EMediaCaptureState::Stopped:
			Result = FLinearColor::Red;
			break;
		case EMediaCaptureState::Capturing:
			Result = FLinearColor::Green;
			break;
		case EMediaCaptureState::Preparing:
		case EMediaCaptureState::StopRequested:
			Result = FLinearColor::Yellow;
			break;
		}
	}
	return Result;
}

const FSlateBrush* SMediaFrameworkCaptureOutputWidget::HandleIconImage() const
{
	const FSlateBrush* Result = nullptr;
	if (MediaCapture.IsValid())
	{
		EMediaCaptureState State = MediaCapture->GetState();
		switch (State)
		{
		case EMediaCaptureState::Error:
		case EMediaCaptureState::Stopped:
			Result = FEditorStyle::GetBrush("Icons.Cross");
			break;
		case EMediaCaptureState::Capturing:
			Result = FEditorStyle::GetBrush("Symbols.Check");
			break;
		}
	}
	return Result;
}

EVisibility SMediaFrameworkCaptureOutputWidget::HandleThrobberVisibility() const
{
	if (MediaCapture.IsValid())
	{
		EMediaCaptureState State = MediaCapture->GetState();
		if (State == EMediaCaptureState::Preparing || State == EMediaCaptureState::StopRequested)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}

bool SMediaFrameworkCaptureOutputWidget::IsValid() const
{
	return MediaOutput.IsValid() && MediaCapture.IsValid();
}

/*
 * SMediaFrameworkCaptureCameraViewportWidget implementation
 */
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMediaFrameworkCaptureCameraViewportWidget::Construct(const FArguments& InArgs)
{
	FEditorDelegates::PostPIEStarted.AddSP(this, &SMediaFrameworkCaptureCameraViewportWidget::OnPostPIEStarted);
	FEditorDelegates::PrePIEEnded.AddSP(this, &SMediaFrameworkCaptureCameraViewportWidget::OnPrePIEEnded);

	PreviewActors = InArgs._PreviewActors;
	ViewMode = InArgs._ViewMode;
	CurrentLockCameraIndex = 0;

	SMediaFrameworkCaptureOutputWidget::FArguments BaseArguments;
	BaseArguments._Owner = InArgs._Owner;
	BaseArguments._MediaOutput = InArgs._MediaOutput;
	SMediaFrameworkCaptureOutputWidget::Construct(BaseArguments);

	LevelViewportClient = MakeShareable(new MediaFrameworkUtilities::FMediaFrameworkCaptureLevelEditorViewportClient(TSharedPtr<class SLevelViewport>(), ViewMode));
	{
		// Preview viewports never be a listener
		LevelViewportClient->bSetListenerPosition = false;

		// Default to "game" show flags for camera previews
		LevelViewportClient->EngineShowFlags = FEngineShowFlags(ESFIM_Game);
		LevelViewportClient->LastEngineShowFlags = FEngineShowFlags(ESFIM_Editor);

		LevelViewportClient->ViewportType = LVT_Perspective;
		LevelViewportClient->bDrawAxes = false;
		LevelViewportClient->bDisableInput = true;
		LevelViewportClient->SetAllowCinematicPreview(false);
		LevelViewportClient->VisibilityDelegate.BindLambda([] {return true; });
		LevelViewportClient->AutoSetPIE();
	}

	UpdateActivePreviewList(GEditor->PlayWorld != nullptr);
	LevelViewportClient->UpdateViewForLockedActor();

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


	TSharedPtr<SWidget> GridPanelWidget = SNullWidget::NullWidget;
	const int32 CameraNum = PreviewActors.Num();
	if (CameraNum > 1)
	{
		TSharedPtr<SGridPanel> GridPanel = SNew(SGridPanel);
		GridPanelWidget = GridPanel;

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
					.IsEnabled_Lambda([this, CameraCounter]()
					{
						if (ActivePreviewActors.IsValidIndex(CameraCounter))
						{
							return ActivePreviewActors[CameraCounter].IsValid();
						}

						return false;
					})
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
				GridPanelWidget.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			.AutoHeight()
			[
				BuildBaseWidget(ViewportWidget.ToSharedRef())
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SMediaFrameworkCaptureCameraViewportWidget::~SMediaFrameworkCaptureCameraViewportWidget()
{
	if (LevelViewportClient.IsValid())
	{
		LevelViewportClient->Viewport = nullptr;
	}

	FEditorDelegates::PostPIEStarted.RemoveAll(this);
	FEditorDelegates::PrePIEEnded.RemoveAll(this);
}

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

void SMediaFrameworkCaptureCameraViewportWidget::OnPostPIEStarted(bool bWasSimulatingInEditor)
{
	const bool bIsPIE = true;
	if (LevelViewportClient.IsValid())
	{
		LevelViewportClient->SetPIE(bIsPIE);
	}

	UpdateActivePreviewList(bIsPIE);
}

void SMediaFrameworkCaptureCameraViewportWidget::OnPrePIEEnded(bool bWasSimulatingInEditor)
{
	const bool bIsPIE = false;
	UpdateActivePreviewList(bIsPIE);
	
	if (LevelViewportClient.IsValid())
	{
		LevelViewportClient->SetPIE(bIsPIE);
	}
}

void SMediaFrameworkCaptureCameraViewportWidget::SetActorLock(int32 CameraIndex)
{
	CurrentLockCameraIndex = CameraIndex;
	if (ActivePreviewActors.IsValidIndex(CurrentLockCameraIndex))
	{
		LevelViewportClient->SetActorLock(ActivePreviewActors[CurrentLockCameraIndex].Get());
	}
	else
	{
		LevelViewportClient->SetActorLock(nullptr);
	}
}

void SMediaFrameworkCaptureCameraViewportWidget::UpdateActivePreviewList(bool bIsPIE)
{
	if (bIsPIE)
	{
		ActivePreviewActors.Empty();
		for (TWeakObjectPtr<AActor> PreviewActor : PreviewActors)
		{
			AActor* CounterpartActor = EditorUtilities::GetSimWorldCounterpartActor(PreviewActor.Get());
			ActivePreviewActors.Add(CounterpartActor);
		}
	}
	else
	{
		ActivePreviewActors = PreviewActors;
	}

	SetActorLock(CurrentLockCameraIndex);
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
	ImageMaterial.Reset(NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient));
	UMaterialExpressionTextureSample* TextureSampler = NewObject<UMaterialExpressionTextureSample>(ImageMaterial.Get());
	{
		TextureSampler->Texture = RenderTarget.Get();
		TextureSampler->AutoSetSampleType();
	}

	FExpressionOutput& Output = TextureSampler->GetOutputs()[0];
	FExpressionInput& Input = ImageMaterial->EmissiveColor;
	{
		Input.Expression = TextureSampler;
		Input.Mask = Output.Mask;
		Input.MaskR = Output.MaskR;
		Input.MaskG = Output.MaskG;
		Input.MaskB = Output.MaskB;
		Input.MaskA = Output.MaskA;
	}

	ImageMaterial->Expressions.Add(TextureSampler);
	ImageMaterial->MaterialDomain = EMaterialDomain::MD_UI;
	ImageMaterial->PostEditChange();

	// create Slate brush
	ImageMaterialBrush = MakeShareable(new FSlateBrush());
	{
		ImageMaterialBrush->SetResourceObject(ImageMaterial.Get());
	}

	TSharedRef<SWidget> PictureBox = SNew(SImage)
		.Image(ImageMaterialBrush.IsValid() ? ImageMaterialBrush.Get() : FEditorStyle::GetBrush("WhiteTexture"));

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
