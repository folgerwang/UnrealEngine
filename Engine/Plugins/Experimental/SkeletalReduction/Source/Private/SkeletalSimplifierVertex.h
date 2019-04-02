// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SkeletalSimplifierLinearAlgebra.h"

#include "Math/Color.h"     // FLinearColor
#include "Math/Vector2D.h"  // FVector2d
#include "Math/Vector.h"    // FVector
#include "MeshBuild.h"      // ApprxEquals for normals & UVs
#include "Templates/UnrealTemplate.h"

namespace SkeletalSimplifier
{
	namespace VertexTypes
	{
		using namespace SkeletalSimplifier::LinearAlgebra;

		/**
		* Class that holds the dense vertex attributes.
		* Normal, Tangent, BiTangent, Color, TextureCoords
		*
		* NB: this can be extended with additional float-type attributes.
		*/
		template <int32 NumTexCoords>
		class TBasicVertexAttrs
		{
		public:
			enum { NumUVs = NumTexCoords};

			// NB: if you update the number of float-equivalent attributes, the '13' will have to be updated.
			typedef TDenseVecD<13 + 2 * NumTexCoords>      DenseVecDType;
			
			typedef DenseVecDType                          WeightArrayType;
			typedef TDenseBMatrix<13 + 2 * NumTexCoords>   DenseBMatrixType;

			// NB: Required that these all have float storage.
			// - Base Attributes: size = 13 + 2 * NumTexCoord

			FVector			Normal;      // 0, 1, 2
			FVector			Tangent;     // 3, 4, 5
			FVector         BiTangent;   // 6, 7, 8
			FLinearColor	Color;       // 9, 10, 11, 12
			FVector2D		TexCoords[NumTexCoords];  // 13, .. 13 + NumTexCoords * 2 - 1

		public:
			// vector semantic wrapper for raw array
			typedef TDenseArrayWrapper<float>                            DenseAttrAccessor;


			/**
			 * default construct to zero values
			 */
			TBasicVertexAttrs() :
				Normal(ForceInitToZero),
				Tangent(ForceInitToZero),
				BiTangent(ForceInitToZero),
				Color(ForceInitToZero)
			{
				for (int32 i = 0; i < NumTexCoords; ++i)
				{
					TexCoords[i] = FVector2D(ForceInitToZero);
				}
			}

			/**
			* copy construct
			*/
			TBasicVertexAttrs(const TBasicVertexAttrs& Other) :
				Normal(Other.Normal),
				Tangent(Other.Tangent),
				BiTangent(Other.BiTangent),
				Color(Other.Color)
			{
				for (int32 i = 0; i < NumTexCoords; ++i)
				{
					TexCoords[i] = Other.TexCoords[i];
				}
			}

			/**
			* Number of float equivalents. 
			*/
			static int32 Size() { return sizeof(TBasicVertexAttrs) / sizeof(float); /*( return 13 + 2 * NumTexCoords;*/ }

			/**
			* Get access to the data as a generic linear array of floats.
			*/
			DenseAttrAccessor  AsDenseAttrAccessor() { return DenseAttrAccessor((float*)&Normal, Size()); }
			const DenseAttrAccessor  AsDenseAttrAccessor() const { return DenseAttrAccessor((float*)&Normal, Size()); }

			/**
			* Assignment operator. 
			*/
			TBasicVertexAttrs& operator=(const TBasicVertexAttrs& Other)
			{
				DenseAttrAccessor MyData = AsDenseAttrAccessor();
				const DenseAttrAccessor OtherData = Other.AsDenseAttrAccessor();

				const int32 NumElements = MyData.Num();
				for (int32 i = 0; i < NumElements; ++i)
				{
					MyData[i] = OtherData[i];
				}

				return *this;
			}
			

			/**
			* Method to insure that the attribute values are valid by correcting any invalid ones.
			*/
			void Correct()
			{
				Normal.Normalize();
				Tangent -= FVector::DotProduct(Tangent, Normal) * Normal;
				Tangent.Normalize();
				BiTangent -= FVector::DotProduct(BiTangent, Normal) * Normal;
				BiTangent -= FVector::DotProduct(BiTangent, Tangent) * Tangent;
				BiTangent.Normalize();
				Color = Color.GetClamped();
			}

			/**
			* Strict equality test.
			*/
			bool  operator==(const TBasicVertexAttrs& Other)
			{
				bool bIsEqual =
					Tangent == Other.Tangent
					&& BiTangent == Other.BiTangent
					&& Normal == Other.Normal
					&& Color == Other.Color;

				for (int32 UVIndex = 0; bIsEqual && UVIndex < NumTexCoords; UVIndex++)
				{
					bIsEqual = bIsEqual && UVsEqual(TexCoords[UVIndex], Other.TexCoords[UVIndex]);
				}

				return bIsEqual;
			}

			/**
			* Approx equality test.
			*/
			bool IsApproxEquals(const TBasicVertexAttrs& Other) const
			{
				bool bIsApprxEqual =
					NormalsEqual(Tangent, Other.Tangent) &&
					NormalsEqual(BiTangent, Other.BiTangent) &&
					NormalsEqual(Normal, Other.Normal) &&
					Color.Equals(Other.Color);

				for (int32 UVIndex = 0; bIsApprxEqual && UVIndex < NumTexCoords; UVIndex++)
				{
					bIsApprxEqual = bIsApprxEqual && UVsEqual(TexCoords[UVIndex], Other.TexCoords[UVIndex]);
				}

				return bIsApprxEqual;
			}

		};

		/**
		* Sparse attributes.
		* Used to hold bone weights where the bone ID is the attribute key.
		*/
		class BoneSparseVertexAttrs : public SparseVecD
		{
		public:

			#define  SmallBoneSize 1.e-12  

			/**
			* Deletes smallest bones if currently more than MaxBoneNumber, and maintain the normalization of weight (all sum to 1).
			* keeping the bones sorted internally by weight (largest to smallest).
			*/
			void Correct( int32 MaxBoneNumber = 8)  
			{
				if (!bIsEmpty())
				{
				
					DeleteSmallBones();

					

					if (SparseData.Num() > MaxBoneNumber)
					{
						// sort by value from large to small
						SparseData.ValueSort([](double A, double B)->bool { return B < A; });

						SparseContainer Tmp;
						int32 Count = 0;
						for (const auto& BoneData : SparseData)
						{
							if (Count == MaxBoneNumber) break;
							Tmp.Add(BoneData.Key, BoneData.Value);
							Count++;
						}
						Swap(SparseData, Tmp);

					}

					SparseData.ValueSort([](double A, double B)->bool { return B < A; });

					Normalize();
				}
			}

			/**
			* Note: the norm here is the sum of weights (not L2 or L1 norm)
			*/
			void Normalize()
			{
				double SumOfWeights = SumValues();
				if (FMath::Abs(SumOfWeights) > 8 * SmallBoneSize)
				{
					double Inv = 1. / SumOfWeights;

					operator*=(Inv);
				}
				else
				{
					Empty();
				}
			}

			/**
			* Removes bones with very small weights. This may not be appropriate for all models.
			*/
			void DeleteSmallBones()
			{
				SparseContainer Tmp;
				for (const auto& BoneData : SparseData)
				{
					if (BoneData.Value > SmallBoneSize ) Tmp.Add(BoneData.Key, BoneData.Value);
				}
				Swap(SparseData, Tmp);
			}

			/**
			* Remove all bones.
			*/
			void Empty()
			{
				SparseContainer Tmp;
				Swap(SparseData, Tmp);
			}

			/**
			* Compare two sparse arrays.  A small bone weight will be equivalent to no bone weight.
			*/
			bool IsApproxEquals(const BoneSparseVertexAttrs& Other, double Tolerance = (double)KINDA_SMALL_NUMBER)
			{

				SparseVecD::SparseContainer Tmp = SparseData;
				auto AddToElement = [&Tmp](const int32 j, const double Value)
				{
					double* Data = Tmp.Find(j);
					if (Data != nullptr)
					{
						*Data += Value;
					}
					else
					{
						Tmp.Add(j, Value);
					}
				};

				// Tmp = SparseData - Other.SparseData
				for (const auto& Data : Other.SparseData)
				{
					AddToElement(Data.Key, -Data.Value);
				}

				bool bIsApproxEquals = true;
				for (const auto& Data : Tmp)
				{
					bIsApproxEquals = bIsApproxEquals &&
						(Data.Value < Tolerance) && (-Data.Value < Tolerance);
				}

				return bIsApproxEquals;

			}


		};



		/**
		* Simplifier vertex type that has been extended to include additional sparse data
		*
		* Implements the interface needed by template simplifier code.
		*/

		template< typename BasicAttrContainerType_, typename AttrContainerType_ , typename BoneContainerType_>
		class TSkeletalSimpVert
		{
			typedef TSkeletalSimpVert< BasicAttrContainerType_, AttrContainerType_, BoneContainerType_ >        VertType;

		public:

			typedef BoneContainerType_                                                   BoneContainer;
			typedef AttrContainerType_                                                   AttrContainerType;
			typedef BasicAttrContainerType_                                              BasicAttrContainerType;
			typedef typename BasicAttrContainerType::DenseVecDType                       DenseVecDType;

			// The Vertex Point
			uint32			MaterialIndex;
			FVector			Position;

			// Additional weight used to select against collapse.

			float           SpecializedWeight;

			// ---- Vertex Attributes ------------------------------------------------------------------
			//      3 Types:  Dense Attributes, Sparse Attributes,  Bones
			//      Dense & Sparse Attributes are used in quadric calculation
			//      Bones are excluded from the quadric error, but maybe used in imposing penalties for collapse.

			// - Base Attributes: Normal, Tangent, BiTangent, Color, TexCoords: size = 13 + 2 * NumTexCoord
			BasicAttrContainerType  BasicAttributes;

			// - Additional Attributes : size not determined at compile time
			AttrContainerType  AdditionalAttributes;

			// - Sparse Bones : size not determined at compile time
			BoneContainer  SparseBones;

		public:

			typedef typename  BasicAttrContainerType::DenseAttrAccessor    DenseAttrAccessor;


			TSkeletalSimpVert() :
				MaterialIndex(0),
				Position(ForceInitToZero),
				SpecializedWeight(0.f),
				BasicAttributes()
			{
			}

			// copy constructor
			TSkeletalSimpVert(const TSkeletalSimpVert& Other) :
				MaterialIndex(Other.MaterialIndex),
				Position(Other.Position),
				SpecializedWeight(Other.SpecializedWeight),
				BasicAttributes(Other.BasicAttributes),
				AdditionalAttributes(Other.AdditionalAttributes),
				SparseBones(Other.SparseBones)
			{}



			uint32 GetMaterialIndex() const { return MaterialIndex; }
			FVector& GetPos() { return Position; }
			const FVector&	GetPos() const { return Position; }


			// Access to the base attributes.  Note these are really floats. 

			static int32  NumBaseAttributes() { return BasicAttrContainerType::Size(); }
			DenseAttrAccessor GetBasicAttrAccessor() { return BasicAttributes.AsDenseAttrAccessor(); }
			const DenseAttrAccessor GetBasicAttrAccessor() const { return BasicAttributes.AsDenseAttrAccessor(); }

			// Additional attributes maybe dense or sparse.  This should hold bone weights and the like.

			AttrContainerType& GetAdditionalAttrContainer() { return AdditionalAttributes; }
			const AttrContainerType&  GetAdditionalAttrContainer() const { return AdditionalAttributes; }


			// Bones, not used in Quadric calculation.

			BoneContainer& GetSparseBones() { return SparseBones; }
			const BoneContainer&  GetSparseBones() const { return SparseBones; }

			// Insure that the attribute values are valid
			// by correcting any invalid ones.

			void Correct()
			{
				// This fixes the normal, tangent, and bi-tangent.
				BasicAttributes.Correct();

				AdditionalAttributes.Correct();

				SparseBones.Correct();
			}



			TSkeletalSimpVert& operator=(const TSkeletalSimpVert& Other)
			{
				MaterialIndex        = Other.MaterialIndex;
				Position             = Other.Position;
				SpecializedWeight    = Other.SpecializedWeight;
				BasicAttributes      = Other.BasicAttributes;
				AdditionalAttributes = Other.AdditionalAttributes;
				SparseBones          = Other.SparseBones;
				return *this;
			}

			// Tests approximate equality using specialized 
			// comparison functions.
			// NB: This functionality exists to help in welding verts 
			//     prior to simplification, but is not used in the simplifier itself.
			bool Equals(const VertType& Other) const
			{
				bool bIsApprxEquals = 
					(MaterialIndex == Other.MaterialIndex)
			     && PointsEqual(Position, Other.Position);

				bIsApprxEquals = bIsApprxEquals
					          && FMath::IsNearlyEqual(SpecializedWeight, Other.SpecializedWeight, 1.e-5);

				bIsApprxEquals = bIsApprxEquals 
					          && BasicAttributes.IsApproxEquals(Other.BasicAttributes);

				bIsApprxEquals = bIsApprxEquals
				 	          && AdditionalAttributes.IsApproxEquals(Other.AdditionalAttributes);

				bIsApprxEquals = bIsApprxEquals
					           && SparseBones.IsApproxEquals(Other.SparseBones);
				return bIsApprxEquals;
			}

			// Exact equality tests

			bool operator==(const VertType& Other) const
			{
				bool bIsEqual = (MaterialIndex == Other.MaterialIndex) &&
					            (Position == Other.Position);
				bIsEqual = bIsEqual && (SpecializedWeight == Other.SpecializedWeight);
				bIsEqual = bIsEqual && (GetBasicAttrAccessor() == Other.GetBasicAttrAccessor());
				bIsEqual = bIsEqual && (GetAdditionalAttrContainer() == Other.GetAdditionalAttrContainer());
				bIsEqual = bIsEqual && (SparseBones == Other.SparseBones);

				return bIsEqual;
			}

			// Standard operator overloading

			VertType operator+(const VertType& Other) const
			{
				VertType Result(*this);
				Result.Position += Other.Position;

				Result.SpecializedWeight = FMath::Max(Result.SpecializedWeight, Other.SpecializedWeight);

				auto BaseAttrs = Result.GetBasicAttrAccessor();
				BaseAttrs += Other.GetBasicAttrAccessor();

				AttrContainerType& SparseAttrs = Result.GetAdditionalAttrContainer();
				SparseAttrs += Other.GetAdditionalAttrContainer();

				SparseBones += Other.SparseBones;

				return Result;
			}

			VertType operator-(const VertType& Other) const
			{
				VertType Result(*this);

				Result.Position -= Other.Position;

				Result.SpecializedWeight = FMath::Max(Result.SpecializedWeight, Other.SpecializedWeight);

				auto BaseAttrs = Result.GetBasicAttrAccessor();
				BaseAttrs -= Other.GetBasicAttrAccessor();

				AttrContainerType& SparseAttrs = Result.GetAdditionalAttrContainer();
				SparseAttrs -= Other.GetAdditionalAttrContainer();

				SparseBones -= Other.SparseBones;
				return Result;
			}

			VertType operator*(const float Scalar) const
			{
				VertType Result(*this);
				Result.Position *= Scalar;

				auto BaseAttrs = Result.GetBasicAttrAccessor();
				BaseAttrs *= Scalar;

				AttrContainerType& SparseAttrs = Result.GetAdditionalAttrContainer();
				SparseAttrs *= Scalar;

				BoneContainer& ResultBones = Result.GetSparseBones();

				ResultBones *= Scalar;
				return Result;
			}

			VertType operator/(const float Scalar) const
			{
				float invScalar = 1.0f / Scalar;
				return (*this) * invScalar;
			}
		};
	}
}