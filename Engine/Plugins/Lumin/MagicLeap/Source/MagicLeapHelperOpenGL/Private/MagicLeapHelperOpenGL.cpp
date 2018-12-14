// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MagicLeapHelperOpenGL.h"
#include "IMagicLeapHelperOpenGLPlugin.h"
#include "Engine/Engine.h"

#if !PLATFORM_MAC
#include "OpenGLDrv.h"
#endif

class FMagicLeapHelperOpenGLPlugin : public IMagicLeapHelperOpenGLPlugin
{};

IMPLEMENT_MODULE(FMagicLeapHelperOpenGLPlugin, MagicLeapHelperOpenGL);

//////////////////////////////////////////////////////////////////////////

void FMagicLeapHelperOpenGL::CopyImageSubData(uint32 SrcName, int32 SrcLevel, int32 SrcX, int32 SrcY, int32 SrcZ, uint32 DstName, int32 DstLevel, int32 DstX, int32 DstY, int32 DstZ, int32 SrcWidth, int32 SrcHeight, int32 SrcDepth)
{
#if !PLATFORM_MAC
	FOpenGL::CopyImageSubData(SrcName, GL_TEXTURE_2D, SrcLevel, SrcX, SrcY, SrcZ, DstName, GL_TEXTURE_2D_ARRAY, DstLevel, DstX, DstY, DstZ, SrcWidth, SrcHeight, SrcDepth);
#endif
}

void FMagicLeapHelperOpenGL::BlitImage(uint32 SrcFBO, uint32 SrcName, int32 SrcLevel, int32 SrcX0, int32 SrcY0, int32 SrcX1, int32 SrcY1, uint32 DstFBO, uint32 DstName, int32 DstLevel, int32 DstX0, int32 DstY0, int32 DstX1, int32 DstY1)
{
#if !PLATFORM_MAC
	GLint CurrentFB = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &CurrentFB);

	GLint FramebufferSRGB = 0;
	glGetIntegerv(GL_FRAMEBUFFER_SRGB, &FramebufferSRGB);
	if (FramebufferSRGB)
	{
		glDisable(GL_FRAMEBUFFER_SRGB);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, SrcFBO);
	FOpenGL::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, SrcName, SrcLevel);

	glBindFramebuffer(GL_FRAMEBUFFER, DstFBO);
	FOpenGL::FramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, DstName, 0, DstLevel);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, SrcFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, DstFBO);
	FOpenGL::BlitFramebuffer(	SrcX0, SrcY0, SrcX1, SrcY1,
								DstX0, DstY0, DstX1, DstY1,
								GL_COLOR_BUFFER_BIT, GL_NEAREST);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, CurrentFB);
	if (FramebufferSRGB)
	{
		glEnable(GL_FRAMEBUFFER_SRGB);
	}
#endif
}