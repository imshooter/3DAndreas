#include <open.mp>
#include <3DAndreas>

/**
 * Tables of discs: a knock between two discs, a wall and its end, a pocket, a
 * disc slowed to a stop by the rule of its table, a run cut at its steps, a
 * break of sixteen discs and its cost, values that are refused, and a gamemode
 * restart that destroys the tables of the gamemode. The first run restarts the
 * gamemode and the second one checks what is left.
 */

#define FLAG "discs.flag"
#define RADIUS (0.05)

enum E_STEP {
    E_STEP_DISC,
    A3D_DISC_STEP:E_STEP_KIND,
    E_STEP_TIME,
    Float:E_STEP_X,
    Float:E_STEP_Y,
    E_STEP_NEXT,
    E_STEP_OTHER,
    Float:E_STEP_SPEED
}

static
    gChecks,
    gFailures
;

main() {}

static stock Expect(bool:ok, const what[]) {
    ++gChecks;

    if (!ok) {
        ++gFailures;
        printf("[discs] FAIL %s", what);
    }
}

static stock bool:Near(Float:value, Float:target, Float:within = 0.002) {
    return floatabs(value - target) <= within;
}

static stock bool:ReadStep(DiscTable:tableid, index, step[E_STEP]) {
    return A3D_GetDiscStep(tableid, index, step[E_STEP_DISC], step[E_STEP_KIND], step[E_STEP_TIME], step[E_STEP_X], step[E_STEP_Y], step[E_STEP_NEXT], step[E_STEP_OTHER], step[E_STEP_SPEED]);
}

static stock FindStep(DiscTable:tableid, disc, A3D_DISC_STEP:kind, step[E_STEP]) {
    new const
        count = A3D_GetDiscStepCount(tableid)
    ;

    for (new i; i != count; ++i) {
        if (ReadStep(tableid, i, step) && step[E_STEP_DISC] == disc && step[E_STEP_KIND] == kind) {
            return i;
        }
    }
    return -1;
}

static stock bool:StepsHold(DiscTable:tableid) {
    new
        count = A3D_GetDiscStepCount(tableid),
        step[E_STEP],
        after[E_STEP],
        time
    ;

    for (new i; i != count; ++i) {
        if (!ReadStep(tableid, i, step) || step[E_STEP_TIME] < time) {
            return false;
        }
        time = step[E_STEP_TIME];
        if (step[E_STEP_NEXT] != -1 && (step[E_STEP_NEXT] <= i || !ReadStep(tableid, step[E_STEP_NEXT], after) || after[E_STEP_DISC] != step[E_STEP_DISC])) {
            return false;
        }
    }
    return true;
}

static stock Knock() {
    new const
        DiscTable:table = A3D_CreateDiscTable(0.0, 0.0),
        cue = A3D_AddDisc(table, 0.0, 0.0, RADIUS, 1.0),
        ball = A3D_AddDisc(table, 1.03, 0.0, RADIUS, 1.0)
    ;
    new
        step[E_STEP],
        after[E_STEP],
        Float:x,
        Float:y
    ;

    Expect(A3D_RunDiscTable(table, cue, 1.0, 0.0, 2.0) > 0, "a run gives steps");
    Expect(StepsHold(table), "steps come in the order of their time and link to later steps of their disc");
    Expect(ReadStep(table, 0, step) && step[E_STEP_DISC] == cue && step[E_STEP_KIND] == A3D_DISC_STEP_MOVE && step[E_STEP_TIME] == 0, "the first step starts the cue");
    Expect(FindStep(table, cue, A3D_DISC_STEP_DISC, step) != -1, "the cue knocks the ball");
    Expect(step[E_STEP_TIME] == 930 && Near(step[E_STEP_X], 0.93) && step[E_STEP_OTHER] == ball && Near(step[E_STEP_SPEED], 1.0), "the knock comes at 0.93 m after 930 ms at 1 m/s");
    Expect(FindStep(table, ball, A3D_DISC_STEP_DISC, step) != -1 && step[E_STEP_OTHER] == cue, "the ball has the knock too");
    Expect(ReadStep(table, step[E_STEP_NEXT], after) && Near(after[E_STEP_X], 1.03 + float(after[E_STEP_TIME] - 930) / 1000.0), "the ball leaves at 1 m/s");
    Expect(A3D_GetDiscPosition(table, cue, x, y) && Near(x, 0.93) && y == 0.0, "the cue stops where it knocked a disc of the same weight");
    A3D_DestroyDiscTable(table);
}

static stock Walls() {
    new const
        DiscTable:table = A3D_CreateDiscTable(0.0, 0.0),
        disc = A3D_AddDisc(table, 0.0, 0.2, RADIUS),
        wall = A3D_AddDiscWall(table, 1.0, -1.0, 1.0, 1.0, 1.0, 1.0),
        corner = A3D_AddDiscWall(table, 3.0, 0.0, 3.0, 1.0, 1.0, 1.0)
    ;
    new
        step[E_STEP],
        after[E_STEP]
    ;

    Expect(wall == 0 && corner == 1, "walls count from 0");
    A3D_RunDiscTable(table, disc, 1.0, 0.0, 2.0);
    Expect(FindStep(table, disc, A3D_DISC_STEP_WALL, step) != -1, "the disc meets the wall");
    Expect(step[E_STEP_TIME] == 950 && Near(step[E_STEP_X], 0.95) && step[E_STEP_OTHER] == wall && Near(step[E_STEP_SPEED], 1.0), "its edge meets the wall after 950 ms at 1 m/s");
    Expect(ReadStep(table, step[E_STEP_NEXT], after) && Near(after[E_STEP_X], 0.95 - float(after[E_STEP_TIME] - 950) / 1000.0) && Near(after[E_STEP_Y], 0.2), "it comes straight back at 1 m/s");

    A3D_SetDiscPosition(table, disc, 2.0, -0.03);
    A3D_RunDiscTable(table, disc, 1.0, 0.0, 2.0);
    Expect(FindStep(table, disc, A3D_DISC_STEP_WALL, step) != -1 && step[E_STEP_OTHER] == corner, "a disc passing the end of a wall meets it");
    Expect(step[E_STEP_TIME] == 960 && Near(step[E_STEP_SPEED], 0.8), "at the end, 960 ms later, at 0.8 m/s into it");
    Expect(ReadStep(table, step[E_STEP_NEXT], after) && after[E_STEP_X] < step[E_STEP_X] && after[E_STEP_Y] < step[E_STEP_Y], "and turns away from the end");
    A3D_DestroyDiscTable(table);
}

static stock Pocket() {
    new const
        DiscTable:table = A3D_CreateDiscTable(0.0, 0.0),
        disc = A3D_AddDisc(table, 0.0, 0.0, RADIUS),
        pocket = A3D_AddDiscPocket(table, 1.0, 0.0, 0.1)
    ;
    new
        step[E_STEP],
        Float:x,
        Float:y
    ;

    A3D_RunDiscTable(table, disc, 1.0, 0.0, 2.0);
    Expect(FindStep(table, disc, A3D_DISC_STEP_POCKET, step) != -1 && step[E_STEP_OTHER] == pocket, "the disc falls in the pocket");
    Expect(step[E_STEP_TIME] == 850 && step[E_STEP_NEXT] == -1, "when its edge comes within the radius, and stops there");
    Expect(!A3D_IsDiscOnTable(table, disc), "a disc that fell in is off the table");
    Expect(A3D_GetDiscPosition(table, disc, x, y) && Near(x, 0.85), "and keeps the place where it fell in");
    Expect(A3D_RunDiscTable(table, disc, 1.0, 0.0) == 0, "a disc off the table does not move");
    Expect(A3D_SetDiscOnTable(table, disc, true) && A3D_SetDiscPosition(table, disc, 0.0, 0.5) && A3D_IsDiscOnTable(table, disc), "the disc goes back on the table");
    Expect(A3D_RunDiscTable(table, disc, 1.0, 0.0, 2.0) > 0 && FindStep(table, disc, A3D_DISC_STEP_POCKET, step) == -1, "and passes the pocket by");
    A3D_DestroyDiscTable(table);
}

static stock Slowing() {
    new const
        DiscTable:table = A3D_CreateDiscTable(0.05, 0.3, 0.01, 0.1),
        disc = A3D_AddDisc(table, 0.0, 0.0, RADIUS)
    ;
    new
        Float:speed = 1.0,
        Float:distance,
        Float:left,
        ticks,
        step[E_STEP],
        Float:x,
        Float:y
    ;

    while (speed > 0.0) {
        distance += speed * 0.1;
        left = speed - (0.05 + 0.3 * speed) * 0.1;
        speed = left > 0.01 ? left : 0.0;
        ++ticks;
    }

    new const
        count = A3D_RunDiscTable(table, disc, 1.0, 0.0)
    ;

    Expect(count == ticks + 1 && StepsHold(table), "one step to start and one each tenth of a second");
    Expect(ReadStep(table, count - 1, step) && step[E_STEP_NEXT] == -1 && step[E_STEP_TIME] == ticks * 100, "the last step stops the disc");
    Expect(A3D_GetDiscPosition(table, disc, x, y) && Near(x, distance, 0.001), "it stops as far as the rule of the table says");
    printf("[discs] a disc at 1 m/s slides %.3f m in %i ms", x, ticks * 100);

    Expect(A3D_RunDiscTable(table, disc, 0.0, 0.0) == 1, "a disc set moving at no speed has only its first step");

    new const
        DiscTable:smooth = A3D_CreateDiscTable(0.0, 0.0),
        puck = A3D_AddDisc(smooth, 0.0, 0.0, RADIUS),
        cut = A3D_RunDiscTable(smooth, puck, 5.0, 0.0, 20.0, 8)
    ;

    Expect(cut > 0 && cut <= 8 && ReadStep(smooth, cut - 1, step) && step[E_STEP_NEXT] == -1, "a run that nears its steps stops every disc");
    A3D_DestroyDiscTable(table);
    A3D_DestroyDiscTable(smooth);
}

static stock Break() {
    new const
        DiscTable:table = A3D_CreateDiscTable(),
        Float:gap = 0.08,
        Float:apart = (RADIUS + 0.0005) * 2.0
    ;
    new
        Float:x[16],
        Float:y[16],
        count = 1,
        steps,
        pocketed,
        start,
        elapsed,
        bool:inside = true,
        bool:apartAll = true,
        Float:ax,
        Float:ay,
        Float:bx,
        Float:by
    ;

    A3D_AddDiscWall(table, -1.0 + gap, -0.5, -gap, -0.5);
    A3D_AddDiscWall(table, gap, -0.5, 1.0 - gap, -0.5);
    A3D_AddDiscWall(table, -1.0 + gap, 0.5, -gap, 0.5);
    A3D_AddDiscWall(table, gap, 0.5, 1.0 - gap, 0.5);
    A3D_AddDiscWall(table, -1.0, -0.5 + gap, -1.0, 0.5 - gap);
    A3D_AddDiscWall(table, 1.0, -0.5 + gap, 1.0, 0.5 - gap);
    for (new i; i != 3; ++i) {
        A3D_AddDiscPocket(table, float(i - 1), -0.5, 0.06);
        A3D_AddDiscPocket(table, float(i - 1), 0.5, 0.06);
    }

    x[0] = -0.5;
    for (new i; i != 5; ++i) {
        for (new j; j <= i; ++j) {
            x[count] = 0.4 + apart * 0.8661 * float(i);
            y[count] = apart * 0.5 * float(2 * j - i);
            ++count;
        }
    }
    for (new i; i != count; ++i) {
        A3D_AddDisc(table, x[i], y[i], RADIUS);
    }

    start = GetTickCount();
    for (new run; run != 100; ++run) {
        for (new i; i != count; ++i) {
            A3D_SetDiscOnTable(table, i, true);
            A3D_SetDiscPosition(table, i, x[i], y[i]);
        }
        steps = A3D_RunDiscTable(table, 0, 7.0, 0.05);
    }
    elapsed = GetTickCount() - start;

    for (new i; i != count; ++i) {
        if (!A3D_IsDiscOnTable(table, i)) {
            ++pocketed;
            continue;
        }
        A3D_GetDiscPosition(table, i, ax, ay);
        if (floatabs(ax) > 1.0 - RADIUS + 0.001 || floatabs(ay) > 0.5 - RADIUS + 0.001) {
            inside = false;
        }
        for (new j = i + 1; j != count; ++j) {
            if (A3D_IsDiscOnTable(table, j) && A3D_GetDiscPosition(table, j, bx, by) && VectorSize(bx - ax, by - ay, 0.0) < RADIUS * 2.0 - 0.001) {
                apartAll = false;
            }
        }
    }
    Expect(steps > 0 && StepsHold(table), "a break of sixteen discs gives steps");
    Expect(inside, "no disc of the break leaves the table through a wall");
    Expect(apartAll, "no two discs of the break end inside each other");
    printf("[discs] a break of %i discs: %i steps, %i pocketed, %.2f ms a run", count, steps, pocketed, float(elapsed) / 100.0);
    A3D_DestroyDiscTable(table);
}

static stock Refused() {
    new const
        Float:nan = Float:0x7FC00000,
        DiscTable:table = A3D_CreateDiscTable(),
        disc = A3D_AddDisc(table, 0.0, 0.0, RADIUS)
    ;
    new
        step[E_STEP],
        Float:x,
        Float:y
    ;

    Expect(table != INVALID_DISC_TABLE_ID && A3D_IsValidDiscTable(table) && disc == 0, "a table and its first disc");
    Expect(A3D_CreateDiscTable(-0.1) == INVALID_DISC_TABLE_ID, "a negative value is refused");
    Expect(A3D_CreateDiscTable(.step = 0.0) == INVALID_DISC_TABLE_ID, "a step of 0.0 is refused");
    Expect(A3D_CreateDiscTable(nan) == INVALID_DISC_TABLE_ID, "a value that is not a number is refused");
    Expect(A3D_AddDisc(table, 0.0, 0.0, 0.0) == -1, "a disc without a radius is refused");
    Expect(A3D_AddDisc(table, nan, 0.0, RADIUS) == -1, "a disc not at a number is refused");
    Expect(A3D_AddDiscPocket(table, 0.0, 0.0, -0.1) == -1, "a pocket with a negative radius is refused");
    Expect(A3D_AddDiscWall(table, 0.0, 0.0, nan, 1.0) == -1, "a wall not at a number is refused");
    Expect(A3D_AddDisc(INVALID_DISC_TABLE_ID, 0.0, 0.0, RADIUS) == -1, "a table that does not exist takes no disc");
    Expect(!A3D_SetDiscPosition(table, disc, nan, 0.0), "a place that is not a number is refused");
    Expect(!A3D_SetDiscPosition(table, 1, 0.0, 0.0) && !A3D_GetDiscPosition(table, 1, x, y), "a disc that does not exist");
    Expect(A3D_RunDiscTable(table, disc, 1.0, 0.0, 20.0, 3) == 0, "fewer than four steps for each disc are refused");
    Expect(A3D_RunDiscTable(table, disc, nan, 0.0) == 0, "a speed that is not a number is refused");
    Expect(A3D_RunDiscTable(table, disc, 1.0, 0.0, 0.0) == 0, "a limit of 0.0 is refused");
    Expect(A3D_RunDiscTable(table, 1, 1.0, 0.0) == 0, "a disc that does not exist does not run");
    Expect(A3D_GetDiscStepCount(table) == 0, "refused runs leave no steps");
    Expect(A3D_RunDiscTable(table, disc, 1.0, 0.0) > 0 && !ReadStep(table, -1, step) && !ReadStep(table, A3D_GetDiscStepCount(table), step), "steps out of range");

    Expect(A3D_DestroyDiscTable(table) && !A3D_IsValidDiscTable(table), "a destroyed table");
    Expect(!A3D_DestroyDiscTable(table) && A3D_GetDiscStepCount(table) == 0, "is gone with its steps");

    new const
        DiscTable:again = A3D_CreateDiscTable()
    ;

    Expect(again == table, "and its id comes back");
    A3D_DestroyDiscTable(again);
}

forward @Run();
public @Run() {
    Knock();
    Walls();
    Pocket();
    Slowing();
    Break();
    Refused();

    new const
        DiscTable:first = A3D_CreateDiscTable(),
        DiscTable:second = A3D_CreateDiscTable(),
        highest = _:(first > second ? first : second),
        File:flag = fopen(FLAG, io_write)
    ;
    new
        text[48]
    ;

    format(text, sizeof (text), "%i\n%i\n%i\n", highest, gChecks, gFailures);
    fwrite(flag, text);
    fclose(flag);
    printf("[discs] first run checks %i failures %i, tables up to %i left for the restart", gChecks, gFailures, highest);
    SendRconCommand("gmx");
}

forward @Restarted();
public @Restarted() {
    new
        text[16],
        File:flag = fopen(FLAG, io_read),
        highest,
        bool:gone = true
    ;

    fread(flag, text);
    highest = strval(text);
    fread(flag, text);
    gChecks = strval(text);
    fread(flag, text);
    gFailures = strval(text);
    fclose(flag);
    fremove(FLAG);

    for (new i; i <= highest; ++i) {
        if (A3D_IsValidDiscTable(DiscTable:i)) {
            gone = false;
        }
    }
    Expect(gone, "a restart destroys the tables of the gamemode");
    Expect(_:A3D_CreateDiscTable() <= highest, "and their ids come back");

    printf("[discs] DONE checks %i failures %i", gChecks, gFailures);
    SendRconCommand("exit");
}

public OnGameModeInit() {
    if (fexist(FLAG)) {
        SetTimer("@Restarted", 1000, false);
    } else {
        SetTimer("@Run", 1000, false);
    }

    return 1;
}
