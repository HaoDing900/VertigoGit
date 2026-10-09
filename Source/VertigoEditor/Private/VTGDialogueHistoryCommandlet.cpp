#include "VTGDialogueHistoryCommandlet.h"
#include "CommonUITypes.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

UVTGDialogueHistoryCommandlet::UVTGDialogueHistoryCommandlet()
{
    IsEditor = true;
    IsClient = false;
    IsServer = false;
    LogToConsole = true;
}
int32 UVTGDialogueHistoryCommandlet::Main(const FString &Params)
{
    const bool Verify = FParse::Param(*Params, TEXT("VerifyOnly"));
    const TCHAR* TablePath = TEXT("/Game/Narrative/NarrativeUI/DT_DialogueControls");
    auto* Table = LoadObject<UDataTable>(nullptr, TablePath, nullptr, LOAD_NoWarn);
    if (!Table && Verify) return 20;
    if (!Table)
    {
        auto* Package = CreatePackage(TablePath);
        Table = NewObject<UDataTable>(Package, TEXT("DT_DialogueControls"), RF_Public | RF_Standalone);
        Table->RowStruct = FCommonInputActionDataBase::StaticStruct();
        FAssetRegistryModule::AssetCreated(Table);
    }
    if (!Verify)
    {
        auto* Source = LoadObject<UDataTable>(nullptr, TEXT("/NarrativeCommonUI/ControllerData/DT_NarrativeInputActions"));
        auto* Skip = Source ? Source->FindRow<FCommonInputActionDataBase>(TEXT("SkipDialogueLine"), TEXT("Dialogue controls")) : nullptr;
        if (!Skip) return 21;
        auto* Keyboard = FindFProperty<FStructProperty>(FCommonInputActionDataBase::StaticStruct(), TEXT("KeyboardInputTypeInfo"));
        auto* Gamepad = FindFProperty<FStructProperty>(FCommonInputActionDataBase::StaticStruct(), TEXT("DefaultGamepadInputTypeInfo"));
        if (!Keyboard || !Gamepad) return 22;
        for (const FName Name : {FName(TEXT("ToggleAuto")), FName(TEXT("History"))})
        {
            if (Table->FindRow<FCommonInputActionDataBase>(Name, TEXT("Existing"), false)) continue;
            FCommonInputActionDataBase Row = *Skip;
            const bool Auto = Name == TEXT("ToggleAuto");
            Row.DisplayName = FText::FromString(Auto ? TEXT("Auto: ON") : TEXT("History"));
            Keyboard->ContainerPtrToValuePtr<FCommonInputTypeInfo>(&Row)->SetKey(Auto ? EKeys::A : EKeys::MouseScrollUp);
            Gamepad->ContainerPtrToValuePtr<FCommonInputTypeInfo>(&Row)->SetKey(Auto ? EKeys::Gamepad_LeftShoulder : EKeys::Gamepad_RightShoulder);
            Table->AddRow(Name, Row);
        }
        for (const FName Name : {FName(TEXT("Continue")), FName(TEXT("SkipHold"))})
        {
            if (Table->FindRow<FCommonInputActionDataBase>(Name, TEXT("Existing"), false)) continue;
            FCommonInputActionDataBase Row = *Skip;
            const bool Hold = Name == TEXT("SkipHold");
            Row.DisplayName = FText::FromString(Hold ? TEXT("Skip (Hold)") : TEXT("Continue"));
            Row.HoldDisplayName = Row.DisplayName;
            auto* KB = Keyboard->ContainerPtrToValuePtr<FCommonInputTypeInfo>(&Row);
            auto* GP = Gamepad->ContainerPtrToValuePtr<FCommonInputTypeInfo>(&Row);
            KB->SetKey(Hold ? EKeys::LeftControl : EKeys::SpaceBar);
            GP->SetKey(Hold ? EKeys::Gamepad_FaceButton_Left : EKeys::Gamepad_FaceButton_Bottom);
            // Repetition is owned by the session controller, not CommonUI's one-shot hold action.
            KB->bActionRequiresHold = false;
            GP->bActionRequiresHold = false;
            Table->AddRow(Name, Row);
        }
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(Table->GetOutermost(), Table, *FPackageName::LongPackageNameToFilename(TablePath, TEXT(".uasset")), Args)) return 23;
    }
    if (!Table->FindRow<FCommonInputActionDataBase>(TEXT("ToggleAuto"), TEXT("Verify")) ||
        !Table->FindRow<FCommonInputActionDataBase>(TEXT("History"), TEXT("Verify"))) return 24;
    int32 Seen = 0;
    for (const TCHAR *Path : {TEXT("/Game/Narrative/NarrativeUI/Widgets/W_NarrativeMenu_Dialogue")})
    {
        auto *BP = LoadObject<UBlueprint>(nullptr, Path);
        if (!BP)
            return 1;
        bool Changed = false;
        UObject* Defaults = BP->GeneratedClass->GetDefaultObject();
        auto* Back = FindFProperty<FBoolProperty>(Defaults->GetClass(), TEXT("bIsBackActionDisplayedInActionBar"));
        auto* SkipAction = FindFProperty<FStructProperty>(Defaults->GetClass(), TEXT("SkipAction"));
        if (!SkipAction) return 27;
        auto* ContinueRow = SkipAction->ContainerPtrToValuePtr<FDataTableRowHandle>(Defaults);
        if (ContinueRow->DataTable != Table || ContinueRow->RowName != TEXT("Continue"))
        {
            if (Verify) return 28;
            ContinueRow->DataTable = Table;
            ContinueRow->RowName = TEXT("Continue");
            Changed = true;
        }
        if (!Back) return 25;
        if (Verify && Back->GetPropertyValue_InContainer(Defaults)) return 26;
        if (!Verify && Back->GetPropertyValue_InContainer(Defaults))
        {
            Back->SetPropertyValue_InContainer(Defaults, false);
            Changed = true;
        }
        TArray<UEdGraph *> Graphs;
        BP->GetAllGraphs(Graphs);
        for (UEdGraph *Graph : Graphs)
        {
            if (Graph->GetFName() != TEXT("BP_OnHandleBackAction"))
                continue;
            for (UEdGraphNode *Node : Graph->Nodes)
                if (auto *Call = Cast<UK2Node_CallFunction>(Node))
                    if (Call->FunctionReference.GetMemberName() == TEXT("TryExitDialogue"))
                    {
                        ++Seen;
                        auto *In = Call->FindPin(UEdGraphSchema_K2::PN_Execute);
                        auto *Out = Call->FindPin(UEdGraphSchema_K2::PN_Then);
                        if (!In || !Out)
                            return 2;
                        UE_LOG(LogTemp, Display, TEXT("Dialogue Back: %s / %s exit incoming=%d outgoing=%d"), Path,
                               *Graph->GetName(), In->LinkedTo.Num(), Out->LinkedTo.Num());
                        if (Verify)
                        {
                            if (!In->LinkedTo.IsEmpty())
                                return 3;
                            continue;
                        }
                        if (In->LinkedTo.IsEmpty())
                            continue;
                        auto Incoming = In->LinkedTo;
                        auto Outgoing = Out->LinkedTo;
                        In->BreakAllPinLinks();
                        Out->BreakAllPinLinks();
                        for (UEdGraphPin *Source : Incoming)
                            for (UEdGraphPin *Dest : Outgoing)
                                if (!Graph->GetSchema()->TryCreateConnection(Source, Dest))
                                    return 4;
                        Changed = true;
                    }
        }
        if (Changed)
        {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
            FCompilerResultsLog Results;
            FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
            if (Results.NumErrors)
                return 5;
            FSavePackageArgs Args;
            Args.TopLevelFlags = RF_Public | RF_Standalone;
            if (!UPackage::SavePackage(
                    BP->GetOutermost(), BP,
                    *FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension()), Args))
                return 6;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Dialogue Back exit removal: checked %d call sites (nodes preserved)."), Seen);
    return Seen == 1 ? 0 : 7;
}
