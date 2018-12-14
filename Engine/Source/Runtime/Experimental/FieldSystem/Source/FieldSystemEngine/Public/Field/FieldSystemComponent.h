// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PrimitiveComponent.h"
#include "Field/FieldSystem.h"
#include "Field/FieldSystemComponentTypes.h"
#include "Field/FieldSystemSimulationCoreProxy.h"

#include "FieldSystemComponent.generated.h"

struct FFieldSystemSampleData;

/**
*	FieldSystemComponent
*/
UCLASS(meta = (BlueprintSpawnableComponent))
class FIELDSYSTEMENGINE_API UFieldSystemComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()
	friend class FFieldSystemEditorCommands;

public:

	//~ Begin UActorComponent Interface.
	virtual void CreateRenderState_Concurrent() override;
	virtual void SendRenderDynamicData_Concurrent() override;
	virtual void BeginPlay() override;
	//~ Begin UActorComponent Interface. 


	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual bool HasAnySockets() const override { return false; }
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	//~ Begin USceneComponent Interface.


	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	/** FieldSystem */
	void SetFieldSystem(UFieldSystem * FieldSystemIn) { FieldSystem = FieldSystemIn; }
	FORCEINLINE const UFieldSystem* GetFieldSystem() const { return FieldSystem; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Field")
	UFieldSystem* FieldSystem;


	//UFUNCTION(BlueprintCallable, Category = "Field")
	void ClearFieldSystem();

	//UFUNCTION(BlueprintCallable, Category = "Field")
	int AddRadialIntMask(FName Name, FVector Position, float Radius, int32 InteriorValue, int32 ExteriorValue, TEnumAsByte<ESetMaskConditionType> Set);

	//UFUNCTION(BlueprintCallable, Category = "Field")
	int AddRadialFalloff(FName Name, float Magnitude, FVector Position, float Radius);

	//UFUNCTION(BlueprintCallable, Category = "Field")
	int AddUniformVector(FName Name, float Magnitude, FVector Direction);

	//UFUNCTION(BlueprintCallable, Category = "Field")
	int AddRadialVector(FName Name, float Magnitude, FVector Position);

	//UFUNCTION(BlueprintCallable, Category = "Field")
	int AddSumVector(FName Name, float Magnitude, int32 ScalarField, int32 RightVectorField, int32 LeftVectorField, EFieldOperationType Operation);

	//UFUNCTION(BlueprintCallable, Category = "Field")
	int AddSumScalar(FName Name, float Magnitude, int32 RightScalarField, int32 LeftScalarField, EFieldOperationType Operation);

	//UFUNCTION(BlueprintCallable, Category = "Field")
	void ApplyField(TEnumAsByte<EFieldPhysicsDefaultFields> FieldName, TEnumAsByte<EFieldPhysicsType> Type, bool Enabled, FVector Position, FVector Direction, float Radius, float Magnitude);

	UFUNCTION(BlueprintCallable, Category = "Field")
	void ApplyLinearForce(bool Enabled, FVector Direction, float Magnitude);

	UFUNCTION(BlueprintCallable, Category = "Field")
	void ApplyRadialForce(bool Enabled, FVector Position, float Magnitude);

	UFUNCTION(BlueprintCallable, Category = "Field")
	void ApplyStayDynamicField(bool Enabled, FVector Position, float Radius, int MaxLevelPerCommand);

	UFUNCTION(BlueprintCallable, Category = "Field")
	void ApplyRadialVectorFalloffForce(bool Enabled, FVector Position, float Radius, float Magnitude);

	UFUNCTION(BlueprintCallable, Category = "Field")
	void ApplyUniformVectorFalloffForce(bool Enabled, FVector Position, FVector Direction, float Radius, float Magnitude);

protected:

	/** Populate the static geometry structures for the render thread. */
	void InitSampleData(FFieldSystemSampleData* SampleData);

	/** Helpers for dispatching field interaction commands safely to the physics thread */
	void DispatchCommand(const FFieldSystemCommand& InCommand);
	void DispatchCommand(const FName& InName, EFieldPhysicsType InType, const FVector& InPosition, const FVector& InDirection, float InRadius, float InMagnitude);

	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
	virtual bool ShouldCreatePhysicsState() const override;
	virtual bool HasValidPhysicsState() const override;

#if INCLUDE_CHAOS
	FFieldSystemSimulationProxy* PhysicsProxy;

	FChaosSolversModule* ChaosModule;
#endif
	bool bHasPhysicsState;

};
