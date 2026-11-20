#include "UI/VTGMapLocationNode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace MapPresentationProbe
{
void Click(UWidget* Target)
{
 const FVector2D P=Target->GetCachedGeometry().LocalToAbsolute(Target->GetCachedGeometry().GetLocalSize()*.5);
 const TSet<FKey> Pressed{EKeys::LeftMouseButton},Released;
 auto& App=FSlateApplication::Get();auto Window=App.FindWidgetWindow(Target->GetCachedWidget().ToSharedRef());check(Window.IsValid());
 App.SetCursorPos(P);const FPointerEvent Down(FSlateApplication::CursorPointerIndex,P,P,Pressed,EKeys::LeftMouseButton,0,FModifierKeysState());App.ProcessMouseMoveEvent(Down);App.ProcessMouseButtonDownEvent(Window->GetNativeWindow(),Down);
 const FPointerEvent Up(FSlateApplication::CursorPointerIndex,P,P,Released,EKeys::LeftMouseButton,0,FModifierKeysState());App.ProcessMouseButtonUpEvent(Up);
}
void Run(const TArray<FString>& Args)
{
 if(Args.IsEmpty())return;
 if(Args[0]==TEXT("shot")){check(Args.Num()>1);FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("Scripts/MapPresentation")/(Args[1]+TEXT(".png")),true,false);return;}
 for(TObjectIterator<UUserWidget> It;It;++It)
 {
  auto* Root=*It;if(!Root->GetWorld() || Root->GetWorld()->WorldType!=EWorldType::PIE || Root->GetClass()->GetName()!=TEXT("WB_TabSwitcher_C") || !Root->IsVisible())continue;
  auto* Switcher=CastChecked<UWidgetSwitcher>(Root->GetWidgetFromName(TEXT("WidgetSwitcher_132")));auto* Page=CastChecked<UUserWidget>(Switcher->GetActiveWidget());auto* Map=CastChecked<UUserWidget>(Page->GetWidgetFromName(TEXT("WBP_Map_1")));
  const TCHAR* Names[]={TEXT("WBP_MapNode"),TEXT("WBP_MapNode_1"),TEXT("WBP_MapNode_2"),TEXT("WBP_MapNode_3")};
  const int Index=Args.Num()>1?FCString::Atoi(*Args[1]):-1;TArray<UVTGMapLocationNode*> Nodes;for(const TCHAR* Name:Names)Nodes.Add(CastChecked<UVTGMapLocationNode>(Map->GetWidgetFromName(Name)));
  if(Args[0]==TEXT("click") || Args[0]==TEXT("name")){check(Nodes.IsValidIndex(Index));Click(Nodes[Index]->GetWidgetFromName(Args[0]==TEXT("click")?TEXT("Btn_NodeInteractor"):TEXT("LocationNameButton")));return;}
  int Descriptions=0;
  for(int I=0;I<Nodes.Num();++I)
  {
   auto* N=Nodes[I];const bool Selected=FindFProperty<FBoolProperty>(N->GetClass(),TEXT("IsSelected?"))->GetPropertyValue_InContainer(N);auto* Badge=N->GetWidgetFromName(TEXT("LocationNameButton"));auto* Panel=FindObject<UBorder>(N->WidgetTree,TEXT("LocationDescriptionPanel"));
   const bool Open=Panel && Panel->IsVisible();Descriptions+=Open;
   if(Open)
   {
    const auto& NodeGeometry=N->GetCachedGeometry();const auto& PanelGeometry=Panel->GetCachedGeometry();
    const bool ExpectedRight=I==3; // North is left of the node-center threshold; Bureau opens left.
    if(ExpectedRight){check(PanelGeometry.GetAbsolutePosition().X>=NodeGeometry.LocalToAbsolute(FVector2D(NodeGeometry.GetLocalSize().X,0)).X-2);}
    else {check(PanelGeometry.LocalToAbsolute(FVector2D(PanelGeometry.GetLocalSize().X,0)).X<=NodeGeometry.GetAbsolutePosition().X+2);}
    UE_LOG(LogTemp,Display,TEXT("MAP_PANEL_SIDE node=%s side=%s"),*N->GetName(),ExpectedRight?TEXT("right"):TEXT("left"));
   }
   check(Selected==(I==Index));check(Badge->IsVisible()==Selected);check(!N->GetWidgetFromName(TEXT("Txt_NodeName"))->IsVisible());check(!N->GetWidgetFromName(TEXT("VB_SubAreaList"))->IsVisible());check(!CastChecked<UButton>(N->GetWidgetFromName(TEXT("Btn_NodeInteractor")))->GetToolTip());
   UE_LOG(LogTemp,Display,TEXT("MAP_PIE node=%s selected=%d badge=%d description=%d"),*N->GetName(),Selected,Badge->IsVisible(),Open);
  }
  const bool ExpectedDescription=Args.Num()>2 && Args[2]==TEXT("description");check(Descriptions==(ExpectedDescription?1:0));
  UE_LOG(LogTemp,Display,TEXT("MAP_PIE_PASS selected=%d descriptions=%d"),Index,Descriptions);return;
 }
 checkNoEntry();
}
static FAutoConsoleCommand Command(TEXT("VTG.MapPresentationProbe"),TEXT("Explicit PIE-only map selection/pointer regression and screenshots."),FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}
