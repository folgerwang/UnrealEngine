// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealEd : ModuleRules
{
	public UnrealEd(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivatePCHHeaderFile = "Private/UnrealEdPrivatePCH.h";

		SharedPCHHeaderFile = "Public/UnrealEdSharedPCH.h";

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Editor/UnrealEd/Private",
				"Editor/UnrealEd/Private/Settings",
				"Editor/PackagesDialog/Public",
				"Developer/DerivedDataCache/Public",
				"Developer/TargetPlatform/Public",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"BehaviorTreeEditor",
				"ClassViewer",
				"ContentBrowser",
				"DerivedDataCache",
				"DesktopPlatform",
				"LauncherPlatform",
				"EnvironmentQueryEditor",
				"GameProjectGeneration",
				"ProjectTargetPlatformEditor",
				"ImageWrapper",
				"MainFrame",
				"MaterialEditor",
				"MergeActors",
				"MeshUtilities",
				"MessagingCommon",
				"MovieSceneCapture",
				"PlacementMode",
				"Settings",
				"SettingsEditor",
				"AudioEditor",
				"ViewportSnapping",
				"SourceCodeAccess",
				"IntroTutorials",
				"OutputLog",
				"Landscape",
				"LocalizationService",
				"HierarchicalLODUtilities",
				"MessagingRpc",
				"PortalRpc",
				"PortalServices",
				"BlueprintNativeCodeGen",
				"ViewportInteraction",
				"VREditor",
				"Persona",
				"PhysicsAssetEditor",
				"ClothingSystemEditorInterface",
				"NavigationSystem",
				"Media",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"BspMode",
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"DirectoryWatcher",
				"Documentation",
				"Engine",
				"Json",
				"Projects",
				"SandboxFile",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"SourceControl",
				"UnrealEdMessages",
				"GameplayDebugger",
				"BlueprintGraph",
				"Http",
				"UnrealAudio",
				"FunctionalTesting",
				"AutomationController",
				"Localization",
				"AudioEditor",
				"NetworkFileSystem",
				"UMG",
				"NavigationSystem",
                "MeshDescription",
                "MeshBuilder",
                "MaterialShaderQualitySettings"
            }
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"LevelSequence",
				"AnimGraph",
				"AppFramework",
				"BlueprintGraph",
				"CinematicCamera",
				"CurveEditor",
				"DesktopPlatform",
				"LauncherPlatform",
				"EditorStyle",
				"EngineSettings",
				"ImageWriteQueue",
				"InputCore",
				"InputBindingEditor",
				"LauncherServices",
				"MaterialEditor",
				"MessageLog",
				"PakFile",
				"PropertyEditor",
				"Projects",
				"RawMesh",
				"MeshUtilitiesCommon",
				"RenderCore",
				"RHI",
				"ShaderCore",
				"Sockets",
				"SourceControlWindows",
				"StatsViewer",
				"SwarmInterface",
				"TargetPlatform",
				"TargetDeviceServices",
				"EditorWidgets",
				"GraphEditor",
				"Kismet",
				"InternationalizationSettings",
				"JsonUtilities",
				"Landscape",
				"HeadMountedDisplay",
				"MeshPaint",
				"MeshPaintMode",
				"Foliage",
				"VectorVM",
				"MaterialUtilities",
				"Localization",
				"LocalizationService",
				"AddContentDialog",
				"GameProjectGeneration",
				"HierarchicalLODUtilities",
				"Analytics",
				"AnalyticsET",
				"PluginWarden",
				"PixelInspectorModule",
				"MovieScene",
				"MovieSceneTracks",
				"ViewportInteraction",
				"VREditor",
				"ClothingSystemEditor",
				"ClothingSystemRuntime",
				"ClothingSystemRuntimeInterface",
				"PIEPreviewDeviceProfileSelector",
				"PakFileUtilities",
				"TimeManagement",
            }
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
                "FontEditor",
				"StaticMeshEditor",
				"TextureEditor",
				"Cascade",
				"UMGEditor",
				"Matinee",
				"AssetTools",
				"ClassViewer",
				"CollectionManager",
				"ContentBrowser",
				"CurveTableEditor",
				"DataTableEditor",
				"EditorSettingsViewer",
				"LandscapeEditor",
				"KismetCompiler",
				"DetailCustomizations",
				"ComponentVisualizers",
				"MainFrame",
				"LevelEditor",
				"PackagesDialog",
				"Persona",
				"PhysicsAssetEditor",
				"ProjectLauncher",
				"DeviceManager",
				"SettingsEditor",
				"SessionFrontend",
				"Sequencer",
				"StringTableEditor",
				"GeometryMode",
				"TextureAlignMode",
				"FoliageEdit",
				"ImageWrapper",
				"Blutility",
				"IntroTutorials",
				"WorkspaceMenuStructure",
				"PlacementMode",
				"MeshUtilities",
				"MergeActors",
				"ProjectSettingsViewer",
				"ProjectTargetPlatformEditor",
				"PListEditor",
				"BehaviorTreeEditor",
				"EnvironmentQueryEditor",
				"ViewportSnapping",
				"GameplayTasksEditor",
				"UndoHistory",
				"SourceCodeAccess",
				"HotReload",
				"HTML5PlatformEditor",
				"PortalProxies",
				"PortalServices",
				"BlueprintNativeCodeGen",
				"OverlayEditor",
				"AnimationModifiers",
				"ClothPainter",
				"Media",
				"TimeManagementEditor",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac)
		{
			DynamicallyLoadedModuleNames.Add("IOSPlatformEditor");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
		{
			DynamicallyLoadedModuleNames.Add("AndroidPlatformEditor");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac)
		{
			DynamicallyLoadedModuleNames.Add("LuminPlatformEditor");
		}

		CircularlyReferencedDependentModules.AddRange(
			new string[]
			{
				"GraphEditor",
				"Kismet",
				"AudioEditor",
				"ViewportInteraction",
				"VREditor",
			}
		);


		// Add include directory for Lightmass
		PublicIncludePaths.Add("Programs/UnrealLightmass/Public");

        PublicIncludePaths.Add("Developer/Android/AndroidDeviceDetection/Public/Interfaces");

        PublicIncludePathModuleNames.AddRange(
			new string[] {
				"CollectionManager",
				"BlueprintGraph",
				"AddContentDialog",
				"MeshUtilities",
				"AssetTools",
				"KismetCompiler",
				"NavigationSystem",
				"GameplayTasks",
				"AIModule",
            }
			);


        if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			PublicDependencyModuleNames.Add("XAudio2");
			PublicDependencyModuleNames.Add("AudioMixerXAudio2");

			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"UEOgg",
				"Vorbis",
				"VorbisFile",
				"DX11Audio"
				);
		}

		if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			PublicDependencyModuleNames.Add("ALAudio");
            PublicDependencyModuleNames.Add("AudioMixerSDL");
		}

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"VHACD",
			"FBX",
			"FreeType2"
		);

		SetupModulePhysicsSupport(Target);

		if (Target.bCompileRecast)
		{
			PrivateDependencyModuleNames.Add("Navmesh");
			PublicDefinitions.Add( "WITH_RECAST=1" );
		}
		else
		{
			PublicDefinitions.Add( "WITH_RECAST=0" );
		}
	}
}
