#include <open.mp>
#include <3DAndreas>

/**
 * Checks what a gamemode restart leaves behind, with `restart-side.pwn` as a side script that stays
 * loaded. The first run removes the tarmac of the airport and restarts the gamemode. The second run
 * finds the tarmac back and the map the side script removed still gone. The moving object of the side
 * script shows as a new global object, and an object the new gamemode creates stays where it was put.
 */

#define GROUND_X 1700.0
#define GROUND_Y -2500.0
#define TARMAC 5002
#define FLAG "restart.flag"

static
    Float:gGround,
    gObject,
    gSideObject,
    gChecks,
    gFailures,
    gStep
;

main() {}

static stock Check(bool:condition, const label[]) {
    ++gChecks;
    if (!condition) {
        ++gFailures;
        printf("[restart] FAIL %s", label);
    }
}

static stock bool:Ground(&Float:z) {
    new
        Float:x,
        Float:y
    ;

    return A3D_RayCast(GROUND_X, GROUND_Y, 100.0, GROUND_X, GROUND_Y, -50.0, x, y, z, .flags = A3D_COLLIDE_MAP);
}

forward Restart();
public Restart() {
    SendRconCommand("gmx");

    return 1;
}

forward Tick();
public Tick() {
    new
        Float:x,
        Float:y,
        Float:z
    ;

    ++gStep;

    if (gStep == 1) {
        gSideObject = CallRemoteFunction("RestartSideObject", "");
        Check(IsValidObject(gSideObject), "the body of the side script is shown by a global object again");
        gObject = CreateObject(1337, 10.0, 20.0, 30.0, 0.0, 0.0, 0.0);
        Check(gObject != gSideObject, "an object of the new gamemode takes another id");
        CallRemoteFunction("RestartSideMove", "");
    }

    if (gStep == 3) {
        GetObjectPos(gObject, x, y, z);
        Check(x == 10.0 && y == 20.0 && z == 30.0, "the object of the new gamemode stays where it was put");
        GetObjectPos(gSideObject, x, y, z);
        Check(floatabs(x - 5000.0) < 1.0 && floatabs(y - 5000.0) < 1.0 && z > 100.0, "the global object of the side script follows its body");
        fremove(FLAG);
        printf("[restart] DONE checks %i failures %i", gChecks, gFailures);
        SendRconCommand("exit");
    }

    return 1;
}

public OnGameModeInit() {
    new
        Float:z,
        text[32]
    ;

    if (!fexist(FLAG)) {
        Check(Ground(gGround), "the tarmac is there on the first run");

        new const File:flag = fopen(FLAG, io_write);

        format(text, sizeof (text), "%f", gGround);
        fwrite(flag, text);
        fclose(flag);
        Check(A3D_RemoveBuilding(TARMAC, GROUND_X, GROUND_Y, 13.0, 500.0) != 0 && !Ground(z), "the first run removes the tarmac");
        Check(!CallRemoteFunction("RestartSideGround", ""), "the side script removed its part of the map");
        SetTimer("Restart", 2000, false);

        return 1;
    }

    new const File:flag = fopen(FLAG, io_read);

    fread(flag, text);
    fclose(flag);
    gGround = floatstr(text);
    Check(Ground(z) && floatabs(z - gGround) < 0.01, "the tarmac the old gamemode removed is back");
    Check(!CallRemoteFunction("RestartSideGround", ""), "the map the side script removed is still gone");
    SetTimer("Tick", 500, true);

    return 1;
}
