const std = @import("std");

pub const Diatonic = 7;
pub const Pitch = enum(u8) {
    C = 0,
    D = 2,
    E = 4,
    F = 5,
    G = 7,
    A = 9,
    B = 11,
};

pub const Mode = enum {
    ionian,
    dorian,
    phrygian,
    lydian,
    mixolydian,
    aeolian,
    locrian,

    pub const major = Mode.ionian;
    pub const minor = Mode.aeolian;
    pub const Basis = [Diatonic]isize {0, 2, 4, 5, 7, 9, 11};
};



pub const Scale = struct {
    pub const MiddleCMajor = Scale {
        .tonic = Pitch.C,
        .octave = 5,
        .mode = Mode.major,
    };
    tonic: Pitch,
    mode: Mode,
    octave: i32,
    pub fn get_abspitch(scale: Scale, degree: usize) isize {
        std.debug.assert(degree <= Diatonic and degree >= 1);
        const base = @intFromEnum(scale.tonic) + scale.octave * 12;
        // degree is 1-based
        const degree_shift = degree + @intFromEnum(scale.mode) - 1;
        // Step is how much we should walk from the tonic
        const step = if (degree_shift < Diatonic)
            Mode.Basis[degree_shift] - Mode.Basis[@intFromEnum(scale.mode)]
            else 12 + Mode.Basis[degree_shift % Diatonic] - Mode.Basis[@intFromEnum(scale.mode)];
            return @intCast(base + step);
    }
};

// 60 is the middle C (C3) in the MIDI convention
// https://computermusicresource.com/midikeys.html
pub fn abspitch_to_freq(abspitch: isize) f32 {
    return 130.81 * @exp2(@as(f32, @floatFromInt(abspitch - 60))/12.0);
}

pub const DrumSymbol = enum {
    S, // snare
    B, // bass drum
    H, // hi-hat
};

