#include "DialogueEventFilter.h"
#include "DialogueGraphEditor.h"
#include "DialogueBlueprint.h"
#include "DialogueGraphNode.h"
#include "DialogueSM.h"
#include "NarrativeEvent.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

namespace NarrativeDialogueEventFilter
{
namespace
{
struct FEventType
{
    FString Path;
    FText Label;
};

class SEventFilter : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SEventFilter) {}
        SLATE_ARGUMENT(TWeakPtr<FDialogueGraphEditor>, Editor)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Editor = Args._Editor;
        if (auto Pinned = Editor.Pin()) Blueprint = Pinned->GetDialogueAsset();
        ChildSlot
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(10)
            [SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(Blueprint.IsValid() ? Blueprint->GetPathName() : TEXT("Dialogue 已关闭")))]
            + SVerticalBox::Slot().AutoHeight().Padding(10, 0)
            [SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(TEXT("筛选节点 Events 数组中的 Narrative Event（含未保存配置）。点击结果定位；修改 Events 后点击刷新。此处不检查 Event Graph 中的函数调用。")))]
            + SVerticalBox::Slot().AutoHeight().Padding(10)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)
                [SAssignNew(TypeCombo, SComboBox<TSharedPtr<FEventType>>)
                    .OptionsSource(&Types)
                    .OnGenerateWidget_Lambda([](TSharedPtr<FEventType> Type) { return SNew(STextBlock).Text(Type->Label); })
                    .OnSelectionChanged_Lambda([this](TSharedPtr<FEventType> Type, ESelectInfo::Type) { Selected = Type; RebuildResults(); })
                    [SNew(STextBlock).Text_Lambda([this]() { return Selected.IsValid() ? Selected->Label : FText::GetEmpty(); })]]
                + SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
                [SNew(SButton).Text(FText::FromString(TEXT("刷新"))).OnClicked_Lambda([this]() { Refresh(); return FReply::Handled(); })]]
            + SVerticalBox::Slot().AutoHeight().Padding(10, 0)
            [SNew(SSearchBox).HintText(FText::FromString(TEXT("搜索节点 ID / 对白 / Event 类型名")))
                .OnTextChanged_Lambda([this](const FText& Text) { Search = Text.ToString(); RebuildResults(); })]
            + SVerticalBox::Slot().AutoHeight().Padding(10)
            [SAssignNew(CountText, STextBlock)]
            + SVerticalBox::Slot().FillHeight(1).Padding(10, 0, 10, 10)
            [SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(Results, SVerticalBox)]]];
        Refresh();
    }

private:
    TWeakPtr<FDialogueGraphEditor> Editor;
    TWeakObjectPtr<UDialogueBlueprint> Blueprint;
    TArray<TSharedPtr<FEventType>> Types;
    TSharedPtr<FEventType> Selected;
    TSharedPtr<SComboBox<TSharedPtr<FEventType>>> TypeCombo;
    TSharedPtr<SVerticalBox> Results;
    TSharedPtr<STextBlock> CountText;
    FString Search;

    void Refresh()
    {
        const FString Previous = Selected.IsValid() ? Selected->Path : FString();
        Types.Reset();
        Types.Add(MakeShared<FEventType>(FEventType{TEXT(""), FText::FromString(TEXT("全部 Narrative Event 类型"))}));
        TSet<FString> Seen;
        if (Blueprint.IsValid() && Blueprint->DialogueGraph)
            for (UEdGraphNode* Node : Blueprint->DialogueGraph->Nodes)
                if (auto* GraphNode = Cast<UDialogueGraphNode>(Node))
                    if (GraphNode->DialogueNode)
                        for (auto* Event : GraphNode->DialogueNode->Events)
                            if (IsValid(Event) && !Seen.Contains(Event->GetClass()->GetPathName()))
                            {
                                UClass* Class = Event->GetClass();
                                Seen.Add(Class->GetPathName());
                                Types.Add(MakeShared<FEventType>(FEventType{Class->GetPathName(), FText::FromString(Class->GetDisplayNameText().ToString() + TEXT(" [") + Class->GetName() + TEXT("]"))}));
                            }
        Types.Sort([](const TSharedPtr<FEventType>& A, const TSharedPtr<FEventType>& B) {
            if (A->Path.IsEmpty() || B->Path.IsEmpty()) return !A->Path.IsEmpty() ? false : !B->Path.IsEmpty();
            return A->Label.ToString() < B->Label.ToString();
        });
        Selected = Types[0];
        for (const auto& Type : Types) if (Type->Path == Previous) Selected = Type;
        TypeCombo->RefreshOptions();
        TypeCombo->SetSelectedItem(Selected);
        RebuildResults();
    }

    void RebuildResults()
    {
        if (!Results.IsValid()) return;
        Results->ClearChildren();
        int32 Count = 0;
        if (Blueprint.IsValid() && Blueprint->DialogueGraph)
            for (UEdGraphNode* Node : Blueprint->DialogueGraph->Nodes)
            {
                auto* GraphNode = Cast<UDialogueGraphNode>(Node);
                if (!GraphNode || !GraphNode->DialogueNode) continue;
                auto* DialogueNode = GraphNode->DialogueNode;
                FString EventLines;
                FString EventSearch;
                bool bMatchesType = false;
                for (auto* Event : DialogueNode->Events)
                {
                    if (!IsValid(Event)) continue;
                    UClass* Class = Event->GetClass();
                    EventSearch += Class->GetPathName() + TEXT(" ") + Class->GetDisplayNameText().ToString() + TEXT(" ");
                    if (!Selected.IsValid() || Selected->Path.IsEmpty() || Selected->Path == Class->GetPathName())
                    {
                        bMatchesType = true;
                        const TCHAR* Runtime = Event->EventRuntime == EEventRuntime::Start ? TEXT("Start") : Event->EventRuntime == EEventRuntime::End ? TEXT("End") : TEXT("Both");
                        EventLines += TEXT("\n") + Class->GetDisplayNameText().ToString() + TEXT(" [") + Class->GetName() + TEXT("] · ") + Runtime;
                    }
                }
                const FString ID = DialogueNode->GetID().IsNone() ? DialogueNode->GetName() : DialogueNode->GetID().ToString();
                const FString Text = GraphNode->GetNodeText().ToString();
                if (!bMatchesType || !(ID + TEXT(" ") + Text + TEXT(" ") + EventSearch).Contains(Search)) continue;
                ++Count;
                const TWeakObjectPtr<UDialogueGraphNode> WeakNode(GraphNode);
                Results->AddSlot().AutoHeight().Padding(0, 4)
                [SNew(SButton)
                    .IsEnabled_Lambda([this, WeakNode]() { return Editor.IsValid() && WeakNode.IsValid(); })
                    .OnClicked_Lambda([this, WeakNode]() {
                        if (auto Pinned = Editor.Pin())
                            if (WeakNode.IsValid() && WeakNode->DialogueNode)
                            {
                                auto* Target = WeakNode->DialogueNode;
                                Pinned->JumpToNodeWithID(Target->GetID().IsNone() ? Target->GetFName() : Target->GetID());
                            }
                        return FReply::Handled();
                    })
                    [SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(ID + TEXT("\n") + Text + EventLines))]];
            }
        CountText->SetText(FText::FromString(FString::Printf(TEXT("匹配 %d 个节点%s"), Count, Editor.IsValid() ? TEXT("") : TEXT(" · 原 Dialogue 编辑器已关闭，请重新从工具栏打开"))));
        if (Count == 0)
            Results->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("没有匹配的节点。可清空搜索、切换 Event 类型，或修改配置后刷新。")))];
    }
};
}

void Open(TSharedRef<FDialogueGraphEditor> Editor)
{
    FSlateApplication::Get().AddWindow(SNew(SWindow)
        .Title(FText::FromString(TEXT("Dialogue · Narrative Event 筛选")))
        .ClientSize(FVector2D(800, 650)).SupportsMinimize(true).SupportsMaximize(true)
        [SNew(SEventFilter).Editor(Editor)]);
}
}
