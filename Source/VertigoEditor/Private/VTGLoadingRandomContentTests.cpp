#include "Loading/VTGLevelLoadingInfo.h"
#include "../../Vertigo/Private/Loading/SVTGLoadingFade.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "WidgetBlueprint.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"

bool VTGTestLoadingRandomContent()
{
    auto *BP = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Widget/Loading/WBP_VTGLoadingScreen"));
    auto *Existing = LoadObject<UVTGLevelLoadingInfo>(nullptr, TEXT("/Game/Widget/Loading/DA_LevelLoadingInfo"));
    if (!BP || !Existing || !BP->GeneratedClass) return false;
    auto *Widget = NewObject<UUserWidget>(GetTransientPackage(), BP->GeneratedClass);
    Widget->Initialize();
    auto *Info = NewObject<UVTGLevelLoadingInfo>();
    auto *Image = Cast<UImage>(Widget->WidgetTree->FindWidget(Info->BackgroundImageWidget));
    auto *Body = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(Info->WidgetNames.NewsBody));
    auto *Heading = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(Info->WidgetNames.NewsHeading));
    if (!Image || !Body || !Heading) return false;
    int Fail = 0;
    auto Expect = [&](bool OK, const TCHAR *Message)
    {
        Fail += !OK;
        UE_LOG(LogTemp, Display, TEXT("LOADING RANDOM %s: %s"), OK ? TEXT("PASS") : TEXT("FAIL"), Message);
    };
    const auto BrushBefore = Image->GetBrush();
    const auto TintBefore = Image->GetColorAndOpacity();
    UWidget *RootBefore = Widget->WidgetTree->RootWidget;
    const auto FontBefore = Body->GetFont();
    const FText FixedHeading = Heading->GetText();
    const FText FixedBody = Body->GetText();
    Info->ApplyToWidget(Widget, TEXT("Unknown"));
    Expect(Body->GetText().EqualTo(FixedBody) && Image->GetBrush().GetResourceObject() == BrushBefore.GetResourceObject() && Image->GetColorAndOpacity() == TintBefore,
           TEXT("empty pools preserve fixed news and designer image/tint"));
    auto *A = NewObject<UTexture2D>(); auto *B = NewObject<UTexture2D>();
    Info->SharedBackgroundImages = {nullptr, A, B};
    FVTGLoadingNews One; One.Heading = FText::FromString(TEXT("One")); One.Body = FText::FromString(TEXT("Body one"));
    FVTGLoadingNews Two; Two.Heading = FText::FromString(TEXT("Two")); Two.Body = FText::FromString(TEXT("Body two"));
    Info->NewsBodies = {FText::GetEmpty(), One.Body, Two.Body};
    bool Valid = true;
    for (int I = 0; I < 32; ++I)
    {
        Info->ApplyToWidget(Widget, TEXT("Unknown"));
        auto *Selected = Image->GetBrush().GetResourceObject();
        Valid &= (Selected == A || Selected == B) &&
                 ((Body->GetText().EqualTo(One.Body) && Heading->GetText().EqualTo(FixedHeading)) ||
                  (Body->GetText().EqualTo(Two.Body) && Heading->GetText().EqualTo(FixedHeading)));
    }
    Expect(Valid && Image->GetColorAndOpacity() == FLinearColor::White, TEXT("random image/news use only valid entries and matched heading/body"));
    Expect(Info->GetNewsForLevel(TEXT("AnyDestination")).Num() == 2, TEXT("all destinations share the body-only news pool; blank bodies are ignored"));
    Expect(!Widget->WidgetTree->FindWidget(TEXT("Title")) && !Widget->WidgetTree->FindWidget(TEXT("Subtitle")), TEXT("location and introduction widgets have been removed"));
    Expect(Heading->GetText().ToString() == TEXT("大渊城新闻"), TEXT("news rotation preserves the fixed city news label"));
    Expect(Widget->WidgetTree->RootWidget == RootBefore && Body->GetFont() == FontBefore, TEXT("layout and font are preserved"));
    if (!FSlateApplication::IsInitialized())
        FSlateApplication::InitializeAsStandaloneApplication(
            FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    auto SlateOwner = Widget->TakeWidget();
    auto BodySlate = Body->GetCachedWidget();
    Expect(BodySlate && BodySlate->GetType() == FName(TEXT("STextBlock")), TEXT("actual loading news exposes a Slate text widget"));
    if (!BodySlate || BodySlate->GetType() != FName(TEXT("STextBlock"))) return false;
    auto Text = StaticCastSharedPtr<STextBlock>(BodySlate);
    auto Fade = SNew(SVTGLoadingFade)[SlateOwner];
    int Changed = 0;
    FText Last = One.Body;
    bool NoRepeat = true;
    auto Update = [Text, &Changed, &Last, &NoRepeat](const FVTGLoadingNews &Item)
    {
        NoRepeat &= !Last.EqualTo(Item.Body); Last = Item.Body; ++Changed; Text->SetText(Item.Body);
    };
    Fade->SetNewsRotation({One, Two}, 5.f, 0, Update);
    Fade->AdvanceNews(4.f);
    Expect(Changed == 0, TEXT("news stays visible until its configured interval"));
    for (int I = 0; I < 10; ++I) Fade->AdvanceNews(5.f);
    Expect(Changed == 10 && NoRepeat && Text->GetText().EqualTo(Last), TEXT("Slate rotation continues without game timers and avoids adjacent repeats"));
    Fade->BeginFadeOut(.5f); Fade->AdvanceNews(5.f);
    Expect(Changed == 10, TEXT("fade-out stops news rotation"));
    auto Single = SNew(SVTGLoadingFade)[SNew(STextBlock)];
    Single->SetNewsRotation({One}, 1.f, 0, Update); Single->AdvanceNews(10.f);
    Single->SetNewsRotation({One, Two}, 0.f, 0, Update); Single->AdvanceNews(10.f);
    Expect(Changed == 10, TEXT("one item and zero interval remain static"));
    return Fail == 0;
}


