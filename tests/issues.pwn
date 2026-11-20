#include <open.mp>
#include <3DAndreas>

/**
 * Checks the cases reported against ColAndreas that a collision library can get wrong: water
 * whose quad lies above dry ground, the open sea beyond the map, ground under an object high in
 * the sky, removals of every model, rays that end on the water, the box of a turned model and
 * ids that do not exist.
 */

#define STUNT_X 2320.40234
#define STUNT_Y 127.52637
#define STUNT_Z 1687.44299

static
    Float:gNone[1],
    gChecks,
    gFailures
;

main() {}

static stock Check(bool:condition, const label[]) {
    ++gChecks;
    if (!condition) {
        ++gFailures;
        printf("[issues] FAIL %s", label);
    }
}

static stock Water() {
    new
        Float:level,
        Float:ground,
        Float:depth
    ;

    Check(!A3D_IsPointInWater(1283.7430, -776.9374, 96.4609), "the ground under the indoor pool of Madd Dogg is dry");

    A3D_GetWaterLevel(222.9651, -1198.2930, level);
    A3D_FindGround(222.9651, -1198.2930, 24.0, ground);
    printf("[issues] Richman level %.3f, tunnel floor %.3f, in water %i", level, ground, A3D_IsPointInWater(222.9651, -1198.2930, 21.7121, depth));
    Check(!A3D_IsPointInWater(222.9651, -1198.2930, 21.7121), "the tunnel under the Richman pool is dry");
    Check(A3D_IsPointInWater(230.0, -1194.0, 72.0), "a point in the Richman pool is in water");

    Check(A3D_GetWaterLevel(25000.0, -25000.0, level) && level == 0.0, "the open sea far beyond the map has a level");
    Check(A3D_IsPointInWater(25000.0, -25000.0, -1.0), "a point under the open sea far beyond the map is in water");
    Check(!A3D_IsPointInWater(25000.0, -25000.0, 1.0), "a point above the open sea is dry");
}

static stock HighGround() {
    new const
        Physic:stunt = A3D_CreateStaticObject(8040, STUNT_X, STUNT_Y, STUNT_Z, 0.0, 0.0, 89.995)
    ;

    new
        Float:x,
        Float:y,
        Float:z
    ;

    Check(stunt != INVALID_PHYSIC_ID, "a static object at 1687 metres");
    Check(A3D_FindGround(STUNT_X, STUNT_Y, STUNT_Z + 50.0, z) && z > STUNT_Z - 5.0, "the ground under a point above it is the object");
    printf("[issues] ground on the stunt object %.3f", z);
    Check(A3D_RayCast(STUNT_X, STUNT_Y, 2500.0, STUNT_X, STUNT_Y, -3500.0, x, y, z) && z > STUNT_Z - 5.0, "a ray from 2500 metres meets the object");
    A3D_DestroyObject(stunt);
}

static stock Rays() {
    new
        Float:x,
        Float:y,
        Float:z,
        A3D_HIT:type = A3D_HIT_OBJECT,
        hitid = 12345,
        material
    ;

    Check(A3D_RayCast(1983.875366, 555.897338, 11.122355, 2028.804321, 567.251770, -7.651627, x, y, z, type, hitid, material, .flags = A3D_COLLIDE_ALL | A3D_COLLIDE_WATER), "the ray that crashed ColAndreas hits");
    printf("[issues] ColAndreas crash ray: type %i id %i at %.2f %.2f %.2f", _:type, hitid, x, y, z);
    Check(!A3D_RayCast(0.0, 0.0, 900.0, 10.0, 10.0, 900.0, x, y, z), "a ray in the empty sky misses");

    new
        A3D_HIT:types[4],
        hitids[4],
        materials[4],
        Float:xs[4],
        Float:ys[4],
        Float:zs[4]
    ;

    Check(A3D_RayCastAll(2000.0, -1700.0, 200.0, 2000.0, -1700.0, -50.0, xs, ys, zs, types, hitids, materials, 0) == 0, "an empty list takes no hit");
    Check(A3D_RayCastAll(2000.0, -1700.0, 200.0, 2000.0, -1700.0, -50.0, xs, ys, zs, types, hitids, materials, -3) == 0, "a negative size takes no hit");
}

static stock Removals() {
    new const
        removed = A3D_RemoveBuilding(-1, 2495.0, -1680.0, 13.0, 60.0, 77)
    ;

    Check(removed > 0, "every model is removed around a point");
    Check(A3D_RestoreBuilding(-1, 2495.0, -1680.0, 13.0, 60.0, 77) == removed, "every model comes back");
}

static stock bool:Near(Float:value, Float:expected) {
    return floatabs(value - expected) < 0.01;
}

static stock Bounds() {
    new
        Float:minX,
        Float:minY,
        Float:minZ,
        Float:maxX,
        Float:maxY,
        Float:maxZ,
        Float:lowX,
        Float:lowY,
        Float:lowZ,
        Float:highX,
        Float:highY,
        Float:highZ
    ;

    A3D_GetModelBoundingBox(1225, minX, minY, minZ, maxX, maxY, maxZ);

    Check(A3D_GetModelBoundingBox(1225, lowX, lowY, lowZ, highX, highY, highZ, 0.0, 0.0, 90.0), "the box of a turned model");
    Check(Near(highX - lowX, maxY - minY) && Near(highY - lowY, maxX - minX) && Near(lowZ, minZ) && Near(highZ, maxZ), "a quarter turn swaps the width and the depth");

    A3D_GetModelBoundingBox(1225, lowX, lowY, lowZ, highX, highY, highZ, 180.0, 0.0, 0.0);
    Check(Near(lowZ, -maxZ) && Near(highZ, -minZ), "upside down the bottom is the old top");

    A3D_GetModelBoundingBox(1225, lowX, lowY, lowZ, highX, highY, highZ, 0.0, 0.0, 45.0);
    Check(Near(highX - lowX, ((maxX - minX) + (maxY - minY)) / floatsqroot(2.0)) && Near(lowZ, minZ), "an eighth of a turn holds the turned corners");

    Check(!A3D_GetModelBoundingBox(1225, lowX, lowY, lowZ, highX, highY, highZ, Float:0x7FC00000), "a rotation that is not a number");
}

static stock Invalid() {
    new
        Float:x,
        Float:y,
        Float:z,
        Float:a,
        Float:b,
        Float:c,
        A3D_HIT:types[4],
        hitids[4],
        name[32],
        players[4]
    ;

    Check(A3D_CreateObject(20000, 0.0, 0.0, 10.0) == INVALID_PHYSIC_ID, "model 20000");
    Check(A3D_CreateObject(-30001, 0.0, 0.0, 10.0) == INVALID_PHYSIC_ID, "a custom model that does not exist");
    Check(A3D_CreateStaticObject(99999, 0.0, 0.0, 10.0) == INVALID_PHYSIC_ID, "a static model that does not exist");
    Check(!A3D_GetModelBoundingBox(25000, x, y, z, a, b, c), "bounds of a model that does not exist");
    Check(!A3D_GetModelBoundingBox(-1, x, y, z, a, b, c), "bounds of model -1");
    Check(!A3D_GetModelBoundingSphere(0x7FFFFFFF, x, y, z, a), "sphere of the largest model id");
    Check(A3D_OverlapModel(99999, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0, types, hitids) == 0, "overlap of a model that does not exist");
    Check(!A3D_IsModelColliding(-99999, 0.0, 0.0, 10.0), "collision of a model that does not exist");

    Check(!A3D_DestroyObject(INVALID_PHYSIC_ID), "destroy the invalid id");
    Check(!A3D_DestroyObject(Physic:0x7FFFFFFF), "destroy the largest id");
    Check(!A3D_GetPosition(Physic:-5, x, y, z), "position of a negative id");
    Check(!A3D_SetVelocity(Physic:123456, 1.0, 0.0, 0.0), "velocity of an id never made");
    Check(A3D_GetObjectID(Physic:-1) == INVALID_OBJECT_ID, "object of the invalid id");

    new const
        Physic:crate = A3D_CreateObject(1220, 5000.0, 5000.0, 100.0)
    ;

    Check(!A3D_SetExtra(crate, -1, 5), "extra slot -1");
    Check(!A3D_SetExtra(crate, 8, 5), "extra slot 8");
    Check(A3D_GetExtra(crate, 0x7FFFFFFF) == 0, "extra slot of the largest number");
    A3D_DestroyObject(crate);
    Check(!A3D_GetPosition(crate, x, y, z), "position of a destroyed object");

    Check(!A3D_GetMaterialName(-1, name), "material -1");
    Check(!A3D_GetMaterialName(100000, name), "material 100000");
    Check(A3D_GetMaterialFlags(-7) == A3D_MATERIAL:0, "flags of material -7");

    Check(!A3D_DestroyRoute(INVALID_ROUTE_ID), "destroy the invalid route");
    Check(!A3D_GetRoutePoint(Route:777, 0, x, y, z), "point of a route never made");
    Check(A3D_GetRoutePoints(Route:-9, gNone, gNone, gNone) == 0, "points of a negative route");

    Check(!A3D_GetPlayerTarget(-1, x, y, z), "target of player -1");
    Check(!A3D_WorldToScreen(1000, 0.0, 0.0, 0.0, x, y), "screen of player 1000");
    Check(!A3D_ShowRouteForPlayer(-1, Route:0), "route for player -1");
    Check(A3D_GetPlayersOnScreen(5000, players) == 0, "players on the screen of player 5000");
    Check(!A3D_GetVehicleOffsetPosition(-1, 0.0, 0.0, 0.0, x, y, z), "offset of vehicle -1");
    Check(!A3D_GetVehiclePartPosition(1, A3D_PART:99, x, y, z), "part 99 of vehicle 1");

    new const
        vehicleid = CreateVehicle(411, 5000.0, 5100.0, 100.0, 0.0, -1, -1, -1)
    ;

    Check(!A3D_GetVehiclePartPosition(vehicleid, A3D_PART:-1, x, y, z), "part -1 of a vehicle");
    Check(!A3D_GetVehiclePartPosition(vehicleid, A3D_PART:16, x, y, z), "part 16 of a vehicle");
    DestroyVehicle(vehicleid);
}

public OnGameModeInit() {
    Water();
    HighGround();
    Rays();
    Removals();
    Bounds();
    Invalid();

    printf("[issues] DONE checks %i failures %i", gChecks, gFailures);
    SendRconCommand("exit");

    return 1;
}
