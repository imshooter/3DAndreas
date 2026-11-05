#include <open.mp>
#include <3DAndreas>

/**
 * Checks that a sphere touches the water with its surface, not its centre: falling on the sea,
 * rising from under it, starting across it, staying under it, thrown at it and dropped in the
 * indoor pool of Madd Dogg. A ray keeps touching the water with its point.
 */

#define SEA_X 2000.0
#define SEA_Y -2900.0
#define RADIUS 0.5
#define CLOSE 0.01

static
    gChecks,
    gFailures
;

main() {}

static stock Check(bool:condition, const label[]) {
    ++gChecks;
    if (!condition) {
        ++gFailures;
        printf("[water] FAIL %s", label);
    }
}

static stock bool:Near(Float:value, Float:expected) {
    return floatabs(value - expected) < CLOSE;
}

static stock bool:SweepZ(Float:x, Float:y, Float:fromZ, Float:toZ, &Float:z, &A3D_HIT:type) {
    new
        Float:hitX,
        Float:hitY
    ;

    return A3D_SweepSphere(RADIUS, x, y, fromZ, x, y, toZ, hitX, hitY, z, type, .flags = A3D_COLLIDE_WATER);
}

public OnGameModeInit() {
    new
        Float:x,
        Float:y,
        Float:z,
        Float:time,
        Float:level,
        A3D_HIT:type,
        hitid,
        material
    ;

    Check(SweepZ(SEA_X, SEA_Y, 5.0, -5.0, z, type) && type == A3D_HIT_WATER && Near(z, RADIUS), "a falling sphere stops with its bottom on the sea");
    printf("[water] falling sphere centre %.3f", z);
    Check(SweepZ(SEA_X, SEA_Y, -5.0, 5.0, z, type) && type == A3D_HIT_WATER && Near(z, -RADIUS), "a rising sphere stops with its top under the sea");
    Check(SweepZ(SEA_X, SEA_Y, 0.2, -5.0, z, type) && type == A3D_HIT_WATER && Near(z, 0.2), "a sphere across the surface touches at its start");
    Check(!SweepZ(SEA_X, SEA_Y, -3.0, -4.0, z, type), "a sphere under the sea crosses no surface");
    Check(SweepZ(9000.0, 9000.0, 5.0, -5.0, z, type) && type == A3D_HIT_WATER && Near(z, RADIUS), "the open sea beyond the map");

    Check(A3D_RayCast(SEA_X, SEA_Y, 5.0, SEA_X, SEA_Y, -5.0, x, y, z, type, .flags = A3D_COLLIDE_WATER) && type == A3D_HIT_WATER && Near(z, 0.0), "a ray touches the sea with its point");

    Check(A3D_PredictTrajectory(SEA_X, SEA_Y, 10.0, 5.0, 0.0, 0.0, x, y, z, time, type, hitid, material, RADIUS, .flags = A3D_COLLIDE_WATER) && type == A3D_HIT_WATER && Near(z, RADIUS), "a thrown sphere lands with its bottom on the sea");
    Check(A3D_PredictTrajectory(SEA_X, SEA_Y, 10.0, 5.0, 0.0, 0.0, x, y, z, time, type, hitid, material, .flags = A3D_COLLIDE_WATER) && type == A3D_HIT_WATER && Near(z, 0.0), "a thrown point lands on the sea");

    Check(A3D_GetWaterLevel(1280.0, -774.0, level), "the indoor pool has a level");
    Check(SweepZ(1280.0, -774.0, level + 5.0, level - 1.0, z, type) && type == A3D_HIT_WATER && Near(z, level + RADIUS), "a sphere dropped in the indoor pool stops on its surface");

    printf("[water] DONE checks %i failures %i", gChecks, gFailures);
    SendRconCommand("exit");
    return 1;
}
