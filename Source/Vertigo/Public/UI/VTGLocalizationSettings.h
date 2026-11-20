#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VTGLocalizationSettings.generated.h"

UCLASS(Config=Game,defaultconfig,meta=(DisplayName="Manual Localization / 手工本地化"))
class VERTIGO_API UVTGLocalizationSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config,EditAnywhere,Category="Dialogue",meta=(DisplayName="英文长台词自动分页 / English Dialogue Pagination"))
    bool bEnableEnglishDialoguePagination=true;
    /** Existing independent translations are always preserved and require review after Origin changes. */
    UPROPERTY(Config,EditAnywhere,Category="Authoring",meta=(DisplayName="同步未翻译字段 / Follow Origin",ToolTip="默认开启。关闭时，已打开编辑区内未翻译的字段保留修改前的原文；已有独立译文始终保留。"))
    bool bFollowOriginForUntranslated=true;
    virtual FName GetCategoryName() const override {return TEXT("Game");}
    /** Use valid UE culture names. en is reserved for Origin; formal English uses en-US. */
    UPROPERTY(Config,EditAnywhere,Category="Languages") TArray<FString> TargetLanguages={TEXT("en-US"),TEXT("zh-Hans")};
    UPROPERTY(Config,EditAnywhere,Category="Languages") FString DefaultReleaseLanguage=TEXT("en-US");
};
