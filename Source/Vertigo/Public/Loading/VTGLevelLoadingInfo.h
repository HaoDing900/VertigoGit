#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VTGLevelLoadingInfo.generated.h"

class UUserWidget;
class UWorld;

USTRUCT(BlueprintType)
struct VERTIGO_API FVTGLoadingContent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading", meta = (DisplayName = "地点名称 / Location"))
    FText Location;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading", meta = (DisplayName = "地区或章节 / Region"))
    FText Region;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading",
              meta = (MultiLine = true, DisplayName = "地点介绍 / Introduction"))
    FText Introduction;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading", meta = (DisplayName = "新闻或提示标题 / Heading"))
    FText NewsHeading;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading",
              meta = (MultiLine = true, DisplayName = "新闻或提示正文 / Body"))
    FText NewsBody;
    /** Optional additional text: key = exact UMG widget name. Only text is changed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading",
              meta = (MultiLine = true, DisplayName = "额外文字（控件名 → 文案）"))
    TMap<FName, FText> ExtraTexts;
};

USTRUCT(BlueprintType)
struct VERTIGO_API FVTGLevelLoadingEntry
{
    GENERATED_BODY()
    /** Select the destination map in the asset picker. No map is loaded to read this entry. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading", meta = (DisplayName = "目标关卡 / Level"))
    TSoftObjectPtr<UWorld> Level;
    /** Blank fields intentionally clear the corresponding text, without changing layout. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading", meta = (DisplayName = "该关卡文案 / Content"))
    FVTGLoadingContent Content;
};

USTRUCT(BlueprintType)
struct VERTIGO_API FVTGLoadingTextBindings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings") FName Location = TEXT("Title");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings") FName Region = TEXT("ChapterLabel");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings") FName Introduction = TEXT("Subtitle");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings") FName NewsHeading = TEXT("TipLabel");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings") FName NewsBody = TEXT("TipText");
};

/** Editable per-map loading copy, independent of the artist-owned Widget Blueprint. */
UCLASS(BlueprintType)
class VERTIGO_API UVTGLevelLoadingInfo : public UDataAsset
{
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "关卡文案",
              meta = (TitleProperty = "Level", DisplayName = "关卡列表 / Levels"))
    TArray<FVTGLevelLoadingEntry> Levels;
    /** Used for any destination not listed above; prevents stale text from the previous map. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "关卡文案",
              meta = (DisplayName = "未配置关卡的默认文案 / Default Content"))
    FVTGLoadingContent DefaultContent;
    /** Change these only if the corresponding Designer widget has been renamed. None disables a binding. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "文字控件绑定",
              meta = (DisplayName = "文字控件名称 / Widget Names"))
    FVTGLoadingTextBindings WidgetNames;

    UFUNCTION(BlueprintPure, Category = "Loading")
    FVTGLoadingContent GetContentForLevel(const FString &LevelName, bool &Found) const;
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void ApplyToWidget(UUserWidget *Widget, const FString &LevelName) const;

    static FString NormalizeLevelName(const FString &LevelName);
};
