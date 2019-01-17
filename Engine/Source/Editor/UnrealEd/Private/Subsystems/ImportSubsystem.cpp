// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Subsystems/ImportSubsystem.h"

#include "Editor.h"

UImportSubsystem::UImportSubsystem()
	: UEditorSubsystem()
{

}

void UImportSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{

}

void UImportSubsystem::Deinitialize()
{

}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

void UImportSubsystem::BroadcastAssetPreImport(UFactory* InFactory, UClass* InClass, UObject* InParent, const FName& Name, const TCHAR* Type)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(InFactory, InClass, InParent, Name, Type);
	OnAssetPreImport.Broadcast(InFactory, InClass, InParent, Name, Type);
	OnAssetPreImport_BP.Broadcast(InFactory, InClass, InParent, Name, FString(Type));
}

void UImportSubsystem::BroadcastAssetPostImport(UFactory* InFactory, UObject* InCreatedObject)
{
	FEditorDelegates::OnAssetPostImport.Broadcast(InFactory, InCreatedObject);
	OnAssetPostImport.Broadcast(InFactory, InCreatedObject);
	OnAssetPostImport_BP.Broadcast(InFactory, InCreatedObject);
}

void UImportSubsystem::BroadcastAssetReimport(UObject* InCreatedObject)
{
	FEditorDelegates::OnAssetReimport.Broadcast(InCreatedObject);
	OnAssetReimport.Broadcast(InCreatedObject);
	OnAssetReimport_BP.Broadcast(InCreatedObject);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS