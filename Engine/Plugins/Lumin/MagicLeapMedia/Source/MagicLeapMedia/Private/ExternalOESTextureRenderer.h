// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ml_api.h"

#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#endif // !EGL_EGLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif // !GL_GLEXT_PROTOTYPES

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

/**
 * Unreal doesn't natively support GL_TEXTURE_EXTERNAL_OES type textures. 
 * This class copies that texture to a frame buffer bound to a GL_TEXTURE_2D which is created by Unreal.
 */
class FExternalOESTextureRenderer
{
public:
	FExternalOESTextureRenderer(bool bUseOwnContext);
	virtual ~FExternalOESTextureRenderer();

	bool CopyFrameTexture(int32 DestTexture, MLHandle NativeBuffer, const FIntPoint& TextureDimensions, void* DestBuffer);

private:
	bool InitContext();
	void SaveContext();
	void MakeCurrent();
	void RestoreContext();
	void ResetInternal();
	void InitSurfaceTexture();
	GLuint CreateShader(GLenum ShaderType, const FString& ShaderSource);
	void UpdateVertexData();
	void Release();

	GLuint TextureID;
	GLuint FBO;

	GLuint ReadTexture;

	GLuint BlitVertexShaderID;
	GLuint BlitFragmentShaderID;
	GLuint Program;

	GLint PositionAttribLocation;
	GLint TexCoordsAttribLocation;
	GLint TextureUniformLocation;

	GLuint BlitBufferVBO;
	float TriangleVertexData[16] = {
		// X, Y, U, V
		-1.0f, -1.0f, 0.f, 0.f,
		1.0f, -1.0f, 1.f, 0.f,
		-1.0f, 1.0f, 0.f, 1.f,
		1.0f, 1.0f, 1.f, 1.f,
	};

	bool bTriangleVerticesDirty;

	FString BlitVertexShader;
	FString BlitFragmentShaderBGRA;

	EGLDisplay Display;
	EGLContext Context;

	EGLDisplay SavedDisplay;
	EGLContext SavedContext;

	bool bUseIsolatedContext;
	bool bInitialized;

	bool bSupportsKHRCreateContext;
	int *ContextAttributes;

	void *vb0pointer;
	void *vb1pointer;
};
