// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraFunctionLibrary.h"
#include "EngineGlobals.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Engine.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "ContentStreaming.h"

#include "NiagaraWorldManager.h"

UNiagaraFunctionLibrary::UNiagaraFunctionLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}


UNiagaraComponent* CreateNiagaraSystem(UNiagaraSystem* SystemTemplate, UWorld* World, AActor* Actor, bool bAutoDestroy, EPSCPoolMethod PoolingMethod)
{
	// todo : implement pooling method.

	UNiagaraComponent* NiagaraComponent = NewObject<UNiagaraComponent>((Actor ? Actor : (UObject*)World));
	NiagaraComponent->SetAutoDestroy(bAutoDestroy);
	NiagaraComponent->bAllowAnyoneToDestroyMe = true;
	NiagaraComponent->SetAsset(SystemTemplate);
	return NiagaraComponent;
}


/**
* Spawns a Niagara System at the specified world location/rotation
* @return			The spawned UNiagaraComponent
*/
UNiagaraComponent* UNiagaraFunctionLibrary::SpawnSystemAtLocation(UObject* WorldContextObject, UNiagaraSystem* SystemTemplate, FVector SpawnLocation, FRotator SpawnRotation, bool bAutoDestroy)
{
	UNiagaraComponent* PSC = NULL;
	if (SystemTemplate)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World != nullptr)
		{
			PSC = CreateNiagaraSystem(SystemTemplate, World, World->GetWorldSettings(), bAutoDestroy, EPSCPoolMethod::None);
#if WITH_EDITORONLY_DATA
			PSC->bWaitForCompilationOnActivate = true;
#endif
			PSC->bAutoActivate = false;
			PSC->RegisterComponentWithWorld(World);

			PSC->SetAbsolute(true, true, true);
			PSC->SetWorldLocationAndRotation(SpawnLocation, SpawnRotation);
			PSC->SetRelativeScale3D(FVector(1.f));
			PSC->Activate(true);
		}
	}
	return PSC;
}





/**
* Spawns a Niagara System attached to a component
* @return			The spawned UNiagaraComponent
*/
UNiagaraComponent* UNiagaraFunctionLibrary::SpawnSystemAttached(UNiagaraSystem* SystemTemplate, USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, FRotator Rotation, EAttachLocation::Type LocationType, bool bAutoDestroy)
{
	UNiagaraComponent* PSC = nullptr;
	if (SystemTemplate)
	{
		if (AttachToComponent == NULL)
		{
			UE_LOG(LogScript, Warning, TEXT("UNiagaraFunctionLibrary::SpawnSystemAttached: NULL AttachComponent specified!"));
		}
		else
		{
			PSC = CreateNiagaraSystem(SystemTemplate, AttachToComponent->GetWorld(), AttachToComponent->GetOwner(), bAutoDestroy, EPSCPoolMethod::None);
			PSC->RegisterComponentWithWorld(AttachToComponent->GetWorld());

			PSC->AttachToComponent(AttachToComponent, FAttachmentTransformRules::KeepRelativeTransform, AttachPointName);
			if (LocationType == EAttachLocation::KeepWorldPosition)
			{
				PSC->SetWorldLocationAndRotation(Location, Rotation);
			}
			else
			{
				PSC->SetRelativeLocationAndRotation(Location, Rotation);
			}
			PSC->SetRelativeScale3D(FVector(1.f));
		}
	}
	return PSC;
}

/**
* Spawns a Niagara System attached to a component
* @return			The spawned UNiagaraComponent
*/

UNiagaraComponent* UNiagaraFunctionLibrary::SpawnSystemAttached(
	UNiagaraSystem* SystemTemplate,
	USceneComponent* AttachToComponent,
	FName AttachPointName,
	FVector Location,
	FRotator Rotation,
	FVector Scale,
	EAttachLocation::Type LocationType,
	bool bAutoDestroy,
	EPSCPoolMethod PoolingMethod
)
{
	UNiagaraComponent* PSC = nullptr;
	if (SystemTemplate)
	{
		if (!AttachToComponent)
		{
			UE_LOG(LogScript, Warning, TEXT("UGameplayStatics::SpawnNiagaraEmitterAttached: NULL AttachComponent specified!"));
		}
		else
		{
			UWorld* const World = AttachToComponent->GetWorld();
			if (World && !World->IsNetMode(NM_DedicatedServer))
			{
				PSC = CreateNiagaraSystem(SystemTemplate, World, AttachToComponent->GetOwner(), bAutoDestroy, PoolingMethod);
				if (PSC)
				{
					PSC->SetupAttachment(AttachToComponent, AttachPointName);

					if (LocationType == EAttachLocation::KeepWorldPosition)
					{
						const FTransform ParentToWorld = AttachToComponent->GetSocketTransform(AttachPointName);
						const FTransform ComponentToWorld(Rotation, Location, Scale);
						const FTransform RelativeTM = ComponentToWorld.GetRelativeTransform(ParentToWorld);
						PSC->RelativeLocation = RelativeTM.GetLocation();
						PSC->RelativeRotation = RelativeTM.GetRotation().Rotator();
						PSC->RelativeScale3D = RelativeTM.GetScale3D();
					}
					else
					{
						PSC->RelativeLocation = Location;
						PSC->RelativeRotation = Rotation;

						if (LocationType == EAttachLocation::SnapToTarget)
						{
							// SnapToTarget indicates we "keep world scale", this indicates we we want the inverse of the parent-to-world scale 
							// to calculate world scale at Scale 1, and then apply the passed in Scale
							const FTransform ParentToWorld = AttachToComponent->GetSocketTransform(AttachPointName);
							PSC->RelativeScale3D = Scale * ParentToWorld.GetSafeScaleReciprocal(ParentToWorld.GetScale3D());
						}
						else
						{
							PSC->RelativeScale3D = Scale;
						}
					}

					PSC->RegisterComponentWithWorld(World);
					PSC->Activate(true);

					// Notify the texture streamer so that PSC gets managed as a dynamic component.
					IStreamingManager::Get().NotifyPrimitiveUpdated(PSC);
				}
			}
		}
	}
	return PSC;
}

/**
* Set a constant in an emitter of a Niagara System
void UNiagaraFunctionLibrary::SetUpdateScriptConstant(UNiagaraComponent* Component, FName EmitterName, FName ConstantName, FVector Value)
{
	TArray<TSharedPtr<FNiagaraEmitterInstance>> &Emitters = Component->GetSystemInstance()->GetEmitters();

	for (TSharedPtr<FNiagaraEmitterInstance> &Emitter : Emitters)
	{		
		if(UNiagaraEmitter* PinnedProps = Emitter->GetProperties().Get())
		{
			FName CurName = *PinnedProps->EmitterName;
			if (CurName == EmitterName)
			{
				Emitter->GetProperties()->UpdateScriptProps.ExternalConstants.SetOrAdd(FNiagaraTypeDefinition::GetVec4Def(), ConstantName, Value);
				break;
			}
		}
	}
}
*/

UNiagaraParameterCollectionInstance* UNiagaraFunctionLibrary::GetNiagaraParameterCollection(UObject* WorldContextObject, UNiagaraParameterCollection* Collection)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World != nullptr)
	{
		return FNiagaraWorldManager::Get(World)->GetParameterCollection(Collection);
	}
	return nullptr;
}
