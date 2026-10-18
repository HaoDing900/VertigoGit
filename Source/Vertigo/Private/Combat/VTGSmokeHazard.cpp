#include "Combat/VTGSmokeHazard.h"
#include "Combat/VTGCombatPresentation.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

namespace Smoke
{
FProperty* Field(const UStruct* S,const FString& N){for(TFieldIterator<FProperty> It(S);It;++It)if(It->GetName()==N || It->GetName().StartsWith(N+TEXT("_")))return *It;return nullptr;}
void Number(UStruct* S,void* O,const FString& N,double V){if(auto* P=CastField<FNumericProperty>(Field(S,N))){auto* D=P->ContainerPtrToValuePtr<void>(O);if(P->IsInteger())P->SetIntPropertyValue(D,int64(V));else P->SetFloatingPointPropertyValue(D,V);}}
double Number(UObject* O,const FString& N,double Fallback){if(O)if(auto* P=CastField<FNumericProperty>(Field(O->GetClass(),N))){auto* D=P->ContainerPtrToValuePtr<void>(O);return P->IsInteger()?double(P->GetSignedIntPropertyValue(D)):P->GetFloatingPointPropertyValue(D);}return Fallback;}
void* Struct(UStruct* S,void* O,const FString& N,UScriptStruct*& Type){auto* P=CastFieldChecked<FStructProperty>(Field(S,N));Type=P->Struct;return P->ContainerPtrToValuePtr<void>(O);}
void* Damage(AActor* Proxy,UScriptStruct*& Type){auto* P=CastFieldChecked<FStructProperty>(Field(Proxy->GetClass(),TEXT("AbilityInfo")));return Struct(P->Struct,P->ContainerPtrToValuePtr<void>(Proxy),TEXT("Damage"),Type);}
}
AVTGSmokeHazard::AVTGSmokeHazard()
{
 PrimaryActorTick.bCanEverTick=true;RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));Cloud=CreateDefaultSubobject<UNiagaraComponent>(TEXT("Cloud"));Cloud->SetupAttachment(RootComponent);Cloud->SetAutoActivate(false);Cloud->SetAutoDestroy(false);
}
AVTGSmokeHazard* AVTGSmokeHazard::Spawn(AActor* Source,AActor* Ability,const UVTGAttackEffectNotify* S,UNiagaraSystem* System,const FVector& Position,float VisualScale)
{
 if(!Source || !Ability || !System)return nullptr;
 auto* World=Source->GetWorld();auto* H=World->SpawnActorDeferred<AVTGSmokeHazard>(StaticClass(),FTransform(Source->GetActorRotation(),Position),Source);
 H->DamageSource=Source;H->OriginalAbility=Ability;H->Lifetime=S->SmokeLifetime;H->DamageStartsAt=S->SmokeDamageDelay;H->NextPulse=H->DamageStartsAt;H->DamageStopsAt=FMath::Max(0.f,H->Lifetime-S->SmokeSafeFadeSeconds);H->Interval=FMath::Max(.1f,S->SmokeTickInterval);H->TickMultiplier=S->SmokeTickMultiplier;H->Radius=Smoke::Number(Ability,TEXT("Radius"),250);
 H->Cloud->SetAsset(System);H->Cloud->SetRelativeScale3D(FVector(VisualScale));H->Cloud->SetCustomTimeDilation(S->EffectPlayRate);H->FinishSpawning(FTransform(Source->GetActorRotation(),Position));H->Cloud->Activate(true);
 // A separate toolkit damage actor preserves its damage interfaces after the original animation/ability ends.
 if(Source->HasAuthority())
 {
  auto* Class=LoadClass<AActor>(nullptr,TEXT("/Game/AI/BossAIToolkit/Blueprints/Abilities/BP_BossAbility_Base.BP_BossAbility_Base_C"));
  if(Class)
  {
   H->DamageProxy=World->SpawnActorDeferred<AActor>(Class,FTransform(Position),Source,Cast<APawn>(Source),ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
   auto* To=FindFProperty<FStructProperty>(Class,TEXT("AbilityInfo"));auto* From=FindFProperty<FStructProperty>(Ability->GetClass(),TEXT("AbilityInfo"));check(To && From && To->Struct==From->Struct);
   To->CopyCompleteValue(To->ContainerPtrToValuePtr<void>(H->DamageProxy),From->ContainerPtrToValuePtr<void>(Ability));
   Smoke::Number(To->Struct,To->ContainerPtrToValuePtr<void>(H->DamageProxy),TEXT("Duration"),60);
   UScriptStruct* DT=nullptr;void* D=Smoke::Damage(H->DamageProxy,DT);
   auto* Mult=CastFieldChecked<FNumericProperty>(Smoke::Field(DT,TEXT("DamageMultiplier")));H->InitialMultiplier=Mult->GetFloatingPointPropertyValue(Mult->ContainerPtrToValuePtr<void>(D));
   Smoke::Number(DT,D,TEXT("Duration"),0);Smoke::Number(DT,D,TEXT("PerTickMultiplier"),0);
   CastFieldChecked<FClassProperty>(Smoke::Field(DT,TEXT("DamageType")))->SetObjectPropertyValue_InContainer(D,LoadClass<UDamageType>(nullptr,TEXT("/Game/AI/BossAIToolkit/Blueprints/DamageTypes/BP_DamageType_Default.BP_DamageType_Default_C")));
   H->DamageProxy->FinishSpawning(FTransform(Position));H->DamageProxy->SetActorHiddenInGame(true);
   // Also initialize explicitly for test worlds and deferred owner startup.
   for(const TCHAR* N:{TEXT("BossRef"),TEXT("BossBehaviorRef")})if(auto* P=FindFProperty<FObjectPropertyBase>(Class,N))if(auto* Q=FindFProperty<FObjectPropertyBase>(Ability->GetClass(),N))P->SetObjectPropertyValue_InContainer(H->DamageProxy,Q->GetObjectPropertyValue_InContainer(Ability));
  }
 }
 return H;
}
bool AVTGSmokeHazard::IsDamageWindowOpen() const{return Age>=DamageStartsAt && Age<DamageStopsAt && Cloud && !Cloud->IsComplete() && IsValid(DamageSource);}
void AVTGSmokeHazard::Tick(float Dt)
{
 Super::Tick(Dt);Age+=Dt;
 if(Age>=Lifetime || !Cloud || (Age>.15f && Cloud->IsComplete())){Destroy();return;}
 if(IsDamageWindowOpen() && Age>=NextPulse)
 {
  NextPulse=Age+Interval;
  bool Alive=true;if(IsValid(DamageSource))for(auto* C:DamageSource->GetComponents())if(Smoke::Field(C->GetClass(),TEXT("Health")))Alive=Smoke::Number(C,TEXT("Health"),0)>0;
  if(Alive)Pulse();
 }
}
void AVTGSmokeHazard::Pulse()
{
 if(!HasAuthority() || !IsValid(DamageProxy))return;
 if(IsValid(OriginalAbility))if(auto* F=OriginalAbility->FindFunction(TEXT("RemoveTelegraph")))OriginalAbility->ProcessEvent(F,nullptr);
 auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
 const bool First=bFirstPulse;++DamagePulses;
 if(!Player || FVector::Dist2D(Player->GetActorLocation(),GetActorLocation())>Radius || FMath::Abs(Player->GetActorLocation().Z-GetActorLocation().Z)>250)return;
 FHitResult Wall;FCollisionQueryParams Query(SCENE_QUERY_STAT(SmokeDamage),false,DamageSource);Query.AddIgnoredActor(Player);Query.AddIgnoredActor(DamageProxy);Query.AddIgnoredActor(this);if(IsValid(OriginalAbility))Query.AddIgnoredActor(OriginalAbility);
 // Creature capsules block Visibility; they must not shield the player from a gas cloud.
 FCollisionObjectQueryParams Obstacles;Obstacles.AddObjectTypesToQuery(ECC_WorldStatic);Obstacles.AddObjectTypesToQuery(ECC_WorldDynamic);
 if(GetWorld()->LineTraceSingleByObjectType(Wall,GetActorLocation()+FVector(0,0,50),Player->GetActorLocation(),Obstacles,Query))return;
 bFirstPulse=false;
 UScriptStruct* DT=nullptr;void* D=Smoke::Damage(DamageProxy,DT);Smoke::Number(DT,D,TEXT("DamageMultiplier"),InitialMultiplier*(First?1:TickMultiplier));
 if(auto* F=DamageProxy->FindFunction(TEXT("Apply Ability Damage")))
 {
  FStructOnScope Args(F);
  FindFProperty<FObjectPropertyBase>(F,TEXT("DamagedActor"))->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Player);
  FindFProperty<FBoolProperty>(F,TEXT("Radial"))->SetPropertyValue_InContainer(Args.GetStructMemory(),false);
  FindFProperty<FBoolProperty>(F,TEXT("AddToDamagedActors"))->SetPropertyValue_InContainer(Args.GetStructMemory(),false);
  *FindFProperty<FStructProperty>(F,TEXT("Origin"))->ContainerPtrToValuePtr<FVector>(Args.GetStructMemory())=(Player->GetActorLocation()-GetActorLocation()).GetSafeNormal();
  auto* Hit=FindFProperty<FStructProperty>(F,TEXT("HitResult"))->ContainerPtrToValuePtr<FHitResult>(Args.GetStructMemory());*Hit=FHitResult(Player,Cast<UPrimitiveComponent>(Player->GetRootComponent()),Player->GetActorLocation(),FVector::UpVector);
  DamageProxy->ProcessEvent(F,Args.GetStructMemory());
 }
}
void AVTGSmokeHazard::EndPlay(const EEndPlayReason::Type Reason){if(IsValid(DamageProxy))DamageProxy->Destroy();if(Cloud)Cloud->DeactivateImmediate();Super::EndPlay(Reason);}
void UVTGSewerAbilityHelpers::InitializeEggSpawner(AActor* Ability)
{
 if(!Ability)return;auto* Info=FindFProperty<FStructProperty>(Ability->GetClass(),TEXT("AbilityInfo"));check(Info);auto* Modifier=CastFieldChecked<FStructProperty>(Smoke::Field(Info->Struct,TEXT("Modifier")));auto* V=Modifier->ContainerPtrToValuePtr<FVector>(Info->ContainerPtrToValuePtr<void>(Ability));
 Smoke::Number(Ability->GetClass(),Ability,TEXT("SpawnCount"),FMath::Max(1,FMath::RoundToInt(V->X)));Smoke::Number(Ability->GetClass(),Ability,TEXT("SpawnRadius"),350);
 UScriptStruct* VT=nullptr;void* Visuals=Smoke::Struct(Info->Struct,Info->ContainerPtrToValuePtr<void>(Ability),TEXT("Visuals"),VT);
 if(auto* To=FindFProperty<FObjectPropertyBase>(Ability->GetClass(),TEXT("SpawnClass")))To->SetObjectPropertyValue_InContainer(Ability,CastFieldChecked<FObjectPropertyBase>(Smoke::Field(VT,TEXT("SpawnClass")))->GetObjectPropertyValue_InContainer(Visuals));
}
void UVTGSewerAbilityHelpers::SpawnEggWithoutNavigation(AActor* Ability)
{
 SpawnEggNearTarget(Ability);
}
void UVTGBossSummonedEggComponent::BindDeath()
{
 if(bDeathBindingsInstalled)return;bDeathBindingsInstalled=true;
 GetOwner()->OnTakeAnyDamage.AddUniqueDynamic(this,&ThisClass::HandleDamage);
 for(auto* C:GetOwner()->GetComponents())
  if(auto* P=FindFProperty<FMulticastDelegateProperty>(C->GetClass(),TEXT("OnBossDeath")))
  {FScriptDelegate D;D.BindUFunction(this,TEXT("HandleEggDeath"));P->AddDelegate(D,C,P->ContainerPtrToValuePtr<void>(C));break;}
 if(auto* Boss=GetOwner()->GetOwner())for(auto* C:Boss->GetComponents())
  if(auto* P=FindFProperty<FMulticastDelegateProperty>(C->GetClass(),TEXT("OnBossDeath")))
  {SummonerBehavior=C;FScriptDelegate D;D.BindUFunction(this,TEXT("HandleSummonerDeath"));P->AddDelegate(D,C,P->ContainerPtrToValuePtr<void>(C));break;}
}
void UVTGBossSummonedEggComponent::HandleDamage(AActor* DamagedActor,float Damage,const UDamageType*,AController* InstigatedBy,AActor* DamageCauser)
{
 if(Damage<=0 || bKilledByPlayer)return;
 bool Dead=false;for(auto* C:DamagedActor->GetComponents())if(Smoke::Field(C->GetClass(),TEXT("Health")))Dead=Smoke::Number(C,TEXT("Health"),1)<=0;
 if(!Dead)return; // An earlier nonlethal player hit must not turn expiry into a player kill.
 bool PlayerSource=InstigatedBy && InstigatedBy->IsPlayerController();
 for(int32 Depth=0;DamageCauser && Depth<8;++Depth,DamageCauser=DamageCauser->GetOwner())
 {
  if(auto* Pawn=Cast<APawn>(DamageCauser))PlayerSource|=Pawn->IsPlayerControlled();
  if(auto* Controller=DamageCauser->GetInstigatorController())PlayerSource|=Controller->IsPlayerController();
 }
 bKilledByPlayer=PlayerSource;
}
void UVTGBossSummonedEggComponent::HandleEggDeath()
{
 if(bEggDeathObserved)return;bEggDeathObserved=true;
 // Toolkit death is broadcast inside Point/RadialDamage, before OnTakeAnyDamage supplies attribution.
 // Defer only the cleanup decision so a lethal player hit can finish the normal death pipeline.
 if(GetWorld())GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this]()
 {if(!bKilledByPlayer)DespawnSummonedEgg();}));
}
void UVTGBossSummonedEggComponent::HandleSummonerDeath()
{
 // Keep corpses from completed player kills; only living summons die with their summoner.
 if(!bEggDeathObserved)DespawnSummonedEgg();
}
void UVTGBossSummonedEggComponent::DespawnSummonedEgg()
{
 if(auto* Owner=GetOwner())Owner->Destroy();
}
void UVTGBossSummonedEggComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if(auto* C=SummonerBehavior.Get())if(auto* P=FindFProperty<FMulticastDelegateProperty>(C->GetClass(),TEXT("OnBossDeath")))
 {FScriptDelegate D;D.BindUFunction(this,TEXT("HandleSummonerDeath"));P->RemoveDelegate(D,C,P->ContainerPtrToValuePtr<void>(C));}
 if(GetOwner())GetOwner()->OnTakeAnyDamage.RemoveDynamic(this,&ThisClass::HandleDamage);
 Super::EndPlay(Reason);
}
void UVTGSewerAbilityHelpers::SpawnEggNearTarget(AActor* Ability)
{
 if(!Ability)return;auto* BossProperty=FindFProperty<FObjectPropertyBase>(Ability->GetClass(),TEXT("BossRef"));auto* ClassProperty=FindFProperty<FClassProperty>(Ability->GetClass(),TEXT("SpawnClass"));
 auto* Boss=BossProperty?Cast<AActor>(BossProperty->GetObjectPropertyValue_InContainer(Ability)):nullptr;auto* Class=ClassProperty?Cast<UClass>(ClassProperty->GetObjectPropertyValue_InContainer(Ability)):nullptr;if(!Boss || !Class)return;
 auto* Player=UGameplayStatics::GetPlayerPawn(Ability,0);
 const FVector Center=Player?Player->GetActorLocation():Boss->GetActorLocation()+Boss->GetActorForwardVector()*300;
 const float Radius=FMath::Max(220.f,float(Smoke::Number(Ability,TEXT("SpawnRadius"),350)));
 FVector Location=Center;bool Found=false;
 auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(Boss->GetWorld());
 FCollisionQueryParams Query(SCENE_QUERY_STAT(EggSpawnFloor),false,Boss);Query.AddIgnoredActor(Ability);if(Player)Query.AddIgnoredActor(Player);
 // A loose ring leaves room to dodge and avoids spawning directly on the player.
 for(int32 Attempt=0;Attempt<8 && !Found;++Attempt)
 {
  const float Angle=FMath::FRandRange(0.f,2*PI),Distance=FMath::FRandRange(200.f,Radius);
  FVector Candidate=Center+FVector(FMath::Cos(Angle)*Distance,FMath::Sin(Angle)*Distance,0);
  FNavLocation Projected;if(Nav && Nav->ProjectPointToNavigation(Candidate,Projected,FVector(100,100,350)))Candidate=Projected.Location;
  FHitResult Ground;
  if(Boss->GetWorld()->LineTraceSingleByChannel(Ground,Candidate+FVector(0,0,150),Candidate-FVector(0,0,700),ECC_Visibility,Query) && Ground.ImpactNormal.Z>.5f)
  {Location=Ground.ImpactPoint;Found=true;}
 }
 if(!Found)return;
 FActorSpawnParameters Params;Params.Owner=Boss;Params.Instigator=Cast<APawn>(Boss);Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
 float Height=100;if(auto* Character=Cast<ACharacter>(Class->GetDefaultObject()))Height=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3;
 if(auto* Egg=Boss->GetWorld()->SpawnActor<AActor>(Class,Location+FVector(0,0,Height),Boss->GetActorRotation(),Params))
 {
  // The toolkit construction script may replace Owner with its (not yet ready) controller.
  Egg->SetOwner(Boss);Egg->SetInstigator(Cast<APawn>(Boss));
  Egg->Tags.AddUnique(TEXT("VTGBossSummonedEgg"));
  auto* Cleanup=NewObject<UVTGBossSummonedEggComponent>(Egg);Egg->AddInstanceComponent(Cleanup);Cleanup->RegisterComponent();Cleanup->BindDeath();
 }
}

