#include "VTGBlackMarketCombatCommandlet.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_AddDelegate.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"

namespace MarketCombat
{
const TCHAR* ArmName=TEXT("InitializeAlleyCombat");

UEdGraph* EventGraph(UBlueprint* BP){for(UEdGraph* G:BP->UbergraphPages)if(G->GetName()==TEXT("EventGraph"))return G;return nullptr;}
UEdGraphNode* Named(UEdGraph* G,const TCHAR* Name){for(UEdGraphNode* N:G->Nodes)if(N->GetName()==Name)return N;return nullptr;}
template<class T> T* Add(UEdGraph* G,int X,int Y){auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;N->NodeComment=TEXT("VTG black market combat: exact three enemies, once per enemy");return N;}
void Link(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP){check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A->FindPinChecked(AP),B->FindPinChecked(BP)));}
UK2Node_CallFunction* Call(UEdGraph* G,UFunction* F,int X,int Y){check(F);auto* N=Add<UK2Node_CallFunction>(G,X,Y);N->SetFromFunction(F);N->AllocateDefaultPins();return N;}
UK2Node_VariableGet* Get(UEdGraph* G,FName Name,int X,int Y){auto* N=Add<UK2Node_VariableGet>(G,X,Y);N->VariableReference.SetSelfMember(Name);N->AllocateDefaultPins();return N;}
UK2Node_CustomEvent* Event(UEdGraph* G,FName Name,int X,int Y){auto* N=Add<UK2Node_CustomEvent>(G,X,Y);N->CustomFunctionName=Name;N->AllocateDefaultPins();return N;}
UK2Node_MacroInstance* Once(UEdGraph* G,int X,int Y)
{
    auto* Macros=LoadObject<UBlueprint>(nullptr,TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));check(Macros);
    UEdGraph* Macro=nullptr;for(UEdGraph* M:Macros->MacroGraphs)if(M->GetName()==TEXT("DoOnce"))Macro=M;check(Macro);
    auto* N=Add<UK2Node_MacroInstance>(G,X,Y);N->SetMacroGraph(Macro);N->AllocateDefaultPins();return N;
}
UK2Node_VariableSet* Set(UEdGraph* G,UBlueprint* BP,FName Name,const TCHAR* Value,int X,int Y,bool Self=true)
{
    auto* N=Add<UK2Node_VariableSet>(G,X,Y);
    if(Self)N->VariableReference.SetSelfMember(Name);else N->VariableReference.SetExternalMember(Name,BP->GeneratedClass);
    N->AllocateDefaultPins();GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*N->FindPinChecked(Name),Value);return N;
}
bool Compile(UBlueprint* BP){FCompilerResultsLog R;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&R);UE_LOG(LogTemp,Display,TEXT("MARKET compile %s errors=%d warnings=%d"),*BP->GetName(),R.NumErrors,R.NumWarnings);return !R.NumErrors && BP->Status!=BS_Error;}
void Save(UBlueprint* BP)
{
    FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    FString Backup=FPaths::ProjectDir()/TEXT("Scripts/BlackMarketCombat/Backup")/(BP->GetName()+TEXT(".uasset"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
    if(!IFileManager::Get().FileExists(*Backup))check(IFileManager::Get().Copy(*Backup,*File)==COPY_OK);
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args));
}
void Apply(UBlueprint* BP,UBlueprint* Player,UBlueprint* Base,UBlueprint* Collision)
{
    auto* G=EventGraph(BP);check(G);
    check(!BP->GeneratedClass->FindFunctionByName(ArmName));
    auto* Arm=Event(G,ArmName,10800,12000);auto* Gate=Once(G,11080,12000);
    auto* Count=Set(G,BP,TEXT("L4_LeftEnemies"),TEXT("3"),11360,12000);
    Link(Arm,TEXT("then"),Gate,TEXT("execute"));Link(Gate,TEXT("Completed"),Count,TEXT("execute"));
    UEdGraphNode* Tail=Count;
    auto* OriginalBind=CastChecked<UK2Node_AddDelegate>(Named(G,TEXT("K2Node_AddDelegate_0")));
    for(int32 I=1;I<=3;++I)
    {
        auto* Enemy=Get(G,FName(*FString::Printf(TEXT("Enm_Goon%d"),I)),11360,I*220+12100);
        auto* Bind=Add<UK2Node_AddDelegate>(G,11680+(I-1)*320,12000);
        Bind->DelegateReference=OriginalBind->DelegateReference;Bind->AllocateDefaultPins();
        const FName Callback(*FString::Printf(TEXT("AlleyEnemy%dDefeated"),I));
        auto* OnDeath=Event(G,Callback,10800,I*380+13000);auto* Unique=Once(G,11080,I*380+13000);
        auto* Notify=Call(G,BP->GeneratedClass->FindFunctionByName(TEXT("OnDefeated")),11360,I*380+13000);
        Link(Tail,TEXT("then"),Bind,TEXT("execute"));Link(Enemy,*FString::Printf(TEXT("Enm_Goon%d"),I),Bind,TEXT("self"));
        Link(OnDeath,TEXT("OutputDelegate"),Bind,TEXT("Delegate"));Link(OnDeath,TEXT("then"),Unique,TEXT("execute"));Link(Unique,TEXT("Completed"),Notify,TEXT("execute"));Tail=Bind;
    }
    auto* PlayerRef=Get(G,TEXT("Player Pawn"),12400,12300);
    auto* Story=Set(G,Player,TEXT("StoryMode?"),TEXT("false"),12640,12000,false);
    Link(PlayerRef,TEXT("Player Pawn"),Story,TEXT("self"));Link(Tail,TEXT("then"),Story,TEXT("execute"));
    // Match StartAlleyConflict's explicit combat-mode update, without deleting the hidden story NPCs.
    auto* InCombat=Set(G,Player,TEXT("InCombat"),TEXT("true"),12960,12000,false);
    Link(PlayerRef,TEXT("Player Pawn"),InCombat,TEXT("self"));Link(Story,TEXT("then"),InCombat,TEXT("execute"));
    auto* Mode=Call(G,Player->GeneratedClass->FindFunctionByName(TEXT("ToggleCombatMode")),13280,12000);
    Link(PlayerRef,TEXT("Player Pawn"),Mode,TEXT("self"));Link(InCombat,TEXT("then"),Mode,TEXT("execute"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);check(Compile(BP));
    // Both entrance routes use the same initialization. Disable the old all-AI binding path.
    auto* Legacy=Named(G,TEXT("K2Node_CallFunction_185"));check(Legacy);
    auto* Input=Legacy->FindPinChecked(TEXT("execute"));check(Input->LinkedTo.Num()==1);
    auto* Before=Input->LinkedTo[0];Input->BreakAllPinLinks();
    auto* ArmOld=Call(G,BP->GeneratedClass->FindFunctionByName(ArmName),10400,11800);check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Before,ArmOld->FindPinChecked(TEXT("execute"))));
    auto* Hide=Named(G,TEXT("K2Node_CallFunction_138"));check(Hide && Hide->FindPinChecked(TEXT("then"))->LinkedTo.IsEmpty());
    auto* ArmTrigger=Call(G,BP->GeneratedClass->FindFunctionByName(ArmName),10400,11500);Link(Hide,TEXT("then"),ArmTrigger,TEXT("execute"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    auto* Damage=Named(EventGraph(Collision),TEXT("K2Node_CallFunction_12"));check(Damage);
    auto* Type=LoadClass<UDamageType>(nullptr,TEXT("/Game/AI/BossAIToolkit/Blueprints/DamageTypes/BP_DamageType_Default.BP_DamageType_Default_C"));check(Type);
    auto* Pin=Damage->FindPinChecked(TEXT("DamageTypeClass"));check(Pin->LinkedTo.IsEmpty());
    GetDefault<UEdGraphSchema_K2>()->TrySetDefaultObject(*Pin,Type);
    Damage->NodeComment=TEXT("Use BI_DamageType-compatible default: player ProcessDamage must receive a valid damage type.");
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Collision);
}
void Run(UObject* O,const TCHAR* Name){auto* F=O->FindFunction(Name);check(F);FStructOnScope Args(F);if(auto* Dt=FindFProperty<FFloatProperty>(F,TEXT("DeltaSeconds")))Dt->SetPropertyValue_InContainer(Args.GetStructMemory(),1.f/60.f);O->ProcessEvent(F,Args.GetStructMemory());}
void Object(UObject* O,const TCHAR* Name,UObject* Value){auto* P=FindFProperty<FObjectPropertyBase>(O->GetClass(),Name);check(P);P->SetObjectPropertyValue_InContainer(O,Value);}
int32 Health(UObject* O){auto* P=FindFProperty<FIntProperty>(O->GetClass(),TEXT("Health"));check(P);return P->GetPropertyValue_InContainer(O);}
void Flag(UObject* O,const TCHAR* Name,bool V){auto* P=FindFProperty<FBoolProperty>(O->GetClass(),Name);check(P);P->SetPropertyValue_InContainer(O,V);}
void Test(UBlueprint* BP,UBlueprint* Player,UBlueprint* Collision)
{
    FEditorScriptExecutionGuard Guard;
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("BlackMarketCombatTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);check(World);auto& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);auto* GI=NewObject<UGameInstance>();World->SetGameInstance(GI);Context.OwningGameInstance=GI;check(World->SetGameMode(FURL(nullptr,TEXT("/Engine/Maps/Entry?game=/Script/Engine.GameModeBase"),TRAVEL_Absolute)));
    FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Sa=World->SpawnActor<ACharacter>(Player->GeneratedClass,FTransform::Identity,Spawn);check(Sa);
    auto* PC=World->SpawnActor<APlayerController>();PC->Possess(Sa);Sa->GetMesh()->InitAnim(true);
    // Only the transient test copy replaces narrative/arena completion and cosmetic combat-mode work.
    auto* Copy=DuplicateObject<UBlueprint>(BP,GetTransientPackage(),TEXT("MarketCombatRegression"));check(Copy);
    FEdGraphPinType CountType;CountType.PinCategory=UEdGraphSchema_K2::PC_Int;check(FBlueprintEditorUtils::AddMemberVariable(Copy,TEXT("RegressionCompletions"),CountType));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Copy);check(Compile(Copy));
    auto* G=EventGraph(Copy);auto* Finish=Named(G,TEXT("K2Node_CallFunction_188"));check(Finish);
    auto* In=Finish->FindPinChecked(TEXT("execute"));check(In->LinkedTo.Num()==1);auto* Before=In->LinkedTo[0];In->BreakAllPinLinks();
    auto* Capture=Set(G,Copy,TEXT("RegressionCompletions"),TEXT("0"),0,0);
    auto* Captured=Get(G,TEXT("RegressionCompletions"),0,200);auto* Inc=Call(G,UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Add_IntInt")),200,200);Link(Captured,TEXT("RegressionCompletions"),Inc,TEXT("A"));GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Inc->FindPinChecked(TEXT("B")),TEXT("1"));Link(Inc,TEXT("ReturnValue"),Capture,TEXT("RegressionCompletions"));
    check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Before,Capture->FindPinChecked(TEXT("execute"))));
    // Cosmetic graph needs a live viewport; its state assignments are still tested.
    for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))if(C->NodePosY==12000 && C->FunctionReference.GetMemberName()==TEXT("ToggleCombatMode"))C->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Copy);check(Compile(Copy));
    auto* Manager=World->SpawnActor<AActor>(Copy->GeneratedClass,FTransform::Identity,Spawn);check(Manager);Object(Manager,TEXT("Player Pawn"),Sa);
    const TCHAR* Names[]={TEXT("BP_Enm_StreetGoon1Misto"),TEXT("BP_Enm_StreetGoon2Nara"),TEXT("BP_Enm_SteetGoon3_Brosky")};
    TArray<ACharacter*> Enemies;
    for(int I=0;I<3;++I){auto* Class=LoadClass<ACharacter>(nullptr,*(FString(TEXT("/Game/Characters/NPC/StreetGoons/"))+Names[I]+TEXT(".")+Names[I]+TEXT("_C")));check(Class);auto* E=World->SpawnActor<ACharacter>(Class,FTransform(FVector(3000+I*500,0,0)),Spawn);check(E);Enemies.Add(E);Object(Manager,*FString::Printf(TEXT("Enm_Goon%d"),I+1),E);}
    Run(Manager,ArmName);Run(Manager,ArmName);auto* Left=FindFProperty<FIntProperty>(Manager->GetClass(),TEXT("L4_LeftEnemies"));check(Left && Left->GetPropertyValue_InContainer(Manager)==3);
    check(!FindFProperty<FBoolProperty>(Sa->GetClass(),TEXT("StoryMode?"))->GetPropertyValue_InContainer(Sa));
    auto* Completed=FindFProperty<FIntProperty>(Manager->GetClass(),TEXT("RegressionCompletions"));check(Completed);
    for(int I=0;I<3;++I)
    {
        auto* P=FindFProperty<FMulticastDelegateProperty>(Enemies[I]->GetClass(),TEXT("On EnemyDeath"));check(P);
        P->ContainerPtrToValuePtr<FMulticastScriptDelegate>(Enemies[I])->ProcessMulticastDelegate<UObject>(nullptr);P->ContainerPtrToValuePtr<FMulticastScriptDelegate>(Enemies[I])->ProcessMulticastDelegate<UObject>(nullptr);
        if(I==0)Run(Manager,ArmName);check(Left->GetPropertyValue_InContainer(Manager)==2-I);check(Completed->GetPropertyValue_InContainer(Manager)==(I==2?1:0));
        UE_LOG(LogTemp,Display,TEXT("MARKET TEST PASS unique death %d remaining=%d completion=%d; duplicate ignored"),I+1,2-I,Completed->GetPropertyValue_InContainer(Manager));
    }
    // Full collision trace -> ApplyPointDamage -> actual BP_Player_Sa ProcessDamage -> Health.
    FHitResult DirectHit(Sa,Sa->GetCapsuleComponent(),Sa->GetActorLocation(),FVector::UpVector);
    auto* DT=LoadClass<UDamageType>(nullptr,TEXT("/Game/AI/BossAIToolkit/Blueprints/DamageTypes/BP_DamageType_Default.BP_DamageType_Default_C"));Flag(Sa,TEXT("InCombat"),false);
    UGameplayStatics::ApplyPointDamage(Sa,7,FVector::ForwardVector,DirectHit,PC,Enemies[0],DT);UE_LOG(LogTemp,Display,TEXT("MARKET direct point damage health=%d"),Health(Sa));check(Health(Sa)==93);FindFProperty<FIntProperty>(Sa->GetClass(),TEXT("Health"))->SetPropertyValue_InContainer(Sa,100);
    int32 Part=2;
    for(auto* Enemy:Enemies)
    {
        auto* P=FindFProperty<FObjectPropertyBase>(Enemy->GetClass(),TEXT("MeleeCollisionComponent"));check(P);auto* Comp=P->GetObjectPropertyValue_InContainer(Enemy);check(Comp);
        if(Enemy==Enemies.Last())Part=0;
        auto* Enable=Comp->FindFunction(TEXT("EnableCollision"));FStructOnScope Args(Enable);auto* Which=FindFProperty<FByteProperty>(Enable,TEXT("CollisionPart"));check(Which);Which->SetPropertyValue_InContainer(Args.GetStructMemory(),Which->Enum->GetValueByNameString(FString::Printf(TEXT("NewEnumerator%d"),Part)));
        Comp->ProcessEvent(Enable,Args.GetStructMemory());
        auto* Components=FindFProperty<FArrayProperty>(Comp->GetClass(),TEXT("CollisionComponents"));check(Components);FScriptArrayHelper List(Components,Components->ContainerPtrToValuePtr<void>(Comp));check(List.Num()>0);auto* Item=CastFieldChecked<FStructProperty>(Components->Inner);auto* Scene=Cast<USceneComponent>(FindFProperty<FObjectProperty>(Item->Struct,TEXT("Component_2_3941386A40A6BCBCE45808A34CB04791"))->GetObjectPropertyValue_InContainer(List.GetRawPtr(0)));check(Scene);auto* Sockets=FindFProperty<FArrayProperty>(Item->Struct,TEXT("Sockets_6_12180EF74F4BA7D53FD5AC84FDE6E1BF"));check(Sockets);FScriptArrayHelper SocketList(Sockets,Sockets->ContainerPtrToValuePtr<void>(List.GetRawPtr(0)));check(SocketList.Num()>0);const FName Socket=CastFieldChecked<FNameProperty>(Sockets->Inner)->GetPropertyValue(SocketList.GetRawPtr(0));
        Sa->SetActorLocation(Scene->GetSocketLocation(Socket));Flag(Sa,TEXT("InCombat"),false);
        UE_LOG(LogTemp,Display,TEXT("MARKET trace state player capsule object=%d collision=%d mesh=%s socket=%s"),(int)Sa->GetCapsuleComponent()->GetCollisionObjectType(),(int)Sa->GetCapsuleComponent()->GetCollisionEnabled(),*Sa->GetMesh()->GetComponentLocation().ToString(),*Enemy->GetMesh()->GetSocketLocation(TEXT("hand_r")).ToString());
        const int32 BeforeHealth=Health(Sa);Run(Comp,TEXT("ReceiveTick"));Run(Comp,TEXT("ReceiveTick"));
        UE_LOG(LogTemp,Display,TEXT("MARKET actual player damage %s Health=%d -> %d"),*Enemy->GetName(),BeforeHealth,Health(Sa));check(Health(Sa)<BeforeHealth);
    }
    for(auto* E:Enemies)World->DestroyActor(E);World->DestroyActor(Manager);World->DestroyActor(Sa);World->DestroyActor(PC);GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
}
}
UVTGBlackMarketCombatCommandlet::UVTGBlackMarketCombatCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}

int32 UVTGBlackMarketCombatCommandlet::Main(const FString& Params)
{
    using namespace MarketCombat;
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket.BPLM_BlackMarket"));
    auto* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa"));
    auto* Base=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AI/AIBehaviorSystem/Blueprint/AI/BP_AI_Base.BP_AI_Base"));
    auto* Collision=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AI/AIBehaviorSystem/Blueprint/Component/BP_CollisionComponent.BP_CollisionComponent"));
    check(BP && Player && Base && Collision);
    if(!Params.Contains(TEXT("VerifyOnly")))Apply(BP,Player,Base,Collision);
    check(Compile(BP) && Compile(Collision) && Compile(Player));
    Test(BP,Player,Collision);
    if(!Params.Contains(TEXT("VerifyOnly"))){Save(BP);Save(Collision);}
    UE_LOG(LogTemp,Display,TEXT("BLACK_MARKET_COMBAT_SUCCESS"));return 0;
}






