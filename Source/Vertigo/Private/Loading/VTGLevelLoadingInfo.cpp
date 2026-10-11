#include "Loading/VTGLevelLoadingInfo.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"

FString UVTGLevelLoadingInfo::NormalizeLevelName(const FString &LevelName)
{
    return FPackageName::ExportTextPathToObjectPath(LevelName);
}
FVTGLoadingContent UVTGLevelLoadingInfo::GetContentForLevel(const FString &LevelName, bool &Found) const
{
    Found = false;
    return DefaultContent;
}
void UVTGLevelLoadingInfo::ApplyToWidget(UUserWidget *Widget, const FString &LevelName) const
{
    if (!Widget || !Widget->WidgetTree) return;
    auto Images = SharedBackgroundImages;
    Images.RemoveAll([](const TObjectPtr<UTexture2D> &Image) { return !IsValid(Image); });
    if (!Images.IsEmpty())
        if (auto *Image = Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("BackgroundArt"))))
        {
            Image->SetBrushFromTexture(Images[FMath::RandHelper(Images.Num())], false);
            Image->SetColorAndOpacity(FLinearColor::White);
        }
    const auto News = GetNewsForLevel(LevelName);
    if (!News.IsEmpty())
        if (auto *Text = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("TipText"))))
            Text->SetText(News[FMath::RandHelper(News.Num())].Body);
    // Branding, the fixed news label, and LoadingLabel belong to the Widget Blueprint.
}
TArray<FVTGLoadingNews> UVTGLevelLoadingInfo::GetNewsForLevel(const FString &LevelName) const
{
    TArray<FVTGLoadingNews> Items;
    for (const FText &Body : NewsBodies)
        if (!Body.IsEmpty())
        {
            auto &Item = Items.AddDefaulted_GetRef();
            Item.Body = Body;
        }
    return Items;
}
