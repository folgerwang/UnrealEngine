// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IInputDevice.h"
#include "Containers/Queue.h"
#include "GenericPlatform/GenericApplication.h"

DECLARE_LOG_CATEGORY_EXTERN(PixelStreamingInputDevice, Log, VeryVerbose);

class FPixelStreamingInputDevice : public IInputDevice
{
public:

	/**
	 * The pixel streaming input device takes input events and passes them to
	 * a message handler.
	 * @param InMessageHandler - The message handler which events should be passed to.
	 * @param InInputComponents - The preexisting list of pixel streaming input components.
	 */
	FPixelStreamingInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, TArray<class UPixelStreamingInputComponent*>& InInputComponents);

	/**
	 * Tell the input device about a new pixel streaming input component.
	 * @param InInputComponent - The new pixel streaming input component.
	 */
	void AddInputComponent(class UPixelStreamingInputComponent* InInputComponent)
	{
		InputComponents.Add(InInputComponent);
	}

	/*
	 * Tell the input device that a pixel streaming input component is no longer
	 * relevant.
	 * @param InInputComponent - The pixel streaming input component which is no longer relevant.
	 */
	void RemoveInputComponent(class UPixelStreamingInputComponent* InInputComponent)
	{
		InputComponents.Remove(InInputComponent);
	}

	/** Tick the interface (e.g. check for new controllers) */
	virtual void Tick(float DeltaTime) override;

	/** Poll for controller state and send events if needed */
	virtual void SendControllerEvents() override;

	/** Set which MessageHandler will get the events from SendControllerEvents. */
	virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;

	/** Exec handler to allow console commands to be passed through for debugging */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/**
	* IForceFeedbackSystem pass through functions
	*/
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;

	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override;

	/** The types of event which can be processed by the device. */
	enum class EventType
	{
		UNDEFINED,		/** No value. */
		KEY_DOWN,		/** A key has been pushed down. */
		KEY_UP,			/** A key has been released. */
		KEY_PRESS,		/** A key has been pressed and a character has been input. */
		MOUSE_ENTER,	/** The mouse has entered canvas. */
		MOUSE_LEAVE,	/** The mouse has left the canvas. */
		MOUSE_MOVE,		/** The mouse has been moved. */
		MOUSE_DOWN,		/** A mouse button has been clicked. */
		MOUSE_UP,		/** A mouse button has been released. */
		MOUSE_WHEEL,	/** The mouse wheel was scrolled. */
		TOUCH_START,	/** A finger is put down onto the canvas. */
		TOUCH_END,		/** A finger is lifted from the canvas. */
		TOUCH_MOVE		/** A finger is being dragged along the surface of the canvas. */
	};

	/** A general input event. */
	struct FEvent
	{
		/** The type of the general event. */
		EventType Event;

		/** A generic piece of data which is used to hold information about the
		 * event, specialized by making a union with an appropriate struct. */
		union
		{
			uint64 Word;

			struct   /** KEY_DOWN */
			{
				uint8 KeyCode;
				bool bIsRepeat;
			} KeyDown;

			struct   /* KEY_UP */
			{
				uint8 KeyCode;
			} KeyUp;

			struct   /** KEY_PRESSED */
			{
				TCHAR Character;
			} Character;

			struct   /** MOUSE_MOVE */
			{
				int16 DeltaX;
				int16 DeltaY;
				uint16 PosX;
				uint16 PosY;
			} MouseMove;
			
			struct   /** MOUSE_DOWN, MOUSE_UP */
			{
				uint8 Button;
				uint16 PosX;
				uint16 PosY;
			} MouseButton;
			
			struct   /** MOUSE_WHEEL */
			{
				int16 Delta;
				uint16 PosX;
				uint16 PosY;
			} MouseWheel;
			
			struct   /** TOUCH_START, TOUCH_END, TOUCH_MOVE */
			{
				uint8 TouchIndex;
				uint16 PosX;
				uint16 PosY;
				uint8 Force;
			} Touch;
		} Data;

		/**
		 * Create a completely empty event.
		 */
		FEvent()
			: Event(EventType::UNDEFINED)
		{
		}
		
		/**
		 * Create an event of the given type.
		 * @param InEvent - The type of the event.
		 */
		FEvent(EventType InEvent)
			: Event(InEvent)
		{
		}

		/**
		 * An event related to a key being pushed down.
		 * @param InKeyCode - Numerical code identifying the pushed down key.
		 * @param InIsRepeat - Whether the key is being kept down and is repeating.
		 */
		void SetKeyDown(uint8 InKeyCode, bool InIsRepeat)
		{
			check(Event == EventType::KEY_DOWN);
			Data.KeyDown.KeyCode = InKeyCode;
			Data.KeyDown.bIsRepeat = InIsRepeat;
		}

		/**
		 * An event related to a key being released.
		 * @param InKeyCode - Numerical code identifying the released key.
		 */
		void SetKeyUp(uint8 InKeyCode)
		{
			check(Event == EventType::KEY_UP);
			Data.KeyUp.KeyCode = InKeyCode;
		}

		/**
		 * An event related to character input.
		 * @param InCharacter - The character being input.
		 */
		void SetCharCode(TCHAR InCharacter)
		{
			check(Event == EventType::KEY_PRESS);
			Data.Character.Character = InCharacter;
		}

		/**
		 * An event related to mouse movement.
		 * @param InPoxX - The X position of the mouse pointer.
		 * @param InPosY - The Y position of the mouse pointer.
		 * @param InDeltaX - The change in the X position of the mouse pointer.
		 * @param InDeltaY - The change in the Y position of the mouse pointer.
		 */
		void SetMouseDelta(uint16 InPosX, uint16 InPosY, int16 InDeltaX, int16 InDeltaY)
		{
			check(Event == EventType::MOUSE_MOVE);
			Data.MouseMove.DeltaX = InDeltaX;
			Data.MouseMove.DeltaY = InDeltaY;
			Data.MouseMove.PosX = InPosX;
			Data.MouseMove.PosY = InPosY;
		}

		/**
		 * An event related to mouse buttons.
		 * @param InButton - The button number corresponding to left, middle, right, etc.
		 * @param InPoxX - The X position of the mouse pointer.
		 * @param InPosY - The Y position of the mouse pointer.
		 */
		void SetMouseClick(uint8 InButton, uint16 InPosX, uint16 InPosY)
		{
			check(Event == EventType::MOUSE_DOWN || Event == EventType::MOUSE_UP);
			Data.MouseButton.Button = InButton;
			Data.MouseButton.PosX = InPosX;
			Data.MouseButton.PosY = InPosY;
		}

		/**
		 * An event related to the mouse scroll wheel.
		 * @param InButton - The amount by which the mouse wheel was scrolled.
		 * @param InPoxX - The X position of the mouse pointer when the wheel was scrolled.
		 * @param InPosY - The Y position of the mouse pointer when the wheel was scrolled.
		 */
		void SetMouseWheel(int16 InDelta, uint16 InPosX, uint16 InPosY)
		{
			check(Event == EventType::MOUSE_WHEEL);
			Data.MouseWheel.Delta = InDelta;
			Data.MouseWheel.PosX = InPosX;
			Data.MouseWheel.PosY = InPosY;
		}

		/**
		 * An event related to a finger touching the canvas.
		 * @param InTouchIndex - The finger used in multi-touch.
		 * @param InPoxX - The X position of the finger.
		 * @param InPosY - The Y position of the finger.
		 * @param InForce - The amount of pressure being applied by the finger.
		 */
		void SetTouch(uint8 InTouchIndex, uint16 InPosX, uint16 InPosY, uint8 InForce)
		{
			check(Event == EventType::TOUCH_START || Event == EventType::TOUCH_END || Event == EventType::TOUCH_MOVE);
			Data.Touch.TouchIndex = InTouchIndex;
			Data.Touch.PosX = InPosX;
			Data.Touch.PosY = InPosY;
			Data.Touch.Force = InForce;
		}

		/**
		 * Get information about an event related to a key being pushed down.
		 * @param OutKeyCode - Numerical code identifying the pushed down key.
		 * @param OutIsRepeat - Whether the key is being kept down and is repeating.
		 */
		void GetKeyDown(uint8& OutKeyCode, bool& OutIsRepeat)
		{
			check(Event == EventType::KEY_DOWN);
			OutKeyCode = Data.KeyDown.KeyCode;
			OutIsRepeat = Data.KeyDown.bIsRepeat;
		}

		/**
		 * Get information about an event related to a key being released.
		 * @param OutKeyCode - Numerical code identifying the released key.
		 */
		void GetKeyUp(uint8& OutKeyCode)
		{
			check(Event == EventType::KEY_UP);
			OutKeyCode = Data.KeyUp.KeyCode;
		}

		/**
		 * Get information about an event related to character input.
		 * @param OutCharacter - The character being input.
		 */
		void GetCharacterCode(TCHAR& OutCharacter)
		{
			check(Event == EventType::KEY_PRESS);
			OutCharacter = Data.Character.Character;
		}

		/**
		 * Get information about an event related to mouse movement.
		 * @param OutPoxX - The X position of the mouse pointer.
		 * @param OutPosY - The Y position of the mouse pointer.
		 * @param OutDeltaX - The change in the X position of the mouse pointer.
		 * @param OutDeltaY - The change in the Y position of the mouse pointer.
		 */
		void GetMouseDelta(uint16& OutPosX, uint16& OutPosY, int16& OutDeltaX, int16& OutDeltaY)
		{
			check(Event == EventType::MOUSE_MOVE);
			OutPosX = Data.MouseMove.PosX;
			OutPosY = Data.MouseMove.PosY;
			OutDeltaX = Data.MouseMove.DeltaX;
			OutDeltaY = Data.MouseMove.DeltaY;
		}

		/**
		 * Get information about an event related to mouse buttons.
		 * @param OutButton - The button number corresponding to left, middle, right, etc.
		 * @param OutPosX - The X position of the mouse pointer.
		 * @param OutPosY - The Y position of the mouse pointer.
		 */
		void GetMouseClick(EMouseButtons::Type& OutButton, uint16& OutPosX, uint16& OutPosY)
		{
			check(Event == EventType::MOUSE_DOWN || Event == EventType::MOUSE_UP);
			// https://developer.mozilla.org/en-US/docs/Web/Events/mousedown
			uint8 Button = Data.MouseButton.Button;
			switch (Button)
			{
			case 0:
			{
				OutButton = EMouseButtons::Left;
			}
			break;
			case 1:
			{
				OutButton = EMouseButtons::Middle;
			}
			break;
			case 2:
			{
				OutButton = EMouseButtons::Right;
			}
			break;
			default:
			{
				UE_LOG(PixelStreamingInputDevice, Error, TEXT("Unknown Pixel Streaming mouse click with button %d and word 0x%016llx"), Button, Data.Word);
			}
			break;
			}
			OutPosX = Data.MouseButton.PosX;
			OutPosY = Data.MouseButton.PosY;
		}

		/**
		 * Get information about an event related to the mouse wheel.
		 * @param OutDelta - The amount by which the mouse wheel was scrolled.
		 * @param PosX - The X position of the mouse pointer when the wheel was scrolled.
		 * @param PosY - The Y position of the mouse pointer when the wheel was scrolled.
		 */
		void GetMouseWheel(int16& OutDelta, uint16& OutPosX, uint16& OutPosY)
		{
			check(Event == EventType::MOUSE_WHEEL);
			OutDelta = Data.MouseWheel.Delta;
			OutPosX = Data.MouseWheel.PosX;
			OutPosY = Data.MouseWheel.PosY;
		}

		/**
		 * Get information about an event related to a finger touching the canvas.
		 * @param OutTouchIndex - The finger used in multi-touch.
		 * @param OutPoxX - The X position of the finger.
		 * @param OutPosY - The Y position of the finger.
		 * @param OutForce - Amount of pressure being applied by the finger.
		 */
		void GetTouch(uint8& OutTouchIndex, uint16& OutPosX, uint16& OutPosY, uint8& OutForce)
		{
			check(Event == EventType::TOUCH_START || Event == EventType::TOUCH_END || Event == EventType::TOUCH_MOVE);
			OutTouchIndex = Data.Touch.TouchIndex;
			OutPosX = Data.Touch.PosX;
			OutPosY = Data.Touch.PosY;
			OutForce = Data.Touch.Force;
		}
	};

	/**
	 * Add a new event to the input device for later processing.
	 * @param InEvent - The new event.
	 */
	void ProcessEvent(const FEvent& InEvent);

	/**
	 * Add a new UI interaction descriptor to the input device for later processing.
	 * @param InDescriptor - The new UI interaction descriptor.
	 */
	void ProcessUIInteraction(const FString& InDescriptor);

	/**
	 * Add a new command descriptor to the input device for later execution.
	 * @param InDescriptor - The new command descriptor.
	 */
	void ProcessCommand(const FString& InDescriptor);

	/**
	 * Is the application faking touch events?
	 * @return True if the application is faking touch events.
	*/
	bool IsFakingTouchEvents() const
	{
		return bFakingTouchEvents;
	}

private:

	/**
	 * A special wrapper over the GenericApplication layer which allows us to
	 * override certain behavior.
	 */
	TSharedPtr<class FPixelStreamingApplicationWrapper> PixelStreamingApplicationWrapper;

	/** Reference to the message handler which events should be passed to. */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	/** The queue of events which are awaiting processing. */
	TQueue<FEvent> Events;

	/**
	 * Pixel streaming input components contain a delegate which will broadcast
	 * UI integrations to interested parties.
	 */
	TArray<UPixelStreamingInputComponent*> InputComponents;

	/**
	 * A queue of UI interaction descriptor strings which contain arbitrary
	 * information related to the interaction.
	 */
	TQueue<FString> UIInteractions;

	/**
	 * It is only possible to enable commands when explicitly permitted as these
	 * have security implications.
	 */
	bool bAllowCommands;
	
	/**
	 * A queue of command descriptor strings which contain the command to
	 * execute and its arguments.
	 */
	TQueue<FString> Commands;

	/**
	 * Is the application faking touch events by dragging the mouse along
	 * the canvas? If so then we must put the browser canvas in a special
	 * state to replicate the behavior of the application.
	 */
	bool bFakingTouchEvents;
};
