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
#include "PyGenUtil.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/EnumProperty.h"

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


FPyWrapperTypeRegistry& FPyWrapperTypeRegistry::Get()
{
	static FPyWrapperTypeRegistry Instance;
	return Instance;
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

	for (const UStruct* Struct : InGeneratedWrappedTypeReferences.StructReferences)
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

	if (const UStruct* Struct = Cast<const UStruct>(InObj))
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
	TMap<FName, FName> PythonMethods;

	auto GenerateWrappedProperty = [this, InClass, &PythonProperties, &GeneratedWrappedType, &OutGeneratedWrappedTypeReferences](const UProperty* InProp)
	{
		const bool bExportPropertyToScript = PyGenUtil::ShouldExportProperty(InProp);
		const bool bExportPropertyToEditor = PyGenUtil::ShouldExportEditorOnlyProperty(InProp);

		if (bExportPropertyToScript || bExportPropertyToEditor)
		{
			GatherWrappedTypesForPropertyReferences(InProp, OutGeneratedWrappedTypeReferences);

			const PyGenUtil::FGeneratedWrappedPropertyDoc& GeneratedPropertyDoc = GeneratedWrappedType->PropertyDocs[GeneratedWrappedType->PropertyDocs.Emplace(InProp)];
			PythonProperties.Add(*GeneratedPropertyDoc.PythonPropName, InProp->GetFName());

			if (bExportPropertyToScript)
			{
				PyGenUtil::FGeneratedWrappedGetSet& GeneratedWrappedGetSet = GeneratedWrappedType->GetSets.TypeGetSets.AddDefaulted_GetRef();
				GeneratedWrappedGetSet.GetSetName = PyGenUtil::TCHARToUTF8Buffer(*GeneratedPropertyDoc.PythonPropName);
				GeneratedWrappedGetSet.GetSetDoc = PyGenUtil::TCHARToUTF8Buffer(*GeneratedPropertyDoc.DocString);
				GeneratedWrappedGetSet.Prop = InProp;
				GeneratedWrappedGetSet.GetFunc.SetFunctionAndExtractParams(InClass->FindFunctionByName(*InProp->GetMetaData(PyGenUtil::BlueprintGetterMetaDataKey)));
				GeneratedWrappedGetSet.SetFunc.SetFunctionAndExtractParams(InClass->FindFunctionByName(*InProp->GetMetaData(PyGenUtil::BlueprintSetterMetaDataKey)));
				GeneratedWrappedGetSet.GetCallback = (getter)&FPyWrapperObject::Getter_Impl;
				GeneratedWrappedGetSet.SetCallback = (setter)&FPyWrapperObject::Setter_Impl;
			}
		}
	};

	auto GenerateDynamicStructMethod = [this, &OutGeneratedWrappedTypeReferences, &OutDirtyModules](const UFunction* InFunc, const PyGenUtil::FGeneratedWrappedMethod& InTypeMethod)
	{
		// Only static functions can be hoisted into the struct
		if (!InFunc->HasAnyFunctionFlags(FUNC_Static))
		{
			UE_LOG(LogPython, Warning, TEXT("Non-static function '%s' is marked as 'ScriptMethod' but only static functions can be hoisted."), *InFunc->GetName());
			return;
		}

		// Get the struct type to hoist this method to (this should be the first parameter)
		PyGenUtil::FGeneratedWrappedMethodParameter StructParam;
		if (InTypeMethod.MethodFunc.InputParams.Num() > 0 && InTypeMethod.MethodFunc.InputParams[0].ParamProp->IsA<UStructProperty>())
		{
			StructParam = InTypeMethod.MethodFunc.InputParams[0];
		}
		if (!StructParam.ParamProp)
		{
			UE_LOG(LogPython, Warning, TEXT("Function '%s' is marked as 'ScriptMethod' but doesn't contain a valid struct as its first argument."), *InFunc->GetName());
			return;
		}

		// Ensure that we've generated a finalized Python type for this struct since we'll be adding this function as a dynamic method on that type
		const UStruct* Struct = CastChecked<UStructProperty>(StructParam.ParamProp)->Struct;
		if (!GenerateWrappedStructType(Struct, OutGeneratedWrappedTypeReferences, OutDirtyModules, true))
		{
			return;
		}

		// Find the wrapped type for the struct as that's what we'll actually add the dynamic method to
		TSharedPtr<PyGenUtil::FGeneratedWrappedStructType> StructGeneratedWrappedType = StaticCastSharedPtr<PyGenUtil::FGeneratedWrappedStructType>(GeneratedWrappedTypes.FindRef(Struct->GetFName()));
		check(StructGeneratedWrappedType.IsValid());

		// Copy the basic wrapped method as we need to adjust some parts of it below
		PyGenUtil::FGeneratedWrappedDynamicStructMethod GeneratedWrappedDynamicStructMethod;
		static_cast<PyGenUtil::FGeneratedWrappedMethod&>(GeneratedWrappedDynamicStructMethod) = InTypeMethod;
		GeneratedWrappedDynamicStructMethod.StructParam = StructParam;

		// Hoisted methods may use an optional name alias
		FString PythonStructMethodName = InFunc->GetMetaData(PyGenUtil::ScriptMethodMetaDataKey);
		if (PythonStructMethodName.IsEmpty())
		{
			PythonStructMethodName = UTF8_TO_TCHAR(InTypeMethod.MethodName.GetData());
		}
		else
		{
			PythonStructMethodName = PythonizeName(PythonStructMethodName, PyGenUtil::EPythonizeNameCase::Lower);
		}
		GeneratedWrappedDynamicStructMethod.MethodName = PyGenUtil::TCHARToUTF8Buffer(*PythonStructMethodName);
		
		// We remove the first function parameter, as that's the struct and we'll infer that when we call
		GeneratedWrappedDynamicStructMethod.MethodFunc.InputParams.RemoveAt(0, 1, /*bAllowShrinking*/false);

		// Set-up some data needed to build the tooltip correctly for the hoisted method
		const bool bIsStaticOverride = false;
		TSet<FName> ParamsToIgnore;
		ParamsToIgnore.Add(StructParam.ParamProp->GetFName());

		// Update the doc string for the method
		FString PythonStructMethodDocString = PyGenUtil::BuildFunctionDocString(InFunc, PythonStructMethodName, GeneratedWrappedDynamicStructMethod.MethodFunc.InputParams, GeneratedWrappedDynamicStructMethod.MethodFunc.OutputParams, &bIsStaticOverride);
		PythonStructMethodDocString += TEXT(" -- ");
		PythonStructMethodDocString += PyGenUtil::PythonizeFunctionTooltip(PyGenUtil::GetFieldTooltip(InFunc), InFunc, ParamsToIgnore);
		GeneratedWrappedDynamicStructMethod.MethodDoc = PyGenUtil::TCHARToUTF8Buffer(*PythonStructMethodDocString);

		// Update the flags as removing the struct argument may have changed the calling convention
		GeneratedWrappedDynamicStructMethod.MethodFlags = GeneratedWrappedDynamicStructMethod.MethodFunc.InputParams.Num() > 0 ? METH_VARARGS | METH_KEYWORDS : METH_NOARGS;

		// Set the correct function pointer for calling this function and inject the struct argument
		GeneratedWrappedDynamicStructMethod.MethodCallback = GeneratedWrappedDynamicStructMethod.MethodFunc.InputParams.Num() > 0 ? PyCFunctionWithClosureCast(&FPyWrapperStruct::CallMethodWithArgs_Impl) : PyCFunctionWithClosureCast(&FPyWrapperStruct::CallMethodNoArgs_Impl);

		// Finished, add the dynamic method now
		StructGeneratedWrappedType->AddDynamicStructMethod(MoveTemp(GeneratedWrappedDynamicStructMethod));
	};

	auto GenerateStructMathOp = [this, &OutGeneratedWrappedTypeReferences, &OutDirtyModules](const UFunction* InFunc, const PyGenUtil::FGeneratedWrappedMethod& InTypeMethod)
	{
		// Only static functions can be hoisted into the struct
		if (!InFunc->HasAnyFunctionFlags(FUNC_Static))
		{
			UE_LOG(LogPython, Warning, TEXT("Non-static function '%s' is marked as 'ScriptMathOp' but only static functions can be hoisted."), *InFunc->GetName());
			return;
		}

		// Get the struct type to hoist this method to (this should be the first parameter)
		PyGenUtil::FGeneratedWrappedStructMathOpFunction MathOpFunc;
		static_cast<PyGenUtil::FGeneratedWrappedFunction&>(MathOpFunc) = InTypeMethod.MethodFunc;
		if (MathOpFunc.InputParams.Num() > 0 && MathOpFunc.InputParams[0].ParamProp->IsA<UStructProperty>())
		{
			MathOpFunc.StructParam = MathOpFunc.InputParams[0];
			MathOpFunc.InputParams.RemoveAt(0, 1, /*bAllowShrinking*/false);
		}
		if (!MathOpFunc.StructParam.ParamProp)
		{
			UE_LOG(LogPython, Warning, TEXT("Function '%s' is marked as 'ScriptMathOp' but doesn't contain a valid struct as its first argument."), *InFunc->GetName());
			return;
		}

		// Ensure that we've generated a finalized Python type for this struct since we'll be adding this function as a operator on that type
		const UStruct* Struct = CastChecked<UStructProperty>(MathOpFunc.StructParam.ParamProp)->Struct;
		if (!GenerateWrappedStructType(Struct, OutGeneratedWrappedTypeReferences, OutDirtyModules, true))
		{
			return;
		}

		// Find the wrapped type for the struct as that's what we'll actually add the operator to (via its meta-data)
		TSharedPtr<PyGenUtil::FGeneratedWrappedStructType> StructGeneratedWrappedType = StaticCastSharedPtr<PyGenUtil::FGeneratedWrappedStructType>(GeneratedWrappedTypes.FindRef(Struct->GetFName()));
		check(StructGeneratedWrappedType.IsValid());

		// Add the function to each specified operator
		const FString& MathOpsStr = InFunc->GetMetaData(PyGenUtil::ScriptMathOpMetaDataKey);
		TArray<FString> MathOpStrArray;
		MathOpsStr.ParseIntoArray(MathOpStrArray, TEXT(";"));
		for (const FString& MathOpStr : MathOpStrArray)
		{
			PyGenUtil::FGeneratedWrappedStructMathOpStack::EOpType MathOp;
			if (PyGenUtil::FGeneratedWrappedStructMathOpStack::StringToOpType(*MathOpStr, MathOp))
			{
				StaticCastSharedPtr<FPyWrapperStructMetaData>(StructGeneratedWrappedType->MetaData)->MathOpStacks[(int32)MathOp].MathOpFuncs.Add(MathOpFunc);
			}
		}
	};

	auto GenerateWrappedMethod = [this, &PythonMethods, &GeneratedWrappedType, &OutGeneratedWrappedTypeReferences, &GenerateDynamicStructMethod, &GenerateStructMathOp](const UFunction* InFunc)
	{
		if (!PyGenUtil::ShouldExportFunction(InFunc))
		{
			return;
		}

		const FString PythonFunctionName = PyGenUtil::GetFunctionPythonName(InFunc);
		const bool bIsStatic = InFunc->HasAnyFunctionFlags(FUNC_Static);
		
		PythonMethods.Add(*PythonFunctionName, InFunc->GetFName());

		PyGenUtil::FGeneratedWrappedMethod& GeneratedWrappedMethod = GeneratedWrappedType->Methods.TypeMethods.AddDefaulted_GetRef();
		GeneratedWrappedMethod.MethodName = PyGenUtil::TCHARToUTF8Buffer(*PythonFunctionName);
		GeneratedWrappedMethod.MethodFunc.SetFunctionAndExtractParams(InFunc);

		for (TFieldIterator<const UProperty> ParamIt(InFunc); ParamIt; ++ParamIt)
		{
			const UProperty* Param = *ParamIt;
			GatherWrappedTypesForPropertyReferences(Param, OutGeneratedWrappedTypeReferences);
		}

		FString FunctionDeclDocString = PyGenUtil::BuildFunctionDocString(InFunc, PythonFunctionName, GeneratedWrappedMethod.MethodFunc.InputParams, GeneratedWrappedMethod.MethodFunc.OutputParams);
		FunctionDeclDocString += TEXT(" -- ");
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

		// Should this function also be hoisted as a struct method or math op?
		if (InFunc->HasMetaData(PyGenUtil::ScriptMethodMetaDataKey))
		{
			GenerateDynamicStructMethod(InFunc, GeneratedWrappedMethod);
		}
		if (InFunc->HasMetaData(PyGenUtil::ScriptMathOpMetaDataKey))
		{
			GenerateStructMathOp(InFunc, GeneratedWrappedMethod);
		}
	};

	GeneratedWrappedType->TypeName = PyGenUtil::TCHARToUTF8Buffer(*PyGenUtil::GetClassPythonName(InClass));

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
	PyGenUtil::FGeneratedWrappedPropertyDoc::AppendDocString(GeneratedWrappedType->PropertyDocs, TypeDocString, /*bEditorVariant*/true);
	GeneratedWrappedType->TypeDoc = PyGenUtil::TCHARToUTF8Buffer(*TypeDocString);

	GeneratedWrappedType->PyType.tp_basicsize = sizeof(FPyWrapperObject);
	GeneratedWrappedType->PyType.tp_base = SuperPyType ? SuperPyType : &PyWrapperObjectType;
	GeneratedWrappedType->PyType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;

	TSharedRef<FPyWrapperObjectMetaData> ObjectMetaData = MakeShared<FPyWrapperObjectMetaData>();
	ObjectMetaData->Class = (UClass*)InClass;
	ObjectMetaData->PythonProperties = MoveTemp(PythonProperties);
	ObjectMetaData->PythonMethods = MoveTemp(PythonMethods);
	GeneratedWrappedType->MetaData = ObjectMetaData;

	if (GeneratedWrappedType->Finalize())
	{
		const FName UnrealModuleName = *PyGenUtil::GetFieldModule(InClass);
		GeneratedWrappedTypesForModule.Add(UnrealModuleName, InClass->GetFName());
		OutDirtyModules.Add(UnrealModuleName);

		const FString PyModuleName = PyGenUtil::GetModulePythonName(UnrealModuleName);
		PyObject* PyModule = PyImport_AddModule(TCHAR_TO_UTF8(*PyModuleName));

		Py_INCREF(&GeneratedWrappedType->PyType);
		PyModule_AddObject(PyModule, GeneratedWrappedType->PyType.tp_name, (PyObject*)&GeneratedWrappedType->PyType);

		RegisterWrappedClassType(InClass->GetFName(), &GeneratedWrappedType->PyType);
		return &GeneratedWrappedType->PyType;
	}
	
	UE_LOG(LogPython, Fatal, TEXT("Failed to generate Python glue code for class '%s'!"), *InClass->GetName());
	return nullptr;
}

void FPyWrapperTypeRegistry::RegisterWrappedClassType(const FName ClassName, PyTypeObject* PyType)
{
	PythonWrappedClasses.Add(ClassName, PyType);
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

PyTypeObject* FPyWrapperTypeRegistry::GenerateWrappedStructType(const UStruct* InStruct, FGeneratedWrappedTypeReferences& OutGeneratedWrappedTypeReferences, TSet<FName>& OutDirtyModules, const bool bForceGenerate)
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_GenerateWrappedStructTotalTime);
	INC_DWORD_STAT(STAT_GenerateWrappedStructCallCount);

	struct FFuncs
	{
		static int Init(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds)
		{
			const int SuperResult = PyWrapperStructType.tp_init((PyObject*)InSelf, InArgs, InKwds);
			if (SuperResult != 0)
			{
				return SuperResult;
			}

			return FPyWrapperStruct::SetPropertyValues(InSelf, InArgs, InKwds);
		}
	};

	// Already processed? Nothing more to do
	if (PyTypeObject* ExistingPyType = PythonWrappedStructs.FindRef(InStruct->GetFName()))
	{
		return ExistingPyType;
	}

	// UFunction derives from UStruct to define its arguments, but we should never process a UFunction as a UStruct for Python
	if (InStruct->IsA<UFunction>())
	{
		return nullptr;
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
	if (const UStruct* SuperStruct = InStruct->GetSuperStruct())
	{
		SuperPyType = GenerateWrappedStructType(SuperStruct, OutGeneratedWrappedTypeReferences, OutDirtyModules, true);
	}

	INC_DWORD_STAT(STAT_GenerateWrappedStructObjCount);

	check(!GeneratedWrappedTypes.Contains(InStruct->GetFName()));
	TSharedRef<PyGenUtil::FGeneratedWrappedStructType> GeneratedWrappedType = MakeShared<PyGenUtil::FGeneratedWrappedStructType>();
	GeneratedWrappedTypes.Add(InStruct->GetFName(), GeneratedWrappedType);

	TMap<FName, FName> PythonProperties;

	auto GenerateWrappedProperty = [this, &PythonProperties, &GeneratedWrappedType, &OutGeneratedWrappedTypeReferences](const UProperty* InProp)
	{
		const bool bExportPropertyToScript = PyGenUtil::ShouldExportProperty(InProp);
		const bool bExportPropertyToEditor = PyGenUtil::ShouldExportEditorOnlyProperty(InProp);

		if (bExportPropertyToScript || bExportPropertyToEditor)
		{
			GatherWrappedTypesForPropertyReferences(InProp, OutGeneratedWrappedTypeReferences);

			const PyGenUtil::FGeneratedWrappedPropertyDoc& GeneratedPropertyDoc = GeneratedWrappedType->PropertyDocs[GeneratedWrappedType->PropertyDocs.Emplace(InProp)];
			PythonProperties.Add(*GeneratedPropertyDoc.PythonPropName, InProp->GetFName());

			if (bExportPropertyToScript)
			{
				PyGenUtil::FGeneratedWrappedGetSet& GeneratedWrappedGetSet = GeneratedWrappedType->GetSets.TypeGetSets.AddDefaulted_GetRef();
				GeneratedWrappedGetSet.GetSetName = PyGenUtil::TCHARToUTF8Buffer(*GeneratedPropertyDoc.PythonPropName);
				GeneratedWrappedGetSet.GetSetDoc = PyGenUtil::TCHARToUTF8Buffer(*GeneratedPropertyDoc.DocString);
				GeneratedWrappedGetSet.Prop = InProp;
				GeneratedWrappedGetSet.GetCallback = (getter)&FPyWrapperStruct::Getter_Impl;
				GeneratedWrappedGetSet.SetCallback = (setter)&FPyWrapperStruct::Setter_Impl;
			}
		}
	};

	GeneratedWrappedType->TypeName = PyGenUtil::TCHARToUTF8Buffer(*PyGenUtil::GetStructPythonName(InStruct));

	for (TFieldIterator<const UProperty> PropIt(InStruct, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
	{
		const UProperty* Prop = *PropIt;
		GenerateWrappedProperty(Prop);
	}

	FString TypeDocString = PyGenUtil::PythonizeTooltip(PyGenUtil::GetFieldTooltip(InStruct));
	if (const UStruct* SuperStruct = InStruct->GetSuperStruct())
	{
		TSharedPtr<PyGenUtil::FGeneratedWrappedStructType> SuperGeneratedWrappedType = StaticCastSharedPtr<PyGenUtil::FGeneratedWrappedStructType>(GeneratedWrappedTypes.FindRef(SuperStruct->GetFName()));
		if (SuperGeneratedWrappedType.IsValid())
		{
			GeneratedWrappedType->PropertyDocs.Append(SuperGeneratedWrappedType->PropertyDocs);
		}
	}
	GeneratedWrappedType->PropertyDocs.Sort(&PyGenUtil::FGeneratedWrappedPropertyDoc::SortPredicate);
	PyGenUtil::FGeneratedWrappedPropertyDoc::AppendDocString(GeneratedWrappedType->PropertyDocs, TypeDocString, /*bEditorVariant*/true);
	GeneratedWrappedType->TypeDoc = PyGenUtil::TCHARToUTF8Buffer(*TypeDocString);

	GeneratedWrappedType->PyType.tp_basicsize = sizeof(FPyWrapperStruct);
	GeneratedWrappedType->PyType.tp_base = SuperPyType ? SuperPyType : &PyWrapperStructType;
	GeneratedWrappedType->PyType.tp_init = (initproc)&FFuncs::Init;
	GeneratedWrappedType->PyType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;

	TSharedRef<FPyWrapperStructMetaData> StructMetaData = MakeShared<FPyWrapperStructMetaData>();
	StructMetaData->Struct = (UStruct*)InStruct;
	StructMetaData->PythonProperties = MoveTemp(PythonProperties);
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
		PyGenUtil::FGeneratedWrappedMethodParameter& GeneratedInitParam = StructMetaData->InitParams.AddDefaulted_GetRef();
		GeneratedInitParam.ParamName = GeneratedWrappedGetSet.GetSetName;
		GeneratedInitParam.ParamProp = GeneratedWrappedGetSet.Prop;
		GeneratedInitParam.ParamDefaultValue = FString();
	}
	GeneratedWrappedType->MetaData = StructMetaData;

	if (GeneratedWrappedType->Finalize())
	{
		const FName UnrealModuleName = *PyGenUtil::GetFieldModule(InStruct);
		GeneratedWrappedTypesForModule.Add(UnrealModuleName, InStruct->GetFName());
		OutDirtyModules.Add(UnrealModuleName);

		const FString PyModuleName = PyGenUtil::GetModulePythonName(UnrealModuleName);
		PyObject* PyModule = PyImport_AddModule(TCHAR_TO_UTF8(*PyModuleName));

		Py_INCREF(&GeneratedWrappedType->PyType);
		PyModule_AddObject(PyModule, GeneratedWrappedType->PyType.tp_name, (PyObject*)&GeneratedWrappedType->PyType);

		RegisterWrappedStructType(InStruct->GetFName(), &GeneratedWrappedType->PyType);
		return &GeneratedWrappedType->PyType;
	}

	UE_LOG(LogPython, Fatal, TEXT("Failed to generate Python glue code for struct '%s'!"), *InStruct->GetName());
	return nullptr;
}

void FPyWrapperTypeRegistry::RegisterWrappedStructType(const FName StructName, PyTypeObject* PyType)
{
	PythonWrappedStructs.Add(StructName, PyType);
}

PyTypeObject* FPyWrapperTypeRegistry::GetWrappedStructType(const UStruct* InStruct) const
{
	PyTypeObject* PyType = &PyWrapperStructType;

	for (const UStruct* Struct = InStruct; Struct; Struct = Struct->GetSuperStruct())
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
	TSharedRef<PyGenUtil::FGeneratedWrappedType> GeneratedWrappedType = MakeShared<PyGenUtil::FGeneratedWrappedType>();
	GeneratedWrappedTypes.Add(InEnum->GetFName(), GeneratedWrappedType);

	GeneratedWrappedType->TypeName = PyGenUtil::TCHARToUTF8Buffer(*PyGenUtil::GetEnumPythonName(InEnum));
	GeneratedWrappedType->TypeDoc = PyGenUtil::TCHARToUTF8Buffer(*PyGenUtil::PythonizeTooltip(PyGenUtil::GetFieldTooltip(InEnum)));

	GeneratedWrappedType->PyType.tp_basicsize = sizeof(FPyWrapperEnum);
	GeneratedWrappedType->PyType.tp_base = &PyWrapperEnumType;
	GeneratedWrappedType->PyType.tp_flags = Py_TPFLAGS_DEFAULT;

	TSharedRef<FPyWrapperEnumMetaData> EnumMetaData = MakeShared<FPyWrapperEnumMetaData>();
	EnumMetaData->Enum = (UEnum*)InEnum;
	GeneratedWrappedType->MetaData = EnumMetaData;

	if (GeneratedWrappedType->Finalize())
	{
		// Register the enum values
		for (int32 EnumEntryIndex = 0; EnumEntryIndex < InEnum->NumEnums() - 1; ++EnumEntryIndex)
		{
			if (PyGenUtil::ShouldExportEnumEntry(InEnum, EnumEntryIndex))
			{
				const int64 EnumEntryValue = InEnum->GetValueByIndex(EnumEntryIndex);
				const FString EnumEntryShortName = InEnum->GetNameStringByIndex(EnumEntryIndex);
				const FString EnumEntryShortPythonName = PyGenUtil::PythonizeName(EnumEntryShortName, PyGenUtil::EPythonizeNameCase::Upper);
				const FString EnumEntryDoc = PyGenUtil::PythonizeTooltip(PyGenUtil::GetEnumEntryTooltip(InEnum, EnumEntryIndex));

				FPyWrapperEnum::SetEnumEntryValue(&GeneratedWrappedType->PyType, EnumEntryValue, TCHAR_TO_UTF8(*EnumEntryShortPythonName), TCHAR_TO_UTF8(*EnumEntryDoc));
			}
		}

		const FName UnrealModuleName = *PyGenUtil::GetFieldModule(InEnum);
		GeneratedWrappedTypesForModule.Add(UnrealModuleName, InEnum->GetFName());
		OutDirtyModules.Add(UnrealModuleName);

		const FString PyModuleName = PyGenUtil::GetModulePythonName(UnrealModuleName);
		PyObject* PyModule = PyImport_AddModule(TCHAR_TO_UTF8(*PyModuleName));

		Py_INCREF(&GeneratedWrappedType->PyType);
		PyModule_AddObject(PyModule, GeneratedWrappedType->PyType.tp_name, (PyObject*)&GeneratedWrappedType->PyType);

		RegisterWrappedEnumType(InEnum->GetFName(), &GeneratedWrappedType->PyType);
		return &GeneratedWrappedType->PyType;
	}

	UE_LOG(LogPython, Fatal, TEXT("Failed to generate Python glue code for enum '%s'!"), *InEnum->GetName());
	return nullptr;
}

void FPyWrapperTypeRegistry::RegisterWrappedEnumType(const FName EnumName, PyTypeObject* PyType)
{
	PythonWrappedEnums.Add(EnumName, PyType);
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

	const FString DelegateBaseTypename = PyGenUtil::GetDelegatePythonName(InDelegateSignature);
	GeneratedWrappedType->TypeName = PyGenUtil::TCHARToUTF8Buffer(*DelegateBaseTypename);
	GeneratedWrappedType->TypeDoc = PyGenUtil::TCHARToUTF8Buffer(*PyGenUtil::PythonizeFunctionTooltip(PyGenUtil::GetFieldTooltip(InDelegateSignature), InDelegateSignature));

	GeneratedWrappedType->PyType.tp_flags = Py_TPFLAGS_DEFAULT;

	if (InDelegateSignature->HasAnyFunctionFlags(FUNC_MulticastDelegate))
	{
		GeneratedWrappedType->PyType.tp_basicsize = sizeof(FPyWrapperMulticastDelegate);
		GeneratedWrappedType->PyType.tp_base = &PyWrapperMulticastDelegateType;

		TSharedRef<FPyWrapperMulticastDelegateMetaData> DelegateMetaData = MakeShared<FPyWrapperMulticastDelegateMetaData>();
		DelegateMetaData->DelegateSignature.SetFunctionAndExtractParams(InDelegateSignature);
		GeneratedWrappedType->MetaData = DelegateMetaData;
	}
	else
	{
		GeneratedWrappedType->PyType.tp_basicsize = sizeof(FPyWrapperDelegate);
		GeneratedWrappedType->PyType.tp_base = &PyWrapperDelegateType;

		TSharedRef<FPyWrapperDelegateMetaData> DelegateMetaData = MakeShared<FPyWrapperDelegateMetaData>();
		DelegateMetaData->DelegateSignature.SetFunctionAndExtractParams(InDelegateSignature);
		GeneratedWrappedType->MetaData = DelegateMetaData;
	}

	if (GeneratedWrappedType->Finalize())
	{
		const FName UnrealModuleName = *PyGenUtil::GetFieldModule(InDelegateSignature);
		GeneratedWrappedTypesForModule.Add(UnrealModuleName, InDelegateSignature->GetFName());
		OutDirtyModules.Add(UnrealModuleName);

		const FString PyModuleName = PyGenUtil::GetModulePythonName(UnrealModuleName);
		PyObject* PyModule = PyImport_AddModule(TCHAR_TO_UTF8(*PyModuleName));

		Py_INCREF(&GeneratedWrappedType->PyType);
		PyModule_AddObject(PyModule, GeneratedWrappedType->PyType.tp_name, (PyObject*)&GeneratedWrappedType->PyType);

		RegisterWrappedDelegateType(InDelegateSignature->GetFName(), &GeneratedWrappedType->PyType);
		return &GeneratedWrappedType->PyType;
	}

	UE_LOG(LogPython, Fatal, TEXT("Failed to generate Python glue code for delegate '%s'!"), *InDelegateSignature->GetName());
	return nullptr;
}

void FPyWrapperTypeRegistry::RegisterWrappedDelegateType(const FName DelegateName, PyTypeObject* PyType)
{
	PythonWrappedDelegates.Add(DelegateName, PyType);
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

void FPyWrapperTypeRegistry::GatherWrappedTypesForPropertyReferences(const UProperty* InProp, FGeneratedWrappedTypeReferences& OutGeneratedWrappedTypeReferences)
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

#endif	// WITH_PYTHON
