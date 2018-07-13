// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PyWrapperTypeRegistry.h"
#include "PyWrapperOwnerContext.h"
#include "PyWrapperObject.h"
#include "PyWrapperStruct.h"
#include "PyWrapperDelegate.h"
#include "PyWrapperEnum.h"
#include "PyWrapperName.h"
#include "PyWrapperText.h"
#include "PyWrapperArray.h"
#include "PyWrapperFixedArray.h"
#include "PyWrapperSet.h"
#include "PyWrapperMap.h"
#include "PyConversion.h"
#include "PyGIL.h"
#include "PyCore.h"
#include "PyFileWriter.h"
#include "PythonScriptPluginSettings.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "SourceCodeNavigation.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/EnumProperty.h"
#include "UObject/StructOnScope.h"

#if WITH_PYTHON

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Generate Wrapped Class Total Time"), STAT_GenerateWrappedClassTotalTime, STATGROUP_Python);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Generate Wrapped Class Call Count"), STAT_GenerateWrappedClassCallCount, STATGROUP_Python);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Generate Wrapped Class Obj Count"), STAT_GenerateWrappedClassObjCount, STATGROUP_Python);

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Generate Wrapped Struct Total Time"), STAT_GenerateWrappedStructTotalTime, STATGROUP_Python);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Generate Wrapped Struct Call Count"), STAT_GenerateWrappedStructCallCount, STATGROUP_Python);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Generate Wrapped Struct Obj Count"), STAT_GenerateWrappedStructObjCount, STATGROUP_Python);

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Generate Wrapped Enum Total Time"), STAT_GenerateWrappedEnumTotalTime, STATGROUP_Python);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Generate Wrapped Enum Call Count"), STAT_GenerateWrappedEnumCallCount, STATGROUP_Python);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Generate Wrapped Enum Obj Count"), STAT_GenerateWrappedEnumObjCount, STATGROUP_Python);

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Generate Wrapped Delegate Total Time"), STAT_GenerateWrappedDelegateTotalTime, STATGROUP_Python);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Generate Wrapped Delegate Call Count"), STAT_GenerateWrappedDelegateCallCount, STATGROUP_Python);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Generate Wrapped Delegate Obj Count"), STAT_GenerateWrappedDelegateObjCount, STATGROUP_Python);

FPyWrapperObjectFactory& FPyWrapperObjectFactory::Get()
{
	static FPyWrapperObjectFactory Instance;
	return Instance;
}

FPyWrapperObject* FPyWrapperObjectFactory::FindInstance(UObject* InUnrealInstance) const
{
	if (!InUnrealInstance)
	{
		return nullptr;
	}

	PyTypeObject* PyType = FPyWrapperTypeRegistry::Get().GetWrappedClassType(InUnrealInstance->GetClass());
	return FindInstanceInternal(InUnrealInstance, PyType);
}

FPyWrapperObject* FPyWrapperObjectFactory::CreateInstance(UObject* InUnrealInstance)
{
	if (!InUnrealInstance)
	{
		return nullptr;
	}

	PyTypeObject* PyType = FPyWrapperTypeRegistry::Get().GetWrappedClassType(InUnrealInstance->GetClass());
	return CreateInstanceInternal(InUnrealInstance, PyType, [InUnrealInstance](FPyWrapperObject* InSelf)
	{
		return FPyWrapperObject::Init(InSelf, InUnrealInstance);
	});
}

FPyWrapperObject* FPyWrapperObjectFactory::CreateInstance(UClass* InInterfaceClass, UObject* InUnrealInstance)
{
	if (!InInterfaceClass || !InUnrealInstance)
	{
		return nullptr;
	}

	PyTypeObject* PyType = FPyWrapperTypeRegistry::Get().GetWrappedClassType(InInterfaceClass);
	return CreateInstanceInternal(InUnrealInstance, PyType, [InUnrealInstance](FPyWrapperObject* InSelf)
	{
		return FPyWrapperObject::Init(InSelf, InUnrealInstance);
	});
}


FPyWrapperStructFactory& FPyWrapperStructFactory::Get()
{
	static FPyWrapperStructFactory Instance;
	return Instance;
}

FPyWrapperStruct* FPyWrapperStructFactory::FindInstance(UScriptStruct* InStruct, void* InUnrealInstance) const
{
	if (!InStruct || !InUnrealInstance)
	{
		return nullptr;
	}

	PyTypeObject* PyType = FPyWrapperTypeRegistry::Get().GetWrappedStructType(InStruct);
	return FindInstanceInternal(InUnrealInstance, PyType);
}

FPyWrapperStruct* FPyWrapperStructFactory::CreateInstance(UScriptStruct* InStruct, void* InUnrealInstance, const FPyWrapperOwnerContext& InOwnerContext, const EPyConversionMethod InConversionMethod)
{
	if (!InStruct || !InUnrealInstance)
	{
		return nullptr;
	}

	PyTypeObject* PyType = FPyWrapperTypeRegistry::Get().GetWrappedStructType(InStruct);
	return CreateInstanceInternal(InUnrealInstance, PyType, [InStruct, InUnrealInstance, &InOwnerContext, InConversionMethod](FPyWrapperStruct* InSelf)
	{
		return FPyWrapperStruct::Init(InSelf, InOwnerContext, InStruct, InUnrealInstance, InConversionMethod);
	}, InConversionMethod == EPyConversionMethod::Copy || InConversionMethod == EPyConversionMethod::Steal);
}


FPyWrapperDelegateFactory& FPyWrapperDelegateFactory::Get()
{
	static FPyWrapperDelegateFactory Instance;
	return Instance;
}

FPyWrapperDelegate* FPyWrapperDelegateFactory::FindInstance(const UFunction* InDelegateSignature, FScriptDelegate* InUnrealInstance) const
{
	if (!InDelegateSignature || !InUnrealInstance)
	{
		return nullptr;
	}

	PyTypeObject* PyType = FPyWrapperTypeRegistry::Get().GetWrappedDelegateType(InDelegateSignature);
	return FindInstanceInternal(InUnrealInstance, PyType);
}

FPyWrapperDelegate* FPyWrapperDelegateFactory::CreateInstance(const UFunction* InDelegateSignature, FScriptDelegate* InUnrealInstance, const FPyWrapperOwnerContext& InOwnerContext, const EPyConversionMethod InConversionMethod)
{
	if (!InDelegateSignature || !InUnrealInstance)
	{
		return nullptr;
	}

	PyTypeObject* PyType = FPyWrapperTypeRegistry::Get().GetWrappedDelegateType(InDelegateSignature);
	return CreateInstanceInternal(InUnrealInstance, PyType, [InUnrealInstance, &InOwnerContext, InConversionMethod](FPyWrapperDelegate* InSelf)
	{
		return FPyWrapperDelegate::Init(InSelf, InOwnerContext, InUnrealInstance, InConversionMethod);
	}, InConversionMethod == EPyConversionMethod::Copy || InConversionMethod == EPyConversionMethod::Steal);
}


FPyWrapperMulticastDelegateFactory& FPyWrapperMulticastDelegateFactory::Get()
{
	static FPyWrapperMulticastDelegateFactory Instance;
	return Instance;
}

FPyWrapperMulticastDelegate* FPyWrapperMulticastDelegateFactory::FindInstance(const UFunction* InDelegateSignature, FMulticastScriptDelegate* InUnrealInstance) const
{
	if (!InDelegateSignature || !InUnrealInstance)
	{
		return nullptr;
	}

	PyTypeObject* PyType = FPyWrapperTypeRegistry::Get().GetWrappedDelegateType(InDelegateSignature);
	return FindInstanceInternal(InUnrealInstance, PyType);
}

FPyWrapperMulticastDelegate* FPyWrapperMulticastDelegateFactory::CreateInstance(const UFunction* InDelegateSignature, FMulticastScriptDelegate* InUnrealInstance, const FPyWrapperOwnerContext& InOwnerContext, const EPyConversionMethod InConversionMethod)
{
	if (!InDelegateSignature || !InUnrealInstance)
	{
		return nullptr;
	}

	PyTypeObject* PyType = FPyWrapperTypeRegistry::Get().GetWrappedDelegateType(InDelegateSignature);
	return CreateInstanceInternal(InUnrealInstance, PyType, [InUnrealInstance, &InOwnerContext, InConversionMethod](FPyWrapperMulticastDelegate* InSelf)
	{
		return FPyWrapperMulticastDelegate::Init(InSelf, InOwnerContext, InUnrealInstance, InConversionMethod);
	}, InConversionMethod == EPyConversionMethod::Copy || InConversionMethod == EPyConversionMethod::Steal);
}


FPyWrapperNameFactory& FPyWrapperNameFactory::Get()
{
	static FPyWrapperNameFactory Instance;
	return Instance;
}

FPyWrapperName* FPyWrapperNameFactory::FindInstance(const FName InUnrealInstance) const
{
	return FindInstanceInternal(InUnrealInstance, &PyWrapperNameType);
}

FPyWrapperName* FPyWrapperNameFactory::CreateInstance(const FName InUnrealInstance)
{
	return CreateInstanceInternal(InUnrealInstance, &PyWrapperNameType, [InUnrealInstance](FPyWrapperName* InSelf)
	{
		return FPyWrapperName::Init(InSelf, InUnrealInstance);
	});
}


FPyWrapperTextFactory& FPyWrapperTextFactory::Get()
{
	static FPyWrapperTextFactory Instance;
	return Instance;
}

FPyWrapperText* FPyWrapperTextFactory::FindInstance(const FText InUnrealInstance) const
{
	return FindInstanceInternal(InUnrealInstance, &PyWrapperTextType);
}

FPyWrapperText* FPyWrapperTextFactory::CreateInstance(const FText InUnrealInstance)
{
	return CreateInstanceInternal(InUnrealInstance, &PyWrapperTextType, [InUnrealInstance](FPyWrapperText* InSelf)
	{
		return FPyWrapperText::Init(InSelf, InUnrealInstance);
	});
}


FPyWrapperArrayFactory& FPyWrapperArrayFactory::Get()
{
	static FPyWrapperArrayFactory Instance;
	return Instance;
}

FPyWrapperArray* FPyWrapperArrayFactory::FindInstance(void* InUnrealInstance) const
{
	if (!InUnrealInstance)
	{
		return nullptr;
	}

	return FindInstanceInternal(InUnrealInstance, &PyWrapperArrayType);
}

FPyWrapperArray* FPyWrapperArrayFactory::CreateInstance(void* InUnrealInstance, const UArrayProperty* InProp, const FPyWrapperOwnerContext& InOwnerContext, const EPyConversionMethod InConversionMethod)
{
	if (!InUnrealInstance)
	{
		return nullptr;
	}

	return CreateInstanceInternal(InUnrealInstance, &PyWrapperArrayType, [InUnrealInstance, InProp, &InOwnerContext, InConversionMethod](FPyWrapperArray* InSelf)
	{
		return FPyWrapperArray::Init(InSelf, InOwnerContext, InProp, InUnrealInstance, InConversionMethod);
	}, InConversionMethod == EPyConversionMethod::Copy || InConversionMethod == EPyConversionMethod::Steal);
}


FPyWrapperFixedArrayFactory& FPyWrapperFixedArrayFactory::Get()
{
	static FPyWrapperFixedArrayFactory Instance;
	return Instance;
}

FPyWrapperFixedArray* FPyWrapperFixedArrayFactory::FindInstance(void* InUnrealInstance) const
{
	if (!InUnrealInstance)
	{
		return nullptr;
	}

	return FindInstanceInternal(InUnrealInstance, &PyWrapperFixedArrayType);
}

FPyWrapperFixedArray* FPyWrapperFixedArrayFactory::CreateInstance(void* InUnrealInstance, const UProperty* InProp, const FPyWrapperOwnerContext& InOwnerContext, const EPyConversionMethod InConversionMethod)
{
	if (!InUnrealInstance)
	{
		return nullptr;
	}

	return CreateInstanceInternal(InUnrealInstance, &PyWrapperFixedArrayType, [InUnrealInstance, InProp, &InOwnerContext, InConversionMethod](FPyWrapperFixedArray* InSelf)
	{
		return FPyWrapperFixedArray::Init(InSelf, InOwnerContext, InProp, InUnrealInstance, InConversionMethod);
	}, InConversionMethod == EPyConversionMethod::Copy || InConversionMethod == EPyConversionMethod::Steal);
}


FPyWrapperSetFactory& FPyWrapperSetFactory::Get()
{
	static FPyWrapperSetFactory Instance;
	return Instance;
}

FPyWrapperSet* FPyWrapperSetFactory::FindInstance(void* InUnrealInstance) const
{
	if (!InUnrealInstance)
	{
		return nullptr;
	}

	return FindInstanceInternal(InUnrealInstance, &PyWrapperSetType);
}

FPyWrapperSet* FPyWrapperSetFactory::CreateInstance(void* InUnrealInstance, const USetProperty* InProp, const FPyWrapperOwnerContext& InOwnerContext, const EPyConversionMethod InConversionMethod)
{
	if (!InUnrealInstance)
	{
		return nullptr;
	}

	return CreateInstanceInternal(InUnrealInstance, &PyWrapperSetType, [InUnrealInstance, InProp, &InOwnerContext, InConversionMethod](FPyWrapperSet* InSelf)
	{
		return FPyWrapperSet::Init(InSelf, InOwnerContext, InProp, InUnrealInstance, InConversionMethod);
	}, InConversionMethod == EPyConversionMethod::Copy || InConversionMethod == EPyConversionMethod::Steal);
}


FPyWrapperMapFactory& FPyWrapperMapFactory::Get()
{
	static FPyWrapperMapFactory Instance;
	return Instance;
}

FPyWrapperMap* FPyWrapperMapFactory::FindInstance(void* InUnrealInstance) const
{
	if (!InUnrealInstance)
	{
		return nullptr;
	}

	return FindInstanceInternal(InUnrealInstance, &PyWrapperMapType);
}

FPyWrapperMap* FPyWrapperMapFactory::CreateInstance(void* InUnrealInstance, const UMapProperty* InProp, const FPyWrapperOwnerContext& InOwnerContext, const EPyConversionMethod InConversionMethod)
{
	if (!InUnrealInstance)
	{
		return nullptr;
	}

	return CreateInstanceInternal(InUnrealInstance, &PyWrapperMapType, [InUnrealInstance, InProp, &InOwnerContext, InConversionMethod](FPyWrapperMap* InSelf)
	{
		return FPyWrapperMap::Init(InSelf, InOwnerContext, InProp, InUnrealInstance, InConversionMethod);
	}, InConversionMethod == EPyConversionMethod::Copy || InConversionMethod == EPyConversionMethod::Steal);
}


FPyWrapperTypeReinstancer& FPyWrapperTypeReinstancer::Get()
{
	static FPyWrapperTypeReinstancer Instance;
	return Instance;
}

void FPyWrapperTypeReinstancer::AddPendingClass(UPythonGeneratedClass* OldClass, UPythonGeneratedClass* NewClass)
{
	ClassesToReinstance.Emplace(MakeTuple(OldClass, NewClass));
}

void FPyWrapperTypeReinstancer::AddPendingStruct(UPythonGeneratedStruct* OldStruct, UPythonGeneratedStruct* NewStruct)
{
	StructsToReinstance.Emplace(MakeTuple(OldStruct, NewStruct));
}

void FPyWrapperTypeReinstancer::ProcessPending()
{
	if (ClassesToReinstance.Num() > 0)
	{
		for (const auto& ClassToReinstancePair : ClassesToReinstance)
		{
			FCoreUObjectDelegates::RegisterClassForHotReloadReinstancingDelegate.Broadcast(ClassToReinstancePair.Key, ClassToReinstancePair.Value);
		}
		FCoreUObjectDelegates::ReinstanceHotReloadedClassesDelegate.Broadcast();

		ClassesToReinstance.Reset();
	}

	// todo: need support for re-instancing structs
}


FPyWrapperTypeRegistry::FPyWrapperTypeRegistry()
	: bCanRegisterInlineStructFactories(true)
{
}

FPyWrapperTypeRegistry& FPyWrapperTypeRegistry::Get()
{
	static FPyWrapperTypeRegistry Instance;
	return Instance;
}

void FPyWrapperTypeRegistry::RegisterNativePythonModule(PyGenUtil::FNativePythonModule&& NativePythonModule)
{
	NativePythonModules.Add(MoveTemp(NativePythonModule));
}

void FPyWrapperTypeRegistry::RegisterInlineStructFactory(const TSharedRef<const IPyWrapperInlineStructFactory>& InFactory)
{
	check(bCanRegisterInlineStructFactories);
	InlineStructFactories.Add(InFactory->GetStructName(), InFactory);
}

const IPyWrapperInlineStructFactory* FPyWrapperTypeRegistry::GetInlineStructFactory(const FName StructName) const
{
	return InlineStructFactories.FindRef(StructName).Get();
}

void FPyWrapperTypeRegistry::GenerateWrappedTypes()
{
	FGeneratedWrappedTypeReferences GeneratedWrappedTypeReferences;
	TSet<FName> DirtyModules;

	double GenerateDuration = 0.0;
	{
		FScopedDurationTimer GenerateDurationTimer(GenerateDuration);

		ForEachObjectOfClass(UObject::StaticClass(), [this, &GeneratedWrappedTypeReferences, &DirtyModules](UObject* InObj)
		{
			GenerateWrappedTypeForObject(InObj, GeneratedWrappedTypeReferences, DirtyModules);
		});

		GenerateWrappedTypesForReferences(GeneratedWrappedTypeReferences, DirtyModules);
	}

	NotifyModulesDirtied(DirtyModules);

	UE_LOG(LogPython, Verbose, TEXT("Took %f seconds to generate and initialize Python wrapped types for the initial load."), GenerateDuration);
}

void FPyWrapperTypeRegistry::GenerateWrappedTypesForModule(const FName ModuleName)
{
	UPackage* const ModulePackage = FindPackage(nullptr, *(FString("/Script/") + ModuleName.ToString()));
	if (!ModulePackage)
	{
		return;
	}

	FGeneratedWrappedTypeReferences GeneratedWrappedTypeReferences;
	TSet<FName> DirtyModules;

	double GenerateDuration = 0.0;
	{
		FScopedDurationTimer GenerateDurationTimer(GenerateDuration);

		ForEachObjectWithOuter(ModulePackage, [this, &GeneratedWrappedTypeReferences, &DirtyModules](UObject* InObj)
		{
			GenerateWrappedTypeForObject(InObj, GeneratedWrappedTypeReferences, DirtyModules);
		});

		GenerateWrappedTypesForReferences(GeneratedWrappedTypeReferences, DirtyModules);
	}

	NotifyModulesDirtied(DirtyModules);

	UE_LOG(LogPython, Verbose, TEXT("Took %f seconds to generate and initialize Python wrapped types for '%s'."), GenerateDuration, *ModuleName.ToString());
}

void FPyWrapperTypeRegistry::OrphanWrappedTypesForModule(const FName ModuleName)
{
	TArray<FName> ModuleTypeNames;
	GeneratedWrappedTypesForModule.MultiFind(ModuleName, ModuleTypeNames, true);
	GeneratedWrappedTypesForModule.Remove(ModuleName);

	for (const FName& ModuleTypeName : ModuleTypeNames)
	{
		TSharedPtr<PyGenUtil::FGeneratedWrappedType> GeneratedWrappedType;
		if (GeneratedWrappedTypes.RemoveAndCopyValue(ModuleTypeName, GeneratedWrappedType))
		{
			OrphanedWrappedTypes.Add(GeneratedWrappedType);

			UnregisterPythonTypeName(UTF8_TO_TCHAR(GeneratedWrappedType->PyType.tp_name), ModuleTypeName);

			PythonWrappedClasses.Remove(ModuleTypeName);
			PythonWrappedStructs.Remove(ModuleTypeName);
			PythonWrappedEnums.Remove(ModuleTypeName);
		}
	}
}

void FPyWrapperTypeRegistry::GenerateWrappedTypesForReferences(const FGeneratedWrappedTypeReferences& InGeneratedWrappedTypeReferences, TSet<FName>& OutDirtyModules)
{
	if (!InGeneratedWrappedTypeReferences.HasReferences())
	{
		return;
	}
	
	FGeneratedWrappedTypeReferences GeneratedWrappedTypeReferences;

	for (const UClass* Class : InGeneratedWrappedTypeReferences.ClassReferences)
	{
		GenerateWrappedClassType(Class, GeneratedWrappedTypeReferences, OutDirtyModules, true);
	}

	for (const UScriptStruct* Struct : InGeneratedWrappedTypeReferences.StructReferences)
	{
		GenerateWrappedStructType(Struct, GeneratedWrappedTypeReferences, OutDirtyModules, true);
	}

	for (const UEnum* Enum : InGeneratedWrappedTypeReferences.EnumReferences)
	{
		GenerateWrappedEnumType(Enum, GeneratedWrappedTypeReferences, OutDirtyModules, true);
	}

	for (const UFunction* DelegateSignature : InGeneratedWrappedTypeReferences.DelegateReferences)
	{
		check(DelegateSignature->HasAnyFunctionFlags(FUNC_Delegate));
		GenerateWrappedDelegateType(DelegateSignature, GeneratedWrappedTypeReferences, OutDirtyModules, true);
	}

	GenerateWrappedTypesForReferences(GeneratedWrappedTypeReferences, OutDirtyModules);
}

void FPyWrapperTypeRegistry::NotifyModulesDirtied(const TSet<FName>& InDirtyModules) const
{
	for (const FName& DirtyModule : InDirtyModules)
	{
		const FString PythonModuleName = PyGenUtil::GetModulePythonName(DirtyModule, false);
		OnModuleDirtiedDelegate.Broadcast(*PythonModuleName);
	}
}

PyTypeObject* FPyWrapperTypeRegistry::GenerateWrappedTypeForObject(const UObject* InObj, FGeneratedWrappedTypeReferences& OutGeneratedWrappedTypeReferences, TSet<FName>& OutDirtyModules, const bool bForceGenerate)
{
	if (const UClass* Class = Cast<const UClass>(InObj))
	{
		return GenerateWrappedClassType(Class, OutGeneratedWrappedTypeReferences, OutDirtyModules, bForceGenerate);
	}

	if (const UScriptStruct* Struct = Cast<const UScriptStruct>(InObj))
	{
		return GenerateWrappedStructType(Struct, OutGeneratedWrappedTypeReferences, OutDirtyModules, bForceGenerate);
	}

	if (const UEnum* Enum = Cast<const UEnum>(InObj))
	{
		return GenerateWrappedEnumType(Enum, OutGeneratedWrappedTypeReferences, OutDirtyModules, bForceGenerate);
	}

	if (const UFunction* Func = Cast<const UFunction>(InObj))
	{
		if (Func->HasAnyFunctionFlags(FUNC_Delegate))
		{
			return GenerateWrappedDelegateType(Func, OutGeneratedWrappedTypeReferences, OutDirtyModules, bForceGenerate);
		}
	}

	return nullptr;
}

PyTypeObject* FPyWrapperTypeRegistry::GenerateWrappedClassType(const UClass* InClass, FGeneratedWrappedTypeReferences& OutGeneratedWrappedTypeReferences, TSet<FName>& OutDirtyModules, const bool bForceGenerate)
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_GenerateWrappedClassTotalTime);
	INC_DWORD_STAT(STAT_GenerateWrappedClassCallCount);

	// Already processed? Nothing more to do
	if (PyTypeObject* ExistingPyType = PythonWrappedClasses.FindRef(InClass->GetFName()))
	{
		return ExistingPyType;
	}

	// todo: allow generation of Blueprint generated classes
	if (PyGenUtil::IsBlueprintGeneratedClass(InClass))
	{
		return nullptr;
	}

	// Should this type be exported?
	if (!bForceGenerate && !PyGenUtil::ShouldExportClass(InClass))
	{
		return nullptr;
	}

	// Make sure the parent class is also wrapped
	PyTypeObject* SuperPyType = nullptr;
	if (const UClass* SuperClass = InClass->GetSuperClass())
	{
		SuperPyType = GenerateWrappedClassType(SuperClass, OutGeneratedWrappedTypeReferences, OutDirtyModules, true);
	}

	INC_DWORD_STAT(STAT_GenerateWrappedClassObjCount);

	check(!GeneratedWrappedTypes.Contains(InClass->GetFName()));
	TSharedRef<PyGenUtil::FGeneratedWrappedClassType> GeneratedWrappedType = MakeShared<PyGenUtil::FGeneratedWrappedClassType>();
	GeneratedWrappedTypes.Add(InClass->GetFName(), GeneratedWrappedType);

	TMap<FName, FName> PythonProperties;
	TMap<FName, FString> PythonDeprecatedProperties;
	TMap<FName, FName> PythonMethods;
	TMap<FName, FString> PythonDeprecatedMethods;

	auto GenerateWrappedProperty = [this, InClass, &PythonProperties, &PythonDeprecatedProperties, &GeneratedWrappedType, &OutGeneratedWrappedTypeReferences](const UProperty* InProp)
	{
		const bool bExportPropertyToScript = PyGenUtil::ShouldExportProperty(InProp);
		const bool bExportPropertyToEditor = PyGenUtil::ShouldExportEditorOnlyProperty(InProp);

		if (bExportPropertyToScript || bExportPropertyToEditor)
		{
			GatherWrappedTypesForPropertyReferences(InProp, OutGeneratedWrappedTypeReferences);

			const PyGenUtil::FGeneratedWrappedPropertyDoc& GeneratedPropertyDoc = GeneratedWrappedType->PropertyDocs[GeneratedWrappedType->PropertyDocs.Emplace(InProp)];
			PythonProperties.Add(*GeneratedPropertyDoc.PythonPropName, InProp->GetFName());

			int32 GeneratedWrappedGetSetIndex = INDEX_NONE;
			if (bExportPropertyToScript)
			{
				GeneratedWrappedGetSetIndex = GeneratedWrappedType->GetSets.TypeGetSets.AddDefaulted();

				auto FindGetSetFunction = [InClass, InProp](const FName& InKey) -> const UFunction*
				{
					const FString GetSetName = InProp->GetMetaData(InKey);
					if (!GetSetName.IsEmpty())
					{
						const UFunction* GetSetFunc = InClass->FindFunctionByName(*GetSetName);
						if (!GetSetFunc)
						{
							REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Property '%s.%s' is marked as '%s' but the function '%s' could not be found."), *InClass->GetName(), *InProp->GetName(), *InKey.ToString(), *GetSetName);
						}
						return GetSetFunc;
					}
					return nullptr;
				};

				PyGenUtil::FGeneratedWrappedGetSet& GeneratedWrappedGetSet = GeneratedWrappedType->GetSets.TypeGetSets[GeneratedWrappedGetSetIndex];
				GeneratedWrappedGetSet.GetSetName = PyGenUtil::TCHARToUTF8Buffer(*GeneratedPropertyDoc.PythonPropName);
				GeneratedWrappedGetSet.GetSetDoc = PyGenUtil::TCHARToUTF8Buffer(*GeneratedPropertyDoc.DocString);
				GeneratedWrappedGetSet.Prop.SetProperty(InProp);
				GeneratedWrappedGetSet.GetFunc.SetFunction(FindGetSetFunction(PyGenUtil::BlueprintGetterMetaDataKey));
				GeneratedWrappedGetSet.SetFunc.SetFunction(FindGetSetFunction(PyGenUtil::BlueprintSetterMetaDataKey));
				GeneratedWrappedGetSet.GetCallback = (getter)&FPyWrapperObject::Getter_Impl;
				GeneratedWrappedGetSet.SetCallback = (setter)&FPyWrapperObject::Setter_Impl;
				if (GeneratedWrappedGetSet.Prop.DeprecationMessage.IsSet())
				{
					PythonDeprecatedProperties.Add(*GeneratedPropertyDoc.PythonPropName, GeneratedWrappedGetSet.Prop.DeprecationMessage.GetValue());
				}

				GeneratedWrappedType->FieldTracker.RegisterPythonFieldName(GeneratedPropertyDoc.PythonPropName, InProp);
			}

			const TArray<FString> DeprecatedPythonPropNames = PyGenUtil::GetDeprecatedPropertyPythonNames(InProp);
			for (const FString& DeprecatedPythonPropName : DeprecatedPythonPropNames)
			{
				FString DeprecationMessage = FString::Printf(TEXT("'%s' was renamed to '%s'."), *DeprecatedPythonPropName, *GeneratedPropertyDoc.PythonPropName);
				PythonProperties.Add(*DeprecatedPythonPropName, InProp->GetFName());
				PythonDeprecatedProperties.Add(*DeprecatedPythonPropName, DeprecationMessage);

				if (GeneratedWrappedGetSetIndex != INDEX_NONE)
				{
					PyGenUtil::FGeneratedWrappedGetSet DeprecatedGeneratedWrappedGetSet = GeneratedWrappedType->GetSets.TypeGetSets[GeneratedWrappedGetSetIndex];
					DeprecatedGeneratedWrappedGetSet.GetSetName = PyGenUtil::TCHARToUTF8Buffer(*DeprecatedPythonPropName);
					DeprecatedGeneratedWrappedGetSet.GetSetDoc = PyGenUtil::TCHARToUTF8Buffer(*FString::Printf(TEXT("deprecated: %s"), *DeprecationMessage));
					DeprecatedGeneratedWrappedGetSet.Prop.DeprecationMessage = MoveTemp(DeprecationMessage);
					GeneratedWrappedType->GetSets.TypeGetSets.Add(MoveTemp(DeprecatedGeneratedWrappedGetSet));

					GeneratedWrappedType->FieldTracker.RegisterPythonFieldName(DeprecatedPythonPropName, InProp);
				}
			}
		}
	};

	auto GenerateWrappedDynamicMethod = [this, &OutGeneratedWrappedTypeReferences, &OutDirtyModules](const UFunction* InFunc, const PyGenUtil::FGeneratedWrappedMethod& InTypeMethod)
	{
		// Only static functions can be hoisted onto other types
		if (!InFunc->HasAnyFunctionFlags(FUNC_Static))
		{
			REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Non-static function '%s.%s' is marked as 'ScriptMethod' but only static functions can be hoisted."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName());
			return;
		}

		// Get the type to hoist this method to (this should be the first parameter)
		PyGenUtil::FGeneratedWrappedMethodParameter SelfParam;
		if (InTypeMethod.MethodFunc.InputParams.Num() > 0 && (InTypeMethod.MethodFunc.InputParams[0].ParamProp->IsA<UStructProperty>() || InTypeMethod.MethodFunc.InputParams[0].ParamProp->IsA<UObjectPropertyBase>()))
		{
			SelfParam = InTypeMethod.MethodFunc.InputParams[0];
		}
		if (!SelfParam.ParamProp)
		{
			REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Function '%s.%s' is marked as 'ScriptMethod' but doesn't contain a valid struct or object as its first argument."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName());
			return;
		}
		if (const UObjectPropertyBase* SelfPropObj = Cast<UObjectPropertyBase>(SelfParam.ParamProp))
		{
			if (SelfPropObj->GetClass()->IsChildOf(InFunc->GetOwnerClass()))
			{
				REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Function '%s.%s' is marked as 'ScriptMethod' but the object argument type (%s) is a child of the the class type of the static function. This is not allowed."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName(), *SelfPropObj->GetClass()->GetName());
				return;
			}
		}

		const FString PythonStructMethodName = PyGenUtil::GetScriptMethodPythonName(InFunc);
		TArray<PyGenUtil::FGeneratedWrappedDynamicMethod, TInlineAllocator<4>> DynamicMethodDefs;

		// Copy the basic wrapped method as we need to adjust some parts of it below
		PyGenUtil::FGeneratedWrappedDynamicMethod& GeneratedWrappedDynamicMethod = DynamicMethodDefs.AddDefaulted_GetRef();
		static_cast<PyGenUtil::FGeneratedWrappedMethod&>(GeneratedWrappedDynamicMethod) = InTypeMethod;
		GeneratedWrappedDynamicMethod.SelfParam = SelfParam;

		// Hoisted methods may use an optional name alias
		GeneratedWrappedDynamicMethod.MethodName = PyGenUtil::TCHARToUTF8Buffer(*PythonStructMethodName);

		// We remove the first function parameter, as that's the 'self' argument and we'll infer that when we call
		GeneratedWrappedDynamicMethod.MethodFunc.InputParams.RemoveAt(0, 1, /*bAllowShrinking*/false);

		// Reference parameters may lead to a 'self' parameter that is also an output parameter
		// In this case we need to remove the output too, and set it as our 'self' return (which will apply the result back onto 'self')
		if (PyUtil::IsOutputParameter(SelfParam.ParamProp))
		{
			for (auto OutParamIt = GeneratedWrappedDynamicMethod.MethodFunc.OutputParams.CreateIterator(); OutParamIt; ++OutParamIt)
			{
				if (SelfParam.ParamProp == OutParamIt->ParamProp)
				{
					GeneratedWrappedDynamicMethod.SelfReturn = MoveTemp(*OutParamIt);
					OutParamIt.RemoveCurrent();
					break;
				}
			}
		}

		// The function may also have been flagged as having a 'self' return
		if (InFunc->HasMetaData(PyGenUtil::ScriptMethodSelfReturnMetaDataKey))
		{
			if (GeneratedWrappedDynamicMethod.SelfReturn.ParamProp)
			{
				REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Function '%s.%s' is marked as 'ScriptMethodSelfReturn' but the 'self' argument is also marked as UPARAM(ref). This is not allowed."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName());
				return;
			}
			else if (GeneratedWrappedDynamicMethod.MethodFunc.OutputParams.Num() == 0 || !GeneratedWrappedDynamicMethod.MethodFunc.OutputParams[0].ParamProp->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Function '%s.%s' is marked as 'ScriptMethodSelfReturn' but has no return value."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName());
				return;
			}
			else if (!SelfParam.ParamProp->IsA<UStructProperty>())
			{
				REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Function '%s.%s' is marked as 'ScriptMethodSelfReturn' but the 'self' argument is not a struct."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName());
				return;
			}
			else if (!GeneratedWrappedDynamicMethod.MethodFunc.OutputParams[0].ParamProp->IsA<UStructProperty>())
			{
				REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Function '%s.%s' is marked as 'ScriptMethodSelfReturn' but the return value is not a struct."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName());
				return;
			}
			else if (CastChecked<UStructProperty>(GeneratedWrappedDynamicMethod.MethodFunc.OutputParams[0].ParamProp)->Struct != CastChecked<UStructProperty>(SelfParam.ParamProp)->Struct)
			{
				REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Function '%s.%s' is marked as 'ScriptMethodSelfReturn' but the return value is not the same type as the 'self' argument."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName());
				return;
			}
			else
			{
				GeneratedWrappedDynamicMethod.SelfReturn = MoveTemp(GeneratedWrappedDynamicMethod.MethodFunc.OutputParams[0]);
				GeneratedWrappedDynamicMethod.MethodFunc.OutputParams.RemoveAt(0, 1, /*bAllowShrinking*/false);
			}
		}

		// Set-up some data needed to build the tooltip correctly for the hoisted method
		const bool bIsStaticOverride = false;
		TSet<FName> ParamsToIgnore;
		ParamsToIgnore.Add(SelfParam.ParamProp->GetFName());

		// Update the doc string for the method
		FString PythonStructMethodDocString = PyGenUtil::BuildFunctionDocString(InFunc, PythonStructMethodName, GeneratedWrappedDynamicMethod.MethodFunc.InputParams, GeneratedWrappedDynamicMethod.MethodFunc.OutputParams, &bIsStaticOverride);
		PythonStructMethodDocString += LINE_TERMINATOR;
		PythonStructMethodDocString += PyGenUtil::PythonizeFunctionTooltip(PyGenUtil::GetFieldTooltip(InFunc), InFunc, ParamsToIgnore);
		GeneratedWrappedDynamicMethod.MethodDoc = PyGenUtil::TCHARToUTF8Buffer(*PythonStructMethodDocString);

		// Update the flags as removing the 'self' argument may have changed the calling convention
		GeneratedWrappedDynamicMethod.MethodFlags = GeneratedWrappedDynamicMethod.MethodFunc.InputParams.Num() > 0 ? METH_VARARGS | METH_KEYWORDS : METH_NOARGS;

		// Set the correct function pointer for calling this function and inject the 'self' argument
		GeneratedWrappedDynamicMethod.MethodCallback = nullptr;
		if (SelfParam.ParamProp->IsA<UObjectPropertyBase>())
		{
			GeneratedWrappedDynamicMethod.MethodCallback = GeneratedWrappedDynamicMethod.MethodFunc.InputParams.Num() > 0 ? PyCFunctionWithClosureCast(&FPyWrapperObject::CallDynamicMethodWithArgs_Impl) : PyCFunctionWithClosureCast(&FPyWrapperObject::CallDynamicMethodNoArgs_Impl);
		}
		else if (SelfParam.ParamProp->IsA<UStructProperty>())
		{
			GeneratedWrappedDynamicMethod.MethodCallback = GeneratedWrappedDynamicMethod.MethodFunc.InputParams.Num() > 0 ? PyCFunctionWithClosureCast(&FPyWrapperStruct::CallDynamicMethodWithArgs_Impl) : PyCFunctionWithClosureCast(&FPyWrapperStruct::CallDynamicMethodNoArgs_Impl);
		}

		// Add any deprecated variants too
		const TArray<FString> DeprecatedPythonStructMethodNames = PyGenUtil::GetDeprecatedScriptMethodPythonNames(InFunc);
		for (const FString& DeprecatedPythonStructMethodName : DeprecatedPythonStructMethodNames)
		{
			FString DeprecationMessage = FString::Printf(TEXT("'%s' was renamed to '%s'."), *DeprecatedPythonStructMethodName, *PythonStructMethodName);

			PyGenUtil::FGeneratedWrappedDynamicMethod& DeprecatedGeneratedWrappedMethod = DynamicMethodDefs.AddDefaulted_GetRef();
			DeprecatedGeneratedWrappedMethod = GeneratedWrappedDynamicMethod;
			DeprecatedGeneratedWrappedMethod.MethodName = PyGenUtil::TCHARToUTF8Buffer(*DeprecatedPythonStructMethodName);
			DeprecatedGeneratedWrappedMethod.MethodDoc = PyGenUtil::TCHARToUTF8Buffer(*FString::Printf(TEXT("deprecated: %s"), *DeprecationMessage));
			DeprecatedGeneratedWrappedMethod.MethodFunc.DeprecationMessage = MoveTemp(DeprecationMessage);
		}

		// Add the dynamic method to either the owner type
		if (SelfParam.ParamProp->IsA<UObjectPropertyBase>())
		{
			// Ensure that we've generated a finalized Python type for this class since we'll be adding this function as a dynamic method on that type
			const UClass* HostedClass = CastChecked<UObjectPropertyBase>(SelfParam.ParamProp)->PropertyClass;
			if (!GenerateWrappedClassType(HostedClass, OutGeneratedWrappedTypeReferences, OutDirtyModules, true))
			{
				return;
			}

			// Find the wrapped type for the class as that's what we'll actually add the dynamic method to
			TSharedPtr<PyGenUtil::FGeneratedWrappedClassType> HostedClassGeneratedWrappedType = StaticCastSharedPtr<PyGenUtil::FGeneratedWrappedClassType>(GeneratedWrappedTypes.FindRef(HostedClass->GetFName()));
			check(HostedClassGeneratedWrappedType.IsValid());

			// Add the dynamic methods to the class
			for (PyGenUtil::FGeneratedWrappedDynamicMethod& GeneratedWrappedDynamicMethodToAdd : DynamicMethodDefs)
			{
				HostedClassGeneratedWrappedType->FieldTracker.RegisterPythonFieldName(UTF8_TO_TCHAR(GeneratedWrappedDynamicMethodToAdd.MethodName.GetData()), InFunc);
				HostedClassGeneratedWrappedType->AddDynamicMethod(MoveTemp(GeneratedWrappedDynamicMethodToAdd));
			}
		}
		else if (SelfParam.ParamProp->IsA<UStructProperty>())
		{
			// Ensure that we've generated a finalized Python type for this struct since we'll be adding this function as a dynamic method on that type
			const UScriptStruct* HostedStruct = CastChecked<UStructProperty>(SelfParam.ParamProp)->Struct;
			if (!GenerateWrappedStructType(HostedStruct, OutGeneratedWrappedTypeReferences, OutDirtyModules, true))
			{
				return;
			}

			// Find the wrapped type for the struct as that's what we'll actually add the dynamic method to
			TSharedPtr<PyGenUtil::FGeneratedWrappedStructType> HostedStructGeneratedWrappedType = StaticCastSharedPtr<PyGenUtil::FGeneratedWrappedStructType>(GeneratedWrappedTypes.FindRef(HostedStruct->GetFName()));
			check(HostedStructGeneratedWrappedType.IsValid());

			// Add the dynamic methods to the struct
			for (PyGenUtil::FGeneratedWrappedDynamicMethod& GeneratedWrappedDynamicMethodToAdd : DynamicMethodDefs)
			{
				HostedStructGeneratedWrappedType->FieldTracker.RegisterPythonFieldName(UTF8_TO_TCHAR(GeneratedWrappedDynamicMethodToAdd.MethodName.GetData()), InFunc);
				HostedStructGeneratedWrappedType->AddDynamicMethod(MoveTemp(GeneratedWrappedDynamicMethodToAdd));
			}
		}
		else
		{
			checkf(false, TEXT("Unexpected SelfParam type!"));
		}
	};

	auto GenerateWrappedOperator = [this, &OutGeneratedWrappedTypeReferences, &OutDirtyModules](const UFunction* InFunc, const PyGenUtil::FGeneratedWrappedMethod& InTypeMethod)
	{
		// Only static functions can be hoisted onto other types
		if (!InFunc->HasAnyFunctionFlags(FUNC_Static))
		{
			REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Non-static function '%s.%s' is marked as 'ScriptOperator' but only static functions can be hoisted."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName());
			return;
		}

		// Get the list of operators to apply this function to
		TArray<FString> ScriptOperators;
		{
			const FString& ScriptOperatorsStr = InFunc->GetMetaData(PyGenUtil::ScriptOperatorMetaDataKey);
			ScriptOperatorsStr.ParseIntoArray(ScriptOperators, TEXT(";"));
		}

		// Go through and try and create a function for each operator, validating that the signature matches what the operator expects
		for (const FString& ScriptOperator : ScriptOperators)
		{
			PyGenUtil::FGeneratedWrappedOperatorSignature OpSignature;
			if (!PyGenUtil::FGeneratedWrappedOperatorSignature::StringToSignature(*ScriptOperator, OpSignature))
			{
				REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Function '%s.%s' is marked as 'ScriptOperator' but uses an unknown operator type '%s'."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName(), *ScriptOperator);
				continue;
			}

			PyGenUtil::FGeneratedWrappedOperatorFunction OpFunc;
			{
				FString SignatureError;
				if (!OpFunc.SetFunction(InTypeMethod.MethodFunc, OpSignature, &SignatureError))
				{
					REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Function '%s.%s' is marked as 'ScriptOperator' but has an invalid signature for the '%s' operator: %s."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName(), *ScriptOperator, *SignatureError);
					continue;
				}
			}

			// Ensure that we've generated a finalized Python type for this struct since we'll be adding this function as a operator on that type
			const UScriptStruct* HostedStruct = CastChecked<UStructProperty>(OpFunc.SelfParam.ParamProp)->Struct;
			if (GenerateWrappedStructType(HostedStruct, OutGeneratedWrappedTypeReferences, OutDirtyModules, true))
			{
				// Find the wrapped type for the struct as that's what we'll actually add the operator to (via its meta-data)
				TSharedPtr<PyGenUtil::FGeneratedWrappedStructType> HostedStructGeneratedWrappedType = StaticCastSharedPtr<PyGenUtil::FGeneratedWrappedStructType>(GeneratedWrappedTypes.FindRef(HostedStruct->GetFName()));
				check(HostedStructGeneratedWrappedType.IsValid());
				StaticCastSharedPtr<FPyWrapperStructMetaData>(HostedStructGeneratedWrappedType->MetaData)->OpStacks[(int32)OpSignature.OpType].Funcs.Add(MoveTemp(OpFunc));
			}
		}
	};

	auto GenerateWrappedConstant = [this, &GeneratedWrappedType, &OutGeneratedWrappedTypeReferences, &OutDirtyModules](const UFunction* InFunc)
	{
		// Only static functions can be constants
		if (!InFunc->HasAnyFunctionFlags(FUNC_Static))
		{
			REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Non-static function '%s.%s' is marked as 'ScriptConstant' but only static functions can be hoisted."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName());
			return;
		}

		// We might want to hoist this function onto another type rather than its owner class
		const UObject* HostType = nullptr;
		if (InFunc->HasMetaData(PyGenUtil::ScriptConstantHostMetaDataKey))
		{
			const FString ConstantOwnerName = InFunc->GetMetaData(PyGenUtil::ScriptConstantHostMetaDataKey);
			HostType = FindObject<UStruct>(ANY_PACKAGE, *ConstantOwnerName);
			if (HostType && !(HostType->IsA<UClass>() || HostType->IsA<UScriptStruct>()))
			{
				HostType = nullptr;
			}
			if (!HostType)
			{
				REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Function '%s.%s' is marked as 'ScriptConstantHost' but the host '%s' could not be found."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName(), *ConstantOwnerName);
				return;
			}
		}
		if (const UClass* HostClass = Cast<UClass>(HostType))
		{
			if (HostClass->IsChildOf(InFunc->GetOwnerClass()))
			{
				REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Function '%s.%s' is marked as 'ScriptConstantHost' but the host type (%s) is a child of the the class type of the static function. This is not allowed."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName(), *HostClass->GetName());
				return;
			}
		}

		// Verify that the function signature is valid
		PyGenUtil::FGeneratedWrappedFunction ConstantFunc;
		ConstantFunc.SetFunction(InFunc);
		if (ConstantFunc.InputParams.Num() != 0 || ConstantFunc.OutputParams.Num() != 1)
		{
			REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Function '%s.%s' is marked as 'ScriptConstant' but has an invalid signature (it must return a value and take no arguments)."), *InFunc->GetOwnerClass()->GetName(), *InFunc->GetName());
			return;
		}

		const FString PythonConstantName = PyGenUtil::GetScriptConstantPythonName(InFunc);
		TArray<PyGenUtil::FGeneratedWrappedConstant, TInlineAllocator<4>> ConstantDefs;

		// Build the constant definition
		PyGenUtil::FGeneratedWrappedConstant& GeneratedWrappedConstant = ConstantDefs.AddDefaulted_GetRef();
		GeneratedWrappedConstant.ConstantName = PyGenUtil::TCHARToUTF8Buffer(*PythonConstantName);
		GeneratedWrappedConstant.ConstantDoc = PyGenUtil::TCHARToUTF8Buffer(*FString::Printf(TEXT("(%s): %s"), *PyGenUtil::GetPropertyPythonType(ConstantFunc.OutputParams[0].ParamProp), *PyGenUtil::GetFieldTooltip(InFunc)));
		GeneratedWrappedConstant.ConstantFunc = ConstantFunc;

		// Build any deprecated variants too
		const TArray<FString> DeprecatedPythonConstantNames = PyGenUtil::GetDeprecatedScriptConstantPythonNames(InFunc);
		for (const FString& DeprecatedPythonConstantName : DeprecatedPythonConstantNames)
		{
			FString DeprecationMessage = FString::Printf(TEXT("'%s' was renamed to '%s'."), *DeprecatedPythonConstantName, *PythonConstantName);

			PyGenUtil::FGeneratedWrappedConstant& DeprecatedGeneratedWrappedConstant = ConstantDefs.AddDefaulted_GetRef();
			DeprecatedGeneratedWrappedConstant = GeneratedWrappedConstant;
			DeprecatedGeneratedWrappedConstant.ConstantName = PyGenUtil::TCHARToUTF8Buffer(*DeprecatedPythonConstantName);
			DeprecatedGeneratedWrappedConstant.ConstantDoc = PyGenUtil::TCHARToUTF8Buffer(*FString::Printf(TEXT("deprecated: %s"), *DeprecationMessage));
		}

		// Add the constant to either the owner type (if specified) or this class
		if (HostType)
		{
			if (HostType->IsA<UClass>())
			{
				const UClass* HostClass = CastChecked<UClass>(HostType);

				// Ensure that we've generated a finalized Python type for this class since we'll be adding this constant to that type
				if (!GenerateWrappedClassType(HostClass, OutGeneratedWrappedTypeReferences, OutDirtyModules, true))
				{
					return;
				}

				// Find the wrapped type for the class as that's what we'll actually add the constant to
				TSharedPtr<PyGenUtil::FGeneratedWrappedClassType> HostedClassGeneratedWrappedType = StaticCastSharedPtr<PyGenUtil::FGeneratedWrappedClassType>(GeneratedWrappedTypes.FindRef(HostClass->GetFName()));
				check(HostedClassGeneratedWrappedType.IsValid());

				// Add the dynamic constants to the struct
				for (PyGenUtil::FGeneratedWrappedConstant& GeneratedWrappedConstantToAdd : ConstantDefs)
				{
					HostedClassGeneratedWrappedType->FieldTracker.RegisterPythonFieldName(UTF8_TO_TCHAR(GeneratedWrappedConstantToAdd.ConstantName.GetData()), InFunc);
					HostedClassGeneratedWrappedType->AddDynamicConstant(MoveTemp(GeneratedWrappedConstantToAdd));
				}
			}
			else if (HostType->IsA<UScriptStruct>())
			{
				const UScriptStruct* HostStruct = CastChecked<UScriptStruct>(HostType);

				// Ensure that we've generated a finalized Python type for this struct since we'll be adding this constant to that type
				if (!GenerateWrappedStructType(HostStruct, OutGeneratedWrappedTypeReferences, OutDirtyModules, true))
				{
					return;
				}

				// Find the wrapped type for the struct as that's what we'll actually add the constant to
				TSharedPtr<PyGenUtil::FGeneratedWrappedStructType> HostedStructGeneratedWrappedType = StaticCastSharedPtr<PyGenUtil::FGeneratedWrappedStructType>(GeneratedWrappedTypes.FindRef(HostStruct->GetFName()));
				check(HostedStructGeneratedWrappedType.IsValid());

				// Add the dynamic constants to the struct
				for (PyGenUtil::FGeneratedWrappedConstant& GeneratedWrappedConstantToAdd : ConstantDefs)
				{
					HostedStructGeneratedWrappedType->FieldTracker.RegisterPythonFieldName(UTF8_TO_TCHAR(GeneratedWrappedConstantToAdd.ConstantName.GetData()), InFunc);
					HostedStructGeneratedWrappedType->AddDynamicConstant(MoveTemp(GeneratedWrappedConstantToAdd));
				}
			}
			else
			{
				checkf(false, TEXT("Unexpected HostType type!"));
			}
		}
		else
		{
			// Add the static constants to this type
			for (PyGenUtil::FGeneratedWrappedConstant& GeneratedWrappedConstantToAdd : ConstantDefs)
			{
				GeneratedWrappedType->FieldTracker.RegisterPythonFieldName(UTF8_TO_TCHAR(GeneratedWrappedConstantToAdd.ConstantName.GetData()), InFunc);
				GeneratedWrappedType->Constants.TypeConstants.Add(MoveTemp(GeneratedWrappedConstantToAdd));
			}
		}
	};

	auto GenerateWrappedMethod = [this, &PythonMethods, &PythonDeprecatedMethods, &GeneratedWrappedType, &OutGeneratedWrappedTypeReferences, &GenerateWrappedDynamicMethod, &GenerateWrappedOperator, &GenerateWrappedConstant](const UFunction* InFunc)
	{
		if (!PyGenUtil::ShouldExportFunction(InFunc))
		{
			return;
		}

		for (TFieldIterator<const UProperty> ParamIt(InFunc); ParamIt; ++ParamIt)
		{
			const UProperty* Param = *ParamIt;
			GatherWrappedTypesForPropertyReferences(Param, OutGeneratedWrappedTypeReferences);
		}

		// Constant functions do not export as real functions, so bail once we've generated their wrapped constant data
		if (InFunc->HasMetaData(PyGenUtil::ScriptConstantMetaDataKey))
		{
			GenerateWrappedConstant(InFunc);
			return;
		}

		const FString PythonFunctionName = PyGenUtil::GetFunctionPythonName(InFunc);
		const bool bIsStatic = InFunc->HasAnyFunctionFlags(FUNC_Static);
		
		PythonMethods.Add(*PythonFunctionName, InFunc->GetFName());

		PyGenUtil::FGeneratedWrappedMethod& GeneratedWrappedMethod = GeneratedWrappedType->Methods.TypeMethods.AddDefaulted_GetRef();
		GeneratedWrappedMethod.MethodName = PyGenUtil::TCHARToUTF8Buffer(*PythonFunctionName);
		GeneratedWrappedMethod.MethodFunc.SetFunction(InFunc);
		if (GeneratedWrappedMethod.MethodFunc.DeprecationMessage.IsSet())
		{
			PythonDeprecatedMethods.Add(*PythonFunctionName, GeneratedWrappedMethod.MethodFunc.DeprecationMessage.GetValue());
		}

		GeneratedWrappedType->FieldTracker.RegisterPythonFieldName(PythonFunctionName, InFunc);

		FString FunctionDeclDocString = PyGenUtil::BuildFunctionDocString(InFunc, PythonFunctionName, GeneratedWrappedMethod.MethodFunc.InputParams, GeneratedWrappedMethod.MethodFunc.OutputParams);
		FunctionDeclDocString += LINE_TERMINATOR;
		FunctionDeclDocString += PyGenUtil::PythonizeFunctionTooltip(PyGenUtil::GetFieldTooltip(InFunc), InFunc);

		GeneratedWrappedMethod.MethodDoc = PyGenUtil::TCHARToUTF8Buffer(*FunctionDeclDocString);
		GeneratedWrappedMethod.MethodFlags = GeneratedWrappedMethod.MethodFunc.InputParams.Num() > 0 ? METH_VARARGS | METH_KEYWORDS : METH_NOARGS;
		if (bIsStatic)
		{
			GeneratedWrappedMethod.MethodFlags |= METH_CLASS;
			GeneratedWrappedMethod.MethodCallback = GeneratedWrappedMethod.MethodFunc.InputParams.Num() > 0 ? PyCFunctionWithClosureCast(&FPyWrapperObject::CallClassMethodWithArgs_Impl) : PyCFunctionWithClosureCast(&FPyWrapperObject::CallClassMethodNoArgs_Impl);
		}
		else
		{
			GeneratedWrappedMethod.MethodCallback = GeneratedWrappedMethod.MethodFunc.InputParams.Num() > 0 ? PyCFunctionWithClosureCast(&FPyWrapperObject::CallMethodWithArgs_Impl) : PyCFunctionWithClosureCast(&FPyWrapperObject::CallMethodNoArgs_Impl);
		}

		const TArray<FString> DeprecatedPythonFuncNames = PyGenUtil::GetDeprecatedFunctionPythonNames(InFunc);
		for (const FString& DeprecatedPythonFuncName : DeprecatedPythonFuncNames)
		{
			FString DeprecationMessage = FString::Printf(TEXT("'%s' was renamed to '%s'."), *DeprecatedPythonFuncName, *PythonFunctionName);
			PythonMethods.Add(*DeprecatedPythonFuncName, InFunc->GetFName());
			PythonDeprecatedMethods.Add(*DeprecatedPythonFuncName, DeprecationMessage);

			PyGenUtil::FGeneratedWrappedMethod DeprecatedGeneratedWrappedMethod = GeneratedWrappedMethod;
			DeprecatedGeneratedWrappedMethod.MethodName = PyGenUtil::TCHARToUTF8Buffer(*DeprecatedPythonFuncName);
			DeprecatedGeneratedWrappedMethod.MethodDoc = PyGenUtil::TCHARToUTF8Buffer(*FString::Printf(TEXT("deprecated: %s"), *DeprecationMessage));
			DeprecatedGeneratedWrappedMethod.MethodFunc.DeprecationMessage = MoveTemp(DeprecationMessage);
			GeneratedWrappedType->Methods.TypeMethods.Add(MoveTemp(DeprecatedGeneratedWrappedMethod));

			GeneratedWrappedType->FieldTracker.RegisterPythonFieldName(DeprecatedPythonFuncName, InFunc);
		}

		// Should this function also be hoisted as a struct method or operator?
		if (InFunc->HasMetaData(PyGenUtil::ScriptMethodMetaDataKey))
		{
			GenerateWrappedDynamicMethod(InFunc, GeneratedWrappedMethod);
		}
		if (InFunc->HasMetaData(PyGenUtil::ScriptOperatorMetaDataKey))
		{
			GenerateWrappedOperator(InFunc, GeneratedWrappedMethod);
		}
	};

	const FString PythonClassName = PyGenUtil::GetClassPythonName(InClass);
	GeneratedWrappedType->TypeName = PyGenUtil::TCHARToUTF8Buffer(*PythonClassName);

	for (TFieldIterator<const UField> FieldIt(InClass, EFieldIteratorFlags::ExcludeSuper); FieldIt; ++FieldIt)
	{
		if (const UProperty* Prop = Cast<const UProperty>(*FieldIt))
		{
			GenerateWrappedProperty(Prop);
			continue;
		}

		if (const UFunction* Func = Cast<const UFunction>(*FieldIt))
		{
			GenerateWrappedMethod(Func);
			continue;
		}
	}

	FString TypeDocString = PyGenUtil::PythonizeTooltip(PyGenUtil::GetFieldTooltip(InClass));
	if (const UClass* SuperClass = InClass->GetSuperClass())
	{
		TSharedPtr<PyGenUtil::FGeneratedWrappedClassType> SuperGeneratedWrappedType = StaticCastSharedPtr<PyGenUtil::FGeneratedWrappedClassType>(GeneratedWrappedTypes.FindRef(SuperClass->GetFName()));
		if (SuperGeneratedWrappedType.IsValid())
		{
			GeneratedWrappedType->PropertyDocs.Append(SuperGeneratedWrappedType->PropertyDocs);
		}
	}
	GeneratedWrappedType->PropertyDocs.Sort(&PyGenUtil::FGeneratedWrappedPropertyDoc::SortPredicate);
	PyGenUtil::AppendCppSourceInformationDocString(InClass, TypeDocString);
	PyGenUtil::FGeneratedWrappedPropertyDoc::AppendDocString(GeneratedWrappedType->PropertyDocs, TypeDocString);
	GeneratedWrappedType->TypeDoc = PyGenUtil::TCHARToUTF8Buffer(*TypeDocString);

	GeneratedWrappedType->PyType.tp_basicsize = sizeof(FPyWrapperObject);
	GeneratedWrappedType->PyType.tp_base = SuperPyType ? SuperPyType : &PyWrapperObjectType;
	GeneratedWrappedType->PyType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;

	TSharedRef<FPyWrapperObjectMetaData> ObjectMetaData = MakeShared<FPyWrapperObjectMetaData>();
	ObjectMetaData->Class = (UClass*)InClass;
	ObjectMetaData->PythonProperties = MoveTemp(PythonProperties);
	ObjectMetaData->PythonDeprecatedProperties = MoveTemp(PythonDeprecatedProperties);
	ObjectMetaData->PythonMethods = MoveTemp(PythonMethods);
	ObjectMetaData->PythonDeprecatedMethods = MoveTemp(PythonDeprecatedMethods);
	{
		FString DeprecationMessageStr;
		if (PyGenUtil::IsDeprecatedClass(InClass, &DeprecationMessageStr))
		{
			ObjectMetaData->DeprecationMessage = MoveTemp(DeprecationMessageStr);
		}
	}
	GeneratedWrappedType->MetaData = ObjectMetaData;

	if (GeneratedWrappedType->Finalize())
	{
		const FName UnrealModuleName = *PyGenUtil::GetFieldModule(InClass);
		GeneratedWrappedTypesForModule.Add(UnrealModuleName, InClass->GetFName());
		OutDirtyModules.Add(UnrealModuleName);

		const FString PyModuleName = PyGenUtil::GetModulePythonName(UnrealModuleName);
		PyObject* PyModule = nullptr;
		// Execute Python code within this block
		{
			FPyScopedGIL GIL;

			PyModule = PyImport_AddModule(TCHAR_TO_UTF8(*PyModuleName));

			Py_INCREF(&GeneratedWrappedType->PyType);
			PyModule_AddObject(PyModule, GeneratedWrappedType->PyType.tp_name, (PyObject*)&GeneratedWrappedType->PyType);
		}
		RegisterWrappedClassType(InClass->GetFName(), &GeneratedWrappedType->PyType);

		// Also generate and register any deprecated aliases for this type
		const TArray<FString> DeprecatedPythonClassNames = PyGenUtil::GetDeprecatedClassPythonNames(InClass);
		for (const FString& DeprecatedPythonClassName : DeprecatedPythonClassNames)
		{
			const FName DeprecatedClassName = *DeprecatedPythonClassName;
			FString DeprecationMessage = FString::Printf(TEXT("'%s' was renamed to '%s'."), *DeprecatedPythonClassName, *PythonClassName);
			
			if (GeneratedWrappedTypes.Contains(DeprecatedClassName))
			{
				REPORT_PYTHON_GENERATION_ISSUE(Warning, TEXT("Deprecated class name '%s' conflicted with an existing type!"), *DeprecatedPythonClassName);
				continue;
			}

			TSharedRef<PyGenUtil::FGeneratedWrappedClassType> DeprecatedGeneratedWrappedType = MakeShared<PyGenUtil::FGeneratedWrappedClassType>();
			GeneratedWrappedTypes.Add(DeprecatedClassName, DeprecatedGeneratedWrappedType);

			DeprecatedGeneratedWrappedType->TypeName = PyGenUtil::TCHARToUTF8Buffer(*DeprecatedPythonClassName);
			DeprecatedGeneratedWrappedType->TypeDoc = PyGenUtil::TCHARToUTF8Buffer(*FString::Printf(TEXT("deprecated: %s"), *DeprecationMessage));
			DeprecatedGeneratedWrappedType->PyType.tp_basicsize = sizeof(FPyWrapperObject);
			DeprecatedGeneratedWrappedType->PyType.tp_base = &GeneratedWrappedType->PyType;
			DeprecatedGeneratedWrappedType->PyType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;

			TSharedRef<FPyWrapperObjectMetaData> DeprecatedObjectMetaData = MakeShared<FPyWrapperObjectMetaData>(*ObjectMetaData);
			DeprecatedObjectMetaData->DeprecationMessage = MoveTemp(DeprecationMessage);
			DeprecatedGeneratedWrappedType->MetaData = DeprecatedObjectMetaData;

			if (DeprecatedGeneratedWrappedType->Finalize())
			{
				GeneratedWrappedTypesForModule.Add(UnrealModuleName, DeprecatedClassName);
				// Execute Python code within this block
				{
					FPyScopedGIL GIL;

					Py_INCREF(&DeprecatedGeneratedWrappedType->PyType);
					PyModule_AddObject(PyModule, DeprecatedGeneratedWrappedType->PyType.tp_name, (PyObject*)&DeprecatedGeneratedWrappedType->PyType);
				}
				RegisterWrappedClassType(DeprecatedClassName, &DeprecatedGeneratedWrappedType->PyType);
			}
			else
			{
				REPORT_PYTHON_GENERATION_ISSUE(Fatal, TEXT("Failed to generate Python glue code for deprecated class '%s'!"), *DeprecatedPythonClassName);
			}
		}

		return &GeneratedWrappedType->PyType;
	}
	
	REPORT_PYTHON_GENERATION_ISSUE(Fatal, TEXT("Failed to generate Python glue code for class '%s'!"), *InClass->GetName());
	return nullptr;
}

void FPyWrapperTypeRegistry::RegisterWrappedClassType(const FName ClassName, PyTypeObject* PyType, const bool InDetectNameConflicts)
{
	if (InDetectNameConflicts)
	{
		RegisterPythonTypeName(UTF8_TO_TCHAR(PyType->tp_name), ClassName);
	}
	PythonWrappedClasses.Add(ClassName, PyType);
}

void FPyWrapperTypeRegistry::UnregisterWrappedClassType(const FName ClassName, PyTypeObject* PyType)
{
	UnregisterPythonTypeName(UTF8_TO_TCHAR(PyType->tp_name), ClassName);
	PythonWrappedClasses.Remove(ClassName);
}

bool FPyWrapperTypeRegistry::HasWrappedClassType(const UClass* InClass) const
{
	return PythonWrappedClasses.Contains(InClass->GetFName());
}

PyTypeObject* FPyWrapperTypeRegistry::GetWrappedClassType(const UClass* InClass) const
{
	PyTypeObject* PyType = &PyWrapperObjectType;

	for (const UClass* Class = InClass; Class; Class = Class->GetSuperClass())
	{
		if (PyTypeObject* ClassPyType = PythonWrappedClasses.FindRef(Class->GetFName()))
		{
			PyType = ClassPyType;
			break;
		}
	}

	return PyType;
}

PyTypeObject* FPyWrapperTypeRegistry::GenerateWrappedStructType(const UScriptStruct* InStruct, FGeneratedWrappedTypeReferences& OutGeneratedWrappedTypeReferences, TSet<FName>& OutDirtyModules, const bool bForceGenerate)
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_GenerateWrappedStructTotalTime);
	INC_DWORD_STAT(STAT_GenerateWrappedStructCallCount);

	// Once we start generating types we can no longer register inline factories as they may affect the size of the generated Python objects
	bCanRegisterInlineStructFactories = false;

	struct FFuncs
	{
		static int Init(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds)
		{
			const int SuperResult = PyWrapperStructType.tp_init((PyObject*)InSelf, InArgs, InKwds);
			if (SuperResult != 0)
			{
				return SuperResult;
			}

			return FPyWrapperStruct::MakeStruct(InSelf, InArgs, InKwds);
		}
	};

	// Already processed? Nothing more to do
	if (PyTypeObject* ExistingPyType = PythonWrappedStructs.FindRef(InStruct->GetFName()))
	{
		return ExistingPyType;
	}

	// todo: allow generation of Blueprint generated structs
	if (PyGenUtil::IsBlueprintGeneratedStruct(InStruct))
	{
		return nullptr;
	}

	// Should this type be exported?
	if (!bForceGenerate && !PyGenUtil::ShouldExportStruct(InStruct))
	{
		return nullptr;
	}

	// Make sure the parent struct is also wrapped
	PyTypeObject* SuperPyType = nullptr;
	if (const UScriptStruct* SuperStruct = Cast<UScriptStruct>(InStruct->GetSuperStruct()))
	{
		SuperPyType = GenerateWrappedStructType(SuperStruct, OutGeneratedWrappedTypeReferences, OutDirtyModules, true);
	}

	INC_DWORD_STAT(STAT_GenerateWrappedStructObjCount);

	check(!GeneratedWrappedTypes.Contains(InStruct->GetFName()));
	TSharedRef<PyGenUtil::FGeneratedWrappedStructType> GeneratedWrappedType = MakeShared<PyGenUtil::FGeneratedWrappedStructType>();
	GeneratedWrappedTypes.Add(InStruct->GetFName(), GeneratedWrappedType);

	TMap<FName, FName> PythonProperties;
	TMap<FName, FString> PythonDeprecatedProperties;

	auto GenerateWrappedProperty = [this, &PythonProperties, &PythonDeprecatedProperties, &GeneratedWrappedType, &OutGeneratedWrappedTypeReferences](const UProperty* InProp)
	{
		const bool bExportPropertyToScript = PyGenUtil::ShouldExportProperty(InProp);
		const bool bExportPropertyToEditor = PyGenUtil::ShouldExportEditorOnlyProperty(InProp);

		if (bExportPropertyToScript || bExportPropertyToEditor)
		{
			GatherWrappedTypesForPropertyReferences(InProp, OutGeneratedWrappedTypeReferences);

			const PyGenUtil::FGeneratedWrappedPropertyDoc& GeneratedPropertyDoc = GeneratedWrappedType->PropertyDocs[GeneratedWrappedType->PropertyDocs.Emplace(InProp)];
			PythonProperties.Add(*GeneratedPropertyDoc.PythonPropName, InProp->GetFName());

			int32 GeneratedWrappedGetSetIndex = INDEX_NONE;
			if (bExportPropertyToScript)
			{
				GeneratedWrappedGetSetIndex = GeneratedWrappedType->GetSets.TypeGetSets.AddDefaulted();

				PyGenUtil::FGeneratedWrappedGetSet& GeneratedWrappedGetSet = GeneratedWrappedType->GetSets.TypeGetSets[GeneratedWrappedGetSetIndex];
				GeneratedWrappedGetSet.GetSetName = PyGenUtil::TCHARToUTF8Buffer(*GeneratedPropertyDoc.PythonPropName);
				GeneratedWrappedGetSet.GetSetDoc = PyGenUtil::TCHARToUTF8Buffer(*GeneratedPropertyDoc.DocString);
				GeneratedWrappedGetSet.Prop.SetProperty(InProp);
				GeneratedWrappedGetSet.GetCallback = (getter)&FPyWrapperStruct::Getter_Impl;
				GeneratedWrappedGetSet.SetCallback = (setter)&FPyWrapperStruct::Setter_Impl;
				if (GeneratedWrappedGetSet.Prop.DeprecationMessage.IsSet())
				{
					PythonDeprecatedProperties.Add(*GeneratedPropertyDoc.PythonPropName, GeneratedWrappedGetSet.Prop.DeprecationMessage.GetValue());
				}

				GeneratedWrappedType->FieldTracker.RegisterPythonFieldName(GeneratedPropertyDoc.PythonPropName, InProp);
			}

			const TArray<FString> DeprecatedPythonPropNames = PyGenUtil::GetDeprecatedPropertyPythonNames(InProp);
			for (const FString& DeprecatedPythonPropName : DeprecatedPythonPropNames)
			{
				FString DeprecationMessage = FString::Printf(TEXT("'%s' was renamed to '%s'."), *DeprecatedPythonPropName, *GeneratedPropertyDoc.PythonPropName);
				PythonProperties.Add(*DeprecatedPythonPropName, InProp->GetFName());
				PythonDeprecatedProperties.Add(*DeprecatedPythonPropName, DeprecationMessage);

				if (GeneratedWrappedGetSetIndex != INDEX_NONE)
				{
					PyGenUtil::FGeneratedWrappedGetSet DeprecatedGeneratedWrappedGetSet = GeneratedWrappedType->GetSets.TypeGetSets[GeneratedWrappedGetSetIndex];
					DeprecatedGeneratedWrappedGetSet.GetSetName = PyGenUtil::TCHARToUTF8Buffer(*DeprecatedPythonPropName);
					DeprecatedGeneratedWrappedGetSet.GetSetDoc = PyGenUtil::TCHARToUTF8Buffer(*FString::Printf(TEXT("deprecated: %s"), *DeprecationMessage));
					DeprecatedGeneratedWrappedGetSet.Prop.DeprecationMessage = MoveTemp(DeprecationMessage);
					GeneratedWrappedType->GetSets.TypeGetSets.Add(MoveTemp(DeprecatedGeneratedWrappedGetSet));

					GeneratedWrappedType->FieldTracker.RegisterPythonFieldName(DeprecatedPythonPropName, InProp);
				}
			}
		}
	};

	const FString PythonStructName = PyGenUtil::GetStructPythonName(InStruct);
	GeneratedWrappedType->TypeName = PyGenUtil::TCHARToUTF8Buffer(*PythonStructName);

	for (TFieldIterator<const UProperty> PropIt(InStruct, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
	{
		const UProperty* Prop = *PropIt;
		GenerateWrappedProperty(Prop);
	}

	FString TypeDocString = PyGenUtil::PythonizeTooltip(PyGenUtil::GetFieldTooltip(InStruct));
	if (const UScriptStruct* SuperStruct = Cast<UScriptStruct>(InStruct->GetSuperStruct()))
	{
		TSharedPtr<PyGenUtil::FGeneratedWrappedStructType> SuperGeneratedWrappedType = StaticCastSharedPtr<PyGenUtil::FGeneratedWrappedStructType>(GeneratedWrappedTypes.FindRef(SuperStruct->GetFName()));
		if (SuperGeneratedWrappedType.IsValid())
		{
			GeneratedWrappedType->PropertyDocs.Append(SuperGeneratedWrappedType->PropertyDocs);
		}
	}
	GeneratedWrappedType->PropertyDocs.Sort(&PyGenUtil::FGeneratedWrappedPropertyDoc::SortPredicate);
	PyGenUtil::AppendCppSourceInformationDocString(InStruct, TypeDocString);
	PyGenUtil::FGeneratedWrappedPropertyDoc::AppendDocString(GeneratedWrappedType->PropertyDocs, TypeDocString);
	GeneratedWrappedType->TypeDoc = PyGenUtil::TCHARToUTF8Buffer(*TypeDocString);

	int32 WrappedStructSizeBytes = sizeof(FPyWrapperStruct);
	if (const IPyWrapperInlineStructFactory* InlineStructFactory = GetInlineStructFactory(InStruct->GetFName()))
	{
		WrappedStructSizeBytes = InlineStructFactory->GetPythonObjectSizeBytes();
	}

	GeneratedWrappedType->PyType.tp_basicsize = WrappedStructSizeBytes;
	GeneratedWrappedType->PyType.tp_base = SuperPyType ? SuperPyType : &PyWrapperStructType;
	GeneratedWrappedType->PyType.tp_init = (initproc)&FFuncs::Init;
	GeneratedWrappedType->PyType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;

	auto FindMakeBreakFunction = [InStruct](const FName& InKey) -> const UFunction*
	{
		const FString MakeBreakName = InStruct->GetMetaData(InKey);
		if (!MakeBreakName.IsEmpty())
		{
			const UFunction* MakeBreakFunc = FindObject<UFunction>(nullptr, *MakeBreakName, true);
			if (!MakeBreakFunc)
			{
				REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Struct '%s' is marked as '%s' but the function '%s' could not be found."), *InStruct->GetName(), *InKey.ToString(), *MakeBreakName);
			}
			return MakeBreakFunc;
		}
		return nullptr;
	};

	auto FindMakeFunction = [InStruct, &FindMakeBreakFunction]() -> PyGenUtil::FGeneratedWrappedFunction
	{
		PyGenUtil::FGeneratedWrappedFunction MakeFunc;
		MakeFunc.SetFunction(FindMakeBreakFunction(PyGenUtil::HasNativeMakeMetaDataKey));
		if (MakeFunc.Func)
		{
			const bool bHasValidReturn = MakeFunc.OutputParams.Num() == 1 && MakeFunc.OutputParams[0].ParamProp->IsA<UStructProperty>() && CastChecked<UStructProperty>(MakeFunc.OutputParams[0].ParamProp)->Struct == InStruct;
			if (!bHasValidReturn)
			{
				REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Struct '%s' is marked as 'HasNativeMake' but the function '%s' does not return the struct type."), *InStruct->GetName(), *MakeFunc.Func->GetPathName());
				MakeFunc.SetFunction(nullptr);
			}
			// Set the make arguments to be optional to mirror the behavior of struct InitParams
			for (PyGenUtil::FGeneratedWrappedMethodParameter& InputParam : MakeFunc.InputParams)
			{
				if (!InputParam.ParamDefaultValue.IsSet())
				{
					InputParam.ParamDefaultValue = FString();
				}
			}
		}
		return MakeFunc;
	};

	auto FindBreakFunction = [InStruct, &FindMakeBreakFunction]() -> PyGenUtil::FGeneratedWrappedFunction
	{
		PyGenUtil::FGeneratedWrappedFunction BreakFunc;
		BreakFunc.SetFunction(FindMakeBreakFunction(PyGenUtil::HasNativeBreakMetaDataKey));
		if (BreakFunc.Func)
		{
			const bool bHasValidInput = BreakFunc.InputParams.Num() == 1 && BreakFunc.InputParams[0].ParamProp->IsA<UStructProperty>() && CastChecked<UStructProperty>(BreakFunc.InputParams[0].ParamProp)->Struct == InStruct;
			if (!bHasValidInput)
			{
				REPORT_PYTHON_GENERATION_ISSUE(Error, TEXT("Struct '%s' is marked as 'HasNativeBreak' but the function '%s' does not have the struct type as its only input argument."), *InStruct->GetName(), *BreakFunc.Func->GetPathName());
				BreakFunc.SetFunction(nullptr);
			}
		}
		return BreakFunc;
	};

	TSharedRef<FPyWrapperStructMetaData> StructMetaData = MakeShared<FPyWrapperStructMetaData>();
	StructMetaData->Struct = (UScriptStruct*)InStruct;
	StructMetaData->PythonProperties = MoveTemp(PythonProperties);
	StructMetaData->PythonDeprecatedProperties = MoveTemp(PythonDeprecatedProperties);
	StructMetaData->MakeFunc = FindMakeFunction();
	StructMetaData->BreakFunc = FindBreakFunction();
	// Build a complete list of init params for this struct (parent struct params + our params)
	if (SuperPyType)
	{
		FPyWrapperStructMetaData* SuperMetaData = FPyWrapperStructMetaData::GetMetaData(SuperPyType);
		if (SuperMetaData)
		{
			StructMetaData->InitParams = SuperMetaData->InitParams;
		}
	}
	for (const PyGenUtil::FGeneratedWrappedGetSet& GeneratedWrappedGetSet : GeneratedWrappedType->GetSets.TypeGetSets)
	{
		if (!GeneratedWrappedGetSet.Prop.DeprecationMessage.IsSet())
		{
			PyGenUtil::FGeneratedWrappedMethodParameter& GeneratedInitParam = StructMetaData->InitParams.AddDefaulted_GetRef();
			GeneratedInitParam.ParamName = GeneratedWrappedGetSet.GetSetName;
			GeneratedInitParam.ParamProp = GeneratedWrappedGetSet.Prop.Prop;
			GeneratedInitParam.ParamDefaultValue = FString();
		}
	}
	GeneratedWrappedType->MetaData = StructMetaData;

	if (GeneratedWrappedType->Finalize())
	{
		const FName UnrealModuleName = *PyGenUtil::GetFieldModule(InStruct);
		GeneratedWrappedTypesForModule.Add(UnrealModuleName, InStruct->GetFName());
		OutDirtyModules.Add(UnrealModuleName);

		const FString PyModuleName = PyGenUtil::GetModulePythonName(UnrealModuleName);
		PyObject* PyModule = nullptr;
		// Execute Python code within this block
		{
			FPyScopedGIL GIL;

			PyModule = PyImport_AddModule(TCHAR_TO_UTF8(*PyModuleName));

			Py_INCREF(&GeneratedWrappedType->PyType);
			PyModule_AddObject(PyModule, GeneratedWrappedType->PyType.tp_name, (PyObject*)&GeneratedWrappedType->PyType);
		}
		RegisterWrappedStructType(InStruct->GetFName(), &GeneratedWrappedType->PyType);

		// Also generate and register any deprecated aliases for this type
		const TArray<FString> DeprecatedPythonStructNames = PyGenUtil::GetDeprecatedStructPythonNames(InStruct);
		for (const FString& DeprecatedPythonStructName : DeprecatedPythonStructNames)
		{
			const FName DeprecatedStructName = *DeprecatedPythonStructName;
			FString DeprecationMessage = FString::Printf(TEXT("'%s' was renamed to '%s'."), *DeprecatedPythonStructName, *PythonStructName);

			if (GeneratedWrappedTypes.Contains(DeprecatedStructName))
			{
				REPORT_PYTHON_GENERATION_ISSUE(Warning, TEXT("Deprecated struct name '%s' conflicted with an existing type!"), *DeprecatedPythonStructName);
				continue;
			}

			TSharedRef<PyGenUtil::FGeneratedWrappedStructType> DeprecatedGeneratedWrappedType = MakeShared<PyGenUtil::FGeneratedWrappedStructType>();
			GeneratedWrappedTypes.Add(DeprecatedStructName, DeprecatedGeneratedWrappedType);

			DeprecatedGeneratedWrappedType->TypeName = PyGenUtil::TCHARToUTF8Buffer(*DeprecatedPythonStructName);
			DeprecatedGeneratedWrappedType->TypeDoc = PyGenUtil::TCHARToUTF8Buffer(*FString::Printf(TEXT("deprecated: %s"), *DeprecationMessage));
			DeprecatedGeneratedWrappedType->PyType.tp_basicsize = WrappedStructSizeBytes;
			DeprecatedGeneratedWrappedType->PyType.tp_base = &GeneratedWrappedType->PyType;
			DeprecatedGeneratedWrappedType->PyType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;

			TSharedRef<FPyWrapperStructMetaData> DeprecatedStructMetaData = MakeShared<FPyWrapperStructMetaData>(*StructMetaData);
			DeprecatedStructMetaData->DeprecationMessage = MoveTemp(DeprecationMessage);
			DeprecatedGeneratedWrappedType->MetaData = DeprecatedStructMetaData;

			if (DeprecatedGeneratedWrappedType->Finalize())
			{
				GeneratedWrappedTypesForModule.Add(UnrealModuleName, DeprecatedStructName);
				// Execute Python code within this block
				{
					FPyScopedGIL GIL;

					Py_INCREF(&DeprecatedGeneratedWrappedType->PyType);
					PyModule_AddObject(PyModule, DeprecatedGeneratedWrappedType->PyType.tp_name, (PyObject*)&DeprecatedGeneratedWrappedType->PyType);
				}
				RegisterWrappedStructType(DeprecatedStructName, &DeprecatedGeneratedWrappedType->PyType);
			}
			else
			{
				REPORT_PYTHON_GENERATION_ISSUE(Fatal, TEXT("Failed to generate Python glue code for deprecated struct '%s'!"), *DeprecatedPythonStructName);
			}
		}

		return &GeneratedWrappedType->PyType;
	}

	REPORT_PYTHON_GENERATION_ISSUE(Fatal, TEXT("Failed to generate Python glue code for struct '%s'!"), *InStruct->GetName());
	return nullptr;
}

void FPyWrapperTypeRegistry::RegisterWrappedStructType(const FName StructName, PyTypeObject* PyType, const bool InDetectNameConflicts)
{
	if (InDetectNameConflicts)
	{
		RegisterPythonTypeName(UTF8_TO_TCHAR(PyType->tp_name), StructName);
	}
	PythonWrappedStructs.Add(StructName, PyType);
}

void FPyWrapperTypeRegistry::UnregisterWrappedStructType(const FName StructName, PyTypeObject* PyType)
{
	UnregisterPythonTypeName(UTF8_TO_TCHAR(PyType->tp_name), StructName);
	PythonWrappedStructs.Remove(StructName);
}

bool FPyWrapperTypeRegistry::HasWrappedStructType(const UScriptStruct* InStruct) const
{
	return PythonWrappedStructs.Contains(InStruct->GetFName());
}

PyTypeObject* FPyWrapperTypeRegistry::GetWrappedStructType(const UScriptStruct* InStruct) const
{
	PyTypeObject* PyType = &PyWrapperStructType;

	for (const UScriptStruct* Struct = InStruct; Struct; Struct = Cast<UScriptStruct>(Struct->GetSuperStruct()))
	{
		if (PyTypeObject* StructPyType = PythonWrappedStructs.FindRef(Struct->GetFName()))
		{
			PyType = StructPyType;
			break;
		}
	}

	return PyType;
}

PyTypeObject* FPyWrapperTypeRegistry::GenerateWrappedEnumType(const UEnum* InEnum, FGeneratedWrappedTypeReferences& OutGeneratedWrappedTypeReferences, TSet<FName>& OutDirtyModules, const bool bForceGenerate)
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_GenerateWrappedEnumTotalTime);
	INC_DWORD_STAT(STAT_GenerateWrappedEnumCallCount);

	// Already processed? Nothing more to do
	if (PyTypeObject* ExistingPyType = PythonWrappedEnums.FindRef(InEnum->GetFName()))
	{
		return ExistingPyType;
	}

	// todo: allow generation of Blueprint generated enums
	if (PyGenUtil::IsBlueprintGeneratedEnum(InEnum))
	{
		return nullptr;
	}

	// Should this type be exported?
	if (!bForceGenerate && !PyGenUtil::ShouldExportEnum(InEnum))
	{
		return nullptr;
	}

	INC_DWORD_STAT(STAT_GenerateWrappedEnumObjCount);

	check(!GeneratedWrappedTypes.Contains(InEnum->GetFName()));
	TSharedRef<PyGenUtil::FGeneratedWrappedEnumType> GeneratedWrappedType = MakeShared<PyGenUtil::FGeneratedWrappedEnumType>();
	GeneratedWrappedTypes.Add(InEnum->GetFName(), GeneratedWrappedType);

	FString TypeDocString = PyGenUtil::PythonizeTooltip(PyGenUtil::GetFieldTooltip(InEnum));
	PyGenUtil::AppendCppSourceInformationDocString(InEnum, TypeDocString);

	const FString PythonEnumName = PyGenUtil::GetEnumPythonName(InEnum);
	GeneratedWrappedType->TypeName = PyGenUtil::TCHARToUTF8Buffer(*PythonEnumName);
	GeneratedWrappedType->TypeDoc = PyGenUtil::TCHARToUTF8Buffer(*TypeDocString);
	GeneratedWrappedType->ExtractEnumEntries(InEnum);

	GeneratedWrappedType->PyType.tp_basicsize = sizeof(FPyWrapperEnum);
	GeneratedWrappedType->PyType.tp_base = &PyWrapperEnumType;
	GeneratedWrappedType->PyType.tp_flags = Py_TPFLAGS_DEFAULT;

	TSharedRef<FPyWrapperEnumMetaData> EnumMetaData = MakeShared<FPyWrapperEnumMetaData>();
	EnumMetaData->Enum = (UEnum*)InEnum;
	GeneratedWrappedType->MetaData = EnumMetaData;

	if (GeneratedWrappedType->Finalize())
	{
		const FName UnrealModuleName = *PyGenUtil::GetFieldModule(InEnum);
		GeneratedWrappedTypesForModule.Add(UnrealModuleName, InEnum->GetFName());
		OutDirtyModules.Add(UnrealModuleName);

		const FString PyModuleName = PyGenUtil::GetModulePythonName(UnrealModuleName);
		PyObject* PyModule = nullptr;
		// Execute Python code within this block
		{
			FPyScopedGIL GIL;

			PyModule = PyImport_AddModule(TCHAR_TO_UTF8(*PyModuleName));

			Py_INCREF(&GeneratedWrappedType->PyType);
			PyModule_AddObject(PyModule, GeneratedWrappedType->PyType.tp_name, (PyObject*)&GeneratedWrappedType->PyType);
		}
		RegisterWrappedEnumType(InEnum->GetFName(), &GeneratedWrappedType->PyType);

		// Also generate and register any deprecated aliases for this type
		const TArray<FString> DeprecatedPythonEnumNames = PyGenUtil::GetDeprecatedEnumPythonNames(InEnum);
		for (const FString& DeprecatedPythonEnumName : DeprecatedPythonEnumNames)
		{
			const FName DeprecatedEnumName = *DeprecatedPythonEnumName;
			FString DeprecationMessage = FString::Printf(TEXT("'%s' was renamed to '%s'."), *DeprecatedPythonEnumName, *PythonEnumName);

			if (GeneratedWrappedTypes.Contains(DeprecatedEnumName))
			{
				REPORT_PYTHON_GENERATION_ISSUE(Warning, TEXT("Deprecated enum name '%s' conflicted with an existing type!"), *DeprecatedPythonEnumName);
				continue;
			}

			TSharedRef<PyGenUtil::FGeneratedWrappedEnumType> DeprecatedGeneratedWrappedType = MakeShared<PyGenUtil::FGeneratedWrappedEnumType>();
			GeneratedWrappedTypes.Add(DeprecatedEnumName, DeprecatedGeneratedWrappedType);

			DeprecatedGeneratedWrappedType->TypeName = PyGenUtil::TCHARToUTF8Buffer(*DeprecatedPythonEnumName);
			DeprecatedGeneratedWrappedType->TypeDoc = PyGenUtil::TCHARToUTF8Buffer(*FString::Printf(TEXT("deprecated: %s"), *DeprecationMessage));
			DeprecatedGeneratedWrappedType->EnumEntries = GeneratedWrappedType->EnumEntries;
			DeprecatedGeneratedWrappedType->PyType.tp_basicsize = sizeof(FPyWrapperEnum);
			DeprecatedGeneratedWrappedType->PyType.tp_base = &PyWrapperEnumType;
			DeprecatedGeneratedWrappedType->PyType.tp_flags = Py_TPFLAGS_DEFAULT;

			TSharedRef<FPyWrapperEnumMetaData> DeprecatedEnumMetaData = MakeShared<FPyWrapperEnumMetaData>(*EnumMetaData);
			DeprecatedEnumMetaData->DeprecationMessage = MoveTemp(DeprecationMessage);
			DeprecatedGeneratedWrappedType->MetaData = DeprecatedEnumMetaData;

			if (DeprecatedGeneratedWrappedType->Finalize())
			{
				GeneratedWrappedTypesForModule.Add(UnrealModuleName, DeprecatedEnumName);
				// Execute Python code within this block
				{
					FPyScopedGIL GIL;

					Py_INCREF(&DeprecatedGeneratedWrappedType->PyType);
					PyModule_AddObject(PyModule, DeprecatedGeneratedWrappedType->PyType.tp_name, (PyObject*)&DeprecatedGeneratedWrappedType->PyType);
				}
				RegisterWrappedEnumType(DeprecatedEnumName, &DeprecatedGeneratedWrappedType->PyType);
			}
			else
			{
				REPORT_PYTHON_GENERATION_ISSUE(Fatal, TEXT("Failed to generate Python glue code for deprecated enum '%s'!"), *DeprecatedPythonEnumName);
			}
		}

		return &GeneratedWrappedType->PyType;
	}

	REPORT_PYTHON_GENERATION_ISSUE(Fatal, TEXT("Failed to generate Python glue code for enum '%s'!"), *InEnum->GetName());
	return nullptr;
}

void FPyWrapperTypeRegistry::RegisterWrappedEnumType(const FName EnumName, PyTypeObject* PyType, const bool InDetectNameConflicts)
{
	if (InDetectNameConflicts)
	{
		RegisterPythonTypeName(UTF8_TO_TCHAR(PyType->tp_name), EnumName);
	}
	PythonWrappedEnums.Add(EnumName, PyType);
}

void FPyWrapperTypeRegistry::UnregisterWrappedEnumType(const FName EnumName, PyTypeObject* PyType)
{
	UnregisterPythonTypeName(UTF8_TO_TCHAR(PyType->tp_name), EnumName);
	PythonWrappedEnums.Remove(EnumName);
}

bool FPyWrapperTypeRegistry::HasWrappedEnumType(const UEnum* InEnum) const
{
	return PythonWrappedEnums.Contains(InEnum->GetFName());
}

PyTypeObject* FPyWrapperTypeRegistry::GetWrappedEnumType(const UEnum* InEnum) const
{
	PyTypeObject* PyType = &PyWrapperEnumType;

	if (PyTypeObject* EnumPyType = PythonWrappedEnums.FindRef(InEnum->GetFName()))
	{
		PyType = EnumPyType;
	}

	return PyType;
}

PyTypeObject* FPyWrapperTypeRegistry::GenerateWrappedDelegateType(const UFunction* InDelegateSignature, FGeneratedWrappedTypeReferences& OutGeneratedWrappedTypeReferences, TSet<FName>& OutDirtyModules, const bool bForceGenerate)
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_GenerateWrappedDelegateTotalTime);
	INC_DWORD_STAT(STAT_GenerateWrappedDelegateCallCount);

	// Already processed? Nothing more to do
	if (PyTypeObject* ExistingPyType = PythonWrappedDelegates.FindRef(InDelegateSignature->GetFName()))
	{
		return ExistingPyType;
	}

	// Is this actually a delegate signature?
	if (!InDelegateSignature->HasAnyFunctionFlags(FUNC_Delegate))
	{
		return nullptr;
	}

	INC_DWORD_STAT(STAT_GenerateWrappedDelegateObjCount);

	check(!GeneratedWrappedTypes.Contains(InDelegateSignature->GetFName()));
	TSharedRef<PyGenUtil::FGeneratedWrappedType> GeneratedWrappedType = MakeShared<PyGenUtil::FGeneratedWrappedType>();
	GeneratedWrappedTypes.Add(InDelegateSignature->GetFName(), GeneratedWrappedType);

	for (TFieldIterator<const UProperty> ParamIt(InDelegateSignature); ParamIt; ++ParamIt)
	{
		const UProperty* Param = *ParamIt;
		GatherWrappedTypesForPropertyReferences(Param, OutGeneratedWrappedTypeReferences);
	}

	FString TypeDocString = PyGenUtil::PythonizeFunctionTooltip(PyGenUtil::GetFieldTooltip(InDelegateSignature), InDelegateSignature);
	PyGenUtil::AppendCppSourceInformationDocString(InDelegateSignature, TypeDocString);

	const FString DelegateBaseTypename = PyGenUtil::GetDelegatePythonName(InDelegateSignature);
	GeneratedWrappedType->TypeName = PyGenUtil::TCHARToUTF8Buffer(*DelegateBaseTypename);
	GeneratedWrappedType->TypeDoc = PyGenUtil::TCHARToUTF8Buffer(*TypeDocString);

	GeneratedWrappedType->PyType.tp_flags = Py_TPFLAGS_DEFAULT;

	// Generate the proxy class needed to wrap Python callables in Unreal delegates
	UClass* PythonCallableForDelegateClass = nullptr;
	{
		PythonCallableForDelegateClass = NewObject<UClass>(GetTransientPackage(), *FString::Printf(TEXT("%s__PythonCallable"), *DelegateBaseTypename), RF_Public);
		UFunction* PythonCallableForDelegateFunc = (UFunction*)StaticDuplicateObject(InDelegateSignature, PythonCallableForDelegateClass, UPythonCallableForDelegate::GeneratedFuncName, RF_AllFlags, UFunction::StaticClass());
		PythonCallableForDelegateFunc->FunctionFlags = (PythonCallableForDelegateFunc->FunctionFlags | FUNC_Native) & ~(FUNC_Delegate | FUNC_MulticastDelegate);
		PythonCallableForDelegateFunc->SetNativeFunc(&UPythonCallableForDelegate::CallPythonNative);
		PythonCallableForDelegateFunc->StaticLink(true);
		PythonCallableForDelegateClass->AddFunctionToFunctionMap(PythonCallableForDelegateFunc, PythonCallableForDelegateFunc->GetFName());
		PythonCallableForDelegateClass->SetSuperStruct(UPythonCallableForDelegate::StaticClass());
		PythonCallableForDelegateClass->Bind();
		PythonCallableForDelegateClass->StaticLink(true);
		PythonCallableForDelegateClass->AssembleReferenceTokenStream();
	}

	if (InDelegateSignature->HasAnyFunctionFlags(FUNC_MulticastDelegate))
	{
		GeneratedWrappedType->PyType.tp_basicsize = sizeof(FPyWrapperMulticastDelegate);
		GeneratedWrappedType->PyType.tp_base = &PyWrapperMulticastDelegateType;

		TSharedRef<FPyWrapperMulticastDelegateMetaData> DelegateMetaData = MakeShared<FPyWrapperMulticastDelegateMetaData>();
		DelegateMetaData->DelegateSignature.SetFunction(InDelegateSignature);
		DelegateMetaData->PythonCallableForDelegateClass = PythonCallableForDelegateClass;
		GeneratedWrappedType->MetaData = DelegateMetaData;
	}
	else
	{
		GeneratedWrappedType->PyType.tp_basicsize = sizeof(FPyWrapperDelegate);
		GeneratedWrappedType->PyType.tp_base = &PyWrapperDelegateType;

		TSharedRef<FPyWrapperDelegateMetaData> DelegateMetaData = MakeShared<FPyWrapperDelegateMetaData>();
		DelegateMetaData->DelegateSignature.SetFunction(InDelegateSignature);
		DelegateMetaData->PythonCallableForDelegateClass = PythonCallableForDelegateClass;
		GeneratedWrappedType->MetaData = DelegateMetaData;
	}

	if (GeneratedWrappedType->Finalize())
	{
		const FName UnrealModuleName = *PyGenUtil::GetFieldModule(InDelegateSignature);
		GeneratedWrappedTypesForModule.Add(UnrealModuleName, InDelegateSignature->GetFName());
		OutDirtyModules.Add(UnrealModuleName);

		const FString PyModuleName = PyGenUtil::GetModulePythonName(UnrealModuleName);
		// Execute Python code within this block
		{
			FPyScopedGIL GIL;

			PyObject* PyModule = PyImport_AddModule(TCHAR_TO_UTF8(*PyModuleName));

			Py_INCREF(&GeneratedWrappedType->PyType);
			PyModule_AddObject(PyModule, GeneratedWrappedType->PyType.tp_name, (PyObject*)&GeneratedWrappedType->PyType);
		}
		RegisterWrappedDelegateType(InDelegateSignature->GetFName(), &GeneratedWrappedType->PyType);

		return &GeneratedWrappedType->PyType;
	}

	REPORT_PYTHON_GENERATION_ISSUE(Fatal, TEXT("Failed to generate Python glue code for delegate '%s'!"), *InDelegateSignature->GetName());
	return nullptr;
}

void FPyWrapperTypeRegistry::RegisterWrappedDelegateType(const FName DelegateName, PyTypeObject* PyType, const bool InDetectNameConflicts)
{
	if (InDetectNameConflicts)
	{
		RegisterPythonTypeName(UTF8_TO_TCHAR(PyType->tp_name), DelegateName);
	}
	PythonWrappedDelegates.Add(DelegateName, PyType);
}

void FPyWrapperTypeRegistry::UnregisterWrappedDelegateType(const FName DelegateName, PyTypeObject* PyType)
{
	UnregisterPythonTypeName(UTF8_TO_TCHAR(PyType->tp_name), DelegateName);
	PythonWrappedDelegates.Remove(DelegateName);
}

bool FPyWrapperTypeRegistry::HasWrappedDelegateType(const UFunction* InDelegateSignature) const
{
	return PythonWrappedDelegates.Contains(InDelegateSignature->GetFName());
}

PyTypeObject* FPyWrapperTypeRegistry::GetWrappedDelegateType(const UFunction* InDelegateSignature) const
{
	PyTypeObject* PyType = InDelegateSignature->HasAnyFunctionFlags(FUNC_MulticastDelegate) ? &PyWrapperMulticastDelegateType : &PyWrapperDelegateType;

	if (PyTypeObject* DelegatePyType = PythonWrappedDelegates.FindRef(InDelegateSignature->GetFName()))
	{
		PyType = DelegatePyType;
	}

	return PyType;
}

void FPyWrapperTypeRegistry::GatherWrappedTypesForPropertyReferences(const UProperty* InProp, FGeneratedWrappedTypeReferences& OutGeneratedWrappedTypeReferences) const
{
	if (const UObjectProperty* ObjProp = Cast<const UObjectProperty>(InProp))
	{
		if (ObjProp->PropertyClass && !PythonWrappedClasses.Contains(ObjProp->PropertyClass->GetFName()))
		{
			OutGeneratedWrappedTypeReferences.ClassReferences.Add(ObjProp->PropertyClass);
		}
		return;
	}

	if (const UStructProperty* StructProp = Cast<const UStructProperty>(InProp))
	{
		if (!PythonWrappedStructs.Contains(StructProp->Struct->GetFName()))
		{
			OutGeneratedWrappedTypeReferences.StructReferences.Add(StructProp->Struct);
		}
		return;
	}

	if (const UEnumProperty* EnumProp = Cast<const UEnumProperty>(InProp))
	{
		if (!PythonWrappedStructs.Contains(EnumProp->GetEnum()->GetFName()))
		{
			OutGeneratedWrappedTypeReferences.EnumReferences.Add(EnumProp->GetEnum());
		}
		return;
	}

	if (const UByteProperty* ByteProp = Cast<const UByteProperty>(InProp))
	{
		if (ByteProp->Enum)
		{
			if (!PythonWrappedStructs.Contains(ByteProp->Enum->GetFName()))
			{
				OutGeneratedWrappedTypeReferences.EnumReferences.Add(ByteProp->Enum);
			}
		}
		return;
	}

	if (const UDelegateProperty* DelegateProp = Cast<const UDelegateProperty>(InProp))
	{
		if (!PythonWrappedStructs.Contains(DelegateProp->SignatureFunction->GetFName()))
		{
			OutGeneratedWrappedTypeReferences.DelegateReferences.Add(DelegateProp->SignatureFunction);
		}
		return;
	}

	if (const UMulticastDelegateProperty* DelegateProp = Cast<const UMulticastDelegateProperty>(InProp))
	{
		if (!PythonWrappedStructs.Contains(DelegateProp->SignatureFunction->GetFName()))
		{
			OutGeneratedWrappedTypeReferences.DelegateReferences.Add(DelegateProp->SignatureFunction);
		}
		return;
	}

	if (const UArrayProperty* ArrayProp = Cast<const UArrayProperty>(InProp))
	{
		GatherWrappedTypesForPropertyReferences(ArrayProp->Inner, OutGeneratedWrappedTypeReferences);
		return;
	}

	if (const USetProperty* SetProp = Cast<const USetProperty>(InProp))
	{
		GatherWrappedTypesForPropertyReferences(SetProp->ElementProp, OutGeneratedWrappedTypeReferences);
		return;
	}

	if (const UMapProperty* MapProp = Cast<const UMapProperty>(InProp))
	{
		GatherWrappedTypesForPropertyReferences(MapProp->KeyProp, OutGeneratedWrappedTypeReferences);
		GatherWrappedTypesForPropertyReferences(MapProp->ValueProp, OutGeneratedWrappedTypeReferences);
		return;
	}
}

void FPyWrapperTypeRegistry::GenerateStubCodeForWrappedTypes(const EPyOnlineDocsFilterFlags InDocGenFlags) const
{
	UE_LOG(LogPython, Display, TEXT("Generating Python API stub file..."));

	FPyFileWriter PythonScript;

	TUniquePtr<FPyOnlineDocsWriter> OnlineDocsWriter;
	TSharedPtr<FPyOnlineDocsModule> OnlineDocsUnrealModule;
	TSharedPtr<FPyOnlineDocsSection> OnlineDocsNativeTypesSection;
	TSharedPtr<FPyOnlineDocsSection> OnlineDocsEnumTypesSection;
	TSharedPtr<FPyOnlineDocsSection> OnlineDocsDelegateTypesSection;
	TSharedPtr<FPyOnlineDocsSection> OnlineDocsStructTypesSection;
	TSharedPtr<FPyOnlineDocsSection> OnlineDocsClassTypesSection;

	if (EnumHasAnyFlags(InDocGenFlags, EPyOnlineDocsFilterFlags::IncludeAll))
	{
		OnlineDocsWriter = MakeUnique<FPyOnlineDocsWriter>();
		OnlineDocsUnrealModule = OnlineDocsWriter->CreateModule(TEXT("unreal"));
		OnlineDocsNativeTypesSection = OnlineDocsWriter->CreateSection(TEXT("Native Types"));
		OnlineDocsStructTypesSection = OnlineDocsWriter->CreateSection(TEXT("Struct Types"));
		OnlineDocsClassTypesSection = OnlineDocsWriter->CreateSection(TEXT("Class Types"));
		OnlineDocsEnumTypesSection = OnlineDocsWriter->CreateSection(TEXT("Enum Types"));
		OnlineDocsDelegateTypesSection = OnlineDocsWriter->CreateSection(TEXT("Delegate Types"));
	}

	// Process additional Python files
	// We split these up so that imports (excluding "unreal" imports) are listed at the top of the stub file
	// with the remaining code at the bottom (as it may depend on reflected APIs)
	TArray<FString> AdditionalPythonCode;
	{
		TArray<FName> ModuleNames;
		GeneratedWrappedTypesForModule.GetKeys(ModuleNames);
		ModuleNames.Sort();

		bool bExportedImports = false;
		for (const FName ModuleName : ModuleNames)
		{
			const FString PythonBaseModuleName = PyGenUtil::GetModulePythonName(ModuleName, false);
			const FString PythonModuleName = FString::Printf(TEXT("unreal_%s"), *PythonBaseModuleName);
		
			FString ModuleFilename;
			if (PyUtil::IsModuleAvailableForImport(*PythonModuleName, &ModuleFilename))
			{
				// Adjust .pyc and .pyd files so we try and find the source Python file
				ModuleFilename = FPaths::ChangeExtension(ModuleFilename, TEXT(".py"));
				if (FPaths::FileExists(ModuleFilename))
				{
					TArray<FString> PythonFile;
					if (FFileHelper::LoadFileToStringArray(PythonFile, *ModuleFilename))
					{
						// Process the file, looking for imports, and top-level classes and methods
						for (FString& PythonFileLine : PythonFile)
						{
							PythonFileLine.ReplaceInline(TEXT("\t"), TEXT("    "));
							
							// Write out each import line (excluding "unreal" imports)
							if (PythonFileLine.Contains(TEXT("import "), ESearchCase::CaseSensitive))
							{
								if (!PythonFileLine.Contains(TEXT("unreal"), ESearchCase::CaseSensitive))
								{
									bExportedImports = true;
									PythonScript.WriteLine(PythonFileLine);
								}
								continue;
							}

							if (OnlineDocsUnrealModule.IsValid())
							{
								// Is this a top-level function?
								if (PythonFileLine.StartsWith(TEXT("def "), ESearchCase::CaseSensitive))
								{
									// Extract the function name
									FString FunctionName;
									for (const TCHAR* CharPtr = *PythonFileLine + 4; *CharPtr && *CharPtr != TEXT('('); ++CharPtr)
									{
										FunctionName += *CharPtr;
									}
									FunctionName.TrimStartAndEndInline();

									OnlineDocsUnrealModule->AccumulateFunction(*FunctionName);
								}
							}

							if (OnlineDocsNativeTypesSection.IsValid())
							{
								// Is this a top-level class?
								if (PythonFileLine.StartsWith(TEXT("class "), ESearchCase::CaseSensitive))
								{
									// Extract the class name
									FString ClassName;
									for (const TCHAR* CharPtr = *PythonFileLine + 6; *CharPtr && *CharPtr != TEXT('(') && *CharPtr != TEXT(':'); ++CharPtr)
									{
										ClassName += *CharPtr;
									}
									ClassName.TrimStartAndEndInline();

									OnlineDocsNativeTypesSection->AccumulateClass(*ClassName);
								}
							}

							// Stash any additional code so that we append it later
							AdditionalPythonCode.Add(MoveTemp(PythonFileLine));
						}
						AdditionalPythonCode.AddDefaulted(); // add an empty line after each file
					}
				}
			}
		}
		if (bExportedImports)
		{
			PythonScript.WriteNewLine();
		}
	}

	// Process native glue code
	UE_LOG(LogPython, Display, TEXT("  ...generating Python API: glue code"));
	PythonScript.WriteLine(TEXT("##### Glue Code #####"));
	PythonScript.WriteNewLine();

	for (const PyGenUtil::FNativePythonModule& NativePythonModule : NativePythonModules)
	{
		for (PyMethodDef* MethodDef = NativePythonModule.PyModuleMethods; MethodDef && MethodDef->ml_name; ++MethodDef)
		{
			const bool bHasKeywords = !!(MethodDef->ml_flags & METH_KEYWORDS);
			PythonScript.WriteLine(FString::Printf(TEXT("def %s(*args%s):"), UTF8_TO_TCHAR(MethodDef->ml_name), bHasKeywords ? TEXT(", **kwargs") : TEXT("")));
			PythonScript.IncreaseIndent();
			PythonScript.WriteDocString(UTF8_TO_TCHAR(MethodDef->ml_doc));
			PythonScript.WriteLine(TEXT("pass"));
			PythonScript.DecreaseIndent();
			PythonScript.WriteNewLine();

			if (OnlineDocsUnrealModule.IsValid())
			{
				OnlineDocsUnrealModule->AccumulateFunction(UTF8_TO_TCHAR(MethodDef->ml_name));
			}
		}

		for (PyTypeObject* PyType : NativePythonModule.PyModuleTypes)
		{
			GenerateStubCodeForWrappedType(PyType, nullptr, PythonScript, OnlineDocsNativeTypesSection.Get());
		}
	}

	// Process generated glue code
	// Also excludes types that don't pass the filters specified in InDocGenFlags using the information about
	// which module it came from and where that module exists on disk.
	auto ProcessWrappedDataArray = [this, &PythonScript, InDocGenFlags](const TMap<FName, PyTypeObject*>& WrappedData, const TSharedPtr<FPyOnlineDocsSection>& OnlineDocsSection)
	{
		if (InDocGenFlags == EPyOnlineDocsFilterFlags::IncludeNone)
		{
			return;
		}

		UE_LOG(LogPython, Display, TEXT("  ...generating Python API: %s"), *OnlineDocsSection->GetName());
		PythonScript.WriteLine(FString::Printf(TEXT("##### %s #####"), *OnlineDocsSection->GetName()));
		PythonScript.WriteNewLine();
		
		FString ProjectTopDir;
		if (FPaths::IsProjectFilePathSet())
		{
			ProjectTopDir / FPaths::GetCleanFilename(FPaths::ProjectDir());
		}

		for (const auto& WrappedDataPair : WrappedData)
		{
			TSharedPtr<PyGenUtil::FGeneratedWrappedType> GeneratedWrappedType = GeneratedWrappedTypes.FindRef(WrappedDataPair.Key);

			if ((InDocGenFlags != EPyOnlineDocsFilterFlags::IncludeAll) && GeneratedWrappedType.IsValid())
			{
				const UField* MetaType = GeneratedWrappedType->MetaData->GetMetaType();

				FString ModulePath;

				if (MetaType)
				{
					FSourceCodeNavigation::FindModulePath(MetaType->GetTypedOuter<UPackage>(), ModulePath);
				}

				if (!ModulePath.IsEmpty())
				{
					// Is Project class?
					if (!ProjectTopDir.IsEmpty()
						&& (ModulePath.Contains(ProjectTopDir)))
					{
						// Optionally exclude Project classes
						if (!EnumHasAnyFlags(InDocGenFlags, EPyOnlineDocsFilterFlags::IncludeProject))
						{
							continue;
						}
					}
					// Is Enterprise class
					else if (ModulePath.Contains(TEXT("/Enterprise/")))
					{
						// Optionally exclude Enterprise classes
						if (!EnumHasAnyFlags(InDocGenFlags, EPyOnlineDocsFilterFlags::IncludeEnterprise))
						{
							continue;
						}
					}
					// is internal class
					else if (FPaths::IsRestrictedPath(ModulePath))
					{
						// Optionally exclude internal classes
						if (!EnumHasAnyFlags(InDocGenFlags, EPyOnlineDocsFilterFlags::IncludeInternal))
						{
							continue;
						}
					}
					// Everything else is considered an "Engine" class
					else
					{
						// Optionally exclude engine classes
						if (!EnumHasAnyFlags(InDocGenFlags, EPyOnlineDocsFilterFlags::IncludeEngine))
						{
							continue;
						}
					}
				}
				// else if cannot determine origin then include
			}

			GenerateStubCodeForWrappedType(WrappedDataPair.Value, GeneratedWrappedType.Get(), PythonScript, OnlineDocsSection.Get());
		}
	};

	ProcessWrappedDataArray(PythonWrappedEnums, OnlineDocsEnumTypesSection);
	ProcessWrappedDataArray(PythonWrappedDelegates, OnlineDocsDelegateTypesSection);
	ProcessWrappedDataArray(PythonWrappedStructs, OnlineDocsStructTypesSection);
	ProcessWrappedDataArray(PythonWrappedClasses, OnlineDocsClassTypesSection);

	// Append any additional Python code now that all the reflected API has been exported
	UE_LOG(LogPython, Display, TEXT("  ...generating Python API: additional code"));
	PythonScript.WriteLine(TEXT("##### Additional Code #####"));
	PythonScript.WriteNewLine();

	for (const FString& AdditionalPythonLine : AdditionalPythonCode)
	{
		PythonScript.WriteLine(AdditionalPythonLine);
	}

	const FString PythonSourceFilename = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir()) / TEXT("PythonStub") / TEXT("unreal.py");
	PythonScript.SaveFile(*PythonSourceFilename);
	UE_LOG(LogPython, Display, TEXT("  ...generated: %s"), *PythonSourceFilename);

	if (OnlineDocsWriter.IsValid())
	{
		// Generate Sphinx files used to generate static HTML for Python API docs.
		OnlineDocsWriter->GenerateFiles(PythonSourceFilename);
	}
}

void FPyWrapperTypeRegistry::GenerateStubCodeForWrappedType(PyTypeObject* PyType, const PyGenUtil::FGeneratedWrappedType* GeneratedTypeData, FPyFileWriter& OutPythonScript, FPyOnlineDocsSection* OutOnlineDocsSection)
{
	const FString PyTypeName = UTF8_TO_TCHAR(PyType->tp_name);
	OutPythonScript.WriteLine(FString::Printf(TEXT("class %s(%s):"), *PyTypeName, UTF8_TO_TCHAR(PyType->tp_base->tp_name)));
	OutPythonScript.IncreaseIndent();
	OutPythonScript.WriteDocString(UTF8_TO_TCHAR(PyType->tp_doc));

	if (OutOnlineDocsSection)
	{
		OutOnlineDocsSection->AccumulateClass(*PyTypeName);
	}

	auto GetFunctionReturnValue = [](const void* InBaseParamsAddr, const TArray<PyGenUtil::FGeneratedWrappedMethodParameter>& InOutputParams) -> FString
	{
		if (InOutputParams.Num() == 0)
		{
			return TEXT("None");
		}
		
		// We use strict typing for return values to aid auto-complete (we also only care about the type and not the value, so structs can be default constructed)
		static const uint32 PythonizeValueFlags = PyGenUtil::EPythonizeValueFlags::UseStrictTyping | PyGenUtil::EPythonizeValueFlags::DefaultConstructStructs;

		// If we have multiple return values and the main return value is a bool, skip it (to mimic PyGenUtils::PackReturnValues)
		int32 ReturnPropIndex = 0;
		if (InOutputParams.Num() > 1 && InOutputParams[0].ParamProp->HasAnyPropertyFlags(CPF_ReturnParm) && InOutputParams[0].ParamProp->IsA<UBoolProperty>())
		{
			ReturnPropIndex = 1; // Start packing at the 1st out value
		}

		// Do we need to return a packed tuple, or just a single value?
		const int32 NumPropertiesToPack = InOutputParams.Num() - ReturnPropIndex;
		if (NumPropertiesToPack == 1)
		{
			const PyGenUtil::FGeneratedWrappedMethodParameter& ReturnParam = InOutputParams[ReturnPropIndex];
			return PyGenUtil::PythonizeValue(ReturnParam.ParamProp, ReturnParam.ParamProp->ContainerPtrToValuePtr<void>(InBaseParamsAddr), PythonizeValueFlags);
		}
		else
		{
			FString FunctionReturnStr = TEXT("(");
			for (int32 PackedPropIndex = 0; ReturnPropIndex < InOutputParams.Num(); ++ReturnPropIndex, ++PackedPropIndex)
			{
				if (PackedPropIndex > 0)
				{
					FunctionReturnStr += TEXT(", ");
				}
				const PyGenUtil::FGeneratedWrappedMethodParameter& ReturnParam = InOutputParams[ReturnPropIndex];
				FunctionReturnStr += PyGenUtil::PythonizeValue(ReturnParam.ParamProp, ReturnParam.ParamProp->ContainerPtrToValuePtr<void>(InBaseParamsAddr), PythonizeValueFlags);
			}
			FunctionReturnStr += TEXT(")");
			return FunctionReturnStr;
		}
	};

	auto ExportConstantValue = [&OutPythonScript](const TCHAR* InConstantName, const TCHAR* InConstantDocString, const TCHAR* InConstantValue)
	{
		if (*InConstantDocString == 0)
		{
			// No docstring
			OutPythonScript.WriteLine(FString::Printf(TEXT("%s = %s"), InConstantName, InConstantValue));
		}
		else
		{
			if (FCString::Strchr(InConstantDocString, TEXT('\n')))
			{
				// Multi-line docstring
				OutPythonScript.WriteLine(FString::Printf(TEXT("%s = %s"), InConstantName, InConstantValue));
				OutPythonScript.WriteDocString(InConstantDocString);
				OutPythonScript.WriteNewLine();
			}
			else
			{
				// Single-line docstring
				OutPythonScript.WriteLine(FString::Printf(TEXT("%s = %s  #: %s"), InConstantName, InConstantValue, InConstantDocString));
			}
		}
	};

	auto ExportGetSet = [&OutPythonScript](const TCHAR* InGetSetName, const TCHAR* InGetSetDocString, const TCHAR* InGetReturnValue, const bool InReadOnly)
	{
		// Getter
		OutPythonScript.WriteLine(TEXT("@property"));
		OutPythonScript.WriteLine(FString::Printf(TEXT("def %s(self):"), InGetSetName));
		OutPythonScript.IncreaseIndent();
		OutPythonScript.WriteDocString(InGetSetDocString);
		OutPythonScript.WriteLine(FString::Printf(TEXT("return %s"), InGetReturnValue));
		OutPythonScript.DecreaseIndent();

		if (!InReadOnly)
		{
			// Setter
			OutPythonScript.WriteLine(FString::Printf(TEXT("@%s.setter"), InGetSetName));
			OutPythonScript.WriteLine(FString::Printf(TEXT("def %s(self, value):"), InGetSetName));
			OutPythonScript.IncreaseIndent();
			OutPythonScript.WriteLine(TEXT("pass"));
			OutPythonScript.DecreaseIndent();
		}
	};

	auto ExportGeneratedMethod = [&OutPythonScript, &GetFunctionReturnValue](const PyGenUtil::FGeneratedWrappedMethod& InTypeMethod)
	{
		FString MethodArgsStr;
		for (const PyGenUtil::FGeneratedWrappedMethodParameter& MethodParam : InTypeMethod.MethodFunc.InputParams)
		{
			MethodArgsStr += TEXT(", ");
			MethodArgsStr += UTF8_TO_TCHAR(MethodParam.ParamName.GetData());
			if (MethodParam.ParamDefaultValue.IsSet())
			{
				MethodArgsStr += TEXT("=");
				MethodArgsStr += PyGenUtil::PythonizeDefaultValue(MethodParam.ParamProp, MethodParam.ParamDefaultValue.GetValue());
			}
		}

		FString MethodReturnStr;
		if (InTypeMethod.MethodFunc.Func)
		{
			FStructOnScope FuncParams(InTypeMethod.MethodFunc.Func);
			MethodReturnStr = GetFunctionReturnValue(FuncParams.GetStructMemory(), InTypeMethod.MethodFunc.OutputParams);
		}
		else
		{
			MethodReturnStr = TEXT("None");
		}

		const bool bIsClassMethod = !!(InTypeMethod.MethodFlags & METH_CLASS);
		if (bIsClassMethod)
		{
			OutPythonScript.WriteLine(TEXT("@classmethod"));
			OutPythonScript.WriteLine(FString::Printf(TEXT("def %s(cls%s):"), UTF8_TO_TCHAR(InTypeMethod.MethodName.GetData()), *MethodArgsStr));
		}
		else
		{
			OutPythonScript.WriteLine(FString::Printf(TEXT("def %s(self%s):"), UTF8_TO_TCHAR(InTypeMethod.MethodName.GetData()), *MethodArgsStr));
		}
		OutPythonScript.IncreaseIndent();
		OutPythonScript.WriteDocString(UTF8_TO_TCHAR(InTypeMethod.MethodDoc.GetData()));
		OutPythonScript.WriteLine(FString::Printf(TEXT("return %s"), *MethodReturnStr));
		OutPythonScript.DecreaseIndent();
	};

	auto ExportGeneratedConstant = [&ExportConstantValue, &GetFunctionReturnValue](const PyGenUtil::FGeneratedWrappedConstant& InTypeConstant)
	{
		// Resolve the constant value
		FString ConstantValueStr;
		if (InTypeConstant.ConstantFunc.Func && InTypeConstant.ConstantFunc.InputParams.Num() == 0)
		{
			UClass* Class = InTypeConstant.ConstantFunc.Func->GetOwnerClass();
			UObject* Obj = Class->GetDefaultObject();

			FStructOnScope FuncParams(InTypeConstant.ConstantFunc.Func);
			PyUtil::InvokeFunctionCall(Obj, InTypeConstant.ConstantFunc.Func, FuncParams.GetStructMemory(), TEXT("export generated constant"));
			PyErr_Clear(); // Clear any errors in case InvokeFunctionCall failed

			ConstantValueStr = GetFunctionReturnValue(FuncParams.GetStructMemory(), InTypeConstant.ConstantFunc.OutputParams);
		}
		else
		{
			ConstantValueStr = TEXT("None");
		}
		ExportConstantValue(UTF8_TO_TCHAR(InTypeConstant.ConstantName.GetData()), UTF8_TO_TCHAR(InTypeConstant.ConstantDoc.GetData()), *ConstantValueStr);
	};

	auto ExportGeneratedGetSet = [&ExportGetSet](const PyGenUtil::FGeneratedWrappedGetSet& InGetSet)
	{
		// We use strict typing for return values to aid auto-complete (we also only care about the type and not the value, so structs can be default constructed)
		static const uint32 PythonizeValueFlags = PyGenUtil::EPythonizeValueFlags::UseStrictTyping | PyGenUtil::EPythonizeValueFlags::DefaultConstructStructs;
		const FString GetReturnValue = PyGenUtil::PythonizeDefaultValue(InGetSet.Prop.Prop, FString(), PythonizeValueFlags);
		const bool bIsReadOnly = InGetSet.Prop.Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly | CPF_EditConst);
		ExportGetSet(UTF8_TO_TCHAR(InGetSet.GetSetName.GetData()), UTF8_TO_TCHAR(InGetSet.GetSetDoc.GetData()), *GetReturnValue, bIsReadOnly);
	};

	auto ExportGeneratedOperator = [&OutPythonScript](const PyGenUtil::FGeneratedWrappedOperatorStack& InOpStack, const PyGenUtil::FGeneratedWrappedOperatorSignature& InOpSignature)
	{
		auto AppendFunctionTooltip = [](const UFunction* InFunc, const TCHAR* InIdentation, FString& OutStr)
		{
			const FString FuncTooltip = PyGenUtil::GetFieldTooltip(InFunc);
			TArray<FString> FuncTooltipLines;
			FuncTooltip.ParseIntoArrayLines(FuncTooltipLines, /*bCullEmpty*/false);

			bool bMultipleLines = false;
			for (const FString& FuncTooltipLine : FuncTooltipLines)
			{
				if (bMultipleLines)
				{
					OutStr += LINE_TERMINATOR;
					OutStr += InIdentation;
				}
				bMultipleLines = true;

				OutStr += FuncTooltipLine;
			}
		};

		FString OpDocString;
		if (InOpSignature.OtherType != PyGenUtil::FGeneratedWrappedOperatorSignature::EType::None)
		{
			OpDocString += TEXT("**Overloads:**") LINE_TERMINATOR;
			for (const PyGenUtil::FGeneratedWrappedOperatorFunction& OpFunc : InOpStack.Funcs)
			{
				if (OpFunc.OtherParam.ParamProp)
				{
					OpDocString += LINE_TERMINATOR TEXT("- ``");  // add as a list and code style
					OpDocString += PyGenUtil::GetPropertyTypePythonName(OpFunc.OtherParam.ParamProp);
					OpDocString += TEXT("`` ");
					AppendFunctionTooltip(OpFunc.Func, TEXT("  "), OpDocString);
				}
			}
		}
		else if (InOpStack.Funcs.Num() > 0)
		{
			AppendFunctionTooltip(InOpStack.Funcs[0].Func, TEXT(""), OpDocString);
		}

		OutPythonScript.WriteLine(FString::Printf(TEXT("def %s(self%s):"), InOpSignature.PyFuncName, (InOpSignature.OtherType == PyGenUtil::FGeneratedWrappedOperatorSignature::EType::None ? TEXT("") : TEXT(", other"))));
		OutPythonScript.IncreaseIndent();
		OutPythonScript.WriteDocString(OpDocString);
		OutPythonScript.WriteLine(InOpSignature.ReturnType == PyGenUtil::FGeneratedWrappedOperatorSignature::EType::Bool ? TEXT("return False") : TEXT("return None"));
		OutPythonScript.DecreaseIndent();
	};

	bool bHasExportedClassData = false;

	// Export the __init__ function for this type
	{
		bool bWriteDefaultInit = true;

		if (GeneratedTypeData)
		{
			const FGuid MetaGuid = GeneratedTypeData->MetaData->GetTypeId();

			if (MetaGuid == FPyWrapperObjectMetaData::StaticTypeId())
			{
				// Skip the __init__ function on derived object types as the base one is already correct
				bWriteDefaultInit = false;
			}
			else if (MetaGuid == FPyWrapperStructMetaData::StaticTypeId())
			{
				TSharedPtr<const FPyWrapperStructMetaData> StructMetaData = StaticCastSharedPtr<FPyWrapperStructMetaData>(GeneratedTypeData->MetaData);
				check(StructMetaData.IsValid());
				
				// Don't export FDateTime values for struct __init__ as they can be non-deterministic
				static const uint32 PythonizeValueFlags = PyGenUtil::EPythonizeValueFlags::DefaultConstructDateTime;

				// Python can only support 255 parameters, so if we have more than that for this struct just use the generic __init__ function
				FString InitParamsStr;
				if (StructMetaData->MakeFunc.Func)
				{
					bWriteDefaultInit = false;

					for (const PyGenUtil::FGeneratedWrappedMethodParameter& InitParam : StructMetaData->MakeFunc.InputParams)
					{
						InitParamsStr += TEXT(", ");
						InitParamsStr += UTF8_TO_TCHAR(InitParam.ParamName.GetData());
						if (InitParam.ParamDefaultValue.IsSet())
						{
							InitParamsStr += TEXT("=");
							InitParamsStr += PyGenUtil::PythonizeDefaultValue(InitParam.ParamProp, InitParam.ParamDefaultValue.GetValue(), PythonizeValueFlags);
						}
					}
				}
				else if (StructMetaData->InitParams.Num() <= 255)
				{
					bWriteDefaultInit = false;

					FStructOnScope StructData(StructMetaData->Struct);
					for (const PyGenUtil::FGeneratedWrappedMethodParameter& InitParam : StructMetaData->InitParams)
					{
						InitParamsStr += TEXT(", ");
						InitParamsStr += UTF8_TO_TCHAR(InitParam.ParamName.GetData());
						if (InitParam.ParamDefaultValue.IsSet())
						{
							InitParamsStr += TEXT("=");
							InitParamsStr += PyGenUtil::PythonizeValue(InitParam.ParamProp, InitParam.ParamProp->ContainerPtrToValuePtr<void>(StructData.GetStructMemory()), PythonizeValueFlags);
						}
					}
				}

				if (!bWriteDefaultInit)
				{
					bHasExportedClassData = true;

					OutPythonScript.WriteLine(FString::Printf(TEXT("def __init__(self%s):"), *InitParamsStr));
					OutPythonScript.IncreaseIndent();
					OutPythonScript.WriteLine(TEXT("pass"));
					OutPythonScript.DecreaseIndent();
				}
			}
			else if (MetaGuid == FPyWrapperEnumMetaData::StaticTypeId())
			{
				// Skip the __init__ function on derived enums
				bWriteDefaultInit = false;
			}
			// todo: have correct __init__ signatures for the other intrinsic types?
		}
		else if (PyType == &PyWrapperObjectType)
		{
			bWriteDefaultInit = false;
			bHasExportedClassData = true;

			OutPythonScript.WriteLine(TEXT("def __init__(self, outer=None, name=\"None\"):"));
			OutPythonScript.IncreaseIndent();
			OutPythonScript.WriteLine(TEXT("pass"));
			OutPythonScript.DecreaseIndent();
		}
		else if (PyType == &PyWrapperEnumType)
		{
			// Enums don't really have an __init__ function at runtime, so just give them a default one (with no arguments)
			bWriteDefaultInit = false;

			OutPythonScript.WriteLine(TEXT("def __init__(self):"));
			OutPythonScript.IncreaseIndent();
			OutPythonScript.WriteLine(TEXT("pass"));
			OutPythonScript.DecreaseIndent();
		}
		else if (PyType == &PyWrapperEnumValueDescrType)
		{
			bWriteDefaultInit = false;
			bHasExportedClassData = true;

			// This is a special internal decorator type used to define enum entries, which is why it has __get__ as well as __init__
			OutPythonScript.WriteLine(TEXT("def __init__(self, enum, name, value):"));
			OutPythonScript.IncreaseIndent();
			OutPythonScript.WriteLine(TEXT("self.enum = enum"));
			OutPythonScript.WriteLine(TEXT("self.name = name"));
			OutPythonScript.WriteLine(TEXT("self.value = value"));
			OutPythonScript.DecreaseIndent();

			OutPythonScript.WriteLine(TEXT("def __get__(self, obj, type=None):"));
			OutPythonScript.IncreaseIndent();
			OutPythonScript.WriteLine(TEXT("return self"));
			OutPythonScript.DecreaseIndent();

			// It also needs a __repr__ function for Sphinx to generate docs correctly
			OutPythonScript.WriteLine(TEXT("def __repr__(self):"));
			OutPythonScript.IncreaseIndent();
			OutPythonScript.WriteLine(TEXT("return \"{0}.{1}\".format(self.enum, self.name)"));
			OutPythonScript.DecreaseIndent();
		}

		if (bWriteDefaultInit)
		{
			bHasExportedClassData = true;
			
			OutPythonScript.WriteLine(TEXT("def __init__(self, *args, **kwargs):"));
			OutPythonScript.IncreaseIndent();
			OutPythonScript.WriteLine(TEXT("pass"));
			OutPythonScript.DecreaseIndent();
		}
	}

	TSet<const PyGetSetDef*> ExportedGetSets;

	if (GeneratedTypeData)
	{
		const FGuid MetaGuid = GeneratedTypeData->MetaData->GetTypeId();

		if (MetaGuid == FPyWrapperObjectMetaData::StaticTypeId())
		{
			// Export class get/sets
			const PyGenUtil::FGeneratedWrappedClassType* ClassType = static_cast<const PyGenUtil::FGeneratedWrappedClassType*>(GeneratedTypeData);

			check(ClassType->GetSets.TypeGetSets.Num() == (ClassType->GetSets.PyGetSets.Num() - 1)); // -1 as PyGetSets has a null terminator
			for (int32 GetSetIndex = 0; GetSetIndex < ClassType->GetSets.TypeGetSets.Num(); ++GetSetIndex)
			{
				bHasExportedClassData = true;
				ExportGeneratedGetSet(ClassType->GetSets.TypeGetSets[GetSetIndex]);
				ExportedGetSets.Add(&ClassType->GetSets.PyGetSets[GetSetIndex]);
			}
		}
		else if (MetaGuid == FPyWrapperStructMetaData::StaticTypeId())
		{
			// Export struct get/sets
			const PyGenUtil::FGeneratedWrappedStructType* StructType = static_cast<const PyGenUtil::FGeneratedWrappedStructType*>(GeneratedTypeData);

			check(StructType->GetSets.TypeGetSets.Num() == (StructType->GetSets.PyGetSets.Num() - 1)); // -1 as PyGetSets has a null terminator
			for (int32 GetSetIndex = 0; GetSetIndex < StructType->GetSets.TypeGetSets.Num(); ++GetSetIndex)
			{
				bHasExportedClassData = true;
				ExportGeneratedGetSet(StructType->GetSets.TypeGetSets[GetSetIndex]);
				ExportedGetSets.Add(&StructType->GetSets.PyGetSets[GetSetIndex]);
			}
		}
	}

	for (PyGetSetDef* GetSetDef = PyType->tp_getset; GetSetDef && GetSetDef->name; ++GetSetDef)
	{
		if (ExportedGetSets.Contains(GetSetDef))
		{
			continue;
		}
		ExportedGetSets.Add(GetSetDef);

		bHasExportedClassData = true;

		ExportGetSet(UTF8_TO_TCHAR(GetSetDef->name), UTF8_TO_TCHAR(GetSetDef->doc), TEXT("None"), /*IsReadOnly*/false);
	}

	for (PyMethodDef* MethodDef = PyType->tp_methods; MethodDef && MethodDef->ml_name; ++MethodDef)
	{
		bHasExportedClassData = true;

		const bool bIsClassMethod = !!(MethodDef->ml_flags & METH_CLASS);
		const bool bHasKeywords = !!(MethodDef->ml_flags & METH_KEYWORDS);
		if (bIsClassMethod)
		{
			OutPythonScript.WriteLine(TEXT("@classmethod"));
			OutPythonScript.WriteLine(FString::Printf(TEXT("def %s(cls, *args%s):"), UTF8_TO_TCHAR(MethodDef->ml_name), bHasKeywords ? TEXT(", **kwargs") : TEXT("")));
		}
		else
		{
			OutPythonScript.WriteLine(FString::Printf(TEXT("def %s(self, *args%s):"), UTF8_TO_TCHAR(MethodDef->ml_name), bHasKeywords ? TEXT(", **kwargs") : TEXT("")));
		}
		OutPythonScript.IncreaseIndent();
		OutPythonScript.WriteDocString(UTF8_TO_TCHAR(MethodDef->ml_doc));
		OutPythonScript.WriteLine(TEXT("pass"));
		OutPythonScript.DecreaseIndent();
	}

	if (GeneratedTypeData)
	{
		const FGuid MetaGuid = GeneratedTypeData->MetaData->GetTypeId();

		if (MetaGuid == FPyWrapperObjectMetaData::StaticTypeId())
		{
			// Export class methods and constants
			const PyGenUtil::FGeneratedWrappedClassType* ClassType = static_cast<const PyGenUtil::FGeneratedWrappedClassType*>(GeneratedTypeData);

			for (const PyGenUtil::FGeneratedWrappedMethod& TypeMethod : ClassType->Methods.TypeMethods)
			{
				bHasExportedClassData = true;
				ExportGeneratedMethod(TypeMethod);
			}

			for (const TSharedRef<PyGenUtil::FGeneratedWrappedDynamicMethodWithClosure>& DynamicMethod : ClassType->DynamicMethods)
			{
				bHasExportedClassData = true;
				ExportGeneratedMethod(*DynamicMethod);
			}

			for (const PyGenUtil::FGeneratedWrappedConstant& TypeConstant : ClassType->Constants.TypeConstants)
			{
				bHasExportedClassData = true;
				ExportGeneratedConstant(TypeConstant);
			}

			for (const TSharedRef<PyGenUtil::FGeneratedWrappedDynamicConstantWithClosure>& DynamicConstant : ClassType->DynamicConstants)
			{
				bHasExportedClassData = true;
				ExportGeneratedConstant(*DynamicConstant);
			}
		}
		else if (MetaGuid == FPyWrapperStructMetaData::StaticTypeId())
		{
			// Export struct methods and constants
			const PyGenUtil::FGeneratedWrappedStructType* StructType = static_cast<const PyGenUtil::FGeneratedWrappedStructType*>(GeneratedTypeData);

			TSharedPtr<const FPyWrapperStructMetaData> StructMetaData = StaticCastSharedPtr<FPyWrapperStructMetaData>(GeneratedTypeData->MetaData);
			check(StructMetaData.IsValid());

			for (const TSharedRef<PyGenUtil::FGeneratedWrappedDynamicMethodWithClosure>& DynamicMethod : StructType->DynamicMethods)
			{
				bHasExportedClassData = true;
				ExportGeneratedMethod(*DynamicMethod);
			}

			for (int32 OpTypeIndex = 0; OpTypeIndex < (int32)PyGenUtil::EGeneratedWrappedOperatorType::Num; ++OpTypeIndex)
			{
				const PyGenUtil::FGeneratedWrappedOperatorStack& OpStack = StructMetaData->OpStacks[OpTypeIndex];
				if (OpStack.Funcs.Num() > 0)
				{
					const PyGenUtil::FGeneratedWrappedOperatorSignature& OpSignature = PyGenUtil::FGeneratedWrappedOperatorSignature::OpTypeToSignature((PyGenUtil::EGeneratedWrappedOperatorType)OpTypeIndex);
					ExportGeneratedOperator(OpStack, OpSignature);
				}
			}

			for (const TSharedRef<PyGenUtil::FGeneratedWrappedDynamicConstantWithClosure>& DynamicConstant : StructType->DynamicConstants)
			{
				bHasExportedClassData = true;
				ExportGeneratedConstant(*DynamicConstant);
			}
		}
		else if (MetaGuid == FPyWrapperEnumMetaData::StaticTypeId())
		{
			// Export enum entries
			const PyGenUtil::FGeneratedWrappedEnumType* EnumType = static_cast<const PyGenUtil::FGeneratedWrappedEnumType*>(GeneratedTypeData);

			if (EnumType->EnumEntries.Num() > 0)
			{
				// Add extra line break for first enum member
				OutPythonScript.WriteNewLine();

				for (const PyGenUtil::FGeneratedWrappedEnumEntry& EnumMember : EnumType->EnumEntries)
				{
					const FString EntryName = UTF8_TO_TCHAR(EnumMember.EntryName.GetData());
					const FString EntryValue = LexToString(EnumMember.EntryValue);

					FString EntryDoc = UTF8_TO_TCHAR(EnumMember.EntryDoc.GetData());
					if (EntryDoc.IsEmpty())
					{
						EntryDoc = EntryValue;
					}
					else
					{
						EntryDoc.InsertAt(0, *FString::Printf(TEXT("%s: "), *EntryValue));
					}

					ExportConstantValue(*EntryName, *EntryDoc, *FString::Printf(TEXT("_EnumEntry(\"%s\", \"%s\", %s)"), *PyTypeName, *EntryName, *EntryValue));
				}
			}
		}
	}

	if (!bHasExportedClassData)
	{
		OutPythonScript.WriteLine(TEXT("pass"));
	}

	OutPythonScript.DecreaseIndent();
	OutPythonScript.WriteNewLine();
}

void FPyWrapperTypeRegistry::RegisterPythonTypeName(const FString& InPythonTypeName, const FName& InUnrealTypeName)
{
	const FName ExistingUnrealTypeName = PythonWrappedTypeNameToUnrealTypeName.FindRef(InPythonTypeName);
	if (ExistingUnrealTypeName.IsNone())
	{
		PythonWrappedTypeNameToUnrealTypeName.Add(InPythonTypeName, InUnrealTypeName);
	}
	else
	{
		REPORT_PYTHON_GENERATION_ISSUE(Warning, TEXT("'%s' and '%s' have the same name (%s) when exposed to Python. Rename one of them using 'ScriptName' meta-data."), *ExistingUnrealTypeName.ToString(), *InUnrealTypeName.ToString(), *InPythonTypeName);
	}
}

void FPyWrapperTypeRegistry::UnregisterPythonTypeName(const FString& InPythonTypeName, const FName& InUnrealTypeName)
{
	const FName* ExistingUnrealTypeNamePtr = PythonWrappedTypeNameToUnrealTypeName.Find(InPythonTypeName);
	if (ExistingUnrealTypeNamePtr && *ExistingUnrealTypeNamePtr == InUnrealTypeName)
	{
		PythonWrappedTypeNameToUnrealTypeName.Remove(InPythonTypeName);
	}
}

#endif	// WITH_PYTHON
