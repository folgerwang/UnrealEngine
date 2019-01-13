// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Debugging/SlateDebugging.h"

#if WITH_SLATE_DEBUGGING

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Input/Reply.h"
#include "HAL/IConsoleManager.h"

/**
 * Allows debugging the behavior of Slate from the console.
 * Basics:
 *   Start - SlateDebugger.Start
 *   Stop  - SlateDebugger.Stop
 *
 * Notes:
 *   If you need to begin debugging slate on startup do, -execcmds="SlateDebugger.Start"
 */
class FConsoleSlateDebugger
{
public:
	FConsoleSlateDebugger();
	~FConsoleSlateDebugger();

	void StartDebugging();
	void StopDebugging();

private:

	void RemoveListeners();
	void UpdateListeners();

	void SetInputFilter(const TArray< FString >& Parms);

	void OnWarning(const FSlateDebuggingWarningEventArgs& EventArgs);
	void OnInputEvent(const FSlateDebuggingInputEventArgs& EventArgs);
	void OnFocusEvent(const FSlateDebuggingFocusEventArgs& EventArgs);
	void OnNavigationEvent(const FSlateDebuggingNavigationEventArgs& EventArgs);
	void OnStateChangeEvent(const FSlateDebuggingMouseCaptureEventArgs& EventArgs);

	void OptionallyDumpCallStack();

private:

	/** Should we capture and dump the callstack when events happen? */
	int32 bCaptureStack;

	/** Which events should we log about. */
	TBitArray<> EnabledInputEvents;

	/**
	 * Console objects
	 */
	FAutoConsoleCommand StartDebuggingCommand;
	FAutoConsoleCommand StopDebuggingCommand;
	FAutoConsoleVariableRef CaptureStackVariable;
	FAutoConsoleCommand SetInputFilterCommand;
};

#endif //WITH_SLATE_DEBUGGING