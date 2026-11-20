#include <open.mp>
#include <3DAndreas>

/**
 * Checks A3D_MoveObject: a platform rises with a crate on it, arrives exactly and calls
 * A3D_OnMoved, turns on the spot and on the way, stops where A3D_StopObject or A3D_SetPosition
 * leaves it, carries a zone over a resting crate, and refuses what it cannot move.
 */

#define BASE_X 5000.0
#define BASE_Y 5000.0
#define BASE_Z 100.0
#define CLOSE 0.01

static
    Physic:gPlatform,
    Physic:gCrate,
    Physic:gZone,
    Physic:gArrived = INVALID_PHYSIC_ID,
    Physic:gEntered = INVALID_PHYSIC_ID,
    Float:gRest,
    Float:gStoppedX,
    gStep,
    gChecks,
    gFailures
;

main() {}

static stock Check(bool:condition, const label[]) {
    ++gChecks;
    if (!condition) {
        ++gFailures;
        printf("[moving] FAIL %s", label);
    }
}

static stock Float:Z(Physic:physicid) {
    new
        Float:x,
        Float:y,
        Float:z
    ;

    A3D_GetPosition(physicid, x, y, z);
    return z;
}

static stock Float:X(Physic:physicid) {
    new
        Float:x,
        Float:y,
        Float:z
    ;

    A3D_GetPosition(physicid, x, y, z);
    return x;
}

static stock Float:Heading(Physic:physicid) {
    new
        Float:rx,
        Float:ry,
        Float:rz
    ;

    A3D_GetRotation(physicid, rx, ry, rz);
    return rz < 0.0 ? rz + 360.0 : rz;
}

public A3D_OnMoved(Physic:physicid) {
    gArrived = physicid;
    return 1;
}

public A3D_OnEntered(Physic:zoneid, Physic:physicid) {
    if (zoneid == gZone) {
        gEntered = physicid;
    }
    return 1;
}

forward @Moving_Step();
public @Moving_Step() {
    new
        pause = 500,
        Float:x,
        Float:y,
        Float:z
    ;

    switch (gStep++) {
        case 0: {
            gRest = Z(gCrate) - Z(gPlatform);
            printf("[moving] crate rests %.3f over the platform", gRest);
            Check(gRest > 0.4 && gRest < 0.8, "the crate rests on the platform");
            Check(A3D_MoveObject(gPlatform, BASE_X, BASE_Y, BASE_Z + 6.0, 2.0) == 3000, "a rise of 6 metres at 2 metres a second takes 3 seconds");
            Check(A3D_IsMoving(gPlatform), "the platform moves");
            pause = 1500;
        }
        case 1: {
            printf("[moving] halfway: platform %.3f, crate over it %.3f", Z(gPlatform), Z(gCrate) - Z(gPlatform));
            Check(floatabs(Z(gPlatform) - (BASE_Z + 3.0)) < 0.4, "halfway the platform is halfway up");
            Check(floatabs(Z(gCrate) - Z(gPlatform) - gRest) < 0.1, "the crate rides on the platform");
            Check(A3D_RayCast(BASE_X + 1.5, BASE_Y + 1.5, BASE_Z + 20.0, BASE_X + 1.5, BASE_Y + 1.5, BASE_Z - 5.0, x, y, z, .flags = A3D_COLLIDE_STATIC) && floatabs(z - Z(gPlatform) - 0.25) < 0.05, "a ray meets the moving platform where it is");
            pause = 2500;
        }
        case 2: {
            Check(gArrived == gPlatform, "A3D_OnMoved is called on arrival");
            Check(floatabs(Z(gPlatform) - (BASE_Z + 6.0)) < 0.001, "the platform stops exactly at the end");
            Check(!A3D_IsMoving(gPlatform), "the platform no longer moves");
            Check(floatabs(Z(gCrate) - Z(gPlatform) - gRest) < 0.1, "the crate is still on the platform");
            gArrived = INVALID_PHYSIC_ID;
            Check(A3D_MoveObject(gPlatform, BASE_X, BASE_Y, BASE_Z + 6.0, 1.0, 0.0, 0.0, 90.0) == 0, "a turn on the spot takes no time");
            Check(floatabs(Heading(gPlatform) - 90.0) < 0.1, "a turn on the spot is done at once");
            pause = 200;
        }
        case 3: {
            Check(gArrived == gPlatform, "a turn on the spot also arrives");
            gArrived = INVALID_PHYSIC_ID;
            Check(A3D_MoveObject(gPlatform, BASE_X + 4.0, BASE_Y, BASE_Z + 6.0, 4.0, -1000.0, -1000.0, 180.0) == 1000, "a slide of 4 metres at 4 metres a second takes a second");
            pause = 500;
        }
        case 4: {
            printf("[moving] halfway through the slide: x %.3f, heading %.3f", X(gPlatform) - BASE_X, Heading(gPlatform));
            Check(floatabs(Heading(gPlatform) - 135.0) < 15.0, "the platform turns on the way");
            Check(A3D_StopObject(gPlatform), "the slide stops");
            Check(!A3D_IsMoving(gPlatform), "a stopped platform does not move");
            Check(!A3D_StopObject(gPlatform), "a stopped platform cannot stop again");
            gStoppedX = X(gPlatform);
            Check(gStoppedX > BASE_X + 1.0 && gStoppedX < BASE_X + 3.0, "it stops where it was");
            pause = 500;
        }
        case 5: {
            Check(floatabs(X(gPlatform) - gStoppedX) < 0.001, "a stopped platform stays");
            Check(gArrived == INVALID_PHYSIC_ID, "a stopped move does not arrive");
            A3D_MoveObject(gPlatform, BASE_X + 10.0, BASE_Y, BASE_Z + 6.0, 2.0);
            A3D_SetPosition(gPlatform, BASE_X, BASE_Y, BASE_Z + 6.0);
            Check(!A3D_IsMoving(gPlatform), "A3D_SetPosition ends a move");
            pause = 500;
        }
        case 6: {
            Check(floatabs(X(gPlatform) - BASE_X) < 0.001, "the platform stays where A3D_SetPosition put it");
            Check(gArrived == INVALID_PHYSIC_ID, "a move ended by A3D_SetPosition does not arrive");

            Check(A3D_MoveObject(gCrate, BASE_X, BASE_Y, BASE_Z, 1.0) == 0, "a physics object that moves on its own is refused");
            Check(A3D_MoveObject(gPlatform, BASE_X, BASE_Y, BASE_Z, 0.0) == 0, "a speed of zero is refused");
            Check(A3D_MoveObject(gPlatform, Float:0x7FC00000, BASE_Y, BASE_Z, 1.0) == 0, "a place that is not a number is refused");
            Check(A3D_MoveObject(INVALID_PHYSIC_ID, BASE_X, BASE_Y, BASE_Z, 1.0) == 0, "the invalid id is refused");
            Check(!A3D_StopObject(INVALID_PHYSIC_ID), "the invalid id cannot stop");

            gZone = A3D_CreateZone(2.0, 2.0, 2.0, BASE_X + 20.0, BASE_Y, BASE_Z + 6.5);
            Check(A3D_MoveObject(gZone, BASE_X, BASE_Y, BASE_Z + 6.5, 10.0) == 2000, "a zone moves");
            pause = 2500;
        }
        case 7: {
            printf("[moving] zone entered by %i, crate %i, crate %.3f over the platform", _:gEntered, _:gCrate, Z(gCrate) - Z(gPlatform));
            Check(gEntered == gCrate, "a moving zone finds the crate that rests in its way");

            new const
                Physic:doomed = A3D_CreateStaticBox(1.0, 1.0, 1.0, BASE_X + 50.0, BASE_Y, BASE_Z)
            ;

            gArrived = INVALID_PHYSIC_ID;
            A3D_MoveObject(doomed, BASE_X + 60.0, BASE_Y, BASE_Z, 5.0);
            A3D_DestroyObject(doomed);
            pause = 2500;
        }
        case 8: {
            Check(gArrived == INVALID_PHYSIC_ID, "a destroyed body never arrives");
            printf("[moving] DONE checks %i failures %i", gChecks, gFailures);
            SendRconCommand("exit");

            return 1;
        }
    }

    SetTimer("@Moving_Step", pause, false);

    return 1;
}

public OnGameModeInit() {
    gPlatform = A3D_CreateStaticBox(4.0, 4.0, 0.5, BASE_X, BASE_Y, BASE_Z);
    gCrate = A3D_CreateObject(1220, BASE_X, BASE_Y, BASE_Z + 1.0);
    SetTimer("@Moving_Step", 3000, false);

    return 1;
}
