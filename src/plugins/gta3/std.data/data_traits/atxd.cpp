/*
 * Copyright (C) 2015  LINK/2012 <dma_2012@hotmail.com>
 * Licensed under the MIT License, see LICENSE at top level directory.
 * 
 */
#include <stdinc.hpp>
#include <set>
#include "atxd.hpp"
using namespace modloader;

REGISTER_RTTI_FOR_ANY(atxd_store);

void DataPlugin::RefreshAtxdSharedData()
{
    if(!this->sh_atxd_data)
        return;

    std::set<std::string> unique_names;
    auto readme_data = this->QueryReadmeData<atxd_store>();

    for(const auto& entry : readme_data)
    {
        const auto& store = entry.second.second.get();
        for(const auto& item : store.container())
        {
            const auto& name = get(get<0>(item.second));
            std::string normalized = name;
            modloader::tolower(normalized);
            if(!normalized.empty())
                unique_names.insert(std::move(normalized));
        }
    }

    std::vector<std::string> next;
    next.reserve(unique_names.size());
    for(const auto& name : unique_names)
        next.push_back(name);

    if(next != this->atxd_shared.txd_names)
    {
        this->atxd_shared.txd_names = std::move(next);
        ++this->atxd_shared.revision;
    }
}

// Additional TXD reader
static auto xinit = initializer([](DataPlugin* plugin_ptr)
{
    plugin_ptr->AddReader<atxd_store>([](const std::string& line) -> maybe_readable<atxd_store>
    {
        static auto regex = make_regex(R"___(^ATXD\s+\S+\.txd\s*$)___",
            sregex::ECMAScript | sregex::optimize | sregex::icase);

        if(regex_match(line, regex))
        {
            atxd_store store;
            if(store.insert(nullptr, line))
                return store;
        }
        return nothing;
    });
});
