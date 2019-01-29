// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/ICursor.h"
#include "Templates/SharedPointer.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "Containers/BitArray.h"
THIRD_PARTY_INCLUDES_START
	#ifdef HTML5_USE_SDL2
		#include <SDL.h>
	#endif
	#include <emscripten/html5.h>
THIRD_PARTY_INCLUDES_END

#define HTML5_INPUT_INTERFACE_MAX_CONTROLLERS 5
#define HTML5_INPUT_INTERFACE_BUTTON_MAPPING_CAP 16

/**
 * Interface class for HTML5 input devices
 */
class FHTML5InputInterface
{
public:
	/** Initialize the interface */
	static TSharedRef< FHTML5InputInterface > Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor );

public:

	~FHTML5InputInterface() {}

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);

#ifdef HTML5_USE_SDL2
	/** Tick the interface (i.e check for new controllers) */
	void Tick( float DeltaTime, const SDL_Event& Event,TSharedRef < FGenericWindow>& ApplicationWindow );
#endif

	/**
	 * Poll for controller state and send events if needed
	 */
	void SendControllerEvents();


private:

	FHTML5InputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor );


private:

	TSharedRef< FGenericApplicationMessageHandler > MessageHandler;
	const TSharedPtr< ICursor > Cursor;

	TBitArray<FDefaultBitArrayAllocator> KeyStates;

	EmscriptenGamepadEvent PrevGamePadState[HTML5_INPUT_INTERFACE_MAX_CONTROLLERS];
	double LastPressedTime[HTML5_INPUT_INTERFACE_MAX_CONTROLLERS][HTML5_INPUT_INTERFACE_BUTTON_MAPPING_CAP];

};
