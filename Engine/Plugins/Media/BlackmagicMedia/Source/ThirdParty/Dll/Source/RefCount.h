// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

/*
 * Simple Reference count base class
 */
class RefCount
{
public:
	virtual ~RefCount() {};
	virtual int AddRef(void)
	{
		++Count;
		return Count;
	}
	virtual int Release(void)
	{
		// copied to avoid the delete
		int LocalCount = --Count;
		if (!LocalCount)
		{
			delete this;
		}
		return LocalCount;
	}
protected:
	Thread::FAtomic Count;
};

/*
 * AutoPointer to manage Reference counted pointers
 */
template<typename T>
class RefPointer
{
public:
	RefPointer()
		: Pointer(nullptr)
	{
	}
	RefPointer(const RefPointer<T>& InAutoPointer)
		: Pointer(InAutoPointer.Pointer)
	{
		if (Pointer)
		{
			Pointer->AddRef();
		}
	}
	RefPointer(T* InPointer)
		: Pointer(InPointer)
	{
		if (Pointer)
		{
			Pointer->AddRef();
		}
	}
	RefPointer(T&& InAutoPointer)
		: Pointer(InAutoPointer.Pointer)
	{
		InAutoPointer.Pointer = nullptr;
	}
	RefPointer& operator=(const RefPointer& InAutoPointer)
	{
		Reset();
		Pointer = InAutoPointer.Pointer;
		if (Pointer)
		{
			Pointer->AddRef();
		}
		return *this;
	}
	RefPointer& operator=(T* InPointer)
	{
		Reset();
		Pointer = InPointer;
		if (Pointer)
		{
			Pointer->AddRef();
		}
		return *this;
	}
	RefPointer& operator=(const RefPointer&& InAutoPointer)
	{
		Reset();
		Pointer = InAutoPointer.Pointer;
		InAutoPointer.Pointer = nullptr;
		return *this;
	}
	~RefPointer()
	{
		Reset();
	}
	T* operator->()
	{
		return Pointer;
	}
	void Reset(void)
	{
		if (Pointer)
		{
			Pointer->Release();
			Pointer = nullptr;
		}
	}
	T* Get()
	{
		return Pointer;
	}
	operator T*()
	{
		return Pointer;
	}
	operator void**()
	{
		Reset();
		return reinterpret_cast<void**>(&Pointer);
	}
	operator T**()
	{
		Reset();
		return &Pointer;
	}
	operator bool()
	{
		return Pointer != nullptr;
	}
protected:
	T* Pointer;
};
