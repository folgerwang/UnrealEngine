// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace GLTF
{
	struct GLTF_API FBuffer
	{
		const uint32 ByteLength;
		const uint8* Data;

		FBuffer(uint32 InByteLength)
		    : ByteLength(InByteLength)
		    , Data(nullptr)
		{
		}

		const uint8* DataAt(uint32 Offset) const
		{
			return Data + Offset;
		}
	};

	struct GLTF_API FBufferView
	{
		const FBuffer& Buffer;
		const uint32   ByteOffset;
		const uint32   ByteLength;
		// if zero then accessor elements are tightly packed, i.e., effective stride equals the size of the element
		const uint32 ByteStride;  // range 4..252

		FBufferView(const FBuffer& InBuffer, uint32 InOffset, uint32 InLength, uint32 InStride)
		    : Buffer(InBuffer)
		    , ByteOffset(InOffset)
		    , ByteLength(InLength)
		    , ByteStride(InStride)
		{
			// check that view fits completely inside the buffer
		}

		const uint8* DataAt(uint32 Offset) const
		{
			return Buffer.DataAt(Offset + ByteOffset);
		}
	};

	struct GLTF_API FAccessor
	{
		// accessor stores the data but has no usage semantics

		enum class EType
		{
			Unknown,
			Scalar,
			Vec2,
			Vec3,
			Vec4,
			Mat2,
			Mat3,
			Mat4,
			Count
		};

		enum class EComponentType
		{
			None,
			S8,   // signed byte
			U8,   // unsigned byte
			S16,  // signed short
			U16,  // unsigned short
			U32,  // unsigned int -- only valid for indices, not attributes
			F32,  // float
			Count
		};

		const uint32         Count;
		const EType          Type;
		const EComponentType ComponentType;
		const bool           Normalized;

		FAccessor(uint32 InCount, EType InType, EComponentType InComponentType, bool InNormalized);

		virtual bool IsValid() const = 0;

		virtual uint32 GetUnsignedInt(uint32 Index) const;
		virtual void   GetUnsignedInt16x4(uint32 Index, uint16 Values[4]) const;

		virtual FVector2D GetVec2(uint32 Index) const;
		virtual FVector   GetVec3(uint32 Index) const;
		virtual FVector4  GetVec4(uint32 Index) const;

		virtual FMatrix GetMat4(uint32 Index) const;

		virtual void GetUnsignedIntArray(TArray<uint32>& Buffer) const;
		virtual void GetVec2Array(TArray<FVector2D>& Buffer) const;
		virtual void GetVec3Array(TArray<FVector>& Buffer) const;
		virtual void GetVec4Array(TArray<FVector4>& Buffer) const;
		virtual void GetMat4Array(TArray<FMatrix>& Buffer) const;
	};

	struct GLTF_API FValidAccessor final : FAccessor
	{
		FValidAccessor(FBufferView& InBufferView, uint32 InOffset, uint32 InCount, EType InType, EComponentType InCompType, bool InNormalized);

		bool IsValid() const override;

		uint32 GetUnsignedInt(uint32 Index) const override;
		void   GetUnsignedInt16x4(uint32 Index, uint16 Values[4]) const override;

		FVector2D GetVec2(uint32 Index) const override;
		FVector   GetVec3(uint32 Index) const override;
		FVector4  GetVec4(uint32 Index) const override;

		FMatrix GetMat4(uint32 Index) const override;

		void GetUnsignedIntArray(TArray<uint32>& Buffer) const override;
		void GetVec2Array(TArray<FVector2D>& Buffer) const override;
		void GetVec3Array(TArray<FVector>& Buffer) const override;
		void GetVec4Array(TArray<FVector4>& Buffer) const override;
		void GetMat4Array(TArray<FMatrix>& Buffer) const override;

	private:
		const FBufferView& BufferView;
		const uint32       ByteOffset;
		const uint32       ElementSize;

		const uint8* DataAt(uint32 Index) const;
	};

	struct GLTF_API FVoidAccessor final : FAccessor
	{
		FVoidAccessor()
		    : FAccessor(0, EType::Scalar, EComponentType::S8, false)
		{
		}

		bool IsValid() const override;
	};

}  // namespace GLTF
