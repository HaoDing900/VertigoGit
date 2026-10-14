#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VTGLocalizationSettings.generated.h"

UCLASS(Config=Game,defaultconfig,meta=(DisplayName="Manual Localization / 手工本地化"))
class VERTIGO_API UVTGLocalizationSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override {return TEXT("Game");}
    /** Use valid UE culture names. en is reserved for Origin; formal English uses en-US. */
    UPROPERTY(Config,EditAnywhere,Category="Languages") TArray<FString> TargetLanguages={TEXT("en-US"),TEXT("zh-Hans")};
    UPROPERTY(Config,EditAnywhere,Category="Languages") FString DefaultReleaseLanguage=TEXT("en-US");
};
