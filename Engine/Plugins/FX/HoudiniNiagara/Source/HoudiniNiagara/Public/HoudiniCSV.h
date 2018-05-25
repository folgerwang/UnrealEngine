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

#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "HoudiniCSV.generated.h"

DECLARE_LOG_CATEGORY_EXTERN( LogHoudiniNiagara, All, All );

UCLASS()
class HOUDININIAGARA_API UHoudiniCSV : public UObject
{
    GENERATED_UCLASS_BODY()
 
    public:
	
	//-----------------------------------------------------------------------------------------
	//  MEMBER FUNCTIONS
	//-----------------------------------------------------------------------------------------

	bool UpdateFromFile( const FString& TheFileName );
	bool UpdateFromStringArray( TArray<FString>& StringArray );

	void SetFileName( const FString& TheFilename );

	// Returns the number of points found in the CSV file
	int32 GetNumberOfPointsInCSV();

	// Returns the number of points found in the CSV file
	int32 GetNumberOfLinesInCSV();

	// Returns the column index for a given string
	bool GetColumnIndexFromString(const FString& ColumnTitle, int32& ColumnIndex);

	// Returns the float value at a given point in the CSV file
	bool GetCSVFloatValue( const int32& lineIndex, const int32& colIndex, float& value );
	// Returns the float value at a given point in the CSV file
	bool GetCSVFloatValue( const int32& lineIndex, const FString& ColumnTitle, float& value );
	// Returns the float value at a given point in the CSV file
	bool GetCSVStringValue( const int32& lineIndex, const int32& colIndex, FString& value );
	// Returns the string value at a given point in the CSV file
	bool GetCSVStringValue( const int32& lineIndex, const FString& ColumnTitle, FString& value );
	// Returns a Vector3 for a given point in the CSV file
	bool GetCSVVectorValue( const int32& lineIndex, const int32& colIndex, FVector& value, const bool& DoSwap = true, const bool& DoScale = true );
	
	// Returns a time value for a given point in the CSV file
	bool GetCSVTimeValue( const int32& lineIndex, float& value );
	// Returns a Position Vector3 for a given point in the CSV file (converted to unreal's coordinate system)
	bool GetCSVPositionValue( const int32& lineIndex, FVector& value );
	// Returns a Normal Vector3 for a given point in the CSV file (converted to unreal's coordinate system)
	bool GetCSVNormalValue( const int32& lineIndex, FVector& value );

	// Get the last index of the particles to be spawned at time t
	// Invalid Index are used to indicate edge cases:
	// -1 will be returned if no particles have been spawned ( t is smaller than the first particle time )
	// NumberOfLines will be returned if all particles in the CSV have been spawned ( t is higher than the last particle time )
	bool GetLastParticleIndexAtTime( const float& time, int32& lastIndex );
	// Returns the previous and next indexes for reading the values of a specified particle at a given time
	bool GetParticleLineIndexAtTime(const int32& ParticleID, const float& desiredTime, int32& PrevIndex, int32& NextIndex, float& PrevWeight);
	// Returns the value for a particle at a given time value (linearly interpolated) 
	bool GetParticleValueAtTime(const int32& ParticleID, const int32& ColumnIndex, const float& desiredTime, float& Value);
	// Returns the Vector Value for a given particle at a given time value (linearly interpolated) 
	bool GetParticleVectorValueAtTime(const int32& ParticleID, const int32& ColumnIndex, const float& desiredTime, FVector& Vector, const bool& DoSwap, const bool& DoScale);
	// Returns the Position Value for a given particle at a given time value (linearly interpolated) 
	bool GetParticlePositionAtTime(const int32& ParticleID, const float& desiredTime, FVector& Vector);

	//-----------------------------------------------------------------------------------------
	//  MEMBER VARIABLES
	//-----------------------------------------------------------------------------------------
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties" )
	FString FileName;

	// The number of values stored in the CSV file (excluding the title row)
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties" )
	int32 NumberOfLines;

	// The number of value TYPES stored in the CSV file
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties" )
	int32 NumberOfColumns;

	// The number of particles found in the CSV file
	UPROPERTY(VisibleAnywhere, Category = "Houdini CSV File Properties")
	int32 NumberOfParticles;

	// The tokenized title raw, describing the content of each column
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties" )
	TArray<FString> TitleRowArray;

#if WITH_EDITORONLY_DATA
	/** Importing data and options used for this asset */
	UPROPERTY(EditAnywhere, Instanced, Category = ImportSettings)
	class UAssetImportData* AssetImportData;

	virtual void PostInitProperties() override;
#endif

    private:

	// Array containing the Raw String data
	UPROPERTY()
	TArray<FString> StringCSVData;

	// Array containing all the CSV data converted to floats
	UPROPERTY()
	TArray<float> FloatCSVData;

	// Array containing the different time values for each particles in the file
	TArray<float> TimeValues;

	// Array containing the spawn times for each particles in the file
	TArray<float> SpawnTimes;

	// Array containing all the life values for each particles in the file 
	TArray<float> LifeValues;	

	// Index of the Position values in the buffer
	UPROPERTY()	    
	int32 PositionColumnIndex;

	// Index of the Normal values in the buffer
	UPROPERTY()
	int32 NormalColumnIndex;

	// Index of the time values in the buffer
	UPROPERTY()
	int32 TimeColumnIndex;

	// Index of the particle id values in the buffer
	UPROPERTY()
	int32 IDColumnIndex;

	// Index of the alive values in the buffer
	UPROPERTY()
	int32 AliveColumnIndex;

	// Index of the life values in the buffer
	UPROPERTY()
	int32 LifeColumnIndex;
};