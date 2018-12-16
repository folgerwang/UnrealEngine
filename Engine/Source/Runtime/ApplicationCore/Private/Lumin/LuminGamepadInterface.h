// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "IInputDevice.h"

/** Max number of controllers. */
#define MAX_NUM_LUMIN_GAMEPADS 1

/** Max number of controller buttons.  Must be < 256*/
#define MAX_NUM_CONTROLLER_BUTTONS 24

/**
* Interface class for Gamepad devices (xbox 360 controller)
*/
class FLuminGamepadInterface : public IInputDevice
{
public:
	virtual ~FLuminGamepadInterface();

	static TSharedRef< IInputDevice > Create(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);

	/**
	* IInputDevice pass through functions
	*/

	/** Tick the interface (e.g. check for new controllers) */
	virtual void Tick(float DeltaTime) override {};

	/** Poll for controller state and send events if needed */
	virtual void SendControllerEvents() override;

	/** Set which MessageHandler will get the events from SendControllerEvents. */
	virtual void SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override;

	/** Exec handler to allow console commands to be passed through for debugging */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override { return false; };

	/**
	* IForceFeedbackSystem pass through functions
	*/
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override {};
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override {};

	virtual bool IsGamepadAttached() const override;

private:

	FLuminGamepadInterface(const TSharedRef< FGenericApplicationMessageHandler >& MessageHandler);

	void Initialize();
	void FindController();
	bool OpenController(const ANSICHAR* ControllerPath);

	struct FControllerState
	{
		/** Last frame's button states, so we only send events on edges */
		bool ButtonStates[MAX_NUM_CONTROLLER_BUTTONS];

		/** Next time a repeat event should be generated for each button */
		double NextRepeatTime[MAX_NUM_CONTROLLER_BUTTONS];

		/** Raw Left thumb x analog value */
		int16 LeftXAnalog;

		/** Raw left thumb y analog value */
		int16 LeftYAnalog;

		/** Raw Right thumb x analog value */
		int16 RightXAnalog;

		/** Raw Right thumb x analog value */
		int16 RightYAnalog;

		/** Left Trigger analog value */
		uint8 LeftTriggerAnalog;

		/** Right trigger analog value */
		uint8 RightTriggerAnalog;

		/** Id of the controller */
		int32 ControllerId;

		/** Controller file descriptor*/
		int ControllerFD;
	};

	/** Are we successfully initialized */
	bool bInitialized;

	/** In the engine, all controllers map to xbox controllers for consistency */
	uint8	X360ToXboxControllerMapping[MAX_NUM_CONTROLLER_BUTTONS];

	/** Controller states */
	FControllerState ControllerStates[MAX_NUM_LUMIN_GAMEPADS];

	/** Delay before sending a repeat message after a button was first pressed */
	float InitialButtonRepeatDelay;

	/** Delay before sending a repeat message after a button has been pressed for a while */
	float ButtonRepeatDelay;

	/**  */
	FGamepadKeyNames::Type Buttons[MAX_NUM_CONTROLLER_BUTTONS];

	/**  */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	bool bGamepadAttached;

	/** File system notify file descriptor */
	int INotifyFD;
	/** Watch file descriptor */
	int WatchFD;
};
