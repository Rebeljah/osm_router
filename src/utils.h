#pragma once

#include <vector>
#include <string>

using std::vector;
using std::string;

// code adapted from: https://www.geeksforgeeks.org/unordered-set-of-pairs-in-c-with-examples/#
/*
This type can be passed into types that require hashing pairs such as unordered maps or unordered sets
example usage: unordered_set<pair<string, string>, PairHasher<string>> foo;
*/
template <typename T>
struct PairHasher
{
    size_t operator()(const std::pair<T, T> &pair) const
    {
        return std::hash<T>{}(pair.first) ^ std::hash<T>{}(pair.second);
    }
};

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