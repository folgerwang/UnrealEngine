// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LuminEGL.h: Private EGL definitions for Lumin-specific functionality
=============================================================================*/
#pragma once

#if !PLATFORM_LUMINGL4

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GLES2/gl2.h>

struct LuminESPImpl;

DECLARE_LOG_CATEGORY_EXTERN(LogEGL, Log, All);

struct FPlatformOpenGLContext
{
	EGLContext	eglContext;
	GLuint		ViewportFramebuffer;
	GLuint		DefaultVertexArrayObject;

	FPlatformOpenGLContext()
	{
		Reset();
	}

	void Reset()
	{
		eglContext = EGL_NO_CONTEXT;
		ViewportFramebuffer = 0;
		DefaultVertexArrayObject = 0;
	}
};

class FScopeContext
{
public:
	FScopeContext(FPlatformOpenGLContext* PlatformContext);
	~FScopeContext(void);

private:
	EGLContext	LastContext;
	bool		bSameContext;
};

class LuminEGL
{
public:
	enum APIVariant
	{
		AV_OpenGLES,
		AV_OpenGLCore
	};

	static LuminEGL* GetInstance();
	~LuminEGL();

	bool IsInitialized();
	void InitBackBuffer();
	void DestroyBackBuffer();
	void Init(APIVariant API, uint32 MajorVersion, uint32 MinorVersion, bool bDebug);
	void ReInit();
	void UnBind();
	bool SwapBuffers();
	void Terminate();

	void GetDimensions(uint32& OutWidth, uint32& OutHeight);
	EGLDisplay GetDisplay();
	EGLContext CreateContext(EGLContext InSharedContext = EGL_NO_CONTEXT);
	int32 GetError();
	EGLBoolean SetCurrentContext(EGLContext InContext, EGLSurface InSurface);
	GLuint GetOnScreenColorRenderBuffer();
	GLuint GetResolveFrameBuffer();
	bool IsCurrentContextValid();
	EGLContext  GetCurrentContext();
	void SetCurrentSharedContext();
	void SetSharedContext();
	void SetSingleThreadRenderingContext();
	void SetMultithreadRenderingContext();
	void SetCurrentRenderingContext();
	uint32_t GetCurrentContextType();
	FPlatformOpenGLContext* GetRenderingContext();

	void* GetNativeWindow();

protected:
	LuminEGL();
	static LuminEGL* Singleton;

private:
	void InitEGL(APIVariant API);
	void TerminateEGL();

	bool InitContexts();
	void DestroyContext(EGLContext InContext);

	void ResetDisplay();

	LuminESPImpl* PImplData;

	void ResetInternal();
	void LogConfigInfo(EGLConfig  EGLConfigInfo);

	bool bSupportsKHRCreateContext;
	bool bSupportsKHRSurfacelessContext;

	int *ContextAttributes;
};

#endif	// !PLATFORM_ANDROIDGL