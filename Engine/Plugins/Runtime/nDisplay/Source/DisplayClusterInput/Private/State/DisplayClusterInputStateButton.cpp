// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterInputStateButton.h"
#include "Misc/DisplayClusterInputLog.h"


bool FButtonState::OnButtonPressed(FGenericApplicationMessageHandler* MessageHandler, int32 ControllerId, bool bIsRepeat)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputButton);

	bool bResult = false;
	for (auto& Key : BindKeys)
	{
		if (MessageHandler->OnControllerButtonPressed(Key, ControllerId, bIsRepeat))
		{
			bResult = true;
		}
	}
	return bResult;
}

bool FButtonState::OnButtonReleased(FGenericApplicationMessageHandler* MessageHandler, int32 ControllerId, bool bIsRepeat)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputButton);

	bool bResult = false;
	for (auto& Key : BindKeys)
	{
		if (MessageHandler->OnControllerButtonReleased(Key, ControllerId, bIsRepeat))
		{
			bResult = true;
		}
	}
	return bResult;
}

//@todo: Should be made configurable and unified with other controllers handling of repeat
float FButtonState::InitialButtonRepeatDelay = 0.2f;
float FButtonState::ButtonRepeatDelay = 0.1f;

void FButtonState::UpdateEvents(FGenericApplicationMessageHandler* MessageHandler, int32 ControllerId, double CurrentTime)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputButton);

	bool bIsChanged = IsChanged();
	ApplyChanges();

	if (BindKeys.Num())
	{
		if (bIsChanged)
		{
			if (bIsPressedState)
			{
				OnButtonPressed(MessageHandler, ControllerId, false);
				// Set the timer for the first repeat
				NextRepeatTime = CurrentTime + InitialButtonRepeatDelay;
			}
			else
			{
				OnButtonReleased(MessageHandler, ControllerId, false);
			}
		}
		// Apply key repeat, if its time for that
		if (bIsPressedState && NextRepeatTime <= CurrentTime)
		{
			OnButtonPressed(MessageHandler, ControllerId, true);
			// Set the timer for the next repeat
			NextRepeatTime = CurrentTime + ButtonRepeatDelay;
		}
	}
	else
	{
		// no binds, reset repeat delay (run-time dynamic bind purpose)
		NextRepeatTime = CurrentTime + InitialButtonRepeatDelay;
	}
}

const FKey FButtonKey::Button_1("nDisplayButton0");
const FKey FButtonKey::Button_2("nDisplayButton1");
const FKey FButtonKey::Button_3("nDisplayButton2");
const FKey FButtonKey::Button_4("nDisplayButton3");
const FKey FButtonKey::Button_5("nDisplayButton4");
const FKey FButtonKey::Button_6("nDisplayButton5");
const FKey FButtonKey::Button_7("nDisplayButton6");
const FKey FButtonKey::Button_8("nDisplayButton7");
const FKey FButtonKey::Button_9("nDisplayButton8");
const FKey FButtonKey::Button_10("nDisplayButton9");
const FKey FButtonKey::Button_11("nDisplayButton10");
const FKey FButtonKey::Button_12("nDisplayButton11");
const FKey FButtonKey::Button_13("nDisplayButton12");
const FKey FButtonKey::Button_14("nDisplayButton13");
const FKey FButtonKey::Button_15("nDisplayButton14");
const FKey FButtonKey::Button_16("nDisplayButton15");
const FKey FButtonKey::Button_17("nDisplayButton16");
const FKey FButtonKey::Button_18("nDisplayButton17");
const FKey FButtonKey::Button_19("nDisplayButton18");
const FKey FButtonKey::Button_20("nDisplayButton19");

const FKey* FButtonKey::ButtonKeys[FButtonKey::TotalCount] =
{
	&FButtonKey::Button_1,
	&FButtonKey::Button_2,
	&FButtonKey::Button_3,
	&FButtonKey::Button_4,
	&FButtonKey::Button_5,
	&FButtonKey::Button_6,
	&FButtonKey::Button_7,
	&FButtonKey::Button_8,
	&FButtonKey::Button_9,
	&FButtonKey::Button_10,
	&FButtonKey::Button_11,
	&FButtonKey::Button_12,
	&FButtonKey::Button_13,
	&FButtonKey::Button_14,
	&FButtonKey::Button_15,
	&FButtonKey::Button_16,
	&FButtonKey::Button_17,
	&FButtonKey::Button_18,
	&FButtonKey::Button_19,
	&FButtonKey::Button_20
};
