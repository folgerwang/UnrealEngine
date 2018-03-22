// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IRemoteSessionChannel;

class REMOTESESSION_API IRemoteSessionRole
{
public:
	virtual~IRemoteSessionRole() {}

	virtual TSharedPtr<IRemoteSessionChannel> GetChannel(const FString& Type) = 0;

	template<class T>
	TSharedPtr<T> GetChannel(const FString& InType)
	{
		TSharedPtr<IRemoteSessionChannel> Channel = GetChannel(InType);

		if (Channel.IsValid())
		{
			return StaticCastSharedPtr<T>(Channel);
		}

		return TSharedPtr<T>();
	}

};


