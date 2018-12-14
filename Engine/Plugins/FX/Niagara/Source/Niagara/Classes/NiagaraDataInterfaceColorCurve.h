// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "StaticMeshResources.h"
#include "Curves/RichCurve.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceColorCurve.generated.h"

class INiagaraCompiler;
class UCurveVector;
class UCurveLinearColor;
class UCurveFloat;
class FNiagaraSystemInstance;


/** Data Interface allowing sampling of color curves. */
UCLASS(EditInlineNew, Category="Curves", meta = (DisplayName = "Curve for Colors"))
class NIAGARA_API UNiagaraDataInterfaceColorCurve : public UNiagaraDataInterfaceCurveBase
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Curve")
	FRichCurve RedCurve;

	UPROPERTY(EditAnywhere, Category = "Curve")
	FRichCurve GreenCurve;

	UPROPERTY(EditAnywhere, Category = "Curve")
	FRichCurve BlueCurve;

	UPROPERTY(EditAnywhere, Category = "Curve")
	FRichCurve AlphaCurve;

	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	//UObject Interface End

	enum
	{
		CurveLUTNumElems = 4,
		CurveLUTMax = (CurveLUTWidth * CurveLUTNumElems) - 1,
	};

	void UpdateLUT();

	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;

	template<typename UseLUT>
	void SampleCurve(FVectorVMContext& Context);

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	//~ UNiagaraDataInterfaceCurveBase interface
	virtual void GetCurveData(TArray<FCurveData>& OutCurveData) override;

	virtual bool GetFunctionHLSL(const FName&  DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;

	virtual int32 GetCurveNumElems()const { return CurveLUTNumElems; }
protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

private:
	template<typename UseLUT>
	FORCEINLINE_DEBUGGABLE FLinearColor SampleCurveInternal(float X);

	static const FName SampleCurveName;
};
