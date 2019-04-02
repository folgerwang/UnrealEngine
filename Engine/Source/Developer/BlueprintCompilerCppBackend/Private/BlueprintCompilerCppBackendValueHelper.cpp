// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"
#include "Misc/PackageName.h"
#include "Engine/Blueprint.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/LatentActionManager.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/TimelineTemplate.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "EdGraphSchema_K2.h"
#include "KismetCompiler.h"
#include "BlueprintCompilerCppBackend.h"
#include "BlueprintCompilerCppBackendGatherDependencies.h"
#include "IBlueprintCompilerCppBackendModule.h"
#include "BlueprintCompilerCppBackendUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Engine/SCS_Node.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "Blueprint/BlueprintSupport.h"

void FEmitDefaultValueHelper::OuterGenerate(FEmitterLocalContext& Context
	, const UProperty* Property
	, const FString& OuterPath
	, const uint8* DataContainer
	, const uint8* OptionalDefaultDataContainer
	, EPropertyAccessOperator AccessOperator
	, bool bAllowProtected)
{
	check(Property);

	if (Property->HasAnyPropertyFlags(CPF_EditorOnly | CPF_Transient))
	{
		UE_LOG(LogK2Compiler, Verbose, TEXT("FEmitDefaultValueHelper Skip EditorOnly or Transient property: %s"), *Property->GetPathName());
		return;
	}

	if (Property->IsA<UDelegateProperty>() || Property->IsA<UMulticastDelegateProperty>())
	{
		UE_LOG(LogK2Compiler, Verbose, TEXT("FEmitDefaultValueHelper delegate property: %s"), *Property->GetPathName());
		return;
	}

	// Check if this is an object property and cache the result.
	const UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property);

	for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)
	{
		if (!OptionalDefaultDataContainer
			|| Property->HasAnyPropertyFlags(CPF_Config)
			|| !Property->Identical_InContainer(DataContainer, OptionalDefaultDataContainer, ArrayIndex))
		{
			FNativizationSummaryHelper::PropertyUsed(Context.GetCurrentlyGeneratedClass(), Property);

			FString PathToMember;
			UBlueprintGeneratedClass* PropertyOwnerAsBPGC = Cast<UBlueprintGeneratedClass>(Property->GetOwnerClass());
			UScriptStruct* PropertyOwnerAsScriptStruct = Cast<UScriptStruct>(Property->GetOwnerStruct());
			const bool bInaccessibleScriptStructProperty = PropertyOwnerAsScriptStruct
				&& !FStructAccessHelper::CanEmitDirectFieldAccess(PropertyOwnerAsScriptStruct)
				// && !PropertyOwnerAsScriptStruct->GetBoolMetaData(TEXT("BlueprintType"))
				&& ensure(EPropertyAccessOperator::Dot == AccessOperator);
			if (PropertyOwnerAsBPGC && !Context.Dependencies.WillClassBeConverted(PropertyOwnerAsBPGC))
			{
				ensure(EPropertyAccessOperator::None != AccessOperator);
				const FString OperatorStr = (EPropertyAccessOperator::Dot == AccessOperator) ? TEXT("&") : TEXT("");
				const FString ContainerStr = (EPropertyAccessOperator::None == AccessOperator) ? TEXT("this") : FString::Printf(TEXT("%s(%s)"), *OperatorStr, *OuterPath);

				PathToMember = FString::Printf(TEXT("FUnconvertedWrapper__%s(%s).GetRef__%s()"), *FEmitHelper::GetCppName(PropertyOwnerAsBPGC), *ContainerStr
					, *UnicodeToCPPIdentifier(Property->GetName(), false, nullptr));
				Context.MarkUnconvertedClassAsNecessary(PropertyOwnerAsBPGC);
			}
			else if (bInaccessibleScriptStructProperty || Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate) || (!bAllowProtected && Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierProtected)))
			{
				const UBoolProperty* BoolProperty = Cast<const UBoolProperty>(Property);
				const bool bBietfield = BoolProperty && !BoolProperty->IsNativeBool();
				const FString OperatorStr = (EPropertyAccessOperator::Dot == AccessOperator) ? TEXT("&") : TEXT("");
				const FString ContainerStr = (EPropertyAccessOperator::None == AccessOperator) ? TEXT("this") : OuterPath;
				if (bBietfield)
				{
					const FString PropertyLocalName = FEmitHelper::GenerateGetPropertyByName(Context, Property);
					const FString ValueStr = Context.ExportTextItem(Property, Property->ContainerPtrToValuePtr<uint8>(DataContainer, ArrayIndex));
					Context.AddLine(FString::Printf(TEXT("(((UBoolProperty*)%s)->%s(%s(%s), %s, %d));")
						, *PropertyLocalName
						, GET_FUNCTION_NAME_STRING_CHECKED(UBoolProperty, SetPropertyValue_InContainer)
						, *OperatorStr
						, *ContainerStr
						, *ValueStr
						, ArrayIndex));
					continue;
				}

				FString OverrideTypeDeclaration;
				if (ObjectProperty)
				{
					UObject* ObjectPropertyValue = ObjectProperty->GetObjectPropertyValue_InContainer(DataContainer, ArrayIndex);
					if (ObjectPropertyValue && ObjectPropertyValue->IsDefaultSubobject())
					{
						UClass* SubobjectClass = ObjectPropertyValue->GetClass();
						OverrideTypeDeclaration = FString::Printf(TEXT("%s*"), *FEmitHelper::GetCppName(SubobjectClass));
					}
				}

				const FString GetPtrStr = FEmitHelper::AccessInaccessibleProperty(Context, Property, OverrideTypeDeclaration, ContainerStr, OperatorStr, ArrayIndex, ENativizedTermUsage::UnspecifiedOrReference, nullptr);
				PathToMember = Context.GenerateUniqueLocalName();
				Context.AddLine(FString::Printf(TEXT("auto& %s = %s;"), *PathToMember, *GetPtrStr));
			}
			else
			{
				const FString AccessOperatorStr = (EPropertyAccessOperator::None == AccessOperator) ? TEXT("")
					: ((EPropertyAccessOperator::Pointer == AccessOperator) ? TEXT("->") : TEXT("."));
				const bool bStaticArray = (Property->ArrayDim > 1);
				const FString ArrayPost = bStaticArray ? FString::Printf(TEXT("[%d]"), ArrayIndex) : TEXT("");
				PathToMember = FString::Printf(TEXT("%s%s%s%s"), *OuterPath, *AccessOperatorStr, *FEmitHelper::GetCppName(Property), *ArrayPost);
			}

			const uint8* ValuePtr = Property->ContainerPtrToValuePtr<uint8>(DataContainer, ArrayIndex);
			const uint8* DefaultValuePtr = OptionalDefaultDataContainer ? Property->ContainerPtrToValuePtr<uint8>(OptionalDefaultDataContainer, ArrayIndex) : nullptr;
			InnerGenerate(Context, Property, PathToMember, ValuePtr, DefaultValuePtr);
		}
	}
}

void FEmitDefaultValueHelper::GenerateUserStructConstructor(const UUserDefinedStruct* Struct, FEmitterLocalContext& Context)
{
	check(Struct);
	const FString StructName = FEmitHelper::GetCppName(Struct);

	// Declaration
	Context.Header.AddLine(FString::Printf(TEXT("%s();"), *StructName));

	// Definition
	Context.Body.AddLine(FString::Printf(TEXT("%s::%s()"), *StructName, *StructName));
	Context.Body.AddLine(TEXT("{"));

	Context.Body.IncreaseIndent();
	{
		TGuardValue<FCodeText*> OriginalDefaultTarget(Context.DefaultTarget, &Context.Body);
		FStructOnScope StructData(Struct);
		FUserStructOnScopeIgnoreDefaults RawDefaultStructOnScope(Struct);
		for (auto Property : TFieldRange<const UProperty>(Struct))
		{
			// Since UDS types are converted to native USTRUCT, all POD fields must be initialized in the ctor, just as with "regular" native USTRUCT types.
			const bool bForceInit = Property->HasAnyPropertyFlags(CPF_IsPlainOldData);
			OuterGenerate(Context, Property, TEXT(""), StructData.GetStructMemory(), bForceInit ? nullptr : RawDefaultStructOnScope.GetStructMemory(), EPropertyAccessOperator::None);
		}
	}
	Context.Body.DecreaseIndent();

	Context.Body.AddLine(TEXT("}"));
}

void FEmitDefaultValueHelper::InnerGenerate(FEmitterLocalContext& Context, const UProperty* Property, const FString& PathToMember, const uint8* ValuePtr, const uint8* DefaultValuePtr, bool bWithoutFirstConstructionLine)
{
	auto InlineValueStruct = [&](UScriptStruct* OuterStruct, const uint8* LocalValuePtr) -> UScriptStruct*
	{ 
		UScriptStruct* InnerStruct = FBackendHelperUMG::InlineValueStruct(OuterStruct, LocalValuePtr);
		if (InnerStruct)
		{
			Context.StructsUsedAsInlineValues.Add(InnerStruct);
		}
		return InnerStruct;
	};
	auto InlineValueData = [](UScriptStruct* OuterStruct, const uint8* LocalValuePtr) -> const uint8*{ return FBackendHelperUMG::InlineValueData(OuterStruct, LocalValuePtr); };
	auto IsTInlineStruct = [](UScriptStruct* OuterStruct) -> bool { return FBackendHelperUMG::IsTInlineStruct(OuterStruct); };
	auto OneLineConstruction = [&](FEmitterLocalContext& LocalContext, const UProperty* LocalProperty, const uint8* LocalValuePtr, FString& OutSingleLine, bool bGenerateEmptyStructConstructor) -> bool
	{
		bool bComplete = true;
		FString ValueStr = HandleSpecialTypes(LocalContext, LocalProperty, LocalValuePtr);
		if (ValueStr.IsEmpty())
		{
			const UStructProperty* StructProperty = Cast<const UStructProperty>(LocalProperty);
			UScriptStruct* InnerInlineStruct = InlineValueStruct(StructProperty ? StructProperty->Struct : nullptr, LocalValuePtr);
			if (StructProperty && StructProperty->Struct && InnerInlineStruct)
			{
				check(InnerInlineStruct);
				FString StructConstructor;
				bComplete = SpecialStructureConstructor(InnerInlineStruct, InlineValueData(StructProperty->Struct, LocalValuePtr), &StructConstructor);
				ValueStr = bComplete
					? FString::Printf(TEXT("%s(%s)"), *FEmitHelper::GetCppName(StructProperty->Struct), *StructConstructor)
					: FString::Printf(TEXT("ConstructTInlineValue<%s>(%s::StaticStruct())"), *FEmitHelper::GetCppName(StructProperty->Struct), *FEmitHelper::GetCppName(InnerInlineStruct));
			}
			else
			{
				ValueStr = LocalContext.ExportTextItem(LocalProperty, LocalValuePtr);
			}
			if (ValueStr.IsEmpty() && StructProperty)
			{
				check(StructProperty->Struct);
				if (bGenerateEmptyStructConstructor)
				{
					ValueStr = FString::Printf(TEXT("%s%s"), *FEmitHelper::GetCppName(StructProperty->Struct), FEmitHelper::EmptyDefaultConstructor(StructProperty->Struct)); //don;t override existing values
				}
				bComplete = false;
			}
			else if (ValueStr.IsEmpty())
			{
				UE_LOG(LogK2Compiler, Error, TEXT("FEmitDefaultValueHelper Cannot generate initialization: %s"), *LocalProperty->GetPathName());
			}
		}
		OutSingleLine += ValueStr;
		return bComplete;
	};

	if (!bWithoutFirstConstructionLine)
	{
		FString ValueStr;
		const bool bComplete = OneLineConstruction(Context, Property, ValuePtr, ValueStr, false);
		if (!ValueStr.IsEmpty())
		{
			Context.AddLine(FString::Printf(TEXT("%s = %s;"), *PathToMember, *ValueStr));
		}
		// array initialization "array_var = TArray<..>()" is complete, but it still needs items.
		if (bComplete && !Property->IsA<UArrayProperty>() && !Property->IsA<USetProperty>() && !Property->IsA<UMapProperty>())
		{
			return;
		}
	}

	if (const UStructProperty* StructProperty = Cast<const UStructProperty>(Property))
	{
		check(StructProperty->Struct);
		UScriptStruct* InnerInlineStruct = InlineValueStruct(StructProperty->Struct, ValuePtr);

		UScriptStruct* ActualStruct = InnerInlineStruct ? InnerInlineStruct : StructProperty->Struct;
		const uint8* ActualValuePtr = InnerInlineStruct ? InlineValueData(StructProperty->Struct, ValuePtr) : ValuePtr;
		const uint8* ActualDefaultValuePtr = InnerInlineStruct ? InlineValueData(StructProperty->Struct, DefaultValuePtr) : DefaultValuePtr;
		// Create default struct instance, only when DefaultValuePtr is null.
		FStructOnScope DefaultStructOnScope(ActualDefaultValuePtr ? nullptr : ActualStruct);

		const FString ActualPathToMember = InnerInlineStruct ? FString::Printf(TEXT("((%s*)%s.GetPtr())"), *FEmitHelper::GetCppName(InnerInlineStruct), *PathToMember) : PathToMember;

		for (auto LocalProperty : TFieldRange<const UProperty>(ActualStruct))
		{
			OuterGenerate(Context, LocalProperty, ActualPathToMember, ActualValuePtr
				, (ActualDefaultValuePtr ? ActualDefaultValuePtr : DefaultStructOnScope.GetStructMemory())
				, InnerInlineStruct ? EPropertyAccessOperator::Pointer : EPropertyAccessOperator::Dot);
		}
	}

	enum class EStructConstructionType
	{
		InitializeStruct,
		EmptyConstructor,
		Custom,
	};

	auto StructConstruction = [IsTInlineStruct](const UStructProperty* InnerStructProperty) -> EStructConstructionType
	{
		//TODO: if the struct has a custom ExportTextItem, that support PPF_ExportCpp, then ELocalConstructionType::Custom should be returned

		//For UDS and regular native structs the default constructor is not reliable, so we need to use InitializeStruct
		const bool bInitializeWithoutScriptStruct = InnerStructProperty && InnerStructProperty->Struct
			&& InnerStructProperty->Struct->IsNative()
			&& ((0 != (InnerStructProperty->Struct->StructFlags & STRUCT_NoExport)) || IsTInlineStruct(InnerStructProperty->Struct));
		if (!bInitializeWithoutScriptStruct)
		{
			if (InnerStructProperty && !FEmitDefaultValueHelper::SpecialStructureConstructor(InnerStructProperty->Struct, nullptr, nullptr))
			{
				return EStructConstructionType::InitializeStruct;
			}
		}
		return bInitializeWithoutScriptStruct ? EStructConstructionType::EmptyConstructor : EStructConstructionType::Custom;
	};

	auto CreateElementSimple = [OneLineConstruction](FEmitterLocalContext& LocalContext, const UProperty* LocalProperty, const uint8* LocalValuePtr) -> FString
	{
		FString ValueStr;
		const bool bComplete = OneLineConstruction(LocalContext, LocalProperty, LocalValuePtr, ValueStr, true);
		ensure(!ValueStr.IsEmpty());
		if (!bComplete)
		{
			const FString ElemLocName = LocalContext.GenerateUniqueLocalName();
			LocalContext.AddLine(FString::Printf(TEXT("auto %s = %s;"), *ElemLocName, *ValueStr));
			InnerGenerate(LocalContext, LocalProperty, ElemLocName, LocalValuePtr, nullptr, /*bWithoutFirstConstructionLine=*/true);
			ValueStr = ElemLocName;
		}
		return ValueStr;
	};

	if (const UArrayProperty* ArrayProperty = Cast<const UArrayProperty>(Property))
	{
		check(ArrayProperty->Inner);
		FScriptArrayHelper ScriptArrayHelper(ArrayProperty, ValuePtr);
		if (ScriptArrayHelper.Num())
		{
			const UStructProperty* StructProperty = Cast<const UStructProperty>(ArrayProperty->Inner);
			const EStructConstructionType Construction = StructConstruction(StructProperty);
			if(EStructConstructionType::InitializeStruct == Construction)
			{
				const UScriptStruct* InnerStruct = StructProperty ? StructProperty->Struct : nullptr;
				ensure(InnerStruct);
				Context.AddLine(FString::Printf(TEXT("%s.%s(%d);"), *PathToMember, TEXT("AddUninitialized"), ScriptArrayHelper.Num()));
				Context.AddLine(FString::Printf(TEXT("%s->%s(%s.GetData(), %d);")
					, *Context.FindGloballyMappedObject(InnerStruct, UScriptStruct::StaticClass())
					, GET_FUNCTION_NAME_STRING_CHECKED(UStruct, InitializeStruct)
					, *PathToMember
					, ScriptArrayHelper.Num()));

				for (int32 Index = 0; Index < ScriptArrayHelper.Num(); ++Index)
				{
					const FString ArrayElementRefName = Context.GenerateUniqueLocalName();
					Context.AddLine(FString::Printf(TEXT("auto& %s = %s[%d];"), *ArrayElementRefName, *PathToMember, Index));
					// This is a Regular Struct (no special constructor), so we don't need to call constructor
					InnerGenerate(Context, ArrayProperty->Inner, ArrayElementRefName, ScriptArrayHelper.GetRawPtr(Index), nullptr, /*bWithoutFirstConstructionLine=*/ true);
				}
			}
			else
			{
				Context.AddLine(FString::Printf(TEXT("%s.%s(%d);"), *PathToMember, TEXT("Reserve"), ScriptArrayHelper.Num()));

				for (int32 Index = 0; Index < ScriptArrayHelper.Num(); ++Index)
				{
					const uint8* LocalValuePtr = ScriptArrayHelper.GetRawPtr(Index);

					FString ValueStr;
					bool bComplete = OneLineConstruction(Context, ArrayProperty->Inner, LocalValuePtr, ValueStr, true);
					Context.AddLine(FString::Printf(TEXT("%s.Add(%s);"), *PathToMember, *ValueStr));
					if (!bComplete)
					{
						// The constructor was already called
						InnerGenerate(Context, ArrayProperty->Inner, FString::Printf(TEXT("%s[%d]"), *PathToMember, Index), LocalValuePtr, nullptr, /*bWithoutFirstConstructionLine=*/ true);
					}
				}
			}
		}
	}
	else if(const USetProperty* SetProperty = Cast<const USetProperty>(Property))
	{
		check(SetProperty->ElementProp);
		FScriptSetHelper ScriptSetHelper(SetProperty, ValuePtr);
		if (ScriptSetHelper.Num())
		{
			Context.AddLine(FString::Printf(TEXT("%s.Reserve(%d);"), *PathToMember, ScriptSetHelper.Num()));

			auto ForEachElementInSet = [&](TFunctionRef<void(int32)> Process)
			{
				int32 Size = ScriptSetHelper.Num();
				for (int32 I = 0; Size; ++I)
				{
					if (ScriptSetHelper.IsValidIndex(I))
					{
						--Size;
						Process(I);
					}
				}
			};

			const UStructProperty* StructProperty = Cast<const UStructProperty>(SetProperty->ElementProp);
			const EStructConstructionType Construction = StructConstruction(StructProperty);
			if (EStructConstructionType::InitializeStruct == Construction)
			{
				const UScriptStruct* InnerStruct = StructProperty ? StructProperty->Struct : nullptr;
				ensure(InnerStruct);
				const FString SetHelperName = Context.GenerateUniqueLocalName();
				const FString PropertyLocalName = FEmitHelper::GenerateGetPropertyByName(Context, SetProperty);
				const FString StructCppName = FEmitHelper::GetCppName(InnerStruct);
				Context.AddLine(FString::Printf(TEXT("FScriptSetHelper %s(CastChecked<USetProperty>(%s), &%s);"), *SetHelperName, *PropertyLocalName, *PathToMember));
				ForEachElementInSet([&](int32 Index)
				{
					const FString ElementName = Context.GenerateUniqueLocalName();
					Context.AddLine(FString::Printf(TEXT("%s& %s = *(%s*)%s.GetElementPtr(%s.AddDefaultValue_Invalid_NeedsRehash());")
						, *StructCppName, *ElementName, *StructCppName, *SetHelperName, *SetHelperName));
					InnerGenerate(Context, StructProperty, ElementName, ScriptSetHelper.GetElementPtr(Index), nullptr, /*bWithoutFirstConstructionLine=*/true);
				});
				Context.AddLine(FString::Printf(TEXT("%s.Rehash();"), *SetHelperName));
			}
			else
			{
				ForEachElementInSet([&](int32 Index)
				{
					const FString Element = CreateElementSimple(Context, SetProperty->ElementProp, ScriptSetHelper.GetElementPtr(Index));
					Context.AddLine(FString::Printf(TEXT("%s.Add(%s);"), *PathToMember, *Element));
				});
			}
		}
	}
	else if(const UMapProperty* MapProperty = Cast<const UMapProperty>(Property))
	{
		check(MapProperty->KeyProp && MapProperty->ValueProp);
		FScriptMapHelper ScriptMapHelper(MapProperty, ValuePtr);
		if (ScriptMapHelper.Num())
		{
			auto ForEachPairInMap = [&](TFunctionRef<void(int32)> Process)
			{
				int32 Size = ScriptMapHelper.Num();
				for (int32 I = 0; Size; ++I)
				{
					if (ScriptMapHelper.IsValidIndex(I))
					{
						--Size;
						Process(I);
					}
				}
			};

			Context.AddLine(FString::Printf(TEXT("%s.Reserve(%d);"), *PathToMember, ScriptMapHelper.Num()));

			const UStructProperty* KeyStructProperty = Cast<const UStructProperty>(MapProperty->KeyProp);
			const EStructConstructionType KeyConstruction = StructConstruction(KeyStructProperty);
			const UStructProperty* ValueStructProperty = Cast<const UStructProperty>(MapProperty->ValueProp);
			const EStructConstructionType ValueConstruction = StructConstruction(ValueStructProperty);
			if ((EStructConstructionType::InitializeStruct == KeyConstruction) || (EStructConstructionType::InitializeStruct == ValueConstruction))
			{
				const FString MapHelperName = Context.GenerateUniqueLocalName();
				const FString PropertyLocalName = FEmitHelper::GenerateGetPropertyByName(Context, MapProperty);
				Context.AddLine(FString::Printf(TEXT("FScriptMapHelper %s(CastChecked<UMapProperty>(%s), &%s);"), *MapHelperName, *PropertyLocalName, *PathToMember));
				const uint32 ElementTypeCppExportFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend | EPropertyExportCPPFlags::CPPF_NoConst;
				const FString ElementTypeStr = Context.ExportCppDeclaration(MapProperty, EExportedDeclaration::Member, ElementTypeCppExportFlags, FEmitterLocalContext::EPropertyNameInDeclaration::Skip).TrimEnd()
					+ TEXT("::ElementType");

				ForEachPairInMap([&](int32 Index)
				{
					const FString PairName = Context.GenerateUniqueLocalName();
					Context.AddLine(FString::Printf(TEXT("%s& %s = *(%s*)%s.GetPairPtr(%s.AddDefaultValue_Invalid_NeedsRehash());")
						, *ElementTypeStr, *PairName, *ElementTypeStr, *MapHelperName, *MapHelperName));

					{
						bool bKeyComplete = false;
						const FString KeyPath = FString::Printf(TEXT("%s.Key"), *PairName);
						if (EStructConstructionType::Custom == KeyConstruction)
						{
							FString KeyStr;
							bKeyComplete = OneLineConstruction(Context, MapProperty->KeyProp, ScriptMapHelper.GetKeyPtr(Index), KeyStr, /*bGenerateEmptyStructConstructor=*/ false);
							if (!KeyStr.IsEmpty())
							{
								Context.AddLine(FString::Printf(TEXT("%s = %s;"), *KeyPath, *KeyStr));
							}
						}
						if (!bKeyComplete)
						{
							InnerGenerate(Context, MapProperty->KeyProp, KeyPath, ScriptMapHelper.GetKeyPtr(Index), nullptr, /*bWithoutFirstConstructionLine=*/true);
						}
					}

					{
						bool bValueComplete = false;
						const FString ValuePath = FString::Printf(TEXT("%s.Value"), *PairName);
						if (EStructConstructionType::Custom == ValueConstruction)
						{
							FString ValueStr;
							bValueComplete = OneLineConstruction(Context, MapProperty->ValueProp, ScriptMapHelper.GetKeyPtr(Index), ValueStr, /*bGenerateEmptyStructConstructor=*/ false);
							if (!ValueStr.IsEmpty())
							{
								Context.AddLine(FString::Printf(TEXT("%s = %s;"), *ValuePath, *ValueStr));
							}
						}
						if (!bValueComplete)
						{
							InnerGenerate(Context, MapProperty->ValueProp, ValuePath, ScriptMapHelper.GetValuePtr(Index), nullptr, /*bWithoutFirstConstructionLine=*/true);
						}
					}

				});
				Context.AddLine(FString::Printf(TEXT("%s.Rehash();"), *MapHelperName));
			}
			else
			{
				ForEachPairInMap([&](int32 Index)
				{
					const FString KeyStr = CreateElementSimple(Context, MapProperty->KeyProp, ScriptMapHelper.GetKeyPtr(Index));
					const FString ValueStr = CreateElementSimple(Context, MapProperty->ValueProp, ScriptMapHelper.GetValuePtr(Index));
					Context.AddLine(FString::Printf(TEXT("%s.Add(%s, %s);"), *PathToMember, *KeyStr, *ValueStr));
				});
			}
		}
	}
}

bool FEmitDefaultValueHelper::SpecialStructureConstructor(const UStruct* Struct, const uint8* ValuePtr, /*out*/ FString* OutResult)
{
	check(ValuePtr || !OutResult);

	if (FBackendHelperUMG::SpecialStructureConstructorUMG(Struct, ValuePtr, OutResult))
	{
		return true;
	}

	if (FLatentActionInfo::StaticStruct() == Struct)
	{
		if (OutResult)
		{
			const FLatentActionInfo* LatentActionInfo = reinterpret_cast<const FLatentActionInfo*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FLatentActionInfo(%d, %d, TEXT(\"%s\"), this)")
				, LatentActionInfo->Linkage
				, LatentActionInfo->UUID
				, *LatentActionInfo->ExecutionFunction.ToString().ReplaceCharWithEscapedChar());
		}
		return true;
	}

	if (TBaseStructure<FTransform>::Get() == Struct)
	{
		if (OutResult)
		{
			const FTransform* Transform = reinterpret_cast<const FTransform*>(ValuePtr);
			const auto Rotation = Transform->GetRotation();
			const auto Translation = Transform->GetTranslation();
			const auto Scale = Transform->GetScale3D();
			*OutResult = FString::Printf(TEXT("FTransform( FQuat(%s,%s,%s,%s), FVector(%s,%s,%s), FVector(%s,%s,%s) )"),
				*FEmitHelper::FloatToString(Rotation.X), *FEmitHelper::FloatToString(Rotation.Y), *FEmitHelper::FloatToString(Rotation.Z), *FEmitHelper::FloatToString(Rotation.W),
				*FEmitHelper::FloatToString(Translation.X), *FEmitHelper::FloatToString(Translation.Y), *FEmitHelper::FloatToString(Translation.Z),
				*FEmitHelper::FloatToString(Scale.X), *FEmitHelper::FloatToString(Scale.Y), *FEmitHelper::FloatToString(Scale.Z));
		}
		return true;
	}

	if (TBaseStructure<FVector>::Get() == Struct)
	{
		if (OutResult)
		{
			const FVector* Vector = reinterpret_cast<const FVector*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FVector(%s, %s, %s)"), *FEmitHelper::FloatToString(Vector->X), *FEmitHelper::FloatToString(Vector->Y), *FEmitHelper::FloatToString(Vector->Z));
		}
		return true;
	}

	if (TBaseStructure<FGuid>::Get() == Struct)
	{
		if (OutResult)
		{
			const FGuid* Guid = reinterpret_cast<const FGuid*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FGuid(0x%08X, 0x%08X, 0x%08X, 0x%08X)"), Guid->A, Guid->B, Guid->C, Guid->D);
		}
		return true;
	}

	if (TBaseStructure<FRotator>::Get() == Struct)
	{
		if (OutResult)
		{
			const FRotator* Rotator = reinterpret_cast<const FRotator*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FRotator(%s, %s, %s)"), *FEmitHelper::FloatToString(Rotator->Pitch), *FEmitHelper::FloatToString(Rotator->Yaw), *FEmitHelper::FloatToString(Rotator->Roll));
		}
		return true;
	}

	if (TBaseStructure<FLinearColor>::Get() == Struct)
	{
		if (OutResult)
		{
			const FLinearColor* LinearColor = reinterpret_cast<const FLinearColor*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FLinearColor(%s, %s, %s, %s)"), *FEmitHelper::FloatToString(LinearColor->R), *FEmitHelper::FloatToString(LinearColor->G), *FEmitHelper::FloatToString(LinearColor->B), *FEmitHelper::FloatToString(LinearColor->A));
		}
		return true;
	}

	if (TBaseStructure<FColor>::Get() == Struct)
	{
		if (OutResult)
		{
			const FColor* Color = reinterpret_cast<const FColor*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FColor(%d, %d, %d, %d)"), Color->R, Color->G, Color->B, Color->A);
		}
		return true;
	}

	if (TBaseStructure<FVector2D>::Get() == Struct)
	{
		if (OutResult)
		{
			const FVector2D* Vector2D = reinterpret_cast<const FVector2D*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FVector2D(%s, %s)"), *FEmitHelper::FloatToString(Vector2D->X), *FEmitHelper::FloatToString(Vector2D->Y));
		}
		return true;
	}

	if (TBaseStructure<FBox2D>::Get() == Struct)
	{
		if (OutResult)
		{
			const FBox2D* Box2D = reinterpret_cast<const FBox2D*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("CreateFBox2D(FVector2D(%s, %s), FVector2D(%s, %s), %s)")
				, *FEmitHelper::FloatToString(Box2D->Min.X)
				, *FEmitHelper::FloatToString(Box2D->Min.Y)
				, *FEmitHelper::FloatToString(Box2D->Max.X)
				, *FEmitHelper::FloatToString(Box2D->Max.Y)
				, Box2D->bIsValid ? TEXT("true") : TEXT("false"));
		}
		return true;
	}

	if (TBaseStructure<FFloatRangeBound>::Get() == Struct)
	{
		if (OutResult)
		{
			const FFloatRangeBound* FloatRangeBound = reinterpret_cast<const FFloatRangeBound*>(ValuePtr);
			if (FloatRangeBound->IsExclusive())
			{
				*OutResult = FString::Printf(TEXT("FFloatRangeBound::%s(%s)"), GET_FUNCTION_NAME_STRING_CHECKED(FFloatRangeBound, Exclusive), *FEmitHelper::FloatToString(FloatRangeBound->GetValue()));
			}
			if (FloatRangeBound->IsInclusive())
			{
				*OutResult = FString::Printf(TEXT("FFloatRangeBound::%s(%s)"), GET_FUNCTION_NAME_STRING_CHECKED(FFloatRangeBound, Inclusive), *FEmitHelper::FloatToString(FloatRangeBound->GetValue()));
			}
			if (FloatRangeBound->IsOpen())
			{
				*OutResult = FString::Printf(TEXT("FFloatRangeBound::%s()"), GET_FUNCTION_NAME_STRING_CHECKED(FFloatRangeBound, Open));
			}
		}
		return true;
	}

	if (TBaseStructure<FFloatRange>::Get() == Struct)
	{
		if (OutResult)
		{
			const FFloatRange* FloatRangeBound = reinterpret_cast<const FFloatRange*>(ValuePtr);

			FString LowerBoundStr;
			FFloatRangeBound LowerBound = FloatRangeBound->GetLowerBound();
			SpecialStructureConstructor(TBaseStructure<FFloatRangeBound>::Get(), (uint8*)&LowerBound, &LowerBoundStr);

			FString UpperBoundStr;
			FFloatRangeBound UpperBound = FloatRangeBound->GetUpperBound();
			SpecialStructureConstructor(TBaseStructure<FFloatRangeBound>::Get(), (uint8*)&UpperBound, &UpperBoundStr);

			*OutResult = FString::Printf(TEXT("FFloatRange(%s, %s)"), *LowerBoundStr, *UpperBoundStr);
		}
		return true;
	}

	if (TBaseStructure<FInt32RangeBound>::Get() == Struct)
	{
		if (OutResult)
		{
			const FInt32RangeBound* RangeBound = reinterpret_cast<const FInt32RangeBound*>(ValuePtr);
			if (RangeBound->IsExclusive())
			{
				*OutResult = FString::Printf(TEXT("FInt32RangeBound::%s(%d)"), GET_FUNCTION_NAME_STRING_CHECKED(FInt32RangeBound, Exclusive), RangeBound->GetValue());
			}
			if (RangeBound->IsInclusive())
			{
				*OutResult = FString::Printf(TEXT("FInt32RangeBound::%s(%d)"), GET_FUNCTION_NAME_STRING_CHECKED(FInt32RangeBound, Inclusive), RangeBound->GetValue());
			}
			if (RangeBound->IsOpen())
			{
				*OutResult = FString::Printf(TEXT("FInt32RangeBound::%s()"), GET_FUNCTION_NAME_STRING_CHECKED(FFloatRangeBound, Open));
			}
		}
		return true;
	}

	if (TBaseStructure<FInt32Range>::Get() == Struct)
	{
		if (OutResult)
		{
			const FInt32Range* RangeBound = reinterpret_cast<const FInt32Range*>(ValuePtr);

			FString LowerBoundStr;
			FInt32RangeBound LowerBound = RangeBound->GetLowerBound();
			SpecialStructureConstructor(TBaseStructure<FInt32RangeBound>::Get(), (uint8*)&LowerBound, &LowerBoundStr);

			FString UpperBoundStr;
			FInt32RangeBound UpperBound = RangeBound->GetUpperBound();
			SpecialStructureConstructor(TBaseStructure<FInt32RangeBound>::Get(), (uint8*)&UpperBound, &UpperBoundStr);

			*OutResult = FString::Printf(TEXT("FInt32Range(%s, %s)"), *LowerBoundStr, *UpperBoundStr);
		}
		return true;
	}

	if (TBaseStructure<FFloatInterval>::Get() == Struct)
	{
		if (OutResult)
		{
			const FFloatInterval* Interval = reinterpret_cast<const FFloatInterval*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FFloatInterval(%s, %s)"), *FEmitHelper::FloatToString(Interval->Min), *FEmitHelper::FloatToString(Interval->Max));
		}
		return true;
	}

	if (TBaseStructure<FInt32Interval>::Get() == Struct)
	{
		if (OutResult)
		{
			const FInt32Interval* Interval = reinterpret_cast<const FInt32Interval*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FFloatInterval(%d, %d)"), Interval->Min, Interval->Max);
		}
		return true;
	}

	if (TBaseStructure<FSoftClassPath>::Get() == Struct)
	{
		if (OutResult)
		{
			const FSoftClassPath* SoftClassPath = reinterpret_cast<const FSoftClassPath*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FSoftClassPath(TEXT(\"%s\"))"), *SoftClassPath->ToString());
		}
		return true;
	}

	if (TBaseStructure<FSoftObjectPath>::Get() == Struct)
	{
		if (OutResult)
		{
			const FSoftObjectPath* SoftObjectPath = reinterpret_cast<const FSoftObjectPath*>(ValuePtr);
			*OutResult = FString::Printf(TEXT("FSoftObjectPath(TEXT(\"%s\"))"), *SoftObjectPath->ToString());
		}
		return true;
	}

	return false;
}

FString FEmitDefaultValueHelper::HandleSpecialTypes(FEmitterLocalContext& Context, const UProperty* Property, const uint8* ValuePtr)
{
	auto HandleObjectValueLambda = [&Context, Property, ValuePtr](UObject* Object, UClass* Class) -> FString
	{
		if (Object)
		{
			const bool bIsDefaultSubobject = Object->IsDefaultSubobject() && Object->HasAllFlags(RF_DefaultSubObject);
			const bool bIsInstancedReference = Property->HasAnyPropertyFlags(CPF_InstancedReference);

			UClass* ObjectClassToUse = Context.GetFirstNativeOrConvertedClass(Class);
			{
				const FString MappedObject = Context.FindGloballyMappedObject(Object, ObjectClassToUse);
				if (!MappedObject.IsEmpty())
				{
					return MappedObject;
				}
			}

			UClass* BPGC = Context.GetCurrentlyGeneratedClass();

			UChildActorComponent* OuterCAC = Cast<UChildActorComponent>(Object->GetOuter());
			const bool bObjectIsCACTemplate = OuterCAC && OuterCAC->IsIn(BPGC) && OuterCAC->GetChildActorTemplate() == Object;

			const bool bCreatingSubObjectsOfClass = (Context.CurrentCodeType == FEmitterLocalContext::EGeneratedCodeType::SubobjectsOfClass);
			{
				UObject* CDO = BPGC ? BPGC->GetDefaultObject(false) : nullptr;
				if (BPGC && Object && CDO && Object->IsIn(BPGC) && !Object->IsIn(CDO) && bCreatingSubObjectsOfClass)
				{
					return HandleClassSubobject(Context, Object, FEmitterLocalContext::EClassSubobjectList::MiscConvertedSubobjects, true, true, bObjectIsCACTemplate);
				}
			}

			if (!bCreatingSubObjectsOfClass && bIsInstancedReference)
			{
				// Emit ctor code to create the instance only if it's not a default subobject; otherwise, just assign the reference value to a local variable for initialization.
				// Note that we also skip the editor-only check if it's a default subobject. In that case, the instance will either have already been created with CreateDefaultSubobject(),
				// or creation will have been skipped (e.g. CreateEditorOnlyDefaultSubobject()). We check the pointer for NULL before assigning default value overrides in the generated ctor.
				const FString MappedObject = HandleInstancedSubobject(Context, Object, /* bCreateInstance = */ !bIsDefaultSubobject, /* bSkipEditorOnlyCheck = */ bIsDefaultSubobject);

				// We should always find a mapping in this case.
				if (ensure(!MappedObject.IsEmpty()))
				{
					return MappedObject;
				}
			}

			if (!bCreatingSubObjectsOfClass && bObjectIsCACTemplate)
			{
				Context.TemplateFromSubobjectsOfClass.AddUnique(Object);
				const FString MappedObject = Context.FindGloballyMappedObject(Object, ObjectClassToUse);
				if (!MappedObject.IsEmpty())
				{
					return MappedObject;
				}
			}
		}
		else
		{
			// Emit valid representation for a null object.
			return Context.ExportTextItem(Property, ValuePtr);
		}

		return FString();
	};

	FString Result;

	if (const UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property))
	{
		Result = HandleObjectValueLambda(ObjectProperty->GetPropertyValue(ValuePtr), ObjectProperty->PropertyClass);
	}
	else if (const UWeakObjectProperty* WeakObjectProperty = Cast<UWeakObjectProperty>(Property))
	{
		Result = HandleObjectValueLambda(WeakObjectProperty->GetObjectPropertyValue(ValuePtr), WeakObjectProperty->PropertyClass);
	}
	else if (const UInterfaceProperty* InterfaceProperty = Cast<UInterfaceProperty>(Property))
	{
		Result = HandleObjectValueLambda(InterfaceProperty->GetPropertyValue(ValuePtr).GetObject(), InterfaceProperty->InterfaceClass);
	}
	else if (const UStructProperty* StructProperty = Cast<UStructProperty>(Property))
	{
		FString StructConstructor;
		if (SpecialStructureConstructor(StructProperty->Struct, ValuePtr, &StructConstructor))
		{
			Result = StructConstructor;
		}
	}

	return Result;
}

struct FDefaultSubobjectData
{
	UObject* Object;
	UObject* Archetype;
	FString VariableName;
	bool bWasCreated;
	bool bAddLocalScope;

	FDefaultSubobjectData()
		: Object(nullptr)
		, Archetype(nullptr)
		, bWasCreated(false)
		, bAddLocalScope(true)
	{
	}

	virtual ~FDefaultSubobjectData()
	{
	}

	// Generate code to initialize the default subobject based on its archetype.
	virtual void EmitPropertyInitialization(FEmitterLocalContext& Context)
	{
		TSharedPtr<FScopeBlock> ScopeBlock;

		// Start a new scope block only if necessary.
		if (bAddLocalScope)
		{
			if (!bWasCreated)
			{
				// Emit code to check for a valid reference if we didn't create the instance. There are cases where this can be NULL at runtime.
				Context.AddLine(FString::Printf(TEXT("if(%s)"), *VariableName));
			}

			ScopeBlock = MakeShareable(new FScopeBlock(Context));
			Context.AddLine(FString::Printf(TEXT("// --- Default subobject \'%s\' //"), *Object->GetName()));
		}

		// Handle nested default subobjects first. We do it this way since default subobject instances are not always assigned to an object property, but might need to be accessed by other DSOs.
		TArray<UObject*> NestedDefaultSubobjects;
		Object->GetDefaultSubobjects(NestedDefaultSubobjects);
		TArray<FDefaultSubobjectData> NestedSubobjectsToInit;
		for (UObject* DSO : NestedDefaultSubobjects)
		{
			// We don't need to emit code to initialize nested default subobjects that are also editor-only, since they won't be used in a cooked build.
			if (!DSO->IsEditorOnly())
			{
				FDefaultSubobjectData* SubobjectData = new(NestedSubobjectsToInit) FDefaultSubobjectData();
				FEmitDefaultValueHelper::HandleInstancedSubobject(Context, DSO, /* bCreateInstance = */ false, /* bSkipEditorOnlyCheck = */ true, SubobjectData);
			}
		}

		// Recursively emit code to initialize any nested default subobjects found above that that are now locally referenced within this scope block.
		for (FDefaultSubobjectData& DSOEntry : NestedSubobjectsToInit)
		{
			DSOEntry.EmitPropertyInitialization(Context);
		}

		// Now walk through the property list and initialize delta values for this instance. Any nested instanced default
		// subobjects found above that are also assigned to a reference property will be correctly seen as already handled.
		const UClass* ObjectClass = Object->GetClass();
		for (auto Property : TFieldRange<const UProperty>(ObjectClass))
		{
			if (!HandledAsSpecialProperty(Context, Property))
			{
				FEmitDefaultValueHelper::OuterGenerate(Context, Property, VariableName
					, reinterpret_cast<const uint8*>(Object)
					, reinterpret_cast<const uint8*>(Archetype)
					, FEmitDefaultValueHelper::EPropertyAccessOperator::Pointer);
			}
		}

		if (bAddLocalScope)
		{
			// Close current scope block (if necessary).
			Context.AddLine(FString::Printf(TEXT("// --- END default subobject \'%s\' //"), *Object->GetName()));
		}
	}

protected:
	// Generate special-case property initialization code. This could be something that is normally handled through custom serialization.
	bool HandledAsSpecialProperty(FEmitterLocalContext& Context, const UProperty* Property)
	{
		bool bWasHandled = true;

		// We treat the 'BodyInstance' property as a special-case, because of the amount of code that would otherwise need to be emitted to the ctor in order to
		// initialize the default value when it's modified in the Blueprint class defaults. This can occur in an SCS component template owned by a Blueprint class,
		// or otherwise with a UPrimitiveComponent-based Blueprint subclass. For example, changing the collision profile on the BodyInstance member can result in
		// several struct fields differing from their default value, resulting in large blocks of initialization code emitted to the BP ctor that looks like this:
		//
		//	auto& __Local__10 = __Local__7[2];
		//	__Local__10.Channel = FName(TEXT("Pawn"));
		//	__Local__10.Response = ECollisionResponse::ECR_Overlap;
		//	auto& __Local__11 = __Local__7[3];
		//	__Local__11.Channel = FName(TEXT("Visibility"));
		//	__Local__11.Response = ECollisionResponse::ECR_Overlap;
		//	...
		//
		// In most cases, the default that's set by the user conforms to a "standard" collision profile that is configured with the same set of constant channel and
		// response values. However, since the archetype on which either the SCS component template or Blueprint component subclass is based will default to only one
		// such profile, any time the user chooses a different standard profile (per each component), it will force us to emit a large block of code to initialize the
		// Blueprint's default value to match the boilerplate configuration that's associated with that standard profile. Additionally, if we have a large hierarchy
		// of Blueprint component class types, in which each child might change the standard collision profile in the class defaults as a worst case, each time we
		// call the nativized parent Blueprint class ctor, we'd be unnecessarily initializing 'BodyInstance' values, since the last ctor in the chain would be the
		// only collision profile that ultimately would apply. Thus, the incurred overhead can consist of both additional code size and CPU cycles that are wasted.
		//
		// To resolve, we can follow the method that's used by native Actor and ActorComponent subclass types at construction time to help us reduce the redundant
		// code generation that this could otherwise lead to. In C++, invoking the UPrimitiveComponent::SetCollisionProfileName() API allows 'BodyInstance' value
		// initialization that's based on a standard collision profile to both be set and (when appropriate) deferred until after all ctors in the hierarchy have
		// finished executing (UPrimitiveComponent::PostInitProperties). As a result, for nativized BP class ctors, as with native C++ class ctors, we emit a single
		// API call to initialize any non-default collision profile selection for each 'BodyInstance' value per UPrimitiveComponent subobject at construction time.
		// This applies to both editable native components and non-native (SCS) component templates, since the latter will be converted to native default subobjects.
		//
		// In addition to initializing values that will be serialized, SetCollisionProfileName() also indirectly initializes some transient collision response data
		// that depends on the selected collision profile (see FBodyInstance::LoadProfileData() for custom collision profiles, and UCollisionProfile::ReadConfig()
		// for standard ones). This must occur at construction time for native component class types, and at load time for any instanced UPrimitiveComponent object,
		// including non-native, Blueprint-owned SCS and ICH component template objects, in order for the component's transient collision response data to be valid.
		// This transient data can be found within the FBodyInstance::CollisionResponses field (specifically, the FCollisionResponse::ResponseToChannels subfield).
		//
		// How does this differ from non-nativized Blueprint class types? Well, in that case, the Blueprint class-owned SCS template object will start out with a
		// 'BodyInstance' value that's inherited from its archetype (in most cases, the native C++ component class default object). So, the SetCollisionProfileName()
		// API has already been called at the C++ level, before we instance the SCS template. The profile can then be changed by the user in the editor UI, in which
		// case SetCollisionProfileName() will again be called to re-initialize the values based on the new selection. When the Blueprint class asset is saved, so is
		// the SCS template, and the initialized 'BodyInstance' value is also serialized. When we load the Blueprint class back into the editor, the template is then
		// deserialized, and UPrimitiveComponent::Serialize() calls UBodyInstance::FixupData() to initialize the transient collision response data that depends on it.
		//
		// In a cooked build, all three of native C++, nativized Blueprint class C++ and non-nativized Blueprint generated class cases deserialize the 'BodyInstance'
		// value that is serialized along with the owned, instanced component, and will call UBodyInstance::FixupData() to initialize the transient collision response
		// data that depends on it. See UPrimitiveComponent::PostLoad() for the point at which this will occur at runtime for all non-template, instanced components.
		static const UProperty* BodyInstanceProperty = UPrimitiveComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, BodyInstance));

		if (Property == BodyInstanceProperty)
		{
			// Get references to the component template along with its archetype. We expect this to always be a UPrimitiveComponent type.
			UPrimitiveComponent* Component = CastChecked<UPrimitiveComponent>(Object);
			const UPrimitiveComponent* ComponentArchetype = CastChecked<UPrimitiveComponent>(Archetype);

			// Get the current collision profile names for each.
			const FName ComponentCollisionProfileName = Component->BodyInstance.GetCollisionProfileName();
			const FName ComponentArchetypeCollisionProfileName = ComponentArchetype->BodyInstance.GetCollisionProfileName();

			// Initialize a new struct instance that matches the archetype (represents the default struct value inherited by the component template).
			FStructOnScope BodyInstanceToCompare(FBodyInstance::StaticStruct());
			FBodyInstance::StaticStruct()->CopyScriptStruct(BodyInstanceToCompare.GetStructMemory(), &ComponentArchetype->BodyInstance);

			if (ComponentCollisionProfileName != ComponentArchetypeCollisionProfileName)
			{
				// If the component template's collision profile setting differs from the default value, set it using the API to load the modified collision profile.
				// This will initialize the struct's default value in the same manner as will occur at runtime, so we don't emit redundant initialization code to the ctor.
				((FBodyInstance*)BodyInstanceToCompare.GetStructMemory())->SetCollisionProfileName(ComponentCollisionProfileName);

				// Now emit the code to call SetCollisionProfileName() at runtime to initialize the collision profile within the instanced UPrimitiveComponent (see notes above).
				Context.AddLine(FString::Printf(TEXT("%s->SetCollisionProfileName(FName(TEXT(\"%s\")));"), *VariableName, *ComponentCollisionProfileName.ToString().ReplaceCharWithEscapedChar()));
			}

			// Emit the C++ code needed to initialize the remainder of the struct's value within the component that's now being instanced as a default subobject within the converted context.
			const FString PathToMember = FString::Printf(TEXT("%s->BodyInstance"), *VariableName);
			FEmitDefaultValueHelper::InnerGenerate(Context, BodyInstanceProperty, PathToMember, (const uint8*)&Component->BodyInstance, BodyInstanceToCompare.GetStructMemory());
		}
		else
		{
			bWasHandled = false;
		}

		return bWasHandled;
	}
};

struct FNonativeComponentData : public FDefaultSubobjectData
{
	////
	const USCS_Node* SCSNode;
	FString ParentVariableName;
	/** Socket/Bone that Component might attach to */
	FName AttachToName;

	FNonativeComponentData()
		: SCSNode(nullptr)
	{
		bAddLocalScope = false;
	}

	virtual ~FNonativeComponentData()
	{
	}

	virtual void EmitPropertyInitialization(FEmitterLocalContext& Context) override
	{
		ensure(!VariableName.IsEmpty());
		if (bWasCreated)
		{
			Context.AddLine(FString::Printf(TEXT("%s->%s = EComponentCreationMethod::Native;"), *VariableName, GET_MEMBER_NAME_STRING_CHECKED(UActorComponent, CreationMethod)));
		}

		if (!ParentVariableName.IsEmpty())
		{
			const FString SocketName = (AttachToName == NAME_None) ? FString() : FString::Printf(TEXT(", TEXT(\"%s\")"), *AttachToName.ToString());
			Context.AddLine(FString::Printf(TEXT("%s->%s(%s, FAttachmentTransformRules::KeepRelativeTransform %s);")
				, *VariableName
				, GET_FUNCTION_NAME_STRING_CHECKED(USceneComponent, AttachToComponent)
				, *ParentVariableName, *SocketName));
			// AttachTo is called first in case some properties will be overridden.
		}

		// Continue inline here with the default logic, but we don't need to enclose it within a new scope block.
		FDefaultSubobjectData::EmitPropertyInitialization(Context);
	}
};

FString FEmitDefaultValueHelper::HandleNonNativeComponent(FEmitterLocalContext& Context, const USCS_Node* Node
	, TSet<const UProperty*>& OutHandledProperties, TArray<FString>& NativeCreatedComponentProperties
	, const USCS_Node* ParentNode, TArray<FNonativeComponentData>& ComponentsToInit
	, bool bBlockRecursion)
{
	check(Node);
	check(Context.CurrentCodeType == FEmitterLocalContext::EGeneratedCodeType::CommonConstructor);

	FString NativeVariablePropertyName;
	UBlueprintGeneratedClass* BPGC = CastChecked<UBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass());
	if (UActorComponent* ComponentTemplate = Node->GetActualComponentTemplate(BPGC))
	{
		const FString VariableCleanName = Node->GetVariableName().ToString();

		const UObjectProperty* VariableProperty = FindField<UObjectProperty>(BPGC, *VariableCleanName);
		if (VariableProperty)
		{
			NativeVariablePropertyName = FEmitHelper::GetCppName(VariableProperty);
			OutHandledProperties.Add(VariableProperty);
		}
		else
		{
			NativeVariablePropertyName = VariableCleanName;
		}

		//TODO: UGLY HACK UE-40026
		if (bBlockRecursion && Context.CommonSubobjectsMap.Contains(ComponentTemplate))
		{
			return FString();
		}

		Context.AddCommonSubObject_InConstructor(ComponentTemplate, NativeVariablePropertyName);

		if (ComponentTemplate->GetOuter() == BPGC)
		{
			FNonativeComponentData NonativeComponentData;
			NonativeComponentData.SCSNode = Node;
			NonativeComponentData.VariableName = NativeVariablePropertyName;
			NonativeComponentData.Object = ComponentTemplate;
			UClass* ComponentClass = ComponentTemplate->GetClass();
			check(ComponentClass != nullptr);

			UObject* ObjectToCompare = ComponentClass->GetDefaultObject(false);

			if (ComponentTemplate->HasAnyFlags(RF_InheritableComponentTemplate))
			{
				ObjectToCompare = Node->GetActualComponentTemplate(Cast<UBlueprintGeneratedClass>(BPGC->GetSuperClass()));
			}
			else
			{
				Context.AddLine(FString::Printf(TEXT("%s%s = CreateDefaultSubobject<%s>(TEXT(\"%s\"));")
					, (VariableProperty == nullptr) ? TEXT("auto ") : TEXT("")
					, *NativeVariablePropertyName
					, *FEmitHelper::GetCppName(ComponentClass)
					, *VariableCleanName));

				NonativeComponentData.bWasCreated = true;
				NativeCreatedComponentProperties.Add(NativeVariablePropertyName);

				FString ParentVariableName;
				if (ParentNode)
				{
					const FString CleanParentVariableName = ParentNode->GetVariableName().ToString();
					const UObjectProperty* ParentVariableProperty = FindField<UObjectProperty>(BPGC, *CleanParentVariableName);
					ParentVariableName = ParentVariableProperty ? FEmitHelper::GetCppName(ParentVariableProperty) : CleanParentVariableName;
				}
				else if (USceneComponent* ParentComponentTemplate = Node->GetParentComponentTemplate(CastChecked<UBlueprint>(BPGC->ClassGeneratedBy)))
				{
					ParentVariableName = Context.FindGloballyMappedObject(ParentComponentTemplate, USceneComponent::StaticClass());
				}
				NonativeComponentData.ParentVariableName = ParentVariableName;
				NonativeComponentData.AttachToName = Node->AttachToName;
			}
			NonativeComponentData.Archetype = ObjectToCompare;
			ComponentsToInit.Add(NonativeComponentData);
		}
	}

	// Recursively handle child nodes.
	if (!bBlockRecursion)
	{
		for (auto ChildNode : Node->ChildNodes)
		{
			HandleNonNativeComponent(Context, ChildNode, OutHandledProperties, NativeCreatedComponentProperties, Node, ComponentsToInit, bBlockRecursion);
		}
	}

	return NativeVariablePropertyName;
}

struct FDependenciesHelper
{
public:
	// Keep sync with FTypeSingletonCache::GenerateSingletonName
	static FString GenerateZConstructor(UField* Item)
	{
		FString Result;
		if (!ensure(Item))
		{
			return Result;
		}

		for (UObject* Outer = Item; Outer; Outer = Outer->GetOuter())
		{
			if (!Result.IsEmpty())
			{
				Result = TEXT("_") + Result;
			}

			if (Cast<UClass>(Outer) || Cast<UScriptStruct>(Outer))
			{
				FString OuterName = FEmitHelper::GetCppName(CastChecked<UField>(Outer), true);
				Result = OuterName + Result;

				// Structs can also have UPackage outer.
				if (Cast<UClass>(Outer) || Cast<UPackage>(Outer->GetOuter()))
				{
					break;
				}
			}
			else
			{
				Result = Outer->GetName() + Result;
			}
		}

		// Can't use long package names in function names.
		if (Result.StartsWith(TEXT("/Script/"), ESearchCase::CaseSensitive))
		{
			Result = FPackageName::GetShortName(Result);
		}

		const FString ClassString = Item->IsA<UClass>() ? TEXT("UClass") : TEXT("UScriptStruct");
		return FString(TEXT("Z_Construct_")) + ClassString + TEXT("_") + Result + TEXT("()");
	}
};

struct FFakeImportTableHelper
{
	TSet<UObject*> SerializeBeforeSerializeStructDependencies;

	TSet<UObject*> SerializeBeforeCreateCDODependencies;

	FFakeImportTableHelper(UStruct* SourceStruct, UClass* OriginalClass, FEmitterLocalContext& Context)
	{
		UClass* SourceClass = Cast<UClass>(SourceStruct);
		if (ensure(SourceStruct) && ensure(!SourceClass || OriginalClass))
		{
			auto GatherDependencies = [this](UStruct* InStruct)
			{
				SerializeBeforeSerializeStructDependencies.Add(InStruct->GetSuperStruct());

				TArray<UObject*> ObjectsInsideStruct;
				GetObjectsWithOuter(InStruct, ObjectsInsideStruct, true);
				for (UObject* Obj : ObjectsInsideStruct)
				{
					UProperty* Property = Cast<UProperty>(Obj);
					if (!Property)
					{
						continue;
					}
					const UProperty* OwnerProperty = Property->GetOwnerProperty();
					if (!IsValid(OwnerProperty))
					{
						continue;
					}
						
					// TODO:
					// Let UDS_A contain UDS_B. Let UDS_B contain an array or a set of UDS_A. It causes a cyclic dependency. 
					// Should we try to fix it at this stage?

					const bool bIsParam = (0 != (OwnerProperty->PropertyFlags & CPF_Parm)) && OwnerProperty->IsIn(InStruct);
					const bool bIsMemberVariable = (OwnerProperty->GetOuter() == InStruct);
					if (bIsParam || bIsMemberVariable) // Affects the class signature. It is necessary while ZCOnstructor/linking.
					{
						TArray<UObject*> LocalPreloadDependencies;
						Property->GetPreloadDependencies(LocalPreloadDependencies);
						for (UObject* Dependency : LocalPreloadDependencies)
						{
							const bool bDependencyMustBeSerializedBeforeStructIsLinked = Dependency
								&& (Dependency->IsA<UScriptStruct>() || Dependency->IsA<UEnum>());
							if (bDependencyMustBeSerializedBeforeStructIsLinked)
							{
								SerializeBeforeSerializeStructDependencies.Add(Dependency);
							}
						}
					}
				}

				if (UClass* Class = Cast<UClass>(InStruct))
				{
					for (const FImplementedInterface& ImplementedInterface : Class->Interfaces)
					{
						SerializeBeforeSerializeStructDependencies.Add(ImplementedInterface.Class);
					}

					SerializeBeforeCreateCDODependencies.Add(Class->GetSuperClass()->GetDefaultObject());
				}
			};

			GatherDependencies(SourceStruct);
			if (OriginalClass)
			{
				GatherDependencies(OriginalClass);
			}

			auto GetClassesOfSubobjects = [this, &Context](TMap<UObject*, FString>& SubobjectsMap)
			{
				TArray<UObject*> Subobjects;
				SubobjectsMap.GetKeys(Subobjects);
				for (UObject* Subobject : Subobjects)
				{
					if (Subobject)
					{
						UClass* SubobjectClass = Subobject->GetClass();
						SerializeBeforeSerializeStructDependencies.Add(SubobjectClass);
						SerializeBeforeCreateCDODependencies.Add(SubobjectClass->GetDefaultObject());

						// This ensures that any nested asset dependencies will be serialized before attempting to instance a subobject that's a converted type when constructing the CDO.
						if (UBlueprintGeneratedClass* SubobjectClassAsBPGC = Cast<UBlueprintGeneratedClass>(SubobjectClass))
						{
							if (Context.Dependencies.ConvertedClasses.Contains(SubobjectClassAsBPGC))
							{
								TSharedPtr<FGatherConvertedClassDependencies> SubobjectClassDependencies = FGatherConvertedClassDependencies::Get(SubobjectClassAsBPGC, Context.Dependencies.NativizationOptions);
								
								SerializeBeforeCreateCDODependencies.Append(SubobjectClassDependencies->Assets);
								SubobjectClassDependencies->GatherAssetsReferencedByConvertedTypes(SerializeBeforeCreateCDODependencies);
							}
						}
					}
				}
			};

			GetClassesOfSubobjects(Context.ClassSubobjectsMap);
			GetClassesOfSubobjects(Context.CommonSubobjectsMap);
		}
	}

	void FillDependencyData(const UObject* Asset, FCompactBlueprintDependencyData& CompactDataRef) const
	{
		ensure(Asset);

		{
			//Dynamic Class requires no non-native class, owner, archetype..
			CompactDataRef.StructDependency.bSerializationBeforeCreateDependency = false;
			CompactDataRef.StructDependency.bCreateBeforeCreateDependency = false;

			const bool bDependencyNecessaryForLinking = SerializeBeforeSerializeStructDependencies.Contains(const_cast<UObject*>(Asset));

			// Super Class, Interfaces, ScriptStructs, Enums..
			CompactDataRef.StructDependency.bSerializationBeforeSerializationDependency = bDependencyNecessaryForLinking;

			// Everything else
			CompactDataRef.StructDependency.bCreateBeforeSerializationDependency = !bDependencyNecessaryForLinking;
		}

		{
			//everything was created for class
			CompactDataRef.CDODependency.bCreateBeforeCreateDependency = false; 

			// Classes of subobjects, created while CDO construction, including assets they depend on for their own construction
			CompactDataRef.CDODependency.bSerializationBeforeCreateDependency = SerializeBeforeCreateCDODependencies.Contains(const_cast<UObject*>(Asset));

			// CDO is not serialized
			CompactDataRef.CDODependency.bCreateBeforeSerializationDependency = false;
			CompactDataRef.CDODependency.bSerializationBeforeSerializationDependency = false;
		}
	}
};

void FEmitDefaultValueHelper::AddStaticFunctionsForDependencies(FEmitterLocalContext& Context
	, TSharedPtr<FGatherConvertedClassDependencies> ParentDependencies
	, FCompilerNativizationOptions NativizationOptions)
{
	constexpr bool bBootTimeEDL = USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME;
	const bool bEnableBootTimeEDLOptimization = IsEventDrivenLoaderEnabledInCookedBuilds() && bBootTimeEDL;

	// HELPERS
	UStruct* SourceStruct = Context.Dependencies.GetActualStruct();
	UClass* OriginalClass = nullptr;
	if (UClass* SourceClass = Cast<UClass>(SourceStruct))
	{
		OriginalClass = Context.Dependencies.FindOriginalClass(SourceClass);
	}
	const FString CppTypeName = FEmitHelper::GetCppName(SourceStruct);
	FFakeImportTableHelper FakeImportTableHelper(SourceStruct, OriginalClass, Context);

	auto CreateAssetToLoadString = [&Context](const UObject* AssetObj) -> FString
	{
		UClass* AssetType = AssetObj->GetClass();
		if (AssetType->IsChildOf<UUserDefinedEnum>())
		{
			AssetType = UEnum::StaticClass();
		}
		else if (AssetType->IsChildOf<UUserDefinedStruct>())
		{
			AssetType = UScriptStruct::StaticClass();
		}
		else if (AssetType->IsChildOf<UBlueprintGeneratedClass>() && Context.Dependencies.WillClassBeConverted(CastChecked<UBlueprintGeneratedClass>(AssetObj)))
		{
			AssetType = UDynamicClass::StaticClass();
		}

		// Specify the outer if it is not the package
		FString OuterName;
		if(AssetObj->GetOuter() && (AssetObj->GetOuter() != AssetObj->GetOutermost()))
		{
			OuterName = AssetObj->GetOuter()->GetName();
		}

		const FString LongPackagePath = FPackageName::GetLongPackagePath(AssetObj->GetOutermost()->GetPathName());
		return FString::Printf(TEXT("FBlueprintDependencyObjectRef(TEXT(\"%s\"), TEXT(\"%s\"), TEXT(\"%s\"), TEXT(\"%s\"), TEXT(\"%s\"), TEXT(\"%s\")),")
			, *LongPackagePath
			, *FPackageName::GetShortName(AssetObj->GetOutermost()->GetPathName())
			, *AssetObj->GetName()
			, *AssetType->GetOutermost()->GetPathName()
			, *AssetType->GetName()
			, *OuterName);
	};

	auto CreateDependencyRecord = [&NativizationOptions, &FakeImportTableHelper, &CppTypeName, OriginalClass, &CreateAssetToLoadString, bEnableBootTimeEDLOptimization](const UObject* InAsset, FString& OptionalComment) -> FCompactBlueprintDependencyData
	{
		ensure(InAsset);
		if (InAsset && IsEditorOnlyObject(InAsset))
		{
			UE_LOG(LogK2Compiler, Warning, TEXT("Nativized %s depends on editor only asset: %s")
				, (OriginalClass ? *OriginalClass->GetPathName() : *CppTypeName)
				,*InAsset->GetPathName());
			OptionalComment = TEXT("Editor Only asset");
			return FCompactBlueprintDependencyData{};
		}

		{
			bool bNotForClient = false;
			bool bNotForServer = false;
			for (const UObject* Search = InAsset; Search && !Search->IsA(UPackage::StaticClass()); Search = Search->GetOuter())
			{
				bNotForClient = bNotForClient || !Search->NeedsLoadForClient();
				bNotForServer = bNotForServer || !Search->NeedsLoadForServer();
			}
			if (bNotForServer && NativizationOptions.ServerOnlyPlatform)
			{
				OptionalComment = TEXT("Not for server");
				return FCompactBlueprintDependencyData{};
			}
			if (bNotForClient && NativizationOptions.ClientOnlyPlatform)
			{
				OptionalComment = TEXT("Not for client");
				return FCompactBlueprintDependencyData{};
			}
		}

		FNativizationSummary::FDependencyRecord& DependencyRecord = FDependenciesGlobalMapHelper::FindDependencyRecord(InAsset);
		ensure(DependencyRecord.Index >= 0);
		if (DependencyRecord.NativeLine.IsEmpty())
		{
			DependencyRecord.NativeLine = CreateAssetToLoadString(InAsset);
		}

		FCompactBlueprintDependencyData Result;
		Result.ObjectRefIndex = static_cast<int16>(DependencyRecord.Index);
		FakeImportTableHelper.FillDependencyData(InAsset, Result);
		return Result;
	};
	
	auto AddAssetArray = [&Context, SourceStruct, &CreateDependencyRecord, bEnableBootTimeEDLOptimization](const TArray<const UObject*>& Assets)
	{
		if (Assets.Num())
		{
			Context.AddLine(TEXT("const FCompactBlueprintDependencyData LocCompactBlueprintDependencyData[] ="));
			Context.AddLine(TEXT("{"));
			Context.IncreaseIndent();
		}

		auto BlueprintDependencyTypeToString = [](FBlueprintDependencyType DependencyType) -> FString
		{
			return FString::Printf(TEXT("FBlueprintDependencyType(%s, %s, %s, %s)")
				, DependencyType.bSerializationBeforeSerializationDependency ? TEXT("true") : TEXT("false")
				, DependencyType.bCreateBeforeSerializationDependency ? TEXT("true") : TEXT("false")
				, DependencyType.bSerializationBeforeCreateDependency ? TEXT("true") : TEXT("false")
				, DependencyType.bCreateBeforeCreateDependency ? TEXT("true") : TEXT("false"));
		};

		for (const UObject* LocAsset : Assets)
		{
			FString OptionalComment;
			const FCompactBlueprintDependencyData DependencyRecord = CreateDependencyRecord(LocAsset, OptionalComment);

			if (SourceStruct->IsA<UClass>())
			{
				Context.AddLine(FString::Printf(TEXT("{%d, %s, %s},  // %s %s ")
					, DependencyRecord.ObjectRefIndex
					, *BlueprintDependencyTypeToString(DependencyRecord.StructDependency)
					, *BlueprintDependencyTypeToString(DependencyRecord.CDODependency)
					, *OptionalComment
					, *LocAsset->GetFullName()));
			}
			else
			{
				Context.AddLine(FString::Printf(TEXT("{%d, %s},  // %s %s ")
					, DependencyRecord.ObjectRefIndex
					, *BlueprintDependencyTypeToString(DependencyRecord.StructDependency)
					, *OptionalComment
					, *LocAsset->GetFullName()));
			}
		}

		if (Assets.Num())
		{
			Context.DecreaseIndent();
			Context.AddLine(TEXT("};"));
			Context.AddLine(TEXT("for(const FCompactBlueprintDependencyData& CompactData : LocCompactBlueprintDependencyData)"));
			Context.AddLine(TEXT("{"));
			Context.AddLine(FString::Printf(TEXT("\tAssetsToLoad.%s(FBlueprintDependencyData(F__NativeDependencies::Get(CompactData.ObjectRefIndex), CompactData));")
				, bEnableBootTimeEDLOptimization ? TEXT("Add") : TEXT("AddUnique")));
			Context.AddLine(TEXT("}"));
		}
	};

	// 1. GATHER UDS DEFAULT VALUE DEPENDENCIES
	{
		TSet<UObject*> References;
		for (UUserDefinedStruct* UDS : Context.StructsWithDefaultValuesUsed)
		{
			FGatherConvertedClassDependencies::GatherAssetsReferencedByUDSDefaultValue(References, UDS);
		}
		for (UObject* Obj : References)
		{
			Context.UsedObjectInCurrentClass.AddUnique(Obj);
		}
	}

	// 2. ALL ASSETS TO LIST
	TSet<const UBlueprintGeneratedClass*> OtherBPGCs;
	TSet<const UObject*> AllDependenciesToHandle = Context.Dependencies.AllDependencies();
	{
		// Append used objects.
		AllDependenciesToHandle.Append(Context.UsedObjectInCurrentClass);

		// Remove invalid dependencies.
		AllDependenciesToHandle.Remove(nullptr);
		
		// Remove unnecessary dependencies.
		for (auto Iter = AllDependenciesToHandle.CreateIterator(); Iter; ++Iter)
		{
			bool bCanExclude = false;

			if (const UObject* ItObj = *Iter)
			{
				// Special case, we don't need to load any dependencies from CoreUObject.
				static const UPackage* CoreUObjectPackage = UProperty::StaticClass()->GetOutermost();
				bCanExclude = ItObj->GetOutermost() == CoreUObjectPackage;

				// We can exclude native type dependencies if EDL is not going to be enabled at boot time.
				if (!bCanExclude && !bEnableBootTimeEDLOptimization)
				{
					if (const UClass* ObjAsClass = Cast<const UClass>(ItObj))
					{
						if (ObjAsClass->HasAnyClassFlags(CLASS_Native))
						{
							bCanExclude = true;
						}
						else if (const UBlueprintGeneratedClass* OtherBPGC = Cast<const UBlueprintGeneratedClass>(ObjAsClass))
						{
							// Gather the set of all non-native, non-interface class dependencies that will be converted. This is used below to help reduce code size when the EDL will not be enabled at boot time.
							const UBlueprint* BP = Cast<const UBlueprint>(OtherBPGC->ClassGeneratedBy);
							if (Context.Dependencies.WillClassBeConverted(OtherBPGC) && BP && (BP->BlueprintType != EBlueprintType::BPTYPE_Interface))
							{
								OtherBPGCs.Add(OtherBPGC);
							}
						}
					}
					else
					{
						// Exclude native UENUM() types that are not user-defined.
						bCanExclude |= (ItObj->IsA<UEnum>() && !ItObj->IsA<UUserDefinedEnum>());

						// Exclude native USTRUCT() types that are not user-defined.
						bCanExclude |= (ItObj->IsA<UScriptStruct>() && !ItObj->IsA<UUserDefinedStruct>());
					}
				}
			}

			if (bCanExclude)
			{
				Iter.RemoveCurrent();
			}
		}
	}

	// 3. LIST OF UsedAssets
	if (SourceStruct->IsA<UClass>())
	{
		FDisableOptimizationOnScope DisableOptimizationOnScope(*Context.DefaultTarget);

		Context.AddLine(FString::Printf(TEXT("void %s::__StaticDependencies_DirectlyUsedAssets(TArray<FBlueprintDependencyData>& AssetsToLoad)"), *CppTypeName));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();
		TArray<const UObject*> AssetsToAdd;
		for (int32 UsedAssetIndex = 0; UsedAssetIndex < Context.UsedObjectInCurrentClass.Num(); ++UsedAssetIndex)
		{
			const UObject* LocAsset = Context.UsedObjectInCurrentClass[UsedAssetIndex];
			if (AllDependenciesToHandle.Contains(LocAsset))
			{
				AssetsToAdd.Add(LocAsset);
				AllDependenciesToHandle.Remove(LocAsset);
			}
		}
		AddAssetArray(AssetsToAdd);
		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}

	// 4. REMAINING DEPENDENCIES
	{
		FDisableOptimizationOnScope DisableOptimizationOnScope(*Context.DefaultTarget);

		Context.AddLine(FString::Printf(TEXT("void %s::__StaticDependenciesAssets(TArray<FBlueprintDependencyData>& AssetsToLoad)"), *CppTypeName));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();

		if (SourceStruct->IsA<UClass>())
		{
			if (!OtherBPGCs.Num() || bEnableBootTimeEDLOptimization)
			{
				Context.AddLine(FString(TEXT("__StaticDependencies_DirectlyUsedAssets(AssetsToLoad);")));
			}
			else
			{
				// To reduce the size of __StaticDependenciesAssets, all __StaticDependenciesAssets of listed BPs will be called.
				FNativizationSummary::FDependencyRecord& DependencyRecord = FDependenciesGlobalMapHelper::FindDependencyRecord(OriginalClass);
				ensure(DependencyRecord.Index >= 0);
				if (DependencyRecord.NativeLine.IsEmpty())
				{
					DependencyRecord.NativeLine = CreateAssetToLoadString(OriginalClass);
				}
				Context.AddLine(FString::Printf(TEXT("const int16 __OwnIndex = %d;"), DependencyRecord.Index));
				Context.AddLine(FString(TEXT("if(FBlueprintDependencyData::ContainsDependencyData(AssetsToLoad, __OwnIndex)) { return; }")));
				Context.AddLine(TEXT("if(GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME){ __StaticDependencies_DirectlyUsedAssets(AssetsToLoad); }"));
				Context.AddLine(TEXT("else"));
				Context.AddLine(TEXT("{"));
				Context.IncreaseIndent();
				Context.AddLine(FString(TEXT("const bool __FirstFunctionCall = !AssetsToLoad.Num();")));
				Context.AddLine(FString(TEXT("TArray<FBlueprintDependencyData> Temp;")));
				// Other __StaticDependenciesAssets fucntions should not see the assets added by __StaticDependencies_DirectlyUsedAssets
				// But in the first function called the assets from __StaticDependencies_DirectlyUsedAssets must go first in unchanged order (to satisfy FConvertedBlueprintsDependencies::FillUsedAssetsInDynamicClass)
				Context.AddLine(FString(TEXT("__StaticDependencies_DirectlyUsedAssets(__FirstFunctionCall ? AssetsToLoad : Temp);")));
				Context.AddLine(FString(TEXT("TArray<FBlueprintDependencyData>& ArrayUnaffectedByDirectlyUsedAssets = __FirstFunctionCall ? Temp : AssetsToLoad;")));

				Context.AddLine(FString(TEXT("ArrayUnaffectedByDirectlyUsedAssets.AddUnique(FBlueprintDependencyData(F__NativeDependencies::Get(__OwnIndex), FCompactBlueprintDependencyData(__OwnIndex, {}, {})));")));

				for (const UBlueprintGeneratedClass* OtherBPGC : OtherBPGCs)
				{
					Context.AddLine(FString::Printf(TEXT("%s::__StaticDependenciesAssets(ArrayUnaffectedByDirectlyUsedAssets);"), *FEmitHelper::GetCppName(OtherBPGC)));
				}
				Context.AddLine(FString(TEXT("FBlueprintDependencyData::AppendUniquely(AssetsToLoad, Temp);")));
				Context.DecreaseIndent();
				Context.AddLine(TEXT("}"));
			}
		}
		
		AddAssetArray(AllDependenciesToHandle.Array());
		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}
}

void FEmitDefaultValueHelper::AddRegisterHelper(FEmitterLocalContext& Context)
{
	UStruct* SourceStruct = Context.Dependencies.GetActualStruct();
	const FString CppTypeName = FEmitHelper::GetCppName(SourceStruct);

	if (UClass* SourceClass = Cast<UClass>(SourceStruct))
	{
		SourceStruct = Context.Dependencies.FindOriginalClass(SourceClass);
	}

	const FString RegisterHelperName = FString::Printf(TEXT("FRegisterHelper__%s"), *CppTypeName);
	Context.AddLine(FString::Printf(TEXT("struct %s"), *RegisterHelperName));
	Context.AddLine(TEXT("{"));
	Context.IncreaseIndent();

	Context.AddLine(FString::Printf(TEXT("%s()"), *RegisterHelperName));
	Context.AddLine(TEXT("{"));
	Context.IncreaseIndent();

	Context.AddLine(FString::Printf(
		TEXT("FConvertedBlueprintsDependencies::Get().RegisterConvertedClass(TEXT(\"%s\"), &%s::__StaticDependenciesAssets);")
		, *SourceStruct->GetOutermost()->GetPathName()
		, *CppTypeName));

	Context.DecreaseIndent();
	Context.AddLine(TEXT("}"));

	Context.AddLine(FString::Printf(TEXT("static %s Instance;"), *RegisterHelperName));

	Context.DecreaseIndent();
	Context.AddLine(TEXT("};"));

	Context.AddLine(FString::Printf(TEXT("%s %s::Instance;"), *RegisterHelperName, *RegisterHelperName));
}

void FEmitDefaultValueHelper::GenerateCustomDynamicClassInitialization(FEmitterLocalContext& Context, TSharedPtr<FGatherConvertedClassDependencies> ParentDependencies)
{
	auto BPGC = CastChecked<UBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass());
	const FString CppClassName = FEmitHelper::GetCppName(BPGC);

	{
		FDisableOptimizationOnScope DisableOptimizationOnScope(*Context.DefaultTarget);

		Context.AddLine(FString::Printf(TEXT("void %s::__CustomDynamicClassInitialization(UDynamicClass* InDynamicClass)"), *CppClassName));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();
		Context.AddLine(FString::Printf(TEXT("ensure(0 == InDynamicClass->%s.Num());"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ReferencedConvertedFields)));
		Context.AddLine(FString::Printf(TEXT("ensure(0 == InDynamicClass->%s.Num());"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, MiscConvertedSubobjects)));
		Context.AddLine(FString::Printf(TEXT("ensure(0 == InDynamicClass->%s.Num());"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, DynamicBindingObjects)));
		Context.AddLine(FString::Printf(TEXT("ensure(0 == InDynamicClass->%s.Num());"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ComponentTemplates)));
		Context.AddLine(FString::Printf(TEXT("ensure(0 == InDynamicClass->%s.Num());"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, Timelines)));
		Context.AddLine(FString::Printf(TEXT("ensure(nullptr == InDynamicClass->%s);"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, AnimClassImplementation)));
		Context.AddLine(FString::Printf(TEXT("InDynamicClass->%s();"), GET_FUNCTION_NAME_STRING_CHECKED(UDynamicClass, AssembleReferenceTokenStream)));

		Context.CurrentCodeType = FEmitterLocalContext::EGeneratedCodeType::SubobjectsOfClass;
		Context.ResetPropertiesForInaccessibleStructs();

		if (Context.Dependencies.ConvertedEnum.Num())
		{
			Context.AddLine(TEXT("// List of all referenced converted enums"));
		}
		for (auto LocEnum : Context.Dependencies.ConvertedEnum)
		{
			Context.AddLine(FString::Printf(TEXT("InDynamicClass->%s.Add(LoadObject<UEnum>(nullptr, TEXT(\"%s\")));"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ReferencedConvertedFields), *(LocEnum->GetPathName().ReplaceCharWithEscapedChar())));
			Context.EnumsInCurrentClass.Add(LocEnum);
		}

		if (Context.Dependencies.ConvertedClasses.Num())
		{
			Context.AddLine(TEXT("// List of all referenced converted classes"));
		}
		for (auto LocStruct : Context.Dependencies.ConvertedClasses)
		{
			UClass* ClassToLoad = Context.Dependencies.FindOriginalClass(LocStruct);
			if (ensure(ClassToLoad))
			{
				if (ParentDependencies.IsValid() && ParentDependencies->ConvertedClasses.Contains(LocStruct))
				{
					continue;
				}

				FString ClassConstructor;
				if (ClassToLoad->HasAnyClassFlags(CLASS_Interface))
				{
					const FString ClassZConstructor = FDependenciesHelper::GenerateZConstructor(ClassToLoad);
					Context.AddLine(FString::Printf(TEXT("extern UClass* %s;"), *ClassZConstructor));
					ClassConstructor = ClassZConstructor;
				}
				else
				{
					ClassConstructor = FString::Printf(TEXT("%s::StaticClass()"), *FEmitHelper::GetCppName(ClassToLoad));
				}
				Context.AddLine(FString::Printf(TEXT("InDynamicClass->%s.Add(%s);"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ReferencedConvertedFields), *ClassConstructor));

				//Context.AddLine(FString::Printf(TEXT("InDynamicClass->ReferencedConvertedFields.Add(LoadObject<UClass>(nullptr, TEXT(\"%s\")));")
				//	, *(ClassToLoad->GetPathName().ReplaceCharWithEscapedChar())));
			}
		}

		if (Context.Dependencies.ConvertedStructs.Num())
		{
			Context.AddLine(TEXT("// List of all referenced converted structures"));
		}
		for (auto LocStruct : Context.Dependencies.ConvertedStructs)
		{
			if (ParentDependencies.IsValid() && ParentDependencies->ConvertedStructs.Contains(LocStruct))
			{
				continue;
			}
			const FString StructConstructor = FDependenciesHelper::GenerateZConstructor(LocStruct);
			Context.AddLine(FString::Printf(TEXT("extern UScriptStruct* %s;"), *StructConstructor));
			Context.AddLine(FString::Printf(TEXT("InDynamicClass->%s.Add(%s);"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, ReferencedConvertedFields), *StructConstructor));
		}

		TArray<UActorComponent*> ActorComponentTempatesOwnedByClass = BPGC->ComponentTemplates;
		// Gather all CT from SCS and IH, the remaining ones are generated for class..
		if (auto SCS = BPGC->SimpleConstructionScript)
		{
			// >>> This code should be removed, once UE-39168 is fixed
			//TODO: it's an ugly workaround - template from DefaultSceneRootNode is unnecessarily cooked :(
			UActorComponent* DefaultSceneRootComponentTemplate = SCS->GetDefaultSceneRootNode() ? SCS->GetDefaultSceneRootNode()->ComponentTemplate : nullptr;
			if (DefaultSceneRootComponentTemplate)
			{
				ActorComponentTempatesOwnedByClass.Add(DefaultSceneRootComponentTemplate);
			}
			// <<< This code should be removed, once UE-39168 is fixed

			for (auto Node : SCS->GetAllNodes())
			{
				ActorComponentTempatesOwnedByClass.RemoveSwap(Node->ComponentTemplate);
			}
		}
		if (auto IH = BPGC->GetInheritableComponentHandler())
		{
			TArray<UActorComponent*> AllTemplates;
			IH->GetAllTemplates(AllTemplates);
			ActorComponentTempatesOwnedByClass.RemoveAllSwap([&](UActorComponent* Component) -> bool
			{
				return AllTemplates.Contains(Component);
			});
		}

		Context.AddLine(TEXT("FConvertedBlueprintsDependencies::FillUsedAssetsInDynamicClass(InDynamicClass, &__StaticDependencies_DirectlyUsedAssets);"));

		ensure(0 == Context.MiscConvertedSubobjects.Num());
		for (UObject* LocalTemplate : Context.TemplateFromSubobjectsOfClass)
		{
			HandleClassSubobject(Context, LocalTemplate, FEmitterLocalContext::EClassSubobjectList::MiscConvertedSubobjects, true, true, true);
		}

		auto CreateAndInitializeClassSubobjects = [&](bool bCreate, bool bInitialize)
		{
			for (auto ComponentTemplate : ActorComponentTempatesOwnedByClass)
			{
				if (ComponentTemplate)
				{
					HandleClassSubobject(Context, ComponentTemplate, FEmitterLocalContext::EClassSubobjectList::ComponentTemplates, bCreate, bInitialize);
				}
			}

			for (auto TimelineTemplate : BPGC->Timelines)
			{
				if (TimelineTemplate)
				{
					HandleClassSubobject(Context, TimelineTemplate, FEmitterLocalContext::EClassSubobjectList::Timelines, bCreate, bInitialize);
				}
			}

			for (auto DynamicBindingObject : BPGC->DynamicBindingObjects)
			{
				if (DynamicBindingObject)
				{
					HandleClassSubobject(Context, DynamicBindingObject, FEmitterLocalContext::EClassSubobjectList::DynamicBindingObjects, bCreate, bInitialize);
				}
			}
			FBackendHelperUMG::CreateClassSubobjects(Context, bCreate, bInitialize);
		};
		CreateAndInitializeClassSubobjects(true, false);
		CreateAndInitializeClassSubobjects(false, true);

		FBackendHelperAnim::CreateAnimClassData(Context);

		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}

	Context.CurrentCodeType = FEmitterLocalContext::EGeneratedCodeType::Regular;
	Context.ResetPropertiesForInaccessibleStructs();

	FBackendHelperUMG::EmitWidgetInitializationFunctions(Context);
}

void FEmitDefaultValueHelper::GenerateConstructor(FEmitterLocalContext& Context)
{
	UBlueprintGeneratedClass* BPGC = CastChecked<UBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass());
	const FString CppClassName = FEmitHelper::GetCppName(BPGC);

	UClass* SuperClass = BPGC->GetSuperClass();
	const bool bSuperHasObjectInitializerConstructor = SuperClass && SuperClass->HasMetaData(TEXT("ObjectInitializerConstructorDeclared"));

	UObject* CDO = BPGC->GetDefaultObject(false);

	UObject* ParentCDO = BPGC->GetSuperClass()->GetDefaultObject(false);
	check(CDO && ParentCDO);

	TArray<const UProperty*> AnimNodeProperties;
	TArray<FString> NativeCreatedComponentProperties;

	{
		FDisableOptimizationOnScope DisableOptimizationOnScope(*Context.DefaultTarget);
		Context.CurrentCodeType = FEmitterLocalContext::EGeneratedCodeType::CommonConstructor;
		Context.ResetPropertiesForInaccessibleStructs();
		Context.AddLine(FString::Printf(TEXT("%s::%s(const FObjectInitializer& ObjectInitializer) : Super(%s)")
			, *CppClassName
			, *CppClassName
			, bSuperHasObjectInitializerConstructor ? TEXT("ObjectInitializer") : TEXT("")));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();

		// Call CustomDynamicClassInitialization
		Context.AddLine(FString::Printf(TEXT("if(HasAnyFlags(RF_ClassDefaultObject) && (%s::StaticClass() == GetClass()))"), *CppClassName));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();
		Context.AddLine(FString::Printf(TEXT("%s::__CustomDynamicClassInitialization(CastChecked<UDynamicClass>(GetClass()));"), *CppClassName));
		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));

		// Subobjects that must be fixed after serialization
		TArray<FDefaultSubobjectData> SubobjectsToInit;
		TArray<FNonativeComponentData> ComponentsToInit;

		{
			Context.AddLine(TEXT(""));

			FString NativeRootComponentFallback;
			TSet<const UProperty*> HandledProperties;

			// Generate ctor init code for native class default subobjects that are always instanced (e.g. components).
			// @TODO - We can probably make this faster by generating code to directly index through the DSO array instead (i.e. in place of HandleInstancedSubobject which will generate a lookup call per DSO).
			TArray<UObject*> NativeDefaultObjectSubobjects;
			BPGC->GetDefaultObjectSubobjects(NativeDefaultObjectSubobjects);
			for (auto DSO : NativeDefaultObjectSubobjects)
			{
				if (DSO && DSO->GetClass()->HasAnyClassFlags(CLASS_DefaultToInstanced))
				{
					// Determine if this is an editor-only subobject.
					const bool bIsEditorOnlySubobject = DSO->IsEditorOnly();

					// Skip ctor code gen for editor-only subobjects, since they won't be used by the runtime. Any dependencies on editor-only subobjects will be handled later (see HandleInstancedSubobject).
					if (!bIsEditorOnlySubobject)
					{
						// Create a local variable to reference the instanced subobject. We defer any code generation for DSO property initialization so that all local references are declared at the same scope.
						FDefaultSubobjectData* SubobjectData = new(SubobjectsToInit) FDefaultSubobjectData();
						const FString VariableName = HandleInstancedSubobject(Context, DSO, /* bCreateInstance = */ false, /* bSkipEditorOnlyCheck = */ true, SubobjectData);

						// Keep track of which component can be used as a root, in case it's not explicitly set.
						if (NativeRootComponentFallback.IsEmpty())
						{
							USceneComponent* SceneComponent = Cast<USceneComponent>(DSO);
							if (SceneComponent && !SceneComponent->GetAttachParent() && SceneComponent->CreationMethod == EComponentCreationMethod::Native)
							{
								NativeRootComponentFallback = VariableName;
							}
						}
					}
				}
			}

			// Emit the code to initialize all instanced default subobjects now referenced by a local variable.
			for (auto& DSOEntry : SubobjectsToInit)
			{
				DSOEntry.EmitPropertyInitialization(Context);
			}

			// Check for a valid RootComponent property value; mark it as handled if already set in the defaults.
			bool bNeedsRootComponentAssignment = false;
			static const FName RootComponentPropertyName(TEXT("RootComponent"));
			const UObjectProperty* RootComponentProperty = FindField<UObjectProperty>(BPGC, RootComponentPropertyName);
			if (RootComponentProperty)
			{
				if (RootComponentProperty->GetObjectPropertyValue_InContainer(CDO))
				{
					HandledProperties.Add(RootComponentProperty);
				}
				else if (!NativeRootComponentFallback.IsEmpty())
				{
					Context.AddLine(FString::Printf(TEXT("RootComponent = %s;"), *NativeRootComponentFallback));
					HandledProperties.Add(RootComponentProperty);
				}
				else
				{
					bNeedsRootComponentAssignment = true;
				}
			}

			// Generate ctor init code for the SCS node hierarchy (i.e. non-native components). SCS nodes may have dependencies on native DSOs, but not vice-versa.
			TArray<const UBlueprintGeneratedClass*> BPGCStack;
			const bool bErrorFree = UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(BPGC, BPGCStack);
			if (bErrorFree)
			{
				// Start at the base of the hierarchy so that dependencies are handled first.
				for (int32 i = BPGCStack.Num() - 1; i >= 0; --i)
				{
					if (BPGCStack[i]->SimpleConstructionScript)
					{
						for (USCS_Node* Node : BPGCStack[i]->SimpleConstructionScript->GetRootNodes())
						{
							if (Node)
							{
								const FString NativeVariablePropertyName = HandleNonNativeComponent(Context, Node, HandledProperties, NativeCreatedComponentProperties, nullptr, ComponentsToInit, false);

								if (bNeedsRootComponentAssignment && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USceneComponent>() && !NativeVariablePropertyName.IsEmpty())
								{
									// Only emit the explicit root component assignment statement if we're looking at the child BPGC that we're generating ctor code
									// for. In all other cases, the root component will already be set up by a chained parent ctor call, so we avoid stomping it here.
									if (i == 0)
									{
										Context.AddLine(FString::Printf(TEXT("RootComponent = %s;"), *NativeVariablePropertyName));
										HandledProperties.Add(RootComponentProperty);
									}

									bNeedsRootComponentAssignment = false;
								}
							}
						}

						//TODO: UGLY HACK for "zombie" nodes - UE-40026
						for (USCS_Node* Node : BPGCStack[i]->SimpleConstructionScript->GetAllNodes())
						{
							if (Node)
							{
								const bool bNodeWasProcessed = nullptr != ComponentsToInit.FindByPredicate([=](const FNonativeComponentData& InData) { return Node == InData.SCSNode; });
								if (!bNodeWasProcessed)
								{
									HandleNonNativeComponent(Context, Node, HandledProperties, NativeCreatedComponentProperties, nullptr, ComponentsToInit, true);
								}
							}
						}

					}
				}

				for (auto& ComponentToInit : ComponentsToInit)
				{
					ComponentToInit.EmitPropertyInitialization(Context);
				}
			}

			// Collect all anim node properties
			for (auto Property : TFieldRange<const UProperty>(BPGC))
			{
				if (!HandledProperties.Contains(Property))
				{
					if(FBackendHelperAnim::ShouldAddAnimNodeInitializationFunctionCall(Context, Property))
					{
						AnimNodeProperties.Add(Property);
					}
				}
			}
	
			// Emit call to anim node init if necessary
			if(AnimNodeProperties.Num())
			{
				FBackendHelperAnim::AddAllAnimNodesInitializationFunctionCall(Context);
			}

			// Generate ctor init code for generated Blueprint class property values that may differ from parent class defaults (or that otherwise belong to the generated Blueprint class).
			for (auto Property : TFieldRange<const UProperty>(BPGC))
			{
				if (!HandledProperties.Contains(Property))
				{
					if(!FBackendHelperAnim::ShouldAddAnimNodeInitializationFunctionCall(Context, Property))
					{
						const bool bNewProperty = Property->GetOwnerStruct() == BPGC;
						OuterGenerate(Context, Property, TEXT(""), reinterpret_cast<const uint8*>(CDO), bNewProperty ? nullptr : reinterpret_cast<const uint8*>(ParentCDO), EPropertyAccessOperator::None, true);
					}
				}
			}
		}
		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}

	// TODO: this mechanism could be required by other instanced subobjects.
	Context.CurrentCodeType = FEmitterLocalContext::EGeneratedCodeType::Regular;
	Context.ResetPropertiesForInaccessibleStructs();

	// Now output any anim node init functions
	if(AnimNodeProperties.Num())
	{
		FBackendHelperAnim::AddAllAnimNodesInitializationFunction(Context, CppClassName, AnimNodeProperties);

		// Add any anim node properties as their own functions now
		for(const UProperty* AnimNodeProperty : AnimNodeProperties)
		{
			const bool bNewProperty = AnimNodeProperty->GetOwnerStruct() == BPGC;
			FBackendHelperAnim::AddAnimNodeInitializationFunction(Context, CppClassName, AnimNodeProperty, bNewProperty, CDO, ParentCDO);

			Context.ResetPropertiesForInaccessibleStructs();
		}
	}

	Context.ResetPropertiesForInaccessibleStructs();
	Context.AddLine(FString::Printf(TEXT("void %s::%s(FObjectInstancingGraph* OuterInstanceGraph)"), *CppClassName, GET_FUNCTION_NAME_STRING_CHECKED(UObject, PostLoadSubobjects)));
	Context.AddLine(TEXT("{"));
	Context.IncreaseIndent();
	Context.AddLine(FString::Printf(TEXT("Super::%s(OuterInstanceGraph);"), GET_FUNCTION_NAME_STRING_CHECKED(UObject, PostLoadSubobjects)));
	for (auto& ComponentToFix : NativeCreatedComponentProperties)
	{
		Context.AddLine(FString::Printf(TEXT("if(%s)"), *ComponentToFix));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();
		Context.AddLine(FString::Printf(TEXT("%s->%s = EComponentCreationMethod::Native;"), *ComponentToFix, GET_MEMBER_NAME_STRING_CHECKED(UActorComponent, CreationMethod)));
		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}
	Context.DecreaseIndent();
	Context.AddLine(TEXT("}"));
}

FString FEmitDefaultValueHelper::HandleClassSubobject(FEmitterLocalContext& Context, UObject* Object, FEmitterLocalContext::EClassSubobjectList ListOfSubobjectsType, bool bCreate, bool bInitialize, bool bForceSubobjectOfClass)
{
	ensure(Context.CurrentCodeType == FEmitterLocalContext::EGeneratedCodeType::SubobjectsOfClass);

	FString LocalNativeName;
	if (bCreate)
	{
		const bool bAddAsSubobjectOfClass = bForceSubobjectOfClass || (Object->GetOuter() == Context.GetCurrentlyGeneratedClass());
		FString OuterStr;
		if (bAddAsSubobjectOfClass)
		{
			OuterStr = TEXT("InDynamicClass");
		}
		else
		{
			OuterStr = Context.FindGloballyMappedObject(Object->GetOuter());
			if (OuterStr.IsEmpty())
			{
				OuterStr = HandleClassSubobject(Context, Object->GetOuter(), ListOfSubobjectsType, bCreate, bInitialize);
				if (OuterStr.IsEmpty())
				{
					return FString();
				}
				const FString AlreadyCreatedObject = Context.FindGloballyMappedObject(Object);
				if (!AlreadyCreatedObject.IsEmpty())
				{
					return AlreadyCreatedObject;
				}
			}
		}
		
		LocalNativeName = Context.GenerateUniqueLocalName();
		Context.AddClassSubObject_InConstructor(Object, LocalNativeName);
		UClass* ObjectClass = Object->GetClass();
		const int32 ObjectFlags = (int32)Object->GetFlags();
		const FString ActualClass = Context.FindGloballyMappedObject(ObjectClass, UClass::StaticClass());
		const FString NativeType = FEmitHelper::GetCppName(Context.GetFirstNativeOrConvertedClass(ObjectClass));
		if(!ObjectClass->IsNative())
		{
			// make sure CDO has been created for NativeType:
			Context.AddLine(FString::Printf(TEXT("%s::StaticClass()->GetDefaultObject();"), *NativeType));
		}
		Context.AddLine(FString::Printf(
			TEXT("auto %s = NewObject<%s>(%s, %s, TEXT(\"%s\"), (EObjectFlags)0x%08x);")
			, *LocalNativeName
			, *NativeType
			, *OuterStr
			, *ActualClass
			, *Object->GetName().ReplaceCharWithEscapedChar()
			, ObjectFlags));
		if (bAddAsSubobjectOfClass)
		{
			Context.RegisterClassSubobject(Object, ListOfSubobjectsType);
			Context.AddLine(FString::Printf(TEXT("InDynamicClass->%s.Add(%s);")
				, Context.ClassSubobjectListName(ListOfSubobjectsType)
				, *LocalNativeName));
		}
	}

	if (bInitialize)
	{
		if (LocalNativeName.IsEmpty())
		{
			LocalNativeName = Context.FindGloballyMappedObject(Object);
		}
		
		if (ensure(!LocalNativeName.IsEmpty()))
		{
			auto CDO = Object->GetClass()->GetDefaultObject(false);
			for (auto Property : TFieldRange<const UProperty>(Object->GetClass()))
			{
				OuterGenerate(Context, Property, LocalNativeName
					, reinterpret_cast<const uint8*>(Object)
					, reinterpret_cast<const uint8*>(CDO)
					, EPropertyAccessOperator::Pointer);
			}
		}
	}
	return LocalNativeName;
}

FString FEmitDefaultValueHelper::HandleInstancedSubobject(FEmitterLocalContext& Context, UObject* Object, bool bCreateInstance, bool bSkipEditorOnlyCheck, FDefaultSubobjectData* SubobjectData)
{
	check(Object);

	// Make sure we don't emit initialization code for the same object more than once.
	FString LocalNativeName = Context.FindGloballyMappedObject(Object);
	if (!LocalNativeName.IsEmpty())
	{
		return LocalNativeName;
	}
	else
	{
		LocalNativeName = Context.GenerateUniqueLocalName();
	}

	if (Context.CurrentCodeType == FEmitterLocalContext::EGeneratedCodeType::SubobjectsOfClass)
	{
		Context.AddClassSubObject_InConstructor(Object, LocalNativeName);
	}
	else if (Context.CurrentCodeType == FEmitterLocalContext::EGeneratedCodeType::CommonConstructor)
	{
		Context.AddCommonSubObject_InConstructor(Object, LocalNativeName);
	}

	UClass* ObjectClass = Object->GetClass();

	// Determine if this is an editor-only subobject. When handling as a dependency, we'll create a "dummy" object in its place (below).
	bool bIsEditorOnlySubobject = false;
	if (!bSkipEditorOnlyCheck)
	{
		if (UActorComponent* ActorComponent = Cast<UActorComponent>(Object))
		{
			bIsEditorOnlySubobject = ActorComponent->IsEditorOnly();
			if (bIsEditorOnlySubobject)
			{
				// Replace the potentially editor-only class with a base actor/scene component class that's available to the runtime. We'll create a "dummy" object of this type to stand in for the editor-only subobject below.
				ObjectClass = ObjectClass->IsChildOf<USceneComponent>() ? USceneComponent::StaticClass() : UActorComponent::StaticClass();
			}
		}
	}

	auto BPGC = Context.GetCurrentlyGeneratedClass();
	auto CDO = BPGC ? BPGC->GetDefaultObject(false) : nullptr;

	FString OuterStr;
	if (ensure(CDO) && (CDO == Object->GetOuter()))
	{
		OuterStr = TEXT("this");
	}
	else
	{
		OuterStr = Context.FindGloballyMappedObject(Object->GetOuter());
	}

	// Outer must be non-empty at this point.
	if (OuterStr.IsEmpty())
	{
		ensureMsgf(false, TEXT("Encountered an unknown or missing outer for subobject %s (%s)"), *Object->GetName(), *BPGC->GetName());
		return FString();
	}
	
	if (!bIsEditorOnlySubobject)
	{
		if (bCreateInstance)
		{
			if (Object->HasAnyFlags(RF_DefaultSubObject))
			{
				Context.AddLine(FString::Printf(TEXT("auto %s = %s->CreateDefaultSubobject<%s>(TEXT(\"%s\"));")
					, *LocalNativeName, *OuterStr, *FEmitHelper::GetCppName(ObjectClass), *Object->GetName()));
			}
			else
			{
				const int32 ObjectFlags = ((int32)Object->GetFlags() & ~RF_ArchetypeObject);
				Context.AddLine(FString::Printf(TEXT("auto %s = NewObject<%s>(%s, TEXT(\"%s\"), (EObjectFlags)0x%08x);")
					, *LocalNativeName, *FEmitHelper::GetCppName(ObjectClass), *OuterStr, *Object->GetName(), ObjectFlags));
			}
		}
		else
		{
			check(Object->IsDefaultSubobject());

			Context.AddLine(FString::Printf(TEXT("auto %s = CastChecked<%s>(%s->%s(TEXT(\"%s\")), ECastCheckedType::NullAllowed);")
				, *LocalNativeName
				, *FEmitHelper::GetCppName(ObjectClass)
				, *OuterStr
				, GET_FUNCTION_NAME_STRING_CHECKED(UObject, GetDefaultSubobjectByName)
				, *Object->GetName()));
		}

		bool bEmitPropertyInitialization = false;
		FDefaultSubobjectData LocalSubobjectData;
		if (!SubobjectData)
		{
			// If no reference was given, then we go ahead and emit code to initialize the instance here.
			bEmitPropertyInitialization = true;
			SubobjectData = &LocalSubobjectData;
		}

		// Track the object for initialization (below).
		SubobjectData->Object = Object;
		SubobjectData->Archetype = Object->GetArchetype();
		SubobjectData->VariableName = LocalNativeName;
		SubobjectData->bWasCreated = bCreateInstance;

		// Emit code to initialize the instance (if not deferred).
		if (bEmitPropertyInitialization)
		{
			SubobjectData->EmitPropertyInitialization(Context);
		}
	}
	else
	{
		// We should always be the one creating an instance in this case.
		check(bCreateInstance);

		// Dummy object that's instanced for any editor-only subobject dependencies.
		const FString ActualClass = Context.FindGloballyMappedObject(ObjectClass, UClass::StaticClass());
		const FString NativeType = FEmitHelper::GetCppName(Context.GetFirstNativeOrConvertedClass(ObjectClass));
		if(!ObjectClass->IsNative())
		{
			// make sure CDO has been created for NativeType:
			Context.AddLine(FString::Printf(TEXT("%s::StaticClass()->GetDefaultObject();"), *NativeType));
		}
		Context.AddLine(FString::Printf(
			TEXT("auto %s = NewObject<%s>(%s, %s, TEXT(\"%s\"));")
			, *LocalNativeName
			, *NativeType
			, *OuterStr
			, *ActualClass
			, *Object->GetName().ReplaceCharWithEscapedChar()));
	}

	return LocalNativeName;
}
