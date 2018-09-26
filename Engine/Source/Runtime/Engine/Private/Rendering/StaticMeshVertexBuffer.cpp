// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Rendering/StaticMeshVertexBuffer.h"
#include "EngineUtils.h"
#include "Components.h"
#include "GPUSkinCache.h"

FStaticMeshVertexBuffer::FStaticMeshVertexBuffer() :
	TangentsData(nullptr),
	TexcoordData(nullptr),
	TangentsDataPtr(nullptr),
	TexcoordDataPtr(nullptr),
	NumTexCoords(0),
	NumVertices(0),
	bUseFullPrecisionUVs(!GVertexElementTypeSupport.IsSupported(VET_Half2)),
	bUseHighPrecisionTangentBasis(false)
{}

FStaticMeshVertexBuffer::~FStaticMeshVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FStaticMeshVertexBuffer::CleanUp()
{
	if (TangentsData)
	{
		delete TangentsData;
		TangentsData = nullptr;
	}
	if (TexcoordData)
	{
		delete TexcoordData;
		TexcoordData = nullptr;
	}
}

void FStaticMeshVertexBuffer::Init(uint32 InNumVertices, uint32 InNumTexCoords, bool bNeedsCPUAccess)
{
	NumTexCoords = InNumTexCoords;
	NumVertices = InNumVertices;
	NeedsCPUAccess = bNeedsCPUAccess;

	// Allocate the vertex data storage type.
	AllocateData(bNeedsCPUAccess);

	// Allocate the vertex data buffer.
	TangentsData->ResizeBuffer(NumVertices);
	TangentsDataPtr = NumVertices ? TangentsData->GetDataPointer() : nullptr;
	TexcoordData->ResizeBuffer(NumVertices * GetNumTexCoords());
	TexcoordDataPtr = NumVertices ? TexcoordData->GetDataPointer() : nullptr;
}

/**
* Initializes the buffer with the given vertices.
* @param InVertices - The vertices to initialize the buffer with.
* @param InNumTexCoords - The number of texture coordinate to store in the buffer.
*/
void FStaticMeshVertexBuffer::Init(const TArray<FStaticMeshBuildVertex>& InVertices, uint32 InNumTexCoords, bool bNeedsCPUAccess)
{
	Init(InVertices.Num(), InNumTexCoords, bNeedsCPUAccess);

	// Copy the vertices into the buffer.
	for (int32 VertexIndex = 0; VertexIndex < InVertices.Num(); VertexIndex++)
	{
		const FStaticMeshBuildVertex& SourceVertex = InVertices[VertexIndex];
		const uint32 DestVertexIndex = VertexIndex;
		SetVertexTangents(DestVertexIndex, SourceVertex.TangentX, SourceVertex.TangentY, SourceVertex.TangentZ);

		for (uint32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++)
		{
			SetVertexUV(DestVertexIndex, UVIndex, SourceVertex.UVs[UVIndex]);
		}
	}
}

/**
* Initializes this vertex buffer with the contents of the given vertex buffer.
* @param InVertexBuffer - The vertex buffer to initialize from.
*/
void FStaticMeshVertexBuffer::Init(const FStaticMeshVertexBuffer& InVertexBuffer, bool bNeedsCPUAccess)
{
	NeedsCPUAccess = bNeedsCPUAccess;
	NumTexCoords = InVertexBuffer.GetNumTexCoords();
	NumVertices = InVertexBuffer.GetNumVertices();
	bUseFullPrecisionUVs = InVertexBuffer.GetUseFullPrecisionUVs();
	bUseHighPrecisionTangentBasis = InVertexBuffer.GetUseHighPrecisionTangentBasis();

	if (NumVertices)
	{
		AllocateData(bNeedsCPUAccess);
		{
			check(TangentsData->GetStride() == InVertexBuffer.TangentsData->GetStride());
			TangentsData->ResizeBuffer(NumVertices);
			TangentsDataPtr = TangentsData->GetDataPointer();
			const uint8* InData = InVertexBuffer.TangentsDataPtr;
			FMemory::Memcpy(TangentsDataPtr, InData, TangentsData->GetStride() * NumVertices);
		}
		{
			check(TexcoordData->GetStride() == InVertexBuffer.TexcoordData->GetStride());
			check(GetNumTexCoords() == InVertexBuffer.GetNumTexCoords());
			const uint8* InData = InVertexBuffer.TexcoordDataPtr;

			// convert half float data to full float if the HW requires it.
			if (!GetUseFullPrecisionUVs() && !GVertexElementTypeSupport.IsSupported(VET_Half2))
			{
				ConvertHalfTexcoordsToFloat(InData);
			}
			else
			{
				TexcoordData->ResizeBuffer(NumVertices * GetNumTexCoords());
				TexcoordDataPtr = TexcoordData->GetDataPointer();
				FMemory::Memcpy(TexcoordDataPtr, InData, TexcoordData->GetStride() * NumVertices * GetNumTexCoords());
			}
		}
	}
}

void FStaticMeshVertexBuffer::ConvertHalfTexcoordsToFloat(const uint8* InData)
{
	check(TexcoordData);
	SetUseFullPrecisionUVs(true);

	FStaticMeshVertexDataInterface* OriginalTexcoordData = TexcoordData;

	typedef TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT> UVType;
	TexcoordData = new TStaticMeshVertexData<UVType>(OriginalTexcoordData->GetAllowCPUAccess());
	TexcoordData->ResizeBuffer(NumVertices * GetNumTexCoords());
	TexcoordDataPtr = TexcoordData->GetDataPointer();
	TexcoordStride = sizeof(UVType);

	FVector2D* DestTexcoordDataPtr = (FVector2D*)TexcoordDataPtr;
	FVector2DHalf* SourceTexcoordDataPtr = (FVector2DHalf*)(InData ? InData : OriginalTexcoordData->GetDataPointer());
	for (uint32 i = 0; i < NumVertices * GetNumTexCoords(); i++)
	{
		*DestTexcoordDataPtr++ = *SourceTexcoordDataPtr++;
	}

	delete OriginalTexcoordData;
	OriginalTexcoordData = nullptr;
}


void FStaticMeshVertexBuffer::AppendVertices( const FStaticMeshBuildVertex* Vertices, const uint32 NumVerticesToAppend )
{
	if ((TangentsData == nullptr || TexcoordData == nullptr) && NumVerticesToAppend > 0)
	{
		check(NumVertices == 0);
		NumTexCoords = 1;

		// Allocate the vertex data storage type if it has never been allocated before
		AllocateData(NeedsCPUAccess);
	}

	if( NumVerticesToAppend > 0 )
	{
		check( Vertices != nullptr );

		const uint32 FirstDestVertexIndex = NumVertices;
		NumVertices += NumVerticesToAppend;

		TangentsData->ResizeBuffer(NumVertices);
		TexcoordData->ResizeBuffer(NumVertices * GetNumTexCoords());

		if( NumVertices > 0 )
		{
			TangentsDataPtr = TangentsData->GetDataPointer();
			TexcoordDataPtr = TexcoordData->GetDataPointer();

			// Copy the vertices into the buffer.
			for( uint32 VertexIter = 0; VertexIter < NumVerticesToAppend; ++VertexIter )
			{
				const FStaticMeshBuildVertex& SourceVertex = Vertices[ VertexIter ];

				const uint32 DestVertexIndex = FirstDestVertexIndex + VertexIter;

				SetVertexTangents( DestVertexIndex, SourceVertex.TangentX, SourceVertex.TangentY, SourceVertex.TangentZ );
				for( uint32 UVIndex = 0; UVIndex < NumTexCoords; UVIndex++ )
				{
					SetVertexUV( DestVertexIndex, UVIndex, SourceVertex.UVs[ UVIndex ] );
				}
			}
		}
	}
}


/**
* Serializer
*
* @param	Ar				Archive to serialize with
* @param	bNeedsCPUAccess	Whether the elements need to be accessed by the CPU
*/
void FStaticMeshVertexBuffer::Serialize(FArchive& Ar, bool bNeedsCPUAccess)
{
	NeedsCPUAccess = bNeedsCPUAccess;

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FStaticMeshVertexBuffer::Serialize"), STAT_StaticMeshVertexBuffer_Serialize, STATGROUP_LoadTime);

	FStripDataFlags StripFlags(Ar, 0, VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX);

	Ar << NumTexCoords << NumVertices;
	Ar << bUseFullPrecisionUVs;
	Ar << bUseHighPrecisionTangentBasis;

	if (Ar.IsLoading())
	{
		// Allocate the vertex data storage type.
		AllocateData(bNeedsCPUAccess);
	}

	if (!StripFlags.IsDataStrippedForServer() || Ar.IsCountingMemory())
	{
		if (TangentsData != nullptr)
		{
			// Serialize the vertex data.
			TangentsData->Serialize(Ar);

			// Make a copy of the vertex data pointer.
			TangentsDataPtr = NumVertices ? TangentsData->GetDataPointer() : nullptr;
		}

		if (TexcoordData != nullptr)
		{
			// Serialize the vertex data.
			TexcoordData->Serialize(Ar);

			// Make a copy of the vertex data pointer.
			TexcoordDataPtr = NumVertices ? TexcoordData->GetDataPointer() : nullptr;

			// convert half float data to full float if the HW requires it.
			if (NumVertices && !GetUseFullPrecisionUVs() && !GVertexElementTypeSupport.IsSupported(VET_Half2))
			{
				ConvertHalfTexcoordsToFloat(nullptr);
			}
		}
	}
}


/**
* Specialized assignment operator, only used when importing LOD's.
*/
void FStaticMeshVertexBuffer::operator=(const FStaticMeshVertexBuffer &Other)
{
	//VertexData doesn't need to be allocated here because Build will be called next,
	CleanUp();
	bUseFullPrecisionUVs = Other.bUseFullPrecisionUVs;
	bUseHighPrecisionTangentBasis = Other.bUseHighPrecisionTangentBasis;
}

void FStaticMeshVertexBuffer::InitRHI()
{
	{
		check(TangentsData);
		FResourceArrayInterface* ResourceArray = TangentsData->GetResourceArray();
		if (ResourceArray->GetResourceDataSize())
		{
			// Create the vertex buffer.
			FRHIResourceCreateInfo CreateInfo(ResourceArray);
			TangentsVertexBuffer.VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(), BUF_Static | BUF_ShaderResource, CreateInfo);
			if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform) || IsGPUSkinCacheAvailable())
			{
				TangentsSRV = RHICreateShaderResourceView(TangentsVertexBuffer.VertexBufferRHI, GetUseHighPrecisionTangentBasis() ? 8 : 4, GetUseHighPrecisionTangentBasis() ? PF_R16G16B16A16_SNORM : PF_R8G8B8A8_SNORM);
			}
		}
	}
	{
		check(TexcoordData);
		FResourceArrayInterface* ResourceArray = TexcoordData->GetResourceArray();
		if (ResourceArray->GetResourceDataSize())
		{
			// Create the vertex buffer.
			FRHIResourceCreateInfo CreateInfo(ResourceArray);
			TexCoordVertexBuffer.VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(), BUF_Static | BUF_ShaderResource, CreateInfo);
			if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
			{
				TextureCoordinatesSRV = RHICreateShaderResourceView(TexCoordVertexBuffer.VertexBufferRHI, GetUseFullPrecisionUVs() ? 8 : 4, GetUseFullPrecisionUVs() ? PF_G32R32F : PF_G16R16F);
			}
		}
	}
}

void FStaticMeshVertexBuffer::ReleaseRHI()
{
	TangentsSRV.SafeRelease();
	TextureCoordinatesSRV.SafeRelease();

	TangentsVertexBuffer.ReleaseRHI();
	TexCoordVertexBuffer.ReleaseRHI();
}

void FStaticMeshVertexBuffer::InitResource()
{
	FRenderResource::InitResource();
	TangentsVertexBuffer.InitResource();
	TexCoordVertexBuffer.InitResource();
}

void FStaticMeshVertexBuffer::ReleaseResource()
{
	FRenderResource::ReleaseResource();
	TangentsVertexBuffer.ReleaseResource();
	TexCoordVertexBuffer.ReleaseResource();
}

void FStaticMeshVertexBuffer::AllocateData(bool bNeedsCPUAccess /*= true*/)
{
	// Clear any old VertexData before allocating.
	CleanUp();

	uint32 VertexStride = 0;
	if (GetUseHighPrecisionTangentBasis())
	{
		typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::TangentTypeT> TangentType;
		TangentsStride = sizeof(TangentType);
		TangentsData = new TStaticMeshVertexData<TangentType>(bNeedsCPUAccess);
	}
	else
	{
		typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::TangentTypeT> TangentType;
		TangentsStride = sizeof(TangentType);
		TangentsData = new TStaticMeshVertexData<TangentType>(bNeedsCPUAccess);
	}

	if (GetUseFullPrecisionUVs())
	{
		typedef TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT> UVType;
		TexcoordStride = sizeof(UVType);
		TexcoordData = new TStaticMeshVertexData<UVType>(bNeedsCPUAccess);
	}
	else
	{
		typedef TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT> UVType;
		TexcoordStride = sizeof(UVType);
		TexcoordData = new TStaticMeshVertexData<UVType>(bNeedsCPUAccess);
	}
}

int FStaticMeshVertexBuffer::GetTangentSize()
{
	if (GetUseHighPrecisionTangentBasis())
	{
		typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::TangentTypeT> TangentType;
		TangentsStride = sizeof(TangentType);
		return TangentsStride * GetNumVertices();
	}
	else
	{
		typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::TangentTypeT> TangentType;
		TangentsStride = sizeof(TangentType);
		return TangentsStride * GetNumVertices();
	}
}

int FStaticMeshVertexBuffer::GetTexCoordSize()
{
	if (GetUseFullPrecisionUVs())
	{
		typedef TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT> UVType;
		TexcoordStride = sizeof(UVType);
		return TexcoordStride * GetNumTexCoords() * GetNumVertices();
	}
	else
	{
		typedef TStaticMeshVertexUVsDatum<typename TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT> UVType;
		TexcoordStride = sizeof(UVType);
		return TexcoordStride * GetNumTexCoords() * GetNumVertices();
	}
}

void FStaticMeshVertexBuffer::BindTangentVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& Data) const
{
	{
		Data.TangentsSRV = TangentsSRV;
	}

	{
		uint32 TangentSizeInBytes = 0;
		uint32 TangentXOffset = 0;
		uint32 TangentZOffset = 0;
		EVertexElementType TangentElemType = VET_None;

		if (GetUseHighPrecisionTangentBasis())
		{
			typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::TangentTypeT> TangentType;
			TangentElemType = TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::VertexElementType;
			TangentXOffset = STRUCT_OFFSET(TangentType, TangentX);
			TangentZOffset = STRUCT_OFFSET(TangentType, TangentZ);
			TangentSizeInBytes = sizeof(TangentType);

		}
		else
		{
			typedef TStaticMeshVertexTangentDatum<typename TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::TangentTypeT> TangentType;
			TangentElemType = TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::VertexElementType;
			TangentXOffset = STRUCT_OFFSET(TangentType, TangentX);
			TangentZOffset = STRUCT_OFFSET(TangentType, TangentZ);
			TangentSizeInBytes = sizeof(TangentType);
		}

		Data.TangentBasisComponents[0] = FVertexStreamComponent(
			&TangentsVertexBuffer,
			TangentXOffset,
			TangentSizeInBytes,
			TangentElemType,
			EVertexStreamUsage::ManualFetch
		);

		Data.TangentBasisComponents[1] = FVertexStreamComponent(
			&TangentsVertexBuffer,
			TangentZOffset,
			TangentSizeInBytes,
			TangentElemType,
			EVertexStreamUsage::ManualFetch
		);
	}
}

void FStaticMeshVertexBuffer::BindPackedTexCoordVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& Data) const
{
	Data.TextureCoordinates.Empty();
	Data.NumTexCoords = GetNumTexCoords();

	{
		Data.TextureCoordinatesSRV = TextureCoordinatesSRV;
	}

	{
		EVertexElementType UVDoubleWideVertexElementType = VET_None;
		EVertexElementType UVVertexElementType = VET_None;
		uint32 UVSizeInBytes = 0;
		if (GetUseFullPrecisionUVs())
		{
			UVSizeInBytes = sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT);
			UVDoubleWideVertexElementType = VET_Float4;
			UVVertexElementType = VET_Float2;
		}
		else
		{
			UVSizeInBytes = sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT);
			UVDoubleWideVertexElementType = VET_Half4;
			UVVertexElementType = VET_Half2;
		}

		uint32 UvStride = UVSizeInBytes * GetNumTexCoords();

		int32 UVIndex;
		for (UVIndex = 0; UVIndex < (int32)GetNumTexCoords() - 1; UVIndex += 2)
		{
			Data.TextureCoordinates.Add(FVertexStreamComponent(
				&TexCoordVertexBuffer,
				UVSizeInBytes * UVIndex,
				UvStride,
				UVDoubleWideVertexElementType,
				EVertexStreamUsage::ManualFetch
			));
		}

		// possible last UV channel if we have an odd number
		if (UVIndex < (int32)GetNumTexCoords())
		{
			Data.TextureCoordinates.Add(FVertexStreamComponent(
				&TexCoordVertexBuffer,
				UVSizeInBytes * UVIndex,
				UvStride,
				UVVertexElementType,
				EVertexStreamUsage::ManualFetch
			));
		}
	}
}

void FStaticMeshVertexBuffer::BindTexCoordVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& Data, int ClampedNumTexCoords) const
{
	Data.TextureCoordinates.Empty();
	Data.NumTexCoords = GetNumTexCoords();

	{
		Data.TextureCoordinatesSRV = TextureCoordinatesSRV;
	}

	{
		EVertexElementType UVVertexElementType = VET_None;
		uint32 UVSizeInBytes = 0;

		if (GetUseFullPrecisionUVs())
		{
			UVSizeInBytes = sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT);
			UVVertexElementType = VET_Float2;
		}
		else
		{
			UVSizeInBytes = sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT);
			UVVertexElementType = VET_Half2;
		}

		uint32 UvStride = UVSizeInBytes * GetNumTexCoords();

		if (ClampedNumTexCoords > -1)
		{
			ClampedNumTexCoords = FMath::Min<uint32>(GetNumTexCoords(), MAX_TEXCOORDS);
		}
		else
		{
			ClampedNumTexCoords = GetNumTexCoords();
		}

		check(ClampedNumTexCoords >= 0);

		for (uint32 UVIndex = 0; UVIndex < (uint32)ClampedNumTexCoords; UVIndex++)
		{
			Data.TextureCoordinates.Add(FVertexStreamComponent(
				&TexCoordVertexBuffer,
				UVSizeInBytes * UVIndex,
				UvStride,
				UVVertexElementType,
				EVertexStreamUsage::ManualFetch
			));
		}
	}
}

void FStaticMeshVertexBuffer::BindLightMapVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& Data, int LightMapCoordinateIndex) const
{
	LightMapCoordinateIndex = LightMapCoordinateIndex < (int32)GetNumTexCoords() ? LightMapCoordinateIndex : (int32)GetNumTexCoords() - 1;

	// Temp patch.
	if (LightMapCoordinateIndex < 0)
	{
		static bool bOnce = false;
		if (!bOnce)
		{
			UE_LOG(LogStaticMesh, Warning, TEXT("LightMapCoordinateIndex (%d)< 0, GetNumTexCoords: %d!"), LightMapCoordinateIndex, (int32)GetNumTexCoords());
			bOnce = false;
		}
		return;
	}
	check(LightMapCoordinateIndex >= 0);

	Data.LightMapCoordinateIndex = LightMapCoordinateIndex;
	Data.NumTexCoords = GetNumTexCoords();

	{
		Data.TextureCoordinatesSRV = TextureCoordinatesSRV;
	}

	{
		EVertexElementType UVVertexElementType = VET_None;
		uint32 UVSizeInBytes = 0;

		if (GetUseFullPrecisionUVs())
		{
			UVSizeInBytes = sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT);
			UVVertexElementType = VET_Float2;
		}
		else
		{
			UVSizeInBytes = sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT);
			UVVertexElementType = VET_Half2;
		}

		uint32 UvStride = UVSizeInBytes * GetNumTexCoords();

		if (LightMapCoordinateIndex >= 0 && (uint32)LightMapCoordinateIndex < GetNumTexCoords())
		{
			Data.LightMapCoordinateComponent = FVertexStreamComponent(
				&TexCoordVertexBuffer,
				UVSizeInBytes * LightMapCoordinateIndex,
				UvStride,
				UVVertexElementType, 
				EVertexStreamUsage::ManualFetch
			);
		}
	}
}
