#include "Utils.h"
#include <string>
#include <vector>
#include <algorithm>

// 将字符串根据指定pattern切分，放到vector里面
std::vector<std::string> SplitStrWithPattern(const std::string& str, const std::string& pattern)
{
    std::vector<std::string> resVec;
    if (str.empty())
        return resVec;

    std::string strs = str + pattern;

    size_t pos = strs.find(pattern);
    size_t size = strs.size();

    while (pos != std::string::npos) {
        std::string x = strs.substr(0, pos);
        resVec.push_back(x);
        strs = strs.substr(pos + pattern.size(), size);
        pos = strs.find(pattern);
    }

    return resVec;
}

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

// 判断string是否是数字
bool isNumeric(const std::string &str)
{
    return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

// 判断字符串是否由start开头
bool StartsWith(const std::string& str, const std::string& start)
{
    size_t srcLen = str.size();
    size_t startLen = start.size();
    if (srcLen >= startLen) {
        std::string temp = str.substr(0, startLen);
        if (temp == start)
            return true;
    }
    return false;
}

// 判断字符串是否由end结尾
bool EndsWith(const std::string& str, const std::string& end)
{
    size_t srcLen = str.size();
    size_t endLen = end.size();
    if (srcLen >= endLen) {
        std::string temp = str.substr(srcLen - endLen, endLen);
        if (temp == end)
            return true;
    }
    return false;
}