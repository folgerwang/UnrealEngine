// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AppleARKitSystem.h"
#include "DefaultXRCamera.h"
#include "AppleARKitSessionDelegate.h"
#include "Misc/ScopeLock.h"
#include "AppleARKitModule.h"
#include "AppleARKitConversion.h"
#include "AppleARKitVideoOverlay.h"
#include "AppleARKitFrame.h"
#include "AppleARKitAnchor.h"
#include "AppleARKitPlaneAnchor.h"
#include "AppleARKitConversion.h"
#include "GeneralProjectSettings.h"
#include "ARSessionConfig.h"
#include "AppleARKitSettings.h"
#include "AppleARKitTrackable.h"
#include "ARLightEstimate.h"
#include "ARTraceResult.h"
#include "ARPin.h"
#include "Async/Async.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/FileHelper.h"

// To separate out the face ar library linkage from standard ar apps
#include "AppleARKitFaceSupport.h"

// For orientation changed
#include "Misc/CoreDelegates.h"

#if PLATFORM_IOS
	#include "IOSRuntimeSettings.h"

	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wpartial-availability"
#endif

DECLARE_CYCLE_STAT(TEXT("SessionDidUpdateFrame_DelegateThread"), STAT_FAppleARKitSystem_SessionUpdateFrame, STATGROUP_ARKIT);
DECLARE_CYCLE_STAT(TEXT("SessionDidAddAnchors_DelegateThread"), STAT_FAppleARKitSystem_SessionDidAddAnchors, STATGROUP_ARKIT);
DECLARE_CYCLE_STAT(TEXT("SessionDidUpdateAnchors_DelegateThread"), STAT_FAppleARKitSystem_SessionDidUpdateAnchors, STATGROUP_ARKIT);
DECLARE_CYCLE_STAT(TEXT("SessionDidRemoveAnchors_DelegateThread"), STAT_FAppleARKitSystem_SessionDidRemoveAnchors, STATGROUP_ARKIT);
DECLARE_CYCLE_STAT(TEXT("UpdateARKitPerf"), STAT_FAppleARKitSystem_UpdateARKitPerf, STATGROUP_ARKIT);
DECLARE_DWORD_COUNTER_STAT(TEXT("ARKit CPU %"), STAT_ARKitThreads, STATGROUP_ARKIT);

// Copied from IOSPlatformProcess because it's not accessible by external code
#define GAME_THREAD_PRIORITY 47
#define RENDER_THREAD_PRIORITY 45

#if PLATFORM_IOS && !PLATFORM_TVOS
// Copied from IOSPlatformProcess because it's not accessible by external code
static void SetThreadPriority(int32 Priority)
{
	struct sched_param Sched;
	FMemory::Memzero(&Sched, sizeof(struct sched_param));
	
	// Read the current priority and policy
	int32 CurrentPolicy = SCHED_RR;
	pthread_getschedparam(pthread_self(), &CurrentPolicy, &Sched);
	
	// Set the new priority and policy (apple recommended FIFO for the two main non-working threads)
	int32 Policy = SCHED_FIFO;
	Sched.sched_priority = Priority;
	pthread_setschedparam(pthread_self(), Policy, &Sched);
}
#else
static void SetThreadPriority(int32 Priority)
{
	// Ignored
}
#endif

//
//  FAppleARKitXRCamera
//

class FAppleARKitXRCamera : public FDefaultXRCamera
{
public:
	FAppleARKitXRCamera(const FAutoRegister& AutoRegister, FAppleARKitSystem& InTrackingSystem, int32 InDeviceId)
	: FDefaultXRCamera( AutoRegister, &InTrackingSystem, InDeviceId )
	, ARKitSystem( InTrackingSystem )
	{}
	
	void AdjustThreadPriority(int32 NewPriority)
	{
		ThreadPriority.Set(NewPriority);
	}
	
private:
	//~ FDefaultXRCamera
	void OverrideFOV(float& InOutFOV)
	{
		// @todo arkit : is it safe not to lock here? Theoretically this should only be called on the game thread.
		ensure(IsInGameThread());
		const bool bShouldOverrideFOV = ARKitSystem.GetARCompositionComponent()->GetSessionConfig().ShouldRenderCameraOverlay();
		if (bShouldOverrideFOV && ARKitSystem.GameThreadFrame.IsValid())
		{
			if (ARKitSystem.DeviceOrientation == EScreenOrientation::Portrait || ARKitSystem.DeviceOrientation == EScreenOrientation::PortraitUpsideDown)
			{
				// Portrait
				InOutFOV = ARKitSystem.GameThreadFrame->Camera.GetVerticalFieldOfViewForScreen(EAppleARKitBackgroundFitMode::Fill);
			}
			else
			{
				// Landscape
				InOutFOV = ARKitSystem.GameThreadFrame->Camera.GetHorizontalFieldOfViewForScreen(EAppleARKitBackgroundFitMode::Fill);
			}
		}
	}
	
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
	{
		FDefaultXRCamera::SetupView(InViewFamily, InView);
	}
	
	virtual void SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData) override
	{
		FDefaultXRCamera::SetupViewProjectionMatrix(InOutProjectionData);
	}
	
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override
	{
		FDefaultXRCamera::BeginRenderViewFamily(InViewFamily);
	}
	
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override
	{
		// Adjust our thread priority if requested
		if (LastThreadPriority.GetValue() != ThreadPriority.GetValue())
		{
			SetThreadPriority(ThreadPriority.GetValue());
			LastThreadPriority.Set(ThreadPriority.GetValue());
		}
		FDefaultXRCamera::PreRenderView_RenderThread(RHICmdList, InView);
	}
	
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override
	{
		// Grab the latest frame from ARKit
		{
			FScopeLock ScopeLock(&ARKitSystem.FrameLock);
			ARKitSystem.RenderThreadFrame = ARKitSystem.LastReceivedFrame;
		}
		
		// @todo arkit: Camera late update? 
		
		if (ARKitSystem.RenderThreadFrame.IsValid())
		{
			VideoOverlay.UpdateVideoTexture_RenderThread(RHICmdList, *ARKitSystem.RenderThreadFrame, InViewFamily);
		}
		
		FDefaultXRCamera::PreRenderViewFamily_RenderThread(RHICmdList, InViewFamily);
	}
	
	virtual void PostRenderBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override
	{
		VideoOverlay.RenderVideoOverlay_RenderThread(RHICmdList, InView, ARKitSystem.DeviceOrientation);
	}
	
	virtual bool IsActiveThisFrame(class FViewport* InViewport) const override
	{
		// Base implementation needs this call as it updates bCurrentFrameIsStereoRendering as a side effect.
		// We'll ignore the result however.
		FDefaultXRCamera::IsActiveThisFrame(InViewport);

		// Check to see if they have disabled the automatic rendering or not
		// Most Face AR apps that are driving other meshes using the face capture (animoji style) will disable this.
		const bool bRenderOverlay =
			ARKitSystem.OnGetARSessionStatus().Status == EARSessionStatus::Running &&
			ARKitSystem.GetARCompositionComponent()->GetSessionConfig().ShouldRenderCameraOverlay();

#if SUPPORTS_ARKIT_1_0
		if (FAppleARKitAvailability::SupportsARKit10())
		{
			return bRenderOverlay;
		}
		else
		{
			return false;
		}
#else
		return false;
#endif
	}
	//~ FDefaultXRCamera
	
private:
	FAppleARKitSystem& ARKitSystem;
	FAppleARKitVideoOverlay VideoOverlay;
	
	// Thread priority support
	FThreadSafeCounter ThreadPriority;
	FThreadSafeCounter LastThreadPriority;
};

//
//  FAppleARKitSystem
//

FAppleARKitSystem::FAppleARKitSystem()
: FXRTrackingSystemBase(this)
, DeviceOrientation(EScreenOrientation::Unknown)
, DerivedTrackingToUnrealRotation(FRotator::ZeroRotator)
, LightEstimate(nullptr)
, CameraImage(nullptr)
, CameraDepth(nullptr)
, LastTrackedGeometry_DebugId(0)
, FaceARSupport(nullptr)
{
	// See Initialize(), as we need access to SharedThis()
}

FAppleARKitSystem::~FAppleARKitSystem()
{
	// Unregister our ability to hit-test in AR with Unreal
}

void FAppleARKitSystem::Shutdown()
{
#if SUPPORTS_ARKIT_1_0
	if (Session != nullptr)
	{
		FaceARSupport = nullptr;
		[Session pause];
		Session.delegate = nullptr;
		[Session release];
		Session = nullptr;
	}
#endif
	CameraDepth = nullptr;
	CameraImage = nullptr;
}

void FAppleARKitSystem::CheckForFaceARSupport(UARSessionConfig* InSessionConfig)
{
	if (InSessionConfig->GetSessionType() != EARSessionType::Face)
	{
		// Clear the face ar support so we don't forward to it
		FaceARSupport = nullptr;
		return;
	}

	// We need to get the face support from the factory method, which is a modular feature to avoid dependencies
	TArray<IAppleARKitFaceSupport*> Impls = IModularFeatures::Get().GetModularFeatureImplementations<IAppleARKitFaceSupport>("AppleARKitFaceSupport");
	if (ensureAlwaysMsgf(Impls.Num() > 0, TEXT("Face AR session has been requested but the face ar plugin is not enabled")))
	{
		FaceARSupport = Impls[0];
		ensureAlwaysMsgf(FaceARSupport != nullptr, TEXT("Face AR session has been requested but the face ar plugin is not enabled"));
	}
}

FName FAppleARKitSystem::GetSystemName() const
{
	static const FName AppleARKitSystemName(TEXT("AppleARKit"));
	return AppleARKitSystemName;
}

bool FAppleARKitSystem::GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition)
{
	if (DeviceId == IXRTrackingSystem::HMDDeviceId && GameThreadFrame.IsValid() && IsHeadTrackingAllowed())
	{
		// Do not have to lock here, because we are on the game
		// thread and GameThreadFrame is only written to from the game thread.
		
		
		// Apply alignment transform if there is one.
		FTransform CurrentTransform(GameThreadFrame->Camera.Orientation, GameThreadFrame->Camera.Translation);
		CurrentTransform = FTransform(DerivedTrackingToUnrealRotation) * CurrentTransform;
		CurrentTransform *= GetARCompositionComponent()->GetAlignmentTransform();
		
		
		// Apply counter-rotation to compensate for mobile device orientation
		OutOrientation = CurrentTransform.GetRotation();
		OutPosition = CurrentTransform.GetLocation();

		return true;
	}
	else
	{
		return false;
	}
}

FString FAppleARKitSystem::GetVersionString() const
{
	return TEXT("AppleARKit - V1.0");
}


bool FAppleARKitSystem::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		static const int32 DeviceId = IXRTrackingSystem::HMDDeviceId;
		OutDevices.Add(DeviceId);
		return true;
	}
	return false;
}

void FAppleARKitSystem::CalcTrackingToWorldRotation()
{
	// We rotate the camera to counteract the portrait vs. landscape viewport rotation
	DerivedTrackingToUnrealRotation = FRotator::ZeroRotator;

	const EARWorldAlignment WorldAlignment = GetARCompositionComponent()->GetSessionConfig().GetWorldAlignment();
	if (WorldAlignment == EARWorldAlignment::Gravity || WorldAlignment == EARWorldAlignment::GravityAndHeading)
	{
		switch (DeviceOrientation)
		{
			case EScreenOrientation::Portrait:
				DerivedTrackingToUnrealRotation = FRotator(0.0f, 0.0f, -90.0f);
				break;
				
			case EScreenOrientation::PortraitUpsideDown:
				DerivedTrackingToUnrealRotation = FRotator(0.0f, 0.0f, 90.0f);
				break;
				
			default:
			case EScreenOrientation::LandscapeLeft:
				break;
				
			case EScreenOrientation::LandscapeRight:
				DerivedTrackingToUnrealRotation = FRotator(0.0f, 0.0f, 180.0f);
				break;
		}
	}
	// Camera aligned which means +X is to the right along the long axis
	else
	{
		switch (DeviceOrientation)
		{
			case EScreenOrientation::Portrait:
				DerivedTrackingToUnrealRotation = FRotator(0.0f, 0.0f, 90.0f);
				break;
				
			case EScreenOrientation::PortraitUpsideDown:
				DerivedTrackingToUnrealRotation = FRotator(0.0f, 0.0f, -90.0f);
				break;
				
			default:
			case EScreenOrientation::LandscapeLeft:
				DerivedTrackingToUnrealRotation = FRotator(0.0f, 0.0f, -180.0f);
				break;
				
			case EScreenOrientation::LandscapeRight:
				break;
		}
	}
}

void FAppleARKitSystem::UpdateFrame()
{
	FScopeLock ScopeLock( &FrameLock );
	// This might get called multiple times per frame so only update if delegate version is newer
	if (!GameThreadFrame.IsValid() || !LastReceivedFrame.IsValid() ||
		GameThreadFrame->Timestamp < LastReceivedFrame->Timestamp)
	{
		GameThreadFrame = LastReceivedFrame;
		if (GameThreadFrame.IsValid())
		{
#if SUPPORTS_ARKIT_1_0
			if (GameThreadFrame->CameraImage != nullptr)
			{
				// Only create a new camera image texture if it's set and we don't already have one
				if (CameraImage == nullptr)
				{
					CameraImage = NewObject<UAppleARKitTextureCameraImage>();
				}
				// Reuse the UObjects because otherwise the time between GCs causes ARKit to be starved of resources
                CameraImage->Init(FPlatformTime::Seconds(), GameThreadFrame->CameraImage);
			}

			if (GameThreadFrame->CameraDepth != nullptr)
			{
				// Only create a new camera depth texture if it's set and we don't already have one
				if (CameraDepth == nullptr)
				{
					CameraDepth = NewObject<UAppleARKitTextureCameraDepth>();
				}
				// Reuse the UObjects because otherwise the time between GCs causes ARKit to be starved of resources
                CameraDepth->Init(FPlatformTime::Seconds(), GameThreadFrame->CameraDepth);
			}
#endif
		}
	}
}

void FAppleARKitSystem::UpdatePoses()
{
	UpdateFrame();
}


void FAppleARKitSystem::ResetOrientationAndPosition(float Yaw)
{
	// @todo arkit implement FAppleARKitSystem::ResetOrientationAndPosition
}

bool FAppleARKitSystem::IsHeadTrackingAllowed() const
{
	// Check to see if they have disabled the automatic camera tracking or not
	// For face AR tracking movements of the device most likely won't want to be tracked
	const bool bEnableCameraTracking =
		OnGetARSessionStatus().Status == EARSessionStatus::Running &&
		GetARCompositionComponent()->GetSessionConfig().ShouldEnableCameraTracking();

#if SUPPORTS_ARKIT_1_0
	if (FAppleARKitAvailability::SupportsARKit10())
	{
		return bEnableCameraTracking;
	}
	else
	{
		return false;
	}
#else
	return false;
#endif
}

TSharedPtr<class IXRCamera, ESPMode::ThreadSafe> FAppleARKitSystem::GetXRCamera(int32 DeviceId)
{
	if (!XRCamera.IsValid())
	{
		TSharedRef<FAppleARKitXRCamera, ESPMode::ThreadSafe> NewCamera = FSceneViewExtensions::NewExtension<FAppleARKitXRCamera>(*this, DeviceId);
		XRCamera = NewCamera;
	}
	
	return XRCamera;
}

float FAppleARKitSystem::GetWorldToMetersScale() const
{
	// @todo arkit FAppleARKitSystem::GetWorldToMetersScale needs a real scale somehow
	return 100.0f;
}

void FAppleARKitSystem::OnBeginRendering_GameThread()
{
	UpdatePoses();
}

bool FAppleARKitSystem::OnStartGameFrame(FWorldContext& WorldContext)
{
	FXRTrackingSystemBase::OnStartGameFrame(WorldContext);
	
	CachedTrackingToWorld = ComputeTrackingToWorldTransform(WorldContext);
	
	if (GameThreadFrame.IsValid())
	{
		if (GameThreadFrame->LightEstimate.bIsValid)
		{
			UARBasicLightEstimate* NewLightEstimate = NewObject<UARBasicLightEstimate>();
			NewLightEstimate->SetLightEstimate( GameThreadFrame->LightEstimate.AmbientIntensity,  GameThreadFrame->LightEstimate.AmbientColorTemperatureKelvin);
			LightEstimate = NewLightEstimate;
		}
		else
		{
			LightEstimate = nullptr;
		}
		
	}
	
	return true;
}

void* FAppleARKitSystem::GetARSessionRawPointer()
{
#if SUPPORTS_ARKIT_1_0
	return static_cast<void*>(Session);
#endif
	ensureAlwaysMsgf(false, TEXT("FAppleARKitSystem::GetARSessionRawPointer is unimplemented on current platform."));
	return nullptr;
}

void* FAppleARKitSystem::GetGameThreadARFrameRawPointer()
{
#if SUPPORTS_ARKIT_1_0
	if (GameThreadFrame.IsValid())
	{
		return GameThreadFrame->NativeFrame;
	}
	else
	{
		return nullptr;
	}
#endif
	ensureAlwaysMsgf(false, TEXT("FAppleARKitSystem::GetARGameThreadFrameRawPointer is unimplemented on current platform."));
	return nullptr;
}

//bool FAppleARKitSystem::ARLineTraceFromScreenPoint(const FVector2D ScreenPosition, TArray<FARTraceResult>& OutHitResults)
//{
//	const bool bSuccess = HitTestAtScreenPosition(ScreenPosition, EAppleARKitHitTestResultType::ExistingPlaneUsingExtent, OutHitResults);
//	return bSuccess;
//}

void FAppleARKitSystem::OnARSystemInitialized()
{
	// Register for device orientation changes
	FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate.AddThreadSafeSP(this, &FAppleARKitSystem::OrientationChanged);
}

EARTrackingQuality FAppleARKitSystem::OnGetTrackingQuality() const
{
	return GameThreadFrame.IsValid()
		? GameThreadFrame->Camera.TrackingQuality
		: EARTrackingQuality::NotTracking;
}

void FAppleARKitSystem::OnStartARSession(UARSessionConfig* SessionConfig)
{
	Run(SessionConfig);
}

void FAppleARKitSystem::OnPauseARSession()
{
	ensureAlwaysMsgf(false, TEXT("FAppleARKitSystem::OnPauseARSession() is unimplemented."));
}

void FAppleARKitSystem::OnStopARSession()
{
	Pause();
}

FARSessionStatus FAppleARKitSystem::OnGetARSessionStatus() const
{
	return IsRunning()
		? FARSessionStatus(EARSessionStatus::Running)
		: FARSessionStatus(EARSessionStatus::NotStarted);
}

void FAppleARKitSystem::OnSetAlignmentTransform(const FTransform& InAlignmentTransform)
{
	const FTransform& NewAlignmentTransform = InAlignmentTransform;
	
	// Update transform for all geometries
	for (auto GeoIt=TrackedGeometries.CreateIterator(); GeoIt; ++GeoIt)
	{
		GeoIt.Value()->UpdateAlignmentTransform(NewAlignmentTransform);
	}
	
	// Update transform for all Pins
	for (UARPin* Pin : Pins)
	{
		Pin->UpdateAlignmentTransform(NewAlignmentTransform);
	}
}

static bool IsHitInRange( float UnrealHitDistance )
{
    // Skip results further than 5m or closer that 20cm from camera
	return 20.0f < UnrealHitDistance && UnrealHitDistance < 500.0f;
}

#if SUPPORTS_ARKIT_1_0

static UARTrackedGeometry* FindGeometryFromAnchor( ARAnchor* InAnchor, TMap<FGuid, UARTrackedGeometry*>& Geometries )
{
	if (InAnchor != NULL)
	{
		const FGuid AnchorGUID = FAppleARKitConversion::ToFGuid( InAnchor.identifier );
		UARTrackedGeometry** Result = Geometries.Find(AnchorGUID);
		if (Result != nullptr)
		{
			return *Result;
		}
	}
	
	return nullptr;
}

#endif

TArray<FARTraceResult> FAppleARKitSystem::OnLineTraceTrackedObjects( const FVector2D ScreenCoord, EARLineTraceChannels TraceChannels )
{
	const float WorldToMetersScale = GetWorldToMetersScale();
	TArray<FARTraceResult> Results;
	
	// Sanity check
	if (IsRunning())
	{
#if SUPPORTS_ARKIT_1_0
		
		TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> This = GetARCompositionComponent();
		
		@autoreleasepool
		{
			// Perform a hit test on the Session's last frame
			ARFrame* HitTestFrame = Session.currentFrame;
			if (HitTestFrame)
			{
				Results.Reserve(8);
				
				// Convert the screen position to normalised coordinates in the capture image space
				FVector2D NormalizedImagePosition = FAppleARKitCamera( HitTestFrame.camera ).GetImageCoordinateForScreenPosition( ScreenCoord, EAppleARKitBackgroundFitMode::Fill );
				switch (DeviceOrientation)
				{
					case EScreenOrientation::Portrait:
						NormalizedImagePosition = FVector2D( NormalizedImagePosition.Y, 1.0f - NormalizedImagePosition.X );
						break;
						
					case EScreenOrientation::PortraitUpsideDown:
						NormalizedImagePosition = FVector2D( 1.0f - NormalizedImagePosition.Y, NormalizedImagePosition.X );
						break;
						
					default:
					case EScreenOrientation::LandscapeLeft:
						break;
						
					case EScreenOrientation::LandscapeRight:
						NormalizedImagePosition = FVector2D(1.0f, 1.0f) - NormalizedImagePosition;
						break;
				};
				
				// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("Hit Test At Screen Position: x: %f, y: %f"), NormalizedImagePosition.X, NormalizedImagePosition.Y));
				
				// First run hit test against existing planes with extents (converting & filtering results as we go)
				if (!!(TraceChannels & EARLineTraceChannels::PlaneUsingExtent) || !!(TraceChannels & EARLineTraceChannels::PlaneUsingBoundaryPolygon))
				{
					// First run hit test against existing planes with extents (converting & filtering results as we go)
					NSArray< ARHitTestResult* >* PlaneHitTestResults = [HitTestFrame hitTest:CGPointMake(NormalizedImagePosition.X, NormalizedImagePosition.Y) types:ARHitTestResultTypeExistingPlaneUsingExtent];
					for ( ARHitTestResult* HitTestResult in PlaneHitTestResults )
					{
						const float UnrealHitDistance = HitTestResult.distance * WorldToMetersScale;
						if ( IsHitInRange( UnrealHitDistance ) )
						{
							// Hit result has passed and above filtering, add it to the list
							// Convert to Unreal's Hit Test result format
							Results.Add(FARTraceResult(This, UnrealHitDistance, EARLineTraceChannels::PlaneUsingExtent, FAppleARKitConversion::ToFTransform(HitTestResult.worldTransform)*GetARCompositionComponent()->GetAlignmentTransform(), FindGeometryFromAnchor(HitTestResult.anchor, TrackedGeometries)));
						}
					}
				}
				
				// If there were no valid results, fall back to hit testing against one shot plane
				if (!!(TraceChannels & EARLineTraceChannels::GroundPlane))
				{
					NSArray< ARHitTestResult* >* PlaneHitTestResults = [HitTestFrame hitTest:CGPointMake(NormalizedImagePosition.X, NormalizedImagePosition.Y) types:ARHitTestResultTypeEstimatedHorizontalPlane];
					for ( ARHitTestResult* HitTestResult in PlaneHitTestResults )
					{
						const float UnrealHitDistance = HitTestResult.distance * WorldToMetersScale;
						if ( IsHitInRange( UnrealHitDistance ) )
						{
							// Hit result has passed and above filtering, add it to the list
							// Convert to Unreal's Hit Test result format
							Results.Add(FARTraceResult(This, UnrealHitDistance, EARLineTraceChannels::GroundPlane, FAppleARKitConversion::ToFTransform(HitTestResult.worldTransform)*GetARCompositionComponent()->GetAlignmentTransform(), FindGeometryFromAnchor(HitTestResult.anchor, TrackedGeometries)));
						}
					}
				}
				
				// If there were no valid results, fall back further to hit testing against feature points
				if (!!(TraceChannels & EARLineTraceChannels::FeaturePoint))
				{
					// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("No results for plane hit test - reverting to feature points"), NormalizedImagePosition.X, NormalizedImagePosition.Y));
					
					NSArray< ARHitTestResult* >* FeatureHitTestResults = [HitTestFrame hitTest:CGPointMake(NormalizedImagePosition.X, NormalizedImagePosition.Y) types:ARHitTestResultTypeFeaturePoint];
					for ( ARHitTestResult* HitTestResult in FeatureHitTestResults )
					{
						const float UnrealHitDistance = HitTestResult.distance * WorldToMetersScale;
						if ( IsHitInRange( UnrealHitDistance ) )
						{
							// Hit result has passed and above filtering, add it to the list
							// Convert to Unreal's Hit Test result format
							Results.Add( FARTraceResult( This, UnrealHitDistance, EARLineTraceChannels::FeaturePoint, FAppleARKitConversion::ToFTransform( HitTestResult.worldTransform )*GetARCompositionComponent()->GetAlignmentTransform(), FindGeometryFromAnchor(HitTestResult.anchor, TrackedGeometries) ) );
						}
					}
				}
			}
		}
#endif
	}
	
	if (Results.Num() > 1)
	{
		Results.Sort([](const FARTraceResult& A, const FARTraceResult& B)
		{
			return A.GetDistanceFromCamera() < B.GetDistanceFromCamera();
		});
	}
	
	return Results;
}

TArray<FARTraceResult> FAppleARKitSystem::OnLineTraceTrackedObjects(const FVector Start, const FVector End, EARLineTraceChannels TraceChannels)
{
	UE_LOG(LogAppleARKit, Warning, TEXT("FAppleARKitSystem::OnLineTraceTrackedObjects(Start, End, TraceChannels) is currently unsupported.  No results will be returned."))
	TArray<FARTraceResult> EmptyResults;
	return EmptyResults;
}

TArray<UARTrackedGeometry*> FAppleARKitSystem::OnGetAllTrackedGeometries() const
{
	TArray<UARTrackedGeometry*> Geometries;
	TrackedGeometries.GenerateValueArray(Geometries);
	return Geometries;
}

TArray<UARPin*> FAppleARKitSystem::OnGetAllPins() const
{
	return Pins;
}

UARTextureCameraImage* FAppleARKitSystem::OnGetCameraImage()
{
	return CameraImage;
}

UARTextureCameraDepth* FAppleARKitSystem::OnGetCameraDepth()
{
	return CameraDepth;
}

UARLightEstimate* FAppleARKitSystem::OnGetCurrentLightEstimate() const
{
	return LightEstimate;
}

UARPin* FAppleARKitSystem::OnPinComponent( USceneComponent* ComponentToPin, const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry, const FName DebugName )
{
	if ( ensureMsgf(ComponentToPin != nullptr, TEXT("Cannot pin component.")) )
	{
		if (UARPin* FindResult = ARKitUtil::PinFromComponent(ComponentToPin, Pins))
		{
			UE_LOG(LogAppleARKit, Warning, TEXT("Component %s is already pinned. Unpin it first."), *ComponentToPin->GetReadableName());
			OnRemovePin(FindResult);
		}

		// PinToWorld * AlignedTrackingToWorld(-1) * TrackingToAlignedTracking(-1) = PinToWorld * WorldToAlignedTracking * AlignedTrackingToTracking
		// The Worlds and AlignedTracking cancel out, and we get PinToTracking
		// But we must translate this logic into Unreal's transform API
		const FTransform& TrackingToAlignedTracking = GetARCompositionComponent()->GetAlignmentTransform();
		const FTransform PinToTrackingTransform = PinToWorldTransform.GetRelativeTransform(GetTrackingToWorldTransform()).GetRelativeTransform(TrackingToAlignedTracking);

		// If the user did not provide a TrackedGeometry, create the simplest TrackedGeometry for this pin.
		UARTrackedGeometry* GeometryToPinTo = TrackedGeometry;
		if (GeometryToPinTo == nullptr)
		{
			double UpdateTimestamp = FPlatformTime::Seconds();
			
			GeometryToPinTo = NewObject<UARTrackedPoint>();
			GeometryToPinTo->UpdateTrackedGeometry(GetARCompositionComponent().ToSharedRef(), 0, FPlatformTime::Seconds(), PinToTrackingTransform, GetARCompositionComponent()->GetAlignmentTransform());
		}
		
		UARPin* NewPin = NewObject<UARPin>();
		NewPin->InitARPin(GetARCompositionComponent().ToSharedRef(), ComponentToPin, PinToTrackingTransform, GeometryToPinTo, DebugName);
		
		Pins.Add(NewPin);
		
		return NewPin;
	}
	else
	{
		return nullptr;
	}
}

void FAppleARKitSystem::OnRemovePin(UARPin* PinToRemove)
{
	Pins.RemoveSingleSwap(PinToRemove);
}

bool FAppleARKitSystem::GetCurrentFrame(FAppleARKitFrame& OutCurrentFrame) const
{
	if( GameThreadFrame.IsValid() )
	{
		OutCurrentFrame = *GameThreadFrame;
		return true;
	}
	else
	{
		return false;
	}
}

bool FAppleARKitSystem::OnIsTrackingTypeSupported(EARSessionType SessionType) const
{
#if SUPPORTS_ARKIT_1_0
	switch (SessionType)
	{
		case EARSessionType::Orientation:
		{
			return AROrientationTrackingConfiguration.isSupported == TRUE;
		}
		case EARSessionType::World:
		{
			return ARWorldTrackingConfiguration.isSupported == TRUE;
		}
		case EARSessionType::Face:
		{
			// We need to get the face support from the factory method, which is a modular feature to avoid dependencies
			TArray<IAppleARKitFaceSupport*> Impls = IModularFeatures::Get().GetModularFeatureImplementations<IAppleARKitFaceSupport>("AppleARKitFaceSupport");
			if (Impls.Num() > 0 && Impls[0] != nullptr)
			{
				return Impls[0]->DoesSupportFaceAR();
			}
			return false;
		}
	}
#endif
	return false;
}

bool FAppleARKitSystem::OnAddManualEnvironmentCaptureProbe(FVector Location, FVector Extent)
{
#if SUPPORTS_ARKIT_2_0
	if (Session != nullptr)
	{
		if (FAppleARKitAvailability::SupportsARKit20())
		{
//@joeg -- Todo need to fix this transform as it needs to use the alignment transform too
			// Build and add the anchor
			simd_float4x4 AnchorMatrix = FAppleARKitConversion::ToARKitMatrix(FTransform(Location));
			simd_float3 AnchorExtent = FAppleARKitConversion::ToARKitVector(Extent * 2.f);
			AREnvironmentProbeAnchor* ARProbe = [[AREnvironmentProbeAnchor alloc] initWithTransform: AnchorMatrix extent: AnchorExtent];
			[Session addAnchor: ARProbe];
			[ARProbe release];
		}
		return true;
	}
#endif
	return false;
}

TArray<FARVideoFormat> FAppleARKitSystem::OnGetSupportedVideoFormats(EARSessionType SessionType) const
{
#if SUPPORTS_ARKIT_1_5
	if (FAppleARKitAvailability::SupportsARKit15())
	{
		switch (SessionType)
		{
			case EARSessionType::Face:
			{
				// We need to get the face support from the factory method, which is a modular feature to avoid dependencies
				TArray<IAppleARKitFaceSupport*> Impls = IModularFeatures::Get().GetModularFeatureImplementations<IAppleARKitFaceSupport>("AppleARKitFaceSupport");
				break;
			}
			case EARSessionType::World:
			{
				return FAppleARKitConversion::FromARVideoFormatArray(ARWorldTrackingConfiguration.supportedVideoFormats);
			}
		}
	}
#endif
	return TArray<FARVideoFormat>();
}

TArray<FVector> FAppleARKitSystem::OnGetPointCloud() const
{
	TArray<FVector> PointCloud;
	
#if SUPPORTS_ARKIT_1_0
	if (GameThreadFrame.IsValid())
	{
		ARFrame* InARFrame = (ARFrame*)GameThreadFrame->NativeFrame;
		ARPointCloud* InARPointCloud = InARFrame.rawFeaturePoints;
		if (InARPointCloud != nullptr)
		{
			const int32 Count = InARPointCloud.count;
			PointCloud.Empty(Count);
			PointCloud.AddUninitialized(Count);
			for (int32 Index = 0; Index < Count; Index++)
			{
				PointCloud[Index] = FAppleARKitConversion::ToFVector(InARPointCloud.points[Index]);
			}
		}
	}
#endif
	return PointCloud;
}

#if SUPPORTS_ARKIT_2_0
/** Since both the object extraction and world saving need to get the world map async, use a common chunk of code for this */
class FAppleARKitGetWorldMapObjectAsyncTask
{
public:
	/** Performs the call to get the world map and triggers the OnWorldMapAcquired() the completion handler */
	void Run()
	{
		[Session getCurrentWorldMapWithCompletionHandler: ^(ARWorldMap* worldMap, NSError* error)
		 {
			 WorldMap = worldMap;
			 [WorldMap retain];
			 bool bWasSuccessful = error == nullptr;
			 FString ErrorString;
			 if (error != nullptr)
			 {
				 ErrorString = [error localizedDescription];
			 }
			 OnWorldMapAcquired(bWasSuccessful, ErrorString);
		 }];
	}
	
protected:
	FAppleARKitGetWorldMapObjectAsyncTask(ARSession* InSession) :
		Session(InSession)
	{
		CFRetain(Session);
	}
	
	void Release()
	{
		if (Session != nullptr)
		{
			[Session release];
			Session = nullptr;
		}
		if (WorldMap != nullptr)
		{
			[WorldMap release];
			WorldMap = nullptr;
		}
	}

	/** Called once the world map completion handler is called */
	virtual void OnWorldMapAcquired(bool bWasSuccessful, FString ErrorString) = 0;

	/** The session object that we'll grab the world from */
	ARSession* Session;
	/** The world map object once the call has completed */
	ARWorldMap* WorldMap;
};

//@joeg -- The API changed last minute so you don't need to resolve the world to get an object anymore
// This needs to be cleaned up
class FAppleARKitGetCandidateObjectAsyncTask :
	public FARGetCandidateObjectAsyncTask
{
public:
	FAppleARKitGetCandidateObjectAsyncTask(ARSession* InSession, FVector InLocation, FVector InExtent) :
		Location(InLocation)
		, Extent(InExtent)
		, ReferenceObject(nullptr)
		, Session(InSession)
	{
		[Session retain];
	}
	
	/** @return the candidate object that you can use for detection later */
	virtual UARCandidateObject* GetCandidateObject() override
	{
		if (ReferenceObject != nullptr)
		{
			UARCandidateObject* CandidateObject = NewObject<UARCandidateObject>();
			
			FVector RefObjCenter = FAppleARKitConversion::ToFVector(ReferenceObject.center);
			FVector RefObjExtent = 0.5f * FAppleARKitConversion::ToFVector(ReferenceObject.extent);
			FBox BoundingBox(RefObjCenter, RefObjExtent);
			CandidateObject->SetBoundingBox(BoundingBox);
			
			// Serialize the object into a byte array and stick that on the candidate object
			NSError* ErrorObj = nullptr;
			NSData* RefObjData = [NSKeyedArchiver archivedDataWithRootObject: ReferenceObject requiringSecureCoding: YES error: &ErrorObj];
			uint32 SavedSize = RefObjData.length;
			TArray<uint8> RawBytes;
			RawBytes.AddUninitialized(SavedSize);
			FPlatformMemory::Memcpy(RawBytes.GetData(), [RefObjData bytes], SavedSize);
			CandidateObject->SetCandidateObjectData(RawBytes);

			return CandidateObject;
		}
		return nullptr;
	}
	
	virtual ~FAppleARKitGetCandidateObjectAsyncTask()
	{
		[Session release];
		if (ReferenceObject != nullptr)
		{
			CFRelease(ReferenceObject);
		}
	}

	void Run()
	{
		simd_float4x4 ARMatrix = FAppleARKitConversion::ToARKitMatrix(FTransform(Location));
		simd_float3 Center = 0.f;
		simd_float3 ARExtent = FAppleARKitConversion::ToARKitVector(Extent * 2.f);

		[Session createReferenceObjectWithTransform: ARMatrix center: Center extent: ARExtent
		 completionHandler: ^(ARReferenceObject* refObject, NSError* error)
		{
			ReferenceObject = refObject;
			bool bWasSuccessful = error == nullptr;
			bHadError = error != nullptr;
			FString ErrorString;
			if (error != nullptr)
			{
				ErrorString = [error localizedDescription];
			}
			bIsDone = true;
		}];
	}
	
private:
	FVector Location;
	FVector Extent;
	ARReferenceObject* ReferenceObject;

	/** The session object that we'll grab the object from */
	ARSession* Session;
};

class FAppleARKitSaveWorldAsyncTask :
	public FARSaveWorldAsyncTask,
	public FAppleARKitGetWorldMapObjectAsyncTask
{
public:
	FAppleARKitSaveWorldAsyncTask(ARSession* InSession) :
		FAppleARKitGetWorldMapObjectAsyncTask(InSession)
	{
	}

	virtual ~FAppleARKitSaveWorldAsyncTask()
	{
		Release();
	}

	virtual void OnWorldMapAcquired(bool bWasSuccessful, FString ErrorString) override
	{
		if (bWasSuccessful)
		{
			NSError* ErrorObj = nullptr;
			NSData* WorldNSData = [NSKeyedArchiver archivedDataWithRootObject: WorldMap requiringSecureCoding: YES error: &ErrorObj];
			if (ErrorObj == nullptr)
			{
				int32 UncompressedSize = WorldNSData.length;
				
				TArray<uint8> CompressedData;
				CompressedData.AddUninitialized(WorldNSData.length + AR_SAVE_WORLD_HEADER_SIZE);
				uint8* Buffer = (uint8*)CompressedData.GetData();
				// Write our magic header into our buffer
				FARWorldSaveHeader& Header = *(FARWorldSaveHeader*)Buffer;
				Header = FARWorldSaveHeader();
				Header.UncompressedSize = UncompressedSize;
				
				// Compress the data
				uint8* CompressInto = Buffer + AR_SAVE_WORLD_HEADER_SIZE;
				int32 CompressedSize = UncompressedSize;
				uint8* UncompressedData = (uint8*)[WorldNSData bytes];
				verify(FCompression::CompressMemory(NAME_Zlib, CompressInto, CompressedSize, UncompressedData, UncompressedSize));
				
				// Only copy out the amount of compressed data and the header
				int32 CompressedSizePlusHeader = CompressedSize + AR_SAVE_WORLD_HEADER_SIZE;
				WorldData.AddUninitialized(CompressedSizePlusHeader);
				FPlatformMemory::Memcpy(WorldData.GetData(), CompressedData.GetData(), CompressedSizePlusHeader);
			}
			else
			{
				Error = [ErrorObj localizedDescription];
				bHadError = true;
			}
		}
		else
		{
			Error = ErrorString;
			bHadError = true;
		}
		// Trigger that we're done
		bIsDone = true;
	}
};
#endif

TSharedPtr<FARGetCandidateObjectAsyncTask, ESPMode::ThreadSafe> FAppleARKitSystem::OnGetCandidateObject(FVector Location, FVector Extent) const
{
#if SUPPORTS_ARKIT_2_0
	if (Session != nullptr)
	{
		if (FAppleARKitAvailability::SupportsARKit20())
		{
			TSharedPtr<FAppleARKitGetCandidateObjectAsyncTask, ESPMode::ThreadSafe> Task = MakeShared<FAppleARKitGetCandidateObjectAsyncTask, ESPMode::ThreadSafe>(Session, Location, Extent);
			Task->Run();
			return Task;
		}
	}
#endif
	return  MakeShared<FARErrorGetCandidateObjectAsyncTask, ESPMode::ThreadSafe>(TEXT("GetCandidateObject - requires a valid, running ARKit 2.0 session"));
}

TSharedPtr<FARSaveWorldAsyncTask, ESPMode::ThreadSafe> FAppleARKitSystem::OnSaveWorld() const
{
#if SUPPORTS_ARKIT_2_0
	if (Session != nullptr)
	{
		if (FAppleARKitAvailability::SupportsARKit20())
		{
			TSharedPtr<FAppleARKitSaveWorldAsyncTask, ESPMode::ThreadSafe> Task = MakeShared<FAppleARKitSaveWorldAsyncTask, ESPMode::ThreadSafe>(Session);
			Task->Run();
			return Task;
		}
	}
#endif
	return  MakeShared<FARErrorSaveWorldAsyncTask, ESPMode::ThreadSafe>(TEXT("SaveWorld - requires a valid, running ARKit 2.0 session"));
}

EARWorldMappingState FAppleARKitSystem::OnGetWorldMappingStatus() const
{
	if (GameThreadFrame.IsValid())
	{
		return GameThreadFrame->WorldMappingState;
	}
	return EARWorldMappingState::NotAvailable;
}


void FAppleARKitSystem::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObjects( TrackedGeometries );
	Collector.AddReferencedObjects( Pins );
	Collector.AddReferencedObject( CameraImage );
	Collector.AddReferencedObject( CameraDepth );
	Collector.AddReferencedObjects( CandidateImages );
	Collector.AddReferencedObjects( CandidateObjects );
	Collector.AddReferencedObject( TimecodeProvider );

	if(LightEstimate)
	{
		Collector.AddReferencedObject(LightEstimate);
	}
}

bool FAppleARKitSystem::HitTestAtScreenPosition(const FVector2D ScreenPosition, EAppleARKitHitTestResultType InTypes, TArray< FAppleARKitHitTestResult >& OutResults)
{
	ensureMsgf(false,TEXT("UNIMPLEMENTED; see OnLineTraceTrackedObjects()"));
	return false;
}

static TOptional<EScreenOrientation::Type> PickAllowedDeviceOrientation( EScreenOrientation::Type InOrientation )
{
#if SUPPORTS_ARKIT_1_0
	const UIOSRuntimeSettings* IOSSettings = GetDefault<UIOSRuntimeSettings>();
	
	const bool bOrientationSupported[] =
	{
		true, // Unknown
		IOSSettings->bSupportsPortraitOrientation != 0, // Portait
		IOSSettings->bSupportsUpsideDownOrientation != 0, // PortraitUpsideDown
		IOSSettings->bSupportsLandscapeRightOrientation != 0, // LandscapeLeft; These are flipped vs the enum name?
		IOSSettings->bSupportsLandscapeLeftOrientation != 0, // LandscapeRight; These are flipped vs the enum name?
		false, // FaceUp
		false // FaceDown
	};
	
	if (bOrientationSupported[static_cast<int32>(InOrientation)])
	{
		return InOrientation;
	}
	else
	{
		return TOptional<EScreenOrientation::Type>();
	}
#else
	return TOptional<EScreenOrientation::Type>();
#endif
}

void FAppleARKitSystem::SetDeviceOrientation( EScreenOrientation::Type InOrientation )
{
	TOptional<EScreenOrientation::Type> NewOrientation = PickAllowedDeviceOrientation(InOrientation);

	if (!NewOrientation.IsSet() && DeviceOrientation == EScreenOrientation::Unknown)
	{
		// We do not currently have a valid orientation, nor did the device provide one.
		// So pick ANY ALLOWED default.
		// This only realy happens if the device is face down on something or
		// in another "useless" state for AR.

		// Note: the order in which this selection is done is important and must match that 
		// established in UEDeployIOS.cs and written into UISupportedInterfaceOrientations.
		// IOSView preferredInterfaceOrientationForPresentation presumably also should match.
		//
		// However it would very likely be better to hook statusBarOrientation instead of deviceOrientation to update
		// our orientation, in which case we would only need to handle unknown.
		
		if (!NewOrientation.IsSet())
		{
			NewOrientation = PickAllowedDeviceOrientation(EScreenOrientation::Portrait);
		}

		if (!NewOrientation.IsSet())
		{
			NewOrientation = PickAllowedDeviceOrientation(EScreenOrientation::PortraitUpsideDown);
		}

#if SUPPORTS_ARKIT_1_0
		const UIOSRuntimeSettings* IOSSettings = GetDefault<UIOSRuntimeSettings>();
		const bool bPreferLandscapeLeftHomeButton = IOSSettings->PreferredLandscapeOrientation == EIOSLandscapeOrientation::LandscapeLeft;
#else
		const bool bPreferLandscapeLeftHomeButton = true;
#endif
		if (bPreferLandscapeLeftHomeButton)
		{
			if (!NewOrientation.IsSet())
			{
				NewOrientation = PickAllowedDeviceOrientation(EScreenOrientation::LandscapeRight);
			}
			if (!NewOrientation.IsSet())
			{
				NewOrientation = PickAllowedDeviceOrientation(EScreenOrientation::LandscapeLeft);
			}
		}
		else
		{
			if (!NewOrientation.IsSet())
			{
				NewOrientation = PickAllowedDeviceOrientation(EScreenOrientation::LandscapeLeft);
			}
			if (!NewOrientation.IsSet())
			{
				NewOrientation = PickAllowedDeviceOrientation(EScreenOrientation::LandscapeRight);
			}
		}
		
		check(NewOrientation.IsSet());
	}
	
	if (NewOrientation.IsSet() && DeviceOrientation != NewOrientation.GetValue())
	{
		DeviceOrientation = NewOrientation.GetValue();
		CalcTrackingToWorldRotation();
	}
}

static EScreenOrientation::Type GetAppOrientation()
{
#if PLATFORM_IOS && !PLATFORM_TVOS
	// We want the orientation that the app is running with, not necessarily the orientation of the device right now.
	UIInterfaceOrientation Orientation = [[UIApplication sharedApplication] statusBarOrientation];
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_8_0
	Orientation = [[IOSAppDelegate GetDelegate].IOSController interfaceOrientation];
#endif
	EScreenOrientation::Type ScreenOrientation = EScreenOrientation::Unknown;
	switch (Orientation)
	{
	case UIInterfaceOrientationUnknown:				return EScreenOrientation::Unknown;
	case UIInterfaceOrientationPortrait:			return EScreenOrientation::Portrait;
	case UIInterfaceOrientationPortraitUpsideDown:	return EScreenOrientation::PortraitUpsideDown;
	case UIInterfaceOrientationLandscapeLeft:		return EScreenOrientation::LandscapeRight;
	case UIInterfaceOrientationLandscapeRight:		return EScreenOrientation::LandscapeLeft;
	default:										check(false); return EScreenOrientation::Unknown;
	}
#else
	return static_cast<EScreenOrientation::Type>(FPlatformMisc::GetDeviceOrientation());
#endif
}

PRAGMA_DISABLE_OPTIMIZATION
bool FAppleARKitSystem::Run(UARSessionConfig* SessionConfig)
{
	TimecodeProvider = UAppleARKitSettings::GetTimecodeProvider();

	{
		// Clear out any existing frames since they aren't valid anymore
		FScopeLock ScopeLock(&FrameLock);
		GameThreadFrame = TSharedPtr<FAppleARKitFrame, ESPMode::ThreadSafe>();
		LastReceivedFrame = TSharedPtr<FAppleARKitFrame, ESPMode::ThreadSafe>();
	}

	// Make sure this is set at session start, because there are timing issues with using only the delegate approach
	if (DeviceOrientation == EScreenOrientation::Unknown)
	{
		const EScreenOrientation::Type ScreenOrientation = GetAppOrientation();
		SetDeviceOrientation( ScreenOrientation );
	}

#if SUPPORTS_ARKIT_1_0
	// Set this based upon the project settings
	bShouldWriteCameraImagePerFrame = GetDefault<UAppleARKitSettings>()->bShouldWriteCameraImagePerFrame;
	WrittenCameraImageScale = GetDefault<UAppleARKitSettings>()->WrittenCameraImageScale;
	WrittenCameraImageRotation = GetDefault<UAppleARKitSettings>()->WrittenCameraImageRotation;
	WrittenCameraImageQuality = GetDefault<UAppleARKitSettings>()->WrittenCameraImageQuality;

	if (FAppleARKitAvailability::SupportsARKit10())
	{
		ARSessionRunOptions options = 0;

		ARConfiguration* Configuration = nullptr;
		CheckForFaceARSupport(SessionConfig);
		if (FaceARSupport == nullptr)
		{
			Configuration = FAppleARKitConversion::ToARConfiguration(SessionConfig, CandidateImages, ConvertedCandidateImages, CandidateObjects);
		}
		else
		{
			Configuration = FaceARSupport->ToARConfiguration(SessionConfig, TimecodeProvider);
		}

		// Not all session types are supported by all devices
		if (Configuration == nullptr)
		{
			UE_LOG(LogAppleARKit, Log, TEXT("The requested session type is not supported by this device"));
			return false;
		}

		// Create our ARSessionDelegate
		if (Delegate == nullptr)
		{
			Delegate = [[FAppleARKitSessionDelegate alloc] initWithAppleARKitSystem:this];
		}

		if (Session == nullptr)
		{
			// Start a new ARSession
			Session = [ARSession new];
			Session.delegate = Delegate;
			Session.delegateQueue = dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0);
		}
		else
		{
			// Check what the user has set for reseting options
			if (SessionConfig->ShouldResetCameraTracking())
			{
				options |= ARSessionRunOptionResetTracking;
			}
			if (SessionConfig->ShouldResetTrackedObjects())
			{
				options |= ARSessionRunOptionRemoveExistingAnchors;
			}

			[Session pause];
		}

		// Create MetalTextureCache
		if (IsMetalPlatform(GMaxRHIShaderPlatform))
		{
			id<MTLDevice> Device = (id<MTLDevice>)GDynamicRHI->RHIGetNativeDevice();
			check(Device);

			CVReturn Return = CVMetalTextureCacheCreate(nullptr, nullptr, Device, nullptr, &MetalTextureCache);
			check(Return == kCVReturnSuccess);
			check(MetalTextureCache);

			// Pass to session delegate to use for Metal texture creation
			[Delegate setMetalTextureCache : MetalTextureCache];
		}
		
#if PLATFORM_IOS && !PLATFORM_TVOS
		// Check if we need to adjust the priorities to allow ARKit to have more CPU time
		if (GetDefault<UAppleARKitSettings>()->bAdjustThreadPrioritiesDuringARSession)
		{
			int32 GameOverride = GetDefault<UAppleARKitSettings>()->GameThreadPriorityOverride;
			int32 RenderOverride = GetDefault<UAppleARKitSettings>()->RenderThreadPriorityOverride;
			SetThreadPriority(GameOverride);
			if (XRCamera.IsValid())
			{
				FAppleARKitXRCamera* Camera = (FAppleARKitXRCamera*)XRCamera.Get();
				Camera->AdjustThreadPriority(RenderOverride);
			}
			
			UE_LOG(LogAppleARKit, Log, TEXT("Overriding thread priorities: Game Thread (%d), Render Thread (%d)"), GameOverride, RenderOverride);
		}
#endif

		UE_LOG(LogAppleARKit, Log, TEXT("Starting session: %p with options %d"), this, options);

		// Start the session with the configuration
		[Session runWithConfiguration : Configuration options : options];
	}
	
#endif
	
	// @todo arkit Add support for relocating ARKit space to Unreal World Origin? BaseTransform = FTransform::Identity;
	
	// Set running state
	bIsRunning = true;
	
	GetARCompositionComponent()->OnARSessionStarted.Broadcast();
	return true;
}
PRAGMA_ENABLE_OPTIMIZATION

bool FAppleARKitSystem::IsRunning() const
{
	return bIsRunning;
}

bool FAppleARKitSystem::Pause()
{
	// Already stopped?
	if (!IsRunning())
	{
		return true;
	}
	
	UE_LOG(LogAppleARKit, Log, TEXT("Stopping session: %p"), this);

#if SUPPORTS_ARKIT_1_0
	if (FAppleARKitAvailability::SupportsARKit10())
	{
		// Suspend the session
		[Session pause];
	
		// Release MetalTextureCache created in Start
		if (MetalTextureCache)
		{
			// Tell delegate to release it
			[Delegate setMetalTextureCache:nullptr];
		
			CFRelease(MetalTextureCache);
			MetalTextureCache = nullptr;
		}
	}
	
#if PLATFORM_IOS && !PLATFORM_TVOS
	// Check if we need to adjust the priorities to allow ARKit to have more CPU time
	if (GetDefault<UAppleARKitSettings>()->bAdjustThreadPrioritiesDuringARSession)
	{
		SetThreadPriority(GAME_THREAD_PRIORITY);
		if (XRCamera.IsValid())
		{
			FAppleARKitXRCamera* Camera = (FAppleARKitXRCamera*)XRCamera.Get();
			Camera->AdjustThreadPriority(RENDER_THREAD_PRIORITY);
		}
		
		UE_LOG(LogAppleARKit, Log, TEXT("Restoring thread priorities: Game Thread (%d), Render Thread (%d)"), GAME_THREAD_PRIORITY, RENDER_THREAD_PRIORITY);
}
#endif
	
#endif
	
	// Set running state
	bIsRunning = false;
	
	return true;
}

void FAppleARKitSystem::OrientationChanged(const int32 NewOrientationRaw)
{
	const EScreenOrientation::Type NewOrientation = static_cast<EScreenOrientation::Type>(NewOrientationRaw);
	SetDeviceOrientation(NewOrientation);
}
						
void FAppleARKitSystem::SessionDidUpdateFrame_DelegateThread(TSharedPtr< FAppleARKitFrame, ESPMode::ThreadSafe > Frame)
{
	{
		auto UpdateFrameTask = FSimpleDelegateGraphTask::FDelegate::CreateThreadSafeSP( this, &FAppleARKitSystem::SessionDidUpdateFrame_Internal, Frame.ToSharedRef() );
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(UpdateFrameTask, GET_STATID(STAT_FAppleARKitSystem_SessionUpdateFrame), nullptr, ENamedThreads::GameThread);
	}
	{
		UpdateARKitPerfStats();
#if SUPPORTS_ARKIT_1_0
		if (bShouldWriteCameraImagePerFrame)
		{
			WriteCameraImageToDisk(Frame->CameraImage);
		}
#endif
	}
}
			
void FAppleARKitSystem::SessionDidFailWithError_DelegateThread(const FString& Error)
{
	UE_LOG(LogAppleARKit, Warning, TEXT("Session failed with error: %s"), *Error);
}

#if SUPPORTS_ARKIT_1_0

TArray<int32> FAppleARKitAnchorData::FaceIndices;

static TSharedPtr<FAppleARKitAnchorData> MakeAnchorData( ARAnchor* Anchor, double Timestamp, uint32 FrameNumber )
{
	TSharedPtr<FAppleARKitAnchorData> NewAnchor;
	if ([Anchor isKindOfClass:[ARPlaneAnchor class]])
	{
		ARPlaneAnchor* PlaneAnchor = (ARPlaneAnchor*)Anchor;
		NewAnchor = MakeShared<FAppleARKitAnchorData>(
			FAppleARKitConversion::ToFGuid(PlaneAnchor.identifier),
			FAppleARKitConversion::ToFTransform(PlaneAnchor.transform),
			FAppleARKitConversion::ToFVector(PlaneAnchor.center),
			// @todo use World Settings WorldToMetersScale
			0.5f*FAppleARKitConversion::ToFVector(PlaneAnchor.extent).GetAbs()
		);

#if SUPPORTS_ARKIT_1_5
		if (FAppleARKitAvailability::SupportsARKit15())
		{
			//@todo All this copying should really happen on-demand.
			const int32 NumBoundaryVerts = PlaneAnchor.geometry.boundaryVertexCount;
			NewAnchor->BoundaryVerts.Reset(NumBoundaryVerts);
			for(int32 i=0; i<NumBoundaryVerts; ++i)
			{
				const vector_float3& Vert = PlaneAnchor.geometry.boundaryVertices[i];
				NewAnchor->BoundaryVerts.Add(FAppleARKitConversion::ToFVector(Vert));
			}
		}
#endif
	}
#if SUPPORTS_ARKIT_1_5
	else if (FAppleARKitAvailability::SupportsARKit15() && [Anchor isKindOfClass:[ARImageAnchor class]])
	{
		ARImageAnchor* ImageAnchor = (ARImageAnchor*)Anchor;
		NewAnchor = MakeShared<FAppleARKitAnchorData>(
			FAppleARKitConversion::ToFGuid(ImageAnchor.identifier),
			FAppleARKitConversion::ToFTransform(ImageAnchor.transform),
			EAppleAnchorType::ImageAnchor,
			FString(ImageAnchor.referenceImage.name)
		);
#if SUPPORTS_ARKIT_2_0
		if (FAppleARKitAvailability::SupportsARKit20())
		{
			NewAnchor->bIsTracked = ImageAnchor.isTracked;
		}
#endif
	}
#endif
#if SUPPORTS_ARKIT_2_0
	else if (FAppleARKitAvailability::SupportsARKit20() && [Anchor isKindOfClass:[AREnvironmentProbeAnchor class]])
	{
		AREnvironmentProbeAnchor* ProbeAnchor = (AREnvironmentProbeAnchor*)Anchor;
		NewAnchor = MakeShared<FAppleARKitAnchorData>(
			FAppleARKitConversion::ToFGuid(ProbeAnchor.identifier),
			FAppleARKitConversion::ToFTransform(ProbeAnchor.transform),
			0.5f * FAppleARKitConversion::ToFVector(ProbeAnchor.extent).GetAbs(),
			ProbeAnchor.environmentTexture
		);
	}
	else if (FAppleARKitAvailability::SupportsARKit20() && [Anchor isKindOfClass:[ARObjectAnchor class]])
	{
		ARObjectAnchor* ObjectAnchor = (ARObjectAnchor*)Anchor;
		NewAnchor = MakeShared<FAppleARKitAnchorData>(
			  FAppleARKitConversion::ToFGuid(ObjectAnchor.identifier),
			  FAppleARKitConversion::ToFTransform(ObjectAnchor.transform),
			  EAppleAnchorType::ObjectAnchor,
			  FString(ObjectAnchor.referenceObject.name)
		  );
	}
#endif
	else
	{
		NewAnchor = MakeShared<FAppleARKitAnchorData>(
			FAppleARKitConversion::ToFGuid(Anchor.identifier),
			FAppleARKitConversion::ToFTransform(Anchor.transform));
	}

	NewAnchor->Timestamp = Timestamp;
	NewAnchor->FrameNumber = FrameNumber;
	
	return NewAnchor;
}

void FAppleARKitSystem::SessionDidAddAnchors_DelegateThread( NSArray<ARAnchor*>* anchors )
{
	// If this object is valid, we are running a face session and need that code to process things
	if (FaceARSupport != nullptr)
	{
		const FRotator& AdjustBy = GetARCompositionComponent()->GetSessionConfig().GetWorldAlignment() == EARWorldAlignment::Camera ? DerivedTrackingToUnrealRotation : FRotator::ZeroRotator;
		const EARFaceTrackingUpdate UpdateSetting = GetARCompositionComponent()->GetSessionConfig().GetFaceTrackingUpdate();

		const TArray<TSharedPtr<FAppleARKitAnchorData>> AnchorList = FaceARSupport->MakeAnchorData(anchors, AdjustBy, UpdateSetting);
		for (TSharedPtr<FAppleARKitAnchorData> NewAnchorData : AnchorList)
		{
			auto AddAnchorTask = FSimpleDelegateGraphTask::FDelegate::CreateSP(this, &FAppleARKitSystem::SessionDidAddAnchors_Internal, NewAnchorData.ToSharedRef());
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(AddAnchorTask, GET_STATID(STAT_FAppleARKitSystem_SessionDidAddAnchors), nullptr, ENamedThreads::GameThread);
		}
		return;
	}

	// Make sure all anchors get the same timestamp and frame number
	double Timestamp = FPlatformTime::Seconds();
	uint32 FrameNumber = TimecodeProvider->GetTimecode().Frames;

	for (ARAnchor* anchor in anchors)
	{
		TSharedPtr<FAppleARKitAnchorData> NewAnchorData = MakeAnchorData(anchor, Timestamp, FrameNumber);
		if (ensure(NewAnchorData.IsValid()))
		{
			auto AddAnchorTask = FSimpleDelegateGraphTask::FDelegate::CreateSP(this, &FAppleARKitSystem::SessionDidAddAnchors_Internal, NewAnchorData.ToSharedRef());
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(AddAnchorTask, GET_STATID(STAT_FAppleARKitSystem_SessionDidAddAnchors), nullptr, ENamedThreads::GameThread);
		}
	}
}

void FAppleARKitSystem::SessionDidUpdateAnchors_DelegateThread( NSArray<ARAnchor*>* anchors )
{
	// If this object is valid, we are running a face session and need that code to process things
	if (FaceARSupport != nullptr)
	{
		double UpdateTimestamp = FPlatformTime::Seconds();
		const FRotator& AdjustBy = GetARCompositionComponent()->GetSessionConfig().GetWorldAlignment() == EARWorldAlignment::Camera ? DerivedTrackingToUnrealRotation : FRotator::ZeroRotator;
		const EARFaceTrackingUpdate UpdateSetting = GetARCompositionComponent()->GetSessionConfig().GetFaceTrackingUpdate();

		const TArray<TSharedPtr<FAppleARKitAnchorData>> AnchorList = FaceARSupport->MakeAnchorData(anchors, AdjustBy, UpdateSetting);
		for (TSharedPtr<FAppleARKitAnchorData> NewAnchorData : AnchorList)
		{
			auto UpdateAnchorTask = FSimpleDelegateGraphTask::FDelegate::CreateSP(this, &FAppleARKitSystem::SessionDidUpdateAnchors_Internal, NewAnchorData.ToSharedRef());
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(UpdateAnchorTask, GET_STATID(STAT_FAppleARKitSystem_SessionDidUpdateAnchors), nullptr, ENamedThreads::GameThread);
		}
		return;
	}

	// Make sure all anchors get the same timestamp and frame number
	double Timestamp = FPlatformTime::Seconds();
	uint32 FrameNumber = TimecodeProvider->GetTimecode().Frames;

	for (ARAnchor* anchor in anchors)
	{
		TSharedPtr<FAppleARKitAnchorData> NewAnchorData = MakeAnchorData(anchor, Timestamp, FrameNumber);
		if (ensure(NewAnchorData.IsValid()))
		{
			auto UpdateAnchorTask = FSimpleDelegateGraphTask::FDelegate::CreateSP(this, &FAppleARKitSystem::SessionDidUpdateAnchors_Internal, NewAnchorData.ToSharedRef());
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(UpdateAnchorTask, GET_STATID(STAT_FAppleARKitSystem_SessionDidUpdateAnchors), nullptr, ENamedThreads::GameThread);
		}
	}
}

void FAppleARKitSystem::SessionDidRemoveAnchors_DelegateThread( NSArray<ARAnchor*>* anchors )
{
	// Face AR Anchors are also removed this way, no need for special code since they are tracked geometry
	for (ARAnchor* anchor in anchors)
	{
		// Convert to FGuid
		const FGuid AnchorGuid = FAppleARKitConversion::ToFGuid( anchor.identifier );

		auto RemoveAnchorTask = FSimpleDelegateGraphTask::FDelegate::CreateSP(this, &FAppleARKitSystem::SessionDidRemoveAnchors_Internal, AnchorGuid);
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(RemoveAnchorTask, GET_STATID(STAT_FAppleARKitSystem_SessionDidRemoveAnchors), nullptr, ENamedThreads::GameThread);
	}
}


void FAppleARKitSystem::SessionDidAddAnchors_Internal( TSharedRef<FAppleARKitAnchorData> AnchorData )
{
	double UpdateTimestamp = FPlatformTime::Seconds();
	
	const TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe>& ARComponent = GetARCompositionComponent();

	// In case we have camera tracking turned off, we still need to update the frame
	if (!ARComponent->GetSessionConfig().ShouldEnableCameraTracking())
	{
		UpdateFrame();
	}
	
	// If this object is valid, we are running a face session and we need to publish LiveLink data on the game thread
	if (FaceARSupport != nullptr && AnchorData->AnchorType == EAppleAnchorType::FaceAnchor)
	{
		FaceARSupport->PublishLiveLinkData(AnchorData);
	}

	FString NewAnchorDebugName;
	UARTrackedGeometry* NewGeometry = nullptr;
	switch (AnchorData->AnchorType)
	{
		case EAppleAnchorType::Anchor:
		{
			NewAnchorDebugName = FString::Printf(TEXT("ANCHOR-%02d"), LastTrackedGeometry_DebugId++);
			NewGeometry = NewObject<UARTrackedGeometry>();
			NewGeometry->UpdateTrackedGeometry(ARComponent.ToSharedRef(), AnchorData->FrameNumber, AnchorData->Timestamp, AnchorData->Transform, GetARCompositionComponent()->GetAlignmentTransform());
			break;
		}
		case EAppleAnchorType::PlaneAnchor:
		{
			NewAnchorDebugName = FString::Printf(TEXT("PLN-%02d"), LastTrackedGeometry_DebugId++);
			UARPlaneGeometry* NewGeo = NewObject<UARPlaneGeometry>();
			NewGeo->UpdateTrackedGeometry(ARComponent.ToSharedRef(), AnchorData->FrameNumber, AnchorData->Timestamp, AnchorData->Transform, GetARCompositionComponent()->GetAlignmentTransform(), AnchorData->Center, AnchorData->Extent);
			NewGeometry = NewGeo;
			break;
		}
		case EAppleAnchorType::FaceAnchor:
		{
			NewAnchorDebugName = FString::Printf(TEXT("FACE-%02d"), LastTrackedGeometry_DebugId++);
			UARFaceGeometry* NewGeo = NewObject<UARFaceGeometry>();
			NewGeo->UpdateFaceGeometry(ARComponent.ToSharedRef(), AnchorData->FrameNumber, AnchorData->Timestamp, AnchorData->Transform, GetARCompositionComponent()->GetAlignmentTransform(), AnchorData->BlendShapes, AnchorData->FaceVerts, AnchorData->FaceIndices, AnchorData->LeftEyeTransform, AnchorData->RightEyeTransform, AnchorData->LookAtTarget);
			NewGeo->SetTrackingState(EARTrackingState::Tracking);
			// @todo JoeG -- remove in 4.22
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			NewGeo->bIsTracked = true;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			NewGeometry = NewGeo;
			break;
		}
		case EAppleAnchorType::ImageAnchor:
		{
			NewAnchorDebugName = FString::Printf(TEXT("IMG-%02d"), LastTrackedGeometry_DebugId++);
			UARTrackedImage* NewImage = NewObject<UARTrackedImage>();
			UARCandidateImage** CandidateImage = CandidateImages.Find(AnchorData->DetectedAnchorName);
			ensure(CandidateImage != nullptr);
			FVector2D PhysicalSize((*CandidateImage)->GetPhysicalWidth(), (*CandidateImage)->GetPhysicalHeight());
			NewImage->UpdateTrackedGeometry(ARComponent.ToSharedRef(), AnchorData->FrameNumber, AnchorData->Timestamp, AnchorData->Transform, GetARCompositionComponent()->GetAlignmentTransform(), PhysicalSize, *CandidateImage);
			NewGeometry = NewImage;
			break;
		}
		case EAppleAnchorType::EnvironmentProbeAnchor:
		{
			NewAnchorDebugName = FString::Printf(TEXT("ENV-%02d"), LastTrackedGeometry_DebugId++);
			UAppleARKitEnvironmentCaptureProbe* NewProbe = NewObject<UAppleARKitEnvironmentCaptureProbe>();
			NewProbe->UpdateEnvironmentCapture(ARComponent.ToSharedRef(), AnchorData->FrameNumber, AnchorData->Timestamp, AnchorData->Transform, GetARCompositionComponent()->GetAlignmentTransform(), AnchorData->Extent, AnchorData->ProbeTexture);
			NewGeometry = NewProbe;
			break;
		}
		case EAppleAnchorType::ObjectAnchor:
		{
			NewAnchorDebugName = FString::Printf(TEXT("OBJ-%02d"), LastTrackedGeometry_DebugId++);
			UARTrackedObject* NewTrackedObject = NewObject<UARTrackedObject>();
			UARCandidateObject** CandidateObject = CandidateObjects.Find(AnchorData->DetectedAnchorName);
			ensure(CandidateObject != nullptr);
			NewTrackedObject->UpdateTrackedGeometry(ARComponent.ToSharedRef(), AnchorData->FrameNumber, AnchorData->Timestamp, AnchorData->Transform, GetARCompositionComponent()->GetAlignmentTransform(), *CandidateObject);
			NewGeometry = NewTrackedObject;
			break;
		}
	}
	check(NewGeometry != nullptr);

	UARTrackedGeometry* NewTrackedGeometry = TrackedGeometries.Add( AnchorData->AnchorGUID, NewGeometry );
	
	NewTrackedGeometry->SetDebugName( FName(*NewAnchorDebugName) );
}

void FAppleARKitSystem::SessionDidUpdateAnchors_Internal( TSharedRef<FAppleARKitAnchorData> AnchorData )
{
	double UpdateTimestamp = FPlatformTime::Seconds();
	
	const TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe>& ARComponent = GetARCompositionComponent();

	// In case we have camera tracking turned off, we still need to update the frame
	if (!ARComponent->GetSessionConfig().ShouldEnableCameraTracking())
	{
		UpdateFrame();
	}

	// If this object is valid, we are running a face session and we need to publish LiveLink data on the game thread
	if (FaceARSupport != nullptr && AnchorData->AnchorType == EAppleAnchorType::FaceAnchor)
	{
		FaceARSupport->PublishLiveLinkData(AnchorData);
	}

	UARTrackedGeometry** GeometrySearchResult = TrackedGeometries.Find(AnchorData->AnchorGUID);
	if (ensure(GeometrySearchResult != nullptr))
	{
		UARTrackedGeometry* FoundGeometry = *GeometrySearchResult;
		TArray<UARPin*> PinsToUpdate = ARKitUtil::PinsFromGeometry(FoundGeometry, Pins);
		
		
		// We figure out the delta transform for the Anchor (aka. TrackedGeometry in ARKit) and apply that
		// delta to figure out the new ARPin transform.
		const FTransform Anchor_LocalToTrackingTransform_PreUpdate = FoundGeometry->GetLocalToTrackingTransform_NoAlignment();
		const FTransform& Anchor_LocalToTrackingTransform_PostUpdate = AnchorData->Transform;
		
		const FTransform AnchorDeltaTransform = Anchor_LocalToTrackingTransform_PreUpdate.GetRelativeTransform(Anchor_LocalToTrackingTransform_PostUpdate);
		
		switch (AnchorData->AnchorType)
		{
			case EAppleAnchorType::Anchor:
			{
				FoundGeometry->UpdateTrackedGeometry(ARComponent.ToSharedRef(), AnchorData->FrameNumber, AnchorData->Timestamp, AnchorData->Transform, GetARCompositionComponent()->GetAlignmentTransform());
				for (UARPin* Pin : PinsToUpdate)
				{
					const FTransform Pin_LocalToTrackingTransform_PostUpdate = Pin->GetLocalToTrackingTransform_NoAlignment() * AnchorDeltaTransform;
					Pin->OnTransformUpdated(Pin_LocalToTrackingTransform_PostUpdate);
				}
				
				break;
			}
			case EAppleAnchorType::PlaneAnchor:
			{
				if (UARPlaneGeometry* PlaneGeo = Cast<UARPlaneGeometry>(FoundGeometry))
				{
					PlaneGeo->UpdateTrackedGeometry(ARComponent.ToSharedRef(), AnchorData->FrameNumber, AnchorData->Timestamp, AnchorData->Transform, GetARCompositionComponent()->GetAlignmentTransform(), AnchorData->Center, AnchorData->Extent, AnchorData->BoundaryVerts, nullptr);
					for (UARPin* Pin : PinsToUpdate)
					{
						const FTransform Pin_LocalToTrackingTransform_PostUpdate = Pin->GetLocalToTrackingTransform_NoAlignment() * AnchorDeltaTransform;
						Pin->OnTransformUpdated(Pin_LocalToTrackingTransform_PostUpdate);
					}
				}
				break;
			}
			case EAppleAnchorType::FaceAnchor:
			{
				if (UARFaceGeometry* FaceGeo = Cast<UARFaceGeometry>(FoundGeometry))
				{
					FaceGeo->UpdateFaceGeometry(ARComponent.ToSharedRef(), AnchorData->FrameNumber, AnchorData->Timestamp, AnchorData->Transform, GetARCompositionComponent()->GetAlignmentTransform(), AnchorData->BlendShapes, AnchorData->FaceVerts, AnchorData->FaceIndices, AnchorData->LeftEyeTransform, AnchorData->RightEyeTransform, AnchorData->LookAtTarget);
					FaceGeo->SetTrackingState(AnchorData->bIsTracked ? EARTrackingState::Tracking : EARTrackingState::NotTracking);
					// @todo JoeG -- remove this in 4.22
PRAGMA_DISABLE_DEPRECATION_WARNINGS
					FaceGeo->bIsTracked = AnchorData->bIsTracked;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
					for (UARPin* Pin : PinsToUpdate)
					{
						const FTransform Pin_LocalToTrackingTransform_PostUpdate = Pin->GetLocalToTrackingTransform_NoAlignment() * AnchorDeltaTransform;
						Pin->OnTransformUpdated(Pin_LocalToTrackingTransform_PostUpdate);
					}
				}
				break;
			}
            case EAppleAnchorType::ImageAnchor:
            {
				if (UARTrackedImage* ImageAnchor = Cast<UARTrackedImage>(FoundGeometry))
				{
					UARCandidateImage** CandidateImage = CandidateImages.Find(AnchorData->DetectedAnchorName);
					ensure(CandidateImage != nullptr);
					FVector2D PhysicalSize((*CandidateImage)->GetPhysicalWidth(), (*CandidateImage)->GetPhysicalHeight());
					ImageAnchor->UpdateTrackedGeometry(ARComponent.ToSharedRef(), AnchorData->FrameNumber, AnchorData->Timestamp, AnchorData->Transform, GetARCompositionComponent()->GetAlignmentTransform(), PhysicalSize, *CandidateImage);
					ImageAnchor->SetTrackingState(AnchorData->bIsTracked ? EARTrackingState::Tracking : EARTrackingState::NotTracking);
					// @todo JoeG -- remove this in 4.22
PRAGMA_DISABLE_DEPRECATION_WARNINGS
					ImageAnchor->bIsTracked = AnchorData->bIsTracked;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
					for (UARPin* Pin : PinsToUpdate)
					{
						const FTransform Pin_LocalToTrackingTransform_PostUpdate = Pin->GetLocalToTrackingTransform_NoAlignment() * AnchorDeltaTransform;
						Pin->OnTransformUpdated(Pin_LocalToTrackingTransform_PostUpdate);
					}
				}
                break;
            }
			case EAppleAnchorType::EnvironmentProbeAnchor:
			{
				if (UAppleARKitEnvironmentCaptureProbe* ProbeAnchor = Cast<UAppleARKitEnvironmentCaptureProbe>(FoundGeometry))
				{
					// NOTE: The metal texture will be a different texture every time the cubemap is updated which requires a render resource flush
					ProbeAnchor->UpdateEnvironmentCapture(ARComponent.ToSharedRef(), AnchorData->FrameNumber, AnchorData->Timestamp, AnchorData->Transform, GetARCompositionComponent()->GetAlignmentTransform(), AnchorData->Extent, AnchorData->ProbeTexture);
					for (UARPin* Pin : PinsToUpdate)
					{
						const FTransform Pin_LocalToTrackingTransform_PostUpdate = Pin->GetLocalToTrackingTransform_NoAlignment() * AnchorDeltaTransform;
						Pin->OnTransformUpdated(Pin_LocalToTrackingTransform_PostUpdate);
					}
				}
				break;
			}
		}
	}
}

void FAppleARKitSystem::SessionDidRemoveAnchors_Internal( FGuid AnchorGuid )
{
	const TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe>& ARComponent = GetARCompositionComponent();

	// In case we have camera tracking turned off, we still need to update the frame
	if (!ARComponent->GetSessionConfig().ShouldEnableCameraTracking())
	{
		UpdateFrame();
	}

	// Notify pin that it is being orphaned
	{
		UARTrackedGeometry* TrackedGeometryBeingRemoved = TrackedGeometries.FindChecked(AnchorGuid);
		TrackedGeometryBeingRemoved->UpdateTrackingState(EARTrackingState::StoppedTracking);
		
		TArray<UARPin*> ARPinsBeingOrphaned = ARKitUtil::PinsFromGeometry(TrackedGeometryBeingRemoved, Pins);
		for(UARPin* PinBeingOrphaned : ARPinsBeingOrphaned)
		{
			PinBeingOrphaned->OnTrackingStateChanged(EARTrackingState::StoppedTracking);
		}
	}
	
	TrackedGeometries.Remove(AnchorGuid);
}

#endif

void FAppleARKitSystem::SessionDidUpdateFrame_Internal( TSharedRef< FAppleARKitFrame, ESPMode::ThreadSafe > Frame )
{
	LastReceivedFrame = Frame;
	UpdateFrame();
}

#if STATS
struct FARKitThreadTimes
{
	TArray<FString> ThreadNames;
	int32 LastTotal;
	int32 NewTotal;
	
	FARKitThreadTimes() :
		LastTotal(0)
		, NewTotal(0)
	{
		ThreadNames.Add(TEXT("com.apple.CoreMotion"));
		ThreadNames.Add(TEXT("com.apple.arkit"));
		ThreadNames.Add(TEXT("FilteringFrameDownsampleNodeWorkQueue"));
		ThreadNames.Add(TEXT("FeatureDetectorNodeWorkQueue"));
		ThreadNames.Add(TEXT("SurfaceDetectionNode"));
		ThreadNames.Add(TEXT("VIOEngineNode"));
		ThreadNames.Add(TEXT("ImageDetectionQueue"));
	}

	bool IsARKitThread(const FString& Name)
	{
		if (Name.Len() == 0)
		{
			return false;
		}
		
		for (int32 Index = 0; Index < ThreadNames.Num(); Index++)
		{
			if (Name.StartsWith(ThreadNames[Index]))
			{
				return true;
			}
		}
		return false;
	}
	
	void FrameReset()
	{
		LastTotal = NewTotal;
		NewTotal = 0;
	}
};
#endif

void FAppleARKitSystem::UpdateARKitPerfStats()
{
#if STATS && SUPPORTS_ARKIT_1_0
	static FARKitThreadTimes ARKitThreadTimes;

	SCOPE_CYCLE_COUNTER(STAT_FAppleARKitSystem_UpdateARKitPerf);
	ARKitThreadTimes.FrameReset();
	
	thread_array_t ThreadArray;
	mach_msg_type_number_t ThreadCount;
	if (task_threads(mach_task_self(), &ThreadArray, &ThreadCount) != KERN_SUCCESS)
	{
		return;
	}

	for (int32 Index = 0; Index < (int32)ThreadCount; Index++)
	{
		mach_msg_type_number_t ThreadInfoCount = THREAD_BASIC_INFO_COUNT;
		mach_msg_type_number_t ExtThreadInfoCount = THREAD_EXTENDED_INFO_COUNT;
		thread_info_data_t ThreadInfo;
		thread_extended_info_data_t ExtThreadInfo;
		// Get the basic thread info for this thread
		if (thread_info(ThreadArray[Index], THREAD_BASIC_INFO, (thread_info_t)ThreadInfo, &ThreadInfoCount) != KERN_SUCCESS)
		{
			continue;
		}
		// And the extended thread info for this thread
		if (thread_info(ThreadArray[Index], THREAD_EXTENDED_INFO, (thread_info_t)&ExtThreadInfo, &ExtThreadInfoCount) != KERN_SUCCESS)
		{
			continue;
		}
		thread_basic_info_t BasicInfo = (thread_basic_info_t)ThreadInfo;
		FString ThreadName(ExtThreadInfo.pth_name);
		if (ARKitThreadTimes.IsARKitThread(ThreadName))
		{
			// CPU usage is reported as a scaled number, so convert to %
			int32 ScaledPercent = FMath::RoundToInt((float)BasicInfo->cpu_usage / (float)TH_USAGE_SCALE * 100.f);
			ARKitThreadTimes.NewTotal += ScaledPercent;
		}
//		UE_LOG(LogAppleARKit, Log, TEXT("Thread %s used cpu (%d), seconds (%d), microseconds (%d)"), *ThreadName, BasicInfo->cpu_usage, BasicInfo->user_time.seconds + BasicInfo->system_time.seconds, BasicInfo->user_time.microseconds + BasicInfo->system_time.microseconds);
	}
	vm_deallocate(mach_task_self(), (vm_offset_t)ThreadArray, ThreadCount * sizeof(thread_t));
	SET_DWORD_STAT(STAT_ARKitThreads, ARKitThreadTimes.NewTotal);
#endif
}

#if SUPPORTS_ARKIT_1_0
void FAppleARKitSystem::WriteCameraImageToDisk(CVPixelBufferRef PixelBuffer)
{
	int32 ImageQuality = WrittenCameraImageQuality;
	float ImageScale = WrittenCameraImageScale;
	ETextureRotationDirection ImageRotation = WrittenCameraImageRotation;
	CIImage* SourceImage = [[CIImage alloc] initWithCVPixelBuffer: PixelBuffer];
	FTimecode Timecode = TimecodeProvider->GetTimecode();
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [SourceImage, ImageQuality, ImageScale, ImageRotation, Timecode]()
	{
		TArray<uint8> JpegBytes;
		IAppleImageUtilsPlugin::Get().ConvertToJPEG(SourceImage, JpegBytes, ImageQuality, true, true, ImageScale, ImageRotation);
		[SourceImage release];
		// Build a unique file name
		FDateTime DateTime = FDateTime::UtcNow();
		static FString UserDir = FPlatformProcess::UserDir();
		FString FileName = FString::Printf(TEXT("%sCameraImages/Image_%d-%d-%d-%d-%d-%d-%d.jpeg"), *UserDir,
			DateTime.GetYear(), DateTime.GetMonth(), DateTime.GetDay(), Timecode.Hours, Timecode.Minutes, Timecode.Seconds, Timecode.Frames);
		// Write the jpeg to disk
		FFileHelper::SaveArrayToFile(JpegBytes, *FileName);
	});
}
#endif


namespace AppleARKitSupport
{
	TSharedPtr<class FAppleARKitSystem, ESPMode::ThreadSafe> CreateAppleARKitSystem()
	{
#if SUPPORTS_ARKIT_1_0
		// Handle older iOS devices somehow calling this
		if (FAppleARKitAvailability::SupportsARKit10())
		{
			auto NewARKitSystem = MakeShared<FAppleARKitSystem, ESPMode::ThreadSafe>();
            return NewARKitSystem;
		}
#endif
		return TSharedPtr<class FAppleARKitSystem, ESPMode::ThreadSafe>();
	}
}

UTimecodeProvider* UAppleARKitSettings::GetTimecodeProvider()
{
	const FString& ProviderName = GetDefault<UAppleARKitSettings>()->ARKitTimecodeProvider;
	UTimecodeProvider* TimecodeProvider = FindObject<UTimecodeProvider>(GEngine, *ProviderName);
	if (TimecodeProvider == nullptr)
	{
		// Try to load the class that was requested
		UClass* Class = LoadClass<UTimecodeProvider>(nullptr, *ProviderName);
		if (Class != nullptr)
		{
			TimecodeProvider = NewObject<UTimecodeProvider>(GEngine, Class);
		}
	}
	// Create the default one if this failed for some reason
	if (TimecodeProvider == nullptr)
	{
		TimecodeProvider = NewObject<UTimecodeProvider>(GEngine, UAppleARKitTimecodeProvider::StaticClass());
	}
	return TimecodeProvider;
}

#if PLATFORM_IOS
	#pragma clang diagnostic pop
#endif
