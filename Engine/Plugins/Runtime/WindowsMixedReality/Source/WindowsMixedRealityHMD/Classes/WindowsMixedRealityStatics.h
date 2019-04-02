// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Windows/WindowsHWrapper.h"

#pragma warning(disable:4668)  
#include <DirectXMath.h>
#pragma warning(default:4668)

#if WITH_WINDOWS_MIXED_REALITY
#include "Windows/AllowWindowsPlatformTypes.h"
#include "MixedRealityInterop.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace WindowsMixedReality
{
	class WINDOWSMIXEDREALITYHMD_API FWindowsMixedRealityStatics
	{
	public:
		// Convert between DirectX XMMatrix to Unreal FMatrix.
		static FORCEINLINE FMatrix ToFMatrix(DirectX::XMMATRIX& M)
		{
			DirectX::XMFLOAT4X4 dst;
			DirectX::XMStoreFloat4x4(&dst, M);

			return FMatrix(
				FPlane(dst._11, dst._21, dst._31, dst._41),
				FPlane(dst._12, dst._22, dst._32, dst._42),
				FPlane(dst._13, dst._23, dst._33, dst._43),
				FPlane(dst._14, dst._24, dst._34, dst._44));
		}

		static FORCEINLINE FVector FromMixedRealityVector(DirectX::XMFLOAT3 pos)
		{
			return FVector(
				-1.0f * pos.z,
				pos.x,
				pos.y);
		}

		static FORCEINLINE FQuat FromMixedRealityQuaternion(DirectX::XMFLOAT4 rot)
		{
			FQuat quaternion(
				-1.0f * rot.z,
				rot.x,
				rot.y,
				-1.0f * rot.w);
			quaternion.Normalize();

			return quaternion;
		}

		static bool SupportsSpatialInput();

#if WITH_WINDOWS_MIXED_REALITY
		static MixedRealityInterop::HMDTrackingStatus GetControllerTrackingStatus(MixedRealityInterop::HMDHand hand);

		static bool GetControllerOrientationAndPosition(MixedRealityInterop::HMDHand hand, FRotator& OutOrientation, FVector& OutPosition);

		static bool PollInput();

		static MixedRealityInterop::HMDInputPressState GetPressState(
			WindowsMixedReality::MixedRealityInterop::HMDHand hand,
			WindowsMixedReality::MixedRealityInterop::HMDInputControllerButtons button);

		static float GetAxisPosition(
			MixedRealityInterop::HMDHand hand,
			MixedRealityInterop::HMDInputControllerAxes axis);

		static void SubmitHapticValue(
			MixedRealityInterop::HMDHand hand,
			float value);
#endif
		// Remoting
		static void ConnectToRemoteHoloLens(FString remoteIP, unsigned int bitrate);
		static void DisconnectFromRemoteHoloLens();
	};
}
