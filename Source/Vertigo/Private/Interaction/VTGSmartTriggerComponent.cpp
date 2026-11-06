#include "Interaction/VTGSmartTriggerComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
bool Flag(const UObject* O,const TCHAR* Name,bool Default=false){auto* P=O?FindFProperty<FBoolProperty>(O->GetClass(),Name):nullptr;return P?P->GetPropertyValue_InContainer(O):Default;}
void SetFlag(UObject* O,const TCHAR* Name,bool V){if(auto* P=FindFProperty<FBoolProperty>(O->GetClass(),Name))P->SetPropertyValue_InContainer(O,V);}
template<class T>T* Field(UObject* O,const TCHAR* Name){auto* P=FindFProperty<FObjectPropertyBase>(O->GetClass(),Name);return P?Cast<T>(P->GetObjectPropertyValue_InContainer(O)):nullptr;}
}
UVTGSmartTriggerComponent::UVTGSmartTriggerComponent()
{
    PrimaryComponentTick.bCanEverTick=false;
    ProxyClass=TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Blueprints/BP_0ISXInteractiveCollision.BP_0ISXInteractiveCollision_C")));
}
UVTGSmartTriggerComponent* UVTGSmartTriggerComponent::Get(AActor* Trigger)
{
    if(!IsValid(Trigger))return nullptr;
    auto* State=Trigger->FindComponentByClass<UVTGSmartTriggerComponent>();
    if(!State){State=NewObject<UVTGSmartTriggerComponent>(Trigger);Trigger->AddInstanceComponent(State);State->RegisterComponent();}
    State->Volume=Field<UBoxComponent>(Trigger,TEXT("Box(ChangeSizeHere)"));
    if(State->Volume.IsValid())State->Volume->OnComponentEndOverlap.AddUniqueDynamic(State,&UVTGSmartTriggerComponent::VolumeExited);
    Trigger->OnDestroyed.AddUniqueDynamic(State,&UVTGSmartTriggerComponent::OwnerDestroyed);
    return State;
}
void UVTGSmartTriggerComponent::InitializeTrigger(AActor* Trigger)
{
    auto* State=Get(Trigger);if(!State || !State->Volume.IsValid())return;
    if(Flag(Trigger,TEXT("RequireInteraction")) && !IsValid(State->Proxy))
    {
        auto* Class=State->ProxyClass.LoadSynchronous();if(!Class)return;
        const FTransform ProxyTransform=State->Volume->GetComponentTransform();
        auto* ProxyActor=Trigger->GetWorld()->SpawnActorDeferred<AActor>(Class,ProxyTransform,Trigger,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(!ProxyActor)return;
        ProxyActor->SetFlags(RF_Transient);ProxyActor->Tags.Add(TEXT("VTG_SmartTriggerInteractionProxy"));
        SetFlag(ProxyActor,TEXT("CanInteract?"),false);SetFlag(ProxyActor,TEXT("Trigger BPLMEvent?"),false);SetFlag(ProxyActor,TEXT("DoOnce?"),false);SetFlag(ProxyActor,TEXT("Lock Cam on Interact?"),false);
        if(auto* Source=FindFProperty<FTextProperty>(Trigger->GetClass(),TEXT("InteractionPrompt")))
            if(auto* Dest=FindFProperty<FTextProperty>(Class,TEXT("InteractionTextOverride")))Dest->SetPropertyValue_InContainer(ProxyActor,Source->GetPropertyValue_InContainer(Trigger));
        UGameplayStatics::FinishSpawningActor(ProxyActor,ProxyTransform);State->Proxy=ProxyActor;
        if(auto* Interaction=Field<UActorComponent>(ProxyActor,TEXT("AC_ActorInteractionComponent")))
            for(const auto& Names:TArray<TPair<FName,FName>>{{TEXT("InteractionDetectionDistance"),TEXT("OverrideDetectionDistance")},{TEXT("InteractionConfirmDistance"),TEXT("OverridePickUpDistance")}})
                if(auto* Source=FindFProperty<FNumericProperty>(Trigger->GetClass(),Names.Key))
                    if(auto* Dest=FindFProperty<FNumericProperty>(Interaction->GetClass(),Names.Value))
                        Dest->SetFloatingPointPropertyValue(Dest->ContainerPtrToValuePtr<void>(Interaction),Source->GetFloatingPointPropertyValue(Source->ContainerPtrToValuePtr<void>(Trigger)));
        ProxyActor->AttachToComponent(State->Volume.Get(),FAttachmentTransformRules::KeepWorldTransform);
        if(auto* Box=Field<UBoxComponent>(ProxyActor,TEXT("CollisionBox(ChangeSizeHere)"))){Box->SetWorldTransform(State->Volume->GetComponentTransform());Box->SetBoxExtent(State->Volume->GetUnscaledBoxExtent());}
        auto* Event=FindFProperty<FMulticastDelegateProperty>(Class,TEXT("OnInteract"));check(Event);
        FScriptDelegate Callback;Callback.BindUFunction(State,TEXT("ProxyInteracted"));Event->AddDelegate(Callback,ProxyActor,Event->ContainerPtrToValuePtr<void>(ProxyActor));
        State->bPromptEnabled=true;State->SetPromptEnabled(false);
    }
    if(State->Proxy)Trigger->GetWorld()->GetTimerManager().SetTimer(State->RefreshTimer,State,&UVTGSmartTriggerComponent::Refresh,.1f,true);
}
bool UVTGSmartTriggerComponent::IsPlayerInside(AActor* Player) const
{
    if(!IsValid(Player) || !Volume.IsValid())return false;
    // Use physical position, not overlap end notifications: disabling/re-enabling collision
    // during dialogue must not count as leaving. Capsule expansion also handles component overlaps.
    FVector Extent(0);
    if(auto* Character=Cast<ACharacter>(Player))Extent=Character->GetCapsuleComponent()->Bounds.BoxExtent;
    else Extent=Player->GetComponentsBoundingBox(true).GetExtent();
    const auto Transform=Volume->GetComponentTransform();
    const FVector Local=Transform.InverseTransformPosition(Player->GetActorLocation());
    FVector Expanded=Volume->GetUnscaledBoxExtent();
    for(int32 I=0;I<3;++I){FVector Axis(0);Axis[I]=Extent[I];Expanded+=Transform.InverseTransformVector(Axis).GetAbs();}
    return FMath::Abs(Local.X)<=Expanded.X && FMath::Abs(Local.Y)<=Expanded.Y && FMath::Abs(Local.Z)<=Expanded.Z;
}
bool UVTGSmartTriggerComponent::CanAccept(AActor* Player) const
{
    if(!IsValid(Player) || !Player->ActorHasTag(TEXT("Player")) || !IsPlayerInside(Player) || !Volume.IsValid() || !Volume->IsQueryCollisionEnabled())return false;
    if(!Flag(GetOwner(),TEXT("TriggerBPLMEvent?")) || bDispatching || bExplicitBusy)return false;
    // One-shot story triggers retain their original overlap behavior even during
    // dialogue. Lifetime consumption already prevents duplicate dispatches;
    // visit/dialogue guards below are only for repeatable triggers.
    if(Flag(GetOwner(),TEXT("DoOnce?")))return !bEverFired;
    if(bSpent && Flag(GetOwner(),TEXT("RequireExitBeforeRetrigger"),true))return false;
    if(auto* Narrative=Player->FindComponentByClass<UNarrativeComponent>())if(Narrative->GetCurrentDialogue())return false;
    if(auto* Pawn=Cast<APawn>(Player))if(auto* Controller=Pawn->GetController())
    {
        if(auto* Narrative=Controller->FindComponentByClass<UNarrativeComponent>())if(Narrative->GetCurrentDialogue())return false;
        if(auto* State=Pawn->GetPlayerState())if(auto* Narrative=State->FindComponentByClass<UNarrativeComponent>())if(Narrative->GetCurrentDialogue())return false;
    }
    return true;
}
bool UVTGSmartTriggerComponent::AcceptTrigger(AActor* Trigger,AActor* Player,bool FromInteraction)
{
    auto* State=Get(Trigger);if(!State)return false;
    if(Flag(Trigger,TEXT("RequireInteraction"))!=FromInteraction)return false;
    // Covers a real leave/re-enter between monitor samples.
    if(State->Occupant.IsValid() && !State->IsPlayerInside(State->Occupant.Get()))State->bSpent=false;
    if(!State->CanAccept(Player))return false;
    State->Occupant=Player;State->bSpent=true;State->bEverFired=true;State->bDispatching=true;
    State->SetPromptEnabled(false);
    Trigger->GetWorld()->GetTimerManager().SetTimer(State->RefreshTimer,State,&UVTGSmartTriggerComponent::Refresh,.1f,true);
    return true;
}
void UVTGSmartTriggerComponent::FinishTriggerDispatch(AActor* Trigger){if(auto* State=Get(Trigger))State->bDispatching=false;}
void UVTGSmartTriggerComponent::SetTriggerBusy(AActor* Trigger,bool Busy){if(auto* State=Get(Trigger))State->bExplicitBusy=Busy;}
void UVTGSmartTriggerComponent::SetPromptEnabled(bool Enabled)
{
    if(!IsValid(Proxy))return;
    // The reused Blueprint also has a delayed startup refresh which can disable
    // these shapes. They remain detection targets; readiness lives on ISX alone.
    for(const TCHAR* Name:{TEXT("CollisionBox(ChangeSizeHere)"),TEXT("Cube(Don't Change)")})
        if(auto* Shape=Field<UPrimitiveComponent>(Proxy,Name))
            if(Shape->GetCollisionEnabled()!=ECollisionEnabled::QueryOnly)Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    if(bPromptEnabled==Enabled)return;
    bPromptEnabled=Enabled;SetFlag(Proxy,TEXT("CanInteract?"),Enabled);
    // Keep the target queryable. ISX owns selection/distance and the icon; changing
    // the proxy's collision here makes its candidate/visibility detection unstable.
    if(auto* Interaction=Field<UActorComponent>(Proxy,TEXT("AC_ActorInteractionComponent")))
        if(auto* Function=Interaction->FindFunction(TEXT("SetInteractionEnabled")))
        {struct {bool Enabled;} Params{Enabled};Interaction->ProcessEvent(Function,&Params);}
}
bool UVTGSmartTriggerComponent::IsManagedInteractionProxy(AActor* Actor)
{
    return IsValid(Actor) && Actor->ActorHasTag(TEXT("VTG_SmartTriggerInteractionProxy"));
}
bool UVTGSmartTriggerComponent::IsInteractionProxyVisible(AActor* Actor,AActor* Player,FVector CameraLocation)
{
    if(!IsManagedInteractionProxy(Actor) || !IsValid(Actor->GetOwner()) || !Flag(Actor,TEXT("CanInteract?")))return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VTGSmartInteractionVisibility),false,Player);
    Query.AddIgnoredActor(Actor->GetOwner());
    Query.AddIgnoredActor(Actor);
    FHitResult Hit;
    const bool Blocked=Actor->GetWorld()->LineTraceSingleByChannel(Hit,CameraLocation,Actor->GetActorLocation(),ECC_Visibility,Query);
    // A camera already inside the interaction box can legitimately produce no hit.
    return !Blocked;
}
void UVTGSmartTriggerComponent::VolumeExited(UPrimitiveComponent*,AActor* OtherActor,UPrimitiveComponent*,int32)
{
    if(Occupant==OtherActor && !IsPlayerInside(OtherActor)){bSpent=false;Occupant.Reset();}
}
void UVTGSmartTriggerComponent::Refresh()
{
    if(!Occupant.IsValid() || !IsPlayerInside(Occupant.Get())){bSpent=false;Occupant.Reset();}
    if(IsValid(Proxy))
    {
        auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
        UE_LOG(LogTemp,Verbose,TEXT("SmartTrigger %s player=%s inside=%d spent=%d dispatch=%d busy=%d once=%d allowed=%d"),*GetNameSafe(GetOwner()),*GetNameSafe(Player),IsPlayerInside(Player),bSpent,bDispatching,bExplicitBusy,Flag(GetOwner(),TEXT("DoOnce?")),CanAccept(Player));
        SetPromptEnabled(CanAccept(Player));
    }
    else if(!bSpent && !bDispatching)GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
}
void UVTGSmartTriggerComponent::ProxyInteracted()
{
    auto* Player=UGameplayStatics::GetPlayerPawn(this,0);
    if(!AcceptTrigger(GetOwner(),Player,true))return;
    if(auto* Event=GetOwner()->FindFunction(TEXT("VTGExecuteAcceptedTrigger")))GetOwner()->ProcessEvent(Event,nullptr);
    else bDispatching=false;
}
void UVTGSmartTriggerComponent::OwnerDestroyed(AActor*)
{
    if(auto* World=GetWorld())World->GetTimerManager().ClearTimer(RefreshTimer);
    if(IsValid(Proxy) && Proxy->GetWorld())Proxy->Destroy();Proxy=nullptr;
}
void UVTGSmartTriggerComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    OwnerDestroyed(GetOwner());
    Super::EndPlay(Reason);
}
