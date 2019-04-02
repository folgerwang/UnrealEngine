// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

class IMagicLeapInputDevice;

class IMagicLeapHMD
{
public:
	// TODO: no point keeping a single function here.
	virtual bool IsInitialized() const = 0;
};
