// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#if	!PLATFORM_LUMINGL4

#include "OpenGLDrvPrivate.h"
#include "LuminEGL.h"
#include "Android/AndroidApplication.h"

LuminEGL* LuminEGL::Singleton = nullptr;
DEFINE_LOG_CATEGORY(LogEGL);

#define ENABLE_EGL_DEBUG 0

const  int EGLMinRedBits		= 5;
const  int EGLMinGreenBits		= 6;
const  int EGLMinBlueBits		= 5;
const  int EGLMinDepthBits		= 16;

const int EGLDesiredRedBits = 8;
const int EGLDesiredGreenBits = 8;
const int EGLDesiredBlueBits = 8;
const int EGLDesiredAlphaBits = 0;
const int EGLDesiredDepthBits = 24;
const int EGLDesiredStencilBits = 0;
const int EGLDesiredSampleBuffers = 0;
const int EGLDesiredSampleSamples = 0;

const EGLint Attributes[] = {
	EGL_RED_SIZE,       EGLMinRedBits,
	EGL_GREEN_SIZE,     EGLMinGreenBits,
	EGL_BLUE_SIZE,      EGLMinBlueBits,
	EGL_DEPTH_SIZE,     EGLMinDepthBits,
	EGL_NONE
};

struct LuminESPImpl
{
	FPlatformOpenGLContext	SharedContext;
	FPlatformOpenGLContext	RenderingContext;
	FPlatformOpenGLContext	SingleThreadedContext;

	EGLDisplay eglDisplay;
	EGLConfig eglConfigParam;
	EGLint eglWidth;
	EGLint eglHeight;
	EGLNativeWindowType Window;
	bool Initialized;
	EOpenGLCurrentContext CurrentContextType;
	GLuint OnScreenColorRenderBuffer;
	GLuint ResolveFrameBuffer;
	LuminESPImpl();
};

LuminESPImpl::LuminESPImpl():
eglDisplay(EGL_NO_DISPLAY)
,eglConfigParam(NULL)
,eglWidth(0)
,eglHeight(0)
,Window(NULL)
,Initialized(false)
,CurrentContextType(CONTEXT_Invalid)
,OnScreenColorRenderBuffer(0)
,ResolveFrameBuffer(0)
{
}

LuminEGL::LuminEGL()
	:	bSupportsKHRCreateContext(false)
	,	bSupportsKHRSurfacelessContext(false)
	,	ContextAttributes(nullptr)
{
	PImplData = new LuminESPImpl();
}

void LuminEGL::ResetDisplay()
{
	if(PImplData->eglDisplay != EGL_NO_DISPLAY)
	{
		eglMakeCurrent(PImplData->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		PImplData->CurrentContextType = CONTEXT_Invalid;
	}
}

void LuminEGL::TerminateEGL()
{

	eglTerminate(PImplData->eglDisplay);
	PImplData->eglDisplay = EGL_NO_DISPLAY;
	PImplData->Initialized = false;
}

/* Can be called from any thread */
EGLBoolean LuminEGL::SetCurrentContext(EGLContext InContext, EGLSurface InSurface)
{
	//context can be null.so can surface from PlatformNULLContextSetup
	EGLBoolean Result = EGL_FALSE;
	EGLContext CurrentContext = GetCurrentContext();

	// activate the context
	if (CurrentContext != InContext)
	{
		if (CurrentContext != EGL_NO_CONTEXT)
		{
			glFlush();
		}
		if (InContext == EGL_NO_CONTEXT)
		{
			ResetDisplay();
		}
		else
		{
			Result = eglMakeCurrent(PImplData->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, InContext);
			checkf(Result == EGL_TRUE, TEXT("ERROR: SetCurrentContext eglMakeCurrent failed : 0x%x"), eglGetError());
		}
	}
	return Result;
}

void LuminEGL::ResetInternal()
{
	Terminate();
}

void LuminEGL::InitEGL(APIVariant API)
{
	// make sure we only do this once (it's optionally done early for cooker communication)
	static bool bAlreadyInitialized = false;
	if (bAlreadyInitialized)
	{
		return;
	}
	bAlreadyInitialized = true;


	check(PImplData->eglDisplay == EGL_NO_DISPLAY);
	PImplData->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	checkf(PImplData->eglDisplay, TEXT(" eglGetDisplay error : 0x%x "), eglGetError());

	EGLBoolean result = eglInitialize(PImplData->eglDisplay, 0, 0);
	checkf(result == EGL_TRUE, TEXT("elgInitialize error: 0x%x "), eglGetError());

	// Get the EGL Extension list to determine what is supported
	FString Extensions = ANSI_TO_TCHAR(eglQueryString(PImplData->eglDisplay, EGL_EXTENSIONS));

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("EGL Extensions: \n%s"), *Extensions);

	bSupportsKHRCreateContext = Extensions.Contains(TEXT("EGL_KHR_create_context"));
	bSupportsKHRSurfacelessContext = Extensions.Contains(TEXT("EGL_KHR_surfaceless_context"));

	if (API == AV_OpenGLES)
	{
		result = eglBindAPI(EGL_OPENGL_ES_API);
	}
	else if (API == AV_OpenGLCore)
	{
		result = eglBindAPI(EGL_OPENGL_API);
	}
	else
	{
		checkf( 0, TEXT("Attempt to initialize EGL with unedpected API type"));
	}

	checkf(result == EGL_TRUE, TEXT("eglBindAPI error: 0x%x "), eglGetError());

	EGLint eglNumConfigs = 0;

	if ((result = eglGetConfigs(PImplData->eglDisplay, nullptr, 0, &eglNumConfigs)) == EGL_FALSE)
	{
		ResetInternal();
	}

	checkf(result == EGL_TRUE, TEXT("eglGetConfigs error: 0x%x"), eglGetError());

	EGLint eglNumVisuals = 0;
	EGLConfig EGLConfigList[eglNumConfigs];
	if (!(result = eglChooseConfig(PImplData->eglDisplay, Attributes, EGLConfigList, eglNumConfigs, &eglNumVisuals)))
	{
		ResetInternal();
	}

	checkf(result == EGL_TRUE, TEXT(" eglChooseConfig error: 0x%x"), eglGetError());

	checkf(eglNumVisuals != 0, TEXT(" eglChooseConfig results is 0 . error: 0x%x"), eglGetError());

	int ResultValue = 0;
	bool haveConfig = false;
	int64 score = LONG_MAX;
	for (int i = 0; i < eglNumVisuals; ++i)
	{
		int64 currScore = 0;
		int r, g, b, a, d, s, sb, sc, nvi;
		eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[i], EGL_RED_SIZE, &ResultValue); r = ResultValue;
		eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[i], EGL_GREEN_SIZE, &ResultValue); g = ResultValue;
		eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[i], EGL_BLUE_SIZE, &ResultValue); b = ResultValue;
		eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[i], EGL_ALPHA_SIZE, &ResultValue); a = ResultValue;
		eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[i], EGL_DEPTH_SIZE, &ResultValue); d = ResultValue;
		eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[i], EGL_STENCIL_SIZE, &ResultValue); s = ResultValue;
		eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[i], EGL_SAMPLE_BUFFERS, &ResultValue); sb = ResultValue;
		eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[i], EGL_SAMPLES, &ResultValue); sc = ResultValue;

		// Optional, Tegra-specific non-linear depth buffer, which allows for much better
		// effective depth range in relatively limited bit-depths (e.g. 16-bit)
		int bNonLinearDepth = 0;
		if (eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[i], EGL_DEPTH_ENCODING_NV, &ResultValue))
		{
			bNonLinearDepth = (ResultValue == EGL_DEPTH_ENCODING_NONLINEAR_NV) ? 1 : 0;
		}

		EGLint NativeVisualId = 0;
		eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[i], EGL_NATIVE_VISUAL_ID, &NativeVisualId);

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

#if ENABLE_EGL_DEBUG

			printf("EGLConfigInfo : Current Score: %lld\n", currScore);

			LogConfigInfo(EGLConfigList[i]);
#endif

			if (currScore < score || !haveConfig)
			{
				PImplData->eglConfigParam = EGLConfigList[i];
				haveConfig = true;
				score = currScore;
			}
		}
#if ENABLE_EGL_DEBUG
		else
		{
			printf("EGLConfigInfo : REJECTED CONFIG DOES NOT MATCH NATIVE VISUAL ID\n");

			LogConfigInfo(EGLConfigList[i]);
		}
#endif
	}

	check(haveConfig);

#if ENABLE_EGL_DEBUG
	printf("Selected EGLConfigInfo : Top Score: %lld\n", score);
	LogConfigInfo(PImplData->eglConfigParam);
#endif

}

LuminEGL* LuminEGL::GetInstance()
{
	if (!Singleton)
	{
		Singleton = new LuminEGL();
	}

	return Singleton;
}

void LuminEGL::DestroyBackBuffer()
{
	if(PImplData->ResolveFrameBuffer)
	{
		glDeleteFramebuffers(1, &PImplData->ResolveFrameBuffer);
		PImplData->ResolveFrameBuffer = 0 ;
	}
	if(PImplData->OnScreenColorRenderBuffer)
	{
		glDeleteRenderbuffers(1, &(PImplData->OnScreenColorRenderBuffer));
		PImplData->OnScreenColorRenderBuffer = 0;
	}
}

void LuminEGL::InitBackBuffer()
{
	//add check to see if any context was made current. 
	GLint OnScreenWidth, OnScreenHeight;
	PImplData->ResolveFrameBuffer = 0;
	PImplData->OnScreenColorRenderBuffer = 0;
	OnScreenWidth = PImplData->eglWidth;
	OnScreenHeight = PImplData->eglHeight;

	PImplData->RenderingContext.ViewportFramebuffer = GetResolveFrameBuffer();
	PImplData->SharedContext.ViewportFramebuffer = GetResolveFrameBuffer();
	PImplData->SingleThreadedContext.ViewportFramebuffer = GetResolveFrameBuffer();
}

void LuminEGL::Init(APIVariant API, uint32 MajorVersion, uint32 MinorVersion, bool bDebug)
{
	if (PImplData->Initialized)
	{
		return;
	}
	InitEGL(API);

	if (bSupportsKHRCreateContext)
	{
		const uint32 MaxElements = 13;
		uint32 Flags = 0;

		Flags |= bDebug ? EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR : 0;

		ContextAttributes = new int[MaxElements];
		uint32 Element = 0;

		ContextAttributes[Element++] = EGL_CONTEXT_MAJOR_VERSION_KHR;
		ContextAttributes[Element++] = MajorVersion;
		ContextAttributes[Element++] = EGL_CONTEXT_MINOR_VERSION_KHR;
		ContextAttributes[Element++] = MinorVersion;
		if (API == AV_OpenGLCore)
		{
			ContextAttributes[Element++] = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
			ContextAttributes[Element++] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
		}
		ContextAttributes[Element++] = EGL_CONTEXT_FLAGS_KHR;
		ContextAttributes[Element++] = Flags;
		ContextAttributes[Element++] = EGL_NONE;

		checkf( Element < MaxElements, TEXT("Too many elements in config list"));
	}
	else
	{
		// Fall back to the least common denominator
		ContextAttributes = new int[3];
		ContextAttributes[0] = EGL_CONTEXT_CLIENT_VERSION;
		ContextAttributes[1] = 2;
		ContextAttributes[2] = EGL_NONE;
	}

	InitContexts();
	PImplData->Initialized   = true;
}

LuminEGL::~LuminEGL()
{
	delete PImplData;
	delete []ContextAttributes;
}

void LuminEGL::GetDimensions(uint32& OutWidth, uint32& OutHeight)
{
	OutWidth = PImplData->eglWidth;
	OutHeight = PImplData->eglHeight;
}

void LuminEGL::DestroyContext(EGLContext InContext)
{
	if(InContext != EGL_NO_CONTEXT) //soft fail
	{
		eglDestroyContext(PImplData->eglDisplay, InContext);
	}
}

EGLContext LuminEGL::CreateContext(EGLContext InSharedContext)
{
	return eglCreateContext(PImplData->eglDisplay, PImplData->eglConfigParam,  InSharedContext, ContextAttributes);
}

int32 LuminEGL::GetError()
{
	return eglGetError();
}

bool LuminEGL::SwapBuffers()
{
	if (!eglSwapBuffers(PImplData->eglDisplay, EGL_NO_SURFACE))
	{
		// @todo lumin shutdown if swap buffering goes down
		// basic reporting
		if( eglGetError() == EGL_CONTEXT_LOST )
		{
			//Logger.LogOut("swapBuffers: EGL11.EGL_CONTEXT_LOST err: " + eglGetError());
			//Process.killProcess(Process.myPid());		//@todo android
		}
		return false;
	}
	return true;
}

bool LuminEGL::IsInitialized()
{
	return PImplData->Initialized;
}

GLuint LuminEGL::GetOnScreenColorRenderBuffer()
{
	return PImplData->OnScreenColorRenderBuffer;
}

GLuint LuminEGL::GetResolveFrameBuffer()
{
	return PImplData->ResolveFrameBuffer;
}

bool LuminEGL::IsCurrentContextValid()
{
	EGLContext eglContext = eglGetCurrentContext();
	return (eglContext != EGL_NO_CONTEXT);
}

EGLContext LuminEGL::GetCurrentContext()
{
	return eglGetCurrentContext();
}

EGLDisplay LuminEGL::GetDisplay()
{
	return PImplData->eglDisplay;
}

void* LuminEGL::GetNativeWindow()
{
	return PImplData->Window;
}

bool LuminEGL::InitContexts()
{
	bool Result = true; 

	PImplData->SharedContext.eglContext = CreateContext();

	PImplData->RenderingContext.eglContext = CreateContext(PImplData->SharedContext.eglContext);

	PImplData->SingleThreadedContext.eglContext = CreateContext();
	return Result;
}

void LuminEGL::SetCurrentSharedContext()
{
	check(IsInGameThread());
	PImplData->CurrentContextType = CONTEXT_Shared;

	if(GUseThreadedRendering)
	{
		SetCurrentContext(PImplData->SharedContext.eglContext, EGL_NO_SURFACE);
	}
	else
	{
		SetCurrentContext(PImplData->SingleThreadedContext.eglContext, EGL_NO_SURFACE);
	}
}

void LuminEGL::SetSharedContext()
{
	check(IsInGameThread());
	PImplData->CurrentContextType = CONTEXT_Shared;

	SetCurrentContext(PImplData->SharedContext.eglContext, EGL_NO_SURFACE);
}

void LuminEGL::SetSingleThreadRenderingContext()
{
	PImplData->CurrentContextType = CONTEXT_Rendering;
	SetCurrentContext(PImplData->SingleThreadedContext.eglContext, EGL_NO_SURFACE);
}

void LuminEGL::SetMultithreadRenderingContext()
{
	PImplData->CurrentContextType = CONTEXT_Rendering;
	SetCurrentContext(PImplData->RenderingContext.eglContext, EGL_NO_SURFACE);
}

void LuminEGL::SetCurrentRenderingContext()
{
	PImplData->CurrentContextType = CONTEXT_Rendering;
	if(GUseThreadedRendering)
	{
		SetCurrentContext(PImplData->RenderingContext.eglContext, EGL_NO_SURFACE);
	}
	else
	{
		SetCurrentContext(PImplData->SingleThreadedContext.eglContext, EGL_NO_SURFACE);
	}
}

void LuminEGL::Terminate()
{
	ResetDisplay();
	DestroyContext(PImplData->SharedContext.eglContext);
	PImplData->SharedContext.Reset();
	DestroyContext(PImplData->RenderingContext.eglContext);
	PImplData->RenderingContext.Reset();
	DestroyContext(PImplData->SingleThreadedContext.eglContext);
	PImplData->SingleThreadedContext.Reset();
	TerminateEGL();
}

uint32_t LuminEGL::GetCurrentContextType()
{
	if (GUseThreadedRendering)
	{
		EGLContext CurrentContext = GetCurrentContext();
		if (CurrentContext == PImplData->RenderingContext.eglContext)
		{
			return CONTEXT_Rendering;
		}
		else if (CurrentContext == PImplData->SharedContext.eglContext)
		{
			return CONTEXT_Shared;
		}
		else if (CurrentContext != EGL_NO_CONTEXT)
		{
			return CONTEXT_Other;
		}
	}
	else
	{
		return CONTEXT_Shared;//make sure current context is valid one. //check(GetCurrentContext != nullptr);
	}

	return CONTEXT_Invalid;
}

FPlatformOpenGLContext* LuminEGL::GetRenderingContext()
{
	if(GUseThreadedRendering)
	{
		return &PImplData->RenderingContext;
	}
	else
	{
		return &PImplData->SingleThreadedContext;
	}
}


 void LuminEGL::LogConfigInfo(EGLConfig  EGLConfigInfo)
{
#if ENABLE_EGL_DEBUG
	EGLint ResultValue = 0 ;
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_RED_SIZE, &ResultValue); printf("EGLConfigInfo : EGL_RED_SIZE :	%u\n" , ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_GREEN_SIZE, &ResultValue);  printf("EGLConfigInfo :EGL_GREEN_SIZE :	%u\n" , ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_BLUE_SIZE, &ResultValue);  printf("EGLConfigInfo :EGL_BLUE_SIZE :	%u\n" , ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_ALPHA_SIZE, &ResultValue); printf("EGLConfigInfo :EGL_ALPHA_SIZE :	%u\n" , ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_DEPTH_SIZE, &ResultValue);  printf("EGLConfigInfo :EGL_DEPTH_SIZE :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_STENCIL_SIZE, &ResultValue);  printf("EGLConfigInfo :EGL_STENCIL_SIZE :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_SAMPLE_BUFFERS, &ResultValue);  printf("EGLConfigInfo :EGL_SAMPLE_BUFFERS :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_BIND_TO_TEXTURE_RGB, &ResultValue);  printf("EGLConfigInfo :EGL_BIND_TO_TEXTURE_RGB :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_SAMPLES, &ResultValue);  printf("EGLConfigInfo :EGL_SAMPLES :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_COLOR_BUFFER_TYPE, &ResultValue);  printf("EGLConfigInfo :EGL_COLOR_BUFFER_TYPE :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_CONFIG_CAVEAT, &ResultValue);  printf("EGLConfigInfo :EGL_CONFIG_CAVEAT :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_CONFIG_ID, &ResultValue); printf("EGLConfigInfo :EGL_CONFIG_ID :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_CONFORMANT, &ResultValue);  printf("EGLConfigInfo :EGL_CONFORMANT :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_LEVEL, &ResultValue);  printf("EGLConfigInfo :EGL_LEVEL :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_LUMINANCE_SIZE, &ResultValue);  printf("EGLConfigInfo :EGL_LUMINANCE_SIZE :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_MAX_SWAP_INTERVAL, &ResultValue);  printf("EGLConfigInfo :EGL_MAX_SWAP_INTERVAL :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_MIN_SWAP_INTERVAL, &ResultValue);  printf("EGLConfigInfo :EGL_MIN_SWAP_INTERVAL :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_NATIVE_RENDERABLE, &ResultValue);  printf("EGLConfigInfo :EGL_NATIVE_RENDERABLE :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_NATIVE_VISUAL_TYPE, &ResultValue);  printf("EGLConfigInfo :EGL_NATIVE_VISUAL_TYPE :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_NATIVE_VISUAL_ID, &ResultValue);  printf("EGLConfigInfo :EGL_NATIVE_VISUAL_ID :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_RENDERABLE_TYPE, &ResultValue);  printf("EGLConfigInfo :EGL_RENDERABLE_TYPE :	%u\n", ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_SURFACE_TYPE, &ResultValue);  printf("EGLConfigInfo :EGL_SURFACE_TYPE :	%u\n", ResultValue );
#endif
 }

FScopeContext::FScopeContext(FPlatformOpenGLContext* PlatformContext)
{
	check(PlatformContext);
	LastContext = eglGetCurrentContext();
	bSameContext = (LastContext == PlatformContext->eglContext);
	if (!bSameContext)
	{
		eglMakeCurrent(LuminEGL::GetInstance()->GetDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, PlatformContext->eglContext);
	}
}

FScopeContext::~FScopeContext(void)
{
	if (!bSameContext)
	{
		if (LastContext)
		{
			eglMakeCurrent(LuminEGL::GetInstance()->GetDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, LastContext);
		}
		else
		{
			eglMakeCurrent(LuminEGL::GetInstance()->GetDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		}
	}
}

#endif	// !PLATFORM_ANDROIDGL
