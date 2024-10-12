# Octave: Music Programming Language


### [Try It Out!!](https://burnham310.github.io/Octave)

This language is designed to make music in a programable way. In this language, user can write music in a syntax akin to a normal music score, but can also define variable, function, if condition, for loop, which can then be compiled into a midi file. The language is declarative with no state and variable assignment.

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
make executable via:
```bash
make
```
