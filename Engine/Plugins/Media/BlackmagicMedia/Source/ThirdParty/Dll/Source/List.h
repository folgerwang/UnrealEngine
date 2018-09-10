// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

/*
 * Node in a list
 */

struct ListNode
{
	void Remove()
	{
		Next->Prev = Prev;
		Prev->Next = Next;
	}

	bool IsLast()
	{
		return Next == 0;
	}
	bool IsFirst()
	{
		return Prev == 0;
	}

	ListNode* Next;
	ListNode* Prev;
};

/*
 * Head/Tail in a list
 */

struct ListHead
{
	ListHead()
	{
		Head = reinterpret_cast<ListNode*>(&Null);
		Null = 0;
		Tail = reinterpret_cast<ListNode*>(&Head);
	}
	bool IsEmpty()
	{
		return Head->IsLast();
	}
	void AddBefore(ListNode* InNode, ListNode* Before)
	{
		InNode->Next = Before;
		InNode->Prev = Before->Prev;
		Before->Prev->Next = InNode;
		Before->Prev = InNode;
	}
	void AddHead(ListNode& InNode)
	{
		AddHead(&InNode);
	}
	void AddHead(ListNode* InNode)
	{
		AddBefore(InNode, Head);
	}
	void AddTail(ListNode& InNode)
	{
		AddTail(&InNode);
	}
	void AddTail(ListNode* InNode)
	{
		AddBefore(InNode, reinterpret_cast<ListNode*>(&Null));
	}
	ListNode* RemHead()
	{
		if (IsEmpty())
		{
			return nullptr;
		}
		ListNode* Return = Head;
		Return->Remove();
		return Return;
	}
	ListNode* RemTail()
	{
		if (IsEmpty())
		{
			return nullptr;
		}
		ListNode* Return = Tail;
		Return->Remove();
		return Return;
	}

	ListNode* Head;
	ListNode* Null;
	ListNode* Tail;
};

/*
 * Helper methods
 */

#define LIST_OFFSET_OF(__CLASS, __MEMBER) reinterpret_cast<size_t>(&reinterpret_cast<__CLASS*>(0)->__MEMBER)
#define LIST_LISTOF(__CLASS, __MEMBER, __POINTER) reinterpret_cast<__CLASS*>(reinterpret_cast<char*>(__POINTER) - LIST_OFFSET_OF(__CLASS, __MEMBER)) 
