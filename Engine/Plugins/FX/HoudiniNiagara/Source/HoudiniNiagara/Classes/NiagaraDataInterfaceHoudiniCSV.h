/*
* Copyright (c) <2018> Side Effects Software Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
*/
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "HoudiniCSV.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceHoudiniCSV.generated.h"


/** Data Interface allowing sampling of Houdini CSV files. */
UCLASS(EditInlineNew, Category = "Houdini Niagara", meta = (DisplayName = "Houdini Array Info"))
class HOUDININIAGARA_API UNiagaraDataInterfaceHoudiniCSV : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()
public:

	UPROPERTY( EditAnywhere, Category = "Houdini Niagara", meta = (DisplayName = "Houdini CSV Asset" ) )
	UHoudiniCSV* HoudiniCSVAsset;

	//----------------------------------------------------------------------------
	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//----------------------------------------------------------------------------

	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	
	/** Returns the delegate for the passed function signature. */
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	//----------------------------------------------------------------------------
	// EXPOSED FUNCTIONS

	// Returns the float value at a given row and column in the CSV file
	template<typename RowParamType, typename ColParamType>
	void GetFloatValue(FVectorVMContext& Context);

	/*
	template<typename RowParamType, typename ColTitleParamType>
	void GetFloatValueByString(FVectorVMContext& Context);
	*/

	// Returns a Vector3 value for a given row in the CSV file
	template<typename RowParamType, typename ColParamType>
	void GetVectorValue(FVectorVMContext& Context);

	// Returns a Vector3 value for a given row in the CSV file
	template<typename RowParamType, typename ColParamType, typename DoSwapParamType, typename DoScaleParamType>
	void GetVectorValueEx(FVectorVMContext& Context);

	// Returns the positions for a given row in the CSV file
	template<typename RowParamType>
	void GetPosition(FVectorVMContext& Context);

	// Returns the normals for a given row in the CSV file
	template<typename RowParamType>
	void GetNormal(FVectorVMContext& Context);

	// Returns the time for a given row in the CSV file
	template<typename RowParamType>
	void GetTime(FVectorVMContext& Context);

	// Returns the velocity for a given row in the CSV file
	template<typename RowParamType>
	void GetVelocity(FVectorVMContext& Context);

	// Returns the color for a given row in the CSV file
	template<typename RowParamType>
	void GetColor(FVectorVMContext& Context);

	// Returns the position and time for a given row in the CSV file
	template<typename RowParamType>
	void GetPositionAndTime(FVectorVMContext& Context);

	// Returns the number of rows found in the CSV file
	void GetNumberOfRows(FVectorVMContext& Context);

	// Returns the number of columns found in the CSV file
	void GetNumberOfColumns(FVectorVMContext& Context);

	// Returns the number of points found in the CSV file
	void GetNumberOfPoints(FVectorVMContext& Context);

	// Returns the last index of the points that should be spawned at time t
	template<typename TimeParamType>
	void GetLastRowIndexAtTime(FVectorVMContext& Context);

	// Returns the indexes (min, max) and number of points that should be spawned at time t
	template<typename TimeParamType>
	void GetPointIDsToSpawnAtTime(FVectorVMContext& Context);

	// Returns the position for a given point at a given time
	template<typename PointIDParamType, typename TimeParamType>
	void GetPointPositionAtTime(FVectorVMContext& Context);

	// Returns a float value for a given point at a given time
	template<typename PointIDParamType, typename ColParamType, typename TimeParamType>
	void GetPointValueAtTime(FVectorVMContext& Context);

	// Returns a Vector value for a given point at a given time
	template<typename PointIDParamType, typename ColParamType, typename TimeParamType>
	void GetPointVectorValueAtTime(FVectorVMContext& Context);

	// Returns a Vector value for a given point at a given time
	template<typename PointIDParamType, typename ColParamType, typename TimeParamType, typename DoSwapParamType, typename DoScaleParamType>
	void GetPointVectorValueAtTimeEx(FVectorVMContext& Context);

	// Returns the line indexes (previous, next) for reading values for a given point at a given time
	template<typename PointIDParamType, typename TimeParamType>
	void GetRowIndexesForPointAtTime(FVectorVMContext& Context);

	// Return the life value for a given point
	template<typename PointIDParamType>
	void GetPointLife(FVectorVMContext& Context);

	// Return the life of a given point at a given time
	//template<typename PointIDParamType, typename TimeParamType>
	//void GetPointLifeAtTime(FVectorVMContext& Context);

	// Return the type value for a given point
	template<typename PointIDParamType>
	void GetPointType( FVectorVMContext& Context );
	
	//----------------------------------------------------------------------------
	// GPU / HLSL Functions
	virtual bool GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;

	// Disabling GPU sim for now.
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return Target == ENiagaraSimTarget::CPUSim; }

protected:

	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	// Indicates the GPU buffers need to be updated
	UPROPERTY()
	bool GPUBufferDirty;

	// Last Spawned PointID
	UPROPERTY()
	int32 LastSpawnedPointID;
};
