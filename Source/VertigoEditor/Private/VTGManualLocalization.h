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
    /**
     * Drop every entry collected from one package, then rewrite the catalogue.
     *
     * For a package that no longer holds those texts - renamed, or the field removed. Compile
     * re-reads the source asset of every /Game/ entry and stops when one cannot be read, so a
     * stale context blocks the whole compile until it is forgotten.
     */
    bool ForgetAsset(const FString& PackageName, int32& OutRemoved, FString& Error);
    /** GatherAsset plus the catalogue write the editor panel does on save, for headless tooling. */
    bool GatherAssetAndSave(UObject* Asset, FString& Error);
    /**
     * Tick "this is player-visible text" for one entry, as the panel checkbox does.
     * Unapproved entries cannot be translated and are skipped by Compile, so text that is
     * player-facing by construction has to be marked before it can be localized at all.
     */
    bool ApproveDisplayText(const FString& Namespace, const FString& Key, FString& Error);
    /**
     * Write one culture's translation as the panel's save does, for headless tooling: the same
     * format validation, and the review snapshot taken from the entry's current Origin so Compile
     * accepts it instead of flagging it as stale.
     */
    bool SetTranslation(const FString& Namespace, const FString& Key, const FString& Culture, const FString& Text, FString& Error);
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
