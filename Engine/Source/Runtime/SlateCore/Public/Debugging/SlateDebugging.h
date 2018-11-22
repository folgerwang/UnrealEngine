// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Input/Reply.h"

#ifndef WITH_SLATE_DEBUGGING
	#define WITH_SLATE_DEBUGGING !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#endif

class SWindow;
class SWidget;
struct FGeometry;
class FPaintArgs;
class FSlateWindowElementList;
class FSlateDrawElement;
class FSlateRect;
class FWeakWidgetPath;
class FWidgetPath;
class FNavigationReply;

enum class ESlateDebuggingInputEvent : uint8
{
	MouseMove,
	MouseEnter,
	MouseLeave,
	MouseButtonDown,
	MouseButtonUp,
	MouseButtonDoubleClick,
	MouseWheel,
	TouchStart,
	TouchEnd,
	DragDetected,
	DragEnter,
	DragLeave,
	DragOver,
	DragDrop,
	DropMessage,
	KeyDown,
	KeyUp,
	KeyChar,
	AnalogInput,
	TouchGesture
};

struct SLATECORE_API FSlateDebuggingInputEventArgs
{
public:
	FSlateDebuggingInputEventArgs(ESlateDebuggingInputEvent InInputEventType, const FReply& InReply, const TSharedPtr<SWidget>& InHandlerWidget, const FString& InAdditionalContent);

	const ESlateDebuggingInputEvent InputEventType;
	const FReply& Reply;
	const TSharedPtr<SWidget>& HandlerWidget;
	const FString& AdditionalContent;
};

enum class ESlateDebuggingFocusEvent : uint8
{
	FocusChanging,
	FocusLost,
	FocusReceived
};

struct SLATECORE_API FSlateDebuggingFocusEventArgs
{
public:
	FSlateDebuggingFocusEventArgs(
		ESlateDebuggingFocusEvent InFocusEventType,
		const FFocusEvent& InFocusEvent,
		const FWeakWidgetPath& InOldFocusedWidgetPath,
		const TSharedPtr<SWidget>& InOldFocusedWidget,
		const FWidgetPath& InNewFocusedWidgetPath,
		const TSharedPtr<SWidget>& InNewFocusedWidget
	);

	ESlateDebuggingFocusEvent FocusEventType;
	const FFocusEvent& FocusEvent;
	const FWeakWidgetPath& OldFocusedWidgetPath;
	const TSharedPtr<SWidget>& OldFocusedWidget;
	const FWidgetPath& NewFocusedWidgetPath;
	const TSharedPtr<SWidget>& NewFocusedWidget;
};

struct SLATECORE_API FSlateDebuggingNavigationEventArgs
{
public:
	FSlateDebuggingNavigationEventArgs(
		const FNavigationEvent& InNavigationEvent,
		const FNavigationReply& InNavigationReply,
		const FWidgetPath& InNavigationSource,
		const TSharedPtr<SWidget>& InDestinationWidget
	);

	const FNavigationEvent& NavigationEvent;
	const FNavigationReply& NavigationReply;
	const FWidgetPath& NavigationSource;
	const TSharedPtr<SWidget>& DestinationWidget;
};

#if WITH_SLATE_DEBUGGING

/**
 * 
 */
class SLATECORE_API FSlateDebugging
{
public:
	/** Called when a widget begins painting. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FBeginWindow, const FSlateWindowElementList& /*ElementList*/);
	static FBeginWindow BeginWindow;

	/** Called when a window finishes painting. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FEndWindow, const FSlateWindowElementList& /*ElementList*/);
	static FEndWindow EndWindow;

	/** Called just before a widget paints. */
	DECLARE_MULTICAST_DELEGATE_SixParams(FBeginWidgetPaint, const SWidget* /*Widget*/, const FPaintArgs& /*Args*/, const FGeometry& /*AllottedGeometry*/, const FSlateRect& /*MyCullingRect*/, const FSlateWindowElementList& /*OutDrawElements*/, int32 /*LayerId*/);
	static FBeginWidgetPaint BeginWidgetPaint;

	/** Called after a widget finishes painting. */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FEndWidgetPaint, const SWidget* /*Widget*/, const FSlateWindowElementList& /*OutDrawElements*/, int32 /*LayerId*/);
	static FEndWidgetPaint EndWidgetPaint;

	/**
	 * Called as soon as the element is added to the element list.
	 * NOTE: These elements are not valid until the widget finishes painting, or you can resolve them all after the window finishes painting.
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FDrawElement, const FSlateWindowElementList& /*ElementList*/, int32 /*ElementIndex*/);
	static FDrawElement ElementAdded;

public:
	/**  */
	DECLARE_MULTICAST_DELEGATE_OneParam(FWidgetInputEvent, const FSlateDebuggingInputEventArgs& /*EventArgs*/);
	static FWidgetInputEvent InputEvent;

	static void BroadcastInputEvent(ESlateDebuggingInputEvent InputEventType, const FReply& InReply);
	static void BroadcastInputEvent(ESlateDebuggingInputEvent InputEventType, const TSharedPtr<SWidget>& HandlerWidget);
	static void BroadcastInputEvent(ESlateDebuggingInputEvent InputEventType, const FReply& InReply, const TSharedPtr<SWidget>& HandlerWidget);
	static void BroadcastInputEvent(ESlateDebuggingInputEvent InputEventType, const FReply& InReply, const TSharedPtr<SWidget>& HandlerWidget, const FString& AdditionalContent);

public:
	/**  */
	DECLARE_MULTICAST_DELEGATE_OneParam(FWidgetFocusEvent, const FSlateDebuggingFocusEventArgs& /*EventArgs*/);
	static FWidgetFocusEvent FocusEvent;

	static void BroadcastFocusChanging(const FFocusEvent& InFocusEvent, const FWeakWidgetPath& InOldFocusedWidgetPath, const TSharedPtr<SWidget>& InOldFocusedWidget, const FWidgetPath& InNewFocusedWidgetPath, const TSharedPtr<SWidget>& InNewFocusedWidget);
	static void BroadcastFocusLost(const FFocusEvent& InFocusEvent, const FWeakWidgetPath& InOldFocusedWidgetPath, const TSharedPtr<SWidget>& InOldFocusedWidget, const FWidgetPath& InNewFocusedWidgetPath, const TSharedPtr<SWidget>& InNewFocusedWidget);
	static void BroadcastFocusReceived(const FFocusEvent& InFocusEvent, const FWeakWidgetPath& InOldFocusedWidgetPath, const TSharedPtr<SWidget>& InOldFocusedWidget, const FWidgetPath& InNewFocusedWidgetPath, const TSharedPtr<SWidget>& InNewFocusedWidget);

public:
	/**  */
	DECLARE_MULTICAST_DELEGATE_OneParam(FWidgetNavigationEvent, const FSlateDebuggingNavigationEventArgs& /*EventArgs*/);
	static FWidgetNavigationEvent NavigationEvent;

	static void AttemptNavigation(const FNavigationEvent& InNavigationEvent, const FNavigationReply& InNavigationReply, const FWidgetPath& InNavigationSource, const TSharedPtr<SWidget>& InDestinationWidget);

public:
	/**  */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FUICommandRun, const FName& /*CommandName*/, const FText& /*CommandLabel*/);
	static FUICommandRun CommandRun;

private:

	// This class is only for namespace use
	FSlateDebugging() {}
};

#endif