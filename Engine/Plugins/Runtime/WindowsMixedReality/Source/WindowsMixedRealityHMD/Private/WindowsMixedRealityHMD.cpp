// Copyright (c) Microsoft Corporation. All rights reserved.

#include "WindowsMixedRealityHMD.h"

#include "WindowsMixedRealityStatics.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Interfaces/IPluginManager.h"
#include "IWindowsMixedRealityHMDPlugin.h"
#include "RHI/Public/PipelineStateCache.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#endif

#include "Engine/GameEngine.h"
#include "Windows/WindowsPlatformMisc.h"
#include "Misc/MessageDialog.h"

// Holographic Remoting is only supported in Windows 10 version 1809 or better
// Originally we were supporting 1803, but there were rendering issues specific to that version so for now we only support 1809
#define MIN_WIN_10_VERSION_FOR_WMR 1809

//---------------------------------------------------
// Windows Mixed Reality HMD Plugin
//---------------------------------------------------

#if WITH_WINDOWS_MIXED_REALITY
class FDepthConversionPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDepthConversionPS, Global);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4);
	}

	FDepthConversionPS() { }

	FDepthConversionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		// Bind shader inputs.
		FarPlaneDistance.Bind(Initializer.ParameterMap, TEXT("FarPlaneDistance"));
		InDepthTexture.Bind(Initializer.ParameterMap, TEXT("InDepthTexture"), SPF_Mandatory);
		InTextureSampler.Bind(Initializer.ParameterMap, TEXT("InTextureSampler"));
	}

	void SetParameters(FRHICommandList& RHICmdList, float farPlaneDistanceValue, FTextureRHIParamRef DepthTexture)
	{
		FPixelShaderRHIParamRef PixelShaderRHI = GetPixelShader();

		SetShaderValue(RHICmdList, PixelShaderRHI, FarPlaneDistance, farPlaneDistanceValue);

		FSamplerStateRHIParamRef SamplerStateRHI = TStaticSamplerState<SF_Point>::GetRHI();
		SetTextureParameter(RHICmdList, PixelShaderRHI, InDepthTexture, InTextureSampler, SamplerStateRHI, DepthTexture);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);

		// Serialize shader inputs.
		Ar << FarPlaneDistance;
		Ar << InDepthTexture;
		Ar << InTextureSampler;

		return bShaderHasOutdatedParameters;
	}

private:
	// Shader parameters.
	FShaderParameter FarPlaneDistance;
	FShaderResourceParameter InDepthTexture;
	FShaderResourceParameter InTextureSampler;
};

IMPLEMENT_SHADER_TYPE(, FDepthConversionPS, TEXT("/Plugin/WindowsMixedReality/Private/DepthConversion.usf"), TEXT("MainPixelShader"), SF_Pixel)
#endif

namespace WindowsMixedReality
{
	class FWindowsMixedRealityHMDPlugin : public IWindowsMixedRealityHMDPlugin
	{
		/** IHeadMountedDisplayModule implementation */
		virtual TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > CreateTrackingSystem() override;

		bool IsHMDConnected()
		{
#if WITH_WINDOWS_MIXED_REALITY
			return HMD && HMD->IsAvailable();
#endif 
			return false;
		}

		FString GetModuleKeyName() const override
		{
			return FString(TEXT("WindowsMixedRealityHMD"));
		}

		void StartupModule() override
		{
#if WITH_WINDOWS_MIXED_REALITY
			IHeadMountedDisplayModule::StartupModule();

			// Get the base directory of this plugin
			FString BaseDir = IPluginManager::Get().FindPlugin("WindowsMixedReality")->GetBaseDir();

			FString EngineDir = FPaths::EngineDir();
			FString BinariesSubDir = FPlatformProcess::GetBinariesSubdirectory();

			FString PerceptionSimulationDLLPath = EngineDir / "Binaries" / BinariesSubDir / "Microsoft.Perception.Simulation.dll";
			FString HolographicStreamerDesktopDLLPath = EngineDir / "Binaries" / BinariesSubDir / "HolographicStreamerDesktop.dll";
			FString MRInteropLibraryPath = BaseDir / "Binaries/ThirdParty/MixedRealityInteropLibrary" / BinariesSubDir / "MixedRealityInterop.dll";

			// Load these dependencies first or MixedRealityInteropLibraryHandle fails to load since it doesn't look in the correct path for its dependencies automatically
			void* PerceptionSimulationDLLHandle = FPlatformProcess::GetDllHandle(*PerceptionSimulationDLLPath);
			void* HolographicStreamerDesktopDLLHandle = FPlatformProcess::GetDllHandle(*HolographicStreamerDesktopDLLPath);

			// Then finally try to load the WMR Interop Library
			void* MixedRealityInteropLibraryHandle = !MRInteropLibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*MRInteropLibraryPath) : nullptr;

			FString OSVersionLabel;
			FString OSSubVersionLabel;
			FWindowsPlatformMisc::GetOSVersions(OSVersionLabel, OSSubVersionLabel);
			// GetOSVersion returns the Win10 release version in the OSVersion rather than the OSSubVersion, so parse it out ourselves
			OSSubVersionLabel = OSVersionLabel;
			bool bHasSupportedWindowsVersion = OSSubVersionLabel.RemoveFromStart("Windows 10 (Release ") && OSSubVersionLabel.RemoveFromEnd(")") && (FCString::Atoi(*OSSubVersionLabel) >= MIN_WIN_10_VERSION_FOR_WMR);
			if (MixedRealityInteropLibraryHandle && bHasSupportedWindowsVersion)
			{
				HMD = new MixedRealityInterop();
			}
			else
			{
				FText ErrorText = FText::Format(FTextFormat(NSLOCTEXT("WindowsMixedRealityHMD", "MixedRealityInteropLibraryError", 
					"Failed to load Windows Mixed Reality Interop Library, or this version of Windows is not supported. \nNote: UE4 only supports Windows Mixed Reality on Windows 10 Release {0} or higher. Current version: {1}")),
					FText::FromString(FString::FromInt(MIN_WIN_10_VERSION_FOR_WMR)), FText::FromString(OSVersionLabel));
				FMessageDialog::Open(EAppMsgType::Ok, ErrorText);
				UE_LOG(LogCore, Error, TEXT("%s"), *ErrorText.ToString());
			}
			FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("WindowsMixedReality"))->GetBaseDir(), TEXT("Shaders"));
			AddShaderSourceDirectoryMapping(TEXT("/Plugin/WindowsMixedReality"), PluginShaderDir);
#else
			UE_LOG(LogCore, Error, TEXT("Windows Mixed Reality compiled with unsupported compiler.  Please recompile with Visual Studio 2017"));
#endif
		}

		void ShutdownModule() override
		{
#if WITH_WINDOWS_MIXED_REALITY
			if (HMD)
			{
				HMD->Dispose(true);
				delete HMD;
				HMD = nullptr;
			}
#endif
		}

		uint64 GetGraphicsAdapterLuid() override
		{
#if WITH_WINDOWS_MIXED_REALITY
			if (HMD)
			{
				return HMD->GraphicsAdapterLUID();
			}
#endif
			return 0;
		}

#if WITH_WINDOWS_MIXED_REALITY
		MixedRealityInterop* HMD = nullptr;
#endif
	};

	TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > FWindowsMixedRealityHMDPlugin::CreateTrackingSystem()
	{
#if WITH_WINDOWS_MIXED_REALITY
		if (HMD)
		{
			auto WindowsMRHMD = FSceneViewExtensions::NewExtension<WindowsMixedReality::FWindowsMixedRealityHMD>(HMD);
			if (WindowsMRHMD->IsInitialized())
			{
				return WindowsMRHMD;
			}
		}
#endif
		return nullptr;
	}

	float FWindowsMixedRealityHMD::GetWorldToMetersScale() const
	{
		return CachedWorldToMetersScale;
	}

	//---------------------------------------------------
	// FWindowsMixedRealityHMD IHeadMountedDisplay Implementation
	//---------------------------------------------------

	bool FWindowsMixedRealityHMD::IsHMDConnected()
	{
#if WITH_WINDOWS_MIXED_REALITY
		if (HMD->IsRemoting())
		{
			return true;
		}

		return HMD->IsAvailable();
#else
		return false;
#endif
	}

	bool FWindowsMixedRealityHMD::IsHMDEnabled() const
	{
		return true;
	}

	EHMDWornState::Type FWindowsMixedRealityHMD::GetHMDWornState()
	{
#if WITH_WINDOWS_MIXED_REALITY
		if (HMD->IsRemoting())
		{
			return EHMDWornState::Type::Unknown;
		}

		MixedRealityInterop::UserPresence currentPresence = HMD->GetCurrentUserPresence();

		EHMDWornState::Type wornState = EHMDWornState::Type::Unknown;

		switch (currentPresence)
		{
		case MixedRealityInterop::UserPresence::Worn:
			wornState = EHMDWornState::Type::Worn;
			break;
		case MixedRealityInterop::UserPresence::NotWorn:
			wornState = EHMDWornState::Type::NotWorn;
			break;
		};

		return wornState;
#else
		return EHMDWornState::Unknown;
#endif
	}

	void FWindowsMixedRealityHMD::OnBeginPlay(FWorldContext & InWorldContext)
	{
		EnableStereo(true);
	}

	void FWindowsMixedRealityHMD::OnEndPlay(FWorldContext & InWorldContext)
	{
		EnableStereo(false);
	}

	TRefCountPtr<ID3D11Device> FWindowsMixedRealityHMD::InternalGetD3D11Device()
	{
		if (!D3D11Device.IsValid())
		{
			FWindowsMixedRealityHMD* Self = this;
			ENQUEUE_RENDER_COMMAND(InternalGetD3D11DeviceCmd)([Self](FRHICommandListImmediate& RHICmdList)
			{
				Self->D3D11Device = (ID3D11Device*)RHIGetNativeDevice();
			});

			FlushRenderingCommands();
		}

		return D3D11Device;
	}

	/** Helper function for acquiring the appropriate FSceneViewport */
	FSceneViewport* FindMRSceneViewport(bool& allowStereo)
	{
		allowStereo = true;

		if (!GIsEditor)
		{
			UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);

			if (GameEngine->SceneViewport.Get() != nullptr)
			{
				allowStereo = GameEngine->SceneViewport.Get()->IsStereoRenderingAllowed();
			}

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
				allowStereo = PIEViewport->IsStereoRenderingAllowed();
				return PIEViewport;
			}
			else
			{
				// Check to see if the active editor viewport is drawing in stereo mode
				FSceneViewport* EditorViewport = (FSceneViewport*)EditorEngine->GetActiveViewport();
				if (EditorViewport != nullptr && EditorViewport->IsStereoRenderingAllowed())
				{
					allowStereo = EditorViewport->IsStereoRenderingAllowed();
					return EditorViewport;
				}
			}
		}
#endif

		allowStereo = false;
		return nullptr;
	}

	FString FWindowsMixedRealityHMD::GetVersionString() const
	{
#if WITH_WINDOWS_MIXED_REALITY
		return FString(HMD->GetDisplayName());
#else
		return FString();
#endif
	}

	void CenterMouse(RECT windowRect)
	{
		int width = windowRect.right - windowRect.left;
		int height = windowRect.bottom - windowRect.top;

		SetCursorPos(windowRect.left + width / 2, windowRect.top + height / 2);
	}

	bool FWindowsMixedRealityHMD::OnStartGameFrame(FWorldContext & WorldContext)
	{
		if (this->bRequestRestart)
		{
			this->bRequestRestart = false;

			ShutdownHolographic();
			EnableStereo(true);

			return true;
		}

#if WITH_WINDOWS_MIXED_REALITY
		if (!HMD->IsInitialized())
		{
			D3D11Device = InternalGetD3D11Device();
			HMD->Initialize(D3D11Device.GetReference(),
				GNearClippingPlane / GetWorldToMetersScale(), farPlaneDistance);
			return true;
		}
		else
		{
			if (!HMD->IsRemoting() && !HMD->IsImmersiveWindowValid())
			{
				// This can happen if the PC went to sleep.
				this->bRequestRestart = true;
				return true;
			}
		}

		if (HMD->IsRemoting() && !bIsStereoDesired)
		{
			EnableStereo(true);
		}

		if (!bIsStereoEnabled && bIsStereoDesired)
		{
			// Set up the HMD
			SetupHolographicCamera();
		}

		if (!HMD->IsRemoting() && HMD->HasUserPresenceChanged())
		{
			currentWornState = GetHMDWornState();

			// Broadcast HMD worn/ not worn delegates.
			if (currentWornState == EHMDWornState::Worn)
			{
				FCoreDelegates::VRHeadsetPutOnHead.Broadcast();
			}
			else if (currentWornState == EHMDWornState::NotWorn)
			{
				FCoreDelegates::VRHeadsetRemovedFromHead.Broadcast();
			}
		}

		if (GEngine
			&& GEngine->GameViewport
			&& GEngine->GameViewport->GetWindow().IsValid())
		{
			HWND gameHWND = (HWND)GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
			if (IsWindow(gameHWND))
			{
				RECT windowRect;
				GetWindowRect(gameHWND, &windowRect);

				gameWindowWidth = windowRect.right - windowRect.left;
				gameWindowHeight = windowRect.bottom - windowRect.top;

				// Restore windows focus to game window to preserve keyboard/mouse input.
				if ((currentWornState == EHMDWornState::Type::Worn) && GEngine)
				{
					// Set mouse focus to center of game window so any clicks interact with the game.
					if (mouseLockedToCenter)
					{
						CenterMouse(windowRect);
					}

					if (GetCapture() != gameHWND)
					{
						// Keyboard input
						SetForegroundWindow(gameHWND);

						// Mouse input
						SetCapture(gameHWND);
						SetFocus(gameHWND);

						FSlateApplication::Get().SetAllUserFocusToGameViewport();
					}
				}
			}
		}

		CachedWorldToMetersScale = WorldContext.World()->GetWorldSettings()->WorldToMeters;
#endif

		return true;
	}

	void FWindowsMixedRealityHMD::SetTrackingOrigin(EHMDTrackingOrigin::Type NewOrigin)
	{
		HMDTrackingOrigin = NewOrigin;
	}

	EHMDTrackingOrigin::Type FWindowsMixedRealityHMD::GetTrackingOrigin()
	{
		return HMDTrackingOrigin;
	}

	bool FWindowsMixedRealityHMD::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type)
	{
		if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
		{
			OutDevices.Add(IXRTrackingSystem::HMDDeviceId);
			return true;
		}
		return false;
	}

	void FWindowsMixedRealityHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
	{
		ipd = NewInterpupillaryDistance;
	}

	float FWindowsMixedRealityHMD::GetInterpupillaryDistance() const
	{
		if (ipd == 0)
		{
			return 0.064f;
		}

		return ipd;
	}

	bool FWindowsMixedRealityHMD::GetCurrentPose(
		int32 DeviceId,
		FQuat& CurrentOrientation,
		FVector& CurrentPosition)
	{
		if (DeviceId != IXRTrackingSystem::HMDDeviceId)
		{
			return false;
		}

		// Get most recently available tracking data.
		InitTrackingFrame();

		CurrentOrientation = CurrOrientation;
		CurrentPosition = CurrPosition;

		return true;
	}

	bool FWindowsMixedRealityHMD::GetRelativeEyePose(int32 DeviceId, EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition)
	{
		OutOrientation = FQuat::Identity;
		OutPosition = FVector::ZeroVector;
		if (DeviceId == IXRTrackingSystem::HMDDeviceId && (Eye == eSSP_LEFT_EYE || Eye == eSSP_RIGHT_EYE))
		{
			OutPosition = FVector(0, (Eye == EStereoscopicPass::eSSP_LEFT_EYE ? 0.5f : -0.5f) * GetInterpupillaryDistance() * GetWorldToMetersScale(), 0);
			return true;
		}
		else
		{
			return false;
		}
	}

	void FWindowsMixedRealityHMD::ResetOrientationAndPosition(float yaw)
	{
#if WITH_WINDOWS_MIXED_REALITY
		HMD->ResetOrientationAndPosition();
#endif
	}

	void FWindowsMixedRealityHMD::InitTrackingFrame()
	{
#if WITH_WINDOWS_MIXED_REALITY
		DirectX::XMMATRIX leftPose;
		DirectX::XMMATRIX rightPose;
		MixedRealityInterop::HMDTrackingOrigin trackingOrigin;
		if (HMD->GetCurrentPose(leftPose, rightPose, trackingOrigin))
		{
			trackingOrigin == MixedRealityInterop::HMDTrackingOrigin::Eye ?
				SetTrackingOrigin(EHMDTrackingOrigin::Eye) :
				SetTrackingOrigin(EHMDTrackingOrigin::Floor);

			// Convert to unreal space
			FMatrix UPoseL = FWindowsMixedRealityStatics::ToFMatrix(leftPose);
			FMatrix UPoseR = FWindowsMixedRealityStatics::ToFMatrix(rightPose);
			RotationL = FQuat(UPoseL);
			RotationR = FQuat(UPoseR);

			RotationL = FQuat(-1 * RotationL.Z, RotationL.X, RotationL.Y, -1 * RotationL.W);
			RotationR = FQuat(-1 * RotationR.Z, RotationR.X, RotationR.Y, -1 * RotationR.W);

			RotationL.Normalize();
			RotationR.Normalize();

			FQuat HeadRotation = FMath::Lerp(RotationL, RotationR, 0.5f);
			HeadRotation.Normalize();

			// Position = forward/ backwards, left/ right, up/ down.
			PositionL = ((FVector(UPoseL.M[2][3], -1 * UPoseL.M[0][3], -1 * UPoseL.M[1][3]) * GetWorldToMetersScale()));
			PositionR = ((FVector(UPoseR.M[2][3], -1 * UPoseR.M[0][3], -1 * UPoseR.M[1][3]) * GetWorldToMetersScale()));

			PositionL = RotationL.RotateVector(PositionL);
			PositionR = RotationR.RotateVector(PositionR);

			if (ipd == 0)
			{
				ipd = FVector::Dist(PositionL, PositionR) / GetWorldToMetersScale();
			}

			FVector HeadPosition = FMath::Lerp(PositionL, PositionR, 0.5f);

			CurrOrientation = HeadRotation;
			CurrPosition = HeadPosition;
		}
#endif
	}

#if WITH_WINDOWS_MIXED_REALITY
	void SetupHiddenVisibleAreaMesh(TArray<FHMDViewMesh>& HiddenMeshes, TArray<FHMDViewMesh>& VisibleMeshes, MixedRealityInterop* HMD)
	{
		for (int i = (int)MixedRealityInterop::HMDEye::Left;
			i <= (int)MixedRealityInterop::HMDEye::Right; i++)
		{
			MixedRealityInterop::HMDEye eye = (MixedRealityInterop::HMDEye)i;

			DirectX::XMFLOAT2* vertices;
			int length;
			if (HMD->GetHiddenAreaMesh(eye, vertices, length))
			{
				FVector2D* const vertexPositions = new FVector2D[length];
				for (int v = 0; v < length; v++)
				{
					// Remap to space Unreal is expecting.
					float x = (vertices[v].x + 1) / 2.0f;
					float y = (vertices[v].y + 1) / 2.0f;

					vertexPositions[v].Set(x, y);
				}
				HiddenMeshes[i].BuildMesh(vertexPositions, length, FHMDViewMesh::MT_HiddenArea);

				delete[] vertexPositions;
			}

			if (HMD->GetVisibleAreaMesh(eye, vertices, length))
			{
				FVector2D* const vertexPositions = new FVector2D[length];
				for (int v = 0; v < length; v++)
				{
					// Remap from NDC space to [0..1] bottom-left origin.
					float x = (vertices[v].x + 1) / 2.0f;
					float y = (vertices[v].y + 1) / 2.0f;

					vertexPositions[v].Set(x, y);
				}
				VisibleMeshes[i].BuildMesh(vertexPositions, length, FHMDViewMesh::MT_VisibleArea);

				delete[] vertexPositions;
			}
		}
	}
#endif

	void FWindowsMixedRealityHMD::SetupHolographicCamera()
	{
#if WITH_WINDOWS_MIXED_REALITY
		// Set the viewport to match the HMD display
		FSceneViewport* SceneVP = FindMRSceneViewport(bIsStereoDesired);

		if (SceneVP)
		{
			TSharedPtr<SWindow> Window = SceneVP->FindWindow();
			if (Window.IsValid() && SceneVP->GetViewportWidget().IsValid())
			{
				if (bIsStereoDesired)
				{
					int Width, Height;
					if (HMD->GetDisplayDimensions(Width, Height))
					{
						SceneVP->SetViewportSize(
							Width * 2,
							Height);

						Window->SetViewportSizeDrivenByWindow(false);

						bIsStereoEnabled = HMD->IsStereoEnabled();
						if (bIsStereoEnabled)
						{
							HMD->CreateHiddenVisibleAreaMesh();

							FWindowsMixedRealityHMD* Self = this;
							ENQUEUE_RENDER_COMMAND(SetupHiddenVisibleAreaMeshCmd)([Self](FRHICommandListImmediate& RHICmdList)
							{
								SetupHiddenVisibleAreaMesh(Self->HiddenAreaMesh, Self->VisibleAreaMesh, Self->HMD);
							});
						}
					}
				}
				else
				{
					FVector2D size = SceneVP->FindWindow()->GetSizeInScreen();
					SceneVP->SetViewportSize(size.X, size.Y);
					Window->SetViewportSizeDrivenByWindow(true);
					bIsStereoEnabled = false;
				}
			}
		}
		else if (GIsEditor && HMD->IsInitialized() && bIsStereoDesired && !bIsStereoEnabled)
		{
			// This can happen if device is disconnected while running in VR Preview, then create a new VR preview window while device is still disconnected.
			// We can get a window that is not configured for stereo when we plug our device back in.
			this->bRequestRestart = true;
		}

		// Uncap fps to enable FPS higher than 62
		GEngine->bForceDisableFrameRateSmoothing = bIsStereoEnabled;
#endif
	}

	bool FWindowsMixedRealityHMD::IsStereoEnabled() const
	{
		return bIsStereoEnabled;
	}

	bool FWindowsMixedRealityHMD::EnableStereo(bool stereo)
	{
#if WITH_WINDOWS_MIXED_REALITY
		if (stereo)
		{
			if (bIsStereoDesired && HMD->IsInitialized())
			{
				return false;
			}

			FindMRSceneViewport(bIsStereoDesired);
			if (!bIsStereoDesired)
			{
				return false;
			}

			HMD->EnableStereo(stereo);

			InitializeHolographic();

			currentWornState = GetHMDWornState();

			FApp::SetUseVRFocus(true);
			FApp::SetHasVRFocus(true);
		}
		else
		{
			ShutdownHolographic();

			FApp::SetUseVRFocus(false);
			FApp::SetHasVRFocus(false);
		}
#endif
		return bIsStereoDesired;
	}

	void FWindowsMixedRealityHMD::AdjustViewRect(
		EStereoscopicPass StereoPass,
		int32& X, int32& Y,
		uint32& SizeX, uint32& SizeY) const
	{
		SizeX *= ScreenScalePercentage;
		SizeY *= ScreenScalePercentage;

		SizeX = SizeX / 2;
		if (StereoPass == eSSP_RIGHT_EYE)
		{
			X += SizeX;
		}
	}

	void FWindowsMixedRealityHMD::CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
	{
		if (StereoPassType != eSSP_LEFT_EYE &&
			StereoPassType != eSSP_RIGHT_EYE)
		{
			return;
		}

		FVector HmdToEyeOffset = FVector::ZeroVector;

		FQuat CurEyeOrient;
		if (StereoPassType == eSSP_LEFT_EYE)
		{
			HmdToEyeOffset = PositionL - CurrPosition;
			CurEyeOrient = RotationL;
		}
		else if (StereoPassType == eSSP_RIGHT_EYE)
		{
			HmdToEyeOffset = PositionR - CurrPosition;
			CurEyeOrient = RotationR;
		}

		const FQuat ViewOrient = ViewRotation.Quaternion();
		const FQuat deltaControlOrientation = ViewOrient * CurEyeOrient.Inverse();
		const FVector vEyePosition = deltaControlOrientation.RotateVector(HmdToEyeOffset);
		ViewLocation += vEyePosition;
	}

	FMatrix FWindowsMixedRealityHMD::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const
	{
		if (StereoPassType != eSSP_LEFT_EYE &&
			StereoPassType != eSSP_RIGHT_EYE)
		{
			return FMatrix::Identity;
		}

#if WITH_WINDOWS_MIXED_REALITY
		DirectX::XMMATRIX projection = (StereoPassType == eSSP_LEFT_EYE)
			? HMD->GetProjectionMatrix(MixedRealityInterop::HMDEye::Left)
			: HMD->GetProjectionMatrix(MixedRealityInterop::HMDEye::Right);

		auto result = FWindowsMixedRealityStatics::ToFMatrix(projection).GetTransposed();
		// Convert from RH to LH projection matrix
		// See PerspectiveOffCenterRH: https://msdn.microsoft.com/en-us/library/windows/desktop/ms918176.aspx
		result.M[2][0] *= -1;
		result.M[2][1] *= -1;
		result.M[2][2] *= -1;
		result.M[2][3] *= -1;

		// Switch to reverse-z, replace near and far distance
		const auto Nz = GNearClippingPlane;
		result.M[2][2] = 0.0f;
		result.M[3][2] = Nz;

		return result;
#else
		return FMatrix::Identity;
#endif
	}

	void FWindowsMixedRealityHMD::GetEyeRenderParams_RenderThread(
		const FRenderingCompositePassContext& Context,
		FVector2D& EyeToSrcUVScaleValue,
		FVector2D& EyeToSrcUVOffsetValue) const
	{
		if (Context.View.StereoPass == eSSP_LEFT_EYE)
		{
			EyeToSrcUVOffsetValue.X = 0.0f;
			EyeToSrcUVOffsetValue.Y = 0.0f;

			EyeToSrcUVScaleValue.X = 0.5f;
			EyeToSrcUVScaleValue.Y = 1.0f;
		}
		else
		{
			EyeToSrcUVOffsetValue.X = 0.5f;
			EyeToSrcUVOffsetValue.Y = 0.0f;

			EyeToSrcUVScaleValue.X = 0.5f;
			EyeToSrcUVScaleValue.Y = 1.0f;
		}
	}

	FIntPoint FWindowsMixedRealityHMD::GetIdealRenderTargetSize() const
	{
		int Width, Height;
#if WITH_WINDOWS_MIXED_REALITY
		HMD->GetDisplayDimensions(Width, Height);
#else
		Width = 100;
		Height = 100;
#endif

		return FIntPoint(Width * 2, Height);
	}

	//TODO: Spelling is intentional, overridden from IHeadMountedDisplay.h
	float FWindowsMixedRealityHMD::GetPixelDenity() const
	{
		check(IsInGameThread());
		return ScreenScalePercentage;
	}

	void FWindowsMixedRealityHMD::SetPixelDensity(const float NewDensity)
	{
		check(IsInGameThread());
		//TODO: Get actual minimum value from platform.
		ScreenScalePercentage = FMath::Clamp<float>(NewDensity, 0.4f, 1.0f);
	}

	// Called when screen size changes.
	void FWindowsMixedRealityHMD::UpdateViewportRHIBridge(bool bUseSeparateRenderTarget, const class FViewport& Viewport, FRHIViewport* const ViewportRHI)
	{
#if WITH_WINDOWS_MIXED_REALITY
		if (IsStereoEnabled() && mCustomPresent != nullptr)
		{
			HMD->SetScreenScaleFactor(ScreenScalePercentage);
			mCustomPresent->UpdateViewport(Viewport, ViewportRHI);
		}
#endif
	}

	void FWindowsMixedRealityHMD::RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture, FVector2D WindowSize) const
	{
		const uint32 WindowWidth = gameWindowWidth;
		const uint32 WindowHeight = gameWindowHeight;

		const uint32 ViewportWidth = BackBuffer->GetSizeX();
		const uint32 ViewportHeight = BackBuffer->GetSizeY();

		const uint32 TextureWidth = SrcTexture->GetSizeX();
		const uint32 TextureHeight = SrcTexture->GetSizeY();

		const uint32 SourceWidth = TextureWidth / 2;
		const uint32 SourceHeight = TextureHeight;

		const float r = (float)SourceWidth / (float)SourceHeight;

		float width = (float)WindowWidth;
		float height = (float)WindowHeight;

		if ((float)WindowWidth / r < WindowHeight)
		{
			width = ViewportWidth;

			float displayHeight = (float)WindowWidth / r;
			height = (float)ViewportHeight * (displayHeight / (float)WindowHeight);
		}
		else // width > height
		{
			height = ViewportHeight;

			float displayWidth = (float)WindowHeight * r;
			width = (float)ViewportWidth * (displayWidth / (float)WindowWidth);
		}

		width = FMath::Clamp<int>(width, 10, ViewportWidth);
		height = FMath::Clamp<int>(height, 10, ViewportHeight);

		const uint32 x = (ViewportWidth - width) * 0.5f;
		const uint32 y = (ViewportHeight - height) * 0.5f;
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		SetRenderTarget(RHICmdList, BackBuffer, FTextureRHIRef());
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		DrawClearQuad(RHICmdList, FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
		RHICmdList.SetViewport(x, y, 0, width + x, height + y, 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		const auto FeatureLevel = GMaxRHIFeatureLevel;
		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTexture);

		RendererModule->DrawRectangle(
			RHICmdList,
			0, 0,
			ViewportWidth, ViewportHeight,
			0.0f, 0.0f,
			0.5f, 1.0f,
			FIntPoint(ViewportWidth, ViewportHeight),
			FIntPoint(1, 1),
			*VertexShader,
			EDRF_Default);
	}

	// Create a BGRA backbuffer for rendering.
	bool FWindowsMixedRealityHMD::AllocateRenderTargetTexture(
		uint32 index,
		uint32 sizeX,
		uint32 sizeY,
		uint8 format,
		uint32 numMips,
		uint32 flags,
		uint32 targetableTextureFlags,
		FTexture2DRHIRef& outTargetableTexture,
		FTexture2DRHIRef& outShaderResourceTexture,
		uint32 numSamples)
	{
		if (!IsStereoEnabled())
		{
			return false;
		}

		FRHIResourceCreateInfo CreateInfo;

		// Since our textures must be BGRA, this plugin did require a change to WindowsD3D11Device.cpp
		// to add the D3D11_CREATE_DEVICE_BGRA_SUPPORT flag to the graphics device.
		RHICreateTargetableShaderResource2D(
			sizeX,
			sizeY,
			PF_B8G8R8A8, // must be BGRA
			numMips,
			flags,
			targetableTextureFlags,
			false,
			CreateInfo,
			outTargetableTexture,
			outShaderResourceTexture);

		CurrentBackBuffer = outTargetableTexture;

		return true;
	}

	bool FWindowsMixedRealityHMD::HasHiddenAreaMesh() const
	{
		return HiddenAreaMesh[0].IsValid() && HiddenAreaMesh[1].IsValid();
	}

	void FWindowsMixedRealityHMD::DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
	{
		if (StereoPass == eSSP_FULL)
		{
			return;
		}

		int index = (StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
		const FHMDViewMesh& Mesh = HiddenAreaMesh[index];
		check(Mesh.IsValid());

		RHICmdList.SetStreamSource(0, Mesh.VertexBufferRHI, 0);
		RHICmdList.DrawIndexedPrimitive(Mesh.IndexBufferRHI, 0, 0, Mesh.NumVertices, 0, Mesh.NumTriangles, 1);
	}

	bool FWindowsMixedRealityHMD::HasVisibleAreaMesh() const
	{
		return VisibleAreaMesh[0].IsValid() && VisibleAreaMesh[1].IsValid();
	}

	void FWindowsMixedRealityHMD::DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
	{
		if (StereoPass == eSSP_FULL)
		{
			return;
		}

		int index = (StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
		const FHMDViewMesh& Mesh = VisibleAreaMesh[index];
		check(Mesh.IsValid());

		RHICmdList.SetStreamSource(0, Mesh.VertexBufferRHI, 0);
		RHICmdList.DrawIndexedPrimitive(Mesh.IndexBufferRHI, 0, 0, Mesh.NumVertices, 0, Mesh.NumTriangles, 1);
	}

	void FWindowsMixedRealityHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
	{
		InViewFamily.EngineShowFlags.MotionBlur = 0;
		InViewFamily.EngineShowFlags.HMDDistortion = false;
		InViewFamily.EngineShowFlags.SetScreenPercentage(false);
		InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();
	}

	void FWindowsMixedRealityHMD::CreateHMDDepthTexture(FRHICommandListImmediate& RHICmdList)
	{
		check(IsInRenderingThread());

#if WITH_WINDOWS_MIXED_REALITY
		// Update depth texture to match format Windows Mixed Reality platform is expecting.
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		FRHITexture2D* depthFRHITexture = SceneContext.GetSceneDepthTexture().GetReference()->GetTexture2D();

		const uint32 viewportWidth = depthFRHITexture->GetSizeX();
		const uint32 viewportHeight = depthFRHITexture->GetSizeY();

		bool recreateTextures = false;
		if (remappedDepthTexture != nullptr)
		{
			int width = remappedDepthTexture->GetSizeX();
			int height = remappedDepthTexture->GetSizeY();

			if (width != viewportWidth || height != viewportHeight)
			{
				recreateTextures = true;
			}
		}

		// Create a new texture for the remapped depth.
		if (remappedDepthTexture == nullptr || recreateTextures)
		{
			FRHIResourceCreateInfo CreateInfo;
			remappedDepthTexture = RHICmdList.CreateTexture2D(depthFRHITexture->GetSizeX(), depthFRHITexture->GetSizeY(),
				PF_R32_FLOAT, 1, 1, ETextureCreateFlags::TexCreate_RenderTargetable | ETextureCreateFlags::TexCreate_UAV, CreateInfo);
		}

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		FRHIRenderPassInfo RPInfo(remappedDepthTexture, ERenderTargetActions::Load_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("RemapDepth"));
		{
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			RHICmdList.SetViewport(0, 0, 0, viewportWidth, viewportHeight, 1.0f);

			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			const auto featureLevel = GMaxRHIFeatureLevel;
			auto shaderMap = GetGlobalShaderMap(featureLevel);

			TShaderMapRef<FScreenVS> vertexShader(shaderMap);
			TShaderMapRef<FDepthConversionPS> pixelShader(shaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*vertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*pixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			pixelShader->SetParameters(RHICmdList, farPlaneDistance / GetWorldToMetersScale(), depthFRHITexture);

			RendererModule->DrawRectangle(
				RHICmdList,
				0, 0, // X, Y
				viewportWidth, viewportHeight, // SizeX, SizeY
				0.0f, 0.0f, // U, V
				1.0f, 1.0f, // SizeU, SizeV
				FIntPoint(viewportWidth, viewportHeight), // TargetSize
				FIntPoint(1, 1), // TextureSize
				*vertexShader,
				EDRF_Default);
		}
		RHICmdList.EndRenderPass();

		ID3D11Device* device = (ID3D11Device*)RHIGetNativeDevice();
		if (device == nullptr)
		{
			return;
		}

		// Create a new depth texture with 2 subresources for depth based reprojection.
		// Directly create an ID3D11Texture2D instead of an FTexture2D because we need an ArraySize of 2.
		if (stereoDepthTexture == nullptr || recreateTextures)
		{
			D3D11_TEXTURE2D_DESC tdesc;

			tdesc.Width = viewportWidth / 2;
			tdesc.Height = viewportHeight;
			tdesc.MipLevels = 1;
			tdesc.ArraySize = 2;
			tdesc.SampleDesc.Count = 1;
			tdesc.SampleDesc.Quality = 0;
			tdesc.Usage = D3D11_USAGE_DEFAULT;
			tdesc.Format = DXGI_FORMAT_R32_FLOAT;
			tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			tdesc.CPUAccessFlags = 0;
			tdesc.MiscFlags = 0;

			device->CreateTexture2D(&tdesc, NULL, &stereoDepthTexture);
		}

		stereoDepthTexture = (ID3D11Texture2D*)remappedDepthTexture->GetNativeResource();
#endif
	}

	void FWindowsMixedRealityHMD::PreRenderViewFamily_RenderThread(
		FRHICommandListImmediate& RHICmdList,
		FSceneViewFamily& InViewFamily)
	{
#if WITH_WINDOWS_MIXED_REALITY
		if (!mCustomPresent || !HMD->IsInitialized() || !HMD->IsAvailable())
		{
			return;
		}

		CreateHMDDepthTexture(RHICmdList);
		if (!HMD->CreateRenderingParameters(stereoDepthTexture))
		{
			// This will happen if an exception is thrown while creating the frame's rendering parameters.
			// Because Windows Mixed Reality can only have 2 rendering parameters in flight at any time, this is fatal.
			this->bRequestRestart = true;
		}
#endif
	}

	bool FWindowsMixedRealityHMD::IsActiveThisFrame(class FViewport* InViewport) const
	{
		return GEngine && GEngine->IsStereoscopic3D(InViewport);
	}

#if WITH_WINDOWS_MIXED_REALITY
	FWindowsMixedRealityHMD::FWindowsMixedRealityHMD(const FAutoRegister& AutoRegister, MixedRealityInterop* InHMD)
		: FHeadMountedDisplayBase(nullptr)
		, FSceneViewExtensionBase(AutoRegister)
		, HMD(InHMD)
		, ScreenScalePercentage(1.0f)
		, mCustomPresent(nullptr)
		, HMDTrackingOrigin(EHMDTrackingOrigin::Floor)
		, CurrOrientation(FQuat::Identity)
		, CurrPosition(FVector::ZeroVector)
	{
		static const FName RendererModuleName("Renderer");
		RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

		HiddenAreaMesh.SetNum(2);
		VisibleAreaMesh.SetNum(2);
	}
#endif

	FWindowsMixedRealityHMD::~FWindowsMixedRealityHMD()
	{
		ShutdownHolographic();
	}

	bool FWindowsMixedRealityHMD::IsInitialized() const
	{
		// Return true here because hmd needs an hwnd to initialize itself, but VR preview will not create a window if this is not true.
		return true;
	}

	// Cleanup resources needed for Windows Holographic view and tracking space.
	void FWindowsMixedRealityHMD::ShutdownHolographic()
	{
		check(IsInGameThread());

#if WITH_WINDOWS_MIXED_REALITY
		HMD->EnableStereo(false);
#endif
		// Ensure that we aren't currently trying to render a frame before destroying our custom present.
		FlushRenderingCommands();
		StopCustomPresent();

		if (PauseHandle.IsValid())
		{
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(PauseHandle);
			PauseHandle.Reset();
		}

		bIsStereoDesired = false;
		bIsStereoEnabled = false;

		for (int i = 0; i < 2; i++)
		{
			HiddenAreaMesh[i].NumVertices = 0;
			HiddenAreaMesh[i].NumIndices = 0;
			HiddenAreaMesh[i].NumTriangles = 0;

			HiddenAreaMesh[i].IndexBufferRHI = nullptr;
			HiddenAreaMesh[i].VertexBufferRHI = nullptr;

			VisibleAreaMesh[i].NumVertices = 0;
			VisibleAreaMesh[i].NumIndices = 0;
			VisibleAreaMesh[i].NumTriangles = 0;

			VisibleAreaMesh[i].IndexBufferRHI = nullptr;
			VisibleAreaMesh[i].VertexBufferRHI = nullptr;
		}
	}

	bool FWindowsMixedRealityHMD::IsCurrentlyImmersive()
	{
#if WITH_WINDOWS_MIXED_REALITY
		return HMD->IsCurrentlyImmersive();
#else
		return false;
#endif
	}

	// Setup Windows Holographic view and tracking space.
	void FWindowsMixedRealityHMD::InitializeHolographic()
	{
#if WITH_WINDOWS_MIXED_REALITY
		if (!HMD->IsInitialized())
		{
			D3D11Device = InternalGetD3D11Device();
			if (D3D11Device != nullptr)
			{
				SetupHolographicCamera();
			}
		}

		static const auto screenPercentVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("vr.PixelDensity"));
		SetPixelDensity(screenPercentVar->GetValueOnGameThread());

		StartCustomPresent();

		// Hook into suspend/resume events
		if (!PauseHandle.IsValid())
		{
			PauseHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FWindowsMixedRealityHMD::AppServicePause);
		}
#endif
	}

	// Prevent crashes if computer goes to sleep.
	void FWindowsMixedRealityHMD::AppServicePause()
	{
		this->bRequestRestart = true;
	}

	bool WindowsMixedReality::FWindowsMixedRealityHMD::IsAvailable()
	{
#if WITH_WINDOWS_MIXED_REALITY
		return HMD->IsAvailable();
#else
		return false;
#endif
	}

	// Initialize Windows Holographic present.
	void FWindowsMixedRealityHMD::StartCustomPresent()
	{
#if WITH_WINDOWS_MIXED_REALITY
		if (mCustomPresent == nullptr)
		{
			mCustomPresent = new FWindowsMixedRealityCustomPresent(HMD, D3D11Device);
		}
#endif
	}

	// Cleanup resources for holographic present.
	void FWindowsMixedRealityHMD::StopCustomPresent()
	{
		mCustomPresent = nullptr;
	}

	// Spatial Input
	bool FWindowsMixedRealityHMD::SupportsSpatialInput()
	{
#if WITH_WINDOWS_MIXED_REALITY
		return HMD->SupportsSpatialInput();
#else
		return false;
#endif
	}

#if WITH_WINDOWS_MIXED_REALITY
	MixedRealityInterop::HMDTrackingStatus FWindowsMixedRealityHMD::GetControllerTrackingStatus(MixedRealityInterop::HMDHand hand)
	{
		return HMD->GetControllerTrackingStatus(hand);
	}

	bool FWindowsMixedRealityHMD::GetControllerOrientationAndPosition(MixedRealityInterop::HMDHand hand, FRotator & OutOrientation, FVector & OutPosition)
	{
		if (!bIsStereoEnabled)
		{
			return false;
		}

		DirectX::XMFLOAT4 rot;
		DirectX::XMFLOAT3 pos;
		if (HMD->GetControllerOrientationAndPosition(hand, rot, pos))
		{
			OutOrientation = FRotator(FWindowsMixedRealityStatics::FromMixedRealityQuaternion(rot));
			OutPosition = FWindowsMixedRealityStatics::FromMixedRealityVector(pos);

			// HoloLens does not have hand rotations, so default to the player camera rotation.
			if (HMD->IsRemoting())
			{
				OutOrientation = FRotator(CurrOrientation);
				OutOrientation.Roll = 0;
				OutOrientation.Pitch = 0;
			}

			return true;
		}
		return false;
	}

	bool FWindowsMixedRealityHMD::PollInput()
	{
		if (!bIsStereoEnabled)
		{
			return false;
		}

		HMD->PollInput();
		return true;
	}

	MixedRealityInterop::HMDInputPressState WindowsMixedReality::FWindowsMixedRealityHMD::GetPressState(MixedRealityInterop::HMDHand hand, MixedRealityInterop::HMDInputControllerButtons button)
	{
		return HMD->GetPressState(hand, button);
	}

	float FWindowsMixedRealityHMD::GetAxisPosition(MixedRealityInterop::HMDHand hand, MixedRealityInterop::HMDInputControllerAxes axis)
	{
		return HMD->GetAxisPosition(hand, axis);
	}

	void FWindowsMixedRealityHMD::SubmitHapticValue(MixedRealityInterop::HMDHand hand, float value)
	{
		HMD->SubmitHapticValue(hand, FMath::Clamp(value, 0.f, 1.f));
	}
#endif

	// Remoting
	void FWindowsMixedRealityHMD::ConnectToRemoteHoloLens(const wchar_t* ip, unsigned int bitrate)
	{
#if WITH_EDITOR
		D3D11Device = InternalGetD3D11Device();

#if WITH_WINDOWS_MIXED_REALITY
		HMD->ConnectToRemoteHoloLens(D3D11Device.GetReference(), ip, bitrate);
#endif
#endif
	}

	void FWindowsMixedRealityHMD::DisconnectFromRemoteHoloLens()
	{
#if WITH_EDITOR
#if WITH_WINDOWS_MIXED_REALITY
		HMD->DisconnectFromRemoteHoloLens();
#endif
#endif
	}
}

IMPLEMENT_MODULE(WindowsMixedReality::FWindowsMixedRealityHMDPlugin, WindowsMixedRealityHMD)
