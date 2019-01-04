// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/StabilizeLocalizationKeys.h"
#include "UObject/TextProperty.h"

#if WITH_EDITOR

void StabilizeLocalizationKeys::StabilizeLocalizationKeysForProperty(UProperty* InProp, void* InPropData, const FString& InNamespace, const FString& InKeyRoot, const bool bAppendPropertyNameToKey)
{
	auto ShouldStabilizeInnerProperty = [](UProperty* InnerProperty) -> bool
	{
		return InnerProperty->IsA<UTextProperty>() || InnerProperty->IsA<UStructProperty>();
	};

	auto GetPropertyName = [InProp]() -> FString
	{
		// UserDefinedStructs should use the display name rather than the raw name, since the raw name doesn't match what the user generally sees or entered as the property name
		for (UStruct* PropOwnerStruct = InProp->GetOwnerStruct(); PropOwnerStruct; PropOwnerStruct = PropOwnerStruct->GetSuperStruct())
		{
			static const FName UserDefinedStructName = TEXT("UserDefinedStruct");
			if (PropOwnerStruct->GetClass()->GetFName() == UserDefinedStructName)
			{
				static const FName DisplayNameKey = TEXT("DisplayName");
				return InProp->HasMetaData(DisplayNameKey) ? InProp->GetMetaData(DisplayNameKey) : InProp->GetName();
			}
		}
		return InProp->GetName();
	};

	const FString PropKeyRoot = bAppendPropertyNameToKey ? FString::Printf(TEXT("%s_%s"), *InKeyRoot, *GetPropertyName()) : InKeyRoot;

	if (UTextProperty* TextProp = Cast<UTextProperty>(InProp))
	{
		for (int32 ArrIndex = 0; ArrIndex < TextProp->ArrayDim; ++ArrIndex)
		{
			void* PropValueData = ((uint8*)InPropData) + (TextProp->ElementSize * ArrIndex);

			FText* TextValuePtr = TextProp->GetPropertyValuePtr(PropValueData);
			check(TextValuePtr);

			if (TextValuePtr->IsInitializedFromString())
			{
				if (TextProp->ArrayDim > 1)
				{
					*TextValuePtr = FText::ChangeKey(InNamespace, FString::Printf(TEXT("%s_Index%d"), *PropKeyRoot, ArrIndex), *TextValuePtr);
				}
				else
				{
					*TextValuePtr = FText::ChangeKey(InNamespace, PropKeyRoot, *TextValuePtr);
				}
			}
		}
		return;
	}

	if (UStructProperty* StructProp = Cast<UStructProperty>(InProp))
	{
		for (int32 ArrIndex = 0; ArrIndex < StructProp->ArrayDim; ++ArrIndex)
		{
			void* PropValueData = ((uint8*)InPropData) + (StructProp->ElementSize * ArrIndex);

			if (StructProp->ArrayDim > 1)
			{
				StabilizeLocalizationKeysForStruct(
					StructProp->Struct,
					PropValueData,
					InNamespace,
					FString::Printf(TEXT("%s_Index%d"), *PropKeyRoot, ArrIndex)
					);
			}
			else
			{
				StabilizeLocalizationKeysForStruct(
					StructProp->Struct,
					PropValueData,
					InNamespace,
					PropKeyRoot
					);
			}
		}
		return;
	}

	if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(InProp))
	{
		if (ShouldStabilizeInnerProperty(ArrayProp->Inner))
		{
			FScriptArrayHelper ScriptArrayHelper(ArrayProp, InPropData);

			const int32 ElementCount = ScriptArrayHelper.Num();
			for (int32 ArrIndex = 0; ArrIndex < ElementCount; ++ArrIndex)
			{
				StabilizeLocalizationKeysForProperty(
					ArrayProp->Inner,
					ScriptArrayHelper.GetRawPtr(ArrIndex),
					InNamespace,
					FString::Printf(TEXT("%s_Index%d"), *PropKeyRoot, ArrIndex),
					/*bAppendPropertyNameToKey*/false
					);
			}
		}
		return;
	}

	if (USetProperty* SetProp = Cast<USetProperty>(InProp))
	{
		if (ShouldStabilizeInnerProperty(SetProp->ElementProp))
		{
			FScriptSetHelper ScriptSetHelper(SetProp, InPropData);

			const int32 ElementCount = ScriptSetHelper.Num();
			for (int32 RawIndex = 0, ElementIndex = 0; ElementIndex < ElementCount; ++RawIndex)
			{
				if (!ScriptSetHelper.IsValidIndex(RawIndex))
				{
					continue;
				}

				StabilizeLocalizationKeysForProperty(
					SetProp->ElementProp,
					ScriptSetHelper.GetElementPtr(RawIndex),
					InNamespace,
					FString::Printf(TEXT("%s_Index%d"), *PropKeyRoot, ElementIndex),
					/*bAppendPropertyNameToKey*/false
					);

				++ElementIndex;
			}

			ScriptSetHelper.Rehash();
		}
		return;
	}

	if (UMapProperty* MapProp = Cast<UMapProperty>(InProp))
	{
		if (ShouldStabilizeInnerProperty(MapProp->KeyProp) || ShouldStabilizeInnerProperty(MapProp->ValueProp))
		{
			FScriptMapHelper ScriptMapHelper(MapProp, InPropData);

			const int32 ElementCount = ScriptMapHelper.Num();
			for (int32 RawIndex = 0, ElementIndex = 0; ElementIndex < ElementCount; ++RawIndex)
			{
				if (!ScriptMapHelper.IsValidIndex(RawIndex))
				{
					continue;
				}

				if (ShouldStabilizeInnerProperty(MapProp->KeyProp))
				{
					StabilizeLocalizationKeysForProperty(
						MapProp->KeyProp,
						ScriptMapHelper.GetKeyPtr(RawIndex),
						InNamespace,
						FString::Printf(TEXT("%s_KeyIndex%d"), *PropKeyRoot, ElementIndex),
						/*bAppendPropertyNameToKey*/false
						);
				}

				if (ShouldStabilizeInnerProperty(MapProp->ValueProp))
				{
					StabilizeLocalizationKeysForProperty(
						MapProp->ValueProp,
						ScriptMapHelper.GetValuePtr(RawIndex),
						InNamespace,
						FString::Printf(TEXT("%s_ValueIndex%d"), *PropKeyRoot, ElementIndex),
						/*bAppendPropertyNameToKey*/false
						);
				}

				++ElementIndex;
			}

			if (ShouldStabilizeInnerProperty(MapProp->KeyProp))
			{
				ScriptMapHelper.Rehash();
			}
		}
		return;
	}
}

void StabilizeLocalizationKeys::StabilizeLocalizationKeysForStruct(UStruct* InStruct, void* InStructData, const FString& InNamespace, const FString& InKeyRoot)
{
	for (TFieldIterator<UProperty> It(InStruct); It; ++It)
	{
		StabilizeLocalizationKeysForProperty(*It, It->ContainerPtrToValuePtr<void>(InStructData), InNamespace, InKeyRoot);
	}
}

#endif	// WITH_EDITOR
