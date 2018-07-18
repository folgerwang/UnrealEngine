// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "NiagaraParameterCollection.h"
#include "UObject/GCObject.h"
#include "NiagaraDataSet.h"
#include "NiagaraScriptExecutionContext.h"
#include "NiagaraSystemSimulation.h"

#include "NiagaraDataInterfaceSkeletalMesh.h"

class UWorld;
class UNiagaraParameterCollection;
class UNiagaraParameterCollectionInstance;



class FNiagaraViewDataMgr : public FRenderResource
{
public:
	FNiagaraViewDataMgr();

	static void Init();
	static void Shutdown();

	void PostOpaqueRender(FPostOpaqueRenderParameters& Params)
	{
		SceneDepthTexture = Params.DepthTexture;
		ViewUniformBuffer = Params.ViewUniformBuffer;
		SceneNormalTexture = Params.NormalTexture;
		SceneTexturesUniformParams = Params.SceneTexturesUniformParams;
	}

	FTexture2DRHIParamRef GetSceneDepthTexture() { return SceneDepthTexture; }
	FTexture2DRHIParamRef GetSceneNormalTexture() { return SceneNormalTexture; }
	FUniformBufferRHIParamRef GetViewUniformBuffer() { return ViewUniformBuffer; }
	TUniformBufferRef<FSceneTexturesUniformParameters> GetSceneTextureUniformParameters() { return SceneTexturesUniformParams; }

	virtual void InitDynamicRHI() override;

	virtual void ReleaseDynamicRHI() override;
private:
	FTexture2DRHIParamRef SceneDepthTexture;
	FTexture2DRHIParamRef SceneNormalTexture;
	FUniformBufferRHIParamRef ViewUniformBuffer;

	TUniformBufferRef<FSceneTexturesUniformParameters> SceneTexturesUniformParams;
	FPostOpaqueRenderDelegate PostOpaqueDelegate;
};

extern TGlobalResource<FNiagaraViewDataMgr> GNiagaraViewDataManager;


/**
* Manager class for any data relating to a particular world.
*/
class FNiagaraWorldManager : public FGCObject
{
public:
	
	FNiagaraWorldManager(UWorld* InWorld);
	static FNiagaraWorldManager* Get(UWorld* World);

	//~ GCObject Interface
	void AddReferencedObjects(FReferenceCollector& Collector);
	//~ GCObject Interface
	
	UNiagaraParameterCollectionInstance* GetParameterCollection(UNiagaraParameterCollection* Collection);
	void SetParameterCollection(UNiagaraParameterCollectionInstance* NewInstance);
	void CleanupParameterCollections();
	TSharedRef<FNiagaraSystemSimulation, ESPMode::ThreadSafe> GetSystemSimulation(UNiagaraSystem* System);
	void DestroySystemSimulation(UNiagaraSystem* System);

	void Tick(float DeltaSeconds);

	void OnWorldCleanup(bool bSessionEnded, bool bCleanupResources);
	
	FORCEINLINE FNDI_SkeletalMesh_GeneratedData& GetSkeletalMeshGeneratedData() { return SkeletalMeshGeneratedData; }
private:
	UWorld* World;

	TMap<UNiagaraParameterCollection*, UNiagaraParameterCollectionInstance*> ParameterCollections;

	TMap<UNiagaraSystem*, TSharedRef<FNiagaraSystemSimulation, ESPMode::ThreadSafe>> SystemSimulations;

	int32 CachedEffectsQuality;

	/** Generated data used by data interfaces*/
	FNDI_SkeletalMesh_GeneratedData SkeletalMeshGeneratedData;
};

