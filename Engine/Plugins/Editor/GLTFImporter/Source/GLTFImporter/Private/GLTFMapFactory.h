// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GLTFMaterialExpressions.h"
#include "GLTFTextureFactory.h"

#include "UObject/ObjectMacros.h"

namespace GLTF
{
	struct FTexture;
	class FTextureFactory;

	struct FPBRMapFactory
	{
		enum class EChannel
		{
			All,
			Red,
			Greeen,
			Blue,
			Alpha,
			RG,
			RGB
		};

		struct FMapChannel
		{
			union {
				float Value;
				float VecValue[3];
			};

			const TCHAR*              ValueName;
			EChannel                  Channel;
			FMaterialExpressionInput* MaterialInput;
			FMaterialExpression*      OutputExpression;

			FMapChannel(float InValue, const TCHAR* InValueName, EChannel InChannel, FMaterialExpressionInput* InMaterialInput, FMaterialExpression* InOutputExpression)
				: FMapChannel(InValueName, InChannel, InMaterialInput, InOutputExpression)
			{
				Value = InValue;
			}

			FMapChannel(float InVecValue[3], const TCHAR* InValueName, EChannel InChannel, FMaterialExpressionInput* InMaterialInput, FMaterialExpression* InOutputExpression)
				: FMapChannel(InValueName, InChannel, InMaterialInput, InOutputExpression)
			{
				SetValue( FVector(InVecValue[0], InVecValue[1], InVecValue[2]) );
			}

			void SetValue(const FVector& Vec)
			{
				*reinterpret_cast<FVector*>(VecValue) = Vec;
			}

		private:
			FMapChannel(const TCHAR* InValueName, EChannel InChannel, FMaterialExpressionInput* InMaterialInput, FMaterialExpression* InOutputExpression)
				: ValueName(InValueName)
				, Channel(InChannel)
				, MaterialInput(InMaterialInput)
				, OutputExpression(InOutputExpression)
			{
			}
		};

		FMaterialElement* CurrentMaterialElement;
		FString           GroupName;

	public:
		FPBRMapFactory(ITextureFactory& TextureFactory);

		void SetParentPackage(UObject* ParentPackage, EObjectFlags Flags);

		void CreateNormalMap(const GLTF::FTexture& Map, int CoordinateIndex, float NormalScale);

		FMaterialExpression* CreateColorMap(const GLTF::FTexture& Map, int CoordinateIndex, const FVector& Color, const TCHAR* MapName,
		                                    const TCHAR* ValueName, ETextureMode TextureMode, FMaterialExpressionInput& MaterialInput);

		FMaterialExpression* CreateColorMap(const GLTF::FTexture& Map, int CoordinateIndex, const FVector4& Color, const TCHAR* MapName,
		                                    const TCHAR* ValueName, ETextureMode TextureMode, FMaterialExpressionInput& MaterialInput);

		FMaterialExpression* CreateScalarMap(const GLTF::FTexture& Map, int CoordinateIndex, float Value, const TCHAR* MapName,
		                                     const TCHAR* ValueName, ETextureMode TextureMode, FMaterialExpressionInput& MaterialInput);

		FMaterialExpressionTexture* CreateTextureMap(const GLTF::FTexture& Map, int CoordinateIndex, const TCHAR* MapName, ETextureMode TextureMode);

		void CreateMultiMap(const GLTF::FTexture& Map, int CoordinateIndex, const TCHAR* MapName, const FMapChannel* MapChannels,
		                    uint32 MapChannelsCount, ETextureMode TextureMode);

	private:
		using FExpressionList = TArray<FMaterialExpression*, TFixedAllocator<4> >;

		template <class ValueExpressionClass, class ValueClass>
		FMaterialExpression* CreateMap(const GLTF::FTexture& Map, int CoordinateIndex, const ValueClass& Value, const TCHAR* MapName,
		                               const TCHAR* ValueName, ETextureMode TextureMode, FMaterialExpressionInput& MaterialInput);

		bool CreateMultiTexture(const GLTF::FTexture& Map, int CoordinateIndex, const TCHAR* MapName, const FMapChannel* MapChannels,
		                        uint32 MapChannelsCount, ETextureMode TextureMode, const FExpressionList& ValueExpressions);

	private:
		ITextureFactory& TextureFactory;
		UObject*         ParentPackage;
		EObjectFlags     Flags;
	};

	inline void FPBRMapFactory::SetParentPackage(UObject* InParentPackage, EObjectFlags InFlags)
	{
		ParentPackage = InParentPackage;
		Flags         = InFlags;
	}

}  // namespace GLTF
