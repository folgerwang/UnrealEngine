// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Containers/StaticBitArray.h"
#include "IAnselPlugin.h"
#include "Camera/CameraTypes.h"
#include "Camera/CameraPhotography.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/ConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/ViewportSplitScreen.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SWindow.h"
#include "Application/SlateApplicationBase.h"
#include "RenderResource.h"

#include <functional>

#include "AnselFunctionLibrary.h"
#include <AnselSDK.h>

DEFINE_LOG_CATEGORY_STATIC(LogAnsel, Log, All);

#define LOCTEXT_NAMESPACE "Photography"

static TAutoConsoleVariable<int32> CVarAllowHighQuality(
	TEXT("r.Photography.AllowHighQuality"),
	1,
	TEXT("Whether to permit Ansel RT (high-quality mode).\n"),
	ECVF_RenderThreadSafe);

// intentionally undocumented until tested further
static TAutoConsoleVariable<int32> CVarExtreme(
	TEXT("r.Photography.Extreme"),
	0,
	TEXT("Whether to allow 'extreme' quality for Ansel RT (EXPERIMENTAL).\n"),
	ECVF_RenderThreadSafe);

// intentionally undocumented - debug flag
static TAutoConsoleVariable<int32> CVarDebug0(
	TEXT("r.Photography.Debug0"),
	0,
	TEXT("Debug - kill RT when in high-quality(!) mode\n"),
	ECVF_RenderThreadSafe);

/////////////////////////////////////////////////
// All the NVIDIA Ansel-specific details

class FNVAnselCameraPhotographyPrivate : public ICameraPhotography
{
public:
	FNVAnselCameraPhotographyPrivate();
	virtual ~FNVAnselCameraPhotographyPrivate() override;
	virtual bool UpdateCamera(FMinimalViewInfo& InOutPOV, APlayerCameraManager* PCMgr) override;
	virtual void UpdatePostProcessing(FPostProcessSettings& InOutPostProcessSettings) override;
	virtual void StartSession() override;
	virtual void StopSession() override;
	virtual bool IsSupported() override;
	virtual void SetUIControlVisibility(uint8 UIControlTarget, bool bIsVisible) override;
	virtual void DefaultConstrainCamera(const FVector NewCameraLocation, const FVector PreviousCameraLocation, const FVector OriginalCameraLocation, FVector& OutCameraLocation, APlayerCameraManager* PCMgr) override;
	virtual const TCHAR* const GetProviderName() override { return TEXT("NVIDIA Ansel"); };

	enum econtrols {
		control_dofscale,
		control_dofsensorwidth,
		control_doffocalregion,
		control_doffocaldistance,
		control_dofdepthbluramount,
		control_dofdepthblurradius,
		control_bloomintensity,
		control_bloomscale,
		control_scenefringeintensity,
		control_COUNT
	};
	typedef union {
		bool bool_val;
		float float_val;
	} ansel_control_val;

private:
	void ReconfigureAnsel();
	void DeconfigureAnsel();

	static ansel::StartSessionStatus AnselStartSessionCallback(ansel::SessionConfiguration& settings, void* userPointer);
	static void AnselStopSessionCallback(void* userPointer);
	static void AnselStartCaptureCallback(const ansel::CaptureConfiguration& CaptureInfo, void* userPointer);
	static void AnselStopCaptureCallback(void* userPointer);
	static void AnselChangeQualityCallback(bool isHighQuality, void* userPointer);

	static bool AnselCamerasMatch(ansel::Camera& a, ansel::Camera& b);

	void AnselCameraToFMinimalView(FMinimalViewInfo& InOutPOV, ansel::Camera& AnselCam);
	void FMinimalViewToAnselCamera(ansel::Camera& InOutAnselCam, FMinimalViewInfo& POV);

	bool BlueprintModifyCamera(ansel::Camera& InOutAnselCam, APlayerCameraManager* PCMgr); // returns whether modified cam is in original (session-start) position

	void ConfigureRenderingSettingsForPhotography(FPostProcessSettings& InOutPostProcessSettings);
	void SetUpSessionCVars();
	void DoCustomUIControls(FPostProcessSettings& InOutPPSettings, bool bRebuildControls);
	void DeclareSlider(int id, FText LocTextLabel, float LowerBound, float UpperBound, float Val);
	bool ProcessUISlider(int id, float& InOutVal);

	bool CaptureCVar(FString CVarName);
	void SetCapturedCVarPredicated(const char* CVarName, float valueIfNotReset, std::function<bool(const float, const float)> comparison, bool wantReset, bool useExistingPriority);
	void SetCapturedCVar(const char* CVarName, float valueIfNotReset, bool wantReset = false, bool useExistingPriority = false);

	ansel::Configuration* AnselConfig;
	ansel::Camera AnselCamera;
	ansel::Camera AnselCameraOriginal;
	ansel::Camera AnselCameraPrevious;

	FMinimalViewInfo UECameraOriginal;
	FMinimalViewInfo UECameraPrevious;

	FPostProcessSettings UEPostProcessingOriginal;

	bool bAnselSessionActive;
	bool bAnselSessionNewlyActive;
	bool bAnselSessionWantDeactivate;
	bool bAnselCaptureActive;
	bool bAnselCaptureNewlyActive;
	bool bAnselCaptureNewlyFinished;
	ansel::CaptureConfiguration AnselCaptureInfo;

	bool bForceDisallow;
	bool bIsOrthoProjection;

	bool bWasMovableCameraBeforeSession;
	bool bWasPausedBeforeSession;
	bool bWasShowingHUDBeforeSession;
	bool bWereSubtitlesEnabledBeforeSession;
	bool bWasFadingEnabledBeforeSession;
	bool bWasScreenMessagesEnabledBeforeSession = false;
	float fTimeDilationBeforeSession;

	bool bCameraIsInOriginalState = true;

	bool bAutoPostprocess;
	bool bAutoPause;
	bool bRayTracingEnabled = false;
	bool bPausedInternally = false;

	bool bHighQualityModeDesired = false;
	bool bHighQualityModeIsSetup = false;

	uint32_t NumFramesSinceSessionStart;

	// members relating to the 'Game Settings' controls in the Ansel overlay UI
	TStaticBitArray<256> bEffectUIAllowed;

	bool bUIControlsNeedRebuild;
	ansel::UserControlDesc UIControls[control_COUNT];
	static ansel_control_val UIControlValues[control_COUNT]; // static to allow access from a callback
	float UIControlRangeLower[control_COUNT];
	float UIControlRangeUpper[control_COUNT];

	/** Console variable delegate for checking when the console variables have changed */
	FConsoleCommandDelegate CVarDelegate;
	FConsoleVariableSinkHandle CVarDelegateHandle;

	struct CVarInfo {
		IConsoleVariable* cvar;
		float fInitialVal;
	};
	TMap<FString, CVarInfo> InitialCVarMap;
};

FNVAnselCameraPhotographyPrivate::ansel_control_val FNVAnselCameraPhotographyPrivate::UIControlValues[control_COUNT];

static void* AnselSDKDLLHandle = 0;
static bool bAnselDLLLoaded = false;

bool FNVAnselCameraPhotographyPrivate::CaptureCVar(FString CVarName)
{
	IConsoleVariable* cvar = IConsoleManager::Get().FindConsoleVariable(CVarName.GetCharArray().GetData());
	if (!cvar) return false;

	CVarInfo info;
	info.cvar = cvar;
	info.fInitialVal = cvar->GetFloat();

	InitialCVarMap.Add(CVarName, info);
	return true;
}

FNVAnselCameraPhotographyPrivate::FNVAnselCameraPhotographyPrivate()
	: ICameraPhotography()
	, bAnselSessionActive(false)
	, bAnselSessionNewlyActive(false)
	, bAnselSessionWantDeactivate(false)
	, bAnselCaptureActive(false)
	, bAnselCaptureNewlyActive(false)
	, bAnselCaptureNewlyFinished(false)
	, bForceDisallow(false)
	, bIsOrthoProjection(false)
{
	for (int i = 0; i < bEffectUIAllowed.Num(); ++i)
	{
		bEffectUIAllowed[i] = true; // allow until explicitly disallowed
	}

	if (bAnselDLLLoaded)
	{
		AnselConfig = new ansel::Configuration();

		CVarDelegate = FConsoleCommandDelegate::CreateLambda([this] {
			static float LastTranslationSpeed = -1.0f;
			static int32 LastSettleFrames = -1;
			
			static IConsoleVariable* CVarTranslationSpeed = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.TranslationSpeed"));
			static IConsoleVariable* CVarSettleFrames = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.SettleFrames"));
			
			float ThisTranslationSpeed = CVarTranslationSpeed->GetFloat();
			int32 ThisSettleFrames = CVarSettleFrames->GetInt();

			if (ThisTranslationSpeed != LastTranslationSpeed ||
				ThisSettleFrames != LastSettleFrames)
			{
				ReconfigureAnsel();
				LastTranslationSpeed = ThisTranslationSpeed;
				LastSettleFrames = ThisSettleFrames;
			}
		});

		CVarDelegateHandle = IConsoleManager::Get().RegisterConsoleVariableSink_Handle(CVarDelegate);
		ReconfigureAnsel();
	}
	else
	{
		UE_LOG(LogAnsel, Log, TEXT("Ansel DLL was not successfully loaded."));
	}
}


FNVAnselCameraPhotographyPrivate::~FNVAnselCameraPhotographyPrivate()
{	
	if (bAnselDLLLoaded)
	{
		IConsoleManager::Get().UnregisterConsoleVariableSink_Handle(CVarDelegateHandle);
		DeconfigureAnsel();
		delete AnselConfig;
	}
}


bool FNVAnselCameraPhotographyPrivate::IsSupported()
{
	return bAnselDLLLoaded && ansel::isAnselAvailable();
}

void FNVAnselCameraPhotographyPrivate::SetUIControlVisibility(uint8 UIControlTarget, bool bIsVisible)
{
	bEffectUIAllowed[UIControlTarget] = bIsVisible;
}

bool FNVAnselCameraPhotographyPrivate::AnselCamerasMatch(ansel::Camera& a, ansel::Camera& b)
{
	return a.position.x == b.position.x &&
		a.position.y == b.position.y &&
		a.position.z == b.position.z &&
		a.rotation.x == b.rotation.x &&
		a.rotation.y == b.rotation.y &&
		a.rotation.z == b.rotation.z &&
		a.rotation.w == b.rotation.w &&
		a.fov == b.fov &&
		a.projectionOffsetX == b.projectionOffsetX &&
		a.projectionOffsetY == b.projectionOffsetY;
}

void FNVAnselCameraPhotographyPrivate::AnselCameraToFMinimalView(FMinimalViewInfo& InOutPOV, ansel::Camera& AnselCam)
{
	InOutPOV.FOV = AnselCam.fov;
	InOutPOV.Location.X = AnselCam.position.x;
	InOutPOV.Location.Y = AnselCam.position.y;
	InOutPOV.Location.Z = AnselCam.position.z;
	FQuat rotq(AnselCam.rotation.x, AnselCam.rotation.y, AnselCam.rotation.z, AnselCam.rotation.w);
	InOutPOV.Rotation = FRotator(rotq);
	InOutPOV.OffCenterProjectionOffset.Set(AnselCam.projectionOffsetX, AnselCam.projectionOffsetY);
}

void FNVAnselCameraPhotographyPrivate::FMinimalViewToAnselCamera(ansel::Camera& InOutAnselCam, FMinimalViewInfo& POV)
{
	InOutAnselCam.fov = POV.FOV;
	InOutAnselCam.position = { POV.Location.X, POV.Location.Y, POV.Location.Z };
	FQuat rotq = POV.Rotation.Quaternion();
	InOutAnselCam.rotation = { rotq.X, rotq.Y, rotq.Z, rotq.W };
	InOutAnselCam.projectionOffsetX = 0.f; // Ansel only writes these, doesn't read
	InOutAnselCam.projectionOffsetY = 0.f;
}

bool FNVAnselCameraPhotographyPrivate::BlueprintModifyCamera(ansel::Camera& InOutAnselCam, APlayerCameraManager* PCMgr)
{
	FMinimalViewInfo Proposed;

	AnselCameraToFMinimalView(Proposed, InOutAnselCam);
	PCMgr->PhotographyCameraModify(Proposed.Location, UECameraPrevious.Location, UECameraOriginal.Location, Proposed.Location/*out by ref*/);
	// only position has possibly changed
	InOutAnselCam.position.x = Proposed.Location.X;
	InOutAnselCam.position.y = Proposed.Location.Y;
	InOutAnselCam.position.z = Proposed.Location.Z;

	UECameraPrevious = Proposed;

	bool bIsCameraInOriginalTransform =
		Proposed.Location.Equals(UECameraOriginal.Location) &&
		Proposed.Rotation.Equals(UECameraOriginal.Rotation) &&
		Proposed.FOV == UECameraOriginal.FOV;
	return bIsCameraInOriginalTransform;
}

void FNVAnselCameraPhotographyPrivate::DeclareSlider(int id, FText LocTextLabel, float LowerBound, float UpperBound, float Val)
{
	UIControlRangeLower[id] = LowerBound;
	UIControlRangeUpper[id] = UpperBound;

	UIControls[id].labelUtf8 = TCHAR_TO_UTF8(LocTextLabel.ToString().GetCharArray().GetData());
	UIControlValues[id].float_val = FMath::GetRangePct(LowerBound, UpperBound, Val);

	UIControls[id].callback = [](const ansel::UserControlInfo& info) {
		UIControlValues[info.userControlId - 1].float_val = *(float*)info.value;
	};
	UIControls[id].info.userControlId = id + 1; // reserve 0 as 'unused'
	UIControls[id].info.userControlType = ansel::kUserControlSlider;
	UIControls[id].info.value = &UIControlValues[id].float_val;

	ansel::UserControlStatus status = ansel::addUserControl(UIControls[id]);
	UE_LOG(LogAnsel, Log, TEXT("control#%d status=%d"), (int)id, (int)status);
}

bool FNVAnselCameraPhotographyPrivate::ProcessUISlider(int id, float& InOutVal)
{
	if (UIControls[id].info.userControlId <= 0)
	{
		return false; // control is not in use
	}

	InOutVal = FMath::Lerp(UIControlRangeLower[id], UIControlRangeUpper[id], UIControlValues[id].float_val);
	return true;
}

void FNVAnselCameraPhotographyPrivate::DoCustomUIControls(FPostProcessSettings& InOutPPSettings, bool bRebuildControls)
{
	if (bRebuildControls)
	{
		// clear existing controls
		for (int i = 0; i < control_COUNT; ++i)
		{
			if (UIControls[i].info.userControlId > 0) // we are using id 0 as 'unused'
			{
				ansel::removeUserControl(UIControls[i].info.userControlId);
				UIControls[i].info.userControlId = 0;
			}
		}

		// save postproc settings at session start
		UEPostProcessingOriginal = InOutPPSettings;

		// add all relevant controls
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (bEffectUIAllowed[DepthOfField])
		{
			bool bAnyDofVisible =
				(InOutPPSettings.DepthOfFieldMethod == DOFM_CircleDOF && InOutPPSettings.DepthOfFieldDepthBlurRadius > 0.f) ||
				(InOutPPSettings.DepthOfFieldMethod == DOFM_CircleDOF && InOutPPSettings.DepthOfFieldDepthBlurAmount > 0.f) ||
				(InOutPPSettings.DepthOfFieldMethod == DOFM_BokehDOF && InOutPPSettings.DepthOfFieldScale > 0.f)
				;

			if (bAnyDofVisible)
			{
				if (InOutPPSettings.DepthOfFieldMethod == DOFM_BokehDOF)
				{
					DeclareSlider(
						control_dofscale,
						LOCTEXT("control_dofscale", "Focus Scale"),
						0.f, 2.f,
						InOutPPSettings.DepthOfFieldScale
					);

					DeclareSlider(
						control_doffocalregion,
						LOCTEXT("control_doffocalregion", "Focus Region"),
						0.f, 10000.f, // UU
						InOutPPSettings.DepthOfFieldFocalRegion
					);
				}

				DeclareSlider(
					control_dofsensorwidth,
					LOCTEXT("control_dofsensorwidth", "Focus Sensor"), // n.b. similar effect to focus scale
					0.1f, 1000.f,
					InOutPPSettings.DepthOfFieldSensorWidth
				);

				DeclareSlider(
					control_doffocaldistance,
					LOCTEXT("control_doffocaldistance", "Focus Distance"),
					0.f, 1000.f, // UU - doc'd to 10000U but that's too coarse for a narrow UI control
					InOutPPSettings.DepthOfFieldFocalDistance
				);

				if (InOutPPSettings.DepthOfFieldMethod == DOFM_CircleDOF)
				{
					// circledof
					DeclareSlider(
						control_dofdepthbluramount,
						LOCTEXT("control_dofbluramount", "Blur Distance km"),
						0.000001f, 1.f, // km; doc'd as up to 100km but that's too coarse for a narrow UI control
						InOutPPSettings.DepthOfFieldDepthBlurAmount
					);
					// circledof
					DeclareSlider(
						control_dofdepthblurradius,
						LOCTEXT("control_dofblurradius", "Blur Radius"),
						0.f, 4.f,
						InOutPPSettings.DepthOfFieldDepthBlurRadius
					);
				}
			}
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		if (bEffectUIAllowed[Bloom] &&
			InOutPPSettings.BloomIntensity > 0.f)
		{
			DeclareSlider(
				control_bloomintensity,
				LOCTEXT("control_bloomintensity", "Bloom Intensity"),
				0.f, 8.f,
				InOutPPSettings.BloomIntensity
			);
			DeclareSlider(
				control_bloomscale,
				LOCTEXT("control_bloomscale", "Bloom Scale"),
				0.f, 64.f,
				InOutPPSettings.BloomSizeScale
			);
		}

		if (bEffectUIAllowed[ChromaticAberration] &&
			InOutPPSettings.SceneFringeIntensity > 0.f)
		{
			DeclareSlider(
				control_scenefringeintensity,
				LOCTEXT("control_chromaticaberration", "Chromatic Aberration"),
				0.f, 15.f, // note: FPostProcesssSettings metadata says range is 0./5. but larger values have been seen in the wild 
				InOutPPSettings.SceneFringeIntensity
			);
		}

		bUIControlsNeedRebuild = false;
	}

	// postprocessing is based upon postprocessing settings at session start time (avoids set of
	// UI tweakables changing due to the camera wandering between postprocessing volumes, also
	// avoids most discontinuities where stereo and panoramic captures can also wander between
	// postprocessing volumes during the capture process)
	InOutPPSettings = UEPostProcessingOriginal;

	// update values where corresponding controls are in use
	if (ProcessUISlider(control_dofscale, InOutPPSettings.DepthOfFieldScale))
	{
		InOutPPSettings.bOverride_DepthOfFieldScale = 1;
	}
	if (ProcessUISlider(control_doffocalregion, InOutPPSettings.DepthOfFieldFocalRegion))
	{
		InOutPPSettings.bOverride_DepthOfFieldFocalRegion = 1;
	}
	if (ProcessUISlider(control_dofsensorwidth, InOutPPSettings.DepthOfFieldSensorWidth))
	{
		InOutPPSettings.bOverride_DepthOfFieldSensorWidth = 1;
	}
	if (ProcessUISlider(control_doffocaldistance, InOutPPSettings.DepthOfFieldFocalDistance))
	{
		InOutPPSettings.bOverride_DepthOfFieldFocalDistance = 1;
	}
	if (ProcessUISlider(control_dofdepthbluramount, InOutPPSettings.DepthOfFieldDepthBlurAmount))
	{
		InOutPPSettings.bOverride_DepthOfFieldDepthBlurAmount = 1;
	}
	if (ProcessUISlider(control_dofdepthblurradius, InOutPPSettings.DepthOfFieldDepthBlurRadius))
	{
		InOutPPSettings.bOverride_DepthOfFieldDepthBlurRadius = 1;
	}
	if (ProcessUISlider(control_bloomintensity, InOutPPSettings.BloomIntensity))
	{
		InOutPPSettings.bOverride_BloomIntensity = 1;
	}
	if (ProcessUISlider(control_bloomscale, InOutPPSettings.BloomSizeScale))
	{
		InOutPPSettings.bOverride_BloomSizeScale = 1;
	}
	if (ProcessUISlider(control_scenefringeintensity, InOutPPSettings.SceneFringeIntensity))
	{
		InOutPPSettings.bOverride_SceneFringeIntensity = 1;
	}
}

bool FNVAnselCameraPhotographyPrivate::UpdateCamera(FMinimalViewInfo& InOutPOV, APlayerCameraManager* PCMgr)
{
	check(PCMgr != nullptr);
	bool bGameCameraCutThisFrame = false;

	bForceDisallow = false;
	if (!bAnselSessionActive)
	{
		// grab & store some view details that effect Ansel session setup but which it could be
		// unsafe to access from the Ansel callbacks (which aren't necessarily on render
		// or game thread).
		bIsOrthoProjection = (InOutPOV.ProjectionMode == ECameraProjectionMode::Orthographic);
		if (UGameViewportClient* ViewportClient = PCMgr->GetWorld()->GetGameViewport())
		{
			bForceDisallow = bForceDisallow || (ViewportClient->GetCurrentSplitscreenConfiguration() != ESplitScreenType::None); // forbid if in splitscreen.
		}
		// forbid if in stereoscopic/VR mode
		bForceDisallow = bForceDisallow || (GEngine->IsStereoscopic3D());
	}

	if (bAnselSessionActive)
	{
		APlayerController* PCOwner = PCMgr->GetOwningPlayerController();
		check(PCOwner != nullptr);

		++NumFramesSinceSessionStart;

		if (bAnselCaptureNewlyActive)
		{
			PCMgr->OnPhotographyMultiPartCaptureStart();
			bGameCameraCutThisFrame = true;
			bAnselCaptureNewlyActive = false;
		}

		if (bAnselCaptureNewlyFinished)
		{
			bGameCameraCutThisFrame = true;
			bAnselCaptureNewlyFinished = false;
			PCMgr->OnPhotographyMultiPartCaptureEnd();
		}

		if (bAnselSessionWantDeactivate)
		{
			bAnselSessionActive = false;
			bAnselSessionWantDeactivate = false;

			// auto-restore state

			if (bAutoPostprocess)
			{
				if (bWasShowingHUDBeforeSession)
				{
					PCOwner->MyHUD->ShowHUD(); // toggle off
				}
				if (bWereSubtitlesEnabledBeforeSession)
				{
					UGameplayStatics::SetSubtitlesEnabled(true);
				}
				if (bWasFadingEnabledBeforeSession)
				{
					PCMgr->bEnableFading = true;
				}
			}

			GAreScreenMessagesEnabled = bWasScreenMessagesEnabledBeforeSession;

			if (bAutoPause && !bWasPausedBeforeSession)
			{
				PCOwner->GetWorldSettings()->SetTimeDilation(fTimeDilationBeforeSession);
				PCOwner->SetPause(false);
				bPausedInternally = false;
			}

			PCMgr->GetWorld()->bIsCameraMoveableWhenPaused = bWasMovableCameraBeforeSession;

			// Re-activate Windows Cursor as Ansel will automatically hide the Windows mouse cursor when Ansel UI is enabled.
			//	See https://nvidiagameworks.github.io/Ansel/md/Ansel_integration_guide.html
			// !Needs to be done after AnselStopSessionCallback
			TSharedPtr<GenericApplication> PlatformApplication = FSlateApplicationBase::Get().GetPlatformApplication();
			if (PlatformApplication.IsValid() && PlatformApplication->Cursor.IsValid())
			{
				//PlatformApplication->Cursor->Show(true); // If we don't show it now, it never seems to come back when PCOwner does actually want it...?  Perhaps an Ansel DX12 bug? -> nerf this kludge until proven to still affect DX12 w/4.22 + latest driver
				PlatformApplication->Cursor->Show(PCOwner->ShouldShowMouseCursor());
			}

			for (auto &foo : InitialCVarMap)
			{
				// RESTORE CVARS FROM SESSION START
				if (foo.Value.cvar)
					foo.Value.cvar->SetWithCurrentPriority(foo.Value.fInitialVal);
			}
			InitialCVarMap.Empty(); // clear saved cvar values

			bHighQualityModeIsSetup = false;
			PCMgr->OnPhotographySessionEnd(); // after unpausing

			// no need to restore original camera params; re-clobbered every frame
		}
		else
		{
			bCameraIsInOriginalState = false;

			if (bAnselSessionNewlyActive)
			{
				NumFramesSinceSessionStart = 0;

				PCMgr->OnPhotographySessionStart(); // before pausing

				// copy these values to avoid mixup if the CVars are changed during capture callbacks
				static IConsoleVariable* CVarAutoPause = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.AutoPause"));
				static IConsoleVariable* CVarAutoPostProcess = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.AutoPostprocess"));

				bAutoPause = !!CVarAutoPause->GetInt();
				bAutoPostprocess = !!CVarAutoPostProcess->GetInt();
				bRayTracingEnabled = IsRayTracingEnabled();
				
				// attempt to pause game
				bWasPausedBeforeSession = PCOwner->IsPaused();
				bWasMovableCameraBeforeSession = PCMgr->GetWorld()->bIsCameraMoveableWhenPaused;
				PCMgr->GetWorld()->bIsCameraMoveableWhenPaused = true;
				if (bAutoPause && !bWasPausedBeforeSession)
				{
					fTimeDilationBeforeSession = PCOwner->GetWorldSettings()->TimeDilation;
					PCOwner->GetWorldSettings()->SetTimeDilation(0.f); // kill character motion-blur, this looks better than setting the motion-blur level to 0 (which flickers) - kinda heavy-handed but the only way I've found to kill motion-blur while also preventing flicker
					// we pause in a *future* frame so Slomo can kick-in properly
				}

				SetUpSessionCVars();

				bWasScreenMessagesEnabledBeforeSession = GAreScreenMessagesEnabled;
				GAreScreenMessagesEnabled = false;

				bWasFadingEnabledBeforeSession = PCMgr->bEnableFading;
				bWasShowingHUDBeforeSession = PCOwner->MyHUD &&
					PCOwner->MyHUD->bShowHUD;
				bWereSubtitlesEnabledBeforeSession = UGameplayStatics::AreSubtitlesEnabled();
				if (bAutoPostprocess)
				{
					if (bWasShowingHUDBeforeSession)
					{
						PCOwner->MyHUD->ShowHUD(); // toggle off
					}
					UGameplayStatics::SetSubtitlesEnabled(false);
					PCMgr->bEnableFading = false;
				}

				bUIControlsNeedRebuild = true;

				// store initial camera info
				UECameraPrevious = InOutPOV;
				UECameraOriginal = InOutPOV;

				FMinimalViewToAnselCamera(AnselCamera, InOutPOV);
				ansel::updateCamera(AnselCamera);

				AnselCameraOriginal = AnselCamera;
				AnselCameraPrevious = AnselCamera;

				bCameraIsInOriginalState = true;

				bAnselSessionNewlyActive = false;
			}
			else
			{
				ansel::updateCamera(AnselCamera);

				// active session; give Blueprints opportunity to modify camera, unless a capture is in progress
				if (!bAnselCaptureActive)
				{
					bCameraIsInOriginalState = BlueprintModifyCamera(AnselCamera, PCMgr);
				}
			}

			// ensure 2 frames have passed before pausing so that 0-timedilation can kick-in and kill the motion-blur!
			// why 2 frames rather than 1 (or even 0)?  dunno!  probably 1 frame for the new time dilation to go into effect and 1 more frame for the motion vectors to update.
			if (NumFramesSinceSessionStart == 2)
			{
				if (bAutoPause && !bWasPausedBeforeSession)
				{
					PCOwner->SetPause(true);
					bPausedInternally = true;
				}
			}

			AnselCameraToFMinimalView(InOutPOV, AnselCamera);

			AnselCameraPrevious = AnselCamera;
		}

		if (bAnselCaptureActive)
		{
			// eliminate letterboxing during capture
			InOutPOV.bConstrainAspectRatio = false;
		}
	}

	return bGameCameraCutThisFrame;
}

void FNVAnselCameraPhotographyPrivate::SetCapturedCVarPredicated(const char* CVarName, float valueIfNotReset, std::function<bool(const float, const float)> comparison, bool wantReset, bool useExistingPriority)
{
	CVarInfo* info = nullptr;
	if (InitialCVarMap.Contains(CVarName) || CaptureCVar(CVarName))
	{
		info = &InitialCVarMap[CVarName];
		if (info->cvar && comparison(valueIfNotReset, info->fInitialVal))
		{
			if (useExistingPriority)
				info->cvar->SetWithCurrentPriority(wantReset ? info->fInitialVal : valueIfNotReset);
			else
				info->cvar->Set(wantReset ? info->fInitialVal : valueIfNotReset);
		}
	}
	if (!(info && info->cvar)) UE_LOG(LogAnsel, Log, TEXT("CVar used by Ansel not found: %s"), CVarName);
}

void FNVAnselCameraPhotographyPrivate::SetCapturedCVar(const char* CVarName, float valueIfNotReset, bool wantReset, bool useExistingPriority)
{
	SetCapturedCVarPredicated(CVarName, valueIfNotReset,
		[](float, float) { return true; },
		wantReset, useExistingPriority);
}

void FNVAnselCameraPhotographyPrivate::ConfigureRenderingSettingsForPhotography(FPostProcessSettings& InOutPostProcessingSettings)
{
#define QUALITY_CVAR(NAME,BOOSTVAL) SetCapturedCVar(NAME, BOOSTVAL, !bHighQualityModeDesired, true)
#define QUALITY_CVAR_AT_LEAST(NAME,BOOSTVAL) SetCapturedCVarPredicated(NAME, BOOSTVAL, std::greater<float>(), !bHighQualityModeDesired, true)
#define QUALITY_CVAR_AT_MOST(NAME,BOOSTVAL) SetCapturedCVarPredicated(NAME, BOOSTVAL, std::less<float>(), !bHighQualityModeDesired, true)
#define QUALITY_CVAR_LOWPRIORITY_AT_LEAST(NAME,BOOSTVAL) SetCapturedCVarPredicated(NAME, BOOSTVAL, std::greater<float>(), !bHighQualityModeDesired, false)

	if (CVarDebug0->GetInt()
		&& CVarAllowHighQuality.GetValueOnAnyThread()
		&& bHighQualityModeIsSetup != bHighQualityModeDesired)
	{
		// Debug - makes HQ mode actually try to kill RT features
		QUALITY_CVAR("r.RayTracing.GlobalIllumination", 0);
		QUALITY_CVAR("r.RayTracing.Reflections", 0);
		QUALITY_CVAR("r.RayTracing.Shadows", 0);
		QUALITY_CVAR("r.RayTracing.Translucency", 0);
		QUALITY_CVAR("r.RayTracing.AmbientOcclusion", 0);
		UE_LOG(LogAnsel, Log, TEXT("Photography Debug0 mode actualized (enabled=%d)"), (int)bHighQualityModeDesired);
		bHighQualityModeIsSetup = bHighQualityModeDesired;
	}
	else if (CVarAllowHighQuality.GetValueOnAnyThread()
		&& bHighQualityModeIsSetup != bHighQualityModeDesired
		&& (bPausedInternally || !bAutoPause) // <- don't start overriding vars until truly paused
		&& (!CVarDebug0->GetInt()))
	{
		// Pump up (or reset) the quality. 

		// bring rendering up to (at least) 100% resolution, but won't override manually set value on console
		QUALITY_CVAR_LOWPRIORITY_AT_LEAST("r.ScreenPercentage", 100);

		// most of these similar to typical cinematic sg.* scalability settings, toned down a little for performance

		// can be a mild help with reflections
		QUALITY_CVAR("r.gbufferformat", 5); // 5 = highest precision

		// bias various geometry LODs
		QUALITY_CVAR_AT_MOST("r.staticmeshloddistancescale", 0.25f); // large quality bias
		QUALITY_CVAR_AT_MOST("r.landscapelodbias", -2);
		QUALITY_CVAR_AT_MOST("r.skeletalmeshlodbias", -2);

		// ~sg.AntiAliasingQuality @ cine
		QUALITY_CVAR("r.postprocessaaquality", 6); // 6 == max
		QUALITY_CVAR("r.defaultfeature.antialiasing", 2); // TAA

		// ~sg.EffectsQuality @ cinematic
		QUALITY_CVAR_AT_LEAST("r.TranslucencyLightingVolumeDim", 64);
		QUALITY_CVAR("r.RefractionQuality", 2);
		QUALITY_CVAR("r.SSR.Quality", 4);
		// QUALITY_CVAR("r.SceneColorFormat", 4); // don't really want to mess with this
		QUALITY_CVAR("r.TranslucencyVolumeBlur", 1);
		QUALITY_CVAR("r.MaterialQualityLevel", 1); // 1==high, 2==medium!
		QUALITY_CVAR("r.SSS.Scale", 1);
		QUALITY_CVAR("r.SSS.SampleSet", 2);
		QUALITY_CVAR("r.SSS.Quality", 1);
		QUALITY_CVAR("r.SSS.HalfRes", 0);
		QUALITY_CVAR_AT_LEAST("r.EmitterSpawnRateScale", 1.f); // not sure this has a point when game is paused though
		QUALITY_CVAR("r.ParticleLightQuality", 2);
		QUALITY_CVAR("r.DetailMode", 2);

		// ~sg.PostProcessQuality @ cinematic
		QUALITY_CVAR("r.AmbientOcclusionMipLevelFactor", 0.4f);
		QUALITY_CVAR("r.AmbientOcclusionMaxQuality", 100);
		QUALITY_CVAR("r.AmbientOcclusionLevels", -1);
		QUALITY_CVAR("r.AmbientOcclusionRadiusScale", 1.f);
		QUALITY_CVAR("r.DepthOfFieldQuality", 4);
		QUALITY_CVAR_AT_LEAST("r.RenderTargetPoolMin", 500); // ?
		QUALITY_CVAR("r.LensFlareQuality", 3);
		QUALITY_CVAR("r.SceneColorFringeQuality", 1);
		QUALITY_CVAR("r.BloomQuality", 5);
		QUALITY_CVAR("r.FastBlurThreshold", 100);
		QUALITY_CVAR("r.Upscale.Quality", 3);
		QUALITY_CVAR("r.Tonemapper.GrainQuantization", 1);
		QUALITY_CVAR("r.LightShaftQuality", 1);
		QUALITY_CVAR("r.Filter.SizeScale", 1);
		QUALITY_CVAR("r.Tonemapper.Quality", 5);
		QUALITY_CVAR("r.DOF.Gather.AccumulatorQuality", 1);
		QUALITY_CVAR("r.DOF.Gather.PostfilterMethod", 1);
		QUALITY_CVAR("r.DOF.Gather.EnableBokehSettings", 1);
		QUALITY_CVAR_AT_LEAST("r.DOF.Gather.RingCount", 5);
		QUALITY_CVAR("r.DOF.Scatter.ForegroundCompositing", 1);
		QUALITY_CVAR("r.DOF.Scatter.BackgroundCompositing", 2);
		QUALITY_CVAR("r.DOF.Scatter.EnableBokehSettings", 1);
		QUALITY_CVAR("r.DOF.Scatter.MaxSpriteRatio", 0.1f);
		QUALITY_CVAR("r.DOF.Recombine.Quality", 2);
		QUALITY_CVAR("r.DOF.Recombine.EnableBokehSettings", 1);
		QUALITY_CVAR("r.DOF.TemporalAAQuality", 1);
		QUALITY_CVAR("r.DOF.Kernel.MaxForegroundRadius", 0.025f);
		QUALITY_CVAR("r.DOF.Kernel.MaxBackgroundRadius", 0.025f);

		// ~sg.TextureQuality @ cinematic
		QUALITY_CVAR("r.Streaming.MipBias", 0);
		QUALITY_CVAR_AT_LEAST("r.MaxAnisotropy", 16);
		QUALITY_CVAR("r.Streaming.MaxEffectiveScreenSize", 0);
		// intentionally don't mess with streaming pool size, see 'CVarExtreme' section below

		// ~sg.FoliageQuality @ cinematic
		QUALITY_CVAR_AT_LEAST("foliage.DensityScale", 1.f);
		QUALITY_CVAR_AT_LEAST("grass.DensityScale", 1.f);

		// ~sg.ViewDistanceQuality @ cine but only mild draw distance boost
		QUALITY_CVAR_AT_LEAST("r.viewdistancescale", 2.0f); // or even more...?

		// ~sg.ShadowQuality @ cinematic
		QUALITY_CVAR_AT_LEAST("r.LightFunctionQuality", 2);
		QUALITY_CVAR("r.ShadowQuality", 5);
		QUALITY_CVAR_AT_LEAST("r.Shadow.CSM.MaxCascades", 10);
		QUALITY_CVAR_AT_LEAST("r.Shadow.MaxResolution", 4096);
		QUALITY_CVAR_AT_LEAST("r.Shadow.MaxCSMResolution", 4096);
		QUALITY_CVAR_AT_MOST("r.Shadow.RadiusThreshold", 0.f);
		QUALITY_CVAR("r.Shadow.DistanceScale", 1.f);
		QUALITY_CVAR("r.Shadow.CSM.TransitionScale", 1.f);
		QUALITY_CVAR("r.Shadow.PreShadowResolutionFactor", 1.f);
		QUALITY_CVAR("r.AOQuality", 2);
		QUALITY_CVAR("r.VolumetricFog", 1);
		QUALITY_CVAR("r.VolumetricFog.GridPixelSize", 4);
		QUALITY_CVAR("r.VolumetricFog.GridSizeZ", 128);
		QUALITY_CVAR_AT_LEAST("r.VolumetricFog.HistoryMissSupersampleCount", 16);
		QUALITY_CVAR_AT_LEAST("r.LightMaxDrawDistanceScale", 2.f);
		QUALITY_CVAR("r.CapsuleShadows", 1);

		// pump up the quality of raytracing features, though we won't necessarily turn them on if the game doesn't already have them enabled
		if (bRayTracingEnabled)
		{
			/*** HIGH-QUALITY MODE DOES *NOT* FORCE GI ON ***/
			QUALITY_CVAR_AT_MOST("r.RayTracing.GlobalIllumination.DiffuseThreshold", 0); // artifact avoidance
			//QUALITY_CVAR_AT_LEAST("r.RayTracing.GlobalIllumination.MaxBounces", 1); // 1~=IQ cost:benefit sweet-spot

			/*** HIGH-QUALITY MODE DOES *NOT* FORCE RT AO ON ***/
			QUALITY_CVAR_AT_LEAST("r.RayTracing.AmbientOcclusion.SamplesPerPixel", 1); // haven't seen benefit from larger values

			/*** HIGH-QUALITY MODE FORCES RT REFLECTIONS ON ***/
			QUALITY_CVAR_AT_LEAST("r.RayTracing.Reflections.MaxBounces", 2); // sweet-spot
			QUALITY_CVAR_AT_LEAST("r.RayTracing.Reflections.MaxRoughness", 0.9f); // speed hit
			QUALITY_CVAR("r.RayTracing.Reflections.SortMaterials", 1); // usually some kind of perf win, especially w/above reflection quality
			QUALITY_CVAR("r.RayTracing.Reflections.DirectLighting", 1);
			//QUALITY_CVAR("r.RayTracing.Reflections.EmissiveAndIndirectLighting", 1);// curiously problematic, leave alone
			QUALITY_CVAR_AT_LEAST("r.RayTracing.Reflections.Shadows", 1); // -1==auto, 0==off, 1==hard, 2==soft/area(requires high spp)
			QUALITY_CVAR("r.RayTracing.Reflections.HeightFog", 1);
			//QUALITY_CVAR_AT_LEAST("r.RayTracing.Reflections.SamplesPerPixel", 2); // -1==use pp vol // NOPE, don't touch spp right now: 1 is ok, ~10 is good, anywhere in-between is noisy
			QUALITY_CVAR_AT_LEAST("r.RayTracing.Reflections.ScreenPercentage", 100);
			QUALITY_CVAR("r.RayTracing.Reflections", 1); // FORCE ON: ignore postproc volume flag

			/*** HIGH-QUALITY MODE DOES *NOT* FORCE RT TRANSLUCENCY ON ***/
			QUALITY_CVAR_AT_LEAST("r.RayTracing.Translucency.MaxRoughness", 0.9f);
			//QUALITY_CVAR_AT_LEAST("r.RayTracing.Translucency.MaxRefractionRays", 11); // buggy with grass, leave alone for now
			QUALITY_CVAR_AT_LEAST("r.RayTracing.Translucency.Shadows", 1); // turn on at least
			//QUALITY_CVAR("r.RayTracing.Translucency", -1); // 1==enabled always, ignore postproc volume flags -- NOPE, DON'T FORCE-ENABLE TRANSLUCENCY, IT MAKES EVERY SINGLE TRANSLUCENCY REFRACT or just plain disappear, too weird for random content (i.e. Infiltrator). -1 == explicitly marked-up volumes use RT

			/*** HIGH-QUALITY MODE FORCES RT SHADOWS ON ***/
			//QUALITY_CVAR_AT_LEAST("r.RayTracing.Shadow.SamplesPerPixel", 1); // 5==reduces stippling artifacts // >1 seems to do nothing extra now?
			////QUALITY_CVAR("r.Shadow.Denoiser", 2); // "GScreenSpaceDenoiser witch may be overriden by a third party plugin"
			QUALITY_CVAR_AT_LEAST("r.RayTracing.Shadows", 1); // 1==enableRT (default)
		}

		 // these are some extreme settings whose quality:risk ratio may be debatable or unproven
		if (CVarExtreme->GetInt())
		{
			// great idea but not until I've proven that this isn't deadly or extremely slow on lower-spec machines:

			QUALITY_CVAR("r.Streaming.LimitPoolSizeToVRAM", 0); // 0 is aggressive but is it safe? seems safe.
			QUALITY_CVAR_AT_LEAST("r.Streaming.PoolSize", 3000); // cine - perhaps redundant when r.streaming.fullyloadusedtextures

			QUALITY_CVAR("r.streaming.hlodstrategy", 2); // probably use 0 if using r.streaming.fullyloadusedtextures, else 2
			//QUALITY_CVAR("r.streaming.fullyloadusedtextures", 1); // no - LODs oscillate when overcommitted
			QUALITY_CVAR_AT_LEAST("r.viewdistancescale", 10.f); // cinematic - extreme

			if (bRayTracingEnabled)
			{
				// higher-IQ thresholds
				QUALITY_CVAR_AT_LEAST("r.RayTracing.Translucency.MaxRoughness", 1.f); // speed hit
				QUALITY_CVAR_AT_LEAST("r.RayTracing.Reflections.MaxRoughness", 1.f); // speed hit

				//QUALITY_CVAR("r.ambientocclusionstaticfraction", 0.f); // trust RT AO/GI...? - needs more testing, doesn't seem a big win

				/*** EXTREME-QUALITY MODE FORCES GI ON ***/
				// first, some IQ:speed tweaks to make GI speed practical
				//
				QUALITY_CVAR("r.RayTracing.GlobalIllumination.ScreenPercentage", 50); // 50% = this is actually a quality DROP by default but it makes the GI speed practical -- requires >>>=2spp though
				QUALITY_CVAR_AT_MOST("r.RayTracing.GlobalIllumination.MaxRayDistance", 7500); // ditto; most of the IQ benefit, but often faster than default huge ray distance
				QUALITY_CVAR_AT_LEAST("r.RayTracing.GlobalIllumination.SamplesPerPixel", 4); // at LEAST 2spp needed to reduce significant noise in some scenes, even up to 8+ helps
				QUALITY_CVAR_AT_LEAST("r.RayTracing.GlobalIllumination.NextEventEstimationSamples", 16); // 2==default; 16 necessary for low-light conditions when using only 4spp, else get blotches.  raising estimation samples cheaper than raising spp.
				QUALITY_CVAR_AT_LEAST("r.GlobalIllumination.Denoiser.ReconstructionSamples", 56/*=max*/); // better if only using 4spp @ quarter rez.  default is 16.
				//QUALITY_CVAR_AT_LEAST("r.RayTracing.GlobalIllumination.MaxBounces", 3); // 2+ is sometimes slightly noticable, sloww
				////QUALITY_CVAR("r.RayTracing.GlobalIllumination.EvalSkyLight", 1); // EXPERIMENTAL
				QUALITY_CVAR("r.RayTracing.GlobalIllumination", 1); // FORCE ON: should be fast enough to not TDR(!) with screenpercentage=50... usually a fair IQ win with random content... hidden behind 'EXTREME' mode until I've exercised it more.

				// just not hugely tested:
				QUALITY_CVAR_AT_LEAST("r.RayTracing.StochasticRectLight.SamplesPerPixel", 4);
				//QUALITY_CVAR("r.RayTracing.StochasticRectLight", 1); // 1==suspicious, probably broken
				QUALITY_CVAR_AT_LEAST("r.RayTracing.SkyLight.SamplesPerPixel", 4); // default==-1 UNPROVEN TRY ME
			}

			// just not hugely tested:
			QUALITY_CVAR("r.particlelodbias", -2);

			// unproven or possibly buggy
			//QUALITY_CVAR("r.streaming.useallmips", 1); // removes relative prioritization spec'd by app... unproven that this is a good idea
			//QUALITY_CVAR_AT_LEAST("r.streaming.boost", 9999); // 0 = supposedly use all available vram, but it looks like 0 = buggy
		}

#undef QUALITY_CVAR
#undef QUALITY_CVAR_AT_LEAST
#undef QUALITY_CVAR_AT_MOST
#undef QUALITY_CVAR_LOWPRIORITY_AT_LEAST

		UE_LOG(LogAnsel, Log, TEXT("Photography HQ mode actualized (enabled=%d)"), (int)bHighQualityModeDesired);
		bHighQualityModeIsSetup = bHighQualityModeDesired;
	}

	if (bAnselCaptureActive)
	{
		// camera doesn't linger in one place very long so maximize streaming rate
		SetCapturedCVar("r.disablelodfade", 1);
		SetCapturedCVar("r.streaming.framesforfullupdate", 1); // recalc required LODs ASAP
		SetCapturedCVar("r.Streaming.MaxNumTexturesToStreamPerFrame", 0); // no limit
		SetCapturedCVar("r.streaming.numstaticcomponentsprocessedperframe", 0); // 0 = load all pending static geom now

		if (bAutoPostprocess)
		{
			// force-disable the standard postprocessing effects which are known to
			// be problematic in multi-part shots

			// nerf remaining motion blur
			InOutPostProcessingSettings.bOverride_MotionBlurAmount = 1;
			InOutPostProcessingSettings.MotionBlurAmount = 0.f;

			// these effects tile poorly
			InOutPostProcessingSettings.bOverride_BloomDirtMaskIntensity = 1;
			InOutPostProcessingSettings.BloomDirtMaskIntensity = 0.f;
			InOutPostProcessingSettings.bOverride_LensFlareIntensity = 1;
			InOutPostProcessingSettings.LensFlareIntensity = 0.f;
			InOutPostProcessingSettings.bOverride_VignetteIntensity = 1;
			InOutPostProcessingSettings.VignetteIntensity = 0.f;
			InOutPostProcessingSettings.bOverride_SceneFringeIntensity = 1;
			InOutPostProcessingSettings.SceneFringeIntensity = 0.f;

			// freeze auto-exposure adaptation
			InOutPostProcessingSettings.bOverride_AutoExposureSpeedDown = 1;
			InOutPostProcessingSettings.AutoExposureSpeedDown = 0.f;
			InOutPostProcessingSettings.bOverride_AutoExposureSpeedUp = 1;
			InOutPostProcessingSettings.AutoExposureSpeedUp = 0.f;

			// bring rendering up to (at least) full resolution
			if (InOutPostProcessingSettings.ScreenPercentage < 100.f)
			{
				// note: won't override r.screenpercentage set from console, that takes precedence
				InOutPostProcessingSettings.bOverride_ScreenPercentage = 1;
				InOutPostProcessingSettings.ScreenPercentage = 100.f;
			}

			bool bAnselSuperresCaptureActive = AnselCaptureInfo.captureType == ansel::kCaptureTypeSuperResolution;
			bool bAnselStereoCaptureActive = AnselCaptureInfo.captureType == ansel::kCaptureType360Stereo || AnselCaptureInfo.captureType == ansel::kCaptureTypeStereo;

			if (bAnselStereoCaptureActive)
			{
				// Attempt to nerf DoF in stereoscopic shots where it can be quite unpleasant for the viewer
				InOutPostProcessingSettings.bOverride_DepthOfFieldScale = 1;
				InOutPostProcessingSettings.DepthOfFieldScale = 0.f; // BokehDOF
				InOutPostProcessingSettings.bOverride_DepthOfFieldNearBlurSize = 1;
				InOutPostProcessingSettings.DepthOfFieldNearBlurSize = 0.f; // GaussianDOF
				InOutPostProcessingSettings.bOverride_DepthOfFieldFarBlurSize = 1;
				InOutPostProcessingSettings.DepthOfFieldFarBlurSize = 0.f; // GaussianDOF
				InOutPostProcessingSettings.bOverride_DepthOfFieldDepthBlurRadius = 1;
				InOutPostProcessingSettings.DepthOfFieldDepthBlurRadius = 0.f; // CircleDOF
				InOutPostProcessingSettings.bOverride_DepthOfFieldVignetteSize = 1;
				InOutPostProcessingSettings.DepthOfFieldVignetteSize = 200.f; // Scene.h says 200.0 means 'no effect'
			}

			if (!bAnselSuperresCaptureActive)
			{
				// Disable SSR in multi-part shots unless taking a super-resolution shot; SSR *usually* degrades gracefully in tiled shots, and super-resolution mode in Ansel has an 'enhance' option which repairs any lingering SSR artifacts quite well.
				InOutPostProcessingSettings.bOverride_ScreenSpaceReflectionIntensity = 1;
				InOutPostProcessingSettings.ScreenSpaceReflectionIntensity = 0.f;
			}
		}
	}
}

void FNVAnselCameraPhotographyPrivate::SetUpSessionCVars()
{
	// This set of CVar tweaks are good - or necessary - for photographic sessions
	{
		SetCapturedCVar("r.oneframethreadlag", 1); // ansel needs frame latency to be predictable

		// these are okay tweaks to streaming heuristics to reduce latency of full texture loads or minimize VRAM waste
		SetCapturedCVar("r.streaming.minmipforsplitrequest", 1); // strictly prioritize what's visible right now
		SetCapturedCVar("r.streaming.hiddenprimitivescale", 0.001f); // hint to engine to deprioritize obscured textures...?
		SetCapturedCVar("r.Streaming.Boost", 1);

		SetCapturedCVar("r.motionblurquality", 0); // this nerfs motion blur for non-characters
	}
}

void FNVAnselCameraPhotographyPrivate::UpdatePostProcessing(FPostProcessSettings& InOutPostProcessingSettings)
{
	if (bAnselSessionActive)
	{
		DoCustomUIControls(InOutPostProcessingSettings, bUIControlsNeedRebuild);

		ConfigureRenderingSettingsForPhotography(InOutPostProcessingSettings);
	}
}

void FNVAnselCameraPhotographyPrivate::StartSession()
{
	ansel::startSession();
}

void FNVAnselCameraPhotographyPrivate::StopSession()
{
	ansel::stopSession();
}

void FNVAnselCameraPhotographyPrivate::DefaultConstrainCamera(const FVector NewCameraLocation, const FVector PreviousCameraLocation, const FVector OriginalCameraLocation, FVector& OutCameraLocation, APlayerCameraManager* PCMgr)
{
	// let proposed camera through unmodified by default
	OutCameraLocation = NewCameraLocation;

	static IConsoleVariable* CVarConstrainCameraDistance = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.Constrain.MaxCameraDistance"));

	// First, constrain by distance
	FVector ConstrainedLocation;
	float MaxDistance = CVarConstrainCameraDistance->GetFloat();
	UAnselFunctionLibrary::ConstrainCameraByDistance(PCMgr, NewCameraLocation, PreviousCameraLocation, OriginalCameraLocation, ConstrainedLocation, MaxDistance);

	// Second, constrain against collidable geometry
	UAnselFunctionLibrary::ConstrainCameraByGeometry(PCMgr, ConstrainedLocation, PreviousCameraLocation, OriginalCameraLocation, OutCameraLocation);
}

ansel::StartSessionStatus FNVAnselCameraPhotographyPrivate::AnselStartSessionCallback(ansel::SessionConfiguration& settings, void* userPointer)
{
	ansel::StartSessionStatus AnselSessionStatus = ansel::kDisallowed;
	FNVAnselCameraPhotographyPrivate* PrivateImpl = static_cast<FNVAnselCameraPhotographyPrivate*>(userPointer);
	check(PrivateImpl != nullptr);

	static IConsoleVariable* CVarAllow = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.Allow"));
	static IConsoleVariable* CVarEnableMultipart = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.EnableMultipart"));
	if (!PrivateImpl->bForceDisallow && CVarAllow->GetInt() && !GIsEditor)
	{
		bool bPauseAllowed = true;
		bool bEnableMultipart = !!CVarEnableMultipart->GetInt();

		settings.isTranslationAllowed = true;
		settings.isFovChangeAllowed = !PrivateImpl->bIsOrthoProjection;
		settings.isRotationAllowed = true;
		settings.isPauseAllowed = bPauseAllowed;
		settings.isHighresAllowed = bEnableMultipart;
		settings.is360MonoAllowed = bEnableMultipart;
		settings.is360StereoAllowed = bEnableMultipart;

		PrivateImpl->bAnselSessionActive = true;
		PrivateImpl->bAnselSessionNewlyActive = true;
		PrivateImpl->bHighQualityModeDesired = false;

		AnselSessionStatus = ansel::kAllowed;
	}

	UE_LOG(LogAnsel, Log, TEXT("Photography camera session attempt started, Allowed=%d, ForceDisallowed=%d"), int(AnselSessionStatus == ansel::kAllowed), int(PrivateImpl->bForceDisallow));

	return AnselSessionStatus;
}

void FNVAnselCameraPhotographyPrivate::AnselStopSessionCallback(void* userPointer)
{
	FNVAnselCameraPhotographyPrivate* PrivateImpl = static_cast<FNVAnselCameraPhotographyPrivate*>(userPointer);
	check(PrivateImpl != nullptr);
	if (PrivateImpl->bAnselSessionActive && PrivateImpl->bAnselSessionNewlyActive)
	{
		// if we've not acted upon the new session at all yet, then just don't.
		PrivateImpl->bAnselSessionActive = false;
	}
	else
	{
		PrivateImpl->bAnselSessionWantDeactivate = true;
	}

	UE_LOG(LogAnsel, Log, TEXT("Photography camera session end"));
}

void FNVAnselCameraPhotographyPrivate::AnselStartCaptureCallback(const ansel::CaptureConfiguration& CaptureInfo, void* userPointer)
{
	FNVAnselCameraPhotographyPrivate* PrivateImpl = static_cast<FNVAnselCameraPhotographyPrivate*>(userPointer);
	check(PrivateImpl != nullptr);
	PrivateImpl->bAnselCaptureActive = true;
	PrivateImpl->bAnselCaptureNewlyActive = true;
	PrivateImpl->AnselCaptureInfo = CaptureInfo;

	UE_LOG(LogAnsel, Log, TEXT("Photography camera multi-part capture started"));
}

void FNVAnselCameraPhotographyPrivate::AnselStopCaptureCallback(void* userPointer)
{
	FNVAnselCameraPhotographyPrivate* PrivateImpl = static_cast<FNVAnselCameraPhotographyPrivate*>(userPointer);
	check(PrivateImpl != nullptr);
	PrivateImpl->bAnselCaptureActive = false;
	PrivateImpl->bAnselCaptureNewlyFinished = true;

	UE_LOG(LogAnsel, Log, TEXT("Photography camera multi-part capture end"));
}

void FNVAnselCameraPhotographyPrivate::AnselChangeQualityCallback(bool isHighQuality, void* ACPPuserPointer)
{
	FNVAnselCameraPhotographyPrivate* PrivateImpl = static_cast<FNVAnselCameraPhotographyPrivate*>(ACPPuserPointer);
	check(PrivateImpl != nullptr);
	PrivateImpl->bHighQualityModeDesired = isHighQuality;

	UE_LOG(LogAnsel, Log, TEXT("Photography HQ mode toggle (%d)"), (int)isHighQuality);
}

void FNVAnselCameraPhotographyPrivate::ReconfigureAnsel()
{
	check(AnselConfig != nullptr);
	AnselConfig->userPointer = this;
	AnselConfig->startSessionCallback = AnselStartSessionCallback;
	AnselConfig->stopSessionCallback = AnselStopSessionCallback;
	AnselConfig->startCaptureCallback = AnselStartCaptureCallback;
	AnselConfig->stopCaptureCallback = AnselStopCaptureCallback;
	AnselConfig->changeQualityCallback = AnselChangeQualityCallback;
	
	if (GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid() && GEngine->GameViewport->GetWindow()->GetNativeWindow().IsValid())
	{
		AnselConfig->gameWindowHandle = GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
	}
	UE_LOG(LogAnsel, Log, TEXT("gameWindowHandle= %p"), AnselConfig->gameWindowHandle);

	static IConsoleVariable* CVarTranslationSpeed = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.TranslationSpeed"));
	AnselConfig->translationalSpeedInWorldUnitsPerSecond = CVarTranslationSpeed->GetFloat();

	AnselConfig->metersInWorldUnit = 1.0f / 100.0f;
	AWorldSettings* WorldSettings = nullptr;
	if (GEngine->GetWorld() != nullptr)
	{
		WorldSettings = GEngine->GetWorld()->GetWorldSettings();
	}
	if (WorldSettings != nullptr && WorldSettings->WorldToMeters != 0.f)
	{
		AnselConfig->metersInWorldUnit = 1.0f / WorldSettings->WorldToMeters;
	}
	UE_LOG(LogAnsel, Log, TEXT("We reckon %f meters to 1 world unit"), AnselConfig->metersInWorldUnit);

	AnselConfig->isCameraOffcenteredProjectionSupported = true;

	AnselConfig->captureLatency = 0; // important

	static IConsoleVariable* CVarSettleFrames = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Photography.SettleFrames"));
	AnselConfig->captureSettleLatency = CVarSettleFrames->GetInt();

	ansel::SetConfigurationStatus status = ansel::setConfiguration(*AnselConfig);
	if (status != ansel::kSetConfigurationSuccess)
	{
		UE_LOG(LogAnsel, Log, TEXT("ReconfigureAnsel setConfiguration returned %ld"), (long int)(status));
	}
}

void FNVAnselCameraPhotographyPrivate::DeconfigureAnsel()
{
	check(AnselConfig != nullptr);

	AnselConfig->userPointer = nullptr;
	AnselConfig->startSessionCallback = nullptr;
	AnselConfig->stopSessionCallback = nullptr;
	AnselConfig->startCaptureCallback = nullptr;
	AnselConfig->stopCaptureCallback = nullptr;
	AnselConfig->gameWindowHandle = nullptr;
	ansel::SetConfigurationStatus status = ansel::setConfiguration(*AnselConfig);
	if (status != ansel::kSetConfigurationSuccess)
	{
		UE_LOG(LogAnsel, Log, TEXT("DeconfigureAnsel setConfiguration returned %ld"), (long int)(status));
	}
}

class FAnselModule : public IAnselModule
{
public:
	virtual void StartupModule() override
	{
		ICameraPhotographyModule::StartupModule();
		check(!bAnselDLLLoaded);

		// Late-load Ansel DLL.  DLL name has been worked out by the build scripts as ANSEL_DLL
		FString AnselDLLName;
		FString AnselBinariesRoot = FPaths::EngineDir() / TEXT("Plugins/Runtime/Nvidia/Ansel/Binaries/ThirdParty/");
		// common preprocessor fudge to convert macro expansion into string
#define STRINGIFY(X) STRINGIFY2(X)
#define STRINGIFY2(X) #X
			AnselDLLName = AnselBinariesRoot + TEXT(STRINGIFY(ANSEL_DLL));
			AnselSDKDLLHandle = FPlatformProcess::GetDllHandle(*(AnselDLLName));

		bAnselDLLLoaded = AnselSDKDLLHandle != 0;
		UE_LOG(LogAnsel, Log, TEXT("Tried to load %s : success=%d"), *AnselDLLName, int(bAnselDLLLoaded));
	}

	virtual void ShutdownModule() override
	{		
		if (bAnselDLLLoaded)
		{
			FPlatformProcess::FreeDllHandle(AnselSDKDLLHandle);
			AnselSDKDLLHandle = 0;
			bAnselDLLLoaded = false;
		}
		ICameraPhotographyModule::ShutdownModule();
	}
private:

	virtual TSharedPtr< class ICameraPhotography > CreateCameraPhotography() override
	{
		TSharedPtr<ICameraPhotography> Photography = nullptr;

		FNVAnselCameraPhotographyPrivate* PhotographyPrivate = new FNVAnselCameraPhotographyPrivate();
		if (PhotographyPrivate->IsSupported())
		{
			Photography = TSharedPtr<ICameraPhotography>(PhotographyPrivate);
		}
		else
		{
			delete PhotographyPrivate;
		}

		return Photography;
	}
};

IMPLEMENT_MODULE(FAnselModule, Ansel)

#undef LOCTEXT_NAMESPACE