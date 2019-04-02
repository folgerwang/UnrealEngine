// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GoogleARCoreXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "RHIDefinitions.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "GoogleARCoreDevice.h"
#include "GoogleARCoreTypes.h"
#include "GoogleARCoreXRCamera.h"
#include "ARSessionConfig.h"

FGoogleARCoreXRTrackingSystem::FGoogleARCoreXRTrackingSystem()
	: FXRTrackingSystemBase(this)
	, ARCoreDeviceInstance(nullptr)
	, bMatchDeviceCameraFOV(false)
	, bEnablePassthroughCameraRendering(false)
	, bHasValidPose(false)
	, CachedPosition(FVector::ZeroVector)
	, CachedOrientation(FQuat::Identity)
	, DeltaControlRotation(FRotator::ZeroRotator)
	, DeltaControlOrientation(FQuat::Identity)
	, LightEstimate(nullptr)
	, EventManager(nullptr)
{
	UE_LOG(LogGoogleARCoreTrackingSystem, Log, TEXT("Creating GoogleARCore Tracking System."));
	ARCoreDeviceInstance = FGoogleARCoreDevice::GetInstance();
	check(ARCoreDeviceInstance);
}

FGoogleARCoreXRTrackingSystem::~FGoogleARCoreXRTrackingSystem()
{
}

/////////////////////////////////////////////////////////////////////////////////
// Begin FGoogleARCoreXRTrackingSystem IHeadMountedDisplay Virtual Interface   //
////////////////////////////////////////////////////////////////////////////////
FName FGoogleARCoreXRTrackingSystem::GetSystemName() const
{
	static FName DefaultName(TEXT("FGoogleARCoreXRTrackingSystem"));
	return DefaultName;
}

bool FGoogleARCoreXRTrackingSystem::IsHeadTrackingAllowed() const
{
#if PLATFORM_ANDROID
	return true;
#else
	return false;
#endif
}

bool FGoogleARCoreXRTrackingSystem::GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition)
{
	if (DeviceId == IXRTrackingSystem::HMDDeviceId)
	{
		OutOrientation = CachedOrientation;
		OutPosition = CachedPosition;
		return true;
	}
	else
	{
		return false;
	}
}

FString FGoogleARCoreXRTrackingSystem::GetVersionString() const
{
	FString s = FString::Printf(TEXT("ARCoreHMD - %s, built %s, %s"), *FEngineVersion::Current().ToString(),
		UTF8_TO_TCHAR(__DATE__), UTF8_TO_TCHAR(__TIME__));

	return s;
}

bool FGoogleARCoreXRTrackingSystem::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type /*= EXRTrackedDeviceType::Any*/)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		static const int32 DeviceId = IXRTrackingSystem::HMDDeviceId;
		OutDevices.Add(DeviceId);
		return true;
	}
	return false;
}

bool FGoogleARCoreXRTrackingSystem::OnStartGameFrame(FWorldContext& WorldContext)
{
	FTransform CurrentPose;
	bHasValidPose = false;
	if (ARCoreDeviceInstance->GetIsARCoreSessionRunning())
	{
		if (ARCoreDeviceInstance->GetTrackingState() == EGoogleARCoreTrackingState::Tracking)
		{
			CurrentPose = ARCoreDeviceInstance->GetLatestPose();
			CurrentPose *= GetARCompositionComponent()->GetAlignmentTransform();
			bHasValidPose = true;
			CachedTrackingToWorld = ComputeTrackingToWorldTransform(WorldContext);
		}

		if (bHasValidPose)
		{
			CachedOrientation = CurrentPose.GetRotation();
			CachedPosition = CurrentPose.GetTranslation();
		}

		if (LightEstimate == nullptr)
		{
			LightEstimate = NewObject<UARBasicLightEstimate>();
		}
		FGoogleARCoreLightEstimate ARCoreLightEstimate = FGoogleARCoreDevice::GetInstance()->GetLatestLightEstimate();
		if (ARCoreLightEstimate.bIsValid)
		{
			LightEstimate->SetLightEstimate(ARCoreLightEstimate.RGBScaleFactor, ARCoreLightEstimate.PixelIntensity);
		}
		else
		{
			LightEstimate = nullptr;
		}
	}

	return true;
}

void FGoogleARCoreXRTrackingSystem::ConfigARCoreXRCamera(bool bInMatchDeviceCameraFOV, bool bInEnablePassthroughCameraRendering)
{
	bMatchDeviceCameraFOV = bInMatchDeviceCameraFOV;
	bEnablePassthroughCameraRendering = bInEnablePassthroughCameraRendering;

	static_cast<FGoogleARCoreXRCamera*>(GetXRCamera().Get())->ConfigXRCamera(bEnablePassthroughCameraRendering, bEnablePassthroughCameraRendering);
}


void FGoogleARCoreXRTrackingSystem::EnableColorCameraRendering(bool bInEnablePassthroughCameraRendering)
{
	bEnablePassthroughCameraRendering = bInEnablePassthroughCameraRendering;
	static_cast<FGoogleARCoreXRCamera*>(GetXRCamera().Get())->ConfigXRCamera(bEnablePassthroughCameraRendering, bEnablePassthroughCameraRendering);
}

bool FGoogleARCoreXRTrackingSystem::GetColorCameraRenderingEnabled()
{
	return bEnablePassthroughCameraRendering;
}

float FGoogleARCoreXRTrackingSystem::GetWorldToMetersScale() const
{
	if (IsInGameThread() && GWorld != nullptr)
	{
		return GWorld->GetWorldSettings()->WorldToMeters;
	}

	// Default value, assume Unreal units are in centimeters
	return 100.0f;
}

void* FGoogleARCoreXRTrackingSystem::GetARSessionRawPointer()
{
#if PLATFORM_ANDROID
	return static_cast<void*>(FGoogleARCoreDevice::GetInstance()->GetARSessionRawPointer());
#endif
	ensureAlwaysMsgf(false, TEXT("FGoogleARCoreXRTrackingSystem::GetARSessionRawPointer is unimplemented on current platform."));
	return nullptr;
}

void* FGoogleARCoreXRTrackingSystem::GetGameThreadARFrameRawPointer()
{
#if PLATFORM_ANDROID
	return static_cast<void*>(FGoogleARCoreDevice::GetInstance()->GetGameThreadARFrameRawPointer());
#endif
	ensureAlwaysMsgf(false, TEXT("FGoogleARCoreXRTrackingSystem::GetARSessionRawPointer is unimplemented on current platform."));
	return nullptr;
}

UGoogleARCoreEventManager* FGoogleARCoreXRTrackingSystem::GetEventManager()
{
	if (EventManager == nullptr)
	{
		EventManager = NewObject<UGoogleARCoreEventManager>();
	}
	return EventManager;
}

void FGoogleARCoreXRTrackingSystem::OnARSystemInitialized()
{

}

EARTrackingQuality FGoogleARCoreXRTrackingSystem::OnGetTrackingQuality() const
{
	if (!bHasValidPose)
	{
		return EARTrackingQuality::NotTracking;
	}

	return EARTrackingQuality::OrientationAndPosition;
}

void FGoogleARCoreXRTrackingSystem::OnStartARSession(UARSessionConfig* SessionConfig)
{
	FGoogleARCoreDevice::GetInstance()->StartARCoreSessionRequest(SessionConfig);
}

void FGoogleARCoreXRTrackingSystem::OnPauseARSession()
{
	FGoogleARCoreDevice::GetInstance()->PauseARCoreSession();
}

void FGoogleARCoreXRTrackingSystem::OnStopARSession()
{
	FGoogleARCoreDevice::GetInstance()->PauseARCoreSession();
	FGoogleARCoreDevice::GetInstance()->ResetARCoreSession();
}

FARSessionStatus FGoogleARCoreXRTrackingSystem::OnGetARSessionStatus() const
{
	return FGoogleARCoreDevice::GetInstance()->GetSessionStatus();
}

void FGoogleARCoreXRTrackingSystem::OnSetAlignmentTransform(const FTransform& InAlignmentTransform)
{
	const FTransform& NewAlignmentTransform = InAlignmentTransform;

	TArray<UARTrackedGeometry*> AllTrackedGeometries = GetARCompositionComponent()->GetAllTrackedGeometries();
	for (UARTrackedGeometry* TrackedGeometry : AllTrackedGeometries)
	{
		TrackedGeometry->UpdateAlignmentTransform(NewAlignmentTransform);
	}

	TArray<UARPin*> AllARPins = GetARCompositionComponent()->GetAllPins();
	for (UARPin* SomePin : AllARPins)
	{
		SomePin->UpdateAlignmentTransform(NewAlignmentTransform);
	}
}

static EGoogleARCoreLineTraceChannel ConvertToGoogleARCoreTraceChannels(EARLineTraceChannels TraceChannels)
{
	EGoogleARCoreLineTraceChannel ARCoreTraceChannels = EGoogleARCoreLineTraceChannel::None;
	if (!!(TraceChannels & EARLineTraceChannels::FeaturePoint))
	{
		ARCoreTraceChannels = ARCoreTraceChannels | EGoogleARCoreLineTraceChannel::FeaturePoint;
	}
	if (!!(TraceChannels & EARLineTraceChannels::GroundPlane))
	{
		ARCoreTraceChannels = ARCoreTraceChannels | EGoogleARCoreLineTraceChannel::InfinitePlane;
	}
	if (!!(TraceChannels & EARLineTraceChannels::PlaneUsingBoundaryPolygon))
	{
		ARCoreTraceChannels = ARCoreTraceChannels | EGoogleARCoreLineTraceChannel::PlaneUsingBoundaryPolygon;
	}
	if (!!(TraceChannels & EARLineTraceChannels::PlaneUsingExtent))
	{
		ARCoreTraceChannels = ARCoreTraceChannels | EGoogleARCoreLineTraceChannel::PlaneUsingExtent;
	}
	return ARCoreTraceChannels;
}

TArray<FARTraceResult> FGoogleARCoreXRTrackingSystem::OnLineTraceTrackedObjects(const FVector2D ScreenCoord, EARLineTraceChannels TraceChannels)
{
	TArray<FARTraceResult> OutHitResults;
	FGoogleARCoreDevice::GetInstance()->ARLineTrace(ScreenCoord, ConvertToGoogleARCoreTraceChannels(TraceChannels), OutHitResults);
	return OutHitResults;
}

TArray<FARTraceResult> FGoogleARCoreXRTrackingSystem::OnLineTraceTrackedObjects(const FVector Start, const FVector End, EARLineTraceChannels TraceChannels)
{
	TArray<FARTraceResult> OutHitResults;
	FGoogleARCoreDevice::GetInstance()->ARLineTrace(Start, End, ConvertToGoogleARCoreTraceChannels(TraceChannels), OutHitResults);
	return OutHitResults;
}

TArray<UARTrackedGeometry*> FGoogleARCoreXRTrackingSystem::OnGetAllTrackedGeometries() const
{
	TArray<UARTrackedGeometry*> AllTrackedGeometry;
	FGoogleARCoreDevice::GetInstance()->GetAllTrackables<UARTrackedGeometry>(AllTrackedGeometry);
	return AllTrackedGeometry;
}

TArray<UARPin*> FGoogleARCoreXRTrackingSystem::OnGetAllPins() const
{
	TArray<UARPin*> AllARPins;
	FGoogleARCoreDevice::GetInstance()->GetAllARPins(AllARPins);
	return AllARPins;
}

bool FGoogleARCoreXRTrackingSystem::OnIsTrackingTypeSupported(EARSessionType SessionType) const
{
	return FGoogleARCoreDevice::GetInstance()->GetIsTrackingTypeSupported(SessionType);
}

UARLightEstimate* FGoogleARCoreXRTrackingSystem::OnGetCurrentLightEstimate() const
{
	return LightEstimate;
}

UARPin* FGoogleARCoreXRTrackingSystem::OnPinComponent(USceneComponent* ComponentToPin, const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry /*= nullptr*/, const FName DebugName /*= NAME_None*/)
{
	UARPin* NewARPin = nullptr;
	// TODO: error handling?
	FGoogleARCoreDevice::GetInstance()->CreateARPin(PinToWorldTransform, TrackedGeometry, ComponentToPin, DebugName, NewARPin);
	return NewARPin;
}

void FGoogleARCoreXRTrackingSystem::OnRemovePin(UARPin* PinToRemove)
{
	FGoogleARCoreDevice::GetInstance()->RemoveARPin(PinToRemove);
}

TArray<FVector> FGoogleARCoreXRTrackingSystem::OnGetPointCloud() const
{
	TArray<FVector> PointCloudPoints;
	UGoogleARCorePointCloud* LatestPointCloud = nullptr;
	if (!(FGoogleARCoreDevice::GetInstance()->GetLatestPointCloud(LatestPointCloud) == EGoogleARCoreFunctionStatus::Success)) 
	{
		return PointCloudPoints;
	}

#if PLATFORM_ANDROID //Static analysis complains that i is always < 0, because GetPointNum returns 0 on non-android platforms
	for(int i=0; i<LatestPointCloud->GetPointNum(); i++)
	{
		FVector Point = FVector::ZeroVector;
		float Confident = 0;
		LatestPointCloud->GetPoint(i, Point, Confident);
		PointCloudPoints.Add(Point);
	}
#endif

	return PointCloudPoints;
}

bool FGoogleARCoreXRTrackingSystem::OnAddRuntimeCandidateImage(UARSessionConfig* SessionConfig, UTexture2D* CandidateTexture, FString FriendlyName, float PhysicalWidth)
{
	EPixelFormat PixelFormat = CandidateTexture->GetPixelFormat();

	if (PixelFormat == EPixelFormat::PF_B8G8R8A8 || PixelFormat == EPixelFormat::PF_G8)
	{
		ensure(CandidateTexture->GetNumMips() > 0);
		FTexture2DMipMap* Mip0 = &CandidateTexture->PlatformData->Mips[0];
		FByteBulkData* RawImageData = &Mip0->BulkData;

		int ImageWidth = CandidateTexture->GetSizeX();
		int ImageHeight = CandidateTexture->GetSizeY();

		TArray<uint8> GrayscaleBuffer;
		int PixelNum = ImageWidth * ImageHeight;
		uint8* RawBytes = static_cast<uint8*>(RawImageData->Lock(LOCK_READ_ONLY));
		if (PixelFormat == EPixelFormat::PF_B8G8R8A8)
		{
			GrayscaleBuffer.SetNumUninitialized(PixelNum);
			ensureMsgf(RawImageData->GetBulkDataSize() == ImageWidth * ImageHeight * 4,
				TEXT("Unsupported texture data when adding runtime candidate image."));
			for (int i = 0; i < PixelNum; i++)
			{
				uint8 R = RawBytes[i * 4 + 2];
				uint8 G = RawBytes[i * 4 + 1];
				uint8 B = RawBytes[i * 4];
				GrayscaleBuffer[i] = 0.2126 * R + 0.7152 * G + 0.0722 * B;
			}
		}
		else
		{
			ensureMsgf(RawImageData->GetBulkDataSize() == ImageWidth * ImageHeight,
				TEXT("Unsupported texture data when adding runtime candidate image."));
			GrayscaleBuffer = TArray<uint8>(RawBytes, PixelNum);
		}
		RawImageData->Unlock();
		return AddRuntimeGrayscaleImage(SessionConfig, GrayscaleBuffer,
			ImageWidth, ImageHeight, FriendlyName, PhysicalWidth);
	}

	UE_LOG(LogGoogleARCoreTrackingSystem, Warning, TEXT("Failed to add runtime candidate image: Unsupported texture format: %s. ARCore only support PF_B8G8R8A8 or PF_G8 for now for adding runtime candidate image in ARCore"), GetPixelFormatString(PixelFormat));
	return false;
}

bool FGoogleARCoreXRTrackingSystem::AddRuntimeGrayscaleImage(UARSessionConfig* SessionConfig, const TArray<uint8>& ImageGrayscalePixels, int ImageWidth, int ImageHeight, FString FriendlyName, float PhysicalWidth)
{
	return FGoogleARCoreDevice::GetInstance()->AddRuntimeCandidateImage(SessionConfig, ImageGrayscalePixels,
		ImageWidth, ImageHeight, FriendlyName, PhysicalWidth);
}

void FGoogleARCoreXRTrackingSystem::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (LightEstimate != nullptr)
	{
		Collector.AddReferencedObject(LightEstimate);
	}

	if (EventManager != nullptr)
	{
		Collector.AddReferencedObject(EventManager);
	}
}

TSharedPtr<class IXRCamera, ESPMode::ThreadSafe> FGoogleARCoreXRTrackingSystem::GetXRCamera(int32 DeviceId /*= HMDDeviceId*/)
{
	check(DeviceId == HMDDeviceId);

	if (!XRCamera.IsValid())
	{
		XRCamera = FSceneViewExtensions::NewExtension<FGoogleARCoreXRCamera>(*this, DeviceId);
	}
	return XRCamera;
}
