// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SimplygonSwarmCommon.h"
#include "SimplygonSwarmHelpers.h"
#include "SimplygonRESTClient.h"
#include "Modules/ModuleManager.h"
#include "UObject/Object.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Misc/EngineVersion.h"
#include "Misc/MonitoredProcess.h"
#include "Templates/UniquePtr.h"
THIRD_PARTY_INCLUDES_START
#include <algorithm>
THIRD_PARTY_INCLUDES_END

#define LOCTEXT_NAMESPACE "SimplygonSwarm"

#include "IMeshReductionInterfaces.h"

#include "MeshMergeData.h"
#include "Features/IModularFeatures.h"

#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "MeshAttributeArray.h"
#include "MeshDescriptionOperations.h"

// Standard Simplygon channels have some issues with extracting color data back from simplification, 
// so we use this workaround with user channels
static const char* USER_MATERIAL_CHANNEL_METALLIC = "UserMetallic";
static const char* USER_MATERIAL_CHANNEL_ROUGHNESS = "UserRoughness";
static const char* USER_MATERIAL_CHANNEL_SPECULAR = "UserSpecular";

static const TCHAR* BASECOLOR_CHANNEL = TEXT("Basecolor");
static const TCHAR* METALLIC_CHANNEL = TEXT("Metallic");
static const TCHAR* SPECULAR_CHANNEL = TEXT("Specular");
static const TCHAR* ROUGHNESS_CHANNEL = TEXT("Roughness");
static const TCHAR* NORMAL_CHANNEL = TEXT("Normals");
static const TCHAR* OPACITY_CHANNEL = TEXT("Opacity");
static const TCHAR* EMISSIVE_CHANNEL = TEXT("Emissive");
static const TCHAR* OPACITY_MASK_CHANNEL = TEXT("OpacityMask");
static const TCHAR* AO_CHANNEL = TEXT("AmbientOcclusion");
static const TCHAR* MATERIAL_MASK_CHANNEL = TEXT("MaterialMask");
static const TCHAR* OUTPUT_LOD = TEXT("outputlod_0");
static const TCHAR* SSF_FILE_TYPE = TEXT("ssf");
static const TCHAR* REMESHING_PROCESSING_SETNAME = TEXT("RemeshingProcessingSet");
static const TCHAR* CLIPPING_GEOMETRY_SETNAME = TEXT("ClippingObjectSet");


#define SIMPLYGON_COLOR_CHANNEL "VertexColors"

#define KEEP_SIMPLYGON_SWARM_TEMPFILES

static const TCHAR* SG_UE_INTEGRATION_REV = TEXT("#SG_UE_INTEGRATION_REV");

#ifdef __clang__
	// SimplygonSDK.h uses 'deprecated' pragma which Clang does not recognize
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wunknown-pragmas"	// warning : unknown pragma ignored [-Wunknown-pragmas]
#endif

#ifdef __clang__
	#pragma clang diagnostic pop
#endif

#define MAX_UPLOAD_PART_SIZE_MB  1024
#define MAX_UPLOAD_PART_SIZE_BYTES ( MAX_UPLOAD_PART_SIZE_MB * 1024 * 1024 ) 

static const TCHAR SHADING_NETWORK_TEMPLATE[] = TEXT("<SimplygonShadingNetwork version=\"1.0\">\n\t<ShadingTextureNode ref=\"node_0\" name=\"ShadingTextureNode\">\n\t\t<DefaultColor0>\n\t\t\t<DefaultValue>1 1 1 1</DefaultValue>\n\t\t</DefaultColor0>\n\t\t<TextureName>%s</TextureName>\n\t\t<TextureLevelName>%s</TextureLevelName>\n\t\t<UseSRGB>%d</UseSRGB>\n\t\t<TileU>1.000000</TileU>\n\t\t<TileV>1.000000</TileV>\n\t</ShadingTextureNode>\n</SimplygonShadingNetwork>");

ssf::pssfMeshData CreateSSFMeshDataFromRawMesh(const FMeshDescription& InRawMesh, TArray<FBox2D> InTextureBounds, TArray<FVector2D> InTexCoords);

class FSimplygonSwarmModule : public IMeshReductionModule
{
public:
	// IModuleInterface interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IMeshReductionModule interface.
	virtual class IMeshReduction* GetStaticMeshReductionInterface() override;
	virtual class IMeshReduction* GetSkeletalMeshReductionInterface() override;
	virtual class IMeshMerging* GetMeshMergingInterface() override;
	virtual class IMeshMerging* GetDistributedMeshMergingInterface() override;
	virtual FString GetName() override;

private:
};

DEFINE_LOG_CATEGORY_STATIC(LogSimplygonSwarm, Log, All);
IMPLEMENT_MODULE(FSimplygonSwarmModule, SimplygonSwarm);

class FSimplygonSwarm
	: public IMeshMerging
{
public:
	virtual ~FSimplygonSwarm()
	{
	}

	static FSimplygonSwarm* Create()
	{
		return new FSimplygonSwarm();
	}

	virtual FString GetName() override
	{
		return FString("SimplygonSwarm");
	}

	struct FMaterialCastingProperties
	{
		bool bCastMaterials;
		bool bCastNormals;
		bool bCastMetallic;
		bool bCastRoughness;
		bool bCastSpecular;

		FMaterialCastingProperties()
			: bCastMaterials(false)
			, bCastNormals(false)
			, bCastMetallic(false)
			, bCastRoughness(false)
			, bCastSpecular(false)
		{
		}
	};

	/**
	* Method used to generate ProxyLOD either using Remeshing or Aggregation
	* @param InData			Mesh Merge Data
	* @param InProxySettings	Settings to use for proxy generation
	* @param InputMaterials	Flattened materials
	* @param InJobGUID			Job GUID
	*/
	 virtual void ProxyLOD(const TArray<FMeshMergeData>& InData,
		const struct FMeshProxySettings& InProxySettings,
		const TArray<struct FFlattenMaterial>& InputMaterials,
		const FGuid InJobGUID)
	{
		FScopedSlowTask SlowTask(3.f, (LOCTEXT("SimplygonSwarm_ProxyLOD", "Generating Proxy Mesh using Simplygon Swarm")));
		SlowTask.MakeDialog();
		
		FMeshDescription OutProxyMesh;
		FFlattenMaterial OutMaterial;

		//setup path variables
		FString JobPath = FGuid::NewGuid().ToString();
		FString JobDirectory = FString::Printf(TEXT("%s%s"), *GetMutableDefault<UEditorPerProjectUserSettings>()->SwarmIntermediateFolder, *JobPath);
		FString InputFolderPath = FString::Printf(TEXT("%s/Input"), *JobDirectory);

		FString ZipFileName = FString::Printf(TEXT("%s/%s.zip"), *JobDirectory, *JobPath);
		FString OutputZipFileName = FString::Printf(TEXT("%s/%s_output.zip"), *JobDirectory, *JobPath);
		FString SPLFileOutputFullPath = FString::Printf(TEXT("%s/input.spl"), *InputFolderPath);
		FString SPLSettingsText;

		EBlendMode OutputMaterialBlendMode = BLEND_Opaque;
		bool bHasMaked = false;
		bool bHasOpacity = false;

		for (int MaterialIndex = 0; MaterialIndex < InputMaterials.Num(); MaterialIndex++)
		{
			if (InputMaterials[MaterialIndex].BlendMode == BLEND_Translucent)
			{
				bHasOpacity = true;
			}

			if (InputMaterials[MaterialIndex].BlendMode == BLEND_Masked)
			{
				bHasMaked = true;
			}
		}

		if ( (bHasMaked && bHasOpacity) || bHasOpacity)
		{
			OutputMaterialBlendMode = BLEND_Translucent;
		}
		else if (bHasMaked && !bHasOpacity)
		{
			OutputMaterialBlendMode = BLEND_Masked;
		}

		//scan for clipping geometry 
		bool bHasClippingGeometry = false;
		if (InData.FindByPredicate([](const FMeshMergeData InMeshData) {return InMeshData.bIsClippingMesh == true; }))
		{
			bHasClippingGeometry = true;
		}

		SPL::SPL* spl = new SPL::SPL();
		spl->Header.ClientName = TCHAR_TO_ANSI(TEXT("UE4"));
		spl->Header.ClientVersion = TCHAR_TO_ANSI(*FEngineVersion::Current().ToString());
		spl->Header.SimplygonVersion = TCHAR_TO_ANSI(TEXT("8.0"));
		SPL::ProcessNode* splProcessNode = new SPL::ProcessNode();
		spl->ProcessGraph = splProcessNode;

		SlowTask.EnterProgressFrame(1.0f, LOCTEXT("SimplygonSwarm_CreateSPL", "Generating Simplygon Processing Settings"));
		 
		CreateRemeshingProcess(InProxySettings, *splProcessNode, OutputMaterialBlendMode, bHasClippingGeometry);

		ssf::pssfScene SsfScene;
		
		TArray<FMeshDescription*> InputMeshes;

		for (auto Data : InData)
		{
			InputMeshes.Push(Data.RawMesh);
		}

		bool bDiscardEmissive = true;
		for (int32 MaterialIndex = 0; MaterialIndex < InputMaterials.Num(); MaterialIndex++)
		{
			const FFlattenMaterial& FlattenMaterial = InputMaterials[MaterialIndex];
			bDiscardEmissive &= ((!FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Emissive) || (FlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Emissive) && FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive)[0] == FColor::Black)));
		}

		SlowTask.EnterProgressFrame(1.0f, LOCTEXT("SimplygonSwarm_GenerateData", "Generating Simplygon Processing Data"));

		//converts UE entities to ssf, Textures will be exported to file
		ConvertMeshMergeDataToSsfScene(InData, InputMaterials, InProxySettings, InputFolderPath, SsfScene);
		
		SsfScene->CoordinateSystem->Value = 1;
		SsfScene->WorldOrientation->Value = 3;
		
		FString SsfOuputPath = FString::Printf(TEXT("%s/input.ssf"), *InputFolderPath);

		//save out ssf file.
		WriteSsfFile(SsfScene, SsfOuputPath);

		spl->Save(TCHAR_TO_ANSI(*SPLFileOutputFullPath));

		SlowTask.EnterProgressFrame(1.0f, LOCTEXT("SimplygonSwarm_UploadData", "Uploading Processing Data to Simplygon Swarm Server"));
		//zip contents and spawn a task 
		if (ZipContentsForUpload(InputFolderPath, ZipFileName))
		{
			//validate if patch exist
			if (!FPaths::FileExists(*FPaths::ConvertRelativePathToFull(ZipFileName)))
			{
				UE_LOG(LogSimplygonSwarm, Error , TEXT("Could not find zip file for uploading %s"), *ZipFileName);
				FailedDelegate.ExecuteIfBound(InJobGUID, TEXT("Could not find zip file for uploading"));
				return; //-V773
			}

			FSwarmTaskkData TaskData;
			TaskData.ZipFilePath = ZipFileName;
			TaskData.SplFilePath = SPLFileOutputFullPath;
			TaskData.OutputZipFilePath = OutputZipFileName;
			TaskData.JobDirectory = JobDirectory;
			TaskData.StateLock = new FCriticalSection();
			TaskData.ProcessorJobID = InJobGUID;
			TaskData.bDitheredTransition = (InputMaterials.Num() > 0) ? InputMaterials[0].bDitheredLODTransition : false;
			TaskData.bEmissive = !bDiscardEmissive;
			TaskData.JobName = InData[0].DebugJobName;
						 
			int32 MaxUploadSizeInBytes = GetMutableDefault<UEditorPerProjectUserSettings>()->SwarmMaxUploadChunkSizeInMB * 1024 * 1024;
			FSimplygonRESTClient::Get()->SetMaxUploadSizeInBytes(MaxUploadSizeInBytes);
			TSharedPtr<FSimplygonSwarmTask> SwarmTask = MakeShareable(new FSimplygonSwarmTask(TaskData));
			SwarmTask->OnAssetDownloaded().BindRaw(this, &FSimplygonSwarm::ImportFile);
			SwarmTask->OnAssetUploaded().BindRaw(this, &FSimplygonSwarm::Cleanup);
			SwarmTask->OnSwarmTaskFailed().BindRaw(this, &FSimplygonSwarm::OnSimplygonSwarmTaskFailed);
			FSimplygonRESTClient::Get()->AddSwarmTask(SwarmTask);			
		}
		
		delete spl;
	}

	/**
	* The following method is called when a swarm task fails. This forwards the call to external module
	* @param InSwarmTask			The completed swarm task
	*/
	void OnSimplygonSwarmTaskFailed(const FSimplygonSwarmTask& InSwarmTask)
	{
		FailedDelegate.ExecuteIfBound(InSwarmTask.TaskData.ProcessorJobID, TEXT("Simplygon Swarm Proxy Generation failed."));
	}

	/**
	* Method to clean up temporary files after uploading the job to Simplygon Grid Server
	* @param InSwarmTask			The completed swarm task
	*/
	void Cleanup(const FSimplygonSwarmTask& InSwarmTask)
	{
		bool bDebuggingEnabled = GetDefault<UEditorPerProjectUserSettings>()->bEnableSwarmDebugging;
		
		if(!bDebuggingEnabled)
		{
			FString InputFolderPath = FPaths::ConvertRelativePathToFull(FString::Printf(TEXT("%s/Input"), *InSwarmTask.TaskData.JobDirectory));
			//remove folder folder
			if (FPaths::DirectoryExists(InputFolderPath))
			{
				if (!IFileManager::Get().DeleteDirectory(*InputFolderPath, true, true))
				{
					UE_LOG(LogSimplygonSwarm, Log, TEXT("Failed to remove simplygon swarm task temp directory %s"), *InputFolderPath);
				}
			}
			FString FullZipPath = FPaths::ConvertRelativePathToFull(*InSwarmTask.TaskData.ZipFilePath);
			//remove uploaded zip file
			if (FPaths::FileExists(FullZipPath))
			{
				if (!IFileManager::Get().Delete(*FullZipPath))
				{
					UE_LOG(LogSimplygonSwarm, Log, TEXT("Failed to remove Simplygon Swarm Task temp file %s"), *InSwarmTask.TaskData.ZipFilePath);
				}
			}
		}
	}


	/**
	* Fired when the Server returns the completed job to the client. Called from RESTClient
	* @param InSwarmTask			The completed swarm task
	*/
	void ImportFile(const FSimplygonSwarmTask& InSwarmTask)
	{

		FMeshDescription OutProxyMesh;
		FFlattenMaterial OutMaterial;
		bool bDebuggingEnabled = GetDefault<UEditorPerProjectUserSettings>()->bEnableSwarmDebugging;
		FString OutputFolderPath = FString::Printf(TEXT("%s/Output"), *InSwarmTask.TaskData.JobDirectory);
		FString ParentDirForOutputSsf = FString::Printf(TEXT("%s/outputlod_0"), *OutputFolderPath);

		//for import the file back in uncomment
		FString ZipFileFullPath = FPaths::ConvertRelativePathToFull(InSwarmTask.TaskData.OutputZipFilePath);
		FString UnzipOutputFullPath = FPaths::ConvertRelativePathToFull(OutputFolderPath);
		if (UnzipDownloadedContent(ZipFileFullPath, UnzipOutputFullPath))
		{
			FString InOuputSsfPath = FString::Printf(TEXT("%s/output.ssf"), *ParentDirForOutputSsf);
			ssf::pssfScene OutSsfScene = new ssf::ssfScene();
			FString SsfFullPath = FPaths::ConvertRelativePathToFull(InOuputSsfPath);

			if (!FPaths::FileExists(SsfFullPath))
			{
				UE_LOG(LogSimplygonSwarm, Log, TEXT("Ssf file not found %s"), *SsfFullPath);
				FailedDelegate.ExecuteIfBound(InSwarmTask.TaskData.ProcessorJobID, TEXT("Ssf file not found"));
				return;
			}

			ReadSsfFile(SsfFullPath, OutSsfScene);
			ConvertFromSsfSceneToRawMesh(OutSsfScene, OutProxyMesh, OutMaterial, ParentDirForOutputSsf);
			OutMaterial.bDitheredLODTransition = InSwarmTask.TaskData.bDitheredTransition;

		    if (!InSwarmTask.TaskData.bEmissive)
		 	{
				OutMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive).Empty();
				OutMaterial.SetPropertySize(EFlattenMaterialProperties::Emissive, FIntPoint(0,0));
			}

			if (!OutProxyMesh.VertexInstances().Num())
			{
				UE_LOG(LogSimplygonSwarm, Log, TEXT("RawMesh is invalid."));
				FailedDelegate.ExecuteIfBound(InSwarmTask.TaskData.ProcessorJobID, TEXT("Invalid FMeshDescription data"));
			}

			 
			//do cleanup work
			if (!bDebuggingEnabled)
			{
				FString FullOutputFolderPath = FPaths::ConvertRelativePathToFull(*OutputFolderPath);
				if (!IFileManager::Get().DeleteDirectory(*FullOutputFolderPath, true, true))
					UE_LOG(LogSimplygonSwarm, Error, TEXT("Failed to remove simplygon swarm task temp directory %s"), *FullOutputFolderPath);

				FString FullOutputFileName = FPaths::ConvertRelativePathToFull(*InSwarmTask.TaskData.OutputZipFilePath);
				//remove uploaded zip file
				if (!IFileManager::Get().Delete(*FullOutputFileName, true, true, false))
				{
					UE_LOG(LogSimplygonSwarm, Error, TEXT("Failed to remove Simplygon Swarm Task temp file %s"), *FullOutputFileName);
				}
			}

			//if is bound then execute
			if (CompleteDelegate.IsBound())
			{
				CompleteDelegate.Execute(OutProxyMesh, OutMaterial, InSwarmTask.TaskData.ProcessorJobID);
			}
			else
			{
				UE_LOG(LogSimplygonSwarm, Error, TEXT("No valid complete delegate is currently bounded. "));
			}	
			 
		}
		else
		{
			UE_LOG(LogSimplygonSwarm, Log, TEXT("Failed to unzip downloaded content %s"), *ZipFileFullPath);
			FailedDelegate.ExecuteIfBound(InSwarmTask.TaskData.ProcessorJobID, TEXT("Invalid FMeshDescription data"));
		}
	}

private:
	FString VersionString;
	FSimplygonRESTClient* SgRESTInterface;
	//FRunnableThread *Thread;
	uint8 ToolMajorVersion;
	uint8 ToolMinorVersion;
	uint16 ToolBuildVersion;

	explicit FSimplygonSwarm()
	{
		VersionString = FString::Printf(TEXT("%s"), SG_UE_INTEGRATION_REV);
		ToolMajorVersion =  FEngineVersion::Current().GetMajor();
		ToolMinorVersion = FEngineVersion::Current().GetMinor();
		ToolBuildVersion = FEngineVersion::Current().GetPatch();		 
	}

	/**
	* Read in ssf file from disk
	* @param InSsfFilePath	Ssf file to read in
	* @param SsfScene		SsfScene that the ssf file is read into
	*/
	void ReadSsfFile(FString InSsfFilePath, ssf::pssfScene& SsfScene)
	{
		ssf::ssfString ToolName = FSimplygonSSFHelper::TCHARToSSFString(TEXT("UE4"));

		ssf::ssfBinaryInputStream InputStream;
		InputStream.OpenFile(FSimplygonSSFHelper::TCHARToSSFString(*InSsfFilePath));
		SsfScene->ReadFile(&InputStream, ToolName, ToolMajorVersion, ToolMinorVersion, ToolBuildVersion);
	}

	/**
	* Write out ssf scene to disk
	* @param SsfScene		SsfScene to write out
	* @param InSsfFilePath	Path to ssf file
	*/
	void WriteSsfFile(ssf::pssfScene SsfScene, FString InSsfFilePath)
	{
		ssf::ssfString ToolName = FSimplygonSSFHelper::TCHARToSSFString(TEXT("UE4"));
		ssf::ssfBinaryOutputStream theOutputStream;
		theOutputStream.OpenFile(FSimplygonSSFHelper::TCHARToSSFString(*InSsfFilePath));
		SsfScene->WriteFile(&theOutputStream, ToolName, ToolMajorVersion, ToolMinorVersion, ToolBuildVersion);
		theOutputStream.CloseFile();
	}

	/**
	* Setup spl mapping image object used for material baking
	* @param InMaterialProxySettings	Proxy Settings to use for setting up remeshing process node
	* @param InMappingImageSettings	Mapping image setting object
	*/
	void SetupSplMappingImage(const struct FMaterialProxySettings& InMaterialProxySettings, SPL::MappingImageSettings& InMappingImageSettings)
	{
		FIntPoint ImageSizes = ComputeMappingImageSize(InMaterialProxySettings);
		bool bAutomaticTextureSize = InMaterialProxySettings.TextureSizingType == TextureSizingType_UseSimplygonAutomaticSizing;

		InMappingImageSettings.GenerateMappingImage = true;
		InMappingImageSettings.GutterSpace = InMaterialProxySettings.GutterSpace;
		InMappingImageSettings.UseAutomaticTextureSize = bAutomaticTextureSize;
		InMappingImageSettings.Height = ImageSizes.X;
		InMappingImageSettings.Width = ImageSizes.Y;
		InMappingImageSettings.UseFullRetexturing = true;
		InMappingImageSettings.GenerateTangents = true;
		InMappingImageSettings.GenerateTexCoords = true;
		InMappingImageSettings.TexCoordLevel = 255;
		InMappingImageSettings.MultisamplingLevel = 3;
		InMappingImageSettings.TexCoordGeneratorType = SPL::TexCoordGeneratorType::SG_TEXCOORDGENERATORTYPE_PARAMETERIZER;
		InMappingImageSettings.Enabled = true;

	}

	/**
	* Create Spl Process node for Remeshing
	* @param InProxySettings			Proxy Settings to use for setting up remeshing process node
	* @param InProcessNodeSpl			SplProcessNode object
	* @param InOutputMaterialBlendMode	Output Material Blend mode
	* @param InHasClippingGeometry		Weather or the scene being processed has clipping geometry
	*/
	void CreateRemeshingProcess(const struct FMeshProxySettings& InProxySettings, SPL::ProcessNode& InProcessNodeSpl, EBlendMode InOutputMaterialBlendMode = BLEND_Opaque, bool InHasClippingGeometry = false)
	{
		SPL::RemeshingProcessor* processor = new SPL::RemeshingProcessor();
		processor->RemeshingSettings = new SPL::RemeshingSettings();
		 
		processor->RemeshingSettings->OnScreenSize = InProxySettings.ScreenSize;
		processor->RemeshingSettings->SurfaceTransferMode = SPL::SurfaceTransferMode::SG_SURFACETRANSFER_ACCURATE;
		processor->RemeshingSettings->ProcessSelectionSetName = TCHAR_TO_ANSI(REMESHING_PROCESSING_SETNAME);

		if (InHasClippingGeometry)
		{
			processor->RemeshingSettings->UseClippingGeometryEmptySpaceOverride = false;
			processor->RemeshingSettings->UseClippingGeometry = InHasClippingGeometry;
			processor->RemeshingSettings->ClippingGeometrySelectionSetName = TCHAR_TO_ANSI(CLIPPING_GEOMETRY_SETNAME);
		}

		if (InProxySettings.bRecalculateNormals)
			processor->RemeshingSettings->HardEdgeAngleInRadians = FMath::DegreesToRadians(InProxySettings.HardAngleThreshold);

		processor->RemeshingSettings->MergeDistance = InProxySettings.MergeDistance;
		processor->RemeshingSettings->Enabled = true;

		FIntPoint ImageSizes = ComputeMappingImageSize(InProxySettings.MaterialSettings);

		//mapping image settings
		processor->MappingImageSettings = new SPL::MappingImageSettings();
		SetupSplMappingImage(InProxySettings.MaterialSettings, *processor->MappingImageSettings);
		 
		SetupSplMaterialCasters(InProxySettings.MaterialSettings, InProcessNodeSpl, InOutputMaterialBlendMode);
		 
		InProcessNodeSpl.Processor = processor;
		InProcessNodeSpl.DefaultTBNType = SPL::SG_TANGENTSPACEMETHOD_ORTHONORMAL_LEFTHANDED;
		 
		SPL::WriteNode* splWriteNode = new SPL::WriteNode();
		splWriteNode->Format = TCHAR_TO_ANSI(SSF_FILE_TYPE);
		splWriteNode->Name = TCHAR_TO_ANSI(OUTPUT_LOD);
		 

		InProcessNodeSpl.Children.push_back(splWriteNode);
	}

	/**
	* Create Spl Process node for Remeshing
	* @param InSplText			Save SPL Text
	* @param InOutputFilePath	SplProcessNode object
	*/
	void SaveSPL(FString InSplText, FString InOutputFilePath)
	{
		FArchive* SPLFile = IFileManager::Get().CreateFileWriter(*InOutputFilePath);
		SPLFile->Logf(TEXT("%s"), *InSplText);
		SPLFile->Close();
	}

	/**
	* Convert collection of FMeshMergeData to SsfScene
	* @param InMeshMergeData	Meshes to merge
	* @param InputMaterials	Flattened Materials
	* @param InProxySettings	Proxy Settings
	* @param InputFolderPath	Input Folder Path
	* @param OutSsfScene		Out SsfScene
	*/
	void ConvertMeshMergeDataToSsfScene(const TArray<FMeshMergeData>& InMeshMergeData,
		const TArray<FFlattenMaterial>& InputMaterials,
		const struct FMeshProxySettings& InProxySettings, FString InputFolderPath, ssf::pssfScene& OutSsfScene)
	{
		//create the ssf scene
		OutSsfScene = new ssf::ssfScene();

		OutSsfScene->CoordinateSystem.Set(1);
		OutSsfScene->WorldOrientation.Set(2);
		OutSsfScene->TextureTable->TexturesDirectory.Set(FSimplygonSSFHelper::TCHARToSSFString(TEXT("/Textures")));

		//set processing and clipping geometry sets

		//processing set
		ssf::ssfNamedIdList<ssf::ssfString> ProcessingObjectsSet;
		ssf::ssfNamedIdList<ssf::ssfString> ClippingGeometrySet;
		 
		ProcessingObjectsSet.Name = FSimplygonSSFHelper::TCHARToSSFString(REMESHING_PROCESSING_SETNAME);
		ProcessingObjectsSet.ID = FSimplygonSSFHelper::SSFNewGuid();
		ClippingGeometrySet.Name = FSimplygonSSFHelper::TCHARToSSFString(CLIPPING_GEOMETRY_SETNAME);
		ClippingGeometrySet.ID = FSimplygonSSFHelper::SSFNewGuid();
		 
		 
		TMap<int, FString> MaterialMap;

		CreateSSFMaterialFromFlattenMaterial(InputMaterials, InProxySettings.MaterialSettings, OutSsfScene->MaterialTable, OutSsfScene->TextureTable, InputFolderPath, true, MaterialMap);

		//create the root node
		ssf::pssfNode SsfRootNode = new ssf::ssfNode();
		SsfRootNode->Id.Set(FSimplygonSSFHelper::SSFNewGuid());
		SsfRootNode->ParentId.Set(FSimplygonSSFHelper::SFFEmptyGuid());

		//add root node to scene
		OutSsfScene->NodeTable->NodeList.push_back(SsfRootNode);

		int32 Count = 0;
		for (FMeshMergeData MergeData : InMeshMergeData)
		{
			//create a the node that will contain the mesh
			ssf::pssfNode SsfNode = new ssf::ssfNode();
			SsfNode->Id.Set(FSimplygonSSFHelper::SSFNewGuid());
			SsfNode->ParentId.Set(SsfRootNode->Id.Get());
			FString NodeName = FString::Printf(TEXT("Node%i"), Count);

			SsfNode->Name.Set(FSimplygonSSFHelper::TCHARToSSFString(*NodeName));
			ssf::ssfMatrix4x4 IdenMatrix;
			IdenMatrix.M[0][0] = IdenMatrix.M[1][1] = IdenMatrix.M[2][2] = IdenMatrix.M[3][3] = 1;
			SsfNode->LocalTransform.Set(IdenMatrix);

			//create the mesh object
			ssf::pssfMesh SsfMesh = new ssf::ssfMesh();
			SsfMesh->Id.Set(FSimplygonSSFHelper::SSFNewGuid());
			FString MeshName = FString::Printf(TEXT("Mesh%i"), Count);
			SsfMesh->Name.Set(FSimplygonSSFHelper::TCHARToSSFString(*MeshName));

			Count++;

			//setup mesh data
			ssf::pssfMeshData SsfMeshData = CreateSSFMeshDataFromRawMesh(*MergeData.RawMesh, MergeData.TexCoordBounds, MergeData.NewUVs);
			SsfMesh->MeshDataList.push_back(SsfMeshData);

			//setup mesh material information
			SsfMesh->MaterialIds.Create();
			TArray<int32> UniqueMaterialIds;
			UniqueMaterialIds.Reserve(InputMaterials.Num());

			//get unqiue material ids
			GetUniqueMaterialIndices(*(MergeData.RawMesh), UniqueMaterialIds);

			SsfMesh->MaterialIds->Items.reserve(UniqueMaterialIds.Num());

			TMap<int, int> GlobalToLocal;
			//map ssfmesh local materials
			for (int32 GlobalMaterialId : UniqueMaterialIds)
			{
				SsfMesh->MaterialIds->Items.push_back(FSimplygonSSFHelper::TCHARToSSFString(*MaterialMap[GlobalMaterialId]));
				int32 localIndex = SsfMesh->MaterialIds->Items.size() - 1;
				//replace 
				GlobalToLocal.Add(GlobalMaterialId, localIndex);
			}

			for (ssf::pssfMeshData MeshData : SsfMesh->MeshDataList)
			{
				for (int Index = 0; Index < MeshData->MaterialIndices.Get().Items.size(); Index++)
				{
					MeshData->MaterialIndices.Get().Items[Index] = GlobalToLocal[MeshData->MaterialIndices.Get().Items[Index]];
				}
			}

			//link mesh to node
			SsfNode->MeshId.Set(SsfMesh->Id.Get().Value);
			 
			//add mesh and node to their respective tables
			OutSsfScene->NodeTable->NodeList.push_back(SsfNode);
			OutSsfScene->MeshTable->MeshList.push_back(SsfMesh);

			if (MergeData.bIsClippingMesh)
			{
				ClippingGeometrySet.Items.push_back(SsfNode->Id->ToCharString());
			}
			else
			{
				ProcessingObjectsSet.Items.push_back(SsfNode->Id->ToCharString());
			}


		}

		if(ClippingGeometrySet.Items.size() > 0)
			OutSsfScene->SelectionGroupSetsList.push_back(ClippingGeometrySet);
		 
		if (ProcessingObjectsSet.Items.size() > 0)
		OutSsfScene->SelectionGroupSetsList.push_back(ProcessingObjectsSet);
		 
	}

	/**
	* Convert SsfScnee to RawMesh. Currently assumes that only a single mesh will be present in the SsfScene
	* @param SsfScene			SsfScene
	* @param OutProxyMesh		Converted SsfMeshData to RawMesh
	* @param OutMaterial		Converted SsfMaterial to Flattened Material
	* @param BaseTexturesPath	Base Path for textures
	*/
	void ConvertFromSsfSceneToRawMesh(ssf::pssfScene SsfScene, FMeshDescription& OutProxyMesh, FFlattenMaterial& OutMaterial, const FString BaseTexturesPath)
	{
		TVertexAttributesRef<FVector> VertexPositions = OutProxyMesh.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
		TEdgeAttributesRef<bool> EdgeHardnesses = OutProxyMesh.EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
		TEdgeAttributesRef<float> EdgeCreaseSharpnesses = OutProxyMesh.EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness);
		TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = OutProxyMesh.PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);
		TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = OutProxyMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
		TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = OutProxyMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
		TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = OutProxyMesh.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
		TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = OutProxyMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
		TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = OutProxyMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

		bool bReverseWinding = true;
		 
		for (ssf::pssfMesh Mesh : SsfScene->MeshTable->MeshList)
		{
			//extract geometry data
			for (ssf::pssfMeshData MeshData : Mesh->MeshDataList)
			{
				int32 TotalVertices = MeshData->GetVerticesCount();
				int32 TotalCorners = MeshData->GetCornersCount();
				int32 TotalTriangles = MeshData->GetTrianglesCount();

				//Assuming only one mesh
				OutProxyMesh.Empty();
				OutProxyMesh.ReserveNewVertices(TotalVertices);
				OutProxyMesh.ReserveNewPolygons(TotalTriangles);
				OutProxyMesh.ReserveNewVertexInstances(TotalCorners);
				OutProxyMesh.ReserveNewEdges(TotalCorners);

				TMap<int32, FVertexID> SsfToMeshDescriptionVertexID;
				SsfToMeshDescriptionVertexID.Reserve(TotalVertices);
				int32 VertexIndex = 0;
				for (ssf::ssfVector3 VertexCoord : MeshData->Coordinates.Get().Items)
				{
					const FVertexID VertexID = OutProxyMesh.CreateVertex();
					VertexPositions[VertexID] = GetConversionMatrixYUP().InverseTransformPosition(FVector(VertexCoord.V[0], VertexCoord.V[1], VertexCoord.V[2]));
					SsfToMeshDescriptionVertexID.Add(VertexIndex, VertexID);
					VertexIndex++;
				}
				
				//Prepare the tex coord
				int32 TexCoordIndex = 0;
				ssf::ssfNamedList<ssf::ssfVector2> BakedMaterialUVs = FSimplygonSSFHelper::GetBakedMaterialUVs(MeshData->TextureCoordinatesList);
				VertexInstanceUVs.SetNumIndices(1);

				//Is buffer has some data?
				bool Normals = !MeshData->Normals.IsEmpty() && MeshData->Normals.Get().Items.size() > 0;
				bool Tangents = !MeshData->Tangents.IsEmpty() && MeshData->Tangents.Get().Items.size() > 0;
				bool Bitangents = !MeshData->Bitangents.IsEmpty() && MeshData->Bitangents.Get().Items.size() > 0;
				bool MaterialIndices = !MeshData->MaterialIndices.IsEmpty() && MeshData->MaterialIndices.Get().Items.size() > 0;
				bool GroupIds = !MeshData->SmoothingGroup.IsEmpty() && MeshData->SmoothingGroup.Get().Items.size() > 0;

				//Setup PolygonGroup
				//Prepare the polygongroup
				TMap<int32, FPolygonGroupID> SsfToRawMaterial;
				for (int32 TriIndex = 0; TriIndex < TotalTriangles; ++TriIndex)
				{
					int32 MaterialIndex = MaterialIndices ? (int32)MeshData->MaterialIndices.Get().Items[TriIndex].Value : 0;
					if (!SsfToRawMaterial.Contains(MaterialIndex))
					{
						const FPolygonGroupID PolygonGroupID(MaterialIndex);
						OutProxyMesh.CreatePolygonGroupWithID(PolygonGroupID);
						PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = FName(*FString(TEXT("SimplygonSwarm_") + FString::FromInt(PolygonGroupID.GetValue())));
						SsfToRawMaterial.Add(MaterialIndex, PolygonGroupID);
					}
				}

				for (int32 TriIndex = 0; TriIndex < TotalTriangles; ++TriIndex)
				{
					FVertexID VertexIndexes[3];
					FVertexInstanceID VertexInstanceIDs[3];
					for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
					{
						int32 SrcCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
						int32 SrcIndex = (3 * TriIndex) + SrcCornerIndex;
						VertexIndexes[CornerIndex] = SsfToMeshDescriptionVertexID[MeshData->TriangleIndices.Get().Items[TriIndex].V[SrcCornerIndex]];
						VertexInstanceIDs[CornerIndex] = OutProxyMesh.CreateVertexInstance(VertexIndexes[CornerIndex]);

						//Texture Coordinates, copy baked material UV's only discard the rest
						VertexInstanceUVs.Get(VertexInstanceIDs[CornerIndex], 0) = FVector2D(BakedMaterialUVs.Items[SrcIndex].V[0], BakedMaterialUVs.Items[SrcIndex].V[1]);

						//Vertex Color, SSF can store multiple color channels. However UE only supports one color channel
						for (ssf::ssfNamedList<ssf::ssfVector4> TexColorChannel : MeshData->ColorsList)
						{
							VertexInstanceColors[VertexInstanceIDs[CornerIndex]] = FVector4(TexColorChannel.Items[SrcIndex].V[0], TexColorChannel.Items[SrcIndex].V[1], TexColorChannel.Items[SrcIndex].V[2], TexColorChannel.Items[SrcIndex].V[3]);
							break; //UE support only one
						}

						//Tangents
						if (Normals)
						{
							FVector NormalValue = FVector(MeshData->Normals.Get().Items[SrcIndex].V[0], MeshData->Normals.Get().Items[SrcIndex].V[1], MeshData->Normals.Get().Items[SrcIndex].V[2]);
							NormalValue = GetConversionMatrixYUP().InverseTransformPosition(NormalValue);
							VertexInstanceNormals[VertexInstanceIDs[CornerIndex]] = NormalValue;
							if (Tangents && Bitangents)
							{
								FVector TangentValue = FVector(MeshData->Tangents.Get().Items[SrcIndex].V[0], MeshData->Tangents.Get().Items[SrcIndex].V[1], MeshData->Tangents.Get().Items[SrcIndex].V[2]);
								TangentValue = GetConversionMatrixYUP().InverseTransformPosition(TangentValue);
								VertexInstanceTangents[VertexInstanceIDs[CornerIndex]] = TangentValue;
								
								FVector BiTangentValue = FVector(MeshData->Bitangents.Get().Items[SrcIndex].V[0], MeshData->Bitangents.Get().Items[SrcIndex].V[1], MeshData->Bitangents.Get().Items[SrcIndex].V[2]);
								BiTangentValue = GetConversionMatrixYUP().InverseTransformPosition(BiTangentValue);
								VertexInstanceBinormalSigns[VertexInstanceIDs[CornerIndex]] = GetBasisDeterminantSign(TangentValue.GetSafeNormal(), BiTangentValue.GetSafeNormal(), NormalValue.GetSafeNormal());
							}
						}
					}
					//Create a polygon from this triangle
					TArray<FMeshDescription::FContourPoint> Contours;
					for (int32 Corner = 0; Corner < 3; ++Corner)
					{
						int32 ContourPointIndex = Contours.AddDefaulted();
						FMeshDescription::FContourPoint& ContourPoint = Contours[ContourPointIndex];
						//Find the matching edge ID
						uint32 CornerIndices[2];
						CornerIndices[0] = (Corner + 0) % 3;
						CornerIndices[1] = (Corner + 1) % 3;

						FVertexID EdgeVertexIDs[2];
						EdgeVertexIDs[0] = VertexIndexes[CornerIndices[0]];
						EdgeVertexIDs[1] = VertexIndexes[CornerIndices[1]];

						FEdgeID MatchEdgeId = OutProxyMesh.GetVertexPairEdge(EdgeVertexIDs[0], EdgeVertexIDs[1]);
						if (MatchEdgeId == FEdgeID::Invalid)
						{
							MatchEdgeId = OutProxyMesh.CreateEdge(EdgeVertexIDs[0], EdgeVertexIDs[1]);
							EdgeHardnesses[MatchEdgeId] = false;
							EdgeCreaseSharpnesses[MatchEdgeId] = 0.0f;
						}
						ContourPoint.EdgeID = MatchEdgeId;
						ContourPoint.VertexInstanceID = VertexInstanceIDs[CornerIndices[0]];
					}
					// Insert a polygon into the mesh
					const FPolygonID NewPolygonID = OutProxyMesh.CreatePolygon(SsfToRawMaterial[MeshData->MaterialIndices.Get().Items[TriIndex].Value], Contours);
					//Triangulate the polygon
					FMeshPolygon& Polygon = OutProxyMesh.GetPolygon(NewPolygonID);
					OutProxyMesh.ComputePolygonTriangulation(NewPolygonID, Polygon.Triangles);
				}

				TArray<uint32> FaceSmoothingMasks;
				FaceSmoothingMasks.AddZeroed(TotalTriangles);
				if (GroupIds)
				{
					for (int32 TriIndex = 0; TriIndex < TotalTriangles; ++TriIndex)
					{
						FaceSmoothingMasks[TriIndex] = MeshData->SmoothingGroup.Get().Items[TriIndex].Value;
					}
				}
				FMeshDescriptionOperations::ConvertSmoothGroupToHardEdges(FaceSmoothingMasks, OutProxyMesh);
			}

			

			//since its a proxy will only contain one material on it
			ssf::ssfString ProxyMaterialGuid = Mesh->MaterialIds.Get().Items[0].Value;
			ssf::pssfMaterial ProxyMaterial = FSimplygonSSFHelper::FindMaterialById(SsfScene, ProxyMaterialGuid);
			if (ProxyMaterial != nullptr)
			{
				SetupMaterial(SsfScene, ProxyMaterial, OutMaterial, BaseTexturesPath);
			}
		}
	}

	/**
	* Extracts texture from a material channel's textures. Currently only returns one Samples
	* @param SsfMaterialChannel	SsfMaterialChannel pointer
	* @param BaseTexturesPath		Base folder path where textures are located
	* @param ChannelName			Channel name
	* @param OutSamples			Out Pixel samples from the texture
	* @param OutTextureSize		Out TextureSizes
	*/
	void ExtractTextureDescriptors(ssf::pssfScene SceneGraph, 
		ssf::pssfMaterialChannel SsfMaterialChannel,
		FString BaseTexturesPath,
		FString ChannelName,
		TArray<FColor>& OutSamples,
		FIntPoint& OutTextureSize)
	{
		for (ssf::pssfMaterialChannelTextureDescriptor TextureDescriptor : SsfMaterialChannel->MaterialChannelTextureDescriptorList)
		{
			ssf::pssfTexture Texture = FSimplygonSSFHelper::FindTextureById(SceneGraph, TextureDescriptor->TextureID.Get().Value);
			 
			if (Texture != nullptr)
			{
				FString TextureFilePath = FString::Printf(TEXT("%s/%s"), *BaseTexturesPath, ANSI_TO_TCHAR(Texture->Path.Get().Value.c_str()));
				CopyTextureData(OutSamples, OutTextureSize, ChannelName, TextureFilePath);
			}
		}
	}

	/**
	* Setup material will extract material information from SsfMaterial and create a flattened material from it.
	* @param InSsfScene			Base folder path where textures are located
	* @param InSsfMaterial			Channel name
	* @param OutMaterial			Out Pixel samples from the texture
	* @param InBaseTexturesPath	Out TextureSizes
	*/
	void SetupMaterial(ssf::pssfScene SceneGraph, ssf::pssfMaterial InSsfMaterial, FFlattenMaterial &OutMaterial, FString InBaseTexturesPath)
	{

		bool  bHasOpacityMask = false;
		bool  bHasOpacity = false;
		for (ssf::pssfMaterialChannel Channel : InSsfMaterial->MaterialChannelList)
		{
			const FString ChannelName(ANSI_TO_TCHAR(Channel->ChannelName.Get().Value.c_str()));

			if (ChannelName.Compare(BASECOLOR_CHANNEL) == 0)
			{
				FIntPoint Size = OutMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse);
				ExtractTextureDescriptors(SceneGraph, Channel, InBaseTexturesPath, ChannelName, OutMaterial.GetPropertySamples(EFlattenMaterialProperties::Diffuse), Size);
				OutMaterial.SetPropertySize(EFlattenMaterialProperties::Diffuse, Size);
			}
			else if (ChannelName.Compare(NORMAL_CHANNEL) == 0)
			{
				FIntPoint Size = OutMaterial.GetPropertySize(EFlattenMaterialProperties::Normal);
				ExtractTextureDescriptors(SceneGraph, Channel, InBaseTexturesPath, ChannelName, OutMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal), Size);
				OutMaterial.SetPropertySize(EFlattenMaterialProperties::Normal, Size);
			}
			else if (ChannelName.Compare(SPECULAR_CHANNEL) == 0)
			{
				FIntPoint Size = OutMaterial.GetPropertySize(EFlattenMaterialProperties::Specular);
				ExtractTextureDescriptors(SceneGraph, Channel, InBaseTexturesPath, ChannelName, OutMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular),	Size);
				OutMaterial.SetPropertySize(EFlattenMaterialProperties::Specular, Size);
			}
			else if (ChannelName.Compare(ROUGHNESS_CHANNEL) == 0)
			{
				FIntPoint Size = OutMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness);
				ExtractTextureDescriptors(SceneGraph, Channel, InBaseTexturesPath, ChannelName, OutMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness),Size);
				OutMaterial.SetPropertySize(EFlattenMaterialProperties::Roughness, Size);
			}
			else if (ChannelName.Compare(METALLIC_CHANNEL) == 0)
			{
				FIntPoint Size = OutMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic);
				ExtractTextureDescriptors(SceneGraph, Channel, InBaseTexturesPath, ChannelName, OutMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic), Size);
				OutMaterial.SetPropertySize(EFlattenMaterialProperties::Metallic, Size);
			}
			else if (ChannelName.Compare(OPACITY_CHANNEL) == 0)
			{
				FIntPoint Size = OutMaterial.GetPropertySize(EFlattenMaterialProperties::Opacity);
				ExtractTextureDescriptors(SceneGraph, Channel, InBaseTexturesPath, ChannelName, OutMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity), Size);
				bHasOpacity = true;
				OutMaterial.SetPropertySize(EFlattenMaterialProperties::Opacity, Size);
			}
			else if (ChannelName.Compare(OPACITY_MASK_CHANNEL) == 0)
			{
				FIntPoint Size = OutMaterial.GetPropertySize(EFlattenMaterialProperties::OpacityMask);
				ExtractTextureDescriptors(SceneGraph, Channel, InBaseTexturesPath, ChannelName, OutMaterial.GetPropertySamples(EFlattenMaterialProperties::OpacityMask), Size);
				bHasOpacityMask = true;
				OutMaterial.SetPropertySize(EFlattenMaterialProperties::OpacityMask, Size);
			}/**/
			else if (ChannelName.Compare(AO_CHANNEL) == 0)
			{
				FIntPoint Size = OutMaterial.GetPropertySize(EFlattenMaterialProperties::AmbientOcclusion);
				ExtractTextureDescriptors(SceneGraph, Channel, InBaseTexturesPath, ChannelName, OutMaterial.GetPropertySamples(EFlattenMaterialProperties::AmbientOcclusion), Size);
				OutMaterial.SetPropertySize(EFlattenMaterialProperties::AmbientOcclusion, Size);
			}
            else if (ChannelName.Compare(EMISSIVE_CHANNEL) == 0)
			{
				FIntPoint Size = OutMaterial.GetPropertySize(EFlattenMaterialProperties::Emissive);
				ExtractTextureDescriptors(SceneGraph, Channel, InBaseTexturesPath, ChannelName, OutMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive), Size);
				OutMaterial.SetPropertySize(EFlattenMaterialProperties::Emissive, Size);
			}  
		}

		if ( (bHasOpacity && bHasOpacityMask) || bHasOpacity)
		{
			OutMaterial.BlendMode = BLEND_Translucent;
		}
		else if (bHasOpacityMask)
		{
			OutMaterial.BlendMode = BLEND_Masked;
		}

		//NOTE: this feature is provided in the advance integration.
		//		Simplygon can bake both worldspace and tangentspace normal maps. 
		//		worldspace normal maps are better in certain cases. 
		//		We will move the functionality in a separate CL.

		//OutMaterial.bTangentspaceNormalmap = InSsfMaterial->TangentSpaceNormals->Value;
		}

	/**
	* Wrapper method which calls UAT with ZipUtils to unzip files.
	* @param ZipFileName			Path to zip file to be extracted.
	* @param OutputFolderPath		Path to folder where the contents of the zip will be extracted.
	*/
	bool UnzipDownloadedContent(FString ZipFileName, FString OutputFolderPath)
	{
		if (!FPaths::FileExists(FPaths::ConvertRelativePathToFull(ZipFileName)))
		{
			return false;
		}

		FString CmdExe = TEXT("cmd.exe");

		bool bEnableDebugging = GetDefault<UEditorPerProjectUserSettings>()->bEnableSwarmDebugging;
	
		FString CommandLine = FString::Printf(TEXT("ZipUtils -archive=\"%s\" -extract=\"%s\" -nocompile"), *ZipFileName, *OutputFolderPath);
		UatTask(CommandLine);

		return true;
	}

	/**
	* Wrapper method which call UAT with the ZipUtils to zip files.
	* @param InputDirectoryPath	Directory to zip
	* @param OutputFileName		Output zipfile path
	*/
bool ZipContentsForUpload(FString InputDirectoryPath, FString OutputFileName)
{
	bool bEnableDebugging = GetDefault<UEditorPerProjectUserSettings>()->bEnableSwarmDebugging;
	FString CmdExe = TEXT("cmd.exe");
	FString CommandLine = FString::Printf(TEXT("ZipUtils -archive=\"%s\" -add=\"%s\" -compression=0 -nocompile"), *FPaths::ConvertRelativePathToFull(OutputFileName), *FPaths::ConvertRelativePathToFull(InputDirectoryPath));
	UE_CLOG(bEnableDebugging, LogSimplygonSwarm, Log, TEXT("Uat command line %s"), *CommandLine);
	
	return UatTask(CommandLine);
}

	/**
	* Takes in a UAT Command and executes it. Is based on MainFrameAction CreateUatTask. A very minimalistic version.
	* @param CommandLine			Commandline argument to run against RunUAT.bat
	*/
	bool UatTask(FString CommandLine)
	{
#if PLATFORM_WINDOWS
		FString RunUATScriptName = TEXT("RunUAT.bat");
		FString CmdExe = TEXT("cmd.exe");
#elif PLATFORM_LINUX
		FString RunUATScriptName = TEXT("RunUAT.sh");
		FString CmdExe = TEXT("/bin/bash");
#else
		FString RunUATScriptName = TEXT("RunUAT.command");
		FString CmdExe = TEXT("/bin/sh");
#endif
		const bool bEnableDebugging = GetDefault<UEditorPerProjectUserSettings>()->bEnableSwarmDebugging;

		const FString UatPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles") / RunUATScriptName);

		if (!FPaths::FileExists(UatPath))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("File"), FText::FromString(UatPath));
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("RequiredFileNotFoundMessage", "A required file could not be found:\n{File}"), Arguments));

			return false;
		}

#if PLATFORM_WINDOWS
		FString FullCommandLine = FString::Printf(TEXT("/c \"\"%s\" %s\""), *UatPath, *CommandLine);
#else
		FString FullCommandLine = FString::Printf(TEXT("\"%s\" %s"), *UatPath, *CommandLine);
#endif
		while (FPlatformProcess::IsApplicationRunning(TEXT("AutomationTool.exe")))
		{
			static const float SleepTime = 0.5f;
			FPlatformProcess::Sleep(SleepTime);
			UE_CLOG(bEnableDebugging, LogSimplygonSwarm, Log, TEXT("UAT already running sleeping for %f seconds"), SleepTime);
		}

		TSharedPtr<FMonitoredProcess> UatProcess = MakeShareable(new FMonitoredProcess(CmdExe, FullCommandLine, true));
		UatProcess->SetSleepInterval(0.1f);

		// create notification item

		const bool bLaunched = UatProcess->Launch();

		UatProcess->OnOutput().BindLambda([&](FString Message) {UE_CLOG(bEnableDebugging, LogSimplygonSwarm, Log, TEXT("UatTask Output %s"), *Message); });

		while (UatProcess->Update()) {}

		return bLaunched;

	}

	/**
	* Get Unique Mateiral Inidices
	* @param OriginalMaterialIds		Original Material Indicies
	* @param ChannelUniqueMaterialIds	OutUniqueMaterialIds
	*/
	void GetUniqueMaterialIndices(const FMeshDescription& MeshDescription, TArray<int32>& UniqueMaterialIds)
	{
		int32 index = 0;
		for (const FPolygonGroupID PolygonGroupID : MeshDescription.PolygonGroups().GetElementIDs())
		{
			UniqueMaterialIds.AddUnique(index);
			index++;
		}
	}
	
	struct FSkeletalMeshData
	{
		TArray<SkeletalMeshImportData::FVertInfluence> Influences;
		TArray<SkeletalMeshImportData::FMeshWedge> Wedges;
		TArray<SkeletalMeshImportData::FMeshFace> Faces;
		TArray<FVector> Points;
		uint32 TexCoordCount;
	};

	/**
	* Method to setup a color caster spl object and attach it to the given process node.
	* @param InSplProcessNode		SplProcessNode to attach the caster to.
	* @param Channel				Channel name to cast (i.e Basecolor, Specular, Roughness)
	*/
	void SetupColorCaster(SPL::ProcessNode& InSplProcessNode, FString Channel)
	{
		SPL::ColorCaster* colorCaster = new SPL::ColorCaster();
		colorCaster->Dilation = 10;
		colorCaster->OutputChannels = 4;
		colorCaster->OutputSRGB = false;
		colorCaster->FillMode = SPL::FillMode::SG_ATLASFILLMODE_INTERPOLATE;
		colorCaster->ColorType = TCHAR_TO_ANSI(*Channel);
		colorCaster->Name = TCHAR_TO_ANSI(*Channel);
		colorCaster->Channel = TCHAR_TO_ANSI(*Channel);
		colorCaster->DitherType = SPL::DitherType::SG_DITHERPATTERNS_FLOYDSTEINBERG;
		//for spl we need to expliclity set the enabled flag.
		colorCaster->Enabled = true;

		InSplProcessNode.MaterialCaster.push_back(colorCaster);
	}

	/**
	* Method to setup a normal caster spl object and attach it to the given process node.
	* Note : You can use this method to define custom normal channels as well.
	* @param InSplProcessNode		SplProcessNode to attach the caster to.
	* @param Channel				Channel name to cast (i.e Normal)
	* @param bTangentspaceNormals	Channel name to cast (i.e Normal)
	*/
	void SetupNormalCaster(SPL::ProcessNode& InSplProcessNode, FString Channel, bool bTangentspaceNormals = true)
	{
		SPL::NormalCaster* normalCaster = new SPL::NormalCaster();
		normalCaster->Name = TCHAR_TO_ANSI(*Channel);
		normalCaster->Channel = TCHAR_TO_ANSI(*Channel);
		normalCaster->GenerateTangentSpaceNormals = bTangentspaceNormals;
		normalCaster->OutputChannels = 3;
		normalCaster->Dilation = 10;
		normalCaster->FlipGreen = false;
		normalCaster->FillMode = SPL::FillMode::SG_ATLASFILLMODE_NEARESTNEIGHBOR;
		normalCaster->DitherType = SPL::DitherType::SG_DITHERPATTERNS_NO_DITHER;
		normalCaster->Enabled = true;

		InSplProcessNode.MaterialCaster.push_back(normalCaster);
	}

	/**
	* Method to setup a normal caster spl object and attach it to the given process node.
	* Note : You can use this method to define custom normal channels as well.
	* @param InSplProcessNode		SplProcessNode to attach the caster to.
	* @param Channel				Channel name to cast (i.e Normal)
	* @param bTangentspaceNormals	Channel name to cast (i.e Normal)
	*/
	void SetupOpacityCaster(SPL::ProcessNode& InSplProcessNode, FString Channel)
	{
		SPL::OpacityCaster* opacityCaster = new SPL::OpacityCaster();
		opacityCaster->Dilation = 10;
		opacityCaster->OutputChannels = 4;
		opacityCaster->FillMode = SPL::FillMode::SG_ATLASFILLMODE_INTERPOLATE;
		opacityCaster->ColorType = TCHAR_TO_ANSI(*Channel);
		opacityCaster->Name = TCHAR_TO_ANSI(*Channel);
		opacityCaster->Channel = TCHAR_TO_ANSI(*Channel);
		opacityCaster->DitherType = SPL::DitherType::SG_DITHERPATTERNS_FLOYDSTEINBERG;

		//for spl we need to expliclity set the enabled flag.
		opacityCaster->Enabled = true;

		InSplProcessNode.MaterialCaster.push_back(opacityCaster);
	}

	/**
	* Setup Material caster for a spl process node
	* @param InMaterialProxySettings	Material proxy settings
	* @param InSplProcessNode			SplProcess node to attach casters to
	* @param InOutputMaterialBlendMode	EBlendMode (Opaque, Translucent, Masked) are supported
	* @returns The calculated view distance
	*/
	void SetupSplMaterialCasters(const FMaterialProxySettings& InMaterialProxySettings, SPL::ProcessNode& InSplProcessNode, EBlendMode InOutputMaterialBlendMode = BLEND_Opaque)
	{
		SetupColorCaster(InSplProcessNode, BASECOLOR_CHANNEL);

		if (InMaterialProxySettings.bRoughnessMap)
		{
			SetupColorCaster(InSplProcessNode, ROUGHNESS_CHANNEL);
		}
		if (InMaterialProxySettings.bSpecularMap)
		{
			SetupColorCaster(InSplProcessNode, SPECULAR_CHANNEL);
		}
		if (InMaterialProxySettings.bMetallicMap)
		{
			SetupColorCaster(InSplProcessNode, METALLIC_CHANNEL);
		}

		if (InMaterialProxySettings.bNormalMap)
		{
			SetupNormalCaster(InSplProcessNode, NORMAL_CHANNEL, true/*InMaterialProxySettings.bUseTangentSpace*/);
		}

		if (InMaterialProxySettings.bOpacityMap)
		{
			SetupOpacityCaster(InSplProcessNode, OPACITY_CHANNEL);
		}
		else if (InMaterialProxySettings.bOpacityMaskMap)
		{
			SetupColorCaster(InSplProcessNode, OPACITY_MASK_CHANNEL);
		}

      //NOTE: Enable this block once AO feature is moved into vanilla integration.
		if (InMaterialProxySettings.bAmbientOcclusionMap)
		{
			SetupColorCaster(InSplProcessNode, AO_CHANNEL);
		}

		if (InMaterialProxySettings.bEmissiveMap)
		{
			SetupColorCaster(InSplProcessNode, EMISSIVE_CHANNEL);
        }
	}

	/**
	* Calculates the view distance that a mesh should be displayed at.
	* @param MaxDeviation - The maximum surface-deviation between the reduced geometry and the original. This value should be acquired from Simplygon
	* @returns The calculated view distance	 
	*/
	float CalculateViewDistance( float MaxDeviation )
	{
		// We want to solve for the depth in world space given the screen space distance between two pixels
		//
		// Assumptions:
		//   1. There is no scaling in the view matrix.
		//   2. The horizontal FOV is 90 degrees.
		//   3. The backbuffer is 1920x1080.
		//
		// If we project two points at (X,Y,Z) and (X',Y,Z) from view space, we get their screen
		// space positions: (X/Z, Y'/Z) and (X'/Z, Y'/Z) where Y' = Y * AspectRatio.
		//
		// The distance in screen space is then sqrt( (X'-X)^2/Z^2 + (Y'-Y')^2/Z^2 )
		// or (X'-X)/Z. This is in clip space, so PixelDist = 1280 * 0.5 * (X'-X)/Z.
		//
		// Solving for Z: ViewDist = (X'-X * 640) / PixelDist

		const float ViewDistance = (MaxDeviation * 960.0f);
		return ViewDistance;
	}

	/**
	* Compute mapping image size from the given material proxy settings
	* @param Settings		Material Proxy Settings
	*/
	static FIntPoint ComputeMappingImageSize(const FMaterialProxySettings& Settings)
	{
		FIntPoint ImageSize = Settings.TextureSize;

		return ImageSize;
	}

	/**
	* Method to swap axis
	*	(1,0,0)
	*	(0,0,1)
	*	(0,1,0)
	*/
	const FMatrix& GetConversionMatrixYUP()
	{

		static FMatrix m;
		static bool bInitialized = false;
		if (!bInitialized)
		{
			m.SetIdentity();
			 
			bInitialized = true;
		}
		return m;
	}

	/**
	* Method to create a SsfMeshData from FMeshDescription
	* @param InRawMesh				Rawmesh to create SsfMeshData from
	* @param InTextureBounds		Texture bounds
	* @param InTexCoords			Corrected texture coordinates generated after material flattening.
	*/
	ssf::pssfMeshData CreateSSFMeshDataFromRawMesh(const FMeshDescription& SrcRawMesh, TArray<FBox2D> InTextureBounds, TArray<FVector2D> InTexCoords)
	{
		TVertexAttributesConstRef<FVector> VertexPositions = SrcRawMesh.VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
		TVertexInstanceAttributesConstRef<FVector> VertexInstanceNormals = SrcRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
		TVertexInstanceAttributesConstRef<FVector> VertexInstanceTangents = SrcRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
		TVertexInstanceAttributesConstRef<float> VertexInstanceBinormalSigns = SrcRawMesh.VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
		TVertexInstanceAttributesConstRef<FVector4> VertexInstanceColors = SrcRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
		TVertexInstanceAttributesConstRef<FVector2D> VertexInstanceUVs = SrcRawMesh.VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

		int32 NumVertices = SrcRawMesh.Vertices().Num();
		int32 NumWedges = 0;
		for (const FPolygonID& PolygonID : SrcRawMesh.Polygons().GetElementIDs())
		{
			const FMeshPolygon& Polygon = SrcRawMesh.GetPolygon(PolygonID);
			NumWedges += Polygon.Triangles.Num() * 3;
		}
		int32 NumTris = NumWedges / 3;

		if (NumWedges == 0)
		{
			return nullptr;
		}

		//assuming everything is left-handed so no need to change winding order and handedness. SSF supports both

		ssf::pssfMeshData SgMeshData = new ssf::ssfMeshData();
		 
		//setup vertex coordinates
		ssf::ssfList<ssf::ssfVector3> & SsfCoorinates = SgMeshData->Coordinates.Create();
		SsfCoorinates.Items.resize(NumVertices);
		TMap<FVertexID, int32> MeshToSsfVertexID;
		MeshToSsfVertexID.Reserve(NumVertices);
		int32 VertexIndex = 0;
		for(const FVertexID& VertexID : SrcRawMesh.Vertices().GetElementIDs())
		{
			ssf::ssfVector3 CurrentVertex;
			FVector4 Position = GetConversionMatrixYUP().TransformPosition(VertexPositions[VertexID]);
			CurrentVertex.V[0] = double(Position.X);
			CurrentVertex.V[1] = double(Position.Y);
			CurrentVertex.V[2] = double(Position.Z);
			SsfCoorinates.Items[VertexIndex] = CurrentVertex;
			MeshToSsfVertexID.Add(VertexID, VertexIndex);
			VertexIndex++;
		}

		//setup triangle data
		ssf::ssfList<ssf::ssfIndex3>& SsfTriangleIndices = SgMeshData->TriangleIndices.Create();
		ssf::ssfList<ssf::ssfUInt32>& SsfMaterialIndices = SgMeshData->MaterialIndices.Create();
		ssf::ssfList<ssf::ssfInt32>& SsfSmoothingGroups = SgMeshData->SmoothingGroup.Create();

		SsfTriangleIndices.Items.resize(NumTris);
		SsfMaterialIndices.Items.resize(NumTris);
		SsfSmoothingGroups.Items.resize(NumTris);

		bool bHasNormals = VertexInstanceNormals.GetNumElements() == NumWedges;
		bool bHasTangents = bHasNormals && (VertexInstanceTangents.GetNumElements() == NumWedges) && (VertexInstanceBinormalSigns.GetNumElements() == NumWedges);
		
		ssf::ssfList<ssf::ssfVector3> EmptyList;
		ssf::ssfList<ssf::ssfVector3>& SsfTangents = bHasTangents ? SgMeshData->Tangents.Create() : EmptyList;
		ssf::ssfList<ssf::ssfVector3>& SsfBitangents = bHasTangents ? SgMeshData->Bitangents.Create() : EmptyList;
		ssf::ssfList<ssf::ssfVector3>& SsfNormals = bHasNormals ? SgMeshData->Normals.Create() : EmptyList;
		if (bHasNormals)
		{
			if (bHasTangents)
			{
				SsfTangents.Items.resize(NumWedges);
				SsfBitangents.Items.resize(NumWedges);
			}
			SsfNormals.Items.resize(NumWedges);
		}

		const int32 TexCoordNumber = FMath::Min(VertexInstanceUVs.GetNumIndices(), (int32)MAX_MESH_TEXTURE_COORDS);
		ssf::ssfNamedList<ssf::ssfVector2> SsfTextureCoordinates[MAX_MESH_TEXTURE_COORDS];
		for (int32 TexCoordIndex = 0; TexCoordIndex < TexCoordNumber; ++TexCoordIndex)
		{
			//Since SSF uses Named Channels
			SsfTextureCoordinates[TexCoordIndex].Name = FSimplygonSSFHelper::TCHARToSSFString(*FString::Printf(TEXT("TexCoord%d"), TexCoordIndex));
			SsfTextureCoordinates[TexCoordIndex].Items.resize(NumWedges);
		}

		ssf::ssfNamedList<ssf::ssfVector4> SsfColorMap;
		bool bHasVertexColor = VertexInstanceColors.GetNumElements() == NumWedges;
		if (bHasVertexColor)
		{
			//setup the color named channel . Currently its se to index zero. If multiple colors channel are need then use an index instead of 0
			SsfColorMap.Name = FSimplygonSSFHelper::TCHARToSSFString(*FString::Printf(TEXT("Colors%d"), 0));
			SsfColorMap.Items.resize(NumWedges);
		}

		//Smooth group
		TArray<uint32> FaceSmoothingMasks;
		FMeshDescriptionOperations::ConvertHardEdgesToSmoothGroup(SrcRawMesh, FaceSmoothingMasks);

		//Reverse winding switches
		bool bReverseWinding = true;
		
		int32 TriangleIndex = 0;
		for (const FPolygonID& PolygonID : SrcRawMesh.Polygons().GetElementIDs())
		{
			const FMeshPolygon& Polygon = SrcRawMesh.GetPolygon(PolygonID);
			
			FPolygonGroupID PolygonGroupID = SrcRawMesh.GetPolygonPolygonGroup(PolygonID);
			int32 MaterialIndex = PolygonGroupID.GetValue();

			for (const FMeshTriangle& Triangle : Polygon.Triangles)
			{
				for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
				{
					int32 DestCornerIndex = bReverseWinding ? 2 - CornerIndex : CornerIndex;
					FVertexInstanceID VertexInstanceID = Triangle.GetVertexInstanceID(CornerIndex);
					SsfTriangleIndices.Items[TriangleIndex].V[DestCornerIndex] = MeshToSsfVertexID[SrcRawMesh.GetVertexInstanceVertex(VertexInstanceID)];
					
					//NTBs
					if (bHasNormals)
					{
						FVector Normal = VertexInstanceNormals[VertexInstanceID];
						if (bHasTangents)
						{
							FVector Tangent = VertexInstanceTangents[VertexInstanceID];
							FVector Bitangent = FVector::CrossProduct(Normal, Tangent).GetSafeNormal() * VertexInstanceBinormalSigns[VertexInstanceID];

							ssf::ssfVector3 SsfTangent;
							FVector4 Tangent4 = GetConversionMatrixYUP().TransformPosition(Tangent);
							SsfTangent.V[0] = double(Tangent4.X);
							SsfTangent.V[1] = double(Tangent4.Y);
							SsfTangent.V[2] = double(Tangent4.Z);
							SsfTangents.Items[TriangleIndex * 3 + DestCornerIndex] = SsfTangent;

							ssf::ssfVector3 SsfBitangent;
							FVector4 Bitangent4 = GetConversionMatrixYUP().TransformPosition(Bitangent);
							SsfBitangent.V[0] = double(Bitangent4.X);
							SsfBitangent.V[1] = double(Bitangent4.Y);
							SsfBitangent.V[2] = double(Bitangent4.Z);
							SsfBitangents.Items[TriangleIndex * 3 + DestCornerIndex] = SsfBitangent;
						}

						ssf::ssfVector3 SsfNormal;
						FVector4 Normal4 = GetConversionMatrixYUP().TransformPosition(Normal);
						SsfNormal.V[0] = double(Normal4.X);
						SsfNormal.V[1] = double(Normal4.Y);
						SsfNormal.V[2] = double(Normal4.Z);
						SsfNormals.Items[TriangleIndex * 3 + DestCornerIndex] = SsfNormal;
					}

					//Vertex color
					if (bHasVertexColor)
					{
						FLinearColor LinearColor = VertexInstanceColors[VertexInstanceID];
						SsfColorMap.Items[TriangleIndex * 3 + DestCornerIndex].V[0] = LinearColor.R;
						SsfColorMap.Items[TriangleIndex * 3 + DestCornerIndex].V[1] = LinearColor.G;
						SsfColorMap.Items[TriangleIndex * 3 + DestCornerIndex].V[2] = LinearColor.B;
						SsfColorMap.Items[TriangleIndex * 3 + DestCornerIndex].V[3] = LinearColor.A;
					}
					

					//Texcoords
					for (int32 TexCoordIndex = 0; TexCoordIndex < TexCoordNumber; ++TexCoordIndex)
					{
						bool bUseInTexCoord = (TexCoordIndex == 0 && InTexCoords.Num() == NumWedges);
						int32 NumTexCoord = bUseInTexCoord ? InTexCoords.Num() : VertexInstanceUVs.GetNumElements();
						if (NumTexCoord == NumWedges)
						{
							// Compute texture bounds for current material.
							float MinU = 0, ScaleU = 1;
							float MinV = 0, ScaleV = 1;
							if (InTextureBounds.IsValidIndex(MaterialIndex) && TexCoordIndex == 0 && InTexCoords.Num() == 0)
							{
								const FBox2D& Bounds = InTextureBounds[MaterialIndex];
								if (Bounds.GetArea() > 0)
								{
									MinU = Bounds.Min.X;
									MinV = Bounds.Min.Y;
									ScaleU = 1.0f / (Bounds.Max.X - Bounds.Min.X);
									ScaleV = 1.0f / (Bounds.Max.Y - Bounds.Min.Y);
								}
							}

							const FVector2D& TexCoord = bUseInTexCoord ? InTexCoords[TriangleIndex * 3 + CornerIndex] : VertexInstanceUVs.Get(VertexInstanceID, TexCoordIndex);
							ssf::ssfVector2 temp;
							temp.V[0] = (TexCoord.X - MinU) * ScaleU;
							temp.V[1] = (TexCoord.Y - MinV) * ScaleV;
							SsfTextureCoordinates[TexCoordIndex].Items[TriangleIndex * 3 + DestCornerIndex] = temp;
						}
					}

				}
				
				//Material
				SsfMaterialIndices.Items[TriangleIndex] = MaterialIndex;

				//Smooth group
				SsfSmoothingGroups.Items[TriangleIndex] = FaceSmoothingMasks[TriangleIndex];

				TriangleIndex++;
			}
		}

		SgMeshData->MaterialIndices.Create();

		//Push back all the data...

		for (int32 TexCoordIndex = 0; TexCoordIndex < TexCoordNumber; ++TexCoordIndex)
		{
			SgMeshData->TextureCoordinatesList.push_back(SsfTextureCoordinates[TexCoordIndex]);
		}
		if (bHasVertexColor)
		{
			SgMeshData->ColorsList.push_back(SsfColorMap);
		}


		return SgMeshData;
	}

	/**
	* Method to copy texture's pixel data into a FColor array
	* @param OutSamples			Out TArray where texture data is copied to.
	* @param OutTextureSize		Out Texture sizes
	* @param TexturePath			Path to Texture
	* @param IsNormalMap			Is this normalmap that we are reading
	*/
	void CopyTextureData(
		TArray<FColor>& OutSamples,
		FIntPoint& OutTextureSize,
		FString ChannelName,
		FString TexturePath,
		bool IsNormalMap = false
		)
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::GetModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		 
		TArray<uint8> TextureData;
		if (!FFileHelper::LoadFileToArray(TextureData, *FPaths::ConvertRelativePathToFull(TexturePath)) && TextureData.Num() > 0)
		{
			UE_LOG(LogSimplygonSwarm, Warning, TEXT("Unable to find Texture file %s"), *TexturePath);
		}
		else
		{
			const TArray<uint8>* RawData = NULL;
			 
			if (ImageWrapper->SetCompressed(TextureData.GetData(), TextureData.Num()) && ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
			{
				OutTextureSize.X = ImageWrapper->GetHeight();
				OutTextureSize.Y = ImageWrapper->GetWidth();
				int32 TexelsCount = ImageWrapper->GetHeight() * ImageWrapper->GetWidth();
				OutSamples.Empty(TexelsCount);
				OutSamples.AddUninitialized(TexelsCount);

				for (int32 X = 0; X < ImageWrapper->GetHeight(); ++X)
				{
					for (int32 Y = 0; Y < ImageWrapper->GetWidth(); ++Y)
					{
						int32 PixelIndex = ImageWrapper->GetHeight() * X + Y;

						OutSamples[PixelIndex].B = (*RawData)[PixelIndex*sizeof(FColor) + 0];
						OutSamples[PixelIndex].G = (*RawData)[PixelIndex*sizeof(FColor) + 1];
						OutSamples[PixelIndex].R = (*RawData)[PixelIndex*sizeof(FColor) + 2];
						OutSamples[PixelIndex].A = (*RawData)[PixelIndex*sizeof(FColor) + 3];
					}
				}				 
			}

		}
	}

	/**
	* Method to create a SsfMaterialChannel object
	* @param InSamples				Color data to output to texture.
	* @param InTextureSize			Texture size
	* @param SsfTextureTable		SsfTexture Table
	* @param TextureName			Texture name
	* @param BaseTexturePath		Texture base folder to use
	* @param IsSRGB				Texture is SRGB based or not.
	*/
	ssf::pssfMaterialChannel CreateSsfMaterialChannel(
		const TArray<FColor>& InSamples,
		FIntPoint InTextureSize,
		ssf::pssfTextureTable SsfTextureTable,
		FString ChannelName, FString TextureName, FString BaseTexturePath, bool IsSRGB = true)
	{

		ssf::pssfMaterialChannel SsfMaterialChannel = new ssf::ssfMaterialChannel();
		SsfMaterialChannel->ChannelName.Set(FSimplygonSSFHelper::TCHARToSSFString(*ChannelName));

		bool bDebuggingEnabled = GetDefault<UEditorPerProjectUserSettings>()->bEnableSwarmDebugging;

		if (InSamples.Num() >= 1)
		{

			IImageWrapperModule& ImageWrapperModule = FModuleManager::GetModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
			TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
			
			FString TextureOutputRelative = FString::Printf(TEXT("%s/%s.png"), ANSI_TO_TCHAR(SsfTextureTable->TexturesDirectory->Value.c_str()), *TextureName);
			FString TextureOutputPath = FString::Printf(TEXT("%s%s"), *BaseTexturePath, *TextureOutputRelative);
			 
			if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(&InSamples[0], InSamples.Num() * sizeof(FColor), InTextureSize.X, InTextureSize.Y, ERGBFormat::BGRA, 8))
			{
				if (FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(), *TextureOutputPath))
				{
					ssf::pssfTexture SsfTexture = new ssf::ssfTexture();
					ssf::pssfMaterialChannelTextureDescriptor SsfTextureDescriptor = new ssf::ssfMaterialChannelTextureDescriptor();
					SsfTexture->Id.Set(FSimplygonSSFHelper::SSFNewGuid());
					SsfTexture->Name.Set(FSimplygonSSFHelper::TCHARToSSFString(*TextureName));
					SsfTexture->Path.Set(FSimplygonSSFHelper::TCHARToSSFString(*TextureOutputRelative));
					SsfTextureDescriptor->TextureID.Set(SsfTexture->Id.Get());

					FString TexCoordText = TEXT("TexCoord0");
					SsfTextureDescriptor->TexCoordSet.Set(FSimplygonSSFHelper::TCHARToSSFString(*TexCoordText));

					SsfMaterialChannel->MaterialChannelTextureDescriptorList.push_back(SsfTextureDescriptor);
					FString ShadingNetwork = FString::Printf(SHADING_NETWORK_TEMPLATE, *TextureName, *TexCoordText, 0);
					SsfMaterialChannel->ShadingNetwork.Set(FSimplygonSSFHelper::TCHARToSSFString(*ShadingNetwork));
					SsfTextureTable->TextureList.push_back(SsfTexture);
				}
				else
				{
					UE_LOG(LogSimplygonSwarm, Error, TEXT("Could not save to file %s"), *TextureOutputPath);
				}
				 
			}

		}
		else
		{
			SsfMaterialChannel->Color.Create();
			SsfMaterialChannel->Color->V[0] = 1.0f;
			SsfMaterialChannel->Color->V[1] = 1.0f;
			SsfMaterialChannel->Color->V[2] = 1.0f;
			SsfMaterialChannel->Color->V[3] = 1.0f;
		}

		return SsfMaterialChannel;
	}

	/**
	* Method to create a SsfMaterialChannel object
	* @param InputMaterials			List of flatten materials.
	* @param InMaterialLODSettings		Material Proxy Settings
	* @param SsfMaterialTable			Material Table
	* @param SsfTextureTable			Texture Table
	* @param BaseTexturePath			Base Texture Path
	* @param bReleaseInputMaterials	Wether or not release Flatten Material you are done.
	* @param OutMaterialMapping		Id to Guid mapping.
	*/
	bool CreateSSFMaterialFromFlattenMaterial(
		const TArray<FFlattenMaterial>& InputMaterials,
		const FMaterialProxySettings& InMaterialLODSettings,
		ssf::pssfMaterialTable SsfMaterialTable,
		ssf::pssfTextureTable SsfTextureTable,
		FString BaseTexturePath,
		bool bReleaseInputMaterials, TMap<int, FString>& OutMaterialMapping)
{
		if (InputMaterials.Num() == 0)
		{
		//If there are no materials, feed Simplygon with a default material instead.
		UE_LOG(LogSimplygonSwarm, Log, TEXT("Input meshes do not contain any materials. A proxy without material will be generated."));
		return false;
		}
		 
		bool bFillEmptyEmissive = false;
		bool bDiscardEmissive = true;
		for (int32 MaterialIndex = 0; MaterialIndex < InputMaterials.Num(); MaterialIndex++)
		{
			const FFlattenMaterial& FlattenMaterial = InputMaterials[MaterialIndex];
			if (FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive).Num() > 1 || (FlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Emissive) && FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive)[0] != FColor::Black))
			{
				bFillEmptyEmissive = true;
			}

			bDiscardEmissive &= ((FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Emissive)) || (FlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Emissive) && FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive)[0] == FColor::Black));
		} 
		 
		for (int32 MaterialIndex = 0; MaterialIndex < InputMaterials.Num(); MaterialIndex++)
		{
			FString MaterialGuidString = FGuid::NewGuid().ToString();
			const FFlattenMaterial& FlattenMaterial = InputMaterials[MaterialIndex];
			FString MaterialName = FString::Printf(TEXT("Material%d"), MaterialIndex);

			ssf::pssfMaterial SsfMaterial = new ssf::ssfMaterial();
			SsfMaterial->Id.Set(FSimplygonSSFHelper::TCHARToSSFString(*MaterialGuidString));
			SsfMaterial->Name.Set(FSimplygonSSFHelper::TCHARToSSFString(*MaterialName));

			OutMaterialMapping.Add(MaterialIndex, MaterialGuidString);

			// Does current material have BaseColor?
			if (FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Diffuse))
			{
				FString ChannelName(BASECOLOR_CHANNEL);
				ssf::pssfMaterialChannel BaseColorChannel = CreateSsfMaterialChannel(FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Diffuse), FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse), SsfTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), BaseTexturePath);

				SsfMaterial->MaterialChannelList.push_back(BaseColorChannel);

				//NOTE: use the commented setting once switching between tangentspace/worldspace is added into the vanilla version of the engine.
				SsfMaterial->TangentSpaceNormals->Create(true /*InMaterialLODSettings.bUseTangentSpace*/);
			}

			// Does current material have Metallic?
			if (FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Metallic))
			{
				FString ChannelName(METALLIC_CHANNEL);
				ssf::pssfMaterialChannel MetallicChannel = CreateSsfMaterialChannel(FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Metallic), FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Metallic), SsfTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), BaseTexturePath);
				SsfMaterial->MaterialChannelList.push_back(MetallicChannel);
			}

			// Does current material have Specular?
			if (FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Specular))
			{
				FString ChannelName(SPECULAR_CHANNEL);
				ssf::pssfMaterialChannel SpecularChannel = CreateSsfMaterialChannel(FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Specular), FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Specular), SsfTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), BaseTexturePath);
				SsfMaterial->MaterialChannelList.push_back(SpecularChannel);
			}

			// Does current material have Roughness?
			if (FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Roughness))
			{
				FString ChannelName(ROUGHNESS_CHANNEL);
				ssf::pssfMaterialChannel RoughnessChannel = CreateSsfMaterialChannel(FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Roughness), FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Roughness), SsfTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), BaseTexturePath);
				SsfMaterial->MaterialChannelList.push_back(RoughnessChannel);
			}

			//Does current material have a normalmap?
			if (FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Normal))
			{
				FString ChannelName(NORMAL_CHANNEL);
				SsfMaterial->TangentSpaceNormals.Create();
				SsfMaterial->TangentSpaceNormals.Set(true);
				ssf::pssfMaterialChannel NormalChannel = CreateSsfMaterialChannel(FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Normal), FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Normal), SsfTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), BaseTexturePath, false);
				SsfMaterial->MaterialChannelList.push_back(NormalChannel);
			}

			// Does current material have Opacity?
			if (FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Opacity))
			{
				FString ChannelName(OPACITY_CHANNEL);
				ssf::pssfMaterialChannel OpacityChannel = CreateSsfMaterialChannel(FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Opacity), FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Opacity), SsfTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), BaseTexturePath);
				SsfMaterial->MaterialChannelList.push_back(OpacityChannel);
			}

			if (FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::OpacityMask))
			{
				FString ChannelName(OPACITY_MASK_CHANNEL);
				ssf::pssfMaterialChannel OpacityChannel = CreateSsfMaterialChannel(FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::OpacityMask), FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::OpacityMask), SsfTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), BaseTexturePath);
				SsfMaterial->MaterialChannelList.push_back(OpacityChannel);
			}

			// Emissive could have been outputted by the shader/swarm due to various reasons, however we don't always need the data that was created so we discard it
			if (FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Emissive) || (FlattenMaterial.IsPropertyConstant(EFlattenMaterialProperties::Emissive) && FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive)[0] == FColor::Black))
			{
				FString ChannelName(EMISSIVE_CHANNEL);
				ssf::pssfMaterialChannel EmissiveChannel = CreateSsfMaterialChannel(FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::Emissive), FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::Emissive), SsfTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), BaseTexturePath);
				SsfMaterial->MaterialChannelList.push_back(EmissiveChannel);
			}
			else if (bFillEmptyEmissive && !FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::Emissive))
			{
				TArray<FColor> Sample;
				Sample.Add(FColor::Black);
				FIntPoint Size(1, 1);
				FString ChannelName(EMISSIVE_CHANNEL);
				TArray<FColor> BlackEmissive;
				BlackEmissive.AddZeroed(1);
				ssf::pssfMaterialChannel EmissiveChannel = CreateSsfMaterialChannel(Sample, Size, SsfTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), BaseTexturePath);
				SsfMaterial->MaterialChannelList.push_back(EmissiveChannel);
			}

			//NOTE: Enable this once AO baking functionality is moved into the engine. 
			if (FlattenMaterial.DoesPropertyContainData(EFlattenMaterialProperties::AmbientOcclusion))
			{
				FString ChannelName(AO_CHANNEL);
				ssf::pssfMaterialChannel AOChannel = CreateSsfMaterialChannel(FlattenMaterial.GetPropertySamples(EFlattenMaterialProperties::AmbientOcclusion), FlattenMaterial.GetPropertySize(EFlattenMaterialProperties::AmbientOcclusion), SsfTextureTable, ChannelName, FString::Printf(TEXT("%s%s"), *MaterialName, *ChannelName), BaseTexturePath);
				SsfMaterial->MaterialChannelList.push_back(AOChannel);
			}

			SsfMaterialTable->MaterialList.push_back(SsfMaterial);

			if (bReleaseInputMaterials)
			{
				// Release FlattenMaterial. Using const_cast here to avoid removal of "const" from input data here
				// and above the call chain.
				const_cast<FFlattenMaterial*>(&FlattenMaterial)->ReleaseData();
			}
		}

	return true;
	}
};

TUniquePtr<FSimplygonSwarm> GSimplygonMeshReduction;


void FSimplygonSwarmModule::StartupModule()
{
	GSimplygonMeshReduction.Reset(FSimplygonSwarm::Create());
	FModuleManager::Get().LoadModule(FName("ImageWrapper"));
	IModularFeatures::Get().RegisterModularFeature(IMeshReductionModule::GetModularFeatureName(), this);
}

void FSimplygonSwarmModule::ShutdownModule()
{
	FSimplygonRESTClient::Shutdown();
	IModularFeatures::Get().UnregisterModularFeature(IMeshReductionModule::GetModularFeatureName(), this);
}

IMeshReduction* FSimplygonSwarmModule::GetStaticMeshReductionInterface()
{
	return nullptr;
}

IMeshReduction* FSimplygonSwarmModule::GetSkeletalMeshReductionInterface()
{
	return nullptr;
}

IMeshMerging* FSimplygonSwarmModule::GetMeshMergingInterface()
{
	return nullptr;
}

class IMeshMerging* FSimplygonSwarmModule::GetDistributedMeshMergingInterface()
{
	return GSimplygonMeshReduction.Get();
}

FString FSimplygonSwarmModule::GetName()
{
	return FString("SimplygonSwarm");
}

#undef LOCTEXT_NAMESPACE
