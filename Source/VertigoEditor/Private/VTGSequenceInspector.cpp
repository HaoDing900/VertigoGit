#include "VTGSequenceInspector.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "UI/VTGSequenceSettings.h"
#include "UI/VTGSequenceDialogue.h"
#include "UI/VTGQuestSequenceBox.h"
#include "Audio/VTGCutsceneAudio.h"
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
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SComboBox.h"
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
TWeakPtr<SComboBox<TSharedPtr<EVTGCutsceneAudioPreset>>> AudioPresetBox;
const TArray<TSharedPtr<EVTGCutsceneAudioPreset>> AudioPresets={
    MakeShared<EVTGCutsceneAudioPreset>(EVTGCutsceneAudioPreset::KeepWorld),
    MakeShared<EVTGCutsceneAudioPreset>(EVTGCutsceneAudioPreset::DialogueFocus),
    MakeShared<EVTGCutsceneAudioPreset>(EVTGCutsceneAudioPreset::MuteWorldBeds)};
FText AudioPresetLabel(EVTGCutsceneAudioPreset Preset)
{
    switch(Preset)
    {
    case EVTGCutsceneAudioPreset::DialogueFocus:return FText::FromString(TEXT("突出剧情（压低环境，淡出音乐）"));
    case EVTGCutsceneAudioPreset::MuteWorldBeds:return FText::FromString(TEXT("静音环境与背景音乐"));
    default:return FText::FromString(TEXT("保留现场声音"));
    }
}

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
    TSharedPtr<SComboBox<TSharedPtr<EVTGCutsceneAudioPreset>>> PresetBox;
    auto Tab=SNew(SDockTab).TabRole(ETabRole::NomadTab)
    [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([](){return FText::FromString(Current.IsValid()?Current->GetPathName():TEXT("Sequence 检查"));})]
      +SVerticalBox::Slot().AutoHeight().Padding(12,4)[SNew(SCheckBox)
        .IsChecked_Lambda([](){auto* S=Current.IsValid()?Cast<UVTGSequenceSettings>(Current->GetAssetUserDataOfClass(UVTGSequenceSettings::StaticClass())):nullptr;return S && S->bAllowSkip?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
        .OnCheckStateChanged_Lambda([](ECheckBoxState State){if(!Current.IsValid())return;const FScopedTransaction Transaction(NSLOCTEXT("VTG","SequenceAllowSkip","Change sequence skip permission"));Current->Modify();auto* S=Cast<UVTGSequenceSettings>(Current->GetAssetUserDataOfClass(UVTGSequenceSettings::StaticClass()));if(!S){S=NewObject<UVTGSequenceSettings>(Current.Get(),NAME_None,RF_Transactional);Current->AddAssetUserData(S);}S->Modify();S->bAllowSkip=State==ECheckBoxState::Checked;Current->MarkPackageDirty();})
        [SNew(STextBlock).Text(FText::FromString(TEXT("Allow Skip（默认关闭，修改后保存 Sequence）")))]]
      +SVerticalBox::Slot().AutoHeight().Padding(12,4)[SNew(SCheckBox)
        .IsEnabled_Lambda([](){return Current.IsValid();})
        .IsChecked_Lambda([](){return UVTGQuestSequenceBox::GetSequenceDefault(Current.Get())?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
        .OnCheckStateChanged_Lambda([](ECheckBoxState State){if(!Current.IsValid())return;const FScopedTransaction T(NSLOCTEXT("VTGSequence","QuestUIEdit","设置过场任务 UI 显示"));UVTGQuestSequenceBox::SetSequenceDefault(Current.Get(),State==ECheckBoxState::Checked);})
        .ToolTipText(FText::FromString(TEXT("默认开启，只隐藏 Quest 的标题、通知和目标区域；对话框与头像保持原逻辑。停止或结束后解除隐藏；暂停期间保持隐藏。修改后保存 Sequence。")))
        [SNew(STextBlock).Text(FText::FromString(TEXT("播放期间隐藏 Quest UI（默认开启）")))]]
      +SVerticalBox::Slot().AutoHeight().Padding(12,4)[SNew(SCheckBox)
        .IsEnabled_Lambda([](){return Current.IsValid();})
        .IsChecked_Lambda([](){return UVTGSequenceDialogueGate::GetSequenceDefault(Current.Get())?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
        .OnCheckStateChanged_Lambda([](ECheckBoxState State){if(!Current.IsValid())return;const FScopedTransaction T(NSLOCTEXT("VTGSequence","AssetExitEdit","设置 Sequence 进入时结束对话"));UVTGSequenceDialogueGate::SetSequenceDefault(Current.Get(),State==ECheckBoxState::Checked);})
        .ToolTipText(FText::FromString(TEXT("保存到当前 Sequence 资产。外部播放开始时强制结束旧 Narrative 对话，忽略 Allow Exit；包括运行时新建播放器。关卡实例可单独覆盖；Narrative 自己的对话镜头保持受保护。修改后保存 Sequence。")))
        [SNew(STextBlock).Text(FText::FromString(TEXT("进入时结束 Narrative 对话（Sequence 默认）")))]]
      +SVerticalBox::Slot().AutoHeight().Padding(12,4)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[SNew(STextBlock).Text(FText::FromString(TEXT("内部音频音量倍数")))]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SNumericEntryBox<float>).MinValue(0.f).MinDesiredValueWidth(100)
          .IsEnabled_Lambda([](){return Current.IsValid();})
          .Value_Lambda([]()->TOptional<float>{auto* S=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Current.Get());return S?S->SequenceAudioVolumeMultiplier:1.f;})
          .OnValueCommitted_Lambda([](float Value,ETextCommit::Type){if(!Current.IsValid() || !FMath::IsFinite(Value))return;const FScopedTransaction T(NSLOCTEXT("VTG","SequenceAudioGain","设置 Sequence 内部音频音量倍数"));Current->Modify();auto* S=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Current.Get(),true);S->Modify();S->SequenceAudioVolumeMultiplier=FMath::Max(0.f,Value);Current->MarkPackageDirty();})
          .ToolTipText(FText::FromString(TEXT("默认 1；0 静音，0.5 减半，2 加倍。乘以内部及嵌套 Audio Track 的原有音量曲线。修改后保存 Sequence。")))]]
      +SVerticalBox::Slot().AutoHeight().Padding(12,4)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[SNew(STextBlock).Text(FText::FromString(TEXT("过场声音预设")))]
        +SHorizontalBox::Slot().AutoWidth()[SAssignNew(PresetBox,SComboBox<TSharedPtr<EVTGCutsceneAudioPreset>>).OptionsSource(&AudioPresets)
          .IsEnabled_Lambda([](){return Current.IsValid();})
          .OnComboBoxOpening_Lambda([](){auto Box=AudioPresetBox.Pin();if(!Box)return;auto* S=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Current.Get());const auto Preset=S?S->Preset:EVTGCutsceneAudioPreset::KeepWorld;for(auto Option:AudioPresets)if(*Option==Preset){Box->SetSelectedItem(Option);break;}})
          .OnGenerateWidget_Lambda([](TSharedPtr<EVTGCutsceneAudioPreset> Preset){return SNew(STextBlock).Text(AudioPresetLabel(*Preset));})
          .OnSelectionChanged_Lambda([](TSharedPtr<EVTGCutsceneAudioPreset> Preset,ESelectInfo::Type SelectInfo){if(!Current.IsValid() || !Preset || SelectInfo==ESelectInfo::Direct)return;auto* Existing=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Current.Get());if((Existing?Existing->Preset:EVTGCutsceneAudioPreset::KeepWorld)==*Preset)return;const FScopedTransaction T(NSLOCTEXT("VTG","SequenceAudioPreset","设置过场声音预设"));Current->Modify();auto* S=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Current.Get(),true);S->Modify();S->Preset=*Preset;Current->MarkPackageDirty();})
          .ToolTipText(FText::FromString(TEXT("与关卡实例 Details 的过场声音预设共用同一份 Sequence 资产设置，修改后保存 Sequence。环境控制影响 Audio Manager 的 Ambient 和勾选跟随过场环境声的场景声源；Music 单独控制。")))
          [SNew(STextBlock).Text_Lambda([](){auto* S=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Current.Get());return AudioPresetLabel(S?S->Preset:EVTGCutsceneAudioPreset::KeepWorld);})]]]
      +SVerticalBox::Slot().AutoHeight().Padding(12,4)[SNew(SButton).Text(FText::FromString(TEXT("重新检查事件（修改后点击）"))).OnClicked_Lambda([](){Refresh();return FReply::Handled();})]
      +SVerticalBox::Slot().FillHeight(1).Padding(12)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Results,SVerticalBox)]]];
    AudioPresetBox=PresetBox;ResultsBox=Results;Refresh();return Tab;
}
}
TArray<FFinding> InspectEndpoint(UEdGraphNode* Endpoint,const FString& Context){TArray<FFinding> Out;TSet<UEdGraphNode*> Seen;Walk(Endpoint,Context,Out,0,Seen);return Out;}
TArray<FFinding> Inspect(ULevelSequence* Sequence){TArray<FFinding> Out;TSet<ULevelSequence*> Seen;Scan(Sequence,Out,Seen);return Out;}
void Startup()
{
    if(IsRunningCommandlet())return;
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId,FOnSpawnTab::CreateStatic(&Spawn)).SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory()).SetDisplayName(FText::FromString(TEXT("Sequence 快进检查"))).SetTooltipText(FText::FromString(TEXT("检查 Sequence 事件异步调用，并设置 Allow Skip。")));
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
    AudioPresetBox.Reset();ResultsBox.Reset();Current.Reset();
}
}
