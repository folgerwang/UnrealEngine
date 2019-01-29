// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidOpenGL.h: Public OpenGL ES definitions for Android-specific functionality
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "RenderingThread.h"
#include "RHI.h"

#if PLATFORM_ANDROID

#include "AndroidEGL.h"

#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <GLES2/gl2ext.h>
	
typedef EGLSyncKHR UGLsync;
#define GLdouble		GLfloat
typedef khronos_int64_t GLint64;
typedef khronos_uint64_t GLuint64;
#define GL_CLAMP		GL_CLAMP_TO_EDGE

#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY	GL_WRITE_ONLY_OES
#endif

#define glTexEnvi(...)

#ifndef GL_RGBA8
#define GL_RGBA8		GL_RGBA // or GL_RGBA8_OES ?
#endif

#define GL_BGRA			GL_BGRA_EXT 
#define GL_UNSIGNED_INT_8_8_8_8_REV	GL_UNSIGNED_BYTE
#define glMapBuffer		glMapBufferOESa
#define glUnmapBuffer	glUnmapBufferOESa

#ifndef GL_HALF_FLOAT
#define GL_HALF_FLOAT	GL_HALF_FLOAT_OES
#endif

#define GL_COMPRESSED_RGB8_ETC2           0x9274
#define GL_COMPRESSED_SRGB8_ETC2          0x9275
#define GL_COMPRESSED_RGBA8_ETC2_EAC      0x9278
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC 0x9279

#define GL_READ_FRAMEBUFFER_NV					0x8CA8
#define GL_DRAW_FRAMEBUFFER_NV					0x8CA9

typedef void (GL_APIENTRYP PFNBLITFRAMEBUFFERNVPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
extern PFNBLITFRAMEBUFFERNVPROC					glBlitFramebufferNV ;

#define GL_QUERY_COUNTER_BITS_EXT				0x8864
#define GL_CURRENT_QUERY_EXT					0x8865
#define GL_QUERY_RESULT_EXT						0x8866
#define GL_QUERY_RESULT_AVAILABLE_EXT			0x8867
#define GL_SAMPLES_PASSED_EXT					0x8914
#define GL_ANY_SAMPLES_PASSED_EXT				0x8C2F


typedef void (GL_APIENTRYP PFNGLGENQUERIESEXTPROC) (GLsizei n, GLuint *ids);
typedef void (GL_APIENTRYP PFNGLDELETEQUERIESEXTPROC) (GLsizei n, const GLuint *ids);
typedef GLboolean (GL_APIENTRYP PFNGLISQUERYEXTPROC) (GLuint id);
typedef void (GL_APIENTRYP PFNGLBEGINQUERYEXTPROC) (GLenum target, GLuint id);
typedef void (GL_APIENTRYP PFNGLENDQUERYEXTPROC) (GLenum target);
typedef void (GL_APIENTRYP PFNGLQUERYCOUNTEREXTPROC) (GLuint id, GLenum target);
typedef void (GL_APIENTRYP PFNGLGETQUERYIVEXTPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTIVEXTPROC) (GLuint id, GLenum pname, GLint *params);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTUIVEXTPROC) (GLuint id, GLenum pname, GLuint *params);
typedef void (GL_APIENTRYP PFNGLGETQUERYOBJECTUI64VEXTPROC) (GLuint id, GLenum pname, GLuint64 *params);
typedef void* (GL_APIENTRYP PFNGLMAPBUFFEROESPROC) (GLenum target, GLenum access);
typedef GLboolean (GL_APIENTRYP PFNGLUNMAPBUFFEROESPROC) (GLenum target);
typedef void (GL_APIENTRYP PFNGLPUSHGROUPMARKEREXTPROC) (GLsizei length, const GLchar *marker);
typedef void (GL_APIENTRYP PFNGLLABELOBJECTEXTPROC) (GLenum type, GLuint object, GLsizei length, const GLchar *label);
typedef void (GL_APIENTRYP PFNGLGETOBJECTLABELEXTPROC) (GLenum type, GLuint object, GLsizei bufSize, GLsizei *length, GLchar *label);
typedef void (GL_APIENTRYP PFNGLPOPGROUPMARKEREXTPROC) (void);
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples);
typedef void (GL_APIENTRYP PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
/** from ES 3.0 but can be called on certain Adreno devices */
typedef void (GL_APIENTRYP PFNGLTEXSTORAGE2DPROC) (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);

// Mobile multi-view
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews);
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLsizei samples, GLint baseViewIndex, GLsizei numViews);

typedef void (GL_APIENTRYP PFNGLCOPYIMAGESUBDATAPROC) (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei width, GLsizei height, GLsizei depth);

extern PFNGLGENQUERIESEXTPROC 			glGenQueriesEXT;
extern PFNGLDELETEQUERIESEXTPROC 		glDeleteQueriesEXT;
extern PFNGLISQUERYEXTPROC 				glIsQueryEXT ;
extern PFNGLBEGINQUERYEXTPROC 			glBeginQueryEXT;
extern PFNGLENDQUERYEXTPROC 			glEndQueryEXT;
extern PFNGLQUERYCOUNTEREXTPROC			glQueryCounterEXT;
extern PFNGLGETQUERYIVEXTPROC 			glGetQueryivEXT;  
extern PFNGLGETQUERYOBJECTUIVEXTPROC 	glGetQueryObjectuivEXT;
extern PFNGLGETQUERYOBJECTUI64VEXTPROC	glGetQueryObjectui64vEXT;
extern PFNGLMAPBUFFEROESPROC			glMapBufferOESa;
extern PFNGLUNMAPBUFFEROESPROC			glUnmapBufferOESa;
extern PFNGLDISCARDFRAMEBUFFEREXTPROC 	glDiscardFramebufferEXT ;
extern PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC	glFramebufferTexture2DMultisampleEXT;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC	glRenderbufferStorageMultisampleEXT;
extern PFNGLPUSHGROUPMARKEREXTPROC		glPushGroupMarkerEXT;
extern PFNGLLABELOBJECTEXTPROC			glLabelObjectEXT;
extern PFNGLGETOBJECTLABELEXTPROC		glGetObjectLabelEXT;
extern PFNGLPOPGROUPMARKEREXTPROC 		glPopGroupMarkerEXT;
extern PFNGLTEXSTORAGE2DPROC			glTexStorage2D;
extern PFNGLDEBUGMESSAGECONTROLKHRPROC	glDebugMessageControlKHR;
extern PFNGLDEBUGMESSAGEINSERTKHRPROC	glDebugMessageInsertKHR;
extern PFNGLDEBUGMESSAGECALLBACKKHRPROC	glDebugMessageCallbackKHR;
extern PFNGLGETDEBUGMESSAGELOGKHRPROC	glDebugMessageLogKHR;
extern PFNGLGETPOINTERVKHRPROC			glGetPointervKHR;
extern PFNGLPUSHDEBUGGROUPKHRPROC		glPushDebugGroupKHR;
extern PFNGLPOPDEBUGGROUPKHRPROC		glPopDebugGroupKHR;
extern PFNGLOBJECTLABELKHRPROC			glObjectLabelKHR;
extern PFNGLGETOBJECTLABELKHRPROC		glGetObjectLabelKHR;
extern PFNGLOBJECTPTRLABELKHRPROC		glObjectPtrLabelKHR;
extern PFNGLGETOBJECTPTRLABELKHRPROC	glGetObjectPtrLabelKHR;
extern PFNGLDRAWELEMENTSINSTANCEDPROC	glDrawElementsInstanced;
extern PFNGLDRAWARRAYSINSTANCEDPROC		glDrawArraysInstanced;
extern PFNGLVERTEXATTRIBDIVISORPROC		glVertexAttribDivisor;

extern PFNGLTEXBUFFEREXTPROC			glTexBufferEXT;
extern PFNGLUNIFORM4UIVPROC				glUniform4uiv;
extern PFNGLCLEARBUFFERFIPROC			glClearBufferfi;
extern PFNGLCLEARBUFFERFVPROC			glClearBufferfv;
extern PFNGLCLEARBUFFERIVPROC			glClearBufferiv;
extern PFNGLCLEARBUFFERUIVPROC			glClearBufferuiv;
extern PFNGLDRAWBUFFERSPROC				glDrawBuffers;
extern PFNGLTEXIMAGE3DPROC				glTexImage3D;
extern PFNGLTEXSUBIMAGE3DPROC			glTexSubImage3D;
extern PFNGLCOMPRESSEDTEXIMAGE3DPROC    glCompressedTexImage3D;
extern PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC	glCompressedTexSubImage3D;
extern PFNGLCOPYTEXSUBIMAGE3DPROC		glCopyTexSubImage3D;
extern PFNGLCOPYIMAGESUBDATAPROC		glCopyImageSubData;

extern PFNGLGETPROGRAMBINARYOESPROC     glGetProgramBinary;
extern PFNGLPROGRAMBINARYOESPROC        glProgramBinary;

extern PFNGLBINDBUFFERRANGEPROC			glBindBufferRange;
extern PFNGLBINDBUFFERBASEPROC			glBindBufferBase;
extern PFNGLGETUNIFORMBLOCKINDEXPROC	glGetUniformBlockIndex;
extern PFNGLUNIFORMBLOCKBINDINGPROC		glUniformBlockBinding;
extern PFNGLBLITFRAMEBUFFERPROC			glBlitFramebuffer;

extern PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR;
extern PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR;
extern PFNGLVERTEXATTRIBIPOINTERPROC	glVertexAttribIPointer;

extern PFNGLGENSAMPLERSPROC				glGenSamplers;
extern PFNGLDELETESAMPLERSPROC			glDeleteSamplers;
extern PFNGLSAMPLERPARAMETERIPROC		glSamplerParameteri;
extern PFNGLBINDSAMPLERPROC				glBindSampler;

extern PFNGLPROGRAMPARAMETERIPROC		glProgramParameteri;

#include "OpenGLES2.h"

typedef khronos_stime_nanoseconds_t EGLnsecsANDROID;

typedef GLboolean(GL_APIENTRYP PFNeglPresentationTimeANDROID) (EGLDisplay dpy, EGLSurface surface, EGLnsecsANDROID time);
typedef GLboolean(GL_APIENTRYP PFNeglGetNextFrameIdANDROID) (EGLDisplay dpy, EGLSurface surface, EGLuint64KHR *frameId);
typedef GLboolean(GL_APIENTRYP PFNeglGetCompositorTimingANDROID) (EGLDisplay dpy, EGLSurface surface, EGLint numTimestamps, const EGLint *names, EGLnsecsANDROID *values);
typedef GLboolean(GL_APIENTRYP PFNeglGetFrameTimestampsANDROID) (EGLDisplay dpy, EGLSurface surface, EGLuint64KHR frameId, EGLint numTimestamps, const EGLint *timestamps, EGLnsecsANDROID *values);
typedef GLboolean(GL_APIENTRYP PFNeglQueryTimestampSupportedANDROID) (EGLDisplay dpy, EGLSurface surface, EGLint timestamp);

#define EGL_TIMESTAMPS_ANDROID 0x3430
#define EGL_COMPOSITE_DEADLINE_ANDROID 0x3431
#define EGL_COMPOSITE_INTERVAL_ANDROID 0x3432
#define EGL_COMPOSITE_TO_PRESENT_LATENCY_ANDROID 0x3433
#define EGL_REQUESTED_PRESENT_TIME_ANDROID 0x3434
#define EGL_RENDERING_COMPLETE_TIME_ANDROID 0x3435
#define EGL_COMPOSITION_LATCH_TIME_ANDROID 0x3436
#define EGL_FIRST_COMPOSITION_START_TIME_ANDROID 0x3437
#define EGL_LAST_COMPOSITION_START_TIME_ANDROID 0x3438
#define EGL_FIRST_COMPOSITION_GPU_FINISHED_TIME_ANDROID 0x3439
#define EGL_DISPLAY_PRESENT_TIME_ANDROID 0x343A
#define EGL_DEQUEUE_READY_TIME_ANDROID 0x343B
#define EGL_READS_DONE_TIME_ANDROID 0x343C
#define EGL_TIMESTAMP_PENDING_ANDROID - 2
#define EGL_TIMESTAMP_INVALID_ANDROID - 1

extern "C"
{
	extern PFNEGLGETSYSTEMTIMENVPROC eglGetSystemTimeNV_p;
	extern PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR_p;
	extern PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR_p;
	extern PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR_p;
	extern PFNEGLGETSYNCATTRIBKHRPROC eglGetSyncAttribKHR_p;

	extern PFNeglPresentationTimeANDROID eglPresentationTimeANDROID_p;
	extern PFNeglGetNextFrameIdANDROID eglGetNextFrameIdANDROID_p;
	extern PFNeglGetCompositorTimingANDROID eglGetCompositorTimingANDROID_p;
	extern PFNeglGetFrameTimestampsANDROID eglGetFrameTimestampsANDROID_p;
	extern PFNeglQueryTimestampSupportedANDROID eglQueryTimestampSupportedANDROID_p;
	extern PFNeglQueryTimestampSupportedANDROID eglGetCompositorTimingSupportedANDROID_p;
	extern PFNeglQueryTimestampSupportedANDROID eglGetFrameTimestampsSupportedANDROID_p;
}

struct FAndroidOpenGL : public FOpenGLES2
{
	static FORCEINLINE bool IsES31Usable()
	{
		check(CurrentFeatureLevelSupport != EFeatureLevelSupport::Invalid);
		return CurrentFeatureLevelSupport >= EFeatureLevelSupport::ES31;
	}

	static FORCEINLINE bool IsES32Usable()
	{
		check(CurrentFeatureLevelSupport != EFeatureLevelSupport::Invalid);
		return CurrentFeatureLevelSupport == EFeatureLevelSupport::ES32;
	}

	static FORCEINLINE EShaderPlatform GetShaderPlatform()
	{
		return IsES31Usable() ? SP_OPENGL_ES3_1_ANDROID : SP_OPENGL_ES2_ANDROID;
	}

	static FORCEINLINE ERHIFeatureLevel::Type GetFeatureLevel()
	{
		return IsES31Usable() ? ERHIFeatureLevel::ES3_1 : ERHIFeatureLevel::ES2;
	}

	static FORCEINLINE bool SupportsUniformBuffers() { return IsES31Usable(); }

	static FORCEINLINE bool HasHardwareHiddenSurfaceRemoval() { return bHasHardwareHiddenSurfaceRemoval; };

	// Optional:
	static void QueryTimestampCounter(GLuint QueryID);

	static GLuint MakeVirtualQueryReal(GLuint QueryID);

	static FORCEINLINE void GenQueries(GLsizei NumQueries, GLuint* QueryIDs)
	{
		*(char*)3 = 0; // this is virtualized and should not be called
	}

	static void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint *OutResult);

	static void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint64* OutResult);

	static void BeginQuery(GLenum QueryType, GLuint QueryId);

	static void EndQuery(GLenum QueryType);

	static bool SupportsFramebufferSRGBEnable();

	static FORCEINLINE void DeleteSync(UGLsync Sync)
	{
		if (GUseThreadedRendering)
		{
			//handle error here
			EGLBoolean Result = eglDestroySyncKHR_p( AndroidEGL::GetInstance()->GetDisplay(), Sync );
			if(Result == EGL_FALSE)
			{
				//handle error here
			}
		}
	}

	static FORCEINLINE UGLsync FenceSync(GLenum Condition, GLbitfield Flags)
	{
		check(Condition == GL_SYNC_GPU_COMMANDS_COMPLETE && Flags == 0);
		return GUseThreadedRendering ? eglCreateSyncKHR_p( AndroidEGL::GetInstance()->GetDisplay(), EGL_SYNC_FENCE_KHR, NULL ) : 0;
	}
	
	static FORCEINLINE bool IsSync(UGLsync Sync)
	{
		if(GUseThreadedRendering)
		{
			return (Sync != EGL_NO_SYNC_KHR) ? true : false;
		}
		else
		{
			return true;
		}
	}

	static FORCEINLINE EFenceResult ClientWaitSync(UGLsync Sync, GLbitfield Flags, GLuint64 Timeout)
	{
		if (GUseThreadedRendering)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_eglClientWaitSyncKHR_p);
			// check( Flags == GL_SYNC_FLUSH_COMMANDS_BIT );
			GLenum Result = eglClientWaitSyncKHR_p( AndroidEGL::GetInstance()->GetDisplay(), Sync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, Timeout );
			switch (Result)
			{
			case EGL_TIMEOUT_EXPIRED_KHR:		return FR_TimeoutExpired;
			case EGL_CONDITION_SATISFIED_KHR:	return FR_ConditionSatisfied;
			}
			return FR_WaitFailed;
		}
		else
		{
			return FR_ConditionSatisfied;
		}
		return FR_WaitFailed;
	}

	static FORCEINLINE void FramebufferTexture2D(GLenum Target, GLenum Attachment, GLenum TexTarget, GLuint Texture, GLint Level)
	{
		check(Attachment == GL_COLOR_ATTACHMENT0 || Attachment == GL_DEPTH_ATTACHMENT || Attachment == GL_STENCIL_ATTACHMENT 
				|| (SupportsMultipleRenderTargets() && Attachment >= GL_COLOR_ATTACHMENT0 && Attachment <= GL_COLOR_ATTACHMENT7));

		glFramebufferTexture2D(Target, Attachment, TexTarget, Texture, Level);
		VERIFY_GL(FramebufferTexture_2D);
	}
	
	// Required:
	static FORCEINLINE void BlitFramebuffer(GLint SrcX0, GLint SrcY0, GLint SrcX1, GLint SrcY1, GLint DstX0, GLint DstY0, GLint DstX1, GLint DstY1, GLbitfield Mask, GLenum Filter)
	{
		if(glBlitFramebufferNV)
		{
			glBlitFramebufferNV(SrcX0, SrcY0, SrcX1, SrcY1, DstX0, DstY0, DstX1, DstY1, Mask, Filter);
		}
		else if (IsES31Usable())
		{
			glBlitFramebuffer(SrcX0, SrcY0, SrcX1, SrcY1, DstX0, DstY0, DstX1, DstY1, Mask, Filter);
		}
		
	}

	static FORCEINLINE bool TexStorage2D(GLenum Target, GLint Levels, GLint InternalFormat, GLsizei Width, GLsizei Height, GLenum Format, GLenum Type, uint32 Flags)
	{
		// glTexStorage2D accepts only sized internal formats and thus we reject base formats
		// also GL_BGRA8_EXT seems to be unsupported
		bool bValidFormat = true;
		switch (InternalFormat)
		{
			case GL_DEPTH_COMPONENT:
			case GL_DEPTH_STENCIL:
			case GL_RED:
			case GL_RG:
			case GL_RGB:
			case GL_RGBA:
			case GL_BGRA_EXT:
			case GL_BGRA8_EXT:
			case GL_LUMINANCE:
			case GL_LUMINANCE_ALPHA:
			case GL_ALPHA:
			case GL_RED_INTEGER:
			case GL_RG_INTEGER:
			case GL_RGB_INTEGER:
			case GL_RGBA_INTEGER:
				bValidFormat = false;
				break;
		}

		if (bES30Support && (bValidFormat || (bUseHalfFloatTexStorage && Type == GetTextureHalfFloatPixelType() && (Flags & TexCreate_RenderTargetable) != 0)))
		{
			glTexStorage2D(Target, Levels, InternalFormat, Width, Height);
			VERIFY_GL(glTexStorage2D);
			return true;
		}

		return false;
	}

	static FORCEINLINE void DrawArraysInstanced(GLenum Mode, GLint First, GLsizei Count, GLsizei InstanceCount)
	{
		check(SupportsInstancing());
		glDrawArraysInstanced(Mode, First, Count, InstanceCount);
	}

	static FORCEINLINE void DrawElementsInstanced(GLenum Mode, GLsizei Count, GLenum Type, const GLvoid* Indices, GLsizei InstanceCount)
	{
		check(SupportsInstancing());
		glDrawElementsInstanced(Mode, Count, Type, Indices, InstanceCount);
	}

	static FORCEINLINE void VertexAttribDivisor(GLuint Index, GLuint Divisor)
	{
		if (SupportsInstancing())
		{
			glVertexAttribDivisor(Index, Divisor);
		}
	}
	
	static FORCEINLINE void TexStorage3D(GLenum Target, GLint Levels, GLint InternalFormat, GLsizei Width, GLsizei Height, GLsizei Depth, GLenum Format, GLenum Type)
	{
		const bool bArrayTexture = Target == GL_TEXTURE_2D_ARRAY || Target == GL_TEXTURE_CUBE_MAP_ARRAY;
		for(uint32 MipIndex = 0; MipIndex < uint32(Levels); MipIndex++)
		{
			glTexImage3D(
				Target,
				MipIndex,
				InternalFormat,
				FMath::Max<uint32>(1,(Width >> MipIndex)),
				FMath::Max<uint32>(1,(Height >> MipIndex)),
				(bArrayTexture) ? Depth : FMath::Max<uint32>(1,(Depth >> MipIndex)),
				0,
				Format,
				Type,
				NULL
				);

			VERIFY_GL(TexImage_3D);
		}
	}
	
	static FORCEINLINE void TexImage3D(GLenum Target, GLint Level, GLint InternalFormat, GLsizei Width, GLsizei Height, GLsizei Depth, GLint Border, GLenum Format, GLenum Type, const GLvoid* PixelData)
	{
		glTexImage3D(Target, Level, InternalFormat, Width, Height, Depth, Border, Format, Type, PixelData);
	}

	static FORCEINLINE void CompressedTexImage3D(GLenum Target, GLint Level, GLenum InternalFormat, GLsizei Width, GLsizei Height, GLsizei Depth, GLint Border, GLsizei ImageSize, const GLvoid* PixelData)
	{
		glCompressedTexImage3D(Target, Level, InternalFormat, Width, Height, Depth, Border, ImageSize, PixelData);
	}

	static FORCEINLINE void TexSubImage3D(GLenum Target, GLint Level, GLint XOffset, GLint YOffset, GLint ZOffset, GLsizei Width, GLsizei Height, GLsizei Depth, GLenum Format, GLenum Type, const GLvoid* PixelData)
	{
		glTexSubImage3D(Target, Level, XOffset, YOffset, ZOffset, Width, Height, Depth, Format, Type, PixelData);
	}

	static FORCEINLINE void	CopyTexSubImage3D(GLenum Target, GLint Level, GLint XOffset, GLint YOffset, GLint ZOffset, GLint X, GLint Y, GLsizei Width, GLsizei Height)
	{
		glCopyTexSubImage3D(Target, Level, XOffset, YOffset, ZOffset, X, Y, Width, Height);
	}
	
	static FORCEINLINE void CopyImageSubData(GLuint SrcName, GLenum SrcTarget, GLint SrcLevel, GLint SrcX, GLint SrcY, GLint SrcZ, GLuint DstName, GLenum DstTarget, GLint DstLevel, GLint DstX, GLint DstY, GLint DstZ, GLsizei Width, GLsizei Height, GLsizei Depth)
	{
		check(bSupportsCopyImage);

		glCopyImageSubData(SrcName, SrcTarget, SrcLevel, SrcX, SrcY, SrcZ, DstName, DstTarget, DstLevel, DstX, DstY, DstZ, Width, Height, Depth);
	}

	static FORCEINLINE void ClearBufferfv(GLenum Buffer, GLint DrawBufferIndex, const GLfloat* Value)
	{
		glClearBufferfv(Buffer, DrawBufferIndex, Value);
	}

	static FORCEINLINE void ClearBufferfi(GLenum Buffer, GLint DrawBufferIndex, GLfloat Depth, GLint Stencil)
	{
		glClearBufferfi(Buffer, DrawBufferIndex, Depth, Stencil);
	}

	static FORCEINLINE void ClearBufferiv(GLenum Buffer, GLint DrawBufferIndex, const GLint* Value)
	{
		glClearBufferiv(Buffer, DrawBufferIndex, Value);
	}

	static FORCEINLINE void DrawBuffers(GLsizei NumBuffers, const GLenum* Buffers)
	{
		glDrawBuffers(NumBuffers, Buffers);
	}
	
	static FORCEINLINE void ColorMaskIndexed(GLuint Index, GLboolean Red, GLboolean Green, GLboolean Blue, GLboolean Alpha)
	{
		check(Index == 0 || SupportsMultipleRenderTargets());
		glColorMask(Red, Green, Blue, Alpha);
	}

	static FORCEINLINE void TexBuffer(GLenum Target, GLenum InternalFormat, GLuint Buffer)
	{
		glTexBufferEXT(Target, InternalFormat, Buffer);
	}

	static FORCEINLINE void ProgramUniform4uiv(GLuint Program, GLint Location, GLsizei Count, const GLuint *Value)
	{
		glUniform4uiv(Location, Count, Value);
	}

	static FORCEINLINE bool SupportsProgramBinary() { return bSupportsProgramBinary; }

	static FORCEINLINE void GetProgramBinary(GLuint Program, GLsizei BufSize, GLsizei *Length, GLenum *BinaryFormat, void *Binary)
	{
		glGetProgramBinary(Program, BufSize, Length, BinaryFormat, Binary);
	}

	static FORCEINLINE void ProgramBinary(GLuint Program, GLenum BinaryFormat, const void *Binary, GLsizei Length)
	{
		glProgramBinary(Program, BinaryFormat, Binary, Length);
	}

	static FORCEINLINE void ProgramParameter(GLuint Program, GLenum PName, GLint Value)
	{
		if (bES30Support)
		{
			check(glProgramParameteri);
			glProgramParameteri(Program, PName, Value);
		}
		else
		{
			FOpenGLBase::ProgramParameter(Program, PName, Value);
		}
	}

	static FORCEINLINE void BindBufferBase(GLenum Target, GLuint Index, GLuint Buffer)
	{
		check(IsES31Usable());
		glBindBufferBase(Target, Index, Buffer);
	}

	static FORCEINLINE void BindBufferRange(GLenum Target, GLuint Index, GLuint Buffer, GLintptr Offset, GLsizeiptr Size)
	{
		check(IsES31Usable());
		glBindBufferRange(Target, Index, Buffer, Offset, Size);
	}
	
	static FORCEINLINE GLuint GetUniformBlockIndex(GLuint Program, const GLchar *UniformBlockName)
	{
		check(IsES31Usable());
		return glGetUniformBlockIndex(Program, UniformBlockName);
	}

	static FORCEINLINE void UniformBlockBinding(GLuint Program, GLuint UniformBlockIndex, GLuint UniformBlockBinding)
	{
		check(IsES31Usable());
		glUniformBlockBinding(Program, UniformBlockIndex, UniformBlockBinding);
	}

	static FORCEINLINE void BufferSubData(GLenum Target, GLintptr Offset, GLsizeiptr Size, const GLvoid* Data)
	{
		check(Target == GL_ARRAY_BUFFER || Target == GL_ELEMENT_ARRAY_BUFFER || (Target == GL_UNIFORM_BUFFER && IsES31Usable()) );
		glBufferSubData(Target, Offset, Size, Data);
	}

	static FORCEINLINE void VertexAttribIPointer(GLuint Index, GLint Size, GLenum Type, GLsizei Stride, const GLvoid* Pointer)
	{
		if (IsES31Usable())
		{
			glVertexAttribIPointer(Index, Size, Type, Stride, Pointer);
		}
		else
		{
			glVertexAttribPointer(Index, Size, Type, GL_FALSE, Stride, Pointer);
		}
	}

	static FORCEINLINE void GenSamplers(GLsizei Count, GLuint* Samplers)
	{
		glGenSamplers(Count, Samplers);
	}

	static FORCEINLINE void DeleteSamplers(GLsizei Count, GLuint* Samplers)
	{
		glDeleteSamplers(Count, Samplers);
	}

	static FORCEINLINE void SetSamplerParameter(GLuint Sampler, GLenum Parameter, GLint Value)
	{
		glSamplerParameteri(Sampler, Parameter, Value);
	}

	static FORCEINLINE void BindSampler(GLuint Unit, GLuint Sampler)
	{
		glBindSampler(Unit, Sampler);
	}

	// Adreno doesn't support HALF_FLOAT
	static FORCEINLINE int32 GetReadHalfFloatPixelsEnum()				{ return GL_FLOAT; }

	static FORCEINLINE GLenum GetTextureHalfFloatPixelType()			
	{ 
		return bES30Support ? GL_HALF_FLOAT : GL_HALF_FLOAT_OES;
	}

	static FORCEINLINE GLenum GetTextureHalfFloatInternalFormat()		
	{ 
		return bES30Support ? GL_RGBA16F : GL_RGBA8;
	}

	// Android ES2 shaders have code that allows compile selection of
	// 32 bpp HDR encoding mode via 'intrinsic_GetHDR32bppEncodeModeES2()'.
	static FORCEINLINE bool SupportsHDR32bppEncodeModeIntrinsic()		{ return true; }

	static FORCEINLINE bool SupportsSRGB()								{ return IsES31Usable(); }		// only with enabled EFeatureLevelSupport::ES31
	static FORCEINLINE bool SupportsTextureSwizzle()					{ return bES30Support; }
	static FORCEINLINE bool SupportsInstancing()						{ return bSupportsInstancing; }
	static FORCEINLINE bool SupportsDrawBuffers()						{ return bES30Support; }
	static FORCEINLINE bool SupportsMultipleRenderTargets()				{ return bES30Support; }
	static FORCEINLINE bool SupportsWideMRT()							{ return bES31Support; }
	static FORCEINLINE bool SupportsResourceView()						{ return bSupportsTextureBuffer; }
	static FORCEINLINE bool SupportsTexture3D()							{ return bES30Support; }
	static FORCEINLINE bool SupportsMobileMultiView()					{ return bSupportsMobileMultiView; }
	static FORCEINLINE bool SupportsImageExternal()						{ return bSupportsImageExternal; }
	static FORCEINLINE bool SupportsSamplerObjects()					{ return IsES31Usable(); }
	static FORCEINLINE bool UseES30ShadingLanguage()
	{
		return bUseES30ShadingLanguage;
	}

	// Disable all queries except occlusion
	// Query is a limited resource on Android and we better spent them all on occlusion
	static FORCEINLINE bool SupportsTimestampQueries()					{ return false; }
	static FORCEINLINE bool SupportsDisjointTimeQueries()				{ return false; }
	
	static FORCEINLINE bool SupportsBlitFramebuffer() { return FOpenGLES2::SupportsBlitFramebuffer() || IsES31Usable(); }

	static FORCEINLINE bool SupportsComputeShaders() { return bES31Support && RHISupportsComputeShaders(GetShaderPlatform()); }

	enum class EImageExternalType : uint8
	{
		None,
		ImageExternal100,
		ImageExternal300,
		ImageExternalESSL300
	};

	static FORCEINLINE EImageExternalType GetImageExternalType() { return ImageExternalType; }

	static FORCEINLINE bool SupportsTextureMaxLevel()					{ return bES31Support; }
	static FORCEINLINE GLenum GetVertexHalfFloatFormat() { return bES31Support ? GL_HALF_FLOAT : GL_HALF_FLOAT_OES; }

	static FORCEINLINE GLenum GetDepthFormat() { return GL_DEPTH_COMPONENT24; }
	static FORCEINLINE GLenum GetShadowDepthFormat() { return GL_DEPTH_COMPONENT16; }

	static FORCEINLINE GLint GetMaxMSAASamplesTileMem() { return MaxMSAASamplesTileMem; }

	static void ProcessExtensions(const FString& ExtensionsString);

	// whether to use ES 3.0 function glTexStorage2D to allocate storage for GL_HALF_FLOAT_OES render target textures
	static bool bUseHalfFloatTexStorage;

	// GL_EXT_texture_buffer
	static bool bSupportsTextureBuffer;

	// whether to use ES 3.0 shading language
	static bool bUseES30ShadingLanguage;
	
	// whether device supports ES 3.0
	static bool bES30Support;

	// whether device supports ES 3.1
	static bool bES31Support;

	// whether device supports hardware instancing
	static bool bSupportsInstancing;

	/** Whether device supports Hidden Surface Removal */
	static bool bHasHardwareHiddenSurfaceRemoval;

	/** Whether device supports mobile multi-view */
	static bool bSupportsMobileMultiView;

	/** Whether device supports image external */
	static bool bSupportsImageExternal;

	/** Type of image external supported */
	static EImageExternalType ImageExternalType;

	/** Maximum number of MSAA samples supported on chip in tile memory, or 1 if not available */
	static GLint MaxMSAASamplesTileMem;

	enum class EFeatureLevelSupport : uint8
	{
		Invalid,	// no feature level has yet been determined
		ES2,
		ES31,
		ES32
	};

	/** Describes which feature level is currently being supported */
	static EFeatureLevelSupport CurrentFeatureLevelSupport;

	/** supported OpenGL ES version queried from the system */
	static int32 GLMajorVerion;
	static int32 GLMinorVersion;
};

typedef FAndroidOpenGL FOpenGL;


/** Unreal tokens that maps to different OpenGL tokens by platform. */
#undef UGL_DRAW_FRAMEBUFFER
#define UGL_DRAW_FRAMEBUFFER	GL_DRAW_FRAMEBUFFER_NV
#undef UGL_READ_FRAMEBUFFER
#define UGL_READ_FRAMEBUFFER	GL_READ_FRAMEBUFFER_NV

#endif // PLATFORM_ANDROID
