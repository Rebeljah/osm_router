#pragma once

#include <vector>
#include <string>

using std::string;
using std::vector;

// https://gist.github.com/tcmug/9712f9192571c5fe65c362e6e86266f8
vector<string> splitString(string s, string delim)
{
    vector<string> result;
    size_t from = 0, to = 0;
    while (string::npos != (to = s.find(delim, from)))
    {
        result.push_back(s.substr(from, to - from));
        from = to + delim.length();
    }
    result.push_back(s.substr(from, to));
    return result;
}