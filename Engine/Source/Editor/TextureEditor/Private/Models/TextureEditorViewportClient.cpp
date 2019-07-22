// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Models/TextureEditorViewportClient.h"
#include "Widgets/Layout/SScrollBar.h"
#include "CanvasItem.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Texture2D.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Engine/TextureCube.h"
#include "Engine/VolumeTexture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "UnrealEdGlobals.h"
#include "CubemapUnwrapUtils.h"
#include "Slate/SceneViewport.h"
#include "Texture2DPreview.h"
#include "VolumeTexturePreview.h"
#include "TextureEditorSettings.h"
#include "Widgets/STextureEditorViewport.h"
#include "CanvasTypes.h"
#include "ImageUtils.h"


/* FTextureEditorViewportClient structors
 *****************************************************************************/

FTextureEditorViewportClient::FTextureEditorViewportClient( TWeakPtr<ITextureEditorToolkit> InTextureEditor, TWeakPtr<STextureEditorViewport> InTextureEditorViewport )
	: TextureEditorPtr(InTextureEditor)
	, TextureEditorViewportPtr(InTextureEditorViewport)
	, CheckerboardTexture(NULL)
{
	check(TextureEditorPtr.IsValid() && TextureEditorViewportPtr.IsValid());

	ModifyCheckerboardTextureColors();
}


FTextureEditorViewportClient::~FTextureEditorViewportClient( )
{
	DestroyCheckerboardTexture();
}


/* FViewportClient interface
 *****************************************************************************/

void FTextureEditorViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	if (!TextureEditorPtr.IsValid())
	{
		return;
	}
	
	UTexture* Texture = TextureEditorPtr.Pin()->GetTexture();
	FVector2D Ratio = FVector2D(GetViewportHorizontalScrollBarRatio(), GetViewportVerticalScrollBarRatio());
	FVector2D ViewportSize = FVector2D(TextureEditorViewportPtr.Pin()->GetViewport()->GetSizeXY().X, TextureEditorViewportPtr.Pin()->GetViewport()->GetSizeXY().Y);
	FVector2D ScrollBarPos = GetViewportScrollBarPositions();
	int32 YOffset = (Ratio.Y > 1.0f)? ((ViewportSize.Y - (ViewportSize.Y / Ratio.Y)) * 0.5f): 0;
	int32 YPos = YOffset - ScrollBarPos.Y;
	int32 XOffset = (Ratio.X > 1.0f)? ((ViewportSize.X - (ViewportSize.X / Ratio.X)) * 0.5f): 0;
	int32 XPos = XOffset - ScrollBarPos.X;
	
	UpdateScrollBars();

	const UTextureEditorSettings& Settings = *GetDefault<UTextureEditorSettings>();

	Canvas->Clear( Settings.BackgroundColor );

	UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
	UTextureCube* TextureCube = Cast<UTextureCube>(Texture);
	UVolumeTexture* VolumeTexture = Cast<UVolumeTexture>(Texture);
	UTextureRenderTarget2D* TextureRT2D = Cast<UTextureRenderTarget2D>(Texture);
	UTextureRenderTargetCube* RTTextureCube = Cast<UTextureRenderTargetCube>(Texture);

	// Fully stream in the texture before drawing it.
	if (Texture2D)
	{
		Texture2D->SetForceMipLevelsToBeResident(30.0f);
		Texture2D->WaitForStreaming();
	}

	TextureEditorPtr.Pin()->PopulateQuickInfo();

	// Figure out the size we need
	uint32 Width, Height;
	TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height);
	const float MipLevel = (float)TextureEditorPtr.Pin()->GetMipLevel();
	const float LayerIndex = (float)TextureEditorPtr.Pin()->GetLayer();

	bool bIsVirtualTexture = false;

	TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;

	if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4)
	{
		if (TextureCube || RTTextureCube)
		{
			BatchedElementParameters = new FMipLevelBatchedElementParameters(MipLevel, false);
		}
		else if (VolumeTexture)
		{
			BatchedElementParameters = new FBatchedElementVolumeTexturePreviewParameters(
				Settings.VolumeViewMode == TextureEditorVolumeViewMode_DepthSlices, 
				FMath::Max<int32>(VolumeTexture->GetSizeZ() >> VolumeTexture->GetCachedLODBias(), 1), 
				MipLevel, 
				(float)TextureEditorPtr.Pin()->GetVolumeOpacity(),
				true, 
				TextureEditorPtr.Pin()->GetVolumeOrientation());
		}
		else if (Texture2D)
		{
			bool bIsNormalMap = Texture2D->IsNormalMap();
			bool bIsSingleChannel = Texture2D->CompressionSettings == TC_Grayscale || Texture2D->CompressionSettings == TC_Alpha;
			bIsVirtualTexture = Texture2D->IsCurrentlyVirtualTextured();
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, bIsNormalMap, bIsSingleChannel, bIsVirtualTexture);
		}
		else if (TextureRT2D)
		{
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, false, false, false);
		}
		else
		{
			// Default to treating any UTexture derivative as a 2D texture resource
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, false, false, false);
		}
	}

	// Draw the background checkerboard pattern in the same size/position as the render texture so it will show up anywhere
	// the texture has transparency
	if (Settings.Background == TextureEditorBackground_CheckeredFill)
	{
		Canvas->DrawTile( 0.0f, 0.0f, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, 0.0f, 0.0f, (Viewport->GetSizeXY().X / CheckerboardTexture->GetSizeX()), (Viewport->GetSizeXY().Y / CheckerboardTexture->GetSizeY()), FLinearColor::White, CheckerboardTexture->Resource);
	}
	else if (Settings.Background == TextureEditorBackground_Checkered)
	{
		Canvas->DrawTile( XPos, YPos, Width, Height, 0.0f, 0.0f, (Width / CheckerboardTexture->GetSizeX()), (Height / CheckerboardTexture->GetSizeY()), FLinearColor::White, CheckerboardTexture->Resource);
	}

	float Exposure = FMath::Pow(2.0f, (float)TextureEditorViewportPtr.Pin()->GetExposureBias());

	if ( Texture->Resource != nullptr )
	{
		FCanvasTileItem TileItem( FVector2D( XPos, YPos ), Texture->Resource, FVector2D( Width, Height ), FLinearColor(Exposure, Exposure, Exposure) );
		TileItem.BlendMode = TextureEditorPtr.Pin()->GetColourChannelBlendMode();
		TileItem.BatchedElementParameters = BatchedElementParameters;
		Canvas->DrawItem( TileItem );

		// Draw a white border around the texture to show its extents
		if (Settings.TextureBorderEnabled)
		{
			FCanvasBoxItem BoxItem( FVector2D(XPos, YPos), FVector2D(Width , Height ) );
			BoxItem.SetColor( Settings.TextureBorderColor );
			Canvas->DrawItem( BoxItem );
		}

		// if we are presenting a virtual texture, make the appropriate tiles resident
		if (bIsVirtualTexture && CVarEnableVTFeedback.GetValueOnAnyThread() != 0)
		{
			FVirtualTexture2DResource* VTResource = static_cast<FVirtualTexture2DResource*>(Texture->Resource);
			const FVector2D ScreenSpaceSize = { (float)Width, (float)Height };

			// Calculate the rect of the texture that is visible on screen
			// When the entire texture is visible, we pass -1 as dimensions (VirtualTextureUtils::CalculateVisibleTiles needs to calculate the effective texture size anyway so don't do it twice)
			auto ZeroOrAbsolute = [](int32 value) -> int32 { return value >= 0 ? 0 : FMath::Abs(value); };
			auto GetCorrectDimension = [](int value, int32 viewportSize, int32 TextureSize) -> int32 { return value <= viewportSize ? TextureSize : viewportSize; };

			const float Zoom = 1.0f / TextureEditorPtr.Pin()->GetZoom();
			const int32 VisibleXPos = FMath::FloorToInt(Zoom * -FMath::Min(0, XPos));
			const int32 VisibleYPos = FMath::FloorToInt(Zoom * -FMath::Min(0, YPos));

			const FIntRect VisibleTextureRect = FIntRect(VisibleXPos, VisibleYPos,
				VisibleXPos + GetCorrectDimension(Zoom * Width, Zoom * ViewportSize.X, VTResource->GetSizeX()),
				VisibleYPos + GetCorrectDimension(Zoom * Height, Zoom * ViewportSize.Y, VTResource->GetSizeY()));

			const ERHIFeatureLevel::Type InFeatureLevel = GMaxRHIFeatureLevel;
			ENQUEUE_RENDER_COMMAND(MakeTilesResident)(
				[InFeatureLevel, VTResource, ScreenSpaceSize, VisibleTextureRect, MipLevel](FRHICommandListImmediate& RHICmdList)
			{
				// AcquireAllocatedVT() must happen on render thread
				GetRendererModule().RequestVirtualTextureTilesForRegion(VTResource->AcquireAllocatedVT(), ScreenSpaceSize, VisibleTextureRect, MipLevel);
				GetRendererModule().LoadPendingVirtualTextureTiles(RHICmdList, InFeatureLevel);
			});
		}
	}

	// If we are requesting an explicit mip level of a VT asset, test to see if we can even display it properly and warn about it
	if (bIsVirtualTexture && MipLevel >= 0.f)
	{
		const uint32 Mip = (uint32)MipLevel;
		const FIntPoint SizeOnMip = { Texture2D->GetSizeX() >> Mip,Texture2D->GetSizeY() >> Mip };
		const uint64 NumPixels = SizeOnMip.X * SizeOnMip.Y;

		const FVirtualTexture2DResource* Resource = (FVirtualTexture2DResource*)Texture2D->Resource;
		const FIntPoint PhysicalTextureSize = Resource->GetPhysicalTextureSize(0u);
		const uint64 NumPhysicalPixels = PhysicalTextureSize.X * PhysicalTextureSize.Y;

		if (NumPixels >= NumPhysicalPixels)
		{
			UFont* ErrorFont = GEngine->GetLargeFont();
			const int32 LineHeight = FMath::TruncToInt(ErrorFont->GetMaxCharHeight());
			const FText Message = NSLOCTEXT("TextureEditor", "InvalidVirtualTextureMipDisplay", "Displaying a virtual texture on a mip level that is larger than the physical cache. Rendering will probably be invalid!");
			const uint32 MessageWidth = ErrorFont->GetStringSize(*Message.ToString());
			const uint32 Xpos = (ViewportSize.X - MessageWidth) / 2;
			Canvas->DrawShadowedText(Xpos, LineHeight*1.5,
				Message,
				ErrorFont, FLinearColor::Red);
		}

	}
}


bool FTextureEditorViewportClient::InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool Gamepad)
{
	if (Key == EKeys::MouseScrollUp)
	{
		TextureEditorPtr.Pin()->ZoomIn();

		return true;
	}
	else if (Key == EKeys::MouseScrollDown)
	{
		TextureEditorPtr.Pin()->ZoomOut();

		return true;
	}
	else if (Key == EKeys::RightMouseButton)
	{
		TextureEditorPtr.Pin()->SetVolumeOrientation(FRotator(90, 0, -90));
	}
	return false;
}

bool FTextureEditorViewportClient::InputAxis(FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	if (Key == EKeys::MouseX || Key == EKeys::MouseY)
	{
		FRotator DeltaRotator(ForceInitToZero);
		const float RotationSpeed = .2f;
		if (Key == EKeys::MouseY)
		{
			DeltaRotator.Pitch = Delta * RotationSpeed;
		}
		else
		{
			DeltaRotator.Yaw = Delta * RotationSpeed;
		}

		TextureEditorPtr.Pin()->SetVolumeOrientation((FRotationMatrix::Make(DeltaRotator) * FRotationMatrix::Make(TextureEditorPtr.Pin()->GetVolumeOrientation())).Rotator());
		return true;
	}

	return false;
}

bool FTextureEditorViewportClient::InputGesture(FViewport* Viewport, EGestureEvent GestureType, const FVector2D& GestureDelta, bool bIsDirectionInvertedFromDevice)
{
	const bool LeftMouseButtonDown = Viewport->KeyState(EKeys::LeftMouseButton);
	const bool RightMouseButtonDown = Viewport->KeyState(EKeys::RightMouseButton);

	if (GestureType == EGestureEvent::Scroll && !LeftMouseButtonDown && !RightMouseButtonDown)
	{
		double CurrentZoom = TextureEditorPtr.Pin()->GetZoom();
		TextureEditorPtr.Pin()->SetZoom(CurrentZoom + GestureDelta.Y * 0.01);
		return true;
	}

	return false;
}


void FTextureEditorViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(CheckerboardTexture);
}


void FTextureEditorViewportClient::ModifyCheckerboardTextureColors()
{
	DestroyCheckerboardTexture();

	const UTextureEditorSettings& Settings = *GetDefault<UTextureEditorSettings>();
	CheckerboardTexture = FImageUtils::CreateCheckerboardTexture(Settings.CheckerColorOne, Settings.CheckerColorTwo, Settings.CheckerSize);
}


FText FTextureEditorViewportClient::GetDisplayedResolution() const
{
	uint32 Height = 1;
	uint32 Width = 1;
	TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height);
	return FText::Format( NSLOCTEXT("TextureEditor", "DisplayedResolution", "Displayed: {0}x{1}"), FText::AsNumber( FMath::Max((uint32)1, Width) ), FText::AsNumber( FMath::Max((uint32)1, Height)) );
}


float FTextureEditorViewportClient::GetViewportVerticalScrollBarRatio() const
{
	uint32 Height = 1;
	uint32 Width = 1;
	float WidgetHeight = 1.0f;
	if (TextureEditorViewportPtr.Pin()->GetVerticalScrollBar().IsValid())
	{
		TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height);

		WidgetHeight = TextureEditorViewportPtr.Pin()->GetViewport()->GetSizeXY().Y;
	}

	return WidgetHeight / Height;
}


float FTextureEditorViewportClient::GetViewportHorizontalScrollBarRatio() const
{
	uint32 Width = 1;
	uint32 Height = 1;
	float WidgetWidth = 1.0f;
	if (TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar().IsValid())
	{
		TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height);

		WidgetWidth = TextureEditorViewportPtr.Pin()->GetViewport()->GetSizeXY().X;
	}

	return WidgetWidth / Width;
}


void FTextureEditorViewportClient::UpdateScrollBars()
{
	TSharedPtr<STextureEditorViewport> Viewport = TextureEditorViewportPtr.Pin();

	if (!Viewport.IsValid() || !Viewport->GetVerticalScrollBar().IsValid() || !Viewport->GetHorizontalScrollBar().IsValid())
	{
		return;
	}

	float VRatio = GetViewportVerticalScrollBarRatio();
	float HRatio = GetViewportHorizontalScrollBarRatio();
	float VDistFromBottom = Viewport->GetVerticalScrollBar()->DistanceFromBottom();
	float HDistFromBottom = Viewport->GetHorizontalScrollBar()->DistanceFromBottom();

	if (VRatio < 1.0f)
	{
		if (VDistFromBottom < 1.0f)
		{
			Viewport->GetVerticalScrollBar()->SetState(FMath::Clamp(1.0f - VRatio - VDistFromBottom, 0.0f, 1.0f), VRatio);
		}
		else
		{
			Viewport->GetVerticalScrollBar()->SetState(0.0f, VRatio);
		}
	}

	if (HRatio < 1.0f)
	{
		if (HDistFromBottom < 1.0f)
		{
			Viewport->GetHorizontalScrollBar()->SetState(FMath::Clamp(1.0f - HRatio - HDistFromBottom, 0.0f, 1.0f), HRatio);
		}
		else
		{
			Viewport->GetHorizontalScrollBar()->SetState(0.0f, HRatio);
		}
	}
}


FVector2D FTextureEditorViewportClient::GetViewportScrollBarPositions() const
{
	FVector2D Positions = FVector2D::ZeroVector;
	if (TextureEditorViewportPtr.Pin()->GetVerticalScrollBar().IsValid() && TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar().IsValid())
	{
		uint32 Width, Height;
		UTexture* Texture = TextureEditorPtr.Pin()->GetTexture();
		float VRatio = GetViewportVerticalScrollBarRatio();
		float HRatio = GetViewportHorizontalScrollBarRatio();
		float VDistFromBottom = TextureEditorViewportPtr.Pin()->GetVerticalScrollBar()->DistanceFromBottom();
		float HDistFromBottom = TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar()->DistanceFromBottom();
	
		TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height);

		if ((TextureEditorViewportPtr.Pin()->GetVerticalScrollBar()->GetVisibility() == EVisibility::Visible) && VDistFromBottom < 1.0f)
		{
			Positions.Y = FMath::Clamp(1.0f - VRatio - VDistFromBottom, 0.0f, 1.0f) * Height;
		}
		else
		{
			Positions.Y = 0.0f;
		}

		if ((TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar()->GetVisibility() == EVisibility::Visible) && HDistFromBottom < 1.0f)
		{
			Positions.X = FMath::Clamp(1.0f - HRatio - HDistFromBottom, 0.0f, 1.0f) * Width;
		}
		else
		{
			Positions.X = 0.0f;
		}
	}

	return Positions;
}

void FTextureEditorViewportClient::DestroyCheckerboardTexture()
{
	if (CheckerboardTexture)
	{
		if (CheckerboardTexture->Resource)
		{
			CheckerboardTexture->ReleaseResource();
		}
		CheckerboardTexture->MarkPendingKill();
		CheckerboardTexture = NULL;
	}
}