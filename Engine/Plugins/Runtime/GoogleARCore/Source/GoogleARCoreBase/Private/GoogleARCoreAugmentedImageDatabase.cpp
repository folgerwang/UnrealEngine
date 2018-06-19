// Copyright 2018 Google Inc.

#include "GoogleARCoreAugmentedImageDatabase.h"

#include "GoogleARCoreBaseLogCategory.h"
#include "GoogleARCoreDevice.h"
#include "GoogleARCoreAPI.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"

#include "png.h"

#if WITH_EDITORONLY_DATA

// Disable warning "interaction between '_setjmp' and C++ object destruction is non-portable"
// and 'fopen': This function or variable may be unsafe. 
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4611)
#pragma warning(disable:4996)
#endif

namespace
{
	bool GoogleARCoreSaveTextureToPNG(UTexture2D *Tex, const FString &Filename)
	{
		TArray<uint8> MipData;
		bool Ret = true;

		if (Tex->Source.GetMipData(MipData, 0))
		{
			if (Tex->Source.GetFormat() != TSF_BGRA8 && Tex->Source.GetFormat() != TSF_RGBA8)
			{
				UE_LOG(
					LogGoogleARCoreAPI, Error,
					TEXT("Texture %s is not RGBA8 or BGRA8 and cannot be used as a tracking target."),
					*Tex->GetName());

				return false;
			}

			int32 Width = Tex->Source.GetSizeX();
			int32 Height = Tex->Source.GetSizeY();

			png_structp PngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
			png_infop PngInfoPtr = png_create_info_struct(PngPtr);

			// We're using C file IO here just because of libPNG interop.
			FILE *OutputFile = fopen(TCHAR_TO_ANSI(*Filename), "wb+");

			if (setjmp(png_jmpbuf(PngPtr)))
			{
				UE_LOG(
					LogGoogleARCoreAPI, Error,
					TEXT("Error writing PNG for texture %s."),
					*Tex->GetName());
				Ret = false;
			}
			else
			{
				png_init_io(PngPtr, OutputFile);
				png_set_IHDR(
					PngPtr, PngInfoPtr, Width, Height,
					8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
					PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
				png_write_info(PngPtr, PngInfoPtr);

				uint8_t *Row = new uint8_t[Width * 3];

				for (int32 y = 0; y < Height; y++)
				{
					for (int32 x = 0; x < Width; x++)
					{
						for (int32 c = 0; c < 3; c++)
						{
							int32 RealChannel = c;
							if (Tex->Source.GetFormat() == TSF_BGRA8)
							{
								if (RealChannel == 0) {
									RealChannel = 2;
								} else if (RealChannel == 2) {
									RealChannel = 0;
								}
							}
							Row[x * 3 + c] = MipData[(y * Width + x) * 4 + RealChannel];
						}
					}
					png_write_row(PngPtr, Row);
				}

				png_write_end(PngPtr, NULL);

				delete[] Row;
			}

			if (OutputFile)
			{
				fclose(OutputFile);
			}

			if (PngInfoPtr)
			{
				png_free_data(PngPtr, PngInfoPtr, PNG_FREE_ALL, -1);
			}

			if (PngPtr)
			{
				png_destroy_write_struct(&PngPtr, nullptr);
			}
		}
		else
		{
			UE_LOG(
				LogGoogleARCoreAPI, Error,
				TEXT("Error reading mip data in texture %s."),
				*Tex->GetName());

			return false;
		}

		return Ret;
	}
}

// Renable warning "interaction between '_setjmp' and C++ object destruction is non-portable"
// and 'fopen': This function or variable may be unsafe. 
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif

void UGoogleARCoreAugmentedImageDatabase::Serialize(FArchive& Ar)
{
	FString OutStdout;
	FString OutStderr;
	int32 OutReturnCode = 0;

#if !PLATFORM_ANDROID

	if (!Ar.IsLoading() && Ar.IsCooking()) {

		SerializedDatabase.Empty();

		if (Entries.Num()) {

			FString PathToDbTool =
				FPaths::Combine(
					*FPaths::EnginePluginsDir(),
					TEXT("Runtime"),
					TEXT("GoogleARCore"),
					TEXT("Tools"),
					*UGameplayStatics::GetPlatformName(),
#if PLATFORM_LINUX
					TEXT("arcoreimg")
#elif PLATFORM_WINDOWS
					TEXT("arcoreimg.exe")
#elif PLATFORM_MAC
					TEXT("ptdbtool_macos_lipobin")
#endif
					);

			FString TempDir =
				FPaths::ConvertRelativePathToFull(
					FPaths::Combine(
						*FPaths::EnginePluginsDir(),
						TEXT("Runtime"),
						TEXT("GoogleARCore"),
						TEXT("Intermediate"),
						TEXT("ARCoreImgTemp")));

			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			if (!PlatformFile.DirectoryExists(*TempDir))
			{
				PlatformFile.CreateDirectory(*TempDir);
			}

			TArray<FString> CleanupList;

			FString ImageListFileContents;

			for (int32 i = 0; i < Entries.Num(); i++) {
				UTexture2D *Tex = Entries[i].ImageAsset;

				if (Tex) {

					FString PNGFilename =
						FPaths::Combine(TempDir, Tex->GetName() + FString(".png"));

					if (GoogleARCoreSaveTextureToPNG(
							Tex,
							PNGFilename))
					{
						ImageListFileContents +=
							Tex->GetName() +
							FString("|") +
							PNGFilename;

						CleanupList.Add(PNGFilename);

						if (Entries[i].Width > 0.0f) {
							ImageListFileContents +=
								FString("|") +
								FString::SanitizeFloat(Entries[i].Width);
						}

						ImageListFileContents += FString("\n");
					}

				}
			}

			FString PathToImageList =
				FPaths::Combine(TempDir, TEXT("image_list.txt"));
			FString PathToImageDb =
				FPaths::Combine(TempDir, TEXT("image_list.imgdb"));

			CleanupList.Add(PathToImageList);
			CleanupList.Add(PathToImageDb);

			FFileHelper::SaveStringToFile(
				ImageListFileContents,
				*PathToImageList);

			OutStderr = "";
			OutStdout = "";
			OutReturnCode = 0;

			FPlatformProcess::ExecProcess(
				*PathToDbTool,
				*(FString("build-db --input_image_list_path=") + PathToImageList +
				  FString(" --output_db_path=") + PathToImageDb),
				&OutReturnCode,
				&OutStdout,
				&OutStderr);

			if (OutReturnCode)
			{
				Ar.SetError();
				Ar.ArIsError = 1;
				Ar.ArIsCriticalError = 1;
				UE_LOG(LogGoogleARCoreAPI, Error, TEXT("Failed to build augmented image database."));
			}
			else
			{
				FFileHelper::LoadFileToArray(SerializedDatabase, *PathToImageDb, 0);
				UE_LOG(LogGoogleARCoreAPI, Log,
					   TEXT("Augmented image database created. Size: %d bytes."),
					   SerializedDatabase.Num());
			}

			for (int32 i = 0; i < CleanupList.Num(); i++)
			{
				UE_LOG(LogGoogleARCoreAPI, Log, TEXT("Cleaning up: %s"), *CleanupList[i]);
				PlatformFile.DeleteFile(*CleanupList[i]);
			}

			PlatformFile.DeleteDirectory(*TempDir);
		}
	}

#endif

	// Must happen AFTER database generation, because we rely on the
	// UPROPERTY serialization to actually save the data.
	Super::Serialize(Ar);
}
