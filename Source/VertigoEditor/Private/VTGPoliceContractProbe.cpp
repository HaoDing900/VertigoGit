#include "VTGNoteInputTestFixture.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NarrativeComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"

int32 VTGPoliceContractProbe(const FString& Params)
{
 TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
 auto* Source=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L3PoliceStation/BPLM_PoliceStation"));check(Source);
 const bool Apply=FParse::Param(*Params,TEXT("ApplyContract"));
 const bool Verify=FParse::Param(*Params,TEXT("VerifyContract"));
 if(Apply || Verify)
 {
  auto* Graph=FBlueprintEditorUtils::FindEventGraph(Source);check(Graph);
  auto FindSource=[&](const TCHAR* Name){for(UEdGraphNode* Node:Graph->Nodes)if(Node->GetFName()==Name)return Node;return (UEdGraphNode*)nullptr;};
  auto* Open=FindSource(TEXT("K2Node_CallFunction_21"));auto* Delay=FindSource(TEXT("K2Node_CallFunction_25"));check(Open && Delay);
  auto* Getter=Cast<UK2Node_VariableGet>(FindSource(TEXT("VTG_ContractCachedAsset")));
  auto* Force=Cast<UK2Node_VariableSet>(FindSource(TEXT("VTG_ContractReapplyForceRead")));
  auto* Close=Cast<UK2Node_VariableSet>(FindSource(TEXT("VTG_ContractResetCloseAllowed")));
  auto* Schema=GetDefault<UEdGraphSchema_K2>();
  if(Apply && !Getter)
  {
   const auto File=FPackageName::LongPackageNameToFilename(Source->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
   const auto Backup=FPaths::ProjectDir()/TEXT("Scripts/PoliceContractAudio/Backup/BPLM_PoliceStation.uasset");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);check(!IFileManager::Get().FileExists(*Backup));check(IFileManager::Get().Copy(*Backup,*File,false)==COPY_OK);
   Getter=NewObject<UK2Node_VariableGet>(Graph,TEXT("VTG_ContractCachedAsset"));Graph->AddNode(Getter,false,false);Getter->CreateNewGuid();Getter->VariableReference.SetSelfMember(TEXT("PendingNoteAsset_0"));Getter->AllocateDefaultPins();Getter->NodePosX=Open->NodePosX-300;Getter->NodePosY=Open->NodePosY+250;
   Open->FindPin(TEXT("Note Data Asset"))->BreakAllPinLinks();check(Schema->TryCreateConnection(Getter->FindPin(TEXT("PendingNoteAsset_0")),Open->FindPin(TEXT("Note Data Asset"))));
   auto* Ref=CastChecked<UK2Node_VariableSet>(FindSource(TEXT("K2Node_VariableSet_5")));
   Force=NewObject<UK2Node_VariableSet>(Graph,TEXT("VTG_ContractReapplyForceRead"));Graph->AddNode(Force,false,false);Force->CreateNewGuid();Force->VariableReference=Ref->VariableReference;Force->AllocateDefaultPins();Schema->TrySetDefaultValue(*Force->FindPin(TEXT("ForceReadAllPages")),TEXT("true"));
   for(auto* P:Ref->FindPin(TEXT("self"))->LinkedTo)check(Schema->TryCreateConnection(P,Force->FindPin(TEXT("self"))));
   auto* CloseRef=CastChecked<UK2Node_VariableSet>(FindSource(TEXT("K2Node_VariableSet_6")));
   Close=NewObject<UK2Node_VariableSet>(Graph,TEXT("VTG_ContractResetCloseAllowed"));Graph->AddNode(Close,false,false);Close->CreateNewGuid();Close->VariableReference=CloseRef->VariableReference;Close->AllocateDefaultPins();Schema->TrySetDefaultValue(*Close->FindPin(TEXT("Note Close Allowed")),TEXT("false"));
   for(auto* P:Ref->FindPin(TEXT("self"))->LinkedTo)check(Schema->TryCreateConnection(P,Close->FindPin(TEXT("self"))));
   Open->FindPin(TEXT("execute"))->BreakAllPinLinks();
   check(Schema->TryCreateConnection(Delay->FindPin(TEXT("then")),Force->FindPin(TEXT("execute"))));check(Schema->TryCreateConnection(Force->FindPin(TEXT("then")),Close->FindPin(TEXT("execute"))));check(Schema->TryCreateConnection(Close->FindPin(TEXT("then")),Open->FindPin(TEXT("execute"))));
   Force->NodePosX=Open->NodePosX-520;Force->NodePosY=Open->NodePosY;Close->NodePosX=Open->NodePosX-260;Close->NodePosY=Open->NodePosY;
   Getter->NodeComment=TEXT("Use the retained contract asset for both initial opening and retries, independent of an earlier event's parameter frame.");Getter->bCommentBubbleVisible=true;
   FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Source);FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(Source,EBlueprintCompileOptions::SkipSave,&Results);check(Results.NumErrors==0);
   FSavePackageArgs Save;Save.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(Source->GetOutermost(),Source,*File,Save));
  }
  check(Getter && Force && Close);check(Open->FindPin(TEXT("Note Data Asset"))->LinkedTo.Contains(Getter->FindPin(TEXT("PendingNoteAsset_0"))));
  for(const TCHAR* Path:{TEXT("/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/Bello/ANM_Bello_Loco_Walk_Fwd_NoRM_Mirror"),TEXT("/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/ANM_Corvos_Walk_Fwd")})
  {
   auto* A=LoadObject<UAnimSequence>(nullptr,Path);check(A);int Count=0;const bool Bello=A->GetName().Contains(TEXT("Bello"));
   for(const auto& E:A->Notifies)if(auto* N=Cast<UAnimNotify_PlaySound>(E.Notify))
    if(N->Sound && N->Sound->GetName()==(Bello?TEXT("Footstep_Boots_06"):TEXT("footstep_concrete_walk_04")))
    {check(FMath::IsNearlyEqual(E.GetTime(),Bello?.36f:.26f,.001f) || FMath::IsNearlyEqual(E.GetTime(),Bello?1.11f:.90f,.001f));++Count;}
   check(Count==2);UE_LOG(LogTemp,Display,TEXT("POLICE_AUDIO_VERIFIED %s count=%d"),*A->GetName(),Count);
  }
  UE_LOG(LogTemp,Display,TEXT("CONTRACT_ASSET_VERIFIED cached asset, force-read reset; original teleport transform and dialogue callbacks preserved"));
 }
 auto* BP=DuplicateObject<UBlueprint>(Source,GetTransientPackage(),TEXT("ContractProbeBP"));check(BP);
 auto* G=FBlueprintEditorUtils::FindEventGraph(BP);check(G);
 auto Find=[&](const TCHAR* Name){for(UEdGraphNode* Node:G->Nodes)if(Node->GetFName()==Name)return Node;checkNoEntry();return (UEdGraphNode*)nullptr;};
 auto Pin=[](UEdGraphNode* Node,const TCHAR* Name){auto* P=Node->FindPin(Name);check(P);return P;};
 auto* Schema=GetDefault<UEdGraphSchema_K2>();
 auto Link=[&](UEdGraphPin* A,UEdGraphPin* B){check(Schema->TryCreateConnection(A,B));};
 auto Call=[&](UClass* Class,const TCHAR* Name){auto* Node=NewObject<UK2Node_CallFunction>(G);G->AddNode(Node,false,false);Node->CreateNewGuid();Node->SetFromFunction(Class->FindFunctionByName(Name));Node->AllocateDefaultPins();return Node;};
 // Test the real chain through its Delay; replace only the final UI RPC with a validity marker.
 auto* Open=Find(TEXT("K2Node_CallFunction_21"));auto* Marker=Call(AActor::StaticClass(),TEXT("SetActorTickEnabled"));auto* Valid=Call(UKismetSystemLibrary::StaticClass(),TEXT("IsValid"));
 for(auto* P:TArray<UEdGraphPin*>(Pin(Open,TEXT("execute"))->LinkedTo))Link(P,Pin(Marker,TEXT("execute")));
 for(auto* P:Pin(Open,TEXT("Note Data Asset"))->LinkedTo)Link(P,Pin(Valid,TEXT("Object")));
 Link(Pin(Valid,TEXT("ReturnValue")),Pin(Marker,TEXT("bEnabled")));
 FBlueprintEditorUtils::RemoveNode(BP,Open,true);
 // Prime the existing event parameter without running unrelated screen/story effects.
 Pin(Find(TEXT("K2Node_CustomEvent_9")),TEXT("then"))->BreakAllPinLinks();
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);check(Log.NumErrors==0);
 auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("ContractProbeWorld"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* SaClass=LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));check(SaClass);
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Sa=W->SpawnActor<ACharacter>(SaClass,FTransform::Identity,Spawn);auto* PC=W->SpawnActor<AVTGPlayerController>();PC->Possess(Sa);W->AddController(PC);
 auto* Manager=W->SpawnActor<AActor>(BP->GeneratedClass,FTransform::Identity,Spawn);check(Manager);Manager->PrimaryActorTick.bCanEverTick=true;
 auto Set=[&](const TCHAR* Name,UObject* O){auto* P=FindFProperty<FObjectPropertyBase>(Manager->GetClass(),Name);check(P);P->SetObjectPropertyValue_InContainer(Manager,O);};
 Set(TEXT("Player Pawn"),Sa);Set(TEXT("Narrative"),Sa->FindComponentByClass<UNarrativeComponent>());
 for(UActorComponent* C:Sa->GetComponents())if(C->GetClass()->GetName()==TEXT("AC_NotesComponent_C"))Set(TEXT("NoteComp_0"),C);
 auto* Note=LoadObject<UObject>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L3PoliceStation/Note_L3_Contract"));check(Note);Set(TEXT("PendingNoteAsset_0"),Note);
 auto Prime=[&](UObject* Value){auto* F=Manager->FindFunction(TEXT("L3_Contract"));check(F);FStructOnScope Values(F);auto* P=FindFProperty<FObjectPropertyBase>(F,TEXT("NoteAsset"));check(P);P->SetObjectPropertyValue_InContainer(Values.GetStructMemory(),Value);Manager->ProcessEvent(F,Values.GetStructMemory());};
 auto Run=[&](const TCHAR* Label){W->GetLatentActionManager().BeginFrame();Manager->SetActorTickEnabled(false);Manager->ProcessEvent(Manager->FindFunction(TEXT("L3_ReopenContractIfNotFinished")),nullptr);W->GetLatentActionManager().ProcessLatentActions(Manager,.5f);UE_LOG(LogTemp,Display,TEXT("CONTRACT_PROBE %s reached_open_with_valid_asset=%d"),Label,Manager->IsActorTickEnabled());};
 Prime(Note);Run(TEXT("Valid pawn and original event parameter"));check(Manager->IsActorTickEnabled());
 if(!Apply && !Verify){Set(TEXT("Player Pawn"),nullptr);Run(TEXT("Missing cached pawn"));Set(TEXT("Player Pawn"),Sa);}
 Prime(nullptr);Run(TEXT("Changed event parameter, cached asset still valid"));if(Apply || Verify)check(Manager->IsActorTickEnabled());
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return 0;
}
