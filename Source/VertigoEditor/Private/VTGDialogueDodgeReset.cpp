#include "Audio/VTGPlayerAudioComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LevelSequenceActor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_ComponentBoundEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
bool RunDialogueDodgeCase(UBlueprint* BP,FName Callback,bool StopSequences,int32 SequenceCount,bool ExpectReset)
{
 TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Sa=W->SpawnActor<ACharacter>(BP->GeneratedClass,FTransform::Identity,Spawn);check(Sa);
 auto* Dodge=FindFProperty<FBoolProperty>(Sa->GetClass(),TEXT("IsDodging?"));auto* Stop=FindFProperty<FBoolProperty>(Sa->GetClass(),TEXT("EndSequenceWhenNDialogueFinishes?"));check(Dodge && Stop);
 Stop->SetPropertyValue_InContainer(Sa,StopSequences);Dodge->SetPropertyValue_InContainer(Sa,false);
 Sa->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Sa->GetCharacterMovement()->Velocity=FVector(200,0,0);
 auto* Audio=Sa->FindComponentByClass<UVTGPlayerAudioComponent>();check(Audio && Audio->CanPlayFootstep());
 TArray<TWeakObjectPtr<ALevelSequenceActor>> Actors;
 for(int I=0;I<SequenceCount;++I)Actors.Add(W->SpawnActor<ALevelSequenceActor>());
 auto* Function=Sa->FindFunction(Callback);check(Function);FStructOnScope Values(Function);Sa->ProcessEvent(Function,Values.GetStructMemory());
 check(Dodge->GetPropertyValue_InContainer(Sa) && !Audio->CanPlayFootstep());
 W->GetLatentActionManager().BeginFrame();W->GetLatentActionManager().ProcessLatentActions(Sa,.9f);
 check(Dodge->GetPropertyValue_InContainer(Sa)); // Preserve the authored one-second suppression window.
 W->GetLatentActionManager().BeginFrame();W->GetLatentActionManager().ProcessLatentActions(Sa,.2f);
 const bool Reset=!Dodge->GetPropertyValue_InContainer(Sa);
 check(Reset==ExpectReset && Audio->CanPlayFootstep()==ExpectReset);
 for(const auto& Actor:Actors)check(StopSequences?!Actor.IsValid():Actor.IsValid());
 UE_LOG(LogTemp,Display,TEXT("DIALOGUE_DODGE_RESET PASS stop=%d sequenceCount=%d reset=%d footstepAllowed=%d"),StopSequences,SequenceCount,Reset,Audio->CanPlayFootstep());
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return Reset;
}
}

int32 VTGRepairDialogueDodgeReset(const FString& Params)
{
 const bool Verify=FParse::Param(*Params,TEXT("Verify"));
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(BP);
 auto* G=FBlueprintEditorUtils::FindEventGraph(BP);check(G);
 auto Find=[&](const TCHAR* Name){for(UEdGraphNode* N:G->Nodes)if(N->GetFName()==Name)return N;checkNoEntry();return (UEdGraphNode*)nullptr;};
 auto P=[](UEdGraphNode* N,const TCHAR* Name){auto* Pin=N->FindPin(Name);check(Pin);return Pin;};
 auto* Loop=Find(TEXT("K2Node_MacroInstance_0"));auto* Destroy=Find(TEXT("K2Node_CallFunction_239"));auto* Merge=Find(TEXT("K2Node_Knot_82"));
 auto* Event=CastChecked<UK2Node_ComponentBoundEvent>(Find(TEXT("K2Node_ComponentBoundEvent_3")));
 check(Event->DelegatePropertyName==TEXT("OnNPCDialogueLineFinished") || Event->DelegatePropertyName==TEXT("NPCDialogueLineFinished"));
 const FName Callback=Event->CustomFunctionName;
 const bool Fixed=P(Loop,TEXT("Completed"))->LinkedTo.Contains(P(Merge,TEXT("InputPin")));
 if(!Fixed)
 {
  check(!Verify);
  // Reproduce the reported blocker on the original saved Blueprint before editing it.
  RunDialogueDodgeCase(BP,Callback,true,0,false);
  const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
  const FString Backup=FPaths::ProjectDir()/TEXT("Scripts/DialogueDodgeReset/Backup/BP_Player_Sa.uasset");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
  if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File,false)==COPY_OK);
  check(P(Destroy,TEXT("then"))->LinkedTo.Contains(P(Merge,TEXT("InputPin"))));
  P(Destroy,TEXT("then"))->BreakLinkTo(P(Merge,TEXT("InputPin")));
  check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(P(Loop,TEXT("Completed")),P(Merge,TEXT("InputPin"))));
  Loop->NodeComment=TEXT("Reset the dialogue's temporary dodge suppression once AFTER enumeration, including zero Sequence actors. Keep the false branch and one-second delay.");Loop->bCommentBubbleVisible=true;
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Results);check(Results.NumErrors==0);
  FSavePackageArgs Save;Save.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,Save));
 }
 check(P(Loop,TEXT("Completed"))->LinkedTo.Contains(P(Merge,TEXT("InputPin"))) && P(Destroy,TEXT("then"))->LinkedTo.IsEmpty());
 for(bool Stop:{false,true})for(int Count:{0,1,3})RunDialogueDodgeCase(BP,Callback,Stop,Count,true);
 UE_LOG(LogTemp,Display,TEXT("DIALOGUE_DODGE_RESET_VERIFIED original failure reproduced; saved graph and six real callback/latency/footstep-gate cases passed"));
 return 0;
}
