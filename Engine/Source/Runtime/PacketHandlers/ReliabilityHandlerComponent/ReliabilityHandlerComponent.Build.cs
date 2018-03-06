// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ReliabilityHandlerComponent : ModuleRules
{
    public ReliabilityHandlerComponent(ReadOnlyTargetRules Target) : base(Target)
    {
        ShortName = "ReliableHComp";
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "PacketHandler",
            }
        );
    }
}
