#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Fonts/SlateFontInfo.h"
#include "VTGSubtitleStyleSettings.generated.h"
UCLASS(Config=Game,DefaultConfig,meta=(DisplayName="Global Subtitle Style"))
class VERTIGO_API UVTGSubtitleStyleSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UVTGSubtitleStyleSettings();
    UPROPERTY(Config,EditAnywhere,Category="Global Subtitle Style",meta=(DisplayName="Use Global Subtitle Style")) bool bUseGlobalStyle=true;
    UPROPERTY(Config,EditAnywhere,Category="Global Subtitle Style") FSlateFontInfo SubtitleFont;
    UPROPERTY(Config,EditAnywhere,Category="Global Subtitle Style") FSlateFontInfo SpeakerFont;
    UPROPERTY(Config,EditAnywhere,Category="Global Subtitle Style") FLinearColor TextColor=FLinearColor::White;
    UPROPERTY(Config,EditAnywhere,Category="Global Subtitle Style") bool bOutlineEnabled=true;
    UPROPERTY(Config,EditAnywhere,Category="Global Subtitle Style",meta=(ClampMin="0",ClampMax="16",EditCondition="bOutlineEnabled")) int32 OutlineSize=2;
    UPROPERTY(Config,EditAnywhere,Category="Global Subtitle Style",meta=(EditCondition="bOutlineEnabled")) FLinearColor OutlineColor=FLinearColor::Black;
};
