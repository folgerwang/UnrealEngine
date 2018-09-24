#pragma once

#include "Runtime/Renderer/Public/VirtualTexturing.h"
#include "VirtualTextureTypes.h"
/*
class FChunkCache
{
public:
	struct FEntry
	{
		FEntry* Next = nullptr;
		FEntry* Previous = nullptr;

		ChunkID Id = INVALID_CHUNK_ID;
		size_t Size = 0;
		uint8* Memory = 0;
		FThreadSafeCounter MappedRefCount;
		FGraphEventRef CompletionEvent;
		FChunkCache* Parent = nullptr;

		void Reset(bool keepMemory = false)
		{
			ensure(MappedRefCount.GetValue() == 0);
			MappedRefCount.Reset();

 			ensure(CompletionEvent == nullptr || CompletionEvent->IsComplete() == true);
 			CompletionEvent = nullptr;

			if (keepMemory == false)
			{
				FMemory::Free(Memory);
				Memory = nullptr;
			}
			Size = 0;

			Id = INVALID_CHUNK_ID;

			ensure(Next == nullptr && Previous == nullptr);
		}

		void Init(size_t InSize, bool reuseMemory = false)
		{
			if (reuseMemory == false)
			{
				ensure(Memory == nullptr);
				Memory = (uint8*)FMemory::Malloc(InSize);
			}
			check(Memory);
			check(FMemory::GetAllocSize(Memory) >= InSize);
			Size = InSize;
		}

		void AddRef()
		{
			ensure(Next == nullptr && Previous == nullptr);
			MappedRefCount.Increment();
		}

		void Release()
		{
			ensure(Next == nullptr && Previous == nullptr);
			if (MappedRefCount.Decrement() <= 0)
			{
				Parent->Unmap(this);
			}
		}
	};

	struct FEntryList
	{
		FEntry* HeadNode = nullptr;
		FEntry* TailNode = nullptr;
		int32 ListSize = 0;

		void AddHead(FEntry* Entry)
		{
			check(Entry);
			if (HeadNode != nullptr)
			{
				Entry->Next = HeadNode;
				HeadNode->Previous = Entry;
				HeadNode = Entry;
			}
			else
			{
				HeadNode = TailNode = Entry;
			}
			ListSize++;
		}

		void AddTail(FEntry* Entry)
		{
			check(Entry);
			if (TailNode != nullptr)
			{
				TailNode->Next = Entry;
				Entry->Previous = TailNode;
				TailNode = Entry;
			}
			else
			{
				HeadNode = TailNode = Entry;
			}
			ListSize++;
		}

		void Remove(FEntry* Entry)
		{
			check(Entry);

			if (ListSize == 1)
			{
				checkSlow(Entry == HeadNode);
				HeadNode = TailNode = nullptr;
				ListSize = 0;
				return;
			}

			if (Entry == HeadNode)
			{
				ensure(HeadNode->Next);
				HeadNode = HeadNode->Next;
				HeadNode->Previous = nullptr;
			}

			else if (Entry == TailNode)
			{
				TailNode = TailNode->Previous;
				TailNode->Next = nullptr;
			}
			else
			{
				Entry->Next->Previous = Entry->Previous;
				Entry->Previous->Next = Entry->Next;
			}

			Entry->Next = Entry->Previous = nullptr;
			ListSize--;
		}

		FEntry* Pop()
		{
			FEntry* Value = HeadNode;
			if (Value == nullptr)
			{
				return nullptr;
			}
			Remove(Value);
			return Value;
		}

		bool Contains(const FEntry* Entry)
		{
			return Entry == HeadNode || Entry == TailNode || (Entry->Next && Entry->Previous);
		}
	};

	bool Initialize(size_t InMaxMemory)
	{
		MaxMemoryUsed = InMaxMemory;
		CurrentMemoryAvailable = MaxMemoryUsed;
		return true;
	}

	FEntry* Allocate(size_t Size, const TFunctionRef<void(FEntry*)>& OnEvict)
	{
		bool AllocatonReuse = false;
		while (Size > CurrentMemoryAvailable)
		{
			FEntry* Entry = Lru.Pop();
			if (Entry == nullptr)
			{
				return nullptr; // all pages mapped
			}

			OnEvict(Entry);
			CurrentMemoryAvailable += Entry->Size;
			AllocatonReuse = Entry->Size >= Size; // this entry can hold the entire requested size --> do not free memory but re-use
			Entry->Reset(AllocatonReuse);

			AddToFreeList(Entry);
		}

		ensure(Size <= CurrentMemoryAvailable);

		FEntry* Entry = GetEntry();
		Entry->Init(Size, AllocatonReuse);
		CurrentMemoryAvailable -= Size;

		return Entry;
	}

	bool Map(FEntry* Entry)
	{
		if (Lru.Contains(Entry)) 
		{
			Lru.Remove(Entry);
		}
		return true;
	}

	void Unmap(FEntry* Entry)
	{
		ensure(Entry->MappedRefCount.GetValue() == 0);
		Lru.AddTail(Entry);
	}

	void Touch(FEntry* Entry)
	{
		if (Lru.Contains(Entry))
		{
			Lru.Remove(Entry);
			ensure(Entry->MappedRefCount.GetValue() == 0);
			Lru.AddTail(Entry);
		}
	}

private:
	size_t MaxMemoryUsed = 0;
	size_t CurrentMemoryAvailable = 0;
	FEntryList Lru;

	void AddToFreeList(FEntry* Entry)
	{
		if (!Freelist)
		{
			Freelist = Entry;
		}
		else
		{
			Entry->Next = Freelist;
			Freelist = Entry;
 		}
	}
	FEntry* GetEntry()
	{
		FEntry* Entry = nullptr;
		if (Freelist)
		{
			FEntry* head = Freelist;
			Freelist = Freelist->Next;
			Entry = head;
		}
		else
		{
			Entry = new FEntry();
		}
		Entry->Parent = this;
		return Entry;
	}
	FEntry* Freelist = nullptr;
};

using FVirtualTextureChunk = FChunkCache::FEntry;
*/