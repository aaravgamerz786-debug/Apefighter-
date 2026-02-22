/*
=======================================================
  FIGHTER JET 3D GAME - C++ Source Code
  Target Device: Oppo A5 5G (1600x720 resolution)
  Libraries: SDL2, SDL2_image, SDL2_ttf, SDL2_mixer
  Author: Generated for Oppo A5 5G Full Screen Support
=======================================================
  
  BUILD INSTRUCTIONS:
  -------------------
  Linux/Android NDK:
    g++ fighter_jet_game.cpp -o fighter_jet \
        -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer \
        -lm -std=c++17
  
  Android (NDK Build):
    Use Android.mk or CMakeLists.txt with NDK r21+
    Add SDL2 as dependency in jni/Android.mk
  
  Screen: 720x1600 portrait (Oppo A5 5G native)
=======================================================
*/

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <memory>

// ==================== SCREEN CONSTANTS ====================
// Oppo A5 5G: 1600x720 (portrait = 720x1600)
const int SCREEN_W = 720;
const int SCREEN_H = 1600;
const float ASPECT = (float)SCREEN_W / SCREEN_H;

// UI Scale factor for high-DPI (Oppo A5 5G ~269 PPI)
const float UI_SCALE = 2.0f;
const int HUD_H    = (int)(160 * UI_SCALE); // bottom HUD height
const int PLAY_H   = SCREEN_H - HUD_H;       // playable area height

// ==================== GAME STATES ====================
enum GameState { STATE_MENU, STATE_PLAYING, STATE_PAUSED, STATE_GAMEOVER, STATE_WIN };

// ==================== COLORS ====================
struct Color { Uint8 r, g, b, a; };
const Color C_SKY_TOP    = {  10,  20,  80, 255 };
const Color C_SKY_BTM    = {  30,  80, 160, 255 };
const Color C_GROUND     = {  20,  80,  30, 255 };
const Color C_JET        = { 180, 200, 220, 255 };
const Color C_JET_DARK   = {  80, 100, 130, 255 };
const Color C_FIRE       = { 255, 140,   0, 255 };
const Color C_BULLET     = { 255, 255,   0, 255 };
const Color C_ENEMY      = { 220,  50,  50, 255 };
const Color C_HUD_BG     = {   0,   0,   0, 200 };
const Color C_GREEN      = {   0, 255,  80, 255 };
const Color C_RED        = { 255,  50,  50, 255 };
const Color C_WHITE      = { 255, 255, 255, 255 };
const Color C_GOLD       = { 255, 215,   0, 255 };
const Color C_CYAN       = {   0, 220, 255, 255 };
const Color C_PURPLE     = { 160,  32, 240, 255 };
const Color C_MISSILE    = { 255, 100,   0, 255 };

// ==================== MATH HELPERS ====================
struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

float lerp(float a, float b, float t) { return a + (b - a) * t; }
float clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
float dist2D(float x1, float y1, float x2, float y2) {
    float dx = x2-x1, dy = y2-y1;
    return sqrtf(dx*dx + dy*dy);
}

// Simple 3D -> 2D projection
struct Camera3D {
    float fov = 60.0f;
    float near = 0.1f;
    float far  = 1000.0f;
};

Vec2 project3D(Vec3 p, Camera3D& cam) {
    float fovRad = cam.fov * M_PI / 180.0f;
    float tanHalf = tanf(fovRad * 0.5f);
    float sx = (p.x / (p.z * tanHalf * ASPECT)) * (SCREEN_W * 0.5f) + SCREEN_W * 0.5f;
    float sy = (-p.y / (p.z * tanHalf)) * (SCREEN_H * 0.5f) + SCREEN_H * 0.5f;
    return { sx, sy };
}

// ==================== SDL DRAW HELPERS ====================
void setColor(SDL_Renderer* r, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

void fillRect(SDL_Renderer* r, int x, int y, int w, int h, Color c) {
    setColor(r, c);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

void drawCircle(SDL_Renderer* r, int cx, int cy, int radius, Color c) {
    setColor(r, c);
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrtf((float)(radius*radius - dy*dy));
        SDL_RenderDrawLine(r, cx-dx, cy+dy, cx+dx, cy+dy);
    }
}

void drawRing(SDL_Renderer* r, int cx, int cy, int outer, int inner, Color c) {
    setColor(r, c);
    for (int dy = -outer; dy <= outer; dy++) {
        int dx_o = (int)sqrtf(std::max(0.0f, (float)(outer*outer - dy*dy)));
        int dx_i = (abs(dy) > inner) ? 0 : (int)sqrtf((float)(inner*inner - dy*dy));
        SDL_RenderDrawLine(r, cx-dx_o, cy+dy, cx-dx_i, cy+dy);
        SDL_RenderDrawLine(r, cx+dx_i, cy+dy, cx+dx_o, cy+dy);
    }
}

void drawLine(SDL_Renderer* r, int x1, int y1, int x2, int y2, Color c) {
    setColor(r, c);
    SDL_RenderDrawLine(r, x1, y1, x2, y2);
}

// Draw text using filled rectangles (pixel font, no TTF needed)
void drawPixelChar(SDL_Renderer* r, int x, int y, char ch, int sz, Color c);
void drawText(SDL_Renderer* r, const std::string& txt, int x, int y, int sz, Color c);

// ==================== PIXEL FONT (5x7 bitmap) ====================
// Minimal 5x7 bitmap font for digits, letters, !, :, /, .
static const uint8_t FONT5x7[128][7] = {
    // Fill with zeros first; we'll define key chars
    [0 ... 127] = {0,0,0,0,0,0,0},
};

// Simple method: draw text as SDL_RenderFillRect blocks
void drawPixelText(SDL_Renderer* rend, const char* text, int x, int y, int scale, Color col) {
    // Very simple 3x5 pixel font using hardcoded segments
    setColor(rend, col);
    // We'll use a simplified approach with just drawing rectangles
    int ox = x;
    for (int ci = 0; text[ci] != '\0'; ci++) {
        char c = text[ci];
        if (c == ' ') { x += 4*scale; continue; }
        
        // Define segments for common chars using 3-wide 5-tall grid
        // Bit pattern: top, top-left, top-right, mid, bot-left, bot-right, bot
        // Use 0=off, 1=on for each pixel row
        uint8_t rows[5] = {0,0,0,0,0};
        
        switch(c) {
            case '0': rows[0]=7; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=7; break;
            case '1': rows[0]=3; rows[1]=1; rows[2]=1; rows[3]=1; rows[4]=7; break;
            case '2': rows[0]=7; rows[1]=1; rows[2]=7; rows[3]=4; rows[4]=7; break;
            case '3': rows[0]=7; rows[1]=1; rows[2]=7; rows[3]=1; rows[4]=7; break;
            case '4': rows[0]=5; rows[1]=5; rows[2]=7; rows[3]=1; rows[4]=1; break;
            case '5': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=1; rows[4]=7; break;
            case '6': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=5; rows[4]=7; break;
            case '7': rows[0]=7; rows[1]=1; rows[2]=1; rows[3]=1; rows[4]=1; break;
            case '8': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=7; break;
            case '9': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=1; rows[4]=7; break;
            case 'A': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=5; break;
            case 'B': rows[0]=6; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=6; break;
            case 'C': rows[0]=7; rows[1]=4; rows[2]=4; rows[3]=4; rows[4]=7; break;
            case 'D': rows[0]=6; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=6; break;
            case 'E': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=4; rows[4]=7; break;
            case 'F': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=4; rows[4]=4; break;
            case 'G': rows[0]=7; rows[1]=4; rows[2]=5; rows[3]=5; rows[4]=7; break;
            case 'H': rows[0]=5; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=5; break;
            case 'I': rows[0]=7; rows[1]=2; rows[2]=2; rows[3]=2; rows[4]=7; break;
            case 'J': rows[0]=1; rows[1]=1; rows[2]=1; rows[3]=5; rows[4]=7; break;
            case 'K': rows[0]=5; rows[1]=5; rows[2]=6; rows[3]=5; rows[4]=5; break;
            case 'L': rows[0]=4; rows[1]=4; rows[2]=4; rows[3]=4; rows[4]=7; break;
            case 'M': rows[0]=5; rows[1]=7; rows[2]=5; rows[3]=5; rows[4]=5; break;
            case 'N': rows[0]=5; rows[1]=7; rows[2]=7; rows[3]=5; rows[4]=5; break;
            case 'O': rows[0]=7; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=7; break;
            case 'P': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=4; rows[4]=4; break;
            case 'Q': rows[0]=7; rows[1]=5; rows[2]=5; rows[3]=7; rows[4]=1; break;
            case 'R': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=5; break; // same as A diff
            case 'S': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=1; rows[4]=7; break;
            case 'T': rows[0]=7; rows[1]=2; rows[2]=2; rows[3]=2; rows[4]=2; break;
            case 'U': rows[0]=5; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=7; break;
            case 'V': rows[0]=5; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=2; break;
            case 'W': rows[0]=5; rows[1]=5; rows[2]=5; rows[3]=7; rows[4]=5; break;
            case 'X': rows[0]=5; rows[1]=5; rows[2]=2; rows[3]=5; rows[4]=5; break;
            case 'Y': rows[0]=5; rows[1]=5; rows[2]=7; rows[3]=2; rows[4]=2; break;
            case 'Z': rows[0]=7; rows[1]=1; rows[2]=2; rows[3]=4; rows[4]=7; break;
            case ':': rows[0]=0; rows[1]=2; rows[2]=0; rows[3]=2; rows[4]=0; break;
            case '-': rows[0]=0; rows[1]=0; rows[2]=7; rows[3]=0; rows[4]=0; break;
            case '/': rows[0]=1; rows[1]=2; rows[2]=2; rows[3]=4; rows[4]=4; break;
            case '%': rows[0]=5; rows[1]=1; rows[2]=2; rows[3]=4; rows[4]=5; break;
            case '!': rows[0]=2; rows[1]=2; rows[2]=2; rows[3]=0; rows[4]=2; break;
            case '.': rows[0]=0; rows[1]=0; rows[2]=0; rows[3]=0; rows[4]=2; break;
            case '+': rows[0]=0; rows[1]=2; rows[2]=7; rows[3]=2; rows[4]=0; break;
            case '*': rows[0]=5; rows[1]=2; rows[2]=7; rows[3]=2; rows[4]=5; break;
            case '<': rows[0]=1; rows[1]=2; rows[2]=4; rows[3]=2; rows[4]=1; break;
            case '>': rows[0]=4; rows[1]=2; rows[2]=1; rows[3]=2; rows[4]=4; break;
            default: x += 4*scale; continue;
        }
        
        for (int row = 0; row < 5; row++) {
            for (int bit = 2; bit >= 0; bit--) {
                if (rows[row] & (1 << bit)) {
                    int bx = x + (2-bit)*scale;
                    int by = y + row*scale;
                    SDL_Rect px = {bx, by, scale, scale};
                    SDL_RenderFillRect(rend, &px);
                }
            }
        }
        x += 4*scale;
    }
}

// ==================== GAME OBJECTS ====================

struct Bullet {
    float x, y;
    float vx, vy;
    bool active;
    bool isEnemy;
    int damage;
    Color col;
};

struct Missile {
    float x, y;
    float vx, vy;
    float targetX, targetY;
    bool active;
    bool isEnemy;
    int damage;
    float life;
};

struct Explosion {
    float x, y;
    float radius;
    float maxRadius;
    float life;
    float maxLife;
    Color col;
};

struct PowerUp {
    float x, y;
    float vy;
    bool active;
    int type; // 0=health, 1=shield, 2=rapid, 3=missile, 4=bomb
    float bob;
};

struct EnemyJet {
    float x, y;
    float vx, vy;
    bool active;
    int hp, maxHp;
    int type;       // 0=basic, 1=fast, 2=heavy, 3=boss
    float shootTimer;
    float shootInterval;
    float moveTimer;
    float depth;    // 3D depth (z)
    int score;
};

struct Star {
    float x, y;
    float speed;
    float brightness;
    int size;
};

struct Cloud {
    float x, y;
    float speed;
    float w, h;
    int alpha;
};

struct Mountain {
    float x, h;
    float speed;
    Color col;
};

// ==================== PLAYER ====================
struct Player {
    float x, y;
    float vx, vy;
    int hp, maxHp;
    int shield, maxShield;
    bool shieldActive;
    float shieldTimer;
    
    int score;
    int lives;
    int level;
    int kills;
    
    // Weapons
    int ammo;         // missiles
    int bombs;
    bool rapidFire;
    float rapidTimer;
    float shootCooldown;
    float shootTimer;
    
    // Thruster animation
    float thrusterAnim;
    
    // Invincibility after hit
    float invTimer;
    
    // Touch drag
    bool dragging;
    float dragOffX, dragOffY;
    
    // 3D tilt
    float tiltX, tiltY; // -1 to 1
};

// ==================== GAME STATE ====================
struct Game {
    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    
    GameState state = STATE_MENU;
    Player player;
    
    std::vector<Bullet>    bullets;
    std::vector<Missile>   missiles;
    std::vector<Explosion> explosions;
    std::vector<PowerUp>   powerups;
    std::vector<EnemyJet>  enemies;
    std::vector<Star>      stars;
    std::vector<Cloud>     clouds;
    std::vector<Mountain>  mountains;
    
    // Timing
    Uint32 lastTime = 0;
    float  dt       = 0;
    float  gameTime = 0;
    
    // Spawning
    float enemySpawnTimer  = 0;
    float enemySpawnInterval = 2.0f;
    float powerupSpawnTimer  = 0;
    float cloudTimer         = 0;
    float mountainTimer      = 0;
    float bossSpawnTimer     = 0;
    bool  bossAlive          = false;
    
    // Scroll
    float scrollY = 0;
    float bgScrollY = 0;
    
    // UI
    float menuAnim = 0;
    float gameoverTimer = 0;
    
    // Combo
    int   combo        = 0;
    float comboTimer   = 0;
    int   highScore    = 0;
    
    // Screen shake
    float shakeTimer   = 0;
    float shakeAmt     = 0;
    
    // Touch
    int touchX = SCREEN_W/2;
    int touchY = PLAY_H/2;
    
    // Buttons (HUD)
    SDL_Rect btnFire;
    SDL_Rect btnMissile;
    SDL_Rect btnBomb;
    SDL_Rect btnPause;
};

// ==================== INIT ====================
void initPlayer(Player& p) {
    p.x = SCREEN_W / 2.0f;
    p.y = PLAY_H - 200.0f;
    p.vx = p.vy = 0;
    p.hp = p.maxHp = 100;
    p.shield = p.maxShield = 50;
    p.shieldActive = false;
    p.shieldTimer = 0;
    p.score = 0;
    p.lives = 3;
    p.level = 1;
    p.kills = 0;
    p.ammo = 10;
    p.bombs = 3;
    p.rapidFire = false;
    p.rapidTimer = 0;
    p.shootCooldown = 0.15f;
    p.shootTimer = 0;
    p.thrusterAnim = 0;
    p.invTimer = 0;
    p.dragging = false;
    p.tiltX = p.tiltY = 0;
}

void initStars(std::vector<Star>& stars) {
    stars.clear();
    for (int i = 0; i < 150; i++) {
        Star s;
        s.x = (float)(rand() % SCREEN_W);
        s.y = (float)(rand() % PLAY_H);
        s.speed = 50.0f + rand() % 150;
        s.brightness = 0.3f + (rand() % 70) / 100.0f;
        s.size = 1 + rand() % 3;
        stars.push_back(s);
    }
}

void initClouds(std::vector<Cloud>& clouds) {
    clouds.clear();
    for (int i = 0; i < 8; i++) {
        Cloud c;
        c.x = (float)(rand() % SCREEN_W);
        c.y = (float)(rand() % PLAY_H);
        c.speed = 30.0f + rand() % 50;
        c.w = 80.0f + rand() % 120;
        c.h = 30.0f + rand() % 40;
        c.alpha = 40 + rand() % 60;
        clouds.push_back(c);
    }
}

void initMountains(std::vector<Mountain>& mountains) {
    mountains.clear();
    for (int i = 0; i < 6; i++) {
        Mountain m;
        m.x = (float)(i * 130);
        m.h = 100.0f + rand() % 150;
        m.speed = 20.0f;
        m.col = { (Uint8)(20 + rand()%30), (Uint8)(60 + rand()%40), (Uint8)(20 + rand()%20), 255 };
        mountains.push_back(m);
    }
}

Game* g_game = nullptr;

// ==================== SPAWN FUNCTIONS ====================
void spawnExplosion(Game& g, float x, float y, float sz, Color col) {
    Explosion e;
    e.x = x; e.y = y;
    e.radius = sz * 0.1f;
    e.maxRadius = sz;
    e.life = e.maxLife = 0.5f;
    e.col = col;
    g.explosions.push_back(e);
    // Screen shake
    g.shakeTimer = 0.2f;
    g.shakeAmt = sz * 0.5f;
}

void spawnBullet(Game& g, float x, float y, float vx, float vy, bool isEnemy, Color col, int dmg=10) {
    Bullet b;
    b.x = x; b.y = y;
    b.vx = vx; b.vy = vy;
    b.active = true;
    b.isEnemy = isEnemy;
    b.damage = dmg;
    b.col = col;
    g.bullets.push_back(b);
}

void spawnMissile(Game& g, float x, float y, float tx, float ty, bool isEnemy, int dmg=30) {
    Missile m;
    m.x = x; m.y = y;
    m.targetX = tx; m.targetY = ty;
    float dx = tx - x, dy = ty - y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len > 0) { dx /= len; dy /= len; }
    m.vx = dx * 400.0f;
    m.vy = dy * 400.0f;
    m.active = true;
    m.isEnemy = isEnemy;
    m.damage = dmg;
    m.life = 3.0f;
    g.missiles.push_back(m);
}

void spawnPowerup(Game& g, float x, float y) {
    PowerUp p;
    p.x = x; p.y = y;
    p.vy = 80.0f;
    p.active = true;
    p.type = rand() % 5;
    p.bob = 0;
    g.powerups.push_back(p);
}

void spawnEnemy(Game& g, int type) {
    EnemyJet e;
    e.x = 60.0f + rand() % (SCREEN_W - 120);
    e.y = -80.0f;
    e.active = true;
    e.moveTimer = 0;
    e.depth = 5.0f + rand() % 10;
    
    switch(type) {
        case 0: // basic
            e.hp = e.maxHp = 30;
            e.vx = (rand()%2 ? 60.0f : -60.0f);
            e.vy = 80.0f;
            e.shootInterval = 1.5f;
            e.score = 100;
            break;
        case 1: // fast
            e.hp = e.maxHp = 20;
            e.vx = (rand()%2 ? 120.0f : -120.0f);
            e.vy = 160.0f;
            e.shootInterval = 1.0f;
            e.score = 200;
            break;
        case 2: // heavy
            e.hp = e.maxHp = 80;
            e.vx = (rand()%2 ? 40.0f : -40.0f);
            e.vy = 60.0f;
            e.shootInterval = 0.8f;
            e.score = 500;
            break;
        case 3: // boss
            e.x = SCREEN_W / 2.0f;
            e.hp = e.maxHp = 500;
            e.vx = 100.0f;
            e.vy = 30.0f;
            e.shootInterval = 0.4f;
            e.score = 5000;
            break;
    }
    e.type = type;
    e.shootTimer = e.shootInterval;
    g.enemies.push_back(e);
}

// ==================== DRAW JET (Player) ====================
void drawPlayerJet(SDL_Renderer* r, float x, float y, float tiltX, float thrusterAnim, float invTimer) {
    // Blink if invincible
    if (invTimer > 0 && (int)(invTimer * 10) % 2 == 0) return;
    
    int cx = (int)x, cy = (int)y;
    float tx = tiltX; // -1=left, 0=straight, 1=right
    
    // Thruster flame
    int flameH = (int)(20 + sinf(thrusterAnim * 10) * 8);
    Color flameCol = { 255, (Uint8)(100 + sinf(thrusterAnim*15)*80), 0, 255 };
    fillRect(r, cx-8,  cy+40, 16, flameH, flameCol);
    fillRect(r, cx-14, cy+38, 8,  flameH-5, {255,200,50,255});
    fillRect(r, cx+6,  cy+38, 8,  flameH-5, {255,200,50,255});
    
    // Wings
    // Left wing
    SDL_Point leftWing[4] = {{cx, cy+10}, {cx-50+(int)(tx*5), cy+20}, {cx-45+(int)(tx*5), cy+30}, {cx, cy+25}};
    setColor(r, C_JET);
    for (int i = 0; i < 3; i++)
        SDL_RenderDrawLine(r, leftWing[i].x, leftWing[i].y, leftWing[i+1].x, leftWing[i+1].y);
    fillRect(r, cx-50+(int)(tx*5), cy+20, 50, 10, C_JET);
    
    // Right wing
    SDL_Point rightWing[4] = {{cx, cy+10}, {cx+50+(int)(tx*5), cy+20}, {cx+45+(int)(tx*5), cy+30}, {cx, cy+25}};
    for (int i = 0; i < 3; i++)
        SDL_RenderDrawLine(r, rightWing[i].x, rightWing[i].y, rightWing[i+1].x, rightWing[i+1].y);
    fillRect(r, cx, cy+20, 50+(int)(tx*5), 10, C_JET);
    
    // Fuselage
    fillRect(r, cx-12, cy-40, 24, 80, C_JET);
    fillRect(r, cx-8,  cy-50, 16, 15, C_JET);
    
    // Nose cone
    for (int i = 0; i < 12; i++) {
        fillRect(r, cx - (6-i/2), cy-50-i, (6-i/2)*2, 1, C_JET);
    }
    
    // Cockpit
    fillRect(r, cx-7, cy-35, 14, 18, C_JET_DARK);
    fillRect(r, cx-5, cy-32, 10, 12, C_CYAN);
    
    // Tail fins
    fillRect(r, cx-20, cy+30, 8, 15, C_JET_DARK);
    fillRect(r, cx+12, cy+30, 8, 15, C_JET_DARK);
    
    // Gun barrels
    fillRect(r, cx-15, cy-5, 4, 20, C_JET_DARK);
    fillRect(r, cx+11, cy-5, 4, 20, C_JET_DARK);
    
    // Wing tip lights (blink)
    Color tipCol = ((int)(SDL_GetTicks()/200) % 2) ? C_RED : C_WHITE;
    fillRect(r, cx-50+(int)(tx*5), cy+22, 5, 5, tipCol);
    fillRect(r, cx+45+(int)(tx*5), cy+22, 5, 5, {0,255,80,255});
}

// ==================== DRAW ENEMY JET ====================
void drawEnemyJet(SDL_Renderer* r, EnemyJet& e) {
    int cx = (int)e.x, cy = (int)e.y;
    Color mainCol = C_ENEMY;
    float hpRatio = (float)e.hp / e.maxHp;
    
    if (e.type == 3) { // Boss - larger and more complex
        mainCol = { 150, 0, 200, 255 };
        // Large wings
        fillRect(r, cx-80, cy, 80, 25, mainCol);
        fillRect(r, cx, cy, 80, 25, mainCol);
        // Body
        fillRect(r, cx-20, cy-60, 40, 100, mainCol);
        // Nose
        for (int i = 0; i < 20; i++)
            fillRect(r, cx-(10-i/2), cy-60-i, (10-i/2)*2, 1, mainCol);
        // Cockpit
        fillRect(r, cx-10, cy-50, 20, 25, {50, 0, 100, 255});
        fillRect(r, cx-7, cy-47, 14, 18, {0, 150, 255, 255});
        // Cannons
        fillRect(r, cx-30, cy+10, 6, 30, {100, 0, 150, 255});
        fillRect(r, cx+24, cy+10, 6, 30, {100, 0, 150, 255});
        // HP bar for boss
        fillRect(r, 50, 10, SCREEN_W-100, 20, {60,0,0,255});
        fillRect(r, 50, 10, (int)((SCREEN_W-100)*hpRatio), 20, {200,0,50,255});
        drawPixelText(r, "BOSS", SCREEN_W/2-24, 12, 4, C_WHITE);
    } else {
        // Normal enemy jets
        Color darkCol = { (Uint8)(mainCol.r/2), (Uint8)(mainCol.g/2), (Uint8)(mainCol.b/2), 255 };
        
        int scale = (e.type == 2) ? 2 : 1; // heavy is bigger
        
        // Inverted wings (pointing down = enemy)
        fillRect(r, cx-30*scale, cy, 30*scale, 8*scale, mainCol);
        fillRect(r, cx, cy, 30*scale, 8*scale, mainCol);
        
        // Body
        fillRect(r, cx-8*scale, cy-30*scale, 16*scale, 50*scale, mainCol);
        
        // Inverted nose (points down toward player)
        for (int i = 0; i < 10*scale; i++)
            fillRect(r, cx-(5*scale-i/2), cy+20*scale+i, (5*scale-i/2)*2, 1, mainCol);
        
        // Cockpit
        fillRect(r, cx-5*scale, cy-20*scale, 10*scale, 14*scale, darkCol);
        fillRect(r, cx-3*scale, cy-18*scale, 6*scale, 10*scale, 
                 e.type==1 ? Color{255,50,50,255} : Color{255,150,0,255});
        
        // Thrusters (at top since inverted)
        int fH = 8 + (int)(sinf(SDL_GetTicks()*0.01f)*4);
        fillRect(r, cx-6*scale, cy-30*scale-fH, 12*scale, fH, {255,140,0,255});
    }
    
    // HP bar (small, above enemy)
    if (e.type != 3) {
        int bw = 40;
        fillRect(r, cx-bw/2, cy-35, bw, 5, {60,0,0,255});
        fillRect(r, cx-bw/2, cy-35, (int)(bw*hpRatio), 5, {0,255,80,255});
    }
}

// ==================== DRAW POWERUP ====================
void drawPowerup(SDL_Renderer* r, PowerUp& p) {
    int cx = (int)p.x;
    int cy = (int)(p.y + sinf(p.bob * 3) * 5);
    int sz = 22;
    
    Color col;
    const char* label;
    switch(p.type) {
        case 0: col = C_GREEN;  label = "HP";  break;
        case 1: col = C_CYAN;   label = "SH";  break;
        case 2: col = C_GOLD;   label = "RF";  break;
        case 3: col = C_MISSILE;label = "MS";  break;
        case 4: col = C_PURPLE; label = "BM";  break;
        default: col = C_WHITE; label = "??"; break;
    }
    
    // Glow
    Color glowCol = {col.r, col.g, col.b, 60};
    drawCircle(r, cx, cy, sz+5, glowCol);
    
    // Hexagon-like shape
    drawCircle(r, cx, cy, sz, col);
    drawRing(r, cx, cy, sz, sz-3, {255,255,255,180});
    
    // Label
    Color darkCol = {(Uint8)(col.r/3), (Uint8)(col.g/3), (Uint8)(col.b/3), 255};
    drawPixelText(r, label, cx-8, cy-5, 2, darkCol);
    
    // Spinning effect
    float angle = SDL_GetTicks() * 0.002f;
    for (int i = 0; i < 6; i++) {
        float a = angle + i * (M_PI / 3);
        int ex = cx + (int)(cosf(a) * (sz+8));
        int ey = cy + (int)(sinf(a) * (sz+8));
        fillRect(r, ex-2, ey-2, 4, 4, col);
    }
}

// ==================== DRAW BACKGROUND (3D-like) ====================
void drawBackground(Game& g) {
    SDL_Renderer* r = g.renderer;
    
    // Sky gradient
    int skyEnd = (int)(PLAY_H * 0.65f);
    for (int y = 0; y < skyEnd; y++) {
        float t = (float)y / skyEnd;
        Uint8 R = (Uint8)lerp(C_SKY_TOP.r, C_SKY_BTM.r, t);
        Uint8 G = (Uint8)lerp(C_SKY_TOP.g, C_SKY_BTM.g, t);
        Uint8 B = (Uint8)lerp(C_SKY_TOP.b, C_SKY_BTM.b, t);
        SDL_SetRenderDrawColor(r, R, G, B, 255);
        SDL_RenderDrawLine(r, 0, y, SCREEN_W, y);
    }
    
    // Ground (bottom of play area)
    for (int y = skyEnd; y < PLAY_H; y++) {
        float t = (float)(y - skyEnd) / (PLAY_H - skyEnd);
        Uint8 R = (Uint8)lerp(30, 10, t);
        Uint8 G = (Uint8)lerp(100, 50, t);
        Uint8 B = (Uint8)lerp(30, 10, t);
        SDL_SetRenderDrawColor(r, R, G, B, 255);
        SDL_RenderDrawLine(r, 0, y, SCREEN_W, y);
    }
    
    // Stars
    for (auto& s : g.stars) {
        Uint8 v = (Uint8)(255 * s.brightness);
        setColor(r, {v, v, v, 255});
        if (s.size == 1)
            SDL_RenderDrawPoint(r, (int)s.x, (int)s.y);
        else
            fillRect(r, (int)s.x - s.size/2, (int)s.y - s.size/2, s.size, s.size, {v,v,v,255});
    }
    
    // Clouds
    for (auto& c : g.clouds) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        Uint8 a = (Uint8)c.alpha;
        fillRect(r, (int)c.x, (int)c.y, (int)c.w, (int)c.h, {220,220,255,a});
        fillRect(r, (int)c.x+15, (int)c.y-12, (int)(c.w*0.6f), (int)(c.h*0.7f), {240,240,255,a});
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
    
    // Mountains (parallax)
    for (auto& m : g.mountains) {
        int bx = (int)m.x;
        int bh = (int)m.h;
        // Draw triangle mountain
        for (int i = 0; i < bh; i++) {
            int w = (int)((float)(bh-i)/bh * 80);
            fillRect(r, bx - w/2, skyEnd + i - bh, w, 1, m.col);
        }
    }
    
    // 3D grid lines (ground perspective)
    setColor(r, {0, 80, 0, 255});
    int horizon = skyEnd;
    int vp_x = SCREEN_W / 2;
    // Vertical lines
    for (int xi = -6; xi <= 6; xi++) {
        int gx = vp_x + xi * 80;
        SDL_RenderDrawLine(r, gx, horizon, vp_x + xi * 400, PLAY_H);
    }
    // Horizontal lines (perspective)
    for (int i = 1; i <= 8; i++) {
        float t = (float)i / 8;
        int gy = horizon + (int)((PLAY_H - horizon) * t * t);
        int lw = (int)(SCREEN_W * t);
        SDL_RenderDrawLine(r, vp_x - lw/2, gy, vp_x + lw/2, gy);
    }
}

// ==================== DRAW HUD ====================
void drawHUD(Game& g) {
    SDL_Renderer* r = g.renderer;
    Player& p = g.player;
    
    int hudY = PLAY_H;
    
    // HUD background
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    fillRect(r, 0, hudY, SCREEN_W, HUD_H, {0,0,20,230});
    // Top border glow
    fillRect(r, 0, hudY, SCREEN_W, 3, C_CYAN);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    
    // === HP Bar ===
    int barW = 180, barH = 18;
    int barX = 10, barY = hudY + 10;
    fillRect(r, barX, barY, barW, barH, {60,0,0,255});
    int hpW = (int)(barW * (float)p.hp / p.maxHp);
    Color hpCol = p.hp > 50 ? Color{0,220,80,255} : p.hp > 25 ? Color{255,180,0,255} : Color{255,50,50,255};
    fillRect(r, barX, barY, hpW, barH, hpCol);
    drawRing(r, barX+barW/2, barY+barH/2, barH/2+1, 0, {255,255,255,40});
    drawPixelText(r, "HP", barX+2, barY+2, 3, C_WHITE);
    
    // === Shield Bar ===
    barY += barH + 5;
    fillRect(r, barX, barY, barW, barH, {0,0,60,255});
    int shW = (int)(barW * (float)p.shield / p.maxShield);
    fillRect(r, barX, barY, shW, barH, C_CYAN);
    drawPixelText(r, "SH", barX+2, barY+2, 3, C_WHITE);
    
    // === Score ===
    char scoreBuf[32];
    snprintf(scoreBuf, 32, "SCORE %d", p.score);
    drawPixelText(r, scoreBuf, SCREEN_W/2 - 60, hudY+8, 3, C_GOLD);
    
    // === Level ===
    char lvlBuf[16];
    snprintf(lvlBuf, 16, "LV %d", p.level);
    drawPixelText(r, lvlBuf, SCREEN_W/2 - 30, hudY+28, 3, C_CYAN);
    
    // === Lives ===
    for (int i = 0; i < p.lives; i++) {
        // Draw tiny jet icon
        int lx = SCREEN_W - 20 - i * 28;
        int ly = hudY + 10;
        fillRect(r, lx-4, ly, 8, 14, C_JET);
        fillRect(r, lx-10, ly+5, 8, 5, C_JET);
        fillRect(r, lx+2, ly+5, 8, 5, C_JET);
    }
    
    // === Ammo / Bombs ===
    char ammoBuf[32];
    snprintf(ammoBuf, 32, "MS %d  BM %d", p.ammo, p.bombs);
    drawPixelText(r, ammoBuf, 10, hudY + 55, 3, C_WHITE);
    
    // === Combo ===
    if (g.combo > 1) {
        char comboBuf[16];
        snprintf(comboBuf, 16, "X%d COMBO", g.combo);
        drawPixelText(r, comboBuf, SCREEN_W/2 - 60, hudY + 55, 3, C_GOLD);
    }
    
    // === Rapid Fire indicator ===
    if (p.rapidFire) {
        drawPixelText(r, "RAPID!", SCREEN_W - 90, hudY + 55, 3, C_GOLD);
    }
    
    // === Action Buttons ===
    // Fire button (right side)
    g.btnFire = { SCREEN_W - 130, hudY + 80, 100, 60 };
    drawCircle(r, g.btnFire.x + g.btnFire.w/2, g.btnFire.y + g.btnFire.h/2, 42, {200, 50, 50, 200});
    drawRing(r, g.btnFire.x + g.btnFire.w/2, g.btnFire.y + g.btnFire.h/2, 42, 38, C_WHITE);
    drawPixelText(r, "FIRE", g.btnFire.x+10, g.btnFire.y+22, 4, C_WHITE);
    
    // Missile button
    g.btnMissile = { SCREEN_W - 260, hudY + 90, 80, 50 };
    Color msCol = p.ammo > 0 ? C_MISSILE : Color{80,80,80,255};
    drawCircle(r, g.btnMissile.x + g.btnMissile.w/2, g.btnMissile.y + g.btnMissile.h/2, 30, msCol);
    drawRing(r, g.btnMissile.x + g.btnMissile.w/2, g.btnMissile.y + g.btnMissile.h/2, 30, 27, C_WHITE);
    drawPixelText(r, "MS", g.btnMissile.x+12, g.btnMissile.y+15, 4, C_WHITE);
    
    // Bomb button
    g.btnBomb = { 20, hudY + 90, 80, 50 };
    Color bmCol = p.bombs > 0 ? C_PURPLE : Color{80,80,80,255};
    drawCircle(r, g.btnBomb.x + g.btnBomb.w/2, g.btnBomb.y + g.btnBomb.h/2, 30, bmCol);
    drawRing(r, g.btnBomb.x + g.btnBomb.w/2, g.btnBomb.y + g.btnBomb.h/2, 30, 27, C_WHITE);
    drawPixelText(r, "BM", g.btnBomb.x+12, g.btnBomb.y+15, 4, C_WHITE);
    
    // Pause button
    g.btnPause = { SCREEN_W/2 - 30, hudY + 88, 60, 40 };
    fillRect(r, g.btnPause.x, g.btnPause.y, g.btnPause.w, g.btnPause.h, {40,40,80,200});
    drawPixelText(r, "II", g.btnPause.x+12, g.btnPause.y+8, 4, C_WHITE);
}

// ==================== DRAW MENU ====================
void drawMenu(Game& g) {
    SDL_Renderer* r = g.renderer;
    float t = g.menuAnim;
    
    // Background
    drawBackground(g);
    
    // Animated stars glow
    for (int i = 0; i < 5; i++) {
        float a = t * 0.5f + i * 1.2f;
        int sx = (int)(SCREEN_W * 0.5f + cosf(a) * 200);
        int sy = (int)(PLAY_H * 0.3f + sinf(a * 1.3f) * 100);
        drawCircle(r, sx, sy, 3, C_GOLD);
    }
    
    // Title Panel
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    fillRect(r, 30, 80, SCREEN_W-60, 200, {0,0,40,200});
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    fillRect(r, 30, 80, SCREEN_W-60, 4, C_CYAN);
    fillRect(r, 30, 276, SCREEN_W-60, 4, C_CYAN);
    
    // Title text (big)
    drawPixelText(r, "FIGHTER", 60, 110, 10, C_GOLD);
    drawPixelText(r, "JET 3D", 80, 175, 10, C_CYAN);
    
    // Animated jet on menu
    float jetY = 350.0f + sinf(t * 2) * 20;
    drawPlayerJet(r, SCREEN_W/2.0f, jetY, sinf(t*0.5f)*0.3f, t, 0);
    
    // Blink "TAP TO START"
    if ((int)(t * 2) % 2 == 0) {
        drawPixelText(r, "TAP TO START", 80, 520, 5, C_WHITE);
    }
    
    // High score
    if (g.highScore > 0) {
        char hsBuf[32];
        snprintf(hsBuf, 32, "BEST %d", g.highScore);
        drawPixelText(r, hsBuf, SCREEN_W/2-60, 580, 4, C_GOLD);
    }
    
    // Controls hint
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    fillRect(r, 20, 640, SCREEN_W-40, 120, {0,0,30,180});
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    drawPixelText(r, "DRAG TO MOVE", 60, 655, 3, C_CYAN);
    drawPixelText(r, "FIRE - SHOOT GUNS", 60, 680, 3, C_WHITE);
    drawPixelText(r, "MS   - FIRE MISSILE", 60, 700, 3, C_MISSILE);
    drawPixelText(r, "BM   - SCREEN BOMB", 60, 720, 3, C_PURPLE);
    
    // Version
    drawPixelText(r, "V1.0 OPPO A5 5G", 100, PLAY_H-30, 3, {100,100,100,255});
}

// ==================== DRAW GAME OVER ====================
void drawGameOver(Game& g) {
    SDL_Renderer* r = g.renderer;
    drawBackground(g);
    
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    fillRect(r, 0, 0, SCREEN_W, PLAY_H, {0,0,0,150});
    fillRect(r, 40, 200, SCREEN_W-80, 600, {20,0,0,220});
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    
    fillRect(r, 40, 200, SCREEN_W-80, 4, C_RED);
    fillRect(r, 40, 796, SCREEN_W-80, 4, C_RED);
    
    drawPixelText(r, "GAME", 90, 240, 12, C_RED);
    drawPixelText(r, "OVER", 90, 340, 12, C_RED);
    
    char scoreBuf[32], killBuf[32], lvlBuf[32];
    snprintf(scoreBuf, 32, "SCORE  %d", g.player.score);
    snprintf(killBuf, 32, "KILLS  %d", g.player.kills);
    snprintf(lvlBuf, 32, "LEVEL  %d", g.player.level);
    
    drawPixelText(r, scoreBuf, 80, 480, 4, C_GOLD);
    drawPixelText(r, killBuf,  80, 520, 4, C_WHITE);
    drawPixelText(r, lvlBuf,   80, 560, 4, C_CYAN);
    
    if (g.player.score >= g.highScore) {
        drawPixelText(r, "NEW RECORD!", 80, 610, 5, C_GOLD);
    }
    
    if ((int)(g.gameoverTimer * 2) % 2 == 0)
        drawPixelText(r, "TAP TO RESTART", 70, 680, 4, C_WHITE);
}

// ==================== DRAW PAUSE ====================
void drawPause(Game& g) {
    SDL_Renderer* r = g.renderer;
    
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    fillRect(r, 0, 0, SCREEN_W, SCREEN_H, {0,0,0,150});
    fillRect(r, 80, 400, SCREEN_W-160, 400, {0,20,60,230});
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    
    fillRect(r, 80, 400, SCREEN_W-160, 4, C_CYAN);
    fillRect(r, 80, 796, SCREEN_W-160, 4, C_CYAN);
    
    drawPixelText(r, "PAUSED", 120, 450, 8, C_CYAN);
    drawPixelText(r, "TAP TO RESUME", 80, 600, 4, C_WHITE);
    
    char scoreBuf[32];
    snprintf(scoreBuf, 32, "SCORE %d", g.player.score);
    drawPixelText(r, scoreBuf, 100, 680, 4, C_GOLD);
}

// ==================== UPDATE GAME ====================
void playerShoot(Game& g) {
    Player& p = g.player;
    if (p.shootTimer > 0) return;
    
    float cd = p.rapidFire ? 0.05f : p.shootCooldown;
    p.shootTimer = cd;
    
    // Double barrel
    spawnBullet(g, p.x-15, p.y-40, 0, -700, false, C_BULLET, 10);
    spawnBullet(g, p.x+15, p.y-40, 0, -700, false, C_BULLET, 10);
    
    // Screen flash (tiny)
    g.shakeAmt = 2;
    g.shakeTimer = 0.03f;
}

void playerFireMissile(Game& g) {
    Player& p = g.player;
    if (p.ammo <= 0) return;
    p.ammo--;
    
    // Find nearest enemy
    float nearDist = 9999;
    float tx = p.x, ty = -100;
    for (auto& e : g.enemies) {
        if (!e.active) continue;
        float d = dist2D(p.x, p.y, e.x, e.y);
        if (d < nearDist) { nearDist = d; tx = e.x; ty = e.y; }
    }
    spawnMissile(g, p.x, p.y-40, tx, ty, false, 50);
}

void playerBomb(Game& g) {
    Player& p = g.player;
    if (p.bombs <= 0) return;
    p.bombs--;
    
    // Destroy/damage all enemies
    for (auto& e : g.enemies) {
        if (!e.active) continue;
        e.hp -= 150;
        spawnExplosion(g, e.x, e.y, 60, C_FIRE);
        if (e.hp <= 0) {
            e.active = false;
            p.score += e.score;
            p.kills++;
            g.combo++;
            g.comboTimer = 2.0f;
        }
    }
    
    // Big screen flash
    g.shakeAmt = 20;
    g.shakeTimer = 0.5f;
    spawnExplosion(g, SCREEN_W/2, PLAY_H/2, 300, C_PURPLE);
}

void updateGame(Game& g) {
    float dt = g.dt;
    Player& p = g.player;
    
    // Timers
    p.shootTimer   = std::max(0.0f, p.shootTimer - dt);
    p.invTimer     = std::max(0.0f, p.invTimer - dt);
    p.rapidTimer   = std::max(0.0f, p.rapidTimer - dt);
    if (p.rapidTimer <= 0) p.rapidFire = false;
    p.thrusterAnim += dt;
    p.tiltX = clamp(p.tiltX * 0.9f, -1, 1); // decay tilt
    
    g.shakeTimer = std::max(0.0f, g.shakeTimer - dt);
    g.comboTimer = std::max(0.0f, g.comboTimer - dt);
    if (g.comboTimer <= 0) g.combo = 0;
    
    g.gameTime += dt;
    
    // Player movement (smooth follow touch)
    if (p.dragging) {
        float dx = g.touchX - p.x;
        float dy = g.touchY - p.y;
        p.tiltX = clamp(dx / 100.0f, -1, 1);
        p.x += dx * dt * 8.0f;
        p.y += dy * dt * 8.0f;
    }
    p.x = clamp(p.x, 40, SCREEN_W-40);
    p.y = clamp(p.y, 60, PLAY_H-80);
    
    // Auto-fire when dragging
    if (p.dragging) playerShoot(g);
    
    // Background scroll
    for (auto& s : g.stars) {
        s.y += s.speed * dt;
        if (s.y > PLAY_H) { s.y = 0; s.x = (float)(rand() % SCREEN_W); }
    }
    for (auto& c : g.clouds) {
        c.y += c.speed * dt;
        if (c.y > PLAY_H) { c.y = -c.h; c.x = (float)(rand() % SCREEN_W); }
    }
    for (auto& m : g.mountains) {
        m.x -= m.speed * dt;
        if (m.x < -100) {
            m.x = SCREEN_W + 50;
            m.h = 100 + rand() % 150;
        }
    }
    
    // Enemy spawning
    g.enemySpawnTimer += dt;
    float spawnInt = std::max(0.5f, g.enemySpawnInterval - g.gameTime * 0.02f);
    if (g.enemySpawnTimer >= spawnInt) {
        g.enemySpawnTimer = 0;
        int type = 0;
        if (g.gameTime > 60) type = rand() % 3;
        else if (g.gameTime > 30) type = rand() % 2;
        if (!g.bossAlive) spawnEnemy(g, type);
    }
    
    // Boss spawn
    if (g.gameTime > 90 && !g.bossAlive) {
        g.bossAlive = true;
        spawnEnemy(g, 3);
    }
    
    // Powerup spawning
    g.powerupSpawnTimer += dt;
    if (g.powerupSpawnTimer > 12.0f) {
        g.powerupSpawnTimer = 0;
        spawnPowerup(g, 60 + rand() % (SCREEN_W-120), -50);
    }
    
    // Update bullets
    for (auto& b : g.bullets) {
        if (!b.active) continue;
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        if (b.y < -20 || b.y > PLAY_H+20 || b.x < -20 || b.x > SCREEN_W+20)
            b.active = false;
        
        if (!b.isEnemy) {
            // Check enemy hits
            for (auto& e : g.enemies) {
                if (!e.active) continue;
                int hitR = (e.type == 3) ? 50 : 30;
                if (dist2D(b.x, b.y, e.x, e.y) < hitR) {
                    b.active = false;
                    e.hp -= b.damage;
                    spawnExplosion(g, b.x, b.y, 20, C_FIRE);
                    if (e.hp <= 0) {
                        e.active = false;
                        p.score += e.score * (1 + g.combo/5);
                        p.kills++;
                        g.combo++;
                        g.comboTimer = 2.0f;
                        if (e.type == 3) { g.bossAlive = false; p.level++; }
                        spawnExplosion(g, e.x, e.y, (e.type == 3) ? 120 : 50, C_FIRE);
                        if (rand() % 3 == 0) spawnPowerup(g, e.x, e.y);
                    }
                }
            }
        } else {
            // Check player hit
            if (p.invTimer <= 0 && dist2D(b.x, b.y, p.x, p.y) < 30) {
                b.active = false;
                if (p.shieldActive && p.shield > 0) {
                    p.shield -= b.damage;
                    if (p.shield < 0) p.shield = 0;
                } else {
                    p.hp -= b.damage;
                }
                p.invTimer = 0.5f;
                spawnExplosion(g, p.x, p.y, 25, {100,100,255,255});
                if (p.hp <= 0) {
                    p.lives--;
                    if (p.lives <= 0) {
                        if (p.score > g.highScore) g.highScore = p.score;
                        g.state = STATE_GAMEOVER;
                        g.gameoverTimer = 0;
                    } else {
                        p.hp = p.maxHp;
                        p.invTimer = 3.0f;
                    }
                }
            }
        }
    }
    
    // Update missiles
    for (auto& m : g.missiles) {
        if (!m.active) continue;
        m.life -= dt;
        if (m.life <= 0) { m.active = false; continue; }
        
        // Homing
        if (!m.isEnemy && !g.enemies.empty()) {
            float nearD = 9999;
            EnemyJet* target = nullptr;
            for (auto& e : g.enemies) {
                if (!e.active) continue;
                float d = dist2D(m.x, m.y, e.x, e.y);
                if (d < nearD) { nearD = d; target = &e; }
            }
            if (target) {
                float dx = target->x - m.x, dy = target->y - m.y;
                float len = sqrtf(dx*dx+dy*dy);
                if (len > 0) { dx /= len; dy /= len; }
                m.vx = lerp(m.vx, dx*500, dt*3);
                m.vy = lerp(m.vy, dy*500, dt*3);
            }
        }
        
        m.x += m.vx * dt;
        m.y += m.vy * dt;
        
        // Hit detection
        if (!m.isEnemy) {
            for (auto& e : g.enemies) {
                if (!e.active) continue;
                int hitR = (e.type == 3) ? 60 : 35;
                if (dist2D(m.x, m.y, e.x, e.y) < hitR) {
                    m.active = false;
                    e.hp -= m.damage;
                    spawnExplosion(g, m.x, m.y, 60, C_FIRE);
                    if (e.hp <= 0) {
                        e.active = false;
                        p.score += e.score * 2;
                        p.kills++;
                        if (e.type == 3) g.bossAlive = false;
                        if (rand() % 2 == 0) spawnPowerup(g, e.x, e.y);
                    }
                }
            }
        } else {
            if (p.invTimer <= 0 && dist2D(m.x, m.y, p.x, p.y) < 35) {
                m.active = false;
                p.hp -= m.damage;
                p.invTimer = 1.0f;
                spawnExplosion(g, p.x, p.y, 60, {100,100,255,255});
                if (p.hp <= 0) {
                    p.lives--;
                    if (p.lives <= 0) { g.state = STATE_GAMEOVER; g.gameoverTimer = 0; }
                    else { p.hp = p.maxHp; p.invTimer = 3.0f; }
                }
            }
        }
        
        if (m.y < -50 || m.y > PLAY_H+50 || m.x < -50 || m.x > SCREEN_W+50)
            m.active = false;
    }
    
    // Update enemies
    for (auto& e : g.enemies) {
        if (!e.active) continue;
        
        e.moveTimer += dt;
        
        // Boss movement pattern
        if (e.type == 3) {
            e.x += e.vx * dt;
            e.y += e.vy * dt * 0.2f;
            if (e.x < 80 || e.x > SCREEN_W-80) e.vx = -e.vx;
            if (e.y > 200) e.vy = -abs(e.vy);
            if (e.y < 50) e.vy = abs(e.vy);
        } else {
            e.x += e.vx * dt;
            e.y += e.vy * dt;
            // Bounce off walls
            if (e.x < 30 || e.x > SCREEN_W-30) e.vx = -e.vx;
            // Wavey movement
            e.x += sinf(e.moveTimer * 2) * 30 * dt;
        }
        
        // Off screen
        if (e.y > PLAY_H + 100) { e.active = false; continue; }
        
        // Enemy shooting
        e.shootTimer -= dt;
        if (e.shootTimer <= 0) {
            e.shootTimer = e.shootInterval;
            float dx = p.x - e.x;
            float dy = p.y - e.y;
            float len = sqrtf(dx*dx+dy*dy);
            if (len > 0) { dx /= len; dy /= len; }
            
            float spd = (e.type == 3) ? 350.0f : 250.0f;
            spawnBullet(g, e.x, e.y, dx*spd, dy*spd, true, {255,50,50,255}, e.type==3 ? 20 : 10);
            
            if (e.type == 3 && g.gameTime > 60) {
                // Boss fires spread
                for (int i = -2; i <= 2; i++) {
                    float a = atan2f(dy, dx) + i * 0.3f;
                    spawnBullet(g, e.x, e.y, cosf(a)*300, sinf(a)*300, true, {255,100,0,255}, 15);
                }
            }
            
            if (e.type == 2 && rand()%3==0) {
                spawnMissile(g, e.x, e.y, p.x, p.y, true, 25);
            }
        }
    }
    
    // Update explosions
    for (auto& ex : g.explosions) {
        ex.life -= dt;
        float t = 1.0f - ex.life / ex.maxLife;
        ex.radius = ex.maxRadius * t;
    }
    g.explosions.erase(
        std::remove_if(g.explosions.begin(), g.explosions.end(),
                       [](const Explosion& e){ return e.life <= 0; }),
        g.explosions.end());
    
    // Cleanup
    g.bullets.erase(std::remove_if(g.bullets.begin(), g.bullets.end(),
                    [](const Bullet& b){ return !b.active; }), g.bullets.end());
    g.missiles.erase(std::remove_if(g.missiles.begin(), g.missiles.end(),
                     [](const Missile& m){ return !m.active; }), g.missiles.end());
    g.enemies.erase(std::remove_if(g.enemies.begin(), g.enemies.end(),
                    [](const EnemyJet& e){ return !e.active; }), g.enemies.end());
    
    // Update powerups
    for (auto& pu : g.powerups) {
        if (!pu.active) continue;
        pu.y += pu.vy * dt;
        pu.bob += dt;
        if (pu.y > PLAY_H + 50) { pu.active = false; continue; }
        
        // Player collect
        if (dist2D(pu.x, pu.y, p.x, p.y) < 40) {
            pu.active = false;
            switch(pu.type) {
                case 0: p.hp = std::min(p.maxHp, p.hp + 30); break;
                case 1: p.shield = std::min(p.maxShield, p.shield + 30); p.shieldActive = true; p.shieldTimer = 5.0f; break;
                case 2: p.rapidFire = true; p.rapidTimer = 8.0f; break;
                case 3: p.ammo += 5; break;
                case 4: p.bombs++; break;
            }
            spawnExplosion(g, pu.x, pu.y, 30, C_GREEN);
            p.score += 50;
        }
    }
    g.powerups.erase(std::remove_if(g.powerups.begin(), g.powerups.end(),
                     [](const PowerUp& p){ return !p.active; }), g.powerups.end());
    
    // Shield timer
    p.shieldTimer -= dt;
    if (p.shieldTimer <= 0) p.shieldActive = false;
    
    // Level progression
    p.level = 1 + (int)(g.gameTime / 30);
}

// ==================== RENDER GAME ====================
void renderGame(Game& g) {
    SDL_Renderer* r = g.renderer;
    
    // Screen shake offset
    int shakeX = 0, shakeY = 0;
    if (g.shakeTimer > 0) {
        float amt = g.shakeAmt * (g.shakeTimer / 0.3f);
        shakeX = (int)((rand()%3-1) * amt);
        shakeY = (int)((rand()%3-1) * amt);
    }
    
    // Clipping to play area
    SDL_Rect playArea = {0, 0, SCREEN_W, PLAY_H};
    SDL_RenderSetClipRect(r, &playArea);
    
    // Translate for shake
    SDL_RenderSetScale(r, 1.0f, 1.0f);
    
    // Draw background
    drawBackground(g);
    
    // Draw powerups
    for (auto& pu : g.powerups)
        if (pu.active) drawPowerup(r, pu);
    
    // Draw enemy bullets
    for (auto& b : g.bullets) {
        if (!b.active) continue;
        if (b.isEnemy) {
            drawCircle(r, (int)b.x, (int)b.y, 6, b.col);
            fillRect(r, (int)b.x-2, (int)b.y-2, 4, 4, C_WHITE);
        }
    }
    
    // Draw player bullets
    for (auto& b : g.bullets) {
        if (!b.active || b.isEnemy) continue;
        fillRect(r, (int)b.x-3, (int)b.y-12, 6, 16, b.col);
        fillRect(r, (int)b.x-1, (int)b.y-14, 2, 4, C_WHITE);
    }
    
    // Draw missiles
    for (auto& m : g.missiles) {
        if (!m.active) continue;
        // Draw missile body
        float angle = atan2f(m.vy, m.vx);
        int mx = (int)m.x, my = (int)m.y;
        fillRect(r, mx-3, my-10, 6, 20, m.isEnemy ? Color{255,80,0,255} : C_MISSILE);
        // Flame trail
        fillRect(r, mx-2, my+10, 4, 10, C_FIRE);
        drawCircle(r, mx, my, 8, {255,100,0,80});
    }
    
    // Draw enemies
    for (auto& e : g.enemies)
        if (e.active) drawEnemyJet(r, e);
    
    // Draw player jet
    drawPlayerJet(r, g.player.x, g.player.y, g.player.tiltX, g.player.thrusterAnim, g.player.invTimer);
    
    // Shield effect
    if (g.player.shieldActive && g.player.shield > 0) {
        float pulse = 0.7f + 0.3f * sinf(g.gameTime * 5);
        Uint8 alpha = (Uint8)(150 * pulse);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        drawRing(r, (int)g.player.x, (int)g.player.y, 55, 48, {0, 200, 255, alpha});
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    }
    
    // Draw explosions
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (auto& ex : g.explosions) {
        float alpha = ex.life / ex.maxLife;
        Uint8 a = (Uint8)(255 * alpha);
        Color c = {ex.col.r, ex.col.g, ex.col.b, a};
        drawCircle(r, (int)ex.x, (int)ex.y, (int)ex.radius, c);
        // Inner bright
        Color inner = {255, 255, 200, (Uint8)(a*0.7f)};
        drawCircle(r, (int)ex.x, (int)ex.y, (int)(ex.radius*0.5f), inner);
        // Sparks
        for (int i = 0; i < 8; i++) {
            float sa = i * M_PI / 4 + g.gameTime * 2;
            float sr = ex.radius * 1.2f;
            int sx = (int)(ex.x + cosf(sa)*sr);
            int sy = (int)(ex.y + sinf(sa)*sr);
            fillRect(r, sx-2, sy-2, 4, 4, {255,200,50,a});
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    
    // Combo display
    if (g.combo > 1) {
        char comboBuf[16];
        snprintf(comboBuf, 16, "X%d!", g.combo);
        int cx = (int)g.player.x - 30;
        int cy = (int)g.player.y - 100;
        drawPixelText(r, comboBuf, cx, cy, 6, C_GOLD);
    }
    
    SDL_RenderSetClipRect(r, nullptr);
    
    // Draw HUD (outside clip)
    drawHUD(g);
}

// ==================== HANDLE INPUT ====================
bool pointInRect(int x, int y, SDL_Rect& rect) {
    return x >= rect.x && x <= rect.x+rect.w && y >= rect.y && y <= rect.y+rect.h;
}

void handleTouch(Game& g, SDL_Event& ev) {
    int tx, ty;
    
    if (ev.type == SDL_FINGERDOWN || ev.type == SDL_FINGERMOTION) {
        tx = (int)(ev.tfinger.x * SCREEN_W);
        ty = (int)(ev.tfinger.y * SCREEN_H);
    } else if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEMOTION) {
        tx = ev.button.x;
        ty = ev.button.y;
    } else return;
    
    if (g.state == STATE_MENU) {
        g.state = STATE_PLAYING;
        return;
    }
    if (g.state == STATE_GAMEOVER) {
        // Restart
        g.player.score = 0;
        g.enemies.clear();
        g.bullets.clear();
        g.missiles.clear();
        g.explosions.clear();
        g.powerups.clear();
        g.gameTime = 0;
        g.bossAlive = false;
        initPlayer(g.player);
        g.state = STATE_PLAYING;
        return;
    }
    if (g.state == STATE_PAUSED) {
        g.state = STATE_PLAYING;
        return;
    }
    if (g.state != STATE_PLAYING) return;
    
    // Button checks
    if (pointInRect(tx, ty, g.btnFire)) {
        playerShoot(g);
        return;
    }
    if (pointInRect(tx, ty, g.btnMissile)) {
        playerFireMissile(g);
        return;
    }
    if (pointInRect(tx, ty, g.btnBomb)) {
        playerBomb(g);
        return;
    }
    if (pointInRect(tx, ty, g.btnPause)) {
        g.state = STATE_PAUSED;
        return;
    }
    
    // Move player
    if (ty < PLAY_H) {
        g.touchX = tx;
        g.touchY = ty;
        g.player.dragging = true;
    }
}

void handleTouchUp(Game& g) {
    g.player.dragging = false;
}

// ==================== MAIN ====================
int main(int argc, char* argv[]) {
    srand((unsigned)time(nullptr));
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    
    Game game;
    g_game = &game;
    
    // Create window - fullscreen for mobile
    game.window = SDL_CreateWindow(
        "Fighter Jet 3D",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP
    );
    
    if (!game.window) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        return 1;
    }
    
    game.renderer = SDL_CreateRenderer(game.window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    if (!game.renderer) {
        SDL_Log("Renderer creation failed: %s", SDL_GetError());
        return 1;
    }
    
    // Scale to fit Oppo A5 5G screen
    SDL_RenderSetLogicalSize(game.renderer, SCREEN_W, SCREEN_H);
    SDL_SetRenderDrawBlendMode(game.renderer, SDL_BLENDMODE_BLEND);
    
    // Init game objects
    initPlayer(game.player);
    initStars(game.stars);
    initClouds(game.clouds);
    initMountains(game.mountains);
    
    game.lastTime = SDL_GetTicks();
    
    // HUD button rects (initial, updated in drawHUD)
    game.btnFire    = { SCREEN_W-130, PLAY_H+80, 100, 60 };
    game.btnMissile = { SCREEN_W-260, PLAY_H+90,  80, 50 };
    game.btnBomb    = {  20, PLAY_H+90,  80, 50 };
    game.btnPause   = { SCREEN_W/2-30, PLAY_H+88, 60, 40 };
    
    bool running = true;
    SDL_Event ev;
    
    while (running) {
        // Delta time
        Uint32 now = SDL_GetTicks();
        game.dt = (now - game.lastTime) / 1000.0f;
        if (game.dt > 0.05f) game.dt = 0.05f; // cap at 20fps min
        game.lastTime = now;
        game.menuAnim += game.dt;
        
        // Events
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (ev.key.keysym.sym == SDLK_ESCAPE) running = false;
                    if (ev.key.keysym.sym == SDLK_SPACE && game.state == STATE_PLAYING)
                        playerShoot(game);
                    if (ev.key.keysym.sym == SDLK_m && game.state == STATE_PLAYING)
                        playerFireMissile(game);
                    if (ev.key.keysym.sym == SDLK_b && game.state == STATE_PLAYING)
                        playerBomb(game);
                    if (ev.key.keysym.sym == SDLK_p) {
                        if (game.state == STATE_PLAYING) game.state = STATE_PAUSED;
                        else if (game.state == STATE_PAUSED) game.state = STATE_PLAYING;
                    }
                    break;
                case SDL_FINGERDOWN:
                case SDL_FINGERMOTION:
                    handleTouch(game, ev);
                    break;
                case SDL_FINGERUP:
                    handleTouchUp(game);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    handleTouch(game, ev);
                    break;
                case SDL_MOUSEBUTTONUP:
                    handleTouchUp(game);
                    break;
                case SDL_MOUSEMOTION:
                    if (ev.motion.state & SDL_BUTTON_LMASK)
                        handleTouch(game, ev);
                    break;
            }
        }
        
        // Update
        switch (game.state) {
            case STATE_MENU:    break;
            case STATE_PLAYING: updateGame(game); break;
            case STATE_PAUSED:  break;
            case STATE_GAMEOVER:
                game.gameoverTimer += game.dt;
                break;
            default: break;
        }
        
        // Render
        SDL_SetRenderDrawColor(game.renderer, 0, 0, 20, 255);
        SDL_RenderClear(game.renderer);
        
        switch (game.state) {
            case STATE_MENU:     drawMenu(game); break;
            case STATE_PLAYING:  renderGame(game); break;
            case STATE_PAUSED:   renderGame(game); drawPause(game); break;
            case STATE_GAMEOVER: drawGameOver(game); break;
            default: break;
        }
        
        SDL_RenderPresent(game.renderer);
    }
    
    SDL_DestroyRenderer(game.renderer);
    SDL_DestroyWindow(game.window);
    SDL_Quit();
    return 0;
}

/*
=======================================================
  ANDROID BUILD GUIDE (Oppo A5 5G)
  
  1. Install Android NDK r21+ and SDL2 for Android
  2. Create Android project structure:
     MyGame/
       jni/
         Android.mk
         Application.mk
         src/
           fighter_jet_game.cpp
       AndroidManifest.xml
  
  3. Android.mk:
     LOCAL_PATH := $(call my-dir)
     include $(CLEAR_VARS)
     LOCAL_MODULE := main
     SDL_PATH := ../SDL2
     LOCAL_C_INCLUDES := $(SDL_PATH)/include
     LOCAL_SRC_FILES := src/fighter_jet_game.cpp
     LOCAL_SHARED_LIBRARIES := SDL2
     LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog -landroid
     include $(BUILD_SHARED_LIBRARY)
  
  4. Application.mk:
     APP_ABI := armeabi-v7a arm64-v8a
     APP_PLATFORM := android-21
     APP_STL := c++_shared
  
  5. Screen orientation in AndroidManifest.xml:
     android:screenOrientation="portrait"
     android:theme="@android:style/Theme.NoTitleBar.Fullscreen"
  
  6. Build:
     ndk-build
     ant debug
  
  Controls:
  - Touch/Drag anywhere in play area to move jet
  - FIRE button  - shoot guns
  - MS button    - fire homing missile
  - BM button    - screen bomb (kills all enemies)
  - II button    - pause
  
  Keyboard (desktop testing):
  - Arrow keys / WASD - move (add if needed)
  - SPACE  - fire
  - M      - missile
  - B      - bomb
  - P      - pause
  - ESC    - quit
=======================================================
*/
