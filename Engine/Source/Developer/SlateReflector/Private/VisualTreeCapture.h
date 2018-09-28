// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWindow;
class SWidget;
class FSlateDrawElement;
class FSlateClippingState;
class FSlateWindowElementList;
class FPaintArgs;
struct FGeometry;
class FSlateRect;

class FVisualEntry
{
public:
	FVector2D TopLeft;
	FVector2D TopRight;
	FVector2D BottomLeft;
	FVector2D BottomRight;

	int32 LayerId;
	int32 ClippingIndex;
	int32 ElementIndex;
	TWeakPtr<const SWidget> Widget;

	FVisualEntry(int32 InElementIndex);

	void Resolve(const FSlateWindowElementList& ElementList);

	bool IsPointInside(const FVector2D& Point) const;
};

class FVisualTreeSnapshot : public TSharedFromThis<FVisualTreeSnapshot>
{
public:
	TSharedPtr<const SWidget> Pick(FVector2D Point);
	
public:
	TArray<FVisualEntry> Entries;
	TArray<FSlateClippingState> ClippingStates;
	TArray<TWeakPtr<const SWidget>> WidgetStack;
};

class FVisualTreeCapture
{
public:
	FVisualTreeCapture();
	~FVisualTreeCapture();

	void Enable();
	void Disable();

	TSharedPtr<FVisualTreeSnapshot> GetVisualTreeForWindow(SWindow* InWindow);
	
private:
	void BeginWindow(const FSlateWindowElementList& ElementList);
	void EndWindow(const FSlateWindowElementList& ElementList);

	void BeginWidgetPaint(const SWidget* Widget, const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, const FSlateWindowElementList& ElementList, int32 LayerId);

	/**  */
	void EndWidgetPaint(const SWidget* Widget, const FSlateWindowElementList& ElementList, int32 LayerId);

	/**  */
	void ElementAdded(const FSlateWindowElementList& ElementList, int32 InElementIndex);

	void OnWindowBeingDestroyed(const SWindow& WindowBeingDestoyed);
private:
	TMap<const SWindow*, TSharedPtr<FVisualTreeSnapshot>> VisualTrees;
};