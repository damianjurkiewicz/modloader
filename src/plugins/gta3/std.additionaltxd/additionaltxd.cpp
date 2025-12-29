/*
 * Copyright (C) 2013-2014  LINK/2012 <dma_2012@hotmail.com>
 * Licensed under the MIT License, see LICENSE at top level directory.
 *
 * std.additionaltxd -- Additional TXD Loader Logic (Ported & Fixed)
 *
 */
#include <stdinc.hpp>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm> // dla transform i tolower

using namespace modloader;

// --- Definicje Typów GTA SA ---
struct RwTexture;
struct RwTexDictionary;

// Definicje wskaźników na funkcje
using f_RwTexDictionaryFindNamedTexture = RwTexture * (__cdecl*)(RwTexDictionary* dict, const char* name);
using f_AssignRemapTxd = int(__cdecl*)(const char* txdName, unsigned short txdID);
using f_GetTexDictionary = RwTexDictionary * (__cdecl*)(int txdIndex);
using f_RequestTxdModel = void(__cdecl*)(int index, int flags);
using f_LoadAllRequestedModels = void(__cdecl*)(bool bOnlyPriorityModels);
using f_CTheScripts_Init = void(__cdecl*)();
using f_LoadTexDictionary = bool(__cdecl*)(const char* filename); // Funkcja ładująca TXD z pliku

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
const int ADDR_CFileLoader_LoadTexDictionary = 0x5B6170; // Funkcja CFileLoader::LoadTexDictionary

// --- Zmienne Globalne (State) ---
static std::vector<int> gTxdIDStore;
static std::vector<RwTexDictionary*> gTxdDictStore;
static std::vector<std::string> gPendingFastLoaders; // Lista plików do załadowania
static bool gbAdditionalTxdUsed = false;

// Oryginalne funkcje (Trampoliny)
static f_RwTexDictionaryFindNamedTexture ogFindNamedTex = nullptr;
static f_AssignRemapTxd ogAssignRemapTex = nullptr;
static f_CTheScripts_Init ogCTheScriptsInit = nullptr;

// --- Funkcje Hooków (Logic) ---

// Hook: Przechwycenie szukania tekstury
static RwTexture* hkRwTexDictionaryFindNamedTexture(RwTexDictionary* dict, const char* name)
{
    RwTexture* pTex = ogFindNamedTex(dict, name);

    if (!pTex && gbAdditionalTxdUsed)
    {
        for (auto& extraDict : gTxdDictStore)
        {
            if (extraDict) {
                pTex = ((f_RwTexDictionaryFindNamedTexture)ADDR_RwTexDictionaryFindNamedTexture_Func)(extraDict, name);
                if (pTex) break;
            }
        }
    }
    return pTex;
}

// Hook: Przechwycenie rejestracji TXD (wykrywanie "fastloader")
// Ta funkcja uruchomi się automatycznie, gdy wywołamy LoadTexDictionary w Init
static void hkAssignRemapTxd(const char* txdName, unsigned short txdId)
{
    if (!txdName) return;

    size_t len = strlen(txdName);
    // Sprawdź czy nazwa > 10 znaków, kończy się cyfrą i zaczyna od "fastloader"
    if (len > 10 && isdigit(txdName[len - 1]) && !strncmp(txdName, "fastloader", 10))
    {
        gTxdIDStore.push_back(txdId);

        // CTxdStore::AddRef(txdId) - zapobiega wyładowaniu
        ((void(__cdecl*)(int))ADDR_CTxdStore_AddRef)(txdId);

        gbAdditionalTxdUsed = true;
    }
    else
    {
        ogAssignRemapTex(txdName, txdId);
    }
}

// Funkcja pomocnicza: Ładowanie zebranych modeli
static void LoadAdditionalTxds()
{
    if (gbAdditionalTxdUsed)
    {
        // Request modeli
        for (auto& id : gTxdIDStore)
        {
            ((f_RequestTxdModel)ADDR_RequestTxdModel)(id, 6); // PRIORITY | KEEP_IN_MEMORY
        }

        ((f_LoadAllRequestedModels)ADDR_LoadAllRequestedModels)(true);

        // Pobierz wskaźniki
        for (auto& id : gTxdIDStore)
        {
            RwTexDictionary* dict = ((f_GetTexDictionary)ADDR_GetTexDictionary)(id);
            if (dict) {
                gTxdDictStore.push_back(dict);
            }
        }
    }
}

// Hook: CTheScripts::Init
static void hkCTheScripts_Init()
{
    // 1. Najpierw ręcznie ładujemy pliki wykryte przez InstallFile
    // To zastępuje potrzebę posiadania pliku .ide
    for (const auto& filename : gPendingFastLoaders)
    {
        // Wywołujemy CFileLoader::LoadTexDictionary("fastloader1.txd")
        // To wewnętrznie wywoła hkAssignRemapTxd, co zarejestruje plik w naszym pluginie
        ((f_LoadTexDictionary)ADDR_CFileLoader_LoadTexDictionary)(filename.c_str());
    }

    // 2. Ładujemy zawartość tekstur do pamięci
    LoadAdditionalTxds();

    // 3. Oryginalna inicjalizacja
    if (ogCTheScriptsInit) ogCTheScriptsInit();
}


// --- Struktura Pluginu Mod Loadera ---

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

const additionaltxd::info& additionaltxd::GetInfo()
{
    static const char* extable[] = { 0 };
    static const info xinfo = { "std.additionaltxd", get_version_by_date(), "LINK/2012 / Ported", -1, extable };
    return xinfo;
}

bool additionaltxd::OnStartup()
{
    // Hook RwTexDictionaryFindNamedTexture
    ogFindNamedTex = (f_RwTexDictionaryFindNamedTexture)injector::GetBranchDestination(ADDR_RwTexDictionaryFindNamedTexture_Call).get();
    injector::MakeCALL(ADDR_RwTexDictionaryFindNamedTexture_Call, hkRwTexDictionaryFindNamedTexture);

    // Hook AssignRemapTxd
    ogAssignRemapTex = (f_AssignRemapTxd)injector::GetBranchDestination(ADDR_AssignRemapTxd_Call).get();
    injector::MakeCALL(ADDR_AssignRemapTxd_Call, hkAssignRemapTxd);

    // Hook CTheScripts::Init
    ogCTheScriptsInit = (f_CTheScripts_Init)injector::GetBranchDestination(ADDR_CGame_Initialise_CallScriptsInit).get();
    injector::MakeCALL(ADDR_CGame_Initialise_CallScriptsInit, hkCTheScripts_Init);

    return true;
}

bool additionaltxd::OnShutdown()
{
    return true;
}

int additionaltxd::GetBehaviour(modloader::file& file)
{
    // Skanujemy pliki w poszukiwaniu fastloader*.txd
    if (file.is_ext("txd"))
    {
        const char* fname = file.filename(); // np. "fastloader1.txd"
        size_t len = strlen(fname);

        // Sprawdzenie warunku: nazwa > 10 znaków + cyfra na końcu (przed kropką)
        // Nazwa z rozszerzeniem: fastloader1.txd (15 znaków)
        // Sprawdzamy samą nazwę bez rozszerzenia dla logiki pluginu, ale tutaj mamy nazwę pliku

        // Prostsze sprawdzenie: czy zaczyna się od "fastloader" i ma cyfrę
        std::string sName = fname;
        std::transform(sName.begin(), sName.end(), sName.begin(), ::tolower);

        if (sName.find("fastloader") == 0 && sName.length() > 14) // fastloader (10) + digit (1) + .txd (4) = 15 min
        {
            // To jest nasz plik! Chcemy go przejąć
            return MODLOADER_BEHAVIOUR_YES;
        }
    }
    return MODLOADER_BEHAVIOUR_NO;
}

bool additionaltxd::InstallFile(const modloader::file& file)
{
    if (file.is_ext("txd"))
    {
        // Dodajemy nazwę pliku do listy oczekujących na załadowanie
        gPendingFastLoaders.push_back(file.filename());
        return true;
    }
    return false;
}

bool additionaltxd::ReinstallFile(const modloader::file&) { return true; }
bool additionaltxd::UninstallFile(const modloader::file&) { return true; }