// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Android/AndroidApplication.h"
#include <Containers/Queue.h>

#include <ml_api.h>
#include <ml_keycodes.h>
#include <ml_input.h>

/**
* Lumin-specific application implementation.
*/
class APPLICATIONCORE_API FLuminApplication : public FAndroidApplication
{
public:
	/** Key events come in on separate threads so we queue them up and process them on the main thread. */
	class DeferredKeyEvent
	{
	public:
		enum Type
		{
			KeyDown,
			KeyUp,
			Char,
		};
	protected:
		Type      KeyEventType;
		MLKeyCode KeyCode;
		uint32_t  KeyData;
	public:
		inline DeferredKeyEvent()
		{
		}
		inline DeferredKeyEvent(Type InKeyEventType, MLKeyCode InKeyCode, uint32 ModifierMask)
			: KeyEventType(InKeyEventType)
			, KeyCode(InKeyCode)
			, KeyData(ModifierMask)
		{
		}
		inline DeferredKeyEvent(uint32 CharUtf32)
			: KeyEventType(Type::Char)
			, KeyData(CharUtf32)
		{
		}
		void SendModified(TSharedRef<FGenericApplicationMessageHandler> MessageHandler, uint32_t& InOutModifierMask) const;
	};

public:
	static FLuminApplication* CreateLuminApplication();

	FLuminApplication();
	~FLuminApplication();

	inline const MLHandle GetInputTracker() const
	{
		return InputTracker;
	}

	void AddDeferredKeyEvent(const DeferredKeyEvent& InDeferredEvent);

	// GenericApplication
	void Tick(const float TimeDelta) override;
	FModifierKeysState GetModifierKeys() const override;

protected:

	void InitializeInputCallbacks();

	MLHandle InputTracker = ML_INVALID_HANDLE;
	MLInputKeyboardCallbacks InputKeyboardCallbacks;
	TQueue<DeferredKeyEvent> DeferredKeyEvents;
	uint32 ModifierMask = 0;
};

extern FLuminApplication* LuminApplication;
