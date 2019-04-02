// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD.h"
#include "OculusHMDPrivateRHI.h"

#include "EngineAnalytics.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "AnalyticsEventAttribute.h"
#include "Slate/SceneViewport.h"
#include "PostProcess/PostProcessHMD.h"
#include "PostProcess/SceneRenderTargets.h"
#include "HardwareInfo.h"
#include "ScreenRendering.h"
#include "GameFramework/PlayerController.h"
#include "Math/UnrealMathUtility.h"
#include "Math/TranslationMatrix.h"
#include "Widgets/SViewport.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Canvas.h"
#include "Engine/GameEngine.h"
#include "Misc/CoreDelegates.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Misc/EngineVersion.h"
#include "ClearQuad.h"
#include "DynamicResolutionState.h"
#include "DynamicResolutionProxy.h"
#include "OculusHMDRuntimeSettings.h"

#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#include "Android/AndroidEGL.h"
#include "Android/AndroidOpenGL.h"
#include "Android/AndroidApplication.h"
#include "HAL/IConsoleManager.h"
#endif
#include "Runtime/UtilityShaders/Public/OculusShaders.h"
#include "PipelineStateCache.h"

#include "IOculusMRModule.h"
#if OCULUS_MR_SUPPORTED_PLATFORMS
#include "OVR_Plugin_MixedReality.h"
#endif

#if WITH_EDITOR
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#endif

#if !UE_BUILD_SHIPPING
#include "Debug/DebugDrawService.h"
#endif

#if OCULUS_HMD_SUPPORTED_PLATFORMS

#define OCULUS_PAUSED_IDLE_FPS 10

namespace OculusHMD
{

#if !UE_BUILD_SHIPPING
	static void __cdecl OvrpLogCallback(ovrpLogLevel level, const char* message)
	{
		FString tbuf = ANSI_TO_TCHAR(message);
		const TCHAR* levelStr = TEXT("");
		switch (level)
		{
		case ovrpLogLevel_Debug: levelStr = TEXT(" Debug:"); break;
		case ovrpLogLevel_Info:  levelStr = TEXT(" Info:"); break;
		case ovrpLogLevel_Error: levelStr = TEXT(" Error:"); break;
		}

		GLog->Logf(TEXT("OCULUS:%s %s"), levelStr, *tbuf);
	}
#endif // !UE_BUILD_SHIPPING

	//-------------------------------------------------------------------------------------------------
	// FOculusHMD
	//-------------------------------------------------------------------------------------------------

	const FName FOculusHMD::OculusSystemName(TEXT("OculusHMD"));

	FName FOculusHMD::GetSystemName() const
		{
		return OculusSystemName;
	}


	FString FOculusHMD::GetVersionString() const
	{
		const char* Version;

		if (OVRP_FAILURE(ovrp_GetVersion2(&Version)))
		{
			Version = "Unknown";
		}

		return FString::Printf(TEXT("%s, OVRPlugin: %s"), *FEngineVersion::Current().ToString(), UTF8_TO_TCHAR(Version));
	}


	bool FOculusHMD::DoesSupportPositionalTracking() const
	{
		ovrpBool trackingPositionSupported;
		return OVRP_SUCCESS(ovrp_GetTrackingPositionSupported2(&trackingPositionSupported)) && trackingPositionSupported;
	}


	bool FOculusHMD::HasValidTrackingPosition()
	{
		ovrpBool nodePositionTracked;
		return OVRP_SUCCESS(ovrp_GetNodePositionTracked2(ovrpNode_Head, &nodePositionTracked)) && nodePositionTracked;
	}


	struct TrackedDevice
	{
		ovrpNode Node;
		EXRTrackedDeviceType Type;
	};

	static TrackedDevice TrackedDevices[] =
		{
		{ ovrpNode_Head, EXRTrackedDeviceType::HeadMountedDisplay },
		{ ovrpNode_HandLeft, EXRTrackedDeviceType::Controller },
		{ ovrpNode_HandRight, EXRTrackedDeviceType::Controller },
		{ ovrpNode_TrackerZero, EXRTrackedDeviceType::TrackingReference },
		{ ovrpNode_TrackerOne, EXRTrackedDeviceType::TrackingReference },
		{ ovrpNode_TrackerTwo, EXRTrackedDeviceType::TrackingReference },
		{ ovrpNode_TrackerThree, EXRTrackedDeviceType::TrackingReference },
		{ ovrpNode_DeviceObjectZero, EXRTrackedDeviceType::Other },
	};

	static uint32 TrackedDeviceCount = sizeof(TrackedDevices) / sizeof(TrackedDevices[0]);

	bool FOculusHMD::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type)
	{
		CheckInGameThread();

		for (uint32 TrackedDeviceId = 0; TrackedDeviceId < TrackedDeviceCount; TrackedDeviceId++)
		{
			if (Type == EXRTrackedDeviceType::Any || Type == TrackedDevices[TrackedDeviceId].Type)
			{
				ovrpBool nodePresent;

				ovrpNode Node = TrackedDevices[TrackedDeviceId].Node;
				if (OVRP_SUCCESS(ovrp_GetNodePresent2(Node, &nodePresent)) && nodePresent)
				{
					const int32 ExternalDeviceId = OculusHMD::ToExternalDeviceId(Node);
					OutDevices.Add(ExternalDeviceId);
				}
			}
		}

		return true;
	}

	void FOculusHMD::UpdateRTPoses()
	{
		CheckInRenderThread();
		FGameFrame* CurrentFrame = GetFrame_RenderThread();
		if (CurrentFrame)
		{
			if (!CurrentFrame->Flags.bRTLateUpdateDone)
			{
				ovrp_Update3(ovrpStep_Render, CurrentFrame->FrameNumber, 0.0);
				CurrentFrame->Flags.bRTLateUpdateDone = true;
			}

		}
		// else, Frame_RenderThread has already been reset/rendered (or not created yet).
		// This can happen when DoEnableStereo() is called, as SetViewportSize (which it calls) enques a render
		// immediately - meaning two render frames were enqueued in the span of one game tick.
	}

	bool FOculusHMD::GetCurrentPose(int32 InDeviceId, FQuat& OutOrientation, FVector& OutPosition)
	{
		OutOrientation = FQuat::Identity;
		OutPosition = FVector::ZeroVector;

		if ((size_t) InDeviceId >= TrackedDeviceCount)
		{
			return false;
		}

		ovrpNode Node = OculusHMD::ToOvrpNode(InDeviceId);
		const FSettings* CurrentSettings;
		FGameFrame* CurrentFrame;

		if (InRenderThread())
		{
			CurrentSettings = GetSettings_RenderThread();
			CurrentFrame = GetFrame_RenderThread();
			UpdateRTPoses();
		}
		else if(InGameThread())
		{
			CurrentSettings = GetSettings();
			CurrentFrame = NextFrameToRender.Get();
		}
		else
		{
			return false;
		}

		if (!CurrentSettings || !CurrentFrame)
		{
			return false;
		}

		ovrpPoseStatef PoseState;
		FPose Pose;

		if (OVRP_FAILURE(ovrp_GetNodePoseState3(ovrpStep_Render, CurrentFrame->FrameNumber, Node, &PoseState)) ||
			!ConvertPose_Internal(PoseState.Pose, Pose, CurrentSettings, CurrentFrame->WorldToMetersScale))
		{
			return false;
		}

		OutPosition = Pose.Position;
		OutOrientation = Pose.Orientation;
		return true;
	}


	bool FOculusHMD::GetRelativeEyePose(int32 InDeviceId, EStereoscopicPass InEye, FQuat& OutOrientation, FVector& OutPosition)
	{
		OutOrientation = FQuat::Identity;
		OutPosition = FVector::ZeroVector;

		if (InDeviceId != HMDDeviceId)
		{
			return false;
		}

		ovrpNode Node;

		switch (InEye)
		{
		case eSSP_LEFT_EYE:
			Node = ovrpNode_EyeLeft;
			break;
		case eSSP_RIGHT_EYE:
			Node = ovrpNode_EyeRight;
			break;
		default:
			return false;
		}

		const FSettings* CurrentSettings;
		FGameFrame* CurrentFrame;

		if (InRenderThread())
		{
			CurrentSettings = GetSettings_RenderThread();
			CurrentFrame = GetFrame_RenderThread();
			UpdateRTPoses();
		}
		else if(InGameThread())
		{
			CurrentSettings = GetSettings();
			CurrentFrame = NextFrameToRender.Get();
		}
		else
		{
			return false;
		}

		if (!CurrentSettings || !CurrentFrame)
		{
			return false;
		}

		ovrpPoseStatef HmdPoseState, EyePoseState;

		if (OVRP_FAILURE(ovrp_GetNodePoseState3(ovrpStep_Render, CurrentFrame->FrameNumber, ovrpNode_Head, &HmdPoseState)) ||
			OVRP_FAILURE(ovrp_GetNodePoseState3(ovrpStep_Render, CurrentFrame->FrameNumber, Node, &EyePoseState)))
		{
			return false;
		}

		FPose HmdPose, EyePose;
		HmdPose.Orientation = ToFQuat(HmdPoseState.Pose.Orientation);
		HmdPose.Position = ToFVector(HmdPoseState.Pose.Position) * CurrentFrame->WorldToMetersScale;
		EyePose.Orientation = ToFQuat(EyePoseState.Pose.Orientation);
		EyePose.Position = ToFVector(EyePoseState.Pose.Position) * CurrentFrame->WorldToMetersScale;

		FQuat HmdOrientationInv = HmdPose.Orientation.Inverse();
		OutOrientation = HmdOrientationInv * EyePose.Orientation;
		OutOrientation.Normalize();
		OutPosition = HmdOrientationInv.RotateVector(EyePose.Position - HmdPose.Position);
		return true;
	}


	bool FOculusHMD::GetTrackingSensorProperties(int32 InDeviceId, FQuat& OutOrientation, FVector& OutPosition, FXRSensorProperties& OutSensorProperties)
	{
		CheckInGameThread();

		if ((size_t) InDeviceId >= TrackedDeviceCount)
		{
			return false;
		}

		ovrpNode Node = OculusHMD::ToOvrpNode(InDeviceId);
		ovrpPoseStatef PoseState;
		FPose Pose;
		ovrpFrustum2f Frustum;

		if (OVRP_FAILURE(ovrp_GetNodePoseState3(ovrpStep_Render, OVRP_CURRENT_FRAMEINDEX, Node, &PoseState)) || !ConvertPose(PoseState.Pose, Pose) ||
			OVRP_FAILURE(ovrp_GetNodeFrustum2(Node, &Frustum)))
		{
			return false;
		}

		OutPosition = Pose.Position;
		OutOrientation = Pose.Orientation;
		OutSensorProperties.LeftFOV = FMath::RadiansToDegrees(FMath::Atan(Frustum.Fov.LeftTan));
		OutSensorProperties.RightFOV = FMath::RadiansToDegrees(FMath::Atan(Frustum.Fov.RightTan));
		OutSensorProperties.TopFOV = FMath::RadiansToDegrees(FMath::Atan(Frustum.Fov.UpTan));
		OutSensorProperties.BottomFOV = FMath::RadiansToDegrees(FMath::Atan(Frustum.Fov.DownTan));
		OutSensorProperties.NearPlane = Frustum.zNear * Frame->WorldToMetersScale;
		OutSensorProperties.FarPlane = Frustum.zFar * Frame->WorldToMetersScale;
		OutSensorProperties.CameraDistance = 1.0f * Frame->WorldToMetersScale;
		return true;
	}


	void FOculusHMD::SetTrackingOrigin(EHMDTrackingOrigin::Type InOrigin)
	{
		TrackingOrigin = InOrigin;
		ovrpTrackingOrigin ovrpOrigin = ovrpTrackingOrigin_EyeLevel;
		if (InOrigin == EHMDTrackingOrigin::Floor)
			ovrpOrigin = ovrpTrackingOrigin_FloorLevel;

		if (ovrp_GetInitialized())
		{
			EHMDTrackingOrigin::Type lastOrigin = GetTrackingOrigin();
			ovrp_SetTrackingOriginType2(ovrpOrigin);
			OCFlags.NeedSetTrackingOrigin = false;

			if(lastOrigin != InOrigin)
				Settings->BaseOffset = FVector::ZeroVector;
		}

		OnTrackingOriginChanged();
	}


	EHMDTrackingOrigin::Type FOculusHMD::GetTrackingOrigin()
	{
		EHMDTrackingOrigin::Type rv = EHMDTrackingOrigin::Eye;
		ovrpTrackingOrigin ovrpOrigin = ovrpTrackingOrigin_EyeLevel;

		if (ovrp_GetInitialized() && OVRP_SUCCESS(ovrp_GetTrackingOriginType2(&ovrpOrigin)))
		{
			switch (ovrpOrigin)
			{
			case ovrpTrackingOrigin_EyeLevel:
				rv = EHMDTrackingOrigin::Eye;
				break;
			case ovrpTrackingOrigin_FloorLevel:
				rv = EHMDTrackingOrigin::Floor;
				break;
			default:
				UE_LOG(LogHMD, Error, TEXT("Unsupported ovr tracking origin type %d"), int(TrackingOrigin));
				break;
			}
		}
		return rv;
	}

	bool FOculusHMD::GetFloorToEyeTrackingTransform(FTransform& OutFloorToEye) const
	{
		float EyeHeight = 0.f;
		const bool bSuccess = ovrp_GetInitialized() && OVRP_SUCCESS(ovrp_GetUserEyeHeight2(&EyeHeight));
		OutFloorToEye = FTransform( FVector(0.f, 0.f, -ConvertFloat_M2U(EyeHeight)) );

		return bSuccess;
	}

	void FOculusHMD::ResetOrientationAndPosition(float yaw)
	{
		Recenter(RecenterOrientationAndPosition, yaw);
	}

	void FOculusHMD::ResetOrientation(float yaw)
	{
		Recenter(RecenterOrientation, yaw);
	}

	void FOculusHMD::ResetPosition()
	{
		Recenter(RecenterPosition, 0);
	}

	void FOculusHMD::Recenter(FRecenterTypes RecenterType, float Yaw)
	{
		CheckInGameThread();

		if (NextFrameToRender)
		{
			const bool floorLevel = GetTrackingOrigin() != EHMDTrackingOrigin::Eye;
			ovrpPoseStatef poseState;
			ovrp_Update3(ovrpStep_Render, NextFrameToRender->FrameNumber, 0.0);
			ovrp_GetNodePoseState3(ovrpStep_Render, NextFrameToRender->FrameNumber, ovrpNode_Head, &poseState);

			if (RecenterType & RecenterPosition)
			{
				Settings->BaseOffset = ToFVector(poseState.Pose.Position);
				if (floorLevel)
					Settings->BaseOffset.Z = 0;
			}

			if (RecenterType & RecenterOrientation)
			{
				Settings->BaseOrientation = FRotator(0, FRotator(ToFQuat(poseState.Pose.Orientation)).Yaw - Yaw, 0).Quaternion();
			}
		}
	}

	void FOculusHMD::SetBaseRotation(const FRotator& BaseRot)
	{
		SetBaseOrientation(BaseRot.Quaternion());
	}


	FRotator FOculusHMD::GetBaseRotation() const
	{
		return GetBaseOrientation().Rotator();
	}


	void FOculusHMD::SetBaseOrientation(const FQuat& BaseOrient)
	{
		CheckInGameThread();

		Settings->BaseOrientation = BaseOrient;
	}


	FQuat FOculusHMD::GetBaseOrientation() const
	{
		CheckInGameThread();

		return Settings->BaseOrientation;
	}


	bool FOculusHMD::IsHeadTrackingEnforced() const
	{
		if (IsInGameThread())
		{
			return Settings.IsValid() && Settings->Flags.bHeadTrackingEnforced;
		}
		else
		{
			CheckInRenderThread();
			return Settings_RenderThread.IsValid() && Settings_RenderThread->Flags.bHeadTrackingEnforced;
		}
	}

	void FOculusHMD::SetHeadTrackingEnforced(bool bEnabled)
	{
		CheckInGameThread();
		check(Settings.IsValid());

		const bool bOldValue = Settings->Flags.bHeadTrackingEnforced;
		Settings->Flags.bHeadTrackingEnforced = bEnabled;

		if (!bEnabled)
		{
			ResetControlRotation();
		}
		else if (!bOldValue)
		{
			InitDevice();
		}
	}

	bool FOculusHMD::IsHeadTrackingAllowed() const
	{
		CheckInGameThread();

		if (!ovrp_GetInitialized())
		{
			return false;
		}

		return FHeadMountedDisplayBase::IsHeadTrackingAllowed();
	}


	void FOculusHMD::OnBeginPlay(FWorldContext& InWorldContext)
	{
		CheckInGameThread();

		CachedViewportWidget.Reset();
		CachedWindow.Reset();

#if WITH_EDITOR
		// @TODO: add more values here.
		// This call make sense when 'Play' is used from the Editor;
		if (GIsEditor && !GEnableVREditorHacks)
		{
			Settings->BaseOrientation = FQuat::Identity;
			Settings->BaseOffset = FVector::ZeroVector;
			Settings->ColorScale = ovrpVector4f{ 1,1,1,1 };
			Settings->ColorOffset = ovrpVector4f{ 0,0,0,0 };

			//Settings->WorldToMetersScale = InWorldContext.World()->GetWorldSettings()->WorldToMeters;
			//Settings->Flags.bWorldToMetersOverride = false;
			Splash->LoadSettings();
			InitDevice();

			FApp::SetUseVRFocus(true);
			FApp::SetHasVRFocus(true);
			OnStartGameFrame(InWorldContext);
		}
#endif
	}


	void FOculusHMD::OnEndPlay(FWorldContext& InWorldContext)
	{
		CheckInGameThread();

		if (GIsEditor && !GEnableVREditorHacks)
		{
			// @todo vreditor: If we add support for starting PIE while in VR Editor, we don't want to kill stereo mode when exiting PIE
			EnableStereo(false);
			ReleaseDevice();

			FApp::SetUseVRFocus(false);
			FApp::SetHasVRFocus(false);
		}
	}


	bool FOculusHMD::OnStartGameFrame(FWorldContext& InWorldContext)
	{
		CheckInGameThread();

		if (GIsRequestingExit)
		{
			return false;
		}

		RefreshTrackingToWorldTransform(InWorldContext);

		// check if HMD is marked as invalid and needs to be killed.
		ovrpBool appShouldRecreateDistortionWindow;

		if (ovrp_GetInitialized() &&
			OVRP_SUCCESS(ovrp_GetAppShouldRecreateDistortionWindow2(&appShouldRecreateDistortionWindow)) &&
			appShouldRecreateDistortionWindow)
		{
			DoEnableStereo(false);
			ReleaseDevice();

			if (!OCFlags.DisplayLostDetected)
			{
				FCoreDelegates::VRHeadsetLost.Broadcast();
				OCFlags.DisplayLostDetected = true;
			}

			Flags.bNeedEnableStereo = true;
		}
#if PLATFORM_ANDROID
		Flags.bNeedEnableStereo = true; // !!!
#endif

		check(Settings.IsValid());
		if (!Settings->IsStereoEnabled())
		{
			FApp::SetUseVRFocus(false);
			FApp::SetHasVRFocus(false);
		}

#if OCULUS_STRESS_TESTS_ENABLED
		FStressTester::TickCPU_GameThread(this);
#endif

		if (!InWorldContext.World() || (!(GEnableVREditorHacks && InWorldContext.WorldType == EWorldType::Editor) && !InWorldContext.World()->IsGameWorld()))	// @todo vreditor: (Also see OnEndGameFrame()) Kind of a hack here so we can use VR in editor viewports.  We need to consider when running GameWorld viewports inside the editor with VR.
		{
			// ignore all non-game worlds
			return false;
		}

		bool bStereoEnabled = Settings->Flags.bStereoEnabled;
		bool bStereoDesired = bStereoEnabled;

		if (Flags.bNeedEnableStereo)
		{
			bStereoDesired = true;
		}

		if (bStereoDesired && (Flags.bNeedDisableStereo || !Settings->Flags.bHMDEnabled))
		{
			bStereoDesired = false;
		}

		bool bStereoDesiredAndIsConnected = bStereoDesired;

		if (bStereoDesired && !(bStereoEnabled ? IsHMDActive() : IsHMDConnected()))
		{
			bStereoDesiredAndIsConnected = false;
		}

		Flags.bNeedEnableStereo = false;
		Flags.bNeedDisableStereo = false;

		if (bStereoEnabled != bStereoDesiredAndIsConnected)
		{
			bStereoEnabled = DoEnableStereo(bStereoDesiredAndIsConnected);
		}

		// Keep trying to enable stereo until we succeed
		Flags.bNeedEnableStereo = bStereoDesired && !bStereoEnabled;

		if (!Settings->IsStereoEnabled() && !Settings->Flags.bHeadTrackingEnforced)
		{
			return false;
		}

		if (Flags.bApplySystemOverridesOnStereo)
		{
			ApplySystemOverridesOnStereo();
			Flags.bApplySystemOverridesOnStereo = false;
		}

		CachedWorldToMetersScale = InWorldContext.World()->GetWorldSettings()->WorldToMeters;

		StartGameFrame_GameThread();

		bool retval = true;

		if (ovrp_GetInitialized())
		{
			if (OCFlags.DisplayLostDetected)
			{
				FCoreDelegates::VRHeadsetReconnected.Broadcast();
				OCFlags.DisplayLostDetected = false;
			}

			if (OCFlags.NeedSetTrackingOrigin)
			{
				SetTrackingOrigin(TrackingOrigin);
			}

			ovrpBool bAppHasVRFocus = ovrpBool_False;
			ovrp_GetAppHasVrFocus2(&bAppHasVRFocus);

			FApp::SetUseVRFocus(true);
			FApp::SetHasVRFocus(bAppHasVRFocus != ovrpBool_False);

			// Do not pause if Editor is running (otherwise it will become very laggy)
			if (!GIsEditor)
			{
				if (!bAppHasVRFocus)
				{
					// not visible,
					if (!Settings->Flags.bPauseRendering)
					{
						UE_LOG(LogHMD, Log, TEXT("The app went out of VR focus, seizing rendering..."));
					}
				}
				else if (Settings->Flags.bPauseRendering)
				{
					UE_LOG(LogHMD, Log, TEXT("The app got VR focus, restoring rendering..."));
				}
				if (OCFlags.NeedSetFocusToGameViewport)
				{
					if (bAppHasVRFocus)
					{
						UE_LOG(LogHMD, Log, TEXT("Setting user focus to game viewport since session status is visible..."));
						FSlateApplication::Get().SetAllUserFocusToGameViewport();
						OCFlags.NeedSetFocusToGameViewport = false;
					}
				}

				bool bPrevPause = Settings->Flags.bPauseRendering;
				Settings->Flags.bPauseRendering = !bAppHasVRFocus;

				if (Settings->Flags.bPauseRendering && (GEngine->GetMaxFPS() != OCULUS_PAUSED_IDLE_FPS))
				{
					GEngine->SetMaxFPS(OCULUS_PAUSED_IDLE_FPS);
				}

				if (bPrevPause != Settings->Flags.bPauseRendering)
				{
					APlayerController* const PC = GEngine->GetFirstLocalPlayerController(InWorldContext.World());
					if (Settings->Flags.bPauseRendering)
					{
						// focus is lost
						GEngine->SetMaxFPS(OCULUS_PAUSED_IDLE_FPS);

						if (!FCoreDelegates::ApplicationWillEnterBackgroundDelegate.IsBound())
						{
							OCFlags.AppIsPaused = false;
							// default action: set pause if not already paused
							if (PC && !PC->IsPaused())
							{
								PC->SetPause(true);
								OCFlags.AppIsPaused = true;
							}
						}
						else
						{
							FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
						}
					}
					else
					{
						// focus is gained
						GEngine->SetMaxFPS(0);

						if (!FCoreDelegates::ApplicationHasEnteredForegroundDelegate.IsBound())
						{
							// default action: unpause if was paused by the plugin
							if (PC && OCFlags.AppIsPaused)
							{
								PC->SetPause(false);
							}
							OCFlags.AppIsPaused = false;
						}
						else
						{
							FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
						}
					}
				}
			}

			ovrpBool AppShouldQuit;
			ovrpBool AppShouldRecenter;

			if (OVRP_SUCCESS(ovrp_GetAppShouldQuit2(&AppShouldQuit)) && AppShouldQuit || OCFlags.EnforceExit)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("OculusHMD plugin requested exit (ShouldQuit == 1)\n"));
#if WITH_EDITOR
				if (GIsEditor)
				{
					FSceneViewport* SceneVP = FindSceneViewport();
					if (SceneVP && SceneVP->IsStereoRenderingAllowed())
					{
						TSharedPtr<SWindow> Window = SceneVP->FindWindow();
						Window->RequestDestroyWindow();
					}
				}
				else
#endif//WITH_EDITOR
				{
					// ApplicationWillTerminateDelegate will fire from inside of the RequestExit
					FPlatformMisc::RequestExit(false);
				}
				OCFlags.EnforceExit = false;
				retval = false;
			}
			else if (OVRP_SUCCESS(ovrp_GetAppShouldRecenter2(&AppShouldRecenter)) && AppShouldRecenter)
			{
				FPlatformMisc::LowLevelOutputDebugString(TEXT("OculusHMD plugin was requested to recenter\n"));
				if (FCoreDelegates::VRHeadsetRecenter.IsBound())
				{
					FCoreDelegates::VRHeadsetRecenter.Broadcast();
				}
				else
				{
					ResetOrientationAndPosition();
				}

				// Call ovrp_RecenterTrackingOrigin2 to clear AppShouldRecenter flag
				ovrp_RecenterTrackingOrigin2(ovrpRecenterFlag_IgnoreAll);
			}

			UpdateHMDWornState();

			// Update tracking
			if (!Splash->IsShown())
			{
				ovrp_Update3(ovrpStep_Render, Frame->FrameNumber, 0.0);
			}
		}

#if OCULUS_MR_SUPPORTED_PLATFORMS
		if (ovrp_GetMixedRealityInitialized())
		{
			ovrp_UpdateExternalCamera();
			ovrp_UpdateCameraDevices();
		}
#endif

		if (GIsRequestingExit)
		{
			PreShutdown();
		}

		return retval;
	}


	bool FOculusHMD::OnEndGameFrame(FWorldContext& InWorldContext)
	{
		CheckInGameThread();

		FGameFrame* const CurrentGameFrame = Frame.Get();

		if (CurrentGameFrame)
		{
			// don't use the cached value, as it could be affected by the player's position, so we update it here at the latest point in the game frame
			CurrentGameFrame->TrackingToWorld = ComputeTrackingToWorldTransform(InWorldContext);
		}
		else
		{
			return false;
		}

		if ( !InWorldContext.World() || (!(GEnableVREditorHacks && InWorldContext.WorldType == EWorldType::Editor) && !InWorldContext.World()->IsGameWorld()) )
		{
			// ignore all non-game worlds
			return false;
		}

		FinishGameFrame_GameThread();

		return true;
	}


	bool FOculusHMD::IsHMDConnected()
	{
		CheckInGameThread();

		return Settings->Flags.bHMDEnabled && IsOculusHMDConnected();
	}


	bool FOculusHMD::IsHMDEnabled() const
	{
		CheckInGameThread();

		return (Settings->Flags.bHMDEnabled);
	}


	EHMDWornState::Type FOculusHMD::GetHMDWornState()
	{
		ovrpBool userPresent;

		if (ovrp_GetInitialized() && OVRP_SUCCESS(ovrp_GetUserPresent2(&userPresent)) && userPresent)
		{
			return EHMDWornState::Worn;
		}
		else
		{
			return EHMDWornState::NotWorn;
		}
	}


	void FOculusHMD::EnableHMD(bool enable)
	{
		CheckInGameThread();

		Settings->Flags.bHMDEnabled = enable;
		if (!Settings->Flags.bHMDEnabled)
		{
			EnableStereo(false);
		}
	}

	bool FOculusHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
	{
		return false;
	}


	void FOculusHMD::GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const
	{
		ovrpFrustum2f Frustum;

		if (OVRP_SUCCESS(ovrp_GetNodeFrustum2(ovrpNode_EyeCenter, &Frustum)))
		{
			InOutVFOVInDegrees = FMath::RadiansToDegrees(FMath::Atan(Frustum.Fov.UpTan) + FMath::Atan(Frustum.Fov.DownTan));
			InOutHFOVInDegrees = FMath::RadiansToDegrees(FMath::Atan(Frustum.Fov.LeftTan) + FMath::Atan(Frustum.Fov.RightTan));
		}
	}


	void FOculusHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
	{
		CheckInGameThread();

		if (ovrp_GetInitialized())
		{
			ovrp_SetUserIPD2(NewInterpupillaryDistance);
		}
	}


	float FOculusHMD::GetInterpupillaryDistance() const
	{
		CheckInGameThread();

		float UserIPD;

		if (!ovrp_GetInitialized() || OVRP_FAILURE(ovrp_GetUserIPD2(&UserIPD)))
		{
			return 0.0f;
		}

		return UserIPD;
	}


	bool FOculusHMD::GetHMDDistortionEnabled(EShadingPath /* ShadingPath */) const
	{
		return false;
	}


	bool FOculusHMD::IsChromaAbCorrectionEnabled() const
	{
		CheckInGameThread();

		return Settings->Flags.bChromaAbCorrectionEnabled;
	}


	bool FOculusHMD::HasHiddenAreaMesh() const
	{
		if (IsInRenderingThread())
		{
			if (ShouldDisableHiddenAndVisibileAreaMeshForSpectatorScreen_RenderThread())
			{
				return false;
			}
		}

		return HiddenAreaMeshes[0].IsValid() && HiddenAreaMeshes[1].IsValid();
	}


	bool FOculusHMD::HasVisibleAreaMesh() const
	{
		if (IsInRenderingThread())
		{
			if (ShouldDisableHiddenAndVisibileAreaMeshForSpectatorScreen_RenderThread())
			{
				return false;
			}
		}

		return VisibleAreaMeshes[0].IsValid() && VisibleAreaMeshes[1].IsValid();
	}


	static void DrawOcclusionMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass, const FHMDViewMesh MeshAssets[])
	{
		CheckInRenderThread();
		check(StereoPass != eSSP_FULL);

		const uint32 MeshIndex = (StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
		const FHMDViewMesh& Mesh = MeshAssets[MeshIndex];
		check(Mesh.IsValid());

		RHICmdList.SetStreamSource(0, Mesh.VertexBufferRHI, 0);
		RHICmdList.DrawIndexedPrimitive(Mesh.IndexBufferRHI, 0, 0, Mesh.NumVertices, 0, Mesh.NumTriangles, 1);
	}


	void FOculusHMD::DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
	{
		CheckInRenderThread();

		DrawOcclusionMesh_RenderThread(RHICmdList, StereoPass, HiddenAreaMeshes);
	}


	void FOculusHMD::DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
		{
		CheckInRenderThread();

		DrawOcclusionMesh_RenderThread(RHICmdList, StereoPass, VisibleAreaMeshes);
		}

	float FOculusHMD::GetPixelDenity() const
	{
		CheckInGameThread();
		return Settings->PixelDensity;
	}

	void FOculusHMD::SetPixelDensity(const float NewPixelDensity)
	{
		CheckInGameThread();
		Settings->SetPixelDensity(NewPixelDensity);
	}

	FIntPoint FOculusHMD::GetIdealRenderTargetSize() const
	{
		CheckInGameThread();
		return Settings->RenderTargetSize;
	}

	bool FOculusHMD::IsStereoEnabled() const
	{
		if (IsInGameThread())
		{
			return Settings.IsValid() && Settings->IsStereoEnabled();
		}
		else
		{
			return Settings_RenderThread.IsValid() && Settings_RenderThread->IsStereoEnabled();
		}
	}


	bool FOculusHMD::IsStereoEnabledOnNextFrame() const
	{
		// !!!

		return Settings.IsValid() && Settings->IsStereoEnabled();
	}


	bool FOculusHMD::EnableStereo(bool bStereo)
	{
		CheckInGameThread();

		return DoEnableStereo(bStereo);
	}


	void FOculusHMD::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
	{
		if (Settings.IsValid())
		{
			const int32 ViewIndex = GetViewIndexForPass(StereoPass);
			X = Settings->EyeUnscaledRenderViewport[ViewIndex].Min.X;
			Y = Settings->EyeUnscaledRenderViewport[ViewIndex].Min.Y;
			SizeX = Settings->EyeUnscaledRenderViewport[ViewIndex].Size().X;
			SizeY = Settings->EyeUnscaledRenderViewport[ViewIndex].Size().Y;
		}
		else
		{
			SizeX = SizeX / 2;
			if (StereoPass == eSSP_RIGHT_EYE)
			{
				X += SizeX;
			}
		}
	}

	void FOculusHMD::SetFinalViewRect(const enum EStereoscopicPass StereoPass, const FIntRect& FinalViewRect)
	{
		CheckInRenderThread();

		const int32 ViewIndex = GetViewIndexForPass(StereoPass);

		if (Settings_RenderThread.IsValid())
		{
			Settings_RenderThread->EyeRenderViewport[ViewIndex] = FinalViewRect;
		}

		// Called after RHIThread has already started.  Need to update Settings_RHIThread as well.
		ExecuteOnRHIThread_DoNotWait([this, ViewIndex, FinalViewRect]()
		{
			CheckInRHIThread();

			if (Settings_RHIThread.IsValid())
			{
				Settings_RHIThread->EyeRenderViewport[ViewIndex] = FinalViewRect;
			}
		});
	}

	void FOculusHMD::CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
	{
		// This method is called from GetProjectionData on a game thread.
		if (InGameThread() && StereoPassType == eSSP_LEFT_EYE && NextFrameToRender.IsValid())
		{
			// Inverse out GameHeadPose.Rotation since PlayerOrientation already contains head rotation.
			FQuat HeadOrientation = FQuat::Identity;
			FVector HeadPosition;

			GetCurrentPose(HMDDeviceId, HeadOrientation, HeadPosition);

			NextFrameToRender->PlayerOrientation = LastPlayerOrientation = ViewRotation.Quaternion() * HeadOrientation.Inverse();
			NextFrameToRender->PlayerLocation = LastPlayerLocation = ViewLocation;
		}

		FHeadMountedDisplayBase::CalculateStereoViewOffset(StereoPassType, ViewRotation, WorldToMeters, ViewLocation);
	}


	FMatrix FOculusHMD::GetStereoProjectionMatrix(EStereoscopicPass StereoPassType) const
	{
		CheckInGameThread();

		check(IsStereoEnabled());

		const int32 ViewIndex = GetViewIndexForPass(StereoPassType);

		FMatrix proj = ToFMatrix(Settings->EyeProjectionMatrices[ViewIndex]);

		// correct far and near planes for reversed-Z projection matrix
		const float WorldScale = GetWorldToMetersScale() * (1.0 / 100.0f); // physical scale is 100 UUs/meter
		float InNearZ = GNearClippingPlane * WorldScale;

		proj.M[3][3] = 0.0f;
		proj.M[2][3] = 1.0f;

		proj.M[2][2] = 0.0f;
		proj.M[3][2] = InNearZ;

		return proj;
	}


	void FOculusHMD::InitCanvasFromView(FSceneView* InView, UCanvas* Canvas)
	{
		// This is used for placing small HUDs (with names)
		// over other players (for example, in Capture Flag).
		// HmdOrientation should be initialized by GetCurrentOrientation (or
		// user's own value).
	}



	void FOculusHMD::RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture, FVector2D WindowSize) const
	{
		CheckInRenderThread();
		check(CustomPresent);

#if PLATFORM_ANDROID
		return;
#endif

		if (SpectatorScreenController)
		{
			SpectatorScreenController->RenderSpectatorScreen_RenderThread(RHICmdList, BackBuffer, SrcTexture, WindowSize);
		}

#if OCULUS_STRESS_TESTS_ENABLED
		FStressTester::TickGPU_RenderThread(RHICmdList, BackBuffer, SrcTexture);
#endif
	}

	FVector2D FOculusHMD::GetEyeCenterPoint_RenderThread(EStereoscopicPass StereoPassType) const
	{
		CheckInRenderThread();

		check(IsStereoEnabled() || IsHeadTrackingEnforced());

		// Don't use GetStereoProjectionMatrix because it is game thread only on oculus, we also don't need the zplane adjustments for this.
		const int32 ViewIndex = GetViewIndexForPass(StereoPassType);
		const FMatrix StereoProjectionMatrix = ToFMatrix(Settings_RenderThread->EyeProjectionMatrices[ViewIndex]);

		//0,0,1 is the straight ahead point, wherever it maps to is the center of the projection plane in -1..1 coordinates.  -1,-1 is bottom left.
		const FVector4 ScreenCenter = StereoProjectionMatrix.TransformPosition(FVector(0.0f, 0.0f, 1.0f));
		//transform into 0-1 screen coordinates 0,0 is top left.
		const FVector2D CenterPoint(0.5f + (ScreenCenter.X / 2.0f), 0.5f - (ScreenCenter.Y / 2.0f) );

		return CenterPoint;
	}

	FIntRect FOculusHMD::GetFullFlatEyeRect_RenderThread(FTexture2DRHIRef EyeTexture) const
	{
		check(IsInRenderingThread());
		// Rift does this differently than other platforms, it already has an idea of what rectangle it wants to use stored.
		FIntRect& EyeRect = Settings_RenderThread->EyeRenderViewport[0];

		// But the rectangle rift specifies has corners cut off, so we will crop a little more.
		static FVector2D SrcNormRectMin(0.05f, 0.0f);
		static FVector2D SrcNormRectMax(0.95f, 1.0f);
		const int32 SizeX = EyeRect.Max.X - EyeRect.Min.X;
		const int32 SizeY = EyeRect.Max.Y - EyeRect.Min.Y;
		return FIntRect(EyeRect.Min.X + SizeX * SrcNormRectMin.X, EyeRect.Min.Y + SizeY * SrcNormRectMin.Y, EyeRect.Min.X + SizeX * SrcNormRectMax.X, EyeRect.Min.Y + SizeY * SrcNormRectMax.Y);
	}


	void FOculusHMD::CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef SrcTexture, FIntRect SrcRect, FTexture2DRHIParamRef DstTexture, FIntRect DstRect, bool bClearBlack, bool bNoAlpha) const
	{
		if (bClearBlack)
		{
			FRHIRenderPassInfo RPInfo(DstTexture, ERenderTargetActions::DontLoad_Store);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("ClearToBlack"));
			{
				const FIntRect ClearRect(0, 0, DstTexture->GetSizeX(), DstTexture->GetSizeY());
				RHICmdList.SetViewport(ClearRect.Min.X, ClearRect.Min.Y, 0, ClearRect.Max.X, ClearRect.Max.Y, 1.0f);
				DrawClearQuad(RHICmdList, FLinearColor::Black);
			}
			RHICmdList.EndRenderPass();
		}

		check(CustomPresent);
		CustomPresent->CopyTexture_RenderThread(RHICmdList, DstTexture, SrcTexture, DstRect, SrcRect, false, bNoAlpha, true, true);
	}


	bool FOculusHMD::PopulateAnalyticsAttributes(TArray<FAnalyticsEventAttribute>& EventAttributes)
	{
		if (!FHeadMountedDisplayBase::PopulateAnalyticsAttributes(EventAttributes))
		{
			return false;
		}

		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HQBuffer"), (bool)Settings->Flags.bHQBuffer));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HQDistortion"), (bool)Settings->Flags.bHQDistortion));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("UpdateOnRT"), (bool)Settings->Flags.bUpdateOnRT));

		return true;
	}


	bool FOculusHMD::ShouldUseSeparateRenderTarget() const
	{
		CheckInGameThread();
		return IsStereoEnabled();
	}


	void FOculusHMD::CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
	{
		CheckInGameThread();

		if (!Settings->IsStereoEnabled())
		{
			return;
		}

		InOutSizeX = Settings->RenderTargetSize.X;
		InOutSizeY = Settings->RenderTargetSize.Y;

		check(InOutSizeX != 0 && InOutSizeY != 0);
	}


	bool FOculusHMD::NeedReAllocateViewportRenderTarget(const FViewport& Viewport)
	{
		CheckInGameThread();

		return ensureMsgf(Settings.IsValid(), TEXT("Unexpected issue with Oculus settings on the GameThread. This should be valid when this is called in EnqueueBeginRenderFrame() - has the callsite changed?")) &&
			Settings->IsStereoEnabled() && bNeedReAllocateViewportRenderTarget;
	}


	bool FOculusHMD::NeedReAllocateDepthTexture(const TRefCountPtr<IPooledRenderTarget>& DepthTarget)
	{
		CheckInRenderThread();

		return ensureMsgf(Settings_RenderThread.IsValid(), TEXT("Unexpected issue with Oculus settings on the RenderThread. This should be valid when this is called in AllocateCommonDepthTargets() - has the callsite changed?")) &&
			Settings_RenderThread->IsStereoEnabled() && bNeedReAllocateDepthTexture_RenderThread;
	}


	bool FOculusHMD::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InTexFlags, uint32 InTargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
	{
		// Only called when RenderThread is suspended.  Both of these checks should pass.
		CheckInGameThread();
		CheckInRenderThread();

		check(Index == 0);

		if (LayerMap[0].IsValid())
		{
			InitializeEyeLayer_RenderThread(GetImmediateCommandList_ForRenderCommand());

			UE_LOG(LogHMD, Log, TEXT("Allocating Oculus %d x %d rendertarget swapchain"), SizeX, SizeY);

			const FTextureSetProxyPtr& TextureSetProxy = EyeLayer_RenderThread->GetTextureSetProxy();

			if (TextureSetProxy.IsValid())
			{
				OutTargetableTexture = TextureSetProxy->GetTexture2D();
				OutShaderResourceTexture = TextureSetProxy->GetTexture2D();
				bNeedReAllocateViewportRenderTarget = false;
				return true;
			}
		}

		return false;
	}


	bool FOculusHMD::AllocateDepthTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 FlagsIn, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
	{
		CheckInRenderThread();

		check(Index == 0);

		if (EyeLayer_RenderThread.IsValid())
		{
			const FTextureSetProxyPtr& TextureSet = EyeLayer_RenderThread->GetDepthTextureSetProxy();

			if (TextureSet.IsValid())
			{
				// Ensure the texture size matches the eye layer. We may get other depth allocations unrelated to the main scene render.
				if (FIntPoint(SizeX, SizeY) == TextureSet->GetTexture2D()->GetSizeXY())
				{
					OutTargetableTexture = TextureSet->GetTexture2D();
					OutShaderResourceTexture = TextureSet->GetTexture2D();
					if (bNeedReAllocateDepthTexture_RenderThread)
					{
						UE_LOG(LogHMD, Log, TEXT("Allocating Oculus %d x %d depth rendertarget swapchain"), SizeX, SizeY);
						bNeedReAllocateDepthTexture_RenderThread = false;
					}
					return true;
				}
			}
		}

		return false;
	}


	void FOculusHMD::UpdateViewportWidget(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget)
	{
		CheckInGameThread();
		check(ViewportWidget);

		TSharedPtr<SWindow> Window = CachedWindow.Pin();
		TSharedPtr<SWidget> CurrentlyCachedWidget = CachedViewportWidget.Pin();
		TSharedRef<SWidget> Widget = ViewportWidget->AsShared();

		if (!Window.IsValid() || Widget != CurrentlyCachedWidget)
		{
			Window = FSlateApplication::Get().FindWidgetWindow(Widget);

			CachedViewportWidget = Widget;
			CachedWindow = Window;
		}

		if (!Settings->IsStereoEnabled())
		{
			// Restore AutoResizeViewport mode for the window
			if (Window.IsValid())
			{
				Window->SetMirrorWindow(false);
				Window->SetViewportSizeDrivenByWindow(true);
			}
			return;
		}

		if (bUseSeparateRenderTarget && Frame.IsValid())
		{
			CachedWindowSize = (Window.IsValid()) ? Window->GetSizeInScreen() : Viewport.GetSizeXY();
		}
	}


	FXRRenderBridge* FOculusHMD::GetActiveRenderBridge_GameThread(bool bUseSeparateRenderTarget)
	{
		CheckInGameThread();

		if (bUseSeparateRenderTarget && NextFrameToRender.IsValid())
		{
			return CustomPresent;
		}
		else
		{
			return nullptr;
		}

	}

	void FOculusHMD::UpdateHMDWornState()
	{
		const EHMDWornState::Type NewHMDWornState = GetHMDWornState();

		if (NewHMDWornState != HMDWornState)
		{
			HMDWornState = NewHMDWornState;
			if (HMDWornState == EHMDWornState::Worn)
			{
				FCoreDelegates::VRHeadsetPutOnHead.Broadcast();
			}
			else if (HMDWornState == EHMDWornState::NotWorn)
			{
				FCoreDelegates::VRHeadsetRemovedFromHead.Broadcast();
			}
		}
	}

	uint32 FOculusHMD::CreateLayer(const IStereoLayers::FLayerDesc& InLayerDesc)
	{
		CheckInGameThread();

		uint32 LayerId = NextLayerId++;
		LayerMap.Add(LayerId, MakeShareable(new FLayer(LayerId, InLayerDesc)));
		return LayerId;
	}

	void FOculusHMD::DestroyLayer(uint32 LayerId)
	{
		CheckInGameThread();
		LayerMap.Remove(LayerId);
	}


	void FOculusHMD::SetLayerDesc(uint32 LayerId, const IStereoLayers::FLayerDesc& InLayerDesc)
	{
		CheckInGameThread();
		FLayerPtr* LayerFound = LayerMap.Find(LayerId);

		if (LayerFound)
		{
			FLayer* Layer = new FLayer(**LayerFound);
			Layer->SetDesc(InLayerDesc);
			*LayerFound = MakeShareable(Layer);
		}
	}


	bool FOculusHMD::GetLayerDesc(uint32 LayerId, IStereoLayers::FLayerDesc& OutLayerDesc)
	{
		CheckInGameThread();
		FLayerPtr* LayerFound = LayerMap.Find(LayerId);

		if (LayerFound)
		{
			OutLayerDesc = (*LayerFound)->GetDesc();
			return true;
		}

		return false;
	}


	void FOculusHMD::MarkTextureForUpdate(uint32 LayerId)
	{
		CheckInGameThread();
		FLayerPtr* LayerFound = LayerMap.Find(LayerId);

		if (LayerFound)
		{
			(*LayerFound)->MarkTextureForUpdate();
		}
	}


	void FOculusHMD::UpdateSplashScreen()
	{
		if (!GetSplash() || !IsInGameThread())
		{
			return;
		}

		if (bSplashIsShown)
		{
			//if update splash screen is shown, update the head orientation default to recenter splash screens
			FQuat HeadOrientation = FQuat::Identity;
			FVector HeadPosition;
			GetCurrentPose(HMDDeviceId, HeadOrientation, HeadPosition);
			SplashRotation = FRotator(HeadOrientation);
			SplashRotation.Pitch = 0;
			SplashRotation.Roll = 0;

			Splash->Show();
		}
		else
		{
			Splash->Hide();
		}
	}

	FOculusSplashDesc FOculusHMD::GetUESplashScreenDesc()
	{
		FOculusSplashDesc Desc;
		Desc.LoadedTexture = bSplashShowMovie ? SplashMovie : SplashTexture;
		Desc.TransformInMeters = Desc.TransformInMeters * FTransform(SplashOffset/GetWorldToMetersScale());
		Desc.bNoAlphaChannel = true;
		Desc.bIsDynamic = bSplashShowMovie;
		Desc.QuadSizeInMeters *= SplashScale;
		return Desc;
	}


	void FOculusHMD::GetAllocatedTexture(uint32 LayerId, FTextureRHIRef &Texture, FTextureRHIRef &LeftTexture)
	{
		Texture = LeftTexture = nullptr;
		FLayerPtr* LayerFound = nullptr;

		if (IsInGameThread())
		{
			LayerFound = LayerMap.Find(LayerId);
		}
		else if (IsInRenderingThread())
		{
			for (int32 LayerIndex = 0; LayerIndex < Layers_RenderThread.Num(); LayerIndex++)
			{
				if (Layers_RenderThread[LayerIndex]->GetId() == LayerId)
				{
					LayerFound = &Layers_RenderThread[LayerIndex];
				}
			}
		}
		else if (IsInRHIThread())
		{
			for (int32 LayerIndex = 0; LayerIndex < Layers_RHIThread.Num(); LayerIndex++)
			{
				if (Layers_RHIThread[LayerIndex]->GetId() == LayerId)
				{
					LayerFound = &Layers_RHIThread[LayerIndex];
				}
			}
		}
		else
		{
			return;
		}

		if (LayerFound && (*LayerFound)->GetTextureSetProxy().IsValid())
		{
			bool bRightTexture = (*LayerFound)->GetRightTextureSetProxy().IsValid();
			switch ((*LayerFound)->GetDesc().ShapeType)
			{
			case IStereoLayers::CubemapLayer:
				if (bRightTexture)
				{
					Texture = (*LayerFound)->GetRightTextureSetProxy()->GetTextureCube();
					LeftTexture = (*LayerFound)->GetTextureSetProxy()->GetTextureCube();
				}
				else
				{
					Texture = LeftTexture = (*LayerFound)->GetTextureSetProxy()->GetTextureCube();
				}				break;

			case IStereoLayers::CylinderLayer:
			case IStereoLayers::QuadLayer:
				if (bRightTexture)
				{
					Texture = (*LayerFound)->GetRightTextureSetProxy()->GetTexture2D();
					LeftTexture = (*LayerFound)->GetTextureSetProxy()->GetTexture2D();
				}
				else
				{
					Texture = LeftTexture = (*LayerFound)->GetTextureSetProxy()->GetTexture2D();
				}
				break;

			default:
				break;
			}
		}
	}

	IStereoLayers::FLayerDesc FOculusHMD::GetDebugCanvasLayerDesc(FTextureRHIRef Texture)
	{
		IStereoLayers::FLayerDesc StereoLayerDesc;
		StereoLayerDesc.Transform = FTransform(FVector(0.f, 0, 0)); //100/0/0 for quads
		StereoLayerDesc.CylinderHeight = 180.f;
		StereoLayerDesc.CylinderOverlayArc = 488.f/4;
		StereoLayerDesc.CylinderRadius = 100.f;
		StereoLayerDesc.QuadSize = FVector2D(180.f, 180.f);
		StereoLayerDesc.PositionType = IStereoLayers::ELayerType::FaceLocked;
		StereoLayerDesc.ShapeType = IStereoLayers::ELayerShape::CylinderLayer;
		StereoLayerDesc.LayerSize = Texture->GetTexture2D()->GetSizeXY();
		StereoLayerDesc.Flags = IStereoLayers::ELayerFlags::LAYER_FLAG_TEX_CONTINUOUS_UPDATE;
		StereoLayerDesc.Flags |= IStereoLayers::ELayerFlags::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO;
		return StereoLayerDesc;
	}


	void FOculusHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
	{
		CheckInGameThread();

		InViewFamily.EngineShowFlags.ScreenPercentage = true;

		if (Settings->Flags.bPauseRendering)
		{
			InViewFamily.EngineShowFlags.Rendering = 0;
		}
	}


	void FOculusHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
	{
		CheckInGameThread();
	}


	void FOculusHMD::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
	{
		CheckInGameThread();

		if (Settings.IsValid() && Settings->IsStereoEnabled())
		{
			Settings->Flags.bsRGBEyeBuffer = IsMobilePlatform(GShaderPlatformForFeatureLevel[InViewFamily.Scene->GetFeatureLevel()]) && IsMobileColorsRGB();

			if (NextFrameToRender.IsValid())
			{
				NextFrameToRender->ShowFlags = InViewFamily.EngineShowFlags;
			}

			if (SpectatorScreenController != nullptr)
			{
				SpectatorScreenController->BeginRenderViewFamily();
			}
		}

		StartRenderFrame_GameThread();
	}


	void FOculusHMD::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
	{
		CheckInRenderThread();

		if (!Frame_RenderThread.IsValid())
		{
			return;
		}

		if (!Settings_RenderThread.IsValid() || !Settings_RenderThread->IsStereoEnabled())
		{
			return;
		}

		if (!ViewFamily.RenderTarget->GetRenderTargetTexture())
		{
			return;
		}

		if (SpectatorScreenController)
		{
			SpectatorScreenController->UpdateSpectatorScreenMode_RenderThread();
			Frame_RenderThread->Flags.bSpectatorScreenActive = SpectatorScreenController->GetSpectatorScreenMode() != ESpectatorScreenMode::Disabled;
		}

		// Update mirror texture
		CustomPresent->UpdateMirrorTexture_RenderThread();

#if !PLATFORM_ANDROID
	#if 0 // The entire target should be cleared by the tonemapper and pp material
 		// Clear the padding between two eyes
		const int32 GapMinX = ViewFamily.Views[0]->UnscaledViewRect.Max.X;
		const int32 GapMaxX = ViewFamily.Views[1]->UnscaledViewRect.Min.X;

		if (GapMinX < GapMaxX)
		{
			SCOPED_DRAW_EVENT(RHICmdList, OculusClearQuad)

			const int32 GapMinY = ViewFamily.Views[0]->UnscaledViewRect.Min.Y;
			const int32 GapMaxY = ViewFamily.Views[1]->UnscaledViewRect.Max.Y;

			FRHIRenderPassInfo RPInfo(ViewFamily.RenderTarget->GetRenderTargetTexture(), ERenderTargetActions::DontLoad_Store);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("Clear"));
			{
				RHICmdList.SetViewport(GapMinX, GapMinY, 0, GapMaxX, GapMaxY, 1.0f);
				DrawClearQuad(RHICmdList, FLinearColor::Black);
			}
			RHICmdList.EndRenderPass();
		}
	#endif
#else
		// ensure we have attached JNI to this thread - this has to happen persistently as the JNI could detach if the app loses focus
		FAndroidApplication::GetJavaEnv();
#endif

		// Start RHI frame
		StartRHIFrame_RenderThread();

		// Update performance stats
		PerformanceStats.Frames++;
		PerformanceStats.Seconds = FPlatformTime::Seconds();
	}


	void FOculusHMD::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
	{
	}


	void FOculusHMD::PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
	{
		CheckInRenderThread();

		FinishRenderFrame_RenderThread(RHICmdList);
	}


	int32 FOculusHMD::GetPriority() const
	{
		// We want to run after the FDefaultXRCamera's view extension
		return -1;
	}


	bool FOculusHMD::IsActiveThisFrame(class FViewport* InViewport) const
	{
		// We need to use GEngine->IsStereoscopic3D in case the current viewport disallows running in stereo.
		return GEngine && GEngine->IsStereoscopic3D(InViewport);
	}


	FOculusHMD::FOculusHMD(const FAutoRegister& AutoRegister)
		: FHeadMountedDisplayBase(nullptr)
		, FSceneViewExtensionBase(AutoRegister)
		, ConsoleCommands(this)
	{
		Flags.Raw = 0;
		OCFlags.Raw = 0;
		TrackingOrigin = EHMDTrackingOrigin::Type::Eye;
		DeltaControlRotation = FRotator::ZeroRotator;  // used from ApplyHmdRotation
		LastPlayerOrientation = FQuat::Identity;
		LastPlayerLocation = FVector::ZeroVector;
		CachedWindowSize = FVector2D::ZeroVector;
		CachedWorldToMetersScale = 100.0f;

		NextFrameNumber = 1;
		NextLayerId = 0;

		Settings = CreateNewSettings();

		RendererModule = nullptr;

		SplashLayerHandle = -1;

		SplashRotation = FRotator();
	}


	FOculusHMD::~FOculusHMD()
	{
		Shutdown();
	}


	bool FOculusHMD::Startup()
	{
		if (GIsEditor)
		{
			Settings->Flags.bHeadTrackingEnforced = true;
		}


		check(!CustomPresent.IsValid());

		FString RHIString;
		{
			FString HardwareDetails = FHardwareInfo::GetHardwareDetailsString();
			FString RHILookup = NAME_RHI.ToString() + TEXT("=");

			if (!FParse::Value(*HardwareDetails, *RHILookup, RHIString))
			{
				return false;
			}
		}

#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11
		if (RHIString == TEXT("D3D11"))
		{
			CustomPresent = CreateCustomPresent_D3D11(this);
		}
		else
#endif
#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
		if (RHIString == TEXT("D3D12"))
		{
			CustomPresent = CreateCustomPresent_D3D12(this);
		}
		else
#endif
#if OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
		if (RHIString == TEXT("OpenGL"))
		{
			CustomPresent = CreateCustomPresent_OpenGL(this);
		}
		else
#endif
#if OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN
		if (RHIString == TEXT("Vulkan"))
		{
			CustomPresent = CreateCustomPresent_Vulkan(this);
		}
		else
#endif
		{
			UE_LOG(LogHMD, Warning, TEXT("%s is not currently supported by OculusHMD plugin"), *RHIString);
			return false;
		}

		// grab a pointer to the renderer module for displaying our mirror window
		static const FName RendererModuleName("Renderer");
		RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

#if PLATFORM_ANDROID
		// register our application lifetime delegates
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FOculusHMD::ApplicationPauseDelegate);
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FOculusHMD::ApplicationResumeDelegate);
#endif

		// Create eye layer
		IStereoLayers::FLayerDesc EyeLayerDesc;
		EyeLayerDesc.Priority = INT_MIN;
		EyeLayerDesc.Flags = LAYER_FLAG_TEX_CONTINUOUS_UPDATE;
		uint32 EyeLayerId = CreateLayer(EyeLayerDesc);
		check(EyeLayerId == 0);

		Splash = MakeShareable(new FSplash(this));
		Splash->Startup();

#if !PLATFORM_ANDROID
		SpectatorScreenController = MakeUnique<FSpectatorScreenController>(this);
#endif
		UE_LOG(LogHMD, Log, TEXT("Oculus plugin initialized. Version: %s"), *GetVersionString());

		return true;
	}


	void FOculusHMD::PreShutdown()
	{
		if (Splash.IsValid())
		{
			Splash->PreShutdown();
		}
	}


	void FOculusHMD::Shutdown()
	{
		CheckInGameThread();

		if (Splash.IsValid())
		{
			Splash->Shutdown();
			Splash = nullptr;
		}

		if (CustomPresent.IsValid())
		{
			CustomPresent->Shutdown();
			CustomPresent = nullptr;
		}

		ReleaseDevice();

		Settings.Reset();
		LayerMap.Reset();
	}

	void FOculusHMD::ApplicationPauseDelegate()
	{
		ExecuteOnRenderThread([this]()
		{
			ExecuteOnRHIThread([this]()
			{
				ovrp_DestroyDistortionWindow2();
			});
		});
		OCFlags.AppIsPaused = true;
	}

	void FOculusHMD::ApplicationResumeDelegate()
	{
		if (OCFlags.AppIsPaused && !InitializeSession())
		{
			UE_LOG(LogHMD, Log, TEXT("HMD initialization failed"));
		}
		OCFlags.AppIsPaused = false;
	}

	bool FOculusHMD::InitializeSession()
	{
		UE_LOG(LogHMD, Log, TEXT("Initializing OVRPlugin session"));

		if (!ovrp_GetInitialized())
		{
#if !UE_BUILD_SHIPPING
			ovrpLogCallback logCallback = OvrpLogCallback;
#else
			ovrpLogCallback logCallback = nullptr;
#endif

#if PLATFORM_ANDROID
			void* activity = (void*) FAndroidApplication::GetGameActivityThis();
#else
			void* activity = nullptr;
#endif

			int initializeFlags = GIsEditor ? ovrpInitializeFlag_SupportsVRToggle : 0;

			initializeFlags |= CustomPresent->supportsSRGB() ? ovrpInitializeFlag_SupportSRGBFrameBuffer : 0;

			if (Settings->Flags.bSupportsDash)
			{
				initializeFlags |= ovrpInitializeFlag_FocusAware;
			}

			if (OVRP_FAILURE(ovrp_Initialize5(
				CustomPresent->GetRenderAPI(),
				logCallback,
				activity,
				CustomPresent->GetOvrpInstance(),
				CustomPresent->GetOvrpPhysicalDevice(),
				CustomPresent->GetOvrpDevice(),
				CustomPresent->GetOvrpCommandQueue(),
				initializeFlags,
				{ OVRP_VERSION })))
			{
				return false;
			}
		}

		ovrp_SetAppEngineInfo2(
			"UnrealEngine",
			TCHAR_TO_ANSI(*FEngineVersion::Current().ToString()),
			GIsEditor ? ovrpBool_True : ovrpBool_False);

#if PLATFORM_ANDROID
		ovrp_SetupDisplayObjects2(AndroidEGL::GetInstance()->GetRenderingContext()->eglContext, AndroidEGL::GetInstance()->GetDisplay(), AndroidEGL::GetInstance()->GetNativeWindow());
		ovrpBool mvSupport;
		ovrp_GetSystemMultiViewSupported2(&mvSupport);
		GSupportsMobileMultiView = mvSupport;
		if (GSupportsMobileMultiView)
		{
			UE_LOG(LogHMD, Log, TEXT("OculusHMD plugin supports multiview!"));
		}
#endif

		ovrp_SetupDistortionWindow3(ovrpDistortionWindowFlag_None);
		ovrp_SetSystemCpuLevel2(Settings->CPULevel);
		ovrp_SetSystemGpuLevel2(Settings->GPULevel);
		ovrp_SetTiledMultiResLevel((ovrpTiledMultiResLevel)Settings->MultiResLevel);
		ovrp_SetAppCPUPriority2(ovrpBool_True);
		ovrp_SetReorientHMDOnControllerRecenter(Settings->Flags.bRecenterHMDWithController ? ovrpBool_True : ovrpBool_False);

		OCFlags.NeedSetTrackingOrigin = true;
		bNeedReAllocateViewportRenderTarget = true;
		bNeedReAllocateDepthTexture_RenderThread = false;

		return true;
	}


	void FOculusHMD::ShutdownSession()
	{
		ExecuteOnRenderThread([this]()
		{
			ExecuteOnRHIThread([this]()
			{
				ovrp_DestroyDistortionWindow2();
			});
		});

		ovrp_Shutdown2();
	}

	bool FOculusHMD::InitDevice()
	{
		CheckInGameThread();

		if (ovrp_GetInitialized())
		{
			// Already created and present
			return true;
		}

		if (!IsHMDConnected())
		{
			// Don't bother if HMD is not connected
			return false;
		}

		LoadFromSettings();

		if (!InitializeSession())
		{
			UE_LOG(LogHMD, Log, TEXT("HMD initialization failed"));
			return false;
		}

		Flags.bNeedDisableStereo = false;
		OCFlags.NeedSetFocusToGameViewport = true;

		if (!CustomPresent->IsUsingCorrectDisplayAdapter())
		{
			UE_LOG(LogHMD, Error, TEXT("Using incorrect display adapter for HMD."));
			ShutdownSession();
			return false;
		}

		if (OVRP_FAILURE(ovrp_GetSystemHeadsetType2(&Settings->SystemHeadset)))
		{
			Settings->SystemHeadset = ovrpSystemHeadset_None;
		}

		UpdateHmdRenderInfo();
		UpdateStereoRenderingParams();

		ExecuteOnRenderThread([this](FRHICommandListImmediate& RHICmdList)
		{
			InitializeEyeLayer_RenderThread(RHICmdList);
		});

		if (!EyeLayer_RenderThread.IsValid() || !EyeLayer_RenderThread->GetTextureSetProxy().IsValid())
		{
			UE_LOG(LogHMD, Error, TEXT("Failed to create eye layer texture set."));
			ShutdownSession();
			return false;
		}

		ovrp_Update3(ovrpStep_Render, 0, 0.0);

		if (!HiddenAreaMeshes[0].IsValid() || !HiddenAreaMeshes[1].IsValid())
		{
			SetupOcclusionMeshes();
		}

#if !UE_BUILD_SHIPPING
		DrawDebugDelegateHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateRaw(this, &FOculusHMD::DrawDebug));
#endif

		// Do not set VR focus in Editor by just creating a device; Editor may have it created w/o requiring focus.
		// Instead, set VR focus in OnBeginPlay (VR Preview will run there first).
		if (!GIsEditor)
		{
			FApp::SetUseVRFocus(true);
			FApp::SetHasVRFocus(true);
		}

		return true;
	}


	void FOculusHMD::ReleaseDevice()
	{
		CheckInGameThread();

		if (ovrp_GetInitialized())
		{

			// Release resources
			ExecuteOnRenderThread([this]()
			{
				ExecuteOnRHIThread([this]()
				{
					for (int32 LayerIndex = 0; LayerIndex < Layers_RenderThread.Num(); LayerIndex++)
					{
						Layers_RenderThread[LayerIndex]->ReleaseResources_RHIThread();
					}

					for (int32 LayerIndex = 0; LayerIndex < Layers_RHIThread.Num(); LayerIndex++)
					{
						Layers_RHIThread[LayerIndex]->ReleaseResources_RHIThread();
					}

					if (Splash.IsValid())
					{
						Splash->ReleaseResources_RHIThread();
					}

					if (CustomPresent)
					{
						CustomPresent->ReleaseResources_RHIThread();
					}

					Settings_RHIThread.Reset();
					Frame_RHIThread.Reset();
					Layers_RHIThread.Reset();
				});

				Settings_RenderThread.Reset();
				Frame_RenderThread.Reset();
				Layers_RenderThread.Reset();
				EyeLayer_RenderThread.Reset();
			});

			Frame.Reset();
			NextFrameToRender.Reset();
			LastFrameToRender.Reset();

#if !UE_BUILD_SHIPPING
			UDebugDrawService::Unregister(DrawDebugDelegateHandle);
#endif

			// The Editor may release VR focus in OnEndPlay
			if (!GIsEditor)
			{
				FApp::SetUseVRFocus(false);
				FApp::SetHasVRFocus(false);
			}

			ShutdownSession();
		}
	}

	void BuildOcclusionMesh(FHMDViewMesh& Mesh, ovrpEye Eye, ovrpViewportStencilType MeshType)
	{
		int VertexCount = 0;
		int IndexCount = 0;

		ovrpResult Result = ovrpResult::ovrpFailure;
		if (OVRP_FAILURE(Result = ovrp_GetViewportStencil(Eye, MeshType, nullptr, &VertexCount, nullptr, &IndexCount)))
		{
			return;
		}

		FRHIResourceCreateInfo CreateInfo;
		Mesh.VertexBufferRHI = RHICreateVertexBuffer(sizeof(FFilterVertex) * VertexCount, BUF_Static, CreateInfo);
		void* VoidPtr = RHILockVertexBuffer(Mesh.VertexBufferRHI, 0, sizeof(FFilterVertex) * VertexCount, RLM_WriteOnly);
		FFilterVertex* pVertices = reinterpret_cast<FFilterVertex*>(VoidPtr);

		Mesh.IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), sizeof(uint16) * IndexCount, BUF_Static, CreateInfo);
		void* VoidPtr2 = RHILockIndexBuffer(Mesh.IndexBufferRHI, 0, sizeof(uint16) * IndexCount, RLM_WriteOnly);
		uint16* pIndices = reinterpret_cast<uint16*>(VoidPtr2);

		ovrpVector2f* const ovrpVertices = new ovrpVector2f[VertexCount];

		ovrp_GetViewportStencil(Eye, MeshType, ovrpVertices, &VertexCount, pIndices, &IndexCount);

		for (int i = 0; i < VertexCount; ++i)
		{
			FFilterVertex& Vertex = pVertices[i];
			CA_SUPPRESS(6385); //  warning C6385: Reading invalid data from 'ovrpVertices':  the readable size is 'VertexCount*8' bytes, but '16' bytes may be read
			const ovrpVector2f& Position = ovrpVertices[i];
			if (MeshType == ovrpViewportStencilType_HiddenArea)
			{
				Vertex.Position.X = (Position.x * 2.0f) - 1.0f;
				Vertex.Position.Y = 1.0f - (Position.y * 2.0f);
				Vertex.Position.Z = 1.0f;
				Vertex.Position.W = 1.0f;
				Vertex.UV.X = 0.0f;
				Vertex.UV.Y = 0.0f;
			}
			else if (MeshType == ovrpViewportStencilType_VisibleArea)
			{
				Vertex.Position.X = Position.x;
				Vertex.Position.Y = Position.y;
				Vertex.Position.Z = 0.0f;
				Vertex.Position.W = 1.0f;
				Vertex.UV.X = Position.x;
				Vertex.UV.Y = Position.y;
			}
			else
			{
				check(0);
			}
		}

		Mesh.NumIndices = IndexCount;
		Mesh.NumVertices = VertexCount;
		Mesh.NumTriangles = IndexCount / 3;

		delete [] ovrpVertices;

		RHIUnlockVertexBuffer(Mesh.VertexBufferRHI);
		RHIUnlockIndexBuffer(Mesh.IndexBufferRHI);
	}

	void FOculusHMD::SetupOcclusionMeshes()
	{
		CheckInGameThread();

		FOculusHMD* const Self = this;
		ENQUEUE_RENDER_COMMAND(SetupOcclusionMeshesCmd)([Self](FRHICommandListImmediate& RHICmdList)
		{
			BuildOcclusionMesh(Self->HiddenAreaMeshes[0], ovrpEye_Left, ovrpViewportStencilType_HiddenArea);
			BuildOcclusionMesh(Self->HiddenAreaMeshes[1], ovrpEye_Right, ovrpViewportStencilType_HiddenArea);
			BuildOcclusionMesh(Self->VisibleAreaMeshes[0], ovrpEye_Left, ovrpViewportStencilType_VisibleArea);
			BuildOcclusionMesh(Self->VisibleAreaMeshes[1], ovrpEye_Right, ovrpViewportStencilType_VisibleArea);
		});
	}


	static ovrpMatrix4f ovrpMatrix4f_Projection(const ovrpFrustum2f& frustum, bool leftHanded)
	{
		float handednessScale = leftHanded ? 1.0f : -1.0f;

		// A projection matrix is very like a scaling from NDC, so we can start with that.
		float projXScale = 2.0f / (frustum.Fov.LeftTan + frustum.Fov.RightTan);
		float projXOffset = (frustum.Fov.LeftTan - frustum.Fov.RightTan) * projXScale * 0.5f;
		float projYScale = 2.0f / (frustum.Fov.UpTan + frustum.Fov.DownTan);
		float projYOffset = (frustum.Fov.UpTan - frustum.Fov.DownTan) * projYScale * 0.5f;

		ovrpMatrix4f projection;

		// Produces X result, mapping clip edges to [-w,+w]
		projection.M[0][0] = projXScale;
		projection.M[0][1] = 0.0f;
		projection.M[0][2] = handednessScale * projXOffset;
		projection.M[0][3] = 0.0f;

		// Produces Y result, mapping clip edges to [-w,+w]
		// Hey - why is that YOffset negated?
		// It's because a projection matrix transforms from world coords with Y=up,
		// whereas this is derived from an NDC scaling, which is Y=down.
		projection.M[1][0] = 0.0f;
		projection.M[1][1] = projYScale;
		projection.M[1][2] = handednessScale * -projYOffset;
		projection.M[1][3] = 0.0f;

		// Produces Z-buffer result
		projection.M[2][0] = 0.0f;
		projection.M[2][1] = 0.0f;
		projection.M[2][2] = -handednessScale * frustum.zFar / (frustum.zNear - frustum.zFar);
		projection.M[2][3] = (frustum.zFar * frustum.zNear) / (frustum.zNear - frustum.zFar);

		// Produces W result (= Z in)
		projection.M[3][0] = 0.0f;
		projection.M[3][1] = 0.0f;
		projection.M[3][2] = handednessScale;
		projection.M[3][3] = 0.0f;

		return projection;
	}


	void FOculusHMD::UpdateStereoRenderingParams()
	{
		CheckInGameThread();

		// Update PixelDensity
		bool bSupportsDepth = true;

		if (Settings->Flags.bPixelDensityAdaptive)
		{
			float AdaptiveGpuPerformanceScale = 1.0f;
			ovrp_GetAdaptiveGpuPerformanceScale2(&AdaptiveGpuPerformanceScale);
			float NewPixelDensity = Settings->PixelDensity * FMath::Sqrt(AdaptiveGpuPerformanceScale);
			NewPixelDensity = FMath::RoundToFloat(NewPixelDensity * 1024.0f) / 1024.0f;
			Settings->SetPixelDensity(NewPixelDensity);
		}
		else
		{
			static const auto PixelDensityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("vr.PixelDensity"));
			Settings->SetPixelDensity(PixelDensityCVar ? PixelDensityCVar->GetFloat() : 1.0f);

			// Due to hijacking the depth target directly from the scene context, we can't support depth compositing if it's being scaled by screen percentage since it wont match our color render target dimensions.
			static const auto ScreenPercentageCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
			bSupportsDepth = !ScreenPercentageCVar || ScreenPercentageCVar->GetFloat() == 100.0f;
		}

		// Update EyeLayer
		FLayerPtr* EyeLayerFound = LayerMap.Find(0);
		FLayer* EyeLayer = new FLayer(**EyeLayerFound);
		*EyeLayerFound = MakeShareable(EyeLayer);

		ovrpLayout Layout = ovrpLayout_DoubleWide;
#if PLATFORM_ANDROID
		static const auto CVarMobileMultiView = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MobileMultiView"));
		static const auto CVarMobileMultiViewDirect = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MobileMultiView.Direct"));
		const bool bIsMobileMultiViewEnabled = (CVarMobileMultiView && CVarMobileMultiView->GetValueOnAnyThread() != 0);
		const bool bIsMobileMultiViewDirectEnabled = (CVarMobileMultiViewDirect && CVarMobileMultiViewDirect->GetValueOnAnyThread() != 0);
		const bool bIsUsingDirectMobileMultiView = GSupportsMobileMultiView && bIsMobileMultiViewEnabled && bIsMobileMultiViewDirectEnabled;
		if (Settings->Flags.bDirectMultiview && bIsUsingDirectMobileMultiView)
		{
			Layout = ovrpLayout_Array;
			Settings->Flags.bIsUsingDirectMultiview = true;
		}

#endif

		ovrpLayerDesc_EyeFov EyeLayerDesc;

		if (OVRP_SUCCESS(ovrp_CalculateEyeLayerDesc2(
			Layout,
			Settings->Flags.bPixelDensityAdaptive ? Settings->PixelDensityMax : Settings->PixelDensity,
			Settings->Flags.bHQDistortion ? 0 : 1,
			1, // UNDONE
			CustomPresent->GetDefaultOvrpTextureFormat(),
			(Settings->Flags.bCompositeDepth && bSupportsDepth) ? CustomPresent->GetDefaultDepthOvrpTextureFormat() : ovrpTextureFormat_None,
			CustomPresent->GetLayerFlags() | (Settings->Flags.bChromaAbCorrectionEnabled ? ovrpLayerFlag_ChromaticAberrationCorrection : 0),
			&EyeLayerDesc)))
		{
			// Update viewports
			// Scaling for DynamicResolution will happen later - see FSceneRenderer::PrepareViewRectsForRendering.
			// If scaling does occur, EyeRenderViewport will be updated in FOculusHMD::SetFinalViewRect.
			ovrpRecti vpRect[2];
			ovrp_CalculateEyeViewportRect(EyeLayerDesc, ovrpEye_Left, 1.0f, &vpRect[0]);
			ovrp_CalculateEyeViewportRect(EyeLayerDesc, ovrpEye_Right, 1.0f, &vpRect[1]);

			if (Settings->Flags.bPixelDensityAdaptive)
			{
				vpRect[0].Size.w = vpRect[1].Size.w = ((int)(vpRect[0].Size.w / Settings->PixelDensityMax) + 3) & ~3;
				vpRect[0].Size.h = vpRect[1].Size.h = ((int)(vpRect[0].Size.h / Settings->PixelDensityMax) + 3) & ~3;

				EyeLayerDesc.MaxViewportSize.w = ((int)(vpRect[0].Size.w * Settings->PixelDensityMax) + 3) & ~3;
				EyeLayerDesc.MaxViewportSize.h = ((int)(vpRect[0].Size.h * Settings->PixelDensityMax) + 3) & ~3;
			}

			// Unreal assumes no gutter between eyes
			EyeLayerDesc.TextureSize.w = EyeLayerDesc.MaxViewportSize.w;
			EyeLayerDesc.TextureSize.h = EyeLayerDesc.MaxViewportSize.h;

			if (Layout == ovrpLayout_DoubleWide)
			{
				vpRect[1].Pos.x = vpRect[0].Size.w;
				EyeLayerDesc.TextureSize.w *= 2;
			}

			EyeLayer->SetEyeLayerDesc(EyeLayerDesc, vpRect);
			EyeLayer->bNeedsTexSrgbCreate = Settings->Flags.bsRGBEyeBuffer;

			Settings->RenderTargetSize = FIntPoint(EyeLayerDesc.TextureSize.w, EyeLayerDesc.TextureSize.h);
			Settings->EyeRenderViewport[0].Min = FIntPoint(vpRect[0].Pos.x, vpRect[0].Pos.y);
			Settings->EyeRenderViewport[0].Max = Settings->EyeRenderViewport[0].Min + FIntPoint(vpRect[0].Size.w, vpRect[0].Size.h);
			Settings->EyeRenderViewport[1].Min = FIntPoint(vpRect[1].Pos.x, vpRect[1].Pos.y);
			Settings->EyeRenderViewport[1].Max = Settings->EyeRenderViewport[1].Min + FIntPoint(vpRect[1].Size.w, vpRect[1].Size.h);

			Settings->EyeUnscaledRenderViewport[0] = Settings->EyeRenderViewport[0];
			Settings->EyeUnscaledRenderViewport[1] = Settings->EyeRenderViewport[1];

			// Update projection matrices
			ovrpFrustum2f frustumLeft = { 0.001f, 1000.0f, EyeLayerDesc.Fov[0] };
			ovrpFrustum2f frustumRight = { 0.001f, 1000.0f, EyeLayerDesc.Fov[1] };

			Settings->EyeProjectionMatrices[0] = ovrpMatrix4f_Projection(frustumLeft, true);
			Settings->EyeProjectionMatrices[1] = ovrpMatrix4f_Projection(frustumRight, true);

			Settings->PerspectiveProjection[0] = ovrpMatrix4f_Projection(frustumLeft, false);
			Settings->PerspectiveProjection[1] = ovrpMatrix4f_Projection(frustumRight, false);

			// Flag if need to recreate render targets
			if (!EyeLayer->CanReuseResources(EyeLayer_RenderThread.Get()))
			{
				bNeedReAllocateViewportRenderTarget = true;
			}
		}
	}


	void FOculusHMD::UpdateHmdRenderInfo()
	{
		CheckInGameThread();
		ovrp_GetSystemDisplayFrequency2(&Settings->VsyncToNextVsync);
	}


	void FOculusHMD::InitializeEyeLayer_RenderThread(FRHICommandListImmediate& RHICmdList)
	{
		CheckInRenderThread();

		if (LayerMap[0].IsValid())
		{
			FLayerPtr EyeLayer = LayerMap[0]->Clone();
			EyeLayer->Initialize_RenderThread(Settings_RenderThread.Get(), CustomPresent, RHICmdList, EyeLayer_RenderThread.Get());

			if(Layers_RenderThread.Num() > 0)
			{
				Layers_RenderThread[0] = EyeLayer;
			}
			else
			{
				Layers_RenderThread.Add(EyeLayer);
			}

			if (EyeLayer->GetDepthTextureSetProxy().IsValid())
			{
				if (!EyeLayer_RenderThread.IsValid() || EyeLayer->GetDepthTextureSetProxy() != EyeLayer_RenderThread->GetDepthTextureSetProxy())
				{
					bNeedReAllocateDepthTexture_RenderThread = true;
				}
			}

			EyeLayer_RenderThread = EyeLayer;
		}
	}


	void FOculusHMD::ApplySystemOverridesOnStereo(bool force)
	{
		CheckInGameThread();
		// ALWAYS SET r.FinishCurrentFrame to 0! Otherwise the perf might be poor.
		// @TODO: revise the FD3D11DynamicRHI::RHIEndDrawingViewport code (and other renderers)
		// to ignore this var completely.
		static const auto CFinishFrameVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
		CFinishFrameVar->Set(0);

#if PLATFORM_ANDROID
		static IConsoleVariable* CVarMobileMSAA = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileMSAA"));
		if (CVarMobileMSAA)
		{
			CVarMobileMSAA->Set(CustomPresent->GetSystemRecommendedMSAALevel());
		}
#endif
	}


	bool FOculusHMD::OnOculusStateChange(bool bIsEnabledNow)
	{
		if (!bIsEnabledNow)
		{
			// Switching from stereo
			ReleaseDevice();

			ResetControlRotation();
			return true;
		}
		else
		{
			// Switching to stereo
			if (InitDevice())
			{
				Flags.bApplySystemOverridesOnStereo = true;
				return true;
			}
			DeltaControlRotation = FRotator::ZeroRotator;
		}
		return false;
	}


	class FSceneViewport* FOculusHMD::FindSceneViewport()
	{
		if (!GIsEditor)
		{
			UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
			return GameEngine->SceneViewport.Get();
		}
#if WITH_EDITOR
		else
		{
			UEditorEngine* EditorEngine = CastChecked<UEditorEngine>(GEngine);
			FSceneViewport* PIEViewport = (FSceneViewport*)EditorEngine->GetPIEViewport();
			if (PIEViewport != nullptr && PIEViewport->IsStereoRenderingAllowed())
			{
				// PIE is setup for stereo rendering
				return PIEViewport;
			}
			else
			{
				// Check to see if the active editor viewport is drawing in stereo mode
				// @todo vreditor: Should work with even non-active viewport!
				FSceneViewport* EditorViewport = (FSceneViewport*)EditorEngine->GetActiveViewport();
				if (EditorViewport != nullptr && EditorViewport->IsStereoRenderingAllowed())
				{
					return EditorViewport;
				}
			}
		}
#endif
		return nullptr;
	}


	bool FOculusHMD::ShouldDisableHiddenAndVisibileAreaMeshForSpectatorScreen_RenderThread() const
	{
		CheckInRenderThread();

		// If you really need the eye corners to look nice, and can't just crop more,
		// and are willing to suffer a frametime hit... you could do this:
#if 0
		switch(GetSpectatorScreenMode_RenderThread())
		{
		case ESpectatorScreenMode::SingleEyeLetterboxed:
		case ESpectatorScreenMode::SingleEyeCroppedToFill:
		case ESpectatorScreenMode::TexturePlusEye:
			return true;
		}
#endif

		return false;
	}


	ESpectatorScreenMode FOculusHMD::GetSpectatorScreenMode_RenderThread() const
	{
		CheckInRenderThread();
		return SpectatorScreenController ? SpectatorScreenController->GetSpectatorScreenMode() : ESpectatorScreenMode::Disabled;
	}


#if !UE_BUILD_SHIPPING
	static const char* FormatLatencyReading(char* buff, size_t size, float val)
	{
		if (val < 0.000001f)
		{
			FCStringAnsi::Strcpy(buff, size, "N/A   ");
		}
		else
		{
			FCStringAnsi::Snprintf(buff, size, "%4.2fms", val * 1000.0f);
		}
		return buff;
	}


	void FOculusHMD::DrawDebug(UCanvas* InCanvas, APlayerController* InPlayerController)
	{
		CheckInGameThread();

		if (InCanvas && IsStereoEnabled() && Settings->Flags.bShowStats)
		{
			static const FColor TextColor(0, 255, 0);
			// Pick a larger font on console.
			UFont* const Font = FPlatformProperties::SupportsWindowedMode() ? GEngine->GetSmallFont() : GEngine->GetMediumFont();
			const int32 RowHeight = FMath::TruncToInt(Font->GetMaxCharHeight() * 1.1f);

			float ClipX = InCanvas->ClipX;
			float ClipY = InCanvas->ClipY;
			float LeftPos = 0;

			ClipX -= 100;
			LeftPos = ClipX * 0.3f;
			float TopPos = ClipY * 0.4f;

			int32 X = (int32)LeftPos;
			int32 Y = (int32)TopPos;

			FString Str;

			if (!Settings->Flags.bPixelDensityAdaptive)
			{
				Str = FString::Printf(TEXT("PD: %.2f"), Settings->PixelDensity);
			}
			else
			{
				Str = FString::Printf(TEXT("PD: %.2f [%0.2f, %0.2f]"), Settings->PixelDensity,
					Settings->PixelDensityMin, Settings->PixelDensityMax);
			}
			InCanvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
			Y += RowHeight;

			Str = FString::Printf(TEXT("W-to-m scale: %.2f uu/m"), GetWorldToMetersScale());
			InCanvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);

			ovrpAppLatencyTimings AppLatencyTimings;
			if (OVRP_SUCCESS(ovrp_GetAppLatencyTimings2(&AppLatencyTimings)))
			{
				Y += RowHeight;

				char buf[5][20];
				char destStr[100];

				FCStringAnsi::Snprintf(destStr, sizeof(destStr), "Latency, ren: %s tw: %s pp: %s err: %s %s",
					FormatLatencyReading(buf[0], sizeof(buf[0]), AppLatencyTimings.LatencyRender),
					FormatLatencyReading(buf[1], sizeof(buf[1]), AppLatencyTimings.LatencyTimewarp),
					FormatLatencyReading(buf[2], sizeof(buf[2]), AppLatencyTimings.LatencyPostPresent),
					FormatLatencyReading(buf[3], sizeof(buf[3]), AppLatencyTimings.ErrorRender),
					FormatLatencyReading(buf[4], sizeof(buf[4]), AppLatencyTimings.ErrorTimewarp));

				Str = ANSI_TO_TCHAR(destStr);
				InCanvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
			}

			// Second row
			X = (int32)LeftPos + 200;
			Y = (int32)TopPos;

			Str = FString::Printf(TEXT("HQ dist: %s"), (Settings->Flags.bHQDistortion) ? TEXT("ON") : TEXT("OFF"));
			InCanvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
			Y += RowHeight;

			float UserIPD;
			if (OVRP_SUCCESS(ovrp_GetUserIPD2(&UserIPD)))
			{
				Str = FString::Printf(TEXT("IPD: %.2f mm"), UserIPD * 1000.f);
				InCanvas->Canvas->DrawShadowedString(X, Y, *Str, Font, TextColor);
				Y += RowHeight;
			}
		}
	}
#endif // #if !UE_BUILD_SHIPPING


	bool FOculusHMD::IsHMDActive() const
	{
		return ovrp_GetInitialized() != ovrpBool_False;
	}

	float FOculusHMD::GetWorldToMetersScale() const
	{
		CheckInGameThread();

		if (NextFrameToRender.IsValid())
		{
			return NextFrameToRender->WorldToMetersScale;
		}

		if (GWorld != nullptr)
		{
#if WITH_EDITOR
			// Workaround to allow WorldToMeters scaling to work correctly for controllers while running inside PIE.
			// The main world will most likely not be pointing at the PIE world while polling input, so if we find a world context
			// of that type, use that world's WorldToMeters instead.
			if (GIsEditor)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					if (Context.WorldType == EWorldType::PIE)
					{
						return Context.World()->GetWorldSettings()->WorldToMeters;
					}
				}
			}
#endif //WITH_EDITOR

			// We're not currently rendering a frame, so just use whatever world to meters the main world is using.
			// This can happen when we're polling input in the main engine loop, before ticking any worlds.
			return GWorld->GetWorldSettings()->WorldToMeters;
		}

		return 100.0f;
	}

	FVector FOculusHMD::GetNeckPosition(const FQuat& HeadOrientation, const FVector& HeadPosition)
	{
		CheckInGameThread();

		FVector NeckPosition = HeadOrientation.Inverse().RotateVector(HeadPosition);

		ovrpVector2f NeckEyeDistance;
		if (OVRP_SUCCESS(ovrp_GetUserNeckEyeDistance2(&NeckEyeDistance)))
		{
			const float WorldToMetersScale = GetWorldToMetersScale();
			NeckPosition.X -= NeckEyeDistance.x * WorldToMetersScale;
			NeckPosition.Z -= NeckEyeDistance.y * WorldToMetersScale;
		}

		return NeckPosition;
	}


	void FOculusHMD::SetBaseOffsetInMeters(const FVector& BaseOffset)
	{
		CheckInGameThread();

		Settings->BaseOffset = BaseOffset;
	}


	FVector FOculusHMD::GetBaseOffsetInMeters() const
	{
		CheckInGameThread();

		return Settings->BaseOffset;
	}


	bool FOculusHMD::ConvertPose(const ovrpPosef& InPose, FPose& OutPose) const
	{
		CheckInGameThread();

		if (!NextFrameToRender.IsValid())
		{
			return false;
		}

		return ConvertPose_Internal(InPose, OutPose, Settings.Get(), NextFrameToRender->WorldToMetersScale);
	}


	bool FOculusHMD::ConvertPose_RenderThread(const ovrpPosef& InPose, FPose& OutPose) const
	{
		CheckInRenderThread();

		if (!Frame_RenderThread.IsValid())
		{
			return false;
		}

		return ConvertPose_Internal(InPose, OutPose, Settings_RenderThread.Get(), Frame_RenderThread->WorldToMetersScale);
	}


	bool FOculusHMD::ConvertPose_Internal(const ovrpPosef& InPose, FPose& OutPose, const FSettings* Settings, float WorldToMetersScale)
	{
		return OculusHMD::ConvertPose_Internal(InPose, OutPose, Settings->BaseOrientation, Settings->BaseOffset, WorldToMetersScale);
	}


	FVector FOculusHMD::ScaleAndMovePointWithPlayer(ovrpVector3f& OculusHMDPoint)
	{
		CheckInGameThread();

		FMatrix TranslationMatrix;
		TranslationMatrix.SetIdentity();
		TranslationMatrix = TranslationMatrix.ConcatTranslation(LastPlayerLocation);

		FVector ConvertedPoint = ToFVector(OculusHMDPoint) * GetWorldToMetersScale();
		FRotator RotateWithPlayer = LastPlayerOrientation.Rotator();
		FVector TransformWithPlayer = RotateWithPlayer.RotateVector(ConvertedPoint);
		TransformWithPlayer = FVector(TranslationMatrix.TransformPosition(TransformWithPlayer));

		if (GetXRCamera(HMDDeviceId)->GetUseImplicitHMDPosition())
		{
			FQuat HeadOrientation = FQuat::Identity;
			FVector HeadPosition;
			GetCurrentPose(HMDDeviceId, HeadOrientation, HeadPosition);
			TransformWithPlayer -= RotateWithPlayer.RotateVector(HeadPosition);
		}

		return TransformWithPlayer;
	}

	ovrpVector3f FOculusHMD::WorldLocationToOculusPoint(const FVector& InUnrealPosition)
	{
		CheckInGameThread();
		FQuat AdjustedPlayerOrientation = GetBaseOrientation().Inverse() * LastPlayerOrientation;
		AdjustedPlayerOrientation.Normalize();

		FVector AdjustedPlayerLocation = LastPlayerLocation;
		if (GetXRCamera(HMDDeviceId)->GetUseImplicitHMDPosition())
		{
			FQuat HeadOrientation = FQuat::Identity; // Unused
			FVector HeadPosition;
			GetCurrentPose(HMDDeviceId, HeadOrientation, HeadPosition);
			AdjustedPlayerLocation -= LastPlayerOrientation.Inverse().RotateVector(HeadPosition);
		}
		const FTransform InvWorldTransform = FTransform(AdjustedPlayerOrientation, AdjustedPlayerLocation).Inverse();
		const FVector ConvertedPosition = InvWorldTransform.TransformPosition(InUnrealPosition) / GetWorldToMetersScale();

		return ToOvrpVector3f(ConvertedPosition);
	}


	float FOculusHMD::ConvertFloat_M2U(float OculusFloat) const
	{
		CheckInGameThread();

		return OculusFloat * GetWorldToMetersScale();
	}


	FVector FOculusHMD::ConvertVector_M2U(ovrpVector3f OculusHMDPoint) const
	{
		CheckInGameThread();

		return ToFVector(OculusHMDPoint) * GetWorldToMetersScale();
	}


	bool FOculusHMD::GetUserProfile(UserProfile& OutProfile)
	{
		float UserIPD;
		ovrpVector2f UserNeckEyeDistance;
		float UserEyeHeight;

		if (ovrp_GetInitialized() &&
			OVRP_SUCCESS(ovrp_GetUserIPD2(&UserIPD)) &&
			OVRP_SUCCESS(ovrp_GetUserNeckEyeDistance2(&UserNeckEyeDistance)) &&
			OVRP_SUCCESS(ovrp_GetUserEyeHeight2(&UserEyeHeight)))
		{
			OutProfile.IPD = UserIPD;
			OutProfile.EyeDepth = UserNeckEyeDistance.x;
			OutProfile.EyeHeight = UserEyeHeight;
			return true;
		}

		return false;
	}


	float FOculusHMD::GetVsyncToNextVsync() const
	{
		CheckInGameThread();

		return Settings->VsyncToNextVsync;
	}


	FPerformanceStats FOculusHMD::GetPerformanceStats() const
	{
		return PerformanceStats;
	}

	void FOculusHMD::SetCPUAndGPULevel(int CPULevel, int GPULevel)
	{
		CheckInGameThread();
		Settings->CPULevel = CPULevel;
		Settings->GPULevel = GPULevel;
		ovrp_SetSystemCpuLevel2(Settings->CPULevel);
		ovrp_SetSystemGpuLevel2(Settings->GPULevel);
	}


	void FOculusHMD::SetTiledMultiResLevel(ETiledMultiResLevel multiresLevel)
	{
		CheckInGameThread();
		Settings->MultiResLevel = multiresLevel;
	}

	void FOculusHMD::SetColorScaleAndOffset(FLinearColor ColorScale, FLinearColor ColorOffset, bool bApplyToAllLayers)
	{
		CheckInGameThread();
		Settings->bApplyColorScaleAndOffsetToAllLayers = bApplyToAllLayers;
		Settings->ColorScale = LinearColorToOvrpVector4f(ColorScale);
		Settings->ColorOffset = LinearColorToOvrpVector4f(ColorOffset);
	}

	bool FOculusHMD::DoEnableStereo(bool bStereo)
	{
		CheckInGameThread();

		FSceneViewport* SceneVP = FindSceneViewport();

		if (!Settings->Flags.bHMDEnabled || (SceneVP && !SceneVP->IsStereoRenderingAllowed()))
		{
			bStereo = false;
		}

		if (Settings->Flags.bStereoEnabled && bStereo || !Settings->Flags.bStereoEnabled && !bStereo)
		{
			// already in the desired mode
			return Settings->Flags.bStereoEnabled;
		}

		TSharedPtr<SWindow> Window;

		if (SceneVP)
		{
			Window = SceneVP->FindWindow();
		}

		if (!Window.IsValid() || !SceneVP || !SceneVP->GetViewportWidget().IsValid())
		{
			// try again next frame
			if (bStereo)
			{
				Flags.bNeedEnableStereo = true;

				// a special case when stereo is enabled while window is not available yet:
				// most likely this is happening from BeginPlay. In this case, if frame exists (created in OnBeginPlay)
				// then we need init device and populate the initial tracking for head/hand poses.
				if (Frame.IsValid())
				{
					InitDevice();
				}
			}
			else
			{
				Flags.bNeedDisableStereo = true;
			}

			return Settings->Flags.bStereoEnabled;
		}

		if (OnOculusStateChange(bStereo))
		{
			Settings->Flags.bStereoEnabled = bStereo;

			// Uncap fps to enable FPS higher than 62
			GEngine->bForceDisableFrameRateSmoothing = bStereo;

			// Set MirrorWindow state on the Window
			Window->SetMirrorWindow(bStereo);

			if (bStereo)
			{
				// Start frame
				StartGameFrame_GameThread();
				StartRenderFrame_GameThread();

				ovrp_Update3(ovrpStep_Render, Frame->FrameNumber, 0.0);


				// Set viewport size to Rift resolution
				// NOTE: this can enqueue a render frame right away as a result (calling into FOculusHMD::BeginRenderViewFamily)
				SceneVP->SetViewportSize(Settings->RenderTargetSize.X, Settings->RenderTargetSize.Y);

				if (Settings->Flags.bPauseRendering)
				{
					GEngine->SetMaxFPS(10);
				}

				// Hook up dynamic res
#if !PLATFORM_ANDROID
				GEngine->ChangeDynamicResolutionStateAtNextFrame(MakeShareable(new FDynamicResolutionState(Settings)));
#endif
			}
			else
			{
				if (Settings->Flags.bPauseRendering)
				{
					GEngine->SetMaxFPS(0);
				}

				// Restore viewport size to window size
				FVector2D size = Window->GetSizeInScreen();
				SceneVP->SetViewportSize(size.X, size.Y);
				Window->SetViewportSizeDrivenByWindow(true);

				// Restore default dynamic res
#if !PLATFORM_ANDROID
				GEngine->ChangeDynamicResolutionStateAtNextFrame(FDynamicResolutionHeuristicProxy::CreateDefaultState());
#endif
			}
		}

		return Settings->Flags.bStereoEnabled;
	}

	void FOculusHMD::ResetControlRotation() const
	{
		// Switching back to non-stereo mode: reset player rotation and aim.
		// Should we go through all playercontrollers here?
		APlayerController* pc = GEngine->GetFirstLocalPlayerController(GWorld);
		if (pc)
		{
			// Reset Aim? @todo
			FRotator r = pc->GetControlRotation();
			r.Normalize();
			// Reset roll and pitch of the player
			r.Roll = 0;
			r.Pitch = 0;
			pc->SetControlRotation(r);
		}
	}


	FSettingsPtr FOculusHMD::CreateNewSettings() const
	{
		FSettingsPtr Result(MakeShareable(new FSettings()));
		return Result;
	}


	FGameFramePtr FOculusHMD::CreateNewGameFrame() const
	{
		FGameFramePtr Result(MakeShareable(new FGameFrame()));
		Result->FrameNumber = NextFrameNumber;
		Result->WindowSize = CachedWindowSize;
		Result->WorldToMetersScale = CachedWorldToMetersScale;
		Result->NearClippingPlane = GNearClippingPlane;
		Result->MultiResLevel = Settings->MultiResLevel;
		return Result;
	}


	void FOculusHMD::StartGameFrame_GameThread()
	{
		CheckInGameThread();
		check(Settings.IsValid());

		if (!Frame.IsValid())
		{
			Frame = CreateNewGameFrame();
			NextFrameToRender = Frame;

			UE_LOG(LogHMD, VeryVerbose, TEXT("StartGameFrame %u %u"), Frame->FrameNumber, Frame->ShowFlags.Rendering);

			UpdateStereoRenderingParams();
		}
	}


	void FOculusHMD::FinishGameFrame_GameThread()
	{
		CheckInGameThread();

		if (Frame.IsValid())
		{
			UE_LOG(LogHMD, VeryVerbose, TEXT("FinishGameFrame %u"), Frame->FrameNumber);
		}

		Frame.Reset();
	}


	void FOculusHMD::StartRenderFrame_GameThread()
	{
		CheckInGameThread();

		if (NextFrameToRender.IsValid() && NextFrameToRender != LastFrameToRender)
		{
			UE_LOG(LogHMD, VeryVerbose, TEXT("StartRenderFrame %u"), NextFrameToRender->FrameNumber);

			LastFrameToRender = NextFrameToRender;
			NextFrameToRender->Flags.bSplashIsShown = Splash->IsShown();// || NextFrameToRender->FrameNumber != NextFrameNumber;

			if (GetSplash())
			{
				Splash->StopTicker();
			}

			if (NextFrameToRender->ShowFlags.Rendering && !NextFrameToRender->Flags.bSplashIsShown)
			{
				UE_LOG(LogHMD, Verbose, TEXT("ovrp_WaitToBeginFrame %u"), NextFrameToRender->FrameNumber);

				ovrpResult Result;
				if (OVRP_FAILURE(Result = ovrp_WaitToBeginFrame(NextFrameToRender->FrameNumber)))
				{
					UE_LOG(LogHMD, Error, TEXT("ovrp_WaitToBeginFrame %u failed (%d)"), NextFrameToRender->FrameNumber, Result);
					NextFrameToRender->ShowFlags.Rendering = false;
				}
				else
				{
					NextFrameNumber++;
				}
			}

			FSettingsPtr XSettings = Settings->Clone();
			FGameFramePtr XFrame = NextFrameToRender->Clone();
			TArray<FLayerPtr> XLayers;

			LayerMap.GenerateValueArray(XLayers);

			for (int32 XLayerIndex = 0; XLayerIndex < XLayers.Num(); XLayerIndex++)
			{
				XLayers[XLayerIndex] = XLayers[XLayerIndex]->Clone();
			}

			XLayers.Sort(FLayerPtr_CompareId());

			ExecuteOnRenderThread_DoNotWait([this, XSettings, XFrame, XLayers](FRHICommandListImmediate& RHICmdList)
			{
				if (XFrame.IsValid())
				{
					Settings_RenderThread = XSettings;
					Frame_RenderThread = XFrame;

					int32 XLayerIndex = 0;
					int32 LayerIndex_RenderThread = 0;

					while (XLayerIndex < XLayers.Num() && LayerIndex_RenderThread < Layers_RenderThread.Num())
					{
						uint32 LayerIdA = XLayers[XLayerIndex]->GetId();
						uint32 LayerIdB = Layers_RenderThread[LayerIndex_RenderThread]->GetId();

						if (LayerIdA < LayerIdB)
						{
							XLayers[XLayerIndex++]->Initialize_RenderThread(Settings_RenderThread.Get(), CustomPresent, RHICmdList);
						}
						else if (LayerIdA > LayerIdB)
						{
							LayerIndex_RenderThread++;
						}
						else
						{
							XLayers[XLayerIndex++]->Initialize_RenderThread(Settings_RenderThread.Get(), CustomPresent, RHICmdList, Layers_RenderThread[LayerIndex_RenderThread++].Get());
						}
					}

					while (XLayerIndex < XLayers.Num())
					{
						XLayers[XLayerIndex++]->Initialize_RenderThread(Settings_RenderThread.Get(), CustomPresent, RHICmdList);
					}

					Layers_RenderThread = XLayers;
				}
			});
		}
	}


	void FOculusHMD::FinishRenderFrame_RenderThread(FRHICommandListImmediate& RHICmdList)
	{
		CheckInRenderThread();

		if (Frame_RenderThread.IsValid())
		{
			UE_LOG(LogHMD, VeryVerbose, TEXT("FinishRenderFrame %u"), Frame_RenderThread->FrameNumber);

			if (Frame_RenderThread->ShowFlags.Rendering)
			{
				for (int32 LayerIndex = 0; LayerIndex < Layers_RenderThread.Num(); LayerIndex++)
				{
					Layers_RenderThread[LayerIndex]->UpdateTexture_RenderThread(CustomPresent, RHICmdList);
				}
			}
		}

		Frame_RenderThread.Reset();
	}


	void FOculusHMD::StartRHIFrame_RenderThread()
	{
		CheckInRenderThread();

		if (Frame_RenderThread.IsValid())
		{
			UE_LOG(LogHMD, VeryVerbose, TEXT("StartRHIFrame %u"), Frame_RenderThread->FrameNumber);

			FSettingsPtr XSettings = Settings_RenderThread->Clone();
			FGameFramePtr XFrame = Frame_RenderThread->Clone();
			TArray<FLayerPtr> XLayers = Layers_RenderThread;

			for (int32 XLayerIndex = 0; XLayerIndex < XLayers.Num(); XLayerIndex++)
			{
				XLayers[XLayerIndex] = XLayers[XLayerIndex]->Clone();
			}

			ExecuteOnRHIThread_DoNotWait([this, XSettings, XFrame, XLayers]()
			{
				if (XFrame.IsValid())
				{
					Settings_RHIThread = XSettings;
					Frame_RHIThread = XFrame;
					Layers_RHIThread = XLayers;

					if (Frame_RHIThread->ShowFlags.Rendering && !Frame_RHIThread->Flags.bSplashIsShown)
					{
						UE_LOG(LogHMD, Verbose, TEXT("ovrp_BeginFrame4 %u"), Frame_RHIThread->FrameNumber);						

						ovrpResult Result;
						if (OVRP_FAILURE(Result = ovrp_BeginFrame4(Frame_RHIThread->FrameNumber, CustomPresent->GetOvrpCommandQueue())))
						{
							UE_LOG(LogHMD, Error, TEXT("ovrp_BeginFrame4 %u failed (%d)"), Frame_RHIThread->FrameNumber, Result);
							Frame_RHIThread->ShowFlags.Rendering = false;
						}
						else
						{
#if PLATFORM_ANDROID
							ovrp_SetTiledMultiResLevel((ovrpTiledMultiResLevel)Frame_RHIThread->MultiResLevel);
#endif
						}
					}
				}
			});
		}
	}


	void FOculusHMD::FinishRHIFrame_RHIThread()
	{
		CheckInRHIThread();

		if (Frame_RHIThread.IsValid())
		{
			UE_LOG(LogHMD, VeryVerbose, TEXT("FinishRHIFrame %u"), Frame_RHIThread->FrameNumber);

			if (Frame_RHIThread->ShowFlags.Rendering && !Frame_RHIThread->Flags.bSplashIsShown)
			{
				TArray<FLayerPtr> Layers = Layers_RHIThread;
				Layers.Sort(FLayerPtr_CompareTotal());
				TArray<const ovrpLayerSubmit*> LayerSubmitPtr;

				int32 LayerNum = Layers.Num();

				LayerSubmitPtr.SetNum(LayerNum);

				for (int32 LayerIndex = 0; LayerIndex < LayerNum; LayerIndex++)
				{
					LayerSubmitPtr[LayerIndex] = Layers[LayerIndex]->UpdateLayer_RHIThread(Settings_RHIThread.Get(), Frame_RHIThread.Get(), LayerIndex);
				}

				UE_LOG(LogHMD, Verbose, TEXT("ovrp_EndFrame4 %u"), Frame_RHIThread->FrameNumber);

				ovrpResult Result;
				if (OVRP_FAILURE(Result = ovrp_EndFrame4(Frame_RHIThread->FrameNumber, LayerSubmitPtr.GetData(), LayerSubmitPtr.Num(), CustomPresent->GetOvrpCommandQueue())))
				{
					UE_LOG(LogHMD, Error, TEXT("ovrp_EndFrame4 %u failed (%d)"), Frame_RHIThread->FrameNumber, Result);
				}
				else
				{
					for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); LayerIndex++)
					{
						Layers[LayerIndex]->IncrementSwapChainIndex_RHIThread(CustomPresent);
					}
				}
			}
		}

		Frame_RHIThread.Reset();
	}


	/// @cond DOXYGEN_WARNINGS

#define BOOLEAN_COMMAND_HANDLER_BODY(ConsoleName, FieldExpr)\
	do\
	{\
		if (Args.Num()) \
		{\
			if (Args[0].Equals(TEXT("toggle"), ESearchCase::IgnoreCase))\
			{\
				(FieldExpr) = !(FieldExpr);\
			}\
			else\
			{\
				(FieldExpr) = FCString::ToBool(*Args[0]);\
			}\
		}\
		Ar.Logf(ConsoleName TEXT(" = %s"), (FieldExpr) ? TEXT("On") : TEXT("Off"));\
	}\
	while(false)


	void FOculusHMD::UpdateOnRenderThreadCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		CheckInGameThread();

		BOOLEAN_COMMAND_HANDLER_BODY(TEXT("vr.oculus.bUpdateOnRenderThread"), Settings->Flags.bUpdateOnRT);
	}


	void FOculusHMD::PixelDensityMinCommandHandler(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
	{
		CheckInGameThread();

		if (Args.Num())
		{
			Settings->SetPixelDensityMin(FCString::Atof(*Args[0]));
		}
		Ar.Logf(TEXT("vr.oculus.PixelDensity.min = \"%1.2f\""), Settings->PixelDensityMin);
	}


	void FOculusHMD::PixelDensityMaxCommandHandler(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
	{
		CheckInGameThread();

		if (Args.Num())
		{
			Settings->SetPixelDensityMax(FCString::Atof(*Args[0]));
		}
		Ar.Logf(TEXT("vr.oculus.PixelDensity.max = \"%1.2f\""), Settings->PixelDensityMax);
	}


	void FOculusHMD::HQBufferCommandHandler(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
	{
		CheckInGameThread();

		BOOLEAN_COMMAND_HANDLER_BODY(TEXT("vr.oculus.bHQBuffer"), Settings->Flags.bHQBuffer);
	}


	void FOculusHMD::HQDistortionCommandHandler(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
	{
		CheckInGameThread();

		BOOLEAN_COMMAND_HANDLER_BODY(TEXT("vr.oculus.bHQDistortion"), Settings->Flags.bHQDistortion);
	}

	void FOculusHMD::ShowGlobalMenuCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		CheckInGameThread();

		if (!OVRP_SUCCESS(ovrp_ShowSystemUI2(ovrpUI::ovrpUI_GlobalMenu)))
		{
			Ar.Logf(TEXT("Could not show platform menu"));
		}
	}

	void FOculusHMD::ShowQuitMenuCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		CheckInGameThread();

		if (!OVRP_SUCCESS(ovrp_ShowSystemUI2(ovrpUI::ovrpUI_ConfirmQuit)))
		{
			Ar.Logf(TEXT("Could not show platform menu"));
		}
	}

#if !UE_BUILD_SHIPPING
	void FOculusHMD::StatsCommandHandler(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
	{
		CheckInGameThread();

		BOOLEAN_COMMAND_HANDLER_BODY(TEXT("vr.oculus.Debug.bShowStats"), Settings->Flags.bShowStats);
	}


	void FOculusHMD::ShowSettingsCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		Ar.Logf(TEXT("stereo ipd=%.4f\n nearPlane=%.4f"), GetInterpupillaryDistance(), GNearClippingPlane);
	}


	void FOculusHMD::IPDCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() > 0)
		{
			SetInterpupillaryDistance(FCString::Atof(*Args[0]));
		}
		Ar.Logf(TEXT("vr.oculus.Debug.IPD = %f"), GetInterpupillaryDistance());
	}

#endif // !UE_BUILD_SHIPPING

	void FOculusHMD::LoadFromSettings()
	{
		UOculusHMDRuntimeSettings* HMDSettings = GetMutableDefault<UOculusHMDRuntimeSettings>();
		check(HMDSettings);

		Settings->Flags.bSupportsDash = HMDSettings->bSupportsDash;
		Settings->Flags.bCompositeDepth = HMDSettings->bCompositesDepth;
		Settings->Flags.bHQDistortion = HMDSettings->bHQDistortion;
		Settings->Flags.bChromaAbCorrectionEnabled = HMDSettings->bChromaCorrection;
		Settings->Flags.bRecenterHMDWithController = HMDSettings->bRecenterHMDWithController;
		Settings->MultiResLevel = HMDSettings->FFRLevel;
		Settings->CPULevel = HMDSettings->CPULevel;
		Settings->GPULevel = HMDSettings->GPULevel;
		Settings->PixelDensityMin = HMDSettings->PixelDensityMin;
		Settings->PixelDensityMax = HMDSettings->PixelDensityMax;
	}

	/// @endcond

} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
