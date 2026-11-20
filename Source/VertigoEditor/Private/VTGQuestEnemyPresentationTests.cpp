#include "UI/VTGQuestSequenceBox.h"
#include "UI/VTGSequenceSettings.h"
#include "UI/VTGEnemyIdentityComponent.h"
#include "VTGManualLocalization.h"
#include "VTGAssetDisplayLocalizationDetails.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "PropertyHandle.h"
#include "Widgets/SWidget.h"
#include "Types/SlateAttributeMetaData.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextLocalizationResource.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGSequenceQuestUITest,"Vertigo.Sequence.QuestUI",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGSequenceQuestUITest::RunTest(const FString&)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("QuestSequenceUITest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
    auto* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay.BP_Narrative3Overlay_C"));
    if(!TestNotNull(TEXT("Real Narrative3Overlay"),Class))return false;
    auto* Overlay=NewObject<UUserWidget>(World,Class);Overlay->Initialize();Overlay->SetDesignerFlags(EWidgetDesignFlags::Designing);
    auto Slate=Overlay->TakeWidget();auto* Quest=Cast<UVTGQuestSequenceBox>(Overlay->GetWidgetFromName(TEXT("TaskBox")));
    if(!TestNotNull(TEXT("Real Quest area uses native independent gate"),Quest))return false;
    auto* Text=Overlay->GetWidgetFromName(TEXT("Line_Main"));auto* Avatar=Overlay->GetWidgetFromName(TEXT("AvtProfile_Main"));
    const auto TextVisibility=Text->GetVisibility(),AvatarVisibility=Avatar->GetVisibility();
    TestFalse(TEXT("Dialogue text is outside Quest gate"),Text->IsChildOf(Quest));TestFalse(TEXT("Avatar is outside Quest gate"),Avatar->IsChildOf(Quest));
    Quest->SetVisibility(ESlateVisibility::Visible);auto QuestSlate=Quest->TakeWidget();TSharedPtr<SWidget> Gate;
    TFunction<void(TSharedRef<SWidget>)> FindGate=[&](TSharedRef<SWidget> W){
        if(W->GetTag()==TEXT("VTG.QuestSequenceGate")){Gate=W;return;}
        auto* Children=W->GetChildren();for(int32 I=0;I<Children->Num() && !Gate;++I)FindGate(Children->GetChildAt(I));
    };
    FindGate(QuestSlate);if(!TestTrue(TEXT("Find native gate beneath optional designer wrappers"),Gate.IsValid()))return false;
    TestTrue(TEXT("Quest gate resolves the runtime world"),Quest->GetWorld()==World);
    auto IsGateShown=[&](){
        // This widget is deliberately offscreen: run the visibility-attribute step
        // normally performed by Slate's paint/invalidation pass, then inspect it.
        FSlateAttributeMetaData::UpdateOnlyVisibilityAttributes(*Gate,FSlateAttributeMetaData::EInvalidationPermission::AllowInvalidationIfConstructed);
        QuestSlate->Invalidate(EInvalidateWidgetReason::Layout);QuestSlate->SlatePrepass(1.f);return Gate->GetVisibility().IsVisible();
    };
    auto* Sequence=NewObject<ULevelSequence>(World);Sequence->Initialize();Sequence->GetMovieScene()->SetPlaybackRange(0,240000);
    ALevelSequenceActor* Actor=nullptr;auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(World,Sequence,FMovieSceneSequencePlaybackSettings(),Actor);
    TestTrue(TEXT("Missing asset data defaults to hiding Quest UI"),UVTGQuestSequenceBox::GetSequenceDefault(Sequence));
    TestTrue(TEXT("No playback leaves gate visible"),IsGateShown());
    Player->Play();TestTrue(TEXT("Playing Sequence hides Quest UI"),UVTGQuestSequenceBox::IsHiddenForWorld(World));TestFalse(TEXT("Native Quest gate is hidden"),IsGateShown());
    TestEqual(TEXT("Quest's own visibility is untouched"),Quest->GetVisibility(),ESlateVisibility::Visible);
    Quest->SetVisibility(ESlateVisibility::Hidden);Quest->SetVisibility(ESlateVisibility::Visible);Quest->SetRenderOpacity(.5f);
    TestFalse(TEXT("Quest notification changes cannot reopen the inner gate"),IsGateShown());
    TestEqual(TEXT("Textbox visibility untouched"),Text->GetVisibility(),TextVisibility);TestEqual(TEXT("Avatar visibility untouched"),Avatar->GetVisibility(),AvatarVisibility);
    Player->Pause();TestFalse(TEXT("Pause keeps Quest UI hidden"),IsGateShown());
    UVTGQuestSequenceBox::SetSequenceDefault(Sequence,false);TestTrue(TEXT("Per-Sequence switch disables hiding immediately"),IsGateShown());
    UVTGQuestSequenceBox::SetSequenceDefault(Sequence,true);
    ALevelSequenceActor* OtherActor=nullptr;auto* Other=ULevelSequencePlayer::CreateLevelSequencePlayer(World,Sequence,FMovieSceneSequencePlaybackSettings(),OtherActor);Other->Play();Player->Stop();
    TestFalse(TEXT("Overlapping Sequence keeps gate hidden after first stops"),IsGateShown());
    Quest->SetVisibility(ESlateVisibility::Collapsed);Other->Stop();
    TestTrue(TEXT("Final stop removes only Sequence interference"),IsGateShown());
    TestEqual(TEXT("Quest's updated collapsed state remains collapsed"),Quest->GetVisibility(),ESlateVisibility::Collapsed);
    Quest->SetVisibility(ESlateVisibility::Visible);Player->Play();Player->Update(float(Player->GetDuration().AsSeconds()+.1));
    TestTrue(TEXT("Natural end releases gate"),IsGateShown());
    Player->Play();Actor->Destroy();TestTrue(TEXT("Aborted/destroyed playback releases gate"),IsGateShown());
    FMovieSceneSequencePlaybackSettings PauseSettings;PauseSettings.bPauseAtEnd=true;
    ALevelSequenceActor* PauseActor=nullptr;auto* PauseEnd=ULevelSequencePlayer::CreateLevelSequencePlayer(World,Sequence,PauseSettings,PauseActor);
    PauseEnd->Play();PauseEnd->Update(float(PauseEnd->GetDuration().AsSeconds()+.1));
    TestTrue(TEXT("Pause At End retains its final paused image"),PauseEnd->IsPaused());
    TestTrue(TEXT("Finished Pause At End releases Quest interference"),IsGateShown());PauseEnd->Stop();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVTGEnemyNameLocalizationTest,"Vertigo.Localization.EnemyName",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVTGEnemyNameLocalizationTest::RunTest(const FString&)
{
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("Automation/EnemyNameLocalization")/FGuid::NewGuid().ToString();
    VTGManualLocalization::SetTestStorageDirectory(Directory);
    ON_SCOPE_EXIT {VTGManualLocalization::SetTestStorageDirectory(FString());FTextLocalizationManager::Get().RefreshResources();};
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Characters/NPC/GenericOnes/DrunkBarFightGuy/BP_Enm_BarFighter"));
    auto* Base=LoadClass<AActor>(nullptr,TEXT("/Game/AI/AIBehaviorSystem/Blueprint/AI/BP_AI_Base.BP_AI_Base_C"));
    if(!TestTrue(TEXT("Requested BarFighter inherits BP_AI_Base"),BP && Base && BP->GeneratedClass->IsChildOf(Base)))return false;
    const bool WasDirty=BP->GetOutermost()->IsDirty();
    auto& Module=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    auto Default=Module.CreateSingleProperty(BP->GeneratedClass->GetDefaultObject(),TEXT("EnemyDisplayName"),FSinglePropertyParams());
    auto Parent=Module.CreateSingleProperty(Base->GetDefaultObject(),TEXT("EnemyDisplayName"),FSinglePropertyParams());
    if(!TestTrue(TEXT("Parent and child name fields available"),Default && Parent))return false;
    TestTrue(TEXT("Inherited child name is whitelisted"),VTGAssetDisplayLocalizationDetails::IsSafeText(Default->GetPropertyHandle()));
    TestTrue(TEXT("Base name is whitelisted"),VTGAssetDisplayLocalizationDetails::IsSafeText(Parent->GetPropertyHandle()));
    FString Origin,Draft,Error;
    TestTrue(TEXT("Actual BarFighter name has editable translation draft"),VTGManualLocalization::GetDialogueDraft(Default->GetPropertyHandle(),TEXT("en-US"),Origin,Draft,Error));
    TestFalse(TEXT("Opening name editor writes no catalog"),FPaths::FileExists(Directory/TEXT("ManualTranslations.json")));
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,TEXT("EnemyNameLocalizationTest"),nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
    auto* GI=NewObject<UGameInstance>(GEngine);World->SetGameInstance(GI);GEngine->GetWorldContextFromWorldChecked(World).OwningGameInstance=GI;
    auto* PC=World->SpawnActor<APlayerController>();auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,0);PC->SetPlayer(Local);
    ON_SCOPE_EXIT {GI->RemoveLocalPlayer(Local);Local->PlayerController=nullptr;};
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy=World->SpawnActor<AActor>(BP->GeneratedClass,FTransform::Identity,Params);
    if(!TestNotNull(TEXT("Actual BarFighter instance"),Enemy))return false;
    auto View=Module.CreateSingleProperty(Enemy,TEXT("EnemyDisplayName"),FSinglePropertyParams());auto Property=View->GetPropertyHandle();
    const FString NS=TEXT("VTG.EnemyName.Test.")+FGuid::NewGuid().ToString(),Source=TEXT("Enemy source");
    Property->SetValue(FText::ChangeKey(NS,TEXT("EnemyName"),FText::FromString(Source)));
    auto* Identity=Enemy->FindComponentByClass<UVTGEnemyIdentityComponent>();
    UWidgetComponent* Bar=nullptr;UTextBlock* Label=nullptr;
    for(auto* Component:Enemy->GetComponents())if(auto* Widget=Cast<UWidgetComponent>(Component))
        if(auto Class=Widget->GetWidgetClass())
        {
            auto* UI=NewObject<UUserWidget>(World,Class);UI->Initialize();
            if(auto* Name=Cast<UTextBlock>(UI->GetWidgetFromName(TEXT("EnemyName")))){Widget->SetWidget(UI);Bar=Widget;Label=Name;break;}
        }
    if(!TestTrue(TEXT("Actual identity component and overhead EnemyName widget"),Identity && Bar && Label))return false;
    TestTrue(TEXT("English name saves"),VTGManualLocalization::SaveDialogueDraft(Property,TEXT("en-US"),Source,TEXT("Enemy display EN"),Error));
    TestTrue(TEXT("Chinese name saves"),VTGManualLocalization::SaveDialogueDraft(Property,TEXT("zh-Hans"),Source,TEXT("敌人显示名"),Error));
    TestTrue(TEXT("Name resources compile in isolated storage"),VTGManualLocalization::Compile(Error));
    for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("en-US"),TEXT("Enemy display EN")},{TEXT("zh-Hans"),TEXT("敌人显示名")}})
    {
        FTextLocalizationResource Resource;TestTrue(TEXT("Name resource loads"),Resource.LoadFromFile(Directory/TEXT("Resources")/Pair.Key/TEXT("Vertigo.locres"),0));
        FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);Identity->RefreshName();
        TestEqual(TEXT("Existing overhead widget updates translated name"),Label->GetText().ToString(),Pair.Value);
    }
    TestEqual(TEXT("Name Origin remains unchanged"),*FTextInspector::GetSourceString(FindFProperty<FTextProperty>(Enemy->GetClass(),TEXT("EnemyDisplayName"))->GetPropertyValue_InContainer(Enemy)),Source);
    TestEqual(TEXT("Original BarFighter asset stays untouched"),BP->GetOutermost()->IsDirty(),WasDirty);
    return true;
}
#endif
