/*
 * Copyright (C) 2024  LINK/2012 <dma_2012@hotmail.com>
 * Licensed under the MIT License, see LICENSE at top level directory.
 *
 */
#include <stdinc.hpp>
#include "../data_traits.hpp"
#include <interfaces/gta3/std.additionaltxd.hpp>
#include <unordered_set>

using namespace modloader;

namespace {

struct additionaltxd_traits : public data_traits
{
    struct dtraits : modloader::dtraits::OpenFile
    {
        static const char* what() { return "additional txd"; }
    };
};

struct additionaltxd_store
{
    using traits_type = additionaltxd_traits;
    std::vector<std::string> entries;

    template<class Archive>
    void serialize(Archive& archive)
    { archive(entries); }
};

REGISTER_RTTI_FOR_ANY(additionaltxd_store);

static modloader_shdata_t* g_additional_txd_shdata = nullptr;
static AdditionalTxdList g_additional_txd_list;

static const AdditionalTxdList* GetAdditionalTxdList()
{
    g_additional_txd_list.clear();
    std::unordered_set<std::string> seen;

    auto readme_data = modloader::plugin_ptr->cast<DataPlugin>().QueryReadmeData<additionaltxd_store>();
    for (const auto& entry : readme_data)
    {
        const auto& store = entry.second.second.get();
        for (const auto& name : store.entries)
        {
            if (seen.emplace(name).second)
                g_additional_txd_list.emplace_back(name);
        }
    }

    return &g_additional_txd_list;
}

static void CreateAdditionalTxdSharedData()
{
    if (g_additional_txd_shdata)
        return;

    g_additional_txd_shdata = modloader::plugin_ptr->loader->CreateSharedData(AdditionalTxdListSharedName());
    if (g_additional_txd_shdata)
    {
        g_additional_txd_shdata->type = MODLOADER_SHDATA_FUNCTION;
        g_additional_txd_shdata->f = reinterpret_cast<void*>(&GetAdditionalTxdList);
    }
}

static std::string ExtractTxdName(const std::string& path)
{
    std::string normalized = modloader::NormalizePath(path);
    std::string base = modloader::GetPathComponentBack(normalized);
    auto dot = base.rfind('.');
    if (dot != std::string::npos)
        base = base.substr(0, dot);
    return base;
}

} // namespace

void ShutdownAdditionalTxdSharedData()
{
    if (g_additional_txd_shdata)
    {
        modloader::plugin_ptr->loader->DeleteSharedData(g_additional_txd_shdata);
        g_additional_txd_shdata = nullptr;
    }
}

static auto xinit = initializer([](DataPlugin* plugin_ptr)
{
    CreateAdditionalTxdSharedData();

    plugin_ptr->AddReader<additionaltxd_store>([](const std::string& line) -> maybe_readable<additionaltxd_store>
    {
        if (!modloader::starts_with(line.c_str(), "ATXD", false))
            return nothing;

        std::string tag;
        std::string rest;
        std::stringstream stream(line);
        if (!(stream >> tag))
            return nothing;

        std::getline(stream, rest);
        modloader::trim(rest);
        if (rest.empty())
            return nothing;

        std::string name = ExtractTxdName(rest);
        if (name.empty())
            return nothing;

        additionaltxd_store store;
        store.entries.emplace_back(std::move(name));
        return store;
    });
});
