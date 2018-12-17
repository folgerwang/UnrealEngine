// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IEditableMeshFormat.h"


/**
 * Implements interactive mesh editing support for Geometry Collections
 */ 
class FGeometryCollectionEditableMeshFormat : public IEditableMeshFormat
{

public:

	// IEditableMeshFormat interface
	virtual void FillMeshObjectPtr( class UPrimitiveComponent& Component, FEditableMeshSubMeshAddress& SubMeshAddress ) override;
	virtual UEditableMesh* MakeEditableMesh( class UPrimitiveComponent& Component, const FEditableMeshSubMeshAddress& SubMeshAddress ) override;
	virtual void RefreshEditableMesh(UEditableMesh* EditableMesh, UPrimitiveComponent& Component) override;
	virtual bool HandlesComponentType(class UPrimitiveComponent& Component) override;
	virtual bool HandlesBones() override;

private:
	void RegisterMeshAttributes(struct FMeshDescription* MeshDescription);

};
