// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "GLTFAsset.h"

#include "Misc/Paths.h"

namespace GLTF
{
	namespace
	{
		template <typename T>
		void GenerateNames(const FString& Prefix, TArray<T>& Objects)
		{
			int32 Counter = 0;
			for (T& Obj : Objects)
			{
				if (Obj.Name.IsEmpty())
				{
					Obj.Name = Prefix + FString::FromInt(Counter++);
				}
			}
		}
	}

	const FMetadata::FExtraData* FMetadata::GetExtraData(const FString& Name) const
	{
		return Extras.FindByPredicate([&Name](const FMetadata::FExtraData& Data) { return Data.Name == Name; });
	}

	void FAsset::Clear(uint32 BinBufferKBytes, uint32 ExtraBinBufferKBytes)
	{
		Buffers.Empty();
		BufferViews.Empty();
		Accessors.Empty();
		Meshes.Empty();

		Scenes.Empty();
		Nodes.Empty();
		Cameras.Empty();
		Lights.Empty();
		Skins.Empty();

		Images.Empty();
		Samplers.Empty();
		Textures.Empty();
		Materials.Empty();

		ExtensionsUsed.Empty((int)EExtension::Count);
		Metadata.GeneratorName.Empty();
		Metadata.Extras.Empty();

		if (BinData.Num() > 0)
			BinData.Empty(BinBufferKBytes * 1024);
		if (ExtraBinData.Num() > 0)
			ExtraBinData.Empty(ExtraBinBufferKBytes * 1024);
	}

	void FAsset::GenerateNames(const FString& Prefix)
	{
		check(!Prefix.IsEmpty());

		GLTF::GenerateNames(Prefix + TEXT("_material_"), Materials);
		GLTF::GenerateNames(Prefix + TEXT("_mesh_"), Meshes);
		GLTF::GenerateNames(Prefix + TEXT("_skin_"), Skins);
		GLTF::GenerateNames(Prefix + TEXT("_animation_"), Animations);

		{
			const FString& NodePrefix = Prefix + TEXT("_node_");
			const FString& JoinPrefix = Prefix + TEXT("_join_");

			int32 Counter[2] = {0, 0};
			for (FNode& Node : Nodes)
			{
				if (!Node.Name.IsEmpty())
					continue;

				bool bIsJoint = Node.Type == FNode::EType::Joint;
				Node.Name     = (bIsJoint ? JoinPrefix : NodePrefix) + FString::FromInt(Counter[bIsJoint]++);
			}
		}

		{
			const FString& TexPrefix = Prefix + TEXT("_texture_");
			int32          Counter   = 0;
			for (FTexture& Tex : Textures)
			{
				if (!Tex.Name.IsEmpty())
					continue;

				if (!Tex.Source.Name.IsEmpty())
				{
					Tex.Name = Tex.Source.Name;
				}
				else if (!Tex.Source.URI.IsEmpty())
				{
					Tex.Name = FPaths::GetBaseFilename(Tex.Source.URI);
				}
				else
				{
					Tex.Name = TexPrefix + FString::FromInt(Counter++);
				}
			}
		}

		for (FCamera& Camera : Cameras)
		{
			if (Camera.Name.IsEmpty())
				Camera.Name = TEXT("camera_") + Camera.Node.Name;  // cant be empty
		}

		int32 Counter = 0;
		for (FLight& Light : Lights)
		{
			if (Light.Name.IsEmpty())
			{
				// cant be empty
				if (Light.Node)
					Light.Name = TEXT("light_") + Light.Node->Name;
				else
					Light.Name = TEXT("light_") + FString::FromInt(Counter++);
			}
		}

		GLTF::GenerateNames(Prefix + TEXT("_image_"), Images);
	}

	void FAsset::GetRootNodes(TArray<int32>& NodeIndices)
	{
		TMap<int32, uint32> VisitCount;
		VisitCount.Reserve(Nodes.Num());
		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
		{
			for (int32 ChildIndex : Nodes[NodeIndex].Children)
			{
				uint32* Found = VisitCount.Find(ChildIndex);
				if (Found)
				{
					(*Found)++;
				}
				else
				{
					VisitCount.Add(ChildIndex) = 1;
				}
			}
		}

		NodeIndices.Empty();
		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
		{
			if (!VisitCount.Contains(NodeIndex))
				NodeIndices.Add(NodeIndex);
			else
				check(VisitCount[NodeIndex] == 1);
		}
	}

	FAsset::EValidationCheck FAsset::ValidationCheck() const
	{
		int32 Res = EValidationCheck::Valid;
		if (Meshes.FindByPredicate([](const FMesh& Mesh) { return !Mesh.IsValid(); }))
			Res |= InvalidMeshPresent;

		if (Nodes.FindByPredicate([](const FNode& Node) { return !Node.Transform.IsValid(); }))
			Res |= InvalidNodeTransform;

		// TODO: lots more validation

		// TODO: verify if image format is PNG or JPEG

		return static_cast<EValidationCheck>(Res);
	}

	const TCHAR* ToString(EExtension Extension)
	{
		switch (Extension)
		{
			case GLTF::EExtension::KHR_MaterialsPbrSpecularGlossiness:
				return TEXT("KHR_Materials_PbrSpecularGlossiness");
			case GLTF::EExtension::KHR_MaterialsUnlit:
				return TEXT("KHR_Materials_Unlit");
			case GLTF::EExtension::KHR_TextureTransform:
				return TEXT("KHR_Texture_Transform");
			case GLTF::EExtension::KHR_DracoMeshCompression:
				return TEXT("KHR_DracoMeshCompression");
			case GLTF::EExtension::KHR_LightsPunctual:
				return TEXT("KHR_LightsPunctual");
			case GLTF::EExtension::KHR_Blend:
				return TEXT("KHR_Blend");
			case GLTF::EExtension::MSFT_TextureDDS:
				return TEXT("MSFT_Texture_DDS");
			case GLTF::EExtension::MSFT_PackingNormalRoughnessMetallic:
				return TEXT("MSFT_Packing_NormalRoughnessMetallic");
			case GLTF::EExtension::MSFT_PackingOcclusionRoughnessMetallic:
				return TEXT("MSFT_Packing_OcclusionRoughnessMetallic");
			case GLTF::EExtension::Count:
				check(false);
			default:
				return TEXT("UnknwonExtension");
		}
	}

}  // namespace GLTF
