// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IncludePython.h"
#include "PyPtr.h"
#include "PyMethodWithClosure.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#if WITH_PYTHON

struct FPyWrapperBaseMetaData;

namespace PyGenUtil
{
	extern const FName ScriptNameMetaDataKey;
	extern const FName ScriptNoExportMetaDataKey;
	extern const FName ScriptMethodMetaDataKey;
	extern const FName ScriptMathOpMetaDataKey;
	extern const FName BlueprintTypeMetaDataKey;
	extern const FName NotBlueprintTypeMetaDataKey;
	extern const FName BlueprintSpawnableComponentMetaDataKey;
	extern const FName BlueprintGetterMetaDataKey;
	extern const FName BlueprintSetterMetaDataKey;
	extern const FName CustomStructureParamMetaDataKey;
	extern const FName DeprecatedPropertyMetaDataKey;
	extern const FName DeprecatedFunctionMetaDataKey;
	extern const FName DeprecationMessageMetaDataKey;

	/** Name used by the Python equivalent of PostInitProperties */
	static const char* PostInitFuncName = "_post_init";

	/** Buffer for storing the UTF-8 strings used by Python types */
	typedef TArray<char> FUTF8Buffer;

	/** Stores the data needed by a runtime generated Python parameter */
	struct FGeneratedWrappedMethodParameter
	{
		FGeneratedWrappedMethodParameter()
			: ParamProp(nullptr)
		{
		}

		FGeneratedWrappedMethodParameter(FGeneratedWrappedMethodParameter&&) = default;
		FGeneratedWrappedMethodParameter(const FGeneratedWrappedMethodParameter&) = default;
		FGeneratedWrappedMethodParameter& operator=(FGeneratedWrappedMethodParameter&&) = default;
		FGeneratedWrappedMethodParameter& operator=(const FGeneratedWrappedMethodParameter&) = default;

		/** The name of the parameter */
		FUTF8Buffer ParamName;

		/** The Unreal property for this parameter */
		const UProperty* ParamProp;

		/** The default Unreal ExportText value of this parameter; parameters with this set are considered optional */
		TOptional<FString> ParamDefaultValue;
	};

	/** Stores the data needed to call an Unreal function via Python */
	struct FGeneratedWrappedFunction
	{
		FGeneratedWrappedFunction()
			: Func(nullptr)
		{
		}

		FGeneratedWrappedFunction(FGeneratedWrappedFunction&&) = default;
		FGeneratedWrappedFunction(const FGeneratedWrappedFunction&) = default;
		FGeneratedWrappedFunction& operator=(FGeneratedWrappedFunction&&) = default;
		FGeneratedWrappedFunction& operator=(const FGeneratedWrappedFunction&) = default;

		/** Set the function to call, and also extract the parameter lists */
		void SetFunctionAndExtractParams(const UFunction* InFunc);

		/** The Unreal function to call (static dispatch) */
		const UFunction* Func;

		/** Array of input parameters associated with the function */
		TArray<FGeneratedWrappedMethodParameter> InputParams;

		/** Array of output (including return) parameters associated with the function */
		TArray<FGeneratedWrappedMethodParameter> OutputParams;
	};

	/** Stores the data needed by a runtime generated Python method */
	struct FGeneratedWrappedMethod
	{
		FGeneratedWrappedMethod()
			: MethodCallback(nullptr)
			, MethodFlags(0)
		{
		}

		FGeneratedWrappedMethod(FGeneratedWrappedMethod&&) = default;
		FGeneratedWrappedMethod(const FGeneratedWrappedMethod&) = default;
		FGeneratedWrappedMethod& operator=(FGeneratedWrappedMethod&&) = default;
		FGeneratedWrappedMethod& operator=(const FGeneratedWrappedMethod&) = default;

		/** Convert this wrapper type to its Python type */
		void ToPython(FPyMethodWithClosureDef& OutPyMethod) const;

		/** The name of the method */
		FUTF8Buffer MethodName;

		/** The doc string of the method */
		FUTF8Buffer MethodDoc;

		/** The Unreal function for this method */
		FGeneratedWrappedFunction MethodFunc;

		/* The C function this method should call */
		PyCFunctionWithClosure MethodCallback;

		/* The METH_ flags for this method */
		int MethodFlags;
	};

	/** Stores the data needed for a set of runtime generated Python methods */
	struct FGeneratedWrappedMethods
	{
		FGeneratedWrappedMethods() = default;
		FGeneratedWrappedMethods(FGeneratedWrappedMethods&&) = default;
		FGeneratedWrappedMethods(const FGeneratedWrappedMethods&) = delete;
		FGeneratedWrappedMethods& operator=(FGeneratedWrappedMethods&&) = default;
		FGeneratedWrappedMethods& operator=(const FGeneratedWrappedMethods&) = delete;

		/** Called to ready the generated methods with Python */
		void Finalize();

		/** Array of methods associated from the wrapped type */
		TArray<FGeneratedWrappedMethod> TypeMethods;

		/** The Python methods that were generated from TypeMethods (in Finalize) */
		TArray<FPyMethodWithClosureDef> PyMethods;
	};

	/** Stores the data needed by a runtime generated Python method that is dynamically created and registered post-finalize of its owner struct type (for hoisting util functions onto structs) */
	struct FGeneratedWrappedDynamicStructMethod : public FGeneratedWrappedMethod
	{
		FGeneratedWrappedDynamicStructMethod() = default;
		FGeneratedWrappedDynamicStructMethod(FGeneratedWrappedDynamicStructMethod&&) = default;
		FGeneratedWrappedDynamicStructMethod(const FGeneratedWrappedDynamicStructMethod&) = default;
		FGeneratedWrappedDynamicStructMethod& operator=(FGeneratedWrappedDynamicStructMethod&&) = default;
		FGeneratedWrappedDynamicStructMethod& operator=(const FGeneratedWrappedDynamicStructMethod&) = default;

		/** The struct parameter information (this parameter is set to the struct instance that calls the method) */
		FGeneratedWrappedMethodParameter StructParam;
	};

	/** Stores the data needed by a runtime generated Python method that is dynamically created and registered post-finalize of its owner struct type (for hoisting util functions onto structs) */
	struct FGeneratedWrappedDynamicStructMethodWithClosure : public FGeneratedWrappedDynamicStructMethod
	{
		FGeneratedWrappedDynamicStructMethodWithClosure() = default;
		FGeneratedWrappedDynamicStructMethodWithClosure(FGeneratedWrappedDynamicStructMethodWithClosure&&) = default;
		FGeneratedWrappedDynamicStructMethodWithClosure(const FGeneratedWrappedDynamicStructMethodWithClosure&) = delete;
		FGeneratedWrappedDynamicStructMethodWithClosure& operator=(FGeneratedWrappedDynamicStructMethodWithClosure&&) = default;
		FGeneratedWrappedDynamicStructMethodWithClosure& operator=(const FGeneratedWrappedDynamicStructMethodWithClosure&) = delete;

		/** Called to ready the generated method for Python */
		void Finalize();

		/** Python method that was generated from this method */
		FPyMethodWithClosureDef PyMethod;
	};

	/** Stores the data needed by a runtime generated Python operator stack function */
	struct FGeneratedWrappedStructMathOpFunction : public FGeneratedWrappedFunction
	{
		FGeneratedWrappedStructMathOpFunction() = default;
		FGeneratedWrappedStructMathOpFunction(FGeneratedWrappedStructMathOpFunction&&) = default;
		FGeneratedWrappedStructMathOpFunction(const FGeneratedWrappedStructMathOpFunction&) = default;
		FGeneratedWrappedStructMathOpFunction& operator=(FGeneratedWrappedStructMathOpFunction&&) = default;
		FGeneratedWrappedStructMathOpFunction& operator=(const FGeneratedWrappedStructMathOpFunction&) = default;

		/** Set the function to call, and also extract the parameter lists */
		bool SetFunctionAndExtractParams(const UFunction* InFunc);

		/** The struct parameter information (this parameter is set to the struct instance that calls the function) */
		FGeneratedWrappedMethodParameter StructParam;
	};

	/** Stores the data needed by a runtime generated Python operator stack that is dynamically created and registered post-finalize of its owner struct type (for hoisting math operators onto structs) */
	struct FGeneratedWrappedStructMathOpStack
	{
		/** Known math operator types */
		enum class EOpType : uint8
		{
			Add = 0,			// +
			InlineAdd,			// +=
			Subtract,			// -
			InlineSubtract,		// -=
			Multiply,			// *
			InlineMultiply,		// *=
			Divide,				// /
			InlineDivide,		// /=
			Modulus,			// %
			InlineModulus,		// %=
			And,				// &
			InlineAnd,			// &=
			Or,					// |
			InlineOr,			// |=
			Xor,				// ^
			InlineXor,			// ^=
			RightShift,			// >>
			InlineRightShift,	// >>=
			LeftShift,			// <<
			InlineLeftShift,	// <<=
			Num,
		};

		/**
		 * Given a potential operator string, try and convert it to a known operator type
		 * @return true if the conversion was a success, false otherwise
		 */
		static bool StringToOpType(const TCHAR* InStr, EOpType& OutOpType);

		/**
		 * Is the given operator a inline operator?
		 */
		static bool IsInlineOp(const EOpType InOpType);

		/** Array of math operator functions associated with this operator stack */
		TArray<FGeneratedWrappedStructMathOpFunction> MathOpFuncs;
	};

	/** Stores the data needed by a runtime generated Python get/set */
	struct FGeneratedWrappedGetSet
	{
		FGeneratedWrappedGetSet()
			: Prop(nullptr)
			, GetCallback(nullptr)
			, SetCallback(nullptr)
		{
		}

		FGeneratedWrappedGetSet(FGeneratedWrappedGetSet&&) = default;
		FGeneratedWrappedGetSet(const FGeneratedWrappedGetSet&) = default;
		FGeneratedWrappedGetSet& operator=(FGeneratedWrappedGetSet&&) = default;
		FGeneratedWrappedGetSet& operator=(const FGeneratedWrappedGetSet&) = default;

		/** Convert this wrapper type to its Python type */
		void ToPython(PyGetSetDef& OutPyGetSet) const;

		/** The name of the get/set */
		FUTF8Buffer GetSetName;

		/** The doc string of the get/set */
		FUTF8Buffer GetSetDoc;

		/** The Unreal property for this get/set */
		const UProperty* Prop;

		/** The Unreal function for the get (if any) */
		FGeneratedWrappedFunction GetFunc;

		/** The Unreal function for the set (if any) */
		FGeneratedWrappedFunction SetFunc;

		/* The C function that should be called for get */
		getter GetCallback;

		/* The C function that should be called for set */
		setter SetCallback;
	};

	/** Stores the data needed for a set of runtime generated Python get/sets */
	struct FGeneratedWrappedGetSets
	{
		FGeneratedWrappedGetSets() = default;
		FGeneratedWrappedGetSets(FGeneratedWrappedGetSets&&) = default;
		FGeneratedWrappedGetSets(const FGeneratedWrappedGetSets&) = delete;
		FGeneratedWrappedGetSets& operator=(FGeneratedWrappedGetSets&&) = default;
		FGeneratedWrappedGetSets& operator=(const FGeneratedWrappedGetSets&) = delete;

		/** Called to ready the generated get/sets with Python */
		void Finalize();

		/** Array of get/sets from the wrapped type */
		TArray<FGeneratedWrappedGetSet> TypeGetSets;

		/** The Python get/sets that were generated from TypeGetSets (in Finalize) */
		TArray<PyGetSetDef> PyGetSets;
	};

	/** Stores the data needed to generate a Python doc string for editor exposed properties */
	struct FGeneratedWrappedPropertyDoc
	{
		explicit FGeneratedWrappedPropertyDoc(const UProperty* InProp);

		FGeneratedWrappedPropertyDoc(FGeneratedWrappedPropertyDoc&&) = default;
		FGeneratedWrappedPropertyDoc(const FGeneratedWrappedPropertyDoc&) = default;
		FGeneratedWrappedPropertyDoc& operator=(FGeneratedWrappedPropertyDoc&&) = default;
		FGeneratedWrappedPropertyDoc& operator=(const FGeneratedWrappedPropertyDoc&) = default;

		/** Util function to sort an array of doc structs based on the Pythonized property name */
		static bool SortPredicate(const FGeneratedWrappedPropertyDoc& InOne, const FGeneratedWrappedPropertyDoc& InTwo);

		/** Util function to convert an array of doc structs into a combined doc string (the array should have been sorted before calling this) */
		static FString BuildDocString(const TArray<FGeneratedWrappedPropertyDoc>& InDocs, const bool bEditorVariant = false);

		/** Util function to convert an array of doc structs into a combined doc string (the array should have been sorted before calling this) */
		static void AppendDocString(const TArray<FGeneratedWrappedPropertyDoc>& InDocs, FString& OutStr, const bool bEditorVariant = false);

		/** Pythonized name of the property */
		FString PythonPropName;

		/** Pythonized doc string of the property */
		FString DocString;

		/** Pythonized editor doc string of the property */
		FString EditorDocString;
	};

	/** Stores the minimal data needed by a runtime generated Python type */
	struct FGeneratedWrappedType
	{
		FGeneratedWrappedType()
		{
			PyType = { PyVarObject_HEAD_INIT(nullptr, 0) };
		}

		virtual ~FGeneratedWrappedType() = default;

		FGeneratedWrappedType(FGeneratedWrappedType&&) = default;
		FGeneratedWrappedType(const FGeneratedWrappedType&) = delete;
		FGeneratedWrappedType& operator=(FGeneratedWrappedType&&) = default;
		FGeneratedWrappedType& operator=(const FGeneratedWrappedType&) = delete;

		/** Called to ready the generated type with Python */
		bool Finalize();

		/** Internal version of Finalize, called before readying the type with Python */
		virtual void Finalize_PreReady();

		/** Internal version of Finalize, called after readying the type with Python */
		virtual void Finalize_PostReady();

		/** The name of the type */
		FUTF8Buffer TypeName;

		/** The doc string of the type */
		FUTF8Buffer TypeDoc;

		/** The meta-data associated with this type */
		TSharedPtr<FPyWrapperBaseMetaData> MetaData;

		/* The Python type that was generated */
		PyTypeObject PyType;
	};

	/** Stores the data needed by a runtime generated Python struct type */
	struct FGeneratedWrappedStructType : public FGeneratedWrappedType
	{
		FGeneratedWrappedStructType() = default;
		FGeneratedWrappedStructType(FGeneratedWrappedStructType&&) = default;
		FGeneratedWrappedStructType(const FGeneratedWrappedStructType&) = delete;
		FGeneratedWrappedStructType& operator=(FGeneratedWrappedStructType&&) = default;
		FGeneratedWrappedStructType& operator=(const FGeneratedWrappedStructType&) = delete;

		/** Internal version of Finalize, called before readying the type with Python */
		virtual void Finalize_PreReady() override;

		/** Called to add a dynamic struct method to this Python type (this should only be called post-finalize) */
		void AddDynamicStructMethod(FGeneratedWrappedDynamicStructMethod&& InDynamicStructMethod);

		/** Get/sets associated with this type */
		FGeneratedWrappedGetSets GetSets;

		/** The doc string data for the properties associated with this type */
		TArray<FGeneratedWrappedPropertyDoc> PropertyDocs;

		/** Array of dynamic struct methods associated with this struct type (call AddDynamicStructMethod to add methods) */
		TArray<TSharedRef<FGeneratedWrappedDynamicStructMethodWithClosure>> DynamicStructMethods;

		/** Python number methods for this struct */
		PyNumberMethods PyNumber;
	};

	/** Stores the data needed by a runtime generated Python class type */
	struct FGeneratedWrappedClassType : public FGeneratedWrappedType
	{
		FGeneratedWrappedClassType() = default;
		FGeneratedWrappedClassType(FGeneratedWrappedClassType&&) = default;
		FGeneratedWrappedClassType(const FGeneratedWrappedClassType&) = delete;
		FGeneratedWrappedClassType& operator=(FGeneratedWrappedClassType&&) = default;
		FGeneratedWrappedClassType& operator=(const FGeneratedWrappedClassType&) = delete;

		/** Internal version of Finalize, called before readying the type with Python */
		virtual void Finalize_PreReady() override;

		/** Internal version of Finalize, called after readying the type with Python */
		virtual void Finalize_PostReady() override;

		/** Methods associated with this type */
		FGeneratedWrappedMethods Methods;

		/** Get/sets associated with this type */
		FGeneratedWrappedGetSets GetSets;

		/** The doc string data for the properties associated with this type */
		TArray<FGeneratedWrappedPropertyDoc> PropertyDocs;
	};

	/** Definition data for an Unreal property generated from a Python type */
	struct FPropertyDef
	{
		FPropertyDef() = default;
		FPropertyDef(FPropertyDef&&) = default;
		FPropertyDef(const FPropertyDef&) = delete;
		FPropertyDef& operator=(FPropertyDef&&) = default;
		FPropertyDef& operator=(const FPropertyDef&) = delete;

		/** Data needed to re-wrap this property for Python */
		FGeneratedWrappedGetSet GeneratedWrappedGetSet;

		/** Definition of the re-wrapped property for Python */
		PyGetSetDef PyGetSet;
	};

	/** Definition data for an Unreal function generated from a Python type */
	struct FFunctionDef
	{
		FFunctionDef() = default;
		FFunctionDef(FFunctionDef&&) = default;
		FFunctionDef(const FFunctionDef&) = delete;
		FFunctionDef& operator=(FFunctionDef&&) = default;
		FFunctionDef& operator=(const FFunctionDef&) = delete;

		/** Data needed to re-wrap this function for Python */
		FGeneratedWrappedMethod GeneratedWrappedMethod;

		/** Definition of the re-wrapped function for Python */
		FPyMethodWithClosureDef PyMethod;

		/** Python function to call when invoking this re-wrapped function */
		FPyObjectPtr PyFunction;

		/** Is this a function hidden from Python? (eg, internal getter/setter function) */
		bool bIsHidden;
	};

	/** How should PythonizeName adjust the final name? */
	enum EPythonizeNameCase : uint8
	{
		/** lower_snake_case */
		Lower,
		/** UPPER_SNAKE_CASE */
		Upper,
	};

	/** Context information passed to PythonizeTooltip */
	struct FPythonizeTooltipContext
	{
		FPythonizeTooltipContext()
			: Prop(nullptr)
			, Func(nullptr)
			, ReadOnlyFlags(CPF_BlueprintReadOnly | CPF_EditConst)
		{
		}

		FPythonizeTooltipContext(const UProperty* InProp, const UFunction* InFunc, const uint64 InReadOnlyFlags = CPF_BlueprintReadOnly | CPF_EditConst);

		/** Optional property that should be used when converting property tooltips */
		const UProperty* Prop;

		/** Optional function that should be used when converting function tooltips */
		const UFunction* Func;

		/** Optional deprecation message for the property or function */
		FString DeprecationMessage;

		/** Optional set of parameters that we should ignore when generating function tooltips */
		TSet<FName> ParamsToIgnore;

		/** Flags that dictate whether the property should be considered read-only */
		uint64 ReadOnlyFlags;
	};

	/** Convert a TCHAR to a UTF-8 buffer */
	FUTF8Buffer TCHARToUTF8Buffer(const TCHAR* InStr);

	/** Get the PostInit function from this Python type */
	PyObject* GetPostInitFunc(PyTypeObject* InPyType);

	/** Add a struct init parameter to the given array of method parameters */
	void AddStructInitParam(const UProperty* InUnrealProp, const TCHAR* InPythonAttrName, TArray<FGeneratedWrappedMethodParameter>& OutInitParams);

	/** Given a function, extract all of its parameter information (input and output) */
	void ExtractFunctionParams(const UFunction* InFunc, TArray<FGeneratedWrappedMethodParameter>& OutInputParams, TArray<FGeneratedWrappedMethodParameter>& OutOutputParams);

	/** Apply default values to function arguments */
	void ApplyParamDefaults(void* InBaseParamsAddr, const TArray<FGeneratedWrappedMethodParameter>& InParamDef);

	/** Parse an array Python objects from the args and keywords based on the generated method parameter data */
	bool ParseMethodParameters(PyObject* InArgs, PyObject* InKwds, const TArray<FGeneratedWrappedMethodParameter>& InParamDef, const char* InPyMethodName, TArray<PyObject*>& OutPyParams);

	/** Given a set of return values and the struct data associated with them, pack them appropriately for returning to Python */
	PyObject* PackReturnValues(void* InBaseParamsAddr, const TArray<FGeneratedWrappedMethodParameter>& InOutputParams, const TCHAR* InErrorCtxt, const TCHAR* InCallingCtxt);

	/** Given a Python return value, unpack the values into the struct data associated with them */
	bool UnpackReturnValues(PyObject* InRetVals, void* InBaseParamsAddr, const TArray<FGeneratedWrappedMethodParameter>& InOutputParams, const TCHAR* InErrorCtxt, const TCHAR* InCallingCtxt);

	/** Build a Python doc string for the given function and arguments list */
	FString BuildFunctionDocString(const UFunction* InFunc, const FString& InFuncPythonName, const TArray<FGeneratedWrappedMethodParameter>& InInputParams, const TArray<FGeneratedWrappedMethodParameter>& InOutputParams, const bool* InStaticOverride = nullptr);

	/** Is the given class generated by Blueprints? */
	bool IsBlueprintGeneratedClass(const UClass* InClass);

	/** Is the given struct generated by Blueprints? */
	bool IsBlueprintGeneratedStruct(const UStruct* InStruct);

	/** Is the given enum generated by Blueprints? */
	bool IsBlueprintGeneratedEnum(const UEnum* InEnum);

	/** Is the given class marked as deprecated? */
	bool IsDeprecatedClass(const UClass* InClass, FString* OutDeprecationMessage = nullptr);

	/** Is the given property marked as deprecated? */
	bool IsDeprecatedProperty(const UProperty* InProp, FString* OutDeprecationMessage = nullptr);

	/** Is the given function marked as deprecated? */
	bool IsDeprecatedFunction(const UFunction* InFunc, FString* OutDeprecationMessage = nullptr);

	/** Should the given class be exported to Python? */
	bool ShouldExportClass(const UClass* InClass);

	/** Should the given struct be exported to Python? */
	bool ShouldExportStruct(const UStruct* InStruct);

	/** Should the given enum be exported to Python? */
	bool ShouldExportEnum(const UEnum* InEnum);

	/** Should the given enum entry be exported to Python? */
	bool ShouldExportEnumEntry(const UEnum* InEnum, int32 InEnumEntryIndex);

	/** Should the given property be exported to Python? */
	bool ShouldExportProperty(const UProperty* InProp);

	/** Should the given property be exported to Python as editor-only data? */
	bool ShouldExportEditorOnlyProperty(const UProperty* InProp);

	/** Should the given function be exported to Python? */
	bool ShouldExportFunction(const UFunction* InFunc);

	/** Given a CamelCase name, convert it to snake_case */
	FString PythonizeName(const FString& InName, const EPythonizeNameCase InNameCase);

	/** Given a CamelCase property name, convert it to snake_case (may remove some superfluous parts of the property name) */
	FString PythonizePropertyName(const FString& InName, const EPythonizeNameCase InNameCase);

	/** Given a property tooltip, convert it to a doc string */
	FString PythonizePropertyTooltip(const FString& InTooltip, const UProperty* InProp, const uint64 InReadOnlyFlags = CPF_BlueprintReadOnly | CPF_EditConst);

	/** Given a function tooltip, convert it to a doc string */
	FString PythonizeFunctionTooltip(const FString& InTooltip, const UFunction* InFunc, const TSet<FName>& ParamsToIgnore = TSet<FName>());

	/** Given a tooltip, convert it to a doc string */
	FString PythonizeTooltip(const FString& InTooltip, const FPythonizeTooltipContext& InContext = FPythonizeTooltipContext());

	/** Get the native module the given field belongs to */
	FString GetFieldModule(const UField* InField);

	/** Given a native module name, get the Python module we should use */
	FString GetModulePythonName(const FName InModuleName, const bool bIncludePrefix = true);

	/** Get the Python name of the given class */
	FString GetClassPythonName(const UClass* InClass);

	/** Get the Python name of the given struct */
	FString GetStructPythonName(const UStruct* InStruct);

	/** Get the Python name of the given enum */
	FString GetEnumPythonName(const UEnum* InEnum);

	/** Get the Python name of the given delegate signature */
	FString GetDelegatePythonName(const UFunction* InDelegateSignature);

	/** Get the Python name of the given property */
	FString GetFunctionPythonName(const UFunction* InFunc);

	/** Get the Python name of the given enum */
	FString GetPropertyTypePythonName(const UProperty* InProp);

	/** Get the Python name of the given property */
	FString GetPropertyPythonName(const UProperty* InProp);

	/** Get the Python type of the given property */
	FString GetPropertyPythonType(const UProperty* InProp, const bool bIncludeReadWriteState = false, const uint64 InReadOnlyFlags = CPF_BlueprintReadOnly | CPF_EditConst);

	/** Append the Python type of the given property to the given string */
	void AppendPropertyPythonType(const UProperty* InProp, FString& OutStr, const bool bIncludeReadWriteState = false, const uint64 InReadOnlyFlags = CPF_BlueprintReadOnly | CPF_EditConst);

	/** Get the tooltip for the given field */
	FString GetFieldTooltip(const UField* InField);

	/** Get the tooltip for the given enum entry */
	FString GetEnumEntryTooltip(const UEnum* InEnum, const int64 InEntryIndex);
}

#endif	// WITH_PYTHON
