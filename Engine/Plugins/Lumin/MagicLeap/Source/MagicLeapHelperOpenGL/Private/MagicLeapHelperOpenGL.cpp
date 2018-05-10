// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

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