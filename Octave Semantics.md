the constructor term of our language include events, e.g. note on event, note off event, volume change event, tempo change event... and constructor operators includes "|": event concatenation operator, &: chorus concatenation operator.  Our goal is to interpret our code into the term that build up from only the constructor terms.

We will represent our semantics using big step SOS. And we will also define some functions for convenience.
### Type
$$
Int
$$
$$
AbsPitch:=A\ |\ B\ |\ C\ |\ D\ |\ E\ |\ F\ |\ G\
$$
$$
Degree:=\ 1\ |\ 2\ |\ 3\ |\ 4\ |\ 5\ |\ 6\ |\ 7\
$$
$$
Pitch:=(AbsPitch\ |\ Degree)\ *\ Int
$$
$$
Chord:=[Pitch]
$$
$$
Note:=Chord \ * \ Int
$$
$$
Section:=[Note]
$$
$$
Chorus:=[Section]
$$


### Functions
`NoteToEvent(scale: Scale, note: Note) -> NoteOnEv|NoteOffEv`: this function takes a note as the input and output two events which construct by note definition, i.e., `NoteOnEv(note)|NoteOnEv(note)` 


$$
<i,\sigma> \Downarrow <i> \quad \text{Int}
$$
$$

\frac{<a_1, \sigma>\Downarrow<i_1> \quad <a_2,\sigma>\Downarrow<i_2>}
	{<a_1+a_2,\sigma>\Downarrow<i_1 +_{int} i_2>} \quad \text{Int-Add}
$$
$$
\frac{<a, \sigma> \Downarrow c: Chord}{<a\ [\ .]^n>\Downarrow<Note(c, n)>} \quad \text{Note}
$$

$$
\frac{<e_1, \sigma>\Downarrow<true,\sigma>}{<if\ e_1\ then\ e_2\ else\ e_3, \sigma>\Downarrow<e_2>} \quad \text{if-then-else-true}
$$
$$
\frac{<e_1, \sigma>\Downarrow<false,\sigma>}{<if\ e_1\ then\ e_2\ else\ e_3, \sigma>\Downarrow<e_3>} \quad \text{if-then-else-false}
$$
$$
\frac{<e_1<e_2,\sigma>\Downarrow true}{<for\ e_1\sim<e_2\ loop\ e_1\ \dots\ e_n\ end, \sigma>\Downarrow<e_1\ \dots \ e_2\ for\ e_1+1\sim<e_2\ loop\ e_1\ \dots\ e_n\ end>} \quad \text{for-loop-true}
$$
$$
\frac{<e_1<e_2,\sigma>\Downarrow false}{<for\ e_1\sim<e_2\ loop\ e_1\ \dots\ e_n\ end, \sigma>\Downarrow<\epsilon ,\sigma>} \quad \text{for-loop-false}
$$

$$
\frac{<a_1,\sigma>\Downarrow<v:Chord>\quad<[a_2 \dots a_n],\sigma>\Downarrow<v_2:Chord>}{<[a_1\ a_2\ \dots a_n],\sigma>\Downarrow<v+_{chord}v_2,\sigma>} \quad \text{Chord}
$$
$$
\frac{<decls,\sigma>\Downarrow<\sigma'>\quad <config,\sigma'>\Downarrow<\sigma''
>\quad <note>}{<|decls:config:notes|,\sigma>\Downarrow<>}
$$

$$
\frac{<a,\sigma>\Downarrow<v>}{<x=a, \sigma>\Downarrow<\sigma[v/x]>}\quad \text{Decleration}
$$

$$
\frac{<d,\sigma>\Downarrow<\sigma'>\quad <decls,\sigma'>\Downarrow<\sigma''>}{<d\ decls, \sigma>\Downarrow<\sigma''>} \quad \text{Declerations}
$$


$$
\frac{<decls,\{\}>\Downarrow<\sigma>}{<decls,\{\}>\Downarrow<\sigma[main]>} \quad \text{Program}
$$