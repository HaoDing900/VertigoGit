#include "Loading/VTGLevelLoadingInfo.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

int32 VTGSimplifyLoadingScreen()
{
    auto *BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Widget/Loading/WBP_VTGLoadingScreen"));
    auto *Info = LoadObject<UVTGLevelLoadingInfo>(nullptr, TEXT("/Game/Widget/Loading/DA_LevelLoadingInfo"));
    if (!BP || !Info || !BP->WidgetTree) return 50;
    auto AddBody = [Info](const FText &Body)
    {
        if (!Body.IsEmpty() && !Info->NewsBodies.ContainsByPredicate([&](const FText &Other) { return Other.EqualTo(Body); }))
            Info->NewsBodies.Add(Body); // Keep existing FText identities for localization.
    };
    auto AddImages = [Info](const TArray<TObjectPtr<UTexture2D>> &Images)
    {
        for (auto Image : Images) if (Image) Info->SharedBackgroundImages.AddUnique(Image);
    };
    for (const auto &News : Info->SharedNewsItems) AddBody(News.Body);
    auto Migrate = [&](const FVTGLoadingContent &Content)
    {
        AddBody(Content.NewsBody);
        for (const auto &News : Content.NewsItems) AddBody(News.Body);
        AddImages(Content.BackgroundImages);
    };
    Migrate(Info->DefaultContent);
    for (const auto &Level : Info->Levels) Migrate(Level.Content);
    if (auto *Body = Cast<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("TipText")))) AddBody(Body->GetText());
    Info->Levels.Reset(); Info->DefaultContent = FVTGLoadingContent(); Info->SharedNewsItems.Reset();
    for (const TCHAR *Name : {TEXT("Title"), TEXT("Subtitle")})
        if (auto *Widget = BP->WidgetTree->FindWidget(Name)) BP->WidgetTree->RemoveWidget(Widget);
    auto Set = [BP](const TCHAR *Name, FText Text)
    {
        auto *Widget = Cast<UTextBlock>(BP->WidgetTree->FindWidget(Name));
        if (!Widget) return false;
        Widget->SetText(Text); return true;
    };
    if (!Set(TEXT("ChapterLabel"), NSLOCTEXT("VTGLoading", "City", "THE GREAT ABYSS CITY")) ||
        !Set(TEXT("TipLabel"), NSLOCTEXT("VTGLoading", "NewsLabel", "大渊城新闻")) ||
        !Set(TEXT("LoadingLabel"), NSLOCTEXT("VTGLoading", "Loading", "正在载入"))) return 51;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Log);
    if (Log.NumErrors) return 52;
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
    for (UObject *Asset : {static_cast<UObject *>(BP), static_cast<UObject *>(Info)})
    {
        const auto Filename = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args)) return 53;
    }
    UE_LOG(LogTemp, Display, TEXT("SIMPLE LOADING PASS: removed location/introduction, migrated %d news bodies and %d images, compiled and saved"), Info->NewsBodies.Num(), Info->SharedBackgroundImages.Num());
    return 0;
}
