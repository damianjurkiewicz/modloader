/*
 * Copyright (C) 2013-2014  LINK/2012 <dma_2012@hotmail.com>
 * Licensed under the MIT License, see LICENSE at top level directory.
 *
 *  std.helloworld -- Minimal Mod Loader plugin example
 *
 */
#include <stdinc.hpp>
using namespace modloader;

/*
 *  The plugin object
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

} hello_world_plugin;

REGISTER_ML_PLUGIN(::hello_world_plugin);

/*
 *  HelloWorldPlugin::GetInfo
 *      Returns information about this plugin
 */
const additionaltxd::info& additionaltxd::GetInfo()
{
    static const char* extable[] = { 0 };
    static const info xinfo      = { "std.helloworld", get_version_by_date(), "LINK/2012", -1, extable };
    return xinfo;
}

/*
 *  HelloWorldPlugin::OnStartup
 *      Startups the plugin
 */
bool additionaltxd::OnStartup()
{
    Log("HELLO FROM std.helloworld");
    return true;
}

/*
 *  HelloWorldPlugin::OnShutdown
 *      Shutdowns the plugin
 */
bool additionaltxd::OnShutdown()
{
    return true;
}

/*
 *  HelloWorldPlugin::GetBehaviour
 *      Gets the relationship between this plugin and the file
 */
int additionaltxd::GetBehaviour(modloader::file&)
{
    return MODLOADER_BEHAVIOUR_NO;
}

/*
 *  HelloWorldPlugin::InstallFile
 *      Installs a file using this plugin
 */
bool additionaltxd::InstallFile(const modloader::file&)
{
    return true;
}

/*
 *  HelloWorldPlugin::ReinstallFile
 *      Reinstall a file previosly installed that has been updated
 */
bool additionaltxd::ReinstallFile(const modloader::file&)
{
    return true;
}

/*
 *  HelloWorldPlugin::UninstallFile
 *      Uninstall a previosly installed file
 */
bool additionaltxd::UninstallFile(const modloader::file&)
{
    return true;
}