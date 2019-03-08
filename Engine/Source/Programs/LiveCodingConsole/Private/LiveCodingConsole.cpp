// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LiveCodingConsole.h"
#include "RequiredProgramMainCPPInclude.h"
#include "Framework/Application/SlateApplication.h"
#include "StandaloneRenderer.h"
#include "LiveCodingConsoleStyle.h"
#include "HAL/PlatformProcess.h"
#include "SLogWidget.h"
#include "Modules/ModuleManager.h"
#include "ILiveCodingServer.h"
#include "Features/IModularFeatures.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Windows/WindowsHWrapper.h"
#include "Misc/MonitoredProcess.h"
#include "LiveCodingManifest.h"

#define LOCTEXT_NAMESPACE "LiveCodingConsole"

IMPLEMENT_APPLICATION(LiveCodingConsole, "LiveCodingConsole");

static void OnRequestExit()
{
	GIsRequestingExit = true;
}

class FLiveCodingConsoleApp
{
private:
	FCriticalSection CriticalSection;
	FSlateApplication& Slate;
	ILiveCodingServer& Server;
	TSharedPtr<SLogWidget> LogWidget;
	TSharedPtr<SWindow> Window;
	TSharedPtr<SNotificationItem> CompileNotification;
	TArray<FSimpleDelegate> MainThreadTasks;
	bool bRequestCancel;
	FDateTime LastPatchTime;
	FDateTime NextPatchStartTime;

public:
	FLiveCodingConsoleApp(FSlateApplication& InSlate, ILiveCodingServer& InServer)
		: Slate(InSlate)
		, Server(InServer)
		, bRequestCancel(false)
		, LastPatchTime(FDateTime::MinValue())
		, NextPatchStartTime(FDateTime::MinValue())
	{
	}

	void Run()
	{
		// open up the app window	
		LogWidget = SNew(SLogWidget);

		// Create the window
		Window = 
			SNew(SWindow)
			.Title(GetWindowTitle())
			.ClientSize(FVector2D(1200.0f, 600.0f))
			.ActivationPolicy(EWindowActivationPolicy::Never)
			.IsInitiallyMaximized(false)
			[
				LogWidget.ToSharedRef()
			];

		// Add the window without showing it
		Slate.AddWindow(Window.ToSharedRef(), false);

		// Show the window without stealling focus
		if (!FParse::Param(FCommandLine::Get(), TEXT("Hidden")))
		{
			HWND ForegroundWindow = GetForegroundWindow();
			if (ForegroundWindow != nullptr)
			{
				::SetWindowPos((HWND)Window->GetNativeWindow()->GetOSWindowHandle(), ForegroundWindow, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			Window->ShowWindow();
		}

		// Get the server interface
		Server.GetBringToFrontDelegate().BindRaw(this, &FLiveCodingConsoleApp::BringToFrontAsync);
		Server.GetLogOutputDelegate().BindRaw(this, &FLiveCodingConsoleApp::AppendLogLine);
		Server.GetShowConsoleDelegate().BindRaw(this, &FLiveCodingConsoleApp::BringToFrontAsync);
		Server.GetSetVisibleDelegate().BindRaw(this, &FLiveCodingConsoleApp::SetVisibleAsync);
		Server.GetCompileDelegate().BindRaw(this, &FLiveCodingConsoleApp::CompilePatch);
		Server.GetCompileStartedDelegate().BindRaw(this, &FLiveCodingConsoleApp::OnCompileStartedAsync);
		Server.GetCompileFinishedDelegate().BindLambda([this](ELiveCodingResult Result, const wchar_t* Message){ OnCompileFinishedAsync(Result, Message); });
		Server.GetStatusChangeDelegate().BindLambda([this](const wchar_t* Status){ OnStatusChangedAsync(Status); });

		// Start the server
		FString ProcessGroupName;
		if (FParse::Value(FCommandLine::Get(), TEXT("-Group="), ProcessGroupName))
		{
			Server.Start(*ProcessGroupName);
			Window->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride::CreateLambda([this](const TSharedRef<SWindow>&){ SetVisible(false); }));
		}
		else
		{
			LogWidget->AppendLine(GetLogColor(ELiveCodingLogVerbosity::Warning), TEXT("Running in standalone mode. Server is disabled."));
		}

		// Setting focus seems to have to happen after the Window has been added
		Slate.ClearKeyboardFocus(EFocusCause::Cleared);

		// loop until the app is ready to quit
		while (!GIsRequestingExit)
		{
			Slate.PumpMessages();
			Slate.Tick();

			FPlatformProcess::Sleep(1.0f / 30.0f);

			// Execute all the main thread tasks
			FScopeLock Lock(&CriticalSection);
			for (FSimpleDelegate& MainThreadTask : MainThreadTasks)
			{
				MainThreadTask.Execute();
			}
			MainThreadTasks.Empty();
		}

		// Make sure the window is hidden, because it might take a while for the background thread to finish.
		Window->HideWindow();

		// Shutdown the server
		Server.Stop();
	}

private:
	FText GetWindowTitle()
	{
		FString ProjectName;
		if (FParse::Value(FCommandLine::Get(), TEXT("-ProjectName="), ProjectName))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ProjectName"), FText::FromString(ProjectName));
			return FText::Format(LOCTEXT("WindowTitleWithProject", "{ProjectName} - Live Coding"), Args);
		}
		return LOCTEXT("WindowTitle", "Live Coding");
	}

	void BringToFrontAsync()
	{
		FScopeLock Lock(&CriticalSection);
		MainThreadTasks.Add(FSimpleDelegate::CreateRaw(this, &FLiveCodingConsoleApp::BringToFront));
	}

	void BringToFront()
	{
		HWND WindowHandle = (HWND)Window->GetNativeWindow()->GetOSWindowHandle();
		if (IsIconic(WindowHandle))
		{
			ShowWindow(WindowHandle, SW_RESTORE);
		}
		::SetWindowPos(WindowHandle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		::SetWindowPos(WindowHandle, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}

	static FSlateColor GetLogColor(ELiveCodingLogVerbosity Verbosity)
	{
		switch (Verbosity)
		{
		case ELiveCodingLogVerbosity::Success:
			return FSlateColor(FLinearColor::Green);
		case ELiveCodingLogVerbosity::Failure:
			return FSlateColor(FLinearColor::Red);
		case ELiveCodingLogVerbosity::Warning:
			return FSlateColor(FLinearColor::Yellow);
		default:
			return FSlateColor(FLinearColor::Gray);
		}
	}

	void AppendLogLine(ELiveCodingLogVerbosity Verbosity, const TCHAR* Text)
	{
		// SLogWidget has its own synchronization
		LogWidget->AppendLine(GetLogColor(Verbosity), MoveTemp(Text));
	}

	bool CompilePatch(const TArray<FString>& Targets, TMap<FString, TArray<FString>>& ModuleToObjectFiles)
	{
		// Update the compile start time. This gets copied into the last patch time once a patch has been confirmed to have been applied.
		NextPatchStartTime = FDateTime::UtcNow();

		// Get the UBT path
		FString Executable = FPaths::EngineDir() / TEXT("Binaries/DotNET/UnrealBuildTool.exe");
		FPaths::MakePlatformFilename(Executable);

		// Build the argument list
		FString Arguments;
		for (const FString& Target : Targets)
		{
			Arguments += FString::Printf(TEXT("-Target=\"%s\" "), *Target.Replace(TEXT("\""), TEXT("\"\"")));
		}

		FString ManifestFileName = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir() / TEXT("LiveCoding.json"));
		Arguments += FString::Printf(TEXT("-LiveCoding -LiveCodingManifest=\"%s\" -WaitMutex"), *ManifestFileName);

		AppendLogLine(ELiveCodingLogVerbosity::Info, *FString::Printf(TEXT("Running %s %s"), *Executable, *Arguments));

		// Spawn UBT and wait for it to complete (or the compile button to be pressed)
		FMonitoredProcess Process(*Executable, *Arguments, true);
		Process.OnOutput().BindLambda([this](const FString& Text){ AppendLogLine(ELiveCodingLogVerbosity::Info, *Text); });
		Process.Launch();
		while(Process.Update())
		{
			if (HasCancelledBuild())
			{
				AppendLogLine(ELiveCodingLogVerbosity::Warning, TEXT("Build cancelled."));
				return false;
			}
			FPlatformProcess::Sleep(0.1f);
		}

		if (Process.GetReturnCode() != 0)
		{
			AppendLogLine(ELiveCodingLogVerbosity::Failure, TEXT("Build failed."));
			return false;
		}

		// Read the output manifest
		FString ManifestFailReason;
		FLiveCodingManifest Manifest;
		if (!Manifest.Read(*ManifestFileName, ManifestFailReason))
		{
			AppendLogLine(ELiveCodingLogVerbosity::Failure, *ManifestFailReason);
			return false;
		}

		// Override the linker path
		Server.SetLinkerPath(*Manifest.LinkerPath);

		// Strip out all the files that haven't been modified
		IFileManager& FileManager = IFileManager::Get();
		for(TPair<FString, TArray<FString>>& Pair : Manifest.BinaryToObjectFiles)
		{
			FDateTime MinTimeStamp = FileManager.GetTimeStamp(*Pair.Key);
			if(LastPatchTime > MinTimeStamp)
			{
				MinTimeStamp = LastPatchTime;
			}

			for (const FString& ObjectFileName : Pair.Value)
			{
				if (FileManager.GetTimeStamp(*ObjectFileName) > MinTimeStamp)
				{
					ModuleToObjectFiles.FindOrAdd(Pair.Key).Add(ObjectFileName);
				}
			}
		}
		return true;
	}

	void CancelBuild()
	{
		FScopeLock Lock(&CriticalSection);
		bRequestCancel = true;
	}

	bool HasCancelledBuild()
	{
		FScopeLock Lock(&CriticalSection);
		return bRequestCancel;
	}

	void SetVisibleAsync(bool bVisible)
	{
		FScopeLock Lock(&CriticalSection);
		MainThreadTasks.Add(FSimpleDelegate::CreateLambda([this, bVisible](){ SetVisible(bVisible); }));
	}

	void SetVisible(bool bVisible)
	{
		if (bVisible)
		{
			if (!Window->IsVisible())
			{
				Window->ShowWindow();
			}
		}
		else
		{
			if (Window->IsVisible())
			{
				Window->HideWindow();
			}
		}
	}

	void ShowConsole()
	{
		SetVisible(true);
		BringToFront();
	}

	void OnCompileStartedAsync()
	{
		FScopeLock Lock(&CriticalSection);
		MainThreadTasks.Add(FSimpleDelegate::CreateRaw(this, &FLiveCodingConsoleApp::OnCompileStarted));
		bRequestCancel = false;
		NextPatchStartTime = FDateTime::UtcNow();
	}

	void OnCompileStarted()
	{
		if (!CompileNotification.IsValid())
		{
			ShowConsole();

			FNotificationInfo Info(FText::FromString(TEXT("Starting...")));
			Info.bFireAndForget = false;
			Info.FadeOutDuration = 0.0f;
			Info.ExpireDuration = 0.0f;
			Info.Hyperlink = FSimpleDelegate::CreateRaw(this, &FLiveCodingConsoleApp::ShowConsole);
			Info.HyperlinkText = LOCTEXT("BuildStatusShowConsole", "Show Console");
			Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("BuildStatusCancel", "Cancel"), FText(), FSimpleDelegate::CreateRaw(this, &FLiveCodingConsoleApp::CancelBuild), SNotificationItem::CS_Pending));

			CompileNotification = FSlateNotificationManager::Get().AddNotification(Info);
			CompileNotification->SetCompletionState(SNotificationItem::CS_Pending);
		}
	}

	void OnCompileFinishedAsync(ELiveCodingResult Result, const FString& Status)
	{
		FScopeLock Lock(&CriticalSection);
		MainThreadTasks.Add(FSimpleDelegate::CreateLambda([this, Result, Status](){ OnCompileFinished(Result, Status); }));

		if (Result == ELiveCodingResult::Success)
		{
			LastPatchTime = NextPatchStartTime;
		}
	}

	void OnCompileFinished(ELiveCodingResult Result, const FString& Status)
	{
		if(CompileNotification.IsValid())
		{
			if(Result == ELiveCodingResult::Success)
			{
				CompileNotification->SetText(FText::FromString(Status));
				CompileNotification->SetCompletionState(SNotificationItem::CS_Success);
				CompileNotification->SetExpireDuration(1.5f);
				CompileNotification->SetFadeOutDuration(0.4f);
			}
			else if(HasCancelledBuild())
			{
				CompileNotification->SetExpireDuration(0.0f);
				CompileNotification->SetFadeOutDuration(0.1f);
			}
			else
			{
				CompileNotification->SetText(FText::FromString(Status));
				CompileNotification->SetCompletionState(SNotificationItem::CS_Fail);
				CompileNotification->SetExpireDuration(5.0f);
				CompileNotification->SetFadeOutDuration(2.0f);
			}
			CompileNotification->ExpireAndFadeout();
			CompileNotification.Reset();
		}
	}

	void OnStatusChangedAsync(const FString& Status)
	{
		FScopeLock Lock(&CriticalSection);
		MainThreadTasks.Add(FSimpleDelegate::CreateLambda([this, Status](){ OnCompileStatusChanged(Status); }));
	}

	void OnCompileStatusChanged(const FString& Status)
	{
		if (CompileNotification.IsValid())
		{
			CompileNotification->SetText(FText::FromString(Status));
		}
	}
};

bool LiveCodingConsoleMain(const TCHAR* CmdLine)
{
	// start up the main loop
	GEngineLoop.PreInit(CmdLine);
	check(GConfig && GConfig->IsReadyForUse());

	{
		// create the platform slate application (what FSlateApplication::Get() returns)
		TSharedRef<FSlateApplication> Slate = FSlateApplication::Create(MakeShareable(FPlatformApplicationMisc::CreateApplication()));

		{
			// initialize renderer
			TSharedRef<FSlateRenderer> SlateRenderer = GetStandardStandaloneRenderer();

			// Try to initialize the renderer. It's possible that we launched when the driver crashed so try a few times before giving up.
			bool bRendererInitialized = Slate->InitializeRenderer(SlateRenderer, true);
			if (!bRendererInitialized)
			{
				// Close down the Slate application
				FSlateApplication::Shutdown();
				return false;
			}

			// set the normal UE4 GIsRequestingExit when outer frame is closed
			Slate->SetExitRequestedHandler(FSimpleDelegate::CreateStatic(&OnRequestExit));

			// Prepare the custom Slate styles
			FLiveCodingConsoleStyle::Initialize();

			// Set the icon
			Slate->SetAppIcon(FLiveCodingConsoleStyle::Get().GetBrush("AppIcon"));

			// Load the server module
			FModuleManager::Get().LoadModuleChecked<ILiveCodingServer>(TEXT("LiveCodingServer"));
			ILiveCodingServer& Server = IModularFeatures::Get().GetModularFeature<ILiveCodingServer>(LIVE_CODING_SERVER_FEATURE_NAME);

			// Run the inner application loop
			FLiveCodingConsoleApp App(Slate.Get(), Server);
			App.Run();

			// Unload the server module
			FModuleManager::Get().UnloadModule(TEXT("LiveCodingServer"));

			// Clean up the custom styles
			FLiveCodingConsoleStyle::Shutdown();
		}

		// Close down the Slate application
		FSlateApplication::Shutdown();
	}

	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FEngineLoop::AppExit();
	return true;
}

int WINAPI WinMain(HINSTANCE hCurrInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	hInstance = hCurrInstance;
	return LiveCodingConsoleMain(GetCommandLineW())? 0 : 1;
}

#undef LOCTEXT_NAMESPACE
