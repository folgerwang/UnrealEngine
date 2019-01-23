// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GLTFMaterial.h"
#include "JsonUtilities.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace GLTF
{
	// Returns scale factor if JSON has it, 1.0 by default.
	inline float SetTextureMap(const FJsonObject& InObject, const TCHAR* InTexName, const TCHAR* InScaleName, const TArray<FTexture>& Textures,
	                           GLTF::FTextureMap& OutMap)
	{
		float Scale = 1.0f;

		if (InObject.HasTypedField<EJson::Object>(InTexName))
		{
			const FJsonObject& TexObj   = *InObject.GetObjectField(InTexName);
			int32              TexIndex = GetIndex(TexObj, TEXT("index"));
			if (Textures.IsValidIndex(TexIndex))
			{
				OutMap.TextureIndex = TexIndex;
				uint32 TexCoord     = GetUnsignedInt(TexObj, TEXT("texCoord"), 0);
				check(TexCoord < 2);
				OutMap.TexCoord = TexCoord;
				if (InScaleName && TexObj.HasTypedField<EJson::Number>(InScaleName))
				{
					Scale = TexObj.GetNumberField(InScaleName);
				}
			}
		}
		else
		{
			Scale = 0.f;
		}

		return Scale;
	}

}  // namespace GLTF
