// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GLTFImportFactory.h"

#include "GLTFImporterContext.h"
#include "GLTFImporterModule.h"

#include "Engine/StaticMesh.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "AssetRegistryModule.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Engine/StaticMesh.h"
#include "IMessageLogListing.h"
#include "Logging/LogMacros.h"
#include "Logging/TokenizedMessage.h"
#include "MessageLogModule.h"
#include "PackageTools.h"

#define LOCTEXT_NAMESPACE "GLTFFactory"

namespace GLTFImporterImpl
{
	void ShowLogMessages(const TArray<GLTF::FLogMessage>& Messages)
	{
		if (Messages.Num() > 0)
		{
			FMessageLogModule&             MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
			TSharedRef<IMessageLogListing> LogListing       = (MessageLogModule.GetLogListing("LoadErrors"));
			LogListing->ClearMessages();
			for (const GLTF::FLogMessage& Error : Messages)
			{
				EMessageSeverity::Type Severity =
				    Error.Get<0>() == GLTF::EMessageSeverity::Error ? EMessageSeverity::Error : EMessageSeverity::Warning;
				LogListing->AddMessage(FTokenizedMessage::Create(Severity, FText::FromString(Error.Get<1>())));
			}
			MessageLogModule.OpenMessageLog("LoadErrors");
		}
	}
}

UGLTFImportFactory::UGLTFImportFactory(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , GLTFImporterModule(&IGLTFImporterModule::Get())
{
	bCreateNew    = false;
	bEditAfterNew = false;
	bEditorImport = true;   // binary / general file source
	bText         = false;  // text source

	SupportedClass = UStaticMesh::StaticClass();

	Formats.Add(TEXT("gltf;GL Transmission Format"));
	Formats.Add(TEXT("glb;GL Transmission Format (Binary)"));
}

UObject* UGLTFImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename,
                                               const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Parms);

	Warn->Log(Filename);

	FGLTFImporterContext& Context = GLTFImporterModule->GetImporterContext();

	UObject* Object = nullptr;
	if (Context.OpenFile(Filename))
	{
		const FString AssetName      = Context.Asset.GetName(Filename);
		const FString NewPackageName = UPackageTools::SanitizePackageName(*(FPaths::GetPath(InParent->GetName()) / AssetName));
		UObject*      ParentPackage  = NewPackageName == InParent->GetName() ? InParent : CreatePackage(nullptr, *NewPackageName);

		const TArray<UStaticMesh*>& CreatedMeshes = Context.ImportMeshes(ParentPackage, Flags, false);

		UpdateMeshes();

		if (CreatedMeshes.Num() == 1)
		{
			Object = CreatedMeshes[0];
		}
		else if (CreatedMeshes.Num() != 0)
		{
			Object = CreatedMeshes[0]->GetOutermost();
		}
	}

	FEditorDelegates::OnAssetPostImport.Broadcast(this, Object);

	GLTFImporterImpl::ShowLogMessages(Context.GetLogMessages());

	return Object;
}

void UGLTFImportFactory::CleanUp()
{
	// cleanup any resources/buffers

	FGLTFImporterContext& Context = GLTFImporterModule->GetImporterContext();
	Context.StaticMeshImporter.CleanUp();

	Context.Asset.Clear(8 * 1024, 512);
}

void UGLTFImportFactory::UpdateMeshes() const
{
	FGLTFImporterContext&       Context = GLTFImporterModule->GetImporterContext();
	const TArray<UStaticMesh*>& Meshes  = Context.StaticMeshImporter.GetMeshes();

	int32 MeshIndex = 0;
	for (UStaticMesh* StaticMesh : Meshes)
	{
		const GLTF::FMesh& GltfMesh = Context.Asset.Meshes[MeshIndex++];

		StaticMesh->MarkPackageDirty();
		StaticMesh->PostEditChange();
		FAssetRegistryModule::AssetCreated(StaticMesh);
	}
}

#undef LOCTEXT_NAMESPACE
