// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"
#include "Misc/MemStack.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "Engine/Level.h"
#include "Components/ActorComponent.h"
#include "Model.h"
#include "Editor/Transactor.h"
#include "Editor/TransBuffer.h"
#include "Components/ModelComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BSPOps.h"
#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogEditorTransaction, Log, All);

inline UObject* BuildSubobjectKey(UObject* InObj, TArray<FName>& OutHierarchyNames)
{
	auto UseOuter = [](const UObject* Obj)
	{
		if (Obj == nullptr)
		{
			return false;
		}

		const bool bIsCDO = Obj->HasAllFlags(RF_ClassDefaultObject);
		const UObject* CDO = bIsCDO ? Obj : nullptr;
		const bool bIsClassCDO = (CDO != nullptr) ? (CDO->GetClass()->ClassDefaultObject == CDO) : false;
		if(!bIsClassCDO && CDO)
		{
			// Likely a trashed CDO, try to recover. Only known cause of this is
			// ambiguous use of DSOs:
			CDO = CDO->GetClass()->ClassDefaultObject;
		}
		const UActorComponent* AsComponent = Cast<UActorComponent>(Obj);
		const bool bIsDSO = Obj->HasAnyFlags(RF_DefaultSubObject);
		const bool bIsSCSComponent = AsComponent && AsComponent->IsCreatedByConstructionScript();
		return (bIsCDO && bIsClassCDO) || bIsDSO || bIsSCSComponent;
	};
	
	UObject* Outermost = nullptr;

	UObject* Iter = InObj;
	while (UseOuter(Iter))
	{
		OutHierarchyNames.Add(Iter->GetFName());
		Iter = Iter->GetOuter();
		Outermost = Iter;
	}

	return Outermost;
}

/*-----------------------------------------------------------------------------
	A single transaction.
-----------------------------------------------------------------------------*/

FTransaction::FObjectRecord::FObjectRecord(FTransaction* Owner, UObject* InObject, TUniquePtr<FChange> InCustomChange, FScriptArray* InArray, int32 InIndex, int32 InCount, int32 InOper, int32 InElementSize, STRUCT_DC InDefaultConstructor, STRUCT_AR InSerializer, STRUCT_DTOR InDestructor)
	:	Object				( InObject )
	,	CustomChange		( MoveTemp( InCustomChange ) )
	,	Array				( InArray )
	,	Index				( InIndex )
	,	Count				( InCount )
	,	Oper				( InOper )
	,	ElementSize			( InElementSize )
	,	DefaultConstructor	( InDefaultConstructor )
	,	Serializer			( InSerializer )
	,	Destructor			( InDestructor )
	,	bRestored			( false )
	,   bFinalized			( false )
	,	bSnapshot			( false )
	,	bWantsBinarySerialization ( true )
{
	// Blueprint compile-in-place can alter class layout so use tagged serialization for objects relying on a UBlueprint's Class
	if (UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(InObject->GetClass()))
	{
		bWantsBinarySerialization = false; 
	}
	// Data tables can contain user structs, so it's unsafe to use binary
	if (UDataTable* DataTable = Cast<UDataTable>(InObject))
	{
		bWantsBinarySerialization = false;
	}

	// Don't bother saving the object state if we have a custom change which can perform the undo operation
	if( CustomChange.IsValid() )
	{
		// @todo mesheditor debug
		//GWarn->Logf( TEXT( "------------ Saved Undo Change ------------" ) );
		//CustomChange->PrintToLog( *GWarn );
		//GWarn->Logf( TEXT( "-------------------------------------------" ) );
	}
	else
	{
		SerializedObject.SetObject(Object.Get());
		FWriter Writer( SerializedObject, bWantsBinarySerialization );
		SerializeContents( Writer, Oper );
	}
}

void FTransaction::FObjectRecord::SerializeContents( FArchive& Ar, int32 InOper )
{
	if( Array )
	{
		const bool bWasArIgnoreOuterRef = Ar.ArIgnoreOuterRef;
		if (Object.SubObjectHierarchyID.Num() != 0)
		{
			Ar.ArIgnoreOuterRef = true;
		}

		//UE_LOG( LogEditorTransaction, Log, TEXT("Array %s %i*%i: %i"), Object ? *Object->GetFullName() : TEXT("Invalid Object"), Index, ElementSize, InOper);

		check((SIZE_T)Array >= (SIZE_T)Object.Get() + sizeof(UObject));
		check((SIZE_T)Array + sizeof(FScriptArray) <= (SIZE_T)Object.Get() + Object->GetClass()->GetPropertiesSize());
		check(ElementSize!=0);
		check(DefaultConstructor!=NULL);
		check(Serializer!=NULL);
		check(Index>=0);
		check(Count>=0);
		if( InOper==1 )
		{
			// "Saving add order" or "Undoing add order" or "Redoing remove order".
			if( Ar.IsLoading() )
			{
				checkSlow(Index+Count<=Array->Num());
				for( int32 i=Index; i<Index+Count; i++ )
				{
					Destructor( (uint8*)Array->GetData() + i*ElementSize );
				}
				Array->Remove( Index, Count, ElementSize );
			}
		}
		else
		{
			// "Undo/Redo Modify" or "Saving remove order" or "Undoing remove order" or "Redoing add order".
			if( InOper==-1 && Ar.IsLoading() )
			{
				Array->InsertZeroed( Index, Count, ElementSize );
				for( int32 i=Index; i<Index+Count; i++ )
				{
					DefaultConstructor( (uint8*)Array->GetData() + i*ElementSize );
				}
			}

			// Serialize changed items.
			check(Index+Count<=Array->Num());
			for( int32 i=Index; i<Index+Count; i++ )
			{
				Serializer( Ar, (uint8*)Array->GetData() + i*ElementSize );
			}
		}

		Ar.ArIgnoreOuterRef = bWasArIgnoreOuterRef;
	}
	else
	{
		//UE_LOG(LogEditorTransaction, Log,  TEXT("Object %s"), *Object->GetFullName());
		check(Index==0);
		check(ElementSize==0);
		check(DefaultConstructor==NULL);
		check(Serializer==NULL);
		SerializeObject( Ar );
	}
}

void FTransaction::FObjectRecord::SerializeObject( FArchive& Ar )
{
	check(!Array);

	UObject* CurrentObject = Object.Get();
	if (CurrentObject)
	{
		const bool bWasArIgnoreOuterRef = Ar.ArIgnoreOuterRef;
		if (Object.SubObjectHierarchyID.Num() != 0)
		{
			Ar.ArIgnoreOuterRef = true;
		}
		CurrentObject->Serialize(Ar);
		Ar.ArIgnoreOuterRef = bWasArIgnoreOuterRef;
	}
}

void FTransaction::FObjectRecord::Restore( FTransaction* Owner )
{
	// only used by FMatineeTransaction:
	if( !bRestored )
	{
		bRestored = true;
		check(!Owner->bFlip);
		check(!CustomChange.IsValid());

		FReader Reader( Owner, SerializedObject, bWantsBinarySerialization );
	
		SerializeContents( Reader, Oper );
	}
}

void FTransaction::FObjectRecord::Save(FTransaction* Owner)
{
	// if record has a custom change, no need to do anything here
	if( CustomChange.IsValid() )
	{
		return;
	}

	// common undo/redo path, before applying undo/redo buffer we save current state:
	check(Owner->bFlip);
	if (!bRestored)
	{
		SerializedObjectFlip.Reset();

		UObject* CurrentObject = Object.Get();
		if (CurrentObject)
		{
			SerializedObjectFlip.SetObject(CurrentObject);
		}

		FWriter Writer(SerializedObjectFlip, bWantsBinarySerialization);
		SerializeContents(Writer, -Oper);
	}
}

void FTransaction::FObjectRecord::Load(FTransaction* Owner)
{
	// common undo/redo path, we apply the saved state and then swap it for the state we cached in ::Save above
	check(Owner->bFlip);
	if (!bRestored)
	{
		bRestored = true;

		if (CustomChange.IsValid())
		{
			TUniquePtr<FChange> InvertedChange = CustomChange->Execute( Object.Get() );
			CustomChange = MoveTemp( InvertedChange );
		}
		else
		{
			// When objects are created outside the transaction system we can end up
			// finding them but not having any data for them, so don't serialize 
			// when that happens:
			if (SerializedObject.Data.Num() > 0)
			{
				FReader Reader(Owner, SerializedObject, bWantsBinarySerialization);
				SerializeContents(Reader, Oper);
			}
			SerializedObject.Swap(SerializedObjectFlip);
		}
		Oper *= -1;
	}
}

void FTransaction::FObjectRecord::Finalize( FTransaction* Owner, TSharedPtr<ITransactionObjectAnnotation>& OutFinalizedObjectAnnotation )
{
	OutFinalizedObjectAnnotation.Reset();

	if (Array)
	{
		// Can only diff objects
		return;
	}

	if (!bFinalized)
	{
		bFinalized = true;

		UObject* CurrentObject = Object.Get();
		if (CurrentObject)
		{
			// Serialize the object so we can diff it
			FSerializedObject CurrentSerializedObject;
			{
				CurrentSerializedObject.SetObject(CurrentObject);
				OutFinalizedObjectAnnotation = CurrentSerializedObject.ObjectAnnotation;
				FWriter Writer(CurrentSerializedObject, bWantsBinarySerialization);
				SerializeObject(Writer);
			}
			
			// Diff against the object state when the transaction started
			Diff(Owner, SerializedObject, CurrentSerializedObject, DeltaChange);

			// If we have a previous snapshot then we need to consider that part of the diff for the finalized object, as systems may 
			// have been tracking delta-changes between snapshots and this finalization will need to account for those changes too
			if (bSnapshot)
			{
				Diff(Owner, SerializedObjectSnapshot, CurrentSerializedObject, DeltaChange);
			}
		}

		// Clear out any snapshot data now as we won't be getting any more snapshot requests once finalized
		bSnapshot = false;
		SerializedObjectSnapshot.Reset();
	}
}

void FTransaction::FObjectRecord::Snapshot( FTransaction* Owner )
{
	if (Array)
	{
		// Can only diff objects
		return;
	}

	if (bFinalized)
	{
		// Cannot snapshot once finalized
		return;
	}

	UObject* CurrentObject = Object.Get();
	if (CurrentObject)
	{
		// Serialize the object so we can diff it
		FSerializedObject CurrentSerializedObject;
		{
			CurrentSerializedObject.SetObject(CurrentObject);
			FWriter Writer(CurrentSerializedObject, bWantsBinarySerialization);
			SerializeObject(Writer);
		}

		// Diff against the correct serialized data depending on whether we already had a snapshot
		const FSerializedObject& InitialSerializedObject = bSnapshot ? SerializedObjectSnapshot : SerializedObject;
		FTransactionObjectDeltaChange SnapshotDeltaChange;
		Diff(Owner, InitialSerializedObject, CurrentSerializedObject, SnapshotDeltaChange);

		// Update the snapshot data for next time
		bSnapshot = true;
		SerializedObjectSnapshot.Swap(CurrentSerializedObject);

		TSharedPtr<ITransactionObjectAnnotation> ChangedObjectTransactionAnnotation = SerializedObjectSnapshot.ObjectAnnotation;

		// Notify any listeners of this change
		if (SnapshotDeltaChange.HasChanged() || ChangedObjectTransactionAnnotation.IsValid())
		{
			CurrentObject->PostTransacted(FTransactionObjectEvent(Owner->GetId(), Owner->GetOperationId(), ETransactionObjectEventType::Snapshot, SnapshotDeltaChange, ChangedObjectTransactionAnnotation, InitialSerializedObject.ObjectName, InitialSerializedObject.ObjectPathName, InitialSerializedObject.ObjectOuterPathName));
		}
	}
}

void FTransaction::FObjectRecord::Diff( FTransaction* Owner, const FSerializedObject& OldSerializedObject, const FSerializedObject& NewSerializedObject, FTransactionObjectDeltaChange& OutDeltaChange )
{
	auto AreObjectPointersIdentical = [&OldSerializedObject, &NewSerializedObject](const FName InPropertyName)
	{
		TArray<int32, TInlineAllocator<8>> OldSerializedObjectIndices;
		OldSerializedObject.SerializedObjectIndices.MultiFind(InPropertyName, OldSerializedObjectIndices, true);

		TArray<int32, TInlineAllocator<8>> NewSerializedObjectIndices;
		NewSerializedObject.SerializedObjectIndices.MultiFind(InPropertyName, NewSerializedObjectIndices, true);

		bool bAreObjectPointersIdentical = OldSerializedObjectIndices.Num() == NewSerializedObjectIndices.Num();
		if (bAreObjectPointersIdentical)
		{
			for (int32 ObjIndex = 0; ObjIndex < OldSerializedObjectIndices.Num() && bAreObjectPointersIdentical; ++ObjIndex)
			{
				const UObject* OldObjectPtr = OldSerializedObject.ReferencedObjects.IsValidIndex(OldSerializedObjectIndices[ObjIndex]) ? OldSerializedObject.ReferencedObjects[OldSerializedObjectIndices[ObjIndex]].Get() : nullptr;
				const UObject* NewObjectPtr = NewSerializedObject.ReferencedObjects.IsValidIndex(NewSerializedObjectIndices[ObjIndex]) ? NewSerializedObject.ReferencedObjects[NewSerializedObjectIndices[ObjIndex]].Get() : nullptr;
				bAreObjectPointersIdentical = OldObjectPtr == NewObjectPtr;
			}
		}
		return bAreObjectPointersIdentical;
	};

	auto AreNamesIdentical = [&OldSerializedObject, &NewSerializedObject](const FName InPropertyName)
	{
		TArray<int32, TInlineAllocator<8>> OldSerializedNameIndices;
		OldSerializedObject.SerializedNameIndices.MultiFind(InPropertyName, OldSerializedNameIndices, true);

		TArray<int32, TInlineAllocator<8>> NewSerializedNameIndices;
		NewSerializedObject.SerializedNameIndices.MultiFind(InPropertyName, NewSerializedNameIndices, true);

		bool bAreNamesIdentical = OldSerializedNameIndices.Num() == NewSerializedNameIndices.Num();
		if (bAreNamesIdentical)
		{
			for (int32 ObjIndex = 0; ObjIndex < OldSerializedNameIndices.Num() && bAreNamesIdentical; ++ObjIndex)
			{
				const FName& OldName = OldSerializedObject.ReferencedNames.IsValidIndex(OldSerializedNameIndices[ObjIndex]) ? OldSerializedObject.ReferencedNames[OldSerializedNameIndices[ObjIndex]] : FName();
				const FName& NewName = NewSerializedObject.ReferencedNames.IsValidIndex(NewSerializedNameIndices[ObjIndex]) ? NewSerializedObject.ReferencedNames[NewSerializedNameIndices[ObjIndex]] : FName();
				bAreNamesIdentical = OldName == NewName;
			}
		}
		return bAreNamesIdentical;
	};

	OutDeltaChange.bHasNameChange |= OldSerializedObject.ObjectName != NewSerializedObject.ObjectName;
	OutDeltaChange.bHasOuterChange |= OldSerializedObject.ObjectOuterPathName != NewSerializedObject.ObjectOuterPathName;
	OutDeltaChange.bHasPendingKillChange |= OldSerializedObject.bIsPendingKill != NewSerializedObject.bIsPendingKill;

	if (!AreObjectPointersIdentical(NAME_None))
	{
		OutDeltaChange.bHasNonPropertyChanges = true;
	}

	if (!AreNamesIdentical(NAME_None))
	{
		OutDeltaChange.bHasNonPropertyChanges = true;
	}

	if (OldSerializedObject.SerializedProperties.Num() > 0 || NewSerializedObject.SerializedProperties.Num() > 0)
	{
		int32 StartOfOldPropertyBlock = INT_MAX;
		int32 StartOfNewPropertyBlock = INT_MAX;
		int32 EndOfOldPropertyBlock = -1;
		int32 EndOfNewPropertyBlock = -1;

		for (const TPair<FName, FSerializedProperty>& NewNamePropertyPair : NewSerializedObject.SerializedProperties)
		{
			const FSerializedProperty* OldSerializedProperty = OldSerializedObject.SerializedProperties.Find(NewNamePropertyPair.Key);
			if (!OldSerializedProperty)
			{
				// Missing property, assume that the property changed
				OutDeltaChange.ChangedProperties.AddUnique(NewNamePropertyPair.Key);
				continue;
			}

			// Update the tracking for the start/end of the property block within the serialized data
			StartOfOldPropertyBlock = FMath::Min(StartOfOldPropertyBlock, OldSerializedProperty->DataOffset);
			StartOfNewPropertyBlock = FMath::Min(StartOfNewPropertyBlock, NewNamePropertyPair.Value.DataOffset);
			EndOfOldPropertyBlock = FMath::Max(EndOfOldPropertyBlock, OldSerializedProperty->DataOffset + OldSerializedProperty->DataSize);
			EndOfNewPropertyBlock = FMath::Max(EndOfNewPropertyBlock, NewNamePropertyPair.Value.DataOffset + NewNamePropertyPair.Value.DataSize);

			// Binary compare the serialized data to see if something has changed for this property
			bool bIsPropertyIdentical = OldSerializedProperty->DataSize == NewNamePropertyPair.Value.DataSize;
			if (bIsPropertyIdentical && NewNamePropertyPair.Value.DataSize > 0)
			{
				bIsPropertyIdentical = FMemory::Memcmp(&OldSerializedObject.Data[OldSerializedProperty->DataOffset], &NewSerializedObject.Data[NewNamePropertyPair.Value.DataOffset], NewNamePropertyPair.Value.DataSize) == 0;
			}
			if (bIsPropertyIdentical)
			{
				bIsPropertyIdentical = AreObjectPointersIdentical(NewNamePropertyPair.Key);
			}
			if (bIsPropertyIdentical)
			{
				bIsPropertyIdentical = AreNamesIdentical(NewNamePropertyPair.Key);
			}

			if (!bIsPropertyIdentical)
			{
				OutDeltaChange.ChangedProperties.AddUnique(NewNamePropertyPair.Key);
			}
		}

		for (const TPair<FName, FSerializedProperty>& OldNamePropertyPair : OldSerializedObject.SerializedProperties)
		{
			const FSerializedProperty* NewSerializedProperty = NewSerializedObject.SerializedProperties.Find(OldNamePropertyPair.Key);
			if (!NewSerializedProperty)
			{
				// Missing property, assume that the property changed
				OutDeltaChange.ChangedProperties.AddUnique(OldNamePropertyPair.Key);
				continue;
			}
		}

		// Compare the data before the property block to see if something else in the object has changed
		if (!OutDeltaChange.bHasNonPropertyChanges)
		{
			const int32 OldHeaderSize = StartOfOldPropertyBlock;
			const int32 CurrentHeaderSize = StartOfNewPropertyBlock;

			bool bIsHeaderIdentical = OldHeaderSize == CurrentHeaderSize;
			if (bIsHeaderIdentical && CurrentHeaderSize > 0)
			{
				bIsHeaderIdentical = FMemory::Memcmp(&OldSerializedObject.Data[0], &NewSerializedObject.Data[0], CurrentHeaderSize) == 0;
			}

			if (!bIsHeaderIdentical)
			{
				OutDeltaChange.bHasNonPropertyChanges = true;
			}
		}

		// Compare the data after the property block to see if something else in the object has changed
		if (!OutDeltaChange.bHasNonPropertyChanges)
		{
			const int32 OldFooterSize = OldSerializedObject.Data.Num() - EndOfOldPropertyBlock;
			const int32 CurrentFooterSize = NewSerializedObject.Data.Num() - EndOfNewPropertyBlock;

			bool bIsFooterIdentical = OldFooterSize == CurrentFooterSize;
			if (bIsFooterIdentical && CurrentFooterSize > 0)
			{
				bIsFooterIdentical = FMemory::Memcmp(&OldSerializedObject.Data[EndOfOldPropertyBlock], &NewSerializedObject.Data[EndOfNewPropertyBlock], CurrentFooterSize) == 0;
			}

			if (!bIsFooterIdentical)
			{
				OutDeltaChange.bHasNonPropertyChanges = true;
			}
		}
	}
	else
	{
		// No properties, so just compare the whole blob
		bool bIsBlobIdentical = OldSerializedObject.Data.Num() == NewSerializedObject.Data.Num();
		if (bIsBlobIdentical && NewSerializedObject.Data.Num() > 0)
		{
			bIsBlobIdentical = FMemory::Memcmp(&OldSerializedObject.Data[0], &NewSerializedObject.Data[0], NewSerializedObject.Data.Num()) == 0;
		}

		if (!bIsBlobIdentical)
		{
			OutDeltaChange.bHasNonPropertyChanges = true;
		}
	}
}

int32 FTransaction::GetRecordCount() const
{
	return Records.Num();
}

bool FTransaction::ContainsPieObjects() const
{
	for( const FObjectRecord& Record : Records )
	{
		if( Record.ContainsPieObject() )
		{
			return true;
		}
	}

	return false;
}

bool FTransaction::IsObjectTransacting(const UObject* Object) const
{
	// This function is meaningless when called outside of a transaction context. Without this
	// ensure clients will commonly introduced bugs by having some logic that runs during
	// the transacting and some logic that does not, yielding assymetrical results.
	ensure(GIsTransacting);
	ensure(ChangedObjects.Num() != 0);
	return ChangedObjects.Contains(Object);
}

void FTransaction::RemoveRecords( int32 Count /* = 1  */ )
{
	if ( Count > 0 && Records.Num() >= Count )
	{
		// Remove anything from the ObjectMap which is about to be removed from the Records array
		for (int32 Index = 0; Index < Count; Index++)
		{
			ObjectMap.Remove( Records[Records.Num() - Count + Index].Object.Get() );
		}

		Records.RemoveAt( Records.Num() - Count, Count );
	}
}

/**
 * Outputs the contents of the ObjectMap to the specified output device.
 */
void FTransaction::DumpObjectMap(FOutputDevice& Ar) const
{
	Ar.Logf( TEXT("===== DumpObjectMap %s ==== "), *Title.ToString() );
	for ( auto It = ObjectMap.CreateConstIterator(); It; ++It )
	{
		const UObject* CurrentObject	= It.Key();
		const int32 SaveCount				= It.Value();
		Ar.Logf( TEXT("%i\t: %s"), SaveCount, *CurrentObject->GetPathName() );
	}
	Ar.Logf( TEXT("=== EndDumpObjectMap %s === "), *Title.ToString() );
}

FArchive& operator<<( FArchive& Ar, FTransaction::FObjectRecord& R )
{
	FMemMark Mark(FMemStack::Get());
	Ar << R.Object;
	Ar << R.SerializedObject.Data;
	Ar << R.SerializedObject.ReferencedObjects;
	Ar << R.SerializedObject.ReferencedNames;
	Mark.Pop();
	return Ar;
}

FTransaction::FObjectRecord::FPersistentObjectRef::FPersistentObjectRef(UObject* InObject)
	: ReferenceType(EReferenceType::Unknown)
	, Object(nullptr)
{
	check(InObject);
	UObject* Outermost = BuildSubobjectKey(InObject, SubObjectHierarchyID);

	if (SubObjectHierarchyID.Num()>0)
	{
		check(Outermost);
		//check(Outermost != GetTransientPackage());
		ReferenceType = EReferenceType::SubObject;
		Object = Outermost;
	}
	else
	{
		SubObjectHierarchyID.Empty();
		ReferenceType = EReferenceType::RootObject;
		Object = InObject;
	}

	// Make sure that when we look up the object we find the same thing:
	checkSlow(Get() == InObject);
}

UObject* FTransaction::FObjectRecord::FPersistentObjectRef::Get() const
{
	if (ReferenceType == EReferenceType::SubObject)
	{
		check (SubObjectHierarchyID.Num() > 0)
		// find the subobject:
		UObject* CurrentObject = Object;
		bool bFoundTargetSubObject = (SubObjectHierarchyID.Num() == 0);
		if (!bFoundTargetSubObject)
		{
			// Current increasing depth into sub-objects, starts at 1 to avoid the sub-object found and placed in NextObject.
			int SubObjectDepth = SubObjectHierarchyID.Num() - 1;
			UObject* NextObject = CurrentObject;
			while (NextObject != nullptr && !bFoundTargetSubObject)
			{
				// Look for any UObject with the CurrentObject's outer to find the next sub-object:
				NextObject = StaticFindObjectFast(UObject::StaticClass(), CurrentObject, SubObjectHierarchyID[SubObjectDepth]);
				bFoundTargetSubObject = SubObjectDepth == 0;
				--SubObjectDepth;
				CurrentObject = NextObject;
			}
		}

		return bFoundTargetSubObject ? CurrentObject : nullptr;
	}

	return Object;
}

void FTransaction::FObjectRecord::AddReferencedObjects( FReferenceCollector& Collector )
{
	UObject* Obj = Object.Object;
	Collector.AddReferencedObject(Obj);
	Object.Object = Obj;

	for (FPersistentObjectRef& ReferencedObject : SerializedObject.ReferencedObjects)
	{
		UObject* RefObj = ReferencedObject.Object;
		Collector.AddReferencedObject(RefObj);
		ReferencedObject.Object = RefObj;
	}

	if (SerializedObject.ObjectAnnotation.IsValid())
	{
		SerializedObject.ObjectAnnotation->AddReferencedObjects(Collector);
	}
}

bool FTransaction::FObjectRecord::ContainsPieObject() const
{
	{
		UObject* Obj = Object.Object;

		if(Obj && Obj->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			return true;
		}
	}

	for (const FPersistentObjectRef& ReferencedObject : SerializedObject.ReferencedObjects)
	{
		const UObject* Obj = ReferencedObject.Object;
		if( Obj && Obj->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			return true;
		}
	}

	return false;
}

void FTransaction::AddReferencedObjects( FReferenceCollector& Collector )
{
	for( FObjectRecord& ObjectRecord : Records )
	{
		ObjectRecord.AddReferencedObjects( Collector );
	}
	Collector.AddReferencedObjects(ObjectMap);
}

void FTransaction::SaveObject( UObject* Object )
{
	check(Object);
	Object->CheckDefaultSubobjects();

	int32* SaveCount = ObjectMap.Find(Object);
	if ( !SaveCount )
	{
		ObjectMap.Add(Object,1);
		// Save the object.
		new( Records )FObjectRecord( this, Object, nullptr, NULL, 0, 0, 0, 0, NULL, NULL, NULL );
	}
	else
	{
		++(*SaveCount);
	}
}

void FTransaction::SaveArray( UObject* Object, FScriptArray* Array, int32 Index, int32 Count, int32 Oper, int32 ElementSize, STRUCT_DC DefaultConstructor, STRUCT_AR Serializer, STRUCT_DTOR Destructor )
{
	check(Object);
	check(Array);
	check(ElementSize);
	check(DefaultConstructor);
	check(Serializer);
	check(Object->IsValidLowLevel());
	check((SIZE_T)Array>=(SIZE_T)Object);
	check((SIZE_T)Array+sizeof(FScriptArray)<=(SIZE_T)Object+Object->GetClass()->PropertiesSize);
	check(Index>=0);
	check(Count>=0);
	check(Index+Count<=Array->Num());

	// don't serialize the array if the object is contained within a PIE package
	if( Object->HasAnyFlags(RF_Transactional) && !Object->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
	{
		// Save the array.
		new( Records )FObjectRecord( this, Object, nullptr, Array, Index, Count, Oper, ElementSize, DefaultConstructor, Serializer, Destructor );
	}
}

void FTransaction::StoreUndo( UObject* Object, TUniquePtr<FChange> UndoChange )
{
	check( Object );
	Object->CheckDefaultSubobjects();

	int32* SaveCount = ObjectMap.Find( Object );
	if( !SaveCount )
	{
		ObjectMap.Add( Object, 0 );
	}

	// Save the undo record
	new( Records )FObjectRecord( this, Object, MoveTemp( UndoChange ), NULL, 0, 0, 0, 0, NULL, NULL, NULL );
}

void FTransaction::SetPrimaryObject(UObject* InObject)
{
	if (PrimaryObject == NULL)
	{
		PrimaryObject = InObject;
	}
}

void FTransaction::SnapshotObject( UObject* InObject )
{
	if (InObject && ObjectMap.Contains(InObject))
	{
		FObjectRecord* FoundObjectRecord = Records.FindByPredicate([InObject](const FObjectRecord& ObjRecord)
		{
			return ObjRecord.Object.Get() == InObject;
		});

		if (FoundObjectRecord)
		{
			FoundObjectRecord->Snapshot(this);
		}
	}
}

void FTransaction::BeginOperation()
{
	check(!OperationId.IsValid());
	OperationId = FGuid::NewGuid();
}

void FTransaction::EndOperation()
{
	check(OperationId.IsValid());
	OperationId.Invalidate();
}

void FTransaction::Apply()
{
	checkSlow(Inc==1||Inc==-1);

	// Figure out direction.
	const int32 Start = Inc==1 ? 0             : Records.Num()-1;
	const int32 End   = Inc==1 ? Records.Num() :              -1;

	// Init objects.
	for( int32 i=Start; i!=End; i+=Inc )
	{
		FObjectRecord& Record = Records[i];
		Record.bRestored = false;

		// Apply may be called before Finalize in order to revert an object back to its prior state in the case that a transaction is canceled
		// In this case we still need to generate a diff for the transaction so that we notify correctly
		if (!Record.bFinalized)
		{
			TSharedPtr<ITransactionObjectAnnotation> FinalizedObjectAnnotation;
			Record.Finalize(this, FinalizedObjectAnnotation);
		}

		UObject* Object = Record.Object.Get();
		if (Object)
		{
			if (!ChangedObjects.Contains(Object))
			{
				Object->CheckDefaultSubobjects();
				Object->PreEditUndo();
			}

			ChangedObjects.Add(Object, FChangedObjectValue(i, Record.SerializedObject.ObjectAnnotation));
		}
	}

	if (bFlip)
	{
		for (int32 i = Start; i != End; i += Inc)
		{
			Records[i].Save(this);
		}
		for (int32 i = Start; i != End; i += Inc)
		{
			Records[i].Load(this);
		}
	}
	else
	{
		for (int32 i = Start; i != End; i += Inc)
		{
			Records[i].Restore(this);
		}
	}

	// An Actor's components must always get its PostEditUndo before the owning Actor
	// so do a quick sort on Outer depth, component will deeper than their owner
	ChangedObjects.KeySort([](UObject& A, UObject& B)
	{
		auto GetObjectDepth = [](UObject* InObj)
		{
			int32 Depth = 0;
			for (UObject* Outer = InObj; Outer; Outer = Outer->GetOuter())
			{
				++Depth;
			}
			return Depth;
		};
		return GetObjectDepth(&A) > GetObjectDepth(&B);
	});

	TArray<ULevel*> LevelsToCommitModelSurface;
	for (auto ChangedObjectIt : ChangedObjects)
	{
		UObject* ChangedObject = ChangedObjectIt.Key;
		UModel* Model = Cast<UModel>(ChangedObject);
		if (Model && Model->Nodes.Num())
		{
			FBSPOps::bspBuildBounds(Model);
		}
		
		if (UModelComponent* ModelComponent = Cast<UModelComponent>(ChangedObject))
		{
			ULevel* Level = ModelComponent->GetTypedOuter<ULevel>();
			check(Level);
			LevelsToCommitModelSurface.AddUnique(Level);
		}

		TSharedPtr<ITransactionObjectAnnotation> ChangedObjectTransactionAnnotation = ChangedObjectIt.Value.Annotation;
		if (ChangedObjectTransactionAnnotation.IsValid())
		{
			ChangedObject->PostEditUndo(ChangedObjectTransactionAnnotation);
		}
		else
		{
			ChangedObject->PostEditUndo();
		}

		const FObjectRecord& ChangedObjectRecord = Records[ChangedObjectIt.Value.RecordIndex];
		const FTransactionObjectDeltaChange& DeltaChange = ChangedObjectRecord.DeltaChange;
		if (DeltaChange.HasChanged() || ChangedObjectTransactionAnnotation.IsValid())
		{
			const FObjectRecord::FSerializedObject& InitialSerializedObject = ChangedObjectRecord.SerializedObject;
			ChangedObject->PostTransacted(FTransactionObjectEvent(Id, OperationId, ETransactionObjectEventType::UndoRedo, DeltaChange, ChangedObjectTransactionAnnotation, InitialSerializedObject.ObjectName, InitialSerializedObject.ObjectPathName, InitialSerializedObject.ObjectOuterPathName));
		}
	}

	// Commit model surfaces for unique levels within the transaction
	for (ULevel* Level : LevelsToCommitModelSurface)
	{
		Level->CommitModelSurfaces();
	}

	// Flip it.
	if (bFlip)
	{
		Inc *= -1;
	}
	for (auto ChangedObjectIt : ChangedObjects)
	{
		UObject* ChangedObject = ChangedObjectIt.Key;
		ChangedObject->CheckDefaultSubobjects();
	}

	ChangedObjects.Reset();
}

void FTransaction::Finalize()
{
	for (int32 i = 0; i < Records.Num(); ++i)
	{
		TSharedPtr<ITransactionObjectAnnotation> FinalizedObjectAnnotation;

		FObjectRecord& ObjectRecord = Records[i];
		ObjectRecord.Finalize(this, FinalizedObjectAnnotation);

		UObject* Object = ObjectRecord.Object.Get();
		if (Object)
		{
			if (!ChangedObjects.Contains(Object))
			{
				ChangedObjects.Add(Object, FChangedObjectValue(i, FinalizedObjectAnnotation));
			}
		}
	}

	// An Actor's components must always be notified before the owning Actor
	// so do a quick sort on Outer depth, component will deeper than their owner
	ChangedObjects.KeySort([](UObject& A, UObject& B)
	{
		auto GetObjectDepth = [](UObject* InObj)
		{
			int32 Depth = 0;
			for (UObject* Outer = InObj; Outer; Outer = Outer->GetOuter())
			{
				++Depth;
			}
			return Depth;
		};
		return GetObjectDepth(&A) > GetObjectDepth(&B);
	});

	for (auto ChangedObjectIt : ChangedObjects)
	{
		TSharedPtr<ITransactionObjectAnnotation> ChangedObjectTransactionAnnotation = ChangedObjectIt.Value.Annotation;

		const FObjectRecord& ChangedObjectRecord = Records[ChangedObjectIt.Value.RecordIndex];
		const FTransactionObjectDeltaChange& DeltaChange = ChangedObjectRecord.DeltaChange;
		if (DeltaChange.HasChanged() || ChangedObjectTransactionAnnotation.IsValid())
		{
			UObject* ChangedObject = ChangedObjectIt.Key;

			const FObjectRecord::FSerializedObject& InitialSerializedObject = ChangedObjectRecord.SerializedObject;
			ChangedObject->PostTransacted(FTransactionObjectEvent(Id, OperationId, ETransactionObjectEventType::Finalized, DeltaChange, ChangedObjectTransactionAnnotation, InitialSerializedObject.ObjectName, InitialSerializedObject.ObjectPathName, InitialSerializedObject.ObjectOuterPathName));
		}
	}

	ChangedObjects.Reset();
}

SIZE_T FTransaction::DataSize() const
{
	SIZE_T Result=0;
	for( int32 i=0; i<Records.Num(); i++ )
	{
		Result += Records[i].SerializedObject.Data.Num();
	}
	return Result;
}

/**
 * Get all the objects that are part of this transaction.
 * @param	Objects		[out] Receives the object list.  Previous contents are cleared.
 */
void FTransaction::GetTransactionObjects(TArray<UObject*>& Objects) const
{
	Objects.Empty(); // Just in case.

	for(int32 i=0; i<Records.Num(); i++)
	{
		UObject* Obj = Records[i].Object.Get();
		if (Obj)
		{
			Objects.AddUnique(Obj);
		}
	}
}


/*-----------------------------------------------------------------------------
	Transaction tracking system.
-----------------------------------------------------------------------------*/
UTransactor::UTransactor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTransBuffer::Initialize(SIZE_T InMaxMemory)
{
	MaxMemory = InMaxMemory;
	// Reset.
	Reset( NSLOCTEXT("UnrealEd", "Startup", "Startup") );
	CheckState();

	UE_LOG(LogInit, Log, TEXT("Transaction tracking system initialized") );
}

// UObject interface.
void UTransBuffer::Serialize( FArchive& Ar )
{
	check( !Ar.IsPersistent() );

	CheckState();

	Super::Serialize( Ar );

	if ( IsObjectSerializationEnabled() || !Ar.IsObjectReferenceCollector() )
	{
		Ar << UndoBuffer;
	}
	Ar << ResetReason << UndoCount << ActiveCount << ActiveRecordCounts;

	CheckState();
}

void UTransBuffer::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		CheckState();
		UE_LOG(LogExit, Log, TEXT("Transaction tracking system shut down") );
	}
	Super::FinishDestroy();
}

void UTransBuffer::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UTransBuffer* This = CastChecked<UTransBuffer>(InThis);
	This->CheckState();

	if ( This->IsObjectSerializationEnabled() )
	{
		// We cannot support undoing across GC if we allow it to eliminate references so we need
		// to suppress it.
		Collector.AllowEliminatingReferences(false);
		for (const TSharedRef<FTransaction>& SharedTrans : This->UndoBuffer)
		{
			SharedTrans->AddReferencedObjects( Collector );
		}
		for (const TSharedRef<FTransaction>& SharedTrans : This->RemovedTransactions)
		{
			SharedTrans->AddReferencedObjects(Collector);
		}
		Collector.AllowEliminatingReferences(true);
	}

	This->CheckState();

	Super::AddReferencedObjects( This, Collector );
}

int32 UTransBuffer::Begin( const TCHAR* SessionContext, const FText& Description )
{
	return BeginInternal<FTransaction>(SessionContext, Description);
}


int32 UTransBuffer::End()
{
	CheckState();
	const int32 Result = ActiveCount;
	// Don't assert as we now purge the buffer when resetting.
	// So, the active count could be 0, but the code path may still call end.
	if (ActiveCount >= 1)
	{
		if( --ActiveCount==0 )
		{
#if 0 // @todo DB: please don't remove this code -- thanks! :)
			// End the current transaction.
			if ( GUndo && GLog )
			{
				// @todo DB: Fix this potentially unsafe downcast.
				static_cast<FTransaction*>(GUndo)->DumpObjectMap( *GLog );
			}
#endif
			if (GUndo)
			{
				GUndo->Finalize();
				TransactionStateChangedDelegate.Broadcast(GUndo->GetContext(), ETransactionStateEventType::TransactionFinalized);
				GUndo->EndOperation();

				// PIE objects now generate transactions.
				// Once the transaction is finalized however, they aren't kept in the undo buffer.
				if (GUndo->ContainsPieObjects())
				{
					check(UndoCount == 0);
					UndoBuffer.Pop(false);
					UndoBufferChangedDelegate.Broadcast();
				}
			}
			GUndo = nullptr;
			PreviousUndoCount = INDEX_NONE;
			RemovedTransactions.Reset();
		}
		ActiveRecordCounts.Pop();
		CheckState();
	}
	return Result;
}


void UTransBuffer::Reset( const FText& Reason )
{
	if (ensure(!GIsTransacting))
	{
		CheckState();

		if (ActiveCount != 0)
		{
			FString ErrorMessage = TEXT("");
			ErrorMessage += FString::Printf(TEXT("Non zero active count in UTransBuffer::Reset") LINE_TERMINATOR);
			ErrorMessage += FString::Printf(TEXT("ActiveCount : %d") LINE_TERMINATOR, ActiveCount);
			ErrorMessage += FString::Printf(TEXT("SessionName : %s") LINE_TERMINATOR, *GetUndoContext(false).Context);
			ErrorMessage += FString::Printf(TEXT("Reason      : %s") LINE_TERMINATOR, *Reason.ToString());

			ErrorMessage += FString::Printf(LINE_TERMINATOR);
			ErrorMessage += FString::Printf(TEXT("Purging the undo buffer...") LINE_TERMINATOR);

			UE_LOG(LogEditorTransaction, Log, TEXT("%s"), *ErrorMessage);


			// Clear out the transaction buffer...
			Cancel(0);
		}

		// Reset all transactions.
		UndoBuffer.Empty();
		UndoCount = 0;
		ResetReason = Reason;
		ActiveCount = 0;
		ActiveRecordCounts.Empty();
		UndoBufferChangedDelegate.Broadcast();

		CheckState();
	}
}


void UTransBuffer::Cancel( int32 StartIndex /*=0*/ )
{
	CheckState();

	// if we don't have any active actions, we shouldn't have an active transaction at all
	if ( ActiveCount > 0 )
	{
		if ( StartIndex == 0 )
		{
			if (GUndo)
			{
				TransactionStateChangedDelegate.Broadcast(GUndo->GetContext(), ETransactionStateEventType::TransactionCanceled);
				GUndo->EndOperation();
			}

			// clear the global pointer to the soon-to-be-deleted transaction
			GUndo = nullptr;
			
			UndoBuffer.Pop(false);
			UndoBuffer.Reserve(UndoBuffer.Num() + RemovedTransactions.Num());

			if (PreviousUndoCount > 0)
			{
				UndoBuffer.Append(RemovedTransactions);
			}
			else
			{
				UndoBuffer.Insert(RemovedTransactions, 0);
			}

			RemovedTransactions.Reset();

			UndoCount = PreviousUndoCount;
			PreviousUndoCount = INDEX_NONE;
			UndoBufferChangedDelegate.Broadcast();
		}
		else
		{
			int32 RecordsToKeep = 0;
			for (int32 ActiveIndex = 0; ActiveIndex <= StartIndex; ++ActiveIndex)
			{
				RecordsToKeep += ActiveRecordCounts[ActiveIndex];
			}

			FTransaction& Transaction = UndoBuffer.Last().Get();
			Transaction.RemoveRecords(Transaction.GetRecordCount() - RecordsToKeep);
		}

		// reset the active count
		ActiveCount = StartIndex;
		ActiveRecordCounts.SetNum(StartIndex);
	}

	CheckState();
}


bool UTransBuffer::CanUndo( FText* Text )
{
	CheckState();
	if( ActiveCount )
	{
		if( Text )
		{
			*Text = NSLOCTEXT("TransactionSystem", "CantUndoDuringTransaction", "(Can't undo while action is in progress)");
		}
		return false;
	}
	
	if (UndoBarrierStack.Num())
	{
		const int32 UndoBarrier = UndoBarrierStack.Last();
		if (UndoBuffer.Num() - UndoCount <= UndoBarrier)
		{
			if (Text)
			{
				*Text = NSLOCTEXT("TransactionSystem", "HitUndoBarrier", "(Hit Undo barrier; can't undo any further)");
			}
			return false;
		}
	}

	if( UndoBuffer.Num()==UndoCount )
	{
		if( Text )
		{
			*Text = FText::Format( NSLOCTEXT("TransactionSystem", "CantUndoAfter", "(Can't undo after: {0})"), ResetReason );
		}
		return false;
	}
	return true;
}


bool UTransBuffer::CanRedo( FText* Text )
{
	CheckState();
	if( ActiveCount )
	{
		if( Text )
		{
			*Text = NSLOCTEXT("TransactionSystem", "CantRedoDuringTransaction", "(Can't redo while action is in progress)");
		}
		return 0;
	}
	if( UndoCount==0 )
	{
		if( Text )
		{
			*Text = NSLOCTEXT("TransactionSystem", "NothingToRedo", "(Nothing to redo)");
		}
		return 0;
	}
	return 1;
}


const FTransaction* UTransBuffer::GetTransaction( int32 QueueIndex ) const
{
	if (UndoBuffer.Num() > QueueIndex && QueueIndex != INDEX_NONE)
	{
		return &UndoBuffer[QueueIndex].Get();
	}

	return NULL;
}


FTransactionContext UTransBuffer::GetUndoContext( bool bCheckWhetherUndoPossible )
{
	FTransactionContext Context;
	FText Title;
	if( bCheckWhetherUndoPossible && !CanUndo( &Title ) )
	{
		Context.Title = Title;
		return Context;
	}

	TSharedRef<FTransaction>& Transaction = UndoBuffer[ UndoBuffer.Num() - (UndoCount + 1) ];
	return Transaction->GetContext();
}


FTransactionContext UTransBuffer::GetRedoContext()
{
	FTransactionContext Context;
	FText Title;
	if( !CanRedo( &Title ) )
	{
		Context.Title = Title;
		return Context;
	}

	TSharedRef<FTransaction>& Transaction = UndoBuffer[ UndoBuffer.Num() - UndoCount ];
	return Transaction->GetContext();
}


void UTransBuffer::SetUndoBarrier()
{
	UndoBarrierStack.Push(UndoBuffer.Num() - UndoCount);
}


void UTransBuffer::RemoveUndoBarrier()
{
	if (UndoBarrierStack.Num() > 0)
	{
		UndoBarrierStack.Pop();
	}
}


void UTransBuffer::ClearUndoBarriers()
{
	UndoBarrierStack.Empty();
}


bool UTransBuffer::Undo(bool bCanRedo)
{
	CheckState();

	if (!CanUndo())
	{
		UndoDelegate.Broadcast(FTransactionContext(), false);

		return false;
	}

	// Apply the undo changes.
	GIsTransacting = true;
	{
		FTransaction& Transaction = UndoBuffer[ UndoBuffer.Num() - ++UndoCount ].Get();
		UE_LOG(LogEditorTransaction, Log,  TEXT("Undo %s"), *Transaction.GetTitle().ToString() );
		CurrentTransaction = &Transaction;
		CurrentTransaction->BeginOperation();

		const FTransactionContext TransactionContext = CurrentTransaction->GetContext();
		TransactionStateChangedDelegate.Broadcast(TransactionContext, ETransactionStateEventType::UndoRedoStarted);
		BeforeRedoUndoDelegate.Broadcast(TransactionContext);
		Transaction.Apply();
		UndoDelegate.Broadcast(TransactionContext, true);
		TransactionStateChangedDelegate.Broadcast(TransactionContext, ETransactionStateEventType::UndoRedoFinalized);

		CurrentTransaction->EndOperation();
		CurrentTransaction = nullptr;

		if (!bCanRedo)
		{
			UndoBuffer.RemoveAt(UndoBuffer.Num() - UndoCount, UndoCount);
			UndoCount = 0;

			UndoBufferChangedDelegate.Broadcast();
		}
	}
	GIsTransacting = false;

	CheckState();

	return true;
}

bool UTransBuffer::Redo()
{
	CheckState();

	if (!CanRedo())
	{
		RedoDelegate.Broadcast(FTransactionContext(), false);

		return false;
	}

	// Apply the redo changes.
	GIsTransacting = true;
	{
		FTransaction& Transaction = UndoBuffer[ UndoBuffer.Num() - UndoCount-- ].Get();
		UE_LOG(LogEditorTransaction, Log,  TEXT("Redo %s"), *Transaction.GetTitle().ToString() );
		CurrentTransaction = &Transaction;
		CurrentTransaction->BeginOperation();

		const FTransactionContext TransactionContext = CurrentTransaction->GetContext();
		TransactionStateChangedDelegate.Broadcast(TransactionContext, ETransactionStateEventType::UndoRedoStarted);
		BeforeRedoUndoDelegate.Broadcast(TransactionContext);
		Transaction.Apply();
		RedoDelegate.Broadcast(TransactionContext, true);
		TransactionStateChangedDelegate.Broadcast(TransactionContext, ETransactionStateEventType::UndoRedoFinalized);

		CurrentTransaction->EndOperation();
		CurrentTransaction = nullptr;
	}
	GIsTransacting = false;

	CheckState();

	return true;
}

bool UTransBuffer::EnableObjectSerialization()
{
	return --DisallowObjectSerialization == 0;
}

bool UTransBuffer::DisableObjectSerialization()
{
	return ++DisallowObjectSerialization == 0;
}


SIZE_T UTransBuffer::GetUndoSize() const
{
	SIZE_T Result=0;
	for( int32 i=0; i<UndoBuffer.Num(); i++ )
	{
		Result += UndoBuffer[i]->DataSize();
	}
	return Result;
}


void UTransBuffer::CheckState() const
{
	// Validate the internal state.
	check(UndoBuffer.Num()>=UndoCount);
	check(ActiveCount>=0);
	check(ActiveRecordCounts.Num() == ActiveCount);
}


void UTransBuffer::SetPrimaryUndoObject(UObject* PrimaryObject)
{
	// Only record the primary object if its transactional, not in any of the temporary packages and theres an active transaction
	if ( PrimaryObject && PrimaryObject->HasAnyFlags( RF_Transactional ) &&
		(PrimaryObject->GetOutermost()->HasAnyPackageFlags( PKG_PlayInEditor|PKG_ContainsScript|PKG_CompiledIn ) == false) )
	{
		const int32 NumTransactions = UndoBuffer.Num();
		const int32 CurrentTransactionIdx = NumTransactions - (UndoCount + 1);

		if ( CurrentTransactionIdx >= 0 )
		{
			TSharedRef<FTransaction>& Transaction = UndoBuffer[ CurrentTransactionIdx ];
			Transaction->SetPrimaryObject(PrimaryObject);
		}
	}
}

bool UTransBuffer::IsObjectInTransationBuffer( const UObject* Object ) const
{
	TArray<UObject*> TransactionObjects;
	for( const TSharedRef<FTransaction>& Transaction : UndoBuffer )
	{
		Transaction->GetTransactionObjects(TransactionObjects);

		if( TransactionObjects.Contains(Object) )
		{
			return true;
		}
		
		TransactionObjects.Reset();
	}

	return false;
}

bool UTransBuffer::IsObjectTransacting(const UObject* Object) const
{
	// We can't provide a truly meaningful answer to this question when not transacting:
	if (ensure(CurrentTransaction))
	{
		return CurrentTransaction->IsObjectTransacting(Object);
	}

	return false;
}

bool UTransBuffer::ContainsPieObjects() const
{
	for( const TSharedRef<FTransaction>& Transaction : UndoBuffer )
	{
		if( Transaction->ContainsPieObjects() )
		{
			return true;
		}
	}

	return false;
}
