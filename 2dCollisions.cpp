#include "2dCollisions.hpp"

const float pointLineDistance( const glm::vec2& point, const glm::vec2& lineStart, const glm::vec2& lineEnd ) {
    glm::vec2 lineDir = lineEnd - lineStart;
    float lineLength = glm::length( lineDir );
    if ( lineLength == 0.0f ) {
        return pointPointDistance( point, lineStart );
    }
    glm::vec2 lineDirNorm = lineDir / lineLength;
    float t = glm::dot( point - lineStart, lineDirNorm );
    t = glm::clamp( t, 0.0f, lineLength );
    glm::vec2 closestPoint = lineStart + t * lineDirNorm;
    return pointPointDistance( point, closestPoint );
};

const bool lineLineIntersect( const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, const glm::vec2& p4, glm::vec2& intersection ) {
    float denom = (p2.x - p1.x) * (p4.y - p3.y) - (p2.y - p1.y) * (p4.x - p3.x);
    if (denom == 0.0f) {
        return false; // Lines are parallel
    }
    float ua = ((p4.x - p3.x) * (p1.y - p3.y) - (p4.y - p3.y) * (p1.x - p3.x)) / denom;
    float ub = ((p2.x - p1.x) * (p1.y - p3.y) - (p2.y - p1.y) * (p1.x - p3.x)) / denom;
    if (ua >= 0.0f && ua <= 1.0f && ub >= 0.0f && ub <= 1.0f) {
        intersection = p1 + ua * (p2 - p1);
        return true;
    }
    return false;
};

