// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GLTFAccessor.h"

#include "ConversionUtilities.h"

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

		template <class ReturnType, uint32 Count>
		ReturnType GetNormalized(FAccessor::EComponentType ComponentType, const void* Pointer)
		{
			ReturnType Res;

			// convert to 0..1
			if (ComponentType == FAccessor::EComponentType::U8)
			{
				const uint8*    P = static_cast<const uint8*>(Pointer);
				constexpr float S = 1.0f / 255.0f;
				for (uint32 Index = 0; Index < Count; ++Index)
				{
					Res[Index] = P[Index] * S;
				}
			}
			else if (ComponentType == FAccessor::EComponentType::U16)
			{
				const uint16*   P = static_cast<const uint16*>(Pointer);
				constexpr float S = 1.0f / 65535.0f;
				for (uint32 Index = 0; Index < Count; ++Index)
				{
					Res[Index] = P[Index] * S;
				}
			}
			else
				check(false);

			return Res;
		}

		FMatrix GetMatrix(const void* Pointer)
		{
			// copy float mat4 directly from buffer
			const float* P = static_cast<const float*>(Pointer);

			FMatrix Matrix;
			for (int32 Row = 0; Row < 4; ++Row)
			{
				for (int32 Col = 0; Col < 4; ++Col)
				{
					// glTF stores matrix elements in column major order
					// Unreal's FMatrix is row major
					Matrix.M[Row][Col] = P[Col * 4 + Row];
				}
			}
			return Matrix;
		}

		template <typename DstT, typename SrcT>
		void Copy(DstT* Dst, const SrcT* Src, uint32 Count)
		{
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				*(Dst++) = *(Src++);
			}
		}

		template <typename DstT, uint32 ElementCount>
		void CopyNormalized(DstT* Dst, const void* Src, FAccessor::EComponentType ComponentType, uint32 Count)
		{
			// convert to 0..1
			if (ComponentType == FAccessor::EComponentType::U8)
			{
				const uint8*    P = static_cast<const uint8*>(Src);
				constexpr float S = 1.0f / 255.0f;
				for (uint32 Index = 0; Index < Count; ++Index)
				{
					DstT& VecDst = *Dst++;
					for (uint32 J = 0; J < ElementCount; ++J)
					{
						VecDst[J] = P[J] * S;
					}
					P += ElementCount;
				}
			}
			else if (ComponentType == FAccessor::EComponentType::U16)
			{
				const uint16*   P = static_cast<const uint16*>(Src);
				constexpr float S = 1.0f / 65535.0f;
				for (uint32 Index = 0; Index < Count; ++Index)
				{
					DstT& VecDst = *Dst++;
					for (uint32 J = 0; J < ElementCount; ++J)
					{
						VecDst[J] = P[J] * S;
					}
					P += ElementCount;
				}
			}
			else
				check(false);
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

	float FAccessor::GetFloat(uint32 Index) const
	{
		return 0.f;
	}

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

	void FAccessor::GetUnsignedIntArray(uint32* Buffer) const {}

	void FAccessor::GetFloatArray(float* Buffer) const {}

	void FAccessor::GetVec2Array(FVector2D* Buffer) const {}

	void FAccessor::GetVec3Array(FVector* Buffer) const {}

	void FAccessor::GetVec4Array(FVector4* Buffer) const {}

	void FAccessor::GetMat4Array(FMatrix* Buffer) const {}

	void FAccessor::GetCoordArray(FVector* Buffer) const
	{
		GetVec3Array(Buffer);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			Buffer[Index] = ConvertVec3(Buffer[Index]);
		}
	}

	void FAccessor::GetQuatArray(FVector4* Buffer) const
	{
		GetVec4Array(Buffer);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FVector4& Value = Buffer[Index];
			FQuat     Quat(Value[0], Value[1], Value[2], Value[3]);
			//	Quat    = GLTF::ConvertQuat(Quat);
			Value.X = Quat.X;
			Value.Y = Quat.Y;
			Value.Z = Quat.Z;
			Value.W = Quat.W;
		}
	}

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
				const void* ValuePtr = DataAt(Index);
				switch (ComponentType)
				{
					case EComponentType::U8:
						for (int i = 0; i < 4; ++i)
						{
							Values[i] = *(const uint8*)ValuePtr;
						}
						return;
					case EComponentType::U16:
						for (int i = 0; i < 4; ++i)
						{
							Values[i] = *(const uint16*)ValuePtr;
						}
						return;
					default:
						break;
				}
			}
		}
		check(false);
	}

	float FValidAccessor::GetFloat(uint32 Index) const
	{
		// should be Scalar float

		if (Index < Count)
		{
			if (Type == EType::Scalar && !Normalized)
			{
				const uint8* ValuePtr = DataAt(Index);
				switch (ComponentType)
				{
					case EComponentType::F32:
						return *ValuePtr;
					default:
						break;
				}
			}
		}

		check(false);
		return 0.f;
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
					return GetNormalized<FVector2D, 2>(ComponentType, Pointer);
				}
			}
		}

		check(false);
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
					return GetNormalized<FVector, 3>(ComponentType, Pointer);
				}
			}
		}

		check(false);
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
					return GetNormalized<FVector4, 4>(ComponentType, Pointer);
				}
			}
		}

		check(false);
		return FVector4();
	}

	FMatrix FValidAccessor::GetMat4(uint32 Index) const
	{
		// Focus on F32 for now, add other types as needed.

		if (Index < Count)
		{
			if (Type == EType::Mat4)  // strict format match, unlike GPU shader fetch
			{
				if (ComponentType == EComponentType::F32)
				{
					const void* Pointer = DataAt(Index);
					return GetMatrix(Pointer);
				}
			}
		}

		check(false);
		return FMatrix();
	}

	void FValidAccessor::GetUnsignedIntArray(uint32* Buffer) const
	{
		if (Type == EType::Scalar && !Normalized)
		{
			const uint8* Src = DataAt(0);
			switch (ComponentType)
			{
				case EComponentType::U8:
					Copy(Buffer, reinterpret_cast<const uint8*>(Src), Count);
					return;
				case EComponentType::U16:
					Copy(Buffer, reinterpret_cast<const uint16*>(Src), Count);
					return;
				case EComponentType::U32:
					memcpy(Buffer, Src, Count * sizeof(uint32));
					return;
				default:
					break;
			}
		}

		check(false);
	}

	void FValidAccessor::GetFloatArray(float* Buffer) const
	{
		if (Type == EType::Scalar && !Normalized)
		{
			const uint8* Src = DataAt(0);
			switch (ComponentType)
			{
				case EComponentType::F32:
					memcpy(Buffer, Src, Count * sizeof(float));
					return;
				default:
					break;
			}
		}

		check(false);
	}

	void FValidAccessor::GetVec2Array(FVector2D* Buffer) const
	{
		if (Type == EType::Vec2)  // strict format match, unlike GPU shader fetch
		{
			const void* Src = DataAt(0);

			if (ComponentType == EComponentType::F32)
			{
				// copy float vec2 directly from buffer
				memcpy(Buffer, Src, Count * sizeof(FVector2D));
				return;
			}
			else if (Normalized)
			{
				CopyNormalized<FVector2D, 2>(Buffer, Src, ComponentType, Count);
				return;
			}
		}
		check(false);
	}

	void FValidAccessor::GetVec3Array(FVector* Buffer) const
	{
		if (Type == EType::Vec3)  // strict format match, unlike GPU shader fetch
		{
			const void* Src = DataAt(0);

			if (ComponentType == EComponentType::F32)
			{
				// copy float vec3 directly from buffer
				memcpy(Buffer, Src, Count * sizeof(FVector));
				return;
			}
			else if (Normalized)
			{
				CopyNormalized<FVector, 3>(Buffer, Src, ComponentType, Count);
				return;
			}
		}
		check(false);
	}

	void FValidAccessor::GetVec4Array(FVector4* Buffer) const
	{
		if (Type == EType::Vec4)  // strict format match, unlike GPU shader fetch
		{
			const void* Src = DataAt(0);

			if (ComponentType == EComponentType::F32)
			{
				// copy float vec4 directly from buffer
				memcpy(Buffer, Src, Count * sizeof(FVector4));
				return;
			}
			else if (Normalized)
			{
				CopyNormalized<FVector4, 4>(Buffer, Src, ComponentType, Count);
				return;
			}
		}
		check(false);
	}

	void FValidAccessor::GetMat4Array(FMatrix* Buffer) const
	{
		if (Type == EType::Mat4 && ComponentType == EComponentType::F32)  // strict format match, unlike GPU shader fetch
		{
			for (uint32 Index = 0; Index < Count; ++Index)
			{
				const void* Pointer = DataAt(Index);
				Buffer[Index]       = GetMatrix(Pointer);
			}
			return;
		}

		check(false);
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
