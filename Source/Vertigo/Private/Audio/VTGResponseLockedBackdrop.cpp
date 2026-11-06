#include "Audio/VTGResponseLockedBackdrop.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

void UVTGResponseLockedBackdrop::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Canvas;
    Image=WidgetTree->ConstructWidget<UImage>();Canvas->AddChildToCanvas(Image);
    SetVisibility(ESlateVisibility::Hidden);SetRenderOpacity(0.f);
}
void UVTGResponseLockedBackdrop::ShowFor(UUserWidget* Button,UTexture2D* Texture,float FadeStart,float FadeEnd)
{
    if(!Button || !Texture || !Image)return;
    // Fill the game viewport behind every dialogue layer, independent of button
    // geometry, DPI scale, window resizing and screen aspect ratio.
    auto* ImageSlot=CastChecked<UCanvasPanelSlot>(Image->Slot);
    ImageSlot->SetAnchors(FAnchors(0.f,0.f,1.f,1.f));
    ImageSlot->SetOffsets(FMargin(0.f));ImageSlot->SetAlignment(FVector2D::ZeroVector);ImageSlot->SetAutoSize(false);
    Image->SetBrushFromTexture(Texture);Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    SetVisibility(ESlateVisibility::HitTestInvisible);SetRenderOpacity(0.f);
    Fade=NewObject<UWidgetAnimation>(this);Fade->MovieScene=NewObject<UMovieScene>(Fade);
    auto* Scene=Fade->MovieScene.Get();Scene->SetTickResolutionDirectly(FFrameRate(1000,1));
    const int32 EndFrame=FMath::Max(2,FMath::RoundToInt(FadeEnd*1000));Scene->SetPlaybackRange(0,EndFrame);
    const FGuid Guid=Scene->AddPossessable(GetName(),GetClass());FWidgetAnimationBinding Binding;
    Binding.AnimationGuid=Guid;Binding.WidgetName=GetFName();Binding.bIsRootWidget=true;Fade->AnimationBindings.Add(Binding);
    auto* Track=Scene->AddTrack<UMovieSceneFloatTrack>(Guid);Track->SetPropertyNameAndPath(TEXT("RenderOpacity"),TEXT("RenderOpacity"));
    auto* Section=CastChecked<UMovieSceneFloatSection>(Track->CreateNewSection());Track->AddSection(*Section);
    Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0),FFrameNumber(EndFrame+1)));Section->EvalOptions.CompletionMode=EMovieSceneCompletionMode::KeepState;
    auto& Channel=Section->GetChannel();Channel.AddLinearKey(FFrameNumber(0),0.f);
    Channel.AddLinearKey(FFrameNumber(FMath::Min(50,EndFrame/4)),1.f);
    Channel.AddLinearKey(FFrameNumber(FMath::RoundToInt(FadeStart*1000)),1.f);Channel.AddLinearKey(FFrameNumber(EndFrame),0.f);
    if(GetWorld() && GetWorld()->GetGameViewport())AddToViewport(-1000);
    PlayAnimation(Fade,0,1,EUMGSequencePlayMode::Forward,1,false);
}
void UVTGResponseLockedBackdrop::HideFeedback()
{
    SetVisibility(ESlateVisibility::Hidden);StopAllAnimations();SetRenderOpacity(0.f);RemoveFromParent();
}
