#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StructOnScope.h"
#include "Combat/VTGSmokeHazard.h"
#include "NiagaraComponent.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/PlayerInput.h"
#include "Tests/AutomationCommon.h"
#include "Combat/VTGCombatPresentation.h"
#include "Save/VTGSaveCoordinator.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/DataTable.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGSewerLiveInputTest,"Vertigo.Sewer.LiveInputProbe",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
namespace
{
class FRestoreProbeSaves : public IAutomationLatentCommand
{
 TMap<FString,TArray<uint8>> Files;
public:
 FRestoreProbeSaves(){TArray<FString> Names;IFileManager::Get().FindFiles(Names,*(FPaths::ProjectSavedDir()/TEXT("SaveGames/*.sav")),true,false);for(const auto& N:Names)check(FFileHelper::LoadFileToArray(Files.Add(N),*(FPaths::ProjectSavedDir()/TEXT("SaveGames")/N)));}
 bool Update() override
 {
  for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)return false;
  TArray<FString> Names;const FString Dir=FPaths::ProjectSavedDir()/TEXT("SaveGames");IFileManager::Get().FindFiles(Names,*(Dir/TEXT("*.sav")),true,false);
  for(const auto& N:Names)if(!Files.Contains(N))check(IFileManager::Get().Delete(*(Dir/N)));
  for(const auto& F:Files)check(FFileHelper::SaveArrayToFile(F.Value,*(Dir/F.Key)));
  return true;
 }
};
class FSewerLiveProbe : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;int Phase=0;double Deadline=0;TWeakObjectPtr<ACharacter> Sa;TWeakObjectPtr<AActor> Egg;
 static void Run(UObject* O,const TCHAR* N){auto* F=O->FindFunction(N);check(F);FStructOnScope Args(F);O->ProcessEvent(F,Args.GetStructMemory());}
 static bool Flag(UObject* O,const TCHAR* N){auto* P=FindFProperty<FBoolProperty>(O->GetClass(),N);return P && P->GetPropertyValue_InContainer(O);}
 void Report(UWorld* W,const TCHAR* Stage)
 {
  auto* P=Sa.Get();auto* Anim=P->GetMesh()->GetAnimInstance();auto* C=P->FindComponentByClass<UNarrativeComponent>();
  auto* Presentation=P->FindComponentByClass<UVTGPlayerPresentationComponent>();auto* Save=W->GetGameInstance()->GetSubsystem<UVTGSaveCoordinator>();
  UE_LOG(LogTemp,Display,TEXT("SEWER LIVE POLICY map=%s inCombat=%d storyLock=%d resolveSewer=%d defeated=%d pos=%s"),*UGameplayStatics::GetCurrentLevelName(W,true),Flag(P,TEXT("InCombat")),Presentation->IsStoryCombatLocked(),Presentation->ResolveCombatState(false,true),Save?Save->GetPersistentInt(TEXT("L3SewerBossDefeated"),0):-1,*P->GetActorLocation().ToString());
  UE_LOG(LogTemp,Display,TEXT("SEWER LIVE %s story=%d cine=%d canAttack=%d canPunch=%d attacking=%d dodge=%d gun=%d weapon=%d health=%d dialogue=%s montage=%s"),Stage,Flag(P,TEXT("StoryMode?")),Flag(P,TEXT("IsInCineCutscene_LockInput(NotWorkForMont)")),Flag(P,TEXT("CanAttack?")),Flag(P,TEXT("CanPunch?")),Flag(P,TEXT("IsAttacking")),Flag(P,TEXT("IsDodging?")),Flag(P,TEXT("HasWeaponInHand")),FindFProperty<FByteProperty>(P->GetClass(),TEXT("CurrentMeleeWeapon"))->GetPropertyValue_InContainer(P),FindFProperty<FIntProperty>(P->GetClass(),TEXT("Health"))->GetPropertyValue_InContainer(P),*GetNameSafe(C?C->GetCurrentDialogue():nullptr),*GetNameSafe(Anim?Anim->GetCurrentActiveMontage():nullptr));
 }
public:
 FSewerLiveProbe(FAutomationTestBase* T):Test(T){}
 bool Update() override
 {
  UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
  const double Now=FPlatformTime::Seconds();if(!Deadline)Deadline=Now+30;
  if(!W){if(Now<Deadline)return false;Test->AddError(TEXT("No PIE world"));return true;}
  // Niagara has no rendered view in NullRHI; advance solo instances as in the smoke lifetime regression.
  for(TActorIterator<AVTGSmokeHazard> It(W);It;++It)if(It->Cloud->GetForceSolo())It->Cloud->AdvanceSimulation(1,W->GetDeltaSeconds());
  if(Phase==0){Sa=Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(W,0));if(!Sa.IsValid())return false;Phase=1;Deadline=Now+5;return false;}
  if(Now<Deadline)return false;
  auto* P=Sa.Get();auto* PC=UGameplayStatics::GetPlayerController(W,0);if(!P || !PC){Test->AddError(TEXT("No player"));return true;}
  auto* NC=P->FindComponentByClass<UNarrativeComponent>();
  if(Phase==1)
  {
   Report(W,TEXT("before pickup"));AActor* Hammer=nullptr;for(TActorIterator<AActor> It(W);It;++It)if(It->GetClass()->GetName()==TEXT("BP_Hammer_C"))Hammer=*It;
   if(!Hammer){Test->AddError(TEXT("No hammer pickup"));return true;}
   Run(Hammer,TEXT("OnInteract_Event"));Phase=2;Deadline=Now+8;return false;
  }
  if(Phase==2)
  {
   if(NC && NC->IsInDialogue()){auto* D=NC->GetCurrentDialogue();if(D->CanSkipCurrentLine())D->SkipCurrentLine();Deadline=Now+.3;return false;}
   Report(W,TEXT("after dialogue"));Run(P,TEXT("CheckEnemyTimer"));Report(W,TEXT("after combat timer"));Phase=3;Deadline=Now+.2;
   if(W->GetGameViewport() && W->GetGameViewport()->Viewport)W->GetGameViewport()->InputKey(FInputKeyEventArgs(W->GetGameViewport()->Viewport,0,EKeys::LeftMouseButton,IE_Pressed));
   else PC->InputKey(FInputKeyParams(EKeys::LeftMouseButton,IE_Pressed,1.0,false));return false;
  }
  if(Phase==3)
  {
   Report(W,TEXT("after real left click"));auto* Anim=P->GetMesh()->GetAnimInstance();Test->TestTrue(TEXT("Real left click starts hammer montage after pickup dialogue"),Anim && Anim->GetCurrentActiveMontage() && Anim->GetCurrentActiveMontage()->GetName().StartsWith(TEXT("Hammer_")));
   PC->InputKey(FInputKeyParams(EKeys::LeftMouseButton,IE_Released,0.0,false));
   // Invoke the same input binding directly to distinguish UI interception from gameplay guards.
   if(!Anim || !Anim->GetCurrentActiveMontage()){Run(P,TEXT("Punch"));Report(W,TEXT("after direct Punch"));}
   if(Anim)Anim->Montage_Stop(0);Run(P,TEXT("ResetCombo"));
   UGameplayStatics::SetGlobalTimeDilation(W,1.f);P->CustomTimeDilation=1.f;
   AActor* Boss=nullptr;for(TActorIterator<AActor> It(W);It;++It)if(It->GetClass()->GetName().Contains(TEXT("BP_Boss_SeaMonster_L3Sewer")))Boss=*It;
   if(!Boss)return true;
   auto* Spawner=W->SpawnActor<AActor>(LoadClass<AActor>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_Ability_SpawnEggs.BP_Ability_SpawnEggs_C")));
   FindFProperty<FObjectPropertyBase>(Spawner->GetClass(),TEXT("BossRef"))->SetObjectPropertyValue_InContainer(Spawner,Boss);
   FindFProperty<FClassProperty>(Spawner->GetClass(),TEXT("SpawnClass"))->SetObjectPropertyValue_InContainer(Spawner,LoadClass<AActor>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg.BP_SeaMonster_Egg_C")));
   UVTGSewerAbilityHelpers::SpawnEggNearTarget(Spawner);for(TActorIterator<AActor> It(W);It;++It)if(It->ActorHasTag(TEXT("VTGBossSummonedEgg")))Egg=*It;
   if(auto* E=Cast<ACharacter>(Egg.Get()))E->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
   if(auto* E=Cast<ACharacter>(Egg.Get())){P->GetCharacterMovement()->DisableMovement();P->SetActorLocation(E->GetActorLocation()+FVector(100,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));}
   Phase=4;Deadline=Now+2;return false;
  }
  if(Phase==4)
  {
   if(!Egg.IsValid()){Test->AddError(TEXT("No summoned egg"));return true;}
   P->GetCharacterMovement()->DisableMovement();
   for(auto* C:Egg->GetComponents())if(auto* F=C->FindFunction(TEXT("ActivateAbility")))
   {
    for(const TCHAR* N:{TEXT("IsAlive"),TEXT("ControllerRef"),TEXT("CurrentAbility"),TEXT("Health")})if(auto* Prop=FindFProperty<FProperty>(C->GetClass(),N)){FString V;Prop->ExportText_InContainer(0,V,C,C,C,PPF_None);UE_LOG(LogTemp,Display,TEXT("SEWER LIVE EGG %s=%s"),N,*V);}
    FindFProperty<FObjectPropertyBase>(C->GetClass(),TEXT("TargetActor"))->SetObjectPropertyValue_InContainer(C,P);
    FStructOnScope Args(F);FindFProperty<FIntProperty>(F,TEXT("OptionalIndex"))->SetPropertyValue_InContainer(Args.GetStructMemory(),0);C->ProcessEvent(F,Args.GetStructMemory());
    if(!FindFProperty<FObjectPropertyBase>(C->GetClass(),TEXT("CurrentAbility"))->GetObjectPropertyValue_InContainer(C))
    {
     // Start the authored gas directly when AI perception has not selected a target in headless PIE.
     auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases"));FArrayProperty* AP=nullptr;for(TFieldIterator<FArrayProperty> It(Table->GetRowStruct());It;++It)if(It->GetName().StartsWith(TEXT("Abilities")))AP=*It;check(AP);
     FScriptArrayHelper Entries(AP,AP->ContainerPtrToValuePtr<void>(Table->FindRowUnchecked(TEXT("SeaMonsterEgg_P1"))));check(Entries.Num()==1);
     const FVector Origin=Egg->GetActorLocation()-FVector(0,0,CastChecked<ACharacter>(Egg.Get())->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
     auto* Area=W->SpawnActorDeferred<AActor>(LoadClass<AActor>(nullptr,TEXT("/Game/AI/BossAIToolkit/Blueprints/Abilities/Damage/BP_Ability_Area.BP_Ability_Area_C")),FTransform(Origin),Egg.Get());auto* Info=FindFProperty<FStructProperty>(Area->GetClass(),TEXT("AbilityInfo"));Info->CopyCompleteValue(Info->ContainerPtrToValuePtr<void>(Area),Entries.GetRawPtr(0));Area->FinishSpawning(FTransform(Origin));
     FindFProperty<FObjectPropertyBase>(C->GetClass(),TEXT("CurrentAbility"))->SetObjectPropertyValue_InContainer(C,Area);
    }
    auto* GasMontage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/Montages/AM_SeaMonsterEgg_ReleaseGas"));
    // NullRHI does not provide a reliable rendered AI animation update. Invoke its authored
    // notify once, then let the real cloud's normal world ticks handle all damage and lifetime.
    for(const auto& E:GasMontage->Notifies)if(auto* N=Cast<UVTGAttackEffectNotify>(E.Notify)){FAnimNotifyEventReference Ref;N->Notify(CastChecked<ACharacter>(Egg.Get())->GetMesh(),GasMontage,Ref);}
    for(TActorIterator<AVTGSmokeHazard> It(W);It;++It){It->Cloud->SetForceSolo(true);It->Cloud->SetAllowScalability(false);It->Cloud->Activate(true);UE_LOG(LogTemp,Display,TEXT("SEWER LIVE CLOUD CREATED %s complete=%d"),*It->GetName(),It->Cloud->IsComplete());}
    UE_LOG(LogTemp,Display,TEXT("SEWER LIVE: invoked authored Gas Burst notify explicitly for headless test; cloud ticks and damage remain normal"));
   }
   Report(W,TEXT("before actual egg attack"));Phase=5;Deadline=Now+1.5;return false;
  }
  if(Phase==5)
  {
   Report(W,TEXT("during actual egg gas"));for(TActorIterator<AVTGSmokeHazard> It(W);It;++It)UE_LOG(LogTemp,Display,TEXT("SEWER LIVE CLOUD %s pos=%s player=%s age=%.2f pulses=%d damageOpen=%d complete=%d"),*It->GetName(),*It->GetActorLocation().ToString(),*P->GetActorLocation().ToString(),It->Age,It->DamagePulses,It->IsDamageWindowOpen(),It->Cloud->IsComplete());Phase=6;Deadline=Now+2;return false;
  }
  Report(W,TEXT("after actual egg gas"));Test->TestTrue(TEXT("Actual summoned egg gas damages Sa in the live sewer"),FindFProperty<FIntProperty>(P->GetClass(),TEXT("Health"))->GetPropertyValue_InContainer(P)<100);return true;
 }
};
}
bool FVTGSewerLiveInputTest::RunTest(const FString&)
{
 AddExpectedError(TEXT("Ensure condition failed: SoftTexture.Get()"),EAutomationExpectedErrorFlags::Contains,0);
 auto Restore=MakeShared<FRestoreProbeSaves>();if(!AutomationOpenMap(TEXT("/Game/ThirdPerson/Maps/L_SewerUnderApartment")))return false;
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FSewerLiveProbe>(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());FAutomationTestFramework::Get().EnqueueLatentCommand(Restore);return true;
}
#endif
