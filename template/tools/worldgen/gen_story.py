#!/usr/bin/env python3
"""gen_story.py - author the narrative layer for a generated world.

Given an about-phrase, theme, archetype family and a SCALE, deterministically
writes into <game>/story/:

    story.md      - human-readable: title, tagline, lore, acts, quests
    items.json    - item defs {name, kind, rarity, description}
    roster.json   - cast {name, role, tier, description}

The AI pipeline feeds items/roster back through brief.json -> enrich_game,
so every described item becomes a real pickup and every roster entry a real
placed enemy. Same seed => same story, forever.
"""
import argparse
import json
import random
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]

# ---------------------------------------------------------------- word banks
SOULS = dict(
    adj=["hollow", "embered", "ashen", "forsaken", "twilight", "cindered",
         "pale", "silent", "ruined", "unlit"],
    noun=["oath", "ember", "pyre", "relic", "shrine", "vigil", "cinder",
          "requiem", "bell", "grave", "sigil", "ash"],
    place=["Court", "Depths", "Parish", "Garden", "Sanctum", "Kiln",
           "Catacombs", "Bastion", "Chapel", "Reach"],
    boss=["the Hollow King", "Sister Cinder", "the Pale Warden",
          "Vow-Knight Aldric", "the Unlit Choir", "Grave-Saint Maren",
          "the Ember-Eyed Hound", "Warden of the First Pyre"],
    item_kind=["weapon", "armor", "trinket", "consumable", "relic"],
    verbs=["kindle", "claim", "break", "swear", "carry", "extinguish"],
)
SPACE = dict(
    adj=["quantum", "derelict", "nebular", "void-touched", "ionized",
         "orbital", "stellar", "fractured"],
    noun=["signal", "wreck", "beacon", "protocol", "anomaly", "cargo",
          "jumpgate", "chorus", "drift"],
    place=["Station", "Drift", "Nebula", "Yard", "Gate", "Halo", "Belt"],
    boss=["ARCHON-9", "the Salvage Queen", "Null-Captain Vess",
          "the Silent Armada", "Overseer Kell"],
    item_kind=["module", "augment", "core", "consumable", "blueprint"],
    verbs=["intercept", "salvage", "decode", "outrun", "dock"],
)
FANTASY = dict(
    adj=["gilded", "thornbound", "ancient", "moonlit", "wyrm-sung",
         "verdant", "forgotten"],
    noun=["crown", "grove", "riddle", "blade", "chalice", "warden",
          "prophecy", "briar"],
    place=["Wood", "Keep", "Vale", "Throne", "Crossing", "Hollow"],
    boss=["the Thorn Queen", "High Warden Elucien", "the Wyrm of Vale",
          "the Gilded Usurper"],
    item_kind=["weapon", "armor", "trinket", "consumable", "scroll"],
    verbs=["seek", "guard", "unravel", "restore"],
)

BANKS = {
    "souls": SOULS, "space": SPACE, "fantasy": FANTASY,
}
# archetype-family -> bank
FAMILY = {}
for name in ("souls", "dark_fantasy", "roguelike", "metroidvania"):
    FAMILY[name] = "souls"
for name in ("space", "scifi", "mecha", "post_apocalyptic", "cyberpunk"):
    FAMILY[name] = "space"
FAMILY["default"] = "fantasy"

SCALES = {
    "small": dict(acts=1, items=6, roster=6, bosses=1),
    "medium": dict(acts=2, items=14, roster=10, bosses=2),
    "full": dict(acts=4, items=30, roster=16, bosses=4),
}

ACT_TITLES = [
    "The Long Approach", "What the Bells Buried", "Ash Below, Sky Above",
    "The Turning of the Vigil", "Where Names Are Spent",
    "The Last Kindling", "Echoes of the First Sin", "Homecoming",
]

QUEST_SHAPES = [
    "Reach {place} and {verb} the {noun} before rival seekers do.",
    "Recover the {adj} {noun}; it is the key to the next threshold.",
    "{verb} three {noun}s hidden across the level - each one quiets a bell.",
    "An NPC at {place} trades passage for the {adj} {noun}.",
    "Survive the gauntlet past {place}; the road narrows behind you.",
]


def _pick(rng, words):
    return rng.choice(words)


def build_items(rng, bank, n):
    kinds = bank["item_kind"]
    used = set()
    items = []
    rarities = ["common"] * 5 + ["uncommon"] * 3 + ["rare"] * 2 + ["legendary"]
    while len(items) < n:
        base = "%s %s" % (_pick(rng, bank["adj"]), _pick(rng, bank["noun"]))
        if base.lower() in used:
            continue
        used.add(base.lower())
        kind = _pick(rng, kinds)
        rarity = rng.choice(rarities)
        desc = {
            "common": "A %s %s. Serviceable, unremarkable, yours." % (
                kind, _pick(rng, bank["noun"])),
            "uncommon": "Carried by someone who almost made it back.",
            "rare": "Warm to the touch. The {noun} remembers hands.".format(
                noun=_pick(rng, bank["noun"])),
            "legendary": "One was forged. One was lost. You are holding it.",
        }[rarity]
        items.append({
            "name": base.title(),
            "kind": kind,
            "rarity": rarity,
            "description": desc,
        })
    return items


def build_roster(rng, bank, n, bosses):
    roles = ["mook", "elite", "boss"]
    out = []
    used = set()
    for i in range(n):
        tier = "boss" if i < bosses else (
            "elite" if i < bosses + max(1, n // 4) else "mook")
        if tier == "boss":
            name = _pick(rng, bank["boss"])
            if name.lower() in used:
                name = "%s, %s" % (name, _pick(rng, bank["adj"]))
            desc = "Act boss. Guards the way onward and remembers every death."
            hp = 300
        elif tier == "elite":
            name = "The %s %s" % (_pick(rng, bank["adj"]).title(),
                                  _pick(rng, bank["noun"]).title())
            desc = "Elite patrol. Faster than it looks; respect its reach."
            hp = 120
        else:
            name = "%s %s" % (_pick(rng, bank["adj"]).title(),
                              _pick(rng, bank["noun"]).title())
            desc = _pick(rng, [
                "Common foe. Travels in pairs when it can.",
                "Slow to anger, slow to forget.",
                "Guards loot caches along the route.",
                "Blind to stillness; sprinting draws it.",
            ])
            hp = 40
        key = name.lower()
        if key in used:
            continue
        used.add(key)
        out.append({
            "name": name, "role": tier, "hp_hint": hp, "description": desc,
        })
        if len(out) >= n:
            break
    # top up if dedupe shrank the list
    j = 0
    while len(out) < n:
        j += 1
        out.append({
            "name": "%s %s %d" % (_pick(rng, bank["adj"]).title(),
                                  _pick(rng, bank["noun"]).title(), j),
            "role": "mook", "hp_hint": 40,
            "description": "Another of the endless.",
        })
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--about", required=True, help="human phrase / premise")
    ap.add_argument("--game-dir", required=True)
    ap.add_argument("--archetype", default=None)
    ap.add_argument("--theme", default=None)
    ap.add_argument("--scale", default="medium", choices=list(SCALES))
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()

    arch = (args.archetype or "").lower()
    family = FAMILY.get(arch, None)
    if family is None:
        about = args.about.lower()
        if any(w in about for w in ("space", "star", "void", "alien", "robot")):
            family = "space"
        elif any(w in about for w in (
                "soul", "dark", "knight", "dungeon", "bonfire", "curse")):
            family = "souls"
        else:
            family = "fantasy"
    bank = BANKS[family]
    sc = SCALES[args.scale]
    rng = random.Random(args.seed * 7919 + len(args.about))

    title_words = [w.capitalize() for w in args.about.split()[:3] if w.isalpha()]
    title = " ".join(title_words[:2]) or _pick(rng, bank["noun"]).title()
    title = "%s: %s %s" % (title, _pick(rng, bank["adj"]).title(),
                           _pick(rng, bank["noun"]).title())

    items = build_items(rng, bank, sc["items"])
    roster = build_roster(rng, bank, sc["roster"], sc["bosses"])

    acts = []
    for a in range(sc["acts"]):
        place = _pick(rng, bank["place"])
        quests = []
        for q in range(2 if args.scale == "small" else 3):
            shape = ACT_TITLES[(a + q) % len(ACT_TITLES)]
            quests.append(QUEST_SHAPES[(a * 3 + q) % len(QUEST_SHAPES)].format(
                place=place, verb=_pick(rng, bank["verbs"]),
                noun=_pick(rng, bank["noun"]), adj=_pick(rng, bank["adj"])))
            del shape
        boss = roster[a]["name"] if a < sc["bosses"] else "an unnamed horror"
        acts.append({
            "title": ACT_TITLES[a % len(ACT_TITLES)],
            "place": place,
            "quests": quests,
            "boss": boss,
        })

    lines = [
        "# %s" % title,
        "",
        "*%s*" % args.about,
        "",
        "## Lore",
        "Before the %s, the %s kept the %s. Then came the %s age:"
        " bells went quiet, names were spent, and something ancient began"
        " to wait beneath %s." % (
            _pick(rng, bank["noun"]), _pick(rng, bank["adj"]),
            _pick(rng, bank["place"]), _pick(rng, bank["adj"]),
            _pick(rng, bank["place"])),
        "",
        "You arrive with nothing but %s and a reason you refuse to say"
        " aloud." % _pick(rng, bank["item_kind"]),
        "",
    ]
    for i, act in enumerate(acts, 1):
        lines += ["## Act %d - %s (%s)" % (i, act["title"], act["place"]), ""]
        for q in act["quests"]:
            lines += ["- %s" % q]
        lines += ["- **Boss:** %s" % act["boss"], ""]
    lines += ["## Items (%d)" % len(items), ""]
    for it in items:
        lines.append("- **%s** *(%s, %s)* - %s" % (
            it["name"], it["kind"], it["rarity"], it["description"]))
    lines += ["", "## Roster (%d)" % len(roster), ""]
    for r in roster:
        lines.append("- **%s** [%s] - %s" % (r["name"], r["role"],
                                             r["description"]))
    lines.append("")

    gdir = Path(args.game_dir)
    sdir = gdir / "story"
    sdir.mkdir(parents=True, exist_ok=True)
    (sdir / "story.md").write_text("\n".join(lines), encoding="utf-8")
    (sdir / "items.json").write_text(
        json.dumps({"items": items}, indent=1), encoding="utf-8")
    (sdir / "roster.json").write_text(
        json.dumps({"roster": roster}, indent=1), encoding="utf-8")

    print(json.dumps({
        "ok": True, "story_dir": str(sdir), "scale": args.scale,
        "acts": len(acts), "items": len(items), "roster": len(roster),
    }))


if __name__ == "__main__":
    main()
