// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CaptureTab/SMediaFrameworkCapture.h"

#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "SlateOptMacros.h"
#include "Slate/SceneViewport.h"
#include "Textures/SlateIcon.h"
#include "UI/MediaFrameworkUtilitiesEditorStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#include "CaptureTab/SMediaFrameworkCaptureOutputWidget.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "MediaFrameworkWorldSettingsAssetUserData.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "MediaFrameworkUtilities"

namespace MediaFrameworkUtilities
{
	static const FName MediaFrameworkUtilitiesApp = FName("MediaFrameworkCaptureCameraViewportApp");
	static const FName LevelEditorModuleName("LevelEditor");

	TSharedRef<SDockTab> CreateMediaFrameworkCaptureCameraViewportTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SMediaFrameworkCapture)
			];
	}

	static float CaptureVerticalBoxPadding = 4.f;
	class SCaptureVerticalBox : public SVerticalBox
	{
	public:
		TWeakPtr<SMediaFrameworkCapture> Owner;
		TArray<TSharedPtr<SMediaFrameworkCaptureOutputWidget>> CaptureOutputWidget;

		void AddCaptureWidget(const TSharedPtr<SMediaFrameworkCaptureOutputWidget>& InWidget)
		{
			AddSlot()
			.Padding(FMargin(0.0f, CaptureVerticalBoxPadding, 0.0f, 0.0f))
			[
				InWidget.ToSharedRef()
			];

			CaptureOutputWidget.Add(InWidget);
		}

		void RemoveCaptureWidget(const TSharedPtr<SMediaFrameworkCaptureOutputWidget>& InWidget)
		{
			RemoveSlot(InWidget.ToSharedRef());
			CaptureOutputWidget.RemoveSingleSwap(InWidget);
		}

		virtual FVector2D ComputeDesiredSize(float Scale) const override
		{
			FVector2D SuperComputeDesiredSize = SVerticalBox::ComputeDesiredSize(Scale);
			float ChildComputeDesiredSizeY = 0.f;
			for(TSharedPtr<SMediaFrameworkCaptureOutputWidget> Widget : CaptureOutputWidget)
			{
				const FVector2D& CurChildDesiredSize = Widget->GetDesiredSize();
				ChildComputeDesiredSizeY += CurChildDesiredSize.Y;
				ChildComputeDesiredSizeY += CaptureVerticalBoxPadding;
			}
			return FVector2D(SuperComputeDesiredSize.X, FMath::Max(SuperComputeDesiredSize.Y, ChildComputeDesiredSizeY));
		}
	};
}

FDelegateHandle SMediaFrameworkCapture::LevelEditorTabManagerChangedHandle;

void SMediaFrameworkCapture::RegisterNomadTabSpawner()
{
	auto RegisterTabSpawner = []()
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(MediaFrameworkUtilities::LevelEditorModuleName);
		TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();

		LevelEditorTabManager->RegisterTabSpawner(MediaFrameworkUtilities::MediaFrameworkUtilitiesApp, FOnSpawnTab::CreateStatic(&MediaFrameworkUtilities::CreateMediaFrameworkCaptureCameraViewportTab))
			.SetDisplayName(LOCTEXT("TabTitle", "Media Capture"))
			.SetTooltipText(LOCTEXT("TooltipText", "Displays Capture Camera Viewport and Render Target."))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
			.SetIcon(FSlateIcon(FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName(), "CaptureCameraViewport_Capture.Small"));
	};

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	if (LevelEditorModule.GetLevelEditorTabManager())
	{
		RegisterTabSpawner();
	}
	else
	{
		LevelEditorTabManagerChangedHandle = LevelEditorModule.OnTabManagerChanged().AddLambda(RegisterTabSpawner);
	}
}

void SMediaFrameworkCapture::UnregisterNomadTabSpawner()
{
	if (FSlateApplication::IsInitialized() && FModuleManager::Get().IsModuleLoaded(MediaFrameworkUtilities::LevelEditorModuleName))
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(MediaFrameworkUtilities::LevelEditorModuleName);
		TSharedPtr<FTabManager> LevelEditorTabManager;
		if (LevelEditorModule)
		{
			LevelEditorTabManager = LevelEditorModule->GetLevelEditorTabManager();
			LevelEditorModule->OnTabManagerChanged().Remove(LevelEditorTabManagerChangedHandle);
		}

		if (LevelEditorTabManager.IsValid())
		{
			LevelEditorTabManager->UnregisterTabSpawner(MediaFrameworkUtilities::MediaFrameworkUtilitiesApp);
		}
	}
}

SMediaFrameworkCapture::~SMediaFrameworkCapture()
{
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.RemoveAll(this);
	GEngine->OnLevelActorDeleted().RemoveAll(this);
	FEditorDelegates::MapChange.RemoveAll(this);
	FEditorDelegates::OnAssetsDeleted.RemoveAll(this);
	EnabledCapture(false);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMediaFrameworkCapture::Construct(const FArguments& InArgs)
{
	bIsCapturing = false;

	FEditorDelegates::MapChange.AddSP(this, &SMediaFrameworkCapture::OnMapChange);
	GEngine->OnLevelActorDeleted().AddSP(this, &SMediaFrameworkCapture::OnLevelActorsRemoved);
	FEditorDelegates::OnAssetsDeleted.AddSP(this, &SMediaFrameworkCapture::OnAssetsDeleted);
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddRaw(this, &SMediaFrameworkCapture::OnObjectPreEditChange);

	UMediaFrameworkWorldSettingsAssetUserData* AssetUserData = FindOrAddMediaFrameworkAssetUserData();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bAllowFavoriteSystem = false;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.ViewIdentifier = "MediaFrameworkUtilitites";
	DetailView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailView->SetObject(AssetUserData);

	SAssignNew(CaptureBoxes, MediaFrameworkUtilities::SCaptureVerticalBox);
	CaptureBoxes->Owner = StaticCastSharedRef<SMediaFrameworkCapture>(AsShared());

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
		[
			MakeToolBar()
		]
		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
		[
			SNew(SSplitter)
			.Orientation(EOrientation::Orient_Vertical)
			+ SSplitter::Slot()
			[
				SNew(SBorder)
				.IsEnabled_Lambda([this]() { return !IsCapturing(); })
				[
					DetailView.ToSharedRef()
				]
			]
			+ SSplitter::Slot()
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					CaptureBoxes.ToSharedRef()
				]
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<class SWidget> SMediaFrameworkCapture::MakeToolBar()
{
	FToolBarBuilder ToolBarBuilder(TSharedPtr<FUICommandList>(), FMultiBoxCustomization::None);
	ToolBarBuilder.BeginSection(TEXT("Player"));
	{
		ToolBarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateLambda([this]
				{
					EnabledCapture(true);
				}),
				FCanExecuteAction::CreateLambda([=]
				{
					return CanEnableViewport() && !bIsCapturing;
				})),
			NAME_None,
			LOCTEXT("Output_Label", "Capture"),
			LOCTEXT("Output_ToolTip", "Capture the camera's viewport and the render target."),
			FSlateIcon(FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName(), "CaptureCameraViewport_Capture")
			);
		ToolBarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateLambda([this]
				{
					EnabledCapture(false);
				}),
				FCanExecuteAction::CreateLambda([this]
				{
					return bIsCapturing;
				})
			),
			NAME_None,
			LOCTEXT("Stop_Label", "Stop"),
			LOCTEXT("Stop_ToolTip", "Stop the capturing of the camera's viewport and the render target."),
			FSlateIcon(FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName(), "CaptureCameraViewport_Stop")
			);
	}
	ToolBarBuilder.EndSection();


	return ToolBarBuilder.MakeWidget();
}

bool SMediaFrameworkCapture::CanEnableViewport() const
{
	UMediaFrameworkWorldSettingsAssetUserData* AssetUserData = FindMediaFrameworkAssetUserData();
	bool bEnabled = AssetUserData && (AssetUserData->ViewportCaptures.Num() || AssetUserData->RenderTargetCaptures.Num());
	if (bEnabled)
	{
		for (const FMediaFrameworkCaptureCameraViewportCameraOutputInfo& Info : AssetUserData->ViewportCaptures)
		{
			bEnabled = Info.MediaOutput && Info.LockedActors.Num() > 0;
			for (const TLazyObjectPtr<AActor>& CameraActorRef : Info.LockedActors)
			{
				bEnabled = bEnabled && CameraActorRef.IsValid();
			}

			if (!bEnabled)
			{
				break;
			}
		}

		if (bEnabled)
		{
			for (const FMediaFrameworkCaptureRenderTargetCameraOutputInfo& Info : AssetUserData->RenderTargetCaptures)
			{
				bEnabled = Info.MediaOutput && Info.RenderTarget;

				if (!bEnabled)
				{
					break;
				}
			}
		}
	}
	return bEnabled;
}

void SMediaFrameworkCapture::EnabledCapture(bool bEnabled)
{
	if (bEnabled)
	{
		bEnabled = CanEnableViewport();
	}

	if (bEnabled)
	{
		if (bIsCapturing)
		{
			EnabledCapture(false);
		}

		UMediaFrameworkWorldSettingsAssetUserData* AssetUserData = FindOrAddMediaFrameworkAssetUserData();
		check(AssetUserData);
		for (const FMediaFrameworkCaptureCameraViewportCameraOutputInfo& Info : AssetUserData->ViewportCaptures)
		{
			TArray<TWeakObjectPtr<AActor>> InfoPreviewActors;
			InfoPreviewActors.Reserve(Info.LockedActors.Num());
			for (const TLazyObjectPtr<AActor>& ActorRef : Info.LockedActors)
			{
				AActor* Actor = ActorRef.Get();
				if (Actor)
				{
					InfoPreviewActors.Add(Actor);
				}
			}

			TSharedPtr<SMediaFrameworkCaptureCameraViewportWidget> CaptureCamaraViewport = SNew(SMediaFrameworkCaptureCameraViewportWidget)
				.Owner(SharedThis(this))
				.PreviewActors(InfoPreviewActors)
				.MediaOutput(Info.MediaOutput)
				.ViewMode(Info.ViewMode);

			CaptureBoxes->AddCaptureWidget(CaptureCamaraViewport);
			CaptureCamaraViewport->StartOutput();
			CaptureCameraViewports.Add(CaptureCamaraViewport);
		}

		for (const FMediaFrameworkCaptureRenderTargetCameraOutputInfo& Info : AssetUserData->RenderTargetCaptures)
		{
			TSharedPtr<SMediaFrameworkCaptureRenderTargetWidget> CaptureRenderTarget = SNew(SMediaFrameworkCaptureRenderTargetWidget)
				.Owner(SharedThis(this))
				.MediaOutput(Info.MediaOutput)
				.RenderTarget(Info.RenderTarget);

			CaptureBoxes->AddCaptureWidget(CaptureRenderTarget);
			CaptureRenderTarget->StartOutput();
			CaptureRenderTargets.Add(CaptureRenderTarget);
		}
	}
	else
	{
		for (TSharedPtr<SMediaFrameworkCaptureCameraViewportWidget> CaptureCameraViewport : CaptureCameraViewports)
		{
			CaptureBoxes->RemoveCaptureWidget(CaptureCameraViewport.ToSharedRef());
		}
		CaptureCameraViewports.Reset();
		for (TSharedPtr<SMediaFrameworkCaptureRenderTargetWidget> CaptureRenderTarget : CaptureRenderTargets)
		{
			CaptureBoxes->RemoveCaptureWidget(CaptureRenderTarget.ToSharedRef());
		}
		CaptureRenderTargets.Reset();
	}

	bIsCapturing = CaptureCameraViewports.Num() > 0 || CaptureRenderTargets.Num() > 0;
}

UMediaFrameworkWorldSettingsAssetUserData* SMediaFrameworkCapture::FindMediaFrameworkAssetUserData() const
{
	UMediaFrameworkWorldSettingsAssetUserData* Result = nullptr;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
	if (World)
	{
		AWorldSettings* WorldSetting = World ? World->GetWorldSettings() : nullptr;
		if (WorldSetting)
		{
			Result = CastChecked<UMediaFrameworkWorldSettingsAssetUserData>(WorldSetting->GetAssetUserDataOfClass(UMediaFrameworkWorldSettingsAssetUserData::StaticClass()), ECastCheckedType::NullAllowed);
		}
	}

	return Result;
}

UMediaFrameworkWorldSettingsAssetUserData* SMediaFrameworkCapture::FindOrAddMediaFrameworkAssetUserData()
{
	UMediaFrameworkWorldSettingsAssetUserData* Result = nullptr;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
	if (World)
	{
		AWorldSettings* WorldSetting = World ? World->GetWorldSettings() : nullptr;
		if (WorldSetting)
		{
			Result = CastChecked<UMediaFrameworkWorldSettingsAssetUserData>(WorldSetting->GetAssetUserDataOfClass(UMediaFrameworkWorldSettingsAssetUserData::StaticClass()), ECastCheckedType::NullAllowed);

			if (!Result)
			{
				Result = NewObject<UMediaFrameworkWorldSettingsAssetUserData>(WorldSetting);
				WorldSetting->AddAssetUserData(Result);
			}
		}
	}

	return Result;
}

void SMediaFrameworkCapture::OnMapChange(uint32 MapFlags)
{
	UMediaFrameworkWorldSettingsAssetUserData* AssetUserData = FindOrAddMediaFrameworkAssetUserData();
	DetailView->SetObject(AssetUserData);
	EnabledCapture(false);
}

void SMediaFrameworkCapture::OnLevelActorsRemoved(AActor* InActor)
{
	UMediaFrameworkWorldSettingsAssetUserData* AssetUserData = FindMediaFrameworkAssetUserData();
	if (AssetUserData)
	{
		for (const FMediaFrameworkCaptureCameraViewportCameraOutputInfo& Info : AssetUserData->ViewportCaptures)
		{
			if (Info.LockedActors.Contains(InActor))
			{
				EnabledCapture(false);
				return;
			}
		}
	}
}

void SMediaFrameworkCapture::OnAssetsDeleted(const TArray<UClass*>& DeletedAssetClasses)
{
	if (bIsCapturing)
	{
		bool bCheck = false;
		for(UClass* AssetClass : DeletedAssetClasses)
		{
			if (AssetClass->IsChildOf<UTextureRenderTarget2D>())
			{
				bCheck = true;
				break;
			}
		}

		if (bCheck)
		{
			for(const TSharedPtr<SMediaFrameworkCaptureRenderTargetWidget>& Capture : CaptureRenderTargets)
			{
				if (!Capture->IsValid())
				{
					EnabledCapture(false);
					return;
				}
			}
		}
	}
}

void SMediaFrameworkCapture::OnObjectPreEditChange(UObject* Object, const FEditPropertyChain& PropertyChain)
{
	UMediaFrameworkWorldSettingsAssetUserData* AssetUserData = FindMediaFrameworkAssetUserData();
	if (Object == AssetUserData)
	{
		EnabledCapture(false);
	}
}

#undef LOCTEXT_NAMESPACE
