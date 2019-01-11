// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GLTFAccessor.h"

namespace GLTF
{
	namespace
	{
		uint32 GetElementSize(FAccessor::EType Type, FAccessor::EComponentType ComponentType)
		{
			static const uint8 ComponentSize[]      = {0, 1, 1, 2, 2, 4, 4};      // keep in sync with EComponentType
			static const uint8 ComponentsPerValue[] = {0, 1, 2, 3, 4, 4, 9, 16};  // keep in sync with EType

			static_assert(
			    (int)FAccessor::EType::Unknown == 0 && ((int)FAccessor::EType::Count) == (sizeof(ComponentsPerValue) / sizeof(ComponentsPerValue[0])),
			    "EType doesn't match!");
			static_assert((int)FAccessor::EComponentType::None == 0 &&
			                  ((int)FAccessor::EComponentType::Count) == (sizeof(ComponentSize) / sizeof(ComponentSize[0])),
			              "EComponentType doesn't match!");

			return ComponentsPerValue[(int)Type] * ComponentSize[(int)ComponentType];
		}
	}

	FAccessor::FAccessor(uint32 InCount, EType InType, EComponentType InCompType, bool InNormalized)
	    : Count(InCount)
	    , Type(InType)
	    , ComponentType(InCompType)
	    , Normalized(InNormalized)
	{
	}

	uint32 FAccessor::GetUnsignedInt(uint32 Index) const
	{
		return 0;
	}

	void FAccessor::GetUnsignedInt16x4(uint32 Index, uint16 Values[4]) const {}

	FVector2D FAccessor::GetVec2(uint32 Index) const
	{
		return FVector2D::ZeroVector;
	}

	FVector FAccessor::GetVec3(uint32 Index) const
	{
		return FVector::ZeroVector;
	}

	FVector4 FAccessor::GetVec4(uint32 Index) const
	{
		return FVector4();
	}

	FMatrix FAccessor::GetMat4(uint32 Index) const
	{
		return FMatrix::Identity;
	}

	void FAccessor::GetUnsignedIntArray(TArray<uint32>& Buffer) const {}

	void FAccessor::GetVec2Array(TArray<FVector2D>& Buffer) const {}

	void FAccessor::GetVec3Array(TArray<FVector>& Buffer) const {}

	void FAccessor::GetVec4Array(TArray<FVector4>& Buffer) const {}

	void FAccessor::GetMat4Array(TArray<FMatrix>& Buffer) const {}

	//

	FValidAccessor::FValidAccessor(FBufferView& InBufferView, uint32 InOffset, uint32 InCount, EType InType, EComponentType InCompType,
	                               bool InNormalized)
	    : FAccessor(InCount, InType, InCompType, InNormalized)
	    , BufferView(InBufferView)
	    , ByteOffset(InOffset)
	    // if zero then tightly packed
	    , ElementSize(BufferView.ByteStride == 0 ? GetElementSize(Type, ComponentType) : BufferView.ByteStride)
	{
	}

	bool FValidAccessor::IsValid() const
	{
		return true;
	}

	uint32 FValidAccessor::GetUnsignedInt(uint32 Index) const
	{
		// should be Scalar, not Normalized, unsigned integer (8, 16 or 32 bit)

		if (Index < Count)
		{
			if (Type == EType::Scalar && !Normalized)
			{
				const uint8* ValuePtr = DataAt(Index);

				switch (ComponentType)
				{
					case EComponentType::U8:
						return *ValuePtr;
					case EComponentType::U16:
						return *reinterpret_cast<const uint16*>(ValuePtr);
					case EComponentType::U32:
						return *reinterpret_cast<const uint32*>(ValuePtr);
					default:
						break;
				}
			}
		}

		check(false);
		return 0;
	}

	void FValidAccessor::GetUnsignedInt16x4(uint32 Index, uint16 Values[4]) const
	{
		// should be Vec4, not Normalized, unsigned integer (8 or 16 bit)

		if (Index < Count)
		{
			if (Type == EType::Vec4 && !Normalized)
			{
				const void* Pointer = DataAt(Index);

				switch (ComponentType)
				{
					case EComponentType::U8:
						for (int i = 0; i < 4; ++i)
						{
							Values[i] = *(const uint8*)Pointer;
						}
					case EComponentType::U16:
						for (int i = 0; i < 4; ++i)
						{
							Values[i] = *(const uint16*)Pointer;
						}
					default:
						break;
				}
			}
		}
	}

	FVector2D FValidAccessor::GetVec2(uint32 Index) const
	{
		// Spec-defined attributes (TEXCOORD_0, TEXCOORD_1) use only these formats:
		// - F32
		// - U8 normalized
		// - U16 normalized
		// Custom attributes can use any CompType, so add support for those when needed.

		if (Index < Count)
		{
			if (Type == EType::Vec2)  // strict format match, unlike GPU shader fetch
			{
				const void* Pointer = DataAt(Index);

				if (ComponentType == EComponentType::F32)
				{
					// copy float vec2 directly from buffer
					return *reinterpret_cast<const FVector2D*>(Pointer);
				}
				else if (Normalized)
				{
					// convert to 0..1
					if (ComponentType == EComponentType::U8)
					{
						const uint8*    P = static_cast<const uint8*>(Pointer);
						constexpr float S = 1.0f / 255.0f;
						return FVector2D(S * P[0], S * P[1]);
					}
					else if (ComponentType == EComponentType::U16)
					{
						const uint16*   P = static_cast<const uint16*>(Pointer);
						constexpr float S = 1.0f / 65535.0f;
						return FVector2D(S * P[0], S * P[1]);
					}
				}
			}
		}

		// unsupported format
		return FVector2D::ZeroVector;
	}

	FVector FValidAccessor::GetVec3(uint32 Index) const
	{
		// Spec-defined attributes (POSITION, NORMAL, COLOR_0) use only these formats:
		// - F32
		// - U8 normalized
		// - U16 normalized
		// Custom attributes can use any CompType, so add support for those when needed.

		if (Index < Count)
		{
			if (Type == EType::Vec3)  // strict format match, unlike GPU shader fetch
			{
				const void* Pointer = DataAt(Index);

				if (ComponentType == EComponentType::F32)
				{
					// copy float vec3 directly from buffer
					return *reinterpret_cast<const FVector*>(Pointer);
				}
				else if (Normalized)
				{
					// convert to 0..1
					if (ComponentType == EComponentType::U8)
					{
						const uint8*    P = static_cast<const uint8*>(Pointer);
						constexpr float S = 1.0f / 255.0f;
						return FVector(S * P[0], S * P[1], S * P[2]);
					}
					else if (ComponentType == EComponentType::U16)
					{
						const uint16*   P = static_cast<const uint16*>(Pointer);
						constexpr float S = 1.0f / 65535.0f;
						return FVector(S * P[0], S * P[1], S * P[2]);
					}
				}
			}
		}

		// unsupported format
		return FVector::ZeroVector;
	}

	FVector4 FValidAccessor::GetVec4(uint32 Index) const
	{
		// Spec-defined attributes (TANGENT, COLOR_0) use only these formats:
		// - F32
		// - U8 normalized
		// - U16 normalized
		// Custom attributes can use any CompType, so add support for those when needed.

		if (Index < Count)
		{
			if (Type == EType::Vec4)  // strict format match, unlike GPU shader fetch
			{
				const void* Pointer = DataAt(Index);

				if (ComponentType == EComponentType::F32)
				{
					// copy float vec4 directly from buffer
					return *reinterpret_cast<const FVector4*>(Pointer);
				}
				else if (Normalized)
				{
					// convert to 0..1
					if (ComponentType == EComponentType::U8)
					{
						const uint8*    P = static_cast<const uint8*>(Pointer);
						constexpr float S = 1.0f / 255.0f;
						return FVector4(S * P[0], S * P[1], S * P[2], S * P[3]);
					}
					else if (ComponentType == EComponentType::U16)
					{
						const uint16*   P = static_cast<const uint16*>(Pointer);
						constexpr float S = 1.0f / 65535.0f;
						return FVector4(S * P[0], S * P[1], S * P[2], S * P[3]);
					}
				}
			}
		}

		// unsupported format
		return FVector4();
	}

	FMatrix FValidAccessor::GetMat4(uint32 Index) const
	{
		// Focus on F32 for now, add other types as needed.

		FMatrix Matrix;

		if (Index < Count)
		{
			if (Type == EType::Mat4)  // strict format match, unlike GPU shader fetch
			{
				const void* Pointer = DataAt(Index);

				if (ComponentType == EComponentType::F32)
				{
					// copy float vec4 directly from buffer
					const float* P = static_cast<const float*>(Pointer);
					for (int32 Row = 0; Row < 4; ++Row)
					{
						for (int32 Col = 0; Col < 4; ++Col)
						{
							// glTF stores matrix elements in column major order
							// Unreal's FMatrix is row major
							Matrix.M[Row][Col] = P[Col * 4 + Row];
						}
					}
				}
				else
				{
					check(false);  // are normalized int types valid?
				}
			}
		}

		// unsupported format
		return Matrix;
	}

	void FValidAccessor::GetUnsignedIntArray(TArray<uint32>& Buffer) const
	{
		Buffer.Reserve(Count);
		for (uint32 i = 0; i < Count; ++i)
		{
			Buffer.Push(GetUnsignedInt(i));
		}
	}

	void FValidAccessor::GetVec2Array(TArray<FVector2D>& Buffer) const
	{
		Buffer.Reserve(Count);
		for (uint32 i = 0; i < Count; ++i)
		{
			Buffer.Push(GetVec2(i));
		}
	}

	void FValidAccessor::GetVec3Array(TArray<FVector>& Buffer) const
	{
		Buffer.Reserve(Count);
		for (uint32 i = 0; i < Count; ++i)
		{
			Buffer.Push(GetVec3(i));
		}
	}

	void FValidAccessor::GetVec4Array(TArray<FVector4>& Buffer) const
	{
		Buffer.Reserve(Count);
		for (uint32 i = 0; i < Count; ++i)
		{
			Buffer.Push(GetVec4(i));
		}
	}

	void FValidAccessor::GetMat4Array(TArray<FMatrix>& Buffer) const
	{
		Buffer.Reserve(Count);
		for (uint32 i = 0; i < Count; ++i)
		{
			Buffer.Push(GetMat4(i));
		}
	}

	inline const uint8* FValidAccessor::DataAt(uint32 Index) const
	{
		check(ElementSize);
		const uint32 Offset = Index * ElementSize;
		return BufferView.DataAt(Offset + ByteOffset);
	}

	//

	bool FVoidAccessor::IsValid() const
	{
		return false;
	}

}  // namespace GLTF
