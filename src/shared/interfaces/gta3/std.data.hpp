/* 
 * Copyright (C) 2015  LINK/2012 <dma_2012@hotmail.com>
 * Licensed under the MIT License, see LICENSE at top level directory.
 * 
 */
#pragma once

#include <modloader/modloader.hpp>
#include <string>
#include <vector>

static constexpr const char* kStdDataAtxdSharedName = "StdDataATXDList";

struct StdDataAtxdSharedData
{
    uint32_t revision = 0;
    std::vector<std::string> txd_names;
};

inline StdDataAtxdSharedData* GetStdDataAtxdSharedData()
{
    if(modloader_shdata_t* data = modloader::plugin_ptr->loader->FindSharedData(kStdDataAtxdSharedName))
    {
        if(data->type == MODLOADER_SHDATA_POINTER)
            return static_cast<StdDataAtxdSharedData*>(data->p);
    }
    return nullptr;
}
