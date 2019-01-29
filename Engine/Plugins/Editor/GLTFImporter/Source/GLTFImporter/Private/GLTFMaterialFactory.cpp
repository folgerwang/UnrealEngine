// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GLTFMaterialFactory.h"

#include "GLTFMapFactory.h"
#include "GLTFTextureFactory.h"

#include "GLTFAsset.h"

#include "Engine/EngineTypes.h"

namespace GLTF
{
	namespace
	{
		const GLTF::FTexture& GetTexture(const GLTF::FTextureMap& Map, const TArray<GLTF::FTexture>& Textures)
		{
			static const GLTF::FImage   Immage;
			static const GLTF::FTexture None(FString(), Immage, GLTF::FSampler::DefaultSampler);
			return Map.TextureIndex != INDEX_NONE ? Textures[Map.TextureIndex] : None;
		}
	}

	class FMaterialFactoryImpl : public GLTF::FBaseLogger
	{
	public:
		FMaterialFactoryImpl(IMaterialElementFactory* MaterialElementFactory, ITextureFactory* TextureFactory)
		    : MaterialElementFactory(MaterialElementFactory)
		    , TextureFactory(TextureFactory)
		{
			check(MaterialElementFactory);
		}

		const TArray<FMaterialElement*>& CreateMaterials(const GLTF::FAsset& Asset, UObject* ParentPackage, EObjectFlags Flags);

	private:
		void HandleOcclusion(const TArray<GLTF::FTexture>& Texture, const GLTF::FMaterial& GLTFMaterial, FPBRMapFactory& MapFactory,
		                     FMaterialElement& MaterialElement);

		void HandleGGX(const TArray<GLTF::FTexture>& Texture, const GLTF::FMaterial& GLTFMaterial, FPBRMapFactory& MapFactory,
		               FMaterialElement& MaterialElement);

		void HandleOpacity(const TArray<GLTF::FTexture>& Texture, const GLTF::FMaterial& GLTFMaterial, FMaterialElement& MaterialElement);

	private:
		TUniquePtr<IMaterialElementFactory> MaterialElementFactory;
		TUniquePtr<ITextureFactory>         TextureFactory;
		TArray<FMaterialElement*>           Materials;

		friend class FMaterialFactory;
	};

	namespace
	{
		EBlendMode ConvertAlphaMode(FMaterial::EAlphaMode Mode)
		{
			switch (Mode)
			{
				case FMaterial::EAlphaMode::Opaque:
					return EBlendMode::BLEND_Opaque;
				case FMaterial::EAlphaMode::Blend:
					return EBlendMode::BLEND_Translucent;
				case FMaterial::EAlphaMode::Mask:
					return EBlendMode::BLEND_Masked;
				default:
					return EBlendMode::BLEND_Opaque;
			}
		}

		template <class ReturnClass>
		ReturnClass* FindExpression(const FString& Name, FMaterialElement& MaterialElement)
		{
			ReturnClass* Result = nullptr;
			for (int32 Index = 0; Index < MaterialElement.GetExpressionsCount(); ++Index)
			{
				FMaterialExpression* Expression = MaterialElement.GetExpression(Index);
				if (Expression->GetType() != EMaterialExpressionType::ConstantColor &&
				    Expression->GetType() != EMaterialExpressionType::ConstantScalar && Expression->GetType() != EMaterialExpressionType::Texture)
					continue;

				FMaterialExpressionParameter* ExpressionParameter = static_cast<FMaterialExpressionParameter*>(Expression);
				if (ExpressionParameter->GetName() == Name)
				{
					Result = static_cast<ReturnClass*>(ExpressionParameter);
					check(Expression->GetType() == (EMaterialExpressionType)ReturnClass::Type);
					break;
				}
			}
			return Result;
		}

	}

	const TArray<FMaterialElement*>& FMaterialFactoryImpl::CreateMaterials(const GLTF::FAsset& Asset, UObject* ParentPackage, EObjectFlags Flags)
	{
		TextureFactory->CleanUp();
		Materials.Empty();
		Materials.Reserve(Asset.Materials.Num() + 1);

		Messages.Empty();

		FPBRMapFactory MapFactory(*TextureFactory);
		MapFactory.SetParentPackage(ParentPackage, Flags);

		for (const GLTF::FMaterial& GLTFMaterial : Asset.Materials)
		{
			check(!GLTFMaterial.Name.IsEmpty());

			FMaterialElement* MaterialElement = MaterialElementFactory->CreateMaterial(*GLTFMaterial.Name, ParentPackage, Flags);
			MaterialElement->SetTwoSided(GLTFMaterial.bIsDoubleSided);
			MaterialElement->SetBlendMode(ConvertAlphaMode(GLTFMaterial.AlphaMode));

			MapFactory.CurrentMaterialElement = MaterialElement;

			MapFactory.GroupName = TEXT("Base Color");
			MapFactory.CreateColorMap(GetTexture(GLTFMaterial.BaseColor, Asset.Textures), GLTFMaterial.BaseColor.TexCoord,
			                          GLTFMaterial.BaseColorFactor, TEXT("BaseColor"), nullptr, ETextureMode::Color, MaterialElement->GetBaseColor());

			MapFactory.GroupName = TEXT("Normal");
			MapFactory.CreateNormalMap(GetTexture(GLTFMaterial.Normal, Asset.Textures), GLTFMaterial.Normal.TexCoord, GLTFMaterial.NormalScale);
			if (GLTFMaterial.Emissive.TextureIndex != INDEX_NONE && !GLTFMaterial.EmissiveFactor.IsNearlyZero())
			{
				MapFactory.GroupName = TEXT("Emission");
				MapFactory.CreateColorMap(GetTexture(GLTFMaterial.Emissive, Asset.Textures), GLTFMaterial.Emissive.TexCoord,
				                          GLTFMaterial.EmissiveFactor, TEXT("Emissive"), TEXT("Color"), ETextureMode::Color,
				                          MaterialElement->GetEmissiveColor());  // emissive map is in sRGB space
			}

			HandleOcclusion(Asset.Textures, GLTFMaterial, MapFactory, *MaterialElement);
			HandleGGX(Asset.Textures, GLTFMaterial, MapFactory, *MaterialElement);
			HandleOpacity(Asset.Textures, GLTFMaterial, *MaterialElement);

			MaterialElement->Finalize();
			Materials.Add(MaterialElement);
		}

		return Materials;
	}

	void FMaterialFactoryImpl::HandleOcclusion(const TArray<GLTF::FTexture>& Textures, const GLTF::FMaterial& GLTFMaterial,
	                                           FPBRMapFactory& MapFactory, FMaterialElement& MaterialElement)
	{
		MapFactory.GroupName = TEXT("Occlusion");

		FMaterialExpressionTexture* TexExpression = MapFactory.CreateTextureMap(
		    GetTexture(GLTFMaterial.Occlusion, Textures), GLTFMaterial.Occlusion.TexCoord, TEXT("Occlusion"), ETextureMode::Grayscale);

		if (!TexExpression)
			return;

		FMaterialExpressionScalar* ConstantExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionScalar>();
		ConstantExpression->GetScalar()               = 1.f;

		FMaterialExpressionGeneric* LerpExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionGeneric>();
		LerpExpression->SetExpressionName(TEXT("LinearInterpolate"));

		FMaterialExpressionScalar* ValueExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionScalar>();
		ValueExpression->SetName(TEXT("Occlusion Strength"));
		ValueExpression->SetGroupName(*MapFactory.GroupName);
		ValueExpression->GetScalar() = GLTFMaterial.OcclusionStrength;

		ConstantExpression->ConnectExpression(*LerpExpression->GetInput(0), 0);
		TexExpression->ConnectExpression(*LerpExpression->GetInput(1), (int)FPBRMapFactory::EChannel::Red);  // ignore other channels
		ValueExpression->ConnectExpression(*LerpExpression->GetInput(2), 0);

		LerpExpression->ConnectExpression(MaterialElement.GetAmbientOcclusion(), 0);
	}

	void FMaterialFactoryImpl::HandleGGX(const TArray<GLTF::FTexture>& Textures, const GLTF::FMaterial& GLTFMaterial, FPBRMapFactory& MapFactory,
	                                     FMaterialElement& MaterialElement)
	{
		MapFactory.GroupName = TEXT("GGX");

		// will need to correct the roughness which is GGX
		FMaterialExpressionGeneric* SqrtExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionGeneric>();
		SqrtExpression->SetExpressionName(TEXT("SquareRoot"));

		TArray<FPBRMapFactory::FMapChannel, TFixedAllocator<4> > Maps;
		switch (GLTFMaterial.ShadingModel)
		{
			case FMaterial::EShadingModel::MetallicRoughness:
			{
				// according to the GLTF specs:
				// cdiff = lerp(baseColor.rgb * (1 - dielectricSpecular.r), black, metallic)
				// F0 = lerp(dieletricSpecular, baseColor.rgb, metallic)
				// alpha = roughness ^ 2

				Maps.Emplace(GLTFMaterial.MetallicRoughness.MetallicFactor, TEXT("Metallic Factor"),
				                                      FPBRMapFactory::EChannel::Blue, &MaterialElement.GetMetallic(), nullptr);
				Maps.Emplace(GLTFMaterial.MetallicRoughness.RoughnessFactor, TEXT("Roughness Factor"),
				                                      FPBRMapFactory::EChannel::Greeen, &MaterialElement.GetRoughness(), SqrtExpression);

				MapFactory.CreateMultiMap(GetTexture(GLTFMaterial.MetallicRoughness.Map, Textures), GLTFMaterial.MetallicRoughness.Map.TexCoord,
				                          TEXT("MetallicRoughness Map"), Maps.GetData(), Maps.Num(), ETextureMode::Grayscale);

				// GLTF specifies that the dielectricSpecular is, 0.04 while for UE4 it's 0.08 * Specular, so correct it
				FMaterialExpressionScalar* ValueExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionScalar>();
				ValueExpression->GetScalar()               = 0.5f;
				ValueExpression->ConnectExpression(MaterialElement.GetSpecular(), 0);

				break;
			}
			case FMaterial::EShadingModel::SpecularGlossiness:
			{
				// according to the GLTF specs:
				// cdiff = diffuse.rgb * (1 - max(specular.r, specular.g, specular.b))
				// F0 = specular
				// alpha = (1 - glossiness) ^ 2

				// convert glossiness to roughness
				{
					FMaterialExpressionGeneric* NegExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionGeneric>();
					NegExpression->SetExpressionName(TEXT("OneMinus"));
					NegExpression->ConnectExpression(*SqrtExpression->GetInput(0), 0);
				}

				// create the multi map expressions
				Maps.Emplace(0.f, TEXT("Specular Factor"), FPBRMapFactory::EChannel::RGB, &MaterialElement.GetSpecular(),
				                                      nullptr);
				Maps[0].SetValue(GLTFMaterial.SpecularGlossiness.SpecularFactor);
				Maps.Emplace(GLTFMaterial.SpecularGlossiness.GlossinessFactor, TEXT("Glossiness Factor"),
				                                      FPBRMapFactory::EChannel::Alpha, &MaterialElement.GetRoughness(), SqrtExpression);

				MapFactory.CreateMultiMap(GetTexture(GLTFMaterial.SpecularGlossiness.Map, Textures), GLTFMaterial.SpecularGlossiness.Map.TexCoord,
				                          TEXT("SpecularGlossiness Map"), Maps.GetData(), Maps.Num(),
				                          ETextureMode::Color);  // specular map is in sRGB space

				// adjust diffuse with specular
				{
					FMaterialExpressionColor* BaseColorFactor = FindExpression<FMaterialExpressionColor>(TEXT("BaseColor"), MaterialElement);
					check(BaseColorFactor);
					const float SpecValue = 1.0 - GLTFMaterial.SpecularGlossiness.SpecularFactor.GetMax();
					BaseColorFactor->GetColor().R *= SpecValue;
					BaseColorFactor->GetColor().G *= SpecValue;
					BaseColorFactor->GetColor().B *= SpecValue;
				}

				FMaterialExpression* BaseColor = MaterialElement.GetBaseColor().GetExpression();
				// convert specular to diffuse term
				{
					FMaterialExpression*        Specular      = MaterialElement.GetSpecular().GetExpression();
					FMaterialExpressionGeneric* AddExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionGeneric>();
					AddExpression->SetExpressionName(TEXT("Add"));

					BaseColor->ConnectExpression(*AddExpression->GetInput(0), 0);
					Specular->ConnectExpression(*AddExpression->GetInput(1), 0);
					AddExpression->ConnectExpression(MaterialElement.GetBaseColor(), 0);
				}
				// convert diffuse to metallic, i.e. when diffuse zero material is metallic
				{
					FMaterialExpressionGeneric* NegExpression =
					    MaterialElement.AddMaterialExpression<FMaterialExpressionGeneric>();  // invert glossiness
					NegExpression->SetExpressionName(TEXT("OneMinus"));
					BaseColor->ConnectExpression(*NegExpression->GetInput(0), 0);
					NegExpression->ConnectExpression(MaterialElement.GetMetallic(), 0);
				}

				break;
			}
			default:
				check(false);
				break;
		}
	}

	void FMaterialFactoryImpl::HandleOpacity(const TArray<GLTF::FTexture>& Textures, const GLTF::FMaterial& GLTFMaterial,
	                                         FMaterialElement& MaterialElement)
	{
		if (GLTFMaterial.IsOpaque())
			return;

		const TCHAR* GroupName = TEXT("Opacity");

		FMaterialExpressionTexture* BaseColorMap = FindExpression<FMaterialExpressionTexture>(TEXT("BaseColor Map"), MaterialElement);
		switch (GLTFMaterial.AlphaMode)
		{
			case FMaterial::EAlphaMode::Mask:
			{
				FMaterialExpressionColor* BaseColorFactor = FindExpression<FMaterialExpressionColor>(TEXT("BaseColor"), MaterialElement);

				FMaterialExpressionGeneric* MultiplyExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionGeneric>();
				MultiplyExpression->SetExpressionName(TEXT("Multiply"));
				BaseColorFactor->ConnectExpression(*MultiplyExpression->GetInput(1), (int)FPBRMapFactory::EChannel::Alpha);
				BaseColorMap->ConnectExpression(*MultiplyExpression->GetInput(0), (int)FPBRMapFactory::EChannel::Alpha);

				FMaterialExpressionFunctionCall* CuttofExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionFunctionCall>();
				CuttofExpression->SetFunctionPathName(TEXT("/Engine/Functions/Engine_MaterialFunctions02/SmoothStep.SmoothStep"));

				FMaterialExpressionScalar* ValueExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionScalar>();
				ValueExpression->SetName(TEXT("Alpha Cuttof"));
				ValueExpression->SetGroupName(GroupName);
				ValueExpression->GetScalar() = GLTFMaterial.AlphaCutoff;

				MultiplyExpression->ConnectExpression(*CuttofExpression->GetInput(0), 0);
				ValueExpression->ConnectExpression(*CuttofExpression->GetInput(1), 0);
				ValueExpression->ConnectExpression(*CuttofExpression->GetInput(2), 0);

				CuttofExpression->ConnectExpression(MaterialElement.GetOpacity(), 0);
				break;
			}
			case FMaterial::EAlphaMode::Blend:
			{
				FMaterialExpressionScalar* ValueExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionScalar>();
				ValueExpression->SetName(TEXT("IOR"));
				ValueExpression->SetGroupName(GroupName);
				ValueExpression->GetScalar() = 1.f;
				ValueExpression->ConnectExpression(MaterialElement.GetRefraction(), 0);

				FMaterialExpressionColor* BaseColorFactor = FindExpression<FMaterialExpressionColor>(TEXT("BaseColor"), MaterialElement);
				if (BaseColorMap)
				{
					FMaterialExpressionGeneric* MultiplyExpression = MaterialElement.AddMaterialExpression<FMaterialExpressionGeneric>();
					MultiplyExpression->SetExpressionName(TEXT("Multiply"));
					BaseColorFactor->ConnectExpression(*MultiplyExpression->GetInput(1), (int)FPBRMapFactory::EChannel::Alpha);
					BaseColorMap->ConnectExpression(*MultiplyExpression->GetInput(0), (int)FPBRMapFactory::EChannel::Alpha);
					MultiplyExpression->ConnectExpression(MaterialElement.GetOpacity(), 0);
				}
				else
					BaseColorFactor->ConnectExpression(MaterialElement.GetOpacity(), (int)FPBRMapFactory::EChannel::Alpha);

				break;
			}
			default:
				check(false);
				break;
		}
	}

	//

	FMaterialFactory::FMaterialFactory(IMaterialElementFactory* MaterialElementFactory, ITextureFactory* TextureFactory)
	    : Impl(new FMaterialFactoryImpl(MaterialElementFactory, TextureFactory))
	{
	}

	FMaterialFactory::~FMaterialFactory() {}

	const TArray<FMaterialElement*>& FMaterialFactory::CreateMaterials(const GLTF::FAsset& Asset, UObject* ParentPackage, EObjectFlags Flags)
	{
		return Impl->CreateMaterials(Asset, ParentPackage, Flags);
	}

	const TArray<FLogMessage>& FMaterialFactory::GetLogMessages() const
	{
		return Impl->GetLogMessages();
	}

	const TArray<FMaterialElement*>& FMaterialFactory::GetMaterials() const
	{
		return Impl->Materials;
	}

	IMaterialElementFactory& FMaterialFactory::GetMaterialElementFactory()
	{
		return *Impl->MaterialElementFactory;
	}

	ITextureFactory& FMaterialFactory::GetTextureFactory()
	{
		return *Impl->TextureFactory;
	}

	void FMaterialFactory::CleanUp()
	{
		for (FMaterialElement* MaterialElement : Impl->Materials)
		{
			delete MaterialElement;
		}
		Impl->Materials.Empty();
	}

}  // namespace GLTF
