// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AppleImageUtilsBlueprintSupport.h"
#include "AppleImageUtilsBlueprintProxy.h"

UK2Node_ConvertToJPEG::UK2Node_ConvertToJPEG(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UAppleImageUtilsBaseAsyncTaskBlueprintProxy, CreateProxyObjectForConvertToJPEG);
	ProxyFactoryClass = UAppleImageUtilsBaseAsyncTaskBlueprintProxy::StaticClass();
	ProxyClass = UAppleImageUtilsBaseAsyncTaskBlueprintProxy::StaticClass();
}

UK2Node_ConvertToHEIF::UK2Node_ConvertToHEIF(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UAppleImageUtilsBaseAsyncTaskBlueprintProxy, CreateProxyObjectForConvertToHEIF);
	ProxyFactoryClass = UAppleImageUtilsBaseAsyncTaskBlueprintProxy::StaticClass();
	ProxyClass = UAppleImageUtilsBaseAsyncTaskBlueprintProxy::StaticClass();
}

UK2Node_ConvertToTIFF::UK2Node_ConvertToTIFF(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UAppleImageUtilsBaseAsyncTaskBlueprintProxy, CreateProxyObjectForConvertToTIFF);
	ProxyFactoryClass = UAppleImageUtilsBaseAsyncTaskBlueprintProxy::StaticClass();
	ProxyClass = UAppleImageUtilsBaseAsyncTaskBlueprintProxy::StaticClass();
}

UK2Node_ConvertToPNG::UK2Node_ConvertToPNG(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UAppleImageUtilsBaseAsyncTaskBlueprintProxy, CreateProxyObjectForConvertToPNG);
	ProxyFactoryClass = UAppleImageUtilsBaseAsyncTaskBlueprintProxy::StaticClass();
	ProxyClass = UAppleImageUtilsBaseAsyncTaskBlueprintProxy::StaticClass();
}
