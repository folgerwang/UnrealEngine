// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#ifdef MIXEDREALITYINTEROP_EXPORTS
#define MIXEDREALITYINTEROP_API __declspec(dllexport)
#else
#define MIXEDREALITYINTEROP_API __declspec(dllimport)
#endif

#pragma warning(disable:4668)
#pragma warning(disable:4005)  
#include <Windows.h>

#include <d3d11.h>

#include <DirectXMath.h>
#pragma warning(default:4005)
#pragma warning(default:4668)

namespace WindowsMixedReality
{
	class MIXEDREALITYINTEROP_API MixedRealityInterop
	{
	public:
		enum class HMDEye
		{
			Left = 0,
			Right = 1
		};

		enum class HMDTrackingOrigin
		{
			Eye,
			Floor
		};

		enum class HMDHand
		{
			Left = 0,
			Right = 1
		};

		enum class HMDTrackingStatus
		{
			NotTracked,
			InertialOnly,
			Tracked
		};

		// Match EHMDWornState
		enum class UserPresence
		{
			Unknown,
			Worn,
			NotWorn
		};

		enum class HMDInputPressState
		{
			NotApplicable = 0,
			Pressed = 1,
			Released = 2
		};

		enum class HMDInputControllerButtons
		{
			Select,
			Grasp,
			Menu,
			Thumbstick,
			Touchpad,
			TouchpadIsTouched
		};

		enum class HMDInputControllerAxes
		{
			SelectValue,
			ThumbstickX,
			ThumbstickY,
			TouchpadX,
			TouchpadY
		};

		MixedRealityInterop();
		~MixedRealityInterop() {}

		UINT64 GraphicsAdapterLUID();

		void Initialize(ID3D11Device* device, float nearPlane = 0.001f, float farPlane = 100000.0f);
		void Dispose(bool force = false);
		bool IsStereoEnabled();
		bool IsTrackingAvailable();
		void ResetOrientationAndPosition();

		bool IsInitialized();
		bool IsImmersiveWindowValid();
		bool IsAvailable();
		bool IsCurrentlyImmersive();
		bool CreateHolographicSpace(HWND hwnd);
		void EnableStereo(bool enableStereo);

		bool HasUserPresenceChanged();
		UserPresence GetCurrentUserPresence();

		void CreateHiddenVisibleAreaMesh();

		bool GetDisplayDimensions(int& width, int& height);
		const wchar_t* GetDisplayName();

		// Get the latest pose information from our tracking frame.
		bool GetCurrentPose(DirectX::XMMATRIX& leftView, DirectX::XMMATRIX& rightView, HMDTrackingOrigin& trackingOrigin);

		DirectX::XMMATRIX GetProjectionMatrix(HMDEye eye);
		bool GetHiddenAreaMesh(HMDEye eye, DirectX::XMFLOAT2*& vertices, int& length);
		bool GetVisibleAreaMesh(HMDEye eye, DirectX::XMFLOAT2*& vertices, int& length);

		void SetScreenScaleFactor(float scale);

		// Use double-width stereo texture for the depth texture or nullptr to ignore.
		bool CreateRenderingParameters(ID3D11Texture2D* depthTexture);

		// Use double-width stereo texture for the viewport texture.
		bool Present(ID3D11DeviceContext* context, ID3D11Texture2D* viewportTexture);

		// Spatial Input
		bool SupportsSpatialInput();

		HMDTrackingStatus GetControllerTrackingStatus(HMDHand hand);

		bool GetControllerOrientationAndPosition(
			HMDHand hand,
			DirectX::XMFLOAT4& orientation,
			DirectX::XMFLOAT3& position);

		void PollInput();
		HMDInputPressState GetPressState(HMDHand hand, HMDInputControllerButtons button);
		void ResetButtonStates();

		float GetAxisPosition(HMDHand hand, HMDInputControllerAxes axis);

		void SubmitHapticValue(HMDHand hand, float value);

		// Remoting
		void ConnectToRemoteHoloLens(ID3D11Device* device, const wchar_t* ip, int bitrate);
		void DisconnectFromRemoteHoloLens();
		bool IsRemoting();
	};
}

