#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "NarrativeEvent.h"
#include "VTGSlideshow.generated.h"

class UImage;
class UTexture2D;

UENUM(BlueprintType)
enum class EVTGSlideFit : uint8
{
 Stretch,
 Cover,
 Contain
};

/** Translation and shake use UMG logical pixels, before viewport DPI scaling. */
USTRUCT(BlueprintType)
struct VERTIGO_API FVTGSlideMotion
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion", meta=(ClampMin="0.01")) float Duration = 8.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion") FVector2D StartScale = FVector2D(1.05, 1.05);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion") FVector2D EndScale = FVector2D(1.15, 1.15);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion") FVector2D StartOffset = FVector2D::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion") FVector2D EndOffset = FVector2D::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion") float StartAngle = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion") float EndAngle = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion") bool bEaseInOut = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion") bool bLoopPingPong = false;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shake", meta=(ClampMin="0")) float ShakeAmplitude = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shake", meta=(ClampMin="0")) float ShakeFrequency = 8.f;
 /** Zero means shake for the whole time the slide is displayed. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shake", meta=(ClampMin="0")) float ShakeDuration = 0.f;
 /** Cover preserves aspect ratio and crops overflow; Contain shows the whole image with black bars. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto Fit") EVTGSlideFit ImageFit = EVTGSlideFit::Stretch;
 /** Automatically pan from the top edge to the bottom edge of a tall image. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Auto Fit") bool bPanTopToBottom = false;
 FWidgetTransform Evaluate(float Age, FVector2D ImageSize = FVector2D(1920,1080), FVector2D CanvasSize = FVector2D(1920,1080)) const;
};

USTRUCT(BlueprintType)
struct VERTIGO_API FVTGSlide
{
 GENERATED_BODY()
 /** Show Slide identifier; zero uses the array position + 1. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) int32 ImageIndex = 0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide") TObjectPtr<UTexture2D> Image;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide", meta=(ClampMin="0")) float FadeSeconds = 0.5f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slide") FVTGSlideMotion Motion;
};

/** Built entirely in C++; no designer setup is needed. */
UCLASS()
class VERTIGO_API UVTGSlideshowWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 void Show(const FVTGSlide& Slide);
 void ChangeMotion(const FVTGSlideMotion& Motion);
 void StopMotion();
 void FadeBackToGame(float Duration);
protected:
 virtual void NativeOnInitialized() override;
 virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
 UPROPERTY(Transient) TObjectPtr<UImage> Images[2];
 FVTGSlideMotion Motions[2];
 float Ages[2] = {0.f, 0.f};
 bool Moving[2] = {false, false};
 int32 Active = -1;
 int32 Previous = -1;
 float FadeAge = 0.f;
 float FadeDuration = 0.f;
 bool bReturningToGame = false;
 float ReturnAge = 0.f;
 float ReturnDuration = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVTGSlideChanged, int32, SlideNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVTGSlideshowClosed);

/** Place one in the level, fill Slides, then call Show Slide from any blueprint. */
UCLASS(Blueprintable)
class VERTIGO_API AVTGSlideshowDirector : public AActor
{
 GENERATED_BODY()
public:
 AVTGSlideshowDirector();
 UFUNCTION(BlueprintPure, Category="Slideshow", meta=(WorldContext="WorldContextObject"))
 static AVTGSlideshowDirector* FindSlideshowByID(const UObject* WorldContextObject, FName ID);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow") TArray<FVTGSlide> Slides;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow") FName SlideshowID = "Ending";
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow") int32 ViewportZOrder = -1;
 /** Off: fade the slideshow away and return to gameplay. On: fade to black and retain it for ending transitions. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow") bool bKeepBlackScreenOnClose = false;
 /** Optional. Otherwise uses the first local player controller. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow") TObjectPtr<APlayerController> OwningPlayer;
 UPROPERTY(BlueprintReadOnly, Category="Slideshow") int32 CurrentSlideNumber = 0;
 UPROPERTY(BlueprintAssignable, Category="Slideshow") FVTGSlideChanged OnSlideChanged;
 UPROPERTY(BlueprintAssignable, Category="Slideshow") FVTGSlideshowClosed OnSlideshowClosed;
 /** Matches Image Index (default: array position + 1). Duplicate/missing IDs leave the current slide unchanged. */
 UFUNCTION(BlueprintCallable, Category="Slideshow") bool ShowSlide(int32 SlideNumber);
 UFUNCTION(BlueprintCallable, Category="Slideshow") bool NextSlide();
 UFUNCTION(BlueprintCallable, Category="Slideshow") bool PreviousSlide();
 /** Restart motion on the current image using these settings. */
 UFUNCTION(BlueprintCallable, Category="Slideshow") void SetCurrentMotion(FVTGSlideMotion Motion);
 UFUNCTION(BlueprintCallable, Category="Slideshow") void StopCurrentMotion();
 /** Fade out and remove the slideshow by default, or keep black if Keep Black Screen On Close is enabled. After the fade, On Slideshow Closed fires. */
 UFUNCTION(BlueprintCallable, Category="Slideshow") void CloseSlideshow(float FadeSeconds = 0.5f);
 UFUNCTION(BlueprintCallable, Category="Slideshow") void RemoveSlideshow();
protected:
 virtual void PostLoad() override;
#if WITH_EDITOR
 virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& Event) override;
#endif
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(Transient) TObjectPtr<UVTGSlideshowWidget> Widget;
 FTimerHandle CloseTimer;
 void FinishClose();
 int32 GetImageIndex(int32 ArrayIndex) const;
 void AssignAutomaticImageIndices();
};

/** Select in a Narrative node's Events list; no event blueprint is required. */
UCLASS(DisplayName="VTG Show Slide")
class VERTIGO_API UVTGShowSlideEvent : public UNarrativeEvent
{
 GENERATED_BODY()
public:
 UVTGShowSlideEvent(const FObjectInitializer& Initializer);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow") FName SlideshowID = "Ending";
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow", meta=(ClampMin="1")) int32 SlideNumber = 1;
 virtual void ExecuteEvent_Implementation(APawn* Pawn, APlayerController* Controller, UNarrativeComponent* NarrativeComponent) override;
 virtual FString GetGraphDisplayText_Implementation() override;
};

UCLASS(DisplayName="VTG Close Slideshow")
class VERTIGO_API UVTGCloseSlideshowEvent : public UNarrativeEvent
{
 GENERATED_BODY()
public:
 UVTGCloseSlideshowEvent(const FObjectInitializer& Initializer);
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow") FName SlideshowID = "Ending";
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow", meta=(ClampMin="0")) float FadeSeconds = 0.5f;
 virtual void ExecuteEvent_Implementation(APawn* Pawn, APlayerController* Controller, UNarrativeComponent* NarrativeComponent) override;
 virtual FString GetGraphDisplayText_Implementation() override;
};






