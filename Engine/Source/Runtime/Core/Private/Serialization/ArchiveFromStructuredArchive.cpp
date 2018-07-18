// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Serialization/ArchiveFromStructuredArchive.h"
#include "Internationalization/Text.h"

FArchiveFromStructuredArchive::FArchiveFromStructuredArchive(FStructuredArchive::FSlot Slot)
	: FArchiveProxy(Slot.GetUnderlyingArchive())
	, bPendingSerialize(true)
	, Pos(0)
{
	// For some reason, the FArchive copy constructor will copy all the trivial members of the source archive, but then specifically set ArIsFilterEditorOnly to false, with a comment saying
	// they don't know why it's doing this... make sure we inherit this flag here!
	ArIsFilterEditorOnly = InnerArchive.ArIsFilterEditorOnly;
	
	if (IsTextFormat())
	{
		Record = Slot.EnterRecord();
		if (IsLoading())
		{
			SerializeInternal();
		}
	}
	else
	{
		Slot.EnterStream();
	}
}

FArchiveFromStructuredArchive::~FArchiveFromStructuredArchive()
{
	SerializeInternal();
}

void FArchiveFromStructuredArchive::Flush()
{
	SerializeInternal();
	FArchive::Flush();
}

bool FArchiveFromStructuredArchive::Close()
{
	SerializeInternal();
	return FArchive::Close();
}

int64 FArchiveFromStructuredArchive::Tell()
{
	if (IsTextFormat())
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
	if (IsTextFormat())
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
	if (IsTextFormat())
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
	if (IsTextFormat())
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
	if(IsTextFormat())
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
				TOptional<FStructuredArchive::FSlot> ObjectsSlot = Record->TryEnterField(FIELD_NAME_TEXT("Objects"), false);
				if (ObjectsSlot.IsSet())
				{
					// We know exactly which stream index we want to load here, but because of the API we need to read through them
					// in order, consuming the string name until we reach the entry we want and then load it as a uobject reference.
					// If we are loading from a text archive, we could easily specify here which index we want, and the internal formatter
					// can just push that single value by itself onto the value stack, but that same API couldn't be implemented for a
					// binary archive as we can't skip over entries because we don't know how big they are. Maybe we could specify a stride
					// or something, but at this point the API is complex and pretty formatter specific. Thought required!
					// For now, just consume all the string names of the objects up until the one we need, then load that as an object
					// pointer.
					FStructuredArchive::FStream Stream = ObjectsSlot->EnterStream();
					for (int32 Index = 0; Index <= ObjectIdx; ++Index)
					{
						if (Index == ObjectIdx)
						{
							Stream.EnterElement() << Value;
							Objects[ObjectIdx] = Value;
						}
						else
						{
							FString Dummy;
							Stream.EnterElement() << Dummy;
						}
					}
				}
				else
				{
					Value = nullptr;
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
	if (IsTextFormat())
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
	if (IsTextFormat())
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

void FArchiveFromStructuredArchive::SerializeInternal()
{
	if (bPendingSerialize && IsTextFormat())
	{
		FStructuredArchive::FSlot DataSlot = Record.GetValue().EnterField(FIELD_NAME_TEXT("Data"));
		DataSlot.Serialize(Buffer);

		TOptional<FStructuredArchive::FSlot> ObjectsSlot = Record.GetValue().TryEnterField(FIELD_NAME_TEXT("Objects"), Objects.Num() > 0);
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
				int32 NumEntries = 0;
				ObjectsSlot.GetValue().EnterArray(NumEntries);
				Objects.AddUninitialized(NumEntries);
				ObjectsValid.Init(false, NumEntries);
			}
			else
			{
				ObjectsSlot.GetValue() << Objects;
			}
		}

		TOptional<FStructuredArchive::FSlot> NamesSlot = Record.GetValue().TryEnterField(FIELD_NAME_TEXT("Names"), Names.Num() > 0);
		if (NamesSlot.IsSet())
		{
			NamesSlot.GetValue() << Names;
		}

		bPendingSerialize = false;
	}
}
