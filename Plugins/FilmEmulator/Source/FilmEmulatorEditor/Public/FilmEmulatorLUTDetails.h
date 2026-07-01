// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"

class UFilmEmulatorLUT;
class UTexture2D;
struct FSlateBrush;

class FFilmEmulatorLUTDetails : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();

    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    TWeakObjectPtr<UFilmEmulatorLUT> LutAsset;
    TStrongObjectPtr<UTexture2D> PreviewTexture;
    TSharedPtr<FSlateBrush> PreviewBrush;

    void BuildPreview(IDetailLayoutBuilder& DetailBuilder);
    UTexture2D* CreatePreviewTexture(UFilmEmulatorLUT* Lut) const;
};

