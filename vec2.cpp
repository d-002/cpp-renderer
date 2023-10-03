#include <cmath>
#include <vector>

using namespace std;

class Vec2 {
    public:
        Vec2(float _x = 0., float _y = 0.) {
            x = _x; y = _y;
        }

        void set(float _x, float _y) {
            x = _x; y = _y;
        }

        void normalize() {
            int l = len();
            x /= l;
            y /= l;
        }

        vector<int> coord() {
            // returns int coordinates corresponding to x and y
            _coord = {(int)x, (int)y};
            return _coord;
        }

        float len() {
            return sqrtf(x*x + y*y);
        }

        float len2() {
            return x*x + y*y;
        }

        string str() {
            return "[" + to_string(x) + " " + to_string(y) + "]";
        }

        float angle() {
            return atan2(y, x);
        }

        void rotate(float angle) {
            float length = sqrtf(x*x + y*y);
            angle += atan2(y, x);
            x = cos(angle)*length;
            y = sin(angle)*length;
        }

        Vec2 operator+(Vec2 other) {
            return Vec2(x+other.x, y+other.y);
        }

        void operator+=(Vec2 other) {
            x += other.x; y += other.y;
        }

        Vec2 operator-(Vec2 other) {
            return Vec2(x-other.x, y-other.y);
        }

        void operator-=(Vec2 other) {
            x -= other.x; y -= other.y;
        }

        Vec2 operator*(float a) {
            return Vec2(x*a, y*a);
        }

        void operator*=(float a) {
            x *= a; y *= a;
        }

        Vec2 operator/(float a) {
            return Vec2(x/a, y/a);
        }

        void operator/=(float a) {
            x /= a; y /= a;
        }

        bool operator==(Vec2 other) {
            return x == other.x && y == other.y;
        }

        float x, y;
        vector<int> _coord;
};
