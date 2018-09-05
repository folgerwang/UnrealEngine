// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NewLandscapeUtils.h"

#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEditorObject.h"
#include "LandscapeEditorModule.h"
#include "LandscapeEditorUtils.h"
#include "LandscapeEdMode.h"

#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Templates/UnrealTemplate.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.NewLandscape"

const int32 FNewLandscapeUtils::SectionSizes[6] = { 7, 15, 31, 63, 127, 255 };
const int32 FNewLandscapeUtils::NumSections[2] = { 1, 2 };

void FNewLandscapeUtils::ChooseBestComponentSizeForImport(ULandscapeEditorObject* UISettings)
{
	int32 Width = UISettings->ImportLandscape_Width;
	int32 Height = UISettings->ImportLandscape_Height;

	bool bFoundMatch = false;
	if(Width > 0 && Height > 0)
	{
		// Try to find a section size and number of sections that exactly matches the dimensions of the heightfield
		for(int32 SectionSizesIdx = ARRAY_COUNT(SectionSizes) - 1; SectionSizesIdx >= 0; SectionSizesIdx--)
		{
			for(int32 NumSectionsIdx = ARRAY_COUNT(NumSections) - 1; NumSectionsIdx >= 0; NumSectionsIdx--)
			{
				int32 ss = SectionSizes[SectionSizesIdx];
				int32 ns = NumSections[NumSectionsIdx];

				if(((Width - 1) % (ss * ns)) == 0 && ((Width - 1) / (ss * ns)) <= 32 &&
					((Height - 1) % (ss * ns)) == 0 && ((Height - 1) / (ss * ns)) <= 32)
				{
					bFoundMatch = true;
					UISettings->NewLandscape_QuadsPerSection = ss;
					UISettings->NewLandscape_SectionsPerComponent = ns;
					UISettings->NewLandscape_ComponentCount.X = (Width - 1) / (ss * ns);
					UISettings->NewLandscape_ComponentCount.Y = (Height - 1) / (ss * ns);
					UISettings->NewLandscape_ClampSize();
					break;
				}
			}
			if(bFoundMatch)
			{
				break;
			}
		}

		if(!bFoundMatch)
		{
			// if there was no exact match, try increasing the section size until we encompass the whole heightmap
			const int32 CurrentSectionSize = UISettings->NewLandscape_QuadsPerSection;
			const int32 CurrentNumSections = UISettings->NewLandscape_SectionsPerComponent;
			for(int32 SectionSizesIdx = 0; SectionSizesIdx < ARRAY_COUNT(SectionSizes); SectionSizesIdx++)
			{
				if(SectionSizes[SectionSizesIdx] < CurrentSectionSize)
				{
					continue;
				}

				const int32 ComponentsX = FMath::DivideAndRoundUp((Width - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
				const int32 ComponentsY = FMath::DivideAndRoundUp((Height - 1), SectionSizes[SectionSizesIdx] * CurrentNumSections);
				if(ComponentsX <= 32 && ComponentsY <= 32)
				{
					bFoundMatch = true;
					UISettings->NewLandscape_QuadsPerSection = SectionSizes[SectionSizesIdx];
					//UISettings->NewLandscape_SectionsPerComponent = ;
					UISettings->NewLandscape_ComponentCount.X = ComponentsX;
					UISettings->NewLandscape_ComponentCount.Y = ComponentsY;
					UISettings->NewLandscape_ClampSize();
					break;
				}
			}
		}

		if(!bFoundMatch)
		{
			// if the heightmap is very large, fall back to using the largest values we support
			const int32 MaxSectionSize = SectionSizes[ARRAY_COUNT(SectionSizes) - 1];
			const int32 MaxNumSubSections = NumSections[ARRAY_COUNT(NumSections) - 1];
			const int32 ComponentsX = FMath::DivideAndRoundUp((Width - 1), MaxSectionSize * MaxNumSubSections);
			const int32 ComponentsY = FMath::DivideAndRoundUp((Height - 1), MaxSectionSize * MaxNumSubSections);

			bFoundMatch = true;
			UISettings->NewLandscape_QuadsPerSection = MaxSectionSize;
			UISettings->NewLandscape_SectionsPerComponent = MaxNumSubSections;
			UISettings->NewLandscape_ComponentCount.X = ComponentsX;
			UISettings->NewLandscape_ComponentCount.Y = ComponentsY;
			UISettings->NewLandscape_ClampSize();
		}

		check(bFoundMatch);
	}
}

void FNewLandscapeUtils::ImportLandscapeData( ULandscapeEditorObject* UISettings, TArray< FLandscapeFileResolution >& ImportResolutions )
{
	if ( !UISettings )
	{
		return;
	}

	ImportResolutions.Reset(1);
	UISettings->ImportLandscape_Width = 0;
	UISettings->ImportLandscape_Height = 0;
	UISettings->ClearImportLandscapeData();
	UISettings->ImportLandscape_HeightmapImportResult = ELandscapeImportResult::Success;
	UISettings->ImportLandscape_HeightmapErrorMessage = FText();

	if(!UISettings->ImportLandscape_HeightmapFilename.IsEmpty())
	{
		ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");
		const ILandscapeHeightmapFileFormat* HeightmapFormat = LandscapeEditorModule.GetHeightmapFormatByExtension(*FPaths::GetExtension(UISettings->ImportLandscape_HeightmapFilename, true));

		if(HeightmapFormat)
		{
			FLandscapeHeightmapInfo HeightmapImportInfo = HeightmapFormat->Validate(*UISettings->ImportLandscape_HeightmapFilename);
			UISettings->ImportLandscape_HeightmapImportResult = HeightmapImportInfo.ResultCode;
			UISettings->ImportLandscape_HeightmapErrorMessage = HeightmapImportInfo.ErrorMessage;
			ImportResolutions = MoveTemp(HeightmapImportInfo.PossibleResolutions);
			if(HeightmapImportInfo.DataScale.IsSet())
			{
				UISettings->NewLandscape_Scale = HeightmapImportInfo.DataScale.GetValue();
				UISettings->NewLandscape_Scale.Z *= LANDSCAPE_INV_ZSCALE;
			}
		}
		else
		{
			UISettings->ImportLandscape_HeightmapImportResult = ELandscapeImportResult::Error;
			UISettings->ImportLandscape_HeightmapErrorMessage = LOCTEXT("Import_UnknownFileType", "File type not recognised");
		}
	}

	if(ImportResolutions.Num() > 0)
	{
		int32 i = ImportResolutions.Num() / 2;
		UISettings->ImportLandscape_Width = ImportResolutions[i].Width;
		UISettings->ImportLandscape_Height = ImportResolutions[i].Height;
		UISettings->ImportLandscapeData();
		ChooseBestComponentSizeForImport(UISettings);
	}
}

TOptional< TArray< FLandscapeImportLayerInfo > > FNewLandscapeUtils::CreateImportLayersInfo( ULandscapeEditorObject* UISettings, int32 NewLandscapePreviewMode )
{
	const int32 ComponentCountX = UISettings->NewLandscape_ComponentCount.X;
	const int32 ComponentCountY = UISettings->NewLandscape_ComponentCount.Y;
	const int32 QuadsPerComponent = UISettings->NewLandscape_SectionsPerComponent * UISettings->NewLandscape_QuadsPerSection;
	const int32 SizeX = ComponentCountX * QuadsPerComponent + 1;
	const int32 SizeY = ComponentCountY * QuadsPerComponent + 1;

	TArray<FLandscapeImportLayerInfo> ImportLayers;

	if(NewLandscapePreviewMode == ENewLandscapePreviewMode::NewLandscape)
	{
		const auto& ImportLandscapeLayersList = UISettings->ImportLandscape_Layers;
		ImportLayers.Reserve(ImportLandscapeLayersList.Num());

		// Fill in LayerInfos array and allocate data
		for(const FLandscapeImportLayer& UIImportLayer : ImportLandscapeLayersList)
		{
			FLandscapeImportLayerInfo ImportLayer = FLandscapeImportLayerInfo(UIImportLayer.LayerName);
			ImportLayer.LayerInfo = UIImportLayer.LayerInfo;
			ImportLayer.SourceFilePath = "";
			ImportLayer.LayerData = TArray<uint8>();
			ImportLayers.Add(MoveTemp(ImportLayer));
		}

		// Fill the first weight-blended layer to 100%
		if(FLandscapeImportLayerInfo* FirstBlendedLayer = ImportLayers.FindByPredicate([](const FLandscapeImportLayerInfo& ImportLayer) { return ImportLayer.LayerInfo && !ImportLayer.LayerInfo->bNoWeightBlend; }))
		{
			FirstBlendedLayer->LayerData.AddUninitialized(SizeX * SizeY);

			uint8* ByteData = FirstBlendedLayer->LayerData.GetData();
			for(int32 i = 0; i < SizeX * SizeY; i++)
			{
				ByteData[i] = 255;
			}
		}
	}
	else if(NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
	{
		const uint32 ImportSizeX = UISettings->ImportLandscape_Width;
		const uint32 ImportSizeY = UISettings->ImportLandscape_Height;

		if(UISettings->ImportLandscape_HeightmapImportResult == ELandscapeImportResult::Error)
		{
			// Cancel import
			return TOptional< TArray< FLandscapeImportLayerInfo > >();
		}

		TArray<FLandscapeImportLayer>& ImportLandscapeLayersList = UISettings->ImportLandscape_Layers;
		ImportLayers.Reserve(ImportLandscapeLayersList.Num());

		// Fill in LayerInfos array and allocate data
		for(FLandscapeImportLayer& UIImportLayer : ImportLandscapeLayersList)
		{
			ImportLayers.Add((const FLandscapeImportLayer&)UIImportLayer); //slicing is fine here
			FLandscapeImportLayerInfo& ImportLayer = ImportLayers.Last();

			if(ImportLayer.LayerInfo != nullptr && ImportLayer.SourceFilePath != "")
			{
				ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");
				const ILandscapeWeightmapFileFormat* WeightmapFormat = LandscapeEditorModule.GetWeightmapFormatByExtension(*FPaths::GetExtension(ImportLayer.SourceFilePath, true));

				if(WeightmapFormat)
				{
					FLandscapeWeightmapImportData WeightmapImportData = WeightmapFormat->Import(*ImportLayer.SourceFilePath, ImportLayer.LayerName, { ImportSizeX, ImportSizeY });
					UIImportLayer.ImportResult = WeightmapImportData.ResultCode;
					UIImportLayer.ErrorMessage = WeightmapImportData.ErrorMessage;
					ImportLayer.LayerData = MoveTemp(WeightmapImportData.Data);
				}
				else
				{
					UIImportLayer.ImportResult = ELandscapeImportResult::Error;
					UIImportLayer.ErrorMessage = LOCTEXT("Import_UnknownFileType", "File type not recognised");
				}

				if(UIImportLayer.ImportResult == ELandscapeImportResult::Error)
				{
					ImportLayer.LayerData.Empty();
					FMessageDialog::Open(EAppMsgType::Ok, UIImportLayer.ErrorMessage);

					// Cancel import
					return TOptional< TArray< FLandscapeImportLayerInfo > >();
				}
			}
		}
	}

	return ImportLayers;
}

TArray< uint16 > FNewLandscapeUtils::ComputeHeightData( ULandscapeEditorObject* UISettings, TArray< FLandscapeImportLayerInfo >& ImportLayers, int32 NewLandscapePreviewMode )
{
	const int32 ComponentCountX = UISettings->NewLandscape_ComponentCount.X;
	const int32 ComponentCountY = UISettings->NewLandscape_ComponentCount.Y;
	const int32 QuadsPerComponent = UISettings->NewLandscape_SectionsPerComponent * UISettings->NewLandscape_QuadsPerSection;
	const int32 SizeX = ComponentCountX * QuadsPerComponent + 1;
	const int32 SizeY = ComponentCountY * QuadsPerComponent + 1;

	const uint32 ImportSizeX = UISettings->ImportLandscape_Width;
	const uint32 ImportSizeY = UISettings->ImportLandscape_Height;

	// Initialize heightmap data
	TArray<uint16> Data;
	Data.AddUninitialized(SizeX * SizeY);
	uint16* WordData = Data.GetData();

	// Initialize blank heightmap data
	for(int32 i = 0; i < SizeX * SizeY; i++)
	{
		WordData[i] = 32768;
	}

	if(NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
	{
		const TArray<uint16>& ImportData = UISettings->GetImportLandscapeData();
		if(ImportData.Num() != 0)
		{
			const int32 OffsetX = (int32)(SizeX - ImportSizeX) / 2;
			const int32 OffsetY = (int32)(SizeY - ImportSizeY) / 2;

			// Heightmap
			Data = LandscapeEditorUtils::ExpandData(ImportData,
				0, 0, ImportSizeX - 1, ImportSizeY - 1,
				-OffsetX, -OffsetY, SizeX - OffsetX - 1, SizeY - OffsetY - 1);

			// Layers
			for(int32 LayerIdx = 0; LayerIdx < ImportLayers.Num(); LayerIdx++)
			{
				TArray<uint8>& ImportLayerData = ImportLayers[LayerIdx].LayerData;
				if(ImportLayerData.Num())
				{
					ImportLayerData = LandscapeEditorUtils::ExpandData(ImportLayerData,
						0, 0, ImportSizeX - 1, ImportSizeY - 1,
						-OffsetX, -OffsetY, SizeX - OffsetX - 1, SizeY - OffsetY - 1);
				}
			}
		}
	}

	return Data;
}

#undef LOCTEXT_NAMESPACE
