#pragma once

#include <string>
#include <sstream>

namespace server {
namespace utils {

std::string formatJson(std::string_view json) {
    std::ostringstream result;
    int level = 0;
    for (std::string::size_type index = 0; index < json.size(); index++) {
        char c = json[index];
        switch (c) {
            case '{':
            case '[':
                result << c << "\n";
                level++;
                result << std::string(4 * level, ' ');
                break;
            case ',':
                result << c << "\n";
                result << std::string(4 * level, ' ');
                break;
            case '}':
            case ']':
                result << "\n";
                level--;
                result << std::string(4 * level, ' ');
                result << c;
                break;
            default:
                result << c;
                break;
        }
    }
    return result.str();
}

} // namespace utils
} // namespace server