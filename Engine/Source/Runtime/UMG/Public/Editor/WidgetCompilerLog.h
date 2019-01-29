// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#if WITH_EDITOR

class IWidgetCompilerLog
{
public:
	TSharedRef<FTokenizedMessage> Error(const FText& Message)
	{
		TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create(EMessageSeverity::Error);
		Line->AddToken(FTextToken::Create(Message));
		InternalLogMessage(Line);
		return Line;
	}

	TSharedRef<FTokenizedMessage> Warning(const FText& Message)
	{
		TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create(EMessageSeverity::Warning);
		Line->AddToken(FTextToken::Create(Message));
		InternalLogMessage(Line);
		return Line;
	}

	TSharedRef<FTokenizedMessage> Note(const FText& Message)
	{
		TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create(EMessageSeverity::Info);
		Line->AddToken(FTextToken::Create(Message));
		InternalLogMessage(Line);
		return Line;
	}
	
protected:
	virtual void InternalLogMessage(TSharedRef<FTokenizedMessage>& Message) = 0;
};

#endif