/*
* Inventory System X
*
* Copyright (C) 2023-2024 Mykhailo Oliynik <m19tes@gmail.com> All Rights Reserved.
*/


#include "UItemThumbnailRenderer.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Objects/Item.h"
#include "Runtime/Launch/Resources/Version.h"
#include "ThumbnailRendering/ThumbnailManager.h"

void UUItemThumbnailRenderer::GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	const UBlueprint* Blueprint = Cast<UBlueprint>(Object);

	if (Blueprint)
	{
		if (const UTexture2D* Texture = GetTextureFromGeneratedClass(Blueprint->GeneratedClass))
		{
			OutWidth = FMath::TruncToInt(Zoom * (float)Texture->GetSurfaceWidth());
			OutHeight = FMath::TruncToInt(Zoom * (float)Texture->GetSurfaceHeight());
		}
	}
	Super::GetThumbnailSize(Object, Zoom, OutWidth, OutHeight);
}

void UUItemThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height,
                                   FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	const UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	if (Blueprint)
	{
		if (const UTexture2D* Texture2D = GetTextureFromGeneratedClass(Blueprint->GeneratedClass))
		{
			const bool bUseTranslucentBlend = Texture2D && Texture2D->HasAlphaChannel() && ((Texture2D->LODGroup ==
				TEXTUREGROUP_UI) || (Texture2D->LODGroup == TEXTUREGROUP_Pixels2D));
			const TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;
			if (bUseTranslucentBlend)
			{
				// If using alpha, draw a checkerboard underneath first.
				const int32 CheckerDensity = 8;
				
#if ENGINE_MAJOR_VERSION >=5
				const	TObjectPtr<UTexture2D> Checker = UThumbnailManager::Get().CheckerboardTexture;
#else
				const UTexture2D* Checker = UThumbnailManager::Get().CheckerboardTexture;
#endif

				Canvas->DrawTile(
					0.0f, 0.0f, Width, Height, // Dimensions
					0.0f, 0.0f, CheckerDensity, CheckerDensity, // UVs
					FLinearColor::White, Checker->GetResource()); // Tint & Texture
			}
			// Use A canvas tile item to draw
			FCanvasTileItem CanvasTile(FVector2D(X, Y), Texture2D->GetResource(), FVector2D(Width, Height),
			                           FLinearColor::White);
			CanvasTile.BlendMode = bUseTranslucentBlend ? SE_BLEND_Translucent : SE_BLEND_Opaque;
			CanvasTile.BatchedElementParameters = BatchedElementParameters;
			CanvasTile.Draw(Canvas);
			if (Texture2D && Texture2D->IsCurrentlyVirtualTextured())
			{
				const auto VTChars = TEXT("VT");
				int32 VTWidth = 0;
				int32 VTHeight = 0;
				StringSize(GEngine->GetLargeFont(), VTWidth, VTHeight, VTChars);
				const float PaddingX = Width / 128.0f;
				const float PaddingY = Height / 128.0f;
				const float ScaleX = Width / 64.0f; //Text is 1/64'th of the size of the thumbnails
				const float ScaleY = Height / 64.0f;
				// VT overlay
				FCanvasTextItem TextItem(
					FVector2D(Width - PaddingX - VTWidth * ScaleX, Height - PaddingY - VTHeight * ScaleY),
					FText::FromString(VTChars), GEngine->GetLargeFont(), FLinearColor::White);
				TextItem.EnableShadow(FLinearColor::Black);
				TextItem.Scale = FVector2D(ScaleX, ScaleY);
				TextItem.Draw(Canvas);
			}
			return;
		}
	}
	Super::Draw(Object, X, Y, Width, Height, RenderTarget, Canvas, bAdditionalViewFamily);
}

bool UUItemThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	const UBlueprint* Blueprint = Cast<UBlueprint>(Object);

	if (Blueprint && GetTextureFromGeneratedClass(Blueprint->GeneratedClass) != nullptr)
	{
		return true;
	}
	return Super::CanVisualizeAsset(Object);
}

UTexture2D* UUItemThumbnailRenderer::GetTextureFromGeneratedClass(UClass* Class) const
{
	if (Class)
	{
		if (Class->IsChildOf(UItem::StaticClass()))
		{
			if (const UItem* CDO = Class->GetDefaultObject<UItem>())
			{
				return CDO->Icon.LoadSynchronous();
			}
		}
	}
	return nullptr;
}
