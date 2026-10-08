#include "VTGSewerCombatCommandlet.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LatentActionManager.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_Event.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputActionDelegateBinding.h"
#include "InputAction.h"

namespace VTGSewerCombat
{
const FString Marker(TEXT("VTG sewer free combat and hammer reach v1"));
UEdGraphPin* Pin(UEdGraphNode* N, const TCHAR* Name) { return N->FindPinChecked(FName(Name)); }
UEdGraphNode* Named(UEdGraph* G, const TCHAR* Name)
{
    for (UEdGraphNode* N : G->Nodes) if (N->GetName() == Name) return N;
    checkf(false, TEXT("Missing %s"), Name); return nullptr;
}
void Link(UEdGraphPin* A, UEdGraphPin* B) { check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B)); }
void Default(UEdGraphPin* P, const TCHAR* Value) { GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*P,Value); }
template<class T> T* Add(UEdGraph* G, int X, int Y)
{
    auto* N = NewObject<T>(G); G->AddNode(N,false,false); N->CreateNewGuid();
    N->NodePosX=X; N->NodePosY=Y; N->NodeComment=Marker; return N;
}
UK2Node_CallFunction* Call(UEdGraph* G, UClass* C, const TCHAR* Name, int X, int Y)
{
    auto* F=C->FindFunctionByName(Name); check(F);
    auto* N=Add<UK2Node_CallFunction>(G,X,Y); N->SetFromFunction(F); N->AllocateDefaultPins(); return N;
}
void Apply(UBlueprint* BP, UEdGraph* G)
{
    TSet<FGuid> Original;
    for(UEdGraphNode* N:G->Nodes) { check(!N->NodeComment.Contains(Marker)); Original.Add(N->NodeGuid); }
    const int Count=G->Nodes.Num();
    auto* Query=CastChecked<UK2Node_CallFunction>(Named(G,TEXT("K2Node_CallFunction_206")));
    check(Query->FunctionReference.GetMemberName()==TEXT("FindNearestEnemy"));
    auto* Found=Pin(Query,TEXT("FoundTarget")); const auto Uses=Found->LinkedTo;
    check(Uses.Num()==2);
    auto* Level=Call(G,UGameplayStatics::StaticClass(),TEXT("GetCurrentLevelName"),-3520,420);
    Default(Pin(Level,TEXT("bRemovePrefixString")),TEXT("true"));
    auto* QueryIn=Pin(Query,TEXT("execute")); const auto QuerySources=QueryIn->LinkedTo; check(QuerySources.Num()==1);
    QueryIn->BreakAllPinLinks(); Link(QuerySources[0],Pin(Level,TEXT("execute"))); Link(Pin(Level,TEXT("then")),QueryIn);
    auto* Sewer=Call(G,UKismetStringLibrary::StaticClass(),TEXT("EqualEqual_StrStr"),-3216,420);
    Link(Pin(Level,TEXT("ReturnValue")),Pin(Sewer,TEXT("A")));
    Default(Pin(Sewer,TEXT("B")),TEXT("L_SewerUnderApartment"));
    auto* Enabled=Call(G,UKismetMathLibrary::StaticClass(),TEXT("BooleanOR"),-2850,420);
    Found->BreakAllPinLinks(); Link(Found,Pin(Enabled,TEXT("A")));
    Link(Pin(Sewer,TEXT("ReturnValue")),Pin(Enabled,TEXT("B")));
    for(auto* Use:Uses) Link(Pin(Enabled,TEXT("ReturnValue")),Use);
    Enabled->NodeComment=Marker+TEXT(": sewer stays in combat even with no target; other levels retain proximity rules");

    // Initialize before the existing BeginPlay flow so attack does not have to
    // wait for the first 0.1-second enemy-check timer. All original startup work remains.
    UK2Node_Event* Begin=nullptr;
    for(UEdGraphNode* N:G->Nodes)
        if(auto* E=Cast<UK2Node_Event>(N); E && E->EventReference.GetMemberName()==TEXT("ReceiveBeginPlay")) { check(!Begin); Begin=E; }
    check(Begin); auto* Start=Pin(Begin,TEXT("then")); const auto After=Start->LinkedTo; check(After.Num());
    auto* Initialize=Call(G,BP->SkeletonGeneratedClass,TEXT("CheckEnemyTimer"),Begin->NodePosX+550,Begin->NodePosY-220);
    Start->BreakAllPinLinks(); Link(Start,Pin(Initialize,TEXT("execute")));
    for(auto* P:After) Link(Pin(Initialize,TEXT("then")),P);

    auto* Damage=CastChecked<UK2Node_CallFunction>(Named(G,TEXT("K2Node_CallFunction_344")));
    check(Damage->FunctionReference.GetMemberName()==TEXT("FindMeleeTargets"));
    auto* Radius=Pin(Damage,TEXT("Radius")); check(Radius->LinkedTo.IsEmpty() && FCString::Atof(*Radius->DefaultValue)==200.f);
    auto* WeaponProp=FindFProperty<FByteProperty>(BP->SkeletonGeneratedClass,TEXT("CurrentMeleeWeapon"));
    check(WeaponProp && WeaponProp->Enum);
    const int64 Hammer=WeaponProp->Enum->GetValueByNameString(TEXT("NewEnumerator1"));
    check(Hammer>=0 && WeaponProp->Enum->GetDisplayNameTextByValue(Hammer).ToString()==TEXT("SmolHammer"));
    auto* Weapon=Add<UK2Node_VariableGet>(G,-2496,9650);
    Weapon->SetFromProperty(WeaponProp,true,BP->SkeletonGeneratedClass); Weapon->AllocateDefaultPins();
    auto* IsHammer=Call(G,UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_ByteByte"),-2240,9650);
    Link(Pin(Weapon,TEXT("CurrentMeleeWeapon")),Pin(IsHammer,TEXT("A")));
    Default(Pin(IsHammer,TEXT("B")),*FString::FromInt(Hammer));
    auto* Reach=Call(G,UKismetMathLibrary::StaticClass(),TEXT("SelectFloat"),-1920,9650);
    Default(Pin(Reach,TEXT("A")),TEXT("300")); Default(Pin(Reach,TEXT("B")),TEXT("200"));
    Link(Pin(IsHammer,TEXT("ReturnValue")),Pin(Reach,TEXT("bPickA")));
    Link(Pin(Reach,TEXT("ReturnValue")),Radius);
    Reach->NodeComment=Marker+TEXT(": hit radius in cm: hammer 300, fist 200; target acquisition stays 500");
    for(UEdGraphNode* N:G->Nodes) Original.Remove(N->NodeGuid);
    check(Original.IsEmpty());
    UE_LOG(LogTemp,Display,TEXT("SEWER retained %d original nodes; added %d"),Count,G->Nodes.Num()-Count);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
}

bool Test(UBlueprint* BP, const FString& MapName, const FString& Prefix, bool AlwaysCombat)
{
    TGuardValue<bool> AllowScript(GAllowActorScriptExecutionInEditor,true);
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    auto* Package=CreatePackage(*(TEXT("/Temp/CombatPolicy/")+MapName));
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,FName(*MapName),Package,true,ERHIFeatureLevel::Num,&Init);
    World->StreamingLevelsPrefix=Prefix;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player=World->SpawnActor<ACharacter>(BP->GeneratedClass,FTransform::Identity,Spawn); check(Player);
    auto* Mesh=Player->GetMesh(); Mesh->SetAnimInstanceClass(UAnimInstance::StaticClass()); Mesh->InitAnim(true);
    auto* Anim=Mesh->GetAnimInstance(); check(Anim);
    auto Bool=[&](const TCHAR* N){auto* P=FindFProperty<FBoolProperty>(Player->GetClass(),N);check(P);return P;};
    auto Set=[&](const TCHAR* N,bool V){Bool(N)->SetPropertyValue_InContainer(Player,V);};
    auto Get=[&](const TCHAR* N){return Bool(N)->GetPropertyValue_InContainer(Player);};
    auto Run=[&](const TCHAR* N){auto* F=Player->FindFunction(N);check(F);FStructOnScope Args(F);Player->ProcessEvent(F,Args.GetStructMemory());};
    auto Clear=[&](){Anim->Montage_Stop(0); Mesh->TickAnimation(.02f,false); Mesh->ConditionallyDispatchQueuedAnimEvents(); World->GetLatentActionManager().BeginFrame(); World->GetLatentActionManager().ProcessLatentActions(Player,.02f); Run(TEXT("ResetCombo"));};
    int Failures=0;
    auto Expect=[&](bool Pass,const TCHAR* Label){UE_LOG(LogTemp,Display,TEXT("SEWER TEST %s [%s]: %s"),Pass?TEXT("PASS"):TEXT("FAIL"),*MapName,Label);Failures+=!Pass;};
    Set(TEXT("InCombat"),false); Set(TEXT("StoryMode?"),true);
    Set(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),false);
    Run(TEXT("CheckEnemyTimer"));
    Expect(Get(TEXT("InCombat"))==AlwaysCombat && Get(TEXT("StoryMode?"))!=AlwaysCombat,TEXT("empty level applies the correct combat policy"));
    UFunction* Fire=nullptr;
    for(auto Binding:CastChecked<UBlueprintGeneratedClass>(BP->GeneratedClass)->DynamicBindingObjects)
        if(auto* Input=Cast<UEnhancedInputActionDelegateBinding>(Binding))
            for(const auto& B:Input->InputActionDelegateBindings)
                if(B.InputAction && B.InputAction->GetName()==TEXT("IA_Fire") && B.TriggerEvent==ETriggerEvent::Started) { check(!Fire); Fire=Player->FindFunction(B.FunctionNameToBind); }
    check(Fire); auto Press=[&](){FStructOnScope Args(Fire);Player->ProcessEvent(Fire,Args.GetStructMemory());};
    Set(TEXT("CanAttack?"),true);Set(TEXT("CanPunch?"),true);Set(TEXT("HasWeaponInHand"),false);
    Press(); Expect((Anim->GetCurrentActiveMontage()!=nullptr)==AlwaysCombat,TEXT("real attack input works without enemies only in the sewer")); Clear();
    auto* Target=World->SpawnActor<AVTGAttackRangeTestTarget>(AVTGAttackRangeTestTarget::StaticClass(),FTransform(FVector(250,0,0)),Spawn);check(Target);
    Target->GetCapsuleComponent()->SetCapsuleSize(1,1);
    Run(TEXT("CheckEnemyTimer"));Expect(Get(TEXT("InCombat")),TEXT("nearby enemy still enables combat"));
    Target->SetActorLocation(FVector(1000,0,0));Run(TEXT("CheckEnemyTimer"));
    Expect(Get(TEXT("InCombat"))==AlwaysCombat,TEXT("leaving the last enemy preserves level combat policy"));
    if(AlwaysCombat)
    {
        Set(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),true); Press();
        Expect(Anim->GetCurrentActiveMontage()==nullptr,TEXT("cinematic input lock still blocks attacks"));
        Set(TEXT("IsInCineCutscene_LockInput(NotWorkForMont)"),false); Press();
        Expect(Anim->GetCurrentActiveMontage()!=nullptr,TEXT("attacking resumes after cinematic lock clears")); Clear();
    }

    auto* Weapon=FindFProperty<FByteProperty>(Player->GetClass(),TEXT("CurrentMeleeWeapon"));check(Weapon&&Weapon->Enum);
    auto* Combo=FindFProperty<FIntProperty>(Player->GetClass(),TEXT("ComboIndex"));check(Combo);Combo->SetPropertyValue_InContainer(Player,0);
    // A tiny receiver isolates the attack query's radius from enemy capsule size.
    const FVector Center=Mesh->GetSocketLocation(TEXT("forearm_stretch_lSocket"));
    auto Hit=[&](uint8 WeaponId,float Distance){Weapon->SetPropertyValue_InContainer(Player,WeaponId);Target->SetActorLocation(Center+FVector(Distance,0,0));Target->TotalDamage=0;Run(TEXT("DealPunchDmg"));return Target->TotalDamage;};
    const uint8 Fist=Weapon->Enum->GetValueByNameString(TEXT("NewEnumerator0"));
    const uint8 Hammer=Weapon->Enum->GetValueByNameString(TEXT("NewEnumerator1"));
    Expect(Hit(Fist,180)>0,TEXT("fist still damages inside original 200 cm radius"));
    Expect(Hit(Fist,250)==0,TEXT("fist does not gain the hammer extension"));
    Expect(Hit(Hammer,250)>0,TEXT("hammer DealPunchDmg damages in the new 200-300 cm band"));
    Expect(Hit(Hammer,350)==0,TEXT("hammer does not damage outside the new radius"));
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    UE_LOG(LogTemp,Display,TEXT("SEWER TEST [%s] failures=%d"),*MapName,Failures);
    return Failures==0;
}
}

UVTGSewerCombatCommandlet::UVTGSewerCombatCommandlet() {IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UVTGSewerCombatCommandlet::Main(const FString& Params)
{
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa"));if(!BP)return 1;
    UEdGraph* G=nullptr;for(UEdGraph* Graph:BP->UbergraphPages)if(Graph->GetName()==TEXT("EventGraph"))G=Graph;if(!G)return 2;
    if(!Params.Contains(TEXT("VerifyOnly")))VTGSewerCombat::Apply(BP,G);
    FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Results);
    UE_LOG(LogTemp,Display,TEXT("SEWER compile errors=%d warnings=%d"),Results.NumErrors,Results.NumWarnings);
    if(Results.NumErrors||BP->Status==BS_Error)return 3;
    if(Params.Contains(TEXT("Test")))
    {
        bool Passed=VTGSewerCombat::Test(BP,TEXT("L_SewerUnderApartment"),TEXT(""),true);
        Passed=VTGSewerCombat::Test(BP,TEXT("UEDPIE_0_L_SewerUnderApartment"),TEXT("UEDPIE_0_"),true)&&Passed;
        Passed=VTGSewerCombat::Test(BP,TEXT("L_CombatTest"),TEXT(""),false)&&Passed;
        if(!Passed)return 4;
    }
    if(Params.Contains(TEXT("VerifyOnly")))return 0;
    FSavePackageArgs Save;Save.TopLevelFlags=RF_Public|RF_Standalone;Save.SaveFlags=SAVE_NoError;
    const FString Filename=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    if(!UPackage::SavePackage(BP->GetOutermost(),BP,*Filename,Save))return 5;
    UE_LOG(LogTemp,Display,TEXT("SEWER saved only %s"),*Filename);return 0;
}
