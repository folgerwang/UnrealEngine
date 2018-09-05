// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "List.h"

namespace Thread
{
	class FAtomic
	{
	public:
		FAtomic()
			: Count(0)
		{
		}
		FAtomic& operator ++()
		{
			::InterlockedIncrement(&Count);
			return *this;
		}
		FAtomic& operator --()
		{
			::InterlockedDecrement(&Count);
			return *this;
		}
		operator int()
		{
			return static_cast<int>(Count);
		}
	protected:
		LONG Count;
	};

	/*
	 * Simple lock
	 */
	class FEvent;
	class FLock
	{
	public:
		friend FEvent;
		FLock()
		{
			::InitializeCriticalSection(&CriticalSection);
		}
		~FLock()
		{
			::DeleteCriticalSection(&CriticalSection);
		}
		void Lock(void)
		{
			EnterCriticalSection(&CriticalSection);
		}
		void Unlock(void)
		{
			LeaveCriticalSection(&CriticalSection);
		}
	protected:
		CRITICAL_SECTION CriticalSection;
	};
	/*
	 * AutoLock to manage locks
	 */
	class FAutoLock
	{
	public:
		FAutoLock(FLock& InLock)
			: Lock(InLock)
		{
			Lock.Lock();
		}
		~FAutoLock()
		{
			Lock.Unlock();
		}
	protected:
		FLock& Lock;
	};
	/*
	 * Simple Event
	 */
	class FEvent
	{
		friend FLock;
	public:
		FEvent()
		{
			InitializeConditionVariable(&ConditionVariable);
		}
		void Wait(FLock& InLock, uint32_t InMilliseconds = INFINITE)
		{
			SleepConditionVariableCS(&ConditionVariable, &InLock.CriticalSection, InMilliseconds);
		}
		void Signal(void)
		{
			WakeConditionVariable(&ConditionVariable);
		}
		void SignalAll(void)
		{
			WakeAllConditionVariable(&ConditionVariable);
		}
		protected:
			CONDITION_VARIABLE ConditionVariable;
	};
	/*
	 * base of message
	 */
	class FMessage
	{
	public:
		ListNode MessageList;
		int MessageType;
	};
	/*
	 * Thread safe message queue
	 */
	class FMailbox
	{
	public:
		FMailbox()
		: Count(0)
		{
		}
		~FMailbox()
		{
		}
		void Send(FMessage* InMessage)
		{
			FAutoLock AutoLock(Lock);
			List.AddTail(InMessage->MessageList);
			++Count;
			Event.Signal();
		}
		bool Peek()
		{
			return !List.IsEmpty();
		}
		FMessage* Read()
		{
			FAutoLock AutoLock(Lock);
			while (List.IsEmpty())
			{
				Event.Wait(Lock);
			}
			FMessage* OutMessage = LIST_LISTOF(FMessage, MessageList, List.RemHead());
			--Count;
			return OutMessage;
		}
		int GetCount()
		{
			return Count;
		}
	protected:
		int Count;
		FLock Lock;
		FEvent Event;
		ListHead List;
	};
}
