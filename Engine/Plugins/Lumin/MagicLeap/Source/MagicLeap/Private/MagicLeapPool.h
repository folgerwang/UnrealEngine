// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#pragma once

#include "Containers/Array.h"

#define DEBUG_MEM_POOL 0

#if DEBUG_MEM_POOL
DEFINE_LOG_CATEGORY_STATIC(MagicLeapMemPool, Log, All);
#endif // DEBUG_MEM_POOL

template <class T>
class FMagicLeapPool
{
public:
	FMagicLeapPool(int32 InPoolSize)
	: Size(InPoolSize)
	{
		Blocks.AddZeroed(1);
		Blocks[0].AddZeroed(Size);
		for (int32 i = 0; i < Size; ++i)
		{
			Free.Push(&Blocks[0][i]);
		}
	}

	T* GetNextFree()
	{
		if (Free.Num() == 0)
		{
#if DEBUG_MEM_POOL
			UE_LOG(MagicLeapMemPool, Log, TEXT("FMagicLeapPool is out of space.  Allocating new block."));
#endif // DEBUG_MEM_POOL
			int32 NewBlockIndex = Blocks.AddZeroed(1);
			Blocks[NewBlockIndex].AddZeroed(Size);
			for (int32 i = 0; i < Size; ++i)
			{
				Free.Push(&Blocks[NewBlockIndex][i]);
			}
		}
#if DEBUG_MEM_POOL
		T* NewAllocation = Free.Pop();
		UE_LOG(MagicLeapMemPool, Log, TEXT("FMagicLeapPool allocated %p."), NewAllocation);
		Allocated.Push(NewAllocation);
		return NewAllocation;
#else
		return Free.Pop();
#endif // DEBUG_MEM_POOL
	}

	void Release(T* InAllocation)
	{
#if DEBUG_MEM_POOL
		checkf(Allocated.Contains(InAllocation), TEXT("Attempting to release memory that was not allocated by this pool!"));
		Allocated.Pop(InAllocation);
		UE_LOG(MagicLeapMemPool, Log, TEXT("FMagicLeapPool released %p."), InAllocation);
#endif // DEBUG_MEM_POOL
		Free.Push(InAllocation);
	}

private:
	const int32 Size;
	TArray<TArray<T>> Blocks;
	TArray<T*> Free;
#if DEBUG_MEM_POOL
	TArray<T*> Allocated;
#endif // DEBUG_MEM_POOL
};