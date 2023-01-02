#include "Plugins.h"
#include <dlfcn.h>
#include <dirent.h>
#include <string>
#include <vector>
#include <map>
#include <sys/stat.h>
#include <sys/types.h>
#include "HttpRequestPacket.h"
#include "HttpResponsePacket.h"
#include "Logger.h"
using namespace std;

// 声明so中的函数原型
typedef bool (*InitFunction)(const char *configFile);
typedef void (*ShutdownFunction)();
typedef bool (*ClientRequestFunction)(HttpRequestPacket *packet);
typedef bool (*ServerResponseFunction)(HttpResponsePacket *packet);

map<string, void *> pluginsList;    //插件列表
Logger pluginsLogger;

// 装载所有插件
void LoadPlugins(const char *pluginDir, const char* configFilePath)
{
    pluginsLogger.setPrefix("PluginsLoader");

    // 遍历pluginDir目录
    struct dirent *ent;
    DIR *pDir = opendir(pluginDir);
    if(pDir == NULL)
    {
        // 目录不存在，创建一个
        mkdir(pluginDir, 0777);
        pluginsLogger.info("Plugins dir no found. Created as %s.", pluginDir);
    }
    else
    {
        while ((ent = readdir(pDir)) != NULL)
        {
            if (ent->d_type & DT_DIR)
            {
                // 是目录，不管
                ;
            }
            else
            {
                // 是文件，尝试加载
                string fileName = ent->d_name;
                if (EndsWith(fileName, ".so"))
                {
                    // 加载so
                    pluginsLogger.info("Find plugin %s, try loading...", fileName.c_str());
                    string soPath = string(pluginDir) + "/" + fileName;
                    void *soHandle = dlopen(soPath.c_str(), RTLD_LAZY);
                    if (!soHandle)
                    {
                        // 加载失败
                        pluginsLogger.warn("Fail to load so.");
                        continue;
                    }
                    InitFunction initFunc = (InitFunction)dlsym(soHandle, "Init");
                    if (dlerror() != NULL)
                    {
                        // 查找函数失败
                        pluginsLogger.warn("Fail to call Init function.");
                        dlclose(soHandle);
                        continue;
                    }
                    // 调用Init函数
                    if(!initFunc(configFilePath))
                    {
                        // Init未成功
                        pluginsLogger.warn("Plugin init failed.");
                        dlclose(soHandle);
                        continue;
                    }

                    pluginsList[fileName] = soHandle;
                    pluginsLogger.info("Plugin <%s> loaded.", fileName.c_str());
                }
            }
        }
        pluginsLogger.info("%d plugins loaded in all.", pluginsList.size());
    }
}

// 卸载所有插件
void UnloadPlugins()
{
    // 遍历每个插件
    for(auto &item : pluginsList)
    {
        const string &name = item.first;
        void* soHandle = item.second;
        pluginsLogger.debug("Unloading plugin %s...", name.c_str());

        ShutdownFunction shutdownFunc = (ShutdownFunction)dlsym(soHandle, "Shutdown");
        if (dlerror() == NULL)
        {
            // 查找函数成功，调用Shutdown
            shutdownFunc();
        }
        else 
            pluginsLogger.warn("Fail to call Shutdown function");
        // 卸载so
        dlclose(soHandle);
        pluginsLogger.info("Plugin <%s> unloaded.", name.c_str());
    }
}

// Client请求到达，呼叫所有插件
void PluginsCallClientRequest(HttpRequestPacket *packet)
{
    // 遍历每个插件
    for(auto &item : pluginsList)
    {
        const string &name = item.first;
        void* soHandle = item.second;
        pluginsLogger.debug("Client request received. Calling plugin %s...", name.c_str());

        ClientRequestFunction clientReqFunc = (ClientRequestFunction)dlsym(soHandle, "ClientRequest");
        if (dlerror() == NULL)
        {
            // 查找函数成功，调用
            if(!clientReqFunc(packet))
            {
                pluginsLogger.debug("Plugin <%s> breaks the event calling to continue.", name.c_str());
                break;
            }
        }
    }
}

// Server响应到达，呼叫所有插件
void PluginsCallServerResponse(HttpResponsePacket *packet)
{
    // 遍历每个插件
    for(auto &item : pluginsList)
    {
        const string &name = item.first;
        void* soHandle = item.second;
        pluginsLogger.debug("Server response received. Calling plugin %s...", name.c_str());


        ServerResponseFunction serverRespFunc = (ServerResponseFunction)dlsym(soHandle, "ServerResponse");
        if (dlerror() == NULL)
        {
            // 查找函数成功，调用
            if(!serverRespFunc(packet))
            {
                pluginsLogger.debug("Plugin <%s> breaks the event calling to continue.", name.c_str());
                break;
            }
        }
    }
}