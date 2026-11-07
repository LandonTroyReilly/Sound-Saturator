#pragma once

#include <algorithm>

// 0 VCA, 1 Flux, 2 Tape, 3 Transformers. Index 0 is VCA then Flux then Tape then Transformers.
inline constexpr int kChainStageCount = 4;
inline constexpr int kChainOrderCount = 24;

enum ChainStage
{
    stageVca = 0,
    stageFlux = 1,
    stageTape = 2,
    stageTransformer = 3
};

inline constexpr const char* chainStageName (int stage) noexcept
{
    switch (stage)
    {
        case stageVca: return "VCA";
        case stageFlux: return "Flux";
        case stageTape: return "Tape";
        default: return "Transformers";
    }
}

inline constexpr void decodeChainOrder (int index, int stages[kChainStageCount]) noexcept
{
    if (index < 0)
        index = 0;
    if (index >= kChainOrderCount)
        index = kChainOrderCount - 1;

    int pool[kChainStageCount] = { stageVca, stageFlux, stageTape, stageTransformer };
    int remaining = kChainStageCount;

    for (int place = 0; place < kChainStageCount; ++place)
    {
        int factorial = 1;
        for (int factor = 2; factor < remaining; ++factor)
            factorial *= factor;

        const int pick = remaining == 1 ? 0 : index / factorial;
        index = remaining == 1 ? 0 : index % factorial;
        stages[place] = pool[pick];

        for (int shift = pick; shift < remaining - 1; ++shift)
            pool[shift] = pool[shift + 1];
        --remaining;
    }
}

inline constexpr int encodeChainOrder (const int stages[kChainStageCount]) noexcept
{
    int pool[kChainStageCount] = { stageVca, stageFlux, stageTape, stageTransformer };
    int remaining = kChainStageCount;
    int index = 0;

    for (int place = 0; place < kChainStageCount; ++place)
    {
        int factorial = 1;
        for (int factor = 2; factor < remaining; ++factor)
            factorial *= factor;

        int pick = 0;
        while (pick < remaining && pool[pick] != stages[place])
            ++pick;
        if (pick == remaining)
            return 0;

        index += pick * factorial;
        for (int shift = pick; shift < remaining - 1; ++shift)
            pool[shift] = pool[shift + 1];
        --remaining;
    }

    return index;
}

namespace
{
constexpr bool chainOrderTableIsValid() noexcept
{
    bool seen[kChainOrderCount] = {};
    for (int index = 0; index < kChainOrderCount; ++index)
    {
        int stages[kChainStageCount];
        decodeChainOrder (index, stages);
        bool used[kChainStageCount] = {};
        for (int place = 0; place < kChainStageCount; ++place)
        {
            if (stages[place] < 0 || stages[place] >= kChainStageCount || used[stages[place]])
                return false;
            used[stages[place]] = true;
        }
        if (encodeChainOrder (stages) != index || seen[index])
            return false;
        seen[index] = true;
    }

    int identity[kChainStageCount];
    decodeChainOrder (0, identity);
    return identity[0] == stageVca && identity[1] == stageFlux && identity[2] == stageTape && identity[3] == stageTransformer;
}
}

static_assert (chainOrderTableIsValid());
