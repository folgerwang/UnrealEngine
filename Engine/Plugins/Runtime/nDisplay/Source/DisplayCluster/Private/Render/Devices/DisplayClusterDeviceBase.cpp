// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Render/Devices/DisplayClusterDeviceBase.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/Controller/IPDisplayClusterNodeController.h"
#include "Config/IPDisplayClusterConfigManager.h"
#include "Game/IPDisplayClusterGameManager.h"

#include "DisplayClusterScreenComponent.h"

#include "HAL/IConsoleManager.h"

#include "RHIStaticStates.h"
#include "Slate/SceneViewport.h"

#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"

#include "Render/Devices/DisplayClusterViewportArea.h"
#include "Render/Devices/DisplayClusterRenderViewport.h"

#include "DisplayClusterGlobals.h"

#include <utility>



#if 0
static int CVarVSyncInterval_Value = 1;
static FAutoConsoleVariableRef  CVarVSyncInterval(
	TEXT("nDisplay.SoftSync.VSyncInterval"),
	CVarVSyncInterval_Value,
	TEXT("VSync interval")
);
#endif


FDisplayClusterDeviceBase::FDisplayClusterDeviceBase(uint32 ViewsPerViewport)
	: FRHICustomPresent()
	, ViewsAmountPerViewport(ViewsPerViewport)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
}

FDisplayClusterDeviceBase::~FDisplayClusterDeviceBase()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
}

bool FDisplayClusterDeviceBase::Initialize()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled)
	{
		return false;
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Use swap interval: %d"), SwapInterval);

	return true;
}

void FDisplayClusterDeviceBase::UpdateProjectionDataForThisFrame()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	check(IsInGameThread());

	if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled)
	{
		return;
	}

	// Store transformations of active projection screens
	check(RenderViewports.Num() > 0);
	for (FDisplayClusterRenderViewport& RenderViewport : RenderViewports)
	{
		const IDisplayClusterProjectionScreenDataProvider* const DataProvider = RenderViewport.GetProjectionDataProvider();
		if (!DataProvider)
		{
			UE_LOG(LogDisplayClusterRender, Error, TEXT("Projection screen data provider not seet"));
			continue;
		}

		// Update projection screen data
		FDisplayClusterRenderViewportContext ViewportContext = RenderViewport.GetViewportContext();
		DataProvider->GetProjectionScreenData(RenderViewport.GetProjectionScreenId(), ViewportContext.ProjectionScreenData);
		RenderViewport.SetViewportContext(ViewportContext);
	}
}

void FDisplayClusterDeviceBase::WaitForBufferSwapSync(int32& InOutSyncInterval)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	// Perform SW synchronization
	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Waiting for swap sync..."));

	// Policies below are available for any render device type
	switch (SwapSyncPolicy)
	{
	case EDisplayClusterSwapSyncPolicy::None:
	{
		exec_BarrierWait();
		InOutSyncInterval = 0;
		break;
	}

	default:
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Swap sync policy drop: %d"), (int)SwapSyncPolicy);
		InOutSyncInterval = 0;
		break;
	}
	}
}

void FDisplayClusterDeviceBase::exec_BarrierWait()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled)
	{
		return;
	}

	double tTime = 0.f;
	double bTime = 0.f;

	IPDisplayClusterNodeController* const pController = GDisplayCluster->GetPrivateClusterMgr()->GetController();
	if (pController)
	{
		pController->WaitForSwapSync(&tTime, &bTime);
	}

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("Render barrier wait: t=%lf b=%lf"), tTime, bTime);
}

void FDisplayClusterDeviceBase::PerformSynchronizationPolicyNone(int32& InOutSyncInterval)
{
	InOutSyncInterval = 0;
}

void FDisplayClusterDeviceBase::PerformSynchronizationPolicySoft(int32& InOutSyncInterval)
{
}

void FDisplayClusterDeviceBase::PerformSynchronizationPolicyNvSwapLock(int32& InOutSyncInterval)
{
	UE_LOG(LogDisplayClusterRender, Warning, TEXT("NvSwapLock synchronization policy hasn't been implemented for current device. The SoftSync will be used."));
	PerformSynchronizationPolicySoft(InOutSyncInterval);
}

EStereoscopicPass FDisplayClusterDeviceBase::EncodeStereoscopicPass(int ViewIndex) const
{
	EStereoscopicPass EncodedPass = EStereoscopicPass::eSSP_FULL;

	// We don't care about mono/stereo. We need to fulfill ViewState and StereoViewStates in a proper way.
	// Look at ULocalPlayer::CalcSceneViewInitOptions for view states mapping.
	if (ViewIndex < 2)
	{
		EncodedPass = (ViewIndex == 0 ? EStereoscopicPass::eSSP_LEFT_EYE : EStereoscopicPass::eSSP_RIGHT_EYE);
	}
	else
	{
		EncodedPass = EStereoscopicPass(int(EStereoscopicPass::eSSP_RIGHT_EYE) + ViewIndex - 1);
	}

	return EncodedPass;
}

EStereoscopicPass FDisplayClusterDeviceBase::DecodeStereoscopicPass(const enum EStereoscopicPass StereoPassType) const
{
	EStereoscopicPass DecodedPass = EStereoscopicPass::eSSP_FULL;

	// Monoscopic rendering
	if (ViewsAmountPerViewport == 1)
	{
		DecodedPass = EStereoscopicPass::eSSP_FULL;
	}
	// Stereoscopic rendering
	else
	{
		switch (StereoPassType)
		{
		case EStereoscopicPass::eSSP_LEFT_EYE:
		case EStereoscopicPass::eSSP_RIGHT_EYE:
			DecodedPass = StereoPassType;
			break;

		default:
			DecodedPass = ((int(StereoPassType) - int(EStereoscopicPass::eSSP_RIGHT_EYE)) % 2 == 0) ? EStereoscopicPass::eSSP_RIGHT_EYE : EStereoscopicPass::eSSP_LEFT_EYE;
			break;
		}
	}

	return DecodedPass;
}

int FDisplayClusterDeviceBase::DecodeViewportIndex(const enum EStereoscopicPass StereoPassType) const
{
	check(ViewsAmountPerViewport > 0);

	const int DecodedViewIndex = GetViewIndexForPass(StereoPassType);
	const int DecodedViewportIndex = DecodedViewIndex / ViewsAmountPerViewport;

	return DecodedViewportIndex;
}

FDisplayClusterDeviceBase::EDisplayClusterEyeType FDisplayClusterDeviceBase::DecodeEyeType(const enum EStereoscopicPass StereoPassType) const
{
	EDisplayClusterEyeType EyeType = EDisplayClusterEyeType::Mono;

	const EStereoscopicPass DecodedPass = DecodeStereoscopicPass(StereoPassType);
	switch (DecodedPass)
	{
	case EStereoscopicPass::eSSP_LEFT_EYE:
		EyeType = EDisplayClusterEyeType::StereoLeft;
		break;

	case EStereoscopicPass::eSSP_FULL:
		EyeType = EDisplayClusterEyeType::Mono;
		break;

	case EStereoscopicPass::eSSP_RIGHT_EYE:
		EyeType = EDisplayClusterEyeType::StereoRight;
		break;

	default:
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Couldn't decode eye type. Falling back to type <%d>"), int(EyeType));
		break;
	}

	return EyeType;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IStereoRendering
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterDeviceBase::IsStereoEnabled() const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	return true;
}

bool FDisplayClusterDeviceBase::IsStereoEnabledOnNextFrame() const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	return true;
}

bool FDisplayClusterDeviceBase::EnableStereo(bool stereo /*= true*/)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	return true;
}

void FDisplayClusterDeviceBase::AdjustViewRect(enum EStereoscopicPass StereoPassType, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
}

void FDisplayClusterDeviceBase::CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	check(IsInGameThread());
	check(WorldToMeters > 0.f);

	const int CurrentViewportIndex = DecodeViewportIndex(StereoPassType);
	check(int32(CurrentViewportIndex) < RenderViewports.Num());

	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("OLD ViewLoc: %s, ViewRot: %s"), *ViewLocation.ToString(), *ViewRotation.ToString());
	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("WorldToMeters: %f"), WorldToMeters);

	CurrentWorldToMeters = WorldToMeters;

	FDisplayClusterRenderViewportContext ViewportContext = RenderViewports[CurrentViewportIndex].GetViewportContext();

	// View vector must be orthogonal to the projection plane.
	ViewRotation = ViewportContext.ProjectionScreenData.Rot;

	// What eye we're going to render image for
	const int EyeIndex = DecodeEyeType(StereoPassType);

	const float ScaledEyeDist = EyeDist * CurrentWorldToMeters;
	const float EyeOffset = ScaledEyeDist / 2.f;
	const float EyeOffsetValues[] = { -EyeOffset, 0.f, EyeOffset };
	const float PassOffset = EyeOffsetValues[EyeIndex];
	// Safe for monoscopic since the offset is zero
	const float PassOffsetSwap = (bEyeSwap == true ? -PassOffset : PassOffset);

	// offset eye position along Y (right) axis of camera
	UDisplayClusterCameraComponent* pCamera = GDisplayCluster->GetPrivateGameMgr()->GetActiveCamera();
	if (pCamera)
	{
		const FQuat eyeQuat = pCamera->GetComponentQuat();
		ViewLocation += eyeQuat.RotateVector(FVector(0.0f, PassOffsetSwap, 0.0f));
	}

	ViewportContext.EyeLoc[EyeIndex] = ViewLocation;
	ViewportContext.EyeRot[EyeIndex] = ViewRotation;

	RenderViewports[CurrentViewportIndex].SetViewportContext(ViewportContext);

	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("NEW ViewLoc: %s, ViewRot: %s"), *ViewLocation.ToString(), *ViewRotation.ToString());
}


FMatrix FDisplayClusterDeviceBase::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	check(IsInGameThread());
	check(StereoPassType != EStereoscopicPass::eSSP_FULL);

	const EStereoscopicPass DecodedPass = DecodeStereoscopicPass(StereoPassType);
	const int CurrentViewportIndex = DecodeViewportIndex(StereoPassType);

	const float n = NearClipPlane;
	const float f = FarClipPlane;

	const FDisplayClusterRenderViewportContext ViewportContext = RenderViewports[CurrentViewportIndex].GetViewportContext();
	const FDisplayClusterProjectionScreenData& ScreenData = ViewportContext.ProjectionScreenData;

	// Half-size
	const float hw = ScreenData.Size.X / 2.f * CurrentWorldToMeters;
	const float hh = ScreenData.Size.Y / 2.f * CurrentWorldToMeters;

	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("StereoProjectionMatrix math: hw:%f hh:%f"), hw, hh);

	// Screen corners
	const FVector pa = ScreenData.Loc + ScreenData.Rot.Quaternion().RotateVector(GetProjectionScreenGeometryLBC(hw, hh)); // left bottom corner
	const FVector pb = ScreenData.Loc + ScreenData.Rot.Quaternion().RotateVector(GetProjectionScreenGeometryRBC(hw, hh)); // right bottom corner
	const FVector pc = ScreenData.Loc + ScreenData.Rot.Quaternion().RotateVector(GetProjectionScreenGeometryLTC(hw, hh)); // left top corner

	// Screen vectors
	FVector vr = pb - pa; // lb->rb normilized vector, right axis of projection screen
	vr.Normalize();
	FVector vu = pc - pa; // lb->lt normilized vector, up axis of projection screen
	vu.Normalize();
	FVector vn = -FVector::CrossProduct(vr, vu); // Projection plane normal. Use minus because of left-handed coordinate system
	vn.Normalize();

	const int eyeIdx = DecodeEyeType(StereoPassType);
	const FVector pe = ViewportContext.EyeLoc[eyeIdx];

	const FVector va = pa - pe; // camera -> lb
	const FVector vb = pb - pe; // camera -> rb
	const FVector vc = pc - pe; // camera -> lt

	const float d = -FVector::DotProduct(va, vn); // distance from eye to screen
	const float ndifd = n / d;
	const float l = FVector::DotProduct(vr, va) * ndifd; // distance to left screen edge
	const float r = FVector::DotProduct(vr, vb) * ndifd; // distance to right screen edge
	const float b = FVector::DotProduct(vu, va) * ndifd; // distance to bottom screen edge
	const float t = FVector::DotProduct(vu, vc) * ndifd; // distance to top screen edge

	const float mx = 2.f * n / (r - l);
	const float my = 2.f * n / (t - b);
	const float ma = -(r + l) / (r - l);
	const float mb = -(t + b) / (t - b);
	const float mc = f / (f - n);
	const float md = -(f * n) / (f - n);
	const float me = 1.f;

	// Normal LHS
	const FMatrix pm = FMatrix(
		FPlane(mx, 0, 0, 0),
		FPlane(0, my, 0, 0),
		FPlane(ma, mb, mc, me),
		FPlane(0, 0, md, 0));

	// Invert Z-axis (UE4 uses Z-inverted LHS)
	const FMatrix flipZ = FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, -1, 0),
		FPlane(0, 0, 1, 1));

	const FMatrix result(pm * flipZ);

	return result;
}

void FDisplayClusterDeviceBase::InitCanvasFromView(class FSceneView* InView, class UCanvas* Canvas)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
}

EStereoscopicPass FDisplayClusterDeviceBase::GetViewPassForIndex(bool bStereoRequested, uint32 ViewIndex) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	const int CurrentPass = EncodeStereoscopicPass(ViewIndex);
	const int CurrentViewportIndex = DecodeViewportIndex((EStereoscopicPass)CurrentPass);
	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("CurrentViewportIdx: %d, CurrentPass: %d"), CurrentViewportIndex, (int)CurrentPass);

	// This is a bit tricky but it works
	return (EStereoscopicPass)CurrentPass;
}

uint32 FDisplayClusterDeviceBase::GetViewIndexForPass(EStereoscopicPass StereoPassType) const
{
	uint32 DecodedViewIndex = 0;

	switch (StereoPassType)
	{
	case EStereoscopicPass::eSSP_LEFT_EYE:
		DecodedViewIndex = 0;
		break;

	case EStereoscopicPass::eSSP_RIGHT_EYE:
		DecodedViewIndex = 1;
		break;

	default:
		DecodedViewIndex = (int(StereoPassType) - int(EStereoscopicPass::eSSP_RIGHT_EYE) + 1);
		break;
	}

	return DecodedViewIndex;
}

void FDisplayClusterDeviceBase::UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	check(IsInGameThread());

	// Update projection screen data
	UpdateProjectionDataForThisFrame();

	// Save current dimensions
	ViewportSize = Viewport.GetSizeXY();
	BackBuffSize = Viewport.GetRenderTargetTextureSizeXY();

#if 0
	// If no custom area specified the full viewport area will be used
	if (ViewportArea.IsValid() == false)
	{
		ViewportArea.SetLocation(FIntPoint::ZeroValue);
		ViewportArea.SetSize(Viewport.GetSizeXY());
	}
#endif

	// Store viewport
	if (!MainViewport)
	{
		MainViewport = (FViewport*)&Viewport;
		Viewport.GetViewportRHI()->SetCustomPresent(this);
	}
}

void FDisplayClusterDeviceBase::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	check(IsInGameThread());

	InOutSizeX = Viewport.GetSizeXY().X;
	InOutSizeY = Viewport.GetSizeXY().Y;

	check(InOutSizeX > 0 && InOutSizeY > 0);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FRHICustomPresent
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterDeviceBase::OnBackBufferResize()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	//@todo: see comment below
	// if we are in the middle of rendering: prevent from calling EndFrame
	//if (RenderContext.IsValid())
	//{
	//	RenderContext->bFrameBegun = false;
	//}
}

bool FDisplayClusterDeviceBase::Present(int32& InOutSyncInterval)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	UE_LOG(LogDisplayClusterRender, Warning, TEXT("Present - default handler implementation. Check stereo device instantiation."));

	// Default behavior
	// Return false to force clean screen. This will indicate that something is going wrong
	// or particular stereo device hasn't been implemented appropriately yet.
	return false;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterStereoDevice
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterDeviceBase::AddViewport(const FString& ViewportId, IDisplayClusterProjectionScreenDataProvider* DataProvider)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	FDisplayClusterConfigViewport Viewport;
	if (!GDisplayCluster->GetPrivateConfigMgr()->GetViewport(ViewportId, Viewport))
	{
		return;
	}

	FDisplayClusterViewportArea ViewportArea(Viewport.Loc, Viewport.Size);
	FDisplayClusterRenderViewport NewViewport(Viewport.ScreenId, DataProvider, ViewportArea);

	{
		FScopeLock lock(&InternalsSyncScope);
		RenderViewports.Add(NewViewport);
	}
}

void FDisplayClusterDeviceBase::RemoveViewport(const FString& ViewportId)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	{
		FScopeLock lock(&InternalsSyncScope);
		RenderViewports.RemoveAll([&ViewportId](const FDisplayClusterRenderViewport& Viewport)
		{
			return Viewport.GetProjectionScreenId() == ViewportId;
		});
	}
}

void FDisplayClusterDeviceBase::RemoveAllViewports()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	{
		FScopeLock lock(&InternalsSyncScope);
		RenderViewports.Reset();
	}
}

void FDisplayClusterDeviceBase::SetDesktopStereoParams(float FOV)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	UE_LOG(LogDisplayClusterRender, Log, TEXT("SetDesktopStereoParams: FOV=%f"), FOV);
	//@todo
}

void FDisplayClusterDeviceBase::SetDesktopStereoParams(const FVector2D& screenSize, const FIntPoint& screenRes, float screenDist)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	UE_LOG(LogDisplayClusterRender, Log, TEXT("SetDesktopStereoParams"));

	FVector2D size = screenSize;
	float dist = screenDist;

	//@todo:
}

void FDisplayClusterDeviceBase::SetInterpupillaryDistance(float dist)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	UE_LOG(LogDisplayClusterRender, Log, TEXT("SetInterpupillaryDistance: %f"), dist);
	FScopeLock lock(&InternalsSyncScope);
	EyeDist = dist;
}

float FDisplayClusterDeviceBase::GetInterpupillaryDistance() const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("GetInterpupillaryDistance: %f"), EyeDist);
	FScopeLock lock(&InternalsSyncScope);
	return EyeDist;
}

void FDisplayClusterDeviceBase::SetEyesSwap(bool swap)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	UE_LOG(LogDisplayClusterRender, Log, TEXT("SetEyesSwap: %s"), DisplayClusterHelpers::str::BoolToStr(swap));
	FScopeLock lock(&InternalsSyncScope);
	bEyeSwap = swap;
}

bool FDisplayClusterDeviceBase::GetEyesSwap() const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("GetEyesSwap: %s"), DisplayClusterHelpers::str::BoolToStr(bEyeSwap));
	FScopeLock lock(&InternalsSyncScope);
	return bEyeSwap;
}

bool FDisplayClusterDeviceBase::ToggleEyesSwap()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	{
		FScopeLock lock(&InternalsSyncScope);
		bEyeSwap = !bEyeSwap;
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("ToggleEyesSwap: swap=%s"), DisplayClusterHelpers::str::BoolToStr(bEyeSwap));
	return bEyeSwap;
}

void FDisplayClusterDeviceBase::SetSwapSyncPolicy(EDisplayClusterSwapSyncPolicy policy)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Swap sync policy: %d"), (int)policy);

	{
		FScopeLock lock(&InternalsSyncScope);
		SwapSyncPolicy = EDisplayClusterSwapSyncPolicy::None;
	}
}

EDisplayClusterSwapSyncPolicy FDisplayClusterDeviceBase::GetSwapSyncPolicy() const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	EDisplayClusterSwapSyncPolicy CurrentSwapSyncPolicy;
	
	{
		FScopeLock lock(&InternalsSyncScope);
		CurrentSwapSyncPolicy = SwapSyncPolicy;
	}

	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("GetSwapSyncPolicy: policy=%d"), (int)CurrentSwapSyncPolicy);
	return CurrentSwapSyncPolicy;
}

void FDisplayClusterDeviceBase::GetCullingDistance(float& NearDistance, float& FarDistance) const
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	
	{
		FScopeLock lock(&InternalsSyncScope);
		NearDistance = NearClipPlane;
		FarDistance = FarClipPlane;
	}
}

void FDisplayClusterDeviceBase::SetCullingDistance(float NearDistance, float FarDistance)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
	UE_LOG(LogDisplayClusterRender, Log, TEXT("New culling distance: NCP=%f, FCP=%f"), NearDistance, FarDistance);

	{
		FScopeLock lock(&InternalsSyncScope);
		NearClipPlane = NearDistance;
		FarClipPlane = FarDistance;
	}
}
