#include "VTGDialogueBlipCommandlet.h"
#include "NarrativeBlipLibrary.h"
#include "NarrativeComponent.h"
#include "Dialogue.h"
#include "DialogueBlueprint.h"
#include "DialogueBlueprintGeneratedClass.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Sound/SoundWave.h"
#include "K2Node_CallFunction.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace BlipEditor
{
const TCHAR* LibraryPath = TEXT("/Game/Narrative/Audio/DA_DialogueBlipLibrary");
void Backup(UObject* Object)
{
    const auto Package = Object->GetOutermost()->GetName();
    const auto File = FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension());
    const auto Target = FPaths::ProjectDir()/TEXT("Scripts/DialogueBlip/Backup")/(Package.RightChop(1)+TEXT(".uasset"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target),true);
    if (IFileManager::Get().FileExists(*File) && !IFileManager::Get().FileExists(*Target)) check(IFileManager::Get().Copy(*Target,*File)==COPY_OK);
}
void Save(UObject* Object)
{
    auto* Package=Object->GetOutermost(); Package->MarkPackageDirty();
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
    check(UPackage::SavePackage(Package,Object,*FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension()),Args));
}
bool IsSa(const FSpeakerInfo& Speaker)
{
    // Migration only. Runtime routing never examines character identities or actor tags.
    return Speaker.SpeakerID==TEXT("Sa") || Speaker.SpeakingActorTag==TEXT("SaP") || Speaker.SpeakingActorTag==TEXT("SaC");
}
void AudioManager(bool Verify)
{
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/LevelNSequence/LevelAudio/BP_VTG_AudioManager"));check(BP);
    if(!Verify) Backup(BP);
    int32 Count=0;
    for(UEdGraph* Graph:BP->FunctionGraphs)
    {
        if(Graph->GetFName()!=TEXT("Audio_PlayOneShot") && Graph->GetFName()!=TEXT("Audio_PlayPsych"))continue;
        ++Count;
        UK2Node_CallFunction* Play=nullptr;UK2Node_CallFunction* Register=nullptr;
        for(UEdGraphNode* Node:Graph->Nodes)if(auto* Call=Cast<UK2Node_CallFunction>(Node))
        {
            if(Call->FunctionReference.GetMemberName()==TEXT("FadeIn") || Call->FunctionReference.GetMemberName()==TEXT("Play"))Play=Call;
            if(Call->FunctionReference.GetMemberName()==TEXT("RegisterPriorityAudio"))Register=Call;
        }
        check(Play);
        if(!Register)
        {
            check(!Verify);Register=NewObject<UK2Node_CallFunction>(Graph);Graph->AddNode(Register,false,false);Register->CreateNewGuid();
            Register->SetFromFunction(UNarrativeBlipAudio::StaticClass()->FindFunctionByName(TEXT("RegisterPriorityAudio")));Register->AllocateDefaultPins();
            Register->NodePosX=Play->NodePosX+300;Register->NodePosY=Play->NodePosY;
            auto Link=[](UEdGraphPin* A,UEdGraphPin* B){check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B));};
            auto* Out=Play->FindPinChecked(TEXT("then"));auto Old=Out->LinkedTo;Out->BreakAllPinLinks();
            for(auto* P:Old)Link(Register->FindPinChecked(TEXT("then")),P);
            Link(Out,Register->FindPinChecked(TEXT("execute")));
            auto* Self=Play->FindPinChecked(TEXT("self"));check(Self->LinkedTo.Num()==1);
            Link(Self->LinkedTo[0],Register->FindPinChecked(TEXT("Audio")));
        }
        check(Play->FindPinChecked(TEXT("then"))->LinkedTo.Contains(Register->FindPinChecked(TEXT("execute"))));
    }
    check(Count==2);
    FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&Log);check(!Log.NumErrors);
    if(!Verify)Save(BP);
    UE_LOG(LogTemp,Display,TEXT("BLIP PASS: event OneShot / Psych channels registered; music / ambience untouched"));
}
void SpeakerProfiles(bool Verify, bool Inspect)
{
    auto* Library=LoadObject<UNarrativeBlipLibrary>(nullptr,LibraryPath);check(Library);
    auto ProfileBackup=[](UObject* Object)
    {
        const FString Package=Object->GetOutermost()->GetName();
        const FString File=FPackageName::LongPackageNameToFilename(Package,FPackageName::GetAssetPackageExtension());
        const FString Target=FPaths::ProjectDir()/TEXT("Scripts/BlipSpeakerProfiles/Backup")/(Package.RightChop(1)+TEXT(".uasset"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target),true);
        if(!IFileManager::Get().FileExists(*Target))check(IFileManager::Get().Copy(*Target,*File)==COPY_OK);
    };
    if(!Verify && !Inspect)ProfileBackup(Library);
    for(const TCHAR* PoolName:{TEXT("IPHAP"),TEXT("Mono"),TEXT("Sa")})
    {
        const FString Prefix=FString(PoolName)==TEXT("IPHAP")?TEXT("IPHA"):PoolName;
        TArray<TObjectPtr<USoundBase>> Sources;
        for(int32 I=1;I<=3;++I)
        {
            const FString Path=FString::Printf(TEXT("/Game/SFX/Blip/%s/BlipSFX_%s%d"),PoolName,*Prefix,I);
            auto* Wave=LoadObject<USoundWave>(nullptr,*Path);check(Wave && !Wave->IsLooping());Sources.Add(Wave);
            UE_LOG(LogTemp,Display,TEXT("BLIP PROFILE SOURCE %s duration=%.3f"),*Path,Wave->GetDuration());
        }
        if(Inspect)continue;
        if(!Verify)Library->BlipPools.FindOrAdd(PoolName).Sources=Sources;
        const auto* Pool=Library->BlipPools.Find(PoolName);check(Pool && Pool->Sources==Sources);
    }
    if(!Verify && !Inspect)Save(Library);
    const TMap<FName,FName> Tags={
        {TEXT("ID_Thought"),TEXT("Mono")},{TEXT("ID_Mail"),TEXT("Mono")},{TEXT("ID_Monologue"),TEXT("Mono")},
        {TEXT("IPHA-Drone"),TEXT("IPHAP")},{TEXT("IPHA-Drone???"),TEXT("IPHAP")},{TEXT("IPHA-Projection"),TEXT("IPHAP")}};
    const TMap<FName,ENarrativeBlipVoicePitch> Pitches={
        {TEXT("Bello"),ENarrativeBlipVoicePitch::High},{TEXT("May"),ENarrativeBlipVoicePitch::High},{TEXT("Fell"),ENarrativeBlipVoicePitch::Low}};
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();Registry.SearchAllAssets(true);
    FARFilter Filter;Filter.PackagePaths.Add(TEXT("/Game/Narrative/Dialogues/Levels"));Filter.bRecursivePaths=true;
    Filter.ClassPaths.Add(UDialogueBlueprint::StaticClass()->GetClassPathName());Filter.bRecursiveClasses=true;
    TArray<FAssetData> Assets;Registry.GetAssets(Filter,Assets);
    TMap<FName,int32> Matches;int32 ChangedAssets=0,Total=0;
    for(const auto& Asset:Assets)
    {
        auto* BP=Cast<UDialogueBlueprint>(Asset.GetAsset());if(!BP || !BP->GeneratedClass)continue;++Total;
        auto* Default=CastChecked<UDialogue>(BP->GeneratedClass->GetDefaultObject());
        TArray<UDialogue*> Configs={Default,BP->DialogueTemplate};
        if(auto* Class=Cast<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass))Configs.AddUnique(Class->GetDialogueTemplate());
        bool Changed=false;
        for(auto* Config:Configs)if(Config)
        {
            auto Configure=[&](FSpeakerInfo& Speaker)
            {
                const auto* Tag=Tags.Find(Speaker.SpeakerID);const auto* Pitch=Pitches.Find(Speaker.SpeakerID);
                if(!Tag && !Pitch)return;
                if(Config==Default)
                {
                    ++Matches.FindOrAdd(Speaker.SpeakerID);
                    UE_LOG(LogTemp,Display,TEXT("BLIP PROFILE %s speaker=%s tag=%s pitch=%d"),*BP->GetPathName(),*Speaker.SpeakerID.ToString(),*Speaker.BlipTag.ToString(),int32(Speaker.BlipVoicePitch));
                }
                if(Inspect)return;
                const bool NeedsChange=(Tag && Speaker.BlipTag!=*Tag)||(Pitch && Speaker.BlipVoicePitch!=*Pitch);
                if(!Verify && NeedsChange)
                {
                    if(!Changed)ProfileBackup(BP);Changed=true;
                    if(Tag)Speaker.BlipTag=*Tag;if(Pitch)Speaker.BlipVoicePitch=*Pitch;
                }
                check(!Tag || Speaker.BlipTag==*Tag);check(!Pitch || Speaker.BlipVoicePitch==*Pitch);
            };
            for(auto& Speaker:Config->Speakers)Configure(Speaker);
            Configure(Config->PlayerSpeakerInfo);
            for(auto& Speaker:Config->PartySpeakerInfo)Configure(Speaker);
        }
        if(Changed){FBlueprintEditorUtils::MarkBlueprintAsModified(BP);Save(BP);++ChangedAssets;}
    }
    for(const auto& Entry:Tags)UE_LOG(LogTemp,Display,TEXT("BLIP PROFILE MATCH %s = %d"),*Entry.Key.ToString(),Matches.FindRef(Entry.Key));
    for(const auto& Entry:Pitches)UE_LOG(LogTemp,Display,TEXT("BLIP PROFILE MATCH %s = %d"),*Entry.Key.ToString(),Matches.FindRef(Entry.Key));
    UE_LOG(LogTemp,Display,TEXT("BLIP PROFILES %s: %d assets inspected, %d assets updated"),Inspect?TEXT("INSPECT"):TEXT("PASS"),Total,ChangedAssets);
}
void Test()
{
    int32 Checks=0;
    auto Expect=[&](bool OK,const TCHAR* Message){++Checks;UE_LOG(LogTemp,Display,TEXT("BLIP %s: %s"),OK?TEXT("PASS"):TEXT("FAIL"),Message);check(OK);};
    Expect(FSpeakerInfo().BlipVoicePitch==ENarrativeBlipVoicePitch::Default,TEXT("unset speaker voice pitch defaults to unchanged"));
    FNarrativeBlipChoice PitchChoice;PitchChoice.Route=ENarrativeBlipRoute::Blip;PitchChoice.Pitch=.97f;
    Expect(UNarrativeBlipAudio::GetPlaybackPitch(PitchChoice,ENarrativeBlipVoicePitch::Default)==.97f,TEXT("Default preserves pool random pitch exactly"));
    const float High=UNarrativeBlipAudio::GetPlaybackPitch(PitchChoice,ENarrativeBlipVoicePitch::High);
    const float Low=UNarrativeBlipAudio::GetPlaybackPitch(PitchChoice,ENarrativeBlipVoicePitch::Low);
    Expect(FMath::IsNearlyEqual(High,.97f*1.1892071f,.00001f) && FMath::IsNearlyEqual(Low,.97f*.8408964f,.00001f),TEXT("High/Low multiply random pitch by symmetric +/-3 semitones"));
    PitchChoice.Route=ENarrativeBlipRoute::VoiceOverride;
    Expect(UNarrativeBlipAudio::GetPlaybackPitch(PitchChoice,ENarrativeBlipVoicePitch::High)==.97f && UNarrativeBlipAudio::GetPlaybackPitch(PitchChoice,ENarrativeBlipVoicePitch::Low)==.97f,TEXT("explicit voice override is never pitch shifted"));
    auto* Library=NewObject<UNarrativeBlipLibrary>();Library->BlipPools.Add(TEXT("BlipNPC"));Library->BlipPools.Add(TEXT("Sa"));
    Expect(Library->ResolveTag(TEXT("Sa"))==TEXT("Sa"),TEXT("Sa pool resolves"));
    Expect(Library->ResolveTag(NAME_None)==TEXT("BlipNPC") && Library->ResolveTag(TEXT("Unknown"))==TEXT("BlipNPC"),TEXT("missing / unknown tag fallback"));
    for(const TCHAR* Text:{TEXT(""),TEXT("   "),TEXT("..."),TEXT("!?"),TEXT("……？！"),TEXT("<b>...</>")})
        Expect(!UNarrativeBlipAudio::HasSpokenText(FText::FromString(Text)),TEXT("empty / punctuation / markup is silent"));
    for(const TCHAR* Text:{TEXT("Hello."),TEXT("What...?"),TEXT("你好。"),TEXT("<b>你好</>"),TEXT("１２３")})
        Expect(UNarrativeBlipAudio::HasSpokenText(FText::FromString(Text)),TEXT("English / Chinese / fullwidth / rich text eligible"));
    FText Text=FText::FromString(TEXT("Hello"));
    auto Choose=[&](FName Tag,USoundBase* Voice=nullptr,bool Enabled=true,bool Suppressed=false,USoundBase* Previous=nullptr){return Library->Choose(Tag,Text,Voice,Enabled,Suppressed,Previous);};
    Expect(Choose(TEXT("Sa")).Route==ENarrativeBlipRoute::Silence,TEXT("empty pool safe"));
    // Transient metadata-only objects: no waveform generated, imported, saved or assigned to content.
    auto* A=NewObject<USoundWave>();auto* B=NewObject<USoundWave>();auto* Loop=NewObject<USoundWave>();Loop->bLooping=true;
    auto& NPC=Library->BlipPools.FindChecked(TEXT("BlipNPC"));NPC.Sources={nullptr,A,A,B,Loop};
    Expect(Choose(TEXT("Sa")).Route==ENarrativeBlipRoute::Silence,TEXT("existing empty pool does not fall back"));
    Expect(Choose(TEXT("Unknown")).Route==ENarrativeBlipRoute::Blip,TEXT("unknown pool falls back to valid NPC"));
    Expect(Choose(NAME_None,nullptr,false).Route==ENarrativeBlipRoute::Silence && Choose(NAME_None,nullptr,true,true).Route==ENarrativeBlipRoute::Silence,TEXT("switch / fast-forward / priority suppress selection"));
    Text=FText::FromString(TEXT("..."));
    Expect(Choose(NAME_None,A,false,true).Route==ENarrativeBlipRoute::VoiceOverride,TEXT("explicit voice wins even for punctuation, disabled blips and suppression"));
    Text=FText::FromString(TEXT("Hello"));NPC.PitchMin=1.1f;NPC.PitchMax=.9f;
    for(int32 I=0;I<20;++I){auto Result=Choose(NAME_None,nullptr,true,false,A);check(Result.Sound==B && Result.Pitch>=.9f && Result.Pitch<=1.1f);}
    Expect(true,TEXT("null / looping / duplicate sources excluded, no immediate repeat, reversed ranges safe"));
    NPC.Sources={A};Expect(Choose(NAME_None,nullptr,true,false,A).Sound==A,TEXT("single-source no-repeat terminates"));
    auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("BlipTests"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Actor=World->SpawnActor<AActor>();auto* Component=NewObject<UNarrativeComponent>(Actor);Component->RegisterComponent();
    auto* Dialogue=NewObject<UDialogue>(World);Dialogue->OwningComp=Component;Dialogue->BlipLibrary=NewObject<UNarrativeBlipLibrary>();
    auto* Node=NewObject<UDialogueNode_NPC>(Dialogue);FDialogueLine Line;Line.Text=Text;FSpeakerInfo Speaker;
    Line.Duration=ELineDuration::LD_WhenAudioEnds;auto Original=Line.Duration;
    Dialogue->StartBlip(Node,Line,Speaker);Expect(!Dialogue->BlipAudio && Line.Duration==Original,TEXT("empty playback never alters audio-end timing"));
    auto Attach=[&](){auto* Audio=NewObject<UAudioComponent>(Actor);Audio->bAutoActivate=false;Audio->RegisterComponent();Dialogue->BlipAudio=Audio;return Audio;};
    Attach();Dialogue->SetBlipFastForward(true);Expect(!Dialogue->BlipAudio,TEXT("entering fast-forward immediately clears blip"));Dialogue->SetBlipFastForward(false);Expect(!Dialogue->BlipAudio,TEXT("leaving fast-forward never catches up"));
    Attach();Line.Text=FText::FromString(TEXT("..."));Dialogue->StartBlip(Node,Line,Speaker);Expect(!Dialogue->BlipAudio,TEXT("silent next line clears previous blip"));
    Attach();auto* Priority=NewObject<UAudioComponent>(Actor);Priority->bAutoActivate=false;Priority->RegisterComponent();
    UNarrativeBlipAudio::RegisterPriorityAudio(Priority);Expect(!Dialogue->BlipAudio,TEXT("priority audio interrupts current blip"));
    Priority->SetPaused(true);Expect(UNarrativeBlipAudio::HasPriorityAudio(World),TEXT("paused priority source still suppresses new blips"));Priority->SetPaused(false);
    Expect(!UNarrativeBlipAudio::HasPriorityAudio(World),TEXT("stopped priority source no longer suppresses"));
    Attach();Dialogue->SetDialogueAudioPaused(true);Expect(Dialogue->BlipAudio && Dialogue->BlipAudio->bIsPaused,TEXT("pause preserves current blip state"));Dialogue->SetDialogueAudioPaused(false);
    auto* Natural=Dialogue->BlipAudio.Get();
    auto* Finish=Dialogue->FindFunctionChecked(TEXT("FinishDialogueNode"));FStructOnScope Args(Finish);
    FindFProperty<FObjectPropertyBase>(Finish,TEXT("Node"))->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Node);
    *FindFProperty<FStructProperty>(Finish,TEXT("Line"))->ContainerPtrToValuePtr<FDialogueLine>(Args.GetStructMemory())=Line;
    *FindFProperty<FStructProperty>(Finish,TEXT("Speaker"))->ContainerPtrToValuePtr<FSpeakerInfo>(Args.GetStructMemory())=Speaker;
    Dialogue->ProcessEvent(Finish,Args.GetStructMemory());
    Expect(Dialogue->BlipAudio==Natural,TEXT("normal line finish allows short blip tail"));
    Node->bEnableBlip=false;Dialogue->StartBlip(Node,Line,Speaker);Expect(!Dialogue->BlipAudio,TEXT("disabled next node still clears old audio"));Node->bEnableBlip=true;
    Attach();Line.DialogueSound=A;Dialogue->StartBlip(Node,Line,Speaker);Expect(!Dialogue->BlipAudio,TEXT("explicit line audio clears old blip without starting another"));
    Attach();Dialogue->Deinitialize();Expect(!Dialogue->BlipAudio,TEXT("dialogue teardown clears playback"));
    GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
    UE_LOG(LogTemp,Display,TEXT("BLIP TESTS: %d checks passed"),Checks);
}
}

int32 UVTGDialogueBlipCommandlet::Main(const FString& Params)
{
    using namespace BlipEditor;
    if(FParse::Param(*Params,TEXT("SpeakerProfiles")))
    {
        SpeakerProfiles(FParse::Param(*Params,TEXT("VerifyOnly")),FParse::Param(*Params,TEXT("Inspect")));
        return 0;
    }
    // Runtime regression tests must not impose the one-time migration's old speaker/tag assignments.
    if (FParse::Param(*Params,TEXT("TestOnly"))) { Test(); return 0; }
    const bool Verify=FParse::Param(*Params,TEXT("VerifyOnly"));
    auto* Library=LoadObject<UNarrativeBlipLibrary>(nullptr,LibraryPath,nullptr,LOAD_NoWarn|LOAD_Quiet);
    if(!Library)
    {
        check(!Verify);Library=NewObject<UNarrativeBlipLibrary>(CreatePackage(LibraryPath),TEXT("DA_DialogueBlipLibrary"),RF_Public|RF_Standalone);
        Library->BlipPools.Add(TEXT("BlipNPC"));auto& Sa=Library->BlipPools.Add(TEXT("Sa"));Sa.PitchMin=.98f;Sa.PitchMax=1.02f;Sa.VolumeMin=.93f;
        FAssetRegistryModule::AssetCreated(Library);Save(Library);
    }
    const bool InstallNPC=FParse::Param(*Params,TEXT("InstallNPC"));
    if(!Verify && Library->BlipPools.Contains(TEXT("NPC")))
    {
        Backup(Library);
        check(!Library->BlipPools.Contains(TEXT("BlipNPC")));
        Library->BlipPools.Add(TEXT("BlipNPC"),Library->BlipPools.FindChecked(TEXT("NPC")));
        Library->BlipPools.Remove(TEXT("NPC"));
        Save(Library);
    }
    check(Library->BlipPools.Contains(TEXT("BlipNPC")) && Library->BlipPools.Contains(TEXT("Sa")));
    if(InstallNPC)
    {
        TArray<TObjectPtr<USoundBase>> Sources;
        for(int32 I=1;I<=3;++I)
        {
            const FString Name=FString::Printf(TEXT("BlipSFX_NPC%d"),I);
            const FString Path=TEXT("/Game/SFX/Blip/")+Name;
            auto* Wave=LoadObject<USoundWave>(nullptr,*Path,nullptr,LOAD_NoWarn|LOAD_Quiet);
            if(!Wave)
            {
                check(!Verify);auto* Task=NewObject<UAssetImportTask>();
                Task->Filename=FPaths::ProjectContentDir()/TEXT("SFX/Blip")/(Name+TEXT(".wav"));
                Task->DestinationPath=TEXT("/Game/SFX/Blip");Task->DestinationName=Name;
                Task->bAutomated=true;Task->bSave=false;
                FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().ImportAssetTasks({Task});
                Wave=LoadObject<USoundWave>(nullptr,*Path);check(Wave);Save(Wave);
            }
            check(Wave && !Wave->IsLooping() && Wave->GetDuration()>0);Sources.Add(Wave);
            UE_LOG(LogTemp,Display,TEXT("BLIP SOURCE %s duration=%.3f"),*Name,Wave->GetDuration());
        }
        auto& Pool=Library->BlipPools.FindChecked(TEXT("BlipNPC"));
        if(!Verify)
        {
            Backup(Library);Pool.Sources=Sources;
            // NPC2 is 2.22 seconds; leave room for its existing 0.96 minimum pitch.
            Library->MaxBlipSeconds=FMath::Max(Library->MaxBlipSeconds,3.f);Save(Library);
        }
        check(Pool.Sources==Sources && Library->MaxBlipSeconds>=3.f);
    }
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();Registry.SearchAllAssets(true);
    FARFilter Filter;Filter.PackagePaths.Add(TEXT("/Game"));Filter.bRecursivePaths=true;Filter.ClassPaths.Add(UDialogueBlueprint::StaticClass()->GetClassPathName());Filter.bRecursiveClasses=true;
    TArray<FAssetData> Assets;Registry.GetAssets(Filter,Assets);int32 Dialogues=0,SaCount=0;
    for(const auto& Asset:Assets)
    {
        auto* BP=Cast<UDialogueBlueprint>(Asset.GetAsset());if(!BP || !BP->GeneratedClass)continue;
        // Existing project dialogues must reach the native playback hook, not bypass it in a BP override.
        auto* Playback=BP->GeneratedClass->FindFunctionByName(TEXT("PlayDialogueNode"));
        checkf(Playback && Playback->HasAnyFunctionFlags(FUNC_Native),TEXT("Audit custom PlayDialogueNode override in %s"),*BP->GetPathName());
        TArray<UDialogue*> Configs={CastChecked<UDialogue>(BP->GeneratedClass->GetDefaultObject()),BP->DialogueTemplate};
        if(auto* Class=Cast<UDialogueBlueprintGeneratedClass>(BP->GeneratedClass))Configs.AddUnique(Class->GetDialogueTemplate());
        if(!Verify)Backup(BP);
        for(auto* Config:Configs)if(Config)
        {
            if(!Verify)Config->BlipLibrary=Library;check(Config->BlipLibrary==Library);
            auto Configure=[&](FSpeakerInfo& Speaker)
            {
                if(!Verify && Speaker.BlipTag==TEXT("NPC"))Speaker.BlipTag=TEXT("BlipNPC");
                check(Speaker.BlipTag!=TEXT("NPC"));
                if(!Verify && Speaker.BlipTag.IsNone())Speaker.BlipTag=IsSa(Speaker)?FName(TEXT("Sa")):FName(TEXT("BlipNPC"));
                if(IsSa(Speaker)){check(Speaker.BlipTag==TEXT("Sa"));++SaCount;}
            };
            for(auto& Speaker:Config->Speakers)Configure(Speaker);
            // VTG's existing Player role is Sa, including dialogues without a jaw actor binding.
            if (!Verify && Config->PlayerSpeakerInfo.SpeakerID==TEXT("Player") &&
                (Config->PlayerSpeakerInfo.BlipTag.IsNone() || Config->PlayerSpeakerInfo.BlipTag==TEXT("BlipNPC")))
                Config->PlayerSpeakerInfo.BlipTag=TEXT("Sa");
            Configure(Config->PlayerSpeakerInfo);
            if(Config->PlayerSpeakerInfo.SpeakerID==TEXT("Player"))check(Config->PlayerSpeakerInfo.BlipTag==TEXT("Sa"));
        }
        if(!Verify){FBlueprintEditorUtils::MarkBlueprintAsModified(BP);Save(BP);}++Dialogues;
    }
    check(Dialogues>0 && SaCount>0);
    AudioManager(Verify);Test();
    UE_LOG(LogTemp,Display,TEXT("BLIP VERIFIED: %d dialogue assets; %d Sa configurations; BlipNPC sources=%d"),Dialogues,SaCount,Library->BlipPools.FindChecked(TEXT("BlipNPC")).Sources.Num());
    return 0;
}
