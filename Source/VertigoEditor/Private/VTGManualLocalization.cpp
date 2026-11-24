#include "VTGManualLocalization.h"
#include "SVTGLocalizationTextBox.h"
#include "NarrativeDialogueDetailsExtension.h"
#include "DialogueSM.h"
#include "Dialogue.h"
#include "QuestSM.h"
#include "QuestTask.h"
#include "NarrativeQuestDetailsExtension.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Layout/WidgetPath.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Internationalization/Text.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/Regex.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "UObject/UObjectHash.h"
#include "UI/VTGLocalizationSettings.h"
#include "UI/VTGLocalizationSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"
#include "Internationalization/Internationalization.h"
#include "VTGDialogueLocalizationDetails.h"
#include "VTGSubtitleLocalizationDetails.h"
#include "VTGQuestLocalizationDetails.h"
#include "VTGMapLocalizationDetails.h"
#include "VTGHintLocalizationDetails.h"
#include "VTGAssetDisplayLocalizationDetails.h"
#include "Engine/DataTable.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "ISinglePropertyView.h"

namespace VTGManualLocalization
{
namespace
{
class FLocalizationSelectAll : public IInputProcessor
{
public:
 void Tick(const float,FSlateApplication&,TSharedRef<ICursor>) override {}
 bool HandleMouseButtonDoubleClickEvent(FSlateApplication& App,const FPointerEvent& Event) override
 {
  if(Event.GetEffectingButton()!=EKeys::LeftMouseButton)return false;
  auto Path=App.LocateWindowUnderMouse(Event.GetScreenSpacePosition(),App.GetInteractiveTopLevelWindows());
  for(int32 I=0;I<Path.Widgets.Num();++I)if(Path.Widgets[I].Widget->GetTypeAsString()==TEXT("SVTGLocalizationTextBox"))
  {auto Box=StaticCastSharedRef<SVTGLocalizationTextBox>(Path.Widgets[I].Widget);App.SetKeyboardFocus(Box,EFocusCause::Mouse);Box->SelectAllText();return true;}
  return false;
 }
};
TSharedPtr<FLocalizationSelectAll> SelectAllInput;
const FName TabId(TEXT("VTG.ManualLocalization"));
// Set only by a gather pass that sees one key carry two sources, and cleared by the next pass that
// does not. Kept here so both sides test the exact same string.
const FString ConflictLockReason(TEXT("警示：同一个文本键对应多个不同原文。请在原资产修复键冲突后再翻译。"));
TArray<FString> Cultures(){return GetDefault<UVTGLocalizationSettings>()->TargetLanguages;}
struct FTranslation { FString Text, ReviewedSource; };
struct FEntry
{
    FString Namespace, Key, Source, Context, LockReason;
    bool Approved=false;
    TMap<FString,FTranslation> Translations;
};
TArray<TSharedPtr<FEntry>> Entries;
TArray<TSharedPtr<FEntry>> VisibleEntries;
TSharedPtr<SComboBox<TSharedPtr<FEntry>>> CatalogCombo;
FString Search;
TSharedPtr<FEntry> Selected;
TSharedPtr<SVerticalBox> Body;
TWeakPtr<IPropertyHandle> ActiveProperty;
FString LockedReason, Status;
FDelegateHandle ExtensionHandle;
bool Loaded=false;
FString CatalogSnapshot;
#if WITH_DEV_AUTOMATION_TESTS
FString TestStorageDirectory;
#endif
TArray<FString> ProtectedTokens(const FString& Source);
void RefreshCatalog()
{
    VisibleEntries.Reset();
    for(auto E:Entries)if(Search.IsEmpty() || E->Source.Contains(Search) || E->Context.Contains(Search) || E->Namespace.Contains(Search) || E->Key.Contains(Search))VisibleEntries.Add(E);
    if(CatalogCombo)CatalogCombo->RefreshOptions();
}
FString CatalogPath(){
#if WITH_DEV_AUTOMATION_TESTS
    if(!TestStorageDirectory.IsEmpty())return TestStorageDirectory/TEXT("ManualTranslations.json");
#endif
    return FPaths::ProjectDir()/TEXT("Localization/ManualTranslations.json");
}
FString ResourcePath(){
#if WITH_DEV_AUTOMATION_TESTS
    if(!TestStorageDirectory.IsEmpty())return TestStorageDirectory/TEXT("Resources");
#endif
    return FPaths::ProjectContentDir()/TEXT("Localization/Vertigo");
}

TSharedPtr<FEntry> Find(const FString& Namespace,const FString& Key)
{
    for(auto E:Entries)if(E->Namespace==Namespace && E->Key==Key)return E;
    return nullptr;
}

bool WriteCatalog(FString& Error)
{
    FString Current;
    if(IFileManager::Get().FileExists(*CatalogPath()) && !FFileHelper::LoadFileToString(Current,*CatalogPath()))
    {Error=TEXT("无法读取已有翻译表，已阻止覆盖。");return false;}
    if(Current!=CatalogSnapshot){Error=TEXT("翻译表已被其他编辑器或外部工具修改，已阻止覆盖。请重启编辑器重新加载，或先合并文件。");return false;}
    TSharedRef<FJsonObject> Root=MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("version"),1);
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(auto E:Entries)
    {
        auto Row=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("namespace"),E->Namespace);Row->SetStringField(TEXT("key"),E->Key);
        Row->SetStringField(TEXT("source"),E->Source);Row->SetStringField(TEXT("context"),E->Context);
        Row->SetBoolField(TEXT("approvedDisplayText"),E->Approved);
        Row->SetStringField(TEXT("lockReason"),E->LockReason);
        auto Translations=MakeShared<FJsonObject>();
        for(const auto& Pair:E->Translations)
        {
            auto T=MakeShared<FJsonObject>();T->SetStringField(TEXT("text"),Pair.Value.Text);T->SetStringField(TEXT("reviewedSource"),Pair.Value.ReviewedSource);
            Translations->SetObjectField(Pair.Key,T);
        }
        Row->SetObjectField(TEXT("translations"),Translations);Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("entries"),Rows);
    FString Json;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(CatalogPath()),true);
    // Atomic replacement avoids truncating the previous catalog on an interrupted write.
    const FString Temp=CatalogPath()+TEXT(".tmp");
    if(!FFileHelper::SaveStringToFile(Json,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) || !IFileManager::Get().Move(*CatalogPath(),*Temp,true,true))
    {Error=TEXT("无法保存翻译表，请检查写入权限或版本控制锁定。");return false;}
    CatalogSnapshot=Json;return true;
}

void GatherCode()
{
    TArray<FString> Files;IFileManager::Get().FindFilesRecursive(Files,*(FPaths::ProjectDir()/TEXT("Source/Vertigo")),TEXT("*.cpp"),true,false);
    for(const auto& File:Files)
    {
        if(File.Contains(TEXT("Preview")))continue;
        FString Code;if(!FFileHelper::LoadFileToString(Code,*File))continue;
        FRegexMatcher M(FRegexPattern(TEXT("NSLOCTEXT\\s*\\(\\s*\"([^\"]*)\"\\s*,\\s*\"([^\"]*)\"\\s*,\\s*\"((?:\\\\.|[^\"\\\\])*)\"\\s*\\)")),Code);
        while(M.FindNext())
        {
            auto E=Find(M.GetCaptureGroup(1),M.GetCaptureGroup(2));
            if(!E){E=MakeShared<FEntry>();E->Namespace=M.GetCaptureGroup(1);E->Key=M.GetCaptureGroup(2);Entries.Add(E);}
            E->Source=M.GetCaptureGroup(3).ReplaceEscapedCharWithChar();
            E->Context=File;
        }
    }
}

bool Load(FString& Error)
{
    if(Loaded)return true;
    FString Json;
    if(IFileManager::Get().FileExists(*CatalogPath()))
    {
        TSharedPtr<FJsonObject> Root;
        if(!FFileHelper::LoadFileToString(Json,*CatalogPath()) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root) || !Root.IsValid())
        {Error=TEXT("翻译表读取失败，已禁止写入，避免覆盖已有内容。");return false;}
        const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
        if(!Root->TryGetArrayField(TEXT("entries"),Rows)){Error=TEXT("翻译表格式错误。");return false;}
        TArray<TSharedPtr<FEntry>> Parsed;
        for(const auto& Value:*Rows)
        {
            const auto Row=Value->AsObject();if(!Row.IsValid()){Error=TEXT("无效翻译条目。");return false;}
            auto E=MakeShared<FEntry>();
            if(!Row->TryGetStringField(TEXT("namespace"),E->Namespace)||!Row->TryGetStringField(TEXT("key"),E->Key)||!Row->TryGetStringField(TEXT("source"),E->Source)||E->Key.IsEmpty())
            {Error=TEXT("翻译条目缺少原文或稳定标识。");return false;}
            for(auto Other:Parsed)if(Other->Namespace==E->Namespace && Other->Key==E->Key){Error=TEXT("翻译表包含重复标识，已锁定。");return false;}
            Row->TryGetStringField(TEXT("context"),E->Context);Row->TryGetBoolField(TEXT("approvedDisplayText"),E->Approved);
            Row->TryGetStringField(TEXT("lockReason"),E->LockReason);
            const TSharedPtr<FJsonObject>* Translations=nullptr;
            if(Row->TryGetObjectField(TEXT("translations"),Translations))for(const auto& Pair:(*Translations)->Values)
            {
                auto T=Pair.Value->AsObject();if(!T.IsValid()){Error=TEXT("译文格式错误。");return false;}
                FTranslation Translation;
                if(!T->TryGetStringField(TEXT("text"),Translation.Text)||!T->TryGetStringField(TEXT("reviewedSource"),Translation.ReviewedSource)){Error=TEXT("译文缺少复核原文。");return false;}
                E->Translations.Add(Pair.Key,Translation);
            }
            Parsed.Add(E);
        }
        Entries=MoveTemp(Parsed);
    }
    // C++ authored labels have no asset field. Expose NSLOCTEXT labels here with read-only Origin.
    CatalogSnapshot=Json;GatherCode();
    Loaded=true;return true;
}

FString TranslationStatus(const FEntry& E,const FString& Culture)
{
    const auto* T=E.Translations.Find(Culture);
    if(!T || T->Text.IsEmpty())return TEXT("未填写；显示 Origin 回退");
    if(T->ReviewedSource!=E.Source)return TEXT("原文已变更，待复核；不会编译旧译文");
    return TEXT("已手工填写并复核");
}

bool IsCurrentSelection()
{
    auto Handle=ActiveProperty.Pin();if(!Handle)return true;
    FText Text;if(Handle->GetValue(Text)!=FPropertyAccess::Success)return false;
    auto NS=FTextInspector::GetNamespace(Text),Key=FTextInspector::GetKey(Text);
    const auto* Source=FTextInspector::GetSourceString(Text);
    return Selected && NS.IsSet() && Key.IsSet() && Source && TextNamespaceUtil::StripPackageNamespace(NS.GetValue())==Selected->Namespace && Key.GetValue()==Selected->Key && *Source==Selected->Source;
}

void Rebuild()
{
    if(!Body)return;Body->ClearChildren();
    Body->AddSlot().AutoHeight().Padding(6)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("只手工填写译文。此面板不会修改 Origin、变量、文本键或原资产。预览请使用编辑器游戏语言预览或字幕预览面板。")))];
    if(!LockedReason.IsEmpty())Body->AddSlot().AutoHeight().Padding(6)[SNew(STextBlock).ColorAndOpacity(FLinearColor(1,.55f,.1f)).AutoWrapText(true).Text(FText::FromString(LockedReason))];
    if(!Selected)return;
    const auto E=Selected;
    Body->AddSlot().AutoHeight().Padding(6)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(E->Context+TEXT("\n只读 Namespace: ")+E->Namespace+TEXT("\n只读 Key: ")+E->Key))];
    Body->AddSlot().AutoHeight().Padding(6)[SNew(STextBlock).Text(FText::FromString(TEXT("Origin（原文只读，在原资产或源码中编辑）")))];
    Body->AddSlot().AutoHeight().Padding(6)[SNew(SVTGLocalizationTextBox).IsReadOnly(true).Text(FText::AsCultureInvariant(E->Source))];
    FString Legend;
    const auto Tokens=ProtectedTokens(E->Source);
    for(int32 Index=0;Index<Tokens.Num();++Index)Legend+=FString::Printf(TEXT("⟦%d⟧ = %s\n"),Index+1,*Tokens[Index]);
    if(!Legend.IsEmpty())Body->AddSlot().AutoHeight().Padding(6)[SNew(STextBlock).AutoWrapText(true).Text(FText::AsCultureInvariant(TEXT("受保护结构（只读）：\n")+Legend))];
    if(LockedReason.IsEmpty())Body->AddSlot().AutoHeight().Padding(6)[SNew(SCheckBox)
        .IsChecked_Lambda([E](){return E->Approved?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
        .OnCheckStateChanged_Lambda([E](ECheckBoxState State){E->Approved=State==ECheckBoxState::Checked;})
        [SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("我已检查：这是玩家显示文本，功能不依赖其显示内容（未知字段默认锁定）")))]];
    for(const FString& Culture:Cultures())
    {
        Body->AddSlot().AutoHeight().Padding(6)[SNew(STextBlock).Text_Lambda([E,Culture](){return FText::FromString(Culture+TEXT(" — ")+TranslationStatus(*E,Culture));})];
        Body->AddSlot().AutoHeight().Padding(6)[SNew(STextBlock).AutoWrapText(true).Text(FText::AsCultureInvariant(TEXT("结构参考（可移动 ⟦编号⟧，不要改编号；实际变量和标签不在译文编辑区）：\n")+EncodeDraft(E->Source,E->Source)))];
        auto Box=SNew(SVTGLocalizationTextBox).Text(FText::AsCultureInvariant(EncodeDraft(E->Source,E->Translations.FindRef(Culture).Text)))
            .IsReadOnly_Lambda([E](){return !E->Approved || !LockedReason.IsEmpty();});
        Body->AddSlot().AutoHeight().Padding(6)[Box];
        Body->AddSlot().AutoHeight().Padding(6)[SNew(SButton).Text(FText::FromString(TEXT("复制 Origin 作为手工填写模板（不会自动保存）")))
            .IsEnabled_Lambda([E](){return E->Approved && LockedReason.IsEmpty();})
            .OnClicked_Lambda([E,Box](){Box->SetText(FText::AsCultureInvariant(EncodeDraft(E->Source,E->Source)));return FReply::Handled();})];
        Body->AddSlot().AutoHeight().Padding(6)[SNew(SButton).Text(FText::FromString(TEXT("校验并保存此语言（确认对应当前 Origin）")))
            .IsEnabled_Lambda([E](){return E->Approved && LockedReason.IsEmpty();})
            .OnClicked_Lambda([E,Culture,Box](){
                if(!IsCurrentSelection()){Status=TEXT("原文或选择已改变，请重新从字段打开，防止保存到错误条目。");return FReply::Handled();}
                FString Error,Value;
                if(!DecodeDraft(E->Source,Box->GetText().ToString(),Value,Error)){Status=Error;return FReply::Handled();}
                if(!Validate(E->Source,Value,Error)){Status=Error;return FReply::Handled();}
                const auto Previous=E->Translations;E->Translations.Add(Culture,{Value,E->Source});
                if(!WriteCatalog(Error)){E->Translations=Previous;Status=Error;}else Status=TEXT("已保存手工译文。点击编译使游戏语言预览生效。");
                return FReply::Handled();})];
    }
}

void SelectProperty(TSharedPtr<IPropertyHandle> Handle,bool OpenTab=true)
{
    Status.Empty();LockedReason.Empty();Selected.Reset();ActiveProperty=Handle;
    if(!Load(Status))return;
    FProperty* P=Handle.IsValid()?Handle->GetProperty():nullptr;
    if(!P || !CastField<FTextProperty>(P))LockedReason=TEXT("警示：String / Name 可能是逻辑标识。只读，不为其直接创建翻译。请为玩家显示增加独立 FText 字段。");
    else
    {
        FText Text;
        if(Handle->GetValue(Text)!=FPropertyAccess::Success)LockedReason=TEXT("请选择一个对象的单一文本字段。多选或失效属性已锁定。");
        else
        {
            auto NS=FTextInspector::GetNamespace(Text),Key=FTextInspector::GetKey(Text);const auto* Source=FTextInspector::GetSourceString(Text);
            if(!NS.IsSet() || !Key.IsSet() || Key.GetValue().IsEmpty() || !Source || Text.IsCultureInvariant())LockedReason=TEXT("警示：此字段没有稳定本地化键，可能是动态结果或不可本地化文字。请先在原编辑器配置可本地化文本；本面板不会自动改键。");
            else
            {
                FString Namespace=TextNamespaceUtil::StripPackageNamespace(NS.GetValue());
                Selected=Find(Namespace,Key.GetValue());
                if(!Selected){Selected=MakeShared<FEntry>();Selected->Namespace=Namespace;Selected->Key=Key.GetValue();Entries.Add(Selected);}
                Selected->Source=*Source;
                TArray<UObject*> Owners;Handle->GetOuterObjects(Owners);
                Selected->Context=(Owners.Num()?Owners[0]->GetPathName():TEXT("Unknown"))+TEXT(" : ")+Handle->GeneratePathToProperty();
                if(P->HasMetaData(TEXT("VTGLocalizationLocked")))Selected->LockReason=P->GetMetaData(TEXT("VTGLocalizationLocked"));
                LockedReason=Selected->LockReason;
            }
        }
    }
    RefreshCatalog();if(OpenTab)FGlobalTabmanager::Get()->TryInvokeTab(TabId);Rebuild();
}

TSharedRef<SDockTab> Spawn(const FSpawnTabArgs&)
{
    Load(Status);RefreshCatalog();
    auto Tab=SNew(SDockTab).TabRole(ETabRole::NomadTab)
    [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("在字幕 Section、Narrative 节点、UMG 控件或 Data Asset 的 Details 字段右侧点击多语言按钮。也可从下拉列表选择已有条目和 C++ 标签。")))]
        +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(SButton).Text(FText::FromString(TEXT("收集 Content Browser 当前选中资产的文本（只读扫描）"))).OnClicked_Lambda([](){
            TArray<FAssetData> Assets;FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Assets);
            FString Error;for(const auto& Asset:Assets)if(!GatherAsset(Asset.GetAsset(),Error)){Status=Error;return FReply::Handled();}
            RefreshCatalog();Status=Assets.IsEmpty()?TEXT("请先在 Content Browser 选择资产。"):TEXT("已收集文本，未知字段仍需确认用途。资产没有被修改。");return FReply::Handled();})]
        +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(SSearchBox).HintText(FText::FromString(TEXT("搜索原文、资产路径或文本键"))).OnTextChanged_Lambda([](const FText& Value){Search=Value.ToString();RefreshCatalog();})]
        +SVerticalBox::Slot().AutoHeight().Padding(6)[SAssignNew(CatalogCombo,SComboBox<TSharedPtr<FEntry>>).OptionsSource(&VisibleEntries)
            .OnGenerateWidget_Lambda([](TSharedPtr<FEntry> E){return SNew(STextBlock).Text(FText::FromString(E->Source.Left(65)+TEXT("  — ")+E->Namespace+TEXT(" / ")+E->Key));})
            .OnSelectionChanged_Lambda([](TSharedPtr<FEntry> E,ESelectInfo::Type){Selected=E;ActiveProperty.Reset();LockedReason=E?E->LockReason:FString();Status.Empty();Rebuild();})
            [SNew(STextBlock).Text_Lambda([](){return FText::FromString(Selected?Selected->Namespace+TEXT(" / ")+Selected->Key:TEXT("选择文本条目"));})]]
        +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Body,SVerticalBox)]]
        +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(SButton).Text(FText::FromString(TEXT("保存审核状态并编译全部手工译文"))).OnClicked_Lambda([](){FString Error;if(Compile(Error))Status=TEXT("已编译；缺失或待复核译文回退 Origin。未生成任何译文。");else Status=Error;return FReply::Handled();})]
        +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([](){return FText::FromString(Status);})]];
    Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>){Body.Reset();CatalogCombo.Reset();ActiveProperty.Reset();}));
    Rebuild();return Tab;
}
}
#if WITH_DEV_AUTOMATION_TESTS
TSharedRef<SDockTab> MakeTestPanel(TSharedPtr<IPropertyHandle> Handle)
{
    SelectProperty(Handle,false);
    return Spawn(FSpawnTabArgs(nullptr,FTabId(TabId)));
}
#endif

namespace
{
TArray<FString> ProtectedTokens(const FString& Source)
{
    TArray<FString> Tokens;FRegexMatcher M(FRegexPattern(TEXT("\\{[^{}]*\\}|<[^>]*>")),Source);
    while(M.FindNext())Tokens.AddUnique(M.GetCaptureGroup(0));return Tokens;
}
}
FString EncodeDraft(const FString& Source,const FString& Translation)
{
    FString Draft=Translation;const auto Tokens=ProtectedTokens(Source);
    for(int32 Index=0;Index<Tokens.Num();++Index)Draft=Draft.Replace(*Tokens[Index],*FString::Printf(TEXT("⟦%d⟧"),Index+1),ESearchCase::CaseSensitive);
    return Draft;
}
bool DecodeDraft(const FString& Source,const FString& Draft,FString& Translation,FString& Error)
{
    Error.Empty();Translation.Empty();if(Draft.IsEmpty())return true;
    if(!ProtectedTokens(Draft).IsEmpty()){Error=TEXT("不要在译文中输入实际变量或富文本标签。请使用结构参考中的 ⟦编号⟧，原结构由程序管理。");return false;}
    Translation=Draft;const auto Tokens=ProtectedTokens(Source);
    for(int32 Index=0;Index<Tokens.Num();++Index)Translation=Translation.Replace(*FString::Printf(TEXT("⟦%d⟧"),Index+1),*Tokens[Index],ESearchCase::CaseSensitive);
    if(Translation.Contains(TEXT("⟦")) || Translation.Contains(TEXT("⟧"))){Error=TEXT("受保护标记编号无效，保存被阻止。");Translation.Empty();return false;}
    return Validate(Source,Translation,Error);
}

bool Validate(const FString& Source,const FString& Translation,FString& Error)
{
    Error.Empty();if(Translation.IsEmpty())return true; // Explicitly clear a translation to restore fallback.
    auto Tokens=[](const FString& Text,const FString& Pattern){TArray<FString> Result;FRegexMatcher M(FRegexPattern(Pattern),Text);while(M.FindNext())Result.Add(M.GetCaptureGroup(0));return Result;};
    // Literal protected tokens are preserved, including occurrences. Order of placeholders may change.
    auto SourceArgs=Tokens(Source,TEXT("\\{[^{}]*\\}")),TranslatedArgs=Tokens(Translation,TEXT("\\{[^{}]*\\}"));
    SourceArgs.Sort();TranslatedArgs.Sort();
    if(SourceArgs!=TranslatedArgs){Error=TEXT("保存被阻止：占位符名称或出现次数发生变化。请保留 Origin 中的所有 {变量}。");return false;}
    auto Count=[](const FString& Text,TCHAR Character){int32 N=0;for(TCHAR C:Text)if(C==Character)++N;return N;};
    for(TCHAR C:{TEXT('{'),TEXT('}')})if(Count(Source,C)!=Count(Translation,C)){Error=TEXT("保存被阻止：变量括号结构发生变化。");return false;}
    // Restrict tag structure to the exact source sequence. Prose and tag positions remain editable.
    if(Tokens(Source,TEXT("<[^>]*>"))!=Tokens(Translation,TEXT("<[^>]*>"))){Error=TEXT("保存被阻止：富文本标签被修改、删除或重排。请保留完整标签及嵌套顺序。");return false;}
    FTextFormat Format(FText::AsCultureInvariant(Translation));
    if(!Format.IsValid()){Error=TEXT("保存被阻止：文本格式无效（检查括号与转义）。");return false;}
    return true;
}

bool Compile(FString& Error)
{
    if(!Load(Error))return false;
    const auto Languages=Cultures();
    TSet<FString> UniqueLanguages;
    for(const auto& Language:Languages)
    {
        if(Language==TEXT("en") || Language==TEXT("origin") || !FInternationalization::Get().GetCulture(Language).IsValid() || UniqueLanguages.Contains(Language))
        {Error=TEXT("目标语言代码无效、重复或占用 Origin 的 en：")+Language;return false;}
        UniqueLanguages.Add(Language);
    }
    if(Languages.IsEmpty() || !Languages.Contains(GetDefault<UVTGLocalizationSettings>()->DefaultReleaseLanguage)){Error=TEXT("请在 Project Settings → Game → Manual Localization 设置目标语言和其中一个发行默认语言。");return false;}
    GatherCode();
    // Refresh touched asset packages before comparing the review snapshots. Never save packages.
    TSet<FString> Packages;
    for(auto E:Entries)if(E->Context.StartsWith(TEXT("/Game/")))
    {
        FString Owner,Property;E->Context.Split(TEXT(" : "),&Owner,&Property);
        FString Package,Rest;if(Owner.Split(TEXT("."),&Package,&Rest))Packages.Add(Package);
    }
    for(const auto& PackageName:Packages)
    {
        auto* Package=LoadPackage(nullptr,*PackageName,LOAD_NoWarn);
        if(!Package || !GatherAsset(Package,Error)){Error=TEXT("无法复核原资产：")+PackageName+TEXT("。编译已停止。");return false;}
    }
    FTextLocalizationResource Native;
    TMap<FString,FTextLocalizationResource> Resources;
    for(const auto& Culture:Languages)Resources.Add(Culture);
    for(auto E:Entries)
    {
        if(!E->Approved || !E->LockReason.IsEmpty())continue;
        Native.AddEntry(FTextKey(E->Namespace),FTextKey(E->Key),E->Source,E->Source,0);
        for(const auto& Culture:Languages)
        {
            auto* T=E->Translations.Find(Culture);
            if(!T || T->Text.IsEmpty() || T->ReviewedSource!=E->Source){Resources.FindChecked(Culture).AddEntry(FTextKey(E->Namespace),FTextKey(E->Key),E->Source,E->Source,0);continue;}
            if(!Validate(E->Source,T->Text,Error)){Error=E->Namespace+TEXT(" / ")+E->Key+TEXT(": ")+Error;return false;}
            Resources.FindChecked(Culture).AddEntry(FTextKey(E->Namespace),FTextKey(E->Key),E->Source,T->Text,0);
        }
    }
    if(!WriteCatalog(Error))return false;
    IFileManager::Get().MakeDirectory(*(ResourcePath()/TEXT("en")),true);
    if(!Native.SaveToFile(ResourcePath()/TEXT("en/Vertigo.locres"))){Error=TEXT("无法写入 Origin 资源。");return false;}
    for(auto& Pair:Resources)
    {
        IFileManager::Get().MakeDirectory(*(ResourcePath()/Pair.Key),true);
        if(!Pair.Value.SaveToFile(ResourcePath()/Pair.Key/TEXT("Vertigo.locres"))){Error=TEXT("无法写入语言资源：")+Pair.Key;return false;}
    }
    FTextLocalizationMetaDataResource Meta;Meta.NativeCulture=TEXT("en");Meta.NativeLocRes=TEXT("en/Vertigo.locres");Meta.CompiledCultures=Languages;Meta.CompiledCultures.Add(TEXT("en"));
    if(!Meta.SaveToFile(ResourcePath()/TEXT("Vertigo.locmeta"))){Error=TEXT("无法写入语言资源元数据。");return false;}
    TextLocalizationResourceUtil::ClearNativeProjectCultureName();
    auto& Manager=FTextLocalizationManager::Get();
    const bool WasPreviewing=Manager.IsGameLocalizationPreviewEnabled();
    const FString PreviewLanguage=Manager.GetConfiguredGameLocalizationPreviewLanguage();
    Manager.RefreshResources();Manager.WaitForAsyncTasks();
    bool RestoredGameLanguage=false;
#if WITH_DEV_AUTOMATION_TESTS
    if(TestStorageDirectory.IsEmpty())
#endif
    {
        for(TObjectIterator<UGameInstance> It;It;++It)
            if(!It->IsTemplate() && It->GetWorld() && It->GetWorld()->IsGameWorld())
                if(auto* LiveLanguages=It->GetSubsystem<UVTGLocalizationSubsystem>())
                    if(!LiveLanguages->GetLanguage().IsEmpty()){RestoredGameLanguage=LiveLanguages->SetLanguage(LiveLanguages->GetLanguage());break;}
        if(!RestoredGameLanguage && GIsEditor)
        {
            if(WasPreviewing)Manager.EnableGameLocalizationPreview(PreviewLanguage);else Manager.DisableGameLocalizationPreview();
            Manager.WaitForAsyncTasks();
        }
    }
    return true;
}

bool GatherAsset(UObject* Asset,FString& Error)
{
    if(!Asset){Error=TEXT("无法加载所选资产。");return false;}
    if(!Load(Error))return false;
    TArray<UObject*> Objects;GetObjectsWithPackage(Asset->GetOutermost(),Objects,true);
    TMap<FString,FString> SeenSources;
    TFunction<void(FProperty*,void*,const FString&)> Visit;
    Visit=[&](FProperty* Property,void* Value,const FString& Context)
    {
        if(auto* TextProperty=CastField<FTextProperty>(Property))
        {
            const FText Text=TextProperty->GetPropertyValue(Value);
            auto NS=FTextInspector::GetNamespace(Text),Key=FTextInspector::GetKey(Text);const auto* Source=FTextInspector::GetSourceString(Text);
            if(!NS.IsSet() || !Key.IsSet() || Key.GetValue().IsEmpty() || !Source || Text.IsCultureInvariant() || Source->IsEmpty())return;
            const FString Namespace=TextNamespaceUtil::StripPackageNamespace(NS.GetValue());
            auto E=Find(Namespace,Key.GetValue());
            if(!E){E=MakeShared<FEntry>();E->Namespace=Namespace;E->Key=Key.GetValue();Entries.Add(E);}
            const FString Identity=Namespace+TEXT("\x1f")+Key.GetValue();
            const auto* Seen=SeenSources.Find(Identity);
            // A conflict lock states what this pass just saw. Drop a stale one from an earlier pass
            // before re-judging, or a fixed asset would stay locked out of translation forever.
            if(!Seen && E->LockReason==ConflictLockReason)E->LockReason.Empty();
            if(Seen && *Seen!=*Source){E->LockReason=ConflictLockReason;E->Approved=false;}
            SeenSources.Add(Identity,*Source);
            E->Source=*Source;E->Context=Context;
            if(Property->HasMetaData(TEXT("VTGLocalizationLocked")))E->LockReason=Property->GetMetaData(TEXT("VTGLocalizationLocked"));
        }
        else if(auto* Struct=CastField<FStructProperty>(Property))
        {
            for(TFieldIterator<FProperty> It(Struct->Struct);It;++It)for(int32 Index=0;Index<It->ArrayDim;++Index)
                Visit(*It,It->ContainerPtrToValuePtr<void>(Value,Index),Context+TEXT(".")+It->GetName());
        }
        else if(auto* Array=CastField<FArrayProperty>(Property))
        {
            FScriptArrayHelper Helper(Array,Value);for(int32 Index=0;Index<Helper.Num();++Index)Visit(Array->Inner,Helper.GetRawPtr(Index),Context+FString::Printf(TEXT("[%d]"),Index));
        }
        else if(auto* Map=CastField<FMapProperty>(Property))
        {
            FScriptMapHelper Helper(Map,Value);for(int32 Index=0;Index<Helper.GetMaxIndex();++Index)if(Helper.IsValidIndex(Index))
            {
                Visit(Map->KeyProp,Helper.GetKeyPtr(Index),Context+FString::Printf(TEXT("[%d].Key"),Index));
                Visit(Map->ValueProp,Helper.GetValuePtr(Index),Context+FString::Printf(TEXT("[%d].Value"),Index));
            }
        }
    };
    for(auto* Object:Objects)
    {
        for(TFieldIterator<FProperty> It(Object->GetClass());It;++It)for(int32 Index=0;Index<It->ArrayDim;++Index)
            Visit(*It,It->ContainerPtrToValuePtr<void>(Object,Index),Object->GetPathName()+TEXT(" : ")+It->GetName());
        // DataTable rows are raw struct memory, not reflected UObject properties.
        if(auto* Table=Cast<UDataTable>(Object))if(Table->GetRowStruct())
            for(const auto& Row:Table->GetRowMap())for(TFieldIterator<FProperty> It(Table->GetRowStruct());It;++It)
                for(int32 Index=0;Index<It->ArrayDim;++Index)
                    Visit(*It,It->ContainerPtrToValuePtr<void>(Row.Value,Index),Table->GetPathName()+TEXT(" : Row[")+Row.Key.ToString()+TEXT("].")+It->GetName());
    }
    return true;
}

/** Ticks the panel checkbox for one entry; without it Compile skips the entry entirely. */
bool ApproveDisplayText(const FString& Namespace,const FString& Key,FString& Error)
{
    if(!Load(Error))return false;
    auto E=Find(Namespace,Key);
    if(!E){Error=FString::Printf(TEXT("未收集到 %s/%s。"),*Namespace,*Key);return false;}
    if(E->Approved)return true;
    E->Approved=true;
    return WriteCatalog(Error);
}

/**
 * Fills a translation field the way the panel's save does. ReviewedSource is the entry's Origin as
 * it stands now: Compile drops any translation whose ReviewedSource no longer matches, which is how
 * a reworded Origin stops shipping its old translation.
 */
bool SetTranslation(const FString& Namespace,const FString& Key,const FString& Culture,const FString& Text,FString& Error)
{
    if(!Load(Error))return false;
    auto E=Find(Namespace,Key);
    if(!E){Error=FString::Printf(TEXT("未收集到 %s/%s。"),*Namespace,*Key);return false;}
    if(!Cultures().Contains(Culture)){Error=TEXT("目标语言未在 Project Settings → Game → Manual Localization 配置：")+Culture;return false;}
    // Mirrors the panel, where an unapproved or locked entry's translation box is read-only.
    if(!E->Approved || !E->LockReason.IsEmpty()){Error=FString::Printf(TEXT("%s/%s 未确认或已锁定，译文不可写。"),*Namespace,*Key);return false;}
    if(!Validate(E->Source,Text,Error)){Error=Namespace+TEXT(" / ")+Key+TEXT(": ")+Error;return false;}
    auto& Translation=E->Translations.FindOrAdd(Culture);
    if(Translation.Text==Text && Translation.ReviewedSource==E->Source)return true;
    Translation.Text=Text;Translation.ReviewedSource=E->Source;
    return WriteCatalog(Error);
}

bool ForgetAsset(const FString& PackageName,int32& OutRemoved,FString& Error)
{
    if(!Load(Error))return false;
    const FString Prefix=PackageName+TEXT(".");
    OutRemoved=Entries.RemoveAll([&Prefix](const TSharedPtr<FEntry>& E){return E.IsValid() && E->Context.StartsWith(Prefix);});
    if(OutRemoved==0)return true;
    if(Selected.IsValid() && !Entries.Contains(Selected))Selected.Reset();
    RefreshCatalog();
    return WriteCatalog(Error);
}

/** The panel gathers and writes in one action; headless tooling needs the same pair. */
bool GatherAssetAndSave(UObject* Asset,FString& Error)
{
    return GatherAsset(Asset,Error) && WriteCatalog(Error);
}

#if WITH_DEV_AUTOMATION_TESTS
bool HasCollectedTextForTests(const FString& Namespace,const FString& Key)
{
    return Find(Namespace,Key).IsValid();
}
void SetTestStorageDirectory(const FString& Directory)
{
    TestStorageDirectory=Directory;Entries.Reset();VisibleEntries.Reset();Selected.Reset();Loaded=false;CatalogSnapshot.Empty();
}
#endif

namespace
{
bool ReadDialogueEntry(TSharedPtr<IPropertyHandle> Property,TSharedPtr<FEntry>& Entry,FString& Error)
{
    Error.Empty();
    if(!VTGDialogueLocalizationDetails::IsSafeText(Property) && !VTGSubtitleLocalizationDetails::IsSafeText(Property) && !VTGQuestLocalizationDetails::IsSafeText(Property) && !VTGMapLocalizationDetails::IsSafeText(Property) && !VTGHintLocalizationDetails::IsSafeText(Property) && !VTGAssetDisplayLocalizationDetails::IsSafeText(Property)){Error=TEXT("此字段未确认是安全的显示文本，翻译区只读。");return false;}
    if(!Load(Error))return false;
    FText Text;
    if(Property->GetValue(Text)!=FPropertyAccess::Success){Error=TEXT("请选择单个对象的文本字段。");return false;}
    const auto Namespace=FTextInspector::GetNamespace(Text),Key=FTextInspector::GetKey(Text);
    const auto* Source=FTextInspector::GetSourceString(Text);
    if(!Source || Source->TrimStartAndEnd().IsEmpty() || !Namespace.IsSet() || !Key.IsSet() || Key.GetValue().IsEmpty() || Text.IsCultureInvariant())
    {Error=TEXT("请先在 Origin 填写原文并提交。空文本保持为空；文本必须具有稳定本地化键。");return false;}
    const FString CleanNamespace=TextNamespaceUtil::StripPackageNamespace(Namespace.GetValue());
    Entry=Find(CleanNamespace,Key.GetValue());
    if(!Entry){Entry=MakeShared<FEntry>();Entry->Namespace=CleanNamespace;Entry->Key=Key.GetValue();Entries.Add(Entry);}
    // A conflict lock outlives the conflict: it is normally written while a dialogue blueprint's node
    // copies disagree mid-edit, and the asset is fixed moments later by saving. Re-gather the owning
    // asset so opening the field is enough to clear it, instead of having to run a full Compile.
    if(Entry->LockReason==ConflictLockReason)
    {
        TArray<UObject*> LockOwners;Property->GetOuterObjects(LockOwners);
        if(LockOwners.Num()==1 && LockOwners[0] && !LockOwners[0]->GetPathName().StartsWith(TEXT("/Engine/Transient")))
        {
            FString GatherError;GatherAsset(LockOwners[0],GatherError);
        }
    }
    if(!Entry->LockReason.IsEmpty()){Error=Entry->LockReason;return false;}
    Entry->Source=*Source;
    TArray<UObject*> Owners;Property->GetOuterObjects(Owners);
    if(Owners.Num()==1)
    {
        // Designer previews are transient copies. They must not replace the persisted
        // asset source path used by Compile to recheck Origin and text identities.
        const FString Path=Owners[0]->GetPathName();
        if(!Path.StartsWith(TEXT("/Engine/Transient")))Entry->Context=Path+TEXT(" : ")+Property->GeneratePathToProperty();
    }
    else if(!VTGAssetDisplayLocalizationDetails::GetDataTableRowContext(Property,Entry->Context)){Error=TEXT("无法定位数据表行来源。");return false;}
    return true;
}
FString PropertySignature(TSharedPtr<IPropertyHandle> Property)
{
    FText Text;if(!Property || Property->GetValue(Text)!=FPropertyAccess::Success)return TEXT("invalid");
    const auto* Source=FTextInspector::GetSourceString(Text);
    return FTextInspector::GetNamespace(Text).Get(FString())+TEXT("\x1f")+FTextInspector::GetKey(Text).Get(FString())+TEXT("\x1f")+(Source?*Source:Text.ToString());
}
}
bool GetDialogueDraft(TSharedPtr<IPropertyHandle> Property,const FString& Culture,FString& Origin,FString& Draft,FString& Error)
{
    Origin.Empty();Draft.Empty();TSharedPtr<FEntry> Entry;
    if(!ReadDialogueEntry(Property,Entry,Error))return false;
    if(!Cultures().Contains(Culture)){Error=TEXT("此语言未加入目标语言列表。");return false;}
    Origin=Entry->Source;
    const auto* Translation=Entry->Translations.Find(Culture);
    // A default copy is an inherited view, not a reviewed translation. Opening details writes nothing.
    Draft=EncodeDraft(Origin,Translation && !Translation->Text.IsEmpty()?Translation->Text:Origin);
    if(Translation && !Translation->Text.IsEmpty() && Translation->ReviewedSource!=Origin)Error=TEXT("Origin 已修改，现有译文待复核。");
    return true;
}
bool SaveDialogueDraft(TSharedPtr<IPropertyHandle> Property,const FString& Culture,const FString& ExpectedOrigin,const FString& Draft,FString& Error)
{
    TSharedPtr<FEntry> Entry;if(!ReadDialogueEntry(Property,Entry,Error))return false;
    if(Entry->Source!=ExpectedOrigin){Error=TEXT("Origin 已改变，已阻止保存旧草稿；请重新加载此文本的翻译区。");return false;}
    if(!Cultures().Contains(Culture)){Error=TEXT("此语言未加入目标语言列表。");return false;}
    FString Translation;if(!DecodeDraft(ExpectedOrigin,Draft,Translation,Error))return false;
    const auto Previous=Entry->Translations;const bool PreviouslyApproved=Entry->Approved;
    Entry->Approved=true;
    if(Translation.TrimStartAndEnd().IsEmpty() || Translation==ExpectedOrigin)Entry->Translations.Remove(Culture);
    else Entry->Translations.Add(Culture,{Translation,ExpectedOrigin});
    if(!WriteCatalog(Error)){Entry->Translations=Previous;Entry->Approved=PreviouslyApproved;return false;}
    RefreshCatalog();return true;
}

namespace
{
struct FBadgeState { int32 Done=0,Total=0; bool Stale=false,Failed=false; };
TArray<FText> NodeTexts(UObject* Node)
{
    TArray<FText> Texts;
    if(auto* Dialogue=Cast<UDialogueNode>(Node))
    {
        Texts.Add(Dialogue->Line.Text);
        for(const auto& Line:Dialogue->AlternativeLines)Texts.Add(Line.Text);
        if(auto* Player=Cast<UDialogueNode_Player>(Node)){Texts.Add(Player->GetRawOptionText());Texts.Add(Player->GetRawHintText());}
    }
    else if(auto* Quest=Cast<UQuestNode>(Node))
    {
        Texts.Add(Quest->Description);
        if(auto* Branch=Cast<UQuestBranch>(Quest))for(auto* Task:Branch->QuestTasks)if(Task)Texts.Add(Task->DescriptionOverride);
    }
    return Texts;
}
FBadgeState ComputeTextBadgeState(const TArray<FText>& Texts)
{
    FBadgeState State;
    FString Error;if(!Load(Error)){State.Failed=true;return State;}
    for(const auto& Text:Texts)
    {
        const auto* Source=FTextInspector::GetSourceString(Text);if(!Source || Source->TrimStartAndEnd().IsEmpty())continue;
        auto E=Find(TextNamespaceUtil::StripPackageNamespace(FTextInspector::GetNamespace(Text).Get(FString())),FTextInspector::GetKey(Text).Get(FString()));
        for(const auto& Culture:Cultures())
        {
            ++State.Total;
            const auto* T=E?E->Translations.Find(Culture):nullptr;
            if(!T || T->Text.IsEmpty())continue;
            if(T->ReviewedSource==*Source)++State.Done;else State.Stale=true;
        }
    }
    return State;
}
FBadgeState ComputeBadgeState(UObject* Node){return ComputeTextBadgeState(NodeTexts(Node));}
FSlateColor TranslationHeaderColor(const FBadgeState& State,bool HasDraft)
{
    if(State.Failed || (!State.Done && !State.Stale))return FSlateColor::UseForeground();
    return State.Done==State.Total && !State.Stale && !HasDraft?FLinearColor(.3f,1,.6f):FLinearColor(1,.35f,.7f);
}

// Evaluate once when selecting a node; do not override the user's later expand/collapse actions.
bool ExpandUntranslatedNode(TSharedPtr<IPropertyHandle> Property)
{
    TArray<UObject*> Owners;Property->GetOuterObjects(Owners);
    if(Owners.Num()!=1)return false;
    const auto State=ComputeBadgeState(Owners[0]);
    FText Text;if(Property->GetValue(Text)!=FPropertyAccess::Success)return false;
    const auto* Source=FTextInspector::GetSourceString(Text);
    return Source && !Source->TrimStartAndEnd().IsEmpty() && !State.Failed && State.Total>0 && !State.Done && !State.Stale;
}
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGLocalizationVisualStateTest,"Vertigo.Localization.VisualStates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGLocalizationVisualStateTest::RunTest(const FString&)
{
    SetTestStorageDirectory(FPaths::ProjectSavedDir()/TEXT("Automation/LocalizationVisuals")/FGuid::NewGuid().ToString());
    ON_SCOPE_EXIT {SetTestStorageDirectory(FString());};
    FString Error;TestTrue(TEXT("Isolated catalog loads"),Load(Error));
    auto* Branch=NewObject<UQuestBranch>();
    const FString NS=TEXT("VTG.Visual.Test");
    Branch->Description=FText::ChangeKey(NS,TEXT("Quest"),FText::FromString(TEXT("Investigate market")));
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    auto View=Module.CreateSingleProperty(Branch,TEXT("Description"),FSinglePropertyParams());
    if(!TestTrue(TEXT("Quest handle available"),View.IsValid()))return false;
    auto Property=View->GetPropertyHandle();
    TestTrue(TEXT("Untranslated node expands"),ExpandUntranslatedNode(Property));
    TestTrue(TEXT("Untranslated headers use default text color"),TranslationHeaderColor(ComputeBadgeState(Branch),false)==FSlateColor::UseForeground());
    TestTrue(TEXT("Unsaved first draft does not color header"),TranslationHeaderColor(ComputeBadgeState(Branch),true)==FSlateColor::UseForeground());
    TestEqual(TEXT("Quest counts each target language"),ComputeBadgeState(Branch).Total,Cultures().Num());
    auto Entry=MakeShared<FEntry>();Entry->Namespace=NS;Entry->Key=TEXT("Quest");Entry->Source=TEXT("Investigate market");Entries.Add(Entry);
    if(Cultures().IsEmpty())return false;
    Entry->Translations.Add(Cultures()[0],{TEXT("译文"),Entry->Source});
    TestFalse(TEXT("Partially translated node stays collapsed"),ExpandUntranslatedNode(Property));
    if(Cultures().Num()>1)TestTrue(TEXT("Partially saved translation is pink"),TranslationHeaderColor(ComputeBadgeState(Branch),false)==FSlateColor(FLinearColor(1,.35f,.7f)));
    for(const auto& Culture:Cultures())Entry->Translations.Add(Culture,{TEXT("译文"),Entry->Source});
    TestEqual(TEXT("All reviewed languages complete"),ComputeBadgeState(Branch).Done,Cultures().Num());
    TestTrue(TEXT("Fully saved translation is green"),TranslationHeaderColor(ComputeBadgeState(Branch),false)==FSlateColor(FLinearColor(.3f,1,.6f)));
    Branch->Description=FText::ChangeKey(NS,TEXT("Quest"),FText::FromString(TEXT("Changed origin")));
    TestTrue(TEXT("Origin change marks saved translations stale"),ComputeBadgeState(Branch).Stale);
    TestFalse(TEXT("Stale translations do not auto-expand"),ExpandUntranslatedNode(Property));
    Branch->Description=FText::GetEmpty();
    TestFalse(TEXT("Empty node stays collapsed"),ExpandUntranslatedNode(Property));
    auto* Player=NewObject<UDialogueNode_Player>();
    Player->Line.Text=FText::ChangeKey(NS,TEXT("Dialogue"),FText::FromString(TEXT("Question")));
    auto LineView=Module.CreateSingleProperty(Player,TEXT("Line"),FSinglePropertyParams());
    if(!TestTrue(TEXT("Dialogue handle available"),LineView.IsValid()))return false;
    TestTrue(TEXT("Untranslated Dialogue also expands"),ExpandUntranslatedNode(LineView->GetPropertyHandle()->GetChildHandle(TEXT("Text"))));
    TestEqual(TEXT("Speaker text uses same accounting"),ComputeTextBadgeState({Player->Line.Text}).Total,Cultures().Num());
    TestFalse(TEXT("Opening details writes no catalog"),FPaths::FileExists(TestStorageDirectory/TEXT("ManualTranslations.json")));
    return true;
}
#endif
class SDialogueTranslations : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDialogueTranslations){}
        SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>,Property)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {
        Property=Args._Property;
        ChildSlot[SNew(SExpandableArea).InitiallyCollapsed(!ExpandUntranslatedNode(Property))
            .HeaderContent()[SNew(STextBlock).ColorAndOpacity_Lambda([this]()->FSlateColor{
                if(Property->GetProperty()->GetOwnerStruct()!=FSpeakerInfo::StaticStruct() && !(Property->GetProperty()->GetFName()==TEXT("EnemyDisplayName") && VTGAssetDisplayLocalizationDetails::IsSafeText(Property)))return FSlateColor::UseForeground();
                FText Text;if(Property->GetValue(Text)!=FPropertyAccess::Success)return FSlateColor::UseForeground();
                const auto State=ComputeTextBadgeState({Text});
                return TranslationHeaderColor(State,Dirty.Num()>0);
            }).Text_Lambda([this](){return FText::FromString(TEXT("多语言译文（展开编辑） | ")+StateText(TEXT("en-US")).ToString()+TEXT("  |  ")+StateText(TEXT("zh-Hans")).ToString());})]
            .BodyContent()[SAssignNew(Content,SVerticalBox)]];
        Rebuild();
    }
    virtual void Tick(const FGeometry& Geometry,double Time,float Delta) override
    {
        SCompoundWidget::Tick(Geometry,Time,Delta);
        if(PropertySignature(Property)!=Signature)
        {
            if(Dirty.Num()==0)
            {
                // Snapshot the previous source before reading the changed property.
                TSharedPtr<FEntry> Changed;FString Error;
                if(!GetDefault<UVTGLocalizationSettings>()->bFollowOriginForUntranslated && !Origin.IsEmpty() && ReadDialogueEntry(Property,Changed,Error) && Changed->Source!=Origin)
                {
                    const auto Previous=Changed->Translations;
                    for(const auto& Culture:Cultures())if(!Changed->Translations.Contains(Culture))Changed->Translations.Add(Culture,{Origin,Origin});
                    if(!WriteCatalog(Error)){Changed->Translations=Previous;Message=Error;return;}
                }
                Rebuild();
            }
            else Message=TEXT("Origin 或文本键已改变。已保留未保存草稿；请重新加载后编辑，防止误写。");
        }
    }
private:
    TSharedPtr<IPropertyHandle> Property;
    TSharedPtr<SVerticalBox> Content;
    TMap<FString,FString> Drafts;
    TSet<FString> Dirty;
    FString Signature,Origin,Message;
    bool CanWrite=false;
    bool Save(const FString& Culture)
    {
        if(PropertySignature(Property)!=Signature){Message=TEXT("Origin 或文本键已改变，保存被阻止。请重新加载。");return false;}
        if(!SaveDialogueDraft(Property,Culture,Origin,Drafts.FindRef(Culture),Message))return false;
        Dirty.Remove(Culture);Message=TEXT("手工译文已保存。点击保存并应用使游戏预览生效。");return true;
    }
    FText StateText(const FString& Culture) const
    {
        if(Dirty.Contains(Culture))return FText::FromString(Culture+TEXT("  → 未保存"));
        TSharedPtr<FEntry> E;FString Error;
        if(!ReadDialogueEntry(Property,E,Error))return FText::FromString(Error);
        const auto* T=E->Translations.Find(Culture);
        return FText::FromString(Culture+(!T || T->Text.IsEmpty()?TEXT("  · 跟随 Origin"):T->ReviewedSource!=E->Source?TEXT("  → 待复核"):TEXT("  → 已保存独立译文")));
    }
    FSlateColor StateColor(const FString& Culture) const
    {
        TSharedPtr<FEntry> E;FString Error;
        if(ReadDialogueEntry(Property,E,Error))if(const auto* T=E->Translations.Find(Culture))
            if(!T->Text.IsEmpty())return !Dirty.Contains(Culture) && T->ReviewedSource==E->Source?FLinearColor(.25f,1,.5f):FLinearColor(1,.35f,.7f);
        return FSlateColor::UseForeground();
    }
    void Rebuild()
    {
        Content->ClearChildren();Drafts.Reset();Dirty.Reset();Origin.Empty();Message.Empty();Signature=PropertySignature(Property);CanWrite=false;
        Content->AddSlot().AutoHeight().Padding(6)[SNew(SCheckBox)
          .IsChecked_Lambda([](){return GetDefault<UVTGLocalizationSettings>()->bFollowOriginForUntranslated?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
          .OnCheckStateChanged_Lambda([](ECheckBoxState State){auto* Settings=GetMutableDefault<UVTGLocalizationSettings>();Settings->bFollowOriginForUntranslated=State==ECheckBoxState::Checked;Settings->SaveConfig();})
          [SNew(STextBlock).Text(FText::FromString(TEXT("同步未翻译字段跟随 Origin（保留已有译文）")))]];
        FString Error;
        for(const auto& Culture:Cultures())
        {
            FString Source,Draft;
            if(!GetDialogueDraft(Property,Culture,Source,Draft,Error))
            {Message=Error;break;}
            CanWrite=true;Origin=Source;Drafts.Add(Culture,Draft);
            const FString Label=Culture==TEXT("en-US")?TEXT("English"):Culture==TEXT("zh-Hans")?TEXT("简体中文"):Culture;
            TSharedPtr<FEntry> Entry;FString EntryError;ReadDialogueEntry(Property,Entry,EntryError);
            const auto* Existing=Entry.IsValid()?Entry->Translations.Find(Culture):nullptr;
            const FString State=!Error.IsEmpty()?Error:Existing && !Existing->Text.IsEmpty()?TEXT("已手工填写"):TEXT("默认复制并跟随 Origin（未编辑）");
            Content->AddSlot().AutoHeight().Padding(6,4)[SNew(STextBlock).Text_Lambda([this,Culture](){return StateText(Culture);}).ColorAndOpacity_Lambda([this,Culture](){return StateColor(Culture);})];
            Content->AddSlot().AutoHeight().Padding(6,2)[SNew(SVTGLocalizationTextBox).Text(FText::AsCultureInvariant(Draft))
                .OnTextChanged_Lambda([this,Culture](const FText& Value){const FString Next=Value.ToString();if(Next!=Drafts.FindRef(Culture)){Drafts.Add(Culture,Next);Dirty.Add(Culture);}})
                .OnTextCommitted_Lambda([this,Culture](const FText&,ETextCommit::Type){if(Dirty.Contains(Culture))Save(Culture);})];
        }
        const auto Tokens=ProtectedTokens(Origin);
        if(Tokens.Num())
        {
            FString Legend=TEXT("只读结构：");for(int32 Index=0;Index<Tokens.Num();++Index)Legend+=FString::Printf(TEXT("  ⟦%d⟧ = %s"),Index+1,*Tokens[Index]);
            Content->AddSlot().AutoHeight().Padding(6)[SNew(STextBlock).AutoWrapText(true).Text(FText::AsCultureInvariant(Legend))];
        }
        Content->AddSlot().AutoHeight().Padding(6)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("保存并应用译文"))).IsEnabled_Lambda([this](){return CanWrite;})
                .OnClicked_Lambda([this](){for(const auto& Pair:Drafts)if(!Save(Pair.Key))return FReply::Handled();if(PropertySignature(Property)!=Signature){Message=TEXT("Origin 已改变，请重新加载。");return FReply::Handled();}if(Compile(Message))Message=TEXT("已应用手工译文，可在游戏控制台切换语言查看。");return FReply::Handled();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SButton).Text(FText::FromString(TEXT("重新加载 / 放弃未保存草稿"))).OnClicked_Lambda([this](){Rebuild();return FReply::Handled();})]];
        Content->AddSlot().AutoHeight().Padding(6)[SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FLinearColor(1,.65f,.2f)).Text_Lambda([this](){return FText::FromString(Message);})];
    }
};
}
TSharedRef<SWidget> MakeDialogueTranslations(TSharedPtr<IPropertyHandle> Property)
{
    return MakeInlineTranslations(Property);
}
TSharedRef<SWidget> MakeInlineTranslations(TSharedPtr<IPropertyHandle> Property)
{
    return SNew(SDialogueTranslations).Property(Property);
}

namespace
{
// Four Slate attributes read the badge every frame and the catalog lookup is a linear scan,
// so each badge keeps its own one-frame cache.
struct FBadgeCache { uint64 Frame=0; FBadgeState State; };
FBadgeState ReadBadgeState(TSharedRef<FBadgeCache> Cache,UObject* Node)
{
    if(Cache->Frame!=GFrameCounter){Cache->Frame=GFrameCounter;Cache->State=ComputeBadgeState(Node);}
    return Cache->State;
}
// A filled pill only once something is translated: leaving the untranslated majority quiet is
// what makes the translated nodes read at a glance.
const FSlateBrush* BadgeBrush(const FBadgeState& State)
{
    static const FSlateRoundedBoxBrush Quiet(FLinearColor(0,0,0,.45f),FLinearColor(1,1,1,.15f),1.f);
    static const FSlateRoundedBoxBrush Partial(FLinearColor(1,.35f,.7f),FLinearColor(.08f,.02f,.05f,.9f),1.5f);
    static const FSlateRoundedBoxBrush Complete(FLinearColor(.3f,1,.6f),FLinearColor(.02f,.08f,.04f,.9f),1.5f);
    if(State.Failed || (!State.Done && !State.Stale))return &Quiet;
    return State.Stale || State.Done<State.Total?&Partial:&Complete;
}
FSlateColor BadgeTextColor(const FBadgeState& State)
{
    if(State.Failed)return FLinearColor(1,.55f,.2f);
    if(!State.Done && !State.Stale)return FLinearColor(.72f,.72f,.75f);
    return FLinearColor(.05f,.02f,.06f);
}
TSharedPtr<SWidget> MakeNodeBadge(UObject* Node)
{
        TWeakObjectPtr<UObject> Weak=Node;
        TSharedRef<FBadgeCache> Cache=MakeShared<FBadgeCache>();
        return SNew(SBorder)
            .BorderImage_Lambda([Weak,Cache](){return BadgeBrush(ReadBadgeState(Cache,Weak.Get()));})
            .Padding(FMargin(6,1,6,2))
            .VAlign(VAlign_Center)
            .Visibility_Lambda([Weak,Cache](){const auto State=ReadBadgeState(Cache,Weak.Get());return State.Failed || State.Total?EVisibility::Visible:EVisibility::Collapsed;})
            .ToolTipText(FText::FromString(TEXT("已保存且对应当前 Origin 的独立译文数 / 本节点显示文本的目标语言总数（Dialogue 台词、备选台词、选项和提示；Quest 描述和 Task Description Override）。跟随原文不计为完成；不代表译文质量已审核。")))
            [SNew(STextBlock)
                .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"),9))
                .Text_Lambda([Weak,Cache](){
                    const auto State=ReadBadgeState(Cache,Weak.Get());
                    if(State.Failed)return FText::FromString(TEXT("翻译表错误"));
                    if(!State.Total)return FText::GetEmpty();
                    return FText::FromString(FString::Printf(TEXT("译文 %d/%d%s"),State.Done,State.Total,State.Stale?TEXT(" · 待复核"):TEXT("")));
                })
                .ColorAndOpacity_Lambda([Weak,Cache]()->FSlateColor{return BadgeTextColor(ReadBadgeState(Cache,Weak.Get()));})];
}

}

void Startup()
{
    if(IsRunningCommandlet())return;
    SelectAllInput=MakeShared<FLocalizationSelectAll>();FSlateApplication::Get().RegisterInputPreProcessor(SelectAllInput);
    GetNarrativeLocalizationBadge().BindLambda([](UDialogueNode* Node){return MakeNodeBadge(Node);});
    GetNarrativeQuestLocalizationBadge().BindLambda([](UQuestNode* Node){return MakeNodeBadge(Node);});
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId,FOnSpawnTab::CreateStatic(&Spawn)).SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory()).SetDisplayName(FText::FromString(TEXT("手工多语言编辑")));
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    ExtensionHandle=Module.GetGlobalRowExtensionDelegate().AddLambda([](const FOnGenerateGlobalRowExtensionArgs& Args,TArray<FPropertyRowExtensionButton>& Buttons){
        auto Handle=Args.PropertyHandle;if(!Handle.IsValid())return;auto* P=Handle->GetProperty();
        if(!P || (!CastField<FTextProperty>(P) && !CastField<FStrProperty>(P) && !CastField<FNameProperty>(P)))return;
        FPropertyRowExtensionButton Button;Button.Icon=FSlateIcon(FAppStyle::GetAppStyleSetName(),TEXT("Icons.Edit"));
        Button.Label=FText::FromString(TEXT("多语言 / 标识风险"));Button.ToolTip=FText::FromString(TEXT("手工翻译显示文本；String/Name 仅显示逻辑标识警示，不修改原资产。"));
        Button.UIAction=FUIAction(FExecuteAction::CreateLambda([Handle](){SelectProperty(Handle);}));Buttons.Add(Button);
    });
}
void Shutdown()
{
    if(IsRunningCommandlet())return;
    if(FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).GetGlobalRowExtensionDelegate().Remove(ExtensionHandle);
    GetNarrativeQuestLocalizationBadge().Unbind();GetNarrativeLocalizationBadge().Unbind();if(SelectAllInput){if(FSlateApplication::IsInitialized())FSlateApplication::Get().UnregisterInputPreProcessor(SelectAllInput);SelectAllInput.Reset();}
    if(FSlateApplication::IsInitialized())
    {
        if(auto Tab=FGlobalTabmanager::Get()->FindExistingLiveTab(TabId))Tab->RequestCloseTab();
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
    }
    Body.Reset();CatalogCombo.Reset();Selected.Reset();Entries.Reset();VisibleEntries.Reset();CatalogSnapshot.Empty();Loaded=false;
}
}
