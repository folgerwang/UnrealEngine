// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/RenderingCommon.h"
#include "Layout/Clipping.h"

class FSlateBatchData;
class FSlateDrawElement;
class FSlateDrawLayer;
class FSlateElementBatch;
class FSlateRenderingPolicy;
class FSlateShaderResource;
class FSlateWindowElementList;
struct FShaderParams;

class FSlateDrawBox;
class FSlateDrawText;
class FSlateDrawShapedText;
class FSlateDrawLines;
class FSlateDrawCachedBuffer;

/**
 * A class which batches Slate elements for rendering
 */
class SLATECORE_API FSlateElementBatcher
{

	friend struct FLineBuilder;
public:

	FSlateElementBatcher( TSharedRef<FSlateRenderingPolicy> InRenderingPolicy );
	~FSlateElementBatcher();

	/** 
	 * Batches elements to be rendered 
	 * 
	 * @param DrawElements	The elements to batch
	 */
	void AddElements( FSlateWindowElementList& ElementList );

	/**
	 * Returns true if the elements in this batcher require v-sync.
	 */
	bool RequiresVsync() const { return bRequiresVsync; }

	/** Whether or not any post process passes were batched */
	bool HasFXPassses() const { return NumPostProcessPasses > 0;}

	/** 
	 * Resets all stored data accumulated during the batching process
	 */
	void ResetBatches();

private:
	void AddElementsInternal(const TArray<FSlateDrawElement>& DrawElements, const FVector2D& ViewportSize);

	void BatchBoxElements();
	void BatchBorderElements();
	void BatchTextElements();
	void BatchShapedTextElements();
	void BatchLineElements();
	void BatchCachedBuffers();
	
	FORCEINLINE FColor PackVertexColor(const FLinearColor& InLinearColor) const
	{
		//NOTE: Using pow(x,2) instead of a full sRGB conversion has been tried, but it ended up
		// causing too much loss of data in the lower levels of black.
		return InLinearColor.ToFColor(bSRGBVertexColor);
	}

	/** 
	 * Creates vertices necessary to draw a Quad element 
	 */
	template<ESlateVertexRounding Rounding>
	void AddQuadElement( const FSlateDrawElement& DrawElement, FColor Color = FColor::White);

	/** 
	 * Creates vertices necessary to draw a 3x3 element
	 */
	template<ESlateVertexRounding Rounding>
	void AddBoxElement( const FSlateDrawBox& DrawElement );

	/** 
	 * Creates vertices necessary to draw a string (one quad per character)
	 */
	template<ESlateVertexRounding Rounding>
	void AddTextElement( const FSlateDrawText& DrawElement );

	/** 
	 * Creates vertices necessary to draw a shaped glyph sequence (one quad per glyph)
	 */
	template<ESlateVertexRounding Rounding>
	void AddShapedTextElement( const FSlateDrawShapedText& DrawElement );

	/** 
	 * Creates vertices necessary to draw a gradient box (horizontal or vertical)
	 */
	template<ESlateVertexRounding Rounding>
	void AddGradientElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a spline (Bezier curve)
	 */
	void AddSplineElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a series of attached line segments
	 */
	template<ESlateVertexRounding Rounding>
	void AddLineElement( const FSlateDrawLines& DrawElement );
	
	/** 
	 * Creates vertices necessary to draw a viewport (just a textured quad)
	 */
	template<ESlateVertexRounding Rounding>
	void AddViewportElement( const FSlateDrawElement& DrawElement );

	/** 
	 * Creates vertices necessary to draw a border element
	 */
	template<ESlateVertexRounding Rounding>
	void AddBorderElement( const FSlateDrawBox& DrawElement );

	/**
	 * Batches a custom slate drawing element
	 *
	 * @param Position		The top left screen space position of the element
	 * @param Size			The size of the element
	 * @param Scale			The amount to scale the element by
	 * @param InPayload		The data payload for this element
	 * @param DrawEffects	DrawEffects to apply
	 * @param Layer			The layer to draw this element in
	 */
	void AddCustomElement( const FSlateDrawElement& DrawElement );

	void AddCustomVerts( const FSlateDrawElement& DrawElement );

	void AddCachedBuffer( const FSlateDrawCachedBuffer& DrawElement );

	void AddLayer(const FSlateDrawElement& DrawElement);

	void AddPostProcessPass(const FSlateDrawElement& DrawElement, const FVector2D& WindowSize);

	/** 
	 * Finds an batch for an element based on the passed in parameters
	 * Elements with common parameters and layers will batched together.
	 *
	 * @param Layer			The layer where this element should be drawn (signifies draw order)
	 * @param ShaderParams	The shader params for this element
	 * @param InTexture		The texture to use in the batch
	 * @param PrimitiveType	The primitive type( triangles, lines ) to use when drawing the batch
	 * @param ShaderType	The shader to use when rendering this batch
	 * @param DrawFlags		Any optional draw flags for this batch
	 * @param ScissorRect   Optional scissor rectangle for this batch
	 * @param SceneIndex    Index in the slate renderer's scenes array associated with this element.
	 */
	FSlateElementBatch& FindBatchForElement( uint32 Layer, 
											 const FShaderParams& ShaderParams, 
											 const FSlateShaderResource* InTexture, 
											 ESlateDrawPrimitive::Type PrimitiveType, 
											 ESlateShader::Type ShaderType, 
											 ESlateDrawEffect DrawEffects, 
											 ESlateBatchDrawFlag DrawFlags,
											 int32 ClippingIndex,
											 int32 SceneIndex = -1);
private:
	/** Batch data currently being filled in */
	FSlateBatchData* BatchData;

	/** The draw layer currently being accumulated */
	FSlateDrawLayer* DrawLayer;

	/** The draw layer currently being accumulated */
	const TArray<FSlateClippingState>* ClippingStates;

	/** Rendering policy we were created from */
	FSlateRenderingPolicy* RenderingPolicy;

	/** Track the number of drawn boxes from the previous frame to report to stats. */
	int32 ElmementStat_Boxes;

	/** Track the number of drawn borders from the previous frame to report to stats. */
	int32 ElmementStat_Borders;

	/** Track the number of drawn text from the previous frame to report to stats. */
	int32 ElmementStat_Text;

	/** Track the number of drawn shaped text from the previous frame to report to stats. */
	int32 ElmementStat_ShapedText;

	/** Track the number of drawn lines from the previous frame to report to stats. */
	int32 ElmementStat_Line;

	/** Track the number of drawn cached buffers from the previous frame to report to stats. */
	int32 ElmementStat_CachedBuffer;

	/** Track the number of drawn batches from the previous frame to report to stats. */
	int32 ElmementStat_Other;

	/** How many post process passes are needed */
	int32 NumPostProcessPasses;

	/** Offset to use when supporting 1:1 texture to pixel snapping */
	const float PixelCenterOffset;

	/** Are the vertex colors expected to be in sRGB space? */
	const bool bSRGBVertexColor;

	// true if any element in the batch requires vsync.
	bool bRequiresVsync;
};
