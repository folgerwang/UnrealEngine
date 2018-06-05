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

#include "NiagaraDataInterfaceHoudiniCSV.h"
#include "NiagaraTypes.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "HoudiniNiagaraCSVDataInterface"  


UNiagaraDataInterfaceHoudiniCSV::UNiagaraDataInterfaceHoudiniCSV(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
    HoudiniCSVAsset = nullptr;
	LastSpawnedPointID = -1;
}

void UNiagaraDataInterfaceHoudiniCSV::PostInitProperties()
{
    Super::PostInitProperties();

    if (HasAnyFlags(RF_ClassDefaultObject))
    {
	    FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
    }

    GPUBufferDirty = true;
	LastSpawnedPointID = -1;
}

void UNiagaraDataInterfaceHoudiniCSV::PostLoad()
{
    Super::PostLoad();
    GPUBufferDirty = true;
	LastSpawnedPointID = -1;
}

#if WITH_EDITOR

void UNiagaraDataInterfaceHoudiniCSV::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceHoudiniCSV, HoudiniCSVAsset))
    {
		Modify();
		if (HoudiniCSVAsset)
		{
			GPUBufferDirty = true;
			LastSpawnedPointID = -1;
		}
    }
}

#endif

bool UNiagaraDataInterfaceHoudiniCSV::CopyToInternal(UNiagaraDataInterface* Destination) const
{
    if ( !Super::CopyToInternal( Destination ) )
		return false;

    UNiagaraDataInterfaceHoudiniCSV* CastedInterface = CastChecked<UNiagaraDataInterfaceHoudiniCSV>( Destination );
    if ( !CastedInterface )
		return false;

    CastedInterface->HoudiniCSVAsset = HoudiniCSVAsset;

    return true;
}

bool UNiagaraDataInterfaceHoudiniCSV::Equals(const UNiagaraDataInterface* Other) const
{
    if ( !Super::Equals(Other) )
		return false;

    const UNiagaraDataInterfaceHoudiniCSV* OtherHNCSV = CastChecked<UNiagaraDataInterfaceHoudiniCSV>(Other);

    if ( OtherHNCSV != nullptr && OtherHNCSV->HoudiniCSVAsset != nullptr && HoudiniCSVAsset )
    {
		// Just make sure the two interfaces point to the same file
		return OtherHNCSV->HoudiniCSVAsset->FileName.Equals( HoudiniCSVAsset->FileName );
    }

    return false;
}


void UNiagaraDataInterfaceHoudiniCSV::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
    {
		// GetFloatValue
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetFloatValue");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));			// Col Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));	// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetFloatValue",
			"Returns the float value in the CSV file for a given Row and Column.\n" ) );

		OutFunctions.Add( Sig );
    }

	/*
    {
		// GetFloatValueByString
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetFloatValueByString");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));		// Row Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetStringDef(), TEXT("ColTitle")));	// Col Title In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));	// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetFloatValueByString",
		"Returns the float value in the CSV file for a given Row, in the column corresponding to the ColTitle string.\n" ) );

		OutFunctions.Add(Sig);
    }
	*/

	{
		// GetVectorValue
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetVectorValue");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));			// Col Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));		// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetVectorValue",
			"Returns a Vector3 in the CSV file for a given Row and Column.\nThe returned Vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetVectorValueEx
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetVectorValueEx");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));			// Col Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoSwap")));		// DoSwap in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoScale")));	// DoScale in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));		// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetVectorValueEx",
			"Returns a Vector3 in the CSV file for a given Row and Column.\nThe DoSwap parameter indicates if the vector should be converted from Houdini*s coordinate system to Unreal's.\nThe DoScale parameter decides if the Vector value should be converted from meters (Houdini) to centimeters (Unreal)."));

		OutFunctions.Add(Sig);
	}

    {
		// GetPosition
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetPosition");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));	// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPosition",
			"Helper function returning the position value for a given Row in the CSV file.\nThe returned Position vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
    }

    {
		// GetPositionAndTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetPositionAndTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));	// Vector3 Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		// float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPositionAndTime",
			"Helper function returning the position and time values for a given Row in the CSV file.\nThe returned Position vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
    }

    {
		// GetNormal
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetNormal");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));	// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetNormal",
			"Helper function returning the normal value for a given Row in the CSV file.\nThe returned Normal vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
    }

    {
		// GetTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		// Float Out

		Sig.SetDescription( LOCTEXT("DataInterfaceHoudini_GetTime",
			"Helper function returning the time value for a given Row in the CSV file.\n") );

		OutFunctions.Add(Sig);
    }

	{
		// GetVelocity
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetVelocity");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));	// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetVelocity",
			"Helper function returning the velocity value for a given Row in the CSV file.\nThe returned velocity vector is converted from Houdini's coordinate system to Unreal's."));

		OutFunctions.Add(Sig);
	}

	{
		// GetColor
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetColor");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));			// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Row")));			// Row Index In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));	// Color Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetColor",
			"Helper function returning the color value for a given Row in the CSV file."));

		OutFunctions.Add(Sig);
	}

    {
		// GetNumberOfPoints
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetNumberOfPoints");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));					// CSV in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfPoints")));  // Int Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetNumberOfPoints",
			"Returns the number of points (with different id values) in the CSV file.\n" ) );

		OutFunctions.Add(Sig);
    }

	{
		// GetNumberOfRows
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetNumberOfRows");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add( FNiagaraVariable( FNiagaraTypeDefinition( GetClass()), TEXT("CSV") ) );					// CSV in
		Sig.Outputs.Add( FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfRows") ) );		// Int Out
		
		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetNumberOfRows",
			"Returns the number of rows in the CSV file.\nOnly the number of value rows is returned, the first \"Title\" Row is ignored." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetNumberOfColumns
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetNumberOfColumns");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add( FNiagaraVariable( FNiagaraTypeDefinition( GetClass()), TEXT("CSV") ) );						// CSV in
		Sig.Outputs.Add( FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("NumberOfColumns") ) );		// Int Out
		
		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetNumberOfColumns",
			"Returns the number of columns in the CSV file." ) );

		OutFunctions.Add(Sig);
	}

    {
		// GetLastRowIndexAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetLastRowIndexAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition(GetClass()), TEXT("CSV") ) );					// CSV in
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time") ) );				// Time in
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("LastRowIndex") ) );	    // Int Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetLastRowIndexAtTime",
			"Returns the index of the last row in the CSV file that has a time value lesser or equal to the Time parameter." ) );

		OutFunctions.Add(Sig);
    }

    {
		// GetPointIdsToSpawnAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetPointIDsToSpawnAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition(GetClass()), TEXT("CSV") ) );				// CSV in
		Sig.Inputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time") ) );		    // Time in
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("MinIndex") ) );	    // Int Out
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("MaxIndex") ) );	    // Int Out
		Sig.Outputs.Add(FNiagaraVariable( FNiagaraTypeDefinition::GetIntDef(), TEXT("Count") ) );		    // Int Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointIDsToSpawnAtTime",
			"Returns the count and point IDs of the points that should spawn for a given time value." ) );

		OutFunctions.Add(Sig);
    }

	{
		// GetRowIndexesForPointAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetRowIndexesForPointAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));					// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));				// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));				// Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PreviousRow")));		// Int Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NextRow")));			// Int Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("PrevWeight")));		// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetRowIndexesForPointAtTime",
			"Returns the row indexes for a given point at a given time.\nThe previous row, next row and weight can then be used to Lerp between values.") );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointPositionAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetPointPositionAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));				// Point Number In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));		// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointPositionAtTime",
			"Helper function returning the linearly interpolated position for a given point at a given time.\nThe returned Position vector is converted from Houdini's coordinate system to Unreal's.") );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointValueAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetPointValueAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));				// Point Number In		
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));				// Col Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));		// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointValueAtTime",
			"Returns the linearly interpolated value in the specified column for a given point at a given time." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointVectorValueAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetPointVectorValueAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point ID In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));				// Col Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));			// Vector3 Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointVectorValueAtTime",
			"Helper function returning the linearly interpolated Vector value in the specified column for a given point at a given time.\nThe returned Vector is converted from Houdini's coordinate system to Unreal's." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointVectorValueAtTimeEx
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetPointVectorValueAtTimeEx");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point ID In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Col")));				// Col Index In
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoSwap")));			// DoSwap in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("DoScale")));		// DoScale in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));			// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointVectorValueAtTime",
			"Helper function returning the linearly interpolated Vector value in the specified column for a given point at a given time.\nThe DoSwap parameter indicates if the vector should be converted from Houdini*s coordinate system to Unreal's.\nThe DoScale parameter decides if the Vector value should be converted from meters (Houdini) to centimeters (Unreal)." ) );

		OutFunctions.Add(Sig);
	}

	{
		// GetPointLife
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetPointLife");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Life")));			// Float Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointLife",
			"Helper function returning the life value for a given point when spawned.\nThe life value is either calculated from the alive attribute or is the life attribute at spawn time."));

		OutFunctions.Add(Sig);
	}

	/*
	{
		// GetPointLifeAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetPointLifeAtTime");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In		
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Life")));			// Float Out

		Sig.SetDescription( LOCTEXT( "DataInterfaceHoudini_GetPointLifeAtTime",
			"Helper function returning the remaining life for a given point in the CSV file at a given time." ) );

		OutFunctions.Add(Sig);
	}
	*/

	{
		// GetPointType
		FNiagaraFunctionSignature Sig;
		Sig.Name = TEXT("GetPointType");
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CSV")));				// CSV in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PointID")));			// Point Number In		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Type")));			// Int Out

		Sig.SetDescription(LOCTEXT("DataInterfaceHoudini_GetPointType",
			"Helper function returning the type value for a given point when spawned.\n"));

		OutFunctions.Add(Sig);
	}
}

// build the shader function HLSL; function name is passed in, as it's defined per-DI; that way, configuration could change
// the HLSL in the spirit of a static switch
// TODO: need a way to identify each specific function here
// 
bool UNiagaraDataInterfaceHoudiniCSV::GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
    //FString BufferName = Descriptors[0].BufferParamName;
    //FString SecondBufferName = Descriptors[1].BufferParamName;

    /*
    if (InstanceFunctionName.Contains( TEXT( "GetFloatValue") ) )
    {
	FString BufferName = Descriptors[0].BufferParamName;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float In_Row, in float In_Col, out float Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value = ") + BufferName + TEXT("[(int)(In_Row + ( In_Col * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if (InstanceFunctionName.Contains(TEXT("GetPosition")))
    {
	FString BufferName = Descriptors[1].BufferParamName;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float In_N, out float3 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[(int)(In_N + ") + FString::FromInt(NumberOfRows) + TEXT(") ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[(int)(In_N + ( 2 * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if (InstanceFunctionName.Contains(TEXT("GetNormal")))
    {
	FString BufferName = Descriptors[2].BufferParamName;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float In_N, out float3 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[(int)(In_N + ") + FString::FromInt(NumberOfRows) + TEXT(") ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[(int)(In_N + ( 2 * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if (InstanceFunctionName.Contains(TEXT("GetTime")))
    {
	FString BufferName = Descriptors[3].BufferParamName;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float In_N, out float Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\n}\n");
    }
    else if (InstanceFunctionName.Contains( TEXT("GetNumberOfPointsInCSV") ) )
    {
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("( out int Out_Value ) \n{\n");
	OutHLSL += TEXT("\t Out_Value = ") + FString::FromInt(NumberOfRows) + TEXT(";");
	OutHLSL += TEXT("\n}\n");
    }*/
    /*else if (InstanceFunctionName.Contains(TEXT("GetPositionAndTime")))
    {
	OutHLSL += TEXT("void ") + FunctionName + TEXT("(in float In_N, out float4 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value.x = ") + BufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\t Out_Value.y = ") + BufferName + TEXT("[(int)(In_N + ") + FString::FromInt(NumberOfRows) + TEXT(") ];");
	OutHLSL += TEXT("\t Out_Value.z = ") + BufferName + TEXT("[(int)(In_N + ( 2 * ") + FString::FromInt(NumberOfRows) + TEXT(") ) ];");
	OutHLSL += TEXT("\t Out_Value.w = ") + SecondBufferName + TEXT("[(int)(In_N) ];");
	OutHLSL += TEXT("\n}\n");
    }*/

    return !OutHLSL.IsEmpty();
}

// build buffer definition hlsl
// 1. Choose a buffer name, add the data interface ID (important!)
// 2. add a DIGPUBufferParamDescriptor to the array argument; that'll be passed on to the FNiagaraShader for binding to a shader param, that can
// then later be found by name via FindDIBufferParam for setting; 
// 3. store buffer declaration hlsl in OutHLSL
// multiple buffers can be defined at once here
//
void UNiagaraDataInterfaceHoudiniCSV::GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
    FString BufferName = "CSVData_" + ParamInfo.DataInterfaceHLSLSymbol;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

    BufferName = "PositionData_" + ParamInfo.DataInterfaceHLSLSymbol;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

    BufferName = "NormalData_" + ParamInfo.DataInterfaceHLSLSymbol;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

    BufferName = "TimeData_" + ParamInfo.DataInterfaceHLSLSymbol;
    OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");
}

DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetFloatValue);
//DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetFloatValueByString);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetVectorValue);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetVectorValueEx);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPosition);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetNormal);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetColor);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetVelocity);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPositionAndTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetLastRowIndexAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointIDsToSpawnAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetRowIndexesForPointAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointPositionAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointValueAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointVectorValueAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointVectorValueAtTimeEx);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointLife);
//DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointLifeAtTime);
DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointType);
void UNiagaraDataInterfaceHoudiniCSV::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
    if (BindingInfo.Name == TEXT("GetFloatValue") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
    {
		TNDIParamBinder<0, int32, TNDIParamBinder<1, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetFloatValue)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    /*else if (BindingInfo.Name == TEXT("GetFloatValueByString") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
    {
		TNDIParamBinder<0, float, TNDIParamBinder<1, FString, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetFloatValueByString)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }*/
    else if (BindingInfo.Name == TEXT("GetVectorValue") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
    {
		TNDIParamBinder<0, int32, TNDIParamBinder<1, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetVectorValue)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
	else if (BindingInfo.Name == TEXT("GetVectorValueEx") && BindingInfo.GetNumInputs() == 4 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, TNDIParamBinder<1, int32, TNDIParamBinder<2, bool, TNDIParamBinder<3, bool, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetVectorValueEx)>>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
    else if (BindingInfo.Name == TEXT("GetPosition") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
		TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPosition)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetNormal") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
		TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetNormal)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
    {
		TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
	else if (BindingInfo.Name == TEXT("GetVelocity") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetVelocity)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("GetColor") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4)
	{
		TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetColor)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
    else if (BindingInfo.Name == TEXT("GetPositionAndTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4)
    {
		TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPositionAndTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if ( BindingInfo.Name == TEXT("GetNumberOfPoints") && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1 )
    {
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceHoudiniCSV::GetNumberOfPoints);
    }
	else if (BindingInfo.Name == TEXT("GetNumberOfRows") && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceHoudiniCSV::GetNumberOfRows);
	}
	else if (BindingInfo.Name == TEXT("GetNumberOfColumns") && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 1)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceHoudiniCSV::GetNumberOfColumns);
	}
    else if (BindingInfo.Name == TEXT("GetLastRowIndexAtTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
    {
		TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetLastRowIndexAtTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
    else if (BindingInfo.Name == TEXT("GetPointIDsToSpawnAtTime") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3)
    {
		TNDIParamBinder<0, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointIDsToSpawnAtTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
    }
	else if (BindingInfo.Name == TEXT("GetRowIndexesForPointAtTime") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, TNDIParamBinder<1, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetRowIndexesForPointAtTime)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("GetPointPositionAtTime") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, TNDIParamBinder<1, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointPositionAtTime)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("GetPointValueAtTime") && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 1)
	{
		TNDIParamBinder<0, int32, TNDIParamBinder<1, int32, TNDIParamBinder<2, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointValueAtTime)>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("GetPointVectorValueAtTime") && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, TNDIParamBinder<1, int32, TNDIParamBinder<2, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointVectorValueAtTime)>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("GetPointVectorValueAtTimeEx") && BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, TNDIParamBinder<1, int32, TNDIParamBinder<2, float, TNDIParamBinder<3, bool, TNDIParamBinder<4, bool, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointVectorValueAtTimeEx)>>>>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("GetPointLife") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
	{
		TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointLife)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	/*
	else if (BindingInfo.Name == TEXT("GetPointLifeAtTime") && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
	{
		TNDIParamBinder<0, int32, TNDIParamBinder<1, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointLifeAtTime)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	*/
	else if (BindingInfo.Name == TEXT("GetPointType") && BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
	{
		TNDIParamBinder<0, int32, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceHoudiniCSV, GetPointType)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
    else
    {
		UE_LOG( LogHoudiniNiagara, Error, 
	    TEXT( "Could not find data interface function:\n\tName: %s\n\tInputs: %i\n\tOutputs: %i" ),
	    *BindingInfo.Name.ToString(), BindingInfo.GetNumInputs(), BindingInfo.GetNumOutputs() );
		OutFunc = FVMExternalFunction();
    }
}

template<typename RowParamType, typename ColParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetFloatValue(FVectorVMContext& Context)
{
    RowParamType RowParam(Context);
    ColParamType ColParam(Context);

    FRegisterHandler<float> OutValue(Context);

    for ( int32 i = 0; i < Context.NumInstances; ++i )
    {
		int32 row = RowParam.Get();
		int32 col = ColParam.Get();
	
		float value = 0.0f;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetFloatValue( row, col, value );

		*OutValue.GetDest() = value;
		RowParam.Advance();
		ColParam.Advance();
		OutValue.Advance();
    }
}

template<typename RowParamType, typename ColParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetVectorValue( FVectorVMContext& Context )
{
    RowParamType RowParam(Context);
    ColParamType ColParam(Context);

	FRegisterHandler<float> OutVectorX(Context);
	FRegisterHandler<float> OutVectorY(Context);
	FRegisterHandler<float> OutVectorZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();
		int32 col = ColParam.Get();

		FVector V = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetVectorValue(row, col, V);

		*OutVectorX.GetDest() = V.X;
		*OutVectorY.GetDest() = V.Y;
		*OutVectorZ.GetDest() = V.Z;

		RowParam.Advance();
		ColParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
    }
}

template<typename RowParamType, typename ColParamType, typename DoSwapParamType, typename DoScaleParamType >
void UNiagaraDataInterfaceHoudiniCSV::GetVectorValueEx(FVectorVMContext& Context)
{
	RowParamType RowParam(Context);
	ColParamType ColParam(Context);
	DoSwapParamType DoSwapParam(Context);
	DoScaleParamType DoScaleParam(Context);

	FRegisterHandler<float> OutVectorX(Context);
	FRegisterHandler<float> OutVectorY(Context);
	FRegisterHandler<float> OutVectorZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 row = RowParam.Get();
		int32 col = ColParam.Get();

		bool DoSwap = DoSwapParam.Get();
		bool DoScale = DoScaleParam.Get();

		FVector V = FVector::ZeroVector;
		if (HoudiniCSVAsset)
			HoudiniCSVAsset->GetVectorValue(row, col, V, DoSwap, DoScale);

		*OutVectorX.GetDest() = V.X;
		*OutVectorY.GetDest() = V.Y;
		*OutVectorZ.GetDest() = V.Z;

		RowParam.Advance();
		ColParam.Advance();
		DoSwapParam.Advance();
		DoScaleParam.Advance();
		OutVectorX.Advance();
		OutVectorY.Advance();
		OutVectorZ.Advance();
	}
}
/*
template<typename RowParamType, typename ColTitleParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetFloatValueByString(FVectorVMContext& Context)
{
    RowParamType RowParam(Context);
    ColTitleParamType ColTitleParam(Context);

    FRegisterHandler<float> OutValue(Context);

    for ( int32 i = 0; i < Context.NumInstances; ++i )
    {
		int32 row = RowParam.Get();
		FString colTitle = ColTitleParam.Get();
	
		float value = 0.0f;
		if ( CSVFile )
			CSVFile->GetCSVFloatValue( row, colTitle, value );

		*OutValue.GetDest() = value;
		RowParam.Advance();
		ColTitleParam.Advance();
		OutValue.Advance();
    }
}
*/

template<typename RowParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetPosition(FVectorVMContext& Context)
{
	RowParamType RowParam(Context);
    FRegisterHandler<float> OutSampleX(Context);
    FRegisterHandler<float> OutSampleY(Context);
    FRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();

		FVector V = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetPositionValue( row, V );

		*OutSampleX.GetDest() = V.X;
		*OutSampleY.GetDest() = V.Y;
		*OutSampleZ.GetDest() = V.Z;
		RowParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
    }
}

template<typename RowParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetNormal(FVectorVMContext& Context)
{
	RowParamType RowParam(Context);
    FRegisterHandler<float> OutSampleX(Context);
    FRegisterHandler<float> OutSampleY(Context);
    FRegisterHandler<float> OutSampleZ(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();

		FVector V = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetNormalValue( row, V );

		*OutSampleX.GetDest() = V.X;
		*OutSampleY.GetDest() = V.Y;
		*OutSampleZ.GetDest() = V.Z;
		RowParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
    }
}

template<typename RowParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetTime(FVectorVMContext& Context)
{
	RowParamType RowParam(Context);
    FRegisterHandler<float> OutValue(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();

		float value = 0.0f;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetTimeValue( row, value );

		*OutValue.GetDest() = value;
		RowParam.Advance();
		OutValue.Advance();
    }
}

template<typename RowParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetVelocity(FVectorVMContext& Context)
{
	RowParamType RowParam(Context);
	FRegisterHandler<float> OutSampleX(Context);
	FRegisterHandler<float> OutSampleY(Context);
	FRegisterHandler<float> OutSampleZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 row = RowParam.Get();

		FVector V = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetVelocityValue( row, V );

		*OutSampleX.GetDest() = V.X;
		*OutSampleY.GetDest() = V.Y;
		*OutSampleZ.GetDest() = V.Z;
		RowParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
	}
}

template<typename RowParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetColor(FVectorVMContext& Context)
{
	RowParamType RowParam(Context);
	FRegisterHandler<float> OutSampleR(Context);
	FRegisterHandler<float> OutSampleG(Context);
	FRegisterHandler<float> OutSampleB(Context);
	FRegisterHandler<float> OutSampleA(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 row = RowParam.Get();

		FLinearColor C = FLinearColor::White;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetColorValue( row, C );

		*OutSampleR.GetDest() = C.R;
		*OutSampleG.GetDest() = C.G;
		*OutSampleB.GetDest() = C.B;
		*OutSampleA.GetDest() = C.A;
		RowParam.Advance();
		OutSampleR.Advance();
		OutSampleG.Advance();
		OutSampleB.Advance();
		OutSampleA.Advance();
	}
}

// Returns the last index of the points that should be spawned at time t
template<typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetLastRowIndexAtTime(FVectorVMContext& Context)
{
    TimeParamType TimeParam(Context);
    FRegisterHandler<int32> OutValue(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		float t = TimeParam.Get();

		int32 value = 0;
		if ( HoudiniCSVAsset )
			HoudiniCSVAsset->GetLastRowIndexAtTime( t, value );

		*OutValue.GetDest() = value;
		TimeParam.Advance();
		OutValue.Advance();
    }
}

// Returns the last index of the points that should be spawned at time t
template<typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetPointIDsToSpawnAtTime( FVectorVMContext& Context )
{
    TimeParamType TimeParam( Context );
    FRegisterHandler<int32> OutMinValue( Context );
    FRegisterHandler<int32> OutMaxValue( Context );
    FRegisterHandler<int32> OutCountValue( Context );

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		float t = TimeParam.Get();

		int32 value = 0;
		int32 min = 0, max = 0, count = 0;
		if ( HoudiniCSVAsset )
		{
			if ( !HoudiniCSVAsset->GetLastPointIDToSpawnAtTime( t, value ) )
			{
				// The CSV file doesn't have time informations, so always return all points in the file
				min = 0;
				max = value;
				count = max - min + 1;
			}
			else
			{
				// The CSV file has time informations
				// First, detect if we need to reset LastSpawnedPointID (after a loop of the emitter)
				if ( value < LastSpawnedPointID )
					LastSpawnedPointID = -1;

				if ( value < 0 )
				{
					// Nothing to spawn, t is lower than the point's time
					LastSpawnedPointID = -1;
				}
				else
				{
					// The last time value in the CSV is lower than t, spawn everything if we didnt already!
					if ( value >= HoudiniCSVAsset->GetNumberOfPoints() )
						value = value - 1;

					if ( value == LastSpawnedPointID)
					{
						// We dont have any new point to spawn
						min = value;
						max = value;
						count = 0;
					}
					else
					{
						// We have points to spawn at time t
						min = LastSpawnedPointID + 1;
						max = value;
						count = max - min + 1;

						LastSpawnedPointID = max;
					}
				}
			}
		}

		*OutMinValue.GetDest() = min;
		*OutMaxValue.GetDest() = max;
		*OutCountValue.GetDest() = count;

		TimeParam.Advance();
		OutMinValue.Advance();
		OutMaxValue.Advance();
		OutCountValue.Advance();
    }
}

template<typename RowParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetPositionAndTime(FVectorVMContext& Context)
{
    RowParamType RowParam(Context);

    FRegisterHandler<float> OutPosX(Context);
    FRegisterHandler<float> OutPosY(Context);
    FRegisterHandler<float> OutPosZ(Context);
    FRegisterHandler<float> OutTime(Context);

    for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 row = RowParam.Get();

		float timeValue = 0.0f;
		FVector posVector = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetTimeValue( row, timeValue);
			HoudiniCSVAsset->GetPositionValue( row, posVector);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		*OutTime.GetDest() = timeValue;

		RowParam.Advance();
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
		OutTime.Advance();
    }
}

template<typename PointIDParamType, typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetRowIndexesForPointAtTime(FVectorVMContext& Context)
{
	PointIDParamType PointIDParam(Context);
	TimeParamType TimeParam(Context);

	FRegisterHandler<int32> OutPrevIndex(Context);
	FRegisterHandler<int32> OutNextIndex(Context);
	FRegisterHandler<float> OutWeightValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		float weight = 0.0f;
		int32 prevIdx = 0;
		int32 nextIdx = 0;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetRowIndexesForPointAtTime( PointID, time, prevIdx, nextIdx, weight );
		}

		*OutPrevIndex.GetDest() = prevIdx;
		*OutNextIndex.GetDest() = nextIdx;
		*OutWeightValue.GetDest() = weight;

		PointIDParam.Advance();
		TimeParam.Advance();
		OutPrevIndex.Advance();
		OutNextIndex.Advance();
		OutWeightValue.Advance();
    }
}

template<typename PointIDParamType, typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetPointPositionAtTime( FVectorVMContext& Context )
{
	PointIDParamType PointIDParam(Context);
	TimeParamType TimeParam(Context);

	FRegisterHandler<float> OutPosX(Context);
	FRegisterHandler<float> OutPosY(Context);
	FRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
    {
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		FVector posVector = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetPointPositionAtTime(PointID, time, posVector);
		}		

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		PointIDParam.Advance();
		TimeParam.Advance();
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
    }
}

template<typename PointIDParamType, typename ColParamType, typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetPointValueAtTime(FVectorVMContext& Context)
{
	PointIDParamType PointIDParam(Context);
	TimeParamType TimeParam(Context);
	ColParamType ColParam(Context);

	FRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 PointID = PointIDParam.Get();
		int32 Col = ColParam.Get();
		float time = TimeParam.Get();		

		float Value = 0.0f;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetPointValueAtTime( PointID, Col, time, Value );
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();
		ColParam.Advance();
		TimeParam.Advance();

		OutValue.Advance();
	}
}

template<typename PointIDParamType, typename ColParamType, typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetPointVectorValueAtTime(FVectorVMContext& Context)
{
	PointIDParamType PointIDParam(Context);
	ColParamType ColParam(Context);
	TimeParamType TimeParam(Context);	

	FRegisterHandler<float> OutPosX(Context);
	FRegisterHandler<float> OutPosY(Context);
	FRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 PointID = PointIDParam.Get();
		int32 Col = ColParam.Get();
		float time = TimeParam.Get();		

		FVector posVector = FVector::ZeroVector;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetPointVectorValueAtTime( PointID, Col, time, posVector, true, true);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		PointIDParam.Advance();
		ColParam.Advance();
		TimeParam.Advance();
		
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
	}
}

template<typename PointIDParamType, typename ColParamType, typename TimeParamType, typename DoSwapParamType, typename DoScaleParamType >
void UNiagaraDataInterfaceHoudiniCSV::GetPointVectorValueAtTimeEx(FVectorVMContext& Context)
{
	PointIDParamType PointIDParam(Context);
	ColParamType ColParam(Context);
	TimeParamType TimeParam(Context);
	DoSwapParamType DoSwapParam(Context);
	DoScaleParamType DoScaleParam(Context);

	FRegisterHandler<float> OutPosX(Context);
	FRegisterHandler<float> OutPosY(Context);
	FRegisterHandler<float> OutPosZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 PointID = PointIDParam.Get();
		int32 Col = ColParam.Get();
		float time = TimeParam.Get();

		bool DoSwap = DoSwapParam.Get();
		bool DoScale = DoScaleParam.Get();

		FVector posVector = FVector::ZeroVector;
		if (HoudiniCSVAsset)
		{
			HoudiniCSVAsset->GetPointVectorValueAtTime(PointID, Col, time, posVector, DoSwap, DoScale);
		}

		*OutPosX.GetDest() = posVector.X;
		*OutPosY.GetDest() = posVector.Y;
		*OutPosZ.GetDest() = posVector.Z;

		PointIDParam.Advance();
		ColParam.Advance();
		TimeParam.Advance();
		DoSwapParam.Advance();
		DoScaleParam.Advance();

		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
	}
}

template<typename PointIDParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetPointLife(FVectorVMContext& Context)
{
	PointIDParamType PointIDParam(Context);

	FRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 PointID = PointIDParam.Get();

		float Value = 0.0f;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetPointLife(PointID, Value);
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();

		OutValue.Advance();
	}
}

/*
template<typename PointIDParamType, typename TimeParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetPointLifeAtTime(FVectorVMContext& Context)
{
	PointIDParamType PointIDParam(Context);
	TimeParamType TimeParam(Context);

	FRegisterHandler<float> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 PointID = PointIDParam.Get();
		float time = TimeParam.Get();

		float Value = 0.0f;
		if ( HoudiniCSVAsset )
		{
			HoudiniCSVAsset->GetPointLifeAtTime(PointID, time, Value);
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();
		TimeParam.Advance();

		OutValue.Advance();
	}
}
*/

template<typename PointIDParamType>
void UNiagaraDataInterfaceHoudiniCSV::GetPointType(FVectorVMContext& Context)
{
	PointIDParamType PointIDParam(Context);

	FRegisterHandler<int32> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 PointID = PointIDParam.Get();

		int32 Value = 0;
		if (HoudiniCSVAsset)
		{
			HoudiniCSVAsset->GetPointType(PointID, Value);
		}

		*OutValue.GetDest() = Value;

		PointIDParam.Advance();

		OutValue.Advance();
	}
}

void UNiagaraDataInterfaceHoudiniCSV::GetNumberOfRows(FVectorVMContext& Context)
{
    FRegisterHandler<int32> OutNumRows(Context);
    *OutNumRows.GetDest() = HoudiniCSVAsset ? HoudiniCSVAsset->GetNumberOfRows() : 0;
	OutNumRows.Advance();
}

void UNiagaraDataInterfaceHoudiniCSV::GetNumberOfColumns(FVectorVMContext& Context)
{
	FRegisterHandler<int32> OutNumCols(Context);
	*OutNumCols.GetDest() = HoudiniCSVAsset ? HoudiniCSVAsset->GetNumberOfColumns() : 0;
	OutNumCols.Advance();
}

void UNiagaraDataInterfaceHoudiniCSV::GetNumberOfPoints(FVectorVMContext& Context)
{
	FRegisterHandler<int32> OutNumPoints(Context);
	*OutNumPoints.GetDest() = HoudiniCSVAsset ? HoudiniCSVAsset->GetNumberOfPoints() : 0;
	OutNumPoints.Advance();
}

#undef LOCTEXT_NAMESPACE
