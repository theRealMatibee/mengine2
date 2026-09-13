#pragma once

#include <glm/glm.hpp>
#include <vector>

inline const float pointPointDistance( const glm::vec2& a, const glm::vec2& b ) {
    return glm::length( a - b );
}

const float pointLineDistance( const glm::vec2& point, const glm::vec2& lineStart, const glm::vec2& lineEnd );

const bool lineLineIntersect( const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, const glm::vec2& p4, glm::vec2& intersection );



class collisionRect {
    public:
        collisionRect(){};
        collisionRect( const glm::vec2 topLeft, const glm::vec2 bottomRight ) : m_topLeft( topLeft ), m_bottomRight( bottomRight ), m_position( topLeft ) {};
        void move( const glm::vec2& delta ) {
            m_topLeft += delta;
            m_bottomRight += delta;
            m_position += delta;
        };
        collisionRect* clone() const {
            return new collisionRect( m_topLeft, m_bottomRight );
        };
        void move( const float x, const float y ) {
            move( glm::vec2( x, y ) );
        };
        void moveTo( const glm::vec2& pos ) {
            glm::vec2 dist = pos - m_topLeft;
            move( dist );
        };
        void moveTo( const float x, const float y ) {
            moveTo( glm::vec2( x, y ) );
        };
        void scale( const float s ){
            m_topLeft *= s;
            m_bottomRight *= s;
            m_position = m_topLeft;
        };
        const bool pointInside( const glm::vec2& point ) const {
            return ( point.x >= m_topLeft.x && point.x <= m_bottomRight.x &&
                     point.y >= m_topLeft.y && point.y <= m_bottomRight.y );
        };
        const bool pointInside( const float x, const float y ) const {
            return pointInside( glm::vec2( x, y ) );
        };
        const bool rectRectIntersect( const collisionRect& other ) const {
            return !( m_bottomRight.x < other.m_topLeft.x || m_topLeft.x > other.m_bottomRight.x ||
                      m_bottomRight.y < other.m_topLeft.y || m_topLeft.y > other.m_bottomRight.y );
        };
        const glm::vec2 midPoint() const {
            return glm::vec2( (m_topLeft.x + m_bottomRight.x) / 2.0f, 
                                (m_topLeft.y + m_bottomRight.y) / 2.0f );
        };
        const bool straddlesX( const float X ) const {
            return m_topLeft.x < X && m_bottomRight.x > X;
        };
        const bool straddlesY( const float Y ) const {
            return m_topLeft.y < Y && m_bottomRight.y > Y;
        };
        inline const float minX() const { return m_topLeft.x; };
        inline const float maxX() const { return m_bottomRight.x; };
        inline const float minY() const { return m_topLeft.y; };
        inline const float maxY() const { return m_bottomRight.y; };
        const bool overlaps( const collisionRect &r ) const { 
            return !( m_bottomRight.x < r.m_topLeft.x || m_topLeft.x > r.m_bottomRight.x ||
                      m_bottomRight.y < r.m_topLeft.y || m_topLeft.y > r.m_bottomRight.y );
        };
        const bool contains( const collisionRect &r ) const {
            return m_topLeft.x <= r.m_topLeft.x && m_bottomRight.x >= r.m_bottomRight.x &&
                   m_topLeft.y <= r.m_topLeft.y && m_bottomRight.y >= r.m_bottomRight.y;
        };


        inline const glm::vec2& position() const { return m_position; };
        inline const glm::vec2& topLeft() const { return m_topLeft; };
        inline const glm::vec2& bottomRight() const { return m_bottomRight; };

    protected:
        glm::vec2 m_topLeft;
        glm::vec2 m_bottomRight;
        glm::vec2 m_position;
};


class collisionCircle : public collisionRect {
    public:
        collisionCircle( const glm::vec2 center, const float radius ) : collisionRect( center - glm::vec2( radius, radius ), center + glm::vec2( radius, radius ) ),  m_radius( radius ), m_center( center ) {};
        const float radius() const { return m_radius; };
        const glm::vec2& center() const { return m_center; };

        void move( const glm::vec2& delta ) {
            collisionRect::move( delta );
            m_center += delta;
        };
        void move( const float x, const float y ) {
            move( glm::vec2( x, y ) );
        };
        void moveTo( const glm::vec2& pos ) {
            glm::vec2 delta = pos - m_center;
            move( delta );
        };
        void moveTo( const float x, const float y ) {
            moveTo( glm::vec2( x, y ) );
        };
        const bool pointInside( const glm::vec2& point ) const {
            return pointPointDistance( point, m_center ) <= m_radius;
        };
        const bool pointInside( const float x, const float y ) const {
            return pointInside( glm::vec2( x, y ) );
        };
        const bool circleCircleIntersect( const collisionCircle& other ) const {
            return pointPointDistance( m_center, other.center() ) <= ( m_radius + other.radius() );
        };
        const bool rectCircleIntersect( const collisionRect& rect ) const {
            if ( !rect.rectRectIntersect( *this ) ) {
                return false;
            }
            return pointLineDistance( m_center, glm::vec2( rect.bottomRight().x, rect.topLeft().y ), glm::vec2( rect.topLeft().x, rect.bottomRight().y ) ) <= m_radius ||
                   pointLineDistance( m_center, glm::vec2( rect.topLeft().x, rect.bottomRight().y ), glm::vec2( rect.bottomRight().x, rect.topLeft().y ) ) <= m_radius ||
                   pointLineDistance( m_center, glm::vec2( rect.topLeft().x, rect.topLeft().y ), glm::vec2( rect.topLeft().x, rect.bottomRight().y ) ) <= m_radius ||
                   pointLineDistance( m_center, glm::vec2( rect.bottomRight().x, rect.topLeft().y ), glm::vec2( rect.bottomRight().x, rect.bottomRight().y ) ) <= m_radius;
        };

    private:
        glm::vec2 m_center;
        float m_radius;    
};

class collisionPolygon : public collisionRect {
    public:
        collisionPolygon( const std::vector<glm::vec2>& points ) : m_points( points ) {
            // Calculate bounding box
            if ( points.empty() ) {
                m_topLeft = glm::vec2( 0.0f, 0.0f );
                m_bottomRight = glm::vec2( 0.0f, 0.0f );
                m_position = m_topLeft;
                return;
            }
            float minX = points[0].x;
            float maxX = points[0].x;
            float minY = points[0].y;
            float maxY = points[0].y;
            for ( const auto& point : points ) {
                if ( point.x < minX ) minX = point.x;
                if ( point.x > maxX ) maxX = point.x;
                if ( point.y < minY ) minY = point.y;
                if ( point.y > maxY ) maxY = point.y;
            }
            m_topLeft = glm::vec2( minX, minY );
            m_bottomRight = glm::vec2( maxX, maxY );
            m_position = m_topLeft;
        };

        const std::vector<glm::vec2>& points() const { return m_points; };

        const bool pointInside( const glm::vec2& point ) const {
            if ( !collisionRect::pointInside( point ) ) {
                return false;
            }
            int intersections = 0;
            size_t numPoints = m_points.size();
            for ( size_t i = 0; i < numPoints; ++i ) {
                const glm::vec2& p1 = m_points[i];
                const glm::vec2& p2 = m_points[(i + 1) % numPoints];
                if ( (p1.y > point.y) != (p2.y > point.y) && 
                     (point.x < (p2.x - p1.x) * (point.y - p1.y) / (p2.y - p1.y) + p1.x) ) {
                    intersections++;
                }
            }
            return (intersections % 2) == 1;
        };
        const bool circlePolygonIntersect( const collisionCircle& circle ) const {
            if ( !collisionRect::rectRectIntersect( circle ) ) {
                return false;
            }
            size_t numPoints = m_points.size(); 
            for ( size_t i = 0; i < numPoints; ++i ) {
                const glm::vec2& p1 = m_points[i];
                const glm::vec2& p2 = m_points[(i + 1) % numPoints];
                if ( pointLineDistance( circle.center(), p1, p2 ) <= circle.radius() ) {
                    return true;
                }
            }
            return false;
        };
        const bool linePolygonIntersect( const glm::vec2& lineStart, const glm::vec2& lineEnd ) const {
            size_t numPoints = m_points.size(); 
            for ( size_t i = 0; i < numPoints; ++i ) {
                const glm::vec2& p1 = m_points[i];
                const glm::vec2& p2 = m_points[(i + 1) % numPoints];
                glm::vec2 intersection;
                if ( lineLineIntersect( lineStart, lineEnd, p1, p2, intersection ) ) {
                    return true;
                }
            }
            return false;
        };
        const bool rectPolygonIntersect( const collisionRect& rect ) const {
            if ( !collisionRect::rectRectIntersect( rect ) ) {
                return false;
            }
            if ( pointInside( rect.topLeft() ) || 
                 pointInside( rect.bottomRight() ) || 
                 pointInside( glm::vec2( rect.topLeft().x, rect.bottomRight().y ) ) || 
                 pointInside( glm::vec2( rect.bottomRight().x, rect.topLeft().y ) ) ) {
                return true;
            }
            if ( linePolygonIntersect( glm::vec2( rect.topLeft().x, rect.bottomRight().y ), rect.bottomRight() ) || 
                 linePolygonIntersect( glm::vec2( rect.topLeft().x, rect.bottomRight().y ), glm::vec2( rect.bottomRight().x, rect.topLeft().y ) ) ||
                 linePolygonIntersect( glm::vec2( rect.topLeft().x, rect.topLeft().y ), glm::vec2( rect.topLeft().x, rect.bottomRight().y ) ) || 
                 linePolygonIntersect( glm::vec2( rect.bottomRight().x, rect.topLeft().y ), rect.bottomRight() ) ) {
                return true;
            }
            return false;
        };


    private:
        std::vector<glm::vec2> m_points;
};

template <typename T> class collRectQuadTree {
    private:
    class QuadNode {
        public:
            QuadNode( const collisionRect& bounds, std::vector<T*> objects ) : m_bounds( bounds ), m_children{ nullptr, nullptr, nullptr, nullptr } { 
                float boundsX = m_bounds.maxX() - m_bounds.minX();
                float boundsY = m_bounds.maxY() - m_bounds.minY();
                float boundsMin = std::min( boundsX, boundsY );
                if ( (objects.size() <= 6) || (boundsMin < 10.0f) ) { // stop subdividing when the object count is low or the bounds are small
                    for ( const auto &obj : objects )
                        m_objects.push_back( obj );
                    return;
                }
                glm::vec2 mid = m_bounds.midPoint();
                std::vector<T*> topleft;
                std::vector<T*> btmright;
                std::vector<T*> topright;
                std::vector<T*> btmleft;
                    
                for ( const auto &obj : objects ) {
                    if ( obj->maxX() <= mid.x && obj->maxY() <= mid.y ) {
                        topleft.push_back( obj );
                    }else if ( obj->minX() >= mid.x && obj->maxY() <= mid.y ){
                        topright.push_back( obj );
                    }else if ( obj->maxX() <= mid.x && obj->minY() >= mid.y ){
                        btmleft.push_back( obj );
                    }else if ( obj->minX() >= mid.x && obj->minY() >= mid.y ){
                        btmright.push_back( obj );
                    }else {
                        m_objects.push_back( obj );
                    }
                }

                if ( topleft.size() > 0 ) {
                    collisionRect bounds( m_bounds.topLeft(), mid );
                    m_children[0] = new QuadNode( bounds, topleft );
                }

                if ( topright.size() > 0 ) {
                    collisionRect bounds( glm::vec2( mid.x, m_bounds.minY() ),
                                            glm::vec2( m_bounds.maxX(), mid.y ));
                    m_children[1] = new QuadNode( bounds, topright );
                }

                if ( btmright.size() > 0 ) {
                    collisionRect bounds( mid, m_bounds.bottomRight() );
                    m_children[2] = new QuadNode( bounds, btmright );
                }

                if ( btmleft.size() > 0 ) {
                    collisionRect bounds( glm::vec2( m_bounds.minX(), mid.y),
                                            glm::vec2( mid.x, m_bounds.maxY() ) );
                    m_children[3] = new QuadNode( bounds, btmleft );
                }
        };
        ~QuadNode() {
            for ( int i = 0; i < 4; ++i ) {
                delete m_children[i];
            }
        };
        void getObjects( const collisionRect& area, std::vector<T*>& results ) const {
            if ( area.overlaps( m_bounds) ) {
                for ( const auto &obj : m_objects ) {
                        results.push_back( obj );
                }
                for ( int i = 0; i < 4; ++i ) {
                    if ( m_children[i] != nullptr )
                        m_children[i]->getObjects( area, results );
                }
            }
        };
        void getDebugLines( std::vector<T*>& lines ) {
            lines.push_back( &m_bounds );
            for ( int i = 0; i < 4; ++i ) {
                if ( m_children[i] != nullptr )
                    m_children[i]->getDebugLines( lines );
            }
        };

        private:
            collisionRect m_bounds;
            QuadNode* m_children[4];
            std::vector<T*> m_objects;  // objects that fall within this node's bounds, but not fully contained in any child node
    };
    public:
        collRectQuadTree( std::vector<T*> &objects ) : m_objects( objects ), m_root(nullptr) {
            collisionRect bounds;
            if ( objects.empty() ) {
                bounds = collisionRect( glm::vec2( 0.0f, 0.0f ), glm::vec2( 0.0f, 0.0f ) );
                return;
            } else {
                float minX = objects[0]->topLeft().x;
                float maxX = objects[0]->bottomRight().x;
                float minY = objects[0]->topLeft().y;
                float maxY = objects[0]->bottomRight().y;
                for ( const auto& obj : objects ) {
                    if ( obj->topLeft().x < minX ) minX = obj->topLeft().x;
                    if ( obj->bottomRight().x > maxX ) maxX = obj->bottomRight().x;
                    if ( obj->topLeft().y < minY ) minY = obj->topLeft().y;
                    if ( obj->bottomRight().y > maxY ) maxY = obj->bottomRight().y;
                }
                bounds = collisionRect( glm::vec2( minX, minY ), glm::vec2( maxX, maxY ) );
                m_root = new QuadNode( bounds, objects );
            }
        };
        ~collRectQuadTree(){
            delete m_root;
        };
        void getObjects( const collisionRect& area, std::vector<T*>& results ) const {
            if ( m_root != nullptr )
                m_root->getObjects( area, results );
        };
        void getDebugLines( std::vector<T*>& lines  ) {
            if ( m_root != nullptr ) {
                m_root->getDebugLines( lines );
            }
        }
        
    private:
        QuadNode* m_root;
        std::vector<T*> m_objects;
};

