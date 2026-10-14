#include "UI/VTGLocalizationSubsystem.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/Internationalization.h"
#include "UI/VTGLocalizationSettings.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"

FString UVTGLocalizationSubsystem::ResolveConsoleLanguage(const FString& Input)
{
    FString Code=Input.TrimStartAndEnd();
    if(Code.Equals(TEXT("en"),ESearchCase::IgnoreCase)||Code.Equals(TEXT("english"),ESearchCase::IgnoreCase))Code=TEXT("en-US");
    else if(Code.Equals(TEXT("zh"),ESearchCase::IgnoreCase)||Code.Equals(TEXT("zh-CN"),ESearchCase::IgnoreCase)||Code.Equals(TEXT("chinese"),ESearchCase::IgnoreCase))Code=TEXT("zh-Hans");
    for(const auto& Language:GetLanguages())if(Code.Equals(Language,ESearchCase::IgnoreCase))return Language;
    return FString();
}

static FAutoConsoleCommandWithWorldAndArgs GVTGTextLanguage(
    TEXT("vtg.Language"),
    TEXT("Switch text language: vtg.Language origin | en | zh | <culture>. No arguments prints current/supported languages. Origin is editor-only; audio is unchanged."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        auto* Settings=World && World->GetGameInstance()?World->GetGameInstance()->GetSubsystem<UVTGLocalizationSubsystem>():nullptr;
        if(!Settings){UE_LOG(LogTemp,Warning,TEXT("vtg.Language: run this command in the game/PIE console while a game is running."));return;}
        const auto Supported=UVTGLocalizationSubsystem::GetLanguages();
        if(Args.IsEmpty()){UE_LOG(LogTemp,Display,TEXT("Text language: %s. Supported: %s"),*Settings->GetLanguage(),*FString::Join(Supported,TEXT(", ")));return;}
        const FString Code=Args.Num()==1?UVTGLocalizationSubsystem::ResolveConsoleLanguage(Args[0]):FString();
        if(Code.IsEmpty() || !Settings->SetLanguage(Code)){UE_LOG(LogTemp,Warning,TEXT("vtg.Language: invalid language. Use one of: %s"),*FString::Join(Supported,TEXT(", ")));return;}
        UE_LOG(LogTemp,Display,TEXT("Text language changed to %s (audio unchanged)."),*Code);
    }));

TArray<FString> UVTGLocalizationSubsystem::GetLanguages()
{
    TArray<FString> Result;
#if WITH_EDITOR
    Result.Add(TEXT("origin"));
#endif
    for(const auto& Language:GetDefault<UVTGLocalizationSettings>()->TargetLanguages)
        if(Language!=TEXT("en") && Language!=TEXT("origin") && FInternationalization::Get().GetCulture(Language).IsValid())Result.AddUnique(Language);
    if(Result.IsEmpty())Result.Add(TEXT("en-US"));
    return Result;
}

void UVTGLocalizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if WITH_EDITOR
    if(GIsEditor)
    {
        auto& Manager=FTextLocalizationManager::Get();PreviousPreviewEnabled=Manager.IsGameLocalizationPreviewEnabled();PreviousPreviewLanguage=Manager.GetConfiguredGameLocalizationPreviewLanguage();
    }
#endif
    FString Saved;
    GConfig->GetString(TEXT("Vertigo.Localization"), TEXT("TextLanguage"), Saved, GGameUserSettingsIni);
    if (!GetLanguages().Contains(Saved))
    {
#if WITH_EDITOR
        Saved=TEXT("origin");
#else
        const auto Languages=GetLanguages();
        Saved=GetDefault<UVTGLocalizationSettings>()->DefaultReleaseLanguage;
        if(!Languages.Contains(Saved))Saved=Languages[0];
        const auto System=UKismetInternationalizationLibrary::GetCurrentLanguage();
        for(const auto& Language:Languages)if(System==Language || System.StartsWith(Language+TEXT("-"))){Saved=Language;break;}
        if(System.StartsWith(TEXT("zh")) && Languages.Contains(TEXT("zh-Hans")))Saved=TEXT("zh-Hans");
#endif
    }
    SetLanguage(Saved);
}

void UVTGLocalizationSubsystem::Deinitialize()
{
#if WITH_EDITOR
    if(GIsEditor)
    {
        auto& Manager=FTextLocalizationManager::Get();
        if(PreviousPreviewEnabled)Manager.EnableGameLocalizationPreview(PreviousPreviewLanguage);else Manager.DisableGameLocalizationPreview();
    }
#endif
    Super::Deinitialize();
}

bool UVTGLocalizationSubsystem::SetLanguage(const FString& Language)
{
    if (!GetLanguages().Contains(Language)) return false;
    if (Language==CurrentLanguage) return true;
    // Origin uses the native en resource. Formal English is a separate en-US resource.
#if WITH_EDITOR
    if(GIsEditor)
    {
        auto& Manager=FTextLocalizationManager::Get();
        if(Language!=TEXT("origin") && Manager.GetNativeCultureName(ELocalizedTextSourceCategory::Game).IsEmpty())
        {UE_LOG(LogTemp,Warning,TEXT("Text language could not change: game localization resources are missing. Compile manual translations and check DefaultGame.ini LocalizationPaths."));return false;}
        if(Language==TEXT("origin"))Manager.DisableGameLocalizationPreview();else Manager.EnableGameLocalizationPreview(Language);
    }
    else
#endif
    if (!UKismetInternationalizationLibrary::SetCurrentLanguage(Language==TEXT("origin") ? TEXT("en") : Language, false)) return false;
    CurrentLanguage=Language;
    GConfig->SetString(TEXT("Vertigo.Localization"), TEXT("TextLanguage"), *Language, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
    OnLanguageChanged.Broadcast(Language);
    return true;
}
