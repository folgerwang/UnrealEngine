// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PropertyHandle.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"

#include "TargetPlatformAudioCustomization.h"

/**
* Detail customization for PS4 target settings panel
*/
class FLuminTargetSettingsDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	
	FAudioPluginWidgetManager AudioPluginManager;
};