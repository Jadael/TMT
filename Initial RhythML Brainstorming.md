```
0:  BassPitch,  BassGate,  MPitch,  MGate,  Kick,  Snare,  Hat,  Volume,    Comment
1:  c2,         x,         e4,      x,      t,     ,       t,    10,
2:  ,           ,          ,        ,       ,      ,       t,    ,          
3:  ,           ,          g4,      x,      t,     t,      t,    ,
4:  ,           ,          ,        ,       ,      ,       t,    ,          
5:  c2+s7,      x,         c5,      x,      t,     ,       t,    ,          ? modulate bass ?
6:  ,           ,          ,        ,       ,      ,       t,    ,          
7:  ,           ,          g4+s9,   x,      t,     t,      t,    ,
8:  ,           ,          ,        ,       ,      ,       t,    ,          
9:  c2*1.1,     x,         e4,      x,      t,     ,       t,    ,          ? slightly higher bass ?
10: ,           ,          ,        ,       ,      ,       t,    ,          
11: ,           ,          g4,      x,      t,     t,      t,    ,
12: ,           ,          ,        ,       ,      ,       t,    ,          
13: c2*1.1+s7,  x,         c5,      x,      t,     ,       t,    ,          ? modulate bass ?
14: ,           ,          ,        ,       ,      ,       t,    ,          
15: ,           ,          g4+s1,   x,      t,     t,      t,    ,          ? add semi-tone ?
16: ,           ,          ,        ,       ,      ,       t,    ,          
17: f2,         x,         a#4,     x,      t,     ,       t,    ,          ? key change ?
18: ,           ,          ,        ,       ,      ,       t,    ,          
19: ,           ,          a#4+s9,  x,      t,     t,      t,    ,
20: ,           ,          ,        ,       ,      ,       t,    ,
21: f2+s7,      x,         c6,      x,      t,     ,       t,    ,          ? modulate bass ?
22: ,           ,          ,        ,       ,      ,       t,    ,
23: ,           ,          c6-s1,   x,      t,     t,      t,    ,          ? decrease by semi-tone ?
24: ,           ,          ,        ,       ,      ,       t,    ,
25: f2,         x,         a#4,     x,      t,     ,       t,    volume/2,  ? reduce volume by half ?
26: ,           ,          ,        ,       ,      ,       t,    ,
27: ,           ,          a#4+s7,  x,      t,     t,      t,    ,
28: ,           ,          ,        ,       ,      ,       t,    ,
29: f2,         x,         c6,      x,      t,     ,       t,    volume/2,  ? further reduce volume ?
30: ,           ,          ,        ,       ,      ,       t,    ,         
31: ,           ,          c6-0.1,  x,      t,     t,      t,    ,          ? pitch shift ?
32: ,           ,          ,        ,       ,      ,       t,    ,
```

---

- Each row is one step
	- Smallest subdivision needed in the song; often an 8th note
- Steps are progressed by external trigger
- Up to 16 comma separated columns
	- Empty cells are allowed, and continue output the most recently specified value in that column
		- default of 0.0 if there was none, or
		- 0.0 if the last value was a gate, trigger, or re-trigger
	- Columns can be labelled in the first row
	- Blank lines are ignored
	- Lines that start with `??` are comment lines; ignored
		- Inline comments are between `? ?`:
			- `1.75 ? Start bringing up the filter ?, B3 ? Key change ?, X ? Release tigers ?`
		- Comments cannot contain linebreaks
	- Column values parse to a decimal number to be output by the module as a Control Voltage (CV)
		- By convention, most users will assume a Eurorack style working range of -10.0 to +10.0, but voltages/notes/values outside that range are fully supported
		- Column values can be written in a variety of formats:
			- Arbitrary digit decimals, e.g. `10.8333`, `2`, `-1.5`, `1337`
			- Scientific pitch notation, e.g. `C4`, `A#5`, `B3`, `C-1`
			- MIDI note numbers, prefixed with `n`, e.g.: `n60` for C4
			- Semitones, prefixed with `s`, e.g.: `s7` for a perfect fifth
			- `X` - a gate; shorthand for 10.0
			- `T` - a trigger; special action outputs a 10.0 signal for 10 milliseconds, then 0.0 thereafter
			- `R` - re-trigger; outputs 0.0 for 10ms, then 10.0 (like a gate) thereafter
		- When reading/outputting, inputs are translated from their respective formats to a floating point number, and the appropriate polyphonic channel is updated
			- Note only the trigger has a sense of time, typically every signal channel simply holds its last value until directed otherwise
			- Unparseable inputs get commented out and skipped, with a `!` for emphasis
				- `?! foo ?`
	- Use multiple columns as needed for layered voices, articulation, effect CVs, etc.
	- You can do arithmetic, including mixing formats, e.g.
		- `C4+s7` - C4 increased by a fifth to get G4
		- `5.25*2` - 5.25 times 2, for 10.5
		- `C2+(1/2)` - C2 (-2.0 volts) plus (1/2) to get -1.5
	- You're allowed to reference columns by label, which parses to the most recent resolved value from that column (defaulting to 0.0 if null or NAN), and even include it in arithmetic, e.g.:
		- `label1`
		- `label1+C4`
		- `label1 * label1`
		- `label2/(label3*2)`
		- `label4 - 1.5`
	- An operator at the start of the cell with no left operand will assume the prior value of the current column, e.g.:
		- `+1` - Increment prior value by +1.0
	- If a label name ever conflicts with core syntax, it always parses as core syntax
	- As a "deep magic" feature, you can actually have any number of columns, giving you plenty of space for an arbitrary amount of data to use in formulas, it's just that only the first 16 are output by the module
- Optional-but-recommended zeroeth column containing the step number, delimited before a `:` colon
	e.g. `1:C4,G4`
	- This allows empty rows to be discarded for "terse" files when sharing, while preserving information about the gaps
- Quality of life tools:
	- Editor can convert between "terse" and "verbose", where empty rows are either re-inserted or removed, respecting step numbers
	- Automatically add/remove/renumber steps
	- Automatically pad columns for readability
	- Spreadsheet style keyboard controls
		- Arrow move between cells
		- Insert/delete/re-arrange rows and columns
	- Time-shift and time-stretch selections
	- Continue patterns
	- Transpose selections
