// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "RHIDefinitions.h"
#include "NiagaraTypes.h"
#include "NiagaraCommon.h"
#include "NiagaraMergeable.h"
#include "NiagaraRendererProperties.generated.h"


UENUM()
enum class ENiagaraSortMode : uint8
{
	/** Perform no additional sorting prior to rendering.*/
	None,
	/** Sort by depth to the camera's near plane.*/
	ViewDepth,
	/** Sort by distance to the camera's origin.*/
	ViewDistance,
	/** Custom sorting according to a per particle attribute. Lower values are rendered before higher values. */
	CustomAscending,
	/** Custom sorting according to a per particle attribute. Higher values are rendered before lower values. */
	CustomDecending,
};


/**
* Emitter properties base class
* Each EmitterRenderer derives from this with its own class, and returns it in GetProperties; a copy
* of those specific properties is stored on UNiagaraEmitter (on the System) for serialization 
* and handed back to the System renderer on load.
*/

class NiagaraRenderer;
class UMaterial;
class UMaterialInterface;

UCLASS(ABSTRACT)
class NIAGARA_API UNiagaraRendererProperties : public UNiagaraMergeable
{
	GENERATED_BODY()

public:
	UNiagaraRendererProperties()
	: bIsEnabled(true)
	{
	}
	virtual NiagaraRenderer* CreateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel) PURE_VIRTUAL ( UNiagaraRendererProperties::CreateEmitterRenderer, return nullptr;);
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials) const PURE_VIRTUAL(UNiagaraRendererProperties::GetUsedMaterials,);

	virtual bool IsSimTargetSupported(ENiagaraSimTarget InSimTarget) const { return false; };

#if WITH_EDITORONLY_DATA
	virtual bool IsMaterialValidForRenderer(UMaterial* Material, FText& InvalidMessage) { return true; }

	virtual void FixMaterial(UMaterial* Material) { }

	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() { static TArray<FNiagaraVariable> Vars; return Vars; };
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() { static TArray<FNiagaraVariable> Vars; return Vars; };
#endif // WITH_EDITORONLY_DATA


	// GPU simulation uses DrawIndirect, so the sim step needs to know indices per instance in order to prepare the draw call parameters
	virtual uint32 GetNumIndicesPerInstance() { return 0; }

	virtual bool GetIsEnabled() const
	{
		return bIsEnabled;
	}

	virtual void SetIsEnabled(bool bInIsEnabled)
	{
		bIsEnabled = bInIsEnabled;
	}
	/** By default, emitters are drawn in the order that they are added to the system. This value will allow you to control the order in a more fine-grained manner. 
	Materials of the same type (i.e. Transparent) will draw in order from lowest to highest within the system. The default value is 0.*/
	UPROPERTY(EditAnywhere, Category = "Sort Order")
	int32 SortOrderHint;
	
	UPROPERTY()
	bool bIsEnabled;
};


