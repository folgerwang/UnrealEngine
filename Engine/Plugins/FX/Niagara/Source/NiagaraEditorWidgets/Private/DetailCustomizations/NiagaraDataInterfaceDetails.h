// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IDetailLayoutBuilder;
class UNiagaraDataInterface;
class IDetailCategoryBuilder;
class FNiagaraDataInterfaceCustomNodeBuilder;

/** Base details customization for Niagara data interfaces. */
class FNiagaraDataInterfaceDetailsBase : public IDetailCustomization
{
public:
	~FNiagaraDataInterfaceDetailsBase();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	static TSharedRef<IDetailCustomization> MakeInstance();

private:
	void OnDataChanged();

private:
	TWeakObjectPtr<UNiagaraDataInterface> DataInterface;
	TSharedPtr<FNiagaraDataInterfaceCustomNodeBuilder> CustomBuilder;
	IDetailCategoryBuilder* ErrorsCategoryBuilder;
	IDetailLayoutBuilder* Builder;
};
