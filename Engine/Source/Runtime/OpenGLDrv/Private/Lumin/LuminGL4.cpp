// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if PLATFORM_LUMINGL4

/*=============================================================================
OpenGLWindowsLoader.cpp: Manual loading of OpenGL functions from DLL.
=============================================================================*/

#include "Lumin/LuminGL4.h"
#include "Misc/ScopeLock.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"
#include "ComponentReregisterContext.h"

#include "Android/AndroidApplication.h"

//Desired settings come from android
const int EGLMinRedBits = 5;
const int EGLMinGreenBits = 6;
const int EGLMinBlueBits = 5;
const int EGLMinDepthBits = 16;
const int EGLDesiredRedBits = 8;
const int EGLDesiredGreenBits = 8;
const int EGLDesiredBlueBits = 8;
const int EGLDesiredAlphaBits = 0;
const int EGLDesiredDepthBits = 24;
const int EGLDesiredStencilBits = 0;
const int EGLDesiredSampleBuffers = 0;
const int EGLDesiredSampleSamples = 0;


/*------------------------------------------------------------------------------
OpenGL function pointers.
------------------------------------------------------------------------------*/
#define DEFINE_GL_ENTRYPOINTS(Type,Func) Type Func = NULL;
namespace GLFuncPointers	// see explanation in OpenGLLinux.h why we need the namespace
{
	ENUM_GL_ENTRYPOINTS_ALL(DEFINE_GL_ENTRYPOINTS);
};

/*------------------------------------------------------------------------------
OpenGL context management.
------------------------------------------------------------------------------*/
static EGLDisplay LuminDefaultDisplay = 0;

/* Can be called from any thread */
static void ContextMakeCurrent(EGLDisplay InDisplay, EGLContext InContext)
{
	EGLBoolean Result = EGL_FALSE;
	Result = eglMakeCurrent(InDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, InContext);
	if (Result != EGL_TRUE)
	{
		UE_LOG(LogRHI, Warning, TEXT("ERROR: ContextMakeCurrent eglMakeCurrent failed : 0x%x"), eglGetError());
	}
}

static EGLContext GetCurrentContext()
{
	return eglGetCurrentContext();
}

static EGLDisplay GetCurrentDisplay()
{
	return LuminDefaultDisplay;
}

/** Platform specific OpenGL context. */
struct FPlatformOpenGLContext

{
	EGLDisplay DisplayConnection;
	EGLContext OpenGLContext;

	int32 SyncInterval;
	GLuint	VertexArrayObject;	// one has to be generated and set for each context (OpenGL 3.2 Core requirements)
};


class FScopeContext
{
public:
	FScopeContext(FPlatformOpenGLContext* Context)
	{
		check(Context);
		PreDisplay = GetCurrentDisplay();
		PreGLContext = GetCurrentContext();

		bSameDisplayAndContext = (PreGLContext == Context->OpenGLContext) && (PreDisplay == Context->DisplayConnection);
		if (!bSameDisplayAndContext)
		{
			// no need to glFlush() on Lumin, it does flush by itself before switching contexts
			ContextMakeCurrent(Context->DisplayConnection, Context->OpenGLContext);
		}
	}

	~FScopeContext(void)
	{
		if (!bSameDisplayAndContext)
		{
			if (PreGLContext)
			{
				ContextMakeCurrent(PreDisplay, PreGLContext);
			}
			else
			{
				EGLDisplay DefaultDisplay = LuminDefaultDisplay;
				ContextMakeCurrent(DefaultDisplay, EGL_NO_CONTEXT);
			}
		}
	}

	bool ContextWasAlreadyActive() const
	{
		return bSameDisplayAndContext;
	}

private:
	EGLDisplay			PreDisplay;
	EGLContext			PreGLContext;		//	this is a pointer, (void*)
	bool				bSameDisplayAndContext;
};

void DeleteQueriesForCurrentContext(EGLContext Context);

/**
* Create a dummy window used to construct OpenGL contexts.
*/
static void PlatformCreateDummyGLWindow(FPlatformOpenGLContext *OutContext)
{	
	EGLDisplay DefaultDisplay = LuminDefaultDisplay;
	checkf(DefaultDisplay, TEXT(" eglGetDisplay error : 0x%x "), eglGetError());
	
	EGLint OutMajorVersion = 4, OutMinorVersion = 0;
	EGLBoolean Result = eglInitialize(DefaultDisplay, &OutMajorVersion, &OutMinorVersion);
	checkf(Result == EGL_TRUE, TEXT("elgInitialize error: 0x%x "), eglGetError());

	OutContext->DisplayConnection = DefaultDisplay;
}

static bool PlatformOpenGL3()
{
	return FParse::Param(FCommandLine::Get(), TEXT("opengl3"));
}

static bool PlatformOpenGL4()
{
	return FParse::Param(FCommandLine::Get(), TEXT("opengl4"));
}

static void PlatformOpenGLVersionFromCommandLine(int& OutMajorVersion, int& OutMinorVersion)
{
	// Lumin GL4 determines OpenGL Context version based on command line arguments
	bool bGL3 = PlatformOpenGL3();
	bool bGL4 = PlatformOpenGL4();
	if (!bGL3 && !bGL4)
	{
		// Defaults to GL4.3(SM5 feature level) if no command line arguments are passed in 
		bGL4 = true;
	}

	if (bGL3)
	{
		OutMajorVersion = 3;
		OutMinorVersion = 2;
	}
	else if (bGL4)
	{
		OutMajorVersion = 4;
		OutMinorVersion = 3;
	}
	else
	{
		verifyf(false, TEXT("OpenGLRHI initialized with invalid command line, must be one of: -opengl3, -opengl4"));
	}
}

/**
* Enable/Disable debug context from the commandline
*/

static bool PlatformOpenGLDebugCtx()
{
	
#if UE_BUILD_DEBUG
	return !FParse::Param(FCommandLine::Get(), TEXT("openglNoDebug"));
#else
	return FParse::Param(FCommandLine::Get(), TEXT("openglDebug"));;
#endif
	return true;
}

/**
* Create a core profile OpenGL context.
*/
static void PlatformCreateOpenGLContextCore(FPlatformOpenGLContext* OutContext, int MajorVersion, int MinorVersion, EGLContext InParentContext)
{
	check(OutContext);
	check(OutContext->DisplayConnection);

	OutContext->SyncInterval = -1;	// invalid value to enforce setup on first buffer swap

	// use desktop GL
	eglBindAPI(EGL_OPENGL_API);

	EGLBoolean result = EGL_FALSE;
	EGLint eglNumConfigs = 0;
	if ((result = eglGetConfigs(OutContext->DisplayConnection, nullptr, 0, &eglNumConfigs)) == EGL_FALSE)
	{
		//should never happen
		check(0);
	}

	checkf(result == EGL_TRUE, TEXT("eglGetConfigs error: 0x%x"), eglGetError());

	EGLint Attributes[] = {
		EGL_DEPTH_SIZE, EGLMinDepthBits,
		EGL_RED_SIZE, EGLMinRedBits,
		EGL_GREEN_SIZE, EGLMinGreenBits,
		EGL_BLUE_SIZE, EGLMinBlueBits,
		EGL_NONE
	};

	EGLint eglNumVisuals = 0;
	EGLConfig* EGLConfigList = NULL;
	EGLConfigList = new EGLConfig[eglNumConfigs];
	if (!(result = eglChooseConfig(OutContext->DisplayConnection, Attributes, EGLConfigList, eglNumConfigs, &eglNumVisuals)))
	{
		//should never happen
		check(0);
	}
	
	checkf(result == EGL_TRUE, TEXT(" eglChooseConfig error: 0x%x"), eglGetError());

	EGLConfig egl_config = 0;
	int ResultValue = 0;
	bool haveConfig = false;
	int64 score = LONG_MAX;
	for (int i = 0; i < eglNumVisuals; ++i)
	{
		int64 currScore = 0;
		int r, g, b, a, d, s, sb, sc, nvi;
		eglGetConfigAttrib(OutContext->DisplayConnection, EGLConfigList[i], EGL_RED_SIZE, &ResultValue); r = ResultValue;
		eglGetConfigAttrib(OutContext->DisplayConnection, EGLConfigList[i], EGL_GREEN_SIZE, &ResultValue); g = ResultValue;
		eglGetConfigAttrib(OutContext->DisplayConnection, EGLConfigList[i], EGL_BLUE_SIZE, &ResultValue); b = ResultValue;
		eglGetConfigAttrib(OutContext->DisplayConnection, EGLConfigList[i], EGL_ALPHA_SIZE, &ResultValue); a = ResultValue;
		eglGetConfigAttrib(OutContext->DisplayConnection, EGLConfigList[i], EGL_DEPTH_SIZE, &ResultValue); d = ResultValue;
		eglGetConfigAttrib(OutContext->DisplayConnection, EGLConfigList[i], EGL_STENCIL_SIZE, &ResultValue); s = ResultValue;
		eglGetConfigAttrib(OutContext->DisplayConnection, EGLConfigList[i], EGL_SAMPLE_BUFFERS, &ResultValue); sb = ResultValue;
		eglGetConfigAttrib(OutContext->DisplayConnection, EGLConfigList[i], EGL_SAMPLES, &ResultValue); sc = ResultValue;

		// Optional, Tegra-specific non-linear depth buffer, which allows for much better
		// effective depth range in relatively limited bit-depths (e.g. 16-bit)
		int bNonLinearDepth = 0;
		if (eglGetConfigAttrib(OutContext->DisplayConnection, EGLConfigList[i], EGL_DEPTH_ENCODING_NV, &ResultValue))
		{
			bNonLinearDepth = (ResultValue == EGL_DEPTH_ENCODING_NONLINEAR_NV) ? 1 : 0;
		}

		EGLint NativeVisualId = 0;
		eglGetConfigAttrib(OutContext->DisplayConnection, EGLConfigList[i], EGL_NATIVE_VISUAL_ID, &NativeVisualId);

		if (NativeVisualId > 0)
		{
			// Favor EGLConfigLists by RGB, then Depth, then Non-linear Depth, then Stencil, then Alpha
			currScore = 0;
			currScore |= ((int64)FPlatformMath::Min(FPlatformMath::Abs(sb - EGLDesiredSampleBuffers), 15)) << 29;
			currScore |= ((int64)FPlatformMath::Min(FPlatformMath::Abs(sc - EGLDesiredSampleSamples), 31)) << 24;
			currScore |= FPlatformMath::Min(
				FPlatformMath::Abs(r - EGLDesiredRedBits) +
				FPlatformMath::Abs(g - EGLDesiredGreenBits) +
				FPlatformMath::Abs(b - EGLDesiredBlueBits), 127) << 17;
			currScore |= FPlatformMath::Min(FPlatformMath::Abs(d - EGLDesiredDepthBits), 63) << 11;
			currScore |= FPlatformMath::Min(FPlatformMath::Abs(1 - bNonLinearDepth), 1) << 10;
			currScore |= FPlatformMath::Min(FPlatformMath::Abs(s - EGLDesiredStencilBits), 31) << 6;
			currScore |= FPlatformMath::Min(FPlatformMath::Abs(a - EGLDesiredAlphaBits), 31) << 0;

			if (currScore < score || !haveConfig)
			{
				egl_config = EGLConfigList[i];
				haveConfig = true;
				score = currScore;
			}
		}
	}
	check(haveConfig);
	delete[] EGLConfigList;
	
	// Check required extensions
	// Get the EGL Extension list to determine what is supported
	FString Extensions = ANSI_TO_TCHAR(eglQueryString(OutContext->DisplayConnection, EGL_EXTENSIONS));
	
	// Debug output all supported extensions
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("EGL Extensions: \n%s"), *Extensions);
	
	// Make sure EGL_KHR_create_context is supported
	bool bSupportsKHRCreateContext = Extensions.Contains(TEXT("EGL_KHR_create_context"));
	if (!bSupportsKHRCreateContext)
	{
		// If EGL_KHR_create_context is missing, we can be sure that Lumin GL4 is not supported. Let's assert fail here..
		UE_LOG(LogRHI, Error, TEXT("Lumin OpenGL4 not supported by driver: EGL_KHR_create_context is missing"));
		check(0);
	}

	// Configure Debug flag
	int DebugFlag = 0;
	if (PlatformOpenGLDebugCtx())
	{
		DebugFlag = EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
	}

	EGLint ContextAttributes[] = {
		EGL_CONTEXT_MAJOR_VERSION_KHR, MajorVersion,
		EGL_CONTEXT_MINOR_VERSION_KHR, MinorVersion,
		EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
		EGL_CONTEXT_PRIORITY_LEVEL_IMG, EGL_CONTEXT_PRIORITY_MEDIUM_IMG,
		EGL_CONTEXT_FLAGS_KHR, DebugFlag,
		EGL_NONE
	};

	OutContext->OpenGLContext = eglCreateContext(OutContext->DisplayConnection, egl_config, InParentContext, ContextAttributes);
}

void PlatformReleaseOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context);

extern void OnQueryInvalidation(void);

/** Platform specific OpenGL device. */
struct FPlatformOpenGLDevice
{
	FPlatformOpenGLContext	SharedContext;
	FPlatformOpenGLContext	RenderingContext;
	TArray<FPlatformOpenGLContext*>	ViewportContexts;
	bool					TargetDirty;

	/** Guards against operating on viewport contexts from more than one thread at the same time. */
	FCriticalSection*		ContextUsageGuard;

	FPlatformOpenGLDevice()
		: TargetDirty(true)
	{
		extern void InitDebugContext();
		ContextUsageGuard = new FCriticalSection;

		int MajorVersion = 0;
		int MinorVersion = 0;
		PlatformOpenGLVersionFromCommandLine(MajorVersion, MinorVersion);

		PlatformCreateDummyGLWindow(&SharedContext);
		PlatformCreateOpenGLContextCore(&SharedContext, MajorVersion, MinorVersion, NULL);
		check(SharedContext.OpenGLContext)
		{
			FScopeContext ScopeContext(&SharedContext);
			InitDebugContext();
			glGenVertexArrays(1, &SharedContext.VertexArrayObject);
			glBindVertexArray(SharedContext.VertexArrayObject);
			InitDefaultGLContextState();
		}

		PlatformCreateDummyGLWindow(&RenderingContext);
		PlatformCreateOpenGLContextCore(&RenderingContext, MajorVersion, MinorVersion, SharedContext.OpenGLContext);
		check(RenderingContext.OpenGLContext);
		{
			FScopeContext ScopeContext(&RenderingContext);
			InitDebugContext();
			glGenVertexArrays(1, &RenderingContext.VertexArrayObject);
			glBindVertexArray(RenderingContext.VertexArrayObject);
			InitDefaultGLContextState();
		}

		ContextMakeCurrent(SharedContext.DisplayConnection, SharedContext.OpenGLContext);
	}

	~FPlatformOpenGLDevice()
	{
		check(ViewportContexts.Num() == 0);

		ContextMakeCurrent(NULL, NULL);

		// Inform all queries about the need to recreate themselves after OpenGL context they're in gets deleted
		OnQueryInvalidation();
		// this gets cleaned up in OpenGLViewport.cpp/~FOpenGLViewport()/line 249
		//PlatformReleaseOpenGLContext(this, &RenderingContext);
		PlatformReleaseOpenGLContext(this, &SharedContext);

		delete ContextUsageGuard;
	}
};

FPlatformOpenGLDevice* PlatformCreateOpenGLDevice()
{
	LuminDefaultDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	FPlatformOpenGLDevice* Device = new FPlatformOpenGLDevice();
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

FPlatformOpenGLContext* PlatformGetOpenGLRenderingContext(FPlatformOpenGLDevice* Device)
{
	return &Device->RenderingContext;
}

/**
* Create an OpenGL context.
*/
FPlatformOpenGLContext* PlatformCreateOpenGLContext(FPlatformOpenGLDevice* Device, void* InWindowHandle)
{
#if 1
	return PlatformGetOpenGLRenderingContext(Device);
#else
	static bool bInitializedWindowClass = false;

	EGLDisplay DefaultDisplay = LuminDefaultDisplay;
	checkf(DefaultDisplay, TEXT(" eglGetDisplay error : 0x%x "), eglGetError());

	Device->TargetDirty = true;
	FPlatformOpenGLContext* Context = new FPlatformOpenGLContext;
	Context->DisplayConnection = DefaultDisplay;
	check(Context->DisplayConnection);

	int MajorVersion = 0;
	int MinorVersion = 0;
	PlatformOpenGLVersionFromCommandLine(MajorVersion, MinorVersion);

	PlatformCreateOpenGLContextCore(Context, MajorVersion, MinorVersion, Device->SharedContext.OpenGLContext);
	check(Context->OpenGLContext);
	{
		FScopeContext Scope(Context);
		InitDefaultGLContextState();
	}

	Device->ViewportContexts.Add(Context);
	return Context;
#endif
}

/**
* Release an OpenGL context.
*/
void PlatformReleaseOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
	check(Context && Context->OpenGLContext);

	Device->ViewportContexts.RemoveSingle(Context);
	Device->TargetDirty = true;

	bool bActiveContextWillBeReleased = false;

	{
		FScopeLock ScopeLock(Device->ContextUsageGuard);
		{
			FScopeContext ScopeContext(Context);

			bActiveContextWillBeReleased = ScopeContext.ContextWasAlreadyActive();

			DeleteQueriesForCurrentContext(Context->OpenGLContext);
			glBindVertexArray(0);
			glDeleteVertexArrays(1, &Context->VertexArrayObject);
		}

		eglDestroyContext(Context->DisplayConnection, Context->OpenGLContext);
		Context->OpenGLContext = NULL;
	}

	if (bActiveContextWillBeReleased)
	{
		EGLDisplay DefaultDisplay = LuminDefaultDisplay;
		ContextMakeCurrent(DefaultDisplay, EGL_NO_CONTEXT);
	}
}

/**
* Destroy an OpenGL context.
*/
void PlatformDestroyOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
	PlatformReleaseOpenGLContext(Device, Context);
}

void* PlatformGetWindow(FPlatformOpenGLContext* Context, void** AddParam)
{
	return NULL;
}

/**
* Main function for transferring data to on-screen buffers.
*/
bool PlatformBlitToViewport(FPlatformOpenGLDevice* Device,
	const FOpenGLViewport& Viewport,
	uint32 BackbufferSizeX,
	uint32 BackbufferSizeY,
	bool bPresent,
	bool bLockToVsync,
	int32 SyncInterval)
{
	FScopeLock ScopeLock(Device->ContextUsageGuard);
	{
		FPlatformOpenGLContext* const Context = Viewport.GetGLContext();
		check(Context && Context->OpenGLContext);
		FScopeContext ScopeContext(Context);
		if (bPresent && Viewport.GetCustomPresent())
		{
			// Commented out becuase we try to match Lumin ES2 standard here. This does not match Windows/Linux though
			// glDisable(GL_FRAMEBUFFER_SRGB);
			bPresent = Viewport.GetCustomPresent()->Present(SyncInterval);
			// glEnable(GL_FRAMEBUFFER_SRGB);
		}
	}
	return bPresent;
}

void PlatformFlushIfNeeded()
{
	glFinish();
}

void PlatformRebindResources(FPlatformOpenGLDevice* Device)
{
	// @todo: Figure out if we need to rebind frame & renderbuffers after switching contexts

}

void PlatformRenderingContextSetup(FPlatformOpenGLDevice* Device)
{
	check(Device && Device->RenderingContext.DisplayConnection && Device->RenderingContext.OpenGLContext);

	if (GetCurrentContext())
	{
		glFlush();
	}
	if (Device->ViewportContexts.Num() == 1)
	{
		ContextMakeCurrent(Device->ViewportContexts[0]->DisplayConnection, Device->RenderingContext.OpenGLContext);
	}
	else
	{
		ContextMakeCurrent(Device->RenderingContext.DisplayConnection, Device->RenderingContext.OpenGLContext);
	}

}

void PlatformSharedContextSetup(FPlatformOpenGLDevice* Device)
{
	check(Device && Device->SharedContext.DisplayConnection && Device->SharedContext.OpenGLContext);
	// no need to glFlush() on Lumin
	// If the calling thread has already a current rendering context, that context is flushed and marked as no longer current.
	ContextMakeCurrent(Device->SharedContext.DisplayConnection, Device->SharedContext.OpenGLContext);
}


void PlatformNULLContextSetup()
{
	EGLDisplay DefaultDisplay = LuminDefaultDisplay;
	ContextMakeCurrent(DefaultDisplay, EGL_NO_CONTEXT);
}

/**
* Resize the GL context.
*/
void PlatformResizeGLContext(FPlatformOpenGLDevice* Device,
	FPlatformOpenGLContext* Context,
	uint32 SizeX, uint32 SizeY,
	bool bFullscreen,
	bool bWasFullscreen,
	GLenum BackBufferTarget,
	GLuint BackBufferResource)
{
	check(Context);

	glViewport(0, 0, SizeX, SizeY);
	VERIFY_GL(glViewport);
	return;
}

void PlatformGetSupportedResolution(uint32 &Width, uint32 &Height)
{
	// should never be called
	UE_LOG(LogRHI, Warning, TEXT("Warning: PlatformGetSupportedResolution(Not implemented) gets called"));
	Width = 0;
	Height = 0;
	return;
}


bool PlatformGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	// should never be called
	UE_LOG(LogRHI, Warning, TEXT("Warning: PlatformGetAvailableResolutions(Not implemented) gets called"));
	return false;
}

void PlatformRestoreDesktopDisplayMode()
{
	// should never be called
	UE_LOG(LogRHI, Warning, TEXT("Warning: PlatformRestoreDesktopDisplayMode(Not implemented) gets called"));
	return;
}

bool PlatformInitOpenGL()
{
	static bool bInitialized = false;
	if (!bInitialized)
	{
		//query funciton pointers here
		bool bFoundAllEntryPoints = true;

#define GET_GL_ENTRYPOINTS(Type,Func) Func = (Type)eglGetProcAddress(#Func);
#define CHECK_GL_ENTRYPOINTS_NULL(Type,Func) if (Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogRHI, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }
#define CHECK_GL_ENTRYPOINTS_OK(Type,Func) if (Func != NULL) {UE_LOG(LogRHI, Warning, TEXT("OK to find entry point for %s"), TEXT(#Func)); }

		ENUM_GL_ENTRYPOINTS_ALL(GET_GL_ENTRYPOINTS);
		ENUM_GL_ENTRYPOINTS_ALL(CHECK_GL_ENTRYPOINTS_NULL);
		ENUM_GL_ENTRYPOINTS_ALL(CHECK_GL_ENTRYPOINTS_OK);
		bInitialized = true;
	}
	return bInitialized;
}

bool PlatformOpenGLContextValid()
{
	return(GetCurrentContext() != NULL);
}

int32 PlatformGlGetError()
{
	return glGetError();
}

EOpenGLCurrentContext PlatformOpenGLCurrentContext(FPlatformOpenGLDevice* Device)
{
	EGLContext GLContext = GetCurrentContext();

	if (GLContext == Device->RenderingContext.OpenGLContext)	// most common case
	{
		return CONTEXT_Rendering;
	}
	else if (GLContext == Device->SharedContext.OpenGLContext)
	{
		return CONTEXT_Shared;
	}
	else if (GLContext)
	{
		return CONTEXT_Other;
	}
	return CONTEXT_Invalid;
}

// mlchanges begin
void* PlatformOpenGLCurrentContextHandle(FPlatformOpenGLDevice* Device)
{
	return GetCurrentContext();
}
// mlchanges end

void PlatformGetBackbufferDimensions(uint32& OutWidth, uint32& OutHeight)
{
	return;
}


struct FOpenGLReleasedQuery
{
	EGLContext	eglContext;
	GLuint	Query;
};

static TArray<FOpenGLReleasedQuery>	ReleasedQueries;
static FCriticalSection* ReleasedQueriesGuard;

void PlatformGetNewRenderQuery(GLuint* OutQuery, uint64* OutQueryContext)
{
	if (!ReleasedQueriesGuard)
	{
		ReleasedQueriesGuard = new FCriticalSection;
	}

	{
		FScopeLock Lock(ReleasedQueriesGuard);

		EGLContext Context = GetCurrentContext();
		check(Context);

		GLuint NewQuery = 0;

		// Check for possible query reuse
		const int32 ArraySize = ReleasedQueries.Num();
		for (int32 Index = 0; Index < ArraySize; ++Index)
		{
			if (ReleasedQueries[Index].eglContext == Context)
			{
				NewQuery = ReleasedQueries[Index].Query;
				ReleasedQueries.RemoveAtSwap(Index);
				break;
			}
		}

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
	// implent this otherwise will fail at assert
	EGLContext Context = GetCurrentContext();
	if ((uint64)Context == QueryContext)
	{
		FOpenGL::DeleteQueries(1, &Query);
	}
	else
	{
		FScopeLock Lock(ReleasedQueriesGuard);

		FOpenGLReleasedQuery ReleasedQuery;
		ReleasedQuery.eglContext = (EGLContext)QueryContext;
		ReleasedQuery.Query = Query;
		ReleasedQueries.Add(ReleasedQuery);
	}
}

void DeleteQueriesForCurrentContext(EGLContext Context)
{
	if (!ReleasedQueriesGuard)
	{
		ReleasedQueriesGuard = new FCriticalSection;
	}

	{
		FScopeLock Lock(ReleasedQueriesGuard);
		for (int32 Index = 0; Index < ReleasedQueries.Num(); ++Index)
		{
			if (ReleasedQueries[Index].eglContext == Context)
			{
				FOpenGL::DeleteQueries(1, &ReleasedQueries[Index].Query);
				ReleasedQueries.RemoveAtSwap(Index);
				--Index;
			}
		}
	}
}

bool PlatformContextIsCurrent(uint64 QueryContext)
{
	return (uint64)GetCurrentContext() == QueryContext;
}

FRHITexture* PlatformCreateBuiltinBackBuffer(FOpenGLDynamicRHI* OpenGLRHI, uint32 SizeX, uint32 SizeY)
{
	return NULL;
}

bool FLuminOpenGL4::bSupportsBindlessTexture = false;

void FLuminOpenGL4::ProcessExtensions(const FString& ExtensionsString)
{
	FOpenGL4::ProcessExtensions(ExtensionsString);

	bSupportsBindlessTexture = ExtensionsString.Contains(TEXT("GL_NV_bindless_texture"));
}

void FAndroidAppEntry::PlatformInit()
{
	// @todo double check this funciton: PlatformInitOpenGL is doing the initilization work, thus we can leave this function empty 
	return;
}

void FAndroidAppEntry::ReleaseEGL()
{
	// @todo Lumin: If we switch to Vk, we may need this when we build for both
}

FString FAndroidMisc::GetGPUFamily()
{
	return FString(TEXT("Lumin"));// (const ANSICHAR*)glGetString(GL_RENDERER));
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
