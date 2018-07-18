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

		if (ExistingElementList->GetWindow() == ForWindow )
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

void FSlateDrawBuffer::RemoveUnusedWindowElement(const TArray<TSharedRef<SWindow>>& AllWindows)
{
	// Remove any window elements that are no longer valid.
	for (int32 WindowIndex = 0; WindowIndex < WindowElementLists.Num(); ++WindowIndex)
	{
		TSharedPtr<SWindow> CandidateWindow = WindowElementLists[WindowIndex]->GetWindow();
		if (CandidateWindow.IsValid() == false || !AllWindows.Contains(CandidateWindow.ToSharedRef()))
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
		if (WindowElementListsPool[WindowIndex]->GetWindow().IsValid() == false)
		{
			WindowElementListsPool.RemoveAtSwap(WindowIndex);
			--WindowIndex;
		}
	}

	// Move all the window elements back into the pool.
	for (TSharedRef<FSlateWindowElementList> ExistingList : WindowElementLists)
	{
		if (ExistingList->GetWindow().IsValid())
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
