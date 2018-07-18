// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Modules/ModuleInterface.h"

class IMediaPlayer;

/**
 * Interface for the MagicLeapMedia module.
 */
class IMagicLeapMediaModule : public IModuleInterface
{
public:
	/**
	 * Creates a MagicLeap based media player.
	 *
	 * @return A new media player, or nullptr if a player couldn't be created.
	 */
	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(class IMediaEventSink& EventSink) = 0;

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreateCameraPreviewPlayer(class IMediaEventSink& EventSink) = 0;

public:
	/** Virtual destructor. */
	virtual ~IMagicLeapMediaModule() { }
};

DECLARE_LOG_CATEGORY_EXTERN(LogMagicLeapMedia, All, All);

