// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ObjectTemplates/DatasmithMaterialInstanceTemplate.h"

#include "Engine/Texture.h"
#include "Materials/MaterialInstanceConstant.h"

namespace DatasmithMaterialInstanceTemplateImpl
{
#if WITH_EDITORONLY_DATA
	void Apply( UMaterialInstanceConstant* MaterialInstance, FName ParameterName, float Value, TOptional< float > PreviousValue )
	{
		float InstanceValue = 0.f;
		MaterialInstance->GetScalarParameterValue( ParameterName, InstanceValue );

		if ( PreviousValue && !FMath::IsNearlyEqual( InstanceValue, PreviousValue.GetValue() ) )
		{
			return;
		}

		if ( !FMath::IsNearlyEqual( Value, InstanceValue ) )
		{
			MaterialInstance->SetScalarParameterValueEditorOnly( ParameterName, Value );
		}
	}

	void Apply( UMaterialInstanceConstant* MaterialInstance, FName ParameterName, FLinearColor Value, TOptional< FLinearColor > PreviousValue )
	{
		FLinearColor InstanceValue = FLinearColor::White;
		MaterialInstance->GetVectorParameterValue( ParameterName, InstanceValue );

		if ( PreviousValue && !InstanceValue.Equals( PreviousValue.GetValue() ) )
		{
			return;
		}

		if ( !Value.Equals( InstanceValue ) )
		{
			MaterialInstance->SetVectorParameterValueEditorOnly( ParameterName, Value );
		}
	}

	void Apply( UMaterialInstanceConstant* MaterialInstance, FName ParameterName, TSoftObjectPtr< UTexture > Value, TOptional< TSoftObjectPtr< UTexture > > PreviousValue )
	{
		UTexture* InstanceValue = nullptr;
		MaterialInstance->GetTextureParameterValue( ParameterName, InstanceValue );

		if ( PreviousValue && InstanceValue != PreviousValue.GetValue() )
		{
			return;
		}

		if ( Value != InstanceValue )
		{
			MaterialInstance->SetTextureParameterValueEditorOnly( ParameterName, Value.Get() );
		}
	}
#endif // #if WITH_EDITORONLY_DATA

	template< typename MapType >
	bool MapEquals( MapType A, MapType B )
	{
		bool bEquals = ( A.Num() == B.Num() );

		for ( typename MapType::TConstIterator It = A.CreateConstIterator(); It && bEquals; ++It )
		{
			const auto* BValue = B.Find( It->Key );

			if ( BValue )
			{
				bEquals = ( It->Value == *BValue );
			}
			else
			{
				bEquals = false;
			}
		}

		return bEquals;
	}
}

void FDatasmithStaticParameterSetTemplate::Apply( UMaterialInstanceConstant* Destination, FDatasmithStaticParameterSetTemplate* PreviousTemplate )
{
#if WITH_EDITORONLY_DATA
	bool bNeedsUpdatePermutations = false;

	FStaticParameterSet DestinationStaticParameters;
	Destination->GetStaticParameterValues( DestinationStaticParameters );

	for ( TMap< FName, bool >::TConstIterator It = StaticSwitchParameters.CreateConstIterator(); It; ++It )
	{
		TOptional< bool > PreviousValue;

		if ( PreviousTemplate )
		{
			bool* PreviousValuePtr = PreviousTemplate->StaticSwitchParameters.Find( It->Key );

			if ( PreviousValuePtr )
			{
				PreviousValue = *PreviousValuePtr;
			}
		}

		for ( FStaticSwitchParameter& DestinationSwitchParameter : DestinationStaticParameters.StaticSwitchParameters )
		{
			if ( DestinationSwitchParameter.ParameterInfo.Name == It->Key )
			{
				if ( ( !PreviousValue || PreviousValue.GetValue() == DestinationSwitchParameter.Value ) && DestinationSwitchParameter.Value != It->Value )
				{
					DestinationSwitchParameter.Value = It->Value;
					DestinationSwitchParameter.bOverride = true;
					bNeedsUpdatePermutations = true;
				}

				break;
			}
		}
	}

	if ( bNeedsUpdatePermutations )
	{
		Destination->UpdateStaticPermutation( DestinationStaticParameters );
	}
#endif // #if WITH_EDITORONLY_DATA
}

void FDatasmithStaticParameterSetTemplate::Load( const UMaterialInstanceConstant& Source )
{
#if WITH_EDITORONLY_DATA
	FStaticParameterSet SourceStaticParameters;
	const_cast< UMaterialInstanceConstant& >( Source ).GetStaticParameterValues( SourceStaticParameters );

	StaticSwitchParameters.Empty( SourceStaticParameters.StaticSwitchParameters.Num() );

	for ( const FStaticSwitchParameter& SourceSwitch : SourceStaticParameters.StaticSwitchParameters )
	{
		if ( SourceSwitch.bOverride )
		{
			StaticSwitchParameters.Add( SourceSwitch.ParameterInfo.Name, SourceSwitch.Value );
		}
	}
#endif // #if WITH_EDITORONLY_DATA
}

bool FDatasmithStaticParameterSetTemplate::Equals( const FDatasmithStaticParameterSetTemplate& Other ) const
{
	return DatasmithMaterialInstanceTemplateImpl::MapEquals( StaticSwitchParameters, Other.StaticSwitchParameters );
}

void UDatasmithMaterialInstanceTemplate::Apply( UObject* Destination, bool bForce )
{
#if WITH_EDITORONLY_DATA
	UMaterialInstanceConstant* MaterialInstance = Cast< UMaterialInstanceConstant >( Destination );

	if ( !MaterialInstance )
	{
		return;
	}

	UDatasmithMaterialInstanceTemplate* PreviousTemplate = !bForce ? FDatasmithObjectTemplateUtils::GetObjectTemplate< UDatasmithMaterialInstanceTemplate >( MaterialInstance ) : nullptr;

	if ( !PreviousTemplate )
	{
		MaterialInstance->ClearParameterValuesEditorOnly(); // If we're not applying a delta (changes vs previous template), we start with a clean slate
	}

	for ( TMap< FName, float >::TConstIterator It = ScalarParameterValues.CreateConstIterator(); It; ++It )
	{
		TOptional< float > PreviousValue;

		if ( PreviousTemplate )
		{
			float* PreviousValuePtr = PreviousTemplate->ScalarParameterValues.Find( It->Key );

			if ( PreviousValuePtr )
			{
				PreviousValue = *PreviousValuePtr;
			}
		}

		DatasmithMaterialInstanceTemplateImpl::Apply( MaterialInstance, It->Key, It->Value, PreviousValue );
	}

	for ( TMap< FName, FLinearColor >::TConstIterator It = VectorParameterValues.CreateConstIterator(); It; ++It )
	{
		TOptional< FLinearColor > PreviousValue;

		if ( PreviousTemplate )
		{
			FLinearColor* PreviousValuePtr = PreviousTemplate->VectorParameterValues.Find( It->Key );

			if ( PreviousValuePtr )
			{
				PreviousValue = *PreviousValuePtr;
			}
		}

		DatasmithMaterialInstanceTemplateImpl::Apply( MaterialInstance, It->Key, It->Value, PreviousValue );
	}

	for ( TMap< FName, TSoftObjectPtr< UTexture > >::TConstIterator It = TextureParameterValues.CreateConstIterator(); It; ++It )
	{
		TOptional< TSoftObjectPtr< UTexture > > PreviousValue;

		if ( PreviousTemplate )
		{
			TSoftObjectPtr< UTexture >* PreviousValuePtr = PreviousTemplate->TextureParameterValues.Find( It->Key );

			if ( PreviousValuePtr )
			{
				PreviousValue = *PreviousValuePtr;
			}
		}

		DatasmithMaterialInstanceTemplateImpl::Apply( MaterialInstance, It->Key, It->Value, PreviousValue );
	}

	StaticParameters.Apply( MaterialInstance, PreviousTemplate ? &PreviousTemplate->StaticParameters : nullptr );

	FDatasmithObjectTemplateUtils::SetObjectTemplate( MaterialInstance, this );
#endif // #if WITH_EDITORONLY_DATA
}

void UDatasmithMaterialInstanceTemplate::Load( const UObject* Source )
{
#if WITH_EDITORONLY_DATA
	const UMaterialInstanceConstant* MaterialInstance = Cast< UMaterialInstanceConstant >( Source );

	if ( !MaterialInstance )
	{
		return;
	}

	// Scalar
	ScalarParameterValues.Empty( MaterialInstance->ScalarParameterValues.Num() );

	for ( const FScalarParameterValue& ScalarParameterValue : MaterialInstance->ScalarParameterValues )
	{
		float Value;
		if ( MaterialInstance->GetScalarParameterValue( ScalarParameterValue.ParameterInfo.Name, Value, true ) )
		{
			ScalarParameterValues.Add( ScalarParameterValue.ParameterInfo.Name, Value );
		}
	}

	// Vector
	VectorParameterValues.Empty( MaterialInstance->VectorParameterValues.Num() );

	for ( const FVectorParameterValue& VectorParameterValue : MaterialInstance->VectorParameterValues )
	{
		FLinearColor Value;
		if ( MaterialInstance->GetVectorParameterValue( VectorParameterValue.ParameterInfo.Name, Value, true ) )
		{
			VectorParameterValues.Add( VectorParameterValue.ParameterInfo.Name, Value );
		}
	}

	// Texture
	TextureParameterValues.Empty( MaterialInstance->TextureParameterValues.Num() );

	for ( const FTextureParameterValue& TextureParameterValue : MaterialInstance->TextureParameterValues )
	{
		UTexture* Value;
		if ( MaterialInstance->GetTextureParameterValue( TextureParameterValue.ParameterInfo.Name, Value, true ) )
		{
			TextureParameterValues.Add( TextureParameterValue.ParameterInfo.Name, Value );
		}
	}

	StaticParameters.Load( *MaterialInstance );
#endif // #if WITH_EDITORONLY_DATA
}

bool UDatasmithMaterialInstanceTemplate::Equals( const UDatasmithObjectTemplate* Other ) const
{
	const UDatasmithMaterialInstanceTemplate* TypedOther = Cast< UDatasmithMaterialInstanceTemplate >( Other );

	if ( !TypedOther )
	{
		return false;
	}

	bool bEquals = DatasmithMaterialInstanceTemplateImpl::MapEquals( ScalarParameterValues, TypedOther->ScalarParameterValues );
	bEquals = bEquals && DatasmithMaterialInstanceTemplateImpl::MapEquals( VectorParameterValues, TypedOther->VectorParameterValues );
	bEquals = bEquals && DatasmithMaterialInstanceTemplateImpl::MapEquals( TextureParameterValues, TypedOther->TextureParameterValues );

	bEquals = bEquals && StaticParameters.Equals( TypedOther->StaticParameters );

	return bEquals;
}