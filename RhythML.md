## RhythML Syntax Specification

RhythML is a plaintext format designed to sequence pitch and CV patterns in a eurorack-style environment. The format is agnostic to what you are using each output for, which allows for specifying both rhythmic triggers and melodic sequences in a compact, easily editable, human readable text format. Below is a detailed specification of the RhythML syntax as implemented by the `Spellbook` module.

### Basic Structure
- **Lines**: Each line in the text input corresponds to a sequence step. Steps are processed sequentially with the receipt of each clock signal at the CLOCK_INPUT.
- **Columns**: Individual values within a line are separated by commas `,`. Each column corresponds to one of the 16 outputs. Columns 17 and beyond are ignored (for now).

### Sequencing using RhythML
Each cell in a grid can contain values in one of these formats. Every cell is parsed independently, so you can freely mix and match formats to express intentions and uses in the music.

1. **Voltage Levels (Decimal Numbers)**:
   - Example: `5`, `-3.5`, `0.0`, `12`
   - Specifies a voltage directly.
   - Anything that parses to a floating point number is allowed, but note Eurorack convention is to stay within -10 to +10 volts, with a 10 volt range between min and max value, and some modules may not accept unexpected numbers.

2. **Gate and Trigger Commands**:
   - `X`: Outputs a gate signal (10 volts) for the entire step, equivalent to writing `10` in the cell.
   - `T`: Outputs 0v for 1ms, then 10v for 1ms, then 0v thereafter.
   - `R`: Outputs 0 volts for the first 1ms of the step, then a high signal (10 volts) for the remainder.
   - Gates (and all signals except for Triggers and Retriggers) are 100% step width; two consecutive gates will output a continuous signal.

3. **Scientific Pitch Names**:
   - Format: `<NoteName><Octave>`
   - Example: `C4`, `G#3`, `Db5`
   - Valid note names are: C, C#, Db, D, D#, Eb, E, F, F#, Gb, G, G#, Ab, A, A#, Bb, B.
   - If no octave is included, but it is still valid note name, a default of 4 is used. For example "C" will be read as "C4".
   - Outputs a voltage corresponding to the specified musical note, in Eurorack 1V/oct standard, where C4 = 0V, C#4 = 1/12V, ..., B4 = 11/12V, C5 = 1V, etc.
   
4. **MIDI Note Numbers**
   - Format: `m<Number>`
   - Example: `m60`, `m72`
   - MIDI Note 60 = C4 = 0.0v.
   - Decimals and numbers outside the normal MIDI range of 0 to 128 are allowed, but are probably not going to be respected if you try to send those pitches back into a MIDI environment
   
5. **Semitones**
   - Format: `s<Number>`
   - Example: `s0`, `s7`, `s-12`
   - Decimals are allowed
   - Semitones are numbered relative to C4, which means they are basically the same as MIDI Notes except 0 = C4

6. **Percentages**
   - Format: `<Number>%`
   - Example: `50%`, `100%`, `-12.5%`
   - Numbers ending in % are divided by 10, so that 100% becomes 10.0v, -50% becomes -5.0v, etc.
   - This scaling is specific to eurorack. If you are implementing RhythML in another environment, percentages should be translated according to the standards of that environment. For example, if you were parsing RhythML into MIDI, 0%-100% might become 0-127.

7. **Empty Cells**:
   - An empty cell or a cell containing only whitespace or comments will leave the output's current voltage as-is, unless that "current voltage" was from a gate, trigger, or retrigger, in which case it reverts to 0.0.

Note that unlike MIDI or a Tracker, a single cell with a pitch in it does not automatically generate anything like a gate or a "note on", it just outputs one voltage to one output, like a Eurorack sequencer. You would need to sequence the rhythm using voltages/gates/triggers/retriggers in another column, or get rhythm from another source like your clock. Remember, any column can be for used for anything you need a voltage for: pitches, gates, velocity, CVs, etc. Think modular!

### Comments
- Any text following a `?` in a cell is considered a comment (until the next `,` (thus comments CANNOT contain `,`)) and is ignored during parsing.

Example:

`5.0 ? This is a comment, 5.5 ? You can put a comment in any cell`

Consider using comments in the first row to label your columns:

```
E4 ? Pitch, X ? Gate, 10 ? Velocity
C5        , T       , 8
D5        , T       , 6
B4        ,         , 5
```

Or to explain musical intentions:

```
7 ? Melody volume, 5 ? Backing volume, 0 ? Drum chance, 0.0 ? Reverb, 3.5 ? Filter Cutoff, ? Notes; Soft start; no drums; slight reverb; mild filter
7                , 5                 , 0              , 0.0         , 3.5                , ? Continue soft intro
8                , 6                 , 3              , 0.5         , 4.0                , ? Increase all volumes; introduce drums softly
```

### Whitespace
- Whitespace in cell values is ignored.
- Cells are normalized during editing such that each cell in a column has uniform spacing padded with spaces to align columns vertically for readability.
	- Blank lines are NOT ignored, and will become new blank rows, with commas added automatically.