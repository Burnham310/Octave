# Octave: Music Programming Language

## Get started

### [Examples](https://burnham310.github.io/Octave/example.html)
### [Online Editor](https://burnham310.github.io/Octave)

This language is designed to make music in a programmable way. In this language, user can write music in a syntax akin to a normal music score, but can also define variable, function, if condition, for loop, which can then be compiled into a midi file. The language is declarative with no state and variable assignment.

## Build
```
zig build
```

To build for wasm,
```
zig build  --sysroot ~/opt/emsdk/upstream/emscripten -Dtarget=wasm32-emscripten -Doptimize=ReleaseSmall
```

