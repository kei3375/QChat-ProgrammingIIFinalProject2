#pragma once

#include <iostream>
#include <string>

std::string htmlentities(std::string html) {
    std::string result;
    result.reserve(html.size());

    for (unsigned char c : html) {
        switch (c) {
            case '&':  result += "&amp;";  break;
            case '<':  result += "&lt;";   break;
            case '>':  result += "&gt;";   break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&#39;";  break;
            default:
                if (c > 127) {
                    result += "&#";
                    result += std::to_string(c);
                    result += ';';
                } else {
                    result += c;
                }
        }
    }

    return result;
}