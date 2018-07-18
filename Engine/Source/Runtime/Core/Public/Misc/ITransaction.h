// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/UObjectHierarchyFwd.h"
#include "Change.h"

// Class for handling undo/redo transactions among objects.
typedef void(*STRUCT_DC)( void* TPtr );						// default construct
typedef void(*STRUCT_AR)( class FArchive& Ar, void* TPtr );	// serialize
typedef void(*STRUCT_DTOR)( void* TPtr );					// destruct


/**
 * Interface for transaction object annotations.
 *
 * Transaction object annotations are used for attaching additional user defined data to a transaction.
 * This is sometimes useful, because the transaction system only remembers changes that are serializable
 * on the UObject that a modification was performed on, but it does not see other changes that may have
 * to be remembered in order to properly restore the object internals.
 */
class ITransactionObjectAnnotation 
{ 
public:
	virtual ~ITransactionObjectAnnotation() {}
	virtual void AddReferencedObjects(class FReferenceCollector& Collector) = 0;
};


/** Delta-change information for an object that was transacted */
struct FTransactionObjectDeltaChange
{
	FTransactionObjectDeltaChange()
		: bHasNameChange(false)
		, bHasOuterChange(false)
		, bHasPendingKillChange(false)
		, bHasNonPropertyChanges(false)
	{
	}

	bool HasChanged() const
	{
		return bHasNameChange || bHasOuterChange || bHasPendingKillChange || bHasNonPropertyChanges || ChangedProperties.Num() > 0;
	}

	/** True if the object name has changed */
	bool bHasNameChange : 1;
	/** True of the object outer has changed */
	bool bHasOuterChange : 1;
	/** True if the object "pending kill" state has changed */
	bool bHasPendingKillChange : 1;
	/** True if the object has changes other than property changes (may be caused by custom serialization) */
	bool bHasNonPropertyChanges : 1;
	/** Array of properties that have changed on the object */
	TArray<FName> ChangedProperties;
};


/** Different kinds of actions that can trigger a transaction object event */
enum class ETransactionObjectEventType : uint8
{
	/** This event was caused by an undo/redo operation */
	UndoRedo,
	/** This event was caused by a transaction being finalized within the transaction system */
	Finalized,
	/** This event was caused by a transaction snapshot. Several of these may be generated in the case of an interactive change */
	Snapshot,
};


/**
 * Transaction object events.
 *
 * Transaction object events are used to notify objects when they are transacted in some way.
 * This mostly just means that an object has had an undo/redo applied to it, however an event is also triggered
 * when the object has been finalized as part of a transaction (allowing you to detect object changes).
 */
class FTransactionObjectEvent
{
public:
	FTransactionObjectEvent(const ETransactionObjectEventType InEventType, const FTransactionObjectDeltaChange& InDeltaChange, const TSharedPtr<ITransactionObjectAnnotation>& InAnnotation, const FName InOriginalObjectName, const FName InOriginalObjectPathName, const FName InOriginalObjectOuterPathName)
		: EventType(InEventType)
		, DeltaChange(InDeltaChange)
		, Annotation(InAnnotation)
		, OriginalObjectName(InOriginalObjectName)
		, OriginalObjectPathName(InOriginalObjectPathName)
		, OriginalObjectOuterPathName(InOriginalObjectOuterPathName)
	{
	}

	/** What kind of action caused this event? */
	ETransactionObjectEventType GetEventType() const
	{
		return EventType;
	}

	/** Was the pending kill state of this object changed? (implies non-property changes) */
	bool HasPendingKillChange() const
	{
		return DeltaChange.bHasPendingKillChange;
	}

	/** Was the name of this object changed? (implies non-property changes) */
	bool HasNameChange() const
	{
		return DeltaChange.bHasNameChange;
	}

	/** Get the original name of this object */
	FName GetOriginalObjectName() const
	{
		return OriginalObjectName;
	}

	/** Get the original path name of this object */
	FName GetOriginalObjectPathName() const
	{
		return OriginalObjectPathName;
	}

	/** Was the outer of this object changed? (implies non-property changes) */
	bool HasOuterChange() const
	{
		return DeltaChange.bHasOuterChange;
	}

	/** Get the original outer path name of this object */
	FName GetOriginalObjectOuterPathName() const
	{
		return OriginalObjectOuterPathName;
	}

	/** Were any non-property changes made to the object? */
	bool HasNonPropertyChanges() const
	{
		return DeltaChange.bHasNameChange || DeltaChange.bHasOuterChange || DeltaChange.bHasPendingKillChange || DeltaChange.bHasNonPropertyChanges;
	}

	/** Were any property changes made to the object? */
	bool HasPropertyChanges() const
	{
		return DeltaChange.ChangedProperties.Num() > 0;
	}

	/** Get the list of changed properties. Each entry is actually a chain of property names (root -> leaf) separated by a dot, eg) "ObjProp.StructProp". */
	const TArray<FName>& GetChangedProperties() const
	{
		return DeltaChange.ChangedProperties;
	}

	/** Get the annotation object associated with the object being transacted (if any). */
	TSharedPtr<const ITransactionObjectAnnotation> GetAnnotation() const
	{
		return Annotation;
	}

private:
	ETransactionObjectEventType EventType;
	FTransactionObjectDeltaChange DeltaChange;
	TSharedPtr<ITransactionObjectAnnotation> Annotation;
	FName OriginalObjectName;
	FName OriginalObjectPathName;
	FName OriginalObjectOuterPathName;
};


/**
 * Interface for transactions.
 *
 * Transactions are created each time an UObject is modified, for example in the Editor.
 * The data stored inside a transaction object can then be used to provide undo/redo functionality.
 */
class ITransaction
{
public:

	/** Called when this transaction is completed to finalize the transaction */
	virtual void Finalize( ) = 0;

	/** Applies the transaction. */
	virtual void Apply( ) = 0;

	/**
	 * Saves an array to the transaction.
	 *
	 * @param Object The object that owns the array.
	 * @param Array The array to save.
	 * @param Index 
	 * @param Count 
	 * @param Oper
	 * @param ElementSize
	 * @param Serializer
	 * @param Destructor
	 * @see SaveObject
	 */
	virtual void SaveArray( UObject* Object, class FScriptArray* Array, int32 Index, int32 Count, int32 Oper, int32 ElementSize, STRUCT_DC DefaultConstructor, STRUCT_AR Serializer, STRUCT_DTOR Destructor ) = 0;

	/**
	 * Saves an UObject to the transaction.
	 *
	 * @param Object The object to save.
	 *
	 * @see SaveArray
	 */
	virtual void SaveObject( UObject* Object ) = 0;

	/**
	 * Stores a command that can be used to undo a change to the specified object.  This may be called multiple times in the
	 * same transaction to stack up changes that will be rolled back in reverse order.  No copy of the object itself is stored.
	 *
	 * @param Object The object the undo change will apply to
	 * @param CustomChange The change that can be used to undo the changes to this object.
	 */
	virtual void StoreUndo( UObject* Object, TUniquePtr<FChange> CustomChange ) = 0;

	/**
	 * Sets the transaction's primary object.
	 *
	 * @param Object The primary object to set.
	 */
	virtual void SetPrimaryObject( UObject* Object ) = 0;

	/**
	 * Snapshots a UObject within the transaction.
	 *
	 * @param Object The object to snapshot.
	 */
	virtual void SnapshotObject( UObject* Object ) = 0;
};
