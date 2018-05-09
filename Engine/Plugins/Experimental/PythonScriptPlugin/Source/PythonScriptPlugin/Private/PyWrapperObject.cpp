// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PyWrapperObject.h"
#include "PyWrapperOwnerContext.h"
#include "PyWrapperTypeRegistry.h"
#include "PyGIL.h"
#include "PyCore.h"
#include "PyUtil.h"
#include "PyConversion.h"
#include "PyReferenceCollector.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "UObject/MetaData.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"
#include "UObject/StructOnScope.h"
#include "UObject/PropertyPortFlags.h"
#include "Engine/World.h"
#include "Templates/Casts.h"

#if WITH_PYTHON

void InitializePyWrapperObject(PyGenUtil::FNativePythonModule& ModuleInfo)
{
	if (PyType_Ready(&PyWrapperObjectType) == 0)
	{
		static FPyWrapperObjectMetaData MetaData;
		FPyWrapperObjectMetaData::SetMetaData(&PyWrapperObjectType, &MetaData);
		ModuleInfo.AddType(&PyWrapperObjectType);
	}
}

FPyWrapperObject* FPyWrapperObject::New(PyTypeObject* InType)
{
	FPyWrapperObject* Self = (FPyWrapperObject*)FPyWrapperBase::New(InType);
	if (Self)
	{
		Self->ObjectInstance = nullptr;
	}
	return Self;
}

void FPyWrapperObject::Free(FPyWrapperObject* InSelf)
{
	Deinit(InSelf);

	FPyWrapperBase::Free(InSelf);
}

int FPyWrapperObject::Init(FPyWrapperObject* InSelf, UObject* InValue)
{
	Deinit(InSelf);

	const int BaseInit = FPyWrapperBase::Init(InSelf);
	if (BaseInit != 0)
	{
		return BaseInit;
	}

	check(InValue);

	InSelf->ObjectInstance = InValue;
	FPyWrapperObjectFactory::Get().MapInstance(InSelf->ObjectInstance, InSelf);
	return 0;
}

void FPyWrapperObject::Deinit(FPyWrapperObject* InSelf)
{
	if (InSelf->ObjectInstance)
	{
		FPyWrapperObjectFactory::Get().UnmapInstance(InSelf->ObjectInstance, Py_TYPE(InSelf));
	}
	InSelf->ObjectInstance = nullptr;
}

bool FPyWrapperObject::ValidateInternalState(FPyWrapperObject* InSelf)
{
	if (!InSelf->ObjectInstance)
	{
		PyUtil::SetPythonError(PyExc_Exception, Py_TYPE(InSelf), TEXT("Internal Error - ObjectInstance is null!"));
		return false;
	}

	return true;
}

FPyWrapperObject* FPyWrapperObject::CastPyObject(PyObject* InPyObject, FPyConversionResult* OutCastResult)
{
	SetOptionalPyConversionResult(FPyConversionResult::Failure(), OutCastResult);

	if (PyObject_IsInstance(InPyObject, (PyObject*)&PyWrapperObjectType) == 1)
	{
		SetOptionalPyConversionResult(FPyConversionResult::Success(), OutCastResult);

		Py_INCREF(InPyObject);
		return (FPyWrapperObject*)InPyObject;
	}

	return nullptr;
}

FPyWrapperObject* FPyWrapperObject::CastPyObject(PyObject* InPyObject, PyTypeObject* InType, FPyConversionResult* OutCastResult)
{
	SetOptionalPyConversionResult(FPyConversionResult::Failure(), OutCastResult);

	if (PyObject_IsInstance(InPyObject, (PyObject*)InType) == 1 && (InType == &PyWrapperObjectType || PyObject_IsInstance(InPyObject, (PyObject*)&PyWrapperObjectType) == 1))
	{
		SetOptionalPyConversionResult(Py_TYPE(InPyObject) == InType ? FPyConversionResult::Success() : FPyConversionResult::SuccessWithCoercion(), OutCastResult);

		Py_INCREF(InPyObject);
		return (FPyWrapperObject*)InPyObject;
	}

	return nullptr;
}

PyObject* FPyWrapperObject::GetPropertyValue(FPyWrapperObject* InSelf, const PyGenUtil::FGeneratedWrappedProperty& InPropDef, const char* InPythonAttrName)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	return PyGenUtil::GetPropertyValue(InSelf->ObjectInstance->GetClass(), InSelf->ObjectInstance, InPropDef, InPythonAttrName, (PyObject*)InSelf, *PyUtil::GetErrorContext(InSelf));
}

int FPyWrapperObject::SetPropertyValue(FPyWrapperObject* InSelf, PyObject* InValue, const PyGenUtil::FGeneratedWrappedProperty& InPropDef, const char* InPythonAttrName, const bool InNotifyChange, const uint64 InReadOnlyFlags)
{
	if (!ValidateInternalState(InSelf))
	{
		return -1;
	}

	const FPyWrapperOwnerContext ChangeOwner = InNotifyChange ? FPyWrapperOwnerContext((PyObject*)InSelf, InPropDef.Prop) : FPyWrapperOwnerContext();
	return PyGenUtil::SetPropertyValue(InSelf->ObjectInstance->GetClass(), InSelf->ObjectInstance, InValue, InPropDef, InPythonAttrName, ChangeOwner, InReadOnlyFlags, InSelf->ObjectInstance->IsTemplate(), *PyUtil::GetErrorContext(InSelf));
}

PyObject* FPyWrapperObject::CallGetterFunction(FPyWrapperObject* InSelf, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	return CallFunction_Impl(InSelf->ObjectInstance, InFuncDef, InFuncDef.Func ? TCHAR_TO_UTF8(*InFuncDef.Func->GetName()) : "null", *PyUtil::GetErrorContext(InSelf));
}

int FPyWrapperObject::CallSetterFunction(FPyWrapperObject* InSelf, PyObject* InValue, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef)
{
	if (!ValidateInternalState(InSelf))
	{
		return -1;
	}

	if (ensureAlways(InFuncDef.Func))
	{
		// Deprecated functions emit a warning
		if (InFuncDef.DeprecationMessage.IsSet())
		{
			if (PyUtil::SetPythonWarning(PyExc_DeprecationWarning, InSelf, *FString::Printf(TEXT("Function '%s.%s' is deprecated: %s"), *InFuncDef.Func->GetOwnerClass()->GetName(), *InFuncDef.Func->GetName(), *InFuncDef.DeprecationMessage.GetValue())) == -1)
			{
				// -1 from SetPythonWarning means the warning should be an exception
				return -1;
			}
		}

		// Setter functions should have a single input parameter and no output parameters
		if (InFuncDef.InputParams.Num() != 1 || InFuncDef.OutputParams.Num() != 0)
		{
			PyUtil::SetPythonError(PyExc_Exception, InSelf, *FString::Printf(TEXT("Setter function '%s.%s' on '%s' has the incorrect number of parameters (expected 1 input and 0 output, got %d input and %d output)"), *InFuncDef.Func->GetOwnerClass()->GetName(), *InFuncDef.Func->GetName(), *InSelf->ObjectInstance->GetName(), InFuncDef.InputParams.Num(), InFuncDef.OutputParams.Num()));
			return -1;
		}

		FStructOnScope FuncParams(InFuncDef.Func);
		if (InValue)
		{
			if (!PyConversion::NativizeProperty_InContainer(InValue, InFuncDef.InputParams[0].ParamProp, FuncParams.GetStructMemory(), 0))
			{
				PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert input parameter when calling function '%s.%s' on '%s'"), *InFuncDef.Func->GetOwnerClass()->GetName(), *InFuncDef.Func->GetName(), *InSelf->ObjectInstance->GetName()));
				return -1;
			}
		}
		if (!PyUtil::InvokeFunctionCall(InSelf->ObjectInstance, InFuncDef.Func, FuncParams.GetStructMemory(), *PyUtil::GetErrorContext(InSelf)))
		{
			return -1;
		}
	}

	return 0;
}

PyObject* FPyWrapperObject::CallFunction(PyTypeObject* InType, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef, const char* InPythonFuncName)
{
	UClass* Class = FPyWrapperObjectMetaData::GetClass(InType);
	UObject* Obj = Class ? Class->GetDefaultObject() : nullptr;
	return CallFunction_Impl(Obj, InFuncDef, InPythonFuncName, *PyUtil::GetErrorContext(InType));
}

PyObject* FPyWrapperObject::CallFunction(PyTypeObject* InType, PyObject* InArgs, PyObject* InKwds, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef, const char* InPythonFuncName)
{
	UClass* Class = FPyWrapperObjectMetaData::GetClass(InType);
	UObject* Obj = Class ? Class->GetDefaultObject() : nullptr;
	return CallFunction_Impl(Obj, InArgs, InKwds, InFuncDef, InPythonFuncName, *PyUtil::GetErrorContext(InType));
}

PyObject* FPyWrapperObject::CallFunction(FPyWrapperObject* InSelf, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef, const char* InPythonFuncName)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	return CallFunction_Impl(InSelf->ObjectInstance, InFuncDef, InPythonFuncName, *PyUtil::GetErrorContext(InSelf));
}

PyObject* FPyWrapperObject::CallFunction(FPyWrapperObject* InSelf, PyObject* InArgs, PyObject* InKwds, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef, const char* InPythonFuncName)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	return CallFunction_Impl(InSelf->ObjectInstance, InArgs, InKwds, InFuncDef, InPythonFuncName, *PyUtil::GetErrorContext(InSelf));
}

PyObject* FPyWrapperObject::CallFunction_Impl(UObject* InObj, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef, const char* InPythonFuncName, const TCHAR* InErrorCtxt)
{
	if (InObj && ensureAlways(InFuncDef.Func))
	{
		// Deprecated functions emit a warning
		if (InFuncDef.DeprecationMessage.IsSet())
		{
			if (PyUtil::SetPythonWarning(PyExc_DeprecationWarning, InErrorCtxt, *FString::Printf(TEXT("Function '%s' on '%s' is deprecated: %s"), UTF8_TO_TCHAR(InPythonFuncName), *InFuncDef.Func->GetOwnerClass()->GetName(), *InFuncDef.DeprecationMessage.GetValue())) == -1)
			{
				// -1 from SetPythonWarning means the warning should be an exception
				return nullptr;
			}
		}

		if (InFuncDef.Func->Children == nullptr)
		{
			// No return value
			if (!PyUtil::InvokeFunctionCall(InObj, InFuncDef.Func, nullptr, InErrorCtxt))
			{
				return nullptr;
			}
		}
		else
		{
			// Return value requires that we create a params struct to hold the result
			FStructOnScope FuncParams(InFuncDef.Func);
			if (!PyUtil::InvokeFunctionCall(InObj, InFuncDef.Func, FuncParams.GetStructMemory(), InErrorCtxt))
			{
				return nullptr;
			}
			return PyGenUtil::PackReturnValues(FuncParams.GetStructMemory(), InFuncDef.OutputParams, InErrorCtxt, *FString::Printf(TEXT("function '%s.%s' on '%s'"), *InFuncDef.Func->GetOwnerClass()->GetName(), *InFuncDef.Func->GetName(), *InObj->GetName()));
		}
	}

	Py_RETURN_NONE;
}

PyObject* FPyWrapperObject::CallFunction_Impl(UObject* InObj, PyObject* InArgs, PyObject* InKwds, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef, const char* InPythonFuncName, const TCHAR* InErrorCtxt)
{
	TArray<PyObject*> Params;
	if (!PyGenUtil::ParseMethodParameters(InArgs, InKwds, InFuncDef.InputParams, InPythonFuncName, Params))
	{
		return nullptr;
	}

	if (InObj && ensureAlways(InFuncDef.Func))
	{
		// Deprecated functions emit a warning
		if (InFuncDef.DeprecationMessage.IsSet())
		{
			if (PyUtil::SetPythonWarning(PyExc_DeprecationWarning, InErrorCtxt, *FString::Printf(TEXT("Function '%s' on '%s' is deprecated: %s"), UTF8_TO_TCHAR(InPythonFuncName), *InFuncDef.Func->GetOwnerClass()->GetName(), *InFuncDef.DeprecationMessage.GetValue())) == -1)
			{
				// -1 from SetPythonWarning means the warning should be an exception
				return nullptr;
			}
		}

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
					PyUtil::SetPythonError(PyExc_TypeError, InErrorCtxt, *FString::Printf(TEXT("Failed to convert parameter '%s' when calling function '%s.%s' on '%s'"), UTF8_TO_TCHAR(ParamDef.ParamName.GetData()), *InFuncDef.Func->GetOwnerClass()->GetName(), *InFuncDef.Func->GetName(), *InObj->GetName()));
					return nullptr;
				}
			}
		}
		if (!PyUtil::InvokeFunctionCall(InObj, InFuncDef.Func, FuncParams.GetStructMemory(), InErrorCtxt))
		{
			return nullptr;
		}
		return PyGenUtil::PackReturnValues(FuncParams.GetStructMemory(), InFuncDef.OutputParams, InErrorCtxt, *FString::Printf(TEXT("function '%s.%s' on '%s'"), *InFuncDef.Func->GetOwnerClass()->GetName(), *InFuncDef.Func->GetName(), *InObj->GetName()));
	}

	Py_RETURN_NONE;
}

PyObject* FPyWrapperObject::CallClassMethodNoArgs_Impl(PyTypeObject* InType, void* InClosure)
{
	const PyGenUtil::FGeneratedWrappedMethod* Closure = (PyGenUtil::FGeneratedWrappedMethod*)InClosure;
	return CallFunction(InType, Closure->MethodFunc, Closure->MethodName.GetData());
}

PyObject* FPyWrapperObject::CallClassMethodWithArgs_Impl(PyTypeObject* InType, PyObject* InArgs, PyObject* InKwds, void* InClosure)
{
	const PyGenUtil::FGeneratedWrappedMethod* Closure = (PyGenUtil::FGeneratedWrappedMethod*)InClosure;
	return CallFunction(InType, InArgs, InKwds, Closure->MethodFunc, Closure->MethodName.GetData());
}

PyObject* FPyWrapperObject::CallMethodNoArgs_Impl(FPyWrapperObject* InSelf, void* InClosure)
{
	const PyGenUtil::FGeneratedWrappedMethod* Closure = (PyGenUtil::FGeneratedWrappedMethod*)InClosure;
	return CallFunction(InSelf, Closure->MethodFunc, Closure->MethodName.GetData());
}

PyObject* FPyWrapperObject::CallMethodWithArgs_Impl(FPyWrapperObject* InSelf, PyObject* InArgs, PyObject* InKwds, void* InClosure)
{
	const PyGenUtil::FGeneratedWrappedMethod* Closure = (PyGenUtil::FGeneratedWrappedMethod*)InClosure;
	return CallFunction(InSelf, InArgs, InKwds, Closure->MethodFunc, Closure->MethodName.GetData());
}

PyObject* FPyWrapperObject::CallDynamicFunction_Impl(FPyWrapperObject* InSelf, PyObject* InArgs, PyObject* InKwds, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef, const PyGenUtil::FGeneratedWrappedMethodParameter& InSelfParam, const char* InPythonFuncName)
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
		if (ensureAlways(Cast<UObjectPropertyBase>(InSelfParam.ParamProp)))
		{
			void* SelfArgInstance = InSelfParam.ParamProp->ContainerPtrToValuePtr<void>(FuncParams.GetStructMemory());
			Cast<UObjectPropertyBase>(InSelfParam.ParamProp)->SetObjectPropertyValue(SelfArgInstance, InSelf->ObjectInstance);
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
		return PyGenUtil::PackReturnValues(FuncParams.GetStructMemory(), InFuncDef.OutputParams, *ErrorCtxt, *FString::Printf(TEXT("function '%s.%s' on '%s'"), *Class->GetName(), *InFuncDef.Func->GetName(), *Obj->GetName()));
	}

	Py_RETURN_NONE;
}

PyObject* FPyWrapperObject::CallDynamicMethodNoArgs_Impl(FPyWrapperObject* InSelf, void* InClosure)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	const PyGenUtil::FGeneratedWrappedDynamicMethod* Closure = (PyGenUtil::FGeneratedWrappedDynamicMethod*)InClosure;
	return CallDynamicFunction_Impl(InSelf, nullptr, nullptr, Closure->MethodFunc, Closure->SelfParam, Closure->MethodName.GetData());
}

PyObject* FPyWrapperObject::CallDynamicMethodWithArgs_Impl(FPyWrapperObject* InSelf, PyObject* InArgs, PyObject* InKwds, void* InClosure)
{
	if (!ValidateInternalState(InSelf))
	{
		return nullptr;
	}

	const PyGenUtil::FGeneratedWrappedDynamicMethod* Closure = (PyGenUtil::FGeneratedWrappedDynamicMethod*)InClosure;
	return CallDynamicFunction_Impl(InSelf, InArgs, InKwds, Closure->MethodFunc, Closure->SelfParam, Closure->MethodName.GetData());
}

PyObject* FPyWrapperObject::Getter_Impl(FPyWrapperObject* InSelf, void* InClosure)
{
	const PyGenUtil::FGeneratedWrappedGetSet* Closure = (PyGenUtil::FGeneratedWrappedGetSet*)InClosure;
	return Closure->GetFunc.Func
		? CallGetterFunction(InSelf, Closure->GetFunc)
		: GetPropertyValue(InSelf, Closure->Prop, Closure->GetSetName.GetData());
}

int FPyWrapperObject::Setter_Impl(FPyWrapperObject* InSelf, PyObject* InValue, void* InClosure)
{
	const PyGenUtil::FGeneratedWrappedGetSet* Closure = (PyGenUtil::FGeneratedWrappedGetSet*)InClosure;
	return Closure->SetFunc.Func
		? CallSetterFunction(InSelf, InValue, Closure->SetFunc)
		: SetPropertyValue(InSelf, InValue, Closure->Prop, Closure->GetSetName.GetData());
}

PyTypeObject InitializePyWrapperObjectType()
{
	struct FFuncs
	{
		static PyObject* New(PyTypeObject* InType, PyObject* InArgs, PyObject* InKwds)
		{
			return (PyObject*)FPyWrapperObject::New(InType);
		}

		static void Dealloc(FPyWrapperObject* InSelf)
		{
			FPyWrapperObject::Free(InSelf);
		}

		static int Init(FPyWrapperObject* InSelf, PyObject* InArgs, PyObject* InKwds)
		{
			UObject* InitValue = nullptr;

			UObject* ObjectOuter = GetTransientPackage();
			FName ObjectName;

			// Parse the args
			{
				PyObject* PyOuterObj = nullptr;
				PyObject* PyNameObj = nullptr;

				static const char *ArgsKwdList[] = { "outer", "name", nullptr };
				if (!PyArg_ParseTupleAndKeywords(InArgs, InKwds, "|OO:call", (char**)ArgsKwdList, &PyOuterObj, &PyNameObj))
				{
					return -1;
				}

				if (PyOuterObj && !PyConversion::Nativize(PyOuterObj, ObjectOuter))
				{
					PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert 'outer' (%s) to 'Object'"), *PyUtil::GetFriendlyTypename(PyOuterObj)));
					return -1;
				}

				if (PyNameObj && !PyConversion::Nativize(PyNameObj, ObjectName))
				{
					PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert 'name' (%s) to 'Name'"), *PyUtil::GetFriendlyTypename(PyNameObj)));
					return -1;
				}
			}

			UClass* ObjClass = FPyWrapperObjectMetaData::GetClass(InSelf);
			if (ObjClass)
			{
				// Deprecated classes emit a warning
				{
					FString DeprecationMessage;
					if (FPyWrapperObjectMetaData::IsClassDeprecated(InSelf, &DeprecationMessage) &&
						PyUtil::SetPythonWarning(PyExc_DeprecationWarning, InSelf, *FString::Printf(TEXT("Class '%s' is deprecated: %s"), UTF8_TO_TCHAR(Py_TYPE(InSelf)->tp_name), *DeprecationMessage)) == -1
						)
					{
						// -1 from SetPythonWarning means the warning should be an exception
						return -1;
					}
				}

				if (ObjClass->HasAnyClassFlags(CLASS_Abstract))
				{
					PyUtil::SetPythonError(PyExc_Exception, InSelf, *FString::Printf(TEXT("Class '%s' is abstract"), UTF8_TO_TCHAR(Py_TYPE(InSelf)->tp_name)));
					return -1;
				}
				else
				{
					InitValue = NewObject<UObject>(ObjectOuter, ObjClass, ObjectName);
				}
			}
			else
			{
				PyUtil::SetPythonError(PyExc_Exception, InSelf, TEXT("Class is null"));
				return -1;
			}

			// Do we have an object instance to wrap?
			if (!InitValue)
			{
				PyUtil::SetPythonError(PyExc_Exception, InSelf, TEXT("Object instance was null during init"));
				return -1;
			}

			return FPyWrapperObject::Init(InSelf, InitValue);
		}

		static PyObject* Str(FPyWrapperObject* InSelf)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			return PyUnicode_FromFormat("<Object '%s' (%p) Class '%s'>", TCHAR_TO_UTF8(*InSelf->ObjectInstance->GetPathName()), InSelf->ObjectInstance, TCHAR_TO_UTF8(*InSelf->ObjectInstance->GetClass()->GetName()));
		}

		static PyUtil::FPyHashType Hash(FPyWrapperObject* InSelf)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return -1;
			}

			const PyUtil::FPyHashType PyHash = (PyUtil::FPyHashType)GetTypeHash(InSelf->ObjectInstance);
			return PyHash != -1 ? PyHash : 0;
		}
	};

	struct FMethods
	{
		static PyObject* PostInit(FPyWrapperObject* InSelf)
		{
			Py_RETURN_NONE;
		}

		static PyObject* Cast(PyTypeObject* InType, PyObject* InArgs)
		{
			PyObject* PyObj = nullptr;
			if (PyArg_ParseTuple(InArgs, "O:cast", &PyObj))
			{
				PyObject* PyCastResult = (PyObject*)FPyWrapperObject::CastPyObject(PyObj, InType);
				if (!PyCastResult)
				{
					PyUtil::SetPythonError(PyExc_TypeError, InType, *FString::Printf(TEXT("Cannot cast type '%s' to '%s'"), *PyUtil::GetFriendlyTypename(PyObj), *PyUtil::GetFriendlyTypename(InType)));
				}
				return PyCastResult;
			}

			return nullptr;
		}

		static PyObject* GetDefaultObject(PyTypeObject* InType)
		{
			UClass* Class = FPyWrapperObjectMetaData::GetClass(InType);
			UObject* CDO = Class ? Class->GetDefaultObject() : nullptr;
			return PyConversion::Pythonize(CDO);
		}

		static PyObject* StaticClass(PyTypeObject* InType)
		{
			UClass* Class = FPyWrapperObjectMetaData::GetClass(InType);
			return PyConversion::PythonizeClass(Class);
		}

		static PyObject* GetClass(FPyWrapperObject* InSelf)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			return PyConversion::PythonizeClass(InSelf->ObjectInstance->GetClass());
		}

		static PyObject* GetOuter(FPyWrapperObject* InSelf)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			return PyConversion::Pythonize(InSelf->ObjectInstance->GetOuter());
		}

		static PyObject* GetTypedOuter(FPyWrapperObject* InSelf, PyObject* InArgs)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			PyObject* PyOuterType = nullptr;
			if (!PyArg_ParseTuple(InArgs, "O:get_typed_outer", &PyOuterType))
			{
				return nullptr;
			}

			UClass* OuterType = nullptr;
			if (!PyConversion::NativizeClass(PyOuterType, OuterType, UObject::StaticClass()))
			{
				return nullptr;
			}

			return PyConversion::Pythonize(InSelf->ObjectInstance->GetTypedOuter(OuterType));
		}

		static PyObject* GetOutermost(FPyWrapperObject* InSelf)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			return PyConversion::Pythonize(InSelf->ObjectInstance->GetOutermost());
		}

		static PyObject* GetName(FPyWrapperObject* InSelf)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			return PyConversion::Pythonize(InSelf->ObjectInstance->GetName());
		}

		static PyObject* GetFName(FPyWrapperObject* InSelf)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			return PyConversion::Pythonize(InSelf->ObjectInstance->GetFName());
		}

		static PyObject* GetFullName(FPyWrapperObject* InSelf)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			return PyConversion::Pythonize(InSelf->ObjectInstance->GetFullName());
		}

		static PyObject* GetPathName(FPyWrapperObject* InSelf)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			return PyConversion::Pythonize(InSelf->ObjectInstance->GetPathName());
		}

		static PyObject* GetWorld(FPyWrapperObject* InSelf)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			return PyConversion::Pythonize(InSelf->ObjectInstance->GetWorld());
		}

		static PyObject* Modify(FPyWrapperObject* InSelf, PyObject* InArgs)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			PyObject* PyAlwaysMarkDirty = nullptr;
			if (!PyArg_ParseTuple(InArgs, "|O:modify", &PyAlwaysMarkDirty))
			{
				return nullptr;
			}

			bool bAlwaysMarkDirty = true;
			if (PyAlwaysMarkDirty && !PyConversion::Nativize(PyAlwaysMarkDirty, bAlwaysMarkDirty))
			{
				return nullptr;
			}

			const bool bResult = InSelf->ObjectInstance->Modify(bAlwaysMarkDirty);
			return PyConversion::Pythonize(bResult);
		}

		static PyObject* Rename(FPyWrapperObject* InSelf, PyObject* InArgs, PyObject* InKwds)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
			{
				return nullptr;
			}

			PyObject* PyNameObj = nullptr;
			PyObject* PyOuterObj = nullptr;

			static const char *ArgsKwdList[] = { "name", "outer", nullptr };
			if (!PyArg_ParseTupleAndKeywords(InArgs, InKwds, "|OO:rename", (char**)ArgsKwdList, &PyNameObj, &PyOuterObj))
			{
				return nullptr;
			}

			FName NewName;
			if (PyNameObj && PyNameObj != Py_None && !PyConversion::Nativize(PyNameObj, NewName))
			{
				PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert 'name' (%s) to 'Name'"), *PyUtil::GetFriendlyTypename(InSelf)));
				return nullptr;
			}

			UObject* NewOuter = nullptr;
			if (PyOuterObj && !PyConversion::Nativize(PyOuterObj, NewOuter))
			{
				PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert 'outer' (%s) to 'Object'"), *PyUtil::GetFriendlyTypename(PyOuterObj)));
				return nullptr;
			}

			const bool bResult = InSelf->ObjectInstance->Rename(NewName.IsNone() ? nullptr : *NewName.ToString(), NewOuter);

			return PyConversion::Pythonize(bResult);
		}

		static PyObject* GetEditorProperty(FPyWrapperObject* InSelf, PyObject* InArgs, PyObject* InKwds)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
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

			const UClass* Class = InSelf->ObjectInstance->GetClass();

			const FName ResolvedName = FPyWrapperObjectMetaData::ResolvePropertyName(InSelf, Name);
			const UProperty* ResolvedProp = Class->FindPropertyByName(ResolvedName);
			if (!ResolvedProp)
			{
				PyUtil::SetPythonError(PyExc_Exception, InSelf, *FString::Printf(TEXT("Failed to find property '%s' for attribute '%s' on '%s'"), *ResolvedName.ToString(), *Name.ToString(), *Class->GetName()));
				return nullptr;
			}

			TOptional<FString> PropDeprecationMessage;
			{
				FString PropDeprecationMessageStr;
				if (FPyWrapperObjectMetaData::IsPropertyDeprecated(InSelf, Name, &PropDeprecationMessageStr))
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

			return FPyWrapperObject::GetPropertyValue(InSelf, WrappedPropDef, TCHAR_TO_UTF8(*Name.ToString()));
		}

		static PyObject* SetEditorProperty(FPyWrapperObject* InSelf, PyObject* InArgs, PyObject* InKwds)
		{
			if (!FPyWrapperObject::ValidateInternalState(InSelf))
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

			const UClass* Class = InSelf->ObjectInstance->GetClass();

			const FName ResolvedName = FPyWrapperObjectMetaData::ResolvePropertyName(InSelf, Name);
			const UProperty* ResolvedProp = Class->FindPropertyByName(ResolvedName);
			if (!ResolvedProp)
			{
				PyUtil::SetPythonError(PyExc_Exception, InSelf, *FString::Printf(TEXT("Failed to find property '%s' for attribute '%s' on '%s'"), *ResolvedName.ToString(), *Name.ToString(), *Class->GetName()));
				return nullptr;
			}

			TOptional<FString> PropDeprecationMessage;
			{
				FString PropDeprecationMessageStr;
				if (FPyWrapperObjectMetaData::IsPropertyDeprecated(InSelf, Name, &PropDeprecationMessageStr))
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

			const int Result = FPyWrapperObject::SetPropertyValue(InSelf, PyValueObj, WrappedPropDef, TCHAR_TO_UTF8(*Name.ToString()), /*InNotifyChange*/true, CPF_EditConst);
			if (Result != 0)
			{
				return nullptr;
			}

			Py_RETURN_NONE;
		}
	};

	static PyMethodDef PyMethods[] = {
		{ PyGenUtil::PostInitFuncName, PyCFunctionCast(&FMethods::PostInit), METH_NOARGS, "x._post_init() -> None -- called during Unreal object initialization (equivalent to PostInitProperties in C++)" },
		{ "cast", PyCFunctionCast(&FMethods::Cast), METH_VARARGS | METH_CLASS, "X.cast(object) -> Object -- cast the given object to this Unreal object type" },
		{ "get_default_object", PyCFunctionCast(&FMethods::GetDefaultObject), METH_NOARGS | METH_CLASS, "X.get_default_object() -> Object -- get the Unreal class default object (CDO) of this type" },
		{ "static_class", PyCFunctionCast(&FMethods::StaticClass), METH_NOARGS | METH_CLASS, "X.static_class() -> Class -- get the Unreal class of this type" },
		{ "get_class", PyCFunctionCast(&FMethods::GetClass), METH_NOARGS, "x.get_class() -> Class -- get the Unreal class of this instance" },
		{ "get_outer", PyCFunctionCast(&FMethods::GetOuter), METH_NOARGS, "x.get_outer() -> Object -- get the outer object from this instance (if any)" },
		{ "get_typed_outer", PyCFunctionCast(&FMethods::GetTypedOuter), METH_VARARGS, "x.get_typed_outer(type) -> type() -- get the first outer object of the given type from this instance (if any)" },
		{ "get_outermost", PyCFunctionCast(&FMethods::GetOutermost), METH_NOARGS, "x.get_outermost() -> Package -- get the outermost object (the package) from this instance" },
		{ "get_name", PyCFunctionCast(&FMethods::GetName), METH_NOARGS, "x.get_name() -> str -- get the name of this instance" },
		{ "get_fname", PyCFunctionCast(&FMethods::GetFName), METH_NOARGS, "x.get_fname() -> FName -- get the name of this instance" },
		{ "get_full_name", PyCFunctionCast(&FMethods::GetFullName), METH_NOARGS, "x.get_full_name() -> str -- get the full name (class name + full path) of this instance" },
		{ "get_path_name", PyCFunctionCast(&FMethods::GetPathName), METH_NOARGS, "x.get_path_name() -> str -- get the path name of this instance" },
		{ "get_world", PyCFunctionCast(&FMethods::GetWorld), METH_NOARGS, "x.get_world() -> World -- get the world associated with this instance (if any)" },
		{ "modify", PyCFunctionCast(&FMethods::Modify), METH_VARARGS, "x.modify(bool) -> bool -- inform that this instance is about to be modified (tracks changes for undo/redo if transactional)" },
		{ "rename", PyCFunctionCast(&FMethods::Rename), METH_VARARGS | METH_KEYWORDS, "x.rename(name=None, outer=None) -> bool -- rename this instance" },
		{ "get_editor_property", PyCFunctionCast(&FMethods::GetEditorProperty), METH_VARARGS | METH_KEYWORDS, "x.get_editor_property(name) -> object -- get the value of any property visible to the editor" },
		{ "set_editor_property", PyCFunctionCast(&FMethods::SetEditorProperty), METH_VARARGS | METH_KEYWORDS, "x.set_editor_property(name, value) -> None -- set the value of any property visible to the editor, ensuring that the pre/post change notifications are called" },
		{ nullptr, nullptr, 0, nullptr }
	};

	PyTypeObject PyType = {
		PyVarObject_HEAD_INIT(nullptr, 0)
		"_ObjectBase", /* tp_name */
		sizeof(FPyWrapperObject), /* tp_basicsize */
	};

	PyType.tp_base = &PyWrapperBaseType;
	PyType.tp_new = (newfunc)&FFuncs::New;
	PyType.tp_dealloc = (destructor)&FFuncs::Dealloc;
	PyType.tp_init = (initproc)&FFuncs::Init;
	PyType.tp_str = (reprfunc)&FFuncs::Str;
	PyType.tp_hash = (hashfunc)&FFuncs::Hash;

	PyType.tp_methods = PyMethods;

	PyType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
	PyType.tp_doc = "Type for all UE4 exposed object instances";

	return PyType;
}

PyTypeObject PyWrapperObjectType = InitializePyWrapperObjectType();

void FPyWrapperObjectMetaData::AddReferencedObjects(FPyWrapperBase* Instance, FReferenceCollector& Collector)
{
	FPyWrapperObject* Self = static_cast<FPyWrapperObject*>(Instance);

	UObject* OldInstance = Self->ObjectInstance;
	Collector.AddReferencedObject(Self->ObjectInstance);
	if (Self->ObjectInstance != OldInstance && Self->ObjectInstance != nullptr)
	{
		// Object instance has been re-pointed, make sure we're still the correct type
		PyTypeObject* NewPyType = FPyWrapperTypeRegistry::Get().GetWrappedClassType(Self->ObjectInstance->GetClass());
		if (PyType_IsSubtype(NewPyType, &PyWrapperObjectType) && NewPyType->tp_basicsize == Py_TYPE(Self)->tp_basicsize)
		{
			Py_TYPE(Self) = NewPyType; // todo: is this safe?
		}
		else
		{
			Self->ObjectInstance = nullptr;
		}
	}

	// We also need to ARO delegates on this object to catch ones that are wrapping Python callables (also recursing into nested structs and containers)
	if (Self->ObjectInstance)
	{
		FPyReferenceCollector::AddReferencedObjectsFromStruct(Collector, Self->ObjectInstance->GetClass(), Self->ObjectInstance, EPyReferenceCollectorFlags::IncludeDelegates | EPyReferenceCollectorFlags::IncludeStructs | EPyReferenceCollectorFlags::IncludeContainers);
	}
}

UClass* FPyWrapperObjectMetaData::GetClass(PyTypeObject* PyType)
{
	FPyWrapperObjectMetaData* PyWrapperMetaData = FPyWrapperObjectMetaData::GetMetaData(PyType);
	return PyWrapperMetaData ? PyWrapperMetaData->Class : nullptr;
}

UClass* FPyWrapperObjectMetaData::GetClass(FPyWrapperObject* Instance)
{
	return GetClass(Py_TYPE(Instance));
}

FName FPyWrapperObjectMetaData::ResolvePropertyName(PyTypeObject* PyType, const FName InPythonPropertyName)
{
	if (FPyWrapperObjectMetaData* PyWrapperMetaData = FPyWrapperObjectMetaData::GetMetaData(PyType))
	{
		if (const FName* MappedPropName = PyWrapperMetaData->PythonProperties.Find(InPythonPropertyName))
		{
			return *MappedPropName;
		}

		if (const UClass* SuperClass = PyWrapperMetaData->Class ? PyWrapperMetaData->Class->GetSuperClass() : nullptr)
		{
			PyTypeObject* SuperClassPyType = FPyWrapperTypeRegistry::Get().GetWrappedClassType(SuperClass);
			return ResolvePropertyName(SuperClassPyType, InPythonPropertyName);
		}
	}

	return InPythonPropertyName;
}

FName FPyWrapperObjectMetaData::ResolvePropertyName(FPyWrapperObject* Instance, const FName InPythonPropertyName)
{
	return ResolvePropertyName(Py_TYPE(Instance), InPythonPropertyName);
}

bool FPyWrapperObjectMetaData::IsPropertyDeprecated(PyTypeObject* PyType, const FName InPythonPropertyName, FString* OutDeprecationMessage)
{
	if (FPyWrapperObjectMetaData* PyWrapperMetaData = FPyWrapperObjectMetaData::GetMetaData(PyType))
	{
		if (const FString* DeprecationMessage = PyWrapperMetaData->PythonDeprecatedProperties.Find(InPythonPropertyName))
		{
			if (OutDeprecationMessage)
			{
				*OutDeprecationMessage = *DeprecationMessage;
			}
			return true;
		}

		if (const UClass* SuperClass = PyWrapperMetaData->Class ? PyWrapperMetaData->Class->GetSuperClass() : nullptr)
		{
			PyTypeObject* SuperClassPyType = FPyWrapperTypeRegistry::Get().GetWrappedClassType(SuperClass);
			return IsPropertyDeprecated(SuperClassPyType, InPythonPropertyName, OutDeprecationMessage);
		}
	}

	return false;
}

bool FPyWrapperObjectMetaData::IsPropertyDeprecated(FPyWrapperObject* Instance, const FName InPythonPropertyName, FString* OutDeprecationMessage)
{
	return IsPropertyDeprecated(Py_TYPE(Instance), InPythonPropertyName, OutDeprecationMessage);
}

FName FPyWrapperObjectMetaData::ResolveFunctionName(PyTypeObject* PyType, const FName InPythonMethodName)
{
	if (FPyWrapperObjectMetaData* PyWrapperMetaData = FPyWrapperObjectMetaData::GetMetaData(PyType))
	{
		if (const FName* MappedFuncName = PyWrapperMetaData->PythonMethods.Find(InPythonMethodName))
		{
			return *MappedFuncName;
		}

		if (const UClass* SuperClass = PyWrapperMetaData->Class ? PyWrapperMetaData->Class->GetSuperClass() : nullptr)
		{
			PyTypeObject* SuperClassPyType = FPyWrapperTypeRegistry::Get().GetWrappedClassType(SuperClass);
			return ResolveFunctionName(SuperClassPyType, InPythonMethodName);
		}
	}

	return InPythonMethodName;
}

FName FPyWrapperObjectMetaData::ResolveFunctionName(FPyWrapperObject* Instance, const FName InPythonMethodName)
{
	return ResolveFunctionName(Py_TYPE(Instance), InPythonMethodName);
}

bool FPyWrapperObjectMetaData::IsFunctionDeprecated(PyTypeObject* PyType, const FName InPythonMethodName, FString* OutDeprecationMessage)
{
	if (FPyWrapperObjectMetaData* PyWrapperMetaData = FPyWrapperObjectMetaData::GetMetaData(PyType))
	{
		if (const FString* DeprecationMessage = PyWrapperMetaData->PythonDeprecatedMethods.Find(InPythonMethodName))
		{
			if (OutDeprecationMessage)
			{
				*OutDeprecationMessage = *DeprecationMessage;
			}
			return true;
		}

		if (const UClass* SuperClass = PyWrapperMetaData->Class ? PyWrapperMetaData->Class->GetSuperClass() : nullptr)
		{
			PyTypeObject* SuperClassPyType = FPyWrapperTypeRegistry::Get().GetWrappedClassType(SuperClass);
			return IsFunctionDeprecated(SuperClassPyType, InPythonMethodName, OutDeprecationMessage);
		}
	}

	return false;
}

bool FPyWrapperObjectMetaData::IsFunctionDeprecated(FPyWrapperObject* Instance, const FName InPythonMethodName, FString* OutDeprecationMessage)
{
	return IsFunctionDeprecated(Py_TYPE(Instance), InPythonMethodName, OutDeprecationMessage);
}

bool FPyWrapperObjectMetaData::IsClassDeprecated(PyTypeObject* PyType, FString* OutDeprecationMessage)
{
	if (FPyWrapperObjectMetaData* PyWrapperMetaData = FPyWrapperObjectMetaData::GetMetaData(PyType))
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

bool FPyWrapperObjectMetaData::IsClassDeprecated(FPyWrapperObject* Instance, FString* OutDeprecationMessage)
{
	return IsClassDeprecated(Py_TYPE(Instance), OutDeprecationMessage);
}

struct FPythonGeneratedClassUtil
{
	static void PrepareOldClassForReinstancing(UPythonGeneratedClass* InOldClass)
	{
		const FString OldClassName = MakeUniqueObjectName(InOldClass->GetOuter(), InOldClass->GetClass(), *FString::Printf(TEXT("%s_REINST"), *InOldClass->GetName())).ToString();
		InOldClass->ClassFlags |= CLASS_NewerVersionExists;
		InOldClass->SetFlags(RF_NewerVersionExists);
		InOldClass->ClearFlags(RF_Public | RF_Standalone);
		InOldClass->Rename(*OldClassName, nullptr, REN_DontCreateRedirectors);
	}

	static UPythonGeneratedClass* CreateClass(const FString& InClassName, UObject* InClassOuter, UClass* InSuperClass)
	{
		UPythonGeneratedClass* Class = NewObject<UPythonGeneratedClass>(InClassOuter, *InClassName, RF_Public | RF_Standalone);
		Class->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
		Class->SetSuperStruct(InSuperClass);
		return Class;
	}

	static void FinalizeClass(UPythonGeneratedClass* InClass, PyTypeObject* InPyType)
	{
		// Finalize the class
		InClass->Bind();
		InClass->StaticLink(true);
		InClass->AssembleReferenceTokenStream();

		// Add the object meta-data to the type
		InClass->PyMetaData.Class = InClass;
		FPyWrapperObjectMetaData::SetMetaData(InPyType, &InClass->PyMetaData);

		// Map the Unreal class to the Python type
		InClass->PyType = FPyTypeObjectPtr::NewReference(InPyType);
		FPyWrapperTypeRegistry::Get().RegisterWrappedClassType(InClass->GetFName(), InPyType);
	}

	static bool CreatePropertyFromDefinition(UPythonGeneratedClass* InClass, PyTypeObject* InPyType, const FString& InFieldName, FPyUPropertyDef* InPyPropDef)
	{
		UClass* SuperClass = InClass->GetSuperClass();

		// Resolve the property name to match any previously exported properties from the parent type
		const FName PropName = FPyWrapperObjectMetaData::ResolvePropertyName(InPyType->tp_base, *InFieldName);
		if (SuperClass->FindPropertyByName(PropName))
		{
			PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Property '%s' (%s) cannot override a property from the base type"), *InFieldName, *PyUtil::GetFriendlyTypename(InPyPropDef->PropType)));
			return false;
		}

		// Create the property from its definition
		UProperty* Prop = PyUtil::CreateProperty(InPyPropDef->PropType, 1, InClass, PropName);
		if (!Prop)
		{
			PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to create property for '%s' (%s)"), *InFieldName, *PyUtil::GetFriendlyTypename(InPyPropDef->PropType)));
			return false;
		}
		Prop->PropertyFlags |= (CPF_Edit | CPF_BlueprintVisible);
		FPyUPropertyDef::ApplyMetaData(InPyPropDef, Prop);
		InClass->AddCppProperty(Prop);

		// Resolve any getter/setter function names
		const FName GetterFuncName = FPyWrapperObjectMetaData::ResolveFunctionName(InPyType->tp_base, *InPyPropDef->GetterFuncName);
		const FName SetterFuncName = FPyWrapperObjectMetaData::ResolveFunctionName(InPyType->tp_base, *InPyPropDef->SetterFuncName);
		if (!GetterFuncName.IsNone())
		{
			Prop->SetMetaData(PyGenUtil::BlueprintGetterMetaDataKey, *GetterFuncName.ToString());
		}
		if (!SetterFuncName.IsNone())
		{
			Prop->SetMetaData(PyGenUtil::BlueprintSetterMetaDataKey, *SetterFuncName.ToString());
		}

		// Build the definition data for the new property accessor
		PyGenUtil::FPropertyDef& PropDef = *InClass->PropertyDefs.Add_GetRef(MakeShared<PyGenUtil::FPropertyDef>());
		PropDef.GeneratedWrappedGetSet.GetSetName = PyGenUtil::TCHARToUTF8Buffer(*InFieldName);
		PropDef.GeneratedWrappedGetSet.GetSetDoc = PyGenUtil::TCHARToUTF8Buffer(*FString::Printf(TEXT("type: %s\n%s"), *PyGenUtil::GetPropertyPythonType(Prop), *PyGenUtil::GetFieldTooltip(Prop)));
		PropDef.GeneratedWrappedGetSet.Prop.SetProperty(Prop);
		PropDef.GeneratedWrappedGetSet.GetFunc.SetFunction(InClass->FindFunctionByName(GetterFuncName));
		PropDef.GeneratedWrappedGetSet.SetFunc.SetFunction(InClass->FindFunctionByName(SetterFuncName));
		PropDef.GeneratedWrappedGetSet.GetCallback = (getter)&FPyWrapperObject::Getter_Impl;
		PropDef.GeneratedWrappedGetSet.SetCallback = (setter)&FPyWrapperObject::Setter_Impl;
		PropDef.GeneratedWrappedGetSet.ToPython(PropDef.PyGetSet);

		// If this property has a getter or setter, also make an internal version with the get/set function cleared so that Python can read/write the internal property value
		if (PropDef.GeneratedWrappedGetSet.GetFunc.Func || PropDef.GeneratedWrappedGetSet.SetFunc.Func)
		{
			PyGenUtil::FPropertyDef& InternalPropDef = *InClass->PropertyDefs.Add_GetRef(MakeShared<PyGenUtil::FPropertyDef>());
			InternalPropDef.GeneratedWrappedGetSet.GetSetName = PyGenUtil::TCHARToUTF8Buffer(*FString::Printf(TEXT("_%s"), *InFieldName));
			InternalPropDef.GeneratedWrappedGetSet.GetSetDoc = PropDef.GeneratedWrappedGetSet.GetSetDoc;
			InternalPropDef.GeneratedWrappedGetSet.Prop.SetProperty(Prop);
			InternalPropDef.GeneratedWrappedGetSet.GetCallback = (getter)&FPyWrapperObject::Getter_Impl;
			InternalPropDef.GeneratedWrappedGetSet.SetCallback = (setter)&FPyWrapperObject::Setter_Impl;
			InternalPropDef.GeneratedWrappedGetSet.ToPython(InternalPropDef.PyGetSet);
		}

		return true;
	}

	static bool CreateFunctionFromDefinition(UPythonGeneratedClass* InClass, PyTypeObject* InPyType, const FString& InFieldName, FPyUFunctionDef* InPyFuncDef)
	{
		UClass* SuperClass = InClass->GetSuperClass();

		// Validate the function definition makes sense
		if (EnumHasAllFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Override))
		{
			if (EnumHasAnyFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Static | EPyUFunctionDefFlags::Getter | EPyUFunctionDefFlags::Setter))
			{
				PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Method '%s' specified as 'override' cannot also specify 'static', 'getter', or 'setter'"), *InFieldName));
				return false;
			}
			if (InPyFuncDef->FuncRetType != Py_None || InPyFuncDef->FuncParamTypes != Py_None)
			{
				PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Method '%s' specified as 'override' cannot also specify 'ret' or 'params'"), *InFieldName));
				return false;
			}
		}
		if (EnumHasAllFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Static) && EnumHasAnyFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Getter | EPyUFunctionDefFlags::Setter))
		{
			PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Method '%s' specified as 'static' cannot also specify 'getter' or 'setter'"), *InFieldName));
			return false;
		}
		if (EnumHasAllFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Getter))
		{
			if (EnumHasAnyFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Setter))
			{
				PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Method '%s' specified as 'getter' cannot also specify 'setter'"), *InFieldName));
				return false;
			}
			if (EnumHasAnyFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Impure))
			{
				PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Method '%s' specified as 'getter' must also specify 'pure=True'"), *InFieldName));
				return false;
			}
		}

		// Resolve the function name to match any previously exported functions from the parent type
		const FName FuncName = FPyWrapperObjectMetaData::ResolveFunctionName(InPyType->tp_base, *InFieldName);
		const UFunction* SuperFunc = SuperClass->FindFunctionByName(FuncName);
		if (SuperFunc && !EnumHasAllFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Override))
		{
			PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Method '%s' cannot override a method from the base type (did you forget to specify 'override=True'?)"), *InFieldName));
			return false;
		}
		if (EnumHasAllFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Override))
		{
			if (!SuperFunc)
			{
				PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Method '%s' was set to 'override', but no method was found to override"), *InFieldName));
				return false;
			}
			if (!SuperFunc->HasAnyFunctionFlags(FUNC_BlueprintEvent))
			{
				PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Method '%s' was set to 'override', but the method found to override was not a blueprint event"), *InFieldName));
				return false;
			}
		}

		// Inspect the argument names and defaults from the Python function
		TArray<FString> FuncArgNames;
		TArray<FPyObjectPtr> FuncArgDefaults;
		if (!PyUtil::InspectFunctionArgs(InPyFuncDef->Func, FuncArgNames, &FuncArgDefaults))
		{
			PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to inspect the arguments for '%s'"), *InFieldName));
			return false;
		}

		// Create the function, either from the definition, or from the super-function found to override
		InClass->AddNativeFunction(*FuncName.ToString(), &UPythonGeneratedClass::CallPythonFunction); // Need to do this before the call to DuplicateObject in the case that the super-function already has FUNC_Native
		UFunction* Func = SuperFunc ? DuplicateObject<UFunction>(SuperFunc, InClass, FuncName) : NewObject<UFunction>(InClass, FuncName);
		if (!SuperFunc)
		{
			Func->FunctionFlags |= FUNC_Public;
		}
		if (EnumHasAllFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Static))
		{
			Func->FunctionFlags |= FUNC_Static;
		}
		if (EnumHasAllFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Pure))
		{
			Func->FunctionFlags |= FUNC_BlueprintPure;
		}
		if (EnumHasAllFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Impure))
		{
			Func->FunctionFlags &= ~FUNC_BlueprintPure;
		}
		if (EnumHasAllFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Getter))
		{
			Func->SetMetaData(PyGenUtil::BlueprintGetterMetaDataKey, TEXT(""));
		}
		if (EnumHasAllFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Setter))
		{
			Func->SetMetaData(PyGenUtil::BlueprintSetterMetaDataKey, TEXT(""));
		}
		Func->FunctionFlags |= (FUNC_Native | FUNC_Event | FUNC_BlueprintEvent | FUNC_BlueprintCallable);
		FPyUFunctionDef::ApplyMetaData(InPyFuncDef, Func);
		InClass->AddFunctionToFunctionMap(Func, Func->GetFName());
		if (!Func->HasAnyFunctionFlags(FUNC_Static))
		{
			// Strip the zero'th 'self' argument when processing a non-static function
			FuncArgNames.RemoveAt(0, 1, /*bAllowShrinking*/false);
			FuncArgDefaults.RemoveAt(0, 1, /*bAllowShrinking*/false);
		}
		if (!SuperFunc)
		{
			// Make sure the number of function arguments matches the number of argument types specified
			const int32 NumArgTypes = (InPyFuncDef->FuncParamTypes && InPyFuncDef->FuncParamTypes != Py_None) ? PySequence_Size(InPyFuncDef->FuncParamTypes) : 0;
			if (NumArgTypes != FuncArgNames.Num())
			{
				PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Incorrect number of arguments specified for '%s' (expected %d, got %d)"), *InFieldName, NumArgTypes, FuncArgNames.Num()));
				return false;
			}

			// Build the arguments struct if not overriding a function
			if (InPyFuncDef->FuncRetType && InPyFuncDef->FuncRetType != Py_None)
			{
				// If we have a tuple, then we actually want to return a bool but add every type within the tuple as output parameters
				const bool bOptionalReturn = PyTuple_Check(InPyFuncDef->FuncRetType);

				PyObject* RetType = bOptionalReturn ? (PyObject*)&PyBool_Type : InPyFuncDef->FuncRetType;
				UProperty* RetProp = PyUtil::CreateProperty(RetType, 1, Func, TEXT("ReturnValue"));
				if (!RetProp)
				{
					PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to create return property (%s) for function '%s'"), *PyUtil::GetFriendlyTypename(RetType), *InFieldName));
					return false;
				}
				RetProp->PropertyFlags |= (CPF_Parm | CPF_ReturnParm);
				Func->AddCppProperty(RetProp);

				if (bOptionalReturn)
				{
					const int32 NumOutArgs = PyTuple_Size(InPyFuncDef->FuncRetType);
					for (int32 ArgIndex = 0; ArgIndex < NumOutArgs; ++ArgIndex)
					{
						PyObject* ArgTypeObj = PySequence_GetItem(InPyFuncDef->FuncRetType, ArgIndex);
						UProperty* ArgProp = PyUtil::CreateProperty(ArgTypeObj, 1, Func, *FString::Printf(TEXT("OutValue%d"), ArgIndex));
						if (!ArgProp)
						{
							PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to create output property (%s) for function '%s' at index %d"), *PyUtil::GetFriendlyTypename(ArgTypeObj), *InFieldName, ArgIndex));
							return false;
						}
						ArgProp->PropertyFlags |= (CPF_Parm | CPF_OutParm);
						Func->AddCppProperty(ArgProp);
						Func->FunctionFlags |= FUNC_HasOutParms;
					}
				}
			}
			for (int32 ArgIndex = 0; ArgIndex < FuncArgNames.Num(); ++ArgIndex)
			{
				PyObject* ArgTypeObj = PySequence_GetItem(InPyFuncDef->FuncParamTypes, ArgIndex);
				UProperty* ArgProp = PyUtil::CreateProperty(ArgTypeObj, 1, Func, *FuncArgNames[ArgIndex]);
				if (!ArgProp)
				{
					PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to create property (%s) for function '%s' argument '%s'"), *PyUtil::GetFriendlyTypename(ArgTypeObj), *InFieldName, *FuncArgNames[ArgIndex]));
					return false;
				}
				ArgProp->PropertyFlags |= CPF_Parm;
				Func->AddCppProperty(ArgProp);
			}
		}
		// Apply the defaults to the function arguments and build the Python method params
		PyGenUtil::FGeneratedWrappedFunction GeneratedWrappedFunction;
		GeneratedWrappedFunction.SetFunction(Func);
		// SetFunction doesn't always use the correct names or defaults for generated classes
		for (int32 InputArgIndex = 0; InputArgIndex < GeneratedWrappedFunction.InputParams.Num(); ++InputArgIndex)
		{
			PyGenUtil::FGeneratedWrappedMethodParameter& GeneratedWrappedMethodParam = GeneratedWrappedFunction.InputParams[InputArgIndex];
			const UProperty* Param = GeneratedWrappedMethodParam.ParamProp;

			const FName DefaultValueMetaDataKey = *FString::Printf(TEXT("CPP_Default_%s"), *Param->GetName());

			TOptional<FString> ResolvedDefaultValue;
			if (FuncArgDefaults.IsValidIndex(InputArgIndex) && FuncArgDefaults[InputArgIndex])
			{
				// Convert the default value to the given property...
				PyUtil::FPropValueOnScope DefaultValue(Param);
				if (!DefaultValue.IsValid() || !DefaultValue.SetValue(FuncArgDefaults[InputArgIndex], *PyUtil::GetErrorContext(InPyType)))
				{
					PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to convert default value for function '%s' argument '%s' (%s)"), *InFieldName, *FuncArgNames[InputArgIndex], *Param->GetClass()->GetName()));
					return false;
				}

				// ... and export it as meta-data
				FString ExportedDefaultValue;
				if (!DefaultValue.GetProp()->ExportText_Direct(ExportedDefaultValue, DefaultValue.GetValue(), DefaultValue.GetValue(), nullptr, PPF_None))
				{
					PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to export default value for function '%s' argument '%s' (%s)"), *InFieldName, *FuncArgNames[InputArgIndex], *Param->GetClass()->GetName()));
					return false;
				}

				ResolvedDefaultValue = ExportedDefaultValue;
			}
			if (!ResolvedDefaultValue.IsSet() && SuperFunc && SuperFunc->HasAnyFunctionFlags(FUNC_HasDefaults))
			{
				if (SuperFunc->HasMetaData(DefaultValueMetaDataKey))
				{
					ResolvedDefaultValue = SuperFunc->GetMetaData(DefaultValueMetaDataKey);
				}
			}
			if (ResolvedDefaultValue.IsSet())
			{
				Func->SetMetaData(DefaultValueMetaDataKey, *ResolvedDefaultValue.GetValue());
				Func->FunctionFlags |= FUNC_HasDefaults;
			}

			GeneratedWrappedMethodParam.ParamName = PyGenUtil::TCHARToUTF8Buffer(FuncArgNames.IsValidIndex(InputArgIndex) ? *FuncArgNames[InputArgIndex] : *Param->GetName());
			GeneratedWrappedMethodParam.ParamDefaultValue = MoveTemp(ResolvedDefaultValue);
		}
		Func->Bind();
		Func->StaticLink(true);

		if (GeneratedWrappedFunction.InputParams.Num() != FuncArgNames.Num())
		{
			PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Incorrect number of arguments specified for '%s' (expected %d, got %d)"), *InFieldName, GeneratedWrappedFunction.InputParams.Num(), FuncArgNames.Num()));
			return false;
		}

		// Apply the doc string as the function tooltip
		{
			static const FName ToolTipKey = TEXT("ToolTip");

			const FString DocString = PyUtil::GetDocString(InPyFuncDef->Func);
			if (!DocString.IsEmpty())
			{
				Func->SetMetaData(ToolTipKey, *DocString);
			}
		}

		// Build the definition data for the new method
		PyGenUtil::FFunctionDef& FuncDef = *InClass->FunctionDefs.Add_GetRef(MakeShared<PyGenUtil::FFunctionDef>());
		FuncDef.GeneratedWrappedMethod.MethodName = PyGenUtil::TCHARToUTF8Buffer(*InFieldName);
		FuncDef.GeneratedWrappedMethod.MethodDoc = PyGenUtil::TCHARToUTF8Buffer(*PyGenUtil::GetFieldTooltip(Func));
		FuncDef.GeneratedWrappedMethod.MethodFunc = MoveTemp(GeneratedWrappedFunction);
		FuncDef.GeneratedWrappedMethod.MethodFlags = FuncArgNames.Num() > 0 ? METH_VARARGS | METH_KEYWORDS : METH_NOARGS;
		if (Func->HasAnyFunctionFlags(FUNC_Static))
		{
			FuncDef.GeneratedWrappedMethod.MethodFlags |= METH_CLASS;
			FuncDef.GeneratedWrappedMethod.MethodCallback = FuncArgNames.Num() > 0 ? PyCFunctionWithClosureCast(&FPyWrapperObject::CallClassMethodWithArgs_Impl) : PyCFunctionWithClosureCast(&FPyWrapperObject::CallClassMethodNoArgs_Impl);
		}
		else
		{
			FuncDef.GeneratedWrappedMethod.MethodCallback = FuncArgNames.Num() > 0 ? PyCFunctionWithClosureCast(&FPyWrapperObject::CallMethodWithArgs_Impl) : PyCFunctionWithClosureCast(&FPyWrapperObject::CallMethodNoArgs_Impl);
		}
		FuncDef.GeneratedWrappedMethod.ToPython(FuncDef.PyMethod);
		FuncDef.PyFunction = FPyObjectPtr::NewReference(InPyFuncDef->Func);
		FuncDef.bIsHidden = EnumHasAnyFlags(InPyFuncDef->FuncFlags, EPyUFunctionDefFlags::Getter | EPyUFunctionDefFlags::Setter);

		return true;
	}

	static bool CopyPropertiesFromOldClass(UPythonGeneratedClass* InClass, UPythonGeneratedClass* InOldClass, PyTypeObject* InPyType)
	{
		InClass->PropertyDefs.Reserve(InOldClass->PropertyDefs.Num());
		for (const TSharedPtr<PyGenUtil::FPropertyDef>& OldPropDef : InOldClass->PropertyDefs)
		{
			const UProperty* OldProp = OldPropDef->GeneratedWrappedGetSet.Prop.Prop;
			const UFunction* OldGetter = OldPropDef->GeneratedWrappedGetSet.GetFunc.Func;
			const UFunction* OldSetter = OldPropDef->GeneratedWrappedGetSet.SetFunc.Func;

			UProperty* Prop = DuplicateObject<UProperty>(OldProp, InClass, OldProp->GetFName());
			if (!Prop)
			{
				PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to duplicate property for '%s'"), UTF8_TO_TCHAR(OldPropDef->PyGetSet.name)));
				return false;
			}

			UMetaData::CopyMetadata((UProperty*)OldProp, Prop);
			InClass->AddCppProperty(Prop);

			PyGenUtil::FPropertyDef& PropDef = *InClass->PropertyDefs.Add_GetRef(MakeShared<PyGenUtil::FPropertyDef>());
			PropDef.GeneratedWrappedGetSet = OldPropDef->GeneratedWrappedGetSet;
			PropDef.GeneratedWrappedGetSet.Prop.SetProperty(Prop);
			if (OldGetter)
			{
				PropDef.GeneratedWrappedGetSet.GetFunc.SetFunction(InClass->FindFunctionByName(OldGetter->GetFName()));
			}
			if (OldSetter)
			{
				PropDef.GeneratedWrappedGetSet.SetFunc.SetFunction(InClass->FindFunctionByName(OldSetter->GetFName()));
			}
			PropDef.GeneratedWrappedGetSet.ToPython(PropDef.PyGetSet);
		}

		return true;
	}

	static bool CopyFunctionsFromOldClass(UPythonGeneratedClass* InClass, UPythonGeneratedClass* InOldClass, PyTypeObject* InPyType)
	{
		InClass->FunctionDefs.Reserve(InOldClass->FunctionDefs.Num());
		for (const TSharedPtr<PyGenUtil::FFunctionDef>& OldFuncDef : InOldClass->FunctionDefs)
		{
			const UFunction* OldFunc = OldFuncDef->GeneratedWrappedMethod.MethodFunc.Func;

			InClass->AddNativeFunction(*OldFunc->GetName(), &UPythonGeneratedClass::CallPythonFunction);
			UFunction* Func = DuplicateObject<UFunction>(OldFunc, InClass, OldFunc->GetFName());
			if (!Func)
			{
				PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to duplicate function for '%s'"), UTF8_TO_TCHAR(OldFuncDef->PyMethod.MethodName)));
				return false;
			}

			UMetaData::CopyMetadata((UFunction*)OldFunc, Func);
			InClass->AddFunctionToFunctionMap(Func, Func->GetFName());

			Func->Bind();
			Func->StaticLink(true);

			PyGenUtil::FFunctionDef& FuncDef = *InClass->FunctionDefs.Add_GetRef(MakeShared<PyGenUtil::FFunctionDef>());
			FuncDef.GeneratedWrappedMethod = OldFuncDef->GeneratedWrappedMethod;
			FuncDef.GeneratedWrappedMethod.MethodFunc.SetFunction(Func);
			FuncDef.PyFunction = OldFuncDef->PyFunction;
			FuncDef.bIsHidden = OldFuncDef->bIsHidden;
			FuncDef.GeneratedWrappedMethod.ToPython(FuncDef.PyMethod);
		}

		return true;
	}

	static bool RegisterDescriptors(UPythonGeneratedClass* InClass, PyTypeObject* InPyType)
	{
		for (const TSharedPtr<PyGenUtil::FPropertyDef>& PropDef : InClass->PropertyDefs)
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

		for (const TSharedPtr<PyGenUtil::FFunctionDef>& FuncDef : InClass->FunctionDefs)
		{
			if (FuncDef->bIsHidden)
			{
				PyDict_DelItemString(InPyType->tp_dict, FuncDef->PyMethod.MethodName);
			}
			else
			{
				FPyObjectPtr MethodDesc = FPyObjectPtr::StealReference(FPyMethodWithClosureDef::NewMethodDescriptor(InPyType, &FuncDef->PyMethod));
				if (!MethodDesc)
				{
					PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to create descriptor for '%s'"), UTF8_TO_TCHAR(FuncDef->PyMethod.MethodName)));
					return false;
				}
				if (PyDict_SetItemString(InPyType->tp_dict, FuncDef->PyMethod.MethodName, MethodDesc) != 0)
				{
					PyUtil::SetPythonError(PyExc_Exception, InPyType, *FString::Printf(TEXT("Failed to assign descriptor for '%s'"), UTF8_TO_TCHAR(FuncDef->PyMethod.MethodName)));
					return false;
				}
			}
		}

		return true;
	}

	static void ReparentPythonType(PyTypeObject* InPyType, PyTypeObject* InNewBasePyType)
	{
		auto UpdateTuple = [](PyObject* InTuple, PyTypeObject* InOldType, PyTypeObject* InNewType)
		{
			if (InTuple)
			{
				const int32 TupleSize = PyTuple_Size(InTuple);
				for (int32 TupleIndex = 0; TupleIndex < TupleSize; ++TupleIndex)
				{
					if (PyTuple_GetItem(InTuple, TupleIndex) == (PyObject*)InOldType)
					{
						FPyTypeObjectPtr NewType = FPyTypeObjectPtr::NewReference(InNewType);
						PyTuple_SetItem(InTuple, TupleIndex, (PyObject*)NewType.Release()); // PyTuple_SetItem steals the reference
					}
				}
			}
		};

		UpdateTuple(InPyType->tp_bases, InPyType->tp_base, InNewBasePyType);
		UpdateTuple(InPyType->tp_mro, InPyType->tp_base, InNewBasePyType);
		InPyType->tp_base = InNewBasePyType;
	}
};

void UPythonGeneratedClass::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);

	FPyWrapperTypeRegistry::Get().UnregisterWrappedClassType(OldName, PyType);
	FPyWrapperTypeRegistry::Get().RegisterWrappedClassType(GetFName(), PyType, !HasAnyFlags(RF_NewerVersionExists));
}

void UPythonGeneratedClass::PostInitInstance(UObject* InObj)
{
	Super::PostInitInstance(InObj);

	// Execute Python code within this block
	{
		FPyScopedGIL GIL;

		if (PyPostInitFunction)
		{
			FPyObjectPtr PySelf = FPyObjectPtr::StealReference((PyObject*)FPyWrapperObjectFactory::Get().CreateInstance(InObj));
			if (PySelf && ensureAlways(PySelf->ob_type == PyType))
			{
				FPyObjectPtr PyArgs = FPyObjectPtr::StealReference(PyTuple_New(1));
				PyTuple_SetItem(PyArgs, 0, PySelf.Release()); // SetItem steals the reference

				FPyObjectPtr Result = FPyObjectPtr::StealReference(PyObject_CallObject(PyPostInitFunction, PyArgs));
				if (!Result)
				{
					PyUtil::ReThrowPythonError();
				}
			}
		}
	}
}

UPythonGeneratedClass* UPythonGeneratedClass::GenerateClass(PyTypeObject* InPyType)
{
	UObject* ClassOuter = GetPythonTypeContainer();
	const FString ClassName = PyUtil::GetCleanTypename(InPyType);

	// Get the correct super class from the parent type in Python
	UClass* SuperClass = FPyWrapperObjectMetaData::GetClass(InPyType->tp_base);
	if (!SuperClass)
	{
		PyUtil::SetPythonError(PyExc_Exception, InPyType, TEXT("No super class could be found for this Python type"));
		return nullptr;
	}

	UPythonGeneratedClass* OldClass = FindObject<UPythonGeneratedClass>(ClassOuter, *ClassName);
	if (OldClass)
	{
		FPythonGeneratedClassUtil::PrepareOldClassForReinstancing(OldClass);
	}

	UPythonGeneratedClass* Class = FPythonGeneratedClassUtil::CreateClass(ClassName, ClassOuter, SuperClass);

	// Get the post-init function
	Class->PyPostInitFunction = FPyObjectPtr::StealReference(PyGenUtil::GetPostInitFunc(InPyType));
	if (!Class->PyPostInitFunction)
	{
		return nullptr;
	}

	// Add the functions to this class
	// We have to process these first as properties may reference them as get/set functions
	{
		PyObject* FieldKey = nullptr;
		PyObject* FieldValue = nullptr;
		Py_ssize_t FieldIndex = 0;
		while (PyDict_Next(InPyType->tp_dict, &FieldIndex, &FieldKey, &FieldValue))
		{
			const FString FieldName = PyUtil::PyObjectToUEString(FieldKey);

			if (PyObject_IsInstance(FieldValue, (PyObject*)&PyUValueDefType) == 1)
			{
				// Values are not supported on classes
				PyUtil::SetPythonError(PyExc_Exception, InPyType, TEXT("Classes do not support values"));
				return nullptr;
			}

			if (PyObject_IsInstance(FieldValue, (PyObject*)&PyUFunctionDefType) == 1)
			{
				FPyUFunctionDef* PyFuncDef = (FPyUFunctionDef*)FieldValue;
				if (!FPythonGeneratedClassUtil::CreateFunctionFromDefinition(Class, InPyType, FieldName, PyFuncDef))
				{
					return nullptr;
				}
			}
		}
	}

	// Add the properties to this class
	{
		PyObject* FieldKey = nullptr;
		PyObject* FieldValue = nullptr;
		Py_ssize_t FieldIndex = 0;
		while (PyDict_Next(InPyType->tp_dict, &FieldIndex, &FieldKey, &FieldValue))
		{
			const FString FieldName = PyUtil::PyObjectToUEString(FieldKey);

			if (PyObject_IsInstance(FieldValue, (PyObject*)&PyUPropertyDefType) == 1)
			{
				FPyUPropertyDef* PyPropDef = (FPyUPropertyDef*)FieldValue;
				if (!FPythonGeneratedClassUtil::CreatePropertyFromDefinition(Class, InPyType, FieldName, PyPropDef))
				{
					return nullptr;
				}
			}
		}
	}

	// Replace the definitions with real descriptors
	if (!FPythonGeneratedClassUtil::RegisterDescriptors(Class, InPyType))
	{
		return nullptr;
	}

	// Let Python know that we've changed its type
	PyType_Modified(InPyType);

	// Finalize the class
	FPythonGeneratedClassUtil::FinalizeClass(Class, InPyType);

	// Re-instance the old class and re-parent any derived classes to this new type
	if (OldClass)
	{
		FPyWrapperTypeReinstancer::Get().AddPendingClass(OldClass, Class);
		ReparentDerivedClasses(OldClass, Class);
	}

	return Class;
}

bool UPythonGeneratedClass::ReparentDerivedClasses(UPythonGeneratedClass* InOldParent, UPythonGeneratedClass* InNewParent)
{
	TArray<UClass*> DerivedClasses;
	GetDerivedClasses(InOldParent, DerivedClasses, /*bRecursive*/false);

	bool bSuccess = true;

	for (UClass* DerivedClass : DerivedClasses)
	{
		if (DerivedClass->HasAnyClassFlags(CLASS_Native))
		{
			continue;
		}

		// todo: Blueprint classes?

		if (UPythonGeneratedClass* PyDerivedClass = Cast<UPythonGeneratedClass>(DerivedClass))
		{
			bSuccess &= !!ReparentClass(PyDerivedClass, InNewParent);
		}
	}

	return bSuccess;
}

UPythonGeneratedClass* UPythonGeneratedClass::ReparentClass(UPythonGeneratedClass* InOldClass, UPythonGeneratedClass* InNewParent)
{
	UObject* ClassOuter = GetPythonTypeContainer();
	const FString ClassName = InOldClass->GetName();

	FPythonGeneratedClassUtil::PrepareOldClassForReinstancing(InOldClass);
	UPythonGeneratedClass* Class = FPythonGeneratedClassUtil::CreateClass(ClassName, ClassOuter, InNewParent);
	PyTypeObject* PyType = InOldClass->PyType;

	// Copy the data from the old class
	Class->PyPostInitFunction = InOldClass->PyPostInitFunction;
	if (!FPythonGeneratedClassUtil::CopyFunctionsFromOldClass(Class, InOldClass, PyType))
	{
		return nullptr;
	}
	if (!FPythonGeneratedClassUtil::CopyPropertiesFromOldClass(Class, InOldClass, PyType))
	{
		return nullptr;
	}

	// Update the descriptors on the type so they reference the new class
	if (!FPythonGeneratedClassUtil::RegisterDescriptors(Class, PyType))
	{
		return nullptr;
	}

	// Update the base of the Python type
	FPythonGeneratedClassUtil::ReparentPythonType(PyType, InNewParent->PyType);

	// Let Python know that we've changed its type
	PyType_Modified(PyType);

	// Finalize the class
	FPythonGeneratedClassUtil::FinalizeClass(Class, PyType);

	// Re-instance the old class and re-parent any derived classes to this new type
	FPyWrapperTypeReinstancer::Get().AddPendingClass(InOldClass, Class);
	ReparentDerivedClasses(InOldClass, Class);

	return Class;
}

DEFINE_FUNCTION(UPythonGeneratedClass::CallPythonFunction)
{
	// Get the correct class from the UFunction so that we can perform static dispatch to the correct type
	const UPythonGeneratedClass* This = CastChecked<UPythonGeneratedClass>(Stack.Node->GetOwnerClass());

	// Find the Python function to call
	TSharedPtr<PyGenUtil::FFunctionDef> FuncDef;
	{
		const TSharedPtr<PyGenUtil::FFunctionDef>* FuncDefPtr = This->FunctionDefs.FindByPredicate([&Stack](const TSharedPtr<PyGenUtil::FFunctionDef>& InFuncDef)
		{
			return InFuncDef->GeneratedWrappedMethod.MethodFunc.Func == Stack.Node;
		});
		FuncDef = FuncDefPtr ? *FuncDefPtr : nullptr;
	}
	if (!FuncDef.IsValid())
	{
		UE_LOG(LogPython, Error, TEXT("Failed to find Python function for '%s' on '%s'"), *Stack.Node->GetName(), *This->GetName());
	}

	// Find the Python object to call the function on
	FPyObjectPtr PySelf;
	if (!Stack.Node->HasAnyFunctionFlags(FUNC_Static))
	{
		FPyScopedGIL GIL;
		PySelf = FPyObjectPtr::StealReference((PyObject*)FPyWrapperObjectFactory::Get().CreateInstance(P_THIS_OBJECT));
		if (!PySelf)
		{
			UE_LOG(LogPython, Error, TEXT("Failed to create a Python wrapper for '%s'"), *P_THIS_OBJECT->GetName());
			return;
		}
	}

	auto DoCall = [&]() -> bool
	{
		if (Stack.Node->Children == nullptr)
		{
			// Simple case, no parameters or return value
			FPyObjectPtr PyArgs;
			if (PySelf)
			{
				PyArgs = FPyObjectPtr::StealReference(PyTuple_New(1));
				PyTuple_SetItem(PyArgs, 0, PySelf.Release()); // SetItem steals the reference
			}
			FPyObjectPtr RetVals = FPyObjectPtr::StealReference(PyObject_CallObject(FuncDef->PyFunction, PyArgs));
			if (!RetVals)
			{
				return false;
			}
		}
		else
		{
			// Complex case, parameters or return value
			TArray<FPyObjectPtr, TInlineAllocator<4>> PyParams;

			// Get the value of the input params for the Python args
			{
				int32 ArgIndex = 0;
				for (const PyGenUtil::FGeneratedWrappedMethodParameter& ParamDef : FuncDef->GeneratedWrappedMethod.MethodFunc.InputParams)
				{
					FPyObjectPtr& PyParam = PyParams.AddDefaulted_GetRef();
					if (!PyConversion::PythonizeProperty_InContainer(ParamDef.ParamProp, Stack.Locals, 0, PyParam.Get()))
					{
						PyUtil::SetPythonError(PyExc_TypeError, FuncDef->PyFunction, *FString::Printf(TEXT("Failed to convert argument at pos '%d' when calling function '%s' on '%s'"), ArgIndex + 1, *Stack.Node->GetName(), *P_THIS_OBJECT->GetName()));
						return false;
					}
					++ArgIndex;
				}
			}

			const int32 PyParamOffset = (PySelf ? 1 : 0);
			FPyObjectPtr PyArgs = FPyObjectPtr::StealReference(PyTuple_New(PyParams.Num() + PyParamOffset));
			if (PySelf)
			{
				PyTuple_SetItem(PyArgs, 0, PySelf.Release()); // SetItem steals the reference
			}
			for (int32 PyParamIndex = 0; PyParamIndex < PyParams.Num(); ++PyParamIndex)
			{
				PyTuple_SetItem(PyArgs, PyParamIndex + PyParamOffset, PyParams[PyParamIndex].Release()); // SetItem steals the reference
			}

			FPyObjectPtr RetVals = FPyObjectPtr::StealReference(PyObject_CallObject(FuncDef->PyFunction, PyArgs));
			if (!RetVals)
			{
				return false;
			}

			if (!PyGenUtil::UnpackReturnValues(RetVals, Stack.Locals, FuncDef->GeneratedWrappedMethod.MethodFunc.OutputParams, *PyUtil::GetErrorContext(FuncDef->PyFunction), *FString::Printf(TEXT("function '%s' on '%s'"), *Stack.Node->GetName(), *P_THIS_OBJECT->GetName())))
			{
				return false;
			}

			// Copy the data back out of the function call
			if (const UProperty* ReturnProp = Stack.Node->GetReturnProperty())
			{
				ReturnProp->CopyCompleteValue(RESULT_PARAM, ReturnProp->ContainerPtrToValuePtr<void>(Stack.Locals));
			}
			for (FOutParmRec* OutParamRec = Stack.OutParms; OutParamRec; OutParamRec = OutParamRec->NextOutParm)
			{
				OutParamRec->Property->CopyCompleteValue(OutParamRec->PropAddr, OutParamRec->Property->ContainerPtrToValuePtr<void>(Stack.Locals));
			}
		}

		return true;
	};

	// Execute Python code within this block
	{
		FPyScopedGIL GIL;

		if (!DoCall())
		{
			PyUtil::ReThrowPythonError();
		}
	}
}

#endif	// WITH_PYTHON
