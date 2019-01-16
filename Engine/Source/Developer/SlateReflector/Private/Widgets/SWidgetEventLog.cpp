// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SWidgetEventLog.h"

#if WITH_SLATE_DEBUGGING

#include "Debugging/SlateDebugging.h"
#include "Widgets/SBoxPanel.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "Logging/MessageLog.h"
#include "Types/ReflectionMetadata.h"

#define LOCTEXT_NAMESPACE "WidgetEventLog"

static FName NAME_WidgetEvents(TEXT("WidgetEvents"));

void SWidgetEventLog::Construct(const FArguments& InArgs)
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
	TSharedRef<IMessageLogListing> MessageLogListing = MessageLogModule.GetLogListing(NAME_WidgetEvents);

	ChildSlot
	[
		MessageLogModule.CreateLogListingWidget(MessageLogListing)
	];

	UpdateListeners();
}

SWidgetEventLog::~SWidgetEventLog()
{
	RemoveListeners();
}

void SWidgetEventLog::RemoveListeners()
{
	FSlateDebugging::InputEvent.RemoveAll(this);
	FSlateDebugging::FocusEvent.RemoveAll(this);
}

void SWidgetEventLog::UpdateListeners()
{
	RemoveListeners();

	FSlateDebugging::InputEvent.AddSP(this, &SWidgetEventLog::OnInputEvent);
	FSlateDebugging::FocusEvent.AddSP(this, &SWidgetEventLog::OnFocusEvent);
}

void SWidgetEventLog::OnInputEvent(const FSlateDebuggingInputEventArgs& EventArgs)
{

}

void SWidgetEventLog::OnFocusEvent(const FSlateDebuggingFocusEventArgs& EventArgs)
{
	static const FText FocusEventFormat = LOCTEXT("FocusEventFormat", "{0}({1}:{2}) - {3} -> {4}");
	static const FText NoneText = LOCTEXT("None", "None");

	FText FocusEventText;
	switch (EventArgs.FocusEventType)
	{
	case ESlateDebuggingFocusEvent::FocusChanging:
		FocusEventText = LOCTEXT("FocusChanging", "Focus Changing");
		break;
	case ESlateDebuggingFocusEvent::FocusLost:
		return;
	case ESlateDebuggingFocusEvent::FocusReceived:
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

	FMessageLog MessageLog(NAME_WidgetEvents);
	MessageLog.SuppressLoggingToOutputLog();

	FText EventText = FText::Format(
		FocusEventFormat,
		FocusEventText,
		UserIndex,
		CauseText,
		OldFocusedWidgetText,
		NewFocusedWidgetText
	);

	MessageLog.Info(EventText);
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_SLATE_DEBUGGING
