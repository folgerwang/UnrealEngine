// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystem.h"
#include "Online.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Misc/AutomationTest.h"
#include "Utils/OnlineErrors.data.h"
#include "Utils/OnlineTestCommon.h"

BEGIN_DEFINE_SPEC(FOnlinePresenceSpec, "OnlinePresenceInterface", EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ApplicationContextMask)

IOnlineIdentityPtr OnlineIdentity;
IOnlineFriendsPtr OnlineFriends;
IOnlinePresencePtr OnlinePresence;

FOnlineAccountCredentials AccountCredentials;
FOnlineAccountCredentials FriendAccountCredentials;

FOnlineTestCommon CommonUtils;

const IOnlinePresence::FOnPresenceTaskCompleteDelegate PresenceCompleteDelegate;

// Delegate Handles
FDelegateHandle OnLogoutCompleteDelegateHandle;
FDelegateHandle	OnLoginCompleteDelegateHandle;

END_DEFINE_SPEC(FOnlinePresenceSpec)

void FOnlinePresenceSpec::Define()
{
	TArray<FName> Subsystems = FOnlineTestCommon::GetEnabledTestSubsystems();

	for (int32 Index = 0; Index < Subsystems.Num(); Index++)
	{
		FName SubsystemType = Subsystems[Index];

		Describe(SubsystemType.ToString(), [this, SubsystemType]()
		{
			BeforeEach([this, SubsystemType]()
			{
				CommonUtils = FOnlineTestCommon();
				AccountCredentials = FOnlineTestCommon::GetSubsystemTestAccountCredentials(SubsystemType);
				FriendAccountCredentials = FOnlineTestCommon::GetSubsystemFriendAccountCredentials(SubsystemType);

				OnlineIdentity = Online::GetIdentityInterface(SubsystemType);
				OnlineFriends = Online::GetFriendsInterface(SubsystemType);
				OnlinePresence = Online::GetPresenceInterface(SubsystemType);

				// If OnlineIdentity, OnlineFriends, or OnlinePresence is not valid, the following test, including all other nested BeforeEaches, will not run
				if (!OnlineIdentity.IsValid())
				{
					UE_LOG(LogOnline, Error, TEXT("OSS Automation: Failed to load OnlineIdentity Interface for %s"), *SubsystemType.ToString());
				}

				if (!OnlineFriends.IsValid())
				{
					UE_LOG(LogOnline, Error, TEXT("OSS Automation: Failed to load OnlineFriends Interface for %s"), *SubsystemType.ToString());
				}

				if (!OnlinePresence.IsValid())
				{
					UE_LOG(LogOnline, Error, TEXT("OSS Automation: Failed to load OnlinePresence Interface for %s"), *SubsystemType.ToString());
				}

			});

			Describe("Online Presence", [this, SubsystemType]()
			{
				Describe("SetPresence", [this, SubsystemType]()
				{
					LatentBeforeEach(EAsyncExecution::ThreadPool, [this, SubsystemType](const FDoneDelegate& TestDone)
					{
						OnLoginCompleteDelegateHandle = OnlineIdentity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateLambda([this, TestDone](int32 LoginLocalUserNum, bool bLoginWasSuccessful, const FUniqueNetId& LoginUserId, const FString& LoginError)
						{
							TestEqual("Verify that bLoginWasSuccessful returns as: True", bLoginWasSuccessful, true);
							TestDone.Execute();
						}));

						OnlineIdentity->Login(0, AccountCredentials);
					});

					It("When calling SetPresence with a valid FUniqueNetId and a status that is different from the user's current status, this subsystem correctly changes the user's presence", EAsyncExecution::ThreadPool, [this, SubsystemType]()
					{
						FString TestAccountIdString = FOnlineTestCommon::GetSubsystemTestAccountUniqueId(SubsystemType);
						TSharedPtr<const FUniqueNetId> TestAccountId = OnlineIdentity->CreateUniquePlayerId(TestAccountIdString);

						if (TestAccountId.IsValid())
						{
							TSharedPtr<FOnlineUserPresence> CachedPresence;
							OnlinePresence->GetCachedPresence(*TestAccountId, CachedPresence);

							if (CachedPresence.IsValid())
							{
								TestEqual("Verify that Status.State is: Online", CachedPresence->Status.State, EOnlinePresenceState::Online);

								FOnlineUserPresenceStatus NewPresenceStatus = FOnlineUserPresenceStatus();
								NewPresenceStatus.State = EOnlinePresenceState::DoNotDisturb;

								OnlinePresence->SetPresence(*TestAccountId, NewPresenceStatus);
								OnlinePresence->GetCachedPresence(*TestAccountId, CachedPresence);

								if (CachedPresence.IsValid())
								{
									TestEqual("Verify that login Status.State is: DoNotDisturb", CachedPresence->Status.State, EOnlinePresenceState::DoNotDisturb);
								}
								else
								{
									UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on CachedPresence failed after a second call to OnlinePresence->GetCachedPresence()"));
								}
							}
							else
							{
								UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on CachedPresence failed after a call to OnlinePresence->GetCachedPresence()"));
							}
						}
						else
						{
							UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on TestAccountId failed after a call to OnlineIdentity->CreateUniquePlayerId()"));
						}
					});

					It("When calling SetPresence with a valid FUniqueNetId and a status that is the same as the user's current status, this subsystem does not change the user's presence", EAsyncExecution::ThreadPool, [this, SubsystemType]()
					{
						FString TestAccountIdString = FOnlineTestCommon::GetSubsystemTestAccountUniqueId(SubsystemType);
						TSharedPtr<const FUniqueNetId> TestAccountId = OnlineIdentity->CreateUniquePlayerId(TestAccountIdString);

						if (TestAccountId.IsValid())
						{
							TSharedPtr<FOnlineUserPresence> CachedPresence;
							OnlinePresence->GetCachedPresence(*TestAccountId, CachedPresence);
							
							if (CachedPresence.IsValid())
							{
								TestEqual("Verify that Status.State is: Online", CachedPresence->Status.State, EOnlinePresenceState::Online);

								FOnlineUserPresenceStatus NewPresenceStatus = FOnlineUserPresenceStatus();
								NewPresenceStatus.State = EOnlinePresenceState::Online;

								OnlinePresence->SetPresence(*TestAccountId, NewPresenceStatus);
								OnlinePresence->GetCachedPresence(*TestAccountId, CachedPresence);

								if(CachedPresence.IsValid())
								{
									TestEqual("Verify that login Status.State is: Online", CachedPresence->Status.State, EOnlinePresenceState::Online);
								}
								else
								{
									UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on CachedPresence failed after a second call to OnlinePresence->GetCachedPresence()"));
								}
							}
							else
							{
								UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on CachedPresence failed after a call to OnlinePresence->GetCachedPresence()"));
							}
						}
						else
						{
							UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on TestAccountId failed after a call to OnlineIdentity->CreateUniquePlayerId()"));
						}
					});

					It("When calling SetPresence with a valid FUniqueNetId but an invalid status, this subsystem does not change the user's presence", EAsyncExecution::ThreadPool, [this, SubsystemType]()
					{
						FString TestAccountIdString = FOnlineTestCommon::GetSubsystemTestAccountUniqueId(SubsystemType);
						TSharedPtr<const FUniqueNetId> TestAccountId = OnlineIdentity->CreateUniquePlayerId(TestAccountIdString);

						if (TestAccountId.IsValid())
						{
							TSharedPtr<FOnlineUserPresence> CachedPresence;
							OnlinePresence->GetCachedPresence(*TestAccountId, CachedPresence);

							if (CachedPresence.IsValid())
							{
								TestEqual("Verify that Status.State is: Online", CachedPresence->Status.State, EOnlinePresenceState::Online);

								FOnlineUserPresenceStatus NewPresenceStatus = FOnlineUserPresenceStatus();
								NewPresenceStatus.State = (EOnlinePresenceState::Type)-1;

								OnlinePresence->SetPresence(*TestAccountId, NewPresenceStatus);
								OnlinePresence->GetCachedPresence(*TestAccountId, CachedPresence);

								if (CachedPresence.IsValid())
								{
									TestEqual("Verify that login Status.State is: Online", CachedPresence->Status.State, EOnlinePresenceState::Online);
								}
								else
								{
									UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on CachedPresence failed after a second call to OnlinePresence->GetCachedPresence()"));
								}
							}
							else
							{
								UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on CachedPresence failed after a call to OnlinePresence->GetCachedPresence()"));
							}
						}
						else
						{
							UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on TestAccountId failed after a call to OnlineIdentity->CreateUniquePlayerId()"));
						}
					});

					// TODO: What should happen with an invalid UserId?
					It("When calling SetPresence with a valid status that is different from the user's current status but an invalid FUniqueNetId, this subsystem does not attempt to change any status", EAsyncExecution::ThreadPool, [this, SubsystemType]()
					{
						FString InvalidUserIdString = TEXT(" ");
						TSharedPtr<const FUniqueNetId> InvalidUserId = OnlineIdentity->CreateUniquePlayerId(InvalidUserIdString);

						FOnlineUserPresenceStatus NewPresenceStatus = FOnlineUserPresenceStatus();
						NewPresenceStatus.State = EOnlinePresenceState::DoNotDisturb;

						TSharedPtr<FOnlineUserPresence> CachedPresence;
						OnlinePresence->SetPresence(*InvalidUserId, NewPresenceStatus);
						OnlinePresence->GetCachedPresence(*InvalidUserId, CachedPresence);

						UE_LOG(LogOnline, Error, TEXT("OSS Automation: Test implementation not yet complete. Need to determine expected results with invalid UserId"));
					});

					LatentIt("When calling SetPresence with a status that is different from the user's current status but a valid FUniqueNetId who is logged out, this subsystem does not attempt to change any status", EAsyncExecution::ThreadPool, [this, SubsystemType](const FDoneDelegate TestDone)
					{
						OnLogoutCompleteDelegateHandle = OnlineIdentity->AddOnLogoutCompleteDelegate_Handle(0, FOnLogoutCompleteDelegate::CreateLambda([this, SubsystemType, TestDone](int32 LoggedOutLocalUserNum, bool bLogoutWasSuccessful)
						{
							FString TestAccountIdString = FOnlineTestCommon::GetSubsystemTestAccountUniqueId(SubsystemType);
							TSharedPtr<const FUniqueNetId> TestAccountId = OnlineIdentity->CreateUniquePlayerId(TestAccountIdString);

							if (TestAccountId.IsValid())
							{
								TSharedPtr<FOnlineUserPresence> CachedPresence;
								OnlinePresence->GetCachedPresence(*TestAccountId, CachedPresence);

								if (CachedPresence.IsValid())
								{
									TestEqual("Verify that Status.State is: Offline", CachedPresence->Status.State, EOnlinePresenceState::Offline);

									FOnlineUserPresenceStatus NewPresenceStatus = FOnlineUserPresenceStatus();
									NewPresenceStatus.State = EOnlinePresenceState::Online;

									OnlinePresence->SetPresence(*TestAccountId, NewPresenceStatus);
									OnlinePresence->GetCachedPresence(*TestAccountId, CachedPresence);

									if (CachedPresence.IsValid())
									{
										TestEqual("Verify that login Status.State is: Offline", CachedPresence->Status.State, EOnlinePresenceState::Offline);
										TestDone.Execute();
									}
									else
									{
										UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on CachedPresence failed after a second call to OnlinePresence->GetCachedPresence()"));
										TestDone.Execute();
									}
								}
								else
								{
									UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on CachedPresence failed after a call to OnlinePresence->GetCachedPresence()"));
									TestDone.Execute();
								}
							}
							else
							{
								UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on TestAccountId failed after a call to OnlineIdentity->CreateUniquePlayerId()"));
								TestDone.Execute();
							}
						}));

						OnlineIdentity->Logout(0);
					});
				});

				// TODO: Is it possible to set presence without updating the cache?
				Describe("QueryPresence", [this, SubsystemType]()
				{
					LatentIt("When calling QueryPresence with a valid FUniqueNetId, this subsystem will update the cached presence status of that user", EAsyncExecution::ThreadPool, [this, SubsystemType](const FDoneDelegate& TestDone)
					{
						FString FriendAccountIdString = FOnlineTestCommon::GetSubsystemFriendAccountUniqueId(SubsystemType);
						TSharedPtr<const FUniqueNetId> FriendAccountId = OnlineIdentity->CreateUniquePlayerId(FriendAccountIdString);

						if (FriendAccountId.IsValid())
						{
							OnlinePresence->QueryPresence(*FriendAccountId, IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateLambda([this, FriendAccountIdString, TestDone](const FUniqueNetId& QueryPresenceUniqueId, bool bQueryPresenceWasSuccessful)
							{
								TestEqual("Verify that bQueryPresenceSuccess returns as: True", bQueryPresenceWasSuccessful, true);
								TestEqual("Verify that QueryPresenceUniqueId is the Id that was originally used", QueryPresenceUniqueId.ToString() == FriendAccountIdString, true);
								UE_LOG(LogOnline, Error, TEXT("OSS Automation: Test implementation not yet complete. Need to figure out how to have friend change presence without test account updating cache."));
								TestDone.Execute();
							}));
						}
						else
						{
							UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on TestAccountId failed after a call to OnlineIdentity->CreateUniquePlayerId()"));
							TestDone.Execute();
						}
					});

					LatentIt("When calling QueryPresence with an invalid FUniqueNetId, this subsystem will not query status", EAsyncExecution::ThreadPool, [this, SubsystemType](const FDoneDelegate& TestDone)
					{
						FString InvalidUserIdString = TEXT(" ");
						TSharedPtr<const FUniqueNetId> InvalidUserId = OnlineIdentity->CreateUniquePlayerId(InvalidUserIdString);

						if (InvalidUserId.IsValid())
						{
							OnlinePresence->QueryPresence(*InvalidUserId, IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateLambda([this, InvalidUserId, TestDone](const FUniqueNetId& QueryPresenceUniqueId, bool bQueryPresenceWasSuccessful)
							{
								TestEqual("Verify that bQueryPresenceSuccess returns as: False", bQueryPresenceWasSuccessful, false);
								TestEqual("Verify that QueryPresenceUniqueId is the Id that was originally used", QueryPresenceUniqueId.ToString() == InvalidUserId->ToString(), true);
								TestDone.Execute();
							}));
						}
						else
						{
							UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on InvalidUserId failed after a call to OnlineIdentity->CreateUniquePlayerId()"));
							TestDone.Execute();
						}
					});
				});

				// TODO: Is it possible to set presence without updating the cache?
				Describe("GetCachedPresence", [this, SubsystemType]() 
				{
					LatentBeforeEach(EAsyncExecution::ThreadPool, [this, SubsystemType](const FDoneDelegate& TestDone)
					{
						CommonUtils.AddFriendToTestAccount(OnlineIdentity, OnlineFriends, SubsystemType, TestDone);
					});

					LatentIt("When calling GetCachedPresence with a valid FUniqueNetId after polling for the user's presence data, this subsystem will return that user's presence", EAsyncExecution::ThreadPool, [this, SubsystemType](const FDoneDelegate& TestDone)
					{
						OnLoginCompleteDelegateHandle = OnlineIdentity->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateLambda([this, SubsystemType, TestDone](int32 LoginLocalUserNum, bool bLoginWasSuccessful, const FUniqueNetId& LoginUserId, const FString& LoginError)
						{
							FString FriendIdString = CommonUtils.GetSubsystemFriendAccountUniqueId(SubsystemType);
							TSharedPtr<const FUniqueNetId> FriendId = OnlineIdentity->CreateUniquePlayerId(FriendIdString);

							OnlinePresence->QueryPresence(*FriendId, IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateLambda([this, FriendId, TestDone](const FUniqueNetId& QueryPresenceUniqueId, bool bQueryPresenceWasSuccessful)
							{
								TSharedPtr<FOnlineUserPresence> CachedPresence;
								OnlinePresence->GetCachedPresence(*FriendId, CachedPresence);

								if (CachedPresence.IsValid())
								{
									TestEqual("Verify that Status.State is: Online", CachedPresence->Status.State, EOnlinePresenceState::Online);

									TestDone.Execute();
								}
								else
								{
									UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on CachedPresence failed after a call to OnlinePresence->GetCachedPresence()"));
									TestDone.Execute();
								}
							}));
						}));

						OnlineIdentity->Login(0, AccountCredentials);
					});

					LatentIt("When calling GetCachedPresence with a valid FUniqueNetId without polling for the user's presence data, this subsystem will not return that user's presence", EAsyncExecution::ThreadPool, [this, SubsystemType](const FDoneDelegate& TestDone)
					{
						UE_LOG(LogOnline, Error, TEXT("OSS Automation: Test implementation not yet complete. Need to figure out how to get cached presence without auto calling the query due to logging in."));
						TestDone.Execute();
					});

					LatentIt("When calling GetCachedPresence with an invalid FUniqueNetId, this subsystem will not return a presence status", EAsyncExecution::ThreadPool, [this, SubsystemType](const FDoneDelegate& TestDone)
					{
						FString InvalidUserIdString = TEXT(" ");
						TSharedPtr<const FUniqueNetId> InvalidUserId = OnlineIdentity->CreateUniquePlayerId(InvalidUserIdString);

						if (InvalidUserId.IsValid())
						{
							TSharedPtr<FOnlineUserPresence> CachedPresence;
							OnlinePresence->GetCachedPresence(*InvalidUserId, CachedPresence);

							TestEqual("Verify that CachedPresence pointer is: Invalid", CachedPresence.IsValid(), false);

							TestDone.Execute();
						}
						else
						{
							UE_LOG(LogOnline, Error, TEXT("OSS Automation: IsValid() check on InvalidUserId failed after a call to OnlineIdentity->CreateUniquePlayerId()"));
							TestDone.Execute();
						}
					});

					LatentAfterEach(EAsyncExecution::ThreadPool, [this, SubsystemType](const FDoneDelegate& TestDone)
					{
						OnlineIdentity->ClearOnLoginCompleteDelegate_Handle(0, OnLoginCompleteDelegateHandle);
						OnlineIdentity->ClearOnLogoutCompleteDelegate_Handle(0, OnLogoutCompleteDelegateHandle);
						OnLogoutCompleteDelegateHandle = OnlineIdentity->AddOnLogoutCompleteDelegate_Handle(0, FOnLogoutCompleteDelegate::CreateLambda([this, SubsystemType, TestDone](int32 LoggedOutLocalUserNum, bool bLogoutWasSuccessful)
						{
							CommonUtils.RemoveFriendFromTestAccount(OnlineIdentity, OnlineFriends, SubsystemType, TestDone);
							OnlineIdentity->ClearOnLogoutCompleteDelegate_Handle(0, OnLogoutCompleteDelegateHandle);
						}));

						OnlineIdentity->Logout(0);
					});
				});

				//TODO: Need to get the local userId and the online Id of the person whos presence you want to look up, and App Id
				Describe("GetCachedPresenceForApp", [this, SubsystemType]()
				{
					It("When calling GetCachedPresenceForApp with a valid local user, a valid FUniqueNetId, and a valid AppId, this subsystem returns that user's presence for that app", EAsyncExecution::ThreadPool, [this]()
					{
						UE_LOG(LogOnline, Error, TEXT("OSS Automation: Test implementation not yet complete."));
						//OnlinePresence->GetCachedPresenceForApp();
					});

					It("When calling GetCachedPresenceForApp with a valid local user and a valid FUniqueNetId but an invalid AppId, this subsystem does not return that user's presence", EAsyncExecution::ThreadPool, [this]()
					{
						UE_LOG(LogOnline, Error, TEXT("OSS Automation: Test implementation not yet complete."));
						//OnlinePresence->GetCachedPresenceForApp();
					});

					It("When calling GetCachedPresenceForApp with a valid local user and a valid AppId but an invalid FUniqueNetId, this subsystem does not return that user's presence for that app", EAsyncExecution::ThreadPool, [this]()
					{
						UE_LOG(LogOnline, Error, TEXT("OSS Automation: Test implementation not yet complete."));
						//OnlinePresence->GetCachedPresenceForApp();
					});

					It("When calling GetCachedPresenceForApp with a valid FUniqueNetId and a valid AppId but an invalid local user (-1), this subsystem does not return a presence for that app", EAsyncExecution::ThreadPool, [this]()
					{
						UE_LOG(LogOnline, Error, TEXT("OSS Automation: Test implementation not yet complete."));
						//OnlinePresence->GetCachedPresenceForApp();
					});
				});
			});

			AfterEach(EAsyncExecution::ThreadPool, [this]()
			{
				// Clean up Identity
				if (OnlineIdentity.IsValid())
				{
					if (OnlineIdentity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
					{
						OnlineIdentity->Logout(0);
					}

					OnlineIdentity->ClearOnLoginCompleteDelegate_Handle(0, OnLoginCompleteDelegateHandle);
					OnlineIdentity->ClearOnLogoutCompleteDelegate_Handle(0, OnLogoutCompleteDelegateHandle);
					OnlineIdentity = nullptr;
				}

				// Clean up Friends
				if (OnlineFriends.IsValid())
				{
					OnlineFriends = nullptr;
				}

				// Clean up Presence
				if (OnlinePresence.IsValid())
				{
					OnlinePresence = nullptr;
				}
			});
		});
	}
}
