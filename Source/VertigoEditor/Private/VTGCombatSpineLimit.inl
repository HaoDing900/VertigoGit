#include "Animation/AnimBlueprint.h"
#include "Animation/VTGCameraLookAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "K2Node_VariableSet.h"

namespace VTGUIFeedback
{
void CombatSpineLimit(bool Verify)
{
    auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/Characters/Sa/ABP_Sa"));check(BP);
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);int32 Count=0;
    for(auto* Graph:Graphs)for(UEdGraphNode* Node:Graph->Nodes)if(auto* Setter=Cast<UK2Node_VariableSet>(Node))
        if(Setter->VariableReference.GetMemberName()==TEXT("Aim Offset"))
            if(auto* Input=Setter->FindPin(TEXT("Aim Offset"));Input && Input->LinkedTo.Num()==1)
                if(auto* Clamp=Cast<UK2Node_CallFunction>(Input->LinkedTo[0]->GetOwningNode());Clamp && Clamp->FunctionReference.GetMemberName()==TEXT("FClamp"))
                {
                    ++Count;auto* Min=Clamp->FindPinChecked(TEXT("Min"));auto* Max=Clamp->FindPinChecked(TEXT("Max"));check(Min->LinkedTo.IsEmpty() && Max->LinkedTo.IsEmpty());
                    // Aim Offset is the negative of view pitch: upward uses the minimum.
                    if(!Verify){Backup(BP);Min->DefaultValue=TEXT("-20.0");Max->DefaultValue=TEXT("30.0");}
                    check(FCString::Atof(*Min->DefaultValue)==-20 && FCString::Atof(*Max->DefaultValue)==30);
                }
    check(Count==1);Compile(BP);if(!Verify)Save(BP);
    const auto* Defaults=CastChecked<UVTGCameraLookAnimInstance>(BP->GeneratedClass->GetDefaultObject());
    UE_LOG(LogTemp,Display,TEXT("SA HEAD LOOK DEFAULTS enabled=%d yaw=%.1f pitch=%.1f interp=%.1f"),Defaults->bCameraLookEnabled,Defaults->MaxLookYaw,Defaults->MaxLookPitch,Defaults->LookInterpSpeed);
    TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Class=LoadClass<ACharacter>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C"));auto* Player=World->SpawnActor<ACharacter>(Class,FTransform::Identity,Spawn);
    auto* Controller=World->SpawnActor<APlayerController>();Controller->Possess(Player);
    auto Flag=[&](const TCHAR* Name,bool Value){auto* P=FindFProperty<FBoolProperty>(Class,Name);check(P);P->SetPropertyValue_InContainer(Player,Value);};
    Flag(TEXT("InCombat"),true);Flag(TEXT("IsAiming"),true);Flag(TEXT("StoryMode?"),false);Flag(TEXT("IsDead?"),false);Flag(TEXT("HidePlayerNCam"),false);Flag(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),false);Flag(TEXT("UsingStunGun"),false);Flag(TEXT("UsingSMG1"),false);
    auto* Mesh=Player->GetMesh();Mesh->InitAnim(true);auto* Anim=CastChecked<UVTGCameraLookAnimInstance>(Mesh->GetAnimInstance());Anim->bCameraLookEnabled=false;
    FindFProperty<FObjectPropertyBase>(Anim->GetClass(),TEXT("As Sa"))->SetObjectPropertyValue_InContainer(Anim,Player);
    auto* Aim=FindFProperty<FNumericProperty>(Anim->GetClass(),TEXT("Aim Offset"));auto* Weapon=FindFProperty<FByteProperty>(Class,TEXT("CurrentMeleeWeapon"));check(Aim && Weapon);
    for(uint8 Equipment:{uint8(0),uint8(Weapon->Enum->GetValueByNameString(TEXT("NewEnumerator1")))})
    {
        Weapon->SetPropertyValue_InContainer(Player,Equipment);
        for(float Pitch:{0.f,15.f,89.f,-89.f,0.f})
        {
            Controller->SetControlRotation(FRotator(Pitch,0,0));
            for(int32 I=0;I<180;++I){Mesh->TickAnimation(1.f/60.f,false);Mesh->ConditionallyDispatchQueuedAnimEvents();}
            const double Value=Aim->GetFloatingPointPropertyValue(Aim->ContainerPtrToValuePtr<void>(Anim));
            UE_LOG(LogTemp,Display,TEXT("COMBAT SPINE weapon=%d input=%.1f aimOffset=%.3f"),Equipment,Pitch,Value);
            check(Value>=-20.001 && Value<=30.001);
            if(FMath::Abs(Pitch)>30)check(FMath::IsNearlyEqual(Value,Pitch>0?-20.:30.,.1));
            if(Pitch==0)check(FMath::IsNearlyZero(Value,.1));
        }
    }
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    UE_LOG(LogTemp,Display,TEXT("COMBAT SPINE LIMIT PASS: real ABP fists/hammer upward capped at 20; downward remains 30; neutral preserved"));
}
}
