// todo: opti background

#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
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

#define SKY 0xfffff0d0;

bool limitFPS = false;

SDL_Window* screen;
SDL_Renderer* renderer;

float FOV = 90; // degrees (don't ask)
float d = -WIDTH/2*tan(FOV*M_PI/360);
float NEAR = 0.1; float FAR = 100;
float renderDistance = 10;
Vec3 camPosition = Vec3(0, 0, 5);
vector<float> camRotation = {0, 0};

// warning: starts at the front (z >)
Uint8 facesIndices[6][4] = {{0, 1, 3, 2}, {5, 4, 6, 7}, {1, 5, 7, 3}, {4, 0, 2, 6}, {4, 5, 1, 0}, {2, 3, 7, 6}};

int renderMode = 0;

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

void debug(float times[], SDL_Color* color, TTF_Font* font) {
    float sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += times[i];
    }
    int smoothFPS;
    if (sum != 0) {
        smoothFPS = 10/sum;
    } else {
        smoothFPS = -1;
    }
    renderText("FPS: " + to_string(smoothFPS), 0, 0, *color, font);
    //renderText("Position: " + camPosition.str(), 0, 16, *color, font);
    //renderText("Rotation: " + Vec3(camRotation[0], camRotation[1]).str(), 0, 32, *color, font);
}

Vec3 rotatePoint(Vec3 point) {
    // rotate point according to camera rotation

    float x = point.x-camPosition.x; float y = point.y-camPosition.y; float z = point.z-camPosition.z;
    //float x = point.x; float y = point.y; float z = point.z;
    float angle = atan2(z,x);
    if (x != 0) {
        x /= cos(angle);
        z = x*sin(angle-camRotation[1]);
        x = x*cos(angle-camRotation[1]);
    } else {
        x = z*sin(camRotation[1]);
        z = z*cos(camRotation[1]);
    }
    angle = atan2(z, y);
    if (y != 0) {
        y /= cos(angle);
        z = y*sin(angle-camRotation[0]);
        y = y*cos(angle-camRotation[0]);
    } else {
        y = z*sin(camRotation[0]);
        z = z*cos(camRotation[0]);
    }

    //return Vec3(x-camPosition.x, y-camPosition.y, z-camPosition.z);
    return Vec3(x, y, z);
}

vector<int16_t> project(Vec3 point) {
    if (-point.z < NEAR || -point.z > FAR) {
        return {NULL, NULL};
    }
    float m = d/point.z;
    return {(int16_t)((WIDTH>>1) + point.x*m), (int16_t)((HEIGHT>>1) - point.y*m)};
}

void drawFace(vector<int16_t> points[4], Uint32* pixels) {
    int16_t* a = &points[0][0];
    int16_t* b = &points[1][0];
    int16_t* c = &points[2][0];
    int16_t* d = &points[3][0];
    if (a[0] == d[0] && b[0] == c[0] && a[0] != b[0]) {
        // straight rectangle: draw vertical lines
        float x0 = b[0] < 0 ? 0 : b[0];
        float x1 = a[0] > WIDTH ? WIDTH : a[0];
        float spanX = a[0]-b[0];

        for (int x = x0; x < x1; x++) {
            float tx = (x-b[0]) / spanX;

            float my = c[1] + (d[1]-c[1])*tx;
            float My = b[1] + (a[1]-b[1])*tx;
            if (my < 0) my = 0;
            if (My > HEIGHT) My = HEIGHT;
            float spanY = (My-my);

            for (int y = my; y < My; y++) {
                float ty = (y-my)/spanY;
                if (ty < 0) ty = 0;
                pixels[x + y*WIDTH] = (255<<24) + ((int)(255*ty)<<16) + ((int)(255 - 255*tx)<<8) + (int)(255*tx);
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

        int16_t* right = &points[(topId+1)%4][0];
        int16_t* bottom = &points[(topId+2)%4][0];
        int16_t* left = &points[(topId+3)%4][0];
        int16_t* top = &points[topId][0];
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

            if (x0 < 0) x0 = 0;
            if (x1 > WIDTH) x1 = WIDTH;
            float spanTx = (txb-txa);
            float spanTy = (tyb-tya);
            for (int x = x0; x < x1; x++) {
                float t = (x-x0)/(x1-x0);
                if (t < 0) t = 0;
                float tx = txa + spanTx*t;
                float ty = tya + spanTy*t;
                pixels[x + y*WIDTH] = ((int)(255*ty)<<16) + ((int)(255*tx)<<8) + (int)(255-255*tx);
            }
        }
    }
}

void background(Uint32* pixels) {
    Uint8 col0[2] = {130, 168};
    Uint8 col1[2] = {187, 212};
    Uint8 a, b;
    int bottom = (HEIGHT>>1) - sin(camRotation[0])*d;
    Uint8 height = HEIGHT>>3;
    if (bottom < 0) {
        for (int i = 0; i < WIDTH*HEIGHT; i++) pixels[i] = (255<<16)+(col1[1]<<8)+col1[0];
    } else if (bottom > HEIGHT+height) {
        for (int i = 0; i < WIDTH*HEIGHT; i++) pixels[i] = (255<<16)+(col0[1]<<8)+col0[0];
    } else {
        for (int x = 0; x < WIDTH; x++) {
            for (int y = 0; y < HEIGHT; y++) {
                if (y < bottom) {
                    float t = (float)(y-bottom+height) / height;
                    if (t < 0) {
                        a = col0[0];
                        b = col0[1];
                    } else {
                        a = col0[0] + (col1[0]-col0[0])*t;
                        b = col0[1] + (col1[1]-col0[1])*t;
                    }
                } else {
                    a = col1[0];
                    b = col1[1];
                }
                pixels[x + y*WIDTH] = (255<<16)+(b<<8)+a;
            }
        }
    }
}

// TODO: cache rotated pos
void renderScene(vector<Vec3> cubePos, Uint32* pixels) {
    cout << endl;
    background(pixels);
    vector<int16_t> invisible = {NULL, NULL};

    for(Vec3 pos: cubePos) {
        bool visible = true;
        vector<int16_t> points[8];
        for (int i = 0; i < 8; i++) {
            vector<int16_t> projected = project(rotatePoint({pos.x+i%2, pos.y+(i%4)/2, pos.z+i/4}));
            if (projected == invisible) {
                visible = false;
                break;
            }
            points[i] = projected;
        }
        if (!visible) continue;

        Uint8 nearest[3] = {255, 255, 255};
        if (pos.x > camPosition.x) nearest[0] = 3;
        else if (pos.x+1 < camPosition.x) nearest[0] = 2;
        if (pos.y > camPosition.y) nearest[1] = 4;
        else if (pos.y+1 < camPosition.y) nearest[1] = 5;
        if (pos.z > camPosition.z) nearest[2] = 0;
        else if (pos.z+1 < camPosition.z) nearest[2] = 1;

        for (Uint8 i : nearest) {
            if (i == 255) continue;
            vector<int16_t> p[4] = {
                points[facesIndices[i][0]],
                points[facesIndices[i][1]],
                points[facesIndices[i][2]],
                points[facesIndices[i][3]]
            };
            drawFace(p, pixels);
        }
    }
}

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_EVERYTHING);
    TTF_Init();
    screen = SDL_CreateWindow("3D renderer", 50, 50, WIDTH, HEIGHT, 0);
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("assets/fonts/Consolas.ttf", 16);
    SDL_Color white = {255, 255, 255};
    SDL_Color black = {0, 0, 0};

    vector<Vec3> cubesPos = {Vec3(0, 0, 0)};
    for (int x = 0; x < 20; x++) {
        for (int y = 0; y < 20; y++) cubesPos.push_back({(float)x, (float)y, 0});
    }

    SDL_Texture* pixBuf = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, WIDTH, HEIGHT);
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    Uint32* pixels = new Uint32[WIDTH*HEIGHT];

    double prev, now;
    float timePassed = 0;
    float toWait;
    float times[10];
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

        renderScene(cubesPos, pixels);
        SDL_UpdateTexture(pixBuf, NULL, pixels, WIDTH*sizeof(uint32_t));
        SDL_RenderCopy(renderer, pixBuf, NULL, NULL);
        debug(times, &black, font);

        SDL_RenderPresent(renderer);

        now = time_point_cast<milliseconds>(system_clock::now()).time_since_epoch().count();
        timePassed = (now-prev) / 1000;
        prev = now;

        toWait = 1/FPS - timePassed;
        if (toWait > 0 && limitFPS) {
            this_thread::sleep_for(milliseconds((int)(toWait*1000)));
            timePassed = 1/FPS;
        }

        for (int i = 0; i < 10; i++) {
            if (i == 9) {
                times[9] = timePassed;
            } else {
                times[i] = times[i+1];
            }
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(screen);
    SDL_Quit();
    delete[] pixels;
    return 0;
}