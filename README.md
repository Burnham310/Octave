# Octave
Music Programming Language

This language is designed to make music in a programable way. In this language, user can write music in a syntax akin to a normal music score, but can also define variable, function, if condition, for loop, which can then be compiled into a midi file. The language is declarative with no state and variable assignment.

A basic example of the language looks like this:

```c
twinkle := |[key=cmajor, bpm=70]; 1.., 1.., 5.., 5.., 6.., 6.., 5., 4.., 4.., 3.., 3.., 2.., 2.., 1.|
```

This defines a `section` named `twinkle`, which is basically a sequence of note. The `|` denotes the start and end of a `section`. The notes in the same `section` share some common attributes: key, tone, BPM (beats per minute), base volume, etc., which can be defined in the `[]`( or omitted to use default values).
The numbers are notes from the scale, and dots after them denote the length of the note, with one dot being a quarter note, and two dots being an eighth note, etc.. Our compiler will take this and generate the twinkle twinkle little star melody.

You can also play multiple notes simultaneously, by combining them with `_`:

```
twinkle := |[key=cmajor, bpm=100,]; (1_3_5).., 1.., 1.., 5.., 6.., 6.., 5., 4.., 4.., 3.., 3.., 2.., 2.., 1.|
```

which turns the first note into a C major chord.

You can also define `variables` in the beginning of a section. For example, you can bind `1_3_5` to the variable `c`:

```c
twinkle := |c=(1_3_5); [key=cmajor, bpm=70]; c.., 1.., 5.., 5.., 6.., 6.., c., 4.., 4.., 3.., 3.., 2.., 2.., 1.|
```

Similar to the `_` operator for notes, you can let two sections play at the same time by doing `_` on them:

```
melody := |[key=cmajor, bpm=70]; 1.., 1.., 5.., 5.., 6.., 6.., 1., 4.., 4.., 3.., 3.., 2.., 2.., 1.|

harmony := |c=(1_3_5), f=(4_6_8), g=(5_7_9); [key=cmajor, bpm=70]; c, f., c., f., c., g., c.|

twinkle := melody_harmony
```

The section itself can also be parametrized:

```
twinkle (k: Key, y: Note) := |x=(1_3_5); [key=k, bpm=70]; x.., 1.., x.., 5.., y.., 6.., x., 4.., 4.., 3.., 3.., 2.., 2.., 1.|
```

Here, `k` and  `y` are parameters of the section. `Key` and `Note` are built-in types in the language. To declare a parameter we have to declare its type. 

This is not a comprehensive list of the syntax and semantic of the language (character limitation). Other features include: algebraic operations on notes, for-loop, global variable. 

In terms of teamwork, Hu Jingyan is responsible for lexing, Yiwei Gong is responsible for parsing, and Tianyi Huang is responsible converting AST into midi.We will be using C++ to implement this language.



