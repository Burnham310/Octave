# Octave Reimaged
#### Reference: 
  - [MIDI Specification](https://www.freqsound.com/SIRA/MIDI%20Specification.pdf)
  - [Synthesis](https://hernandis.me/2019/10/20/haskell-infinite-structures.html)


Currently, the languag is able to produce **static** midi files. This is not particular interesting. What I want is the ability to interpret an `Octave` program and generate sounds in real time. This makes the following possible:
  - Infinite sequence of sounds that could change according the environment.
  - Being embedded inside another program (e.g. a game), and passing values back and forth

The MIDI backend is the main block to these functionalities:
## The MIDI problem
Here is brief recap of what MIDI is. It is a format of events that control instruments. A MIDI sequencer sends MIDI events in real time to a synthesizer, which play sounds accordingly. This is a bit different from the **MIDI File** that Octave is concerned with. Real time MIDI event does not contain any time information. However, when stored inside a file, each MIDI events is paired with time, which defines how many `ticks` should the event activates after the previous event. Inside a MIDI file, such sequence of MIDI events paired with a timing is called a `track`. However, there is actaully more than one ways of organizing multiple tracks.

There are actually 3 kinds of MIDI File, conveniently named format 0, 1, 2. Format 2 is basically obselete, and its purpose is irrelevant to the function of Octave anyway.
Currently, our backend produces format 1 MIDI file.
### Format 0
MIDI has the concept of `channel`. The number of channels is the maximum number of notes that can be played together. A `note on` evnet has a channel number speicified. Format 0 is composed of exactly **1** track, where events for differnet channels are all intervened with each other.
### Format 1
Format 1 is composed of **1 or more** tracks that will be played simultaneously. This allow (though not neccessay) events targetting different channels to be placed into diffrent tracks. This reduce the memory usage alongside with a feature in MIDI called the `running status`. However, because each track (and thus channel) is placed sequentially, one track has to end before another track starts. Therefore, it is no longer possible to genereate MIDI in real time with this format.


Suppose the backend is rewritten to generate format 0, we still need to pipe the output to a MIDI synthesizer in order to generate actual sound (currently using [timidity](https://sourceforge.net/projects/timidity/)). This creates extra hustle if we want to embed Octave in a game. We could create our own MIDI backend, as well as other backends to directly generate WAV.
## Interpreter
Currently, an Octave program is evaulated by doing simple AST interpreting. This is suboptimial if want to generate MIDI in **real time**. Here are a few options:
  - Transpile it into another language, for example `C`. This has a lot of overhead, but is probably the easiest and fastest. It also makes it easy to implement intrinsics in C. This will not allow us to port our compiler to WebAssembly.
  - Our own bytecode and VM, probably stack-based.
  - A JIT compiler, maybe together with CIR.
### Garbage Collector
Array is a pretty fundamental structure in Octave, and even a simple example contains a lot of dynamically growing array. Our currently implementation does NOt keep track of unused array at all, and does not free anything.
### Evaulation
Here is a simple example with two instruments.
```
guitar = | :scale=/D 3 MAJ/, bpm=140, instrument=27: 
    1... oo'2... o'7... o'5...
| // Guitar END


// Bass section configuration

bass = | :scale=/D 2 MAJ/, bpm=140, instrument=34:  
    1.. 3..
|
main = [guitar bass]
```
To evaluate this progrom, we starts from `main` (even though it is defined the last). `main` consists two concurrent section, which needs to be evaulated "concurrently":
- We evaluate the first note of `bass` and `guitar`. The resulted the MIDI events are sent.
- We evaluate the second note of `bass` and `guitar`. The notes from `guitar` shall come first. The note from `bass` is halted.
- We evaluate the third note from ONLY `guitar`. The note from `guitar` and `bass` now should be sent together.
- ...
#### Lazy Evaluation?
The aforementioned semantics look like [non-strict evaulation](https://wiki.haskell.org/Lazy_evaluation). The idea of [infinite data structure](https://hernandis.me/2019/10/20/haskell-infinite-structures.html) seems to be a good fit for the purpose of Octave. The downside being, it requires complete rework of the interpreter, and induces a lot of overhead, even with optimization.
## Other
### Ast Internals

Currently, the ast nodes are all allocated within a dynamic array. We could instead use a arena allocator (segmented array), where each resize does NOT realloc the original buffer, but instead allocate another buffer of the same size, creating a linked list of buffers.
This makes pointer stable, so we can directly uses pointer instead of array indexes. 
