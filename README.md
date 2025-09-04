# Octave: Music Programming Language

## Get started

### [Examples](https://burnham310.github.io/Octave/example.html)
### [Online Editor](https://burnham310.github.io/Octave)

This language is designed to make music in a programmable way. In this language, user can write music in a syntax akin to a normal music score, but can also define variable, function, if condition, for loop, which can then be compiled into a midi file. The language is declarative with no state and variable assignment.
## Roadmap
- [ ] 1.0
  - [ ] Instruments
    - [ ] Allow defining instrument within Octave
    - [ ] Symbol as note
  - [ ] for loop
    - [x] capture the value of loop variable
    - [ ] infinite loop
  - [ ] tag that the can mark a certain position
    - [ ] "wait" for the tag to be reached
    - [ ] volume interpolation based on tag
  - [ ] if condition
  - [ ] arithmetic on fraction

## Build
```
zig build
```

To build for wasm,
```
zig build  --sysroot ~/opt/emsdk/upstream/emscripten -Dtarget=wasm32-emscripten -Doptimize=ReleaseSmall
```

