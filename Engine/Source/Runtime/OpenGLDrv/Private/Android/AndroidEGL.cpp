// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidPlatform.h"

#if USE_ANDROID_OPENGL

#include "OpenGLDrvPrivate.h"
#include "AndroidEGL.h"
#include "Android/AndroidApplication.h"
#include "Android/AndroidWindow.h"
#include <android/native_window.h>
#if USE_ANDROID_JNI
#include <android/native_window_jni.h>
#endif
#include "OpenGLDrvPrivate.h"


AndroidEGL* AndroidEGL::Singleton = NULL;
DEFINE_LOG_CATEGORY(LogEGL);

static TAutoConsoleVariable<int32> CVarAllowFrameTimestamps(
	TEXT("a.AllowFrameTimestamps"),
	1,
	TEXT("True to allow the use use eglGetFrameTimestampsANDROID et al for frame pacing or spew."));

#define ENABLE_CONFIG_FILTER 1
#define ENABLE_EGL_DEBUG 0
#define ENABLE_VERIFY_EGL 0
#define ENABLE_VERIFY_EGL_TRACE 0

#if ENABLE_VERIFY_EGL

#define VERIFY_EGL(msg) { VerifyEGLResult(eglGetError(),TEXT(#msg),TEXT(""),TEXT(__FILE__),__LINE__); }

void VerifyEGLResult(EGLint ErrorCode, const TCHAR* Msg1, const TCHAR* Msg2, const TCHAR* Filename, uint32 Line)
{
	if (ErrorCode != EGL_SUCCESS)
	{
		static const TCHAR* EGLErrorStrings[] =
		{
			TEXT("EGL_NOT_INITIALIZED"),
			TEXT("EGL_BAD_ACCESS"),
			TEXT("EGL_BAD_ALLOC"),
			TEXT("EGL_BAD_ATTRIBUTE"),
			TEXT("EGL_BAD_CONFIG"),
			TEXT("EGL_BAD_CONTEXT"),
			TEXT("EGL_BAD_CURRENT_SURFACE"),
			TEXT("EGL_BAD_DISPLAY"),
			TEXT("EGL_BAD_MATCH"),
			TEXT("EGL_BAD_NATIVE_PIXMAP"),
			TEXT("EGL_BAD_NATIVE_WINDOW"),
			TEXT("EGL_BAD_PARAMETER"),
			TEXT("EGL_BAD_SURFACE"),
			TEXT("EGL_CONTEXT_LOST"),
			TEXT("UNKNOWN EGL ERROR")
		};

		uint32 ErrorIndex = FMath::Min<uint32>(ErrorCode - EGL_SUCCESS, ARRAY_COUNT(EGLErrorStrings) - 1);
		UE_LOG(LogRHI, Warning, TEXT("%s(%u): %s%s failed with error %s (0x%x)"),
			Filename, Line, Msg1, Msg2, EGLErrorStrings[ErrorIndex], ErrorCode);
		check(0);
	}
}

class FEGLErrorScope
{
public:
	FEGLErrorScope(
		const TCHAR* InFunctionName,
		const TCHAR* InFilename,
		const uint32 InLine)
		: FunctionName(InFunctionName)
		, Filename(InFilename)
		, Line(InLine)
	{
#if ENABLE_VERIFY_EGL_TRACE
		UE_LOG(LogRHI, Log, TEXT("EGL log before %s(%d): %s"), InFilename, InLine, InFunctionName);
#endif
		CheckForErrors(TEXT("Before "));
	}

	~FEGLErrorScope()
	{
#if ENABLE_VERIFY_EGL_TRACE
		UE_LOG(LogRHI, Log, TEXT("EGL log after  %s(%d): %s"), Filename, Line, FunctionName);
#endif
		CheckForErrors(TEXT("After "));
	}

private:
	const TCHAR* FunctionName;
	const TCHAR* Filename;
	const uint32 Line;

	void CheckForErrors(const TCHAR* PrefixString)
	{
		VerifyEGLResult(eglGetError(), PrefixString, FunctionName, Filename, Line);
	}
};

#define MACRO_TOKENIZER(IdentifierName, Msg, FileName, LineNumber) FEGLErrorScope IdentifierName_ ## LineNumber (Msg, FileName, LineNumber)
#define MACRO_TOKENIZER2(IdentifierName, Msg, FileName, LineNumber) MACRO_TOKENIZER(IdentiferName, Msg, FileName, LineNumber)
#define VERIFY_EGL_SCOPE_WITH_MSG_STR(MsgStr) MACRO_TOKENIZER2(ErrorScope_, MsgStr, TEXT(__FILE__), __LINE__)
#define VERIFY_EGL_SCOPE() VERIFY_EGL_SCOPE_WITH_MSG_STR(ANSI_TO_TCHAR(__FUNCTION__))
#define VERIFY_EGL_FUNC(Func, ...) { VERIFY_EGL_SCOPE_WITH_MSG_STR(TEXT(#Func)); Func(__VA_ARGS__); }
#else
#define VERIFY_EGL(...)
#define VERIFY_EGL_SCOPE(...)
#endif
const  int EGLMinRedBits		= 5;
const  int EGLMinGreenBits		= 6;
const  int EGLMinBlueBits		= 5;
const  int EGLMinAlphaBits		= 0;
const  int EGLMinDepthBits		= 16;
const  int EGLMinStencilBits	= 8; // This is required for UMG clipping
const  int EGLMinSampleBuffers	= 0;
const  int EGLMinSampleSamples	= 0;



struct EGLConfigParms
{
	/** Whether this is a valid configuration or not */
	int validConfig;
	/** The number of bits requested for the red component */
	int redSize ;
	/** The number of bits requested for the green component */
	int greenSize  ;
	/** The number of bits requested for the blue component */
	int blueSize;
	/** The number of bits requested for the alpha component */
	int alphaSize ;
	/** The number of bits requested for the depth component */
	int depthSize;
	/** The number of bits requested for the stencil component */
	int stencilSize ;
	/** The number of multisample buffers requested */
	int sampleBuffers;
	/** The number of samples requested */
	int sampleSamples;

	EGLConfigParms();
	EGLConfigParms(const EGLConfigParms& Parms);
};


struct AndroidESPImpl
{
	FPlatformOpenGLContext	SharedContext;
	FPlatformOpenGLContext	RenderingContext;
	FPlatformOpenGLContext	SingleThreadedContext;

	EGLDisplay eglDisplay;
	EGLint eglNumConfigs;
	EGLint eglFormat;
	EGLConfig eglConfigParam;
	EGLSurface eglSurface;
	EGLSurface auxSurface;
	EGLint eglWidth;
	EGLint eglHeight;
	EGLint NativeVisualID;
	float eglRatio;
	EGLConfigParms Parms;
	int DepthSize;
	uint32_t SwapBufferFailureCount;
	ANativeWindow* Window;
	bool Initalized ;
	EOpenGLCurrentContext CurrentContextType;
	GLuint OnScreenColorRenderBuffer;
	GLuint ResolveFrameBuffer;
	int32 DesiredSyncIntervalRelativeTo60Hz;
	int32 DesiredSyncIntervalRelativeToDevice;
	int32 DriverSyncIntervalRelativeToDevice;
	float DriverRefreshRate;
	int64 DriverRefreshNanos;

	//unknown google mystery meat, maybe search for open source google code called swappy
	int64 DriverAppVsyncOffsetNanos; 
	int64 DriverDeadlineNanos;
	int64 DriverSlopNanos;
	EGLSyncKHR SyncFenceForChoreographerMethod;


	double LastTimeEmulatedSync;
	AndroidESPImpl();
};

const EGLint Attributes[] = {
	EGL_RED_SIZE,       EGLMinRedBits,
	EGL_GREEN_SIZE,     EGLMinGreenBits,
	EGL_BLUE_SIZE,      EGLMinBlueBits,
	EGL_ALPHA_SIZE,     EGLMinAlphaBits,
	EGL_DEPTH_SIZE,     EGLMinDepthBits,
	EGL_STENCIL_SIZE,   EGLMinStencilBits,
	EGL_SAMPLE_BUFFERS, EGLMinSampleBuffers,
	EGL_SAMPLES,        EGLMinSampleSamples,
	EGL_RENDERABLE_TYPE,  EGL_OPENGL_ES2_BIT,
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
	EGL_CONFIG_CAVEAT,  EGL_NONE,
	EGL_NONE
};


EGLConfigParms::EGLConfigParms(const EGLConfigParms& Parms)
{
	validConfig = Parms.validConfig;
	redSize = Parms.redSize;
	greenSize = Parms.greenSize;
	blueSize = Parms.blueSize;
	alphaSize = Parms.alphaSize;
	depthSize = Parms.depthSize;
	stencilSize = Parms.stencilSize;
	sampleBuffers = Parms.sampleBuffers;
	sampleSamples = Parms.sampleSamples;
}

EGLConfigParms::EGLConfigParms()  : 
validConfig (0)
	,redSize(8)
	,greenSize(8)
	,blueSize(8)
	,alphaSize(0)
	,depthSize(24)
	,stencilSize(0)
	,sampleBuffers(0)
	,sampleSamples(0)
{
	// If not default, set the preference
	int DepthBufferPreference = (int)FAndroidWindow::GetDepthBufferPreference();
	if (DepthBufferPreference > 0)
		depthSize = DepthBufferPreference;
}

AndroidEGL::AndroidEGL()
:	bSupportsKHRCreateContext(false)
,	bSupportsKHRSurfacelessContext(false)
,	ContextAttributes(nullptr)
{
	PImplData = new AndroidESPImpl();
}

void AndroidEGL::ResetDisplay()
{
	VERIFY_EGL_SCOPE();
	if(PImplData->eglDisplay != EGL_NO_DISPLAY)
	{
		FPlatformMisc::LowLevelOutputDebugStringf( TEXT("AndroidEGL::ResetDisplay()" ));
		eglMakeCurrent(PImplData->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		PImplData->CurrentContextType = CONTEXT_Invalid;
	}
}

void AndroidEGL::DestroySurface()
{
	VERIFY_EGL_SCOPE();
	FPlatformMisc::LowLevelOutputDebugStringf( TEXT("AndroidEGL::DestroySurface()" ));
	if( PImplData->eglSurface != EGL_NO_SURFACE )
	{
		eglDestroySurface(PImplData->eglDisplay, PImplData->eglSurface);
		PImplData->eglSurface = EGL_NO_SURFACE;
	}
	if( PImplData->auxSurface != EGL_NO_SURFACE )
	{
		eglDestroySurface( PImplData->eglDisplay, PImplData->auxSurface);
		PImplData->auxSurface = EGL_NO_SURFACE;
	}

	PImplData->RenderingContext.eglSurface = EGL_NO_SURFACE;
	PImplData->SingleThreadedContext.eglSurface = EGL_NO_SURFACE;
	PImplData->SharedContext.eglSurface = EGL_NO_SURFACE;
}

void AndroidEGL::TerminateEGL()
{
	VERIFY_EGL_SCOPE();

	eglTerminate(PImplData->eglDisplay);
	PImplData->eglDisplay = EGL_NO_DISPLAY;
	PImplData->Initalized = false;
}

/* Can be called from any thread */
EGLBoolean AndroidEGL::SetCurrentContext(EGLContext InContext, EGLSurface InSurface)
{
	VERIFY_EGL_SCOPE();
	//context can be null.so can surface from PlatformNULLContextSetup
	EGLBoolean Result = EGL_FALSE;
	EGLContext CurrentContext = GetCurrentContext();

	// activate the context
	if( CurrentContext != InContext)
	{
		if (CurrentContext !=EGL_NO_CONTEXT )
		{
			glFlush();
		}
		if(InContext == EGL_NO_CONTEXT && InSurface == EGL_NO_SURFACE)
		{
			ResetDisplay();
		}
		else
		{
			//if we have a valid context, and no surface then create a tiny pbuffer and use that temporarily
			EGLSurface Surface = InSurface;
			if (!bSupportsKHRSurfacelessContext && InContext != EGL_NO_CONTEXT && InSurface == EGL_NO_SURFACE)
			{
				checkf(PImplData->auxSurface == EGL_NO_SURFACE, TEXT("ERROR: PImplData->auxSurface already in use. PBuffer surface leak!"));
				EGLint PBufferAttribs[] =
				{
					EGL_WIDTH, 1,
					EGL_HEIGHT, 1,
					EGL_TEXTURE_TARGET, EGL_NO_TEXTURE,
					EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE,
					EGL_NONE
				};
				PImplData->auxSurface = eglCreatePbufferSurface(PImplData->eglDisplay, PImplData->eglConfigParam, PBufferAttribs);
				if (PImplData->auxSurface == EGL_NO_SURFACE)
				{
					checkf(PImplData->auxSurface != EGL_NO_SURFACE, TEXT("eglCreatePbufferSurface error : 0x%x"), eglGetError());
				}
				Surface = PImplData->auxSurface;
			}

			Result = eglMakeCurrent(PImplData->eglDisplay, Surface, Surface, InContext);
			checkf(Result == EGL_TRUE, TEXT("ERROR: SetCurrentSharedContext eglMakeCurrent failed : 0x%x"), eglGetError());
		}
	}
	return Result;
}

void AndroidEGL::ResetInternal()
{
	Terminate();
}

void AndroidEGL::CreateEGLSurface(ANativeWindow* InWindow, bool bCreateWndSurface)
{
	VERIFY_EGL_SCOPE();

	// due to possible early initialization, don't redo this
	if (PImplData->eglSurface != EGL_NO_SURFACE)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("AndroidEGL::CreateEGLSurface() Already initialized: %p"), PImplData->eglSurface);
		return;
	}

	if (bCreateWndSurface)
	{
		//need ANativeWindow
		PImplData->eglSurface = eglCreateWindowSurface(PImplData->eglDisplay, PImplData->eglConfigParam,InWindow, NULL);

		if (CVarAllowFrameTimestamps.GetValueOnAnyThread())
		{
			eglSurfaceAttrib(PImplData->eglDisplay, PImplData->eglSurface, EGL_TIMESTAMPS_ANDROID, EGL_TRUE);
		}

		FPlatformMisc::LowLevelOutputDebugStringf( TEXT("AndroidEGL::CreateEGLSurface() %p" ),PImplData->eglSurface);

		if(PImplData->eglSurface == EGL_NO_SURFACE )
		{
			checkf(PImplData->eglSurface != EGL_NO_SURFACE, TEXT("eglCreateWindowSurface error : 0x%x"), eglGetError());
			ResetInternal();
		}

		// On some Android devices, eglChooseConfigs will lie about valid configurations (specifically 32-bit color)
		/*	if (eglGetError() == EGL10.EGL_BAD_MATCH)
		{
		Logger.LogOut("eglCreateWindowSurface FAILED, retrying with more restricted context");

		// Dump what's already been initialized
		cleanupEGL();

		// Reduce target color down to 565
		eglAttemptedParams.redSize = 5;
		eglAttemptedParams.greenSize = 6;
		eglAttemptedParams.blueSize = 5;
		eglAttemptedParams.alphaSize = 0;
		initEGL(eglAttemptedParams);

		// try again
		eglSurface = eglCreateWindowSurface(PImplData->eglDisplay, eglConfig, surface, null);
		}

		*/
		EGLBoolean result = EGL_FALSE;
		if (!( result =  ( eglQuerySurface(PImplData->eglDisplay, PImplData->eglSurface, EGL_WIDTH, &PImplData->eglWidth) && eglQuerySurface(PImplData->eglDisplay, PImplData->eglSurface, EGL_HEIGHT, &PImplData->eglHeight) ) ) )
		{
			ResetInternal();
		}

		checkf(result == EGL_TRUE, TEXT("eglQuerySurface error : 0x%x"), eglGetError());
	}
	else
	{
		// create a fake surface instead
		EGLint pbufferAttribs[] =
		{
			EGL_WIDTH, 1,
			EGL_HEIGHT, 1,
			EGL_TEXTURE_TARGET, EGL_NO_TEXTURE,
			EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE,
			EGL_NONE
		};

		checkf(PImplData->eglWidth != 0, TEXT("eglWidth is ZERO; could be a problem!"));
		checkf(PImplData->eglHeight != 0, TEXT("eglHeight is ZERO; could be a problem!"));
		pbufferAttribs[1] = PImplData->eglWidth;
		pbufferAttribs[3] = PImplData->eglHeight;

		FPlatformMisc::LowLevelOutputDebugStringf( TEXT("AndroidEGL::CreateEGLSurface(%d), eglSurface = eglCreatePbufferSurface(), %dx%d" ),
			int(bCreateWndSurface), pbufferAttribs[1], pbufferAttribs[3]);
		PImplData->eglSurface = eglCreatePbufferSurface(PImplData->eglDisplay, PImplData->eglConfigParam, pbufferAttribs);
		if(PImplData->eglSurface== EGL_NO_SURFACE )
		{
			checkf(PImplData->eglSurface != EGL_NO_SURFACE, TEXT("eglCreatePbufferSurface error : 0x%x"), eglGetError());
			ResetInternal();
		}
	}

	EGLint pbufferAttribs[] =
	{
		EGL_WIDTH, 1,
		EGL_HEIGHT, 1,
		EGL_TEXTURE_TARGET, EGL_NO_TEXTURE,
		EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE,
		EGL_NONE
	};

	checkf(PImplData->eglWidth != 0, TEXT("eglWidth is ZERO; could be a problem!"));
	checkf(PImplData->eglHeight != 0, TEXT("eglHeight is ZERO; could be a problem!"));
	pbufferAttribs[1] = PImplData->eglWidth;
	pbufferAttribs[3] = PImplData->eglHeight;

	FPlatformMisc::LowLevelOutputDebugStringf( TEXT("AndroidEGL::CreateEGLSurface(%d), auxSurface = eglCreatePbufferSurface(), %dx%d" ), 
		int(bCreateWndSurface), pbufferAttribs[1], pbufferAttribs[3]);
	PImplData->auxSurface = eglCreatePbufferSurface(PImplData->eglDisplay, PImplData->eglConfigParam, pbufferAttribs);
	if(PImplData->auxSurface== EGL_NO_SURFACE )
	{
		checkf(PImplData->auxSurface != EGL_NO_SURFACE, TEXT("eglCreatePbufferSurface error : 0x%x"), eglGetError());
		ResetInternal();
	}
}


void AndroidEGL::InitEGL(APIVariant API)
{
	VERIFY_EGL_SCOPE();
	// make sure we only do this once (it's optionally done early for cooker communication)
//	static bool bAlreadyInitialized = false;
	if (PImplData->Initalized)
	{
		return;
	}
//	bAlreadyInitialized = true;

	check(PImplData->eglDisplay == EGL_NO_DISPLAY)
	PImplData->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	checkf(PImplData->eglDisplay, TEXT(" eglGetDisplay error : 0x%x "), eglGetError());
	
	EGLBoolean  result = 	eglInitialize(PImplData->eglDisplay, 0 , 0);
	checkf( result == EGL_TRUE, TEXT("elgInitialize error: 0x%x "), eglGetError());

	// Get the EGL Extension list to determine what is supported
	FString Extensions = ANSI_TO_TCHAR( eglQueryString( PImplData->eglDisplay, EGL_EXTENSIONS));

	FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGL Extensions: \n%s" ), *Extensions );

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
		checkf( 0, TEXT("Attempt to initialize EGL with unexpected API type"));
	}

	checkf( result == EGL_TRUE, TEXT("eglBindAPI error: 0x%x "), eglGetError());

#if ENABLE_CONFIG_FILTER

	EGLConfig* EGLConfigList = NULL;
	result = eglChooseConfig(PImplData->eglDisplay, Attributes, NULL, 0, &PImplData->eglNumConfigs);
	if (result)
	{
		int NumConfigs = PImplData->eglNumConfigs;
		EGLConfigList = new EGLConfig[NumConfigs];
		result = eglChooseConfig(PImplData->eglDisplay, Attributes, EGLConfigList, NumConfigs, &PImplData->eglNumConfigs);
	}
	if (!result)
	{
		ResetInternal();
	}

	checkf(result == EGL_TRUE , TEXT(" eglChooseConfig error: 0x%x"), eglGetError());

	checkf(PImplData->eglNumConfigs != 0  ,TEXT(" eglChooseConfig num EGLConfigLists is 0 . error: 0x%x"), eglGetError());

	int ResultValue = 0 ;
	bool haveConfig = false;
	int64 score = LONG_MAX;
	for (uint32_t i = 0; i < PImplData->eglNumConfigs; i++)
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
		else
		{
			// explicitly consume the egl error if EGL_DEPTH_ENCODING_NV does not exist.
			GetError();
		}

		// Favor EGLConfigLists by RGB, then Depth, then Non-linear Depth, then Stencil, then Alpha
		currScore = 0;
		currScore |= ((int64)FPlatformMath::Min(FPlatformMath::Abs(sb - PImplData->Parms.sampleBuffers), 15)) << 29;
		currScore |= ((int64)FPlatformMath::Min(FPlatformMath::Abs(sc - PImplData->Parms.sampleSamples), 31)) << 24;
		currScore |= FPlatformMath::Min(
						FPlatformMath::Abs(r - PImplData->Parms.redSize) +
						FPlatformMath::Abs(g - PImplData->Parms.greenSize) +
						FPlatformMath::Abs(b - PImplData->Parms.blueSize), 127) << 17;
		currScore |= FPlatformMath::Min(FPlatformMath::Abs(d - PImplData->Parms.depthSize), 63) << 11;
		currScore |= FPlatformMath::Min(FPlatformMath::Abs(1 - bNonLinearDepth), 1) << 10;
		currScore |= FPlatformMath::Min(FPlatformMath::Abs(s - PImplData->Parms.stencilSize), 31) << 6;
		currScore |= FPlatformMath::Min(FPlatformMath::Abs(a - PImplData->Parms.alphaSize), 31) << 0;

#if ENABLE_EGL_DEBUG
		LogConfigInfo(EGLConfigList[i]);
#endif

		if (currScore < score || !haveConfig)
		{
			PImplData->eglConfigParam	= EGLConfigList[i];
			PImplData->DepthSize = d;		// store depth/stencil sizes
			haveConfig	= true;
			score		= currScore;
			eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[i], EGL_NATIVE_VISUAL_ID, &ResultValue);PImplData->NativeVisualID = ResultValue;
		}
	}
	check(haveConfig);
	delete[] EGLConfigList;
#else

	EGLConfig EGLConfigList[1];
	if(!( result = eglChooseConfig(PImplData->eglDisplay, Attributes, EGLConfigList, 1,  &PImplData->eglNumConfigs)))
	{
		ResetInternal();
	}

	checkf(result == EGL_TRUE , TEXT(" eglChooseConfig error: 0x%x"), eglGetError());

	checkf(PImplData->eglNumConfigs != 0  ,TEXT(" eglChooseConfig num EGLConfigLists is 0 . error: 0x%x"), eglGetError());
	PImplData->eglConfigParam	= EGLConfigList[0];
	int ResultValue = 0 ;
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[0], EGL_DEPTH_SIZE, &ResultValue); PImplData->DepthSize = ResultValue;
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigList[0], EGL_NATIVE_VISUAL_ID, &ResultValue);PImplData->NativeVisualID = ResultValue;
#endif
}



AndroidESPImpl::AndroidESPImpl():
eglDisplay(EGL_NO_DISPLAY)
	,eglNumConfigs(0)
	,eglFormat(-1)
	,eglConfigParam(NULL)
	,eglSurface(EGL_NO_SURFACE)
	,auxSurface(EGL_NO_SURFACE)
	,eglWidth(8)  // required for Gear VR apps with internal win surf mgmt
	,eglHeight(8) // required for Gear VR apps with internal win surf mgmt
	,eglRatio(0)
	,DepthSize(0)
	,SwapBufferFailureCount(0)
	,Window(NULL)
	,Initalized(false)
	,OnScreenColorRenderBuffer(0)
	,ResolveFrameBuffer(0)
	,NativeVisualID(0)
	,CurrentContextType(CONTEXT_Invalid)
	,DesiredSyncIntervalRelativeTo60Hz(-1)
	,DesiredSyncIntervalRelativeToDevice(-1)
	,DriverSyncIntervalRelativeToDevice(1)
	,DriverRefreshRate(60.0f)
	,DriverRefreshNanos(16666666)
	,DriverAppVsyncOffsetNanos(2000000)
	,DriverDeadlineNanos(10666666)
	,DriverSlopNanos(1000000)
	,SyncFenceForChoreographerMethod(EGL_NO_SYNC_KHR)

	,LastTimeEmulatedSync(-1.0)
{
}

AndroidEGL* AndroidEGL::GetInstance()
{
	if(!Singleton)
	{
		Singleton = new AndroidEGL();
	}

	return Singleton;
}

void AndroidEGL::DestroyBackBuffer()
{
	VERIFY_GL_SCOPE();

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

void AndroidEGL::InitBackBuffer()
{
	//add check to see if any context was made current. 
	GLint OnScreenWidth, OnScreenHeight;
	PImplData->ResolveFrameBuffer = 0;
	PImplData->OnScreenColorRenderBuffer = 0;
	OnScreenWidth = PImplData->eglWidth;
	OnScreenHeight = PImplData->eglHeight;

	PImplData->RenderingContext.ViewportFramebuffer =GetResolveFrameBuffer();
	PImplData->SharedContext.ViewportFramebuffer = GetResolveFrameBuffer();
	PImplData->SingleThreadedContext.ViewportFramebuffer = GetResolveFrameBuffer();
}

extern void AndroidThunkCpp_SetDesiredViewSize(int32 Width, int32 Height);

void AndroidEGL::InitSurface(bool bUseSmallSurface, bool bCreateWndSurface)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("AndroidEGL::InitSurface %d, %d"), int(bUseSmallSurface), int(bCreateWndSurface));

	ANativeWindow* window = (ANativeWindow*)FAndroidWindow::GetHardwareWindow();
	if (window == NULL)
	{
		// Sleep if the hardware window isn't currently available.
		// The Window may not exist if the activity is pausing/resuming, in which case we make this thread wait
		// This case will come up frequently as a result of the DON flow in Gvr.
		// Until the app is fully resumed. It would be nicer if this code respected the lifecycle events
		// of an android app instead, but all of those events are handled on a separate thread and it would require
		// significant re-architecturing to do.
		FPlatformMisc::LowLevelOutputDebugString(TEXT("Waiting for Native window in AndroidEGL::InitSurface"));
		while (window == NULL)
		{
			FPlatformProcess::Sleep(0.001f);
			window = (ANativeWindow*)FAndroidWindow::GetHardwareWindow();
		}
	}

	PImplData->Window = (ANativeWindow*)window;
	int32 Width = 8, Height = 8;
	if (!bUseSmallSurface)
	{
		FPlatformRect WindowSize = FAndroidWindow::GetScreenRect();
		Width = WindowSize.Right;
		Height = WindowSize.Bottom;
		AndroidThunkCpp_SetDesiredViewSize(Width, Height);
	}
	ANativeWindow_setBuffersGeometry(PImplData->Window, Width, Height, PImplData->NativeVisualID);
	CreateEGLSurface(PImplData->Window, bCreateWndSurface);
	
	PImplData->SharedContext.eglSurface = PImplData->auxSurface;
	PImplData->RenderingContext.eglSurface = PImplData->eglSurface;
	PImplData->SingleThreadedContext.eglSurface = PImplData->eglSurface;
}

// call out to JNI to see if the application was packaged for Gear VR
extern bool AndroidThunkCpp_IsGearVRApplication();

void AndroidEGL::ReInit()
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("AndroidEGL::ReInit()"));
	SetCurrentContext(EGL_NO_CONTEXT, EGL_NO_SURFACE);
	bool bCreateSurface = !AndroidThunkCpp_IsGearVRApplication();
	InitSurface(false, bCreateSurface);
	SetCurrentSharedContext();
}

void AndroidEGL::Init(APIVariant API, uint32 MajorVersion, uint32 MinorVersion, bool bDebug)
{

	if (PImplData->Initalized)
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
	PImplData->Initalized   = true;
//	INITIATE_GL_FRAME_DUMP();
}

AndroidEGL::~AndroidEGL()
{
	delete PImplData;
	delete []ContextAttributes;
}

void AndroidEGL::GetDimensions(uint32& OutWidth, uint32& OutHeight)
{
	OutWidth = PImplData->eglWidth;
	OutHeight = PImplData->eglHeight;
}

void AndroidEGL::DestroyContext(EGLContext InContext)
{
	VERIFY_EGL_SCOPE();
	if(InContext != EGL_NO_CONTEXT) //soft fail
	{
		eglDestroyContext(PImplData->eglDisplay, InContext);
	}
}

EGLContext AndroidEGL::CreateContext(EGLContext InSharedContext)
{
	VERIFY_EGL_SCOPE();
	return eglCreateContext(PImplData->eglDisplay, PImplData->eglConfigParam,  InSharedContext , ContextAttributes);
}

int32 AndroidEGL::GetError()
{
	return eglGetError();
}


#include <chrono>

static int64 ChoreographerClock()
{
	return std::chrono::steady_clock::now().time_since_epoch().count();
}

extern void StartChoreographer(TFunction<int64(int64)> Callback);
extern bool ChoreographerIsAvailable();

static TAutoConsoleVariable<int32> CVarUseChoreographerEventArrivalTimes(
	TEXT("a.UseChoreographerEventArriveTimes"),
	0,
	TEXT("On some devices and drivers the choreographer time stamps are wobbly and bad. If this is set to 1, then use the arrival times of the choreographer event instead"));

struct AChoreographer;
struct FChoreographerFramePacer
{
	bool bSetup = false;
	bool bStopped = false;
	FCriticalSection ChoreographerSetupLock;

	FEvent* ChoreographerThreadWaitEvent = nullptr;
	FEvent* RHIThreadWaitEvent = nullptr;
	bool bRHIThreadWaiting = false;

	int64 NextDelay = -1;
	int64 SleepTime = -1;
	int64 SlopTime = 1000000;

	void SetupChoreographer()
	{
		if (!bSetup)
		{
			bSetup = true;
			bRHIThreadWaiting = false;
			if (!ChoreographerThreadWaitEvent)
			{
				ChoreographerThreadWaitEvent = FPlatformProcess::GetSynchEventFromPool(false);
				RHIThreadWaitEvent = FPlatformProcess::GetSynchEventFromPool(false);
			}
			else
			{
				ChoreographerThreadWaitEvent->Reset();
				RHIThreadWaitEvent->Reset();
			}
			StartChoreographer([this](int64 FrameCounter) {return DoCallback(FrameCounter); });
		}
	}

	void StopChoreographer()
	{
		FScopeLock Lock(&ChoreographerSetupLock);
		bSetup = false;
		if (bRHIThreadWaiting)
		{
			bRHIThreadWaiting = false;
			NextDelay = -1;
			ChoreographerThreadWaitEvent->Trigger();
		}
	}

	int64 DoCallback(int64 FrameTime)
	{
		if (!bSetup)
		{
			return -1;
		}
		int64 WakeUpDelay = (CVarUseChoreographerEventArrivalTimes.GetValueOnAnyThread() > 0) ? 0.0 : (ChoreographerClock() - FrameTime);
		int64 AdjustedSleepTime = SlopTime + SleepTime - WakeUpDelay;

		if (AdjustedSleepTime > 0)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ChoreographerSleep);
			//UE_LOG(LogRHI, Log, TEXT("Sleep %6.2fms"), float(AdjustedSleepTime) / 1000000.0f);
			FPlatformProcess::Sleep(float(AdjustedSleepTime) / 1000000000.0f);
		}
		{
			FScopeLock Lock(&ChoreographerSetupLock);
			if (!bSetup)
			{
				return -1;
			}
			if (!bRHIThreadWaiting)
			{
				//UE_LOG(LogRHI, Log, TEXT("Missed VSync (CPU)"));
				return SlopTime;
			}
		}
		RHIThreadWaitEvent->Trigger();
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ChoreographerWait);
			if (!ChoreographerThreadWaitEvent->Wait(FTimespan::FromMilliseconds(3000.0)))
			{
				UE_LOG(LogRHI, Fatal, TEXT("Timed out waiting for ChoreographerThreadWaitEvent."));
			}
		}
		FScopeLock Lock(&ChoreographerSetupLock);
		ChoreographerThreadWaitEvent->Reset();
		//UE_LOG(LogRHI, Log, TEXT("Next Delay %6.2fms"), float(NextDelay) / 1000000.0f);
		return NextDelay;
	}
	void StartSync(int64 InNextDelay, int64 InSleepTime)
	{
		FScopeLock Lock(&ChoreographerSetupLock);
		SleepTime = InSleepTime;
		NextDelay = InNextDelay >= 0 ? InNextDelay + SlopTime : SlopTime;
		SetupChoreographer();
		if (bRHIThreadWaiting)
		{
			bRHIThreadWaiting = false;
			ChoreographerThreadWaitEvent->Trigger();
		}
	}
	void WaitSync()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_StallForChoreographerSyncInterval);
		check(RHIThreadWaitEvent);
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_StallForChoreographerSyncInterval_Lock);
			FScopeLock Lock(&ChoreographerSetupLock);
			check(!bRHIThreadWaiting);
			bRHIThreadWaiting = true;
		}
		if (!RHIThreadWaitEvent->Wait(FTimespan::FromMilliseconds(3000.0)))
		{
			UE_LOG(LogRHI, Fatal, TEXT("Timed out waiting for RHIThreadWaitEvent."));
		}
		RHIThreadWaitEvent->Reset();
	}
};
FChoreographerFramePacer TheChoreographerFramePacer;

static TAutoConsoleVariable<int32> CVarUseChoreographer(
	TEXT("a.UseChoreographerForPacing"),
	0,
	TEXT("True to use Choreographer to do frame pacing on android."));

bool ShouldUseChoreographer()
{
	// should check the ndk version, etc
	return CVarUseChoreographer.GetValueOnAnyThread() > 0 && ChoreographerIsAvailable() && eglGetSyncAttribKHR_p;
}


extern int64 AndroidThunkCpp_GetMetaDataLong(const FString& Key);
extern float AndroidThunkCpp_GetMetaDataFloat(const FString& Key);

static uint32 NextFrameIDSlot = 0;
#define NUM_FRAMES_TO_MONITOR (4)
static EGLuint64KHR FrameIDs[NUM_FRAMES_TO_MONITOR] = { 0 };

static int32 RecordedFrameInterval[100];
static int32 NumRecordedFrameInterval = 0;

static TAutoConsoleVariable<int32> CVarUseGetFrameTimestamps(
	TEXT("a.UseFrameTimeStampsForPacing"),
	0,
	TEXT("True to use eglGetFrameTimestampsANDROID for frame pacing on android (if supported). Only active if a.UseChoreographer is false or the various things needed to use that are not available."));

static TAutoConsoleVariable<int32> CVarSpewGetFrameTimestamps(
	TEXT("a.SpewFrameTimeStamps"),
	0,
	TEXT("True to information about frame pacing to the log (if supported). Setting this to 2 results in more detail."));

static TAutoConsoleVariable<float> CVarStallSwap(
	TEXT("CriticalPathStall.Swap"),
	0.0f,
	TEXT("Sleep for the given time after the swap (android only for now). Time is given in ms. This is a debug option used for critical path analysis and forcing a change in the critical path."));

static TAutoConsoleVariable<int32> CVarDisableOpenGLGPUSync(
	TEXT("r.Android.DisableOpenGLGPUSync"),
	1,
	TEXT("When true, android OpenGL will not prevent the GPU from running more than one frame behind. This will allow higher performance on some devices but increase input latency."),
	ECVF_RenderThreadSafe);

bool ShouldUseGPUFencesToLimitLatency()
{
	if (ShouldUseChoreographer())
	{
		return false; // this method does its own GPU fences as part of the swap.
	}
	if (CVarUseGetFrameTimestamps.GetValueOnAnyThread())
	{
		return true; // this method requires a GPU fence to give steady results
	}
	return CVarDisableOpenGLGPUSync.GetValueOnAnyThread() == 0; // otherwise just based on the cvar; thought to be bad to use GPU fences on PowerVR
}


bool AndroidEGL::SwapBuffers(int32 SyncInterval)
{
	//SyncInterval = 3;
#if !UE_BUILD_SHIPPING
	if (CVarStallSwap.GetValueOnAnyThread() > 0.0f)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_Swap_Intentional_Stall);
		FPlatformProcess::Sleep(CVarStallSwap.GetValueOnRenderThread() / 1000.0f);
	}
#endif

	VERIFY_EGL_SCOPE();


	static bool bUseChoreographer = false;

	bool bChoreographerActive = false;

	if (bUseChoreographer)
	{
		TheChoreographerFramePacer.WaitSync();
		UE_CLOG(PImplData->SyncFenceForChoreographerMethod == EGL_NO_SYNC_KHR, LogRHI, Fatal, TEXT("SyncFenceForChoreographerMethod was EGL_NO_SYNC_KHR"));
		bool bReady = false;
		for (int32 Retry = 0; Retry < 150 && !bReady; Retry++)
		{
			EGLint Status = 0;
			EGLBoolean Result = eglGetSyncAttribKHR_p(PImplData->eglDisplay, PImplData->SyncFenceForChoreographerMethod, EGL_SYNC_STATUS_KHR, &Status);
			if (Result == EGL_FALSE)
			{
				UE_LOG(LogRHI, Error, TEXT("eglGetSyncAttribKHR returned false"));
			}
			else if (Status == EGL_SIGNALED_KHR)
			{
				bReady = true;
			}
			else if (Status != EGL_UNSIGNALED_KHR)
			{
				UE_LOG(LogRHI, Fatal, TEXT("eglGetSyncAttribKHR unexpected value %d"), Status);
			}

			if (!bReady)
			{
				//UE_LOG(LogRHI, Log, TEXT("Missed VSync (GPU)"));
				TheChoreographerFramePacer.StartSync(-1, PImplData->DriverRefreshNanos - PImplData->DriverDeadlineNanos - PImplData->DriverAppVsyncOffsetNanos);
				TheChoreographerFramePacer.WaitSync();
			}
		}
		UE_CLOG(!bReady, LogRHI, Fatal, TEXT("Exhausted retries waiting for a GPU fence....GPU hang?"));
		EGLBoolean Result = eglDestroySyncKHR_p(PImplData->eglDisplay, PImplData->SyncFenceForChoreographerMethod);
		if (Result == EGL_FALSE)
		{
			UE_LOG(LogRHI, Fatal, TEXT("eglDestroySyncKHR_p returned false"));
		}
		PImplData->SyncFenceForChoreographerMethod = EGL_NO_SYNC_KHR;

		bUseChoreographer = false;
		bChoreographerActive = true;
	}

	bool bPrintMethod = false;
	if (PImplData->DesiredSyncIntervalRelativeTo60Hz != SyncInterval)
	{
		bPrintMethod = true;
		PImplData->DesiredSyncIntervalRelativeTo60Hz = SyncInterval;
		PImplData->DriverRefreshRate = 60.0f;
		PImplData->DriverRefreshNanos = 16666666;
		PImplData->DriverAppVsyncOffsetNanos = 2000000;
		PImplData->DriverDeadlineNanos = 10666666;
		PImplData->DriverSlopNanos = 1000000;


		EGLnsecsANDROID EGL_COMPOSITE_DEADLINE_ANDROID_Value = -1;
		EGLnsecsANDROID EGL_COMPOSITE_INTERVAL_ANDROID_Value = -1;
		EGLnsecsANDROID EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID_Value = -1;

		if (eglGetCompositorTimingANDROID_p)
		{
			{
				EGLint Item = EGL_COMPOSITE_DEADLINE_ANDROID;
				if (!eglGetCompositorTimingANDROID_p(PImplData->eglDisplay, PImplData->eglSurface, 1, &Item, &EGL_COMPOSITE_DEADLINE_ANDROID_Value))
				{
					EGL_COMPOSITE_DEADLINE_ANDROID_Value = -1;
				}
			}
			{
				EGLint Item = EGL_COMPOSITE_INTERVAL_ANDROID;
				if (!eglGetCompositorTimingANDROID_p(PImplData->eglDisplay, PImplData->eglSurface, 1, &Item, &EGL_COMPOSITE_INTERVAL_ANDROID_Value))
				{
					EGL_COMPOSITE_INTERVAL_ANDROID_Value = -1;
				}
			}
			{
				EGLint Item = EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID;
				if (!eglGetCompositorTimingANDROID_p(PImplData->eglDisplay, PImplData->eglSurface, 1, &Item, &EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID_Value))
				{
					EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID_Value = -1;
				}
			}
			UE_LOG(LogRHI, Log, TEXT("AndroidEGL:SwapBuffers eglGetCompositorTimingANDROID EGL_COMPOSITE_DEADLINE_ANDROID=%lld, EGL_COMPOSITE_INTERVAL_ANDROID=%lld, EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID=%lld"),
				EGL_COMPOSITE_DEADLINE_ANDROID_Value,
				EGL_COMPOSITE_INTERVAL_ANDROID_Value,
				EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID_Value
			);
		}

		int64 PresentationDeadlineNanos = AndroidThunkCpp_GetMetaDataLong(TEXT("ue4.display.PresentationDeadlineNanos"));
		int64 AppVsyncOffsetNanos = AndroidThunkCpp_GetMetaDataLong(TEXT("ue4.display.AppVsyncOffsetNanos"));

		float RefreshRate = AndroidThunkCpp_GetMetaDataFloat(TEXT("ue4.display.getRefreshRate"));

		UE_LOG(LogRHI, Log, TEXT("JNI Display getPresentationDeadlineNanos=%lld getAppVsyncOffsetNanos=%lld getRefreshRate=%f"),
			PresentationDeadlineNanos,
			AppVsyncOffsetNanos,
			RefreshRate
		);

		if (EGL_COMPOSITE_INTERVAL_ANDROID_Value >= 4000000 && EGL_COMPOSITE_INTERVAL_ANDROID_Value <= 41666666)
		{
			PImplData->DriverRefreshRate = float(1000000000.0 / double(EGL_COMPOSITE_INTERVAL_ANDROID_Value));
			PImplData->DriverRefreshNanos = EGL_COMPOSITE_INTERVAL_ANDROID_Value;
		}
		else if (RefreshRate >= 24.0f && RefreshRate <= 250.0f)
		{
			PImplData->DriverRefreshRate = RefreshRate;
			PImplData->DriverRefreshNanos = int64(0.5 + 1000000000.0 / double(RefreshRate));
		}

		if (EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID_Value > 0 && EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID_Value <= PImplData->DriverRefreshNanos)
		{
			PImplData->DriverDeadlineNanos = EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID_Value;
		}
		if (PresentationDeadlineNanos > 1000000 && PresentationDeadlineNanos - 1000000 <= PImplData->DriverRefreshNanos)
		{
			PImplData->DriverDeadlineNanos = PresentationDeadlineNanos - 1000000;
		}

		if (AppVsyncOffsetNanos >= 0 && AppVsyncOffsetNanos < PImplData->DriverRefreshNanos)
		{
			PImplData->DriverAppVsyncOffsetNanos = AppVsyncOffsetNanos;
		}

		UE_LOG(LogRHI, Log, TEXT("Final display timing metrics: DriverRefreshRate=%7.4f  DriverRefreshNanos=%lld  DriverDeadlineNanos=%lld  DriverAppVsyncOffsetNanos=%lld"),
			PImplData->DriverRefreshRate,
			PImplData->DriverRefreshNanos,
			PImplData->DriverDeadlineNanos,
			PImplData->DriverAppVsyncOffsetNanos
		);


		// make sure requested interval is in supported range
		EGLint MinSwapInterval, MaxSwapInterval;
		eglGetConfigAttrib(PImplData->eglDisplay, PImplData->eglConfigParam, EGL_MIN_SWAP_INTERVAL, &MinSwapInterval);
		eglGetConfigAttrib(PImplData->eglDisplay, PImplData->eglConfigParam, EGL_MAX_SWAP_INTERVAL, &MaxSwapInterval);

		int64 SyncIntervalNanos = (30 + 1000000000l * int64(SyncInterval)) / 60;

		int32 UnderDriverInterval = int32(SyncIntervalNanos / PImplData->DriverRefreshNanos);
		int32 OverDriverInterval = UnderDriverInterval + 1;

		int64 UnderNanos = int64(UnderDriverInterval) * PImplData->DriverRefreshNanos;
		int64 OverNanos = int64(OverDriverInterval) * PImplData->DriverRefreshNanos;

		PImplData->DesiredSyncIntervalRelativeToDevice = (FMath::Abs(SyncIntervalNanos - UnderNanos) < FMath::Abs(SyncIntervalNanos - OverNanos)) ?
			UnderDriverInterval : OverDriverInterval;

		int32 DesiredDriverSyncInterval = FMath::Clamp<int32>(PImplData->DesiredSyncIntervalRelativeToDevice, MinSwapInterval, MaxSwapInterval);
		
		UE_LOG(LogRHI, Log, TEXT("AndroidEGL:SwapBuffers Min=%d, Max=%d, Request=%d, ClosestDriver=%d, SetDriver=%d"), MinSwapInterval, MaxSwapInterval, PImplData->DesiredSyncIntervalRelativeTo60Hz, PImplData->DesiredSyncIntervalRelativeToDevice, DesiredDriverSyncInterval);

		if (DesiredDriverSyncInterval != PImplData->DriverSyncIntervalRelativeToDevice)
		{
			PImplData->DriverSyncIntervalRelativeToDevice = DesiredDriverSyncInterval;
			UE_LOG(LogRHI, Log, TEXT("Called eglSwapInterval %d"), DesiredDriverSyncInterval);
			eglSwapInterval(PImplData->eglDisplay, PImplData->DriverSyncIntervalRelativeToDevice);
		}
	}

	if (PImplData->DesiredSyncIntervalRelativeToDevice > PImplData->DriverSyncIntervalRelativeToDevice)
	{
		// this is a prototype currently unused, left here for possible future use
		if (ShouldUseChoreographer())
		{
			UE_CLOG(bPrintMethod, LogRHI, Display, TEXT("Using google choreographer method for frame pacing"));

			TheChoreographerFramePacer.StartSync((PImplData->DesiredSyncIntervalRelativeToDevice - 1) * PImplData->DriverRefreshNanos, PImplData->DriverRefreshNanos - PImplData->DriverDeadlineNanos - PImplData->DriverAppVsyncOffsetNanos);
			UE_CLOG(PImplData->SyncFenceForChoreographerMethod != EGL_NO_SYNC_KHR, LogRHI, Fatal, TEXT("SyncFenceForChoreographerMethod was NOT EGL_NO_SYNC_KHR"));
			PImplData->SyncFenceForChoreographerMethod = eglCreateSyncKHR_p(PImplData->eglDisplay, EGL_SYNC_FENCE_KHR, NULL);
			if (eglPresentationTimeANDROID_p)
			{
				EGLnsecsANDROID PresentationTime = ChoreographerClock() + PImplData->DesiredSyncIntervalRelativeToDevice * PImplData->DriverRefreshNanos;
				eglPresentationTimeANDROID_p(PImplData->eglDisplay, PImplData->eglSurface, PresentationTime);
			}

			bUseChoreographer = true;
		}
		else
		{
			UE_CLOG(bPrintMethod, LogRHI, Display, TEXT("Using niave method for frame pacing (possible with timestamps method)"));
			if (PImplData->LastTimeEmulatedSync > 0.0)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_StallForEmulatedSyncInterval);
				float MinTimeBetweenFrames = (float(PImplData->DesiredSyncIntervalRelativeToDevice) / PImplData->DriverRefreshRate);

				float ThisTime = FPlatformTime::Seconds() - PImplData->LastTimeEmulatedSync;
				if (ThisTime > 0 && ThisTime < MinTimeBetweenFrames)
				{
					FPlatformProcess::Sleep(MinTimeBetweenFrames - ThisTime);
				}
			}
		}
	}
	if (bChoreographerActive && !bUseChoreographer)
	{
		TheChoreographerFramePacer.StopChoreographer();
	}
	if (!bUseChoreographer && eglPresentationTimeANDROID_p && CVarUseGetFrameTimestamps.GetValueOnAnyThread())
	{
		if (eglGetFrameTimestampsANDROID_p && eglGetNextFrameIdANDROID_p)
		{
			UE_CLOG(bPrintMethod, LogRHI, Display, TEXT("Using eglGetFrameTimestampsANDROID method for frame pacing"));

			//static bool bPrintOnce = true;
			if (FrameIDs[(int32(NextFrameIDSlot) - 1) % NUM_FRAMES_TO_MONITOR])
				// not supported   && eglGetFrameTimestampsSupportedANDROID_p && eglGetFrameTimestampsSupportedANDROID_p(PImplData->eglDisplay, PImplData->eglSurface, EGL_FIRST_COMPOSITION_START_TIME_ANDROID))
			{
				//UE_CLOG(bPrintOnce, LogRHI, Log, TEXT("eglGetFrameTimestampsSupportedANDROID retured true for EGL_FIRST_COMPOSITION_START_TIME_ANDROID"));
				EGLint TimestampList = EGL_FIRST_COMPOSITION_START_TIME_ANDROID;
				//EGLint TimestampList = EGL_COMPOSITION_LATCH_TIME_ANDROID;
				//EGLint TimestampList = EGL_LAST_COMPOSITION_START_TIME_ANDROID;
				//EGLint TimestampList = EGL_DISPLAY_PRESENT_TIME_ANDROID;
				EGLnsecsANDROID Result = 0;
				int32 DeltaFrameIndex = 1;
				for (int32 Index = int32(NextFrameIDSlot) - 1; Index >= int32(NextFrameIDSlot) - NUM_FRAMES_TO_MONITOR && Index >= 0; Index--)
				{
					Result = 0;
					if (FrameIDs[Index % NUM_FRAMES_TO_MONITOR])
					{
						eglGetFrameTimestampsANDROID_p(PImplData->eglDisplay, PImplData->eglSurface, FrameIDs[Index % NUM_FRAMES_TO_MONITOR], 1, &TimestampList, &Result);
					}
					if (Result > 0)
					{
						break;
					}
					DeltaFrameIndex++;
				}
				if (Result > 0)
				{
					EGLnsecsANDROID FudgeFactor = 0; //  8333 * 1000;
					EGLnsecsANDROID DeltaNanos = EGLnsecsANDROID(PImplData->DesiredSyncIntervalRelativeToDevice) * EGLnsecsANDROID(DeltaFrameIndex) * PImplData->DriverRefreshNanos;
					EGLnsecsANDROID PresentationTime = Result + DeltaNanos + FudgeFactor;
					eglPresentationTimeANDROID_p(PImplData->eglDisplay, PImplData->eglSurface, PresentationTime);
				}
			}
			else
			{
				//UE_CLOG(bPrintOnce, LogRHI, Log, TEXT("eglGetFrameTimestampsSupportedANDROID doesn't exist or retured false for EGL_FIRST_COMPOSITION_START_TIME_ANDROID, discarding eglGetNextFrameIdANDROID_p and eglGetFrameTimestampsANDROID_p"));
			}
			//bPrintOnce = false;
		}
	}

	PImplData->LastTimeEmulatedSync = FPlatformTime::Seconds();

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_eglSwapBuffers);

		FrameIDs[(NextFrameIDSlot) % NUM_FRAMES_TO_MONITOR] = 0;
		if (eglGetNextFrameIdANDROID_p && (CVarUseGetFrameTimestamps.GetValueOnAnyThread() || CVarSpewGetFrameTimestamps.GetValueOnAnyThread()))
		{
			eglGetNextFrameIdANDROID_p(PImplData->eglDisplay, PImplData->eglSurface, &FrameIDs[(NextFrameIDSlot) % NUM_FRAMES_TO_MONITOR]);
		}
		NextFrameIDSlot++;

		if (PImplData->eglSurface == NULL || !eglSwapBuffers(PImplData->eglDisplay, PImplData->eglSurface))
		{
			// shutdown if swapbuffering goes down
			if (PImplData->SwapBufferFailureCount > 10)
			{
				//Process.killProcess(Process.myPid());		//@todo android
			}
			PImplData->SwapBufferFailureCount++;

			// basic reporting
			if (PImplData->eglSurface == NULL)
			{
				return false;
			}
			else
			{
				if (eglGetError() == EGL_CONTEXT_LOST)
				{
					//Logger.LogOut("swapBuffers: EGL11.EGL_CONTEXT_LOST err: " + eglGetError());					
					//Process.killProcess(Process.myPid());		//@todo android
				}
			}

			return false;
		}
	}

	if (PImplData->DesiredSyncIntervalRelativeToDevice > 0 && eglGetFrameTimestampsANDROID_p && CVarSpewGetFrameTimestamps.GetValueOnAnyThread())
	{
		static EGLint TimestampList[9] = 
		{
			EGL_REQUESTED_PRESENT_TIME_ANDROID,
			EGL_RENDERING_COMPLETE_TIME_ANDROID,
			EGL_COMPOSITION_LATCH_TIME_ANDROID,
			EGL_FIRST_COMPOSITION_START_TIME_ANDROID,
			EGL_LAST_COMPOSITION_START_TIME_ANDROID,
			EGL_FIRST_COMPOSITION_GPU_FINISHED_TIME_ANDROID,
			EGL_DISPLAY_PRESENT_TIME_ANDROID,
			EGL_DEQUEUE_READY_TIME_ANDROID,
			EGL_READS_DONE_TIME_ANDROID
		};

		static const TCHAR* TimestampStrings[9] =
		{
			TEXT("EGL_REQUESTED_PRESENT_TIME_ANDROID"),
			TEXT("EGL_RENDERING_COMPLETE_TIME_ANDROID"),
			TEXT("EGL_COMPOSITION_LATCH_TIME_ANDROID"),
			TEXT("EGL_FIRST_COMPOSITION_START_TIME_ANDROID"),
			TEXT("EGL_LAST_COMPOSITION_START_TIME_ANDROID"),
			TEXT("EGL_FIRST_COMPOSITION_GPU_FINISHED_TIME_ANDROID"),
			TEXT("EGL_DISPLAY_PRESENT_TIME_ANDROID"),
			TEXT("EGL_DEQUEUE_READY_TIME_ANDROID"),
			TEXT("EGL_READS_DONE_TIME_ANDROID")
		};


		EGLnsecsANDROID Results[NUM_FRAMES_TO_MONITOR][9] = { {0} };
		EGLnsecsANDROID FirstRealValue = 0;
		for (int32 Index = int32(NextFrameIDSlot) - NUM_FRAMES_TO_MONITOR; Index < int32(NextFrameIDSlot); Index++)
		{
			eglGetFrameTimestampsANDROID_p(PImplData->eglDisplay, PImplData->eglSurface, FrameIDs[Index % NUM_FRAMES_TO_MONITOR], 9, TimestampList, Results[Index % NUM_FRAMES_TO_MONITOR]);
			for (int32 IndexInner = 0; IndexInner < 9; IndexInner++)
			{
				if (!FirstRealValue || (Results[Index % NUM_FRAMES_TO_MONITOR][IndexInner] > 1 && Results[Index % NUM_FRAMES_TO_MONITOR][IndexInner] < FirstRealValue))
				{
					FirstRealValue = Results[Index % NUM_FRAMES_TO_MONITOR][IndexInner];
				}
			}
		}
		UE_CLOG(CVarSpewGetFrameTimestamps.GetValueOnAnyThread() > 1, LogRHI, Log, TEXT("************************************  frame %d   base time is %lld"), NextFrameIDSlot - 1, FirstRealValue);

		for (int32 Index = int32(NextFrameIDSlot) - NUM_FRAMES_TO_MONITOR; Index < int32(NextFrameIDSlot); Index++)
		{

			UE_CLOG(CVarSpewGetFrameTimestamps.GetValueOnAnyThread() > 1, LogRHI, Log, TEXT("eglGetFrameTimestampsANDROID_p  frame %d"), Index);
			for (int32 IndexInner = 0; IndexInner < 9; IndexInner++)
			{
				int32 MsVal = (Results[Index % NUM_FRAMES_TO_MONITOR][IndexInner] > 1) ? int32((Results[Index % NUM_FRAMES_TO_MONITOR][IndexInner] - FirstRealValue) / 1000000) : int32(Results[Index % NUM_FRAMES_TO_MONITOR][IndexInner]);

				UE_CLOG(CVarSpewGetFrameTimestamps.GetValueOnAnyThread() > 1, LogRHI, Log, TEXT("     %8d    %s"), MsVal, TimestampStrings[IndexInner]);
			}
		}

		int32 IndexLast = int32(NextFrameIDSlot) - NUM_FRAMES_TO_MONITOR;
		int32 IndexLastNext = IndexLast + 1;

		if (Results[IndexLast % NUM_FRAMES_TO_MONITOR][3] > 1 && Results[IndexLastNext % NUM_FRAMES_TO_MONITOR][3] > 1)
		{
			int32 MsVal = int32((Results[IndexLastNext % NUM_FRAMES_TO_MONITOR][3] - Results[IndexLast % NUM_FRAMES_TO_MONITOR][3]) / 1000000);

			RecordedFrameInterval[NumRecordedFrameInterval++] = MsVal;
			if (NumRecordedFrameInterval == 100)
			{
				FString All;
				int32 NumOnTarget = 0;
				int32 NumBelowTarget = 0;
				int32 NumAboveTarget = 0;
				for (int32 Index = 0; Index < 100; Index++)
				{
					if (Index)
					{
						All += TCHAR(' ');
					}
					All += FString::Printf(TEXT("%d"), RecordedFrameInterval[Index]);

					if (RecordedFrameInterval[Index] > PImplData->DesiredSyncIntervalRelativeTo60Hz * 16 - 8 && RecordedFrameInterval[Index] < PImplData->DesiredSyncIntervalRelativeTo60Hz * 16 + 8)
					{
						NumOnTarget++;
					}
					else if (RecordedFrameInterval[Index] < PImplData->DesiredSyncIntervalRelativeTo60Hz * 16)
					{
						NumBelowTarget++;
					}
					else
					{
						NumAboveTarget++;
					}
				}
				UE_LOG(LogRHI, Log, TEXT("%3d fast  %3d ok  %3d slow   %s"), NumBelowTarget, NumOnTarget, NumAboveTarget, *All);
				NumRecordedFrameInterval = 0;
			}
		}
	}


	return true;
}

bool AndroidEGL::IsInitialized()
{
	return PImplData->Initalized;
}

GLuint AndroidEGL::GetOnScreenColorRenderBuffer()
{
	return PImplData->OnScreenColorRenderBuffer;
}

GLuint AndroidEGL::GetResolveFrameBuffer()
{
	return PImplData->ResolveFrameBuffer;
}

bool AndroidEGL::IsCurrentContextValid()
{
	VERIFY_EGL_SCOPE();
	EGLContext eglContext =  eglGetCurrentContext();
	return ( eglContext != EGL_NO_CONTEXT);
}

EGLContext AndroidEGL::GetCurrentContext()
{
	VERIFY_EGL_SCOPE();
	return eglGetCurrentContext();
}

EGLDisplay AndroidEGL::GetDisplay() const
{
	return PImplData->eglDisplay;
}

ANativeWindow* AndroidEGL::GetNativeWindow() const
{
	return PImplData->Window;
}

bool AndroidEGL::InitContexts()
{
	bool Result = true; 

	PImplData->SharedContext.eglContext = CreateContext();
	
	PImplData->RenderingContext.eglContext = CreateContext(PImplData->SharedContext.eglContext);
	
	PImplData->SingleThreadedContext.eglContext = CreateContext();
	return Result;
}

void AndroidEGL::SetCurrentSharedContext()
{
	check(IsInGameThread());
	PImplData->CurrentContextType = CONTEXT_Shared;

	if(GUseThreadedRendering)
	{
		SetCurrentContext(PImplData->SharedContext.eglContext, PImplData->SharedContext.eglSurface);
	}
	else
	{
		SetCurrentContext(PImplData->SingleThreadedContext.eglContext, PImplData->SingleThreadedContext.eglSurface);
	}
}

void AndroidEGL::SetSharedContext()
{
	check(IsInGameThread());
	PImplData->CurrentContextType = CONTEXT_Shared;

	SetCurrentContext(PImplData->SharedContext.eglContext, PImplData->SharedContext.eglSurface);
}

void AndroidEGL::SetSingleThreadRenderingContext()
{
	PImplData->CurrentContextType = CONTEXT_Rendering;
	SetCurrentContext(PImplData->SingleThreadedContext.eglContext, PImplData->SingleThreadedContext.eglSurface);
}

void AndroidEGL::SetMultithreadRenderingContext()
{
	PImplData->CurrentContextType = CONTEXT_Rendering;
	SetCurrentContext(PImplData->RenderingContext.eglContext, PImplData->RenderingContext.eglSurface);
}

void AndroidEGL::SetCurrentRenderingContext()
{
	PImplData->CurrentContextType = CONTEXT_Rendering;
	if(GUseThreadedRendering)
	{
		SetCurrentContext(PImplData->RenderingContext.eglContext, PImplData->RenderingContext.eglSurface);
	}
	else
	{
		SetCurrentContext(PImplData->SingleThreadedContext.eglContext, PImplData->SingleThreadedContext.eglSurface);
	}
}

void AndroidEGL::Terminate()
{
	ResetDisplay();
	DestroyContext(PImplData->SharedContext.eglContext);
	PImplData->SharedContext.Reset();
	DestroyContext(PImplData->RenderingContext.eglContext);
	PImplData->RenderingContext.Reset();
	DestroyContext(PImplData->SingleThreadedContext.eglContext);
	PImplData->SingleThreadedContext.Reset();
	DestroySurface();
	TerminateEGL();
}

uint32_t AndroidEGL::GetCurrentContextType()
{
	if(GUseThreadedRendering)
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
		return CONTEXT_Shared;//make sure current context is valid one. //check(GetCurrentContext != NULL);
	}

	return CONTEXT_Invalid;
}

FPlatformOpenGLContext* AndroidEGL::GetRenderingContext()
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

void AndroidEGL::UnBind()
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("AndroidEGL::UnBind()"));
	ResetDisplay();
	DestroySurface();
}

void FAndroidAppEntry::ReInitWindow(void* NewNativeWindowHandle)
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("AndroidEGL::ReInitWindow()"));

	// Check for and call a registered window re-init callback. 
	// RHIs may wish to perform additional operations when the window handle changes.
	// Currently only vulkan RHI uses this.
	const auto& OnReinitWindowCallback = FAndroidMisc::GetOnReInitWindowCallback();
	if (OnReinitWindowCallback)
	{
		OnReinitWindowCallback(NewNativeWindowHandle);
	}

	// It isn't safe to call ShouldUseVulkan if AndroidEGL is not initialized.
	// However, since we don't need to ReInit the window in that case anyways we
	// can return early.
	if (!AndroidEGL::GetInstance()->IsInitialized())
	{
		return;
	}

	// @todo vulkan: Clean this up, and does vulkan need any code here?
	if (!FAndroidMisc::ShouldUseVulkan())
	{
		AndroidEGL::GetInstance()->ReInit();
	}
}


void FAndroidAppEntry::OnPauseEvent()
{
	const auto& OnPauseCallback = FAndroidMisc::GetOnPauseCallback();
	if (OnPauseCallback)
	{
		OnPauseCallback();
	}
}

void FAndroidAppEntry::DestroyWindow()
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("AndroidEGL::DestroyWindow()"));

	// It isn't safe to call ShouldUseVulkan if AndroidEGL is not initialized.
	// However, since we don't need to UnBind AndoirdEGL in that case anyways we
	// can return early.
	if (!AndroidEGL::GetInstance()->IsInitialized())
	{
		return;
	}

	// @todo vulkan: Clean this up, and does vulkan need any code here?
	if (!FAndroidMisc::ShouldUseVulkan())
	{
		AndroidEGL::GetInstance()->UnBind();
	}
}

void AndroidEGL::LogConfigInfo(EGLConfig  EGLConfigInfo)
{
	VERIFY_EGL_SCOPE();
	EGLint ResultValue = 0 ;
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_RED_SIZE, &ResultValue); FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo : EGL_RED_SIZE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_GREEN_SIZE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_GREEN_SIZE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_BLUE_SIZE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_BLUE_SIZE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_ALPHA_SIZE, &ResultValue); FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_ALPHA_SIZE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_DEPTH_SIZE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_DEPTH_SIZE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_STENCIL_SIZE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_STENCIL_SIZE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay, EGLConfigInfo, EGL_SAMPLE_BUFFERS, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_SAMPLE_BUFFERS :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_BIND_TO_TEXTURE_RGB, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_BIND_TO_TEXTURE_RGB :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_SAMPLES, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_SAMPLES :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_COLOR_BUFFER_TYPE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_COLOR_BUFFER_TYPE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_CONFIG_CAVEAT, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_CONFIG_CAVEAT :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_CONFIG_ID, &ResultValue); FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_CONFIG_ID :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_CONFORMANT, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_CONFORMANT :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_LEVEL, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_LEVEL :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_LUMINANCE_SIZE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_LUMINANCE_SIZE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_MAX_PBUFFER_WIDTH, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_MAX_PBUFFER_WIDTH :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_MAX_PBUFFER_HEIGHT, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_MAX_PBUFFER_HEIGHT :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_MAX_PBUFFER_PIXELS, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_MAX_PBUFFER_PIXELS :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_MAX_SWAP_INTERVAL, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_MAX_SWAP_INTERVAL :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_MIN_SWAP_INTERVAL, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_MIN_SWAP_INTERVAL :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_NATIVE_RENDERABLE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_NATIVE_RENDERABLE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_NATIVE_VISUAL_TYPE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_NATIVE_VISUAL_TYPE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_NATIVE_VISUAL_ID, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_NATIVE_VISUAL_ID :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_RENDERABLE_TYPE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_RENDERABLE_TYPE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_SURFACE_TYPE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_SURFACE_TYPE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_TRANSPARENT_TYPE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_TRANSPARENT_TYPE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_TRANSPARENT_RED_VALUE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_TRANSPARENT_RED_VALUE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_TRANSPARENT_GREEN_VALUE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_TRANSPARENT_GREEN_VALUE :	%u" ), ResultValue );
	eglGetConfigAttrib(PImplData->eglDisplay,EGLConfigInfo, EGL_TRANSPARENT_BLUE_VALUE, &ResultValue);  FPlatformMisc::LowLevelOutputDebugStringf( TEXT("EGLConfigInfo :EGL_TRANSPARENT_BLUE_VALUE :	%u" ), ResultValue );
}


#endif
