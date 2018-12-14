// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditableMeshCustomVersion.h"
#include "Serialization/CustomVersion.h"

const FGuid FEditableMeshCustomVersion::GUID( 0xFB26E412, 0x1F154B4D, 0x9372550A, 0x961D2F70 );

// Register the custom version with core
FCustomVersionRegistration GRegisterEditableMeshCustomVersion( FEditableMeshCustomVersion::GUID, FEditableMeshCustomVersion::LatestVersion, TEXT( "EditableMeshVer" ) );
