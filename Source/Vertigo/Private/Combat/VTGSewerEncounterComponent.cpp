#include "Combat/VTGSewerEncounterComponent.h"
#include "UI/VTGBossHealthBarComponent.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "Combat/VTGCombatPresentation.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "VTGLevelManagerBase.h"
#include "Save/VTGSaveCoordinator.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"

namespace SewerEncounter
{
const FName Started(TEXT("L3_BossEncounterStarted")),Defeated(TEXT("L3_BossDefeated")),Count(TEXT("L3_MonsterEggKillCount")),Ids(TEXT("L3_CountedEggIds"));
bool Bool(UObject* O,FName N){auto* P=O?FindFProperty<FBoolProperty>(O->GetClass(),N):nullptr;return P && P->GetPropertyValue_InContainer(O);}
void Bool(UObject* O,FName N,bool V){if(auto* P=FindFProperty<FBoolProperty>(O->GetClass(),N))P->SetPropertyValue_InContainer(O,V);}
UObject* Object(UObject* O,FName N){auto* P=O?FindFProperty<FObjectPropertyBase>(O->GetClass(),N):nullptr;return P?P->GetObjectPropertyValue_InContainer(O):nullptr;}
double Health(AActor* A){if(A)for(auto* C:A->GetComponents())if(auto* P=FindFProperty<FNumericProperty>(C->GetClass(),TEXT("Health")))return P->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(C));return -1;}
void Combat(UActorComponent* C,bool Value)
{
 if(auto* F=C?C->FindFunction(TEXT("SetCombatState")):nullptr)
 {FStructOnScope Args(F);if(auto* P=FindFProperty<FBoolProperty>(F,TEXT("InCombat")))P->SetPropertyValue_InContainer(Args.GetStructMemory(),Value);C->ProcessEvent(F,Args.GetStructMemory());}
}
bool PlayerSource(AActor* A,APawn* Player)
{
 // Weapons/projectiles commonly supply their owning pawn instead of an event instigator.
 for(int32 Depth=0;A && Depth<8;++Depth){if(A==Player || A->GetInstigator()==Player)return true;A=A->GetOwner();}return false;
}
}
UVTGSewerEncounterComponent::UVTGSewerEncounterComponent(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=.1f;}
void UVTGSewerEncounterComponent::ResolveBoss()
{
 if(Behavior)return;
 Boss=Cast<AActor>(SewerEncounter::Object(GetOwner(),TEXT("L3_Boss")));
 if(!IsValid(Boss))return;
 for(auto* C:Boss->GetComponents())if(FindFProperty<FMulticastDelegateProperty>(C->GetClass(),TEXT("OnBossDeath")))
 {
  Behavior=C;FScriptDelegate D;D.BindUFunction(this,TEXT("HandleBossDefeated"));
  auto* P=FindFProperty<FMulticastDelegateProperty>(C->GetClass(),TEXT("OnBossDeath"));P->AddDelegate(D,C,P->ContainerPtrToValuePtr<void>(C));break;
 }
 if(!Behavior)return;
 if(auto* P=FindFProperty<FByteProperty>(Behavior->GetClass(),TEXT("CombatTrigger")))P->SetPropertyValue_InContainer(Behavior,2); // Toolkit: Take Damage; pre-encounter damage is disabled.
 Boss->SetCanBeDamaged(false);
 if(SewerEncounter::Bool(GetOwner(),SewerEncounter::Defeated))
 {Boss->SetActorHiddenInGame(true);Boss->SetActorEnableCollision(false);Behavior->SetComponentTickEnabled(false);}
}
void UVTGSewerEncounterComponent::BeginPlay()
{
 Super::BeginPlay();ResolveBoss();
 bEncounterRequested=SewerEncounter::Bool(GetOwner(),SewerEncounter::Started) && !SewerEncounter::Bool(GetOwner(),SewerEncounter::Defeated);
}
void UVTGSewerEncounterComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if(IntroNarrative.IsValid())IntroNarrative->OnDialogueFinished.RemoveDynamic(this,&UVTGSewerEncounterComponent::OnBossIntroFinished);
 if(IsValid(Behavior)){FScriptDelegate D;D.BindUFunction(this,TEXT("HandleBossDefeated"));if(auto* P=FindFProperty<FMulticastDelegateProperty>(Behavior->GetClass(),TEXT("OnBossDeath")))P->RemoveDelegate(D,Behavior,P->ContainerPtrToValuePtr<void>(Behavior));}
 if(auto* World=GetWorld())World->GetTimerManager().ClearTimer(BossBodyTimer);
 Super::EndPlay(Reason);
}
bool UVTGSewerEncounterComponent::StartBossEncounter(bool bWaitForIntroDialogue)
{
 ResolveBoss();if(!IsValid(Boss) || !Behavior || SewerEncounter::Bool(GetOwner(),SewerEncounter::Defeated) || SewerEncounter::Health(Boss)<=0)return false;
 if(bEncounterRequested || bCombatActivated)return true;
 if(bWaitForIntroDialogue)
 {
  if(auto* HUD=Boss->FindComponentByClass<UVTGBossHealthBarComponent>())HUD->SetPresentationAllowed(false);
  // An ambient comment still holding the scene would make the caller's Begin Dialogue fail and drop the authored intro line.
  if(auto* Narrative=OwnerNarrative();Narrative && Narrative->GetCurrentDialogue())Narrative->TryExitDialogue();
 }
 SewerEncounter::Bool(GetOwner(),SewerEncounter::Started,true);
 if(auto* Manager=Cast<AVTGLevelManagerBase>(GetOwner());Manager && Manager->FindFunction(TEXT("VTG_ResumeSewerBossFight")))
  if(auto* Player=UGameplayStatics::GetPlayerPawn(this,0))
   Manager->SaveEncounterCheckpoint(TEXT("SewerBossFightStart"),TEXT("VTG_ResumeSewerBossFight"),Player->GetActorTransform(),100);
 bEncounterRequested=true;SewerEncounter::Bool(GetOwner(),SewerEncounter::Started,true);
 return true;
}
UNarrativeComponent* UVTGSewerEncounterComponent::OwnerNarrative() const
{return Cast<UNarrativeComponent>(SewerEncounter::Object(GetOwner(),TEXT("Narrative")));}
void UVTGSewerEncounterComponent::WaitForBossIntroDialogue(UNarrativeComponent* Narrative,bool bDialogueStarted)
{
 if(IntroNarrative.IsValid())IntroNarrative->OnDialogueFinished.RemoveDynamic(this,&UVTGSewerEncounterComponent::OnBossIntroFinished);
 IntroNarrative=Narrative;IntroDialogue=bDialogueStarted && Narrative?Narrative->GetCurrentDialogue():nullptr;
 if(IntroDialogue.IsValid())
 {
  bIntroRetryPending=false;
  Narrative->OnDialogueFinished.AddUniqueDynamic(this,&UVTGSewerEncounterComponent::OnBossIntroFinished);
  return;
 }
 // The intro never started because a dialogue that refuses to exit still owns the scene: replay the beat once, after it ends.
 if(!bDialogueStarted && !bIntroRetryUsed && Narrative && Narrative->GetCurrentDialogue() && GetOwner()->FindFunction(TEXT("L3BossEncounter")))
 {
  bIntroRetryPending=true;bIntroRetryUsed=true;
  Narrative->OnDialogueFinished.AddUniqueDynamic(this,&UVTGSewerEncounterComponent::OnBossIntroFinished);
  return;
 }
 bIntroRetryPending=false;ReleaseBossHealthBar();
}
void UVTGSewerEncounterComponent::OnBossIntroFinished(UDialogue* Dialogue,bool bStartingNewDialogue)
{
 if(bIntroRetryPending)
 {
  // A replacement dialogue reports the previous one finished in the same frame while still owning the scene.
  if(bStartingNewDialogue)return;
  bIntroRetryPending=false;
  if(IntroNarrative.IsValid())IntroNarrative->OnDialogueFinished.RemoveDynamic(this,&UVTGSewerEncounterComponent::OnBossIntroFinished);
  // Begin Dialogue inside this callback would re-enter the component that is still finishing; wait one tick.
  if(auto* World=GetWorld())World->GetTimerManager().SetTimerForNextTick(this,&UVTGSewerEncounterComponent::RetryBossIntro);
  else RetryBossIntro();
  return;
 }
 if(Dialogue && Dialogue==IntroDialogue.Get())ReleaseBossHealthBar();
}
void UVTGSewerEncounterComponent::RetryBossIntro()
{
 if(SewerEncounter::Bool(GetOwner(),SewerEncounter::Defeated)){ReleaseBossHealthBar();return;}
 if(auto* Event=GetOwner()->FindFunction(TEXT("L3BossEncounter")))GetOwner()->ProcessEvent(Event,nullptr);
 else ReleaseBossHealthBar();
}
void UVTGSewerEncounterComponent::ReleaseBossHealthBar()
{
 if(IntroNarrative.IsValid())IntroNarrative->OnDialogueFinished.RemoveDynamic(this,&UVTGSewerEncounterComponent::OnBossIntroFinished);
 IntroNarrative.Reset();IntroDialogue.Reset();
 if(IsValid(Boss))if(auto* HUD=Boss->FindComponentByClass<UVTGBossHealthBarComponent>())HUD->SetPresentationAllowed(true);
}
void UVTGSewerEncounterComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick)
{
 Super::TickComponent(Dt,Type,Tick);ResolveBoss();
 if(auto* GI=GetWorld()->GetGameInstance())if(auto* Save=GI->GetSubsystem<UVTGSaveCoordinator>();Save && Save->HasPendingWorldRestore())return;
 if(!bEncounterRequested || bCombatActivated || !IsValid(Boss) || !Behavior || SewerEncounter::Bool(GetOwner(),SewerEncounter::Defeated))return;
 // The toolkit creates its controller/blackboard during BeginPlay; do not race that initialization.
 if(!IsValid(SewerEncounter::Object(Behavior,TEXT("ControllerRef"))))return;
 Boss->SetCanBeDamaged(true);SewerEncounter::Combat(Behavior,true);
 bCombatActivated=SewerEncounter::Bool(Behavior,TEXT("InCombat"));
}
void UVTGSewerEncounterComponent::HandleBossDefeated()
{
 if(SewerEncounter::Bool(GetOwner(),SewerEncounter::Defeated))return;
 SewerEncounter::Bool(GetOwner(),SewerEncounter::Defeated,true);bEncounterRequested=false;
 if(auto* Manager=Cast<AVTGLevelManagerBase>(GetOwner());Manager && Manager->FindFunction(TEXT("VTG_ResumeAfterSewerBossFight")))
  Manager->SetEncounterResumeEvent(TEXT("VTG_ResumeAfterSewerBossFight"));
 if(auto* Player=UGameplayStatics::GetPlayerPawn(this,0))if(auto* Presentation=Player->FindComponentByClass<UVTGPlayerPresentationComponent>())Presentation->FinishSewerCombat();
 // Let the death animation read, then reach the same cleared room a reload rebuilds, so the body never blocks the switch.
 if(auto* World=GetWorld())World->GetTimerManager().SetTimer(BossBodyTimer,this,&UVTGSewerEncounterComponent::HideBossBody,FMath::Max(.01f,BossBodyHideDelay),false);
 else HideBossBody();
 if(auto* Event=GetOwner()->FindFunction(TEXT("L3_AfterBossIsDefeated")))GetOwner()->ProcessEvent(Event,nullptr);
}
void UVTGSewerEncounterComponent::HideBossBody()
{
 if(IsValid(Boss)){Boss->SetCanBeDamaged(false);Boss->SetActorHiddenInGame(true);Boss->SetActorEnableCollision(false);}
 if(Behavior)Behavior->SetComponentTickEnabled(false);
}
void UVTGSewerEncounterComponent::RestoreEncounterCheckpointScene()
{
 auto Invoke=[this](FName Name){if(auto* F=GetOwner()->FindFunction(Name))GetOwner()->ProcessEvent(F,nullptr);};
 ResolveBoss();
 // The entrance barrier keeps whatever the level and L3_SaPosition_Entrance authored; this path no longer duplicates that location.
 Invoke(TEXT("L3_DisableAllISXColiInBossRoom"));
 if(!SewerEncounter::Bool(GetOwner(),SewerEncounter::Defeated))
 {
  if(auto* Manager=Cast<AVTGLevelManagerBase>(GetOwner()))Manager->SetPlayerStoryCombatLock(false);
  StartBossEncounter();return;
 }
 bEncounterRequested=false;
 if(IsValid(Boss)){Boss->SetCanBeDamaged(false);Boss->SetActorHiddenInGame(true);Boss->SetActorEnableCollision(false);}
 if(Behavior)Behavior->SetComponentTickEnabled(false);
 if(auto* Player=UGameplayStatics::GetPlayerPawn(this,0))if(auto* P=Player->FindComponentByClass<UVTGPlayerPresentationComponent>())P->FinishSewerCombat();
 const bool Opened=SewerEncounter::Bool(GetOwner(),TEXT("L3_CheckpointDoorOpened"));
 const bool Passed=SewerEncounter::Bool(GetOwner(),TEXT("L3_CheckpointPassedDoor"));
 if(!Opened)Invoke(TEXT("L3_BossRoom_EnableSwitchColi"));
 if(auto* Door=Cast<AActor>(SewerEncounter::Object(GetOwner(),TEXT("RoundDoor"))))
  if(auto* Mesh=Door->FindComponentByClass<USkeletalMeshComponent>())
   if(auto* Anim=LoadObject<UAnimSequence>(nullptr,Opened && !Passed?TEXT("/Game/EnvArt/Door/RoundVaultDoor1/RoundDoor_anim_open.RoundDoor_anim_open"):TEXT("/Game/EnvArt/Door/RoundVaultDoor1/Anm_RoundDoor_Closed.Anm_RoundDoor_Closed")))
   {Mesh->PlayAnimation(Anim,false);Mesh->SetPosition(Anim->GetPlayLength(),false);Mesh->Stop();Mesh->TickAnimation(0,false);Mesh->RefreshBoneTransforms();}
}
int32 UVTGSewerEncounterComponent::GetMonsterEggKillCount() const
{auto* P=FindFProperty<FIntProperty>(GetOwner()->GetClass(),SewerEncounter::Count);return P?P->GetPropertyValue_InContainer(GetOwner()):0;}
void UVTGSewerEncounterComponent::RecordPlayerEggKill(AActor* Egg)
{
 if(!Egg)return;
 auto* Counter=FindFProperty<FIntProperty>(GetOwner()->GetClass(),SewerEncounter::Count);
 auto* SavedIds=FindFProperty<FStrProperty>(GetOwner()->GetClass(),SewerEncounter::Ids);if(!Counter || !SavedIds)return;
 // Loaded map actors retain stable names across save/reload; spawned eggs get a unique per-spawn ID.
 const FString Id=Egg->HasAnyFlags(RF_WasLoaded)?Egg->GetFName().ToString():FGuid::NewGuid().ToString();
 FString List=SavedIds->GetPropertyValue_InContainer(GetOwner());const FString Token=TEXT("|")+Id+TEXT("|");if(List.Contains(Token))return;
 List+=Token;SavedIds->SetPropertyValue_InContainer(GetOwner(),List);
 const int32 NewCount=GetMonsterEggKillCount()+1;Counter->SetPropertyValue_InContainer(GetOwner(),NewCount);OnMonsterEggKillCountChanged.Broadcast(NewCount);
}
void UVTGEggKillReporterComponent::BeginPlay()
{
 Super::BeginPlay();bDeathObserved=SewerEncounter::Health(GetOwner())<=0;
 GetOwner()->OnTakeAnyDamage.AddUniqueDynamic(this,&UVTGEggKillReporterComponent::HandleDamage);
}
void UVTGEggKillReporterComponent::HandleDamage(AActor* DamagedActor,float Damage,const UDamageType* DamageType,AController* InstigatedBy,AActor* DamageCauser)
{
 if(bDeathObserved || Damage<=0 || SewerEncounter::Health(DamagedActor)>0)return;
 bDeathObserved=true;
 auto* Player=UGameplayStatics::GetPlayerPawn(this,0);if(!Player)return;
 if(!(InstigatedBy && InstigatedBy->GetPawn()==Player) && !SewerEncounter::PlayerSource(DamageCauser,Player))return;
 for(TActorIterator<AActor> It(GetWorld());It;++It)if(auto* Encounter=It->FindComponentByClass<UVTGSewerEncounterComponent>())Encounter->RecordPlayerEggKill(DamagedActor);
}
