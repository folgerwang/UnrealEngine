// Copyright (c) Microsoft Corporation. All rights reserved.

// To use this lib in engines that do not build cppwinrt:
// WinRT headers and types must be in the cpp and not the header.

#include "stdafx.h"
#include "MixedRealityInterop.h"

#include "wrl/client.h"
#include "wrl/wrappers/corewrappers.h"

#include <Roapi.h>
#include <queue>

#include "winrt/Windows.Devices.Haptics.h"
#include "winrt/Windows.Perception.h"
#include "winrt/Windows.Perception.Spatial.h"
#include "winrt/Windows.UI.Input.Spatial.h"
#include "winrt/Windows.Foundation.Numerics.h"

#include <HolographicSpaceInterop.h>
#include <SpatialInteractionManagerInterop.h>
#include <Windows.Graphics.Holographic.h>
#include <windows.ui.input.spatial.h>

#include <DXGI1_4.h>

#include "winrt/Windows.Graphics.Holographic.h"
#include "winrt/Windows.Graphics.DirectX.Direct3D11.h"
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>

// Remoting
#include "HolographicStreamerHelpers.h"

#pragma comment(lib, "OneCore")

using namespace Microsoft::WRL;
using namespace Microsoft::Holographic;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Foundation::Numerics;

using namespace winrt::Windows::Devices::Haptics;
using namespace winrt::Windows::Graphics::Holographic;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::UI::Input::Spatial;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::Perception;

namespace WindowsMixedReality
{
	bool bInitialized = false;

	// Since WinRT types cannot be included in the header, 
	// we are declaring classes using WinRT types in this cpp file.
	// Forward declare the relevant classes here to keep variables at the top of the class.
	class TrackingFrame;
	class HolographicFrameResources;
	class HolographicCameraResources;

	// WinRT types must be declared inside of the cpp.
	HolographicSpace holographicSpace = nullptr;
	SpatialLocator Locator = nullptr;
	IDirect3DDevice InteropD3DDevice;

	SpatialInteractionManager interactionManager = nullptr;

	// Reference Frames
	SpatialStationaryFrameOfReference StationaryReferenceFrame = nullptr;
	SpatialStageFrameOfReference StageReferenceFrame = nullptr;
	SpatialLocatorAttachedFrameOfReference AttachedReferenceFrame = nullptr;

	// Tracking frames.
	TrackingFrame* currentFrame = nullptr;
	HolographicFrameResources* CurrentFrameResources = nullptr;
	float4x4 LastKnownCoordinateSystemTransform = float4x4::identity();
	HolographicStereoTransform LastKnownProjection;
	std::mutex poseLock;
	std::mutex disposeLock_GetProjection;
	std::mutex disposeLock_Present;

	HWND stereoWindowHandle;

	// Event registration tokens declared in cpp since events surface WinRT types.
	winrt::event_token CameraAddedToken;
	winrt::event_token CameraRemovedToken;
	winrt::event_token LocatabilityChangedToken;
	winrt::event_token StageChangedEventToken;
	winrt::event_token UserPresenceChangedToken;

	MixedRealityInterop::UserPresence currentUserPresence = MixedRealityInterop::UserPresence::Unknown;
	// Default to true to get worn state on first load.
	bool userPresenceChanged = true;
	std::mutex PresenceLock;

	// Variables used from event handlers must be declared inside of the cpp.
	// Camera resources.
	float nearPlaneDistance = 0.001f;
	float farPlaneDistance = 100000.0f;
	float ScreenScaleFactor = 1.0f;
	std::unique_ptr<HolographicCameraResources> CameraResources = nullptr;
	std::mutex CameraResourcesLock;
	std::mutex StageLock;

	const float defaultPlayerHeight = -1.8f;

	// Hidden Area Mesh
	std::vector<DirectX::XMFLOAT2> hiddenMesh[2];
	std::vector<DirectX::XMFLOAT2> visibleMesh[2];

	// Flags for supported API features.
	bool isSpatialStageSupported = false;
	bool isHiddenAreaMeshSupported = false;
	bool isVisibleAreaMeshSupported = false;
	bool isDepthBasedReprojectionSupported = false;
	bool isUserPresenceSupported = false;
	// Spatial Controllers
	bool supportsSpatialInput = false;
	bool supportsSourceOrientation = false;
	bool supportsMotionControllers = false;
	bool supportsHapticFeedback = false;
	bool supportsHandedness = false;

	// Remoting
	bool isRemoteHolographicSpace = false;
	Microsoft::WRL::Wrappers::SRWLock                   m_connectionStateLock;
	Microsoft::Holographic::HolographicStreamerHelpers^ m_streamerHelpers;
	Windows::Foundation::EventRegistrationToken ConnectedToken;
	Windows::Foundation::EventRegistrationToken DisconnectedToken;
	ConnectedEvent^ RemotingConnectedEvent = nullptr;
	DisconnectedEvent^ RemotingDisconnectedEvent = nullptr;

	// Controller pose
	float3 ControllerPositions[2];
	quaternion ControllerOrientations[2];

	// IDs for unhanded controllers.
	int HandIDs[2];

	// Controller state
	MixedRealityInterop::HMDInputPressState CurrentSelectState[2];
	MixedRealityInterop::HMDInputPressState PreviousSelectState[2];

	MixedRealityInterop::HMDInputPressState CurrentGraspState[2];
	MixedRealityInterop::HMDInputPressState PreviousGraspState[2];

	MixedRealityInterop::HMDInputPressState CurrentMenuState[2];
	MixedRealityInterop::HMDInputPressState PreviousMenuState[2];

	MixedRealityInterop::HMDInputPressState CurrentThumbstickPressState[2];
	MixedRealityInterop::HMDInputPressState PreviousThumbstickPressState[2];

	MixedRealityInterop::HMDInputPressState CurrentTouchpadPressState[2];
	MixedRealityInterop::HMDInputPressState PreviousTouchpadPressState[2];

	MixedRealityInterop::HMDInputPressState CurrentTouchpadIsTouchedState[2];
	MixedRealityInterop::HMDInputPressState PreviousTouchpadIsTouchedState[2];

	BOOL IsRegkeyVersionAtLeast(DWORD versionToCheck)
	{
		HKEY hKey;
		LONG lRes = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
			L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey);

		LPCWSTR keyName = L"CurrentBuild";

		const int bufferSize = 500;
		DWORD cbData = bufferSize;
		wchar_t value[bufferSize];

		LONG nError = RegQueryValueExW(hKey,
			keyName,
			0,
			NULL,
			reinterpret_cast<LPBYTE>(&value),
			&cbData);

		RegCloseKey(hKey);

		if (nError == ERROR_SUCCESS)
		{
			int currentBuildNumber = _wtoi(value);
			return currentBuildNumber >= (int)versionToCheck;
		}

		return false;
	}

	// Some API's need to check for the windows version they were introduced in.
	bool IsAtLeastWindowsBuild(const uint32_t build)
	{
		OSVERSIONINFOEX ver{};
		ver.dwOSVersionInfoSize = sizeof(ver);
		ver.dwBuildNumber = build;

		DWORDLONG condition = 0;
		VER_SET_CONDITION(condition, VER_BUILDNUMBER, VER_GREATER_EQUAL);

		if (VerifyVersionInfo(&ver, VER_BUILDNUMBER, condition))
		{
			return true;
		}

		// VerifyVersionInfo may fail in certain architectures or windows versions.
		// Fall back to regkey to check for false negative version.
		return IsRegkeyVersionAtLeast(build);
	}

	SpatialCoordinateSystem GetReferenceCoordinateSystem(MixedRealityInterop::HMDTrackingOrigin& trackingOrigin)
	{
		std::lock_guard<std::mutex> lock(StageLock);

		// Check for new stage if necessary.
		if (isSpatialStageSupported && !isRemoteHolographicSpace)
		{
			if (StageReferenceFrame == nullptr)
			{
				StageReferenceFrame = SpatialStageFrameOfReference::Current();
			}

			if (StageReferenceFrame != nullptr)
			{
				trackingOrigin = MixedRealityInterop::HMDTrackingOrigin::Floor;
				return StageReferenceFrame.CoordinateSystem();
			}
		}

		if (StageReferenceFrame == nullptr &&
			StationaryReferenceFrame != nullptr)
		{
			trackingOrigin = MixedRealityInterop::HMDTrackingOrigin::Eye;
			return StationaryReferenceFrame.CoordinateSystem();
		}

		return nullptr;
	}

#pragma region Camera Resources
	class HolographicCameraResources
	{
	public:
		HolographicCameraResources(
			const winrt::Windows::Graphics::Holographic::HolographicCamera & InCamera)
			: Camera(InCamera)
		{
			bool bIsStereo = InCamera.IsStereo();
			bStereoEnabled = bIsStereo;
			RenderTargetSize = InCamera.RenderTargetSize();

			Viewport.TopLeftX = Viewport.TopLeftY = 0.0f;
			Viewport.Width = RenderTargetSize.Width;
			Viewport.Height = RenderTargetSize.Height;
			Viewport.MinDepth = 0;
			Viewport.MaxDepth = 1.0f;
		}

		winrt::Windows::Graphics::Holographic::HolographicCamera GetCamera() const { return Camera; }
		winrt::Windows::Foundation::Size GetRenderTargetSize() const { return RenderTargetSize; }
		const D3D11_VIEWPORT & GetViewport() const { return Viewport; }
		bool IsStereoEnabled() const { return bStereoEnabled; }

	private:
		winrt::Windows::Graphics::Holographic::HolographicCamera Camera;
		winrt::Windows::Foundation::Size RenderTargetSize;
		D3D11_VIEWPORT Viewport;
		bool bStereoEnabled;
	};

	class TrackingFrame
	{
	public:
		TrackingFrame(HolographicFrame frame)
		{
			Frame = HolographicFrame(frame);
		}

		bool CalculatePose(const SpatialCoordinateSystem& CoordinateSystem)
		{
			if (Frame == nullptr)
			{
				return false;
			}

			// Get a prediction of where holographic cameras will be when this frame is presented.
			HolographicFramePrediction Prediction = Frame.CurrentPrediction();
			if (!Prediction)
			{
				return false;
			}

			IVectorView<HolographicCameraPose> CameraPoses = Prediction.CameraPoses();
			if (CameraPoses == nullptr || CoordinateSystem == nullptr)
			{
				return false;
			}

			UINT32 Size = CameraPoses.Size();
			if (Size == 0)
			{
				return false;
			}

			Pose = CameraPoses.GetAt(0);
			if (Pose == nullptr)
			{
				return false;
			}

			// Get position and orientation from a stationary or stage reference frame.
			winrt::Windows::Foundation::IReference<HolographicStereoTransform> stationaryViewTransform = Pose.TryGetViewTransform(CoordinateSystem);

			// Get rotation only from attached reference frame.
			winrt::Windows::Foundation::IReference<HolographicStereoTransform> orientationOnlyTransform{ nullptr };
			SpatialCoordinateSystem locatorAttachedCoordinateSystem = nullptr;
			if (AttachedReferenceFrame != nullptr)
			{
				locatorAttachedCoordinateSystem = AttachedReferenceFrame.GetStationaryCoordinateSystemAtTimestamp(Prediction.Timestamp());
				orientationOnlyTransform = Pose.TryGetViewTransform(locatorAttachedCoordinateSystem);
			}

			if ((stationaryViewTransform == nullptr) &&
				(orientationOnlyTransform == nullptr))
			{
				// We have no information for either frames
				return false;
			}

			bool orientationOnlyTracking = false;
			if (stationaryViewTransform == nullptr)
			{
				// We have lost world-locked tracking (6dof), and need to fall back to orientation-only tracking attached to the hmd (3dof).
				orientationOnlyTracking = true;
			}

			// If the stationary/stage is valid, cache transform between coordinate systems so we can reuse it in subsequent frames.
			if (!orientationOnlyTracking && locatorAttachedCoordinateSystem != nullptr)
			{
				winrt::Windows::Foundation::IReference<float4x4> locatorToFixedCoordTransform = CoordinateSystem.TryGetTransformTo(locatorAttachedCoordinateSystem);
				if (locatorToFixedCoordTransform != nullptr)
				{
					LastKnownCoordinateSystemTransform = locatorToFixedCoordTransform.Value();
				}
			}

			HolographicStereoTransform hst;
			if (!orientationOnlyTracking)
			{
				hst = stationaryViewTransform.Value();
			}
			else
			{
				hst = orientationOnlyTransform.Value();
			}

			leftPose = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)&hst.Left);
			rightPose = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)&hst.Right);

			// When our position-tracked transform is not valid, re-use the last known transform between coordinate systems to adjust the 
			// position and orientation so there's no visible jump.
			if (orientationOnlyTracking)
			{
				DirectX::XMMATRIX lastKnownCoordSystemTransform = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)&LastKnownCoordinateSystemTransform);

				// Transform the left and right poses by the last known coordinate system transform.
				leftPose = DirectX::XMMatrixMultiply(lastKnownCoordSystemTransform, leftPose);
				rightPose = DirectX::XMMatrixMultiply(lastKnownCoordSystemTransform, rightPose);
			}

			return true;
		}

		DirectX::XMMATRIX leftPose = DirectX::XMMatrixIdentity();
		DirectX::XMMATRIX rightPose = DirectX::XMMatrixIdentity();

		HolographicFrame Frame = nullptr;
		HolographicCameraPose Pose = nullptr;
	};

	class HolographicFrameResources
	{
	public:
		HolographicFrameResources()
		{
		}

		bool CreateRenderingParameters(TrackingFrame* frame, Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture, bool& succeeded)
		{
			succeeded = true;

			if (frame->Frame == nullptr
				|| frame->Pose == nullptr
				|| CameraResources == nullptr
				|| holographicSpace == nullptr)
			{
				return false;
			}

			if (!isRemoteHolographicSpace && !holographicSpace.IsAvailable())
			{
				return false;
			}

			// Getting rendering parameters can fail if the PC goes to sleep.
			// Wrap this in a try-catch so we do not crash.
			HolographicCameraRenderingParameters RenderingParameters = nullptr;
			try
			{
				RenderingParameters = frame->Frame.GetRenderingParameters(frame->Pose);
			}
			catch (...)
			{
				RenderingParameters = nullptr;
				succeeded = false;
			}

			if (RenderingParameters == nullptr)
			{
				return false;
			}

			// Use depth buffer to stabilize frame.
			CommitDepthTexture(depthTexture, RenderingParameters);

			// Get the WinRT object representing the holographic camera's back buffer.
			IDirect3DSurface surface = RenderingParameters.Direct3D11BackBuffer();
			if (surface == nullptr)
			{
				return false;
			}

			// Get a DXGI interface for the holographic camera's back buffer.
			// Holographic cameras do not provide the DXGI swap chain, which is owned
			// by the system. The Direct3D back buffer resource is provided using WinRT
			// interop APIs.
			winrt::com_ptr<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> DxgiInterfaceAccess =
				surface.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
			if (!DxgiInterfaceAccess)
			{
				return false;
			}

			ComPtr<ID3D11Resource> resource;
			DxgiInterfaceAccess->GetInterface(IID_PPV_ARGS(&resource));
			if (resource == nullptr)
			{
				return false;
			}

			// Get a Direct3D interface for the holographic camera's back buffer.
			resource.As(&BackBufferTexture);
			if (BackBufferTexture == nullptr)
			{
				return false;
			}

			return true;
		}

		ID3D11Texture2D* GetBackBufferTexture() const { return BackBufferTexture.Get(); }

	private:
		Microsoft::WRL::ComPtr<ID3D11Texture2D> BackBufferTexture;

		bool CommitDepthTexture(Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture, HolographicCameraRenderingParameters RenderingParameters)
		{
			if (isRemoteHolographicSpace)
			{
				return false;
			}

			if (!isDepthBasedReprojectionSupported || depthTexture == nullptr)
			{
				return false;
			}

			Microsoft::WRL::ComPtr<IDXGIResource1> depthResource;
			HRESULT hr = depthTexture.As(&depthResource);
			ComPtr<IDXGISurface2> depthDxgiSurface;
			if (SUCCEEDED(hr))
			{
				hr = depthResource->CreateSubresourceSurface(0, &depthDxgiSurface);
			}

			if (FAILED(hr))
			{
				return false;
			}

			Microsoft::WRL::ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> depthD3DSurface;
			hr = CreateDirect3D11SurfaceFromDXGISurface(depthDxgiSurface.Get(), &depthD3DSurface);
			if (FAILED(hr) || depthD3DSurface == nullptr)
			{
				return false;
			}

			winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface depth_winrt = nullptr;
			winrt::check_hresult(depthD3DSurface.Get()->QueryInterface(
				winrt::guid_of<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface>(),
				reinterpret_cast<void**>(winrt::put_abi(depth_winrt))));

			if (depth_winrt != nullptr)
			{
				try
				{
					RenderingParameters.CommitDirect3D11DepthBuffer(depth_winrt);
				}
				catch (...)
				{
					return false;
				}
			}

			return true;
		}
	};
#pragma endregion

	static MixedRealityInterop::UserPresence GetInteropUserPresence()
	{
		std::lock_guard<std::mutex> lock(poseLock);

		if (!isUserPresenceSupported || holographicSpace == nullptr)
		{
			return MixedRealityInterop::UserPresence::Unknown;
		}

		switch (holographicSpace.UserPresence())
		{
		case HolographicSpaceUserPresence::Absent:
			return MixedRealityInterop::UserPresence::NotWorn;
		case HolographicSpaceUserPresence::PresentActive:
		case HolographicSpaceUserPresence::PresentPassive:
			return MixedRealityInterop::UserPresence::Worn;
		default:
			return MixedRealityInterop::UserPresence::Unknown;
		}
	}

#pragma region Event Callbacks
	void OnLocatabilityChanged(
		const SpatialLocator& sender,
		const winrt::Windows::Foundation::IInspectable& args)
	{
	}

	void InternalCreateHiddenVisibleAreaMesh(HolographicCamera Camera)
	{
		if (isRemoteHolographicSpace)
		{
			return;
		}

		for (int i = (int)MixedRealityInterop::HMDEye::Left;
			i <= (int)MixedRealityInterop::HMDEye::Right; i++)
		{
			if (isHiddenAreaMeshSupported)
			{
				winrt::array_view<winrt::Windows::Foundation::Numerics::float2> vertices =
					Camera.LeftViewportParameters().HiddenAreaMesh();
				if (i == (int)MixedRealityInterop::HMDEye::Right)
				{
					vertices = Camera.RightViewportParameters().HiddenAreaMesh();
				}

				hiddenMesh[i].clear();

				for (int v = 0; v < (int)vertices.size(); v++)
				{
					hiddenMesh[i].push_back(DirectX::XMFLOAT2(vertices[v].x, vertices[v].y));
				}
			}

			if (isVisibleAreaMeshSupported)
			{
				winrt::array_view<winrt::Windows::Foundation::Numerics::float2> vertices =
					Camera.LeftViewportParameters().VisibleAreaMesh();
				if (i == (int)MixedRealityInterop::HMDEye::Right)
				{
					vertices = Camera.RightViewportParameters().VisibleAreaMesh();
				}

				visibleMesh[i].clear();

				for (int v = 0; v < (int)vertices.size(); v++)
				{
					visibleMesh[i].push_back(DirectX::XMFLOAT2(vertices[v].x, vertices[v].y));
				}
			}
		}
	}

	void MixedRealityInterop::CreateHiddenVisibleAreaMesh()
	{
		std::lock_guard<std::mutex> lock(CameraResourcesLock);
		if (CameraResources == nullptr)
		{
			return;
		}

		HolographicCamera camera = CameraResources->GetCamera();
		if (camera == nullptr)
		{
			return;
		}

		InternalCreateHiddenVisibleAreaMesh(camera);
	}

	bool MixedRealityInterop::GetHiddenAreaMesh(HMDEye eye, DirectX::XMFLOAT2*& vertices, int& length)
	{
		if (hiddenMesh[(int)eye].empty())
		{
			return false;
		}

		length = (int)hiddenMesh[(int)eye].size();
		vertices = &hiddenMesh[(int)eye][0];

		return true;
	}

	bool MixedRealityInterop::GetVisibleAreaMesh(HMDEye eye, DirectX::XMFLOAT2*& vertices, int& length)
	{
		if (visibleMesh[(int)eye].empty())
		{
			return false;
		}

		length = (int)visibleMesh[(int)eye].size();
		vertices = &visibleMesh[(int)eye][0];

		return true;
	}

	void OnCameraAdded(
		const HolographicSpace& sender,
		const HolographicSpaceCameraAddedEventArgs& args)
	{
		std::lock_guard<std::mutex> lock(CameraResourcesLock);
		HolographicCamera Camera = args.Camera();

		CameraResources = std::make_unique<HolographicCameraResources>(Camera);

		float width = Camera.RenderTargetSize().Width * 2.0f;
		float height = Camera.RenderTargetSize().Height;

		Camera.SetNearPlaneDistance(nearPlaneDistance);
		Camera.SetFarPlaneDistance(farPlaneDistance);

		InternalCreateHiddenVisibleAreaMesh(Camera);
	}

	void OnCameraRemoved(
		const HolographicSpace& sender,
		const HolographicSpaceCameraRemovedEventArgs& args)
	{
		std::lock_guard<std::mutex> lock(CameraResourcesLock);
		if (CameraResources == nullptr)
		{
			return;
		}

		HolographicCamera Camera = args.Camera();
		if (Camera == CameraResources->GetCamera())
		{
			CameraResources.reset();
			CameraResources = nullptr;
		}
	}

	void OnUserPresenceChanged(
		const HolographicSpace& sender,
		const winrt::Windows::Foundation::IInspectable& args)
	{
		std::lock_guard<std::mutex> lock(PresenceLock);

		MixedRealityInterop::UserPresence updatedPresence = GetInteropUserPresence();

		// OnUserPresenceChanged can fire more often than Unreal cares about since the Windows MR platform has multiple events for a valid worn state.
		if (currentUserPresence != updatedPresence)
		{
			currentUserPresence = updatedPresence;
			userPresenceChanged = true;
		}
	}
#pragma endregion

	MixedRealityInterop::MixedRealityInterop()
	{
		bInitialized = false;

		for (int i = 0; i < 2; i++)
		{
			ControllerPositions[i] = float3(0, 0, 0);
			ControllerOrientations[i] = quaternion::identity();
			HandIDs[i] = -1;
		}

		ResetButtonStates();

		// APIs introduced in 10586
		bool is10586 = IsAtLeastWindowsBuild(10586);
		supportsSpatialInput = is10586;

		// APIs introduced in 14393
		bool is14393 = IsAtLeastWindowsBuild(14393);
		supportsSourceOrientation = is14393;

		// APIs introduced in 15063
		bool is15063 = IsAtLeastWindowsBuild(15063);
		isSpatialStageSupported = is15063;
		isHiddenAreaMeshSupported = is15063;
		isDepthBasedReprojectionSupported = is15063;
		supportsMotionControllers = is15063;
		supportsHapticFeedback = is15063;

		// APIs introduced in 16299
		bool is16299 = IsAtLeastWindowsBuild(16299);
		supportsHandedness = is16299;

		// APIs introduced in 17134
		bool is17134 = IsAtLeastWindowsBuild(17134);
		isVisibleAreaMeshSupported = is17134;
		isUserPresenceSupported = is17134;
	}

	bool CreateInteropDevice(ID3D11Device* device)
	{
		// Acquire the DXGI interface for the Direct3D device.
		Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice(device);

		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		d3dDevice.As(&dxgiDevice);

		winrt::com_ptr<::IInspectable> object;
		HRESULT hr = CreateDirect3D11DeviceFromDXGIDevice(
			dxgiDevice.Get(),
			reinterpret_cast<::IInspectable**>(winrt::put_abi(object)));

		if (SUCCEEDED(hr))
		{
			InteropD3DDevice = object.as<IDirect3DDevice>();

			try
			{
				holographicSpace.SetDirect3D11Device(InteropD3DDevice);
			}
			catch (...)
			{
				return false;
			}

			return true;
		}

		return false;
	}

	UINT64 MixedRealityInterop::GraphicsAdapterLUID()
	{
		UINT64 graphicsAdapterLUID = 0;

		// If we do not have a holographic space, the engine is trying to initialize our plugin before we are ready.
		// Create a temporary window to get the correct adapter LUID.
		if (holographicSpace == nullptr)
		{
			HWND temporaryWindowHwnd = CreateWindow(L"STATIC", L"TemporaryWindow", 0, 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);
			HolographicSpace tempHolographicSpace = nullptr;

			Microsoft::WRL::ComPtr<IHolographicSpaceInterop> spaceInterop = nullptr;
			Windows::Foundation::GetActivationFactory(
				Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Graphics_Holographic_HolographicSpace).Get(),
				&spaceInterop);

			// Get the Holographic Space
			spaceInterop->CreateForWindow(
				temporaryWindowHwnd,
				winrt::guid_of<HolographicSpace>(),
				winrt::put_abi(tempHolographicSpace));

			HolographicAdapterId adapterId = tempHolographicSpace.PrimaryAdapterId();
			graphicsAdapterLUID = ((UINT64)(adapterId.HighPart) << 32) | adapterId.LowPart;

			spaceInterop = nullptr;
			tempHolographicSpace = nullptr;
			DestroyWindow(temporaryWindowHwnd);
		}
		else
		{
			HolographicAdapterId adapterId = holographicSpace.PrimaryAdapterId();
			graphicsAdapterLUID = ((UINT64)(adapterId.HighPart) << 32) | adapterId.LowPart;
		}

		return graphicsAdapterLUID;
	}

	void MixedRealityInterop::Initialize(ID3D11Device* device, float nearPlane, float farPlane)
	{
		nearPlaneDistance = nearPlane;
		farPlaneDistance = farPlane;

		if (device == nullptr
			|| bInitialized
			|| holographicSpace == nullptr)
		{
			return;
		}

		if (!isRemoteHolographicSpace && !holographicSpace.IsAvailable())
		{
			return;
		}

		// Use the default SpatialLocator to track the motion of the device.
		if (Locator == nullptr)
		{
			Microsoft::WRL::ComPtr<ABI::Windows::Perception::Spatial::ISpatialLocatorStatics> spatialLocatorStatics;
			HRESULT hr = Windows::Foundation::GetActivationFactory(Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Perception_Spatial_SpatialLocator).Get(), &spatialLocatorStatics);

			spatialLocatorStatics->GetDefault((ABI::Windows::Perception::Spatial::ISpatialLocator**)winrt::put_abi(Locator));
		}
		if (Locator == nullptr)
		{
			return;
		}

		if (!CreateInteropDevice(device))
		{
			return;
		}

		// The simplest way to render world-locked holograms is to create a stationary reference frame
		// when the app is launched. This is roughly analogous to creating a "world" coordinate system
		// with the origin placed at the device's position as the app is launched.
		if (StationaryReferenceFrame == nullptr)
		{
			StationaryReferenceFrame = Locator.CreateStationaryFrameOfReferenceAtCurrentLocation();
		}
		if (StationaryReferenceFrame == nullptr)
		{
			return;
		}

		// Create a locator attached frame of reference to fall back to if tracking is lost,
		// allowing for orientation-only tracking to take over.
		if (AttachedReferenceFrame == nullptr)
		{
			AttachedReferenceFrame = Locator.CreateAttachedFrameOfReferenceAtCurrentHeading();
		}

		if (AttachedReferenceFrame == nullptr)
		{
			return;
		}

		// Register events.
		LocatabilityChangedToken = Locator.LocatabilityChanged(
			[=](const SpatialLocator & sender, const winrt::Windows::Foundation::IInspectable & args)
		{
			OnLocatabilityChanged(sender, args);
		});

		CameraAddedToken = holographicSpace.CameraAdded(
			[=](const HolographicSpace & sender, const HolographicSpaceCameraAddedEventArgs & args)
		{
			OnCameraAdded(sender, args);
		});

		CameraRemovedToken = holographicSpace.CameraRemoved(
			[=](const HolographicSpace & sender, const HolographicSpaceCameraRemovedEventArgs & args)
		{
			OnCameraRemoved(sender, args);
		});

		// Check for an updated stage:
		StageChangedEventToken = SpatialStageFrameOfReference::CurrentChanged(
			[=](auto &&, auto &&)
		{
			// Reset stage reference frame so we can establish a new one next frame.
			std::lock_guard<std::mutex> lock(StageLock);
			StageReferenceFrame = nullptr;
		});

		if (!isRemoteHolographicSpace && isUserPresenceSupported)
		{
			UserPresenceChangedToken = holographicSpace.UserPresenceChanged(
				[=](const HolographicSpace & sender, const winrt::Windows::Foundation::IInspectable & args)
			{
				OnUserPresenceChanged(sender, args);
			});
		}

		bInitialized = true;
	}

	void MixedRealityInterop::Dispose(bool force)
	{
		std::lock_guard<std::mutex> lock(poseLock);

		std::lock_guard<std::mutex> renderLock_projection(disposeLock_GetProjection);
		std::lock_guard<std::mutex> renderLock_present(disposeLock_Present);

		if (currentFrame != nullptr)
		{
			currentFrame->Frame = nullptr;
			currentFrame->Pose = nullptr;
			delete currentFrame;
			currentFrame = nullptr;
		}

		CurrentFrameResources = nullptr;

		for (int i = 0; i < 2; i++)
		{
			ControllerPositions[i] = float3(0, 0, 0);
			ControllerOrientations[i] = quaternion::identity();
			HandIDs[i] = -1;

			hiddenMesh[i].clear();
			visibleMesh[i].clear();
		}

		if (!force && isRemoteHolographicSpace)
		{
			return;
		}

		if (holographicSpace != nullptr)
		{
			if (CameraAddedToken.value != 0)
			{
				holographicSpace.CameraAdded(CameraAddedToken);
				CameraAddedToken.value = 0;
			}

			if (CameraRemovedToken.value != 0)
			{
				holographicSpace.CameraRemoved(CameraRemovedToken);
				CameraRemovedToken.value = 0;
			}

			if (UserPresenceChangedToken.value != 0)
			{
				holographicSpace.UserPresenceChanged(UserPresenceChangedToken);
				UserPresenceChangedToken.value = 0;
			}
		}

		if (IsWindow(stereoWindowHandle))
		{
			DestroyWindow(stereoWindowHandle);
		}
		stereoWindowHandle = (HWND)INVALID_HANDLE_VALUE;

		if (Locator != nullptr && LocatabilityChangedToken.value != 0)
		{
			Locator.LocatabilityChanged(LocatabilityChangedToken);
			LocatabilityChangedToken.value = 0;
		}
		Locator = nullptr;

		if (StageReferenceFrame != nullptr && StageChangedEventToken.value != 0)
		{
			SpatialStageFrameOfReference::CurrentChanged(StageChangedEventToken);
			StageChangedEventToken.value = 0;
		}

		bInitialized = false;
		holographicSpace = nullptr;
		interactionManager = nullptr;

		CameraResources = nullptr;
		AttachedReferenceFrame = nullptr;
		StationaryReferenceFrame = nullptr;
		StageReferenceFrame = nullptr;

		isRemoteHolographicSpace = false;
	}

	bool MixedRealityInterop::IsStereoEnabled()
	{
		if (CameraResources == nullptr)
		{
			return false;
		}

		return CameraResources->IsStereoEnabled();
	}

	bool MixedRealityInterop::IsTrackingAvailable()
	{
		if (Locator == nullptr)
		{
			return false;
		}

		return Locator.Locatability() != SpatialLocatability::Unavailable;
	}

	void MixedRealityInterop::ResetOrientationAndPosition()
	{
		StationaryReferenceFrame = Locator.CreateStationaryFrameOfReferenceAtCurrentLocation();

		if (isSpatialStageSupported)
		{
			StageReferenceFrame = SpatialStageFrameOfReference::Current();
		}
	}

	bool MixedRealityInterop::IsInitialized()
	{
		if (!isRemoteHolographicSpace && (holographicSpace == nullptr || !holographicSpace.IsAvailable()))
		{
			return false;
		}

		return bInitialized
			&& holographicSpace != nullptr
			&& CameraResources != nullptr;
	}

	bool MixedRealityInterop::IsImmersiveWindowValid()
	{
		return IsWindow(stereoWindowHandle);
	}

	bool MixedRealityInterop::IsAvailable()
	{
		if (isRemoteHolographicSpace)
		{
			return holographicSpace != nullptr;
		}

		if (IsAtLeastWindowsBuild(15063))
		{
			return HolographicSpace::IsAvailable();
		}

		return true;
	}

	bool MixedRealityInterop::IsCurrentlyImmersive()
	{
		return IsInitialized()
			&& IsImmersiveWindowValid();
	}

	bool MixedRealityInterop::CreateHolographicSpace(HWND hwnd)
	{
		if (holographicSpace != nullptr)
		{
			// We already have a holographic space.
			return true;
		}

		Microsoft::WRL::ComPtr<IHolographicSpaceInterop> spaceInterop = nullptr;
		HRESULT hr = Windows::Foundation::GetActivationFactory(
			Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Graphics_Holographic_HolographicSpace).Get(),
			&spaceInterop);

		// Convert the game window into an immersive holographic space.
		if (FAILED(hr))
		{
			return false;
		}

		// Get the Holographic Space
		hr = spaceInterop->CreateForWindow(
			hwnd,
			winrt::guid_of<HolographicSpace>(),
			winrt::put_abi(holographicSpace));

		if (FAILED(hr))
		{
			return false;
		}

		// Get the interaction manager.
		Microsoft::WRL::ComPtr<ISpatialInteractionManagerInterop> interactionManagerInterop = nullptr;
		hr = Windows::Foundation::GetActivationFactory(
			Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_UI_Input_Spatial_SpatialInteractionManager).Get(),
			&interactionManagerInterop);

		if (FAILED(hr))
		{
			return false;
		}

		// Get the Interaction Manager
		hr = interactionManagerInterop->GetForWindow(
			hwnd,
			winrt::guid_of<winrt::Windows::UI::Input::Spatial::SpatialInteractionManager>(),
			winrt::put_abi(interactionManager));

		return SUCCEEDED(hr);
	}

	void ForceAllowInput(HWND hWnd)
	{
		if (!IsWindow(hWnd))
		{
			return;
		}

		// Workaround to successfully route input to our new HWND.
		AllocConsole();
		HWND hWndConsole = GetConsoleWindow();
		SetWindowPos(hWndConsole, 0, 0, 0, 0, 0, SWP_NOACTIVATE);
		FreeConsole();

		SetForegroundWindow(hWnd);
	}

	void MixedRealityInterop::EnableStereo(bool enableStereo)
	{
		if (enableStereo && holographicSpace == nullptr)
		{
			stereoWindowHandle = CreateWindow(L"STATIC", L"UE4Game_WindowsMR", 0, 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);

			// Go immersive on this window handle before it has been shown.
			CreateHolographicSpace(stereoWindowHandle);

			// Show the window to go immersive.
			ShowWindow(stereoWindowHandle, SW_SHOWNORMAL);

			// Force this window into getting input focus.
			ForceAllowInput(stereoWindowHandle);
		}
		else if (!enableStereo && holographicSpace != nullptr)
		{
			Dispose();
		}
	}

	bool MixedRealityInterop::HasUserPresenceChanged()
	{
		std::lock_guard<std::mutex> lock(PresenceLock);

		bool changedInternal = userPresenceChanged;

		// Reset so we just get this event once.
		if (userPresenceChanged) { userPresenceChanged = false; }

		return changedInternal;
	}

	MixedRealityInterop::UserPresence MixedRealityInterop::GetCurrentUserPresence()
	{
		return GetInteropUserPresence();
	}

	bool MixedRealityInterop::GetDisplayDimensions(int& width, int& height)
	{
		std::lock_guard<std::mutex> lock(CameraResourcesLock);
		width = 1920;
		height = 1080;

		if (CameraResources == nullptr)
		{
			return false;
		}

		auto size = CameraResources->GetRenderTargetSize();
		width = (int)(size.Width);
		height = (int)(size.Height);

		return true;
	}

	const wchar_t* MixedRealityInterop::GetDisplayName()
	{
		const wchar_t* name = L"WindowsMixedReality";

		std::lock_guard<std::mutex> lock(CameraResourcesLock);
		if (CameraResources == nullptr)
		{
			return name;
		}

		HolographicCamera camera = CameraResources->GetCamera();
		if (camera == nullptr)
		{
			return name;
		}

		HolographicDisplay display = camera.Display();
		if (display == nullptr)
		{
			return name;
		}

		return display.DisplayName().c_str();
	}

	// Copy a double-wide src texture into a single-wide dst texture with 2 subresources.
	void StereoCopy(
		ID3D11DeviceContext* D3D11Context,
		const float viewportScale,
		ID3D11Texture2D* src,
		ID3D11Texture2D* dst)
	{
		D3D11_TEXTURE2D_DESC desc{};
		dst->GetDesc(&desc);

		const uint32_t scaledWidth = (uint32_t)(desc.Width * viewportScale);
		const uint32_t scaledHeight = (uint32_t)(desc.Height * viewportScale);

		D3D11_BOX box = {};
		box.right = scaledWidth;
		box.bottom = scaledHeight;
		box.back = 1;
		for (int i = 0; i < 2; ++i) { // Copy each eye to HMD backbuffer
			const uint32_t offsetX = (desc.Width - scaledWidth) / 2;
			const uint32_t offsetY = (desc.Height - scaledHeight) / 2;
			D3D11Context->CopySubresourceRegion(dst, i, offsetX, offsetY, 0, src, 0, &box);
			box.left += scaledWidth;
			box.right += scaledWidth;
		}
	}

	bool MixedRealityInterop::GetCurrentPose(DirectX::XMMATRIX& leftView, DirectX::XMMATRIX& rightView, HMDTrackingOrigin& trackingOrigin)
	{
		std::lock_guard<std::mutex> lock(poseLock);

		if (!IsInitialized()
			|| CameraResources == nullptr
			// Do not update the frame after we generate rendering parameters for it.
			|| CurrentFrameResources != nullptr)
		{
			return false;
		}

		auto CoordinateSystem = GetReferenceCoordinateSystem(trackingOrigin);
		if (holographicSpace == nullptr || CoordinateSystem == nullptr) { return false; }

		// We do not have a current frame, create a new one.
		if (currentFrame == nullptr)
		{
			HolographicFrame frame = holographicSpace.CreateNextFrame();
			if (frame == nullptr) { return false; }

			currentFrame = new TrackingFrame(frame);
		}
		else
		{
			// Update the existing frame to get more recent pose information.
			currentFrame->Frame.UpdateCurrentPrediction();
		}

		if (currentFrame == nullptr)
		{
			return false;
		}

		if (!currentFrame->CalculatePose(CoordinateSystem))
		{
			// If we fail to calculate a pose for this frame, reset the current frame to try again with a new frame.
			currentFrame = nullptr;
			return false;
		}

		leftView = currentFrame->leftPose;
		rightView = currentFrame->rightPose;

		// Do not add a vertical offset if we have previously used a stage as a reference frame, 
		// since a stage reference frame uses a floor origin.
		if (trackingOrigin == MixedRealityInterop::HMDTrackingOrigin::Eye)
		{
			// Add a vertical offset if using eye tracking so the player does not start in the floor.
			DirectX::XMMATRIX heightOffset = DirectX::XMMatrixTranslation(0, defaultPlayerHeight, 0);

			leftView = DirectX::XMMatrixMultiply(heightOffset, leftView);
			rightView = DirectX::XMMatrixMultiply(heightOffset, rightView);
		}

		return true;
	}

	DirectX::XMMATRIX MixedRealityInterop::GetProjectionMatrix(HMDEye eye)
	{
		std::lock_guard<std::mutex> lock(disposeLock_GetProjection);

		if (currentFrame == nullptr
			|| currentFrame->Pose == nullptr)
		{
			winrt::Windows::Foundation::Numerics::float4x4 projection = (eye == HMDEye::Left)
				? LastKnownProjection.Left
				: LastKnownProjection.Right;

			return DirectX::XMLoadFloat4x4(&projection);
		}

		IHolographicCameraPose pose = currentFrame->Pose;

		HolographicStereoTransform CameraProjectionTransform = pose.ProjectionTransform();
		LastKnownProjection = HolographicStereoTransform(CameraProjectionTransform);

		winrt::Windows::Foundation::Numerics::float4x4 projection = (eye == HMDEye::Left)
			? CameraProjectionTransform.Left
			: CameraProjectionTransform.Right;

		return DirectX::XMLoadFloat4x4(&projection);
	}

	void MixedRealityInterop::SetScreenScaleFactor(float scale)
	{
		ScreenScaleFactor = scale;

		std::lock_guard<std::mutex> lock(CameraResourcesLock);
		if (CameraResources == nullptr)
		{
			return;
		}

		HolographicCamera camera = CameraResources->GetCamera();
		if (camera == nullptr)
		{
			return;
		}

		camera.ViewportScaleFactor(ScreenScaleFactor);
	}

	bool MixedRealityInterop::CreateRenderingParameters(ID3D11Texture2D* depthTexture)
	{
		std::lock_guard<std::mutex> lock(poseLock);
		bool succeeded = true;

		if (currentFrame == nullptr
			|| currentFrame->Frame == nullptr
			|| currentFrame->Pose == nullptr
			// Do not recreate rendering parameters for a frame, this will throw an exception.
			|| CurrentFrameResources != nullptr)
		{
			return succeeded;
		}

		CurrentFrameResources = new HolographicFrameResources();
		bool renderingParamsCreated = CurrentFrameResources->CreateRenderingParameters(currentFrame, depthTexture, succeeded);

		if (!renderingParamsCreated
			|| CurrentFrameResources->GetBackBufferTexture() == nullptr)
		{
			// We failed to produce rendering parameters, try again next frame.
			CurrentFrameResources = nullptr;
		}

		return succeeded;
	}

	bool MixedRealityInterop::Present(ID3D11DeviceContext* context, ID3D11Texture2D* viewportTexture)
	{
		std::lock_guard<std::mutex> lock(disposeLock_Present);

		if (currentFrame == nullptr
			|| !CurrentFrameResources
			|| CurrentFrameResources->GetBackBufferTexture() == nullptr
			|| viewportTexture == nullptr)
		{
			return true;
		}

		StereoCopy(
			context,
			ScreenScaleFactor,
			viewportTexture,
			CurrentFrameResources->GetBackBufferTexture());

		HolographicFramePresentResult presentResult = currentFrame->Frame.PresentUsingCurrentPrediction();

		// Reset the frame pointer to allow for a new frame to be created.
		CurrentFrameResources = nullptr;
		currentFrame = nullptr;

		return true;
	}

	bool MixedRealityInterop::SupportsSpatialInput()
	{
		return supportsSpatialInput;
	}

	bool CheckHandedness(SpatialInteractionSource source, MixedRealityInterop::HMDHand hand)
	{
		if (!isRemoteHolographicSpace)
		{
			SpatialInteractionSourceHandedness desiredHandedness = (hand == MixedRealityInterop::HMDHand::Left) ?
				SpatialInteractionSourceHandedness::Left : SpatialInteractionSourceHandedness::Right;

			return source.Handedness() == desiredHandedness;
		}

		// For HoloLens, we must check handedness from the source id.
		return HandIDs[(int)hand] == source.Id();
	}

	bool GetInputSources(IVectorView<SpatialInteractionSourceState>& sourceStates)
	{
		if (interactionManager == nullptr || !bInitialized)
		{
			return false;
		}

		MixedRealityInterop::HMDTrackingOrigin trackingOrigin;
		SpatialCoordinateSystem coordinateSystem = GetReferenceCoordinateSystem(trackingOrigin);
		if (coordinateSystem == nullptr)
		{
			return false;
		}

		Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestampHelperStatics> timestampStatics;
		HRESULT hr = Windows::Foundation::GetActivationFactory(Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Perception_PerceptionTimestampHelper).Get(), &timestampStatics);

		// Convert from winrt DateTime to ABI DateTime.
		winrt::Windows::Foundation::DateTime dt = winrt::clock::now();
		winrt::Windows::Foundation::TimeSpan timespan = dt.time_since_epoch();

		ABI::Windows::Foundation::DateTime dt_abi;
		dt_abi.UniversalTime = timespan.count();

		ABI::Windows::Perception::IPerceptionTimestamp* timestamp = nullptr;
		timestampStatics->FromHistoricalTargetTime(dt_abi, &timestamp);

		if (timestamp == nullptr)
		{
			return false;
		}

		winrt::Windows::Perception::PerceptionTimestamp ts = nullptr;

		winrt::check_hresult(timestamp->QueryInterface(winrt::guid_of<winrt::Windows::Perception::PerceptionTimestamp>(),
			reinterpret_cast<void**>(winrt::put_abi(ts))));

		sourceStates = interactionManager.GetDetectedSourcesAtTimestamp(ts);

		return true;
	}

	MixedRealityInterop::HMDTrackingStatus MixedRealityInterop::GetControllerTrackingStatus(HMDHand hand)
	{
		HMDTrackingStatus trackingStatus = HMDTrackingStatus::NotTracked;

		if (!IsInitialized())
		{
			return trackingStatus;
		}

		IVectorView<SpatialInteractionSourceState> sourceStates;
		if (!GetInputSources(sourceStates))
		{
			return trackingStatus;
		}

		int sourceCount = sourceStates.Size();
		for (int i = 0; i < sourceCount; i++)
		{
			SpatialInteractionSourceState state = sourceStates.GetAt(i);
			if (state == nullptr)
			{
				continue;
			}

			SpatialInteractionSource source = state.Source();
			if (source == nullptr)
			{
				continue;
			}

			if (!CheckHandedness(source, hand))
			{
				continue;
			}

			HMDTrackingOrigin trackingOrigin;
			SpatialCoordinateSystem coordinateSystem = GetReferenceCoordinateSystem(trackingOrigin);
			if (coordinateSystem != nullptr)
			{
				SpatialInteractionSourceProperties prop = state.Properties();
				if (prop == nullptr)
				{
					continue;
				}

				SpatialInteractionSourceLocation sourceLocation = prop.TryGetLocation(coordinateSystem);
				if (sourceLocation != nullptr)
				{
					if (sourceLocation.Position() != nullptr)
					{
						ControllerPositions[(int)hand] = sourceLocation.Position().Value();
						trackingStatus = HMDTrackingStatus::Tracked;

						// Do not add a vertical offset if we have previously used a stage as a reference frame, 
						// since a stage reference frame uses a floor origin.
						if (trackingOrigin == HMDTrackingOrigin::Eye)
						{
							// Add a vertical offset if using eye tracking so the player does not start in the floor.
							ControllerPositions[(int)hand] -= float3(0, defaultPlayerHeight, 0);
						}
					}
					if (supportsSourceOrientation &&
						(sourceLocation.Orientation() != nullptr))
					{
						ControllerOrientations[(int)hand] = sourceLocation.Orientation().Value();

						if (sourceLocation.Position() == nullptr)
						{
							trackingStatus = HMDTrackingStatus::InertialOnly;
						}
					}
				}
			}
		}

		return trackingStatus;
	}

	bool MixedRealityInterop::GetControllerOrientationAndPosition(HMDHand hand, DirectX::XMFLOAT4& orientation, DirectX::XMFLOAT3& position)
	{
		if (isRemoteHolographicSpace)
		{
			if (HandIDs[(int)hand] == -1)
			{
				return false;
			}
		}

		float3 pos = ControllerPositions[(int)hand];
		quaternion rot = ControllerOrientations[(int)hand];

		orientation = DirectX::XMFLOAT4(rot.x, rot.y, rot.z, rot.w);
		position = DirectX::XMFLOAT3(pos.x, pos.y, pos.z);

		return true;
	}

	MixedRealityInterop::HMDInputPressState PressStateFromBool(bool isPressed)
	{
		return isPressed ?
			MixedRealityInterop::HMDInputPressState::Pressed :
			MixedRealityInterop::HMDInputPressState::Released;
	}

	void UpdateButtonStates(SpatialInteractionSourceState state)
	{
		SpatialInteractionSource source = state.Source();
		if (source == nullptr)
		{
			return;
		}

		int handIndex = 0;
		if (!isRemoteHolographicSpace)
		{
			// Find hand index from source handedness.
			SpatialInteractionSourceHandedness handedness = source.Handedness();
			if (handedness != SpatialInteractionSourceHandedness::Left)
			{
				handIndex = 1;
			}
		}
		else
		{
			// If source does not support handedness, find hand index from HandIDs array.
			handIndex = -1;
			for (int i = 0; i < 2; i++)
			{
				if (source.Id() == HandIDs[i])
				{
					handIndex = i;
					break;
				}
			}

			if (handIndex == -1)
			{
				// No hands.
				return;
			}
		}

		if (!supportsMotionControllers || isRemoteHolographicSpace)
		{
			// Prior to motion controller support, Select was the only press
			bool isPressed = state.IsPressed();
			PreviousSelectState[handIndex] = CurrentSelectState[handIndex];
			CurrentSelectState[handIndex] = PressStateFromBool(isPressed);
		}
		else if (supportsMotionControllers && !isRemoteHolographicSpace)
		{
			// Select
			bool isPressed = state.IsSelectPressed();
			PreviousSelectState[handIndex] = CurrentSelectState[handIndex];
			CurrentSelectState[handIndex] = PressStateFromBool(isPressed);

			// Grasp
			isPressed = state.IsGrasped();
			PreviousGraspState[handIndex] = CurrentGraspState[handIndex];
			CurrentGraspState[handIndex] = PressStateFromBool(isPressed);

			// Menu
			isPressed = state.IsMenuPressed();
			PreviousMenuState[handIndex] = CurrentMenuState[handIndex];
			CurrentMenuState[handIndex] = PressStateFromBool(isPressed);

			SpatialInteractionControllerProperties controllerProperties = state.ControllerProperties();
			if (controllerProperties == nullptr)
			{
				// All remaining controller buttons require the controller properties.
				return;
			}

			// Thumbstick
			isPressed = controllerProperties.IsThumbstickPressed();
			PreviousThumbstickPressState[handIndex] = CurrentThumbstickPressState[handIndex];
			CurrentThumbstickPressState[handIndex] = PressStateFromBool(isPressed);

			// Touchpad
			isPressed = controllerProperties.IsTouchpadPressed();
			PreviousTouchpadPressState[handIndex] = CurrentTouchpadPressState[handIndex];
			CurrentTouchpadPressState[handIndex] = PressStateFromBool(isPressed);

			// Touchpad (is touched)
			isPressed = controllerProperties.IsTouchpadTouched();
			PreviousTouchpadIsTouchedState[handIndex] = CurrentTouchpadIsTouchedState[handIndex];
			CurrentTouchpadIsTouchedState[handIndex] = PressStateFromBool(isPressed);
		}
	}

	bool HandCurrentlyTracked(int id)
	{
		for (int i = 0; i < 2; i++)
		{
			if (HandIDs[i] == id)
			{
				return true;
			}
		}

		return false;
	}

	void AddHand(int id)
	{
		// Check right hand first (index 1).
		for (int i = 1; i >= 0; i--)
		{
			if (HandIDs[i] == -1)
			{
				HandIDs[i] = id;
				return;
			}
		}
	}

	void UpdateTrackedHands(IVectorView<SpatialInteractionSourceState> sourceStates)
	{
		int sourceCount = sourceStates.Size();

		for (int i = 0; i < sourceCount; i++)
		{
			SpatialInteractionSourceState state = sourceStates.GetAt(i);
			if (state == nullptr)
			{
				continue;
			}

			SpatialInteractionSource source = state.Source();
			if (source == nullptr)
			{
				continue;
			}

			if (!HandCurrentlyTracked(source.Id()))
			{
				AddHand(source.Id());
			}
		}
	}

	// Reset any lost hands.
	void ResetHandIDs(IVectorView<SpatialInteractionSourceState> sourceStates)
	{
		int sourceCount = sourceStates.Size();

		for (int i = 0; i < 2; i++)
		{
			// Hand already reset.
			if (HandIDs[i] == -1)
			{
				continue;
			}

			bool handFound = false;
			for (int j = 0; j < sourceCount; j++)
			{
				SpatialInteractionSourceState state = sourceStates.GetAt(j);
				if (state == nullptr)
				{
					continue;
				}

				SpatialInteractionSource source = state.Source();
				if (source == nullptr)
				{
					continue;
				}

				if (HandIDs[i] == source.Id())
				{
					handFound = true;
					break;
				}
			}

			if (!handFound)
			{
				HandIDs[i] = -1;
			}
		}
	}

	void MixedRealityInterop::PollInput()
	{
		IVectorView<SpatialInteractionSourceState> sourceStates;
		if (!GetInputSources(sourceStates))
		{
			return;
		}

		// Update unhanded controller mapping.
		if (isRemoteHolographicSpace)
		{
			// Remove and hands that have been removed since last update.
			ResetHandIDs(sourceStates);

			// Add new tracked hands.
			UpdateTrackedHands(sourceStates);
		}

		int sourceCount = sourceStates.Size();
		for (int i = 0; i < sourceCount; i++)
		{
			SpatialInteractionSourceState state = sourceStates.GetAt(i);
			if (state == nullptr)
			{
				continue;
			}

			UpdateButtonStates(state);
		}
	}

	MixedRealityInterop::HMDInputPressState MixedRealityInterop::GetPressState(HMDHand hand, HMDInputControllerButtons button)
	{
		int index = (int)hand;

		HMDInputPressState pressState = HMDInputPressState::NotApplicable;

		switch (button)
		{
		case HMDInputControllerButtons::Grasp:
			pressState = (CurrentGraspState[index] != PreviousGraspState[index]) ? CurrentGraspState[index] : HMDInputPressState::NotApplicable;
			break;

		case HMDInputControllerButtons::Menu:
			pressState = (CurrentMenuState[index] != PreviousMenuState[index]) ? CurrentMenuState[index] : HMDInputPressState::NotApplicable;
			break;

		case HMDInputControllerButtons::Select:
			pressState = (CurrentSelectState[index] != PreviousSelectState[index]) ? CurrentSelectState[index] : HMDInputPressState::NotApplicable;
			break;

		case HMDInputControllerButtons::Thumbstick:
			pressState = (CurrentThumbstickPressState[index] != PreviousThumbstickPressState[index]) ? CurrentThumbstickPressState[index] : HMDInputPressState::NotApplicable;
			break;

		case HMDInputControllerButtons::Touchpad:
			pressState = (CurrentTouchpadPressState[index] != PreviousTouchpadPressState[index]) ? CurrentTouchpadPressState[index] : HMDInputPressState::NotApplicable;
			break;

		case HMDInputControllerButtons::TouchpadIsTouched:
			pressState = (CurrentTouchpadIsTouchedState[index] != PreviousTouchpadIsTouchedState[index]) ? CurrentTouchpadIsTouchedState[index] : HMDInputPressState::NotApplicable;
			break;
		}

		return pressState;
	}

	void MixedRealityInterop::ResetButtonStates()
	{
		for (int i = 0; i < 2; i++)
		{
			CurrentSelectState[i] = HMDInputPressState::NotApplicable;
			PreviousSelectState[i] = HMDInputPressState::NotApplicable;

			CurrentGraspState[i] = HMDInputPressState::NotApplicable;
			PreviousGraspState[i] = HMDInputPressState::NotApplicable;

			CurrentMenuState[i] = HMDInputPressState::NotApplicable;
			PreviousMenuState[i] = HMDInputPressState::NotApplicable;

			CurrentThumbstickPressState[i] = HMDInputPressState::NotApplicable;
			PreviousThumbstickPressState[i] = HMDInputPressState::NotApplicable;

			CurrentTouchpadPressState[i] = HMDInputPressState::NotApplicable;
			PreviousTouchpadPressState[i] = HMDInputPressState::NotApplicable;

			CurrentTouchpadIsTouchedState[i] = HMDInputPressState::NotApplicable;
			PreviousTouchpadIsTouchedState[i] = HMDInputPressState::NotApplicable;
		}
	}

	float MixedRealityInterop::GetAxisPosition(HMDHand hand, HMDInputControllerAxes axis)
	{
		if (!supportsMotionControllers || isRemoteHolographicSpace)
		{
			return 0.0f;
		}

		IVectorView<SpatialInteractionSourceState> sourceStates;
		if (!GetInputSources(sourceStates))
		{
			return 0.0f;
		}

		int sourceCount = sourceStates.Size();
		for (int i = 0; i < sourceCount; i++)
		{
			SpatialInteractionSourceState state = sourceStates.GetAt(i);
			if (state == nullptr)
			{
				continue;
			}

			SpatialInteractionSource source = state.Source();
			if (source == nullptr)
			{
				continue;
			}

			if (!CheckHandedness(source, hand))
			{
				continue;
			}

			if (axis == HMDInputControllerAxes::SelectValue)
			{
				return static_cast<float>(state.SelectPressedValue());
			}

			SpatialInteractionControllerProperties controllerProperties = state.ControllerProperties();
			if (controllerProperties == nullptr)
			{
				return 0.0f;
			}

			double axisValue = 0.0;
			switch (axis)
			{
			case HMDInputControllerAxes::ThumbstickX:
				axisValue = controllerProperties.ThumbstickX();
				break;

			case HMDInputControllerAxes::ThumbstickY:
				axisValue = controllerProperties.ThumbstickY();
				break;

			case HMDInputControllerAxes::TouchpadX:
				axisValue = controllerProperties.TouchpadX();
				break;

			case HMDInputControllerAxes::TouchpadY:
				axisValue = controllerProperties.TouchpadY();
				break;
			}

			return static_cast<float>(axisValue);
		}

		return 0.0f;
	}

	void MixedRealityInterop::SubmitHapticValue(HMDHand hand, float value)
	{
		if (!supportsHapticFeedback || isRemoteHolographicSpace)
		{
			return;
		}

		IVectorView<SpatialInteractionSourceState> sourceStates;
		if (!GetInputSources(sourceStates))
		{
			return;
		}

		int sourceCount = sourceStates.Size();
		for (int i = 0; i < sourceCount; i++)
		{
			SpatialInteractionSourceState state = sourceStates.GetAt(i);
			if (state == nullptr)
			{
				continue;
			}

			SpatialInteractionSource source = state.Source();
			if (source == nullptr)
			{
				continue;
			}

			if (!CheckHandedness(source, hand))
			{
				continue;
			}

			SpatialInteractionController controller = source.Controller();
			if (controller == nullptr)
			{
				return;
			}

			SimpleHapticsController hapticsController = controller.SimpleHapticsController();
			if (hapticsController == nullptr)
			{
				return;
			}

			IVectorView<SimpleHapticsControllerFeedback> supportedFeedback = hapticsController.SupportedFeedback();
			uint32_t feedbackSize = supportedFeedback.Size();
			if (feedbackSize == 0)
			{
				return;
			}

			SimpleHapticsControllerFeedback feedback = nullptr;
			for (uint32_t i = 0; i < feedbackSize; i++)
			{
				SimpleHapticsControllerFeedback feed = supportedFeedback.GetAt(i);
				if (feed == nullptr)
				{
					break;
				}

				// Check for specific waveform(s)
				uint16_t waveform = feed.Waveform();
				if (waveform == KnownSimpleHapticsControllerWaveforms::BuzzContinuous())
				{
					// We found a suitable waveform
					feedback = feed;
					break;
				}
			}

			if (feedback == nullptr)
			{
				// We did not find a suitable waveform.
				return;
			}

			// Submit the feedback value
			if (value > 0.0f)
			{
				hapticsController.SendHapticFeedback(
					feedback,
					static_cast<double>(value));
			}
			else
			{
				hapticsController.StopFeedback();
			}
		}
	}

	void MixedRealityInterop::ConnectToRemoteHoloLens(ID3D11Device* device, const wchar_t * ip, int bitrate)
	{
		if (m_streamerHelpers != nullptr)
		{
			// We are already connected to the remote device.
			return;
		}

		const int STREAMER_WIDTH = 1280;
		const int STREAMER_HEIGHT = 720;

		if (bitrate < 1024) { bitrate = 1024; }
		if (bitrate > 99999) { bitrate = 99999; }

		// Connecting to the remote device can change the connection state.
		auto exclusiveLock = m_connectionStateLock.LockExclusive();

		if (m_streamerHelpers == nullptr)
		{
			m_streamerHelpers = ref new HolographicStreamerHelpers();
			m_streamerHelpers->CreateStreamer(device);
			m_streamerHelpers->SetVideoFrameSize(STREAMER_WIDTH, STREAMER_HEIGHT);
			m_streamerHelpers->SetMaxBitrate(bitrate);

			RemotingConnectedEvent = ref new ConnectedEvent(
				[&]()
			{
				isRemoteHolographicSpace = true;

				winrt::check_hresult(reinterpret_cast<::IUnknown*>(m_streamerHelpers->HolographicSpace)
					->QueryInterface(winrt::guid_of<HolographicSpace>(),
						reinterpret_cast<void**>(winrt::put_abi(holographicSpace))));

				interactionManager = SpatialInteractionManager::GetForCurrentView();
			});
			ConnectedToken = m_streamerHelpers->OnConnected += RemotingConnectedEvent;

			RemotingDisconnectedEvent = ref new DisconnectedEvent(
				[&](_In_ HolographicStreamerConnectionFailureReason failureReason)
			{
				DisconnectFromRemoteHoloLens();
			});
			DisconnectedToken = m_streamerHelpers->OnDisconnected += RemotingDisconnectedEvent;

			try
			{
				m_streamerHelpers->Connect(ip, 8001);
			}
			catch (Platform::Exception^ ex)
			{
				OutputDebugString(L"Connect failed with hr = ");
				OutputDebugString(std::to_wstring(ex->HResult).c_str());
				OutputDebugString(L"\n");
			}
		}
	}

	void MixedRealityInterop::DisconnectFromRemoteHoloLens()
	{
		// Disconnecting from the remote device can change the connection state.
		auto exclusiveLock = m_connectionStateLock.LockExclusive();

		if (m_streamerHelpers != nullptr)
		{
			m_streamerHelpers->OnConnected -= ConnectedToken;
			m_streamerHelpers->OnDisconnected -= DisconnectedToken;

			RemotingConnectedEvent = nullptr;
			RemotingDisconnectedEvent = nullptr;

			m_streamerHelpers->Disconnect();

			// Reset state
			m_streamerHelpers = nullptr;

			Dispose(true);
		}
	}

	bool MixedRealityInterop::IsRemoting()
	{
		return isRemoteHolographicSpace && holographicSpace != nullptr;
	}
}