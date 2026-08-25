// ============================================================================
// Litt Engine - Council Self-Test Demo   (native/council_demo.cpp)
// Exercises every litt_council.h API: platform detection, quality-tier
// presets, weighted voting (pass + quorum-block), manual override, final table.
// Build: g++ -std=c++17 -O2 -I. council_demo.cpp -o council_demo.exe
// ============================================================================
#include "littcore/litt_council.h"

#include <cstdio>

using litt::Council;
using litt::CouncilTier;
using litt::Feature;
using litt::Vote;

static void feature_line(const char* tag, const Council& c, Feature f) {
    std::printf("  %-8s [%s] compiled=%-3s active=%s\n", tag,
                litt::to_string(f), Council::compiled(f) ? "Y" : "N",
                c.active(f) ? "ON" : "off");
}

static void dump(const char* title, const Council& c) {
    std::printf("%s\n", title);
    for (size_t i = 0; i < litt::kFeatureCount; ++i)
        feature_line("", c, static_cast<Feature>(i));
}

int main() {
    // --- platform -------------------------------------------------------------
    std::printf("== LITT ENGINE COUNCIL SELF-TEST ==\n");
    std::printf("platform          : %s\n\n", litt::to_string(litt::host_platform()));

    // --- quality tiers --------------------------------------------------------
    for (CouncilTier t : {CouncilTier::Low, CouncilTier::Medium, CouncilTier::High, CouncilTier::Ultra}) {
        Council c;
        c.apply_tier(t);
        std::printf("tier %-6s loads :", litt::to_string(t));
        for (size_t i = 0; i < litt::kFeatureCount; ++i)
            if (c.active(static_cast<Feature>(i)))
                std::printf(" %s", litt::to_string(static_cast<Feature>(i)));
        std::printf("\n");
    }
    std::printf("\n");

    // --- voters ---------------------------------------------------------------
    Council council;
    const size_t lead = council.add_voter({"lead", 3});   // weight 3
    const size_t gfx  = council.add_voter({"gfx", 2});    // weight 2
    const size_t qa   = council.add_voter({"qa"});        // weight 1 (default)
    std::printf("seats registered  : lead(w3)=id%zu  gfx(w2)=id%zu  qa(w1)=id%zu\n",
                lead, gfx, qa);
    std::printf("total seat weight : 6, default quorum 0.50 -> needs turnout >= 3\n\n");

    // --- vote 1: PASSES (yes 5 > no 1, turnout 6/6) ----------------------------
    council.vote(lead, Feature::Renderer, Vote::Yes);
    council.vote(gfx,  Feature::Renderer, Vote::Yes);
    council.vote(qa,   Feature::Renderer, Vote::No);
    std::printf("motion 'load renderer'  : yes 5 vs no 1 ... %s\n",
                council.decide(Feature::Renderer) ? "PASSED" : "FAILED");

    // --- vote 2: BLOCKED BY QUORUM (turnout 1/6 < 0.5) --------------------------
    council.vote(qa, Feature::Audio, Vote::Yes);          // lone w1 voter
    std::printf("motion 'load audio'     : yes 1, abstain 5 ... %s\n",
                council.decide(Feature::Audio) ? "PASSED" : "BLOCKED BY QUORUM");

    // --- manual override -------------------------------------------------------
    council.override_feature(Feature::Audio, true);
    std::printf("override audio -> ON    : active=%s\n",
                council.active(Feature::Audio) ? "true" : "false");
    council.override_feature(Feature::Physics, false);
    std::printf("override physics -> OFF : active=%s\n\n",
                council.active(Feature::Physics) ? "true" : "false");

    // --- final table ------------------------------------------------------------
    dump("FINAL COUNCIL STATE (compiled-in && currently loaded):", council);
    return 0;
}
