#include <iostream>
#include <string>
#include "../HttpRequestPacket.h"
#include "../HttpResponsePacket.h"
using namespace std;

// string替换
std::string& ReplaceStr(std::string& str, const std::string& oldValue, const std::string& newValue) 
{
    for (std::string::size_type pos(0); pos != std::string::npos; pos += newValue.length()) {
        if ((pos = str.find(oldValue, pos)) != std::string::npos)
            str.replace(pos, oldValue.length(), newValue);
        else
            break;
    }
    return str;
}

// 插件加载时调用
extern "C" bool Init(const char* configFile)
{
    cout << "[INFO][RenameNginx] Plugin demo \"RenameNginx\" loaded." << endl;
    return true;
}

// 插件卸载时调用
extern "C" void Shutdown()
{
    cout << "[INFO][RenameNginx] Plugin demo \"RenameNginx\" unloaded." << endl;
}

// 有客户端请求到达时调用
extern "C" bool ClientRequest(HttpRequestPacket *packet)
{
    cout << "[INFO][RenameNginx] Get client request: " << packet->requestLine << endl;

    // 关闭gzip压缩，为了后面可以修改response
    packet->headers["Accept-Encoding"] = "";
    return true;
}

// 有服务端响应到达时调用
extern "C" bool ServerResponse(HttpResponsePacket *packet)
{
    cout << "[INFO][RenameNginx] Get server response: " << packet->responseLine << endl;

    // 如果响应是200，且Content-Type是html，则修改其中内容
    if(packet->code == 200 && packet->headers["Content-Type"].find("text/html") != std::string::npos)
    {
        ReplaceStr(packet->bodyData, "nginx news", "Proxy Server has Modified this page!");
        cout << "[INFO][RenameNginx] Replace \"nginx news\" -> \"Proxy Server has Modified this page!\"" << endl;
    }
    return true;
}