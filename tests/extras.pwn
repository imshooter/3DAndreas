#include <open.mp>
#include <3DAndreas>

/**
 * Extra numbers on physics objects: the slots 0 to 7 and the keys of
 * A3D_EXTRA_KEY, what Has, Get and Remove answer, that a key goes with its
 * object, and the cost of a key next to a slot.
 */

#define KEY_A A3D_EXTRA_KEY(0x616161)
#define KEY_B A3D_EXTRA_KEY(0x626262)
#define N (1000000)

static
    gChecks,
    gFailures
;

main() {}

static stock Expect(bool:ok, const what[]) {
    ++gChecks;

    if (!ok) {
        ++gFailures;
        printf("[extras] FAIL %s", what);
    }
}

public OnGameModeInit() {
    SetTimer("@Run", 3000, false);

    return 1;
}

forward @Run();
public @Run() {
    new
        Physic:box = A3D_CreateStaticBox(1.0, 1.0, 1.0, 0.0, 0.0, 500.0),
        Physic:zone = A3D_CreateZoneSphere(1.0, 10.0, 0.0, 500.0)
    ;

    Expect(A3D_HasExtra(box, 0), "a slot is always there");
    Expect(A3D_GetExtra(box, 3) == 0, "a slot starts at 0");
    Expect(!A3D_HasExtra(box, KEY_A), "no key at first");
    Expect(A3D_GetExtra(box, KEY_A) == 0, "a missing key reads 0");
    Expect(!A3D_HasExtra(box, 8), "slot 8 is out of range");
    Expect(!A3D_SetExtra(box, 8, 1), "slot 8 refused");
    Expect(!A3D_SetExtra(box, -1, 1), "slot -1 refused");

    Expect(A3D_SetExtra(box, 3, 33), "set slot 3");
    Expect(A3D_SetExtra(box, KEY_A, 0), "set key A to 0");
    Expect(A3D_SetExtra(box, KEY_B, 77), "set key B");
    Expect(A3D_HasExtra(box, KEY_A) && A3D_GetExtra(box, KEY_A) == 0, "key A holds 0 and is there");
    Expect(A3D_GetExtra(box, KEY_B) == 77, "key B reads 77");
    Expect(A3D_GetExtra(box, 3) == 33, "slot 3 untouched by keys");
    Expect(A3D_SetExtra(box, KEY_B, 78) && A3D_GetExtra(box, KEY_B) == 78, "key B replaced");
    Expect(A3D_RemoveExtra(box, KEY_A) && !A3D_HasExtra(box, KEY_A), "key A removed");
    Expect(!A3D_RemoveExtra(box, KEY_A), "key A not removed twice");
    Expect(A3D_GetExtra(box, KEY_B) == 78, "key B kept after A went");
    Expect(A3D_RemoveExtra(box, 3) && A3D_GetExtra(box, 3) == 0, "slot 3 back to 0");

    Expect(A3D_SetExtra(zone, KEY_A, 5) && A3D_GetExtra(zone, KEY_A) == 5, "a zone keeps a key");
    Expect(!A3D_HasExtra(box, KEY_A), "keys stay on their own object");

    A3D_DestroyObject(box);
    Expect(!A3D_HasExtra(box, KEY_B), "a destroyed object has no key");
    Expect(!A3D_SetExtra(box, KEY_B, 1), "a destroyed object takes no key");

    new const
        Physic:again = A3D_CreateStaticBox(1.0, 1.0, 1.0, 0.0, 20.0, 500.0)
    ;

    Expect(again == box, "the id came back");
    Expect(!A3D_HasExtra(again, KEY_B), "an id that came back starts without keys");
    Expect(A3D_GetExtra(again, 0) == 0, "and with its slots at 0");

    printf("[extras] DONE checks %i failures %i", gChecks, gFailures);

    new
        Physic:other = A3D_CreateStaticBox(1.0, 1.0, 1.0, 0.0, 40.0, 500.0),
        sink,
        start,
        base
    ;

    A3D_SetExtra(again, 0, 6);
    A3D_SetExtra(again, KEY_A, 7);
    A3D_SetExtra(again, KEY_B, 8);

    start = GetTickCount();
    for (new i; i != N; ++i) {
        sink += i;
    }
    base = GetTickCount() - start;

    start = GetTickCount();
    for (new i; i != N; ++i) {
        sink += A3D_GetExtra(again, 0);
    }
    printf("[extras] slot                     %5.1f ns", float(GetTickCount() - start - base) * 1000000.0 / float(N));

    start = GetTickCount();
    for (new i; i != N; ++i) {
        sink += _:A3D_HasExtra(other, KEY_A);
    }
    printf("[extras] key, not ours            %5.1f ns", float(GetTickCount() - start - base) * 1000000.0 / float(N));

    start = GetTickCount();
    for (new i; i != N; ++i) {
        if (A3D_HasExtra(again, KEY_B)) {
            sink += A3D_GetExtra(again, KEY_B);
        }
    }
    printf("[extras] key, ours (has + get)    %5.1f ns", float(GetTickCount() - start - base) * 1000000.0 / float(N));

    printf("[extras] BENCH %i", sink != 0);
    SendRconCommand("exit");
}
