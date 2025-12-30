/*
 * Copyright (C) 2013-2014  LINK/2012 <dma_2012@hotmail.com>
 * Licensed under the MIT License, see LICENSE at top level directory.
 *
 * std.additionaltxd -- Additional TXD dictionaries (fastloader*)
 *
 * Ported from plugin-sdk based implementation (src-additionaltxd.cpp).
 */

#include <stdinc.hpp>

#include <interfaces/gta3/std.data.hpp>
#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <unordered_set>
#include <vector>



using namespace modloader;

// --- Minimal RenderWare forward declarations ---
struct RwTexDictionary;
struct RwTexture;

namespace additionaltxd_impl
{
    // GTA SA 1.0 US addresses used by the original implementation
    // NOTE: If you need other EXE versions, these must be translated.
    static constexpr uintptr_t kCall_FindNamedTexture = 0x731733;
    static constexpr uintptr_t kCall_AssignRemapTxd = 0x5B62C2;

    // Game functions (addresses known from common GTA SA databases / plugin-sdk meta)
    static constexpr uintptr_t kFn_CTxdStore_AddRefByName = 0x731C80; // int __cdecl CTxdStore::AddRef(char* name)
    static constexpr uintptr_t kFn_CTxdStore_GetTxdDictionary = 0x408340; // RwTexDictionary* __cdecl CTxdStore::GetTxd(int id)
    static constexpr uintptr_t kFn_CStreaming_RequestTxdModel = 0x407100; // void __cdecl CStreaming::RequestTxdModel(int txdId, int flags)
    static constexpr uintptr_t kFn_CStreaming_LoadAllRequestedModels = 0x40EA10; // void __cdecl CStreaming::LoadAllRequestedModels(bool)

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
    static std::unordered_set<std::string> g_atxdRegistry;
    static uint32_t g_atxdRevision = 0;

    static bool g_hasFastloader = false;
    static bool g_loaded = false;  // means: at least one extra dict cached
    static bool g_dirty = false;  // ids changed => cache rebuild needed

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
        return (txdName && std::strncmp(txdName, "fastloader", 10) == 0);
    }

    static void SyncAtxdRegistry()
    {
        auto* shared = GetStdDataAtxdSharedData();
        if(!shared)
            return;

        if(shared->revision == g_atxdRevision)
            return;

        g_atxdRevision = shared->revision;
        g_atxdRegistry.clear();

        for(const auto& name : shared->txd_names)
        {
            std::string lowered = name;
            modloader::tolower(lowered);
            if(!lowered.empty())
                g_atxdRegistry.emplace(std::move(lowered));
        }

        g_txdIds.clear();
        g_extraDicts.clear();
        g_loaded = false;
        g_dirty = true;
        g_hasFastloader = !g_atxdRegistry.empty();
    }

    static bool IsRegisteredAtxdName(const char* txdName)
    {
        if(!txdName)
            return false;

        std::string lowered = txdName;
        modloader::tolower(lowered);
        return g_atxdRegistry.find(lowered) != g_atxdRegistry.end();
    }

    static void RebuildCache()
    {
        if (!g_hasFastloader)
            return;

        g_extraDicts.clear();

        // Request all stored TXD slots
        for (uint16_t txdId : g_txdIds)
            RequestTxdModel()(static_cast<int>(txdId), 2 /* STREAMING_PRIORITY_REQUEST */);

        // Force streaming to load what was requested
        LoadAllRequestedModels()(true);

        // Cache dictionaries
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
        if (!g_hasFastloader)
            return;

        // Rebuild if ids changed or we never successfully cached any dict
        if (g_dirty || !g_loaded)
            RebuildCache();
    }

    static void PreloadOne(uint16_t txdId)
    {
        // Preload as early as possible to avoid "too late" misses in FindNamedTexture
        RequestTxdModel()(static_cast<int>(txdId), 2);
        LoadAllRequestedModels()(true);

        RwTexDictionary* dict = GetTxdDictionary()(static_cast<int>(txdId));
        if (dict)
        {
            // Avoid duplicates
            if (std::find(g_extraDicts.begin(), g_extraDicts.end(), dict) == g_extraDicts.end())
                g_extraDicts.push_back(dict);

            g_loaded = true;
        }
    }

    static RwTexture* __cdecl hkRwTexDictionaryFindNamedTexture(RwTexDictionary* dict, const char* name)
    {
        SyncAtxdRegistry();

        // First try original dictionary
        RwTexture* tex = g_ogFindNamedTex(dict, name);
        if (tex)
            return tex;

        if (!g_hasFastloader)
            return nullptr;

        // Make sure extra dicts are ready
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
        // CRITICAL: Always execute original behavior to keep game's mapping intact
        g_ogAssignRemap(txdName, txdId);

        SyncAtxdRegistry();

        if (!IsFastloaderName(txdName) || !IsRegisteredAtxdName(txdName))
            return;

        // Track TXD slot id
        const bool isNew =
            (std::find(g_txdIds.begin(), g_txdIds.end(), txdId) == g_txdIds.end());

        if (isNew)
        {
            g_txdIds.push_back(txdId);
            g_dirty = true; // ids changed => cache rebuild needed
        }

        // Keep it referenced so it doesn't get unloaded
        AddRefByName()(const_cast<char*>(txdName));

        g_hasFastloader = true;

       

        // Recommended: preload right here (early enough before materials resolve textures)
        PreloadOne(txdId);
    }

    static bool PatchCall(uintptr_t callSite, void* newTarget, void** outOriginal)
    {
        // CALL rel32 is 0xE8 <rel32>
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

/*
 * The plugin object
 */
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
    additionaltxd_impl::SyncAtxdRegistry();

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

int additionaltxd::GetBehaviour(modloader::file&)
{
    // This plugin doesn't install files; it only hooks game functions.
    return MODLOADER_BEHAVIOUR_NO;
}

bool additionaltxd::InstallFile(const modloader::file&) { return true; }
bool additionaltxd::ReinstallFile(const modloader::file&) { return true; }
bool additionaltxd::UninstallFile(const modloader::file&) { return true; }
