#include "VTGResponseTransitionFixture.h"
#include "VTGCinematicRegressionFixture.h"
#include "Audio/VTGResponseAudioSubsystem.h"
#include "Audio/VTGUIFeedbackLibrary.h"
#include "Audio/VTGResponseLockedBackdrop.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Sections/MovieSceneFloatSection.h"
#include "NarrativeBlipLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/RichTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "UObject/UnrealType.h"

bool VTGRunResponseTransitionChecks()
{
    if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    int32 Checks=0;auto Expect=[&](bool Result,const TCHAR* Message){checkf(Result,TEXT("RESPONSE TRANSITION FAIL: %s"),Message);++Checks;};
    Expect(UVTGUIFeedbackLibrary::FilterSpeakerLabel(TEXT("ID_Mail"),FText::FromString(TEXT("Letter")),FText::FromString(TEXT("Letter"))).IsEmpty(),TEXT("Mail ID hides a different display name"));
    Expect(UVTGUIFeedbackLibrary::FilterSpeakerLabel(TEXT("Other"),FText::FromString(TEXT("Mail")),FText::FromString(TEXT("Mail"))).IsEmpty(),TEXT("Mail name hides label independently of ID"));
    Expect(UVTGUIFeedbackLibrary::FilterSpeakerLabel(TEXT("Sa"),FText::FromString(TEXT("Sa")),FText::FromString(TEXT("Sa"))).ToString()==TEXT("Sa"),TEXT("ordinary character names preserved"));
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* PC=World->SpawnActor<APlayerController>();
    auto* Component=NewObject<UNarrativeComponent>(PC);Component->RegisterComponent();
    auto* Dialogue=NewObject<UVTGReturnRegressionDialogue>(Component);
    auto* Back=NewObject<UDialogueNode_Player>(Dialogue);auto* Parent=NewObject<UDialogueNode_NPC>(Dialogue);auto* Choice=NewObject<UDialogueNode_Player>(Dialogue);
    Choice->Line.Text=FText::FromString(TEXT("Next menu"));Parent->PlayerReplies={Choice};Back->NPCReplies={Parent};
    Dialogue->Prepare(Component,Back);Dialogue->WaitForChoice();Component->CurrentDialogue=Dialogue;
    auto MakeWidget=[&](){auto* W=NewObject<UVTGResponseTransitionWidget>(World);W->Initialize();W->WidgetTree->RootWidget=NewObject<USizeBox>(W->WidgetTree);return W;};
    auto* Menu=MakeWidget();auto* Button=MakeWidget();auto* Other=MakeWidget();Button->OwningWidget=Menu;Other->OwningWidget=Menu;
    Menu->SB_PlayerDialogueOptions=NewObject<UScrollBox>(Menu);Menu->SB_PlayerDialogueOptions->AddChild(Button);Menu->SB_PlayerDialogueOptions->AddChild(Other);
    auto* Overlay=MakeWidget();Overlay->NarrativeComponent=Component;Overlay->Line_Main=NewObject<URichTextBlock>(Overlay);
    Overlay->Line_Main->SetText(FText::FromString(TEXT("Previous line")));Overlay->Line_Main->SetRenderOpacity(.65f);
    Overlay->DialogueBox=NewObject<USizeBox>(Overlay);Overlay->AvtProfile_Main=NewObject<USizeBox>(Overlay);
    Button->SetRenderOpacity(.72f);Button->SetVisibility(ESlateVisibility::Visible);Other->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    auto* State=World->GetSubsystem<UVTGResponseAudioSubsystem>();check(State);
    auto* RealOverlayClass=LoadClass<UUserWidget>(nullptr,TEXT("/Game/Narrative/NarrativeUI/Widgets/BP_Narrative3Overlay.BP_Narrative3Overlay_C"));check(RealOverlayClass);
    auto* RealOverlay=NewObject<UUserWidget>(World,RealOverlayClass);RealOverlay->Initialize();
    TArray<UWidget*> RealPresentation;
    for(FName Name:{FName(TEXT("DialogueBox")),FName(TEXT("DialogueBox_Side")),FName(TEXT("AvtProfile_Main")),FName(TEXT("AvtProfile_Side")),FName(TEXT("CharacterName_Main")),FName(TEXT("CharacterName_Side")),FName(TEXT("MonologueBox")),FName(TEXT("ThoughtBox")),FName(TEXT("MailBox"))})
    {
        auto* Widget=RealOverlay->GetWidgetFromName(Name);Expect(Widget!=nullptr,TEXT("real overlay presentation widget exists"));
        Widget->SetVisibility(ESlateVisibility::Visible);RealPresentation.Add(Widget);
    }
    auto Advance=[&](float Seconds){++GFrameCounter;World->GetTimerManager().Tick(Seconds);};
    Advance(.001f);
    Expect(State->BeginTransition(Button,Component,Back),TEXT("valid selection starts transition"));
    Expect(!State->BeginTransition(Button,Component,Back),TEXT("repeated confirmation rejected"));
    Expect(!Dialogue->CanSelectDialogueOption(Back) && !Dialogue->CanSkipCurrentLine(),TEXT("Narrative selection and skip share transition guard"));
    Expect(!Dialogue->SelectDialogueOption(Back),TEXT("direct selection cannot bypass guard"));
    Expect(Other->GetVisibility()==ESlateVisibility::Hidden && Button->GetVisibility()==ESlateVisibility::Visible,TEXT("selected appearance remains visible while Narrative blocks repeated input"));
    Expect(Overlay->Line_Main->GetRenderOpacity()==0 && Overlay->Line_Main->GetText().ToString()==TEXT("Previous line"),TEXT("old text hidden without changing its data"));
    Expect(Overlay->DialogueBox->GetRenderOpacity()==0 && Overlay->AvtProfile_Main->GetRenderOpacity()==0,TEXT("dialogue box and portrait hide with previous text"));
    for(auto* Widget:RealPresentation)Expect(Widget->GetVisibility()==ESlateVisibility::Hidden,TEXT("real unbound HUD overlay frame and portrait hidden"));
    auto* Confirm=NewObject<UAudioComponent>(PC);Confirm->bAutoActivate=false;Confirm->RegisterComponent();Confirm->SetPaused(true);UNarrativeBlipAudio::RegisterPriorityAudio(Confirm);State->ConfirmAudio=Confirm;
    auto* Quest=NewObject<UAudioComponent>(PC);Quest->bAutoActivate=false;Quest->RegisterComponent();Quest->SetPaused(true);UNarrativeBlipAudio::RegisterPriorityAudio(Quest);
    const float Duration=UVTGResponseAudioSubsystem::GetTransitionDuration();
    Expect(FMath::IsNearlyEqual(Duration,.66f,.001f),TEXT("UI deadline reduced ten percent and then another twenty percent"));
    Expect(FMath::IsNearlyEqual(Duration*.9f-UVTGResponseAudioSubsystem::GetButtonFadeStart(Duration),Duration*.3f/1.2f,.0001f),TEXT("button fade plays twenty percent faster"));
    Expect(GetDefault<UVTGUIFeedbackLibrary>()->ResponseLockedTexture!=nullptr,TEXT("locked response texture referenced for packaging"));
    auto* Backdrop=NewObject<UVTGResponseLockedBackdrop>(World);Backdrop->Initialize();
    // This headless fixture has no local-player context, so UUserWidget does not
    // invoke NativeOnInitialized automatically as CreateWidget does in gameplay.
    Backdrop->NativeOnInitialized();
    Expect(Backdrop->GetVisibility()==ESlateVisibility::Hidden,TEXT("locked backdrop starts hidden"));
    Backdrop->ShowFor(Button,GetDefault<UVTGUIFeedbackLibrary>()->ResponseLockedTexture,UVTGResponseAudioSubsystem::GetButtonFadeStart(Duration),Duration*.9f);
    Expect(Backdrop->GetVisibility()==ESlateVisibility::HitTestInvisible && Backdrop->Image->GetBrush().GetResourceObject()!=nullptr,TEXT("locked backdrop shows without intercepting input"));
    const auto* BackdropSlot=CastChecked<UCanvasPanelSlot>(Backdrop->Image->Slot);
    Expect(BackdropSlot->GetAnchors().Minimum==FVector2D::ZeroVector && BackdropSlot->GetAnchors().Maximum==FVector2D(1,1) && BackdropSlot->GetOffsets()==FMargin(0) && !BackdropSlot->GetAutoSize(),TEXT("locked texture stretches across entire viewport with zero margins"));
    for(const auto& Binding:Backdrop->Fade->MovieScene->GetBindings())for(auto* Track:Binding.GetTracks())for(auto* Section:Track->GetAllSections())
    {
        auto Values=CastChecked<UMovieSceneFloatSection>(Section)->GetChannel().GetData();
        Expect(Values.GetTimes().Num()==4 && Values.GetTimes()[1].Value==50 && Values.GetTimes()[3].Value==594,TEXT("backdrop fades in quickly and ends with selected button"));
    }
    Backdrop->HideFeedback();Expect(Backdrop->GetVisibility()==ESlateVisibility::Hidden,TEXT("backdrop teardown prevents reappearance"));
    Advance(Duration*.6f-.001f);Expect(Dialogue->IsResponseTransitionPending(),TEXT("no submit during selected hold"));
    Advance(Duration*.3f+.002f);Expect(Button->GetRenderOpacity()==0 && Dialogue->IsResponseTransitionPending(),TEXT("empty pause begins after fade"));
    Button->SetRenderOpacity(.72f); // Simulate UMG's later pre-animation-state restoration.
    Expect(Button->GetVisibility()==ESlateVisibility::Hidden,TEXT("late UMG opacity restoration cannot flash old option"));
    Advance(Duration*.1f-.002f);Expect(Dialogue->IsResponseTransitionPending(),TEXT("no submit before pool mean deadline"));
    Advance(.002f);
    Expect(!Dialogue->IsResponseTransitionPending() && Dialogue->VisibleChoices==1,TEXT("silent return submits once and publishes next menu"));
    Expect(Button->GetRenderOpacity()==0 && Button->GetVisibility()==ESlateVisibility::Hidden && Other->GetVisibility()==ESlateVisibility::Hidden && FMath::IsNearlyEqual(Overlay->Line_Main->GetRenderOpacity(),.65f),TEXT("consumed options remain hidden without a restoration flash"));
    Button->SetRenderOpacity(.72f);Button->SetVisibility(ESlateVisibility::Visible);Other->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Expect(IsValid(Confirm) && Confirm->bIsPaused,TEXT("confirmation component survives transition completion"));
    Expect(UNarrativeBlipAudio::HasPriorityAudio(World),TEXT("quest priority remains active"));
    UNarrativeBlipAudio::UnregisterPriorityAudio(Quest);
    Expect(UNarrativeBlipAudio::HasPriorityAudio(World),TEXT("confirmation tail retains priority after UI submission"));
    UNarrativeBlipAudio::UnregisterPriorityAudio(Confirm);
    Expect(!UNarrativeBlipAudio::HasPriorityAudio(World),TEXT("quest and confirmation priority remain independently owned"));
    Advance(.5f);Expect(Dialogue->VisibleChoices==1,TEXT("no duplicate submission after timers expire"));
    Component->OnDialogueOptionSelected.RemoveAll(Dialogue);Component->OnDialogueRepliesAvailable.RemoveAll(Dialogue);
    Dialogue->Prepare(Component,Back);Dialogue->WaitForChoice();
    Expect(State->BeginTransition(Button,Component,Back),TEXT("next transition works"));
    const float Remaining=World->GetTimerManager().GetTimerRemaining(State->SubmitTimer);
    World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
    World->Tick(LEVELTICK_All,.5f);
    Expect(Dialogue->IsResponseTransitionPending() && FMath::IsNearlyEqual(Remaining,World->GetTimerManager().GetTimerRemaining(State->SubmitTimer)),TEXT("game pause preserves remaining transition time"));
    World->GetWorldSettings()->SetPauserPlayerState(nullptr);
    State->CancelTransitionFor(Button);Expect(!Dialogue->IsResponseTransitionPending(),TEXT("button teardown cancels reservation"));
    Expect(Overlay->DialogueBox->GetRenderOpacity()==1 && Overlay->AvtProfile_Main->GetRenderOpacity()==1,TEXT("cancel restores box and portrait"));
    for(auto* Widget:RealPresentation)Expect(Widget->GetVisibility()==ESlateVisibility::Visible,TEXT("real overlay visibility restored on cancel"));
    Advance(.2f);Expect(Dialogue->CanSelectDialogueOption(Back),TEXT("cancelled timer never submits"));
    Expect(State->BeginTransition(Button,Component,Back),TEXT("transition before dialogue exit"));
    Component->OnDialogueFinished.Broadcast(Dialogue,false);Expect(!Dialogue->IsResponseTransitionPending(),TEXT("dialogue exit cancels immediately"));
    Advance(.2f);Expect(Dialogue->CanSelectDialogueOption(Back),TEXT("exit has no stale submission"));
    Expect(State->BeginTransition(Button,Component,Back),TEXT("transition before replacement"));
    Component->CurrentDialogue=nullptr;Advance(Duration+.01f);Expect(!Dialogue->IsResponseTransitionPending() && Dialogue->CanSelectDialogueOption(Back),TEXT("replaced dialogue never receives stale choice"));
    GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
    UE_LOG(LogTemp,Display,TEXT("RESPONSE TRANSITION TESTS: %d checks passed"),Checks);return true;
}
