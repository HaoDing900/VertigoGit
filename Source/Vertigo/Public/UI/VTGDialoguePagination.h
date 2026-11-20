#pragma once
#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "UObject/StrongObjectPtr.h"
namespace VTGDialoguePagination
{
/** Fits complete, independently balanced rich-text pages. Never splits a grapheme or tag. */
VERTIGO_API TArray<FString> Split(const FString& Text,TFunctionRef<bool(const FString&)> Fits,bool AllowWordSplit=false);
}

class URichTextBlock;
class UDataTable;
/** Per-instance fitting of translated side-dialogue text; restores authored styles for Origin. */
class VERTIGO_API FVTGDialogueTextFit
{
public:
    void Update(URichTextBlock* Label,const FVector2D& Available,bool Enabled,const FString* CompleteText=nullptr,bool AllowPages=true);
    void Restore();
    const TArray<FString>& GetPages() const { return Pages; }
private:
    TWeakObjectPtr<URichTextBlock> Target;
    TStrongObjectPtr<UDataTable> OriginalTable;
    FTextBlockStyle OriginalStyle;
    bool HadStyleOverride=false;
    FString LastText;
    TArray<FString> Pages;
    bool LastAllowPages=false;
    FVector2D LastSize=FVector2D::ZeroVector;
};
