// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LuminTargetSettingsDetails.h"

#include "SExternalImageReference.h"
#include "EditorDirectories.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "UObject/UnrealType.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LuminTargetSettingsDetails"

TSharedRef<IDetailCustomization> FLuminTargetSettingsDetails::MakeInstance()
{
	return MakeShareable(new FLuminTargetSettingsDetails);
}

void FLuminTargetSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	AudioPluginManager.BuildAudioCategory(DetailBuilder, EAudioPlatform::Lumin);
}

#undef LOCTEXT_NAMESPACE
