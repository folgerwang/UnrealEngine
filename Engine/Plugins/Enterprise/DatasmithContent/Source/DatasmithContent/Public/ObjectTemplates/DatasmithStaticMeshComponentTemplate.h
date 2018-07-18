// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectTemplates/DatasmithSceneComponentTemplate.h"

#include "DatasmithStaticMeshComponentTemplate.generated.h"

UCLASS()
class DATASMITHCONTENT_API UDatasmithStaticMeshComponentTemplate : public UDatasmithSceneComponentTemplate
{
	GENERATED_BODY()

public:

	UPROPERTY()
	class UStaticMesh* StaticMesh;

	UPROPERTY()
	TArray< class UMaterialInterface* > OverrideMaterials;

	virtual void Apply( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;
};
