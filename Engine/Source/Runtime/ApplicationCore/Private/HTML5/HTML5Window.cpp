// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "HTML5/HTML5Window.h"
#include "HAL/OutputDevices.h"

THIRD_PARTY_INCLUDES_START
	#include <emscripten/emscripten.h>
	#include <emscripten/html5.h>
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogHTML5Window, Log, All);

FHTML5Window::~FHTML5Window()
{
	//    Use NativeWindow_Destroy() instead.
}

TSharedRef<FHTML5Window> FHTML5Window::Make()
{
	return MakeShareable( new FHTML5Window() );
}

FHTML5Window::FHTML5Window()
{
}

bool FHTML5Window::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	FPlatformRect ScreenRect = GetScreenRect();
	X = ScreenRect.Left;
	Y = ScreenRect.Top;
	Width = ScreenRect.Right - ScreenRect.Left;
	Height = ScreenRect.Bottom - ScreenRect.Top;
	return true;
}


void FHTML5Window::SetOSWindowHandle(void* InWindow)
{
}


FPlatformRect FHTML5Window::GetScreenRect()
{
	FPlatformRect ScreenRect;
	ScreenRect.Left = 0;
	ScreenRect.Top = 0;

	int Width, Height;
	emscripten_get_canvas_element_size(NULL, &Width, &Height);
	UE_LOG(LogHTML5Window, Verbose, TEXT("emscripten_get_canvas_element_size: Width:%d, Height:%d"), Width, Height);

	CalculateSurfaceSize(NULL,Width,Height);
	ScreenRect.Right = Width;
	ScreenRect.Bottom = Height;
	return ScreenRect;
}

void FHTML5Window::CalculateSurfaceSize(void* InWindow, int32_t& SurfaceWidth, int32_t& SurfaceHeight)
{
	// ensure the size is divisible by a specified amount
	const int DividableBy = 8;
	SurfaceWidth  = ((SurfaceWidth  + DividableBy - 1) / DividableBy) * DividableBy;
	SurfaceHeight = ((SurfaceHeight + DividableBy - 1) / DividableBy) * DividableBy;
}

EWindowMode::Type FHTML5Window::GetWindowMode() const
{
//#ifdef __EMSCRIPTEN_PTHREADS__
//	/// XXX TODO Restore this, emscripten_get_fullscreen_status() currently has a moderately high performance impact, so disabled for local testing
//	return EWindowMode::Windowed;
//#else
	EmscriptenFullscreenChangeEvent fullscreenStatus;
	EMSCRIPTEN_RESULT r = emscripten_get_fullscreen_status(&fullscreenStatus);
	return (r == EMSCRIPTEN_RESULT_SUCCESS && fullscreenStatus.isFullscreen) ? EWindowMode::Fullscreen : EWindowMode::Windowed;
//#endif
}

void FHTML5Window::ReshapeWindow(int32 X, int32 Y, int32 Width, int32 Height)
{
}
