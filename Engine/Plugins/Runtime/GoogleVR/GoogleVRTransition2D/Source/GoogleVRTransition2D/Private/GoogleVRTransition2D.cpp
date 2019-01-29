// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GoogleVRTransition2D.h"
#include "GoogleVRTransition2DBPLibrary.h"

#define LOCTEXT_NAMESPACE "FGoogleVRTransition2DModule"

DEFINE_LOG_CATEGORY(LogGoogleVRTransition2D);

void FGoogleVRTransition2DModule::StartupModule()
{
	UGoogleVRTransition2DBPLibrary::Initialize();
}

void FGoogleVRTransition2DModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGoogleVRTransition2DModule, GoogleVRTransition2D)