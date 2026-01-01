/*
 * Copyright (C) 2013-2014  LINK/2012 <dma_2012@hotmail.com>
 * Licensed under the MIT License, see LICENSE at top level directory.
 *
 * std.additionaltxd -- Additional TXD dictionaries (fastloader*)
 *
 * Ported from plugin-sdk based implementation (src-additionaltxd.cpp).
 */

#include <stdinc.hpp>

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <cctype> // Potrzebne do tolower

using namespace modloader;

// --- Minimal RenderWare forward declarations ---
struct RwTexDictionary;
struct RwTexture;

namespace additionaltxd_impl
{
    static const char* AdditionalTxdListSharedName()
    {
        return "AdditionalTxdListGet";
    }

    using AdditionalTxdList = std::vector<std::string>;
    using AdditionalTxdListGetter = const AdditionalTxdList* (*)();

    // GTA SA 1.0 US addresses
    static constexpr uintptr_t kCall_FindNamedTexture = 0x731733;
    static constexpr uintptr_t kCall_AssignRemapTxd = 0x5B62C2;

    // Game functions
    static constexpr uintptr_t kFn_CTxdStore_AddRefByName = 0x731C80;
    static constexpr uintptr_t kFn_CTxdStore_GetTxdDictionary = 0x408340;
    static constexpr uintptr_t kFn_CStreaming_RequestTxdModel = 0x407100;
    static constexpr uintptr_t kFn_CStreaming_LoadAllRequestedModels = 0x40EA10;

    using fnFindNamedTex = RwTexture * (__cdecl*)(RwTexDictionary*, const char*);
    using fnAssignRemap = void(__cdecl*)(const char*, uint16_t);

    using fnAddRefByName = int(__cdecl*)(char*);
    using fnGetTxdDict = RwTexDictionary * (__cdecl*)(int);
    using fnRequestTxd = void(__cdecl*)(int, int);
    using fnLoadAllReq = void(__cdecl*)(bool);

    static fnFindNamedTex g_ogFindNamedTex = nullptr;
    static fnAssignRemap  g_ogAssignRemap = nullptr;

    static std::vector<uint16_t>          g_txdIds;
    static std::vector<RwTexDictionary*>  g_extraDicts;
    static AdditionalTxdList             g_additionalTxdFiles;

    static bool g_hasFastloader = false;
    static bool g_hasAdditional = false;
    static bool g_loaded = false;
    static bool g_dirty = false;

    // --- NOWA FUNKCJA POMOCNICZA ---
    // Zamienia string na małe litery, żeby ignorować wielkość liter przy porównywaniu
    static std::string ToLower(std::string str)
    {
        std::transform(str.begin(), str.end(), str.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return str;
    }

    static fnAddRefByName AddRefByName()
    {
        return reinterpret_cast<fnAddRefByName>(kFn_CTxdStore_AddRefByName);
    }

    static fnGetTxdDict GetTxdDictionary()
    {
        return reinterpret_cast<fnGetTxdDict>(kFn_CTxdStore_GetTxdDictionary);
    }

    static fnRequestTxd RequestTxdModel()
    {
        return reinterpret_cast<fnRequestTxd>(kFn_CStreaming_RequestTxdModel);
    }

    static fnLoadAllReq LoadAllRequestedModels()
    {
        return reinterpret_cast<fnLoadAllReq>(kFn_CStreaming_LoadAllRequestedModels);
    }

    static bool IsFastloaderName(const char* txdName)
    {
        // Sprawdza tylko prefiks "fastloader" - to działało dobrze w Twoich testach
        return (txdName && std::strncmp(txdName, "fastloader", 10) == 0);
    }

    static bool IsAdditionalName(const char* txdName)
    {
        if (!txdName)
            return false;

        std::string normalized = modloader::NormalizePath(txdName);
        std::string base = modloader::GetPathComponentBack(normalized);
        auto dot = base.rfind('.');
        if (dot != std::string::npos)
            base = base.substr(0, dot);

        // --- POPRAWKA ---
        // Zamieniamy nazwę otrzymaną z gry na małe litery przed sprawdzeniem w liście.
        // Jeśli gra zapyta o "LALA", a w configu masz "lala", teraz to zadziała.
        base = ToLower(base);

        for (const auto& entry : g_additionalTxdFiles)
        {
            // entry też jest trzymane jako lowercase (zrobione w SyncAdditionalTxdFiles)
            if (entry == base)
                return true;
        }
        return false;
    }

    static void SyncAdditionalTxdFiles()
    {
        AdditionalTxdList new_list;
        if (modloader_shdata_t* data = modloader::plugin_ptr->loader->FindSharedData(AdditionalTxdListSharedName()))
        {
            if (data->type == MODLOADER_SHDATA_FUNCTION)
            {
                auto getter = reinterpret_cast<AdditionalTxdListGetter>(data->f);
                if (const AdditionalTxdList* list = getter())
                    new_list = *list;
            }
        }

        // --- POPRAWKA ---
        // NormalizePath zwraca string, więc musimy przypisać wynik.
        // Dodatkowo wymuszamy małe litery, żeby lista była spójna.
        for (auto& entry : new_list) {
            entry = modloader::NormalizePath(entry);
            entry = ToLower(entry);
        }

        const bool changed = (new_list != g_additionalTxdFiles);
        g_additionalTxdFiles = std::move(new_list);
        g_hasAdditional = !g_additionalTxdFiles.empty();

        if (!g_hasAdditional)
        {
            g_extraDicts.clear();
            g_loaded = false;
        }

        if (changed)
            g_dirty = true;
    }

    static void RebuildCache()
    {
        if (!g_hasFastloader && !g_hasAdditional)
            return;

        g_extraDicts.clear();

        for (uint16_t txdId : g_txdIds)
            RequestTxdModel()(static_cast<int>(txdId), 2);

        LoadAllRequestedModels()(true);

        for (uint16_t txdId : g_txdIds)
        {
            RwTexDictionary* dict = GetTxdDictionary()(static_cast<int>(txdId));
            if (dict)
                g_extraDicts.push_back(dict);
        }

        g_loaded = !g_extraDicts.empty();
        g_dirty = false;
    }

    static void EnsureLoaded()
    {
        if (!g_hasFastloader && !g_hasAdditional)
            return;

        if (g_dirty || !g_loaded)
            RebuildCache();
    }

    static void PreloadOne(uint16_t txdId)
    {
        RequestTxdModel()(static_cast<int>(txdId), 2);
        LoadAllRequestedModels()(true);

        RwTexDictionary* dict = GetTxdDictionary()(static_cast<int>(txdId));
        if (dict)
        {
            if (std::find(g_extraDicts.begin(), g_extraDicts.end(), dict) == g_extraDicts.end())
                g_extraDicts.push_back(dict);

            g_loaded = true;
        }
    }

    static RwTexture* __cdecl hkRwTexDictionaryFindNamedTexture(RwTexDictionary* dict, const char* name)
    {
        RwTexture* tex = g_ogFindNamedTex(dict, name);
        if (tex)
            return tex;

        if (!g_hasFastloader && !g_hasAdditional)
            return nullptr;

        EnsureLoaded();

        for (RwTexDictionary* extraDict : g_extraDicts)
        {
            if (!extraDict) continue;
            tex = g_ogFindNamedTex(extraDict, name);
            if (tex) return tex;
        }

        return nullptr;
    }

    static void __cdecl hkAssignRemapTxd(const char* txdName, uint16_t txdId)
    {
        g_ogAssignRemap(txdName, txdId);

        const bool is_fastloader = IsFastloaderName(txdName);
        const bool is_additional = IsAdditionalName(txdName);

        if (!is_fastloader && !is_additional)
            return;

        const bool isNew =
            (std::find(g_txdIds.begin(), g_txdIds.end(), txdId) == g_txdIds.end());

        if (isNew)
        {
            g_txdIds.push_back(txdId);
            g_dirty = true;
        }

        AddRefByName()(const_cast<char*>(txdName));

        g_hasFastloader = g_hasFastloader || is_fastloader;
        g_hasAdditional = g_hasAdditional || is_additional;

        PreloadOne(txdId);
    }

    static bool PatchCall(uintptr_t callSite, void* newTarget, void** outOriginal)
    {
        auto* p = reinterpret_cast<uint8_t*>(callSite);
        if (*p != 0xE8)
            return false;

        int32_t oldRel = *reinterpret_cast<int32_t*>(p + 1);
        uintptr_t oldTarget = callSite + 5u + static_cast<uintptr_t>(oldRel);
        if (outOriginal)
            *outOriginal = reinterpret_cast<void*>(oldTarget);

        uintptr_t newT = reinterpret_cast<uintptr_t>(newTarget);
        int32_t newRel = static_cast<int32_t>(newT - (callSite + 5u));

        DWORD oldProtect = 0;
        if (!VirtualProtect(reinterpret_cast<void*>(callSite), 5, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        *p = 0xE8;
        *reinterpret_cast<int32_t*>(p + 1) = newRel;

        DWORD tmp = 0;
        VirtualProtect(reinterpret_cast<void*>(callSite), 5, oldProtect, &tmp);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(callSite), 5);
        return true;
    }

    static bool Hook()
    {
        bool ok1 = PatchCall(kCall_FindNamedTexture,
            reinterpret_cast<void*>(&hkRwTexDictionaryFindNamedTexture),
            reinterpret_cast<void**>(&g_ogFindNamedTex));

        bool ok2 = PatchCall(kCall_AssignRemapTxd,
            reinterpret_cast<void*>(&hkAssignRemapTxd),
            reinterpret_cast<void**>(&g_ogAssignRemap));

        return ok1 && ok2 && g_ogFindNamedTex && g_ogAssignRemap;
    }
}

class additionaltxd : public modloader::basic_plugin
{
public:
    const info& GetInfo();
    bool OnStartup();
    bool OnShutdown();
    int GetBehaviour(modloader::file&);
    bool InstallFile(const modloader::file&);
    bool ReinstallFile(const modloader::file&);
    bool UninstallFile(const modloader::file&);
    void Update() override;

} additionaltxd_plugin;

REGISTER_ML_PLUGIN(::additionaltxd_plugin);

const additionaltxd::info& additionaltxd::GetInfo()
{
    static const char* extable[] = { 0 };
    static const info xinfo = { "std.additionaltxd", get_version_by_date(), "LINK/2012 + port+fix", -1, extable };
    return xinfo;
}

bool additionaltxd::OnStartup()
{
    additionaltxd_impl::SyncAdditionalTxdFiles();
    if (additionaltxd_impl::Hook())
        Log("std.additionaltxd: hooks installed");
    else
        Log("std.additionaltxd: failed to install hooks (wrong exe version / address?)");

    return true;
}

bool additionaltxd::OnShutdown()
{
    return true;
}

void additionaltxd::Update()
{
    additionaltxd_impl::SyncAdditionalTxdFiles();
}

int additionaltxd::GetBehaviour(modloader::file&)
{
    return MODLOADER_BEHAVIOUR_NO;
}

bool additionaltxd::InstallFile(const modloader::file&) { return true; }
bool additionaltxd::ReinstallFile(const modloader::file&) { return true; }
bool additionaltxd::UninstallFile(const modloader::file&) { return true; }