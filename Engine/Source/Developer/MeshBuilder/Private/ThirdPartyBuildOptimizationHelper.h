// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
//#include "StaticMeshResources.h"

#include "NvTriStrip.h"
#include "forsythtriangleorderoptimizer.h"
#include "nvtess.h"

namespace BuildOptimizationPrivate
{
	// CVars
	static TAutoConsoleVariable<int32> CVarTriangleOrderOptimization(
		TEXT("r.TriangleOrderOptimization_MD"),
		1,
		TEXT("Controls the algorithm to use when optimizing the triangle order for the post-transform cache.\n")
		TEXT("0: Use NVTriStrip (slower)\n")
		TEXT("1: Use Forsyth algorithm (fastest)(default)")
		TEXT("2: No triangle order optimization. (least efficient, debugging purposes only)"),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarSupportDepthOnlyIndexBuffers(
		TEXT("r.SupportDepthOnlyIndexBuffers_MD"),
		1,
		TEXT("Enables depth-only index buffers. Saves a little time at the expense of doubling the size of index buffers."),
		ECVF_ReadOnly | ECVF_RenderThreadSafe);

	static TAutoConsoleVariable<int32> CVarSupportReversedIndexBuffers(
		TEXT("r.SupportReversedIndexBuffers_MD"),
		1,
		TEXT("Enables reversed index buffers. Saves a little time at the expense of doubling the size of index buffers."),
		ECVF_ReadOnly | ECVF_RenderThreadSafe);

	/*------------------------------------------------------------------------------
	NVTriStrip for cache optimizing index buffers.
	------------------------------------------------------------------------------*/

	namespace NvTriStripHelper
	{
		/**
		* Converts 16 bit indices to 32 bit prior to passing them into the real GenerateStrips util method
		*/
		void GenerateStrips(
			const uint8* Indices,
			bool Is32Bit,
			const uint32 NumIndices,
			PrimitiveGroup** PrimGroups,
			uint32* NumGroups
		)
		{
			if (Is32Bit)
			{
				GenerateStrips((uint32*)Indices, NumIndices, PrimGroups, NumGroups);
			}
			else
			{
				// convert to 32 bit
				uint32 Idx;
				TArray<uint32> NewIndices;
				NewIndices.AddUninitialized(NumIndices);
				for (Idx = 0; Idx < NumIndices; ++Idx)
				{
					NewIndices[Idx] = ((uint16*)Indices)[Idx];
				}
				GenerateStrips(NewIndices.GetData(), NumIndices, PrimGroups, NumGroups);
			}

		}

		/**
		* Orders a triangle list for better vertex cache coherency.
		*
		* *** WARNING: This is safe to call for multiple threads IF AND ONLY IF all
		* threads call SetListsOnly(true) and SetCacheSize(CACHESIZE_GEFORCE3). If
		* NvTriStrip is ever used with different settings the library will need
		* some modifications to be thread-safe. ***
		*/
		template<typename IndexDataType, typename Allocator>
		void CacheOptimizeIndexBuffer(TArray<IndexDataType, Allocator>& Indices)
		{
			static_assert(sizeof(IndexDataType) == 2 || sizeof(IndexDataType) == 4, "Indices must be short or int.");

			PrimitiveGroup*	PrimitiveGroups = NULL;
			uint32			NumPrimitiveGroups = 0;
			bool Is32Bit = sizeof(IndexDataType) == 4;

			SetListsOnly(true);
			SetCacheSize(CACHESIZE_GEFORCE3);

			GenerateStrips((uint8*)Indices.GetData(), Is32Bit, Indices.Num(), &PrimitiveGroups, &NumPrimitiveGroups);

			Indices.Empty();
			Indices.AddUninitialized(PrimitiveGroups->numIndices);

			if (Is32Bit)
			{
				FMemory::Memcpy(Indices.GetData(), PrimitiveGroups->indices, Indices.Num() * sizeof(IndexDataType));
			}
			else
			{
				for (uint32 I = 0; I < PrimitiveGroups->numIndices; ++I)
				{
					Indices[I] = (uint16)PrimitiveGroups->indices[I];
				}
			}

			delete[] PrimitiveGroups;
		}

		/**
		* Provides static mesh render data to the NVIDIA tessellation library.
		*/
		class FStaticMeshNvRenderBuffer : public nv::RenderBuffer
		{
		public:

			/** Construct from static mesh render buffers. */
			FStaticMeshNvRenderBuffer(
				const FPositionVertexBuffer& InPositionVertexBuffer,
				const FStaticMeshVertexBuffer& InVertexBuffer,
				const TArray<uint32>& Indices)
				: PositionVertexBuffer(InPositionVertexBuffer)
				, VertexBuffer(InVertexBuffer)
			{
				check(PositionVertexBuffer.GetNumVertices() == VertexBuffer.GetNumVertices());
				mIb = new nv::IndexBuffer((void*)Indices.GetData(), nv::IBT_U32, Indices.Num(), false);
			}

			/** Retrieve the position and first texture coordinate of the specified index. */
			virtual nv::Vertex getVertex(unsigned int Index) const
			{
				nv::Vertex Vertex;

				check(Index < PositionVertexBuffer.GetNumVertices());

				const FVector& Position = PositionVertexBuffer.VertexPosition(Index);
				Vertex.pos.x = Position.X;
				Vertex.pos.y = Position.Y;
				Vertex.pos.z = Position.Z;

				if (VertexBuffer.GetNumTexCoords())
				{
					const FVector2D UV = VertexBuffer.GetVertexUV(Index, 0);
					Vertex.uv.x = UV.X;
					Vertex.uv.y = UV.Y;
				}
				else
				{
					Vertex.uv.x = 0.0f;
					Vertex.uv.y = 0.0f;
				}

				return Vertex;
			}

		private:

			/** The position vertex buffer for the static mesh. */
			const FPositionVertexBuffer& PositionVertexBuffer;

			/** The vertex buffer for the static mesh. */
			const FStaticMeshVertexBuffer& VertexBuffer;

			/** Copying is forbidden. */
			FStaticMeshNvRenderBuffer(const FStaticMeshNvRenderBuffer&);
			FStaticMeshNvRenderBuffer& operator=(const FStaticMeshNvRenderBuffer&);
		};
	}

	/*------------------------------------------------------------------------------
	Forsyth algorithm for cache optimizing index buffers.
	------------------------------------------------------------------------------*/

	namespace ForsythHelper
	{
		/**
		* Converts 16 bit indices to 32 bit prior to passing them into the real OptimizeFaces util method
		*/
		void OptimizeFaces(
			const uint8* Indices,
			bool Is32Bit,
			const uint32 NumIndices,
			uint32 NumVertices,
			uint32* OutIndices,
			uint16 CacheSize
		)
		{
			if (Is32Bit)
			{
				Forsyth::OptimizeFaces((uint32*)Indices, NumIndices, NumVertices, OutIndices, CacheSize);
			}
			else
			{
				// convert to 32 bit
				uint32 Idx;
				TArray<uint32> NewIndices;
				NewIndices.AddUninitialized(NumIndices);
				for (Idx = 0; Idx < NumIndices; ++Idx)
				{
					NewIndices[Idx] = ((uint16*)Indices)[Idx];
				}
				Forsyth::OptimizeFaces(NewIndices.GetData(), NumIndices, NumVertices, OutIndices, CacheSize);
			}

		}

		/**
		* Orders a triangle list for better vertex cache coherency.
		*/
		template<typename IndexDataType, typename Allocator>
		void CacheOptimizeIndexBuffer(TArray<IndexDataType, Allocator>& Indices)
		{
			static_assert(sizeof(IndexDataType) == 2 || sizeof(IndexDataType) == 4, "Indices must be short or int.");
			bool Is32Bit = sizeof(IndexDataType) == 4;

			// Count the number of vertices
			uint32 NumVertices = 0;
			for (int32 Index = 0; Index < Indices.Num(); ++Index)
			{
				if (Indices[Index] > NumVertices)
				{
					NumVertices = Indices[Index];
				}
			}
			NumVertices += 1;

			TArray<uint32> OptimizedIndices;
			OptimizedIndices.AddUninitialized(Indices.Num());
			uint16 CacheSize = 32;
			OptimizeFaces((uint8*)Indices.GetData(), Is32Bit, Indices.Num(), NumVertices, OptimizedIndices.GetData(), CacheSize);

			if (Is32Bit)
			{
				FMemory::Memcpy(Indices.GetData(), OptimizedIndices.GetData(), Indices.Num() * sizeof(IndexDataType));
			}
			else
			{
				for (int32 I = 0; I < OptimizedIndices.Num(); ++I)
				{
					Indices[I] = (uint16)OptimizedIndices[I];
				}
			}
		}
	}
}
