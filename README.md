# Octave

Music Programming Language

This language is designed to make music in a programable way. In this language, user can write music in a syntax akin to a normal music score, but can also define variable, function, if condition, for loop, which can then be compiled into a midi file. The language is declarative with no state and variable assignment.

A basic example of the language looks like this:

```c
twinkle := |:[key=cmajor, bpm=70]: 1.., 1.., 5.., 5.., 6.., 6.., 5., 4.., 4.., 3.., 3.., 2.., 2.., 1.|;
```

This defines a `section` named `twinkle`, which is basically a sequence of note. The `|` denotes the start and end of a `section`. The notes in the same `section` share some common attributes: key, tone, BPM (beats per minute), base volume, etc., which can be defined in the `[]`( or omitted to use default values). The `:` is used to separate different parts of a section. The first part is variable definition, the second part is configuration, the third part is notes. The `:` still has to be present even if the part is empty, for ease of parsing.

The numbers are notes from the scale, and dots after them denote the length of the note, with one dot being a quarter note, and two dots being an eighth note, etc.. Our compiler will take this and generate the twinkle twinkle little star melody.

You can also play multiple notes simultaneously, by combining them with `_`:

```
twinkle = |:[key=cmajor, bpm=100]: (1_3_5).., 1.., 1.., 5.., 6.., 6.., 5., 4.., 4.., 3.., 3.., 2.., 2.., 1.|;
```

which turns the first note into a C major chord.

You can also define `variables` in the beginning of a section. For example, you can bind `1_3_5` to the variable `c`:

```c
twinkle = |c=(1_3_5) : [key=cmajor, bpm=70] : 1.., 1.., 5.., 5.., 6.., 6.., c., 4.., 4.., 3.., 3.., 2.., 2.., 1.|;
```

Similar to the `_` operator for notes, you can letsudo apt install libglib2.0-dev
 two sections play at the same time by doing `_` on them:

```
melody = |[key=cmajor, bpm=70]: 1.., 1.., 5.., 5.., 6.., 6.., 1., 4.., 4.., 3.., 3.., 2.., 2.., 1.|;

harmony = |c=(1_3_5), f=(4_6_8), g=(5_7_9): [key=cmajor, bpm=70]; c, f., c., f., c., g., c.|;

twinkle = melody_harmony;
```

or append them together with `+`: 

```
melody1 = |[key=cmajor, bpm=70]: 1.., 1.., 5.., 5.., 6.., 6.., 1.|;
melody2 = |[key=cmajor, bpm=70]: 4.., 4.., 3.., 3.., 2.., 2.., 1.|;
melody 	= melody1 + melody2;
```

The section itself can also be parametrized:

```
twinkle (k: Key, y: Note) = |x=1_3_5: [key=k, bpm=70]: x.., 1.., x.., 5.., y.., 6.., x., 4.., 4.., 3.., 3.., 2.., 2.., 1.|
```

Here, `k` and  `y` are parameters of the section. `Key` and `Note` are built-in types in the language. To declare a parameter we have to declare its type. 

A section or notes may also be repeated with the `repeats` keyword.

```
section = |:[]: 1. repeats 10|
```

You can also have if-condition:

```
section (x: Note) = |:[key=if x == 1 then gmajor else gminor, bpm=70]: x.|;
```

## Run
install dependencies

```bash
sudo apt-get install libsmf-dev     
sudo apt-get install libglib2.0-dev
```

make executable via:
```bash
make
```