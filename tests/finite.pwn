#include <open.mp>
#include <3DAndreas>

/**
 * Checks that no native lets a value that is infinite or not a number into the simulation. The
 * natives that create refuse it, the natives that set return false and change nothing, and the
 * bodies around stay where rays and overlaps find them.
 */

#define BASE_X 5000.0
#define BASE_Y 5000.0
#define FLOOR_Z 100.0
#define NEIGHBOURS 40
#define CRATE 1220

static
    Physic:gNeighbours[NEIGHBOURS],
    Physic:gBody,
    gChecks,
    gFailures,
    gStep
;

main() {}

static stock Check(bool:condition, const label[]) {
    ++gChecks;
    if (!condition) {
        ++gFailures;
        printf("[finite] FAIL %s", label);
    }
}

static stock bool:Finite(Float:value) {
    return (_:value & 0x7F800000) != 0x7F800000;
}

static stock Refusals() {
    new const
        Float:none = Float:0x7FC00000,
        Float:endless = Float:0x7F800000,
        Float:x = BASE_X + 30.0,
        Float:y = BASE_Y,
        Float:z = FLOOR_Z + 5.0
    ;

    Check(A3D_CreateSphere(-1, none, x, y, z) == INVALID_PHYSIC_ID, "a sphere with a radius that is not a number");
    Check(A3D_CreateSphere(-1, endless, x, y, z) == INVALID_PHYSIC_ID, "a sphere with an infinite radius");
    Check(A3D_CreateSphere(-1, 0.2, none, y, z) == INVALID_PHYSIC_ID, "a sphere at a place that is not a number");
    Check(A3D_CreateSphere(-1, 0.2, x, y, z, endless) == INVALID_PHYSIC_ID, "a sphere with an infinite mass");
    Check(A3D_CreateBox(-1, none, 1.0, 1.0, x, y, z) == INVALID_PHYSIC_ID, "a box with a size that is not a number");
    Check(A3D_CreateBox(-1, 1.0, 1.0, 1.0, x, y, z, none) == INVALID_PHYSIC_ID, "a box with a rotation that is not a number");
    Check(A3D_CreateObject(CRATE, x, y, endless) == INVALID_PHYSIC_ID, "a model at an infinite height");
    Check(A3D_CreateStaticObject(CRATE, none, y, z) == INVALID_PHYSIC_ID, "a static model at a place that is not a number");
    Check(A3D_CreateStaticBox(1.0, endless, 1.0, x, y, z) == INVALID_PHYSIC_ID, "a static box with an infinite size");
    Check(A3D_CreateStaticCylinder(none, 1.0, x, y, z) == INVALID_PHYSIC_ID, "a static cylinder with a radius that is not a number");
    Check(A3D_CreateStaticCylinder(1.0, 1.0, x, y, z, .rz = none) == INVALID_PHYSIC_ID, "a static cylinder with a rotation that is not a number");
    Check(A3D_CreateZone(1.0, 1.0, none, x, y, z) == INVALID_PHYSIC_ID, "a zone with a size that is not a number");
    Check(A3D_CreateZoneSphere(none, x, y, z) == INVALID_PHYSIC_ID, "a round zone with a radius that is not a number");

    new const Physic:crate = A3D_CreateObject(CRATE, x, y, z);

    Check(crate != INVALID_PHYSIC_ID, "the same model with finite values is created");
    A3D_DestroyObject(crate);

    Check(!A3D_SetVelocity(gBody, none, 0.0, 0.0), "a velocity that is not a number");
    Check(!A3D_ApplyImpulse(gBody, 0.0, endless, 0.0), "an infinite impulse");
    Check(!A3D_SetAcceleration(gBody, 0.0, 0.0, none), "an acceleration that is not a number");
    Check(!A3D_SetGravity(gBody, none), "a gravity that is not a number");
    Check(!A3D_SetRollingFriction(gBody, none), "a rolling friction that is not a number");
    Check(!A3D_SetCollisionSpeed(gBody, none), "a collision speed that is not a number");
    Check(!A3D_SetPosition(gBody, none, y, z), "a place that is not a number");
    Check(!A3D_SetRotation(gBody, 0.0, endless, 0.0), "an infinite rotation");
    Check(!A3D_SetFriction(gBody, none), "a friction that is not a number");
    Check(!A3D_SetBounce(gBody, none), "a bounce that is not a number");
    Check(!A3D_SetDamping(gBody, none, 0.1), "a linear damping that is not a number");
    Check(!A3D_SetDamping(gBody, 0.1, endless), "an infinite angular damping");
    Check(!A3D_SetBuoyancy(gBody, none), "a buoyancy that is not a number");
    Check(!A3D_SetBuoyancy(gBody, 1.0, none), "a drag that is not a number");
    Check(!A3D_SetSyncError(gBody, none), "a sync error that is not a number");
    Check(A3D_CreateExplosion(BASE_X + 3.5, BASE_Y + 2.0, FLOOR_Z + 0.7, 20.0, none) == 0, "an explosion with a force that is not a number pushes nothing");
    Check(A3D_CreateExplosion(BASE_X + 3.5, BASE_Y + 2.0, FLOOR_Z + 0.7, none, 5.0) == 0, "an explosion with a radius that is not a number pushes nothing");
}

forward Tick();
public Tick() {
    ++gStep;

    if (gStep == 2) {
        Refusals();
    }

    if (gStep == 3) {
        for (new i; i != NEIGHBOURS; ++i) {
            A3D_SetVelocity(gNeighbours[i], 0.0, 0.0, 3.0);
        }
    }

    if (gStep == 8) {
        new
            Float:x,
            Float:y,
            Float:z,
            A3D_HIT:type,
            A3D_HIT:types[64],
            hitids[64],
            found
        ;

        A3D_GetPosition(gBody, x, y, z);
        Check(Finite(x) && Finite(y) && Finite(z) && floatabs(z - (FLOOR_Z + 0.7)) < 0.05, "the body given those values rests where it was");

        for (new i; i != NEIGHBOURS; ++i) {
            new const
                Float:columnX = BASE_X + float(i % 8),
                Float:columnY = BASE_Y + float(i / 8)
            ;

            if (A3D_RayCast(columnX, columnY, FLOOR_Z + 5.0, columnX, columnY, FLOOR_Z + 0.6, x, y, z, type, .flags = A3D_COLLIDE_OBJECTS) && type == A3D_HIT_OBJECT) {
                ++found;
            }
        }

        Check(found == NEIGHBOURS, "rays find every body around it after a hop");
        Check(A3D_OverlapSphere(BASE_X + 3.5, BASE_Y + 2.0, FLOOR_Z + 0.7, 20.0, types, hitids, .flags = A3D_COLLIDE_OBJECTS) == NEIGHBOURS + 1, "an overlap finds it and every body around it");
        printf("[finite] DONE checks %i failures %i", gChecks, gFailures);
        SendRconCommand("exit");
    }

    return 1;
}

public OnGameModeInit() {
    A3D_CreateStaticBox(40.0, 40.0, 1.0, BASE_X, BASE_Y, FLOOR_Z);

    for (new i; i != NEIGHBOURS; ++i) {
        gNeighbours[i] = A3D_CreateSphere(-1, 0.2, BASE_X + float(i % 8), BASE_Y + float(i / 8), FLOOR_Z + 0.7);
    }

    gBody = A3D_CreateSphere(-1, 0.2, BASE_X + 3.5, BASE_Y + 6.0, FLOOR_Z + 0.7);
    SetTimer("Tick", 500, true);

    return 1;
}
