#include <open.mp>
#include <3DAndreas>

/**
 * Checks the collision of custom models: a model added with AddSimpleModel collides with the
 * collision its .dff file holds, a model whose file has none is refused, and a model asked for
 * before it is added is found once it is. The bottle gives its collision a size 4 bytes longer than
 * the file holds, as most model files do. Needs the model files of the AddSimpleModel calls below in
 * the models folder.
 */

#define BASE_X 5000.0
#define BASE_Y 5000.0
#define BASE_Z 100.0

#define WORKSHOP (-2000)
#define CRANE (-2001)
#define OFFICE (-2002)
#define BOTTLE (-2003)

static
    gChecks,
    gFailures
;

main() {}

static stock Check(bool:condition, const label[]) {
    ++gChecks;
    if (!condition) {
        ++gFailures;
        printf("[custom] FAIL %s", label);
    }
}

public OnGameModeInit() {
    new
        Float:minX,
        Float:minY,
        Float:minZ,
        Float:maxX,
        Float:maxY,
        Float:maxZ,
        Float:x,
        Float:y,
        Float:z,
        A3D_HIT:type,
        hitid
    ;

    Check(A3D_CreateStaticObject(OFFICE, BASE_X, BASE_Y, BASE_Z) == INVALID_PHYSIC_ID, "a custom model not added yet has no collision");

    Check(AddSimpleModel(-1, 11391, WORKSHOP, "doherty/hubprops6_sfse.dff", "doherty/hubprops2_sfse.txd"), "the workshop is added");
    Check(AddSimpleModel(-1, 1382, CRANE, "crane/magnocrane_03.dff", "crane/cranes_dyn2_cj.txd"), "the crane arm is added");
    Check(AddSimpleModel(-1, 1486, BOTTLE, "furniture/coa_beerbottle1.dff", "furniture/coa_beerbottles.txd"), "the bottle is added");
    Check(AddSimpleModel(-1, 11393, OFFICE, "doherty/hubprops1_sfs.dff", "doherty/hubprops2_sfse.txd"), "the office is added");

    Check(A3D_GetModelBoundingBox(WORKSHOP, minX, minY, minZ, maxX, maxY, maxZ), "the workshop has a box");
    printf("[custom] workshop box %.2f %.2f %.2f / %.2f %.2f %.2f", minX, minY, minZ, maxX, maxY, maxZ);

    new const
        Physic:workshop = A3D_CreateStaticObject(WORKSHOP, BASE_X, BASE_Y, BASE_Z)
    ;

    new
        hits
    ;

    for (new i; i < 20; ++i) {
        for (new j; j < 30; ++j) {
            x = BASE_X + minX + float(i);
            y = BASE_Y + minY + float(j);

            if (A3D_RayCast(x, y, BASE_Z + 50.0, x, y, BASE_Z - 50.0, x, y, z, type, hitid, .flags = A3D_COLLIDE_STATIC) && type == A3D_HIT_STATIC && Physic:hitid == workshop) {
                ++hits;
            }
        }
    }

    Check(workshop != INVALID_PHYSIC_ID, "the workshop collides");
    Check(hits > 0, "rays down over the workshop meet its props");
    printf("[custom] %i of 600 rays meet the workshop", hits);

    Check(A3D_CreateStaticObject(CRANE, BASE_X + 100.0, BASE_Y, BASE_Z) == INVALID_PHYSIC_ID, "a model file without collision gives none");
    Check(!A3D_GetModelBoundingBox(CRANE, minX, minY, minZ, maxX, maxY, maxZ), "a model file without collision gives no box");
    Check(A3D_CreateStaticObject(CRANE, BASE_X + 100.0, BASE_Y, BASE_Z) == INVALID_PHYSIC_ID, "asking twice gives the same answer");

    Check(A3D_GetModelBoundingBox(BOTTLE, minX, minY, minZ, maxX, maxY, maxZ) && maxZ > minZ, "a model file with a long collision size still gives its box");
    printf("[custom] bottle box %.3f %.3f %.3f / %.3f %.3f %.3f", minX, minY, minZ, maxX, maxY, maxZ);
    Check(A3D_CreateStaticObject(BOTTLE, BASE_X + 150.0, BASE_Y, BASE_Z) != INVALID_PHYSIC_ID, "a model file with a long collision size collides");

    Check(A3D_CreateStaticObject(OFFICE, BASE_X + 200.0, BASE_Y, BASE_Z) != INVALID_PHYSIC_ID, "a model asked for before it was added collides once it is");
    Check(A3D_CreateObject(OFFICE, BASE_X + 300.0, BASE_Y, BASE_Z + 10.0) != INVALID_PHYSIC_ID, "a custom model can be a moving physics object");

    printf("[custom] DONE checks %i failures %i", gChecks, gFailures);
    SendRconCommand("exit");

    return 1;
}
