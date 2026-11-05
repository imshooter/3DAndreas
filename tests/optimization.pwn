#include <open.mp>
#include <3DAndreas>

/**
 * Times long and random water rays, sphere sweeps on water and dense overlaps, and prints a
 * checksum of each so two builds can be compared.
 */

#define OVERLAP_BODIES                  (600)
#define WATER_RAYS                     (3000)
#define WATER_SAMPLES                  (5000)
#define OVERLAP_QUERIES                (600)

main() {}

public OnGameModeInit() {
    new
        A3D_HIT:types[OVERLAP_BODIES],
        ids[OVERLAP_BODIES],
        waterHits,
        waterChecksum,
        overlapChecksum,
        start,
        Float:hitX,
        Float:hitY,
        Float:hitZ,
        A3D_HIT:hitType
    ;

    for (new i; i != OVERLAP_BODIES; ++i) {
        new const
            Float:x = 5000.0 + float(i % 20) * 0.3,
            Float:y = 5000.0 + float(i / 20) * 0.3
        ;

        A3D_CreateStaticBox(0.2, 0.2, 0.2, x, y, 100.0);
    }

    new const
        found = A3D_OverlapSphere(5003.0, 5004.5, 100.0, 12.0, types, ids, .flags = A3D_COLLIDE_STATIC)
    ;

    if (found != OVERLAP_BODIES) {
        printf("[optimization] FAIL overlap found %i of %i", found, OVERLAP_BODIES);
    }

    for (new i; i != found; ++i) {
        overlapChecksum = (overlapChecksum * 31) ^ (ids[i] + (_:types[i]));
    }
    printf("[optimization] overlap order checksum %i", overlapChecksum);
    overlapChecksum = 0;

    start = GetTickCount();
    for (new i; i != WATER_RAYS; ++i) {
        new const
            Float:shift = float(i % 31) * 0.35,
            bool:clear = A3D_IsLineOfSight(
                -2900.0 + shift, -2800.0, 25.0,
                2900.0 - shift, 2800.0, -25.0,
                .flags = A3D_COLLIDE_WATER
            )
        ;

        waterHits += !clear ? 1 : 0;
        waterChecksum = (waterChecksum * 31) ^ (_:clear + i);
    }
    printf("[optimization] diagonal water %i rays %i ms %i hits checksum %i", WATER_RAYS, GetTickCount() - start, waterHits, waterChecksum);

    waterHits = 0;
    waterChecksum = 0;
    start = GetTickCount();
    for (new i; i != WATER_RAYS; ++i) {
        new const
            Float:shift = float(i % 31) * 0.35,
            bool:clear = A3D_IsLineOfSight(
                -2900.0 + shift, -2800.0, 25.0,
                2900.0 - shift, 2800.0, 25.0,
                .flags = A3D_COLLIDE_WATER
            )
        ;

        waterHits += !clear ? 1 : 0;
        waterChecksum = (waterChecksum * 31) ^ (_:clear + i);
    }
    printf("[optimization] horizontal water %i rays %i ms %i hits checksum %i", WATER_RAYS, GetTickCount() - start, waterHits, waterChecksum);

    waterHits = 0;
    waterChecksum = 0;
    start = GetTickCount();
    for (new i; i != WATER_SAMPLES; ++i) {
        new const
            Float:fromX = float((i * 7919) % 6000) - 3000.0,
            Float:fromY = float((i * 4231) % 6000) - 3000.0,
            Float:toX = float((i * 4967 + 1234) % 6000) - 3000.0,
            Float:toY = float((i * 3181 + 567) % 6000) - 3000.0,
            bool:hit = A3D_RayCast(fromX, fromY, 20.0, toX, toY, -20.0, hitX, hitY, hitZ, hitType, .flags = A3D_COLLIDE_WATER)
        ;

        waterHits += hit ? 1 : 0;
        waterChecksum = (waterChecksum * 31) ^ (hit ? floatround(hitX * 100.0) + floatround(hitY * 100.0) + floatround(hitZ * 100.0) + (_:hitType) : i);
    }
    printf("[optimization] sampled water rays %i queries %i ms %i hits checksum %i", WATER_SAMPLES, GetTickCount() - start, waterHits, waterChecksum);

    waterHits = 0;
    waterChecksum = 0;
    start = GetTickCount();
    for (new i; i != WATER_SAMPLES; ++i) {
        new const
            Float:fromX = float((i * 7919) % 6000) - 3000.0,
            Float:fromY = float((i * 4231) % 6000) - 3000.0,
            Float:toX = float((i * 4967 + 1234) % 6000) - 3000.0,
            Float:toY = float((i * 3181 + 567) % 6000) - 3000.0,
            bool:hit = A3D_SweepSphere(0.5, fromX, fromY, 20.0, toX, toY, -20.0, hitX, hitY, hitZ, hitType, .flags = A3D_COLLIDE_WATER)
        ;

        waterHits += hit ? 1 : 0;
        waterChecksum = (waterChecksum * 31) ^ (hit ? floatround(hitX * 100.0) + floatround(hitY * 100.0) + floatround(hitZ * 100.0) + (_:hitType) : i);
    }
    printf("[optimization] sampled water sweeps %i queries %i ms %i hits checksum %i", WATER_SAMPLES, GetTickCount() - start, waterHits, waterChecksum);

    start = GetTickCount();
    for (new i; i != OVERLAP_QUERIES; ++i) {
        overlapChecksum += A3D_OverlapSphere(5003.0, 5004.5, 100.0, 12.0, types, ids, .size = 0, .flags = A3D_COLLIDE_STATIC);
    }
    printf("[optimization] dense overlap %i bodies %i queries %i ms checksum %i", OVERLAP_BODIES, OVERLAP_QUERIES, GetTickCount() - start, overlapChecksum);

    SendRconCommand("exit");
    return 1;
}
