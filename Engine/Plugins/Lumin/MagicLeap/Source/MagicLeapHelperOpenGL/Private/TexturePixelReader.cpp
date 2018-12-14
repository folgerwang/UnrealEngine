// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TexturePixelReader.h"
#include "Misc/CString.h"
#include "IMagicLeapHelperOpenGLPlugin.h"
#include "RHIDefinitions.h"

#if !PLATFORM_MAC
#include "OpenGLDrv.h"
#endif

#if PLATFORM_MAC
	class FTecturePixelReaderImpl
	{};

	FTexturePixelReader::FTexturePixelReader() {}
	FTexturePixelReader::~FTexturePixelReader() {}
	bool FTexturePixelReader::RenderTextureToRenderBuffer(const UTexture2D& SrcTexture, uint8* PixelData)
	{
		return false;
	}

	void FTexturePixelReader::Init() {}
	void FTexturePixelReader::UpdateVertexData() {}
	void FTexturePixelReader::Release() {}

#else

class FTexturePixelReaderImpl
{
public:
	FTexturePixelReaderImpl()
	: RBO(-1)
	, FBO(-1)
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

		BlitFragmentShader = 
			FString(TEXT("uniform sampler2D SrcTexture;\n")) +
#if PLATFORM_LUMINGL4
			FString(TEXT("varying vec2 TexCoord;\n")) +
#else
			FString(TEXT("varying highp vec2 TexCoord;\n")) +
#endif
			FString(TEXT("void main()\n")) +
			FString(TEXT("{\n")) +
			// TODO: 1.0 - TexCoord.y should not be needed. Tackle the flip with the UV mapping.
			FString(TEXT("  gl_FragColor = texture2D(SrcTexture, TexCoord);\n")) +
			FString(TEXT("}\n"));
	}

	GLuint RBO;
	GLuint FBO;
	
	GLuint BlitVertexShaderID;
	GLuint BlitFragmentShaderID;
	GLuint Program;
	
	GLint PositionAttribLocation;
	GLint TexCoordsAttribLocation;
	GLint TextureUniformLocation;
	
	GLuint BlitBufferVBO;

	// UVs are flipped vertically.
	float TriangleVertexData[16] = {
		// X, Y, U, V
		-1.0f, -1.0f, 0.f, 0.f,
		1.0f, -1.0f, 1.f, 0.f,
		-1.0f, 1.0f, 0.f, 1.f,
		1.0f, 1.0f, 1.f, 1.f,
	};

	FString BlitVertexShader;
	FString BlitFragmentShader;
};

GLuint CreateShader(GLenum ShaderType, const FString& ShaderSource)
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
			UE_LOG(LogMagicLeapHelperOpenGL, Error, TEXT("Could not compile shader %d"), static_cast<int32>(ShaderType));
			int32 maxLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
			
			GLchar* errorLog = new GLchar[maxLength + 1];

			glGetShaderInfoLog(shader, maxLength, &maxLength, errorLog);

			UE_LOG(LogMagicLeapHelperOpenGL, Error, TEXT("%s"), ANSI_TO_TCHAR(errorLog));

			delete[] errorLog;
			glDeleteShader(shader);
			shader = 0;
		}
	}
	return shader;
}

FTexturePixelReader::FTexturePixelReader()
: Impl(new FTexturePixelReaderImpl())
{
	Init();
}

FTexturePixelReader::~FTexturePixelReader()
{
	Release();
	delete Impl;
}

void FTexturePixelReader::Init()
{
	if (!IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		UE_LOG(LogMagicLeapHelperOpenGL, Warning, TEXT("FTexturePixelReader is only supported on OpenGL."));
		return;
	}

	glGenRenderbuffers(1, &Impl->RBO);
	if (Impl->RBO <= 0)
	{
		Release();
		return;
	}

	glGenFramebuffers(1, &Impl->FBO);
	if (Impl->FBO <= 0)
	{
		Release();
		return;
	}

	Impl->BlitVertexShaderID = CreateShader(GL_VERTEX_SHADER, Impl->BlitVertexShader);
	if (Impl->BlitVertexShaderID == 0)
	{
		Release();
		return;
	}

	Impl->BlitFragmentShaderID = CreateShader(GL_FRAGMENT_SHADER, Impl->BlitFragmentShader);
	if (Impl->BlitFragmentShaderID == 0)
	{
		Release();
		return;
	}

	Impl->Program = glCreateProgram();
	glAttachShader(Impl->Program, Impl->BlitVertexShaderID);
	glAttachShader(Impl->Program, Impl->BlitFragmentShaderID);
	glLinkProgram(Impl->Program);

	glDetachShader(Impl->Program, Impl->BlitVertexShaderID);
	glDetachShader(Impl->Program, Impl->BlitFragmentShaderID);

	glDeleteShader(Impl->BlitVertexShaderID);
	glDeleteShader(Impl->BlitFragmentShaderID);

	Impl->BlitVertexShaderID = 0;
	Impl->BlitFragmentShaderID = 0;

	GLint linkStatus;
	glGetProgramiv(Impl->Program, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE)
	{
		UE_LOG(LogMagicLeapHelperOpenGL, Error, TEXT("Could not link program"));

		int32 maxLength = 0;
		glGetProgramiv(Impl->Program, GL_INFO_LOG_LENGTH, &maxLength);

		GLchar* errorLog = new GLchar[maxLength + 1];

		glGetProgramInfoLog(Impl->Program, maxLength, &maxLength, errorLog);

		UE_LOG(LogMagicLeapHelperOpenGL, Error, TEXT("%s"), ANSI_TO_TCHAR(errorLog));

		delete[] errorLog;

		glDeleteProgram(Impl->Program);
		Impl->Program = 0;
		Release();
		return;
	}

	Impl->PositionAttribLocation = glGetAttribLocation(Impl->Program, "Position");
	Impl->TexCoordsAttribLocation = glGetAttribLocation(Impl->Program, "TexCoords");
	Impl->TextureUniformLocation = glGetAttribLocation(Impl->Program, "SrcTexture");

	glGenBuffers(1, &Impl->BlitBufferVBO);
	if (Impl->BlitBufferVBO <= 0)
	{
		Release();
		return;
	}
}

void FTexturePixelReader::UpdateVertexData()
{
	if (Impl->BlitBufferVBO <= 0)
	{
		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, Impl->BlitBufferVBO);
	glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), Impl->TriangleVertexData, GL_STATIC_DRAW);
}

bool FTexturePixelReader::RenderTextureToRenderBuffer(const UTexture2D& SrcTexture, uint8* PixelData)
{
	if (!IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		return false;
	}
	
	GLboolean previousBlend = GL_FALSE;
	GLboolean previousCullFace = GL_FALSE;
	GLboolean previousScissorTest = GL_FALSE;
	GLboolean previousStencilTest = GL_FALSE;
	GLboolean previousDepthTest = GL_FALSE;
	GLboolean previousDither = GL_FALSE;
	GLint previousFBO = 0;
	GLint previousRBO = 0;
	GLint previousVBO = 0;
	GLint previousMinFilter = 0;
	GLint previousMagFilter = 0;
	GLint previousViewport[4];
	GLint previousProgram = -1;

	// Clear gl errors as they can creep in from the UE4 renderer.
	GLenum error = glGetError();
	if(error != GL_NO_ERROR)
	{
		UE_LOG(LogMagicLeapHelperOpenGL, Error, TEXT("gl error %d"), static_cast<int32>(error));
	}

	previousBlend = glIsEnabled(GL_BLEND);
	previousCullFace = glIsEnabled(GL_CULL_FACE);
	previousScissorTest = glIsEnabled(GL_SCISSOR_TEST);
	previousStencilTest = glIsEnabled(GL_STENCIL_TEST);
	previousDepthTest = glIsEnabled(GL_DEPTH_TEST);
	previousDither = glIsEnabled(GL_DITHER);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);
	glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRBO);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousVBO);
	glGetIntegerv(GL_VIEWPORT, previousViewport);
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);

	GLint vb0enabled = 0;
	GLint vb0size = 0;
	GLint vb0type = 0;
	GLint vb0normalized = 0;
	GLint vb0stride = 0;
	GLint vb0bufferbinding = 0;
	void *vb0pointer = 0;
	glGetVertexAttribiv(Impl->PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &vb0enabled);
	glGetVertexAttribiv(Impl->PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_SIZE, &vb0size);
	glGetVertexAttribiv(Impl->PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_TYPE, &vb0type);
	glGetVertexAttribiv(Impl->PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &vb0normalized);
	glGetVertexAttribiv(Impl->PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &vb0stride);
	glGetVertexAttribiv(Impl->PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vb0bufferbinding);
	glGetVertexAttribPointerv(Impl->PositionAttribLocation, GL_VERTEX_ATTRIB_ARRAY_POINTER, &vb0pointer);
	
	GLint vb1enabled = 0;
	GLint vb1size = 0;
	GLint vb1type = 0;
	GLint vb1normalized = 0;
	GLint vb1stride = 0;
	GLint vb1bufferbinding = 0;
	void *vb1pointer = 0;
	glGetVertexAttribiv(Impl->TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &vb1enabled);
	glGetVertexAttribiv(Impl->TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_SIZE, &vb1size);
	glGetVertexAttribiv(Impl->TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_TYPE, &vb1type);
	glGetVertexAttribiv(Impl->TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &vb1normalized);
	glGetVertexAttribiv(Impl->TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &vb1stride);
	glGetVertexAttribiv(Impl->TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &vb1bufferbinding);
	glGetVertexAttribPointerv(Impl->TexCoordsAttribLocation, GL_VERTEX_ATTRIB_ARRAY_POINTER, &vb1pointer);

	glActiveTexture(GL_TEXTURE0);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &previousMinFilter);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &previousMagFilter);

	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_DITHER);
	glColorMask(true, true, true, true);

	glViewport(0, 0, SrcTexture.GetSurfaceWidth(), SrcTexture.GetSurfaceHeight());

	glBindRenderbuffer(GL_RENDERBUFFER, Impl->RBO);
#if PLATFORM_LUMINGL4
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, SrcTexture.GetSurfaceWidth(), SrcTexture.GetSurfaceHeight());
#else
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, SrcTexture.GetSurfaceWidth(), SrcTexture.GetSurfaceHeight());
#endif

	glBindFramebuffer(GL_FRAMEBUFFER, Impl->FBO);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, Impl->RBO);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		UE_LOG(LogMagicLeapHelperOpenGL, Error, TEXT("Failed to complete framebuffer attachment (%d)"), static_cast<int32>(status));
		return false;
	}

	glUseProgram(Impl->Program);

	UpdateVertexData();
	
	glBindBuffer(GL_ARRAY_BUFFER, Impl->BlitBufferVBO);
	glEnableVertexAttribArray(Impl->PositionAttribLocation);
	glVertexAttribPointer(Impl->PositionAttribLocation, 2, GL_FLOAT, false, 4 * sizeof(float), 0);
	glEnableVertexAttribArray(Impl->TexCoordsAttribLocation);
	glVertexAttribPointer(Impl->TexCoordsAttribLocation, 2, GL_FLOAT, false, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

	glClear(GL_COLOR_BUFFER_BIT);

	glActiveTexture(GL_TEXTURE0);
	int32 SrcTextureID = *reinterpret_cast<int32*>(SrcTexture.Resource->TextureRHI->GetNativeResource());
	int oldTexture;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
	glBindTexture(GL_TEXTURE_2D, SrcTextureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glUniform1i(Impl->TextureUniformLocation, 0);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels(0, 0, SrcTexture.GetSurfaceWidth(), SrcTexture.GetSurfaceHeight(), GL_RGBA, GL_UNSIGNED_BYTE, PixelData);

	// restore previous state
	if (vb0enabled)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vb0bufferbinding);
		glVertexAttribPointer(Impl->PositionAttribLocation, vb0size, vb0type, vb0normalized, vb0stride, vb0pointer);
	}
	else
	{
		glDisableVertexAttribArray(Impl->PositionAttribLocation);
	}

	if (vb1bufferbinding)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vb1bufferbinding);
		glVertexAttribPointer(Impl->TexCoordsAttribLocation, vb1size, vb1type, vb1normalized, vb1stride, vb1pointer);
	}
	else
	{
		glDisableVertexAttribArray(Impl->TexCoordsAttribLocation);
	}

	glBindTexture(GL_TEXTURE_2D, oldTexture);
	glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, previousRBO);
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

void FTexturePixelReader::Release()
{
	if (!IsOpenGLPlatform(GMaxRHIShaderPlatform))
	{
		return;
	}

	if (Impl->BlitBufferVBO > 0)
	{
		glDeleteBuffers(1, &Impl->BlitBufferVBO);
		Impl->BlitBufferVBO = -1;
	}
	if (Impl->Program > 0)
	{
		glDeleteProgram(Impl->Program);
		Impl->Program = -1;
	}
	if (Impl->RBO > 0)
	{
		glDeleteRenderbuffers(1, &Impl->RBO);
		Impl->RBO = -1;
	}
	if (Impl->FBO > 0)
	{
		glDeleteFramebuffers(1, &Impl->FBO);
		Impl->FBO = -1;
	}
}

#endif // PLATFORM_MAC