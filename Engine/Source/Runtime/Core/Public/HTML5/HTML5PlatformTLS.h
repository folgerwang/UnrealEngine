// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5TLS.h: HTML5 platform TLS (Thread local storage and thread ID) functions
==============================================================================================*/

#pragma once
#include "Containers/Array.h"
#include "GenericPlatform/GenericPlatformTLS.h"

#ifdef __EMSCRIPTEN_PTHREADS__
#include <pthread.h>
#endif

/**
 * HTML5 implementation of the TLS OS functions
 */
struct CORE_API FHTML5TLS : public FGenericPlatformTLS
{
#ifndef __EMSCRIPTEN_PTHREADS__
	/**
	 * In singlethreaded Emscripten builds, returns a
	 * regular array to serve as an emulated TLS storage
	 * (Emscripten does support the pthread API even in
	 * singlethreaded builds, so the same set/getspecific
	 * code below could be used there, but it's faster
	 * to use this this approach and reduces code size
	 * a tiny bit.
	 */
	static TArray<void*>& GetFakeTLSArray()
	{
		static TArray<void*> TLS;
		return TLS;
	}
#endif

	/**
	 * Returns the currently executing thread's id
	 */
	static FORCEINLINE uint32 GetCurrentThreadId(void)
	{
#ifdef __EMSCRIPTEN_PTHREADS__
		return (uint32)pthread_self();
#else
		return 0;
#endif
	}

	/**
	 * Allocates a thread local store slot
	 */
	static FORCEINLINE uint32 AllocTlsSlot(void)
	{
#ifdef __EMSCRIPTEN_PTHREADS__
		// allocate a per-thread mem slot
		pthread_key_t Key = 0;
		if (pthread_key_create(&Key, NULL) != 0)
		{
			Key = 0xFFFFFFFF;  // matches the Windows TlsAlloc() retval //@todo android: should probably check for this below, or assert out instead
		}
		return Key;
#else
		return GetFakeTLSArray().Add(0);
#endif
	}

	/**
	 * Sets a value in the specified TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 * @param Value the value to store in the slot
	 */
static FORCEINLINE void SetTlsValue(uint32 SlotIndex,void* Value)
	{
#ifdef __EMSCRIPTEN_PTHREADS__
		pthread_setspecific((pthread_key_t)SlotIndex, Value);
#else
		GetFakeTLSArray()[SlotIndex] = Value;
#endif
	}

	/**
	 * Reads the value stored at the specified TLS slot
	 *
	 * @return the value stored in the slot
	 */
	static FORCEINLINE void* GetTlsValue(uint32 SlotIndex)
	{
#ifdef __EMSCRIPTEN_PTHREADS__
		return pthread_getspecific((pthread_key_t)SlotIndex);
#else
		return GetFakeTLSArray()[SlotIndex];
#endif
	}

	/**
	 * Frees a previously allocated TLS slot
	 *
	 * @param SlotIndex the TLS index to store it in
	 */
	static FORCEINLINE void FreeTlsSlot(uint32 SlotIndex)
	{
#ifdef __EMSCRIPTEN_PTHREADS__
		pthread_key_delete((pthread_key_t)SlotIndex);
#else
		// nothing to do, just grow the array forever
		// @todo if this done a lot, we can make a TMap
#endif
	}

protected:

};

typedef FHTML5TLS FPlatformTLS;
