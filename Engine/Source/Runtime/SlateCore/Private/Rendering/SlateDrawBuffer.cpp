// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Rendering/SlateDrawBuffer.h"
#include "Rendering/DrawElements.h"
#include "Application/SlateApplicationBase.h"


/* FSlateDrawBuffer interface
 *****************************************************************************/

FSlateDrawBuffer::~FSlateDrawBuffer()
{
}

FSlateWindowElementList& FSlateDrawBuffer::AddWindowElementList(TSharedRef<SWindow> ForWindow)
{
	for ( int32 WindowIndex = 0; WindowIndex < WindowElementListsPool.Num(); ++WindowIndex )
	{
		TSharedRef<FSlateWindowElementList> ExistingElementList = WindowElementListsPool[WindowIndex];

		if (ExistingElementList->GetPaintWindow() == &ForWindow.Get())
		{
			WindowElementLists.Add(ExistingElementList);
			WindowElementListsPool.RemoveAtSwap(WindowIndex);

			ExistingElementList->ResetElementBuffers();

			return *ExistingElementList;
		}
	}

	TSharedRef<FSlateWindowElementList> WindowElements = MakeShared<FSlateWindowElementList>(ForWindow);
	WindowElementLists.Add(WindowElements);

	return *WindowElements;
}

void FSlateDrawBuffer::RemoveUnusedWindowElement(const TArray<SWindow*>& AllWindows)
{
	// Remove any window elements that are no longer valid.
	for (int32 WindowIndex = 0; WindowIndex < WindowElementLists.Num(); ++WindowIndex)
	{
		SWindow* CandidateWindow = WindowElementLists[WindowIndex]->GetPaintWindow();
		if (!CandidateWindow || !AllWindows.Contains(CandidateWindow))
		{
			WindowElementLists.RemoveAtSwap(WindowIndex);
			--WindowIndex;
		}
	}
}

bool FSlateDrawBuffer::Lock()
{
	return FPlatformAtomics::InterlockedCompareExchange(&Locked, 1, 0) == 0;
}

void FSlateDrawBuffer::Unlock()
{
	FPlatformAtomics::InterlockedExchange(&Locked, 0);
}

void FSlateDrawBuffer::ClearBuffer()
{
	// Remove any window elements that are no longer valid.
	for (int32 WindowIndex = 0; WindowIndex < WindowElementListsPool.Num(); ++WindowIndex)
	{
		if (WindowElementListsPool[WindowIndex]->GetPaintWindow() == nullptr)
		{
			WindowElementListsPool.RemoveAtSwap(WindowIndex);
			--WindowIndex;
		}
	}

	// Move all the window elements back into the pool.
	for (TSharedRef<FSlateWindowElementList> ExistingList : WindowElementLists)
	{
		if (ExistingList->GetPaintWindow() != nullptr)
		{
			WindowElementListsPool.Add(ExistingList);
		}
	}

	WindowElementLists.Reset();
}


void FSlateDrawBuffer::UpdateResourceVersion(uint32 NewResourceVersion)
{
	if (IsInGameThread() && NewResourceVersion != ResourceVersion)
	{
		WindowElementListsPool.Empty();
		ResourceVersion = NewResourceVersion;
	}
}
