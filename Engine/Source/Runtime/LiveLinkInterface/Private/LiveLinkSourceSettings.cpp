// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkSourceSettings.h"
#include "UObject/EnterpriseObjectVersion.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void ULiveLinkSourceSettings::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// LiveLinkSourceSettings aren't persistently stored by the engine,
	// but they could have been elsewhere.

	Ar.UsingCustomVersion(FEnterpriseObjectVersion::GUID);
#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading() && FEnterpriseObjectVersion::LiveLinkTimeSynchronization > Ar.CustomVer(FEnterpriseObjectVersion::GUID))
	{
		Mode = InterpolationSettings.bUseInterpolation_DEPRECATED ? ELiveLinkSourceMode::Interpolated : ELiveLinkSourceMode::Default;
	}
#endif
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS