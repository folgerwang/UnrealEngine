// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Niagara/Public/NiagaraCommon.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "StaticMeshResources.h"
#include "Curves/RichCurve.h"
#include "NiagaraMergeable.h"
#include "NiagaraDataInterfaceBase.h"
#include "NiagaraDataInterface.generated.h"

class INiagaraCompiler;
class UCurveVector;
class UCurveLinearColor;
class UCurveFloat;
class FNiagaraSystemInstance;

struct FNDITransformHandlerNoop
{
	FORCEINLINE void TransformPosition(FVector& V, const FMatrix& M) {  }
	FORCEINLINE void TransformVector(FVector& V, const FMatrix& M) { }
};

struct FNDITransformHandler
{
	FORCEINLINE void TransformPosition(FVector& P, const FMatrix& M) { P = M.TransformPosition(P); }
	FORCEINLINE void TransformVector(FVector& V, const FMatrix& M) { V = M.TransformVector(V).GetUnsafeNormal3(); }
};

//////////////////////////////////////////////////////////////////////////
// Some helper classes allowing neat, init time binding of templated vm external functions.

struct TNDINoopBinder {};

// Adds a known type to the parameters
template<typename DirectType, typename NextBinder>
struct TNDIExplicitBinder
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
		NextBinder::template Bind<ParamTypes..., DirectType>(Interface, BindingInfo, InstanceData, OutFunc);
	}
};

// Binder that tests the location of an operand and adds the correct handler type to the Binding parameters.
template<int32 ParamIdx, typename DataType, typename NextBinder>
struct TNDIParamBinder
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
		if (BindingInfo.InputParamLocations[ParamIdx])
		{
			NextBinder::template Bind<ParamTypes..., VectorVM::FExternalFuncConstHandler<DataType>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
		else
		{
			NextBinder::template Bind<ParamTypes..., VectorVM::FExternalFuncRegisterHandler<DataType>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
	}
};

template<int32 ParamIdx, typename DataType>
struct TNDIParamBinder<ParamIdx, DataType, TNDINoopBinder>
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
	}
};

#define NDI_FUNC_BINDER(ClassName, FuncName) T##ClassName##_##FuncName##Binder

#define DEFINE_NDI_FUNC_BINDER(ClassName, FuncName)\
struct NDI_FUNC_BINDER(ClassName, FuncName)\
{\
	template<typename ... ParamTypes>\
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)\
	{\
		auto Lambda = [Interface](FVectorVMContext& Context) { static_cast<ClassName*>(Interface)->FuncName<ParamTypes...>(Context); };\
		OutFunc = FVMExternalFunction::CreateLambda(Lambda);\
	}\
};

#define DEFINE_NDI_DIRECT_FUNC_BINDER(ClassName, FuncName)\
struct NDI_FUNC_BINDER(ClassName, FuncName)\
{\
	static void Bind(UNiagaraDataInterface* Interface, FVMExternalFunction &OutFunc)\
	{\
		auto Lambda = [Interface](FVectorVMContext& Context) { static_cast<ClassName*>(Interface)->FuncName(Context); };\
		OutFunc = FVMExternalFunction::CreateLambda(Lambda);\
	}\
};

#if WITH_EDITOR
// Helper class for GUI error handling
DECLARE_DELEGATE_RetVal(bool, FNiagaraDataInterfaceFix);
class FNiagaraDataInterfaceError
{
public:
	FNiagaraDataInterfaceError(FText InErrorText,
		FText InErrorSummaryText,
		FNiagaraDataInterfaceFix InFix)
		: ErrorText(InErrorText)
		, ErrorSummaryText(InErrorSummaryText)
		, Fix(InFix)

	{};
	FNiagaraDataInterfaceError()
	{};
	/** Returns true if the error can be fixed automatically. */
	bool GetErrorFixable() const
	{
		return Fix.IsBound();
	};

	/** Applies the fix if a delegate is bound for it.*/
	bool TryFixError()
	{
		return Fix.IsBound() ? Fix.Execute() : false;
	};

	/** Full error description text */
	FText GetErrorText() const
	{
		return ErrorText;
	};

	/** Shortened error description text*/
	FText GetErrorSummaryText() const
	{
		return ErrorSummaryText;
	};

private:
	FText ErrorText;
	FText ErrorSummaryText;
	FNiagaraDataInterfaceFix Fix;
};
#endif

//////////////////////////////////////////////////////////////////////////

/** Base class for all Niagara data interfaces. */
UCLASS(abstract, EditInlineNew)
class NIAGARA_API UNiagaraDataInterface : public UNiagaraDataInterfaceBase
{
	GENERATED_UCLASS_BODY()
		 
public: 

	// UObject Interface
	virtual void PostLoad()override;
	// UObject Interface END

	/** Initializes the per instance data for this interface. Returns false if there was some error and the simulation should be disabled. */
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) { return true; }

	/** Destroys the per instence data for this interface. */
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) {}

	/** Ticks the per instance data for this interface, if it has any. */
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) { return false; }
	virtual bool PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) { return false; }

	/** 
	Returns the size of the per instance data for this interface. 0 if this interface has no per instance data. 
	Must depend solely on the class of the interface and not on any particular member data of a individual interface.
	*/
	virtual int32 PerInstanceDataSize()const { return 0; }

	/** Gets all the available functions for this data interface. */
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) {}

	/** Returns the delegate for the passed function signature. */
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) { };
	
	/** Copies the contents of this DataInterface to another.*/
	bool CopyTo(UNiagaraDataInterface* Destination) const;

	/** Determines if this DataInterface is the same as another.*/
	virtual bool Equals(const UNiagaraDataInterface* Other) const;

	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const { return false; }

	/** Determines if this type definition matches to a known data interface type.*/
	static bool IsDataInterfaceType(const FNiagaraTypeDefinition& TypeDef);

	virtual bool GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
	{
//		checkf(false, TEXT("Unefined HLSL in data interface. Interfaces need to be able to return HLSL for each function they define in GetFunctions."));
		return false;
	}
	virtual void GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
	{
//		checkf(false, TEXT("Unefined HLSL in data interface. Interfaces need to define HLSL for uniforms their functions access."));
	}

#if WITH_EDITOR	
	/** Refreshes and returns the errors detected with the corresponding data, if any.*/
	virtual TArray<FNiagaraDataInterfaceError> GetErrors() { return TArray<FNiagaraDataInterfaceError>(); }

	/** Validates a function being compiled and allows interface classes to post custom compile errors when their API changes. */
	virtual void ValidateFunction(const FNiagaraFunctionSignature& Function, TArray<FText>& OutValidationErrors);
#endif

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const;
};

/** Base class for curve data interfaces which facilitates handling the curve data in a standardized way. */
UCLASS(EditInlineNew, Category = "Curves", meta = (DisplayName = "Float Curve"))
class NIAGARA_API UNiagaraDataInterfaceCurveBase : public UNiagaraDataInterface
{
protected:
	GENERATED_BODY()
	UPROPERTY()
	bool GPUBufferDirty;
	UPROPERTY()
	TArray<float> ShaderLUT;
	UPROPERTY()
	float LUTMinTime;
	UPROPERTY()
	float LUTMaxTime;
	UPROPERTY()
	float LUTInvTimeRange;

	/** Remap a sample time for this curve to 0 to 1 between first and last keys for LUT access.*/
	FORCEINLINE float NormalizeTime(float T)
	{
		return (T - LUTMinTime) * LUTInvTimeRange;
	}

	/** Remap a 0 to 1 value between the first and last keys to a real sample time for this curve. */
	FORCEINLINE float UnnormalizeTime(float T)
	{
		return (T / LUTInvTimeRange) + LUTMinTime;
	}

public:
	UNiagaraDataInterfaceCurveBase()
		: GPUBufferDirty(false)
		, LUTMinTime(0.0f)
		, LUTMaxTime(1.0f)
		, LUTInvTimeRange(1.0f)
		, bUseLUT(true)
#if WITH_EDITORONLY_DATA
		, ShowInCurveEditor(false)
#endif
	{
	}

	UNiagaraDataInterfaceCurveBase(FObjectInitializer const& ObjectInitializer)
		: GPUBufferDirty(false)
		, LUTMinTime(0.0f)
		, LUTMaxTime(1.0f)
		, LUTInvTimeRange(1.0f)
		, bUseLUT(true)
#if WITH_EDITORONLY_DATA
		, ShowInCurveEditor(false)
#endif
	{}

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Curve")
	uint32 bUseLUT : 1;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Transient, Category = "Curve")
		bool ShowInCurveEditor;
#endif

	enum
	{
		CurveLUTWidth = 128,
		CurveLUTWidthMinusOne = 127,
	};
	/** Structure to facilitate getting standardized curve information from a curve data interface. */
	struct FCurveData
	{
		FCurveData(FRichCurve* InCurve, FName InName, FLinearColor InColor)
			: Curve(InCurve)
			, Name(InName)
			, Color(InColor)
		{
		}
		/** A pointer to the curve. */
		FRichCurve* Curve;
		/** The name of the curve, unique within the data interface, which identifies the curve in the UI. */
		FName Name;
		/** The color to use when displaying this curve in the UI. */
		FLinearColor Color;
	};

	/** Gets information for all of the curves owned by this curve data interface. */
	virtual void GetCurveData(TArray<FCurveData>& OutCurveData) { }

	virtual void GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)override;
	virtual FNiagaraDataInterfaceParametersCS* ConstructComputeParameters()const override;

	virtual int32 GetCurveNumElems()const 
	{
		checkf(false, TEXT("You must implement this function so the GPU buffer can be created of the correct size."));
		return 0; 
	}

	//TODO: Make this a texture and get HW filter + clamping?
	FReadBuffer& GetCurveLUTGPUBuffer();

	//UNiagaraDataInterface interface
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return true; }
	virtual void UpdateLUT() { };

	FORCEINLINE float GetMinTime()const { return LUTMinTime; }
	FORCEINLINE float GetMaxTime()const { return LUTMaxTime; }
	FORCEINLINE float GetInvTimeRange()const { return LUTInvTimeRange; }


protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	virtual bool CompareLUTS(const TArray<float>& OtherLUT) const;
	//UNiagaraDataInterface interface END

	FReadBuffer CurveLUT;
};

//External function binder choosing between template specializations based on if a curve should use the LUT over full evaluation.
template<typename NextBinder>
struct TCurveUseLUTBinder
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
		UNiagaraDataInterfaceCurveBase* CurveInterface = CastChecked<UNiagaraDataInterfaceCurveBase>(Interface);
		if (CurveInterface->bUseLUT)
		{
			NextBinder::template Bind<ParamTypes..., TIntegralConstant<bool, true>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
		else
		{
			NextBinder::template Bind<ParamTypes..., TIntegralConstant<bool, false>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
	}
};
