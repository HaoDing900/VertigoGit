#pragma once
#include "CoreMinimal.h"
class UK2Node_CallFunction;
namespace VTGMailRowJump
{
    void Startup();
    void Shutdown();
    bool Resolve(const UK2Node_CallFunction* Node, FName& RowName);
}
