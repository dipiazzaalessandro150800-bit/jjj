// BATTLE ROYALE DS - mini battle royale 3D per Nintendo DS (devkitARM + libnds)
//
// Schermo superiore: mondo 3D in prima persona
// Schermo inferiore: HUD + TOUCH SCREEN PER GUARDARE (trascina il pennino)
//
// Croce direzionale = muoviti      A oppure R = spara
// B = salta                        X = costruisci un muro (come in Fortnite)
// START = ricomincia
//
// Obiettivo: elimina tutti i bot e resta dentro il cerchio della tempesta.

#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define H        2048          // 0.5 in formato v16 (1.0 = 4096)
#define MAXBOX   64
#define NBUILD   8
#define NTREE    14
#define NBOT     7
#define ARENA    46.0f
#define RAD      0.0174533f
#define SEGS     24

typedef struct { float x, z, w, d, h; int r, g, b; } Box;
typedef struct { float x, z; int hp, alive, cd; } Bot;

static Box   boxes[MAXBOX];
static int   nbox, nfixed, wallCount;
static Bot   bots[NBOT];
static float px, pz, py, vy, yaw, pitch, stormR;
static int   hp, state, tick, fireCd, buildCd, hitFlash;   // state: 0 gioco, 1 morto, 2 vittoria
static float cosT[SEGS], sinT[SEGS];

static float rnd(float a, float b) { return a + (b - a) * (rand() % 1000) / 1000.0f; }

static void addBox(float x, float z, float w, float d, float h, int r, int g, int b) {
    if (nbox >= MAXBOX) return;
    Box *q = &boxes[nbox++];
    q->x = x; q->z = z; q->w = w; q->d = d; q->h = h; q->r = r; q->g = g; q->b = b;
}

static int blocked(float x, float z, float feet) {
    for (int i = 0; i < nbox; i++) {
        Box *b = &boxes[i];
        if (feet < b->h &&
            fabsf(x - b->x) < b->w * 0.5f + 0.4f &&
            fabsf(z - b->z) < b->d * 0.5f + 0.4f) return 1;
    }
    return 0;
}

// Raggio contro scatola (slab test 3D)
static int rayHitsBox(float ox, float oy, float oz, float dx, float dy, float dz,
                      const Box *b, float maxT) {
    float lo[3] = { b->x - b->w * 0.5f, 0.0f, b->z - b->d * 0.5f };
    float hi[3] = { b->x + b->w * 0.5f, b->h, b->z + b->d * 0.5f };
    float o[3] = { ox, oy, oz }, d[3] = { dx, dy, dz };
    float tmin = 0.0f, tmax = maxT;
    for (int i = 0; i < 3; i++) {
        if (fabsf(d[i]) < 1e-6f) {
            if (o[i] < lo[i] || o[i] > hi[i]) return 0;
        } else {
            float t1 = (lo[i] - o[i]) / d[i], t2 = (hi[i] - o[i]) / d[i];
            if (t1 > t2) { float t = t1; t1 = t2; t2 = t; }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return 0;
        }
    }
    return 1;
}

static void resetGame(void) {
    nbox = 0; wallCount = 0;

    for (int i = 0; i < NBUILD; i++) {
        for (int t = 0; t < 20; t++) {
            float x = rnd(-38, 38), z = rnd(-38, 38);
            if (sqrtf(x * x + z * z) < 12.0f) continue;
            addBox(x, z, rnd(5, 9), rnd(5, 9), rnd(3, 5), 14, 13, 12);
            break;
        }
    }
    for (int i = 0; i < NTREE; i++) {
        for (int t = 0; t < 20; t++) {
            float x = rnd(-42, 42), z = rnd(-42, 42);
            if (sqrtf(x * x + z * z) < 8.0f) continue;
            addBox(x, z, 1.0f, 1.0f, rnd(3, 5), 3, 14, 4);
            break;
        }
    }
    nfixed = nbox;

    px = pz = py = vy = 0.0f;
    yaw = pitch = 0.0f;
    hp = 100; state = 0; tick = 0;
    fireCd = buildCd = hitFlash = 0;

    for (int i = 0; i < NBOT; i++) {
        for (int t = 0; t < 30; t++) {
            float a = rnd(0, 6.283f), r = rnd(14, 40);
            bots[i].x = cosf(a) * r;
            bots[i].z = sinf(a) * r;
            if (!blocked(bots[i].x, bots[i].z, 0)) break;
        }
        bots[i].hp = 3; bots[i].alive = 1; bots[i].cd = 0;
    }
}

static int botsAlive(void) {
    int n = 0;
    for (int i = 0; i < NBOT; i++) if (bots[i].alive) n++;
    return n;
}

static void shoot(void) {
    float cp = cosf(pitch * RAD);
    float fx = sinf(yaw * RAD) * cp, fy = sinf(pitch * RAD), fz = -cosf(yaw * RAD) * cp;
    float ox = px, oy = py + 1.6f, oz = pz;
    int best = -1; float bestT = 100.0f;

    for (int i = 0; i < NBOT; i++) {
        if (!bots[i].alive) continue;
        float cx = bots[i].x - ox, cy = 0.9f - oy, cz = bots[i].z - oz;
        float t = cx * fx + cy * fy + cz * fz;
        if (t < 0.5f || t > bestT) continue;
        float d2 = cx * cx + cy * cy + cz * cz - t * t;
        if (d2 < 0.49f) { best = i; bestT = t; }
    }
    if (best >= 0) {
        for (int i = 0; i < nbox; i++)
            if (rayHitsBox(ox, oy, oz, fx, fy, fz, &boxes[i], bestT)) { best = -1; break; }
    }
    if (best >= 0) {
        hitFlash = 6;
        if (--bots[best].hp <= 0) bots[best].alive = 0;
    }
}

static void buildWall(void) {
    float s = sinf(yaw * RAD), c = cosf(yaw * RAD);
    float x = px + s * 3.0f, z = pz - c * 3.0f;
    float w = 3.0f, d = 0.3f;
    if (fabsf(s) > fabsf(c)) { w = 0.3f; d = 3.0f; }

    if (nbox < MAXBOX) {
        addBox(x, z, w, d, 3.0f, 20, 14, 6);
    } else if (MAXBOX > nfixed) {
        Box *q = &boxes[nfixed + (wallCount % (MAXBOX - nfixed))];
        q->x = x; q->z = z; q->w = w; q->d = d; q->h = 3.0f; q->r = 20; q->g = 14; q->b = 6;
    }
    wallCount++;
}

static void update(void) {
    scanKeys();
    int down = keysDown(), held = keysHeld();

    if (down & KEY_START) { resetGame(); return; }
    if (state != 0) return;

    // --- Sguardo con il touch screen (trascina il pennino) ---
    static int touching = 0, lastX = 0, lastY = 0;
    if (held & KEY_TOUCH) {
        touchPosition t;
        touchRead(&t);
        if (touching) {
            yaw   += (t.px - lastX) * 0.6f;
            pitch -= (t.py - lastY) * 0.5f;
        }
        lastX = t.px; lastY = t.py; touching = 1;
    } else {
        touching = 0;
    }
    if (yaw >= 360.0f) yaw -= 360.0f;
    if (yaw < 0.0f)    yaw += 360.0f;
    if (pitch > 75.0f)  pitch = 75.0f;
    if (pitch < -75.0f) pitch = -75.0f;

    // --- Movimento ---
    float fwd = ((held & KEY_UP) ? 1.0f : 0.0f) - ((held & KEY_DOWN) ? 1.0f : 0.0f);
    float str = ((held & KEY_RIGHT) ? 1.0f : 0.0f) - ((held & KEY_LEFT) ? 1.0f : 0.0f);
    if (fwd != 0.0f && str != 0.0f) { fwd *= 0.7f; str *= 0.7f; }
    float s = sinf(yaw * RAD), c = cosf(yaw * RAD);
    float mx = (s * fwd + c * str) * 0.1f;
    float mz = (-c * fwd + s * str) * 0.1f;
    if (!blocked(px + mx, pz, py)) px += mx;
    if (!blocked(px, pz + mz, py)) pz += mz;
    if (px >  ARENA) px =  ARENA;
    if (px < -ARENA) px = -ARENA;
    if (pz >  ARENA) pz =  ARENA;
    if (pz < -ARENA) pz = -ARENA;

    // --- Salto ---
    if ((down & KEY_B) && py <= 0.001f) vy = 0.22f;
    py += vy; vy -= 0.012f;
    if (py < 0.0f) { py = 0.0f; vy = 0.0f; }

    // --- Arma e costruzione ---
    if (fireCd > 0) fireCd--;
    if (buildCd > 0) buildCd--;
    if (hitFlash > 0) hitFlash--;
    if ((held & (KEY_A | KEY_R)) && fireCd <= 0) { shoot(); fireCd = 10; }
    if ((down & KEY_X) && buildCd <= 0) { buildWall(); buildCd = 15; }

    // --- Bot ---
    for (int i = 0; i < NBOT; i++) {
        Bot *b = &bots[i];
        if (!b->alive) continue;
        float dx = px - b->x, dz = pz - b->z;
        float d = sqrtf(dx * dx + dz * dz);
        if (d > 1.8f) {
            float nx = b->x + dx / d * 0.055f, nz = b->z + dz / d * 0.055f;
            if (!blocked(nx, b->z, 0)) b->x = nx;
            if (!blocked(b->x, nz, 0)) b->z = nz;
        } else if (b->cd <= 0 && py < 1.0f) {
            hp -= 8; b->cd = 40;
        }
        if (b->cd > 0) b->cd--;
    }

    // --- Tempesta ---
    tick++;
    stormR = ARENA - tick / 60.0f * 0.45f;
    if (stormR < 4.0f) stormR = 4.0f;
    if (tick % 60 == 0) {
        if (px * px + pz * pz > stormR * stormR) hp -= 3;
        for (int i = 0; i < NBOT; i++) {
            if (bots[i].alive && bots[i].x * bots[i].x + bots[i].z * bots[i].z > stormR * stormR)
                if (--bots[i].hp <= 0) bots[i].alive = 0;
        }
    }

    if (hp <= 0) state = 1;
    else if (botsAlive() == 0) state = 2;
}

// ---------------------------------------------------------------- grafica

static u16 sh(int r, int g, int b, int pct) {
    return RGB15(r * pct / 100, g * pct / 100, b * pct / 100);
}

// Scatola con centro (x,y,z), dimensioni (w,h,d), rotazione Y opzionale
static void drawBox(float x, float y, float z, float w, float h, float d,
                    float rotY, int r, int g, int b) {
    glPushMatrix();
    glTranslatef(x, y, z);
    if (rotY != 0.0f) glRotateY(rotY);
    glScalef(w, h, d);
    glBegin(GL_QUADS);
    glColor(sh(r, g, b, 100));  // sopra
    glVertex3v16(-H, H, -H); glVertex3v16(H, H, -H); glVertex3v16(H, H, H); glVertex3v16(-H, H, H);
    glColor(sh(r, g, b, 80));   // +z
    glVertex3v16(-H, -H, H); glVertex3v16(H, -H, H); glVertex3v16(H, H, H); glVertex3v16(-H, H, H);
    glColor(sh(r, g, b, 80));   // -z
    glVertex3v16(-H, -H, -H); glVertex3v16(-H, H, -H); glVertex3v16(H, H, -H); glVertex3v16(H, -H, -H);
    glColor(sh(r, g, b, 60));   // +x
    glVertex3v16(H, -H, -H); glVertex3v16(H, H, -H); glVertex3v16(H, H, H); glVertex3v16(H, -H, H);
    glColor(sh(r, g, b, 60));   // -x
    glVertex3v16(-H, -H, -H); glVertex3v16(-H, -H, H); glVertex3v16(-H, H, H); glVertex3v16(-H, H, -H);
    glEnd();
    glPopMatrix(1);
}

static void drawBar(int x1, int y1, int x2, int y2) {
    glVertex3v16(x1, y1, -2048); glVertex3v16(x2, y1, -2048);
    glVertex3v16(x2, y2, -2048); glVertex3v16(x1, y2, -2048);
}

static void render(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(70, 256.0f / 192.0f, 0.3f, 120.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotateX(-pitch);
    glRotateY(yaw);
    glTranslatef(-px, -(py + 1.6f), -pz);

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));

    // Terreno a scacchiera
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 12; j++) {
            glPushMatrix();
            glTranslatef((i - 5.5f) * 8.0f, 0.0f, (j - 5.5f) * 8.0f);
            glScalef(4.0f, 1.0f, 4.0f);
            glBegin(GL_QUADS);
            glColor(((i + j) & 1) ? RGB15(6, 19, 6) : RGB15(8, 23, 8));
            glVertex3v16(-4096, 0, -4096); glVertex3v16(4096, 0, -4096);
            glVertex3v16(4096, 0, 4096);   glVertex3v16(-4096, 0, 4096);
            glEnd();
            glPopMatrix(1);
        }
    }

    // Edifici, alberi, muri costruiti
    for (int i = 0; i < nbox; i++) {
        Box *b = &boxes[i];
        drawBox(b->x, b->h * 0.5f, b->z, b->w, b->h, b->d, 0.0f, b->r, b->g, b->b);
    }

    // Bot
    for (int i = 0; i < NBOT; i++) {
        if (!bots[i].alive) continue;
        drawBox(bots[i].x, 0.6f, bots[i].z, 0.7f, 1.2f, 0.5f, 0.0f, 28, 5, 5);
        drawBox(bots[i].x, 1.45f, bots[i].z, 0.45f, 0.45f, 0.45f, 0.0f, 28, 22, 16);
    }

    // Tempesta (muro viola semitrasparente)
    glPolyFmt(POLY_ALPHA(14) | POLY_CULL_NONE | POLY_ID(2));
    float seg = 6.2832f * stormR / SEGS * 1.05f;
    for (int k = 0; k < SEGS; k++) {
        float rot = -(k * 360.0f / SEGS + 90.0f);
        drawBox(stormR * cosT[k], 6.0f, stormR * sinT[k], seg, 12.0f, 0.4f, rot, 22, 6, 30);
    }

    // Arma e mirino (in coordinate di visuale)
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(3));
    glLoadIdentity();
    drawBox(0.12f, -0.12f, -0.45f, 0.05f, 0.05f, 0.25f, 0.0f, 8, 8, 9);

    glBegin(GL_QUADS);
    glColor(hitFlash > 0 ? RGB15(31, 4, 4) : RGB15(31, 31, 31));
    drawBar(-205, -12, -61, 12);
    drawBar(61, -12, 205, 12);
    drawBar(-12, 61, 12, 205);
    drawBar(-12, -205, 12, -61);
    glEnd();

    glFlush(0);
}

static void hud(void) {
    consoleClear();
    iprintf("\x1b[0;4HBATTLE ROYALE DS");
    iprintf("\x1b[2;1HVita: %d   ", hp < 0 ? 0 : hp);
    iprintf("\x1b[3;1HBot rimasti: %d", botsAlive());
    iprintf("\x1b[4;1HRaggio tempesta: %d", (int)stormR);
    iprintf("\x1b[8;1HTocco: trascina per guardare");
    iprintf("\x1b[10;1HCroce: muovi  A/R: spara");
    iprintf("\x1b[11;1HB: salta  X: costruisci muro");
    if (state == 1) iprintf("\x1b[16;5H*** ELIMINATO ***\x1b[18;7HPremi START");
    if (state == 2) iprintf("\x1b[16;4H*** VITTORIA REALE ***\x1b[18;7HPremi START");
}

int main(void) {
    videoSetMode(MODE_0_3D);
    vramSetBankA(VRAM_A_TEXTURE);
    consoleDemoInit();      // HUD sullo schermo inferiore

    glInit();
    glEnable(GL_ANTIALIAS);
    glClearColor(14, 22, 31, 31);
    glClearPolyID(63);
    glClearDepth(0x7FFF);
    glViewport(0, 0, 255, 191);

    for (int k = 0; k < SEGS; k++) {
        float a = k * 6.2832f / SEGS;
        cosT[k] = cosf(a);
        sinT[k] = sinf(a);
    }

    srand(time(NULL));
    stormR = ARENA;
    resetGame();

    int frame = 0;
    while (1) {
        update();
        render();
        if (frame++ % 10 == 0) hud();
        swiWaitForVBlank();
    }
    return 0;
}
