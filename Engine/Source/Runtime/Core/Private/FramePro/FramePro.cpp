/*
	This software is provided 'as-is', without any express or implied warranty.
	In no event will the author(s) be held liable for any damages arising from
	the use of this software.

	Permission is granted to anyone to use this software for any purpose, including
	commercial applications, and to alter it and redistribute it freely, subject to
	the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.

	Author: Stewart Lynch
	www.puredevsoftware.com
	slynch@puredevsoftware.com

	Add FramePro.cpp to your project to allow FramePro to communicate with your application.
*/

// BEGIN EPIC 
#include "FramePro/FramePro.h"
#include "CoreGlobals.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
//END EPIC 
//------------------------------------------------------------------------
// EventTraceWin32.cpp

//------------------------------------------------------------------------
// EventTraceWin32.hpp
#ifndef FRAMEPRO_EVENTTRACEWIN32_H_INCLUDED
#define FRAMEPRO_EVENTTRACEWIN32_H_INCLUDED

//------------------------------------------------------------------------

//------------------------------------------------------------------------
// FrameProLib.hpp
#ifndef FRAMEPROLIB_H_INCLUDED
#define FRAMEPROLIB_H_INCLUDED

//------------------------------------------------------------------------

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <new>

#if FRAMEPRO_WIN_BASED_PLATFORM
	#include <tchar.h>
#endif

// BEGIN EPIC
#if FRAMEPRO_UE4_BASED_PLATFORM
	#include "HAL/PlatformProcess.h"
	#include "HAL/PlatformTLS.h"
	#include "HAL/Event.h"
	#include "HAL/CriticalSection.h"
	#include "HAL/Runnable.h"
	#include "HAL/RunnableThread.h"
	#include "Templates/UniquePtr.h"
#endif
// END EPIC

// BEGIN EPIC 
// Remove FRAMEPRO_UNIX_BASED_PLATFORM and FRAMEPRO_TIMER_QUERY_PERFORMANCE_COUNTER blocks
// END EPIC
//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED
	#if FRAMEPRO_WIN_BASED_PLATFORM
		#if defined(AF_IPX) && !defined(_WINSOCK2API_)
			#error winsock already defined. Please include winsock2.h before including windows.h or use WIN32_LEAN_AND_MEAN. See the FAQ for more info.
		#endif
		#if FRAMEPRO_TOOLSET_UE4
			// BEGIN EPIC 
			#include "Windows/AllowWindowsPlatformTypes.h"
			// END EPIC
		#endif
		#include <winsock2.h>
		#include <ws2tcpip.h>
		#if FRAMEPRO_TOOLSET_UE4
			// BEGIN EPIC 
			#include "Windows/HideWindowsPlatformTypes.h"
			// END EPIC
		#endif
	// BEGIN EPIC
	#elif FRAMEPRO_PLATFORM_SWITCH
		#include "Switch/SwitchPlatformFramePro.h"
	// END EPIC
	#else
		#include <sys/socket.h>
		#include <netinet/in.h>
	#endif
#endif

//------------------------------------------------------------------------
#define FRAMEPRO_MAX_INLINE_STRING_LENGTH 256

//------------------------------------------------------------------------
// BEGIN EPIC 
#if PLATFORM_64BITS
// END EPIC
	#define FRAMEPRO_X64 1
#else
	#define FRAMEPRO_X64 0
#endif

//------------------------------------------------------------------------
// BEGIN EPIC
//#if FRAMEPRO_WIN_BASED_PLATFORM
//	#define FRAMEPRO_THREAD_LOCAL __declspec(thread)
//#endif
// END EPIC

//------------------------------------------------------------------------
// BEGIN EPIC 
#define FRAMEPRO_NO_INLINE 
#define FRAMEPRO_FORCE_INLINE FORCEINLINE
// END EPIC

//------------------------------------------------------------------------
#if FRAMEPRO_WIN_BASED_PLATFORM
	#define FRAMEPRO_TCHAR _TCHAR
#else
	#define FRAMEPRO_TCHAR char
#endif

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	enum { g_FrameProLibVersion = 11 };

	//------------------------------------------------------------------------
	namespace StringLiteralType
	{
		enum Enum
		{
			NameAndSourceInfo = 0,
			NameAndSourceInfoW,
			SourceInfo,
			GeneralString,
			StringLiteralTimerName,
		};
	}

	//------------------------------------------------------------------------
	class FrameProTLS;

	//------------------------------------------------------------------------
// BEGIN EPIC
//	extern FRAMEPRO_THREAD_LOCAL FrameProTLS* gp_FrameProTLS;
// END EPIC
	extern FRAMEPRO_NO_INLINE FrameProTLS* CreateFrameProTLS();
	extern FRAMEPRO_NO_INLINE void DestroyFrameProTLS(FrameProTLS* p_framepro_tls);

// BEGIN EPIC
	static uint32 GetFrameProTLSSlot()
	{
		static uint32 TlsSlot = FPlatformTLS::AllocTlsSlot();
		return TlsSlot;
	}
// END EPIC

	//------------------------------------------------------------------------
	FrameProTLS* GetFrameProTLS()
	{
// BEGIN EPIC
//		FrameProTLS* p_framepro_tls = gp_FrameProTLS;
		FrameProTLS* p_framepro_tls = (FrameProTLS*)FPlatformTLS::GetTlsValue(GetFrameProTLSSlot());
// END EPIC
		return p_framepro_tls ? p_framepro_tls : CreateFrameProTLS();
	}

	//------------------------------------------------------------------------
	void DebugWrite(const FRAMEPRO_TCHAR* p_str, ...);

	//------------------------------------------------------------------------
	inline bool IsPow2(int value)
	{
		return (value & (value-1)) == 0;
	}

	//------------------------------------------------------------------------
	inline int AlignUpPow2(int value, int alignment)
	{
		FRAMEPRO_ASSERT(IsPow2(alignment));		// non-pow2 value passed to align function
		int mask = alignment - 1;
		return (value + mask) & ~mask;
	}

	//------------------------------------------------------------------------
	template<typename T>
	inline T FramePro_Min(T a, T b)
	{
		return a < b ? a : b;
	}

	//------------------------------------------------------------------------
	template<typename T>
	inline T FramePro_Max(T a, T b)
	{
		return a > b ? a : b;
	}

	//------------------------------------------------------------------------
	template<class T>
	inline T* New(Allocator* p_allocator)
	{
		T* p = (T*)p_allocator->Alloc(sizeof(T));
		new (p)T();
		return p;
	}

	//------------------------------------------------------------------------
	template<class T, typename Targ1>
	inline T* New(Allocator* p_allocator, Targ1 arg1)
	{
		T* p = (T*)p_allocator->Alloc(sizeof(T));
		new (p)T(arg1);
		return p;
	}

	//------------------------------------------------------------------------
	template<class T, typename Targ1, typename Targ2, typename TArg3>
	inline T* New(Allocator* p_allocator, Targ1 arg1, Targ2 arg2, TArg3 arg3)
	{
		T* p = (T*)p_allocator->Alloc(sizeof(T));
		new (p)T(arg1, arg2, arg3);
		return p;
	}

	//------------------------------------------------------------------------
	template<typename T>
	inline void Delete(Allocator* p_allocator, T* p)
	{
		p->~T();
		p_allocator->Free(p);
	}

	//------------------------------------------------------------------------
	template<typename T>
	inline void Swap(T& a, T& b)
	{
		T temp = a;
		a = b;
		b = temp;
	}

	//------------------------------------------------------------------------
	namespace ThreadState
	{
		enum Enum
		{
			Initialized = 0,
			Ready,
			Running,
			Standby,
			Terminated,
			Waiting,
			Transition,
			DeferredReady,
		};
	}

	//------------------------------------------------------------------------
	namespace ThreadWaitReason
	{
		enum Enum
		{
			Executive = 0,
			FreePage,
			PageIn,
			PoolAllocation,
			DelayExecution,
			Suspended,
			UserRequest,
			WrExecutive,
			WrFreePage,
			WrPageIn,
			WrPoolAllocation,
			WrDelayExecution,
			WrSuspended,
			WrUserRequest,
			WrEventPair,
			WrQueue,
			WrLpcReceive,
			WrLpcReply,
			WrVirtualMemory,
			WrPageOut,
			WrRendezvous,
			WrKeyedEvent,
			WrTerminated,
			WrProcessInSwap,
			WrCpuRateControl,
			WrCalloutStack,
			WrKernel,
			WrResource,
			WrPushLock,
			WrMutex,
			WrQuantumEnd,
			WrDispatchInt,
			WrPreempted,
			WrYieldExecution,
			WrFastMutex,
			WrGuardedMutex,
			WrRundown,
			MaximumWaitReason,
		};
	}

	//------------------------------------------------------------------------
	struct ContextSwitch
	{
		int64 m_Timestamp;
		int m_ProcessId;
		int m_CPUId;
		int m_OldThreadId;
		int m_NewThreadId;
		ThreadState::Enum m_OldThreadState;
		ThreadWaitReason::Enum m_OldThreadWaitReason;
	};

	//------------------------------------------------------------------------
	#if !FRAMEPRO_WIN_BASED_PLATFORM
		#define sprintf_s sprintf

		inline void strncpy_s(char *p_dest, size_t element_count, const char *p_source, size_t count)
		{
			(void)(element_count);
			strncpy(p_dest, p_source, count);
		}

		inline void wcsncpy_s(wchar_t *p_dest, size_t element_count, const wchar_t *p_source, size_t count)
		{
			(void)(element_count);
			wcsncpy(p_dest, p_source, count);
		}

		inline void strcpy_s(char *p_dest, const char *p_source)
		{
			strcpy(p_dest, p_source);
		}

		inline void strcpy_s(char *p_dest, size_t dest_length, const char *p_source)
		{
			(void)(dest_length);
			strcpy(p_dest, p_source);
		}

		inline void _vstprintf_s(FRAMEPRO_TCHAR* const p_buffer, size_t const buffer_size, FRAMEPRO_TCHAR const* const p_format, va_list arg_list)
		{
			(void)(buffer_size);
			vsprintf(p_buffer, p_format, arg_list);
		}

		inline void OutputDebugString(const FRAMEPRO_TCHAR* p_string)
		{
			printf("%s", p_string);
		}

		inline uint64 GetCurrentThreadId()
		{
// START EPIC
#if FRAMEPRO_UE4_BASED_PLATFORM
			return FPlatformTLS::GetCurrentThreadId();
#else
			return (uint64)pthread_self();
#endif
// END EPIC
		}

		inline int fopen_s(FILE **pp_file, const char* p_filename, const char* p_mode)
		{
			*pp_file = fopen(p_filename, p_mode);
			return *pp_file ? 0 : 1;
		}

		inline int GetCurrentProcessId()
		{
// START EPIC
#if FRAMEPRO_UE4_BASED_PLATFORM
			return FPlatformProcess::GetCurrentProcessId();
#else
			return getpid();
#endif
// END EPIC
		}

		inline void localtime_s(struct tm* p_tm, const time_t *p_time)
		{
			tm* p_local_tm = localtime(p_time);
			*p_tm = *p_local_tm;
		}
	#endif

	//------------------------------------------------------------------------
	#if FRAMEPRO_WIN_BASED_PLATFORM
		#define FRAMEPRO_SOCKET SOCKET
		#define FRAMEPRO_INVALID_SOCKET INVALID_SOCKET
		#define FRAMEPRO_MAX_PATH MAX_PATH
		#define FRAMEPRO_STRING _T
		#define FRAMEPRO_SOCKET_ERROR SOCKET_ERROR
// START EPIC
	#else
// END EPIC
		typedef int FRAMEPRO_SOCKET;
		#define FRAMEPRO_INVALID_SOCKET -1
		#define FRAMEPRO_MAX_PATH 256
		#define FRAMEPRO_STRING
		#define FRAMEPRO_SOCKET_ERROR -1
	#endif
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPROLIB_H_INCLUDED

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	class Allocator;
	class EventTraceWin32Imp;
	class DynamicString;

	//------------------------------------------------------------------------
	typedef void (*ContextSwitchCallback)(const ContextSwitch& context_switch, void* p_param);

	//------------------------------------------------------------------------
	class EventTraceWin32
	{
	public:
		EventTraceWin32(Allocator* p_allocator);

		~EventTraceWin32();

		bool Start(ContextSwitchCallback p_context_switch_callback, void* p_context_switch_callback_param, DynamicString& error);

		void Stop();

		void Flush();

		//------------------------------------------------------------------------
		// data
	private:
		EventTraceWin32Imp* mp_Imp;

		Allocator* mp_Allocator;
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPRO_EVENTTRACEWIN32_H_INCLUDED


//------------------------------------------------------------------------
// CriticalSection.hpp
#ifndef FRAMEPRO_CRITICALSECTION_H_INCLUDED
#define FRAMEPRO_CRITICALSECTION_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	class CriticalSection
	{
	public:
		CriticalSection()
		{
#if FRAMEPRO_WIN_BASED_PLATFORM
			InitializeSRWLock(&lock);
// START EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
			//...
// END EPIC
#else
			pthread_mutexattr_t attr;
			pthread_mutexattr_init(&attr);
			pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
			pthread_mutex_init(&cs, &attr);
#endif

#if FRAMEPRO_DEBUG
			m_Locked = false;

			#if FRAMEPRO_WIN_BASED_PLATFORM
				m_LockedOnThread = 0xffffffff;
			#endif
#endif
		}

		~CriticalSection()
		{
// START EPIC
#if !FRAMEPRO_WIN_BASED_PLATFORM && !FRAMEPRO_UE4_BASED_PLATFORM
			pthread_mutex_destroy(&cs);
#endif
// END EPIC
		}

		void Enter()
		{
			#if FRAMEPRO_WIN_BASED_PLATFORM
				FRAMEPRO_ASSERT(GetCurrentThreadId() != m_LockedOnThread);
			#endif

#if FRAMEPRO_WIN_BASED_PLATFORM
			AcquireSRWLockExclusive(&lock);
// START EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
			cs.Lock();
// END EPIC
#else
			pthread_mutex_lock(&cs);
#endif

#if FRAMEPRO_DEBUG
			m_Locked = true;
			#if FRAMEPRO_WIN_BASED_PLATFORM
				m_LockedOnThread = GetCurrentThreadId();
			#endif
#endif
		}

		void Leave()
		{
#if FRAMEPRO_DEBUG
			m_Locked = false;
			#if FRAMEPRO_WIN_BASED_PLATFORM
				m_LockedOnThread = 0xffffffff;
			#endif
#endif

#if FRAMEPRO_WIN_BASED_PLATFORM
			ReleaseSRWLockExclusive(&lock);
// START EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
			cs.Unlock();
// END EPIC
#else
			pthread_mutex_unlock(&cs);
#endif
		}

		//------------------------------------------------------------------------
#if FRAMEPRO_DEBUG
		bool Locked() const		// only safe to use in an assert to check that it IS locked
		{
			return m_Locked;
		}
#endif

		//------------------------------------------------------------------------
		// data
	private:
#if FRAMEPRO_WIN_BASED_PLATFORM
		SRWLOCK lock;
// START EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
		FCriticalSection cs;
// END EPIC
#else
		pthread_mutex_t cs;
#endif

#if FRAMEPRO_DEBUG
		RelaxedAtomic<bool> m_Locked;
		#if FRAMEPRO_WIN_BASED_PLATFORM
			unsigned int m_LockedOnThread;
		#endif
#endif
	};

	//------------------------------------------------------------------------
	class CriticalSectionScope
	{
	public:
		CriticalSectionScope(CriticalSection& in_cs) : cs(in_cs) { cs.Enter(); }
		~CriticalSectionScope() { cs.Leave(); }
	private:
		CriticalSectionScope(const CriticalSectionScope&);
		CriticalSectionScope& operator=(const CriticalSectionScope&);
		CriticalSection& cs;
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPRO_CRITICALSECTION_H_INCLUDED

//------------------------------------------------------------------------
// HashMap.hpp
#ifndef HASHMAP_H_INCLUDED
#define HASHMAP_H_INCLUDED

//------------------------------------------------------------------------

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//-----------------------------------------------------------------
#include <new>

//-----------------------------------------------------------------
#define FRAMEPRO_PROFILE_HashMap 0

//-----------------------------------------------------------------
namespace FramePro
{
	//-----------------------------------------------------------------
	typedef unsigned int uint;
	typedef char byte;

	//-----------------------------------------------------------------
	template<typename TKey, typename TValue>
	class HashMap
	{
	public:
		//-----------------------------------------------------------------
		struct Pair
		{
			TKey m_Key;
			TValue m_Value;
		};

		//-----------------------------------------------------------------
		// The default capacity of the set. The capacity is the number
		// of elements that the set is expected to hold. The set will resized
		// when the item count is greater than the capacity;
		HashMap(Allocator* p_allocator)
		:	m_Capacity(0),
			mp_Table(NULL),
			m_Count(0),
			mp_ItemPool(NULL),
			mp_FreePair(NULL),
			mp_Allocator(p_allocator)
#if FRAMEPRO_PROFILE_HashMap
			,m_IterAcc(0)
			,m_IterCount(0)
#endif
		{
			AllocTable(GetNextPow2((256 * m_DefaultCapacity) / m_Margin));
		}

		//-----------------------------------------------------------------
		~HashMap()
		{
			Clear();

			mp_Allocator->Free(mp_Table);
			FreePools();
		}

		//-----------------------------------------------------------------
		void Clear()
		{
			RemoveAll();
		}

		//-----------------------------------------------------------------
		void RemoveAll()
		{
			for(int i=0; i<m_Capacity; ++i)
			{
				Pair* p_pair = mp_Table[i];
				if(p_pair)
				{
					FreePair(p_pair);
					mp_Table[i] = NULL;
				}
			}
			m_Count = 0;
		}

		//-----------------------------------------------------------------
		// Add a value to this set.
		// If this set already contains the value does nothing.
		void Add(const TKey& key, const TValue& value)
		{
			int index = GetItemIndex(key);

			if(IsItemInUse(index))
			{
				mp_Table[index]->m_Value = value;
			}
			else
			{
				if(m_Capacity == 0 || m_Count == (m_Margin * m_Capacity) / 256)
				{
					Resize(2*m_Capacity);
					index = GetItemIndex(key);
				}

				// make a copy of the value
				Pair* p_pair = AllocPair();
				p_pair->m_Key = key;
				p_pair->m_Value = value;

				// add to table
				mp_Table[index] = p_pair;

				++m_Count;
			}
		}

		//-----------------------------------------------------------------
		// if this set contains the value set value to the existing value and
		// return true, otherwise set to the default value and return false.
		bool TryGetValue(const TKey& key, TValue& value) const
		{
			if(!mp_Table)
				return false;

			const int index = GetItemIndex(key);
			if(IsItemInUse(index))
			{
				value = mp_Table[index]->m_Value;
				return true;
			}
			else
			{
				return false;
			}
		}

		//-----------------------------------------------------------------
		int GetCount() const
		{
			return m_Count;
		}

		//-----------------------------------------------------------------
		void Resize(int new_capacity)
		{
			new_capacity = GetNextPow2(new_capacity);

			// keep a copy of the old table
			Pair** const p_old_table = mp_Table;
			const int old_capacity = m_Capacity;

			// allocate the new table
			AllocTable(new_capacity);

			// copy the values from the old to the new table
			Pair** p_old_pair = p_old_table;
			for(int i=0; i<old_capacity; ++i, ++p_old_pair)
			{
				Pair* p_pair = *p_old_pair;
				if(p_pair)
				{
					const int index = GetItemIndex(p_pair->m_Key);
					mp_Table[index] = p_pair;
				}
			}

			mp_Allocator->Free(p_old_table);
		}

		//-----------------------------------------------------------------
		size_t GetMemorySize() const
		{
			size_t table_memory = m_Capacity * sizeof(Pair*);

			size_t item_memory = 0;
			byte* p_pool = mp_ItemPool;
			while(p_pool)
			{
				p_pool = *(byte**)p_pool;
				item_memory += m_ItemBlockSize;
			}

			return table_memory + item_memory;
		}

	private:
		//-----------------------------------------------------------------
		static int GetNextPow2(int value)
		{
			int p = 2;
			while(p < value)
				p *= 2;
			return p;
		}

		//-----------------------------------------------------------------
		void AllocTable(const int capacity)
		{
			FRAMEPRO_ASSERT(capacity < m_MaxCapacity);
			m_Capacity = capacity;

			// allocate a block of memory for the table
			if(capacity > 0)
			{
				const int size = capacity * sizeof(Pair*);
				mp_Table = (Pair**)mp_Allocator->Alloc(size);
				memset(mp_Table, 0, size);
			}
		}

		//-----------------------------------------------------------------
		bool IsItemInUse(const int index) const
		{
			return mp_Table[index] != NULL;
		}

		//-----------------------------------------------------------------
		int GetItemIndex(const TKey& key) const
		{
			FRAMEPRO_ASSERT(mp_Table);
			const uint hash = key.GetHashCode();
			int srch_index = hash & (m_Capacity-1);
			while(IsItemInUse(srch_index) && !(mp_Table[srch_index]->m_Key == key))
			{
				srch_index = (srch_index + 1) & (m_Capacity-1);
#if FRAMEPRO_PROFILE_HashMap
				++m_IterAcc;
#endif
			}

#if FRAMEPRO_PROFILE_HashMap
			++m_IterCount;
			double average = m_IterAcc / (double)m_IterCount;
			if(average > 2.0)
			{
				static int last_write_time = 0;
				int now = GetTickCount();
				if(now - last_write_time > 1000)
				{
					last_write_time = now;
					FRAMEPRO_TCHAR temp[64];
					_stprintf_s(temp, FRAMEPRO_STRING("WARNING: HashMap average: %f\n"), (float)average);
					OutputDebugString(temp);
				}
			}
#endif
			return srch_index;
		}

		//-----------------------------------------------------------------
		static bool InRange(
			const int index,
			const int start_index,
			const int end_index)
		{
			return (start_index <= end_index) ?
				index >= start_index && index <= end_index :
				index >= start_index || index <= end_index;
		}

		//-----------------------------------------------------------------
		void FreePools()
		{
			byte* p_pool = mp_ItemPool;
			while(p_pool)
			{
				byte* p_next_pool = *(byte**)p_pool;
				mp_Allocator->Free(p_pool);
				p_pool = p_next_pool;
			}
			mp_ItemPool = NULL;
			mp_FreePair = NULL;
		}

		//-----------------------------------------------------------------
		Pair* AllocPair()
		{
			if(!mp_FreePair)
			{
				// allocate a new pool and link to pool list
				byte* p_new_pool = (byte*)mp_Allocator->Alloc(m_ItemBlockSize);
				*(byte**)p_new_pool = mp_ItemPool;
				mp_ItemPool = p_new_pool;

				// link all items onto free list
				mp_FreePair = p_new_pool + sizeof(Pair);
				byte* p = (byte*)mp_FreePair;
				int item_count = m_ItemBlockSize / sizeof(Pair) - 2;	// subtract 2 for pool pointer and last item
				FRAMEPRO_ASSERT(item_count);
				for(int i=0; i<item_count; ++i, p+=sizeof(Pair))
				{
					*(byte**)p = p + sizeof(Pair);
				}
				*(byte**)p = NULL;
			}

			// take item off free list
			Pair* p_pair = (Pair*)mp_FreePair;
			mp_FreePair = *(byte**)mp_FreePair;

			// construct the pair
			new (p_pair)Pair;

			return p_pair;
		}

		//-----------------------------------------------------------------
		void FreePair(Pair* p_pair)
		{
			p_pair->~Pair();

			*(byte**)p_pair = mp_FreePair;
			mp_FreePair = (byte*)p_pair;
		}

		//-----------------------------------------------------------------
		// data
	private:
		enum { m_DefaultCapacity = 32 };
		enum { m_InvalidIndex = -1 };
		enum { m_MaxCapacity = 0x7fffffff };
		enum { m_Margin = (30 * 256) / 100 };
		enum { m_ItemBlockSize = 4096 };

		int m_Capacity;			// the current capacity of this set, will always be >= m_Margin*m_Count/256
		Pair** mp_Table;		// NULL for a set with capacity 0
		int m_Count;			// the current number of items in this set, will always be <= m_Margin*m_Count/256

		byte* mp_ItemPool;
		byte* mp_FreePair;

		Allocator* mp_Allocator;

#if FRAMEPRO_PROFILE_HashMap
		mutable int64 m_IterAcc;
		mutable int64 m_IterCount;
#endif
	};
}

//-----------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//-----------------------------------------------------------------
#endif		// #ifndef HASHMAP_H_INCLUDED

//------------------------------------------------------------------------
// FrameProString.hpp
#ifndef FRAMEPROSTRING_H_INCLUDED
#define FRAMEPROSTRING_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
// IncrementingBlockAllocator.hpp
#ifndef INCREMENTINGBLOCKALLOCATOR_H_INCLUDED
#define INCREMENTINGBLOCKALLOCATOR_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	class Allocator;

	//------------------------------------------------------------------------
	class IncrementingBlockAllocator
	{
		static const int m_BlockSize = 4096;
		static const int m_MemoryBlockSize = m_BlockSize - sizeof(struct Block*);

		struct Block
		{
			Block* mp_Next;
			char m_Memory[m_MemoryBlockSize];
		};
		static_assert(sizeof(Block) == m_BlockSize, "Block size incorrect");

	public:
		IncrementingBlockAllocator(Allocator* p_allocator);

		~IncrementingBlockAllocator();

		void Clear();

		void* Alloc(size_t size);

		size_t GetMemorySize() const { return m_MemorySize; }

	private:
		void AllocateBlock();

		//------------------------------------------------------------------------
		// data
	private:
		Allocator* mp_Allocator;

		Block* mp_BlockList;

		size_t m_CurrentBlockSize;

		size_t m_MemorySize;
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef INCREMENTINGBLOCKALLOCATOR_H_INCLUDED

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#include <string.h>
#include <wchar.h>

//------------------------------------------------------------------------
namespace FramePro
{
	//-------------------------------------------------------------
	// from http://murmurhash.googlepages.com/MurmurHash2.cpp
	inline unsigned int MurmurHash2(const void * key, int len, unsigned int seed)
	{
		// 'm' and 'r' are mixing constants generated offline.
		// They're not really 'magic', they just happen to work well.

		const unsigned int m = 0x5bd1e995;
		const int r = 24;

		// Initialize the hash to a 'random' value

		unsigned int h = seed ^ len;

		// Mix 4 bytes at a time into the hash

		const unsigned char * data = (const unsigned char *)key;

		while(len >= 4)
		{
			unsigned int k = *(unsigned int *)data;

			k *= m; 
			k ^= k >> r; 
			k *= m; 
				
			h *= m; 
			h ^= k;

			data += 4;
			len -= 4;
		}
			
		// Handle the last few bytes of the input array

		switch(len)
		{
		case 3: h ^= data[2] << 16;
		case 2: h ^= data[1] << 8;
		case 1: h ^= data[0];
				h *= m;
		};

		// Do a few final mixes of the hash to ensure the last few
		// bytes are well-incorporated.

		h ^= h >> 13;
		h *= m;
		h ^= h >> 15;

		return h;
	} 

	//-------------------------------------------------------------
	FRAMEPRO_FORCE_INLINE unsigned int MurmurHash2(const char* p_str)
	{
		const unsigned int prime = 0x1000193;
		return MurmurHash2(p_str, (int)strlen(p_str), prime);
	}

	//-------------------------------------------------------------
	FRAMEPRO_FORCE_INLINE unsigned int MurmurHash2(const wchar_t* p_str)
	{
		const unsigned int prime = 0x1000193;
		return MurmurHash2(p_str, (int)wcslen(p_str) * sizeof(wchar_t), prime);
	}

	//------------------------------------------------------------------------
	// this string class is meant to be as light weight as possible and does
	// not clean up its allocations.
	class String
	{
	public:
		//------------------------------------------------------------------------
		String()
		{
		}

		//------------------------------------------------------------------------
		// fast constructor for the case where we are giving it a string literal
		String(const char* p_value)
		:	
#if FRAMEPRO_DETECT_HASH_COLLISIONS
			mp_Value(p_value),
#endif
			m_HashCode(MurmurHash2(p_value))
		{
		}

		//------------------------------------------------------------------------
		// allocate a copy of the string and change mp_Value to point to it
		void TakeCopy(IncrementingBlockAllocator& allocator)
		{
#if FRAMEPRO_DETECT_HASH_COLLISIONS
			const char* p_old_value = mp_Value;
			size_t len = strlen(p_old_value);
			char* p_new_value = (char*)allocator.Alloc(len+1);
			strncpy_s(p_new_value, len+1, mp_Value, len);
			mp_Value = p_new_value;
#else
			(void)allocator;
#endif
		}

		//------------------------------------------------------------------------
		FRAMEPRO_FORCE_INLINE unsigned int GetHashCode() const
		{
			return m_HashCode;
		}

		//------------------------------------------------------------------------
		FRAMEPRO_FORCE_INLINE bool operator==(const String& other) const
		{
			return
				m_HashCode == other.m_HashCode
#if FRAMEPRO_DETECT_HASH_COLLISIONS
				&& strcmp(mp_Value, other.mp_Value) == 0
#endif
				;
		}

		//------------------------------------------------------------------------
		// data
	private:
#if FRAMEPRO_DETECT_HASH_COLLISIONS
		const char* mp_Value;
#endif
		unsigned int m_HashCode;
	};

	//------------------------------------------------------------------------
	// this string class is meant to be as light weight as possible and does
	// not clean up its allocations.
	class WString
	{
	public:
		//------------------------------------------------------------------------
		WString()
		{
		}

		//------------------------------------------------------------------------
		// fast constructor for the case where we are giving it a string literal
		WString(const wchar_t* p_value)
		:	
#if FRAMEPRO_DETECT_HASH_COLLISIONS
			mp_Value(p_value),
#endif
			m_HashCode(MurmurHash2(p_value))
		{
		}

		//------------------------------------------------------------------------
		// allocate a copy of the string and change mp_Value to point to it
		void TakeCopy(IncrementingBlockAllocator& allocator)
		{
#if FRAMEPRO_DETECT_HASH_COLLISIONS
			const wchar_t* p_old_value = mp_Value;
			size_t len = wcslen(p_old_value);
			wchar_t* p_new_value = (wchar_t*)allocator.Alloc((len+1)*sizeof(wchar_t));
			wcsncpy_s(p_new_value, len+1, mp_Value, len);
			mp_Value = p_new_value;
#else
			(void)allocator;
#endif
		}

		//------------------------------------------------------------------------
		FRAMEPRO_FORCE_INLINE unsigned int GetHashCode() const
		{
			return m_HashCode;
		}

		//------------------------------------------------------------------------
		FRAMEPRO_FORCE_INLINE bool operator==(const WString& other) const
		{
			return m_HashCode == other.m_HashCode
#if FRAMEPRO_DETECT_HASH_COLLISIONS
				&& wcscmp(mp_Value, other.mp_Value) == 0
#endif
				;
		}

		//------------------------------------------------------------------------
		// data
	private:
#if FRAMEPRO_DETECT_HASH_COLLISIONS
		const wchar_t* mp_Value;
#endif
		unsigned int m_HashCode;
	};

	//------------------------------------------------------------------------
	class DynamicString
	{
	public:
		//------------------------------------------------------------------------
		DynamicString(Allocator* p_allocator)
		:	mp_Value(NULL),
			mp_Allocator(p_allocator)
		{
		}

		//------------------------------------------------------------------------
		void operator=(const char* p_value)
		{
			FRAMEPRO_ASSERT(!mp_Value);
			size_t len = strlen(p_value);
			mp_Value = (char*)mp_Allocator->Alloc(len + 1);
			strncpy_s(mp_Value, len + 1, p_value, len);
		}

		//------------------------------------------------------------------------
		~DynamicString()
		{
			if (mp_Value)
				mp_Allocator->Free(mp_Value);
		}

		//------------------------------------------------------------------------
		void CopyTo(char* p_dest, size_t max_length)
		{
			if (mp_Value)
			{
				size_t len = strlen(mp_Value);
				len = FramePro_Min(len, max_length - 1);
				strncpy_s(p_dest, max_length, mp_Value, len);
			}
			else
			{
				strcpy_s(p_dest, max_length, "");
			}
		}

		//------------------------------------------------------------------------
		// data
	private:
		char* mp_Value;
		Allocator* mp_Allocator;
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPROSTRING_H_INCLUDED

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#if FRAMEPRO_EVENT_TRACE_WIN32
	#include <wmistr.h>
	#define INITGUID
	#include <evntrace.h>
	#include <evntcons.h>
	#include <tchar.h>
	#include <Tdh.h>

	#pragma comment(lib, "tdh.lib")
	#pragma comment(lib, "Advapi32.lib")
#endif

//------------------------------------------------------------------------
#if FRAMEPRO_EVENT_TRACE_WIN32
namespace FramePro
{
	//------------------------------------------------------------------------
	struct ThreadIdKey
	{
		ThreadIdKey() :	m_ThreadId(0) {}

		ThreadIdKey(int thread_id) : m_ThreadId(thread_id) {}

		uint GetHashCode() const
		{
			const unsigned int prime = 0x1000193;
			return (uint)(m_ThreadId * prime);
		}

		bool operator==(const ThreadIdKey& other) const
		{
			return m_ThreadId == other.m_ThreadId;
		}

		int m_ThreadId;
	};

	//------------------------------------------------------------------------
	RelaxedAtomic<bool> g_ShuttingDown = false;		// no way to wait for ETW to stop receiving callbacks after stopping, so we need this horrible global bool

	//------------------------------------------------------------------------
	class EventTraceWin32Imp
	{
	public:
		EventTraceWin32Imp(Allocator* p_allocator);

		~EventTraceWin32Imp();

		bool Start(ContextSwitchCallback p_context_switch_callback, void* p_context_switch_callback_param, DynamicString& error);

		void Stop();

		void Flush();

	private:
		static unsigned long __stdcall TracingThread_Static(LPVOID);

		void TracingThread();

		unsigned long GetEventInformation(PEVENT_RECORD p_event, PTRACE_EVENT_INFO& p_info);

		static VOID WINAPI EventCallback_Static(_In_ PEVENT_RECORD p_event);

		void EventCallback(PEVENT_RECORD p_event);

		//------------------------------------------------------------------------
		// data
	private:
		Allocator* mp_Allocator;

		TRACEHANDLE m_Session;
		TRACEHANDLE m_Consumer;

		CriticalSection m_CriticalSection;
		ContextSwitchCallback m_Callback;
		void* m_CallbackParam;

		HashMap<ThreadIdKey, int> m_ThreadProcessHashMap;

		char m_PropertiesBuffer[sizeof(EVENT_TRACE_PROPERTIES) + sizeof(KERNEL_LOGGER_NAME)];

		void* mp_EventInfoBuffer;
		int m_EventInfoBufferSize;
	};

	//------------------------------------------------------------------------
	EventTraceWin32Imp::EventTraceWin32Imp(Allocator* p_allocator)
	:	mp_Allocator(p_allocator),
		m_Session(0),
		m_Consumer(0),
		m_Callback(NULL),
		m_CallbackParam(NULL),
		m_ThreadProcessHashMap(p_allocator),
		mp_EventInfoBuffer(NULL),
		m_EventInfoBufferSize(0)
	{
		g_ShuttingDown = false;
	}

	//------------------------------------------------------------------------
	EventTraceWin32Imp::~EventTraceWin32Imp()
	{
		if(mp_EventInfoBuffer)
			mp_Allocator->Free(mp_EventInfoBuffer);
	}

	//------------------------------------------------------------------------
	unsigned long EventTraceWin32Imp::GetEventInformation(PEVENT_RECORD p_event, PTRACE_EVENT_INFO& p_info)
	{
		unsigned long buffer_size = 0;
		unsigned long status = TdhGetEventInformation(p_event, 0, NULL, p_info, &buffer_size);

		if (status == ERROR_INSUFFICIENT_BUFFER)
		{
			if((int)buffer_size > m_EventInfoBufferSize)
			{
				mp_Allocator->Free(mp_EventInfoBuffer);
				mp_EventInfoBuffer = mp_Allocator->Alloc(buffer_size);
				FRAMEPRO_ASSERT(mp_EventInfoBuffer);
				m_EventInfoBufferSize = buffer_size;
			}

			p_info = (TRACE_EVENT_INFO*)mp_EventInfoBuffer;

			status = TdhGetEventInformation(p_event, 0, NULL, p_info, &buffer_size);
		}

		return status;
	}

	//------------------------------------------------------------------------
	VOID WINAPI EventTraceWin32Imp::EventCallback_Static(_In_ PEVENT_RECORD p_event)
	{
		if(g_ShuttingDown)
			return;

		EventTraceWin32Imp* p_this = (EventTraceWin32Imp*)p_event->UserContext;
		p_this->EventCallback(p_event);
	}

	//------------------------------------------------------------------------
	void EventTraceWin32Imp::EventCallback(PEVENT_RECORD p_event)
	{
		CriticalSectionScope lock(m_CriticalSection);

		if(!m_Callback)
			return;

		PTRACE_EVENT_INFO p_info = NULL;
		unsigned long status = GetEventInformation(p_event, p_info);

		// check to see this is an MOF class and that it is the context switch event (36)
		if (status == ERROR_SUCCESS && DecodingSourceWbem == p_info->DecodingSource && p_event->EventHeader.EventDescriptor.Opcode == 36)
		{
			PROPERTY_DATA_DESCRIPTOR desc = {0};
			desc.ArrayIndex = ULONG_MAX;

			unsigned long result = 0;

			desc.PropertyName = (ULONGLONG)L"OldThreadId";
			int old_thread_id = 0;
			result = TdhGetProperty(p_event, 0, NULL, 1, &desc, sizeof(old_thread_id), (PBYTE)&old_thread_id);
			FRAMEPRO_ASSERT(result == ERROR_SUCCESS);

			desc.PropertyName = (ULONGLONG)L"NewThreadId";
			int new_thread_id = 0;
			result = TdhGetProperty(p_event, 0, NULL, 1, &desc, sizeof(new_thread_id), (PBYTE)&new_thread_id);
			FRAMEPRO_ASSERT(result == ERROR_SUCCESS);

			desc.PropertyName = (ULONGLONG)L"OldThreadState";
			char old_thread_state = 0;
			result = TdhGetProperty(p_event, 0, NULL, 1, &desc, sizeof(old_thread_state), (PBYTE)&old_thread_state);
			FRAMEPRO_ASSERT(result == ERROR_SUCCESS);

			desc.PropertyName = (ULONGLONG)L"OldThreadWaitReason";
			char old_thread_wait_reason = 0;
			result = TdhGetProperty(p_event, 0, NULL, 1, &desc, sizeof(old_thread_wait_reason), (PBYTE)&old_thread_wait_reason);
			FRAMEPRO_ASSERT(result == ERROR_SUCCESS);

			// the event header process id never seem to be set, so we work it out from the thread id
			int process_id = -1;
			int process_thread_id = new_thread_id ? new_thread_id : old_thread_id;
			if(process_thread_id)
			{
				if(!m_ThreadProcessHashMap.TryGetValue(process_thread_id, process_id))
				{
					HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, false, process_thread_id);
					if(thread)
					{
						process_id = GetProcessIdOfThread(thread);
						CloseHandle(thread);
					}

					m_ThreadProcessHashMap.Add(process_thread_id, process_id);
				}
			}

			ContextSwitch context_switch;
			context_switch.m_Timestamp = p_event->EventHeader.TimeStamp.QuadPart;
			context_switch.m_ProcessId = process_id;
#if _MSC_VER > 1600
			context_switch.m_CPUId = p_event->BufferContext.ProcessorIndex;
#else
			context_switch.m_CPUId = p_event->BufferContext.ProcessorNumber;
#endif
			context_switch.m_OldThreadId = old_thread_id;
			context_switch.m_NewThreadId = new_thread_id;
			context_switch.m_OldThreadState = (ThreadState::Enum)old_thread_state;
			context_switch.m_OldThreadWaitReason = (ThreadWaitReason::Enum)old_thread_wait_reason;

			m_Callback(context_switch, m_CallbackParam);
		}
	}

	//------------------------------------------------------------------------
	unsigned long __stdcall EventTraceWin32Imp::TracingThread_Static(LPVOID p_param)
	{
		EventTraceWin32Imp* p_this = (EventTraceWin32Imp*)p_param;
		p_this->TracingThread();
		return 0;
	}

	//------------------------------------------------------------------------
	void EventTraceWin32Imp::TracingThread()
	{
		FRAMEPRO_SET_THREAD_NAME("FramePro ETW Processing Thread");

		ProcessTrace(&m_Consumer, 1, 0, 0);
	}

	//------------------------------------------------------------------------
	void ErrorCodeToString(ULONG error_code, DynamicString& error_string)
	{
		switch (error_code)
		{
			case ERROR_BAD_LENGTH:
				error_string = "ERROR_BAD_LENGTH";
				break;

			case ERROR_INVALID_PARAMETER:
				error_string = "ERROR_INVALID_PARAMETER";
				break;

			case ERROR_ALREADY_EXISTS:
				error_string = "ERROR_ALREADY_EXISTS. Please check that there isn't another application running which is tracing context switches";
				break;

			case ERROR_BAD_PATHNAME:
				error_string = "ERROR_BAD_PATHNAME";
				break;

			case ERROR_DISK_FULL:
				error_string = "ERROR_DISK_FULL";
				break;

			case ERROR_ACCESS_DENIED:
				error_string = "ERROR_ACCESS_DENIED. Please make sure you are running your application with administrator privileges";
				break;

			default:
			{
				char temp[128];
// START EPIC 
				sprintf_s(temp, "Error code: %ld", error_code);
// END EPIC 
				error_string = temp;
			}
		}
	}

	//------------------------------------------------------------------------
	bool EventTraceWin32Imp::Start(ContextSwitchCallback p_context_switch_callback, void* p_context_switch_callback_param, DynamicString& error)
	{
		// only one kernal session allowed, so much stop any currently running session first
		Stop();

		{
			CriticalSectionScope lock(m_CriticalSection);
			m_Callback = p_context_switch_callback;
			m_CallbackParam = p_context_switch_callback_param;
		}

		// session name is stored at the end of the properties struct
		size_t properties_buffer_size = sizeof(m_PropertiesBuffer);
		char* p_properties_mem = m_PropertiesBuffer;
		memset(p_properties_mem, 0, properties_buffer_size);

		// initialise the session properties
		EVENT_TRACE_PROPERTIES* p_properties = (EVENT_TRACE_PROPERTIES*)p_properties_mem;

		p_properties->Wnode.BufferSize = (ULONG)properties_buffer_size;
		p_properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
		p_properties->Wnode.Guid = SystemTraceControlGuid;		// GUID for a NT Kernel Logger session
		p_properties->Wnode.ClientContext = 1;					// Clock resolution: use query performance counter (QPC)

		p_properties->EnableFlags = EVENT_TRACE_FLAG_CSWITCH;
		p_properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
		p_properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

		// start the new session
		ULONG result = StartTrace(&m_Session, KERNEL_LOGGER_NAME, p_properties);
		if (result != ERROR_SUCCESS)
		{
			ErrorCodeToString(result, error);
			return false;
		}

		// open the session
		EVENT_TRACE_LOGFILE log_file = {0};
		log_file.LoggerName = (FRAMEPRO_TCHAR*)KERNEL_LOGGER_NAME;
		log_file.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP | PROCESS_TRACE_MODE_REAL_TIME;
		log_file.EventRecordCallback = EventCallback_Static;
		log_file.Context = this;

		m_Consumer = OpenTrace(&log_file);
		if (m_Consumer == INVALID_PROCESSTRACE_HANDLE)
		{
			error = "OpenTrace() failed";
			return false;
		}

		// start the processing thread
		HANDLE thread = CreateThread(0, 0, TracingThread_Static, this, 0, NULL);
		CloseHandle(thread);

		return true;
	}

	//------------------------------------------------------------------------
	void EventTraceWin32Imp::Stop()
	{
		size_t properties_buffer_size = sizeof(m_PropertiesBuffer);
		char* p_properties_mem = m_PropertiesBuffer;
		memset(p_properties_mem, 0, properties_buffer_size);

		EVENT_TRACE_PROPERTIES* p_properties = (EVENT_TRACE_PROPERTIES*)p_properties_mem;

		p_properties->Wnode.BufferSize = (ULONG)properties_buffer_size;
		p_properties->Wnode.Guid = SystemTraceControlGuid;		// GUID for a NT Kernel Logger session
		p_properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

		_tcscpy_s((FRAMEPRO_TCHAR*)((char*)p_properties + p_properties->LoggerNameOffset), sizeof(KERNEL_LOGGER_NAME)/sizeof(FRAMEPRO_TCHAR), KERNEL_LOGGER_NAME);

		// stop any old sessions that were not stopped
		ControlTrace(0, KERNEL_LOGGER_NAME, p_properties, EVENT_TRACE_CONTROL_STOP);

		m_Session = 0;

		if(m_Consumer)
		{
			CloseTrace(m_Consumer);
			m_Consumer = 0;
		}

		{
			CriticalSectionScope lock(m_CriticalSection);
			m_Callback = NULL;
			m_CallbackParam = NULL;
		}
	}

	//------------------------------------------------------------------------
	void EventTraceWin32Imp::Flush()
	{
		if(!m_Session)
			return;

		size_t properties_buffer_size = sizeof(m_PropertiesBuffer);
		char* p_properties_mem = m_PropertiesBuffer;
		memset(p_properties_mem, 0, properties_buffer_size);

		EVENT_TRACE_PROPERTIES* p_properties = (EVENT_TRACE_PROPERTIES*)p_properties_mem;

		p_properties->Wnode.BufferSize = (ULONG)properties_buffer_size;
		p_properties->Wnode.Guid = SystemTraceControlGuid;		// GUID for a NT Kernel Logger session
		p_properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

		_tcscpy_s((FRAMEPRO_TCHAR*)((char*)p_properties + p_properties->LoggerNameOffset), sizeof(KERNEL_LOGGER_NAME)/sizeof(FRAMEPRO_TCHAR), KERNEL_LOGGER_NAME);

		#if FRAMEPRO_DEBUG
			ULONG result = ControlTrace(m_Session, NULL, p_properties, EVENT_TRACE_CONTROL_FLUSH);
			FRAMEPRO_ASSERT(result == ERROR_SUCCESS);
		#else
			ControlTrace(m_Session, NULL, p_properties, EVENT_TRACE_CONTROL_FLUSH);
		#endif
	}
}
#endif		// #if FRAMEPRO_EVENT_TRACE_WIN32

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	EventTraceWin32::EventTraceWin32(Allocator* p_allocator)
#if FRAMEPRO_EVENT_TRACE_WIN32
	:	mp_Imp(New<EventTraceWin32Imp>(p_allocator, p_allocator)),
#else
	:	mp_Imp(NULL),
#endif
		mp_Allocator(p_allocator)
	{
	}

	//------------------------------------------------------------------------
	EventTraceWin32::~EventTraceWin32()
	{
#if FRAMEPRO_EVENT_TRACE_WIN32
		g_ShuttingDown = true;

		Delete(mp_Allocator, mp_Imp);
#endif
	}

	//------------------------------------------------------------------------
	bool EventTraceWin32::Start(ContextSwitchCallback p_context_switch_callback, void* p_context_switch_callback_param, DynamicString& error)
	{
#if FRAMEPRO_EVENT_TRACE_WIN32
		return mp_Imp->Start(p_context_switch_callback, p_context_switch_callback_param, error);
#else
		return false;
#endif
	}

	//------------------------------------------------------------------------
	void EventTraceWin32::Stop()
	{
#if FRAMEPRO_EVENT_TRACE_WIN32
		mp_Imp->Stop();
#endif
	}

	//------------------------------------------------------------------------
	void EventTraceWin32::Flush()
	{
#if FRAMEPRO_EVENT_TRACE_WIN32
		mp_Imp->Flush();
#endif
	}
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED
//------------------------------------------------------------------------
// FramePro.cpp


//------------------------------------------------------------------------
// FrameProTLS.hpp
#ifndef FRAMEPROTLS_H_INCLUDED
#define FRAMEPROTLS_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------

//------------------------------------------------------------------------
// Socket.hpp
#ifndef FRAMEPRO_SOCKET_H_INCLUDED
#define FRAMEPRO_SOCKET_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED

//------------------------------------------------------------------------
#if FRAMEPRO_WIN_BASED_PLATFORM
	#pragma warning(push)
	#pragma warning(disable : 4100)
#endif

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	class SocketImp;

	//------------------------------------------------------------------------
	class Socket
	{
	public:
		inline Socket();

		inline ~Socket();

		void Disconnect();

		bool Bind(const char* p_port);

		bool StartListening();

		bool Accept(Socket& client_socket);

		int Receive(void* p_buffer, int size);

		bool Send(const void* p_buffer, size_t size);

		inline bool IsValid() const { return m_Socket != FRAMEPRO_INVALID_SOCKET; }

		static void HandleError();

	private:
		bool InitialiseWSA();

		void CleanupWSA();

		//------------------------------------------------------------------------
		// data
		FRAMEPRO_SOCKET m_Socket;

		bool m_Listening;
	};

	//------------------------------------------------------------------------
	Socket::Socket()
	:	m_Socket(FRAMEPRO_INVALID_SOCKET),
		m_Listening(false)
	{
	}

	//------------------------------------------------------------------------
	Socket::~Socket()
	{
		CleanupWSA();
	}
}

//------------------------------------------------------------------------
#if FRAMEPRO_WIN_BASED_PLATFORM
	#pragma warning(pop)
#endif

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_SOCKETS_ENABLED

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPRO_SOCKET_H_INCLUDED

//------------------------------------------------------------------------
// Packets.hpp
#ifndef PACKETS_H_INCLUDED
#define PACKETS_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	namespace PacketType
	{
		enum Enum
		{
			Connect = 0xaabb,
			FrameStart,
			TimeSpan,
			TimeSpanW,
			NamedTimeSpan,
			StringLiteralNamedTimeSpan,
			ThreadName,
			ThreadOrder,
			StringPacket,
			WStringPacket,
			NameAndSourceInfoPacket,
			NameAndSourceInfoPacketW,
			SourceInfoPacket,
			MainThreadPacket,
			RequestStringLiteralPacket,
			SetConditionalScopeMinTimePacket,
			ConnectResponsePacket,
			SessionInfoPacket,
			RequestRecordedDataPacket,
			SessionDetailsPacket,
			ContextSwitchPacket,
			ContextSwitchRecordingStartedPacket,
			ProcessNamePacket,
			CustomStatPacket,
			StringLiteralTimerNamePacket,
			HiResTimerScopePacket,
			LogPacket,
			EventPacket,
			StartWaitEventPacket,
			StopWaitEventPacket,
			TriggerWaitEventPacket,
			TimeSpanCustomStatPacket,
			TimeSpanWithCallstack,
			TimeSpanWWithCallstack,
			NamedTimeSpanWithCallstack,
			StringLiteralNamedTimeSpanWithCallstack,
			ModulePacket,
			SetCallstackRecordingEnabledPacket,
		};
	};

	//------------------------------------------------------------------------
	// send packets
	//------------------------------------------------------------------------

	//------------------------------------------------------------------------
	#ifdef __clang__
		#pragma clang diagnostic push
		#pragma clang diagnostic ignored "-Wunused-private-field"
	#endif

	//------------------------------------------------------------------------
	namespace Platform
	{
		enum Enum
		{
			Windows = 0,
			Windows_UWP,
			XBoxOne,
			XBox360,
			Unix,
		};
	}

	//------------------------------------------------------------------------
	namespace CustomStatValueType
	{
		enum Enum
		{
			Int64 = 0,
			Double,
		};
	}

	//------------------------------------------------------------------------
	struct ConnectPacket
	{
		ConnectPacket(int64 clock_frequency, int process_id, Platform::Enum platform)
		:	m_PacketType(PacketType::Connect),
			m_Version(g_FrameProLibVersion),
			m_ClockFrequency(clock_frequency),
			m_ProcessId(process_id),
			m_Platform(platform)
		{
		}

	private:
		PacketType::Enum m_PacketType;
		int m_Version;
		int64 m_ClockFrequency;
		int m_ProcessId;
		Platform::Enum m_Platform;
	};

	//------------------------------------------------------------------------
	struct SessionDetailsPacket
	{
		SessionDetailsPacket(StringId name, StringId build_id, StringId date)
		:	m_PacketType(PacketType::SessionDetailsPacket),
			m_Padding(0),
			m_Name(name),
			m_BuildId(build_id),
			m_Date(date)
		{
		}

	private:
		PacketType::Enum m_PacketType;
		int m_Padding;
		StringId m_Name;
		StringId m_BuildId;
		StringId m_Date;
	};

	//------------------------------------------------------------------------
	struct TimeSpanPacket
	{
		int m_PacketType_AndCore;
		int m_ThreadId;
		StringId m_NameAndSourceInfo;
		int64 m_StartTime;
		int64 m_EndTime;
	};

	//------------------------------------------------------------------------
	struct TimeSpanCustomStatPacket
	{
		int m_PacketType;
		int m_ThreadId;
		int m_ValueType;	// CustomStatValueType enum
		int m_Padding;
		StringId m_Name;
		StringId m_Unit;
		int64 m_ValueInt64;
		double m_ValueDouble;
		int64 m_Time;
	};

	//------------------------------------------------------------------------
	struct NamedTimeSpanPacket
	{
		int m_PacketType_AndCore;
		int m_ThreadId;
		int64 m_Name;
		StringId m_SourceInfo;
		int64 m_StartTime;
		int64 m_EndTime;
	};

	//------------------------------------------------------------------------
	struct FrameStartPacket
	{
		FrameStartPacket(int64 frame_start_time, int64 wait_for_send_complete_time)
		:	m_PacketType(PacketType::FrameStart),
			m_Legacy1(0),
			m_Legacy2(0),
			m_Padding(0xffffffff),
			m_FrameStartTime(frame_start_time),
			m_WaitForSendCompleteTime(wait_for_send_complete_time),
			m_Legacy4(0)
		{
		}

	private:
		PacketType::Enum m_PacketType;
		int m_Legacy1;
		int m_Legacy2;
		int m_Padding;
		int64 m_FrameStartTime;
		int64 m_WaitForSendCompleteTime;
		int64 m_Legacy4;
	};

	//------------------------------------------------------------------------
	struct ThreadNamePacket
	{
	public:
		ThreadNamePacket(int thread_id, int64 name)
		:	m_PacketType(PacketType::ThreadName),
			m_ThreadID(thread_id),
			m_Name(name)
		{
		}

	private:
		PacketType::Enum m_PacketType;
		int m_ThreadID;
		int64 m_Name;
	};

	//------------------------------------------------------------------------
	struct ThreadOrderPacket
	{
	public:
		ThreadOrderPacket(StringId thread_name)
		:	m_PacketType(PacketType::ThreadOrder),
			m_Padding(0xffffffff),
			m_ThreadName(thread_name)
		{
		}

	private:
		PacketType::Enum m_PacketType;
		int m_Padding;
		StringId m_ThreadName;
	};

	//------------------------------------------------------------------------
	struct StringPacket
	{
		PacketType::Enum m_PacketType;
		int m_Length;			// length in chars
		StringId m_StringId;
		// name string follows in buffer
	};

	//------------------------------------------------------------------------
	struct MainThreadPacket
	{
		MainThreadPacket(int thread_id)
		:	m_PacketType(PacketType::MainThreadPacket),
			m_ThreadId(thread_id)
		{
		}

	private:
		PacketType::Enum m_PacketType;
		int m_ThreadId;
	};

	//------------------------------------------------------------------------
	struct SessionInfoPacket
	{
		SessionInfoPacket()
		:	m_PacketType(PacketType::SessionInfoPacket),
			m_Padding(0xffffffff),
			m_SendBufferSize(0),
			m_StringMemorySize(0),
			m_MiscMemorySize(0),
			m_RecordingFileSize(0)
		{
		}

		PacketType::Enum m_PacketType;
		int m_Padding;
		int64 m_SendBufferSize;
		int64 m_StringMemorySize;
		int64 m_MiscMemorySize;
		int64 m_RecordingFileSize;
	};

	//------------------------------------------------------------------------
	struct ContextSwitchPacket
	{
		PacketType::Enum m_PacketType;
		int m_CPUId;
		int64 m_Timestamp;
		int m_ProcessId;
		int m_OldThreadId;
		int m_NewThreadId;
		int m_OldThreadState;
		int m_OldThreadWaitReason;
		int m_Padding;
	};

	//------------------------------------------------------------------------
	struct ContextSwitchRecordingStartedPacket
	{
		PacketType::Enum m_PacketType;
		int m_StartedSucessfully;		// bool
		char m_Error[FRAMEPRO_MAX_INLINE_STRING_LENGTH];
	};

	//------------------------------------------------------------------------
	struct ProcessNamePacket
	{
		ProcessNamePacket(int process_id, int64 name_id)
		:	m_PacketType(PacketType::ProcessNamePacket),
			m_ProcessId(process_id),
			m_NameId(name_id)
		{
		}

		PacketType::Enum m_PacketType;
		int m_ProcessId;
		int64 m_NameId;
	};

	//------------------------------------------------------------------------
	struct CustomStatPacketInt64
	{
		uint m_PacketTypeAndValueType;
		int m_Count;
		StringId m_Name;
		int64 m_Value;
		StringId m_Graph;
		StringId m_Unit;
	};

	//------------------------------------------------------------------------
	struct CustomStatPacketDouble
	{
		uint m_PacketTypeAndValueType;
		int m_Count;
		StringId m_Name;
		double m_Value;
		StringId m_Graph;
		StringId m_Unit;
	};

	//------------------------------------------------------------------------
	struct HiResTimerScopePacket
	{
		PacketType::Enum m_PacketType;
		int m_Padding;
		int64 m_StartTime;
		int64 m_EndTime;
		int m_Count;
		int m_ThreadId;
		// array of HiResTimer follows

		struct HiResTimer
		{
			StringId m_Name;
			int64 m_Duration;
			int64 m_Count;
		};
	};

	//------------------------------------------------------------------------
	struct LogPacket
	{
		PacketType::Enum m_PacketType;
		int m_Length;			// length in chars
		int64 m_Time;
		// name string follows in buffer
	};

	//------------------------------------------------------------------------
	struct EventPacket
	{
		PacketType::Enum m_PacketType;
		uint m_Colour;
		StringId m_Name;
		int64 m_Time;
	};

	//------------------------------------------------------------------------
	struct WaitEventPacket
	{
		PacketType::Enum m_PacketType;
		int m_Thread;
		int m_Core;
		int m_Padding;
		int64 m_EventId;
		int64 m_Time;
	};

	//------------------------------------------------------------------------
	struct CallstackPacket
	{
		// we don't have a packet type here because it always follows a time span packet
		int m_CallstackId;
		int m_CallstackSize;	// size of the callstack that follows in the send buffer, or 0 if we have already sent this callstack
	};

	//------------------------------------------------------------------------
	struct ModulePacket
	{
		PacketType::Enum m_PacketType;
		int m_UseLookupFunctionForBaseAddress;
		int64 m_ModuleBase;
		char m_Sig[16];
		int m_Age;
		int m_Padding;
		char m_ModuleName[FRAMEPRO_MAX_INLINE_STRING_LENGTH];
		char m_SymbolFilename[FRAMEPRO_MAX_INLINE_STRING_LENGTH];
	};

	//------------------------------------------------------------------------
	// receive packets
	//------------------------------------------------------------------------

	//------------------------------------------------------------------------
	struct RequestStringLiteralPacket
	{
		StringId m_StringId;
		int m_StringLiteralType;
		int m_Padding;
	};

	//------------------------------------------------------------------------
	struct SetConditionalScopeMinTimePacket
	{
		int m_MinTime;
	};

	//------------------------------------------------------------------------
	struct ConnectResponsePacket
	{
		int m_Interactive;
		int m_RecordContextSwitches;
	};

	//------------------------------------------------------------------------
	struct RequestRecordedDataPacket
	{
	};

	//------------------------------------------------------------------------
	struct SetCallstackRecordingEnabledPacket
	{
		int m_Enabled;
	};

	//------------------------------------------------------------------------
	#ifdef __clang__
		#pragma clang diagnostic pop
	#endif
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef PACKETS_H_INCLUDED

//------------------------------------------------------------------------
// PointerSet.hpp
#ifndef FRAMEPRO_POINTERSET_H_INCLUDED
#define FRAMEPRO_POINTERSET_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#define FRAMEPRO_PRIME 0x01000193

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	class Allocator;

	//------------------------------------------------------------------------
	class PointerSet
	{
	public:
		PointerSet(Allocator* p_allocator);

		~PointerSet();

		size_t GetMemorySize() const { return m_Capacity * sizeof(const void*); }

		//------------------------------------------------------------------------
		// return true if added, false if already in set
		FRAMEPRO_FORCE_INLINE bool Add(const void* p)
		{
#if FRAMEPRO_X64
			unsigned int hash = (unsigned int)((unsigned long long)p * 18446744073709551557UL);
#else
			unsigned int hash = (unsigned int)p * 4294967291;
#endif
			int index = hash & m_CapacityMask;

			// common case handled inline
			const void* p_existing = mp_Data[index];
			if(p_existing == p)
				return false;

			return AddInternal(p, hash, index);
		}

	private:
		void Grow();

		bool AddInternal(const void* p, int64 hash, int index);

		//------------------------------------------------------------------------
		// data
	private:
		const void** mp_Data;
		unsigned int m_CapacityMask;
		int m_Count;
		int m_Capacity;

		Allocator* mp_Allocator;
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPRO_POINTERSET_H_INCLUDED



//------------------------------------------------------------------------
// SendBuffer.hpp
#ifndef SENDBUFFER_H_INCLUDED
#define SENDBUFFER_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	class Allocator;
	class FrameProTLS;

	//------------------------------------------------------------------------
	class SendBuffer
	{
	public:
		SendBuffer(Allocator* p_allocator, int capacity, FrameProTLS* p_owner);

		~SendBuffer();

		const void* GetBuffer() const { return mp_Buffer; }

		void AllocateBuffer(int capacity);

		void ClearBuffer();

		void ClearSize() { m_Size = 0; }

		int GetSize() const { return m_Size; }

		int GetCapacity() const { return m_Capacity; }

		SendBuffer* GetNext() const { return mp_Next; }

		void SetNext(SendBuffer* p_next) { mp_Next = p_next; }

		void Swap(void*& p_buffer, int& size, int capacity)
		{
			FramePro::Swap(mp_Buffer, p_buffer);
			FramePro::Swap(m_Size, size);
			m_Capacity = capacity;
		}

		void Swap(SendBuffer* p_send_buffer)
		{
			FramePro::Swap(mp_Buffer, p_send_buffer->mp_Buffer);
			FramePro::Swap(m_Size, p_send_buffer->m_Size);
			FramePro::Swap(m_Capacity, p_send_buffer->m_Capacity);
		}

		FrameProTLS* GetOwner() { return mp_Owner; }

		int64 GetCreationTime() const { return m_CreationTime; }

		void SetCreationTime();

		//------------------------------------------------------------------------
		// data
	private:
		void* mp_Buffer;
		int m_Size;

		int m_Capacity;

		SendBuffer* mp_Next;

		Allocator* mp_Allocator;

		FrameProTLS* mp_Owner;

		int64 m_CreationTime;
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef SENDBUFFER_H_INCLUDED



//------------------------------------------------------------------------
// Buffer.hpp
#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//--------------------------------------------------------------------
	class Buffer
	{
	public:
		Buffer()
		:	mp_Buffer(NULL),
			m_Size(0),
			m_Capacity(0),
			mp_Allocator(NULL)
		{
		}

		Buffer(Allocator* p_allocator)
			:	mp_Buffer(NULL),
			m_Size(0),
			m_Capacity(0),
			mp_Allocator(p_allocator)
		{
		}

		~Buffer()
		{
			if(mp_Buffer)
				mp_Allocator->Free(mp_Buffer);
		}

		void SetAllocator(Allocator* p_allocator) { mp_Allocator = p_allocator; }

		void* GetBuffer() const { return mp_Buffer; }

		int GetSize() const { return m_Size; }

		int GetMemorySize() const { return m_Capacity; }

		void Clear()
		{
			m_Size = 0;
		}

		void ClearAndFree()
		{
			Clear();

			if(mp_Buffer)
			{
				mp_Allocator->Free(mp_Buffer);
				mp_Buffer = NULL;
			}
		}

		void* Allocate(int size)
		{
			int old_size = m_Size;
			int new_size = old_size + size;
			if(new_size > m_Capacity)
			{
				int double_capacity = 2*m_Capacity;
				Resize(double_capacity > new_size ? double_capacity : new_size);
			}
			void* p = (char*)mp_Buffer + old_size;
			m_Size = new_size;

			return p;
		}

	private:
		void Resize(int new_capacity)
		{
			void* p_new_buffer = mp_Allocator->Alloc(new_capacity);

			int current_size = m_Size;
			if(current_size)
				memcpy(p_new_buffer, mp_Buffer, current_size);

			mp_Allocator->Free(mp_Buffer);
			mp_Buffer = p_new_buffer;

			m_Capacity = new_capacity;
		}

		//------------------------------------------------------------------------
		// data
	private:
		void* mp_Buffer;
		
		int m_Size;
		int m_Capacity;

		Allocator* mp_Allocator;
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef BUFFER_H_INCLUDED

//------------------------------------------------------------------------
// ConditionalParentScope.hpp
#ifndef FRAMEPRO_CONDITIONALPARENTSCOPE_H_INCLUDED
#define FRAMEPRO_CONDITIONALPARENTSCOPE_H_INCLUDED

//------------------------------------------------------------------------



//------------------------------------------------------------------------
// List.hpp
#ifndef FRAMEPRO_LIST_H_INCLUDED
#define FRAMEPRO_LIST_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	template<typename T>
	class List
	{
	public:
		//------------------------------------------------------------------------
		List()
		:	mp_Head(NULL),
			mp_Tail(NULL)
		{
		}

		//------------------------------------------------------------------------
		bool IsEmpty() const
		{
			return mp_Head == NULL;
		}

		//------------------------------------------------------------------------
		void Clear()
		{
			mp_Head = mp_Tail = NULL;
			CheckState();
		}

		//------------------------------------------------------------------------
		T* GetHead()
		{
			return mp_Head;
		}

		//------------------------------------------------------------------------
		const T* GetHead() const
		{
			return mp_Head;
		}

		//------------------------------------------------------------------------
		void AddHead(T* p_item)
		{
			FRAMEPRO_ASSERT(!p_item->GetNext());
			p_item->SetNext(mp_Head);
			mp_Head = p_item;
			if(!mp_Tail)
				mp_Tail = p_item;

			CheckState();
		}

		//------------------------------------------------------------------------
		T* RemoveHead()
		{
			T* p_item = mp_Head;
			T* p_new_head = p_item->GetNext();
			mp_Head = p_new_head;
			p_item->SetNext(NULL);
			if(!p_new_head)
				mp_Tail = NULL;
			CheckState();
			return p_item;
		}

		//------------------------------------------------------------------------
		void AddTail(T* p_item)
		{
			FRAMEPRO_ASSERT(!p_item->GetNext());

			if(mp_Tail)
			{
				FRAMEPRO_ASSERT(mp_Head);
				mp_Tail->SetNext(p_item);
			}
			else
			{
				mp_Head = p_item;
			}

			mp_Tail = p_item;

			CheckState();
		}

		//------------------------------------------------------------------------
		void MoveAppend(List<T>& list)
		{
			if(list.IsEmpty())
				return;

			T* p_head = list.GetHead();

			if(mp_Tail)
				mp_Tail->SetNext(p_head);
			else
				mp_Head = p_head;

			mp_Tail = list.mp_Tail;

			list.Clear();
			list.CheckState();

			CheckState();
		}

		//------------------------------------------------------------------------
		void Remove(T* p_item)
		{
			T* p_prev = NULL;
			for(T* p_iter=mp_Head; p_iter && p_iter!=p_item; p_iter=p_iter->GetNext())
				p_prev = p_iter;
			FRAMEPRO_ASSERT(!p_prev || p_prev->GetNext());
			
			T* p_next = p_item->GetNext();
			if(p_prev)
				p_prev->SetNext(p_item->GetNext());
			else
				mp_Head = p_next;

			if(mp_Tail == p_item)
				mp_Tail = p_prev;

			p_item->SetNext(NULL);

			CheckState();
		}

		//------------------------------------------------------------------------
		void CheckState()
		{
			FRAMEPRO_ASSERT((!mp_Head && !mp_Tail) || (mp_Head && mp_Tail));

			#if FRAMEPRO_DEBUG
				T* p_tail = mp_Head;
				while (p_tail && p_tail->GetNext())
					p_tail = p_tail->GetNext();
				FRAMEPRO_ASSERT(mp_Tail == p_tail);
			#endif
		}

		//------------------------------------------------------------------------
		// data
	private:
		T* mp_Head;
		T* mp_Tail;
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPRO_LIST_H_INCLUDED

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	class ConditionalParentScope
	{
	public:
		ConditionalParentScope(const char* p_name)
		:	mp_Name(p_name),
			m_PreDuration(0),
			m_PostDuration(0),
			mp_SendBuffer(NULL),
			mp_Next(NULL),
			m_LastPopConditionalChildrenTime(0)
		{
		}

		ConditionalParentScope* GetNext() const { return mp_Next; }

		void SetNext(ConditionalParentScope* p_next) { mp_Next = p_next; }

		// data
		const char* mp_Name;
		int64 m_PreDuration;					// in ms
		int64 m_PostDuration;					// in ms
		SendBuffer* mp_SendBuffer;					// only accessed by TLS thread
		List<SendBuffer> m_ChildSendBuffers;		// accessed from multiple threads
		ConditionalParentScope* mp_Next;
		int64 m_LastPopConditionalChildrenTime;
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPRO_CONDITIONALPARENTSCOPE_H_INCLUDED


//------------------------------------------------------------------------
// Array.hpp
#ifndef ARRAY_H_INCLUDED
#define ARRAY_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//--------------------------------------------------------------------
	template<typename T>
	class Array
	{
	public:
		//--------------------------------------------------------------------
		Array()
		:	mp_Array(NULL),
			m_Count(0),
			m_Capacity(0),
			mp_Allocator(NULL)
		{
		}

		//--------------------------------------------------------------------
		~Array()
		{
			FRAMEPRO_ASSERT(!mp_Array);
		}

		//--------------------------------------------------------------------
		int GetCount() const
		{
			return m_Count;
		}

		//--------------------------------------------------------------------
		void Clear()
		{
			if(mp_Array)
			{
				mp_Allocator->Free(mp_Array);
				mp_Array = NULL;
			}

			m_Count = 0;
			m_Capacity = 0;
		}

		//--------------------------------------------------------------------
		void ClearNoFree()
		{
			m_Count = 0;
		}

		//--------------------------------------------------------------------
		void SetAllocator(Allocator* p_allocator)
		{
			FRAMEPRO_ASSERT(mp_Allocator == p_allocator || !(mp_Allocator != NULL && p_allocator != NULL));
			mp_Allocator = p_allocator;
		}

		//--------------------------------------------------------------------
		void Add(const T& value)
		{
			if(m_Count == m_Capacity)
				Grow();

			mp_Array[m_Count] = value;
			++m_Count;
		}

		//--------------------------------------------------------------------
		const T& operator[](int index) const
		{
			FRAMEPRO_ASSERT(index >= 0 && index < m_Count);
			return mp_Array[index];
		}

		//--------------------------------------------------------------------
		T& operator[](int index)
		{
			FRAMEPRO_ASSERT(index >= 0 && index < m_Count);
			return mp_Array[index];
		}

		//--------------------------------------------------------------------
		void RemoveAt(int index)
		{
			FRAMEPRO_ASSERT(index >= 0 && index < m_Count);
		
			if(index < m_Count - 1)
				memmove(mp_Array + index, mp_Array + index + 1, (m_Count - 1 - index) * sizeof(T));

			--m_Count;
		}

		//--------------------------------------------------------------------
		T RemoveLast()
		{
			FRAMEPRO_ASSERT(m_Count);

			return mp_Array[--m_Count];
		}

		//--------------------------------------------------------------------
		bool Contains(const T& value) const
		{
			for(int i=0; i<m_Count; ++i)
				if(mp_Array[i] == value)
					return true;
			return false;
		}

	private:
		//--------------------------------------------------------------------
		void Grow()
		{
			m_Capacity = m_Capacity ? 2*m_Capacity : 32;
			T* p_new_array = (T*)mp_Allocator->Alloc(sizeof(T)*m_Capacity);
			if(mp_Array)
				memcpy(p_new_array, mp_Array, sizeof(T)*m_Count);
			mp_Allocator->Free(mp_Array);
			mp_Array = p_new_array;
		}

		//------------------------------------------------------------------------
		// data
	private:
		T* mp_Array;
		
		int m_Count;
		int m_Capacity;

		Allocator* mp_Allocator;
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef ARRAY_H_INCLUDED

//------------------------------------------------------------------------
// FrameProStackTrace.hpp
#ifndef FRAMEPRO_STACKTRACE_H_INCLUDED
#define FRAMEPRO_STACKTRACE_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
// FrameProCallstackSet.hpp
#ifndef FRAMEPRO_CALLSTACKSET_H_INCLUDED
#define FRAMEPRO_CALLSTACKSET_H_INCLUDED

//------------------------------------------------------------------------



//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	struct Callstack
	{
		uint64* mp_Stack;
		int m_ID;
		int m_Size;
		unsigned int m_Hash;
	};

	//------------------------------------------------------------------------
	// A hash set collection for Callstack structures. Callstacks are added and
	// retreived using the stack address array as the key.
	// This class only allocates memory using virtual alloc/free to avoid going
	// back into the mian allocator.
	class CallstackSet
	{
	public:
		CallstackSet(Allocator* p_allocator);

		~CallstackSet();

		Callstack* Get(uint64* p_stack, int stack_size, unsigned int hash);

		Callstack* Add(uint64* p_stack, int stack_size, unsigned int hash);

		void Clear();

	private:
		void Grow();

		void Add(Callstack* p_callstack);

		//------------------------------------------------------------------------
		// data
	private:
		Callstack** mp_Data;
		unsigned int m_CapacityMask;
		int m_Count;
		int m_Capacity;

		Allocator* mp_Allocator;
		IncrementingBlockAllocator m_BlockAllocator;
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPRO_CALLSTACKSET_H_INCLUDED

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLE_CALLSTACKS

//------------------------------------------------------------------------
#define FRAMEPRO_STACK_TRACE_SIZE 128

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	struct CallstackResult
	{
		Callstack* mp_Callstack;
		bool m_IsNew;
	};

	//------------------------------------------------------------------------
	class StackTrace
	{
	public:
		StackTrace(Allocator* p_allocator);

		void Clear();

		CallstackResult Capture();

		//------------------------------------------------------------------------
		// data
	private:
		void* m_Stack[FRAMEPRO_STACK_TRACE_SIZE];

		int m_StackCount;
		unsigned int m_StackHash;

		CallstackSet m_CallstackSet;
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLE_CALLSTACKS

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPRO_STACKTRACE_H_INCLUDED
#include <atomic>

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	class FrameProTLS
	{
		struct HiResTimer
		{
			const char* mp_Name;
			int64 m_Duration;
			int64 m_Count;
		};

		struct HiResTimerList
		{
			Array<HiResTimer> m_Timers;

			HiResTimerList* mp_Prev;
			HiResTimerList* mp_Next;

			HiResTimerList* GetPrev() { return mp_Prev; }
			HiResTimerList* GetNext() { return mp_Next; }
			void SetPrev(HiResTimerList* p_prev) { mp_Prev = p_prev; }
			void SetNext(HiResTimerList* p_next) { mp_Next = p_next; }
		};

	public:
		FrameProTLS(Allocator* p_allocator, int64 clock_frequency);

		~FrameProTLS();

		//---------------------------------------------------
		// these functions are called from the main thread, so care needs to be taken with thread safety

		void OnConnected(bool recording_to_file);

		void OnDisconnected();

		void SendSessionInfoBuffer();

		void OnFrameStart();

		void FlushSendBuffers();

		void LockSessionInfoBuffer() { m_SessionInfoBufferLock.Enter(); }
		
		void UnlockSessionInfoBuffer() { m_SessionInfoBufferLock.Leave(); }

		void SetInteractive(bool value)
		{
			m_Interactive = value;
			UpdateSendStringsImmediatelyFlag();
		}

		FrameProTLS* GetNext() const { return mp_Next; }

		void SetNext(FrameProTLS* p_next) { mp_Next = p_next; }

		size_t GetStringMemorySize() const { return m_StringMemorySize + m_LiteralStringSetMemorySize; }

		size_t GetSendBufferMemorySize() const { return m_SendBufferMemorySize + m_SessionInfoBufferMemorySize; }

		//---------------------------------------------------
		// these function are only called from the TLS thread

		FRAMEPRO_FORCE_INLINE int GetThreadId() const { return m_ThreadId; }

		FRAMEPRO_FORCE_INLINE bool IsInteractive() const { return m_Interactive; }

		FRAMEPRO_FORCE_INLINE void* AllocateSpaceInBuffer(int size)
		{
			FRAMEPRO_ASSERT(m_CurrentSendBufferCriticalSection.Locked());
			FRAMEPRO_ASSERT(IsOnTLSThread() || !g_Connected);		// can only be accessed from TLS thread, unless we haven't connected yet
			FRAMEPRO_ASSERT(size <= m_SendBufferCapacity);

			if(m_CurrentSendBufferSize + size >= m_SendBufferCapacity)
				FlushCurrentSendBuffer_no_lock();

			void* p = (char*)mp_CurrentSendBuffer + m_CurrentSendBufferSize;
			m_CurrentSendBufferSize += size;
			return p;
		}

		template<typename T>
		FRAMEPRO_FORCE_INLINE T* AllocateSpaceInBuffer()
		{
			return (T*)AllocateSpaceInBuffer(sizeof(T));
		}
		
		void SetThreadName(int thread_id, const char* p_name);

		void SetThreadOrder(StringId thread_name);

		void SetMainThread(int main_thraed_id);

		StringId RegisterString(const char* p_str);

		StringId RegisterString(const wchar_t* p_str);

		FRAMEPRO_NO_INLINE void SendString(const char* p_string, PacketType::Enum packet_type);

		FRAMEPRO_NO_INLINE void SendString(const wchar_t* p_string, PacketType::Enum packet_type);

		void SendFrameStartPacket(int64 wait_for_send_complete_time);

		void SendConnectPacket(int64 clock_frequency, int process_id, Platform::Enum platform);

		void SendStringLiteral(StringLiteralType::Enum string_literal_type, StringId string_id);

		void Send(const void* p_data, int size);

		bool SendStringsImmediately() const { return m_SendStringsImmediately; }

		void CollectSendBuffers(List<SendBuffer>& list);

		void AddEmptySendBuffer(SendBuffer* p_send_buffer);

		template<class PacketT>
		void SendSessionInfoPacket(const PacketT& packet)
		{
			SendSessionInfo(&packet, sizeof(packet));
		}

		template<class T> FRAMEPRO_FORCE_INLINE void SendPacket(const T& packet) { Send(&packet, sizeof(packet)); }

		CriticalSection& GetCurrentSendBufferCriticalSection() { return m_CurrentSendBufferCriticalSection; }

		void Shutdown() { m_ShuttingDown = true; }

		bool ShuttingDown() const { return m_ShuttingDown; }

		FRAMEPRO_NO_INLINE void FlushCurrentSendBuffer();

		void PushConditionalParentScope(const char* p_name, int64 pre_duration, int64 post_duration);
		
		void PopConditionalParentScope(bool add_children);

		void SendLogPacket(const char* p_message);

		void SendEventPacket(const char* p_name, uint colour);

		FRAMEPRO_FORCE_INLINE void StartHiResTimer(const char* p_name)
		{
			FRAMEPRO_ASSERT(IsOnTLSThread());

			// try and find the timer of the specified name
			int count = m_HiResTimers.GetCount();
			HiResTimer* p_timer = NULL;
			int i;
			for (i = 0; i<count; ++i)
			{
				HiResTimer* p_timer_tier = &m_HiResTimers[i];
				if (p_timer_tier->mp_Name == p_name)
				{
					p_timer = p_timer_tier;
					break;
				}
			}

			// add the timer if not found
			if (!p_timer)
			{
				HiResTimer hires_timer;
				hires_timer.mp_Name = p_name;
				hires_timer.m_Duration = 0;
				hires_timer.m_Count = 0;
				m_HiResTimers.Add(hires_timer);
			}

			// remember the current active timer and set this timer as the new active timer
			int current_index = m_ActiveHiResTimerIndex;
			m_ActiveHiResTimerIndex = i;

			// get time (do this as late as possible)
			int64 now;
			FRAMEPRO_GET_CLOCK_COUNT(now);

			// pause the current active timer
			if (current_index != -1)
				m_HiResTimers[current_index].m_Duration += now - m_HiResTimerStartTime;
			m_PausedHiResTimerStack.Add(current_index);

			// start the new timer
			m_HiResTimerStartTime = now;
		}

		FRAMEPRO_FORCE_INLINE void StopHiResTimer()
		{
			FRAMEPRO_ASSERT(IsOnTLSThread());

			// get time (do this as early as possible)
			int64 now;
			FRAMEPRO_GET_CLOCK_COUNT(now);

			// get the current active timer
			HiResTimer& timer = m_HiResTimers[m_ActiveHiResTimerIndex];

			// add time and count to active timer
			timer.m_Duration += now - m_HiResTimerStartTime;
			++timer.m_Count;

			// unpause previous timer
			m_ActiveHiResTimerIndex = m_PausedHiResTimerStack.RemoveLast();
			m_HiResTimerStartTime = now;
		}

		FRAMEPRO_FORCE_INLINE bool HasHiResTimers() const
		{
			return m_HiResTimers.GetCount() != 0;
		}

		FRAMEPRO_FORCE_INLINE void SubmitHiResTimers(int64 current_time)
		{
			FRAMEPRO_ASSERT(IsOnTLSThread());

			if (m_HiResTimers.GetCount() != 0)
				SendHiResTimersScope(current_time);

			m_HiResTimerScopeStartTime = current_time;
		}

		FRAMEPRO_NO_INLINE void SendHiResTimersScope(int64 current_time);

#ifdef FRAMEPRO_SCOPE_MIN_TIME
		int64 GetScopeMinTime() const { return m_ScopeMinTime; }
#endif

#ifdef FRAMEPRO_WAIT_EVENT_MIN_TIME
		int64 GetWaitEventMinTime() const { return m_WaitEventMinTime; }
#endif

		void SetCustomTimeSpanStat(StringId name, int64 value, const char* p_unit);

		void SetCustomTimeSpanStat(StringId name, int64 value, const wchar_t* p_unit);

		void SetCustomTimeSpanStat(StringId name, double value, const char* p_unit);

		void SetCustomTimeSpanStat(StringId name, double value, const wchar_t* p_unit);

#if FRAMEPRO_ENABLE_CALLSTACKS
		bool ShouldSendCallstacks() const { return m_SendCallstacks; }

		void SetSendCallstacks(bool b) { m_SendCallstacks = b; }

		CallstackResult GetCallstack();
#endif

	private:
		void Clear();

		void SendString(StringId string_id, const char* p_str, PacketType::Enum packet_type);

		void SendString(StringId string_id, const wchar_t* p_str, PacketType::Enum packet_type);

		void ShowMemoryWarning() const;

		void SendSessionInfo(const void* p_data, int size);

		void UpdateStringMemorySize();

		FRAMEPRO_NO_INLINE void FlushCurrentSendBuffer_no_lock();

		void AllocateCurrentSendBuffer();

		void FreeCurrentSendBuffer();
		
		SendBuffer* AllocateSendBuffer();

		void UpdateSendStringsImmediatelyFlag() { m_SendStringsImmediately = m_RecordingToFile || !m_Interactive; }

		bool AddStringLiteral(const void* p_string)
		{
			bool added = m_LiteralStringSet.Add(p_string);
			m_LiteralStringSetMemorySize = m_LiteralStringSet.GetMemorySize();
			return added;
		}

		ConditionalParentScope* GetConditionalParentScope(const char* p_name);

		ConditionalParentScope* CreateConditionalParentScope(const char* p_name);

		void FlushConditionalChildSendBuffers();

		FRAMEPRO_NO_INLINE static void AddHiResTimer(const char* p_name, HiResTimerList* p_timers);

		void PushHiResTimerList();

		void PopHiResTimerList();

		void SendHiResTimerList(HiResTimerList* p_hires_timers, int64 current_time);

		void SendRootHiResTimerList();

#if FRAMEPRO_DEBUG
		bool IsOnTLSThread() const { return GetCurrentThreadId() == m_OSThreadId; }
#endif
		template<typename T>
		void DeleteListItems(List<T>& list)
		{
			while (!list.IsEmpty())
			{
				T* p_item = list.RemoveHead();
				Delete(mp_Allocator, p_item);
			}
		}

		//------------------------------------------------------------------------
		// data
	private:

		// keep these together because they are accessed by AddTimeSpan functions, which we need to be fast

#ifdef FRAMEPRO_SCOPE_MIN_TIME
		int64 m_ScopeMinTime;			// read-only after initialisation so no need to be atomic
#endif

#ifdef FRAMEPRO_WAIT_EVENT_MIN_TIME
		int64 m_WaitEventMinTime;			// read-only after initialisation so no need to be atomic
#endif

		RelaxedAtomic<bool> m_Interactive;

		RelaxedAtomic<bool> m_RecordingToFile;
		RelaxedAtomic<bool> m_SendStringsImmediately;

		CriticalSection m_CurrentSendBufferCriticalSection;
		void* mp_CurrentSendBuffer;						// access must be protected with m_CurrentSendBufferCriticalSection
		int m_CurrentSendBufferSize;					// access must be protected with m_CurrentSendBufferCriticalSection
		
		int m_ThreadId;
		uint64 m_OSThreadId;

// START EPIC
		#if !FRAMEPRO_WIN_BASED_PLATFORM && !FRAMEPRO_UE4_BASED_PLATFORM
// END EPIC

			static int m_NewThreadId;
		#endif

		int64 m_HiResTimerScopeStartTime;

		// everything else

		// HiRes timers stuff is only accessed from the tls thread
		Array<HiResTimer> m_HiResTimers;
		Array<int> m_PausedHiResTimerStack;
		int64 m_HiResTimerStartTime;
		int m_ActiveHiResTimerIndex;

		List<SendBuffer> m_SendBufferFreeList;

		FrameProTLS* mp_Next;

		Allocator* mp_Allocator;

		List<SendBuffer> m_SendBufferList;

		PointerSet m_LiteralStringSet;
		RelaxedAtomic<size_t> m_LiteralStringSetMemorySize;

		static std::atomic<long> m_StringCount;
		HashMap<String, StringId> m_StringHashMap;
		HashMap<WString, StringId> m_WStringHashMap;

		Buffer m_SessionInfoBuffer;
		CriticalSection m_SessionInfoBufferLock;
		RelaxedAtomic<size_t> m_SessionInfoBufferMemorySize;

		CriticalSection m_CriticalSection;

		std::atomic<bool> m_Connected;

		IncrementingBlockAllocator m_StringAllocator;

		char m_TempStringBuffer[FRAMEPRO_MAX_PATH];

		RelaxedAtomic<size_t> m_SendBufferMemorySize;
		RelaxedAtomic<size_t> m_StringMemorySize;

		static const int m_SendBufferCapacity = 32*1024;

		int64 m_ClockFrequency;

		RelaxedAtomic<bool> m_ShuttingDown;

		// conditional parent scope
		CriticalSection m_ConditionalParentScopeListCritSec;
		List<ConditionalParentScope> m_ConditionalParentScopeList;
		ConditionalParentScope* mp_CurrentConditionalParentScope;

		char m_FalseSharingSpacerBuffer[128];		// separate TLS classes to avoid false sharing

#if FRAMEPRO_ENABLE_CALLSTACKS
		StackTrace m_StackTrace;
		bool m_SendCallstacks;
#endif
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPROTLS_H_INCLUDED

//------------------------------------------------------------------------
// FrameProSession.hpp
#ifndef FRAMEPROSESSION_H_INCLUDED
#define FRAMEPROSESSION_H_INCLUDED

//------------------------------------------------------------------------




//------------------------------------------------------------------------
// Thread.hpp
#ifndef FRAMEPRO_THREAD_H_INCLUDED
#define FRAMEPRO_THREAD_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
// Event.hpp
#ifndef FRAMEPRO_EVENT_H_INCLUDED
#define FRAMEPRO_EVENT_H_INCLUDED

//------------------------------------------------------------------------


//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//--------------------------------------------------------------------
	class Event
	{
	public:
		//--------------------------------------------------------------------
		Event(bool initial_state, bool auto_reset)
		{
#if FRAMEPRO_WIN_BASED_PLATFORM
			m_Handle = CreateEvent(NULL, !auto_reset, initial_state, NULL);
// START EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
			m_Event = FPlatformProcess::CreateSynchEvent( !auto_reset );
			if( initial_state )
				Set();
// END EPIC
#else
			pthread_cond_init(&m_Cond, NULL);
			pthread_mutex_init(&m_Mutex, NULL);
			m_Signalled = false;
			m_AutoReset = auto_reset;
	
			if(initial_state)
				Set();
#endif
		}

		//--------------------------------------------------------------------
		~Event()
		{
#if FRAMEPRO_WIN_BASED_PLATFORM
			CloseHandle(m_Handle);
// START EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
			delete m_Event;
			m_Event = nullptr;
// END EPIC
#else
			pthread_mutex_destroy(&m_Mutex);
			pthread_cond_destroy(&m_Cond);
#endif
		}

		//--------------------------------------------------------------------
		void Set() const
		{
#if FRAMEPRO_WIN_BASED_PLATFORM
			SetEvent(m_Handle);
// START EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
			m_Event->Trigger();
// END EPIC
#else
			pthread_mutex_lock(&m_Mutex);
			m_Signalled = true;
			pthread_mutex_unlock(&m_Mutex);
			pthread_cond_signal(&m_Cond);
#endif
		}

		//--------------------------------------------------------------------
		void Reset()
		{
#if FRAMEPRO_WIN_BASED_PLATFORM
			ResetEvent(m_Handle);
// START EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
			m_Event->Reset();
// END EPIC
#else
			pthread_mutex_lock(&m_Mutex);
			m_Signalled = false;
			pthread_mutex_unlock(&m_Mutex);
#endif
		}

		//--------------------------------------------------------------------
		int Wait(int timeout=-1) const
		{
#if FRAMEPRO_WIN_BASED_PLATFORM
			return WaitForSingleObject(m_Handle, timeout) == 0/*WAIT_OBJECT_0*/;
// START EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
			return (timeout==-1) ? m_Event->Wait() : m_Event->Wait( timeout );
// END EPIC
#else
			pthread_mutex_lock(&m_Mutex);
	
			if(m_Signalled)
			{
				m_Signalled = false;
				pthread_mutex_unlock(&m_Mutex);
				return true;
			}
	
			if(timeout == -1)
			{
				while(!m_Signalled)
					pthread_cond_wait(&m_Cond, &m_Mutex);
		
				if(!m_AutoReset)
					m_Signalled = false;

				pthread_mutex_unlock(&m_Mutex);
		
				return true;
			}
			else
			{		
				timeval curr;
				gettimeofday(&curr, NULL);
		
				timespec time;
				time.tv_sec  = curr.tv_sec + timeout / 1000;
				time.tv_nsec = (curr.tv_usec * 1000) + ((timeout % 1000) * 1000000);
		
				time.tv_sec += time.tv_nsec / 1000000000L;
				time.tv_nsec = time.tv_nsec % 1000000000L;

				int ret = 0;
				do
				{
					ret = pthread_cond_timedwait(&m_Cond, &m_Mutex, &time);

				} while(!m_Signalled && ret != ETIMEDOUT);
		
				if(m_Signalled)
				{
					if(!m_AutoReset)
						m_Signalled = false;

					pthread_mutex_unlock(&m_Mutex);
					return true;
				}
		
				pthread_mutex_unlock(&m_Mutex);
				return false;
			}
#endif
		}

		//------------------------------------------------------------------------
		// data
	private:
#if FRAMEPRO_WIN_BASED_PLATFORM
		HANDLE m_Handle;
// START EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
		FEvent* m_Event;
// END EPIC
#else
		mutable pthread_cond_t  m_Cond;
		mutable pthread_mutex_t m_Mutex;
		mutable volatile bool m_Signalled;
		bool m_AutoReset;
#endif
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPRO_EVENT_H_INCLUDED

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#if FRAMEPRO_WIN_BASED_PLATFORM
	#pragma warning(push)
	#pragma warning(disable : 4100)
#endif

//------------------------------------------------------------------------
#if !FRAMEPRO_WIN_BASED_PLATFORM
	#include <pthread.h>
#endif

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	typedef int (*ThreadMain)(void*);

	//------------------------------------------------------------------------
	class Thread
// BEGIN EPIC
#if FRAMEPRO_UE4_BASED_PLATFORM
	: public FRunnable
#endif		
// END EPIC

	{
	public:
		Thread();

		void CreateThread(ThreadMain p_thread_main, void* p_param=NULL);

		bool IsAlive() const { return m_Alive; }

		void SetPriority(int priority);

		void SetAffinity(int affinity);

		void WaitForThreadToTerminate(int timeout);

	private:
		#if FRAMEPRO_WIN_BASED_PLATFORM
			static unsigned long WINAPI PlatformThreadMain(void* p_param);
		// BEGIN EPIC
		#elif FRAMEPRO_UE4_BASED_PLATFORM
			virtual uint32 Run() override;
		// END EPIC
		#else
			static void* PlatformThreadMain(void* p_param);
		#endif

		//------------------------------------------------------------------------
		// data
	private:
#if FRAMEPRO_WIN_BASED_PLATFORM
		mutable HANDLE m_Handle;
// BEGIN EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
		mutable TUniquePtr<FRunnableThread> m_Runnable;
// END EPIC
#else
		mutable pthread_t m_Thread;
#endif
		mutable bool m_Alive;

		mutable ThreadMain mp_ThreadMain;
		mutable void* mp_Param;

		Event m_ThreadTerminatedEvent;
	};
}

//------------------------------------------------------------------------
#if FRAMEPRO_WIN_BASED_PLATFORM
	#pragma warning(pop)
#endif

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPRO_THREAD_H_INCLUDED








//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#include <atomic>

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	class FrameProTLS;
	class Allocator;
	struct ThreadCustomStats;
	class SendBuffer;
#if FRAMEPRO_EVENT_TRACE_WIN32
	class EventTraceWin32;
#endif

	//------------------------------------------------------------------------
	class FrameProSession
	{
	public:
		FrameProSession();

		~FrameProSession();
		
		void BlockSockets();

		void UnblockSockets();

		void FrameStart();

		void Shutdown();

		int64 GetClockFrequency();

		void AddFrameProTLS(FrameProTLS* p_framepro_tls);

		void RemoveFrameProTLS(FrameProTLS* p_framepro_tls);

		void SetPort(int port);
		
		void SetAllocator(Allocator* p_allocator);

		Allocator* GetAllocator()
		{
			Allocator* p_allocator = mp_Allocator;
			return p_allocator ? p_allocator : CreateDefaultAllocator();
		}

		void SetThreadName(const char* p_name);

// START EPIC
		void StartRecording(const FString& p_filename, bool context_switches, int64 max_file_size);
// END EPIC

		void StopRecording();

		void RegisterConnectionChangedCallback(ConnectionChangedCallback p_callback, void* p_context);

		void UnregisterConnectionChangedCallback(ConnectionChangedCallback p_callback);

		void SetThreadPriority(int priority);
		
		void SetThreadAffinity(int affinity);

		void SendSessionDetails(const char* p_name, const char* p_build_id);

		void SendSessionDetails(const wchar_t* p_name, const wchar_t* p_build_id);

		void AddGlobalHiResTimer(GlobalHiResTimer* p_timer);

		bool CallConditionalParentScopeCallback(ConditionalParentScopeCallback p_callback, const char* p_name, int64 start_time, int64 end_time);

		void EnumerateLoadedModulesCallback(
			int64 module_base,
			const char* p_module_name,
			bool m_UseLookupFunctionForBaseAddress);

		void SetConditionalScopeMinTimeInMicroseconds(int64 value);

	private:
		void Initialise(FrameProTLS* p_framepro_tls);

		void SendSessionDetails(StringId name, StringId build_id);

		Allocator* CreateDefaultAllocator();

		void InitialiseConnection(FrameProTLS* p_framepro_tls);

		FRAMEPRO_FORCE_INLINE void CalculateTimerFrequency();

		void SetConnected(bool value);

		bool SendSendBuffer(SendBuffer* p_send_buffer, Socket& socket);

// START EPIC
		void WriteSendBuffer(SendBuffer* p_send_buffer, IFileHandle* p_file, int64& file_size);
// END EPIC
		
		void SendFrameBuffer();

		static int StaticSendThreadMain(void*);

		int SendThreadMain();

		void HandleDisconnect();

		void HandleDisconnect_NoLock();

		void SendRecordedDataAndDisconnect();

		void SendHeartbeatInfo(FrameProTLS* p_framepro_tls);

		void SendImmediate(void* p_data, int size, FrameProTLS* p_framepro_tls);

		bool HasSetThreadName(int thread_id) const;

		void OnConnectionChanged(bool connected) const;

		int GetConnectionChangedCallbackIndex(ConnectionChangedCallback p_callback);

		size_t GetMemoryUsage() const;

		void CreateSendThread();

		static void ContextSwitchCallback_Static(const ContextSwitch& context_switch, void* p_param);

		void ContextSwitchCallback(const ContextSwitch& context_switch);

		void StartRecordingContextSitches();

		void FlushGlobalHiResTimers(FrameProTLS* p_framepro_tls);

		void ClearGlobalHiResTimers();

		void EnumerateModules();

		void SendExtraModuleInfo(int64 ModuleBase, FrameProTLS* p_framepro_tls);

#if FRAMEPRO_SOCKETS_ENABLED
		bool InitialiseFileCache();

		static int StaticConnectThreadMain(void*);

		int ConnectThreadMain();

		void SendOnMainThread(void* p_src, int size);

		static int StaticReceiveThreadMain(void*);

		int OnReceiveThreadExit();

		int ReceiveThreadMain();

		void OpenListenSocket();

		void StartConnectThread();

		void CreateReceiveThread();

		template<typename T>
		void SendOnMainThread(T& packet)
		{
			SendOnMainThread(&packet, sizeof(packet));
		}
#endif

		//------------------------------------------------------------------------
		// data
	private:
		static FrameProSession* mp_Inst;		// just used for debugging

		mutable CriticalSection m_CriticalSection;

		char m_Port[8];

		Allocator* mp_Allocator;
		bool m_CreatedAllocator;

		bool m_Initialised;

		std::atomic<bool> m_InitialiseConnectionNextFrame;

		std::atomic<bool> m_StartContextSwitchRecording;

		int64 m_ClockFrequency;

		mutable CriticalSection m_TLSListCriticalSection;
		List<FrameProTLS> m_FrameProTLSList;

		int m_MainThreadId;

		Thread m_SendThread;
		Event m_SendThreadStarted;
		Event m_SendReady;
		Event m_SendComplete;

		Thread m_ReceiveThread;
		Event m_ReceiveThreadTerminatedEvent;

		CriticalSection m_SendFrameBufferCriticalSection;

		RelaxedAtomic<bool> m_Interactive;
// START EPIC
		IFileHandle* mp_NonInteractiveRecordingFile;
// END EPIC
		int64 m_NonInteractiveRecordingFileSize;

		int64 m_LastSessionInfoSendTime;

		Array<int> m_NamedThreads;

// START EPIC
		IFileHandle* mp_RecordingFile;
// END EPIC
		int64 m_RecordingFileSize;
		int64 m_MaxRecordingFileSize;

		bool m_ThreadPrioritySet;
		int m_ThreadPriority;
		bool m_ThreadAffinitySet;
		int m_ThreadAffinity;

#if FRAMEPRO_SOCKETS_ENABLED
		Thread m_ConnectThread;
		Socket m_ListenSocket;
		Socket m_ClientSocket;
#endif

		std::atomic<bool> m_SendThreadExit;
		Event m_SendThreadFinished;

		bool m_SocketsBlocked;

		struct ConnectionChangedcallbackInfo
		{
			ConnectionChangedCallback mp_Callback;
			void* mp_Context;
		};
		mutable CriticalSection m_ConnectionChangedCriticalSection;
		Array<ConnectionChangedcallbackInfo> m_Connectionchangedcallbacks;

		Array<int> m_ProcessIds;

		Buffer m_MainThreadSendBuffer;
		CriticalSection m_MainThreadSendBufferLock;

		Array<RequestStringLiteralPacket> m_StringRequestPackets;
		CriticalSection m_StringRequestPacketsLock;

		GlobalHiResTimer* mp_GlobalHiResTimers;

		int m_ModulesSent;

		Array<ModulePacket*> m_ModulePackets;

#if FRAMEPRO_EVENT_TRACE_WIN32
		EventTraceWin32* mp_EventTraceWin32;
#endif

#if FRAMEPRO_ENABLE_CALLSTACKS
		bool m_SendModules;
#endif
	};
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPROSESSION_H_INCLUDED

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#include <atomic>

//------------------------------------------------------------------------
#if FRAMEPRO_WIN_BASED_PLATFORM
	#include <intrin.h>
#endif

//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED && FRAMEPRO_WIN_BASED_PLATFORM
	#pragma comment(lib, "Ws2_32.lib")
#endif

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	RelaxedAtomic<bool> g_Connected = false;

	RelaxedAtomic<unsigned int> g_ConditionalScopeMinTime = UINT_MAX;

	//------------------------------------------------------------------------

// START EPIC
#if FRAMEPRO_PLATFORM_ANDROID
	#include <sys/syscall.h>
#endif

	FRAMEPRO_FORCE_INLINE int GetCore()
	{
	#if FRAMEPRO_PLATFORM_XBOXONE || FRAMEPRO_PLATFORM_UWP
		int cpu_info[4];
		__cpuid(cpu_info, 1);
		return (cpu_info[1] >> 24) & 0xff;
	#elif FRAMEPRO_PLATFORM_WIN
		return GetCurrentProcessorNumber();
	#elif FRAMEPRO_PLATFORM_UNIX
		return sched_getcpu();
	#elif FRAMEPRO_PLATFORM_ANDROID
		unsigned cpu;
		int err = syscall(__NR_getcpu, &cpu, NULL, NULL);
		return (!err) ? (int)cpu : 0;
	#elif FRAMEPRO_PLATFORM_IOS
		return 0; // TODO
	#elif FRAMEPRO_UE4_BASED_PLATFORM
		return FPlatformProcess::GetCurrentCoreNumber();
	#else
		#error
	#endif
	}
// END EPIC

	//------------------------------------------------------------------------
	FrameProSession& GetFrameProSession()
	{
		static FrameProSession session;
		return session;
	}
		
	//------------------------------------------------------------------------
// BEGIN EPIC
//	FRAMEPRO_THREAD_LOCAL FrameProTLS* gp_FrameProTLS = NULL;
// END EPIC

	//------------------------------------------------------------------------
	FRAMEPRO_NO_INLINE FrameProTLS* CreateFrameProTLS()
	{
		FrameProSession& framepro_session = GetFrameProSession();

		Allocator* p_allocator = framepro_session.GetAllocator();

		FrameProTLS* p_framepro_tls = (FrameProTLS*)p_allocator->Alloc(sizeof(FrameProTLS));
		new (p_framepro_tls)FrameProTLS(p_allocator, framepro_session.GetClockFrequency());

		framepro_session.AddFrameProTLS(p_framepro_tls);

// BEGIN EPIC
		FPlatformTLS::SetTlsValue(GetFrameProTLSSlot(), (void*)p_framepro_tls);
//		gp_FrameProTLS = p_framepro_tls;
// END EPIC

		return p_framepro_tls;
	}

	//------------------------------------------------------------------------
	FRAMEPRO_NO_INLINE void DestroyFrameProTLS(FrameProTLS* p_framepro_tls)
	{
		FrameProSession& framepro_session = GetFrameProSession();

		framepro_session.RemoveFrameProTLS(p_framepro_tls);

		p_framepro_tls->~FrameProTLS();

		framepro_session.GetAllocator()->Free(p_framepro_tls);
	}

	//------------------------------------------------------------------------
	void SendWaitEventPacket(int64 event_id, int64 time, PacketType::Enum packet_type)
	{
		if (!g_Connected)
			return;

		FrameProTLS* p_framepro_tls = GetFrameProTLS();

		CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

		WaitEventPacket* p_packet = p_framepro_tls->AllocateSpaceInBuffer<WaitEventPacket>();

		p_packet->m_PacketType = packet_type;
		p_packet->m_Thread = p_framepro_tls->GetThreadId();
		p_packet->m_Core = GetCore();
		p_packet->m_EventId = event_id;
		p_packet->m_Time = time;
	}
}

//------------------------------------------------------------------------
void FramePro::SetAllocator(Allocator* p_allocator)
{
	GetFrameProSession().SetAllocator(p_allocator);
}

//------------------------------------------------------------------------
// START EPIC
// Remove QueryPerformanceCounter and DebugBreak
// END EPIC

void FramePro::Shutdown()
{
	GetFrameProSession().Shutdown();
}

//------------------------------------------------------------------------
void FramePro::FrameStart()
{
	GetFrameProSession().FrameStart();
}

//------------------------------------------------------------------------
void FramePro::RegisterConnectionChangedCallback(ConnectionChangedCallback p_callback, void* p_context)
{
	GetFrameProSession().RegisterConnectionChangedCallback(p_callback, p_context);
}

//------------------------------------------------------------------------
void FramePro::UnregisterConnectionChangedcallback(ConnectionChangedCallback p_callback)
{
	GetFrameProSession().UnregisterConnectionChangedCallback(p_callback);
}

//------------------------------------------------------------------------
#if FRAMEPRO_WIN_BASED_PLATFORM
	#pragma warning(push)
	#pragma warning(disable : 4127)
#endif

//------------------------------------------------------------------------
void FramePro::AddTimeSpan(const char* p_name_and_source_info, int64 start_time, int64 end_time)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

#ifdef FRAMEPRO_SCOPE_MIN_TIME
	if (end_time - start_time < p_framepro_tls->GetScopeMinTime())
		return;
#endif

	p_framepro_tls->SubmitHiResTimers(end_time);

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if(p_framepro_tls->SendStringsImmediately())
		p_framepro_tls->SendString(p_name_and_source_info, PacketType::NameAndSourceInfoPacket);

#if FRAMEPRO_ENABLE_CALLSTACKS
	if (p_framepro_tls->ShouldSendCallstacks())
	{
		CallstackResult callstack_result = p_framepro_tls->GetCallstack();

		int send_size = sizeof(TimeSpanPacket) + sizeof(CallstackPacket);
		if (callstack_result.m_IsNew)
			send_size += callstack_result.mp_Callstack->m_Size * sizeof(uint64);

		{
			CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

			TimeSpanPacket* p_packet = (TimeSpanPacket*)p_framepro_tls->AllocateSpaceInBuffer(send_size);

			p_packet->m_PacketType_AndCore = PacketType::TimeSpanWithCallstack | (FramePro::GetCore() << 16);
			p_packet->m_ThreadId = p_framepro_tls->GetThreadId();
			p_packet->m_NameAndSourceInfo = (StringId)p_name_and_source_info;
			p_packet->m_StartTime = start_time;
			p_packet->m_EndTime = end_time;

			CallstackPacket* p_callstack_packet = (CallstackPacket*)(p_packet + 1);
			p_callstack_packet->m_CallstackId = callstack_result.mp_Callstack->m_ID;
			p_callstack_packet->m_CallstackSize = 0;

			if (callstack_result.m_IsNew)
			{
				p_callstack_packet->m_CallstackSize = callstack_result.mp_Callstack->m_Size;
				memcpy(
					(char*)(p_callstack_packet + 1),
					callstack_result.mp_Callstack->mp_Stack,
					callstack_result.mp_Callstack->m_Size * sizeof(uint64));
			}
		}
	}
	else
#endif
	{
		CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

		TimeSpanPacket* p_packet = p_framepro_tls->AllocateSpaceInBuffer<TimeSpanPacket>();

		p_packet->m_PacketType_AndCore = PacketType::TimeSpan | (FramePro::GetCore() << 16);
		p_packet->m_ThreadId = p_framepro_tls->GetThreadId();
		p_packet->m_NameAndSourceInfo = (StringId)p_name_and_source_info;
		p_packet->m_StartTime = start_time;
		p_packet->m_EndTime = end_time;
	}
}

//------------------------------------------------------------------------
void FramePro::AddTimeSpan(const wchar_t* p_name_and_source_info, int64 start_time, int64 end_time)
{
	FRAMEPRO_ASSERT(start_time <= end_time);

	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	p_framepro_tls->SubmitHiResTimers(end_time);

#ifdef FRAMEPRO_SCOPE_MIN_TIME
	if (end_time - start_time < p_framepro_tls->GetScopeMinTime())
		return;
#endif

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if(p_framepro_tls->SendStringsImmediately())
		p_framepro_tls->SendString(p_name_and_source_info, PacketType::NameAndSourceInfoPacketW);

#if FRAMEPRO_ENABLE_CALLSTACKS
	if (p_framepro_tls->ShouldSendCallstacks())
	{
		CallstackResult callstack_result = p_framepro_tls->GetCallstack();

		int send_size = sizeof(TimeSpanPacket) + sizeof(CallstackPacket);
		if (callstack_result.m_IsNew)
			send_size += callstack_result.mp_Callstack->m_Size * sizeof(uint64);

		{
			CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

			TimeSpanPacket* p_packet = (TimeSpanPacket*)p_framepro_tls->AllocateSpaceInBuffer(send_size);

			p_packet->m_PacketType_AndCore = PacketType::TimeSpanWWithCallstack | (FramePro::GetCore() << 16);
			p_packet->m_ThreadId = p_framepro_tls->GetThreadId();
			p_packet->m_NameAndSourceInfo = (StringId)p_name_and_source_info;
			p_packet->m_StartTime = start_time;
			p_packet->m_EndTime = end_time;

			CallstackPacket* p_callstack_packet = (CallstackPacket*)(p_packet + 1);
			p_callstack_packet->m_CallstackId = callstack_result.mp_Callstack->m_ID;
			p_callstack_packet->m_CallstackSize = 0;

			if (callstack_result.m_IsNew)
			{
				p_callstack_packet->m_CallstackSize = callstack_result.mp_Callstack->m_Size;
				memcpy(
					(char*)(p_callstack_packet + 1),
					callstack_result.mp_Callstack->mp_Stack,
					callstack_result.mp_Callstack->m_Size * sizeof(uint64));
			}
		}
	}
	else
#endif
	{
		CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

		TimeSpanPacket* p_packet = p_framepro_tls->AllocateSpaceInBuffer<TimeSpanPacket>();

		p_packet->m_PacketType_AndCore = PacketType::TimeSpanW | (FramePro::GetCore() << 16);
		p_packet->m_ThreadId = p_framepro_tls->GetThreadId();
		p_packet->m_NameAndSourceInfo = (StringId)p_name_and_source_info;
		p_packet->m_StartTime = start_time;
		p_packet->m_EndTime = end_time;
	}
}

//------------------------------------------------------------------------
void FramePro::AddTimeSpan(StringId name, const char* p_source_info, int64 start_time, int64 end_time)
{
	FRAMEPRO_ASSERT(start_time <= end_time);

	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	p_framepro_tls->SubmitHiResTimers(end_time);

#ifdef FRAMEPRO_SCOPE_MIN_TIME
	if (end_time - start_time < p_framepro_tls->GetScopeMinTime())
		return;
#endif

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if(p_framepro_tls->SendStringsImmediately())
		p_framepro_tls->SendString(p_source_info, PacketType::SourceInfoPacket);

#if FRAMEPRO_ENABLE_CALLSTACKS
	if (p_framepro_tls->ShouldSendCallstacks())
	{
		CallstackResult callstack_result = p_framepro_tls->GetCallstack();

		int send_size = sizeof(NamedTimeSpanPacket) + sizeof(CallstackPacket);
		if (callstack_result.m_IsNew)
			send_size += callstack_result.mp_Callstack->m_Size * sizeof(uint64);

		{
			CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

			NamedTimeSpanPacket* p_packet = (NamedTimeSpanPacket*)p_framepro_tls->AllocateSpaceInBuffer(send_size);

			p_packet->m_PacketType_AndCore = PacketType::NamedTimeSpanWithCallstack | (FramePro::GetCore() << 16);
			p_packet->m_ThreadId = p_framepro_tls->GetThreadId();
			p_packet->m_Name = name;
			p_packet->m_SourceInfo = (StringId)p_source_info;
			p_packet->m_StartTime = start_time;
			p_packet->m_EndTime = end_time;

			CallstackPacket* p_callstack_packet = (CallstackPacket*)(p_packet + 1);
			p_callstack_packet->m_CallstackId = callstack_result.mp_Callstack->m_ID;
			p_callstack_packet->m_CallstackSize = 0;

			if (callstack_result.m_IsNew)
			{
				p_callstack_packet->m_CallstackSize = callstack_result.mp_Callstack->m_Size;
				memcpy(
					(char*)(p_callstack_packet + 1),
					callstack_result.mp_Callstack->mp_Stack,
					callstack_result.mp_Callstack->m_Size * sizeof(uint64));
			}
		}
	}
	else
#endif
	{
		CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

		NamedTimeSpanPacket* p_packet = p_framepro_tls->AllocateSpaceInBuffer<NamedTimeSpanPacket>();

		p_packet->m_PacketType_AndCore = PacketType::NamedTimeSpan | (FramePro::GetCore() << 16);
		p_packet->m_ThreadId = p_framepro_tls->GetThreadId();
		p_packet->m_Name = name;
		p_packet->m_SourceInfo = (StringId)p_source_info;
		p_packet->m_StartTime = start_time;
		p_packet->m_EndTime = end_time;
	}
}

//------------------------------------------------------------------------
void FramePro::AddTimeSpan(StringId name, const char* p_source_info, int64 start_time, int64 end_time, int thread_id, int core)
{
	FRAMEPRO_ASSERT(start_time <= end_time);

	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	p_framepro_tls->SubmitHiResTimers(end_time);

#ifdef FRAMEPRO_SCOPE_MIN_TIME
	if (end_time - start_time < p_framepro_tls->GetScopeMinTime())
		return;
#endif

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if(p_framepro_tls->SendStringsImmediately())
		p_framepro_tls->SendString(p_source_info, PacketType::SourceInfoPacket);

#if FRAMEPRO_ENABLE_CALLSTACKS
	if (p_framepro_tls->ShouldSendCallstacks())
	{
		CallstackResult callstack_result = p_framepro_tls->GetCallstack();

		int send_size = sizeof(NamedTimeSpanPacket) + sizeof(CallstackPacket);
		if (callstack_result.m_IsNew)
			send_size += callstack_result.mp_Callstack->m_Size * sizeof(uint64);

		{
			CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

			NamedTimeSpanPacket* p_packet = (NamedTimeSpanPacket*)p_framepro_tls->AllocateSpaceInBuffer(send_size);

			p_packet->m_PacketType_AndCore = PacketType::NamedTimeSpanWithCallstack | (core << 16);
			p_packet->m_ThreadId = thread_id;
			p_packet->m_Name = name;
			p_packet->m_SourceInfo = (StringId)p_source_info;
			p_packet->m_StartTime = start_time;
			p_packet->m_EndTime = end_time;

			CallstackPacket* p_callstack_packet = (CallstackPacket*)(p_packet + 1);
			p_callstack_packet->m_CallstackId = callstack_result.mp_Callstack->m_ID;
			p_callstack_packet->m_CallstackSize = 0;

			if (callstack_result.m_IsNew)
			{
				p_callstack_packet->m_CallstackSize = callstack_result.mp_Callstack->m_Size;
				memcpy(
					(char*)(p_callstack_packet + 1),
					callstack_result.mp_Callstack->mp_Stack,
					callstack_result.mp_Callstack->m_Size * sizeof(uint64));
			}
		}
	}
	else
#endif
	{
		CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

		NamedTimeSpanPacket* p_packet = p_framepro_tls->AllocateSpaceInBuffer<NamedTimeSpanPacket>();

		p_packet->m_PacketType_AndCore = PacketType::NamedTimeSpan | (core << 16);
		p_packet->m_ThreadId = thread_id;
		p_packet->m_Name = name;
		p_packet->m_SourceInfo = (StringId)p_source_info;
		p_packet->m_StartTime = start_time;
		p_packet->m_EndTime = end_time;
	}
}

//------------------------------------------------------------------------
// p_name is a string literal
void FramePro::AddTimeSpan(const char* p_name, const char* p_source_info, int64 start_time, int64 end_time)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	p_framepro_tls->SubmitHiResTimers(end_time);

#ifdef FRAMEPRO_SCOPE_MIN_TIME
	if (end_time - start_time < p_framepro_tls->GetScopeMinTime())
		return;
#endif

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if(p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_name, PacketType::StringPacket);
		p_framepro_tls->SendString(p_source_info, PacketType::SourceInfoPacket);
	}

#if FRAMEPRO_ENABLE_CALLSTACKS
	if (p_framepro_tls->ShouldSendCallstacks())
	{
		CallstackResult callstack_result = p_framepro_tls->GetCallstack();

		int send_size = sizeof(NamedTimeSpanPacket) + sizeof(CallstackPacket);
		if (callstack_result.m_IsNew)
			send_size += callstack_result.mp_Callstack->m_Size * sizeof(uint64);

		{
			CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

			NamedTimeSpanPacket* p_packet = (NamedTimeSpanPacket*)p_framepro_tls->AllocateSpaceInBuffer(send_size);

			p_packet->m_PacketType_AndCore = PacketType::StringLiteralNamedTimeSpanWithCallstack | (FramePro::GetCore() << 16);
			p_packet->m_ThreadId = p_framepro_tls->GetThreadId();
			p_packet->m_Name = (StringId)p_name;
			p_packet->m_SourceInfo = (StringId)p_source_info;
			p_packet->m_StartTime = start_time;
			p_packet->m_EndTime = end_time;

			CallstackPacket* p_callstack_packet = (CallstackPacket*)(p_packet + 1);
			p_callstack_packet->m_CallstackId = callstack_result.mp_Callstack->m_ID;
			p_callstack_packet->m_CallstackSize = 0;

			if (callstack_result.m_IsNew)
			{
				p_callstack_packet->m_CallstackSize = callstack_result.mp_Callstack->m_Size;
				memcpy(
					(char*)(p_callstack_packet + 1),
					callstack_result.mp_Callstack->mp_Stack,
					callstack_result.mp_Callstack->m_Size * sizeof(uint64));
			}
		}
	}
	else
#endif
	{
		CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

		NamedTimeSpanPacket* p_packet = p_framepro_tls->AllocateSpaceInBuffer<NamedTimeSpanPacket>();

		p_packet->m_PacketType_AndCore = PacketType::StringLiteralNamedTimeSpan | (FramePro::GetCore() << 16);
		p_packet->m_ThreadId = p_framepro_tls->GetThreadId();
		p_packet->m_Name = (StringId)p_name;
		p_packet->m_SourceInfo = (StringId)p_source_info;
		p_packet->m_StartTime = start_time;
		p_packet->m_EndTime = end_time;
	}
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(const char* p_name, int value, const char* p_graph, const char* p_unit)
{
	AddCustomStat(p_name, (int64)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(const char* p_name, int64 value, const char* p_graph, const char* p_unit)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if (p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_name, PacketType::StringPacket);
		p_framepro_tls->SendString(p_graph, PacketType::StringPacket);
		p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
	}

	CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

	CustomStatPacketInt64* p_packet = p_framepro_tls->AllocateSpaceInBuffer<CustomStatPacketInt64>();

	CustomStatValueType::Enum value_type = CustomStatValueType::Int64;

	p_packet->m_PacketTypeAndValueType = PacketType::CustomStatPacket | (value_type << 16);
	p_packet->m_Count = 1;
	p_packet->m_Name = (StringId)p_name;
	p_packet->m_Value = value;
	p_packet->m_Graph = (StringId)p_graph;
	p_packet->m_Unit = (StringId)p_unit;
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(const char* p_name, float value, const char* p_graph, const char* p_unit)
{
	AddCustomStat(p_name, (double)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(const char* p_name, double value, const char* p_graph, const char* p_unit)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if (p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_name, PacketType::StringPacket);
		p_framepro_tls->SendString(p_graph, PacketType::StringPacket);
		p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
	}

	CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

	CustomStatPacketDouble* p_packet = p_framepro_tls->AllocateSpaceInBuffer<CustomStatPacketDouble>();

	CustomStatValueType::Enum value_type = CustomStatValueType::Double;

	p_packet->m_PacketTypeAndValueType = PacketType::CustomStatPacket | (value_type << 16);
	p_packet->m_Count = 1;
	p_packet->m_Name = (StringId)p_name;
	p_packet->m_Value = value;
	p_packet->m_Graph = (StringId)p_graph;
	p_packet->m_Unit = (StringId)p_unit;
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(const wchar_t* p_name, int value, const wchar_t* p_graph, const wchar_t* p_unit)
{
	AddCustomStat(p_name, (int64)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(const wchar_t* p_name, int64 value, const wchar_t* p_graph, const wchar_t* p_unit)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if (p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_name, PacketType::StringPacket);
		p_framepro_tls->SendString(p_graph, PacketType::StringPacket);
		p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
	}

	CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

	CustomStatPacketInt64* p_packet = p_framepro_tls->AllocateSpaceInBuffer<CustomStatPacketInt64>();

	CustomStatValueType::Enum value_type = CustomStatValueType::Int64;

	p_packet->m_PacketTypeAndValueType = PacketType::CustomStatPacket | (value_type << 16);
	p_packet->m_Count = 1;
	p_packet->m_Name = (StringId)p_name;
	p_packet->m_Value = value;
	p_packet->m_Graph = (StringId)p_graph;
	p_packet->m_Unit = (StringId)p_unit;
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(const wchar_t* p_name, float value, const wchar_t* p_graph, const wchar_t* p_unit)
{
	AddCustomStat(p_name, (double)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(const wchar_t* p_name, double value, const wchar_t* p_graph, const wchar_t* p_unit)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if (p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_name, PacketType::StringPacket);
		p_framepro_tls->SendString(p_graph, PacketType::StringPacket);
		p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
	}

	CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

	CustomStatPacketDouble* p_packet = p_framepro_tls->AllocateSpaceInBuffer<CustomStatPacketDouble>();

	CustomStatValueType::Enum value_type = CustomStatValueType::Double;

	p_packet->m_PacketTypeAndValueType = PacketType::CustomStatPacket | (value_type << 16);
	p_packet->m_Count = 1;
	p_packet->m_Name = (StringId)p_name;
	p_packet->m_Value = value;
	p_packet->m_Graph = (StringId)p_graph;
	p_packet->m_Unit = (StringId)p_unit;
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(StringId name, int value, const char* p_graph, const char* p_unit)
{
	AddCustomStat(name, (int64)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(StringId name, int64 value, const char* p_graph, const char* p_unit)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if (p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_graph, PacketType::StringPacket);
		p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
	}

	CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

	CustomStatPacketInt64* p_packet = p_framepro_tls->AllocateSpaceInBuffer<CustomStatPacketInt64>();

	CustomStatValueType::Enum value_type = CustomStatValueType::Int64;

	p_packet->m_PacketTypeAndValueType = PacketType::CustomStatPacket | (value_type << 16);
	p_packet->m_Count = 1;
	p_packet->m_Name = name;
	p_packet->m_Value = value;
	p_packet->m_Graph = (StringId)p_graph;
	p_packet->m_Unit = (StringId)p_unit;
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(StringId name, float value, const char* p_graph, const char* p_unit)
{
	AddCustomStat(name, (double)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(StringId name, double value, const char* p_graph, const char* p_unit)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if (p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_graph, PacketType::StringPacket);
		p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
	}

	CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

	CustomStatPacketDouble* p_packet = p_framepro_tls->AllocateSpaceInBuffer<CustomStatPacketDouble>();

	CustomStatValueType::Enum value_type = CustomStatValueType::Double;

	p_packet->m_PacketTypeAndValueType = PacketType::CustomStatPacket | (value_type << 16);
	p_packet->m_Count = 1;
	p_packet->m_Name = name;
	p_packet->m_Value = value;
	p_packet->m_Graph = (StringId)p_graph;
	p_packet->m_Unit = (StringId)p_unit;
}

// START EPIC 
//------------------------------------------------------------------------
void FramePro::AddCustomStat(StringId name, int value, const wchar_t* p_graph, const wchar_t* p_unit)
{
	AddCustomStat(name, (int64)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(StringId name, int64 value, const wchar_t* p_graph, const wchar_t* p_unit)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if (p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_graph, PacketType::StringPacket);
		p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
	}

	CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

	CustomStatPacketInt64* p_packet = p_framepro_tls->AllocateSpaceInBuffer<CustomStatPacketInt64>();

	CustomStatValueType::Enum value_type = CustomStatValueType::Int64;

	p_packet->m_PacketTypeAndValueType = PacketType::CustomStatPacket | (value_type << 16);
	p_packet->m_Count = 1;
	p_packet->m_Name = name;
	p_packet->m_Value = value;
	p_packet->m_Graph = (StringId)p_graph;
	p_packet->m_Unit = (StringId)p_unit;
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(StringId name, float value, const wchar_t* p_graph, const wchar_t* p_unit)
{
	AddCustomStat(name, (double)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::AddCustomStat(StringId name, double value, const wchar_t* p_graph, const wchar_t* p_unit)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
	if (p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_graph, PacketType::StringPacket);
		p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
	}

	CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

	CustomStatPacketDouble* p_packet = p_framepro_tls->AllocateSpaceInBuffer<CustomStatPacketDouble>();

	CustomStatValueType::Enum value_type = CustomStatValueType::Double;

	p_packet->m_PacketTypeAndValueType = PacketType::CustomStatPacket | (value_type << 16);
	p_packet->m_Count = 1;
	p_packet->m_Name = name;
	p_packet->m_Value = value;
	p_packet->m_Graph = (StringId)p_graph;
	p_packet->m_Unit = (StringId)p_unit;
}
// END EPIC

//------------------------------------------------------------------------
#if FRAMEPRO_WIN_BASED_PLATFORM
	#pragma warning(pop)
#endif

//------------------------------------------------------------------------
void FramePro::SetThreadName(const char* p_name)
{
	GetFrameProSession().SetThreadName(p_name);
}

//------------------------------------------------------------------------
void FramePro::SetThreadOrder(StringId thread_name)
{
	GetFrameProTLS()->SetThreadOrder(thread_name);
}

//------------------------------------------------------------------------
FramePro::StringId FramePro::RegisterString(const char* p_str)
{
	return GetFrameProTLS()->RegisterString(p_str);
}

//------------------------------------------------------------------------
FramePro::StringId FramePro::RegisterString(const wchar_t* p_str)
{
	return GetFrameProTLS()->RegisterString(p_str);
}

//------------------------------------------------------------------------
// START EPIC
void FramePro::StartRecording(const FString& p_filename, bool context_switches, int64 max_file_size)
{
	GetFrameProSession().StartRecording(p_filename, context_switches, max_file_size);
}
// END EPIC

//------------------------------------------------------------------------
void FramePro::StopRecording()
{
	GetFrameProSession().StopRecording();
}

//------------------------------------------------------------------------
void FramePro::SetThreadPriority(int priority)
{
	GetFrameProSession().SetThreadPriority(priority);
}

//------------------------------------------------------------------------
void FramePro::SetThreadAffinity(int affinity)
{
	GetFrameProSession().SetThreadAffinity(affinity);
}

//------------------------------------------------------------------------
void FramePro::BlockSockets()
{
	GetFrameProSession().BlockSockets();
}

//------------------------------------------------------------------------
void FramePro::UnblockSockets()
{
	GetFrameProSession().UnblockSockets();
}

//------------------------------------------------------------------------
void FramePro::SetPort(int port)
{
	GetFrameProSession().SetPort(port);
}

//------------------------------------------------------------------------
void FramePro::SendSessionInfo(const char* p_name, const char* p_build_id)
{
	GetFrameProSession().SendSessionDetails(p_name, p_build_id);
}

//------------------------------------------------------------------------
void FramePro::SendSessionInfo(const wchar_t* p_name, const wchar_t* p_build_id)
{
	GetFrameProSession().SendSessionDetails(p_name, p_build_id);
}

//------------------------------------------------------------------------
void FramePro::AddGlobalHiResTimer(GlobalHiResTimer* p_timer)
{
	GetFrameProSession().AddGlobalHiResTimer(p_timer);
}

//------------------------------------------------------------------------
void FramePro::CleanupThread()
{
	GetFrameProTLS()->FlushCurrentSendBuffer();

	GetFrameProTLS()->Shutdown();		// will get cleaned up the next time the buffers are sent on the send thread
}

//------------------------------------------------------------------------
void FramePro::PushConditionalParentScope(const char* p_name, int64 pre_duration, int64 post_duration)
{
	GetFrameProTLS()->PushConditionalParentScope(p_name, pre_duration, post_duration);
}

//------------------------------------------------------------------------
void FramePro::PopConditionalParentScope(bool add_children)
{
	GetFrameProTLS()->PopConditionalParentScope(add_children);
}

//------------------------------------------------------------------------
bool FramePro::CallConditionalParentScopeCallback(ConditionalParentScopeCallback p_callback, const char* p_name, int64 start_time, int64 end_time)
{
	return GetFrameProSession().CallConditionalParentScopeCallback(p_callback, p_name, start_time, end_time);
}

//------------------------------------------------------------------------
void FramePro::StartHiResTimer(const char* p_name)
{
	GetFrameProTLS()->StartHiResTimer(p_name);
}

//------------------------------------------------------------------------
void FramePro::StopHiResTimer()
{
	GetFrameProTLS()->StopHiResTimer();
}

//------------------------------------------------------------------------
void FramePro::SubmitHiResTimers(int64 current_time)
{
	FRAMEPRO_ASSERT(g_Connected);

	GetFrameProTLS()->SubmitHiResTimers(current_time);
}

//------------------------------------------------------------------------
void FramePro::Log(const char* p_message)
{
	if(g_Connected)
		GetFrameProTLS()->SendLogPacket(p_message);
}

//------------------------------------------------------------------------
void FramePro::AddEvent(const char* p_name, uint colour)
{
	if (g_Connected)
		GetFrameProTLS()->SendEventPacket(p_name, colour);
}

//------------------------------------------------------------------------
void FramePro::AddWaitEvent(int64 event_id, int64 start_time, int64 end_time)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

#ifdef FRAMEPRO_SCOPE_MIN_TIME
	if (end_time - start_time < p_framepro_tls->GetWaitEventMinTime())
		return;
#endif
	SendWaitEventPacket(event_id, start_time, PacketType::StartWaitEventPacket);
	SendWaitEventPacket(event_id, end_time, PacketType::StopWaitEventPacket);
}

//------------------------------------------------------------------------
void FramePro::TriggerWaitEvent(int64 event_id)
{
	int64 time;
	FRAMEPRO_GET_CLOCK_COUNT(time);

	SendWaitEventPacket(event_id, time, PacketType::TriggerWaitEventPacket);
}

//------------------------------------------------------------------------
void FramePro::SetScopeCustomStat(const char* p_name, int64 value, const char* p_graph, const char* p_unit)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	if (p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_name, PacketType::StringPacket);
		p_framepro_tls->SendString(p_graph, PacketType::StringPacket);
		p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
	}

	p_framepro_tls->SetCustomTimeSpanStat((StringId)p_name, value, p_unit);

	AddCustomStat(p_name, value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::SetScopeCustomStat(const wchar_t* p_name, int64 value, const wchar_t* p_graph, const wchar_t* p_unit)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	if (p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_name, PacketType::StringPacket);
		p_framepro_tls->SendString(p_graph, PacketType::StringPacket);
		p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
	}

	p_framepro_tls->SetCustomTimeSpanStat((StringId)p_name, value, p_unit);

	AddCustomStat(p_name, value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::SetScopeCustomStat(StringId name, int64 value, const char* p_graph, const char* p_unit)
{
	GetFrameProTLS()->SetCustomTimeSpanStat(name, value, p_unit);

	AddCustomStat(name, value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::SetScopeCustomStat(const char* p_name, int value, const char* p_graph, const char* p_unit)
{
	SetScopeCustomStat(p_name, (int64)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::SetScopeCustomStat(const wchar_t* p_name, int value, const wchar_t* p_graph, const wchar_t* p_unit)
{
	SetScopeCustomStat(p_name, (int64)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::SetScopeCustomStat(StringId name, int value, const char* p_graph, const char* p_unit)
{
	SetScopeCustomStat(name, (int64)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::SetScopeCustomStat(const char* p_name, float value, const char* p_graph, const char* p_unit)
{
	SetScopeCustomStat(p_name, (double)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::SetScopeCustomStat(const wchar_t* p_name, float value, const wchar_t* p_graph, const wchar_t* p_unit)
{
	SetScopeCustomStat(p_name, (double)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::SetScopeCustomStat(StringId name, float value, const char* p_graph, const char* p_unit)
{
	SetScopeCustomStat(name, (double)value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::SetScopeCustomStat(const char* p_name, double value, const char* p_graph, const char* p_unit)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	if (p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_name, PacketType::StringPacket);
		p_framepro_tls->SendString(p_graph, PacketType::StringPacket);
		p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
	}

	p_framepro_tls->SetCustomTimeSpanStat((StringId)p_name, value, p_unit);

	AddCustomStat(p_name, value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::SetScopeCustomStat(const wchar_t* p_name, double value, const wchar_t* p_graph, const wchar_t* p_unit)
{
	FrameProTLS* p_framepro_tls = GetFrameProTLS();

	if (p_framepro_tls->SendStringsImmediately())
	{
		p_framepro_tls->SendString(p_name, PacketType::StringPacket);
		p_framepro_tls->SendString(p_graph, PacketType::StringPacket);
		p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
	}

	p_framepro_tls->SetCustomTimeSpanStat((StringId)p_name, value, p_unit);

	AddCustomStat(p_name, value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::SetScopeCustomStat(StringId name, double value, const char* p_graph, const char* p_unit)
{
	GetFrameProTLS()->SetCustomTimeSpanStat(name, value, p_unit);

	AddCustomStat(name, value, p_graph, p_unit);
}

//------------------------------------------------------------------------
void FramePro::SetConditionalScopeMinTimeInMicroseconds(int64 value)
{
	GetFrameProSession().SetConditionalScopeMinTimeInMicroseconds(value);
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED
//------------------------------------------------------------------------
// FrameProCallstackSet.cpp



//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	const int g_CallstackSetInitialCapacity = 4096;		// must be a power of 2

	//------------------------------------------------------------------------
	inline bool StacksMatch(FramePro::Callstack* p_callstack, uint64* p_stack, int stack_size, unsigned int hash)
	{
		if(p_callstack->m_Size != stack_size)
			return false;

		if(p_callstack->m_Hash != hash)
			return false;

		for(int i=0; i<stack_size; ++i)
			if(p_callstack->mp_Stack[i] != p_stack[i])
				return false;

		return true;
	}
}

//------------------------------------------------------------------------
FramePro::CallstackSet::CallstackSet(Allocator* p_allocator)
:	mp_Data((Callstack**)p_allocator->Alloc(g_CallstackSetInitialCapacity*sizeof(Callstack*))),
	m_CapacityMask(g_CallstackSetInitialCapacity-1),
	m_Count(0),
	m_Capacity(g_CallstackSetInitialCapacity),
	mp_Allocator(p_allocator),
	m_BlockAllocator(p_allocator)
{
	memset(mp_Data, 0, g_CallstackSetInitialCapacity*sizeof(Callstack*));
}

//------------------------------------------------------------------------
FramePro::CallstackSet::~CallstackSet()
{
	Clear();
}

//------------------------------------------------------------------------
void FramePro::CallstackSet::Grow()
{
	int old_capacity = m_Capacity;
	Callstack** p_old_data = mp_Data;

	// allocate a new set
	m_Capacity *= 2;
	m_CapacityMask = m_Capacity - 1;
	int size = m_Capacity * sizeof(Callstack*);
	mp_Data = (Callstack**)mp_Allocator->Alloc(size);
	memset(mp_Data, 0, size);

	// transfer callstacks from old set
	m_Count = 0;
	for(int i=0; i<old_capacity; ++i)
	{
		Callstack* p_callstack = p_old_data[i];
		if(p_callstack)
			Add(p_callstack);
	}

	// release old buffer
	mp_Allocator->Free(p_old_data);
}

//------------------------------------------------------------------------
FramePro::Callstack* FramePro::CallstackSet::Get(uint64* p_stack, int stack_size, unsigned int hash)
{
	int index = hash & m_CapacityMask;

	while(mp_Data[index] && !StacksMatch(mp_Data[index], p_stack, stack_size, hash))
		index = (index + 1) & m_CapacityMask;

	return mp_Data[index];
}

//------------------------------------------------------------------------
FramePro::Callstack* FramePro::CallstackSet::Add(uint64* p_stack, int stack_size, unsigned int hash)
{
	// grow the set if necessary
	if(m_Count > m_Capacity/4)
		Grow();

	// create a new callstack
	Callstack* p_callstack = (Callstack*)m_BlockAllocator.Alloc(sizeof(Callstack));
	p_callstack->m_ID = m_Count;
	p_callstack->m_Size = stack_size;
	p_callstack->mp_Stack = (uint64*)m_BlockAllocator.Alloc(stack_size*sizeof(uint64));
	p_callstack->m_Hash = hash;
	memcpy(p_callstack->mp_Stack, p_stack, stack_size*sizeof(uint64));

	Add(p_callstack);

	return p_callstack;
}

//------------------------------------------------------------------------
void FramePro::CallstackSet::Add(Callstack* p_callstack)
{
	// find a clear index
	int index = p_callstack->m_Hash & m_CapacityMask;
	while(mp_Data[index])
		index = (index + 1) & m_CapacityMask;

	mp_Data[index] = p_callstack;

	++m_Count;
}

//------------------------------------------------------------------------
void FramePro::CallstackSet::Clear()
{
	m_BlockAllocator.Clear();

	mp_Allocator->Free(mp_Data);

	size_t size = g_CallstackSetInitialCapacity*sizeof(Callstack*);
	mp_Data = (Callstack**)mp_Allocator->Alloc((int)size);
	memset(mp_Data, 0, size);
	m_CapacityMask = g_CallstackSetInitialCapacity-1;
	m_Count = 0;
	m_Capacity = g_CallstackSetInitialCapacity;
}

//------------------------------------------------------------------------
#endif		// #ifdef ENABLE_MEMPRO
//------------------------------------------------------------------------
// FrameProLib.cpp


//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#include <stdarg.h>

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	void DebugWrite(const FRAMEPRO_TCHAR* p_str, ...)
	{
		va_list args;
		va_start(args, p_str);

		static FRAMEPRO_TCHAR g_TempString[1024];
		_vstprintf_s(g_TempString, sizeof(g_TempString)/sizeof(FRAMEPRO_TCHAR), p_str, args);
		OutputDebugString(g_TempString);

		va_end(args);
	}
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED
//------------------------------------------------------------------------
// FrameProSession.cpp





#include <ctime>

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#if FRAMEPRO_PLATFORM_WIN
	#if FRAMEPRO_TOOLSET_UE4
		#include "Windows/AllowWindowsPlatformTypes.h"
	#endif
	#include <psapi.h>
	#if FRAMEPRO_TOOLSET_UE4
		#include "Windows/HideWindowsPlatformTypes.h"
	#endif
#endif

//------------------------------------------------------------------------
// if you are having problems compiling this on your platform undefine FRAMEPRO_ENUMERATE_MODULES and it send info for just the main module
#define FRAMEPRO_ENUMERATE_MODULES (!FRAMEPRO_PLATFORM_XBOXONE && FRAMEPRO_ENABLE_CALLSTACKS && FRAMEPRO_X64 && 1)

//------------------------------------------------------------------------
#if FRAMEPRO_ENUMERATE_MODULES
    #if FRAMEPRO_WIN_BASED_PLATFORM

		#ifdef __UNREAL__
			#include "AllowWindowsPlatformTypes.h"
		#endif

		#pragma warning(push)
		#pragma warning(disable : 4091)
			#include <Dbghelp.h>
		#pragma warning(pop)

		#pragma comment(lib, "Dbghelp.lib")

		#ifdef __UNREAL__
			#include "HideWindowsPlatformTypes.h"
		#endif
	#else
        #include <link.h>
    #endif
#endif

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
// START EPIC
	const TCHAR* g_NonInteractiveRecordingFilePath = TEXT("framepro_recording.bin");
// END EPIC

	//------------------------------------------------------------------------
	FrameProSession* FrameProSession::mp_Inst = NULL;

	//------------------------------------------------------------------------
	class DefaultAllocator : public Allocator
	{
	public:
		void* Alloc(size_t size) { return new char[size]; }
		void Free(void* p) { delete[] (char*)p; }
	};

	//------------------------------------------------------------------------
	void GetDateString(char* p_date, size_t len)
	{
		time_t rawtime;
		time(&rawtime);

		tm timeinfo;
		localtime_s(&timeinfo, &rawtime);

		strftime(p_date, len, "%d-%m-%Y %I:%M:%S", &timeinfo);
	}

	//------------------------------------------------------------------------
	void BaseAddressLookupFunction()
	{
	}

	//------------------------------------------------------------------------
	bool GetProcessName(int process_id, char* p_name, int max_name_length)
	{
#if FRAMEPRO_PLATFORM_WIN
		HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, true, process_id);
		if(process)
		{
			unsigned long result = GetProcessImageFileNameA(process, p_name, max_name_length);
			CloseHandle(process);

			if(result)
			{
				int total_length = (int)strlen(p_name);
				char* p_filename = strrchr(p_name, '\\');
				if(p_filename && p_filename[1])
				{
					++p_filename;
					memmove(p_name, p_filename, p_name + total_length + 1 - p_filename);
				}

				return true;
			}
		}
#endif
		return false;
	}

	//------------------------------------------------------------------------
	FrameProSession::FrameProSession()
	:	mp_Allocator(NULL),
		m_CreatedAllocator(false),
		m_Initialised(false),
		m_InitialiseConnectionNextFrame(false),
		m_StartContextSwitchRecording(false),
		m_ClockFrequency(0),
		m_MainThreadId(-1),
		m_SendThreadStarted(false, true),
		m_SendReady(false, true),
		m_SendComplete(false, false),
		m_ReceiveThreadTerminatedEvent(false, false),
		m_Interactive(true),
		mp_NonInteractiveRecordingFile(NULL),
		m_NonInteractiveRecordingFileSize(0),
		m_LastSessionInfoSendTime(0),
		mp_RecordingFile(NULL),
		m_RecordingFileSize(0),
		m_MaxRecordingFileSize(0),
		m_ThreadPrioritySet(false),
		m_ThreadPriority(0),
		m_ThreadAffinitySet(false),
		m_ThreadAffinity(0),
		m_SendThreadExit(false),
		m_SendThreadFinished(false, true),
		m_SocketsBlocked(FRAMEPRO_SOCKETS_BLOCKED_BY_DEFAULT),
		mp_GlobalHiResTimers(NULL)
#if FRAMEPRO_EVENT_TRACE_WIN32
		,mp_EventTraceWin32(NULL)
#endif
#if FRAMEPRO_ENABLE_CALLSTACKS
		,m_SendModules(false)
#endif
	{
		mp_Inst = this;

		strcpy_s(m_Port, FRAMEPRO_PORT);

		CalculateTimerFrequency();
	}

	//------------------------------------------------------------------------
	FrameProSession::~FrameProSession()
	{
		HandleDisconnect();

		m_NamedThreads.Clear();

#if FRAMEPRO_EVENT_TRACE_WIN32
		if(mp_EventTraceWin32)
			Delete(mp_Allocator, mp_EventTraceWin32);
#endif
		// must clear all arrays and buffers and detach the allocator before deleting the allocator
		m_ProcessIds.Clear();
		m_ProcessIds.SetAllocator(NULL);
		
		m_MainThreadSendBuffer.ClearAndFree();
		m_MainThreadSendBuffer.SetAllocator(NULL);

		m_StringRequestPackets.Clear();
		m_StringRequestPackets.SetAllocator(NULL);

		m_ModulePackets.Clear();
		m_ModulePackets.SetAllocator(NULL);

		m_NamedThreads.Clear();
		m_NamedThreads.SetAllocator(NULL);

		m_Connectionchangedcallbacks.Clear();
		m_Connectionchangedcallbacks.SetAllocator(NULL);

		if(m_CreatedAllocator)
			delete mp_Allocator;
	}

	//------------------------------------------------------------------------
	void FrameProSession::SetPort(int port)
	{
		sprintf_s(m_Port, "%d", port);
	}

	//------------------------------------------------------------------------
	void FrameProSession::SetAllocator(Allocator* p_allocator)
	{
		if(mp_Allocator)
			FRAMEPRO_BREAK();		// allocator already set. You must call Allocator BEFORE calling FRAMEPRO_FRAME_START

		FRAMEPRO_ASSERT(p_allocator);
		mp_Allocator = p_allocator;
	}

	//------------------------------------------------------------------------
	Allocator* FrameProSession::CreateDefaultAllocator()
	{
		CriticalSectionScope lock(m_CriticalSection);

		if(!mp_Allocator)
		{
			mp_Allocator = new DefaultAllocator();
			m_CreatedAllocator = true;
		}

		return mp_Allocator;
	}

	//------------------------------------------------------------------------
	void FrameProSession::CalculateTimerFrequency()
	{
// START EPIC
		m_ClockFrequency = (1.0 / FPlatformTime::GetSecondsPerCycle());
// END EPIC
	}

	//------------------------------------------------------------------------
	int FrameProSession::StaticSendThreadMain(void* p_arg)
	{
		FrameProSession* p_this = (FrameProSession*)p_arg;
		return p_this->SendThreadMain();
	}

	//------------------------------------------------------------------------
	int FrameProSession::SendThreadMain()
	{
		SetThreadName("FramePro Send Thread");

		m_SendThreadStarted.Set();

		m_SendReady.Wait();

		while(!m_SendThreadExit)
		{
			int64 start_time;
			FRAMEPRO_GET_CLOCK_COUNT(start_time);

			{
				FRAMEPRO_NAMED_SCOPE("FramePro Send");
				SendFrameBuffer();
			}

			int64 end_time;
			FRAMEPRO_GET_CLOCK_COUNT(end_time);

			m_SendComplete.Set();

			int sleep_time = FRAMEPRO_MAX_SEND_DELAY - (int)((end_time - start_time) * 1000 / m_ClockFrequency);
			if(sleep_time > 0)
				m_SendReady.Wait(sleep_time);
		}

		SendFrameBuffer();

		m_SendComplete.Set();

		m_SendThreadFinished.Set();

		return 0;
	}

	//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED
	int FrameProSession::StaticConnectThreadMain(void* p_arg)
	{
		FrameProSession* p_this = (FrameProSession*)p_arg;
		return p_this->ConnectThreadMain();
	}
#endif

	//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED
	int FrameProSession::ConnectThreadMain()
	{
		if(m_SocketsBlocked)
			return 0;

		{
			CriticalSectionScope lock(m_CriticalSection);
			if(mp_RecordingFile)
			{
				m_ListenSocket.Disconnect();		// don't allow connections while recording
				return 0;
			}
		}

		bool accepted = m_ListenSocket.Accept(m_ClientSocket);

		if(accepted)
			m_InitialiseConnectionNextFrame = true;

		return 0;
	}
#endif

	//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED
	int FrameProSession::StaticReceiveThreadMain(void* p_arg)
	{
		FrameProSession* p_this = (FrameProSession*)p_arg;
		int ret = p_this->ReceiveThreadMain();

		DestroyFrameProTLS(GetFrameProTLS());

		return ret;
	}
#endif

	//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED
	void FrameProSession::SendOnMainThread(void* p_src, int size)
	{
		CriticalSectionScope tls_lock(m_MainThreadSendBufferLock);

		void* p_dst = m_MainThreadSendBuffer.Allocate(size);
		memcpy(p_dst, p_src, size);
	}
#endif

	//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED
	int FrameProSession::ReceiveThreadMain()
	{
		while(g_Connected)
		{
			int packet_type;

			if (m_ClientSocket.Receive(&packet_type, sizeof(packet_type)) != sizeof(packet_type))
			{
				m_ReceiveThreadTerminatedEvent.Set();
				return OnReceiveThreadExit();
			}

			int padding = 0;
			if (m_ClientSocket.Receive(&padding, sizeof(padding)) != sizeof(padding))
			{
				m_ReceiveThreadTerminatedEvent.Set();
				return OnReceiveThreadExit();
			}

			switch(packet_type)
			{
				case PacketType::RequestStringLiteralPacket:
				{
					RequestStringLiteralPacket packet;
					if (m_ClientSocket.Receive(&packet, sizeof(packet)) != sizeof(packet))
					{
						m_ReceiveThreadTerminatedEvent.Set();
						return OnReceiveThreadExit();
					}

					{
						CriticalSectionScope tls_lock(m_StringRequestPacketsLock);
						m_StringRequestPackets.Add(packet);
					}
				} break;

				case PacketType::SetConditionalScopeMinTimePacket:
				{
					SetConditionalScopeMinTimePacket packet;
					if (m_ClientSocket.Receive(&packet, sizeof(packet)) != sizeof(packet))
					{
						m_ReceiveThreadTerminatedEvent.Set();
						return OnReceiveThreadExit();
					}

					g_ConditionalScopeMinTime = packet.m_MinTime;
				} break;

				case PacketType::ConnectResponsePacket:
				{
					ConnectResponsePacket packet;
					if (m_ClientSocket.Receive(&packet, sizeof(packet)) != sizeof(packet))
					{
						m_ReceiveThreadTerminatedEvent.Set();
						return OnReceiveThreadExit();
					}

					{
						CriticalSectionScope lock(m_SendFrameBufferCriticalSection);

						if(!packet.m_Interactive)
						{
							FRAMEPRO_ASSERT(!mp_NonInteractiveRecordingFile);
// START EPIC
							FString FileName = FPaths::ProfilingDir() + TEXT("FramePro/") + g_NonInteractiveRecordingFilePath;
							
							IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
							mp_NonInteractiveRecordingFile = PlatformFile.OpenWrite(*FileName);
// END EPIC
						}

						m_Interactive = packet.m_Interactive ? 1 : 0;

						{
							CriticalSectionScope tls_lock(m_TLSListCriticalSection);
							for(FrameProTLS* p_iter=m_FrameProTLSList.GetHead(); p_iter!=NULL; p_iter=p_iter->GetNext())
								p_iter->SetInteractive(m_Interactive);
						}
					}

					if(packet.m_RecordContextSwitches)
						StartRecordingContextSitches();

				} break;

				case PacketType::RequestRecordedDataPacket:
				{
					SendRecordedDataAndDisconnect();
				} break;

				case PacketType::SetCallstackRecordingEnabledPacket:
				{
					#if FRAMEPRO_ENABLE_CALLSTACKS
						SetCallstackRecordingEnabledPacket packet;
						if (m_ClientSocket.Receive(&packet, sizeof(packet)) == sizeof(packet))
						{
							// enumerate modules
							if (!m_SendModules)
							{
								EnumerateModules();
								m_SendModules = true;
							}

							{
								CriticalSectionScope tls_lock(m_TLSListCriticalSection);
								for (FrameProTLS* p_tls = m_FrameProTLSList.GetHead(); p_tls != NULL; p_tls = p_tls->GetNext())
									p_tls->SetSendCallstacks(packet.m_Enabled != 0);
							}
						}
					#endif
				} break;
			}
		}

		m_ReceiveThreadTerminatedEvent.Set();
		return 0;
	}
#endif

	//------------------------------------------------------------------------
	void FrameProSession::StartRecordingContextSitches()
	{
#if FRAMEPRO_EVENT_TRACE_WIN32
		if(!mp_EventTraceWin32)
			mp_EventTraceWin32 = New<EventTraceWin32>(mp_Allocator, mp_Allocator);

		DynamicString error(GetAllocator());
		bool started = mp_EventTraceWin32->Start(ContextSwitchCallback_Static, this, error);

		// send the context switch started packet
		ContextSwitchRecordingStartedPacket response_packet;
		response_packet.m_PacketType = PacketType::ContextSwitchRecordingStartedPacket;
		response_packet.m_StartedSucessfully = started;
		error.CopyTo(response_packet.m_Error, sizeof(response_packet.m_Error));

		SendOnMainThread(response_packet);

		if(!started)
		{
			#if FRAMEPRO_PLATFORM_WIN || FRAMEPRO_PLATFORM_UWP
				FramePro::DebugWrite(FRAMEPRO_STRING("FramePro Warning: Failed to start recording context switches. Please make sure that you are running with administrator privileges.\n"));
			#else
				FramePro::DebugWrite(FRAMEPRO_STRING("FramePro Warning: Failed to start recording context switches. Context switches may not be supported for this platform\n"));
			#endif
		}
#endif
	}

	//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED
	int FrameProSession::OnReceiveThreadExit()
	{
		HandleDisconnect();
		return 0;
	}
#endif

	//------------------------------------------------------------------------
	void FrameProSession::CreateSendThread()
	{
		m_CriticalSection.Leave();

		m_SendThread.CreateThread(StaticSendThreadMain, this);

		if(m_ThreadPrioritySet)
			m_SendThread.SetPriority(m_ThreadPriority);

		if(m_ThreadAffinitySet)
			m_SendThread.SetAffinity(m_ThreadAffinity);
	
		m_SendThreadStarted.Wait();

		m_CriticalSection.Enter();
	}

	//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED
	void FrameProSession::CreateReceiveThread()
	{
		m_ReceiveThreadTerminatedEvent.Reset();

		m_ReceiveThread.CreateThread(StaticReceiveThreadMain, this);

		if(m_ThreadPrioritySet)
			m_ReceiveThread.SetPriority(m_ThreadPriority);

		if(m_ThreadAffinitySet)
			m_ReceiveThread.SetAffinity(m_ThreadAffinity);
	}
#endif

	//------------------------------------------------------------------------
	void FrameProSession::ContextSwitchCallback_Static(const ContextSwitch& context_switch, void* p_param)
	{
		FrameProSession* p_this = (FrameProSession*)p_param;
		p_this->ContextSwitchCallback(context_switch);
	}

	//------------------------------------------------------------------------
	void FrameProSession::ContextSwitchCallback(const ContextSwitch& context_switch)
	{
		FrameProTLS* p_framepro_tls = GetFrameProTLS();

		// send the process name string
		if(!m_ProcessIds.Contains(context_switch.m_ProcessId))
		{
			m_ProcessIds.SetAllocator(GetAllocator());
			m_ProcessIds.Add(context_switch.m_ProcessId);

			const int max_process_name_length = 260;
			char process_name[max_process_name_length];
			if(GetProcessName(context_switch.m_ProcessId, process_name, max_process_name_length))
			{
				StringId name_id = RegisterString(process_name);
				p_framepro_tls->SendSessionInfoPacket(ProcessNamePacket(context_switch.m_ProcessId, name_id));
			}
		}

		CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

		// send the context switch packet
		ContextSwitchPacket* p_packet = p_framepro_tls->AllocateSpaceInBuffer<ContextSwitchPacket>();

		p_packet->m_PacketType = PacketType::ContextSwitchPacket;
		p_packet->m_ProcessId = context_switch.m_ProcessId;
		p_packet->m_CPUId = context_switch.m_CPUId;
		p_packet->m_Timestamp = context_switch.m_Timestamp;
		p_packet->m_OldThreadId = context_switch.m_OldThreadId;
		p_packet->m_NewThreadId = context_switch.m_NewThreadId;
		p_packet->m_OldThreadState = context_switch.m_OldThreadState;
		p_packet->m_OldThreadWaitReason = context_switch.m_OldThreadWaitReason;
		p_packet->m_Padding = 0;
	}

	//------------------------------------------------------------------------
	Platform::Enum GetPlatformEnum()
	{
#if FRAMEPRO_PLATFORM_WIN
		return Platform::Windows;
#elif FRAMEPRO_PLATFORM_UWP
		return Platform::Windows_UWP;
#elif FRAMEPRO_PLATFORM_XBOXONE
		return Platform::XBoxOne;
#elif FRAMEPRO_PLATFORM_XBOX360
		return Platform::XBox360;
// START EPIC
#elif FRAMEPRO_PLATFORM_ANDROID || FRAMEPRO_PLATFORM_UNIX || FRAMEPRO_PLATFORM_IOS || FRAMEPRO_PLATFORM_SWITCH
// END EPIC
		return Platform::Unix;
#else
		#error
#endif
	}

	//------------------------------------------------------------------------
#if FRAMEPRO_WIN_BASED_PLATFORM
	struct CV_HEADER
	{
		int Signature;
		int Offset;
	};

	struct CV_INFO_PDB20
	{
		CV_HEADER CvHeader;
		int Signature;
		int Age;
		char PdbFileName[FRAMEPRO_MAX_PATH];
	};

	struct CV_INFO_PDB70
	{
		int  CvSignature;
		GUID Signature;
		int Age;
		char PdbFileName[FRAMEPRO_MAX_PATH];
	};
#endif

	//------------------------------------------------------------------------
	void GetExtraModuleInfo(int64 ModuleBase, ModulePacket* p_module_packet)
	{
#if FRAMEPRO_PLATFORM_WIN
		IMAGE_DOS_HEADER* p_dos_header = (IMAGE_DOS_HEADER*)ModuleBase;
		IMAGE_NT_HEADERS* p_nt_header = (IMAGE_NT_HEADERS*)((char*)ModuleBase + p_dos_header->e_lfanew);
		IMAGE_OPTIONAL_HEADER& optional_header = p_nt_header->OptionalHeader;
		IMAGE_DATA_DIRECTORY& image_data_directory = optional_header.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
		IMAGE_DEBUG_DIRECTORY* p_debug_info_array = (IMAGE_DEBUG_DIRECTORY*)(ModuleBase + image_data_directory.VirtualAddress);
		int count = image_data_directory.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
		for(int i=0; i<count; ++i)
		{
			if(p_debug_info_array[i].Type == IMAGE_DEBUG_TYPE_CODEVIEW)
			{
				char* p_cv_data = (char*)(ModuleBase + p_debug_info_array[i].AddressOfRawData);
				if(strncmp(p_cv_data, "RSDS", 4) == 0)
				{
					CV_INFO_PDB70* p_cv_info = (CV_INFO_PDB70*)p_cv_data;

					p_module_packet->m_PacketType = PacketType::ModulePacket;
					p_module_packet->m_Age = p_cv_info->Age;
					
					static_assert(sizeof(p_module_packet->m_Sig) == sizeof(p_cv_info->Signature), "sig size wrong");
					memcpy(p_module_packet->m_Sig, &p_cv_info->Signature, sizeof(p_cv_info->Signature));

					strcpy_s(p_module_packet->m_SymbolFilename, p_cv_info->PdbFileName);

					return;									// returning here
				}
				else if(strncmp(p_cv_data, "NB10", 4) == 0)
				{
					CV_INFO_PDB20* p_cv_info = (CV_INFO_PDB20*)p_cv_data;

					p_module_packet->m_PacketType = PacketType::ModulePacket;
					p_module_packet->m_Age = p_cv_info->Age;

					memset(p_module_packet->m_Sig, 0, sizeof(p_module_packet->m_Sig));
					static_assert(sizeof(p_cv_info->Signature) <= sizeof(p_module_packet->m_Sig), "sig size wrong");
					memcpy(p_module_packet->m_Sig, &p_cv_info->Signature, sizeof(p_cv_info->Signature));

					strcpy_s(p_module_packet->m_SymbolFilename, p_cv_info->PdbFileName);

					return;									// returning here
				}
			}
		}
#endif
	}

	//------------------------------------------------------------------------
#if FRAMEPRO_ENUMERATE_MODULES
    #if FRAMEPRO_WIN_BASED_PLATFORM
		#if !defined(_IMAGEHLP_SOURCE_) && defined(_IMAGEHLP64)
			// depending on your platform you may need to change PCSTR to PSTR for ModuleName
			BOOL CALLBACK EnumerateLoadedModulesCallback(__in PCSTR ModuleName,__in DWORD64 ModuleBase,__in ULONG,__in_opt PVOID UserContext)
		#else
			BOOL CALLBACK EnumerateLoadedModulesCallback(__in PCSTR ModuleName,__in ULONG ModuleBase,__in ULONG,__in_opt PVOID UserContext)
		#endif
			{
				FrameProSession* p_this = (FrameProSession*)UserContext;

				p_this->EnumerateLoadedModulesCallback(ModuleBase, ModuleName, false);

				return true;
			}
	#else
		int EnumerateLoadedModulesCallback(struct dl_phdr_info* info, size_t size, void* data)
		{
			FrameProSession* p_this = (FrameProSession*)data;

			int64 module_base = 0;
			for (int j = 0; j < info->dlpi_phnum; j++)
			{
				if (info->dlpi_phdr[j].p_type == PT_LOAD)
				{
					module_base = info->dlpi_addr + info->dlpi_phdr[j].p_vaddr;
					break;
				}
			}

			static bool first = true;
			if(first)
			{
				first = false;

				int64 module_base = (int64)BaseAddressLookupFunction;		// use the address of the BaseAddressLookupFunction function so that we can work it out later

				// get the module name
				char arg1[20];
				char char_filename[FRAMEPRO_MAX_PATH];
				sprintf(arg1, "/proc/%d/exe", getpid());
				memset(char_filename, 0, FRAMEPRO_MAX_PATH);
				readlink(arg1, char_filename, FRAMEPRO_MAX_PATH -1);

				p_this->EnumerateLoadedModulesCallback(
					module_base,
					char_filename,
					true);
			}
			else
			{
				p_this->EnumerateLoadedModulesCallback(
					module_base,
					info->dlpi_name,
					false);
			}

			return 0;
		}
	#endif
#endif

	//------------------------------------------------------------------------
	void FrameProSession::EnumerateLoadedModulesCallback(
		int64 module_base,
		const char* p_module_name,
		bool use_lookup_function_for_base_address)
	{
		CriticalSectionScope lock(m_CriticalSection);

		ModulePacket* p_module_packet = (ModulePacket*)mp_Allocator->Alloc(sizeof(ModulePacket));
		memset(p_module_packet, 0, sizeof(ModulePacket));

		p_module_packet->m_ModuleBase = module_base;
		strcpy_s(p_module_packet->m_ModuleName, p_module_name);
		p_module_packet->m_UseLookupFunctionForBaseAddress = use_lookup_function_for_base_address ? 1 : 0;

		GetExtraModuleInfo(module_base, p_module_packet);

		m_ModulePackets.Add(p_module_packet);
	}

	//------------------------------------------------------------------------
	void FrameProSession::EnumerateModules()
	{
		// if you are having problems compiling this on your platform unset FRAMEPRO_ENUMERATE_MODULES and it send info for just the main module
		#if FRAMEPRO_ENUMERATE_MODULES
			#if FRAMEPRO_WIN_BASED_PLATFORM
				EnumerateLoadedModules64(GetCurrentProcess(), FramePro::EnumerateLoadedModulesCallback, this);
			#else
				dl_iterate_phdr(FramePro::EnumerateLoadedModulesCallback, this);
			#endif
		#endif

		if (!m_ModulePackets.GetCount())
		{
			// if FRAMEPRO_ENUMERATE_MODULES is set or enumeration failed for some reason, fall back
			// to getting the base address for the main module. This will always work for for all platforms.

			ModulePacket* p_module_packet = (ModulePacket*)mp_Allocator->Alloc(sizeof(ModulePacket));
			memset(p_module_packet, 0, sizeof(ModulePacket));

			p_module_packet->m_PacketType = PacketType::ModulePacket;
			p_module_packet->m_UseLookupFunctionForBaseAddress = 0;

			#if FRAMEPRO_WIN_BASED_PLATFORM
				static int module = 0;
				HMODULE module_handle = 0;
				GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)&module, &module_handle);

				p_module_packet->m_ModuleBase = (int64)module_handle;

				TCHAR tchar_filename[FRAMEPRO_MAX_PATH] = { 0 };
				GetModuleFileName(NULL, tchar_filename, FRAMEPRO_MAX_PATH);

				#ifdef UNICODE
					size_t chars_converted = 0;
					wcstombs_s(&chars_converted, p_module_packet->m_SymbolFilename, tchar_filename, FRAMEPRO_MAX_PATH);
				#else
					strcpy_s(p_module_packet->m_SymbolFilename, tchar_filename);
				#endif
// BEGIN EPIC
			#elif FRAMEPRO_UE4_BASED_PLATFORM
				p_module_packet->m_UseLookupFunctionForBaseAddress = 1;
				p_module_packet->m_ModuleBase = (int64)BaseAddressLookupFunction;
				strcpy_s( p_module_packet->m_SymbolFilename, FRAMEPRO_MAX_INLINE_STRING_LENGTH-1, TCHAR_TO_ANSI(FPlatformProcess::ExecutableName(false)) );
// END EPIC
			#else
				p_module_packet->m_UseLookupFunctionForBaseAddress = 1;

				p_module_packet->m_ModuleBase = (int64)BaseAddressLookupFunction;		// use the address of the BaseAddressLookupFunction function so that we can work it out later

				// get the module name
				char arg1[20];
				sprintf(arg1, "/proc/%d/exe", getpid());
				memset(p_module_packet->m_SymbolFilename, 0, FRAMEPRO_MAX_PATH);
				readlink(arg1, p_module_packet->m_SymbolFilename, FRAMEPRO_MAX_PATH -1);
			#endif

			m_ModulePackets.Add(p_module_packet);
		}

		// send module packets
		for (int i = 0; i < m_ModulePackets.GetCount(); ++i)
		{
			ModulePacket* p_module_packet = m_ModulePackets[i];
			SendImmediate(p_module_packet, sizeof(ModulePacket), GetFrameProTLS());
			mp_Allocator->Free(p_module_packet);
		}
		m_ModulePackets.Clear();
	}

	//------------------------------------------------------------------------
	void FrameProSession::InitialiseConnection(FrameProTLS* p_framepro_tls)
	{
		// start the Send thread FIRST, but paused (because it adds another TLS that we need to call OnConnected on)
		m_SendComplete.Reset();
		m_SendReady.Reset();
		CreateSendThread();

		// call OnConnected on all TLS threads
		bool recording_to_file = mp_RecordingFile != NULL;
		{
			CriticalSectionScope tls_lock(m_TLSListCriticalSection);
			for(FrameProTLS* p_tls = m_FrameProTLSList.GetHead(); p_tls!=NULL; p_tls=p_tls->GetNext())
				p_tls->OnConnected(recording_to_file);
		}

		p_framepro_tls->SendConnectPacket(m_ClockFrequency, GetCurrentProcessId(), GetPlatformEnum());

		// tell the send thread that there is data ready and wait for it to be sent
		m_SendReady.Set();
		m_CriticalSection.Leave();
		m_SendComplete.Wait();
		m_CriticalSection.Enter();
		m_SendComplete.Reset();

		// make sure no more TLS threads are added while we are setting stuff up
		m_TLSListCriticalSection.Enter();

		// lock the session info buffers of all threads
		for(FrameProTLS* p_tls = m_FrameProTLSList.GetHead(); p_tls!=NULL; p_tls=p_tls->GetNext())
		{
			p_tls->OnConnected(recording_to_file);		// in case any threads have been added since sending the connect packet
			p_tls->LockSessionInfoBuffer();
		}

		// send the session info buffers for all threads
		for(FrameProTLS* p_tls = m_FrameProTLSList.GetHead(); p_tls!=NULL; p_tls=p_tls->GetNext())
			p_tls->SendSessionInfoBuffer();

		p_framepro_tls->SendFrameStartPacket(0);

		g_ConditionalScopeMinTime = (unsigned int)((((int64)FRAMEPRO_DEFAULT_COND_SCOPE_MIN_TIME) * m_ClockFrequency) / 1000000LL);

		// Do this (almost) last. Threads will start sending data once this is set. This atomic flag also publishes all the above data.
		std::atomic_thread_fence(std::memory_order_seq_cst);
		g_Connected = true;

#if FRAMEPRO_SOCKETS_ENABLED
		// create the receive thread. Must do this AFTER setting g_Connected
		if (!mp_RecordingFile)
			CreateReceiveThread();
#endif

		// unlock the session info buffers of all threads. Must do this after g_Connected is set
		for(FrameProTLS* p_tls = m_FrameProTLSList.GetHead(); p_tls!=NULL; p_tls=p_tls->GetNext())
			p_tls->UnlockSessionInfoBuffer();

		m_TLSListCriticalSection.Leave();
		
		// start recording context switches if dumping to a file
		if(m_StartContextSwitchRecording)
		{
			StartRecordingContextSitches();
			m_StartContextSwitchRecording = false;
		}

		ClearGlobalHiResTimers();

		OnConnectionChanged(true);
	}

	//------------------------------------------------------------------------
	void FrameProSession::Initialise(FrameProTLS* p_framepro_tls)
	{
		if(!HasSetThreadName(p_framepro_tls->GetThreadId()))
			p_framepro_tls->SetThreadName(p_framepro_tls->GetThreadId(), "Main Thread");

		{
			CriticalSectionScope tls_lock(m_MainThreadSendBufferLock);
			m_MainThreadSendBuffer.SetAllocator(GetAllocator());
		}

		{
			CriticalSectionScope tls_lock(m_StringRequestPacketsLock);
			m_StringRequestPackets.SetAllocator(GetAllocator());
		}

		m_ModulePackets.SetAllocator(GetAllocator());

#if FRAMEPRO_SOCKETS_ENABLED
		OpenListenSocket();
		StartConnectThread();
#endif
	}

	//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED
	void FrameProSession::OpenListenSocket()
	{
		if(m_SocketsBlocked)
			return;

		bool bind_result = m_ListenSocket.Bind(m_Port);

		if(!bind_result)
		{
			DebugWrite(FRAMEPRO_STRING("FramePro ERROR: Failed to bind port. This usually means that another process is already running with FramePro enabled.\n"));
			return;
		}

		if(!m_ListenSocket.StartListening())
		{
			DebugWrite(FRAMEPRO_STRING("FramePro ERROR: Failed to start listening on socket\n"));
		}
	}
#endif

	//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED
	void FrameProSession::StartConnectThread()
	{
		m_ConnectThread.CreateThread(StaticConnectThreadMain, this);
	}
#endif

	//------------------------------------------------------------------------
	bool FrameProSession::SendSendBuffer(SendBuffer* p_send_buffer, Socket& socket)
	{
#if FRAMEPRO_DEBUG_TCP
		static FILE* p_debug_file = NULL;
		if (!p_debug_file)
			fopen_s(&p_debug_file, "framepro_network_data.framepro_recording", "wb");
		fwrite(p_send_buffer->GetBuffer(), p_send_buffer->GetSize(), 1, p_debug_file);
#endif

		return socket.Send(p_send_buffer->GetBuffer(), p_send_buffer->GetSize());
	}

	//------------------------------------------------------------------------
// START EPIC
	void FrameProSession::WriteSendBuffer(SendBuffer* p_send_buffer, IFileHandle* p_file, int64& file_size)
	{
		int size = p_send_buffer->GetSize();
		p_file->Write((uint8*)p_send_buffer->GetBuffer(), size);
		file_size += size;
	}
// END EPIC

	//------------------------------------------------------------------------
	void FrameProSession::SendFrameBuffer()
	{
		CriticalSectionScope lock(m_SendFrameBufferCriticalSection);

		List<SendBuffer> send_buffer_list;

		// get all of the send buffers
		{
			CriticalSectionScope tls_lock(m_TLSListCriticalSection);
			for(FrameProTLS* p_framepro_tls = m_FrameProTLSList.GetHead(); p_framepro_tls!=NULL; p_framepro_tls=p_framepro_tls->GetNext())
				p_framepro_tls->CollectSendBuffers(send_buffer_list);
		}

		// send the send buffers
		for(SendBuffer* p_send_buffer = send_buffer_list.GetHead(); p_send_buffer; p_send_buffer = p_send_buffer->GetNext())
		{
// START EPIC
            CriticalSectionScope lock2(m_CriticalSection);
                
            if(mp_RecordingFile)
            {
                WriteSendBuffer(p_send_buffer, mp_RecordingFile, m_RecordingFileSize);
            }
            else
            {
#if FRAMEPRO_SOCKETS_ENABLED
                if(m_Interactive)
                {
                    if(!SendSendBuffer(p_send_buffer, m_ClientSocket))
                        break;        // disconnected
                }
                else
                {
                    WriteSendBuffer(p_send_buffer, mp_NonInteractiveRecordingFile, m_NonInteractiveRecordingFileSize);
                }
#endif
            }
// END EPIC
		}

		// give the empty send buffers back to the TLS objects
		SendBuffer* p_iter = send_buffer_list.GetHead();
		while(p_iter)
		{
			SendBuffer* p_next = p_iter->GetNext();

			p_iter->SetNext(NULL);
			p_iter->ClearSize();

			p_iter->GetOwner()->AddEmptySendBuffer(p_iter);

			p_iter = p_next;
		}

		{
			CriticalSectionScope tls_lock(m_TLSListCriticalSection);
			FrameProTLS* p_framepro_tls = m_FrameProTLSList.GetHead();
			while(p_framepro_tls)
			{
				FrameProTLS* p_next = p_framepro_tls->GetNext();

				if (p_framepro_tls->ShuttingDown())
				{
					m_TLSListCriticalSection.Leave();
					DestroyFrameProTLS(p_framepro_tls);
					m_TLSListCriticalSection.Enter();
				}

				p_framepro_tls = p_next;
			}
		}
	}

	//------------------------------------------------------------------------
	void FrameProSession::SendRecordedDataAndDisconnect()
	{
#if FRAMEPRO_SOCKETS_ENABLED
		CriticalSectionScope lock(m_SendFrameBufferCriticalSection);

		FRAMEPRO_ASSERT(!m_Interactive);

		g_Connected = false;

// START EPIC
		delete mp_NonInteractiveRecordingFile;
		mp_NonInteractiveRecordingFile = NULL;

		FString FileName = FPaths::ProfilingDir() + TEXT("FramePro/") + g_NonInteractiveRecordingFilePath;

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* p_read_file = PlatformFile.OpenRead(*FileName);

		size_t bytes_to_read = p_read_file->Size();

		const int block_size = 64*1024;
		uint8* p_read_buffer = (uint8*)mp_Allocator->Alloc(block_size);
		while(bytes_to_read)
		{
			size_t size_to_read = block_size < bytes_to_read ? block_size : bytes_to_read;
			p_read_file->Read(p_read_buffer, size_to_read);
			m_ClientSocket.Send(p_read_buffer, size_to_read);
			bytes_to_read -= size_to_read;
		}
		delete p_read_file;
		p_read_file = NULL;
// END EPIC
		mp_Allocator->Free(p_read_buffer);

		HandleDisconnect_NoLock();
#endif
	}

	//------------------------------------------------------------------------
	void FrameProSession::HandleDisconnect()
	{
		CriticalSectionScope lock(m_CriticalSection);
		if(g_Connected)
			HandleDisconnect_NoLock();
	}

	//------------------------------------------------------------------------
	void FrameProSession::HandleDisconnect_NoLock()
	{
#if FRAMEPRO_EVENT_TRACE_WIN32
		if(mp_EventTraceWin32)
			mp_EventTraceWin32->Stop();
#endif

#if FRAMEPRO_SOCKETS_ENABLED
		m_ClientSocket.Disconnect();
#endif
		g_Connected = false;

		// shut down the send thread
		if(m_SendThread.IsAlive())
		{
			m_SendThreadExit = true;
			m_SendReady.Set();
			m_CriticalSection.Leave();
			m_SendThreadFinished.Wait();
			m_CriticalSection.Enter();
			m_SendThreadExit = false;
		}

		// shut down the receive thread
		if(m_ReceiveThread.IsAlive())
		{
			m_CriticalSection.Leave();
			m_ReceiveThreadTerminatedEvent.Wait(10*1000);
			m_CriticalSection.Enter();
		}

		{
			CriticalSectionScope tls_lock(m_TLSListCriticalSection);
			for(FrameProTLS* p_framepro_tls = m_FrameProTLSList.GetHead(); p_framepro_tls!=NULL; p_framepro_tls=p_framepro_tls->GetNext())
				p_framepro_tls->OnDisconnected();
		}

		g_ConditionalScopeMinTime = UINT_MAX;

		m_InitialiseConnectionNextFrame = false;

        {
            CriticalSectionScope lock2(m_CriticalSection);
            if(mp_RecordingFile)
            {
                // START EPIC
                delete mp_RecordingFile;
                // END EPIC
                mp_RecordingFile = NULL;
            }
        }

#if FRAMEPRO_SOCKETS_ENABLED
		// start listening for new connections
		StartConnectThread();
#endif
		OnConnectionChanged(false);
	}

	//------------------------------------------------------------------------
	void FrameProSession::SendHeartbeatInfo(FrameProTLS* p_framepro_tls)
	{
		int64 now;
		FRAMEPRO_GET_CLOCK_COUNT(now);

		if(now - m_LastSessionInfoSendTime > m_ClockFrequency && g_Connected)
		{
			m_LastSessionInfoSendTime = now;

			// notify FramePro of the main thread
			int thread_id = p_framepro_tls->GetThreadId();
			if(m_MainThreadId != thread_id)
			{
				p_framepro_tls->SetMainThread(thread_id);
				m_MainThreadId = thread_id;
			}

			// send session info
			SessionInfoPacket session_info_packet;

			{
				CriticalSectionScope tls_lock(m_TLSListCriticalSection);
				for(FrameProTLS* p_framepro_tls_iter = m_FrameProTLSList.GetHead(); p_framepro_tls_iter!=NULL; p_framepro_tls_iter=p_framepro_tls_iter->GetNext())
				{
					session_info_packet.m_SendBufferSize += p_framepro_tls_iter->GetSendBufferMemorySize();
					session_info_packet.m_StringMemorySize += p_framepro_tls_iter->GetStringMemorySize();
					session_info_packet.m_MiscMemorySize += sizeof(FrameProTLS);
				}
			}

			session_info_packet.m_RecordingFileSize = m_NonInteractiveRecordingFileSize;

			SendImmediate(&session_info_packet, sizeof(session_info_packet), p_framepro_tls);
		}
	}

	//------------------------------------------------------------------------
	// when interactive mode is disabled sends over the socket directly, otherwise send as normal
	void FrameProSession::SendImmediate(void* p_data, int size, FrameProTLS* p_framepro_tls)
	{
		if(mp_RecordingFile)
		{
			p_framepro_tls->Send(p_data, size);
		}
		else
		{
#if FRAMEPRO_SOCKETS_ENABLED
			if(m_Interactive)
				p_framepro_tls->Send(p_data, size);
			else
				m_ClientSocket.Send(p_data, size);
#endif
		}
	}

	//------------------------------------------------------------------------
	void FrameProSession::SendSessionDetails(const char* p_name, const char* p_build_id)
	{
		StringId name = RegisterString(p_name);
		StringId build_id = RegisterString(p_build_id);

		SendSessionDetails(name, build_id);
	}

	//------------------------------------------------------------------------
	void FrameProSession::SendSessionDetails(const wchar_t* p_name, const wchar_t* p_build_id)
	{
		StringId name = RegisterString(p_name);
		StringId build_id = RegisterString(p_build_id);

		SendSessionDetails(name, build_id);
	}

	//------------------------------------------------------------------------
	void FrameProSession::SendSessionDetails(StringId name, StringId build_id)
	{
		// this needs to be outside the critical section lock because it might lock the critical section itself
		FrameProTLS* p_framepro_tls = GetFrameProTLS();

		CriticalSectionScope lock(m_CriticalSection);

		// initialise FramePro ONCE
		if(!m_Initialised)
		{
			Initialise(p_framepro_tls);
			m_Initialised = true;
		}

		char date_str[64];
		GetDateString(date_str, sizeof(date_str));
		StringId date = RegisterString(date_str);

		p_framepro_tls->SendSessionInfoPacket(SessionDetailsPacket(name, build_id, date));
	}

	//------------------------------------------------------------------------
	void FrameProSession::BlockSockets()
	{
		CriticalSectionScope lock(m_CriticalSection);

		if (!m_SocketsBlocked)
		{
#if FRAMEPRO_SOCKETS_ENABLED
			m_ListenSocket.Disconnect();
#endif
			m_SocketsBlocked = true;
		}
	}

	//------------------------------------------------------------------------
	void FrameProSession::UnblockSockets()
	{
		CriticalSectionScope lock(m_CriticalSection);

		if(m_SocketsBlocked)
		{
			m_SocketsBlocked = false;

			if(m_Initialised)
			{
				#if FRAMEPRO_SOCKETS_ENABLED
					OpenListenSocket();
					StartConnectThread();
				#endif
			}
		}
	}

	//------------------------------------------------------------------------
	void FrameProSession::Shutdown()
	{
		m_TLSListCriticalSection.Enter();

		while (!m_FrameProTLSList.IsEmpty())
		{
			FrameProTLS* p_framepro_tls = m_FrameProTLSList.GetHead();

			m_TLSListCriticalSection.Leave();
			DestroyFrameProTLS(p_framepro_tls);
			m_TLSListCriticalSection.Enter();
		}

		m_TLSListCriticalSection.Leave();
	}

	//------------------------------------------------------------------------
	int64 FrameProSession::GetClockFrequency()
	{
		return m_ClockFrequency;
	}

	//------------------------------------------------------------------------
	void FrameProSession::FrameStart()
	{
		FRAMEPRO_NAMED_SCOPE("FramePro Start Frame");

		// this needs to be outside the critical section lock because it might lock the critical section itself
		FrameProTLS* p_framepro_tls = GetFrameProTLS();

		CriticalSectionScope lock(m_CriticalSection);

		// initialise FramePro ONCE
		if(!m_Initialised)
		{
			Initialise(p_framepro_tls);
			m_Initialised = true;
		}

		// initialise the connection
		if(m_InitialiseConnectionNextFrame)
		{
			InitialiseConnection(p_framepro_tls);
			m_InitialiseConnectionNextFrame = false;
		}

		// send any outstanding string literals
		{
			CriticalSectionScope tls_lock(m_StringRequestPacketsLock);
			int count = m_StringRequestPackets.GetCount();
			if(count)
			{
				for(int i=0; i<count; ++i)
				{
					const RequestStringLiteralPacket& packet = m_StringRequestPackets[i];
					p_framepro_tls->SendStringLiteral((StringLiteralType::Enum)packet.m_StringLiteralType, packet.m_StringId);
				}
				m_StringRequestPackets.Clear();
			}
		}

		// send the m_MainThreadSendBuffer
		{
			CriticalSectionScope tls_lock(m_MainThreadSendBufferLock);
			if(m_MainThreadSendBuffer.GetSize())
			{
				p_framepro_tls->Send(m_MainThreadSendBuffer.GetBuffer(), m_MainThreadSendBuffer.GetSize());
				m_MainThreadSendBuffer.Clear();
			}
		}

		if(g_Connected)
		{
			// flush context switches
#if FRAMEPRO_EVENT_TRACE_WIN32
			if(mp_EventTraceWin32)
				mp_EventTraceWin32->Flush();
#endif
			int64 wait_start_time;
			FRAMEPRO_GET_CLOCK_COUNT(wait_start_time);

			FlushGlobalHiResTimers(p_framepro_tls);

			{
				FRAMEPRO_NAMED_SCOPE("FramePro Wait For Send");

				if(GetMemoryUsage() > FRAMEPRO_MAX_MEMORY)
				{
					// wait until the send from the previous frame has finished
					m_CriticalSection.Leave();

					m_SendReady.Set();

					m_SendComplete.Wait();

					m_CriticalSection.Enter();
				}
			}

			int64 wait_end_time;
			FRAMEPRO_GET_CLOCK_COUNT(wait_end_time);
			int64 wait_for_send_complete_time = wait_end_time - wait_start_time;

			m_SendComplete.Reset();

			// tell the TLS objects that the frame has started
			{
				CriticalSectionScope tls_lock(m_TLSListCriticalSection);
				for(FrameProTLS* p_iter=m_FrameProTLSList.GetHead(); p_iter!=NULL; p_iter=p_iter->GetNext())
					p_iter->OnFrameStart();
			}

			// send the frame data
			SendHeartbeatInfo(p_framepro_tls);

			p_framepro_tls->SendFrameStartPacket(wait_for_send_complete_time);
		}

		// stop recording if the file has become too big
		//if (mp_RecordingFile && m_RecordingFileSize > m_MaxRecordingFileSize)
		if (false)
		{
			StopRecording();
		}
	}

	//------------------------------------------------------------------------
	size_t FrameProSession::GetMemoryUsage() const
	{
		size_t memory = 0;

		CriticalSectionScope tls_lock(m_TLSListCriticalSection);
		for(const FrameProTLS* p_framepro_tls_iter = m_FrameProTLSList.GetHead(); p_framepro_tls_iter!=NULL; p_framepro_tls_iter=p_framepro_tls_iter->GetNext())
		{
			memory += p_framepro_tls_iter->GetSendBufferMemorySize();
			memory += p_framepro_tls_iter->GetStringMemorySize();
			memory += sizeof(FrameProTLS);
		}

		return memory;
	}

	//------------------------------------------------------------------------
	void FrameProSession::AddFrameProTLS(FrameProTLS* p_framepro_tls)
	{
		CriticalSectionScope lock(m_CriticalSection);

		{
			CriticalSectionScope tls_lock(m_TLSListCriticalSection);
			m_FrameProTLSList.AddTail(p_framepro_tls);
		}

		if(g_Connected)
			p_framepro_tls->OnConnected(mp_RecordingFile != NULL);
	}

	//------------------------------------------------------------------------
	void FrameProSession::RemoveFrameProTLS(FrameProTLS* p_framepro_tls)
	{
		CriticalSectionScope lock(m_CriticalSection);

		{
			CriticalSectionScope tls_lock(m_TLSListCriticalSection);
			m_FrameProTLSList.Remove(p_framepro_tls);
		}
	}

	//------------------------------------------------------------------------
	void FrameProSession::SetThreadName(const char* p_name)
	{
		FrameProTLS* p_framepro_tls = GetFrameProTLS();

		CriticalSectionScope lock(m_CriticalSection);

		m_NamedThreads.SetAllocator(GetAllocator());

		int thread_id = p_framepro_tls->GetThreadId();

		if(!m_NamedThreads.Contains(thread_id))
			m_NamedThreads.Add(thread_id);

		p_framepro_tls->SetThreadName(thread_id, p_name);
	}

	//------------------------------------------------------------------------
	bool FrameProSession::HasSetThreadName(int thread_id) const
	{
		return m_NamedThreads.Contains(thread_id);
	}

	//------------------------------------------------------------------------
	int FrameProSession::GetConnectionChangedCallbackIndex(ConnectionChangedCallback p_callback)
	{
		for(int i=0; i<m_Connectionchangedcallbacks.GetCount(); ++i)
		{
			if(m_Connectionchangedcallbacks[i].mp_Callback == p_callback)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------
	void FrameProSession::RegisterConnectionChangedCallback(ConnectionChangedCallback p_callback, void* p_context)
	{
		CriticalSectionScope lock(m_ConnectionChangedCriticalSection);

		// call immediately if already connected
		if(g_Connected)
			p_callback(true, p_context);

		if(GetConnectionChangedCallbackIndex(p_callback) == -1)
		{
			ConnectionChangedcallbackInfo data;
			data.mp_Callback = p_callback;
			data.mp_Context = p_context;

			m_Connectionchangedcallbacks.SetAllocator(GetAllocator());

			m_Connectionchangedcallbacks.Add(data);
		}
	}

	//------------------------------------------------------------------------
	void FrameProSession::UnregisterConnectionChangedCallback(ConnectionChangedCallback p_callback)
	{
		CriticalSectionScope lock(m_ConnectionChangedCriticalSection);

		int index = GetConnectionChangedCallbackIndex(p_callback);
		if(index != -1)
			m_Connectionchangedcallbacks.RemoveAt(index);
	}

	//------------------------------------------------------------------------
	void FrameProSession::OnConnectionChanged(bool connected) const
	{
		CriticalSectionScope lock(m_ConnectionChangedCriticalSection);

		for(int i=0; i<m_Connectionchangedcallbacks.GetCount(); ++i)
		{
			const ConnectionChangedcallbackInfo& data = m_Connectionchangedcallbacks[i];
			data.mp_Callback(connected, data.mp_Context);
		}
	}

	//------------------------------------------------------------------------
// START EPIC
	void FrameProSession::StartRecording(const FString& p_filename, bool context_switches, int64 max_file_size)
	{
		CriticalSectionScope lock(m_CriticalSection);

		if(mp_RecordingFile)
			StopRecording();

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		mp_RecordingFile = PlatformFile.OpenWrite(*p_filename);

		if(mp_RecordingFile)
		{
			const char* p_id = "framepro_recording";
			mp_RecordingFile->Write((uint8*)p_id, strlen(p_id));

			#if FRAMEPRO_SOCKETS_ENABLED
				m_ListenSocket.Disconnect();		// don't allow connections while recording
			#endif
			
			m_StartContextSwitchRecording = context_switches;
			
			m_InitialiseConnectionNextFrame = true;

			m_RecordingFileSize = 0;
			m_MaxRecordingFileSize = max_file_size;
		}
	}
// END EPIC

	//------------------------------------------------------------------------
	void FrameProSession::StopRecording()
	{
		CriticalSectionScope lock(m_CriticalSection);

		if(mp_RecordingFile)
		{
			#if FRAMEPRO_SOCKETS_ENABLED
				OpenListenSocket();					// start the listening socket again so that we can accept new connections
			#endif
			HandleDisconnect_NoLock();
		}
	}

	//------------------------------------------------------------------------
	void FrameProSession::SetThreadPriority(int priority)
	{
		m_ThreadPriority = priority;
		m_ThreadPrioritySet = true;

		if(m_SendThread.IsAlive())
			m_SendThread.SetPriority(priority);

		if(m_ReceiveThread.IsAlive())
			m_ReceiveThread.SetPriority(priority);
	}

	//------------------------------------------------------------------------
	void FrameProSession::SetThreadAffinity(int affinity)
	{
		m_ThreadAffinity = affinity;
		m_ThreadAffinitySet = true;

		if(m_SendThread.IsAlive())
			m_SendThread.SetAffinity(affinity);

		if(m_ReceiveThread.IsAlive())
			m_ReceiveThread.SetAffinity(affinity);
	}

	//------------------------------------------------------------------------
	void FrameProSession::AddGlobalHiResTimer(GlobalHiResTimer* p_timer)
	{
		CriticalSectionScope lock(m_CriticalSection);

		p_timer->SetNext(mp_GlobalHiResTimers);
		mp_GlobalHiResTimers = p_timer;
	}

	//------------------------------------------------------------------------
	void FrameProSession::FlushGlobalHiResTimers(FrameProTLS* p_framepro_tls)
	{
		for(GlobalHiResTimer* p_timer = mp_GlobalHiResTimers; p_timer != NULL; p_timer = p_timer->GetNext())
		{
			uint64 value;
			uint count;
			p_timer->GetAndClear(value, count);

			const char* p_unit = "cycles";

			// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
			if (p_framepro_tls->SendStringsImmediately())
			{
				p_framepro_tls->SendString(p_timer->GetName(), PacketType::StringPacket);
				p_framepro_tls->SendString(p_timer->GetGraph(), PacketType::StringPacket);
				p_framepro_tls->SendString(p_unit, PacketType::StringPacket);
			}

			CriticalSectionScope lock(p_framepro_tls->GetCurrentSendBufferCriticalSection());

			CustomStatPacketInt64* p_packet = p_framepro_tls->AllocateSpaceInBuffer<CustomStatPacketInt64>();

			CustomStatValueType::Enum value_type = CustomStatValueType::Int64;

			p_packet->m_PacketTypeAndValueType = PacketType::CustomStatPacket | (value_type << 16);
			p_packet->m_Count = count;
			p_packet->m_Name = (StringId)p_timer->GetName();
			p_packet->m_Value = value;
			p_packet->m_Graph = (StringId)p_timer->GetGraph();
			p_packet->m_Unit = (StringId)p_unit;
		}
	}

	//------------------------------------------------------------------------
	void FrameProSession::ClearGlobalHiResTimers()
	{
		for (GlobalHiResTimer* p_timer = mp_GlobalHiResTimers; p_timer != NULL; p_timer = p_timer->GetNext())
		{
			uint64 value;
			uint count;
			p_timer->GetAndClear(value, count);
		}
	}

	//------------------------------------------------------------------------
	bool FrameProSession::CallConditionalParentScopeCallback(ConditionalParentScopeCallback p_callback, const char* p_name, int64 start_time, int64 end_time)
	{
		return p_callback(p_name, start_time, end_time, m_ClockFrequency);
	}

	//------------------------------------------------------------------------
	void FrameProSession::SetConditionalScopeMinTimeInMicroseconds(int64 value)
	{
		g_ConditionalScopeMinTime = (int)((value * m_ClockFrequency) / 1000000LL);
	}
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED
//------------------------------------------------------------------------
// FrameProStackTrace.cpp



//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLE_CALLSTACKS

//------------------------------------------------------------------------
using namespace FramePro;

//------------------------------------------------------------------------
// if both of these options are commented out it will use CaptureStackBackTrace (or backtrace on linux)
#define FRAMEPRO_USE_STACKWALK64 0				// much slower but possibly more reliable. FRAMEPRO_USE_STACKWALK64 only implemented for x86 builds.
#define FRAMEPRO_USE_RTLVIRTUALUNWIND 0			// reported to be faster than StackWalk64 - only available on x64 builds
#define FRAMEPRO_USE_RTLCAPTURESTACKBACKTRACE 0	// system version of FRAMEPRO_USE_RTLVIRTUALUNWIND - only available on x64 builds

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	bool GetStackTrace(void** stack, int& stack_size, unsigned int& hash)
	{
		#if FRAMEPRO_PLATFORM_WIN
			#if FRAMEPRO_USE_STACKWALK64

				// get the context
				CONTEXT context;
				memset(&context, 0, sizeof(context));
				RtlCaptureContext(&context);

				// setup the stack frame
				STACKFRAME64 stack_frame;
				memset(&stack_frame, 0, sizeof(stack_frame));
				stack_frame.AddrPC.Mode = AddrModeFlat;
				stack_frame.AddrFrame.Mode = AddrModeFlat;
				stack_frame.AddrStack.Mode = AddrModeFlat;
				DWORD machine = IMAGE_FILE_MACHINE_IA64;
				stack_frame.AddrPC.Offset = context.Rip;
				stack_frame.AddrFrame.Offset = context.Rsp;
				stack_frame.AddrStack.Offset = context.Rbp;
				HANDLE thread = GetCurrentThread();

				static HANDLE process = GetCurrentProcess();

				stack_size = 0;
				while (StackWalk64(
					machine,
					process,
					thread,
					&stack_frame,
					&context,
					NULL,
					SymFunctionTableAccess64,
					SymGetModuleBase64,
					NULL) && stack_size < FRAMEPRO_STACK_TRACE_SIZE)
				{
					void* p = (void*)(stack_frame.AddrPC.Offset);
					stack[stack_size++] = p;
				}
				hash = GetHash(stack, stack_size);
			#elif FRAMEPRO_USE_RTLVIRTUALUNWIND
				MemPro::VirtualUnwindStackWalk(stack, FRAMEPRO_STACK_TRACE_SIZE);
				hash = GetHashAndStackSize(stack, stack_size);
			#elif FRAMEPRO_USE_RTLCAPTURESTACKBACKTRACE
				MemPro::RTLCaptureStackBackTrace(stack, FRAMEPRO_STACK_TRACE_SIZE, hash, stack_size);
			#else
				CaptureStackBackTrace(0, FRAMEPRO_STACK_TRACE_SIZE, stack, (PDWORD)&hash);
				for (stack_size = 0; stack_size<FRAMEPRO_STACK_TRACE_SIZE; ++stack_size)
					if (!stack[stack_size])
						break;
			#endif
			return true;

		#elif FRAMEPRO_PLATFORM_XBOX360
			DmCaptureStackBackTrace(FRAMEPRO_STACK_TRACE_SIZE, stack);
			hash = GetHashAndStackSize(stack, stack_size);
			return true;

		#elif FRAMEPRO_PLATFORM_XBOXONE
			#if FRAMEPRO_USE_RTLCAPTURESTACKBACKTRACE
					MemPro::RTLCapruteStackBackTrace(stack, FRAMEPRO_STACK_TRACE_SIZE, hash, stack_size);
			#elif FRAMEPRO_USE_RTLVIRTUALUNWIND
					MemPro::VirtualUnwindStackWalk(stack, FRAMEPRO_STACK_TRACE_SIZE);
					hash = GetHashAndStackSize(stack, stack_size);
			#else
				#error
			#endif
			return true;

		#elif MEMPRO_PLATFORM_PS4
			SceDbgCallFrame frames[FRAMEPRO_STACK_TRACE_SIZE];
			unsigned int frame_count = 0;
			sceDbgBacktraceSelf(frames, sizeof(frames), &frame_count, SCE_DBG_BACKTRACE_MODE_DONT_EXCEED);

			stack_size = frame_count;
			for (unsigned int i = 0; i < frame_count; ++i)
				stack[i] = (void*)frames[i].pc;
			return true;

		#elif MEMPRO_UNIX_BASED_PLATFORM
			stack_size = backtrace(stack, FRAMEPRO_STACK_TRACE_SIZE);
			hash = GetHashAndStackSize(stack, stack_size);
			return true;

		#else
			return false;
		#endif
	}
}

//------------------------------------------------------------------------
StackTrace::StackTrace(Allocator* p_allocator)
:	m_StackCount(0),
	m_StackHash(0),
	m_CallstackSet(p_allocator)
{
	memset(m_Stack, 0, sizeof(m_Stack));
}

//------------------------------------------------------------------------
void StackTrace::Clear()
{
	m_CallstackSet.Clear();
}

//------------------------------------------------------------------------
CallstackResult StackTrace::Capture()
{
	CallstackResult result;

	result.m_IsNew = false;

	memset(m_Stack, 0, sizeof(m_Stack));

	if (!GetStackTrace(m_Stack, m_StackCount, m_StackHash))
	{
		result.mp_Callstack = NULL;
		return result;
	}

	result.mp_Callstack = m_CallstackSet.Get((uint64*)m_Stack, m_StackCount, m_StackHash);

	if (!result.mp_Callstack)
	{
		result.mp_Callstack = m_CallstackSet.Add((uint64*)m_Stack, m_StackCount, m_StackHash);
		result.m_IsNew = true;
	}

	return result;
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLE_CALLSTACKS

//------------------------------------------------------------------------
#endif		// #ifdef ENABLE_MEMPRO
//------------------------------------------------------------------------
// FrameProTLS.cpp



//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
// START EPIC
	#if !FRAMEPRO_WIN_BASED_PLATFORM && !FRAMEPRO_UE4_BASED_PLATFORM
// END EPIC
		int FrameProTLS::m_NewThreadId = 1;
	#endif

	//------------------------------------------------------------------------
	static const int g_FrameProTLSBufferMarker = 0xfbfbfbfb;

	std::atomic<long> FrameProTLS::m_StringCount(0);

	//------------------------------------------------------------------------
	FrameProTLS::FrameProTLS(Allocator* p_allocator, int64 clock_frequency)
	:	m_Interactive(true),
		m_RecordingToFile(false),
		m_SendStringsImmediately(false),
		mp_CurrentSendBuffer(NULL),
		m_CurrentSendBufferSize(0),
		m_ThreadId(0),
		m_OSThreadId(GetCurrentThreadId()),
		m_HiResTimerScopeStartTime(0),
		m_HiResTimerStartTime(0),
		m_ActiveHiResTimerIndex(-1),
		mp_Next(NULL),
		mp_Allocator(p_allocator),
		m_LiteralStringSet(p_allocator),
		m_LiteralStringSetMemorySize(0),
		m_StringHashMap(p_allocator),
		m_WStringHashMap(p_allocator),
		m_SessionInfoBuffer(p_allocator),
		m_SessionInfoBufferMemorySize(0),
		m_Connected(false),
		m_StringAllocator(p_allocator),
		m_SendBufferMemorySize(0),
		m_StringMemorySize(0),
		m_ClockFrequency(clock_frequency),
		m_ShuttingDown(false),
		mp_CurrentConditionalParentScope(NULL)
#if FRAMEPRO_ENABLE_CALLSTACKS
		,m_StackTrace(p_allocator)
		,m_SendCallstacks(false)
#endif
	{
// START EPIC
		#if FRAMEPRO_WIN_BASED_PLATFORM || FRAMEPRO_UE4_BASED_PLATFORM
// END EPIC
			m_ThreadId = (int)m_OSThreadId;
		#else
			m_ThreadId = m_NewThreadId++;
		#endif

		UpdateSendStringsImmediatelyFlag();

		memset(m_FalseSharingSpacerBuffer, g_FrameProTLSBufferMarker, sizeof(m_FalseSharingSpacerBuffer));

		m_HiResTimers.SetAllocator(p_allocator);
		m_PausedHiResTimerStack.SetAllocator(p_allocator);

#ifdef FRAMEPRO_SCOPE_MIN_TIME
		m_ScopeMinTime = FramePro_Max(1LL, (FRAMEPRO_SCOPE_MIN_TIME * m_ClockFrequency) / 1000000000LL);
#endif

#ifdef FRAMEPRO_WAIT_EVENT_MIN_TIME
		m_WaitEventMinTime = FramePro_Max(1LL, (FRAMEPRO_WAIT_EVENT_MIN_TIME * m_ClockFrequency) / 1000000000LL);
#endif
	}

	//------------------------------------------------------------------------
	FrameProTLS::~FrameProTLS()
	{
		{
			CriticalSectionScope lock(m_CriticalSection);
			Clear();
		}

		FreeCurrentSendBuffer();
	}

	//------------------------------------------------------------------------
	// this is called from the main thread
	void FrameProTLS::OnDisconnected()
	{
		CriticalSectionScope lock(m_CriticalSection);

		m_Connected = false;
		SetInteractive(true);		// we are interactive until told otherwise

		Clear();
	}

	//------------------------------------------------------------------------
	void FrameProTLS::Clear()
	{
		FRAMEPRO_ASSERT(m_CriticalSection.Locked());

		DeleteListItems(m_SendBufferList);
		DeleteListItems(m_SendBufferFreeList);

		m_SendBufferMemorySize = 0;

		{
			CriticalSectionScope frame_swap_lock(m_CurrentSendBufferCriticalSection);
			m_CurrentSendBufferSize = 0;
		}

		{
			CriticalSectionScope cond_lock(m_ConditionalParentScopeListCritSec);
			ConditionalParentScope* p_scope = m_ConditionalParentScopeList.GetHead();
			while (p_scope)
			{
				ConditionalParentScope* p_next = p_scope->GetNext();
				DeleteListItems(p_scope->m_ChildSendBuffers);
				Delete(mp_Allocator, p_scope);
				p_scope = p_next;
			}
			m_ConditionalParentScopeList.Clear();
		}

		UpdateStringMemorySize();

#if FRAMEPRO_ENABLE_CALLSTACKS
		m_StackTrace.Clear();
#endif
		// we can't delete the hires timer stuff here because we don't want to introduce a lock
	}

	//------------------------------------------------------------------------
	void FrameProTLS::UpdateStringMemorySize() 
	{
		m_StringMemorySize =
			m_StringAllocator.GetMemorySize() +
			m_StringHashMap.GetMemorySize() +
			m_WStringHashMap.GetMemorySize();
	}

	//------------------------------------------------------------------------
	// this is called from the main thread
	void FrameProTLS::OnConnected(bool recording_to_file)
	{
		CriticalSectionScope lock(m_CriticalSection);
		
		if(!m_Connected)
		{
			Clear();

			m_Connected = true;
			
			m_RecordingToFile = recording_to_file;
			UpdateSendStringsImmediatelyFlag();

			{
				CriticalSectionScope lock2(m_CurrentSendBufferCriticalSection);
				AllocateCurrentSendBuffer();
			}
		}
	}

	//------------------------------------------------------------------------
	void FrameProTLS::AllocateCurrentSendBuffer()
	{
		FRAMEPRO_ASSERT(m_CriticalSection.Locked());
		FRAMEPRO_ASSERT(m_CurrentSendBufferCriticalSection.Locked());
		FRAMEPRO_ASSERT(IsOnTLSThread() || !g_Connected);		// can only be accessed from TLS thread, unless we haven't connected yet

		if(!mp_CurrentSendBuffer)
		{
			mp_CurrentSendBuffer = mp_Allocator->Alloc(m_SendBufferCapacity);
			FRAMEPRO_ASSERT(mp_CurrentSendBuffer);

			m_SendBufferMemorySize = m_SendBufferMemorySize + m_SendBufferCapacity;
		}
	}

	//------------------------------------------------------------------------
	void FrameProTLS::FreeCurrentSendBuffer()
	{
		CriticalSectionScope lock(m_CurrentSendBufferCriticalSection);

		if(mp_CurrentSendBuffer)
		{
			mp_Allocator->Free(mp_CurrentSendBuffer);
			mp_CurrentSendBuffer = NULL;
			m_CurrentSendBufferSize = 0;
		}
	}

	//------------------------------------------------------------------------
	// send the session info buffer (stuff we cache between connects)
	void FrameProTLS::SendSessionInfoBuffer()
	{
		// m_SessionInfoBufferLock should have been locked before calling this function
		Send(m_SessionInfoBuffer.GetBuffer(), m_SessionInfoBuffer.GetSize());
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SendConnectPacket(int64 clock_frequency, int process_id, Platform::Enum platform)
	{
		SendPacket(ConnectPacket(clock_frequency, process_id, platform));
		FlushCurrentSendBuffer();
	}

	//------------------------------------------------------------------------
	void FrameProTLS::Send(const void* p_data, int size)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread() || !g_Connected);		// can only be accessed from TLS thread, unless we haven't connected yet

		// make common case fast
		if(size <= m_SendBufferCapacity)
		{
			CriticalSectionScope lock(m_CurrentSendBufferCriticalSection);

			void* p_dest = AllocateSpaceInBuffer(size);
			memcpy(p_dest, p_data, size);
		}
		else
		{
			List<SendBuffer> send_buffer_list;

			{
				CriticalSectionScope lock(m_CurrentSendBufferCriticalSection);

				int available_space = m_SendBufferCapacity - m_CurrentSendBufferSize;

				if (!available_space)
				{
					FlushCurrentSendBuffer_no_lock();
					available_space = m_SendBufferCapacity;
				}

				int bytes_to_send = size;
				const void* p_src = p_data;
				while (bytes_to_send)
				{
					// copy to the current send buffer
					int send_size = FramePro_Min(bytes_to_send, available_space);
					void* p_dest = (char*)mp_CurrentSendBuffer + m_CurrentSendBufferSize;
					memcpy(p_dest, p_src, send_size);
					m_CurrentSendBufferSize += send_size;
					bytes_to_send -= send_size;
					p_src = (char*)p_src + send_size;

					// copy current send buffer to a new send buffer object
					SendBuffer* p_send_buffer = AllocateSendBuffer();
					p_send_buffer->Swap(mp_CurrentSendBuffer, m_CurrentSendBufferSize, m_SendBufferCapacity);
					FRAMEPRO_ASSERT(mp_CurrentSendBuffer);
					available_space = m_SendBufferCapacity;

					send_buffer_list.AddTail(p_send_buffer);
				}
			}

			{
				CriticalSectionScope lock(m_CriticalSection);
				m_SendBufferList.MoveAppend(send_buffer_list);
			}
		}
	}

	//------------------------------------------------------------------------
	void FrameProTLS::OnFrameStart()
	{
		UpdateStringMemorySize();

		m_SessionInfoBufferMemorySize = m_SessionInfoBuffer.GetMemorySize();

		FlushCurrentSendBuffer();

		FlushConditionalChildSendBuffers();
	}

	//------------------------------------------------------------------------
	void FrameProTLS::AddEmptySendBuffer(SendBuffer* p_send_buffer)
	{
		CriticalSectionScope lock(m_CriticalSection);

		FRAMEPRO_ASSERT(p_send_buffer->GetOwner() == this);

		// only keep the buffer for the first free send buffer, otherwise clear it
		if(m_SendBufferFreeList.IsEmpty())
		{
			m_SendBufferFreeList.AddHead(p_send_buffer);
		}
		else
		{
			FRAMEPRO_ASSERT(m_SendBufferMemorySize >= (size_t)p_send_buffer->GetCapacity());
			m_SendBufferMemorySize = m_SendBufferMemorySize - p_send_buffer->GetCapacity();		// doesn't have to be atomic because it's only for stats

			p_send_buffer->ClearBuffer();

			m_SendBufferFreeList.AddTail(p_send_buffer);
		}
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SendFrameStartPacket(int64 wait_for_send_complete_time)
	{
		// start the new frame
		int64 frame_start_time;
		FRAMEPRO_GET_CLOCK_COUNT(frame_start_time);
		SendPacket(FrameStartPacket(frame_start_time, wait_for_send_complete_time));
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SetThreadName(int thread_id, const char* p_name)
	{
		StringId name_id = RegisterString(p_name);

		SendSessionInfoPacket(ThreadNamePacket(thread_id, name_id));
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SetThreadOrder(StringId thread_name)
	{
		SendSessionInfoPacket(ThreadOrderPacket(thread_name));
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SetMainThread(int main_thraed_id)
	{
		SendSessionInfoPacket(MainThreadPacket(main_thraed_id));
	}

	//------------------------------------------------------------------------
	StringId FrameProTLS::RegisterString(const char* p_str)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());

		String str(p_str);

		StringId string_id = 0;
		if(!m_StringHashMap.TryGetValue(str, string_id))
		{
			string_id = ++m_StringCount;
			str.TakeCopy(m_StringAllocator);
			m_StringHashMap.Add(str, string_id);
		
			SendString(string_id, p_str, PacketType::StringPacket);

			UpdateStringMemorySize();
		}

		return string_id;
	}

	//------------------------------------------------------------------------
	StringId FrameProTLS::RegisterString(const wchar_t* p_str)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());

		WString str(p_str);

		StringId string_id = 0;
		if(!m_WStringHashMap.TryGetValue(str, string_id))
		{
			string_id = ++m_StringCount;
			str.TakeCopy(m_StringAllocator);
			m_WStringHashMap.Add(str, string_id);
		
			SendString(string_id, p_str, PacketType::WStringPacket);

			UpdateStringMemorySize();
		}

		return string_id;
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SendSessionInfo(const void* p_data, int size)
	{
		{
			// copy it to the session buffer
			CriticalSectionScope lock(m_SessionInfoBufferLock);
			void* p_dest = m_SessionInfoBuffer.Allocate(size);
			memcpy(p_dest, p_data, size);
		}

		if(m_Connected)
			Send(p_data, size);
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SendString(StringId string_id, const char* p_str, PacketType::Enum packet_type)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());

		int string_len = (int)strlen(p_str);
		FRAMEPRO_ASSERT(string_len <= INT_MAX);

		int aligned_string_len = AlignUpPow2(string_len, 4);
		int size_to_allocate = sizeof(StringPacket) + aligned_string_len;
 
		StringPacket* p_packet = NULL;
		{
			CriticalSectionScope lock(m_SessionInfoBufferLock);

			p_packet = (StringPacket*)(m_SessionInfoBuffer.Allocate(size_to_allocate));
			if(!p_packet)
			{
				ShowMemoryWarning();
				return;
			}

			p_packet->m_PacketType = packet_type;
			p_packet->m_Length = (int)string_len;
			p_packet->m_StringId = string_id;
			memcpy(p_packet + 1, p_str, string_len);
		}

		if(m_Connected)
			Send(p_packet, size_to_allocate);
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SendString(StringId string_id, const wchar_t* p_str, PacketType::Enum packet_type)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());

		int string_len = (int)wcslen(p_str);
		FRAMEPRO_ASSERT(string_len <= INT_MAX);

#if FRAMEPRO_WIN_BASED_PLATFORM
		int string_size = string_len * sizeof(wchar_t);
		int aligned_string_size = AlignUpPow2(string_size, 4);
		int size_to_allocate = sizeof(StringPacket) + aligned_string_size;

		StringPacket* p_packet = NULL;
		{
			CriticalSectionScope lock(m_SessionInfoBufferLock);

			p_packet = (StringPacket*)(m_SessionInfoBuffer.Allocate(size_to_allocate));
			if(!p_packet)
			{
				ShowMemoryWarning();
				return;
			}

			p_packet->m_PacketType = packet_type;
			p_packet->m_Length = (int)string_len;
			p_packet->m_StringId = string_id;
			memcpy(p_packet + 1, p_str, string_size);
		}
#else
		static_assert(sizeof(wchar_t) == 4, "Expected wchar size to be 4 on this platform");
		int string_size = string_len * 2;
		int aligned_string_size = AlignUpPow2(string_size, 4);
		int size_to_allocate = sizeof(StringPacket) + aligned_string_size;

		StringPacket* p_packet = NULL;
		{
			CriticalSectionScope lock(m_SessionInfoBufferLock);

			p_packet = (StringPacket*)(m_SessionInfoBuffer.Allocate(size_to_allocate));
			if (!p_packet)
			{
				ShowMemoryWarning();
				return;
			}

			p_packet->m_PacketType = packet_type;
			p_packet->m_Length = (int)string_len;
			p_packet->m_StringId = string_id;

			// convert UTF-32 to UTF-16 by truncating (take only first 2 bytes of the 4 bytes)
			FRAMEPRO_ASSERT(sizeof(wchar_t) == 4);
			char* p_dest = (char*)(p_packet + 1);
			char* p_source = (char*)p_str;
			for (int i = 0; i < string_len; ++i)
			{
				*p_dest++ = *p_source++;
				*p_dest++ = *p_source++;
				p_source += 2;
			}
		}
#endif
		if(m_Connected)
			Send(p_packet, size_to_allocate);
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SendLogPacket(const char* p_message)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());
		FRAMEPRO_ASSERT(m_Connected);

		int string_len = (int)strlen(p_message);
		FRAMEPRO_ASSERT(string_len <= INT_MAX);

		int aligned_string_len = AlignUpPow2(string_len, 4);
		int size_to_allocate = sizeof(LogPacket) + aligned_string_len;

		CriticalSectionScope lock(m_CurrentSendBufferCriticalSection);

		LogPacket* p_packet = (LogPacket*)AllocateSpaceInBuffer(size_to_allocate);

		int64 time;
		FRAMEPRO_GET_CLOCK_COUNT(time);

		p_packet->m_PacketType = PacketType::LogPacket;
		p_packet->m_Time = time;

		p_packet->m_Length = (int)string_len;
		memcpy(p_packet + 1, p_message, string_len);
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SendEventPacket(const char* p_name, uint colour)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());
		FRAMEPRO_ASSERT(m_Connected);

		int64 timestamp = 0;
		FRAMEPRO_GET_CLOCK_COUNT(timestamp);

		// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
		if (m_SendStringsImmediately)
			SendString(p_name, PacketType::StringPacket);

		EventPacket packet;

		packet.m_PacketType = PacketType::EventPacket;
		packet.m_Colour = colour;
		packet.m_Name = (StringId)p_name;
		packet.m_Time = timestamp;

		SendPacket(packet);
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SendString(const char* p_string, PacketType::Enum packet_type)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());

		if(AddStringLiteral(p_string))
			SendString((StringId)p_string, p_string, packet_type);
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SendString(const wchar_t* p_string, PacketType::Enum packet_type)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());

		if(AddStringLiteral(p_string))
			SendString((StringId)p_string, p_string, packet_type);
	}

	//------------------------------------------------------------------------
#if FRAMEPRO_SOCKETS_ENABLED
	void FrameProTLS::SendStringLiteral(StringLiteralType::Enum string_literal_type, StringId string_id)
	{
		switch(string_literal_type)
		{
			case StringLiteralType::NameAndSourceInfo:
				SendString(string_id, (const char*)string_id, PacketType::NameAndSourceInfoPacket);
				break;

			case StringLiteralType::NameAndSourceInfoW:
				SendString(string_id, (const wchar_t*)string_id, PacketType::NameAndSourceInfoPacketW);
				break;

			case StringLiteralType::SourceInfo:
				SendString(string_id, (const char*)string_id, PacketType::SourceInfoPacket);
				break;

			case StringLiteralType::GeneralString:
				SendString(string_id, (const char*)string_id, PacketType::StringPacket);
				break;

			case StringLiteralType::StringLiteralTimerName:
				SendString(string_id, (const char*)string_id, PacketType::StringLiteralTimerNamePacket);
				break;

			default:
				FRAMEPRO_BREAK();
				break;
		}
	}
#endif

	//------------------------------------------------------------------------
	void FrameProTLS::ShowMemoryWarning() const
	{
		static int64 last_warn_time = 0;
		int64 now;
		FRAMEPRO_GET_CLOCK_COUNT(now);

		if(now - last_warn_time >= m_ClockFrequency)
		{
			OutputDebugString(FRAMEPRO_STRING("Warning: FramePro failed to allocate enough memory."));
			last_warn_time = now;
		}
	}

	//------------------------------------------------------------------------
	void FrameProTLS::FlushCurrentSendBuffer()
	{
		SendBuffer* p_send_buffer = AllocateSendBuffer();

		{
			CriticalSectionScope lock(m_CurrentSendBufferCriticalSection);
			p_send_buffer->Swap(mp_CurrentSendBuffer, m_CurrentSendBufferSize, m_SendBufferCapacity);

			FRAMEPRO_ASSERT(mp_CurrentSendBuffer);
			FRAMEPRO_ASSERT(!m_CurrentSendBufferSize);
		}

		if (mp_CurrentConditionalParentScope)
		{
			CriticalSectionScope lock(m_ConditionalParentScopeListCritSec);
			mp_CurrentConditionalParentScope->m_ChildSendBuffers.AddTail(p_send_buffer);
		}
		else
		{
			CriticalSectionScope lock(m_CriticalSection);
			m_SendBufferList.AddTail(p_send_buffer);
		}
	}

	//------------------------------------------------------------------------
	void FrameProTLS::FlushCurrentSendBuffer_no_lock()
	{
		FRAMEPRO_ASSERT(m_CurrentSendBufferCriticalSection.Locked());
		FRAMEPRO_ASSERT(IsOnTLSThread() || !g_Connected);		// can only be accessed from TLS thread, unless we haven't connected yet

		SendBuffer* p_send_buffer = AllocateSendBuffer();

		p_send_buffer->Swap(mp_CurrentSendBuffer, m_CurrentSendBufferSize, m_SendBufferCapacity);

		FRAMEPRO_ASSERT(mp_CurrentSendBuffer);
		FRAMEPRO_ASSERT(!m_CurrentSendBufferSize);

		if (mp_CurrentConditionalParentScope)
		{
			SendBuffer* p_new_parent_send_buffer = AllocateSendBuffer();

			{
				// move the current child send buffer to the parents child buffer list
				CriticalSectionScope lock(m_ConditionalParentScopeListCritSec);
				mp_CurrentConditionalParentScope->m_ChildSendBuffers.AddTail(p_send_buffer);

				p_new_parent_send_buffer->Swap(mp_CurrentConditionalParentScope->mp_SendBuffer);
			}

			{
				// move the parent send buffer to the main send buffer list
				CriticalSectionScope lock(m_CriticalSection);
				m_SendBufferList.AddTail(p_new_parent_send_buffer);
			}
		}
		else
		{
			CriticalSectionScope lock(m_CriticalSection);
			m_SendBufferList.AddTail(p_send_buffer);
		}
	}

	//------------------------------------------------------------------------
	SendBuffer* FrameProTLS::AllocateSendBuffer()
	{
		CriticalSectionScope lock(m_CriticalSection);

		SendBuffer* p_send_buffer = NULL;

		if(!m_SendBufferFreeList.IsEmpty())
		{
			p_send_buffer = m_SendBufferFreeList.RemoveHead();
		}
		else
		{
			p_send_buffer = New<SendBuffer>(mp_Allocator, mp_Allocator, m_SendBufferCapacity, this);
			m_SendBufferMemorySize = m_SendBufferMemorySize + m_SendBufferCapacity + sizeof(SendBuffer);		// doesn't need to be atomic, it's only for stats
		}

		FRAMEPRO_ASSERT(!p_send_buffer->GetSize());
		FRAMEPRO_ASSERT(!p_send_buffer->GetNext());

		if (!p_send_buffer->GetBuffer())
		{
			p_send_buffer->AllocateBuffer(m_SendBufferCapacity);
			m_SendBufferMemorySize = m_SendBufferMemorySize + m_SendBufferCapacity;
		}

		p_send_buffer->SetCreationTime();

		return p_send_buffer;
	}

	//------------------------------------------------------------------------
	void FrameProTLS::CollectSendBuffers(List<SendBuffer>& list)
	{
		CriticalSectionScope lock(m_CriticalSection);

		list.MoveAppend(m_SendBufferList);
	}

	//------------------------------------------------------------------------
	ConditionalParentScope* FrameProTLS::GetConditionalParentScope(const char* p_name)
	{
		FRAMEPRO_ASSERT(m_ConditionalParentScopeListCritSec.Locked());

		ConditionalParentScope* p_scope = m_ConditionalParentScopeList.GetHead();
		while (p_scope)
		{
			if (p_scope->mp_Name == p_name)
				return p_scope;

			p_scope = p_scope->GetNext();
		}

		return NULL;
	}

	//------------------------------------------------------------------------
	ConditionalParentScope* FrameProTLS::CreateConditionalParentScope(const char* p_name)
	{
		FRAMEPRO_ASSERT(m_ConditionalParentScopeListCritSec.Locked());

		ConditionalParentScope* p_scope = New<ConditionalParentScope>(mp_Allocator, p_name);
		m_ConditionalParentScopeList.AddTail(p_scope);

		return p_scope;
	}

	//------------------------------------------------------------------------
	void FrameProTLS::PushConditionalParentScope(const char* p_name, int64 pre_duration, int64 post_duration)
	{
		CriticalSectionScope lock(m_ConditionalParentScopeListCritSec);

		FRAMEPRO_ASSERT(!mp_CurrentConditionalParentScope);		// nested conditional parent scopes not supported

		ConditionalParentScope* p_scope = GetConditionalParentScope(p_name);
		if(!p_scope)
			p_scope = CreateConditionalParentScope(p_name);

		FRAMEPRO_ASSERT(!p_scope->mp_SendBuffer);
		p_scope->mp_SendBuffer = AllocateSendBuffer();

		p_scope->m_PreDuration = pre_duration;
		p_scope->m_PostDuration = post_duration;

		{
			CriticalSectionScope send_buffer_lock(m_CurrentSendBufferCriticalSection);
			p_scope->mp_SendBuffer->Swap(mp_CurrentSendBuffer, m_CurrentSendBufferSize, m_SendBufferCapacity);
		}

		mp_CurrentConditionalParentScope = p_scope;
	}

	//------------------------------------------------------------------------
	void FrameProTLS::PopConditionalParentScope(bool add_children)
	{
		CriticalSectionScope lock(m_ConditionalParentScopeListCritSec);

		ConditionalParentScope* p_scope = mp_CurrentConditionalParentScope;
		mp_CurrentConditionalParentScope = NULL;

		FRAMEPRO_ASSERT(p_scope);		// popped without a push

		{
			// restore the original parent send buffer and grab the current one
			CriticalSectionScope send_buffer_lock(m_CurrentSendBufferCriticalSection);
			p_scope->mp_SendBuffer->Swap(mp_CurrentSendBuffer, m_CurrentSendBufferSize, m_SendBufferCapacity);
		}

		p_scope->m_ChildSendBuffers.AddTail(p_scope->mp_SendBuffer);
		p_scope->mp_SendBuffer = NULL;

		if (add_children)
		{
			FRAMEPRO_GET_CLOCK_COUNT(p_scope->m_LastPopConditionalChildrenTime);
		}

		int64 now;
		FRAMEPRO_GET_CLOCK_COUNT(now);
		bool in_post_duration = now - p_scope->m_LastPopConditionalChildrenTime < (p_scope->m_PostDuration * m_ClockFrequency) / 1000000;

		if (add_children || in_post_duration)
		{
			{
				CriticalSectionScope send_lock(m_CriticalSection);
				m_SendBufferList.MoveAppend(p_scope->m_ChildSendBuffers);
			}
		}
	}

	//------------------------------------------------------------------------
	void FrameProTLS::FlushConditionalChildSendBuffers()
	{
		CriticalSectionScope lock(m_ConditionalParentScopeListCritSec);
		
		int64 now;
		FRAMEPRO_GET_CLOCK_COUNT(now);

		ConditionalParentScope* p_scope = m_ConditionalParentScopeList.GetHead();
		while (p_scope)
		{
			int64 max_duration = (p_scope->m_PreDuration * m_ClockFrequency) / 1000000;
			
			// throw away send buffers that are too old
			SendBuffer* p_send_buffer = p_scope->m_ChildSendBuffers.GetHead();
			while(p_send_buffer && now - p_send_buffer->GetCreationTime() > max_duration)
			{
				p_scope->m_ChildSendBuffers.RemoveHead();
				p_send_buffer->ClearSize();
				AddEmptySendBuffer(p_send_buffer);
				p_send_buffer = p_scope->m_ChildSendBuffers.GetHead();
			}

			p_scope = p_scope->GetNext();
		}
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SendHiResTimersScope(int64 current_time)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());

		int count = m_HiResTimers.GetCount();
		FRAMEPRO_ASSERT(count);

		int size_to_send = sizeof(HiResTimerScopePacket) + count * sizeof(HiResTimerScopePacket::HiResTimer);

		// if we are connecting to FramePro, FramePro will ask for the string value later, otherwise we need to send it now
		if (m_SendStringsImmediately)
		{
			for(int i=0; i<count; ++i)
				SendString(m_HiResTimers[i].mp_Name, PacketType::StringLiteralTimerNamePacket);
		}

		{
			CriticalSectionScope lock(m_CurrentSendBufferCriticalSection);

			HiResTimerScopePacket* p_packet = (HiResTimerScopePacket*)AllocateSpaceInBuffer(size_to_send);
			p_packet->m_PacketType = PacketType::HiResTimerScopePacket;
			p_packet->m_StartTime = m_HiResTimerScopeStartTime;
			p_packet->m_EndTime = current_time;
			p_packet->m_Count = count;
			p_packet->m_ThreadId = m_ThreadId;
			p_packet->m_Padding = 0;

			HiResTimerScopePacket::HiResTimer* p_send_hires_timer = (HiResTimerScopePacket::HiResTimer*)(p_packet + 1);
			memcpy(p_send_hires_timer, &m_HiResTimers[0], count * sizeof(HiResTimer));
		}

		m_HiResTimers.ClearNoFree();
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SetCustomTimeSpanStat(StringId name, int64 value, const char* p_unit)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());

		int64 time;
		FRAMEPRO_GET_CLOCK_COUNT(time);

		{
			CriticalSectionScope lock(m_CurrentSendBufferCriticalSection);

			TimeSpanCustomStatPacket* p_packet = (TimeSpanCustomStatPacket*)AllocateSpaceInBuffer(sizeof(TimeSpanCustomStatPacket));
			p_packet->m_PacketType = PacketType::TimeSpanCustomStatPacket;
			p_packet->m_ThreadId = m_ThreadId;
			p_packet->m_ValueType = CustomStatValueType::Int64;
			p_packet->m_Name = name;
			p_packet->m_Unit = (StringId&)p_unit;
			p_packet->m_ValueInt64 = value;
			p_packet->m_ValueDouble = 0.0;
			p_packet->m_Time = time;
		}

		m_HiResTimers.ClearNoFree();
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SetCustomTimeSpanStat(StringId name, int64 value, const wchar_t* p_unit)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());

		int64 time;
		FRAMEPRO_GET_CLOCK_COUNT(time);

		{
			CriticalSectionScope lock(m_CurrentSendBufferCriticalSection);

			TimeSpanCustomStatPacket* p_packet = (TimeSpanCustomStatPacket*)AllocateSpaceInBuffer(sizeof(TimeSpanCustomStatPacket));
			p_packet->m_PacketType = PacketType::TimeSpanCustomStatPacket;
			p_packet->m_ThreadId = m_ThreadId;
			p_packet->m_ValueType = CustomStatValueType::Int64;
			p_packet->m_Name = name;
			p_packet->m_Unit = (StringId&)p_unit;
			p_packet->m_ValueInt64 = value;
			p_packet->m_ValueDouble = 0.0;
			p_packet->m_Time = time;
		}

		m_HiResTimers.ClearNoFree();
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SetCustomTimeSpanStat(StringId name, double value, const char* p_unit)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());

		int64 time;
		FRAMEPRO_GET_CLOCK_COUNT(time);

		{
			CriticalSectionScope lock(m_CurrentSendBufferCriticalSection);

			TimeSpanCustomStatPacket* p_packet = (TimeSpanCustomStatPacket*)AllocateSpaceInBuffer(sizeof(TimeSpanCustomStatPacket));
			p_packet->m_PacketType = PacketType::TimeSpanCustomStatPacket;
			p_packet->m_ThreadId = m_ThreadId;
			p_packet->m_ValueType = CustomStatValueType::Double;
			p_packet->m_Name = name;
			p_packet->m_Unit = (StringId&)p_unit;
			p_packet->m_ValueInt64 = 0;
			p_packet->m_ValueDouble = value;
			p_packet->m_Time = time;
		}

		m_HiResTimers.ClearNoFree();
	}

	//------------------------------------------------------------------------
	void FrameProTLS::SetCustomTimeSpanStat(StringId name, double value, const wchar_t* p_unit)
	{
		FRAMEPRO_ASSERT(IsOnTLSThread());

		int64 time;
		FRAMEPRO_GET_CLOCK_COUNT(time);

		{
			CriticalSectionScope lock(m_CurrentSendBufferCriticalSection);

			TimeSpanCustomStatPacket* p_packet = (TimeSpanCustomStatPacket*)AllocateSpaceInBuffer(sizeof(TimeSpanCustomStatPacket));
			p_packet->m_PacketType = PacketType::TimeSpanCustomStatPacket;
			p_packet->m_ThreadId = m_ThreadId;
			p_packet->m_ValueType = CustomStatValueType::Double;
			p_packet->m_Name = name;
			p_packet->m_Unit = (StringId&)p_unit;
			p_packet->m_ValueInt64 = 0;
			p_packet->m_ValueDouble = value;
			p_packet->m_Time = time;
		}

		m_HiResTimers.ClearNoFree();
	}

	//------------------------------------------------------------------------
#if FRAMEPRO_ENABLE_CALLSTACKS
	CallstackResult FrameProTLS::GetCallstack()
	{
		return m_StackTrace.Capture();
	}
#endif
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED
//------------------------------------------------------------------------
// IncrementingBlockAllocator.cpp



//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	IncrementingBlockAllocator::IncrementingBlockAllocator(Allocator* p_allocator)
	:	mp_Allocator(p_allocator),
		mp_BlockList(NULL),
		m_CurrentBlockSize(m_MemoryBlockSize),
		m_MemorySize(0)
	{
	}

	//------------------------------------------------------------------------
	IncrementingBlockAllocator::~IncrementingBlockAllocator()
	{
		Clear();
	}

	//------------------------------------------------------------------------
	void IncrementingBlockAllocator::Clear()
	{
		Block* p_block = mp_BlockList;
		while(p_block)
		{
			Block* p_next = p_block->mp_Next;
			mp_Allocator->Free(p_block);
			p_block = p_next;
		}

		mp_BlockList = NULL;
		m_CurrentBlockSize = m_MemoryBlockSize;
		m_MemorySize = 0;
	}

	//------------------------------------------------------------------------
	void* IncrementingBlockAllocator::Alloc(size_t size)
	{
		if(m_CurrentBlockSize + size > m_MemoryBlockSize)
			AllocateBlock();

		void* p_mem = mp_BlockList->m_Memory + m_CurrentBlockSize;
		m_CurrentBlockSize += size;
		return p_mem;
	}

	//------------------------------------------------------------------------
	void IncrementingBlockAllocator::AllocateBlock()
	{
		Block* p_block = (Block*)mp_Allocator->Alloc(sizeof(Block));
		p_block->mp_Next = mp_BlockList;
		mp_BlockList = p_block;
		m_CurrentBlockSize = 0;

		m_MemorySize += m_BlockSize;
	}
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED
//------------------------------------------------------------------------
// PointerSet.cpp



//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	const int g_InitialCapacity = 32;
}

//------------------------------------------------------------------------
FramePro::PointerSet::PointerSet(Allocator* p_allocator)
:	mp_Data((const void**)p_allocator->Alloc(g_InitialCapacity*sizeof(const void*))),
	m_CapacityMask(g_InitialCapacity-1),
	m_Count(0),
	m_Capacity(g_InitialCapacity),
	mp_Allocator(p_allocator)
{
	memset(mp_Data, 0, g_InitialCapacity*sizeof(const void*));
}

//------------------------------------------------------------------------
FramePro::PointerSet::~PointerSet()
{
	mp_Allocator->Free(mp_Data);
}

//------------------------------------------------------------------------
void FramePro::PointerSet::Grow()
{
	int old_capacity = m_Capacity;
	const void** p_old_data = mp_Data;

	// allocate a new set
	m_Capacity = m_Capacity ? 2*m_Capacity : 32;
	FRAMEPRO_ASSERT(m_Capacity < (int)(INT_MAX/sizeof(void*)));

	m_CapacityMask = m_Capacity - 1;
	size_t alloc_size = m_Capacity * sizeof(const void*);
	mp_Data = (const void**)mp_Allocator->Alloc(alloc_size);

	int size = m_Capacity * sizeof(void*);
	memset(mp_Data, 0, size);

	// transfer pointers from old set
	m_Count = 0;
	for(int i=0; i<old_capacity; ++i)
	{
		const void* p = p_old_data[i];
		if(p)
			Add(p);
	}

	// release old buffer
	mp_Allocator->Free(p_old_data);

	alloc_size -= old_capacity * sizeof(const void*);
}

//------------------------------------------------------------------------
// return true if added, false if already in set
bool FramePro::PointerSet::AddInternal(const void* p, int64 hash, int index)
{
	if(m_Count >= m_Capacity/4)
	{
		Grow();
		index = hash & m_CapacityMask;
	}

	const void* p_existing = mp_Data[index];
	while(p_existing)
	{
		if(p_existing == p)
			return false;
		index = (index + 1) & m_CapacityMask;
		p_existing = mp_Data[index];
	}

	mp_Data[index] = p;

	++m_Count;

	return true;
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED
//------------------------------------------------------------------------
// SendBuffer.cpp



//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	//------------------------------------------------------------------------
	SendBuffer::SendBuffer(Allocator* p_allocator, int capacity, FrameProTLS* p_owner)
	:	mp_Buffer(p_allocator->Alloc(capacity)),
		m_Size(0),
		m_Capacity(capacity),
		mp_Next(NULL),
		mp_Allocator(p_allocator),
		mp_Owner(p_owner),
		m_CreationTime(0)
	{
		SetCreationTime();
	}

	//------------------------------------------------------------------------
	SendBuffer::~SendBuffer()
	{
		ClearBuffer();
	}

	//------------------------------------------------------------------------
	void SendBuffer::AllocateBuffer(int capacity)
	{
		FRAMEPRO_ASSERT(!mp_Buffer);

		mp_Buffer = mp_Allocator->Alloc(capacity);
		m_Capacity = capacity;
	}

	//------------------------------------------------------------------------
	void SendBuffer::ClearBuffer()
	{
		if(mp_Buffer)
		{
			mp_Allocator->Free(mp_Buffer);
			mp_Buffer = NULL;
		}

		m_Size = 0;
		m_Capacity = 0;
	}

	//------------------------------------------------------------------------
	void SendBuffer::SetCreationTime()
	{
		FRAMEPRO_GET_CLOCK_COUNT(m_CreationTime);
	}
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED
//------------------------------------------------------------------------
// Socket.cpp

// START EPIC
// Remove FRAMEPRO_UNIX_BASED_PLATFORM
// END EPIC

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED && FRAMEPRO_SOCKETS_ENABLED

//------------------------------------------------------------------------
namespace FramePro
{
	volatile int g_InitialiseCount = 0;
}

//------------------------------------------------------------------------
bool FramePro::Socket::InitialiseWSA()
{
	if(g_InitialiseCount == 0)
	{
#if FRAMEPRO_PLATFORM_XBOX360
		  XNetStartupParams xnsp;
		  memset(&xnsp, 0, sizeof(xnsp));
		  xnsp.cfgSizeOfStruct = sizeof(XNetStartupParams);
		  xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
		  INT err = XNetStartup(&xnsp);
#endif

#if FRAMEPRO_WIN_BASED_PLATFORM
		// Initialize Winsock
		WSADATA wsaData;
		if(WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
		{
			HandleError();
			return false;
		}
#endif
// BEGIN EPIC
#if FRAMEPRO_PLATFORM_SWITCH
		if( !FramePro::PlatformDebugNetInit() )
		{
			return false;
		}
#endif
// END EPIC

	}

	++g_InitialiseCount;

	return true;
}

//------------------------------------------------------------------------
void FramePro::Socket::CleanupWSA()
{
	--g_InitialiseCount;

	if(g_InitialiseCount == 0)
	{
#if FRAMEPRO_WIN_BASED_PLATFORM
		if(WSACleanup() == FRAMEPRO_SOCKET_ERROR)
			HandleError();
#endif

#if FRAMEPRO_PLATFORM_XBOX360
		 XNetCleanup();
#endif
	}
}

//------------------------------------------------------------------------
void FramePro::Socket::Disconnect()
{
	if(m_Socket != FRAMEPRO_INVALID_SOCKET)
	{
#if FRAMEPRO_WIN_BASED_PLATFORM
		if(!m_Listening && shutdown(m_Socket, SD_BOTH) == FRAMEPRO_SOCKET_ERROR)
			HandleError();
#else
		if(shutdown(m_Socket, SHUT_RDWR) == FRAMEPRO_SOCKET_ERROR)
			HandleError();
#endif

		// loop until the socket is closed to ensure all data is sent
		unsigned int buffer = 0;
		size_t ret = 0;
		do { ret = recv(m_Socket, (char*)&buffer, sizeof(buffer), 0); } while(ret != 0 && ret != (size_t)FRAMEPRO_SOCKET_ERROR);

#if FRAMEPRO_WIN_BASED_PLATFORM
	    if(closesocket(m_Socket) == FRAMEPRO_SOCKET_ERROR)
			HandleError();
#else
		close(m_Socket);
#endif
		m_Socket = FRAMEPRO_INVALID_SOCKET;
	}
}

//------------------------------------------------------------------------
bool FramePro::Socket::StartListening()
{
	FRAMEPRO_ASSERT(m_Socket != FRAMEPRO_INVALID_SOCKET);

	if (listen(m_Socket, SOMAXCONN) == FRAMEPRO_SOCKET_ERROR)
	{
		HandleError();
		return false;
	}

	m_Listening = true;

	return true;
}

//------------------------------------------------------------------------
bool FramePro::Socket::Bind(const char* p_port)
{
	FRAMEPRO_ASSERT(m_Socket == FRAMEPRO_INVALID_SOCKET);

	if(!InitialiseWSA())
		return false;

#if FRAMEPRO_WIN_BASED_PLATFORM
	// setup the addrinfo struct
	addrinfo info;
	ZeroMemory(&info, sizeof(info));
	info.ai_family = AF_INET;
	info.ai_socktype = SOCK_STREAM;
	info.ai_protocol = IPPROTO_TCP;
	info.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	addrinfo* p_result_info;
	HRESULT result = getaddrinfo(NULL, p_port, &info, &p_result_info);
	if (result != 0)
	{
		HandleError();
		return false;
	}

	m_Socket = socket(
		p_result_info->ai_family,
		p_result_info->ai_socktype, 
		p_result_info->ai_protocol);
#else
	m_Socket = socket(
		AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP);
#endif

	if (m_Socket == FRAMEPRO_INVALID_SOCKET)
	{
#if FRAMEPRO_WIN_BASED_PLATFORM
		freeaddrinfo(p_result_info);
#endif
		HandleError();
		return false;
	}

	// Setup the TCP listening socket
#if FRAMEPRO_WIN_BASED_PLATFORM
	result = ::bind(m_Socket, p_result_info->ai_addr, (int)p_result_info->ai_addrlen);
	freeaddrinfo(p_result_info);
#else
	// Bind to INADDR_ANY
	sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = INADDR_ANY;
	int iport = atoi(p_port);
	sa.sin_port = htons(iport);
	int result = ::bind(m_Socket, (const sockaddr*)(&sa), sizeof(sockaddr_in));
#endif

	if (result == FRAMEPRO_SOCKET_ERROR)
	{
		HandleError();
		Disconnect();
		return false;
	}

	return true;
}

//------------------------------------------------------------------------
bool FramePro::Socket::Accept(Socket& client_socket)
{
	FRAMEPRO_ASSERT(client_socket.m_Socket == FRAMEPRO_INVALID_SOCKET);
	client_socket.m_Socket = accept(m_Socket, NULL, NULL);
	return client_socket.m_Socket != FRAMEPRO_INVALID_SOCKET;
}

//------------------------------------------------------------------------
bool FramePro::Socket::Send(const void* p_buffer, size_t size)
{
	FRAMEPRO_ASSERT(size >= 0 && size <= INT_MAX);

	int bytes_to_send = (int)size;
	while(bytes_to_send != 0)
	{
// START EPIC
#if FRAMEPRO_PLATFORM_UNIX
// END EPIC
		int flags = MSG_NOSIGNAL;
#else
		int flags = 0;
#endif

		int bytes_sent = (int)send(m_Socket, (char*)p_buffer, bytes_to_send, flags);
		if(bytes_sent == FRAMEPRO_SOCKET_ERROR)
		{
			HandleError();
			Disconnect();
			return false;
		}
		p_buffer = (char*)p_buffer + bytes_sent;
		bytes_to_send -= bytes_sent;
	}

	return true;
}

//------------------------------------------------------------------------
int FramePro::Socket::Receive(void* p_buffer, int size)
{
	int total_bytes_received = 0;

	while(size)
	{
		int bytes_received = (int)recv(m_Socket, (char*)p_buffer, size, 0);

		if(bytes_received == 0)
		{
			Disconnect();
			return bytes_received;
		}
		else if(bytes_received == FRAMEPRO_SOCKET_ERROR)
		{
			HandleError();
			Disconnect();
			return total_bytes_received;
		}

		total_bytes_received += bytes_received;

		size -= bytes_received;
		FRAMEPRO_ASSERT(size >= 0);

		p_buffer = (char*)p_buffer + bytes_received;
	}

	return total_bytes_received;
}

//------------------------------------------------------------------------
void FramePro::Socket::HandleError()
{
#if FRAMEPRO_WIN_BASED_PLATFORM
	if(WSAGetLastError() == WSAEADDRINUSE)
	{
		OutputDebugString(FRAMEPRO_STRING("FramePro: Network connection conflict. Please make sure that other FramePro enabled applications are shut down, or change the port in the the FramePro lib and FramePro settings.\n"));
		return;
	}

	int buffer_size = 1024;
	TCHAR* p_buffer = (TCHAR*)HeapAlloc(GetProcessHeap(), 0, buffer_size*sizeof(TCHAR));
	memset(p_buffer, 0, buffer_size*sizeof(TCHAR));

	va_list args;
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		WSAGetLastError(),
		0,
		p_buffer,
		buffer_size,
		&args);

	DebugWrite(FRAMEPRO_STRING("FramePro Network Error: %s\n"), p_buffer);

	HeapFree(GetProcessHeap(), 0, p_buffer);
#endif
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED && FRAMEPRO_SOCKETS_ENABLED

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED
//------------------------------------------------------------------------
// Thread.cpp



//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
// START EPIC
// Remove FRAMEPRO_UNIX_BASED_PLATFORM
// END EPIC

//------------------------------------------------------------------------
FramePro::Thread::Thread()
#if FRAMEPRO_WIN_BASED_PLATFORM
	:	m_Handle(0),
		m_Alive(false),
		m_ThreadTerminatedEvent(false, false)
// BEGIN EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
	:	m_Alive(false),
		m_ThreadTerminatedEvent(false,false)
// END EPIC
#else
	:	m_Alive(false),
		m_ThreadTerminatedEvent(false, false)
#endif
{
}

//------------------------------------------------------------------------
void FramePro::Thread::CreateThread(ThreadMain p_thread_main, void* p_param)
{
	mp_ThreadMain = p_thread_main;
	mp_Param = p_param;

#if FRAMEPRO_WIN_BASED_PLATFORM
	m_Handle = ::CreateThread(NULL, 0, PlatformThreadMain, this, 0, NULL);
// BEGIN EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
	m_Runnable = TUniquePtr<FRunnableThread>( FRunnableThread::Create( this, TEXT("FramePro") ) );
// END EPIC
#else
	pthread_create(&m_Thread, NULL, PlatformThreadMain, this);
#endif
}

//------------------------------------------------------------------------
#if FRAMEPRO_WIN_BASED_PLATFORM
unsigned long WINAPI FramePro::Thread::PlatformThreadMain(void* p_param)
{
	Thread* p_thread = (Thread*)p_param;
	p_thread->m_Alive = true;
	unsigned long ret = (unsigned long)p_thread->mp_ThreadMain(p_thread->mp_Param);
	p_thread->m_Alive = false;
	p_thread->m_ThreadTerminatedEvent.Set();
	return ret;
}
// BEGIN EPIC
#elif FRAMEPRO_UE4_BASED_PLATFORM
uint32 FramePro::Thread::Run()
{
	m_Alive = true;
	mp_ThreadMain(mp_Param);
	m_Alive = false;
	m_ThreadTerminatedEvent.Set();
	return 0;
}
// END EPIC
#else
void* FramePro::Thread::PlatformThreadMain(void* p_param)
{
	Thread* p_thread = (Thread*)p_param;
	p_thread->m_Alive = true;
	p_thread->mp_ThreadMain(p_thread->mp_Param);
	p_thread->m_Alive = false;
	p_thread->m_ThreadTerminatedEvent.Set();
	return NULL;
}
#endif

//------------------------------------------------------------------------
void FramePro::Thread::SetPriority(int priority)
{
#if FRAMEPRO_WIN_BASED_PLATFORM
	::SetThreadPriority(m_Handle, priority);
#endif
}

//------------------------------------------------------------------------
void FramePro::Thread::SetAffinity(int affinity)
{
#if FRAMEPRO_WIN_BASED_PLATFORM && !FRAMEPRO_PLATFORM_UWP
	SetThreadAffinityMask(m_Handle, affinity);
#endif
}

//------------------------------------------------------------------------
void FramePro::Thread::WaitForThreadToTerminate(int timeout)
{
	m_ThreadTerminatedEvent.Wait(timeout);
}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED
