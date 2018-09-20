// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"

THIRD_PARTY_INCLUDES_START
#include "mkvparser/mkvparser.h"
THIRD_PARTY_INCLUDES_END

class FMkvFileReader : public mkvparser::IMkvReader
{
public:
	bool Open(const TCHAR* Filename);

public:
	// IMkvReader interface
	virtual int Read(long long Position, long Lenght, unsigned char* Buffer) override;
	virtual int Length(long long* Total, long long* Available) override;

private:
	TUniquePtr<IFileHandle> m_file;
};
