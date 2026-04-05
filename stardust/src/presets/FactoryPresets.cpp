#include "FactoryPresets.h"

std::vector<Preset> getAllFactoryPresets()
{
    // Init preset — all defaults, no bank (appears at index 0)
    std::vector<Preset> all;
    all.push_back({ "Init", defaultPresetValues(), true, {} });

    auto append = [&](std::vector<Preset> bank) {
        all.insert(all.end(),
                   std::make_move_iterator(bank.begin()),
                   std::make_move_iterator(bank.end()));
    };

    append(getLoFiPresets());
    append(getGrainsPresets());
    append(getGlitchPresets());
    append(getTapePresets());
    append(getAtmospherePresets());

    return all;
}
