// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CurveEditorSelection.h"
#include "CurveEditorTypes.h"
#include "Algo/BinarySearch.h"

FCurveEditorSelection::FCurveEditorSelection()
{
	SelectionType = ECurvePointType::Key;
	SerialNumber = 0;
}

void FCurveEditorSelection::Clear()
{
	SelectionType = ECurvePointType::Key;
	CurveToSelectedKeys.Reset();
	++SerialNumber;
}

const FKeyHandleSet* FCurveEditorSelection::FindForCurve(FCurveModelID InCurveID) const
{
	return CurveToSelectedKeys.Find(InCurveID);
}

int32 FCurveEditorSelection::Count() const
{
	int32 Num = 0;
	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveToSelectedKeys)
	{
		Num += Pair.Value.Num();
	}
	return Num;
}

bool FCurveEditorSelection::IsSelected(FCurvePointHandle InHandle) const
{
	const FKeyHandleSet* SelectedKeys = CurveToSelectedKeys.Find(InHandle.CurveID);
	return SelectionType == InHandle.PointType && Contains(InHandle.CurveID, InHandle.KeyHandle);
}

bool FCurveEditorSelection::Contains(FCurveModelID CurveID, FKeyHandle KeyHandle) const
{
	const FKeyHandleSet* SelectedKeys = CurveToSelectedKeys.Find(CurveID);
	return SelectedKeys && SelectedKeys->Contains(KeyHandle);
}

void FCurveEditorSelection::Add(FCurvePointHandle InHandle)
{
	Add(InHandle.CurveID, InHandle.PointType, InHandle.KeyHandle);
}

void FCurveEditorSelection::Add(FCurveModelID CurveID, ECurvePointType PointType, FKeyHandle KeyHandle)
{
	Add(CurveID, PointType, TArrayView<const FKeyHandle>(&KeyHandle, 1));
}

void FCurveEditorSelection::Add(FCurveModelID CurveID, ECurvePointType PointType, TArrayView<const FKeyHandle> Keys)
{
	if (Keys.Num() > 0)
	{
		ChangeSelectionPointType(PointType);

		FKeyHandleSet& SelectedKeys = CurveToSelectedKeys.FindOrAdd(CurveID);
		for (FKeyHandle Key : Keys)
		{
			SelectedKeys.Add(Key);
		}
	}

	++SerialNumber;
}

void FCurveEditorSelection::Toggle(FCurvePointHandle InHandle)
{
	Toggle(InHandle.CurveID, InHandle.PointType, InHandle.KeyHandle);
}

void FCurveEditorSelection::Toggle(FCurveModelID CurveID, ECurvePointType PointType, FKeyHandle KeyHandle)
{
	Toggle(CurveID, PointType, TArrayView<const FKeyHandle>(&KeyHandle, 1));
}

void FCurveEditorSelection::Toggle(FCurveModelID CurveID, ECurvePointType PointType, TArrayView<const FKeyHandle> Keys)
{
	if (Keys.Num() > 0)
	{
		ChangeSelectionPointType(PointType);

		FKeyHandleSet& SelectedKeys = CurveToSelectedKeys.FindOrAdd(CurveID);
		for (FKeyHandle Key : Keys)
		{
			SelectedKeys.Toggle(Key);
		}

		if (SelectedKeys.Num() == 0)
		{
			CurveToSelectedKeys.Remove(CurveID);
		}
	}

	++SerialNumber;
}

void FCurveEditorSelection::Remove(FCurvePointHandle InHandle)
{
	Remove(InHandle.CurveID, InHandle.PointType, InHandle.KeyHandle);
}

void FCurveEditorSelection::Remove(FCurveModelID CurveID, ECurvePointType PointType, FKeyHandle KeyHandle)
{
	Remove(CurveID, PointType, TArrayView<const FKeyHandle>(&KeyHandle, 1));
}

void FCurveEditorSelection::Remove(FCurveModelID CurveID, ECurvePointType PointType, TArrayView<const FKeyHandle> Keys)
{
	if (Keys.Num() > 0)
	{
		ChangeSelectionPointType(PointType);

		FKeyHandleSet& SelectedKeys = CurveToSelectedKeys.FindOrAdd(CurveID);
		for (FKeyHandle Key : Keys)
		{
			SelectedKeys.Remove(Key);
		}
	}

	++SerialNumber;
}

void FCurveEditorSelection::Remove(FCurveModelID InCurveID)
{
	CurveToSelectedKeys.Remove(InCurveID);
	++SerialNumber;
}

void FCurveEditorSelection::ChangeSelectionPointType(ECurvePointType InPointType)
{
	if (SelectionType != InPointType)
	{
		SelectionType = InPointType;
		//CurveToSelectedKeys.Reset();
		++SerialNumber;
	}
}


void FKeyHandleSet::Add(FKeyHandle Handle)
{
	int32 ExistingIndex = Algo::LowerBound(SortedHandles, Handle);
	if (ExistingIndex >= SortedHandles.Num() || SortedHandles[ExistingIndex] != Handle)
	{
		SortedHandles.Insert(Handle, ExistingIndex);
	}
}

void FKeyHandleSet::Toggle(FKeyHandle Handle)
{
	int32 ExistingIndex = Algo::LowerBound(SortedHandles, Handle);
	if (ExistingIndex < SortedHandles.Num() && SortedHandles[ExistingIndex] == Handle)
	{
		SortedHandles.RemoveAt(ExistingIndex, 1, false);
	}
	else
	{
		SortedHandles.Insert(Handle, ExistingIndex);
	}
}

void FKeyHandleSet::Remove(FKeyHandle Handle)
{
	int32 ExistingIndex = Algo::LowerBound(SortedHandles, Handle);
	if (ExistingIndex < SortedHandles.Num() && SortedHandles[ExistingIndex] == Handle)
	{
		SortedHandles.RemoveAt(ExistingIndex, 1, false);
	}
}

bool FKeyHandleSet::Contains(FKeyHandle Handle) const
{
	return Algo::BinarySearch(SortedHandles, Handle) != INDEX_NONE;
}