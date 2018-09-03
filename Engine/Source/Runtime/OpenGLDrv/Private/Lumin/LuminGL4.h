// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
OpenGLWindows.h: Manual loading of OpenGL functions from DLL.
=============================================================================*/

#pragma once

#if PLATFORM_LUMINGL4

#define ENABLE_DRAW_MARKERS		(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "RHI.h"

THIRD_PARTY_INCLUDES_START
#include <GL/glcorearb.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
THIRD_PARTY_INCLUDES_END

//EGL Extension #39 for Context creation
#ifndef EGL_KHR_create_context
#define EGL_KHR_create_context 1
#define EGL_CONTEXT_MAJOR_VERSION_KHR                       0x3098
#define EGL_CONTEXT_MINOR_VERSION_KHR                       0x30FB
#define EGL_CONTEXT_FLAGS_KHR                               0x30FC
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR                 0x30FD
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR  0x31BD
#define EGL_NO_RESET_NOTIFICATION_KHR                       0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET_KHR                       0x31BF
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR                    0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR       0x00000002
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR            0x00000004
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR             0x00000001
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR    0x00000002
#define EGL_OPENGL_ES3_BIT_KHR                              0x00000040
#endif

//https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_depth_bounds_test.txt
#ifndef EXT_depth_bounds_test
#define EXT_depth_bounds_test 1
typedef void(APIENTRYP PFNGLDEPTHBOUNDSEXTPROC)(GLclampd zmin, GLclampd zmax);
#define GL_DEPTH_BOUNDS_TEST_EXT                       0x8890
#define GL_DEPTH_BOUNDS_EXT                            0x8891
#endif

//https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_filter_anisotropic.txt
#ifndef EXT_texture_filter_anisotropic
#define EXT_texture_filter_anisotropic 1
#define GL_TEXTURE_MAX_ANISOTROPY_EXT          0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT      0x84FF
#endif

//https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_format_BGRA8888.txt
#ifndef EXT_texture_format_BGRA8888
#define EXT_texture_format_BGRA8888 1
#define GL_BGRA_EXT                            0x80E1
#endif 

//https://www.khronos.org/registry/OpenGL/extensions/NV/NV_bindless_texture.txt
#ifndef GL_NV_bindless_texture
#define GL_NV_bindless_texture 1
typedef uint64_t GLuint64EXT;
#define GL_UNSIGNED_INT64_NV             0x140F
typedef GLuint64(APIENTRYP PFNGLGETTEXTUREHANDLENVPROC) (GLuint texture);
typedef GLuint64(APIENTRYP PFNGLGETTEXTURESAMPLERHANDLENVPROC) (GLuint texture, GLuint sampler);
typedef void (APIENTRYP PFNGLMAKETEXTUREHANDLERESIDENTNVPROC) (GLuint64 handle);
typedef void (APIENTRYP PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC) (GLuint64 handle);
typedef GLuint64(APIENTRYP PFNGLGETIMAGEHANDLENVPROC) (GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format);
typedef void (APIENTRYP PFNGLMAKEIMAGEHANDLERESIDENTNVPROC) (GLuint64 handle, GLenum access);
typedef void (APIENTRYP PFNGLMAKEIMAGEHANDLENONRESIDENTNVPROC) (GLuint64 handle);
typedef void (APIENTRYP PFNGLUNIFORMHANDLEUI64NVPROC) (GLint location, GLuint64 value);
typedef void (APIENTRYP PFNGLUNIFORMHANDLEUI64VNVPROC) (GLint location, GLsizei count, const GLuint64 *value);
typedef void (APIENTRYP PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC) (GLuint program, GLint location, GLuint64 value);
typedef void (APIENTRYP PFNGLPROGRAMUNIFORMHANDLEUI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64 *values);
typedef GLboolean(APIENTRYP PFNGLISTEXTUREHANDLERESIDENTNVPROC) (GLuint64 handle);
typedef GLboolean(APIENTRYP PFNGLISIMAGEHANDLERESIDENTNVPROC) (GLuint64 handle);
typedef void (APIENTRYP PFNGLVERTEXATTRIBL1UI64NVPROC) (GLuint index, GLuint64EXT x);
typedef void (APIENTRYP PFNGLVERTEXATTRIBL1UI64VNVPROC) (GLuint index, const GLuint64EXT *v);
typedef void (APIENTRYP PFNGLGETVERTEXATTRIBLUI64VNVPROC) (GLuint index, GLenum pname, GLuint64EXT *params);
#endif /* GL_NV_bindless_texture */

/** List all OpenGL entry points used by Unreal. */
#define ENUM_GL_ENTRYPOINTS_1_0(EnumMacro)				  \
	EnumMacro(PFNGLLINEWIDTHPROC, glLineWidth)            \
	EnumMacro(PFNGLSCISSORPROC, glScissor)                \
	EnumMacro(PFNGLTEXPARAMETERFPROC, glTexParameterf)    \
	EnumMacro(PFNGLTEXPARAMETERIPROC, glTexParameteri)    \
	EnumMacro(PFNGLTEXPARAMETERFVPROC, glTexParameterfv)  \
	EnumMacro(PFNGLTEXIMAGE2DPROC, glTexImage2D)          \
	EnumMacro(PFNGLCLEARPROC, glClear)                    \
	EnumMacro(PFNGLCLEARCOLORPROC, glClearColor)          \
	EnumMacro(PFNGLCOLORMASKPROC, glColorMask)            \
	EnumMacro(PFNGLDEPTHMASKPROC, glDepthMask)            \
	EnumMacro(PFNGLDISABLEPROC, glDisable)                \
	EnumMacro(PFNGLENABLEPROC, glEnable)                  \
	EnumMacro(PFNGLFINISHPROC, glFinish)                  \
	EnumMacro(PFNGLFLUSHPROC, glFlush)                    \
	EnumMacro(PFNGLBLENDFUNCPROC, glBlendFunc)            \
	EnumMacro(PFNGLDEPTHFUNCPROC, glDepthFunc)            \
	EnumMacro(PFNGLGETSTRINGPROC, glGetString)            \
	EnumMacro(PFNGLPIXELSTOREIPROC, glPixelStorei)        \
	EnumMacro(PFNGLGETERRORPROC, glGetError)              \
	EnumMacro(PFNGLGETFLOATVPROC, glGetFloatv)            \
	EnumMacro(PFNGLGETINTEGERVPROC, glGetIntegerv)        \
	EnumMacro(PFNGLVIEWPORTPROC, glViewport)              \
	EnumMacro(PFNGLISENABLEDPROC, glIsEnabled)            \
	EnumMacro(PFNGLFRONTFACEPROC, glFrontFace)            \
	EnumMacro(PFNGLHINTPROC, glHint)                      \
	EnumMacro(PFNGLCULLFACEPROC, glCullFace)              \
	EnumMacro(PFNGLREADPIXELSPROC, glReadPixels)          \
	EnumMacro(PFNGLREADBUFFERPROC, glReadBuffer)          \
	EnumMacro(PFNGLPOINTSIZEPROC, glPointSize)            \
	EnumMacro(PFNGLPOLYGONMODEPROC, glPolygonMode)        \
	EnumMacro(PFNGLCLEARDEPTHPROC, glClearDepth)          \
	EnumMacro(PFNGLDEPTHRANGEPROC, glDepthRange)          \
	EnumMacro(PFNGLISPROGRAMPROC, glIsProgram)

#define ENUM_GL_ENTRYPOINTS_1_1(EnumMacro)					   \
	EnumMacro(PFNGLDRAWARRAYSPROC, glDrawArrays)               \
	EnumMacro(PFNGLDRAWELEMENTSPROC, glDrawElements)           \
	EnumMacro(PFNGLTEXSUBIMAGE2DPROC, glTexSubImage2D)         \
	EnumMacro(PFNGLDELETETEXTURESPROC, glDeleteTextures)       \
	EnumMacro(PFNGLGENTEXTURESPROC, glGenTextures)             \
	EnumMacro(PFNGLPOLYGONOFFSETPROC,glPolygonOffset) \
	EnumMacro(PFNGLCOPYTEXSUBIMAGE2DPROC, glCopyTexSubImage2D)

#define ENUM_GL_ENTRYPOINTS_1_3(EnumMacro)					   \
	EnumMacro(PFNGLACTIVETEXTUREPROC, glActiveTexture)

#define ENUM_GL_ENTRYPOINTS_1_4(EnumMacro)					   \
	EnumMacro(PFNGLBLENDEQUATIONPROC, glBlendEquation)

#define ENUM_GL_ENTRYPOINTS_1_5(EnumMacro)                     \
	EnumMacro(PFNGLBINDBUFFERPROC, glBindBuffer)               \
	EnumMacro(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)         \
	EnumMacro(PFNGLGENBUFFERSPROC, glGenBuffers)               \
	EnumMacro(PFNGLBUFFERDATAPROC, glBufferData)               \
	EnumMacro(PFNGLBUFFERSUBDATAPROC, glBufferSubData)         \
	EnumMacro(PFNGLUNMAPBUFFERPROC, glUnmapBuffer)             \
	EnumMacro(PFNGLGENQUERIESPROC, glGenQueries)               \
	EnumMacro(PFNGLDELETEQUERIESPROC, glDeleteQueries)         \
	EnumMacro(PFNGLISQUERYPROC, glIsQuery)                     \
	EnumMacro(PFNGLBEGINQUERYPROC, glBeginQuery)               \
	EnumMacro(PFNGLENDQUERYPROC, glEndQuery)                   \
	EnumMacro(PFNGLGETQUERYIVPROC, glGetQueryiv)               \
	EnumMacro(PFNGLGETQUERYOBJECTUIVPROC, glGetQueryObjectuiv) \
	EnumMacro(PFNGLMAPBUFFERPROC, glMapBuffer)                 \
	EnumMacro(PFNGLGETQUERYOBJECTIVPROC, glGetQueryObjectiv)

#define ENUM_GL_ENTRYPOINTS_2_0(EnumMacro)                       \
	EnumMacro(PFNGLGETSHADERSOURCEPROC,glGetShaderSource) \
	EnumMacro(PFNGLBINDTEXTUREPROC, glBindTexture)             \
	EnumMacro(PFNGLSTENCILFUNCPROC,glStencilFunc) \
	EnumMacro(PFNGLSTENCILMASKPROC,glStencilMask) \
	EnumMacro(PFNGLSTENCILOPPROC,glStencilOp) \
	EnumMacro(PFNGLCLEARSTENCILPROC,glClearStencil) \
	EnumMacro(PFNGLBINDATTRIBLOCATIONPROC,glBindAttribLocation) \
	EnumMacro(PFNGLBLENDFUNCSEPARATEPROC,glBlendFuncSeparate) \
	EnumMacro(PFNGLDRAWBUFFERSPROC,glDrawBuffers) \
	EnumMacro(PFNGLSTENCILOPSEPARATEPROC,glStencilOpSeparate) \
	EnumMacro(PFNGLSTENCILFUNCSEPARATEPROC,glStencilFuncSeparate) \
	EnumMacro(PFNGLSTENCILMASKSEPARATEPROC,glStencilMaskSeparate) \
	EnumMacro(PFNGLCOMPRESSEDTEXIMAGE3DPROC,glCompressedTexImage3D) \
	EnumMacro(PFNGLCOMPRESSEDTEXIMAGE2DPROC,glCompressedTexImage2D) \
	EnumMacro(PFNGLCOMPRESSEDTEXIMAGE1DPROC,glCompressedTexImage1D) \
	EnumMacro(PFNGLGETBOOLEANVPROC,glGetBooleanv) \
	EnumMacro(PFNGLGETDOUBLEVPROC,glGetDoublev) \
	EnumMacro(PFNGLGETATTACHEDSHADERSPROC,glGetAttachedShaders) \
	EnumMacro(PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC,glCompressedTexSubImage3D) \
	EnumMacro(PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC,glCompressedTexSubImage2D) \
	EnumMacro(PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC,glCompressedTexSubImage1D) \
	EnumMacro(PFNGLCOPYTEXSUBIMAGE3DPROC,glCopyTexSubImage3D) \
	EnumMacro(PFNGLDRAWRANGEELEMENTSPROC,glDrawRangeElements) \
	EnumMacro(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate)       \
	EnumMacro(PFNGLATTACHSHADERPROC, glAttachShader)                         \
	EnumMacro(PFNGLCOMPILESHADERPROC, glCompileShader)                       \
	EnumMacro(PFNGLCREATEPROGRAMPROC, glCreateProgram)                       \
	EnumMacro(PFNGLCREATESHADERPROC, glCreateShader)                         \
	EnumMacro(PFNGLDELETEPROGRAMPROC, glDeleteProgram)                       \
	EnumMacro(PFNGLDELETESHADERPROC, glDeleteShader)                         \
	EnumMacro(PFNGLDETACHSHADERPROC, glDetachShader)                         \
	EnumMacro(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) \
	EnumMacro(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)   \
	EnumMacro(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                         \
	EnumMacro(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)               \
	EnumMacro(PFNGLGETSHADERIVPROC, glGetShaderiv)                           \
	EnumMacro(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)                 \
	EnumMacro(PFNGLGETATTRIBLOCATIONPROC, glGetAttribLocation)               \
	EnumMacro(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)             \
	EnumMacro(PFNGLLINKPROGRAMPROC, glLinkProgram)                           \
	EnumMacro(PFNGLSHADERSOURCEPROC, glShaderSource)                         \
	EnumMacro(PFNGLUSEPROGRAMPROC, glUseProgram)                             \
	EnumMacro(PFNGLUNIFORM1FPROC, glUniform1f)                               \
	EnumMacro(PFNGLUNIFORM2FPROC, glUniform2f)                               \
	EnumMacro(PFNGLUNIFORM3FPROC, glUniform3f)                               \
	EnumMacro(PFNGLUNIFORM4FPROC, glUniform4f)                               \
	EnumMacro(PFNGLUNIFORM1IPROC, glUniform1i)                               \
	EnumMacro(PFNGLUNIFORM2IPROC, glUniform2i)                               \
	EnumMacro(PFNGLUNIFORM3IPROC, glUniform3i)                               \
	EnumMacro(PFNGLUNIFORM4IPROC, glUniform4i)                               \
	EnumMacro(PFNGLUNIFORM1FVPROC, glUniform1fv)                             \
	EnumMacro(PFNGLUNIFORM2FVPROC, glUniform2fv)                             \
	EnumMacro(PFNGLUNIFORM3FVPROC, glUniform3fv)                             \
	EnumMacro(PFNGLUNIFORM4FVPROC, glUniform4fv)                             \
	EnumMacro(PFNGLUNIFORM4IVPROC, glUniform4iv)							 \
	EnumMacro(PFNGLUNIFORMMATRIX3FVPROC, glUniformMatrix3fv)                 \
	EnumMacro(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)                 \
	EnumMacro(PFNGLDRAWBUFFERPROC,glDrawBuffer)								 \
	EnumMacro(PFNGLVERTEXATTRIB1DPROC,glVertexAttrib1d)						 \
	EnumMacro(PFNGLVERTEXATTRIB1DVPROC,glVertexAttrib1dv)					 \
	EnumMacro(PFNGLVERTEXATTRIB1FPROC,glVertexAttrib1f)						 \
	EnumMacro(PFNGLVERTEXATTRIB1FVPROC,glVertexAttrib1fv)					 \
	EnumMacro(PFNGLVERTEXATTRIB1SPROC,glVertexAttrib1s)						 \
	EnumMacro(PFNGLVERTEXATTRIB1SVPROC,glVertexAttrib1sv)					 \
	EnumMacro(PFNGLVERTEXATTRIB2DPROC,glVertexAttrib2d)						 \
	EnumMacro(PFNGLVERTEXATTRIB2DVPROC,glVertexAttrib2dv)					 \
	EnumMacro(PFNGLVERTEXATTRIB2FPROC,glVertexAttrib2f)						 \
	EnumMacro(PFNGLVERTEXATTRIB2FVPROC,glVertexAttrib2fv)					 \
	EnumMacro(PFNGLVERTEXATTRIB2SPROC,glVertexAttrib2s)						 \
	EnumMacro(PFNGLVERTEXATTRIB2SVPROC,glVertexAttrib2sv)					 \
	EnumMacro(PFNGLVERTEXATTRIB3DPROC,glVertexAttrib3d)						 \
	EnumMacro(PFNGLVERTEXATTRIB3DVPROC,glVertexAttrib3dv)					 \
	EnumMacro(PFNGLVERTEXATTRIB3FPROC,glVertexAttrib3f)						 \
	EnumMacro(PFNGLVERTEXATTRIB3FVPROC,glVertexAttrib3fv)					 \
	EnumMacro(PFNGLVERTEXATTRIB3SPROC,glVertexAttrib3s)						 \
	EnumMacro(PFNGLVERTEXATTRIB3SVPROC,glVertexAttrib3sv)					 \
	EnumMacro(PFNGLVERTEXATTRIB4NBVPROC,glVertexAttrib4Nbv)					 \
	EnumMacro(PFNGLVERTEXATTRIB4NIVPROC,glVertexAttrib4Niv)					 \
	EnumMacro(PFNGLVERTEXATTRIB4NSVPROC,glVertexAttrib4Nsv)					 \
	EnumMacro(PFNGLVERTEXATTRIB4NUBPROC,glVertexAttrib4Nub)					 \
	EnumMacro(PFNGLVERTEXATTRIB4NUBVPROC,glVertexAttrib4Nubv)				 \
	EnumMacro(PFNGLVERTEXATTRIB4NUIVPROC,glVertexAttrib4Nuiv)				 \
	EnumMacro(PFNGLVERTEXATTRIB4NUSVPROC,glVertexAttrib4Nusv)				 \
	EnumMacro(PFNGLVERTEXATTRIB4BVPROC,glVertexAttrib4bv)					 \
	EnumMacro(PFNGLVERTEXATTRIB4DPROC,glVertexAttrib4d)						 \
	EnumMacro(PFNGLVERTEXATTRIB4DVPROC,glVertexAttrib4dv)					 \
	EnumMacro(PFNGLVERTEXATTRIB4FPROC,glVertexAttrib4f)						 \
	EnumMacro(PFNGLVERTEXATTRIB4FVPROC,glVertexAttrib4fv)					 \
	EnumMacro(PFNGLVERTEXATTRIB4IVPROC,glVertexAttrib4iv)					 \
	EnumMacro(PFNGLVERTEXATTRIB4SPROC,glVertexAttrib4s)						 \
	EnumMacro(PFNGLVERTEXATTRIB4SVPROC,glVertexAttrib4sv)					 \
	EnumMacro(PFNGLVERTEXATTRIB4UBVPROC,glVertexAttrib4ubv)					 \
	EnumMacro(PFNGLVERTEXATTRIB4UIVPROC,glVertexAttrib4uiv)					 \
	EnumMacro(PFNGLVERTEXATTRIB4USVPROC,glVertexAttrib4usv)					 \
	EnumMacro(PFNGLTEXSUBIMAGE3DPROC,glTexSubImage3D) \
	EnumMacro(PFNGLTEXIMAGE3DPROC,glTexImage3D) \
	EnumMacro(PFNGLGETCOMPRESSEDTEXIMAGEPROC,glGetCompressedTexImage) \
	EnumMacro(PFNGLGETTEXPARAMETERIVPROC,glGetTexParameteriv) \
	EnumMacro(PFNGLGETVERTEXATTRIBIVPROC,glGetVertexAttribiv) \
	EnumMacro(PFNGLGETVERTEXATTRIBPOINTERVPROC,glGetVertexAttribPointerv) \
	EnumMacro(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)

#define ENUM_GL_ENTRYPOINTS_2_1(EnumMacro)                       \
	EnumMacro(PFNGLUNIFORMMATRIX3X2FVPROC, glUniformMatrix3x2fv)

#define ENUM_GL_ENTRYPOINTS_3_0(EnumMacro)							   \
	EnumMacro(PFNGLRENDERBUFFERSTORAGEPROC,glRenderbufferStorage) \
	EnumMacro(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC,glRenderbufferStorageMultisample) \
	EnumMacro(PFNGLBINDFRAGDATALOCATIONPROC,glBindFragDataLocation) \
	EnumMacro(PFNGLCLEARBUFFERFVPROC,glClearBufferfv) \
	EnumMacro(PFNGLCLEARBUFFERIVPROC,glClearBufferiv) \
	EnumMacro(PFNGLCLEARBUFFERUIVPROC,glClearBufferuiv) \
	EnumMacro(PFNGLCLEARBUFFERFIPROC,glClearBufferfi) \
	EnumMacro(PFNGLCOLORMASKIPROC,glColorMaski) \
	EnumMacro(PFNGLBINDBUFFERBASEPROC, glBindBufferBase)                 \
	EnumMacro(PFNGLBINDBUFFERRANGEPROC, glBindBufferRange)			   \
	EnumMacro(PFNGLGETSTRINGIPROC, glGetStringi)                         \
	EnumMacro(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)               \
	EnumMacro(PFNGLCLAMPCOLORPROC, glClampColor)                         \
	EnumMacro(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)         \
	EnumMacro(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)               \
	EnumMacro(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
	EnumMacro(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)     \
	EnumMacro(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap)                 \
	EnumMacro(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange)                 \
	EnumMacro(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)               \
	EnumMacro(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)         \
	EnumMacro(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)               \
	EnumMacro(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer)               \
	EnumMacro(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers)             \
	EnumMacro(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers)       \
	EnumMacro(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer)             \
	EnumMacro(PFNGLVERTEXATTRIBIPOINTERPROC,glVertexAttribIPointer)	   \
	EnumMacro(PFNGLUNIFORM4UIVPROC, glUniform4uiv)					   \
	EnumMacro(PFNGLFRAMEBUFFERTEXTURELAYERPROC,glFramebufferTextureLayer) \
	EnumMacro(PFNGLFRAMEBUFFERTEXTURE3DPROC,glFramebufferTexture3D) \
	EnumMacro(PFNGLDISABLEIPROC,glDisablei) \
	EnumMacro(PFNGLENABLEIPROC,glEnablei) \
	EnumMacro(PFNGLVERTEXATTRIBI4IVPROC,glVertexAttribI4iv) \
	EnumMacro(PFNGLVERTEXATTRIBI4UIVPROC,glVertexAttribI4uiv) \
	EnumMacro(PFNGLVERTEXATTRIBI4SVPROC,glVertexAttribI4sv) \
	EnumMacro(PFNGLVERTEXATTRIBI4USVPROC,glVertexAttribI4usv) \
	EnumMacro(PFNGLVERTEXATTRIBI4BVPROC,glVertexAttribI4bv) \
	EnumMacro(PFNGLVERTEXATTRIBI4UBVPROC,glVertexAttribI4ubv) \
	EnumMacro(PFNGLGETTEXIMAGEPROC,glGetTexImage) \
	EnumMacro(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer)

#define ENUM_GL_ENTRYPOINTS_3_1(EnumMacro)							 \
	EnumMacro(PFNGLDRAWELEMENTSINSTANCEDPROC, glDrawElementsInstanced) \
	EnumMacro(PFNGLUNIFORMBLOCKBINDINGPROC,glUniformBlockBinding)		 \
	EnumMacro(PFNGLGETUNIFORMBLOCKINDEXPROC,glGetUniformBlockIndex)	 \
	EnumMacro(PFNGLTEXBUFFERPROC,glTexBuffer) \
	EnumMacro(PFNGLDRAWARRAYSINSTANCEDPROC,glDrawArraysInstanced) \
	EnumMacro(PFNGLCOPYBUFFERSUBDATAPROC, glCopyBufferSubData)

#define ENUM_GL_ENTRYPOINTS_3_2(EnumMacro)                       \
	EnumMacro(PFNGLISSYNCPROC, glIsSync)						 \
	EnumMacro(PFNGLFENCESYNCPROC, glFenceSync)					 \
	EnumMacro(PFNGLDELETESYNCPROC, glDeleteSync)				 \
	EnumMacro(PFNGLGETSYNCIVPROC, glGetSynciv)					 \
	EnumMacro(PFNGLCLIENTWAITSYNCPROC, glClientWaitSync)		 \
	EnumMacro(PFNGLBINDSAMPLERPROC,glBindSampler)				 \
	EnumMacro(PFNGLSAMPLERPARAMETERIPROC,glSamplerParameteri)	 \
	EnumMacro(PFNGLGENSAMPLERSPROC,glGenSamplers)				 \
	EnumMacro(PFNGLFRAMEBUFFERTEXTUREPROC,glFramebufferTexture)  \
	EnumMacro(PFNGLDELETESAMPLERSPROC,glDeleteSamplers)			 \
	EnumMacro(PFNGLTEXIMAGE2DMULTISAMPLEPROC,glTexImage2DMultisample) \
	EnumMacro(PFNGLGETINTEGER64VPROC, glGetInteger64v)

#define ENUM_GL_ENTRYPOINTS_3_3(EnumMacro)                         \
	EnumMacro(PFNGLVERTEXATTRIBDIVISORPROC, glVertexAttribDivisor) \
	EnumMacro(PFNGLQUERYCOUNTERPROC, glQueryCounter)               \
	EnumMacro(PFNGLGETQUERYOBJECTI64VPROC, glGetQueryObjecti64v)   \
	EnumMacro(PFNGLGETQUERYOBJECTUI64VPROC, glGetQueryObjectui64v)

#define ENUM_GL_ENTRYPOINTS_4_0(EnumMacro)                       \
	EnumMacro(PFNGLDRAWARRAYSINDIRECTPROC, glDrawArraysIndirect)\
	EnumMacro(PFNGLBLENDEQUATIONIPROC, glBlendEquationi) \
	EnumMacro(PFNGLDRAWELEMENTSINDIRECTPROC, glDrawElementsIndirect)\
	EnumMacro(PFNGLBLENDFUNCIPROC, glBlendFunci) \
	EnumMacro(PFNGLPATCHPARAMETERIPROC, glPatchParameteri)\
	EnumMacro(PFNGLBLENDEQUATIONSEPARATEIPROC, glBlendEquationSeparatei) \
	EnumMacro(PFNGLBLENDFUNCSEPARATEIPROC, glBlendFuncSeparatei)

#define ENUM_GL_ENTRYPOINTS_4_1(EnumMacro)                       \
	EnumMacro(PFNGLPROGRAMPARAMETERIPROC, glProgramParameteri)\
	EnumMacro(PFNGLBINDPROGRAMPIPELINEPROC, glBindProgramPipeline)\
	EnumMacro(PFNGLDELETEPROGRAMPIPELINESPROC, glDeleteProgramPipelines)\
	EnumMacro(PFNGLGENPROGRAMPIPELINESPROC, glGenProgramPipelines)\
	EnumMacro(PFNGLVALIDATEPROGRAMPIPELINEPROC, glValidateProgramPipeline)\
	EnumMacro(PFNGLUSEPROGRAMSTAGESPROC, glUseProgramStages)\
	EnumMacro(PFNGLPROGRAMUNIFORM1IPROC, glProgramUniform1i)\
	EnumMacro(PFNGLPROGRAMUNIFORM4IVPROC, glProgramUniform4iv)\
	EnumMacro(PFNGLPROGRAMUNIFORM4FVPROC, glProgramUniform4fv)\
	EnumMacro(PFNGLPROGRAMUNIFORM4UIVPROC, glProgramUniform4uiv)\
	EnumMacro(PFNGLGETPROGRAMPIPELINEIVPROC, glGetProgramPipelineiv)\
	EnumMacro(PFNGLGETPROGRAMPIPELINEINFOLOGPROC, glGetProgramPipelineInfoLog)\
	EnumMacro(PFNGLISPROGRAMPIPELINEPROC, glIsProgramPipeline)

#define ENUM_GL_ENTRYPOINTS_4_2(EnumMacro)                       \
	EnumMacro(PFNGLTEXSTORAGE1DPROC, glTexStorage1D)			 \
	EnumMacro(PFNGLTEXSTORAGE2DPROC, glTexStorage2D)			 \
	EnumMacro(PFNGLTEXSTORAGE3DPROC, glTexStorage3D)             \
	EnumMacro(PFNGLBINDIMAGETEXTUREPROC, glBindImageTexture)	 \
	EnumMacro(PFNGLMEMORYBARRIERPROC, glMemoryBarrier)

#define ENUM_GL_ENTRYPOINTS_4_3(EnumMacro)                           \
	EnumMacro(PFNGLBINDVERTEXBUFFERPROC, glBindVertexBuffer)\
	EnumMacro(PFNGLCLEARBUFFERDATAPROC, glClearBufferData)\
	EnumMacro(PFNGLDISPATCHCOMPUTEINDIRECTPROC, glDispatchComputeIndirect) \
	EnumMacro(PFNGLOBJECTLABELPROC, glObjectLabel)\
	EnumMacro(PFNGLOBJECTPTRLABELPROC, glObjectPtrLabel)\
	EnumMacro(PFNGLPUSHDEBUGGROUPPROC, glPushDebugGroup)\
	EnumMacro(PFNGLPOPDEBUGGROUPPROC, glPopDebugGroup)\
	EnumMacro(PFNGLVERTEXBINDINGDIVISORPROC, glVertexBindingDivisor)\
	EnumMacro(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback) \
	EnumMacro(PFNGLDEBUGMESSAGECONTROLPROC, glDebugMessageControl)   \
	EnumMacro(PFNGLDISPATCHCOMPUTEPROC, glDispatchCompute)			 \
	EnumMacro(PFNGLTEXTUREVIEWPROC, glTextureView)\
	EnumMacro(PFNGLCOPYIMAGESUBDATAPROC, glCopyImageSubData)\
	EnumMacro(PFNGLTEXSTORAGE2DMULTISAMPLEPROC, glTexStorage2DMultisample)

#define ENUM_GL_ENTRYPOINTS_4_4(EnumMacro)                       \
	EnumMacro(PFNGLBUFFERSTORAGEPROC, glBufferStorage)

#define ENUM_GL_ENTRYPOINTS_4_5(EnumMacro)                       \
	EnumMacro(PFNGLVERTEXATTRIBBINDINGPROC, glVertexAttribBinding)\
	EnumMacro(PFNGLVERTEXATTRIBFORMATPROC, glVertexAttribFormat)\
	EnumMacro(PFNGLCLIPCONTROLPROC,glClipControl) \
	EnumMacro(PFNGLVERTEXATTRIBIFORMATPROC, glVertexAttribIFormat)

#define ENUM_GL_ENTRYPOINTS_OPTIONAL(EnumMacro) \
	EnumMacro(PFNGLDEBUGMESSAGECALLBACKARBPROC,glDebugMessageCallbackARB) \
	EnumMacro(PFNGLDEPTHBOUNDSEXTPROC, glDepthBoundsEXT)\
	EnumMacro(PFNGLGETTEXTUREHANDLENVPROC, glGetTextureHandleNV)\
	EnumMacro(PFNGLGETTEXTURESAMPLERHANDLENVPROC, glGetTextureSamplerHandleNV)\
	EnumMacro(PFNGLMAKETEXTUREHANDLERESIDENTNVPROC, glMakeTextureHandleResidentNV)\
	EnumMacro(PFNGLUNIFORMHANDLEUI64NVPROC, glUniformHandleui64NV)\
	EnumMacro(PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC, glMakeTextureHandleNonResidentNV)\
	EnumMacro(PFNGLDEBUGMESSAGECONTROLARBPROC,glDebugMessageControlARB)

/** List of all OpenGL entry points. */
#define ENUM_GL_ENTRYPOINTS_ALL(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_1_0(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_1_1(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_1_3(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_1_4(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_1_5(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_2_0(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_2_1(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_3_0(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_3_1(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_3_2(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_3_3(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_4_0(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_4_1(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_4_2(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_4_3(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_4_4(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_4_5(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_OPTIONAL(EnumMacro)

/** Declare all GL functions. */
#define DECLARE_GL_ENTRYPOINTS(Type,Func) extern Type Func;

// We need to make pointer names different from GL functions otherwise we may end up getting
// addresses of those symbols when looking for extensions.
namespace GLFuncPointers
{
	ENUM_GL_ENTRYPOINTS_ALL(DECLARE_GL_ENTRYPOINTS);
};

// this using is needed since the rest of code uses plain GL names
using namespace GLFuncPointers;

//========================================================================

#include "OpenGL4.h"

struct FLuminOpenGL4 : public FOpenGL4
{
	static FORCEINLINE void InitDebugContext()
	{
#if ENABLE_DRAW_MARKERS
		bDebugContext = true;
#else
		bDebugContext = glIsEnabled(GL_DEBUG_OUTPUT) != GL_FALSE;
#endif
	}

	static FORCEINLINE void LabelObject(GLenum Type, GLuint Object, const ANSICHAR* Name)
	{
		if (glObjectLabel && bDebugContext)
		{
			glObjectLabel(Type, Object, FCStringAnsi::Strlen(Name), Name);
		}
	}

	static FORCEINLINE void PushGroupMarker(const ANSICHAR* Name)
	{
		if (glPushDebugGroup && bDebugContext)
		{
			glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 1, FCStringAnsi::Strlen(Name), Name);
		}
	}

	static FORCEINLINE void PopGroupMarker()
	{
		if (glPopDebugGroup && bDebugContext)
		{
			glPopDebugGroup();
		}
	}

	static FORCEINLINE bool TexStorage2D(GLenum Target, GLint Levels, GLint InternalFormat, GLsizei Width, GLsizei Height, GLenum Format, GLenum Type, uint32 Flags)
	{
		if (glTexStorage2D != NULL)
		{
			glTexStorage2D(Target, Levels, InternalFormat, Width, Height);
			return true;
		}
		else
		{
			return false;
		}
	}

	static FORCEINLINE bool TexStorage2DMultisample(GLenum Target, GLsizei Samples, GLint InternalFormat, GLsizei Width, GLsizei Height, GLboolean FixedSampleLocations)
	{
		if (glTexStorage2DMultisample != NULL)
		{
			glTexStorage2DMultisample(Target, Samples, InternalFormat, Width, Height, FixedSampleLocations);
			return true;
		}
		else
		{
			return false;
		}
	}

	static FORCEINLINE void TexStorage3D(GLenum Target, GLint Levels, GLint InternalFormat, GLsizei Width, GLsizei Height, GLsizei Depth, GLenum Format, GLenum Type)
	{
		if (glTexStorage3D)
		{
			glTexStorage3D(Target, Levels, InternalFormat, Width, Height, Depth);
		}
		else
		{
			const bool bArrayTexture = Target == GL_TEXTURE_2D_ARRAY || Target == GL_TEXTURE_CUBE_MAP_ARRAY;

			for (uint32 MipIndex = 0; MipIndex < uint32(Levels); MipIndex++)
			{
				glTexImage3D(
					Target,
					MipIndex,
					InternalFormat,
					FMath::Max<uint32>(1, (Width >> MipIndex)),
					FMath::Max<uint32>(1, (Height >> MipIndex)),
					(bArrayTexture) ? Depth : FMath::Max<uint32>(1, (Depth >> MipIndex)),
					0,
					Format,
					Type,
					NULL
				);
			}
		}
	}

	static FORCEINLINE void CopyImageSubData(GLuint SrcName, GLenum SrcTarget, GLint SrcLevel, GLint SrcX, GLint SrcY, GLint SrcZ, GLuint DstName, GLenum DstTarget, GLint DstLevel, GLint DstX, GLint DstY, GLint DstZ, GLsizei Width, GLsizei Height, GLsizei Depth)
	{
		glCopyImageSubData(SrcName, SrcTarget, SrcLevel, SrcX, SrcY, SrcZ, DstName, DstTarget, DstLevel, DstX, DstY, DstZ, Width, Height, Depth);
	}

	static FORCEINLINE bool SupportsBindlessTexture()
	{
		return bSupportsBindlessTexture;
	}

	static FORCEINLINE GLuint64 GetTextureSamplerHandle(GLuint Texture, GLuint Sampler)
	{
		return glGetTextureSamplerHandleNV(Texture, Sampler);
	}

	static FORCEINLINE GLuint64 GetTextureHandle(GLuint Texture)
	{
		return glGetTextureHandleNV(Texture);
	}

	static FORCEINLINE void MakeTextureHandleResident(GLuint64 TextureHandle)
	{
		glMakeTextureHandleResidentNV(TextureHandle);
	}

	static FORCEINLINE void MakeTextureHandleNonResident(GLuint64 TextureHandle)
	{
		glMakeTextureHandleNonResidentNV(TextureHandle);
	}

	static FORCEINLINE void UniformHandleui64(GLint Location, GLuint64 Value)
	{
		glUniformHandleui64NV(Location, Value);
	}

	static void ProcessExtensions(const FString& ExtensionsString);

	// whether NV_bindless_texture is supported
	static bool bSupportsBindlessTexture;

	static FORCEINLINE bool SupportsResourceView() { return true; }

	static FORCEINLINE void TexBuffer(GLenum Target, GLenum InternalFormat, GLuint Buffer)
	{
		glTexBuffer(Target, InternalFormat, Buffer);
	}
};

typedef FLuminOpenGL4 FOpenGL;

#endif
