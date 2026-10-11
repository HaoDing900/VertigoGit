#include "VTGSubtitlePreview.h"
#include "UI/VTGSubtitleSubsystem.h"
#include "UI/VTGSubtitleTrack.h"
#include "UI/VTGSubtitleStyleSettings.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "LevelSequence.h"
#include "ISequencer.h"
#include "MovieScene.h"
#include "UObject/StrongObjectPtr.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/Internationalization.h"

namespace VTGSubtitlePreview
{
namespace
{
const FName TabId(TEXT("VTG.SubtitlePreview"));
const FName GlobalTabId(TEXT("VTG.GlobalSubtitleStyle"));
TWeakPtr<ISequencer> Current;
TWeakObjectPtr<ULevelSequence> LastSequence;
TWeakObjectPtr<UMovieSceneVTGSubtitleTrack> LastTrack;
TStrongObjectPtr<UVTGSubtitleWidget> Widget;
TWeakPtr<IDetailsView> Details;
bool Enabled=false;
bool OwnsLanguage=false;
bool PreviousLanguageEnabled=false;
FString PreviousLanguage;
FString Language;
FString LanguageStatus;
TSharedRef<SDockTab> SpawnGlobal(const FSpawnTabArgs&)
{
    FDetailsViewArgs Args;Args.bAllowSearch=true;Args.bHideSelectionTip=true;
    auto View=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(Args);
    View->SetObject(GetMutableDefault<UVTGSubtitleStyleSettings>());
    return SNew(SDockTab).TabRole(ETabRole::NomadTab)
    [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("修改全局样式会影响所有未单独覆盖的 Sequence 字幕。此窗口不显示预览画面，可停靠、关闭或折叠。")))]
      +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SButton).Text(FText::FromString(TEXT("保存全局字幕设置"))).OnClicked_Lambda([](){auto* Settings=GetMutableDefault<UVTGSubtitleStyleSettings>();Settings->SaveConfig(CPF_Config,*Settings->GetDefaultConfigFilename());return FReply::Handled();})]
      +SVerticalBox::Slot().FillHeight(1).Padding(8)[SNew(SExpandableArea).InitiallyCollapsed(false).HeaderContent()[SNew(STextBlock).Text(FText::FromString(TEXT("全局字体、颜色与描边")))].BodyContent()[View]]];
}
void RestoreLanguage()
{
    if(!OwnsLanguage)return;
    auto& Manager=FTextLocalizationManager::Get();
    if(PreviousLanguageEnabled)Manager.EnableGameLocalizationPreview(PreviousLanguage);else Manager.DisableGameLocalizationPreview();
    OwnsLanguage=false;LanguageStatus.Empty();
}
TSharedRef<SDockTab> Spawn(const FSpawnTabArgs&)
{
    auto Class=LoadClass<UVTGSubtitleWidget>(nullptr,TEXT("/Game/Widget/Subtitles/WBP_SequenceSubtitles.WBP_SequenceSubtitles_C"));
    if(Class){Widget.Reset(NewObject<UVTGSubtitleWidget>(GetTransientPackage(),Class));Widget->Initialize();}
    FDetailsViewArgs Args;Args.bAllowSearch=true;Args.bHideSelectionTip=true;auto View=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(Args);Details=View;
    TSharedRef<SWidget> Content=Widget.IsValid()?Widget->TakeWidget():SNew(STextBlock).Text(FText::FromString(TEXT("无法加载 WBP_SequenceSubtitles")));
    auto Tab=SNew(SDockTab).TabRole(ETabRole::NomadTab)
    [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight().Padding(8)[SNew(SCheckBox).IsChecked_Lambda([](){return Enabled?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([](ECheckBoxState State){Enabled=State==ECheckBoxState::Checked;if(!Enabled && Widget.IsValid())Widget->SetLine(nullptr);})[SNew(STextBlock).Text(FText::FromString(TEXT("编辑器字幕预览（只影响此面板）")))]]
      +SVerticalBox::Slot().AutoHeight().Padding(8,2)[SNew(SButton).Text(FText::FromString(TEXT("打开全局字幕样式（独立可折叠窗口）"))).OnClicked_Lambda([](){FGlobalTabmanager::Get()->TryInvokeTab(GlobalTabId);return FReply::Handled();})]
      +SVerticalBox::Slot().AutoHeight().Padding(8,2)[SNew(STextBlock).Text_Lambda([](){auto S=Current.Pin();return FText::FromString(S && S->GetFocusedMovieSceneSequence()?S->GetFocusedMovieSceneSequence()->GetPathName():TEXT("请打开带字幕轨道的 Sequence"));})]
      +SVerticalBox::Slot().AutoHeight().Padding(8,2)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1)[SNew(SEditableTextBox).HintText(FText::FromString(TEXT("预览语言代码，例如 zh-Hans / en"))).OnTextChanged_Lambda([](const FText& Text){Language=Text.ToString();})]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("预览语言"))).OnClicked_Lambda([](){if(!FInternationalization::Get().GetCulture(Language).IsValid()){LanguageStatus=TEXT("语言代码无效");return FReply::Handled();}auto& Manager=FTextLocalizationManager::Get();if(Manager.GetNativeCultureName(ELocalizedTextSourceCategory::Game).IsEmpty()){LanguageStatus=TEXT("尚未配置游戏本地化目标：请先在 Localization Dashboard 设置原生语言并编译译文");return FReply::Handled();}if(!OwnsLanguage){PreviousLanguageEnabled=Manager.IsGameLocalizationPreviewEnabled();PreviousLanguage=Manager.GetConfiguredGameLocalizationPreviewLanguage();OwnsLanguage=true;}Manager.EnableGameLocalizationPreview(Language);LanguageStatus=TEXT("游戏文本预览语言：")+Language+TEXT("；未翻译条目显示原文");return FReply::Handled();})]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("恢复语言"))).OnClicked_Lambda([](){RestoreLanguage();return FReply::Handled();})]]
      +SVerticalBox::Slot().AutoHeight().Padding(8,2)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([](){return FText::FromString(LanguageStatus);})]
      +SVerticalBox::Slot().FillHeight(.55f).Padding(8)[SNew(SBorder).BorderBackgroundColor(FLinearColor(.07,.07,.07,1))[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1920).HeightOverride(1080)[Content]]]]
      +SVerticalBox::Slot().AutoHeight().Padding(8,2)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("下方是单轨道样式。默认使用全局样式；需要单独修改时勾选 Override Global Style，再保存 Sequence。"))) ]
      +SVerticalBox::Slot().FillHeight(.45f).Padding(8)[View]];
    Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>){Enabled=false;RestoreLanguage();if(Widget.IsValid())Widget->SetLine(nullptr);Widget.Reset();Details.Reset();LastSequence.Reset();LastTrack.Reset();}));
    return Tab;
}
}
void Startup(){if(!IsRunningCommandlet()){FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId,FOnSpawnTab::CreateStatic(&Spawn)).SetDisplayName(FText::FromString(TEXT("字幕预览与样式")));FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GlobalTabId,FOnSpawnTab::CreateStatic(&SpawnGlobal)).SetDisplayName(FText::FromString(TEXT("全局字幕样式")));}}
void Show(TSharedRef<ISequencer> Sequencer){Current=Sequencer;FGlobalTabmanager::Get()->TryInvokeTab(TabId);Enabled=true;LastSequence.Reset();Update(Sequencer);}
void Update(TSharedPtr<ISequencer> Sequencer)
{
    if(!Widget.IsValid() || !Sequencer)return;
    auto Previous=Current.Pin();if(Previous && Previous!=Sequencer)return;Current=Sequencer;
    auto* Seq=Cast<ULevelSequence>(Sequencer->GetFocusedMovieSceneSequence());
    UMovieSceneVTGSubtitleTrack* Track=nullptr;
    if(Seq)for(auto* T:Seq->GetMovieScene()->GetTracks())if((Track=Cast<UMovieSceneVTGSubtitleTrack>(T)))break;
    if(Seq!=LastSequence.Get() || Track!=LastTrack.Get())
    {
        LastSequence=Seq;LastTrack=Track;
        if(auto View=Details.Pin())View->SetObject(Track);
    }
    Widget->SetLine(Enabled && Seq?UVTGSubtitleSubsystem::Resolve(Seq,Sequencer->GetLocalTime().Time):nullptr);
}
void Release(TSharedPtr<ISequencer> Sequencer){if(Current.Pin()!=Sequencer)return;if(Widget.IsValid())Widget->SetLine(nullptr);if(auto View=Details.Pin())View->SetObject(nullptr);Current.Reset();LastSequence.Reset();LastTrack.Reset();}
void SetEnabled(bool Value){Enabled=Value;if(!Value && Widget.IsValid())Widget->SetLine(nullptr);}
FText DisplayedText(){if(Widget.IsValid())if(auto* Text=Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("SubtitleText"))))return Text->GetText();return FText::GetEmpty();}
void ShowGlobalStyle(){FGlobalTabmanager::Get()->TryInvokeTab(GlobalTabId);}
void Shutdown(){if(!IsRunningCommandlet()){if(auto Tab=FGlobalTabmanager::Get()->FindExistingLiveTab(TabId))Tab->RequestCloseTab();if(auto GlobalTab=FGlobalTabmanager::Get()->FindExistingLiveTab(GlobalTabId))GlobalTab->RequestCloseTab();FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GlobalTabId);}RestoreLanguage();Widget.Reset();Details.Reset();Current.Reset();LastSequence.Reset();LastTrack.Reset();}
}



