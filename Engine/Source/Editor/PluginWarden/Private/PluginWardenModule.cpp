// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PluginWardenModule.h"
#include "Async/TaskGraphInterfaces.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"

#include "PluginWardenAuthorizer.h"
#include "SAuthorizingPlugin.h"

IMPLEMENT_MODULE( FPluginWardenModule, PluginWarden );

#define LOCTEXT_NAMESPACE "PluginWarden"

TSet<FString> AuthorizedPlugins;

void FPluginWardenModule::StartupModule()
{

}

void FPluginWardenModule::ShutdownModule()
{

}

void FPluginWardenModule::CheckEntitlementForPlugin(const FText& PluginFriendlyName, const FString& PluginItemId, const FString& PluginOfferId, const FText& UnauthorizedMessageOverride, EUnauthorizedErrorHandling UnauthorizedErrorHandling, TFunction<void()> AuthorizedCallback)
{
	// If we've previously authorized the plug-in, just immediately verify access.
	if ( AuthorizedPlugins.Contains(PluginItemId) )
	{
		AuthorizedCallback();
		return;
	}

	if (IsRunningCommandlet() || GIsRunningUnattendedScript)
	{
		if (RunAuthorizationPipeline(PluginFriendlyName, PluginItemId, PluginOfferId))
		{
			AuthorizedPlugins.Add(PluginItemId);
			AuthorizedCallback();
		}
	}
	else
	{
		// Create the window
		TSharedRef<SWindow> AuthorizingPluginWindow = SNew(SWindow)
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			.HasCloseButton(true)
			.SizingRule(ESizingRule::Autosized)
			.Title(FText::Format(LOCTEXT("EntitlementCheckFormat", "{0} - Entitlement Check"), PluginFriendlyName));

		TSharedRef<SAuthorizingPlugin> PluginAuthPanel = SNew(SAuthorizingPlugin, AuthorizingPluginWindow, PluginFriendlyName, PluginItemId, PluginOfferId, AuthorizedCallback);
		PluginAuthPanel->SetUnauthorizedOverride(UnauthorizedMessageOverride, UnauthorizedErrorHandling);

		AuthorizingPluginWindow->SetContent(PluginAuthPanel);

		FSlateApplication::Get().AddModalWindow(AuthorizingPluginWindow, nullptr);
	}
}

bool FPluginWardenModule::RunAuthorizationPipeline(const FText& PluginFriendlyName, const FString& PluginItemId, const FString& PluginOfferId)
{
	FPluginWardenAuthorizer Authorizer(PluginFriendlyName, PluginItemId, PluginOfferId);

	EPluginAuthorizationState AuthorizationState = EPluginAuthorizationState::Initializing;
	bool bAuthorizationCompleted = false;

	double LastLoopTime = FPlatformTime::Seconds();
	double LastTickTime = FPlatformTime::Seconds();
	const float MinThrottlePeriod = (1.0f / 60.0f); //Throttle the loop to a maximum of 60Hz

	while (!bAuthorizationCompleted)
	{
		//Throttle the loop
		const double CurrentLoopTime = FPlatformTime::Seconds();
		const float SleepTime = MinThrottlePeriod - (float)(CurrentLoopTime-LastLoopTime);
		LastLoopTime = CurrentLoopTime;
		if (SleepTime > 0.0f)
		{
			// Sleep a bit to not eat up all CPU time
			FPlatformProcess::Sleep(SleepTime);
		}

		const double CurrentTickTime = FPlatformTime::Seconds();
		float DeltaTime = (float)(CurrentTickTime - LastTickTime);
		LastTickTime = CurrentTickTime;

		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FTicker::GetCoreTicker().Tick(DeltaTime);

		const EPluginAuthorizationState PreviousState = AuthorizationState;
		AuthorizationState = Authorizer.UpdateAuthorizationState(DeltaTime);

		switch ( AuthorizationState )
		{
			case EPluginAuthorizationState::Canceled:
			case EPluginAuthorizationState::Authorized:
			case EPluginAuthorizationState::Unauthorized:
			case EPluginAuthorizationState::Timeout:
			case EPluginAuthorizationState::LauncherStartFailed:
			case EPluginAuthorizationState::SigninFailed:
			{
				bAuthorizationCompleted = true;
				break;
			}
		}

		if (PreviousState != AuthorizationState)
		{
			switch ( AuthorizationState )
			{
				case EPluginAuthorizationState::StartLauncher_Waiting:
				{
					UE_LOG(PluginWarden, Log, TEXT("Waiting for launcher ..."));
					break;
				}
				case EPluginAuthorizationState::SigninRequired_Waiting:
				{
					UE_LOG(PluginWarden, Log, TEXT("Sign-in required ..."));
					break;
				}
				case EPluginAuthorizationState::Signin_Waiting:
				{
					UE_LOG(PluginWarden, Log, TEXT("Signing in ..."));
					break;
				}
				case EPluginAuthorizationState::AuthorizePlugin_Waiting:
				{
					UE_LOG(PluginWarden, Log, TEXT("Waiting for authorization on plug-in ..."));
					break;
				}
			}
		}
	}

	switch (AuthorizationState)
	{
		case EPluginAuthorizationState::Authorized:
		{
			return true;
		}
		case EPluginAuthorizationState::Unauthorized:
		{
			UE_LOG(PluginWarden, Warning, TEXT("It looks like your Epic Games account doesn't have entitlements for %s."), *PluginFriendlyName.ToString());
			break;
		}
		case EPluginAuthorizationState::Timeout:
		{
			UE_LOG(PluginWarden, Error, TEXT("We were unable to verify your access to the plugin before timing out."));
			break;
		}
		case EPluginAuthorizationState::LauncherStartFailed:
		{
			UE_LOG(PluginWarden, Error, TEXT("Cannot start the launcher. Please open the launcher and sign in manually."));
			break;
		}
		case EPluginAuthorizationState::SigninFailed:
		{
			UE_LOG(PluginWarden, Error, TEXT("Sign-in failed. Please sign in manually through the launcher."));
			break;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
