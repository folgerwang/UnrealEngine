// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaOutput.h"


/* UAjaMediaOutput
*****************************************************************************/

UAjaMediaOutput::UAjaMediaOutput()
	: OutputType(EAjaMediaOutputType::FillOnly)
	, bOutputWithAutoCirculating(false)
	, bOutputTimecode(true)
	, bCopyVideoOnRenderThread(true)
	, bWaitForSyncEvent(false)
	, bClearBuffer(false)
	, ClearBufferColor(FColor::Green)
	, bEncodeTimecodeInTexel(false)
{
}

#if WITH_EDITOR
bool UAjaMediaOutput::CanEditChange(const UProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UAjaMediaOutput, KeyPort))
	{
		return OutputType == EAjaMediaOutputType::FillAndKey;
	}

	return true;
}

void UAjaMediaOutput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UAjaMediaOutput, bOutputWithAutoCirculating))
	{
		if (!bOutputWithAutoCirculating)
		{
			bWaitForSyncEvent = false;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif //WITH_EDITOR
