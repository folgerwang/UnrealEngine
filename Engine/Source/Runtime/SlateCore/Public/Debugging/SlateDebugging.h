// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

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

#if WITH_SLATE_DEBUGGING

class SLATECORE_API FSlateDebugging
{
public:
	/**  */
	DECLARE_MULTICAST_DELEGATE_OneParam(FBeginWindow, const FSlateWindowElementList& /*ElementList*/);
	static FBeginWindow BeginWindow;

	DECLARE_MULTICAST_DELEGATE_OneParam(FEndWindow, const FSlateWindowElementList& /*ElementList*/);
	static FEndWindow EndWindow;

	/**  */
	DECLARE_MULTICAST_DELEGATE_SixParams(FBeginWidgetPaint, const SWidget* /*Widget*/, const FPaintArgs& /*Args*/, const FGeometry& /*AllottedGeometry*/, const FSlateRect& /*MyCullingRect*/, const FSlateWindowElementList& /*OutDrawElements*/, int32 /*LayerId*/);
	static FBeginWidgetPaint BeginWidgetPaint;

	/**  */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FEndWidgetPaint, const SWidget* /*Widget*/, const FSlateWindowElementList& /*OutDrawElements*/, int32 /*LayerId*/);
	static FEndWidgetPaint EndWidgetPaint;

	/**
	 * Called as soon as the element is added to the element list.
	 * Note that these are not valid until the widget finishes painting, or you can resolve them all after the window finishes painting.
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FDrawElement, const FSlateWindowElementList& /*ElementList*/, int32 /*ElementIndex*/);
	static FDrawElement ElementAdded;

private:

	// This class is only for namespace use
	FSlateDebugging() {}
};

#endif