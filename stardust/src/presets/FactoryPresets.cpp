#include "FactoryPresets.h"

std::vector<Preset> getAllFactoryPresets()
{
    // Init preset — all defaults, no bank (appears at index 0)
    std::vector<Preset> all;
    all.push_back({ "Init", defaultPresetValues(), true, {} });

    all.push_back(makePreset("Dust - Soft Blanket", "Dust", {
        {"characterMode", 0.0f}, {"characterAmount", 0.35f},
        {"destroyJitter", 0.10f}, {"destroyFader", 30000.0f}, {"destroyBits", 15.0f},
        {"destroyIn", 1.5f}, {"destroyMix", 0.28f},
        {"exciterDrive", 0.18f}, {"exciterTone", 5200.0f}, {"exciterMix", 0.10f}
    }));
    all.push_back(makePreset("Dust - Worn Loop", "Dust", {
        {"characterMode", 0.0f}, {"characterAmount", 0.55f},
        {"destroyJitter", 0.16f}, {"destroyFader", 24000.0f}, {"destroyBits", 13.0f},
        {"destroyIn", 3.0f}, {"destroyMix", 0.48f},
        {"exciterDrive", 0.20f}, {"exciterTone", 4800.0f}, {"exciterMix", 0.12f}
    }));
    all.push_back(makePreset("Dust - Vinyl Shade", "Dust", {
        {"characterMode", 0.0f}, {"characterAmount", 0.68f},
        {"destroyJitter", 0.22f}, {"destroyFader", 18000.0f}, {"destroyBits", 12.0f},
        {"destroyIn", 4.5f}, {"destroyMix", 0.58f},
        {"exciterDrive", 0.16f}, {"exciterTone", 4200.0f}, {"exciterMix", 0.08f}
    }));
    all.push_back(makePreset("Dust - Old Sampler", "Dust", {
        {"characterMode", 0.0f}, {"characterAmount", 0.80f},
        {"destroyJitter", 0.30f}, {"destroyFader", 12000.0f}, {"destroyBits", 10.0f},
        {"destroyIn", 6.0f}, {"destroyMix", 0.72f},
        {"exciterDrive", 0.22f}, {"exciterTone", 3900.0f}, {"exciterMix", 0.10f}
    }));
    all.push_back(makePreset("Dust - Fossil Tape", "Dust", {
        {"characterMode", 0.0f}, {"characterAmount", 0.92f},
        {"destroyJitter", 0.38f}, {"destroyFader", 9000.0f}, {"destroyBits", 9.0f},
        {"destroyIn", 7.5f}, {"destroyMix", 0.82f},
        {"exciterDrive", 0.18f}, {"exciterTone", 3600.0f}, {"exciterMix", 0.07f}
    }));

    all.push_back(makePreset("Glass - Clean Lift", "Glass", {
        {"characterMode", 1.0f}, {"characterAmount", 0.32f},
        {"destroyJitter", 0.01f}, {"destroyFader", 48000.0f}, {"destroyBits", 20.0f},
        {"destroyIn", 0.5f}, {"destroyMix", 0.08f},
        {"exciterDrive", 0.24f}, {"exciterTone", 9200.0f}, {"exciterMix", 0.28f}
    }));
    all.push_back(makePreset("Glass - Vocal Shine", "Glass", {
        {"characterMode", 1.0f}, {"characterAmount", 0.50f},
        {"destroyJitter", 0.02f}, {"destroyFader", 44100.0f}, {"destroyBits", 18.0f},
        {"destroyIn", 1.0f}, {"destroyMix", 0.12f},
        {"exciterDrive", 0.34f}, {"exciterTone", 10500.0f}, {"exciterMix", 0.38f}
    }));
    all.push_back(makePreset("Glass - Crystal Drums", "Glass", {
        {"characterMode", 1.0f}, {"characterAmount", 0.62f},
        {"destroyJitter", 0.03f}, {"destroyFader", 36000.0f}, {"destroyBits", 17.0f},
        {"destroyIn", 2.0f}, {"destroyMix", 0.18f},
        {"exciterDrive", 0.42f}, {"exciterTone", 11800.0f}, {"exciterMix", 0.42f}
    }));
    all.push_back(makePreset("Glass - Frozen Top", "Glass", {
        {"characterMode", 1.0f}, {"characterAmount", 0.76f},
        {"destroyJitter", 0.04f}, {"destroyFader", 32000.0f}, {"destroyBits", 16.0f},
        {"destroyIn", 2.5f}, {"destroyMix", 0.22f},
        {"exciterDrive", 0.52f}, {"exciterTone", 13500.0f}, {"exciterMix", 0.48f}
    }));
    all.push_back(makePreset("Glass - Diamond Edge", "Glass", {
        {"characterMode", 1.0f}, {"characterAmount", 0.88f},
        {"destroyJitter", 0.05f}, {"destroyFader", 30000.0f}, {"destroyBits", 15.0f},
        {"destroyIn", 3.0f}, {"destroyMix", 0.26f},
        {"exciterDrive", 0.60f}, {"exciterTone", 15000.0f}, {"exciterMix", 0.55f}
    }));

    all.push_back(makePreset("Rust - Soft Oxide", "Rust", {
        {"characterMode", 2.0f}, {"characterAmount", 0.38f},
        {"destroyJitter", 0.16f}, {"destroyFader", 22000.0f}, {"destroyBits", 12.0f},
        {"destroyIn", 3.5f}, {"destroyMix", 0.42f},
        {"exciterDrive", 0.18f}, {"exciterTone", 4200.0f}, {"exciterMix", 0.08f}
    }));
    all.push_back(makePreset("Rust - Dirty Keys", "Rust", {
        {"characterMode", 2.0f}, {"characterAmount", 0.56f},
        {"destroyJitter", 0.24f}, {"destroyFader", 17000.0f}, {"destroyBits", 10.0f},
        {"destroyIn", 5.5f}, {"destroyMix", 0.58f},
        {"exciterDrive", 0.24f}, {"exciterTone", 3600.0f}, {"exciterMix", 0.12f}
    }));
    all.push_back(makePreset("Rust - Drum Tarnish", "Rust", {
        {"characterMode", 2.0f}, {"characterAmount", 0.70f},
        {"destroyJitter", 0.30f}, {"destroyFader", 14000.0f}, {"destroyBits", 9.0f},
        {"destroyIn", 7.0f}, {"destroyMix", 0.70f},
        {"exciterDrive", 0.30f}, {"exciterTone", 3300.0f}, {"exciterMix", 0.14f}
    }));
    all.push_back(makePreset("Rust - Corroded Bus", "Rust", {
        {"characterMode", 2.0f}, {"characterAmount", 0.84f},
        {"destroyJitter", 0.38f}, {"destroyFader", 10500.0f}, {"destroyBits", 8.0f},
        {"destroyIn", 8.5f}, {"destroyMix", 0.80f},
        {"exciterDrive", 0.36f}, {"exciterTone", 3000.0f}, {"exciterMix", 0.16f}
    }));
    all.push_back(makePreset("Rust - Acid Shell", "Rust", {
        {"characterMode", 2.0f}, {"characterAmount", 0.96f},
        {"destroyJitter", 0.48f}, {"destroyFader", 7600.0f}, {"destroyBits", 7.0f},
        {"destroyIn", 10.0f}, {"destroyMix", 0.90f},
        {"exciterDrive", 0.44f}, {"exciterTone", 2800.0f}, {"exciterMix", 0.20f}
    }));

    all.push_back(makePreset("Heat - Gentle Warmth", "Heat", {
        {"characterMode", 3.0f}, {"characterAmount", 0.36f},
        {"destroyJitter", 0.02f}, {"destroyFader", 44100.0f}, {"destroyBits", 18.0f},
        {"destroyIn", 3.0f}, {"destroyMix", 0.30f},
        {"exciterDrive", 0.22f}, {"exciterTone", 5600.0f}, {"exciterMix", 0.14f}
    }));
    all.push_back(makePreset("Heat - Console Push", "Heat", {
        {"characterMode", 3.0f}, {"characterAmount", 0.54f},
        {"destroyJitter", 0.03f}, {"destroyFader", 44100.0f}, {"destroyBits", 16.0f},
        {"destroyIn", 5.5f}, {"destroyMix", 0.46f},
        {"exciterDrive", 0.30f}, {"exciterTone", 6200.0f}, {"exciterMix", 0.18f}
    }));
    all.push_back(makePreset("Heat - Saturated Hook", "Heat", {
        {"characterMode", 3.0f}, {"characterAmount", 0.68f},
        {"destroyJitter", 0.04f}, {"destroyFader", 39000.0f}, {"destroyBits", 15.0f},
        {"destroyIn", 7.0f}, {"destroyMix", 0.58f},
        {"exciterDrive", 0.36f}, {"exciterTone", 7000.0f}, {"exciterMix", 0.22f}
    }));
    all.push_back(makePreset("Heat - Tape Flame", "Heat", {
        {"characterMode", 3.0f}, {"characterAmount", 0.82f},
        {"destroyJitter", 0.06f}, {"destroyFader", 33000.0f}, {"destroyBits", 14.0f},
        {"destroyIn", 8.5f}, {"destroyMix", 0.70f},
        {"exciterDrive", 0.44f}, {"exciterTone", 7600.0f}, {"exciterMix", 0.26f}
    }));
    all.push_back(makePreset("Heat - Melted Master", "Heat", {
        {"characterMode", 3.0f}, {"characterAmount", 0.94f},
        {"destroyJitter", 0.08f}, {"destroyFader", 28000.0f}, {"destroyBits", 13.0f},
        {"destroyIn", 10.0f}, {"destroyMix", 0.80f},
        {"exciterDrive", 0.52f}, {"exciterTone", 8200.0f}, {"exciterMix", 0.30f}
    }));

    all.push_back(makePreset("Broken - Light Damage", "Broken", {
        {"characterMode", 4.0f}, {"characterAmount", 0.42f},
        {"destroyJitter", 0.22f}, {"destroyFader", 12000.0f}, {"destroyBits", 9.0f},
        {"destroyIn", 6.5f}, {"destroyMix", 0.58f},
        {"exciterDrive", 0.34f}, {"exciterTone", 4500.0f}, {"exciterMix", 0.20f}
    }));
    all.push_back(makePreset("Broken - Bad Cable", "Broken", {
        {"characterMode", 4.0f}, {"characterAmount", 0.60f},
        {"destroyJitter", 0.34f}, {"destroyFader", 8500.0f}, {"destroyBits", 7.0f},
        {"destroyIn", 8.0f}, {"destroyMix", 0.74f},
        {"exciterDrive", 0.46f}, {"exciterTone", 4100.0f}, {"exciterMix", 0.26f}
    }));
    all.push_back(makePreset("Broken - Cracked MPC", "Broken", {
        {"characterMode", 4.0f}, {"characterAmount", 0.76f},
        {"destroyJitter", 0.46f}, {"destroyFader", 6200.0f}, {"destroyBits", 6.0f},
        {"destroyIn", 10.0f}, {"destroyMix", 0.86f},
        {"exciterDrive", 0.58f}, {"exciterTone", 3800.0f}, {"exciterMix", 0.32f}
    }));
    all.push_back(makePreset("Broken - Data Collapse", "Broken", {
        {"characterMode", 4.0f}, {"characterAmount", 0.90f},
        {"destroyJitter", 0.60f}, {"destroyFader", 4200.0f}, {"destroyBits", 5.0f},
        {"destroyIn", 12.0f}, {"destroyMix", 0.94f},
        {"exciterDrive", 0.70f}, {"exciterTone", 3400.0f}, {"exciterMix", 0.38f}
    }));
    all.push_back(makePreset("Broken - Total Fragment", "Broken", {
        {"characterMode", 4.0f}, {"characterAmount", 1.0f},
        {"destroyJitter", 0.76f}, {"destroyFader", 2800.0f}, {"destroyBits", 4.0f},
        {"destroyIn", 14.0f}, {"destroyMix", 1.0f},
        {"exciterDrive", 0.82f}, {"exciterTone", 3000.0f}, {"exciterMix", 0.45f}
    }));

    all.push_back(makePreset("Glow - Soft Halo", "Glow", {
        {"characterMode", 5.0f}, {"characterAmount", 0.34f},
        {"destroyJitter", 0.01f}, {"destroyFader", 44100.0f}, {"destroyBits", 18.0f},
        {"destroyIn", 1.0f}, {"destroyMix", 0.10f},
        {"exciterDrive", 0.26f}, {"exciterTone", 7600.0f}, {"exciterMix", 0.30f}
    }));
    all.push_back(makePreset("Glow - Wide Air", "Glow", {
        {"characterMode", 5.0f}, {"characterAmount", 0.52f},
        {"destroyJitter", 0.02f}, {"destroyFader", 44100.0f}, {"destroyBits", 18.0f},
        {"destroyIn", 1.5f}, {"destroyMix", 0.16f},
        {"exciterDrive", 0.34f}, {"exciterTone", 8800.0f}, {"exciterMix", 0.42f}
    }));
    all.push_back(makePreset("Glow - Vocal Aura", "Glow", {
        {"characterMode", 5.0f}, {"characterAmount", 0.66f},
        {"destroyJitter", 0.02f}, {"destroyFader", 39000.0f}, {"destroyBits", 17.0f},
        {"destroyIn", 2.0f}, {"destroyMix", 0.20f},
        {"exciterDrive", 0.42f}, {"exciterTone", 9800.0f}, {"exciterMix", 0.52f}
    }));
    all.push_back(makePreset("Glow - Neon Layer", "Glow", {
        {"characterMode", 5.0f}, {"characterAmount", 0.80f},
        {"destroyJitter", 0.03f}, {"destroyFader", 34000.0f}, {"destroyBits", 16.0f},
        {"destroyIn", 2.5f}, {"destroyMix", 0.24f},
        {"exciterDrive", 0.52f}, {"exciterTone", 11200.0f}, {"exciterMix", 0.60f}
    }));
    all.push_back(makePreset("Glow - Starlit Bus", "Glow", {
        {"characterMode", 5.0f}, {"characterAmount", 0.92f},
        {"destroyJitter", 0.04f}, {"destroyFader", 30000.0f}, {"destroyBits", 15.0f},
        {"destroyIn", 3.0f}, {"destroyMix", 0.30f},
        {"exciterDrive", 0.62f}, {"exciterTone", 12800.0f}, {"exciterMix", 0.68f}
    }));

    return all;
}
