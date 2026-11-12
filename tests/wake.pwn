#include <open.mp>
#include <3DAndreas>

/**
 * Checks that a body asleep on something falls when that thing goes away: a static box moved, turned
 * or destroyed, and a moving box destroyed or sent to another virtual world. A body nothing was taken
 * from stays asleep.
 */

#define BASE_X 5000.0
#define BASE_Y 5000.0
#define FLOOR_Z 100.0
#define GAP 20.0

enum {
    CASE_MOVED,
    CASE_TURNED,
    CASE_DESTROYED,
    CASE_BOX_DESTROYED,
    CASE_BOX_WORLD,
    CASE_UNTOUCHED,
    CASES
};

static
    Physic:gSupport[CASES],
    Physic:gResting[CASES],
    Float:gRestZ[CASES],
    gChecks,
    gFailures,
    gStep
;

main() {}

static stock Check(bool:condition, const label[]) {
    ++gChecks;
    if (!condition) {
        ++gFailures;
        printf("[wake] FAIL %s", label);
    }
}

static stock Float:HeightOf(Physic:physicid) {
    new
        Float:x,
        Float:y,
        Float:z
    ;

    A3D_GetPosition(physicid, x, y, z);

    return z;
}

static stock bool:Fell(index) {
    return HeightOf(gResting[index]) < gRestZ[index] - 0.5;
}

forward Tick();
public Tick() {
    ++gStep;

    if (gStep == 8) {
        for (new i; i != CASES; ++i) {
            gRestZ[i] = HeightOf(gResting[i]);
            Check(!A3D_IsMoving(gResting[i]), "every body is asleep before anything changes");
        }

        A3D_SetPosition(gSupport[CASE_MOVED], BASE_X, BASE_Y + 100.0, FLOOR_Z);
        A3D_SetRotation(gSupport[CASE_TURNED], 0.0, 0.0, 90.0);
        A3D_DestroyObject(gSupport[CASE_DESTROYED]);
        A3D_DestroyObject(gSupport[CASE_BOX_DESTROYED]);
        A3D_SetWorld(gSupport[CASE_BOX_WORLD], 2);
    }

    if (gStep == 13) {
        Check(Fell(CASE_MOVED), "a body falls when the static box under it is moved away");
        Check(Fell(CASE_TURNED), "a body falls when the static box under it is turned away");
        Check(Fell(CASE_DESTROYED), "a body falls when the static box under it is destroyed");
        Check(Fell(CASE_BOX_DESTROYED), "a body falls when the moving box under it is destroyed");
        Check(Fell(CASE_BOX_WORLD), "a body falls when the moving box under it goes to another world");
        Check(floatabs(HeightOf(gResting[CASE_UNTOUCHED]) - gRestZ[CASE_UNTOUCHED]) < 0.01 && !A3D_IsMoving(gResting[CASE_UNTOUCHED]), "a body nothing was taken from stays asleep");
        printf("[wake] DONE checks %i failures %i", gChecks, gFailures);
        SendRconCommand("exit");
    }

    return 1;
}

public OnGameModeInit() {
    for (new i; i != CASES; ++i) {
        new const Float:x = BASE_X + float(i) * GAP;

        switch (i) {
            case CASE_TURNED: {
                gSupport[i] = A3D_CreateStaticBox(4.0, 0.5, 1.0, x, BASE_Y, FLOOR_Z);
                gResting[i] = A3D_CreateSphere(-1, 0.2, x + 1.5, BASE_Y, FLOOR_Z + 0.7);
            }
            case CASE_BOX_DESTROYED, CASE_BOX_WORLD: {
                A3D_CreateStaticBox(4.0, 4.0, 1.0, x, BASE_Y, FLOOR_Z);
                gSupport[i] = A3D_CreateBox(-1, 1.0, 1.0, 1.0, x, BASE_Y, FLOOR_Z + 1.0, .worldid = 1);
                gResting[i] = A3D_CreateBox(-1, 0.5, 0.5, 0.5, x, BASE_Y, FLOOR_Z + 1.75, .worldid = 1);
            }
            default: {
                gSupport[i] = A3D_CreateStaticBox(4.0, 4.0, 1.0, x, BASE_Y, FLOOR_Z);
                gResting[i] = A3D_CreateSphere(-1, 0.2, x, BASE_Y, FLOOR_Z + 0.7);
            }
        }
    }

    SetTimer("Tick", 500, true);

    return 1;
}
