// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Misc/Optional.h"
#include "Curves/KeyHandle.h"
#include "CurveEditorTypes.h"

struct FCurvePointHandle;


/**
 * A set of key handles implemented as a sorted array for transparent passing to TArrayView<> APIs.
 * Lookup is achieved via binary search: O(log(n)).
 */
struct FKeyHandleSet
{
	/**
	 * Add a new key handle to this set
	 */
	void Add(FKeyHandle Handle);

	/**
	 * Remove a handle from this set if it already exists, otherwise add it to the set
	 */
	void Toggle(FKeyHandle Handle);

	/**
	 * Remove a handle from this set
	 */
	void Remove(FKeyHandle Handle);

	/**
	 * Check whether the specified handle exists in this set
	 */
	bool Contains(FKeyHandle Handle) const;

	/**
	 * Retrieve the number of handles in this set
	 */
	FORCEINLINE int32 Num() const { return SortedHandles.Num(); }

	/**
	 * Retrieve a constant view of this set as an array
	 */
	FORCEINLINE TArrayView<const FKeyHandle> AsArray() const { return SortedHandles; }

private:

	/** Sorted array of key handles */
	TArray<FKeyHandle, TInlineAllocator<1>> SortedHandles;
};


/**
 * Class responsible for tracking selections of keys.
 * Only one type of point selection is supported at a time (key, arrive tangent, or leave tangent)
 */
struct CURVEEDITOR_API FCurveEditorSelection
{
	/**
	 * Default constructor
	 */
	FCurveEditorSelection();

	/**
	 * Retrieve the current type of selection
	 */
	FORCEINLINE ECurvePointType GetSelectionType() const { return SelectionType; }

	/**
	 * Retrieve this selection's serial number. Incremented whenever a change is made to the selection.
	 */
	FORCEINLINE uint32 GetSerialNumber() const { return SerialNumber; }

	/**
	 * Check whether the selection is empty
	 */
	FORCEINLINE bool IsEmpty() const { return CurveToSelectedKeys.Num() == 0; }

	/**
	 * Retrieve all selected key handles, organized by curve ID
	 */
	FORCEINLINE const TMap<FCurveModelID, FKeyHandleSet>& GetAll() const { return CurveToSelectedKeys; }

	/**
	 * Retrieve a set of selected key handles for the specified curve
	 */
	const FKeyHandleSet* FindForCurve(FCurveModelID InCurveID) const;

	/**
	 * Count the total number of selected keys by accumulating the number of selected keys for each curve
	 */
	int32 Count() const;

	/**
	 * Check whether the specified handle is selected
	 */
	bool IsSelected(FCurvePointHandle InHandle) const;

	/**
	 * Check whether the specified handle and curve ID is contained in this selection.
	 *
	 * @note: Does not compare the current selection type
	 */
	bool Contains(FCurveModelID CurveID, FKeyHandle KeyHandle) const;

public:

	/**
	 * Add a point handle to this selection, changing the selection type if necessary.
	 */
	void Add(FCurvePointHandle InHandle);

	/**
	 * Add a key handle to this selection, changing the selection type if necessary.
	 */
	void Add(FCurveModelID CurveID, ECurvePointType PointType, FKeyHandle KeyHandle);

	/**
	 * Add key handles to this selection, changing the selection type if necessary.
	 */
	void Add(FCurveModelID CurveID, ECurvePointType PointType, TArrayView<const FKeyHandle> Keys);

public:

	/**
	 * Toggle the selection of the specified point handle, changing the selection type if necessary.
	 */
	void Toggle(FCurvePointHandle InHandle);

	/**
	 * Toggle the selection of the specified key handle, changing the selection type if necessary.
	 */
	void Toggle(FCurveModelID CurveID, ECurvePointType PointType, FKeyHandle KeyHandle);

	/**
	 * Toggle the selection of the specified key handles, changing the selection type if necessary.
	 */
	void Toggle(FCurveModelID CurveID, ECurvePointType PointType, TArrayView<const FKeyHandle> Keys);

public:

	/**
	 * Remove the specified point handle from the selection
	 */
	void Remove(FCurvePointHandle InHandle);

	/**
	 * Remove the specified key handle from the selection
	 */
	void Remove(FCurveModelID CurveID, ECurvePointType PointType, FKeyHandle KeyHandle);

	/**
	 * Remove the specified key handles from the selection
	 */
	void Remove(FCurveModelID CurveID, ECurvePointType PointType, TArrayView<const FKeyHandle> Keys);

	/**
	 * Remove all key handles associated with the specified curve ID from the selection
	 */
	void Remove(FCurveModelID InCurveID);

	/**
	 * Clear the selection entirely
	 */
	void Clear();

	/**
	 * Change the current selection type if it differs from the type specified
	 */
	void ChangeSelectionPointType(ECurvePointType InPointType);

private:

	/** A serial number that increments every time a change is made to the selection */
	uint32 SerialNumber;

	/** The type of point currently selected */
	ECurvePointType SelectionType;

	/** A map of selected handles stored by curve ID */
	TMap<FCurveModelID, FKeyHandleSet> CurveToSelectedKeys;
};