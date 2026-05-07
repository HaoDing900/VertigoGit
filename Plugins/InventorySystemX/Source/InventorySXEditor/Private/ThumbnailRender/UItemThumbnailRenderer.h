/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/

#pragma once

#include "CoreMinimal.h"
#include "ThumbnailRendering/BlueprintThumbnailRenderer.h"
#include "UItemThumbnailRenderer.generated.h"

/**
 * 
 */
UCLASS()
class INVENTORYSXEDITOR_API UUItemThumbnailRenderer : public UBlueprintThumbnailRenderer
{
	GENERATED_BODY()
protected:
	UUItemThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
	{}

	// UThumbnailRenderer implementation
	virtual void GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	virtual bool CanVisualizeAsset(UObject* Object) override;
protected:

	UTexture2D* GetTextureFromGeneratedClass(UClass* Class) const;
};




