// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraMergeable.h"
#include "NiagaraDataInterfaceBase.generated.h"

class INiagaraCompiler;
class UCurveVector;
class UCurveLinearColor;
class UCurveFloat;
class FNiagaraSystemInstance;
class FNiagaraShader;
struct FNiagaraDataInterfaceParamRef;
class FRHICommandList;

/**
* An interface to the parameter bindings for the data interface used by a Niagara compute shader.
*/
struct FNiagaraDataInterfaceParametersCS
{
	virtual ~FNiagaraDataInterfaceParametersCS() {}
	virtual void Bind(const FNiagaraDataInterfaceParamRef& ParamRef, const class FShaderParameterMap& ParameterMap) {}
	virtual void Serialize(FArchive& Ar) { }
	virtual void Set(FRHICommandList& RHICmdList, FNiagaraShader* Shader, class UNiagaraDataInterface* DataInterface) const {}
};

//////////////////////////////////////////////////////////////////////////

/** Base class for all Niagara data interfaces. */
UCLASS(abstract, EditInlineNew)
class NIAGARACORE_API UNiagaraDataInterfaceBase : public UNiagaraMergeable
{
	GENERATED_UCLASS_BODY()

public:
	
	/** Constructs the correct CS parameter type for this DI (if any). The object type returned by this can only vary by class and not per object data. */
	virtual FNiagaraDataInterfaceParametersCS* ConstructComputeParameters()const
	{
		return nullptr;
	}
};

