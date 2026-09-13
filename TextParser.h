#pragma once

#include <fstream>
#include <map>
#include <string>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

class TextParser {
public:
    static int tokeniseLine(const std::string& line, std::map<std::string, std::string>& lineMap);
    static bool parseBracketedFloatPair(const std::string& s, float& outX, float& outY);
    static bool seekVectorPosFromFile(std::ifstream& stream, const std::string& ident, float& x, float& y);
    
    static std::string trimCopy(const std::string& value);
    static bool startsWithKey(const std::string& line, const std::string& key);
    static std::string assignmentValue(const std::string& line);
    static bool parseAssignedInt(const std::string& line, int& outValue);
    static bool parseAssignedFloat(const std::string& line, float& outValue);
    static bool parseAssignedBool(const std::string& line, bool& outValue);
    static bool parseResourceId(const std::string& line, int& outId);
    static bool parseAssignedVec2(const std::string& line, glm::vec2& outVec);
    static bool parseAssignedColorRGBA(const std::string& line, glm::vec4& outColor);
    static std::string parentDirectory(const std::string& path);
    static std::string trimQuotes(const std::string& value);
};
