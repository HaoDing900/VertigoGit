#include "UI/VTGPlaneSlideshow.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AVTGPlaneSlideshow::AVTGPlaneSlideshow()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Screen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Screen"));
    Screen->SetupAttachment(RootComponent);
    Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Screen->SetCastShadow(false);
    // Engine plane is XY. Keep local X horizontal and rotate Y into Z.
    Screen->SetRelativeRotation(FRotator(0, 0, 90));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (Plane.Succeeded()) Screen->SetStaticMesh(Plane.Object);
}

int32 AVTGPlaneSlideshow::FindImage(int32 From, bool bWrap) const
{
    if (Images.IsEmpty()) return INDEX_NONE;
    for (int32 Offset = 0; Offset < Images.Num(); ++Offset)
    {
        const int32 Candidate = From + Offset;
        if (!bWrap && Candidate >= Images.Num()) break;
        const int32 Index = ((Candidate % Images.Num()) + Images.Num()) % Images.Num();
        if (Images[Index]) return Index;
    }
    return INDEX_NONE;
}

void AVTGPlaneSlideshow::InitializeScreen()
{
    Screen->SetRelativeScale3D(FVector(FMath::Max(.01f, ScreenWidth) / 100.f, FMath::Max(.01f, ScreenHeight) / 100.f, 1.f));
    if (!ScreenMaterial) { DynamicMaterial = nullptr; return; }
    DynamicMaterial = UMaterialInstanceDynamic::Create(ScreenMaterial, this);
    Screen->SetMaterial(0, DynamicMaterial);
    DynamicMaterial->SetScalarParameterValue(TEXT("Brightness"), FMath::Max(0.f, Brightness));
    const int32 First = FindImage(0, false);
    Screen->SetVisibility(First != INDEX_NONE);
    SetImageImmediately(First);
}

void AVTGPlaneSlideshow::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    InitializeScreen();
}

void AVTGPlaneSlideshow::BeginPlay()
{
    Super::BeginPlay();
    InitializeScreen();
    SetActorTickEnabled(false);
    if (bAutoPlay) Play();
}

void AVTGPlaneSlideshow::SetImageImmediately(int32 Index)
{
    CurrentIndex = Index;
    bFading = false;
    SetActorTickEnabled(false);
    if (DynamicMaterial && Images.IsValidIndex(Index) && Images[Index])
    {
        Screen->SetVisibility(true);
        DynamicMaterial->SetTextureParameterValue(TEXT("ImageA"), Images[Index]);
        DynamicMaterial->SetTextureParameterValue(TEXT("ImageB"), Images[Index]);
        DynamicMaterial->SetScalarParameterValue(TEXT("Blend"), 0.f);
    }
}

void AVTGPlaneSlideshow::ScheduleNext()
{
    GetWorldTimerManager().ClearTimer(HoldTimer);
    if (!bPlaying || bFading || !DynamicMaterial || CurrentIndex == INDEX_NONE) return;
    const int32 Next = FindImage(CurrentIndex + 1, bLoop);
    if (Next == INDEX_NONE || Next == CurrentIndex) { bPlaying = false; return; }
    GetWorldTimerManager().SetTimer(HoldTimer, this, &AVTGPlaneSlideshow::NextImage, FMath::Max(.01f, HoldSeconds), false);
}

void AVTGPlaneSlideshow::Play()
{
    if (!DynamicMaterial) InitializeScreen();
    if (CurrentIndex == INDEX_NONE) SetImageImmediately(FindImage(0, false));
    bPlaying = true;
    if (bFading) SetActorTickEnabled(true);
    ScheduleNext();
}

void AVTGPlaneSlideshow::Pause()
{
    bPlaying = false;
    GetWorldTimerManager().ClearTimer(HoldTimer);
    SetActorTickEnabled(false);
}

void AVTGPlaneSlideshow::Restart()
{
    Pause();
    InitializeScreen();
    Play();
}

void AVTGPlaneSlideshow::ShowImage(int32 Index)
{
    if (!Images.IsValidIndex(Index) || !Images[Index]) return;
    if (!DynamicMaterial) InitializeScreen();
    SetImageImmediately(Index);
    ScheduleNext();
}

void AVTGPlaneSlideshow::NextImage()
{
    GetWorldTimerManager().ClearTimer(HoldTimer);
    if (!DynamicMaterial) return;
    if (bFading) FinishFade();
    const int32 Next = FindImage(CurrentIndex + 1, bLoop);
    if (Next == INDEX_NONE || Next == CurrentIndex) { bPlaying = false; return; }
    if (FadeSeconds <= 0.f || !Images.IsValidIndex(CurrentIndex) || !Images[CurrentIndex])
    { SetImageImmediately(Next); ScheduleNext(); return; }
    DynamicMaterial->SetTextureParameterValue(TEXT("ImageA"), Images[CurrentIndex]);
    DynamicMaterial->SetTextureParameterValue(TEXT("ImageB"), Images[Next]);
    DynamicMaterial->SetScalarParameterValue(TEXT("Blend"), 0.f);
    CurrentIndex = Next;
    FadeElapsed = 0.f;
    ActiveFadeSeconds = FadeSeconds;
    bFading = true;
    SetActorTickEnabled(true);
}

void AVTGPlaneSlideshow::FinishFade()
{
    SetImageImmediately(CurrentIndex);
}

void AVTGPlaneSlideshow::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bFading || !DynamicMaterial) return;
    FadeElapsed += DeltaSeconds;
    DynamicMaterial->SetScalarParameterValue(TEXT("Blend"), FMath::Clamp(FadeElapsed / ActiveFadeSeconds, 0.f, 1.f));
    if (FadeElapsed >= ActiveFadeSeconds) { FinishFade(); ScheduleNext(); }
}

void AVTGPlaneSlideshow::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(HoldTimer);
    Super::EndPlay(Reason);
}
