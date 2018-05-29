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

#include "HoudiniCSV.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Math/NumericLimits.h"

#if WITH_EDITOR
#include "EditorFramework/AssetImportData.h"
#endif

#define LOCTEXT_NAMESPACE "HoudiniNiagaraCSVAsset"

DEFINE_LOG_CATEGORY(LogHoudiniNiagara);


struct FHoudiniCSVSortPredicate
{
	FHoudiniCSVSortPredicate(const int32 &InTimeCol, const int32 &InIDCol )
		: TimeColumnIndex( InTimeCol ), IDColumnIndex( InIDCol )
	{}

	bool operator()( const TArray<FString>& A, const TArray<FString>& B ) const
	{
		float ATime = TNumericLimits< float >::Lowest();
		if ( A.IsValidIndex( TimeColumnIndex ) )
			ATime = FCString::Atof( *A[ TimeColumnIndex ] );

		float BTime = TNumericLimits< float >::Lowest();
		if ( B.IsValidIndex( TimeColumnIndex ) )
			BTime = FCString::Atof( *B[ TimeColumnIndex ] );

		if ( ATime != BTime )
		{
			return ATime < BTime;
		}
		else
		{
			float AID = TNumericLimits< float >::Lowest();
			if ( A.IsValidIndex( IDColumnIndex ) )
				AID = FCString::Atof( *A[ IDColumnIndex ]);

			float BID = TNumericLimits< float >::Lowest();
			if ( B.IsValidIndex( IDColumnIndex ) )
				BID = FCString::Atof( *B[ IDColumnIndex ] );

			return AID <= BID;
		}
	}

	int32 TimeColumnIndex;
	int32 IDColumnIndex;
};
 
UHoudiniCSV::UHoudiniCSV( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer ),
	NumberOfLines( -1 ),
	NumberOfColumns( -1 ),
	NumberOfParticles( -1 )
{
	PositionColumnIndex = INDEX_NONE;
	NormalColumnIndex = INDEX_NONE;
	TimeColumnIndex = INDEX_NONE;
	IDColumnIndex = INDEX_NONE;
	AliveColumnIndex = INDEX_NONE;
	LifeColumnIndex = INDEX_NONE;
	ColorColumnIndex = INDEX_NONE;
	AlphaColumnIndex = INDEX_NONE;
	VelocityColumnIndex = INDEX_NONE;
}

void UHoudiniCSV::SetFileName( const FString& TheFileName )
{
    FileName = TheFileName;
}

bool UHoudiniCSV::UpdateFromFile( const FString& TheFileName )
{
    if ( TheFileName.IsEmpty() )
		return false;

    // Check the CSV file exists
    const FString FullCSVFilename = FPaths::ConvertRelativePathToFull( TheFileName );
    if ( !FPaths::FileExists( FullCSVFilename ) )
		return false;

    FileName = TheFileName;

    // Parse the file to a string array
    TArray<FString> StringArray;
    if ( !FFileHelper::LoadFileToStringArray( StringArray, *FullCSVFilename ) )
		return false;

    return UpdateFromStringArray( StringArray );
}

bool UHoudiniCSV::UpdateFromStringArray( TArray<FString>& RawStringArray )
{
    // Reset the CSV sizes
    NumberOfColumns = 0;
    NumberOfLines = 0;
	NumberOfParticles = 0;

    // Reset the position, normal and time indexes
    PositionColumnIndex = INDEX_NONE;
    NormalColumnIndex = INDEX_NONE;
    TimeColumnIndex = INDEX_NONE;
	IDColumnIndex = INDEX_NONE;
	AliveColumnIndex = INDEX_NONE;
	LifeColumnIndex = INDEX_NONE;
	ColorColumnIndex = INDEX_NONE;
	AlphaColumnIndex = INDEX_NONE;
	VelocityColumnIndex = INDEX_NONE;

    if ( RawStringArray.Num() <= 0 )
    {
		UE_LOG( LogHoudiniNiagara, Error, TEXT( "Could not load the CSV file, error: not enough lines." ) );
		return false;
    }

    // Remove empty lines from the CSV
    RawStringArray.RemoveAll( [&]( const FString& InString ) { return InString.IsEmpty(); } );

    // Number of lines in the CSV (ignoring the title line)
    NumberOfLines = RawStringArray.Num() - 1;
    if ( NumberOfLines < 1 )
    {
		UE_LOG( LogHoudiniNiagara, Error, TEXT( "Could not load the CSV file, error: not enough lines." ) );
		return false;
    }

	// Parses the CSV file's title line to update the column indexes of special values we're interested in
	// Also look for packed vectors in the first line and update the indexes accordingly
	bool HasPackedVectors = false;
	if ( !ParseCSVTitleLine( RawStringArray[0], RawStringArray[1], HasPackedVectors ) )
		return false;
    
    // Remove the title row now that it's been processed
    RawStringArray.RemoveAt( 0 );

	// Parses each string of the csv file to a string array
	TArray< TArray< FString > > ParsedStringArrays;
	ParsedStringArrays.SetNum( NumberOfLines );
	for ( int32 rowIdx = 0; rowIdx < NumberOfLines; rowIdx++ )
	{
		// Get the current line
		FString CurrentLine = RawStringArray[ rowIdx ];
		if ( HasPackedVectors )
		{
			// Clean up the packing characters: ()" from the line so it can be parsed properly
			CurrentLine.ReplaceInline( TEXT("("), TEXT("") );
			CurrentLine.ReplaceInline( TEXT(")"), TEXT("") );
			CurrentLine.ReplaceInline( TEXT("\""), TEXT("") );
		}

		// Parse the current line to an array
		TArray<FString> CurrentParsedLine;
		CurrentLine.ParseIntoArray( CurrentParsedLine, TEXT(",") );

		// Check the parsed line and number of columns match
		if ( NumberOfColumns != CurrentParsedLine.Num() )
			UE_LOG( LogHoudiniNiagara, Warning,
			TEXT("Error while parsing the CSV File. Line %d has %d values instead of the expected %d!"),
			rowIdx + 1, CurrentParsedLine.Num(), NumberOfColumns );

		// Store the parsed line
		ParsedStringArrays[ rowIdx ] = CurrentParsedLine;
	}

	// If we have time values, we have to make sure the lines are sorted by time
	if ( TimeColumnIndex != INDEX_NONE )
	{
		// First check if we need to sort the array
		bool NeedToSort = false;
		float PreviousTimeValue = 0.0f;
		for ( int32 rowIdx = 0; rowIdx < ParsedStringArrays.Num(); rowIdx++ )
		{
			if ( !ParsedStringArrays[ rowIdx ].IsValidIndex( TimeColumnIndex ) )
				continue;

			// Get the current time value
			float CurrentValue = FCString::Atof(*(ParsedStringArrays[ rowIdx ][ TimeColumnIndex ]));
			if ( rowIdx == 0 )
			{
				PreviousTimeValue = CurrentValue;
				continue;
			}

			// Time values arent sorted properly
			if ( PreviousTimeValue > CurrentValue )
			{
				NeedToSort = true;
				break;
			}
		}

		if ( NeedToSort )
		{
			// We need to sort the CSV lines by their time values
			ParsedStringArrays.Sort<FHoudiniCSVSortPredicate>( FHoudiniCSVSortPredicate( TimeColumnIndex, IDColumnIndex ) );
		}
	}

    // Initialize our different buffers
    FloatCSVData.Empty();
    FloatCSVData.SetNumZeroed( NumberOfLines * NumberOfColumns );
    
    StringCSVData.Empty();
    StringCSVData.SetNumZeroed( NumberOfLines * NumberOfColumns );

	// Due to the way that some of the DI functions work,
	// we expect that the particle IDs start at zero, and increment as the particles are spawned
	// Make sure this is the case by converting the particle IDs as we read them
	int32 NextParticleID = 0;
	TMap<float, int32> HoudiniIDToNiagaraIDMap;

	// We also keep track of the row indexes for each time values
	//float lastTimeValue = 0.0;
	//TimeValuesIndexes.Empty();

	// And the row indexes for each particle
	ParticleValueIndexes.Empty();

    // Extract all the values from the table to the float & string buffers
    TArray<FString> CurrentParsedLine;
    for ( int rowIdx = 0; rowIdx < ParsedStringArrays.Num(); rowIdx++ )
    {
		CurrentParsedLine = ParsedStringArrays[ rowIdx ];

		// Store the CSV Data in the buffers
		// The data is stored transposed in those buffers
		int32 CurrentID = -1;
		for ( int colIdx = 0; colIdx < NumberOfColumns; colIdx++ )
		{
			// Get the string value for the current column
			FString CurrentVal = TEXT("0");
			if ( CurrentParsedLine.IsValidIndex( colIdx ) )
			{
				CurrentVal = CurrentParsedLine[ colIdx ];
			}
			else
			{
				UE_LOG( LogHoudiniNiagara, Warning,
				TEXT("Error while parsing the CSV File. Line %d has an invalid value for column %d!"),
				rowIdx + 1, colIdx + 1 );
			}

			// Convert the string value to a float
			float FloatValue = FCString::Atof( *CurrentVal );

			// Handle particles IDs here
			if ( colIdx == IDColumnIndex )
			{
				// The particle ID may need to be replaced
				if ( !HoudiniIDToNiagaraIDMap.Contains( FloatValue ) )
				{
					// We found a new particle, add it to the ID map
					HoudiniIDToNiagaraIDMap.Add( FloatValue, NextParticleID++ );

					// Add a new array for that particle's indexes
					ParticleValueIndexes.Add( FParticleIndexes() );
				}

				// Get the Niagara ID from this Houdini ID
				CurrentID = HoudiniIDToNiagaraIDMap[ FloatValue ];
				FloatValue = (float)CurrentID;

				// Add the current row to this particle's row index list
				ParticleValueIndexes[ CurrentID ].RowIndexes.Add( rowIdx );
			}

			/*
			if ( colIdx == TimeColumnIndex )
			{
				// Keep track of new time values row indexes
				if ( ( FloatValue != lastTimeValue ) || ( rowIdx == 0 ) )
				{
					TimeValuesIndexes.Add( FloatValue, rowIdx );
					lastTimeValue = FloatValue;
				}
			}
			*/

			// Store the Value in the buffer
			FloatCSVData[ rowIdx + ( colIdx * NumberOfLines ) ] = FloatValue;

			// Keep the original string value in a buffer too
			StringCSVData[ rowIdx + ( colIdx * NumberOfLines ) ] = CurrentVal;
		}

		// Look at the particle ID, the max ID will be our number of particles
		if ( NumberOfParticles <= CurrentID )
			NumberOfParticles = CurrentID + 1;
    }
	
	NumberOfParticles = HoudiniIDToNiagaraIDMap.Num();
	if ( NumberOfParticles <= 0 )
		NumberOfParticles = NumberOfLines;

	// Look for particle specific attributes
	SpawnTimes.Empty();
	SpawnTimes.Init( -1.0f, NumberOfParticles );

	LifeValues.Empty();
	LifeValues.Init( -1.0f,  NumberOfParticles );

	float MaxTime = -1.0f;
	for ( int rowIdx = 0; rowIdx < NumberOfLines; rowIdx++ )
	{
		// Get the particle ID
		int32 CurrentID = rowIdx;
		if ( IDColumnIndex != INDEX_NONE )
			CurrentID = (int32)FloatCSVData[ rowIdx + ( IDColumnIndex * NumberOfLines ) ];

		// Get the time value for the current line
		float CurrentTime = 0.0f;
		if ( TimeColumnIndex != INDEX_NONE )
			CurrentTime = FloatCSVData[ rowIdx + ( TimeColumnIndex * NumberOfLines ) ];

		if ( LifeColumnIndex != INDEX_NONE )
		{
			// Set spawn time and life using life values
			float CurrentLife = FloatCSVData[ rowIdx + ( LifeColumnIndex * NumberOfLines ) ];
			if ( SpawnTimes[ CurrentID ] < 0.0f )
			{
				SpawnTimes[ CurrentID ] = CurrentTime;
				LifeValues[ CurrentID ] = CurrentLife;
			}
		}
		else if ( AliveColumnIndex != INDEX_NONE )
		{
			// Set spawn time and life using the alive bool values
			bool CurrentAlive = ( FloatCSVData[ rowIdx + ( AliveColumnIndex * NumberOfLines ) ] == 1.0f );
			if ( ( SpawnTimes[ CurrentID ] < 0.0f ) && CurrentAlive )
			{
				// Spawn time is when the particle is first seen alive
				SpawnTimes[ CurrentID ] = CurrentTime;
			}
			else if ( ( SpawnTimes[ CurrentID ] >= 0.0f ) && !CurrentAlive )
			{
				// Life is the difference between spawn time and time of death
				LifeValues[ CurrentID ] = CurrentTime - SpawnTimes[ CurrentID ];
			}
		}
		else
		{
			// No life or alive value, spawn time is the first time we see the particle
			if ( SpawnTimes [ CurrentID ] == INDEX_NONE )
				SpawnTimes[ CurrentID ] = CurrentTime;
		}
	}
    return true;
}

bool UHoudiniCSV::ParseCSVTitleLine( const FString& TitleLine, const FString& FirstLine, bool& HasPackedVectors )
{
	// Get the number of values per lines via the title line
    //FString TitleString = RawStringArray[ 0 ];
    TitleRowArray.Empty();
    TitleLine.ParseIntoArray( TitleRowArray, TEXT(",") );
    NumberOfColumns = TitleRowArray.Num();
    if ( NumberOfColumns < 1 )
    {
		UE_LOG( LogHoudiniNiagara, Error, TEXT( "Could not load the CSV file, error: not enough columns." ) );
		return false;
    }

    // Look for the position, normal and time attributes indexes
    for ( int32 n = 0; n < TitleRowArray.Num(); n++ )
    {
		// Remove spaces from the title row
		TitleRowArray[ n ].ReplaceInline( TEXT(" "), TEXT("") );

		FString CurrentTitle = TitleRowArray[ n ];
		if ( CurrentTitle.Equals( TEXT("P"), ESearchCase::IgnoreCase )
			|| CurrentTitle.Equals( TEXT("Px"), ESearchCase::IgnoreCase )
			|| CurrentTitle.Equals( TEXT("X"), ESearchCase::IgnoreCase )
			|| CurrentTitle.Equals(TEXT("pos"), ESearchCase::IgnoreCase ) )
		{
			if ( PositionColumnIndex == INDEX_NONE )
				PositionColumnIndex = n;
		}
		else if ( CurrentTitle.Equals( TEXT("N"), ESearchCase::IgnoreCase )
			|| CurrentTitle.Equals( TEXT("Nx"), ESearchCase::IgnoreCase ) )
		{
			if ( NormalColumnIndex == INDEX_NONE )
				NormalColumnIndex = n;
		}
		else if ( ( CurrentTitle.Equals( TEXT("T"), ESearchCase::IgnoreCase ) )
			|| ( CurrentTitle.Contains( TEXT("time"), ESearchCase::IgnoreCase ) ) )
		{
			if ( TimeColumnIndex == INDEX_NONE )
				TimeColumnIndex = n;
		}
		else if ( ( CurrentTitle.Equals( TEXT("#"), ESearchCase::IgnoreCase ) )
			|| ( CurrentTitle.Equals( TEXT("id"), ESearchCase::IgnoreCase ) ) )
		{
			if ( IDColumnIndex == INDEX_NONE )
				IDColumnIndex = n;
		}
		else if ( CurrentTitle.Equals( TEXT("alive"), ESearchCase::IgnoreCase ) )
		{
			if ( AliveColumnIndex == INDEX_NONE )
				AliveColumnIndex = n;
		}
		else if ( CurrentTitle.Equals( TEXT("life"), ESearchCase::IgnoreCase ) )
		{
			if ( LifeColumnIndex == INDEX_NONE)
				LifeColumnIndex = n;
		}
		else if ( ( CurrentTitle.Equals( TEXT("Cd"), ESearchCase::IgnoreCase ) )
			|| ( CurrentTitle.Equals(TEXT("color"), ESearchCase::IgnoreCase ) ) )
		{
			if ( ColorColumnIndex == INDEX_NONE )
				ColorColumnIndex = n;
		}
		else if ( ( CurrentTitle.Equals( TEXT("alpha"), ESearchCase::IgnoreCase ) )
			|| ( CurrentTitle.Equals(TEXT("A"), ESearchCase::IgnoreCase ) ) )
		{
			if ( AlphaColumnIndex == INDEX_NONE )
				AlphaColumnIndex = n;
		}
		else if ( ( CurrentTitle.Equals( TEXT("v"), ESearchCase::IgnoreCase ) )
			|| ( CurrentTitle.Equals(TEXT("Vx"), ESearchCase::IgnoreCase ) ) )
		{
			if ( VelocityColumnIndex == INDEX_NONE )
				VelocityColumnIndex = n;
		}
    }

	// Read the first line of the CSV file, and look for packed vectors value (X,Y,Z)
    // We'll have to expand them in the title row to match the parsed data
	HasPackedVectors = false;
	int32 FoundPackedVectorCharIndex = 0;    
    while ( FoundPackedVectorCharIndex != INDEX_NONE )
    {
		// Try to find ( in the line
		FoundPackedVectorCharIndex = FirstLine.Find( TEXT("("), ESearchCase::IgnoreCase, ESearchDir::FromStart, FoundPackedVectorCharIndex );
		if ( FoundPackedVectorCharIndex == INDEX_NONE )
			break;

		// We want to know which column this char belong to
		int32 FoundPackedVectorColumnIndex = INDEX_NONE;
		{
			// Chop the first line up to the found character
			FString FirstLineLeft = FirstLine.Left( FoundPackedVectorCharIndex );

			// ReplaceInLine returns the number of occurences of ",", that's what we want! 
			FoundPackedVectorColumnIndex = FirstLineLeft.ReplaceInline(TEXT(","), TEXT(""));
		}

		if ( !TitleRowArray.IsValidIndex( FoundPackedVectorColumnIndex ) )
		{
			UE_LOG( LogHoudiniNiagara, Warning,
			TEXT( "Error while parsing the CSV File. Couldn't unpack vector found at character %d in the first line!" ),
			FoundPackedVectorCharIndex + 1 );
			continue;
		}

		// We found a packed vector, get its size
		int32 FoundVectorSize = 0;
		{
			// Extract the vector string
			int32 FoundPackedVectorEndCharIndex = FirstLine.Find( TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, FoundPackedVectorCharIndex );
			FString VectorString = FirstLine.Mid( FoundPackedVectorCharIndex + 1, FoundPackedVectorEndCharIndex - FoundPackedVectorCharIndex - 1 );

			// Use ReplaceInLine to count the number of , to get the vector's size!
			FoundVectorSize = VectorString.ReplaceInline( TEXT(","), TEXT("") ) + 1;
		}

		if ( FoundVectorSize < 2 )
			continue;

		// Increment the number of columns
		NumberOfColumns += ( FoundVectorSize - 1 );

		// Expand TitleRowArray
		if ( ( FoundPackedVectorColumnIndex == PositionColumnIndex ) && ( FoundVectorSize == 3 ) )
		{
			// Expand P to Px,Py,Pz
			TitleRowArray[ PositionColumnIndex ] = TEXT("Px");
			TitleRowArray.Insert( TEXT( "Py" ), PositionColumnIndex + 1 );
			TitleRowArray.Insert( TEXT( "Pz" ), PositionColumnIndex + 2 );
		}
		else if ( ( FoundPackedVectorColumnIndex == NormalColumnIndex ) && ( FoundVectorSize == 3 ) )
		{
			// Expand N to Nx,Ny,Nz
			TitleRowArray[ NormalColumnIndex ] = TEXT("Nx");
			TitleRowArray.Insert( TEXT("Ny"), NormalColumnIndex + 1 );
			TitleRowArray.Insert( TEXT("Nz"), NormalColumnIndex + 2 );
		}
		else if ( ( FoundPackedVectorColumnIndex == VelocityColumnIndex ) && ( FoundVectorSize == 3 ) )
		{
			// Expand V to Vx,Vy,Vz
			TitleRowArray[ VelocityColumnIndex ] = TEXT("Vx");
			TitleRowArray.Insert( TEXT("Vy"), VelocityColumnIndex + 1 );
			TitleRowArray.Insert( TEXT("Vz"), VelocityColumnIndex + 2 );
		}
		else if ( ( FoundPackedVectorColumnIndex == ColorColumnIndex ) && ( ( FoundVectorSize == 3 ) || ( FoundVectorSize == 4 ) ) )
		{
			// Expand Cd to R, G, B 
			TitleRowArray[ ColorColumnIndex ] = TEXT("R");
			TitleRowArray.Insert( TEXT("G"), ColorColumnIndex + 1 );
			TitleRowArray.Insert( TEXT("B"), ColorColumnIndex + 2 );

			if ( FoundVectorSize == 4 )
			{
				// Insert A if we had RGBA
				TitleRowArray.Insert( TEXT("A"), ColorColumnIndex + 3 );
				if ( AlphaColumnIndex == INDEX_NONE )
					AlphaColumnIndex = ColorColumnIndex + 3;
			}
		}
		else
		{
			// Expand the vector's title from V to V, V1, V2, V3 ...
			FString FoundPackedVectortTitle = TitleRowArray[ FoundPackedVectorColumnIndex ];
			for ( int32 n = 1; n < FoundVectorSize; n++ )
			{
				FString CurrentTitle = FoundPackedVectortTitle + FString::FromInt( n );
				TitleRowArray.Insert( CurrentTitle, FoundPackedVectorColumnIndex + n );
			}
		}

		// Eventually offset the stored index
		if ( PositionColumnIndex != INDEX_NONE && ( PositionColumnIndex > FoundPackedVectorColumnIndex) )
			PositionColumnIndex += FoundVectorSize - 1;

		if ( NormalColumnIndex != INDEX_NONE && ( NormalColumnIndex > FoundPackedVectorColumnIndex) )
			NormalColumnIndex += FoundVectorSize - 1;

		if ( TimeColumnIndex != INDEX_NONE && ( TimeColumnIndex > FoundPackedVectorColumnIndex) )
			TimeColumnIndex += FoundVectorSize - 1;

		if ( IDColumnIndex != INDEX_NONE && ( IDColumnIndex > FoundPackedVectorColumnIndex ) )
			IDColumnIndex += FoundVectorSize - 1;

		if ( AliveColumnIndex != INDEX_NONE && ( AliveColumnIndex > FoundPackedVectorColumnIndex ) )
			AliveColumnIndex += FoundVectorSize - 1;

		if ( LifeColumnIndex != INDEX_NONE && ( LifeColumnIndex > FoundPackedVectorColumnIndex ) )
			LifeColumnIndex += FoundVectorSize - 1;

		if ( ColorColumnIndex != INDEX_NONE && ( ColorColumnIndex > FoundPackedVectorColumnIndex ) )
			ColorColumnIndex += FoundVectorSize - 1;

		if ( AlphaColumnIndex != INDEX_NONE && ( AlphaColumnIndex > FoundPackedVectorColumnIndex ) )
			AlphaColumnIndex += FoundVectorSize - 1;

		if ( VelocityColumnIndex != INDEX_NONE && ( VelocityColumnIndex > FoundPackedVectorColumnIndex ) )
			VelocityColumnIndex += FoundVectorSize - 1;

		HasPackedVectors = true;
		FoundPackedVectorCharIndex++;
    }

    // For sanity, Check that the number of columns matches the title row and the first line
    {
		// Check the title row
		if ( NumberOfColumns != TitleRowArray.Num() )
			UE_LOG( LogHoudiniNiagara, Error,
			TEXT( "Error while parsing the CSV File. Found %d columns but the Title string has %d values! Some values will have an offset!" ),
			NumberOfColumns, TitleRowArray.Num() );

		// Use ReplaceInLine to count the number of columns in the first line and make sure it's correct
		FString FirstLineCopy = FirstLine;
		int32 FirstLineColumns = FirstLineCopy.ReplaceInline( TEXT(","), TEXT("") ) + 1;
		if ( NumberOfColumns != FirstLineColumns )
			UE_LOG( LogHoudiniNiagara, Error,
			TEXT("Error while parsing the CSV File. Found %d columns but found %d values in the first line! Some values will have an offset!" ),
			NumberOfColumns, FirstLineColumns );
    }

	return true;
}


// Returns the float value at a given point in the CSV file
bool UHoudiniCSV::GetCSVFloatValue( const int32& lineIndex, const int32& colIndex, float& value )
{
    if ( lineIndex < 0 || lineIndex >= NumberOfLines )
		return false;

    if ( colIndex < 0 || colIndex >= NumberOfColumns )
		return false;

    int32 Index = lineIndex + ( colIndex * NumberOfLines );
    if ( FloatCSVData.IsValidIndex( Index ) )
    {
		value = FloatCSVData[ Index ];
		return true;
    }

    return false;
}

// Returns the float value at a given point in the CSV file
bool UHoudiniCSV::GetCSVStringValue( const int32& lineIndex, const int32& colIndex, FString& value )
{
    if ( lineIndex < 0 || lineIndex >= NumberOfLines )
		return false;

    if ( colIndex < 0 || colIndex >= NumberOfColumns )
		return false;

    int32 Index = lineIndex + ( colIndex * NumberOfLines );
    if ( StringCSVData.IsValidIndex( Index ) )
    {
		value = StringCSVData[ Index ];
		return true;
    }

    return false;
}

// Returns a Vector 3 for a given point in the CSV file
bool UHoudiniCSV::GetCSVVectorValue( const int32& lineIndex, const int32& colIndex, FVector& value, const bool& DoSwap, const bool& DoScale )
{
    FVector V = FVector::ZeroVector;
    if ( !GetCSVFloatValue( lineIndex, colIndex, V.X ) )
		return false;

    if ( !GetCSVFloatValue( lineIndex, colIndex + 1, V.Y ) )
		return false;

    if ( !GetCSVFloatValue( lineIndex, colIndex + 2, V.Z ) )
		return false;

    if ( DoScale )
		V *= 100.0f;

    value = V;

    if ( DoSwap )
    {
		value.Y = V.Z;
		value.Z = V.Y;
    }

    return true;
}

// Returns a Vector 3 for a given point in the CSV file
bool UHoudiniCSV::GetCSVPositionValue( const int32& lineIndex, FVector& value )
{
    FVector V = FVector::ZeroVector;
    if ( !GetCSVVectorValue( lineIndex, PositionColumnIndex, V, true, true ) )
		return false;

    value = V;

    return true;
}

// Returns a Vector 3 for a given point in the CSV file
bool UHoudiniCSV::GetCSVNormalValue( const int32& lineIndex, FVector& value )
{
    FVector V = FVector::ZeroVector;
    if ( !GetCSVVectorValue(lineIndex, NormalColumnIndex, V, true, false ) )
		return false;

    value = V;

    return true;
}

// Returns a time value for a given point in the CSV file
bool UHoudiniCSV::GetCSVTimeValue( const int32& lineIndex, float& value )
{
    float temp;
    if ( !GetCSVFloatValue( lineIndex, TimeColumnIndex, temp ) )
		return false;

    value = temp;

    return true;
}

// Returns a Color for a given point in the CSV file
bool UHoudiniCSV::GetCSVColorValue( const int32& lineIndex, FLinearColor& value )
{
	FVector V = FVector::OneVector;
	if ( !GetCSVVectorValue( lineIndex, ColorColumnIndex, V, false, false ) )
		return false;

	FLinearColor C = FLinearColor::White;
	C.R = V.X;
	C.G = V.Y;
	C.B = V.Z;

	float alpha = 1.0f;
	if ( GetCSVFloatValue( lineIndex, AlphaColumnIndex, alpha ) )
		C.A = alpha;	

	value = C;

	return true;
}

// Returns a Velocity Vector3 for a given point in the CSV file
bool UHoudiniCSV::GetCSVVelocityValue( const int32& lineIndex, FVector& value )
{
	FVector V = FVector::ZeroVector;
	if ( !GetCSVVectorValue( lineIndex, VelocityColumnIndex, V, true, false ) )
		return false;

	value = V;

	return true;
}

// Returns the number of points found in the CSV file
int32 UHoudiniCSV::GetNumberOfParticlesInCSV()
{
	if ( IDColumnIndex != INDEX_NONE )
		return NumberOfParticles;

    return NumberOfLines;
}

// Returns the number of lines found in the CSV file
int32 UHoudiniCSV::GetNumberOfLinesInCSV()
{
	return NumberOfLines;
}

// Returns the number of columns found in the CSV file
int32 UHoudiniCSV::GetNumberOfColumnsInCSV()
{
	return NumberOfColumns;
}

// Get the last row index for a given time value (the row with a time smaller or equal to desiredTime)
// If the CSV file doesn't have time informations, returns false and set the LastRowIndex to the last line in the file
// If desiredTime is smaller than the time value in the first row, LastRowIndex will be set to -1
// If desiredTime is higher than the last time value in the last row of the csv file, LastIndex will be set to the last row's index
bool UHoudiniCSV::GetLastRowIndexAtTime(const float& desiredTime, int32& lastRowIndex)
{
	// If we dont have proper time info, always return the last line
	if (TimeColumnIndex < 0 || TimeColumnIndex >= NumberOfColumns)
	{
		lastRowIndex = NumberOfLines - 1;
		return false;
	}

	float temp_time = 0.0f;
	// Check first if we have anything to spawn at the current time by looking at the last value
	if ( GetCSVTimeValue( NumberOfLines - 1, temp_time ) && temp_time < desiredTime )
	{
		// We didn't find a suitable index because the desired time is higher than our last time value
		lastRowIndex = NumberOfLines - 1;
		return true;
	}

	// Iterates through all the lines
	lastRowIndex = INDEX_NONE;
	for ( int32 n = 0; n < NumberOfLines; n++ )
	{
		if ( GetCSVTimeValue( n, temp_time ) )
		{
			if ( temp_time == desiredTime )
			{
				lastRowIndex = n;
			}
			else if ( temp_time > desiredTime )
			{
				lastRowIndex = n - 1;
				return true;
			}
		}
	}

	// We didn't find a suitable index because the desired time is higher than our last time value
	if ( lastRowIndex == INDEX_NONE )
		lastRowIndex = NumberOfLines - 1;

	return true;
}
// Get the last index of the particles with a time value smaller or equal to desiredTime
// If the CSV file doesn't have time informations, returns false and set the LastIndex to the last particle
// If desiredTime is smaller than the first particle, LastIndex will be set to -1
// If desiredTime is higher than the last particle in the csv file, LastIndex will be set to the last particle's index
bool UHoudiniCSV::GetLastParticleIndexAtTime( const float& desiredTime, int32& lastID )
{
    // If we dont have proper time info, always return the last particle
    if ( TimeColumnIndex < 0 || TimeColumnIndex >= NumberOfColumns )
    {
		lastID = NumberOfParticles - 1;
		return false;
    }

    float temp_time = 0.0f;
	if ( !SpawnTimes.IsValidIndex( NumberOfParticles - 1 ) )
	{
		lastID = NumberOfParticles - 1;
		return false;
	}

	if ( SpawnTimes[ NumberOfParticles - 1 ] < desiredTime )
	{
		// We didn't find a suitable index because the desired time is higher than our last time value
		lastID = NumberOfParticles - 1;
		return true;
	}

	// Iterates through all the particles
	lastID = INDEX_NONE;
	for ( int32 n = 0; n < NumberOfParticles; n++ )
	{
		temp_time = SpawnTimes[ n ];

		if ( temp_time == desiredTime )
		{
			lastID = n;
		}
		else if ( temp_time > desiredTime )
		{
			lastID = n - 1;
			return true;
		}
	}

	return true;
}

bool UHoudiniCSV::GetParticleLifeAtTime( const int32& ParticleID, const float& DesiredTime, float& Value )
{
	if ( !SpawnTimes.IsValidIndex( ParticleID )  || !LifeValues.IsValidIndex( ParticleID ) )
	{
		Value = -1.0f;
		return false;
	}

	if ( DesiredTime < SpawnTimes[ ParticleID ] )
	{
		Value = LifeValues[ ParticleID ];
	}
	else
	{
		Value = LifeValues[ ParticleID ] - ( DesiredTime - SpawnTimes[ ParticleID ] );
	}

	return true;
}

bool UHoudiniCSV::GetParticleValueAtTime( const int32& ParticleID, const int32& ColumnIndex, const float& desiredTime, float& Value )
{
	int32 PrevIndex = -1;
	int32 NextIndex = -1;
	float PrevWeight = 1.0f;

	if ( !GetParticleLineIndexAtTime( ParticleID, desiredTime, PrevIndex, NextIndex, PrevWeight ) )
		return false;

	float PrevValue, NextValue;
	if ( !GetCSVFloatValue( PrevIndex, ColumnIndex, PrevValue ) )
		return false;
	if ( !GetCSVFloatValue( NextIndex, ColumnIndex, NextValue ) )
		return false;

	Value = FMath::Lerp( PrevValue, NextValue, PrevWeight );

	return true;
}

bool UHoudiniCSV::GetParticlePositionAtTime( const int32& ParticleID, const float& desiredTime, FVector& Vector )
{
	FVector V = FVector::ZeroVector;
	if ( !GetParticleVectorValueAtTime(ParticleID, PositionColumnIndex, desiredTime, V, true, true ) )
		return false;

	Vector = V;

	return true;
}

bool UHoudiniCSV::GetParticleVectorValueAtTime( const int32& ParticleID, const int32& ColumnIndex, const float& desiredTime, FVector& Vector, const bool& DoSwap, const bool& DoScale )
{
	int32 PrevIndex = -1;
	int32 NextIndex = -1;
	float PrevWeight = 1.0f;

	if ( !GetParticleLineIndexAtTime( ParticleID, desiredTime, PrevIndex, NextIndex, PrevWeight ) )
		return false;

	FVector PrevVector, NextVector;
	if ( !GetCSVVectorValue( PrevIndex, ColumnIndex, PrevVector, DoSwap, DoScale ) )
		return false;
	if ( !GetCSVVectorValue( NextIndex, ColumnIndex, NextVector, DoSwap, DoScale ) )
		return false;

	Vector = FMath::Lerp(PrevVector, NextVector, PrevWeight);

	return true;
}

bool UHoudiniCSV::GetParticleLineIndexAtTime(const int32& ParticleID, const float& desiredTime, int32& PrevIndex, int32& NextIndex, float& PrevWeight )
{
	float PrevTime = -1.0f;
	float NextTime = -1.0f;

	// Invalid Particle ID
	if ( ParticleID < 0 || ParticleID >= NumberOfParticles )
		return false;

	// Get the spawn time
	float ParticleSpawnTime = 0.0f;
	if ( SpawnTimes.IsValidIndex( ParticleID ) )
		ParticleSpawnTime = SpawnTimes[ ParticleID ];

	// If particle hasn't spawn before DesiredTime
	if ( ParticleSpawnTime > desiredTime )
		return false;

	// Get the life value
	float ParticleLife = 0.0f;
	if ( LifeValues.IsValidIndex( ParticleID ) )
		ParticleLife = LifeValues[ ParticleID ];

	// If particle is dead before DesiredTime
	if ( ParticleLife > 0.0f && ( ParticleSpawnTime + ParticleLife < desiredTime ) )	
		return false;

	// We don't have Id information
	// return ParticleId for the rowIndexes ??
	if ( IDColumnIndex == INDEX_NONE )
		return false;

	// We don't have time information
	if ( TimeColumnIndex == INDEX_NONE )
		return false;

	// Get the row indexes for this particle
	TArray<int32>* RowIndexes = nullptr;
	if ( ParticleValueIndexes.IsValidIndex( ParticleID ) )
		RowIndexes = &( ParticleValueIndexes[ ParticleID ].RowIndexes );

	if ( !RowIndexes )
		return false;

	for ( auto n : *RowIndexes )
	{
		// Get the time
		float currentTime = -1.0f;
		if ( GetCSVTimeValue( n, currentTime) )
		{
			if ( currentTime == desiredTime )
			{
				PrevIndex = n;
				NextIndex = n;
				PrevWeight = 1.0f;
				return true;
			}
			else if ( currentTime < desiredTime )
			{
				if ( PrevTime == -1.0f || PrevTime < currentTime )
				{
					PrevIndex = n;
					PrevTime = currentTime;
				}
			}
			else
			{
				if ( NextTime == -1.0f || NextTime > currentTime)
				{
					NextIndex = n;
					NextTime = currentTime;

					// TODO: since the csv is sorted by time, we can break now
					break;
				}
			}
		}
	}

	if ( PrevIndex < 0 && NextIndex < 0 )
		return false;

	if ( PrevIndex < 0 )
	{
		PrevWeight = 0.0f;
		PrevIndex = NextIndex;
		return true;
	}

	if ( NextIndex < 0 )
	{
		PrevWeight = 1.0f;
		NextIndex = PrevIndex;
		return true;
	}

	// Calculate the weight
	PrevWeight = ( ( desiredTime - PrevTime) / ( NextTime - PrevTime ) );

	return true;
}



// Returns the column index for a given string
bool UHoudiniCSV::GetColumnIndexFromString( const FString& ColumnTitle, int32& ColumnIndex )
{
    if ( !TitleRowArray.Find( ColumnTitle, ColumnIndex ) )
		return true;

    // Handle packed positions/normals here
    if ( ColumnTitle.Equals( "P" ) )
		return TitleRowArray.Find( TEXT( "Px" ), ColumnIndex );
    else if ( ColumnTitle.Equals( "N" ) )
		return TitleRowArray.Find( TEXT( "Nx" ), ColumnIndex );

    return false;
}

// Returns the float value at a given point in the CSV file
bool UHoudiniCSV::GetCSVFloatValue( const int32& lineIndex, const FString& ColumnTitle, float& value )
{
    int32 ColIndex = -1;
    if ( !GetColumnIndexFromString( ColumnTitle, ColIndex ) )
		return false;

    return GetCSVFloatValue( lineIndex, ColIndex, value );
}


// Returns the string value at a given point in the CSV file
bool UHoudiniCSV::GetCSVStringValue( const int32& lineIndex, const FString& ColumnTitle, FString& value )
{
    int32 ColIndex = -1;
    if ( !GetColumnIndexFromString( ColumnTitle, ColIndex ) )
		return false;

    return GetCSVStringValue( lineIndex, ColIndex, value );
}

#if WITH_EDITORONLY_DATA
void UHoudiniCSV::PostInitProperties()
{
    if ( !HasAnyFlags( RF_ClassDefaultObject ) )
    {
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
    }

    Super::PostInitProperties();
}
#endif

#undef LOCTEXT_NAMESPACE
