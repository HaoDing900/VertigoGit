#include "VTGMailRowJump.h"
#include "K2Node_CallFunction.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/DataTable.h"
#include "Editor.h"
#include "DataTableEditorUtils.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenus.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "KismetPins/SGraphPinString.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"

namespace VTGMailRowJump
{
namespace
{
FDelegateHandle StartupHandle;
const TCHAR* TablePath = TEXT("/Game/Narrative/Mail/DT_Mail_ID_FillHere.DT_Mail_ID_FillHere");

bool IsMailCall(const UK2Node_CallFunction* Node)
{
    if (!Node) return false;
    const FName Function = Node->FunctionReference.GetMemberName();
    return Function == TEXT("SetMailData") || Function == TEXT("ForceReadMail");
}

void Jump(TWeakObjectPtr<const UK2Node_CallFunction> WeakNode)
{
    FName ID;
    if (!GEditor || !Resolve(WeakNode.Get(), ID)) return;
    auto* Table = LoadObject<UDataTable>(nullptr, TablePath);
    if (!Table || !Table->GetRowMap().Contains(ID)) return;
    GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Table);
    FDataTableEditorUtils::SelectRow(Table, ID);
}

// Editable text consumes right-clicks before the graph node menu receives them.
// Keep the stock name-pin editing behavior, adding the same action to its text menu.
class SMailIDPin : public SGraphPinString
{
protected:
    virtual TSharedRef<SWidget> GetDefaultValueWidget() override
    {
        const TWeakObjectPtr<const UK2Node_CallFunction> Node(Cast<UK2Node_CallFunction>(GraphPinObj->GetOwningNode()));
        return SNew(SEditableTextBox)
            .Style(FAppStyle::Get(), "Graph.EditableTextBox")
            .Text(this, &SMailIDPin::GetTypeInValue)
            .SelectAllTextWhenFocused(true)
            .Visibility(this, &SGraphPin::GetDefaultValueVisibility)
            .IsReadOnly(this, &SMailIDPin::GetDefaultValueIsReadOnly)
            .OnTextCommitted(this, &SMailIDPin::SetTypeInValue)
            .ContextMenuExtender(FMenuExtensionDelegate::CreateLambda([Node](FMenuBuilder& Menu)
            {
                Menu.AddMenuEntry(NSLOCTEXT("VTGMail", "Jump", "Jump to Mail DataTable Row"),
                    NSLOCTEXT("VTGMail", "TextJumpTip", "Open DT_Mail_ID_FillHere and select this Mail ID."), FSlateIcon(),
                    FUIAction(FExecuteAction::CreateLambda([Node]() { Jump(Node); }),
                        FCanExecuteAction::CreateLambda([Node]()
                        {
                            FName ID;
                            const auto* Table = LoadObject<UDataTable>(nullptr, TablePath);
                            return Resolve(Node.Get(), ID) && Table && Table->GetRowMap().Contains(ID);
                        })));
            }));
    }
};

class FMailIDPinFactory : public FGraphPanelPinFactory
{
    virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* Pin) const override
    {
        if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == TEXT("MailID") &&
            Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name && IsMailCall(Cast<UK2Node_CallFunction>(Pin->GetOwningNode())))
            return SNew(SMailIDPin, Pin);
        return nullptr;
    }
};
TSharedPtr<FMailIDPinFactory> PinFactory;

void Populate(UToolMenu* Menu)
{
    const auto* Context = Menu ? Menu->FindContext<UGraphNodeContextMenuContext>() : nullptr;
    const auto* Node = Context ? Cast<UK2Node_CallFunction>(Context->Node) : nullptr;
    if (!IsMailCall(Node)) return;
    const TWeakObjectPtr<const UK2Node_CallFunction> WeakNode(Node);
    FName ID;
    Resolve(Node, ID);
    auto& Section = Menu->AddSection(TEXT("VTGMailRowJump"), NSLOCTEXT("VTGMail", "Section", "Mail"));
    Section.AddMenuEntry(TEXT("JumpToMailRow"),
        ID.IsNone() ? NSLOCTEXT("VTGMail", "Jump", "Jump to Mail DataTable Row") :
            FText::Format(NSLOCTEXT("VTGMail", "JumpID", "Jump to Mail DataTable Row: {0}"), FText::FromName(ID)),
        NSLOCTEXT("VTGMail", "Tooltip", "Open DT_Mail_ID_FillHere and select this Mail ID. Requires a literal ID matching an existing row."),
        FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([WeakNode]() { Jump(WeakNode); }),
            FCanExecuteAction::CreateLambda([WeakNode]()
            {
                FName Row;
                if (!Resolve(WeakNode.Get(), Row)) return false;
                const auto* Table = LoadObject<UDataTable>(nullptr, TablePath);
                return Table && Table->GetRowMap().Contains(Row);
            })));
}

void Register()
{
    UToolMenus::Get()->ExtendMenu(TEXT("GraphEditor.GraphNodeContextMenu.K2Node_CallFunction"))->
        AddDynamicSection(TEXT("VTGMailRowJumpDynamic"), FNewToolMenuDelegate::CreateStatic(&Populate));
}
}

bool Resolve(const UK2Node_CallFunction* Node, FName& RowName)
{
    RowName = NAME_None;
    if (!IsMailCall(Node)) return false;
    const auto* Pin = Node->FindPin(TEXT("MailID"));
    if (!Pin || Pin->Direction != EGPD_Input || Pin->LinkedTo.Num() || Pin->DefaultValue.IsEmpty()) return false;
    RowName = FName(*Pin->DefaultValue);
    return !RowName.IsNone();
}

void Startup()
{
    PinFactory = MakeShared<FMailIDPinFactory>();
    FEdGraphUtilities::RegisterVisualPinFactory(PinFactory);
    if (!IsRunningCommandlet()) StartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&Register));
}

void Shutdown()
{
    if (PinFactory) FEdGraphUtilities::UnregisterVisualPinFactory(PinFactory);
    PinFactory.Reset();
    UToolMenus::UnRegisterStartupCallback(StartupHandle);
    if (UObjectInitialized() && UToolMenus::IsToolMenuUIEnabled())
        UToolMenus::Get()->RemoveSection(TEXT("GraphEditor.GraphNodeContextMenu.K2Node_CallFunction"), TEXT("VTGMailRowJumpDynamic"));
}
}
