// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Rendering/ShaderResourceManager.h"

DECLARE_CYCLE_STAT(TEXT("GetResourceHandle Time"), STAT_SlateGetResourceHandle, STATGROUP_Slate);

FSlateResourceHandle FSlateShaderResourceManager::GetResourceHandle( const FSlateBrush& InBrush )
{
	SCOPE_CYCLE_COUNTER(STAT_SlateGetResourceHandle);

	FSlateShaderResourceProxy* Proxy = GetShaderResource( InBrush );

	FSlateResourceHandle NewHandle;
	if( Proxy )
	{
		if( !Proxy->HandleData.IsValid() )
		{
			Proxy->HandleData = MakeShareable( new FSlateSharedHandleData( Proxy ) );
		}

		NewHandle.Data = Proxy->HandleData;
	}

	return NewHandle;
}
