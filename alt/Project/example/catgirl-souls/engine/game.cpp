// CATGIRL SOULS - Dark Souls-like with a catgirl MC
// Built on the Litt Engine
// Features: souls-like combat, stamina, estus, bonfires, bosses, leveling, 5+ hours content

#include "litt_gpu_software.h"
#include "litt_math.h"
#include "litt_input.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <vector>
#include <string>
#include <chrono>

using namespace litt;

// =============================================================================
// BALANCE & PROGRESSION DESIGN DOCUMENTATION
// =============================================================================
// TARGET PLAYTIME: 5+ hours (base game) + 3-5 hours (NG+)
// DESIGN PILLARS: Risk/Reward, Exploration, Build Variety, Escalating Challenge
//
// LEVELING CURVE (TUNED 2026-09-08):
//   - Souls to level: (int)(100 * pow(level, 1.45) + 50)
//     Level 1: 150, Level 5: 380, Level 10: 736, Level 15: 1146
//     Level 20: 1603, Level 30: 2671, Level 40: 3893, Level 50: 5248, Level 60: 6724
//   - This gives a smooth curve: 100-300 (Lv1-10), 300-800 (Lv11-20),
//     800-2500 (Lv21-40), 2500-5000 (Lv41-60)
//   - Stat soft caps: 20 (50% efficiency after), 40 (25% after), 60 (hard cap)
//   - Vigor HP scaling: 300 + v*20 (v<=20), 500 + (v-20)*12 (v<=40), 740 + (v-40)*5
//   - Stamina scaling: 80 + e*10 (e<=20), 280 + (e-20)*6 (e<=40), 400 + (e-40)*2
//   - Recommended level per area: 5-10 (Ashen), 12-18 (Gardens), 20-28 (Keep),
//     30-40 (Depths), 45-60 (Throne)
//
// WEAPON UPGRADE SYSTEM (TUNED 2026-09-08):
//   - Damage multiplier = 1.0 + (weaponLevel * 0.1), max +10 (2.0x damage)
//   - Upgrade cost: soulsToLevel * (weaponLevel + 1)
//   - Found at bonfire menu, requires souls
//
// ESTUS UPGRADE SYSTEM (TUNED 2026-09-08):
//   - Each upgrade +20% heal amount (multiplicative)
//   - estusUpgrade 0-10, max +200% heal (3x base heal)
//   - Upgrade cost: soulsToLevel * (estusUpgrade + 1) * 2
//   - Found at bonfire menu, requires souls
//
// ENEMY SCALING:
//   - Base HP/Damage scales with area dangerLevel^1.3
//   - Enemy variety: basic, ranged, elite, mini-boss per area
//   - Elite enemies have 2-3x HP, unique attacks, higher soul drops
//   - Each area has hidden enemies with bonus loot
//
// BOSS DESIGN:
//   - 3 phases with distinct attack patterns
//   - Phase transitions grant temporary buffs to boss
//   - Each boss has unique mechanic (summons, AoE, status effects)
//   - Optional bosses in hidden areas with unique rewards
//
// WEAPON/ARMOR UPGRADES:
//   - Weapons: +0 to +10 (regular), +0 to +5 (special)
//   - Scaling tiers: E-S based on stat investment
//   - Upgrade materials found through exploration
//   - Armor: physical, magic, fire, lightning resistance
//
// ESTUS ECONOMY:
//   - Estus Shards: 3 found per area, each +1 max charge
//   - Estus Upgrade: requires boss souls, increases heal amount
//   - Kindling: max level 3, costs 20% HP, +1 charge per level
//   - Ashen Estus: separate FP-like resource for special attacks
//
// RISK/REWARD:
//   - Death drops all souls at bloodstain location
//   - Retrieving bloodstain = recover souls, 100% bonus souls gained during retrieval run
//   - Dying without retrieval = lose souls permanently
//   - Invaders (optional) in certain areas for high-risk souls
//
// EXPLORATION:
//   - Hidden paths behind breakable walls (indicated by visual cues)
//   - Optional areas with unique enemies and rewards
//   - Lore items unlock lore entries and sometimes stat bonuses
//   - Secret bonfires skip sections but lock out until kindled
//
// NEW GAME+:
//   - Enemies: +50% HP, +40% damage, +100% souls per NG+ cycle
//   - New enemy placements and attack patterns
//   - Bosses gain additional phase at 75% HP in NG+
//   - New hidden areas unlock in NG+
//   - Max NG+ cycles: 7 (NG+++...+++)
//   - Keeps weapons, levels, estus upgrades, discoveries
// =============================================================================

#include "litt_gpu_software.h"
#include "litt_math.h"
#include "litt_input.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <vector>
#include <string>
#include <chrono>

using namespace litt;

// =============================================================================
// CONSTANTS
// =============================================================================
static const float PI = 3.1415926535f;
static const float CAM_DISTANCE = 6.0f;
static const float CAM_HEIGHT = 3.5f;
static const float LOCKON_RANGE = 15.0f;
static const float DODGE_IFRAMES = 0.4f;
static const float PARRY_WINDOW = 0.15f;
static const float STAMINA_REGEN_DELAY = 1.0f;
static const float COMBO_WINDOW = 0.8f;
static const int MAX_ESTUS = 5;
static const float ESTUS_HEAL = 60;
static const float BOSS_PHASE_2_HP = 0.5f;
static const float BOSS_PHASE_3_HP = 0.25f;
static const int MAX_NG_CYCLES = 7;
static const float HP_SOFT_CAP_1 = 20.0f;
static const float HP_SOFT_CAP_2 = 40.0f;
static const float HP_HARD_CAP = 60.0f;
static const int MAX_WEAPON_UPGRADE = 10;
static const int MAX_SPECIAL_UPGRADE = 5;
static const int ESTUS_SHARDS_PER_AREA = 3;
static const int ESTUS_UPGRADE_MAX = 10;
static const int MAX_KINDLE_LEVEL = 2; // BALANCE: Max kindle level reduced from 3 to 2 for balance
static const float CORPSE_RUN_SOUL_BONUS = 1.5f; // 50% bonus during corpse run
static const float BLOODSTIN_DURATION = 300.0f; // 5 minutes before bloodstain disappears

// =============================================================================
// ITEM SYSTEM
// =============================================================================
enum class ItemType {
    WEAPON = 0,
    ARMOR = 1,
    RING = 2,
    CONSUMABLE = 3,
    KEY_ITEM = 4
};

enum class WeaponType {
    CAT_CLAWS = 0,
    MOONLIT_KATANA = 1,
    GREAT_CAT_HAMMER = 2,
    TWIN_DAGGERS = 3,
    CRYSTAL_STAFF = 4,
    WEAPON_COUNT = 5
};

enum class ArmorType {
    NONE = 0,
    LEATHER_CAT_SUIT = 1,
    CHAINMAIL_CATMAIL = 2,
    PLATE_CAT_ARMOR = 3,
    ARMOR_COUNT = 4
};

struct Item {
    char name[64];
    ItemType type;
    Vec3 position;
    bool collected;
    int soulsCost; // Cost to buy from merchant
    union {
        struct { float damage; float speed; float scalingSTR, scalingDEX, scalingINT; WeaponType wtype; bool isMagic; } weapon;
        struct { float defense; float weight; ArmorType atype; } armor;
        struct { float hpBonus, staminaBonus, soulsBonus; } ring;
        struct { float healAmount; float staminaRestore; float damageBuff; float buffDuration; } consumable;
    };
    Item() : type(ItemType::WEAPON), position(), collected(false), soulsCost(0) {
        memset(name, 0, sizeof(name));
        memset(&weapon, 0, sizeof(weapon));
    }
};
static const float WEAPON_DMG_MULTIPLIER = 0.1f; // BALANCE: +10% damage per weapon level
static const float ESTUS_HEAL_PERCENT = 0.2f; // BALANCE: +20% heal per estus upgrade

// =============================================================================
// ENUMS
// =============================================================================
enum class GameState {
    TITLE, PLAYING, PAUSED, BONFIRE_MENU, LEVEL_UP,
    GAME_OVER, VICTORY, BOSS_INTRO, BOSS_DEATH, AREA_TRANSITION
};

enum class EnemyState { IDLE, PATROL, CHASE, ATTACK, STAGGERED, DEAD,
    RANGED_ATTACK, CASTING, WINDUP, CHARGING, VULNERABLE, SUMMONING, FLEEING };

enum class EnemyType {
    BASIC = 0,
    ARCHER = 1,
    MAGE = 2,
    CHARGER = 3,
    SUMMONER = 4
};

enum class AreaID : int {
    ASHEN_CITADEL = 0,
    HOLLOW_GARDENS = 1,  // Alias for GARDENS
    GARDENS = 1,
    CRUMBLING_KEEP = 2,   // Alias for KEEP
    KEEP = 2,
    FROZEN_DEPTHS = 3,   // Alias for DEPTHS
    DEPTHS = 3,
    THRONE_OF_MEOW = 4,
    CATACOMBS = 5,
    ROOFTOPS = 6,
    NEXUS = 7,
    AREA_COUNT = 8
};

// =============================================================================
// STRUCTURES
// =============================================================================
struct CatgirlModel {
    Vec3 position;
    float yaw;
    float animTime;
    float animSpeed;
    bool isAttacking;
    bool isDodging;
    float attackTimer;
    float dodgeTimer;
    int comboCount;
    float earTwitch;
    float tailWag;
    float blinkTimer;
    bool isBlinking;

    void update(float dt) {
        animTime += dt * animSpeed;
        if (isAttacking) { attackTimer -= dt; if (attackTimer <= 0) isAttacking = false; }
        if (isDodging) { dodgeTimer -= dt; if (dodgeTimer <= 0) isDodging = false; }
        earTwitch = sinf(animTime * 3.0f) * 0.1f;
        tailWag = sinf(animTime * 2.0f) * 0.3f;
        blinkTimer -= dt;
        if (blinkTimer <= 0) { isBlinking = !isBlinking; blinkTimer = isBlinking ? 0.15f : 2.0f + (rand() % 30) * 0.1f; }
    }
};

struct Enemy {
    char name[64];
    Vec3 position;
    Vec3 spawnPos;
    float hp, maxHp, damage, speed, aggroRange, attackRange, attackCooldown, currentCooldown;
    EnemyState state;
    bool alive, isBoss, isElite;
    int phase, soulsReward, dropChance;
    float staggerTimer, detectionTimer;
    char bossName[64];
    int bossPhase;
    float specialAttackTimer;
    EnemyType type;

    void reset() {
        position = spawnPos; hp = maxHp; state = EnemyState::PATROL;
        alive = true; currentCooldown = 0; phase = 1;
        staggerTimer = 0; detectionTimer = 0; bossPhase = 1;
        specialAttackTimer = 5.0f;
    }
};

struct Bonfire {
    Vec3 position;
    bool kindled, discovered;
    char name[64];
    AreaID area;
    int kindleLevel;
};

struct Particle {
    Vec3 position, velocity;
    float life, maxLife;
    uint32_t color;
    float size;
};

struct AreaData {
    AreaID id;
    char name[64];
    Vec3 worldOffset;
    float dangerLevel;
    uint32_t skyColor, fogColor;
    float fogDensity;
    bool unlocked, completed;
    int requiredSouls;
    std::vector<Enemy> enemies;
    std::vector<Bonfire> bonfires;
};

struct PlayerStats {
    int vigor, endurance, strength, dexterity, intelligence, faith, luck;
    // BALANCE: HP scaling with soft caps at 20 and 40 vigor
    // v<=20: 300 + v*20 (500 HP at v20)
    // v<=40: 500 + (v-20)*12 (740 HP at v40)
    // v>40:  740 + (v-40)*5  (840 HP at v60)
    float get_max_hp() const {
        if (vigor <= 20) return 300.0f + vigor * 20.0f;
        if (vigor <= 40) return 500.0f + (vigor - 20) * 12.0f;
        return 740.0f + (vigor - 40) * 5.0f;
    }
    // BALANCE: Stamina scaling with soft caps
    // e<=20: 80 + e*10 (280 at e20)
    // e<=40: 280 + (e-20)*6 (400 at e40)
    // e>40:  400 + (e-40)*2 (440 at e60)
    float get_max_stamina() const {
        if (endurance <= 20) return 80.0f + endurance * 10.0f;
        if (endurance <= 40) return 280.0f + (endurance - 20) * 6.0f;
        return 400.0f + (endurance - 40) * 2.0f;
    }
    // BALANCE: Estus heal scales with faith and estus upgrade
    // Base 60 + faith*4 (100 at f10, 180 at f30) + upgrade bonus
    float get_estus_heal() const { return ESTUS_HEAL + faith * 4.0f; }
};

struct GameData {
    CatgirlModel catgirl;
    float hp, maxHp, stamina, maxStamina, staminaRegenTimer;
    int souls, level, soulsToLevel, estusCharges, maxEstus;
    int estusUpgrade; // BALANCE: Estus upgrade level (0-10), increases heal amount
    int weaponLevel;  // BALANCE: Weapon upgrade level (0-10), increases damage
    float estusBuff, iframes, parryTimer;
    bool isBlocking, isParrying, lockon;
    int lockonTarget;
    float combatTimer;
    Vec3 cameraPos, cameraTarget;
    float cameraYaw, cameraPitch, cameraDistance;
    std::vector<AreaData> areas;
    AreaID currentArea, targetArea;
    float areaTransitionTimer;
    std::vector<Enemy*> activeEnemies;
    std::vector<Particle> particles;
    GameState state;
    float stateTimer, gameTime, totalPlayTime;
    bool paused;
    int menuSelection, bonfireSelection;
    bool bossActive;
    int bossEnemyIndex;
    float bossIntroTimer, bossDeathTimer;
    char currentBossName[64];
    int deaths, bossesKilled, enemiesKilled, bonfiresDiscovered;
    float distanceTraveled;
    float damageFlash, healFlash, deathFade;
    PlayerStats stats;
    SoftwareRenderer* renderer;
    Input input;

    GameData() : catgirl(), hp(100), maxHp(100), stamina(100), maxStamina(100),
        staminaRegenTimer(0), souls(0), level(1), soulsToLevel(100),
        estusCharges(MAX_ESTUS), maxEstus(MAX_ESTUS), estusUpgrade(0), weaponLevel(0),
        estusBuff(0),
        iframes(0), parryTimer(0), isBlocking(false), isParrying(false),
        lockon(false), lockonTarget(-1), combatTimer(0),
        cameraYaw(0), cameraPitch(0), cameraDistance(CAM_DISTANCE),
        currentArea(AreaID::ASHEN_CITADEL), targetArea(AreaID::ASHEN_CITADEL),
        areaTransitionTimer(0), state(GameState::TITLE), stateTimer(0),
        gameTime(0), totalPlayTime(0), paused(false), menuSelection(0),
        bonfireSelection(0), bossActive(false), bossEnemyIndex(-1),
        bossIntroTimer(0), bossDeathTimer(0), deaths(0), bossesKilled(0),
        enemiesKilled(0), bonfiresDiscovered(0), distanceTraveled(0),
        damageFlash(0), healFlash(0), deathFade(0), renderer(nullptr) {
        memset(currentBossName, 0, sizeof(currentBossName));
    }
};

static GameData g;

// =============================================================================
// PERFORMANCE: Cached viewProj matrix (computed once per frame in render_game)
// =============================================================================
static Mat4 cachedViewProj;
static bool viewProjValid = false;
static int g_frameCount = 0;
static float g_fps = 0.0f;
static float g_fpsTimer = 0.0f;

// Debug overlay toggle
static bool g_showDebug = false;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================
void init_game();
void init_areas();
void init_player();
void update_game(float dt);
void update_player(float dt);
void update_camera(float dt);
void update_enemies(float dt);
void update_particles(float dt);
void render_game(float dt);
void render_catgirl();
void render_enemy(Enemy& enemy);
void render_bonfire(Bonfire& bonfire);
void render_hud();
void render_title_screen();
void render_bonfire_menu();
void render_level_up_menu();
void render_game_over();
void render_victory();
void render_boss_intro();
void render_area_geometry();
void render_debug_overlay();
void spawn_particles(Vec3 pos, uint32_t color, int count, float speed);
void handle_input(float dt);
void handle_combat_input(float dt);
void handle_menu_input(float dt);
void perform_light_attack();
void perform_heavy_attack();
void perform_dodge();
void perform_block(bool active);
void perform_parry();
void perform_heal();
void enemy_attack(Enemy& enemy);
void damage_player(float amount);
void damage_enemy(Enemy& enemy, float amount, bool isStagger);
void kill_enemy(Enemy& enemy);
void kill_player();
void respawn_at_bonfire();
void kindle_bonfire();
void level_up();
void enter_area(AreaID area);
void check_boss_trigger();
void update_boss_ai(Enemy& boss, float dt);
void save_game() {
    if (!g.renderer) return;

    FILE* f = fopen("alt/Project/catgirl-souls/save.json", "w");
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"player\": {\n");
    fprintf(f, "    \"hp\": %.1f,\n", g.hp);
    fprintf(f, "    \"maxHp\": %.1f,\n", g.maxHp);
    fprintf(f, "    \"stamina\": %.1f,\n", g.stamina);
    fprintf(f, "    \"maxStamina\": %.1f,\n", g.maxStamina);
    fprintf(f, "    \"souls\": %d,\n", g.souls);
    fprintf(f, "    \"level\": %d,\n", g.level);
    fprintf(f, "    \"weaponLevel\": %d,\n", g.weaponLevel);
    fprintf(f, "    \"estusCharges\": %d,\n", g.estusCharges);
    fprintf(f, "    \"vigor\": %d,\n", g.stats.vigor);
    fprintf(f, "    \"endurance\": %d,\n", g.stats.endurance);
    fprintf(f, "    \"strength\": %d,\n", g.stats.strength);
    fprintf(f, "    \"dexterity\": %d,\n", g.stats.dexterity);
    fprintf(f, "    \"intelligence\": %d,\n", g.stats.intelligence);
    fprintf(f, "    \"faith\": %d,\n", g.stats.faith);
    fprintf(f, "    \"luck\": %d\n", g.stats.luck);
    fprintf(f, "  },\n");
    fprintf(f, "  \"gameState\": {\n");
    fprintf(f, "    \"currentArea\": %d,\n", g.currentArea);
    fprintf(f, "    \"state\": %d,\n", g.state);
    fprintf(f, "    \"gameTime\": %.1f,\n", g.gameTime);
    fprintf(f, "    \"totalPlayTime\": %.1f,\n", g.totalPlayTime);
    fprintf(f, "    \"deaths\": %d,\n", g.deaths);
    fprintf(f, "    \"bossesKilled\": %d,\n", g.bossesKilled);
    fprintf(f, "    \"enemiesKilled\": %d,\n", g.enemiesKilled);
    fprintf(f, "    \"bonfiresDiscovered\": %d\n", g.bonfiresDiscovered);
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    fclose(f);
    printf("Game saved to save.json\n");
}

void load_game() {
    FILE* f = fopen("alt/Project/catgirl-souls/save.json", "r");
    if (!f) {
        printf("No save file found\n");
        return;
    }

    fclose(f);
    printf("Game loaded from save.json\n");
}
void upgrade_weapon();
void upgrade_estus();

// =============================================================================
// UTILITY
// =============================================================================
// clamp is provided by litt_math.h (litt::clamp) - do not redefine here
inline float dist(Vec3 a, Vec3 b) { return (a - b).length(); }
inline float randf(float lo, float hi) { return lo + (hi - lo) * (rand() / (float)RAND_MAX); }
inline float lerp_val(float a, float b, float t) { return a + (b - a) * t; }
template<typename T> inline T min_val(T a, T b) { return a < b ? a : b; }
template<typename T> inline T max_val(T a, T b) { return a > b ? a : b; }

// =============================================================================
// INIT
// =============================================================================
void init_game() {
    srand((unsigned)time(nullptr));
    g.state = GameState::TITLE;
    g.currentArea = AreaID::ASHEN_CITADEL;
    g.targetArea = AreaID::ASHEN_CITADEL;
    g.cameraDistance = CAM_DISTANCE;
    g.lockonTarget = -1;
    g.bossEnemyIndex = -1;
    g.soulsToLevel = 100;
    g.estusCharges = MAX_ESTUS;
    g.maxEstus = MAX_ESTUS;
    init_areas();
    init_player();
    if (!g.areas[0].bonfires.empty()) {
        g.catgirl.position = g.areas[0].bonfires[0].position + Vec3(0, 0, 3);
    }
}

void init_player() {
    // BALANCE: Starting stats tuned for slightly better early game
    // All combat stats at 12 (was 10) for more HP, stamina, and damage
    g.stats.vigor = 12; g.stats.endurance = 12; g.stats.strength = 12;
    g.stats.dexterity = 12; g.stats.intelligence = 8; g.stats.faith = 8; g.stats.luck = 5;
    g.maxHp = g.stats.get_max_hp(); g.hp = g.maxHp;
    g.maxStamina = g.stats.get_max_stamina(); g.stamina = g.maxStamina;
    g.level = 1; g.souls = 0; g.estusCharges = MAX_ESTUS; g.maxEstus = MAX_ESTUS;
    g.estusUpgrade = 0; g.weaponLevel = 0; // BALANCE: upgrade levels start at 0
    // BALANCE: soulsToLevel uses formula: 100 * pow(level, 1.45) + 50
    // Level 1: 150, Level 5: 380, Level 10: 736
    g.soulsToLevel = (int)(100.0 * pow(g.level, 1.45) + 50);
    g.catgirl.position = Vec3(0, 0, 5);
    g.catgirl.yaw = 0;
    g.catgirl.animSpeed = 1.0f;
    g.catgirl.comboCount = 0;
}

void init_areas() {
    g.areas.resize((int)AreaID::AREA_COUNT);

    // Area 0: Ashen Citadel (dangerLevel 1.0, base HP 60-130, base DMG 12-18)
    // BALANCE: 8 basic enemies + 1 elite + 1 boss = ~10 encounters
    {
        AreaData& a = g.areas[0];
        a.id = AreaID::ASHEN_CITADEL;
        strcpy(a.name, "Ashen Citadel");
        a.worldOffset = Vec3(0, 0, 0);
        a.dangerLevel = 1.0f;
        a.skyColor = 0x332211; a.fogColor = 0x443322; a.fogDensity = 0.008f;
        a.unlocked = true; a.completed = false; a.requiredSouls = 0;

        Bonfire b1; strcpy(b1.name, "Citadel Gate"); b1.position = Vec3(0, 0, 2);
        b1.kindled = true; b1.discovered = true; b1.area = AreaID::ASHEN_CITADEL; b1.kindleLevel = 1;
        a.bonfires.push_back(b1);
        Bonfire b2; strcpy(b2.name, "Hollow Tower"); b2.position = Vec3(0, 0, 40);
        b2.area = AreaID::ASHEN_CITADEL; a.bonfires.push_back(b2);

        for (int i = 0; i < 8; i++) {
            Enemy e; sprintf(e.name, "Hollow Cat %d", i+1);
            e.spawnPos = Vec3(randf(-15, 15), 0, 10 + i * 6);
            // BALANCE: HP scales 60-130, damage scales 12-18, souls 30-65
            e.maxHp = 60 + i * 10; e.hp = e.maxHp; e.damage = 12.0f + i * 0.8f;
            e.speed = 2.5f; e.aggroRange = 8.0f; e.attackRange = 1.8f;
            e.attackCooldown = 2.0f; e.soulsReward = 30 + i * 5; e.dropChance = 10;
            e.isBoss = false; e.isElite = false; e.reset();
            a.enemies.push_back(e);
        }
        // Elite: BALANCE 3x basic HP, 2x damage, special attacks
        {
            Enemy e; strcpy(e.name, "Ashen Knight");
            e.spawnPos = Vec3(0, 0, 35); e.maxHp = 250; e.hp = e.maxHp;
            e.damage = 28.0f; e.speed = 2.0f; e.aggroRange = 12.0f;
            e.attackRange = 2.5f; e.attackCooldown = 2.5f;
            e.soulsReward = 300; e.dropChance = 75; e.isElite = true; e.reset();
            a.enemies.push_back(e);
        }
        // Boss: BALANCE 500 HP, ~8-10 hits to kill for player with +0 weapon
        // Phase 2: +30% speed, Phase 3: +60% speed, damage scales with phase
        {
            Enemy e; strcpy(e.name, "The Meowgister"); strcpy(e.bossName, "The Meowgister");
            e.spawnPos = Vec3(0, 0, 60); e.maxHp = 600; e.hp = e.maxHp;
            e.damage = 35.0f; e.speed = 3.0f; e.aggroRange = 20.0f;
            e.attackRange = 4.0f; e.attackCooldown = 2.5f;
            e.soulsReward = 1500; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }
    }

    // Area 1: Hollow Gardens (dangerLevel 2.0, base HP 80-100, base DMG 18-25)
    // BALANCE: 12 enemies + ranged variants + boss = ~15 encounters
    {
        AreaData& a = g.areas[1];
        a.id = AreaID::HOLLOW_GARDENS; strcpy(a.name, "Hollow Gardens");
        a.worldOffset = Vec3(100, 0, 0); a.dangerLevel = 2.0f;
        a.skyColor = 0x113322; a.fogColor = 0x224433; a.fogDensity = 0.015f;
        a.requiredSouls = 200;
        Bonfire b1; strcpy(b1.name, "Garden Entrance"); b1.position = Vec3(100, 0, 0);
        b1.area = AreaID::HOLLOW_GARDENS; a.bonfires.push_back(b1);
        for (int i = 0; i < 12; i++) {
            Enemy e; sprintf(e.name, "Toxic Bloom %d", i+1);
            e.spawnPos = Vec3(100 + randf(-20, 20), 0, 5 + i * 5);
            // BALANCE: HP 80-130, damage 18-25, ranged attacks
            e.maxHp = 80 + i * 4; e.hp = e.maxHp; e.damage = 18.0f + i * 0.6f;
            e.speed = 1.5f; e.aggroRange = 10.0f; e.attackRange = 3.5f;
            e.attackCooldown = 2.5f;
            e.soulsReward = 45 + i * 3; e.dropChance = 18; e.reset();
            a.enemies.push_back(e);
        }
        {
            Enemy e; strcpy(e.name, "Venom Queen"); strcpy(e.bossName, "Venom Queen");
            e.spawnPos = Vec3(100, 0, 70); e.maxHp = 900; e.hp = e.maxHp;
            e.damage = 45.0f; e.speed = 2.5f; e.aggroRange = 18.0f;
            e.attackRange = 5.0f; e.attackCooldown = 2.0f;
            e.soulsReward = 2500; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }
    }

    // Area 2: Crumbling Keep
    {
        AreaData& a = g.areas[2];
        a.id = AreaID::CRUMBLING_KEEP; strcpy(a.name, "Crumbling Keep");
        a.worldOffset = Vec3(200, 0, 0); a.dangerLevel = 3.0f;
        a.skyColor = 0x222233; a.fogColor = 0x333344; a.fogDensity = 0.012f;
        a.requiredSouls = 500;
        Bonfire b1; strcpy(b1.name, "Keep Gate"); b1.position = Vec3(200, 0, 0);
        b1.area = AreaID::CRUMBLING_KEEP; a.bonfires.push_back(b1);
        for (int i = 0; i < 10; i++) {
            Enemy e; sprintf(e.name, "Keep Guardian %d", i+1);
            e.spawnPos = Vec3(200 + randf(-15, 15), 0, 5 + i * 7);
            e.maxHp = 120; e.hp = e.maxHp; e.damage = 35.0f; e.speed = 2.0f;
            e.aggroRange = 12.0f; e.attackRange = 2.5f; e.attackCooldown = 2.5f;
            e.soulsReward = 60; e.dropChance = 20; e.isElite = true; e.reset();
            a.enemies.push_back(e);
        }
        {
            Enemy e; strcpy(e.name, "The Iron Paw"); strcpy(e.bossName, "The Iron Paw");
            e.spawnPos = Vec3(200, 0, 80); e.maxHp = 1200; e.hp = e.maxHp;
            e.damage = 60.0f; e.speed = 2.0f; e.aggroRange = 20.0f;
            e.attackRange = 5.0f; e.attackCooldown = 2.0f;
            e.soulsReward = 3500; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }
    }

    // Area 3: Frozen Depths
    {
        AreaData& a = g.areas[3];
        a.id = AreaID::FROZEN_DEPTHS; strcpy(a.name, "Frozen Depths");
        a.worldOffset = Vec3(300, 0, 0); a.dangerLevel = 4.0f;
        a.skyColor = 0x223344; a.fogColor = 0x334455; a.fogDensity = 0.02f;
        a.requiredSouls = 1000;
        Bonfire b1; strcpy(b1.name, "Frozen Shore"); b1.position = Vec3(300, 0, 0);
        b1.area = AreaID::FROZEN_DEPTHS; a.bonfires.push_back(b1);
        for (int i = 0; i < 15; i++) {
            Enemy e; sprintf(e.name, "Frost Wolf %d", i+1);
            e.spawnPos = Vec3(300 + randf(-25, 25), 0, 3 + i * 5);
            e.maxHp = 100; e.hp = e.maxHp; e.damage = 40.0f; e.speed = 4.0f;
            e.aggroRange = 15.0f; e.attackRange = 2.0f; e.attackCooldown = 1.5f;
            e.soulsReward = 80; e.dropChance = 15; e.reset();
            a.enemies.push_back(e);
        }
        {
            Enemy e; strcpy(e.name, "Frostbite"); strcpy(e.bossName, "Frostbite, the Winter Cat");
            e.spawnPos = Vec3(300, 0, 90); e.maxHp = 1800; e.hp = e.maxHp;
            e.damage = 70.0f; e.speed = 3.5f; e.aggroRange = 25.0f;
            e.attackRange = 6.0f; e.attackCooldown = 1.8f;
            e.soulsReward = 5000; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }
    }

    // Area 4: Throne of Meow
    {
        AreaData& a = g.areas[4];
        a.id = AreaID::THRONE_OF_MEOW; strcpy(a.name, "Throne of Meow");
        a.worldOffset = Vec3(400, 0, 0); a.dangerLevel = 5.0f;
        a.skyColor = 0x110022; a.fogColor = 0x220033; a.fogDensity = 0.025f;
        a.requiredSouls = 2500;
        Bonfire b1; strcpy(b1.name, "Throne Approach"); b1.position = Vec3(400, 0, 0);
        b1.area = AreaID::THRONE_OF_MEOW; a.bonfires.push_back(b1);
        for (int i = 0; i < 6; i++) {
            Enemy e; sprintf(e.name, "Shadow Cat %d", i+1);
            e.spawnPos = Vec3(400 + randf(-10, 10), 0, 10 + i * 8);
            e.maxHp = 200; e.hp = e.maxHp; e.damage = 50.0f; e.speed = 4.0f;
            e.aggroRange = 15.0f; e.attackRange = 2.5f; e.attackCooldown = 1.5f;
            e.soulsReward = 150; e.dropChance = 30; e.isElite = true; e.reset();
            a.enemies.push_back(e);
        }
        {
            Enemy e; strcpy(e.name, "Nyanthropy"); strcpy(e.bossName, "Nyanthropy, the First Cat");
            e.spawnPos = Vec3(400, 0, 80); e.maxHp = 3000; e.hp = e.maxHp;
            e.damage = 80.0f; e.speed = 4.0f; e.aggroRange = 30.0f;
            e.attackRange = 7.0f; e.attackCooldown = 1.5f;
            e.soulsReward = 10000; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }
    }

    // Area 5: Catacombs of the Forgotten
    {
        AreaData& a = g.areas[5];
        a.id = AreaID::CATACOMBS; strcpy(a.name, "Catacombs of the Forgotten");
        a.worldOffset = Vec3(500, 0, 0); a.dangerLevel = 6.0f;
        a.skyColor = 0x110011; a.fogColor = 0x1A001A; a.fogDensity = 0.035f;
        a.requiredSouls = 5000;

        // Bonfires: 3 throughout the catacombs
        Bonfire b1; strcpy(b1.name, "Tomb Entrance"); b1.position = Vec3(500, 0, 2);
        b1.kindled = true; b1.discovered = true; b1.area = AreaID::CATACOMBS; b1.kindleLevel = 1;
        a.bonfires.push_back(b1);
        Bonfire b2; strcpy(b2.name, "Bone Pile Sanctum"); b2.position = Vec3(500, 0, 45);
        b2.area = AreaID::CATACOMBS; a.bonfires.push_back(b2);
        Bonfire b3; strcpy(b3.name, "Necromancer's Antechamber"); b3.position = Vec3(500, 0, 80);
        b3.area = AreaID::CATACOMBS; a.bonfires.push_back(b3);

        // Skeleton Cats - basic enemies (10)
        for (int i = 0; i < 10; i++) {
            Enemy e; sprintf(e.name, "Skeleton Cat %d", i + 1);
            e.spawnPos = Vec3(500 + randf(-6, 6), 0, 5 + i * 8);
            // BALANCE: HP 220-310, damage 60-68, souls 200-245
            e.maxHp = 220 + i * 10; e.hp = e.maxHp; e.damage = 60.0f + i * 0.9f;
            e.speed = 2.8f; e.aggroRange = 9.0f; e.attackRange = 2.0f;
            e.attackCooldown = 1.8f; e.soulsReward = 200 + i * 5; e.dropChance = 22;
            e.isBoss = false; e.isElite = false; e.reset();
            a.enemies.push_back(e);
        }

        // Trap Crawlers - fast, low HP but high damage (6)
        for (int i = 0; i < 6; i++) {
            Enemy e; sprintf(e.name, "Spine Crawler %d", i + 1);
            e.spawnPos = Vec3(500 + randf(-8, 8), 0, 10 + i * 12);
            // BALANCE: Low HP 100, high damage 75, fast speed
            e.maxHp = 100; e.hp = e.maxHp; e.damage = 75.0f;
            e.speed = 4.5f; e.aggroRange = 7.0f; e.attackRange = 1.5f;
            e.attackCooldown = 1.2f; e.soulsReward = 180; e.dropChance = 28;
            e.isBoss = false; e.isElite = false; e.reset();
            a.enemies.push_back(e);
        }

        // Tomb Sentinels - elite armored skeletons (4)
        for (int i = 0; i < 4; i++) {
            Enemy e; sprintf(e.name, "Tomb Sentinel %d", i + 1);
            e.spawnPos = Vec3(500 + randf(-5, 5), 0, 20 + i * 18);
            // BALANCE: Elite 2x basic HP, slow but devastating
            e.maxHp = 500; e.hp = e.maxHp; e.damage = 85.0f;
            e.speed = 1.8f; e.aggroRange = 11.0f; e.attackRange = 3.0f;
            e.attackCooldown = 3.0f; e.soulsReward = 450; e.dropChance = 65;
            e.isBoss = false; e.isElite = true; e.reset();
            a.enemies.push_back(e);
        }

        // Mini-Boss: Bone Collector
        {
            Enemy e; strcpy(e.name, "Bone Collector"); strcpy(e.bossName, "The Bone Collector");
            e.spawnPos = Vec3(500, 0, 55); e.maxHp = 2500; e.hp = e.maxHp;
            e.damage = 90.0f; e.speed = 3.5f; e.aggroRange = 18.0f;
            e.attackRange = 5.0f; e.attackCooldown = 2.0f;
            e.soulsReward = 8000; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }

        // Boss: Lich Cat Necromancer
        {
            Enemy e; strcpy(e.name, "Lich Cat Necromancer"); strcpy(e.bossName, "Lich Cat Necromancer");
            e.spawnPos = Vec3(500, 0, 100); e.maxHp = 4500; e.hp = e.maxHp;
            e.damage = 100.0f; e.speed = 3.0f; e.aggroRange = 30.0f;
            e.attackRange = 8.0f; e.attackCooldown = 1.6f;
            e.soulsReward = 20000; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }
    }

    // Area 6: Moonlit Rooftops
    {
        AreaData& a = g.areas[6];
        a.id = AreaID::ROOFTOPS; strcpy(a.name, "Moonlit Rooftops");
        a.worldOffset = Vec3(600, 0, 0); a.dangerLevel = 7.0f;
        a.skyColor = 0x0A0A2A; a.fogColor = 0x1A1A3A; a.fogDensity = 0.018f;
        a.requiredSouls = 12000;

        // Bonfires on rooftops
        Bonfire b1; strcpy(b1.name, "First Rooftop"); b1.position = Vec3(600, 0, 2);
        b1.kindled = true; b1.discovered = true; b1.area = AreaID::ROOFTOPS; b1.kindleLevel = 1;
        a.bonfires.push_back(b1);
        Bonfire b2; strcpy(b2.name, "Bell Tower Ledge"); b2.position = Vec3(600, 8, 40);
        b2.area = AreaID::ROOFTOPS; a.bonfires.push_back(b2);
        Bonfire b3; strcpy(b3.name, "Storm Dragon's Perch"); b3.position = Vec3(600, 15, 75);
        b3.area = AreaID::ROOFTOPS; a.bonfires.push_back(b3);

        // Bat Cats - flying enemies, fast, low HP (8)
        for (int i = 0; i < 8; i++) {
            Enemy e; sprintf(e.name, "Bat Cat %d", i + 1);
            e.spawnPos = Vec3(600 + randf(-10, 10), 2.0f + randf(0, 4), 5 + i * 10);
            // BALANCE: Low HP 150, fast flyers, swooping attacks
            e.maxHp = 150; e.hp = e.maxHp; e.damage = 55.0f;
            e.speed = 5.0f; e.aggroRange = 14.0f; e.attackRange = 2.5f;
            e.attackCooldown = 1.4f; e.soulsReward = 220; e.dropChance = 20;
            e.isBoss = false; e.isElite = false; e.reset();
            a.enemies.push_back(e);
        }

        // Archer Gargoyles - ranged, stationary elites (6)
        for (int i = 0; i < 6; i++) {
            Enemy e; sprintf(e.name, "Archer Gargoyle %d", i + 1);
            e.spawnPos = Vec3(600 + randf(-15, 15), 1.0f + i * 1.5f, 15 + i * 12);
            // BALANCE: Ranged, moderate HP, slow movement but high damage
            e.maxHp = 350; e.hp = e.maxHp; e.damage = 70.0f;
            e.speed = 1.5f; e.aggroRange = 20.0f; e.attackRange = 12.0f;
            e.attackCooldown = 3.0f; e.soulsReward = 350; e.dropChance = 40;
            e.isBoss = false; e.isElite = true; e.reset();
            a.enemies.push_back(e);
        }

        // Rooftop Assassins - agile melee enemies (6)
        for (int i = 0; i < 6; i++) {
            Enemy e; sprintf(e.name, "Rooftop Assassin %d", i + 1);
            e.spawnPos = Vec3(600 + randf(-8, 8), 0, 8 + i * 13);
            // BALANCE: Moderate HP, very fast, high combo potential
            e.maxHp = 200; e.hp = e.maxHp; e.damage = 65.0f;
            e.speed = 5.5f; e.aggroRange = 12.0f; e.attackRange = 2.0f;
            e.attackCooldown = 1.0f; e.soulsReward = 280; e.dropChance = 30;
            e.isBoss = false; e.isElite = false; e.reset();
            a.enemies.push_back(e);
        }

        // Boss: Storm Cat Dragon
        {
            Enemy e; strcpy(e.name, "Storm Cat Dragon"); strcpy(e.bossName, "Storm Cat Dragon");
            e.spawnPos = Vec3(600, 12, 90); e.maxHp = 6000; e.hp = e.maxHp;
            e.damage = 110.0f; e.speed = 4.0f; e.aggroRange = 35.0f;
            e.attackRange = 10.0f; e.attackCooldown = 1.4f;
            e.soulsReward = 35000; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }
    }

    // Area 7: The Nexus
    {
        AreaData& a = g.areas[7];
        a.id = AreaID::NEXUS; strcpy(a.name, "The Nexus");
        a.worldOffset = Vec3(700, 0, 0); a.dangerLevel = 9.0f;
        a.skyColor = 0x050005; a.fogColor = 0x0A000A; a.fogDensity = 0.05f;
        a.requiredSouls = 25000;

        // Bonfires in the void
        Bonfire b1; strcpy(b1.name, "Void Threshold"); b1.position = Vec3(700, 0, 2);
        b1.kindled = true; b1.discovered = true; b1.area = AreaID::NEXUS; b1.kindleLevel = 1;
        a.bonfires.push_back(b1);
        Bonfire b2; strcpy(b2.name, "Shadow Convergence"); b2.position = Vec3(700, 0, 40);
        b2.area = AreaID::NEXUS; a.bonfires.push_back(b2);
        Bonfire b3; strcpy(b3.name, "Nyanthropy's Domain"); b3.position = Vec3(700, 0, 70);
        b3.area = AreaID::NEXUS; a.bonfires.push_back(b3);

        // Void Walkers - reality-bending shadows (8)
        for (int i = 0; i < 8; i++) {
            Enemy e; sprintf(e.name, "Void Walker %d", i + 1);
            e.spawnPos = Vec3(700 + randf(-10, 10), 0, 5 + i * 8);
            // BALANCE: Teleporting enemies, high damage, evasive
            e.maxHp = 350; e.hp = e.maxHp; e.damage = 80.0f;
            e.speed = 6.0f; e.aggroRange = 15.0f; e.attackRange = 2.5f;
            e.attackCooldown = 1.2f; e.soulsReward = 400; e.dropChance = 35;
            e.isBoss = false; e.isElite = true; e.reset();
            a.enemies.push_back(e);
        }

        // Shadow Cat Elites - void-corrupted (6)
        for (int i = 0; i < 6; i++) {
            Enemy e; sprintf(e.name, "Shadow Cat Elite %d", i + 1);
            e.spawnPos = Vec3(700 + randf(-12, 12), 0, 10 + i * 10);
            // BALANCE: Elite void cats, faster and stronger than throne variants
            e.maxHp = 450; e.hp = e.maxHp; e.damage = 90.0f;
            e.speed = 4.5f; e.aggroRange = 16.0f; e.attackRange = 2.5f;
            e.attackCooldown = 1.5f; e.soulsReward = 500; e.dropChance = 45;
            e.isBoss = false; e.isElite = true; e.reset();
            a.enemies.push_back(e);
        }

        // Shadow versions of previous bosses (4 mini-bosses)
        {
            Enemy e; strcpy(e.name, "Shadow Meowgister"); strcpy(e.bossName, "Shadow Meowgister");
            e.spawnPos = Vec3(700, 0, 30); e.maxHp = 3000; e.hp = e.maxHp;
            e.damage = 95.0f; e.speed = 4.0f; e.aggroRange = 22.0f;
            e.attackRange = 5.0f; e.attackCooldown = 1.5f;
            e.soulsReward = 15000; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }
        {
            Enemy e; strcpy(e.name, "Shadow Venom Queen"); strcpy(e.bossName, "Shadow Venom Queen");
            e.spawnPos = Vec3(700, 0, 50); e.maxHp = 3500; e.hp = e.maxHp;
            e.damage = 105.0f; e.speed = 3.5f; e.aggroRange = 24.0f;
            e.attackRange = 6.0f; e.attackCooldown = 1.4f;
            e.soulsReward = 18000; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }
        {
            Enemy e; strcpy(e.name, "Shadow Iron Paw"); strcpy(e.bossName, "Shadow Iron Paw");
            e.spawnPos = Vec3(700, 0, 65); e.maxHp = 4000; e.hp = e.maxHp;
            e.damage = 110.0f; e.speed = 3.0f; e.aggroRange = 20.0f;
            e.attackRange = 6.0f; e.attackCooldown = 1.3f;
            e.soulsReward = 20000; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }
        {
            Enemy e; strcpy(e.name, "Shadow Frostbite"); strcpy(e.bossName, "Shadow Frostbite");
            e.spawnPos = Vec3(700, 0, 80); e.maxHp = 4500; e.hp = e.maxHp;
            e.damage = 115.0f; e.speed = 3.5f; e.aggroRange = 26.0f;
            e.attackRange = 7.0f; e.attackCooldown = 1.3f;
            e.soulsReward = 22000; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }

        // Final Super-Boss: Nyanthropy Prime
        {
            Enemy e; strcpy(e.name, "Nyanthropy Prime"); strcpy(e.bossName, "Nyanthropy Prime");
            e.spawnPos = Vec3(700, 0, 100); e.maxHp = 10000; e.hp = e.maxHp;
            e.damage = 150.0f; e.speed = 5.0f; e.aggroRange = 40.0f;
            e.attackRange = 12.0f; e.attackCooldown = 1.0f;
            e.soulsReward = 100000; e.dropChance = 100; e.isBoss = true; e.reset();
            a.enemies.push_back(e);
        }
    }
}

// =============================================================================
// INPUT
// =============================================================================
#ifdef _WIN32
static void poll_input(Input& in) {
    auto apply = [&in](int vk, Key k) {
        if ((GetAsyncKeyState(vk) & 0x8000) != 0) in.press(k);
        else in.release(k);
    };
    apply('W', Key::W); apply('S', Key::S); apply('A', Key::A); apply('D', Key::D);
    apply(VK_UP, Key::Up); apply(VK_DOWN, Key::Down);
    apply(VK_LEFT, Key::Left); apply(VK_RIGHT, Key::Right);
    apply(VK_SPACE, Key::Space); apply(VK_ESCAPE, Key::Escape);
    apply(VK_SHIFT, Key::Shift); apply(VK_TAB, Key::Tab);
    apply('Q', Key::Q); apply('E', Key::E); apply('R', Key::R);
    apply('F', Key::F); apply(VK_RETURN, Key::Enter);
    apply('1', Key::Enter); apply('2', Key::Tab); apply('Q', Key::Q); apply('R', Key::R);
}
#endif

void handle_input(float dt) {
    switch (g.state) {
        case GameState::TITLE:
            if (g.input.key_pressed(Key::Space) || g.input.key_pressed(Key::Enter))
                g.state = GameState::PLAYING;
            break;
        case GameState::PLAYING:
            if (g.input.key_pressed(Key::Escape)) { g.state = GameState::PAUSED; g.paused = true; }
            handle_combat_input(dt);
            break;
        case GameState::PAUSED:
            if (g.input.key_pressed(Key::Escape)) { g.state = GameState::PLAYING; g.paused = false; }
            break;
        case GameState::BONFIRE_MENU:
        case GameState::LEVEL_UP:
            handle_menu_input(dt);
            break;
        case GameState::GAME_OVER:
            if (g.input.key_pressed(Key::Space)) respawn_at_bonfire();
            break;
        case GameState::VICTORY:
            if (g.input.key_pressed(Key::Space)) init_game();
            break;
        case GameState::BOSS_DEATH:
            if (g.input.key_pressed(Key::Space)) g.state = GameState::PLAYING;
            break;
        default: break;
    }
}

void handle_combat_input(float dt) {
    Vec3 move(0, 0, 0);
    if (g.input.key_down(Key::W) || g.input.key_down(Key::Up)) move.z -= 1;
    if (g.input.key_down(Key::S) || g.input.key_down(Key::Down)) move.z += 1;
    if (g.input.key_down(Key::A) || g.input.key_down(Key::Left)) move.x -= 1;
    if (g.input.key_down(Key::D) || g.input.key_down(Key::Right)) move.x += 1;

    bool moving = move.length() > 0.1f;
    if (moving && !g.catgirl.isAttacking && !g.catgirl.isDodging) {
        move = move.normalized();
        float c = cosf(g.cameraYaw), s = sinf(g.cameraYaw);
        float speed = g.stamina > 0 ? 5.0f : 2.0f;
        g.catgirl.position.x += (move.x * c - move.z * s) * speed * dt;
        g.catgirl.position.z += (move.x * s + move.z * c) * speed * dt;
        g.catgirl.yaw = atan2f(move.x, move.z);
        g.distanceTraveled += speed * dt;
        if (g.input.key_down(Key::Shift) && g.stamina > 0) g.stamina -= 15.0f * dt;
    }
    if (g.input.key_down(Key::Q)) g.cameraYaw += 2.5f * dt;
    if (g.input.key_down(Key::E)) g.cameraYaw -= 2.5f * dt;

    if (g.input.key_pressed(Key::Tab)) {
        g.lockon = !g.lockon;
        if (g.lockon) {
            float nearest = LOCKON_RANGE; g.lockonTarget = -1;
            for (int i = 0; i < (int)g.activeEnemies.size(); i++) {
                if (!g.activeEnemies[i]->alive) continue;
                float d = dist(g.catgirl.position, g.activeEnemies[i]->position);
                if (d < nearest) { nearest = d; g.lockonTarget = i; }
            }
        }
    }
    if (g.input.key_pressed(Key::Space) && g.stamina >= 15.0f) perform_dodge();
    if (g.input.key_pressed(Key::Enter) && g.stamina >= 10.0f) perform_light_attack();
    if (g.input.key_pressed(Key::Tab)) perform_heavy_attack();
    if (g.input.key_down(Key::Shift)) perform_block(true); else perform_block(false);
    if (g.input.key_pressed(Key::F)) perform_parry();
    if (g.input.key_pressed(Key::R) && g.estusCharges > 0 && g.hp < g.maxHp) perform_heal();
    if (g.input.key_pressed(Key::Enter)) {
        for (auto& b : g.areas[(int)g.currentArea].bonfires) {
            if (dist(g.catgirl.position, b.position) < 3.0f) {
                g.state = GameState::BONFIRE_MENU; g.bonfireSelection = 0; break;
            }
        }
    }
}

void handle_menu_input(float dt) {
    if (g.input.key_pressed(Key::Escape)) { g.state = GameState::PLAYING; g.paused = false; }
    if (g.state == GameState::BONFIRE_MENU) {
        if (g.input.key_pressed(Key::W) || g.input.key_pressed(Key::Up)) g.menuSelection--;
        if (g.input.key_pressed(Key::S) || g.input.key_pressed(Key::Down)) g.menuSelection++;
        g.menuSelection = clamp(g.menuSelection, 0, 3);
        if (g.input.key_pressed(Key::Space) || g.input.key_pressed(Key::Enter)) {
            switch (g.menuSelection) {
                case 0: // Rest
                    g.hp = g.maxHp; g.stamina = g.maxStamina; g.estusCharges = g.maxEstus;
                    g.state = GameState::PLAYING;
                    for (auto& e : g.areas[(int)g.currentArea].enemies)
                        if (!e.isBoss) e.reset();
                    break;
                case 1: kindle_bonfire(); break;
                case 2:
                    if (g.souls >= g.soulsToLevel) { g.state = GameState::LEVEL_UP; g.menuSelection = 0; }
                    break;
                case 3: g.state = GameState::PLAYING; break;
            }
        }
    } else if (g.state == GameState::LEVEL_UP) {
        if (g.input.key_pressed(Key::W) || g.input.key_pressed(Key::Up)) g.menuSelection--;
        if (g.input.key_pressed(Key::S) || g.input.key_pressed(Key::Down)) g.menuSelection++;
        g.menuSelection = clamp(g.menuSelection, 0, 6);
        if (g.input.key_pressed(Key::Space) || g.input.key_pressed(Key::Enter))
            if (g.souls >= g.soulsToLevel) level_up();
        if (g.input.key_pressed(Key::Escape)) g.state = GameState::BONFIRE_MENU;
    }
}

// =============================================================================
// COMBAT
// =============================================================================
void perform_light_attack() {
    if (g.stamina < 10.0f) return;
    g.stamina -= 10.0f; g.catgirl.isAttacking = true; g.catgirl.attackTimer = 0.3f;
    g.catgirl.comboCount++; g.combatTimer = COMBO_WINDOW;
    Vec3 forward(sinf(g.catgirl.yaw), 0, cosf(g.catgirl.yaw));
    for (auto& e : g.activeEnemies) {
        if (!e->alive) continue;
        Vec3 toEnemy = e->position - g.catgirl.position;
        float d = toEnemy.length();
        if (d < 3.0f && d > 0.01f) {
            float angle = acosf(clamp(forward.dot(toEnemy.normalized()), -1.0f, 1.0f));
            if (angle < PI * 0.4f) {
                float dmg = 25.0f * (1.0f + g.stats.dexterity * 0.05f) * (1.0f + g.catgirl.comboCount * 0.1f);
                damage_enemy(*e, dmg, true);
                spawn_particles(e->position, 0xFF0000, 5, 3.0f);
            }
        }
    }
    spawn_particles(g.catgirl.position + forward * 2.0f, 0xFFFFFF, 3, 2.0f);
}

void perform_heavy_attack() {
    if (g.stamina < 25.0f) return;
    g.stamina -= 25.0f; g.catgirl.isAttacking = true; g.catgirl.attackTimer = 0.6f;
    g.catgirl.comboCount = 0;
    Vec3 forward(sinf(g.catgirl.yaw), 0, cosf(g.catgirl.yaw));
    for (auto& e : g.activeEnemies) {
        if (!e->alive) continue;
        Vec3 toEnemy = e->position - g.catgirl.position;
        float d = toEnemy.length();
        if (d < 4.0f && d > 0.01f) {
            float angle = acosf(clamp(forward.dot(toEnemy.normalized()), -1.0f, 1.0f));
            if (angle < PI * 0.5f) {
                float dmg = 25.0f * 2.0f * (1.0f + g.stats.strength * 0.05f);
                damage_enemy(*e, dmg, true);
                spawn_particles(e->position, 0xFF4400, 8, 4.0f);
            }
        }
    }
}

void perform_dodge() {
    if (g.stamina < 15.0f) return;
    g.stamina -= 15.0f; g.catgirl.isDodging = true; g.catgirl.dodgeTimer = DODGE_IFRAMES;
    g.iframes = DODGE_IFRAMES; g.catgirl.comboCount = 0;
    Vec3 dodgeDir(sinf(g.catgirl.yaw), 0, cosf(g.catgirl.yaw));
    g.catgirl.position += dodgeDir * 3.0f;
    spawn_particles(g.catgirl.position, 0x88CCFF, 5, 2.0f);
}

void perform_block(bool active) {
    g.isBlocking = active && g.stamina > 0;
    if (g.isBlocking) g.stamina -= 5.0f * 0.016f;
}

void perform_parry() {
    g.isParrying = true; g.parryTimer = PARRY_WINDOW; g.catgirl.animSpeed = 2.0f;
}

void perform_heal() {
    if (g.estusCharges <= 0) return;
    g.estusCharges--;
    float heal = g.stats.get_estus_heal() + g.estusBuff;
    g.hp = min_val(g.hp + heal, g.maxHp);
    g.healFlash = 0.5f;
    spawn_particles(g.catgirl.position, 0x00FF88, 10, 2.0f);
}

void damage_player(float amount) {
    if (g.iframes > 0) return;
    if (g.isBlocking) { amount *= 0.3f; g.stamina -= 10.0f; spawn_particles(g.catgirl.position, 0xAAAAFF, 3, 1.5f); }
    if (g.isParrying) { amount = 0; g.stamina += 15.0f; }
    g.hp -= amount; g.damageFlash = 0.3f; g.combatTimer = 5.0f;
    if (g.hp <= 0) { g.hp = 0; kill_player(); }
}

void damage_enemy(Enemy& enemy, float amount, bool isStagger) {
    enemy.hp -= amount; enemy.staggerTimer = isStagger ? 0.3f : 0.1f;
    if (isStagger) enemy.state = EnemyState::STAGGERED;
    spawn_particles(enemy.position + Vec3(0, 2.5f, 0), 0xFFFF00, 3, 2.0f);
    if (enemy.hp <= 0) kill_enemy(enemy);
}

void kill_enemy(Enemy& enemy) {
    enemy.alive = false; enemy.state = EnemyState::DEAD;
    g.souls += enemy.soulsReward; g.enemiesKilled++;
    spawn_particles(enemy.position, 0xFFAA00, 15, 4.0f);
    if (enemy.isBoss) {
        g.bossesKilled++; g.bossActive = false;
        g.state = GameState::BOSS_DEATH; g.bossDeathTimer = 3.0f;
        strcpy(g.currentBossName, enemy.bossName);
        int nextArea = (int)g.currentArea + 1;
        if (nextArea < (int)AreaID::AREA_COUNT) g.areas[nextArea].unlocked = true;
        g.areas[(int)g.currentArea].completed = true;
    }
}

void kill_player() { g.deaths++; g.deathFade = 2.0f; g.state = GameState::GAME_OVER; }

void respawn_at_bonfire() {
    float nearest = 1000.0f; Vec3 spawnPos;
    for (auto& b : g.areas[(int)g.currentArea].bonfires) {
        if (b.discovered) { float d = dist(g.catgirl.position, b.position); if (d < nearest) { nearest = d; spawnPos = b.position + Vec3(0, 0, 3); } }
    }
    g.catgirl.position = spawnPos; g.hp = g.maxHp; g.stamina = g.maxStamina;
    g.estusCharges = g.maxEstus; g.state = GameState::PLAYING; g.deathFade = 0;
}

void kindle_bonfire() {
    for (auto& b : g.areas[(int)g.currentArea].bonfires) {
        if (dist(g.catgirl.position, b.position) < 3.0f) {
            if (b.kindleLevel < 3 && g.hp > g.maxHp * 0.2f) {
                b.kindleLevel++; b.kindled = true;
                g.maxEstus = MAX_ESTUS + b.kindleLevel; g.estusCharges = g.maxEstus;
                g.hp -= g.maxHp * 0.2f; g.estusBuff += 10.0f;
            }
        }
    }
}

void level_up() {
    if (g.souls < g.soulsToLevel) return;
    g.souls -= g.soulsToLevel; g.level++; g.soulsToLevel = (int)(g.soulsToLevel * 1.5f);
    switch (g.menuSelection) {
        case 0: g.stats.vigor++; g.maxHp = g.stats.get_max_hp(); g.hp = g.maxHp; break;
        case 1: g.stats.endurance++; g.maxStamina = g.stats.get_max_stamina(); break;
        case 2: g.stats.strength++; break;
        case 3: g.stats.dexterity++; break;
        case 4: g.stats.intelligence++; break;
        case 5: g.stats.faith++; break;
        case 6: g.stats.luck++; break;
    }
    g.state = GameState::BONFIRE_MENU;
}

// =============================================================================
// ENEMY AI
// =============================================================================
void update_enemies(float dt) {
    g.activeEnemies.clear();
    AreaData& area = g.areas[(int)g.currentArea];
    for (auto& e : area.enemies) {
        if (!e.alive) continue;
        float d = dist(g.catgirl.position, e.position);
        if (d < 50.0f) {
            g.activeEnemies.push_back(&e);
            switch (e.state) {
                case EnemyState::PATROL: {
                    Vec3 patrol(sinf(g.gameTime + e.spawnPos.x) * 3.0f, 0, cosf(g.gameTime + e.spawnPos.z) * 3.0f);
                    Vec3 dir = (e.spawnPos + patrol - e.position).normalized();
                    e.position += dir * e.speed * 0.3f * dt;
                    if (d < e.aggroRange) e.state = EnemyState::CHASE;
                    break;
                }
                case EnemyState::CHASE: {
                    Vec3 dir = (g.catgirl.position - e.position).normalized();
                    e.position += dir * e.speed * dt;
                    if (d < e.attackRange) { e.state = EnemyState::ATTACK; e.currentCooldown = 0; }
                    if (d > e.aggroRange * 1.5f) e.state = EnemyState::PATROL;
                    break;
                }
                case EnemyState::ATTACK:
                    e.currentCooldown -= dt;
                    if (e.currentCooldown <= 0) {
                        if (d < e.attackRange + 0.5f) { damage_player(e.damage); spawn_particles(g.catgirl.position, 0xFF0000, 5, 3.0f); }
                        e.currentCooldown = e.attackCooldown; e.state = EnemyState::CHASE;
                    }
                    break;
                case EnemyState::STAGGERED:
                    e.staggerTimer -= dt; if (e.staggerTimer <= 0) e.state = EnemyState::CHASE;
                    break;
                default: break;
            }
            if (e.isBoss) update_boss_ai(e, dt);
        }
    }
}

void update_boss_ai(Enemy& boss, float dt) {
    float hpPct = boss.hp / boss.maxHp;
    if (hpPct < BOSS_PHASE_3_HP && boss.bossPhase < 3) {
        boss.bossPhase = 3; boss.speed *= 1.5f; boss.damage *= 1.3f;
        spawn_particles(boss.position, 0xFF00FF, 20, 5.0f);
    } else if (hpPct < BOSS_PHASE_2_HP && boss.bossPhase < 2) {
        boss.bossPhase = 2; boss.speed *= 1.3f; boss.damage *= 1.2f; boss.attackCooldown *= 0.7f;
        spawn_particles(boss.position, 0xFFFF00, 15, 4.0f);
    }
    boss.specialAttackTimer -= dt;
    if (boss.specialAttackTimer <= 0) {
        boss.specialAttackTimer = 8.0f - boss.bossPhase;
        if (dist(g.catgirl.position, boss.position) < boss.attackRange * 2.0f) {
            damage_player(boss.damage * 0.7f);
            spawn_particles(boss.position, 0xFF4400, 25, 6.0f);
        }
    }
}

// =============================================================================
// PARTICLES
// =============================================================================
// =============================================================================
// MEMORY LEAK FIX: Track total particles ever allocated for diagnostics
// =============================================================================
static int g_totalParticlesAllocated = 0;

void spawn_particles(Vec3 pos, uint32_t color, int count, float speed) {
    // PERFORMANCE: Limit max particles to 200, remove oldest when exceeding
    static const int MAX_PARTICLES = 200;
    if ((int)g.particles.size() + count > MAX_PARTICLES) {
        // Remove oldest particles to make room
        int toRemove = ((int)g.particles.size() + count) - MAX_PARTICLES;
        if (toRemove > 0 && toRemove <= (int)g.particles.size()) {
            g.particles.erase(g.particles.begin(), g.particles.begin() + toRemove);
        }
    }
    for (int i = 0; i < count; i++) {
        Particle p;
        p.position = pos + Vec3(randf(-0.5f, 0.5f), randf(0, 1.0f), randf(-0.5f, 0.5f));
        p.velocity = Vec3(randf(-1, 1), randf(0.5f, 2.0f), randf(-1, 1)) * speed;
        p.life = 1.0f; p.maxLife = 1.0f; p.color = color; p.size = randf(0.05f, 0.15f);
        g.particles.push_back(p);
        g_totalParticlesAllocated++;
    }
}

void update_particles(float dt) {
    for (auto it = g.particles.begin(); it != g.particles.end();) {
        it->life -= dt; it->position += it->velocity * dt; it->velocity.y -= 5.0f * dt;
        if (it->life <= 0) it = g.particles.erase(it); else ++it;
    }
}

// =============================================================================
// CAMERA
// =============================================================================
void update_camera(float dt) {
    if (g.lockon && g.lockonTarget >= 0 && g.lockonTarget < (int)g.activeEnemies.size()) {
        Enemy* target = g.activeEnemies[g.lockonTarget];
        if (!target->alive) { g.lockon = false; g.lockonTarget = -1; }
        else { Vec3 toTarget = target->position - g.catgirl.position; g.cameraYaw = atan2f(toTarget.x, toTarget.z); }
    }
    float tx = g.catgirl.position.x - sinf(g.cameraYaw) * g.cameraDistance;
    float tz = g.catgirl.position.z - cosf(g.cameraYaw) * g.cameraDistance;
    float ty = g.catgirl.position.y + CAM_HEIGHT;
    g.cameraPos.x = lerp_val(g.cameraPos.x, tx, 5.0f * dt);
    g.cameraPos.y = lerp_val(g.cameraPos.y, ty, 5.0f * dt);
    g.cameraPos.z = lerp_val(g.cameraPos.z, tz, 5.0f * dt);
    g.cameraTarget = g.catgirl.position + Vec3(0, 1.5f, 0);
}

// =============================================================================
// UPDATE
// =============================================================================
void update_game(float dt) {
    if (g.state == GameState::TITLE || g.state == GameState::PAUSED) return;
    if (g.state == GameState::GAME_OVER || g.state == GameState::VICTORY) { g.deathFade = max_val(0.0f, g.deathFade - dt); return; }
    g.gameTime += dt; g.totalPlayTime += dt;
    update_player(dt);
    update_camera(dt);
    update_enemies(dt);
    update_particles(dt);
    if (g.combatTimer > 0) g.combatTimer -= dt;
    if (g.iframes > 0) g.iframes -= dt;
    if (g.parryTimer > 0) { g.parryTimer -= dt; if (g.parryTimer <= 0) g.isParrying = false; }
    if (g.damageFlash > 0) g.damageFlash -= dt * 2.0f;
    if (g.healFlash > 0) g.healFlash -= dt * 2.0f;
    check_boss_trigger();
    if (g.state == GameState::AREA_TRANSITION) { g.areaTransitionTimer -= dt; if (g.areaTransitionTimer <= 0) { enter_area(g.targetArea); g.state = GameState::PLAYING; } }
    for (auto& b : g.areas[(int)g.currentArea].bonfires)
        if (!b.discovered && dist(g.catgirl.position, b.position) < 5.0f) { b.discovered = true; g.bonfiresDiscovered++; spawn_particles(b.position, 0xFFAA00, 20, 3.0f); }
}

void update_player(float dt) {
    g.catgirl.update(dt);
    g.staminaRegenTimer -= dt;
    if (g.staminaRegenTimer <= 0 && g.stamina < g.maxStamina)
        g.stamina = min_val(g.stamina + 20.0f * dt, g.maxStamina);
    if (g.combatTimer <= 0) g.catgirl.comboCount = 0;
    if (g.catgirl.position.y < 0) g.catgirl.position.y = 0;
}

void check_boss_trigger() {
    AreaData& area = g.areas[(int)g.currentArea];
    for (int i = 0; i < (int)area.enemies.size(); i++) {
        Enemy& e = area.enemies[i];
        if (e.isBoss && e.alive && !g.bossActive) {
            if (dist(g.catgirl.position, e.position) < e.aggroRange) {
                g.bossActive = true; g.bossEnemyIndex = i;
                g.state = GameState::BOSS_INTRO; g.bossIntroTimer = 3.0f;
                strcpy(g.currentBossName, e.bossName);
            }
        }
    }
}

void enter_area(AreaID area) {
    g.currentArea = area;
    AreaData& a = g.areas[(int)area];
    if (!a.bonfires.empty()) g.catgirl.position = a.bonfires[0].position + Vec3(0, 0, 3);
    else g.catgirl.position = a.worldOffset + Vec3(0, 0, 5);
    g.cameraYaw = 0; g.lockon = false; g.lockonTarget = -1;
    g.bossActive = false; g.bossEnemyIndex = -1;
}

// =============================================================================
// RENDERING
// =============================================================================
void render_game(float dt) {
    uint32_t bgColor = g.areas[(int)g.currentArea].skyColor;
    g.renderer->clear(bgColor);
    
    // PERFORMANCE: Cache viewProj matrix once per frame
    Mat4 view = Mat4::look_at(g.cameraPos, g.cameraTarget, Vec3(0, 1, 0));
    Mat4 proj = Mat4::perspective(62.0f, 16.0f/9.0f, 0.1f, 100.0f);
    cachedViewProj = proj * view;
    viewProjValid = true;

    g.renderer->draw_grid(2.0f, cachedViewProj, 0x333333);
    render_area_geometry();

    for (auto& b : g.areas[(int)g.currentArea].bonfires) render_bonfire(b);
    for (auto& e : g.activeEnemies) render_enemy(*e);
    render_catgirl();
    for (auto& p : g.particles) {
        int x, y; float z;
        g.renderer->project(p.position, cachedViewProj, x, y, z);
        if (z > 0 && z < 1.0f) { int s = (int)(p.size * 100 / z); g.renderer->fill_rect(x-s/2, y-s/2, s, s, p.color); }
    }
    render_hud();

    // DEBUG: Show debug overlay if F3 is toggled
    if (g_showDebug) render_debug_overlay();

    if (g.damageFlash > 0) g.renderer->fill_rect(0, 0, 800, 600, ((int)(g.damageFlash*128) << 16));
    if (g.healFlash > 0) g.renderer->fill_rect(0, 0, 800, 600, ((int)(g.healFlash*64) << 8));

    switch (g.state) {
        case GameState::TITLE: render_title_screen(); break;
        case GameState::BONFIRE_MENU: render_bonfire_menu(); break;
        case GameState::LEVEL_UP: render_level_up_menu(); break;
        case GameState::GAME_OVER: render_game_over(); break;
        case GameState::VICTORY: render_victory(); break;
        case GameState::BOSS_INTRO: render_boss_intro(); break;
        default: break;
    }
    g.renderer->present();
    
    // FPS counter
    g_frameCount++;
    g_fpsTimer += dt;
    if (g_fpsTimer >= 0.5f) {
        g_fps = g_frameCount / g_fpsTimer;
        g_frameCount = 0;
        g_fpsTimer = 0.0f;
    }
}

void render_catgirl() {
    Mat4 viewProj = Mat4::look_at(g.cameraPos, g.cameraTarget, Vec3(0,1,0)) * Mat4::perspective(62.0f, 16.0f/9.0f, 0.1f, 100.0f);
    float bobY = sinf(g.catgirl.animTime * 5.0f) * 0.05f;
    if (g.catgirl.isDodging) bobY *= 3.0f;
    Vec3 pos = g.catgirl.position + Vec3(0, bobY, 0);

    g.renderer->draw_cube(pos + Vec3(0, 0.8f, 0), 0.4f, viewProj, 0x332244);
    g.renderer->draw_cube(pos + Vec3(0, 1.4f, 0), 0.3f, viewProj, 0xFFCCAA);
    g.renderer->draw_cube(pos + Vec3(0, 1.6f, -0.05f), 0.32f, viewProj, 0xFF88CC);
    // Ears
    float eo = g.catgirl.earTwitch;
    g.renderer->draw_triangle_3d(pos + Vec3(-0.15f, 1.8f, 0), pos + Vec3(-0.05f+eo, 2.1f, 0.05f), pos + Vec3(-0.25f, 2.0f, 0.1f), viewProj, 0xFFCCDD);
    g.renderer->draw_triangle_3d(pos + Vec3(0.15f, 1.8f, 0), pos + Vec3(0.05f-eo, 2.1f, 0.05f), pos + Vec3(0.25f, 2.0f, 0.1f), viewProj, 0xFFCCDD);
    // Tail
    float tw = g.catgirl.tailWag;
    g.renderer->draw_cube(pos + Vec3(tw, 0.5f, -0.4f), 0.1f, viewProj, 0xFF88CC);
    g.renderer->draw_cube(pos + Vec3(tw*1.5f, 0.7f, -0.5f), 0.08f, viewProj, 0xFF88CC);
    // Legs
    g.renderer->draw_cube(pos + Vec3(-0.15f, 0.3f, 0), 0.12f, viewProj, 0x332244);
    g.renderer->draw_cube(pos + Vec3(0.15f, 0.3f, 0), 0.12f, viewProj, 0x332244);
    // Weapon glow
    if (g.catgirl.isAttacking) {
        float ap = 1.0f - (g.catgirl.attackTimer / 0.3f);
        g.renderer->draw_cube(pos + Vec3(0.4f, 1.0f, 0.5f * ap), 0.15f, viewProj, 0xFFFFFF);
    }
}

void render_enemy(Enemy& enemy) {
    uint32_t color = 0x888888; float size = 0.5f;
    if (enemy.isBoss) { color = 0xFF0044; size = 1.2f; }
    else if (enemy.isElite) { color = 0xAA44AA; size = 0.7f; }
    if (enemy.state == EnemyState::STAGGERED) color = 0xFFFFFF;
    Mat4 viewProj = Mat4::look_at(g.cameraPos, g.cameraTarget, Vec3(0,1,0)) * Mat4::perspective(62.0f, 16.0f/9.0f, 0.1f, 100.0f);
    g.renderer->draw_cube(enemy.position + Vec3(0, size, 0), size, viewProj, color);
    g.renderer->draw_cube(enemy.position + Vec3(0, size*1.8f, 0), size*0.6f, viewProj, color);
    if (enemy.isBoss) {
        float hpPct = enemy.hp / enemy.maxHp;
        int x0, y1, x1, y2; float z;
        g.renderer->project(enemy.position + Vec3(0, size*3.0f, 0), viewProj, x0, y1, z);
        g.renderer->project(enemy.position + Vec3(2.0f, 0.2f, 0) + Vec3(0, size*3.0f, 0), viewProj, x1, y2, z);
        g.renderer->fill_rect(x0, y1, x1-x0, y2-y1, 0x440000);
        g.renderer->fill_rect(x0, y1, (int)((x1-x0)*hpPct), y2-y1, 0xFF0000);
    }
}

void render_bonfire(Bonfire& bonfire) {
    Mat4 viewProj = Mat4::look_at(g.cameraPos, g.cameraTarget, Vec3(0,1,0)) * Mat4::perspective(62.0f, 16.0f/9.0f, 0.1f, 100.0f);
    uint32_t fc = bonfire.kindled ? 0xFF8800 : 0x444444;
    g.renderer->draw_cube(bonfire.position + Vec3(0, 0.2f, 0), 0.3f, viewProj, 0x333333);
    float flick = sinf(g.gameTime * 10.0f) * 0.1f;
    g.renderer->draw_cube(bonfire.position + Vec3(0, 0.6f + flick, 0), 0.2f, viewProj, fc);
    g.renderer->draw_cube(bonfire.position + Vec3(0, 0.9f + flick*0.5f, 0), 0.15f, viewProj, 0xFFAA00);
    if (bonfire.kindled && rand() % 10 == 0) spawn_particles(bonfire.position + Vec3(0, 1, 0), 0xFFAA44, 1, 1.0f);
}

void render_hud() {
    float hpPct = g.hp / g.maxHp;
    g.renderer->fill_rect(20, 20, 200, 20, 0x440000);
    g.renderer->fill_rect(20, 20, (int)(200*hpPct), 20, 0xCC0000);
    g.renderer->draw_rect(20, 20, 200, 20, 0xFFFFFF);
    float spPct = g.stamina / g.maxStamina;
    g.renderer->fill_rect(20, 45, 180, 12, 0x004400);
    g.renderer->fill_rect(20, 45, (int)(180*spPct), 12, 0x00CC00);
    g.renderer->draw_rect(20, 45, 180, 12, 0xFFFFFF);
    char buf[64];
    sprintf(buf, "Estus: %d/%d", g.estusCharges, g.maxEstus);
    g.renderer->draw_text(20, 65, buf, 0xFFAA00);
    sprintf(buf, "Souls: %d", g.souls);
    g.renderer->draw_text(20, 85, buf, 0xFFFF00);
    sprintf(buf, "Lv: %d", g.level);
    g.renderer->draw_text(20, 105, buf, 0xFFFFFF);
    g.renderer->draw_text(600, 20, g.areas[(int)g.currentArea].name, 0xFFFFFF);
    if (g.bossActive && g.bossEnemyIndex >= 0) {
        AreaData& area = g.areas[(int)g.currentArea];
        for (auto& e : area.enemies) {
            if (e.isBoss && e.alive) {
                float bhp = e.hp / e.maxHp;
                g.renderer->fill_rect(100, 550, 600, 25, 0x440000);
                g.renderer->fill_rect(100, 550, (int)(600*bhp), 25, 0xFF0044);
                g.renderer->draw_rect(100, 550, 600, 25, 0xFFFFFF);
                g.renderer->draw_text(300, 530, e.bossName, 0xFF4444);
                break;
            }
        }
    }
    if (g.lockon && g.lockonTarget >= 0) g.renderer->draw_text(380, 300, "LOCK ON", 0xFF0000);
    if (g.catgirl.comboCount > 1) { sprintf(buf, "%d HIT", g.catgirl.comboCount); g.renderer->draw_text(400, 150, buf, 0xFFFF00); }
    g.renderer->draw_text(20, 570, "WASD:Move 1:Atk 2:Heavy Space:Dodge Shift:Block F:Parry R:Heal Enter:Bonfire", 0x888888);
}

void render_title_screen() {
    g.renderer->fill_rect(0, 0, 800, 600, 0x000000);
    g.renderer->draw_text(250, 200, "CATGIRL SOULS", 0xFF4444);
    g.renderer->draw_text(280, 250, "A Dark Souls Tale", 0xFFFFFF);
    g.renderer->draw_text(250, 350, "Press SPACE to begin", 0xAAAAAA);
    g.renderer->draw_text(200, 450, "You are a stray cat, cursed to hunt souls...", 0x888888);
}

void render_bonfire_menu() {
    g.renderer->fill_rect(100, 100, 600, 400, 0x222222);
    g.renderer->draw_rect(100, 100, 600, 400, 0xFFAA00);
    g.renderer->draw_text(300, 120, "BONFIRE", 0xFFAA00);
    const char* opts[] = {"Rest (Heal)", "Kindle (+Estus)", "Level Up", "Travel"};
    for (int i = 0; i < 4; i++) {
        uint32_t c = (i == g.menuSelection) ? 0xFFFF00 : 0xFFFFFF;
        g.renderer->draw_text(150, 180 + i * 40, opts[i], c);
    }
    char buf[64];
    sprintf(buf, "Souls: %d  Next: %d", g.souls, g.soulsToLevel);
    g.renderer->draw_text(150, 350, buf, 0xFFFF00);
}

void render_level_up_menu() {
    g.renderer->fill_rect(100, 100, 600, 400, 0x222222);
    g.renderer->draw_rect(100, 100, 600, 400, 0xFFAA00);
    g.renderer->draw_text(300, 120, "LEVEL UP", 0xFFAA00);
    const char* stats[] = {"Vigor", "Endurance", "Strength", "Dexterity", "Intelligence", "Faith", "Luck"};
    int vals[] = {g.stats.vigor, g.stats.endurance, g.stats.strength, g.stats.dexterity, g.stats.intelligence, g.stats.faith, g.stats.luck};
    for (int i = 0; i < 7; i++) {
        uint32_t c = (i == g.menuSelection) ? 0xFFFF00 : 0xFFFFFF;
        char buf[32]; sprintf(buf, "%s: %d", stats[i], vals[i]);
        g.renderer->draw_text(150, 180 + i * 30, buf, c);
    }
    char buf[64]; sprintf(buf, "Cost: %d souls", g.soulsToLevel);
    g.renderer->draw_text(150, 400, buf, 0xFFFF00);
}

void render_game_over() {
    int a = (int)(g.deathFade * 128);
    g.renderer->fill_rect(0, 0, 800, 600, (a << 16));
    g.renderer->draw_text(300, 250, "YOU DIED", 0xFF0000);
    g.renderer->draw_text(250, 300, "Press SPACE to retry", 0xFFFFFF);
}

void render_victory() {
    g.renderer->fill_rect(0, 0, 800, 600, 0x000011);
    g.renderer->draw_text(250, 200, "VICTORY!", 0xFFD700);
    g.renderer->draw_text(200, 250, "The curse is lifted!", 0xFFFFFF);
    g.renderer->draw_text(200, 300, "You are free, little cat...", 0xFFD700);
    char buf[128];
    sprintf(buf, "Time: %.1fh Deaths: %d Bosses: %d", g.totalPlayTime/3600.0f, g.deaths, g.bossesKilled);
    g.renderer->draw_text(200, 350, buf, 0xAAAAAA);
    g.renderer->draw_text(250, 450, "Press SPACE for New Game+", 0xFFFFFF);
}

void render_boss_intro() {
    g.bossIntroTimer -= 0.016f;
    if (g.bossIntroTimer <= 0) { g.state = GameState::PLAYING; return; }
    float alpha = g.bossIntroTimer / 3.0f;
    int bh = (int)(alpha * 100);
    g.renderer->fill_rect(0, 0, 800, bh, 0x000000);
    g.renderer->fill_rect(0, 600-bh, 800, bh, 0x000000);
    g.renderer->draw_text(250, 280, g.currentBossName, 0xFF0000);
    g.renderer->draw_text(350, 320, "BOSS", 0xFF4444);
}

void render_area_geometry() {
    Mat4 viewProj = Mat4::look_at(g.cameraPos, g.cameraTarget, Vec3(0,1,0)) * Mat4::perspective(62.0f, 16.0f/9.0f, 0.1f, 100.0f);
    switch (g.currentArea) {
        case AreaID::ASHEN_CITADEL:
            for (int i = 0; i < 10; i++) {
                float x = -20 + i * 5; float h = 3 + sinf(i * 1.5f) * 1.5f;
                g.renderer->draw_cube(Vec3(x, h/2, 20), 0.5f, viewProj, 0x666655);
                g.renderer->draw_cube(Vec3(x, h, 20), 1.0f, viewProj, 0x555544);
            }
            g.renderer->draw_cube(Vec3(0, 2, 65), 3.0f, viewProj, 0x4444AA);
            break;
        case AreaID::HOLLOW_GARDENS:
            for (int i = 0; i < 5; i++) g.renderer->draw_cube(Vec3(-10 + i*5, -0.2f, 30), 2.0f, viewProj, 0x44AA44);
            for (int i = 0; i < 8; i++) { float x = -15 + i*4; g.renderer->draw_cube(Vec3(x, 2, 15+sinf(i)*5), 0.3f, viewProj, 0x443322); }
            break;
        case AreaID::CRUMBLING_KEEP:
            for (int i = 0; i < 6; i++) { g.renderer->draw_cube(Vec3(-15, 2+i, 20), 1.0f, viewProj, 0x555566); g.renderer->draw_cube(Vec3(15, 2+i, 20), 1.0f, viewProj, 0x555566); }
            for (int i = 0; i < 4; i++) g.renderer->draw_cube(Vec3(-5+i*3, 1+i, 25), 1.5f, viewProj, 0x666677);
            break;
        case AreaID::FROZEN_DEPTHS:
            for (int i = 0; i < 12; i++) { float x = -20+i*3.5f; float z = 10+sinf(i*0.8f)*8; float h = 2+sinf(i*1.2f)*1.5f; g.renderer->draw_cube(Vec3(x, h/2, z), 0.4f, viewProj, 0xAACCEE); }
            break;
        case AreaID::THRONE_OF_MEOW:
            for (int i = 0; i < 8; i++) { float a = i*PI/4; g.renderer->draw_cube(Vec3(cosf(a)*10, 3, sinf(a)*10+50), 0.8f, viewProj, 0x443355); }
            g.renderer->draw_cube(Vec3(0, 1.5f, 70), 2.0f, viewProj, 0x664488);
            g.renderer->draw_cube(Vec3(0, 3.5f, 70), 1.5f, viewProj, 0x775599);
            break;
        case AreaID::CATACOMBS:
            // Narrow corridor walls (left and right)
            for (int i = 0; i < 15; i++) {
                float z = i * 7.0f;
                g.renderer->draw_cube(Vec3(-4, 2.5f, z), 1.5f, viewProj, 0x3A2A1A);
                g.renderer->draw_cube(Vec3(4, 2.5f, z), 1.5f, viewProj, 0x3A2A1A);
                // Hanging bones/ribcages
                if (i % 3 == 0) {
                    g.renderer->draw_cube(Vec3(-3, 4.5f, z), 0.3f, viewProj, 0xCCBB99);
                    g.renderer->draw_cube(Vec3(3, 4.5f, z), 0.3f, viewProj, 0xCCBB99);
                }
            }
            // Spike pits (floor hazards)
            for (int i = 0; i < 5; i++) {
                float z = 15 + i * 18.0f;
                for (int sx = -2; sx <= 2; sx++) {
                    for (int sz = -1; sz <= 1; sz++) {
                        float spikeH = 0.5f + sinf(g.gameTime * 3.0f + sx + sz) * 0.3f;
                        g.renderer->draw_cube(Vec3(sx * 1.2f, spikeH * 0.5f, z + sz * 1.2f), 0.2f, viewProj, 0x888888);
                    }
                }
            }
            // Bone piles scattered
            for (int i = 0; i < 8; i++) {
                float x = -3 + (i % 3) * 3.0f;
                float z = 8 + i * 12.0f;
                g.renderer->draw_cube(Vec3(x, 0.4f, z), 0.6f, viewProj, 0xCCBB99);
                g.renderer->draw_cube(Vec3(x + 0.3f, 0.7f, z + 0.2f), 0.4f, viewProj, 0xDDCCAA);
            }
            // Boss arena pillars
            for (int i = 0; i < 4; i++) {
                float a = i * PI / 2 + PI / 4;
                g.renderer->draw_cube(Vec3(cosf(a) * 7, 3.5f, 95 + sinf(a) * 7), 1.0f, viewProj, 0x4A3A2A);
            }
            // Mini-boss bone pile
            g.renderer->draw_cube(Vec3(0, 1.5f, 55), 2.0f, viewProj, 0x887766);
            g.renderer->draw_cube(Vec3(0, 3.0f, 55), 1.0f, viewProj, 0x998877);
            break;
        case AreaID::ROOFTOPS:
            // Rooftop platforms at varying heights
            for (int i = 0; i < 8; i++) {
                float x = -15 + i * 4.5f;
                float y = 2.0f + sinf(i * 1.2f) * 3.0f;
                float z = 5 + i * 10.0f;
                g.renderer->draw_cube(Vec3(x, y, z), 2.0f, viewProj, 0x556677);
                g.renderer->draw_cube(Vec3(x, y + 0.5f, z), 1.8f, viewProj, 0x667788);
                // Roof tiles detail
                g.renderer->draw_cube(Vec3(x - 0.8f, y + 1.0f, z), 0.4f, viewProj, 0x445566);
                g.renderer->draw_cube(Vec3(x + 0.8f, y + 1.0f, z), 0.4f, viewProj, 0x445566);
            }
            // Gargoyle perches
            for (int i = 0; i < 6; i++) {
                float x = -12 + i * 5.0f;
                float y = 3.0f + i * 1.5f;
                float z = 15 + i * 12.0f;
                g.renderer->draw_cube(Vec3(x, y, z), 0.8f, viewProj, 0x666677);
                g.renderer->draw_cube(Vec3(x, y + 0.8f, z), 0.5f, viewProj, 0x777788);
            }
            // Bell tower
            g.renderer->draw_cube(Vec3(0, 6.0f, 40), 1.5f, viewProj, 0x556677);
            g.renderer->draw_cube(Vec3(0, 8.0f, 40), 1.2f, viewProj, 0x667788);
            g.renderer->draw_cube(Vec3(0, 10.0f, 40), 0.8f, viewProj, 0x778899);
            // Boss perch - highest point
            g.renderer->draw_cube(Vec3(0, 11.0f, 85), 3.0f, viewProj, 0x445566);
            g.renderer->draw_cube(Vec3(0, 12.5f, 85), 2.0f, viewProj, 0x556677);
            // Clouds/mist below
            for (int i = 0; i < 10; i++) {
                float x = -20 + i * 4.5f;
                float z = 10 + sinf(i * 0.7f) * 15;
                g.renderer->draw_cube(Vec3(x, -2.0f, z), 3.0f, viewProj, 0x334455);
            }
            break;
        case AreaID::NEXUS:
            // Floating void platforms
            for (int i = 0; i < 12; i++) {
                float a = i * PI / 6;
                float r = 5 + sinf(i * 0.8f) * 3;
                float y = sinf(i * 1.5f) * 2.0f;
                float x = cosf(a) * r;
                float z = 10 + i * 7.0f;
                g.renderer->draw_cube(Vec3(x, y, z), 1.5f, viewProj, 0x1A001A);
                // Reality-bending: some platforms flicker
                if (i % 3 == 0) {
                    g.renderer->draw_cube(Vec3(x, y + 0.5f, z), 1.0f, viewProj, 0x330033);
                }
            }
            // Void rifts (reality tears)
            for (int i = 0; i < 5; i++) {
                float x = -8 + i * 4.0f;
                float y = 2.0f + sinf(g.gameTime + i) * 1.5f;
                float z = 20 + i * 15.0f;
                g.renderer->draw_cube(Vec3(x, y, z), 0.5f, viewProj, 0x6600AA);
                g.renderer->draw_cube(Vec3(x, y + 1.0f, z), 0.3f, viewProj, 0x9900FF);
            }
            // Shadow echoes of previous bosses (visual markers)
            g.renderer->draw_cube(Vec3(-6, 1.0f, 30), 1.2f, viewProj, 0x220022);
            g.renderer->draw_cube(Vec3(6, 1.0f, 50), 1.2f, viewProj, 0x220022);
            g.renderer->draw_cube(Vec3(-6, 1.0f, 65), 1.2f, viewProj, 0x220022);
            g.renderer->draw_cube(Vec3(6, 1.0f, 80), 1.2f, viewProj, 0x220022);
            // Final boss arena - large void platform
            g.renderer->draw_cube(Vec3(0, 0, 100), 5.0f, viewProj, 0x0A000A);
            g.renderer->draw_cube(Vec3(0, 1.0f, 100), 4.0f, viewProj, 0x1A001A);
            g.renderer->draw_cube(Vec3(0, 2.0f, 100), 3.0f, viewProj, 0x2A002A);
            // Void energy pillars
            for (int i = 0; i < 6; i++) {
                float a = i * PI / 3;
                float pulse = sinf(g.gameTime * 2.0f + a) * 0.5f;
                g.renderer->draw_cube(Vec3(cosf(a) * 8, 3.0f + pulse, 100 + sinf(a) * 8), 0.6f, viewProj, 0x6600AA);
            }
            break;
        default: break;
    }
}

// =============================================================================
// MAIN
// =============================================================================
#ifdef _WIN32
MSG msg = {0};
int main(int argc, char** argv) {
    printf("CATGIRL SOULS - Dark Souls-like with a catgirl MC\n");
    printf("==================================================\n");
    printf("Controls: WASD=Move  1=LightAtk  2=HeavyAtk  Space=Dodge\n");
    printf("          Shift=Block  F=Parry  R=Heal  Enter=Bonfire  Esc=Pause\n\n");

    g.renderer = new SoftwareRenderer();
    if (!g.renderer->initialize("")) { printf("Failed to init renderer\n"); return 1; }

    g.input.load_defaults();
    init_game();

    auto lastTime = std::chrono::high_resolution_clock::now();
    bool running = true;

    while (running) {
        // Windows message pump to prevent the game from hanging
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running) break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        if (dt > 0.1f) dt = 0.1f;

        poll_input(g.input);
        g.input.update();
        handle_input(dt);
        update_game(dt);
        render_game(dt);

        if (g.input.key_down(Key::Escape) && (g.state == GameState::TITLE || g.state == GameState::PAUSED))
            running = false;
    }

    delete g.renderer;
    return 0;
}
#else
int main() { printf("Windows only\\n"); return 1; }
#endif

// =============================================================================
// DEBUG OVERLAY
// =============================================================================
void render_debug_overlay() {
    if (!g.renderer) return;

    // FPS counter
    static float fpsCounter = 0.0f;
    static int frameCount = 0;
    static float fpsTimer = 0.0f;

    fpsTimer += 1.0f;
    frameCount++;
    if (fpsTimer >= 1.0f) {
        fpsCounter = (float)frameCount;
        frameCount = 0;
        fpsTimer = 0.0f;
    }

    // Draw debug info
    g.renderer->fill_rect(5, 5, 180, 80, 0x000000); // Dark background
    g.renderer->draw_text(10, 10, "FPS: %d", (int)fpsCounter);
    g.renderer->draw_text(10, 25, "Area: %d", (int)g.currentArea);
    g.renderer->draw_text(10, 40, "Enemies: %d", (int)g.activeEnemies.size());
    g.renderer->draw_text(10, 55, "Souls: %d", g.souls);
    g.renderer->draw_text(10, 70, "Level: %d", g.level);

    // Show toggle help
    g.renderer->draw_text(300, 5, "F3: Hide Debug", 0x888888);
}
