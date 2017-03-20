// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MeshEditingRuntime : ModuleRules
	{
        public MeshEditingRuntime(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
                new string[] {
				    "Core",
				    "CoreUObject",
				    "Engine",
					"RenderCore",	// @todo mesheditor: For FlushRenderingCommands()
				}
			);

			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[] { "MikkTSpace", "OpenSubdiv" });
		}
	}
}