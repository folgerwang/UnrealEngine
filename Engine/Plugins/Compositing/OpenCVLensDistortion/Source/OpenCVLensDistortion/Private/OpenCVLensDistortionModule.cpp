// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IOpenCVLensDistortionModule.h"

#include "Logging/LogMacros.h"


DEFINE_LOG_CATEGORY(LogOpenCVLensDistortion)

//////////////////////////////////////////////////////////////////////////
// FOpenCVLensDistortionModule
class FOpenCVLensDistortionModule : public IOpenCVLensDistortionModule
{

};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FOpenCVLensDistortionModule, OpenCVLensDistortion);


