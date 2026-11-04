#include <open.mp>
#include <3DAndreas>

static
    gChecks,
    gFailures,
    Physic:gStatic[3],
    Physic:gFloor,
    Physic:gMovedFloor,
    Physic:gSupported,
    Physic:gDrop[2],
    Physic:gMapDrop[2],
    Physic:gDry,
    Physic:gZone[2],
    Physic:gInside[2],
    gEntered[2],
    gWrongCrossings,
    gVehicles[2],
    gPlayers[2],
    gActor,
    Physic:gAimWall
;

main() {}

static stock Check(bool:condition, const label[]) {
    ++gChecks;
    if (!condition) {
        ++gFailures;
        printf("[interiors] FAIL %s", label);
    }
}

static stock RayID(Float:atX, worldid, interiorid, A3D_COLLIDE:flags = A3D_COLLIDE_STATIC) {
    new
        Float:x,
        Float:y,
        Float:z,
        hitid
    ;

    if (!A3D_RayCast(atX, 5000.0, 105.0, atX, 5000.0, 95.0, x, y, z, .hitid = hitid, .worldid = worldid, .flags = flags, .interiorid = interiorid)) {
        return -1;
    }

    return hitid;
}

public OnGameModeInit() {
    gStatic[0] = A3D_CreateStaticBox(2.0, 2.0, 2.0, 5000.0, 5000.0, 100.0, .worldid = 7, .interiorid = 3);
    gStatic[1] = A3D_CreateStaticBox(2.0, 2.0, 2.0, 5000.0, 5000.0, 100.0, .worldid = 7, .interiorid = 4);
    gStatic[2] = A3D_CreateStaticBox(2.0, 2.0, 2.0, 5000.0, 5000.0, 100.0, .worldid = 8, .interiorid = 3);

    Check(gStatic[0] != INVALID_PHYSIC_ID && gStatic[1] != INVALID_PHYSIC_ID && gStatic[2] != INVALID_PHYSIC_ID, "create isolated boxes");
    Check(RayID(5000.0, 7, 3) == _:gStatic[0], "same position world 7 interior 3");
    Check(RayID(5000.0, 7, 4) == _:gStatic[1], "same position world 7 interior 4");
    Check(RayID(5000.0, 8, 3) == _:gStatic[2], "same position world 8 interior 3");
    Check(RayID(5000.0, 8, 4) == -1, "empty world/interior pair");
    Check(RayID(5000.0, 7, 0) == -1, "interior zero is not a wildcard");

    new const
        Physic:allWorlds = A3D_CreateStaticBox(2.0, 2.0, 2.0, 5010.0, 5000.0, 100.0, .interiorid = 3),
        Physic:allInteriors = A3D_CreateStaticBox(2.0, 2.0, 2.0, 5020.0, 5000.0, 100.0, .worldid = 7),
        Physic:global = A3D_CreateStaticBox(2.0, 2.0, 2.0, 5030.0, 5000.0, 100.0)
    ;

    Check(RayID(5010.0, 7, 3) == _:allWorlds && RayID(5010.0, 8, 3) == _:allWorlds, "body wildcard world");
    Check(RayID(5010.0, 7, 4) == -1, "wildcard world still filters interior");
    Check(RayID(5020.0, 7, 3) == _:allInteriors && RayID(5020.0, 7, 4) == _:allInteriors, "body wildcard interior");
    Check(RayID(5020.0, 8, 3) == -1, "wildcard interior still filters world");
    Check(RayID(5030.0, 8, 4) == _:global, "global body");
    Check(RayID(5000.0, -1, 4) == _:gStatic[1], "query wildcard world");
    Check(RayID(5000.0, 8, -1) == _:gStatic[2], "query wildcard interior");

    new
        A3D_HIT:types[8],
        ids[8],
        materials[8],
        Float:xs[8],
        Float:ys[8],
        Float:zs[8],
        Float:x,
        Float:y,
        Float:z,
        Float:nx,
        Float:ny,
        Float:nz,
        hitid
    ;

    Check(A3D_OverlapSphere(5000.0, 5000.0, 100.0, 0.5, types, ids, .worldid = 7, .interiorid = 4) == 1 && ids[0] == _:gStatic[1], "sphere overlap");
    Check(A3D_OverlapBox(0.5, 0.5, 0.5, 5000.0, 5000.0, 100.0, 0.0, 0.0, 0.0, types, ids, .worldid = 8, .interiorid = 3) == 1 && ids[0] == _:gStatic[2], "box overlap");
    Check(A3D_OverlapSphere(5000.0, 5000.0, 100.0, 0.5, types, ids) == 3, "legacy overlap sees all contexts");
    Check(A3D_OverlapModel(1220, 5000.0, 5000.0, 100.0, 0.0, 0.0, 0.0, types, ids, .worldid = 7, .interiorid = 4) == 1 && ids[0] == _:gStatic[1], "model overlap");
    Check(!A3D_IsModelColliding(1220, 5000.0, 5000.0, 100.0, .worldid = 7, .interiorid = 5), "model collision ignores another interior");
    Check(A3D_RayCastAll(5000.0, 5000.0, 105.0, 5000.0, 5000.0, 95.0, xs, ys, zs, types, ids, materials, .worldid = 7, .interiorid = 4) == 1 && ids[0] == _:gStatic[1], "all-hit ray");
    Check(A3D_RayCastNormal(5000.0, 5000.0, 105.0, 5000.0, 5000.0, 95.0, x, y, z, nx, ny, nz, .hitid = hitid, .worldid = 7, .interiorid = 4) && hitid == _:gStatic[1] && nz > 0.99, "normal ray");
    Check(A3D_SweepSphere(0.25, 5000.0, 5000.0, 105.0, 5000.0, 5000.0, 95.0, x, y, z, .hitid = hitid, .worldid = 7, .interiorid = 4) && hitid == _:gStatic[1], "sphere sweep");
    Check(A3D_IsLineOfSight(5000.0, 5000.0, 105.0, 5000.0, 5000.0, 95.0, .worldid = 7, .interiorid = 5), "line of sight through another interior");
    Check(!A3D_FindGround(5000.0, 5000.0, 105.0, z, .worldid = 7, .depth = 10.0, .interiorid = 5), "ground ignores another interior");
    Check(A3D_PredictTrajectory(5000.0, 5000.0, 105.0, 0.0, 0.0, -10.0, x, y, z, .hitid = hitid, .duration = 1.0, .worldid = 7, .interiorid = 4) && hitid == _:gStatic[1], "trajectory");
    Check(!A3D_PredictTrajectory(5000.0, 5000.0, 105.0, 0.0, 0.0, -10.0, x, y, z, .duration = 1.0, .worldid = 7, .interiorid = 5), "trajectory ignores another interior");
    Check(A3D_FindRandomPoint(5000.0, 5000.0, 103.0, 0.0, x, y, z, .worldid = 7, .interiorid = 4), "random point in matching interior");
    Check(!A3D_FindRandomPoint(5000.0, 5000.0, 103.0, 0.0, x, y, z, .worldid = 7, .interiorid = 5), "random point ignores another interior");

#if !defined A3D_INTERIOR_BASELINE
    Check(A3D_GetInterior(gStatic[0]) == 3, "get interior");
    Check(A3D_SetInterior(gStatic[0], 6), "set interior");
    Check(RayID(5000.0, 7, 3) == -1 && RayID(5000.0, 7, 6) == _:gStatic[0], "interior setter refreshes queries immediately");
    A3D_SetInterior(gStatic[0], 3);
    A3D_SetWorld(gStatic[0], 9);
    Check(RayID(5000.0, 7, 3) == -1 && RayID(5000.0, 9, 3) == _:gStatic[0], "world setter preserves interior");
    A3D_SetWorld(gStatic[0], 7);
    Check(!A3D_SetInterior(INVALID_PHYSIC_ID, 3), "invalid setter");
    Check(A3D_GetInterior(INVALID_PHYSIC_ID) == -1, "invalid getter");

    new const
        Physic:large = A3D_CreateStaticBox(2.0, 2.0, 2.0, 5040.0, 5000.0, 100.0, .worldid = cellmax, .interiorid = cellmax)
    ;

    Check(RayID(5040.0, cellmax, cellmax) == _:large, "full-width world and interior");
    Check(RayID(5040.0, 0, cellmax) == -1, "full-width world is not global");
    A3D_DestroyObject(large);
    new const
        Physic:reused = A3D_CreateStaticBox(2.0, 2.0, 2.0, 5040.0, 5000.0, 100.0, .worldid = 0, .interiorid = 0)
    ;

    Check(RayID(5040.0, 0, 0) == _:reused && RayID(5040.0, cellmax, cellmax) == -1, "reused body slot takes the new context");
    Check(A3D_GetInterior(A3D_CreateObject(1220, 5050.0, 5000.0, 100.0, .interiorid = 4)) == 4, "model creation interior");
    Check(A3D_GetInterior(A3D_CreateBox(-1, 1.0, 1.0, 1.0, 5060.0, 5000.0, 100.0, .interiorid = 4)) == 4, "box creation interior");
    Check(A3D_GetInterior(A3D_CreateStaticObject(1220, 5070.0, 5000.0, 100.0, .interiorid = 4)) == 4, "static model creation interior");
    Check(A3D_GetInterior(A3D_CreateStaticCylinder(1.0, 1.0, 5080.0, 5000.0, 100.0, .interiorid = 4)) == 4, "cylinder creation interior");
    new
        Physic:walls[4]
    ;

    Check(A3D_CreateArea(5600.0, 4998.0, 5604.0, 5002.0, 98.0, 102.0, walls, .interiorid = 4), "area creation");
    for (new i; i != sizeof (walls); ++i) {
        Check(A3D_GetInterior(walls[i]) == 4, "area propagates interior");
    }
#endif

    Check(A3D_FindGround(1700.0, -2500.0, 20.0, z, .flags = A3D_COLLIDE_MAP, .depth = 20.0, .interiorid = 0), "exterior map ground");
    Check(!A3D_FindGround(1700.0, -2500.0, 20.0, z, .flags = A3D_COLLIDE_MAP, .depth = 20.0, .interiorid = 6), "exterior map absent from interior");
    Check(A3D_FindGround(772.0, -4.0, 1005.0, z, .flags = A3D_COLLIDE_MAP, .depth = 20.0, .interiorid = 5), "gym map in IPL interior");
    Check(!A3D_FindGround(772.0, -4.0, 1005.0, z, .flags = A3D_COLLIDE_MAP, .depth = 20.0, .interiorid = 0), "gym map absent from exterior");
    Check(A3D_RemoveBuilding(-1, 0.0, 0.0, 0.0, 20000.0, 7, 5) > 0, "remove interior placements in one world");
    Check(!A3D_FindGround(772.0, -4.0, 1005.0, z, .worldid = 7, .flags = A3D_COLLIDE_MAP, .depth = 20.0, .interiorid = 5), "removed interior absent in its world");
    Check(A3D_FindGround(772.0, -4.0, 1005.0, z, .worldid = 8, .flags = A3D_COLLIDE_MAP, .depth = 20.0, .interiorid = 5), "removal leaves another world intact");
    Check(A3D_FindGround(1700.0, -2500.0, 20.0, z, .worldid = 7, .flags = A3D_COLLIDE_MAP, .depth = 20.0, .interiorid = 0), "removal leaves another interior intact");
    Check(A3D_RestoreBuilding(-1, 0.0, 0.0, 0.0, 20000.0, 7, 0) == 0, "restore filters interior");
    Check(A3D_RestoreBuilding(-1, 0.0, 0.0, 0.0, 20000.0, 7, 5) > 0, "restore matching interior");
    Check(A3D_FindGround(772.0, -4.0, 1005.0, z, .worldid = 7, .flags = A3D_COLLIDE_MAP, .depth = 20.0, .interiorid = 5), "restored map answers again");
    Check(!A3D_IsPointInWater(1280.0, -774.0, 300.0, .worldid = 7), "indoor pool does not flood the exterior below it");
    Check(!A3D_IsPointInWater(1280.0, -774.0, 1070.0, .worldid = 7), "pool floor blocks water");
    Check(A3D_RemoveBuilding(-1, 1280.0, -774.0, 1082.0, 200.0, 7) > 0, "remove pool floor in one world");
    Check(A3D_IsPointInWater(1280.0, -774.0, 1070.0, .worldid = 7), "water sees the floor removed in its world");
    Check(!A3D_IsPointInWater(1280.0, -774.0, 1070.0, .worldid = 8), "water keeps the floor in another world");
    Check(A3D_RestoreBuilding(-1, 1280.0, -774.0, 1082.0, 200.0, 7) > 0, "restore pool floor");
    Check(!A3D_IsPointInWater(1280.0, -774.0, 1070.0, .worldid = 7), "restored pool floor blocks water again");

    gFloor = A3D_CreateStaticBox(30.0, 4.0, 0.4, 5105.0, 5000.0, 100.0, .worldid = 7, .interiorid = 3);
    Check(gFloor != INVALID_PHYSIC_ID, "create isolated floor");
    gDrop[0] = A3D_CreateSphere(-1, 0.5, 5100.0, 5000.0, 103.0, .worldid = 7, .interiorid = 3);
    gDrop[1] = A3D_CreateSphere(-1, 0.5, 5110.0, 5000.0, 103.0, .worldid = 7, .interiorid = 4);
    A3D_SetBounce(gDrop[0], 0.0);
    gMovedFloor = A3D_CreateStaticBox(4.0, 4.0, 0.4, 5130.0, 5000.0, 100.0, .worldid = 7, .interiorid = 3);
    gSupported = A3D_CreateSphere(-1, 0.5, 5130.0, 5000.0, 103.0, .worldid = 7, .interiorid = 3);
    A3D_SetBounce(gSupported, 0.0);
    Check(gMovedFloor != INVALID_PHYSIC_ID, "create movable-context floor");
    gMapDrop[0] = A3D_CreateSphere(-1, 0.25, 1700.0, -2500.0, 15.0, .worldid = 7, .interiorid = 0);
    gMapDrop[1] = A3D_CreateSphere(-1, 0.25, 1702.0, -2500.0, 15.0, .worldid = 7, .interiorid = 6);
    A3D_SetBounce(gMapDrop[0], 0.0);
    gDry = A3D_CreateSphere(-1, 0.25, 1280.0, -774.0, 300.0, .worldid = 7, .interiorid = 0);
    A3D_SetBuoyancy(gDry, 1.0);
    gZone[0] = A3D_CreateZone(4.0, 4.0, 4.0, 5200.0, 5000.0, 100.0, .worldid = 7, .interiorid = 3);
    gZone[1] = A3D_CreateZoneSphere(2.0, 5200.0, 5000.0, 100.0, .worldid = 7, .interiorid = 4);
    gInside[0] = A3D_CreateSphere(-1, 0.25, 5200.0, 5000.0, 100.0, .worldid = 7, .interiorid = 3);
    gInside[1] = A3D_CreateSphere(-1, 0.25, 5200.0, 5000.0, 100.0, .worldid = 7, .interiorid = 4);
    A3D_SetGravity(gInside[0], 0.0);
    A3D_SetGravity(gInside[1], 0.0);

    for (new i; i != 2; ++i) {
        gVehicles[i] = CreateVehicle(411, 5300.0, 5000.0, 100.0, 0.0, -1, -1, -1);
        SetVehicleVirtualWorld(gVehicles[i], 7);
        LinkVehicleToInterior(gVehicles[i], 3 + i);
    }

    A3D_CreateStaticBox(0.4, 4.0, 4.0, 5305.0, 5000.0, 100.0, .worldid = 7, .interiorid = 4);
    A3D_CreateStaticBox(0.4, 4.0, 4.0, 5405.0, 5000.0, 100.0, .worldid = 7, .interiorid = 4);
    gAimWall = A3D_CreateStaticBox(0.4, 4.0, 4.0, 5410.0, 5000.0, 100.0, .worldid = 7, .interiorid = 3);
    gActor = CreateActor(0, 5500.0, 5000.0, 100.0, 0.0);
    SetActorVirtualWorld(gActor, 7);
    gPlayers[0] = NPC_Create("InteriorThree");
    gPlayers[1] = NPC_Create("InteriorFour");
    NPC_Spawn(gPlayers[0]);
    NPC_Spawn(gPlayers[1]);
    SetTimer("@Interiors_Check", 2000, false);

    return 1;
}

public OnNPCSpawn(npcid) {
    NPC_SetPos(npcid, 5400.0, 5000.0, 100.0);
    NPC_SetVirtualWorld(npcid, 7);
    NPC_SetInterior(npcid, npcid == gPlayers[0] ? 3 : 4);
    NPC_SetInvulnerable(npcid);
    NPC_SetWeapon(npcid, WEAPON_M4);
    NPC_AimAt(npcid, 5410.0, 5000.0, 100.5);

    return 1;
}

forward @Interiors_Check();
public @Interiors_Check() {
    new
        Float:x,
        Float:y,
        Float:z,
        hitid,
        ids[4]
    ;

    A3D_GetPosition(gDrop[0], x, y, z);
    Check(floatabs(z - 100.7) < 0.05, "sphere rests on matching floor");
    A3D_GetPosition(gDrop[1], x, y, z);
    Check(z < 95.0, "sphere falls through floor in another interior");
    A3D_GetPosition(gMapDrop[0], x, y, z);
    Check(floatabs(z - 12.804) < 0.05, "sphere rests on matching map placement");
    A3D_GetPosition(gMapDrop[1], x, y, z);
    Check(z < 5.0, "sphere ignores map placements of another interior");
    A3D_GetPosition(gDry, x, y, z);
    Check(z < 290.0, "indoor pool does not apply buoyancy below its floor");
    Check(gEntered[0] == 1 && gEntered[1] == 1, "each zone receives its own body");
    Check(gWrongCrossings == 0, "no cross-interior zone callbacks");
    Check(RayID(5300.0, 7, 3, A3D_COLLIDE_VEHICLES) == gVehicles[0], "vehicle interior 3");
    Check(RayID(5300.0, 7, 4, A3D_COLLIDE_VEHICLES) == gVehicles[1], "vehicle interior 4");
    Check(RayID(5400.0, 7, 3, A3D_COLLIDE_PLAYERS) == gPlayers[0], "player interior 3");
    Check(RayID(5400.0, 7, 4, A3D_COLLIDE_PLAYERS) == gPlayers[1], "player interior 4");
    Check(RayID(5500.0, 7, 3, A3D_COLLIDE_ACTORS) == gActor && RayID(5500.0, 7, 4, A3D_COLLIDE_ACTORS) == gActor, "actor keeps wildcard interior");
    Check(RayID(5500.0, 8, 3, A3D_COLLIDE_ACTORS) == -1, "actor still filters world");
    Check(A3D_GetPlayerTarget(gPlayers[0], x, y, z, .hitid = hitid, .flags = A3D_COLLIDE_STATIC) && hitid == _:gAimWall, "aim follows the viewer interior");
    Check(A3D_GetScreenTarget(gPlayers[0], 320.0, 224.0, x, y, z, .hitid = hitid, .flags = A3D_COLLIDE_STATIC) && hitid == _:gAimWall, "screen ray follows the viewer interior");
    Check(A3D_IsPointOnScreen(gPlayers[0], 5408.0, 5000.0, 100.5, true), "screen sight ignores a wall in another interior");
    Check(A3D_GetPlayersInSphere(5408.0, 5000.0, 100.5, 12.0, ids, .worldid = 7, .interiorid = 3, .sight = true) == 1 && ids[0] == gPlayers[0], "sphere sight uses requested interior");
    Check(A3D_GetPlayersInCone(5408.0, 5000.0, 100.5, -1.0, 0.0, 0.0, 10.0, 12.0, ids, .worldid = 7, .interiorid = 3, .sight = true) == 1 && ids[0] == gPlayers[0], "cone sight uses requested interior");
    Check(A3D_GetVehiclesInSphere(5310.0, 5000.0, 100.0, 15.0, ids, .worldid = 7, .interiorid = 3, .sight = true) == 1 && ids[0] == gVehicles[0], "vehicle sight uses requested interior");
    Check(A3D_CreateExplosion(5200.0, 5000.0, 99.0, 5.0, 1.0, 7, 3) == 1, "explosion only pushes matching interior");
    Check(A3D_GetSpeed(gInside[1]) < 0.01, "other interior unaffected by explosion");

    LinkVehicleToInterior(gVehicles[0], 6);
    NPC_SetInterior(gPlayers[0], 6);
#if !defined A3D_INTERIOR_BASELINE
    A3D_SetInterior(gDrop[0], 4);
    Check(!A3D_IsMoving(gSupported), "supported body is asleep before changing its floor");
    A3D_SetInterior(gMovedFloor, 4);
#endif
    SetTimer("@Interiors_Moved", 600, false);

    return 1;
}

forward @Interiors_Moved();
public @Interiors_Moved() {
    Check(RayID(5300.0, 7, 3, A3D_COLLIDE_VEHICLES) == -1, "vehicle leaves old interior without moving");
    Check(RayID(5300.0, 7, 6, A3D_COLLIDE_VEHICLES) == gVehicles[0], "vehicle enters new interior without moving");
    Check(RayID(5400.0, 7, 3, A3D_COLLIDE_PLAYERS) == -1, "player leaves old interior without moving");
    Check(RayID(5400.0, 7, 6, A3D_COLLIDE_PLAYERS) == gPlayers[0], "player enters new interior without moving");
#if !defined A3D_INTERIOR_BASELINE
    new
        Float:x,
        Float:y,
        Float:z
    ;

    A3D_GetPosition(gDrop[0], x, y, z);
    Check(z < 99.5, "changing interior wakes a sleeping body and drops old contacts");
    A3D_GetPosition(gSupported, x, y, z);
    Check(z < 99.5, "changing static interior wakes bodies resting on it");
#endif
    printf("[interiors] DONE checks %i failures %i", gChecks, gFailures);
    SendRconCommand("exit");

    return 1;
}

public A3D_OnEntered(Physic:zoneid, Physic:physicid) {
    for (new i; i != 2; ++i) {
        if (zoneid != gZone[i]) {
            continue;
        }
        if (physicid == gInside[i]) {
            ++gEntered[i];
        } else {
            ++gWrongCrossings;
        }
    }

    return 1;
}
