// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class NavigationSystem : ModuleRules
    {
        public NavigationSystem(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicIncludePaths.AddRange(
                new string[] {
                    "Runtime/NavigationSystem/Public",
                }
                );

            PrivateIncludePaths.AddRange(
                new string[] {
                    "Runtime/NavigationSystem/Private",
                    "Runtime/Engine/Private",
                    "Developer/DerivedDataCache/Public",
                }
                );

            PublicDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                }
                );

            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "RHI",
                    "RenderCore",
                    "ShaderCore",
                }
                );

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"TargetPlatform",
				}
				);
            
            if (!Target.bBuildRequiresCookedData && Target.bCompileAgainstEngine)
            {
                DynamicallyLoadedModuleNames.Add("DerivedDataCache");
            }

            SetupModulePhysicsSupport(Target);

            if (Target.bCompileRecast)
            {
                PrivateDependencyModuleNames.Add("Navmesh");
                PublicDefinitions.Add("WITH_RECAST=1");
            }
            else
            {
                // Because we test WITH_RECAST in public Engine header files, we need to make sure that modules
                // that import us also have this definition set appropriately.  Recast is a private dependency
                // module, so it's definitions won't propagate to modules that import Engine.
                PublicDefinitions.Add("WITH_RECAST=0");
            }

            if (Target.bBuildEditor == true)
            {
                // @todo api: Only public because of WITH_EDITOR and UNREALED_API
                PublicDependencyModuleNames.Add("UnrealEd");
                CircularlyReferencedDependentModules.Add("UnrealEd");
            }
        }
    }
}
