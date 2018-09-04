// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once
#pragma warning(disable:4668)
#pragma warning(disable:4005)  
#include <Windows.h>

#include <d3d11.h>

#include <DirectXMath.h>
#pragma warning(default:4005)
#pragma warning(default:4668)

namespace WindowsMixedReality
{
	class MixedRealityInterop
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
			ThumbstickX,
			ThumbstickY,
			TouchpadX,
			TouchpadY
		};

		MixedRealityInterop();
		~MixedRealityInterop() {}

		UINT64 GraphicsAdapterLUID();

		void Initialize(ID3D11Device* device, float nearPlane = 0.001f, float farPlane = 100000.0f);
		void Dispose();
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

		int GetDisplayWidth();
		int GetDisplayHeight();
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

	private:
		bool CreateInteropDevice(ID3D11Device* device);

		void StereoCopy(
			ID3D11DeviceContext* D3D11Context,
			const float viewportScale,
			ID3D11Texture2D* src,
			ID3D11Texture2D* dst);

		bool bInitialized;
	};
}

