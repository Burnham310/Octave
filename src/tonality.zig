const Diatonic = 7;
const Pitch = enum(u3) {
    C = 0,
    D = 2,
    E = 4,
    F = 5,
    G = 7,
    A = 9,
    B = 11,
};

const Mode = enum {
    ionian,
    dorian,
    phrygian,
    lydian,
    mixolydian,
    aeolian,
    locrian,

    const major = Mode.ionian;
    const minor = Mode.aeolian;
};

const Scale = struct {
    tonic: Pitch,
    mode: Mode,
    octave: i32,
};

