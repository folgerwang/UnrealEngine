// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraShaderModule.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(INiagaraShaderModule, NiagaraShader);

INiagaraShaderModule* INiagaraShaderModule::Singleton(nullptr);

FDelegateHandle INiagaraShaderModule::SetOnProcessShaderCompilationQueue(FOnProcessQueue InOnProcessQueue)
{
	checkf(OnProcessQueue.IsBound() == false, TEXT("Shader processing queue delegate already set."));
	OnProcessQueue = InOnProcessQueue;
	return OnProcessQueue.GetHandle();
}

void INiagaraShaderModule::ResetOnProcessShaderCompilationQueue(FDelegateHandle DelegateHandle)
{
	checkf(OnProcessQueue.GetHandle() == DelegateHandle, TEXT("Can only reset the process compilation queue delegate with the handle it was created with."));
	OnProcessQueue.Unbind();
}

void INiagaraShaderModule::ProcessShaderCompilationQueue()
{
	checkf(OnProcessQueue.IsBound(), TEXT("Can not process shader queue.  Delegate was never set."));
	return OnProcessQueue.Execute();
}

FDelegateHandle INiagaraShaderModule::SetOnRequestDefaultDataInterfaceHandler(FOnRequestDefaultDataInterface InHandler)
{
	checkf(OnRequestDefaultDataInterface.IsBound() == false, TEXT("Shader OnRequestDefaultDataInterface delegate already set."));
	OnRequestDefaultDataInterface = InHandler;
	return OnRequestDefaultDataInterface.GetHandle();
}

void  INiagaraShaderModule::ResetOnRequestDefaultDataInterfaceHandler()
{
	OnRequestDefaultDataInterface.Unbind();
}

UNiagaraDataInterfaceBase* INiagaraShaderModule::RequestDefaultDataInterface(const FString& DIClassName)
{
	checkf(OnRequestDefaultDataInterface.IsBound(), TEXT("Can not invoke OnRequestDefaultDataInterface.  Delegate was never set."));
	return OnRequestDefaultDataInterface.Execute(DIClassName);
}