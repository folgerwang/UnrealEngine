// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Lumin/LuminApplication.h"
#include "Misc/ScopeLock.h"
#include "Misc/ConfigCacheIni.h"

FLuminApplication* LuminApplication = nullptr;

void FLuminApplication::DeferredKeyEvent::SendModified(
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler, uint32_t& InOutModifierMask) const
{
	switch (KeyEventType)
	{
	case Type::KeyDown:
		InOutModifierMask = KeyData;
		MessageHandler->OnKeyDown(KeyCode, 0, false);
		break;
	case Type::KeyUp:
		InOutModifierMask = KeyData;
		MessageHandler->OnKeyUp(KeyCode, 0, false);
		break;
	case Type::Char:
		MessageHandler->OnKeyChar(KeyData, false);
		break;
	}
}

FLuminApplication::FLuminApplication()
{
	// Pull tracking preference from config file
	MLInputConfiguration InputConfig = { };

	InputConfig.dof[0] = InputConfig.dof[1] = MLInputControllerDof_6;

	FString EnumVal;
	GConfig->GetString(TEXT("/Script/LuminRuntimeSettings.LuminRuntimeSettings"),
		TEXT("ControllerTrackingType"), EnumVal, GEngineIni);
	if (EnumVal.Len() > 0)
	{
		// Can't get enums as strings at this level
		if (0 == EnumVal.Compare("NotTracked", ESearchCase::IgnoreCase))
		{
			InputConfig.dof[0] = InputConfig.dof[1] = MLInputControllerDof_None;
		}
		else if (0 == EnumVal.Compare("InertialOnly", ESearchCase::IgnoreCase))
		{
			InputConfig.dof[0] = InputConfig.dof[1] = MLInputControllerDof_3;
		}
	}

	// Create the input tracker
	MLResult Result = MLInputCreate(&InputConfig, &InputTracker);
	if (MLResult_Ok != Result)
	{
		UE_LOG(LogCore, Error, TEXT("FLuminApplication: Unable to initialize "
			"input system: %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
		InputTracker = ML_INVALID_HANDLE;
	}
	else
	{
		InitializeInputCallbacks();
	}
}

FLuminApplication::~FLuminApplication()
{
	if (ML_INVALID_HANDLE != InputTracker)
	{
		auto Result = MLInputDestroy(InputTracker);
		if (MLResult_Ok != Result)
		{
			UE_LOG(LogCore, Error, TEXT("FLuminApplication::~FLuminApplication: "
				"Failure in MLInputDestroy: %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
		}
		InputTracker = ML_INVALID_HANDLE;
	}
	LuminApplication = nullptr;
}

FLuminApplication* FLuminApplication::CreateLuminApplication()
{
	LuminApplication = new FLuminApplication;
	return LuminApplication;
}

void FLuminApplication::Tick(const float TimeDelta)
{
	// Roll through the pending events at our leisure
	while (!DeferredKeyEvents.IsEmpty())
	{
		DeferredKeyEvent CurrKeyEvent;
		DeferredKeyEvents.Dequeue(CurrKeyEvent);
		CurrKeyEvent.SendModified(MessageHandler, ModifierMask);
	}
}

void FLuminApplication::InitializeInputCallbacks()
{
	FMemory::Memset(&InputKeyboardCallbacks, 0, sizeof(InputKeyboardCallbacks));

	InputKeyboardCallbacks.on_char = [](uint32_t char_utf32, void *data)
	{
		auto application = reinterpret_cast<FLuminApplication*>(data);
		if (application)
		{
			application->AddDeferredKeyEvent(DeferredKeyEvent(char_utf32));
		}
	};

	InputKeyboardCallbacks.on_key_down = [](MLKeyCode key_code, uint32 modifier_mask, void *data)
	{
		auto application = reinterpret_cast<FLuminApplication*>(data);
		if (application)
		{
			application->AddDeferredKeyEvent(DeferredKeyEvent(
				DeferredKeyEvent::Type::KeyDown, key_code, modifier_mask));
		}
	};

	InputKeyboardCallbacks.on_key_up = [](MLKeyCode key_code, uint32 modifier_mask, void *data)
	{
		auto application = reinterpret_cast<FLuminApplication*>(data);
		if (application)
		{
			application->AddDeferredKeyEvent(DeferredKeyEvent(
				DeferredKeyEvent::Type::KeyUp, key_code, modifier_mask));
		}
	};

	MLResult Result = MLInputSetKeyboardCallbacks(InputTracker, &InputKeyboardCallbacks, this);
	if (MLResult_Ok != Result)
	{
		UE_LOG(LogCore, Error, TEXT("FLuminApplication::InitializeInputCallbacks: "
			"unable to set keyboard callbacks: %s"), UTF8_TO_TCHAR(MLGetResultString(Result)));
	}
}

void FLuminApplication::AddDeferredKeyEvent(const DeferredKeyEvent& InDeferredEvent)
{
	DeferredKeyEvents.Enqueue(InDeferredEvent);
}

FModifierKeysState FLuminApplication::GetModifierKeys() const
{
	// ML platform only exposes single Shift, Control, and Alt modifiers; we'll map them to the
	// left because that is the most common location on single-modifier keyboards
	return FModifierKeysState(
		(ModifierMask & MLKEYMODIFIER_SHIFT) != 0,    // bInIsLeftShiftDown
		false,                                        // bInIsRightShiftDown
		(ModifierMask & MLKEYMODIFIER_CTRL) != 0,     // bInIsLeftControlDown
		false,                                        // bInIsRightControlDown
		(ModifierMask & MLKEYMODIFIER_ALT) != 0,      // bInIsLeftAltDown
		false,                                        // bInIsRightAltDown
		false,                                        // bInIsLeftCommandDown
		false,                                        // bInIsRightCommandDown
		(ModifierMask & MLKEYMODIFIER_CAPS_LOCK) != 0 // bInAreCapsLocked
	);
}

