// Copyright 2026 TOXIC STOCK All rights reserved.

#include "FilmEmulatorLUTDetails.h"

#include "FilmEmulatorLUT.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/Texture2D.h"
#include "Math/UnrealMathUtility.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FilmEmulatorLUTDetails"

TSharedRef<IDetailCustomization> FFilmEmulatorLUTDetails::MakeInstance()
{
    return MakeShared<FFilmEmulatorLUTDetails>();
}

void FFilmEmulatorLUTDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);
    if (Objects.Num() > 0)
    {
        LutAsset = Cast<UFilmEmulatorLUT>(Objects[0].Get());
    }

    DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UFilmEmulatorLUT, Samples));

    BuildPreview(DetailBuilder);
}

void FFilmEmulatorLUTDetails::BuildPreview(IDetailLayoutBuilder& DetailBuilder)
{
    if (!LutAsset.IsValid())
    {
        return;
    }

    PreviewTexture.Reset(CreatePreviewTexture(LutAsset.Get()));

    IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Preview");

    if (!PreviewTexture.IsValid())
    {
        Category.AddCustomRow(LOCTEXT("LUTPreviewUnavailable", "Preview"))
            .WholeRowContent()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("LUTPreviewUnavailableText", "Preview unavailable"))
            ];
        return;
    }

    PreviewBrush = MakeShared<FSlateBrush>();
    PreviewBrush->SetResourceObject(PreviewTexture.Get());
    const FVector2D BrushSize(
        static_cast<float>(PreviewTexture->GetSizeX()),
        static_cast<float>(PreviewTexture->GetSizeY()));
    PreviewBrush->ImageSize = BrushSize;

    const float MaxDisplaySize = 256.0f;
    const float DisplaySize = FMath::Min(MaxDisplaySize, FMath::Max(BrushSize.X, BrushSize.Y));

    Category.AddCustomRow(LOCTEXT("LUTPreview", "Preview"))
        .WholeRowContent()
        [
            SNew(SBox)
            .WidthOverride(DisplaySize)
            .HeightOverride(DisplaySize)
            [
                SNew(SImage)
                .Image(PreviewBrush.Get())
            ]
        ];
}

UTexture2D* FFilmEmulatorLUTDetails::CreatePreviewTexture(UFilmEmulatorLUT* Lut) const
{
    if (!Lut)
    {
        return nullptr;
    }

    const int32 Size = Lut->Size;
    const int32 Total = Size * Size * Size;
    if (Size <= 0 || Lut->Samples.Num() != Total)
    {
        return nullptr;
    }

    const int32 Grid = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Size))));
    const int32 FullWidth = Grid * Size;
    const int32 FullHeight = Grid * Size;
    const int32 MaxPreviewSize = 256;
    const int32 MaxDim = FMath::Max(FullWidth, FullHeight);
    const float Scale = (MaxDim > MaxPreviewSize) ? (static_cast<float>(MaxPreviewSize) / static_cast<float>(MaxDim)) : 1.0f;
    const int32 Width = FMath::Max(1, FMath::RoundToInt(FullWidth * Scale));
    const int32 Height = FMath::Max(1, FMath::RoundToInt(FullHeight * Scale));

    UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
    if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
    {
        return nullptr;
    }

    Texture->SRGB = true;
    Texture->CompressionSettings = TC_Default;
    Texture->MipGenSettings = TMGS_NoMipmaps;
    Texture->Filter = TF_Bilinear;
    Texture->NeverStream = true;

    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
    if (!Data)
    {
        Mip.BulkData.Unlock();
        return nullptr;
    }
    FColor* Dest = static_cast<FColor*>(Data);

    const int32 PixelCount = Width * Height;
    for (int32 i = 0; i < PixelCount; ++i)
    {
        Dest[i] = FColor::Black;
    }

    const float MaxX = static_cast<float>(FullWidth - 1);
    const float MaxY = static_cast<float>(FullHeight - 1);

    for (int32 Y = 0; Y < Height; ++Y)
    {
        const float SrcYf = (Height > 1) ? (static_cast<float>(Y) / static_cast<float>(Height - 1)) * MaxY : 0.0f;
        const int32 SrcY = FMath::Clamp(FMath::RoundToInt(SrcYf), 0, FullHeight - 1);
        const int32 GridY = SrcY / Size;
        const int32 G = SrcY - GridY * Size;

        for (int32 X = 0; X < Width; ++X)
        {
            const float SrcXf = (Width > 1) ? (static_cast<float>(X) / static_cast<float>(Width - 1)) * MaxX : 0.0f;
            const int32 SrcX = FMath::Clamp(FMath::RoundToInt(SrcXf), 0, FullWidth - 1);
            const int32 GridX = SrcX / Size;
            const int32 R = SrcX - GridX * Size;
            const int32 B = GridY * Grid + GridX;
            const int32 DstIndex = Y * Width + X;

            if (B >= Size)
            {
                Dest[DstIndex] = FColor::Black;
                continue;
            }

            const int32 SrcIndex = R + G * Size + B * Size * Size;
            const FVector3f Sample = Lut->Samples[SrcIndex];
            FLinearColor Linear(Sample.X, Sample.Y, Sample.Z, 1.0f);
            Linear = Linear.GetClamped(0.0f, 1.0f);
            Dest[DstIndex] = Linear.ToFColor(true);
        }
    }

    Mip.BulkData.Unlock();
    Texture->UpdateResource();
    return Texture;
}

#undef LOCTEXT_NAMESPACE





