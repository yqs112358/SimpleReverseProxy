#define CONFIG_FILE_PATH "./config.ini"
#define PLUGINS_DIR "./plugins"

#include <iostream>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ThreadPool/threadpool.h"
#include "SimpleIni/SimpleIni.h"
#include "HttpRequestPacket.h"
#include "HttpResponsePacket.h"
#include "Logger.h"
#include "Plugins.h"
#include "Utils.h"
using namespace std;

// 全局数据
// main
string listenHost = "0.0.0.0";
int listenPort = 8080;
int maxListen = 5;
int bufferSize = 4096;
// proxy
string targetHost = "";
int targetPort = 80;
// threadpool
int minThread = 3;
int maxThread = 20;
int waitBeforeShrink = 3;
// logger
Logger mainLogger;
Logger::LogLevel logLevel;

// 读取配置文件
bool ReadConfigFile()
{
    CSimpleIniA ini;
	ini.SetUnicode();

	SI_Error rc = ini.LoadFile(CONFIG_FILE_PATH);
	if (rc < 0) {
        std::cerr << "[ERROR] Fail to load config file!" << endl;
        return false;
    };

    // ListenHost
    const char* data;
	data = ini.GetValue("Main", "ListenHost", listenHost.c_str());
    listenHost = string(data);
    if(listenHost.empty())
        listenHost = "0.0.0.0";

    // ListenPort
    listenPort = ini.GetLongValue("Main", "ListenPort", listenPort);
    // MaxListen
    maxListen = ini.GetLongValue("Main", "MaxListen", maxListen);

    // BufferSize
    bufferSize = ini.GetLongValue("Main", "BufferSize", bufferSize);
    if(bufferSize <= 0)
        bufferSize = 4096;

    // LogLevel
    data = ini.GetValue("Main", "LogLevel", "INFO");
    if(strcmp(data, "DEBUG") == 0)
        logLevel = Logger::LogLevel::DEBUG;
    else if(strcmp(data, "INFO") == 0)
        logLevel = Logger::LogLevel::INFO;
    else if(strcmp(data, "WARN") == 0)
        logLevel = Logger::LogLevel::WARN;
    else if(strcmp(data, "ERROR") == 0)
        logLevel = Logger::LogLevel::ERROR;
    else if(strcmp(data, "FATAL") == 0)
        logLevel = Logger::LogLevel::FATAL;
    else if(strcmp(data, "NONE") == 0)
        logLevel = Logger::LogLevel::NONE;

    // TargetHost
    data = ini.GetValue("Proxy", "TargetHost", targetHost.c_str());
    targetHost = string(data);
    // 去掉前缀后缀
    if(StartsWith(targetHost, "http://"))
        targetHost = targetHost.substr(7);
    else if(StartsWith(targetHost, "https://"))
        targetHost = targetHost.substr(8);
    auto pos = targetHost.find("/");
    if(pos != std::string::npos)
        targetHost = targetHost.substr(0, pos);

    // TargetPort
    targetPort = ini.GetLongValue("Proxy", "TargetPort", targetPort);

    // MinThread
    minThread = ini.GetLongValue("ThreadPool", "MinThread", minThread);
    // MaxThread
    maxThread = ini.GetLongValue("ThreadPool", "MaxThread", maxThread);
    // WaitBeforeShrink
    waitBeforeShrink = ini.GetLongValue("ThreadPool", "WaitBeforeShrink", waitBeforeShrink);

    // 检查数据
    if(targetHost.empty() || listenPort < 0 || listenPort > 65535 || 
        targetPort < 0 || targetPort > 65535 )
    {
        std::cerr << "[ERROR] Bad config file!" << endl;
        return false;
    }
    return true;
}

// 代理worker类，对每个客户端有一个worker实例
// （由于反代需要维护一些状态，用类简单封装一下）
class ProxyClientWorker
{
    Logger &logger;

    int clientSocket;
    string clientIp;
    int clientPort;

    int serverSocket = 0;
    sockaddr_in serverAddr;
    string oldHostStr;
    string targetStr;

public:
    ProxyClientWorker(int clientSocket, sockaddr_in clientAddr, Logger &logger)
        :logger(logger), clientSocket(clientSocket)
    {
        // 将sockaddr_in中数据拆出
        char ipBuf[16] = {0};
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuf, 16);
        this->clientIp = string(ipBuf);
        this->clientPort = ntohs(clientAddr.sin_port);
    }

    ~ProxyClientWorker()
    {
        if(clientSocket > 0)
            close(clientSocket);
        if(serverSocket > 0)
            close(serverSocket);
    }

    // 连接到服务器
    bool connectToServer(string targetHost, int targetPort)
    {
        this->targetStr = targetHost;
        if(targetPort != 80)
            this->targetStr = this->targetStr + ":" + std::to_string(targetPort);
        logger.debug("Connecting to server %s...", this->targetStr.c_str());

        // 创建socket
        this->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        this->serverAddr.sin_family = AF_INET;
        this->serverAddr.sin_port = htons(targetPort);

        sockaddr_in addrTest;
        // 检查给出的host是否是IP
        if(inet_pton(AF_INET, targetHost.c_str(), &addrTest) > 0)
        {
            // 是IP，直接填充
            this->serverAddr.sin_addr.s_addr = inet_addr(targetHost.c_str());
            logger.debug("IP address given.");
        }
        else
        {
            // 不是IP，尝试做域名解析
            logger.debug("Not IP address. Try to resolve domain...");
            struct hostent* host = gethostbyname(targetHost.c_str());
            if (!host)      // 解析失败
            {
                logger.error("Fail to resolve domain: %s", targetHost.c_str());
                return false;
            }

            // 填充解析得到的第一个IP地址
            char serverIp[16] = {0};
            strncpy(serverIp, inet_ntoa(*(in_addr*)host->h_addr), 16);
            this->serverAddr.sin_addr.s_addr = inet_addr(serverIp);
            logger.debug("Resolved. Got IP: %s", serverIp);
        }
        return connect(this->serverSocket, (sockaddr *)&(this->serverAddr), sizeof(this->serverAddr)) >= 0;
    }

    // 处理客户端请求
    bool processClientRequest()
    {
        logger.debug("[S <- C] Client request received.");

        // recv
        char buf[bufferSize] = {0};
        int recvLen = recv(clientSocket, buf, bufferSize, 0);

        if (recvLen <= 0)   // 连接断开
        {
            logger.debug("[S <- C] Connection closed by client.");
            return false;
        }
        logger.debug("[S <- C] Recv %d bytes from client.", recvLen);

        // 拆分请求数据
        HttpRequestPacket packet(string(buf, recvLen));
        logger.info("[S <- C] %s", packet.requestLine.c_str());

        // 如果请求过长，接收剩余的body
        if(packet.headers.find("Transfer-Encoding") != packet.headers.end())
        {
            // 给出了Transfer-Encoding，检查是否为chunked模式
            string encoding = packet.headers["Transfer-Encoding"];
            logger.debug("[S <- C] Transfer-Encoding: %s", encoding.c_str());
            if(encoding.find("chunked") != std::string::npos)
            {
                // chunked模式，分块接收
                logger.debug("[S <- C] Chunked mode detected.");
                string nowBuf = packet.bodyData;
                packet.bodyData = "";

                // 循环处理，每次先读取一个chunk size，再根据size读取对应长度的数据块，直到size为0表示结束
                while(true)
                {
                    // 如果此时数据都不足以表示一个chunk size，先recv到足够为止
                    while(nowBuf.find("\r\n") == std::string::npos)
                    {
                        logger.debug("[S <- C] More data needed to get a chunk size, just recv");
                        char buf[bufferSize] = {0};
                        int recvLen = recv(clientSocket, buf, bufferSize , 0);
                        if(recvLen <= 0)
                        {
                            // 连接断开
                            logger.debug("[S <- C] Connection closed by client.");
                            return false;
                        }
                        nowBuf.append(string(buf, recvLen));
                        logger.debug("[S <- C] Recv %d, now data size in buffer: %d", recvLen, nowBuf.size());
                    }

                    // 切分出头部的chunk size（16进制表示）
                    auto chunkSizeDataSplit = nowBuf.find("\r\n");
                    string chunkSizeStr = nowBuf.substr(0, chunkSizeDataSplit);
                    int chunkSize = std::stoi(chunkSizeStr, 0, 16);
                    nowBuf = nowBuf.substr(chunkSizeDataSplit + 2);

                    // 如果chunk size为0，表示传输结束
                    if(chunkSize == 0)
                    {
                        logger.debug("[S <- C] Chunk transfer finished.");
                        packet.bodyData.append("0\r\n\r\n");
                        break;
                    }
                    else
                        logger.debug("[S <- C] Next chunk size: %d", chunkSize);

                    // 如果此时缓冲区内剩下的数据长度不够chunk size + 2，继续接收
                    while((int)nowBuf.size() < chunkSize + 2)
                    {
                        logger.debug("[S <- C] More data needed to get next chunk, just recv");                        
                        char buf[bufferSize] = {0};
                        int recvLen = recv(clientSocket, buf, bufferSize , 0);
                        if(recvLen <= 0)
                        {
                            // 连接断开
                            logger.debug("[S <- C] Connection closed by client.");
                            return false;
                        }
                        nowBuf.append(string(buf, recvLen));
                        logger.debug("[S <- C] Recv %d, now data size in buffer: %d", recvLen, nowBuf.size());
                    }

                    // 切分出这一块chunk的数据，放入bodyData
                    // 跳过\r\n，继续下一次循环
                    packet.bodyData.append(chunkSizeStr + "\r\n");
                    packet.bodyData.append(nowBuf.substr(0, chunkSize));
                    packet.bodyData.append("\r\n");

                    nowBuf = nowBuf.substr(chunkSize + 2);
                    logger.debug("[S <- C] Recv chunk: %d bytes", chunkSize);      
                }
            }
        }
        else if(packet.headers.find("Content-Length") != packet.headers.end())
        {
            // 非chunked模式，不过有Content-Length，检查长度是否完整
            string contentLength = packet.headers["Content-Length"];
            if(isNumeric(contentLength))
            {
                int receivedLen = packet.bodyData.size();
                int realLen = std::stoi(contentLength);
                logger.debug("[S <- C] Content-Length: %d, Received: %d", realLen, receivedLen);

                if(realLen > receivedLen)
                {
                    // content-length还有剩余，接收剩余的body
                    int remains = realLen - receivedLen;
                    logger.debug("[S <- C] More bytes remain to recv: %d", remains);
                    // 循环接收，每次将接收到的数据添到bodyData后面，直到remains为0
                    while(remains > 0)
                    {
                        memset(buf, 0, bufferSize);
                        int toRecv = bufferSize < remains ? bufferSize : remains;
                        int recvLen = recv(clientSocket, buf, toRecv , 0);
                        if(recvLen <= 0)
                        {
                            // 连接断开
                            logger.debug("[S <- C] Connection closed by client.");
                            return false;
                        }
                        packet.bodyData = packet.bodyData + buf;
                        remains -= recvLen;
                        logger.debug("[S <- C] Recved %d, remain %d", recvLen, remains);
                    }
                }
            }
        }
        // 接收完毕
        logger.debug("[S <- C] Finished recv from client");

        // 重写headers里的Host
        oldHostStr = packet.headers["Host"];
        packet.headers["Host"] = targetStr;
        logger.debug("[S <- C] Rewrite Host: %s -> %s", oldHostStr.c_str(), targetStr.c_str());

        // 调用插件
        PluginsCallClientRequest(&packet);

        // send
        logger.debug("[S <- C] Send request to server.");
        if(!packet.sendTo(serverSocket, true))
        {
            logger.error("[S <- C] Fail to send data to target server.");
            return false;
        }
        return true;
    }

    // 处理服务端响应
    bool processServerResponse()
    {
        logger.debug("[S -> C] Server response received.");

        // recv
        char buf[bufferSize] = {0};
        int recvLen = recv(serverSocket, buf, bufferSize, 0);

        if (recvLen <= 0)   // 连接断开
        {
            logger.debug("[S -> C] Connection closed by server.");
            return false;
        }
        logger.debug("[S -> C] Recv %d bytes from server.", recvLen);

        // 拆分响应数据
        HttpResponsePacket packet(string(buf, recvLen));
        logger.info("[S -> C] %s", packet.responseLine.c_str());

        // 如果响应过长，接收剩余的body
        if(packet.headers.find("Transfer-Encoding") != packet.headers.end())
        {
            // 给出了Transfer-Encoding，检查是否为chunked模式
            string encoding = packet.headers["Transfer-Encoding"];
            logger.debug("[S -> C] Transfer-Encoding: %s", encoding.c_str());
            if(encoding.find("chunked") != std::string::npos)
            {
                // chunked模式，分块接收
                logger.debug("[S -> C] Chunked mode detected.");
                string nowBuf = packet.bodyData;
                packet.bodyData = "";

                // 循环处理，每次先读取一个chunk size，再根据size读取对应长度的数据块，直到size为0表示结束
                while(true)
                {
                    // 如果此时数据都不足以表示一个chunk size，先recv到足够为止
                    while(nowBuf.find("\r\n") == std::string::npos)
                    {
                        logger.debug("[S -> C] More data needed to get a chunk size, just recv");
                        char buf[bufferSize] = {0};
                        int recvLen = recv(serverSocket, buf, bufferSize , 0);
                        if(recvLen <= 0)
                        {
                            // 连接断开
                            logger.debug("[S -> C] Connection closed by server.");
                            return false;
                        }
                        nowBuf.append(string(buf, recvLen));
                        logger.debug("[S -> C] Recv %d, now data size in buffer: %d", recvLen, nowBuf.size());
                    }

                    // 切分出头部的chunk size（16进制表示）
                    auto chunkSizeDataSplit = nowBuf.find("\r\n");
                    string chunkSizeStr = nowBuf.substr(0, chunkSizeDataSplit);
                    int chunkSize = std::stoi(chunkSizeStr, 0, 16);
                    nowBuf = nowBuf.substr(chunkSizeDataSplit + 2);

                    // 如果chunk size为0，表示传输结束
                    if(chunkSize == 0)
                    {
                        logger.debug("[S -> C] Chunk transfer finished.");
                        packet.bodyData.append("0\r\n\r\n");
                        break;
                    }
                    else
                        logger.debug("[S -> C] Next chunk size: %d", chunkSize);

                    // 如果此时缓冲区内剩下的数据长度不够chunk size + 2，继续接收
                    while((int)nowBuf.size() < chunkSize + 2)
                    {
                        logger.debug("[S -> C] More data needed to get next chunk, just recv");                        
                        char buf[bufferSize] = {0};
                        int recvLen = recv(serverSocket, buf, bufferSize , 0);
                        if(recvLen <= 0)
                        {
                            // 连接断开
                            logger.debug("[S -> C] Connection closed by server.");
                            return false;
                        }
                        nowBuf.append(string(buf, recvLen));
                        logger.debug("[S -> C] Recv %d, now data size in buffer: %d", recvLen, nowBuf.size());
                    }

                    // 切分出这一块chunk的数据，放入bodyData
                    // 跳过\r\n，继续下一次循环
                    packet.bodyData.append(chunkSizeStr + "\r\n");
                    packet.bodyData.append(nowBuf.substr(0, chunkSize));
                    packet.bodyData.append("\r\n");

                    nowBuf = nowBuf.substr(chunkSize + 2);
                    logger.debug("[S -> C] Recv chunk: %d bytes", chunkSize);
                }
            }
        }
        else if(packet.headers.find("Content-Length") != packet.headers.end())
        {
            // 非chunked模式，不过有Content-Length，检查长度是否完整
            string contentLength = packet.headers["Content-Length"];
            if(isNumeric(contentLength))
            {
                int receivedLen = packet.bodyData.size();
                int realLen = std::stoi(contentLength);
                logger.debug("[S -> C] Content-Length: %d, Received: %d", realLen, receivedLen);

                if(realLen > receivedLen)
                {
                    // content-length还有剩余，接收剩余的body
                    int remains = realLen - receivedLen;
                    logger.debug("[S -> C] More bytes remain to recv: %d", remains);
                    // 循环接收，每次将接收到的数据添到bodyData后面，直到remains为0
                    while(remains > 0)
                    {
                        memset(buf, 0, bufferSize);
                        int toRecv = bufferSize < remains ? bufferSize : remains;
                        int recvLen = recv(serverSocket, buf, toRecv , 0);
                        if(recvLen <= 0)
                        {
                            // 连接断开
                            logger.debug("[S -> C] Connection closed by server.");
                            return false;
                        }
                        packet.bodyData.append(string(buf, recvLen));
                        remains -= recvLen;
                        logger.debug("[S -> C] Recved %d, remain %d", recvLen, remains);
                    }
                }
            }
        }
        // 接收完毕
        logger.debug("[S -> C] Finished recv from server");

        // 处理301和302
        // （改写Location为之前存的oldHostStr）
        if(packet.code == 301 || packet.code == 302)
        {
            ReplaceStr(packet.headers["Location"], targetHost, oldHostStr);
            logger.debug("[S -> C] %d redirect, rewrite Location: %s -> %s", packet.code, targetHost, oldHostStr);
        }

        // 调用插件
        PluginsCallServerResponse(&packet);
        
        // send
        logger.debug("[S -> C] Send response to client.");
        if(!packet.sendTo(clientSocket, true))
        {
            logger.error("[S -> C] Fail to send data to client.");
            return false;
        }
        return true;
    }

    // 循环监听
    void mainLoop()
    {
        while (true)
        {
            fd_set readFds;
            FD_ZERO(&readFds);
            FD_SET(clientSocket, &readFds);
            FD_SET(serverSocket, &readFds);

            int maxFd = std::max(clientSocket, serverSocket) + 1;
            if (select(maxFd, &readFds, nullptr, nullptr, nullptr) < 0)
            {
                logger.error("Error in select().");
                break;
            }

            if (FD_ISSET(clientSocket, &readFds))
            {
                // client -> server
                if(!processClientRequest())
                    break;
            }

            if (FD_ISSET(serverSocket, &readFds))
            {
                // server -> client
                if(!processServerResponse())
                    break;
            }
        }
    }

    // 获取客户端地址字符串
    string getClientAddr()
    {
        return clientIp + ":" + std::to_string(clientPort);
    }
};

// 子线程传递参数用的
struct ClientThreadData
{
    int clientSocket;
    sockaddr_in clientAddr;
};

// 工作线程
void ClientThreadFunc(void *data)
{
    pthread_detach(pthread_self());
    ClientThreadData *threadData = (ClientThreadData *)data;
    int clientSocket = threadData->clientSocket;
    sockaddr_in clientAddr = threadData->clientAddr;

    // 创建logger
    Logger logger;
    logger.setLogLevel(logLevel);

    // 初始化worker
    ProxyClientWorker worker(clientSocket, clientAddr, logger);
    logger.setPrefix(worker.getClientAddr());

    // 连接到服务器
    logger.info("New connection received.");
    if(!worker.connectToServer(targetHost, targetPort))
    {
        logger.error("Failed to connect to target server.");
        return;
    }
    logger.info("Connected to server.");

    // 循环监听
    worker.mainLoop();

    // 结束
    delete threadData;
}

int main(int argc, char *argv[])
{
    // 读配置文件
    if(!ReadConfigFile())
        return 1;
    mainLogger.setLogLevel(logLevel);

    // 创建套接字监听
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);

    // 设置REUSERADDR，方便崩溃后快速重启
    int on = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // 填充地址，绑定
    sockaddr_in listenAddr;
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_port = htons(listenPort);
    listenAddr.sin_addr.s_addr = inet_addr(listenHost.c_str());
    if (bind(listenSocket, (sockaddr *)&listenAddr, sizeof(listenAddr)) < 0)
    {
        mainLogger.error("Failed to bind to port %d", listenPort);
        return 2;
    }
    if (listen(listenSocket, 5) < 0)
    {
        mainLogger.error("Failed to listen on port %d", listenPort);
        return 3;
    }

    // 创建线程池
    ThreadPool threadPool(minThread, maxThread, waitBeforeShrink);

    // 加载插件
    LoadPlugins(PLUGINS_DIR, CONFIG_FILE_PATH);

    mainLogger.info("Reverse proxy for %s:%d", targetHost.c_str(), targetPort);
    mainLogger.info("Proxy started at %s:%d", (listenHost == "0.0.0.0" ? "localhost" : listenHost.c_str()),
        listenPort);

    // 循环监听客户端
    while (true)
    {
        sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        int clientSocket = accept(listenSocket, (sockaddr *)&clientAddr, &addrLen);
        if (clientSocket < 0)
        {
            mainLogger.error("Fail to recv connection from client");
            continue;
        }
        ClientThreadData *threadData = new ClientThreadData{clientSocket, clientAddr};

        // 插入任务到线程池
        threadPool.addTask(ClientThreadFunc, threadData);
    }

    close(listenSocket);
    UnloadPlugins();
    return 0;
}