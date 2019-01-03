// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditorPythonExecuter.h"

#include "AssetRegistryModule.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "IAssetRegistry.h"
#include "Modules/ModuleManager.h"
#include "Stats/Stats.h"
#include "TickableEditorObject.h"

#include "EditorStyleSet.h"
#include "Interfaces/IMainFrameModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogEditorPythonExecuter, Log, All);

#define LOCTEXT_NAMESPACE "EditorPythonRunner"

namespace InternalEditorPythonRunner
{
	class SExecutingDialog;
	class FExecuterTickable;

	TSharedPtr<SExecutingDialog> ExecuterDialog = nullptr;
	FExecuterTickable* Executer = nullptr;

	/*
	 * Show a window to tell the user what is going on
	 */
	class SExecutingDialog : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SExecutingDialog) {}
		SLATE_END_ARGS()

		/** Constructs this widget with InArgs */
		void Construct(const FArguments& InArgs)
		{
			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(4, 8, 4, 4))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(16, 0)
					.FillHeight(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0, 0, 0, 8)
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("WaitPythonExecuting", "Please wait while Python is executing."))
						]
					]

					// Cancel button
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 4)
					.HAlign(HAlign_Right)
					[
						SNew(SButton)
						.OnClicked(this, &SExecutingDialog::CancelClicked)
						.Text(LOCTEXT("CancelButton", "Cancel"))
					]
				]
			];
		}

		/** Opens the dialog in a new window */
		static void OpenDialog()
		{
			TSharedRef<SWindow> PythonWindow = SNew(SWindow)
				.Title(LOCTEXT("PythonWindowsDialog", "Executing Python..."))
				.SizingRule(ESizingRule::Autosized)
				.SupportsMaximize(false)
				.SupportsMinimize(false)
				[
					SAssignNew(ExecuterDialog, SExecutingDialog)
				];

			IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));

			if (MainFrameModule.GetParentWindow().IsValid())
			{
				FSlateApplication::Get().AddWindowAsNativeChild(PythonWindow, MainFrameModule.GetParentWindow().ToSharedRef());
			}
			else
			{
				FSlateApplication::Get().AddWindow(PythonWindow);
			}
		}

		/** Closes the dialog. */
		void CloseDialog()
		{
			TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());

			if (Window.IsValid())
			{
				Window->RequestDestroyWindow();
			}
		}

	private:
		/** Handler for when "Cancel" is clicked */
		FReply CancelClicked()
		{
			if (GEditor)
			{
				GEditor->CloseEditor();
			}

			CloseDialog();
			return FReply::Handled();
		}
	};

	/*
	 * Tick until we are ready.
	 * We could also listen to events like FAssetRegistryModule::FileLoadedEvent but Python script can possibly be executed on multiple frames and we need to wait until it is completed to return.
	 * And we can't close the editor on the same frame that we execute the Python script because a full tick needs to happen first.
	 */
	class FExecuterTickable : FTickableEditorObject
	{
	public:
		FExecuterTickable(const FString& InFileName)
			: FileName(InFileName)
		{
			GIsRunningUnattendedScript = true; // Prevent all dialog modal from showing up
		}

		virtual void Tick(float DeltaTime) override
		{
			if (bIsRunning)
			{
				CloseEditor();
			}

			// if we are here the editor is ready.
			if (!GIsRequestingExit && !bIsRunning && GWorld && GEngine && GEditor && DeltaTime > 0 && GLog)
			{
				if (!FileName.IsEmpty())
				{
					// check if the AssetRegistryModule is ready
					FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
					if (!AssetRegistryModule.Get().IsLoadingAssets())
					{
						bIsRunning = true;

						// Try and run the command
						if (!GEngine->Exec(GWorld, *FString::Printf(TEXT("PY \"%s\""), *FileName), *GLog))
						{
							UE_LOG(LogEditorPythonExecuter, Error, TEXT("-ExecutePythonScript cannot be used without a valid Python Script Plugin. Ensure the plugin is enabled and wasn't compiled with Python support stubbed out."));
						}
					}
				}
				else
				{
					CloseEditor();
				}
			}
		}

		virtual TStatId GetStatId() const override
		{
			return TStatId();
		}

		void CloseEditor()
		{
			if (ExecuterDialog.IsValid())
			{
				ExecuterDialog->CloseDialog();
				ExecuterDialog = nullptr;
			}
			if (GEngine)
			{
				GEngine->HandleDeferCommand(TEXT("QUIT_EDITOR"), *GLog); // Defer close the editor
			}
		}

		FString FileName;
		bool bIsRunning = false;
	};
}

void FEditorPythonExecuter::OnStartupModule()
{
	FString FileValue;
	if (FParse::Value(FCommandLine::Get(), TEXT("ExecutePythonScript="), FileValue))
	{
		if (!GIsEditor)
		{
			UE_LOG(LogEditorPythonExecuter, Error, TEXT("-ExecutePythonScript cannot be used outside of the editor."));
		}
		else if (IsRunningCommandlet())
		{
			UE_LOG(LogEditorPythonExecuter, Error, TEXT("-ExecutePythonScript cannot be used by a commandlet."));
		}
		else
		{
			InternalEditorPythonRunner::Executer = new InternalEditorPythonRunner::FExecuterTickable(MoveTemp(FileValue));
			InternalEditorPythonRunner::SExecutingDialog::OpenDialog();
		}
	}
}

void FEditorPythonExecuter::OnShutdownModule()
{
	if (InternalEditorPythonRunner::ExecuterDialog.IsValid())
	{
		InternalEditorPythonRunner::ExecuterDialog->CloseDialog();
		InternalEditorPythonRunner::ExecuterDialog = nullptr;
	}

	delete InternalEditorPythonRunner::Executer;
	InternalEditorPythonRunner::Executer = nullptr;
}

#undef LOCTEXT_NAMESPACE
