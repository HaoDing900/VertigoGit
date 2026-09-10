#include "VTGRepairMeleeCommandlet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Engine/LatentActionManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
const FString Marker(TEXT("VTG melee interrupt repair v1"));
UEdGraphPin* Pin(UEdGraphNode* Node, const TCHAR* Name)
{
	check(Node);
	UEdGraphPin* Result = Node->FindPin(FName(Name));
	checkf(Result, TEXT("Missing pin %s on %s"), Name, *Node->GetName());
	return Result;
}
void Link(UEdGraphPin* A, UEdGraphPin* B)
{
	check(A && B);
	checkf(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A, B), TEXT("Cannot connect %s.%s -> %s.%s"), *A->GetOwningNode()->GetName(), *A->PinName.ToString(), *B->GetOwningNode()->GetName(), *B->PinName.ToString());
}
UEdGraphNode* Named(UEdGraph* Graph, const TCHAR* Name)
{
	for (UEdGraphNode* Node : Graph->Nodes) if (Node && Node->GetName() == Name) return Node;
	return nullptr;
}
UK2Node_CustomEvent* Event(UEdGraph* Graph, const TCHAR* Name)
{
	for (UEdGraphNode* Node : Graph->Nodes)
		if (auto* E = Cast<UK2Node_CustomEvent>(Node); E && E->CustomFunctionName == FName(Name)) return E;
	return nullptr;
}
template<class T> T* Add(UEdGraph* Graph, int32 X, int32 Y)
{
	T* Node = NewObject<T>(Graph);
	Graph->AddNode(Node, false, false);
	Node->CreateNewGuid();
	Node->NodePosX = X; Node->NodePosY = Y;
	Node->NodeComment = Marker;
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
template<class T> T* Variable(UEdGraph* Graph, UBlueprint* BP, const TCHAR* Name, int32 X, int32 Y)
{
	FProperty* Prop = FindFProperty<FProperty>(BP->SkeletonGeneratedClass, Name);
	checkf(Prop, TEXT("Missing variable %s"), Name);
	auto* Node = Add<T>(Graph, X, Y);
	Node->SetFromProperty(Prop, true, BP->SkeletonGeneratedClass);
	Node->AllocateDefaultPins();
	return Node;
}
void Insert(UEdGraphPin* Before, UEdGraphPin* In, UEdGraphPin* Out)
{
	const TArray<UEdGraphPin*> OldLinks = Before->LinkedTo;
	check(OldLinks.Num());
	Before->BreakAllPinLinks();
	Link(Before, In);
	for (UEdGraphPin* Old : OldLinks) Link(Out, Old);
}
void Repair(UBlueprint* BP, UEdGraph* Graph)
{
	for (UEdGraphNode* Node : Graph->Nodes) checkf(!Node->NodeComment.Contains(Marker), TEXT("Repair already applied; use -VerifyOnly"));
	TSet<FGuid> OriginalGuids;
	for (UEdGraphNode* Node : Graph->Nodes) OriginalGuids.Add(Node->NodeGuid);
	const int32 OriginalCount = Graph->Nodes.Num();
	auto* Montage = Named(Graph, TEXT("K2Node_PlayMontage_2"));
	check(Montage && Montage->NodeGuid == FGuid(0xAFC4C0F7, 0x4FBA3E71, 0x3C6A359C, 0x01E2968A));
	auto* EndEvent = Event(Graph, TEXT("EndAttackSafely"));
	auto* ResetEvent = Event(Graph, TEXT("ResetCombo"));
	check(EndEvent && ResetEvent);
	UEdGraphPin* Interrupted = Pin(Montage, TEXT("OnInterrupted"));
	check(Interrupted->LinkedTo.Num() == 1);
	auto* Print = CastChecked<UK2Node_CallFunction>(Interrupted->LinkedTo[0]->GetOwningNode());
	check(Print->FunctionReference.GetMemberName() == TEXT("PrintString"));
	check(Pin(Print, TEXT("InString"))->DefaultValue == TEXT("Interrupted"));
	check(Pin(Print, TEXT("then"))->LinkedTo.IsEmpty());
	check(Pin(Montage, TEXT("MontageToPlay"))->LinkedTo.Num() == 1);
	UEdGraphPin* SelectedMontage = Pin(Montage, TEXT("MontageToPlay"))->LinkedTo[0];
	UK2Node_CallFunction* OldAny = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
		if (auto* C = Cast<UK2Node_CallFunction>(Node); C && C->FunctionReference.GetMemberName() == TEXT("IsAnyMontagePlaying"))
		{ check(!OldAny); OldAny = C; }
	check(OldAny && Pin(OldAny, TEXT("ReturnValue"))->LinkedTo.Num() == 1);
	UEdGraphPin* ExistingCondition = Pin(OldAny, TEXT("ReturnValue"))->LinkedTo[0];
	check(Pin(OldAny, TEXT("self"))->LinkedTo.Num() == 1);
	UEdGraphPin* AnimInstance = Pin(OldAny, TEXT("self"))->LinkedTo[0];

	// The old Montage's callback may be queued while the next combo is starting.
	// A zero-duration Delay resumes on the next latent-action update, then checks
	// the CURRENT combo asset instead of any montage (e.g. a hit reaction).
	auto* Delay = Call(Graph, UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("Delay")), -4032, 9264);
	GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Pin(Delay, TEXT("Duration")), TEXT("0.0"));
	auto* EndCall = Call(Graph, BP->SkeletonGeneratedClass->FindFunctionByName(TEXT("EndAttackSafely")), -3760, 9264);
	Link(Pin(Print, TEXT("then")), Pin(Delay, TEXT("execute")));
	Link(Pin(Delay, TEXT("then")), Pin(EndCall, TEXT("execute")));
	Delay->NodeComment = Marker + TEXT(": wait for combo handoff before checking the active attack");

	// A dodge may already have reset the attack before the deferred check runs.
	auto* Guard = Add<UK2Node_IfThenElse>(Graph, -5664, 8144); Guard->AllocateDefaultPins();
	auto* Attacking = Variable<UK2Node_VariableGet>(Graph, BP, TEXT("IsAttacking"), -5872, 8224);
	Link(Pin(Attacking, TEXT("IsAttacking")), Pin(Guard, TEXT("Condition")));
	Insert(Pin(EndEvent, TEXT("then")), Pin(Guard, TEXT("execute")), Pin(Guard, TEXT("then")));
	auto* Playing = Call(Graph, UAnimInstance::StaticClass()->FindFunctionByName(TEXT("Montage_IsPlaying")), -5632, 7824);
	Link(AnimInstance, Pin(Playing, TEXT("self")));
	Link(SelectedMontage, Pin(Playing, TEXT("Montage")));
	auto* Valid = Call(Graph, UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("IsValid")), -5632, 7664);
	Link(SelectedMontage, Pin(Valid, TEXT("Object")));
	auto* Both = Call(Graph, UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("BooleanAND")), -5344, 7872);
	Link(Pin(Valid, TEXT("ReturnValue")), Pin(Both, TEXT("A")));
	Link(Pin(Playing, TEXT("ReturnValue")), Pin(Both, TEXT("B")));
	Pin(OldAny, TEXT("ReturnValue"))->BreakAllPinLinks();
	Link(Pin(Both, TEXT("ReturnValue")), ExistingCondition);
	// Retain the original node for reference; it no longer controls cleanup.
	OldAny->NodeComment += TEXT(" [Retained: melee cleanup now checks the selected attack montage]");
	Playing->NodeComment = Marker + TEXT(": a hit reaction must not keep the attack locked");

	// Reset the notify-driven combo window too, even if its end was interrupted.
	auto* ClearWindow = Variable<UK2Node_VariableSet>(Graph, BP, TEXT("IsInComboWindow"), -4960, 8656);
	GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Pin(ClearWindow, TEXT("IsInComboWindow")), TEXT("false"));
	Insert(Pin(ResetEvent, TEXT("then")), Pin(ClearWindow, TEXT("execute")), Pin(ClearWindow, TEXT("then")));

	// Preserve the death movement state if death interrupted an attack.
	auto* Walk = CastChecked<UK2Node_CallFunction>(Named(Graph, TEXT("K2Node_CallFunction_327")));
	check(Walk->FunctionReference.GetMemberName() == TEXT("SetMovementMode") && Pin(Walk, TEXT("NewMovementMode"))->DefaultValue == TEXT("MOVE_Walking"));
	check(Walk && Pin(Walk, TEXT("execute"))->LinkedTo.Num() == 1 && Pin(Walk, TEXT("then"))->LinkedTo.Num() == 1);
	UEdGraphPin* BeforeWalk = Pin(Walk, TEXT("execute"))->LinkedTo[0];
	UEdGraphPin* AfterWalk = Pin(Walk, TEXT("then"))->LinkedTo[0];
	auto* DeadBranch = Add<UK2Node_IfThenElse>(Graph, -4112, 8656); DeadBranch->AllocateDefaultPins();
	auto* Dead = Variable<UK2Node_VariableGet>(Graph, BP, TEXT("IsDead?"), -4336, 8752);
	Link(Pin(Dead, TEXT("IsDead?")), Pin(DeadBranch, TEXT("Condition")));
	BeforeWalk->BreakLinkTo(Pin(Walk, TEXT("execute")));
	Link(BeforeWalk, Pin(DeadBranch, TEXT("execute")));
	Link(Pin(DeadBranch, TEXT("else")), Pin(Walk, TEXT("execute")));
	Link(Pin(DeadBranch, TEXT("then")), AfterWalk);
	DeadBranch->NodeComment = Marker + TEXT(": clear attack flags without enabling walking after death");
	for (UEdGraphNode* Node : Graph->Nodes) OriginalGuids.Remove(Node->NodeGuid);
	check(OriginalGuids.IsEmpty());
	UE_LOG(LogTemp, Display, TEXT("MeleeRepair: retained all %d original EventGraph nodes; added %d"), OriginalCount, Graph->Nodes.Num() - OriginalCount);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
}
bool Compile(UBlueprint* BP)
{
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
	UE_LOG(LogTemp, Display, TEXT("MeleeRepair: compile errors=%d warnings=%d status=%d"), Results.NumErrors, Results.NumWarnings, int32(BP->Status));
	return Results.NumErrors == 0 && BP->Status != BS_Error;
}

// Exercise the real compiled character events and Unreal Montage callbacks in
// a transient world. No level is loaded, begun, or saved by these tests.
bool TestRepair(UBlueprint* BP)
{
	TGuardValue<bool> AllowScript(GAllowActorScriptExecutionInEditor, true);
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MeleeRepairTest"), nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Actor = World->SpawnActor<ACharacter>(BP->GeneratedClass, FTransform::Identity, Spawn);
	check(Actor);
	USkeletalMeshComponent* Mesh = Actor->GetMesh();
	// Isolate Montage lifecycle tests from the locomotion AnimBP and rendering.
	Mesh->SetAnimInstanceClass(UAnimInstance::StaticClass());
	Mesh->InitAnim(true);
	UAnimInstance* Anim = Mesh->GetAnimInstance();
	check(Anim);
	auto Bool = [&](const TCHAR* Name) { auto* P = FindFProperty<FBoolProperty>(Actor->GetClass(), Name); check(P); return P; };
	auto SetBool = [&](const TCHAR* Name, bool Value) { Bool(Name)->SetPropertyValue_InContainer(Actor, Value); };
	auto GetBool = [&](const TCHAR* Name) { return Bool(Name)->GetPropertyValue_InContainer(Actor); };
	auto* Index = FindFProperty<FIntProperty>(Actor->GetClass(), TEXT("ComboIndex")); check(Index);
	auto Run = [&](const TCHAR* Name) { UFunction* F = Actor->FindFunction(Name); check(F); Actor->ProcessEvent(F, nullptr); };
	auto Tick = [&]() {
		Mesh->TickAnimation(1.f / 60.f, false);
		Mesh->ConditionallyDispatchQueuedAnimEvents();
		World->GetLatentActionManager().BeginFrame();
		World->GetLatentActionManager().ProcessLatentActions(Actor, 1.f / 60.f);
	};
	int32 Failed = 0;
	auto Expect = [&](bool Condition, const TCHAR* Name) {
		UE_LOG(LogTemp, Display, TEXT("MeleeRepair TEST %s: %s"), Condition ? TEXT("PASS") : TEXT("FAIL"), Name);
		if (!Condition) ++Failed;
	};
	auto Start = [&](int32 Combo) {
		SetBool(TEXT("IsDead?"), false);
		SetBool(TEXT("IsAttacking"), true);
		SetBool(TEXT("IsInComboWindow"), true);
		SetBool(TEXT("SaveAttack"), true);
		Index->SetPropertyValue_InContainer(Actor, Combo);
		Run(TEXT("ExcuteCombo"));
		UAnimMontage* M = Anim->GetCurrentActiveMontage();
		Expect(M && Actor->GetCharacterMovement()->MovementMode == MOVE_None, TEXT("attack starts with a real Montage and locks movement"));
		return M;
	};
	auto Clean = [&]() {
		return !GetBool(TEXT("IsAttacking")) && !GetBool(TEXT("SaveAttack")) && !GetBool(TEXT("IsInComboWindow")) && Index->GetPropertyValue_InContainer(Actor) == 0;
	};

	Start(0);
	Anim->Montage_Stop(0.1f);
	Tick(); Tick();
	Expect(Clean() && Actor->GetCharacterMovement()->MovementMode == MOVE_Walking, TEXT("external stop unlocks without dodge"));

	UAnimMontage* First = Start(0);
	UAnimMontage* Second = Start(1);
	Tick(); Tick();
	Expect(First && Second && First != Second && Anim->Montage_IsPlaying(Second) && GetBool(TEXT("IsAttacking")) && Index->GetPropertyValue_InContainer(Actor) == 1 && Actor->GetCharacterMovement()->MovementMode == MOVE_None, TEXT("old Montage interruption preserves the next combo"));

	auto* Reaction = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Characters/Sa/Anm/MoveStop/ANM_Sa_Walk_F_End_Inplace_Montage.ANM_Sa_Walk_F_End_Inplace_Montage"));
	check(Reaction);
	Expect(Anim->Montage_Play(Reaction, 0.1f) > 0.f, TEXT("replacement Montage starts"));
	Tick(); Tick();
	UE_LOG(LogTemp, Display, TEXT("MeleeRepair: replacement check attacking=%d save=%d window=%d combo=%d movement=%d replacementPlaying=%d attackPlaying=%d reactionLength=%f"), GetBool(TEXT("IsAttacking")), GetBool(TEXT("SaveAttack")), GetBool(TEXT("IsInComboWindow")), Index->GetPropertyValue_InContainer(Actor), int32(Actor->GetCharacterMovement()->MovementMode), Anim->Montage_IsPlaying(Reaction), Anim->Montage_IsPlaying(Second), Reaction->GetPlayLength());
	Expect(Clean() && Actor->GetCharacterMovement()->MovementMode == MOVE_Walking && Anim->Montage_IsPlaying(Reaction), TEXT("attack clears while a non-attack Montage is still playing"));

	Start(0);
	Anim->Montage_Stop(0.1f);
	Run(TEXT("ResetCombo"));
	Actor->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Tick(); Tick();
	Expect(Clean() && Actor->GetCharacterMovement()->MovementMode == MOVE_Falling, TEXT("deferred callback does not overwrite an already reset action"));

	Start(0);
	SetBool(TEXT("IsDead?"), true);
	Anim->Montage_Stop(0.1f);
	Tick(); Tick();
	Expect(Clean() && Actor->GetCharacterMovement()->MovementMode == MOVE_None, TEXT("death clears attack flags without enabling walking"));

	UAnimMontage* Last = Start(0);
	for (int32 I = 0; Last && I < FMath::CeilToInt((Last->GetPlayLength() + 1.f) * 60.f); ++I) Tick();
	Expect(Clean() && Actor->GetCharacterMovement()->MovementMode == MOVE_Walking, TEXT("normal completion still resets the combo"));
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	UE_LOG(LogTemp, Display, TEXT("MeleeRepair: headless state tests failures=%d"), Failed);
	return Failed == 0;
}
}

UVTGRepairMeleeCommandlet::UVTGRepairMeleeCommandlet()
{
	IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}
int32 UVTGRepairMeleeCommandlet::Main(const FString& Params)
{
	auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa"));
	if (!BP) return 1;
	UEdGraph* Graph = nullptr;
	for (UEdGraph* G : BP->UbergraphPages) if (G->GetName() == TEXT("EventGraph")) Graph = G;
	if (!Graph) return 2;
	if (Params.Contains(TEXT("Inspect")))
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node->NodePosY < 7600 && Node->GetName() != TEXT("K2Node_CallFunction_316")) continue;
			UE_LOG(LogTemp, Display, TEXT("NODE %s %s (%d,%d)"), *Node->GetName(), *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), Node->NodePosX, Node->NodePosY);
			for (UEdGraphPin* P : Node->Pins)
			{
				FString Links;
				for (UEdGraphPin* L : P->LinkedTo) Links += L->GetOwningNode()->GetName() + TEXT(".") + L->PinName.ToString() + TEXT(" ");
				UE_LOG(LogTemp, Display, TEXT(" PIN %s default=%s links=%s"), *P->PinName.ToString(), *P->DefaultValue, *Links);
			}
		}
		return 0;
	}
	if (!Params.Contains(TEXT("VerifyOnly"))) Repair(BP, Graph);
	if (!Compile(BP)) return 3;
	if (Params.Contains(TEXT("Test")) && !TestRepair(BP)) return 5;
	if (Params.Contains(TEXT("VerifyOnly"))) return 0;
	const FString Filename = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(BP->GetOutermost(), BP, *Filename, SaveArgs)) return 4;
	UE_LOG(LogTemp, Display, TEXT("MeleeRepair: saved only %s"), *Filename);
	return 0;
}
