#include "VTGLevelAudioCommandlet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/AudioComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_SwitchName.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Sound/SoundWave.h"
#include "NarrativeEvent.h"
#include "VTGLevelManagerBase.h"
#include "Audio/VTGPlayerAudioComponent.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace VTGLevelAudio
{
const FString Root = TEXT("/Game/LevelNSequence/LevelAudio/");
const TCHAR* Names[] = {TEXT("Audio_PlayAmbience"),TEXT("Audio_StopAmbience"),TEXT("Audio_PlayMusic"),TEXT("Audio_PlayPsych"),TEXT("Audio_StopMusic"),TEXT("Audio_StopPsych")};
const TCHAR* Tracks[] = {TEXT("AC_Ambience"),TEXT("AC_Ambience"),TEXT("AC_Music"),TEXT("AC_Psych"),TEXT("AC_Music"),TEXT("AC_Psych")};
bool Play(int I) { return I==0 || I==2 || I==3; }
bool Fade(int I) { return I!=3 && I!=5; }
const UEdGraphSchema_K2* S() { return GetDefault<UEdGraphSchema_K2>(); }
UEdGraphPin* P(UEdGraphNode* N,const TCHAR* Name) { auto* Pin=N->FindPin(Name); checkf(Pin,TEXT("Missing %s on %s"),Name,*N->GetName()); return Pin; }
void L(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP) { checkf(S()->TryCreateConnection(P(A,AP),P(B,BP)),TEXT("Link failed %s.%s -> %s.%s"),*A->GetName(),AP,*B->GetName(),BP); }
void D(UEdGraphNode* N,const TCHAR* Pin,const TCHAR* Value) { S()->TrySetDefaultValue(*P(N,Pin),Value); }
template<class T> T* Add(UEdGraph* G,int X,int Y) { auto* N=NewObject<T>(G); G->AddNode(N,false,false); N->CreateNewGuid(); N->NodePosX=X; N->NodePosY=Y; return N; }
FEdGraphPinType Obj(UClass* C) { FEdGraphPinType T; T.PinCategory=UEdGraphSchema_K2::PC_Object; T.PinSubCategoryObject=C; return T; }
FEdGraphPinType Float() { FEdGraphPinType T; T.PinCategory=UEdGraphSchema_K2::PC_Real; T.PinSubCategory=UEdGraphSchema_K2::PC_Float; return T; }
FEdGraphPinType NameType() { FEdGraphPinType T; T.PinCategory=UEdGraphSchema_K2::PC_Name; return T; }
UK2Node_CallFunction* Call(UEdGraph* G,UClass* C,const TCHAR* Name,int X,int Y) { auto* N=Add<UK2Node_CallFunction>(G,X,Y); auto* F=C->FindFunctionByName(Name); checkf(F,TEXT("Missing function %s"),Name); N->SetFromFunction(F); N->AllocateDefaultPins(); return N; }
template<class T=UK2Node_VariableGet> T* Var(UEdGraph* G,const TCHAR* Name,int X,int Y) { auto* N=Add<T>(G,X,Y); N->VariableReference.SetSelfMember(Name); N->AllocateDefaultPins(); return N; }
UK2Node_IfThenElse* Gate(UEdGraph* G,UEdGraphNode* Value,const TCHAR* Pin,int X,int Y) { auto* Valid=Call(G,UKismetSystemLibrary::StaticClass(),TEXT("IsValid"),X-200,Y+160); L(Value,Pin,Valid,TEXT("Object")); auto* B=Add<UK2Node_IfThenElse>(G,X,Y); B->AllocateDefaultPins(); L(Valid,TEXT("ReturnValue"),B,TEXT("Condition")); return B; }
void Compile(UBlueprint* B) { FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B); FCompilerResultsLog Log; FKismetEditorUtilities::CompileBlueprint(B,EBlueprintCompileOptions::SkipGarbageCollection,&Log); checkf(Log.NumErrors==0 && B->Status!=BS_Error,TEXT("Compile failed %s"),*B->GetName()); UE_LOG(LogTemp,Display,TEXT("VTG_AUDIO compile %s errors=%d warnings=%d"),*B->GetName(),Log.NumErrors,Log.NumWarnings); }
void Save(UObject* O) { auto* Package=O->GetOutermost(); Package->MarkPackageDirty(); FSavePackageArgs A; A.TopLevelFlags=RF_Public|RF_Standalone; A.SaveFlags=SAVE_NoError; check(UPackage::SavePackage(Package,O,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),A)); }
void Backup(UBlueprint* B) { FString File=FPackageName::LongPackageNameToFilename(B->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()); FString To=FPaths::ProjectDir()/TEXT("Scripts/LevelAudio/Backup")/(B->GetName()+TEXT(".uasset")); IFileManager::Get().MakeDirectory(*FPaths::GetPath(To),true); if(!IFileManager::Get().FileExists(*To))check(IFileManager::Get().Copy(*To,*File,false)==COPY_OK); }
UBlueprint* NewBP(const TCHAR* Name,UClass* Parent) { FString Path=Root+Name; check(!FPackageName::DoesPackageExist(Path)); auto* B=FKismetEditorUtilities::CreateBlueprint(Parent,CreatePackage(*Path),Name,BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass()); FAssetRegistryModule::AssetCreated(B); return B; }
UK2Node_FunctionEntry* Function(UBlueprint* B,const TCHAR* Name) { check(!B->GeneratedClass->FindFunctionByName(Name)); auto* G=FBlueprintEditorUtils::CreateNewGraph(B,Name,UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass()); FBlueprintEditorUtils::AddFunctionGraph(B,G,false,static_cast<UClass*>(nullptr)); for(UEdGraphNode* N:G->Nodes)if(auto* E=Cast<UK2Node_FunctionEntry>(N)) { E->AddExtraFlags(FUNC_Public|FUNC_BlueprintCallable); E->MetaData.Category=FText::FromString(TEXT("VTG Audio")); return E; } checkNoEntry(); return nullptr; }
void Inputs(UK2Node_FunctionEntry* E,int I) { if(Play(I))E->CreateUserDefinedPin(TEXT("NewSound"),Obj(USoundBase::StaticClass()),EGPD_Output); if(Fade(I)){ E->CreateUserDefinedPin(TEXT("FadeTime"),Float(),EGPD_Output); E->UserDefinedPins.Last()->PinDefaultValue=TEXT("1.0"); } }
void Member(UBlueprint* B,const TCHAR* Name,const FEdGraphPinType& T,const FString& Default=TEXT("")) { check(FBlueprintEditorUtils::AddMemberVariable(B,Name,T,Default)); FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(B,Name,false); FBlueprintEditorUtils::SetBlueprintVariableCategory(B,Name,nullptr,FText::FromString(TEXT("VTG Audio"))); }
UK2Node_Event* Event(UEdGraph* G,UClass* Owner,const TCHAR* Name,int Y=0) { auto* E=Add<UK2Node_Event>(G,0,Y); E->EventReference.SetExternalMember(Name,Owner); E->bOverrideFunction=true; E->AllocateDefaultPins(); return E; }

void BuildAudio(UBlueprint* B)
{
    Backup(B);
    for(const TCHAR* Name:{TEXT("AC_Music"),TEXT("AC_Psych")}) {
        auto* N=B->SimpleConstructionScript->CreateNode(UAudioComponent::StaticClass(),Name);
        B->SimpleConstructionScript->GetRootNodes()[0]->AddChildNode(N);
        auto* A=CastChecked<UAudioComponent>(N->ComponentTemplate); A->bAutoActivate=false; A->bAllowSpatialization=false; A->VolumeMultiplier=Name==FString(TEXT("AC_Music"))?.25f:.4f;
    }
    Compile(B);
    for(int I=0;I<6;++I) {
        auto* E=Function(B,Names[I]); Inputs(E,I); auto* G=E->GetGraph();
        auto* Track=Var(G,Tracks[I],0,300); UEdGraphNode* Exec=E;
        if(Play(I)) { auto* V=Gate(G,E,TEXT("NewSound"),300,0); L(E,TEXT("then"),V,TEXT("execute")); Exec=V;
            auto* Stop=Call(G,UAudioComponent::StaticClass(),TEXT("Stop"),550,0); L(Exec,TEXT("then"),Stop,TEXT("execute")); L(Track,Tracks[I],Stop,TEXT("self"));
            auto* Set=Call(G,UAudioComponent::StaticClass(),TEXT("SetSound"),800,0); L(Stop,TEXT("then"),Set,TEXT("execute")); L(Track,Tracks[I],Set,TEXT("self")); L(E,TEXT("NewSound"),Set,TEXT("NewSound")); Exec=Set;
        }
        const TCHAR* Method=Play(I)?(Fade(I)?TEXT("FadeIn"):TEXT("Play")):(Fade(I)?TEXT("FadeOut"):TEXT("Stop"));
        auto* Action=Call(G,UAudioComponent::StaticClass(),Method,1100,0); L(Exec,TEXT("then"),Action,TEXT("execute")); L(Track,Tracks[I],Action,TEXT("self"));
        if(Fade(I)) { auto* Clamp=Call(G,UKismetMathLibrary::StaticClass(),TEXT("FMax"),700,400); L(E,TEXT("FadeTime"),Clamp,TEXT("A")); D(Clamp,TEXT("B"),TEXT("0")); L(Clamp,TEXT("ReturnValue"),Action,Play(I)?TEXT("FadeInDuration"):TEXT("FadeOutDuration")); D(Action,TEXT("FadeVolumeLevel"),Play(I)?TEXT("1"):TEXT("0")); }
        E->NodeComment=TEXT("Null sound: no change. Replaces current sound on this channel. Fade time is clamped to zero.");
    }
    Compile(B); Save(B);
}

void BuildBPLM(UBlueprint* B,UBlueprint* Audio)
{
    Backup(B); Member(B,TEXT("AudioManagerRef"),Obj(Audio->GeneratedClass)); Compile(B);
    auto* Init=Function(B,TEXT("Audio_Initialize")); auto* G=Init->GetGraph(); auto* Ref=Var(G,TEXT("AudioManagerRef"),0,250);
    auto* Valid=Gate(G,Ref,TEXT("AudioManagerRef"),300,0); L(Init,TEXT("then"),Valid,TEXT("execute"));
    auto* Find=Call(G,UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"),500,250); S()->TrySetDefaultObject(*P(Find,TEXT("ActorClass")),Audio->GeneratedClass);
    auto* CastNode=Add<UK2Node_DynamicCast>(G,800,100); CastNode->TargetType=Audio->GeneratedClass; CastNode->AllocateDefaultPins(); L(Valid,TEXT("else"),Find,TEXT("execute")); L(Find,TEXT("then"),CastNode,TEXT("execute")); L(Find,TEXT("ReturnValue"),CastNode,TEXT("Object"));
    auto* Set=Var<UK2Node_VariableSet>(G,TEXT("AudioManagerRef"),1100,100); check(S()->TryCreateConnection(CastNode->GetCastResultPin(),P(Set,TEXT("AudioManagerRef")))); L(CastNode,TEXT("then"),Set,TEXT("execute"));
    Init->NodeComment=TEXT("Uses the level instance's explicit AudioManagerRef first. Otherwise finds one AudioManager (including child classes). Assign explicitly when multiple managers are loaded.");
    auto* Stage=Function(B,TEXT("Audio_ApplyStage")); Stage->CreateUserDefinedPin(TEXT("Stage"),NameType(),EGPD_Output);
    Stage->NodeComment=TEXT("Override in each level BPLM child. Switch on Name(Stage) -> inherited Audio_Play/Stop functions. Called at startup; call again from existing stage-change flow. No audio state machine."); Stage->bCommentBubbleVisible=true;
    Compile(B);
    for(int I=0;I<6;++I) {
        auto* E=Function(B,Names[I]); Inputs(E,I); auto* Graph=E->GetGraph();
        auto* Resolve=Call(Graph,B->GeneratedClass,TEXT("Audio_Initialize"),250,0); L(E,TEXT("then"),Resolve,TEXT("execute"));
        auto* R=Var(Graph,TEXT("AudioManagerRef"),300,250); auto* V=Gate(Graph,R,TEXT("AudioManagerRef"),600,0); L(Resolve,TEXT("then"),V,TEXT("execute"));
        auto* C=Call(Graph,Audio->GeneratedClass,Names[I],900,0); L(V,TEXT("then"),C,TEXT("execute")); L(R,TEXT("AudioManagerRef"),C,TEXT("self")); if(Play(I))L(E,TEXT("NewSound"),C,TEXT("NewSound")); if(Fade(I))L(E,TEXT("FadeTime"),C,TEXT("FadeTime"));
    }
    Compile(B);
    // Preserve the parent's existing BeginPlay wiring. Initialize audio before it.
    UEdGraphNode* Begin=nullptr; UEdGraphPin* Out=nullptr;
    for(UEdGraph* Graph:B->UbergraphPages)for(UEdGraphNode* N:Graph->Nodes)if(auto* E=Cast<UK2Node_Event>(N))if(E->EventReference.GetMemberName()==TEXT("ReceiveBeginPlay")){ Begin=E; Out=P(E,TEXT("then")); }
    if(!Begin)for(UEdGraph* Graph:B->FunctionGraphs)if(Graph->GetFName()==TEXT("ReceiveBeginPlay"))for(UEdGraphNode* N:Graph->Nodes)if(auto* E=Cast<UK2Node_FunctionEntry>(N)){Begin=E;Out=P(E,TEXT("then"));}
    check(Begin && Out); auto* Graph=Begin->GetGraph(); auto Old=Out->LinkedTo; Out->BreakAllPinLinks();
    auto* Seq=Add<UK2Node_ExecutionSequence>(Graph,Begin->NodePosX+220,Begin->NodePosY-500); Seq->AllocateDefaultPins(); check(S()->TryCreateConnection(Out,P(Seq,TEXT("execute"))));
    auto* Resolve=Call(Graph,B->GeneratedClass,TEXT("Audio_Initialize"),Seq->NodePosX+250,Seq->NodePosY); L(Seq,TEXT("then_0"),Resolve,TEXT("execute"));
    auto* Active=Call(Graph,AVTGLevelManagerBase::StaticClass(),TEXT("GetActiveStage"),Seq->NodePosX+250,Seq->NodePosY+180);
    auto* Apply=Call(Graph,B->GeneratedClass,TEXT("Audio_ApplyStage"),Seq->NodePosX+550,Seq->NodePosY); L(Resolve,TEXT("then"),Apply,TEXT("execute")); L(Active,TEXT("ReturnValue"),Apply,TEXT("Stage"));
    for(auto* Previous:Old)check(S()->TryCreateConnection(P(Seq,TEXT("then_1")),Previous));
    Compile(B); Save(B);
}

void BuildNarrative(UBlueprint* LM)
{
    for(int I=0;I<6;++I) {
        FString Name=TEXT("NE_VTG_")+FString(Names[I]); auto* B=NewBP(*Name,UNarrativeEvent::StaticClass());
        if(Play(I))Member(B,TEXT("NewSound"),Obj(USoundBase::StaticClass())); if(Fade(I))Member(B,TEXT("FadeTime"),Float(),TEXT("1.0")); Compile(B);
        auto* G=FBlueprintEditorUtils::FindEventGraph(B); check(G); auto* E=Event(G,UNarrativeEvent::StaticClass(),TEXT("ExecuteEvent"));
        auto* Find=Call(G,UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"),200,220); S()->TrySetDefaultObject(*P(Find,TEXT("ActorClass")),LM->GeneratedClass); L(E,TEXT("Pawn"),Find,TEXT("WorldContextObject"));
        auto* CastNode=Add<UK2Node_DynamicCast>(G,500,0); CastNode->TargetType=LM->GeneratedClass; CastNode->AllocateDefaultPins(); L(E,TEXT("then"),Find,TEXT("execute")); L(Find,TEXT("then"),CastNode,TEXT("execute")); L(Find,TEXT("ReturnValue"),CastNode,TEXT("Object"));
        auto* C=Call(G,LM->GeneratedClass,Names[I],800,0); L(CastNode,TEXT("then"),C,TEXT("execute")); check(S()->TryCreateConnection(CastNode->GetCastResultPin(),P(C,TEXT("self"))));
        if(Play(I)){auto* V=Var(G,TEXT("NewSound"),500,220); L(V,TEXT("NewSound"),C,TEXT("NewSound"));} if(Fade(I)){auto* V=Var(G,TEXT("FadeTime"),500,350); L(V,TEXT("FadeTime"),C,TEXT("FadeTime"));}
        Compile(B); CastChecked<UNarrativeEvent>(B->GeneratedClass->GetDefaultObject())->bRefireOnLoad=I!=3; Save(B);
    }
}

void FixFind(UBlueprint* B)
{
    TArray<UEdGraph*> Graphs; B->GetAllGraphs(Graphs);
    for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
        if(C->FunctionReference.GetMemberName()==TEXT("GetActorOfClass") && P(C,TEXT("execute"))->LinkedTo.IsEmpty()) {
            auto* Consumer=P(C,TEXT("ReturnValue"))->LinkedTo[0]->GetOwningNode(); auto* Exec=P(Consumer,TEXT("execute")); auto Old=Exec->LinkedTo; Exec->BreakAllPinLinks();
            for(auto* Pin:Old)check(S()->TryCreateConnection(Pin,P(C,TEXT("execute")))); L(C,TEXT("then"),Consumer,TEXT("execute"));
        }
    Compile(B); Save(B);
}

void Demo(UBlueprint* Audio,UBlueprint* LM)
{
    auto* ChildAudio=NewBP(TEXT("BP_VTG_AudioManager_Example"),Audio->GeneratedClass); Compile(ChildAudio); Save(ChildAudio);
    auto* B=NewBP(TEXT("BPLM_AudioExample"),LM->GeneratedClass);
    Member(B,TEXT("ExampleAmbience"),Obj(USoundBase::StaticClass())); Member(B,TEXT("ExampleMusic"),Obj(USoundBase::StaticClass())); Member(B,TEXT("ExamplePsych"),Obj(USoundBase::StaticClass())); Compile(B);
    auto* G=FBlueprintEditorUtils::CreateNewGraph(B,TEXT("Audio_ApplyStage"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph(B,G,false,LM->GeneratedClass.Get());
    UK2Node_FunctionEntry* E=nullptr; for(UEdGraphNode* N:G->Nodes)if(auto* Entry=Cast<UK2Node_FunctionEntry>(N))E=Entry; check(E);
    auto* Switch=Add<UK2Node_SwitchName>(G,300,0); Switch->PinNames={TEXT("Demo_Normal"),TEXT("Demo_Disturbed"),TEXT("Demo_Void")}; Switch->AllocateDefaultPins(); L(E,TEXT("then"),Switch,TEXT("execute")); L(E,TEXT("Stage"),Switch,TEXT("Selection"));
    auto Action=[&](int I,int X,int Y,const TCHAR* Variable)->UK2Node_CallFunction* { auto* C=Call(G,LM->GeneratedClass,Names[I],X,Y); if(Variable){auto* V=Var(G,Variable,X,Y+160); L(V,Variable,C,TEXT("NewSound"));} if(Fade(I))D(C,TEXT("FadeTime"),TEXT("1.5")); return C; };
    auto* Amb=Action(0,650,0,TEXT("ExampleAmbience")); auto* Music=Action(2,1000,0,TEXT("ExampleMusic")); L(Switch,TEXT("Demo_Normal"),Amb,TEXT("execute")); L(Amb,TEXT("then"),Music,TEXT("execute"));
    auto* Quiet=Action(1,650,400,nullptr); auto* Psych=Action(3,1000,400,TEXT("ExamplePsych")); L(Switch,TEXT("Demo_Disturbed"),Quiet,TEXT("execute")); L(Quiet,TEXT("then"),Psych,TEXT("execute"));
    auto* StopA=Action(1,650,800,nullptr); auto* StopM=Action(4,1000,800,nullptr); auto* StopP=Action(5,1350,800,nullptr); L(Switch,TEXT("Demo_Void"),StopA,TEXT("execute")); L(StopA,TEXT("then"),StopM,TEXT("execute")); L(StopM,TEXT("then"),StopP,TEXT("execute"));
    G=FBlueprintEditorUtils::FindEventGraph(B); auto* Start=Event(G,AActor::StaticClass(),TEXT("ReceiveBeginPlay"));
    Start->NodeComment=TEXT("Isolated demonstration only: overrides BeginPlay without running the story-specific BPLM parent graph. Production children must retain Parent: BeginPlay."); Start->bCommentBubbleVisible=true;
    UEdGraphNode* Last=Start; int X=300;
    for(const TCHAR* StageName:{TEXT("Demo_Normal"),TEXT("Demo_Disturbed"),TEXT("Demo_Void")}) {
        if(X>300){auto* Delay=Call(G,UKismetSystemLibrary::StaticClass(),TEXT("Delay"),X,0); D(Delay,TEXT("Duration"),TEXT("6")); L(Last,TEXT("then"),Delay,TEXT("execute")); Last=Delay; X+=300;}
        auto* Apply=Call(G,LM->GeneratedClass,TEXT("Audio_ApplyStage"),X,0); D(Apply,TEXT("Stage"),StageName); L(Last,TEXT("then"),Apply,TEXT("execute")); Last=Apply; X+=300;
        auto* Print=Call(G,UKismetSystemLibrary::StaticClass(),TEXT("PrintString"),X,0); D(Print,TEXT("InString"),StageName); D(Print,TEXT("Duration"),TEXT("5")); L(Last,TEXT("then"),Print,TEXT("execute")); Last=Print; X+=300;
    }
    Compile(B); Save(B);
}

void OneShotUpgrade(UBlueprint* Audio,UBlueprint* LM)
{
    // Keep AC_Psych and old functions intact: existing placed children and Narrative assets retain compatibility.
    auto AddInputs=[](UK2Node_FunctionEntry* E) {
        E->CreateUserDefinedPin(TEXT("NewSound"),Obj(USoundBase::StaticClass()),EGPD_Output);
        E->CreateUserDefinedPin(TEXT("FadeInTime"),Float(),EGPD_Output);
        E->CreateUserDefinedPin(TEXT("VolumeMultiplier"),Float(),EGPD_Output);
        E->UserDefinedPins.Last()->PinDefaultValue=TEXT("1.0");
    };
    Backup(Audio); Backup(LM);
    auto* E=Function(Audio,TEXT("Audio_PlayOneShot")); AddInputs(E); auto* G=E->GetGraph();
    auto* V=Gate(G,E,TEXT("NewSound"),300,0); L(E,TEXT("then"),V,TEXT("execute"));
    auto* Track=Var(G,TEXT("AC_Psych"),50,250);
    auto* Stop=Call(G,UAudioComponent::StaticClass(),TEXT("Stop"),550,0); L(V,TEXT("then"),Stop,TEXT("execute")); L(Track,TEXT("AC_Psych"),Stop,TEXT("self"));
    auto* Set=Call(G,UAudioComponent::StaticClass(),TEXT("SetSound"),800,0); L(Stop,TEXT("then"),Set,TEXT("execute")); L(Track,TEXT("AC_Psych"),Set,TEXT("self")); L(E,TEXT("NewSound"),Set,TEXT("NewSound"));
    auto* PlayNode=Call(G,UAudioComponent::StaticClass(),TEXT("FadeIn"),1100,0); L(Set,TEXT("then"),PlayNode,TEXT("execute")); L(Track,TEXT("AC_Psych"),PlayNode,TEXT("self"));
    for(const TCHAR* Input:{TEXT("FadeInTime"),TEXT("VolumeMultiplier")}) {
        auto* Clamp=Call(G,UKismetMathLibrary::StaticClass(),TEXT("FMax"),750,Input==FString(TEXT("FadeInTime"))?250:420);
        L(E,Input,Clamp,TEXT("A")); D(Clamp,TEXT("B"),TEXT("0")); L(Clamp,TEXT("ReturnValue"),PlayNode,Input==FString(TEXT("FadeInTime"))?TEXT("FadeInDuration"):TEXT("FadeVolumeLevel"));
    }
    E->NodeComment=TEXT("Generic one-shot channel. Use non-looping source audio. Reuses AC_Psych for backward compatibility; new play replaces the previous one-shot. Volume is relative to the component volume."); E->bCommentBubbleVisible=true;
    auto* SE=Function(Audio,TEXT("Audio_StopOneShot")); auto* StopAlias=Call(SE->GetGraph(),Audio->GeneratedClass,TEXT("Audio_StopPsych"),300,0); L(SE,TEXT("then"),StopAlias,TEXT("execute"));
    Compile(Audio); Save(Audio);
    for(bool bPlay:{true,false}) {
        const TCHAR* Fn=bPlay?TEXT("Audio_PlayOneShot"):TEXT("Audio_StopOneShot"); auto* Entry=Function(LM,Fn); if(bPlay)AddInputs(Entry); G=Entry->GetGraph();
        auto* Resolve=Call(G,LM->GeneratedClass,TEXT("Audio_Initialize"),250,0); L(Entry,TEXT("then"),Resolve,TEXT("execute"));
        auto* Ref=Var(G,TEXT("AudioManagerRef"),300,250); auto* Valid=Gate(G,Ref,TEXT("AudioManagerRef"),600,0); L(Resolve,TEXT("then"),Valid,TEXT("execute"));
        auto* Invoke=Call(G,Audio->GeneratedClass,Fn,900,0); L(Valid,TEXT("then"),Invoke,TEXT("execute")); L(Ref,TEXT("AudioManagerRef"),Invoke,TEXT("self"));
        if(bPlay)for(const TCHAR* Pin:{TEXT("NewSound"),TEXT("FadeInTime"),TEXT("VolumeMultiplier")})L(Entry,Pin,Invoke,Pin);
    }
    Compile(LM); Save(LM);
    for(bool bPlay:{true,false}) {
        const TCHAR* Fn=bPlay?TEXT("Audio_PlayOneShot"):TEXT("Audio_StopOneShot");
        FString Name=TEXT("NE_VTG_")+FString(Fn), Path=Root+TEXT("NE/")+Name; check(!FPackageName::DoesPackageExist(Path));
        auto* B=FKismetEditorUtilities::CreateBlueprint(UNarrativeEvent::StaticClass(),CreatePackage(*Path),*Name,BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass()); FAssetRegistryModule::AssetCreated(B);
        if(bPlay){Member(B,TEXT("NewSound"),Obj(USoundBase::StaticClass()));Member(B,TEXT("FadeInTime"),Float(),TEXT("0"));Member(B,TEXT("VolumeMultiplier"),Float(),TEXT("1"));}
        Compile(B); G=FBlueprintEditorUtils::FindEventGraph(B); auto* Start=Event(G,UNarrativeEvent::StaticClass(),TEXT("ExecuteEvent"));
        auto* Find=Call(G,UGameplayStatics::StaticClass(),TEXT("GetActorOfClass"),250,0); S()->TrySetDefaultObject(*P(Find,TEXT("ActorClass")),LM->GeneratedClass);L(Start,TEXT("Pawn"),Find,TEXT("WorldContextObject"));L(Start,TEXT("then"),Find,TEXT("execute"));
        auto* CastNode=Add<UK2Node_DynamicCast>(G,550,0); CastNode->TargetType=LM->GeneratedClass;CastNode->AllocateDefaultPins();L(Find,TEXT("then"),CastNode,TEXT("execute"));L(Find,TEXT("ReturnValue"),CastNode,TEXT("Object"));
        auto* Invoke=Call(G,LM->GeneratedClass,Fn,900,0);L(CastNode,TEXT("then"),Invoke,TEXT("execute"));check(S()->TryCreateConnection(CastNode->GetCastResultPin(),P(Invoke,TEXT("self"))));
        if(bPlay){int Y=220;for(const TCHAR* Pin:{TEXT("NewSound"),TEXT("FadeInTime"),TEXT("VolumeMultiplier")}){auto* Value=Var(G,Pin,600,Y);Y+=130;L(Value,Pin,Invoke,Pin);}}
        Compile(B);CastChecked<UNarrativeEvent>(B->GeneratedClass->GetDefaultObject())->bRefireOnLoad=false;Save(B);
    }
}
}

extern bool VTGVerifyLevelAudio();
UVTGLevelAudioCommandlet::UVTGLevelAudioCommandlet(){ IsEditor=true; IsClient=false; IsServer=false; LogToConsole=true; }
int32 UVTGLevelAudioCommandlet::Main(const FString& Params)
{
    using namespace VTGLevelAudio;
    if(Params.Contains(TEXT("DetachSewer")) || Params.Contains(TEXT("SewerDetachedCheck"))) {
        const bool CheckOnly=Params.Contains(TEXT("SewerDetachedCheck"));
        auto* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa"));check(Player);
        if(!CheckOnly)Backup(Player); bool Found=false;
        for(auto* Node:Player->SimpleConstructionScript->GetAllNodes())if(auto* C=Cast<UVTGPlayerAudioComponent>(Node->ComponentTemplate)) {
            if(CheckOnly){
                check(C->SewerLevel.IsNone() && !C->WaterBed && !C->PipeBed && !C->PipeHiss && !C->AmbientAttenuation);
                check(!C->PipeReverb && !C->CorridorReverb && !C->ChamberReverb && C->WaterVolume==0 && C->PipeVolume==0);
                check(C->DefaultSteps.Num()>0 && C->ArmorRunStartSounds.Num()>0);Found=true;continue;
            }
            C->SewerLevel=NAME_None; C->WaterBed=nullptr;C->PipeBed=nullptr;C->PipeHiss=nullptr;C->AmbientAttenuation=nullptr;
            C->PipeReverb=nullptr;C->CorridorReverb=nullptr;C->ChamberReverb=nullptr;
            C->WaterVolume=0;C->PipeVolume=0;C->PipeWetLevel=0;C->CorridorWetLevel=0;C->ChamberWetLevel=0;
            check(C->DefaultSteps.Num()>0);Found=true;
        }
        check(Found);Compile(Player);if(!CheckOnly)Save(Player);UE_LOG(LogTemp,Display,TEXT("VTG_AUDIO DETACHED sewer assets from PlayerAudio; footsteps/armor retained; reloaded verification=%d"),CheckOnly);return 0;
    }
    if(Params.Contains(TEXT("Verify"))) return VTGVerifyLevelAudio()?0:1;
    auto* Audio=LoadObject<UBlueprint>(nullptr,*(Root+TEXT("BP_VTG_AudioManager"))); check(Audio);
    if(Params.Contains(TEXT("Phase2"))) { BuildAudio(Audio); return 0; }
    auto* LM=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/BPLM")); check(LM);
    if(Params.Contains(TEXT("OneShotUpgrade"))) { OneShotUpgrade(Audio,LM); return 0; }
    if(Params.Contains(TEXT("Phase3"))) { BuildBPLM(LM,Audio); BuildNarrative(LM); return 0; }
    if(Params.Contains(TEXT("FixFind"))) { FixFind(LM); for(int I=0;I<6;++I)FixFind(LoadObject<UBlueprint>(nullptr,*(Root+TEXT("NE_VTG_")+Names[I]))); return 0; }
    if(Params.Contains(TEXT("Demo"))) { Demo(Audio,LM); return 0; }
    return 1;
}
