// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GLTFMapFactory.h"

#include "GLTFMaterial.h"
#include "GLTFMaterialExpressions.h"

#include "Containers/UnrealString.h"

namespace GLTF
{
	namespace
	{
		void CreateTextureCoordinate(int32 TexCoord, FMaterialExpressionTexture& TexExpression, FMaterialElement& MaterialElement)
		{
			if (TexCoord)
			{
				FMaterialExpressionTextureCoordinate* CoordExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionTextureCoordinate>();
				CoordExpression->SetCoordinateIndex(TexCoord + 1);
				CoordExpression->ConnectExpression(TexExpression.GetInputCoordinate(), 0);
			}
		}

		FMaterialExpressionInput& GetFirstInput(FMaterialExpression* Expression)
		{
			FMaterialExpressionInput* Input = Expression->GetInput(0);
			if (Input->GetExpression() == nullptr)
				return *Input;
			else
				return GetFirstInput(Input->GetExpression());
		}

		inline void SetExpresisonValue(float Value, FMaterialExpressionScalar* Expression)
		{
			Expression->GetScalar() = Value;
		}

		inline void SetExpresisonValue(const FVector& Color, FMaterialExpressionColor* Expression)
		{
			Expression->GetColor() = FLinearColor(Color);
		}

		inline void SetExpresisonValue(const FVector4& Color, FMaterialExpressionColor* Expression)
		{
			Expression->GetColor() = FLinearColor(Color);
		}
	}

	FPBRMapFactory::FPBRMapFactory(ITextureFactory& TextureFactory)
	    : CurrentMaterialElement(nullptr)
	    , TextureFactory(TextureFactory)
	    , ParentPackage(nullptr)
	    , Flags(RF_NoFlags)
	{
	}

	void FPBRMapFactory::CreateNormalMap(const GLTF::FTexture& Map, int CoordinateIndex, float NormalScale)
	{
		check(CurrentMaterialElement);

		FMaterialExpressionTexture* TexExpression = CreateTextureMap(Map, CoordinateIndex, TEXT("Normal Map"), ETextureMode::Normal);
		if (!TexExpression)
			return;

		FMaterialExpressionScalar* ScalarExpression = CurrentMaterialElement->AddMaterialExpression<FMaterialExpressionScalar>();
		ScalarExpression->SetName(TEXT("Normal Scale"));
		ScalarExpression->SetGroupName(*GroupName);
		ScalarExpression->GetScalar() = NormalScale;

		// GLTF specifies that the following formula is used:
		// scaledNormal = normalize((<sampled normal texture value> * 2.0 - 1.0) * vec3(<normal scale>, <normal scale>, 1.0)).

		FMaterialExpressionFunctionCall* NormalExpression = CurrentMaterialElement->AddMaterialExpression<FMaterialExpressionFunctionCall>();
		NormalExpression->SetFunctionPathName(TEXT("/DatasmithContent/Materials/MDL/AdjustNormal.AdjustNormal"));
		TexExpression->ConnectExpression(*NormalExpression->GetInput(0), 0);
		ScalarExpression->ConnectExpression(*NormalExpression->GetInput(1), 0);

		NormalExpression->ConnectExpression(CurrentMaterialElement->GetNormal(), 0);
	}

	FMaterialExpression* FPBRMapFactory::CreateColorMap(const GLTF::FTexture& Map, int CoordinateIndex, const FVector& Color, const TCHAR* MapName,
	                                                    const TCHAR* ValueName, ETextureMode TextureMode, FMaterialExpressionInput& MaterialInput)
	{
		return CreateMap<FMaterialExpressionColor>(Map, CoordinateIndex, Color, MapName, ValueName, TextureMode, MaterialInput);
	}

	FMaterialExpression* FPBRMapFactory::CreateColorMap(const GLTF::FTexture& Map, int CoordinateIndex, const FVector4& Color, const TCHAR* MapName,
	                                                    const TCHAR* ValueName, ETextureMode TextureMode, FMaterialExpressionInput& MaterialInput)
	{
		return CreateMap<FMaterialExpressionColor>(Map, CoordinateIndex, Color, MapName, ValueName, TextureMode, MaterialInput);
	}

	FMaterialExpression* FPBRMapFactory::CreateScalarMap(const GLTF::FTexture& Map, int CoordinateIndex, float Value, const TCHAR* MapName,
	                                                     const TCHAR* ValueName, ETextureMode TextureMode, FMaterialExpressionInput& MaterialInput)
	{
		return CreateMap<FMaterialExpressionScalar>(Map, CoordinateIndex, Value, MapName, ValueName, TextureMode, MaterialInput);
	}

	void FPBRMapFactory::CreateMultiMap(const GLTF::FTexture& Map, int CoordinateIndex, const TCHAR* MapName, const FMapChannel* MapChannels,
	                                    uint32 MapChannelsCount, ETextureMode TextureMode)
	{
		FExpressionList ValueExpressions;
		for (uint32 Index = 0; Index < MapChannelsCount; ++Index)
		{
			const FMapChannel& MapChannel = MapChannels[Index];

			FMaterialExpressionParameter* ValueExpression = nullptr;
			switch (MapChannel.Channel)
			{
				case EChannel::RG:
				case EChannel::RGB:
				{
					ValueExpression                           = CurrentMaterialElement->AddMaterialExpression<FMaterialExpressionColor>();
					FMaterialExpressionColor* ColorExpression = static_cast<FMaterialExpressionColor*>(ValueExpression);
					SetExpresisonValue(*reinterpret_cast<const FVector*>(MapChannel.VecValue), ColorExpression);
					ColorExpression->SetGroupName(*GroupName);
					break;
				}
				default:
					ValueExpression                             = CurrentMaterialElement->AddMaterialExpression<FMaterialExpressionScalar>();
					FMaterialExpressionScalar* ScalarExpression = static_cast<FMaterialExpressionScalar*>(ValueExpression);
					SetExpresisonValue(MapChannel.Value, ScalarExpression);
					ScalarExpression->SetGroupName(*GroupName);
			}

			ValueExpression->SetName(MapChannel.ValueName);

			ValueExpressions.Add(ValueExpression);
		}

		if (!CreateMultiTexture(Map, CoordinateIndex, MapName, MapChannels, MapChannelsCount, TextureMode, ValueExpressions))
		{
			// no texture present, make value connections
			for (uint32 Index = 0; Index < MapChannelsCount; ++Index)
			{
				const FMapChannel& MapChannel = MapChannels[Index];

				if (MapChannel.OutputExpression)
				{
					ValueExpressions[Index]->ConnectExpression(GetFirstInput(MapChannel.OutputExpression), 0);
					MapChannel.OutputExpression->ConnectExpression(*MapChannel.MaterialInput, 0);
				}
				else
					ValueExpressions[Index]->ConnectExpression(*MapChannel.MaterialInput, 0);
			}
		}
	}

	bool FPBRMapFactory::CreateMultiTexture(const GLTF::FTexture& Map, int CoordinateIndex, const TCHAR* MapName, const FMapChannel* MapChannels,
	                                        uint32 MapChannelsCount, ETextureMode TextureMode, const FExpressionList& ValueExpressions)
	{
		FMaterialExpressionTexture* TexExpression = CreateTextureMap(Map, CoordinateIndex, MapName, TextureMode);
		if (!TexExpression)
			return false;

		for (uint32 Index = 0; Index < MapChannelsCount; ++Index)
		{
			const FMapChannel& MapChannel = MapChannels[Index];

			FMaterialExpressionGeneric* MultiplyExpression = CurrentMaterialElement->AddMaterialExpression<FMaterialExpressionGeneric>();
			MultiplyExpression->SetExpressionName(TEXT("Multiply"));

			switch (MapChannel.Channel)
			{
				case EChannel::RG:
				{
					FMaterialExpressionFunctionCall* MakeFloat2 = CurrentMaterialElement->AddMaterialExpression<FMaterialExpressionFunctionCall>();
					MakeFloat2->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/MakeFloat2.MakeFloat2"));

					TexExpression->ConnectExpression(*MakeFloat2->GetInput(0), (int32)EChannel::Red);
					TexExpression->ConnectExpression(*MakeFloat2->GetInput(1), (int32)EChannel::Greeen);
					MakeFloat2->ConnectExpression(*MultiplyExpression->GetInput(0), 0);
					break;
				}
				case EChannel::RGB:
				{
					FMaterialExpressionFunctionCall* MakeFloat3 = CurrentMaterialElement->AddMaterialExpression<FMaterialExpressionFunctionCall>();
					MakeFloat3->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/MakeFloat3.MakeFloat3"));

					TexExpression->ConnectExpression(*MakeFloat3->GetInput(0), (int32)EChannel::Red);
					TexExpression->ConnectExpression(*MakeFloat3->GetInput(1), (int32)EChannel::Greeen);
					TexExpression->ConnectExpression(*MakeFloat3->GetInput(2), (int32)EChannel::Blue);
					MakeFloat3->ConnectExpression(*MultiplyExpression->GetInput(0), 0);
					break;
				}
				default:
					// single channel connection
					TexExpression->ConnectExpression(*MultiplyExpression->GetInput(0), (int32)MapChannel.Channel);
					break;
			}

			ValueExpressions[Index]->ConnectExpression(*MultiplyExpression->GetInput(1), 0);
			if (MapChannel.OutputExpression)
			{
				MultiplyExpression->ConnectExpression(GetFirstInput(MapChannel.OutputExpression), 0);
				MapChannel.OutputExpression->ConnectExpression(*MapChannel.MaterialInput, 0);
			}
			else
				MultiplyExpression->ConnectExpression(*MapChannel.MaterialInput, 0);
		}
		return true;
	}

	FMaterialExpressionTexture* FPBRMapFactory::CreateTextureMap(const GLTF::FTexture& Map, int CoordinateIndex, const TCHAR* MapName,
	                                                             ETextureMode TextureMode)
	{
		ITextureElement* Texture = TextureFactory.CreateTexture(Map, ParentPackage, Flags, TextureMode);

		FMaterialExpressionTexture* TexExpression = nullptr;
		if (Texture)
		{
			TexExpression = CurrentMaterialElement->AddMaterialExpression<FMaterialExpressionTexture>();
			TexExpression->SetTexture(Texture);
			TexExpression->SetName(*(FString(MapName) + TEXT(" Map")));
			TexExpression->SetGroupName(*GroupName);

			CreateTextureCoordinate(CoordinateIndex, *TexExpression, *CurrentMaterialElement);
		}
		return TexExpression;
	}

	template <class ValueExpressionClass, class ValueClass>
	FMaterialExpression* FPBRMapFactory::CreateMap(const GLTF::FTexture& Map, int CoordinateIndex, const ValueClass& Value, const TCHAR* MapName,
	                                               const TCHAR* ValueName, ETextureMode TextureMode, FMaterialExpressionInput& MaterialInput)
	{
		check(MapName);
		check(CurrentMaterialElement);

		ValueExpressionClass* ValueExpression = CurrentMaterialElement->AddMaterialExpression<ValueExpressionClass>();
		if (ValueName)
			ValueExpression->SetName(*(FString(MapName) + TEXT(" ") + ValueName));
		else
			ValueExpression->SetName(MapName);
		ValueExpression->SetGroupName(*GroupName);
		SetExpresisonValue(Value, ValueExpression);

		FMaterialExpressionTexture* TexExpression = CreateTextureMap(Map, CoordinateIndex, MapName, TextureMode);
		if (TexExpression)
		{
			FMaterialExpressionGeneric* MultiplyExpression = CurrentMaterialElement->AddMaterialExpression<FMaterialExpressionGeneric>();
			MultiplyExpression->SetExpressionName(TEXT("Multiply"));

			TexExpression->ConnectExpression(*MultiplyExpression->GetInput(0), 0);
			ValueExpression->ConnectExpression(*MultiplyExpression->GetInput(1), 0);
			MultiplyExpression->ConnectExpression(MaterialInput, 0);
			return MultiplyExpression;
		}
		else
		{
			ValueExpression->ConnectExpression(MaterialInput, 0);
			return ValueExpression;
		}
	}

}  // namespace GLTF
