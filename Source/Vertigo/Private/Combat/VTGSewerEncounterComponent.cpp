#include "Combat/VTGSewerEncounterComponent.h"
#include "Combat/VTGCombatPresentation.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

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
 if(IsValid(Behavior)){FScriptDelegate D;D.BindUFunction(this,TEXT("HandleBossDefeated"));if(auto* P=FindFProperty<FMulticastDelegateProperty>(Behavior->GetClass(),TEXT("OnBossDeath")))P->RemoveDelegate(D,Behavior,P->ContainerPtrToValuePtr<void>(Behavior));}
 Super::EndPlay(Reason);
}
bool UVTGSewerEncounterComponent::StartBossEncounter()
{
 ResolveBoss();if(!IsValid(Boss) || !Behavior || SewerEncounter::Bool(GetOwner(),SewerEncounter::Defeated) || SewerEncounter::Health(Boss)<=0)return false;
 bEncounterRequested=true;SewerEncounter::Bool(GetOwner(),SewerEncounter::Started,true);
 return true;
}
void UVTGSewerEncounterComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Tick)
{
 Super::TickComponent(Dt,Type,Tick);ResolveBoss();
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
 if(auto* Player=UGameplayStatics::GetPlayerPawn(this,0))if(auto* Presentation=Player->FindComponentByClass<UVTGPlayerPresentationComponent>())Presentation->FinishSewerCombat();
 if(auto* Event=GetOwner()->FindFunction(TEXT("L3_AfterBossIsDefeated")))GetOwner()->ProcessEvent(Event,nullptr);
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
