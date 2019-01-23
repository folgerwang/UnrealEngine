// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConsoleSlateDebugger.h"

#if WITH_SLATE_DEBUGGING

#include "Debugging/SlateDebugging.h"
#include "Widgets/SBoxPanel.h"
#include "Modules/ModuleManager.h"
#include "Logging/MessageLog.h"
#include "Types/ReflectionMetadata.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "HAL/PlatformStackWalk.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Layout/WidgetPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlateDebugger, Log, All);

#define LOCTEXT_NAMESPACE "ConsoleSlateDebugger"

static FConsoleSlateDebugger SlateConsoleDebugger;

FConsoleSlateDebugger::FConsoleSlateDebugger()
	: bCaptureStack(0)
	, EnabledInputEvents(false, (uint8)ESlateDebuggingInputEvent::COUNT)
	, StartDebuggingCommand(
		TEXT("SlateDebugger.Start"),
		*LOCTEXT("StartDebugger", "Starts the debugger.").ToString(),
		FConsoleCommandDelegate::CreateRaw(this, &FConsoleSlateDebugger::StartDebugging))
	, StopDebuggingCommand(
		TEXT("SlateDebugger.Stop"),
		*LOCTEXT("StopDebugger", "Stops the debugger.").ToString(),
		FConsoleCommandDelegate::CreateRaw(this, &FConsoleSlateDebugger::StartDebugging))
	, CaptureStackVariable(
		TEXT("SlateDebugger.CaptureStack"),
		bCaptureStack,
		*LOCTEXT("CaptureStack", "Should we capture the stack when there are events?").ToString())
	, SetInputFilterCommand(
		TEXT("SlateDebugger.SetInputFilter"),
		*LOCTEXT("SetInputFilter", "Enable or Disable specific filters").ToString(),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FConsoleSlateDebugger::SetInputFilter))
{
	//EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseMove] = true;
	//EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseEnter] = true;
	//EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseLeave] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseButtonDown] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseButtonUp] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseButtonDoubleClick] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::MouseWheel] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::TouchStart] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::TouchEnd] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::DragDetected] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::DragEnter] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::DragLeave] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::DragOver] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::DragDrop] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::DropMessage] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::KeyDown] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::KeyUp] = true;
	//EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::KeyChar] = true;
	//EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::AnalogInput] = true;
	EnabledInputEvents[(uint8)ESlateDebuggingInputEvent::TouchGesture] = true;


	// TODO Add commands for setting and unsetting input event filters.
}

FConsoleSlateDebugger::~FConsoleSlateDebugger()
{
	RemoveListeners();
}

void FConsoleSlateDebugger::StartDebugging()
{
	UE_LOG(LogSlateDebugger, Log, TEXT("Start Slate Debugger"));

	UpdateListeners();
}

void FConsoleSlateDebugger::StopDebugging()
{
	UE_LOG(LogSlateDebugger, Log, TEXT("Stop Slate Debugger"));

	RemoveListeners();
}

void FConsoleSlateDebugger::SetInputFilter(const TArray< FString >& Params)
{
	if (Params.Num() != 2)
	{
		return;
	}

	static const UEnum* SlateDebuggingInputEventEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ESlateDebuggingInputEvent"));
	
	const int64 InputEventEnumValue = SlateDebuggingInputEventEnum->GetValueByNameString(Params[0]);
	if (InputEventEnumValue == INDEX_NONE)
	{
		return;
	}

	bool bEnable = false;
	if (!LexTryParseString(bEnable, *Params[1]))
	{
		return;
	}
	
	EnabledInputEvents[InputEventEnumValue] = bEnable;
}

void FConsoleSlateDebugger::RemoveListeners()
{
#if WITH_SLATE_DEBUGGING
	FSlateDebugging::Warning.RemoveAll(this);
	FSlateDebugging::InputEvent.RemoveAll(this);
	FSlateDebugging::FocusEvent.RemoveAll(this);
	FSlateDebugging::NavigationEvent.RemoveAll(this);
	FSlateDebugging::MouseCaptureEvent.RemoveAll(this);
#endif
}

void FConsoleSlateDebugger::UpdateListeners()
{
	RemoveListeners();

#if WITH_SLATE_DEBUGGING
	FSlateDebugging::Warning.AddRaw(this, &FConsoleSlateDebugger::OnWarning);
	FSlateDebugging::InputEvent.AddRaw(this, &FConsoleSlateDebugger::OnInputEvent);
	FSlateDebugging::FocusEvent.AddRaw(this, &FConsoleSlateDebugger::OnFocusEvent);
	FSlateDebugging::NavigationEvent.AddRaw(this, &FConsoleSlateDebugger::OnNavigationEvent);
	FSlateDebugging::MouseCaptureEvent.AddRaw(this, &FConsoleSlateDebugger::OnStateChangeEvent);
#endif
}

void FConsoleSlateDebugger::OnWarning(const FSlateDebuggingWarningEventArgs& EventArgs)
{
	static const FText InputEventFormat = LOCTEXT("WarningEventFormat", "{0} (Widget: {1})");

	const FText ContextWidget = FText::FromString(FReflectionMetaData::GetWidgetDebugInfo(EventArgs.OptionalContextWidget.Get()));

	FText EventText = FText::Format(
		InputEventFormat,
		EventArgs.Warning,
		ContextWidget
	);

	UE_LOG(LogSlateDebugger, Warning, TEXT("%s"), *EventText.ToString());

	OptionallyDumpCallStack();
}

void FConsoleSlateDebugger::OnInputEvent(const FSlateDebuggingInputEventArgs& EventArgs)
{
	// If the input event isn't in the set we care about don't write it out.
	if (!EnabledInputEvents[(uint8)EventArgs.InputEventType])
	{
		return;
	}

	static const FText InputEventFormat = LOCTEXT("InputEventFormat", "{0} - ({1}) - [{2}]");

	static const UEnum* SlateDebuggingInputEventEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ESlateDebuggingInputEvent"));
	const FText InputEventTypeText = SlateDebuggingInputEventEnum->GetDisplayNameTextByValue((int64)EventArgs.InputEventType);
	const FText AdditionalContent = FText::FromString(EventArgs.AdditionalContent);
	const FText HandlerWidget = FText::FromString(FReflectionMetaData::GetWidgetDebugInfo(EventArgs.HandlerWidget.Get()));

	FText EventText = FText::Format(
		InputEventFormat,
		InputEventTypeText,
		HandlerWidget,
		AdditionalContent
	);

	UE_LOG(LogSlateDebugger, Log, TEXT("%s"), *EventText.ToString());

	OptionallyDumpCallStack();
}

void FConsoleSlateDebugger::OnFocusEvent(const FSlateDebuggingFocusEventArgs& EventArgs)
{
	static const FText FocusEventFormat = LOCTEXT("FocusEventFormat", "{0}({1}:{2}) - {3} -> {4}");

	FText FocusEventText;
	switch (EventArgs.FocusEventType)
	{
	case ESlateDebuggingFocusEvent::FocusChanging:
		FocusEventText = LOCTEXT("FocusChanging", "Focus Changing");
		break;
	case ESlateDebuggingFocusEvent::FocusLost:
		// Ignore the Lost
		return;
	case ESlateDebuggingFocusEvent::FocusReceived:
		// Ignore the Received
		return;
	}

	FText CauseText;
	switch (EventArgs.FocusEvent.GetCause())
	{
	case EFocusCause::Mouse:
		CauseText = LOCTEXT("FocusCause_Mouse", "Mouse");
		break;
	case EFocusCause::Navigation:
		CauseText = LOCTEXT("FocusCause_Navigation", "Navigation");
		break;
	case EFocusCause::SetDirectly:
		CauseText = LOCTEXT("FocusCause_SetDirectly", "SetDirectly");
		break;
	case EFocusCause::Cleared:
		CauseText = LOCTEXT("FocusCause_Cleared", "Cleared");
		break;
	case EFocusCause::OtherWidgetLostFocus:
		CauseText = LOCTEXT("FocusCause_OtherWidgetLostFocus", "OtherWidgetLostFocus");
		break;
	case EFocusCause::WindowActivate:
		CauseText = LOCTEXT("FocusCause_WindowActivate", "WindowActivate");
		break;
	}

	const int32 UserIndex = EventArgs.FocusEvent.GetUser();

	const FText OldFocusedWidgetText = FText::FromString(FReflectionMetaData::GetWidgetDebugInfo(EventArgs.OldFocusedWidget.Get()));
	const FText NewFocusedWidgetText = FText::FromString(FReflectionMetaData::GetWidgetDebugInfo(EventArgs.NewFocusedWidget.Get()));

	FText EventText = FText::Format(
		FocusEventFormat,
		FocusEventText,
		UserIndex,
		CauseText,
		OldFocusedWidgetText,
		NewFocusedWidgetText
	);

	UE_LOG(LogSlateDebugger, Log, TEXT("%s"), *EventText.ToString());

	OptionallyDumpCallStack();
}

void FConsoleSlateDebugger::OnNavigationEvent(const FSlateDebuggingNavigationEventArgs& EventArgs)
{
	static const UEnum* UINavigationEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EUINavigation"));
	static const UEnum* NavigationGenesisEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ENavigationGenesis"));

	static const FText NavEventFormat = LOCTEXT("NavEventFormat", "Nav: {0}:{1} | {2} -> {3}");

	const FText SourceWidget = FText::FromString(FReflectionMetaData::GetWidgetDebugInfo(&EventArgs.NavigationSource.GetLastWidget().Get()));
	const FText DestinationWidget = FText::FromString(FReflectionMetaData::GetWidgetDebugInfo(EventArgs.DestinationWidget.Get()));
	const FText NavigationTypeText = UINavigationEnum->GetDisplayNameTextByValue((int64)EventArgs.NavigationEvent.GetNavigationType());
	const FText NavigationGenesisText = UINavigationEnum->GetDisplayNameTextByValue((int64)EventArgs.NavigationEvent.GetNavigationGenesis());

	FText EventText = FText::Format(
		NavEventFormat,
		NavigationTypeText,
		NavigationGenesisText,
		SourceWidget,
		DestinationWidget
	);

	UE_LOG(LogSlateDebugger, Log, TEXT("%s"), *EventText.ToString());

	OptionallyDumpCallStack();
}

void FConsoleSlateDebugger::OnStateChangeEvent(const FSlateDebuggingMouseCaptureEventArgs& EventArgs)
{
	static const FText StateChangeEventFormat = LOCTEXT("StateChangeEventFormat", "{0} : {1}");

	const FText StateText = LOCTEXT("MouseCaptured", "Mouse Captured");
	const FText SourceWidget = FText::FromString(FReflectionMetaData::GetWidgetDebugInfo(EventArgs.CapturingWidget.Get()));

	FText EventText = FText::Format(
		StateChangeEventFormat,
		StateText,
		SourceWidget
	);

	UE_LOG(LogSlateDebugger, Log, TEXT("%s"), *EventText.ToString());

	OptionallyDumpCallStack();
}

void FConsoleSlateDebugger::OptionallyDumpCallStack()
{
	if (!bCaptureStack)
	{
		return;
	}

	PrintScriptCallstack();

	TArray<FProgramCounterSymbolInfo> Stack = FPlatformStackWalk::GetStack(7, 5);

	for (int i = 0; i < Stack.Num(); i++)
	{
		UE_LOG(LogSlateDebugger, Log, TEXT("%s"), ANSI_TO_TCHAR(Stack[i].FunctionName));

		//ANSICHAR HumanReadableString[1024];
		//if (FPlatformStackWalk::SymbolInfoToHumanReadableString(Stack[i], HumanReadableString, 1024))
		//{
		//	UE_LOG(LogSlateDebugger, Log, TEXT("%s"), ANSI_TO_TCHAR((const ANSICHAR*)&HumanReadableString[0]));
		//}
	}
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_SLATE_DEBUGGING
