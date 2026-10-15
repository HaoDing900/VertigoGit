#include "VTGHintIntegrationCommandlet.h"
#include "UI/VTGHintLayer.h"
#include "VTGManualLocalization.h"
#include "VTGHintLocalizationDetails.h"
#include "VTGCinematicRegressionFixture.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "PropertyHandle.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"

namespace HintIntegration
{
TArray<TSharedPtr<ISinglePropertyView>> PropertyViews;
void Compile(UBlueprint* BP){FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog L;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&L);checkf(!L.NumErrors,TEXT("Compile failed %s"),*BP->GetName());}
void Backup(UObject* O){const auto Source=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());const auto Target=FPaths::ProjectDir()/TEXT("Scripts/HintSkipIntegration/Backup")/(O->GetName()+TEXT(".uasset"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target),true);if(!IFileManager::Get().FileExists(*Target))check(IFileManager::Get().Copy(*Target,*Source)==COPY_OK);}
void Save(UObject* O){O->GetOutermost()->FullyLoad();ResetLoaders(O->GetOutermost());O->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;check(UPackage::SavePackage(O->GetOutermost(),O,*FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),A));}
FVector2D TemplateKeySize()
{
    // WB_ControlsHintKeyImage uses 45 * the row's Size_PC scale.
    FVector2D Result(45,45);auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/InventorySystemX/InventorySystemX/Data/DT_KeyHints"));if(!Table)return Result;
    for(const auto& Row:Table->GetRowMap())if(Row.Key.ToString().Contains(TEXT("Mail")))
        for(TFieldIterator<FStructProperty> It(Table->GetRowStruct());It;++It)if(It->GetName().StartsWith(TEXT("Size_PC")) && It->Struct==TBaseStructure<FVector2D>::Get())Result=*It->ContainerPtrToValuePtr<FVector2D>(Row.Value)*45.;
    UE_LOG(LogTemp,Display,TEXT("HINT KEY TEMPLATE: %s"),*Result.ToString());return Result;
}
UTextBlock* Text(UWidgetTree* Tree,FName Name,const FText& Value,UTextBlock* Reference)
{
    auto* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);T->SetText(Value);T->SetFont(Reference->GetFont());T->SetColorAndOpacity(Reference->GetColorAndOpacity());T->SetShadowColorAndOpacity(Reference->GetShadowColorAndOpacity());T->SetShadowOffset(Reference->GetShadowOffset());return T;
}
void Apply(UWidgetBlueprint* BP)
{
    Backup(BP);BP->ParentClass=UVTGHintLayer::StaticClass();Compile(BP);UWidgetTree* Tree=BP->WidgetTree;auto* Root=CastChecked<UOverlay>(Tree->RootWidget);
    if(!Tree->FindWidget(TEXT("SkipHintPanel")))
    {
        auto* Controls=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/Inventory/ControlsHints/WB_ControlsHintBlock"));auto* Reference=CastChecked<UTextBlock>(Controls->WidgetTree->FindWidget(TEXT("Action_Text")));auto* CDO=CastChecked<UVTGHintLayer>(BP->GeneratedClass->GetDefaultObject());
        auto* Panel=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("SkipHintPanel"));Panel->SetVisibility(ESlateVisibility::Collapsed);auto* Position=Root->AddChildToOverlay(Panel);Position->SetHorizontalAlignment(HAlign_Right);Position->SetVerticalAlignment(VAlign_Bottom);Position->SetPadding(FMargin(0,0,0,40));
        auto* Row=Tree->ConstructWidget<UHorizontalBox>();Panel->AddChildToVerticalBox(Row);
        auto* Hold=Text(Tree,TEXT("SkipHoldTextBlock"),CDO->SkipHoldText,Reference);auto* HoldSlot=Row->AddChildToHorizontalBox(Hold);HoldSlot->SetVerticalAlignment(VAlign_Center);
        auto* Key=Tree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("SkipKeyImage"));Key->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/Textures/Controls/PC/T_Space_Key_Dark")));auto Brush=Key->GetBrush();Brush.ImageSize=TemplateKeySize();Key->SetBrush(Brush);auto* Box=Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("SkipKeySize"));Box->SetWidthOverride(Brush.ImageSize.X);Box->SetHeightOverride(Brush.ImageSize.Y);Box->SetContent(Key);auto* KeySlot=Row->AddChildToHorizontalBox(Box);KeySlot->SetVerticalAlignment(VAlign_Center);KeySlot->SetPadding(FMargin(5.465879f,0));
        auto* Action=Text(Tree,TEXT("SkipActionTextBlock"),CDO->SkipActionText,Reference);Row->AddChildToHorizontalBox(Action)->SetVerticalAlignment(VAlign_Center);
        auto* Bar=Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),TEXT("SkipHoldProgress"));Bar->SetPercent(0);Bar->SetFillColorAndOpacity(FLinearColor(.2,.85,.9,1));auto* Height=Tree->ConstructWidget<USizeBox>();Height->SetHeightOverride(3);Height->SetContent(Bar);Panel->AddChildToVerticalBox(Height);
    }
    Compile(BP);Save(BP);
}
TSharedPtr<IPropertyHandle> Property(UObject* Object,FName Name)
{
    FSinglePropertyParams Params;auto View=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateSingleProperty(Object,Name,Params);PropertyViews.Add(View);return View->GetPropertyHandle();
}
void Translate(UWidgetBlueprint* BP)
{
    FString Error;checkf(VTGManualLocalization::GatherAsset(BP,Error),TEXT("%s"),*Error);auto* CDO=CastChecked<UVTGHintLayer>(BP->GeneratedClass->GetDefaultObject());
    for(const auto& Pair:{TPair<FName,FString>(TEXT("SkipHoldText"),TEXT("长按")),TPair<FName,FString>(TEXT("SkipActionText"),TEXT("跳过"))})
    {
        auto Handle=Property(CDO,Pair.Key);check(VTGHintLocalizationDetails::IsSafeText(Handle));FString Origin,Draft;
        checkf(VTGManualLocalization::GetDialogueDraft(Handle,TEXT("zh-Hans"),Origin,Draft,Error),TEXT("%s"),*Error);
        checkf(VTGManualLocalization::SaveDialogueDraft(Handle,TEXT("zh-Hans"),Origin,VTGManualLocalization::EncodeDraft(Origin,Pair.Value),Error),TEXT("%s"),*Error);
    }
    checkf(VTGManualLocalization::Compile(Error),TEXT("%s"),*Error);
}
void Verify(UWidgetBlueprint* BP)
{
    check(BP->ParentClass==UVTGHintLayer::StaticClass());auto* Root=CastChecked<UOverlay>(BP->WidgetTree->RootWidget);check(Root->GetChildrenCount()==3);auto* Mail=BP->WidgetTree->FindWidget(TEXT("WB_ControlsHints"));check(Mail && CastChecked<UOverlaySlot>(Mail->Slot)->GetVerticalAlignment()==VAlign_Center);
    auto* Panel=BP->WidgetTree->FindWidget(TEXT("SkipHintPanel"));check(Panel && Panel->GetVisibility()==ESlateVisibility::Collapsed && CastChecked<UOverlaySlot>(Panel->Slot)->GetVerticalAlignment()==VAlign_Bottom);
    auto* Layer=NewObject<UVTGHintLayer>(GetTransientPackage(),BP->GeneratedClass);Layer->Initialize();auto* MailText=CastChecked<UTextBlock>(Layer->GetWidgetFromName(TEXT("HintText")));MailText->SetText(FText::FromString(TEXT("MAIL SENTINEL")));auto* Hint=Layer->GetWidgetFromName(TEXT("WB_ControlsHints"));Hint->SetVisibility(ESlateVisibility::Visible);
    FSlateBrush Brush;Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/Textures/Controls/PC/T_Space_Key_Dark")));Layer->UpdateSkipHint(true,.5,Brush);check(Layer->GetWidgetFromName(TEXT("SkipHintPanel"))->GetVisibility()==ESlateVisibility::HitTestInvisible && MailText->GetText().ToString()==TEXT("MAIL SENTINEL") && Hint->GetVisibility()==ESlateVisibility::Visible);Layer->UpdateSkipHint(false,0,Brush);check(MailText->GetText().ToString()==TEXT("MAIL SENTINEL") && Hint->GetVisibility()==ESlateVisibility::Visible);
    FTextLocalizationResource Resource;check(Resource.LoadFromFile(FPaths::ProjectContentDir()/TEXT("Localization/Vertigo/zh-Hans/Vertigo.locres"),0));FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);Layer->UpdateSkipHint(true,.5,Brush);check(CastChecked<UTextBlock>(Layer->GetWidgetFromName(TEXT("SkipHoldTextBlock")))->GetText().ToString()==TEXT("长按"));check(CastChecked<UTextBlock>(Layer->GetWidgetFromName(TEXT("SkipActionTextBlock")))->GetText().ToString()==TEXT("跳过"));
    auto* Block=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/Inventory/ControlsHints/WB_ControlsHintBlock"));auto* TemplateText=CastChecked<UTextBlock>(Block->WidgetTree->FindWidget(TEXT("Action_Text")));auto* SkipText=CastChecked<UTextBlock>(Layer->GetWidgetFromName(TEXT("SkipHoldTextBlock")));
    check(SkipText->GetFont()==TemplateText->GetFont());check(CastChecked<USizeBox>(Layer->GetWidgetFromName(TEXT("SkipKeySize")))->GetWidthOverride()==TemplateKeySize().X);
    auto Handle=Property(Layer,TEXT("SkipHoldText"));FString Origin,Draft,Error;checkf(VTGManualLocalization::GetDialogueDraft(Handle,TEXT("en-US"),Origin,Draft,Error),TEXT("%s"),*Error);
    {
        TGuardValue<bool> AllowScript(GAllowActorScriptExecutionInEditor,true);
        auto* HideMail=Layer->FindFunction(TEXT("Hide Hint"));check(HideMail && HideMail->ParmsSize==0);Layer->ProcessEvent(HideMail,nullptr);
        check(Hint->GetVisibility()==ESlateVisibility::Collapsed && MailText->GetText().IsEmpty());
        check(Layer->GetWidgetFromName(TEXT("SkipHintPanel"))->GetVisibility()==ESlateVisibility::HitTestInvisible);
    }
    UE_LOG(LogTemp,Display,TEXT("HINT INTEGRATION PASS: preserved Mail placement/state; independent bottom-right Skip; matching template font/key size; inline language fields; compiled Chinese translation."));
}
void Preview(UWidgetBlueprint* BP,const FString& Output)
{
    auto* Layer=NewObject<UVTGHintLayer>(GetTransientPackage(),BP->GeneratedClass);Layer->Initialize();FSlateBrush Brush;Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Inventory/InventoryXBased/UMG/Textures/Controls/PC/T_Space_Key_Dark")));Layer->UpdateSkipHint(true,.5,Brush);CastChecked<UTextBlock>(Layer->GetWidgetFromName(TEXT("HintText")))->SetText(FText::FromString(TEXT("Mail")));
    auto Root=SNew(SOverlay)+SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.025,.035,.045,1))]+SOverlay::Slot()[Layer->TakeWidget()];FWidgetRenderer Renderer(true);auto* Target=Renderer.DrawWidget(Root,FVector2D(1920,1080));FlushRenderingCommands();TArray<FColor> Pixels;check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1920,1080,Pixels,PNG);check(FFileHelper::SaveArrayToFile(PNG,*Output));
}
}
UVTGHintIntegrationCommandlet::UVTGHintIntegrationCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UVTGHintIntegrationCommandlet::Main(const FString& Params)
{
    if(!FSlateApplication::IsInitialized())FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    auto* BP=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Blueprints/Widget/WBP_HintLayer"));check(BP);if(!Params.Contains(TEXT("VerifyOnly"))){HintIntegration::Apply(BP);HintIntegration::Translate(BP);}HintIntegration::Verify(BP);if(!VTGRunSequenceSkipChecks())return 1;FString Output;if(FParse::Value(*Params,TEXT("Preview="),Output))HintIntegration::Preview(BP,Output);HintIntegration::PropertyViews.Reset();return 0;
}
