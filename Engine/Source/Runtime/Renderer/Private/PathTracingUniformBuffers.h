// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PathTracingUniformBuffers.h
=============================================================================*/

#pragma once

#include "UniformBuffer.h"

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FPathTracingData, )
	SHADER_PARAMETER(uint32, MaxBounces)
END_GLOBAL_SHADER_PARAMETER_STRUCT()


// Lights
const int32 GLightCountMaximum = 64;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FPathTracingLightData, )
	SHADER_PARAMETER(uint32, Count)
	SHADER_PARAMETER_ARRAY(uint32, Type, [GLightCountMaximum])
	// Geometry
	SHADER_PARAMETER_ARRAY(FVector, Position, [GLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector, Normal, [GLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector, dPdu, [GLightCountMaximum])
	SHADER_PARAMETER_ARRAY(FVector, dPdv, [GLightCountMaximum])
	// Color
	SHADER_PARAMETER_ARRAY(FVector, Color, [GLightCountMaximum])
	// Light-specific
	SHADER_PARAMETER_ARRAY(FVector, Dimensions, [GLightCountMaximum])
	SHADER_PARAMETER_ARRAY(float, Attenuation, [GLightCountMaximum])
	SHADER_PARAMETER_ARRAY(float, RectLightBarnCosAngle, [GLightCountMaximum])
	SHADER_PARAMETER_ARRAY(float, RectLightBarnLength, [GLightCountMaximum])
END_GLOBAL_SHADER_PARAMETER_STRUCT()


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FPathTracingAdaptiveSamplingData, )
	SHADER_PARAMETER(uint32, UseAdaptiveSampling)
	SHADER_PARAMETER(uint32, RandomSequence)
	SHADER_PARAMETER(uint32, MinimumSamplesPerPixel)
	SHADER_PARAMETER(uint32, Iteration)
	SHADER_PARAMETER(float, MaxNormalBias)
	SHADER_PARAMETER(FIntVector, VarianceDimensions)
	SHADER_PARAMETER_SRV(Buffer<float>, VarianceMipTree)
END_GLOBAL_SHADER_PARAMETER_STRUCT()
