// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderResource.h"
#include "RendererInterface.h"
#include "PrimitiveUniformShaderParameters.h"

class FRHICommandList;
class FScene;
class FViewInfo;

extern void UploadDynamicPrimitiveShaderDataForView(FRHICommandList& RHICmdList, FScene& Scene, FViewInfo& View);
extern void UpdateGPUScene(FRHICommandList& RHICmdList, FScene& Scene);
extern void AddPrimitiveToUpdateGPU(FScene& Scene, int32 PrimitiveId);

