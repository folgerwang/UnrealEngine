// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XmlParser : ModuleRules
{
	public XmlParser( ReadOnlyTargetRules Target ) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core",
				"CoreUObject",
			}
			);
	}
}
