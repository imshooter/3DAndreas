#include <open.mp>
#include <3DAndreas>

main() {}

public OnGameModeInit() {
    new const
        Physic:first = A3D_CreateStaticBox(2.0, 2.0, 2.0, 5000.0, 5000.0, 100.0, .worldid = 7),
        Physic:second = A3D_CreateStaticBox(2.0, 2.0, 2.0, 5000.0, 5000.0, 100.0, .worldid = 8),
        Physic:global = A3D_CreateSphere(-1, 0.25, 5010.0, 5000.0, 100.0)
    ;
    new
        checks,
        failures,
        Float:x,
        Float:y,
        Float:z,
        A3D_HIT:types[4],
        ids[4],
        hitid
    ;

    ++checks;
    if (!A3D_RayCast(5000.0, 5000.0, 105.0, 5000.0, 5000.0, 95.0, x, y, z, .hitid = hitid, .worldid = 7) || hitid != _:first) {
        ++failures;
    }
    ++checks;
    if (!A3D_RayCast(5000.0, 5000.0, 105.0, 5000.0, 5000.0, 95.0, x, y, z, .hitid = hitid, .worldid = 8) || hitid != _:second) {
        ++failures;
    }
    ++checks;
    if (A3D_OverlapSphere(5000.0, 5000.0, 100.0, 0.5, types, ids) != 2) {
        ++failures;
    }
    ++checks;
    if (!A3D_FindGround(772.0, -4.0, 1005.0, z, .depth = 20.0)) {
        ++failures;
    }
    ++checks;
    if (!A3D_IsLineOfSight(5000.0, 5000.0, 105.0, 5000.0, 5000.0, 95.0, .worldid = 9)) {
        ++failures;
    }
    ++checks;
    if (A3D_GetWorld(global) != -1 || !A3D_SetWorld(first, 9) || A3D_GetWorld(first) != 9) {
        ++failures;
    }
    printf("[interiors] DONE checks %i failures %i", checks, failures);
    SendRconCommand("exit");

    return 1;
}
