// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PluginWardenAuthorizer.h"
#include "PluginWardenModule.h"

#include "Containers/Ticker.h"
#include "Async/TaskGraphInterfaces.h"
#include "Editor.h"

#include "Logging/LogMacros.h"

#include "IPortalServiceLocator.h"
#include "Account/IPortalUserLogin.h"
#include "Application/IPortalApplicationWindow.h"

#include "ILauncherPlatform.h"
#include "LauncherPlatformModule.h"

extern TSet<FString> AuthorizedPlugins;

DEFINE_LOG_CATEGORY(PluginWarden);

#define LOCTEXT_NAMESPACE "PluginWardenAuthorization"

FPluginWardenAuthorizer::FPluginWardenAuthorizer(const FText& InPluginFriendlyName, const FString& InPluginItemId, const FString& InPluginOfferId)
	: CurrentState(EPluginAuthorizationState::Initializing)
	, WaitingTime(0)
	, PluginFriendlyName(InPluginFriendlyName)
	, PluginItemId(InPluginItemId)
	, PluginOfferId(InPluginOfferId)
{
	TSharedRef<IPortalServiceLocator> ServiceLocator = GEditor->GetServiceLocator();
	PortalWindowService = ServiceLocator->GetServiceRef<IPortalApplicationWindow>();
	PortalUserService = ServiceLocator->GetServiceRef<IPortalUser>();
	PortalUserLoginService = ServiceLocator->GetServiceRef<IPortalUserLogin>();
}

EPluginAuthorizationState FPluginWardenAuthorizer::UpdateAuthorizationState(float DeltaTime)
{
	EPluginAuthorizationState NewState = CurrentState;

	switch ( CurrentState )
	{
		case EPluginAuthorizationState::Initializing:
		{
			WaitingTime = 0;
			if ( PortalWindowService->IsAvailable() && PortalUserService->IsAvailable() )
			{
				NewState = EPluginAuthorizationState::AuthorizePlugin;
			}
			else
			{
				NewState = EPluginAuthorizationState::StartLauncher;
			}
			break;
		}
		case EPluginAuthorizationState::StartLauncher:
		{
			WaitingTime = 0;
			ILauncherPlatform* LauncherPlatform = FLauncherPlatformModule::Get();

			if (LauncherPlatform != nullptr )
			{
				if ( !FPlatformProcess::IsApplicationRunning(TEXT("EpicGamesLauncher")) &&
						!FPlatformProcess::IsApplicationRunning(TEXT("EpicGamesLauncher-Mac-Shipping")) )
				{
					FOpenLauncherOptions SilentOpen;
					if (LauncherPlatform->OpenLauncher(SilentOpen) )
					{
						NewState = EPluginAuthorizationState::StartLauncher_Waiting;
					}
					else
					{
						NewState = EPluginAuthorizationState::LauncherStartFailed;
					}
				}
				else
				{
					// If the process is found to be running already, move into the next state.
					NewState = EPluginAuthorizationState::StartLauncher_Waiting;
				}
			}
			else
			{
				NewState = EPluginAuthorizationState::LauncherStartFailed;
			}
			break;
		}
		case EPluginAuthorizationState::StartLauncher_Waiting:
		{
			UE_LOG(PluginWarden, Log, TEXT("Waiting for launcher to run for the past %f seconds"), WaitingTime);
			if ( FPlatformProcess::IsApplicationRunning(TEXT("EpicGamesLauncher")) && PortalWindowService->IsAvailable() && PortalUserService->IsAvailable() )
			{
				NewState = EPluginAuthorizationState::AuthorizePlugin;
			}
			else
			{
				WaitingTime += DeltaTime;
			}
			break;
		}
		case EPluginAuthorizationState::AuthorizePlugin:
		{
			WaitingTime = 0;
			EntitlementResult = PortalUserService->IsEntitledToItem(PluginItemId, EEntitlementCacheLevelRequest::Memory);
			NewState = EPluginAuthorizationState::AuthorizePlugin_Waiting;
			break;
		}
		case EPluginAuthorizationState::AuthorizePlugin_Waiting:
		{
			WaitingTime += DeltaTime;

			check(EntitlementResult.GetFuture().IsValid());
			if ( EntitlementResult.GetFuture().IsReady() )
			{
				FPortalUserIsEntitledToItemResult Entitlement = EntitlementResult.GetFuture().Get();
				if ( Entitlement.IsEntitled )
				{
					NewState = EPluginAuthorizationState::Authorized;
				}
				else
				{
					NewState = EPluginAuthorizationState::IsUserSignedIn;
				}
			}

			break;
		}
		case EPluginAuthorizationState::IsUserSignedIn:
		{
			WaitingTime = 0;
			UserDetailsResult = PortalUserService->GetUserDetails();
			NewState = EPluginAuthorizationState::IsUserSignedIn_Waiting;
			break;
		}
		case EPluginAuthorizationState::IsUserSignedIn_Waiting:
		{
			WaitingTime += DeltaTime;

			check(UserDetailsResult.GetFuture().IsValid());
			if ( UserDetailsResult.GetFuture().IsReady() )
			{
				FPortalUserDetails UserDetails = UserDetailsResult.GetFuture().Get();

				if ( UserDetails.IsSignedIn )
				{
					// if the user is signed in, and we're at this stage, we know they are unauthorized.
					NewState = EPluginAuthorizationState::Unauthorized;
				}
				else
				{
					// If they're not signed in, but they were unauthorized, they may have purchased it
					// they may just need to sign-in.
					if ( PortalUserLoginService->IsAvailable() )
					{
						NewState = EPluginAuthorizationState::SigninRequired;
					}
				}
			}

			break;
		}
		case EPluginAuthorizationState::SigninRequired:
		{
			WaitingTime = 0;
			UserSigninResult = PortalUserLoginService->PromptUserForSignIn();
			NewState = EPluginAuthorizationState::SigninRequired_Waiting;
			break;
		}
		case EPluginAuthorizationState::SigninRequired_Waiting:
		{
			// We don't advance the wait time in the sign-required state, as this may take a long time.

			check(UserSigninResult.GetFuture().IsValid());
			if ( UserSigninResult.GetFuture().IsReady() )
			{
				bool IsUserSignedIn = UserSigninResult.GetFuture().Get();
				if ( IsUserSignedIn )
				{
					UserDetailsResult = PortalUserService->GetUserDetails();
					NewState = EPluginAuthorizationState::Signin_Waiting;
					WaitingTime = 0;
				}
				else
				{
					NewState = EPluginAuthorizationState::Unauthorized;
				}
			}

			break;
		}
		// We stay in the Signin_Waiting state until the user is signed in or until they cancel the
		// authorizing plug-in UI.  It would be nice to be able to know if the user closes the sign-in
		// dialog and cancel out of this dialog automatically.
		case EPluginAuthorizationState::Signin_Waiting:
		{
			WaitingTime += DeltaTime;
			UE_LOG(PluginWarden, Log, TEXT("Waiting for sign in for the past %f seconds"), WaitingTime);

			check(UserDetailsResult.GetFuture().IsValid());
			if ( UserDetailsResult.GetFuture().IsReady() )
			{
				FPortalUserDetails UserDetails = UserDetailsResult.GetFuture().Get();

				if ( UserDetails.IsSignedIn )
				{
					// if the user is signed in, and we're at this stage, we know they are unauthorized.
					NewState = EPluginAuthorizationState::AuthorizePlugin;
				}
				else
				{
					NewState = EPluginAuthorizationState::SigninFailed;
				}
			}

			break;
		}
	}

	// If we're in a waiting state, check to see if we're over the timeout period.
	switch ( NewState )
	{
		case EPluginAuthorizationState::StartLauncher_Waiting:
		case EPluginAuthorizationState::AuthorizePlugin_Waiting:
		case EPluginAuthorizationState::IsUserSignedIn_Waiting:
		case EPluginAuthorizationState::SigninRequired_Waiting:
		// We Ignore EPluginAuthorizationState::Signin_Waiting, that state could take forever, the user needs to sign-in or close the dialog.
		{
			const float TimeoutSeconds = 15;
			if ( WaitingTime > TimeoutSeconds )
			{
				NewState = EPluginAuthorizationState::Timeout;
			}
			break;
		}
	}

	CurrentState = NewState;

	return CurrentState;
}

#undef LOCTEXT_NAMESPACE