// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IPinnedCommandListModule.h"
#include "SPinnedCommandList.h"
#include "Modules/ModuleManager.h"

class FPinnedCommandListModule : public IPinnedCommandListModule
{
public:
	virtual TSharedRef<IPinnedCommandList> CreatePinnedCommandList(const FName& InContextName) override
	{
		return SNew(SPinnedCommandList, InContextName);
	}
};

IMPLEMENT_MODULE(FPinnedCommandListModule, PinnedCommandList)
