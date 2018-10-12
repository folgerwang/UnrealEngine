// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDeviceBase.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Cluster/Controller/IPDisplayClusterNodeController.h"
#include "Config/IPDisplayClusterConfigManager.h"
#include "Game/IPDisplayClusterGameManager.h"

#include "DisplayClusterScreenComponent.h"

#include "RHIStaticStates.h"
#include "Slate/SceneViewport.h"

#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"

#include "DisplayClusterGlobals.h"
#include "IPDisplayCluster.h"

#include <utility>


FDisplayClusterDeviceBase::FDisplayClusterDeviceBase() :
	FRHICustomPresent()
{
	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT(".ctor FDisplayClusterDeviceBase"));
}

FDisplayClusterDeviceBase::~FDisplayClusterDeviceBase()
{
	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT(".dtor FDisplayClusterDeviceBase"));
}

bool FDisplayClusterDeviceBase::Initialize()
{
	if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled)
	{
		return false;
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Use swap interval: %d"), SwapInterval);

	return true;
}

void FDisplayClusterDeviceBase::WaitForBufferSwapSync(int32& InOutSyncInterval)
{
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

void FDisplayClusterDeviceBase::UpdateProjectionScreenDataForThisFrame()
{
	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("UpdateProjectionScreenDataForThisFrame"));
	check(IsInGameThread());

	if (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled)
	{
		return;
	}

	// Store transformations of active projection screen
	UDisplayClusterScreenComponent* pScreen = GDisplayCluster->GetPrivateGameMgr()->GetActiveScreen();
	if (pScreen)
	{
		ProjectionScreenLoc  = pScreen->GetComponentLocation();
		ProjectionScreenRot  = pScreen->GetComponentRotation();
		ProjectionScreenSize = pScreen->GetScreenSize();
	}
}

void FDisplayClusterDeviceBase::exec_BarrierWait()
{
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

//////////////////////////////////////////////////////////////////////////////////////////////
// IStereoRendering
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterDeviceBase::IsStereoEnabled() const
{
	//UE_LOG(LogDisplayClusterRender, Verbose, TEXT("IsStereoEnabled"));
	return true;
}

bool FDisplayClusterDeviceBase::IsStereoEnabledOnNextFrame() const
{
	//UE_LOG(LogDisplayClusterRender, Verbose, TEXT("IsStereoEnabledOnNextFrame"));
	return true;
}

bool FDisplayClusterDeviceBase::EnableStereo(bool stereo /*= true*/)
{
	//UE_LOG(LogDisplayClusterRender, Verbose, TEXT("EnableStereo"));
	return true;
}

void FDisplayClusterDeviceBase::AdjustViewRect(enum EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	X = ViewportArea.GetLocation().X;
	SizeX = ViewportArea.GetSize().X;

	Y = ViewportArea.GetLocation().Y;
	SizeY = ViewportArea.GetSize().Y;
}

void FDisplayClusterDeviceBase::CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	//UE_LOG(LogDisplayClusterRender, Verbose, TEXT("CalculateStereoViewOffset"));
	
	check(IsInGameThread());
	check(WorldToMeters > 0.f);

	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("OLD ViewLoc: %s, ViewRot: %s"), *ViewLocation.ToString(), *ViewRotation.ToString());
	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("WorldToMeters: %f"), WorldToMeters);

	CurrentWorldToMeters = WorldToMeters;

	// View vector must be orthogonal to the projection plane.
	ViewRotation = ProjectionScreenRot;

	const float ScaledEyeDist = EyeDist * CurrentWorldToMeters;
	const float EyeOffset = ScaledEyeDist / 2.f;
	const float PassOffset = (StereoPassType == EStereoscopicPass::eSSP_LEFT_EYE ? -EyeOffset : EyeOffset);
	const float PassOffsetSwap = (bEyeSwap == true ? -PassOffset : PassOffset);

	// offset eye position along Y (right) axis of camera
	UDisplayClusterCameraComponent* pCamera = GDisplayCluster->GetPrivateGameMgr()->GetActiveCamera();
	if(pCamera)
	{
		const FQuat eyeQuat = pCamera->GetComponentQuat();
		ViewLocation += eyeQuat.RotateVector(FVector(0.0f, PassOffsetSwap, 0.0f));
	}

	const int eyeIdx = (StereoPassType == EStereoscopicPass::eSSP_LEFT_EYE ? 0 : 1);
	EyeLoc[eyeIdx] = ViewLocation;
	EyeRot[eyeIdx] = ViewRotation;

	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("NEW ViewLoc: %s, ViewRot: %s"), *ViewLocation.ToString(), *ViewRotation.ToString());
}


FMatrix FDisplayClusterDeviceBase::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const
{
	//UE_LOG(LogDisplayClusterRender, Verbose, TEXT("GetStereoProjectionMatrix"));
	
	check(IsInGameThread());
	check(StereoPassType != EStereoscopicPass::eSSP_FULL);
	
	const float n = NearClipPlane;
	const float f = FarClipPlane;

	// Half-size
	const float hw = ProjectionScreenSize.X / 2.f * CurrentWorldToMeters;
	const float hh = ProjectionScreenSize.Y / 2.f * CurrentWorldToMeters;

	UE_LOG(LogDisplayClusterRender, VeryVerbose, TEXT("StereoProjectionMatrix math: hw:%f hh:%f"), hw, hh);

	// Screen corners
	const FVector pa = ProjectionScreenLoc + ProjectionScreenRot.Quaternion().RotateVector(GetProjectionScreenGeometryLBC(StereoPassType, hw, hh)); // left bottom corner
	const FVector pb = ProjectionScreenLoc + ProjectionScreenRot.Quaternion().RotateVector(GetProjectionScreenGeometryRBC(StereoPassType, hw, hh)); // right bottom corner
	const FVector pc = ProjectionScreenLoc + ProjectionScreenRot.Quaternion().RotateVector(GetProjectionScreenGeometryLTC(StereoPassType, hw, hh)); // left top corner

	// Screen vectors
	FVector vr = pb - pa; // lb->rb normilized vector, right axis of projection screen
	vr.Normalize();
	FVector vu = pc - pa; // lb->lt normilized vector, up axis of projection screen
	vu.Normalize();
	FVector vn = -FVector::CrossProduct(vr, vu); // Projection plane normal. Use minus because of left-handed coordinate system
	vn.Normalize();

	const int eyeIdx = (StereoPassType == EStereoscopicPass::eSSP_LEFT_EYE ? 0 : 1);
	const FVector pe = EyeLoc[eyeIdx];
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
		FPlane(1, 0,  0, 0),
		FPlane(0, 1,  0, 0),
		FPlane(0, 0, -1, 0),
		FPlane(0, 0,  1, 1));

	const FMatrix result(pm * flipZ);

	return result;
}

void FDisplayClusterDeviceBase::InitCanvasFromView(class FSceneView* InView, class UCanvas* Canvas)
{
	//UE_LOG(LogDisplayClusterRender, Verbose, TEXT("InitCanvasFromView"));
}

void FDisplayClusterDeviceBase::UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget)
{
	//UE_LOG(LogDisplayClusterRender, Verbose, TEXT("UpdateViewport"));
	check(IsInGameThread());

	// Update projection screen data
	UpdateProjectionScreenDataForThisFrame();

	// Save current dimensions
	ViewportSize = Viewport.GetSizeXY();
	BackBuffSize = Viewport.GetRenderTargetTextureSizeXY();

	// If no custom area specified the full viewport area will be used
	if (ViewportArea.IsValid() == false)
	{
		ViewportArea.SetLocation(FIntPoint::ZeroValue);
		ViewportArea.SetSize(Viewport.GetSizeXY());
	}

	// Store viewport
	CurrentViewport = (FViewport*)&Viewport;
	Viewport.GetViewportRHI()->SetCustomPresent(this);
}

void FDisplayClusterDeviceBase::CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY)
{
	//UE_LOG(LogDisplayClusterRender, Log, TEXT("FDisplayClusterDeviceBase::CalculateRenderTargetSize"));
	check(IsInGameThread());

	InOutSizeX = Viewport.GetSizeXY().X;
	// Add one pixel height line for right eye (will be skipped on copy)
	InOutSizeY = Viewport.GetSizeXY().Y;

	check(InOutSizeX > 0 && InOutSizeY > 0);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FRHICustomPresent
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterDeviceBase::OnBackBufferResize()
{
	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("OnBackBufferResize"));

	//@todo: see comment below
	// if we are in the middle of rendering: prevent from calling EndFrame
	//if (RenderContext.IsValid())
	//{
	//	RenderContext->bFrameBegun = false;
	//}
}

bool FDisplayClusterDeviceBase::Present(int32& InOutSyncInterval)
{
	UE_LOG(LogDisplayClusterRender, Warning, TEXT("Present - default handler implementation. Check stereo device instantiation."));

	// Default behavior
	// Return false to force clean screen. This will indicate that something is going wrong
	// or particular stereo device hasn't been implemented appropriately yet.
	return false;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterStereoDevice
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterDeviceBase::SetViewportArea(const FIntPoint& loc, const FIntPoint& size)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("SetViewportArea: loc=%s size=%s"), *loc.ToString(), *size.ToString());

	FScopeLock lock(&InternalsSyncScope);
	ViewportArea.SetLocation(loc);
	ViewportArea.SetSize(size);
}

void FDisplayClusterDeviceBase::SetDesktopStereoParams(float FOV)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("SetDesktopStereoParams: FOV=%f"), FOV);
	//@todo
}

void FDisplayClusterDeviceBase::SetDesktopStereoParams(const FVector2D& screenSize, const FIntPoint& screenRes, float screenDist)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("SetDesktopStereoParams"));

	FVector2D size = screenSize;
	float dist = screenDist;

	//@todo:
}

void FDisplayClusterDeviceBase::SetInterpupillaryDistance(float dist)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("SetInterpupillaryDistance: %f"), dist);
	FScopeLock lock(&InternalsSyncScope);
	EyeDist = dist;
}

float FDisplayClusterDeviceBase::GetInterpupillaryDistance() const
{
	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("GetInterpupillaryDistance: %f"), EyeDist);
	FScopeLock lock(&InternalsSyncScope);
	return EyeDist;
}

void FDisplayClusterDeviceBase::SetEyesSwap(bool swap)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("SetEyesSwap: %s"), DisplayClusterHelpers::str::BoolToStr(swap));
	FScopeLock lock(&InternalsSyncScope);
	bEyeSwap = swap;
}

bool FDisplayClusterDeviceBase::GetEyesSwap() const
{
	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("GetEyesSwap: %s"), DisplayClusterHelpers::str::BoolToStr(bEyeSwap));
	FScopeLock lock(&InternalsSyncScope);
	return bEyeSwap;
}

bool FDisplayClusterDeviceBase::ToggleEyesSwap()
{
	{
		FScopeLock lock(&InternalsSyncScope);
		bEyeSwap = !bEyeSwap;
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("ToggleEyesSwap: swap=%s"), DisplayClusterHelpers::str::BoolToStr(bEyeSwap));
	return bEyeSwap;
}

void FDisplayClusterDeviceBase::SetSwapSyncPolicy(EDisplayClusterSwapSyncPolicy policy)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("Swap sync policy: %d"), (int)policy);
	
	// Since not all our devices are opengl compatible in terms of implementation
	// we have to perform some wrapping logic for the policies.
	switch (policy)
	{
		// Policies below are available for any render device type
		case EDisplayClusterSwapSyncPolicy::None:
			SwapSyncPolicy = policy;
			break;

		default:
			UE_LOG(LogDisplayClusterRender, Error, TEXT("Unsupported policy type: %d"), (int)policy);
			SwapSyncPolicy = EDisplayClusterSwapSyncPolicy::None;
			break;
	}
}

EDisplayClusterSwapSyncPolicy FDisplayClusterDeviceBase::GetSwapSyncPolicy() const
{
	UE_LOG(LogDisplayClusterRender, Verbose, TEXT("GetSwapSyncPolicy: policy=%d"), (int)SwapSyncPolicy);
	return SwapSyncPolicy;
}

void FDisplayClusterDeviceBase::GetCullingDistance(float& NearDistance, float& FarDistance) const
{
	FScopeLock lock(&InternalsSyncScope);
	NearDistance = NearClipPlane;
	FarDistance = FarClipPlane;
}

void FDisplayClusterDeviceBase::SetCullingDistance(float NearDistance, float FarDistance)
{
	UE_LOG(LogDisplayClusterRender, Log, TEXT("New culling distance: NCP=%f, FCP=%f"), NearDistance, FarDistance);

	FScopeLock lock(&InternalsSyncScope);
	NearClipPlane = NearDistance;
	FarClipPlane = FarDistance;
}
