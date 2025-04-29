# TODO

## Ast Internals

Currently, the ast nodes are all allocated within a dynamic array. We could instead use a arena allocator (segmented array), where each resize does NOT realloc the original buffer, but instead allocate another buffer of the same size, creating a linked list of buffers.
This makes pointer stable, so we can directly uses pointer instead of array indexes.

## MIDI backend Overhaul

The backend has a lot of bugs and needs to rework. Maybe we should create an midi writer as a seperate package, together with a midi player.

## Additional backend

- [ ] Sine wave backend

## Embedding into another program

We should provide API to be used in another program, including passing variables and contexes back and forth.
