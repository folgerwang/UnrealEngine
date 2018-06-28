// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaSettings.h"

/* UAjaMediaSettings
*****************************************************************************/

FAjaMediaMode UAjaMediaSettings::GetInputMediaMode(const FAjaMediaPort& InMediaPort) const
{
	FAjaMediaMode DefaultMode;
	const FAjaInputPortSettings* FoundSetting = InputPortSettings.FindByPredicate([&](const FAjaInputPortSettings& Setting) { return Setting.MediaPort == InMediaPort; });
	if (FoundSetting != nullptr)
	{
		DefaultMode = FoundSetting->MediaMode;
	}

	return DefaultMode;
}

FAjaMediaMode UAjaMediaSettings::GetOutputMediaMode(const FAjaMediaPort& InMediaPort) const
{
	FAjaMediaMode DefaultMode;
	const FAjaOutputPortSettings* FoundSetting = OutputPortSettings.FindByPredicate([&](const FAjaOutputPortSettings& Setting) { return Setting.MediaPort == InMediaPort; });
	if (FoundSetting != nullptr)
	{
		DefaultMode = FoundSetting->MediaMode;
	}

	return DefaultMode;
}
