#include "VTGCameraRatio.h"
#include "VTGCameraRatioCommandlet.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CineCameraComponent.h"
#include "CineCameraActor.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneBinding.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneBoolChannel.h"
#include "MovieSceneSection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Editor.h"
#include "Containers/Ticker.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SWindow.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Misc/CoreDelegates.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectIterator.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

namespace VTGCameraRatio
{
constexpr float Ratio=16.f/9.f;
FDelegateHandle PreSaveHandle,AddedHandle,PrePIEHandle,PostPIEHandle;
FTSTicker::FDelegateHandle PIEFitTicker;
bool Busy=false;
FString Report;
FString FocusText(UCineCameraComponent* Cine)
{
    FString Text;auto* P=FindFProperty<FProperty>(Cine->GetClass(),TEXT("FocusSettings"));check(P);
    P->ExportText_InContainer(0,Text,Cine,nullptr,Cine,0);return Text;
}
bool Camera(UCameraComponent* C,bool Apply=true)
{
    if(!C)return false;
    const FTransform Transform=C->GetRelativeTransform();
    bool Changed=!FMath::IsNearlyEqual(C->AspectRatio,Ratio,.00001f) || !C->bConstrainAspectRatio;
    if(!Apply)
    {
        if(auto* Cine=Cast<UCineCameraComponent>(C))
            Changed|=!FMath::IsNearlyEqual(Cine->Filmback.SensorHeight,Cine->Filmback.SensorWidth/Ratio,.0001f) ||
                (Cine->CropSettings.AspectRatio!=0.f && !FMath::IsNearlyEqual(Cine->CropSettings.AspectRatio,Ratio,.00001f));
        if(Changed)UE_LOG(LogTemp,Display,TEXT("RATIO_MISMATCH %s aspect=%.6f constrain=%d"),*C->GetPathName(),C->AspectRatio,C->bConstrainAspectRatio);
        return Changed;
    }
    if(auto* Cine=Cast<UCineCameraComponent>(C))
    {
        const float Focal=Cine->CurrentFocalLength;const FString Focus=FocusText(Cine);
        auto Filmback=Cine->Filmback;
        if(!FMath::IsNearlyEqual(Filmback.SensorHeight,Filmback.SensorWidth/Ratio,.0001f))
        {Filmback.SensorHeight=Filmback.SensorWidth/Ratio;Cine->SetFilmback(Filmback);Changed=true;}
        auto Crop=Cine->CropSettings;
        if(Crop.AspectRatio!=0.f && !FMath::IsNearlyEqual(Crop.AspectRatio,Ratio,.00001f)) {Crop.AspectRatio=Ratio;Cine->SetCropSettings(Crop);Changed=true;}
        check(Cine->CurrentFocalLength==Focal && FocusText(Cine)==Focus && Cine->Filmback.SensorWidth==Filmback.SensorWidth);
    }
    C->AspectRatio=Ratio;C->bConstrainAspectRatio=true;
    check(C->GetRelativeTransform().Equals(Transform));
    if(Changed)Report+=TEXT("CAMERA ")+C->GetPathName()+TEXT(" focal/focus/transform preserved\n");
    return Changed;
}
bool Sequence(ULevelSequence* Seq,bool Inspect=false)
{
    if(!Seq || !Seq->GetMovieScene())return false;
    bool Changed=false;auto* Scene=Seq->GetMovieScene();
    for(int32 I=0;I<Scene->GetSpawnableCount();++I)
        if(auto* Actor=Cast<ACameraActor>(Scene->GetSpawnable(I).GetObjectTemplate()))
            Changed|=Camera(Actor->GetCameraComponent(),!Inspect);
    for(const auto& B:Scene->GetBindings())for(auto* T:B.GetTracks())if(auto* Track=Cast<UMovieScenePropertyTrack>(T))
    {
        const FString Path=Track->GetPropertyPath().ToString();
        if(Path.Contains(TEXT("AspectRatio")) || Path.Contains(TEXT("SensorHeight")) || Path.Contains(TEXT("SensorWidth")))
        {
            UE_LOG(LogTemp,Display,TEXT("RATIO_TRACK %s %s"),*Seq->GetPathName(),*Path);
            for(auto* Section:Track->GetAllSections())
            {
                if(Path.EndsWith(TEXT("AspectRatio")) && !Path.Contains(TEXT("Constrain")))
                    for(auto* Channel:Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>())
                    {
                        bool Different=!Channel->GetDefault().IsSet() || !FMath::IsNearlyEqual(Channel->GetDefault().GetValue(),Ratio,.00001f);
                        for(const auto& V:Channel->GetData().GetValues())Different|=!FMath::IsNearlyEqual(V.Value,Ratio,.00001f);
                        Changed|=Different;
                        if(Different&&!Inspect){Channel->SetDefault(Ratio);for(auto& V:Channel->GetData().GetValues()){V.Value=Ratio;V.Tangent.ArriveTangent=0;V.Tangent.LeaveTangent=0;}}
                    }
                if(Path.Contains(TEXT("ConstrainAspectRatio")))
                    for(auto* Channel:Section->GetChannelProxy().GetChannels<FMovieSceneBoolChannel>())
                    {
                        bool Different=!Channel->GetDefault().IsSet() || !Channel->GetDefault().GetValue();
                        for(bool V:Channel->GetData().GetValues())Different|=!V;
                        Changed|=Different;
                        if(Different&&!Inspect){Channel->SetDefault(true);for(auto& V:Channel->GetData().GetValues())V=true;}
                    }
            }
        }
    }
    return Changed;
}
bool PackageCameras(UPackage* P,bool Apply=true)
{
    bool Changed=false;
    ForEachObjectWithPackage(P,[&](UObject* O){if(auto* C=Cast<UCameraComponent>(O))if(Cast<ACameraActor>(C->GetOwner()))Changed|=Camera(C,Apply);return true;},true);
    return Changed;
}
void PrepareSave(UObject* O)
{
    if(Busy || !O || !O->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))return;
    TGuardValue<bool> Guard(Busy,true);
    if(auto* Seq=Cast<ULevelSequence>(O))Sequence(Seq);
    if(auto* World=Cast<UWorld>(O))PackageCameras(World->GetOutermost());
}
void NormalizePlaySettings(ULevelEditorPlaySettings* Settings)
{
    // Exact 16:9 integer dimensions, including the cached size that UE restores on the next play.
    const int32 Width=Settings->NewWindowWidth>0?Settings->NewWindowWidth:1920;
    const int32 Height=Settings->NewWindowHeight>0?Settings->NewWindowHeight:1080;
    const int32 Units=FMath::Clamp(FMath::Min(Width/16,Height/9),1,160);
    Settings->NewWindowWidth=16*Units;Settings->NewWindowHeight=9*Units;
    Settings->LastSize=FIntPoint(Settings->NewWindowWidth,Settings->NewWindowHeight);
    Settings->CenterNewWindow=true;
}
void FitPIEViewport()
{
    FTSTicker::GetCoreTicker().RemoveTicker(PIEFitTicker);
    PIEFitTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Attempts=0](float) mutable
    {
        if(!GEditor || !GEditor->PlayWorld || ++Attempts>20)return false;
        auto* Client=GEditor->PlayWorld->GetGameViewport();
        auto* Viewport=Client?Client->GetGameViewport():nullptr;
        if(!Viewport)return true;
        auto Window=Viewport->FindWindow();
        // Selected-viewport PIE must never resize the editor's main window.
        if(!Window.IsValid() || Window->GetTag()!=TEXT("PIEWindow"))return false;
        const auto* Settings=GetDefault<ULevelEditorPlaySettings>();
        const FIntPoint Desired(Settings->NewWindowWidth,Settings->NewWindowHeight);
        const FIntPoint Actual=Viewport->GetSizeXY();
        if(Actual.X<=0 || Actual.Y<=0)return true;
        if(Actual==Desired)
        {
            UE_LOG(LogTemp,Display,TEXT("RATIO_PIE_VIEWPORT actual=%dx%d"),Actual.X,Actual.Y);
            return false;
        }
        // Slate's custom resize border can add pixels beyond ClientSize. Correct from measured content.
        Window->Resize(Window->GetClientSizeInScreen()+FVector2D(Desired.X-Actual.X,Desired.Y-Actual.Y));
        return true;
    }),.1f);
}
void Startup()
{
    if(IsRunningCommandlet())return;
    Camera(GetMutableDefault<UCameraComponent>());Camera(GetMutableDefault<UCineCameraComponent>());
    Camera(GetMutableDefault<ACameraActor>()->GetCameraComponent());
    Camera(GetMutableDefault<ACineCameraActor>()->GetCineCameraComponent());
    NormalizePlaySettings(GetMutableDefault<ULevelEditorPlaySettings>());
    PrePIEHandle=FEditorDelegates::PreBeginPIE.AddLambda([](bool)
    {
        // UE duplicates play settings before PreBeginPIE, so normalize the request copy too.
        for(TObjectIterator<ULevelEditorPlaySettings> It;It;++It)NormalizePlaySettings(*It);
    });
    PostPIEHandle=FEditorDelegates::PostPIEStarted.AddLambda([](bool){FitPIEViewport();});
    PreSaveHandle=FCoreUObjectDelegates::OnObjectPreSave.AddLambda([](UObject* O,FObjectPreSaveContext)
    {
        PrepareSave(O);
    });
    auto& AR=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    AddedHandle=AR.OnAssetAdded().AddLambda([](const FAssetData& Data)
    {if(!Busy && Data.IsAssetLoaded())if(auto* Seq=Cast<ULevelSequence>(Data.GetAsset())) {TGuardValue<bool> Guard(Busy,true);if(Sequence(Seq))Seq->MarkPackageDirty();}});
}
void Shutdown()
{
    FCoreUObjectDelegates::OnObjectPreSave.Remove(PreSaveHandle);
    FEditorDelegates::PreBeginPIE.Remove(PrePIEHandle);
    FEditorDelegates::PostPIEStarted.Remove(PostPIEHandle);
    FTSTicker::GetCoreTicker().RemoveTicker(PIEFitTicker);
    if(FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().OnAssetAdded().Remove(AddedHandle);
}
}
UVTGCameraRatioCommandlet::UVTGCameraRatioCommandlet(){IsEditor=true;IsClient=false;IsServer=false;LogToConsole=true;}
int32 UVTGCameraRatioCommandlet::Main(const FString& Params)
{
    using namespace VTGCameraRatio;
    if(FParse::Param(*Params,TEXT("ConfigurePIE")))
    {
        auto* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
        Settings->NewWindowWidth=1920;Settings->NewWindowHeight=1080;
        Settings->SetClientWindowSize(FIntPoint(1280,720));
        NormalizePlaySettings(Settings);Settings->SaveConfig();
        UE_LOG(LogTemp,Display,TEXT("RATIO_PIE configured 1920x1080 (75%% of 2560x1440), centered, cached size reset"));
        if(!FParse::Param(*Params,TEXT("TestPolicy")))return 0;
    }
    if(FParse::Param(*Params,TEXT("TestPolicy")))
    {
        auto* Seq=NewObject<ULevelSequence>(CreatePackage(TEXT("/Game/__VTG_RatioPolicyTest")),TEXT("RatioTest"));Seq->Initialize();
        auto* Template=NewObject<ACineCameraActor>(GetTransientPackage(),NAME_None,RF_ArchetypeObject);
        auto* C=Template->GetCineCameraComponent();C->Filmback.SensorWidth=36.f;C->Filmback.SensorHeight=24.f;C->AspectRatio=1.5f;C->bConstrainAspectRatio=false;
        C->CurrentFocalLength=53.f;C->FocusSettings.ManualFocusDistance=987.f;
        Seq->GetMovieScene()->AddSpawnable(TEXT("Camera"),*Template);
        PrepareSave(Seq);
        auto* Result=CastChecked<ACineCameraActor>(Seq->GetMovieScene()->GetSpawnable(0).GetObjectTemplate())->GetCineCameraComponent();
        check(FMath::IsNearlyEqual(Result->AspectRatio,Ratio) && Result->bConstrainAspectRatio && FMath::IsNearlyEqual(Result->Filmback.SensorHeight,20.25f));
        check(Result->CurrentFocalLength==53.f && Result->FocusSettings.ManualFocusDistance==987.f);
        check(!Sequence(Seq,true));
        UE_LOG(LogTemp,Display,TEXT("RATIO_POLICY_PASS new sequence save normalized to 16:9; focal 53 and focus 987 preserved"));return 0;
    }
    const bool Apply=FParse::Param(*Params,TEXT("Apply"));
    const bool Verify=FParse::Param(*Params,TEXT("Verify"));
    const bool Samples=FParse::Param(*Params,TEXT("Samples"));
    auto& AR=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();AR.SearchAllAssets(true);
    TArray<FAssetData> Maps;AR.GetAssetsByPath(TEXT("/Game/ThirdPerson/Maps"),Maps,true);
    TSet<FName> Relevant;TArray<FName> Queue;TSet<FName> MapPackages;
    for(const auto& D:Maps)if(D.AssetClassPath==UWorld::StaticClass()->GetClassPathName()){Queue.Add(D.PackageName);MapPackages.Add(D.PackageName);}
    for(int32 I=0;I<Queue.Num();++I)
    {if(Relevant.Contains(Queue[I]))continue;Relevant.Add(Queue[I]);TArray<FName> Deps;AR.GetDependencies(Queue[I],Deps);for(auto D:Deps)if(D.ToString().StartsWith(TEXT("/Game/"))&&!Relevant.Contains(D))Queue.Add(D);}
    TArray<FAssetData> Assets;AR.GetAssetsByClass(ULevelSequence::StaticClass()->GetClassPathName(),Assets,true);
    int32 Sequences=0,ChangedPackages=0,Cameras=0,MismatchedPackages=0,SequenceTemplates=0,SequencePossessables=0;
    auto Save=[&](UObject* O,bool Map)
    {
        const FString File=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),Map?FPackageName::GetMapPackageExtension():FPackageName::GetAssetPackageExtension());
        const FString Copy=FPaths::ProjectSavedDir()/TEXT("QA/CameraRatio16x9/Backup")/O->GetOutermost()->GetName().RightChop(6)+(Map?TEXT(".umap"):TEXT(".uasset"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Copy),true);if(!IFileManager::Get().FileExists(*Copy))check(IFileManager::Get().Copy(*Copy,*File,false)==COPY_OK);
        FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;O->MarkPackageDirty();check(UPackage::SavePackage(O->GetOutermost(),O,*File,A));++ChangedPackages;
    };
    for(const auto& D:Assets)if(Relevant.Contains(D.PackageName) && (!Samples || IFileManager::Get().FileExists(*(FPaths::ProjectDir()/TEXT("Scripts/RatioAndBoundaryPolish/BackupRatio")/D.PackageName.ToString().RightChop(6)+TEXT(".uasset")))))
    {
        auto* Seq=CastChecked<ULevelSequence>(D.GetAsset());++Sequences;
        auto* Scene=Seq->GetMovieScene();int32 Templates=0,Possessables=0;
        for(int32 I=0;I<Scene->GetSpawnableCount();++I)if(Cast<ACameraActor>(Scene->GetSpawnable(I).GetObjectTemplate()))++Templates;
        for(int32 I=0;I<Scene->GetPossessableCount();++I)if(auto* Class=Scene->GetPossessable(I).GetPossessedObjectClass())if(Class->IsChildOf(ACameraActor::StaticClass()))++Possessables;
        SequenceTemplates+=Templates;SequencePossessables+=Possessables;
        const bool Changed=Sequence(Seq,!Apply);if(Changed)++MismatchedPackages;
        UE_LOG(LogTemp,Display,TEXT("RATIO_SEQUENCE %s cameraTemplates=%d cameraActorBindings=%d mismatch=%d"),*D.PackageName.ToString(),Templates,Possessables,Changed);
        if(Changed&&Apply)Save(Seq,false);if(Sequences%20==0)CollectGarbage(RF_NoFlags);
    }
    if(!FParse::Param(*Params,TEXT("SequencesOnly")))for(FName Map:MapPackages)
    {
        if(Samples && Map!=TEXT("/Game/ThirdPerson/Maps/L2Bar") && Map!=TEXT("/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar"))continue;
        auto* P=LoadPackage(nullptr,*Map.ToString(),LOAD_None);if(!P)continue;
        int32 Count=0;ForEachObjectWithPackage(P,[&](UObject* O){if(auto* C=Cast<UCameraComponent>(O))if(Cast<ACameraActor>(C->GetOwner()))++Count;return true;},true);Cameras+=Count;
        const bool Changed=PackageCameras(P,Apply);if(Changed)++MismatchedPackages;
        if(Apply && Changed)if(auto* W=UWorld::FindWorldInPackage(P))Save(W,true);
        UE_LOG(LogTemp,Display,TEXT("RATIO_MAP %s cameras=%d"),*Map.ToString(),Count);CollectGarbage(RF_NoFlags);
    }
    if(Apply){const FString Out=FPaths::ProjectSavedDir()/TEXT("QA/CameraRatio16x9/camera-report.txt");IFileManager::Get().MakeDirectory(*FPaths::GetPath(Out),true);FFileHelper::SaveStringToFile(Report,*Out);}
    UE_LOG(LogTemp,Display,TEXT("RATIO_RESULT apply=%d maps=%d sequences=%d sequenceCameraTemplates=%d sequenceCameraActorBindings=%d placedCameras=%d changedPackages=%d mismatchedPackages=%d"),Apply,MapPackages.Num(),Sequences,SequenceTemplates,SequencePossessables,Cameras,ChangedPackages,MismatchedPackages);
    return Verify && MismatchedPackages?1:0;
}



