// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
class IDetailLayoutBuilder;
class IPropertyHandle;
class IDetailChildrenBuilder;

/** Details customization for Niagara skeletal mesh data interface. */
class FNiagaraSkeletalMeshSamplingInfoDetails : public IDetailCustomization
{
public:
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	static TSharedRef<IDetailCustomization> MakeInstance();
private:
	void OnGenerateRegionEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder);
private:
	TArray<TSharedPtr<FName>> PossibleBonesNames;
	TArray<TSharedPtr<FName>> PossibleMaterialsNames;
};
