// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MediaBundle.h"

#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "Materials/MaterialInterface.h"


/* UMediaBundle
 *****************************************************************************/

bool UMediaBundle::OpenMediaSource()
{
	bool bResult = false;
	if (MediaSource)
	{
		bResult = true;

		// Only play once
		const EMediaState MediaState = MediaPlayer->GetPlayerFacade()->GetPlayer().IsValid() ? MediaPlayer->GetPlayerFacade()->GetPlayer()->GetControls().GetState() : EMediaState::Closed;
		if (MediaState == EMediaState::Closed || MediaState == EMediaState::Error)
		{
			bResult = MediaPlayer->OpenSource(MediaSource);
		}

		if (bResult)
		{
			++ReferenceCount;
		}
	}
	return bResult;
}

void UMediaBundle::CloseMediaSource()
{
	--ReferenceCount;
	if (ReferenceCount == 0)
	{
		MediaPlayer->Close();
	}
}

#if WITH_EDITOR
void UMediaBundle::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMediaBundle, MediaSource))
	{
		MediaPlayer->Close();
		if (MediaSource && ReferenceCount > 0)
		{
			MediaPlayer->OpenSource(MediaSource);
		}
	}
}
#endif
