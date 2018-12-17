// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once


#if PLATFORM_WINDOWS

#include "D3D11RHIPrivate.h"
#include "D3D11Util.h"

//-------------------------------------------------------------------------------------------------
// D3D12
//-------------------------------------------------------------------------------------------------

#define GetD3D11CubeFace GetD3D12CubeFace
#define VerifyD3D11Result VerifyD3D12Result
#define GetD3D11TextureFromRHITexture GetD3D12TextureFromRHITexture
#define FRingAllocation FRingAllocation_D3D12
#define GetRenderTargetFormat GetRenderTargetFormat_D3D12
#define ED3D11ShaderOffsetBuffer ED3D12ShaderOffsetBuffer
#define FindShaderResourceDXGIFormat FindShaderResourceDXGIFormat_D3D12
#define FindUnorderedAccessDXGIFormat FindUnorderedAccessDXGIFormat_D3D12
#define FindDepthStencilDXGIFormat FindDepthStencilDXGIFormat_D3D12
#define HasStencilBits HasStencilBits_D3D12
#define FVector4VertexDeclaration FVector4VertexDeclaration_D3D12
#define GLOBAL_CONSTANT_BUFFER_INDEX GLOBAL_CONSTANT_BUFFER_INDEX_D3D12
#define MAX_CONSTANT_BUFFER_SLOTS MAX_CONSTANT_BUFFER_SLOTS_D3D12
#define FD3DGPUProfiler FD3D12GPUProfiler
#define FRangeAllocator FRangeAllocator_D3D12

#include "D3D12RHIPrivate.h"
#include "D3D12Util.h"

#undef GetD3D11CubeFace
#undef VerifyD3D11Result
#undef GetD3D11TextureFromRHITexture
#undef FRingAllocation
#undef GetRenderTargetFormat
#undef ED3D11ShaderOffsetBuffer
#undef FindShaderResourceDXGIFormat
#undef FindUnorderedAccessDXGIFormat
#undef FindDepthStencilDXGIFormat
#undef HasStencilBits
#undef FVector4VertexDeclaration
#undef GLOBAL_CONSTANT_BUFFER_INDEX
#undef MAX_CONSTANT_BUFFER_SLOTS
#undef FD3DGPUProfiler
#undef FRangeAllocator


#include "../../OpenGLDrv/Public/OpenGLDrv.h"
#include "../../OpenGLDrv/Public/OpenGLResources.h"
#include "OpenGLResources.h"

extern PFNWGLSWAPINTERVALEXTPROC      DisplayCluster_wglSwapIntervalEXT_ProcAddress;

extern PFNWGLJOINSWAPGROUPNVPROC      DisplayCluster_wglJoinSwapGroupNV_ProcAddress;
extern PFNWGLBINDSWAPBARRIERNVPROC    DisplayCluster_wglBindSwapBarrierNV_ProcAddress;
extern PFNWGLQUERYSWAPGROUPNVPROC     DisplayCluster_wglQuerySwapGroupNV_ProcAddress;
extern PFNWGLQUERYMAXSWAPGROUPSNVPROC DisplayCluster_wglQueryMaxSwapGroupsNV_ProcAddress;
extern PFNWGLQUERYFRAMECOUNTNVPROC    DisplayCluster_wglQueryFrameCountNV_ProcAddress;
extern PFNWGLRESETFRAMECOUNTNVPROC    DisplayCluster_wglResetFrameCountNV_ProcAddress;


void DisplayClusterInitCapabilitiesForGL();

// This is redeclaration of WINDOWS specific FPlatformOpenGLContext
// which is declared in private OpenGLWindows.cpp file.
//@note: Keep it synced with original type (Engine\Source\Runtime\OpenGLDrv\Private\Windows\OpenGLWindows.cpp)
struct FPlatformOpenGLContext
{
	HWND WindowHandle;
	HDC DeviceContext;
	HGLRC OpenGLContext;
	bool bReleaseWindowOnDestroy;
	int32 SyncInterval;
	GLuint	ViewportFramebuffer;
	GLuint	VertexArrayObject;	// one has to be generated and set for each context (OpenGL 3.2 Core requirements)
	GLuint	BackBufferResource;
	GLenum	BackBufferTarget;
};
#endif



#if PLATFORM_LINUX
// This is redeclaration of LINUX specific FPlatformOpenGLContext
// which is declared in private OpenGLWindows.cpp file.
//@note: Keep it synced with original type (Engine\Source\Runtime\OpenGLDrv\Private\Linux\OpenGLLinux.cpp)
struct FPlatformOpenGLContext
{
	SDL_HWindow    hWnd;
	SDL_HGLContext hGLContext; // this is a (void*) pointer

	bool bReleaseWindowOnDestroy;
	int32 SyncInterval;
	GLuint	ViewportFramebuffer;
	GLuint	VertexArrayObject;	// one has to be generated and set for each context (OpenGL 3.2 Core requirements)
};

//@note: Place here any Linux targeted device implementations
#endif

