// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ExternalOESTextureRenderer.h"
#include "RenderingThread.h"
#include "Misc/ScopeLock.h"
#include "IMagicLeapMediaModule.h"
#include "Misc/CString.h"
#include "Lumin/LuminEGL.h"

const  int EGLMinRedBits		= 5;
const  int EGLMinGreenBits		= 6;
const  int EGLMinBlueBits		= 5;
const  int EGLMinDepthBits		= 16;
const EGLint Attributes[] = {
	EGL_RED_SIZE,       EGLMinRedBits,
	EGL_GREEN_SIZE,     EGLMinGreenBits,
	EGL_BLUE_SIZE,      EGLMinBlueBits,
	EGL_DEPTH_SIZE,     EGLMinDepthBits,
	EGL_NONE
};

const int EGLDesiredRedBits = 8;
const int EGLDesiredGreenBits = 8;
const int EGLDesiredBlueBits = 8;
const int EGLDesiredAlphaBits = 0;
const int EGLDesiredDepthBits = 24;
const int EGLDesiredStencilBits = 0;
const int EGLDesiredSampleBuffers = 0;
const int EGLDesiredSampleSamples = 0;

FExternalOESTextureRenderer::FExternalOESTextureRenderer(bool bUseOwnContext)
: TextureID(-1)
, FBO(-1)
, ReadTexture(0)
, bTriangleVerticesDirty(true)
, Display(EGL_NO_DISPLAY)
, Context(EGL_NO_CONTEXT)
, SavedDisplay(EGL_NO_DISPLAY)
, SavedContext(EGL_NO_CONTEXT)
, bUseIsolatedContext(bUseOwnContext)
, bInitialized(false)
, bSupportsKHRCreateContext(false)
, ContextAttributes(nullptr)
, vb0pointer(nullptr)
, vb1pointer(nullptr)
{

	BlitVertexShader =
		FString(TEXT("attribute vec2 Position;\n")) +
		FString(TEXT("attribute vec2 TexCoords;\n")) +
		FString(TEXT("varying vec2 TexCoord;\n")) +
		FString(TEXT("void main()\n")) +
		FString(TEXT("{\n")) +
		FString(TEXT("  TexCoord = TexCoords;\n")) +
		FString(TEXT("  gl_Position = vec4(Position, 0.0, 1.0);\n")) +
		FString(TEXT("}\n"));

	BlitFragmentShaderBGRA = 
		FString(TEXT("#extension GL_OES_EGL_image_external : require\n")) +
		FString(TEXT("uniform samplerExternalOES VideoTexture;\n")) +
		FString(TEXT("varying highp vec2 TexCoord;\n")) +
		FString(TEXT("void main()\n")) +
		FString(TEXT("{\n"));
		if (FLuminPlatformMisc::ShouldUseVulkan() || FLuminPlatformMisc::ShouldUseDesktopOpenGL())
		{
			BlitFragmentShaderBGRA.Append(FString(TEXT("  gl_FragColor = texture2D(VideoTexture, TexCoord).bgra;\n")));
		}
		else
		{
			BlitFragmentShaderBGRA.Append(FString(TEXT("  gl_FragColor = texture2D(VideoTexture, TexCoord);\n")));      
		}
		BlitFragmentShaderBGRA.Append(FString(TEXT("}\n")));
}

FExternalOESTextureRenderer::~FExternalOESTextureRenderer()
{
	Release();
	ResetInternal();
	delete []ContextAttributes;
}

bool FExternalOESTextureRenderer::InitContext()
{
	if (Context == EGL_NO_CONTEXT && bUseIsolatedContext)
	{
		Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		checkf(Display, TEXT(" eglGetDisplay error : 0x%x "), eglGetError());

		EGLBoolean result = eglInitialize(Display, 0, 0);
		checkf(result == EGL_TRUE, TEXT("elgInitialize error: 0x%x "), eglGetError());

		// Get the EGL Extension list to determine what is supported
		FString Extensions = ANSI_TO_TCHAR(eglQueryString(Display, EGL_EXTENSIONS));

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("EGL Extensions: \n%s"), *Extensions);

		bSupportsKHRCreateContext = Extensions.Contains(TEXT("EGL_KHR_create_context"));

		result = eglBindAPI(EGL_OPENGL_ES_API);
		checkf(result == EGL_TRUE, TEXT("eglBindAPI error: 0x%x "), eglGetError());

		EGLint eglNumConfigs = 0;
		if ((result = eglGetConfigs(Display, nullptr, 0, &eglNumConfigs)) == EGL_FALSE)
		{
			ResetInternal();
		}
		checkf(result == EGL_TRUE, TEXT("eglGetConfigs error: 0x%x"), eglGetError());

		EGLint eglNumVisuals = 0;
		EGLConfig EGLConfigList[eglNumConfigs];
		if (!(result = eglChooseConfig(Display, Attributes, EGLConfigList, eglNumConfigs, &eglNumVisuals)))
		{
			ResetInternal();
		}
		checkf(result == EGL_TRUE, TEXT(" eglChooseConfig error: 0x%x"), eglGetError());
		checkf(eglNumVisuals != 0, TEXT(" eglChooseConfig results is 0 . error: 0x%x"), eglGetError());

		EGLConfig ConfigParam;
		int ResultValue = 0;
		bool haveConfig = false;
		int64 score = LONG_MAX;
		for (int i = 0; i < eglNumVisuals; ++i)
		{
			int64 currScore = 0;
			int r, g, b, a, d, s, sb, sc, nvi;
			eglGetConfigAttrib(Display, EGLConfigList[i], EGL_RED_SIZE, &ResultValue); r = ResultValue;
			eglGetConfigAttrib(Display, EGLConfigList[i], EGL_GREEN_SIZE, &ResultValue); g = ResultValue;
			eglGetConfigAttrib(Display, EGLConfigList[i], EGL_BLUE_SIZE, &ResultValue); b = ResultValue;
			eglGetConfigAttrib(Display, EGLConfigList[i], EGL_ALPHA_SIZE, &ResultValue); a = ResultValue;
			eglGetConfigAttrib(Display, EGLConfigList[i], EGL_DEPTH_SIZE, &ResultValue); d = ResultValue;
			eglGetConfigAttrib(Display, EGLConfigList[i], EGL_STENCIL_SIZE, &ResultValue); s = ResultValue;
			eglGetConfigAttrib(Display, EGLConfigList[i], EGL_SAMPLE_BUFFERS, &ResultValue); sb = ResultValue;
			eglGetConfigAttrib(Display, EGLConfigList[i], EGL_SAMPLES, &ResultValue); sc = ResultValue;

			// Optional, Tegra-specific non-linear depth buffer, which allows for much better
			// effective depth range in relatively limited bit-depths (e.g. 16-bit)
			int bNonLinearDepth = 0;
			if (eglGetConfigAttrib(Display, EGLConfigList[i], EGL_DEPTH_ENCODING_NV, &ResultValue))
			{
				bNonLinearDepth = (ResultValue == EGL_DEPTH_ENCODING_NONLINEAR_NV) ? 1 : 0;
			}

			EGLint NativeVisualId = 0;
			eglGetConfigAttrib(Display, EGLConfigList[i], EGL_NATIVE_VISUAL_ID, &NativeVisualId);

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
					ConfigParam = EGLConfigList[i];
					haveConfig = true;
					score = currScore;
				}
			}
		}

		if (bSupportsKHRCreateContext)
		{
			const uint32 MaxElements = 13;
			uint32 Flags = 0;

			Flags |= 0;

			ContextAttributes = new int[MaxElements];
			uint32 Element = 0;

			ContextAttributes[Element++] = EGL_CONTEXT_MAJOR_VERSION_KHR;
			ContextAttributes[Element++] = 2;
			ContextAttributes[Element++] = EGL_CONTEXT_MINOR_VERSION_KHR;
			ContextAttributes[Element++] = 0;
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

		Context = eglCreateContext(Display, ConfigParam, EGL_NO_CONTEXT, ContextAttributes);
	}

	return bUseIsolatedContext ? (Context != EGL_NO_CONTEXT) : true;
}

void FExternalOESTextureRenderer::SaveContext()
{
	SavedDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	SavedContext = eglGetCurrentContext();
}

void FExternalOESTextureRenderer::MakeCurrent()
{
	if (bUseIsolatedContext)
	{
		EGLBoolean bResult = eglMakeCurrent(Display, EGL_NO_SURFACE, EGL_NO_SURFACE, Context);
		if (bResult == EGL_FALSE)
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("Error setting media player context."));
		}
	}
}

void FExternalOESTextureRenderer::RestoreContext()
{
	if (bUseIsolatedContext)
	{
		EGLBoolean bResult = eglMakeCurrent(SavedDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, SavedContext);
		if (bResult == EGL_FALSE)
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("Error setting unreal context."));
		}
	}
}

void FExternalOESTextureRenderer::ResetInternal()
{
	if (bUseIsolatedContext)
	{
		if (Display != EGL_NO_DISPLAY)
		{
			eglMakeCurrent(Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		}
		eglDestroyContext(Display, Context);
		eglTerminate(Display);
	}
}

void FExternalOESTextureRenderer::InitSurfaceTexture()
{
	glGenTextures(1, &TextureID);
	if (TextureID <= 0)
	{
		Release();
		return;
	}

	glGenFramebuffers(1, &FBO);
	if (FBO <= 0)
	{
		Release();
		return;
	}

	BlitVertexShaderID = CreateShader(GL_VERTEX_SHADER, BlitVertexShader);
	if (BlitVertexShaderID == 0)
	{
		Release();
		return;
	}

	BlitFragmentShaderID = CreateShader(GL_FRAGMENT_SHADER, BlitFragmentShaderBGRA);
	if (BlitFragmentShaderID == 0)
	{
		Release();
		return;
	}

	Program = glCreateProgram();
	glAttachShader(Program, BlitVertexShaderID);
	glAttachShader(Program, BlitFragmentShaderID);
	glLinkProgram(Program);

	glDetachShader(Program, BlitVertexShaderID);
	glDetachShader(Program, BlitFragmentShaderID);

	glDeleteShader(BlitVertexShaderID);
	glDeleteShader(BlitFragmentShaderID);

	BlitVertexShaderID = 0;
	BlitFragmentShaderID = 0;

	GLint linkStatus;
	glGetProgramiv(Program, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("Could not link program"));

		int32 maxLength = 0;
		glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &maxLength);

		GLchar* errorLog = new GLchar[maxLength + 1];

		glGetProgramInfoLog(Program, maxLength, &maxLength, errorLog);

		UE_LOG(LogMagicLeapMedia, Error, TEXT("%s"), ANSI_TO_TCHAR(errorLog));

		delete[] errorLog;

		glDeleteProgram(Program);
		Program = 0;
		Release();
		return;
	}

	PositionAttribLocation = glGetAttribLocation(Program, "Position");
	TexCoordsAttribLocation = glGetAttribLocation(Program, "TexCoords");
	TextureUniformLocation = glGetAttribLocation(Program, "VideoTexture");

	glGenBuffers(1, &BlitBufferVBO);
	if (BlitBufferVBO <= 0)
	{
		Release();
		return;
	}

	bTriangleVerticesDirty = true;
}

GLuint FExternalOESTextureRenderer::CreateShader(GLenum ShaderType, const FString& ShaderSource)
{
	GLuint shader = glCreateShader(ShaderType);
	if (shader != 0)
	{
		TArray<ANSICHAR> ShaderSourceAnsi;
		ShaderSourceAnsi.AddDefaulted(ShaderSource.Len() + 1);
		FCStringAnsi::Strncpy(ShaderSourceAnsi.GetData(), TCHAR_TO_ANSI(*ShaderSource), ShaderSourceAnsi.Num());

		const int32 ShaderSourceLength = ShaderSource.Len() + 1;
		const char* ShaderSourceCharPtr = ShaderSourceAnsi.GetData();
		glShaderSource(shader, 1, &ShaderSourceCharPtr, &ShaderSourceLength);
		glCompileShader(shader);

		GLint compiled;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE)
		{
			UE_LOG(LogMagicLeapMedia, Error, TEXT("Could not compile shader %d"), static_cast<int32>(ShaderType));
			int32 maxLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
			
			GLchar* errorLog = new GLchar[maxLength + 1];

			glGetShaderInfoLog(shader, maxLength, &maxLength, errorLog);

			UE_LOG(LogMagicLeapMedia, Error, TEXT("%s"), ANSI_TO_TCHAR(errorLog));

			delete[] errorLog;
			glDeleteShader(shader);
			shader = 0;
		}
	}
	return shader;
}

void FExternalOESTextureRenderer::UpdateVertexData()
{
	if (!bTriangleVerticesDirty || BlitBufferVBO <= 0)
	{
		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, BlitBufferVBO);
	glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), TriangleVertexData, GL_STATIC_DRAW);

	bTriangleVerticesDirty = false;
}

bool FExternalOESTextureRenderer::CopyFrameTexture(int32 DestTexture, MLHandle NativeBuffer, const FIntPoint& TextureDimensions, void* DestBuffer)
{
	if (!bInitialized)
	{
		bInitialized = InitContext();
		// bInitialized = true;  // set to true for now since we are not using the local context anyways.
		if (bInitialized)
		{
			SaveContext();
			MakeCurrent();
			InitSurfaceTexture();
			if (DestBuffer != nullptr)
			{
				glGenTextures(1, &ReadTexture);
				GLint prevTexture;
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
				glBindTexture(GL_TEXTURE_2D, ReadTexture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TextureDimensions.X, TextureDimensions.Y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
				glBindTexture(GL_TEXTURE_2D, prevTexture);
			}
			RestoreContext();
		}
		else
		{
			return false;
		}
	}

	bool previousBlend = false;
	bool previousCullFace = false;
	bool previousScissorTest = false;
	bool previousStencilTest = false;
	bool previousDepthTest = false;
	bool previousDither = false;
	GLint previousFBO = 0;
	GLint previousVBO = 0;
	GLint previousMinFilter = 0;
	GLint previousMagFilter = 0;
	GLint previousViewport[4];
	GLint previousProgram = -1;

	// Clear gl errors as they can creep in from the UE4 renderer.
	GLenum error = glGetError();
	if(error != GL_NO_ERROR)
	{
		UE_LOG(LogMagicLeapMedia, Error, TEXT("gl error %d"), static_cast<int32>(error));
	}

	previousBlend = glIsEnabled(GL_BLEND);
	previousCullFace = glIsEnabled(GL_CULL_FACE);
	previousScissorTest = glIsEnabled(GL_SCISSOR_TEST);
	previousStencilTest = glIsEnabled(GL_STENCIL_TEST);
	previousDepthTest = glIsEnabled(GL_DEPTH_TEST);
	previousDither = glIsEnabled(GL_DITHER);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousVBO);
	glGetIntegerv(GL_VIEWPORT, previousViewport);
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);

	GLint vb0enabled = 0;
	GLint vb0size = 0;
	GLint vb0type = 0;
	GLint vb0normalized = 0;
	GLint vb0stride = 0;
	GLint vb0bufferbinding = 0;
	glGetVertexAttribiv(PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &vb0enabled);
	glGetVertexAttribiv(PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_SIZE, &vb0size);
	glGetVertexAttribiv(PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_TYPE, &vb0type);
	glGetVertexAttribiv(PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &vb0normalized);
	glGetVertexAttribiv(PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &vb0stride);
	glGetVertexAttribiv(PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vb0bufferbinding);
	glGetVertexAttribPointerv(PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_POINTER, &vb0pointer);
	
	GLint vb1enabled = 0;
	GLint vb1size = 0;
	GLint vb1type = 0;
	GLint vb1normalized = 0;
	GLint vb1stride = 0;
	GLint vb1bufferbinding = 0;
	glGetVertexAttribiv(TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &vb1enabled);
	glGetVertexAttribiv(TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_SIZE, &vb1size);
	glGetVertexAttribiv(TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_TYPE, &vb1type);
	glGetVertexAttribiv(TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &vb1normalized);
	glGetVertexAttribiv(TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &vb1stride);
	glGetVertexAttribiv(TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vb1bufferbinding);
	glGetVertexAttribPointerv(TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_POINTER, &vb1pointer);

	glActiveTexture(GL_TEXTURE0);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &previousMinFilter);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &previousMagFilter);

	SaveContext();
	MakeCurrent();

	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_DITHER);
	glColorMask(true, true, true, true);

	// Wrap latest decoded frame into a new gl texture oject
	EGLImageKHR image = eglCreateImageKHR(eglGetCurrentDisplay(), EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, (EGLClientBuffer)(void *)NativeBuffer, NULL);
	if (image == EGL_NO_IMAGE_KHR)
	{
		EGLint errorcode = eglGetError();
		FString ErrorText;
		if (errorcode == EGL_BAD_DISPLAY)
		{
			ErrorText = TEXT("EGL_BAD_DISPLAY");
		}
		else if (errorcode == EGL_BAD_CONTEXT)
		{
			ErrorText = TEXT("EGL_BAD_CONTEXT");
		}
		else if (errorcode == EGL_BAD_PARAMETER)
		{
			ErrorText = TEXT("EGL_BAD_PARAMETER");
		}
		else if (errorcode == EGL_BAD_ACCESS)
		{
			ErrorText = TEXT("EGL_BAD_ACCESS");
		}
		else if (errorcode == EGL_BAD_ALLOC)
		{
			ErrorText = TEXT("EGL_BAD_ALLOC");
		}
		else if (errorcode == EGL_BAD_DISPLAY)
		{
			ErrorText = TEXT("EGL_BAD_DISPLAY");
		}
		else
		{
			ErrorText = TEXT("Unspecified error");      
		}
		UE_LOG(LogMagicLeapMedia, Error, TEXT("Failed to create EGLImage from the buffer. %s %d"), *ErrorText, errorcode);
		RestoreContext();
		return false;
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, TextureID);
	glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

	if (DestBuffer != nullptr)
	{
		DestTexture = static_cast<int32>(ReadTexture);
	}

	// Set the FBO to draw into the texture one-to-one.
	int oldTexture;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
	glBindTexture(GL_TEXTURE_2D, DestTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindTexture(GL_TEXTURE_2D, oldTexture);

	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, DestTexture, 0);

// #if	UE_BUILD_DEBUG
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		UE_LOG(LogMagicLeapMedia, Warning, TEXT("Failed to complete framebuffer attachment (%d)"), static_cast<int32>(status));
	}
// #endif

	glViewport(0, 0, TextureDimensions.X, TextureDimensions.Y);

	glUseProgram(Program);

	UpdateVertexData();
	
	glBindBuffer(GL_ARRAY_BUFFER, BlitBufferVBO);
	glEnableVertexAttribArray(PositionAttribLocation);
	glVertexAttribPointer(PositionAttribLocation, 2, GL_FLOAT, false, 4 * sizeof(float), 0);
	glEnableVertexAttribArray(TexCoordsAttribLocation);
	glVertexAttribPointer(TexCoordsAttribLocation, 2, GL_FLOAT, false, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

	glUniform1i(TextureUniformLocation, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, TextureID);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	if (DestBuffer != nullptr)
	{
		glReadPixels(0, 0, TextureDimensions.X, TextureDimensions.Y, GL_RGBA, GL_UNSIGNED_BYTE, DestBuffer);
	}

	eglDestroyImageKHR(eglGetCurrentDisplay(), image);

	glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

	RestoreContext();

	if (vb0enabled)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vb0bufferbinding);
		glVertexAttribPointer(PositionAttribLocation, vb0size, vb0type, vb0normalized, vb0stride, vb0pointer);
	}
	else
	{
		glDisableVertexAttribArray(PositionAttribLocation);
	}

	if (vb1bufferbinding)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vb1bufferbinding);
		glVertexAttribPointer(TexCoordsAttribLocation, vb1size, vb1type, vb1normalized, vb1stride, vb1pointer);
	}
	else
	{
		glDisableVertexAttribArray(TexCoordsAttribLocation);
	}

	// restore previous state
	glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
	glBindBuffer(GL_ARRAY_BUFFER, previousVBO);
	if (previousBlend) glEnable(GL_BLEND);
	if (previousCullFace) glEnable(GL_CULL_FACE);
	if (previousScissorTest) glEnable(GL_SCISSOR_TEST);
	if (previousStencilTest) glEnable(GL_STENCIL_TEST);
	if (previousDepthTest) glEnable(GL_DEPTH_TEST);
	if (previousDither) glEnable(GL_DITHER);
	glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
	glUseProgram(previousProgram);

	return true;
}

void FExternalOESTextureRenderer::Release()
{
	if (BlitBufferVBO > 0)
	{
		glDeleteBuffers(1, &BlitBufferVBO);
		BlitBufferVBO = -1;
	}
	if (Program > 0)
	{
		glDeleteProgram(Program);
		Program = -1;
	}
	if (FBO > 0)
	{
		glDeleteFramebuffers(1, &FBO);
		FBO = -1;
	}
	if (TextureID > 0)
	{
		glDeleteTextures(1, &TextureID);
		TextureID = -1;
	}
	if (ReadTexture > 0)
	{
		glDeleteTextures(1, &ReadTexture);
		ReadTexture = 0;
	}
}
