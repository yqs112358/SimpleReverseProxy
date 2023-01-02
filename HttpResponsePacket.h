#ifndef HTTP_RESPONSE_PACKET_BY_YQ
#define HTTP_RESPONSE_PACKET_BY_YQ

#include <string>
#include <map>
#include <sstream>
#include <iostream>
#include <sys/socket.h>
#include "Utils.h"

/* http响应
 *
 * HTTP/1.1 200 OK\r\n
 * Server: Apache Tomcat/5.0.12\r\n
 * Date: Mon,6Oct2003 13:23:42 GMT\r\n
 * ...\r\n
 * Content-Length: 112\r\n\r\n
 * (data)
*/
struct HttpResponsePacket
{
    std::string rawData;
    std::string responseLine;
    std::string version;
    int code;
    std::string message;
    std::map<std::string, std::string> headers;
    std::string bodyData;
    std::string host;
    int port;

    HttpResponsePacket(const std::string &data, bool autoParse = true)
        :rawData(data)
    {
        if (autoParse)
            parse();
    }

    // 将rawData内容拆到各个字段
    void parse()
    {
        auto resplineHeaderSplit = rawData.find("\r\n");         // 第一个\r\n切分respline和header
        auto headerDataSplit = rawData.find("\r\n\r\n");        // 第一个\r\n\r\n切分header和body

        // 切分响应行
        responseLine = rawData.substr(0, resplineHeaderSplit);
        auto res = SplitStrWithPattern(responseLine, " ");       // 根据空格切响应行
        this->version = res[0];
        this->code = std::stoi(res[1]);
        this->message = res[2];

        // 切分响应头
        std::string requestHeader = rawData.substr(resplineHeaderSplit + 2, headerDataSplit - resplineHeaderSplit - 2);
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
        // 构造respline
        rawDataBuilder << version << " " << code << " " << message << "\r\n";
        // 构造header
        for(auto &item : headers)
            rawDataBuilder << item.first << ": " << item.second << "\r\n";
        // 构造body
        rawDataBuilder << "\r\n" << bodyData;
        this->rawData = rawDataBuilder.str();
    }

    // 发送响应
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