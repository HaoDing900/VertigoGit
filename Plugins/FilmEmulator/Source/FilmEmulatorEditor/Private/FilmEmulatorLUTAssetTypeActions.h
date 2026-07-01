// Copyright 2026 TOXIC STOCK All rights reserved.

#pragma once

#include "AssetTypeActions_Base.h"

class FFilmEmulatorLUTAssetTypeActions : public FAssetTypeActions_Base
{
public:
    virtual FText GetName() const override;
    virtual FColor GetTypeColor() const override;
    virtual UClass* GetSupportedClass() const override;
    virtual uint32 GetCategories() override;
};
