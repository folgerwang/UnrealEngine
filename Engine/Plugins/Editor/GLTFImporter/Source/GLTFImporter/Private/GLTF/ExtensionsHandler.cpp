// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ExtensionsHandler.h"

#include "GLTFAsset.h"
#include "JsonUtilities.h"
#include "MaterialUtilities.h"

namespace GLTF
{
	namespace
	{
		static const TArray<FString> LightExtensions = {TEXT("KHR_lights_punctual"), TEXT("KHR_lights")};
		static const TArray<FString> BaseExtensions  = LightExtensions;

		TSharedPtr<FJsonObject> GetLightExtension(const TSharedPtr<FJsonObject>& Object)
		{
			if (!Object)
				return nullptr;

			TSharedPtr<FJsonObject> LightsObj;
			if (Object->HasTypedField<EJson::Object>(LightExtensions[0]))
			{
				LightsObj = Object->GetObjectField(LightExtensions[0]);
			}
			else if (Object->HasTypedField<EJson::Object>(LightExtensions[1]))
			{
				LightsObj = Object->GetObjectField(LightExtensions[1]);
			}
			return LightsObj;
		}

		TSharedPtr<FJsonObject> GetExtensions(const FJsonObject& Object)
		{
			if (!Object.HasTypedField<EJson::Object>(TEXT("extensions")))
			{
				return nullptr;
			}

			return Object.GetObjectField(TEXT("extensions"));
		}
	}

	FExtensionsHandler::FExtensionsHandler(TArray<FLogMessage>& InMessages)
	    : Messages(InMessages)
	    , Asset(nullptr)
	{
	}

	void FExtensionsHandler::SetupAssetExtensions(const FJsonObject& Object) const
	{
		const TSharedPtr<FJsonObject>& ExtensionsObj = GetExtensions(Object);

		// lights

		if (TSharedPtr<FJsonObject> LightsObj = GetLightExtension(ExtensionsObj))
		{
			Asset->ExtensionsUsed.Add(EExtension::KHR_LightsPunctual);

			uint32 LightCount = ArraySize(*LightsObj, TEXT("lights"));
			if (LightCount > 0)
			{
				Asset->Lights.Reserve(LightCount);
				for (const TSharedPtr<FJsonValue>& Value : LightsObj->GetArrayField(TEXT("lights")))
				{
					SetupLightPunctual(*Value->AsObject());
				}
			}
		}

		CheckExtensions(Object, BaseExtensions);
	}

	void FExtensionsHandler::SetupMaterialExtensions(const FJsonObject& Object, FMaterial& Material) const
	{
		if (!Object.HasTypedField<EJson::Object>(TEXT("extensions")))
		{
			return;
		}

		enum
		{
			KHR_materials_pbrSpecularGlossiness = 0,
			KHR_materials_unlit,
			MSFT_packing_occlusionRoughnessMetallic,
			MSFT_packing_normalRoughnessMetallic,
		};
		static const TArray<FString> Extensions = {TEXT("KHR_materials_pbrSpecularGlossiness"), TEXT("KHR_materials_unlit"),
		                                           TEXT("MSFT_packing_occlusionRoughnessMetallic"), TEXT("MSFT_packing_normalRoughnessMetallic")};

		const FJsonObject& ExtensionsObj = *Object.GetObjectField(TEXT("extensions"));
		for (int32 Index = 0; Index < Extensions.Num(); ++Index)
		{
			const FString ExtensionName = Extensions[Index];
			if (!ExtensionsObj.HasTypedField<EJson::Object>(ExtensionName))
				continue;

			const FJsonObject& ExtObj = *ExtensionsObj.GetObjectField(ExtensionName);
			switch (Index)
			{
				case KHR_materials_pbrSpecularGlossiness:
				{
					const FJsonObject& PBR = ExtObj;
					GLTF::SetTextureMap(PBR, TEXT("diffuseTexture"), nullptr, Asset->Textures, Material.BaseColor);
					Material.BaseColorFactor = GetVec4(PBR, TEXT("diffuseFactor"), FVector4(1.0f, 1.0f, 1.0f, 1.0f));

					GLTF::SetTextureMap(PBR, TEXT("specularGlossinessTexture"), nullptr, Asset->Textures, Material.SpecularGlossiness.Map);
					Material.SpecularGlossiness.SpecularFactor   = GetVec3(PBR, TEXT("specularFactor"), FVector(1.0f));
					Material.SpecularGlossiness.GlossinessFactor = GetScalar(PBR, TEXT("glossinessFactor"), 1.0f);

					Material.ShadingModel = FMaterial::EShadingModel::SpecularGlossiness;

					Asset->ExtensionsUsed.Add(EExtension::KHR_MaterialsPbrSpecularGlossiness);
				}
				break;
				case KHR_materials_unlit:
				{
					Material.bIsUnlitShadingModel = true;
					Asset->ExtensionsUsed.Add(EExtension::KHR_MaterialsUnlit);
				}
				break;
				case MSFT_packing_occlusionRoughnessMetallic:
				{
					const FJsonObject& Packing = ExtObj;
					if (GLTF::SetTextureMap(Packing, TEXT("occlusionRoughnessMetallicTexture"), nullptr, Asset->Textures, Material.Packing.Map))
					{
						Material.Packing.Flags = (int)GLTF::FMaterial::EPackingFlags::OcclusionRoughnessMetallic;
					}
					else if (GLTF::SetTextureMap(Packing, TEXT("roughnessMetallicOcclusionTexture"), nullptr, Asset->Textures, Material.Packing.Map))
					{
						Material.Packing.Flags = (int)GLTF::FMaterial::EPackingFlags::RoughnessMetallicOcclusion;
					}
					if (GLTF::SetTextureMap(Packing, TEXT("normalTexture"), nullptr, Asset->Textures, Material.Packing.NormalMap))
					{
						// can have extra packed two channel(RG) normal map
						Material.Packing.Flags |= (int)GLTF::FMaterial::EPackingFlags::NormalRG;
					}

					if (Material.Packing.Flags != (int)GLTF::FMaterial::EPackingFlags::None)
						Asset->ExtensionsUsed.Add(EExtension::MSFT_PackingOcclusionRoughnessMetallic);
				}
				break;
				case MSFT_packing_normalRoughnessMetallic:
				{
					const FJsonObject& Packing = ExtObj;
					if (GLTF::SetTextureMap(Packing, TEXT("normalRoughnessMetallicTexture"), nullptr, Asset->Textures, Material.Packing.Map))
					{
						Material.Packing.NormalMap = Material.Packing.Map;
						Material.Packing.Flags     = (int)GLTF::FMaterial::EPackingFlags::NormalRoughnessMetallic;
						Asset->ExtensionsUsed.Add(EExtension::MSFT_PackingNormalRoughnessMetallic);
					}
				}
				break;
				default:
					check(false);
					break;
			}
		}

		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupBufferExtensions(const FJsonObject& Object, FBuffer& Buffer) const
	{
		static const TArray<FString> Extensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupBufferViewExtensions(const FJsonObject& Object, FBufferView& BufferView) const
	{
		static const TArray<FString> Extensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupAccessorExtensions(const FJsonObject& Object, FAccessor& Accessor) const
	{
		static const TArray<FString> Extensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupPrimitiveExtensions(const FJsonObject& Object, FPrimitive& Primitive) const
	{
		static const TArray<FString> Extensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupMeshExtensions(const FJsonObject& Object, FMesh& Mesh) const
	{
		static const TArray<FString> Extensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupSceneExtensions(const FJsonObject& Object, FScene& Scene) const
	{
		static const TArray<FString> Extensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupNodeExtensions(const FJsonObject& Object, FNode& Node) const
	{
		const TSharedPtr<FJsonObject>& ExtensionsObj = GetExtensions(Object);
		if (TSharedPtr<FJsonObject> LightsObj = GetLightExtension(ExtensionsObj))
		{
			Node.LightIndex = GetIndex(*LightsObj, TEXT("light"));
		}

		static const TArray<FString> Extensions = LightExtensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupCameraExtensions(const FJsonObject& Object, FCamera& Camera) const
	{
		static const TArray<FString> Extensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupSkinExtensions(const FJsonObject& Object, FSkinInfo& Skin) const
	{
		static const TArray<FString> Extensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupAnimationExtensions(const FJsonObject& Object, FAnimation& Animation) const
	{
		static const TArray<FString> Extensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupImageExtensions(const FJsonObject& Object, FImage& Image) const
	{
		static const TArray<FString> Extensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupSamplerExtensions(const FJsonObject& Object, FSampler& Sampler) const
	{
		static const TArray<FString> Extensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::SetupTextureExtensions(const FJsonObject& Object, FTexture& Texture) const
	{
		static const TArray<FString> Extensions;
		CheckExtensions(Object, Extensions);
	}

	void FExtensionsHandler::CheckExtensions(const FJsonObject& Object, const TArray<FString>& ExtensionsSupported) const
	{
		if (!Object.HasTypedField<EJson::Object>(TEXT("extensions")))
		{
			return;
		}

		const FJsonObject& ExtensionsObj = *Object.GetObjectField(TEXT("extensions"));
		for (const auto& StrValuePair : ExtensionsObj.Values)
		{
			if (ExtensionsSupported.Find(StrValuePair.Get<0>()) == INDEX_NONE)
			{
				Messages.Emplace(EMessageSeverity::Warning, FString::Printf(TEXT("Extension is not supported: %s"), *StrValuePair.Get<0>()));
			}
		}
	}

	void FExtensionsHandler::SetupLightPunctual(const FJsonObject& Object) const
	{
		const uint32 LightIndex = Asset->Lights.Num();
		const FNode* Found      = Asset->Nodes.FindByPredicate([LightIndex](const FNode& Node) { return LightIndex == Node.LightIndex; });

		FLight& Light = Asset->Lights.Emplace_GetRef(Found);

		Light.Name      = GetString(Object, TEXT("name"));
		Light.Color     = GetVec3(Object, TEXT("color"), FVector(1.f));
		Light.Intensity = GetScalar(Object, TEXT("intensity"), 1.f);
		Light.Range     = GetScalar(Object, TEXT("range"), Light.Range);

		const FString Type = GetString(Object, TEXT("type"));
		if (Type == TEXT("spot"))
		{
			Light.Type = FLight::EType::Spot;
			if (Object.HasTypedField<EJson::Object>(TEXT("spot")))
			{
				const TSharedPtr<FJsonObject>& SpotObj = Object.GetObjectField(TEXT("spot"));
				Light.Spot.InnerConeAngle              = GetScalar(*SpotObj, TEXT("innerConeAngle"), 0.f);
				Light.Spot.OuterConeAngle              = GetScalar(*SpotObj, TEXT("outerConeAngle"), Light.Spot.OuterConeAngle);
			}
		}
		else if (Type == TEXT("point"))
			Light.Type = FLight::EType::Point;
		else if (Type == TEXT("directional"))
			Light.Type = FLight::EType::Directional;
		else
			Messages.Emplace(EMessageSeverity::Warning, FString::Printf(TEXT("Light has no type specified: %s"), *Light.Name));
	}

}  // namespace GLTF
