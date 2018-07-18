// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithAssetUserData.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "UObject/Object.h"

#include "DatasmithObjectTemplate.generated.h"

UCLASS(abstract)
class DATASMITHCONTENT_API UDatasmithObjectTemplate : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Applies the object template to a Destination object
	 *
	 * @param Destination	The object to apply this template to
	 * @param bForce		Force the application of the template on all properties, even if they were changed from the previous template values.
	 */
	virtual void Apply( UObject* Destination, bool bForce = false ) {}

	/**
	 * Fills this template properties with the values from the Source object.
	 */
	virtual void Load( const UObject* Source ) {}

	/**
	 * Returns if this template equals another template of the same type.
	 */
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const { return false; }
};

// Sets Destination->MemberName with the value of MemberName only if PreviousTemplate is null or has the same value for MemberName as the Destination.
// The goal is to set a new value only if it wasn't changed (overriden) in the Destination.
#define DATASMITHOBJECTTEMPLATE_CONDITIONALSET(MemberName, Destination, PreviousTemplate) \
	if ( !PreviousTemplate || Destination->MemberName == PreviousTemplate->MemberName ) \
	{ \
		Destination->MemberName = MemberName; \
	}

struct FDatasmithObjectTemplateUtils
{
	inline static bool HasObjectTemplates( UObject* Outer )
	{
#if WITH_EDITORONLY_DATA
		if ( !Outer )
		{
			return false;
		}

		UDatasmithAssetUserData* UserData = nullptr;

		if ( Outer->GetClass()->ImplementsInterface( UInterface_AssetUserData::StaticClass() ) )
		{
			IInterface_AssetUserData* AssetUserDataInterface = Cast< IInterface_AssetUserData >( Outer );

			UserData = AssetUserDataInterface->GetAssetUserData< UDatasmithAssetUserData >();
		}

		return UserData != nullptr && UserData->ObjectTemplates.Num() > 0;
#else
		return false;
#endif // #if WITH_EDITORONLY_DATA
	}

	inline static TMap< TSubclassOf< UDatasmithObjectTemplate >, UDatasmithObjectTemplate* >* FindOrCreateObjectTemplates( UObject* Outer )
	{
#if WITH_EDITORONLY_DATA
		if ( !Outer )
		{
			return nullptr;
		}

		UDatasmithAssetUserData* UserData = nullptr;

		if ( Outer->GetClass()->ImplementsInterface( UInterface_AssetUserData::StaticClass() ) )
		{
			IInterface_AssetUserData* AssetUserDataInterface = Cast< IInterface_AssetUserData >( Outer );

			UserData = AssetUserDataInterface->GetAssetUserData< UDatasmithAssetUserData >();

			if ( !UserData )
			{
				UserData = NewObject< UDatasmithAssetUserData >( Outer, NAME_None );
				AssetUserDataInterface->AddAssetUserData( UserData );
			}
		}

		if ( !UserData )
		{
			return nullptr;
		}

		return &UserData->ObjectTemplates;
#else
		return nullptr;
#endif // #if WITH_EDITORONLY_DATA
	}

	inline static UDatasmithObjectTemplate* GetObjectTemplate( UObject* Outer, TSubclassOf< UDatasmithObjectTemplate > Subclass )
	{
#if WITH_EDITORONLY_DATA
		TMap< TSubclassOf< UDatasmithObjectTemplate >, UDatasmithObjectTemplate* >* ObjectTemplatesMap = FindOrCreateObjectTemplates( Outer );

		if ( !ObjectTemplatesMap )
		{
			return nullptr;
		}

		UDatasmithObjectTemplate** ObjectTemplatePtr = ObjectTemplatesMap->Find( Subclass );

		return ObjectTemplatePtr ? *ObjectTemplatePtr : nullptr;
#else
		return nullptr;
#endif // #if WITH_EDITORONLY_DATA
	}

	template < typename T >
	static T* GetObjectTemplate( UObject* Outer )
	{
		return Cast< T >( GetObjectTemplate( Outer, T::StaticClass() ) );
	}

	inline static void SetObjectTemplate( UObject* Outer, UDatasmithObjectTemplate* ObjectTemplate )
	{
#if WITH_EDITORONLY_DATA
		TMap< TSubclassOf< UDatasmithObjectTemplate >, UDatasmithObjectTemplate* >* ObjectTemplatesMap = FindOrCreateObjectTemplates( Outer );
		ensure( ObjectTemplatesMap );

		if ( !ObjectTemplatesMap )
		{
			return;
		}

		ObjectTemplatesMap->FindOrAdd( ObjectTemplate->GetClass() ) = ObjectTemplate;
#endif // #if WITH_EDITORONLY_DATA
	}
};
