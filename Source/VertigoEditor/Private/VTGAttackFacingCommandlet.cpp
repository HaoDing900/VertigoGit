#include "VTGAttackFacingCommandlet.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LatentActionManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/VTGCombatStatics.h"

namespace
{
const FString FacingMarker(TEXT("VTG attack-only facing v1"));

UEdGraphNode* Named(UEdGraph* Graph, const TCHAR* Name)
{
    for (UEdGraphNode* Node : Graph->Nodes) if (Node->GetName() == Name) return Node;
    checkf(false, TEXT("Missing node %s"), Name);
    return nullptr;
}
UEdGraphPin* Pin(UEdGraphNode* Node, const TCHAR* Name)
{
    return Node->FindPinChecked(FName(Name));
}
void Link(UEdGraphPin* A, UEdGraphPin* B)
{
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A, B));
}
template<class T> T* Add(UEdGraph* Graph, int32 X, int32 Y)
{
    auto* Node = NewObject<T>(Graph);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->NodePosX = X; Node->NodePosY = Y;
    Node->NodeComment = FacingMarker;
    return Node;
}
UK2Node_CallFunction* Call(UEdGraph* Graph, UFunction* Function, int32 X, int32 Y)
{
    check(Function);
    auto* Node = Add<UK2Node_CallFunction>(Graph, X, Y);
    Node->SetFromFunction(Function);
    Node->AllocateDefaultPins();
    return Node;
}
void ApplyFacing(UBlueprint* BP, UEdGraph* Graph)
{
    TSet<FGuid> Original;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        check(!Node->NodeComment.Contains(FacingMarker));
        Original.Add(Node->NodeGuid);
    }
    const int32 OriginalCount = Original.Num();
    auto* Tick = CastChecked<UK2Node_Event>(Named(Graph, TEXT("K2Node_Event_3")));
    check(Tick->EventReference.GetMemberName() == TEXT("ReceiveTick"));
    auto* TickOut = Pin(Tick, TEXT("then"));
    check(TickOut->LinkedTo.Num() == 1 && TickOut->LinkedTo[0]->GetOwningNode() == Named(Graph, TEXT("K2Node_IfThenElse_84")));
    // This tick event contains only proximity facing. Keep its old graph for reference.
    TickOut->BreakAllPinLinks();
    Tick->NodeComment = FacingMarker + TEXT(": proximity no longer rotates the character");

    // Entering combat and finishing a combat dodge both return to natural locomotion.
    // The separate gun-aiming orientation nodes remain unchanged.
    for (const TCHAR* Name : {TEXT("K2Node_VariableSet_94"), TEXT("K2Node_VariableSet_33")})
    {
        auto* Node = CastChecked<UK2Node_VariableSet>(Named(Graph, Name));
        check(Node->VariableReference.GetMemberName() == TEXT("bOrientRotationToMovement"));
        auto* Value = Pin(Node, TEXT("bOrientRotationToMovement"));
        check(Value->LinkedTo.IsEmpty() && Value->DefaultValue == TEXT("false"));
        GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Value, TEXT("true"));
        Node->NodeComment = FacingMarker + TEXT(": follow movement direction even in combat");
    }

    auto* Attack = CastChecked<UK2Node_CustomEvent>(Named(Graph, TEXT("K2Node_CustomEvent_36")));
    check(Attack->CustomFunctionName == TEXT("ExcuteCombo"));
    auto* Before = Pin(Attack, TEXT("then"));
    check(Before->LinkedTo.Num() == 1);
    auto* After = Before->LinkedTo[0];
    check(After->GetOwningNode() == Named(Graph, TEXT("K2Node_CallFunction_187")));
    auto* Nearest = Call(Graph, BP->SkeletonGeneratedClass->FindFunctionByName(TEXT("FindNearestEnemy")), -7000, 9264);
    auto* Valid = Add<UK2Node_IfThenElse>(Graph, -6712, 9264); Valid->AllocateDefaultPins();
    auto* Face = Call(Graph, AActor::StaticClass()->FindFunctionByName(TEXT("K2_SetActorRotation")), -6400, 9264);
    auto* Yaw = CastChecked<UK2Node_CallFunction>(Named(Graph, TEXT("K2Node_CallFunction_305")));
    check(Yaw->FunctionReference.GetMemberName() == TEXT("MakeRotator"));
    // Reuse the existing yaw-only calculation, preserving pitch/roll. Do not use
    // the old frame interpolation or its 120 cm cutoff: the first hit must face
    // its target immediately, including an enemy standing right beside us.
    Link(Pin(Yaw, TEXT("ReturnValue")), Pin(Face, TEXT("NewRotation")));
    Before->BreakLinkTo(After);
    Link(Before, Pin(Nearest, TEXT("execute")));
    Link(Pin(Nearest, TEXT("then")), Pin(Valid, TEXT("execute")));
    Link(Pin(Nearest, TEXT("FoundTarget")), Pin(Valid, TEXT("Condition")));
    Link(Pin(Valid, TEXT("then")), Pin(Face, TEXT("execute")));
    Link(Pin(Face, TEXT("then")), After);
    Link(Pin(Valid, TEXT("else")), After);
    Nearest->NodeComment = FacingMarker + TEXT(": refresh the nearest living target for EACH combo strike");
    Valid->NodeComment = FacingMarker + TEXT(": no target keeps the current direction and original attack flow");
    Face->NodeComment = FacingMarker + TEXT(": face target before the attack montage starts; never on Tick");
    for (UEdGraphNode* Node : Graph->Nodes) Original.Remove(Node->NodeGuid);
    check(Original.IsEmpty());
    UE_LOG(LogTemp, Display, TEXT("FACING: retained %d original nodes, added %d"), OriginalCount, Graph->Nodes.Num() - OriginalCount);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
}

bool TestFacing(UBlueprint* BP)
{
    TGuardValue<bool> AllowScript(GAllowActorScriptExecutionInEditor, true);
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("AttackFacingTest"), nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->DeltaTimeSeconds = 1.f / 60.f;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player = World->SpawnActor<ACharacter>(BP->GeneratedClass, FTransform::Identity, Spawn);
    check(Player);
    auto* Mesh = Player->GetMesh();
    Mesh->SetAnimInstanceClass(UAnimInstance::StaticClass()); Mesh->InitAnim(true);
    auto* Anim = Mesh->GetAnimInstance(); check(Anim);
    auto* EnemyClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/Characters/NPC/GenericOnes/DrunkBarFightGuy/BP_Enm_BarFighter.BP_Enm_BarFighter_C"));
    check(EnemyClass);
    auto* Far = World->SpawnActor<ACharacter>(EnemyClass, FTransform(FVector(400, 0, 0)), Spawn);
    auto* Near = World->SpawnActor<ACharacter>(EnemyClass, FTransform(FVector(0, 240, 0)), Spawn);
    check(Far && Near);
    auto* TargetProp = FindFProperty<FObjectPropertyBase>(Player->GetClass(), TEXT("CurrentTarget")); check(TargetProp);
    auto* Combo = FindFProperty<FIntProperty>(Player->GetClass(), TEXT("ComboIndex")); check(Combo);
    auto SetBool = [&](const TCHAR* Name, bool Value) { auto* P = FindFProperty<FBoolProperty>(Player->GetClass(), Name); check(P); P->SetPropertyValue_InContainer(Player, Value); };
    auto Run = [&](const TCHAR* Name) { auto* F = Player->FindFunction(Name); check(F); FStructOnScope Args(F); Player->ProcessEvent(F, Args.GetStructMemory()); };
    auto Target = [&]() { return TargetProp->GetObjectPropertyValue_InContainer(Player); };
    auto AnimTick = [&]() {
        Mesh->TickAnimation(1.f/60.f, false); Mesh->ConditionallyDispatchQueuedAnimEvents();
        World->GetLatentActionManager().BeginFrame();
        World->GetLatentActionManager().ProcessLatentActions(Player, 1.f/60.f);
    };
    auto Clear = [&]() { Anim->Montage_Stop(0); AnimTick(); AnimTick(); Run(TEXT("ResetCombo")); };
    auto YawIs = [&](float Yaw) { return FMath::Abs(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, Yaw)) < .1f; };
    int32 Failures = 0;
    auto Expect = [&](bool Pass, const TCHAR* Name) { UE_LOG(LogTemp, Display, TEXT("FACING TEST %s: %s"), Pass ? TEXT("PASS") : TEXT("FAIL"), Name); Failures += !Pass; };

    Expect(UVTGCombatStatics::CanReceiveDamage(Near) && UVTGCombatStatics::CanReceiveDamage(Far), TEXT("real enemy blueprints are valid damageable targets"));
    Run(TEXT("FindNearestEnemy"));
    Expect(Target() == Near, TEXT("existing 500 cm query finds nearest enemy"));
    SetBool(TEXT("InCombat"), true); Run(TEXT("ToggleCombatMode"));
    Expect(Player->GetCharacterMovement()->bOrientRotationToMovement, TEXT("combat entry preserves movement-facing locomotion"));
    Player->SetActorRotation(FRotator(0, -35, 0));
    for (int32 I=0; I<10; ++I)
        if (auto* Tick = Player->FindFunction(TEXT("ReceiveTick")))
        {
            FStructOnScope Args(Tick);
            FindFProperty<FFloatProperty>(Tick, TEXT("DeltaSeconds"))->SetPropertyValue_InContainer(Args.GetStructMemory(), 1.f/60.f);
            Player->ProcessEvent(Tick, Args.GetStructMemory());
        }
    Expect(YawIs(-35), TEXT("nearby enemy does not rotate an idle player over successive ticks"));

    SetBool(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"), false);
    SetBool(TEXT("IsDead?"), false);
    Clear();
    // Deliberately stale selection: attack must query again rather than use it.
    TargetProp->SetObjectPropertyValue_InContainer(Player, Far);
    Run(TEXT("Punch"));
    Expect(Target() == Near && YawIs(90) && Anim->GetCurrentActiveMontage(), TEXT("first punch refreshes stale target and faces nearest enemy before playing"));
    Expect(Player->GetCharacterMovement()->MovementMode == MOVE_None, TEXT("original attack movement lock is retained"));
    Near->SetActorLocation(FVector(0, 700, 0)); Far->SetActorLocation(FVector(200, -200, 0));
    SetBool(TEXT("IsInComboWindow"), true); Run(TEXT("Punch"));
    Expect(Target() == Far && YawIs(-45) && Combo->GetPropertyValue_InContainer(Player) == 1, TEXT("next combo strike retargets the new nearest enemy"));

    Clear();
    Far->SetActorLocation(FVector(-70, 0, 0)); Player->SetActorRotation(FRotator::ZeroRotator);
    Run(TEXT("Punch"));
    Expect(Target() == Far && YawIs(180), TEXT("point-blank enemy behind player is faced without old 120 cm dead zone"));
    Clear();
    auto* Dead = FindFProperty<FBoolProperty>(Far->GetClass(), TEXT("Dead?")); check(Dead);
    Dead->SetPropertyValue_InContainer(Far, true);
    Near->SetActorLocation(FVector(0, -240, 0)); Run(TEXT("Punch"));
    Expect(Target() == Near && YawIs(-90), TEXT("nearer corpse is ignored when choosing attack direction"));
    Clear();
    Near->SetCanBeDamaged(false); Player->SetActorRotation(FRotator(0, 37, 0)); Run(TEXT("Punch"));
    Expect(Target() == nullptr && YawIs(37) && Anim->GetCurrentActiveMontage(), TEXT("no eligible target keeps facing and original attack behavior"));
    auto* Last = Anim->GetCurrentActiveMontage();
    for (int32 I=0; Last && I<FMath::CeilToInt((Last->GetPlayLength()+1)*60); ++I) AnimTick();
    Expect(Player->GetCharacterMovement()->MovementMode == MOVE_Walking && Player->GetCharacterMovement()->bOrientRotationToMovement, TEXT("attack completion returns to natural movement"));

    SetBool(TEXT("StoryMode?"), false);
    auto* Dodge = Player->FindFunction(TEXT("DodgeSystem")); check(Dodge);
    FStructOnScope DodgeArgs(Dodge);
    auto* Forward = FindFProperty<FNumericProperty>(Dodge, TEXT("DodgeForward")); check(Forward);
    Forward->SetFloatingPointPropertyValue(Forward->ContainerPtrToValuePtr<void>(DodgeArgs.GetStructMemory()), 1.0);
    Player->ProcessEvent(Dodge, DodgeArgs.GetStructMemory());
    auto* DodgeMontage = Anim->GetCurrentActiveMontage();
    Expect(DodgeMontage != nullptr, TEXT("original combat dodge still plays"));
    for (int32 I=0; DodgeMontage && I<FMath::CeilToInt((DodgeMontage->GetPlayLength()+1)*60); ++I) AnimTick();
    Expect(Player->GetCharacterMovement()->bOrientRotationToMovement, TEXT("combat dodge completion restores movement-facing locomotion"));

    SetBool(TEXT("InCombat"), false); Run(TEXT("ToggleCombatMode"));
    Expect(Player->GetCharacterMovement()->bOrientRotationToMovement && Target() == nullptr, TEXT("leaving combat still restores locomotion and clears target"));
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    UE_LOG(LogTemp, Display, TEXT("FACING TEST failures=%d"), Failures);
    return Failures == 0;
}
}

UVTGAttackFacingCommandlet::UVTGAttackFacingCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UVTGAttackFacingCommandlet::Main(const FString& Params)
{
    auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa"));
    if (!BP) return 1;
    if (!Params.Contains(TEXT("Inspect")))
    {
        UEdGraph* Graph = nullptr;
        for (UEdGraph* G : BP->UbergraphPages) if (G->GetName() == TEXT("EventGraph")) Graph = G;
        if (!Graph) return 2;
        if (!Params.Contains(TEXT("VerifyOnly"))) ApplyFacing(BP, Graph);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
        UE_LOG(LogTemp, Display, TEXT("FACING compile errors=%d warnings=%d"), Results.NumErrors, Results.NumWarnings);
        if (Results.NumErrors || BP->Status == BS_Error) return 3;
        if (Params.Contains(TEXT("Test")) && !TestFacing(BP)) return 4;
        if (Params.Contains(TEXT("VerifyOnly"))) return 0;
        FSavePackageArgs Save; Save.TopLevelFlags = RF_Public | RF_Standalone; Save.SaveFlags = SAVE_NoError;
        const FString Filename = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(BP->GetOutermost(), BP, *Filename, Save)) return 5;
        UE_LOG(LogTemp, Display, TEXT("FACING saved only %s"), *Filename);
        return 0;
    }
    TArray<UEdGraph*> Graphs;
    BP->GetAllGraphs(Graphs);
    for (auto* Graph : Graphs)
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        auto* Variable = Cast<UK2Node_VariableSet>(Node);
        if (Graph->GetName() != TEXT("FindNearestEnemy") &&
            !(Node->NodePosX < -15000 && Node->NodePosY > 6000 && Node->NodePosY < 7300) &&
            !(Node->NodePosX > -6100 && Node->NodePosX < -3400 && Node->NodePosY > 8200 && Node->NodePosY < 9600) &&
            !(Variable && Variable->VariableReference.GetMemberName() == TEXT("bOrientRotationToMovement"))) continue;
        UE_LOG(LogTemp, Display, TEXT("FACING NODE %s/%s %s (%d,%d)"), *Graph->GetName(), *Node->GetName(), *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), Node->NodePosX, Node->NodePosY);
        for (auto* Pin : Node->Pins)
        {
            FString Links;
            for (auto* Other : Pin->LinkedTo) Links += Other->GetOwningNode()->GetName() + TEXT(".") + Other->PinName.ToString() + TEXT(" ");
            UE_LOG(LogTemp, Display, TEXT("FACING PIN %s=%s %s -> %s"), *Pin->PinName.ToString(), *Pin->DefaultValue, *GetNameSafe(Pin->DefaultObject), *Links);
        }
    }
    return 0;
}
