// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Modules/ModuleInterface.h"

class IMediaPlayer;

/**
 * Interface for the MagicLeapMediaCodec module.
 */
class IMagicLeapMediaCodecModule : public IModuleInterface
{
public:
	/**
	 * Creates a MagicLeap codec based media player.
	 *
	 * @return A new media player, or nullptr if a player couldn't be created.
	 */
	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(class IMediaEventSink& EventSink) = 0;

public:
	/** Virtual destructor. */
	virtual ~IMagicLeapMediaCodecModule() { }
};

DECLARE_LOG_CATEGORY_EXTERN(LogMagicLeapMediaCodec, All, All);
