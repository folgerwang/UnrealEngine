// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SkeletalMeshComponent.h"
#include "SkeletalMeshComponentBudgeted.generated.h"

class IAnimationBudgetAllocator;
class USkeletalMeshComponentBudgeted;

/** Delegate called to increase/decrease the amount of work a component performs */
DECLARE_DELEGATE_TwoParams(FOnReduceWork, USkeletalMeshComponentBudgeted* /*InComponent*/, bool /*bReduce*/);

/** Delegate called to calculate significance if bAutoCalculateSignificance = true */
DECLARE_DELEGATE_RetVal_OneParam(float, FOnCalculateSignificance, USkeletalMeshComponentBudgeted* /*InComponent*/);

/** A skeletal mesh component that has its tick rate governed by a global animation budget */
UCLASS(meta=(BlueprintSpawnableComponent))
class ANIMATIONBUDGETALLOCATOR_API USkeletalMeshComponentBudgeted : public USkeletalMeshComponent
{
	GENERATED_BODY()

	friend class FAnimationBudgetAllocator;

public:
	USkeletalMeshComponentBudgeted(const FObjectInitializer& ObjectInitializer);

	/** Set this component to automatically register with the budget allocator */
	UFUNCTION(BlueprintSetter)
	void SetAutoRegisterWithBudgetAllocator(bool bInAutoRegisterWithBudgetAllocator) { bAutoRegisterWithBudgetAllocator = bInAutoRegisterWithBudgetAllocator; }

	/** Set this component to automatically calculate its significance */
	void SetAutoCalculateSignificance(bool bInAutoCalculateSignificance) { bAutoCalculateSignificance = bInAutoCalculateSignificance; }

	/** Check whether this component auto-calculates its significance */
	bool GetAutoCalculateSignificance() const { return bAutoCalculateSignificance; }

	/** Get delegate called to increase/decrease the amount of work a component performs */
	FOnReduceWork& OnReduceWork() { return OnReduceWorkDelegate; }

	/** Get delegate called to calculate significance if bAutoCalculateSignificance = true */
	static FOnCalculateSignificance& OnCalculateSignificance() { return OnCalculateSignificanceDelegate; }

private:
	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// USkeletalMeshComponent interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void CompleteParallelAnimationEvaluation(bool bDoPostAnimEvaluation) override;

	/** Get the handle used to identify this component to the allocator */
	int32 GetAnimationBudgetHandle() const { return AnimationBudgetHandle; }

	/** Set the handle used to identify this component to the allocator */
	void SetAnimationBudgetHandle(int32 InHandle) { AnimationBudgetHandle = InHandle; }

	/** Set the budget allocator that is tracking us */
	void SetAnimationBudgetAllocator(IAnimationBudgetAllocator* InAnimationBudgetAllocator) { AnimationBudgetAllocator = InAnimationBudgetAllocator; }

private:
	/** Delegate called to increase/decrease the amount of work a component performs */
	FOnReduceWork OnReduceWorkDelegate;

	/** Delegate called to calculate significance if bAutoCalculateSignificance = true */
	static FOnCalculateSignificance OnCalculateSignificanceDelegate;

	/** Handle used for identification */
	int32 AnimationBudgetHandle;

	/** Owning animation budget allocator */
	IAnimationBudgetAllocator* AnimationBudgetAllocator;

	/** Whether this component should automatically register with the budget allocator in OnRegister/OnUnregister */
	UPROPERTY(EditAnywhere, BlueprintSetter=SetAutoRegisterWithBudgetAllocator, Category = Budgeting)
	uint8 bAutoRegisterWithBudgetAllocator : 1;

	/** Whether this component should automatically register with the budget allocator in OnRegister/OnUnregister */
	UPROPERTY(EditAnywhere, Category = Budgeting)
	uint8 bAutoCalculateSignificance : 1;
};