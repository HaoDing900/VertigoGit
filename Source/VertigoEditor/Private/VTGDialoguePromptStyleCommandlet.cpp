#include "VTGDialoguePromptStyleCommandlet.h"
#include "CommonUITypes.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

UVTGDialoguePromptStyleCommandlet::UVTGDialoguePromptStyleCommandlet()
{
    IsEditor = true;
    IsClient = false;
    IsServer = false;
    LogToConsole = true;
}

int32 UVTGDialoguePromptStyleCommandlet::Main(const FString& Params)
{
    const bool Verify = FParse::Param(*Params, TEXT("VerifyOnly"));
    auto* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Narrative/NarrativeUI/DT_DialogueControls"));
    if (!Table) return 1;
    auto* Keyboard = FindFProperty<FStructProperty>(FCommonInputActionDataBase::StaticStruct(), TEXT("KeyboardInputTypeInfo"));
    if (!Keyboard) return 5;
    struct FPrompt { const TCHAR* Row; const TCHAR* Image; FKey Key; };
    const FPrompt Prompts[] = {
        {TEXT("Continue"), TEXT("T_Space_Key_Dark"), EKeys::SpaceBar},
        {TEXT("ToggleAuto"), TEXT("T_A_Key_Dark"), EKeys::A},
        {TEXT("History"), TEXT("T_Mouse_Middle_Key_Dark"), EKeys::MouseScrollUp},
        {TEXT("SkipHold"), TEXT("T_Ctrl_Key_Dark"), EKeys::LeftControl}
    };
    for (const auto& Prompt : Prompts)
    {
        auto* Row = Table->FindRow<FCommonInputActionDataBase>(Prompt.Row, TEXT("Dialogue prompt style"));
        auto* Texture = LoadObject<UTexture2D>(nullptr, *(FString(TEXT("/Game/Inventory/InventoryXBased/UMG/Textures/Controls/PC/")) + Prompt.Image));
        if (!Row || !Texture) return 2;
        auto* Info = Keyboard->ContainerPtrToValuePtr<FCommonInputTypeInfo>(Row);
        if (Info->GetKey() != Prompt.Key) return 2;
        auto& Brush = Info->OverrideBrush;
        if (!Verify)
        {
            Brush = FSlateBrush();
            Brush.SetResourceObject(Texture);
            Brush.ImageSize = FVector2D(45, 45); // WBP_HintLayer.SkipHintKey brush and 45-unit row height.
            Brush.DrawAs = ESlateBrushDrawType::Image;
        }
        if (Brush.GetResourceObject() != Texture || !FVector2D(Brush.ImageSize).Equals(FVector2D(45,45))) return 3;
        UE_LOG(LogTemp, Display, TEXT("DIALOGUE_PROMPT %s key=%s image=%s size=45x45"), Prompt.Row, *Prompt.Key.ToString(), *Texture->GetPathName());
    }
    if (!Verify)
    {
        Table->MarkPackageDirty();
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(Table->GetOutermost(), Table,
            *FPackageName::LongPackageNameToFilename(Table->GetOutermost()->GetName(), TEXT(".uasset")), Args)) return 4;
    }
    UE_LOG(LogTemp, Display, TEXT("DIALOGUE_PROMPT_STYLE_PASS"));
    return 0;
}
