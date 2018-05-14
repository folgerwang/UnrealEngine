// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AppleVisionBlueprintSupport.h"
#include "AppleVisionBlueprintProxy.h"

UK2Node_DetectFaces::UK2Node_DetectFaces(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UAppleVisionDetectFacesAsyncTaskBlueprintProxy, CreateProxyObjectForDetectFaces);
	ProxyFactoryClass = UAppleVisionDetectFacesAsyncTaskBlueprintProxy::StaticClass();
	ProxyClass = UAppleVisionDetectFacesAsyncTaskBlueprintProxy::StaticClass();
}
