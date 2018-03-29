// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PyWrapperStruct.h"
#include "PyWrapperTypeRegistry.h"
#include "PyCore.h"
#include "PyReferenceCollector.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "UObject/PropertyPortFlags.h"
#include "Templates/Casts.h"
#include "Engine/UserDefinedStruct.h"

#if WITH_PYTHON

void InitializePyWrapperStruct(PyObject* PyModule)
{
	if (PyType_Ready(&PyWrapperStructType) == 0)
	{
		static FPyWrapperStructMetaData MetaData;
		FPyWrapperStructMetaData::SetMetaData(&PyWrapperStructType, &MetaData);

		Py_INCREF(&PyWrapperStructType);
		PyModule_AddObject(PyModule, PyWrapperStructType.tp_name, (PyObject*)&PyWrapperStructType);
	}
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

	UScriptStruct* Struct = Cast<UScriptStruct>(FPyWrapperStructMetaData::GetStruct(InSelf));
	if (!Struct)
	{
		PyUtil::SetPythonError(PyExc_Exception, InSelf, TEXT("Struct is null"));
		return -1;
	}

	const IPyWrapperStructAllocationPolicy* AllocPolicy = FPyWrapperStructMetaData::GetAllocationPolicy(InSelf);
	if (!AllocPolicy)
	{
		PyUtil::SetPythonError(PyExc_Exception, InSelf, TEXT("AllocPolicy is null"));
		return -1;
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

	const IPyWrapperStructAllocationPolicy* AllocPolicy = FPyWrapperStructMetaData::GetAllocationPolicy(InSelf);
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

		const IPyWrapperStructAllocationPolicy* AllocPolicy = FPyWrapperStructMetaData::GetAllocationPolicy(InSelf);
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

				const int Result = PyUtil::SetUEPropValue(NewStruct->ScriptStruct, NewStruct->StructInstance, SequenceItem, InitParam.ParamProp, InitParam.ParamName.GetData(), FPyWrapperOwnerContext(), 0, *PyUtil::GetErrorContext(NewStruct.Get()));
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

		// Attempt to convert each matching entry in the dict to the corresponding struct entry
		for (const PyGenUtil::FGeneratedWrappedMethodParameter& InitParam : StructMetaData->InitParams)
		{
			PyObject* MappingItem = PyMapping_GetItemString(InPyObject, (char*)InitParam.ParamName.GetData());
			if (MappingItem)
			{
				const int Result = PyUtil::SetUEPropValue(NewStruct->ScriptStruct, NewStruct->StructInstance, MappingItem, InitParam.ParamProp, InitParam.ParamName.GetData(), FPyWrapperOwnerContext(), 0, *PyUtil::GetErrorContext(NewStruct.Get()));
				if (Result != 0)
				{
					return nullptr;
				}
			}
		}

		SetOptionalPyConversionResult(FPyConversionResult::SuccessWithCoercion(), OutCastResult);
		return NewStruct.Release();
	}

	return nullptr;
}

int FPyWrapperStruct::SetPropertyValues(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds)
{
	if (!ValidateInternalState(InSelf))
	{
		return -1;
	}

	FPyWrapperStructMetaData* StructMetaData = FPyWrapperStructMetaData::GetMetaData(InSelf);
	if (!StructMetaData)
	{
		return 0;
	}

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
			const int Result = PyUtil::SetUEPropValue(InSelf->ScriptStruct, InSelf->StructInstance, PyValue, InitParam.ParamProp, InitParam.ParamName.GetData(), FPyWrapperOwnerContext(), 0, *PyUtil::GetErrorContext(InSelf));
			if (Result != 0)
			{
				return Result;
			}
		}
	}

	return 0;
}

PyObject* FPyWrapperStruct::GetPropertyValueByName(FPyWrapperStruct* InSelf, const FName InPropName, const char* InPythonAttrName)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	UProperty* Prop = InSelf->ScriptStruct->FindPropertyByName(InPropName);
	if (!Prop)
	{
		PyUtil::SetPythonError(PyExc_Exception, InSelf, *FString::Printf(TEXT("Failed to find property '%s' for attribute '%s' on '%s'"), *InPropName.ToString(), UTF8_TO_TCHAR(InPythonAttrName), *InSelf->ScriptStruct->GetName()));
		return nullptr;
	}

	return GetPropertyValue(InSelf, Prop, InPythonAttrName);
}

PyObject* FPyWrapperStruct::GetPropertyValue(FPyWrapperStruct* InSelf, const UProperty* InProp, const char* InPythonAttrName)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	return PyUtil::GetUEPropValue(InSelf->ScriptStruct, InSelf->StructInstance, InProp, InPythonAttrName, (PyObject*)InSelf, *PyUtil::GetErrorContext(InSelf));
}

int FPyWrapperStruct::SetPropertyValueByName(FPyWrapperStruct* InSelf, PyObject* InValue, const FName InPropName, const char* InPythonAttrName, const bool InNotifyChange, const uint64 InReadOnlyFlags)
{
	if (!ValidateInternalState(InSelf))
	{
		return -1;
	}

	UProperty* Prop = InSelf->ScriptStruct->FindPropertyByName(InPropName);
	if (!Prop)
	{
		PyUtil::SetPythonError(PyExc_Exception, InSelf, *FString::Printf(TEXT("Failed to find property '%s' for attribute '%s' on '%s'"), *InPropName.ToString(), UTF8_TO_TCHAR(InPythonAttrName), *InSelf->ScriptStruct->GetName()));
		return -1;
	}

	return SetPropertyValue(InSelf, InValue, Prop, InPythonAttrName, InNotifyChange, InReadOnlyFlags);
}

int FPyWrapperStruct::SetPropertyValue(FPyWrapperStruct* InSelf, PyObject* InValue, const UProperty* InProp, const char* InPythonAttrName, const bool InNotifyChange, const uint64 InReadOnlyFlags)
{
	if (!ValidateInternalState(InSelf))
	{
		return -1;
	}

	const FPyWrapperOwnerContext ChangeOwner = InNotifyChange ? FPyWrapperOwnerContext((PyObject*)InSelf, InProp) : FPyWrapperOwnerContext();
	return PyUtil::SetUEPropValue(InSelf->ScriptStruct, InSelf->StructInstance, InValue, InProp, InPythonAttrName, ChangeOwner, InReadOnlyFlags, *PyUtil::GetErrorContext(InSelf));
}

PyObject* FPyWrapperStruct::CallFunction_Impl(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef, const PyGenUtil::FGeneratedWrappedMethodParameter& InStructParam, const char* InPythonFuncName, const TCHAR* InErrorCtxt)
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
		{
			FString DeprecationMessage;
			if (PyGenUtil::IsDeprecatedFunction(InFuncDef.Func, &DeprecationMessage) && 
				PyUtil::SetPythonWarning(PyExc_DeprecationWarning, InErrorCtxt, *FString::Printf(TEXT("Function '%s.%s' is deprecated: %s"), *Class->GetName(), *InFuncDef.Func->GetName(), *DeprecationMessage)) == -1
				)
			{
				// -1 from SetPythonWarning means the warning should be an exception
				return nullptr;
			}
		}

		FStructOnScope FuncParams(InFuncDef.Func);
		PyGenUtil::ApplyParamDefaults(FuncParams.GetStructMemory(), InFuncDef.InputParams);
		if (ensureAlways(InStructParam.ParamProp))
		{
			void* StructArgInstance = InStructParam.ParamProp->ContainerPtrToValuePtr<void>(FuncParams.GetStructMemory());
			CastChecked<UStructProperty>(InStructParam.ParamProp)->Struct->CopyScriptStruct(StructArgInstance, InSelf->StructInstance);
		}
		for (int32 ParamIndex = 0; ParamIndex < Params.Num(); ++ParamIndex)
		{
			const PyGenUtil::FGeneratedWrappedMethodParameter& ParamDef = InFuncDef.InputParams[ParamIndex];

			PyObject* PyValue = Params[ParamIndex];
			if (PyValue)
			{
				if (!PyConversion::NativizeProperty_InContainer(PyValue, ParamDef.ParamProp, FuncParams.GetStructMemory(), 0))
				{
					PyUtil::SetPythonError(PyExc_TypeError, InErrorCtxt, *FString::Printf(TEXT("Failed to convert parameter '%s.%s' when calling function '%s' on '%s'"), UTF8_TO_TCHAR(ParamDef.ParamName.GetData()), *Class->GetName(), *InFuncDef.Func->GetName(), *Obj->GetName()));
					return nullptr;
				}
			}
		}
		PyUtil::InvokeFunctionCall(Obj, InFuncDef.Func, FuncParams.GetStructMemory(), InErrorCtxt);
		return PyGenUtil::PackReturnValues(FuncParams.GetStructMemory(), InFuncDef.OutputParams, InErrorCtxt, *FString::Printf(TEXT("function '%s.%s' on '%s'"), *Class->GetName(), *InFuncDef.Func->GetName(), *Obj->GetName()));
	}

	Py_RETURN_NONE;
}

PyObject* FPyWrapperStruct::CallMethodNoArgs_Impl(FPyWrapperStruct* InSelf, void* InClosure)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	const PyGenUtil::FGeneratedWrappedDynamicStructMethod* Closure = (PyGenUtil::FGeneratedWrappedDynamicStructMethod*)InClosure;
	return CallFunction_Impl(InSelf, nullptr, nullptr, Closure->MethodFunc, Closure->StructParam, Closure->MethodName.GetData(), *PyUtil::GetErrorContext(InSelf));
}

PyObject* FPyWrapperStruct::CallMethodWithArgs_Impl(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds, void* InClosure)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	const PyGenUtil::FGeneratedWrappedDynamicStructMethod* Closure = (PyGenUtil::FGeneratedWrappedDynamicStructMethod*)InClosure;
	return CallFunction_Impl(InSelf, InArgs, InKwds, Closure->MethodFunc, Closure->StructParam, Closure->MethodName.GetData(), *PyUtil::GetErrorContext(InSelf));
}

PyObject* FPyWrapperStruct::CallBinaryOperatorFunction_Impl(FPyWrapperStruct* InSelf, PyObject* InRHS, const PyGenUtil::FGeneratedWrappedStructMathOpFunction& InMathOpFunc, const bool InInlineOp, const TOptional<EPyConversionResultState> InRequiredConversionResult, FPyConversionResult* OutRHSConversionResult)
{
	SetOptionalPyConversionResult(FPyConversionResult::Failure(), OutRHSConversionResult);

	// Binary functions must have a single input parameter (excluding the struct parameter) and a single output parameter (the return value)
	if (InMathOpFunc.InputParams.Num() != 1 || InMathOpFunc.OutputParams.Num() != 1)
	{
		return nullptr;
	}

	if (ensureAlways(InMathOpFunc.Func))
	{
		UClass* Class = InMathOpFunc.Func->GetOwnerClass();
		UObject* Obj = Class->GetDefaultObject();

		// Build the input arguments (failures here aren't fatal as we may have multiple functions to evaluate on the stack, only one of which may accept the RHS parameter)
		FStructOnScope FuncParams(InMathOpFunc.Func);
		{
			const FPyConversionResult RHSResult = PyConversion::NativizeProperty_InContainer(InRHS, InMathOpFunc.InputParams[0].ParamProp, FuncParams.GetStructMemory(), 0, FPyWrapperOwnerContext(), PyConversion::ESetErrorState::No);
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
		if (ensureAlways(InMathOpFunc.StructParam.ParamProp))
		{
			void* StructArgInstance = InMathOpFunc.StructParam.ParamProp->ContainerPtrToValuePtr<void>(FuncParams.GetStructMemory());
			CastChecked<UStructProperty>(InMathOpFunc.StructParam.ParamProp)->Struct->CopyScriptStruct(StructArgInstance, InSelf->StructInstance);
		}
		PyUtil::InvokeFunctionCall(Obj, InMathOpFunc.Func, FuncParams.GetStructMemory(), *PyUtil::GetErrorContext(InSelf));

		PyObject* ReturnPyObj = nullptr;
		const PyGenUtil::FGeneratedWrappedMethodParameter& ReturnParamDef = InMathOpFunc.OutputParams[0];
		if (InInlineOp)
		{
			// Copy the result back into ourself
			if (const UStructProperty* ReturnStructProp = Cast<const UStructProperty>(ReturnParamDef.ParamProp))
			{
				if (ReturnStructProp->Struct == FPyWrapperStructMetaData::GetStruct(InSelf))
				{
					void* ReturnStructInstance = ReturnStructProp->ContainerPtrToValuePtr<void>(FuncParams.GetStructMemory());
					ReturnStructProp->Struct->CopyScriptStruct(InSelf->StructInstance, ReturnStructInstance);

					Py_INCREF(InSelf);
					ReturnPyObj = (PyObject*)InSelf;
				}
			}
		}
		else
		{
			PyConversion::PythonizeProperty_InContainer(ReturnParamDef.ParamProp, FuncParams.GetStructMemory(), 0, ReturnPyObj, EPyConversionMethod::Steal);
		}

		if (!ReturnPyObj)
		{
			PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert return property '%s' (%s) when calling function '%s' on '%s'"), *ReturnParamDef.ParamProp->GetName(), *ReturnParamDef.ParamProp->GetClass()->GetName(), *InMathOpFunc.Func->GetName(), *Obj->GetName()));
		}

		return ReturnPyObj;
	}

	return nullptr;
}

PyObject* FPyWrapperStruct::CallBinaryOperator_Impl(FPyWrapperStruct* InSelf, PyObject* InRHS, const PyGenUtil::FGeneratedWrappedStructMathOpStack::EOpType InOpType)
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

	// We process the operator stack in two passes:
	//	- The first pass looks for a signature that exactly matches the given argument
	//	- The second pass allows type coercion to occur when calling the signature
	// We use the first pass to find a function that may be called for the second pass
	const bool bInlineOp = PyGenUtil::FGeneratedWrappedStructMathOpStack::IsInlineOp(InOpType);
	const PyGenUtil::FGeneratedWrappedStructMathOpFunction* CoercedMathOpFunc = nullptr;
	for (const PyGenUtil::FGeneratedWrappedStructMathOpFunction& MathOpFunc : StructMetaData->MathOpStacks[(int32)InOpType].MathOpFuncs)
	{
		FPyConversionResult RHSConversionResult = FPyConversionResult::Failure();
		PyObject* PyResult = CallBinaryOperatorFunction_Impl(InSelf, InRHS, MathOpFunc, bInlineOp, EPyConversionResultState::Success, &RHSConversionResult);
		if (PyResult)
		{
			return PyResult;
		}
		else if (RHSConversionResult.GetState() == EPyConversionResultState::SuccessWithCoercion)
		{
			CoercedMathOpFunc = &MathOpFunc;
		}
	}
	if (CoercedMathOpFunc)
	{
		PyObject* PyResult = CallBinaryOperatorFunction_Impl(InSelf, InRHS, *CoercedMathOpFunc, bInlineOp);
		if (PyResult)
		{
			return PyResult;
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
			UStruct* Struct = FPyWrapperStructMetaData::GetStruct(InType);
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

		static PyObject* GetEditorProperty(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds)
		{
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
			return FPyWrapperStruct::GetPropertyValueByName(InSelf, ResolvedName, TCHAR_TO_UTF8(*Name.ToString()));
		}

		static PyObject* SetEditorProperty(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds)
		{
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
			const int Result = FPyWrapperStruct::SetPropertyValueByName(InSelf, PyValueObj, ResolvedName, TCHAR_TO_UTF8(*Name.ToString()), /*InNotifyChange*/true, CPF_EditConst);
			if (Result != 0)
			{
				return nullptr;
			}

			Py_RETURN_NONE;
		}
	};

	static PyMethodDef PyMethods[] = {
		{ PyGenUtil::PostInitFuncName, PyCFunctionCast(&FMethods::PostInit), METH_NOARGS, "x._post_init() -- called during Unreal struct initialization (equivalent to PostInitProperties in C++)" },
		{ "cast", PyCFunctionCast(&FMethods::Cast), METH_VARARGS | METH_CLASS, "X.cast(object) -> struct -- cast the given object to this Unreal struct type" },
		{ "static_struct", PyCFunctionCast(&FMethods::StaticStruct), METH_NOARGS | METH_CLASS, "X.static_struct() -> UStruct -- get the Unreal struct of this type" },
		{ "__copy__", PyCFunctionCast(&FMethods::Copy), METH_NOARGS, "x.__copy__() -> struct -- copy this Unreal struct" },
		{ "copy", PyCFunctionCast(&FMethods::Copy), METH_NOARGS, "x.copy() -> struct -- copy this Unreal struct" },
		{ "get_editor_property", PyCFunctionCast(&FMethods::GetEditorProperty), METH_VARARGS | METH_KEYWORDS, "x.get_editor_property(name) -> object -- get the value of any property visible to the editor" },
		{ "set_editor_property", PyCFunctionCast(&FMethods::SetEditorProperty), METH_VARARGS | METH_KEYWORDS, "x.set_editor_property(name, value) -- set the value of any property visible to the editor, ensuring that the pre/post change notifications are called" },
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
	PyType.tp_hash = (hashfunc)&FFuncs::Hash;

	PyType.tp_methods = PyMethods;

	PyType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
	PyType.tp_doc = "Type for all UE4 exposed struct instances";

	return PyType;
}

PyTypeObject PyWrapperStructType = InitializePyWrapperStructType();

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

FPyWrapperStructMetaData::FPyWrapperStructMetaData()
	: AllocPolicy(nullptr)
	, Struct(nullptr)
{
	static const FPyWrapperStructAllocationPolicy_Heap HeapAllocPolicy = FPyWrapperStructAllocationPolicy_Heap();
	AllocPolicy = &HeapAllocPolicy;
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

const IPyWrapperStructAllocationPolicy* FPyWrapperStructMetaData::GetAllocationPolicy(PyTypeObject* PyType)
{
	FPyWrapperStructMetaData* PyWrapperMetaData = FPyWrapperStructMetaData::GetMetaData(PyType);
	return PyWrapperMetaData ? PyWrapperMetaData->AllocPolicy : nullptr;
}

const IPyWrapperStructAllocationPolicy* FPyWrapperStructMetaData::GetAllocationPolicy(FPyWrapperStruct* Instance)
{
	return GetAllocationPolicy(Py_TYPE(Instance));
}

UStruct* FPyWrapperStructMetaData::GetStruct(PyTypeObject* PyType)
{
	FPyWrapperStructMetaData* PyWrapperMetaData = FPyWrapperStructMetaData::GetMetaData(PyType);
	return PyWrapperMetaData ? PyWrapperMetaData->Struct : nullptr;
}

UStruct* FPyWrapperStructMetaData::GetStruct(FPyWrapperStruct* Instance)
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

		if (const UStruct* SuperStruct = PyWrapperMetaData->Struct ? PyWrapperMetaData->Struct->GetSuperStruct() : nullptr)
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

struct FPythonGeneratedStructUtil
{
	static void PrepareOldStructForReinstancing(UPythonGeneratedStruct* InOldStruct)
	{
		const FString OldStructName = MakeUniqueObjectName(InOldStruct->GetOuter(), InOldStruct->GetClass(), *FString::Printf(TEXT("%s_REINST"), *InOldStruct->GetName())).ToString();
		InOldStruct->SetFlags(RF_NewerVersionExists);
		InOldStruct->ClearFlags(RF_Public | RF_Standalone);
		InOldStruct->Rename(*OldStructName, nullptr, REN_DontCreateRedirectors);
	}

	static UPythonGeneratedStruct* CreateStruct(const FString& InStructName, UObject* InStructOuter, UStruct* InSuperStruct)
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
			PyGenUtil::FGeneratedWrappedMethodParameter& StructInitParam = StructInitParams.AddDefaulted_GetRef();
			StructInitParam.ParamName = PropDef->GeneratedWrappedGetSet.GetSetName;
			StructInitParam.ParamProp = PropDef->GeneratedWrappedGetSet.Prop;
			StructInitParam.ParamDefaultValue = FString();
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
		UStruct* SuperStruct = InStruct->GetSuperStruct();

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
		PropDef.GeneratedWrappedGetSet.Prop = Prop;
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

	FPyWrapperTypeRegistry::Get().RegisterWrappedStructType(OldName, nullptr);
	FPyWrapperTypeRegistry::Get().RegisterWrappedStructType(GetFName(), PyType);
}

void UPythonGeneratedStruct::InitializeStruct(void* Dest, int32 ArrayDim) const
{
	Super::InitializeStruct(Dest, ArrayDim);

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
					PyUtil::LogPythonError();
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
	UStruct* SuperStruct = nullptr;
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
