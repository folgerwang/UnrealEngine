// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Engine/SubsurfaceProfile.h"
#include "RenderingThread.h"
#include "RendererInterface.h"
#include "Rendering/SeparableSSS.h"
#include "EngineModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubsurfaceProfile, Log, All);

// lives on the render thread
ENGINE_API TGlobalResource<FSubsurfaceProfileTexture> GSubsurfaceProfileTextureObject;

// Texture with one or more SubSurfaceProfiles or 0 if there is no user
static TRefCountPtr<IPooledRenderTarget> GSSProfiles;

FSubsurfaceProfileTexture::FSubsurfaceProfileTexture()
{
	check(IsInGameThread());

	FSubsurfaceProfileStruct DefaultSkin;

	// add element 0, it is used as default profile
	SubsurfaceProfileEntries.Add(FSubsurfaceProfileEntry(DefaultSkin, 0));
}

FSubsurfaceProfileTexture::~FSubsurfaceProfileTexture()
{
}

int32 FSubsurfaceProfileTexture::AddProfile(const FSubsurfaceProfileStruct Settings, const USubsurfaceProfile* InProfile)
{
	check(InProfile);
	check(FindAllocationId(InProfile) == -1);

	int32 RetAllocationId = -1;
	{
		for (int32 i = 1; i < SubsurfaceProfileEntries.Num(); ++i)
		{
			if (SubsurfaceProfileEntries[i].Profile == 0)
			{
				RetAllocationId = i;
				SubsurfaceProfileEntries[RetAllocationId].Profile = InProfile;
				break;
			}
		}

		if(RetAllocationId == -1)
		{
			RetAllocationId = SubsurfaceProfileEntries.Num();
			SubsurfaceProfileEntries.Add(FSubsurfaceProfileEntry(Settings, InProfile));
		}
	}

	UpdateProfile(RetAllocationId, Settings);

	return RetAllocationId;
}


void FSubsurfaceProfileTexture::RemoveProfile(const USubsurfaceProfile* InProfile)
{
	int32 AllocationId = FindAllocationId(InProfile);

	if(AllocationId == -1)
	{
		// -1: no allocation, no work needed
		return;
	}

	// >0 as 0 is used as default profile which should never be removed
	check(AllocationId > 0);

	check(SubsurfaceProfileEntries[AllocationId].Profile == InProfile);

	// make it available for reuse
	SubsurfaceProfileEntries[AllocationId].Profile = 0;
	SubsurfaceProfileEntries[AllocationId].Settings.Invalidate();
}

void FSubsurfaceProfileTexture::UpdateProfile(int32 AllocationId, const FSubsurfaceProfileStruct Settings)
{
	check(IsInRenderingThread());


	if (AllocationId == -1)
	{
		// if we modify a profile that is not assigned/used yet, no work is needed
		return;
	}

	check(AllocationId < SubsurfaceProfileEntries.Num());

	SubsurfaceProfileEntries[AllocationId].Settings = Settings;

	GSSProfiles.SafeRelease();
}

const IPooledRenderTarget* FSubsurfaceProfileTexture::GetTexture(FRHICommandListImmediate& RHICmdList)
{
	if (!GSSProfiles)
	{
		CreateTexture(RHICmdList);
	}

	return GSSProfiles;
}

void FSubsurfaceProfileTexture::ReleaseDynamicRHI()
{
	GSSProfiles.SafeRelease();
}

static float GetNextSmallerPositiveFloat(float x)
{
	check(x > 0);
	uint32 bx = *(uint32 *)&x;

	// float are ordered like int, at least for the positive part
	uint32 ax = bx - 1;

	return *(float *)&ax;
}

// NOTE: Changing offsets below requires updating all instances of #SSSS_CONSTANTS
// TODO: This needs to be defined in a single place and shared between C++ and shaders!
#define SSSS_SUBSURFACE_COLOR_OFFSET			0
#define SSSS_TRANSMISSION_OFFSET				(SSSS_SUBSURFACE_COLOR_OFFSET+1)
#define SSSS_BOUNDARY_COLOR_BLEED_OFFSET		(SSSS_TRANSMISSION_OFFSET+1)
#define SSSS_DUAL_SPECULAR_OFFSET				(SSSS_BOUNDARY_COLOR_BLEED_OFFSET+1)
#define SSSS_KERNEL0_OFFSET						(SSSS_DUAL_SPECULAR_OFFSET+1)
#define SSSS_KERNEL0_SIZE						13
#define SSSS_KERNEL1_OFFSET						(SSSS_KERNEL0_OFFSET + SSSS_KERNEL0_SIZE)
#define SSSS_KERNEL1_SIZE						9
#define SSSS_KERNEL2_OFFSET						(SSSS_KERNEL1_OFFSET + SSSS_KERNEL1_SIZE)
#define SSSS_KERNEL2_SIZE						6
#define SSSS_KERNEL_TOTAL_SIZE					(SSSS_KERNEL0_SIZE + SSSS_KERNEL1_SIZE + SSSS_KERNEL2_SIZE)
#define SSSS_TRANSMISSION_PROFILE_OFFSET		(SSSS_KERNEL0_OFFSET + SSSS_KERNEL_TOTAL_SIZE)
#define SSSS_TRANSMISSION_PROFILE_SIZE			32
#define	SSSS_MAX_TRANSMISSION_PROFILE_DISTANCE	5.0f // See MaxTransmissionProfileDistance in ComputeTransmissionProfile(), SeparableSSS.cpp
#define	SSSS_MAX_DUAL_SPECULAR_ROUGHNESS		2.0f

float Sqrt2(float X)
{
	return sqrtf(sqrtf(X));
}

float Pow4(float X)
{
	return X * X * X * X;
}

void FSubsurfaceProfileTexture::CreateTexture(FRHICommandListImmediate& RHICmdList)
{
	uint32 Height = SubsurfaceProfileEntries.Num();

	check(Height);

	// true:16bit (currently required to have very small and very large kernel sizes), false: 8bit
	const bool b16Bit = true;

	// Each row of the texture contains SSS parameters, followed by 3 precomputed kernels. Texture must be wide enough to fit all data.
	const uint32 Width = SSSS_TRANSMISSION_PROFILE_OFFSET + SSSS_TRANSMISSION_PROFILE_SIZE;

	// at minimum 64 lines (less reallocations)
	FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(FIntPoint(Width, FMath::Max(Height, (uint32)64)), PF_B8G8R8A8, FClearValueBinding::None, 0, TexCreate_None, false));
	if (b16Bit)
	{
		Desc.Format = PF_A16B16G16R16;
	}

	GetRendererModule().RenderTargetPoolFindFreeElement(RHICmdList, Desc, GSSProfiles, TEXT("SSProfiles"));

	// Write the contents of the texture.
	uint32 DestStride;
	uint8* DestBuffer = (uint8*)RHICmdList.LockTexture2D((FTexture2DRHIRef&)GSSProfiles->GetRenderTargetItem().ShaderResourceTexture, 0, RLM_WriteOnly, DestStride, false);

	FLinearColor TextureRow[Width];
	FMemory::Memzero(TextureRow);

	const float FloatScale = GetNextSmallerPositiveFloat(0x10000);
	check((int32)GetNextSmallerPositiveFloat(0x10000) == 0xffff);

	for (uint32 y = 0; y < Height; ++y)
	{
		FSubsurfaceProfileStruct Data = SubsurfaceProfileEntries[y].Settings;

		// bias to avoid div by 0 and a jump to a different value
		// this basically means we don't want subsurface scattering
		// 0.0001f turned out to be too small to fix the issue (for a small KernelSize)
		const float Bias = 0.009f;

		Data.SubsurfaceColor = Data.SubsurfaceColor.GetClamped();
		Data.FalloffColor = Data.FalloffColor.GetClamped(Bias);

		// to allow blending of the Subsurface with fullres in the shader
		TextureRow[SSSS_SUBSURFACE_COLOR_OFFSET] = Data.SubsurfaceColor;
		TextureRow[SSSS_SUBSURFACE_COLOR_OFFSET].A = 0; // unused

		TextureRow[SSSS_BOUNDARY_COLOR_BLEED_OFFSET] = Data.BoundaryColorBleed;

		float MaterialRoughnessToAverage = Data.Roughness0 * (1.0f - Data.LobeMix) + Data.Roughness1 * Data.LobeMix;
		float AverageToRoughness0 = Data.Roughness0 / MaterialRoughnessToAverage;
		float AverageToRoughness1 = Data.Roughness1 / MaterialRoughnessToAverage;

		TextureRow[SSSS_DUAL_SPECULAR_OFFSET].R = FMath::Clamp(AverageToRoughness0 / SSSS_MAX_DUAL_SPECULAR_ROUGHNESS, 0.0f, 1.0f);
		TextureRow[SSSS_DUAL_SPECULAR_OFFSET].G = FMath::Clamp(AverageToRoughness1 / SSSS_MAX_DUAL_SPECULAR_ROUGHNESS, 0.0f, 1.0f);
		TextureRow[SSSS_DUAL_SPECULAR_OFFSET].B = Data.LobeMix;
		TextureRow[SSSS_DUAL_SPECULAR_OFFSET].A = FMath::Clamp(MaterialRoughnessToAverage / SSSS_MAX_DUAL_SPECULAR_ROUGHNESS, 0.0f, 1.0f);

		//X:ExtinctionScale, Y:Normal Scale, Z:ScatteringDistribution, W:OneOverIOR
		TextureRow[SSSS_TRANSMISSION_OFFSET].R = Data.ExtinctionScale;
		TextureRow[SSSS_TRANSMISSION_OFFSET].G = Data.NormalScale;
		TextureRow[SSSS_TRANSMISSION_OFFSET].B = Data.ScatteringDistribution;
		TextureRow[SSSS_TRANSMISSION_OFFSET].A = 1.0f / Data.IOR;

		ComputeMirroredSSSKernel(&TextureRow[SSSS_KERNEL0_OFFSET], SSSS_KERNEL0_SIZE, Data.SubsurfaceColor, Data.FalloffColor);
		ComputeMirroredSSSKernel(&TextureRow[SSSS_KERNEL1_OFFSET], SSSS_KERNEL1_SIZE, Data.SubsurfaceColor, Data.FalloffColor);
		ComputeMirroredSSSKernel(&TextureRow[SSSS_KERNEL2_OFFSET], SSSS_KERNEL2_SIZE, Data.SubsurfaceColor, Data.FalloffColor);

		ComputeTransmissionProfile(&TextureRow[SSSS_TRANSMISSION_PROFILE_OFFSET], SSSS_TRANSMISSION_PROFILE_SIZE, Data.SubsurfaceColor, Data.FalloffColor, Data.ExtinctionScale);

		// could be lower than 1 (but higher than 0) to range compress for better quality (for 8 bit)
		const float TableMaxRGB = 1.0f;
		const float TableMaxA = 3.0f;
		const FLinearColor TableColorScale = FLinearColor(
			1.0f / TableMaxRGB,
			1.0f / TableMaxRGB,
			1.0f / TableMaxRGB,
			1.0f / TableMaxA);

		const float CustomParameterMaxRGB = 1.0f;
		const float CustomParameterMaxA = 1.0f;
		const FLinearColor CustomParameterColorScale = FLinearColor(
			1.0f / CustomParameterMaxRGB,
			1.0f / CustomParameterMaxRGB,
			1.0f / CustomParameterMaxRGB,
			1.0f / CustomParameterMaxA);

		// each kernel is normalized to be 1 per channel (center + one_side_samples * 2)
		for (int32 Pos = 0; Pos < Width; ++Pos)
		{
			FVector4 C = TextureRow[Pos];

			// Remap custom parameter and kernel values into 0..1
			if (Pos >= SSSS_KERNEL0_OFFSET && Pos < SSSS_KERNEL0_OFFSET + SSSS_KERNEL_TOTAL_SIZE)
			{
				C *= TableColorScale;
				// requires 16bit (could be made with 8 bit e.g. using sample0.w as 8bit scale applied to all samples (more multiplications in the shader))
				C.W *= Data.ScatterRadius / SUBSURFACE_RADIUS_SCALE;
			}
			else
			{
				C *= CustomParameterColorScale;
			}

			if (b16Bit)
			{
				// scale from 0..1 to 0..0xffff
				// scale with 0x10000 and round down to evenly distribute, avoid 0x10000

				uint16* Dest = (uint16*)(DestBuffer + DestStride * y);

				Dest[Pos * 4 + 0] = (uint16)(C.X * FloatScale);
				Dest[Pos * 4 + 1] = (uint16)(C.Y * FloatScale);
				Dest[Pos * 4 + 2] = (uint16)(C.Z * FloatScale);
				Dest[Pos * 4 + 3] = (uint16)(C.W * FloatScale);
			}
			else
			{
				FColor* Dest = (FColor*)(DestBuffer + DestStride * y);

				Dest[Pos] = FColor(FMath::Quantize8UnsignedByte(C.X), FMath::Quantize8UnsignedByte(C.Y), FMath::Quantize8UnsignedByte(C.Z), FMath::Quantize8UnsignedByte(C.W));
			}
		}
	}

	RHICmdList.UnlockTexture2D((FTexture2DRHIRef&)GSSProfiles->GetRenderTargetItem().ShaderResourceTexture, 0, false);
}

TCHAR MiniFontCharFromIndex(uint32 Index)
{
	if (Index <= 9)
	{
		return (TCHAR)('0' + Index);
	}

	Index -= 10;

	if (Index <= 'Z' - 'A')
	{
		return (TCHAR)('A' + Index);
	}

	return (TCHAR)'?';
}

bool FSubsurfaceProfileTexture::GetEntryString(uint32 Index, FString& Out) const
{
	if (Index >= (uint32)SubsurfaceProfileEntries.Num())
	{
		return false;
	}

	const FSubsurfaceProfileStruct& ref = SubsurfaceProfileEntries[Index].Settings;


	Out = FString::Printf(TEXT(" %c. %p ScatterRadius=%.1f, SubsurfaceColor=%.1f %.1f %.1f, FalloffColor=%.1f %.1f %.1f"), 
		MiniFontCharFromIndex(Index), 
		SubsurfaceProfileEntries[Index].Profile,
		ref.ScatterRadius,
		ref.SubsurfaceColor.R, ref.SubsurfaceColor.G, ref.SubsurfaceColor.B,
		ref.FalloffColor.R, ref.FalloffColor.G, ref.FalloffColor.B);

	return true;
}

int32 FSubsurfaceProfileTexture::FindAllocationId(const USubsurfaceProfile* InProfile) const
{
	// we start at 1 because [0] is the default profile and always [0].Profile = 0 so we don't need to iterate that one
	for (int32 i = 1; i < SubsurfaceProfileEntries.Num(); ++i)
	{
		if (SubsurfaceProfileEntries[i].Profile == InProfile)
		{
			return i;
		}
	}

	return -1;
}

// for debugging
void FSubsurfaceProfileTexture::Dump()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UE_LOG(LogSubsurfaceProfile, Log, TEXT("USubsurfaceProfileManager::Dump"));
	for (int32 i = 0; i < SubsurfaceProfileEntries.Num(); ++i)
	{
		// + 1 as the Id is one higher than the array index, 0 is used for the default profile (not assigned)
		UE_LOG(LogSubsurfaceProfile, Log, TEXT("  %d. AllocationId=%d, Pointer=%p"), i, i + 1, SubsurfaceProfileEntries[i].Profile);

		{
			UE_LOG(LogSubsurfaceProfile, Log, TEXT("     ScatterRadius = %f"),
				SubsurfaceProfileEntries[i].Settings.ScatterRadius);
			UE_LOG(LogSubsurfaceProfile, Log, TEXT("     SubsurfaceColor=%f %f %f"),
				SubsurfaceProfileEntries[i].Settings.SubsurfaceColor.R, SubsurfaceProfileEntries[i].Settings.SubsurfaceColor.G, SubsurfaceProfileEntries[i].Settings.SubsurfaceColor.B);
			UE_LOG(LogSubsurfaceProfile, Log, TEXT("     FalloffColor=%f %f %f"),
				SubsurfaceProfileEntries[i].Settings.FalloffColor.R, SubsurfaceProfileEntries[i].Settings.FalloffColor.G, SubsurfaceProfileEntries[i].Settings.FalloffColor.B);
		}
	}

	UE_LOG(LogSubsurfaceProfile, Log, TEXT(""));
#endif
}



ENGINE_API const IPooledRenderTarget* GetSubsufaceProfileTexture_RT(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	return GSubsurfaceProfileTextureObject.GetTexture(RHICmdList);
}

// ------------------------------------------------------

USubsurfaceProfile::USubsurfaceProfile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USubsurfaceProfile::BeginDestroy()
{
	USubsurfaceProfile* Ref = this;
	ENQUEUE_RENDER_COMMAND(RemoveSubsurfaceProfile)(
		[Ref](FRHICommandList& RHICmdList)
		{
			GSubsurfaceProfileTextureObject.RemoveProfile(Ref);
		});

	Super::BeginDestroy();
}

void USubsurfaceProfile::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FSubsurfaceProfileStruct SettingsLocal = this->Settings;
	USubsurfaceProfile* Profile = this;
	ENQUEUE_RENDER_COMMAND(UpdateSubsurfaceProfile)(
		[SettingsLocal, Profile](FRHICommandListImmediate& RHICmdList)
		{
			// any changes to the setting require an update of the texture
			GSubsurfaceProfileTextureObject.UpdateProfile(SettingsLocal, Profile);
		});
}
