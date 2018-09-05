// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaOutput.h"
#include "BlackmagicMediaPrivate.h"


/* UBlackmagicMediaOutput
*****************************************************************************/

UBlackmagicMediaOutput::UBlackmagicMediaOutput()
	: OutputType(EBlackmagicMediaOutputType::FillOnly)
	, bClearBuffer(false)
	, ClearBufferColor(FColor::Green)
	, bOutputTimecode(true)
	, bCopyOnRenderThread(true)
	, bWaitForOutputFrame(true)
	, bWaitForVSyncEvent(true)
	, bVSyncEventOnAnotherThread(false)
	, bEncodeTimecodeInTexel(false)
{
}
