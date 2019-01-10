// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "UObject/ObjectMacros.h"

#include "GLTFLogger.h"

class UStaticMesh;

namespace GLTF
{
	struct FAsset;
	class FStaticMeshImporterImpl;

	class GLTFIMPORTER_API FStaticMeshImporter
	{
	public:
		FStaticMeshImporter();
		~FStaticMeshImporter();

		const TArray<UStaticMesh*>& ImportMeshes(const GLTF::FAsset& Asset, UObject* ParentPackage, EObjectFlags Flags, bool bApplyPostEditChange);

		const TArray<UStaticMesh*>& GetMeshes() const;
		const TArray<FLogMessage>&  GetLogMessages() const;

		float GetUniformScale() const;
		void  SetUniformScale(float Scale);

		bool GetGenerateLightmapUVs() const;
		void SetGenerateLightmapUVs(bool bGenerateLightmapUVs);

		void CleanUp();

	private:
		TUniquePtr<FStaticMeshImporterImpl> Impl;
	};

}  // namespace GLTF
