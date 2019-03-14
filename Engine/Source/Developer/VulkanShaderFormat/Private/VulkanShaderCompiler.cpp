// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
// .

#include "VulkanShaderFormat.h"
#include "VulkanCommon.h"
#include "ShaderPreprocessor.h"
#include "ShaderCompilerCommon.h"
#include "hlslcc.h"

#if PLATFORM_MAC
// Horrible hack as we need the enum available but the Vulkan headers do not compile on Mac
enum VkDescriptorType {
	VK_DESCRIPTOR_TYPE_SAMPLER = 0,
	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1,
	VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE = 2,
	VK_DESCRIPTOR_TYPE_STORAGE_IMAGE = 3,
	VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER = 4,
	VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER = 5,
	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6,
	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER = 7,
	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC = 8,
	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC = 9,
	VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT = 10,
	VK_DESCRIPTOR_TYPE_BEGIN_RANGE = VK_DESCRIPTOR_TYPE_SAMPLER,
	VK_DESCRIPTOR_TYPE_END_RANGE = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
	VK_DESCRIPTOR_TYPE_RANGE_SIZE = (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1),
	VK_DESCRIPTOR_TYPE_MAX_ENUM = 0x7FFFFFFF
};
#else
#include "vulkan.h"
#endif
#include "VulkanBackend.h"
#include "VulkanShaderResources.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"


DEFINE_LOG_CATEGORY_STATIC(LogVulkanShaderCompiler, Log, All); 

//static int32 GUseExternalShaderCompiler = 0;
//static FAutoConsoleVariableRef CVarVulkanUseExternalShaderCompiler(
//	TEXT("r.Vulkan.UseExternalShaderCompiler"),
//	GUseExternalShaderCompiler,
//	TEXT("Whether to use the internal shader compiling library or the external glslang tool.\n")
//	TEXT(" 0: Internal compiler\n")
//	TEXT(" 1: External compiler)"),
//	ECVF_Default
//	);


static TArray<ANSICHAR> ParseIdentifierANSI(const FString& Str)
{
	TArray<ANSICHAR> Result;
	Result.Reserve(Str.Len());
	for (int32 Index = 0; Index < Str.Len(); ++Index)
	{
		Result.Add(FChar::ToLower((ANSICHAR)Str[Index]));
	}
	Result.Add('\0');

	return Result;
}


inline const ANSICHAR * CStringEndOfLine(const ANSICHAR * Text)
{
	const ANSICHAR * LineEnd = FCStringAnsi::Strchr(Text, '\n');
	if (nullptr == LineEnd)
	{
		LineEnd = Text + FCStringAnsi::Strlen(Text);
	}
	return LineEnd;
}

inline bool CStringIsBlankLine(const ANSICHAR * Text)
{
	while (!FCharAnsi::IsLinebreak(*Text))
	{
		if (!FCharAnsi::IsWhitespace(*Text))
		{
			return false;
		}
		++Text;
	}
	return true;
}

static inline bool IsAlpha(ANSICHAR c)
{
	return (c >='a' && c <= 'z') || (c >='A' && c <='Z');
}

static inline bool IsDigit(ANSICHAR c)
{
	return c >= '0' && c <= '9';
}

static FString ParseIdentifier(const ANSICHAR* &Str)
{
	FString Result;

	while ((*Str >= 'A' && *Str <= 'Z')
		|| (*Str >= 'a' && *Str <= 'z')
		|| (*Str >= '0' && *Str <= '9')
		|| *Str == '_')
	{
		Result += *Str;
		++Str;
	}

	return Result;
}

inline void AppendCString(TArray<ANSICHAR> & Dest, const ANSICHAR * Source)
{
	if (Dest.Num() > 0)
	{
		Dest.Insert(Source, FCStringAnsi::Strlen(Source), Dest.Num() - 1);;
	}
	else
	{
		Dest.Append(Source, FCStringAnsi::Strlen(Source) + 1);
	}
}

inline bool MoveHashLines(TArray<ANSICHAR> & Dest, TArray<ANSICHAR> & Source)
{
	// Walk through the lines to find the first non-# line...
	const ANSICHAR * LineStart = Source.GetData();
	for (bool FoundNonHashLine = false; !FoundNonHashLine;)
	{
		const ANSICHAR * LineEnd = CStringEndOfLine(LineStart);
		if (LineStart[0] != '#' && !CStringIsBlankLine(LineStart))
		{
			FoundNonHashLine = true;
		}
		else if (LineEnd[0] == '\n')
		{
			LineStart = LineEnd + 1;
		}
		else
		{
			LineStart = LineEnd;
		}
	}
	// Copy the hash lines over, if we found any. And delete from
	// the source.
	if (LineStart > Source.GetData())
	{
		int32 LineLength = LineStart - Source.GetData();
		if (Dest.Num() > 0)
		{
			Dest.Insert(Source.GetData(), LineLength, Dest.Num() - 1);
		}
		else
		{
			Dest.Append(Source.GetData(), LineLength);
			Dest.Append("", 1);
		}
		if (Dest.Last(1) != '\n')
		{
			Dest.Insert("\n", 1, Dest.Num() - 1);
		}
		Source.RemoveAt(0, LineStart - Source.GetData());
		return true;
	}
	return false;
}

static bool Match(const ANSICHAR* &Str, ANSICHAR Char)
{
	if (*Str == Char)
	{
		++Str;
		return true;
	}

	return false;
}

template <typename T>
uint32 ParseNumber(const T* Str)
{
	check(Str);

	uint32 Num = 0;

	int32 Len = 0;
	// Find terminating character
	for(int32 Index=0; Index<128; Index++)
	{
		if(Str[Index] == 0)
		{
			Len = Index;
			break;
		}
	}

	check(Len > 0);

	// Find offset to integer type
	int32 Offset = -1;
	for(int32 Index=0; Index<Len; Index++)
	{
		if (*(Str + Index) >= '0' && *(Str + Index) <= '9')
		{
			Offset = Index;
			break;
		}
	}

	// Check if we found a number
	check(Offset >= 0);

	Str += Offset;

	while (*(Str) && *Str >= '0' && *Str <= '9')
	{
		Num = Num * 10 + *Str++ - '0';
	}

	return Num;
}

static inline FString GetExtension(EHlslShaderFrequency Frequency, bool bAddDot = true)
{
	const TCHAR* Name = nullptr;
	switch (Frequency)
	{
	default:
		check(0);
		// fallthrough...

	case HSF_PixelShader:		Name = TEXT(".frag"); break;
	case HSF_VertexShader:		Name = TEXT(".vert"); break;
	case HSF_ComputeShader:		Name = TEXT(".comp"); break;
	case HSF_GeometryShader:	Name = TEXT(".geom"); break;
	case HSF_HullShader:		Name = TEXT(".tesc"); break;
	case HSF_DomainShader:		Name = TEXT(".tese"); break;
	}

	if (!bAddDot)
	{
		++Name;
	}
	return FString(Name);
}

static uint32 GetTypeComponents(const FString& Type)
{
	static const FString TypePrefix[] = { "f", "i", "u" };
	uint32 Components = 0;
	int32 PrefixLength = 0;
	for (uint32 i = 0; i<ARRAY_COUNT(TypePrefix); i++)
	{
		const FString& Prefix = TypePrefix[i];
		const int32 CmpLength = Type.Contains(Prefix, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		if (CmpLength == Prefix.Len())
		{
			PrefixLength = CmpLength;
			break;
		}
	}

	check(PrefixLength > 0);
	Components = ParseNumber(*Type + PrefixLength);

	check(Components > 0);
	return Components;
}


static bool ContainsBinding(const FVulkanBindingTable& BindingTable, const FString& Name)
{
	for (const FVulkanBindingTable::FBinding& Binding : BindingTable.GetBindings())
	{
		if (Binding.Name == Name)
		{
			return true;
		}
	}

	return false;
}

static FString GetResourceEntryFromUBMember(const TMap<FString, FResourceTableEntry>& ResourceTableMap, const FString& UBName, uint16 ResourceIndex, FResourceTableEntry& OutEntry)
{
	for (const auto& Pair : ResourceTableMap)
	{
		if (Pair.Value.UniformBufferName == UBName && Pair.Value.ResourceIndex == ResourceIndex)
		{
			OutEntry = Pair.Value;
			return Pair.Key;
		}
	}

	check(0);
	return "";
}

static FString FindTextureNameForSamplerState(const CrossCompiler::FHlslccHeader& CCHeader, const FString& InSamplerName)
{
	for (const auto& Sampler : CCHeader.Samplers)
	{
		for (const auto& SamplerState : Sampler.SamplerStates)
		{
			if (SamplerState == InSamplerName)
			{
				return Sampler.Name;
			}
		}
	}
	return TEXT("");
}

static uint16 GetCombinedSamplerStateAlias(const FString& ParameterName,
											VkDescriptorType DescriptorType,
											const FVulkanBindingTable& BindingTable,
											const CrossCompiler::FHlslccHeader& CCHeader,
											const TArray<FString>& GlobalNames)
{
	if (DescriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	{
		if (!ContainsBinding(BindingTable, ParameterName))
		{
			// Not found as a regular parameter, find corresponding Texture and return that ResourceEntryIndex
			const FString& TextureName = FindTextureNameForSamplerState(CCHeader, ParameterName);
			check(TextureName.Len() > 0);

			int32 Found = GlobalNames.Find(TextureName);
			check(Found >= 0);
			return (uint16)Found;
		}
	}

	return UINT16_MAX;
}

struct FPatchType
{
	int32	HeaderGlobalIndex;
	uint16	CombinedAliasIndex;
};


class FVulkanShaderSerializedBindings : public CrossCompiler::FShaderBindings
{
public:
	FVulkanShaderSerializedBindings()
	{
		InOutMask = 0;
		NumSamplers = 0;
		NumUniformBuffers = 0;
		NumUAVs = 0;
		bHasRegularUniformBuffers = 0;
	}
};

struct FOLDVulkanCodeHeader
{
	FVulkanShaderSerializedBindings SerializedBindings;

	struct FShaderDescriptorInfo
	{
		TArray<VkDescriptorType> DescriptorTypes;
		uint16 NumImageInfos;
		uint16 NumBufferInfos;
	};
	FShaderDescriptorInfo NEWDescriptorInfo;

	struct FPackedUBToVulkanBindingIndex
	{
		CrossCompiler::EPackedTypeName	TypeName;
		uint8							VulkanBindingIndex;
	};
	TArray<FPackedUBToVulkanBindingIndex> NEWPackedUBToVulkanBindingIndices;

	// List of memory copies from RHIUniformBuffer to packed uniforms when emulating UB's
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;

	FString ShaderName;
	FSHAHash SourceHash;

	uint64 UniformBuffersWithDescriptorMask;

	// Number of uniform buffers (not including PackedGlobalUBs) UNUSED
	uint32 UNUSED_NumNonGlobalUBs;

	// (Separated to improve cache) if this is non-zero, then we can assume all UBs are emulated
	TArray<uint32> NEWPackedGlobalUBSizes;

	// Number of copies per emulated buffer source index (to skip searching among UniformBuffersCopyInfo). Upper uint16 is the index, Lower uint16 is the count
	TArray<uint32> NEWEmulatedUBCopyRanges;
};

static void AddImmutable(FVulkanShaderHeader& OutHeader, int32 GlobalIndex)
{
	check(GlobalIndex < UINT16_MAX);
	OutHeader.Globals[GlobalIndex].bImmutableSampler = true;
}

static int32 AddGlobal(FOLDVulkanCodeHeader& OLDHeader,
						const FVulkanBindingTable& BindingTable,
						const CrossCompiler::FHlslccHeader& CCHeader,
						const FString& ParameterName,
						uint16 BaseIndex,
						const FSpirv& Spirv,
						FVulkanShaderHeader& OutHeader,
						const TArray<FString>& GlobalNames,
						TArray<FPatchType>& OutTypePatch,
						uint16 CombinedAliasIndex)
{
	int32 HeaderGlobalIndex = GlobalNames.Find(ParameterName);//OutHeader.Globals.AddZeroed();
	check(HeaderGlobalIndex != INDEX_NONE);
	check(GlobalNames[HeaderGlobalIndex] == ParameterName);

	FVulkanShaderHeader::FGlobalInfo& GlobalInfo = OutHeader.Globals[HeaderGlobalIndex];
	const FSpirv::FEntry* Entry = Spirv.GetEntry(ParameterName);
	bool bIsCombinedSampler = false;
	if (Entry)
	{
		if (Entry->Binding == -1)
		{
			// Texel buffers get put into a uniform block
			Entry = Spirv.GetEntry(ParameterName + TEXT("_BUFFER"));
			check(Entry);
			check(Entry->Binding != -1);
		}
	}
	else
	{
		Entry = CombinedAliasIndex == UINT16_MAX ? Spirv.GetEntryByBindingIndex(BaseIndex) : Spirv.GetEntry(GlobalNames[CombinedAliasIndex]);
		check(Entry);
		check(Entry->Binding != -1);
		bIsCombinedSampler = true;
	}

	VkDescriptorType DescriptorType = bIsCombinedSampler ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : OLDHeader.NEWDescriptorInfo.DescriptorTypes[Entry->Binding];

	GlobalInfo.OriginalBindingIndex = Entry->Binding;
	OutHeader.GlobalSpirvInfos[HeaderGlobalIndex] = FVulkanShaderHeader::FSpirvInfo(Entry->WordDescriptorSetIndex, Entry->WordBindingIndex);
	if (bIsCombinedSampler)
	{
		uint16 NewCombinedAliasIndex = GetCombinedSamplerStateAlias(ParameterName, DescriptorType, BindingTable, CCHeader, GlobalNames);
		check(NewCombinedAliasIndex != UINT16_MAX);

		{
			// Ideally we would set up the type index here, but we might not have processed the aliased texture yet:
			//		GlobalInfo.TypeIndex = OutHeader.Globals[NewCombinedAliasIndex].TypeIndex;
			// Instead postpone this patching
			GlobalInfo.TypeIndex = UINT16_MAX;
			OutTypePatch.Add({HeaderGlobalIndex, NewCombinedAliasIndex});
		}

		GlobalInfo.CombinedSamplerStateAliasIndex = CombinedAliasIndex == UINT16_MAX ? NewCombinedAliasIndex : CombinedAliasIndex;
	}
	else
	{
		int32 GlobalDescriptorTypeIndex = OutHeader.GlobalDescriptorTypes.Add(DescriptorType);
		GlobalInfo.TypeIndex = GlobalDescriptorTypeIndex;
		check(GetCombinedSamplerStateAlias(ParameterName, DescriptorType, BindingTable, CCHeader, GlobalNames) == UINT16_MAX);
		GlobalInfo.CombinedSamplerStateAliasIndex = UINT16_MAX;
	}
#if VULKAN_ENABLE_SHADER_DEBUG_NAMES
	GlobalInfo.DebugName = ParameterName;
#endif

	return HeaderGlobalIndex;
}

static int32 AddGlobalForUBEntry(FOLDVulkanCodeHeader& OLDHeader,
									const FVulkanBindingTable& BindingTable,
									const CrossCompiler::FHlslccHeader& CCHeader,
									const FString& ParameterName,
									uint16 BaseIndex,
									const FSpirv& Spirv,
									const TArray<FString>&
									GlobalNames,
									EUniformBufferBaseType UBEntryType,
									TArray<FPatchType>& OutTypePatch,
									FVulkanShaderHeader& OutHeader)
{
	uint16 CombinedAliasIndex = UINT16_MAX;
	if (UBEntryType == UBMT_SAMPLER)
	{
		if (!ContainsBinding(BindingTable, ParameterName))
		{
			// Not found as a regular parameter, find corresponding Texture and return that ResourceEntryIndex
			const FString& TextureName = FindTextureNameForSamplerState(CCHeader, ParameterName);
			check(TextureName.Len() > 0);

			int32 TextureGlobalIndex = GlobalNames.Find(TextureName);
			check(TextureGlobalIndex >= 0);

			CombinedAliasIndex = (uint16)TextureGlobalIndex;
		}
	}

	return AddGlobal(OLDHeader, BindingTable, CCHeader, ParameterName, BaseIndex, Spirv, OutHeader, GlobalNames, OutTypePatch, CombinedAliasIndex);
}

static void AddUBResources(FOLDVulkanCodeHeader& OLDHeader,
							const FString& UBName,
							const TMap<FString, FResourceTableEntry>& ResourceTableMap,
							uint32 BufferIndex,
							const TArray<uint32>& BindingArray,
							const FVulkanBindingTable& BindingTable,
							const TArray<VkDescriptorType>& DescriptorTypes,
							const FSpirv& Spirv,
							const CrossCompiler::FHlslccHeader& CCHeader,
							FVulkanShaderHeader::FUniformBufferInfo& OutUBInfo,
							FVulkanShaderHeader& OutHeader,
							TArray<FPatchType>& OutTypePatch,
							TArray<FString>& GlobalNames)
{
	if (BindingArray.Num() > 0)
	{
		uint32 BufferOffset = BindingArray[BufferIndex];
		if (BufferOffset > 0)
		{
			// Extract all resources related to the current BufferIndex
			const uint32* ResourceInfos = &BindingArray[BufferOffset];
			uint32 ResourceInfo = *ResourceInfos++;
			do
			{
				// Verify that we have correct buffer index
				check(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);

				// Extract binding index from ResourceInfo
				const uint32 BindingIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

				// Extract index of the resource stored in the resource table from ResourceInfo
				const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);

				FResourceTableEntry ResourceTableEntry;
				FString MemberName = GetResourceEntryFromUBMember(ResourceTableMap, UBName, ResourceIndex, ResourceTableEntry);

				int32 HeaderUBResourceInfoIndex = OutUBInfo.ResourceEntries.AddZeroed();
				FVulkanShaderHeader::FUBResourceInfo& UBResourceInfo = OutUBInfo.ResourceEntries[HeaderUBResourceInfoIndex];

				int32 HeaderGlobalIndex = AddGlobalForUBEntry(OLDHeader, BindingTable, CCHeader, MemberName, ResourceIndex, Spirv, GlobalNames, (EUniformBufferBaseType)ResourceTableEntry.Type, OutTypePatch, OutHeader);
				UBResourceInfo.SourceUBResourceIndex = ResourceIndex;
				UBResourceInfo.OriginalBindingIndex = BindingIndex;
				UBResourceInfo.GlobalIndex = HeaderGlobalIndex;
				UBResourceInfo.UBBaseType = (EUniformBufferBaseType)ResourceTableEntry.Type;
#if VULKAN_ENABLE_SHADER_DEBUG_NAMES
				UBResourceInfo.DebugName = MemberName;
#endif
				// Iterate to next info
				ResourceInfo = *ResourceInfos++;
			}
			while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
		}
	}
}

static void AddUniformBuffer(FOLDVulkanCodeHeader& OLDHeader,
	const FVulkanBindingTable& BindingTable,
	const FShaderCompilerInput& ShaderInput,
	const CrossCompiler::FHlslccHeader& CCHeader,
	const FSpirv& Spirv,
	const FString& UBName,
	uint16 BindingIndex,
	FShaderParameterMap& InOutParameterMap,
	FVulkanShaderHeader& OutHeader,
	TArray<FPatchType>& OutTypePatch,
	TArray<FString>& GlobalNames,
	bool bUseRealUBs)
{
	int32 HeaderUBIndex = -1;
	if (bUseRealUBs)
	{
		HeaderUBIndex = OutHeader.UniformBuffers.AddZeroed();
	}
	else
	{
		if (BindingIndex >= OutHeader.UniformBuffers.Num())
		{
			OutHeader.UniformBuffers.SetNumZeroed(BindingIndex + 1);
		}
		HeaderUBIndex = BindingIndex;
	}
	FVulkanShaderHeader::FUniformBufferInfo& UBInfo = OutHeader.UniformBuffers[HeaderUBIndex];
	const uint32* LayoutHash = ShaderInput.Environment.ResourceTableLayoutHashes.Find(UBName);
	UBInfo.LayoutHash = LayoutHash ? *LayoutHash : 0;
#if VULKAN_ENABLE_SHADER_DEBUG_NAMES
	UBInfo.DebugName = UBName;
#endif
	const FSpirv::FEntry* Entry = Spirv.GetEntry(UBName);
	if (Entry)
	{
		checkf(bUseRealUBs, TEXT("Internal error: Emulated should NOT have a binding!"));
		UBInfo.bOnlyHasResources = false;
		UBInfo.ConstantDataOriginalBindingIndex = BindingIndex;
		if (bUseRealUBs)
		{
			// Only real UBs require an entry for SPIRV
			int32 SpirvInfoIndex = OutHeader.UniformBufferSpirvInfos.Add(FVulkanShaderHeader::FSpirvInfo(Entry->WordDescriptorSetIndex, Entry->WordBindingIndex));
			check(SpirvInfoIndex == HeaderUBIndex);
		}
	}
	else
	{
		UBInfo.bOnlyHasResources = true;
		UBInfo.ConstantDataOriginalBindingIndex = UINT16_MAX;
		if (bUseRealUBs)
		{
			// Only real UBs require a dummy entry for SPIRV
			int32 SpirvInfoIndex = OutHeader.UniformBufferSpirvInfos.Add(FVulkanShaderHeader::FSpirvInfo());
			check(SpirvInfoIndex == HeaderUBIndex);
		}
	}

	// Add used resources...
	const FShaderCompilerResourceTable& SRT = OLDHeader.SerializedBindings.ShaderResourceTable;
	if (SRT.ResourceTableBits & (1 << BindingIndex))
	{
		// Make sure to process in the same order as when gathering names below
		AddUBResources(OLDHeader, UBName, ShaderInput.Environment.ResourceTableMap, BindingIndex, SRT.TextureMap, BindingTable, OLDHeader.NEWDescriptorInfo.DescriptorTypes, Spirv, CCHeader, UBInfo, OutHeader, OutTypePatch, GlobalNames);
		AddUBResources(OLDHeader, UBName, ShaderInput.Environment.ResourceTableMap, BindingIndex, SRT.SamplerMap, BindingTable, OLDHeader.NEWDescriptorInfo.DescriptorTypes, Spirv, CCHeader, UBInfo, OutHeader, OutTypePatch, GlobalNames);
		AddUBResources(OLDHeader, UBName, ShaderInput.Environment.ResourceTableMap, BindingIndex, SRT.ShaderResourceViewMap, BindingTable, OLDHeader.NEWDescriptorInfo.DescriptorTypes, Spirv, CCHeader, UBInfo, OutHeader, OutTypePatch, GlobalNames);
		AddUBResources(OLDHeader, UBName, ShaderInput.Environment.ResourceTableMap, BindingIndex, SRT.UnorderedAccessViewMap, BindingTable, OLDHeader.NEWDescriptorInfo.DescriptorTypes, Spirv, CCHeader, UBInfo, OutHeader, OutTypePatch, GlobalNames);
	}
	else
	{
		// If we're using real uniform buffers we have to have resources at least
		check(!bUseRealUBs || !UBInfo.bOnlyHasResources);
	}

	// Currently we don't support mismatched uniform buffer layouts/cbuffers with resources!
	check(LayoutHash || UBInfo.ResourceEntries.Num() == 0);

	InOutParameterMap.RemoveParameterAllocation(*UBName);
	InOutParameterMap.AddParameterAllocation(*UBName, HeaderUBIndex, (uint16)FVulkanShaderHeader::UniformBuffer, 1, EShaderParameterType::UniformBuffer);
}

static int32 DoAddGlobal(const FString& Name, FVulkanShaderHeader& OutHeader, TArray<FString>& OutGlobalNames)
{
	check(!OutGlobalNames.Contains(Name));
	int32 NameIndex = OutGlobalNames.Add(Name);
	int32 GlobalIndex = OutHeader.Globals.AddDefaulted();
	check(NameIndex == GlobalIndex);
	int32 GlobalSpirvIndex = OutHeader.GlobalSpirvInfos.AddDefaulted();
	check(GlobalSpirvIndex == GlobalIndex);
	return GlobalIndex;
}

struct FVulkanHlslccHeader : public CrossCompiler::FHlslccHeader
{
	virtual bool ParseCustomHeaderEntries(const ANSICHAR*& ShaderSource) override
	{
		if (FCStringAnsi::Strncmp(ShaderSource, "// @ExternalTextures: ", 22) == 0)
		{
			ShaderSource += 22;
			while (*ShaderSource && *ShaderSource != '\n')
			{
				FString ExternalTextureName;
				if (!CrossCompiler::ParseIdentifier(ShaderSource, ExternalTextureName))
				{
					return false;
				}

				ExternalTextures.Add(ExternalTextureName);

				if (Match(ShaderSource, '\n'))
				{
					break;
				}

				if (Match(ShaderSource, ','))
				{
					continue;
				}
			}
		}

		return true;
	}

	TArray<FString> ExternalTextures;
};

static void PrepareUBResourceEntryGlobals(const FVulkanHlslccHeader& CCHeader, const TArray<uint32>& BindingArray, const TMap<FString, FResourceTableEntry>& ResourceTableMap,
	int32 BufferIndex, const FString& UBName, TArray<FString>& OutGlobalNames, FVulkanShaderHeader& OutHeader)
{
	if (BindingArray.Num() > 0)
	{
		uint32 BufferOffset = BindingArray[BufferIndex];
		if (BufferOffset > 0)
		{
			// Extract all resources related to the current BufferIndex
			const uint32* ResourceInfos = &BindingArray[BufferOffset];
			uint32 ResourceInfo = *ResourceInfos++;
			do
			{
				// Verify that we have correct buffer index
				check(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);

				// Extract binding index from ResourceInfo
				const uint32 BindingIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

				// Extract index of the resource stored in the resource table from ResourceInfo
				const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);

				FResourceTableEntry ResourceTableEntry;
				FString MemberName = GetResourceEntryFromUBMember(ResourceTableMap, UBName, ResourceIndex, ResourceTableEntry);

				int32 GlobalIndex = DoAddGlobal(MemberName, OutHeader, OutGlobalNames);
				if (CCHeader.ExternalTextures.Contains(MemberName))
				{
					AddImmutable(OutHeader, GlobalIndex);
				}

				// Iterate to next info
				ResourceInfo = *ResourceInfos++;
			}
			while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
		}
	}
}

static bool IsSamplerState(const CrossCompiler::FHlslccHeader& CCHeader, const FString& ParameterName)
{
	for (const auto& Sampler : CCHeader.Samplers)
	{
		if (Sampler.SamplerStates.Contains(ParameterName))
		{
			return true;
		}
	}

	return false;
}

static void PrepareGlobals(const FVulkanBindingTable& BindingTable, const FVulkanHlslccHeader& CCHeader, const FShaderCompilerResourceTable& SRT, const TMap<FString, FVulkanShaderHeader::EType>& EntryTypes, const FShaderCompilerInput& ShaderInput, const TArray<FString>& ParameterNames, FShaderParameterMap& ParameterMap, TArray<FString>& OutGlobalNames, FVulkanShaderHeader& OutHeader, bool bHasRealUBs)
{
	// First pass, gather names for all the Globals that are NOT Samplers
	for (int32 ParameterIndex = 0; ParameterIndex < ParameterNames.Num(); ++ParameterIndex)
	{
		uint16 BufferIndex;
		uint16 BaseIndex;
		uint16 Size;
		const FString& ParameterName = *ParameterNames[ParameterIndex];
		ParameterMap.FindParameterAllocation(*ParameterName, BufferIndex, BaseIndex, Size);

		auto AddGlobalNamesForUB = [&]()
		{
			// Add used resources...
			if (SRT.ResourceTableBits & (1 << BufferIndex))
			{
				PrepareUBResourceEntryGlobals(CCHeader, SRT.TextureMap, ShaderInput.Environment.ResourceTableMap, BufferIndex, ParameterName, OutGlobalNames, OutHeader);
				PrepareUBResourceEntryGlobals(CCHeader, SRT.ShaderResourceViewMap, ShaderInput.Environment.ResourceTableMap, BufferIndex, ParameterName, OutGlobalNames, OutHeader);
				PrepareUBResourceEntryGlobals(CCHeader, SRT.UnorderedAccessViewMap, ShaderInput.Environment.ResourceTableMap, BufferIndex, ParameterName, OutGlobalNames, OutHeader);
			}
		};

		const FVulkanShaderHeader::EType* FoundType = EntryTypes.Find(ParameterName);
		if (FoundType)
		{
			switch (*FoundType)
			{
			case FVulkanShaderHeader::Global:
				if (!IsSamplerState(CCHeader, ParameterName))
				{
					int32 GlobalIndex = DoAddGlobal(ParameterName, OutHeader, OutGlobalNames);
					if (CCHeader.ExternalTextures.Contains(ParameterName))
					{
						AddImmutable(OutHeader, GlobalIndex);
					}
				}
				break;
			case FVulkanShaderHeader::UniformBuffer:
				check(bHasRealUBs);
				AddGlobalNamesForUB();
				break;
			case FVulkanShaderHeader::PackedGlobal:
				if (bHasRealUBs || Size > 0)
				{
					// Ignore
				}
				else if (!bHasRealUBs)
				{
					AddGlobalNamesForUB();
				}
				break;
			default:
				check(0);
				break;
			}
		}
		else
		{
			AddGlobalNamesForUB();
		}
	}

	// Second pass, add all samplers
	for (int32 ParameterIndex = 0; ParameterIndex < ParameterNames.Num(); ++ParameterIndex)
	{
		uint16 BufferIndex;
		uint16 BaseIndex;
		uint16 Size;
		const FString& ParameterName = *ParameterNames[ParameterIndex];
		ParameterMap.FindParameterAllocation(*ParameterName, BufferIndex, BaseIndex, Size);

		auto AddGlobalNamesForUB = [&]()
		{
			// Add used resources...
			if (SRT.ResourceTableBits & (1 << BufferIndex))
			{
				PrepareUBResourceEntryGlobals(CCHeader, SRT.SamplerMap, ShaderInput.Environment.ResourceTableMap, BufferIndex, ParameterName, OutGlobalNames, OutHeader);
			}
		};

		const FVulkanShaderHeader::EType* FoundType = EntryTypes.Find(ParameterName);
		if (FoundType)
		{
			switch (*FoundType)
			{
			case FVulkanShaderHeader::Global:
				if (IsSamplerState(CCHeader, ParameterName))
				{
					int32 GlobalIndex = DoAddGlobal(ParameterName, OutHeader, OutGlobalNames);
					if (CCHeader.ExternalTextures.Contains(ParameterName))
					{
						AddImmutable(OutHeader, GlobalIndex);
					}
				}
				break;
			case FVulkanShaderHeader::UniformBuffer:
				AddGlobalNamesForUB();
				break;
			case FVulkanShaderHeader::PackedGlobal:
				if (bHasRealUBs || Size > 0)
				{
					// Ignore
				}
				else if (!bHasRealUBs)
				{
					AddGlobalNamesForUB();
				}
				break;
			default:
				check(0);
				break;
			}
		}
		else
		{
			AddGlobalNamesForUB();
		}
	}

	// Now input attachments
	for (int32 Index = 0; Index < BindingTable.InputAttachments.Num(); ++Index)
	{
		const FString& AttachmentName  = BindingTable.InputAttachments[Index];
		DoAddGlobal(AttachmentName, OutHeader, OutGlobalNames);
	}
}

static void ConvertToNEWHeader(FOLDVulkanCodeHeader& OLDHeader,
	const FVulkanBindingTable& BindingTable,
	const FSpirv& Spirv,
	const TMap<FString, FVulkanShaderHeader::EType>& EntryTypes,
	const FShaderCompilerInput& ShaderInput,
	FVulkanHlslccHeader& CCHeader,
	FShaderParameterMap& InOutParameterMap,
	FVulkanShaderHeader& OutHeader,
	bool bHasRealUBs)
{
	// Names that match the Header.Globals array
	TArray<FString> GlobalNames;

	TArray<FPatchType> TypePatchList;

	TArray<FString> ParameterNames;
	InOutParameterMap.GetAllParameterNames(ParameterNames);

	const FShaderCompilerResourceTable& SRT = OLDHeader.SerializedBindings.ShaderResourceTable;

	PrepareGlobals(BindingTable, CCHeader, SRT, EntryTypes, ShaderInput, ParameterNames, InOutParameterMap, GlobalNames, OutHeader, bHasRealUBs);

	for (int32 ParameterIndex = 0; ParameterIndex < ParameterNames.Num(); ++ParameterIndex)
	{
		uint16 BufferIndex;
		uint16 BaseIndex;
		uint16 Size;
		const FString& ParameterName = *ParameterNames[ParameterIndex];
		InOutParameterMap.FindParameterAllocation(*ParameterName, BufferIndex, BaseIndex, Size);

		const FVulkanShaderHeader::EType* FoundType = EntryTypes.Find(ParameterName);
		if (FoundType)
		{
			switch (*FoundType)
			{
			case FVulkanShaderHeader::Global:
				{
					int32 HeaderGlobalIndex = AddGlobal(OLDHeader, BindingTable, CCHeader, ParameterName, BaseIndex, Spirv, OutHeader, GlobalNames, TypePatchList, UINT16_MAX);

					const FParameterAllocation* ParameterAllocation = InOutParameterMap.GetParameterMap().Find(*ParameterName);
					check(ParameterAllocation);
					const EShaderParameterType ParamType = ParameterAllocation->Type;

					InOutParameterMap.RemoveParameterAllocation(*ParameterName);
					InOutParameterMap.AddParameterAllocation(*ParameterName, (uint16)FVulkanShaderHeader::Global, HeaderGlobalIndex, Size, ParamType);
				}
				break;
			case FVulkanShaderHeader::PackedGlobal:
				{
					if (bHasRealUBs || Size > 0)
					{
						int32 HeaderPackedGlobalIndex = OutHeader.PackedGlobals.AddZeroed();
						FVulkanShaderHeader::FPackedGlobalInfo& PackedGlobalInfo = OutHeader.PackedGlobals[HeaderPackedGlobalIndex];
						PackedGlobalInfo.PackedTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(OLDHeader.NEWPackedUBToVulkanBindingIndices[BufferIndex].TypeName);
						PackedGlobalInfo.PackedUBIndex = BufferIndex;
						check(Size > 0);
						PackedGlobalInfo.ConstantDataSizeInFloats = Size / sizeof(float);
#if VULKAN_ENABLE_SHADER_DEBUG_NAMES
						PackedGlobalInfo.DebugName = ParameterName;
#endif
						// Keep the original parameter info from InOutParameterMap as it's a shortcut into the packed global array!
					}
					else if (!bHasRealUBs)
					{
						check(Size == 0);
						check(EntryTypes.FindChecked(ParameterName) == FVulkanShaderHeader::PackedGlobal);
						// Ignore, this is an empty param (Size == 0) for an emulated UB, but keep the original parameter info
						// from InOutParameterMap as it's a shortcut into the packed global ub copies!
						AddUniformBuffer(OLDHeader, BindingTable, ShaderInput, CCHeader, Spirv, ParameterName, BufferIndex, InOutParameterMap, OutHeader, TypePatchList, GlobalNames, bHasRealUBs);
					}
					else
					{
						check(0);
					}
				}
				break;
			case FVulkanShaderHeader::UniformBuffer:
				check(bHasRealUBs);
				AddUniformBuffer(OLDHeader, BindingTable, ShaderInput, CCHeader, Spirv, ParameterName, BufferIndex, InOutParameterMap, OutHeader, TypePatchList, GlobalNames, bHasRealUBs);
				break;
			default:
				check(0);
				break;
			}
		}
		else
		{
			// Not found means it's a new resource-only UniformBuffer
			AddUniformBuffer(OLDHeader, BindingTable, ShaderInput, CCHeader, Spirv, ParameterName, BufferIndex, InOutParameterMap, OutHeader, TypePatchList, GlobalNames, bHasRealUBs);
		}
	}

	// Process the type patch list
	for (const FPatchType& Patch : TypePatchList)
	{
		check(OutHeader.Globals[Patch.HeaderGlobalIndex].TypeIndex == UINT16_MAX);
		OutHeader.Globals[Patch.HeaderGlobalIndex].TypeIndex = OutHeader.Globals[Patch.CombinedAliasIndex].TypeIndex;
	}

	// Add the packed global UBs
	for (int32 Index = 0; Index < OLDHeader.NEWPackedUBToVulkanBindingIndices.Num(); ++Index)
	{
		const FOLDVulkanCodeHeader::FPackedUBToVulkanBindingIndex& PackedArrayInfo = OLDHeader.NEWPackedUBToVulkanBindingIndices[Index];
		FVulkanShaderHeader::FPackedUBInfo& PackedUB = OutHeader.PackedUBs[OutHeader.PackedUBs.AddZeroed()];
		PackedUB.OriginalBindingIndex = PackedArrayInfo.VulkanBindingIndex;
		PackedUB.PackedTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(PackedArrayInfo.TypeName);
		PackedUB.SizeInBytes = OLDHeader.NEWPackedGlobalUBSizes[Index];

		const FSpirv::FEntry* Entry = Spirv.GetEntryByBindingIndex(PackedArrayInfo.VulkanBindingIndex);
		check(Entry);
		check(Entry->Name.StartsWith(TEXT("HLSLCC_CB")));
		PackedUB.SPIRVDescriptorSetOffset = Entry->WordDescriptorSetIndex;
		PackedUB.SPIRVBindingIndexOffset = Entry->WordBindingIndex;
	}

	// Finally check for subpass/input attachments
	if (BindingTable.InputAttachments.Num() > 0)
	{
		for (int32 Index = 0; Index < BindingTable.InputAttachments.Num(); ++Index)
		{
			const FString& AttachmentName  = BindingTable.InputAttachments[Index];
			const FVulkanBindingTable::FBinding* Found = BindingTable.GetBindings().FindByPredicate([&AttachmentName](const FVulkanBindingTable::FBinding& Entry)
				{
					return Entry.Name == AttachmentName;
				});
			check(Found);
			int32 BindingIndex = (int32)(Found - BindingTable.GetBindings().GetData());
			check(BindingIndex >= 0 && BindingIndex <= BindingTable.GetBindings().Num());
			const bool bIsFetch = AttachmentName == VULKAN_SUBPASS_FETCH_VAR_W;
			const bool bIsDepthFetch = AttachmentName == VULKAN_SUBPASS_DEPTH_FETCH_VAR_W;
			if (bIsFetch || bIsDepthFetch)
			{
				int32 HeaderGlobalIndex = GlobalNames.Find(AttachmentName);
				check(HeaderGlobalIndex != INDEX_NONE);
				check(GlobalNames[HeaderGlobalIndex] == AttachmentName);
				FVulkanShaderHeader::FGlobalInfo& GlobalInfo = OutHeader.Globals[HeaderGlobalIndex];
				const FSpirv::FEntry* Entry = Spirv.GetEntry(AttachmentName);
				check(Entry);
				check(Entry->Binding != -1);

				VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				GlobalInfo.OriginalBindingIndex = Entry->Binding;
				OutHeader.GlobalSpirvInfos[HeaderGlobalIndex] = FVulkanShaderHeader::FSpirvInfo(Entry->WordDescriptorSetIndex, Entry->WordBindingIndex);
				int32 GlobalDescriptorTypeIndex = OutHeader.GlobalDescriptorTypes.Add(DescriptorType);
				GlobalInfo.TypeIndex = GlobalDescriptorTypeIndex;
				GlobalInfo.CombinedSamplerStateAliasIndex = UINT16_MAX;
#if VULKAN_ENABLE_SHADER_DEBUG_NAMES
				GlobalInfo.DebugName = AttachmentName;
#endif
				int32 HeaderAttachmentIndex = OutHeader.InputAttachments.AddZeroed();
				check(HeaderAttachmentIndex == Index);
				FVulkanShaderHeader::FInputAttachment& AttachmentInfo = OutHeader.InputAttachments[HeaderAttachmentIndex];
				AttachmentInfo.GlobalIndex = HeaderGlobalIndex;
				ensure(bIsFetch != bIsDepthFetch);
				AttachmentInfo.Type = bIsFetch ? FVulkanShaderHeader::EAttachmentType::Color : FVulkanShaderHeader::EAttachmentType::Depth;
			}
			else
			{
				ensureMsgf(0, TEXT("Unknown Attachment name '%s'!"), *AttachmentName);
			}
		}
	}

	check(!bHasRealUBs || OLDHeader.UniformBuffersCopyInfo.Num() == 0);
	OutHeader.EmulatedUBsCopyInfo = OLDHeader.UniformBuffersCopyInfo;
	OutHeader.EmulatedUBCopyRanges = OLDHeader.NEWEmulatedUBCopyRanges;
	OutHeader.SourceHash = OLDHeader.SourceHash;
	OutHeader.SpirvCRC = Spirv.CRC;
#if VULKAN_ENABLE_SHADER_DEBUG_NAMES
	OutHeader.DebugName = OLDHeader.ShaderName;
#endif
	OutHeader.InOutMask = OLDHeader.SerializedBindings.InOutMask;
	OutHeader.bHasRealUBs = bHasRealUBs;
}


static void BuildShaderOutput(
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	const ANSICHAR* InShaderSource,
	int32 SourceLen,
	const FVulkanBindingTable& BindingTable,
	uint32 NumLines,
	FSpirv& Spirv,
	const FString& DebugName,
	bool bHasRealUBs
	)
{
	const ANSICHAR* USFSource = InShaderSource;
	FVulkanHlslccHeader CCHeader;
	if (!CCHeader.Read(USFSource, SourceLen))
	{
		UE_LOG(LogVulkanShaderCompiler, Error, TEXT("Bad hlslcc header found"));
		return;
	}

	if (*USFSource != '#')
	{
		UE_LOG(LogVulkanShaderCompiler, Error, TEXT("Bad hlslcc header found! Missing '#'!"));
		return;
	}

	FOLDVulkanCodeHeader OLDHeader;

	FShaderParameterMap& ParameterMap = ShaderOutput.ParameterMap;
	EShaderFrequency Frequency = (EShaderFrequency)ShaderOutput.Target.Frequency;

	TBitArray<> UsedUniformBufferSlots;
	UsedUniformBufferSlots.Init(false, 32);


	static const FString AttributePrefix = TEXT("in_ATTRIBUTE");
	static const FString GL_Prefix = TEXT("gl_");
	for (auto& Input : CCHeader.Inputs)
	{
		// Only process attributes for vertex shaders.
		if (Frequency == SF_Vertex && Input.Name.StartsWith(AttributePrefix))
		{
			int32 AttributeIndex = ParseNumber(*Input.Name + AttributePrefix.Len());
			OLDHeader.SerializedBindings.InOutMask |= (1 << AttributeIndex);
		}
#if 0
		// Record user-defined input varyings
		else if (!Input.Name.StartsWith(GL_Prefix))
		{
			FVulkanShaderVarying Var;
			Var.Location = Input.Index;
			Var.Varying = ParseIdentifierANSI(Input.Name);
			Var.Components = GetTypeComponents(Input.Type);
			Header.SerializedBindings.InputVaryings.Add(Var);
		}
#endif
	}

	static const FString TargetPrefix = "out_Target";
	static const FString GL_FragDepth = "gl_FragDepth";
	for (auto& Output : CCHeader.Outputs)
	{
		// Only targets for pixel shaders must be tracked.
		if (Frequency == SF_Pixel && Output.Name.StartsWith(TargetPrefix))
		{
			uint8 TargetIndex = ParseNumber(*Output.Name + TargetPrefix.Len());
			OLDHeader.SerializedBindings.InOutMask |= (1 << TargetIndex);
		}
		// Only depth writes for pixel shaders must be tracked.
		else if (Frequency == SF_Pixel && Output.Name.Equals(GL_FragDepth))
		{
			OLDHeader.SerializedBindings.InOutMask |= 0x8000;
		}
#if 0
		// Record user-defined output varyings
		else if (!Output.Name.StartsWith(GL_Prefix))
		{
			FVulkanShaderVarying Var;
			Var.Location = Output.Index;
			Var.Varying = ParseIdentifierANSI(Output.Name);
			Var.Components = GetTypeComponents(Output.Type);
			Header.SerializedBindings.OutputVaryings.Add(Var);
		}
#endif
	}


	TMap<FString, FVulkanShaderHeader::EType> NEWEntryTypes;

	// Then 'normal' uniform buffers.
	const FString CBPrefix = "HLSLCC_CB";
	for (auto& UniformBlock : CCHeader.UniformBlocks)
	{
		uint16 UBIndex = UniformBlock.Index;
		if (UniformBlock.Name.StartsWith(CBPrefix))
		{
			// Skip...
		}
		else
		{
			// Regular UB
			int32 VulkanBindingIndex = Spirv.FindBinding(UniformBlock.Name, true);
			check(VulkanBindingIndex != -1);
			check(!UsedUniformBufferSlots[VulkanBindingIndex]);
			UsedUniformBufferSlots[VulkanBindingIndex] = true;
			ParameterMap.AddParameterAllocation(*UniformBlock.Name, VulkanBindingIndex, 0, 0, EShaderParameterType::UniformBuffer);
			++OLDHeader.SerializedBindings.NumUniformBuffers;
			NEWEntryTypes.Add(*UniformBlock.Name, FVulkanShaderHeader::UniformBuffer);
		}
	}

	const TArray<FVulkanBindingTable::FBinding>& HlslccBindings = BindingTable.GetBindings();
	OLDHeader.NEWDescriptorInfo.NumBufferInfos = 0;
	OLDHeader.NEWDescriptorInfo.NumImageInfos = 0;
	for (int32 Index = 0; Index < HlslccBindings.Num(); ++Index)
	{
		const FVulkanBindingTable::FBinding& Binding = HlslccBindings[Index];

		OLDHeader.NEWDescriptorInfo.DescriptorTypes.Add(BindingToDescriptorType(Binding.Type));

		switch (Binding.Type)
		{
		case EVulkanBindingType::Sampler:
		case EVulkanBindingType::CombinedImageSampler:
		case EVulkanBindingType::Image:
		case EVulkanBindingType::StorageImage:
		case EVulkanBindingType::InputAttachment:
			++OLDHeader.NEWDescriptorInfo.NumImageInfos;
			break;
		case EVulkanBindingType::UniformBuffer:
		case EVulkanBindingType::StorageBuffer:
			++OLDHeader.NEWDescriptorInfo.NumBufferInfos;
			break;
		case EVulkanBindingType::PackedUniformBuffer:
			{
				FOLDVulkanCodeHeader::FPackedUBToVulkanBindingIndex* New = new(OLDHeader.NEWPackedUBToVulkanBindingIndices) FOLDVulkanCodeHeader::FPackedUBToVulkanBindingIndex;
				New->TypeName = (CrossCompiler::EPackedTypeName)Binding.SubType;
				New->VulkanBindingIndex = Index;
				++OLDHeader.NEWDescriptorInfo.NumBufferInfos;
			}
			break;
		case EVulkanBindingType::UniformTexelBuffer:
		case EVulkanBindingType::StorageTexelBuffer:
			break;
		default:
			checkf(0, TEXT("Binding Type %d not found"), (int32)Binding.Type);
			break;
		}
	}

	const uint16 BytesPerComponent = 4;

	// Packed global uniforms
	TMap<CrossCompiler::EPackedTypeName, uint32> PackedGlobalArraySize;
	for (auto& PackedGlobal : CCHeader.PackedGlobals)
	{
		int32 Found = -1;
		for (int32 Index = 0; Index < OLDHeader.NEWPackedUBToVulkanBindingIndices.Num(); ++Index)
		{
			if (OLDHeader.NEWPackedUBToVulkanBindingIndices[Index].TypeName == (CrossCompiler::EPackedTypeName)PackedGlobal.PackedType)
			{
				Found = Index;
				break;
			}
		}
		check(Found != -1);

		ParameterMap.AddParameterAllocation(
			*PackedGlobal.Name,
			Found,
			PackedGlobal.Offset * BytesPerComponent,
			PackedGlobal.Count * BytesPerComponent,
			EShaderParameterType::LooseData
			);
		NEWEntryTypes.Add(*PackedGlobal.Name, FVulkanShaderHeader::PackedGlobal);

		uint32& Size = PackedGlobalArraySize.FindOrAdd((CrossCompiler::EPackedTypeName)PackedGlobal.PackedType);
		Size = FMath::Max<uint32>(BytesPerComponent * (PackedGlobal.Offset + PackedGlobal.Count), Size);
	}

	// Packed Uniform Buffers
	TMap<int, TMap<CrossCompiler::EPackedTypeName, uint16> > PackedUniformBuffersSize;
	OLDHeader.UNUSED_NumNonGlobalUBs = 0;
	for (auto& PackedUB : CCHeader.PackedUBs)
	{
		//check(PackedUB.Attribute.Index == Header.SerializedBindings.NumUniformBuffers);
		check(!UsedUniformBufferSlots[OLDHeader.UNUSED_NumNonGlobalUBs]);
		UsedUniformBufferSlots[OLDHeader.UNUSED_NumNonGlobalUBs] = true;
		ParameterMap.AddParameterAllocation(*PackedUB.Attribute.Name, OLDHeader.UNUSED_NumNonGlobalUBs++, PackedUB.Attribute.Index, 0, EShaderParameterType::UniformBuffer);
		NEWEntryTypes.Add(PackedUB.Attribute.Name, FVulkanShaderHeader::PackedGlobal);
	}

	//#todo-rco: When using regular UBs, also set UsedUniformBufferSlots[] = 1

	// Remap the destination UB index into the packed global array index
	auto RemapDestIndexIntoPackedUB = [&OLDHeader](int8 DestUBTypeName)
	{
		for (int32 Index = 0; Index < OLDHeader.NEWPackedUBToVulkanBindingIndices.Num(); ++Index)
		{
			if (OLDHeader.NEWPackedUBToVulkanBindingIndices[Index].TypeName == (CrossCompiler::EPackedTypeName)DestUBTypeName)
			{
				return Index;
			}
		}
		check(0);
		return -1;
	};

	for (auto& PackedUBCopy : CCHeader.PackedUBCopies)
	{
		// Not used: For flattening each UB into its own packed array (not a global one)
		ensure(0);
		CrossCompiler::FUniformBufferCopyInfo CopyInfo;
		CopyInfo.SourceUBIndex = PackedUBCopy.SourceUB;
		CopyInfo.SourceOffsetInFloats = PackedUBCopy.SourceOffset;
		CopyInfo.DestUBTypeName = PackedUBCopy.DestPackedType;
		CopyInfo.DestUBIndex = RemapDestIndexIntoPackedUB(CopyInfo.DestUBTypeName);
		CopyInfo.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
		CopyInfo.DestOffsetInFloats = PackedUBCopy.DestOffset;
		CopyInfo.SizeInFloats = PackedUBCopy.Count;

		OLDHeader.UniformBuffersCopyInfo.Add(CopyInfo);

		auto& UniformBufferSize = PackedUniformBuffersSize.FindOrAdd(CopyInfo.DestUBIndex);
		uint16& Size = UniformBufferSize.FindOrAdd((CrossCompiler::EPackedTypeName)CopyInfo.DestUBTypeName);
		Size = FMath::Max<uint16>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);
	}

	for (auto& PackedUBCopy : CCHeader.PackedUBGlobalCopies)
	{
		ensure(!bHasRealUBs);
		CrossCompiler::FUniformBufferCopyInfo CopyInfo;
		CopyInfo.SourceUBIndex = PackedUBCopy.SourceUB;
		CopyInfo.SourceOffsetInFloats = PackedUBCopy.SourceOffset;
		CopyInfo.DestUBTypeName = PackedUBCopy.DestPackedType;
		CopyInfo.DestUBIndex = RemapDestIndexIntoPackedUB(CopyInfo.DestUBTypeName);
		CopyInfo.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
		CopyInfo.DestOffsetInFloats = PackedUBCopy.DestOffset;
		CopyInfo.SizeInFloats = PackedUBCopy.Count;

		OLDHeader.UniformBuffersCopyInfo.Add(CopyInfo);

		uint32& Size = PackedGlobalArraySize.FindOrAdd((CrossCompiler::EPackedTypeName)CopyInfo.DestUBTypeName);
		Size = FMath::Max<uint32>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);
	}

	// Generate a shortcut table for the PackedUBGlobalCopies
	TMap<uint32, uint32> PackedUBGlobalCopiesRanges;
	{
		int32 MaxDestUBIndex = -1;
		{
			// Verify table is sorted
			int32 PrevSourceUB = -1;
			int32 Index = 0;
			for (auto& Copy : OLDHeader.UniformBuffersCopyInfo)
			{
				if (PrevSourceUB < Copy.SourceUBIndex)
				{
					PrevSourceUB = Copy.SourceUBIndex;
					MaxDestUBIndex = FMath::Max(MaxDestUBIndex, (int32)Copy.SourceUBIndex);
					PackedUBGlobalCopiesRanges.Add(Copy.SourceUBIndex) = (Index << 16) | 1;
				}
				else if (PrevSourceUB == Copy.SourceUBIndex)
				{
					++PackedUBGlobalCopiesRanges.FindChecked(Copy.SourceUBIndex);
				}
				else
				{
					// Internal error
					check(0);
				}
				++Index;
			}
		}

		OLDHeader.NEWEmulatedUBCopyRanges.AddZeroed(MaxDestUBIndex + 1);
		for (int32 Index = 0; Index <= MaxDestUBIndex; ++Index)
		{
			uint32* Found = PackedUBGlobalCopiesRanges.Find(Index);
			if (Found)
			{
				OLDHeader.NEWEmulatedUBCopyRanges[Index] = *Found;
			}
		}
	}

	// Update Packed global array sizes
	OLDHeader.NEWPackedGlobalUBSizes.AddZeroed(OLDHeader.NEWPackedUBToVulkanBindingIndices.Num());
	for (auto& Pair : PackedGlobalArraySize)
	{
		CrossCompiler::EPackedTypeName TypeName = Pair.Key;
		int32 PackedArrayIndex = -1;
		for (int32 Index = 0; Index < OLDHeader.NEWPackedUBToVulkanBindingIndices.Num(); ++Index)
		{
			if (OLDHeader.NEWPackedUBToVulkanBindingIndices[Index].TypeName == TypeName)
			{
				PackedArrayIndex = Index;
				break;
			}
		}
		check(PackedArrayIndex != -1);
		// In bytes
		OLDHeader.NEWPackedGlobalUBSizes[PackedArrayIndex] = Align((uint32)Pair.Value, (uint32)16);
	}

	TSet<FString> SharedSamplerStates;
	for (int32 i = 0; i < CCHeader.SamplerStates.Num(); i++)
	{
		const FString& Name = CCHeader.SamplerStates[i].Name;
		int32 HlslccBindingIndex = Spirv.FindBinding(Name);
		check(HlslccBindingIndex != -1);

		SharedSamplerStates.Add(Name);
		auto& Binding = HlslccBindings[HlslccBindingIndex];
		int32 BindingIndex = Spirv.FindBinding(Binding.Name, true);
		check(BindingIndex != -1);
		ParameterMap.AddParameterAllocation(
			*Name,
			0,
			BindingIndex,
			1,
			EShaderParameterType::Sampler
		);
		NEWEntryTypes.Add(Name, FVulkanShaderHeader::Global);
	}

	for (auto& Sampler : CCHeader.Samplers)
	{
		int32 VulkanBindingIndex = Spirv.FindBinding(Sampler.Name, true);
		check(VulkanBindingIndex != -1);
		ParameterMap.AddParameterAllocation(
			*Sampler.Name,
			Sampler.Offset,
			VulkanBindingIndex,
			Sampler.Count,
			EShaderParameterType::SRV
			);
		NEWEntryTypes.Add(Sampler.Name, FVulkanShaderHeader::Global);

		OLDHeader.SerializedBindings.NumSamplers = FMath::Max<uint8>(
			OLDHeader.SerializedBindings.NumSamplers,
			Sampler.Offset + Sampler.Count
			);

		for (auto& SamplerState : Sampler.SamplerStates)
		{
			if (!SharedSamplerStates.Contains(SamplerState))
			{
				// ParameterMap does not use a TMultiMap, so we cannot push the same entry to it more than once!  if we try to, we've done something wrong...
				check(!ParameterMap.ContainsParameterAllocation(*SamplerState));
				ParameterMap.AddParameterAllocation(
					*SamplerState,
					Sampler.Offset,
					VulkanBindingIndex,
					Sampler.Count,
					EShaderParameterType::Sampler
				);
				NEWEntryTypes.Add(SamplerState, FVulkanShaderHeader::Global);
			}
		}
	}

	for (auto& UAV : CCHeader.UAVs)
	{
		int32 VulkanBindingIndex = Spirv.FindBinding(UAV.Name);
		check(VulkanBindingIndex != -1);

		ParameterMap.AddParameterAllocation(
			*UAV.Name,
			UAV.Offset,
			VulkanBindingIndex,
			UAV.Count,
			EShaderParameterType::UAV
			);
		NEWEntryTypes.Add(UAV.Name, FVulkanShaderHeader::Global);

		OLDHeader.SerializedBindings.NumUAVs = FMath::Max<uint8>(
			OLDHeader.SerializedBindings.NumUAVs,
			UAV.Offset + UAV.Count
			);
	}

	// Lats make sure that there is some type of name visible
	OLDHeader.ShaderName = CCHeader.Name.Len() > 0 ? CCHeader.Name : DebugName;

	FSHA1::HashBuffer(USFSource, FCStringAnsi::Strlen(USFSource), (uint8*)&OLDHeader.SourceHash);

	TArray<FString> OriginalParameters;
	ShaderOutput.ParameterMap.GetAllParameterNames(OriginalParameters);

	// Build the SRT for this shader.
	{
		// Build the generic SRT for this shader.
		FShaderCompilerResourceTable GenericSRT;
		if (!BuildResourceTableMapping(ShaderInput.Environment.ResourceTableMap, ShaderInput.Environment.ResourceTableLayoutHashes, UsedUniformBufferSlots, ShaderOutput.ParameterMap, /*MaxBoundResourceTable, */GenericSRT))
		{
			ShaderOutput.Errors.Add(TEXT("Internal error on BuildResourceTableMapping."));
			return;
		}

		// Copy over the bits indicating which resource tables are active.
		OLDHeader.SerializedBindings.ShaderResourceTable.ResourceTableBits = GenericSRT.ResourceTableBits;

		OLDHeader.SerializedBindings.ShaderResourceTable.ResourceTableLayoutHashes = GenericSRT.ResourceTableLayoutHashes;

		// Now build our token streams.
		BuildResourceTableTokenStream(GenericSRT.TextureMap, GenericSRT.MaxBoundResourceTable, OLDHeader.SerializedBindings.ShaderResourceTable.TextureMap, true);
		BuildResourceTableTokenStream(GenericSRT.ShaderResourceViewMap, GenericSRT.MaxBoundResourceTable, OLDHeader.SerializedBindings.ShaderResourceTable.ShaderResourceViewMap, true);
		BuildResourceTableTokenStream(GenericSRT.SamplerMap, GenericSRT.MaxBoundResourceTable, OLDHeader.SerializedBindings.ShaderResourceTable.SamplerMap, true);
		BuildResourceTableTokenStream(GenericSRT.UnorderedAccessViewMap, GenericSRT.MaxBoundResourceTable, OLDHeader.SerializedBindings.ShaderResourceTable.UnorderedAccessViewMap, true);
	}

	TArray<FString> NewParameters;
	ShaderOutput.ParameterMap.GetAllParameterNames(NewParameters);

	// Mark all used uniform buffer indices; however some are empty (eg GBuffers) so gather those as NewParameters
	OLDHeader.UniformBuffersWithDescriptorMask = *UsedUniformBufferSlots.GetData();
	uint16 NumParams = 0;
	for (int32 Index = NewParameters.Num() - 1; Index >= 0; --Index)
	{
		uint16 OutIndex, OutBase, OutSize;
		bool bFound = ShaderOutput.ParameterMap.FindParameterAllocation(*NewParameters[Index], OutIndex, OutBase, OutSize);
		ensure(bFound);
		NumParams = FMath::Max((uint16)(OutIndex + 1), NumParams);
		if (OriginalParameters.Contains(NewParameters[Index]))
		{
			NewParameters.RemoveAtSwap(Index, 1, false);
		}
	}

	// All newly added parameters are empty uniform buffers (with no constant data used), so no Vulkan Binding is required: remove from the mask
	for (int32 Index = 0; Index < NewParameters.Num(); ++Index)
	{
		uint16 OutIndex, OutBase, OutSize;
		ShaderOutput.ParameterMap.FindParameterAllocation(*NewParameters[Index], OutIndex, OutBase, OutSize);
		OLDHeader.UniformBuffersWithDescriptorMask = OLDHeader.UniformBuffersWithDescriptorMask & ~((uint64)1 << (uint64)OutIndex);
	}

	FVulkanShaderHeader NEWHeader(FVulkanShaderHeader::EZero);
	ConvertToNEWHeader(OLDHeader, BindingTable, Spirv, NEWEntryTypes, ShaderInput, CCHeader, ShaderOutput.ParameterMap, NEWHeader, bHasRealUBs);

	// Write out the header and shader source code.
	FMemoryWriter Ar(ShaderOutput.ShaderCode.GetWriteAccess(), true);
	Ar << NEWHeader;

	check(Spirv.Data.Num() != 0);
	Ar << Spirv.Data;

	// store data we can pickup later with ShaderCode.FindOptionalData('n'), could be removed for shipping
	// Daniel L: This GenerateShaderName does not generate a deterministic output among shaders as the shader code can be shared. 
	//			uncommenting this will cause the project to have non deterministic materials and will hurt patch sizes
	// ShaderOutput.ShaderCode.AddOptionalData('n', TCHAR_TO_UTF8(*ShaderInput.GenerateShaderName()));

	// Something to compare.
	ShaderOutput.NumInstructions = NumLines;
	ShaderOutput.NumTextureSamplers = OLDHeader.SerializedBindings.NumSamplers;
	ShaderOutput.bSucceeded = true;

	if (ShaderInput.ExtraSettings.bExtractShaderSource)
	{
		TArray<ANSICHAR> CodeOriginal;
		CodeOriginal.Append(USFSource, FCStringAnsi::Strlen(USFSource) + 1);
		ShaderOutput.OptionalFinalShaderSource = FString(CodeOriginal.GetData());
	}
	if (ShaderInput.ExtraSettings.OfflineCompilerPath.Len() > 0)
	{
		if (IsVulkanMobilePlatform((EShaderPlatform)ShaderInput.Target.Platform))
		{
			CompileOfflineMali(ShaderInput, ShaderOutput, (ANSICHAR *)Spirv.Data.GetData(), Spirv.Data.Num(), true);
		}
	}
}

//static void BuildShaderOutput(
//	FShaderCompilerOutput& ShaderOutput,
//	const FShaderCompilerInput& ShaderInput,
//	const ANSICHAR* InShaderSource,
//	int32 SourceLen,
//	const FVulkanBindingTable& BindingTable,
//	int32 SourceLenES,
//	const FString& SPVFile,
//	const FString& DebugName
//	)
//{
//	TArray<uint8> Spirv;
//	FFileHelper::LoadFileToArray(Spirv, *SPVFile);
//
//	BuildShaderOutput(
//		ShaderOutput,
//		ShaderInput,
//		InShaderSource,
//		SourceLen,
//		BindingTable,
//		SourceLenES,
//		Spirv,
//		DebugName
//		);
//}

static bool StringToFile(const FString& Filepath, const char* str)
{
	int32 StrLength = str ? FCStringAnsi::Strlen(str) : 0;

	if(StrLength == 0)
	{
		return false;
	}

	FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*Filepath);
	if (FileWriter)
	{
		// const cast...
		FileWriter->Serialize((void*)str, StrLength+1);
		FileWriter->Close();
		delete FileWriter;
	}

	return true;
}

static char* PatchGLSLVersionPosition(const char* InSourceGLSL)
{
	if(!InSourceGLSL)
	{
		return nullptr;
	}

	const int32 InSrcLength = FCStringAnsi::Strlen(InSourceGLSL);
	if(InSrcLength <= 0)
	{
		return nullptr;
	}

	char* GlslSource = (char*)malloc(InSrcLength+1);
	check(GlslSource);
	memcpy(GlslSource, InSourceGLSL, InSrcLength+1);

	// Find begin of "#version" line
	char* VersionBegin = strstr(GlslSource, "#version");

	// Find end of "#version line"
	char* VersionEnd = VersionBegin ? strstr(VersionBegin, "\n") : nullptr;

	if(VersionEnd)
	{
		// Add '\n' character
		VersionEnd++;

		const int32 VersionLineLength = VersionEnd - VersionBegin - 1;

		// Copy version line into a temporary buffer (+1 for term-char).
		const int32 TmpStrBytes = (VersionEnd - VersionBegin) + 1;
		char* TmpVersionLine = (char*)malloc(TmpStrBytes);
		check(TmpVersionLine);
		memset(TmpVersionLine, 0, TmpStrBytes);
		memcpy(TmpVersionLine, VersionBegin, VersionEnd - VersionBegin);

		// Erase current version number, just replace it with spaces...
		for(char* str=VersionBegin; str<(VersionEnd-1); str++)
		{
			*str=' ';
		}

		// Allocate new source buffer to place version string on the first line.
		char* NewSource = (char*)malloc(InSrcLength + TmpStrBytes);
		check(NewSource);

		// Copy version line
		memcpy(NewSource, TmpVersionLine, TmpStrBytes);

		// Copy original source after the source line
		// -1 to offset back from the term-char
		memcpy(NewSource + TmpStrBytes - 1, GlslSource, InSrcLength + 1);

		free(TmpVersionLine);
		TmpVersionLine = nullptr;
							
		// Update string pointer
		free(GlslSource);
		GlslSource = NewSource;
	}

	return GlslSource;
}

static void PatchForToWhileLoop(char** InOutSourceGLSL)
{
	//checkf(InOutSourceGLSL, TEXT("Attempting to patch an invalid glsl source-string"));

	char* srcGlsl = *InOutSourceGLSL;
	//checkf(srcGlsl, TEXT("Attempting to patch an invalid glsl source-string"));

	const size_t InSrcLength = strlen(srcGlsl);
	//checkf(InSrcLength > 0, TEXT("Attempting to patch an empty glsl source-string."));

	// This is what we are relacing
	const char* srcPatchable = "for (;;)";
	const size_t srcPatchableLength = strlen(srcPatchable);

	// This is where we are replacing with
	const char* dstPatchable = "while(true)";
	const size_t dstPatchableLength = strlen(dstPatchable);
	
	// Find number of occurances
	int numNumberOfOccurances = 0;
	for(char* dstReplacePos = strstr(srcGlsl, srcPatchable);
		dstReplacePos != NULL;
		dstReplacePos = strstr(dstReplacePos+srcPatchableLength, srcPatchable))
	{
		numNumberOfOccurances++;
	}

	// No patching needed
	if(numNumberOfOccurances == 0)
	{
		return;
	}

	// Calc new required string-length
	const size_t newLength = InSrcLength + (dstPatchableLength-srcPatchableLength)*numNumberOfOccurances;

	// Allocate destination buffer + 1 char for terminating character
	char* GlslSource = (char*)malloc(newLength+1);
	check(GlslSource)
	memset(GlslSource, 0, sizeof(char)*(newLength+1));
	memcpy(GlslSource, srcGlsl, InSrcLength);

	// Scan and replace
	char* dstReplacePos = strstr(GlslSource, srcPatchable);
	char* srcReplacePos = strstr(srcGlsl, srcPatchable);
	int bytesRemaining = (int)newLength;
	while(dstReplacePos != NULL && srcReplacePos != NULL)
	{
		// Replace the string
		bytesRemaining = (int)newLength - (int)(dstReplacePos - GlslSource);
		memcpy(dstReplacePos, dstPatchable, dstPatchableLength);
		
		// Increment positions
		dstReplacePos+=dstPatchableLength;
		srcReplacePos+=srcPatchableLength;

		// Append remaining code
		int bytesToCopy = InSrcLength - (int)(srcReplacePos - srcGlsl);
		memcpy(dstReplacePos, srcReplacePos, bytesToCopy);

		dstReplacePos = strstr(dstReplacePos, srcPatchable);
		srcReplacePos = strstr(srcReplacePos, srcPatchable);
	}

	free(*InOutSourceGLSL);

	*InOutSourceGLSL = GlslSource;
}

static FString CreateShaderCompileCommandLine(FCompilerInfo& CompilerInfo, EHlslCompileTarget Target)
{
	//const FString OutputFileNoExt = FPaths::GetBaseFilename(OutputFile);
	FString CmdLine;
	FString GLSLFile = CompilerInfo.Input.DumpDebugInfoPath / (TEXT("Output") + GetExtension(CompilerInfo.Frequency));
	FString SPVFile = CompilerInfo.Input.DumpDebugInfoPath / TEXT("Output.spv");
	FString SPVDisasmFile = CompilerInfo.Input.DumpDebugInfoPath / TEXT("Output.spvasm");

	CmdLine += TEXT("\n\"");
#if PLATFORM_WINDOWS
	CmdLine += *(FPaths::RootDir() / TEXT("Engine/Binaries/ThirdParty/glslang/glslangValidator.exe"));
#elif PLATFORM_LINUX
	CmdLine += *(FPaths::RootDir() / TEXT("Engine/Binaries/ThirdParty/glslang/glslangValidator"));
#endif
	CmdLine += TEXT("\"");
	CmdLine += TEXT(" -V -H -r -o \"") + SPVFile + TEXT("\" \"") + GLSLFile + TEXT("\" > \"" + SPVDisasmFile + "\"");
	CmdLine += TEXT("\npause\n");

	return CmdLine;
}


FCompilerInfo::FCompilerInfo(const FShaderCompilerInput& InInput, const FString& InWorkingDirectory, EHlslShaderFrequency InFrequency) :
	Input(InInput),
	WorkingDirectory(InWorkingDirectory),
	CCFlags(0),
	Frequency(InFrequency),
	bDebugDump(false)
{
	bDebugDump = Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath);
	BaseSourceFilename = Input.GetSourceFilename();
}

/**
 * Compile a shader using the internal shader compiling library
 */
static bool CompileUsingInternal(FCompilerInfo& CompilerInfo, FVulkanBindingTable& BindingTable, TArray<ANSICHAR>& GlslSource, FString& EntryPointName, FShaderCompilerOutput& Output, bool bHasRealUBs)
{
	FString Errors;
	FSpirv Spirv;
	const ANSICHAR* Main = GlslSource.GetData();
	Main = FCStringAnsi::Strstr(Main, "void main_");
	check(Main);
	auto GetNumEOLs = [](const ANSICHAR* Ptr)
	{
		uint32 NumLines = 0;
		while (*Ptr)
		{
			if (*Ptr == '\n')
			{
				++NumLines;
			}
			++Ptr;
		}

		return NumLines;
	};
	uint32 NumLines = GetNumEOLs(Main);
	if (GenerateSpirv(GlslSource.GetData(), CompilerInfo, Errors, CompilerInfo.Input.DumpDebugInfoPath, Spirv))
	{
		FString DebugName = CompilerInfo.Input.DumpDebugInfoPath.Right(CompilerInfo.Input.DumpDebugInfoPath.Len() - CompilerInfo.Input.DumpDebugInfoRootPath.Len());

		Output.Target = CompilerInfo.Input.Target;
		BuildShaderOutput(Output, CompilerInfo.Input,
			GlslSource.GetData(), GlslSource.Num(),
			BindingTable, NumLines, Spirv, DebugName, bHasRealUBs);
		return true;
	}
	else
	{
		if (Errors.Len() > 0)
		{
			FShaderCompilerError* Error = new(Output.Errors) FShaderCompilerError();
			Error->ErrorLineString = Errors;
		}

		return false;
	}
}


static bool CallHlslcc(const FString& PreprocessedShader, FVulkanBindingTable& BindingTable, FCompilerInfo& CompilerInfo, FString& EntryPointName, EHlslCompileTarget HlslCompilerTarget, FShaderCompilerOutput& Output, TArray<ANSICHAR>& OutGlsl)
{
	char* GlslShaderSource = nullptr;
	char* ErrorLog = nullptr;

	auto InnerFunction = [&]()
	{
		// Call hlslcc
		FVulkanCodeBackend VulkanBackend(CompilerInfo.CCFlags, BindingTable, HlslCompilerTarget);
		FHlslCrossCompilerContext CrossCompilerContext(CompilerInfo.CCFlags, CompilerInfo.Frequency, HlslCompilerTarget);
		const bool bShareSamplers = false;
		FVulkanLanguageSpec VulkanLanguageSpec(true);
		int32 Result = 0;
		if (CrossCompilerContext.Init(TCHAR_TO_ANSI(*CompilerInfo.Input.VirtualSourceFilePath), &VulkanLanguageSpec))
		{
			Result = CrossCompilerContext.Run(
				TCHAR_TO_ANSI(*PreprocessedShader),
				TCHAR_TO_ANSI(*EntryPointName),
				&VulkanBackend,
				&GlslShaderSource,
				&ErrorLog
				) ? 1 : 0;
		}

		if (Result == 0)
		{
			FString Tmp = ANSI_TO_TCHAR(ErrorLog);
			TArray<FString> ErrorLines;
			Tmp.ParseIntoArray(ErrorLines, TEXT("\n"), true);
			for (int32 LineIndex = 0; LineIndex < ErrorLines.Num(); ++LineIndex)
			{
				const FString& Line = ErrorLines[LineIndex];
				CrossCompiler::ParseHlslccError(Output.Errors, Line, CompilerInfo.Input.bSkipPreprocessedCache);
			}

			return false;
		}

		check(GlslShaderSource);

		// Patch GLSL source
		PatchForToWhileLoop(&GlslShaderSource);

		if (CompilerInfo.bDebugDump)
		{
			FString DumpedGlslFile = CompilerInfo.Input.DumpDebugInfoPath / (TEXT("Output") + GetExtension(CompilerInfo.Frequency));
			FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*DumpedGlslFile);
			if (FileWriter)
			{
				FileWriter->Serialize(GlslShaderSource, FCStringAnsi::Strlen(GlslShaderSource));
				FileWriter->Close();
				delete FileWriter;
			}
		}

		int32 Length = FCStringAnsi::Strlen(GlslShaderSource);
		OutGlsl.AddUninitialized(Length + 1);
		FCStringAnsi::Strcpy(OutGlsl.GetData(), Length + 1, GlslShaderSource);

		return true;
	};

	bool bResult = InnerFunction();

	if (ErrorLog)
	{
		free(ErrorLog);
	}

	if (GlslShaderSource)
	{
		free(GlslShaderSource);
	}

	return bResult;
}


void DoCompileVulkanShader(const FShaderCompilerInput& Input, FShaderCompilerOutput& Output, const class FString& WorkingDirectory, EVulkanShaderVersion Version)
{
	const EShaderPlatform ShaderPlatform = (EShaderPlatform)Input.Target.Platform;
	check(IsVulkanPlatform(ShaderPlatform));

	//if (GUseExternalShaderCompiler)
	//{
	//	// Old path...
	//	CompileUsingExternal(Input, Output, WorkingDirectory, Version);
	//	return;
	//}

	const bool bHasRealUBs = HasRealUBs(Version);
	const bool bIsSM5 = (Version == EVulkanShaderVersion::SM5 || Version == EVulkanShaderVersion::SM5_NOUB);
	const bool bIsSM4 = (Version == EVulkanShaderVersion::SM4 || Version == EVulkanShaderVersion::SM4_NOUB);

	const EHlslShaderFrequency FrequencyTable[] =
	{
		HSF_VertexShader,
		bIsSM5 ? HSF_HullShader : HSF_InvalidFrequency,
		bIsSM5 ? HSF_DomainShader : HSF_InvalidFrequency,
		HSF_PixelShader,
		(bIsSM4 || bIsSM5) ? HSF_GeometryShader : HSF_InvalidFrequency,
		RHISupportsComputeShaders(ShaderPlatform) ? HSF_ComputeShader : HSF_InvalidFrequency
	};

	const EHlslShaderFrequency Frequency = FrequencyTable[Input.Target.Frequency];
	if (Frequency == HSF_InvalidFrequency)
	{
		Output.bSucceeded = false;
		FShaderCompilerError* NewError = new(Output.Errors) FShaderCompilerError();
		NewError->StrippedErrorMessage = FString::Printf(
			TEXT("%s shaders not supported for use in Vulkan."),
			CrossCompiler::GetFrequencyName((EShaderFrequency)Input.Target.Frequency));
		return;
	}

	FString PreprocessedShader;
	FShaderCompilerDefinitions AdditionalDefines;
	EHlslCompileTarget HlslCompilerTarget = HCT_FeatureLevelES3_1Ext;
	EHlslCompileTarget HlslCompilerTargetES = HCT_FeatureLevelES3_1Ext;
	AdditionalDefines.SetDefine(TEXT("COMPILER_HLSLCC"), 1);
	AdditionalDefines.SetDefine(TEXT("COMPILER_VULKAN"), 1);
	if (Version == EVulkanShaderVersion::ES3_1 || Version == EVulkanShaderVersion::ES3_1_ANDROID || Version == EVulkanShaderVersion::ES3_1_NOUB || Version == EVulkanShaderVersion::ES3_1_ANDROID_NOUB)
	{
		HlslCompilerTarget = HCT_FeatureLevelES3_1Ext;
		HlslCompilerTargetES = HCT_FeatureLevelES3_1Ext;
		AdditionalDefines.SetDefine(TEXT("ES2_PROFILE"), 1);
		AdditionalDefines.SetDefine(TEXT("VULKAN_PROFILE"), 1);
	}
	else if (bIsSM4)
	{
		HlslCompilerTarget = HCT_FeatureLevelSM4;
		HlslCompilerTargetES = HCT_FeatureLevelSM4;
		AdditionalDefines.SetDefine(TEXT("VULKAN_PROFILE_SM4"), 1);
	}
	else if (bIsSM5)
	{
		HlslCompilerTarget = HCT_FeatureLevelSM5;
		HlslCompilerTargetES = HCT_FeatureLevelSM5;
		AdditionalDefines.SetDefine(TEXT("VULKAN_PROFILE_SM5"), 1);
	}
	AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));

	AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)1);

	const bool bUseFullPrecisionInPS = Input.Environment.CompilerFlags.Contains(CFLAG_UseFullPrecisionInPS);
	if (bUseFullPrecisionInPS)
	{
		AdditionalDefines.SetDefine(TEXT("FORCE_FLOATS"), (uint32)1);
	}

	//#todo-rco: Glslang doesn't allow this yet
	AdditionalDefines.SetDefine(TEXT("noperspective"), TEXT(""));

	// Preprocess the shader.
	FString PreprocessedShaderSource;
	const bool bDirectCompile = FParse::Param(FCommandLine::Get(), TEXT("directcompile"));
	if (bDirectCompile)
	{
		if (!FFileHelper::LoadFileToString(PreprocessedShaderSource, *Input.VirtualSourceFilePath))
		{
			return;
		}

		// Remove const as we are on debug-only mode
		CrossCompiler::CreateEnvironmentFromResourceTable(PreprocessedShaderSource, (FShaderCompilerEnvironment&)Input.Environment);
	}
	else
	{
		if (!PreprocessShader(PreprocessedShaderSource, Output, Input, AdditionalDefines))
		{
			// The preprocessing stage will add any relevant errors.
			return;
		}

		// Disable instanced stereo until supported for Vulkan
		StripInstancedStereo(PreprocessedShaderSource);
	}

	FString EntryPointName = Input.EntryPointName;

	RemoveUniformBuffersFromSource(Input.Environment, PreprocessedShaderSource);

	FCompilerInfo CompilerInfo(Input, WorkingDirectory, Frequency);

	CompilerInfo.CCFlags |= HLSLCC_PackUniforms;
	CompilerInfo.CCFlags |= HLSLCC_PackUniformsIntoUniformBuffers;
	if (bHasRealUBs)
	{
		// Only flatten structures inside UBs
		CompilerInfo.CCFlags |= HLSLCC_FlattenUniformBufferStructures;
	}
	else
	{
		// Flatten ALL UBs
		CompilerInfo.CCFlags |= HLSLCC_FlattenUniformBuffers | HLSLCC_ExpandUBMemberArrays;
	}

	if (bUseFullPrecisionInPS)
	{
		CompilerInfo.CCFlags |= HLSLCC_UseFullPrecisionInPS;
	}

	CompilerInfo.CCFlags |= HLSLCC_SeparateShaderObjects;
	CompilerInfo.CCFlags |= HLSLCC_KeepSamplerAndImageNames;

	CompilerInfo.CCFlags |= HLSLCC_RetainSizes;

	// ES doesn't support origin layout
	CompilerInfo.CCFlags |= HLSLCC_DX11ClipSpace;

	// Required as we added the RemoveUniformBuffersFromSource() function (the cross-compiler won't be able to interpret comments w/o a preprocessor)
	CompilerInfo.CCFlags &= ~HLSLCC_NoPreprocess;

	if (!bDirectCompile || UE_BUILD_DEBUG)
	{
		// Validation is expensive - only do it when compiling directly for debugging
		CompilerInfo.CCFlags |= HLSLCC_NoValidation;
	}

	// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
	if (CompilerInfo.bDebugDump)
	{
		FString DumpedUSFFile = CompilerInfo.Input.DumpDebugInfoPath / CompilerInfo.BaseSourceFilename;
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*DumpedUSFFile);
		if (FileWriter)
		{
			auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShaderSource);
			FileWriter->Serialize((ANSICHAR*)AnsiSourceFile.Get(), AnsiSourceFile.Length());
			{
				FString Line = CrossCompiler::CreateResourceTableFromEnvironment(Input.Environment);

				Line += TEXT("#if 0 /*DIRECT COMPILE*/\n");
				Line += CreateShaderCompilerWorkerDirectCommandLine(Input, CompilerInfo.CCFlags);
				Line += TEXT("\n#endif /*DIRECT COMPILE*/\n");

				FileWriter->Serialize(TCHAR_TO_ANSI(*Line), Line.Len());
			}
			FileWriter->Close();
			delete FileWriter;
		}

		const FString BatchFileContents = CreateShaderCompileCommandLine(CompilerInfo, HlslCompilerTarget);
		FFileHelper::SaveStringToFile(BatchFileContents, *(CompilerInfo.Input.DumpDebugInfoPath / TEXT("CompileSPIRV.bat")));
	}

	TArray<ANSICHAR> GeneratedGlslSource;
	FVulkanBindingTable BindingTable(CompilerInfo.Frequency);
	bool bSuccess = false;

	if (CallHlslcc(PreprocessedShaderSource, BindingTable, CompilerInfo, EntryPointName, HlslCompilerTarget, Output, GeneratedGlslSource))
	{
		//#todo-rco: Once it's all cleaned up...
		//if (GUseExternalShaderCompiler)
		//{
		//	CompileUsingExternal(CompilerInfo, BindingTable, GeneratedGlslSource, EntryPointName, Output);
		//}
		//else
		{
			// For debugging: if you hit an error from Glslang/Spirv, use the SourceNoHeader for line numbers
			auto* SourceWithHeader = GeneratedGlslSource.GetData();
			char* SourceNoHeader = strstr(SourceWithHeader, "#version");
			bSuccess = CompileUsingInternal(CompilerInfo, BindingTable, GeneratedGlslSource, EntryPointName, Output, bHasRealUBs);
			if (bDirectCompile)
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Success: %d\n%s\n"), bSuccess, ANSI_TO_TCHAR(SourceWithHeader));
			}
		}
	}
	
	if (bDirectCompile)
	{
		for (const auto& Error : Output.Errors)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s\n"), *Error.GetErrorString());
		}
		ensure(bSuccess);
	}
}
