# Octave: Music Programming Language

 
### [Online Editor](https://burnham310.github.io/Octave) | [Compiler Manual](./doc/octaveDoc.pdf) | [IDE Support](https://github.com/Burnham310/Octave-Editor-Support)
### 
### 

This language is designed to make music in a programmable way. In this language, user can write music in a syntax akin to a normal music score, but can also define variable, function, if condition, for loop, which can then be compiled into a midi file. The language is declarative with no state and variable assignment.

### Basics:
The fundamental building block of the language is the ***pitch***:
```c
1 // do
2 // re
3 // mi
4 // fa
5 // so
6 // la
7 // ti
```
Another important fundamental building block is the ***chord***, which by itself is a collection of pitches played simultaneously:
```c
[1 3 5] // do, mi, so played together
[6 2 4] // re, fa, la played together

[5] // do played alone # this is a valid chord but can be simplified to 5
[3] // mi played alone # this is a valid chord but can be simplified to 3

[2 2 6] // re, re, la played together # this is a valid chord but can be simplified to [2 6]
[1 1 1] // do, do, do played together # this is a valid chord but can be simplified to [1] or 1

[5 [2 3]] // do played together with re and mi # this is a valid chord but can be simplified to [5 2 3]
[5 [2 3] 6] // do played together with re, mi, and la # this is a valid chord but can be simplified to [5 2 3 6]

[5 [2 3] 6 [1 4]] // do played together with re, mi, and la, and fa played together with do # this is a valid chord but can be simplified to [5 2 3 6 1 4]
```
All of the above cases are possible because cords are just a bunch of more complex instances of pitch, and the language allows for nesting of chords.

Note: putting a standalone pitch in the language will result in a parse error as the language only accepts ***note*** as the smallest unit of music and a ***note*** is a ***pitch*** with a ***duration***.
Here are some examples of valid notes:
```c
1. // do whole note
2. // re whole note

[2 3].. // re and mi played together half note
[1 4].. // do and fa played together half note

[1 3 5]... // do, mi, so played together quarter note
[6 2 4]... // re, fa, la played together quarter note
```
The ***duration*** of a note is defined by the number of dots after the note. The number of dots is directly proportional to the duration of the note. Each dot halves the duration of the note e.g. a whole note is `1.`, a half note is `1..`, a quarter note is `1...`, a eighth note is `1....`, and so on.

The language also supports the following ***special notes***:
```c
o' // Raise the note by an octave
s' // Lower the note by an octave
#' // Raise the note by a semitone
b' // Lower the note by a semitone
```
The special notes must be placed before the note they are modifying and can be chained together  
[NOTE: `'` must be placed between the special note and the note it is modifying]:
```c
o'1. // Raise the whole note do by an octave
s'2.. // Lower the half note re by an octave
#'[1 3]... // Raise the quarter note do and mi by a semitone
b'6.... // Lower the eighth note la by a semitone

o#'[2 4]... // Raise the quarter note re and fa by an octave and raise it by a semitone
s#'[1 3 5].... // Lower the eighth note do, mi, so by an octave and raise it by a semitone
oooo'1. // Raise the whole note do by four octaves

os'1. // Raise the whole note do by an octave and lower it by an octave # this is a valid note but can be simplified to 1.
```

### Examples:
A basic example of the language looks like this:

```c
// Spit Fountain
// Composer: Algernon Cadwallader 
// Genra: Midwest Emo

guitar = | :scale=/D 3 MAJ/, bpm=140, instrument=27:   // section configuration

<init_setting>[volume=100 linear]

    1.. oo'2.. o'7.. o'5..
    2... 3... 5... 6... o'2... o#'4... o'5..

<build_up>[volume=10 linear]

    1.. oo'2.. o'7.. o'5.. 
    2... 3... 5... 6... o'2... o#'4... o'5... oo'2...

    1.. oo'2.. o'7.. o'5.. 
    2... 3... 5... 6... o'2... o#'4... o'5..

<done>[volume=100]
| // Guitar END


// Bass section configuration

bass = | :scale=/D 2 MAJ/, bpm=140, instrument=34:  

<init_setting>[volume=50 linear]
    1.. 3.. 5.. 6..
    o'1... o'3... o'5... o'6...

    1.. 3.. 5.. 6..
    o'1... o'3... o'5... o'7... o'6...

    1.. 3.. 5.. 6..
    o'1... o'3... o'5... o'6...

    1.. 3.. 5.. 6..
    o'1... o'3... o'5... o'6...
|

main = guitar & bass

// END
```

## Run

To compile and run the project, use the following command:

```bash
make
```
For playing the generated output, it is recommended to use [TiMidity++](https://timidity.sourceforge.net/).

## Webasm

To generate WebAssembly, use:
```bash
make webasm
```
Note: Due to limitations in the JavaScript library, certain features (e.g., the [interpolate function]) are not supported in the frontend player.

