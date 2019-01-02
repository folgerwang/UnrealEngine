// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if PLATFORM_ANDROIDESDEFERRED

#include "OpenGLDrvPrivate.h"
#include "Android/AndroidApplication.h"

#include <dlfcn.h>
#include <android/log.h>

#include "Misc/ScopeLock.h"

#define	LOG_TAG "UE4"
#define	LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define	LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define	LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

///////////////////////////////////////////////////////////////////////////////

// OpenGL function pointers

#define DEFINE_GL_ENTRYPOINTS(Type,Func) Type Func = NULL;
ENUM_GL_ENTRYPOINTS_CORE(DEFINE_GL_ENTRYPOINTS) \
ENUM_GL_ENTRYPOINTS_MANUAL(DEFINE_GL_ENTRYPOINTS) \
ENUM_GL_ENTRYPOINTS_OPTIONAL(DEFINE_GL_ENTRYPOINTS)

///////////////////////////////////////////////////////////////////////////////

bool FLuminESDeferredOpenGL::bSupportsBindlessTexture = false;

void FLuminESDeferredOpenGL::ProcessExtensions(const FString& ExtensionsString)
{
	FOpenGLESDeferred::ProcessExtensions(ExtensionsString);

	bSupportsBindlessTexture = ExtensionsString.Contains(TEXT("GL_NV_bindless_texture"));
}



///////////////////////////////////////////////////////////////////////////////

// OpenGL platform functions

/**
* Enable/Disable debug context from the commandline
*/
static bool PlatformOpenGLDebugCtx()
{
#if UE_BUILD_DEBUG
	return ! FParse::Param(FCommandLine::Get(),TEXT("openglNoDebug"));
#else
	return FParse::Param(FCommandLine::Get(),TEXT("openglDebug"));;
#endif
}


struct FPlatformOpenGLDevice
{
	void SetCurrentSharedContext();
	void SetCurrentRenderingContext();
	void SetCurrentNULLContext();

	FPlatformOpenGLDevice();
	~FPlatformOpenGLDevice();
	void Init();
	void LoadEXT();
	void Terminate();
	void ReInit();
};

FPlatformOpenGLDevice::FPlatformOpenGLDevice()
{
}

FPlatformOpenGLDevice::~FPlatformOpenGLDevice()
{
	LuminEGL::GetInstance()->DestroyBackBuffer();
	LuminEGL::GetInstance()->Terminate();
}

void FPlatformOpenGLDevice::Init()
{
	UE_LOG( LogRHI, Warning, TEXT("Entering FPlatformOpenGLDevice::Init"));
	extern void InitDebugContext();

	LuminEGL::GetInstance()->SetSingleThreadRenderingContext();

	// Initialize all of the entry points we have to query manually
#define GET_GL_ENTRYPOINTS(Type,Func) Func = (Type)eglGetProcAddress(#Func);
	ENUM_GL_ENTRYPOINTS_CORE(GET_GL_ENTRYPOINTS);
	ENUM_GL_ENTRYPOINTS_MANUAL(GET_GL_ENTRYPOINTS);
	ENUM_GL_ENTRYPOINTS_OPTIONAL(GET_GL_ENTRYPOINTS);

	// Check that all of the required entry points have been initialized
	bool bFoundAllEntryPoints = true;
#define CHECK_GL_ENTRYPOINTS(Type,Func) if (Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }
	ENUM_GL_ENTRYPOINTS_CORE(CHECK_GL_ENTRYPOINTS);

	checkf(bFoundAllEntryPoints, TEXT("Failed to find all required OpenGL entry points."));

	ENUM_GL_ENTRYPOINTS_MANUAL(CHECK_GL_ENTRYPOINTS);
	ENUM_GL_ENTRYPOINTS_OPTIONAL(CHECK_GL_ENTRYPOINTS);

	const FString ExtensionsString = ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_EXTENSIONS));

	// If EXT_disjoint_timer_query wasn't found, NV_timer_query might be available
	{
		// These functions get exported under different names by different extensions
		// Can't just check for NULL, because Android returns and unimplemented function catch
		
		if ( !ExtensionsString.Contains(TEXT("GL_EXT_disjoint_timer_query")) && ExtensionsString.Contains(TEXT("GL_NV_timer_query")))
		{
			glQueryCounterEXT = (PFNGLQUERYCOUNTEREXTPROC)eglGetProcAddress("glQueryCounterNV");
			glGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC)eglGetProcAddress("glGetQueryObjectui64vNV");
		}
	}

	const bool bAdvancedFeatures = FOpenGL::SupportsAdvancedFeatures();

	// devices that have ES2.0 only might support some ES3.x core functionality with extensions
	if(!bAdvancedFeatures)
	{
		if(ExtensionsString.Contains(TEXT("GL_EXT_occlusion_query_boolean")))
		{
			glGenQueries = (PFNGLGENQUERIESPROC(eglGetProcAddress("glGenQueriesEXT")));
			glDeleteQueries = (PFNGLDELETEQUERIESPROC(eglGetProcAddress("glDeleteQueriesEXT")));
			glGetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVPROC)((void*)eglGetProcAddress("glGetQueryObjectuivEXT"));

		}

		// Android doesn't setup formats completely compatible with glTexStorage in ES2 mode
		glTexStorage2D = nullptr;
		glTexStorage3D = nullptr;

	}

	// For MSAA
	glFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)((void*)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT"));
	glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)((void*)eglGetProcAddress("glRenderbufferStorageMultisampleEXT"));

	if (!bFoundAllEntryPoints)
	{
		UE_LOG(LogRHI, Warning, TEXT("Failed to acquire all optional OpenGL entrypoints, may fall back to OpenGL ES 2.0"));
	}

	if (bAdvancedFeatures)
	{
		GLuint DefaultVertexArrayObject = 0;
		glGenVertexArrays(1, &DefaultVertexArrayObject);
		glBindVertexArray(DefaultVertexArrayObject);
	}
	InitDefaultGLContextState();
	InitDebugContext();

	LuminEGL::GetInstance()->SetMultithreadRenderingContext();
	if (bAdvancedFeatures)
	{
		GLuint DefaultVertexArrayObject = 0;
		glGenVertexArrays(1, &DefaultVertexArrayObject);
		glBindVertexArray(DefaultVertexArrayObject);
	}
	InitDefaultGLContextState();
	InitDebugContext();

	LuminEGL::GetInstance()->SetSharedContext();
	if (bAdvancedFeatures)
	{
		GLuint DefaultVertexArrayObject = 0;
		glGenVertexArrays(1, &DefaultVertexArrayObject);
		glBindVertexArray(DefaultVertexArrayObject);
	}
	InitDefaultGLContextState();
	InitDebugContext();

	PlatformSharedContextSetup(this);

	LuminEGL::GetInstance()->InitBackBuffer(); //can be done only after context is made current.
}

void FPlatformOpenGLDevice::SetCurrentSharedContext()
{
	LuminEGL::GetInstance()->SetCurrentSharedContext();
}

void FPlatformOpenGLDevice::SetCurrentRenderingContext()
{
	LuminEGL::GetInstance()->SetCurrentRenderingContext();
}


FPlatformOpenGLDevice* PlatformCreateOpenGLDevice()
{
	FPlatformOpenGLDevice* Device = new FPlatformOpenGLDevice();
	Device->Init();
	return Device;
}

bool PlatformCanEnableGPUCapture()
{
	return false;
}

void PlatformDestroyOpenGLDevice(FPlatformOpenGLDevice* Device)
{
	delete Device;
}

FPlatformOpenGLContext* PlatformCreateOpenGLContext(FPlatformOpenGLDevice* Device, void* InWindowHandle)
{
	//Assumes Device is already initialized and context already created.
	return LuminEGL::GetInstance()->GetRenderingContext();
}

void PlatformReleaseOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* PlatformContext)
{
	// nothing to do for now
}

void* PlatformGetWindow(FPlatformOpenGLContext* Context, void** AddParam)
{
	check(Context);

	return (void*)&Context->eglContext;
}

void PlatformDestroyOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* PlatformContext)
{
	delete Device; //created here, destroyed here, but held by RHI.
}

bool PlatformBlitToViewport( FPlatformOpenGLDevice* Device, const FOpenGLViewport& Viewport, uint32 BackbufferSizeX, uint32 BackbufferSizeY, bool bPresent,bool bLockToVsync, int32 SyncInterval )
{
	FPlatformOpenGLContext* const Context = Viewport.GetGLContext();
	check(Context && Context->eglContext);
	FScopeContext ScopeContext(Context);
	if (FOpenGL::IsES2())
	{
		if (bPresent && Viewport.GetCustomPresent())
		{
			glBindFramebuffer(GL_FRAMEBUFFER, Context->ViewportFramebuffer);
			bPresent = Viewport.GetCustomPresent()->Present(SyncInterval);
		}
		if (bPresent)
		{
			LuminEGL::GetInstance()->SwapBuffers();
		}
	}
	else
	{
		if (bPresent && Viewport.GetCustomPresent())
		{
			glBindFramebuffer(GL_FRAMEBUFFER, Context->ViewportFramebuffer);
			bPresent = Viewport.GetCustomPresent()->Present(SyncInterval);
			if (!bPresent)
			{
				return false;
			}
		}
		if (bPresent && !Viewport.GetCustomPresent())
		{
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			//Disabling for now to work around a GL_INVALID_OPERATION which might or might not be legit in the context of EGL.
			//Note that the drawbuffer state is part of the FBO state, so we don't need to touch it per frame.
			//glDrawBuffer(GL_BACK);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, Context->ViewportFramebuffer);
			glReadBuffer(GL_COLOR_ATTACHMENT0);

			//LOGD("PlatformBlitToViewport: backbuffer (%d, %d) to screen (%d, %d)", BackbufferSizeX, BackbufferSizeY, GRealScreenWidth, GRealScreenHeight);

			uint32 GRealScreenHeight, GRealScreenWidth;
			LuminEGL::GetInstance()->GetDimensions(GRealScreenWidth, GRealScreenHeight);

			glBlitFramebuffer(
				0, 0, BackbufferSizeX, BackbufferSizeY,
				0, 0, GRealScreenWidth, GRealScreenHeight,
				GL_COLOR_BUFFER_BIT,
				GL_LINEAR
			);
		}
		if (bPresent)
		{
			uint32 IdleStart = FPlatformTime::Cycles();

			LuminEGL::GetInstance()->SwapBuffers();
			REPORT_GL_END_BUFFER_EVENT_FOR_FRAME_DUMP();
//			INITIATE_GL_FRAME_DUMP_EVERY_X_CALLS( 1000 );

			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUPresent] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUPresent]++;
		}
	}
//	return true;
	//Do not want WaitForFrameEventCompletion
	return false;
}

void PlatformRenderingContextSetup(FPlatformOpenGLDevice* Device)
{
	Device->SetCurrentRenderingContext();
}

void PlatformFlushIfNeeded()
{
}

void PlatformRebindResources(FPlatformOpenGLDevice* Device)
{
}

void PlatformSharedContextSetup(FPlatformOpenGLDevice* Device)
{
	Device->SetCurrentSharedContext();
}

void PlatformNULLContextSetup()
{
	LuminEGL::GetInstance()->SetCurrentContext(EGL_NO_CONTEXT, EGL_NO_SURFACE);
}

EOpenGLCurrentContext PlatformOpenGLCurrentContext(FPlatformOpenGLDevice* Device)
{
	return (EOpenGLCurrentContext)LuminEGL::GetInstance()->GetCurrentContextType();
}

void* PlatformOpenGLCurrentContextHandle(FPlatformOpenGLDevice* Device)
{
	return LuminEGL::GetInstance()->GetCurrentContext();
}

void PlatformRestoreDesktopDisplayMode()
{
}

FRHITexture* PlatformCreateBuiltinBackBuffer(FOpenGLDynamicRHI* OpenGLRHI, uint32 SizeX, uint32 SizeY)
{
	FOpenGLTexture2D* Texture2D = NULL;

	if ( FOpenGL::IsES2())
	{
		uint32 Flags = TexCreate_RenderTargetable;
		Texture2D = new FOpenGLTexture2D(OpenGLRHI, LuminEGL::GetInstance()->GetOnScreenColorRenderBuffer(), GL_RENDERBUFFER, GL_COLOR_ATTACHMENT0, SizeX, SizeY, 0, 1, 1, 1, 0, PF_B8G8R8A8, false, false, Flags, nullptr, FClearValueBinding::Transparent);
		OpenGLTextureAllocated(Texture2D, Flags);
	}

	return Texture2D;
}

void PlatformResizeGLContext( FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context, uint32 SizeX, uint32 SizeY, bool bFullscreen, bool bWasFullscreen, GLenum BackBufferTarget, GLuint BackBufferResource)
{
	check(Context);
	FScopeContext ScopeContext(Context);

	if (FOpenGL::IsES2())
	{
		glViewport(0, 0, SizeX, SizeY);
		VERIFY_GL(glViewport);
	}
	else
	{
		if (Context->ViewportFramebuffer == 0)
		{
			glGenFramebuffers(1, &Context->ViewportFramebuffer);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, Context->ViewportFramebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, BackBufferTarget, BackBufferResource, 0);

#if UE_BUILD_DEBUG
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		FOpenGL::DrawBuffer(GL_COLOR_ATTACHMENT0);
		GLenum CompleteResult = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (CompleteResult != GL_FRAMEBUFFER_COMPLETE)
		{
			UE_LOG(LogRHI, Fatal, TEXT("PlatformResizeGLContext: Framebuffer not complete. Status = 0x%x"), CompleteResult);
		}
#endif

		glViewport(0, 0, SizeX, SizeY);
		static GLfloat ZeroColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		glClearBufferfv(GL_COLOR, 0, ZeroColor);
	}
}

void PlatformGetSupportedResolution(uint32 &Width, uint32 &Height)
{
}

bool PlatformGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{

	return true;
}

bool PlatformInitOpenGL()
{
	// Original location for querying function entrypoints
	return true;
}

bool PlatformOpenGLContextValid()
{
	return (eglGetCurrentContext() != EGL_NO_CONTEXT);
}

int32 PlatformGlGetError()
{
	return glGetError();
}

void PlatformGetBackbufferDimensions( uint32& OutWidth, uint32& OutHeight )
{
	LuminEGL::GetInstance()->GetDimensions(OutWidth, OutHeight);
}

// =============================================================

struct FOpenGLReleasedQuery
{
	EGLContext	Context;
	GLuint		Query;
};

static TArray<FOpenGLReleasedQuery>	ReleasedQueries;
static FCriticalSection*			ReleasedQueriesGuard;

void PlatformGetNewRenderQuery(GLuint* OutQuery, uint64* OutQueryContext)
{
	if (!ReleasedQueriesGuard)
	{
		ReleasedQueriesGuard = new FCriticalSection;
	}

	{
		FScopeLock Lock(ReleasedQueriesGuard);

#ifdef UE_BUILD_DEBUG
		check(OutQuery && OutQueryContext);
#endif

		EGLContext Context = eglGetCurrentContext();
		check(Context);

		GLuint NewQuery = 0;

		/*
		 * Note, not reusing queries, because timestamp and occlusion are different
		 */

		if (!NewQuery)
		{
			FOpenGL::GenQueries(1, &NewQuery);
		}

		*OutQuery = NewQuery;
		*OutQueryContext = (uint64)Context;
	}
}

void PlatformReleaseRenderQuery(GLuint Query, uint64 QueryContext)
{
	EGLContext Context = eglGetCurrentContext();
	if ((uint64)Context == QueryContext)
	{
		FOpenGL::DeleteQueries(1, &Query);
	}
	else
	{
		FScopeLock Lock(ReleasedQueriesGuard);
#ifdef UE_BUILD_DEBUG
		check(Query && QueryContext && ReleasedQueriesGuard);
#endif
		FOpenGLReleasedQuery ReleasedQuery;
		ReleasedQuery.Context = (EGLContext)QueryContext;
		ReleasedQuery.Query = Query;
		ReleasedQueries.Add(ReleasedQuery);
	}
}

void DeleteOcclusionQueriesForCurrentContext(EGLContext Context)
{
	if (!ReleasedQueriesGuard)
	{
		ReleasedQueriesGuard = new FCriticalSection;
	}

	{
		FScopeLock Lock(ReleasedQueriesGuard);
		for (int32 Index = 0; Index < ReleasedQueries.Num(); ++Index)
		{
			if (ReleasedQueries[Index].Context == Context)
			{
				FOpenGL::DeleteQueries(1, &ReleasedQueries[Index].Query);
				ReleasedQueries.RemoveAtSwap(Index);
				--Index;
			}
		}
	}
}

bool PlatformContextIsCurrent( uint64 QueryContext )
{
	return (uint64)eglGetCurrentContext() == QueryContext;
}

void FAndroidAppEntry::PlatformInit()
{
	const bool Debug = PlatformOpenGLDebugCtx();

	// So you'd think we'd pass 3,1 into version here, but the Android ES 3.1 code passes version 2 as well.
	LuminEGL::GetInstance()->Init(LuminEGL::AV_OpenGLES, 2, 0, false);
}

void FAndroidAppEntry::ReleaseEGL()
{
	// @todo Lumin: If we switch to Vk, we may need this when we build for both
}



// these are shared
FString FAndroidMisc::GetGPUFamily()
{
	return FString((const ANSICHAR*)glGetString(GL_RENDERER));
}

FString FAndroidMisc::GetGLVersion()
{
	return FString((const ANSICHAR*)glGetString(GL_VERSION));
}

bool FAndroidMisc::SupportsFloatingPointRenderTargets()
{
	// @todo Lumin: True?
	return true;
}

bool FAndroidMisc::SupportsShaderFramebufferFetch()
{
	// @todo Lumin: True?
	return true;
}

#endif
