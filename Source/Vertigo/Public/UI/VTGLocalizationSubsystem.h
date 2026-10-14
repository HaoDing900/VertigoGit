#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VTGLocalizationSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTGLanguageChanged, const FString&, Language);

/** Text language only. Audio and gameplay asset selection are never changed. */
UCLASS()
class VERTIGO_API UVTGLocalizationSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintPure, Category="Localization") static TArray<FString> GetLanguages();
    /** Console aliases are resolved to the same supported-language whitelist as the menu. */
    static FString ResolveConsoleLanguage(const FString& Input);
    UFUNCTION(BlueprintPure, Category="Localization") FString GetLanguage() const { return CurrentLanguage; }
    UFUNCTION(BlueprintCallable, Category="Localization") bool SetLanguage(const FString& Language);
    UPROPERTY(BlueprintAssignable, Category="Localization") FVTGLanguageChanged OnLanguageChanged;
private:
    FString CurrentLanguage;
#if WITH_EDITOR
    bool PreviousPreviewEnabled=false;
    FString PreviousPreviewLanguage;
#endif
};
