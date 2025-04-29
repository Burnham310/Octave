# Octave Reimaged
Reference: 
  - [MIDI Specification](https://www.freqsound.com/SIRA/MIDI%20Specification.pdf)
## Overview
Currently, the languag is able to produce **static** midi files. This is not particular interesting. What I want is the ability to interpret a `oct` file and generate sounds n real time. This makes the following possible:
  - Infinite sequence of sounds that could change according the environment.
  - Being embedded inside another program (e.g. a game), and passing values back and forth

The MIDI backend is the main block to these functionalities:
### The MIDI problem
Here is brief recap of what MIDI is. It is a format of events that control instruments. A MIDI sequencer sends MIDI events in real time to a synthesizer, which play sounds accordingly. This is a bit different from the **MIDI File** that Octave is concerned with. Real time MIDI event does not contain any time information. However, when stored inside a file, each MIDI events is paired with time, which defines how many `ticks` should the event activates after the previous event. Inside a MIDI file, such sequence of MIDI events paired with a timing is called a `track`. However, there is actaully more than one ways of organizing multiple tracks.

There are actually 3 kinds of MIDI File, conveniently named format 0, 1, 2. Format 2 is basically obselete, and its purpose is irrelevant to the function of Octave anyway.
Currently, our backend produces format 1 MIDI file.
#### Format 0
MIDI has the concept of `channel`. The number of channels is the maximum number of notes that can be played together. A `note on` evnet has a channel number speicified. Format 0 is composed of exactly **1** track, where events for differnet channels are all intervened with each other.
#### Format 1
Format 1 is composed of **1 or more** tracks that will be played simultaneously. This allow (though not neccessay) events targetting different channels to be placed into diffrent tracks. This reduce the memory usage alongside with a feature in MIDI called the `running status`.

TODO

## Ast Internals

Currently, the ast nodes are all allocated within a dynamic array. We could instead use a arena allocator (segmented array), where each resize does NOT realloc the original buffer, but instead allocate another buffer of the same size, creating a linked list of buffers.
This makes pointer stable, so we can directly uses pointer instead of array indexes.

## MIDI backend Overhaul

The backend has a lot of bugs and needs to rework. Maybe we should create an midi writer as a seperate package, together with a midi player.

## Additional backend

- [ ] Sine wave backend

## Embedding into another program

We should provide API to be used in another program, including passing variables and contexes back and forth.
