#include "miscfunctions.h"

#include <sstream>

void SplitString(const std::string& str, const std::string& delim, std::vector<std::string>& output)
{
    std::string::size_type offset = 0;
    std::string::size_type di = str.find(delim, offset);

    while (di != std::string::npos)
    {
        output.push_back(str.substr(offset, di - offset));
        offset += di - offset + delim.length();
        di = str.find(delim, offset);
    }

    output.push_back(str.substr(offset));
}

void SplitString(const std::string& str, const std::string& delim, std::vector<int64_t>& output)
{
    std::vector<std::string> ostr;
    SplitString(str, delim, ostr);
    output.clear();

    for (std::vector<std::string>::const_iterator i = ostr.begin(); i != ostr.end(); i++)
    {
        int64_t val = -1;
        std::istringstream istr((*i));
        if (istr >> val)
        {
            output.push_back(val);
        }
    }
}
