#include "VTGSequenceInspector.h"
#include "UI/VTGSequenceSettings.h"
#include "UI/VTGSubtitleTrack.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "Sections/MovieSceneEventTriggerSection.h"
#include "Sections/MovieSceneEventRepeaterSection.h"
#include "Sections/MovieSceneSubSection.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Composite.h"
#include "K2Node_Timeline.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ISequencerModule.h"
#include "ISequencer.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "ScopedTransaction.h"
#include "Modules/ModuleManager.h"

namespace VTGSequenceInspector
{
namespace
{
const FName TabId(TEXT("VTG.SequenceInspector"));
FDelegateHandle SequencerHandle;
TWeakObjectPtr<ULevelSequence> Current;
TWeakPtr<SVerticalBox> ResultsBox;

void Walk(UEdGraphNode* Node,const FString& Context,TArray<FFinding>& Out,int Depth,TSet<UEdGraphNode*>& Seen)
{
    if(!Node || Seen.Contains(Node))return;
    if(Depth>32 || Seen.Num()>2000){Out.Add({Context,TEXT("检查达到深度/节点上限，仍有调用未检查"),true});return;}
    Seen.Add(Node);
    const FString Type=Node->GetClass()->GetName();
    if(Cast<UK2Node_Timeline>(Node) || Type.Contains(TEXT("Montage")) || Type.Contains(TEXT("Async")))
        Out.Add({Context,Node->GetNodeTitle(ENodeTitleType::ListView).ToString(),false});
    if(auto* Call=Cast<UK2Node_CallFunction>(Node))
    {
        UFunction* Function=Call->GetTargetFunction();
        const FString Name=Call->FunctionReference.GetMemberName().ToString();
        const bool Risk=Name.Contains(TEXT("Delay")) || Name.Contains(TEXT("SetTimer")) || Name.Contains(TEXT("Montage_Play")) || (Name.StartsWith(TEXT("Play")) && Name.Contains(TEXT("Montage"))) || (Function && Function->HasMetaData(TEXT("Latent")));
        if(Risk)Out.Add({Context,Name,false});
        UBlueprint* BP=Function?Cast<UBlueprint>(Function->GetOwnerClass()->ClassGeneratedBy):nullptr;
        if(BP)
        {
            TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
            bool Found=false;
            for(UEdGraph* Graph:Graphs)for(UEdGraphNode* Entry:Graph->Nodes)
                if((Cast<UK2Node_FunctionEntry>(Entry) && Graph->GetFName()==Call->FunctionReference.GetMemberName()) ||
                   (Cast<UK2Node_CustomEvent>(Entry) && CastChecked<UK2Node_CustomEvent>(Entry)->CustomFunctionName==Call->FunctionReference.GetMemberName()))
                {Found=true;Walk(Entry,Context+TEXT(" → ")+BP->GetName()+TEXT(".")+Name,Out,Depth+1,Seen);}
            if(!Found)Out.Add({Context,Name+TEXT("：蓝图调用目标无法静态确认"),true});
        }
        else if(!Risk && (!Function || (Function->GetOwnerClass()->GetOutermost()->GetName()!=TEXT("/Script/Engine") && Function->GetOwnerClass()->GetOutermost()->GetName()!=TEXT("/Script/CoreUObject"))))
            Out.Add({Context,Name+TEXT("：原生/外部调用，请人工确认是否启动异步操作"),true});
    }
    UEdGraph* Nested=nullptr;
    if(auto* Macro=Cast<UK2Node_MacroInstance>(Node))Nested=Macro->GetMacroGraph();
    if(auto* Composite=Cast<UK2Node_Composite>(Node))Nested=Composite->BoundGraph;
    if(Nested)for(UEdGraphNode* Entry:Nested->Nodes)
    {
        bool HasExecInput=false;for(auto* Pin:Entry->Pins)HasExecInput|=Pin->Direction==EGPD_Input && Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec;
        if(!HasExecInput)Walk(Entry,Context+TEXT(" → ")+Nested->GetName(),Out,Depth+1,Seen);
    }
    for(auto* Pin:Node->Pins)if(Pin->Direction==EGPD_Output && Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec)
        for(auto* Linked:Pin->LinkedTo)Walk(Linked->GetOwningNode(),Context,Out,Depth,Seen);
}
void Scan(ULevelSequence* Sequence,TArray<FFinding>& Out,TSet<ULevelSequence*>& Seen)
{
    if(!Sequence || Seen.Contains(Sequence))return;Seen.Add(Sequence);
    auto* Scene=Sequence->GetMovieScene();if(!Scene)return;
    TArray<UMovieSceneTrack*> Tracks;for(auto* T:Scene->GetTracks())Tracks.Add(T);
    for(const auto& Binding:Scene->GetBindings())for(auto* T:Binding.GetTracks())Tracks.Add(T);
    auto Event=[&](const FMovieSceneEvent& E,const FString& Context)
    {
        if(auto* Endpoint=Cast<UEdGraphNode>(E.WeakEndpoint.Get())){auto Findings=InspectEndpoint(Endpoint,Context);Out.Append(Findings);}
        else Out.Add({Context,TEXT("Event endpoint 无法解析，请打开 Director Blueprint 人工检查"),true});
    };
    for(auto* Track:Tracks)for(auto* Section:Track->GetAllSections())
    {
        if(!Section->IsActive())continue;
        if(auto* Trigger=Cast<UMovieSceneEventTriggerSection>(Section))
        {
            auto Data=Trigger->EventChannel.GetData();
            for(int32 I=0;I<Data.GetValues().Num();++I)
            {
                const double Seconds=Scene->GetTickResolution().AsSeconds(Data.GetTimes()[I]);
                Event(Data.GetValues()[I],FString::Printf(TEXT("%s / Event @ %.3fs"),*Sequence->GetName(),Seconds));
            }
        }
        if(auto* Repeater=Cast<UMovieSceneEventRepeaterSection>(Section))Event(Repeater->Event,Sequence->GetName()+TEXT(" / Repeater"));
        if(auto* Sub=Cast<UMovieSceneSubSection>(Section))Scan(Cast<ULevelSequence>(Sub->GetSequence()),Out,Seen);
    }
    for(auto* Track:Tracks)if(auto* Subtitles=Cast<UMovieSceneVTGSubtitleTrack>(Track))
    {
        const auto& Sections=Subtitles->GetAllSections();
        for(int32 I=0;I<Sections.Num();++I)for(int32 J=I+1;J<Sections.Num();++J)
            if(Sections[I]->IsActive() && Sections[J]->IsActive() && Sections[I]->GetRange().Overlaps(Sections[J]->GetRange()))
                Out.Add({Sequence->GetName()+TEXT(" / Subtitles"),TEXT("字幕区间重叠：运行时只显示起点较晚的一条，请调整时间"),true});
    }
}
void Refresh()
{
    auto Box=ResultsBox.Pin();if(!Box)return;Box->ClearChildren();
    if(!Current.IsValid()){Box->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("打开一个 Level Sequence 后自动检查。")))];return;}
    const auto Findings=Inspect(Current.Get());
    Box->AddSlot().AutoHeight().Padding(0,8)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(Findings.IsEmpty()?TEXT("未发现已识别的异步节点。这不是安全保证；事件动态绑定和运行时行为仍需测试。"):FString::Printf(TEXT("发现 %d 条提示。快进不会推进世界 Delay / Timer / 独立 Montage 的时间。"),Findings.Num())))];
    for(const auto& Finding:Findings)Box->AddSlot().AutoHeight().Padding(0,6)[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(Finding.bUncertain?FLinearColor(1,.8,.3):FLinearColor(1,.45,.25)).Text(FText::FromString((Finding.bUncertain?TEXT("[需确认] "):TEXT("[异步] "))+Finding.Context+TEXT("\n")+Finding.Node))];
}
TSharedRef<SDockTab> Spawn(const FSpawnTabArgs&)
{
    TSharedPtr<SVerticalBox> Results;
    auto Tab=SNew(SDockTab).TabRole(ETabRole::NomadTab)
    [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([](){return FText::FromString(Current.IsValid()?Current->GetPathName():TEXT("Sequence 检查"));})]
      +SVerticalBox::Slot().AutoHeight().Padding(12,4)[SNew(SCheckBox)
        .IsChecked_Lambda([](){auto* S=Current.IsValid()?Cast<UVTGSequenceSettings>(Current->GetAssetUserDataOfClass(UVTGSequenceSettings::StaticClass())):nullptr;return S && S->bAllowSkip?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
        .OnCheckStateChanged_Lambda([](ECheckBoxState State){if(!Current.IsValid())return;const FScopedTransaction Transaction(NSLOCTEXT("VTG","SequenceAllowSkip","Change sequence skip permission"));Current->Modify();auto* S=Cast<UVTGSequenceSettings>(Current->GetAssetUserDataOfClass(UVTGSequenceSettings::StaticClass()));if(!S){S=NewObject<UVTGSequenceSettings>(Current.Get(),NAME_None,RF_Transactional);Current->AddAssetUserData(S);}S->Modify();S->bAllowSkip=State==ECheckBoxState::Checked;Current->MarkPackageDirty();})
        [SNew(STextBlock).Text(FText::FromString(TEXT("Allow Skip（默认关闭，修改后保存 Sequence）")))]]
      +SVerticalBox::Slot().AutoHeight().Padding(12,4)[SNew(SButton).Text(FText::FromString(TEXT("重新检查事件（修改后点击）"))).OnClicked_Lambda([](){Refresh();return FReply::Handled();})]
      +SVerticalBox::Slot().FillHeight(1).Padding(12)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Results,SVerticalBox)]]];
    ResultsBox=Results;Refresh();return Tab;
}
}
TArray<FFinding> InspectEndpoint(UEdGraphNode* Endpoint,const FString& Context){TArray<FFinding> Out;TSet<UEdGraphNode*> Seen;Walk(Endpoint,Context,Out,0,Seen);return Out;}
TArray<FFinding> Inspect(ULevelSequence* Sequence){TArray<FFinding> Out;TSet<ULevelSequence*> Seen;Scan(Sequence,Out,Seen);return Out;}
void Startup()
{
    if(IsRunningCommandlet())return;
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId,FOnSpawnTab::CreateStatic(&Spawn)).SetDisplayName(FText::FromString(TEXT("Sequence 快进检查"))).SetTooltipText(FText::FromString(TEXT("检查 Sequence 事件异步调用，并设置 Allow Skip。")));
    auto& Module=FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));
    SequencerHandle=Module.RegisterOnSequencerCreated(FOnSequencerCreated::FDelegate::CreateLambda([](TSharedRef<ISequencer> Sequencer){Current=Cast<ULevelSequence>(Sequencer->GetRootMovieSceneSequence());if(Current.IsValid()){FGlobalTabmanager::Get()->TryInvokeTab(TabId);Refresh();}}));
}
void Shutdown()
{
    if(SequencerHandle.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("Sequencer")))FModuleManager::GetModuleChecked<ISequencerModule>(TEXT("Sequencer")).UnregisterOnSequencerCreated(SequencerHandle);
    SequencerHandle.Reset();
    if(!IsRunningCommandlet())
    {
        if(auto Tab=FGlobalTabmanager::Get()->FindExistingLiveTab(TabId))Tab->RequestCloseTab();
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
    }
    ResultsBox.Reset();Current.Reset();
}
}
