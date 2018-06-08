// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"

#if !PLATFORM_LUMINGL4

#include "OpenGLDrvPrivate.h"
#include "OpenGLES2.h"
#include "Android/AndroidApplication.h"

namespace GL_EXT
{
	PFNEGLGETSYSTEMTIMENVPROC eglGetSystemTimeNV;
	PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR = NULL;
	PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR = NULL;
	PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR = NULL;

	// Occlusion Queries
	PFNGLGENQUERIESEXTPROC 					glGenQueriesEXT = NULL;
	PFNGLDELETEQUERIESEXTPROC 				glDeleteQueriesEXT = NULL;
	PFNGLISQUERYEXTPROC 					glIsQueryEXT = NULL;
	PFNGLBEGINQUERYEXTPROC 					glBeginQueryEXT = NULL;
	PFNGLENDQUERYEXTPROC 					glEndQueryEXT = NULL;
	PFNGLGETQUERYIVEXTPROC 					glGetQueryivEXT = NULL;
	PFNGLGETQUERYOBJECTIVEXTPROC 			glGetQueryObjectivEXT = NULL;
	PFNGLGETQUERYOBJECTUIVEXTPROC 			glGetQueryObjectuivEXT = NULL;

	PFNGLQUERYCOUNTEREXTPROC				glQueryCounterEXT = NULL;
	PFNGLGETQUERYOBJECTUI64VEXTPROC			glGetQueryObjectui64vEXT = NULL;

	// Offscreen MSAA rendering
	PFNBLITFRAMEBUFFERNVPROC				glBlitFramebufferNV = NULL;
	PFNGLDISCARDFRAMEBUFFEREXTPROC			glDiscardFramebufferEXT = NULL;
	PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC	glFramebufferTexture2DMultisampleEXT = NULL;
	PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC	glRenderbufferStorageMultisampleEXT = NULL;

	PFNGLPUSHGROUPMARKEREXTPROC				glPushGroupMarkerEXT = NULL;
	PFNGLPOPGROUPMARKEREXTPROC				glPopGroupMarkerEXT = NULL;
	PFNGLLABELOBJECTEXTPROC					glLabelObjectEXT = NULL;
	PFNGLGETOBJECTLABELEXTPROC				glGetObjectLabelEXT = NULL;

	PFNGLMAPBUFFEROESPROC					glMapBufferOESa = NULL;
	PFNGLUNMAPBUFFEROESPROC					glUnmapBufferOESa = NULL;

	PFNGLTEXSTORAGE2DPROC					glTexStorage2D = NULL;

	// KHR_debug
	PFNGLDEBUGMESSAGECONTROLKHRPROC			glDebugMessageControlKHR = NULL;
	PFNGLDEBUGMESSAGEINSERTKHRPROC			glDebugMessageInsertKHR = NULL;
	PFNGLDEBUGMESSAGECALLBACKKHRPROC		glDebugMessageCallbackKHR = NULL;
	PFNGLGETDEBUGMESSAGELOGKHRPROC			glDebugMessageLogKHR = NULL;
	PFNGLGETPOINTERVKHRPROC					glGetPointervKHR = NULL;
	PFNGLPUSHDEBUGGROUPKHRPROC				glPushDebugGroupKHR = NULL;
	PFNGLPOPDEBUGGROUPKHRPROC				glPopDebugGroupKHR = NULL;
	PFNGLOBJECTLABELKHRPROC					glObjectLabelKHR = NULL;
	PFNGLGETOBJECTLABELKHRPROC				glGetObjectLabelKHR = NULL;
	PFNGLOBJECTPTRLABELKHRPROC				glObjectPtrLabelKHR = NULL;
	PFNGLGETOBJECTPTRLABELKHRPROC			glGetObjectPtrLabelKHR = NULL;

	PFNGLDRAWELEMENTSINSTANCEDPROC			glDrawElementsInstanced = NULL;
	PFNGLDRAWARRAYSINSTANCEDPROC			glDrawArraysInstanced = NULL;
	PFNGLVERTEXATTRIBDIVISORPROC			glVertexAttribDivisor = NULL;

	PFNGLUNIFORM4UIVPROC					glUniform4uiv = NULL;
	PFNGLTEXIMAGE3DPROC						glTexImage3D = NULL;
	PFNGLTEXSUBIMAGE3DPROC					glTexSubImage3D = NULL;
	PFNGLCOMPRESSEDTEXIMAGE3DPROC			glCompressedTexImage3D = NULL;
	PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC		glCompressedTexSubImage3D = NULL;
	PFNGLCOPYTEXSUBIMAGE3DPROC				glCopyTexSubImage3D = NULL;
	PFNGLCLEARBUFFERFIPROC					glClearBufferfi = NULL;
	PFNGLCLEARBUFFERFVPROC					glClearBufferfv = NULL;
	PFNGLCLEARBUFFERIVPROC					glClearBufferiv = NULL;
	PFNGLCLEARBUFFERUIVPROC					glClearBufferuiv = NULL;
	PFNGLDRAWBUFFERSPROC					glDrawBuffers = NULL;
	PFNGLTEXBUFFEREXTPROC					glTexBufferEXT = NULL;

	PFNGLREADBUFFERPROC glReadBuffer = nullptr;
	PFNGLCOPYIMAGESUBDATAEXTPROC glCopyImageSubDataEXT = nullptr;

	PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR = NULL;
	PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR = NULL;

	PFNGLFRAMEBUFFERTEXTURELAYERPROC glFramebufferTextureLayer = NULL;
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


FPlatformOpenGLDevice::~FPlatformOpenGLDevice()
{
	LuminEGL::GetInstance()->DestroyBackBuffer();
	LuminEGL::GetInstance()->Terminate();
}

FPlatformOpenGLDevice::FPlatformOpenGLDevice()
{
}

void FPlatformOpenGLDevice::Init()
{
	extern void InitDebugContext();

	PlatformRenderingContextSetup(this);

	LoadEXT();

	InitDefaultGLContextState();
	InitDebugContext();

	PlatformSharedContextSetup(this);
	InitDefaultGLContextState();
	InitDebugContext();

	LuminEGL::GetInstance()->InitBackBuffer(); //can be done only after context is made current.
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

void PlatformReleaseOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
}

void* PlatformGetWindow(FPlatformOpenGLContext* Context, void** AddParam)
{
	check(Context);

	return (void*)&Context->eglContext;
}

bool PlatformBlitToViewport(FPlatformOpenGLDevice* Device, const FOpenGLViewport& Viewport, uint32 BackbufferSizeX, uint32 BackbufferSizeY, bool bPresent, bool bLockToVsync, int32 SyncInterval)
{
	FPlatformOpenGLContext* const Context = Viewport.GetGLContext();
	check(Context && Context->eglContext);
	FScopeContext ScopeContext(Context);
	if (bPresent && Viewport.GetCustomPresent())
	{
		glBindFramebuffer(GL_FRAMEBUFFER, Context->ViewportFramebuffer);
		bPresent = Viewport.GetCustomPresent()->Present(SyncInterval);
	}
	if (bPresent)
	{
		// SwapBuffers not supported on Lumin EGL; surfaceless.
		// LuminEGL::GetInstance()->SwapBuffers();
	}
	return bPresent;
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

bool PlatformInitOpenGL()
{
	return true;
}

bool PlatformOpenGLContextValid()
{
	return LuminEGL::GetInstance()->IsCurrentContextValid();
}

void PlatformGetBackbufferDimensions(uint32& OutWidth, uint32& OutHeight)
{
	LuminEGL::GetInstance()->GetDimensions(OutWidth, OutHeight);
}

// =============================================================

void PlatformGetNewOcclusionQuery(GLuint* OutQuery, uint64* OutQueryContext)
{
}

bool PlatformContextIsCurrent(uint64 QueryContext)
{
	return true;
}

void FPlatformOpenGLDevice::LoadEXT()
{
	eglGetSystemTimeNV = (PFNEGLGETSYSTEMTIMENVPROC)((void*)eglGetProcAddress("eglGetSystemTimeNV"));
	eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)((void*)eglGetProcAddress("eglCreateSyncKHR"));
	eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC)((void*)eglGetProcAddress("eglDestroySyncKHR"));
	eglClientWaitSyncKHR = (PFNEGLCLIENTWAITSYNCKHRPROC)((void*)eglGetProcAddress("eglClientWaitSyncKHR"));

	glDebugMessageControlKHR = (PFNGLDEBUGMESSAGECONTROLKHRPROC)((void*)eglGetProcAddress("glDebugMessageControlKHR"));
	glDebugMessageInsertKHR = (PFNGLDEBUGMESSAGEINSERTKHRPROC)((void*)eglGetProcAddress("glDebugMessageInsertKHR"));
	glDebugMessageCallbackKHR = (PFNGLDEBUGMESSAGECALLBACKKHRPROC)((void*)eglGetProcAddress("glDebugMessageCallbackKHR"));
	glDebugMessageLogKHR = (PFNGLGETDEBUGMESSAGELOGKHRPROC)((void*)eglGetProcAddress("glDebugMessageLogKHR"));
	glGetPointervKHR = (PFNGLGETPOINTERVKHRPROC)((void*)eglGetProcAddress("glGetPointervKHR"));
	glPushDebugGroupKHR = (PFNGLPUSHDEBUGGROUPKHRPROC)((void*)eglGetProcAddress("glPushDebugGroupKHR"));
	glPopDebugGroupKHR = (PFNGLPOPDEBUGGROUPKHRPROC)((void*)eglGetProcAddress("glPopDebugGroupKHR"));
	glObjectLabelKHR = (PFNGLOBJECTLABELKHRPROC)((void*)eglGetProcAddress("glObjectLabelKHR"));
	glGetObjectLabelKHR = (PFNGLGETOBJECTLABELKHRPROC)((void*)eglGetProcAddress("glGetObjectLabelKHR"));
	glObjectPtrLabelKHR = (PFNGLOBJECTPTRLABELKHRPROC)((void*)eglGetProcAddress("glObjectPtrLabelKHR"));
	glGetObjectPtrLabelKHR = (PFNGLGETOBJECTPTRLABELKHRPROC)((void*)eglGetProcAddress("glGetObjectPtrLabelKHR"));

	if (GL_EXT::glReadBuffer == nullptr)
	{
		glReadBuffer = (PFNGLREADBUFFERPROC)((void*)eglGetProcAddress("glReadBuffer"));
	}
	if (GL_EXT::glReadBuffer == nullptr)
	{
		glReadBuffer = (PFNGLREADBUFFERPROC)((void*)eglGetProcAddress("glReadBufferNV"));
	}

	glCopyImageSubDataEXT = (PFNGLCOPYIMAGESUBDATAEXTPROC)((void*)eglGetProcAddress("glCopyImageSubDataEXT"));
}

FPlatformOpenGLContext* PlatformCreateOpenGLContext(FPlatformOpenGLDevice* Device, void* InWindowHandle)
{
	//Assumes Device is already initialized and context already created.
	return LuminEGL::GetInstance()->GetRenderingContext();
}

void PlatformDestroyOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
	delete Device; //created here, destroyed here, but held by RHI.
}

FRHITexture* PlatformCreateBuiltinBackBuffer(FOpenGLDynamicRHI* OpenGLRHI, uint32 SizeX, uint32 SizeY)
{
	uint32 Flags = TexCreate_RenderTargetable;
	FOpenGLTexture2D* Texture2D = new FOpenGLTexture2D(OpenGLRHI, LuminEGL::GetInstance()->GetOnScreenColorRenderBuffer(), GL_RENDERBUFFER, GL_COLOR_ATTACHMENT0, SizeX, SizeY, 0, 1, 1, 1, 0, PF_B8G8R8A8, false, false, Flags, nullptr, FClearValueBinding::Transparent);
	OpenGLTextureAllocated(Texture2D, Flags);

	return Texture2D;
}

void PlatformResizeGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context, uint32 SizeX, uint32 SizeY, bool bFullscreen, bool bWasFullscreen, GLenum BackBufferTarget, GLuint BackBufferResource)
{
	check(Context);

	glViewport(0, 0, SizeX, SizeY);
	VERIFY_GL(glViewport);
}

void PlatformGetSupportedResolution(uint32 &Width, uint32 &Height)
{
}

bool PlatformGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	return true;
}

int32 PlatformGlGetError()
{
	return glGetError();
}

// =============================================================

void PlatformReleaseOcclusionQuery(GLuint Query, uint64 QueryContext)
{
}

void FPlatformOpenGLDevice::SetCurrentSharedContext()
{
	LuminEGL::GetInstance()->SetCurrentSharedContext();
}

void PlatformDestroyOpenGLDevice(FPlatformOpenGLDevice* Device)
{
	delete Device;
}

void FPlatformOpenGLDevice::SetCurrentRenderingContext()
{
	LuminEGL::GetInstance()->SetCurrentRenderingContext();
}

void PlatformLabelObjects()
{
	// @todo: Check that there is a valid id (non-zero) as LabelObject will fail otherwise
	GLuint RenderBuffer = LuminEGL::GetInstance()->GetOnScreenColorRenderBuffer();
	if (RenderBuffer != 0)
	{
		FOpenGL::LabelObject(GL_RENDERBUFFER, RenderBuffer, "OnScreenColorRB");
	}

	GLuint FrameBuffer = LuminEGL::GetInstance()->GetResolveFrameBuffer();
	if (FrameBuffer != 0)
	{
		FOpenGL::LabelObject(GL_FRAMEBUFFER, FrameBuffer, "ResolveFB");
	}
}

//--------------------------------

void PlatformGetNewRenderQuery(GLuint* OutQuery, uint64* OutQueryContext)
{
	GLuint NewQuery = 0;
	FOpenGL::GenQueries(1, &NewQuery);
	*OutQuery = NewQuery;
	*OutQueryContext = 0;
}

void PlatformReleaseRenderQuery(GLuint Query, uint64 QueryContext)
{
	FOpenGL::DeleteQueries(1, &Query);
}


bool FLuminOpenGL::bUseHalfFloatTexStorage = false;
bool FLuminOpenGL::bSupportsTextureBuffer = false;
bool FLuminOpenGL::bUseES30ShadingLanguage = false;
bool FLuminOpenGL::bES30Support = false;
bool FLuminOpenGL::bES31Support = false;
bool FLuminOpenGL::bSupportsInstancing = false;
bool FLuminOpenGL::bHasHardwareHiddenSurfaceRemoval = false;
bool FLuminOpenGL::bSupportsMobileMultiView = false;
bool FLuminOpenGL::bSupportsImageExternal = false;
FLuminOpenGL::EImageExternalType FLuminOpenGL::ImageExternalType = FLuminOpenGL::EImageExternalType::None;

void FLuminOpenGL::ProcessExtensions(const FString& ExtensionsString)
{
	FOpenGLES2::ProcessExtensions(ExtensionsString);

	FString VersionString = FString(ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_VERSION)));

	bES30Support = VersionString.Contains(TEXT("OpenGL ES 3."));
	bES31Support = VersionString.Contains(TEXT("OpenGL ES 3.1")) || VersionString.Contains(TEXT("OpenGL ES 3.2"));

	// Get procedures
	if (bSupportsOcclusionQueries || bSupportsDisjointTimeQueries)
	{
		glGenQueriesEXT = (PFNGLGENQUERIESEXTPROC)((void*)eglGetProcAddress("glGenQueriesEXT"));
		glDeleteQueriesEXT = (PFNGLDELETEQUERIESEXTPROC)((void*)eglGetProcAddress("glDeleteQueriesEXT"));
		glIsQueryEXT = (PFNGLISQUERYEXTPROC)((void*)eglGetProcAddress("glIsQueryEXT"));
		glBeginQueryEXT = (PFNGLBEGINQUERYEXTPROC)((void*)eglGetProcAddress("glBeginQueryEXT"));
		glEndQueryEXT = (PFNGLENDQUERYEXTPROC)((void*)eglGetProcAddress("glEndQueryEXT"));
		glGetQueryivEXT = (PFNGLGETQUERYIVEXTPROC)((void*)eglGetProcAddress("glGetQueryivEXT"));
		glGetQueryObjectivEXT = (PFNGLGETQUERYOBJECTIVEXTPROC)((void*)eglGetProcAddress("glGetQueryObjectivEXT"));
		glGetQueryObjectuivEXT = (PFNGLGETQUERYOBJECTUIVEXTPROC)((void*)eglGetProcAddress("glGetQueryObjectuivEXT"));
	}

	if (bSupportsDisjointTimeQueries)
	{
		glQueryCounterEXT = (PFNGLQUERYCOUNTEREXTPROC)((void*)eglGetProcAddress("glQueryCounterEXT"));
		glGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC)((void*)eglGetProcAddress("glGetQueryObjectui64vEXT"));

		// If EXT_disjoint_timer_query wasn't found, NV_timer_query might be available
		if (glQueryCounterEXT == NULL)
		{
			glQueryCounterEXT = (PFNGLQUERYCOUNTEREXTPROC)eglGetProcAddress("glQueryCounterNV");
		}
		if (glGetQueryObjectui64vEXT == NULL)
		{
			glGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC)eglGetProcAddress("glGetQueryObjectui64vNV");
		}
	}

	glDiscardFramebufferEXT = (PFNGLDISCARDFRAMEBUFFEREXTPROC)((void*)eglGetProcAddress("glDiscardFramebufferEXT"));
	glFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)((void*)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT"));
	glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)((void*)eglGetProcAddress("glRenderbufferStorageMultisampleEXT"));
	glPushGroupMarkerEXT = (PFNGLPUSHGROUPMARKEREXTPROC)((void*)eglGetProcAddress("glPushGroupMarkerEXT"));
	glPopGroupMarkerEXT = (PFNGLPOPGROUPMARKEREXTPROC)((void*)eglGetProcAddress("glPopGroupMarkerEXT"));
	glLabelObjectEXT = (PFNGLLABELOBJECTEXTPROC)((void*)eglGetProcAddress("glLabelObjectEXT"));
	glGetObjectLabelEXT = (PFNGLGETOBJECTLABELEXTPROC)((void*)eglGetProcAddress("glGetObjectLabelEXT"));

	bSupportsETC2 = bES30Support;
	bUseES30ShadingLanguage = bES30Support;

	FString RendererString = FString(ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_RENDERER)));

	// Check for external image support for different ES versions
	ImageExternalType = EImageExternalType::None;

	static const auto CVarOverrideExternalTextureSupport = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Lumin.OverrideExternalTextureSupport"));
	const int32 OverrideExternalTextureSupport = CVarOverrideExternalTextureSupport->GetValueOnAnyThread();
	switch (OverrideExternalTextureSupport)
	{
		case 1:
			ImageExternalType = EImageExternalType::None;
			break;

		case 2:
			ImageExternalType = EImageExternalType::ImageExternal100;
			break;

		case 3:
			ImageExternalType = EImageExternalType::ImageExternal300;
			break;	

		case 4:
			ImageExternalType = EImageExternalType::ImageExternalESSL300;
			break;

		case 0:
		default:
			// auto-detect by extensions (default)
			bool bHasImageExternal = ExtensionsString.Contains(TEXT("GL_OES_EGL_image_external ")) || ExtensionsString.EndsWith(TEXT("GL_OES_EGL_image_external"));
			bool bHasImageExternalESSL3 = ExtensionsString.Contains(TEXT("OES_EGL_image_external_essl3"));
			if (bHasImageExternal || bHasImageExternalESSL3)
			{
				ImageExternalType = EImageExternalType::ImageExternal100;
				if (bUseES30ShadingLanguage)
				{
					if (bHasImageExternalESSL3)
					{
						ImageExternalType = EImageExternalType::ImageExternalESSL300;
					}
				}
			}
			break;
	}
	switch (ImageExternalType)
	{
		case EImageExternalType::None:
			UE_LOG(LogRHI, Log, TEXT("Image external disabled"));
			break;

		case EImageExternalType::ImageExternal100:
			UE_LOG(LogRHI, Log, TEXT("Image external enabled: ImageExternal100"));
			break;

		case EImageExternalType::ImageExternal300:
			UE_LOG(LogRHI, Log, TEXT("Image external enabled: ImageExternal300"));
			break;

		case EImageExternalType::ImageExternalESSL300:
			UE_LOG(LogRHI, Log, TEXT("Image external enabled: ImageExternalESSL300"));
			break;

		default:
			ImageExternalType = EImageExternalType::None;
			UE_LOG(LogRHI, Log, TEXT("Image external disabled; unknown type"));
	}
	bSupportsImageExternal = ImageExternalType != EImageExternalType::None;

	if (bES30Support)
	{
		glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC)((void*)eglGetProcAddress("glDrawElementsInstanced"));
		glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)((void*)eglGetProcAddress("glDrawArraysInstanced"));
		glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)((void*)eglGetProcAddress("glVertexAttribDivisor"));
		glUniform4uiv = (PFNGLUNIFORM4UIVPROC)((void*)eglGetProcAddress("glUniform4uiv"));
		glTexImage3D = (PFNGLTEXIMAGE3DPROC)((void*)eglGetProcAddress("glTexImage3D"));
		glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC)((void*)eglGetProcAddress("glTexSubImage3D"));
		glCompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC)((void*)eglGetProcAddress("glCompressedTexImage3D"));
		glCompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)((void*)eglGetProcAddress("glCompressedTexSubImage3D"));
		glCopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC)((void*)eglGetProcAddress("glCopyTexSubImage3D"));
		glClearBufferfi = (PFNGLCLEARBUFFERFIPROC)((void*)eglGetProcAddress("glClearBufferfi"));
		glClearBufferfv = (PFNGLCLEARBUFFERFVPROC)((void*)eglGetProcAddress("glClearBufferfv"));
		glClearBufferiv = (PFNGLCLEARBUFFERIVPROC)((void*)eglGetProcAddress("glClearBufferiv"));
		glClearBufferuiv = (PFNGLCLEARBUFFERUIVPROC)((void*)eglGetProcAddress("glClearBufferuiv"));
		glDrawBuffers = (PFNGLDRAWBUFFERSPROC)((void*)eglGetProcAddress("glDrawBuffers"));
		glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC)((void*)eglGetProcAddress("glFramebufferTextureLayer"));

		// Required by the ES3 spec
		bSupportsInstancing = true;
		bSupportsTextureFloat = true;
		bSupportsTextureHalfFloat = true;
	}

	if (bES30Support)
	{
		// Mobile multi-view setup
		const bool bMultiViewSupport = ExtensionsString.Contains(TEXT("GL_OVR_multiview"));
		const bool bMultiView2Support = ExtensionsString.Contains(TEXT("GL_OVR_multiview2"));
		const bool bMultiViewMultiSampleSupport = ExtensionsString.Contains(TEXT("GL_OVR_multiview_multisampled_render_to_texture"));
		if (bMultiViewSupport && bMultiView2Support && bMultiViewMultiSampleSupport)
		{
			glFramebufferTextureMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)((void*)eglGetProcAddress("glFramebufferTextureMultiviewOVR"));
			glFramebufferTextureMultisampleMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)((void*)eglGetProcAddress("glFramebufferTextureMultisampleMultiviewOVR"));

			bSupportsMobileMultiView = (glFramebufferTextureMultiviewOVR != NULL) && (glFramebufferTextureMultisampleMultiviewOVR != NULL);

			// Just because the driver declares multi-view support and hands us valid function pointers doesn't actually guarantee the feature works...
			if (bSupportsMobileMultiView)
			{
				UE_LOG(LogRHI, Log, TEXT("Device supports mobile multi-view."));
			}
		}
	}

	// Adreno's implementation of GL_EXT_texture_buffer errors when creating light grid resources
	if (bES31Support)
	{
		bSupportsTextureBuffer = ExtensionsString.Contains(TEXT("GL_EXT_texture_buffer"));
		if (bSupportsTextureBuffer)
		{
			glTexBufferEXT = (PFNGLTEXBUFFEREXTPROC)((void*)eglGetProcAddress("glTexBufferEXT"));
		}
	}
	
	if (bES30Support)
	{
		// Attempt to find ES 3.0 glTexStorage2D if we're on an ES 3.0 device
		glTexStorage2D = (PFNGLTEXSTORAGE2DPROC)((void*)eglGetProcAddress("glTexStorage2D"));
		if( glTexStorage2D != NULL )
		{
			bUseHalfFloatTexStorage = true;
		}
		else
		{
			// need to disable GL_EXT_color_buffer_half_float support because we have no way to allocate the storage and the driver doesn't work without it.
			UE_LOG(LogRHI,Warning,TEXT("Disabling support for GL_EXT_color_buffer_half_float as we cannot bind glTexStorage2D"));
			bSupportsColorBufferHalfFloat = false;
		}
	}


	//@todo android: need GMSAAAllowed	 ?
	if (bSupportsNVFrameBufferBlit)
	{
		glBlitFramebufferNV = (PFNBLITFRAMEBUFFERNVPROC)((void*)eglGetProcAddress("glBlitFramebufferNV"));
	}

	glMapBufferOESa = (PFNGLMAPBUFFEROESPROC)((void*)eglGetProcAddress("glMapBufferOES"));
	glUnmapBufferOESa = (PFNGLUNMAPBUFFEROESPROC)((void*)eglGetProcAddress("glUnmapBufferOES"));

	//On Android, there are problems compiling shaders with textureCubeLodEXT calls in the glsl code,
	// so we set this to false to modify the glsl manually at compile-time.
	bSupportsTextureCubeLodEXT = false;
	
	if (bSupportsBGRA8888)
	{
		// Check whether device supports BGRA as color attachment
		GLuint FrameBuffer;
		glGenFramebuffers(1, &FrameBuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, FrameBuffer);
		GLuint BGRA8888Texture;
		glGenTextures(1, &BGRA8888Texture);
		glBindTexture(GL_TEXTURE_2D, BGRA8888Texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, 256, 256, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, BGRA8888Texture, 0);

		bSupportsBGRA8888RenderTarget = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		glDeleteTextures(1, &BGRA8888Texture);
		glDeleteFramebuffers(1, &FrameBuffer);
	}
}

void FAndroidAppEntry::PlatformInit()
{
	LuminEGL::GetInstance()->Init(LuminEGL::AV_OpenGLES, 2, 0, false);
}


void FAndroidAppEntry::ReleaseEGL()
{
	// @todo Lumin: If we switch to Vk, we may need this when we build for both
}

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


