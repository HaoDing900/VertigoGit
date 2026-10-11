#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VTGLevelLoadingInfo.generated.h"
class UUserWidget;
class UWorld;
class UTexture2D;

// Legacy fields remain serializable only so existing authored content can be migrated.
USTRUCT(BlueprintType)
struct VERTIGO_API FVTGLoadingNews
{
    GENERATED_BODY()
    UPROPERTY() FText Heading;
    UPROPERTY() FText Body;
};
USTRUCT(BlueprintType)
struct VERTIGO_API FVTGLoadingContent
{
    GENERATED_BODY()
    UPROPERTY() FText Location;
    UPROPERTY() FText Region;
    UPROPERTY() FText Introduction;
    UPROPERTY() FText NewsHeading;
    UPROPERTY() FText NewsBody;
    UPROPERTY() TArray<TObjectPtr<UTexture2D>> BackgroundImages;
    UPROPERTY() TArray<FVTGLoadingNews> NewsItems;
    UPROPERTY() TMap<FName, FText> ExtraTexts;
};
USTRUCT(BlueprintType)
struct VERTIGO_API FVTGLevelLoadingEntry
{
    GENERATED_BODY()
    UPROPERTY() TSoftObjectPtr<UWorld> Level;
    UPROPERTY() FVTGLoadingContent Content;
};
USTRUCT(BlueprintType)
struct VERTIGO_API FVTGLoadingTextBindings
{
    GENERATED_BODY()
    UPROPERTY() FName Location = TEXT("Title");
    UPROPERTY() FName Region = TEXT("ChapterLabel");
    UPROPERTY() FName Introduction = TEXT("Subtitle");
    UPROPERTY() FName NewsHeading = TEXT("TipLabel");
    UPROPERTY() FName NewsBody = TEXT("TipText");
};

/** One shared image pool and body-only news pool for every loading screen. */
UCLASS(BlueprintType)
class VERTIGO_API UVTGLevelLoadingInfo : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (DisplayName = "随机图片 / Background Images"))
    TArray<TObjectPtr<UTexture2D>> SharedBackgroundImages;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (MultiLine = true, DisplayName = "新闻正文列表 / News Bodies"))
    TArray<FText> NewsBodies;
    /** Zero shows one randomly selected news body per loading screen. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (ClampMin = "0.0", DisplayName = "新闻轮换秒数（0 不轮换） / News Rotation Seconds"))
    float NewsRotationSeconds = 5.f;

    UPROPERTY() TArray<FVTGLevelLoadingEntry> Levels;
    UPROPERTY() FVTGLoadingContent DefaultContent;
    UPROPERTY() TArray<FVTGLoadingNews> SharedNewsItems;
    UPROPERTY() FName BackgroundImageWidget = TEXT("BackgroundArt");
    UPROPERTY() FLinearColor BackgroundImageTint = FLinearColor::White;
    UPROPERTY() FVTGLoadingTextBindings WidgetNames;

    // Kept for compatibility with existing Blueprint references and migration tools.
    UFUNCTION(BlueprintPure, Category = "Loading")
    FVTGLoadingContent GetContentForLevel(const FString &LevelName, bool &Found) const;
    UFUNCTION(BlueprintCallable, Category = "Loading")
    void ApplyToWidget(UUserWidget *Widget, const FString &LevelName) const;
    TArray<FVTGLoadingNews> GetNewsForLevel(const FString &LevelName) const;
    static FString NormalizeLevelName(const FString &LevelName);
};
