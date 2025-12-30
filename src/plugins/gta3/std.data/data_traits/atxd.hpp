/*
 * Copyright (C) 2015  LINK/2012 <dma_2012@hotmail.com>
 * Licensed under the MIT License, see LICENSE at top level directory.
 * 
 */
#pragma once
#include <stdinc.hpp>
#include <modloader/util/container.hpp>
#include <modloader/util/path.hpp>
#include "../data_traits.hpp"

struct atxd_traits : public data_traits
{
    static const bool has_sections      = false;
    static const bool per_line_section  = false;

    struct dtraits : modloader::dtraits::OpenFile
    {
        static const char* what()       { return "additional txd"; }
        static const char* datafile()   { return "atxd"; }
    };

    using key_type   = std::size_t;
    using value_type = data_slice<modelname>;

    key_type key_from_value(const value_type& value)
    {
        return hash_model(get<0>(value));
    }

    template<class StoreType>
    static bool setbyline(StoreType&, value_type& data, const gta3::section_info*, const std::string& line)
    {
        auto pos = line.find_first_of(" \t");
        if(pos == std::string::npos)
            return false;

        std::string path = line.substr(pos + 1);
        modloader::trim(path);
        if(path.empty())
            return false;

        std::string name = modloader::GetPathComponentBack(path);
        auto dot = name.find_last_of('.');
        if(dot != std::string::npos)
            name.erase(dot);

        modloader::trim(name);
        if(name.empty())
            return false;

        modloader::tolower(name);
        data = value_type(make_insen_string(name));
        return true;
    }
};

using atxd_store = gta3::data_store<atxd_traits, std::map<
                        atxd_traits::key_type, atxd_traits::value_type
                        >>;
