// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Presentation/PropertyTable/PropertyTableColumn.h"
#include "Editor/EditorEngine.h"
#include "IPropertyTableCell.h"
#include "UObject/TextProperty.h"
#include "ObjectPropertyNode.h"
#include "Presentation/PropertyTable/PropertyTableCell.h"
#include "Presentation/PropertyTable/DataSource.h"
#include "PropertyEditorHelpers.h"

#define LOCTEXT_NAMESPACE "PropertyTableColumn"

struct FCompareRowByColumnBase
{
	virtual int32 Compare(const TSharedRef< IPropertyTableRow >& Lhs, const TSharedRef< IPropertyTableRow >& Rhs) const = 0;
	virtual ~FCompareRowByColumnBase() {}
};

struct FCompareRowPrimaryAndSecondary
{
	FCompareRowPrimaryAndSecondary(FCompareRowByColumnBase* InPrimarySort, FCompareRowByColumnBase* InSecondarySort)
		: PrimarySort(InPrimarySort)
		, SecondarySort(InSecondarySort)
	{}
	
	bool operator()(const TSharedRef< IPropertyTableRow >& Lhs, const TSharedRef< IPropertyTableRow >& Rhs) const
	{
		const int32 PrimaryResult = PrimarySort->Compare(Lhs, Rhs);
		if (PrimaryResult != 0 || !SecondarySort)
		{
			return PrimaryResult < 0;
		}
		else
		{
			return SecondarySort->Compare(Lhs, Rhs) < 0;
		}
	}
private:
	FCompareRowByColumnBase* PrimarySort;
	FCompareRowByColumnBase* SecondarySort;

};



template< typename UPropertyType >
struct FCompareRowByColumnAscending : public FCompareRowByColumnBase
{
public:
	FCompareRowByColumnAscending( const TSharedRef< IPropertyTableColumn >& InColumn, const UPropertyType* InUProperty )
		: Property( InUProperty )
		, Column( InColumn )
	{

	}

	int32 Compare( const TSharedRef< IPropertyTableRow >& Lhs, const TSharedRef< IPropertyTableRow >& Rhs ) const
	{
		const TSharedRef< IPropertyTableCell > LhsCell = Column->GetCell( Lhs );
		const TSharedRef< IPropertyTableCell > RhsCell = Column->GetCell( Rhs );

		const TSharedPtr< FPropertyNode > LhsPropertyNode = LhsCell->GetNode();
		if ( !LhsPropertyNode.IsValid() )
		{
			return 1;
		}

		const TSharedPtr< FPropertyNode > RhsPropertyNode = RhsCell->GetNode();
		if ( !RhsPropertyNode.IsValid() )
		{
			return -1;
		}

		const TSharedPtr< IPropertyHandle > LhsPropertyHandle = PropertyEditorHelpers::GetPropertyHandle( LhsPropertyNode.ToSharedRef(), NULL, NULL );
		if ( !LhsPropertyHandle.IsValid() )
		{
			return 1;
		}

		const TSharedPtr< IPropertyHandle > RhsPropertyHandle = PropertyEditorHelpers::GetPropertyHandle( RhsPropertyNode.ToSharedRef(), NULL, NULL );
		if ( !RhsPropertyHandle.IsValid() )
		{
			return -1;
		}

		return ComparePropertyValue( LhsPropertyHandle, RhsPropertyHandle );
	}

private:

	int32 ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
	{
		typename UPropertyType::TCppType LhsValue; 
		LhsPropertyHandle->GetValue( LhsValue );

		typename UPropertyType::TCppType RhsValue; 
		RhsPropertyHandle->GetValue( RhsValue );

		if (LhsValue < RhsValue)
		{
			return -1;
		}
		else if (LhsValue > RhsValue)
		{
			return 1;
		}

		return 0;
	}

private:

	const UPropertyType* Property;
	TSharedRef< IPropertyTableColumn > Column;
};

template< typename UPropertyType >
struct FCompareRowByColumnDescending : public FCompareRowByColumnBase
{
public:
	FCompareRowByColumnDescending( const TSharedRef< IPropertyTableColumn >& InColumn, const UPropertyType* InUProperty )
		: Comparer( InColumn, InUProperty )
	{

	}

	int32 Compare( const TSharedRef< IPropertyTableRow >& Lhs, const TSharedRef< IPropertyTableRow >& Rhs ) const override
	{
		return Comparer.Compare(Rhs, Lhs);
	}


private:

	const FCompareRowByColumnAscending< UPropertyType > Comparer;
};

struct FCompareRowByColumnUsingExportTextLexicographic : public FCompareRowByColumnBase
{
public:
	FCompareRowByColumnUsingExportTextLexicographic( const TSharedRef< IPropertyTableColumn >& InColumn, const UProperty* InUProperty, bool InAscendingOrder )
		: Property( InUProperty )
		, Column( InColumn )
		, bAscending( InAscendingOrder )
	{

	}

	int32 Compare( const TSharedRef< IPropertyTableRow >& Lhs, const TSharedRef< IPropertyTableRow >& Rhs ) const
	{
		const TSharedRef< IPropertyTableCell > LhsCell = Column->GetCell( Lhs );
		const TSharedRef< IPropertyTableCell > RhsCell = Column->GetCell( Rhs );

		const TSharedPtr< FPropertyNode > LhsPropertyNode = LhsCell->GetNode();
		if ( !LhsPropertyNode.IsValid() )
		{
			return 1;
		}

		const TSharedPtr< FPropertyNode > RhsPropertyNode = RhsCell->GetNode();
		if ( !RhsPropertyNode.IsValid() )
		{
			return -1;
		}

		const TSharedPtr< IPropertyHandle > LhsPropertyHandle = PropertyEditorHelpers::GetPropertyHandle( LhsPropertyNode.ToSharedRef(), NULL, NULL );
		if ( !LhsPropertyHandle.IsValid() )
		{
			return 1;
		}

		const TSharedPtr< IPropertyHandle > RhsPropertyHandle = PropertyEditorHelpers::GetPropertyHandle( RhsPropertyNode.ToSharedRef(), NULL, NULL );
		if ( !RhsPropertyHandle.IsValid() )
		{
			return -1;
		}

		return ComparePropertyValue( LhsPropertyHandle, RhsPropertyHandle );
	}

private:

	int32 ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
	{
		FString LhsValue; 
		LhsPropertyHandle->GetValueAsDisplayString( LhsValue );

		FString RhsValue; 
		RhsPropertyHandle->GetValueAsDisplayString( RhsValue );

		if (LhsValue < RhsValue)
		{
			return bAscending ? -1 : 1;
		}
		else if (LhsValue > RhsValue)
		{
			return bAscending ? 1: -1;
		}

		return 0;
	}

private:

	const UProperty* Property;
	TSharedRef< IPropertyTableColumn > Column;
	bool bAscending;
};



template<>
FORCEINLINE int32 FCompareRowByColumnAscending<UEnumProperty>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	// Only Bytes work right now

	// Get the basic uint8 values
	uint8 LhsValue; 
	LhsPropertyHandle->GetValue( LhsValue );

	uint8 RhsValue; 
	RhsPropertyHandle->GetValue( RhsValue );

	// Bytes are trivially sorted numerically
	UEnum* PropertyEnum = Property->GetEnum();

	// Enums are sorted alphabetically based on the full enum entry name - must be sure that values are within Enum bounds!
	int32 LhsIndex = PropertyEnum->GetIndexByValue(LhsValue);
	int32 RhsIndex = PropertyEnum->GetIndexByValue(RhsValue);
	bool bLhsEnumValid = LhsIndex != INDEX_NONE;
	bool bRhsEnumValid = RhsIndex != INDEX_NONE;
	if (bLhsEnumValid && bRhsEnumValid)
	{
		FName LhsEnumName(PropertyEnum->GetNameByIndex(LhsIndex));
		FName RhsEnumName(PropertyEnum->GetNameByIndex(RhsIndex));
		return LhsEnumName.Compare(RhsEnumName);
	}
	else if(bLhsEnumValid)
	{
		return -1;
	}
	else if(bRhsEnumValid)
	{
		return 1;
	}
	else
	{
		return RhsValue - LhsValue;
	}
}

// UByteProperty objects may in fact represent Enums - so they need special handling for alphabetic Enum vs. numerical Byte sorting.
template<>
FORCEINLINE int32 FCompareRowByColumnAscending<UByteProperty>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	// Get the basic uint8 values
	uint8 LhsValue; 
	LhsPropertyHandle->GetValue( LhsValue );

	uint8 RhsValue; 
	RhsPropertyHandle->GetValue( RhsValue );

	// Bytes are trivially sorted numerically
	UEnum* PropertyEnum = Property->GetIntPropertyEnum();
	if(PropertyEnum == nullptr)
	{
		return LhsValue < RhsValue;
	}
	else
	{
		int32 LhsIndex = PropertyEnum->GetIndexByValue(LhsValue);
		int32 RhsIndex = PropertyEnum->GetIndexByValue(RhsValue);
		// But Enums are sorted alphabetically based on the full enum entry name - must be sure that values are within Enum bounds!
		bool bLhsEnumValid = LhsIndex != INDEX_NONE;
		bool bRhsEnumValid = RhsIndex != INDEX_NONE;
		if(bLhsEnumValid && bRhsEnumValid)
		{
			FName LhsEnumName(PropertyEnum->GetNameByIndex(LhsIndex));
			FName RhsEnumName(PropertyEnum->GetNameByIndex(RhsIndex));
			return LhsEnumName.Compare(RhsEnumName);
		}
		else if(bLhsEnumValid)
		{
			return true;
		}
		else if(bRhsEnumValid)
		{
			return false;
		}
		else
		{
			return RhsValue - LhsValue;
		}
	}
}

template<>
FORCEINLINE int32 FCompareRowByColumnAscending<UNameProperty>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	FName LhsValue; 
	LhsPropertyHandle->GetValue( LhsValue );

	FName RhsValue; 
	RhsPropertyHandle->GetValue( RhsValue );

	return LhsValue.Compare(RhsValue);
}

template<>
FORCEINLINE int32 FCompareRowByColumnAscending<UObjectPropertyBase>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	UObject* LhsValue; 
	LhsPropertyHandle->GetValue( LhsValue );

	if ( LhsValue == NULL )
	{
		return 1;
	}

	UObject* RhsValue; 
	RhsPropertyHandle->GetValue( RhsValue );

	if ( RhsValue == NULL )
	{
		return -1;
	}

	return FCString::Stricmp(*LhsValue->GetName(), *RhsValue->GetName());
}

template<>
FORCEINLINE int32 FCompareRowByColumnAscending<UStructProperty>::ComparePropertyValue( const TSharedPtr< IPropertyHandle >& LhsPropertyHandle, const TSharedPtr< IPropertyHandle >& RhsPropertyHandle ) const
{
	if ( !FPropertyTableColumn::IsSupportedStructProperty(LhsPropertyHandle->GetProperty() ) )
	{
		return 1;
	}

	if ( !FPropertyTableColumn::IsSupportedStructProperty(RhsPropertyHandle->GetProperty() ) )
	{
		return -1;
	}

	{
		FVector LhsVector;
		FVector RhsVector;

		if ( LhsPropertyHandle->GetValue(LhsVector) != FPropertyAccess::Fail && RhsPropertyHandle->GetValue(RhsVector) != FPropertyAccess::Fail )
		{
			return RhsVector.SizeSquared() - LhsVector.SizeSquared();
		}

		FVector2D LhsVector2D;
		FVector2D RhsVector2D;

		if ( LhsPropertyHandle->GetValue(LhsVector2D) != FPropertyAccess::Fail && RhsPropertyHandle->GetValue(RhsVector2D) != FPropertyAccess::Fail )
		{
			return RhsVector2D.SizeSquared() - LhsVector2D.SizeSquared();
		}

		FVector4 LhsVector4;
		FVector4 RhsVector4;

		if ( LhsPropertyHandle->GetValue(LhsVector4) != FPropertyAccess::Fail && RhsPropertyHandle->GetValue(RhsVector4) != FPropertyAccess::Fail )
		{
			return RhsVector4.SizeSquared() - LhsVector4.SizeSquared();
		}
	}

	ensureMsgf(false, TEXT("A supported struct property does not have a defined implementation for sorting a property column."));
	return 0;
}


FPropertyTableColumn::FPropertyTableColumn( const TSharedRef< IPropertyTable >& InTable, const TWeakObjectPtr< UObject >& InObject )
	: Cells()
	, DataSource( MakeShareable( new UObjectDataSource( InObject.Get() ) ) )
	, Table( InTable )
	, Id( NAME_None )
	, DisplayName()
	, Width( 1.0f )
	, bIsHidden( false )
	, bIsFrozen( false )
	, PartialPath( FPropertyPath::CreateEmpty() )
	, SizeMode(EPropertyTableColumnSizeMode::Fill)
{
	GenerateColumnId();
	GenerateColumnDisplayName();
}

FPropertyTableColumn::FPropertyTableColumn( const TSharedRef< IPropertyTable >& InTable, const TSharedRef< FPropertyPath >& InPropertyPath )
	: Cells()
	, DataSource( MakeShareable( new PropertyPathDataSource( InPropertyPath ) ) )
	, Table( InTable )
	, Id( NAME_None )
	, DisplayName()
	, Width( 1.0f )
	, bIsHidden( false )
	, bIsFrozen( false )
	, PartialPath( FPropertyPath::CreateEmpty() )
	, SizeMode(EPropertyTableColumnSizeMode::Fill)
{
	GenerateColumnId();
	GenerateColumnDisplayName();
}

FPropertyTableColumn::FPropertyTableColumn( const TSharedRef< class IPropertyTable >& InTable, const TWeakObjectPtr< UObject >& InObject, const TSharedRef< FPropertyPath >& InPartialPropertyPath )
	: Cells()
	, DataSource( MakeShareable( new UObjectDataSource( InObject.Get() ) ) )
	, Table( InTable )
	, Id( NAME_None )
	, DisplayName()
	, Width( 1.0f )
	, bIsHidden( false )
	, bIsFrozen( false )
	, PartialPath( InPartialPropertyPath )
	, SizeMode(EPropertyTableColumnSizeMode::Fill)
{
	GenerateColumnId();
}


void FPropertyTableColumn::GenerateColumnId()
{
	TWeakObjectPtr< UObject > Object = DataSource->AsUObject();
	TSharedPtr< FPropertyPath > PropertyPath = DataSource->AsPropertyPath();

	// Use partial path for a valid column ID if we have one. We are pointing to a container with an array, but all columns must be unique
	if ( PartialPath->GetNumProperties() > 0 )
	{
		Id = FName( *PartialPath->ToString());
	}
	else if ( Object.IsValid() )
	{
		Id = Object->GetFName();
	}
	else if ( PropertyPath.IsValid() )
	{
		Id = FName( *PropertyPath->ToString() );
	}
	else
	{
		Id = NAME_None;
	}
}

void FPropertyTableColumn::GenerateColumnDisplayName()
{
	TWeakObjectPtr< UObject > Object = DataSource->AsUObject();
	TSharedPtr< FPropertyPath > PropertyPath = DataSource->AsPropertyPath();

	if ( Object.IsValid() )
	{
		if ( Object->IsA( UProperty::StaticClass() ) )
		{
			const UProperty* Property = Cast< UProperty >( Object.Get() );
			DisplayName = FText::FromString(UEditorEngine::GetFriendlyName(Property)); 
		}
		else
		{
			DisplayName = FText::FromString(Object->GetFName().ToString());
		}
	}
	else if ( PropertyPath.IsValid() )
	{
		//@todo unify this logic with all the property editors [12/11/2012 Justin.Sargent]
		FString NewName;
		bool FirstAddition = true;
		const FPropertyInfo* PreviousPropInfo = NULL;
		for (int PropertyIndex = 0; PropertyIndex < PropertyPath->GetNumProperties(); PropertyIndex++)
		{
			const FPropertyInfo& PropInfo = PropertyPath->GetPropertyInfo( PropertyIndex );

			if ( !(PropInfo.Property->IsA( UArrayProperty::StaticClass() ) && PropertyIndex != PropertyPath->GetNumProperties() - 1 ) )
			{
				if ( !FirstAddition )
				{
					NewName += TEXT( "->" );
				}

				FString PropertyName = PropInfo.Property->GetDisplayNameText().ToString();

				if ( PropertyName.IsEmpty() )
				{
					PropertyName = PropInfo.Property->GetName();

					const bool bIsBoolProperty = Cast<const UBoolProperty>( PropInfo.Property.Get() ) != NULL;

					if ( PreviousPropInfo != NULL )
					{
						const UStructProperty* ParentStructProperty = Cast<const UStructProperty>( PreviousPropInfo->Property.Get() );
						if( ParentStructProperty && ParentStructProperty->Struct->GetFName() == NAME_Rotator )
						{
							if( PropInfo.Property->GetFName() == "Roll" )
							{
								PropertyName = TEXT("X");
							}
							else if( PropInfo.Property->GetFName() == "Pitch" )
							{
								PropertyName = TEXT("Y");
							}
							else if( PropInfo.Property->GetFName() == "Yaw" )
							{
								PropertyName = TEXT("Z");
							}
							else
							{
								check(0);
							}
						}
					}

					PropertyName = FName::NameToDisplayString( PropertyName, bIsBoolProperty );
				}

				NewName += PropertyName;

				if ( PropInfo.ArrayIndex != INDEX_NONE )
				{
					NewName += FString::Printf( TEXT( "[%d]" ), PropInfo.ArrayIndex );
				}

				PreviousPropInfo = &PropInfo;
				FirstAddition = false;
			}
		}

		DisplayName = FText::FromString(*NewName);
	}
	else
	{
		DisplayName = LOCTEXT( "InvalidColumnName", "Invalid Property" );
	}
}

FName FPropertyTableColumn::GetId() const 
{ 
	return Id;
}

FText FPropertyTableColumn::GetDisplayName() const 
{ 
	return DisplayName;
}

TSharedRef< IPropertyTableCell > FPropertyTableColumn::GetCell( const TSharedRef< class IPropertyTableRow >& Row ) 
{
	//@todo Clean Cells cache when rows get updated [11/27/2012 Justin.Sargent]
	TSharedRef< IPropertyTableCell >* CellPtr = Cells.Find( Row );

	if( CellPtr != NULL )
	{
		return *CellPtr;
	}

	TSharedRef< IPropertyTableCell > Cell = MakeShareable( new FPropertyTableCell( SharedThis( this ), Row ) );
	Cells.Add( Row, Cell );

	return Cell;
}

void FPropertyTableColumn::RemoveCellsForRow( const TSharedRef< class IPropertyTableRow >& Row )
{
	Cells.Remove( Row );
}

TSharedRef< class IPropertyTable > FPropertyTableColumn::GetTable() const
{
	return Table.Pin().ToSharedRef();
}

bool FPropertyTableColumn::CanSortBy() const
{
	TWeakObjectPtr< UObject > Object = DataSource->AsUObject();
	UProperty* Property = Cast< UProperty >( Object.Get() );

	TSharedPtr< FPropertyPath > Path = DataSource->AsPropertyPath();
	if ( Property == NULL && Path.IsValid() )
	{
		Property = Path->GetLeafMostProperty().Property.Get();
	}

	return ( Property != nullptr );
}

TSharedPtr<FCompareRowByColumnBase> FPropertyTableColumn::GetPropertySorter(UProperty* Property, EColumnSortMode::Type SortMode)
{
	if (Property->IsA(UEnumProperty::StaticClass()))
	{
		UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property);

		if (SortMode == EColumnSortMode::Ascending)
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnAscending<UEnumProperty>(SharedThis(this), EnumProperty));
		}
		else
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnDescending<UEnumProperty>(SharedThis(this), EnumProperty));
		}
	}
	else if (Property->IsA(UByteProperty::StaticClass()))
	{
		UByteProperty* ByteProperty = Cast<UByteProperty>(Property);

		if (SortMode == EColumnSortMode::Ascending)
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnAscending<UByteProperty>(SharedThis(this), ByteProperty));
		}
		else
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnDescending<UByteProperty>(SharedThis(this), ByteProperty));
		}
	}
	else if (Property->IsA(UIntProperty::StaticClass()))
	{
		UIntProperty* IntProperty = Cast<UIntProperty>(Property);

		if (SortMode == EColumnSortMode::Ascending)
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnAscending<UIntProperty>(SharedThis(this), IntProperty));
		}
		else
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnDescending<UIntProperty>(SharedThis(this), IntProperty));
		}
	}
	else if (Property->IsA(UBoolProperty::StaticClass()))
	{
		UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property);

		if (SortMode == EColumnSortMode::Ascending)
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnAscending<UBoolProperty>(SharedThis(this), BoolProperty));
		}
		else
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnDescending<UBoolProperty >(SharedThis(this), BoolProperty));
		}
	}
	else if (Property->IsA(UFloatProperty::StaticClass()))
	{
		UFloatProperty* FloatProperty(Cast< UFloatProperty >(Property));

		if (SortMode == EColumnSortMode::Ascending)
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnAscending<UFloatProperty>(SharedThis(this), FloatProperty));
		}
		else
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnDescending<UFloatProperty>(SharedThis(this), FloatProperty));
		}
	}
	else if (Property->IsA(UNameProperty::StaticClass()))
	{
		UNameProperty* NameProperty = Cast<UNameProperty>(Property);

		if (SortMode == EColumnSortMode::Ascending)
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnAscending<UNameProperty>(SharedThis(this), NameProperty));
		}
		else
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnDescending<UNameProperty>(SharedThis(this), NameProperty));
		}
	}
	else if (Property->IsA(UStrProperty::StaticClass()))
	{
		UStrProperty* StrProperty = Cast<UStrProperty>(Property);

		if (SortMode == EColumnSortMode::Ascending)
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnAscending<UStrProperty>(SharedThis(this), StrProperty));
		}
		else
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnDescending<UStrProperty>(SharedThis(this), StrProperty));
		}
	}
	else if (Property->IsA(UObjectPropertyBase::StaticClass()) && !Property->HasAnyPropertyFlags(CPF_InstancedReference))
	{
		UObjectPropertyBase* ObjectProperty = Cast<UObjectPropertyBase>(Property);

		if (SortMode == EColumnSortMode::Ascending)
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnAscending<UObjectPropertyBase>(SharedThis(this), ObjectProperty));
		}
		else
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnDescending<UObjectPropertyBase>(SharedThis(this), ObjectProperty));
		}
	}
	else if (IsSupportedStructProperty(Property))
	{
		UStructProperty* StructProperty = Cast<UStructProperty>(Property);

		if (SortMode == EColumnSortMode::Ascending)
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnAscending<UStructProperty>(SharedThis(this), StructProperty));
		}
		else
		{
			return MakeShareable<FCompareRowByColumnBase>(new FCompareRowByColumnDescending<UStructProperty>(SharedThis(this), StructProperty));
		}
	}
	//else if ( Property->IsA( UTextProperty::StaticClass() ) )
	//{
	//	if ( SortMode == EColumnSortMode::Ascending )
	//	{
	//		Rows.Sort( FCompareRowByColumnAscending< UTextProperty >( SharedThis( this ) ) );
	//	}
	//	else
	//	{
	//		Rows.Sort( FCompareRowByColumnDescending< UTextProperty >( SharedThis( this ) ) );
	//	}
	//}
	else
	{
		return MakeShareable<FCompareRowByColumnUsingExportTextLexicographic>(new FCompareRowByColumnUsingExportTextLexicographic(SharedThis(this), Property, (SortMode == EColumnSortMode::Ascending)));
	}
}

void FPropertyTableColumn::Sort( TArray< TSharedRef< class IPropertyTableRow > >& Rows, const EColumnSortMode::Type PrimarySortMode, const TSharedPtr<IPropertyTableColumn>& SecondarySortColumn, const EColumnSortMode::Type SecondarySortMode )
{
	if (PrimarySortMode == EColumnSortMode::None )
	{
		return;
	}

	UObject* PrimaryObject = DataSource->AsUObject().Get();
	UProperty* PrimaryProperty = Cast< UProperty >(PrimaryObject);
	TSharedPtr< FPropertyPath > PrimaryPath = DataSource->AsPropertyPath();
	if (PrimaryProperty == nullptr && PrimaryPath.IsValid())
	{
		PrimaryProperty = PrimaryPath->GetLeafMostProperty().Property.Get();
	}

	UObject* SecondaryObject = nullptr;
	UProperty* SecondaryProperty = nullptr;
	if(SecondarySortColumn.IsValid())
	{
		SecondaryObject = SecondarySortColumn->GetDataSource()->AsUObject().Get();
		SecondaryProperty = Cast< UProperty >(SecondaryObject);
		TSharedPtr< FPropertyPath > SecondaryPath = SecondarySortColumn->GetDataSource()->AsPropertyPath();
		if (SecondaryProperty == nullptr && SecondaryPath.IsValid())
		{
			SecondaryProperty = SecondaryPath->GetLeafMostProperty().Property.Get();
		}
	}


	if (!PrimaryProperty)
	{
		return;
	}


	TSharedPtr<FCompareRowByColumnBase> SecondarySorter = nullptr;
	if(SecondaryProperty && SecondarySortMode != EColumnSortMode::None)
	{
		SecondarySorter = SecondarySortColumn->GetPropertySorter(SecondaryProperty, SecondarySortMode);
	}

	// if we had a secondary sort we need to make sure the primary sort is stable to not break the secondary results
	TSharedPtr<FCompareRowByColumnBase> PrimarySorter = GetPropertySorter(PrimaryProperty, PrimarySortMode);
	Rows.Sort(FCompareRowPrimaryAndSecondary(PrimarySorter.Get(), SecondarySorter.Get()));

}

void FPropertyTableColumn::Tick()
{
	if ( !DataSource->AsPropertyPath().IsValid() )
	{
		const TSharedRef< IPropertyTable > TableRef = GetTable();
		const TWeakObjectPtr< UObject > Object = DataSource->AsUObject();

		if ( !Object.IsValid() )
		{
			TableRef->RemoveColumn( SharedThis( this ) );
		}
		else
		{
			const TSharedRef< FObjectPropertyNode > Node = TableRef->GetObjectPropertyNode( Object );
			EPropertyDataValidationResult Result = Node->EnsureDataIsValid();

			if ( Result == EPropertyDataValidationResult::ObjectInvalid )
			{
				TableRef->RemoveColumn( SharedThis( this ) );
			}
			else if ( Result == EPropertyDataValidationResult::ArraySizeChanged )
			{
				TableRef->RequestRefresh();
			}
		}
	}
}

void FPropertyTableColumn::SetFrozen(bool InIsFrozen)
{
	bIsFrozen = InIsFrozen;
	FrozenStateChanged.Broadcast( SharedThis(this) );
}

bool FPropertyTableColumn::IsSupportedStructProperty(const UProperty* InProperty)
{
	if ( InProperty != nullptr && Cast<UStructProperty>(InProperty) != nullptr)
	{
		const UStructProperty* StructProp = Cast<UStructProperty>(InProperty);
		FName StructName = StructProp->Struct->GetFName();

		return StructName == NAME_Vector ||
			StructName == NAME_Vector2D	 ||
			StructName == NAME_Vector4;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
