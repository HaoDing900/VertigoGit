#include "VTGPauseExitPageCommandlet.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace PauseExit
{
const TCHAR* BlueprintPath = TEXT("/Game/ThirdPerson/Blueprints/UI/WBP_PauseMenu");
const TCHAR* ExitButton = TEXT("B_Exit");
/**
 * The authored English. The old text carried an auto-generated key in the empty namespace, which is
 * what let one key collide with another source string and lock the translation panel.
 */
const TCHAR* LabelNamespace = TEXT("VTG.WidgetLabels");
const TCHAR* LabelKey = TEXT("WBP_PauseMenu.ExitEntryText");
const TCHAR* LabelSource = TEXT("EXIT GAME");

void Backup(UObject* Object)
{
    const FString File = FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(),
                                                                 FPackageName::GetAssetPackageExtension());
    const FString To = FPaths::ProjectDir() / TEXT("Scripts/PauseExitPage/Backup") / FPaths::GetCleanFilename(File);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(To), true);
    if (!IFileManager::Get().FileExists(*To))
        check(IFileManager::Get().Copy(*To, *File) == COPY_OK);
}
}

UVTGPauseExitPageCommandlet::UVTGPauseExitPageCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UVTGPauseExitPageCommandlet::Main(const FString& Params)
{
    using namespace PauseExit;
    const bool bApply = FParse::Param(*Params, TEXT("Apply"));
    auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, BlueprintPath);
    check(Blueprint && Blueprint->WidgetTree);
    auto* Button = Cast<UButton>(Blueprint->WidgetTree->FindWidget(ExitButton));
    check(Button);
    auto* Label = Cast<UTextBlock>(Button->GetContent());
    check(Label);

    bool bChanged = false;
    if (bApply)
        Backup(Blueprint);

    // ---- the Exit button must stop quitting the game straight from the Blueprint ------------------
    int32 QuitCalls = 0;
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        TArray<UEdGraphNode*> Doomed;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            auto* Bound = Cast<UK2Node_ComponentBoundEvent>(Node);
            if (!Bound || Bound->ComponentPropertyName != FName(ExitButton))
                continue;
            UEdGraphPin* Then = Bound->FindPin(TEXT("then"));
            if (!Then)
                continue;
            for (UEdGraphPin* Linked : Then->LinkedTo)
            {
                auto* Call = Cast<UK2Node_CallFunction>(Linked->GetOwningNode());
                if (!Call || Call->FunctionReference.GetMemberName() != FName("QuitGame"))
                    continue;
                ++QuitCalls;
                UE_LOG(LogTemp, Display, TEXT("PAUSE EXIT found %s on %s"), *Call->GetNodeTitle(ENodeTitleType::ListView).ToString(),
                       *Bound->GetName());
                if (bApply)
                    Doomed.AddUnique(Call);
            }
            if (bApply && !Doomed.IsEmpty())
            {
                GetDefault<UEdGraphSchema_K2>()->BreakPinLinks(*Then, true);
                bChanged = true;
            }
        }
        for (UEdGraphNode* Node : Doomed)
            Graph->RemoveNode(Node);
    }
    UE_LOG(LogTemp, Display, TEXT("PAUSE EXIT quit-game calls still wired to %s: %d"), ExitButton, bApply ? 0 : QuitCalls);

    // ---- the label needs a key nothing else can collide with --------------------------------------
    const FText Before = Label->GetText();
    const FString Namespace = FTextInspector::GetNamespace(Before).Get(FString());
    const FString Key = FTextInspector::GetKey(Before).Get(FString());
    UE_LOG(LogTemp, Display, TEXT("PAUSE EXIT label namespace=\"%s\" key=\"%s\" source=\"%s\""), *Namespace, *Key,
           FTextInspector::GetSourceString(Before) ? **FTextInspector::GetSourceString(Before) : *Before.ToString());
    if (bApply && Key != LabelKey)
    {
        Label->Modify();
        Label->SetText(FText::ChangeKey(LabelNamespace, LabelKey, FText::FromString(LabelSource)));
        bChanged = true;
    }
    const FText After = Label->GetText();
    check(FTextInspector::GetKey(After).Get(FString()) == LabelKey);
    check(FTextInspector::GetNamespace(After).Get(FString()).Contains(LabelNamespace));

    if (!bApply)
        return 0;
    if (bChanged)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
        UE_LOG(LogTemp, Display, TEXT("PAUSE EXIT compile errors=%d warnings=%d"), Results.NumErrors, Results.NumWarnings);
        if (Results.NumErrors || Blueprint->Status == BS_Error)
            return 3;
        UPackage* Package = Blueprint->GetOutermost();
        Package->FullyLoad();
        ResetLoaders(Package);
        Package->MarkPackageDirty();
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        check(UPackage::SavePackage(Package, Blueprint,
                                    *FPackageName::LongPackageNameToFilename(Package->GetName(),
                                                                             FPackageName::GetAssetPackageExtension()),
                                    Args));
    }
    UE_LOG(LogTemp, Display, TEXT("PAUSE EXIT PASS"));
    return 0;
}
