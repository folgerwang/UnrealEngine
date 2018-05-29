// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LuminGamepadInterface.h"
#include "Android/AndroidApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/PlatformTime.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/input.h>

#define CONTROLLER_VENDOR 0x45e
#define CONTROLLER_PRODUCT 0x28e
#define INPUT_DIR_NAME "/dev/input"

#define GAMEPAD_LEFT_THUMB_DEADZONE  7849
#define GAMEPAD_RIGHT_THUMB_DEADZONE 8689
#define GAMEPAD_TRIGGER_THRESHOLD    30

/**
* Evdev button mappings.
*/
enum EvdevButtons {
	kBtnA = 0x130,
	kBtnB = 0x131,
	kBtnX = 0x133,
	kBtnY = 0x134,
	kBtnLBumper = 0x136,
	kBtnRBumper = 0x137,
	kBtnBack = 0x13a,
	kBtnStart = 0x13b,
	kBtnLogo = 0x13c,
	kBtnThumbR = 0x13d,
	kBtnThumbL = 0x13e,
};

/**
* Evdev axes mappings.
*/
enum EvdevAxes {
	kAbsLeftX = 0x00,
	kAbsLeftY = 0x01,
	kAbsLTrigger = 0x02,
	kAbsRightX = 0x03,
	kAbsRightY = 0x04,
	kAbsRTrigger = 0x05,
	kAbsDPadX = 0x10,
	kAbsDPadY = 0x11,
};

TSharedRef< IInputDevice > FLuminGamepadInterface::Create(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
	return MakeShareable(new FLuminGamepadInterface(InMessageHandler));
}


FLuminGamepadInterface::FLuminGamepadInterface(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
	: MessageHandler(InMessageHandler)
	, bGamepadAttached(false)
	, bInitialized(false)
	, INotifyFD(-1)
	, WatchFD(-1)
{
	for (int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_LUMIN_GAMEPADS; ++ControllerIndex)
	{
		FControllerState& ControllerState = ControllerStates[ControllerIndex];
		FMemory::Memzero(&ControllerState, sizeof(FControllerState));

		ControllerState.ControllerId = ControllerIndex;
		ControllerState.ControllerFD = -1;
	}

	InitialButtonRepeatDelay = 0.2f;
	ButtonRepeatDelay = 0.1f;

	// In the engine, all controllers map to xbox controllers for consistency 
	X360ToXboxControllerMapping[0] = 0;		// A
	X360ToXboxControllerMapping[1] = 1;		// B
	X360ToXboxControllerMapping[2] = 2;		// X
	X360ToXboxControllerMapping[3] = 3;		// Y
	X360ToXboxControllerMapping[4] = 4;		// L1
	X360ToXboxControllerMapping[5] = 5;		// R1
	X360ToXboxControllerMapping[6] = 7;		// Back 
	X360ToXboxControllerMapping[7] = 6;		// Start
	X360ToXboxControllerMapping[8] = 8;		// Left thumbstick
	X360ToXboxControllerMapping[9] = 9;		// Right thumbstick
	X360ToXboxControllerMapping[10] = 10;	// L2
	X360ToXboxControllerMapping[11] = 11;	// R2
	X360ToXboxControllerMapping[12] = 12;	// Dpad up
	X360ToXboxControllerMapping[13] = 13;	// Dpad down
	X360ToXboxControllerMapping[14] = 14;	// Dpad left
	X360ToXboxControllerMapping[15] = 15;	// Dpad right
	X360ToXboxControllerMapping[16] = 16;	// Left stick up
	X360ToXboxControllerMapping[17] = 17;	// Left stick down
	X360ToXboxControllerMapping[18] = 18;	// Left stick left
	X360ToXboxControllerMapping[19] = 19;	// Left stick right
	X360ToXboxControllerMapping[20] = 20;	// Right stick up
	X360ToXboxControllerMapping[21] = 21;	// Right stick down
	X360ToXboxControllerMapping[22] = 22;	// Right stick left
	X360ToXboxControllerMapping[23] = 23;	// Right stick right

	Buttons[0] = FGamepadKeyNames::FaceButtonBottom;
	Buttons[1] = FGamepadKeyNames::FaceButtonRight;
	Buttons[2] = FGamepadKeyNames::FaceButtonLeft;
	Buttons[3] = FGamepadKeyNames::FaceButtonTop;
	Buttons[4] = FGamepadKeyNames::LeftShoulder;
	Buttons[5] = FGamepadKeyNames::RightShoulder;
	Buttons[6] = FGamepadKeyNames::SpecialRight;
	Buttons[7] = FGamepadKeyNames::SpecialLeft;
	Buttons[8] = FGamepadKeyNames::LeftThumb;
	Buttons[9] = FGamepadKeyNames::RightThumb;
	Buttons[10] = FGamepadKeyNames::LeftTriggerThreshold;
	Buttons[11] = FGamepadKeyNames::RightTriggerThreshold;
	Buttons[12] = FGamepadKeyNames::DPadUp;
	Buttons[13] = FGamepadKeyNames::DPadDown;
	Buttons[14] = FGamepadKeyNames::DPadLeft;
	Buttons[15] = FGamepadKeyNames::DPadRight;
	Buttons[16] = FGamepadKeyNames::LeftStickUp;
	Buttons[17] = FGamepadKeyNames::LeftStickDown;
	Buttons[18] = FGamepadKeyNames::LeftStickLeft;
	Buttons[19] = FGamepadKeyNames::LeftStickRight;
	Buttons[20] = FGamepadKeyNames::RightStickUp;
	Buttons[21] = FGamepadKeyNames::RightStickDown;
	Buttons[22] = FGamepadKeyNames::RightStickLeft;
	Buttons[23] = FGamepadKeyNames::RightStickRight;
}

FLuminGamepadInterface::~FLuminGamepadInterface()
{
	for (int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_LUMIN_GAMEPADS; ++ControllerIndex)
	{
		FControllerState& ControllerState = ControllerStates[ControllerIndex];
		if (-1 != ControllerState.ControllerFD)
		{
			close(ControllerState.ControllerFD);
			ControllerState.ControllerFD = -1;
		}
	}

	inotify_rm_watch(INotifyFD, WatchFD);
	close(INotifyFD);
}

void FLuminGamepadInterface::Initialize()
{
	check(!bInitialized);

	// Extend filesystems to notice changes to the filesystem, and report those changes to applications 
	INotifyFD = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (-1 == INotifyFD)
	{
		UE_LOG(LogHAL, Warning, TEXT("Failed to initialize FLuminGamepadInterface file system change notification: %s"), ANSI_TO_TCHAR(strerror(errno)));
		return;
	}

	WatchFD = inotify_add_watch(INotifyFD, INPUT_DIR_NAME, IN_CREATE);

	if (-1 == WatchFD)
	{
		UE_LOG(LogHAL, Warning, TEXT("Failed to initialize FLuminGamepadInterface input directory watcher: %s"), ANSI_TO_TCHAR(strerror(errno)));
		return;
	}

	FindController();
	bInitialized = true;
}

void FLuminGamepadInterface::FindController()
{
	DIR* Directory = opendir(INPUT_DIR_NAME);
	bGamepadAttached = false;

	if (nullptr == Directory)
	{
		UE_LOG(LogHAL, Warning, TEXT("Failed to open FLuminGamepadInterface input directory: %s"), ANSI_TO_TCHAR(strerror(errno)));
		return;
	}

	struct dirent* DirEntity;

	while (nullptr != (DirEntity = readdir(Directory)))
	{
		char Path[20];
		snprintf(Path, sizeof(Path), "%s/%s", INPUT_DIR_NAME, DirEntity->d_name);

		bGamepadAttached = OpenController(Path);
		if (bGamepadAttached)
		{
			break;
		}
	}

	closedir(Directory);
}

bool FLuminGamepadInterface::OpenController(const ANSICHAR* ControllerPath)
{
	int DeviceFD = open(ControllerPath, O_RDONLY | O_NONBLOCK);

	if (-1 == DeviceFD)
	{
		UE_LOG(LogHAL, Warning, TEXT("Failed to open controller '%s': %s"), ANSI_TO_TCHAR(ControllerPath), ANSI_TO_TCHAR(strerror(errno)));
		return false;
	}

	struct input_id InputInfo;
	ioctl(DeviceFD, EVIOCGID, &InputInfo);

	if ((InputInfo.vendor == CONTROLLER_VENDOR) && (InputInfo.product == CONTROLLER_PRODUCT))
	{
		char Name[128] = { 0 };

		ioctl(DeviceFD, EVIOCGNAME(sizeof(Name)), Name);
		UE_LOG(LogHAL, Log, TEXT("Controller - %s: %s"), ANSI_TO_TCHAR(Name), ANSI_TO_TCHAR(strerror(errno)));
		// @todo Lumin support more than one controller?

		ControllerStates[0].ControllerFD = DeviceFD;
		return true;
	}
	else
	{
		close(DeviceFD);
	}

	return false;
}

float ShortToNormalizedFloat(int AxisVal)
{
	// normalize [-32768..32767] -> [-1..1]
	const float Norm = (AxisVal <= 0 ? 32768.f : 32767.f);
	return float(AxisVal) / Norm;
}

void FLuminGamepadInterface::SendControllerEvents()
{
	if (!bInitialized)
	{
		Initialize();
	}

	if (bInitialized)
	{
		ssize_t Offset = 0;
		char Buffer[2048];
		const ssize_t Size = read(INotifyFD, Buffer, sizeof(Buffer));

		// Sift through our inotify buffer for relevant events. In this case file creation in the events directory.
		while (Size > Offset)
		{
			const struct inotify_event* Event = (struct inotify_event*) (Buffer + Offset);

			// @todo Lumin support more than one controller?
			if ((IN_CREATE == (Event->mask & IN_CREATE)) && (-1 == ControllerStates[0].ControllerFD))
			{
				FindController();
			}

			Offset += sizeof(struct inotify_event) + Event->len;
		}

		for (int i = 0; i < MAX_NUM_LUMIN_GAMEPADS; ++i)
		{
			if (-1 != ControllerStates[i].ControllerFD)
			{
				FControllerState& ControllerState = ControllerStates[i];
				// Make a copy that we'll apply events to
				FControllerState CurrentState = ControllerStates[i];

				// process available events
				for (;;)
				{
					struct input_event Event;

					if (read(ControllerStates[i].ControllerFD, &Event, sizeof(Event)) < 0)
					{
						if (errno == ENODEV)
						{
							UE_LOG(LogHAL, Log, TEXT("Controller Disconnected %s"), ANSI_TO_TCHAR(strerror(errno)));
							close(ControllerStates[i].ControllerFD);
							ControllerStates[i].ControllerFD = -1;
						}
						break;
					}

					if (Event.type == EV_ABS)
					{
						switch (Event.code)
						{
						case kAbsLeftX:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[18]] = !!(Event.value < -GAMEPAD_LEFT_THUMB_DEADZONE);
							CurrentState.ButtonStates[X360ToXboxControllerMapping[19]] = !!(Event.value > GAMEPAD_LEFT_THUMB_DEADZONE);
							CurrentState.LeftXAnalog = Event.value;
							break;
						case kAbsLeftY:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[16]] = !!(Event.value < -GAMEPAD_LEFT_THUMB_DEADZONE);
							CurrentState.ButtonStates[X360ToXboxControllerMapping[17]] = !!(Event.value > GAMEPAD_LEFT_THUMB_DEADZONE);
							CurrentState.LeftYAnalog = Event.value;
							break;
						case kAbsLTrigger:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[10]] = !!(Event.value > GAMEPAD_TRIGGER_THRESHOLD);
							CurrentState.LeftTriggerAnalog = Event.value;
							break;
						case kAbsRightX:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[22]] = !!(Event.value < -GAMEPAD_RIGHT_THUMB_DEADZONE);
							CurrentState.ButtonStates[X360ToXboxControllerMapping[23]] = !!(Event.value > GAMEPAD_RIGHT_THUMB_DEADZONE);
							CurrentState.RightXAnalog = Event.value;
							break;
						case kAbsRightY:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[20]] = !!(Event.value < -GAMEPAD_RIGHT_THUMB_DEADZONE);
							CurrentState.ButtonStates[X360ToXboxControllerMapping[21]] = !!(Event.value > GAMEPAD_RIGHT_THUMB_DEADZONE);
							CurrentState.RightYAnalog = Event.value;
							break;
						case kAbsRTrigger:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[11]] = !!(Event.value > GAMEPAD_TRIGGER_THRESHOLD);
							CurrentState.RightTriggerAnalog = Event.value;
							break;
						case kAbsDPadX:
							// todo @lumin these might be flipped?
							// left
							CurrentState.ButtonStates[X360ToXboxControllerMapping[14]] = !!(Event.value < 0);
							// right
							CurrentState.ButtonStates[X360ToXboxControllerMapping[15]] = !!(Event.value > 0);
							break;
						case kAbsDPadY:
							// todo @lumin these might be flipped?
							// up 
							CurrentState.ButtonStates[X360ToXboxControllerMapping[12]] = !!(Event.value < 0);
							// down
							CurrentState.ButtonStates[X360ToXboxControllerMapping[13]] = !!(Event.value > 0);
							break;
						default:
							UE_LOG(LogHAL, Warning, TEXT("EV_ABS : Unknown - %d "), Event.value);
							break;
						}
					}
					else if (Event.type == EV_KEY)
					{
						switch (Event.code)
						{
						case kBtnA:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[0]] = Event.value != 0;
							break;
						case kBtnB:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[1]] = Event.value != 0;
							break;
						case kBtnX:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[2]] = Event.value != 0;
							break;
						case kBtnY:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[3]] = Event.value != 0;
							break;
						case kBtnLBumper:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[4]] = Event.value != 0;
							break;
						case kBtnRBumper:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[5]] = Event.value != 0;
							break;
						case kBtnBack:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[6]] = Event.value != 0;
							break;
						case kBtnStart:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[7]] = Event.value != 0;
							break;
						case kBtnThumbL:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[8]] = Event.value != 0;
							break;
						case kBtnThumbR:
							CurrentState.ButtonStates[X360ToXboxControllerMapping[9]] = Event.value != 0;
							break;
						default:
							UE_LOG(LogHAL, Warning, TEXT("EV_KEY : Unknown - %d "), Event.value);
							break;
						}
					}
				}

				// Now we've processed all available events for this frame, let's send messages and update our controller state

				// Check Analog state

				if (ControllerState.LeftXAnalog != CurrentState.LeftXAnalog)
				{
					MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogX, ControllerState.ControllerId, ShortToNormalizedFloat(CurrentState.LeftXAnalog));
					ControllerState.LeftXAnalog = CurrentState.LeftXAnalog;
				}

				if (ControllerState.LeftYAnalog != CurrentState.LeftYAnalog)
				{
					// This is inverted on Lumin
					MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogY, ControllerState.ControllerId, -ShortToNormalizedFloat(CurrentState.LeftYAnalog));
					ControllerState.LeftYAnalog = CurrentState.LeftYAnalog;
				}

				if (ControllerState.RightXAnalog != CurrentState.RightXAnalog)
				{
					MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogX, ControllerState.ControllerId, ShortToNormalizedFloat(CurrentState.RightXAnalog));
					ControllerState.RightXAnalog = CurrentState.RightXAnalog;
				}

				if (ControllerState.RightYAnalog != CurrentState.RightYAnalog)
				{
					// This is inverted on Lumin
					MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogY, ControllerState.ControllerId, -ShortToNormalizedFloat(CurrentState.RightYAnalog));
					ControllerState.RightYAnalog = CurrentState.RightYAnalog;
				}

				if (ControllerState.LeftTriggerAnalog != CurrentState.LeftTriggerAnalog)
				{
					MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftTriggerAnalog, ControllerState.ControllerId, CurrentState.LeftTriggerAnalog / 255.f);
					ControllerState.LeftTriggerAnalog = CurrentState.LeftTriggerAnalog;
				}

				if (ControllerState.RightTriggerAnalog != CurrentState.RightTriggerAnalog)
				{
					MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightTriggerAnalog, ControllerState.ControllerId, CurrentState.RightTriggerAnalog / 255.f);
					ControllerState.RightTriggerAnalog = CurrentState.RightTriggerAnalog;
				}

				const double CurrentTime = FPlatformTime::Seconds();

				// For each button check against the previous state and send the correct message if any
				for (int32 ButtonIndex = 0; ButtonIndex < MAX_NUM_CONTROLLER_BUTTONS; ++ButtonIndex)
				{
					if (CurrentState.ButtonStates[ButtonIndex] != ControllerState.ButtonStates[ButtonIndex])
					{
						if (CurrentState.ButtonStates[ButtonIndex])
						{
							MessageHandler->OnControllerButtonPressed(Buttons[ButtonIndex], ControllerState.ControllerId, false);
						}
						else
						{
							MessageHandler->OnControllerButtonReleased(Buttons[ButtonIndex], ControllerState.ControllerId, false);
						}

						if (CurrentState.ButtonStates[ButtonIndex] != 0)
						{
							// this button was pressed - set the button's NextRepeatTime to the InitialButtonRepeatDelay
							ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
						}
					}
					else if (CurrentState.ButtonStates[ButtonIndex] != 0 && ControllerState.NextRepeatTime[ButtonIndex] <= CurrentTime)
					{
						MessageHandler->OnControllerButtonPressed(Buttons[ButtonIndex], ControllerState.ControllerId, true);

						// set the button's NextRepeatTime to the ButtonRepeatDelay
						ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
					}

					// Update the state for next time
					ControllerState.ButtonStates[ButtonIndex] = CurrentState.ButtonStates[ButtonIndex];
				}

			}
		}
	}
}

void FLuminGamepadInterface::SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

bool FLuminGamepadInterface::IsGamepadAttached() const
{
	// TODO: check controller connection status.
	return bGamepadAttached;
}
