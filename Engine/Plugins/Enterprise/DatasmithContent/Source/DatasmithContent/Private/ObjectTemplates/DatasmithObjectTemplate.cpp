// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "DatasmithAssetUserData.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "UObject/Object.h"
#include "GameFramework/Actor.h"


namespace
{

IInterface_AssetUserData* GetUserDataInterface(UObject* Outer)
{
#if WITH_EDITORONLY_DATA
	if (Outer && Outer->GetClass()->IsChildOf(AActor::StaticClass()))
	{
		// The root Component holds AssetUserData on behalf of the actor
		Outer = Cast<AActor>(Outer)->GetRootComponent();
	}

	if (!Outer || !Outer->GetClass()->ImplementsInterface(UInterface_AssetUserData::StaticClass()))
	{
		return nullptr;
	}

	return Cast< IInterface_AssetUserData >(Outer);
#else
	return nullptr;
#endif // #if WITH_EDITORONLY_DATA
}

}

bool FDatasmithObjectTemplateUtils::HasObjectTemplates(UObject* Outer)
{
#if WITH_EDITORONLY_DATA
	IInterface_AssetUserData* AssetUserDataInterface = GetUserDataInterface(Outer);
	if (!AssetUserDataInterface)
	{
		return false;
	}

	UDatasmithAssetUserData* UserData = AssetUserDataInterface->GetAssetUserData< UDatasmithAssetUserData >();

	return UserData != nullptr && UserData->ObjectTemplates.Num() > 0;
#else
	return false;
#endif // #if WITH_EDITORONLY_DATA
}


TMap< TSubclassOf< UDatasmithObjectTemplate >, UDatasmithObjectTemplate* >* FDatasmithObjectTemplateUtils::FindOrCreateObjectTemplates(UObject* Outer)
{
#if WITH_EDITORONLY_DATA
	IInterface_AssetUserData* AssetUserDataInterface = GetUserDataInterface(Outer);
	if (!AssetUserDataInterface)
	{
		return nullptr;
	}

	UDatasmithAssetUserData* UserData = AssetUserDataInterface->GetAssetUserData< UDatasmithAssetUserData >();

	if (!UserData)
	{
		EObjectFlags Flags = RF_Public /*| RF_Transactional*/; // RF_Transactional Disabled as is can cause a crash in the transaction system for blueprints

		if ( Outer->GetClass()->IsChildOf<AActor>() )
		{
			// The outer should never be an actor. (UE-70039)
			Outer = static_cast<AActor*>(Outer)->GetRootComponent();
		}

		UserData = NewObject< UDatasmithAssetUserData >(Outer, NAME_None, Flags);
		AssetUserDataInterface->AddAssetUserData(UserData);
	}

	return &UserData->ObjectTemplates;
#else
	return nullptr;
#endif // #if WITH_EDITORONLY_DATA
}

UDatasmithObjectTemplate* FDatasmithObjectTemplateUtils::GetObjectTemplate(UObject* Outer, TSubclassOf< UDatasmithObjectTemplate > Subclass)
{
#if WITH_EDITORONLY_DATA
	TMap< TSubclassOf< UDatasmithObjectTemplate >, UDatasmithObjectTemplate* >* ObjectTemplatesMap = FindOrCreateObjectTemplates(Outer);

	if (!ObjectTemplatesMap)
	{
		return nullptr;
	}

	UDatasmithObjectTemplate** ObjectTemplatePtr = ObjectTemplatesMap->Find(Subclass);

	return ObjectTemplatePtr ? *ObjectTemplatePtr : nullptr;
#else
	return nullptr;
#endif // #if WITH_EDITORONLY_DATA
}

void FDatasmithObjectTemplateUtils::SetObjectTemplate(UObject* Outer, UDatasmithObjectTemplate* ObjectTemplate)
{
#if WITH_EDITORONLY_DATA
	TMap< TSubclassOf< UDatasmithObjectTemplate >, UDatasmithObjectTemplate* >* ObjectTemplatesMap = FindOrCreateObjectTemplates(Outer);
	ensure(ObjectTemplatesMap);

	if (!ObjectTemplatesMap)
	{
		return;
	}

	ObjectTemplatesMap->FindOrAdd(ObjectTemplate->GetClass()) = ObjectTemplate;
#endif // #if WITH_EDITORONLY_DATA
}

TSet<FName> FDatasmithObjectTemplateUtils::ThreeWaySetMerge(const TSet<FName>& OldSet, const TSet<FName>& CurrentSet, const TSet<FName>& NewSet)
{
	TSet<FName> UserRemoved = OldSet.Difference(CurrentSet);
	TSet<FName> UserAdded = CurrentSet.Difference(OldSet);
	return NewSet.Union(UserAdded).Difference(UserRemoved);
}

bool FDatasmithObjectTemplateUtils::SetsEquals(const TSet<FName>& Left, const TSet<FName>& Right)
{
	return Left.Num() == Right.Num() && Left.Includes(Right);
}

