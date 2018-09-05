// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingInputDevice.h"
#include "PixelStreamingInputComponent.h"
#include "PixelStreamingSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SWindow.h"
#include "Misc/ScopeLock.h"
#include "JavaScriptKeyCodes.inl"

DEFINE_LOG_CATEGORY(PixelStreamingInputDevice);

/**
 * When reading input from a browser then the cursor position will be sent
 * across with mouse events. We want to use this position and avoid getting the
 * cursor position from the operating system. This is not relevant to touch
 * events.
 */
class FPixelStreamingCursor : public ICursor
{
public:

	FPixelStreamingCursor() {}
	virtual ~FPixelStreamingCursor() = default;
	virtual FVector2D GetPosition() const override { return Position; }
	virtual void SetPosition(const int32 X, const int32 Y) override { Position = FVector2D(X, Y); };
	virtual void SetType(const EMouseCursor::Type InNewCursor) override {};
	virtual EMouseCursor::Type GetType() const override { return EMouseCursor::Type::Default; };
	virtual void GetSize(int32& Width, int32& Height) const override {};
	virtual void Show(bool bShow) override {};
	virtual void Lock(const RECT* const Bounds) override {};
	virtual void SetTypeShape(EMouseCursor::Type InCursorType, void* CursorHandle) override {};

private:

	/** The cursor position sent across with mouse events. */
	FVector2D Position;
};

/**
* Wrap the GenericApplication layer so we can replace the cursor and override
* certain behavior.
*/
class FPixelStreamingApplicationWrapper : public GenericApplication
{
public:

	FPixelStreamingApplicationWrapper(TSharedPtr<GenericApplication> InWrappedApplication)
		: GenericApplication(MakeShareable(new FPixelStreamingCursor()))
		, WrappedApplication(InWrappedApplication)
	{
	}

	/**
	 * Functions passed directly to the wrapped application.
	 */

	virtual void SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) { WrappedApplication->SetMessageHandler(InMessageHandler); }
	virtual void PollGameDeviceState(const float TimeDelta) { WrappedApplication->PollGameDeviceState(TimeDelta); }
	virtual void PumpMessages(const float TimeDelta) { WrappedApplication->PumpMessages(TimeDelta); }
	virtual void ProcessDeferredEvents(const float TimeDelta) { WrappedApplication->ProcessDeferredEvents(TimeDelta); }
	virtual void Tick(const float TimeDelta) { WrappedApplication->Tick(TimeDelta); }
	virtual TSharedRef< FGenericWindow > MakeWindow() { return WrappedApplication->MakeWindow(); }
	virtual void InitializeWindow(const TSharedRef< FGenericWindow >& Window, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately) { WrappedApplication->InitializeWindow(Window, InDefinition, InParent, bShowImmediately); }
	virtual void SetCapture(const TSharedPtr< FGenericWindow >& InWindow) { WrappedApplication->SetCapture(InWindow); }
	virtual void* GetCapture(void) const { return WrappedApplication->GetCapture(); }
	virtual FModifierKeysState GetModifierKeys() const { return WrappedApplication->GetModifierKeys(); }
	virtual TSharedPtr< FGenericWindow > GetWindowUnderCursor() { return WrappedApplication->GetWindowUnderCursor(); }
	virtual void SetHighPrecisionMouseMode(const bool Enable, const TSharedPtr< FGenericWindow >& InWindow) { WrappedApplication->SetHighPrecisionMouseMode(Enable, InWindow); };
	virtual bool IsUsingHighPrecisionMouseMode() const { return WrappedApplication->IsUsingHighPrecisionMouseMode(); }
	virtual bool IsUsingTrackpad() const { return WrappedApplication->IsUsingTrackpad(); }
	virtual bool IsMouseAttached() const { return WrappedApplication->IsMouseAttached(); }
	virtual bool IsGamepadAttached() const { return WrappedApplication->IsGamepadAttached(); }
	virtual void RegisterConsoleCommandListener(const FOnConsoleCommandListener& InListener) { WrappedApplication->RegisterConsoleCommandListener(InListener); }
	virtual void AddPendingConsoleCommand(const FString& InCommand) { WrappedApplication->AddPendingConsoleCommand(InCommand); }
	virtual FPlatformRect GetWorkArea(const FPlatformRect& CurrentWindow) const { return WrappedApplication->GetWorkArea(CurrentWindow); }
	virtual bool TryCalculatePopupWindowPosition(const FPlatformRect& InAnchor, const FVector2D& InSize, const FVector2D& ProposedPlacement, const EPopUpOrientation::Type Orientation, /*OUT*/ FVector2D* const CalculatedPopUpPosition) const { return WrappedApplication->TryCalculatePopupWindowPosition(InAnchor, InSize, ProposedPlacement, Orientation, CalculatedPopUpPosition); }
	virtual void GetInitialDisplayMetrics(FDisplayMetrics& OutDisplayMetrics) const { WrappedApplication->GetInitialDisplayMetrics(OutDisplayMetrics); }
	virtual EWindowTitleAlignment::Type GetWindowTitleAlignment() const { return WrappedApplication->GetWindowTitleAlignment(); }
	virtual EWindowTransparency GetWindowTransparencySupport() const { return WrappedApplication->GetWindowTransparencySupport(); }
	virtual void DestroyApplication() { WrappedApplication->DestroyApplication(); }
	virtual IInputInterface* GetInputInterface() { return WrappedApplication->GetInputInterface(); }
	virtual ITextInputMethodSystem* GetTextInputMethodSystem() { return WrappedApplication->GetTextInputMethodSystem(); }
	virtual void SendAnalytics(IAnalyticsProvider* Provider) { WrappedApplication->SendAnalytics(Provider); }
	virtual bool SupportsSystemHelp() const { return WrappedApplication->SupportsSystemHelp(); }
	virtual void ShowSystemHelp() { WrappedApplication->ShowSystemHelp(); }
	virtual bool ApplicationLicenseValid(FPlatformUserId PlatformUser = PLATFORMUSERID_NONE) { return WrappedApplication->ApplicationLicenseValid(PlatformUser); }

	/**
	 * Functions with overridden behavior.
	 */
	virtual bool IsCursorDirectlyOverSlateWindow() const { return true; }

	TSharedPtr<GenericApplication> WrappedApplication;
};

FPixelStreamingInputDevice::FPixelStreamingInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler, TArray<UPixelStreamingInputComponent*>& InInputComponents)
	: PixelStreamingApplicationWrapper(MakeShareable(new FPixelStreamingApplicationWrapper(FSlateApplication::Get().GetPlatformApplication())))
	, MessageHandler(InMessageHandler)
	, InputComponents(InInputComponents)
	, bAllowCommands(FParse::Param(FCommandLine::Get(), TEXT("AllowPixelStreamingCommands")))
	, bFakingTouchEvents(FSlateApplication::Get().IsFakingTouchEvents())
{
	if (GEngine->GameViewport && !GEngine->GameViewport->HasSoftwareCursor(EMouseCursor::Default))
	{
		// Pixel streaming always requires a default software cursor as it needs
		// to be shown on the browser to allow the user to click UI elements.
		const UPixelStreamingSettings* Settings = GetDefault<UPixelStreamingSettings>();
		check(Settings);
		
		GEngine->GameViewport->AddSoftwareCursor(EMouseCursor::Default, Settings->PixelStreamingDefaultCursorClassName);
	}
}

void FPixelStreamingInputDevice::Tick(float DeltaTime)
{
	FEvent Event;
	while (Events.Dequeue(Event))
	{
		switch (Event.Event)
		{
		case EventType::UNDEFINED:
		{
			checkNoEntry();
		}
		break;
		case EventType::KEY_DOWN:
		{
			uint8 JavaScriptKeyCode;
			bool IsRepeat;
			Event.GetKeyDown(JavaScriptKeyCode, IsRepeat);
			const FKey* AgnosticKey = AgnosticKeys[JavaScriptKeyCode];
			const uint32* KeyCode;
			const uint32* CharacterCode;
			FInputKeyManager::Get().GetCodesFromKey(*AgnosticKey, KeyCode, CharacterCode);
			MessageHandler->OnKeyDown(KeyCode ? *KeyCode : 0, CharacterCode ? *CharacterCode : 0, IsRepeat);
			UE_LOG(PixelStreamingInputDevice, Verbose, TEXT("KEY_DOWN: KeyCode = %d; CharacterCode = %d; IsRepeat = %s"), KeyCode, CharacterCode, IsRepeat ? TEXT("True") : TEXT("False"));
		}
		break;
		case EventType::KEY_UP:
		{
			uint8 JavaScriptKeyCode;
			Event.GetKeyUp(JavaScriptKeyCode);
			const FKey* AgnosticKey = AgnosticKeys[JavaScriptKeyCode];
			const uint32* KeyCode;
			const uint32* CharacterCode;
			FInputKeyManager::Get().GetCodesFromKey(*AgnosticKey, KeyCode, CharacterCode);
			MessageHandler->OnKeyUp(KeyCode ? *KeyCode : 0, CharacterCode ? *CharacterCode : 0, false);   // Key up events are never repeats.
			UE_LOG(PixelStreamingInputDevice, Verbose, TEXT("KEY_UP: KeyCode = %d; CharacterCode = %d"), KeyCode, CharacterCode);
		}
		break;
		case EventType::KEY_PRESS:
		{
			TCHAR UnicodeCharacter;
			Event.GetCharacterCode(UnicodeCharacter);
			MessageHandler->OnKeyChar(UnicodeCharacter, false);   // Key press repeat not yet available but are not intrinsically used.
			UE_LOG(PixelStreamingInputDevice, Verbose, TEXT("KEY_PRESSED: Character = '%c'"), UnicodeCharacter);
		}
		break;
		case EventType::MOUSE_ENTER:
		{
			// Override application layer to special pixel streaming version.
			FSlateApplication::Get().OverridePlatformApplication(PixelStreamingApplicationWrapper);
			FSlateApplication::Get().OnCursorSet();

			// Make sure the viewport is active.
			FSlateApplication::Get().ProcessApplicationActivationEvent(true);

			// Double the number of hit test cells to cater for the possibility
			// that the window will be off screen.
			UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
			TSharedPtr<SWindow> Window = GameEngine->SceneViewport->FindWindow();
			Window->GetHittestGrid()->SetNumCellsExcess(Window->GetHittestGrid()->GetNumCells());

			UE_LOG(PixelStreamingInputDevice, Verbose, TEXT("MOUSE_ENTER"));
		}
		break;
		case EventType::MOUSE_LEAVE:
		{
			// Restore normal application layer.
			FSlateApplication::Get().OverridePlatformApplication(PixelStreamingApplicationWrapper->WrappedApplication);

			// Reduce the number of hit test cells back to normal.
			UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
			TSharedPtr<SWindow> Window = GameEngine->SceneViewport->FindWindow();
			Window->GetHittestGrid()->SetNumCellsExcess(FIntPoint(0, 0));

			UE_LOG(PixelStreamingInputDevice, Verbose, TEXT("MOUSE_LEAVE"));
		}
		break;
		case EventType::MOUSE_MOVE:
		{
			uint16 PosX;
			uint16 PosY;
			int16 DeltaX;
			int16 DeltaY;
			Event.GetMouseDelta(PosX, PosY, DeltaX, DeltaY);
			FVector2D CursorPos = GEngine->GameViewport->GetWindow()->GetPositionInScreen() + FVector2D(PosX, PosY);
			PixelStreamingApplicationWrapper->Cursor->SetPosition(CursorPos.X, CursorPos.Y);
			MessageHandler->OnRawMouseMove(DeltaX, DeltaY);
			UE_LOG(PixelStreamingInputDevice, VeryVerbose, TEXT("MOUSE_MOVE: Pos = (%d, %d); CursorPos = (%d, %d); Delta = (%d, %d)"), PosX, PosY, static_cast<int>(CursorPos.X), static_cast<int>(CursorPos.Y), DeltaX, DeltaY);
		}
		break;
		case EventType::MOUSE_DOWN:
		{
			// If a user clicks on the application window and then clicks on the
			// browser then this will move the focus away from the application
			// window which will deactivate the application, so we need to check
			// if we must reactivate the application.
			if (!FSlateApplication::Get().IsActive())
			{
				FSlateApplication::Get().ProcessApplicationActivationEvent(true);
			}

			EMouseButtons::Type Button;
			uint16 PosX;
			uint16 PosY;
			Event.GetMouseClick(Button, PosX, PosY);
			FVector2D CursorPos = GEngine->GameViewport->GetWindow()->GetPositionInScreen() + FVector2D(PosX, PosY);
			PixelStreamingApplicationWrapper->Cursor->SetPosition(CursorPos.X, CursorPos.Y);
			MessageHandler->OnMouseDown(GEngine->GameViewport->GetWindow()->GetNativeWindow(), Button, CursorPos);
			UE_LOG(PixelStreamingInputDevice, Verbose, TEXT("MOUSE_DOWN: Button = %d; Pos = (%d, %d); CursorPos = (%d, %d)"), Button, PosX, PosY, static_cast<int>(CursorPos.X), static_cast<int>(CursorPos.Y));
		}
		break;
		case EventType::MOUSE_UP:
		{
			EMouseButtons::Type Button;
			uint16 PosX;
			uint16 PosY;
			Event.GetMouseClick(Button, PosX, PosY);
			FVector2D CursorPos = GEngine->GameViewport->GetWindow()->GetPositionInScreen() + FVector2D(PosX, PosY);
			PixelStreamingApplicationWrapper->Cursor->SetPosition(CursorPos.X, CursorPos.Y);
			MessageHandler->OnMouseUp(Button);
			UE_LOG(PixelStreamingInputDevice, Verbose, TEXT("MOUSE_UP: Button = %d; Pos = (%d, %d); CursorPos = (%d, %d)"), Button, PosX, PosY, static_cast<int>(CursorPos.X), static_cast<int>(CursorPos.Y));
		}
		break;
		case EventType::MOUSE_WHEEL:
		{
			int16 Delta;
			uint16 PosX;
			uint16 PosY;
			Event.GetMouseWheel(Delta, PosX, PosY);
			const float SpinFactor = 1 / 120.0f;
			FVector2D CursorPos = GEngine->GameViewport->GetWindow()->GetPositionInScreen() + FVector2D(PosX, PosY);
			MessageHandler->OnMouseWheel(Delta * SpinFactor, CursorPos);
			UE_LOG(PixelStreamingInputDevice, Verbose, TEXT("MOUSE_WHEEL: Delta = %d; Pos = (%d, %d); CursorPos = (%d, %d)"), Delta, PosX, PosY, static_cast<int>(CursorPos.X), static_cast<int>(CursorPos.Y));
		}
		break;
		case EventType::TOUCH_START:
		{
			uint8 TouchIndex;
			uint16 PosX;
			uint16 PosY;
			uint8 Force;   // Force is between 0.0 and 1.0 so will need to unquantize from byte.
			Event.GetTouch(TouchIndex, PosX, PosY, Force);
			FVector2D CursorPos = GEngine->GameViewport->GetWindow()->GetPositionInScreen() + FVector2D(PosX, PosY);
			MessageHandler->OnTouchStarted(GEngine->GameViewport->GetWindow()->GetNativeWindow(), CursorPos, Force / 255.0f, TouchIndex, 0);   // TODO: ControllerId?
			UE_LOG(PixelStreamingInputDevice, Verbose, TEXT("TOUCH_START: TouchIndex = %d; Pos = (%d, %d); CursorPos = (%d, %d); Force = %.3f"), TouchIndex, PosX, PosY, static_cast<int>(CursorPos.X), static_cast<int>(CursorPos.Y), Force / 255.0f);
		}
		break;
		case EventType::TOUCH_END:
		{
			uint8 TouchIndex;
			uint16 PosX;
			uint16 PosY;
			uint8 Force;
			Event.GetTouch(TouchIndex, PosX, PosY, Force);
			FVector2D CursorPos = GEngine->GameViewport->GetWindow()->GetPositionInScreen() + FVector2D(PosX, PosY);
			MessageHandler->OnTouchEnded(CursorPos, TouchIndex, 0);   // TODO: ControllerId?
			UE_LOG(PixelStreamingInputDevice, Verbose, TEXT("TOUCH_END: TouchIndex = %d; Pos = (%d, %d); CursorPos = (%d, %d)"), TouchIndex, PosX, PosY, static_cast<int>(CursorPos.X), static_cast<int>(CursorPos.Y));
		}
		break;
		case EventType::TOUCH_MOVE:
		{
			uint8 TouchIndex;
			uint16 PosX;
			uint16 PosY;
			uint8 Force;   // Force is between 0.0 and 1.0 so will need to unquantize from byte.
			Event.GetTouch(TouchIndex, PosX, PosY, Force);
			FVector2D CursorPos = GEngine->GameViewport->GetWindow()->GetPositionInScreen() + FVector2D(PosX, PosY);
			MessageHandler->OnTouchMoved(CursorPos, Force / 255.0f, TouchIndex, 0);   // TODO: ControllerId?
			UE_LOG(PixelStreamingInputDevice, VeryVerbose, TEXT("TOUCH_MOVE: TouchIndex = %d; Pos = (%d, %d); CursorPos = (%d, %d); Force = %.3f"), TouchIndex, PosX, PosY, static_cast<int>(CursorPos.X), static_cast<int>(CursorPos.Y), Force / 255.0f);
		}
		break;
		default:
		{
			UE_LOG(PixelStreamingInputDevice, Error, TEXT("Unknown Pixel Streaming event %d with word 0x%016llx"), static_cast<int>(Event.Event), Event.Data.Word);
		}
		break;
		}
	}

	FString UIInteraction;
	while (UIInteractions.Dequeue(UIInteraction))
	{
		for (UPixelStreamingInputComponent* InputComponent : InputComponents)
		{
			InputComponent->OnPixelStreamingInputEvent.Broadcast(UIInteraction);
			UE_LOG(PixelStreamingInputDevice, Verbose, TEXT("UIInteraction = %s"), *UIInteraction);
		}
	}

	FString Command;
	while (Commands.Dequeue(Command))
	{
		for (UPixelStreamingInputComponent* InputComponent : InputComponents)
		{
			if (InputComponent->OnCommand(Command))
			{
				UE_LOG(PixelStreamingInputDevice, Verbose, TEXT("Command = %s"), *Command);
			}
			else
			{
				UE_LOG(PixelStreamingInputDevice, Warning, TEXT("Failed to run Command = %s"), *Command);
			}
		}
	}
}

void FPixelStreamingInputDevice::SendControllerEvents()
{
}

void FPixelStreamingInputDevice::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

bool FPixelStreamingInputDevice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return true;
}

void FPixelStreamingInputDevice::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
}

void FPixelStreamingInputDevice::SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values)
{
}

void FPixelStreamingInputDevice::ProcessEvent(const FEvent& InEvent)
{
	bool Success = Events.Enqueue(InEvent);
	checkf(Success, TEXT("Unable to enqueue new event of type %d"), static_cast<int>(InEvent.Event));
}

void FPixelStreamingInputDevice::ProcessUIInteraction(const FString& InDescriptor)
{
	bool Success = UIInteractions.Enqueue(InDescriptor);
	checkf(Success, TEXT("Unable to enqueue new UI Interaction %s"), *InDescriptor);
}

void FPixelStreamingInputDevice::ProcessCommand(const FString& InDescriptor)
{
	if (bAllowCommands)
	{
		bool Success = Commands.Enqueue(InDescriptor);
		checkf(Success, TEXT("Unable to enqueue new Command %s"), *InDescriptor);
	}
}
