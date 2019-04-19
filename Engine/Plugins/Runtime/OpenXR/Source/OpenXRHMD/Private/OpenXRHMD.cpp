// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenXRHMD.h"
#include "OpenXRHMDPrivate.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "IOpenXRHMDPlugin.h"
#include "SceneRendering.h"
#include "PostProcess/PostProcessHMD.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/CString.h"
#include "ClearQuad.h"
#include "XRThreadUtils.h"
#include "PipelineStateCache.h"
#include "Slate/SceneViewport.h"
#include "Engine/GameEngine.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#endif

namespace {
	struct FormatMap
	{
		DXGI_FORMAT DxFormat;
		EPixelFormat PixelFormat;
	};
	// Map of D3D texture formats to PixelFormats. 
	const FormatMap SupportedColorSwapchainFormats[] = {
		{ DXGI_FORMAT_R8G8B8A8_UNORM, PF_R8G8B8A8 },
		{ DXGI_FORMAT_B8G8R8A8_UNORM, PF_B8G8R8A8 },
		{ DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, PF_R8G8B8A8 },
		{ DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, PF_B8G8R8A8 },
    };

	/** Helper function for acquiring the appropriate FSceneViewport */
	FSceneViewport* FindSceneViewport()
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
}

//---------------------------------------------------
// OpenXRHMD Plugin Implementation
//---------------------------------------------------

class FOpenXRHMDPlugin : public IOpenXRHMDPlugin
{
public:
	FOpenXRHMDPlugin()
		: LoaderHandle(nullptr)
		, Instance(XR_NULL_HANDLE)
		, System(XR_NULL_SYSTEM_ID)
		, AdapterLuid(0)
	{ }

	/** IHeadMountedDisplayModule implementation */
	virtual TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > CreateTrackingSystem() override;
	virtual uint64 GetGraphicsAdapterLuid() override;
	virtual bool PreInit() override;

	FString GetModuleKeyName() const override
	{
		return FString(TEXT("OpenXRHMD"));
	}

	void GetModuleAliases(TArray<FString>& AliasesOut) const override
	{
		AliasesOut.Add(TEXT("OpenXR"));
	}

	void ShutdownModule() override
	{
		if (LoaderHandle)
		{
			FPlatformProcess::FreeDllHandle(LoaderHandle);
			LoaderHandle = nullptr;
		}
	}

	virtual bool IsHMDConnected() override { return true; }

private:
	void *LoaderHandle;
	XrInstance Instance;
	XrSystemId System;
	uint64 AdapterLuid;
};

IMPLEMENT_MODULE( FOpenXRHMDPlugin, OpenXRHMD )

TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > FOpenXRHMDPlugin::CreateTrackingSystem()
{
	auto OpenXRHMD = FSceneViewExtensions::NewExtension<FOpenXRHMD>(Instance, System);
	if( OpenXRHMD->IsInitialized() )
	{
		return OpenXRHMD;
	}
	return nullptr;
}

uint64 FOpenXRHMDPlugin::GetGraphicsAdapterLuid()
{
	return AdapterLuid;
}

bool FOpenXRHMDPlugin::PreInit()
{
#if PLATFORM_WINDOWS
#if PLATFORM_64BITS
	FString BinariesPath = FPaths::EngineDir() / FString(TEXT("Binaries/ThirdParty/OpenXR/win64"));
#else
	FString BinariesPath = FPaths::EngineDir() / FString(TEXT("Binaries/ThirdParty/OpenXR/win32"));
#endif

	FString LoaderName = FString::Printf(TEXT("openxr_loader-%d_%d.dll"), XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), XR_VERSION_MINOR(XR_CURRENT_API_VERSION));
	FPlatformProcess::PushDllDirectory(*BinariesPath);
	LoaderHandle = FPlatformProcess::GetDllHandle(*(BinariesPath / LoaderName));
	FPlatformProcess::PopDllDirectory(*BinariesPath);
#endif

	if (!LoaderHandle)
	{
		UE_LOG(LogHMD, Log, TEXT("Failed to load openxr_loader-%d_%d.dll"), XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), XR_VERSION_MINOR(XR_CURRENT_API_VERSION));
		return false;
	}

	FString AppName = FApp::GetName();
	const char* extensions[] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME };

	XrInstanceCreateInfo Info;
	Info.type = XR_TYPE_INSTANCE_CREATE_INFO;
	Info.next = nullptr;
	Info.createFlags = 0;
	FPlatformString::Convert(Info.applicationInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, GetData(AppName), AppName.Len() + 1);
	Info.applicationInfo.applicationVersion = 0;
	FCStringAnsi::Strcpy(Info.applicationInfo.engineName, XR_MAX_ENGINE_NAME_SIZE, "Unreal Engine");
	Info.applicationInfo.engineVersion = (uint32)FEngineVersion::Current().GetMajor() << 16 | FEngineVersion::Current().GetMinor();
	Info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
	Info.enabledApiLayerCount = 0;
	Info.enabledApiLayerNames = nullptr;
	Info.enabledExtensionCount = 1;
	Info.enabledExtensionNames = extensions;
	XrResult rs = xrCreateInstance(&Info, &Instance);
	if (XR_FAILED(rs))
	{
		char error[XR_MAX_RESULT_STRING_SIZE] = { '\0' };
		xrResultToString(XR_NULL_HANDLE, rs, error);
		UE_LOG(LogHMD, Log, TEXT("Failed to create an OpenXR instance, result is %s. Please check if you have an OpenXR runtime installed."), error);
		return false;
	}

	XrSystemGetInfo SystemInfo;
	SystemInfo.type = XR_TYPE_SYSTEM_GET_INFO;
	SystemInfo.next = nullptr;
	SystemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	rs = xrGetSystem(Instance, &SystemInfo, &System);
	if (XR_FAILED(rs))
	{
		char error[XR_MAX_RESULT_STRING_SIZE] = { '\0' };
		xrResultToString(XR_NULL_HANDLE, rs, error);
		UE_LOG(LogHMD, Log, TEXT("Failed to get an OpenXR system, result is %s. Please check that your runtime supports VR headsets."), error);
		return false;
	}

	XrGraphicsRequirementsD3D11KHR Requirements;
	Requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR;
	Requirements.next = nullptr;
	if (XR_ENSURE(xrGetD3D11GraphicsRequirementsKHR(Instance, System, &Requirements)))
	{
		AdapterLuid = reinterpret_cast<uint64&>(Requirements.adapterLuid);
	}
	return true;
}

FOpenXRHMD::FOpenXRSwapchain::FOpenXRSwapchain(XrSwapchain InSwapchain, FTexture2DRHIParamRef InRHITexture, const TArray<FTexture2DRHIRef>& InRHITextureSwapChain) :
	Handle(InSwapchain), RHITexture(InRHITexture), RHITextureSwapChain(InRHITextureSwapChain), SwapChainIndex_RenderThread(0), IsAcquired(false)
{
	IncrementSwapChainIndex_RenderThread(XR_NO_DURATION);
}

FOpenXRHMD::FOpenXRSwapchain::~FOpenXRSwapchain()
{
	if (IsInGameThread())
	{
		ExecuteOnRenderThread([this]()
		{
			ReleaseResources_RenderThread();
		});
	}
	else
	{
		ReleaseResources_RenderThread();
	}
}

void FOpenXRHMD::FOpenXRSwapchain::IncrementSwapChainIndex_RenderThread(XrDuration Timeout)
{
	check(IsInRenderingThread());

	if (IsAcquired)
		return;
	
	XrSwapchainImageAcquireInfo Info;
	Info.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
	Info.next = nullptr;
	XR_ENSURE(xrAcquireSwapchainImage(Handle, &Info, &SwapChainIndex_RenderThread));

	IsAcquired = true;

	XrSwapchainImageWaitInfo WaitInfo;
	WaitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
	WaitInfo.next = nullptr;
	WaitInfo.timeout = Timeout;
	XR_ENSURE(xrWaitSwapchainImage(Handle, &WaitInfo));

	FD3D11DynamicRHI* DynamicRHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
	DynamicRHI->RHIAliasTextureResources(RHITexture, RHITextureSwapChain[SwapChainIndex_RenderThread]);
}

void FOpenXRHMD::FOpenXRSwapchain::ReleaseSwapChainImage_RenderThread()
{
	check(IsInRenderingThread());

	if (!IsAcquired)
		return;

	XrSwapchainImageReleaseInfo ReleaseInfo;
	ReleaseInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
	ReleaseInfo.next = nullptr;
	XR_ENSURE(xrReleaseSwapchainImage(Handle, &ReleaseInfo));

	IsAcquired = false;
}

void FOpenXRHMD::FOpenXRSwapchain::ReleaseResources_RenderThread()
{
	check(IsInRenderingThread());

	RHITexture = nullptr;
	RHITextureSwapChain.Empty();
	xrDestroySwapchain(Handle);
}

float FOpenXRHMD::GetWorldToMetersScale() const
{
	return 100.0f;
}

//---------------------------------------------------
// OpenXRHMD IHeadMountedDisplay Implementation
//---------------------------------------------------

bool FOpenXRHMD::IsHMDEnabled() const
{
	return true;
}

void FOpenXRHMD::EnableHMD(bool enable)
{
}

bool FOpenXRHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
	MonitorDesc.MonitorName = "";
	MonitorDesc.MonitorId = 0;
	MonitorDesc.DesktopX = MonitorDesc.DesktopY = MonitorDesc.ResolutionX = MonitorDesc.ResolutionY = 0;
	return false;
}

void FOpenXRHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	OutHFOVInDegrees = 0.0f;
	OutVFOVInDegrees = 0.0f;
}

bool FOpenXRHMD::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		OutDevices.Add(IXRTrackingSystem::HMDDeviceId);
	}
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::Controller)
	{
		for (int32 i = 0; i < DeviceSpaces.Num(); i++)
		{
			OutDevices.Add(i);
		}
	}
	return OutDevices.Num() > 0;
}

void FOpenXRHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
}

float FOpenXRHMD::GetInterpupillaryDistance() const
{
	return 0.064f;
}

bool FOpenXRHMD::GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	if (!DeviceSpaces.IsValidIndex(DeviceId) || FrameState.predictedDisplayTime <= 0)
	{
		return false;
	}

	XrSpaceRelation Relation = {};
	Relation.type = XR_TYPE_SPACE_RELATION;
	XrResult Result = xrLocateSpace(DeviceSpaces[DeviceId], GetTrackingSpace(), FrameState.predictedDisplayTime, &Relation);
	if (!XR_ENSURE(Result))
	{
		return false;
	}

	if (Relation.relationFlags & XR_SPACE_RELATION_ORIENTATION_VALID_BIT)
	{
		CurrentOrientation = ToFQuat(Relation.pose.orientation);
	}
	else
	{
		CurrentOrientation = FQuat::Identity;
	}
	if (Relation.relationFlags & XR_SPACE_RELATION_POSITION_VALID_BIT)
	{
		CurrentPosition = ToFVector(Relation.pose.position, GetWorldToMetersScale());
	}
	else
	{
		CurrentPosition = FVector::ZeroVector;
	}
	return true;
}

bool FOpenXRHMD::IsChromaAbCorrectionEnabled() const
{
	return false;
}

void FOpenXRHMD::ResetOrientationAndPosition(float yaw)
{
	ResetOrientation(yaw);
	ResetPosition();
}

void FOpenXRHMD::ResetOrientation(float Yaw)
{
}

void FOpenXRHMD::ResetPosition()
{
}

void FOpenXRHMD::SetBaseRotation(const FRotator& BaseRot)
{
}

FRotator FOpenXRHMD::GetBaseRotation() const
{
	return FRotator::ZeroRotator;
}

void FOpenXRHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
}

FQuat FOpenXRHMD::GetBaseOrientation() const
{
	return FQuat::Identity;
}

bool FOpenXRHMD::IsStereoEnabled() const
{
	return true;
}

bool FOpenXRHMD::EnableStereo(bool stereo)
{
	return true;
}

void FOpenXRHMD::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	const uint32 ViewIndex = GetViewIndexForPass(StereoPass);

	const XrViewConfigurationView& Config = Configs[ViewIndex];

	for (uint32 i = 0; i < ViewIndex; ++i)
	{
		X += Configs[i].recommendedImageRectWidth;
	}

	SizeX = Config.recommendedImageRectWidth;
	SizeY = Config.recommendedImageRectHeight;
}

EStereoscopicPass FOpenXRHMD::GetViewPassForIndex(bool bStereoRequested, uint32 ViewIndex) const
{
	if (!bStereoRequested)
		return EStereoscopicPass::eSSP_FULL;

	return static_cast<EStereoscopicPass>(eSSP_LEFT_EYE + ViewIndex);
}

uint32 FOpenXRHMD::GetViewIndexForPass(EStereoscopicPass StereoPassType) const
{
	switch (StereoPassType)
	{
	case eSSP_LEFT_EYE:
	case eSSP_FULL:
		return 0;

	case eSSP_RIGHT_EYE:
		return 1;

	default:
		return StereoPassType - eSSP_LEFT_EYE;
	}
}

int32 FOpenXRHMD::GetDesiredNumberOfViews(bool bStereoRequested) const
{
	return bStereoRequested ? Views.Num() : 1; // FIXME: Monoscopic actually needs 2 views for quad vr
}

bool FOpenXRHMD::GetRelativeEyePose(int32 InDeviceId, EStereoscopicPass InEye, FQuat& OutOrientation, FVector& OutPosition)
{
	if (InDeviceId != IXRTrackingSystem::HMDDeviceId)
	{
		return false;
	}

	const uint32 ViewIndex = GetViewIndexForPass(InEye);
	const XrView& View = Views[ViewIndex];
	OutOrientation = ToFQuat(View.pose.orientation);
	OutPosition = ToFVector(View.pose.position, GetWorldToMetersScale());
	return true;
}

FMatrix FOpenXRHMD::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const
{
	const uint32 ViewIndex = GetViewIndexForPass(StereoPassType);

	XrFovf Fov = Views[ViewIndex].fov;

	float ZNear = GNearClippingPlane;

	Fov.angleUp = tan(Fov.angleUp);
	Fov.angleDown = tan(Fov.angleDown);
	Fov.angleLeft = tan(-Fov.angleLeft);
	Fov.angleRight = tan(-Fov.angleRight);

	float SumRL = (Fov.angleLeft + Fov.angleRight);
	float SumTB = (Fov.angleUp + Fov.angleDown);
	float InvRL = (1.0f / (Fov.angleLeft - Fov.angleRight));
	float InvTB = (1.0f / (Fov.angleUp - Fov.angleDown));

	FMatrix Mat = FMatrix(
		FPlane((2.0f * InvRL), 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, (2.0f * InvTB), 0.0f, 0.0f),
		FPlane((SumRL * InvRL), (SumTB * InvTB), 0.0f, 1.0f),
		FPlane(0.0f, 0.0f, ZNear, 0.0f)
	);

	return Mat;
}

void FOpenXRHMD::GetEyeRenderParams_RenderThread(const FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	EyeToSrcUVOffsetValue = FVector2D::ZeroVector;
	EyeToSrcUVScaleValue = FVector2D(1.0f, 1.0f);
}


void FOpenXRHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	InViewFamily.EngineShowFlags.MotionBlur = 0;
	InViewFamily.EngineShowFlags.HMDDistortion = false;
	InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();

	// TODO: Handle dynamic resolution in the driver, so the runtime
	// can take advantage of the extra resolution in the distortion process.
	InViewFamily.EngineShowFlags.ScreenPercentage = 0;

	// TODO: Move this to EnableStereo
	// Uncap fps to enable FPS higher than 62
	GEngine->bForceDisableFrameRateSmoothing = true;

	if (Configs.Num() > 2)
	{
		InViewFamily.EngineShowFlags.Vignette = 0;
		InViewFamily.EngineShowFlags.Bloom = 0;
	}
}

void FOpenXRHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
}

void FOpenXRHMD::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FOpenXRHMD::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	check(IsInRenderingThread());
}

void FOpenXRHMD::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());

	if (SpectatorScreenController)
	{
		SpectatorScreenController->UpdateSpectatorScreenMode_RenderThread();
	}
}

bool FOpenXRHMD::IsActiveThisFrame(class FViewport* InViewport) const
{
	return GEngine && GEngine->IsStereoscopic3D(InViewport);
}

FOpenXRHMD::FOpenXRHMD(const FAutoRegister& AutoRegister, XrInstance InInstance, XrSystemId InSystem)
	: FHeadMountedDisplayBase(nullptr)
	, FSceneViewExtensionBase(AutoRegister)
	, bIsRunning(false)
	, Instance(InInstance)
	, System(InSystem)
	, Session(XR_NULL_HANDLE)
	, DeviceSpaces()
	, LocalSpace(XR_NULL_HANDLE)
	, StageSpace(XR_NULL_HANDLE)
	, TrackingSpaceType(XR_REFERENCE_SPACE_TYPE_STAGE)
	, RenderBridge(nullptr)
	, Swapchain(XR_NULL_HANDLE)
{
	{
		// Enumerate the viewport configurations
		uint32 ConfigurationCount;
		TArray<XrViewConfigurationType> Types;
		XR_ENSURE(xrEnumerateViewConfigurations(Instance, System, 0, &ConfigurationCount, nullptr));
		Types.SetNum(ConfigurationCount);
		XR_ENSURE(xrEnumerateViewConfigurations(Instance, System, ConfigurationCount, &ConfigurationCount, Types.GetData()));

		// Ensure the configuration type we want is provided
		ensure(Types.Contains(XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO));

		// Enumerate the viewport view configurations
		uint32 ViewCount;
		XR_ENSURE(xrEnumerateViewConfigurationViews(Instance, System, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &ViewCount, nullptr));
		Configs.SetNum(ViewCount);
		for (XrViewConfigurationView& View : Configs)
		{
			View.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
			View.next = nullptr;
		}
		XR_ENSURE(xrEnumerateViewConfigurationViews(Instance, System, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, ViewCount, &ViewCount, Configs.GetData()));
	}

	FOpenXRHMD* Self = this;
	ENQUEUE_RENDER_COMMAND(OpenXRCreateSession)([Self](FRHICommandListImmediate& RHICmdList)
	{
#if PLATFORM_WINDOWS
		XrGraphicsBindingD3D11KHR Binding = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR, nullptr, (ID3D11Device*)RHIGetNativeDevice() };
#endif
		XrSessionCreateInfo SessionInfo;
		SessionInfo.type = XR_TYPE_SESSION_CREATE_INFO;
		SessionInfo.next = &Binding;
		SessionInfo.createFlags = 0;
		SessionInfo.systemId = Self->System;
		XR_ENSURE(xrCreateSession(Self->Instance, &SessionInfo, &Self->Session));
	});

	// Ensure the views have sane values before we locate them
	Views.SetNum(Configs.Num());
	for (XrView& View : Views)
	{
		View.type = XR_TYPE_VIEW;
		View.next = nullptr;
		View.fov = XrFovf{ -PI / 4.0f, PI / 4.0f, PI / 4.0f, -PI / 4.0f };
		View.pose = ToXrPose(FTransform::Identity);
	}

	FlushRenderingCommands();

	uint32_t referenceSpacesCount;
	XR_ENSURE(xrEnumerateReferenceSpaces(Session, 0, &referenceSpacesCount, nullptr));

	TArray<XrReferenceSpaceType> spaces;
	spaces.SetNum(referenceSpacesCount);
	XR_ENSURE(xrEnumerateReferenceSpaces(Session, (uint32_t)spaces.Num(), &referenceSpacesCount, spaces.GetData()));
	ensure(referenceSpacesCount == spaces.Num());

	XrSpace Space;
	XrReferenceSpaceCreateInfo SpaceInfo;
	ensure(spaces.Contains(XR_REFERENCE_SPACE_TYPE_VIEW));
	SpaceInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	SpaceInfo.next = nullptr;
	SpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	SpaceInfo.poseInReferenceSpace = ToXrPose(FTransform::Identity);
	XR_ENSURE(xrCreateReferenceSpace(Session, &SpaceInfo, &Space));
	DeviceSpaces.Add(Space);

	ensure(spaces.Contains(XR_REFERENCE_SPACE_TYPE_LOCAL));
	SpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	XR_ENSURE(xrCreateReferenceSpace(Session, &SpaceInfo, &LocalSpace));

	// Prefer a stage space over a local space
	if (spaces.Contains(XR_REFERENCE_SPACE_TYPE_STAGE))
	{
		TrackingSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		SpaceInfo.referenceSpaceType = TrackingSpaceType;
		XR_ENSURE(xrCreateReferenceSpace(Session, &SpaceInfo, &StageSpace));
	}
	else
	{
		TrackingSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	}

#if PLATFORM_WINDOWS
	RenderBridge = new D3D11Bridge(this);
#endif
	ensure(RenderBridge != nullptr);

	// grab a pointer to the renderer module for displaying our mirror window
	static const FName RendererModuleName("Renderer");
	RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	SpectatorScreenController = MakeUnique<FDefaultSpectatorScreenController>(this);
}

FOpenXRHMD::~FOpenXRHMD()
{
	if (Session)
	{
		xrDestroySession(Session);
	}
	if (Instance)
	{
		xrDestroyInstance(Instance);
	}
}

int32 FOpenXRHMD::AddActionDevice(XrAction Action)
{
	XrSpace Space;
	XrActionSpaceCreateInfo SpaceInfo;
	SpaceInfo.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
	SpaceInfo.next = nullptr;
	SpaceInfo.subactionPath = XR_NULL_PATH;
	SpaceInfo.poseInActionSpace = ToXrPose(FTransform::Identity);
	XR_ENSURE(xrCreateActionSpace(Action, &SpaceInfo, &Space));
	return DeviceSpaces.Add(Space);
}

bool FOpenXRHMD::IsInitialized() const
{
	return Session != XR_NULL_HANDLE;
}

bool FOpenXRHMD::IsRunning() const
{
	return bIsRunning;
}

void FOpenXRHMD::OnBeginPlay(FWorldContext& InWorldContext)
{
	if (!bIsRunning)
	{
		XrSessionBeginInfo Begin = { XR_TYPE_SESSION_BEGIN_INFO, nullptr, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO };
		bIsRunning = XR_ENSURE(xrBeginSession(Session, &Begin));
	}
}

void FOpenXRHMD::OnEndPlay(FWorldContext& InWorldContext)
{
	if (bIsRunning)
	{
		bIsRunning = false;
		XR_ENSURE(xrEndSession(Session));
	}
}

IStereoRenderTargetManager* FOpenXRHMD::GetRenderTargetManager()
{
	return this;
}

bool FOpenXRHMD::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, uint32 TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	check(IsInRenderingThread());

	const FormatMap* SwapchainFormat = nullptr;
	uint32_t swapchainFormatsCount;
	XR_ENSURE(xrEnumerateSwapchainFormats(Session, 0, &swapchainFormatsCount, nullptr));

	TArray<int64_t> formats;
	formats.SetNum(swapchainFormatsCount);
	XR_ENSURE(xrEnumerateSwapchainFormats(Session, (uint32_t)formats.Num(), &swapchainFormatsCount, formats.GetData()));
	ensure(swapchainFormatsCount == formats.Num());

	// Pick the first matching swapchain format to use for the swapchain.
	for (int i = 0; i < _countof(SupportedColorSwapchainFormats); i++)
	{
		if (formats.Contains(SupportedColorSwapchainFormats[i].DxFormat))
		{
			SwapchainFormat = &SupportedColorSwapchainFormats[i];
			break;
		}
	}

	if (SwapchainFormat == nullptr)
	{
		UE_LOG(LogHMD, Log, TEXT("No valid swapchain format found."));
		return false;
	}

	XrSwapchain SwapchainHandle;
	XrSwapchainCreateInfo info;
	info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
	info.next = nullptr;
	info.createFlags = 0;
	info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
	info.format = SwapchainFormat->DxFormat; // FIXME: (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
	info.sampleCount = NumSamples;
	info.width = SizeX;
	info.height = SizeY;
	info.faceCount = 1;
	info.arraySize = 1;
	info.mipCount = NumMips;
	if (!XR_ENSURE(xrCreateSwapchain(Session, &info, &SwapchainHandle)))
	{
		return false;
	}

	uint32_t ChainCount;
	TArray<XrSwapchainImageD3D11KHR> Images;
	xrEnumerateSwapchainImages(SwapchainHandle, 0, &ChainCount, nullptr);

	Images.AddZeroed(ChainCount);
	for (auto& Image : Images)
	{
		Image.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
	}
	XR_ENSURE(xrEnumerateSwapchainImages(SwapchainHandle, ChainCount, &ChainCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(Images.GetData())));

	FD3D11DynamicRHI* DynamicRHI = static_cast<FD3D11DynamicRHI*>(GDynamicRHI);
	TArray<FTexture2DRHIRef> TextureChain;
	FTexture2DRHIRef ChainTarget = DynamicRHI->RHICreateTexture2DFromResource(SwapchainFormat->PixelFormat, TexCreate_RenderTargetable | TexCreate_ShaderResource, FClearValueBinding::Black, Images[0].texture);
	for (const auto& Image : Images)
	{
		TextureChain.Add(DynamicRHI->RHICreateTexture2DFromResource(SwapchainFormat->PixelFormat, TexCreate_RenderTargetable | TexCreate_ShaderResource, FClearValueBinding::Black, Image.texture));
	}

	Swapchain = MakeShareable(new FOpenXRSwapchain(SwapchainHandle, ChainTarget, TextureChain));
	OutTargetableTexture = OutShaderResourceTexture = ChainTarget;
	return true;
}

void FOpenXRHMD::OnBeginRendering_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	XrFrameBeginInfo BeginInfo;
	BeginInfo.type = XR_TYPE_FRAME_BEGIN_INFO;
	BeginInfo.next = nullptr;
	xrBeginFrame(Session, &BeginInfo);

	const FSceneView* MainView = ViewFamily.Views[0];
	check(MainView);
	BaseTransform = FTransform(MainView->BaseHmdOrientation, MainView->BaseHmdLocation);

	Swapchain->IncrementSwapChainIndex_RenderThread(FrameStateRHI.predictedDisplayPeriod);

	ViewsRHI.SetNum(Views.Num());
	int32 OffsetX = 0;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const XrView& View = Views[ViewIndex];
		const XrViewConfigurationView& Config = Configs[ViewIndex];
		FTransform ViewTransform = ToFTransform(View.pose, GetWorldToMetersScale());

		XrCompositionLayerProjectionView& Projection = ViewsRHI[ViewIndex];
		Projection.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		Projection.next = nullptr;
		Projection.fov = View.fov;
		Projection.pose = ToXrPose(ViewTransform * BaseTransform, GetWorldToMetersScale());
		Projection.subImage.swapchain = Swapchain->Handle;
		Projection.subImage.imageArrayIndex = 0;
		Projection.subImage.imageRect = {
			{ OffsetX, 0 },
			{
				(int32)Config.recommendedImageRectWidth,
				(int32)Config.recommendedImageRectHeight
			}
		};
		OffsetX += Config.recommendedImageRectWidth;
	}

	// Give the RHI thread its own copy of the frame state and tracking space
	FrameStateRHI = FrameState;
	TrackingSpaceRHI = GetTrackingSpace();
}

void FOpenXRHMD::OnLateUpdateApplied_RenderThread(const FTransform& NewRelativeTransform)
{
	FHeadMountedDisplayBase::OnLateUpdateApplied_RenderThread(NewRelativeTransform);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		XrCompositionLayerProjectionView& Projection = ViewsRHI[ViewIndex];
		FTransform ViewTransform = ToFTransform(Projection.pose, GetWorldToMetersScale()) * BaseTransform.Inverse();
		Projection.pose = ToXrPose(ViewTransform * NewRelativeTransform, GetWorldToMetersScale());
	}
}

void FOpenXRHMD::OnBeginRendering_GameThread()
{
	XrFrameWaitInfo WaitInfo;
	WaitInfo.type = XR_TYPE_FRAME_WAIT_INFO;
	WaitInfo.next = nullptr;
	XR_ENSURE(xrWaitFrame(Session, &WaitInfo, &FrameState));

	uint32_t ViewCount;
	XrViewLocateInfo ViewInfo;
	ViewInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
	ViewInfo.next = nullptr;
	ViewInfo.space = DeviceSpaces[HMDDeviceId];
	ViewInfo.displayTime = FrameState.predictedDisplayTime;
	XR_ENSURE(xrLocateViews(Session, &ViewInfo, &ViewState, 0, &ViewCount, nullptr));
	Views.SetNum(ViewCount);
	XR_ENSURE(xrLocateViews(Session, &ViewInfo, &ViewState, Views.Num(), &ViewCount, Views.GetData()));
}

bool FOpenXRHMD::OnStartGameFrame(FWorldContext& WorldContext)
{
	// Initialize an event buffer to hold the output.
	XrEventDataBuffer event;
	// Only the header needs to be initialized.
	event.type = XR_TYPE_EVENT_DATA_BUFFER;
	event.next = nullptr;
	while (xrPollEvent(Instance, &event) == XR_SUCCESS) {
		switch (event.type) {
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
		{
			const XrEventDataSessionStateChanged& SessionState =
				reinterpret_cast<XrEventDataSessionStateChanged&>(event);
			if (SessionState.state != XR_SESSION_STATE_STOPPING && SessionState.state != XR_SESSION_STATE_EXITING)
			{
				break;
			}
		}
		// Intentional fall-through
		case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
		{
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
			break;
		}
		case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
		{
			const XrEventDataReferenceSpaceChangePending& SpaceChange =
				reinterpret_cast<XrEventDataReferenceSpaceChangePending&>(event);
			if (SpaceChange.referenceSpaceType == TrackingSpaceType)
			{
				OnTrackingOriginChanged();
			}
			break;
		}
		}
	}

	return true;
}

void FOpenXRHMD::FinishRendering()
{
	XrCompositionLayerProjection Layer = {};
	Layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
	Layer.next = nullptr;
	Layer.space = TrackingSpaceRHI;
	Layer.viewCount = ViewsRHI.Num();
	Layer.views = ViewsRHI.GetData();

	Swapchain->ReleaseSwapChainImage_RenderThread();

	XrFrameEndInfo EndInfo;
	XrCompositionLayerBaseHeader* Headers[1] = { reinterpret_cast<XrCompositionLayerBaseHeader*>(&Layer) };
	EndInfo.type = XR_TYPE_FRAME_END_INFO;
	EndInfo.next = nullptr;
	EndInfo.displayTime = FrameStateRHI.predictedDisplayTime;
	EndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	EndInfo.layerCount = 1;
	EndInfo.layers = reinterpret_cast<XrCompositionLayerBaseHeader**>(Headers);
	XrResult Result = xrEndFrame(Session, &EndInfo);

	// Ignore invalid call order for now, we will recover on the next frame
	ensure(XR_SUCCEEDED(Result) || Result == XR_ERROR_CALL_ORDER_INVALID);
}

FXRRenderBridge* FOpenXRHMD::GetActiveRenderBridge_GameThread(bool /* bUseSeparateRenderTarget */)
{
	return RenderBridge;
}

FIntPoint FOpenXRHMD::GetIdealRenderTargetSize() const
{
	FIntPoint Size(EForceInit::ForceInitToZero);
	for (XrViewConfigurationView Config : Configs)
	{
		Size.X += (int)Config.recommendedImageRectWidth;
		Size.Y = FMath::Max(Size.Y, (int)Config.recommendedImageRectHeight);
	}
	return Size;
}

FIntRect FOpenXRHMD::GetFullFlatEyeRect_RenderThread(FTexture2DRHIRef EyeTexture) const
{
	FVector2D SrcNormRectMin(0.05f, 0.2f);
	FVector2D SrcNormRectMax(0.45f, 0.8f);
	if (Configs.Num() > 2)
	{
		SrcNormRectMin.X /= 2;
		SrcNormRectMax.X /= 2;
	}

	return FIntRect(EyeTexture->GetSizeX() * SrcNormRectMin.X, EyeTexture->GetSizeY() * SrcNormRectMin.Y, EyeTexture->GetSizeX() * SrcNormRectMax.X, EyeTexture->GetSizeY() * SrcNormRectMax.Y);
}

void FOpenXRHMD::CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef SrcTexture, FIntRect SrcRect, FTexture2DRHIParamRef DstTexture, FIntRect DstRect, bool bClearBlack, bool bNoAlpha) const
{
	check(IsInRenderingThread());

	const uint32 ViewportWidth = DstRect.Width();
	const uint32 ViewportHeight = DstRect.Height();
	const FIntPoint TargetSize(ViewportWidth, ViewportHeight);

	const float SrcTextureWidth = SrcTexture->GetSizeX();
	const float SrcTextureHeight = SrcTexture->GetSizeY();
	float U = 0.f, V = 0.f, USize = 1.f, VSize = 1.f;
	if (!SrcRect.IsEmpty())
	{
		U = SrcRect.Min.X / SrcTextureWidth;
		V = SrcRect.Min.Y / SrcTextureHeight;
		USize = SrcRect.Width() / SrcTextureWidth;
		VSize = SrcRect.Height() / SrcTextureHeight;
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SetRenderTarget(RHICmdList, DstTexture, FTextureRHIRef());
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (bClearBlack)
	{
		const FIntRect ClearRect(0, 0, DstTexture->GetSizeX(), DstTexture->GetSizeY());
		RHICmdList.SetViewport(ClearRect.Min.X, ClearRect.Min.Y, 0, ClearRect.Max.X, ClearRect.Max.Y, 1.0f);
		DrawClearQuad(RHICmdList, FLinearColor::Black);
	}

	RHICmdList.SetViewport(DstRect.Min.X, DstRect.Min.Y, 0, DstRect.Max.X, DstRect.Max.Y, 1.0f);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = bNoAlpha ? TStaticBlendState<>::GetRHI() : TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	const auto FeatureLevel = GMaxRHIFeatureLevel;
	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
	TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	const bool bSameSize = DstRect.Size() == SrcRect.Size();
	if (bSameSize)
	{
		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SrcTexture);
	}
	else
	{
		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTexture);
	}

	RendererModule->DrawRectangle(
		RHICmdList,
		0, 0,
		ViewportWidth, ViewportHeight,
		U, V,
		USize, VSize,
		TargetSize,
		FIntPoint(1, 1),
		*VertexShader,
		EDRF_Default);
}

void FOpenXRHMD::RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture, FVector2D WindowSize) const
{
	if (SpectatorScreenController)
	{
		SpectatorScreenController->RenderSpectatorScreen_RenderThread(RHICmdList, BackBuffer, SrcTexture, WindowSize);
	}
}

void FOpenXRHMD::DrawHiddenAreaMesh_RenderThread(class FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	check(IsInRenderingThread());
	check(StereoPass != eSSP_FULL);

#if 0
	const uint32 ViewIndex = GetViewIndexForPass(StereoPass);
	const FHMDViewMesh& Mesh = HiddenAreaMeshes[ViewIndex];
	check(Mesh.IsValid());

	RHICmdList.SetStreamSource(0, Mesh.VertexBufferRHI, 0);
	RHICmdList.DrawIndexedPrimitive(Mesh.IndexBufferRHI, 0, 0, Mesh.NumVertices, 0, Mesh.NumTriangles, 1);
#endif
}

bool FOpenXRHMD::D3D11Bridge::Present(int32& InOutSyncInterval)
{
	OpenXRHMD->FinishRendering();

	InOutSyncInterval = 0; // VSync off

	return true;
}
