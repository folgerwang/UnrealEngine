// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "EditorSkeletalMeshLibrary.generated.h"

class USkeletalMesh;

/**
* Utility class to altering and analyzing a SkeletalMesh and use the common functionalities of the SkeletalMesh Editor.
* The editor should not be in play in editor mode.
 */
UCLASS()
class EDITORSCRIPTINGUTILITIES_API UEditorSkeletalMeshLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Regenerate LODs of the mesh
	 *
	 * @param SkeletalMesh	The mesh that will regenerate LOD
	 * @param NewLODCount	Set valid value (>0) if you want to change LOD count.
	 *						Otherwise, it will use the current LOD and regenerate
	 * @param bRegenerateEvenIfImported	If this is true, it only regenerate even if this LOD was imported before
	 *									If false, it will regenerate for only previously auto generated ones
	 * @param bGenerateBaseLOD If this is true and there is some reduction data, the base LOD will be reduce according to the settings
	 * @return	true if succeed. If mesh reduction is not available this will return false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | SkeletalMesh", meta = (ScriptMethod))
	static bool RegenerateLOD(USkeletalMesh* SkeletalMesh, int32 NewLODCount = 0, bool bRegenerateEvenIfImported = false, bool bGenerateBaseLOD = false);

	/** Get number of mesh vertices for an LOD of a Skeletal Mesh
	 *
	 * @param SkeletalMesh		Mesh to get number of vertices from.
	 * @param LODIndex			Index of the mesh LOD.
	 * @return Number of vertices. Returns 0 if invalid mesh or LOD index.
	 */
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | SkeletalMesh")
	static int32 GetNumVerts(USkeletalMesh* SkeletalMesh, int32 LODIndex);
};

