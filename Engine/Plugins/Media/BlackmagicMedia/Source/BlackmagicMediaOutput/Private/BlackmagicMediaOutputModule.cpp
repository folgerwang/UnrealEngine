// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IBlackmagicMediaOutputModule.h"

#include "BlackmagicMediaFrameGrabberProtocol.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "BlackmagicMediaOutput"

DEFINE_LOG_CATEGORY(LogBlackmagicMediaOutput);

class FBlackmagicMediaOutputModule : public IBlackmagicMediaOutputModule
{
};

IMPLEMENT_MODULE(FBlackmagicMediaOutputModule, BlackmagicMediaOutput )

#undef LOCTEXT_NAMESPACE
