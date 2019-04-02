// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenGLDrvPrivate.h"
THIRD_PARTY_INCLUDES_START
#ifdef HTML5_USE_SDL2
#include <SDL.h>
#endif
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
THIRD_PARTY_INCLUDES_END
#include "HTML5JavaScriptFx.h"

DEFINE_LOG_CATEGORY_STATIC(LogHTML5OpenGL, Log, All);

bool FHTML5OpenGL::bCombinedDepthStencilAttachment = false;
bool FHTML5OpenGL::bSupportsDrawBuffers = false;
bool FHTML5OpenGL::bSupportsInstancing = false;
uint8_t FHTML5OpenGL::currentVertexAttribDivisor[64];
bool FHTML5OpenGL::bIsWebGL2 = false;

void FHTML5OpenGL::ProcessExtensions( const FString& ExtensionsString )
{
	FOpenGLES2::ProcessQueryGLInt();
	FOpenGLBase::ProcessExtensions(ExtensionsString);

	bSupportsMapBuffer = ExtensionsString.Contains(TEXT("GL_OES_mapbuffer"));
	bSupportsDepthTexture = ExtensionsString.Contains(TEXT("GL_OES_depth_texture"));
	bSupportsOcclusionQueries = ExtensionsString.Contains(TEXT("GL_ARB_occlusion_query2")) || ExtensionsString.Contains(TEXT("GL_EXT_occlusion_query_boolean"));
	bSupportsRGBA8 = ExtensionsString.Contains(TEXT("GL_OES_rgb8_rgba8"));
	bSupportsBGRA8888 = ExtensionsString.Contains(TEXT("GL_APPLE_texture_format_BGRA8888")) || ExtensionsString.Contains(TEXT("GL_IMG_texture_format_BGRA8888")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_format_BGRA8888"));
	bSupportsVertexHalfFloat = false;//ExtensionsString.Contains(TEXT("GL_OES_vertex_half_float"));
	bSupportsTextureFloat = ExtensionsString.Contains(TEXT("GL_OES_texture_float"));
	bSupportsTextureHalfFloat = ExtensionsString.Contains(TEXT("GL_OES_texture_half_float")) &&
		ExtensionsString.Contains(TEXT("GL_OES_texture_half_float_linear"));
	bSupportsSGRB = ExtensionsString.Contains(TEXT("GL_EXT_sRGB"));
	bSupportsColorBufferHalfFloat = ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float"));
	bSupportsShaderFramebufferFetch = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_NV_shader_framebuffer_fetch"));
	bRequiresUEShaderFramebufferFetchDef = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch"));
	// @todo ios7: SRGB support does not work with our texture format setup (ES2 docs indicate that internalFormat and format must match, but they don't at all with sRGB enabled)
	//             One possible solution us to use GLFormat.InternalFormat[bSRGB] instead of GLFormat.Format
	bSupportsSGRB = false;//ExtensionsString.Contains(TEXT("GL_EXT_sRGB"));
	bSupportsDXT = ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc"))  ||
		ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc")) ||
		ExtensionsString.Contains(TEXT("WEBGL_compressed_texture_s3tc"))   ||
		(ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_dxt1")) &&    // ANGLE (for HTML5_Win32) exposes these 3 as discrete extensions
		ExtensionsString.Contains(TEXT("GL_ANGLE_texture_compression_dxt3")) &&
		ExtensionsString.Contains(TEXT("GL_ANGLE_texture_compression_dxt5")) );
	bSupportsPVRTC = ExtensionsString.Contains(TEXT("GL_IMG_texture_compression_pvrtc")) ;
	bSupportsATITC = ExtensionsString.Contains(TEXT("GL_ATI_texture_compression_atitc")) || ExtensionsString.Contains(TEXT("GL_AMD_compressed_ATC_texture"));
	bSupportsVertexArrayObjects = ExtensionsString.Contains(TEXT("GL_OES_vertex_array_object")) ;
	bSupportsDiscardFrameBuffer = ExtensionsString.Contains(TEXT("GL_EXT_discard_framebuffer"));
	bSupportsNVFrameBufferBlit = ExtensionsString.Contains(TEXT("GL_NV_framebuffer_blit"));
	bSupportsShaderTextureLod = ExtensionsString.Contains(TEXT("GL_EXT_shader_texture_lod"));
	bSupportsTextureCubeLodEXT = bSupportsShaderTextureLod;

	// This never exists in WebGL (ANGLE exports this, so we want to force it to false)
	bSupportsRGBA8 = false;
	// This is not color-renderable in WebGL/ANGLE (ANGLE exposes this)
	bSupportsBGRA8888 = false;
	bSupportsBGRA8888RenderTarget = false;
	// ANGLE/WEBGL_depth_texture is sort of like OES_depth_texture, you just can't upload bulk data to it (via Tex*Image2D); that should be OK?
	bSupportsDepthTexture =
		ExtensionsString.Contains(TEXT("WEBGL_depth_texture")) ||    // Catch "WEBGL_depth_texture", "MOZ_WEBGL_depth_texture" and "WEBKIT_WEBGL_depth_texture".
		ExtensionsString.Contains(TEXT("GL_ANGLE_depth_texture")) || // for HTML5_WIN32 build with ANGLE
		ExtensionsString.Contains(TEXT("GL_OES_depth_texture"));     // for a future HTML5 build without ANGLE

	bSupportsDrawBuffers = ExtensionsString.Contains(TEXT("WEBGL_draw_buffers"));
	bSupportsInstancing = ExtensionsString.Contains(TEXT("ANGLE_instanced_arrays"));

	// WebGL 1 extensions that were adopted to core WebGL 2 spec:
	UE_LOG(LogTemp, Warning, TEXT("UE_BrowserWebGLVersion %d"), UE_BrowserWebGLVersion());
	if (UE_BrowserWebGLVersion() == 2)
	{
		bSupportsStandardDerivativesExtension = bSupportsDrawBuffers = true;
		bSupportsTextureFloat = bSupportsTextureHalfFloat = bSupportsColorBufferHalfFloat = true;
		bSupportsVertexArrayObjects = bSupportsShaderTextureLod = bSupportsDepthTexture = true;
		bSupportsInstancing = true;

		bIsWebGL2 = true;
	}

	ResetVertexAttribDivisorCache();

	// The core WebGL spec has a combined GL_DEPTH_STENCIL_ATTACHMENT, unlike the core GLES2 spec.
	bCombinedDepthStencilAttachment = true;
	// Note that WebGL always supports packed depth stencil renderbuffers (DEPTH_STENCIL renderbuffor format), but for textures
	// needs WEBGL_depth_texture (at which point it's DEPTH_STENCIL + UNSIGNED_INT_24_8)
	// @todo: if we can always create PF_DepthStencil as DEPTH_STENCIL renderbuffers, we could remove the dependency
	bSupportsPackedDepthStencil = bSupportsDepthTexture;

	if (!bSupportsDepthTexture) {
		UE_LOG(LogRHI, Warning, TEXT("This browser does not support WEBGL_depth_texture. Rendering will not function since fallback code is not available."));
	}

	if (bSupportsTextureHalfFloat && !bSupportsColorBufferHalfFloat) {
		// Initial implementations of WebGL's texture_float screwed up, and allowed
		// rendering to fp textures, even though the underlying EXT_texture_float doesn't
		// explicitly allow anything such.  FP rendering without explicit
		// EXT_color_buffer_half_float may be possible, so we test for that here
		// by checking for framebuffer completeness.  The spec is "wrong" as far as
		// clamping and the like (which WEBGL_color_buffer_float/EXT_color_buffer_half_float
		// fixes, but in practice it might "just work").
		//
		// See http://www.khronos.org/webgl/public-mailing-list/archives/1211/msg00133.html
		// for more information.

		UE_LOG(LogRHI, Warning, TEXT("Trying to enable fp rendering without explicit EXT_color_buffer_half_float by checking for framebuffer completeness"));

		GLenum err = glGetError();
		if (err != GL_NO_ERROR) {
			UE_LOG(LogRHI, Warning, TEXT("Detected OpenGL error 0x%04x before checking for implicit half-float fb support"), err);
		}

		GLuint tex, fb;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_HALF_FLOAT_OES, NULL);
		glGenFramebuffers(1, &fb);
		glBindFramebuffer(GL_FRAMEBUFFER, fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

		GLenum fbstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		bSupportsColorBufferHalfFloat = fbstatus == GL_FRAMEBUFFER_COMPLETE && err == GL_NO_ERROR;
		if (bSupportsColorBufferHalfFloat)
		{
			UE_LOG(LogRHI, Log, TEXT("Enabling implicit ColorBufferHalfFloat after checking fb completeness"));
		}
		else
		{
			UE_LOG(LogRHI, Log, TEXT("Could not enable implicit ColorBufferHalfFloat after checking fb completeness"));
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &fb);
		glDeleteTextures(1, &tex);
	}

	// Report shader precision
	int Range[2];
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_FLOAT, Range, &ShaderLowPrecision);
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, Range, &ShaderMediumPrecision);
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, Range, &ShaderHighPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader lowp precision: %d"), ShaderLowPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader mediump precision: %d"), ShaderMediumPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader highp precision: %d"), ShaderHighPrecision);
}

struct FPlatformOpenGLContext
{
	GLuint			ViewportFramebuffer;
#ifdef HTML5_USE_SDL2
	SDL_GLContext Context; /* Our opengl context handle */
#else
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE Context;
#endif

	FPlatformOpenGLContext()
		:Context(NULL),
		ViewportFramebuffer(0)
	{
	}
};

extern "C" int GSystemResolution_ResX();
extern "C" int GSystemResolution_ResY();

struct FPlatformOpenGLDevice
{
	FPlatformOpenGLDevice()
	{
		SharedContext = new FPlatformOpenGLContext;

		int width, height;
		emscripten_get_canvas_element_size(NULL, &width, &height);

#ifdef HTML5_USE_SDL2
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_EGL, 1);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 32 );
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

		WindowHandle = SDL_CreateWindow("HTML5", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
#else
		// Specifies the CSS ID of the <canvas> element on the page on which to create a context. ("#canvas" is special and means Module['canvas'])
		WindowHandle = "#canvas";
#ifdef __EMSCRIPTEN_PTHREADS__
		thread = 0;
#endif
#endif
		UE_GSystemResolution( GSystemResolution_ResX, GSystemResolution_ResY );
//		emscripten_set_canvas_element_size( GSystemResolution_ResX(), GSystemResolution_ResY() ); // XXX: this might need to go into HTML5JavaScript.js::UE_GSystemResolution()
		PlatformCreateOpenGLContext(this,(void*)WindowHandle);
	}

	FPlatformOpenGLContext* SharedContext;

#ifdef HTML5_USE_SDL2
	SDL_Window* WindowHandle; /* Our window handle */
#else
	const char *WindowHandle;
#ifdef __EMSCRIPTEN_PTHREADS__
	pthread_t thread;
#endif
#endif
};

FPlatformOpenGLDevice* PlatformCreateOpenGLDevice()
{
	return new FPlatformOpenGLDevice;
}

bool PlatformCanEnableGPUCapture()
{
	return false;
}

void PlatformReleaseOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
	check (Device);
EM_ASM({ console.log("!!! PlatformReleaseOpenGLContext 00"); });

	glDeleteFramebuffers(1, &Device->SharedContext->ViewportFramebuffer);
	Device->SharedContext->ViewportFramebuffer = 0;

#ifdef HTML5_USE_SDL2
	SDL_GL_DeleteContext(Device->SharedContext->Context);
	SDL_DestroyWindow(Device->WindowHandle);
#else
	emscripten_webgl_make_context_current(0);
	emscripten_webgl_destroy_context(Device->SharedContext->Context);
#ifdef __EMSCRIPTEN_PTHREADS__
	if ( Device->thread )
	{
		pthread_exit(0);
		Device->thread = 0;
	}
#endif
#endif
	Device->SharedContext->Context = 0;
}

void PlatformDestroyOpenGLDevice(FPlatformOpenGLDevice* Device)
{
	PlatformReleaseOpenGLContext( Device, NULL);
}

FPlatformOpenGLContext* PlatformGetOpenGLRenderingContext(FPlatformOpenGLDevice* Device)
{
	return Device->SharedContext;
}

void _HTML5CreateContext(FPlatformOpenGLDevice* Device, void* InWindowHandle)
{
	EM_ASM({console.log("kai _HTML5CreateContext Device[" + $0 + "] InWindowHandle[" + $1 + "] GUseThreadedRendering[" + $2 + "] FHTML5Misc::AllowRenderThread()[" + $3 + "]")}, Device, InWindowHandle, GUseThreadedRendering, FHTML5Misc::AllowRenderThread());
	EmscriptenWebGLContextAttributes attr;
	emscripten_webgl_init_context_attributes(&attr);
	// Enabling alpha channel on the back buffer would allow web page to composit canvas on top of the elements behind it.
	// Though UE4 engine pipeline always outputs alpha channel as all zeroes, so if alpha was enabled here, the canvas would
	// be completely hidden, so disable it. (not used for anything currently, and might be tiny bit faster without it as well,
	// especially on mobile)
	attr.alpha = 0;
	attr.depth = 1;
	attr.stencil = 0;
	attr.antialias = 0;
	attr.majorVersion = 2; // TODO: Set to 2 if targeting WebGL 2
	attr.enableExtensionsByDefault = 1;
#ifdef __EMSCRIPTEN_PTHREADS__
	if ( GUseThreadedRendering )
	{
		// New explicit swapping support is only available in multithreaded mode.
		if (FHTML5Misc::AllowRenderThread())
		{
			attr.explicitSwapControl = 0;
			UE_LOG(LogRHI, Log, TEXT("Multithreading enabled, targeting explicitSwapControl=0"));

			// Rendering thread requires access to a WebGL context from multiple threads, in which case
			// WebGL proxying will need to be used (no OffscreenCanvas)
			attr.proxyContextToMainThread = EMSCRIPTEN_WEBGL_CONTEXT_PROXY_ALWAYS;
			attr.renderViaOffscreenBackBuffer = EM_TRUE;
		}
		else
		{
			attr.explicitSwapControl = 1;
			UE_LOG(LogRHI, Log, TEXT("Multithreading enabled, targeting explicitSwapControl=1"));
		}
	}
	else
#endif
	{
		attr.explicitSwapControl = 0;
		UE_LOG(LogRHI, Log, TEXT("Multithreading not enabled, setting explicitSwapControl=0"));
	}

#ifdef HTML5_USE_SDL2
	Device->SharedContext->Context = SDL_GL_CreateContext((SDL_Window*)InWindowHandle);
#else
	Device->SharedContext->Context = emscripten_webgl_create_context(0, &attr);
#endif

	if (!Device->SharedContext->Context && attr.majorVersion == 2)
	{
		// If WebGL 2 context creation failed, try WebGL 1 as a fallback.
		attr.majorVersion = 1;
#ifdef HTML5_USE_SDL2
		Device->SharedContext->Context = SDL_GL_CreateContext((SDL_Window*)InWindowHandle);
#else
		Device->SharedContext->Context = emscripten_webgl_create_context(0, &attr);
#endif
	}

	if (!Device->SharedContext->Context)
	{
		UE_LOG(LogRHI, Fatal, TEXT("Failed to create WebGL context!"));
	}

#ifndef HTML5_USE_SDL2
	EMSCRIPTEN_RESULT r = emscripten_webgl_make_context_current(Device->SharedContext->Context);
	if (r != EMSCRIPTEN_RESULT_SUCCESS)
	{
		UE_LOG(LogRHI, Fatal, TEXT("Failed to activate WebGL context!"));
	}
#endif
}

//#if HTML5_ENABLE_RENDERER_THREAD
//void *_HTML5CreateContextThread(void *arg)
//{
////...// XXX: attempt 1
////...//	_HTML5CreateContext((FPlatformOpenGLDevice*)arg /*Device*/);
////...//	FHTML5PlatformProcess::SleepInfinite();
//
//// XXX: attempt 2
//	do {
//		FHTML5PlatformProcess::SleepNoStats(1.0f);
////	} while (PlatformOpenGLContextValid()); // XXX: attempt 3
//	} while (true);
//
//	return NULL;
//}
//#endif

FPlatformOpenGLContext* PlatformCreateOpenGLContext(FPlatformOpenGLDevice* Device, void* InWindowHandle)
{
#ifdef HTML5_USE_SDL2
	Device->SharedContext->Context = SDL_GL_CreateContext((SDL_Window*)InWindowHandle);
#else
#ifdef __EMSCRIPTEN_PTHREADS__
	if ( GUseThreadedRendering )
	{
		if (!FHTML5Misc::AllowRenderThread())
		{
			pthread_attr_t pt_attr;
			pthread_attr_init(&pt_attr);
EM_ASM({ console.log("CANVAS BEFORE"); });
EM_ASM({ console.log( Module['canvas'] ); });
			emscripten_pthread_attr_settransferredcanvases(&pt_attr, Device->WindowHandle); // #canvas
EM_ASM({ console.log("CANVAS AFTER"); });
EM_ASM({ console.log( Module['canvas'] ); });
//			int rc = pthread_create(&Device->thread, &pt_attr, _HTML5CreateContextThread, NULL);
//			if (rc == ENOSYS)
//			{
//				UE_LOG(LogRHI, Fatal, TEXT("OffscreenCanvas is not supported!"));
//			}
			pthread_attr_destroy(&pt_attr);
		}
	}
#endif
	_HTML5CreateContext(Device, InWindowHandle);

#endif
	return Device->SharedContext;
}

void PlatformDestroyOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
	PlatformReleaseOpenGLContext( Device, Context);
}

void* PlatformGetWindow(FPlatformOpenGLContext* Context, void** AddParam)
{
	check(Context);

	return (void*)Context->Context;
}

bool PlatformBlitToViewport( FPlatformOpenGLDevice* Device, const FOpenGLViewport& Viewport, uint32 BackbufferSizeX, uint32 BackbufferSizeY, bool bPresent,bool bLockToVsync, int32 SyncInterval )
{
#ifdef HTML5_USE_SDL2
	SDL_GL_SwapWindow(Device->WindowHandle);
#else

#ifdef __EMSCRIPTEN_PTHREADS__
	if ( GUseThreadedRendering )
	{
		if (!FHTML5Misc::AllowRenderThread())
		{
			// In multithreaded builds, we always use Emscripten's explicit swap mode, where we present on demand.
			// In singlethreaded builds, this does not exist, and we rely on WebGL's "implicit" swap behavior where exiting the animation tick handler() always swaps.
			emscripten_webgl_commit_frame();
		}
	}
#endif

#endif
	return true;
}

void PlatformRenderingContextSetup(FPlatformOpenGLDevice* Device)
{
#ifndef HTML5_USE_SDL2
	// Function name says "Rendering Context", but WebGL doesn't support resource
	// sharing, so this actually just does Shared Context setup.
	EM_ASM({console.log("kai PlatformRenderingContextSetup " + $0)}, Device->SharedContext->Context);

	check( Device && Device->WindowHandle && Device->SharedContext && Device->SharedContext->Context );

	EM_ASM({console.log("kai PlatformRenderingContextSetup: AllowRenderThread ->", $0)}, FHTML5Misc::AllowRenderThread());
	if (FHTML5Misc::AllowRenderThread())
	{
		EMSCRIPTEN_RESULT r = emscripten_webgl_make_context_current(Device->SharedContext->Context);
		EM_ASM({console.log("kai PlatformRenderingContextSetup: emscripten_webgl_make_context_current ->", $0)}, r);

#ifdef __EMSCRIPTEN_PTHREADS__
		// Hack: We may be on another thread than the one that initially created the context. Create a new context to this thread.
		if (r != EMSCRIPTEN_RESULT_SUCCESS)
		{
			UE_LOG(LogRHI, Warning, TEXT("Failed to activate WebGL context!"));

			PlatformCreateOpenGLContext(Device, (void*)"HTML5OpenGL.cpp");

			if (!Device->SharedContext->Context)
			{
				UE_LOG(LogRHI, Fatal, TEXT("Failed to create WebGL context on thread!"));
			}

			r = emscripten_webgl_make_context_current(Device->SharedContext->Context);
		}
#endif

		if (r != EMSCRIPTEN_RESULT_SUCCESS)
		{
			UE_LOG(LogRHI, Fatal, TEXT("Failed to activate WebGL context after creation!"));
		}
	}
#else
	// TODO: get Kai's SDL2 solution
#endif
}

void PlatformFlushIfNeeded()
{
EM_ASM({ console.log("XXX XXX PlatformFlushIfNeeded -- PlatformFlushIfNeeded -- PlatformFlushIfNeeded "); });
}

void PlatformRebindResources(FPlatformOpenGLDevice* Device)
{
}

void PlatformSharedContextSetup(FPlatformOpenGLDevice* Device)
{
	EM_ASM({console.log("kai PlatformSharedContextSetup " + $0)}, Device->SharedContext->Context);
	PlatformRenderingContextSetup(Device);
}

void PlatformNULLContextSetup()
{
#ifndef HTML5_USE_SDL2
	emscripten_webgl_make_context_current(0);
#else
	// PlatformReleaseOpenGLContext?
#endif
}

EOpenGLCurrentContext PlatformOpenGLCurrentContext(FPlatformOpenGLDevice* Device)
{
	check(PlatformContextIsCurrent((uint64) Device->SharedContext->Context));
	return CONTEXT_Shared;
}

void* PlatformOpenGLCurrentContextHandle(FPlatformOpenGLDevice* Device)
{
	return(void*) Device->SharedContext->Context;
}

void PlatformResizeGLContext( FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context, uint32 SizeX, uint32 SizeY, bool bFullscreen, bool bWasFullscreen, GLenum BackBufferTarget, GLuint BackBufferResource)
{
	check(Context);
	VERIFY_GL_SCOPE();

	UE_LOG(LogHTML5OpenGL, Verbose, TEXT("PlatformResizeGLContext(%d,%d)"), SizeX, SizeY);

#ifdef HTML5_USE_SDL2
	SDL_SetWindowSize(Device->WindowHandle,SizeX,SizeY);
#else
	emscripten_set_canvas_element_size(Device->WindowHandle, SizeX, SizeY);
#endif

	glViewport(0, 0, SizeX, SizeY);
}

void PlatformGetSupportedResolution(uint32 &Width, uint32 &Height)
{
	emscripten_get_canvas_element_size(NULL, (int*)&Width, (int*)&Height);
}

bool PlatformGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	return true;
}

bool PlatformInitOpenGL()
{
	UE_LOG(LogTemp, Warning, TEXT("PlatformInitOpenGL"));
	return true;
}

bool PlatformOpenGLContextValid()
{
	return emscripten_webgl_get_current_context() != 0;
}

int32 PlatformGlGetError()
{
	return glGetError();
}

void PlatformGetBackbufferDimensions( uint32& OutWidth, uint32& OutHeight )
{
#ifdef HTML5_USE_SDL2
	SDL_Window* WindowHandle= SDL_GL_GetCurrentWindow();
	check(WindowHandle);
	SDL_Surface *Surface= SDL_GetWindowSurface(WindowHandle);
	check(Surface);
	OutWidth  = Surface->w;
	OutHeight = Surface->h;
#else
	emscripten_get_canvas_element_size("#canvas", (int*)&OutWidth, (int*)&OutHeight);
#endif
	UE_LOG(LogHTML5OpenGL, Verbose, TEXT("PlatformGetBackbufferDimensions(%d, %d)"), OutWidth, OutHeight);
}

// =============================================================

bool PlatformContextIsCurrent( uint64 QueryContext )
{
#ifdef HTML5_USE_SDL2
	return true;
#else
#if defined(__EMSCRIPTEN_PTHREADS__) && ! HTML5_ENABLE_RENDERER_THREAD
	if ( emscripten_webgl_get_current_context() != (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)QueryContext ) {
		EM_ASM({
			console.log("!!! XXX !!! thread["+_pthread_self()+"] PlatformContextIsCurrent curctx["+$0+"] qctx["+$1+"]");
		}, emscripten_webgl_get_current_context(), (uint32)QueryContext );
		return true;
	}
#endif
	return emscripten_webgl_get_current_context() == (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)QueryContext;
#endif
}

FRHITexture* PlatformCreateBuiltinBackBuffer(FOpenGLDynamicRHI* OpenGLRHI, uint32 SizeX, uint32 SizeY)
{
	UE_LOG(LogHTML5OpenGL, Verbose, TEXT("PlatformCreateBuiltinBackBuffer(%d, %d)"), SizeX, SizeY);
	uint32 Flags = TexCreate_RenderTargetable;
	FOpenGLTexture2D* Texture2D = new FOpenGLTexture2D(OpenGLRHI, 0, GL_RENDERBUFFER, GL_COLOR_ATTACHMENT0,
			SizeX, SizeY, 0, 1, 1, 1, 1,
			PF_B8G8R8A8, // format indicates this is WITH transparent values
			false, false, Flags, nullptr,
			FClearValueBinding::Black	// UE-49622: Chrome renderes transparent on OSX - even though canvas has been set with alpha:false
										// in other words, if backbuffer is needed with alpha values -- this will need to be rewritten...
										// for now, UE4 HTML5 seems to be using only a single backbuffer texture
		);
	OpenGLTextureAllocated(Texture2D, Flags);

	return Texture2D;
}

void PlatformGetNewRenderQuery( GLuint* OutQuery, uint64* OutQueryContext )
{
}

void PlatformReleaseRenderQuery( GLuint Query, uint64 QueryContext )
{
}

void PlatformRestoreDesktopDisplayMode()
{
	MAIN_THREAD_EM_ASM( Module['canvas'].UE_canvas.bIsFullScreen = 0; );
}

#include "UnrealEngine.h" // GSystemResolution
extern "C"
{
	int GSystemResolution_ResX()
	{
		return GSystemResolution.ResX;
	}

	int GSystemResolution_ResY()
	{
		return GSystemResolution.ResY;
	}
}


