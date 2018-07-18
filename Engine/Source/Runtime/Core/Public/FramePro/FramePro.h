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

// START EPIC
#include "CoreTypes.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
// END EPIC

//------------------------------------------------------------------------
// FramePro.hpp
//------------------------------------------------------------------------
/*
	FramePro
	Version:	1.4.7.1
*/
//------------------------------------------------------------------------
#ifndef FRAMEPRO_H_INCLUDED
#define FRAMEPRO_H_INCLUDED

//------------------------------------------------------------------------
#ifndef FRAMEPRO_ENABLED
// START EPIC
	#define FRAMEPRO_ENABLED	0
// END EPIC
#endif

//------------------------------------------------------------------------
// Scroll down for detailed documentation on all of these macros.
#if FRAMEPRO_ENABLED

	// session macros
	#define FRAMEPRO_FRAME_START()															FramePro::FrameStart()
	#define FRAMEPRO_SHUTDOWN()																FramePro::Shutdown()
	#define FRAMEPRO_SET_PORT(port)															FramePro::SetPort(port)
	#define FRAMEPRO_SET_SESSION_INFO(name, build_id)										FramePro::SendSessionInfo(name, build_id)
	#define FRAMEPRO_SET_ALLOCATOR(p_allocator)												FramePro::SetAllocator(p_allocator)
	#define FRAMEPRO_SET_THREAD_NAME(name)													FramePro::SetThreadName(name)
	#define FRAMEPRO_THREAD_ORDER(thread_name)												FramePro::SetThreadOrder(FramePro::RegisterString(thread_name))
	#define FRAMEPRO_REGISTER_STRING(str)													FramePro::RegisterString(str)
	#define FRAMEPRO_START_RECORDING(filename, context_switches, max_file_size)				FramePro::StartRecording(filename, context_switches, max_file_size)
	#define FRAMEPRO_STOP_RECORDING()														FramePro::StopRecording()
	#define FRAMEPRO_REGISTER_CONNECTION_CHANGED_CALLBACK(callback, context)				FramePro::RegisterConnectionChangedCallback(callback, context)
	#define FRAMEPRO_UNREGISTER_CONNECTION_CHANGED_CALLBACK(callback)						FramePro::UnregisterConnectionChangedcallback(callback)
	#define FRAMEPRO_SET_THREAD_PRIORITY(priority)											FramePro::SetThreadPriority(priority)
	#define FRAMEPRO_SET_THREAD_AFFINITY(affinity)											FramePro::SetThreadAffinity(affinity)
	#define FRAMEPRO_BLOCK_SOCKETS()														FramePro::BlockSockets()
	#define FRAMEPRO_UNBLOCK_SOCKETS()														FramePro::UnblockSockets()
	#define FRAMEPRO_CLEANUP_THREAD()														FramePro::CleanupThread()
	#define FRAMEPRO_THREAD_SCOPE(thread_name)												FramePro::ThreadScope framepro_thread_scope(thread_name)
	#define FRAMEPRO_LOG(message)															FramePro::Log(message)
	#define FRAMEPRO_COLOUR(r, g, b)														((uint)((((uint)((r)&0xff))<<16) | (((uint)((g)&0xff))<<16) | ((b)&0xff)))
	#define FRAMEPRO_SET_CONDITIONAL_SCOPE_MIN_TIME(microseconds)							FramePro::SetConditionalScopeMinTimeInMicroseconds(microseconds)

	// scope macros
	#define FRAMEPRO_SCOPE()																FramePro::TimerScope FRAMEPRO_UNIQUE(timer_scope)(FRAMEPRO_FUNCTION_NAME "|" FRAMEPRO_SOURCE_STRING)
	#define FRAMEPRO_NAMED_SCOPE(name)														FramePro::TimerScope FRAMEPRO_UNIQUE(timer_scope)(name "|" FRAMEPRO_SOURCE_STRING)
	#define FRAMEPRO_NAMED_SCOPE_W(name)													FramePro::TimerScopeW FRAMEPRO_UNIQUE(timer_scope)(name L"|" FRAMEPRO_SOURCE_STRING_W)
	#define FRAMEPRO_ID_SCOPE(name_id)														FramePro::IdTimerScope FRAMEPRO_UNIQUE(timer_scope)(name_id, FRAMEPRO_SOURCE_STRING)
	#define FRAMEPRO_DYNAMIC_SCOPE(dynamic_string)											FramePro::IdTimerScope FRAMEPRO_UNIQUE(timer_scope)(FramePro::IsConnected() ? FramePro::RegisterString(dynamic_string) : -1, FRAMEPRO_SOURCE_STRING)
	#define FRAMEPRO_CONDITIONAL_SCOPE()													FramePro::ConditionalTimerScope FRAMEPRO_UNIQUE(timer_scope)(FRAMEPRO_FUNCTION_NAME "|" FRAMEPRO_SOURCE_STRING)
	#define FRAMEPRO_CONDITIONAL_ID_SCOPE(name)												FramePro::ConditionalTimerScopeId FRAMEPRO_UNIQUE(timer_scope)(name, FRAMEPRO_SOURCE_STRING)
	#define FRAMEPRO_CONDITIONAL_NAMED_SCOPE(name)											FramePro::ConditionalTimerScope FRAMEPRO_UNIQUE(timer_scope)(name "|" FRAMEPRO_SOURCE_STRING)
	#define FRAMEPRO_CONDITIONAL_NAMED_SCOPE_W(name)										FramePro::ConditionalTimerScopeW FRAMEPRO_UNIQUE(timer_scope)(name L"|" FRAMEPRO_SOURCE_STRING_W)
	#define FRAMEPRO_CONDITIONAL_BOOL_SCOPE(b)												FramePro::ConditionalBoolTimerScope FRAMEPRO_UNIQUE(timer_scope)(FRAMEPRO_FUNCTION_NAME "|" FRAMEPRO_SOURCE_STRING, (b))
	#define FRAMEPRO_CONDITIONAL_BOOL_ID_SCOPE(name, b)										FramePro::ConditionalBoolTimerScopeId FRAMEPRO_UNIQUE(timer_scope)(name, FRAMEPRO_SOURCE_STRING, (b))
	#define FRAMEPRO_CONDITIONAL_BOOL_NAMED_SCOPE(name, b)									FramePro::ConditionalBoolTimerScope FRAMEPRO_UNIQUE(timer_scope)(name "|" FRAMEPRO_SOURCE_STRING, (b))
	#define FRAMEPRO_CONDITIONAL_BOOL_NAMED_SCOPE_W(name, b)								FramePro::ConditionalBoolTimerScopeW FRAMEPRO_UNIQUE(timer_scope)(name L"|" FRAMEPRO_SOURCE_STRING_W, (b))
	#define FRAMEPRO_START_NAMED_SCOPE(name)												FramePro::int64 framepro_start_##name=0; FRAMEPRO_GET_CLOCK_COUNT(framepro_start_##name);
	#define FRAMEPRO_STOP_NAMED_SCOPE(name)													MULTI_STATEMENT( if(FramePro::IsConnected()) { FramePro::int64 framepro_end_##name; FRAMEPRO_GET_CLOCK_COUNT(framepro_end_##name); FramePro::AddTimeSpan(#name "|" FRAMEPRO_SOURCE_STRING, framepro_start_##name, framepro_end_##name); } )
	#define FRAMEPRO_CONDITIONAL_START_SCOPE()												FramePro::int64 framepro_start=0; MULTI_STATEMENT( if(FramePro::IsConnected()) { FRAMEPRO_GET_CLOCK_COUNT(framepro_start); } )
	#define FRAMEPRO_CONDITIONAL_STOP_NAMED_SCOPE(name)										MULTI_STATEMENT( if(FramePro::IsConnected()) { FramePro::int64 framepro_end; FRAMEPRO_GET_CLOCK_COUNT(framepro_end); if(framepro_end - framepro_start > FramePro::GetConditionalScopeMinTime()) FramePro::AddTimeSpan(name "|" FRAMEPRO_SOURCE_STRING, framepro_start, framepro_end); } )
	#define FRAMEPRO_CONDITIONAL_STOP_DYNAMIC_SCOPE(dynamic_string)							MULTI_STATEMENT( if(FramePro::IsConnected()) { FramePro::int64 framepro_end; FRAMEPRO_GET_CLOCK_COUNT(framepro_end); if(framepro_end - framepro_start > FramePro::GetConditionalScopeMinTime()) FramePro::AddTimeSpan(FramePro::RegisterString(dynamic_string), FRAMEPRO_SOURCE_STRING, framepro_start, framepro_end); } )
	#define FRAMEPRO_CONDITIONAL_PARENT_SCOPE(name, callback, pre_duration, post_duration)	FramePro::ConditionalParentTimerScope FRAMEPRO_UNIQUE(timer_scope)(name, FRAMEPRO_SOURCE_STRING, callback, pre_duration, post_duration)

	// idle scope macros
	#define FRAMEPRO_IDLE_SCOPE()															FramePro::TimerScope FRAMEPRO_UNIQUE(timer_scope)(FRAMEPRO_FUNCTION_NAME "|" FRAMEPRO_SOURCE_STRING_IDLE)
	#define FRAMEPRO_IDLE_NAMED_SCOPE(name)													FramePro::TimerScope FRAMEPRO_UNIQUE(timer_scope)(name "|" FRAMEPRO_SOURCE_STRING_IDLE)
	#define FRAMEPRO_IDLE_NAMED_SCOPE_W(name)												FramePro::TimerScopeW FRAMEPRO_UNIQUE(timer_scope)(name L"|" FRAMEPRO_SOURCE_STRING_IDLE_W)
	#define FRAMEPRO_IDLE_ID_SCOPE(name_id)													FramePro::IdTimerScope FRAMEPRO_UNIQUE(timer_scope)(name_id, FRAMEPRO_SOURCE_STRING_IDLE)
	#define FRAMEPRO_IDLE_DYNAMIC_SCOPE(dynamic_string)										FramePro::IdTimerScope FRAMEPRO_UNIQUE(timer_scope)(FramePro::IsConnected() ? FramePro::RegisterString(dynamic_string) : -1, FRAMEPRO_SOURCE_STRING_IDLE)
	#define FRAMEPRO_IDLE_CONDITIONAL_SCOPE()												FramePro::ConditionalTimerScope FRAMEPRO_UNIQUE(timer_scope)(FRAMEPRO_FUNCTION_NAME "|" FRAMEPRO_SOURCE_STRING_IDLE)
	#define FRAMEPRO_IDLE_CONDITIONAL_ID_SCOPE(name)										FramePro::ConditionalTimerScopeId FRAMEPRO_UNIQUE(timer_scope)(name, FRAMEPRO_SOURCE_STRING_IDLE)
	#define FRAMEPRO_IDLE_CONDITIONAL_NAMED_SCOPE(name)										FramePro::ConditionalTimerScope FRAMEPRO_UNIQUE(timer_scope)(name "|" FRAMEPRO_SOURCE_STRING_IDLE)
	#define FRAMEPRO_IDLE_CONDITIONAL_NAMED_SCOPE_W(name)									FramePro::ConditionalTimerScopeW FRAMEPRO_UNIQUE(timer_scope)(name L"|" FRAMEPRO_SOURCE_STRING_IDLE_W)
	#define FRAMEPRO_IDLE_START_NAMED_SCOPE(name)											FramePro::int64 framepro_start_##name=0; FRAMEPRO_GET_CLOCK_COUNT(framepro_start_##name);
	#define FRAMEPRO_IDLE_STOP_NAMED_SCOPE(name)											MULTI_STATEMENT( if(FramePro::IsConnected()) { FramePro::int64 framepro_end_##name; FRAMEPRO_GET_CLOCK_COUNT(framepro_end_##name); FramePro::AddTimeSpan(#name "|" FRAMEPRO_SOURCE_STRING_IDLE, framepro_start_##name, framepro_end_##name); } )
	#define FRAMEPRO_IDLE_CONDITIONAL_START_SCOPE()											FramePro::int64 framepro_start=0; MULTI_STATEMENT( if(FramePro::IsConnected()) { FRAMEPRO_GET_CLOCK_COUNT(framepro_start); } )
	#define FRAMEPRO_IDLE_CONDITIONAL_STOP_NAMED_SCOPE(name)								MULTI_STATEMENT( if(FramePro::IsConnected()) { FramePro::int64 framepro_end; FRAMEPRO_GET_CLOCK_COUNT(framepro_end); if(framepro_end - framepro_start > FramePro::GetConditionalScopeMinTime()) FramePro::AddTimeSpan(name "|" FRAMEPRO_SOURCE_STRING_IDLE, framepro_start, framepro_end); } )
	#define FRAMEPRO_IDLE_CONDITIONAL_STOP_DYNAMIC_SCOPE(dynamic_string)					MULTI_STATEMENT( if(FramePro::IsConnected()) { FramePro::int64 framepro_end; FRAMEPRO_GET_CLOCK_COUNT(framepro_end); if(framepro_end - framepro_start > FramePro::GetConditionalScopeMinTime()) FramePro::AddTimeSpan(FramePro::RegisterString(dynamic_string), FRAMEPRO_SOURCE_STRING_IDLE, framepro_start, framepro_end); } )

	// custom stat macros
	#define FRAMEPRO_CUSTOM_STAT(name, value, graph, unit)									MULTI_STATEMENT( if(FramePro::IsConnected()) FramePro::AddCustomStat(name, value, graph, unit); )
	#define FRAMEPRO_DYNAMIC_CUSTOM_STAT(name, value, graph, unit)							MULTI_STATEMENT( if(FramePro::IsConnected()) FramePro::AddCustomStat(FramePro::IsConnected() ? FramePro::RegisterString(name) : -1, value, graph, unit); )
	#define FRAMEPRO_SCOPE_CUSTOM_STAT(name, value, graph, unit)							MULTI_STATEMENT( if(FramePro::IsConnected()) FramePro::SetScopeCustomStat(name, value, graph, unit); )

	// high-res timers
	#define FRAMEPRO_HIRES_SCOPE(name)														FramePro::HiResTimerScope FRAMEPRO_UNIQUE(hires_scope)(name)

	// global high-res timers
	#define FRAMEPRO_DECL_GLOBAL_HIRES_TIMER(name, graph_name)								FramePro::GlobalHiResTimer g_FrameProHiResTimer##name(#name, graph_name)
	#define FRAMEPRO_GLOBAL_HIRES_SCOPE(name)												FramePro::GlobalHiResTimerScope FRAMEPRO_UNIQUE(timer_scope)(g_FrameProHiResTimer##name)

	// events
	#define FRAMEPRO_EVENT(name, colour)													FramePro::AddEvent(name, colour)

	// wait events
	#define FRAMEPRO_WAIT_EVENT_SCOPE(event_id)												FramePro::WaitEventScope FRAMEPRO_UNIQUE(timer_scope)((FramePro::int64)event_id)
	#define FRAMEPRO_TRIGGER_WAIT_EVENT(event_id)											FramePro::TriggerWaitEvent((FramePro::int64)event_id);
#else

	#define FRAMEPRO_FRAME_START()															((void)0)
	#define FRAMEPRO_SHUTDOWN()																((void)0)
	#define FRAMEPRO_SET_PORT(port)															((void)0)
	#define FRAMEPRO_SET_SESSION_INFO(name, id)												((void)0)
	#define FRAMEPRO_SET_ALLOCATOR(p_allocator)												((void)0)
	#define FRAMEPRO_SET_THREAD_NAME(name)													((void)0)
	#define FRAMEPRO_THREAD_ORDER(thread_name)												((void)0)
	#define FRAMEPRO_REGISTER_STRING(str)													0
	#define FRAMEPRO_START_RECORDING(filename, context_switches, max_file_size)				((void)0)
	#define FRAMEPRO_STOP_RECORDING()														((void)0)
	#define FRAMEPRO_REGISTER_CONNECTION_CHANGED_CALLBACK(callback, context)				((void)0)
	#define FRAMEPRO_UNREGISTER_CONNECTION_CHANGED_CALLBACK(callback)						((void)0)
	#define FRAMEPRO_SET_THREAD_PRIORITY(priority)											((void)0)
	#define FRAMEPRO_SET_THREAD_AFFINITY(affinity)											((void)0)
	#define FRAMEPRO_UNBLOCK_SOCKETS()														((void)0)
	#define FRAMEPRO_CLEANUP_THREAD()														((void)0)
	#define FRAMEPRO_THREAD_SCOPE(thread_name)												((void)0)
	#define FRAMEPRO_LOG(message)															((void)0)
	#define FRAMEPRO_COLOUR(r, g, b)														((void)0)
	#define FRAMEPRO_SET_CONDITIONAL_SCOPE_MIN_TIME(microseconds)							((void)0)

	#define FRAMEPRO_SCOPE()																((void)0)
	#define FRAMEPRO_NAMED_SCOPE(name)														((void)0)
	#define FRAMEPRO_NAMED_SCOPE_W(name)													((void)0)
	#define FRAMEPRO_ID_SCOPE(name_id)														((void)name_id)
	#define FRAMEPRO_DYNAMIC_SCOPE(dynamic_string)											((void)0)
	#define FRAMEPRO_CONDITIONAL_SCOPE()													((void)0)
	#define FRAMEPRO_CONDITIONAL_ID_SCOPE(name)												((void)0)
	#define FRAMEPRO_CONDITIONAL_NAMED_SCOPE(name)											((void)0)
	#define FRAMEPRO_CONDITIONAL_NAMED_SCOPE_W(name)										((void)0)
	#define FRAMEPRO_CONDITIONAL_BOOL_SCOPE(b)												((void)0)
	#define FRAMEPRO_CONDITIONAL_BOOL_ID_SCOPE(name, b)										((void)0)
	#define FRAMEPRO_CONDITIONAL_BOOL_NAMED_SCOPE(name, b)									((void)0)
	#define FRAMEPRO_CONDITIONAL_BOOL_NAMED_SCOPE_W(name, b)								((void)0)
	#define FRAMEPRO_START_NAMED_SCOPE(name)												((void)0)
	#define FRAMEPRO_STOP_NAMED_SCOPE(name)													((void)0)
	#define FRAMEPRO_CONDITIONAL_START_SCOPE()												((void)0)
	#define FRAMEPRO_CONDITIONAL_STOP_NAMED_SCOPE(name)										((void)0)
	#define FRAMEPRO_CONDITIONAL_STOP_DYNAMIC_SCOPE(dynamic_string)							((void)0)
	#define FRAMEPRO_CONDITIONAL_PARENT_SCOPE(name, callback, pre_duration, post_duration)	((void)0)

	#define FRAMEPRO_IDLE_SCOPE()															((void)0)
	#define FRAMEPRO_IDLE_NAMED_SCOPE(name)													((void)0)
	#define FRAMEPRO_IDLE_NAMED_SCOPE_W(name)												((void)0)
	#define FRAMEPRO_IDLE_ID_SCOPE(name_id)													((void)name_id)
	#define FRAMEPRO_IDLE_DYNAMIC_SCOPE(dynamic_string)										((void)0)
	#define FRAMEPRO_IDLE_CONDITIONAL_SCOPE()												((void)0)
	#define FRAMEPRO_IDLE_CONDITIONAL_ID_SCOPE(name)										((void)0)
	#define FRAMEPRO_IDLE_CONDITIONAL_NAMED_SCOPE(name)										((void)0)
	#define FRAMEPRO_IDLE_CONDITIONAL_NAMED_SCOPE_W(name)									((void)0)
	#define FRAMEPRO_IDLE_START_NAMED_SCOPE(name)											((void)0)
	#define FRAMEPRO_IDLE_STOP_NAMED_SCOPE(name)											((void)0)
	#define FRAMEPRO_IDLE_CONDITIONAL_START_SCOPE()											((void)0)
	#define FRAMEPRO_IDLE_CONDITIONAL_STOP_NAMED_SCOPE(name)								((void)0)
	#define FRAMEPRO_IDLE_CONDITIONAL_STOP_DYNAMIC_SCOPE(dynamic_string)					((void)0)

	#define FRAMEPRO_CUSTOM_STAT(name, value, graph, unit)									((void)0)
	#define FRAMEPRO_DYNAMIC_CUSTOM_STAT(name, value, graph, unit)							((void)0)
	#define FRAMEPRO_SCOPE_CUSTOM_STAT(name, value, graph, unit)							((void)0)

	#define FRAMEPRO_HIRES_SCOPE(name)														((void)0)

	#define FRAMEPRO_DECL_GLOBAL_HIRES_TIMER(name, graph_name)								
	#define FRAMEPRO_GLOBAL_HIRES_SCOPE(name)												((void)0)

	#define FRAMEPRO_EVENT(name, colour)													((void)0)

	#define FRAMEPRO_WAIT_EVENT_SCOPE(event_id)												((void)0)
	#define FRAMEPRO_TRIGGER_WAIT_EVENT(event_id)											((void)0)
#endif

//------------------------------------------------------------------------
// StringId typedef always needs to be defined
namespace FramePro
{
	typedef long long StringId;
}

//------------------------------------------------------------------------
// General defines

//------------------------------------------------------------------------
// Set this to zero if you don't want anyone to be able to connect with FramePro.
// Recording to a file from code is sill supported.
#define FRAMEPRO_SOCKETS_ENABLED 1

//------------------------------------------------------------------------
// Thread local storage buffers are flushed every 30 ms by default. You shouldn't need to change this.
#define FRAMEPRO_MAX_SEND_DELAY 30

//------------------------------------------------------------------------
// Write the network data out to a file. Only useful for debugging network issues.
#define FRAMEPRO_DEBUG_TCP 0

//------------------------------------------------------------------------
// FramePro will attempt to keep below this amount of memory. If the memory limit
// is reached it will stall your game. Nothing is pre-allocated, so FramePro will
// only use up as much mmeory as it needs, which depends on how many scopes are
// being sent per frame.
#define FRAMEPRO_MAX_MEMORY (50*1024*1024)

//------------------------------------------------------------------------
// For added security you can set this to true to disable all networking unless explicitly
// enabled from code using the FRAMEPRO_UNBLOCK_SOCKETS() macro. In general, it's best to completely
// define out FramePro with the FRAMEPRO_ENABLED define for retail builds.
#define FRAMEPRO_SOCKETS_BLOCKED_BY_DEFAULT false

//------------------------------------------------------------------------
// Never send scopes shorter than this. Undefine to send all scopes.
#define FRAMEPRO_SCOPE_MIN_TIME 10       // in ns

//------------------------------------------------------------------------
#define FRAMEPRO_WAIT_EVENT_MIN_TIME 10		// in ns

//------------------------------------------------------------------------
// START EPIC
#define FRAMEPRO_ENABLE_CALLSTACKS 0
// END EPIC

//------------------------------------------------------------------------
// If this is disabled and two dynamic strings happen to hash to the same value you may see
// incorrect strings in the FramePro app. Enabling this does add significant overhead
// to RegisterString. It is a good idea to use the string literal macros wherever possible
// which don't call RegiserString anyway.
#define FRAMEPRO_DETECT_HASH_COLLISIONS 0			

//------------------------------------------------------------------------
// DOCUMENTATION
//------------------------------------------------------------------------

// Setup
// -----
// Please see the setup guide in the help documentation or online, or the setup video for
// full instructions on setting up FramePro.
//

// Overview
// --------
// The scope macros are designed to have minimal overhead. Sending a scope to FramePro requires
// recording the start and end time of the scope and sending a string for the scope name. In order
// to avoid having to send strings across the network, FramePro only sends string ids. For string
// literals the id is the same as the string pointer and no further work needs to be done. The
// FramePro app will do the lookup and request the string value if necessary. For synamic strings
// the string has to be looked up in a hash map. For this reason it is always preferable to use
// the string literal versions of the macros.

// To send a scope the scope data (start, end, name_id) is copied to a thread local storage buffer.
// This means that there is no locking involved. The thread local storage buffers are flushed to the
// socket and sent across the network once every 30ms.

// Summary: Recording and sending a scope consists of grabbing the scope start and end time and copying
// those to a TLS buffer along with the name id (typically the string literal pointer) - and that's it!
// No locking, no hashmap lookups, minimal overhead.


// Definitions
// -----------
// string literal:		Normal C++ definition. A hard coded string value, for example "my_string"
// dynamic string:		A string built at run-time, for example std::string my_string = string1 + string2;
// Scope:				Normal meaning in the C++ context. A FramePro Scope is a struct that has a start/end time
//						and a name and appears in the thread and core views.
// Session:				A single profile. This can be either connected and the session data is sent to FramePro in realtime,
//						or the session can be recorded to a file.
// Recording:			The session data is written out to a file instead of being sent to FramePro over
//						the network.
// Conditional scope:	A scope that is only sent if the duration is above a threshold
// Dynamic scope:		A scope that takes a dynamic string as an argument (most macros only take string literals)


//--------------------------------------------------------------------------------------------------------
// Session Macros
//
// Session macros are macros that give general information about the session and threads, and
// start/stop recording sessions. The only macros that is required by FramePro is the FRAMEPRO_FRAME_START.
// It is also advised to set the thread name for each thread which is created.


// FRAMEPRO_FRAME_START()
// ----------------------
// This must be called at the start of each frame. It tells FramePro when the old frame ends and the new frame starts


// FRAMEPRO_SHUTDOWN()
// -------------------
// Cleans up all FramePro resources. No thread can call into FramePro after this is called.


// FRAMEPRO_SET_SESSION_INFO(name, build_id)
// -----------------------------------------
// name: string - name of the session
// build_id: string - build id of the build being profiled
// The session name is typically the game name. The build can be anything. These values are shown in the session info panel in FramePro.


// FRAMEPRO_SET_ALLOCATOR(p_allocator)
// -----------------------------------
// p_allocator: FramePro::Allocator*
// Set the allocator that FramePro will use to allocate memory. This must be a class inherited from the FramePro::Allocator base class.
// The allocator class that you provide must be thread safe.
// NOTE: Call this before calling any other FramePro macro.


// FRAMEPRO_SET_THREAD_NAME(name)
// -----------------------------------------
// name: string (literal or dynamic)
// Tell FramePro the name of the current thread.
// FramePro will pick a colour based on the thread name which will always stay the same.


// FRAMEPRO_THREAD_ORDER(thread_name)
// ----------------------------------
// thread_name: string (literal or dynamic)
// To keep the order of the threads in the thread view consistent for all users you can specify the thread ordering
// from your game. The threads will appear in the order that the macros are called. For example:
//		FRAMEPRO_THREAD_ORDER("MainThread");
//		FRAMEPRO_THREAD_ORDER("RenderTHread");
//		FRAMEPRO_THREAD_ORDER("WorkerThread1");
//		FRAMEPRO_THREAD_ORDER("WorkerThread2");
//		FRAMEPRO_THREAD_ORDER("WorkerThread3");
//		FRAMEPRO_THREAD_ORDER("WorkerThread4");


// FRAMEPRO_REGISTER_STRING(str)
// -----------------------------
// str: string (dynamic)
// returns: FramePro::StringId
// Passing in dynamic strings to scope macros is costly because the string has to be looked up ni a hashmap to
// determine if it needs to send the string value to FramePro. for dynamic strings that never change (for example
// the name of an entity) you can cache off the string id (in the entity) using this macro and pass that in to the
// scope instead of the string.


// FRAMEPRO_START_RECORDING(filename, context_switches, max_file_size)
// -------------------------------------------------------------------
// filename: string - the filename to write the recording out to. Extension should be .framepro_recording
// context_switches: bool - whether to record context switches
// max_file_size: stop recording once the file reaches this size
// Instead of connecting to your game with FramePro you can instead write all of the network data out to a file.
// This recording file can then be loaded in FramePro. This can be useful if you want to start/stop a recording
// at a specific point in code, or if you want to automate the profiling session capture.
// If a recording was already started that recording will be stopped and this recording will be started.
// Connections from FramePro are disabled while a recording is in progress.


// FRAMEPRO_STOP_RECORDING()
// -------------------------
// Stop a recording started with FRAMEPRO_START_RECORDING.


// FRAMEPRO_REGISTER_CONNECTION_CHANGED_CALLBACK(callback, context)
// ----------------------------------------------------------------
// callback: ConnectionChangedCallback function pointer
// context: void*
// Add a callback to be called when a connection is made or lost.
// context is passed in to the callback as user_data.
// The callback is of the form:
//		typedef void (*ConnectionChangedCallback)(bool connected, void* user_data);
// If already connected the callback will be called immediately.
// Multiple callbacks can be registered.


// FRAMEPRO_UNREGISTER_CONNECTION_CHANGED_CALLBACK(callback)
// ---------------------------------------------------------
// Unregister a callback that was registered with FRAMEPRO_REGISTER_CONNECTION_CHANGED_CALLBACK


// FRAMEPRO_SET_THREAD_PRIORITY(priority)
// --------------------------------------
// priority: int
// Tell FramePro the priority of a thread. This will be displayed in the thread information (hover over the thread panel)
// It is often useful to be able to see the thread priorities in FramePro when looking at core usage.


// FRAMEPRO_SET_THREAD_AFFINITY(affinity)
// --------------------------------------
// affinity: int
// Tell FramePro the affinity of a thread. This will be displayed in the thread information (hover over the thread panel)
// It is often useful to be able to see the thread priorities in FramePro when looking at core usage.


// FRAMEPRO_UNBLOCK_SOCKETS()
// --------------------------
// As a security measure you can define FRAMEPRO_SOCKETS_BLOCKED_BY_DEFAULT to true. This will disable all network
// access by default. You can then call FRAMEPRO_UNBLOCK_SOCKETS() to explicitly allow connections.


// FRAMEPRO_CLEANUP_THREAD()
// --------------------------
// Call from a thread just before it exits to free the memory allocated for this thread. Do not call any other
// FramePro macros from the thread after calling this. FRAMEPRO_SHUTDOWN will automatically call this on all
// threads, so usually you don't need to call this. It is only necessary for threads that are created and destroyed.
// Note: Make sure that you don't call this inside a FramePro scope.
// during the session.


// FRAMEPRO_THREAD_SCOPE(thread_name)
// ----------------------------------
// thraed_name: string (literal or dynamic)
// Calls FRAMEPRO_SET_THREAD_NAME at the start of the scope, and FRAMEPRO_CLEANUP_THREAD at the end of the scope.
// Ensure that this is called outside of any other scopes.


// FRAMEPRO_LOG(message)
// ---------------------
// message: string (literal or dynamic)
// Send a log message to FramePro. All log messages are timestamped.





//--------------------------------------------------------------------------------------------------------
// scope macros


// FRAMEPRO_SCOPE()
// ----------------
// Starts a timer immediately and stops it when the current scope ends. The scope name will be the function name.
// This is typically the most used scope. Put it at the start of a function.
// This macro has minimal overhead.


// FRAMEPRO_NAMED_SCOPE(name)
// --------------------------
// name: string - NOTE: MUST be string literal
// Starts a timer immediately and stops it when the current scope ends. This macro is used for timing any scope
// and allows you to explicitly set the scope name.
// This macro has minimal overhead.


// FRAMEPRO_NAMED_SCOPE_W(name)
// ----------------------------
// name: wide string - NOTE: MUST be string literal
// Starts a timer immediately and stops it when the current scope ends. This macro is used for timing any scope
// and allows you to explicitly set the scope name.
// This macro has minimal overhead.


// FRAMEPRO_ID_SCOPE(name_id)
// --------------------------
// name_id: FramePro::StringId
// Starts a timer immediately and stops it when the current scope ends. This macro is used for timing any scope
// and allows you to explicitly set the scope name. The name_id is a FramePro string is returned from FRAMEPRO_REGISTER_STRING
// This macro has minimal overhead.


// FRAMEPRO_DYNAMIC_SCOPE(dynamic_string)
// --------------------------------------
// dynamic_string: dynamic string
// Starts a timer immediately and stops it when the current scope ends. This macro is used for timing any scope
// and allows you to explicitly set the scope name.
// This macro has medium overhead beacuse it has to create a hash of the string and look it up on a hash map.
// Only use where absolutely necessary. In general, try and use the string literal or string id versions.


// FRAMEPRO_CONDITIONAL_SCOPE()
// ----------------------------
// Starts a timer immediately and stops it when the current scope ends. The scope name will be the function name.
// Put it at the start of a function. The data will only be sent to FramePro if the scope time is greater than the
// conditional scope threshold. The threshold is set in FramePro using the threshold slider and defaults to 50ms.
// Conditional scopes scan be useful for scopes that would be too numerous if always sent, but occasionally spike.


// FRAMEPRO_CONDITIONAL_ID_SCOPE(name)
// -----------------------------------
// name: FramePro::StringId
// Starts a timer immediately and stops it when the current scope ends. The name of the scope is set explicitly.
// The data will only be sent to FramePro if the scope time is greater than the conditional scope threshold. The
// threshold is set in FramePro using the threshold slider and defaults to 50ms.
// Conditional scopes scan be useful for scopes that would be too numerous if always sent, but occasionally spike.


// FRAMEPRO_CONDITIONAL_NAMED_SCOPE(name)
// --------------------------------------
// name: string - NOTE: MUST be string literal
// Starts a timer immediately and stops it when the current scope ends. The name of the scope is set explicitly.
// The data will only be sent to FramePro if the scope time is greater than the conditional scope threshold. The
// threshold is set in FramePro using the threshold slider and defaults to 50ms.
// Conditional scopes scan be useful for scopes that would be too numerous if always sent, but occasionally spike.


// FRAMEPRO_CONDITIONAL_NAMED_SCOPE_W(name)
// --------------------------------------
// name: string - NOTE: MUST be string literal
// Starts a timer immediately and stops it when the current scope ends. The name of the scope is set explicitly.
// The data will only be sent to FramePro if the scope time is greater than the conditional scope threshold. The
// threshold is set in FramePro using the threshold slider and defaults to 50ms.
// Conditional scopes scan be useful for scopes that would be too numerous if always sent, but occasionally spike.


// FRAMEPRO_CONDITIONAL_BOOL_SCOPE(b)
// ----------------------------------
// b: bool - whether to send the scope or not
// Starts a timer immediately and stops it when the current scope ends. The scope name will be the function name.
// Put it at the start of a function. The data will only be sent to FramePro if the scope time is greater than the
// conditional scope threshold. The threshold is set in FramePro using the threshold slider and defaults to 50ms.
// Conditional scopes scan be useful for scopes that would be too numerous if always sent, but occasionally spike.


// FRAMEPRO_CONDITIONAL_BOOL_ID_SCOPE(name, b)
// -------------------------------------------
// b: bool - whether to send the scope or not
// name: FramePro::StringId
// Starts a timer immediately and stops it when the current scope ends. The name of the scope is set explicitly.
// The data will only be sent to FramePro if the scope time is greater than the conditional scope threshold. The
// threshold is set in FramePro using the threshold slider and defaults to 50ms.
// Conditional scopes scan be useful for scopes that would be too numerous if always sent, but occasionally spike.


// FRAMEPRO_CONDITIONAL_BOOL_NAMED_SCOPE(name, b)
// ----------------------------------------------
// b: bool - whether to send the scope or not
// name: string - NOTE: MUST be string literal
// Starts a timer immediately and stops it when the current scope ends. The name of the scope is set explicitly.
// The data will only be sent to FramePro if the scope time is greater than the conditional scope threshold. The
// threshold is set in FramePro using the threshold slider and defaults to 50ms.
// Conditional scopes scan be useful for scopes that would be too numerous if always sent, but occasionally spike.


// FRAMEPRO_CONDITIONAL_NAMED_BOOL_SCOPE_W(name, b)
// ------------------------------------------------
// b: bool - whether to send the scope or not
// name: string - NOTE: MUST be string literal
// Starts a timer immediately and stops it when the current scope ends. The name of the scope is set explicitly.
// The data will only be sent to FramePro if the scope time is greater than the conditional scope threshold. The
// threshold is set in FramePro using the threshold slider and defaults to 50ms.
// Conditional scopes scan be useful for scopes that would be too numerous if always sent, but occasionally spike.


// FRAMEPRO_START_NAMED_SCOPE(name)
// --------------------------------------
// name: string literal without quotes - see comment
// name: string - NOTE: MUST be string literal
// Start a named scope. Creates a local variable which stores the start time of the scope.
// This must be matched by a call to FRAMEPRO_STOP_NAMED_SCOPE with the same name passed in.
// In general, it is preferable to use the scope marcos that automatically close the scope on exit, but
// there are occasions when you need to start/sto explicitly. Be careful not to overlap scopes, and be careful
// to always end the scope that you started. Otherwise you will see strange behavour in the thread stack view
// in FramePro.
// The name passed in is a string literal without the quotes, for example FRAMEPRO_START_NAMED_SCOPE(my_scope).
// This will create a local variable using called framepro_start_my_scope and send the scope using the "my_scope"
// string literal.


// FRAMEPRO_STOP_NAMED_SCOPE(name)
// -------------------------------
// name: string literal without quotes - this must be the same name that was passed in to FRAMEPRO_START_NAMED_SCOPE
// Stop a scope started with FRAMEPRO_START_NAMED_SCOPE


// FRAMEPRO_CONDITIONAL_START_SCOPE()
// ----------------------------------
// Starts a conditional scope. Note that you can only start one conditional scope per scope.
// Creates a local variable which stores the scope start time.


// FRAMEPRO_CONDITIONAL_STOP_NAMED_SCOPE(name)
// -------------------------------------------
// name: string - NOTE: MUST be string literal
// Stop a scope started with FRAMEPRO_CONDITIONAL_START_SCOPE. The name passed in is the name of the scope.
// The data will only be sent to FramePro if the scope time is greater than the conditional scope threshold. The
// threshold is set in FramePro using the threshold slider and defaults to 50ms.
// Conditional scopes scan be useful for scopes that would be too numerous if always sent, but occasionally spike.


// FRAMEPRO_CONDITIONAL_STOP_DYNAMIC_SCOPE(dynamic_string)
// -------------------------------------------------------
// name: string - dynamic string
// Stop a scope started with FRAMEPRO_CONDITIONAL_START_SCOPE. The name passed in is the name of the scope.
// The data will only be sent to FramePro if the scope time is greater than the conditional scope threshold. The
// threshold is set in FramePro using the threshold slider and defaults to 50ms.
// Conditional scopes scan be useful for scopes that would be too numerous if always sent, but occasionally spike.
// This scope is particularly useful for dynamic strings that are slow to evaluate. The string will only be
// evaluated if the scope time exceeds the threshold.
// For example: FRAMEPRO_CONDITIONAL_STOP_DYNAMIC_SCOPE(GetEntityName())
// Where GetEntityName() is too expensive to call all of the time, but if the scopes takes > 50ms it is worth
// calling it so that the name shows up in FramePro.


// FRAMEPRO_CONDITIONAL_PARENT_SCOPE(name, callback, pre_duration, post_duration)
// ------------------------------------------------------------------------------
// name: string literal - unique name of the parent
// callback: FramePro::ConditionalParentScopeCallback - called at the end of the parent scope. Return true form the callback to send the child scopes.
// pre_duration: int64 - how long to keep the child scopes if the callback returns false (in micro-sec). When the callback returns true all of these scopes will be sent.
// post_duration: int64 - how long to keep sending the child scopes after the callback has returned false (in micro-sec)
// The callback is called at the end of the scope. If the callback returns false
// the child scopes are not sent, instead they are added to a list. The
// child scopes are kept on the list for pre_duration us. If the
// callback reutrns true all of the child scopes for this frame and previous frames are sent,
// and the child scopes are continued to be sent for post_duration after the callback returns false.
// Note that nesting conditional parent scopes is not supported.


//--------------------------------------------------------------------------------------------------------
// idle scope macros

// All scope macros have an idle scope equivalent. An idle scope shows up as semi-transparent in FramePro.
// Separate macros were created to avoid having to add the overhead of an extra conditional to each scope.
// Idle scopes are typically used in Sleep or Event wait calls.


// FRAMEPRO_IDLE_SCOPE()
// ---------------------
// see FRAMEPRO_SCOPE

// FRAMEPRO_IDLE_NAMED_SCOPE(name)
// -------------------------------
// see FRAMEPRO_NAMED_SCOPE

// FRAMEPRO_IDLE_NAMED_SCOPE_W(name)
// ---------------------------------
// see FRAMEPRO_NAMED_SCOPE_W

// FRAMEPRO_IDLE_ID_SCOPE(name_id)
// -------------------------------
// see FRAMEPRO_ID_SCOPE

// FRAMEPRO_IDLE_DYNAMIC_SCOPE(dynamic_string)
// -------------------------------------------
// see FRAMEPRO_DYNAMIC_SCOPE

// FRAMEPRO_IDLE_CONDITIONAL_SCOPE()
// ---------------------------------
// see FRAMEPRO_CONDITIONAL_SCOPE

// FRAMEPRO_IDLE_CONDITIONAL_ID_SCOPE()
// ---------------------------------
// see FRAMEPRO_CONDITIONAL_SCOPE_ID

// FRAMEPRO_IDLE_CONDITIONAL_NAMED_SCOPE(name)
// -------------------------------------------
// see FRAMEPRO_CONDITIONAL_NAMED_SCOPE

// FRAMEPRO_IDLE_CONDITIONAL_NAMED_SCOPE_W(name)
// ---------------------------------------------
// see FRAMEPRO_CONDITIONAL_NAMED_SCOPE_W

// FRAMEPRO_IDLE_START_NAMED_SCOPE(name)
// -------------------------------------
// see FRAMEPRO_START_NAMED_SCOPE

// FRAMEPRO_IDLE_STOP_NAMED_SCOPE(name)
// ------------------------------------
// see FRAMEPRO_STOP_NAMED_SCOPE

// FRAMEPRO_IDLE_CONDITIONAL_START_SCOPE()
// ---------------------------------------
// see FRAMEPRO_CONDITIONAL_START_SCOPE

// FRAMEPRO_IDLE_CONDITIONAL_STOP_NAMED_SCOPE(name)
// ------------------------------------------------
// see FRAMEPRO_CONDITIONAL_STOP_NAMED_SCOPE

// FRAMEPRO_IDLE_CONDITIONAL_STOP_DYNAMIC_SCOPE(dynamic_string)
// ------------------------------------------------------------
// see FRAMEPRO_CONDITIONAL_STOP_DYNAMIC_SCOPE


//--------------------------------------------------------------------------------------------------------
// Custom Stat macros

// Custom Stats are zero'd at the start of the frame and accumulated during the frame. Each stat has an accumulated value
// and a count.The count is the number of times the stat was added to in a frame. Custom stats are keyed off the name
// value, which must be unique.The "graph" and "utils" values are only read on the first call for a stat and must stay
// the same thereafter. Graph is the graph name to display the stats. Multipole stats can be put into the same graph.
// The Units will show in the y-axis.

// FRAMEPRO_CUSTOM_STAT(name, value, graph, unit)
// -----------------------------------------------
// name: string literal					- the name of the custom stat
// value: int, int64, float, double		- value to add to the custom stat
// graph: string literal				- the name of the graph in which to display the stat
// unit: string literal				- y-axis unit to be displayed in FramePro, eg "ms", "MB" etc.
// Adds value to the custom stat with the specified name

// FRAMEPRO_DYNAMIC_CUSTOM_STAT(name, value, graph, unit)
// -----------------------------------------------
// name: dynamic string					- the name of the custom stat
// value: int, int64, float, double		- value to add to the custom stat
// graph: string literal				- the name of the graph in which to display the stat
// unit: string literal				- y-axis unit to be displayed in FramePro, eg "ms", "MB" etc.
// Adds value to the custom stat with the specified name

// FRAMEPRO_SCOPE_CUSTOM_STAT(name, value)
// ---------------------------------------
// name: string literal or StringId - the name of the custom stat
// value: int64 - the stat value
// Add a custom stat to the current scope. This will be displayed in the scope hover box and in the frame graph view


//--------------------------------------------------------------------------------------------------------
// hires timers

// FRAMEPRO_HIRES_SCOPE(name)
// --------------------------
// name: string literal
// Adds a hi-res timer for the current scope. Use hires timers as a low overhead timer for events that
// happen at a high frequency.
//
// Hires timers for a scope will be shown below that scope as a percentage of the total scope time. Start
// and end times are not recorded for hires timers, instead the total time and call count are accumulated
// for the current scope.
//
// For example, hires timer1 accounted for 60% of scope1 and was called 460 times. hires2 accounted
// for 10% of the time. The other 30% of time is spent in scope1.
// |---------------------------------------------------------------------------------------|
// |                                        scope1                                         |
// |---------------------------------------------------------------------------------------|
// |             hires1 60% (count: 460)                  |  hires2 10% (count: 80) |
// |--------------------------------------------------------------------------------|
//
// Hires timers are useful when you have something that you want to measure which happens at a very high
// frequency, such as memory allocations, string operations or updates for lots of entities. Because these
// sort of events happen so many times during a frame, adding normal scopes would have too much overhead
// and make the thread graph hard to read. Hires timers have minimal overhead and are clear and easy to see
// in the thread graph.



//--------------------------------------------------------------------------------------------------------
// global hires timers

// Global high resolution timers are used when you want to time scopes that happen many thousands of times a frame
// and don't want the overhead of recording a timespan for each scope. The scope simple adds to a global
// value which stores total duration and count, and then sends that value once a frame. The timer is reset
// at the start of each frame.

// Usage:
//		// this defines a global timer with name "my_timer" which will be shown in the "hires timers" graph in FramePro
//		FRAMEPRO_DECL_GLOBAL_HIRES_TIMER(my_timer, "hires timers")
//
//		void MyFunction()
//		{
//			FRAMEPRO_HIRES_SCOPE(my_timer)
//			...
//		}

// FRAMEPRO_DECL_GLOBAL_HIRES_TIMER(name, graph)
// ---------------------------------------------
// name: unquoted string		the name of the global timer object
// graph: string literal		the name of the graph in which to display the timer
// Declares a high resolution timer with the specified name.

// FRAMEPRO_GLOBAL_HIRES_SCOPE(name)
// ---------------------------------
// name: unquoted string		the name of the global timer declared with FRAMEPRO_GLOBAL_HIRES_SCOPE
// Times the current scope and adds the value to the specified high-res timer. The high-res timer must
// have already been declared with FRAMEPRO_GLOBAL_HIRES_SCOPE.


//--------------------------------------------------------------------------------------------------------
// events

// FRAMEPRO_EVENT(name)
// --------------------
// name: string literal
// Add a named event which will be added to the timeilne.


//--------------------------------------------------------------------------------------------------------
// wait events

// Wait Events are not to be confused with Events. Wait Events are for measuring mutex or critical section
// locks. Use these macros to record how long your lock is held for, and who unlocked the event. Wait
// events will appear in the Cores View.

// FRAMEPRO_WAIT_EVENT_SCOPE(event_id)
// -----------------------------------
// event_id: unique identifier for the event (must be castable to int64)
// The easiest thing to use for the event_id is the this pointer of your Event or critical section class.
// Measure how long a thread waits on an event. For Win32 Events put this around the WaitForSingleObject
// call. For Win32 critical sections put this around the EnterCriticalSection call. In order to not flood
// the channel with events, the wait event is only sent if it is longer than FRAMEPRO_WAIT_EVENT_MIN_TIME.
// FramePro will also only show a wait event if it was triggered with FRAMEPRO_STOP_WAIT_EVENT.

// FRAMEPRO_TRIGGER_WAIT_EVENT(event_id)
// ----------------------------------
// event_id: unique identifier for the event (must be castable to int64).
// The event_id MUST match the id passed in to FRAMEPRO_WAIT_EVENT_SCOPE.
// Use the FRAMEPRO_TRIGGER_WAIT_EVENT to tell FramePro that a wait event has been triggered. For Win32
// events call _before_ the SetEvent call. For Win32 critical sections call just before the LeaveCriticalSection.



//------------------------------------------------------------------------
//
//                        ---- FramePro private ----
//
// In general do not use anything below this point. Stuff is only here in order ot keep everything inlined.
// The macros cover everything that you should need. Also, sticking to the macros means that FramePro can be
// completely defined out in retail builds.

//------------------------------------------------------------------------
#if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
// enable this if you are linking to FRamePro.dll
//#define FRAMEPRO_DLL

//------------------------------------------------------------------------
#if defined(_MSC_VER) && _MSC_VER <= 1600
	#error FramePro only supports Visual Studio 2012 and above. This is because it needs atomics. If you really need 2010 support please contact slynch@puredevsoftware.com
#endif

//------------------------------------------------------------------------
#include <atomic>
#include <stdlib.h>

//------------------------------------------------------------------------
// enable debug here to get FramePro asserts
#if defined(DEBUG) || defined(_DEBUG)
	#define FRAMEPRO_DEBUG 1
#else
	#define FRAMEPRO_DEBUG 0
#endif

//------------------------------------------------------------------------
#ifdef __UNREAL__
	#define FRAMEPRO_TOOLSET_UE4 1
#else
	#define FRAMEPRO_TOOLSET_UE4 0
#endif

//------------------------------------------------------------------------
#if FRAMEPRO_TOOLSET_UE4
	#define FRAMEPRO_API CORE_API
#elif defined(FRAMEPRO_DLL_EXPORT)
	#define FRAMEPRO_API __declspec(dllexport)
#elif defined(FRAMEPRO_DLL)
	#define FRAMEPRO_API __declspec(dllimport)
#else
	#define FRAMEPRO_API
#endif

//------------------------------------------------------------------------
#define FRAMEPRO_STRINGIZE(x) FRAMEPRO_STRINGIZE2(x)
#define FRAMEPRO_STRINGIZE2(x) #x

//------------------------------------------------------------------------
#define FRAMEPRO_JOIN(a, b) FRAMEPRO_JOIN2(a, b)
#define FRAMEPRO_JOIN2(a, b) a##b
#define FRAMEPRO_UNIQUE(a) FRAMEPRO_JOIN(a, __LINE__)

//------------------------------------------------------------------------
#define FRAMEPRO_WIDESTR(s) FRAMEPRO_WIDESTR2(s)
#define FRAMEPRO_WIDESTR2(s) L##s

//------------------------------------------------------------------------
// START EPIC
#define FRAMEPRO_PLATFORM_XBOXONE 	PLATFORM_XBOXONE
#define FRAMEPRO_PLATFORM_XBOX360 	0
#define FRAMEPRO_PLATFORM_UWP 		0
#define FRAMEPRO_PLATFORM_WIN 		PLATFORM_WINDOWS
#define FRAMEPRO_PLATFORM_UNIX 		PLATFORM_LINUX
#define FRAMEPRO_PLATFORM_ANDROID 	PLATFORM_ANDROID
#define FRAMEPRO_PLATFORM_IOS 		PLATFORM_IOS
#define FRAMEPRO_PLATFORM_SWITCH	PLATFORM_SWITCH
// END EPIC

// START EPIC
#if FRAMEPRO_PLATFORM_WIN || FRAMEPRO_PLATFORM_UWP || FRAMEPRO_PLATFORM_XBOXONE
	#define FRAMEPRO_WIN_BASED_PLATFORM 1
#else
	#define FRAMEPRO_WIN_BASED_PLATFORM 0
#endif

#if FRAMEPRO_PLATFORM_SWITCH //#TODO: could add others here...
	#define FRAMEPRO_UE4_BASED_PLATFORM 1
#else
	#define FRAMEPRO_UE4_BASED_PLATFORM 0
#endif
// END EPIC

//------------------------------------------------------------------------
#if FRAMEPRO_WIN_BASED_PLATFORM && !defined(__clang__)
	#define FRAMEPRO_FUNCTION_DEFINE_IS_STRING_LITERAL 1
#else
	#define FRAMEPRO_FUNCTION_DEFINE_IS_STRING_LITERAL 0
#endif

//------------------------------------------------------------------------
// On non-windows based platforms __FUNCTION__ can't be concatonated with
// other string literals so we need to pass in the name separately.
#if !FRAMEPRO_FUNCTION_DEFINE_IS_STRING_LITERAL
	#ifdef FRAMEPRO_SCOPE
		#undef FRAMEPRO_SCOPE
		#define FRAMEPRO_SCOPE() 					FramePro::StringLiteralNamedTimerScope FRAMEPRO_UNIQUE(timer_scope)(__FUNCTION__, FRAMEPRO_SOURCE_STRING)
	#endif
	#ifdef FRAMEPRO_CONDITIONAL_SCOPE
		#undef FRAMEPRO_CONDITIONAL_SCOPE
		#define FRAMEPRO_CONDITIONAL_SCOPE()		FramePro::StringLiteralNamedConditionalTimerScope FRAMEPRO_UNIQUE(timer_scope)(__FUNCTION__, FRAMEPRO_SOURCE_STRING)
	#endif
	#ifdef FRAMEPRO_IDLE_SCOPE
		#undef FRAMEPRO_IDLE_SCOPE
		#define FRAMEPRO_IDLE_SCOPE()				FramePro::StringLiteralNamedTimerScope FRAMEPRO_UNIQUE(timer_scope)(__FUNCTION__, FRAMEPRO_SOURCE_STRING_IDLE)
	#endif
	#ifdef FRAMEPRO_IDLE_CONDITIONAL_SCOPE
		#undef FRAMEPRO_IDLE_CONDITIONAL_SCOPE
		#define FRAMEPRO_IDLE_CONDITIONAL_SCOPE()	FramePro::StringLiteralNamedConditionalTimerScope FRAMEPRO_UNIQUE(timer_scope)(__FUNCTION__, FRAMEPRO_SOURCE_STRING_IDLE)
	#endif
#endif

//------------------------------------------------------------------------
#if FRAMEPRO_WIN_BASED_PLATFORM
	#define MULTI_STATEMENT(s) do { s } while(true,false)
#else
	#define MULTI_STATEMENT(s) do { s } while(false)
#endif

//------------------------------------------------------------------------
#if FRAMEPRO_FUNCTION_DEFINE_IS_STRING_LITERAL
	#define FRAMEPRO_FUNCTION_NAME __FUNCTION__
#else
	#define FRAMEPRO_FUNCTION_NAME ""		// __FUNCTION__ is not a string literal on Unix platforms so we can't combine it with other string literals
#endif

//------------------------------------------------------------------------
#define FRAMEPRO_SOURCE_STRING			__FILE__ "|" FRAMEPRO_FUNCTION_NAME "|" FRAMEPRO_STRINGIZE(__LINE__) "|"
#define FRAMEPRO_SOURCE_STRING_W		FRAMEPRO_WIDESTR(__FILE__) L"|" FRAMEPRO_WIDESTR(FRAMEPRO_FUNCTION_NAME) L"|" FRAMEPRO_WIDESTR(FRAMEPRO_STRINGIZE(__LINE__)) L"|"
#define FRAMEPRO_SOURCE_STRING_IDLE		__FILE__ "|" FRAMEPRO_FUNCTION_NAME "|" FRAMEPRO_STRINGIZE(__LINE__) "|Idle"
#define FRAMEPRO_SOURCE_STRING_IDLE_W	FRAMEPRO_WIDESTR(__FILE__) L"|" FRAMEPRO_WIDESTR(FRAMEPRO_FUNCTION_NAME) L"|" FRAMEPRO_WIDESTR(FRAMEPRO_STRINGIZE(__LINE__)) L"|Idle"

//------------------------------------------------------------------------
// START EPIC
#define FRAMEPRO_BREAK() UE_DEBUG_BREAK()
// END EPIC

//------------------------------------------------------------------------
#if FRAMEPRO_DEBUG
	#define FRAMEPRO_ASSERT(b) if(!(b)) FRAMEPRO_BREAK()
#else
	#define FRAMEPRO_ASSERT(b) ((void)0)
#endif

//------------------------------------------------------------------------
// START EPIC
// Remove UNIX inttypes.h and sys/signal.h includes
//------------------------------------------------------------------------
// typedefs
namespace FramePro
{
	typedef uint32 uint;
}
// END EPIC

#if FRAMEPRO_PLATFORM_WIN
	#define FRAMEPRO_EVENT_TRACE_WIN32 1
#else
	#define FRAMEPRO_EVENT_TRACE_WIN32 0
#endif

//------------------------------------------------------------------------
// port
#if FRAMEPRO_PLATFORM_WIN || FRAMEPRO_PLATFORM_UWP
	#define FRAMEPRO_PORT "8428"
#elif FRAMEPRO_PLATFORM_XBOXONE
	#define FRAMEPRO_PORT "4420"
// START EPIC
#elif FRAMEPRO_PLATFORM_UNIX || FRAMEPRO_PLATFORM_ANDROID || FRAMEPRO_PLATFORM_IOS || FRAMEPRO_PLATFORM_SWITCH
// END EPIC
	#define FRAMEPRO_PORT "8428"
#else
	#error FramePro platform not defined
#endif

//------------------------------------------------------------------------
#define FRAMEPRO_DEFAULT_COND_SCOPE_MIN_TIME 50

//------------------------------------------------------------------------
// START EPIC
// Remove FRAMEPRO_TIMER_CLOCK_GETTIME
// END EPIC
// the macro to get the current clock time
namespace FramePro
{
// START EPIC
#define FRAMEPRO_GET_CLOCK_COUNT(time) \
	time = FPlatformTime::Cycles64()
// END EPIC
}

//------------------------------------------------------------------------
namespace FramePro
{
	template<typename T>
	class RelaxedAtomic
	{
	public:
		RelaxedAtomic() { static_assert(sizeof(T) <= sizeof(size_t), "bad template arg"); FRAMEPRO_ASSERT((((FramePro::uint64)this) % sizeof(T) == 0)); }
		RelaxedAtomic(T value) : m_Value(value) { static_assert(sizeof(T) <= sizeof(size_t), "bad template arg"); FRAMEPRO_ASSERT((((FramePro::uint64)this) % sizeof(T) == 0)); }
		void operator=(T value) { m_Value = value; }
		operator T() const { return m_Value; }
	private:
		T m_Value;
	};
}

//------------------------------------------------------------------------
// the main FramePro interface
namespace FramePro
{
	// callbacks
	typedef void(*ConnectionChangedCallback)(bool connected, void* user_data);
	typedef bool(*ConditionalParentScopeCallback)(const char* p_name, int64 start_time, int64 end_time, int64 ticks_per_second);

	FRAMEPRO_API extern RelaxedAtomic<bool> g_Connected;
	FRAMEPRO_API extern RelaxedAtomic<unsigned int> g_ConditionalScopeMinTime;

// START EPIC
// Remove FRAMEPRO_TIMER_QUERY_PERFORMANCE_COUNTER and FRAMEPRO_WIN_BASED_PLATFORM
// END EPIC

	FRAMEPRO_API void Shutdown();

	FRAMEPRO_API void SetPort(int port);

	FRAMEPRO_API void SendSessionInfo(const char* p_name, const char* p_build_id);

	FRAMEPRO_API void SendSessionInfo(const wchar_t* p_name, const wchar_t* p_build_id);

	FRAMEPRO_API void SetAllocator(class Allocator* p_allocator);		// if you call this you must call it BEFORE any other calls

	FRAMEPRO_API inline bool IsConnected() { return g_Connected; }

	FRAMEPRO_API inline unsigned int GetConditionalScopeMinTime() { return g_ConditionalScopeMinTime; }

	FRAMEPRO_API void FrameStart();		// call at the start of each of your frames

	FRAMEPRO_API void AddTimeSpan(const char* p_name_file_and_line, int64 start_time, int64 end_time);

	FRAMEPRO_API void AddTimeSpan(const wchar_t* p_name_file_and_line, int64 start_time, int64 end_time);

	FRAMEPRO_API void AddTimeSpan(const char* p_name, const char* p_file_and_line, int64 start_time, int64 end_time);

	FRAMEPRO_API void AddTimeSpan(StringId name_id, const char* p_file_and_line, int64 start_time, int64 end_time);

	FRAMEPRO_API void AddTimeSpan(StringId name_id, const char* p_file_and_line, int64 start_time, int64 end_time, int thread_id, int core);

	FRAMEPRO_API void AddCustomStat(const char* p_name, int value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void AddCustomStat(const char* p_name, int64 value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void AddCustomStat(const char* p_name, float value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void AddCustomStat(const char* p_name, double value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void AddCustomStat(const wchar_t* p_name, int value, const wchar_t* p_graph, const wchar_t* p_unit);

	FRAMEPRO_API void AddCustomStat(const wchar_t* p_name, int64 value, const wchar_t* p_graph, const wchar_t* p_unit);

	FRAMEPRO_API void AddCustomStat(const wchar_t* p_name, float value, const wchar_t* p_graph, const wchar_t* p_unit);

	FRAMEPRO_API void AddCustomStat(const wchar_t* p_name, double value, const wchar_t* p_graph, const wchar_t* p_unit);

	FRAMEPRO_API void AddCustomStat(StringId name, int value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void AddCustomStat(StringId name, int64 value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void AddCustomStat(StringId name, float value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void AddCustomStat(StringId name, double value, const char* p_graph, const char* p_unit);

	// START EPIC 
	FRAMEPRO_API void AddCustomStat(StringId name, int value, const wchar_t* p_graph, const wchar_t* p_unit);

	FRAMEPRO_API void AddCustomStat(StringId name, int64 value, const wchar_t* p_graph, const wchar_t* p_unit);

	FRAMEPRO_API void AddCustomStat(StringId name, float value, const wchar_t* p_graph, const wchar_t* p_unit);

	FRAMEPRO_API void AddCustomStat(StringId name, double value, const wchar_t* p_graph, const wchar_t* p_unit);
	// END EPIC

	FRAMEPRO_API void AddEvent(const char* p_name, uint colour);

	FRAMEPRO_API void AddWaitEvent(int64 event_id, int64 start_time, int64 end_time);

	FRAMEPRO_API void TriggerWaitEvent(int64 event_id);

	FRAMEPRO_API void SetThreadName(const char* p_name);

	FRAMEPRO_API void SetThreadOrder(StringId thread_name);

	FRAMEPRO_API StringId RegisterString(const char* p_str);

	FRAMEPRO_API StringId RegisterString(const wchar_t* p_str);

	FRAMEPRO_API void RegisterConnectionChangedCallback(ConnectionChangedCallback p_callback, void* p_context);

	FRAMEPRO_API void UnregisterConnectionChangedcallback(ConnectionChangedCallback p_callback);

// START EPIC
	FRAMEPRO_API void StartRecording(const FString& p_filename, bool context_switches, int64 max_file_size);
// END EPIC

	FRAMEPRO_API void StopRecording();

	FRAMEPRO_API void SetThreadPriority(int priority);

	FRAMEPRO_API void SetThreadAffinity(int affinity);

	FRAMEPRO_API void BlockSockets();

	FRAMEPRO_API void UnblockSockets();

	FRAMEPRO_API void AddGlobalHiResTimer(class GlobalHiResTimer* p_timer);

	FRAMEPRO_API void CleanupThread();

	FRAMEPRO_API void PushConditionalParentScope(const char* p_name, int64 pre_duration, int64 post_duration);		// durations are in us

	FRAMEPRO_API void PopConditionalParentScope(bool add_children);

	FRAMEPRO_API bool CallConditionalParentScopeCallback(ConditionalParentScopeCallback p_callback, const char* p_name, int64 start_time, int64 end_time);

	FRAMEPRO_API void StartHiResTimer(const char* p_name);

	FRAMEPRO_API void StopHiResTimer();

	FRAMEPRO_API void SubmitHiResTimers(int64 current_time);

	FRAMEPRO_API void Log(const char* p_message);

	FRAMEPRO_API void SetScopeCustomStat(const char* p_name, int value, const char* p_graph, const char* p_unit);
	
	FRAMEPRO_API void SetScopeCustomStat(const wchar_t* p_name, int value, const wchar_t* p_graph, const wchar_t* p_unit);

	FRAMEPRO_API void SetScopeCustomStat(StringId name, int value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void SetScopeCustomStat(const char* p_name, int64 value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void SetScopeCustomStat(const wchar_t* p_name, int64 value, const wchar_t* p_graph, const wchar_t* p_unit);

	FRAMEPRO_API void SetScopeCustomStat(StringId name, int64 value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void SetScopeCustomStat(const char* p_name, float value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void SetScopeCustomStat(const wchar_t* p_name, float value, const wchar_t* p_graph, const wchar_t* p_unit);

	FRAMEPRO_API void SetScopeCustomStat(StringId name, float value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void SetScopeCustomStat(const char* p_name, double value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void SetScopeCustomStat(const wchar_t* p_name, double value, const wchar_t* p_graph, const wchar_t* p_unit);

	FRAMEPRO_API void SetScopeCustomStat(StringId name, double value, const char* p_graph, const char* p_unit);

	FRAMEPRO_API void SetConditionalScopeMinTimeInMicroseconds(int64 value);
}

//------------------------------------------------------------------------
// TimerScope
namespace FramePro
{
	//------------------------------------------------------------------------
	class Allocator
	{
	public:
		virtual void* Alloc(size_t size) = 0;
		virtual void Free(void* p) = 0;
		virtual ~Allocator() {}
	};

	//------------------------------------------------------------------------
	class TimerScope
	{
	public:
		TimerScope(const char* p_name_and_source_info)
		:	mp_NameAndSourceInfo(p_name_and_source_info)
		{
			bool connected = FramePro::IsConnected();
			m_Connected = connected;

			int64 start_time;
			FRAMEPRO_GET_CLOCK_COUNT(start_time);
			if (connected)
				FramePro::SubmitHiResTimers(start_time);
			m_StartTime = start_time;
		}

		~TimerScope()
		{
			if(m_Connected)
			{
				int64 end_time = 0;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);
				FramePro::AddTimeSpan(mp_NameAndSourceInfo, m_StartTime, end_time);
			}
		}

	private:
		bool m_Connected;
		const char* mp_NameAndSourceInfo;
		int64 m_StartTime;
	};

	//------------------------------------------------------------------------
	class TimerScopeW
	{
	public:
		TimerScopeW(const wchar_t* p_name_and_source_info)
		:	mp_NameAndSourceInfo(p_name_and_source_info)
		{
			bool connected = FramePro::IsConnected();
			m_Connected = connected;

			int64 start_time;
			FRAMEPRO_GET_CLOCK_COUNT(start_time);
			if (connected)
				FramePro::SubmitHiResTimers(start_time);
			m_StartTime = start_time;
		}

		~TimerScopeW()
		{
			if(m_Connected)
			{
				int64 end_time = 0;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);
				FramePro::AddTimeSpan(mp_NameAndSourceInfo, m_StartTime, end_time);
			}
		}

	private:
		bool m_Connected;
		const wchar_t* mp_NameAndSourceInfo;
		int64 m_StartTime;
	};

	//------------------------------------------------------------------------
	class ConditionalParentTimerScope
	{
	public:
		ConditionalParentTimerScope(const char* p_name, const char* p_source_info, ConditionalParentScopeCallback p_callback, int64 pre_duration, int64 post_duration)
		:	mp_Name(p_name),
			mp_SourceInfo(p_source_info),
			m_StartTime(0),
			mp_Callback(p_callback)
		{
			bool connected = FramePro::IsConnected();
			m_Connected = connected;

			if (connected)
			{
				FramePro::PushConditionalParentScope(p_name, pre_duration, post_duration);

				int64 start_time;
				FRAMEPRO_GET_CLOCK_COUNT(start_time);
				if (connected)
					FramePro::SubmitHiResTimers(start_time);
				m_StartTime = start_time;
			}
		}

		~ConditionalParentTimerScope()
		{
			if (m_Connected)
			{
				int64 end_time = 0;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);

				bool add_children = FramePro::CallConditionalParentScopeCallback(mp_Callback, mp_Name, m_StartTime, end_time);
				FramePro::PopConditionalParentScope(add_children);

				FramePro::AddTimeSpan(mp_Name, mp_SourceInfo, m_StartTime, end_time);
			}
		}

	private:
		bool m_Connected;
		const char* mp_Name;
		const char* mp_SourceInfo;
		int64 m_StartTime;

		ConditionalParentScopeCallback mp_Callback;
	};

	//------------------------------------------------------------------------
	class IdTimerScope
	{
	public:
		IdTimerScope(StringId name, const char* p_source_info)
		:	m_Name(name),
			mp_SourceInfo(p_source_info)
		{
			bool connected = FramePro::IsConnected();
			m_Connected = connected;

			int64 start_time;
			FRAMEPRO_GET_CLOCK_COUNT(start_time);
			if (connected)
				FramePro::SubmitHiResTimers(start_time);
			m_StartTime = start_time;
		}

		~IdTimerScope()
		{
			if(m_Connected)
			{
				int64 end_time = 0;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);
				FramePro::AddTimeSpan(m_Name, mp_SourceInfo, m_StartTime, end_time);
			}
		}

	private:
		bool m_Connected;
		StringId m_Name;
		const char* mp_SourceInfo;
		int64 m_StartTime;
	};

	//------------------------------------------------------------------------
	class StringLiteralNamedTimerScope
	{
	public:
		// p_name must be a string literal
		StringLiteralNamedTimerScope(const char* p_name, const char* p_source_info)
		:	mp_Name(p_name),
			mp_SourceInfo(p_source_info)
		{
			bool connected = FramePro::IsConnected();
			m_Connected = connected;

			int64 start_time;
			FRAMEPRO_GET_CLOCK_COUNT(start_time);
			if (connected)
				FramePro::SubmitHiResTimers(start_time);
			m_StartTime = start_time;
		}

		~StringLiteralNamedTimerScope()
		{
			if(m_Connected)
			{
				int64 end_time = 0;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);
				FramePro::AddTimeSpan(mp_Name, mp_SourceInfo, m_StartTime, end_time);
			}
		}

	private:
		bool m_Connected;
		const char* mp_Name;
		const char* mp_SourceInfo;
		int64 m_StartTime;
	};

	//------------------------------------------------------------------------
	class ConditionalTimerScope
	{
	public:
		ConditionalTimerScope(const char* p_name_and_source_info)
		:	mp_NameAndSourceInfo(p_name_and_source_info)
		{
			bool connected = FramePro::IsConnected();
			m_Connected = connected;

			int64 start_time;
			FRAMEPRO_GET_CLOCK_COUNT(start_time);
			if (connected)
				FramePro::SubmitHiResTimers(start_time);
			m_StartTime = start_time;
		}

		~ConditionalTimerScope()
		{
			if(m_Connected)
			{
				int64 end_time = 0;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);
				if(end_time - m_StartTime > FramePro::GetConditionalScopeMinTime())
					FramePro::AddTimeSpan(mp_NameAndSourceInfo, m_StartTime, end_time);
			}
		}

	private:
		bool m_Connected;
		const char* mp_NameAndSourceInfo;
		int64 m_StartTime;
	};

	//------------------------------------------------------------------------
	class ConditionalTimerScopeId
	{
	public:
		ConditionalTimerScopeId(StringId name, const char* p_source_info)
		:	m_Name(name),
			mp_SourceInfo(p_source_info)
		{
			bool connected = FramePro::IsConnected();
			m_Connected = connected;

			int64 start_time;
			FRAMEPRO_GET_CLOCK_COUNT(start_time);
			if (connected)
				FramePro::SubmitHiResTimers(start_time);
			m_StartTime = start_time;
		}

		~ConditionalTimerScopeId()
		{
			if(m_Connected)
			{
				int64 end_time = 0;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);
				if(end_time - m_StartTime > FramePro::GetConditionalScopeMinTime())
					FramePro::AddTimeSpan(m_Name, mp_SourceInfo, m_StartTime, end_time);
			}
		}

	private:
		bool m_Connected;
		StringId m_Name;
		const char* mp_SourceInfo;
		int64 m_StartTime;
	};

	//------------------------------------------------------------------------
	class ConditionalTimerScopeW
	{
	public:
		ConditionalTimerScopeW(const wchar_t* p_name_and_source_info)
		:	mp_NameAndSourceInfo(p_name_and_source_info)
		{
			bool connected = FramePro::IsConnected();
			m_Connected = connected;

			int64 start_time;
			FRAMEPRO_GET_CLOCK_COUNT(start_time);
			if (connected)
				FramePro::SubmitHiResTimers(start_time);
			m_StartTime = start_time;
		}

		~ConditionalTimerScopeW()
		{
			if(m_Connected)
			{
				int64 end_time = 0;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);
				if(end_time - m_StartTime > FramePro::GetConditionalScopeMinTime())
					FramePro::AddTimeSpan(mp_NameAndSourceInfo, m_StartTime, end_time);
			}
		}

	private:
		bool m_Connected;
		const wchar_t* mp_NameAndSourceInfo;
		int64 m_StartTime;
	};

	//------------------------------------------------------------------------
	class ConditionalBoolTimerScope
	{
	public:
		ConditionalBoolTimerScope(const char* p_name_and_source_info, bool b)
		:	mp_NameAndSourceInfo(p_name_and_source_info)
		{
			bool send_scope = b && FramePro::IsConnected();
			m_SendScope = send_scope;

			if (send_scope)
			{
				int64 start_time;
				FRAMEPRO_GET_CLOCK_COUNT(start_time);
				FramePro::SubmitHiResTimers(start_time);
				m_StartTime = start_time;
			}
		}

		~ConditionalBoolTimerScope()
		{
			if(m_SendScope)
			{
				int64 end_time = 0;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);
				if(end_time - m_StartTime > FramePro::GetConditionalScopeMinTime())
					FramePro::AddTimeSpan(mp_NameAndSourceInfo, m_StartTime, end_time);
			}
		}

	private:
		bool m_SendScope;
		const char* mp_NameAndSourceInfo;
		int64 m_StartTime;
	};

	//------------------------------------------------------------------------
	class ConditionalBoolTimerScopeId
	{
	public:
		ConditionalBoolTimerScopeId(StringId name, const char* p_source_info, bool b)
		:	m_Name(name),
			mp_SourceInfo(p_source_info)
		{
			bool send_scope = b && FramePro::IsConnected();
			m_SendScope = send_scope;

			if (send_scope)
			{
				int64 start_time;
				FRAMEPRO_GET_CLOCK_COUNT(start_time);
				FramePro::SubmitHiResTimers(start_time);
				m_StartTime = start_time;
			}
		}

		~ConditionalBoolTimerScopeId()
		{
			if(m_SendScope)
			{
				int64 end_time = 0;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);
				if(end_time - m_StartTime > FramePro::GetConditionalScopeMinTime())
					FramePro::AddTimeSpan(m_Name, mp_SourceInfo, m_StartTime, end_time);
			}
		}

	private:
		bool m_SendScope;
		StringId m_Name;
		const char* mp_SourceInfo;
		int64 m_StartTime;
	};

	//------------------------------------------------------------------------
	class ConditionalBoolTimerScopeW
	{
	public:
		ConditionalBoolTimerScopeW(const wchar_t* p_name_and_source_info, bool b)
		:	mp_NameAndSourceInfo(p_name_and_source_info)
		{
			bool send_scope = b && FramePro::IsConnected();
			m_SendScope = send_scope;

			if (send_scope)
			{
				int64 start_time;
				FRAMEPRO_GET_CLOCK_COUNT(start_time);
				FramePro::SubmitHiResTimers(start_time);
				m_StartTime = start_time;
			}
		}

		~ConditionalBoolTimerScopeW()
		{
			if(m_SendScope)
			{
				int64 end_time = 0;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);
				if(end_time - m_StartTime > FramePro::GetConditionalScopeMinTime())
					FramePro::AddTimeSpan(mp_NameAndSourceInfo, m_StartTime, end_time);
			}
		}

	private:
		bool m_SendScope;
		const wchar_t* mp_NameAndSourceInfo;
		int64 m_StartTime;
	};

	//------------------------------------------------------------------------
	class StringLiteralNamedConditionalTimerScope
	{
	public:
		StringLiteralNamedConditionalTimerScope(const char* p_name, const char* p_source_info)
		:	mp_Name(p_name),
			mp_SourceInfo(p_source_info)
		{
			bool connected = FramePro::IsConnected();
			m_Connected = connected;

			int64 start_time;
			FRAMEPRO_GET_CLOCK_COUNT(start_time);
			if (connected)
				FramePro::SubmitHiResTimers(start_time);
			m_StartTime = start_time;
		}

		~StringLiteralNamedConditionalTimerScope()
		{
			if(m_Connected)
			{
				int64 end_time = 0;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);
				if(end_time - m_StartTime > FramePro::GetConditionalScopeMinTime())
					FramePro::AddTimeSpan(mp_Name, mp_SourceInfo, m_StartTime, end_time);
			}
		}

	private:
		bool m_Connected;
		const char* mp_Name;
		const char* mp_SourceInfo;
		int64 m_StartTime;
	};

	//------------------------------------------------------------------------
	class GlobalHiResTimer
	{
	public:
		GlobalHiResTimer(const char* p_name, const char* p_graph) : m_Value(0), mp_Next(NULL), mp_Name(p_name), mp_Graph(p_graph)
		{
			FramePro::AddGlobalHiResTimer(this);
		}

		void Add(uint value)
		{
			uint64 existing_value = m_Value.load(std::memory_order_relaxed);
			uint64 new_value;
			do
			{
				uint64 duration = existing_value & 0xffffffffff;
				uint64 count = (existing_value >> 40) & 0xffffff;
				duration += value;
				++count;
				new_value = (count << 40) | duration;

				FRAMEPRO_ASSERT(count <= 0xffffff);
				FRAMEPRO_ASSERT(duration <= 0xffffffffffL);

			} while (!m_Value.compare_exchange_weak(existing_value, new_value, std::memory_order_relaxed, std::memory_order_relaxed));
		}

		void GetAndClear(uint64& value, uint& count)
		{
			uint64 existing_value = m_Value.load(std::memory_order_relaxed);
			while (!m_Value.compare_exchange_weak(existing_value, 0, std::memory_order_relaxed, std::memory_order_relaxed))
				;

			value = existing_value & 0xffffffffff;
			count = (existing_value >> 40) & 0xffffff;
		}

		void SetNext(GlobalHiResTimer* p_next) { mp_Next = p_next; }

		GlobalHiResTimer* GetNext() const { return mp_Next; }

		const char* GetName() const { return mp_Name; }

		const char* GetGraph() const { return mp_Graph; }

	private:
		std::atomic<uint64> m_Value;

		GlobalHiResTimer* mp_Next;

		const char* mp_Name;
		const char* mp_Graph;
	};

	//------------------------------------------------------------------------
	class GlobalHiResTimerScope
	{
	public:
		GlobalHiResTimerScope(GlobalHiResTimer& timer)
		:	m_Timer(timer)
		{
			FRAMEPRO_GET_CLOCK_COUNT(m_StartTime);
		}

		~GlobalHiResTimerScope()
		{
			if (FramePro::IsConnected())
			{
				int64 end_time;
				FRAMEPRO_GET_CLOCK_COUNT(end_time);

				m_Timer.Add((uint)(end_time - m_StartTime));
			}
		}

	private:
		GlobalHiResTimerScope(const GlobalHiResTimerScope&);
		GlobalHiResTimerScope& operator=(const GlobalHiResTimerScope&);

		int64 m_StartTime;
		GlobalHiResTimer& m_Timer;
	};

	//------------------------------------------------------------------------
	class HiResTimerScope
	{
	public:
		HiResTimerScope(const char* p_name)
		{
			bool connected = FramePro::IsConnected();
			m_Connected = connected;

			if (connected)
				FramePro::StartHiResTimer(p_name);
		}

		~HiResTimerScope()
		{
			if (m_Connected)
				FramePro::StopHiResTimer();
		}

	private:
		bool m_Connected;
	};

	//------------------------------------------------------------------------
	class ThreadScope
	{
	public:
		ThreadScope(const char* p_thread_name)
		{
			SetThreadName(p_thread_name);
		}

		~ThreadScope()
		{
			CleanupThread();
		}
	};

	//------------------------------------------------------------------------
	class WaitEventScope
	{
	public:
		WaitEventScope(int64 event_id)
		:	m_EventId(event_id)
		{
			FRAMEPRO_GET_CLOCK_COUNT(m_StartTime);
		}

		~WaitEventScope()
		{
			int64 end_time;
			FRAMEPRO_GET_CLOCK_COUNT(end_time);

			FramePro::AddWaitEvent(m_EventId, m_StartTime, end_time);
		}

	private:
		int64 m_EventId;
		int64 m_StartTime;
	};

}

//------------------------------------------------------------------------
#endif		// #if FRAMEPRO_ENABLED

//------------------------------------------------------------------------
#endif		// #ifndef FRAMEPRO_H_INCLUDED

