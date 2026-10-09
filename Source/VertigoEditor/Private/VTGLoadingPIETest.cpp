#include "EdGraph/EdGraph.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "K2Node_CallFunction.h"
#include "Loading/VTGMediaLoadingPageSystem.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGLoadingPIETest, "Vertigo.Loading.MenuPIE",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FVTGCheckPIETravel : public IAutomationLatentCommand
{
  public:
    explicit FVTGCheckPIETravel(FAutomationTestBase *InTest) : Test(InTest)
    {
    }
    bool Update() override
    {
        if (StartedAt == 0.0)
        {
            UWorld *World = PIEWorld();
            if (!World)
            {
                if (PIEWaitDeadline == 0.0)
                    PIEWaitDeadline = FPlatformTime::Seconds() + 10.0;
                if (FPlatformTime::Seconds() < PIEWaitDeadline)
                    return false;
                Test->AddError(TEXT("PIE did not start."));
                return true;
            }
            Test->TestEqual(TEXT("Testing a real PIE world"), World->WorldType, EWorldType::PIE);
            OldWorld = World;
            System = World->GetGameInstance()->GetSubsystem<UVTGMediaLoadingPageSystem>();
            StartedAt = FPlatformTime::Seconds();
            UVTGMediaLoadingPageSystem::OpenLevelWithLoadingScreen(
                World, TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Engine/Maps/Entry.Entry"))), true,
                TEXT("game=/Script/Engine.GameModeBase"));
            Test->TestTrue(TEXT("Menu call displays loading BEFORE travel"), System->IsLoadingPageVisible());
            Test->TestTrue(TEXT("Fade-in starts in the original world"), PIEWorld() == OldWorld.Get());
            return false;
        }
        if (!System.IsValid())
        {
            Test->AddError(TEXT("Game instance lost during PIE travel."));
            return true;
        }
        if (!System->IsLoadingPageVisible())
        {
            Test->TestTrue(TEXT("Travel replaced the world"), PIEWorld() != OldWorld.Get());
            Test->TestTrue(TEXT("Destination world available"), PIEWorld() != nullptr);
            Test->TestTrue(TEXT("Loading was held through fade-in and load"),
                           FPlatformTime::Seconds() - StartedAt > 0.4);
            Test->AddInfo(TEXT("PIE menu loading, map travel and fade-out cleanup passed."));
            return true;
        }
        if (FPlatformTime::Seconds() - StartedAt > 30.0)
        {
            Test->AddError(TEXT("PIE loading overlay did not finish within 30 seconds."));
            return true;
        }
        return false;
    }

  private:
    static UWorld *PIEWorld()
    {
        for (const FWorldContext &Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE)
                return Context.World();
        return nullptr;
    }
    FAutomationTestBase *Test;
    double StartedAt = 0.0;
    double PIEWaitDeadline = 0.0;
    TWeakObjectPtr<UWorld> OldWorld;
    TWeakObjectPtr<UVTGMediaLoadingPageSystem> System;
};

bool FVTGLoadingPIETest::RunTest(const FString &Parameters)
{
    AddExpectedError(TEXT("Failed to load cursor"), EAutomationExpectedErrorFlags::Contains, 2);
    auto *Menu = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu"));
    if (!TestNotNull(TEXT("Main menu exists"), Menu))
        return false;
    int32 Connected = 0;
    TArray<UEdGraph *> Graphs;
    Menu->GetAllGraphs(Graphs);
    for (UEdGraph *Graph : Graphs)
        for (UEdGraphNode *Node : Graph->Nodes)
            if (auto *Call = Cast<UK2Node_CallFunction>(Node))
            {
                if (Call->FunctionReference.GetMemberName() == TEXT("OpenLevelWithLoadingScreen"))
                    ++Connected;
                TestFalse(TEXT("Menu no longer bypasses loading"),
                          Call->FunctionReference.GetMemberName() == TEXT("OpenLevelBySoftObjectPtr"));
            }
    TestTrue(TEXT("Menu uses the tested loading entry"), Connected > 0);
    UWorld *World = FAutomationEditorCommonUtils::CreateNewMap();
    World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShareable(new FVTGCheckPIETravel(this)));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
