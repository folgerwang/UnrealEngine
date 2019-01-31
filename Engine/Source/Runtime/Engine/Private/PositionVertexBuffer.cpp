// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Rendering/PositionVertexBuffer.h"

#include "CoreMinimal.h"
#include "RHI.h"
#include "Components.h"

#include "StaticMeshVertexData.h"
#include "GPUSkinCache.h"

/*-----------------------------------------------------------------------------
FPositionVertexBuffer
-----------------------------------------------------------------------------*/

/** The implementation of the static mesh position-only vertex data storage type. */
class FPositionVertexData :
	public TStaticMeshVertexData<FPositionVertex>
{
public:
	FPositionVertexData( bool InNeedsCPUAccess=false )
		: TStaticMeshVertexData<FPositionVertex>( InNeedsCPUAccess )
	{
	}
};


FPositionVertexBuffer::FPositionVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{}

FPositionVertexBuffer::~FPositionVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FPositionVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = NULL;
	}
}

void FPositionVertexBuffer::Init(uint32 InNumVertices, bool bInNeedsCPUAccess)
{
	NumVertices = InNumVertices;
	bNeedsCPUAccess = bInNeedsCPUAccess;

	// Allocate the vertex data storage type.
	AllocateData(bInNeedsCPUAccess);

	// Allocate the vertex data buffer.
	VertexData->ResizeBuffer(NumVertices);
	Data = NumVertices ? VertexData->GetDataPointer() : nullptr;
}

/**
* Initializes the buffer with the given vertices, used to convert legacy layouts.
* @param InVertices - The vertices to initialize the buffer with.
*/
void FPositionVertexBuffer::Init(const TArray<FStaticMeshBuildVertex>& InVertices, bool bInNeedsCPUAccess)
{
	Init(InVertices.Num(), bInNeedsCPUAccess);

	// Copy the vertices into the buffer.
	for(int32 VertexIndex = 0;VertexIndex < InVertices.Num();VertexIndex++)
	{
		const FStaticMeshBuildVertex& SourceVertex = InVertices[VertexIndex];
		const uint32 DestVertexIndex = VertexIndex;
		VertexPosition(DestVertexIndex) = SourceVertex.Position;
	}
}

/**
* Initializes this vertex buffer with the contents of the given vertex buffer.
* @param InVertexBuffer - The vertex buffer to initialize from.
*/
void FPositionVertexBuffer::Init(const FPositionVertexBuffer& InVertexBuffer, bool bInNeedsCPUAccess)
{
	bNeedsCPUAccess = bInNeedsCPUAccess;
	if ( InVertexBuffer.GetNumVertices() )
	{
		Init(InVertexBuffer.GetNumVertices(), bInNeedsCPUAccess);

		check( Stride == InVertexBuffer.GetStride() );

		const uint8* InData = InVertexBuffer.Data;
		FMemory::Memcpy( Data, InData, Stride * NumVertices );
	}
}

void FPositionVertexBuffer::Init(const TArray<FVector>& InPositions, bool bInNeedsCPUAccess)
{
	NumVertices = InPositions.Num();
	bNeedsCPUAccess = bInNeedsCPUAccess;
	if ( NumVertices )
	{
		AllocateData(bInNeedsCPUAccess);
		check( Stride == InPositions.GetTypeSize() );
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();
		FMemory::Memcpy( Data, InPositions.GetData(), Stride * NumVertices );
	}
}

void FPositionVertexBuffer::AppendVertices( const FStaticMeshBuildVertex* Vertices, const uint32 NumVerticesToAppend )
{
	if (VertexData == nullptr && NumVerticesToAppend > 0)
	{
		// Allocate the vertex data storage type if the buffer was never allocated before
		AllocateData(bNeedsCPUAccess);
	}

	if( NumVerticesToAppend > 0 )
	{
		check( VertexData != nullptr );
		check( Vertices != nullptr );

		const uint32 FirstDestVertexIndex = NumVertices;
		NumVertices += NumVerticesToAppend;
		VertexData->ResizeBuffer( NumVertices );
		if( NumVertices > 0 )
		{
			Data = VertexData->GetDataPointer();

			// Copy the vertices into the buffer.
			for( uint32 VertexIter = 0; VertexIter < NumVerticesToAppend; ++VertexIter )
			{
				const FStaticMeshBuildVertex& SourceVertex = Vertices[ VertexIter ];

				const uint32 DestVertexIndex = FirstDestVertexIndex + VertexIter;
				VertexPosition( DestVertexIndex ) = SourceVertex.Position;
			}
		}
	}
}

/**
 * Serializer
 *
 * @param	Ar					Archive to serialize with
 * @param	bInNeedsCPUAccess	Whether the elements need to be accessed by the CPU
 */
void FPositionVertexBuffer::Serialize( FArchive& Ar, bool bInNeedsCPUAccess )
{
	bNeedsCPUAccess = bInNeedsCPUAccess;

	Ar << Stride << NumVertices;

	if(Ar.IsLoading())
	{
		// Allocate the vertex data storage type.
		AllocateData( bInNeedsCPUAccess );
	}

	if(VertexData != NULL)
	{
		// Serialize the vertex data.
		VertexData->Serialize(Ar);

		// Make a copy of the vertex data pointer.
		Data = NumVertices ? VertexData->GetDataPointer() : nullptr;
	}
}

/**
* Specialized assignment operator, only used when importing LOD's.  
*/
void FPositionVertexBuffer::operator=(const FPositionVertexBuffer &Other)
{
	//VertexData doesn't need to be allocated here because Build will be called next,
	VertexData = NULL;
}

void FPositionVertexBuffer::InitRHI()
{
	check(VertexData);
	FResourceArrayInterface* ResourceArray = VertexData->GetResourceArray();
	if (ResourceArray->GetResourceDataSize())
	{
		// Create the vertex buffer.
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(), BUF_Static | BUF_ShaderResource, CreateInfo);

		// we have decide to create the SRV based on GMaxRHIShaderPlatform because this is created once and shared between feature levels for editor preview.
		if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform) || IsGPUSkinCacheAvailable())
		{
			PositionComponentSRV = RHICreateShaderResourceView(VertexBufferRHI, 4, PF_R32_FLOAT);
		}
	}
}

void FPositionVertexBuffer::ReleaseRHI()
{
	PositionComponentSRV.SafeRelease();

	FVertexBuffer::ReleaseRHI();
}

void FPositionVertexBuffer::AllocateData( bool bInNeedsCPUAccess /*= true*/ )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FPositionVertexData(bInNeedsCPUAccess);
	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}

void FPositionVertexBuffer::BindPositionVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& StaticMeshData) const
{
	StaticMeshData.PositionComponent = FVertexStreamComponent(
		this,
		STRUCT_OFFSET(FPositionVertex, Position),
		GetStride(),
		VET_Float3
	);
	StaticMeshData.PositionComponentSRV = PositionComponentSRV;
}