// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MeshDescription : ModuleRules
	{
		public MeshDescription(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateIncludePaths.Add("Runtime/MeshDescription/Private");
            PublicIncludePaths.Add("Runtime/MeshDescription/Public");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				}
			);
		}
	}
}
