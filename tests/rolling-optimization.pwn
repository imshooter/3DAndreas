#include <open.mp>
#include <3DAndreas>

/**
 * Times the simulation with a thousand active bodies and no rolling friction, then checks that
 * rolling friction slows a sphere, stops when turned off and does not stay on a reused body.
 */

#define ACTIVE_BODIES (1000)

static
    gWarmActive,
    gWarmUpdates,
    Float:gWarmMilliseconds,
    Physic:gRoller
;

main() {}

forward @Rolling_Warm();
public @Rolling_Warm() {
    A3D_GetStats(gWarmActive, gWarmUpdates, gWarmMilliseconds);
    return 1;
}

forward @Rolling_End();
public @Rolling_End() {
    new
        active,
        updates,
        Float:milliseconds
    ;

    A3D_GetStats(active, updates, milliseconds);

    printf("[rolling] %i active, %i updates, %.1f ms physics time in 3 s", active, updates - gWarmUpdates, milliseconds - gWarmMilliseconds);

    gRoller = A3D_CreateSphere(-1, 0.2, 6000.0, 6000.0, 100.0);
    A3D_SetRollingFriction(gRoller, 5.0);
    A3D_SetVelocity(gRoller, 10.0, 0.0, 0.0);
    SetTimer("@Rolling_Enabled", 1000, false);
    return 1;
}

forward @Rolling_Enabled();
public @Rolling_Enabled() {
    new
        Float:vx,
        Float:vy,
        Float:vz
    ;

    A3D_GetVelocity(gRoller, vx, vy, vz);
    printf("[rolling] enabled velocity %.3f", vx);
    if (vx < 2.0 || vx > 7.0) {
        printf("[rolling] FAIL enabled friction");
    }

    A3D_SetRollingFriction(gRoller, 0.0);
    A3D_SetVelocity(gRoller, 10.0, 0.0, 0.0);
    SetTimer("@Rolling_Disabled", 1000, false);
    return 1;
}

forward @Rolling_Disabled();
public @Rolling_Disabled() {
    new
        Float:vx,
        Float:vy,
        Float:vz
    ;

    A3D_GetVelocity(gRoller, vx, vy, vz);
    printf("[rolling] disabled velocity %.3f", vx);
    if (vx < 8.0) {
        printf("[rolling] FAIL disabled friction");
    }

    A3D_DestroyObject(gRoller);
    gRoller = A3D_CreateSphere(-1, 0.2, 6000.0, 6000.0, 100.0);
    A3D_SetVelocity(gRoller, 10.0, 0.0, 0.0);
    SetTimer("@Rolling_Reused", 1000, false);
    return 1;
}

forward @Rolling_Reused();
public @Rolling_Reused() {
    new
        Float:vx,
        Float:vy,
        Float:vz
    ;

    A3D_GetVelocity(gRoller, vx, vy, vz);
    printf("[rolling] reused velocity %.3f", vx);
    if (vx < 8.0) {
        printf("[rolling] FAIL reused body kept friction");
    }

    SendRconCommand("exit");
    return 1;
}

public OnGameModeInit() {
    new
        created
    ;

    for (new i; i != ACTIVE_BODIES; ++i) {
        new const
            Float:x = 5000.0 + float(i % 40) * 2.0,
            Float:y = 5000.0 + float(i / 40) * 2.0
        ;

        if (A3D_CreateSphere(-1, 0.2, x, y, 100.0) != INVALID_PHYSIC_ID) {
            ++created;
        }
    }

    printf("[rolling] created %i of %i bodies", created, ACTIVE_BODIES);
    SetTimer("@Rolling_Warm", 1000, false);
    SetTimer("@Rolling_End", 4000, false);
    return 1;
}
