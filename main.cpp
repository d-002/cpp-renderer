// TODO
// sort cubes by distance
// cache rotated pos
// use arrays instead of vectors
// bug disappear in the distance
// matrix transformations
/// fix bug no rotation weird z change
/// fix render distance

#include <iostream>
#include <fstream>
#include <sstream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <png.h>
#include <chrono>
#include <thread>
#include "vec2.cpp"
#include "vec3.cpp"

using namespace std;
using namespace std::chrono;

#define WIDTH 900
#define HEIGHT 500
#define FPS 60
#define MOVESPEED 4
#define ROTSPEED 2

#define TEXSIZE 16
#define TEXSIZELOG 4

bool limitFPS = false;

SDL_Window* screen;
SDL_Renderer* renderer;

float FOV = 90; // degrees (don't ask)
float d = -WIDTH/2*tan(FOV*M_PI/360);
float renderDistance = 20;
Vec3 camPosition = Vec3(0, 0, 10);
vector<float> camRotation = {0, 0};
Uint8 sky0[3] = {130, 168, 255};
Uint8 sky1[3] = {187, 212, 255};
Uint32* textureMap;

// warning: starts at the front (z >)
Uint8 facesIndices[6][4] = {{0, 1, 3, 2}, {5, 4, 6, 7}, {4, 0, 2, 6}, {1, 5, 7, 3}, {4, 5, 1, 0}, {2, 3, 7, 6}};

void renderText(string text, int x, int y, SDL_Color color, TTF_Font* font) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    SDL_QueryTexture(texture, nullptr, nullptr, &rect.w, &rect.h);
    SDL_RenderCopy(renderer, texture, nullptr, &rect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void debug(float t, SDL_Color* color, TTF_Font* font) {
    renderText("FPS: " + (t == 0 ? "inf" : to_string((int)(1/t))), 0, 0, *color, font);
    //renderText("Position: " + camPosition.str(), 0, 16, *color, font);
    //renderText("Rotation: " + Vec3(camRotation[0], camRotation[1]).str(), 0, 32, *color, font);
}

class Cube {
    public:
        Cube(Vec3 _pos, Uint8 _texId) {
            pos = _pos;
            texId = _texId;
        }
        void updateVisible(vector<Cube> cubes, int n_cubes) {
            for (Uint8 i = 0; i < 6; i++) visible[i] = true;
            Uint8 n = 0; // if reaches 6, all faces are hidden (no further checks)
            for (int i = 0; i < n_cubes; i++) {
                if (n == 6) break;
                Vec3 p = cubes[i].pos;
                bool ex = p.x == pos.x;
                bool ey = p.y == pos.y;
                bool ez = p.z == pos.z;
                if (ex && ey) {
                    if (p.z+1 == pos.z) { visible[0] = false; n++; }
                    if (p.z-1 == pos.z) { visible[1] = false; n++; }
                } if (ey && ez) {
                    if (p.x+1 == pos.x) { visible[2] = false; n++; }
                    if (p.x-1 == pos.x) { visible[3] = false; n++; }
                } if (ex && ez) {
                    if (p.y+1 == pos.y) { visible[4] = false; n++; }
                    if (p.y-1 == pos.y) { visible[5] = false; n++; }
                }
            }
        }
        int getTexId(Uint8 face) {
            switch (texId) {
                case 0: return face == 4 ? 2 : face == 5 ? 0 : 1;
                case 6: return face == 4 ? 6 : face == 5 ? 6 : 7;
                default: return texId;
            }
        }
        Vec3 pos;
        Uint8 texId; // position in the texture
        bool visible[6]; // don't draw hidden faces
};

Uint32* getTextureMap() {
    /* Uint32* textureMap = (Uint32*)malloc(16*sizeof(Uint32)<<(TEXSIZELOG<<1));
    for (int t = 0; t < 16; t++) {
        for (int x = 0; x < TEXSIZE; x++) {
            for (int y = 0; y < TEXSIZE; y++) {
                int i = x + (y + (t << TEXSIZELOG) << TEXSIZELOG);
                if ((x+y) % 2 == 0) textureMap[i] = 0xffffff;
                else textureMap[i] = 0;
            }
        }
    }
    return textureMap; */

    // thank you ChatGPT
    FILE* file = fopen("assets/tex.png", "rb");
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    png_init_io(png, file);
    png_read_info(png, info);
    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_read_update_info(png, info);
    int row_bytes = png_get_rowbytes(png, info);
    png_bytep row_pointers[height];

    for (int i = 0; i < height; ++i) {
        row_pointers[i] = new png_byte[row_bytes];
    }

    png_read_image(png, row_pointers);

    // fill textureMap with png data
    Uint32* textureMap = (Uint32*)malloc(width*height*4);
    int w = width>>TEXSIZELOG; // number of textures per line
    for (int Y = 0; Y < height>>TEXSIZELOG; Y++) {
        for (int y = 0; y < TEXSIZE; y++) {
            png_bytep row = row_pointers[y + (Y<<TEXSIZELOG)];
            for (int X = 0; X < w; X++) {
                for (int x = 0; x < TEXSIZE; x++) {
                    int i = x + (y + (X + Y*w << TEXSIZELOG) << TEXSIZELOG);
                    textureMap[i] = 0;
                    for (int c = 0; c < 4; c++) {
                        textureMap[i] += row[(x + (X<<TEXSIZELOG)) * 4 + c] << (c<<3);
                    }
                }
            }
        }
    }

    // clean up
    for (int i = 0; i < height; i++) {
        delete[] row_pointers[i];
    }

    png_destroy_read_struct(&png, &info, NULL);
    fclose(file);

    return textureMap;
}

pair<int, vector<Cube>> loadWorld(string path) {
    vector<Cube> cubes;
    int n_cubes = 0;
    string line;
    ifstream file(path);

    while (getline(file, line)) {
        istringstream iline(line);
        vector<int> values;
        int n;
        while (iline >> n) values.push_back(n);
        cubes.push_back(Cube(Vec3(values[0], values[1], values[2]), values[3]));
        n_cubes++;
    }
    file.close();
    return pair(n_cubes, cubes);
}

Vec3 rotatePoint(Vec3 point) {
    // rotate point according to camera rotation

    float x = point.x-camPosition.x; float y = point.y-camPosition.y; float z = point.z-camPosition.z;
    //float x = point.x; float y = point.y; float z = point.z;
    float angle = atan2(z,x);
    if (x != 0) {
        float l;
        if (-0.01 < x && x < 0.01) l = sqrt(x*x + z*z);
        else l = x/cos(angle);
        z = l*sin(angle-camRotation[1]);
        x = l*cos(angle-camRotation[1]);
    } else {
        x = z*sin(camRotation[1]);
        z = z*cos(camRotation[1]);
    }
    angle = atan2(z, y);
    if (y != 0) {
        float l;
        if (-0.01 < y && y < 0.01) l = sqrt(y*y + z*z);
        else l = y/cos(angle);
        z = l*sin(angle-camRotation[0]);
        y = l*cos(angle-camRotation[0]);
    } else {
        y = z*sin(camRotation[0]);
        z = z*cos(camRotation[0]);
    }

    //return Vec3(x-camPosition.x, y-camPosition.y, z-camPosition.z);
    return Vec3(x, y, z);
}

vector<int> project(Vec3 point) {
    if (-point.z > renderDistance || point.z > 1) {
        return {NULL, NULL};
    }
    float m = point.z == 0 ? -d*1000 : point.z > 0 ? -d/point.z : d/point.z;
    return {(int)((WIDTH>>1) + point.x*m), (int)((HEIGHT>>1) - point.y*m)};
}

void drawFace(vector<int> points[4], Uint32* pixels, Uint8 texId) {
    int* a = &points[0][0];
    int* b = &points[1][0];
    int* c = &points[2][0];
    int* d = &points[3][0];
    int texi = texId<<(TEXSIZELOG<<1);
    if (a[0] == d[0] && b[0] == c[0] && a[0] != b[0] && camRotation[0] == 0) {
        // straight rectangle: draw vertical lines
        float x0 = b[0] < 0 ? 0 : b[0];
        float x1 = a[0] > WIDTH ? WIDTH : a[0];
        float spanX = a[0]-b[0];

        for (int x = x0; x < x1; x++) {
            float tx = (x-b[0]) / spanX;

            float y0 = c[1] + (d[1]-c[1])*tx;
            float y1 = b[1] + (a[1]-b[1])*tx;
            float my = y0 < 0 ? 0 : y0;
            float My = y1 > WIDTH ? WIDTH : y1;
            float spanY = (y1-y0);

            for (int y = my; y < My; y++) {
                float ty = (y-y0)/spanY;
                if (ty < 0) ty = 0;
                pixels[x + y*WIDTH] = textureMap[texi + (int)(tx*TEXSIZE) + ((int)(ty*TEXSIZE)<<TEXSIZELOG)];
            }
        }
    } else {
        // quads oriented randomly
        int topId = 0;
        float my, My;
        my = My = points[0][1];
        for (int i = 0; i < 4; i++) {
            int y = points[i][1];
            if (y < my) {
                topId = i;
                my = y;
            }
            if (y > My) My = y;
        }

        int* right = &points[(topId+1)%4][0];
        int* bottom = &points[(topId+2)%4][0];
        int* left = &points[(topId+3)%4][0];
        int* top = &points[topId][0];
        if (my == bottom[1]) return;
        bool lSwap = false;
        bool rSwap = false;
        if (left[1] > bottom[1]) lSwap = true;
        if (right[1] > bottom[1]) rSwap = true;

        float y0 = my; float y1 = My;
        if (y0 < 0) y0 = 0;
        if (y1 > HEIGHT) y1 = HEIGHT;

        // todo: vertical side longer than the other, other y in between
        for (int y = y0; y < y1; y++) {
            float t, x0, x1;
            float txa, txb, tya, tyb;
            if (y < left[1]) {
                if (left[1] == my) t = 0;
                else t = (y-my)/(left[1]-my);
                x0 = top[0]+(left[0]-top[0])*t;
                txa = 0;
                tya = t;
            } else {
                if (rSwap && y > bottom[1]) {
                    if (bottom[1] == right[1]) y = 0;
                    else t = (float)(y-bottom[1])/(right[1]-bottom[1]);
                    x0 = bottom[0]+(right[0]-bottom[0])*t;
                    txa = 1;
                    tya = 1-t;
                } else {
                    if (left[1] == bottom[1]) t = 0;
                    else t = (float)(y-left[1])/(bottom[1]-left[1]);
                    x0 = left[0]+(bottom[0]-left[0])*t;
                    txa = t;
                    tya = 1;
                }
            }
            if (y < right[1]) {
                if (right[1] == my) t = 0;
                else t = (y-my)/(right[1]-my);
                x1 = top[0]+(right[0]-top[0])*t;
                txb = t;
                tyb = 0;
            } else {
                if (lSwap && y > bottom[1]) {
                    if (bottom[1] == left[1]) t = 0;
                    else t = (float)(y-bottom[1])/(left[1]-bottom[1]);
                    x1 = bottom[0]+(left[0]-bottom[0])*t;
                    txb = 1-t;
                    tyb = 1;
                } else {
                    if (right[1] == bottom[1]) t = 0;
                    else t = (float)(y-right[1])/(bottom[1]-right[1]);
                    x1 = right[0]+(bottom[0]-right[0])*t;
                    txb = 1;
                    tyb = t;
                }
            }
            // rotate texture to start from anchor
            if (topId == 3) {
                float _txa = txa;
                float _txb = txb;
                txa = 1-tya;
                txb = 1-tyb;
                tya = _txa;
                tyb = _txb;
            } else if (topId == 0) {
                txa = 1-txa;
                txb = 1-txb;
                tya = 1-tya;
                tyb = 1-tyb;
            } else if (topId == 1) {
                float _txa = txa;
                float _txb = txb;
                txa = tya;
                txb = tyb;
                tya = 1-_txa;
                tyb = 1-_txb;
           }

            float _x0 = x0 < 0 ? 0 : x0;
            float _x1 = x1 > WIDTH ? WIDTH : x1;
            float spanTx = (txb-txa);
            float spanTy = (tyb-tya);
            if (x0 == x1) continue;
            for (int x = _x0; x < _x1; x++) {
                float t = (x-x0)/(x1-x0);
                if (t <= 0) t = 0.01;
                if (t > 1) t = 1;
                float tx = txa + spanTx*t;
                float ty = tya + spanTy*t;
                pixels[x + y*WIDTH] = textureMap[texi + (int)(tx*TEXSIZE) + ((int)(ty*TEXSIZE)<<TEXSIZELOG)];
            }
        }
    }
}

void background(Uint32* pixels) {
    Uint8 a, b;
    int bottom = (HEIGHT>>1) - sin(camRotation[0])*d;
    Uint8 height = HEIGHT>>3;
    if (bottom < 0) {
        for (int i = 0; i < WIDTH*HEIGHT; i++) pixels[i] = (sky0[2]<<16)+(sky1[1]<<8)+sky1[0];
    } else if (bottom > HEIGHT+height) {
        for (int i = 0; i < WIDTH*HEIGHT; i++) pixels[i] = (sky0[2]<<16)+(sky0[1]<<8)+sky0[0];
    } else {
        for (int x = 0; x < WIDTH; x++) {
            for (int y = 0; y < HEIGHT; y++) {
                if (y < bottom) {
                    float t = (float)(y-bottom+height) / height;
                    if (t < 0) {
                        a = sky0[0];
                        b = sky0[1];
                    } else {
                        a = sky0[0] + (sky1[0]-sky0[0])*t;
                        b = sky0[1] + (sky1[1]-sky0[1])*t;
                    }
                } else {
                    a = sky1[0];
                    b = sky1[1];
                }
                pixels[x + y*WIDTH] = 16711680+(b<<8)+a;
            }
        }
    }
}

void sortCubes(int* indexes, float* dists, int n_cubes) {
    for (int i = 0; i < n_cubes; i++) {
        int maxi = i;
        float max = dists[i];
        for (int j = i+1; j < n_cubes; j++) {
            if (dists[j] > max) {
                maxi = j;
                max = dists[j];
            }
        }
        int _i = indexes[i];
        indexes[i] = indexes[maxi];
        indexes[maxi] = _i;
        float d = dists[i];
        dists[i] = max;
        dists[maxi] = d;
    }
}

void renderScene(vector<Cube> cubes, int n_cubes, Uint32* pixels) {
    background(pixels);
    vector<int> invisible = {NULL, NULL};
    if (!n_cubes) return;

    // sort cubes: sort indexes depending on dist^2
    int indexes[n_cubes];
    float dists[n_cubes];
    for (int i = 0; i < n_cubes; i++) {
        Vec3 pos = cubes[i].pos;
        float dx = camPosition.x-pos.x-0.5;
        float dy = camPosition.y-pos.x-0.5;
        float dz = camPosition.z-pos.x-0.5;
        indexes[i] = i;
        dists[i] = (camPosition-pos).len2();
    }
    sortCubes(indexes, dists, n_cubes);

    for(int k = 0; k < n_cubes; k++) {
        int i = indexes[k];
        Cube cube = cubes[i];
        Vec3 pos = cubes[i].pos;
        bool visible = true;

        vector<int> points[8];
        for (int i = 0; i < 8; i++) {
            vector<int> projected = project(rotatePoint({pos.x+i%2, pos.y+(i%4)/2, pos.z+i/4}));
            if (projected == invisible) {
                visible = false;
                break;
            }
            points[i] = projected;
        }
        if (!visible) continue;

        Uint8 nearest[3] = {255, 255, 255};
        if (pos.x > camPosition.x && cube.visible[2]) nearest[0] = 2;
        else if (pos.x+1 < camPosition.x && cube.visible[3]) nearest[0] = 3;
        if (pos.y > camPosition.y && cube.visible[4]) nearest[1] = 4;
        else if (pos.y+1 < camPosition.y && cube.visible[5]) nearest[1] = 5;
        if (pos.z > camPosition.z && cube.visible[0]) nearest[2] = 0;
        else if (pos.z+1 < camPosition.z && cube.visible[1]) nearest[2] = 1;

        for (Uint8 i : nearest) {
            if (i == 255) continue;
            vector<int> p[4] = {
                points[facesIndices[i][0]],
                points[facesIndices[i][1]],
                points[facesIndices[i][2]],
                points[facesIndices[i][3]]
            };
            drawFace(p, pixels, cube.getTexId(i));
        }
    }
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_EVERYTHING);
    TTF_Init();
    screen = SDL_CreateWindow("3D renderer", 50, 50, WIDTH, HEIGHT, 0);
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("assets/Consolas.ttf", 16);
    SDL_Color white = {255, 255, 255};
    SDL_Color black = {0, 0, 0};

    textureMap = getTextureMap();

    pair<int, vector<Cube>> p = loadWorld("assets/worlds/tree");
    auto [n_cubes, cubes] = p;
    if (n_cubes > 0) for (int i = 0; i < n_cubes; i++) cubes[i].updateVisible(cubes, n_cubes);

    SDL_Texture* pixBuf = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, WIDTH, HEIGHT);
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    Uint32* pixels = new Uint32[WIDTH*HEIGHT];

    double prev, now;
    float timePassed = 0;
    float toWait;
    SDL_Event event;
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
            }
        }
        Vec2 movementFlat = {0, 0};
        float y = 0;
        if (keys[SDL_SCANCODE_W]) movementFlat.y -= 1;
        if (keys[SDL_SCANCODE_A]) movementFlat.x -= 1;
        if (keys[SDL_SCANCODE_S]) movementFlat.y += 1;
        if (keys[SDL_SCANCODE_D]) movementFlat.x += 1;
        if (keys[SDL_SCANCODE_SPACE]) y += 1;
        if (keys[SDL_SCANCODE_LSHIFT]) y -= 1;
        float r = ROTSPEED*timePassed;
        if (keys[SDL_SCANCODE_LEFT]) camRotation[1] -= r;
        if (keys[SDL_SCANCODE_RIGHT]) camRotation[1] += r;
        if (keys[SDL_SCANCODE_DOWN]) camRotation[0] -= r;
        if (keys[SDL_SCANCODE_UP]) camRotation[0] += r;
        if (keys[SDL_SCANCODE_LCTRL]) movementFlat *= 2;
        movementFlat.rotate(camRotation[1]);
        Vec3 movement = {movementFlat.x, y, movementFlat.y};
        camPosition += movement*MOVESPEED*timePassed;

        renderScene(cubes, n_cubes, pixels);
        SDL_UpdateTexture(pixBuf, NULL, pixels, WIDTH*sizeof(uint32_t));
        SDL_RenderCopy(renderer, pixBuf, NULL, NULL);
        debug(timePassed, &black, font);

        SDL_RenderPresent(renderer);

        now = time_point_cast<milliseconds>(system_clock::now()).time_since_epoch().count();
        timePassed = (now-prev) / 1000;
        prev = now;

        if (limitFPS) {
            toWait = 1/FPS - timePassed;
            if (toWait > 0) {
                this_thread::sleep_for(milliseconds((int)(toWait*1000)));
                timePassed = 1/FPS;
            }
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(screen);
    SDL_Quit();
    delete[] pixels;
    free(textureMap);
    return 0;
}