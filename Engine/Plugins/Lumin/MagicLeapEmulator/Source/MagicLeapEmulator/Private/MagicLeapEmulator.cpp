// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapEmulator.h"

#include "Engine/Engine.h"
#include "CoreMinimal.h"
#include "Misc/CoreDelegates.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Components/SceneCaptureComponent2D.h"
#include "IXRTrackingSystem.h"
#include "IXRCamera.h"
#include "XRRenderTargetManager.h"

#if WITH_EDITOR
	#include "Editor.h"
	#include "ISettingsModule.h"
	#include "ISettingsSection.h"
#endif

#include "IHeadMountedDisplay.h"
#include "IMagicLeapEmulatorPlugin.h"
#include "MagicLeapEmulatorSettings.h"
#include "MagicLeapEmulatorBackgroundMarker.h"
#include "MagicLeapEmulatorSceneCaptureComponent.h"
#include "EmulatorCameraModifier.h"
#include "Materials/MaterialInstanceDynamic.h"

#define LOCTEXT_NAMESPACE "MagicLeapEmulator"

//PRAGMA_DISABLE_OPTIMIZATION

class FMagicLeapEmulatorPlugin : public IMagicLeapEmulatorPlugin
{
public:
	FMagicLeapEmulatorPlugin()
	{}

	virtual void StartupModule() override
	{
#if WITH_EDITOR
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FMagicLeapEmulatorPlugin::InitSettings);
#endif // WITH_EDITOR

		FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FMagicLeapEmulatorPlugin::WorldInitialized);
		FWorldDelegates::OnWorldCleanup.AddRaw(this, &FMagicLeapEmulatorPlugin::WorldCleanup);
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		// unregister settings
		ISettingsModule* const SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "Magic Leap Emulator");
		}
#endif //WITH_EDITOR
	}

private:
	void InitSettings()
	{
#if WITH_EDITOR
		// register settings
		ISettingsModule* const SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "Magic Leap Emulator",
				LOCTEXT("MagicLeapEmulatorSettingsName", "Magic Leap Emulator"),
				LOCTEXT("MagicLeapEmulatorSettingsDescription", "Configure the Magic Leap Emulator plug-in."),
				GetMutableDefault<UMagicLeapEmulatorSettings>()
			);
		}
#endif // WITH_EDITOR
	}

	void WorldInitialized(UWorld* World, const UWorld::InitializationValues IVS)
	{
		if (World->IsGameWorld())
		{
			UMagicLeapEmulatorSettings const* const Settings = GetDefault<UMagicLeapEmulatorSettings>();
			if (Settings && Settings->bEnableMagicLeapEmulation)
			{
				Emulator.StartEmulating(World);
			}
		}
	}
	void WorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
	{
		Emulator.StopEmulating(World);
	}
	
	// #todo, handle multiple emulators for multiple worlds?
	FMagicLeapEmulator Emulator;
};

IMPLEMENT_MODULE(FMagicLeapEmulatorPlugin, MagicLeapEmulator);

FMagicLeapEmulator::FMagicLeapEmulator()
{
	bEmulatorInitialized = 0;
}

FMagicLeapEmulator::~FMagicLeapEmulator()
{
}

void FMagicLeapEmulator::StartEmulating(UWorld* World)
{
	// ignore if world is already set, another world is using the emulator
	if (World && (MyWorld.IsValid() == false))
	{
		MyWorld = World;

		// after login, we'll have a playercontroller, at which point we can set up the camera to emulate ML's additive rendering on top of the 
		// background scenecapture
		PostLoginDelegateHandle = FGameModeEvents::GameModePostLoginEvent.AddRaw(this, &FMagicLeapEmulator::InitEmulatorCamera);
	}
}

void FMagicLeapEmulator::InitEmulatorCamera(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginDelegateHandle);

	MyPlayerController = NewPlayer;

	if (NewPlayer && bEmulatorInitialized == false)
	{
		NewPlayer->GetWorldTimerManager().SetTimer(InitCameraTimerHandle, FTimerDelegate::CreateRaw(this, &FMagicLeapEmulator::ReallyInitCamera), 0.3f, false);
	}
}

bool CreateSceneCaptureSetup(FMagicLeapEmulator* Emulator, APlayerController* PC, APlayerCameraManager* Cam, UMagicLeapEmulatorSceneCaptureComponent*& OutSceneCapComp, UTextureRenderTarget2D*& OutRenderTarget)
{
	if (PC && Cam)
	{
		// this is the scenecapture component for full frame, or left eye in stereo
		OutSceneCapComp = NewObject<UMagicLeapEmulatorSceneCaptureComponent>(Cam);
		if (OutSceneCapComp)
		{
			OutSceneCapComp->FOVAngle = Cam->GetFOVAngle();
			OutSceneCapComp->Emulator = Emulator;

			OutSceneCapComp->RegisterComponent();

			// #todo, bind to onviewportresize, recreate the render target?
			int32 ViewportSizeX = 0;
			int32 ViewportSizeY = 0;

			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

			UGameViewportClient* const VC = PC->GetWorld()->GetGameViewport();
			if (VC && GEngine->StereoRenderingDevice.IsValid())
			{
				bool const bStereo = (GEngine->IsStereoscopic3D(VC->Viewport) && GEngine->XRSystem->GetHMDDevice());
				if (bStereo)
				{
					// we only want half width per eye if stereo
					ViewportSizeX /= 2;
				}

				uint32 RTSizeX = static_cast<int32>(ViewportSizeX);
				uint32 RTSizeY = static_cast<int32>(ViewportSizeY);
				GEngine->StereoRenderingDevice->GetRenderTargetManager()->CalculateRenderTargetSize(*VC->Viewport, RTSizeX, RTSizeY);
				ViewportSizeX = static_cast<int32>(RTSizeX);
				ViewportSizeY = static_cast<int32>(RTSizeY);
			}

			OutRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(PC, ViewportSizeX, ViewportSizeY);
			if (OutRenderTarget)
			{
				OutRenderTarget->TargetGamma = 2.2f;
				OutSceneCapComp->TextureTarget = OutRenderTarget;
				return true;
			}
		}
	}

	OutSceneCapComp = nullptr;
	OutRenderTarget = nullptr;
	return false;
}

bool bForceStereoCaptures = false;

bool bBackgroundInRenderTarget = false;

static const FName NAME_EmulatorBackground(TEXT("EmulatorBackground"));


void FMagicLeapEmulator::ReallyInitCamera()
{
	UWorld* const World = MyWorld.Get();

	APlayerController* NewPlayer = MyPlayerController.Get();
	APlayerCameraManager* const PlayerCamera = NewPlayer ? NewPlayer->PlayerCameraManager : nullptr;
	if (World && PlayerCamera && GEngine)
	{
		UGameViewportClient* const VC = World->GetGameViewport();
		bool const bStereo = VC ? (bForceStereoCaptures || (GEngine->IsStereoscopic3D(VC->Viewport) && GEngine->XRSystem.IsValid())) : false;

		// set up scene capture for left eye (or full screen if nonstereo)
		UMagicLeapEmulatorSceneCaptureComponent* SceneCapLeftOrFull = nullptr;
		UTextureRenderTarget2D* BGRT_LeftOrFull = nullptr;
		CreateSceneCaptureSetup(this, NewPlayer, PlayerCamera, SceneCapLeftOrFull, BGRT_LeftOrFull);
		BackgroundRenderTarget_LeftOrFull = BGRT_LeftOrFull;
		BackgroundSceneCaptureComponent_LeftOrFull = SceneCapLeftOrFull;

		// set up scene capture for right eye if stereo
		UMagicLeapEmulatorSceneCaptureComponent* SceneCapRight = nullptr;
		UTextureRenderTarget2D* BGRT_Right = nullptr;
		if (bStereo)
		{
			CreateSceneCaptureSetup(this, NewPlayer, PlayerCamera, SceneCapRight, BGRT_Right);
			BackgroundRenderTarget_Right = BGRT_Right;
			BackgroundSceneCaptureComponent_Right = SceneCapRight;
		}

		if (SceneCapLeftOrFull)
		{
			AMagicLeapEmulatorBackgroundMarker* FirstBGMarker = nullptr;

			// build list of background levels
			TArray<ULevel*> BGLevels;
			for (TActorIterator<AMagicLeapEmulatorBackgroundMarker> It(MyWorld.Get()); It; ++It)
			{
				AMagicLeapEmulatorBackgroundMarker* const BGMarker = *It;
				if (BGMarker->bParentLevelIsBackgroundLevel)
				{
					BGLevels.AddUnique(BGMarker->GetLevel());
				}

				if (FirstBGMarker == nullptr)
				{
					FirstBGMarker = BGMarker;
					FirstBGMarker->Emulator = this;
				}
			}

			// we pick just one of the BG actors to make it tickable
			if (FirstBGMarker)
			{
				FirstBGMarker->SetActorTickEnabled(true);
			}

			UMagicLeapEmulatorSettings const* const Settings = GetDefault<UMagicLeapEmulatorSettings>();
			bool const bEnableCollisionWithBackground = (Settings && Settings->bEnableCollisionWithBackground);

			if (bBackgroundInRenderTarget)
			{
				SceneCapLeftOrFull->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
				if (SceneCapRight)
				{
					SceneCapRight->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
				}

				// set up background levels
				for (ULevel* L : BGLevels)
				{
					// add every actor to showonlyactors
					SceneCapLeftOrFull->ShowOnlyActors.Append(L->Actors);
					if (SceneCapRight)
					{
						SceneCapRight->ShowOnlyActors.Append(L->Actors);
					}

					// also add every actor to player's hidden actors
					NewPlayer->HiddenActors.Append(L->Actors);

					if (bEnableCollisionWithBackground == false)
					{
						// no collision on anything in the background
						for (AActor* A : L->Actors)
						{
							A->SetActorEnableCollision(false);
						}
					}
				}
			}
			else
			{
				// foreground is the scenecapture
				// background is natural rendering
				SceneCapLeftOrFull->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
				if (SceneCapRight)
				{
					SceneCapRight->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
				}

				// set up background levels
				for (ULevel* L : BGLevels)
				{
					// hide all background actors from the foreground scene capture
					SceneCapLeftOrFull->HiddenActors.Append(L->Actors);
					if (SceneCapRight)
					{
						SceneCapRight->HiddenActors.Append(L->Actors);
					}

					if (bEnableCollisionWithBackground == false)
					{
						// no collision on anything in the background
						for (AActor* A : L->Actors)
						{
							A->SetActorEnableCollision(false);
						}
					}
				}

				// look for any custom-tagged components and hide them in the foreground rendering
				for (TActorIterator<AActor> It(MyWorld.Get()); It; ++It)
				{
					AActor* A = *It;
					TArray<UActorComponent*> BGComps = A->GetComponentsByTag(UPrimitiveComponent::StaticClass(), NAME_EmulatorBackground);

					for (UActorComponent* Comp : BGComps)
					{
						UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp);
						if (Prim)
						{
							SceneCapLeftOrFull->HiddenComponents.AddUnique(Prim);
							if (SceneCapRight)
							{
								SceneCapRight->HiddenComponents.AddUnique(Prim);
							}
						}
					}
				}

				// hide all foreground actors from background/normal render 
				TArray<ULevel*> AllLevels = MyWorld->GetLevels();
				for (ULevel* L : AllLevels)
				{
					if (BGLevels.Contains(L))
					{
						continue;
					}

					for (AActor* A : L->Actors)
					{
						if (A)
						{
							TArray<UActorComponent*> BGComps = A->GetComponentsByTag(UPrimitiveComponent::StaticClass(), NAME_EmulatorBackground);
							if (BGComps.Num() > 0)
							{
								TArray<UActorComponent*> AllComps = A->GetComponentsByClass(UPrimitiveComponent::StaticClass());
								for (UActorComponent* C : AllComps)
								{
									if (BGComps.Contains(C) == false)
									{
										UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(C);
										if (PrimComp)
										{
											// hide non-bg components
											NewPlayer->HiddenPrimitiveComponents.Add(PrimComp);
										}
									}
								}
							}
							else
							{
								// hide whole actor
								NewPlayer->HiddenActors.Add(A);
							}
						}
					}

				//	NewPlayer->HiddenActors.Append(L->Actors);
				}

				// we consider spawned objects to always be foreground, so listen for that event and hide them appropriately
				FOnActorSpawned::FDelegate ActorSpawnedDelegate = FOnActorSpawned::FDelegate::CreateRaw(this, &FMagicLeapEmulator::HandleOnActorSpawned);
				OnActorSpawnedHandle = World->AddOnActorSpawnedHandler(ActorSpawnedDelegate);
			}
		}

		// create the camera modifier we will use to composite the scene capture with the normal rendering (via PostProcess material)
		EmulatorCameraModifier = Cast<UEmulatorCameraModifier>(PlayerCamera->AddNewCameraModifier(UEmulatorCameraModifier::StaticClass()));
		if (EmulatorCameraModifier.IsValid())
		{
			EmulatorCameraModifier->InitForEmulation(BGRT_LeftOrFull, BGRT_Right);
		}

		bEmulatorInitialized = true;
	}
}

void FMagicLeapEmulator::HandleOnActorSpawned(AActor* A)
{
	if ((bBackgroundInRenderTarget == false) && MyPlayerController.IsValid())
	{
		//MyPlayerController->HiddenActors.Add(A);

		TArray<UActorComponent*> AllComps = A->GetComponentsByClass(UPrimitiveComponent::StaticClass());
		for (UActorComponent* C : AllComps)
		{
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(C);
			if (Prim)
			{
				if (C->ComponentHasTag(NAME_EmulatorBackground))
				{
					BackgroundSceneCaptureComponent_LeftOrFull->HiddenComponents.AddUnique(Prim);
					if (BackgroundSceneCaptureComponent_Right.IsValid())
					{
						BackgroundSceneCaptureComponent_Right->HiddenComponents.AddUnique(Prim);
					}
				}
				else
				{
					MyPlayerController->HiddenPrimitiveComponents.Add(Prim);
				}
			}
		}

// 
// 		// look for any custom-tagged components and hide them in the foreground rendering
// 		TArray<UActorComponent*> BGComps = A->GetComponentsByTag(UPrimitiveComponent::StaticClass(), NAME_EmulatorBackground);
// 		for (UActorComponent* Comp : BGComps)
// 		{
// 			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp);
// 			if (Prim)
// 			{
// 				BackgroundSceneCaptureComponent_LeftOrFull->HiddenComponents.AddUnique(Prim);
// 				if (BackgroundSceneCaptureComponent_Right.IsValid())
// 				{
// 					BackgroundSceneCaptureComponent_Right->HiddenComponents.AddUnique(Prim);
// 				}
// 			}
// 		}
	}
}

void FMagicLeapEmulator::StopEmulating(UWorld* World)
{
	if (World == MyWorld.Get())
	{
		MyWorld = nullptr;
		EmulatorCameraModifier = nullptr;
		FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginDelegateHandle);
		bEmulatorInitialized = false;
	}
}

static bool bForceUseImplicit = false;

void FMagicLeapEmulator::Tick(float DeltaTime)
{

}

void FMagicLeapEmulator::Update(float DeltaTime)
{
	if (MyWorld.IsValid())
	{
		// update L/R eye locations, in case this changes at runtime (e.g. user changes IPD on the device)
		UGameViewportClient* const VC = MyWorld->GetGameViewport();
		bool const bStereo = GEngine->IsStereoscopic3D(VC->Viewport) && GEngine->StereoRenderingDevice.IsValid();
		if (bStereo && BackgroundSceneCaptureComponent_LeftOrFull.IsValid() && BackgroundSceneCaptureComponent_Right.IsValid())
		{
			static bool bHackLateUpdate = false;
			FQuat HMDRotQ;
			FVector HMDPos;
			if (bHackLateUpdate)
			{
				// test to force a hmd pose refresh, sort of a mini late update
				GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HMDRotQ, HMDPos);
			}
			FRotator HMDRot = HMDRotQ.Rotator();

			FTransform HMDToWorld(HMDRotQ, HMDPos);


			bool bCachedUseImplicit = false;
			if (bForceUseImplicit)
			{
				bCachedUseImplicit = GEngine->XRSystem->GetXRCamera()->GetUseImplicitHMDPosition();
				GEngine->XRSystem->GetXRCamera()->UseImplicitHMDPosition(true);
			}

			static bool bTransformTest = false;
			static bool bSetAbsolute = false;

			FVector ViewLoc;
			FRotator ViewRot;
			MyPlayerController->GetPlayerViewPoint(ViewLoc, ViewRot);

			FVector LeftEyeLoc = ViewLoc;
			FRotator LeftEyeRot = ViewRot;
			GEngine->StereoRenderingDevice->CalculateStereoViewOffset(EStereoscopicPass::eSSP_LEFT_EYE, LeftEyeRot, MyWorld->GetWorldSettings()->WorldToMeters, LeftEyeLoc);

			if (bTransformTest)
			{
				FTransform LeftEyeToWorld(LeftEyeRot, LeftEyeLoc);
				FTransform LeftEyeToHMD = LeftEyeToWorld * HMDToWorld.Inverse();
				FTransform ViewToWorld(ViewRot, ViewLoc);
				FTransform LeftEyeToWorld_ViaView = LeftEyeToHMD * ViewToWorld;
				LeftEyeLoc = LeftEyeToWorld_ViaView.GetLocation();
				LeftEyeRot = LeftEyeToWorld_ViaView.Rotator();
			}
			BackgroundSceneCaptureComponent_LeftOrFull->SetWorldLocationAndRotation(LeftEyeLoc, LeftEyeRot);
			BackgroundSceneCaptureComponent_LeftOrFull->SetAbsolute(bSetAbsolute, bSetAbsolute, bSetAbsolute);

			FVector RightEyeLoc = ViewLoc;
			FRotator RightEyeRot = ViewRot;
			GEngine->StereoRenderingDevice->CalculateStereoViewOffset(EStereoscopicPass::eSSP_RIGHT_EYE, RightEyeRot, MyWorld->GetWorldSettings()->WorldToMeters, RightEyeLoc);
			if (bTransformTest)
			{
				FTransform RightEyeToWorld(RightEyeRot, RightEyeLoc);
				FTransform RightEyeToHMD = RightEyeToWorld * HMDToWorld.Inverse();
				FTransform ViewToWorld(ViewRot, ViewLoc);
				FTransform RightEyeToWorld_ViaView = RightEyeToHMD * ViewToWorld;
				RightEyeLoc = RightEyeToWorld_ViaView.GetLocation();
				RightEyeRot = RightEyeToWorld_ViaView.Rotator();
			}
			BackgroundSceneCaptureComponent_Right->SetWorldLocationAndRotation(RightEyeLoc, RightEyeRot);
			BackgroundSceneCaptureComponent_LeftOrFull->SetAbsolute(bSetAbsolute, bSetAbsolute, bSetAbsolute);

			UE_LOG(LogTemp, Log, TEXT("MLE: BaseView is loc=%s, rot=%s"), *ViewLoc.ToString(), *ViewRot.ToString());
			UE_LOG(LogTemp, Log, TEXT("MLE: Left/0 is loc=%s, rot=%s"), *LeftEyeLoc.ToString(), *LeftEyeRot.ToString());
			UE_LOG(LogTemp, Log, TEXT("MLE: Right/1 is loc=%s, rot=%s"), *RightEyeLoc.ToString(), *RightEyeRot.ToString());

			// #hack
			if (bForceUseImplicit)
			{
				GEngine->XRSystem->GetXRCamera()->UseImplicitHMDPosition(bCachedUseImplicit);
			}

			float Unused = 90.f;
			FMatrix M_Left = GEngine->StereoRenderingDevice->GetStereoProjectionMatrix(EStereoscopicPass::eSSP_LEFT_EYE);
			BackgroundSceneCaptureComponent_LeftOrFull->bUseCustomProjectionMatrix = true;
			BackgroundSceneCaptureComponent_LeftOrFull->CustomProjectionMatrix = M_Left;

			FMatrix M_Right = GEngine->StereoRenderingDevice->GetStereoProjectionMatrix(EStereoscopicPass::eSSP_RIGHT_EYE);
			BackgroundSceneCaptureComponent_Right->bUseCustomProjectionMatrix = true;
			BackgroundSceneCaptureComponent_Right->CustomProjectionMatrix = M_Right;

			static bool bManuallyCapture = false;
			if (bManuallyCapture)
			{
				BackgroundSceneCaptureComponent_LeftOrFull->bCaptureEveryFrame = false;
				BackgroundSceneCaptureComponent_LeftOrFull->bCaptureOnMovement = false;
				BackgroundSceneCaptureComponent_LeftOrFull->CaptureScene();

				BackgroundSceneCaptureComponent_Right->bCaptureEveryFrame = false;
				BackgroundSceneCaptureComponent_Right->bCaptureOnMovement = false;
				BackgroundSceneCaptureComponent_Right->CaptureScene();
			}
		}

		// hack test
		else if (BackgroundSceneCaptureComponent_LeftOrFull.IsValid() && BackgroundSceneCaptureComponent_Right.IsValid())
		{
			BackgroundSceneCaptureComponent_LeftOrFull->SetRelativeLocation(FVector(-30.f, 0, 0));
			BackgroundSceneCaptureComponent_Right->SetRelativeLocation(FVector(30.f, 0, 0));
		}
	}
}

static const FName NAME_UMin(TEXT("UMin"));
static const FName NAME_UMin_Right(TEXT("UMin_Right"));
static const FName NAME_VMin(TEXT("VMin"));
static const FName NAME_VMin_Right(TEXT("VMin_Right"));

static const FName NAME_UMax(TEXT("UMax"));
static const FName NAME_UMax_Right(TEXT("UMax_Right"));
static const FName NAME_VMax(TEXT("VMax"));
static const FName NAME_VMax_Right(TEXT("VMax_Right"));


static TAutoConsoleVariable<float> CVarForegroundClipBiasX(
	TEXT("ml.emulator.ForegroundClipBiasX"),
	0.f,
	TEXT(""),
	ECVF_Default);


static TAutoConsoleVariable<float> CVarForegroundClipBiasY(
	TEXT("ml.emulator.ForegroundClipBiasY"),
	0.f,
	TEXT(""),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarForegroundStereoGapBias(
	TEXT("ml.emulator.ForegroundStereoGapBias"),
	0.f,
	TEXT(""),
	ECVF_Default);


void FMagicLeapEmulator::UpdateSceneCaptureTransform(UMagicLeapEmulatorSceneCaptureComponent* Comp)
{
	if (Comp && MyWorld.IsValid())
	{
		EStereoscopicPass const EyePass = (Comp == BackgroundSceneCaptureComponent_LeftOrFull) ? EStereoscopicPass::eSSP_LEFT_EYE : EStereoscopicPass::eSSP_RIGHT_EYE;

		// update L/R eye locations, in case this changes at runtime (e.g. user changes IPD on the device)
		UGameViewportClient* const VC = MyWorld->GetGameViewport();
		bool const bStereo = GEngine->IsStereoscopic3D(VC->Viewport) && GEngine->StereoRenderingDevice.IsValid();
		if (bStereo)
		{
			static bool bHackLateUpdate = false;
			FQuat HMDRotQ;
			FVector HMDPos;
			if (bHackLateUpdate)
			{
				// test to force a hmd pose refresh, sort of a mini late update
				GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HMDRotQ, HMDPos);
			}
			FRotator HMDRot = HMDRotQ.Rotator();

			FTransform HMDToWorld(HMDRotQ, HMDPos);


			bool bCachedUseImplicit = false;
			if (bForceUseImplicit)
			{
				bCachedUseImplicit = GEngine->XRSystem->GetXRCamera()->GetUseImplicitHMDPosition();
				GEngine->XRSystem->GetXRCamera()->UseImplicitHMDPosition(true);
			}

			FVector ViewLoc;
			FRotator ViewRot;
			MyPlayerController->GetPlayerViewPoint(ViewLoc, ViewRot);

			// pass in zero, relative transform will come back out
 			FVector EyeLoc = ViewLoc;
 			FRotator EyeRot = ViewRot;
			GEngine->StereoRenderingDevice->CalculateStereoViewOffset(EyePass, EyeRot, MyWorld->GetWorldSettings()->WorldToMeters, EyeLoc);
			Comp->SetWorldLocationAndRotation(EyeLoc, EyeRot);

// 			UE_LOG(LogTemp, Log, TEXT("MLE: BaseView is loc=%s, rot=%s"), *ViewLoc.ToString(), *ViewRot.ToString());
// 			UE_LOG(LogTemp, Log, TEXT("MLE: Left/0 is loc=%s, rot=%s"), *LeftEyeLoc.ToString(), *LeftEyeRot.ToString());
// 			UE_LOG(LogTemp, Log, TEXT("MLE: Right/1 is loc=%s, rot=%s"), *RightEyeLoc.ToString(), *RightEyeRot.ToString());

			// #hack
			if (bForceUseImplicit)
			{
				GEngine->XRSystem->GetXRCamera()->UseImplicitHMDPosition(bCachedUseImplicit);
			}

			float Unused = 90.f;
			FMatrix ProjMat = GEngine->StereoRenderingDevice->GetStereoProjectionMatrix(EyePass);

			// M[0][0] is 2.f * (1.0f / (Right - Left)) where Right and Left are tan(half_fov)
			// if we assume half_fov_left == -half_fov_right, then 
			// half_fov = atan(1/M[0][0])
			LastProjectionFOVDegrees = FMath::RadiansToDegrees(FMath::Atan(1.f / ProjMat.M[0][0]) * 2.f);
			float TanHorizFOV = 1.f / ProjMat.M[0][0];

			Comp->bUseCustomProjectionMatrix = true;
			Comp->CustomProjectionMatrix = ProjMat;
		}
 		else
 		{
			// nonstereo
			FVector ViewLoc;
			FRotator ViewRot;
			MyPlayerController->GetPlayerViewPoint(ViewLoc, ViewRot);
			LastProjectionFOVDegrees = MyPlayerController->PlayerCameraManager ? MyPlayerController->PlayerCameraManager->GetFOVAngle() : 90.f;
			Comp->SetWorldLocationAndRotation(ViewLoc, ViewRot);
 		}

		if (EmulatorCameraModifier.IsValid() && EmulatorCameraModifier->CompositingMatInst)
		{
			UTextureRenderTarget2D* const RT = Comp->TextureTarget;

			UMagicLeapEmulatorSettings const* const Settings = GetDefault<UMagicLeapEmulatorSettings>();
			if (Settings && Settings->bEnableMagicLeapEmulation)
			{
				if (Settings->bLimitForegroundFOV)
				{
					float const UBias = CVarForegroundClipBiasX.GetValueOnGameThread();
					float const VBias = CVarForegroundClipBiasY.GetValueOnGameThread();

					// #todo, skip the tan and atan above
					float const ForegroundVisibleSizeX = RT->SizeX * (FMath::Tan(FMath::DegreesToRadians(Settings->ForegroundHorizontalFOV * 0.5f)) / FMath::Tan(FMath::DegreesToRadians(LastProjectionFOVDegrees * 0.5f)));
					float const ForegroundVisibleSizeY = ForegroundVisibleSizeX / Settings->ForegroundAspectRatio;

					float UMin = (1.f - (ForegroundVisibleSizeX / RT->SizeX)) * 0.5f;
					float VMin = (1.f - (ForegroundVisibleSizeY / RT->SizeY)) * 0.5f;
					float UMax = 1.f - UMin;
					float VMax = 1.f - VMin;

					// apply bias values
					UMin += UBias;
					UMax += UBias;
					VMin += VBias;
					VMax += VBias;
					if (bStereo)
					{
						float GapBias = CVarForegroundStereoGapBias.GetValueOnGameThread();
						UMin += (EyePass == eSSP_LEFT_EYE) ? -GapBias : GapBias;
						UMax += (EyePass == eSSP_LEFT_EYE) ? -GapBias : GapBias;
					}
					
					/** This the screen V coordinate below which we don't composite the foreground. Nor do we composite V values > 1-VMask */
					EmulatorCameraModifier->CompositingMatInst->SetScalarParameterValue(((EyePass == eSSP_LEFT_EYE) ? NAME_UMin : NAME_UMin_Right), UMin);
					EmulatorCameraModifier->CompositingMatInst->SetScalarParameterValue(((EyePass == eSSP_LEFT_EYE) ? NAME_UMax : NAME_UMax_Right), UMax);
					EmulatorCameraModifier->CompositingMatInst->SetScalarParameterValue(((EyePass == eSSP_LEFT_EYE) ? NAME_VMin : NAME_VMin_Right), VMin);
					EmulatorCameraModifier->CompositingMatInst->SetScalarParameterValue(((EyePass == eSSP_LEFT_EYE) ? NAME_VMax : NAME_VMax_Right), VMax);
				}
				else
				{
					/** This the screen V coordinate below which we don't composite the foreground. Nor do we composite V values > 1-VMask */
					EmulatorCameraModifier->CompositingMatInst->SetScalarParameterValue(((EyePass == eSSP_LEFT_EYE) ? NAME_UMin : NAME_UMin_Right), 0.f);
					EmulatorCameraModifier->CompositingMatInst->SetScalarParameterValue(((EyePass == eSSP_LEFT_EYE) ? NAME_VMin : NAME_VMin_Right), 0.f);
					EmulatorCameraModifier->CompositingMatInst->SetScalarParameterValue(((EyePass == eSSP_LEFT_EYE) ? NAME_UMax : NAME_UMax_Right), 1.f);
					EmulatorCameraModifier->CompositingMatInst->SetScalarParameterValue(((EyePass == eSSP_LEFT_EYE) ? NAME_VMax : NAME_VMax_Right), 1.f);
				}
			}
		}
	}
}

//PRAGMA_ENABLE_OPTIMIZATION


#undef LOCTEXT_NAMESPACE
