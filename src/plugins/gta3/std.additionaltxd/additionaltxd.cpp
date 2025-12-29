/*
 * Copyright (C) 2013-2014  LINK/2012 <dma_2012@hotmail.com>
 * Licensed under the MIT License, see LICENSE at top level directory.
 *
 * std.additionaltxd -- Additional TXD Loader Logic (Stable)
 *
 */
#include <stdinc.hpp>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <cstdio>

using namespace modloader;

// --- Definicje Typów ---
struct RwTexture;
struct RwTexDictionary;

using f_RwTexDictionaryFindNamedTexture = RwTexture* (__cdecl*)(RwTexDictionary* dict, const char* name);
using f_AssignRemapTxd = int (__cdecl*)(const char* txdName, unsigned short txdID);
using f_GetTexDictionary = RwTexDictionary* (__cdecl*)(int txdIndex);
using f_RequestTxdModel = void (__cdecl*)(int index, int flags);
using f_LoadAllRequestedModels = void (__cdecl*)(bool bOnlyPriorityModels);
using f_CTheScripts_Init = void (__cdecl*)();
using f_LoadTexDictionary = bool (__cdecl*)(const char* filename);

// --- Adresy Pamięci (GTA SA 1.0 US) ---
const int ADDR_RwTexDictionaryFindNamedTexture_Call = 0x731733; 
const int ADDR_RwTexDictionaryFindNamedTexture_Func = 0x730E60; 
const int ADDR_AssignRemapTxd_Call = 0x5B62C2;
const int ADDR_GetTexDictionary = 0x408340;
const int ADDR_RequestTxdModel = 0x4087E0;
const int ADDR_LoadAllRequestedModels = 0x40EA10;
const int ADDR_CTxdStore_AddRef = 0x408460;
const int ADDR_CTheScripts_Init = 0x468D50;
const int ADDR_CGame_Initialise_CallScriptsInit = 0x4408C3;
const int ADDR_CFileLoader_LoadTexDictionary = 0x5B6170;

// --- Zmienne Globalne ---
static std::vector<int> gTxdIDStore;
static std::vector<RwTexDictionary*> gTxdDictStore;
static std::vector<std::string> gPendingFastLoaders;
static bool gbAdditionalTxdUsed = false;

static f_RwTexDictionaryFindNamedTexture ogFindNamedTex = nullptr;
static f_AssignRemapTxd ogAssignRemapTex = nullptr;
static f_CTheScripts_Init ogCTheScriptsInit = nullptr;

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
} hello_world_plugin;

REGISTER_ML_PLUGIN(::hello_world_plugin);

// --- Hooki ---

static RwTexture* hkRwTexDictionaryFindNamedTexture(RwTexDictionary* dict, const char* name)
{
    // Bezpieczne wywołanie oryginału
    RwTexture* pTex = nullptr;
    if (ogFindNamedTex) pTex = ogFindNamedTex(dict, name);
    
    if (!pTex && gbAdditionalTxdUsed)
    {
        for (auto& extraDict : gTxdDictStore)
        {
            if (extraDict) {
                // Bezpośrednie wywołanie silnika
                pTex = ((f_RwTexDictionaryFindNamedTexture)ADDR_RwTexDictionaryFindNamedTexture_Func)(extraDict, name);
                if (pTex) break;
            }
        }
    }
    return pTex;
}

static void hkAssignRemapTxd(const char* txdName, unsigned short txdId)
{
    // Zabezpieczenie przed NULL
    if (!txdName) {
        if(ogAssignRemapTex) ogAssignRemapTex(txdName, txdId);
        return;
    }

    size_t len = strlen(txdName);
    bool isFastLoader = (len > 10 && isdigit(txdName[len - 1]) && !strncmp(txdName, "fastloader", 10));

    if (isFastLoader)
    {
        gTxdIDStore.push_back(txdId);
        // Ważne: to wywołanie musi być w 10us.hpp!
        ((void(__cdecl*)(int))ADDR_CTxdStore_AddRef)(txdId);
        gbAdditionalTxdUsed = true;
    }
    else
    {
        if(ogAssignRemapTex) ogAssignRemapTex(txdName, txdId);
    }
}

static void LoadAdditionalTxds()
{
    if (gbAdditionalTxdUsed)
    {
        for (auto& id : gTxdIDStore)
            ((f_RequestTxdModel)ADDR_RequestTxdModel)(id, 6);

        ((f_LoadAllRequestedModels)ADDR_LoadAllRequestedModels)(true);

        for (auto& id : gTxdIDStore)
        {
            RwTexDictionary* dict = ((f_GetTexDictionary)ADDR_GetTexDictionary)(id);
            if (dict) gTxdDictStore.push_back(dict);
        }
    }
}

static void hkCTheScripts_Init()
{
    // Ładowanie plików wykrytych przez InstallFile
    for (const auto& filename : gPendingFastLoaders)
    {
        ((f_LoadTexDictionary)ADDR_CFileLoader_LoadTexDictionary)(filename.c_str());
    }

    LoadAdditionalTxds();
    
    if(ogCTheScriptsInit) ogCTheScriptsInit();
}

// --- Implementacja Pluginu ---

const additionaltxd::info& additionaltxd::GetInfo()
{
    static const char* extable[] = { 0 };
    static const info xinfo      = { "std.additionaltxd", get_version_by_date(), "FIXED BUILD", -1, extable };
    return xinfo;
}

bool additionaltxd::OnStartup()
{
    // UWAGA: Używamy wartości zwracanej przez MakeCALL, aby pobrać prawidłowy wskaźnik do oryginału.
    // To jest bezpieczniejsze niż GetBranchDestination w środowisku Mod Loadera.
    
    // 1. Hook Texture Find
    ogFindNamedTex = injector::MakeCALL(ADDR_RwTexDictionaryFindNamedTexture_Call, hkRwTexDictionaryFindNamedTexture).get();

    // 2. Hook Assign Remap
    ogAssignRemapTex = injector::MakeCALL(ADDR_AssignRemapTxd_Call, hkAssignRemapTxd).get();

    // 3. Hook Scripts Init
    ogCTheScriptsInit = injector::MakeCALL(ADDR_CGame_Initialise_CallScriptsInit, hkCTheScripts_Init).get();

    return true;
}

bool additionaltxd::OnShutdown() { return true; }

int additionaltxd::GetBehaviour(modloader::file& file)
{
    if (file.is_ext("txd"))
    {
        std::string sName = file.filename();
        std::transform(sName.begin(), sName.end(), sName.begin(), ::tolower);
        
        if (sName.find("fastloader") == 0 && sName.length() > 14)
            return MODLOADER_BEHAVIOUR_YES;
    }
    return MODLOADER_BEHAVIOUR_NO;
}

bool additionaltxd::InstallFile(const modloader::file& file)
{
    if (file.is_ext("txd"))
    {
        gPendingFastLoaders.push_back(file.filename());
        return true; 
    }
    return false;
}

bool additionaltxd::ReinstallFile(const modloader::file&) { return true; }
bool additionaltxd::UninstallFile(const modloader::file&) { return true; }