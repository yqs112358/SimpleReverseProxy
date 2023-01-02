#ifndef PLUGINS_BY_YQ
#define PLUGINS_BY_YQ

class HttpRequestPacket;
class HttpResponsePacket;

void LoadPlugins(const char* pluginDir, const char* configFilePath);
void UnloadPlugins();
void PluginsCallClientRequest(HttpRequestPacket *packet);
void PluginsCallServerResponse(HttpResponsePacket *packet);

#endif