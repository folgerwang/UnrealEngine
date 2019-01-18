// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


class FOpenColorIOLibHandler
{
public:
	static bool Initialize();
	static bool IsInitialized();
	static void Shutdown();

private:
	static void* LibHandle;
};
