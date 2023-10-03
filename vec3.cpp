#include <cmath>
#include <vector>
#include <string>

using namespace std;

class Vec3 {
    public:
        Vec3(float _x = 0., float _y = 0., float _z = 0.) {
            x = _x; y = _y; z = _z;
        }

        void set(float _x, float _y, float _z) {
            x = _x; y = _y; z = _z;
        }

        void normalize() {
            int l = len();
            x /= l;
            y /= l;
            z /= l;
        }

        float len() {
            return sqrtf(x*x + y*y + z*z);
        }

        float len2() {
            return x*x + y*y + z*z;
        }

        float dot(Vec3 other) {
            return x*other.x + y*other.y + z*other.z;
        }

        Vec3 cross(Vec3 other) {
            return Vec3(y*other.z - z*other.y, z*other.x - x*other.z, x*other.y - y*other.x);
        }

        string str() {
            return "[" + to_string(x) + " " + to_string(y) + " " + to_string(z) + "]";
        }

        Vec3 operator+(Vec3 other) {
            return Vec3(x+other.x, y+other.y, z+other.z);
        }

        void operator+=(Vec3 other) {
            x += other.x; y += other.y; z += other.z;
        }

        Vec3 operator-(Vec3 other) {
            return Vec3(x-other.x, y-other.y, z-other.z);
        }

        void operator-=(Vec3 other) {
            x -= other.x; y -= other.y; z -= other.z;
        }

        Vec3 operator*(float a) {
            return Vec3(x*a, y*a, z*a);
        }

        void operator*=(float a) {
            x *= a; y *= a; z *= a;
        }

        Vec3 operator/(float a) {
            return Vec3(x/a, y/a, z/a);
        }

        void operator/=(float a) {
            x /= a; y /= a; z /= a;
        }

        bool operator==(Vec3 other) {
            return x == other.x && y == other.y && z == other.z;
        }

        float x, y, z;
};
