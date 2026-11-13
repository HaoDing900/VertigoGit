#include "Blueprint/UserWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Input/Events.h"
#include "UObject/UObjectIterator.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Editor.h"
#include "GameFramework/PlayerController.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/GameViewportClient.h"
#include "NarrativeUIInputComponent.h"
#include "Layout/WidgetPath.h"
#include "Blueprint/WidgetTree.h"

// Diagnostic only: invoked explicitly in an isolated editor PIE session.
namespace VTGTabProbe
{
void Dump(UWorld* World, const TCHAR* Stage)
{
    if(auto* PC=World->GetFirstPlayerController())
    {
        auto* Input=UNarrativeUIInputComponent::Find(PC);
        UE_LOG(LogTemp,Display,TEXT("TERMINAL_PIE %s mode=%s cursor=%d ignored=%d owner=%s"),Stage,*PC->GetCurrentInputModeDebugString(),PC->ShouldShowMouseCursor(),World->GetGameViewport()?World->GetGameViewport()->IgnoreInput():false,*GetNameSafe(Input?Input->GetActiveOwner():nullptr));
        if(auto* Pawn=PC->GetPawn())for(auto* Component:Pawn->GetComponents())if(auto* P=FindFProperty<FObjectPropertyBase>(Component->GetClass(),TEXT("TabSwitchWidget")))UE_LOG(LogTemp,Display,TEXT("TERMINAL_PIE hud=%s tab=%s"),*Component->GetPathName(),*GetPathNameSafe(P->GetObjectPropertyValue_InContainer(Component)));
    }
    for (TObjectIterator<UUserWidget> It; It; ++It)
    {
        auto* Widget = *It;
        if (Widget->GetWorld() != World) continue;
        if(Widget->GetClass()->GetName()==TEXT("WB_TabSwitcher_C"))UE_LOG(LogTemp,Display,TEXT("TERMINAL_PIE root=%s visible=%d focus=%d childfocus=%d player=%s"),*Widget->GetPathName(),Widget->IsVisible(),Widget->HasKeyboardFocus(),Widget->HasFocusedDescendants(),*GetNameSafe(Widget->GetOwningPlayer()));
        if (Widget->HasKeyboardFocus())
            UE_LOG(LogTemp, Display, TEXT("TABPROBE_NATIVE %s focus=%s class=%s"), Stage, *Widget->GetName(), *Widget->GetClass()->GetName());
        if (Widget->GetClass()->GetName() != TEXT("WB_TabSwitcherMenu_C")) continue;
        int32 Active = -999;
        if (auto* Function = Widget->FindFunction(TEXT("GetActiveTabIndex")))
        {
            FStructOnScope Parameters(Function);
            Widget->ProcessEvent(Function, Parameters.GetStructMemory());
            for (TFieldIterator<FProperty> P(Function); P; ++P)
                if (P->HasAnyPropertyFlags(CPF_OutParm))
                    if (auto* Number = CastField<FIntProperty>(*P))
                        Active = Number->GetPropertyValue_InContainer(Parameters.GetStructMemory());
        }
        int32 Buttons = -1;
        if (auto* Array = FindFProperty<FArrayProperty>(Widget->GetClass(), TEXT("TabButtonsWidgets")))
            Buttons = FScriptArrayHelper(Array, Array->ContainerPtrToValuePtr<void>(Widget)).Num();
        UE_LOG(LogTemp, Display, TEXT("TABPROBE_NATIVE %s menu=%s visible=%d buttons=%d active=%d"), Stage, *Widget->GetName(), Widget->IsVisible(), Buttons, Active);
    }
}
void Run(const TArray<FString>& Args)
{
    UWorld* World = nullptr;
    if (GEngine)
        for (auto& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE) World = Context.World();
    if (!World || !FSlateApplication::IsInitialized()) return;
    Dump(World, TEXT("before"));
    TArray<FKey> Keys = Args.Num() ? TArray<FKey>{FKey(*Args[0])} : TArray<FKey>{EKeys::E, EKeys::Q};
    for (FKey Key : Keys)
    {
        const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
        const bool Handled = FSlateApplication::Get().ProcessKeyDownEvent(Event);
        UE_LOG(LogTemp, Display, TEXT("TABPROBE_NATIVE key=%s handled=%d"), *Key.ToString(), Handled);
        FSlateApplication::Get().ProcessKeyUpEvent(Event);
        Dump(World, *Key.ToString());
    }
}
static FAutoConsoleCommand Command(TEXT("VTG.TabSwitcherProbe"), TEXT("Trace Q/E Slate routing and tab state in PIE without saving assets."), FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
static FAutoConsoleCommand ClickCommand(TEXT("VTG.TerminalClickQuest"),TEXT("Send a Slate mouse click to the visible terminal's first tab."),FConsoleCommandDelegate::CreateLambda([]
{
 for(TObjectIterator<UUserWidget> It;It;++It)
 {
  auto* Root=*It;if(!Root->GetWorld() || Root->GetWorld()->WorldType!=EWorldType::PIE || Root->GetClass()->GetName()!=TEXT("WB_TabSwitcher_C") || !Root->IsVisible())continue;
  auto* Menu=Cast<UUserWidget>(Root->GetWidgetFromName(TEXT("WB_TabSwitcher")));check(Menu);
  auto* Property=FindFProperty<FArrayProperty>(Menu->GetClass(),TEXT("TabButtonsWidgets"));check(Property);FScriptArrayHelper Buttons(Property,Property->ContainerPtrToValuePtr<void>(Menu));check(Buttons.Num()==4);
  auto* Button=CastChecked<UUserWidget>(CastFieldChecked<FObjectPropertyBase>(Property->Inner)->GetObjectPropertyValue(Buttons.GetRawPtr(0)));
  const FVector2D Position=Button->GetCachedGeometry().LocalToAbsolute(Button->GetCachedGeometry().GetLocalSize()*.5);
  const TSet<FKey> Pressed{EKeys::LeftMouseButton},Released;
  const FPointerEvent Down(FSlateApplication::CursorPointerIndex,Position,Position,Pressed,EKeys::LeftMouseButton,0,FModifierKeysState());
  auto Window=FSlateApplication::Get().FindWidgetWindow(Button->GetCachedWidget().ToSharedRef());check(Window.IsValid());
  FSlateApplication::Get().SetCursorPos(Position);FSlateApplication::Get().ProcessMouseMoveEvent(Down);
  FSlateApplication::Get().ProcessMouseButtonDownEvent(Window->GetNativeWindow(),Down);
  const FPointerEvent Up(FSlateApplication::CursorPointerIndex,Position,Position,Released,EKeys::LeftMouseButton,0,FModifierKeysState());FSlateApplication::Get().ProcessMouseButtonUpEvent(Up);
  Dump(Root->GetWorld(),TEXT("mouse-click-quest"));
  return;
 }
}));
static FAutoConsoleCommand BeginCommand(TEXT("VTG.TabSwitcherStartPIE"), TEXT("Start an isolated, normal PIE session in the existing hidden editor viewport."), FConsoleCommandDelegate::CreateLambda([] { FStartPIECommand(false).Update(); }));
static FAutoConsoleCommand TabCommand(TEXT("VTG.TabSwitcherPressTab"), TEXT("Send Tab to the PIE player controller; pass release for key up."), FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
{
    if (GEngine) for (auto& Context : GEngine->GetWorldContexts())
        if (Context.WorldType == EWorldType::PIE)
            if (auto* PC = Context.World()->GetFirstPlayerController())
            {
                const bool Release = Args.Num() && Args[0] == TEXT("release");
                const bool Handled = PC->InputKey(EKeys::Tab, Release ? IE_Released : IE_Pressed, 1.f, false);
                UE_LOG(LogTemp, Display, TEXT("TABPROBE_NATIVE Tab release=%d handled=%d pc=%s pawn=%s"), Release, Handled, *PC->GetClass()->GetName(), PC->GetPawn() ? *PC->GetPawn()->GetClass()->GetName() : TEXT("None"));
            }
}));
}
