#include "UI/VTGMapLocationNode.h"
#include "VTGManualLocalization.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/OverlaySlot.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextLocalizationResource.h"
#include "VTGMapLocalizationDetails.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"

namespace MapLabelsRepair
{
const FString Base = TEXT("/Game/Inventory/InventoryXBased/UMG/TabSwitcher/InTabWidget/MapWBP/");
bool Backup(UBlueprint* BP)
{
    const FString File = FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), TEXT(".uasset"));
    const FString Target = FPaths::ProjectSavedDir() / TEXT("QA/MapLabelsBefore") / (BP->GetName()+TEXT(".uasset"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target), true);
    return IFileManager::Get().FileExists(*Target) || IFileManager::Get().Copy(*Target, *File) == COPY_OK;
}
int32 Run(bool VerifyOnly)
{
    auto* NodeBP = LoadObject<UWidgetBlueprint>(nullptr, *(Base+TEXT("WBP_MapNode")));
    if (!NodeBP) return 1;
    if (!VerifyOnly)
    {
        if (!Backup(NodeBP)) return 2;
        NodeBP->ParentClass = UVTGMapLocationNode::StaticClass();
        auto* Caption = CastChecked<UTextBlock>(NodeBP->WidgetTree->FindWidget(TEXT("Txt_NodeName")));
        // The authored eight-pixel wrap width split names into individual letters.
        FindFProperty<FFloatProperty>(Caption->GetClass(), TEXT("WrapTextAt"))->SetPropertyValue_InContainer(Caption, 0.f);
        Caption->SetAutoWrapText(true); Caption->SetJustification(ETextJustify::Center);
        auto Font = Caption->GetFont(); Font.Size = 14; Font.OutlineSettings.OutlineColor = FLinearColor::Black; Caption->SetFont(Font);
        if (auto* Slot = Cast<UOverlaySlot>(Caption->Slot))
        {
            Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Bottom);
            Slot->SetPadding(FMargin(8, 0, 8, 18));
        }
        // The name is now present inside the button in normal, hover and selected states.
        for (UEdGraphNode* Node : NodeBP->UbergraphPages[0]->Nodes)
            if (auto* Call = Cast<UK2Node_CallFunction>(Node))
                if (Call->FunctionReference.GetMemberName() == TEXT("SetVisibility"))
                    if (auto* Target = Call->FindPin(TEXT("self")); Target && Target->LinkedTo.Num() == 1)
                        if (auto* Getter = Cast<UK2Node_VariableGet>(Target->LinkedTo[0]->GetOwningNode()))
                            if (Getter->VariableReference.GetMemberName() == TEXT("Txt_NodeName"))
                                GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Call->FindPinChecked(TEXT("InVisibility")), TEXT("HitTestInvisible"));
        if (!AlleyFix::Save(NodeBP)) return 3;
    }
    if (!NodeBP->GeneratedClass->IsChildOf(UVTGMapLocationNode::StaticClass())) return 4;
    int32 Count = 0;
    auto* MapBP = LoadObject<UWidgetBlueprint>(nullptr, *(Base+TEXT("WBP_Map_1")));
    if (!MapBP) return 5;
    if (!VerifyOnly && !Backup(MapBP)) return 6;
    MapBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
    {
        auto* Location = Cast<UVTGMapLocationNode>(Widget);
        if (!Location) return;
        ++Count;
        if (VerifyOnly || !Location->LocationDisplayName.IsEmpty()) return;
        FString Source;
        if (Widget->GetName() == TEXT("WBP_MapNode")) Source = TEXT("Slickwater Docks");
        if (Widget->GetName() == TEXT("WBP_MapNode_1")) Source = TEXT("Bureau Module");
        if (Widget->GetName() == TEXT("WBP_MapNode_2")) Source = TEXT("Commerce Module");
        if (Widget->GetName() == TEXT("WBP_MapNode_3")) Source = TEXT("North Tunnel");
        if (!Source.IsEmpty())
            Location->LocationDisplayName = FText::ChangeKey(TEXT("VTGMapLocations"), TEXT("WBP_Map_1.")+Widget->GetName()+TEXT(".Name"), FText::FromString(Source));
    });
    if (Count != 4) return 7;
    if (!VerifyOnly && !AlleyFix::Save(MapBP)) return 8;
    // Exercise the actual Blueprint widgets with a transient world. No saves or translations are written by tests.
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MapLocationTest"), nullptr, true, ERHIFeatureLevel::Num, &Init);
    auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game); Context.SetCurrentWorld(World);
    auto* GI = NewObject<UGameInstance>(GEngine); World->SetGameInstance(GI); Context.OwningGameInstance = GI;
    GI->InitializeStandalone();
    // InitializeStandalone creates its own context; bind the fixture explicitly.
    if (auto* GIContext = GI->GetWorldContext()) GIContext->SetCurrentWorld(World);
    if (!FSlateApplication::IsInitialized())
        FSlateApplication::InitializeAsStandaloneApplication(FModuleManager::LoadModuleChecked<ISlateRHIRendererModule>(TEXT("SlateRHIRenderer")).CreateSlateRHIRenderer());
    auto* Map = CreateWidget<UUserWidget>(World, MapBP->GeneratedClass.Get()); check(Map);
    auto MapSlate = Map->TakeWidget();
    TArray<UVTGMapLocationNode*> Locations;
    Map->WidgetTree->ForEachWidget([&](UWidget* Widget) { if (auto* Location = Cast<UVTGMapLocationNode>(Widget)) Locations.Add(Location); });
    check(Locations.Num() == 4);
    TSet<FString> Names;
    for (auto* Location : Locations)
    {
        check(!Location->LocationDisplayName.IsEmpty());
        Names.Add(Location->LocationDisplayName.ToString());
        Location->LocationDescription = FText::ChangeKey(TEXT("VTGMapLocationTest"), Location->GetName(), FText::FromString(TEXT("A short location introduction. Longer text wraps inside the floating panel without covering the button.")));
        Location->RefreshLocationText();
        auto* Label = CastChecked<UTextBlock>(Location->GetWidgetFromName(TEXT("Txt_NodeName")));
        auto* Button = CastChecked<UButton>(Location->GetWidgetFromName(TEXT("Btn_NodeInteractor")));
        check(Label->GetText().EqualTo(Location->LocationDisplayName));
        check(Label->GetVisibility() == ESlateVisibility::HitTestInvisible);
        auto* Tooltip = CastChecked<UVTGMapLocationTooltip>(Button->GetToolTip());
        Tooltip->TakeWidget();
        auto* Body = CastChecked<UTextBlock>(Tooltip->GetWidgetFromName(TEXT("LocationDescription")));
        check(Body->GetText().EqualTo(Location->LocationDescription));
        check(Body->GetColorAndOpacity().GetSpecifiedColor() == FLinearColor::White);
        Button->OnHovered.Broadcast(); Button->OnUnhovered.Broadcast();
        check(Label->GetVisibility() == ESlateVisibility::HitTestInvisible);
        Button->OnClicked.Broadcast();
        check(Label->GetText().EqualTo(Location->LocationDisplayName));
    }
    check(Names.Num() == 4);
#if WITH_DEV_AUTOMATION_TESTS
    // The same inline manual-translation workflow used by dialogue/subtitles.
    VTGManualLocalization::SetTestStorageDirectory(FPaths::ProjectSavedDir()/TEXT("Automation/MapLocationLocalization")/FGuid::NewGuid().ToString());
    for (const FName Field : {FName(TEXT("LocationDisplayName")), FName(TEXT("LocationDescription"))})
    {
        FSinglePropertyParams Args;
        auto View = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateSingleProperty(Locations[0], Field, Args);
        check(View.IsValid()); auto Handle = View->GetPropertyHandle();
        check(VTGMapLocalizationDetails::IsSafeText(Handle));
        FString Source, Draft, Error;
        check(VTGManualLocalization::GetDialogueDraft(Handle, TEXT("zh-Hans"), Source, Draft, Error));
        check(VTGManualLocalization::SaveDialogueDraft(Handle, TEXT("zh-Hans"), Source, TEXT("手工地图译文测试"), Error));
        check(VTGManualLocalization::GetDialogueDraft(Handle, TEXT("zh-Hans"), Source, Draft, Error));
        check(Draft == TEXT("手工地图译文测试"));
    }
    VTGManualLocalization::SetTestStorageDirectory(FString());
#endif
    // Translation changes the same FText objects held by both the caption and tooltip.
    auto* First = Locations[0];
    FTextLocalizationResource Resource;
    auto Namespace = FTextInspector::GetNamespace(First->LocationDisplayName); auto Key = FTextInspector::GetKey(First->LocationDisplayName);
    check(Namespace.IsSet() && Key.IsSet());
    Resource.AddEntry(*Namespace, *Key, First->LocationDisplayName.ToString(), TEXT("地点名称本地化测试"), 0);
    const auto BodyNamespace = FTextInspector::GetNamespace(First->LocationDescription);
    const auto BodyKey = FTextInspector::GetKey(First->LocationDescription);
    check(BodyNamespace.IsSet() && BodyKey.IsSet());
    Resource.AddEntry(*BodyNamespace, *BodyKey, First->LocationDescription.ToString(), TEXT("这是一段地点介绍。更长的内容会在浮动文字框中自动换行。"), 0);
    FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
    auto* Label = CastChecked<UTextBlock>(First->GetWidgetFromName(TEXT("Txt_NodeName")));
    check(Label->GetText().ToString() == TEXT("地点名称本地化测试"));
    auto* Button = CastChecked<UButton>(First->GetWidgetFromName(TEXT("Btn_NodeInteractor")));
    auto* Tooltip = CastChecked<UVTGMapLocationTooltip>(Button->GetToolTip());
    check(CastChecked<UTextBlock>(Tooltip->GetWidgetFromName(TEXT("LocationTitle")))->GetText().ToString() == TEXT("地点名称本地化测试"));
    check(CastChecked<UTextBlock>(Tooltip->GetWidgetFromName(TEXT("LocationDescription")))->GetText().ToString() == TEXT("这是一段地点介绍。更长的内容会在浮动文字框中自动换行。"));
    FTextLocalizationResource Origin;
    const FString NameSource = *FTextInspector::GetSourceString(First->LocationDisplayName);
    const FString BodySource = *FTextInspector::GetSourceString(First->LocationDescription);
    Origin.AddEntry(*Namespace, *Key, NameSource, NameSource, 0);
    Origin.AddEntry(*BodyNamespace, *BodyKey, BodySource, BodySource, 0);
    FTextLocalizationManager::Get().UpdateFromLocalizationResource(Origin);
    for (auto* Location : Locations)
        if (auto* Reset = Location->FindFunction(TEXT("ForceReset"))) Location->ProcessEvent(Reset, nullptr);
    FString Preview;
    if (FParse::Value(FCommandLine::Get(), TEXT("PreviewFile="), Preview))
    {
        FWidgetRenderer Renderer(true);
        auto* Target = Renderer.DrawWidget(MapSlate, FVector2D(1280, 960)); FlushRenderingCommands(); check(Target);
        TArray<FColor> Pixels; check(Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
        TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(1280, 960, Pixels, PNG); check(FFileHelper::SaveArrayToFile(PNG, *Preview));
        auto* TipTarget = Renderer.DrawWidget(Tooltip->TakeWidget(), FVector2D(320, 240)); FlushRenderingCommands(); check(TipTarget);
        Pixels.Reset(); check(TipTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels));
        PNG.Reset(); FImageUtils::PNGCompressImageArray(320, 240, Pixels, PNG); check(FFileHelper::SaveArrayToFile(PNG, *(FPaths::GetPath(Preview)/TEXT("tooltip-preview.png"))));
    }
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    UE_LOG(LogTemp, Display, TEXT("MAP LOCATION PASS: four independent FText captions, editable introductions, black/white hover tooltip, existing interaction callbacks, live localized caption and tooltip."));
    return 0;
}
}
