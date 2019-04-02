// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Debugging/SlateDebugging.h"
#include "SlateGlobals.h"

#define LOCTEXT_NAMESPACE "SlateDebugger"

FSlateDebuggingInputEventArgs::FSlateDebuggingInputEventArgs(ESlateDebuggingInputEvent InInputEventType, const FReply& InReply, const TSharedPtr<SWidget>& InHandlerWidget, const FString& InAdditionalContent)
	: InputEventType(InInputEventType)
	, Reply(InReply)
	, HandlerWidget(InHandlerWidget)
	, AdditionalContent(InAdditionalContent)
{
}

FSlateDebuggingFocusEventArgs::FSlateDebuggingFocusEventArgs(
	ESlateDebuggingFocusEvent InFocusEventType,
	const FFocusEvent& InFocusEvent,
	const FWeakWidgetPath& InOldFocusedWidgetPath,
	const TSharedPtr<SWidget>& InOldFocusedWidget,
	const FWidgetPath& InNewFocusedWidgetPath,
	const TSharedPtr<SWidget>& InNewFocusedWidget)
	: FocusEventType(InFocusEventType)
	, FocusEvent(InFocusEvent)
	, OldFocusedWidgetPath(InOldFocusedWidgetPath)
	, OldFocusedWidget(InOldFocusedWidget)
	, NewFocusedWidgetPath(InNewFocusedWidgetPath)
	, NewFocusedWidget(InNewFocusedWidget)
{
}

FSlateDebuggingNavigationEventArgs::FSlateDebuggingNavigationEventArgs(
	const FNavigationEvent& InNavigationEvent,
	const FNavigationReply& InNavigationReply,
	const FWidgetPath& InNavigationSource,
	const TSharedPtr<SWidget>& InDestinationWidget)
	: NavigationEvent(InNavigationEvent)
	, NavigationReply(InNavigationReply)
	, NavigationSource(InNavigationSource)
	, DestinationWidget(InDestinationWidget)
{
}

FSlateDebuggingWarningEventArgs::FSlateDebuggingWarningEventArgs(
	const FText& InWarning,
	const TSharedPtr<SWidget>& InOptionalContextWidget)
	: Warning(InWarning)
	, OptionalContextWidget(InOptionalContextWidget)
{
}

FSlateDebuggingMouseCaptureEventArgs::FSlateDebuggingMouseCaptureEventArgs(
	const TSharedPtr<SWidget>& InCapturingWidget)
	: CapturingWidget(InCapturingWidget)
{
}

#if WITH_SLATE_DEBUGGING

FSlateDebugging::FBeginWindow FSlateDebugging::BeginWindow;

FSlateDebugging::FEndWindow FSlateDebugging::EndWindow;

FSlateDebugging::FBeginWidgetPaint FSlateDebugging::BeginWidgetPaint;

FSlateDebugging::FEndWidgetPaint FSlateDebugging::EndWidgetPaint;

FSlateDebugging::FDrawElement FSlateDebugging::ElementAdded;

FSlateDebugging::FWidgetWarningEvent FSlateDebugging::Warning;

FSlateDebugging::FWidgetInputEvent FSlateDebugging::InputEvent;

FSlateDebugging::FWidgetFocusEvent FSlateDebugging::FocusEvent;

FSlateDebugging::FWidgetNavigationEvent FSlateDebugging::NavigationEvent;

FSlateDebugging::FWidgetMouseCaptureEvent FSlateDebugging::MouseCaptureEvent;

FSlateDebugging::FUICommandRun FSlateDebugging::CommandRun;

void FSlateDebugging::BroadcastWarning(const FText& WarningText, const TSharedPtr<SWidget>& OptionalContextWidget)
{
	Warning.Broadcast(FSlateDebuggingWarningEventArgs(WarningText, OptionalContextWidget));
}

void FSlateDebugging::BroadcastInputEvent(ESlateDebuggingInputEvent InputEventType, const FReply& InReply)
{
	if (InReply.IsEventHandled())
	{
		InputEvent.Broadcast(FSlateDebuggingInputEventArgs(InputEventType, InReply, TSharedPtr<SWidget>(), TEXT("")));
	}
}

void FSlateDebugging::BroadcastInputEvent(ESlateDebuggingInputEvent InputEventType, const TSharedPtr<SWidget>& HandlerWidget)
{
	InputEvent.Broadcast(FSlateDebuggingInputEventArgs(InputEventType, FReply::Handled(), TSharedPtr<SWidget>(), TEXT("")));
}

void FSlateDebugging::BroadcastInputEvent(ESlateDebuggingInputEvent InputEventType, const FReply& InReply, const TSharedPtr<SWidget>& HandlerWidget)
{
	if (InReply.IsEventHandled())
	{
		InputEvent.Broadcast(FSlateDebuggingInputEventArgs(InputEventType, InReply, HandlerWidget, TEXT("")));
	}
}

void FSlateDebugging::BroadcastInputEvent(ESlateDebuggingInputEvent InputEventType, const FReply& InReply, const TSharedPtr<SWidget>& HandlerWidget, const FString& AdditionalContent)
{
	if (InReply.IsEventHandled())
	{
		InputEvent.Broadcast(FSlateDebuggingInputEventArgs(InputEventType, InReply, HandlerWidget, AdditionalContent));
	}
}

void FSlateDebugging::BroadcastFocusChanging(const FFocusEvent& InFocusEvent, const FWeakWidgetPath& InOldFocusedWidgetPath, const TSharedPtr<SWidget>& InOldFocusedWidget, const FWidgetPath& InNewFocusedWidgetPath, const TSharedPtr<SWidget>& InNewFocusedWidget)
{
	FocusEvent.Broadcast(FSlateDebuggingFocusEventArgs(ESlateDebuggingFocusEvent::FocusChanging, InFocusEvent, InOldFocusedWidgetPath, InOldFocusedWidget, InNewFocusedWidgetPath, InNewFocusedWidget));
}

void FSlateDebugging::BroadcastFocusLost(const FFocusEvent& InFocusEvent, const FWeakWidgetPath& InOldFocusedWidgetPath, const TSharedPtr<SWidget>& InOldFocusedWidget, const FWidgetPath& InNewFocusedWidgetPath, const TSharedPtr<SWidget>& InNewFocusedWidget)
{
	FocusEvent.Broadcast(FSlateDebuggingFocusEventArgs(ESlateDebuggingFocusEvent::FocusLost, InFocusEvent, InOldFocusedWidgetPath, InOldFocusedWidget, InNewFocusedWidgetPath, InNewFocusedWidget));
}

void FSlateDebugging::BroadcastFocusReceived(const FFocusEvent& InFocusEvent, const FWeakWidgetPath& InOldFocusedWidgetPath, const TSharedPtr<SWidget>& InOldFocusedWidget, const FWidgetPath& InNewFocusedWidgetPath, const TSharedPtr<SWidget>& InNewFocusedWidget)
{
	FocusEvent.Broadcast(FSlateDebuggingFocusEventArgs(ESlateDebuggingFocusEvent::FocusReceived, InFocusEvent, InOldFocusedWidgetPath, InOldFocusedWidget, InNewFocusedWidgetPath, InNewFocusedWidget));
}

void FSlateDebugging::AttemptNavigation(const FNavigationEvent& InNavigationEvent, const FNavigationReply& InNavigationReply, const FWidgetPath& InNavigationSource, const TSharedPtr<SWidget>& InDestinationWidget)
{
	NavigationEvent.Broadcast(FSlateDebuggingNavigationEventArgs(InNavigationEvent, InNavigationReply, InNavigationSource, InDestinationWidget));
}

DECLARE_MULTICAST_DELEGATE_OneParam(FWidgetMouseCaptureEvent, const FSlateDebuggingMouseCaptureEventArgs& /*EventArgs*/);
static FWidgetMouseCaptureEvent MouseCaptureEvent;

void FSlateDebugging::MouseCapture(const TSharedPtr<SWidget>& InCapturingWidget)
{
	MouseCaptureEvent.Broadcast(FSlateDebuggingMouseCaptureEventArgs(InCapturingWidget));
}

#endif

#undef LOCTEXT_NAMESPACE