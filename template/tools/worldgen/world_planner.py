#!/usr/bin/env python3
"""world_planner.py - WorldForge planning half (CDR-011).

Turns ONE open-ended phrase into an explicit, hand-editable multi-region
world spec (schema ``litt.worldforge/1``, see docs/specs/WORLDFORGE_SPEC.md),
modeled on the WorldClaw planning agent: prompt -> structured region spec.

Modes
-----
plan:
  python world_planner.py --about "a frozen kingdom with a volcanic arena" \
      [--seed S] [--name N] [--regions K] [--loop] [--out PATH]
  Writes world_spec.json. Last stdout line is machine-readable JSON.

validate:
  python world_planner.py --spec-in world_spec.json
  Checks an existing (possibly HAND-EDITED) spec against the schema.
  Exit 0 + report, or exit 1 + violation list. This is what makes hand edits
  checkable; regeneration from an edited spec is THE feature of WorldForge.

Decomposition is deterministic: the phrase is split into segments
("a frozen kingdom" / "a volcanic arena"), tokens are matched against
genre_index.csv + design_types.json + design_rules.json + themes.json
vocabularies, and regions get themes/archetypes/patterns/generators by score
with stable tie-breaks. Same --about/--seed => byte-identical spec file.

Stdlib only. Read-only over the reference vocabularies; writes only --out.
"""
import argparse
import csv
import hashlib
import json
import math
import random
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

SCHEMA_ID = "litt.worldforge/1"
DEFAULT_SEED = 1337
DEFAULT_REGIONS = 3
MIN_REGIONS, MAX_REGIONS = 2, 5

GENERATORS = ("soulslike", "space", "tabletop", "platformer25d", "archetype")
ROLES = ("start", "middle", "finale")
PATTERNS = ("arena_ring", "corridor_run", "hub_spoke",
            "grid_board", "spline_track", "room_graph")

NAME_RE = re.compile(r"^[a-z0-9][a-z0-9-]{0,47}$")
ID_RE = re.compile(r"^[a-z0-9][a-z0-9_-]{0,63}$")
TOKEN_RE = re.compile(r"[a-z0-9']+", re.ASCII)
SEG_SPLIT_RE = re.compile(r"(?:,|;|\bwith\b|\bthen\b|\band\b|\bafter that\b)",
                          re.IGNORECASE)

TOP_KEYS = {"schema", "name", "about", "seed", "regions",
            "spawn_region", "objective_chain_hint"}
REGION_KEYS = {"id", "generator", "archetype", "pattern", "theme",
               "role", "origin", "links", "size"}

SIZE_BASE = {"soulslike": 72, "space": 96, "tabletop": 48,
             "platformer25d": 44, "archetype": 60}
SIZE_PATTERN_DELTA = {"arena_ring": -8, "spline_track": 16,
                      "room_graph": -10, "corridor_run": -6,
                      "hub_spoke": 4, "grid_board": -14}
SIZE_MIN, SIZE_MAX = 24, 140

# Fallback archetypes per generator when no archetype keyword matched.
GEN_DEFAULT_ARCH = {
    "soulslike": "soulslike",
    "space": "walking_simulator",
    "tabletop": "grid_tactics",
    "platformer25d": "precision_action",
    "archetype": "open_world_survival",
}
# Ordered rotation for fill middles when keyword hits run dry.
MIDDLE_ARCH_FALLBACK = ("walking_simulator", "open_world_survival",
                        "dungeon_crawler")

# ---------------------------------------------------------- theme keywords
# (alternatives, theme). Order = tie-break order. Themes verified against
# themes.json at load; unknown entries would be dropped loudly in tests.
THEME_KEYWORDS = [
    (("arctic", "frozen", "ice", "icy", "snow", "winter", "glacier",
      "tundra", "frost"), "arctic_expanse"),
    (("desert", "dune", "dunes", "sand", "sandy", "oasis", "sahara"),
     "desert_dunes"),
    (("egypt", "egyptian", "pyramid", "tomb", "pharaoh", "sphinx", "mummy"),
     "egyptian_desert"),
    (("tropical", "island", "islands", "beach", "pirate", "paradise",
      "lagoon", "palm"), "tropical_island"),
    (("forest", "woods", "nature", "druid", "elf", "jungle", "grove"),
     "deep_forest"),
    (("swamp", "marsh", "bog", "mire", "fen"), "swamplands"),
    (("cave", "cavern", "underground", "mine", "mines", "depths"),
     "underground_caves"),
    (("sky", "floating", "cloud", "clouds", "aerial"), "sky_islands"),
    (("underwater", "reef", "submarine", "aquatic", "ocean", "sea",
      "abyss", "sunken"), "underwater_reef"),
    (("cyberpunk", "neon", "hacker", "cyber", "synth"), "cyberpunk_neon"),
    (("retro", "scifi", "futuristic", "laser", "hologram"), "retro_scifi"),
    (("space", "station", "orbital", "galaxy", "galactic", "alien",
      "asteroid", "void", "cosmos", "starship"), "space_station_core"),
    (("steampunk", "brass", "gear", "gears", "clockwork", "victorian"),
     "steampunk_brass"),
    (("western", "west", "cowboy", "frontier", "saloon"),
     "wild_west_frontier"),
    (("city", "urban", "street", "mall", "downtown", "metropolis"),
     "modern_city_day"),
    (("night", "midnight", "noir"), "modern_city_night"),
    (("military", "war", "trench", "soldier", "outpost", "fort", "base"),
     "military_outpost"),
    (("greek", "myth", "mythology", "olympus", "titan", "zeus", "hades"),
     "greek_mythology"),
    (("haunted", "haunt", "ghost", "ghosts", "cursed", "manor", "estate",
      "spooky", "creepy", "scary", "horror", "undead", "wraith", "spirit"),
     "haunted_estate"),
    (("candy", "sweet", "pastel", "cozy", "cute", "kawaii"), "candy_land"),
    (("toy", "toys", "voxel", "playground", "blocky", "lego"),
     "toy_voxel_playground"),
    (("minimalist", "minimal", "abstract", "monochrome", "clean"),
     "minimalist_abstract"),
    (("medieval", "kingdom", "castle", "throne", "royal", "village",
      "keep"), "medieval_realism"),
    (("fantasy", "magic", "wizard", "dragon", "arcane", "enchanted"),
     "high_fantasy"),
    (("dark", "grim", "evil", "demon", "volcanic", "volcano", "lava",
      "magma", "infernal", "ember", "embers", "fire", "burning", "ash",
      "ashen"), "dark_fantasy"),
    (("apocalypse", "apocalyptic", "wasteland", "ruins", "ruined",
      "radioactive", "fallout"), "post_apocalypse"),
]

# ------------------------------------------------------- archetype keywords
ARCHETYPE_KEYWORDS = [
    (("souls", "boss", "knight", "hollow", "estus"), "soulslike"),
    (("rogue", "roguelike", "permadeath"), "roguelike"),
    (("metroidvania", "backtrack"), "metroidvania"),
    (("farming", "farm", "crop", "crops", "harvest", "agriculture"),
     "farming_sim"),
    (("heist", "rob", "steal", "burglar", "thief", "vault"), "coop_heist"),
    (("survival", "crafting", "hunger", "wilderness"), "open_world_survival"),
    (("horror", "dread", "nightmare"), "psychological_horror"),
    (("zombie", "zombies", "outbreak", "infection"), "survival_horror"),
    (("racing", "race", "kart", "drift", "speedway"), "kart_racer"),
    (("platformer", "platform", "jump", "jumps", "parkour"), "precision_action"),
    (("arena", "duel", "colosseum", "tournament", "versus", "brawler"),
     "character_action"),
    (("puzzle", "puzzles"), "physics_puzzle"),
    (("stealth", "assassin", "sneak", "infiltration"), "stealth_pure"),
    (("tower", "defense"), "tower_defense"),
    (("deckbuilder", "cards"), "deckbuilder"),
    (("dungeon", "crypt", "catacomb", "catacombs"), "dungeon_crawler"),
    (("kingdom", "realm", "empire", "reign", "rule"), "open_world_rpg"),
    (("city", "building", "manage"), "city_builder"),
    (("western", "cowboy"), "open_world_western"),
    (("naval", "pirate", "ship", "sail", "sailing"), "naval_pirate"),
    (("colony",), "colony_sim"),
    (("tycoon", "park"), "tycoon_management"),
    (("rhythm", "music", "dance", "beat"), "rhythm_music"),
    (("detective", "investigation", "mystery", "clue"), "detective_mystery"),
    (("royale",), "battle_royale"),
    (("flight", "aviation", "plane", "aircraft"), "flight_sim_aviation"),
    (("walking", "exploring", "serene", "peaceful"), "walking_simulator"),
]

# ------------------------------------------------------ generator keywords
GENERATOR_KEYWORDS = [
    (("space", "station", "orbital", "asteroid", "asteroids", "galaxy",
      "alien", "zero-g", "vacuum", "starship"), "space"),
    (("souls", "bonfire", "corpse-run", "estus", "bossrush"), "soulslike"),
    (("boardgame", "board", "chess", "tile", "tiles", "tactics", "dice",
      "cards", "tabletop", "turn-based"), "tabletop"),
    (("platformer", "side-scroller", "2-5d", "25d", "jump", "jumps"),
     "platformer25d"),
]

# --------------------------------------------------------- pattern keywords
PATTERN_KEYWORDS = [
    (("arena", "ring", "colosseum", "pit"), "arena_ring"),
    (("race", "racing", "track", "circuit", "rally", "course", "marathon"),
     "spline_track"),
    (("board", "chess", "grid", "tactics", "strategy"), "grid_board"),
    (("maze", "labyrinth", "dungeon", "rooms", "vault"), "room_graph"),
    (("corridor", "runner", "gauntlet"), "corridor_run"),
    (("hub", "town", "market", "camp", "village", "district"), "hub_spoke"),
]

# Palette-compatible families so multi-region specs feel like ONE world.
THEME_FAMILIES = {
    "fantasy": ("dark_fantasy", "high_fantasy", "medieval_realism",
                "greek_mythology"),
    "scifi": ("cyberpunk_neon", "retro_scifi", "space_station_core",
              "steampunk_brass"),
    "warm_nature": ("desert_dunes", "egyptian_desert", "tropical_island",
                    "deep_forest", "swamplands"),
    "cold_nature": ("arctic_expanse", "sky_islands", "underwater_reef"),
    "urban": ("modern_city_day", "modern_city_night", "military_outpost",
              "wild_west_frontier", "post_apocalypse"),
    "subterranean": ("underground_caves", "haunted_estate"),
    "playful": ("candy_land", "toy_voxel_playground", "minimalist_abstract"),
}

# ------------------------------------------------------------------ vocab
_VOCAB = {}


def _read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def load_vocab():
    """Load + cache the read-only reference vocabularies."""
    if _VOCAB:
        return _VOCAB
    themes = sorted(_read_json(HERE / "themes.json")["themes"].keys())
    rules = sorted(_read_json(HERE / "design_rules.json")["archetypes"].keys())
    csv_arch = {}
    with open(HERE / "genre_index.csv", newline="", encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            key = (row.get("Archetype_Key") or "").strip()
            if not key:
                continue
            pat = (row.get("Pattern_Or_Generator") or "").strip()
            if pat not in PATTERNS:
                pat = None          # e.g. "gen_soulslike.py" rows
            thm = (row.get("Suggested_Theme") or "").strip() or None
            if key not in csv_arch:  # first row wins (stable CSV order)
                csv_arch[key] = {"pattern": pat, "theme": thm}
    _VOCAB.update(themes=themes, archetypes=rules,
                  theme_set=frozenset(themes), arch_set=frozenset(rules),
                  csv=csv_arch)
    # Guard the curated tables against vocabulary drift.
    for _alts, t in THEME_KEYWORDS:
        assert t in _VOCAB["theme_set"], "unknown theme in THEME_KEYWORDS: %s" % t
    for _alts, a in ARCHETYPE_KEYWORDS:
        assert a in _VOCAB["arch_set"], "unknown archetype: %s" % a
    for g in GEN_DEFAULT_ARCH.values():
        assert g in _VOCAB["arch_set"], "unknown GEN_DEFAULT_ARCH: %s" % g
    for a in MIDDLE_ARCH_FALLBACK:
        assert a in _VOCAB["arch_set"], "unknown MIDDLE_ARCH fallback: %s" % a
    return _VOCAB


# ------------------------------------------------------------ text analysis
def tokenize(text):
    return TOKEN_RE.findall(text.lower())


def grams(tokens):
    """Unigrams + bigrams so phrases like 'battle royale' can match."""
    out = list(tokens)
    out.extend("%s %s" % (a, b) for a, b in zip(tokens, tokens[1:]))
    return out


def score_table(text_tokens, table):
    """Rank table entries against tokens.

    Per alternative, ONE contribution: exact unigram/bigram match +3, else
    substring (alt inside a gram, len>=4) +1 - never summed twice. Ranked by
    (-score, first match position, table order) - fully stable.
    Returns [(target, score, first_pos)] best first.
    """
    toks = grams(text_tokens)
    n_uni = len(text_tokens)
    ranked = []
    for order, (alts, target) in enumerate(table):
        score = 0
        first = None
        for alt in alts:
            exact = any(tok == alt for tok in toks)
            sub = any(len(alt) >= 4 and alt in tok and tok != alt
                      for tok in toks)
            if exact:
                score += 3
            elif sub:
                score += 1
            else:
                continue
            for pos, tok in enumerate(toks):
                matched = tok == alt or (len(alt) >= 4 and alt in tok)
                if matched:
                    pos_eff = pos if pos < n_uni else n_uni
                    if first is None or pos_eff < first:
                        first = pos_eff
                    break
        if score:
            ranked.append((-score, first if first is not None else 999,
                           order, target))
    ranked.sort()
    return [(t, -s, f) for s, f, o, t in ranked]


def segments(about):
    parts = [p.strip(" \t.,;:") for p in SEG_SPLIT_RE.split(about)
             if p and p.strip(" \t.,;:")]
    return parts or [about.strip()]


def analyze_segment(seg):
    toks = tokenize(seg)
    return {
        "text": seg,
        "themes": score_table(toks, THEME_KEYWORDS),
        "archs": score_table(toks, ARCHETYPE_KEYWORDS),
        "gens": score_table(toks, GENERATOR_KEYWORDS),
        "patterns": score_table(toks, PATTERN_KEYWORDS),
    }


def merge_ranks(analyses, key):
    """Combine per-segment ranked lists: earlier segments win ties."""
    seen, out = set(), []
    for an in analyses:
        for target, score, _pos in an[key]:
            if target not in seen:
                seen.add(target)
                out.append(target)
    return out


# ------------------------------------------------------------- spec builder
def slugify(text, max_len=48):
    s = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    s = re.sub(r"-{2,}", "-", s)[:max_len].strip("-")
    return s or "untitled"


def derive_seed(about, seed):
    digest = hashlib.sha256(
        ("worldforge|%s|%d" % (about, seed)).encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "big")


def family_of(theme):
    for fam, members in THEME_FAMILIES.items():
        if theme in members:
            return fam
    return None


def family_siblings(theme):
    fam = family_of(theme)
    return [t for t in THEME_FAMILIES.get(fam, ()) if t != theme]


def estimate_size(generator, pattern):
    base = SIZE_BASE[generator]
    delta = SIZE_PATTERN_DELTA.get(pattern, 0)
    return max(SIZE_MIN, min(SIZE_MAX, base + delta))


def pick_theme(rank, used, rng):
    """Best unused theme from rank; else palette-family sibling; else any.

    ``used`` is an ORDERED list (deterministic anchor order matters - never
    a set, whose iteration order varies with PYTHONHASHSEED)."""
    vocab = load_vocab()
    for cand in rank:
        if cand not in used:
            return cand
    anchors = [t for t in used if t in vocab["theme_set"]]
    pool = []
    for anchor in anchors:                       # same palette family first
        pool.extend(t for t in family_siblings(anchor) if t not in used)
    pool.extend(t for t in vocab["themes"] if t not in used)
    if not pool:
        return rng.choice(vocab["themes"])
    return pool[0]


def pick_arch(rank, used, generator, rng):
    """Best unused archetype hit; else rotation fallback. ``used`` ordered."""
    for cand in rank:
        if cand not in used:
            return cand
    rot = [a for a in MIDDLE_ARCH_FALLBACK if a not in used]
    return rot[len(used) % len(rot)] if rot else \
        GEN_DEFAULT_ARCH[generator]


def pattern_for(arch, rank, csv_row):
    """rank: plain pattern-name list (best first)."""
    if rank:
        return rank[0]
    if csv_row and csv_row.get("pattern"):
        return csv_row["pattern"]
    structure = (_read_json(HERE / "design_rules.json")["archetypes"]
                 .get(arch, {}).get("structure") or "")
    hints = (("procedural", "dungeon"), "room_graph"), \
            (("hub_spoke", "semi_open", "metroidvania"), "hub_spoke"), \
            (("mission_based", "arenas", "wave_based"), "arena_ring"), \
            (("infinite", "linear"), "corridor_run")
    for keys, pat in hints:
        if any(k in structure for k in keys):
            return pat
    return "arena_ring"


def layout_origins(sizes):
    """Origins on a line (K!=4) or diamond (K==4), y=0, ints, spacing-safe.

    Spacing rule (docs/specs/WORLDFORGE_SPEC.md section 6): every pairwise
    distance >= (size_i + size_j) / 2. GRID quantization keeps JSON tidy.
    """
    n = len(sizes)
    grid = 10
    halfsum = lambda i, j: (sizes[i] + sizes[j]) / 2.0

    def snap(v):
        return int(math.ceil(max(v, grid) / grid) * grid)

    if n == 4:                                   # diamond W/N/S/E
        arm = snap(max(halfsum(i, j) / math.sqrt(2.0)
                       for i, j in ((0, 1), (0, 2), (3, 1), (3, 2))))
        wide = snap(max(halfsum(i, j) / 2.0 for i, j in ((0, 3), (1, 2))))
        s = max(arm, wide)
        return [[-s, 0, 0], [0, 0, -s], [0, 0, s], [s, 0, 0]]
    xs, x = [0], 0
    for k in range(n - 1):                       # line along +X
        gap = snap(halfsum(k, k + 1)) + grid     # margin beyond the rule
        x += gap
        xs.append(x)
    return [[x, 0, 0] for x in xs]


def chain_hint(position, role, rid, theme, label):
    ordinal = "%d." % (position + 1)
    if role == "start":
        return ("%s start: spawn at '%s' (%s), then head on" %
                (ordinal, rid, theme))
    if role == "middle":
        return ("%s middle: cross '%s' (%s %s)" %
                (ordinal, rid, theme, label))
    return ("%s finale: final objective at '%s' (%s %s): complete the "
            "finale" % (ordinal, rid, theme, label))


def plan_world(about, seed=DEFAULT_SEED, name=None, k=DEFAULT_REGIONS,
               loop=False):
    """Build the spec dict deterministically. Pure: no IO, no clock."""
    vocab = load_vocab()
    k = max(MIN_REGIONS, min(MAX_REGIONS, int(k)))
    rng = random.Random(derive_seed(about, seed))

    segs = segments(about)
    analyses = [analyze_segment(s) for s in segs]
    global_gens = merge_ranks(analyses, "gens")
    default_gen = global_gens[0] if global_gens else "archetype"

    # Map segments onto roles: seg0->start, LAST seg->finale, inner segs->
    # middles (evenly sampled). When there are MORE segments than regions the
    # tail folds toward the finale; when there are FEWER, leftover middles get
    # empty analyses and are filled from palette-family/rotation fallbacks.
    roles = ["start"] + ["middle"] * (k - 2) + ["finale"]
    seg_for_role = {}
    if len(segs) == 1:
        seg_for_role[0], seg_for_role[k - 1] = 0, 0
    elif len(segs) >= k:
        step = (len(segs) - 1) / float(k - 1)
        for i in range(k):
            seg_for_role[i] = min(len(segs) - 1, int(round(i * step)))
        seg_for_role[0], seg_for_role[k - 1] = 0, len(segs) - 1
    else:                                   # fewer segments than regions
        last = len(segs) - 1
        seg_for_role[0], seg_for_role[k - 1] = 0, last
        span = max(1.0, (last - 1) / float(max(1, k - 2)))
        for i in range(1, k - 1):           # middles sample inner segments
            idx = 1 + int((i - 1) * span)
            seg_for_role[i] = idx if 1 <= idx <= last - 1 else None
    empty = {"themes": [], "archs": [], "gens": [], "patterns": []}

    used_themes, used_archs = [], []
    built = {}
    # Assign start FIRST, then FINALE, then middles: the strongest segment
    # opens the world and the last spoken segment closes it; fill middles
    # absorb whatever vocabulary is left over.
    for i in [0, k - 1] + list(range(1, k - 1)):
        role = roles[i]
        an = analyses[seg_for_role[i]] if seg_for_role.get(i) is not None \
            else empty
        gen_hits = [g for g, _s, _pos in an["gens"]] or list(global_gens)
        generator = gen_hits[0] if gen_hits else default_gen

        theme_rank = [t for t, _s, _pos in an["themes"]]
        theme = pick_theme(theme_rank, used_themes, rng)
        used_themes.append(theme)

        arch_rank = [a for a, _s, _pos in an["archs"]]
        if role == "start" and not arch_rank:
            arch = GEN_DEFAULT_ARCH[generator]
        elif role == "finale":
            # Finale prefers combat-capable hits (arena/boss/horror words).
            combat = [a for a in arch_rank
                      if a in ("character_action", "soulslike",
                               "survival_horror", "psychological_horror")]
            arch = (combat[0] if combat else None) or \
                pick_arch(arch_rank, used_archs, generator, rng)
        else:
            arch = pick_arch(arch_rank, used_archs, generator, rng)
        used_archs.append(arch)

        pat_rank = [p for p, _s, _pos in an["patterns"]]
        if role == "finale":
            pattern = "arena_ring" if "arena_ring" in pat_rank else \
                pattern_for(arch, pat_rank, vocab["csv"].get(arch))
        else:
            pattern = pattern_for(arch, pat_rank, vocab["csv"].get(arch))

        if generator != "archetype":
            arch = None
            pattern = None

        label = arch if arch else generator
        built[i] = {
            "id": "r%02d-%s" % (i + 1, slugify(theme or label, 32)),
            "generator": generator,
            "archetype": arch,
            "pattern": pattern,
            "theme": theme,
            "role": role,
            "origin": [0, 0, 0],         # laid out below
            "links": [],
            "size": estimate_size(generator, pattern),
        }
    regions = [built[i] for i in range(k)]
    chain_meta = [(built[i]["archetype"] or built[i]["generator"])
                  for i in range(k)]

    origins = layout_origins([r["size"] for r in regions])
    for reg, org in zip(regions, origins):
        reg["origin"] = list(org)

    for a, b in zip(regions, regions[1:]):       # the chain
        a["links"].append(b["id"])
    if loop:
        regions[-1]["links"].append(regions[0]["id"])

    hints = [chain_hint(i, r["role"], r["id"], r["theme"],
                        chain_meta[i]) for i, r in enumerate(regions)]

    return {
        "schema": SCHEMA_ID,
        "name": name or slugify(about),
        "about": about.strip(),
        "seed": int(seed),
        "regions": regions,
        "spawn_region": regions[0]["id"],
        "objective_chain_hint": hints,
    }


# ---------------------------------------------------------------- validation
def validate_spec(obj):
    """Validate a parsed spec dict. Returns list of {code, where, detail}."""
    v = []
    add = lambda code, where, detail: v.append(
        {"code": code, "where": where, "detail": detail})
    vocab = load_vocab()

    if not isinstance(obj, dict):
        add("V001", "$", "spec must be a JSON object")
        return v
    unknown = sorted(set(obj.keys()) - TOP_KEYS)
    if unknown:
        add("V026", "$", "unknown top-level keys: %s" % ", ".join(unknown))

    if obj.get("schema") != SCHEMA_ID:
        add("V002", "$.schema", "must be %r, got %r" % (SCHEMA_ID,
                                                        obj.get("schema")))
    name = obj.get("name")
    if not isinstance(name, str) or not NAME_RE.match(name or ""):
        add("V003", "$.name", "must match ^[a-z0-9][a-z0-9-]{0,47}$, got %r"
            % (name,))
    about = obj.get("about")
    if not isinstance(about, str) or not about.strip():
        add("V004", "$.about", "must be a non-empty string")
    seed = obj.get("seed")
    if isinstance(seed, bool) or not isinstance(seed, int) or seed < 0:
        add("V005", "$.seed", "must be an int >= 0, got %r" % (seed,))

    regions = obj.get("regions")
    if not isinstance(regions, list) or not (
            MIN_REGIONS <= len(regions) <= MAX_REGIONS):
        add("V006", "$.regions", "need %d..%d regions, got %r"
            % (MIN_REGIONS, MAX_REGIONS,
               len(regions) if isinstance(regions, list) else regions))
        return v

    ids, starts, finales = [], [], []
    for idx, reg in enumerate(regions):
        where = "$.regions[%d]" % idx
        if not isinstance(reg, dict):
            add("V001", where, "region must be an object")
            continue
        unknown = sorted(set(reg.keys()) - REGION_KEYS)
        if unknown:
            add("V026", where, "unknown region keys: %s" % ", ".join(unknown))

        rid = reg.get("id")
        if not isinstance(rid, str) or not ID_RE.match(rid or ""):
            add("V010", where + ".id", "bad id %r" % (rid,))
            rid = None
        elif rid in ids:
            add("V010", where + ".id", "duplicate id %r" % rid)
        else:
            ids.append(rid)

        gen = reg.get("generator")
        if gen not in GENERATORS:
            add("V011", where + ".generator",
                "must be one of %s, got %r" % ("|".join(GENERATORS), gen))

        theme = reg.get("theme")
        if theme not in vocab["theme_set"]:
            add("V012", where + ".theme", "unknown theme %r (see themes.json)"
                % (theme,))

        role = reg.get("role")
        if role not in ROLES:
            add("V013", where + ".role",
                "must be start|middle|finale, got %r" % (role,))
        elif role == "start":
            starts.append(idx)
        elif role == "finale":
            finales.append(idx)

        origin = reg.get("origin")
        ok_origin = (isinstance(origin, list) and len(origin) == 3
                     and all(isinstance(c, (int, float))
                             and not isinstance(c, bool) for c in origin))
        if not ok_origin:
            add("V014", where + ".origin",
                "must be [x, y, z] numbers, got %r" % (origin,))

        size = reg.get("size")
        if isinstance(size, bool) or not isinstance(size, int) \
                or not (SIZE_MIN <= size <= SIZE_MAX):
            add("V015", where + ".size",
                "must be int in %d..%d, got %r" % (SIZE_MIN, SIZE_MAX, size))

        links = reg.get("links")
        if not isinstance(links, list) or \
                not all(isinstance(l, str) for l in links):
            add("V016", where + ".links", "must be a list of region ids")
        else:
            if rid in links:
                add("V016", where + ".links", "self-link on %r" % rid)
            if len(set(links)) != len(links):
                add("V016", where + ".links", "duplicate link targets")
            for tgt in links:
                all_ids = [r.get("id") for r in regions
                           if isinstance(r, dict)]
                if tgt not in all_ids:
                    add("V016", where + ".links",
                        "missing link target %r" % tgt)

        arch = reg.get("archetype", None)
        if gen == "archetype":
            if arch not in vocab["arch_set"]:
                add("V017", where + ".archetype",
                    "required for generator=archetype and must exist in "
                    "design_rules.json, got %r" % (arch,))
        elif arch is not None:
            add("V017", where + ".archetype",
                "only allowed when generator=archetype, got %r" % (arch,))

        pattern = reg.get("pattern", None)
        if gen == "archetype":
            if pattern not in PATTERNS:
                add("V018", where + ".pattern",
                    "must be one of %s, got %r" % ("|".join(PATTERNS),
                                                   pattern))
        elif pattern is not None:
            add("V018", where + ".pattern",
                "only allowed when generator=archetype, got %r" % (pattern,))

    if len(starts) != 1:
        add("V020", "$.regions", "exactly one role=start required, found %d"
            % len(starts))
    if len(finales) != 1:
        add("V021", "$.regions", "exactly one role=finale required, found %d"
            % len(finales))

    spawn = obj.get("spawn_region")
    if starts and spawn != regions[starts[0]].get("id"):
        add("V022", "$.spawn_region",
            "must equal the id of the start region %r, got %r"
            % (regions[starts[0]].get("id"), spawn))
    elif not starts:
        add("V022", "$.spawn_region", "no start region to spawn in")

    hint = obj.get("objective_chain_hint")
    if not isinstance(hint, list) or len(hint) != len(regions) or \
            not all(isinstance(s, str) and s.strip() for s in hint):
        add("V023", "$.objective_chain_hint",
            "need one non-empty string per region (%d), got %r"
            % (len(regions), hint))

    # V024 spacing: dist(origin_i, origin_j) >= (size_i+size_j)/2
    good = [(i, r) for i, r in enumerate(regions)
            if isinstance(r, dict) and isinstance(r.get("origin"), list)
            and len(r["origin"]) == 3 and isinstance(r.get("size"), int)]
    for ai in range(len(good)):
        for bi in range(ai + 1, len(good)):
            ia, ra = good[ai]
            ib, rb = good[bi]
            d = math.sqrt(sum((float(ra["origin"][c]) -
                               float(rb["origin"][c])) ** 2
                              for c in range(3)))
            need = (ra["size"] + rb["size"]) / 2.0
            if d < need:
                add("V024", "$.regions[%d,%d]" % (ia, ib),
                    "origins %.1f apart but sizes need >= %.1f" % (d, need))

    # V025 directed reachability from spawn_region
    if starts and spawn == regions[starts[0]].get("id"):
        adj = {}
        for r in regions:
            if not isinstance(r, dict):
                continue
            links = r.get("links")
            adj[r.get("id")] = links if isinstance(links, list) else []
        seen, queue = set(), [spawn]
        while queue:
            cur = queue.pop()
            if cur in seen:
                continue
            seen.add(cur)
            nxt = adj.get(cur, [])
            queue.extend(t for t in nxt if isinstance(t, str))
        unreachable = [rid for rid in ids if rid and rid not in seen]
        if unreachable:
            add("V025", "$.links",
                "unreachable from spawn_region: %s" % ", ".join(unreachable))
    return v


class SpecJsonError(ValueError):
    pass


def validate_spec_file(path):
    try:
        obj = _read_json(Path(path))
    except FileNotFoundError:
        raise SpecJsonError("file not found: %s" % path)
    except json.JSONDecodeError as exc:
        raise SpecJsonError("broken JSON: %s" % exc)
    return validate_spec(obj)


# --------------------------------------------------------------------- main
def emit_violations(path, violations):
    print("[planner] INVALID spec: %s" % path)
    for item in violations:
        print("  %-5s %-28s %s" % (item["code"], item["where"],
                                   item["detail"]))
    print(json.dumps({"ok": False, "spec": str(path),
                      "violations": violations}, sort_keys=True))
    return 1


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="WorldForge planner: phrase -> editable world spec "
                    "(schema litt.worldforge/1)")
    ap.add_argument("--about", default=None,
                    help='e.g. "a frozen kingdom with a volcanic arena"')
    ap.add_argument("--seed", type=int, default=DEFAULT_SEED,
                    help="default %d (always deterministic)" % DEFAULT_SEED)
    ap.add_argument("--name", default=None, help="slug for the fused game")
    ap.add_argument("--regions", type=int, default=DEFAULT_REGIONS,
                    help="2..5 regions (default %d)" % DEFAULT_REGIONS)
    ap.add_argument("--loop", action="store_true",
                    help="add a finale->start loop link")
    ap.add_argument("--out", default="./world_spec.json",
                    help="output path (default ./world_spec.json)")
    ap.add_argument("--spec-in", default=None, dest="spec_in",
                    help="validate this spec instead of planning")
    args = ap.parse_args(argv)

    if args.spec_in:
        try:
            violations = validate_spec_file(args.spec_in)
        except SpecJsonError as exc:
            print("[planner] %s" % exc)
            print(json.dumps({"ok": False, "spec": str(args.spec_in),
                              "violations": [{"code": "V001", "where": "$",
                                              "detail": str(exc)}]},
                             sort_keys=True))
            return 1
        if violations:
            return emit_violations(args.spec_in, violations)
        spec = _read_json(Path(args.spec_in))
        print("[planner] OK: %s" % args.spec_in)
        print("[planner] schema=%s name=%s seed=%s regions=%d spawn=%s"
              % (spec.get("schema"), spec.get("name"), spec.get("seed"),
                 len(spec.get("regions", [])), spec.get("spawn_region")))
        print(json.dumps({"ok": True, "spec": str(args.spec_in),
                          "violations": []}, sort_keys=True))
        return 0

    if not args.about or not args.about.strip():
        ap.error('pass --about "<phrase>" (or --spec-in <file> to validate)')

    spec = plan_world(args.about, seed=args.seed, name=args.name,
                      k=args.regions, loop=args.loop)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(spec, indent=2, sort_keys=True,
                         ensure_ascii=True) + "\n"
    out.write_text(payload, encoding="utf-8")

    print("[planner] about=%r seed=%d" % (spec["about"], spec["seed"]))
    for reg in spec["regions"]:
        print("[planner]   %-22s %-6s gen=%-12s arch=%-22s pat=%-12s "
              "theme=%-20s at=%s links=%s"
              % (reg["id"], reg["role"], reg["generator"],
                 reg["archetype"] or "-", reg["pattern"] or "-",
                 reg["theme"], reg["origin"], ",".join(reg["links"]) or "-"))
    print("[planner] wrote %s (%d bytes); edit it freely, then check with "
          "--spec-in" % (out, len(payload.encode("utf-8"))))
    print(json.dumps({"ok": True, "spec": str(out), "name": spec["name"],
                      "seed": spec["seed"],
                      "regions": [r["id"] for r in spec["regions"]]},
                     sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
