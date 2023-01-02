#ifndef HTTP_REQUEST_PACKET_BY_YQ
#define HTTP_REQUEST_PACKET_BY_YQ

#include <string>
#include <map>
#include <sstream>
#include <iostream>
#include <sys/socket.h>
#include "Utils.h"

/* http请求
 *
 * GET http://comp3310.ddns.net/index.html HTTP/1.1\r\n
 * Host: comp3310.ddns.net\r\n
 * Proxy-Connection: keep-alive\r\n
 * ...\r\n
 * Cache-Control: max-age=0\r\n\r\n
 * (data)
*/
struct HttpRequestPacket
{
    std::string rawData;
    std::string requestLine;
    std::string method;
    std::string uri;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string bodyData;
    std::string host;
    int port;

    HttpRequestPacket(const std::string &data, bool autoParse = true)
        :rawData(data)
    {
        if (autoParse)
            parse();
    }

    // 将rawData内容拆到各个字段
    void parse()
    {
        auto reqlineHeaderSplit = rawData.find("\r\n");         // 第一个\r\n切分reqline和header
        auto headerDataSplit = rawData.find("\r\n\r\n");        // 第一个\r\n\r\n切分header和body

        // 切分请求行
        requestLine = rawData.substr(0, reqlineHeaderSplit);
        auto res = SplitStrWithPattern(requestLine, " ");       // 根据空格切请求行
        this->method = res[0];
        this->uri = res[1];
        this->version = res[2];

        // 切分请求头
        std::string requestHeader = rawData.substr(reqlineHeaderSplit + 2, headerDataSplit - reqlineHeaderSplit - 2);
        this->headers.clear();

        std::istringstream sin(requestHeader);
        std::string headerLine;
        while (getline(sin, headerLine))
        {
            if(headerLine.empty())
                break;
            // 去掉换行符
            if(headerLine.back() == '\n')
                headerLine.pop_back();
            if(headerLine.back() == '\r')
                headerLine.pop_back();
            // 切分行
            auto res = SplitStrWithPattern(headerLine, ": ");
            std::string key = res[0];
            std::string value = res[1];
            this->headers[key] = value;
        }

        // 如果Host含有端口号，切开，否则默认80
        std::string host = this->headers["Host"];
        size_t colonPos = host.rfind(':');
        if (colonPos != std::string::npos)
        {
            this->host = host.substr(0, colonPos);
            this->port = std::stoi(host.substr(colonPos + 1));
        }
        else
        {
            this->host = host;
            this->port = 80;
        }
        // 剩余的为body
        this->bodyData = rawData.substr(headerDataSplit + 4);
    }

    // 将各字段重新拼接成rawData
    void updateRawData()
    {
        std::ostringstream rawDataBuilder;
        // 构造reqline
        rawDataBuilder << method << " " << uri << " " << version << "\r\n";
        // 构造header
        for(auto &item : headers)
            rawDataBuilder << item.first << ": " << item.second << "\r\n";
        // 构造body
        rawDataBuilder << "\r\n" << bodyData;
        this->rawData = rawDataBuilder.str();
    }

    // 发送请求
    bool sendTo(int targetSocket, bool isModified = true)
    {
        if(isModified)
            updateRawData();
        
        // 如果内容较长一次没发完，分几次发
        int bytesSent = 0;
        int sendLen = (int)rawData.size();
        const char *data = rawData.c_str();
        while (bytesSent < sendLen)
        {
            int bytesWritten = send(targetSocket, data + bytesSent, sendLen - bytesSent, 0);
            if (bytesWritten <= 0)      // 出现问题
                return false;
            bytesSent += bytesWritten;
        }
        return true;
    }
};

#endif