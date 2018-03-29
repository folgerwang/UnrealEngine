// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PyWrapperBase.h"
#include "PyWrapperOwnerContext.h"
#include "PyUtil.h"
#include "PyGenUtil.h"
#include "PyConversion.h"
#include "UObject/Class.h"
#include "Templates/TypeCompatibleBytes.h"
#include "PyWrapperStruct.generated.h"

#if WITH_PYTHON

/** Python type for FPyWrapperStruct */
extern PyTypeObject PyWrapperStructType;

/** Initialize the PyWrapperStruct types and add them to the given Python module */
void InitializePyWrapperStruct(PyObject* PyModule);

/** Type for all UE4 exposed struct instances */
struct FPyWrapperStruct : public FPyWrapperBase
{
	/** The owner of the wrapped struct instance (if any) */
	FPyWrapperOwnerContext OwnerContext;

	/** Struct type of this instance */
	UScriptStruct* ScriptStruct;

	/** Wrapped struct instance */
	void* StructInstance;

	/** New this wrapper instance (called via tp_new for Python, or directly in C++) */
	static FPyWrapperStruct* New(PyTypeObject* InType);

	/** Free this wrapper instance (called via tp_dealloc for Python) */
	static void Free(FPyWrapperStruct* InSelf);

	/** Initialize this wrapper instance (called via tp_init for Python, or directly in C++) */
	static int Init(FPyWrapperStruct* InSelf);

	/** Initialize this wrapper instance to the given value (called via tp_init for Python, or directly in C++) */
	static int Init(FPyWrapperStruct* InSelf, const FPyWrapperOwnerContext& InOwnerContext, UScriptStruct* InStruct, void* InValue, const EPyConversionMethod InConversionMethod);

	/** Deinitialize this wrapper instance (called via Init and Free to restore the instance to its New state) */
	static void Deinit(FPyWrapperStruct* InSelf);

	/** Called to validate the internal state of this wrapper instance prior to operating on it (should be called by all functions that expect to operate on an initialized type; will set an error state on failure) */
	static bool ValidateInternalState(FPyWrapperStruct* InSelf);

	/** Cast the given Python object to this wrapped type (returns a new reference) */
	static FPyWrapperStruct* CastPyObject(PyObject* InPyObject, FPyConversionResult* OutCastResult = nullptr);

	/** Cast the given Python object to this wrapped type, or attempt to convert the type into a new wrapped instance (returns a new reference) */
	static FPyWrapperStruct* CastPyObject(PyObject* InPyObject, PyTypeObject* InType, FPyConversionResult* OutCastResult = nullptr);

	/** Set the named property values on this instance (called via generated code) */
	static int SetPropertyValues(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds);

	/** Get a named property value from this instance (called via generated code) */
	static PyObject* GetPropertyValueByName(FPyWrapperStruct* InSelf, const FName InPropName, const char* InPythonAttrName);

	/** Get a property value from this instance (called via generated code) */
	static PyObject* GetPropertyValue(FPyWrapperStruct* InSelf, const UProperty* InProp, const char* InPythonAttrName);

	/** Set a named property value on this instance (called via generated code) */
	static int SetPropertyValueByName(FPyWrapperStruct* InSelf, PyObject* InValue, const FName InPropName, const char* InPythonAttrName, const bool InNotifyChange = false, const uint64 InReadOnlyFlags = CPF_EditConst | CPF_BlueprintReadOnly);

	/** Set a property value on this instance (called via generated code) */
	static int SetPropertyValue(FPyWrapperStruct* InSelf, PyObject* InValue, const UProperty* InProp, const char* InPythonAttrName, const bool InNotifyChange = false, const uint64 InReadOnlyFlags = CPF_EditConst | CPF_BlueprintReadOnly);

	/** Call a function on this instance (CallFunction internal use only) */
	static PyObject* CallFunction_Impl(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds, const PyGenUtil::FGeneratedWrappedFunction& InFuncDef, const PyGenUtil::FGeneratedWrappedMethodParameter& InStructParam, const char* InPythonFuncName, const TCHAR* InErrorCtxt);

	/** Implementation of the "call" logic for a Python method with no arguments (internal Python bindings use only) */
	static PyObject* CallMethodNoArgs_Impl(FPyWrapperStruct* InSelf, void* InClosure);

	/** Implementation of the "call" logic for a Python method with arguments (internal Python bindings use only) */
	static PyObject* CallMethodWithArgs_Impl(FPyWrapperStruct* InSelf, PyObject* InArgs, PyObject* InKwds, void* InClosure);

	/** Implementation of the "call" logic for a Python numeric binary function (internal CallBinaryOperator use only) */
	static PyObject* CallBinaryOperatorFunction_Impl(FPyWrapperStruct* InSelf, PyObject* InRHS, const PyGenUtil::FGeneratedWrappedStructMathOpFunction& InMathOpFunc, const bool InInlineOp, const TOptional<EPyConversionResultState> InRequiredConversionResult = TOptional<EPyConversionResultState>(), FPyConversionResult* OutRHSConversionResult = nullptr);

	/** Implementation of the "call" logic for a Python numeric binary function (internal Python bindings use only) */
	static PyObject* CallBinaryOperator_Impl(FPyWrapperStruct* InSelf, PyObject* InRHS, const PyGenUtil::FGeneratedWrappedStructMathOpStack::EOpType InOpType);

	/** Implementation of the "getter" logic for a Python descriptor reading from an struct property (internal Python bindings use only) */
	static PyObject* Getter_Impl(FPyWrapperStruct* InSelf, void* InClosure);

	/** Implementation of the "setter" logic for a Python descriptor writing to an struct property (internal Python bindings use only) */
	static int Setter_Impl(FPyWrapperStruct* InSelf, PyObject* InValue, void* InClosure);

	/** Get a pointer to the typed struct instance this wrapper represents */
	template <typename StructType>
	static StructType* GetTypedStructPtr(FPyWrapperStruct* InSelf)
	{
		return (StructType*)InSelf->StructInstance;
	}

	/** Get a reference to the typed struct instance this wrapper represents */
	template <typename StructType>
	static StructType& GetTypedStruct(FPyWrapperStruct* InSelf)
	{
		return *GetTypedStructPtr<StructType>(InSelf);
	}
};

/** Specialized version of FPyWrapperStruct that can store its struct payload inline (requires a known type) */
template <typename InlineType>
struct TPyWrapperInlineStruct : public FPyWrapperStruct
{
	typedef InlineType WrappedType;

	/** Inline struct instance (do not use directly, StructInstance will be set to point to this when appropriate via FPyWrapperStructAllocationPolicy_Inline) */
	TTypeCompatibleBytes<WrappedType> InlineStructInstance;

	/** Cast the given Python object to this wrapped type (returns a new reference) */
	static TPyWrapperInlineStruct* CastPyObject(PyObject* InPyObject, FPyConversionResult* OutCastResult = nullptr)
	{
		return (TPyWrapperInlineStruct*)FPyWrapperStruct::CastPyObject(InPyObject, OutCastResult);
	}

	/** Cast the given Python object to this wrapped type, or attempt to convert the type into a new wrapped instance (returns a new reference) */
	static TPyWrapperInlineStruct* CastPyObject(PyObject* InPyObject, PyTypeObject* InType, FPyConversionResult* OutCastResult = nullptr)
	{
		return (TPyWrapperInlineStruct*)FPyWrapperStruct::CastPyObject(InPyObject, InType, OutCastResult);
	}

	/** Get a pointer to the typed struct instance this wrapper represents */
	static WrappedType* GetTypedStructPtr(TPyWrapperInlineStruct* InSelf)
	{
		return FPyWrapperStruct::GetTypedStructPtr<WrappedType>(InSelf);
	}

	/** Get a reference to the typed struct instance this wrapper represents */
	static WrappedType& GetTypedStruct(TPyWrapperInlineStruct* InSelf)
	{
		return FPyWrapperStruct::GetTypedStruct<WrappedType>(InSelf);
	}

	/** Getter function for intrinsic field access (for use with PyGetSetDef) */
	template <typename FieldType, FieldType WrappedType::*FieldPtr>
	static PyObject* IntrinsicFieldGetter(TPyWrapperInlineStruct* InSelf, void* InClosure)
	{
		return PyConversion::Pythonize(GetTypedStruct(InSelf).*FieldPtr);
	}

	/** Setter function for intrinsic field access (for use with PyGetSetDef) */
	template <typename FieldType, FieldType WrappedType::*FieldPtr>
	static int IntrinsicFieldSetter(TPyWrapperInlineStruct* InSelf, PyObject* InValue, void* InClosure)
	{
		if (!InValue)
		{
			PyUtil::SetPythonError(PyExc_TypeError, InSelf, TEXT("Cannot delete attribute from type"));
			return -1;
		}

		if (!PyConversion::Nativize(InValue, GetTypedStruct(InSelf).*FieldPtr))
		{
			return -1;
		}

		return 0;
	}

	/** Getter function for struct field access (for use with PyGetSetDef) */
	template <typename FieldType, FieldType WrappedType::*FieldPtr>
	static PyObject* StructFieldGetter(TPyWrapperInlineStruct* InSelf, void* InClosure)
	{
		return PyConversion::PythonizeStruct(GetTypedStruct(InSelf).*FieldPtr);
	}

	/** Setter function for struct field access (for use with PyGetSetDef) */
	template <typename FieldType, FieldType WrappedType::*FieldPtr>
	static int StructFieldSetter(TPyWrapperInlineStruct* InSelf, PyObject* InValue, void* InClosure)
	{
		if (!InValue)
		{
			PyUtil::SetPythonError(PyExc_TypeError, InSelf, TEXT("Cannot delete attribute from type"));
			return -1;
		}

		if (!PyConversion::NativizeStruct(InValue, GetTypedStruct(InSelf).*FieldPtr))
		{
			return -1;
		}

		return 0;
	}

	/** Call a function with no parameters and no return value (for use with PyMethodDef) */
	template <void(WrappedType::*FuncPtr)()>
	static PyObject* CallFunc_NoParams_NoReturn(TPyWrapperInlineStruct* InSelf)
	{
		(GetTypedStruct(InSelf).*FuncPtr)();
		Py_RETURN_NONE;
	}

	/** Call a const function with no parameters and no return value (for use with PyMethodDef) */
	template <void(WrappedType::*FuncPtr)() const>
	static PyObject* CallConstFunc_NoParams_NoReturn(TPyWrapperInlineStruct* InSelf)
	{
		(GetTypedStruct(InSelf).*FuncPtr)();
		Py_RETURN_NONE;
	}

	/** Call a function with no parameters and an intrinsic return value (for use with PyMethodDef) */
	template <typename ReturnType, ReturnType(WrappedType::*FuncPtr)()>
	static PyObject* CallFunc_NoParams_IntrinsicReturn(TPyWrapperInlineStruct* InSelf)
	{
		return PyConversion::Pythonize((GetTypedStruct(InSelf).*FuncPtr)());
	}

	/** Call a const function with no parameters and an intrinsic return value (for use with PyMethodDef) */
	template <typename ReturnType, ReturnType(WrappedType::*FuncPtr)() const>
	static PyObject* CallConstFunc_NoParams_IntrinsicReturn(TPyWrapperInlineStruct* InSelf)
	{
		return PyConversion::Pythonize((GetTypedStruct(InSelf).*FuncPtr)());
	}

	/** Call a function with no parameters and a struct return value (for use with PyMethodDef) */
	template <typename ReturnType, ReturnType(WrappedType::*FuncPtr)()>
	static PyObject* CallFunc_NoParams_StructReturn(TPyWrapperInlineStruct* InSelf)
	{
		return PyConversion::PythonizeStruct((GetTypedStruct(InSelf).*FuncPtr)());
	}

	/** Call a const function with no parameters and a struct return value (for use with PyMethodDef) */
	template <typename ReturnType, ReturnType(WrappedType::*FuncPtr)() const>
	static PyObject* CallConstFunc_NoParams_StructReturn(TPyWrapperInlineStruct* InSelf)
	{
		return PyConversion::PythonizeStruct((GetTypedStruct(InSelf).*FuncPtr)());
	}
};

/** Default struct allocation policy interface */
class IPyWrapperStructAllocationPolicy
{
public:
	/** Allocate memory to hold an instance of the given struct and return the result */
	virtual void* AllocateStruct(const FPyWrapperStruct* InSelf, UScriptStruct* InStruct) const = 0;

	/** Free memory previously allocated with AllocateStruct */
	virtual void FreeStruct(const FPyWrapperStruct* InSelf, void* InAlloc) const = 0;
};

/** Meta-data for all UE4 exposed struct types */
struct FPyWrapperStructMetaData : public FPyWrapperBaseMetaData
{
	PY_METADATA_METHODS(FPyWrapperStructMetaData, FGuid(0x03C9EA75, 0x2C86448B, 0xB53D1453, 0x94AA6BAE))

	FPyWrapperStructMetaData();

	/** Get the allocation policy from the given type */
	static const IPyWrapperStructAllocationPolicy* GetAllocationPolicy(PyTypeObject* PyType);

	/** Get the allocation policy from the type of the given instance */
	static const IPyWrapperStructAllocationPolicy* GetAllocationPolicy(FPyWrapperStruct* Instance);

	/** Get the UStruct from the given type */
	static UStruct* GetStruct(PyTypeObject* PyType);

	/** Get the UStruct from the type of the given instance */
	static UStruct* GetStruct(FPyWrapperStruct* Instance);

	/** Resolve the original property name of a Python method from the given type */
	static FName ResolvePropertyName(PyTypeObject* PyType, const FName InPythonPropertyName);

	/** Resolve the original property name of a Python method of the given instance */
	static FName ResolvePropertyName(FPyWrapperStruct* Instance, const FName InPythonPropertyName);

	/** Add object references from the given Python object to the given collector */
	virtual void AddReferencedObjects(FPyWrapperBase* Instance, FReferenceCollector& Collector) override;

	/** Allocation policy used with when allocating struct instances for a wrapped type */
	const IPyWrapperStructAllocationPolicy* AllocPolicy;

	/** Unreal struct */
	UStruct* Struct;

	/** Map of properties that were exposed to Python mapped to their original name */
	TMap<FName, FName> PythonProperties;

	/** Array of properties that were exposed to Python and can be used during init */
	TArray<PyGenUtil::FGeneratedWrappedMethodParameter> InitParams;

	/** The math operator stacks for this struct type */
	PyGenUtil::FGeneratedWrappedStructMathOpStack MathOpStacks[(int32)PyGenUtil::FGeneratedWrappedStructMathOpStack::EOpType::Num];
};

/** Meta-data for all UE4 exposed inline struct types */
template <typename InlineType>
struct TPyWrapperInlineStructMetaData : public FPyWrapperStructMetaData
{
	class FPyWrapperStructAllocationPolicy_Inline : public IPyWrapperStructAllocationPolicy
	{
	public:
		virtual void* AllocateStruct(const FPyWrapperStruct* InSelf, UScriptStruct* InStruct) const override
		{
			return (void*)&static_cast<const TPyWrapperInlineStruct<InlineType>*>(InSelf)->InlineStructInstance;
		}

		virtual void FreeStruct(const FPyWrapperStruct* InSelf, void* InAlloc) const override
		{
		}
	};

	TPyWrapperInlineStructMetaData()
	{
		static const FPyWrapperStructAllocationPolicy_Inline InlineAllocPolicy = FPyWrapperStructAllocationPolicy_Inline();
		AllocPolicy = &InlineAllocPolicy;
	}
};

typedef TPyPtr<FPyWrapperStruct> FPyWrapperStructPtr;

#endif	// WITH_PYTHON

/** An Unreal struct that was generated from a Python type */
UCLASS()
class UPythonGeneratedStruct : public UScriptStruct
{
	GENERATED_BODY()

#if WITH_PYTHON

public:
	//~ UObject interface
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;

	//~ UStruct interface
	virtual void InitializeStruct(void* Dest, int32 ArrayDim = 1) const override;

	/** Generate an Unreal struct from the given Python type */
	static UPythonGeneratedStruct* GenerateStruct(PyTypeObject* InPyType);

private:
	/** Python type this struct was generated from */
	FPyTypeObjectPtr PyType;

	/** PostInit function for this struct */
	FPyObjectPtr PyPostInitFunction;

	/** Array of properties generated for this struct */
	TArray<TSharedPtr<PyGenUtil::FPropertyDef>> PropertyDefs;

	/** Meta-data for this generated struct that is applied to the Python type */
	FPyWrapperStructMetaData PyMetaData;

	friend struct FPythonGeneratedStructUtil;

#endif	// WITH_PYTHON
};
