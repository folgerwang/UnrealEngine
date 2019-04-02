// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"

#include "IInputDevice.h"
#include "GenericPlatform/IInputInterface.h"
#include "IMotionController.h"
#include "IHapticDevice.h"
#include "InputCoreTypes.h"
#include "XRMotionControllerBase.h"

#include "Features/IModularFeatures.h"

#include "WindowsSpatialInputDefinitions.h"

namespace WindowsMixedReality
{
	class FWindowsMixedRealitySpatialInput
		: public IInputDevice
		, public IHapticDevice
		, public FXRMotionControllerBase
	{
	public:
		// WindowsMixedRealitySpatialInput.cpp
		FWindowsMixedRealitySpatialInput(
			const TSharedRef< FGenericApplicationMessageHandler > & InMessageHandler);
		virtual ~FWindowsMixedRealitySpatialInput();

		// Inherited via IInputDevice
		virtual void Tick(float DeltaTime) override;
		virtual void SendControllerEvents() override;
		virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
		virtual bool Exec(UWorld * InWorld, const TCHAR * Cmd, FOutputDevice & Ar) override;
		virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
		virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues & values) override;

		// Inherited via IHapticDevice
		virtual IHapticDevice* GetHapticDevice() override
		{
			return this;
		}

		virtual void SetHapticFeedbackValues(int32 ControllerId, int32 DeviceHand, const FHapticFeedbackValues & Values) override;
		virtual void GetHapticFrequencyRange(float & MinFrequency, float & MaxFrequency) const override;
		virtual float GetHapticAmplitudeScale() const override;

		// Inherited via FXRMotionControllerBase
		virtual FName GetMotionControllerDeviceTypeName() const override;
		virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator & OutOrientation, FVector & OutPosition, float WorldToMetersScale) const override;
		virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const override;

	private:
		void RegisterKeys() noexcept;
		void InitializeSpatialInput() noexcept;
		void UninitializeSpatialInput() noexcept;

#if WITH_WINDOWS_MIXED_REALITY
		void SendButtonEvents(uint32 source);
		void SendAxisEvents(uint32 source);
#endif

		bool isLeftTouchpadTouched = false;
		bool isRightTouchpadTouched = false;

		FThreadSafeBool IsInitialized = false;

		// Unreal message handler.
		TSharedPtr< FGenericApplicationMessageHandler > MessageHandler;
	};
}