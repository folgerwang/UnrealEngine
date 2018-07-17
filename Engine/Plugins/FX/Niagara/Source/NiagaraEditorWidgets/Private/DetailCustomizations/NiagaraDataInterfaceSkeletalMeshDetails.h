// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterfaceDetails.h"
#include "Types/SlateEnums.h"
#include "Widgets/Input/SComboBox.h"

class IDetailLayoutBuilder;
class IDetailChildrenBuilder;
class IPropertyHandle;
class SWidget;
class FName;
class FNiagaraDetailSourcedArrayBuilder;
class UNiagaraDataInterfaceSkeletalMesh;
class USkeletalMesh;

/** Details customization for Niagara skeletal mesh data interface. */
class FNiagaraDataInterfaceSkeletalMeshDetails : public FNiagaraDataInterfaceDetailsBase
{
public:
	~FNiagaraDataInterfaceSkeletalMeshDetails();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	static TSharedRef<IDetailCustomization> MakeInstance();

private:
	void OnInterfaceChanged();
	void OnDataChanged();
	TArray<TSharedPtr<FName>> GenerateSourceArray();

private:
	TSharedPtr<FNiagaraDetailSourcedArrayBuilder> RegionsBuilder;
	IDetailLayoutBuilder* LayoutBuilder;
	UNiagaraDataInterfaceSkeletalMesh*  MeshInterface;
	USkeletalMesh* MeshObject;
};
