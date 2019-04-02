// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FXSystemSet.h: Internal redirector to several fx systems.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "FXSystem.h"

/**
 * FX system.
 */
class FFXSystemSet : public FFXSystemInterface
{
public:

	TArray<FFXSystemInterface*> FXSystems;

	virtual FFXSystemInterface* GetInterface(const FName& InName) override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual void Suspend() override;
	virtual void Resume() override;
#endif // #if WITH_EDITOR

	virtual void DrawDebug(FCanvas* Canvas) override;
	virtual void AddVectorField(UVectorFieldComponent* VectorFieldComponent) override;
	virtual void RemoveVectorField(UVectorFieldComponent* VectorFieldComponent) override;
	virtual void UpdateVectorField(UVectorFieldComponent* VectorFieldComponent) override;
	virtual void PreInitViews() override;
	virtual bool UsesGlobalDistanceField() const override;
	virtual void PreRender(FRHICommandListImmediate& RHICmdList, const class FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData) override;
	virtual void PostRenderOpaque(
		FRHICommandListImmediate& RHICmdList, 
		const FUniformBufferRHIParamRef ViewUniformBuffer, 
		const class FShaderParametersMetadata* SceneTexturesUniformBufferStruct,
		FUniformBufferRHIParamRef SceneTexturesUniformBuffer) override;

protected:

	/** By making the destructor protected, an instance must be destroyed via FFXSystemInterface::Destroy. */
	virtual ~FFXSystemSet();
};
