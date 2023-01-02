#ifndef UTILS_BY_YQ
#define UTILS_BY_YQ

#include <string>
#include <vector>
#include <algorithm>

// 将字符串根据指定pattern切分，放到vector里面
std::vector<std::string> SplitStrWithPattern(const std::string& str, const std::string& pattern);

// string替换
std::string& ReplaceStr(std::string& str, const std::string& oldValue, const std::string& newValue) ;

// 判断string是否是数字
bool isNumeric(const std::string &str);

// 判断字符串是否由start开头
bool StartsWith(const std::string& str, const std::string& start);

// 判断字符串是否由end结尾
bool EndsWith(const std::string& str, const std::string& end);

#endif