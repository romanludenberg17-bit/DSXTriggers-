/*
 * DSXTriggers - плагин xNVSE для Fallout: New Vegas (1.4.0.525).
 * Читает из памяти игры оружие в руках игрока и шлёт профиль правого курка
 * DualSense в DSX по UDP (127.0.0.1:6969). Без CRT, чистый C.
 */
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

typedef struct { u32 infoVersion; const char *name; u32 version; } PluginInfo;
typedef struct { u32 nvseVersion; u32 runtimeVersion; u32 editorVersion; u32 isEditor; } NVSEHead;
typedef struct { u16 family; u16 port; u32 addr; u8 zero[8]; } SockAddrIn;

__declspec(dllimport) int   __stdcall WSAStartup(u16 ver, void *data);
__declspec(dllimport) u32   __stdcall socket(int af, int type, int proto);
__declspec(dllimport) int   __stdcall sendto(u32 s, const char *buf, int len, int flags, const void *to, int tolen);
__declspec(dllimport) void *__stdcall CreateThread(void *attr, u32 stack, void *start, void *param, u32 flags, u32 *id);
__declspec(dllimport) void  __stdcall Sleep(u32 ms);
__declspec(dllimport) int   __stdcall IsBadReadPtr(const void *p, u32 n);

#define ADDR_PLAYER   0x011DEA3Cu   /* PlayerCharacter** (1.4.0.525) */
#define OFF_BASEPROC  0x068u        /* Actor::baseProcess            */
#define OFF_WEAPINFO  0x114u        /* HighProcess::weaponInfo       */
#define OFF_WEAPOUT   0x135u        /* HighProcess::isWeaponOut      */
#define OFF_WI_WEAP   0x008u        /* WeaponInfo::weapon            */
#define OFF_W_TYPE    0x0F4u        /* TESObjectWEAP::eWeaponType    */
#define OFF_W_FLAGS1  0x100u        /* weaponFlags1 (bit 0x2 = auto) */
#define OFF_W_SIGHT   0x110u        /* sightFOV (float)              */
#define OFF_W_PROJ    0x11Eu        /* numProjectiles                */
#define OFF_W_RATE    0x134u        /* fireRate (float)              */

/* DSX: Trigger Left=1 Right=2; TriggerMode */
#define M_NORMAL 0
#define M_SOFT   3
#define M_RESIST 13
#define M_SEMI   16
#define M_AUTO   17

int _fltused = 1;
static u32 g_sock = 0xFFFFFFFFu;
static SockAddrIn g_addr;

typedef struct { int mode, a, b, c; int r, g, bl; } Profile;

static char g_buf[512];
static int  g_len;

static void put_s(const char *s) { while (*s) g_buf[g_len++] = *s++; }
static void put_i(int v) {
    char t[12]; int n = 0;
    if (v < 0) { g_buf[g_len++] = '-'; v = -v; }
    if (v == 0) t[n++] = '0';
    while (v > 0) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n > 0) g_buf[g_len++] = t[--n];
}

static void send_json(void) {
    if (g_sock == 0xFFFFFFFFu) return;
    sendto(g_sock, g_buf, g_len, 0, &g_addr, sizeof(g_addr));
}

static void send_profile(const Profile *p, int nparams) {
    g_len = 0;
    put_s("{\"instructions\":[{\"type\":1,\"parameters\":[0,2,");
    put_i(p->mode);
    if (nparams > 0) { put_s(","); put_i(p->a); }
    if (nparams > 1) { put_s(","); put_i(p->b); }
    if (nparams > 2) { put_s(","); put_i(p->c); }
    put_s("]},{\"type\":2,\"parameters\":[0,");
    put_i(p->r); put_s(","); put_i(p->g); put_s(","); put_i(p->bl);
    put_s("]}]}" );
    send_json();
}

static void send_reset(void) {
    g_len = 0;
    put_s("{\"instructions\":[{\"type\":1,\"parameters\":[0,2,0]},{\"type\":1,\"parameters\":[0,1,0]}]}");
    send_json();
}

static int readable(u32 a, u32 n) { return a > 0x10000u && !IsBadReadPtr((const void *)a, n); }
#define R8(a)  (*(volatile u8 *)(a))
#define R32(a) (*(volatile u32 *)(a))
#define RF(a)  (*(volatile float *)(a))

/* Возвращает число параметров режима; state — ключ для сравнения изменений. */
static int pick_profile(Profile *p, u32 *state) {
    u32 player, proc, wi, w;
    u8 type, flags, proj; float fov, rate; int automatic, energy;

    p->mode = M_NORMAL; p->r = 90; p->g = 90; p->bl = 90;
    *state = 0;

    if (!readable(ADDR_PLAYER, 4)) return 0;
    player = R32(ADDR_PLAYER);
    if (!readable(player + OFF_BASEPROC, 4)) return 0;
    proc = R32(player + OFF_BASEPROC);
    if (!readable(proc + OFF_WEAPINFO, 0x30)) return 0;
    if (!R8(proc + OFF_WEAPOUT)) { *state = 1; return 0; }
    wi = R32(proc + OFF_WEAPINFO);
    if (!readable(wi + OFF_WI_WEAP, 4)) return 0;
    w = R32(wi + OFF_WI_WEAP);
    if (!readable(w + OFF_W_FLAGS1, 4)) return 0;

    type = R8(w + OFF_W_TYPE);
    flags = R8(w + OFF_W_FLAGS1);
    proj = R8(w + OFF_W_PROJ);
    fov = RF(w + OFF_W_SIGHT);
    rate = RF(w + OFF_W_RATE);
    automatic = (flags & 0x2) != 0;
    energy = (type == 6 || type == 7);

    if (automatic) {
        p->mode = M_AUTO; p->a = 1; p->b = 20; p->c = (int)(rate > 0.0f ? rate : 10.0f);
        *state = 2 | ((u32)(rate * 10.0f) << 8); return 3;
    }
    if (energy) { p->mode = M_SOFT; p->a = 1; p->b = 20; p->r = 40; p->g = 220; p->bl = 80; *state = 3; return 2; }
    if (proj >= 4) { p->mode = M_RESIST; p->a = 8; p->b = 80; p->r = 220; p->g = 80; p->bl = 20; *state = 4; return 2; }
    if (fov > 0.0f && fov < 18.0f) { p->mode = M_RESIST; p->a = 10; p->b = 90; p->r = 60; p->g = 120; p->bl = 240; *state = 5; return 2; }

    switch (type) {
        case 1: p->mode = M_SEMI; p->a = 20; p->r = 240; p->g = 210; p->bl = 40; *state = 6; return 1;
        case 2: p->mode = M_RESIST; p->a = 20; p->b = 70; p->r = 240; p->g = 140; p->bl = 20; *state = 7; return 2;
        case 3: p->mode = M_RESIST; p->a = 40; p->b = 90; p->r = 240; p->g = 60; p->bl = 20; *state = 8; return 2;
        case 4: p->mode = M_SOFT; p->a = 1; p->b = 25; p->r = 40; p->g = 220; p->bl = 80; *state = 9; return 2;
        case 5: p->mode = M_RESIST; p->a = 10; p->b = 100; *state = 10; return 2;
        default: *state = 11; return 0;
    }
}
