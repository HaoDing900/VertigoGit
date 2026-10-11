#include "VTGSequenceInspector.h"
#include "Misc/AutomationTest.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Animation/AnimInstance.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Sections/MovieSceneEventTriggerSection.h"
#include "Sections/MovieSceneSubSection.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGSequenceInspectorTest,"Vertigo.Sequence.AsyncInspector",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGSequenceInspectorTest::RunTest(const FString&)
{
    auto* BP=NewObject<UBlueprint>();BP->SkeletonGeneratedClass=UObject::StaticClass();BP->GeneratedClass=UObject::StaticClass();auto* Graph=NewObject<UEdGraph>(BP);Graph->Schema=UEdGraphSchema_K2::StaticClass();BP->UbergraphPages.Add(Graph);
    auto* Entry=NewObject<UK2Node_CustomEvent>(Graph);Graph->AddNode(Entry);Entry->CustomFunctionName=TEXT("TestEvent");Entry->AllocateDefaultPins();
    auto Call=[&](UClass* Class,const TCHAR* Name){auto* N=NewObject<UK2Node_CallFunction>(Graph);Graph->AddNode(N);auto* F=Class->FindFunctionByName(Name);check(F);N->SetFromFunction(F);N->AllocateDefaultPins();return N;};
    auto* Delay=Call(UKismetSystemLibrary::StaticClass(),TEXT("Delay"));
    auto* Timer=Call(UKismetSystemLibrary::StaticClass(),TEXT("K2_SetTimer"));
    Call(UAnimInstance::StaticClass(),TEXT("Montage_Play")); // Unconnected node must not be reported.
    auto* Schema=GetDefault<UEdGraphSchema_K2>();check(Schema->TryCreateConnection(Entry->FindPin(TEXT("then")),Delay->FindPin(TEXT("execute"))));
    check(Schema->TryCreateConnection(Delay->FindPin(TEXT("then")),Timer->FindPin(TEXT("execute"))));
    const auto Findings=VTGSequenceInspector::InspectEndpoint(Entry,TEXT("Fixture"));
    TestEqual(TEXT("Reachable Delay and Timer, excluding disconnected Montage"),Findings.Num(),2);
    TestTrue(TEXT("Delay identified"),Findings.ContainsByPredicate([](const auto& F){return F.Node==TEXT("Delay");}));
    TestTrue(TEXT("Timer identified"),Findings.ContainsByPredicate([](const auto& F){return F.Node==TEXT("K2_SetTimer");}));
    auto* Sequence=NewObject<ULevelSequence>();Sequence->Initialize();
    auto* Track=Sequence->GetMovieScene()->AddTrack<UMovieSceneEventTrack>();
    auto* Section=NewObject<UMovieSceneEventTriggerSection>(Track);Section->SetRange(TRange<FFrameNumber>(0,100));Track->AddSection(*Section);
    FMovieSceneEvent Event;Event.WeakEndpoint=Entry;Section->EventChannel.GetData().AddKey(20,Event);
    TestEqual(TEXT("Actual event track scanned"),VTGSequenceInspector::Inspect(Sequence).Num(),2);
    Section->SetIsActive(false);TestEqual(TEXT("Inactive event section ignored"),VTGSequenceInspector::Inspect(Sequence).Num(),0);Section->SetIsActive(true);
    auto* Root=NewObject<ULevelSequence>();Root->Initialize();auto* SubTrack=Root->GetMovieScene()->AddTrack<UMovieSceneSubTrack>();
    auto* Sub=NewObject<UMovieSceneSubSection>(SubTrack);Sub->SetSequence(Sequence);Sub->SetRange(TRange<FFrameNumber>(0,100));SubTrack->AddSection(*Sub);
    TestEqual(TEXT("Nested sequence events inspected"),VTGSequenceInspector::Inspect(Root).Num(),2);
    FMovieSceneEvent Unknown;Section->EventChannel.GetData().AddKey(30,Unknown);
    auto Mixed=VTGSequenceInspector::Inspect(Sequence);TestEqual(TEXT("Unresolved endpoint retained as uncertainty"),Mixed.Num(),3);
    TestTrue(TEXT("Uncertainty explicitly marked"),Mixed.ContainsByPredicate([](const auto& F){return F.bUncertain;}));
    return true;
}
#endif
