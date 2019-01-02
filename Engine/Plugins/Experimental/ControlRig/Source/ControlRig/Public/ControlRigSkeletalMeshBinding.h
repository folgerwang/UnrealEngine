// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "IControlRigObjectBinding.h"

class USkeletalMeshComponent;

class CONTROLRIG_API FControlRigSkeletalMeshBinding : public IControlRigObjectBinding
{
public:
	// IControlRigObjectBinding interface
	virtual void BindToObject(UObject* InObject) override;
	virtual void UnbindFromObject() override;
	virtual bool IsBoundToObject(UObject* InObject) const override;
	virtual UObject* GetBoundObject() const override;
	virtual AActor* GetHostingActor() const override;

private:
	/** The skeletal mesh component we are bound to */
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;
};