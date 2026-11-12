#define FILTERSCRIPT

#include <open.mp>
#include <3DAndreas>

/**
 * The side script of `restart.pwn`. It removes the map around a point of Grove Street and owns a
 * moving object with a global object, and it stays loaded while the gamemode restarts.
 */

#define STREET_X 2495.0
#define STREET_Y -1687.0

static
    Physic:gBall
;

static stock bool:Ground() {
    new
        Float:x,
        Float:y,
        Float:z
    ;

    return A3D_RayCast(STREET_X, STREET_Y, 200.0, STREET_X, STREET_Y, -50.0, x, y, z, .flags = A3D_COLLIDE_MAP);
}

forward RestartSideGround();
public RestartSideGround() {
    return Ground();
}

forward RestartSideObject();
public RestartSideObject() {
    return A3D_GetObjectID(gBall);
}

forward RestartSideMove();
public RestartSideMove() {
    return A3D_SetPosition(gBall, 5000.0, 5000.0, 120.0);
}

public OnFilterScriptInit() {
    if (!Ground() || A3D_RemoveBuilding(-1, STREET_X, STREET_Y, 13.0, 60.0) == 0) {
        print("[restart] FAIL the side script removes the map around its point");
    }

    A3D_CreateStaticBox(10.0, 10.0, 1.0, 5000.0, 5000.0, 100.0);
    gBall = A3D_CreateSphere(1946, 0.2, 5000.0, 5000.0, 100.7);

    if (!IsValidObject(A3D_GetObjectID(gBall))) {
        print("[restart] FAIL the body of the side script is shown by a global object");
    }

    return 1;
}
