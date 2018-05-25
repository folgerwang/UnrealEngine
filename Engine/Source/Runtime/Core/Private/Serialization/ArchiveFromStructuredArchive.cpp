// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Serialization/ArchiveFromStructuredArchive.h"
#include "Internationalization/Text.h"

FArchiveFromStructuredArchive::FArchiveFromStructuredArchive(FStructuredArchive::FSlot Slot)
	: FArchiveProxy(Slot.GetUnderlyingArchive())
	, bPendingSerialize(true)
	, bWasOpened(false)
	, Pos(0)
	, RootSlot(Slot)
{
	// For some reason, the FArchive copy constructor will copy all the trivial members of the source archive, but then specifically set ArIsFilterEditorOnly to false, with a comment saying
	// they don't know why it's doing this... make sure we inherit this flag here!
	ArIsFilterEditorOnly = InnerArchive.ArIsFilterEditorOnly;
	SetIsTextFormat(false);
}

FArchiveFromStructuredArchive::~FArchiveFromStructuredArchive()
{
	Commit();
}

void FArchiveFromStructuredArchive::Flush()
{
	Commit();
	FArchive::Flush();
}

bool FArchiveFromStructuredArchive::Close()
{
	Commit();
	return FArchive::Close();
}

int64 FArchiveFromStructuredArchive::Tell()
{
	if (InnerArchive.IsTextFormat())
	{
		return Pos;
	}
	else
	{
		return InnerArchive.Tell();
	}
}

int64 FArchiveFromStructuredArchive::TotalSize()
{
	checkf(false, TEXT("FArchiveFromStructuredArchive does not support TotalSize()"));
	return FArchive::TotalSize();
}

void FArchiveFromStructuredArchive::Seek(int64 InPos)
{
	if (InnerArchive.IsTextFormat())
	{
		check(Pos >= 0 && Pos <= Buffer.Num());
		Pos = InPos;
	}
	else
	{
		InnerArchive.Seek(InPos);
	}
}

bool FArchiveFromStructuredArchive::AtEnd()
{
	if (InnerArchive.IsTextFormat())
	{
		return Pos == Buffer.Num();
	}
	else
	{
		return InnerArchive.AtEnd();
	}
}

FArchive& FArchiveFromStructuredArchive::operator<<(class FName& Value)
{
	OpenArchive();

	if (InnerArchive.IsTextFormat())
	{
		if (IsLoading())
		{
			int32 NameIdx = 0;
			Serialize(&NameIdx, sizeof(NameIdx));
			Value = Names[NameIdx];
		}
		else
		{
			int32* NameIdxPtr = NameToIndex.Find(Value);
			if (NameIdxPtr == nullptr)
			{
				NameIdxPtr = &(NameToIndex.Add(Value));
				*NameIdxPtr = Names.Add(Value);
			}
			Serialize(NameIdxPtr, sizeof(*NameIdxPtr));
		}
	}
	else
	{
		InnerArchive << Value;
	}
	return *this;
}

FArchive& FArchiveFromStructuredArchive::operator<<(class UObject*& Value)
{
	OpenArchive();

	if(InnerArchive.IsTextFormat())
	{
		if (IsLoading())
		{
			int32 ObjectIdx = 0;
			Serialize(&ObjectIdx, sizeof(ObjectIdx));

			// If this object has already been accessed, return the cached value
			if (ObjectsValid[ObjectIdx])
			{
				Value = Objects[ObjectIdx];
			}
			else
			{
				FStructuredArchive::FStream Stream = Root->EnterStream(FIELD_NAME_TEXT("Objects"));

				// We know exactly which stream index we want to load here, but because of the API we need to read through them
				// in order, consuming the string name until we reach the entry we want and then load it as a uobject reference.
				// If we are loading from a text archive, we could easily specify here which index we want, and the internal formatter
				// can just push that single value by itself onto the value stack, but that same API couldn't be implemented for a
				// binary archive as we can't skip over entries because we don't know how big they are. Maybe we could specify a stride
				// or something, but at this point the API is complex and pretty formatter specific. Thought required!
				// For now, just consume all the string names of the objects up until the one we need, then load that as an object
				// pointer.

				FString Dummy;
				for (int32 Index = 0; Index < Objects.Num(); ++Index)
				{
					if (Index == ObjectIdx)
					{
						Stream.EnterElement() << Value;
						Objects[ObjectIdx] = Value;
					}
					else
					{
						Stream.EnterElement() << Dummy;
					}
				}

				Objects[ObjectIdx] = Value;
				ObjectsValid[ObjectIdx] = true;
			}
		}
		else
		{
			int32* ObjectIdxPtr = ObjectToIndex.Find(Value);
			if (ObjectIdxPtr == nullptr)
			{
				ObjectIdxPtr = &(ObjectToIndex.Add(Value));
				*ObjectIdxPtr = Objects.Add(Value);
			}
			Serialize(ObjectIdxPtr, sizeof(*ObjectIdxPtr));
		}
	}
	else
	{
		InnerArchive << Value;
	}
	return *this;
}

FArchive& FArchiveFromStructuredArchive::operator<<(class FText& Value)
{
	OpenArchive();

	if (InnerArchive.IsTextFormat())
	{
		FText::SerializeText(*this, Value);
	}
	else
	{
		InnerArchive << Value;
	}
	return *this;
}

void FArchiveFromStructuredArchive::Serialize(void* V, int64 Length)
{
	OpenArchive();

	if (InnerArchive.IsTextFormat())
	{
		if (IsLoading())
		{
			if (Pos + Length > Buffer.Num())
			{
				checkf(false, TEXT("Attempt to read past end of archive"));
			}
			FMemory::Memcpy(V, Buffer.GetData() + Pos, Length);
			Pos += Length;
		}
		else
		{
			if (Pos + Length > Buffer.Num())
			{
				Buffer.AddUninitialized(Pos + Length - Buffer.Num());
			}
			FMemory::Memcpy(Buffer.GetData() + Pos, V, Length);
			Pos += Length;
		}
	}
	else
	{
		InnerArchive.Serialize(V, Length);
	}
}

void FArchiveFromStructuredArchive::Commit()
{
	if (bWasOpened && InnerArchive.IsTextFormat())
	{
		SerializeInternal(Root.GetValue());
	}
}

void FArchiveFromStructuredArchive::SerializeInternal(FStructuredArchive::FRecord Record)
{
	check(bWasOpened);

	if (bPendingSerialize)
	{
		FStructuredArchive::FSlot DataSlot = Record.EnterField(FIELD_NAME_TEXT("Data"));
		DataSlot.Serialize(Buffer);

		TOptional<FStructuredArchive::FSlot> ObjectsSlot = Record.TryEnterField(FIELD_NAME_TEXT("Objects"), Objects.Num() > 0);
		if (ObjectsSlot.IsSet())
		{
			if (IsLoading())
			{
				// We don't want to load all the referenced objects here, as this causes all sorts of 
				// dependency issues. The legacy archive would load any referenced objects at the point
				// that their pointer was serialized by the owning export. For now, we just need to
				// know how many objects there are so we can pre-size our arrays
				// NOTE: The json formatter will push all the values in the array onto the value stack
				// when we enter the array here. We never read them, so I'm assuming they just sit there
				// until we destroy this archive wrapper. Perhaps we need something in the API here to just access
				// the size of the array but not preparing to access it's values?
				ObjectsSlot.GetValue() << ObjectNames;
				//int32 NumEntries = 0;
				//ObjectsSlot.GetValue().EnterArray(NumEntries);
				Objects.AddUninitialized(ObjectNames.Num());
				ObjectsValid.Init(false, ObjectNames.Num());
			}
			else
			{
				ObjectsSlot.GetValue() << Objects;
			}
		}

		TOptional<FStructuredArchive::FSlot> NamesSlot = Record.TryEnterField(FIELD_NAME_TEXT("Names"), Names.Num() > 0);
		if (NamesSlot.IsSet())
		{
			NamesSlot.GetValue() << Names;
		}

		bPendingSerialize = false;
	}
}

void FArchiveFromStructuredArchive::OpenArchive()
{
	if (!bWasOpened)
	{
		bWasOpened = true;

		if (InnerArchive.IsTextFormat())
		{
			Root = RootSlot.EnterRecord();

			if (IsLoading())
			{
				SerializeInternal(Root.GetValue());
			}
		}
		else
		{
			RootSlot.EnterStream();
		}
	}
}