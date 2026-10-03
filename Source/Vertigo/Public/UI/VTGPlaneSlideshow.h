#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VTGPlaneSlideshow.generated.h"

class UStaticMeshComponent;
class UTexture2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** A world-space picture screen. Ticks only while crossfading. */
UCLASS(Blueprintable)
class VERTIGO_API AVTGPlaneSlideshow : public AActor
{
    GENERATED_BODY()
public:
    AVTGPlaneSlideshow();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Screen")
    TObjectPtr<UStaticMeshComponent> Screen;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow", meta=(ToolTip="Drop Texture2D assets here in playback order. Empty entries are skipped."))
    TArray<TObjectPtr<UTexture2D>> Images;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow", meta=(ClampMin="0.01", Units="s"))
    float HoldSeconds = 4.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow", meta=(ClampMin="0", Units="s"))
    float FadeSeconds = .5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow")
    bool bAutoPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Slideshow")
    bool bLoop = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screen", meta=(ClampMin="0.01", Units="cm"))
    float ScreenWidth = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screen", meta=(ClampMin="0.01", Units="cm"))
    float ScreenHeight = 45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screen", meta=(ClampMin="0"))
    float Brightness = 1.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Screen")
    TObjectPtr<UMaterialInterface> ScreenMaterial;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Slideshow")
    int32 CurrentIndex = INDEX_NONE;

    UFUNCTION(BlueprintCallable, Category="Slideshow") void Play();
    UFUNCTION(BlueprintCallable, Category="Slideshow") void Pause();
    UFUNCTION(BlueprintCallable, Category="Slideshow") void Restart();
    UFUNCTION(BlueprintCallable, Category="Slideshow") void NextImage();
    UFUNCTION(BlueprintCallable, Category="Slideshow") void ShowImage(int32 Index);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;
    FTimerHandle HoldTimer;
    bool bPlaying = false;
    bool bFading = false;
    float FadeElapsed = 0.f;
    float ActiveFadeSeconds = 0.f;
    int32 FindImage(int32 From, bool bWrap) const;
    void InitializeScreen();
    void SetImageImmediately(int32 Index);
    void ScheduleNext();
    void FinishFade();
};
