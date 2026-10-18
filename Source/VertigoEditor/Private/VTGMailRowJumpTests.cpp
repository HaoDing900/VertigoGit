#include "VTGMailRowJump.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Knot.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/AutomationTest.h"
#include "ToolMenus.h"
#include "NodeFactory.h"
#include "SGraphPin.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGMailRowJumpTest, "Vertigo.Editor.MailRowJump",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVTGMailRowJumpTest::RunTest(const FString&)
{
    auto* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Narrative/Mail/DT_Mail_ID_FillHere.DT_Mail_ID_FillHere"));
    auto* Player = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa"));
    auto* BPLM = LoadObject<UBlueprint>(nullptr, TEXT("/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt.BPLM_SewerUnderApt"));
    if (!TestTrue(TEXT("Project mail assets load"), Table && Table->GetRowMap().Num() && Player && Player->GeneratedClass && BPLM)) return false;
    const bool WasDirty = Table->GetOutermost()->IsDirty();
    const FName Row = Table->GetRowNames()[0];
    for (const FName Function : {FName(TEXT("SetMailData")), FName(TEXT("ForceReadMail"))})
    {
        auto* Node = NewObject<UK2Node_CallFunction>();
        Node->FunctionReference.SetExternalMember(Function, Player->GeneratedClass);
        auto* Pin = Node->CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Name, TEXT("MailID"));
        Pin->DefaultValue = Row.ToString();
        FName Resolved;
        TestTrue(TEXT("Both mail function types support row navigation"), VTGMailRowJump::Resolve(Node, Resolved));
        TestEqual(TEXT("Literal MailID resolves without changing it"), Resolved, Row);
        auto* OutputNode = NewObject<UK2Node_Knot>();
        auto* Output = OutputNode->CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Name, TEXT("Output"));
        Pin->MakeLinkTo(Output);
        TestFalse(TEXT("Connected MailID cannot use its stale default"), VTGMailRowJump::Resolve(Node, Resolved));
        Pin->BreakAllPinLinks();
        Pin->DefaultValue = TEXT("None");
        TestFalse(TEXT("None MailID does not navigate"), VTGMailRowJump::Resolve(Node, Resolved));
    }
    int32 SetCalls = 0, ReadCalls = 0, ExistingRows = 0;
    TSet<FName> CheckedMenus;
    TArray<UEdGraph*> Graphs;
    BPLM->GetAllGraphs(Graphs);
    for (auto* Graph : Graphs) for (const auto& GraphNode : Graph->Nodes)
        if (auto* Node = Cast<UK2Node_CallFunction>(GraphNode.Get()))
        {
            FName ID;
            if (!VTGMailRowJump::Resolve(Node, ID)) continue;
            if (Node->FunctionReference.GetMemberName() == TEXT("SetMailData")) ++SetCalls;
            if (Node->FunctionReference.GetMemberName() == TEXT("ForceReadMail")) ++ReadCalls;
            if (Table->GetRowMap().Contains(ID)) ++ExistingRows;
            if (!CheckedMenus.Contains(Node->FunctionReference.GetMemberName()))
            {
                auto PinWidget = FNodeFactory::CreatePinWidget(Node->FindPin(TEXT("MailID")));
                TestTrue(TEXT("Actual Mail ID textbox has the jump-enabled pin widget"), PinWidget && PinWidget->GetTypeAsString() == TEXT("SMailIDPin"));
                auto* ContextObject = NewObject<UGraphNodeContextMenuContext>();
                ContextObject->Init(Graph, Node, Node->FindPin(TEXT("MailID")), false);
                FToolMenuContext Context;
                Context.AddObject(ContextObject);
                auto* Menu = UToolMenus::Get()->GenerateMenu(TEXT("GraphEditor.GraphNodeContextMenu.K2Node_CallFunction"), Context);
                auto* Section = Menu ? Menu->FindSection(TEXT("VTGMailRowJump")) : nullptr;
                TestTrue(TEXT("Actual mail call's right-click menu contains row navigation"), Section && Section->FindEntry(TEXT("JumpToMailRow")));
                CheckedMenus.Add(Node->FunctionReference.GetMemberName());
            }
        }
    TestTrue(TEXT("Actual Sewer Set Mail Data calls resolve"), SetCalls > 0);
    TestTrue(TEXT("Actual Sewer Force Read Mail calls resolve"), ReadCalls > 0);
    TestTrue(TEXT("Actual Sewer calls target existing DT rows"), ExistingRows > 0);
    TestEqual(TEXT("Reading jump targets never dirties the mail table"), Table->GetOutermost()->IsDirty(), WasDirty);
    return true;
}
#endif
