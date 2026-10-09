#include "Loading/VTGLevelLoadingInfo.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Misc/PackageName.h"

FString UVTGLevelLoadingInfo::NormalizeLevelName(const FString &LevelName)
{
    FString Result = FPackageName::ExportTextPathToObjectPath(LevelName.TrimStartAndEnd());
    int32 Pos;
    if (Result.FindChar(TEXT('?'), Pos))
        Result.LeftInline(Pos);
    if (Result.FindChar(TEXT('.'), Pos))
        Result.LeftInline(Pos);
    int32 Slash = INDEX_NONE;
    Result.FindLastChar(TEXT('/'), Slash);
    FString Leaf = Result.Mid(Slash + 1);
    if (Leaf.StartsWith(TEXT("UEDPIE_")))
    {
        const int32 PrefixEnd = Leaf.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 7);
        if (PrefixEnd != INDEX_NONE)
            Leaf.RightChopInline(PrefixEnd + 1);
    }
    return Result.Left(Slash + 1) + Leaf;
}

FVTGLoadingContent UVTGLevelLoadingInfo::GetContentForLevel(const FString &LevelName, bool &Found) const
{
    Found = false;
    const FString Target = NormalizeLevelName(LevelName);
    if (Target.IsEmpty())
        return DefaultContent;
    const FVTGLevelLoadingEntry *Match = nullptr;
    for (const auto &Entry : Levels)
    {
        if (Entry.Level.IsNull())
            continue;
        const FString Path = Entry.Level.ToSoftObjectPath().GetLongPackageName();
        const bool Matches = Target.Contains(TEXT("/"))
                                 ? Path.Equals(Target, ESearchCase::IgnoreCase)
                                 : FPackageName::GetShortName(Path).Equals(Target, ESearchCase::IgnoreCase);
        if (!Matches)
            continue;
        // Ambiguous short names / duplicate entries must not silently choose the wrong location.
        if (Match)
        {
            UE_LOG(LogTemp, Warning, TEXT("Ambiguous loading info for %s; using default content."), *LevelName);
            return DefaultContent;
        }
        Match = &Entry;
    }
    if (!Match)
        return DefaultContent;
    Found = true;
    return Match->Content;
}

void UVTGLevelLoadingInfo::ApplyToWidget(UUserWidget *Widget, const FString &LevelName) const
{
    if (!Widget || !Widget->WidgetTree)
        return;
    bool Found;
    const auto Content = GetContentForLevel(LevelName, Found);
    auto SetText = [Widget](FName Name, const FText &Value)
    {
        if (Name.IsNone())
            return;
        UWidget *Target = Widget->WidgetTree->FindWidget(Name);
        if (auto *Text = Cast<UTextBlock>(Target))
            Text->SetText(Value);
        else if (auto *Rich = Cast<URichTextBlock>(Target))
            Rich->SetText(Value);
        else
            UE_LOG(LogTemp, Warning, TEXT("Loading info text widget missing: %s in %s"), *Name.ToString(),
                   *Widget->GetClass()->GetName());
    };
    SetText(WidgetNames.Location, Content.Location);
    SetText(WidgetNames.Region, Content.Region);
    SetText(WidgetNames.Introduction, Content.Introduction);
    SetText(WidgetNames.NewsHeading, Content.NewsHeading);
    SetText(WidgetNames.NewsBody, Content.NewsBody);
    for (const auto &Pair : Content.ExtraTexts)
        SetText(Pair.Key, Pair.Value);
    UE_LOG(LogTemp, Display, TEXT("Loading info applied: destination=%s location=%s matched=%d"), *LevelName,
           *Content.Location.ToString(), Found);
}
