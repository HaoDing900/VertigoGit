#pragma once
#include "CoreMinimal.h"
class IPropertyHandle;
class SDockTab;
class SWidget;

namespace VTGManualLocalization
{
    void Startup();
    void Shutdown();
    /** Validate the protected format structure, independently of the translated prose. */
    bool Validate(const FString& Source, const FString& Translation, FString& Error);
    FString EncodeDraft(const FString& Source,const FString& Translation);
    bool DecodeDraft(const FString& Source,const FString& Draft,FString& Translation,FString& Error);
    bool Compile(FString& Error);
    /** Read text identities from a loaded asset and its package objects; never modifies assets. */
    bool GatherAsset(UObject* Asset, FString& Error);
    TSharedRef<SWidget> MakeDialogueTranslations(TSharedPtr<IPropertyHandle> Property);
    TSharedRef<SWidget> MakeInlineTranslations(TSharedPtr<IPropertyHandle> Property);
    bool GetDialogueDraft(TSharedPtr<IPropertyHandle> Property,const FString& Culture,FString& Origin,FString& Draft,FString& Error);
    bool SaveDialogueDraft(TSharedPtr<IPropertyHandle> Property,const FString& Culture,const FString& ExpectedOrigin,const FString& Draft,FString& Error);
#if WITH_DEV_AUTOMATION_TESTS
    TSharedRef<SDockTab> MakeTestPanel(TSharedPtr<IPropertyHandle> Handle);
    bool HasCollectedTextForTests(const FString& Namespace,const FString& Key);
    void SetTestStorageDirectory(const FString& Directory);
#endif
}
