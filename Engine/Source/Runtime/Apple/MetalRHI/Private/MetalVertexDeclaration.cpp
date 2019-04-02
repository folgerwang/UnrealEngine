// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalVertexDeclaration.cpp: Metal vertex declaration RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"

mtlpp::VertexFormat GMetalFColorVertexFormat = mtlpp::VertexFormat::UChar4Normalized;

static mtlpp::VertexFormat TranslateElementTypeToMTLType(EVertexElementType Type)
{
	switch (Type)
	{
		case VET_Float1:		return mtlpp::VertexFormat::Float;
		case VET_Float2:		return mtlpp::VertexFormat::Float2;
		case VET_Float3:		return mtlpp::VertexFormat::Float3;
		case VET_Float4:		return mtlpp::VertexFormat::Float4;
		case VET_PackedNormal:	return mtlpp::VertexFormat::Char4Normalized;
		case VET_UByte4:		return mtlpp::VertexFormat::UChar4;
		case VET_UByte4N:		return mtlpp::VertexFormat::UChar4Normalized;
		case VET_Color:			return GMetalFColorVertexFormat;
		case VET_Short2:		return mtlpp::VertexFormat::Short2;
		case VET_Short4:		return mtlpp::VertexFormat::Short4;
		case VET_Short2N:		return mtlpp::VertexFormat::Short2Normalized;
		case VET_Half2:			return mtlpp::VertexFormat::Half2;
		case VET_Half4:			return mtlpp::VertexFormat::Half4;
		case VET_Short4N:		return mtlpp::VertexFormat::Short4Normalized;
		case VET_UShort2:		return mtlpp::VertexFormat::UShort2;
		case VET_UShort4:		return mtlpp::VertexFormat::UShort4;
		case VET_UShort2N:		return mtlpp::VertexFormat::UShort2Normalized;
		case VET_UShort4N:		return mtlpp::VertexFormat::UShort4Normalized;
		case VET_URGB10A2N:		return mtlpp::VertexFormat::UInt1010102Normalized;
		case VET_UInt:			return mtlpp::VertexFormat::UInt;
		default:				UE_LOG(LogMetal, Fatal, TEXT("Unknown vertex element type!")); return mtlpp::VertexFormat::Float;
	};

}

uint32 TranslateElementTypeToSize(EVertexElementType Type)
{
	switch (Type)
	{
		case VET_Float1:		return 4;
		case VET_Float2:		return 8;
		case VET_Float3:		return 12;
		case VET_Float4:		return 16;
		case VET_PackedNormal:	return 4;
		case VET_UByte4:		return 4;
		case VET_UByte4N:		return 4;
		case VET_Color:			return 4;
		case VET_Short2:		return 4;
		case VET_Short4:		return 8;
		case VET_UShort2:		return 4;
		case VET_UShort4:		return 8;
		case VET_Short2N:		return 4;
		case VET_UShort2N:		return 4;
		case VET_Half2:			return 4;
		case VET_Half4:			return 8;
		case VET_Short4N:		return 8;
		case VET_UShort4N:		return 8;
		case VET_URGB10A2N:		return 4;
		case VET_UInt:			return 4;
		default:				UE_LOG(LogMetal, Fatal, TEXT("Unknown vertex element type!")); return 0;
	};
}

FMetalHashedVertexDescriptor::FMetalHashedVertexDescriptor()
: VertexDescHash(0)
, VertexDesc(nil)
{
}

FMetalHashedVertexDescriptor::FMetalHashedVertexDescriptor(mtlpp::VertexDescriptor Desc, uint32 Hash)
: VertexDescHash(Hash)
, VertexDesc(Desc)
{
}

FMetalHashedVertexDescriptor::FMetalHashedVertexDescriptor(FMetalHashedVertexDescriptor const& Other)
: VertexDescHash(0)
, VertexDesc(nil)
{
	operator=(Other);
}

FMetalHashedVertexDescriptor::~FMetalHashedVertexDescriptor()
{
}

FMetalHashedVertexDescriptor& FMetalHashedVertexDescriptor::operator=(FMetalHashedVertexDescriptor const& Other)
{
	if (this != &Other)
	{
		VertexDescHash = Other.VertexDescHash;
		VertexDesc = Other.VertexDesc;
	}
	return *this;
}

bool FMetalHashedVertexDescriptor::operator==(FMetalHashedVertexDescriptor const& Other) const
{
	bool bEqual = false;
	if (this != &Other)
	{
		if (VertexDescHash == Other.VertexDescHash)
		{
			bEqual = true;
			if (VertexDesc.GetPtr() != Other.VertexDesc.GetPtr())
			{
				ns::Array<mtlpp::VertexBufferLayoutDescriptor> Layouts = VertexDesc.GetLayouts();
				ns::Array<mtlpp::VertexAttributeDescriptor> Attributes = VertexDesc.GetAttributes();
				
				ns::Array<mtlpp::VertexBufferLayoutDescriptor> OtherLayouts = Other.VertexDesc.GetLayouts();
				ns::Array<mtlpp::VertexAttributeDescriptor> OtherAttributes = Other.VertexDesc.GetAttributes();
				check(Layouts && Attributes && OtherLayouts && OtherAttributes);
				
				for (uint32 i = 0; bEqual && i < MaxVertexElementCount; i++)
				{
					mtlpp::VertexBufferLayoutDescriptor LayoutDesc = Layouts[(NSUInteger)i];
					mtlpp::VertexBufferLayoutDescriptor OtherLayoutDesc = OtherLayouts[(NSUInteger)i];
					
					bEqual &= ((LayoutDesc != nil) == (OtherLayoutDesc != nil));
					
					if (LayoutDesc && OtherLayoutDesc)
					{
						bEqual &= (LayoutDesc.GetStride() == OtherLayoutDesc.GetStride());
						bEqual &= (LayoutDesc.GetStepFunction() == OtherLayoutDesc.GetStepFunction());
						bEqual &= (LayoutDesc.GetStepRate() == OtherLayoutDesc.GetStepRate());
					}
					
					mtlpp::VertexAttributeDescriptor AttrDesc = Attributes[(NSUInteger)i];
					mtlpp::VertexAttributeDescriptor OtherAttrDesc = OtherAttributes[(NSUInteger)i];
					
					bEqual &= ((AttrDesc != nil) == (OtherAttrDesc != nil));
					
					if (AttrDesc && OtherAttrDesc)
					{
						bEqual &= (AttrDesc.GetFormat() == OtherAttrDesc.GetFormat());
						bEqual &= (AttrDesc.GetOffset() == OtherAttrDesc.GetOffset());
						bEqual &= (AttrDesc.GetBufferIndex() == OtherAttrDesc.GetBufferIndex());
					}
				}
			}
		}
	}
	else
	{
		bEqual = true;
	}
	return bEqual;
}

FMetalVertexDeclaration::FMetalVertexDeclaration(const FVertexDeclarationElementList& InElements)
	: Elements(InElements)
	, BaseHash(0)
{
	GenerateLayout(InElements);
}

FMetalVertexDeclaration::~FMetalVertexDeclaration()
{
}

FVertexDeclarationRHIRef FMetalDynamicRHI::RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	@autoreleasepool {
	uint32 Key = FCrc::MemCrc32(Elements.GetData(), Elements.Num() * sizeof(FVertexElement));
	// look up an existing declaration
	FVertexDeclarationRHIRef* VertexDeclarationRefPtr = VertexDeclarationCache.Find(Key);
	if (VertexDeclarationRefPtr == NULL)
	{
//		NSLog(@"VertDecl Key: %x", Key);

		// create and add to the cache if it doesn't exist.
		VertexDeclarationRefPtr = &VertexDeclarationCache.Add(Key, new FMetalVertexDeclaration(Elements));
	}

	return *VertexDeclarationRefPtr;
	}
}

void FMetalVertexDeclaration::GenerateLayout(const FVertexDeclarationElementList& InElements)
{
	mtlpp::VertexDescriptor NewLayout;
	
	ns::Array<mtlpp::VertexBufferLayoutDescriptor> Layouts = NewLayout.GetLayouts();
	ns::Array<mtlpp::VertexAttributeDescriptor> Attributes = NewLayout.GetAttributes();

	BaseHash = 0;
	uint32 StrideHash = BaseHash;

	TMap<uint32, uint32> BufferStrides;
	for (uint32 ElementIndex = 0; ElementIndex < InElements.Num(); ElementIndex++)
	{
		const FVertexElement& Element = InElements[ElementIndex];
		
		checkf(Element.Stride == 0 || Element.Offset + TranslateElementTypeToSize(Element.Type) <= Element.Stride, 
			TEXT("Stream component is bigger than stride: Offset: %d, Size: %d [Type %d], Stride: %d"), Element.Offset, TranslateElementTypeToSize(Element.Type), (uint32)Element.Type, Element.Stride);

		BaseHash = FCrc::MemCrc32(&Element.StreamIndex, sizeof(Element.StreamIndex), BaseHash);
		BaseHash = FCrc::MemCrc32(&Element.Offset, sizeof(Element.Offset), BaseHash);
		BaseHash = FCrc::MemCrc32(&Element.Type, sizeof(Element.Type), BaseHash);
		BaseHash = FCrc::MemCrc32(&Element.AttributeIndex, sizeof(Element.AttributeIndex), BaseHash);
		
		uint32 Stride = Element.Stride;
		StrideHash = FCrc::MemCrc32(&Stride, sizeof(Stride), StrideHash);

		// Vertex & Constant buffers are set up in the same space, so add VB's from the top
		uint32 ShaderBufferIndex = UNREAL_TO_METAL_BUFFER_INDEX(Element.StreamIndex);

		// track the buffer stride, making sure all elements with the same buffer have the same stride
		uint32* ExistingStride = BufferStrides.Find(ShaderBufferIndex);
		if (ExistingStride == NULL)
		{
			// handle 0 stride buffers
			mtlpp::VertexStepFunction Function = (Element.Stride == 0 ? mtlpp::VertexStepFunction::Constant : (Element.bUseInstanceIndex ? mtlpp::VertexStepFunction::PerInstance : mtlpp::VertexStepFunction::PerVertex));
			uint32 StepRate = (Element.Stride == 0 ? 0 : 1);

			// even with MTLVertexStepFunctionConstant, it needs a non-zero stride (not sure why)
			if (Element.Stride == 0)
			{
				Stride = TranslateElementTypeToSize(Element.Type);
			}
						
			// look for any unset strides coming from UE4 (this can be removed when all are fixed)
			if (Element.Stride == 0xFFFF)
			{
				NSLog(@"Setting illegal stride - break here if you want to find out why, but this won't break until we try to render with it");
				Stride = 200;
			}

			// set the stride once per buffer
			mtlpp::VertexBufferLayoutDescriptor VBLayout = Layouts[ShaderBufferIndex];
			VBLayout.SetStride(Stride);
			VBLayout.SetStepFunction(Function);
			VBLayout.SetStepRate(StepRate);

			// track this buffer and stride
			BufferStrides.Add(ShaderBufferIndex, Element.Stride);
		}
		else
		{
			// if the strides of elements with same buffer index have different strides, something is VERY wrong
			check(Element.Stride == *ExistingStride);
		}

		// set the format for each element
		mtlpp::VertexAttributeDescriptor Attrib = Attributes[Element.AttributeIndex];
		Attrib.SetFormat(TranslateElementTypeToMTLType(Element.Type));
		Attrib.SetOffset(Element.Offset);
		Attrib.SetBufferIndex(ShaderBufferIndex);
	}
	
	Layout = FMetalHashedVertexDescriptor(NewLayout, HashCombine(BaseHash, StrideHash));
}
