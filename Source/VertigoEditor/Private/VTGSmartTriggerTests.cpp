#include "Interaction/VTGSmartTriggerComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/LatentActionManager.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "Editor.h"
#include "UObject/UnrealType.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Camera/CameraComponent.h"
#include "UObject/StructOnScope.h"

bool VTGTestSmartTriggers()
{
    if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    FEditorScriptExecutionGuard Guard;
    int32 Count=0;auto Expect=[&](bool OK,const TCHAR* Message){checkf(OK,TEXT("SMART TEST FAIL: %s"),Message);++Count;};
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);auto* GI=NewObject<UGameInstance>();World->SetGameInstance(GI);Context.OwningGameInstance=GI;
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player=World->SpawnActor<ACharacter>(ACharacter::StaticClass(),FTransform::Identity,Spawn);Player->Tags.Add(TEXT("Player"));Player->GetCapsuleComponent()->UpdateBounds();
    Player->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Controller=World->SpawnActor<APlayerController>();
    World->AddController(Controller);
    auto* LocalPlayer=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(LocalPlayer,0);Controller->SetPlayer(LocalPlayer);Controller->Possess(Player);
    auto Set=[&](AActor* Actor,const TCHAR* Name,bool V){auto* P=FindFProperty<FBoolProperty>(Actor->GetClass(),Name);check(P);P->SetPropertyValue_InContainer(Actor,V);};
    auto GetBox=[&](AActor* Actor){auto* P=FindFProperty<FObjectPropertyBase>(Actor->GetClass(),TEXT("Box(ChangeSizeHere)"));return CastChecked<UBoxComponent>(P->GetObjectPropertyValue_InContainer(Actor));};
    auto Accept=[&](AActor* Actor,bool Interaction=false){return UVTGSmartTriggerComponent::AcceptTrigger(Actor,Player,Interaction);};
    for(const TCHAR* Path:{TEXT("/Game/Blueprints/BPA_0SmartTrigger.BPA_0SmartTrigger_C"),TEXT("/Game/Blueprints/BPA_SmartRestrict.BPA_SmartRestrict_C")})
    {
        auto* Class=LoadClass<AActor>(nullptr,Path);check(Class);auto* Actor=World->SpawnActor<AActor>(Class,FTransform::Identity,Spawn);check(Actor);
        Set(Actor,TEXT("DoOnce?"),false);UVTGSmartTriggerComponent::InitializeTrigger(Actor);auto* State=Actor->FindComponentByClass<UVTGSmartTriggerComponent>();check(State);
        Player->SetActorLocation(GetBox(Actor)->GetComponentLocation(),false,nullptr,ETeleportType::TeleportPhysics);
        Expect(Accept(Actor),TEXT("default auto trigger accepts first visit"));
        UVTGSmartTriggerComponent::FinishTriggerDispatch(Actor);
        Expect(!Accept(Actor),TEXT("duplicate overlap rejected while player remains inside"));
        GetBox(Actor)->SetCollisionEnabled(ECollisionEnabled::NoCollision);State->Refresh();GetBox(Actor)->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Expect(!Accept(Actor),TEXT("collision off/on does not rearm occupied volume"));
        Player->SetActorLocation(FVector(5000,0,0),false,nullptr,ETeleportType::TeleportPhysics);State->Refresh();
        Player->SetActorLocation(GetBox(Actor)->GetComponentLocation(),false,nullptr,ETeleportType::TeleportPhysics);
        Expect(Accept(Actor),TEXT("physical exit and re-entry allow next use"));UVTGSmartTriggerComponent::FinishTriggerDispatch(Actor);
        Set(Actor,TEXT("RequireExitBeforeRetrigger"),false);Expect(Accept(Actor),TEXT("opt-out permits existing repeated entry behavior"));UVTGSmartTriggerComponent::FinishTriggerDispatch(Actor);
        UVTGSmartTriggerComponent::SetTriggerBusy(Actor,true);Expect(!Accept(Actor),TEXT("explicit asynchronous busy lock blocks retrigger"));UVTGSmartTriggerComponent::SetTriggerBusy(Actor,false);
        auto* Narrative=NewObject<UNarrativeComponent>(Player);Player->AddInstanceComponent(Narrative);Narrative->RegisterComponent();Narrative->CurrentDialogue=NewObject<UDialogue>(Narrative);
        Expect(!Accept(Actor),TEXT("active dialogue blocks repeatable activation"));
        auto* OneShot=World->SpawnActor<AActor>(Class,FTransform::Identity,Spawn);
        Set(OneShot,TEXT("DoOnce?"),true);
        UVTGSmartTriggerComponent::InitializeTrigger(OneShot);
        Expect(Accept(OneShot),TEXT("fresh one-shot accepts during active dialogue"));
        Expect(!Accept(OneShot),TEXT("one-shot rejects reentrant dispatch"));
        UVTGSmartTriggerComponent::FinishTriggerDispatch(OneShot);
        Expect(!Accept(OneShot),TEXT("one-shot stays consumed during active dialogue"));
        Narrative->CurrentDialogue=nullptr;
        Player->SetActorLocation(FVector(5000,0,0),false,nullptr,ETeleportType::TeleportPhysics);
        OneShot->FindComponentByClass<UVTGSmartTriggerComponent>()->Refresh();
        Player->SetActorLocation(GetBox(OneShot)->GetComponentLocation(),false,nullptr,ETeleportType::TeleportPhysics);
        Expect(!Accept(OneShot),TEXT("one-shot stays consumed after dialogue ends and player re-enters"));
        OneShot->Destroy();Narrative->DestroyComponent();
        Set(Actor,TEXT("DoOnce?"),true);Expect(!Accept(Actor),TEXT("existing lifetime one-shot restriction still wins"));
        Actor->Destroy();
    }
    auto* RestrictClass=LoadClass<AActor>(nullptr,TEXT("/Game/Blueprints/BPA_SmartRestrict.BPA_SmartRestrict_C"));
    auto* Restrict=World->SpawnActor<AActor>(RestrictClass,FTransform::Identity,Spawn);Set(Restrict,TEXT("RequireInteraction"),true);Set(Restrict,TEXT("DoOnce?"),false);
    Player->SetActorLocation(GetBox(Restrict)->GetComponentLocation(),false,nullptr,ETeleportType::TeleportPhysics);
    UVTGSmartTriggerComponent::InitializeTrigger(Restrict);auto* State=Restrict->FindComponentByClass<UVTGSmartTriggerComponent>();
    Restrict->DispatchBeginPlay();
    Expect(!Accept(Restrict),TEXT("interaction mode never triggers merely from overlap"));
    AActor* Proxy=nullptr;for(TActorIterator<AActor> It(World);It;++It)if(It->GetOwner()==Restrict && It->ActorHasTag(TEXT("VTG_SmartTriggerInteractionProxy")))Proxy=*It;
    Expect(Proxy!=nullptr,TEXT("interaction mode creates existing ISX prompt actor"));
    auto* RootBox=GetBox(Restrict);auto* P=FindFProperty<FObjectPropertyBase>(Proxy->GetClass(),TEXT("CollisionBox(ChangeSizeHere)"));auto* ProxyBox=CastChecked<UBoxComponent>(P->GetObjectPropertyValue_InContainer(Proxy));
    Expect(ProxyBox->GetComponentTransform().Equals(RootBox->GetComponentTransform()) && ProxyBox->GetUnscaledBoxExtent().Equals(RootBox->GetUnscaledBoxExtent()),TEXT("interaction collision matches authored trigger bounds"));
    Expect(Accept(Restrict,true),TEXT("confirmed interaction accepts once"));UVTGSmartTriggerComponent::FinishTriggerDispatch(Restrict);
    Expect(!Accept(Restrict,true),TEXT("repeated confirm remains locked until exit"));
    Player->SetActorLocation(FVector(5000,0,0),false,nullptr,ETeleportType::TeleportPhysics);State->Refresh();Player->SetActorLocation(RootBox->GetComponentLocation(),false,nullptr,ETeleportType::TeleportPhysics);
    Expect(Accept(Restrict,true),TEXT("interaction rearmed by real exit"));UVTGSmartTriggerComponent::FinishTriggerDispatch(Restrict);
    Player->SetActorLocation(FVector(5000,0,0),false,nullptr,ETeleportType::TeleportPhysics);State->Refresh();Player->SetActorLocation(RootBox->GetComponentLocation(),false,nullptr,ETeleportType::TeleportPhysics);State->Refresh();
    Expect(FindFProperty<FBoolProperty>(Proxy->GetClass(),TEXT("CanInteract?"))->GetPropertyValue_InContainer(Proxy),TEXT("existing ISX prompt enabled on eligible visit"));
    const FVector CameraLocation=Proxy->GetActorLocation()+FVector(100,0,0);
    auto* DetectionClass=LoadClass<UActorComponent>(nullptr,TEXT("/InventorySystemX/InventorySystemX/Components/AC_PlayerInteractionComponent.AC_PlayerInteractionComponent_C"));check(DetectionClass);
    auto* Detection=NewObject<UActorComponent>(Player,DetectionClass);
    auto* Camera=NewObject<UCameraComponent>(Player);Camera->RegisterComponent();Camera->SetWorldLocation(CameraLocation);
    FindFProperty<FObjectPropertyBase>(DetectionClass,TEXT("PlayerCameraComponent"))->SetObjectPropertyValue_InContainer(Detection,Camera);
    FindFProperty<FObjectPropertyBase>(DetectionClass,TEXT("Player Pawn"))->SetObjectPropertyValue_InContainer(Detection,Player);
    auto Visible=[&](){auto* Function=Detection->FindFunctionChecked(TEXT("IsVisibleActor"));FStructOnScope Params(Function);FindFProperty<FObjectPropertyBase>(Function,TEXT("Actor"))->SetObjectPropertyValue_InContainer(Params.GetStructMemory(),Proxy);Detection->ProcessEvent(Function,Params.GetStructMemory());return FindFProperty<FBoolProperty>(Function,TEXT("Return"))->GetPropertyValue_InContainer(Params.GetStructMemory());};
    Expect(Visible(),TEXT("real ISX visibility accepts camera inside proxy collision"));
    ProxyBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);State->Refresh();
    Expect(ProxyBox->IsQueryCollisionEnabled() && Visible(),TEXT("delayed ISX startup cannot disable the manual detection target"));
    auto Blocker=[&](AActor* Owner){auto* Box=NewObject<UBoxComponent>(Owner);Owner->AddInstanceComponent(Box);Box->SetBoxExtent(FVector(8,100,100));Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Ignore);Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Box->RegisterComponent();Box->SetWorldLocation(Proxy->GetActorLocation()+FVector(50,0,0));return Box;};
    auto* OwnWall=Blocker(Restrict);Expect(Visible(),TEXT("own restrict wall does not hide interaction icon"));
    auto* Obstacle=World->SpawnActor<AActor>();auto* ExternalWall=Blocker(Obstacle);Expect(!Visible(),TEXT("unrelated wall still hides interaction icon"));ExternalWall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    for(int32 I=0;I<30;++I){State->Refresh();Expect(Visible(),TEXT("stationary eligible interaction remains visible across refreshes"));}
    OwnWall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Interact=FindFProperty<FMulticastDelegateProperty>(Proxy->GetClass(),TEXT("OnInteract"));
    Interact->GetMulticastDelegate(Interact->ContainerPtrToValuePtr<void>(Proxy))->ProcessMulticastDelegate<UObject>(nullptr);
    Expect(!Accept(Restrict,true),TEXT("real ISX dispatcher reaches shared trigger guard"));
    Expect(!FindFProperty<FBoolProperty>(Proxy->GetClass(),TEXT("CanInteract?"))->GetPropertyValue_InContainer(Proxy),TEXT("confirmation disables existing ISX prompt"));
    for(int32 I=0;I<12;++I){++GFrameCounter;World->GetLatentActionManager().BeginFrame();World->GetLatentActionManager().ProcessLatentActions(Restrict,1.f/60.f);}
    Set(Restrict,TEXT("RequireExitBeforeRetrigger"),false);
    Expect(Accept(Restrict,true),TEXT("original BPLM dispatch completes and releases in-progress guard"));
    Restrict->Destroy();Expect(!IsValid(Proxy) || Proxy->IsActorBeingDestroyed(),TEXT("proxy lifetime follows restrict actor"));
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    UE_LOG(LogTemp,Display,TEXT("SMART TRIGGER TESTS: %d checks passed"),Count);return true;
}
