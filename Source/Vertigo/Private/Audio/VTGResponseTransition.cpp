#include "Audio/VTGResponseAudioSubsystem.h"
#include "Audio/VTGResponseLockedBackdrop.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "CommonButtonBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Dialogue.h"
#include "DialogueSM.h"
#include "NarrativeComponent.h"
#include "NarrativeBlipLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "Audio/VTGUIFeedbackLibrary.h"
#include "Sound/SoundBase.h"

namespace
{
template<class T> T* ObjectField(UObject* Object,FName Name)
{
    if(!IsValid(Object))return nullptr;
    if(auto* Property=FindFProperty<FObjectPropertyBase>(Object->GetClass(),Name))
        return Cast<T>(Property->GetObjectPropertyValue_InContainer(Object));
    return nullptr;
}
UWidgetAnimation* MakeFade(UObject* Outer,UUserWidget* Button,float Duration)
{
    auto* Animation=NewObject<UWidgetAnimation>(Outer);
    Animation->MovieScene=NewObject<UMovieScene>(Animation);
    const int32 Frames=FMath::Max(1,FMath::RoundToInt(Duration*1000));
    auto* Scene=Animation->MovieScene.Get();Scene->SetTickResolutionDirectly(FFrameRate(1000,1));Scene->SetPlaybackRange(0,Frames);
    const FGuid Guid=Scene->AddPossessable(Button->GetName(),Button->GetClass());
    FWidgetAnimationBinding Binding;Binding.AnimationGuid=Guid;Binding.WidgetName=Button->GetFName();Binding.bIsRootWidget=true;
    Animation->AnimationBindings.Add(Binding);
    auto* Track=Scene->AddTrack<UMovieSceneFloatTrack>(Guid);Track->SetPropertyNameAndPath(TEXT("RenderOpacity"),TEXT("RenderOpacity"));
    auto* Section=CastChecked<UMovieSceneFloatSection>(Track->CreateNewSection());Track->AddSection(*Section);
    Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0),FFrameNumber(Frames+1)));
    Section->EvalOptions.CompletionMode=EMovieSceneCompletionMode::KeepState;
    Section->GetChannel().AddLinearKey(FFrameNumber(0),Button->GetRenderOpacity());
    Section->GetChannel().AddLinearKey(FFrameNumber(Frames),0.f);
    return Animation;
}
}
float UVTGResponseAudioSubsystem::GetTransitionDuration()
{
    float Sum=0;int32 Count=0;
    for(USoundBase* Sound:GetDefault<UVTGUIFeedbackLibrary>()->ResponseSounds)
        if(IsValid(Sound) && !Sound->IsLooping() && FMath::IsFinite(Sound->GetDuration()) && Sound->GetDuration()>0)
        {Sum+=Sound->GetDuration();++Count;}
    // Previous UI scale .7, then 10% and another 20% shorter, independently of playback.
    return Count?FMath::Max(.01f,FMath::RoundToFloat(Sum/Count*.7f*.9f*.8f*100.f)/100.f):.1008f;
}

bool UVTGResponseAudioSubsystem::BeginTransition(UUserWidget* Button,UNarrativeComponent* Component,UDialogueNode_Player* Option)
{
    if(TransitionToken || !IsValid(Button) || !IsValid(Component))return false;
    auto* Dialogue=Component->GetCurrentDialogue();if(!IsValid(Dialogue))return false;
    auto* Menu=ObjectField<UUserWidget>(Button,TEXT("OwningWidget"));
    auto* Panel=ObjectField<UPanelWidget>(Menu,TEXT("SB_PlayerDialogueOptions"));
    if(!Panel || !Panel->HasChild(Button))return false;
    const uint32 Token=Dialogue->BeginResponseTransition(Option);if(!Token)return false;
    TransitionToken=Token;PendingDialogue=Dialogue;PendingComponent=Component;PendingOption=Option;SelectedButton=Button;
    Component->OnDialogueFinished.AddUniqueDynamic(this,&UVTGResponseAudioSubsystem::DialogueEnded);
    // Faster button fade: retain the same fade-end/empty-pause deadlines.
    TransitionDuration=GetTransitionDuration();
    GetWorld()->GetTimerManager().SetTimer(StartFadeTimer,this,&UVTGResponseAudioSubsystem::StartFade,GetButtonFadeStart(TransitionDuration),false);
    GetWorld()->GetTimerManager().SetTimer(FadeTimer,this,&UVTGResponseAudioSubsystem::FinishFade,TransitionDuration*.9f,false);
    GetWorld()->GetTimerManager().SetTimer(SubmitTimer,this,&UVTGResponseAudioSubsystem::SubmitResponse,TransitionDuration,false);
    Confirm();
    if(Button->GetOwningPlayer() && GetWorld()->GetGameViewport())
    {
        LockedBackdrop=CreateWidget<UVTGResponseLockedBackdrop>(Button->GetOwningPlayer());
        if(LockedBackdrop)LockedBackdrop->ShowFor(Button,GetDefault<UVTGUIFeedbackLibrary>()->ResponseLockedTexture,GetButtonFadeStart(TransitionDuration),TransitionDuration*.9f);
    }
    GetWorld()->GetTimerManager().SetTimerForNextTick(this,&UVTGResponseAudioSubsystem::StartConfirmAppearance);
    for(UWidget* Widget:Panel->GetAllChildren())
    {
        TransitionWidgets.Add({Widget,Widget->GetVisibility(),Widget->GetRenderOpacity(),true});
        // Keep the clicked button's native hover state. Narrative owns the input guard.
        if(Widget!=Button)Widget->SetVisibility(ESlateVisibility::Hidden);
    }
    // The live dialogue overlay owns the four line displays, separately from the option menu.
    // Hide presentation only: leave text values, bindings, history, names and artwork untouched.
    for(TObjectIterator<UUserWidget> It;It;++It)
    {
        if(It->IsTemplate() || It->GetWorld()!=GetWorld())continue;
        // The real overlay can be owned by the HUD while its NarrativeComponent field is
        // unassigned. Resolve that local player's overlay as well as explicitly bound widgets.
        const bool Bound=ObjectField<UNarrativeComponent>(*It,TEXT("NarrativeComponent"))==Component;
        const bool LocalOverlay=It->GetClass()->GetName()==TEXT("BP_Narrative3Overlay_C") &&
            It->GetOwningPlayer()==Button->GetOwningPlayer();
        if(!Bound && !LocalOverlay)continue;
        for(FName Name:{FName(TEXT("Line_Main")),FName(TEXT("Line_Side")),FName(TEXT("Line_Cinematic")),FName(TEXT("Line_?")),FName(TEXT("DialogueBox")),FName(TEXT("DialogueBox_Side")),FName(TEXT("AvtProfile_Main")),FName(TEXT("AvtProfile_Side")),FName(TEXT("CharacterName_Main")),FName(TEXT("CharacterName_Side")),FName(TEXT("MonologueBox")),FName(TEXT("ThoughtBox")),FName(TEXT("MailBox"))})
            if(auto* Line=ObjectField<UWidget>(*It,Name)?ObjectField<UWidget>(*It,Name):It->GetWidgetFromName(Name))
            {
                TransitionWidgets.Add({Line,Line->GetVisibility(),Line->GetRenderOpacity(),true});
                Line->SetVisibility(ESlateVisibility::Hidden);Line->SetRenderOpacity(0.f);
            }
    }
    return true;
}

void UVTGResponseAudioSubsystem::StartConfirmAppearance()
{
    auto* Button=SelectedButton.Get();
    if(!TransitionToken || !Button)return;
    auto* Root=Button->WidgetTree?Cast<UButton>(Button->WidgetTree->RootWidget):nullptr;
    auto* Common=Cast<UCommonButtonBase>(Button);
    if(!Root || !Common || !Common->GetStyle() || !Root->GetContent())return;
    auto* Content=Root->GetContent();
    auto* Slot=Cast<UButtonSlot>(Content->Slot);
    const FMargin Padding=Slot?Slot->GetPadding():FMargin(0);
    const auto Horizontal=Slot?Slot->GetHorizontalAlignment():HAlign_Fill;
    const auto Vertical=Slot?Slot->GetVerticalAlignment():VAlign_Fill;
    auto* Overlay=Button->WidgetTree->ConstructWidget<UOverlay>();
    auto* Image=Button->WidgetTree->ConstructWidget<UImage>();
    Image->SetBrush(Common->GetStyle()->NormalPressed);
    Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    Root->SetContent(Overlay);
    auto* ImageSlot=Overlay->AddChildToOverlay(Image);ImageSlot->SetHorizontalAlignment(HAlign_Fill);ImageSlot->SetVerticalAlignment(VAlign_Fill);
    auto* ContentSlot=Overlay->AddChildToOverlay(Content);ContentSlot->SetHorizontalAlignment(HAlign_Fill);ContentSlot->SetVerticalAlignment(VAlign_Fill);
    auto* RootSlot=CastChecked<UButtonSlot>(Overlay->Slot);RootSlot->SetPadding(Padding);RootSlot->SetHorizontalAlignment(Horizontal);RootSlot->SetVerticalAlignment(Vertical);
    AppearanceOverlay=Overlay;
    ConfirmAppearance=NewObject<UWidgetAnimation>(this);
    ConfirmAppearance->MovieScene=NewObject<UMovieScene>(ConfirmAppearance);
    auto* Scene=ConfirmAppearance->MovieScene.Get();Scene->SetTickResolutionDirectly(FFrameRate(1000,1));Scene->SetPlaybackRange(0,403);
    const FGuid Guid=Scene->AddPossessable(Image->GetName(),Image->GetClass());
    FWidgetAnimationBinding Binding;Binding.AnimationGuid=Guid;Binding.WidgetName=Image->GetFName();ConfirmAppearance->AnimationBindings.Add(Binding);
    auto* Track=Scene->AddTrack<UMovieSceneFloatTrack>(Guid);Track->SetPropertyNameAndPath(TEXT("RenderOpacity"),TEXT("RenderOpacity"));
    auto* Section=CastChecked<UMovieSceneFloatSection>(Track->CreateNewSection());Track->AddSection(*Section);Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0),FFrameNumber(404)));Section->EvalOptions.CompletionMode=EMovieSceneCompletionMode::KeepState;
    Section->GetChannel().AddLinearKey(FFrameNumber(0),1.f);Section->GetChannel().AddLinearKey(FFrameNumber(252),1.f);Section->GetChannel().AddLinearKey(FFrameNumber(403),0.f);
    Button->PlayAnimation(ConfirmAppearance,0,1,EUMGSequencePlayMode::Forward,1,false);
}

void UVTGResponseAudioSubsystem::StartFade()
{
    if(!TransitionToken)return;
    if(auto* Button=SelectedButton.Get())
    {
        const float Remaining=GetWorld()->GetTimerManager().GetTimerRemaining(SubmitTimer)-TransitionDuration*.1f;
        if(Remaining<=0){FinishFade();return;}
        ResponseFade=MakeFade(this,Button,Remaining);
        Button->PlayAnimation(ResponseFade,0,1,EUMGSequencePlayMode::Forward,1,false);
    }
}

void UVTGResponseAudioSubsystem::FinishFade()
{
    if(!TransitionToken)return;
    if(LockedBackdrop)LockedBackdrop->HideFeedback();
    if(auto* Button=SelectedButton.Get())
    {
        // UMG may restore pre-animation opacity when its sequence finishes on a later
        // Slate update. Visibility is an independent latch and cannot flash with it.
        Button->SetVisibility(ESlateVisibility::Hidden);
        if(ResponseFade)Button->StopAnimation(ResponseFade);
        Button->SetRenderOpacity(0.f);
    }
}

void UVTGResponseAudioSubsystem::RestoreTransitionUI()
{
    if(LockedBackdrop){LockedBackdrop->HideFeedback();LockedBackdrop=nullptr;}
    if(auto* Button=SelectedButton.Get())if(ConfirmAppearance)Button->StopAnimation(ConfirmAppearance);
    if(auto* Overlay=AppearanceOverlay.Get())if(auto* Root=Cast<UButton>(Overlay->GetParent()))
    {
        auto* Content=Overlay->GetChildrenCount()>1?Overlay->GetChildAt(1):nullptr;
        auto* Slot=CastChecked<UButtonSlot>(Overlay->Slot);
        const auto Padding=Slot->GetPadding();const auto H=Slot->GetHorizontalAlignment();const auto V=Slot->GetVerticalAlignment();
        if(Content){Overlay->RemoveChild(Content);Root->SetContent(Content);auto* NewSlot=CastChecked<UButtonSlot>(Content->Slot);NewSlot->SetPadding(Padding);NewSlot->SetHorizontalAlignment(H);NewSlot->SetVerticalAlignment(V);}
    }
    AppearanceOverlay.Reset();ConfirmAppearance=nullptr;
    if(auto* Button=SelectedButton.Get())if(ResponseFade)Button->StopAnimation(ResponseFade);
    for(const auto& State:TransitionWidgets)if(auto* Widget=State.Widget.Get())
    {
        Widget->SetRenderOpacity(State.Opacity);
        if(State.bChangedVisibility)Widget->SetVisibility(State.Visibility);
    }
    TransitionWidgets.Empty();ResponseFade=nullptr;
}

void UVTGResponseAudioSubsystem::CancelTransition()
{
    if(!TransitionToken)return;
    const auto Token=TransitionToken;TransitionToken=0;
    if(auto* World=GetWorld()){World->GetTimerManager().ClearTimer(StartFadeTimer);World->GetTimerManager().ClearTimer(FadeTimer);World->GetTimerManager().ClearTimer(SubmitTimer);}
    if(auto* Component=PendingComponent.Get())Component->OnDialogueFinished.RemoveDynamic(this,&UVTGResponseAudioSubsystem::DialogueEnded);
    // The confirmation tail keeps its priority until playback naturally finishes.
    RestoreTransitionUI();
    if(auto* Dialogue=PendingDialogue.Get())Dialogue->EndResponseTransition(Token);
    SelectedButton.Reset();PendingDialogue.Reset();PendingComponent.Reset();PendingOption.Reset();
}

void UVTGResponseAudioSubsystem::SubmitResponse()
{
    auto* Component=PendingComponent.Get();auto* Dialogue=PendingDialogue.Get();auto* Option=PendingOption.Get();
    const bool Valid=TransitionToken && Component && Dialogue && SelectedButton.IsValid() &&
        Component->GetCurrentDialogue()==Dialogue && Dialogue->OwnsResponseTransition(TransitionToken) && Dialogue->IsWaitingForPlayerResponse();
    // Restore and detach first: silent routing may synchronously destroy widgets and create a new menu.
    // Keep old option widgets invisible during synchronous menu rebuild and destruction.
    // Cancellation still restores their original presentation exactly.
    if(Valid)for(auto& State:TransitionWidgets)
        if(auto* Widget=State.Widget.Get())if(Widget==SelectedButton.Get() || Widget->GetParent()==ObjectField<UPanelWidget>(ObjectField<UUserWidget>(SelectedButton.Get(),TEXT("OwningWidget")),TEXT("SB_PlayerDialogueOptions")))
        {
            Widget->SetVisibility(ESlateVisibility::Hidden);
            State.Visibility=ESlateVisibility::Hidden;
            State.Opacity=0.f;
        }
    CancelTransition();
    if(Valid && Dialogue->CanSelectDialogueOption(Option))Component->TrySelectDialogueOption(Option);
}
void UVTGResponseAudioSubsystem::CancelTransitionFor(UUserWidget* Button)
{
    if(SelectedButton==Button)CancelTransition();
}
void UVTGResponseAudioSubsystem::DialogueEnded(UDialogue* Dialogue,bool)
{
    if(PendingDialogue==Dialogue)CancelTransition();
}
