// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UObject/TextProperty.h"
#include "Internationalization/ITextData.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/Package.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Internationalization/StringTableRegistry.h"
#include "Internationalization/StringTableCore.h"
#include "Serialization/ArchiveUObjectFromStructuredArchive.h"

EConvertFromTypeResult UTextProperty::ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct)
{
	// Convert serialized string to text.
	if (Tag.Type==NAME_StrProperty)
	{
		FString str;
		Slot << str;
		FText Text = FText::FromString(str);
		Text.TextData->PersistText();
		Text.Flags |= ETextFlag::ConvertedProperty;
		SetPropertyValue_InContainer(Data, Text, Tag.ArrayIndex);
		return EConvertFromTypeResult::Converted;
	}

	// Convert serialized name to text.
	if (Tag.Type==NAME_NameProperty)
	{
		FName Name;
		Slot << Name;
		FText Text = FText::FromName(Name);
		Text.Flags |= ETextFlag::ConvertedProperty;
		SetPropertyValue_InContainer(Data, Text, Tag.ArrayIndex);
		return EConvertFromTypeResult::Converted;
	}

	return EConvertFromTypeResult::UseSerializeItem;
}

bool UTextProperty::Identical_Implementation(const FText& ValueA, const FText& ValueB, uint32 PortFlags)
{
	// A culture variant text is never equal to a culture invariant text
	// A transient text is never equal to a non-transient text
	// An empty text is never equal to a non-empty text
	if (ValueA.IsCultureInvariant() != ValueB.IsCultureInvariant() || ValueA.IsTransient() != ValueB.IsTransient() || ValueA.IsEmpty() != ValueB.IsEmpty())
	{
		return false;
	}

	// If both texts are empty (see the above check), then they must be equal
	if (ValueA.IsEmpty())
	{
		return true;
	}

	// If both texts share the same pointer, then they must be equal
	if (ValueA.IdenticalTo(ValueB))
	{
		// Placeholder string table entries will have the same pointer, but should only be considered equal if they're using the same string table and key
		if (ValueA.IsFromStringTable() && FTextInspector::GetSourceString(ValueA) == &FStringTableEntry::GetPlaceholderSourceString())
		{
			FName ValueAStringTableId;
			FString ValueAStringTableEntryKey;
			FTextInspector::GetTableIdAndKey(ValueA, ValueAStringTableId, ValueAStringTableEntryKey);

			FName ValueBStringTableId;
			FString ValueBStringTableEntryKey;
			FTextInspector::GetTableIdAndKey(ValueB, ValueBStringTableId, ValueBStringTableEntryKey);

			return ValueAStringTableId == ValueBStringTableId && ValueAStringTableEntryKey.Equals(ValueBStringTableEntryKey, ESearchCase::CaseSensitive);
		}

		// Otherwise they're equal
		return true;
	}

	// We compare the display strings in editor (as we author in the native language)
	// We compare the display string for culture invariant and transient texts as they don't have an identity
	if (GIsEditor || ValueA.IsCultureInvariant() || ValueA.IsTransient())
	{
		return FTextInspector::GetDisplayString(ValueA).Equals(FTextInspector::GetDisplayString(ValueB), ESearchCase::CaseSensitive);
	}
	
	// If we got this far then the texts don't share the same pointer, which means that they can't share the same identity
	return false;
}

bool UTextProperty::Identical( const void* A, const void* B, uint32 PortFlags ) const
{
	const TCppType ValueA = GetPropertyValue(A);
	if ( B )
	{
		const TCppType ValueB = GetPropertyValue(B);
		return Identical_Implementation(ValueA, ValueB, PortFlags);
	}

	return FTextInspector::GetDisplayString(ValueA).IsEmpty();
}

void UTextProperty::SerializeItem(FStructuredArchive::FSlot Slot, void* Value, void const* Defaults) const
{
	TCppType* TextPtr = GetPropertyValuePtr(Value);
	Slot << *TextPtr;
}

void UTextProperty::ExportTextItem( FString& ValueStr, const void* PropertyValue, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	const FText& TextValue = GetPropertyValue(PropertyValue);

	if (PortFlags & PPF_ExportCpp)
	{
		ValueStr += GenerateCppCodeForTextValue(TextValue, FString());
	}
	else if (PortFlags & PPF_PropertyWindow)
	{
		if (PortFlags & PPF_Delimited)
		{
			ValueStr += TEXT("\"");
			ValueStr += TextValue.ToString();
			ValueStr += TEXT("\"");
		}
		else
		{
			ValueStr += TextValue.ToString();
		}
	}
	else
	{
		FTextStringHelper::WriteToBuffer(ValueStr, TextValue, !!(PortFlags & PPF_Delimited));
	}
}

const TCHAR* UTextProperty::ImportText_Internal( const TCHAR* Buffer, void* Data, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText ) const
{
	FText* TextPtr = GetPropertyValuePtr(Data);

	FString TextNamespace;
	if (Parent && HasAnyPropertyFlags(CPF_Config))
	{
		const bool bPerObject = UsesPerObjectConfig(Parent);
		if (bPerObject)
		{
			FString PathNameString;
			UPackage* ParentOutermost = Parent->GetOutermost();
			if (ParentOutermost == GetTransientPackage())
			{
				PathNameString = Parent->GetName();
			}
			else
			{
				PathNameString = Parent->GetPathName(ParentOutermost);
			}
			TextNamespace = PathNameString + TEXT(" ") + Parent->GetClass()->GetName();

			Parent->OverridePerObjectConfigSection(TextNamespace);
		}
		else
		{
			const bool bGlobalConfig = HasAnyPropertyFlags(CPF_GlobalConfig);
			UClass* ConfigClass = bGlobalConfig ? GetOwnerClass() : Parent->GetClass();
			TextNamespace = ConfigClass->GetPathName();
		}
	}

	FString PackageNamespace;
#if USE_STABLE_LOCALIZATION_KEYS
	if (GIsEditor && !(PortFlags & (PPF_DuplicateVerbatim | PPF_DuplicateForPIE)))
	{
		PackageNamespace = TextNamespaceUtil::EnsurePackageNamespace(Parent);
	}
#endif // USE_STABLE_LOCALIZATION_KEYS

	return FTextStringHelper::ReadFromBuffer(Buffer, *TextPtr, *TextNamespace, *PackageNamespace, !!(PortFlags & PPF_Delimited));
}

FString UTextProperty::GenerateCppCodeForTextValue(const FText& InValue, const FString& Indent)
{
	FString CppCode;

	if (InValue.IsEmpty())
	{
		CppCode += TEXT("FText::GetEmpty()");
	}
	else if (InValue.IsCultureInvariant())
	{
		const FString& StringValue = FTextInspector::GetDisplayString(InValue);

		// Produces FText::AsCultureInvariant(TEXT("..."))
		CppCode += TEXT("FText::AsCultureInvariant(\n");
		CppCode += UStrProperty::ExportCppHardcodedText(StringValue, Indent + TEXT("\t\t"));
		CppCode += TEXT("\t)");
	}
	else
	{
		FString ExportedText;
		FTextStringHelper::WriteToBuffer(ExportedText, InValue);

		if (FTextStringHelper::IsComplexText(*ExportedText))
		{
			// Produces FTextStringHelper::CreateFromBuffer(TEXT("..."))
			CppCode += TEXT("FTextStringHelper::CreateFromBuffer(\n");
			CppCode += UStrProperty::ExportCppHardcodedText(ExportedText, Indent + TEXT("\t\t"));
			CppCode += Indent;
			CppCode += TEXT("\t)");
		}
		else
		{
			// Produces FText::FromString(TEXT("..."))
			CppCode += TEXT("FText::FromString(\n");
			CppCode += UStrProperty::ExportCppHardcodedText(ExportedText, Indent + TEXT("\t\t"));
			CppCode += TEXT("\t)");
		}
	}

	return CppCode;
}

FString UTextProperty::GetCPPTypeForwardDeclaration() const
{
	return FString();
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UTextProperty, UProperty,
{
}
);
