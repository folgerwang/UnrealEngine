// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "ControlRigComponent.generated.h"

class UControlRig;
class UControlRigComponent;

/** Bindable event for external objects to hook into ControlRig-level execution */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FControlRigSignature, UControlRigComponent*, Component);

/** A component that hosts an animation ControlRig, manages control components and marshals data between the two */
UCLASS(Blueprintable, ClassGroup = "Animation", meta = (BlueprintSpawnableComponent))
class CONTROLRIG_API UControlRigComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

public:
	/** Event fired before this component's ControlRig is initialized */
	UPROPERTY(BlueprintAssignable, Category = "ControlRig", meta = (DisplayName = "On Pre Initialize"))
	FControlRigSignature OnPreInitializeDelegate;

	/** Event fired before this component's ControlRig is evaluated */
	UPROPERTY(BlueprintAssignable, Category = "ControlRig", meta = (DisplayName = "On Post Initialize"))
	FControlRigSignature OnPostInitializeDelegate;

	/** Event fired before this component's ControlRig is evaluated */
	UPROPERTY(BlueprintAssignable, Category = "ControlRig", meta = (DisplayName = "On Pre Evaluate"))
	FControlRigSignature OnPreEvaluateDelegate;

	/** Event fired after this component's ControlRig is evaluated */
	UPROPERTY(BlueprintAssignable, Category = "ControlRig", meta = (DisplayName = "On Post Evaluate"))
	FControlRigSignature OnPostEvaluateDelegate;

	/** The current root instance of our ControlRig */
	UPROPERTY(EditAnywhere, Instanced, Category = "ControlRig", meta = (ShowOnlyInnerProperties))
	UControlRig* ControlRig;

public:
	// UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;

	/** Get the ControlRig hosted by this component */
	UFUNCTION(BlueprintPure, Category = "ControlRig", meta = (DisplayName = "Get ControlRig", ScriptName = "GetControlRig"))
	UControlRig* BP_GetControlRig() const;

	UFUNCTION(BlueprintNativeEvent, Category = "ControlRig", meta = (CallInEditor = "true"))
	void OnPreInitialize();

	UFUNCTION(BlueprintNativeEvent, Category = "ControlRig", meta = (CallInEditor = "true"))
	void OnPostInitialize();

	UFUNCTION(BlueprintNativeEvent, Category = "ControlRig", meta = (CallInEditor = "true"))
	void OnPreEvaluate();

	UFUNCTION(BlueprintNativeEvent, Category = "ControlRig", meta = (CallInEditor = "true"))
	void OnPostEvaluate();

	/** Get the ControlRig hosted by this component */
	template<typename ControlRigType>
	ControlRigType* GetControlRig() const
	{
		return Cast<ControlRigType>(BP_GetControlRig());
	}

private:
	friend struct FControlRigComponentInstanceData;
};

/** Used to store animation ControlRig data during recompile of BP */
USTRUCT()
struct FControlRigComponentInstanceData : public FActorComponentInstanceData
{
	GENERATED_BODY()
public:
	FControlRigComponentInstanceData()
		: AnimControlRig(nullptr)
	{}
	FControlRigComponentInstanceData(const UControlRigComponent* SourceComponent)
		: FActorComponentInstanceData(SourceComponent)
		, AnimControlRig(SourceComponent->ControlRig)
	{}
	virtual ~FControlRigComponentInstanceData() = default;

	virtual bool ContainsData() const override
	{
		return (AnimControlRig != nullptr) || Super::ContainsData();
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// stored object
	UPROPERTY()
	UControlRig* AnimControlRig;
};
