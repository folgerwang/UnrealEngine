// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterInputStateAnalog.h"
#include "IDisplayClusterInputModule.h"
#include "Misc/DisplayClusterInputLog.h"


bool FAnalogState::OnAnalogChanges(FGenericApplicationMessageHandler* MessageHandler, int32 ControllerId)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputAnalog);

	bool bResult = false;
	for (auto& Key : BindKeys)
	{
		if (MessageHandler->OnControllerAnalog(Key, ControllerId, AnalogState))
		{
			bResult = true;
		}
	}
	return bResult;
}

void FAnalogState::UpdateEvents(FGenericApplicationMessageHandler* MessageHandler, int32 ControllerId, double CurrentTime)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterInputAnalog);

	if (IsChanged())
	{
		ApplyChanges();
		OnAnalogChanges(MessageHandler, ControllerId);
	}
}



const FKey FAnalogKey::Analog_1("nDisplayAnalog0");
const FKey FAnalogKey::Analog_2("nDisplayAnalog1");
const FKey FAnalogKey::Analog_3("nDisplayAnalog2");
const FKey FAnalogKey::Analog_4("nDisplayAnalog3");
const FKey FAnalogKey::Analog_5("nDisplayAnalog4");
const FKey FAnalogKey::Analog_6("nDisplayAnalog5");
const FKey FAnalogKey::Analog_7("nDisplayAnalog6");
const FKey FAnalogKey::Analog_8("nDisplayAnalog7");
const FKey FAnalogKey::Analog_9("nDisplayAnalog8");
const FKey FAnalogKey::Analog_10("nDisplayAnalog9");

const FKey FAnalogKey::Analog_11("nDisplayAnalog10");
const FKey FAnalogKey::Analog_12("nDisplayAnalog11");
const FKey FAnalogKey::Analog_13("nDisplayAnalog12");
const FKey FAnalogKey::Analog_14("nDisplayAnalog13");
const FKey FAnalogKey::Analog_15("nDisplayAnalog14");
const FKey FAnalogKey::Analog_16("nDisplayAnalog15");
const FKey FAnalogKey::Analog_17("nDisplayAnalog16");
const FKey FAnalogKey::Analog_18("nDisplayAnalog17");
const FKey FAnalogKey::Analog_19("nDisplayAnalog18");
const FKey FAnalogKey::Analog_20("nDisplayAnalog19");

const FKey* FAnalogKey::AnalogKeys[FAnalogKey::TotalCount] =
{
	&FAnalogKey::Analog_1,
	&FAnalogKey::Analog_2,
	&FAnalogKey::Analog_3,
	&FAnalogKey::Analog_4,
	&FAnalogKey::Analog_5,
	&FAnalogKey::Analog_6,
	&FAnalogKey::Analog_7,
	&FAnalogKey::Analog_8,
	&FAnalogKey::Analog_9,
	&FAnalogKey::Analog_10,

	&FAnalogKey::Analog_11,
	&FAnalogKey::Analog_12,
	&FAnalogKey::Analog_13,
	&FAnalogKey::Analog_14,
	&FAnalogKey::Analog_15,
	&FAnalogKey::Analog_16,
	&FAnalogKey::Analog_17,
	&FAnalogKey::Analog_18,
	&FAnalogKey::Analog_19,
	&FAnalogKey::Analog_20
};
