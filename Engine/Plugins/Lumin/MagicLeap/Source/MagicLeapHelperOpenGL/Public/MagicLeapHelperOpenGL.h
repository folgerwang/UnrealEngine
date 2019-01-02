// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class MAGICLEAPHELPEROPENGL_API FMagicLeapHelperOpenGL
{
public:
	static void CopyImageSubData(uint32 SrcName, int32 SrcLevel, int32 SrcX, int32 SrcY, int32 SrcZ, uint32 DstName, int32 DstLevel, int32 DstX, int32 DstY, int32 DstZ, int32 SrcWidth, int32 SrcHeight, int32 SrcDepth);
	static void BlitImage(uint32 SrcFBO, uint32 SrcName, int32 SrcLevel, int32 SrcX0, int32 SrcY0, int32 SrcX1, int32 SrcY1, uint32 DstFBO, uint32 DstName, int32 DstLevel, int32 DstX0, int32 DstY0, int32 DstX1, int32 DstY1);
};
