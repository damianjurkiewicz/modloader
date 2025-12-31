/*
 * Copyright (C) 2024  LINK/2012 <dma_2012@hotmail.com>
 * Licensed under the MIT License, see LICENSE at top level directory.
 *
 */
#pragma once
#include <modloader/modloader.hpp>
#include <string>
#include <vector>

// Interface between std.data and std.additionaltxd for readme ATXD entries.

inline const char* AdditionalTxdListSharedName()
{
    return "AdditionalTxdListGet";
}

using AdditionalTxdList = std::vector<std::string>;
using AdditionalTxdListGetter = const AdditionalTxdList* (*)();

inline const AdditionalTxdList* AdditionalTxdListGet()
{
    if(modloader_shdata_t* data = modloader::plugin_ptr->loader->FindSharedData(AdditionalTxdListSharedName()))
    {
        if(data->type == MODLOADER_SHDATA_FUNCTION)
        {
            auto getter = reinterpret_cast<AdditionalTxdListGetter>(data->f);
            return getter();
        }
    }
    return nullptr;
}
