// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PyWrapperStruct.h"
#include "PyWrapperTypeRegistry.h"
#include "PyGIL.h"
#include "PyCore.h"
#include "PyReferenceCollector.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "UObject/PropertyPortFlags.h"
#include "Templates/Casts.h"
#include "Engine/UserDefinedStruct.h"

#if WITH_PYTHON

void InitializePyWrapperStruct(PyGenUtil::FNativePythonModule& ModuleInfo)
{
	if (PyType_Ready(&PyWrapperStructType) == 0)
	{
		static FPyWrapperStructMetaData MetaData;
		FPyWrapperStructMetaData::SetMetaData(&PyWrapperStructType, &MetaData);
		ModuleInfo.AddType(&PyWrapperStructType);
	}
}

const IPyWrapperStructAllocationPolicy* GetPyWrapperStructAllocationPolicy(UScriptStruct* InStruct)
{
	class FPyWrapperStructAllocationPolicy_Heap : public IPyWrapperStructAllocationPolicy
	{
		virtual void* AllocateStruct(const FPyWrapperStruct* InSelf, UScriptStruct* InStruct) const override
		{
			return FMemory::Malloc(FMath::Max(InStruct->GetStructureSize(), 1));
		}

		virtual void FreeStruct(const FPyWrapperStruct* InSelf, void* InAlloc) const override
		{
			FMemory::Free(InAlloc);
		}
	};

	if (const IPyWrapperInlineStructFactory* InlineStructFactory = FPyWrapperTypeRegistry::Get().GetInlineStructFactory(InStruct->GetFName()))
	{
		return InlineStructFactory->GetPythonObjectAllocationPolicy();
	}

	static const FPyWrapperStructAllocationPolicy_Heap HeapAllocPolicy = FPyWrapperStructAllocationPolicy_Heap();
	return &HeapAllocPolicy;
}

FPyWrapperStruct* FPyWrapperStruct::New(PyTypeObject* InType)
{
	FPyWrapperStruct* Self = (FPyWrapperStruct*)FPyWrapperBase::New(InType);
	if (Self)
	{
		new(&Self->OwnerContext) FPyWrapperOwnerContext();
		Self->ScriptStruct = nullptr;
		Self->StructInstance = nullptr;
	}
	return Self;
}

void FPyWrapperStruct::Free(FPyWrapperStruct* InSelf)
{
	Deinit(InSelf);

	InSelf->OwnerContext.~FPyWrapperOwnerContext();
	FPyWrapperBase::Free(InSelf);
}

int FPyWrapperStruct::Init(FPyWrapperStruct* InSelf)
{
	Deinit(InSelf);

	const int BaseInit = FPyWrapperBase::Init(InSelf);
	if (BaseInit != 0)
	{
		return BaseInit;
	}

	UScriptStruct* Struct = FPyWrapperStructMetaData::GetStruct(InSelf);
	if (!Struct)
	{
		PyUtil::SetPythonError(PyExc_Exception, InSelf, TEXT("Struct is null"));
		return -1;
	}

	const IPyWrapperStructAllocationPolicy* AllocPolicy = GetPyWrapperStructAllocationPolicy(Struct);
	if (!AllocPolicy)
	{
		PyUtil::SetPythonError(PyExc_Exception, InSelf, TEXT("AllocPolicy is null"));
		return -1;
	}

	// Deprecated structs emit a warning
	{
		FString DeprecationMessage;
		if (FPyWrapperStructMetaData::IsStructDeprecated(InSelf, &DeprecationMessage) &&
			PyUtil::SetPythonWarning(PyExc_DeprecationWarning, InSelf, *FString::Printf(TEXT("Struct '%s' is deprecated: %s"), UTF8_TO_TCHAR(Py_TYPE(InSelf)->tp_name), *DeprecationMessage)) == -1
			)
		{
			// -1 from SetPythonWarning means the warning should be an exception
			return -1;
		}
	}

	void* StructInstance = AllocPolicy->AllocateStruct(InSelf, Struct);
	Struct->InitializeStruct(StructInstance);

	InSelf->ScriptStruct = Struct;
	InSelf->StructInstance = StructInstance;

	FPyWrapperStructFactory::Get().MapInstance(InSelf->StructInstance, InSelf);
	return 0;
}

int FPyWrapperStruct::Init(FPyWrapperStruct* InSelf, const FPyWrapperOwnerContext& InOwnerContext, UScriptStruct* InStruct, void* InValue, const EPyConversionMethod InConversionMethod)
{
	InOwnerContext.AssertValidConversionMethod(InConversionMethod);

	Deinit(InSelf);

	const int BaseInit = FPyWrapperBase::Init(InSelf);
	if (BaseInit != 0)
	{
		return BaseInit;
	}

	check(InValue);

	const IPyWrapperStructAllocationPolicy* AllocPolicy = GetPyWrapperStructAllocationPolicy(InStruct);
	if (!AllocPolicy)
	{
		PyUtil::SetPythonError(PyExc_Exception, InSelf, TEXT("AllocPolicy is null"));
		return -1;
	}

	void* StructInstanceToUse = nullptr;
	switch (InConversionMethod)
	{
	case EPyConversionMethod::Copy:
	case EPyConversionMethod::Steal:
		{
			StructInstanceToUse = AllocPolicy->AllocateStruct(InSelf, InStruct);
			InStruct->InitializeStruct(StructInstanceToUse);
			InStruct->CopyScriptStruct(StructInstanceToUse, InValue);
		}
		break;

	case EPyConversionMethod::Reference:
		{
			StructInstanceToUse = InValue;
		}
		break;

	default:
		checkf(false, TEXT("Unknown EPyConversionMethod"));
		break;
	}

	check(StructInstanceToUse);

	InSelf->OwnerContext = InOwnerContext;
	InSelf->ScriptStruct = InStruct;
	InSelf->StructInstance = StructInstanceToUse;

	FPyWrapperStructFactory::Get().MapInstance(InSelf->StructInstance, InSelf);
	return 0;
}

void FPyWrapperStruct::Deinit(FPyWrapperStruct* InSelf)
{
	if (InSelf->StructInstance)
	{
		FPyWrapperStructFactory::Get().UnmapInstance(InSelf->StructInstance, Py_TYPE(InSelf));
	}

	if (InSelf->OwnerContext.HasOwner())
	{
		InSelf->OwnerContext.Reset();
	}
	else if (InSelf->StructInstance)
	{
		if (InSelf->ScriptStruct)
		{
			InSelf->ScriptStruct->DestroyStruct(InSelf->StructInstance);
		}

		const IPyWrapperStructAllocationPolicy* AllocPolicy = GetPyWrapperStructAllocationPolicy(InSelf->ScriptStruct);
		if (AllocPolicy)
		{
			AllocPolicy->FreeStruct(InSelf, InSelf->StructInstance);
		}
	}
	InSelf->ScriptStruct = nullptr;
	InSelf->StructInstance = nullptr;
}

bool FPyWrapperStruct::ValidateInternalState(FPyWrapperStruct* InSelf)
{
	if (!InSelf->ScriptStruct)
	{
		PyUtil::SetPythonError(PyExc_Exception, Py_TYPE(InSelf), TEXT("Internal Error - ScriptStruct is null!"));
		return false;
	}

	if (!InSelf->StructInstance)
	{
		PyUtil::SetPythonError(PyExc_Exception, Py_TYPE(InSelf), TEXT("Internal Error - StructInstance is null!"));
		return false;
	}

	return true;
}

FPyWrapperStruct* FPyWrapperStruct::CastPyObject(PyObject* InPyObject, FPyConversionResult* OutCastResult)
{
	SetOptionalPyConversionResult(FPyConversionResult::Failure(), OutCastResult);

	if (PyObject_IsInstance(InPyObject, (PyObject*)&PyWrapperStructType) == 1)
	{
		SetOptionalPyConversionResult(FPyConversionResult::Success(), OutCastResult);

		Py_INCREF(InPyObject);
		return (FPyWrapperStruct*)InPyObject;
	}

	return nullptr;
}

FPyWrapperStruct* FPyWrapperStruct::CastPyObject(PyObject* InPyObject, PyTypeObject* InType, FPyConversionResult* OutCastResult)
{
	SetOptionalPyConversionResult(FPyConversionResult::Failure(), OutCastResult);

	if (PyObject_IsInstance(InPyObject, (PyObject*)InType) == 1 && (InType == &PyWrapperStructType || PyObject_IsInstance(InPyObject, (PyObject*)&PyWrapperStructType) == 1))
	{
		SetOptionalPyConversionResult(Py_TYPE(InPyObject) == InType ? FPyConversionResult::Success() : FPyConversionResult::SuccessWithCoercion(), OutCastResult);

		Py_INCREF(InPyObject);
		return (FPyWrapperStruct*)InPyObject;
	}

	if (PyUtil::HasLength(InPyObject) && !PyUtil::IsMappingType(InPyObject))
	{
		FPyWrapperStructPtr NewStruct = FPyWrapperStructPtr::StealReference(FPyWrapperStruct::New(InType));
		if (FPyWrapperStruct::Init(NewStruct) != 0)
		{
			return nullptr;
		}

		FPyWrapperStructMetaData* StructMetaData = FPyWrapperStructMetaData::GetMetaData(NewStruct);
		if (!StructMetaData)
		{
			return nullptr;
		}

		// Don't allow conversion from sequences with more items than we have InitParams
		const int32 SequenceLen = PyObject_Length(InPyObject);
		if (SequenceLen > StructMetaData->InitParams.Num())
		{
			PyUtil::SetPythonError(PyExc_Exception, NewStruct.Get(), *FString::Printf(TEXT("Struct has %d initialization parameters, but the given sequence had %d elements"), StructMetaData->InitParams.Num(), SequenceLen));
			return nullptr;
		}

		// Attempt to convert each entry in the sequence to the corresponding struct entry
		FPyObjectPtr PyObjIter = FPyObjectPtr::StealReference(PyObject_GetIter(InPyObject));
		if (PyObjIter)
		{
			for (const PyGenUtil::FGeneratedWrappedMethodParameter& InitParam : StructMetaData->InitParams)
			{
				FPyObjectPtr SequenceItem = FPyObjectPtr::StealReference(PyIter_Next(PyObjIter));
				if (!SequenceItem)
				{
					if (PyErr_Occurred())
					{
						return nullptr;
					}
					break;
				}

				const int Result = PyUtil::SetPropertyValue(NewStruct->ScriptStruct, NewStruct->StructInstance, SequenceItem, InitParam.ParamProp, InitParam.ParamName.GetData(), FPyWrapperOwnerContext(), 0, false, *PyUtil::GetErrorContext(NewStruct.Get()));
				if (Result != 0)
				{
					return nullptr;
				}
			}
		}

		SetOptionalPyConversionResult(FPyConversionResult::SuccessWithCoercion(), OutCastResult);
		return NewStruct.Release();
	}

	if (PyUtil::IsMappingType(InPyObject))
	{
		FPyWrapperStructPtr NewStruct = FPyWrapperStructPtr::StealReference(FPyWrapperStruct::New(InType));
		if (FPyWrapperStruct::Init(NewStruct) != 0)
		{
			return nullptr;
		}

		FPyWrapperStructMetaData* StructMetaData = FPyWrapperStructMetaData::GetMetaData(NewStruct);
		if (!StructMetaData)
		{
			return nullptr;
		}

		// Don't allow conversion from dicts with more items than we have InitParams
		const int32 DictLen = PyObject_Length(InPyObject);
		if (DictLen > StructMetaData->InitParams.Num())
		{
			PyUtil::SetPythonError(PyExc_Exception, NewStruct.Get(), *FString::Printf(TEXT("Struct has %d initialization parameters, but the given dict had %d elements"), StructMetaData->InitParams.Num(), DictLen));
			return nullptr;
		}

		// Attempt to convert each matching entry in the dict to the corresponding struct entry
		for (const PyGenUtil::FGeneratedWrappedMethodParameter& InitParam : StructMetaData->InitParams)
		{
			PyObject* MappingItem = PyMapping_GetItemString(InPyObject, (char*)InitParam.ParamName.GetData());
			if (MappingItem)
			{
				const int Result = PyUtil::SetPropertyValue(NewStruct->ScriptStruct, NewStruct->StructInstance, MappingItem, InitParam.ParamProp, InitParam.ParamName.GetData(), FPyWrapperOwnerContext(), 0, false, *PyUtil::GetErrorContext(NewStruct.Get()));
				if (Result != 0)
				{
					return nullptr;
				}
			}
			else
			{
				// Clear the look-up error
				PyErr_Clear();
			}
		}

		SetOptionalPyConversionResult(FPyConversionResult::SuccessWithCoercion(), OutCastResult);
		return NewStruct.Release();
	}

	return nullptr;
}

int FPyWrapperStruct::MakeStruct(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds)
{
	if (!ValidateInternalState(InSelf))
	{
		return -1;
	}

	FPyWrapperStructMetaData* StructMetaData = FPyWrapperStructMetaData::GetMetaData(InSelf);
	if (!StructMetaData)
	{
		return -1;
	}

	// We can early out if we have no data to apply
	if (PyTuple_Size(InArgs) == 0 && (!InKwds || PyDict_Size(InKwds) == 0))
	{
		return 0;
	}

	// If this struct has a custom make function, use that rather than use the generic version
	if (StructMetaData->MakeFunc.Func)
	{
		return CallMakeFunction_Impl(InSelf, InArgs, InKwds, StructMetaData->MakeFunc);
	}

	// Generic implementation just tries to assign each property
	TArray<PyObject*> Params;
	if (!PyGenUtil::ParseMethodParameters(InArgs, InKwds, StructMetaData->InitParams, "call", Params))
	{
		return -1;
	}

	for (int32 ParamIndex = 0; ParamIndex < Params.Num(); ++ParamIndex)
	{
		PyObject* PyValue = Params[ParamIndex];
		if (PyValue)
		{
			const PyGenUtil::FGeneratedWrappedMethodParameter& InitParam = StructMetaData->InitParams[ParamIndex];
			if (!PyConversion::NativizeProperty_InContainer(PyValue, InitParam.ParamProp, InSelf->StructInstance, 0))
			{
				PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert type '%s' to property '%s' (%s) for attribute '%s' on '%s'"), *PyUtil::GetFriendlyTypename(PyValue), *InitParam.ParamProp->GetName(), *InitParam.ParamProp->GetClass()->GetName(), UTF8_TO_TCHAR(InitParam.ParamName.GetData()), *InSelf->ScriptStruct->GetName()));
				return -1;
			}
		}
	}

	return 0;
}

PyObject* FPyWrapperStruct::BreakStruct(FPyWrapperStruct* InSelf)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	FPyWrapperStructMetaData* StructMetaData = FPyWrapperStructMetaData::GetMetaData(InSelf);
	if (!StructMetaData)
	{
		return nullptr;
	}

	// If this struct has a custom break function, use that rather than use the generic version
	if (StructMetaData->BreakFunc.Func)
	{
		return CallBreakFunction_Impl(InSelf, StructMetaData->BreakFunc);
	}

	// Generic implementation just creates a tuple from each property
	FPyObjectPtr PyPropTuple = FPyObjectPtr::StealReference(PyTuple_New(StructMetaData->InitParams.Num()));
	for (int32 ParamIndex = 0; ParamIndex < StructMetaData->InitParams.Num(); ++ParamIndex)
	{
		const PyGenUtil::FGeneratedWrappedMethodParameter& InitParam = StructMetaData->InitParams[ParamIndex];

		PyObject* PyValue = nullptr;
		if (!PyConversion::PythonizeProperty_InContainer(InitParam.ParamProp, InSelf->StructInstance, 0, PyValue))
		{
			PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert property '%s' (%s) for attribute '%s' on '%s'"), *InitParam.ParamProp->GetName(), *InitParam.ParamProp->GetClass()->GetName(), UTF8_TO_TCHAR(InitParam.ParamName.GetData()), *InSelf->ScriptStruct->GetName()));
			return nullptr;
		}
		PyTuple_SetItem(PyPropTuple, ParamIndex, PyValue); // SetItem steals the reference
	}

	return PyPropTuple.Release();
}

PyObject* FPyWrapperStruct::GetPropertyValue(FPyWrapperStruct* InSelf, const PyGenUtil::FGeneratedWrappedProperty& InPropDef, const char* InPythonAttrName)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	return PyGenUtil::GetPropertyValue(InSelf->ScriptStruct, InSelf->StructInstance, InPropDef, InPythonAttrName, (PyObject*)InSelf, *PyUtil::GetErrorContext(InSelf));
}

int FPyWrapperStruct::SetPropertyValue(FPyWrapperStruct* InSelf, PyObject* InValue, const PyGenUtil::FGeneratedWrappedProperty& InPropDef, const char* InPythonAttrName, const bool InNotifyChange, const uint64 InReadOnlyFlags)
{
	if (!ValidateInternalState(InSelf))
	{
		return -1;
	}

	// Structs are not a template by default (for standalone structs)
	bool OwnerIsTemplate = false;
	if (const UObject* OwnerObject = PyUtil::GetOwnerObject((PyObject*)InSelf))
	{
		OwnerIsTemplate = OwnerObject->IsTemplate();
	}

	const FPyWrapperOwnerContext ChangeOwner = InNotifyChange ? FPyWrapperOwnerContext((PyObject*)InSelf, InPropDef.Prop) : FPyWrapperOwnerContext();
	return PyGenUtil::SetPropertyValue(InSelf->ScriptStruct, InSelf->StructInstance, InValue, InPropDef, InPythonAttrName, ChangeOwner, InReadOnlyFlags, OwnerIsTemplate, *PyUtil::GetErrorContext(InSelf));
}

int FPyWrapperStruct::CallMakeFunction_Impl(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef)
{
	TArray<PyObject*> Params;
	if (!PyGenUtil::ParseMethodParameters(InArgs, InKwds, InFuncDef.InputParams, "call", Params))
	{
		return -1;
	}

	if (ensureAlways(InFuncDef.Func))
	{
		UClass* Class = InFuncDef.Func->GetOwnerClass();
		UObject* Obj = Class->GetDefaultObject();

		FStructOnScope FuncParams(InFuncDef.Func);
		PyGenUtil::ApplyParamDefaults(FuncParams.GetStructMemory(), InFuncDef.InputParams);
		for (int32 ParamIndex = 0; ParamIndex < Params.Num(); ++ParamIndex)
		{
			const PyGenUtil::FGeneratedWrappedMethodParameter& ParamDef = InFuncDef.InputParams[ParamIndex];

			PyObject* PyValue = Params[ParamIndex];
			if (PyValue)
			{
				if (!PyConversion::NativizeProperty_InContainer(PyValue, ParamDef.ParamProp, FuncParams.GetStructMemory(), 0))
				{
					PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert parameter '%s' when calling function '%s.%s' on '%s'"), UTF8_TO_TCHAR(ParamDef.ParamName.GetData()), *Class->GetName(), *InFuncDef.Func->GetName(), *Obj->GetName()));
					return -1;
				}
			}
		}
		if (!PyUtil::InvokeFunctionCall(Obj, InFuncDef.Func, FuncParams.GetStructMemory(), *PyUtil::GetErrorContext(InSelf)))
		{
			return -1;
		}
		if (ensureAlways(InFuncDef.OutputParams.Num() == 1 && Cast<UStructProperty>(InFuncDef.OutputParams[0].ParamProp) && CastChecked<UStructProperty>(InFuncDef.OutputParams[0].ParamProp)->Struct->IsChildOf(InSelf->ScriptStruct)))
		{
			// Copy the result back onto ourself
			const PyGenUtil::FGeneratedWrappedMethodParameter& ReturnParam = InFuncDef.OutputParams[0];
			const void* ReturnArgInstance = ReturnParam.ParamProp->ContainerPtrToValuePtr<void>(FuncParams.GetStructMemory());
			InSelf->ScriptStruct->CopyScriptStruct(InSelf->StructInstance, ReturnArgInstance);
		}
	}

	return 0;
}

PyObject* FPyWrapperStruct::CallBreakFunction_Impl(FPyWrapperStruct* InSelf, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef)
{
	if (ensureAlways(InFuncDef.Func))
	{
		UClass* Class = InFuncDef.Func->GetOwnerClass();
		UObject* Obj = Class->GetDefaultObject();

		FStructOnScope FuncParams(InFuncDef.Func);
		if (ensureAlways(InFuncDef.InputParams.Num() == 1 && Cast<UStructProperty>(InFuncDef.InputParams[0].ParamProp) && InSelf->ScriptStruct->IsChildOf(CastChecked<UStructProperty>(InFuncDef.InputParams[0].ParamProp)->Struct)))
		{
			// Copy us as the 'self' argument
			const PyGenUtil::FGeneratedWrappedMethodParameter& SelfParam = InFuncDef.InputParams[0];
			void* SelfArgInstance = SelfParam.ParamProp->ContainerPtrToValuePtr<void>(FuncParams.GetStructMemory());
			CastChecked<UStructProperty>(SelfParam.ParamProp)->Struct->CopyScriptStruct(SelfArgInstance, InSelf->StructInstance);
		}
		if (!PyUtil::InvokeFunctionCall(Obj, InFuncDef.Func, FuncParams.GetStructMemory(), *PyUtil::GetErrorContext(InSelf)))
		{
			return nullptr;
		}
		FPyObjectPtr PyPropTuple = FPyObjectPtr::StealReference(PyTuple_New(InFuncDef.OutputParams.Num()));
		for (int32 ParamIndex = 0; ParamIndex < InFuncDef.OutputParams.Num(); ++ParamIndex)
		{
			const PyGenUtil::FGeneratedWrappedMethodParameter& ParamDef = InFuncDef.OutputParams[ParamIndex];

			PyObject* PyValue = nullptr;
			if (!PyConversion::PythonizeProperty_InContainer(ParamDef.ParamProp, FuncParams.GetStructMemory(), 0, PyValue, EPyConversionMethod::Steal))
			{
				PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert return property '%s' when calling function '%s.%s' on '%s'"), UTF8_TO_TCHAR(ParamDef.ParamName.GetData()), *Class->GetName(), *InFuncDef.Func->GetName(), *Obj->GetName()));
				return nullptr;
			}
			PyTuple_SetItem(PyPropTuple, ParamIndex, PyValue); // SetItem steals the reference
		}
		return PyPropTuple.Release();
	}

	Py_RETURN_NONE;
}

PyObject* FPyWrapperStruct::CallDynamicFunction_Impl(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef, const PyGenUtil::FGeneratedWrappedMethodParameter& InSelfParam, const PyGenUtil::FGeneratedWrappedMethodParameter& InSelfReturn, const char* InPythonFuncName)
{
	TArray<PyObject*> Params;
	if ((InArgs || InKwds) && !PyGenUtil::ParseMethodParameters(InArgs, InKwds, InFuncDef.InputParams, InPythonFuncName, Params))
	{
		return nullptr;
	}

	if (ensureAlways(InFuncDef.Func))
	{
		UClass* Class = InFuncDef.Func->GetOwnerClass();
		UObject* Obj = Class->GetDefaultObject();

		// Deprecated functions emit a warning
		if (InFuncDef.DeprecationMessage.IsSet())
		{
			if (PyUtil::SetPythonWarning(PyExc_DeprecationWarning, InSelf, *FString::Printf(TEXT("Function '%s' on '%s' is deprecated: %s"), UTF8_TO_TCHAR(InPythonFuncName), *Class->GetName(), *InFuncDef.DeprecationMessage.GetValue())) == -1)
			{
				// -1 from SetPythonWarning means the warning should be an exception
				return nullptr;
			}
		}

		FStructOnScope FuncParams(InFuncDef.Func);
		PyGenUtil::ApplyParamDefaults(FuncParams.GetStructMemory(), InFuncDef.InputParams);
		if (ensureAlways(Cast<UStructProperty>(InSelfParam.ParamProp) && InSelf->ScriptStruct->IsChildOf(CastChecked<UStructProperty>(InSelfParam.ParamProp)->Struct)))
		{
			void* SelfArgInstance = InSelfParam.ParamProp->ContainerPtrToValuePtr<void>(FuncParams.GetStructMemory());
			CastChecked<UStructProperty>(InSelfParam.ParamProp)->Struct->CopyScriptStruct(SelfArgInstance, InSelf->StructInstance);
		}
		for (int32 ParamIndex = 0; ParamIndex < Params.Num(); ++ParamIndex)
		{
			const PyGenUtil::FGeneratedWrappedMethodParameter& ParamDef = InFuncDef.InputParams[ParamIndex];

			PyObject* PyValue = Params[ParamIndex];
			if (PyValue)
			{
				if (!PyConversion::NativizeProperty_InContainer(PyValue, ParamDef.ParamProp, FuncParams.GetStructMemory(), 0))
				{
					PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert parameter '%s' when calling function '%s.%s' on '%s'"), UTF8_TO_TCHAR(ParamDef.ParamName.GetData()), *Class->GetName(), *InFuncDef.Func->GetName(), *Obj->GetName()));
					return nullptr;
				}
			}
		}
		const FString ErrorCtxt = PyUtil::GetErrorContext(InSelf);
		if (!PyUtil::InvokeFunctionCall(Obj, InFuncDef.Func, FuncParams.GetStructMemory(), *ErrorCtxt))
		{
			return nullptr;
		}
		if (InSelfReturn.ParamProp && ensureAlways(Cast<UStructProperty>(InSelfReturn.ParamProp) && CastChecked<UStructProperty>(InSelfReturn.ParamProp)->Struct->IsChildOf(InSelf->ScriptStruct)))
		{
			// Copy the 'self' return value back onto ourself
			const void* SelfReturnInstance = InSelfReturn.ParamProp->ContainerPtrToValuePtr<void>(FuncParams.GetStructMemory());
			InSelf->ScriptStruct->CopyScriptStruct(InSelf->StructInstance, SelfReturnInstance);
		}
		return PyGenUtil::PackReturnValues(FuncParams.GetStructMemory(), InFuncDef.OutputParams, *ErrorCtxt, *FString::Printf(TEXT("function '%s.%s' on '%s'"), *Class->GetName(), *InFuncDef.Func->GetName(), *Obj->GetName()));
	}

	Py_RETURN_NONE;
}

PyObject* FPyWrapperStruct::CallDynamicMethodNoArgs_Impl(FPyWrapperStruct* InSelf, void* InClosure)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	const PyGenUtil::FGeneratedWrappedDynamicMethod* Closure = (PyGenUtil::FGeneratedWrappedDynamicMethod*)InClosure;
	return CallDynamicFunction_Impl(InSelf, nullptr, nullptr, Closure->MethodFunc, Closure->SelfParam, Closure->SelfReturn, Closure->MethodName.GetData());
}

PyObject* FPyWrapperStruct::CallDynamicMethodWithArgs_Impl(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds, void* InClosure)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	const PyGenUtil::FGeneratedWrappedDynamicMethod* Closure = (PyGenUtil::FGeneratedWrappedDynamicMethod*)InClosure;
	return CallDynamicFunction_Impl(InSelf, InArgs, InKwds, Closure->MethodFunc, Closure->SelfParam, Closure->SelfReturn, Closure->MethodName.GetData());
}

PyObject* FPyWrapperStruct::CallOperatorFunction_Impl(FPyWrapperStruct* InSelf, PyObject* InRHS, const PyGenUtil::FGeneratedWrappedOperatorFunction& InOpFunc, const TOptional<EPyConversionResultState> InRequiredConversionResult, FPyConversionResult* OutRHSConversionResult)
{
	SetOptionalPyConversionResult(FPyConversionResult::Failure(), OutRHSConversionResult);

	if (ensureAlways(InOpFunc.Func))
	{
		UClass* Class = InOpFunc.Func->GetOwnerClass();
		UObject* Obj = Class->GetDefaultObject();

		// Build the input arguments (failures here aren't fatal as we may have multiple functions to evaluate on the stack, only one of which may accept the RHS parameter)
		FStructOnScope FuncParams(InOpFunc.Func);
		PyGenUtil::ApplyParamDefaults(FuncParams.GetStructMemory(), InOpFunc.AdditionalParams);
		if (InOpFunc.OtherParam.ParamProp)
		{
			const FPyConversionResult RHSResult = PyConversion::NativizeProperty_InContainer(InRHS, InOpFunc.OtherParam.ParamProp, FuncParams.GetStructMemory(), 0, FPyWrapperOwnerContext(), PyConversion::ESetErrorState::No);
			SetOptionalPyConversionResult(RHSResult, OutRHSConversionResult);

			if (!RHSResult)
			{
				return nullptr;
			}

			if (InRequiredConversionResult.IsSet() && RHSResult.GetState() != InRequiredConversionResult.GetValue())
			{
				return nullptr;
			}
		}
		if (ensureAlways(Cast<UStructProperty>(InOpFunc.SelfParam.ParamProp) && InSelf->ScriptStruct->IsChildOf(CastChecked<UStructProperty>(InOpFunc.SelfParam.ParamProp)->Struct)))
		{
			void* StructArgInstance = InOpFunc.SelfParam.ParamProp->ContainerPtrToValuePtr<void>(FuncParams.GetStructMemory());
			CastChecked<UStructProperty>(InOpFunc.SelfParam.ParamProp)->Struct->CopyScriptStruct(StructArgInstance, InSelf->StructInstance);
		}
		if (!PyUtil::InvokeFunctionCall(Obj, InOpFunc.Func, FuncParams.GetStructMemory(), *PyUtil::GetErrorContext(InSelf)))
		{
			return nullptr;
		}

		PyObject* ReturnPyObj = nullptr;
		if (InOpFunc.SelfReturn.ParamProp)
		{
			if (ensureAlways(Cast<UStructProperty>(InOpFunc.SelfReturn.ParamProp) && CastChecked<UStructProperty>(InOpFunc.SelfReturn.ParamProp)->Struct->IsChildOf(InSelf->ScriptStruct)))
			{
				// Copy the 'self' return value back onto ourself
				const void* SelfReturnInstance = InOpFunc.SelfReturn.ParamProp->ContainerPtrToValuePtr<void>(FuncParams.GetStructMemory());
				InSelf->ScriptStruct->CopyScriptStruct(InSelf->StructInstance, SelfReturnInstance);
			}

			Py_INCREF(InSelf);
			ReturnPyObj = (PyObject*)InSelf;
		}
		else if (InOpFunc.ReturnParam.ParamProp)
		{
			if (!PyConversion::PythonizeProperty_InContainer(InOpFunc.ReturnParam.ParamProp, FuncParams.GetStructMemory(), 0, ReturnPyObj, EPyConversionMethod::Steal))
			{
				PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert return property '%s' (%s) when calling function '%s' on '%s'"), *InOpFunc.ReturnParam.ParamProp->GetName(), *InOpFunc.ReturnParam.ParamProp->GetClass()->GetName(), *InOpFunc.Func->GetName(), *Obj->GetName()));
				return nullptr;
			}
		}
		else
		{
			Py_INCREF(Py_None);
			ReturnPyObj = Py_None;
		}

		return ReturnPyObj;
	}

	return nullptr;
}

PyObject* FPyWrapperStruct::CallOperator_Impl(FPyWrapperStruct* InSelf, PyObject* InRHS, const PyGenUtil::EGeneratedWrappedOperatorType InOpType)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	// Walk up the inheritance chain to find the correct op functions to use
	// We take the first one with any functions set, so that overrides on a derived type hide those from the base type
	TArray<PyGenUtil::FGeneratedWrappedOperatorFunction>* OpFuncsPtr = nullptr;
	{
		PyTypeObject* PyType = Py_TYPE(InSelf);
		do
		{
			PyTypeObject* NextPyType = nullptr;
			if (FPyWrapperStructMetaData* PyWrapperMetaData = FPyWrapperStructMetaData::GetMetaData(PyType))
			{
				if (PyWrapperMetaData->OpStacks[(int32)InOpType].Funcs.Num() > 0)
				{
					OpFuncsPtr = &PyWrapperMetaData->OpStacks[(int32)InOpType].Funcs;
					break;
				}

				if (const UScriptStruct* SuperStruct = PyWrapperMetaData->Struct ? Cast<UScriptStruct>(PyWrapperMetaData->Struct->GetSuperStruct()) : nullptr)
				{
					NextPyType = FPyWrapperTypeRegistry::Get().GetWrappedStructType(SuperStruct);
				}
			}
			PyType = NextPyType;
		}
		while (PyType);
	}

	if (OpFuncsPtr)
	{
		// We process the operator stack in two passes:
		//	- The first pass looks for a signature that exactly matches the given argument
		//	- The second pass allows type coercion to occur when calling the signature
		// We use the first pass to find a function that may be called for the second pass
		const PyGenUtil::FGeneratedWrappedOperatorFunction* CoercedOpFunc = nullptr;
		for (const PyGenUtil::FGeneratedWrappedOperatorFunction& OpFunc : *OpFuncsPtr)
		{
			FPyConversionResult RHSConversionResult = FPyConversionResult::Failure();
			PyObject* PyResult = CallOperatorFunction_Impl(InSelf, InRHS, OpFunc, EPyConversionResultState::Success, &RHSConversionResult);
			if (PyResult)
			{
				return PyResult;
			}
			else if (!CoercedOpFunc && RHSConversionResult.GetState() == EPyConversionResultState::SuccessWithCoercion)
			{
				CoercedOpFunc = &OpFunc;
			}
		}
		if (CoercedOpFunc)
		{
			PyObject* PyResult = CallOperatorFunction_Impl(InSelf, InRHS, *CoercedOpFunc);
			if (PyResult)
			{
				return PyResult;
			}
		}
	}

	Py_INCREF(Py_NotImplemented);
	return Py_NotImplemented;
}

PyObject* FPyWrapperStruct::Getter_Impl(FPyWrapperStruct* InSelf, void* InClosure)
{
	const PyGenUtil::FGeneratedWrappedGetSet* Closure = (PyGenUtil::FGeneratedWrappedGetSet*)InClosure;
	return GetPropertyValue(InSelf, Closure->Prop, Closure->GetSetName.GetData());
}

int FPyWrapperStruct::Setter_Impl(FPyWrapperStruct* InSelf, PyObject* InValue, void* InClosure)
{
	const PyGenUtil::FGeneratedWrappedGetSet* Closure = (PyGenUtil::FGeneratedWrappedGetSet*)InClosure;
	return SetPropertyValue(InSelf, InValue, Closure->Prop, Closure->GetSetName.GetData());
}

PyTypeObject InitializePyWrapperStructType()
{
	struct FFuncs
	{
		static PyObject* New(PyTypeObject* InType, PyObject* InArgs, PyObject* InKwds)
		{
			return (PyObject*)FPyWrapperStruct::New(InType);
		}

		static void Dealloc(FPyWrapperStruct* InSelf)
		{
			FPyWrapperStruct::Free(InSelf);
		}

		static int Init(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds)
		{
			return FPyWrapperStruct::Init(InSelf);
		}

		static PyObject* Str(FPyWrapperStruct* InSelf)
		{
			if (!FPyWrapperStruct::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			const FString ExportedStruct = PyUtil::GetFriendlyStructValue(InSelf->ScriptStruct, InSelf->StructInstance, PPF_IncludeTransient);
			return PyUnicode_FromFormat("<Struct '%s' (%p) %s>", TCHAR_TO_UTF8(*InSelf->ScriptStruct->GetName()), InSelf->StructInstance, TCHAR_TO_UTF8(*ExportedStruct));
		}

		static PyObject* RichCmp(FPyWrapperStruct* InSelf, PyObject* InOther, int InOp)
		{
			if (!FPyWrapperStruct::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			auto PythonCmpOpToWrapperOp = [InOp]() -> PyGenUtil::EGeneratedWrappedOperatorType
			{
				switch (InOp)
				{
				case Py_EQ:
					return PyGenUtil::EGeneratedWrappedOperatorType::Equal;
				case Py_NE:
					return PyGenUtil::EGeneratedWrappedOperatorType::NotEqual;
				case Py_LT:
					return PyGenUtil::EGeneratedWrappedOperatorType::Less;
				case Py_LE:
					return PyGenUtil::EGeneratedWrappedOperatorType::LessEqual;
				case Py_GT:
					return PyGenUtil::EGeneratedWrappedOperatorType::Greater;
				case Py_GE:
					return PyGenUtil::EGeneratedWrappedOperatorType::GreaterEqual;
				default:
					checkf(false, TEXT("Unknown Python comparison type!"));
					break;
				}
				return PyGenUtil::EGeneratedWrappedOperatorType::Equal;
			};

			return FPyWrapperStruct::CallOperator_Impl(InSelf, InOther, PythonCmpOpToWrapperOp());
		}

		static PyUtil::FPyHashType Hash(FPyWrapperStruct* InSelf)
		{
			if (!FPyWrapperStruct::ValidateInternalState(InSelf))
			{
				return -1;
			}

			// UserDefinedStruct overrides GetStructTypeHash to work without valid CppStructOps
			if (InSelf->ScriptStruct->IsA<UUserDefinedStruct>() || (InSelf->ScriptStruct->GetCppStructOps() && InSelf->ScriptStruct->GetCppStructOps()->HasGetTypeHash()))
			{
				const PyUtil::FPyHashType PyHash = (PyUtil::FPyHashType)InSelf->ScriptStruct->GetStructTypeHash(InSelf->StructInstance);
				return PyHash != -1 ? PyHash : 0;
			}

			PyUtil::SetPythonError(PyExc_Exception, InSelf, TEXT("Type cannot be hashed"));
			return -1;
		}
	};

#define DEFINE_INQUIRY_OPERATOR_FUNC(OP, NOT_IMPLEMENTED_VALUE)																	\
	static int OP(FPyWrapperStruct* InLHS)																						\
	{																															\
		PyObject* PyResult = FPyWrapperStruct::CallOperator_Impl(InLHS, nullptr, PyGenUtil::EGeneratedWrappedOperatorType::OP);	\
		const int Result = PyObjectResultToInt(PyResult, (NOT_IMPLEMENTED_VALUE));												\
		Py_XDECREF(PyResult);																									\
		return Result;																											\
	}
#define DEFINE_UNARY_OPERATOR_FUNC(OP)																							\
	static PyObject* OP(FPyWrapperStruct* InLHS)																				\
	{																															\
		return FPyWrapperStruct::CallOperator_Impl(InLHS, nullptr, PyGenUtil::EGeneratedWrappedOperatorType::OP);				\
	}
#define DEFINE_BINARY_OPERATOR_FUNC(OP)																							\
	static PyObject* OP(FPyWrapperStruct* InLHS, PyObject* InRHS)																\
	{																															\
		return FPyWrapperStruct::CallOperator_Impl(InLHS, InRHS, PyGenUtil::EGeneratedWrappedOperatorType::OP);					\
	}
	struct FNumberFuncs
	{
		static int PyObjectResultToInt(PyObject* PyResult, int NotImplementedValue)
		{
			int Result = -1;
			if (PyResult)
			{
				if (PyResult == Py_NotImplemented)
				{
					Result = NotImplementedValue;
				}
				else if (PyBool_Check(PyResult))
				{
					Result = (PyResult == Py_True) ? 1 : 0;
				}
				else
				{
					PyConversion::Nativize(PyResult, Result);
				}
			}
			return Result;
		}

		DEFINE_INQUIRY_OPERATOR_FUNC(Bool, 1)
		DEFINE_BINARY_OPERATOR_FUNC(Add)
		DEFINE_BINARY_OPERATOR_FUNC(InlineAdd)
		DEFINE_BINARY_OPERATOR_FUNC(Subtract)
		DEFINE_BINARY_OPERATOR_FUNC(InlineSubtract)
		DEFINE_BINARY_OPERATOR_FUNC(Multiply)
		DEFINE_BINARY_OPERATOR_FUNC(InlineMultiply)
		DEFINE_BINARY_OPERATOR_FUNC(Divide)
		DEFINE_BINARY_OPERATOR_FUNC(InlineDivide)
		DEFINE_BINARY_OPERATOR_FUNC(Modulus)
		DEFINE_BINARY_OPERATOR_FUNC(InlineModulus)
		DEFINE_BINARY_OPERATOR_FUNC(And)
		DEFINE_BINARY_OPERATOR_FUNC(InlineAnd)
		DEFINE_BINARY_OPERATOR_FUNC(Or)
		DEFINE_BINARY_OPERATOR_FUNC(InlineOr)
		DEFINE_BINARY_OPERATOR_FUNC(Xor)
		DEFINE_BINARY_OPERATOR_FUNC(InlineXor)
		DEFINE_BINARY_OPERATOR_FUNC(RightShift)
		DEFINE_BINARY_OPERATOR_FUNC(InlineRightShift)
		DEFINE_BINARY_OPERATOR_FUNC(LeftShift)
		DEFINE_BINARY_OPERATOR_FUNC(InlineLeftShift)
	};
#undef DEFINE_INQUIRY_OPERATOR_FUNC
#undef DEFINE_UNARY_OPERATOR_FUNC
#undef DEFINE_BINARY_OPERATOR_FUNC

	struct FMethods
	{
		static PyObject* PostInit(FPyWrapperStruct* InSelf)
		{
			Py_RETURN_NONE;
		}

		static PyObject* Cast(PyTypeObject* InType, PyObject* InArgs)
		{
			PyObject* PyObj = nullptr;
			if (PyArg_ParseTuple(InArgs, "O:cast", &PyObj))
			{
				PyObject* PyCastResult = (PyObject*)FPyWrapperStruct::CastPyObject(PyObj, InType);
				if (!PyCastResult)
				{
					PyUtil::SetPythonError(PyExc_TypeError, InType, *FString::Printf(TEXT("Cannot cast type '%s' to '%s'"), *PyUtil::GetFriendlyTypename(PyObj), *PyUtil::GetFriendlyTypename(InType)));
				}
				return PyCastResult;
			}

			return nullptr;
		}

		static PyObject* StaticStruct(PyTypeObject* InType)
		{
			UScriptStruct* Struct = FPyWrapperStructMetaData::GetStruct(InType);
			return PyConversion::Pythonize(Struct);
		}

		static PyObject* Copy(FPyWrapperStruct* InSelf)
		{
			if (!FPyWrapperStruct::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			return (PyObject*)FPyWrapperStructFactory::Get().CreateInstance(InSelf->ScriptStruct, InSelf->StructInstance, FPyWrapperOwnerContext(), EPyConversionMethod::Copy);
		}

		static PyObject* Assign(FPyWrapperStruct* InSelf, PyObject* InArgs)
		{
			if (!FPyWrapperStruct::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			PyObject* PyObj = nullptr;
			if (!PyArg_ParseTuple(InArgs, "O:assign", &PyObj))
			{
				return nullptr;
			}
			check(PyObj);

			FPyWrapperStructPtr PyStruct = FPyWrapperStructPtr::StealReference(FPyWrapperStruct::CastPyObject(PyObj, Py_TYPE(InSelf)));
			if (!PyStruct)
			{
				PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Cannot cast type '%s' to '%s'"), *PyUtil::GetFriendlyTypename(PyObj), *PyUtil::GetFriendlyTypename(InSelf)));
				return nullptr;
			}

			if (PyStruct && ensureAlways(PyStruct->ScriptStruct->IsChildOf(InSelf->ScriptStruct)))
			{
				InSelf->ScriptStruct->CopyScriptStruct(InSelf->StructInstance, PyStruct->StructInstance);
			}

			Py_RETURN_NONE;
		}

		static PyObject* ToTuple(FPyWrapperStruct* InSelf)
		{
			return FPyWrapperStruct::BreakStruct(InSelf);
		}

		static PyObject* GetEditorProperty(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds)
		{
			if (!FPyWrapperStruct::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			PyObject* PyNameObj = nullptr;

			static const char *ArgsKwdList[] = { "name", nullptr };
			if (!PyArg_ParseTupleAndKeywords(InArgs, InKwds, "O:get_editor_property", (char**)ArgsKwdList, &PyNameObj))
			{
				return nullptr;
			}

			FName Name;
			if (!PyConversion::Nativize(PyNameObj, Name))
			{
				PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert 'name' (%s) to 'Name'"), *PyUtil::GetFriendlyTypename(InSelf)));
				return nullptr;
			}

			const FName ResolvedName = FPyWrapperStructMetaData::ResolvePropertyName(InSelf, Name);
			const UProperty* ResolvedProp = InSelf->ScriptStruct->FindPropertyByName(ResolvedName);
			if (!ResolvedProp)
			{
				PyUtil::SetPythonError(PyExc_Exception, InSelf, *FString::Printf(TEXT("Failed to find property '%s' for attribute '%s' on '%s'"), *ResolvedName.ToString(), *Name.ToString(), *InSelf->ScriptStruct->GetName()));
				return nullptr;
			}
			
			TOptional<FString> PropDeprecationMessage;
			{
				FString PropDeprecationMessageStr;
				if (FPyWrapperStructMetaData::IsPropertyDeprecated(InSelf, Name, &PropDeprecationMessageStr))
				{
					PropDeprecationMessage = MoveTemp(PropDeprecationMessageStr);
				}
			}

			PyGenUtil::FGeneratedWrappedProperty WrappedPropDef;
			if (PropDeprecationMessage.IsSet())
			{
				WrappedPropDef.SetProperty(ResolvedProp, PyGenUtil::FGeneratedWrappedProperty::SPF_None);
				WrappedPropDef.DeprecationMessage = MoveTemp(PropDeprecationMessage);
			}
			else
			{
				WrappedPropDef.SetProperty(ResolvedProp);
			}

			return FPyWrapperStruct::GetPropertyValue(InSelf, WrappedPropDef, TCHAR_TO_UTF8(*Name.ToString()));
		}

		static PyObject* SetEditorProperty(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds)
		{
			if (!FPyWrapperStruct::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			PyObject* PyNameObj = nullptr;
			PyObject* PyValueObj = nullptr;

			static const char *ArgsKwdList[] = { "name", "value", nullptr };
			if (!PyArg_ParseTupleAndKeywords(InArgs, InKwds, "OO:set_editor_property", (char**)ArgsKwdList, &PyNameObj, &PyValueObj))
			{
				return nullptr;
			}

			FName Name;
			if (!PyConversion::Nativize(PyNameObj, Name))
			{
				PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert 'name' (%s) to 'Name'"), *PyUtil::GetFriendlyTypename(InSelf)));
				return nullptr;
			}

			const FName ResolvedName = FPyWrapperStructMetaData::ResolvePropertyName(InSelf, Name);
			const UProperty* ResolvedProp = InSelf->ScriptStruct->FindPropertyByName(ResolvedName);
			if (!ResolvedProp)
			{
				PyUtil::SetPythonError(PyExc_Exception, InSelf, *FString::Printf(TEXT("Failed to find property '%s' for attribute '%s' on '%s'"), *ResolvedName.ToString(), *Name.ToString(), *InSelf->ScriptStruct->GetName()));
				return nullptr;
			}

			TOptional<FString> PropDeprecationMessage;
			{
				FString PropDeprecationMessageStr;
				if (FPyWrapperStructMetaData::IsPropertyDeprecated(InSelf, Name, &PropDeprecationMessageStr))
				{
					PropDeprecationMessage = MoveTemp(PropDeprecationMessageStr);
				}
			}

			PyGenUtil::FGeneratedWrappedProperty WrappedPropDef;
			if (PropDeprecationMessage.IsSet())
			{
				WrappedPropDef.SetProperty(ResolvedProp, PyGenUtil::FGeneratedWrappedProperty::SPF_None);
				WrappedPropDef.DeprecationMessage = MoveTemp(PropDeprecationMessage);
			}
			else
			{
				WrappedPropDef.SetProperty(ResolvedProp);
			}

			const int Result = FPyWrapperStruct::SetPropertyValue(InSelf, PyValueObj, WrappedPropDef, TCHAR_TO_UTF8(*Name.ToString()), /*InNotifyChange*/true, CPF_EditConst);
			if (Result != 0)
			{
				return nullptr;
			}

			Py_RETURN_NONE;
		}
	};

	static PyMethodDef PyMethods[] = {
		{ PyGenUtil::PostInitFuncName, PyCFunctionCast(&FMethods::PostInit), METH_NOARGS, "x._post_init() -> None -- called during Unreal struct initialization (equivalent to PostInitProperties in C++)" },
		{ "cast", PyCFunctionCast(&FMethods::Cast), METH_VARARGS | METH_CLASS, "X.cast(object) -> struct -- cast the given object to this Unreal struct type" },
		{ "static_struct", PyCFunctionCast(&FMethods::StaticStruct), METH_NOARGS | METH_CLASS, "X.static_struct() -> Struct -- get the Unreal struct of this type" },
		{ "__copy__", PyCFunctionCast(&FMethods::Copy), METH_NOARGS, "x.__copy__() -> struct -- copy this Unreal struct" },
		{ "copy", PyCFunctionCast(&FMethods::Copy), METH_NOARGS, "x.copy() -> struct -- copy this Unreal struct" },
		{ "assign", PyCFunctionCast(&FMethods::Assign), METH_VARARGS, "x.assign(object) -> None -- assign the value of this Unreal struct to value of the given object" },
		{ "to_tuple", PyCFunctionCast(&FMethods::ToTuple), METH_NOARGS, "x.to_tuple() -> tuple -- break this Unreal struct into a tuple of its properties" },
		{ "get_editor_property", PyCFunctionCast(&FMethods::GetEditorProperty), METH_VARARGS | METH_KEYWORDS, "x.get_editor_property(name) -> object -- get the value of any property visible to the editor" },
		{ "set_editor_property", PyCFunctionCast(&FMethods::SetEditorProperty), METH_VARARGS | METH_KEYWORDS, "x.set_editor_property(name, value) -> None -- set the value of any property visible to the editor, ensuring that the pre/post change notifications are called" },
		{ nullptr, nullptr, 0, nullptr }
	};

	PyTypeObject PyType = {
		PyVarObject_HEAD_INIT(nullptr, 0)
		"StructBase", /* tp_name */
		sizeof(FPyWrapperStruct), /* tp_basicsize */
	};

	PyType.tp_base = &PyWrapperBaseType;
	PyType.tp_new = (newfunc)&FFuncs::New;
	PyType.tp_dealloc = (destructor)&FFuncs::Dealloc;
	PyType.tp_init = (initproc)&FFuncs::Init;
	PyType.tp_str = (reprfunc)&FFuncs::Str;
	PyType.tp_richcompare = (richcmpfunc)&FFuncs::RichCmp;
	PyType.tp_hash = (hashfunc)&FFuncs::Hash;

	PyType.tp_methods = PyMethods;

	PyType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
#if PY_MAJOR_VERSION < 3
	PyType.tp_flags |= Py_TPFLAGS_CHECKTYPES;
#endif	// PY_MAJOR_VERSION < 3
	PyType.tp_doc = "Type for all UE4 exposed struct instances";

	static PyNumberMethods PyNumber;
#if PY_MAJOR_VERSION >= 3
	PyNumber.nb_bool = (inquiry)&FNumberFuncs::Bool;
#else	// PY_MAJOR_VERSION >= 3
	PyNumber.nb_nonzero = (inquiry)&FNumberFuncs::Bool;
#endif	// PY_MAJOR_VERSION >= 3
	PyNumber.nb_add = (binaryfunc)&FNumberFuncs::Add;
	PyNumber.nb_inplace_add = (binaryfunc)&FNumberFuncs::InlineAdd;
	PyNumber.nb_subtract = (binaryfunc)&FNumberFuncs::Subtract;
	PyNumber.nb_inplace_subtract = (binaryfunc)&FNumberFuncs::InlineSubtract;
	PyNumber.nb_multiply = (binaryfunc)&FNumberFuncs::Multiply;
	PyNumber.nb_inplace_multiply = (binaryfunc)&FNumberFuncs::InlineMultiply;
#if PY_MAJOR_VERSION >= 3
	PyNumber.nb_true_divide = (binaryfunc)&FNumberFuncs::Divide;
	PyNumber.nb_inplace_true_divide = (binaryfunc)&FNumberFuncs::InlineDivide;
#else	// PY_MAJOR_VERSION >= 3
	PyNumber.nb_divide = (binaryfunc)&FNumberFuncs::Divide;
	PyNumber.nb_inplace_divide = (binaryfunc)&FNumberFuncs::InlineDivide;
#endif	// PY_MAJOR_VERSION >= 3
	PyNumber.nb_remainder = (binaryfunc)&FNumberFuncs::Modulus;
	PyNumber.nb_inplace_remainder = (binaryfunc)&FNumberFuncs::InlineModulus;
	PyNumber.nb_and = (binaryfunc)&FNumberFuncs::And;
	PyNumber.nb_inplace_and = (binaryfunc)&FNumberFuncs::InlineAnd;
	PyNumber.nb_or = (binaryfunc)&FNumberFuncs::Or;
	PyNumber.nb_inplace_or = (binaryfunc)&FNumberFuncs::InlineOr;
	PyNumber.nb_xor = (binaryfunc)&FNumberFuncs::Xor;
	PyNumber.nb_inplace_xor = (binaryfunc)&FNumberFuncs::InlineXor;
	PyNumber.nb_rshift = (binaryfunc)&FNumberFuncs::RightShift;
	PyNumber.nb_inplace_rshift = (binaryfunc)&FNumberFuncs::InlineRightShift;
	PyNumber.nb_lshift = (binaryfunc)&FNumberFuncs::LeftShift;
	PyNumber.nb_inplace_lshift = (binaryfunc)&FNumberFuncs::InlineLeftShift;

	PyType.tp_as_number = &PyNumber;

	return PyType;
}

PyTypeObject PyWrapperStructType = InitializePyWrapperStructType();

FPyWrapperStructMetaData::FPyWrapperStructMetaData()
	: Struct(nullptr)
{
}

/** Add object references from the given Python object to the given collector */
void FPyWrapperStructMetaData::AddReferencedObjects(FPyWrapperBase* Instance, FReferenceCollector& Collector)
{
	FPyWrapperStruct* Self = static_cast<FPyWrapperStruct*>(Instance);
	Collector.AddReferencedObject(Self->ScriptStruct);
	if (Self->ScriptStruct && Self->StructInstance && !Self->OwnerContext.HasOwner())
	{
		FPyReferenceCollector::AddReferencedObjectsFromStruct(Collector, Self->ScriptStruct, Self->StructInstance);
	}
}

UScriptStruct* FPyWrapperStructMetaData::GetStruct(PyTypeObject* PyType)
{
	FPyWrapperStructMetaData* PyWrapperMetaData = FPyWrapperStructMetaData::GetMetaData(PyType);
	return PyWrapperMetaData ? PyWrapperMetaData->Struct : nullptr;
}

UScriptStruct* FPyWrapperStructMetaData::GetStruct(FPyWrapperStruct* Instance)
{
	return GetStruct(Py_TYPE(Instance));
}

FName FPyWrapperStructMetaData::ResolvePropertyName(PyTypeObject* PyType, const FName InPythonPropertyName)
{
	if (FPyWrapperStructMetaData* PyWrapperMetaData = FPyWrapperStructMetaData::GetMetaData(PyType))
	{
		if (const FName* MappedPropName = PyWrapperMetaData->PythonProperties.Find(InPythonPropertyName))
		{
			return *MappedPropName;
		}

		if (const UScriptStruct* SuperStruct = PyWrapperMetaData->Struct ? Cast<UScriptStruct>(PyWrapperMetaData->Struct->GetSuperStruct()) : nullptr)
		{
			PyTypeObject* SuperStructPyType = FPyWrapperTypeRegistry::Get().GetWrappedStructType(SuperStruct);
			return ResolvePropertyName(SuperStructPyType, InPythonPropertyName);
		}
	}

	return InPythonPropertyName;
}

FName FPyWrapperStructMetaData::ResolvePropertyName(FPyWrapperStruct* Instance, const FName InPythonPropertyName)
{
	return ResolvePropertyName(Py_TYPE(Instance), InPythonPropertyName);
}

bool FPyWrapperStructMetaData::IsPropertyDeprecated(PyTypeObject* PyType, const FName InPythonPropertyName, FString* OutDeprecationMessage)
{
	if (FPyWrapperStructMetaData* PyWrapperMetaData = FPyWrapperStructMetaData::GetMetaData(PyType))
	{
		if (const FString* DeprecationMessage = PyWrapperMetaData->PythonDeprecatedProperties.Find(InPythonPropertyName))
		{
			if (OutDeprecationMessage)
			{
				*OutDeprecationMessage = *DeprecationMessage;
			}
			return true;
		}

		if (const UScriptStruct* SuperStruct = PyWrapperMetaData->Struct ? Cast<UScriptStruct>(PyWrapperMetaData->Struct->GetSuperStruct()) : nullptr)
		{
			PyTypeObject* SuperStructPyType = FPyWrapperTypeRegistry::Get().GetWrappedStructType(SuperStruct);
			return IsPropertyDeprecated(SuperStructPyType, InPythonPropertyName, OutDeprecationMessage);
		}
	}

	return false;
}

bool FPyWrapperStructMetaData::IsPropertyDeprecated(FPyWrapperStruct* Instance, const FName InPythonPropertyName, FString* OutDeprecationMessage)
{
	return IsPropertyDeprecated(Py_TYPE(Instance), InPythonPropertyName, OutDeprecationMessage);
}

bool FPyWrapperStructMetaData::IsStructDeprecated(PyTypeObject* PyType, FString* OutDeprecationMessage)
{
	if (FPyWrapperStructMetaData* PyWrapperMetaData = FPyWrapperStructMetaData::GetMetaData(PyType))
	{
		if (PyWrapperMetaData->DeprecationMessage.IsSet())
		{
			if (OutDeprecationMessage)
			{
				*OutDeprecationMessage = PyWrapperMetaData->DeprecationMessage.GetValue();
			}
			return true;
		}
	}

	return false;
}

bool FPyWrapperStructMetaData::IsStructDeprecated(FPyWrapperStruct* Instance, FString* OutDeprecationMessage)
{
	return IsStructDeprecated(Py_TYPE(Instance), OutDeprecationMessage);
}

struct FPythonGeneratedStructUtil
{
	static void PrepareOldStructForReinstancing(UPythonGeneratedStruct* InOldStruct)
	{
		const FString OldStructName = MakeUniqueObjectName(InOldStruct->GetOuter(), InOldStruct->GetClass(), *FString::Printf(TEXT("%s_REINST"), *InOldStruct->GetName())).ToString();
		InOldStruct->SetFlags(RF_NewerVersionExists);
		InOldStruct->ClearFlags(RF_Public | RF_Standalone);
		InOldStruct->Rename(*OldStructName, nullptr, REN_DontCreateRedirectors);
	}

	static UPythonGeneratedStruct* CreateStruct(const FString& InStructName, UObject* InStructOuter, UScriptStruct* InSuperStruct)
	{
		UPythonGeneratedStruct* Struct = NewObject<UPythonGeneratedStruct>(InStructOuter, *InStructName, RF_Public | RF_Standalone);
		Struct->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		Struct->SetSuperStruct(InSuperStruct);
		return Struct;
	}

	static void FinalizeStruct(UPythonGeneratedStruct* InStruct, PyTypeObject* InPyType)
	{
		// Build a complete list of init params for this struct
		TArray<PyGenUtil::FGeneratedWrappedMethodParameter> StructInitParams;
		if (const FPyWrapperStructMetaData* SuperMetaData = FPyWrapperStructMetaData::GetMetaData(InPyType->tp_base))
		{
			StructInitParams = SuperMetaData->InitParams;
		}
		for (const TSharedPtr<PyGenUtil::FPropertyDef>& PropDef : InStruct->PropertyDefs)
		{
			if (!PropDef->GeneratedWrappedGetSet.Prop.DeprecationMessage.IsSet())
			{
				PyGenUtil::FGeneratedWrappedMethodParameter& StructInitParam = StructInitParams.AddDefaulted_GetRef();
				StructInitParam.ParamName = PropDef->GeneratedWrappedGetSet.GetSetName;
				StructInitParam.ParamProp = PropDef->GeneratedWrappedGetSet.Prop.Prop;
				StructInitParam.ParamDefaultValue = FString();
			}
		}

		// Finalize the struct
		InStruct->Bind();
		InStruct->StaticLink(true);

		// Add the object meta-data to the type
		InStruct->PyMetaData.Struct = InStruct;
		InStruct->PyMetaData.InitParams = MoveTemp(StructInitParams);
		FPyWrapperStructMetaData::SetMetaData(InPyType, &InStruct->PyMetaData);

		// Map the Unreal struct to the Python type
		InStruct->PyType = FPyTypeObjectPtr::NewReference(InPyType);
		FPyWrapperTypeRegistry::Get().RegisterWrappedStructType(InStruct->GetFName(), InPyType);
	}

	static bool CreatePropertyFromDefinition(UPythonGeneratedStruct* InStruct, PyTypeObject* InPyType, const FString& InFieldName, FPyUPropertyDef* InPyPropDef)
	{
		UScriptStruct* SuperStruct = Cast<UScriptStruct>(InStruct->GetSuperStruct());

		// Resolve the property name to match any previously exported properties from the parent type
		const FName PropName = FPyWrapperStructMetaData::ResolvePropertyName(InPyType->tp_base, *InFieldName);
		if (SuperStruct && SuperStruct->FindPropertyByName(PropName))
		{
			PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Property '%s' (%s) cannot override a property from the base type"), *InFieldName, *PyUtil::GetFriendlyTypename(InPyPropDef->PropType)));
			return false;
		}

		// Structs cannot support getter/setter functions (or any functions)
		if (!InPyPropDef->GetterFuncName.IsEmpty() || !InPyPropDef->SetterFuncName.IsEmpty())
		{
			PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Struct property '%s' (%s) has a getter or setter, which is not supported on structs"), *InFieldName, *PyUtil::GetFriendlyTypename(InPyPropDef->PropType)));
			return false;
		}

		// Create the property from its definition
		UProperty* Prop = PyUtil::CreateProperty(InPyPropDef->PropType, 1, InStruct, PropName);
		if (!Prop)
		{
			PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to create property for '%s' (%s)"), *InFieldName, *PyUtil::GetFriendlyTypename(InPyPropDef->PropType)));
			return false;
		}
		Prop->PropertyFlags |= (CPF_Edit | CPF_BlueprintVisible);
		FPyUPropertyDef::ApplyMetaData(InPyPropDef, Prop);
		InStruct->AddCppProperty(Prop);

		// Build the definition data for the new property accessor
		PyGenUtil::FPropertyDef& PropDef = *InStruct->PropertyDefs.Add_GetRef(MakeShared<PyGenUtil::FPropertyDef>());
		PropDef.GeneratedWrappedGetSet.GetSetName = PyGenUtil::TCHARToUTF8Buffer(*InFieldName);
		PropDef.GeneratedWrappedGetSet.GetSetDoc = PyGenUtil::TCHARToUTF8Buffer(*FString::Printf(TEXT("type: %s\n%s"), *PyGenUtil::GetPropertyPythonType(Prop), *PyGenUtil::GetFieldTooltip(Prop)));
		PropDef.GeneratedWrappedGetSet.Prop.SetProperty(Prop);
		PropDef.GeneratedWrappedGetSet.GetCallback = (getter)&FPyWrapperStruct::Getter_Impl;
		PropDef.GeneratedWrappedGetSet.SetCallback = (setter)&FPyWrapperStruct::Setter_Impl;
		PropDef.GeneratedWrappedGetSet.ToPython(PropDef.PyGetSet);

		return true;
	}

	static bool RegisterDescriptors(UPythonGeneratedStruct* InStruct, PyTypeObject* InPyType)
	{
		for (const TSharedPtr<PyGenUtil::FPropertyDef>& PropDef : InStruct->PropertyDefs)
		{
			FPyObjectPtr GetSetDesc = FPyObjectPtr::StealReference(PyDescr_NewGetSet(InPyType, &PropDef->PyGetSet));
			if (!GetSetDesc)
			{
				PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to create descriptor for '%s'"), UTF8_TO_TCHAR(PropDef->PyGetSet.name)));
				return false;
			}
			if (PyDict_SetItemString(InPyType->tp_dict, PropDef->PyGetSet.name, GetSetDesc) != 0)
			{
				PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to assign descriptor for '%s'"), UTF8_TO_TCHAR(PropDef->PyGetSet.name)));
				return false;
			}
		}

		return true;
	}
};

void UPythonGeneratedStruct::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);

	FPyWrapperTypeRegistry::Get().UnregisterWrappedStructType(OldName, PyType);
	FPyWrapperTypeRegistry::Get().RegisterWrappedStructType(GetFName(), PyType, !HasAnyFlags(RF_NewerVersionExists));
}

void UPythonGeneratedStruct::InitializeStruct(void* Dest, int32 ArrayDim) const
{
	Super::InitializeStruct(Dest, ArrayDim);

	// Execute Python code within this block
	{
		FPyScopedGIL GIL;

		if (PyPostInitFunction)
		{
			const int32 Stride = GetStructureSize();
			for (int32 ArrIndex = 0; ArrIndex < ArrayDim; ++ArrIndex)
			{
				void* StructInstance = static_cast<uint8*>(Dest) + (ArrIndex * Stride);
				FPyObjectPtr PySelf = FPyObjectPtr::StealReference((PyObject*)FPyWrapperStructFactory::Get().CreateInstance((UPythonGeneratedStruct*)this, StructInstance, FPyWrapperOwnerContext(Py_None), EPyConversionMethod::Reference));
				if (PySelf && ensureAlways(PySelf->ob_type == PyType))
				{
					FPyObjectPtr PyArgs = FPyObjectPtr::StealReference(PyTuple_New(1));
					PyTuple_SetItem(PyArgs, 0, PySelf.Release()); // SetItem steals the reference

					FPyObjectPtr Result = FPyObjectPtr::StealReference(PyObject_CallObject((PyObject*)PyPostInitFunction.GetPtr(), PyArgs));
					if (!Result)
					{
						PyUtil::ReThrowPythonError();
					}
				}
			}
		}
	}
}

UPythonGeneratedStruct* UPythonGeneratedStruct::GenerateStruct(PyTypeObject* InPyType)
{
	UObject* StructOuter = GetPythonTypeContainer();
	const FString StructName = PyUtil::GetCleanTypename(InPyType);

	// Get the correct super struct from the parent type in Python
	UScriptStruct* SuperStruct = nullptr;
	if (InPyType->tp_base != &PyWrapperStructType)
	{
		SuperStruct = FPyWrapperStructMetaData::GetStruct(InPyType->tp_base);
		if (!SuperStruct)
		{
			PyUtil::SetPythonError(PyExc_Exception, InPyType, TEXT("No super struct could be found for this Python type"));
			return nullptr;
		}
	}

	UPythonGeneratedStruct* OldStruct = FindObject<UPythonGeneratedStruct>(StructOuter, *StructName);
	if (OldStruct)
	{
		FPythonGeneratedStructUtil::PrepareOldStructForReinstancing(OldStruct);
	}

	UPythonGeneratedStruct* Struct = FPythonGeneratedStructUtil::CreateStruct(StructName, StructOuter, SuperStruct);

	// Get the post-init function
	Struct->PyPostInitFunction = FPyObjectPtr::StealReference(PyGenUtil::GetPostInitFunc(InPyType));
	if (!Struct->PyPostInitFunction)
	{
		return nullptr;
	}

	// Add the fields to this struct
	{
		PyObject* FieldKey = nullptr;
		PyObject* FieldValue = nullptr;
		Py_ssize_t FieldIndex = 0;
		while (PyDict_Next(InPyType->tp_dict, &FieldIndex, &FieldKey, &FieldValue))
		{
			const FString FieldName = PyUtil::PyObjectToUEString(FieldKey);

			if (PyObject_IsInstance(FieldValue, (PyObject*)&PyUValueDefType) == 1)
			{
				// Values are not supported on structs
				PyUtil::SetPythonError(PyExc_Exception, InPyType, TEXT("Structs do not support values"));
				return nullptr;
			}

			if (PyObject_IsInstance(FieldValue, (PyObject*)&PyUPropertyDefType) == 1)
			{
				FPyUPropertyDef* PyPropDef = (FPyUPropertyDef*)FieldValue;
				if (!FPythonGeneratedStructUtil::CreatePropertyFromDefinition(Struct, InPyType, FieldName, PyPropDef))
				{
					return nullptr;
				}
			}

			if (PyObject_IsInstance(FieldValue, (PyObject*)&PyUFunctionDefType) == 1)
			{
				// Functions are not supported on structs
				PyUtil::SetPythonError(PyExc_Exception, InPyType, TEXT("Structs do not support methods"));
				return nullptr;
			}
		}
	}

	// Replace the definitions with real descriptors
	if (!FPythonGeneratedStructUtil::RegisterDescriptors(Struct, InPyType))
	{
		return nullptr;
	}

	// Let Python know that we've changed its type
	PyType_Modified(InPyType);

	// Finalize the struct
	FPythonGeneratedStructUtil::FinalizeStruct(Struct, InPyType);

	// Re-instance the old struct
	if (OldStruct)
	{
		FPyWrapperTypeReinstancer::Get().AddPendingStruct(OldStruct, Struct);
	}

	return Struct;
}

#endif	// WITH_PYTHON
