#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputActionDelegateBinding.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "UObject/StructOnScope.h"
#include "NarrativeComponent.h"
#include "VTGGameInstanceBase.h"

int32 VerifyAlley()
{
 TGuardValue<bool> Scripts(GAllowActorScriptExecutionInEditor,true);
 const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("AlleyInputRegression"),nullptr,true,ERHIFeatureLevel::Num,&Init);
 auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);
 auto* GI=NewObject<UVTGGameInstanceBase>();World->SetGameInstance(GI);Context.OwningGameInstance=GI;
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));
 auto* Actor=World->SpawnActor<ACharacter>(BP->GeneratedClass,FTransform::Identity,Spawn);
 auto* PC=World->SpawnActor<APlayerController>();PC->Possess(Actor);
 auto SetBool=[&](const TCHAR* N,bool V){auto* P=FindFProperty<FBoolProperty>(Actor->GetClass(),N);check(P);P->SetPropertyValue_InContainer(Actor,V);};
 SetBool(TEXT("FixedCameraLevel?"),true);SetBool(TEXT("fixed cam add input special value?"),true);
 SetBool(TEXT("IsReadingMail"),false);SetBool(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),false);
 auto* Yaw=FindFProperty<FIntProperty>(Actor->GetClass(),TEXT("fixed cam add input special value"));check(Yaw);
 UFunction* Move=nullptr;
 for(auto Binding:CastChecked<UBlueprintGeneratedClass>(BP->GeneratedClass)->DynamicBindingObjects)
 if(auto* Input=Cast<UEnhancedInputActionDelegateBinding>(Binding))for(const auto& B:Input->InputActionDelegateBindings)
 if(B.InputAction&&B.InputAction->GetName()==TEXT("IA_Move")&&B.TriggerEvent==ETriggerEvent::Triggered)Move=Actor->FindFunction(B.FunctionNameToBind);
 check(Move);
 int32 Failures=0;
 auto Expect=[&](bool Pass,const TCHAR* Label){UE_LOG(LogTemp,Display,TEXT("ALLEY TEST %s: %s"),Pass?TEXT("PASS"):TEXT("FAIL"),Label);if(!Pass)++Failures;};
 auto Input=[&](double X,double Y) {
 FStructOnScope Args(Move);
 auto* Value=FindFProperty<FStructProperty>(Move,TEXT("ActionValue"));check(Value);
 checkf(Value->Struct==FInputActionValue::StaticStruct(),TEXT("Unexpected input type: %s"),*Value->Struct->GetName());
 *Value->ContainerPtrToValuePtr<FInputActionValue>(Args.GetStructMemory())=FInputActionValue(FVector2D(X,Y));
 Actor->ConsumeMovementInputVector();Actor->ProcessEvent(Move,Args.GetStructMemory());
 return Actor->ConsumeMovementInputVector();
 };
 for(int32 Angle:{60,0,90,-90,180}) {
 Yaw->SetPropertyValue_InContainer(Actor,Angle);
 const FVector Forward=Input(0,1),Right=Input(1,0);
 Expect(Forward.Equals(FRotator(0,Angle,0).Vector(),.001),TEXT("W follows authored yaw"));
 Expect(Right.Equals(FRotationMatrix(FRotator(0,Angle,0)).GetUnitAxis(EAxis::Y),.001),TEXT("D uses perpendicular authored right"));
 Expect(FMath::Abs(FVector::DotProduct(Forward,Right))<.001,TEXT("W and D remain orthogonal"));
 Expect(Input(0,-1).Equals(-Forward,.001)&&Input(-1,0).Equals(-Right,.001),TEXT("S and A are exact opposites"));
 }
 auto* ManagerBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/SaApartment/BPLM_AptAlley"));
 auto* Manager=World->SpawnActor<AActor>(ManagerBP->GeneratedClass,FTransform::Identity,Spawn);
 FindFProperty<FNameProperty>(Manager->GetClass(),TEXT("Stage"))->SetPropertyValue_InContainer(Manager,TEXT("L3Day1"));
 auto* DialogueBP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_AptAlley_Day1"));
 auto* Component=NewObject<UNarrativeComponent>(Actor);Actor->AddInstanceComponent(Component);Component->RegisterComponent();
 auto* Dialogue=NewObject<UDialogue>(Component,DialogueBP->GeneratedClass);
 Dialogue->OwningComp=Component;
 auto* Template=CastChecked<UDialogueBlueprintGeneratedClass>(DialogueBP->GeneratedClass)->GetDialogueTemplate();
 UDialogueNode* Destination=nullptr;
 for(auto* N:Template->GetNodes())if(N->GetName()==TEXT("DialogueNode_NPC_5"))Destination=N;
 check(Destination);
 Expect(Destination->HasBoundNodeEvent(),TEXT("Police destination has an executable event"));
 UFunction* Enter=Dialogue->FindFunction(Destination->OnPlayNodeFuncName);check(Enter);
 FStructOnScope Params(Enter);
 FindFProperty<FObjectPropertyBase>(Enter,TEXT("Node"))->SetObjectPropertyValue_InContainer(Params.GetStructMemory(),Destination);
 auto* Started=FindFProperty<FBoolProperty>(Enter,TEXT("bStarted"));
 Started->SetPropertyValue_InContainer(Params.GetStructMemory(),true);
 Dialogue->ProcessEvent(Enter,Params.GetStructMemory());
 Expect(Context.TravelURL.Contains(TEXT("L3_CentralPoliceStation_FrontDoor_DownStream")),TEXT("Real dialogue event requests police map travel"));
 Expect(GI->CurrentStage==FName(TEXT("L3Day1Morning")),TEXT("Travel sets police morning stage"));
 Context.TravelURL.Empty();Started->SetPropertyValue_InContainer(Params.GetStructMemory(),false);
 Dialogue->ProcessEvent(Enter,Params.GetStructMemory());
 Expect(Context.TravelURL.IsEmpty(),TEXT("Node finish does not request duplicate travel"));
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 UE_LOG(LogTemp,Display,TEXT("ALLEY REGRESSION failures=%d"),Failures);
 return Failures?1:0;
}
