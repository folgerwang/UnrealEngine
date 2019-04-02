// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericWindow.h"
#include "GenericPlatform/GenericApplication.h"
#include <android/native_window.h> 
#if USE_ANDROID_JNI
#include <android/native_window_jni.h>
#endif


/**
 * A platform specific implementation of FNativeWindow.
 * Native windows provide platform-specific backing for and are always owned by an SWindow.
 */
 
class APPLICATIONCORE_API FAndroidWindow : public FGenericWindow
{
public:
	~FAndroidWindow();

	/** Create a new FAndroidWindow.
	 *
	 * @param OwnerWindow		The SlateWindow for which we are crating a backing AndroidWindow
	 * @param InParent			Parent iOS window; usually NULL.
	 */
	static TSharedRef<FAndroidWindow> Make();

	
	virtual void* GetOSWindowHandle() const override { return Window; } //can be null.

	void Initialize( class FAndroidApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FAndroidWindow >& InParent, const bool bShowImmediately );

	/** Returns the rectangle of the screen the window is associated with */
	virtual bool GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const override;

	virtual void SetOSWindowHandle(void*);

	static FPlatformRect GetScreenRect();
	static void InvalidateCachedScreenRect();

	static void CalculateSurfaceSize(void* InWindow, int32_t& SurfaceWidth, int32_t& SurfaceHeight);
	static bool OnWindowOrientationChanged(bool bIsPortrait);

	static int32 GetDepthBufferPreference();
	
	static void AcquireWindowRef(ANativeWindow* InWindow);
	static void ReleaseWindowRef(ANativeWindow* InWindow);

	static void* GetHardwareWindow();
	static void SetHardwareWindow(void* InWindow);

	/** Waits on the current thread for a hardware window and returns it. 
	 *  May return nullptr if the application is shutting down.
	 */
	static void* WaitForHardwareWindow();

protected:
	/** @return true if the native window is currently in fullscreen mode, false otherwise */
	virtual EWindowMode::Type GetWindowMode() const override { return EWindowMode::Fullscreen; }

private:
	/** called from GetScreenRect function */
	/** test cached values from the latest computations stored by CacheRect to decide their validity with the provided arguments */
	static bool IsCachedRectValid(const bool bMosaicEnabled, const float RequestedContentScaleFactor, ANativeWindow* Window);
	/** caches some values used to compute the size of the window by GetScreenRect function */
	static void CacheRect(ANativeWindow* Window, const int32 Width, const int32 Height, const float RequestedContentScaleFactor, const bool bMosaicEnabled);

	/**
	 * Protect the constructor; only TSharedRefs of this class can be made.
	 */
	FAndroidWindow();

	FAndroidApplication* OwningApplication;

	/** iOS window handle, typically, only one should ever exist */
	ANativeWindow *Window;

	/** Store the window region size for querying whether a point lies within the window */
	int32 RegionX;
	int32 RegionY;

	static void* NativeWindow;
};
