// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditableMeshTypes.h"
#include "EditableMeshFactory.generated.h"


// @todo mesheditor: Comment these classes and enums!


UCLASS()
class EDITABLEMESH_API UEditableMeshFactory : public UObject
{
	GENERATED_BODY()

public:

	static FEditableMeshSubMeshAddress MakeSubmeshAddress( class UPrimitiveComponent* PrimitiveComponent, const int32 LODIndex );

	static UEditableMesh* MakeEditableMesh( class UPrimitiveComponent* PrimitiveComponent, const FEditableMeshSubMeshAddress& SubMeshAddress );

	UFUNCTION( BlueprintCallable, Category = "Editable Mesh" )
	static UEditableMesh* MakeEditableMesh( class UPrimitiveComponent* PrimitiveComponent, const int32 LODIndex );

	static void RefreshEditableMesh(UEditableMesh* EditableMesh, class UPrimitiveComponent& PrimitiveComponent);

};
