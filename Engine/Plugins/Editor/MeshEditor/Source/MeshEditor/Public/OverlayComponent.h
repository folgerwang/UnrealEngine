// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "Containers/SparseArray.h"
#include "RenderResource.h"
#include "PackedNormal.h"
#include "OverlayComponent.generated.h"

class FPrimitiveSceneProxy;


USTRUCT()
struct FOverlayLine
{
	GENERATED_BODY()

	FOverlayLine()
		: Start(ForceInitToZero)
		, End(ForceInitToZero)
		, Color(ForceInitToZero)
		, Thickness(0.f)
	{}

	FOverlayLine( const FVector& InStart, const FVector& InEnd, const FColor& InColor, const float InThickness )
		: Start( InStart ),
		  End( InEnd ),
		  Color( InColor ),
		  Thickness( InThickness )
	{
	}

	UPROPERTY()
	FVector Start;

	UPROPERTY()
	FVector End;

	UPROPERTY()
	FColor Color;

	UPROPERTY()
	float Thickness;
};


USTRUCT()
struct FOverlayPoint
{
	GENERATED_BODY()

	FOverlayPoint()
		: Position(ForceInitToZero)
		, Color(ForceInitToZero)
		, Size(0.f)
	{}

	FOverlayPoint( const FVector& InPosition, const FColor& InColor, const float InSize )
		: Position( InPosition ),
		  Color( InColor ),
		  Size( InSize )
	{}

	UPROPERTY()
	FVector Position;

	UPROPERTY()
	FColor Color;

	UPROPERTY()
	float Size;
};


USTRUCT()
struct FOverlayTriangleVertex
{
	GENERATED_BODY()

	FOverlayTriangleVertex()
		: Position(ForceInitToZero)
		, UV(ForceInitToZero)
		, Normal(ForceInitToZero)
		, Color(ForceInitToZero)
	{}

	FOverlayTriangleVertex( const FVector& InPosition, const FVector2D& InUV, const FVector& InNormal, const FColor& InColor )
		: Position( InPosition ),
		  UV( InUV ),
		  Normal( InNormal ),
		  Color( InColor )
	{
	}

	UPROPERTY()
	FVector Position;

	UPROPERTY()
	FVector2D UV;

	UPROPERTY()
	FVector Normal;

	UPROPERTY()
	FColor Color;
};


USTRUCT()
struct FOverlayTriangle
{
	GENERATED_BODY()

	FOverlayTriangle()
		: Material(nullptr)
		, Vertex0()
		, Vertex1()
		, Vertex2()
	{}

	FOverlayTriangle( UMaterialInterface* InMaterial, const FOverlayTriangleVertex& InVertex0, const FOverlayTriangleVertex& InVertex1, const FOverlayTriangleVertex& InVertex2 )
		: Material( InMaterial ),
		  Vertex0( InVertex0 ),
		  Vertex1( InVertex1 ),
		  Vertex2( InVertex2 )
	{
	}

	UPROPERTY()
	UMaterialInterface* Material;

	UPROPERTY()
	FOverlayTriangleVertex Vertex0;

	UPROPERTY()
	FOverlayTriangleVertex Vertex1;

	UPROPERTY()
	FOverlayTriangleVertex Vertex2;
};


USTRUCT()
struct FOverlayLineID
{
	GENERATED_BODY()

	FOverlayLineID() : ID( 0 ) {}
	explicit FOverlayLineID( const int32 InID ) : ID( InID ) {}

	int32 GetValue() const { return ID; }

	UPROPERTY()
	int32 ID;
};


USTRUCT()
struct FOverlayPointID
{
	GENERATED_BODY()

	FOverlayPointID() : ID( 0 ) {}
	explicit FOverlayPointID( const int32 InID ) : ID( InID ) {}

	int32 GetValue() const { return ID; }

	UPROPERTY()
	int32 ID;
};


USTRUCT()
struct FOverlayTriangleID
{
	GENERATED_BODY()

	FOverlayTriangleID() : ID( 0 ) {}
	explicit FOverlayTriangleID( const int32 InID ) : ID( InID ) {}

	int32 GetValue() const { return ID; }

	UPROPERTY()
	int32 ID;
};


UCLASS()
class MESHEDITOR_API UOverlayComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	/**
	 * Default UObject constructor.
	 */
	UOverlayComponent( const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get() );

	/** Specify material which handles lines */
	void SetLineMaterial( UMaterialInterface* InLineMaterial );

	/** Specify material which handles points */
	void SetPointMaterial( UMaterialInterface* InPointMaterial );

	/** Clear all primitives */
	void Clear();

	/** Add a line to the overlay */
	FOverlayLineID AddLine( const FOverlayLine& OverlayLine );

	/** Insert a line with the given ID to the overlay */
	void InsertLine( const FOverlayLineID ID, const FOverlayLine& OverlayLine );

	/** Sets the color of a line */
	void SetLineColor( const FOverlayLineID ID, const FColor& NewColor );

	/** Sets the thickness of a line */
	void SetLineThickness( const FOverlayLineID ID, const float NewThickness );

	/** Remove a line from the overlay */
	void RemoveLine( const FOverlayLineID ID );

	/** Queries whether a line with the given ID exists */
	bool IsLineValid( const FOverlayLineID ID ) const;

	/** Add a point to the overlay */
	FOverlayPointID AddPoint( const FOverlayPoint& OverlayPoint );

	/** Insert a point with the given ID to the overlay */
	void InsertPoint( const FOverlayPointID ID, const FOverlayPoint& OverlayPoint );

	/** Sets the color of a point */
	void SetPointColor( const FOverlayPointID ID, const FColor& NewColor );

	/** Sets the size of a point */
	void SetPointSize( const FOverlayPointID ID, const float NewSize );

	/** Remove a point from the overlay */
	void RemovePoint( const FOverlayPointID ID );

	/** Queries whether a point with the given ID exists */
	bool IsPointValid( const FOverlayPointID ID ) const;

	/** Add a triangle to the overlay */
	FOverlayTriangleID AddTriangle( const FOverlayTriangle& OverlayTriangle );

	/** Insert a triangle with the given ID to the overlay */
	void InsertTriangle( const FOverlayTriangleID ID, const FOverlayTriangle& OverlayTriangle );

	/** Remove a triangle from the overlay */
	void RemoveTriangle( const FOverlayTriangleID ID );

	/** Queries whether a triangle with the given ID exists */
	bool IsTriangleValid( const FOverlayTriangleID ID ) const;

private:

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds( const FTransform& LocalToWorld ) const override;
	//~ Begin USceneComponent Interface.

	int32 FindOrAddMaterialIndex( UMaterialInterface* Material );

	UPROPERTY()
	const UMaterialInterface* LineMaterial;

	UPROPERTY()
	const UMaterialInterface* PointMaterial;

	UPROPERTY()
	mutable FBoxSphereBounds Bounds;

	UPROPERTY()
	mutable bool bBoundsDirty;

	TSparseArray<FOverlayLine> Lines;
	TSparseArray<FOverlayPoint> Points;
	TSparseArray<TTuple<int32, int32>> Triangles;
	TSparseArray<TSparseArray<FOverlayTriangle>> TrianglesByMaterial;
	TMap<UMaterialInterface*, int32> MaterialToIndex;

	friend class FOverlaySceneProxy;
};
