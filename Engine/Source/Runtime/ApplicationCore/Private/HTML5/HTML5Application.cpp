// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "HTML5/HTML5Application.h"
#include "HTML5/HTML5Cursor.h"
#include "HTML5/HTML5InputInterface.h"
#include "HAL/OutputDevices.h"

#ifdef HTML5_USE_SDL2
#include "SDL_opengl.h"
#endif


// TODO: move mouse and keyboard handlers to HTML5InputInterface.cpp


DEFINE_LOG_CATEGORY_STATIC(LogHTML5Application, Log, All);

#include <emscripten/emscripten.h>
#ifdef __EMSCRIPTEN_PTHREADS__
#include <emscripten/threading.h>
#endif
#include <emscripten/key_codes.h>
#include <emscripten/html5.h>
#include "HTML5JavaScriptFx.h"

static const uint32 MaxWarmUpTicks = 10;

FHTML5Application* FHTML5Application::CreateHTML5Application()
{
	return new FHTML5Application();
}

// In HTML5 builds, we do not directly listen to browser window resize events in UE4 engine side,
// because we want the web page author to be able to fully control how the canvas size should react to
// when window size changes. All canvas resize operations occur by logic on the .html page, and the web
// developer should call the JavaScript function UE_JSlib.UE_CanvasSizeChanged() to report when they
// resized the canvas. This way developers can customize the logic of how the canvas should scale with
// the page based on the needs of their web site layout.
// When UE_JSlib.UE_CanvasSizeChanged() is called, it is flagged true here, and the next iteration
// of the engine renderer will apply those changes and resize the GL viewport to match.
// Note that this size change refers to a change in the WebGL render target resolution, and not a
// change in the visible CSS pixel size of the canvas DOM element (those two can be separately set and
// do not need to match). If the CSS size of the <canvas> element changes, UE4 engine does not really
// care to know, but the engine only follows size changes on the WebGL render target itself.
static volatile bool canvas_size_changed = false;

static void on_canvas_size_changed()
{
	canvas_size_changed = true;
}

static EM_BOOL canvas_resized_on_fullscreen_change(int eventType, const void *reserved, void *userData)
{
	on_canvas_size_changed();
	return 0;
}

// callback from javascript.
// This callback is run when user clicks on the "Fullscreen" button on the main page. It runs on the
// main browser thread, so do not call into UE4 from this function, since that would not be thread safe.
EM_BOOL request_fullscreen_callback_browser_thread(int eventType, const EmscriptenMouseEvent* evt, void* user)
{
	EmscriptenFullscreenStrategy FSStrat = {};

	// Ask user HTML page to resize the canvas when entering fullscreen. (Generally users do not need
	// to do anything specific here, but one of the premade resizing scenarios below should be good enough)
	bool abortFullscreen = EM_ASM_INT({
			if (Module['UE4_resizeCanvas'])
				return Module['UE4_resizeCanvas'](/*aboutToEnterFullscreen=*/true);
			return false;
		});
	if (abortFullscreen)
	{
		// If caller returns true above, abort the initiated attempt to go to move to fullscreen mode.
		return 0;
	}

	FSStrat.scaleMode = EM_ASM_INT({ return Module['UE4_fullscreenScaleMode']; });
	FSStrat.canvasResolutionScaleMode = EM_ASM_INT({ return Module['UE4_fullscreenCanvasResizeMode']; });
	FSStrat.filteringMode = EM_ASM_INT({ return Module['UE4_fullscreenFilteringMode']; });

	// If the WebGL render target size is going to change when entering & exiting fullscreen mode, track those changes
	// to be able to resize the viewport accordingly.
	if (FSStrat.canvasResolutionScaleMode != EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_NONE)
	{
		FSStrat.canvasResizedCallback = canvas_resized_on_fullscreen_change;
	}

	// TODO: UE4_useSoftFullscreenMode does not quite work right now because the "mainarea" div on the main page
	// has margins, which cause it to not align up, so this parameter is not exposed to the main html page
	// at the moment. Also page will need to manually hook e.g. esc button to exit the soft fullscreen mode,
	// which is not added. However this could be an useful feature to add at some point in the future.
	bool softFullscreen = EM_ASM_INT({ return Module['UE4_useSoftFullscreenMode']; });
	EMSCRIPTEN_RESULT result;
	if (softFullscreen)
	{
		result = emscripten_enter_soft_fullscreen("canvas", &FSStrat);
	}
	else
	{
		result = emscripten_request_fullscreen_strategy("canvas", false /* Don't allow deferring the request */, &FSStrat);
	}

	if (result == EMSCRIPTEN_RESULT_SUCCESS)
	{
		// Force keyboard focus on the canvas, so that any other DOM elements having active input control will no longer be accepting
		// input once we are in fullscreen.
		EM_ASM({Module['canvas'].focus()});
		on_canvas_size_changed();
	}
	return 0;
}

#ifdef __EMSCRIPTEN_PTHREADS__
// Thread ID of the main UE4 application thread.
static pthread_t ue4ThreadId = 0;
#endif

EM_BOOL pointerlockchange_callback(int eventType, const EmscriptenPointerlockChangeEvent *pointerLockChangeEvent, void *userData)
{
	FHTML5Application *application = reinterpret_cast<FHTML5Application*>(userData);
	if (!application) return 0;
	application->OnPointerLockChangeEvent(eventType, pointerLockChangeEvent);
	return 0;
}

static EM_BOOL focus_callback(int eventType, const EmscriptenFocusEvent *focusEvent, void *userData)
{
	FHTML5Application *application = reinterpret_cast<FHTML5Application*>(userData);
	if (!application) return 0;
	application->OnFocusEvent(eventType, focusEvent);
	return 0;
}

// This function is called on the main UE4 application thread, so it's safe to call to UE4 from here.
static EM_BOOL key_callback_ue4_thread(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData)
{
	FHTML5Application *application = reinterpret_cast<FHTML5Application*>(userData);
	if (!application) return 0;
	return application->OnKeyEvent(eventType, keyEvent);
}

// Web page developers have the following choices to hook into UE4's processing of input events. Implement a flexible
// mechanism for users to choose from, since for most developers, recompiling from source to customize behavior would be a difficult
// feat.
// Input event has two modes: 'process' or 'discard'. With 'process', UE4 will act on the received input event. With 'discard', the
// given input event is filtered out and not passed to UE4.
// Suppress action has three modes: 'default', 'no', and 'yes'. With 'no', UE4 input handling will not suppress default browser
// navigation from occurring. With 'yes', UE4 will call event.preventDefault(); to make browser not act on the input event. With
// 'default', UE4 will use its default set of heuristics to suppress the event or not.
#define UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_DEFAULT 0
#define UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_NO 1
#define UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_YES 2
#define UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_DEFAULT 3
#define UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_NO 4
#define UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_YES 5

// This function is called on the main browser thread, and not on the application main thread!
// Therefore do not access any Unreal Engine 4 data structures in this function.
EM_BOOL key_callback_browser_thread(int eventType, const EmscriptenKeyboardEvent *e, void *userData)
{
	// Event callback handling is a bit tricky when multithreading is involved. This is because we want to process
	// the actual keyboard event on the UE4 main application thread, but we also want to choose to suppress some keys
	// from getting the default browser behavior. For example, the Space key should not navigate the page one full
	// page down, and Tab key should not cycle between DOM element focus, (at least not when we are in pointer lock mode)

	// Proxy the input event to the UE4 main engine thread, and decide after that in browser thread whether to suppress the event or not.
	// Allow web page developer to hook into this process, so that they have means to fine tune the behavior without having to rebuild
	// the engine.

	int domPhysicalKeyCode = emscripten_compute_dom_pk_code(e->code);

	EmscriptenPointerlockChangeEvent pointerlockStatus = {};
	emscripten_get_pointerlock_status(&pointerlockStatus);

	EmscriptenFullscreenChangeEvent fullscreenStatus = {};
	emscripten_get_fullscreen_status(&fullscreenStatus);

	// User web pages may have other input elements which should function normally and be able to take keyboard input to enter text etc.
	// Therefore we process keyboard input only if it looks like user is not interacting with other DOM elements on the page.
	bool UE4CanvasHasFocus = EM_ASM_INT({return document.activeElement === document.body || document.activeElement === Module['canvas']});

	// Let JavaScript page developer hook into keyboard input event handling behavior.
	int processingBehavior = EM_ASM_INT({ if (Module['UE4_keyEvent']) { return Module['UE4_keyEvent']($0, Pointer_stringify($1), $2, $3, $4); }}, eventType, e->key, e->keyCode, domPhysicalKeyCode, e);
	if (processingBehavior == UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_YES) return EM_TRUE;
	if (processingBehavior == UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_NO) return EM_FALSE;
	if (UE4CanvasHasFocus && processingBehavior != UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_DEFAULT)
	{
		// Recompute physical key code, if JavaScript developer converted the input event to something else to
		// alter the input structure.
		domPhysicalKeyCode = emscripten_compute_dom_pk_code(e->code);

#ifdef __EMSCRIPTEN_PTHREADS__
		EmscriptenKeyboardEvent *keyEvent = (EmscriptenKeyboardEvent*)malloc(sizeof(*e));
		memcpy(keyEvent, e, sizeof(*e));
		emscripten_async_queue_on_thread(ue4ThreadId, EM_FUNC_SIG_IIII, key_callback_ue4_thread, keyEvent, eventType, keyEvent, userData);
#else
		key_callback_ue4_thread(eventType, e, userData);
#endif
	}
	if (processingBehavior == UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_YES) return EM_TRUE;
	if (processingBehavior == UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_NO) return EM_FALSE;

	// This is where we decide which keys should be suppressed from getting the default browser event handling behavior.
	// If we do not suppress a KeyDown event, it gets transformed to a KeyPress event in the browser. In practically all cases, this is
	// desirable. Browsers don't do any default navigation activities on KeyDown or KeyUp events, only on KeyPress events, so suppress
	// browser's default event handling only on KeyPresses. By default, try to be minimal and only suppress the fewest amount of inputs.
	if (eventType == EMSCRIPTEN_EVENT_KEYPRESS)
	{
		if (pointerlockStatus.isActive // If we have pointer lock, take keyboard input exclusively (browser overrides esc key though)
			|| fullscreenStatus.isFullscreen) // If we are in fullscreen mode, also take keyboard input exclusively (browser overrides esc key again)
			return EM_TRUE;

		if (UE4CanvasHasFocus &&
			(domPhysicalKeyCode == DOM_PK_BACKQUOTE
			|| domPhysicalKeyCode == DOM_PK_SPACE
			|| domPhysicalKeyCode == DOM_PK_ARROW_UP || domPhysicalKeyCode == DOM_PK_NUMPAD_8 // Don't want arrow keys to navigate the page
			|| domPhysicalKeyCode == DOM_PK_ARROW_DOWN || domPhysicalKeyCode == DOM_PK_NUMPAD_2
			|| domPhysicalKeyCode == DOM_PK_ARROW_LEFT || domPhysicalKeyCode == DOM_PK_NUMPAD_4
			|| domPhysicalKeyCode == DOM_PK_ARROW_RIGHT || domPhysicalKeyCode == DOM_PK_NUMPAD_6
			|| e->keyCode == DOM_VK_BACK_SPACE // This would navigate away from page
			|| e->keyCode == DOM_VK_RETURN
			|| e->keyCode == DOM_VK_TAB // This might cycle between active keyboard input elements in the browser
			|| !strcmp(e->key, "'") || !strcmp(e->key, "/"))) // These open up a "Quick find" shortcut on the page
			return EM_TRUE;
	}

	return EM_FALSE;
}

static EM_BOOL mouse_callback_ue4_thread(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData)
{
	FHTML5Application *application = reinterpret_cast<FHTML5Application*>(userData);
	if (!application) return 0;
	return application->OnMouseEvent(eventType, mouseEvent);
}

// Due to security, web browsers require that mouse pointer lock and fullscreen change requests are performed inside
// browser's user-initiated keyboard and mouse input handlers. However, when running in multithreaded mode, all input
// events are asynchronously routed to UE4 application thread to handle. This asynchronicity prevents us from synchronously
// asking UE4 engine whether to request pointer lock or not. Therefore use the following global data field to create
// a shared memory location where UE4 engine posts information whether it thinks mouse should now be locked or not.
// By reading this shared memory location, the browser thread side can synchronously deduce when to initiate pointer lock
// requests.
static volatile bool ue4_desires_pointer_lock = false;

// This function runs on the main browser thread, do not synchronously call into any UE4 data structures, it would not be thread safe.
static EM_BOOL mouse_callback_browser_thread(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
	// Let JavaScript page developer hook into mouse input event handling behavior.
	int processingBehavior = EM_ASM_INT({ if (Module['UE4_mouseEvent']) { return Module['UE4_mouseEvent']($0, $1, $2, $3, $4, $5); }}, eventType, e->canvasX, e->canvasY, e->button, e->buttons, e);
	if (processingBehavior == UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_YES) return EM_TRUE;
	if (processingBehavior == UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_NO) return EM_FALSE;
	if (processingBehavior != UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_DEFAULT)
	{

		// Asynchronously post the input event to UE4 main engine thread to handle.
#ifdef __EMSCRIPTEN_PTHREADS__
		EmscriptenMouseEvent *mouseEvent = (EmscriptenMouseEvent*)malloc(sizeof(*e));
		memcpy(mouseEvent, e, sizeof(*e));
		emscripten_async_queue_on_thread(ue4ThreadId, EM_FUNC_SIG_IIII, mouse_callback_ue4_thread, mouseEvent, eventType, mouseEvent, userData);
#else
		mouse_callback_ue4_thread(eventType, e, userData);
#endif
	}
	if (processingBehavior == UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_YES) return EM_TRUE;
	if (processingBehavior == UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_NO) return EM_FALSE;

	// Deduce whether we should be initiating a pointer lock request.
	if (ue4_desires_pointer_lock && eventType == EMSCRIPTEN_EVENT_MOUSEDOWN)
	{
		EmscriptenPointerlockChangeEvent pointerlockStatus = {};
		emscripten_get_pointerlock_status(&pointerlockStatus);
		if (!pointerlockStatus.isActive)
		{
			EMSCRIPTEN_RESULT res = emscripten_request_pointerlock("#canvas", false);
			if (res == EMSCRIPTEN_RESULT_SUCCESS)
			{
				// Force keyboard focus on the canvas, so that any other DOM elements having active input control will no longer be accepting
				// input once we are in fullscreen.
				EM_ASM({Module['canvas'].focus()});
			}
		}
	}

	// Any mouse clicks on the client area of the canvas should suppress default browser behavior.
	double w, h;
	emscripten_get_element_css_size("#canvas", &w, &h);
	return (e->canvasX >= 0 && e->canvasY >= 0 && e->canvasX < w && e->canvasY < h);
}

static EM_BOOL wheel_callback_ue4_thread(int eventType, const EmscriptenWheelEvent *wheelEvent, void *userData)
{
	FHTML5Application *application = reinterpret_cast<FHTML5Application*>(userData);
	if (!application) return 0;
	return application->OnWheelEvent(eventType, wheelEvent);
}

// This function runs on the main browser thread, do not synchronously call into any UE4 data structures, it would not be thread safe.
static EM_BOOL wheel_callback_browser_thread(int eventType, const EmscriptenWheelEvent *e, void *userData)
{
	// Let JavaScript page developer hook into mouse wheel input event handling behavior.
	int processingBehavior = EM_ASM_INT({ if (Module['UE4_wheelEvent']) { return Module['UE4_wheelEvent']($0, $1, $2, $3, $4, $5, $6, $7); }}, eventType, e->mouse.canvasX, e->mouse.canvasY, e->mouse.button, e->mouse.buttons, e->deltaX, e->deltaY, e);
	if (processingBehavior == UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_YES) return EM_TRUE;
	if (processingBehavior == UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_NO) return EM_FALSE;
	if (processingBehavior != UE4_INPUT_EVENT_DISCARD_SUPPRESS_ACTION_DEFAULT)
	{
		// Asynchronously post the input event to UE4 main engine thread to handle.
#ifdef __EMSCRIPTEN_PTHREADS__
		EmscriptenWheelEvent *wheelEvent = (EmscriptenWheelEvent*)malloc(sizeof(*e));
		memcpy(wheelEvent, e, sizeof(*e));
		emscripten_async_queue_on_thread(ue4ThreadId, EM_FUNC_SIG_IIII, wheel_callback_ue4_thread, wheelEvent, eventType, wheelEvent, userData);
#else
		wheel_callback_ue4_thread(eventType, e, userData);
#endif
	}
	if (processingBehavior == UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_YES) return EM_TRUE;
	if (processingBehavior == UE4_INPUT_EVENT_PROCESS_SUPPRESS_ACTION_NO) return EM_FALSE;

	EmscriptenPointerlockChangeEvent pointerlockStatus = {};
	emscripten_get_pointerlock_status(&pointerlockStatus);

	double w, h;
	emscripten_get_element_css_size("#canvas", &w, &h);

	// If one scrolls the mouse wheel on top of the canvas, or if pointer lock is active, wheel
	// should not scroll the page.
	return pointerlockStatus.isActive || (!pointerlockStatus.isActive && e->mouse.canvasX >= 0 && e->mouse.canvasY >= 0 && e->mouse.canvasX < w && e->mouse.canvasY < h);
}

EM_BOOL FHTML5Application::OnFocusEvent(int eventType, const EmscriptenFocusEvent *focusEvent)
{
	switch(eventType)
	{
	case EMSCRIPTEN_EVENT_FOCUS:
		UE_LOG(LogHTML5Application, Verbose, TEXT("WindowFocusGained"));
		MessageHandler->OnCursorSet();
		MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Activate);
		WarmUpTicks = 0;
		break;

	case EMSCRIPTEN_EVENT_BLUR:
		UE_LOG(LogHTML5Application, Verbose, TEXT("WindowFocusLost"));
		MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Deactivate);
		break;
	}
	return 0;
}

EM_BOOL FHTML5Application::OnPointerLockChangeEvent(int eventType, const EmscriptenPointerlockChangeEvent *pointerlockChange)
{
	UE_LOG(LogHTML5Application, Verbose, TEXT("PointerLockChangeEvent: Active:%d"), pointerlockChange->isActive);

#ifdef HTML5_USE_SDL2
	static int Prev = 0;
	// Generate a fake WindowsEnter event when the pointerlock goes from inactive to active.
	if (pointerlockChange->isActive && Prev == 0)
	{
		SDL_Event event;
		SDL_zero(event);
		event.type = SDL_WINDOWEVENT;
		event.window.event = SDL_WINDOWEVENT_ENTER;
		SDL_PushEvent(&event);
	}
	Prev = pointerlockChange->isActive;
#else
	if (pointerlockChange->isActive)
	{
		MessageHandler->OnCursorSet();
		MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Activate);
	}
	else
	{
		MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Deactivate);
	}
#endif
	return EM_FALSE;
}

// In browsers, KeyDown events do not carry a "character code" for most characters, but KeyPresses do.
// For WSAD input, we'd like to know the character code of the button press already at KeyDown time, so
// use the following method to interpret it from the event structure already at KeyDown event.
static int InterpretCharCode(int eventType, const EmscriptenKeyboardEvent *keyEvent)
{
	if (eventType == EMSCRIPTEN_EVENT_KEYPRESS && keyEvent->which) return keyEvent->which;
	if (keyEvent->charCode) return keyEvent->charCode;
	if (strlen(keyEvent->key) == 1) return (int)tolower(keyEvent->key[0]);
	if (keyEvent->which) return keyEvent->which;
	return keyEvent->keyCode;
}

// Counts the length of the given UTF8 string in number of characters (and not bytes)
static int NumCharsInUTF8String(const unsigned char *str)
{
	if (!str) return 0;
	int numChars = 0;
	while(*str)
	{
		if ((*str++ & 0xC0) != 0x80) ++numChars; // Skip all continuation bytes
	}
	return numChars;
}

// Converts a single UTF8 encoded character to a 32-bit Unicode codepoint.
static unsigned int Utf8CharToUtf32(const unsigned char *utf8Char)
{
	if (((*utf8Char) & 0x80) == 0) return (unsigned int)*utf8Char;
	if (((*utf8Char) & 0xE0) == 0xC0) return (((unsigned int)utf8Char[0] & 31) << 6) | ((unsigned int)utf8Char[1] & 63);
	if (((*utf8Char) & 0xF0) == 0xE0) return (((unsigned int)utf8Char[0] & 15) << 12) | (((unsigned int)utf8Char[1] & 63) << 6) | ((unsigned int)utf8Char[2] & 63);
	if (((*utf8Char) & 0xF8) == 0xF0) return (((unsigned int)utf8Char[0] & 7) << 18) | (((unsigned int)utf8Char[1] & 63) << 12) | (((unsigned int)utf8Char[2] & 63) << 6) | ((unsigned int)utf8Char[3] & 63);
	if (((*utf8Char) & 0xFC) == 0xF8) return (((unsigned int)utf8Char[0] & 3) << 24) | (((unsigned int)utf8Char[1] & 63) << 18) | (((unsigned int)utf8Char[2] & 63) << 12) | (((unsigned int)utf8Char[3] & 63) << 6) | ((unsigned int)utf8Char[4] & 63);
	return (((unsigned int)utf8Char[0] & 1) << 30) | (((unsigned int)utf8Char[1] & 63) << 24) | (((unsigned int)utf8Char[2] & 63) << 18) | (((unsigned int)utf8Char[3] & 63) << 12) | (((unsigned int)utf8Char[4] & 63) << 6) | ((unsigned int)utf8Char[5] & 63);
}

// Converts a UTF8 encoded string to a UTF32 string.
static void Utf8StringToUtf32(unsigned int *dstUtf32, int maxBytesToWrite, const unsigned char *utf8)
{
	unsigned int *end = dstUtf32 + (maxBytesToWrite - 4)/4;
	if (utf8)
	{
		while(*utf8 && dstUtf32 < end)
		{
			*dstUtf32++ = Utf8CharToUtf32(utf8++);
			while(*utf8 && (*utf8 & 0xC0) == 0x80)
				++utf8; // Skip all continuation bytes
		}
	}
	*dstUtf32 = 0;
}

EM_BOOL FHTML5Application::OnKeyEvent(int eventType, const EmscriptenKeyboardEvent *keyEvent)
{
	// To test out keyEvent fields in action in your browser, visit
	// https://s3.amazonaws.com/mozilla-games/test_keyboard_codes/test_keyboard_codes.html
	// which is a live compiled build of https://github.com/kripken/emscripten/tree/incoming/tests/test_keyboard_codes.c

	int keyCode = keyEvent->keyCode;
	int CharCode = InterpretCharCode(eventType, keyEvent);
	static_assert(sizeof(TCHAR) == 4, "The line below assumes TCHAR=UTF32");
	TCHAR Character = (TCHAR)Utf8CharToUtf32((unsigned char*)keyEvent->key);
	int domPhysicalKeyCode = emscripten_compute_dom_pk_code(keyEvent->code);

	// Slight hack: The keyEvent->keyCode field from browsers is not quite keyboard layout agnostic,
	// but we want the key above Tab key to always open the developer console, independent of
	// which keyboard layout one is using. The physical key "code" field is layout agnostic, but
	// unfortunately the physical key code values do not correspond with native VK or scan codes.
	// Therefore if we detect the physical key above Tab being pressed, make sure it is explicitly
	// treated as the en-us virtual key (VK) code of DOM_VK_BACK_QUOTE to map to opening the
	// developer console.
	if (domPhysicalKeyCode == DOM_PK_BACKQUOTE)
	{
		keyCode = DOM_VK_BACK_QUOTE;
		CharCode = Character = '`';
	}

	/* BUG: Workaround a crash - when pressing the (') character, HTML5 builds crash on assertion failure for some reason. Error message:
		Expression '(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5) || !PendingState.bFramebufferSetupInvalid' failed in Engine/Source/Runtime/OpenGLDrv/Private/OpenGLRenderTarget.cpp:1036!
		  at _FOpenGLDynamicRHI::BindPendingFramebuffer(FOpenGLContextState&)
		  at _FOpenGLDynamicRHI::RHIEndDrawIndexedPrimitiveUP()
		  at _non-virtual thunk to FOpenGLDynamicRHI::RHIEndDrawIndexedPrimitiveUP()
		  at _DrawIndexedPrimitiveUP(FRHICommandList&, unsigned int, unsigned int, unsigned int, unsigned int, void const*, unsigned int, void const*, unsigned int)
		  at _FBatchedElements::Draw(FRHICommandList&, FDrawingPolicyRenderState const&, ERHIFeatureLevel::Type, bool, FSceneView const&, bool, float, TRefCountPtr<FRHITexture2D>, EBlendModeFilter::Type) const
		  at _FCanvasBatchedElementRenderItem::Render_GameThread(FCanvas const*)::__8::operator()(FRHICommandList&) const
		  at _TEnqueueUniqueRenderCommandType<FCanvasBatchedElementRenderItem::Render_GameThread(FCanvas const*)::BatchedDrawCommandName, FCanvasBatchedElementRenderItem::Render_GameThread(FCanvas const*)::__8>::DoTask(ENamedThreads::Type, TRefCountPtr<FGraphEvent> const&)
		  at _FCanvasBatchedElementRenderItem::Render_GameThread(FCanvas const*)
		  at _FCanvas::Flush_GameThread(bool)
		  at _UGameViewportClient::Draw(FViewport*, FCanvas*)
		  at _non-virtual thunk to UGameViewportClient::Draw(FViewport*, FCanvas*)
		  at _FViewport::Draw(bool)
		  at _UGameEngine::RedrawViewports(bool)
		  at _UGameEngine::Tick(float, bool)
		  at _FEngineLoop::Tick()
		  at _HTML5_Tick()
		  at dynCall_v
		  at browserIterationFunc
		  at runIter
		  at Browser_mainLoop_runner
		Note: Currently this prevents inputting the ' character to console. TODO: Figure out why this crashes. */
	if (!strcmp(keyEvent->key, "'")) return 0;

	/* BUG: Workaround a crash - when pressing F5, HTML5 builds crash on assertion failure:
		DebugBreak() called!
	      at __Z19ValidateBoundShader12TRefCountPtrI23FOpenGLBoundShaderStateEP15FRHIPixelShader
	      at __ZN17FOpenGLDynamicRHI25RHISetShaderUniformBufferEP15FRHIPixelShaderjP17FRHIUniformBuffer
	      at __ZThn4_N17FOpenGLDynamicRHI25RHISetShaderUniformBufferEP15FRHIPixelShaderjP17FRHIUniformBuffer
	      at __Z25SetUniformBufferParameterIP15FRHIPixelShader39FMobileDirectionalLightShaderParameters15FRHICommandListEvRT1_T_RK29TShaderUniformBufferParameterIT0_ERK17TUniformBufferRefIS8_E
	      at __ZNK28TMobileBasePassDrawingPolicyI22FUniformLightMapPolicyLi0EE18SetMeshRenderStateER15FRHICommandListRK9FViewInfoPK20FPrimitiveSceneProxyRK10FMeshBatchiRK25FDrawingPolicyRenderStateRKNS1_15ElementDataTypeEN18FMeshDrawingPolicy15ContextDataTypeE
	      at __ZN36FDrawMobileBasePassDynamicMeshAction7ProcessILi0EEEvR15FRHICommandListRK30FProcessBasePassMeshParametersRK22FUniformLightMapPolicyRKPK20FLightCacheInterface
	      at __Z25ProcessMobileBasePassMeshI36FDrawMobileBasePassDynamicMeshActionLi0EEvR15FRHICommandListRK30FProcessBasePassMeshParametersOT_
	      at __ZN41FMobileBasePassOpaqueDrawingPolicyFactory20DrawDynamicMeshTemplILi0EEEvR15FRHICommandListRK9FViewInfoNS_11ContextTypeERK25FDrawingPolicyRenderStateRK10FMeshBatchPK9FMaterialPK20FPrimitiveSceneProxy11FHitProxyId
	      at __ZN41FMobileBasePassOpaqueDrawingPolicyFactory15DrawDynamicMeshER15FRHICommandListRK9FViewInfoNS_11ContextTypeERK10FMeshBatchbRK25FDrawingPolicyRenderStatePK20FPrimitiveSceneProxy11FHitProxyId
	      at __ZN20FMobileSceneRenderer20RenderMobileBasePassER24FRHICommandListImmediate10TArrayViewIPK9FViewInfoE
	      at __ZN20FMobileSceneRenderer6RenderER24FRHICommandListImmediate
	      at __ZL29RenderViewFamily_RenderThreadR24FRHICommandListImmediateP14FSceneRenderer
	      at __ZZN15FRendererModule24BeginRenderingViewFamilyEP7FCanvasP16FSceneViewFamilyEN27EURCMacro_FDrawSceneCommand6DoTaskEN13ENamedThreads4TypeERK12TRefCountPtrI11FGraphEventE
	      at __ZN15FRendererModule24BeginRenderingViewFamilyEP7FCanvasP16FSceneViewFamily
	      at __ZN19UGameViewportClient4DrawEP9FViewportP7FCanvas
	      at __ZThn32_N19UGameViewportClient4DrawEP9FViewportP7FCanvas
	      at __ZN9FViewport4DrawEb
	      at __ZN11UGameEngine15RedrawViewportsEb
	      at __ZN11UGameEngine4TickEfb
	      at __ZN11FEngineLoop4TickEv
	      at __Z10HTML5_Tickv
	      at dynCall_v
	      at browserIterationFunc
	      at runIter
	      at Browser_mainLoop_runner */
	if (domPhysicalKeyCode == DOM_PK_F5) return 0;

	switch(eventType)
	{
	case EMSCRIPTEN_EVENT_KEYDOWN:
		// First send OnKeyDown, and then OnKeyChar(). This is important, as in-game console ignores first character otherwise.
		UE_LOG(LogHTML5Application, Verbose, TEXT("KeyDown: keyCode: %d, CharCode: %d, physicalKey: %d, keyEvent->repeat: %s"), keyCode, CharCode, domPhysicalKeyCode, keyEvent->repeat ? TEXT("TRUE") : TEXT("FALSE"));
		MessageHandler->OnKeyDown(keyCode, CharCode, keyEvent->repeat);

		// Eagerly handle some non-printable text input characters in KeyDown, rather than waiting for KeyPress, which manages printable characters.
		if (keyCode == DOM_VK_BACK_SPACE || keyCode == DOM_VK_RETURN || keyCode == DOM_VK_TAB)
		{
			UE_LOG(LogHTML5Application, Verbose, TEXT("KeyChar on KeyDown: keyCode: %d, keyEvent->repeat: %s"), keyCode, keyEvent->repeat ? TEXT("TRUE") : TEXT("FALSE"));
			MessageHandler->OnKeyChar(keyCode, keyEvent->repeat);
		}
		break;

	case EMSCRIPTEN_EVENT_KEYUP:
		UE_LOG(LogHTML5Application, Verbose, TEXT("KeyUp KeyCode: %d, CharCode: %d, physicalKey: %d"), keyCode, CharCode, domPhysicalKeyCode);
		MessageHandler->OnKeyUp(keyCode, CharCode, keyEvent->repeat);
		break;

	case EMSCRIPTEN_EVENT_KEYPRESS:
		// keyEvent->key is a UTF8 string containing the textual representation of the key that was pressed.
		// It can be a single printable character, like "c" or "C", but also for non-printable characters,
		// a human readable description of the key, e.g. "Enter", "Backspace" or "Tab". We want to filter out these
		// non-printable characters (pressing Tab should not enter the text "Tab" on console).

		// Heuristic: Assume all printables are represented by a string that has exactly one character, other are control characters.
		if (NumCharsInUTF8String((unsigned char*)keyEvent->key) == 1)
		{
			UE_LOG(LogHTML5Application, Verbose, TEXT("KeyPress: Character: %lc, keyEvent->repeat: %s"), Character, keyEvent->repeat ? TEXT("TRUE") : TEXT("FALSE"));
			MessageHandler->OnKeyChar(Character, keyEvent->repeat);
		}
		else
		{
			// Log out to Verbose channel the key that we are discarding, to help debugging if some important key presses
			// are getting lost based on the above heuristic.
			unsigned int keyUtf32[64];
			Utf8StringToUtf32(keyUtf32, sizeof(keyUtf32), (unsigned char*)keyEvent->key);
			static_assert(sizeof(TCHAR) == 4, "The line below assumes TCHAR=UTF32");
			UE_LOG(LogHTML5Application, Verbose, TEXT("Ignored KeyChar on KeyPress, since it is a non-printable: key: %s, keyEvent->repeat: %s"), (TCHAR*)keyUtf32, keyEvent->repeat ? TEXT("TRUE") : TEXT("FALSE"));
		}
		break;
	}
	return 0;
}

// https://developer.mozilla.org/en-US/docs/Web/API/MouseEvent/button
static EMouseButtons::Type DomMouseButtonToUE4MouseButton(int button)
{
	switch(button)
	{
		case 0: return EMouseButtons::Left;
		case 1: return EMouseButtons::Middle;
		case 2: return EMouseButtons::Right;
		case 3: return EMouseButtons::Thumb01; // Note: Most web browsers don't really send buttons 3/4, but usually hardcode them for navigation.
		case 4: return EMouseButtons::Thumb02;
		default: return EMouseButtons::Invalid;
	}
}

EM_BOOL FHTML5Application::OnMouseEvent(int eventType, const EmscriptenMouseEvent *mouseEvent)
{
	switch(eventType)
	{
	case EMSCRIPTEN_EVENT_MOUSEDOWN:
		MessageHandler->OnMouseDown(ApplicationWindow, DomMouseButtonToUE4MouseButton(mouseEvent->button));
		UE_LOG(LogHTML5Application, Verbose, TEXT("MouseButtonDown ID:%d"), DomMouseButtonToUE4MouseButton(mouseEvent->button));
		break;

	case EMSCRIPTEN_EVENT_MOUSEUP:
		MessageHandler->OnMouseUp(DomMouseButtonToUE4MouseButton(mouseEvent->button));
		UE_LOG(LogHTML5Application, Verbose, TEXT("MouseButtonUp ID:%d"), DomMouseButtonToUE4MouseButton(mouseEvent->button));
		break;

	case EMSCRIPTEN_EVENT_MOUSEMOVE:
		{
//			bool bMouseMove = true;
//			EmscriptenFullscreenChangeEvent fullscreenStatus = {};
//			emscripten_get_fullscreen_status(&fullscreenStatus);
//			if ( !fullscreenStatus.isFullscreen )
//			{
//				bool UE4CanvasHasFocus = EM_ASM_INT({return document.activeElement === document.body || document.activeElement === Module['canvas']});
//				if ( ! UE4CanvasHasFocus )
//				{
//					if (ue4_desires_pointer_lock)
//					{
//						// Only send relative mouse motion events to the application when pointer lock is active, since the camera moving
//						// around when one hovers the mouse cursor around the scene is distracting UX.
//						EmscriptenPointerlockChangeEvent pointerlockStatus = {};
////#ifndef __EMSCRIPTEN_PTHREADS__
////						/// XXX TODO Restore this, emscripten_get_pointerlock_status() currently has a moderately high performance impact, so disabled for local testing
//						emscripten_get_pointerlock_status(&pointerlockStatus);
////#endif
//						if (!pointerlockStatus.isActive)
//						{
//							bMouseMove = false;
//						}
//					}
//				}
//			}
//
//			if (bMouseMove)
//			{
				MessageHandler->OnRawMouseMove(mouseEvent->movementX, mouseEvent->movementY);
				MessageHandler->OnMouseMove();
				UE_LOG(LogHTML5Application, Verbose, TEXT("MouseMotion Pos(%d, %d) XRel:%d YRel:%d"), mouseEvent->canvasX, mouseEvent->canvasY, mouseEvent->movementX, mouseEvent->movementY)
//			}
		}
		break;
	}
	return 0;
}

EM_BOOL FHTML5Application::OnWheelEvent(int eventType, const EmscriptenWheelEvent *wheelEvent)
{
	switch(eventType)
	{
	case EMSCRIPTEN_EVENT_WHEEL:
		const float SpinFactor = 1 / 120.0f;
		MessageHandler->OnMouseWheel(wheelEvent->deltaY * SpinFactor);
		UE_LOG(LogHTML5Application, Verbose, TEXT("MouseWheel deltaY=%f, scaled to SpinFactor=%f"), wheelEvent->deltaY, wheelEvent->deltaY * SpinFactor);
		break;
	}
	return 0;
}

FHTML5Application::FHTML5Application()
	: GenericApplication( MakeShareable( new FHTML5Cursor() ) )
	, ApplicationWindow(FHTML5Window::Make())
	, InputInterface( FHTML5InputInterface::Create(MessageHandler, Cursor ) )
	, WarmUpTicks(-1)
{
	// full screen will only be requested after the first click after the window gains focus.
	// the problem is that because of security/UX reasons browsers don't allow pointer lock in main loop
	// but only through callbacks generated by browser.

#ifdef HTML5_USE_SDL2
	// USING SDL2 -- impliments polling (see PollGameDeviceState below)
	UE_LOG(LogHTML5Application, Display, TEXT("FHTML5Application -- HTML5_USE_SDL2"));
#else
	// USING EMSCRIPTEN NATIVE -- impliments callbacks
	UE_LOG(LogHTML5Application, Display, TEXT("FHTML5Application -- using emscripten native functions"));

#ifdef __EMSCRIPTEN_PTHREADS__
	ue4ThreadId = pthread_self();

	emscripten_set_click_callback_on_thread("fullscreen_request", nullptr, true, request_fullscreen_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);

	emscripten_set_keydown_callback_on_thread("#window", this, 0, key_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
	emscripten_set_keyup_callback_on_thread("#window", this, 0, key_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
	emscripten_set_keypress_callback_on_thread("#window", this, 0, key_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);

	emscripten_set_mousedown_callback_on_thread("#canvas", this, 0, mouse_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
	emscripten_set_mouseup_callback_on_thread("#canvas", this, 0, mouse_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
	emscripten_set_mousemove_callback_on_thread("#canvas", this, 0, mouse_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);

	emscripten_set_wheel_callback_on_thread("#canvas", this, 0, wheel_callback_browser_thread, EM_CALLBACK_THREAD_CONTEXT_MAIN_BROWSER_THREAD);
#else
	// work around emscripten bug where deffered browser requests are not called if there are no callbacks.
	emscripten_set_click_callback("fullscreen_request", this, 0, request_fullscreen_callback_browser_thread);

	emscripten_set_keydown_callback("#window", this, 0, key_callback_browser_thread);
	emscripten_set_keyup_callback("#window", this, 0, key_callback_browser_thread);
	emscripten_set_keypress_callback("#window", this, 0, key_callback_browser_thread);

	emscripten_set_mousedown_callback("#canvas",this,0,mouse_callback_browser_thread);
	emscripten_set_mouseup_callback("#canvas", this, 0, mouse_callback_browser_thread);
	emscripten_set_mousemove_callback("#canvas", this, 0, mouse_callback_browser_thread);

	emscripten_set_wheel_callback("#canvas", this, 0, wheel_callback_browser_thread);
#endif
#endif

	emscripten_set_pointerlockchange_callback(0, 0, true, pointerlockchange_callback);

	emscripten_set_focus_callback("#window", this, 1, focus_callback);
	emscripten_set_blur_callback("#window", this, 1, focus_callback);

	// Register to listen to when web developer decides to change the size of the WebGL canvas.
	UE_EngineRegisterCanvasResizeListener(on_canvas_size_changed);
}


void FHTML5Application::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler( MessageHandler );
}

void FHTML5Application::PollGameDeviceState( const float TimeDelta )
{
#ifdef HTML5_USE_SDL2
	SDL_Event Event;
	while (SDL_PollEvent(&Event))
	{
		// Tick Input Interface.
		switch (Event.type)
		{
				case SDL_WINDOWEVENT:
				{
					SDL_WindowEvent windowEvent = Event.window;

					switch (windowEvent.event)
					{
					case SDL_WINDOWEVENT_ENTER:
						{
							UE_LOG(LogHTML5Application, Verbose, TEXT("WindowEnter"));
						//	MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Activate);
							WarmUpTicks = 0;
						}
						break;
					case SDL_WINDOWEVENT_LEAVE:
						{
							UE_LOG(LogHTML5Application, Verbose, TEXT("WindowLeave"));
						 //	MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Deactivate);
						}
						break;
					case SDL_WINDOWEVENT_FOCUS_GAINED:
						{
							UE_LOG(LogHTML5Application, Verbose, TEXT("WindowFocusGained"));
							MessageHandler->OnCursorSet();
							MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Activate);
									WarmUpTicks = 0;
						}
						break;
					case SDL_WINDOWEVENT_FOCUS_LOST:
						{
							UE_LOG(LogHTML5Application, Verbose, TEXT("WindowFocusLost"));
							MessageHandler->OnWindowActivationChanged(ApplicationWindow, EWindowActivation::Deactivate);
						}
						break;
					default:
						break;
					}
				}
			default:
			{
				InputInterface->Tick( TimeDelta,Event, ApplicationWindow);
			}
		}
	}
#endif

	InputInterface->SendControllerEvents();

/*


	if ( WarmUpTicks >= 0)
		WarmUpTicks ++;


	if ( WarmUpTicks == MaxWarmUpTicks  )
	{
		// browsers don't allow locking and hiding to work independently. use warmup ticks after the application has settled
		// on its mouse lock/visibility status.  This is necessary even in cases where the game doesn't want to locking because
		// the lock status oscillates for few ticks before settling down. This causes a Browser UI pop even when we don't intend to lock.
		// see http://www.w3.org/TR/pointerlock more for information.
		if (((FHTML5Cursor*)Cursor.Get())->LockStatus && !((FHTML5Cursor*)Cursor.Get())->CursorStatus)
		{
			UE_LOG(LogHTML5Application, Verbose, TEXT("Request pointer lock"));
			emscripten_request_pointerlock ( "#canvas" , true);
		}
		else
		{
			UE_LOG(LogHTML5Application, Verbose, TEXT("Exit pointer lock"));
			//emscripten_exit_pointerlock();
		}
		WarmUpTicks = -1;
	}
*/

//#ifndef __EMSCRIPTEN_PTHREADS__
//	/// XXX TODO Restore this, emscripten_get_pointerlock_status() currently has a moderately high performance impact, so disabled for local testing

	// Update information to the web browser main thread about whether UE4 engine would like the browser pointer lock
	// to be active ...
	ue4_desires_pointer_lock = (((FHTML5Cursor*)Cursor.Get())->LockStatus && !((FHTML5Cursor*)Cursor.Get())->CursorStatus);

	// ... and if not, check if it was, and we should be exiting pointer lock mode.
	if (!ue4_desires_pointer_lock)
	{
		EmscriptenPointerlockChangeEvent pointerlockStatus = {};
		emscripten_get_pointerlock_status(&pointerlockStatus);
		if (pointerlockStatus.isActive)
		{
			emscripten_exit_pointerlock();
		}
	}
//#endif

	// If the WebGL canvas has changed its size, pick up the changes and propagate the viewport
	// resize throughout the engine.
	if (canvas_size_changed)
	{
		canvas_size_changed = false;

		int canvas_w, canvas_h;
		emscripten_get_canvas_element_size(NULL, &canvas_w, &canvas_h);

		UE_LOG(LogHTML5Application, Display, TEXT("Canvas size changed: New size: %dx%d"), canvas_w, canvas_h);

		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

		MessageHandler->OnSizeChanged(ApplicationWindow,canvas_w,canvas_h, false);
		MessageHandler->OnResizingWindow(ApplicationWindow);
		BroadcastDisplayMetricsChanged(DisplayMetrics);
	}
}

FPlatformRect FHTML5Application::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	return FHTML5Window::GetScreenRect();
}

void FDisplayMetrics::RebuildDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FHTML5Window::GetScreenRect();
	OutDisplayMetrics.VirtualDisplayRect    =	OutDisplayMetrics.PrimaryDisplayWorkAreaRect;
	OutDisplayMetrics.PrimaryDisplayWidth   =	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right;
	OutDisplayMetrics.PrimaryDisplayHeight  =	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom;
	UE_LOG(LogHTML5Application, Verbose, TEXT("GetDisplayMetrics Width:%d, Height:%d"), OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right, OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom);

	// Apply the debug safe zones
	OutDisplayMetrics.ApplyDefaultSafeZones();
}

TSharedRef< FGenericWindow > FHTML5Application::MakeWindow()
{
	return ApplicationWindow;
}
