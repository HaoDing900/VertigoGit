#include "VTGCutsceneAudioDetails.h"
#include "Audio/VTGCutsceneAudio.h"
#include "UI/VTGSequenceDialogue.h"
#include "UI/VTGQuestSequenceBox.h"
#include "Sound/AmbientSound.h"
#include "Components/AudioComponent.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "MovieScene.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyEditorModule.h"
#include "Editor.h"
#include "Misc/CoreDelegates.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"

namespace VTGCutsceneAudioDetails
{
TMap<FName,FDetailLayoutCallback> Previous;
TSet<FName> Registered;
FDelegateHandle PostEngineInitHandle;
class FDetails : public IDetailCustomization
{
    FName RegisteredFor;
    TSharedPtr<IDetailCustomization> Original;
    TArray<TWeakObjectPtr<UAudioComponent>> Audio;
    TWeakObjectPtr<ULevelSequence> Sequence;
    TArray<TWeakObjectPtr<ALevelSequenceActor>> SequenceActors;
    TArray<TSharedPtr<EVTGCutsceneAudioPreset>> Options;
    static FText Label(EVTGCutsceneAudioPreset P)
    {
        switch(P)
        {
        case EVTGCutsceneAudioPreset::DialogueFocus:return NSLOCTEXT("VTGCutsceneAudio","Focus","突出剧情（压低环境，淡出音乐）");
        case EVTGCutsceneAudioPreset::MuteWorldBeds:return NSLOCTEXT("VTGCutsceneAudio","Mute","静音环境与背景音乐");
        default:return NSLOCTEXT("VTGCutsceneAudio","Keep","保留现场声音");
        }
    }
    ECheckBoxState Checked() const
    {
        bool On=false,Off=false;
        for(auto A:Audio)if(A.IsValid()) {if(UVTGCutsceneAudioLibrary::GetFollowAmbientSequenceMute(A.Get()))On=true;else Off=true;}
        return On&&Off?ECheckBoxState::Undetermined:On?ECheckBoxState::Checked:ECheckBoxState::Unchecked;
    }
    void Toggle(ECheckBoxState State)
    {
        const FScopedTransaction Transaction(NSLOCTEXT("VTGCutsceneAudio","SetFollow","设置场景声音过场控制"));
        for(auto A:Audio)if(A.IsValid()&&!A->IsTemplate())
        {A->Modify();if(A->GetOwner())A->GetOwner()->Modify();UVTGCutsceneAudioLibrary::SetFollowAmbientSequenceMute(A.Get(),State==ECheckBoxState::Checked);A->MarkPackageDirty();}
    }
    void SetPreset(TSharedPtr<EVTGCutsceneAudioPreset> Selected,ESelectInfo::Type)
    {
        if(!Selected || !Sequence.IsValid())return;
        const FScopedTransaction Transaction(NSLOCTEXT("VTGCutsceneAudio","SetPreset","设置过场音频预设"));
        auto* Seq=Sequence.Get();Seq->Modify();auto* Settings=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Seq,true);Settings->Modify();Settings->Preset=*Selected;Seq->MarkPackageDirty();
    }
public:
    explicit FDetails(FOnGetDetailCustomizationInstance Old,FName ClassName):RegisteredFor(ClassName)
    {if(Old.IsBound())Original=Old.Execute();}
    virtual void CustomizeDetails(IDetailLayoutBuilder& Builder) override
    {
        if(Original)Original->CustomizeDetails(Builder);
        // A single-component AmbientSound layout invokes both class customizations on the
        // same actor view. Let the actor add our row there; use the component path when
        // the component itself (or another actor's component) is being inspected.
        const bool AmbientActorView=Builder.GetBaseClass() && Builder.GetBaseClass()->IsChildOf(AAmbientSound::StaticClass());
        if((RegisteredFor==TEXT("AudioComponent") && AmbientActorView) ||
           (RegisteredFor==TEXT("AmbientSound") && !AmbientActorView))return;
        Audio.Reset();Sequence.Reset();SequenceActors.Reset();Options.Reset();
        TArray<TWeakObjectPtr<UObject>> Objects;Builder.GetObjectsBeingCustomized(Objects);
        for(auto Object:Objects)if(auto* O=Object.Get())
        {
            if(auto* A=Cast<AAmbientSound>(O))Audio.AddUnique(A->GetAudioComponent());
            if(auto* A=Cast<UAudioComponent>(O))if(!A->IsTemplate())
            {
                auto* Manager=FindObject<UClass>(nullptr,TEXT("/Game/LevelNSequence/LevelAudio/BP_VTG_AudioManager.BP_VTG_AudioManager_C"));
                if(!Manager || !A->GetOwner() || !A->GetOwner()->IsA(Manager))Audio.AddUnique(A);
            }
            if(auto* S=Cast<ULevelSequence>(O))Sequence=S;
            if(auto* A=Cast<ALevelSequenceActor>(O)){Sequence=A->GetSequence();SequenceActors.Add(A);}
            if(auto* S=Cast<UMovieScene>(O))Sequence=Cast<ULevelSequence>(S->GetOuter());
        }
        auto& Category=Builder.EditCategory(TEXT("VTG Cutscene Audio"),NSLOCTEXT("VTGCutsceneAudio","Category","过场音频"),ECategoryPriority::Important);
        if(Sequence.IsValid() && Objects.Num()==1)
        {
            auto& Entry=Builder.EditCategory(TEXT("VTG Sequence Entry"),NSLOCTEXT("VTGSequence","Entry","过场入口"),ECategoryPriority::Important);
            Entry.AddCustomRow(NSLOCTEXT("VTGSequence","QuestUISearch","Quest 任务 UI 隐藏"))
            .NameContent()[SNew(STextBlock).Text(NSLOCTEXT("VTGSequence","QuestUILabel","播放期间隐藏 Quest UI"))]
            .ValueContent()[SNew(SCheckBox)
                .IsChecked_Lambda([this](){return UVTGQuestSequenceBox::GetSequenceDefault(Sequence.Get())?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this](ECheckBoxState State){const FScopedTransaction T(NSLOCTEXT("VTGSequence","QuestUIEdit","设置过场任务 UI 显示"));UVTGQuestSequenceBox::SetSequenceDefault(Sequence.Get(),State==ECheckBoxState::Checked);})
                .ToolTipText(NSLOCTEXT("VTGSequence","QuestUITip","默认开启。仅遮住任务标题、通知和目标列表，对话框、头像和任务逻辑不变。Sequence 结束/停止后解除遮挡，暂停期间保持。保存 Sequence 生效。"))];
            Entry.AddCustomRow(NSLOCTEXT("VTGSequence","AssetExitSearch","Sequence 默认 Narrative 进入时结束对话"))
            .NameContent()[SNew(STextBlock).Text(NSLOCTEXT("VTGSequence","AssetExitLabel","Sequence 默认：进入时结束对话"))]
            .ValueContent()[SNew(SCheckBox)
                .IsChecked_Lambda([this](){return UVTGSequenceDialogueGate::GetSequenceDefault(Sequence.Get())?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this](ECheckBoxState State){const FScopedTransaction T(NSLOCTEXT("VTGSequence","AssetExitEdit","设置 Sequence 进入时结束对话"));UVTGSequenceDialogueGate::SetSequenceDefault(Sequence.Get(),State==ECheckBoxState::Checked);})
                .ToolTipText(NSLOCTEXT("VTGSequence","AssetExitTip","保存到 Sequence 资产。外部播放开始时忽略 Allow Exit 强制结束旧对话；动态创建的播放器也生效。关卡实例可单独覆盖。修改后保存 Sequence。"))];
        }
        if(!SequenceActors.IsEmpty())
        {
            auto& Entry=Builder.EditCategory(TEXT("VTG Sequence Entry"),NSLOCTEXT("VTGSequence","Entry","过场入口"),ECategoryPriority::Important);
            Entry.AddCustomRow(NSLOCTEXT("VTGSequence","ExitSearch","Narrative Force Exit Dialogue 进入时结束对话"))
            .NameContent()[SNew(STextBlock).Text(NSLOCTEXT("VTGSequence","ExitLabel","本实例：进入时结束 Narrative 对话"))]
            .ValueContent()[SNew(SHorizontalBox)
              +SHorizontalBox::Slot().AutoWidth()[SNew(SCheckBox)
                .IsChecked_Lambda([this](){bool On=false,Off=false;for(auto A:SequenceActors)if(A.IsValid()){if(UVTGSequenceDialogueGate::ShouldEndDialogue(A.Get()))On=true;else Off=true;}return On&&Off?ECheckBoxState::Undetermined:On?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this](ECheckBoxState State){const FScopedTransaction T(NSLOCTEXT("VTGSequence","ExitEdit","设置过场开始时结束对话"));for(auto A:SequenceActors)if(A.IsValid()){A->Modify();if(State==ECheckBoxState::Checked){A->Tags.Remove(UVTGSequenceDialogueGate::KeepDialogueTag);A->Tags.AddUnique(UVTGSequenceDialogueGate::EndDialogueTag);}else{A->Tags.Remove(UVTGSequenceDialogueGate::EndDialogueTag);A->Tags.AddUnique(UVTGSequenceDialogueGate::KeepDialogueTag);}A->MarkPackageDirty();}})
                .ToolTipText(NSLOCTEXT("VTGSequence","ExitTip","本实例覆盖 Sequence 默认。勾选强制结束旧对话（忽略 Allow Exit），取消则保留旧对话；点击跟随 Sequence 清除实例覆盖。保存关卡生效。Narrative 自己的对话镜头保持受保护。"))]
              +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SButton).Text(NSLOCTEXT("VTGSequence","FollowAsset","跟随 Sequence"))
                .OnClicked_Lambda([this](){const FScopedTransaction T(NSLOCTEXT("VTGSequence","ResetExitOverride","清除 Sequence 实例退出覆盖"));for(auto A:SequenceActors)if(A.IsValid()){A->Modify();A->Tags.Remove(UVTGSequenceDialogueGate::EndDialogueTag);A->Tags.Remove(UVTGSequenceDialogueGate::KeepDialogueTag);A->MarkPackageDirty();}return FReply::Handled();})]];
        }
        if(!Audio.IsEmpty())
            Category.AddCustomRow(NSLOCTEXT("VTGCutsceneAudio","FollowSearch","Follow Ambient Seq Mute 跟随过场环境声"))
            .NameContent()[SNew(STextBlock).Text(NSLOCTEXT("VTGCutsceneAudio","Follow","跟随过场环境声"))]
            .ValueContent()[SNew(SCheckBox).IsChecked(this,&FDetails::Checked).OnCheckStateChanged(this,&FDetails::Toggle)
                .ToolTipText(NSLOCTEXT("VTGCutsceneAudio","FollowTip","默认关闭。开启后，此关卡声源实例跟随 Ambient 的过场压低/静音规则；不修改共享的 Sound Cue。"))];
        if(Sequence.IsValid())
        {
            Category.AddCustomRow(NSLOCTEXT("VTGCutsceneAudio","GainSearch","Sequence Audio Volume Multiplier 内部音频音量倍数"))
            .NameContent()[SNew(STextBlock).Text(NSLOCTEXT("VTGCutsceneAudio","GainLabel","内部音频音量倍数"))]
            .ValueContent().MinDesiredWidth(180)[SNew(SNumericEntryBox<float>).MinValue(0.f).MinSliderValue(0.f).MaxSliderValue(4.f)
                .Value_Lambda([this]()->TOptional<float>{auto* S=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Sequence.Get());return S?S->SequenceAudioVolumeMultiplier:1.f;})
                .OnValueCommitted_Lambda([this](float Value,ETextCommit::Type){if(!Sequence.IsValid() || !FMath::IsFinite(Value))return;const FScopedTransaction T(NSLOCTEXT("VTGCutsceneAudio","GainEdit","设置 Sequence 内部音频音量倍数"));Sequence->Modify();auto* S=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Sequence.Get(),true);S->Modify();S->SequenceAudioVolumeMultiplier=FMath::Max(0.f,Value);Sequence->MarkPackageDirty();})
                .ToolTipText(NSLOCTEXT("VTGCutsceneAudio","GainTip","默认 1。统一乘以当前 Sequence 及嵌套 Sequence 的 Audio Track 音量，保留片段原有音量曲线；0 静音，0.5 减半，2 加倍。嵌套 Sequence 自身的倍数继续相乘。保存 Sequence 资产后，编辑器预览与游戏均生效。"))];
            for(auto P:{EVTGCutsceneAudioPreset::KeepWorld,EVTGCutsceneAudioPreset::DialogueFocus,EVTGCutsceneAudioPreset::MuteWorldBeds})Options.Add(MakeShared<EVTGCutsceneAudioPreset>(P));
            Category.AddCustomRow(NSLOCTEXT("VTGCutsceneAudio","ProfileSearch","Audio Profile Preset 过场声音预设"))
            .NameContent()[SNew(STextBlock).Text(NSLOCTEXT("VTGCutsceneAudio","Profile","过场声音预设"))]
            .ValueContent().MinDesiredWidth(280)[SNew(SComboBox<TSharedPtr<EVTGCutsceneAudioPreset>>).OptionsSource(&Options)
                .OnGenerateWidget_Lambda([](TSharedPtr<EVTGCutsceneAudioPreset> P){return SNew(STextBlock).Text(Label(*P));})
                .OnSelectionChanged(this,&FDetails::SetPreset)
                .ToolTipText(NSLOCTEXT("VTGCutsceneAudio","PresetTip","保存到当前 Sequence 资产。未设置时保留现场；环境控制影响 Audio Manager 的 Ambient 和主动勾选的场景声源，Music 单独控制。"))
                [SNew(STextBlock).Text_Lambda([this](){auto* S=UVTGCutsceneAudioLibrary::GetSequenceAudioSettings(Sequence.Get());return Label(S?S->Preset:EVTGCutsceneAudioPreset::KeepWorld);})]];
        }
    }
};
void RegisterCustomizations()
{
    if(!GEditor || !Registered.IsEmpty())return;
    FModuleManager::Get().LoadModule(TEXT("DetailCustomizations"));
    FModuleManager::Get().LoadModule(TEXT("AudioEditor"));
    FModuleManager::Get().LoadModule(TEXT("LevelSequenceEditor"));
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    for(FName Name:{FName(TEXT("AmbientSound")),FName(TEXT("AudioComponent")),FName(TEXT("LevelSequence")),FName(TEXT("LevelSequenceActor")),FName(TEXT("MovieScene"))})
    {
        FOnGetDetailCustomizationInstance Old;
        if(auto* Existing=Module.GetClassNameToDetailLayoutNameMap().Find(Name)){Previous.Add(Name,*Existing);Old=Existing->DetailLayoutDelegate;}
        Module.RegisterCustomClassLayout(Name,FOnGetDetailCustomizationInstance::CreateLambda([Old,Name](){return MakeShared<FDetails>(Old,Name);}));Registered.Add(Name);
    }
    Module.NotifyCustomizationModuleChanged();
}
void Startup()
{
    if(IsRunningCommandlet())return;
    // LevelSequenceEditor loads PlacementMode, which dereferences GEditor during startup.
    // The project's Default loading phase runs before the editor engine exists.
    if(GEditor)
    {
        RegisterCustomizations();
    }
    else if(!PostEngineInitHandle.IsValid())
    {
        PostEngineInitHandle=FCoreDelegates::OnPostEngineInit.AddStatic(&RegisterCustomizations);
    }
}
void Shutdown()
{
    FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
    PostEngineInitHandle.Reset();
    if(!FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))return;
    auto& Module=FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    for(auto Name:Registered)
    {
        Module.UnregisterCustomClassLayout(Name);
        if(auto* Old=Previous.Find(Name)){FRegisterCustomClassLayoutParams Params;Params.OptionalOrder=Old->Order;Module.RegisterCustomClassLayout(Name,Old->DetailLayoutDelegate,Params);}
    }
    Previous.Empty();Registered.Empty();
}
}
