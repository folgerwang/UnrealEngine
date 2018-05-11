// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE:  All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law.  Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY.  Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure  of  this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of  COMPANY.   ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC  PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE  OF THIS
// SOURCE CODE  WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES.  THE RECEIPT OR POSSESSION OF  THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT  MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------*/
// %BANNER_END%

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
