// VC100Checklist - in-game 100% completion checklist for GTA Vice City (PC, v1.0 exe).
//
// Reads completion flags straight from the running game (mission script
// variables and stats), lists what is still missing, and can mark the start
// point of a remaining side activity on the radar and pause map.
// It reads game memory and patches three CALL sites (HUD draw, radar blips, map blips). Two optional extras are off
// unless enabled in the ini: an experimental aim helper that writes the player's heading, and a desktop-resolution
// switch (done by the VC100Display.exe helper) for the ASI loader's borderless window mode.
//
// Build (32-bit):  see build.cmd
//
// Flag offsets come from disassembling the stock v1.0 main.scm: every
// PLAYER_MADE_PROGRESS call (154 points in total) was matched to the variable
// the script sets when that point is awarded.

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cmath>

// ---------------------------------------------------------------- game memory (v1.0)
static unsigned char* const ScriptSpace = (unsigned char*)0x821280;
static inline int   G(int off)  { return *(int*)(ScriptSpace + off); }
static inline int   MemI(DWORD a) { return *(int*)a; }
static inline float MemF(DWORD a) { return *(float*)a; }
static inline bool  MemB(DWORD a) { return *(unsigned char*)a != 0; }

static const DWORD A_RsGlobal        = 0x9B48D8;   // +4 maximumWidth, +8 maximumHeight, +0xC screenWidth, +0x10 screenHeight
static const DWORD A_FrontEnd        = 0x869630;   // +0x35 m_bDrawRadarOrMap
static const DWORD A_PropertyOwned   = 0xA10AFD;   // bool[15]
static const DWORD A_ProgressMade    = 0x9B6CDC;   // float
static const DWORD A_ProgressTotal   = 0x974B0C;   // float
static const DWORD A_RampagesPassed  = 0x974C08;
static const DWORD A_RampagesTotal   = 0x974C0C;
static const DWORD A_LvlAmbulance    = 0x978DB8;
static const DWORD A_LvlFire         = 0x975310;
static const DWORD A_LvlVigilante    = 0x94DD60;
static const DWORD A_TaxiFares       = 0xA0D1DC;
static const DWORD A_TopRangeScore   = 0xA0D8A4;   // float
static const DWORD A_TimeMs          = 0x974B2C;

struct RGBA { unsigned char r, g, b, a; };
struct Rect { float x1, y1, x2, y2; };
struct V2   { float x, y; };

#define CALL(addr, sig) ((sig)(addr))
static void FontSetScale(float w, float h)   { CALL(0x550230, void(__cdecl*)(float, float))(w, h); }
static void FontSetColor(RGBA c)             { CALL(0x550170, void(__cdecl*)(RGBA*))(&c); }
static void FontSetDropColor(RGBA c)         { CALL(0x54FF30, void(__cdecl*)(RGBA*))(&c); }
static void FontSetStyle(short s)            { CALL(0x54FFE0, void(__cdecl*)(short))(s); }
static void FontSetDropShadow(short s)       { CALL(0x54FF20, void(__cdecl*)(short))(s); }
static void FontSetWrapx(float w)            { CALL(0x550100, void(__cdecl*)(float))(w); }
static void FontSetAlphaFade(float a)        { CALL(0x54FFC0, void(__cdecl*)(float))(a); }
static void FontVoid(DWORD a)                { CALL(a, void(__cdecl*)())(); }
static void FontPrint(float x, float y, const wchar_t* t) { CALL(0x551040, void(__cdecl*)(float, float, const wchar_t*))(x, y, t); }
static void DrawRect(Rect r, RGBA c)         { CALL(0x577B00, void(__cdecl*)(Rect*, RGBA*))(&r, &c); }
static const wchar_t* GxtGet(const char* key) { return CALL(0x584F30, const wchar_t*(__thiscall*)(void*, const char*))((void*)0x94B220, key); }
static void RadarTrace(float x, float y, unsigned size, RGBA c, unsigned char type) {
    CALL(0x4C32F0, void(__cdecl*)(float, float, unsigned, unsigned char, unsigned char, unsigned char, unsigned char, unsigned char))(x, y, size, c.r, c.g, c.b, c.a, type);
}

// ---------------------------------------------------------------- checklist data
enum Cat { C_SUMMARY, C_STORY, C_ASSET, C_PROPERTY, C_ODDJOB, C_RACE, C_COLLECT, C_COUNT };
static const char* const CatName[C_COUNT] = {
    "Summary", "Story missions", "Asset and phone missions", "Properties",
    "Odd jobs", "Races and challenges", "Collectibles"
};

enum Type { T_FLAG, T_PROP, T_INTRO, T_PACKAGES, T_RAMPAGES, T_USJ, T_STORES };
enum Detail { D_NONE, D_AMBULANCE, D_FIRE, D_VIGILANTE, D_TAXI, D_RANGE };

struct Item {
    Cat cat; Type type; int a;          // a = script flag offset, or property id
    const char* gxt;                    // GXT key for the official name (or null)
    const char* name;                   // plain name when there is no GXT key
    const char* hint;
    int pts;                            // progress points this entry is worth
    float x, y;                         // start location (0,0 = none)
    Detail detail;
};

#define H_KEN   "Ken Rosenberg's office, Washington Beach"
#define H_COL   "Colonel Cortez's yacht, Ocean Beach marina"
#define H_DIAZ  "Ricardo Diaz's mansion, Starfish Island"
#define H_AVERY "Avery Carrington's limo at the construction site"
#define H_VERC  "Vercetti Estate, Starfish Island"
#define H_MALI  "Malibu Club, Vice Point (buy it first)"
#define H_PHIL  "Phil's Place, Little Haiti (after The Job)"
#define H_FILM  "Film studio, Prawn Island (buy it first)"
#define H_PRINT "Print works, Little Haiti (buy it first)"
#define H_BIKE  "The Greasy Chopper biker bar, Downtown"
#define H_CUB   "Cafe Robina, Little Havana"
#define H_HAT   "Auntie Poulet's shack, Little Haiti"
#define H_ROCK  "Love Fist's recording studio, Downtown"
#define H_PHONE "Answer the ringing payphone (phone icon on the map)"
#define H_KAUF  "Kaufman Cabs, Little Haiti (buy it first)"
#define H_RACE  "Race map in the Sunshine Autos garage (buy it first). Win the race."
#define H_STAD  "Hyman Memorial Stadium, Downtown, 20:00-00:00. The event changes daily."
#define H_HELI  "Get in the parked Sparrow helicopter and collect every checkpoint."
#define H_RC    "Get in the Top Fun van."

static const Item Items[] = {
    // ---- story
    { C_STORY, T_INTRO, 0,    0, "In the Beginning... (intro)", "", 1 },
    { C_STORY, T_FLAG, 896,  "LAW_1", 0, H_KEN, 1 },
    { C_STORY, T_FLAG, 900,  "LAW_2", 0, H_KEN, 1 },
    { C_STORY, T_FLAG, 904,  "LAW_3", 0, H_KEN, 1 },
    { C_STORY, T_FLAG, 908,  "LAW_4", 0, H_KEN, 1 },
    { C_STORY, T_FLAG, 916,  "COL_1", 0, H_COL, 1 },
    { C_STORY, T_FLAG, 920,  "COL_2", 0, H_COL, 1 },
    { C_STORY, T_FLAG, 924,  "COL_3", 0, H_COL, 1 },
    { C_STORY, T_FLAG, 928,  "COL_4", 0, H_COL, 1 },
    { C_STORY, T_FLAG, 932,  "COL_5", 0, H_COL, 1 },
    { C_STORY, T_FLAG, 940,  "COK_1", 0, H_DIAZ, 1 },
    { C_STORY, T_FLAG, 944,  "COK_2", 0, H_DIAZ, 1 },
    { C_STORY, T_FLAG, 948,  "COK_3", 0, H_DIAZ, 1 },
    { C_STORY, T_FLAG, 952,  "COK_4", 0, H_DIAZ, 1 },
    { C_STORY, T_FLAG, 968,  "KENT_1", 0, "Kent Paul at the Malibu Club, Vice Point", 1 },
    { C_STORY, T_FLAG, 956,  "ASS_1", 0, "Starfish Island, after Death Row", 1 },
    { C_STORY, T_FLAG, 976,  "TEX_1", 0, H_AVERY, 1 },
    { C_STORY, T_FLAG, 980,  "TEX_3", 0, H_AVERY, 1 },
    { C_STORY, T_FLAG, 984,  "TEX_2", 0, H_AVERY, 1 },
    { C_STORY, T_FLAG, 1064, "BUD_1", 0, H_VERC, 1 },
    { C_STORY, T_FLAG, 1068, "BUD_2", 0, H_VERC, 1 },
    { C_STORY, T_FLAG, 1072, "BUD_3", 0, H_VERC, 1 },
    { C_STORY, T_FLAG, 1076, "CAP_1", 0, H_VERC " (needs most assets completed)", 1 },
    { C_STORY, T_FLAG, 1080, "FIN_1", 0, H_VERC " (after Cap the Collector)", 1 },
    { C_STORY, T_FLAG, 1100, "BIKE_1", 0, H_BIKE, 1 },
    { C_STORY, T_FLAG, 1104, "BIKE_2", 0, H_BIKE, 1 },
    { C_STORY, T_FLAG, 1108, "BIKE_3", 0, H_BIKE, 1 },
    { C_STORY, T_FLAG, 1116, "CUB_1", 0, H_CUB, 1 },
    { C_STORY, T_FLAG, 1120, "CUB_2", 0, H_CUB, 1 },
    { C_STORY, T_FLAG, 1124, "CUB_3", 0, H_CUB, 1 },
    { C_STORY, T_FLAG, 1128, "CUB_4", 0, H_CUB, 1 },
    { C_STORY, T_FLAG, 1136, "HAT_1", 0, H_HAT, 1 },
    { C_STORY, T_FLAG, 1140, "HAT_2", 0, H_HAT, 1 },
    { C_STORY, T_FLAG, 1144, "HAT_3", 0, H_HAT, 1 },
    { C_STORY, T_FLAG, 1152, "ROCK_1", 0, H_ROCK, 1 },
    { C_STORY, T_FLAG, 1156, "ROCK_2", 0, H_ROCK, 1 },
    { C_STORY, T_FLAG, 1160, "ROCK_3", 0, H_ROCK, 1 },
    { C_STORY, T_FLAG, 1016, "PHIL_1", 0, H_PHIL, 1 },
    { C_STORY, T_FLAG, 1020, "PHIL_2", 0, H_PHIL, 1 },

    // ---- asset and phone missions
    { C_ASSET, T_FLAG, 992,  "BANK_1", 0, H_MALI, 1 },
    { C_ASSET, T_FLAG, 996,  "BANK_2", 0, H_MALI, 1 },
    { C_ASSET, T_FLAG, 1000, "BANK_3", 0, H_MALI, 1 },
    { C_ASSET, T_FLAG, 1004, "BANK_4", 0, H_MALI, 1 },
    { C_ASSET, T_FLAG, 1028, "PORN_1", 0, H_FILM, 1 },
    { C_ASSET, T_FLAG, 1032, "PORN_2", 0, H_FILM, 1 },
    { C_ASSET, T_FLAG, 1036, "PORN_3", 0, H_FILM, 1 },
    { C_ASSET, T_FLAG, 1040, "PORN_4", 0, H_FILM, 1 },
    { C_ASSET, T_FLAG, 1088, "CNT_1", 0, H_PRINT, 1 },
    { C_ASSET, T_FLAG, 1092, "CNT_2", 0, H_PRINT, 1 },
    { C_ASSET, T_FLAG, 1232, "TAXI_1", 0, H_KAUF, 1 },
    { C_ASSET, T_FLAG, 1236, "TAXI_2", 0, H_KAUF, 1 },
    { C_ASSET, T_FLAG, 2492, "TAXI_3", 0, H_KAUF, 1 },
    { C_ASSET, T_FLAG, 2448, "CREAM", 0, "Cherry Popper factory, Little Havana (buy it first). Sell 50 in one Mr. Whoopee run.", 1, -864.3f, -576.6f },
    { C_ASSET, T_FLAG, 2428, 0, "Checkpoint Charlie", "Boatyard, Viceport (buy it first). Take one of the boats.", 1, -685.8f, -1495.6f },
    { C_ASSET, T_FLAG, 4384, 0, "Pole Position Club: spend $300", "Buy the club (Ocean Beach), then stay in the private room until $300 is spent.", 1, 99.5f, -1468.5f },
    { C_ASSET, T_FLAG, 4500, 0, "Sunshine Autos car list 1", "Landstalker, Idaho, Esperanto, Stallion, Rancher, Blista Compact", 1, -1007.3f, -869.9f },
    { C_ASSET, T_FLAG, 4504, 0, "Sunshine Autos car list 2", "Sabre, Virgo, Sentinel, Stretch, Washington, Admiral", 1, -1007.3f, -869.9f },
    { C_ASSET, T_FLAG, 4508, 0, "Sunshine Autos car list 3", "Cheetah, Infernus, Banshee, Phoenix, Comet, Stinger", 1, -1007.3f, -869.9f },
    { C_ASSET, T_FLAG, 4512, 0, "Sunshine Autos car list 4", "Voodoo, Cuban Hermes, Caddy, Baggage Handler, Mr. Whoopee, Pizza Boy", 1, -1007.3f, -869.9f },
    { C_ASSET, T_FLAG, 1192, "JOB_1", 0, H_PHONE, 1 },
    { C_ASSET, T_FLAG, 1196, "JOB_2", 0, H_PHONE, 1 },
    { C_ASSET, T_FLAG, 1200, "JOB_3", 0, H_PHONE, 2 },
    { C_ASSET, T_FLAG, 1204, "JOB_4", 0, H_PHONE, 1 },
    { C_ASSET, T_FLAG, 1208, "JOB_5", 0, H_PHONE, 1 },

    // ---- properties (a = property id used by the game)
    { C_PROPERTY, T_PROP, 13, 0, "Skumole Shack",           "$1,000 - Downtown", 1, -560.1f, 703.6f },
    { C_PROPERTY, T_PROP, 8,  0, "3321 Vice Point",         "$2,500 - Vice Point, north end", 1, 531.4f, 1273.7f },
    { C_PROPERTY, T_PROP, 11, 0, "1102 Washington Street",  "$3,000 - Washington Beach", 1, 88.5f, -804.7f },
    { C_PROPERTY, T_PROP, 9,  0, "Links View Apartment",    "$6,000 - Vice Point", 1, 304.5f, 376.3f },
    { C_PROPERTY, T_PROP, 12, 0, "Ocean Heights Apartment", "$7,000 - Ocean Beach", 1, 14.0f, -1500.7f },
    { C_PROPERTY, T_PROP, 10, 0, "El Swanko Casa",          "$8,000 - Vice Point", 1, 428.4f, 605.9f },
    { C_PROPERTY, T_PROP, 14, 0, "Hyman Condo",             "$14,000 - Downtown", 1, -834.8f, 1306.9f },
    { C_PROPERTY, T_PROP, 6,  0, "Boatyard",                "$10,000 - Viceport (assets go on sale after Shakedown)", 1, -685.8f, -1495.6f },
    { C_PROPERTY, T_PROP, 3,  0, "Cherry Popper Ice Cream Factory", "$20,000 - Little Havana", 1, -864.3f, -576.6f },
    { C_PROPERTY, T_PROP, 7,  0, "Pole Position Club",      "$30,000 - Ocean Beach", 1, 99.5f, -1468.5f },
    { C_PROPERTY, T_PROP, 5,  0, "Kaufman Cabs",            "$40,000 - Little Haiti", 1, -1011.7f, 203.9f },
    { C_PROPERTY, T_PROP, 4,  0, "Sunshine Autos",          "$50,000 - Little Havana", 1, -1007.3f, -869.9f },
    { C_PROPERTY, T_PROP, 2,  0, "Film Studio",             "$60,000 - Prawn Island", 1, 15.2f, 962.6f },
    { C_PROPERTY, T_PROP, 1,  0, "Print Works",             "$70,000 - Little Haiti", 1, -1059.6f, -274.5f },
    { C_PROPERTY, T_PROP, 0,  0, "The Malibu Club",         "$120,000 - Vice Point", 1, 487.2f, -81.5f },

    // ---- odd jobs
    { C_ODDJOB, T_FLAG, 6324, 0, "Paramedic: level 12",   "Any Ambulance, start the vehicle mission. All 12 levels in one go.", 1, 0, 0, D_AMBULANCE },
    { C_ODDJOB, T_FLAG, 6332, 0, "Firefighter: level 12", "Any Fire Truck, start the vehicle mission. All 12 levels in one go.", 1, 0, 0, D_FIRE },
    { C_ODDJOB, T_FLAG, 6328, 0, "Vigilante: level 12",   "Any police vehicle, start the vehicle mission. All 12 levels in one go.", 1, 0, 0, D_VIGILANTE },
    { C_ODDJOB, T_FLAG, 1556, 0, "Pizza Boy: level 10",   "Pizza Boy moped outside a Well Stacked Pizza. All 10 levels in one go.", 1 },
    { C_ODDJOB, T_FLAG, 1492, 0, "Taxi Driver: 100 fares", "Any taxi. Fares add up across sessions.", 1, 0, 0, D_TAXI },
    { C_ODDJOB, T_FLAG, 432,  0, "Shooting range: 45 points", "Downtown Ammu-Nation, door at the back (opens with The Shootist).", 1, -665.6f, 1231.9f, D_RANGE },

    // ---- races and challenges
    { C_RACE, T_FLAG, 6352, 0, "Street race: Terminal Velocity", H_RACE, 1, -967.7f, -827.3f },
    { C_RACE, T_FLAG, 6356, 0, "Street race: Ocean Drive",       H_RACE, 1, -967.7f, -827.3f },
    { C_RACE, T_FLAG, 6360, 0, "Street race: Border Run",        H_RACE, 1, -967.7f, -827.3f },
    { C_RACE, T_FLAG, 6364, 0, "Street race: Capital Cruise",    H_RACE, 1, -967.7f, -827.3f },
    { C_RACE, T_FLAG, 6368, 0, "Street race: Tour!",             H_RACE, 1, -967.7f, -827.3f },
    { C_RACE, T_FLAG, 6372, 0, "Street race: V.C. Endurance",    H_RACE, 1, -967.7f, -827.3f },
    { C_RACE, T_FLAG, 6388, 0, "Stadium: Hotring",   H_STAD, 1, -1110.3f, 1331.1f },
    { C_RACE, T_FLAG, 6392, 0, "Stadium: Bloodring", H_STAD, 1, -1110.3f, 1331.1f },
    { C_RACE, T_FLAG, 220,  0, "Stadium: Dirtring",  H_STAD, 1, -1110.3f, 1331.1f },
    { C_RACE, T_FLAG, 1452, 0, "Trial by Dirt",  "Sanchez at the dirt track, north Downtown.", 1, -425.0f, 1410.0f },
    { C_RACE, T_FLAG, 1456, 0, "Test Track",     "Landstalker at the dirt track, north Downtown.", 1, -425.0f, 1410.0f },
    { C_RACE, T_FLAG, 1356, 0, "PCJ Playground", "Get on the parked PCJ 600 and collect 24 checkpoints.", 1, 507.4f, -308.8f },
    { C_RACE, T_FLAG, 1404, 0, "Cone Crazy",     "Stallion on the roof of the multi-storey car park, Ocean Beach.", 1, 127.5f, -1157.5f },
    { C_RACE, T_FLAG, 6336, 0, "Chopper checkpoint: Downtown",     H_HELI, 1, -569.1f, 851.1f },
    { C_RACE, T_FLAG, 6340, 0, "Chopper checkpoint: Ocean Beach",  H_HELI, 1, 28.4f, -1311.8f },
    { C_RACE, T_FLAG, 6344, 0, "Chopper checkpoint: Vice Point",   H_HELI, 1, 375.8f, 332.9f },
    { C_RACE, T_FLAG, 6348, 0, "Chopper checkpoint: Little Haiti", H_HELI, 1, -886.6f, 236.6f },
    { C_RACE, T_FLAG, 32624, 0, "RC Raider Pickup", H_RC " Airport area.", 1, -1235.1f, -1235.7f },
    { C_RACE, T_FLAG, 32964, 0, "RC Bandit Race",   H_RC " Vice Point beach, north.", 1, 718.5f, 701.4f },
    { C_RACE, T_FLAG, 33940, 0, "RC Baron Race",    H_RC " North Point Mall car park roof.", 1, 307.9f, 1254.6f },

    // ---- collectibles (shown on radar/map by the Collectibles on Radar mod)
    { C_COLLECT, T_PACKAGES, 0, 0, "Hidden packages", "A progress point for every 10. Green on the radar and map.", 10 },
    { C_COLLECT, T_RAMPAGES, 0, 0, "Rampages",        "All 35 for one progress point. Pink on the radar and map.", 1 },
    { C_COLLECT, T_USJ,      0, 0, "Unique stunt jumps", "One progress point each. Blue on the radar and map.", 36 },
    { C_COLLECT, T_STORES,   0, 0, "Store robberies", "Aim a gun at the clerk in all 15 stores. Brown on the radar and map.", 1 },
};
static const int NItems = sizeof(Items) / sizeof(Items[0]);

// ---------------------------------------------------------------- status
static int Packages()     { int n = G(5224); return n < 0 ? 0 : (n > 100 ? 100 : n); }
static int UsjDone()      { int n = 0; for (int i = 0; i < 36; i++) if (G(3180 + 4 * i) != 0) n++; return n; }
static bool StoresDone()  { return G(6124) == -1 && MemF(0x97F898) >= 15.0f; }
static int StoresRobbed() { int n = (int)MemF(0x97F898); return n < 0 ? 0 : (n > 15 ? 15 : n); }

static int PointsDone(const Item& it) {
    switch (it.type) {
    case T_FLAG:     return G(it.a) != 0 ? it.pts : 0;
    case T_PROP:     return MemB(A_PropertyOwned + it.a) ? 1 : 0;
    case T_INTRO:    return 1;
    case T_PACKAGES: return Packages() / 10;
    case T_RAMPAGES: return (MemI(A_RampagesTotal) > 0 && MemI(A_RampagesPassed) >= MemI(A_RampagesTotal)) ? 1 : 0;
    case T_USJ:      return UsjDone();
    case T_STORES:   return StoresDone() ? 1 : 0;
    }
    return 0;
}
static bool IsDone(const Item& it) { return PointsDone(it) >= it.pts; }

// ---------------------------------------------------------------- text helpers
static wchar_t g_w[512];
static const wchar_t* W(const char* s) {
    int i = 0;
    for (; s[i] && i < 510; i++) g_w[i] = (unsigned char)s[i] == '~' ? L'-' : (wchar_t)(unsigned char)s[i];
    g_w[i] = 0;
    return g_w;
}
static void ItemName(const Item& it, char* out, int n) {
    if (it.gxt) {
        const wchar_t* w = GxtGet(it.gxt);
        int i = 0;
        if (w) for (; w[i] && i < n - 1; i++) out[i] = (w[i] < 128 && w[i] != L'~') ? (char)w[i] : '?';
        out[i] = 0;
        if (i == 0) _snprintf(out, n, "%s", it.gxt);
    } else {
        _snprintf(out, n, "%s", it.name);
    }
    out[n - 1] = 0;
}
static void ItemDetail(const Item& it, char* out, int n) {
    out[0] = 0;
    switch (it.type) {
    case T_PACKAGES: _snprintf(out, n, "%d / 100", Packages()); break;
    case T_RAMPAGES: _snprintf(out, n, "%d / %d", MemI(A_RampagesPassed), MemI(A_RampagesTotal) > 0 ? MemI(A_RampagesTotal) : 35); break;
    case T_USJ:      _snprintf(out, n, "%d / 36", UsjDone()); break;
    case T_STORES:   _snprintf(out, n, "%d / 15", StoresRobbed()); break;
    default: break;
    }
    switch (it.detail) {
    case D_AMBULANCE: _snprintf(out, n, "best level %d", MemI(A_LvlAmbulance)); break;
    case D_FIRE:      _snprintf(out, n, "best level %d", MemI(A_LvlFire)); break;
    case D_VIGILANTE: _snprintf(out, n, "best level %d", MemI(A_LvlVigilante)); break;
    case D_TAXI:      _snprintf(out, n, "%d / 100 fares", MemI(A_TaxiFares)); break;
    case D_RANGE:     _snprintf(out, n, "best score %d", (int)MemF(A_TopRangeScore)); break;
    default: break;
    }
    out[n - 1] = 0;
}

// ---------------------------------------------------------------- settings / state
static char g_dir[MAX_PATH];
static int  kToggle = VK_F7, kPrev = VK_F5, kNext = VK_F6, kMark = VK_F8, kUp = VK_PRIOR, kDown = VK_NEXT;
static bool g_show = false;
static int  g_cat = C_SUMMARY;
static int  g_scroll = 0;
static int  g_marked = -1;          // index into Items, or -1
static int  g_lastTime = 0;
static bool g_logged = false;
static float g_ui = 0.6f;         // overall panel scale (ini: [Display] ScalePercent)
static float g_px = 1.5f, g_py = 2.0f;
static int g_fillTries = 0;
static int g_fill = 1;              // ini: [Window] FillScreen
static int g_aimFix = 1;            // ini: [Camera] AimTurnsPlayer // panel position, percent of the screen from the top-left

static void LoadSettings() {
    char ini[MAX_PATH]; _snprintf(ini, MAX_PATH, "%sVC100Checklist.ini", g_dir);
    kToggle = GetPrivateProfileIntA("Keys", "Toggle", kToggle, ini);
    kPrev   = GetPrivateProfileIntA("Keys", "PreviousCategory", kPrev, ini);
    kNext   = GetPrivateProfileIntA("Keys", "NextCategory", kNext, ini);
    kMark   = GetPrivateProfileIntA("Keys", "MarkOnMap", kMark, ini);
    kUp     = GetPrivateProfileIntA("Keys", "ScrollUp", kUp, ini);
    kDown   = GetPrivateProfileIntA("Keys", "ScrollDown", kDown, ini);
    int pct = GetPrivateProfileIntA("Display", "ScalePercent", 60, ini);
    if (pct < 30) pct = 30; if (pct > 200) pct = 200;
    g_ui = pct / 100.0f;
    g_px = (float)GetPrivateProfileIntA("Display", "LeftPercent", 1, ini);
    g_py = (float)GetPrivateProfileIntA("Display", "TopPercent", 2, ini);
    g_aimFix = GetPrivateProfileIntA("Camera", "AimTurnsPlayer", 0, ini);
    g_fill = GetPrivateProfileIntA("Window", "FillScreen", 0, ini);
}

static void WriteLog() {
    char path[MAX_PATH]; _snprintf(path, MAX_PATH, "%sVC100Checklist.log", g_dir);
    FILE* f = fopen(path, "w");
    if (!f) return;
    int mine = 0, total = 0;
    for (int i = 0; i < NItems; i++) { mine += PointsDone(Items[i]); total += Items[i].pts; }
    fprintf(f, "game progress %.0f / %.0f, checklist counts %d / %d\n", MemF(A_ProgressMade), MemF(A_ProgressTotal), mine, total);
    for (int i = 0; i < NItems; i++) {
        char nm[96], dt[64]; ItemName(Items[i], nm, 96); ItemDetail(Items[i], dt, 64);
        fprintf(f, "[%s] %-22s %s %s\n", IsDone(Items[i]) ? "x" : " ", CatName[Items[i].cat], nm, dt);
    }
    fclose(f);
}

static bool Pressed(int vk) {
    static unsigned char prev[256];
    bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    bool hit = down && !prev[vk & 255];
    prev[vk & 255] = down;
    return hit;
}
static bool GameFocused() {
    DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

// remaining items of the current category, in order
static int Remaining(int cat, int* out, int max) {
    int n = 0;
    for (int i = 0; i < NItems && n < max; i++)
        if (Items[i].cat == cat && !IsDone(Items[i])) out[n++] = i;
    return n;
}

static void HandleKeys() {
    bool focus = GameFocused();
    bool t = Pressed(kToggle), p = Pressed(kPrev), nx = Pressed(kNext), m = Pressed(kMark), u = Pressed(kUp), d = Pressed(kDown);
    if (!focus) return;
    if (t) { g_show = !g_show; if (g_show) { LoadSettings(); WriteLog(); } }
    if (!g_show) return;
    if (p)  { g_cat = (g_cat + C_COUNT - 1) % C_COUNT; g_scroll = 0; }
    if (nx) { g_cat = (g_cat + 1) % C_COUNT; g_scroll = 0; }
    if (u && g_scroll > 0) g_scroll--;
    if (d) g_scroll++;
    if (m && g_cat != C_SUMMARY) {
        // cycle the map marker through remaining entries of this category that have a location
        int rem[128]; int n = Remaining(g_cat, rem, 128);
        int start = -1;
        for (int i = 0; i < n; i++) if (rem[i] == g_marked) start = i;
        g_marked = -1;
        for (int i = start + 1; i < n; i++)
            if (Items[rem[i]].x != 0.0f || Items[rem[i]].y != 0.0f) { g_marked = rem[i]; break; }
    }
}

// ---------------------------------------------------------------- drawing
static float SW() { return (float)MemI(A_RsGlobal + 4); }
static float SH() { return (float)MemI(A_RsGlobal + 8); }

static void Text(float x, float y, float size, RGBA col, const char* s, float wrap) {
    float k = SH() / 448.0f * g_ui;
    FontVoid(0x5500D0);            // SetBackgroundOff
    FontVoid(0x550080);            // SetBackGroundOnlyTextOff
    FontVoid(0x550020);            // SetPropOn
    FontVoid(0x550140);            // SetJustifyOff
    FontVoid(0x550110);            // SetCentreOff
    FontVoid(0x550040);            // SetRightJustifyOff
    FontSetAlphaFade(255.0f);
    FontSetStyle(1);
    float sx = 0.42f * size * k;
    FontSetScale(sx, 0.90f * size * k);
    FontSetWrapx(SW() * 4.0f);
    const wchar_t* ws = W(s);
    float tw = CALL(0x550650, float(__cdecl*)(const wchar_t*, bool))(ws, true);
    if (tw > wrap - x && tw > 0.0f) FontSetScale(sx * (wrap - x) / tw, 0.90f * size * k);   // shrink to fit the panel
    FontSetDropShadow(1);
    RGBA black = { 0, 0, 0, 255 };
    FontSetDropColor(black);
    FontSetColor(col);
    FontPrint(x, y, ws);
}

static const RGBA WHITE = { 235, 235, 235, 255 }, GREY = { 170, 170, 170, 255 }, PINK = { 255, 105, 180, 255 },
                  GOLD = { 245, 200, 90, 255 }, GREEN = { 120, 220, 120, 255 };

static void DrawPanel() {
    float w = SW(), h = SH(), k = h / 448.0f * g_ui;
    float x0 = w * g_px / 100.0f, y0 = h * g_py / 100.0f, pw = 400 * k, ph = 300 * k;
    if (x0 + pw > w) pw = w - x0;
    Rect r = { x0, y0, x0 + pw, y0 + ph };
    RGBA bg = { 10, 6, 24, 205 };
    DrawRect(r, bg);

    float x = x0 + 8 * k, y = y0 + 6 * k, wrap = x0 + pw - 8 * k;
    char line[256];
    _snprintf(line, 256, "100%% CHECKLIST - %.0f / %.0f", MemF(A_ProgressMade), MemF(A_ProgressTotal) > 0 ? MemF(A_ProgressTotal) : 154.0f);
    Text(x, y, 1.15f, GOLD, line, wrap); y += 19 * k;
    _snprintf(line, 256, "< %s >   (%d of %d)", CatName[g_cat], g_cat + 1, (int)C_COUNT);
    Text(x, y, 1.0f, PINK, line, wrap); y += 19 * k;

    if (g_cat == C_SUMMARY) {
        for (int c = 1; c < C_COUNT; c++) {
            int done = 0, total = 0;
            for (int i = 0; i < NItems; i++) if (Items[i].cat == c) { done += PointsDone(Items[i]); total += Items[i].pts; }
            _snprintf(line, 256, "%s: %d / %d", CatName[c], done, total);
            Text(x, y, 1.0f, done >= total ? GREEN : WHITE, line, wrap); y += 17 * k;
        }
        y += 8 * k;
        Text(x, y, 0.85f, GREY, "F5 / F6: change page.  PgUp / PgDn: scroll.", wrap); y += 14 * k;
        Text(x, y, 0.85f, GREY, "F8: mark the next remaining task on the map.", wrap); y += 14 * k;
        Text(x, y, 0.85f, GREY, "F7: close.  F9: collectible markers on/off.", wrap);
    } else {
        int rem[128]; int n = Remaining(g_cat, rem, 128);
        const int perPage = 8;
        if (n == 0) {
            Text(x, y, 1.0f, GREEN, "All done!", wrap);
        } else {
            if (g_scroll > n - perPage) g_scroll = n - perPage;
            if (g_scroll < 0) g_scroll = 0;
            for (int i = g_scroll; i < n && i < g_scroll + perPage; i++) {
                const Item& it = Items[rem[i]];
                char nm[96], dt[64]; ItemName(it, nm, 96); ItemDetail(it, dt, 64);
                bool marked = rem[i] == g_marked;
                if (dt[0]) _snprintf(line, 256, "%s  (%s)%s", nm, dt, marked ? "  [on map]" : "");
                else       _snprintf(line, 256, "%s%s", nm, marked ? "  [on map]" : "");
                Text(x, y, 1.0f, marked ? PINK : WHITE, line, wrap); y += 15 * k;
                if (it.hint && it.hint[0]) { Text(x + 6 * k, y, 0.8f, GREY, it.hint, wrap); y += 16 * k; }
                else y += 4 * k;
            }
            _snprintf(line, 256, "%d remaining, showing %d-%d", n, g_scroll + 1, (g_scroll + perPage < n ? g_scroll + perPage : n));
            Text(x, y0 + ph - 16 * k, 0.85f, GREY, line, wrap);
        }
    }
    FontVoid(0x550250);            // DrawFonts (flush)
}

// With a free camera, firing or aiming on foot makes the camera swing round behind Tommy. While a mouse button is
// held on foot this turns Tommy to face where the camera already looks instead, so the view stays put.
static void AimTurnsPlayer() {
    if (!g_aimFix || !GameFocused()) return;
    if (!((GetAsyncKeyState(VK_LBUTTON) | GetAsyncKeyState(VK_RBUTTON)) & 0x8000)) return;
    unsigned char* ped = CALL(0x4BC120, unsigned char*(__cdecl*)())();          // FindPlayerPed
    if (!ped || ped[0x3AC]) return;                                             // none, or in a vehicle
    const DWORD cam = 0x7E4688 + 0x188 + (DWORD)(*(unsigned char*)(0x7E4688 + 0x76)) * 0x1CC;   // active CCam
    if (*(unsigned char*)(0x7E4688 + 0x76) > 2) return;
    if (*(short*)(cam + 0x0C) != 4) return;                                     // only the follow-ped camera
    if (MemB(0x7E4688 + 0x6D)) return;                                          // cutscene widescreen
    float fx = MemF(cam + 0x168), fy = MemF(cam + 0x16C);
    if (fx * fx + fy * fy < 0.01f) return;
    float heading = atan2f(-fx, fy);
    *(float*)(ped + 0x374) = heading;
    *(float*)(ped + 0x378) = heading;
}

static void (__cdecl* g_origHud)() = 0;
static void __cdecl HudHook() {
    if (g_origHud) g_origHud();
    int now = MemI(A_TimeMs);
    if (now < g_lastTime) { g_marked = -1; }      // a save was loaded or a new game started
    g_lastTime = now;
    HandleKeys();
    AimTurnsPlayer();
    if (g_marked >= 0 && IsDone(Items[g_marked])) g_marked = -1;
    if (!g_logged && now > 3000) { g_logged = true; WriteLog(); }
    if (g_show) DrawPanel();
}

// marker for the selected task, drawn with the game's own radar/map transforms
static void DrawMarker() {
    if (g_marked < 0) return;
    const Item& it = Items[g_marked];
    bool map = MemB(A_FrontEnd + 0x35);
    float range = MemF(0x974BEC);
    if (range == 0.0f) return;
    float ox = MemF(0x704734), oy = MemF(0x704738), s = MemF(0x70483C), c = MemF(0x704840);
    float rx = (it.x - ox) / range, ry = (it.y - oy) / range;
    V2 in = { s * ry + c * rx, c * ry - s * rx };
    V2 out;
    if (map) {
        out.x = (MemF(0x68FD0C) * MemF(0x869670) * in.x + MemF(0x68FD10) * MemF(0x869670) + MemF(0x869674));
        out.x *= ((float)MemI(A_RsGlobal + 0xC) * MemF(0x68FD14));
        out.y = (MemF(0x869678) - MemF(0x68FD1C) * MemF(0x869670) - MemF(0x68FD18) * MemF(0x869670) * in.y);
        out.y *= ((float)MemI(A_RsGlobal + 0x10) * MemF(0x68FD20));
    } else {
        float len = sqrtf(in.x * in.x + in.y * in.y);
        if (len > 1.0f) { in.x /= len; in.y /= len; }
        float sw = (float)MemI(A_RsGlobal + 0xC), sh = (float)MemI(A_RsGlobal + 0x10);
        float w = sw * MemF(0x68FD14) * MemF(0x68FD24);
        out.x = in.x * w * MemF(0x68FD28) + w * MemF(0x68FD28) + MemF(0x68FD2C);
        out.y = MemF(0x68FD30) * (sh * MemF(0x68FD20)) * MemF(0x68FD28) + sh
              - MemF(0x68FD34) * (sh * MemF(0x68FD20))
              - in.y * (MemF(0x68FD30) * (sh * MemF(0x68FD20))) * MemF(0x68FD28);
    }
    RGBA pink = { 255, 105, 180, 255 };
    RadarTrace(out.x, out.y, 2, pink, 2);
}
static void (__cdecl* g_origBlipsRadar)() = 0;
static void (__cdecl* g_origBlipsMap)() = 0;
static void __cdecl BlipsRadarHook() { if (g_origBlipsRadar) g_origBlipsRadar(); DrawMarker(); }
static void __cdecl BlipsMapHook()   { if (g_origBlipsMap) g_origBlipsMap(); DrawMarker(); }

// ---------------------------------------------------------------- borderless window fill
// With the ASI loader's window mode (wndmode.ini) the game window is only as big as the game's resolution. This keeps
// that borderless window stretched over its whole monitor; Direct3D scales the picture, so the game keeps rendering
// at its own resolution (e.g. 2560x1440 on a 3840x2160 desktop) and Alt+Tab keeps working.
static BOOL CALLBACK FindGameWindow(HWND w, LPARAM out) {
    DWORD pid = 0; GetWindowThreadProcessId(w, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(w) || GetWindow(w, GW_OWNER)) return TRUE;
    char title[64] = ""; GetWindowTextA(w, title, 64);
    if (strcmp(title, "GTA: Vice City") != 0) return TRUE;
    *(HWND*)out = w;
    return FALSE;
}
static DWORD WINAPI FillThread(LPVOID) {
    for (;;) {
        Sleep(700);
        if (!g_fill) continue;
        HWND w = 0; EnumWindows(FindGameWindow, (LPARAM)&w);
        if (!w || IsIconic(w)) continue;
        if (GetWindowLongA(w, GWL_STYLE) & WS_CAPTION) continue;            // a framed window: leave it alone
        MONITORINFO mi; mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoA(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &mi)) continue;
        RECT r; GetWindowRect(w, &r);
        const RECT& m = mi.rcMonitor;
        if (r.left == m.left && r.top == m.top && r.right == m.right && r.bottom == m.bottom) continue;
        if (r.right - r.left > m.right - m.left || r.bottom - r.top > m.bottom - m.top) continue;   // bigger than the screen: not ours to fix
        // The loader's window mode intercepts user32's SetWindowPos inside this process, so go one level below it
        typedef BOOL (WINAPI* SetPosFn)(HWND, HWND, int, int, int, int, UINT);
        static SetPosFn direct = (SetPosFn)GetProcAddress(LoadLibraryA("win32u.dll"), "NtUserSetWindowPos");
        SetPosFn setPos = direct ? direct : (SetPosFn)SetWindowPos;
        setPos(w, 0, m.left, m.top, m.right - m.left, m.bottom - m.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        g_fillTries++;
    }
}

// ---------------------------------------------------------------- desktop resolution while the game runs
// In the loader's borderless window mode the game window is as big as the game's resolution. To have it fill the
// screen at a lower resolution than the desktop's, the desktop itself is switched to that resolution for as long as
// the game runs. CDS_FULLSCREEN makes the change temporary: Windows restores the desktop when the process ends,
// also after a crash.
static bool g_modeChanged = false;
static void ApplyDesktopMode() {
    char ini[MAX_PATH]; _snprintf(ini, MAX_PATH, "%sVC100Checklist.ini", g_dir);
    int w = GetPrivateProfileIntA("Window", "DesktopWidth", 0, ini);
    int h = GetPrivateProfileIntA("Window", "DesktopHeight", 0, ini);
    if (w <= 0 || h <= 0) return;
    char wnd[MAX_PATH]; GetModuleFileNameA(0, wnd, MAX_PATH);
    char* sl = strrchr(wnd, '\\'); if (!sl) return;
    strcpy(sl + 1, "wndmode.ini");
    if (GetFileAttributesA(wnd) == INVALID_FILE_ATTRIBUTES) return;       // exclusive fullscreen: the game switches modes itself

    // The loader's window mode swallows mode changes made inside this process, so a small helper does it from
    // outside and restores the desktop when the game exits. Wait (briefly) until it has switched, so that the
    // game window is created on the new desktop size.
    HANDLE ready = CreateEventA(0, TRUE, FALSE, "VC100DisplayReady");
    char cmd[MAX_PATH * 2];
    _snprintf(cmd, sizeof(cmd), "\"%sVC100Display.exe\" %d %d %lu", g_dir, w, h, GetCurrentProcessId());
    STARTUPINFOA si; memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    if (CreateProcessA(0, cmd, 0, 0, FALSE, CREATE_NO_WINDOW, 0, g_dir, &si, &pi)) {
        g_modeChanged = true;
        if (ready) WaitForSingleObject(ready, 8000);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    if (ready) CloseHandle(ready);
}

// ---------------------------------------------------------------- hooking
static void* HookCall(DWORD addr, void* fn) {
    if (*(unsigned char*)addr != 0xE8) return 0;
    DWORD old;
    if (!VirtualProtect((void*)addr, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    void* orig = (void*)(addr + 5 + *(int*)(addr + 1));
    *(int*)(addr + 1) = (int)fn - (int)(addr + 5);
    VirtualProtect((void*)addr, 5, old, &old);
    return orig;
}

static void Fail(const char* why) {
    char path[MAX_PATH]; _snprintf(path, MAX_PATH, "%sVC100Checklist.log", g_dir);
    FILE* f = fopen(path, "w");
    if (f) { fprintf(f, "not loaded: %s\n", why); fclose(f); }
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(inst);
    GetModuleFileNameA(inst, g_dir, MAX_PATH);
    char* slash = strrchr(g_dir, '\\'); if (slash) slash[1] = 0;
    LoadSettings();
    ApplyDesktopMode();

    // v1.0 check: the three call sites this plugin hooks must all be CALL instructions
    if (*(unsigned char*)0x4A64D0 != 0xE8 || *(unsigned char*)0x55AB18 != 0xE8 || *(unsigned char*)0x49B34C != 0xE8) {
        Fail("this is not the v1.0 gta-vc.exe");
        return TRUE;
    }
    g_origHud        = (void(__cdecl*)())HookCall(0x4A64D0, HudHook);
    g_origBlipsRadar = (void(__cdecl*)())HookCall(0x55AB18, BlipsRadarHook);
    g_origBlipsMap   = (void(__cdecl*)())HookCall(0x49B34C, BlipsMapHook);
    CreateThread(0, 0, FillThread, 0, 0, 0);
    return TRUE;
}
