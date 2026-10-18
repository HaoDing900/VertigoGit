#include "VTGSewerGasLifetimeCommandlet.h"
#include "Combat/VTGCombatPresentation.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RenderingThread.h"
#include "UObject/StructOnScope.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"

struct FVTGSewerFollowupRegression
{
 static void Run(bool SummonOnly=false)
 {
  TGuardValue<bool> Execution(GAllowActorScriptExecutionInEditor,true);TGuardValue<bool> Client(GIsClient,true);
  auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
  auto* W=UWorld::CreateWorld(EWorldType::Game,false,TEXT("SewerFollowupTests"),nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
  {TGuardValue<bool> Rendering(PRIVATE_GIsRunningCommandlet,false);W->CreateFXSystem();}FlushRenderingCommands();W->InitializeActorsForPlay(FURL());
  auto Obj=[](UObject* O,const TCHAR* N,UObject* V){auto* P=FindFProperty<FObjectPropertyBase>(O->GetClass(),N);check(P);P->SetObjectPropertyValue_InContainer(O,V);};
  auto Num=[](UObject* O,const TCHAR* N,double V){auto* P=FindFProperty<FNumericProperty>(O->GetClass(),N);check(P);auto* A=P->ContainerPtrToValuePtr<void>(O);if(P->IsInteger())P->SetIntPropertyValue(A,int64(V));else P->SetFloatingPointPropertyValue(A,V);};
  auto Flag=[](UObject* O,const TCHAR* N,bool V){auto* P=FindFProperty<FBoolProperty>(O->GetClass(),N);check(P);P->SetPropertyValue_InContainer(O,V);};
  FActorSpawnParameters SP;SP.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Floor=W->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);Floor->SetRootComponent(Box);Box->SetBoxExtent(FVector(3000,3000,50));Box->SetCollisionProfileName(TEXT("BlockAll"));Box->RegisterComponent();Floor->SetActorLocation(FVector(0,0,-50));
  auto* Player=W->SpawnActor<AVTGSmokeDamageTestPawn>(AVTGSmokeDamageTestPawn::StaticClass(),FTransform(FVector(1000,0,100)),SP);auto* PC=W->SpawnActor<APlayerController>();PC->Possess(Player);
  auto* Boss=W->SpawnActor<ACharacter>(ACharacter::StaticClass(),FTransform(FVector(0,0,100)),SP);
  auto* Behavior=NewObject<UActorComponent>(Boss,LoadObject<UBlueprint>(nullptr,TEXT("/Game/AI/BossAIToolkit/Blueprints/BPC_Boss_Behavior"))->GeneratedClass);Boss->AddInstanceComponent(Behavior);Num(Behavior,TEXT("AttackPower"),10);Obj(Behavior,TEXT("ControllerRef"),PC);Obj(Behavior,TEXT("CharacterRef"),Boss);
  auto* SpawnBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_Ability_SpawnEggs"));auto* Ability=W->SpawnActor<AActor>(SpawnBP->GeneratedClass,FTransform::Identity,SP);
  Obj(Ability,TEXT("BossRef"),Boss);Obj(Ability,TEXT("SpawnClass"),LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg"))->GeneratedClass);Num(Ability,TEXT("SpawnRadius"),350);
  for(int I=0;I<3;++I)UVTGSewerAbilityHelpers::SpawnEggNearTarget(Ability);
  TArray<AActor*> Eggs;for(TActorIterator<AActor> It(W);It;++It)if(It->ActorHasTag(TEXT("VTGBossSummonedEgg")))Eggs.Add(*It);check(Eggs.Num()==3);
  for(auto* Egg:Eggs)
  {
   check(FVector::Dist2D(Egg->GetActorLocation(),Player->GetActorLocation())<550);check(Egg->GetOwner()==Boss);
  }
  auto EggBehavior=[](AActor* Egg){for(auto* C:Egg->GetComponents())if(FindFProperty<FMulticastDelegateProperty>(C->GetClass(),TEXT("OnBossDeath")))return C;return static_cast<UActorComponent*>(nullptr);};
  auto Death=[](UActorComponent* B){check(B);auto* P=FindFProperty<FMulticastDelegateProperty>(B->GetClass(),TEXT("OnBossDeath"));P->ContainerPtrToValuePtr<FMulticastScriptDelegate>(B)->ProcessMulticastDelegate<UObject>(nullptr);};
  auto Flush=[&](){++GFrameCounter;W->GetTimerManager().Tick(.016f);++GFrameCounter;W->GetTimerManager().Tick(.016f);};
  // Mirror engine ordering: Blueprint death first, damage attribution afterwards.
  Num(EggBehavior(Eggs[0]),TEXT("Health"),0);Death(EggBehavior(Eggs[0]));Eggs[0]->OnTakeAnyDamage.Broadcast(Eggs[0],60,nullptr,PC,Player);
  Num(EggBehavior(Eggs[1]),TEXT("Health"),0);Death(EggBehavior(Eggs[1]));
  Num(EggBehavior(Eggs[2]),TEXT("Health"),10);Eggs[2]->OnTakeAnyDamage.Broadcast(Eggs[2],50,nullptr,PC,Player);
  Num(EggBehavior(Eggs[2]),TEXT("Health"),0);Death(EggBehavior(Eggs[2]));Flush();
  check(!Eggs[0]->IsActorBeingDestroyed());check(Eggs[1]->IsActorBeingDestroyed() && Eggs[2]->IsActorBeingDestroyed());
  UE_LOG(LogTemp,Display,TEXT("PASS lethal player hit keeps normal death; automatic death destroys; prior nonlethal player hit does not preserve automatic death"));
  Player->SetActorLocation(FVector(1000,1000,100));for(int I=0;I<3;++I)UVTGSewerAbilityHelpers::SpawnEggNearTarget(Ability);
  TArray<AActor*> More;for(TActorIterator<AActor> It(W);It;++It)if(It->ActorHasTag(TEXT("VTGBossSummonedEgg")) && !Eggs.Contains(*It))More.Add(*It);check(More.Num()==3);
  auto* Weapon=W->SpawnActor<AActor>();Weapon->SetOwner(Player);
  Num(EggBehavior(More[0]),TEXT("Health"),0);Death(EggBehavior(More[0]));More[0]->OnTakeAnyDamage.Broadcast(More[0],60,nullptr,nullptr,Weapon);Flush();check(!More[0]->IsActorBeingDestroyed());
  More[1]->FindComponentByClass<UVTGBossSummonedEggComponent>()->DespawnSummonedEgg();check(More[1]->IsActorBeingDestroyed());
  Death(Behavior);check(More[2]->IsActorBeingDestroyed());check(!Eggs[0]->IsActorBeingDestroyed() && !More[0]->IsActorBeingDestroyed());
  UE_LOG(LogTemp,Display,TEXT("PASS player-owned weapon kill keeps corpse; explicit despawn destroys; boss death destroys live summons and preserves completed player kills"));
  auto* Placed=W->SpawnActor<AActor>(LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg"))->GeneratedClass,FTransform(FVector(-1500,0,100)),SP);check(!Placed->FindComponentByClass<UVTGBossSummonedEggComponent>());
  UE_LOG(LogTemp,Display,TEXT("PASS near-player spawn and placed egg death behavior unchanged"));
  if(SummonOnly){GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return;}

  Player->SetActorLocation(FVector(200,0,100));
  auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases"));
  auto Field=[](const UStruct* S,const FString& Name)->FProperty*{for(TFieldIterator<FProperty> It(S);It;++It)if(It->GetName()==Name || It->GetName().StartsWith(Name+TEXT("_")))return *It;checkNoEntry();return nullptr;};
  auto* AP=CastFieldChecked<FArrayProperty>(Field(Table->GetRowStruct(),TEXT("Abilities")));auto* AT=CastFieldChecked<FStructProperty>(AP->Inner);FScriptArrayHelper Entries(AP,AP->ContainerPtrToValuePtr<void>(Table->FindRowUnchecked(TEXT("SeaMonster_P1"))));void* Gas=nullptr;
  for(int I=0;I<Entries.Num();++I)if(CastFieldChecked<FNameProperty>(Field(AT->Struct,TEXT("AbilityName")))->GetPropertyValue_InContainer(Entries.GetRawPtr(I))==TEXT("Toxic Gas"))Gas=Entries.GetRawPtr(I);check(Gas);
  auto* Area=W->SpawnActor<AActor>(LoadObject<UBlueprint>(nullptr,TEXT("/Game/AI/BossAIToolkit/Blueprints/Abilities/Damage/BP_Ability_Area"))->GeneratedClass,FTransform(FVector(0,0,100)),SP);
  auto* Info=FindFProperty<FStructProperty>(Area->GetClass(),TEXT("AbilityInfo"));Info->CopyCompleteValue(Info->ContainerPtrToValuePtr<void>(Area),Gas);Obj(Area,TEXT("BossRef"),Boss);Obj(Area,TEXT("BossBehaviorRef"),Behavior);Num(Area,TEXT("Radius"),645.368112);
  auto* Settings=NewObject<UVTGAttackEffectNotify>();Settings->SmokeLifetime=3;Settings->SmokeDamageDelay=.5;Settings->SmokeSafeFadeSeconds=.6;Settings->SmokeTickMultiplier=.25;
  auto* System=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Characters/Monster/SeaMonsterBoss/FX/NS_Boss_ToxicGas_Lifetime"));check(System);
  auto* H=AVTGSmokeHazard::Spawn(Boss,Area,Settings,System,FVector(0,0,100),1);check(H);
  auto* Creature=W->SpawnActor<ACharacter>(ACharacter::StaticClass(),FTransform(FVector(100,0,100)),SP);Creature->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
  H->Pulse();check(FMath::IsNearlyEqual(Player->ReceivedDamage,4.f));H->Pulse();check(FMath::IsNearlyEqual(Player->ReceivedDamage,5.f));
  auto* Wall=W->SpawnActor<AActor>();auto* WallBox=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(WallBox);WallBox->SetBoxExtent(FVector(5,100,150));WallBox->SetCollisionProfileName(TEXT("BlockAll"));WallBox->RegisterComponent();Wall->SetActorLocation(FVector(150,0,150));H->Pulse();check(FMath::IsNearlyEqual(Player->ReceivedDamage,5.f));Wall->Destroy();
  // An initial miss must not consume the stronger first hit before the player enters the cloud.
  H->bFirstPulse=true;Player->SetActorLocation(FVector(1000,0,100));H->Pulse();check(H->bFirstPulse);Player->SetActorLocation(FVector(200,0,100));H->Pulse();check(FMath::IsNearlyEqual(Player->ReceivedDamage,9.f));
  H->Age=2.4f;check(!H->IsDamageWindowOpen());
  UE_LOG(LogTemp,Display,TEXT("PASS gas: first=4 subsequent=1, creatures do not shield player, walls still block, late entry gets first hit, safe fade stops damage"));

  auto* Real=W->SpawnActor<ACharacter>(LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"))->GeneratedClass,FTransform(FVector(-1000,0,100)),SP);
  Flag(Real,TEXT("StoryMode?"),false);Flag(Real,TEXT("IsDodging?"),false);Num(Real,TEXT("Health"),100);
  auto* Damage=Real->FindFunction(TEXT("ReceivePointDamage"));check(Damage);FStructOnScope Args(Damage);FindFProperty<FFloatProperty>(Damage,TEXT("Damage"))->SetPropertyValue_InContainer(Args.GetStructMemory(),4);
  auto* Type=LoadClass<UDamageType>(nullptr,TEXT("/Game/AI/BossAIToolkit/Blueprints/DamageTypes/BP_DamageType_Default.BP_DamageType_Default_C"));FindFProperty<FObjectPropertyBase>(Damage,TEXT("DamageType"))->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Type->GetDefaultObject());FindFProperty<FObjectPropertyBase>(Damage,TEXT("DamageCauser"))->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Area);Real->ProcessEvent(Damage,Args.GetStructMemory());
  auto* Health=FindFProperty<FIntProperty>(Real->GetClass(),TEXT("Health"));UE_LOG(LogTemp,Display,TEXT("SA actual health after 4 damage=%d"),Health?Health->GetPropertyValue_InContainer(Real):-1);check(Health && Health->GetPropertyValue_InContainer(Real)==96);
  auto* Hearts=NewObject<UVTGPlayerHearts>();Hearts->Health=96;Hearts->MaxHealth=100;check(FMath::IsNearlyEqual(Hearts->GetHeartFill(7),.68f,1.e-4f));Hearts->Health=50;for(int I=0;I<8;++I)check(Hearts->GetHeartFill(I)==(I<4?1.f:0.f));Hearts->Health=400;Hearts->MaxHealth=800;check(Hearts->GetHeartFill(3)==1 && Hearts->GetHeartFill(4)==0);
  UE_LOG(LogTemp,Display,TEXT("PASS real Sa receives 4 damage: 100 -> 96; eighth heart=68 percent; 50 percent total health is exactly four hearts at any max health"));
  PC->Possess(Real);
  auto* ManagerBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt"));auto* Manager=W->SpawnActor<AActor>(ManagerBP->GeneratedClass,FTransform::Identity,SP);
  auto* SwitchActor=W->SpawnActor<ASkeletalMeshActor>();Obj(Manager,TEXT("RoundDoorSwitch"),SwitchActor);SwitchActor->GetSkeletalMeshComponent()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
  UFunction* Incoming=nullptr;for(TFieldIterator<UFunction> It(Manager->GetClass());It;++It){FString Name=It->GetName().Replace(TEXT(" "),TEXT(""));if(Name.Equals(TEXT("ReceiveSmartTriggerCall"),ESearchCase::IgnoreCase)){Incoming=*It;break;}}check(Incoming);
  FStructOnScope Trigger(Incoming);bool HasId=false;for(TFieldIterator<FNameProperty> It(Incoming);It;++It)if(It->HasAnyPropertyFlags(CPF_Parm) && !It->HasAnyPropertyFlags(CPF_ReturnParm)){It->SetPropertyValue_InContainer(Trigger.GetStructMemory(),TEXT("L3_OpenRoundDoor"));HasId=true;}check(HasId);
  Manager->ProcessEvent(Incoming,Trigger.GetStructMemory());check(SwitchActor->GetSkeletalMeshComponent()->GetAnimationMode()==EAnimationMode::AnimationSingleNode);
  UE_LOG(LogTemp,Display,TEXT("PASS actual BPLM trigger L3_OpenRoundDoor reaches switch animation instead of falling through unmatched ID"));
  GEngine->DestroyWorldContext(W);W->DestroyWorld(false);
 }
};
