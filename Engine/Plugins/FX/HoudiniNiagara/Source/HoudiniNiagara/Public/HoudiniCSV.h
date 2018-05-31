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


USTRUCT()
struct FPointIndexes
{
	GENERATED_BODY()

	// Simple structure for storing all the row indexes used for a given point
	UPROPERTY()
	TArray<int32> RowIndexes;
};

UCLASS()
class HOUDININIAGARA_API UHoudiniCSV : public UObject
{
    GENERATED_UCLASS_BODY()
 
    public:
	
	//-----------------------------------------------------------------------------------------
	//  MEMBER FUNCTIONS
	//-----------------------------------------------------------------------------------------

	virtual void GetAssetRegistryTags(TArray< FAssetRegistryTag > & OutTags) const override;

	bool UpdateFromFile( const FString& TheFileName );
	bool UpdateFromStringArray( TArray<FString>& StringArray );

	void SetFileName( const FString& TheFilename );

	// Returns the number of points found in the CSV file
	int32 GetNumberOfPoints();

	// Returns the number of rows found in the CSV file
	int32 GetNumberOfRows();

	// Returns the number of columns found in the CSV file
	int32 GetNumberOfColumns();

	// Returns the column index for a given string
	bool GetColumnIndexFromString(const FString& ColumnTitle, int32& ColumnIndex);

	// Returns the float value at a given point in the CSV file
	bool GetFloatValue( const int32& rowIndex, const int32& colIndex, float& value );
	// Returns the float value at a given point in the CSV file
	bool GetFloatValue( const int32& rowIndex, const FString& ColumnTitle, float& value );
	/*
	// Returns the float value at a given point in the CSV file
	bool GetCSVStringValue( const int32& rowIndex, const int32& colIndex, FString& value );
	// Returns the string value at a given point in the CSV file
	bool GetCSVStringValue( const int32& rowIndex, const FString& ColumnTitle, FString& value );
	*/
	// Returns a Vector3 for a given point in the CSV file
	bool GetVectorValue( const int32& rowIndex, const int32& colIndex, FVector& value, const bool& DoSwap = true, const bool& DoScale = true );
	
	// Returns a time value for a given point in the CSV file
	bool GetTimeValue( const int32& rowIndex, float& value );
	// Returns a Position Vector3 for a given point in the CSV file (converted to unreal's coordinate system)
	bool GetPositionValue( const int32& rowIndex, FVector& value );
	// Returns a Normal Vector3 for a given point in the CSV file (converted to unreal's coordinate system)
	bool GetNormalValue( const int32& rowIndex, FVector& value );
	// Returns a Color for a given point in the CSV file
	bool GetColorValue( const int32& rowIndex, FLinearColor& value );
	// Returns a Velocity Vector3 for a given point in the CSV file
	bool GetVelocityValue(const int32& rowIndex, FVector& value );

	// Get the last row index for a given time value (the row with a time smaller or equal to desiredTime)
	// If the CSV file doesn't have time informations, returns false and set LastRowIndex to the last row in the file
	// If desiredTime is smaller than the time value in the first row, LastRowIndex will be set to -1
	// If desiredTime is higher than the last time value in the last row of the csv file, LastIndex will be set to the last row's index
	bool GetLastRowIndexAtTime( const float& desiredTime, int32& lastRowIndex );

	// Get the last pointID of the points to be spawned at time t
	// Invalid Index are used to indicate edge cases:
	// -1 will be returned if there is no points to spawn ( t is smaller than the first point time )
	// NumberOfRows will be returned if all points in the CSV have been spawned ( t is higher than the last point time )
	bool GetLastPointIDToSpawnAtTime( const float& time, int32& lastIndex );

	// Returns the previous and next indexes for reading the values of a specified point at a given time
	bool GetRowIndexesForPointAtTime(const int32& PointID, const float& desiredTime, int32& PrevIndex, int32& NextIndex, float& PrevWeight);
	// Returns the value for a point at a given time value (linearly interpolated) 
	bool GetPointValueAtTime(const int32& PointID, const int32& ColumnIndex, const float& desiredTime, float& Value);
	// Returns the Vector Value for a given point at a given time value (linearly interpolated) 
	bool GetPointVectorValueAtTime(const int32& PointID, const int32& ColumnIndex, const float& desiredTime, FVector& Vector, const bool& DoSwap, const bool& DoScale);
	// Returns the Position Value for a given point at a given time value (linearly interpolated) 
	bool GetPointPositionAtTime(const int32& PointID, const float& desiredTime, FVector& Vector);
	// Return a given point's life value at spawn
	bool GetPointLife(const int32& PointID, float& Value);
	// Return a point's life for a given time value
	bool GetPointLifeAtTime(const int32& PointID, const float& DesiredTime, float& Value);
	// Return a point's type at spawn
	bool GetPointType(const int32& PointID, int32& Value);

	//-----------------------------------------------------------------------------------------
	//  MEMBER VARIABLES
	//-----------------------------------------------------------------------------------------
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties" )
	FString FileName;

	// The number of values stored in the CSV file (excluding the title row)
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties" )
	int32 NumberOfRows;

	// The number of value TYPES stored in the CSV file
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties" )
	int32 NumberOfColumns;

	// The number of points found in the CSV file
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties")
	int32 NumberOfPoints;

	// The source title row, describing the content of each column and used to locate specific values in the CSV file.
	// Editing this will trigger a re-import of the CSV file.
	UPROPERTY(EditAnywhere, Category = "Houdini CSV File Properties")
	FString SourceTitleRow;

	// The final column titles used by the asset after parsing.
	// Describes the content of each column.
	// Packed vector values are expended, so additional column might have been inserted.
	// Use the indexes in this array to access your data.
	UPROPERTY( VisibleAnywhere, Category = "Houdini CSV File Properties" )
	TArray<FString> ColumnTitleArray;

#if WITH_EDITORONLY_DATA
	/** Importing data and options used for this asset */
	UPROPERTY( EditAnywhere, Instanced, Category = ImportSettings )
	class UAssetImportData* AssetImportData;

	virtual void PostInitProperties() override;

	virtual void PostEditChangeProperty( FPropertyChangedEvent & PropertyChangedEvent ) override;
#endif

	protected:

	// Parses the CSV title row to update the column indexes of special values we're interested in
	// Also look for packed vectors in the first row and update the indexes accordingly
	bool ParseCSVTitleRow(const FString& TitleRow, const FString& FirstValueRow, bool& HasPackedVectors);

    private:

	static bool SortPredicate(const TArray<FString>& A, const TArray<FString>& B);

	// Array containing the Raw String data
	//UPROPERTY()
	//TArray<FString> StringCSVData;

	// Array containing all the CSV data converted to floats
	UPROPERTY()
	TArray<float> FloatCSVData;
	
	// Array containing the spawn times for each point in the file
	UPROPERTY()
	TArray<float> SpawnTimes;

	// Array containing all the life values for each point in the file
	UPROPERTY()
	TArray<float> LifeValues;

	// Array containing all the type values for each point in the file
	UPROPERTY()
	TArray<int32> PointTypes;

	// Index of the Position values in the buffer
	UPROPERTY()
	int32 PositionColumnIndex;

	// Index of the Normal values in the buffer
	UPROPERTY()
	int32 NormalColumnIndex;

	// Index of the time values in the buffer
	UPROPERTY()
	int32 TimeColumnIndex;

	// Index of the point id values in the buffer
	UPROPERTY()
	int32 IDColumnIndex;

	// Index of the alive values in the buffer
	UPROPERTY()
	int32 AliveColumnIndex;

	// Index of the life values in the buffer
	UPROPERTY()
	int32 LifeColumnIndex;

	// Index of the color values in the buffer
	UPROPERTY()
	int32 ColorColumnIndex;

	// Index of the color values in the buffer
	UPROPERTY()
	int32 AlphaColumnIndex;

	// Index of the velocity values in the buffer
	UPROPERTY()
	int32 VelocityColumnIndex;

	// Column index of the "type" values in the buffer
	UPROPERTY()
	int32 TypeColumnIndex;

	// Row indexes for new time values
	//UPROPERTY()
	//TMap<float, int32> TimeValuesIndexes;

	// Row indexes for each point
	UPROPERTY()
	TArray< FPointIndexes > PointValueIndexes;

	UPROPERTY()
	bool UseCustomTitleRow;
};