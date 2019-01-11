// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GLTFAsset.h"
#include "GLTFLogger.h"
#include "GLTFReader.h"
#include "GLTFStaticMeshImporter.h"

#include "Misc/Paths.h"

struct FGLTFImporterContext
{
	mutable TArray<GLTF::FLogMessage> LogMessages;
	GLTF::FFileReader                 Reader;
	GLTF::FAsset                      Asset;
	GLTF::FStaticMeshImporter         StaticMeshImporter;

	bool OpenFile(const FString& FilePath)
	{
		LogMessages.Empty();

		Reader.ReadFile(FilePath, false, true, Asset);

		auto Found = Reader.GetLogMessages().FindByPredicate([](const GLTF::FLogMessage& Message) { return Message.Get<0>() == GLTF::EMessageSeverity::Error; });
		if (Found)
		{
			return false;
		}
		check(Asset.ValidationCheck() == GLTF::FAsset::Valid);

		// check extensions supported
		static const TArray<GLTF::EExtension> SupportedExtensions = {GLTF::EExtension::KHR_MaterialsPbrSpecularGlossiness,
		                                                             GLTF::EExtension::KHR_MaterialsUnlit, GLTF::EExtension::KHR_LightsPunctual};
		for (auto Extension : Asset.ExtensionsUsed)
		{
			if (SupportedExtensions.Find(Extension) == INDEX_NONE)
			{
				LogMessages.Emplace(GLTF::EMessageSeverity::Warning,
				                    FString::Printf(TEXT("Extension is not supported: %s"), GLTF::ToString(Extension)));
			}
		}

		Asset.GenerateNames(FPaths::GetBaseFilename(FilePath));

		return true;
	}

	const TArray<UStaticMesh*>& ImportMeshes(UObject* ParentPackage, EObjectFlags Flags, bool bApplyPostEditChange)
	{
		return StaticMeshImporter.ImportMeshes(Asset, ParentPackage, Flags, bApplyPostEditChange);
	}

	const TArray<GLTF::FLogMessage>& GetLogMessages() const
	{
		LogMessages.Append(Reader.GetLogMessages());
		LogMessages.Append(StaticMeshImporter.GetLogMessages());
		return LogMessages;
	}
};
