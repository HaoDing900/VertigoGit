#include "VTGKeyBackgroundCommandlet.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Modules/ModuleManager.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UI/VTGWidgetStatics.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace KeyBackground
{
const TCHAR* OverlayPath = TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay");
/** The image the on-screen key prompts sit on; it used to be simply always on. */
const TCHAR* Background = TEXT("BottomBackgroundOfKey");
/** UMG looks for "<PropertyName>Delegate" on the widget, so this is the Visibility attribute. */
const TCHAR* BoundProperty = TEXT("Visibility");
const FName FunctionName = TEXT("Get_BottomBackgroundOfKey_Visibility");
/** The main text box and its two variants. Each one is shown and hidden from a different place in
    this Blueprint's 300-odd node dialogue chains, which is exactly why this reads them instead. */
const TCHAR* Boxes[] = {TEXT("DialogueBox"), TEXT("ThoughtBox"), TEXT("MonologueBox")};

template <typename TNode> TNode* Add(UEdGraph* Graph, int32 X, int32 Y)
{
    auto* Node = NewObject<TNode>(Graph);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->PostPlacedNewNode();
    Node->NodePosX = X;
    Node->NodePosY = Y;
    return Node;
}
UEdGraphPin* Pin(UEdGraphNode* Node, const TCHAR* Name)
{
    if (UEdGraphPin* Found = Node->FindPin(Name)) return Found;
    // A missing pin here means the node was built differently than assumed, so say which one it is
    // rather than leaving a bare assertion address.
    FString Have;
    for (UEdGraphPin* P : Node->Pins) Have += P->PinName.ToString() + TEXT(" ");
    UE_LOG(LogTemp, Error, TEXT("KEY BG no pin '%s' on %s (%s); it has: %s"), Name, *Node->GetName(),
           *Node->GetClass()->GetName(), *Have);
    checkNoEntry();
    return nullptr;
}
void Link(UEdGraphNode* A, const TCHAR* APin, UEdGraphNode* B, const TCHAR* BPin)
{
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(A, APin), Pin(B, BPin)));
}
}

UVTGKeyBackgroundCommandlet::UVTGKeyBackgroundCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UVTGKeyBackgroundCommandlet::Main(const FString& Params)
{
    using namespace KeyBackground;
    const bool bApply = FParse::Param(*Params, TEXT("Apply"));
    auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, OverlayPath);
    check(Blueprint && Blueprint->WidgetTree);
    UWidget* Image = Blueprint->WidgetTree->FindWidget(FName(Background));
    check(Image);

    if (bApply)
    {
        // ---- the function the binding calls -----------------------------------------------------
        // Rebuilt from scratch every run: it is generated, so a re-run is how it gets corrected.
        for (UEdGraph* G : TArray<UEdGraph*>(Blueprint->FunctionGraphs))
            if (G->GetFName() == FunctionName)
                FBlueprintEditorUtils::RemoveGraph(Blueprint, G, EGraphRemoveFlags::Recompile);
        UEdGraph* Graph = nullptr;
        {
            Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FunctionName, UEdGraph::StaticClass(),
                                                          UEdGraphSchema_K2::StaticClass());
            FBlueprintEditorUtils::AddFunctionGraph(Blueprint, Graph, false, static_cast<UClass*>(nullptr));

            UK2Node_FunctionEntry* Entry = nullptr;
            for (UEdGraphNode* N : Graph->Nodes)
                if (auto* E = Cast<UK2Node_FunctionEntry>(N)) Entry = E;
            check(Entry);
            // A UMG property binding is rejected unless the function is pure, so there is no Branch
            // here: the whole answer is one pure call.
            Entry->AddExtraFlags(FUNC_Public | FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const);
            Entry->MetaData.Category = FText::FromString(TEXT("Visibility"));
            Entry->NodeComment = TEXT("Bound to BottomBackgroundOfKey's Visibility.\n"
                                      "It follows the main text box, the thought box and the monologue box:\n"
                                      "on while any of them is up, off once the last one goes away.\n"
                                      "A fourth box (MailBox, say) can just be dragged into the spare pin.");
            Entry->bCommentBubbleVisible = true;

            const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
            UFunction* Follow =
                UVTGWidgetStatics::StaticClass()->FindFunctionByName(TEXT("VisibilityFollowingWidgets"));
            check(Follow);
            auto* Call = Add<UK2Node_CallFunction>(Graph, 540, 40);
            Call->SetFromFunction(Follow);
            Call->AllocateDefaultPins();
            // Set explicitly rather than trusting the C++ parameter defaults: an enum pin left empty
            // reads as entry 0, which for ESlateVisibility is Visible - the background would then
            // never go away, and the graph would look perfectly correct while doing it.
            Pin(Call, TEXT("WhenAnyVisible"))->DefaultValue = TEXT("Visible");
            Pin(Call, TEXT("WhenNoneVisible"))->DefaultValue = TEXT("Collapsed");

            const TCHAR* Slots[] = {TEXT("A"), TEXT("B"), TEXT("C")};
            int32 Row = 0;
            for (const TCHAR* Box : Boxes)
            {
                auto* Get = Add<UK2Node_VariableGet>(Graph, 220, 60 + Row * 110);
                Get->VariableReference.SetSelfMember(FName(Box));
                Get->AllocateDefaultPins();
                check(Schema->TryCreateConnection(Pin(Get, Box), Pin(Call, Slots[Row])));
                ++Row;
            }

            auto* Result = Add<UK2Node_FunctionResult>(Graph, 940, 40);
            Result->FunctionReference.SetSelfMember(FunctionName);
            Result->AllocateDefaultPins();
            if (!Result->FindPin(TEXT("ReturnValue")))
            {
                FEdGraphPinType Visibility;
                Visibility.PinCategory = UEdGraphSchema_K2::PC_Byte;
                Visibility.PinSubCategoryObject = StaticEnum<ESlateVisibility>();
                Result->CreateUserDefinedPin(TEXT("ReturnValue"), Visibility, EGPD_Input);
            }
            check(Schema->TryCreateConnection(Pin(Call, TEXT("ReturnValue")), Pin(Result, TEXT("ReturnValue"))));
            Link(Entry, TEXT("then"), Result, TEXT("execute"));
        }

        // ---- and the binding itself --------------------------------------------------------------
        FDelegateEditorBinding Binding;
        Binding.ObjectName = Background;
        Binding.PropertyName = FName(BoundProperty);
        Binding.FunctionName = FunctionName;
        Binding.MemberGuid = Graph->GraphGuid;
        Binding.Kind = EBindingKind::Function;
        Blueprint->Bindings.Remove(Binding); // operator== is object+property, so this replaces an old one
        Blueprint->Bindings.Add(Binding);

        // Nothing is on screen before the first line arrives, so neither is the background.
        Image->Modify();
        Image->SetVisibility(ESlateVisibility::Collapsed);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
        UE_LOG(LogTemp, Display, TEXT("KEY BG compile errors=%d warnings=%d"), Results.NumErrors, Results.NumWarnings);
        if (Results.NumErrors || Blueprint->Status == BS_Error) return 3;
    }

    // ---- what the asset now says ----------------------------------------------------------------
    bool bHasFunction = false;
    for (UEdGraph* G : Blueprint->FunctionGraphs) bHasFunction |= G->GetFName() == FunctionName;
    UE_LOG(LogTemp, Display, TEXT("KEY BG function=%d defaultVisibility=%d bindings=%d"), bHasFunction,
           (int32)Image->GetVisibility(), Blueprint->Bindings.Num());
    for (const FDelegateEditorBinding& B : Blueprint->Bindings)
        UE_LOG(LogTemp, Display, TEXT("KEY BG binding object=%s property=%s function=%s kind=%d"), *B.ObjectName,
               *B.PropertyName.ToString(), *B.FunctionName.ToString(), (int32)B.Kind);

    // ---- does the binding actually drive the image? ----------------------------------------------
    // The compiler only checks that the function is pure and matches the delegate; whether the image
    // then follows the boxes is a runtime question, so build the widget and ask it.
    if (FParse::Param(*Params, TEXT("Verify")))
    {
        TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor, true);
        if (!FSlateApplication::IsInitialized())
            FSlateApplication::InitializeAsStandaloneApplication(
                FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer"))
                    .CreateSlateRHIRenderer());
        const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
                              .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("KeyBackgroundTest"), nullptr, true,
                                          ERHIFeatureLevel::Num, &Init);
        check(World);
        auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
        Context.SetCurrentWorld(World);
        auto* GI = NewObject<UGameInstance>();
        World->SetGameInstance(GI);
        Context.OwningGameInstance = GI;
        auto* PC = World->SpawnActor<APlayerController>();
        auto* Local = NewObject<ULocalPlayer>(GEngine);
        GI->AddLocalPlayer(Local, 0);
        PC->SetPlayer(Local);
        World->AddController(PC);

        TSubclassOf<UUserWidget> OverlayClass = Blueprint->GeneratedClass;
        check(OverlayClass);
        UUserWidget* Overlay = CreateWidget<UUserWidget>(PC, OverlayClass);
        check(Overlay);
        Overlay->TakeWidget(); // GetVisibility only reports the bound attribute once Slate exists
        auto* Live = Overlay->WidgetTree->FindWidget(FName(Background));
        check(Live);
        // Ask the bound delegate itself. Slate only re-evaluates a bound attribute during its own
        // update pass, which a commandlet never runs, so reading GetVisibility here would prove
        // nothing either way - whereas this is exactly what Slate will call each time.
        check(Live->VisibilityDelegate.IsBound());
        auto Answer = [&] { return Live->VisibilityDelegate.Execute(); };
        for (const TCHAR* Box : Boxes)
            Overlay->WidgetTree->FindWidget(FName(Box))->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogTemp, Display, TEXT("KEY BG verify noBoxes -> %d"), (int32)Answer());
        check(Answer() == ESlateVisibility::Collapsed);
        for (const TCHAR* Box : Boxes)
        {
            UWidget* Shown = Overlay->WidgetTree->FindWidget(FName(Box));
            Shown->SetVisibility(ESlateVisibility::Visible);
            UE_LOG(LogTemp, Display, TEXT("KEY BG verify %s -> %d"), Box, (int32)Answer());
            check(Answer() == ESlateVisibility::Visible);
            // HitTestInvisible is how DialogueBox_Side and Line_Side are shown, and it is on screen.
            Shown->SetVisibility(ESlateVisibility::HitTestInvisible);
            check(Answer() == ESlateVisibility::Visible);
            Shown->SetVisibility(ESlateVisibility::Collapsed);
            check(Answer() == ESlateVisibility::Collapsed);
        }
        UE_LOG(LogTemp, Display, TEXT("KEY BG VERIFY PASS"));
    }

    if (!bApply) return 0;
    check(bHasFunction);
    check(Blueprint->Bindings.Num() == 1);
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
    UE_LOG(LogTemp, Display, TEXT("KEY BG PASS"));
    return 0;
}
