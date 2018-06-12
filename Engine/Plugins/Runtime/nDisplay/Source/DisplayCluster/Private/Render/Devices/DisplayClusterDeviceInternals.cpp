// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterDeviceInternals.h"


#if PLATFORM_WINDOWS
PFNWGLSWAPINTERVALEXTPROC    DisplayCluster_wglSwapIntervalEXT_ProcAddress   = nullptr;

PFNWGLJOINSWAPGROUPNVPROC      DisplayCluster_wglJoinSwapGroupNV_ProcAddress   = nullptr;
PFNWGLBINDSWAPBARRIERNVPROC    DisplayCluster_wglBindSwapBarrierNV_ProcAddress = nullptr;
PFNWGLQUERYSWAPGROUPNVPROC     DisplayCluster_wglQuerySwapGroupNV_ProcAddress = nullptr;
PFNWGLQUERYMAXSWAPGROUPSNVPROC DisplayCluster_wglQueryMaxSwapGroupsNV_ProcAddress = nullptr;
PFNWGLQUERYFRAMECOUNTNVPROC    DisplayCluster_wglQueryFrameCountNV_ProcAddress = nullptr;
PFNWGLRESETFRAMECOUNTNVPROC    DisplayCluster_wglResetFrameCountNV_ProcAddress = nullptr;


// Copy/pasted from OpenGLDrv.cpp
static void DisplayClusterGetExtensionsString(FString& ExtensionsString)
{
	GLint ExtensionCount = 0;
	ExtensionsString = TEXT("");
	if (FOpenGL::SupportsIndexedExtensions())
	{
		glGetIntegerv(GL_NUM_EXTENSIONS, &ExtensionCount);
		for (int32 ExtensionIndex = 0; ExtensionIndex < ExtensionCount; ++ExtensionIndex)
		{
			const ANSICHAR* ExtensionString = FOpenGL::GetStringIndexed(GL_EXTENSIONS, ExtensionIndex);

			ExtensionsString += TEXT(" ");
			ExtensionsString += ANSI_TO_TCHAR(ExtensionString);
		}
	}
	else
	{
		const ANSICHAR* GlGetStringOutput = (const ANSICHAR*)glGetString(GL_EXTENSIONS);
		if (GlGetStringOutput)
		{
			ExtensionsString += GlGetStringOutput;
			ExtensionsString += TEXT(" ");
		}
	}
}

// https://www.opengl.org/wiki/Load_OpenGL_Functions
static void* DisplayClusterGetGLFuncAddress(const char *name)
{
	HMODULE module = LoadLibraryA("opengl32.dll");
	if (module)
	{
		return (void *)GetProcAddress(module, name);
	}
	else
	{
		return nullptr;
	}
}

// Copy/pasted from OpenGLDevice.cpp
// static void InitRHICapabilitiesForGL()
void DisplayClusterInitCapabilitiesForGL()
{
	bool bWindowsSwapControlExtensionPresent = false;
	{
		FString ExtensionsString;
		DisplayClusterGetExtensionsString(ExtensionsString);

		if (ExtensionsString.Contains(TEXT("WGL_EXT_swap_control")))
		{
			bWindowsSwapControlExtensionPresent = true;
		}
	}

#pragma warning(push)
#pragma warning(disable:4191)
	if (bWindowsSwapControlExtensionPresent)
	{
		DisplayCluster_wglSwapIntervalEXT_ProcAddress = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	}

	DisplayCluster_wglJoinSwapGroupNV_ProcAddress      = (PFNWGLJOINSWAPGROUPNVPROC)wglGetProcAddress("wglJoinSwapGroupNV");
	DisplayCluster_wglBindSwapBarrierNV_ProcAddress    = (PFNWGLBINDSWAPBARRIERNVPROC)wglGetProcAddress("wglBindSwapBarrierNV");
	DisplayCluster_wglQuerySwapGroupNV_ProcAddress     = (PFNWGLQUERYSWAPGROUPNVPROC)wglGetProcAddress("wglQuerySwapGroupNV");
	DisplayCluster_wglQueryMaxSwapGroupsNV_ProcAddress = (PFNWGLQUERYMAXSWAPGROUPSNVPROC)wglGetProcAddress("wglQueryMaxSwapGroupsNV");
	DisplayCluster_wglQueryFrameCountNV_ProcAddress    = (PFNWGLQUERYFRAMECOUNTNVPROC)wglGetProcAddress("wglQueryFrameCountNV");
	DisplayCluster_wglResetFrameCountNV_ProcAddress    = (PFNWGLRESETFRAMECOUNTNVPROC)wglGetProcAddress("wglResetFrameCountNV");

#pragma warning(pop)
}
#endif



#if PLATFORM_LINUX
//@todo: Implementation for Linux
#endif
