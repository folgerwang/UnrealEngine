// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SpatializationEditorModule.h"
#include "Features/IModularFeatures.h"


void FSpatializationEditorModule::StartupModule()
{
}

void FSpatializationEditorModule::ShutdownModule()
{
	// Nothing done here for now.
}

IMPLEMENT_MODULE(FSpatializationEditorModule, SpatializationEditor)