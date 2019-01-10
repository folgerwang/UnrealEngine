// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Misc/LevelSequencePlaybackContext.h"
#include "Misc/LevelSequenceEditorSettings.h"
#include "LevelSequencePlayer.h"
#include "Engine/World.h"

#include "Delegates/Delegate.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "MovieSceneCaptureDialogModule.h"
#include "EditorStyleSet.h"
#include "Engine/Engine.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "LevelSequencePlaybackContext"

class SLevelSequenceContextPicker : public SCompoundWidget
{
public:

	DECLARE_DELEGATE_OneParam(FOnSetValue, UWorld*);

	SLATE_BEGIN_ARGS(SLevelSequenceContextPicker){}

		/** Attribute for retrieving the current context */
		SLATE_ATTRIBUTE(UWorld*, Value)

		/** Called when the user explicitly chooses a new context world. */
		SLATE_EVENT(FOnSetValue, OnSetValue)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	TSharedRef<SWidget> BuildWorldPickerMenu();

	static FText GetWorldDescription(UWorld* World);

	FText GetCurrentContextText() const
	{
		UWorld* CurrentWorld = ValueAttribute.Get();
		check(CurrentWorld);
		return GetWorldDescription(CurrentWorld);
	}

	const FSlateBrush* GetBorderBrush() const
	{
		UWorld* CurrentWorld = ValueAttribute.Get();
		check(CurrentWorld);

		if (CurrentWorld->WorldType == EWorldType::PIE)
		{
			return GEditor->bIsSimulatingInEditor ? FEditorStyle::GetBrush("LevelViewport.StartingSimulateBorder") : FEditorStyle::GetBrush("LevelViewport.StartingPlayInEditorBorder");
		}
		else
		{
			return FEditorStyle::GetBrush("LevelViewport.NoViewportBorder");
		}
	}

	void ToggleAutoPIE() const
	{
		ULevelSequenceEditorSettings* Settings = GetMutableDefault<ULevelSequenceEditorSettings>();
		Settings->bAutoBindToPIE = !Settings->bAutoBindToPIE;
		Settings->SaveConfig();

		OnSetValueEvent.ExecuteIfBound(nullptr);
	}

	bool IsAutoPIEChecked() const
	{
		return GetDefault<ULevelSequenceEditorSettings>()->bAutoBindToPIE;
	}

	void ToggleAutoSimulate() const
	{
		ULevelSequenceEditorSettings* Settings = GetMutableDefault<ULevelSequenceEditorSettings>();
		Settings->bAutoBindToSimulate = !Settings->bAutoBindToSimulate;
		Settings->SaveConfig();

		OnSetValueEvent.ExecuteIfBound(nullptr);
	}

	bool IsAutoSimulateChecked() const
	{
		return GetDefault<ULevelSequenceEditorSettings>()->bAutoBindToSimulate;
	}

	void OnSetValue(TWeakObjectPtr<UWorld> InWorld)
	{
		if (UWorld* NewContext = InWorld.Get())
		{
			OnSetValueEvent.ExecuteIfBound(NewContext);
		}
	}

	bool IsWorldCurrentValue(TWeakObjectPtr<UWorld> InWorld)
	{
		return InWorld == ValueAttribute.Get();
	}

private:
	TAttribute<UWorld*> ValueAttribute;
	FOnSetValue OnSetValueEvent;
};



FLevelSequencePlaybackContext::FLevelSequencePlaybackContext()
{
	FEditorDelegates::MapChange.AddRaw(this, &FLevelSequencePlaybackContext::OnMapChange);
	FEditorDelegates::PreBeginPIE.AddRaw(this, &FLevelSequencePlaybackContext::OnPieEvent);
	FEditorDelegates::BeginPIE.AddRaw(this, &FLevelSequencePlaybackContext::OnPieEvent);
	FEditorDelegates::PostPIEStarted.AddRaw(this, &FLevelSequencePlaybackContext::OnPieEvent);
	FEditorDelegates::PrePIEEnded.AddRaw(this, &FLevelSequencePlaybackContext::OnPieEvent);
	FEditorDelegates::EndPIE.AddRaw(this, &FLevelSequencePlaybackContext::OnPieEvent);
}

FLevelSequencePlaybackContext::~FLevelSequencePlaybackContext()
{
	FEditorDelegates::MapChange.RemoveAll(this);
	FEditorDelegates::PreBeginPIE.RemoveAll(this);
	FEditorDelegates::BeginPIE.RemoveAll(this);
	FEditorDelegates::PostPIEStarted.RemoveAll(this);
	FEditorDelegates::PrePIEEnded.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);
}

void FLevelSequencePlaybackContext::OnPieEvent(bool)
{
	WeakCurrentContext = nullptr;
}

void FLevelSequencePlaybackContext::OnMapChange(uint32)
{
	WeakCurrentContext = nullptr;
}

UWorld* FLevelSequencePlaybackContext::Get() const
{
	UWorld* Context = WeakCurrentContext.Get();
	if (Context)
	{
		return Context;
	}

	Context = ComputePlaybackContext();
	check(Context);
	WeakCurrentContext = Context;
	return Context;
}

UObject* FLevelSequencePlaybackContext::GetAsObject() const
{
	return Get();
}

TArray<UObject*> FLevelSequencePlaybackContext::GetEventContexts() const
{
	TArray<UObject*> Contexts;
	ULevelSequencePlayer::GetEventContexts(*Get(), Contexts);
	return Contexts;
}

void FLevelSequencePlaybackContext::OverrideWith(UWorld* InNewContext)
{
	// InNewContext may be null to force an auto update
	WeakCurrentContext = InNewContext;
}

TSharedRef<SWidget> FLevelSequencePlaybackContext::BuildWorldPickerCombo()
{
	return SNew(SLevelSequenceContextPicker)
		.Value(this, &FLevelSequencePlaybackContext::Get)
		.OnSetValue(this, &FLevelSequencePlaybackContext::OverrideWith);
}

UWorld* FLevelSequencePlaybackContext::ComputePlaybackContext()
{
	const ULevelSequenceEditorSettings* Settings            = GetDefault<ULevelSequenceEditorSettings>();
	IMovieSceneCaptureDialogModule*     CaptureDialogModule = FModuleManager::GetModulePtr<IMovieSceneCaptureDialogModule>("MovieSceneCaptureDialog");

	UWorld* RecordingWorld = CaptureDialogModule ? CaptureDialogModule->GetCurrentlyRecordingWorld() : nullptr;

	// Only allow PIE and Simulate worlds if the settings allow them
	const bool bIsSimulatingInEditor = GEditor && GEditor->bIsSimulatingInEditor;
	const bool bIsPIEValid           = (!bIsSimulatingInEditor && Settings->bAutoBindToPIE) || ( bIsSimulatingInEditor && Settings->bAutoBindToSimulate);

	UWorld* EditorWorld = nullptr;

	// Return PIE worlds if there are any
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE)
		{
			UWorld* ThisWorld = Context.World();
			if (bIsPIEValid && RecordingWorld != ThisWorld)
			{
				return ThisWorld;
			}
		}
		else if (Context.WorldType == EWorldType::Editor)
		{
			EditorWorld = Context.World();
		}
	}

	check(EditorWorld);
	return EditorWorld;
}


void SLevelSequenceContextPicker::Construct(const FArguments& InArgs)
{
	ValueAttribute = InArgs._Value;
	OnSetValueEvent = InArgs._OnSetValue;
	
	check(ValueAttribute.IsSet());
	check(OnSetValueEvent.IsBound());

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(this, &SLevelSequenceContextPicker::GetBorderBrush)
		[
			SNew(SComboButton)
			.ContentPadding(0)
			.ForegroundColor(FSlateColor::UseForeground())
			.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
			.OnGetMenuContent(this, &SLevelSequenceContextPicker::BuildWorldPickerMenu)
			.ToolTipText(LOCTEXT("WorldPickerText", "The world context that sequencer should be bound to, and playback within."))
			.ButtonContent()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("SceneOutliner.World"))
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2, 0, 0, 0)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SLevelSequenceContextPicker::GetCurrentContextText)
				]
			]
		]
	];
}

FText SLevelSequenceContextPicker::GetWorldDescription(UWorld* World)
{
	FText PostFix;
	if (World->WorldType == EWorldType::PIE)
	{
		switch(World->GetNetMode())
		{
		case NM_Client:
			PostFix = FText::Format(LOCTEXT("ClientPostfixFormat", " (Client {0})"), FText::AsNumber(World->GetOutermost()->PIEInstanceID - 1));
			break;
		case NM_DedicatedServer:
		case NM_ListenServer:
			PostFix = LOCTEXT("ServerPostfix", " (Server)");
			break;
		case NM_Standalone:
			PostFix = GEditor->bIsSimulatingInEditor ? LOCTEXT("SimulateInEditorPostfix", " (Simulate)") : LOCTEXT("PlayInEditorPostfix", " (PIE)");
			break;
		}
	}
	else if (World->WorldType == EWorldType::Editor)
	{
		PostFix = LOCTEXT("EditorPostfix", " (Editor)");
	}

	return FText::Format(LOCTEXT("WorldFormat", "{0}{1}"), FText::FromString(World->GetFName().GetPlainNameString()), PostFix);
}

TSharedRef<SWidget> SLevelSequenceContextPicker::BuildWorldPickerMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	const ULevelSequenceEditorSettings* Settings = GetDefault<ULevelSequenceEditorSettings>();
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("WorldsHeader", "Worlds"));
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World == nullptr || (Context.WorldType != EWorldType::PIE && Context.WorldType != EWorldType::Editor))
			{
				continue;
			}

			MenuBuilder.AddMenuEntry(
				GetWorldDescription(World),
				FText(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SLevelSequenceContextPicker::OnSetValue, MakeWeakObjectPtr(World)),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SLevelSequenceContextPicker::IsWorldCurrentValue, MakeWeakObjectPtr(World))
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}
	}
	MenuBuilder.EndSection();


	MenuBuilder.BeginSection(NAME_None, LOCTEXT("OptionsHeader", "Options"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AutoBindPIE_Label", "Auto Bind to PIE"),
			LOCTEXT("AutoBindPIE_Tip",   "Automatically binds an active Sequencer window to the current PIE world, if available."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SLevelSequenceContextPicker::ToggleAutoPIE),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SLevelSequenceContextPicker::IsAutoPIEChecked)
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("AutoBindSimulate_Label", "Auto Bind to Simulate"),
			LOCTEXT("AutoBindSimulate_Tip",   "Automatically binds an active Sequencer window to the current Simulate world, if available."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SLevelSequenceContextPicker::ToggleAutoSimulate),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SLevelSequenceContextPicker::IsAutoSimulateChecked)
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE