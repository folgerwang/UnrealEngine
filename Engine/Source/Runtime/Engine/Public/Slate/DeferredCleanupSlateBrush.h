// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderingThread.h"
#include "UObject/GCObject.h"
#include "Styling/SlateBrush.h"

class ENGINE_API FDeferredCleanupSlateBrush : public ISlateBrushSource, public FDeferredCleanupInterface, public FGCObject
{
public:
	static TSharedRef<FDeferredCleanupSlateBrush> CreateBrush(const FSlateBrush& Brush);

	static TSharedRef<FDeferredCleanupSlateBrush> CreateBrush(
		class UTexture* InTexture,
		const FLinearColor& InTint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),
		ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile,
		ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);

	static TSharedRef<FDeferredCleanupSlateBrush> CreateBrush(
		class UObject* InResource,
		const FVector2D& InImageSize,
		const FLinearColor& InTint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),
		ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile,
		ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);

	virtual const FSlateBrush* GetSlateBrush() const override { return &InternalBrush; }

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

	static const FSlateBrush* TrySlateBrush(const TSharedPtr<FDeferredCleanupSlateBrush>& DeferredSlateBrush)
	{
		return DeferredSlateBrush.IsValid() ? DeferredSlateBrush->GetSlateBrush() : nullptr;
	}

private:
	FDeferredCleanupSlateBrush();

	FDeferredCleanupSlateBrush(const FSlateBrush& Brush)
		: InternalBrush(Brush)
	{
	}

	FSlateBrush InternalBrush;
};
