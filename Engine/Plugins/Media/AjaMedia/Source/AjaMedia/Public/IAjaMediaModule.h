// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class IMediaEventSink;
class IMediaPlayer;

/**
 * Interface for the AjaMedia module.
 */
class IAjaMediaModule : public IModuleInterface
{
public:

	/**
	 * Create an Aja based media player.
	 * @param EventSink The object that receives media events from the player.
	 * @return A new media player, or nullptr if a player couldn't be created.
	 */
	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) = 0;

	/** @return true if the Aja module and Aja dll could be loaded */
	virtual bool IsInitialized() const = 0;
};

