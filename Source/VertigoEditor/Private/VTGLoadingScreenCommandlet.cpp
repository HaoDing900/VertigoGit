#include "VTGLoadingScreenCommandlet.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "Loading/VTGLevelLoadingInfo.h"
#include "Loading/VTGMediaLoadingPageSystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CircularThrobber.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

UVTGLoadingScreenCommandlet::UVTGLoadingScreenCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

bool VTGTestLoadingRandomContent();
bool VTGTestLoadingTransitionTiming();
int32 VTGConnectLevelLoading(const FString& Params);
int32 VTGSimplifyLoadingScreen();
int32 VTGDeathBackground(const FString& Params);

int32 UVTGLoadingScreenCommandlet::Main(const FString &Params)
{
    if (FParse::Param(*Params, TEXT("VerifyTiming"))) return VTGTestLoadingTransitionTiming()?0:82;
    if (FParse::Param(*Params, TEXT("ConnectTransitions"))) return VTGConnectLevelLoading(Params);
    if (FParse::Param(*Params, TEXT("DeathBackground"))) return VTGDeathBackground(Params);
    if (FParse::Param(*Params, TEXT("VerifyRandomContent")))
        return VTGTestLoadingRandomContent() ? 0 : 40;
    if (FParse::Param(*Params, TEXT("Simplify"))) return VTGSimplifyLoadingScreen();
    if (FParse::Param(*Params, TEXT("VerifyLevelInfo"))) return VTGTestLoadingRandomContent() ? 0 : 40;
    if (FParse::Param(*Params, TEXT("CreateLevelInfo")))
    {
        const TCHAR *Path = TEXT("/Game/Widget/Loading/DA_LevelLoadingInfo");
        if (LoadObject<UVTGLevelLoadingInfo>(nullptr, Path)) return 0;
        auto *Package = CreatePackage(Path);
        auto *Info = NewObject<UVTGLevelLoadingInfo>(Package, TEXT("DA_LevelLoadingInfo"), RF_Public | RF_Standalone);
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
        return UPackage::SavePackage(Package, Info, *FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension()), Args) ? 0 : 21;
    }
    if (FParse::Param(*Params, TEXT("ConnectMenu")))
    {
        auto *Menu = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu"));
        if (!Menu)
            return 10;
        TArray<UEdGraph *> Graphs;
        Menu->GetAllGraphs(Graphs);
        int32 Changed = 0;
        for (UEdGraph *Graph : Graphs)
            for (UEdGraphNode *Node : Graph->Nodes)
                if (auto *Call = Cast<UK2Node_CallFunction>(Node))
                    if (Call->FunctionReference.GetMemberName() == TEXT("OpenLevelBySoftObjectPtr"))
                    {
                        UE_LOG(LogTemp, Display, TEXT("Menu travel node: %s / %s"), *Graph->GetName(),
                               *Call->GetName());
                        for (const auto *Pin : Call->Pins)
                            UE_LOG(LogTemp, Display, TEXT("  %s = %s %s; links=%d"), *Pin->PinName.ToString(),
                                   *Pin->DefaultValue, *GetPathNameSafe(Pin->DefaultObject), Pin->LinkedTo.Num());
                        Call->SetFromFunction(UVTGMediaLoadingPageSystem::StaticClass()->FindFunctionByName(
                            TEXT("OpenLevelWithLoadingScreen")));
                        Call->ReconstructNode();
                        ++Changed;
                    }
        if (!Changed)
        {
            UE_LOG(LogTemp, Display, TEXT("Menu already connected or no matching travel node."));
            return 0;
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Menu);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(Menu, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
        if (Results.NumErrors)
            return 11;
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(Menu->GetOutermost(), Menu,
                                   *FPackageName::LongPackageNameToFilename(Menu->GetOutermost()->GetName(),
                                                                            FPackageName::GetAssetPackageExtension()),
                                   Args))
            return 12;
        UE_LOG(LogTemp, Display, TEXT("Menu loading integration saved: %d travel node(s)."), Changed);
        return 0;
    }
    const TCHAR *Path = TEXT("/Game/Widget/Loading/WBP_VTGLoadingScreen");
    UWidgetBlueprint *BP = LoadObject<UWidgetBlueprint>(nullptr, Path);
    const bool Verify = FParse::Param(*Params, TEXT("VerifyOnly"));
    if (!BP && Verify)
        return 1;
    if (!BP)
    {
        UPackage *Package = CreatePackage(Path);
        BP = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, TEXT("WBP_VTGLoadingScreen"), BPTYPE_Normal,
            UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
        UWidgetTree *Tree = BP->WidgetTree;
        auto *Root = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Screen"));
        Tree->RootWidget = Root;
        auto *BG = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackgroundArt"));
        BG->SetColorAndOpacity(FLinearColor(.012f, .022f, .025f, 1));
        auto *BGSlot = Root->AddChildToOverlay(BG);
        BGSlot->SetHorizontalAlignment(HAlign_Fill);
        BGSlot->SetVerticalAlignment(VAlign_Fill);
        auto *Scale = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ResponsiveLayout"));
        Scale->SetStretch(EStretch::ScaleToFit);
        auto *SS = Root->AddChildToOverlay(Scale);
        SS->SetHorizontalAlignment(HAlign_Fill);
        SS->SetVerticalAlignment(VAlign_Fill);
        auto *Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DesignSize1920x1080"));
        Size->SetWidthOverride(1920);
        Size->SetHeightOverride(1080);
        Scale->AddChild(Size);
        auto *Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EditableLayout"));
        Size->AddChild(Canvas);
        auto Place = [&](UWidget *W, float X, float Y, float Width, float Height)
        {
            auto *Slot = Canvas->AddChildToCanvas(W);
            Slot->SetPosition(FVector2D(X, Y));
            Slot->SetSize(FVector2D(Width, Height));
        };
        const FLinearColor Accent(.28f, .65f, .61f, 1);
        const FLinearColor Muted(.29f, .38f, .39f, 1);
        auto Rect = [&](const TCHAR *Name, float X, float Y, float W, float H, FLinearColor Color)
        {
            auto *I = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
            I->SetColorAndOpacity(Color);
            Place(I, X, Y, W, H);
        };
        auto *Font = LoadObject<UFont>(nullptr, TEXT("/Game/Fonts/Noto_Sans_SC/static/NotoSansSC-Regular_Font"));
        if (!Font)
            return 2;
        auto Text = [&](const TCHAR *Name, const TCHAR *Value, float X, float Y, float W, float H, int32 Points,
                        FLinearColor Color)
        {
            auto *T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
            T->SetText(FText::FromString(Value));
            T->SetFont(FSlateFontInfo(Font, Points));
            T->SetColorAndOpacity(FSlateColor(Color));
            Place(T, X, Y, W, H);
            return T;
        };
        // All artwork is ordinary UMG content: no runtime code overrides designer properties.
        Rect(TEXT("AccentRule"), 112, 110, 64, 3, Accent);
        Text(TEXT("ChapterLabel"), TEXT("THE GREAT ABYSS CITY"), 112, 130, 1000, 50, 18, Muted);
        // A quiet architectural motif; artists may replace this entire canvas with their own art.
        for (int32 I = 0; I < 5; ++I)
        {
            const float X = 1160 + I * 45, Y = 205 + I * 44, W = 490 - I * 70, H = 600 - I * 80;
            const FLinearColor C(.035f + I * .006f, .074f + I * .008f, .078f + I * .008f, 1);
            Rect(*FString::Printf(TEXT("TunnelLeft%d"), I), X, Y, 2, H, C);
            Rect(*FString::Printf(TEXT("TunnelTop%d"), I), X, Y, W, 2, C);
            Rect(*FString::Printf(TEXT("TunnelRight%d"), I), X + W, Y, 2, H, C);
        }
        Rect(TEXT("FooterRule"), 112, 870, 1696, 1, Muted);
        Text(TEXT("TipLabel"), TEXT("大渊城新闻"), 112, 906, 180, 40, 16, Accent);
        auto *Tip = Text(TEXT("TipText"), TEXT("脚步声会在通道中回响。留意来自黑暗深处的动静。"), 112, 948, 1180, 80,
                         22, FLinearColor(.57f, .64f, .62f, 1));
        Tip->SetAutoWrapText(true);
        auto *Spinner =
            Tree->ConstructWidget<UCircularThrobber>(UCircularThrobber::StaticClass(), TEXT("LoadingSpinner"));
        Spinner->SetNumberOfPieces(8);
        Spinner->SetPeriod(1.4f);
        Spinner->SetRadius(14);
        Place(Spinner, 1545, 945, 40, 40);
        Text(TEXT("LoadingLabel"), TEXT("正在载入"), 1600, 943, 230, 50, 22, Accent);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Results;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection, &Results);
        if (Results.NumErrors)
            return 3;
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(
                Package, BP, *FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension()),
                Args))
            return 4;
    }
    FString TipOverride;
    if (FParse::Value(*Params, TEXT("Tip="), TipOverride) && !Verify)
    {
        auto *Tip = Cast<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("TipText")));
        if (!Tip)
            return 8;
        Tip->SetText(FText::FromString(TipOverride));
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(
                BP->GetOutermost(), BP,
                *FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension()), Args))
            return 9;
    }
    if (!BP->GeneratedClass || !BP->WidgetTree || !BP->WidgetTree->RootWidget)
        return 5;
    UE_LOG(LogTemp, Display, TEXT("Loading screen verified: %s (existing layouts preserved)"), Path);
    FString Output;
    if (FParse::Value(*Params, TEXT("Preview="), Output))
    {
        if (!FSlateApplication::IsInitialized())
            FSlateApplication::InitializeAsStandaloneApplication(
                FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer"))
                    .CreateSlateRHIRenderer());
        auto *Widget = NewObject<UUserWidget>(GetTransientPackage(), BP->GeneratedClass);
        Widget->Initialize();
        FWidgetRenderer Renderer(true);
        const auto Slate = Widget->TakeWidget();
        FString PreviewLevel;
        if (FParse::Value(*Params, TEXT("Level="), PreviewLevel))
            if (auto *Info =
                    LoadObject<UVTGLevelLoadingInfo>(nullptr, TEXT("/Game/Widget/Loading/DA_LevelLoadingInfo")))
                Info->ApplyToWidget(Widget, PreviewLevel);
        auto *RT = Renderer.DrawWidget(Slate, FVector2D(1280, 720));
        FlushRenderingCommands();
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags ReadFlags;
        ReadFlags.SetLinearToGamma(false);
        if (!RT || !RT->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags))
            return 6;
        TArray64<uint8> PNG;
        FImageUtils::PNGCompressImageArray(1280, 720, Pixels, PNG);
        if (!FFileHelper::SaveArrayToFile(PNG, *Output))
            return 7;
    }
    return 0;
}
