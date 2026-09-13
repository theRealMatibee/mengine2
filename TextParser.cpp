#include "TextParser.h"

#include <cctype>
#include <sstream>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

int TextParser::tokeniseLine(const std::string& line, std::map<std::string, std::string>& lineMap) {
    int tokenCount = 0;
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;

    for (char ch : line) {
        if (ch == '"') {
            inQuotes = !inQuotes;
            current.push_back(ch);
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch)) && !inQuotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    for (const std::string &token : tokens) {
        const size_t equalsPos = token.find('=');
        if (equalsPos == std::string::npos) {
            continue;
        }

        std::string key = token.substr(0, equalsPos);
        std::string value = token.substr(equalsPos + 1);

        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        lineMap[key] = value;
        ++tokenCount;
    }

    return tokenCount;
}

bool TextParser::parseBracketedFloatPair(const std::string& s, float& outX, float& outY) {
    const size_t begin = s.find('(');
    const size_t end = s.find(')', begin == std::string::npos ? 0 : begin);
    if (begin == std::string::npos || end == std::string::npos || begin + 1 >= end) {
        return false;
    }

    std::string inner = s.substr(begin + 1, end - begin - 1);
    const size_t comma = inner.find(',');
    if (comma == std::string::npos) {
        return false;
    }

    auto trim = [](std::string& str) {
        size_t start = 0;
        while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
            ++start;
        }

        size_t finish = str.size();
        while (finish > start && std::isspace(static_cast<unsigned char>(str[finish - 1]))) {
            --finish;
        }

        str = str.substr(start, finish - start);
    };

    std::string xStr = inner.substr(0, comma);
    std::string yStr = inner.substr(comma + 1);
    trim(xStr);
    trim(yStr);

    try {
        outX = std::stof(xStr);
        outY = std::stof(yStr);
    } catch (...) {
        return false;
    }

    return true;
}

bool TextParser::seekVectorPosFromFile(std::ifstream& stream, const std::string& ident, float& x, float& y) {
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind(ident, 0) == 0) {
            return parseBracketedFloatPair(line, x, y);
        }
    }

    return false;
}

std::string TextParser::trimCopy(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t finish = value.size();
    while (finish > start && std::isspace(static_cast<unsigned char>(value[finish - 1]))) {
        --finish;
    }

    return value.substr(start, finish - start);
}

bool TextParser::startsWithKey(const std::string& line, const std::string& key) {
    const std::string trimmed = trimCopy(line);
    return trimmed.rfind(key, 0) == 0;
}

std::string TextParser::assignmentValue(const std::string& line) {
    const size_t equalsPos = line.find('=');
    if (equalsPos == std::string::npos) {
        return std::string();
    }

    return trimCopy(line.substr(equalsPos + 1));
}

bool TextParser::parseAssignedInt(const std::string& line, int& outValue) {
    const std::string value = assignmentValue(line);
    if (value.empty()) {
        return false;
    }

    try {
        outValue = std::stoi(value);
    } catch (...) {
        return false;
    }

    return true;
}

bool TextParser::parseAssignedFloat(const std::string& line, float& outValue) {
    const std::string value = assignmentValue(line);
    if (value.empty()) {
        return false;
    }

    try {
        outValue = std::stof(value);
    } catch (...) {
        return false;
    }

    return true;
}

bool TextParser::parseAssignedBool(const std::string& line, bool& outValue) {
    const std::string value = assignmentValue(line);
    if (value == "true") {
        outValue = true;
        return true;
    }
    if (value == "false") {
        outValue = false;
        return true;
    }

    return false;
}

bool TextParser::parseResourceId(const std::string& line, int& outId) {
    const size_t begin = line.find('(');
    const size_t end = line.find(')', begin == std::string::npos ? 0 : begin);
    if (begin == std::string::npos || end == std::string::npos || begin + 1 >= end) {
        return false;
    }

    const std::string inner = trimCopy(line.substr(begin + 1, end - begin - 1));
    try {
        outId = std::stoi(inner);
    } catch (...) {
        return false;
    }

    return true;
}

bool TextParser::parseAssignedVec2(const std::string& line, glm::vec2& outVec) {
    float x = 0.0f;
    float y = 0.0f;
    if (!parseBracketedFloatPair(line, x, y)) {
        return false;
    }

    outVec = glm::vec2(x, y);
    return true;
}

bool TextParser::parseAssignedColorRGBA(const std::string& line, glm::vec4& outColor) {
    const std::string value = assignmentValue(line);
    if (value.empty()) {
        return false;
    }

    const size_t colorPos = value.find("Color");
    const size_t begin = value.find('(', colorPos == std::string::npos ? 0 : colorPos);
    const size_t end = value.find(')', begin == std::string::npos ? 0 : begin);
    if (begin == std::string::npos || end == std::string::npos || begin + 1 >= end) {
        return false;
    }

    std::string inner = value.substr(begin + 1, end - begin - 1);
    std::stringstream ss(inner);
    std::string token;
    float components[4] = {};
    int count = 0;

    while (std::getline(ss, token, ',') && count < 4) {
        token = trimCopy(token);
        if (token.empty()) {
            return false;
        }

        try {
            components[count] = std::stof(token);
        } catch (...) {
            return false;
        }

        count += 1;
    }

    if (count != 4) {
        return false;
    }

    outColor = glm::vec4(components[0], components[1], components[2], components[3]);
    return true;
}

std::string TextParser::parentDirectory(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return std::string();
    }

    return path.substr(0, slash);
}

std::string TextParser::trimQuotes(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}
