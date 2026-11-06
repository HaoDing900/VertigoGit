#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "NarrativeComponent.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace AptWindowEgg
{
const FName HelperName(TEXT("L3_ReappearGrossEff"));
UEdGraphNode* Node(UEdGraph* Graph,FName Name)
{for(UEdGraphNode* N:Graph->Nodes)if(N->GetFName()==Name)return N;return nullptr;}
UEdGraph* Graph(UBlueprint* BP)
{for(UEdGraph* G:BP->UbergraphPages)if(G->GetFName()==TEXT("EventGraph"))return G;checkNoEntry();return nullptr;}
void Link(UEdGraphPin* A,UEdGraphPin* B)
{check(A && B && GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B));}
UK2Node_CallFunction* Call(UEdGraph* G,FName Name,UFunction* Function,int32 X,int32 Y)
{
    check(Function);auto* N=NewObject<UK2Node_CallFunction>(G,Name,RF_Transactional);
    G->AddNode(N,false,false);N->CreateNewGuid();N->SetFromFunction(Function);N->AllocateDefaultPins();
    N->NodePosX=X;N->NodePosY=Y;return N;
}
void Replace(UEdGraph* G,UEdGraphNode* Old,UK2Node_CallFunction* New)
{
    for(auto* P:TArray<UEdGraphPin*>(Old->FindPinChecked(TEXT("execute"))->LinkedTo))Link(P,New->FindPinChecked(TEXT("execute")));
    for(auto* P:TArray<UEdGraphPin*>(Old->FindPinChecked(TEXT("then"))->LinkedTo))Link(New->FindPinChecked(TEXT("then")),P);
    FBlueprintEditorUtils::RemoveNode(CastChecked<UBlueprint>(G->GetOuter()),Old,true);
}
void Compile(UBlueprint* BP)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);
    check(!Log.NumErrors && BP->Status!=BS_Error);
}
AActor* Manager(UWorld* World,UBlueprint* BP)
{
    for(AActor* A:World->PersistentLevel->Actors)if(A && A->GetClass()==BP->GeneratedClass)return A;
    checkNoEntry();return nullptr;
}
AActor* Egg(AActor* M)
{return CastChecked<AActor>(FindFProperty<FObjectPropertyBase>(M->GetClass(),TEXT("EggOnWindow"))->GetObjectPropertyValue_InContainer(M));}
void SeekEnd(ULevelSequencePlayer* Player,ULevelSequence* Sequence)
{
    Player->Pause();
    const FFrameTime Frame=FFrameRate::TransformTime(FFrameTime(Sequence->GetMovieScene()->GetPlaybackRange().GetUpperBoundValue()-1),
        Sequence->GetMovieScene()->GetTickResolution(),Player->GetFrameRate());
    Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(Frame,EUpdatePositionMethod::Jump));
}
void Backup(UBlueprint* BP)
{
    const FString Source=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    const FString Dest=FPaths::ProjectSavedDir()/TEXT("QA/SaApartmentWindow/Backup")/(BP->GetName()+TEXT(".uasset"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest),true);
    if(!IFileManager::Get().FileExists(*Dest))check(IFileManager::Get().Copy(*Dest,*Source)==COPY_OK);
}
void Save(UBlueprint* BP)
{
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    const FString File=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
    check(UPackage::SavePackage(BP->GetOutermost(),BP,*File,Args));
}
void CheckState(AActor* E,const FTransform& Final,const TCHAR* Phase)
{
    UE_LOG(LogTemp,Display,TEXT("APT WINDOW EGG %s hidden=%d position=%s"),Phase,E->IsHidden(),*E->GetActorLocation().ToString());
    check(!E->IsHidden() && E->GetActorTransform().Equals(Final,.01f));
}
}

bool VTGRepairAptWindowEgg(bool Verify)
{
    using namespace AptWindowEgg;
    FEditorScriptExecutionGuard Guard;
    auto* M=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/In_Level/SaApartment/BPLM_SaApartment"));
    auto* D=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_SaApartmentD1"));
    auto* S=LoadObject<ULevelSequence>(nullptr,TEXT("/Game/LevelNSequence/In_Level/L3Apartment/LS/Squid/BLS_L3_Apt_SuqidLayEgg"));
    auto* W=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/ThirdPerson/Maps/SaApartment/SaApartment"));check(M && D && S && W);
    auto* OriginalEgg=Egg(Manager(W,M));const FTransform Initial=OriginalEgg->GetActorTransform();const bool InitiallyHidden=OriginalEgg->IsHidden();
    FMovieSceneSequencePlaybackSettings Settings;Settings.bRestoreState=true;
    ALevelSequenceActor* SequenceActor=nullptr;
    auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(W,S,Settings,SequenceActor);check(Player);
    Player->Play();SeekEnd(Player,S);
    const FTransform Final=OriginalEgg->GetActorTransform();
    check(!OriginalEgg->IsHidden() && !Initial.GetLocation().Equals(Final.GetLocation(),1.f));
    Player->Stop();SequenceActor->Destroy();
    check(OriginalEgg->GetActorTransform().Equals(Initial,.01f));
    auto* MG=Graph(M);auto* DG=Graph(D);
    if(!Verify)
    {
        Backup(M);Backup(D);
        if(!Node(MG,TEXT("VTG_WindowEggStopSequence")))
        {
            auto* Entry=Node(MG,TEXT("K2Node_CustomEvent_25"));auto* Show=Node(MG,TEXT("K2Node_CallFunction_33"));check(Entry && Show);
            auto* Stop=Call(MG,TEXT("VTG_WindowEggStopSequence"),UNarrativeComponent::StaticClass()->FindFunctionByName(TEXT("StopDialogueLevelSequence")),Entry->NodePosX+240,Entry->NodePosY);
            Stop->NodeComment=TEXT("Finish Sequence restore before committing the persistent window egg state");Stop->bCommentBubbleVisible=true;
            auto* Narrative=NewObject<UK2Node_VariableGet>(MG,TEXT("VTG_WindowEggNarrative"),RF_Transactional);
            MG->AddNode(Narrative,false,false);Narrative->CreateNewGuid();Narrative->VariableReference.SetSelfMember(TEXT("Narrative"));Narrative->AllocateDefaultPins();
            Narrative->NodePosX=Stop->NodePosX;Narrative->NodePosY=Stop->NodePosY+180;
            Link(Narrative->FindPinChecked(TEXT("Narrative")),Stop->FindPinChecked(TEXT("self")));
            auto* Transform=Call(MG,TEXT("VTG_WindowEggCommitTransform"),AActor::StaticClass()->FindFunctionByName(TEXT("K2_SetActorTransform")),Stop->NodePosX+400,Stop->NodePosY);
            auto* Make=Call(MG,TEXT("VTG_WindowEggFinalTransform"),UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("MakeTransform")),Transform->NodePosX,Transform->NodePosY+220);
            const FVector P=Final.GetLocation(),Scale=Final.GetScale3D();const FRotator R=Final.Rotator();
            auto* Schema=GetDefault<UEdGraphSchema_K2>();
            Schema->TrySetDefaultValue(*Make->FindPinChecked(TEXT("Location")),FString::Printf(TEXT("%.9f,%.9f,%.9f"),P.X,P.Y,P.Z));
            Schema->TrySetDefaultValue(*Make->FindPinChecked(TEXT("Rotation")),FString::Printf(TEXT("%.9f,%.9f,%.9f"),R.Pitch,R.Yaw,R.Roll));
            Schema->TrySetDefaultValue(*Make->FindPinChecked(TEXT("Scale")),FString::Printf(TEXT("%.9f,%.9f,%.9f"),Scale.X,Scale.Y,Scale.Z));
            Link(Make->FindPinChecked(TEXT("ReturnValue")),Transform->FindPinChecked(TEXT("NewTransform")));
            Transform->FindPinChecked(TEXT("bSweep"))->DefaultValue=TEXT("false");Transform->FindPinChecked(TEXT("bTeleport"))->DefaultValue=TEXT("true");
            Link(Node(MG,TEXT("K2Node_VariableGet_85"))->FindPinChecked(TEXT("EggOnWindow")),Transform->FindPinChecked(TEXT("self")));
            Entry->FindPinChecked(TEXT("then"))->BreakAllPinLinks();Show->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();
            Link(Entry->FindPinChecked(TEXT("then")),Stop->FindPinChecked(TEXT("execute")));
            Link(Stop->FindPinChecked(TEXT("then")),Transform->FindPinChecked(TEXT("execute")));
            Link(Transform->FindPinChecked(TEXT("then")),Show->FindPinChecked(TEXT("execute")));
        }
        Compile(M);
        if(auto* Old=Node(MG,TEXT("K2Node_CallFunction_94")))
        {
            auto* Repeat=Call(MG,TEXT("VTG_WindowEggRestoreRepeat"),M->GeneratedClass->FindFunctionByName(HelperName),Old->NodePosX,Old->NodePosY);
            Replace(MG,Old,Repeat);
        }
        if(!Node(DG,TEXT("VTG_WindowEggCommitAfterSquid")))
        {
            auto* Old=Node(DG,TEXT("K2Node_CallFunction_33"));check(Old);
            auto* Commit=Call(DG,TEXT("VTG_WindowEggCommitAfterSquid"),M->GeneratedClass->FindFunctionByName(HelperName),Old->NodePosX,Old->NodePosY);
            Link(Node(DG,TEXT("K2Node_CallFunction_19"))->FindPinChecked(TEXT("ReturnValue")),Commit->FindPinChecked(TEXT("self")));
            Replace(DG,Old,Commit);
        }
        Compile(M);Compile(D);
    }
    check(Node(MG,TEXT("VTG_WindowEggStopSequence")) && Node(MG,TEXT("VTG_WindowEggCommitTransform")) && Node(MG,TEXT("VTG_WindowEggRestoreRepeat")));
    check(Node(DG,TEXT("VTG_WindowEggCommitAfterSquid")) && !Node(DG,TEXT("K2Node_CallFunction_33")));
    // Exercise the actual Blueprint helper through the same owned Sequence API used by the dialogue.
    auto* RuntimeManager=Manager(W,M);auto* RuntimeEgg=Egg(RuntimeManager);
    auto* Field=FindFProperty<FObjectPropertyBase>(RuntimeManager->GetClass(),TEXT("Narrative"));check(Field);
    UObject* PreviousNarrative=Field->GetObjectPropertyValue_InContainer(RuntimeManager);
    auto* Narrative=NewObject<UNarrativeComponent>(RuntimeManager);RuntimeManager->AddInstanceComponent(Narrative);Narrative->RegisterComponent();
    Field->SetObjectPropertyValue_InContainer(RuntimeManager,Narrative);
    RuntimeEgg->SetActorTransform(Initial);RuntimeEgg->SetActorHiddenInGame(true);
    auto* Owned=Narrative->PlayDialogueLevelSequence(S,false,false,1.f,0,true,false);check(Owned);
    SeekEnd(Owned,S);
    RuntimeManager->ProcessEvent(RuntimeManager->FindFunctionChecked(HelperName),nullptr);
    check(!Narrative->GetDialogueLevelSequencePlayer());CheckState(RuntimeEgg,Final,TEXT("after helper stops restoring Sequence"));
    Narrative->StopDialogueLevelSequence();CheckState(RuntimeEgg,Final,TEXT("after later dialogue cleanup"));
    RuntimeManager->ProcessEvent(RuntimeManager->FindFunctionChecked(HelperName),nullptr);CheckState(RuntimeEgg,Final,TEXT("repeated window interaction"));
    Field->SetObjectPropertyValue_InContainer(RuntimeManager,PreviousNarrative);Narrative->DestroyComponent();
    RuntimeEgg->SetActorTransform(Initial);RuntimeEgg->SetActorHiddenInGame(InitiallyHidden);
    if(!Verify){Save(M);Save(D);}
    UE_LOG(LogTemp,Display,TEXT("APT WINDOW EGG PASS: stop owned Sequence, commit authored final transform, unhide; repeat and later cleanup preserved; only two Blueprints saved"));
    return true;
}
