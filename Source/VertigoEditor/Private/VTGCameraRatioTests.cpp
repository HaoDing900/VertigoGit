#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGCameraRatioPIETest,"Vertigo.Editor.CameraRatio.PIEAndNewCameras",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

namespace
{
class FVTGCheckPIERatio : public IAutomationLatentCommand
{
public:
    explicit FVTGCheckPIERatio(FAutomationTestBase* InTest):Test(InTest),Deadline(FPlatformTime::Seconds()+45.){}
    bool Update() override
    {
        if(GEditor->PlayWorld && GEditor->PlayWorld->GetGameViewport() && GEditor->PlayWorld->GetGameViewport()->Viewport)
        {
            const FIntPoint Size=GEditor->PlayWorld->GetGameViewport()->Viewport->GetSizeXY();
            if(Size.X>0 && Size.Y>0)
            {
                if(ViewportReadyTime==0.)ViewportReadyTime=FPlatformTime::Seconds();
                if(FPlatformTime::Seconds()-ViewportReadyTime<1.)return false;
                UE_LOG(LogTemp,Display,TEXT("RATIO_ACTUAL_PIE viewport=%dx%d"),Size.X,Size.Y);
                Test->TestEqual(TEXT("PIE output width"),Size.X,1920);
                Test->TestEqual(TEXT("PIE output height"),Size.Y,1080);
                Test->TestEqual(TEXT("PIE exact 16:9"),Size.X*9,Size.Y*16);
                return true;
            }
        }
        if(FPlatformTime::Seconds()>Deadline){Test->AddError(TEXT("PIE did not create a viewport"));return true;}
        return false;
    }
private:
    FAutomationTestBase* Test;
    double Deadline;
    double ViewportReadyTime=0.;
};
}

bool FVTGCameraRatioPIETest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();
    World->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
    auto* Camera=World->SpawnActor<ACameraActor>();
    auto* Cine=World->SpawnActor<ACineCameraActor>();
    TestTrue(TEXT("New regular camera defaults to constrained 16:9"),
        Camera->GetCameraComponent()->bConstrainAspectRatio && FMath::IsNearlyEqual(Camera->GetCameraComponent()->AspectRatio,16.f/9.f,.00001f));
    const auto* Component=Cine->GetCineCameraComponent();
    TestTrue(TEXT("New Cine camera defaults to constrained 16:9"),Component->bConstrainAspectRatio &&
        FMath::IsNearlyEqual(Component->AspectRatio,16.f/9.f,.00001f) &&
        FMath::IsNearlyEqual(Component->Filmback.SensorHeight,Component->Filmback.SensorWidth*9.f/16.f,.0001f));
    auto* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
    TestEqual(TEXT("Saved PIE default width"),Settings->NewWindowWidth,1920);
    TestEqual(TEXT("Saved PIE default height"),Settings->NewWindowHeight,1080);
    TestTrue(TEXT("PIE centered to bypass old cached size"),bool(Settings->CenterNewWindow));
    // A stale LastSize must not replace the requested 16:9 dimensions at startup.
    Settings->LastSize=FIntPoint(1274,680);
    FRequestPlaySessionParams Request;
    Request.bAllowOnlineSubsystem=false;
    GEditor->RequestPlaySession(Request);
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShareable(new FVTGCheckPIERatio(this)));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
