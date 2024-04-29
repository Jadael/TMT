# T's Musical Tools
A collection of esoteric modules mostly focused around manipulating RNG and polyphonic signals in useful ways.

- [VCV Rack Library plugin page](https://library.vcvrack.com/TMT)

# Shuffle

![Shuffle](screenshots/shuffle.png)

Shuffles the channels of an incoming polyphonic signal and outputs the re-ordered signal. The module allows you to control the number of output channels and provides a trigger input to initiate the shuffle. The shuffle is deterministic, meaning that the same seed will always give the same shuffle. If no seed is provided, the module will generate and use a new (unpredictable) seed on every trigger input.

- Polyphonic input and output
- Control over the number of output channels
- Trigger input to initiate shuffle
- Seed input to control deterministic randomness of shuffle
- Outputs track the inputs as they change, while retaining the current shuffle
- Alt Mode: Allow input channels to potentially be selected multiple times in the output

## Inputs & Outputs

- Shuffle Trigger: Triggers a shuffle of the polyphonic voltages, according to the current Seed.
- Poly In (optional): Polyphonic signal to be shuffled. If no cable is connected, a one-octave chromatic scale will be used as the default input.
- Seed (optional): Provide a random seed for the shuffle. The same seed will always gives the same shuffle. If no cable is connected, a new (unpredictable) seed will be generated on every trigger input.
- Number of Output Channels (optional) - Controls the number of output channels: one channel at 0 volts, half the input channels at 5 volts, all input channels at 10 volts. If no cable is connected, all input channels will be used.
- Poly Out: Re-ordered polyphonic channels.

---

# Calendar

![Calendar](screenshots/calendar.png)

Calendar generates a set of very slow LFO-like signals representing the progress through various various time/calendar units. The module outputs voltages ranging from 0 to 10V, where each voltage corresponds to the current "progress" through the respective time unit.

- Ramps, triggers, and gates for seconds, minutes, hours, days, months, quarters, and years
- Voltages range from 0 to 10V
- Alt Mode: Use UTC time instead of Local time.

## Outputs
Each time unit has a row of five outputs:

- Linear Ramp: This output is a smooth, continuous voltage ranging from 0 to 10V. It is calculated by taking the current progress (a value between 0 and 1) of the time unit and scaling it by a factor of 10. As the progress of the time unit increases, the output voltage increases linearly.
- Stepped Ramp: This output is a stepped version of the smooth output. It divides the progress into equal intervals based on how that time unit is traditionally subdivided. This output can be useful for leaning into the nature of using times and dates in a patch.
	- Second: Four steps
	- Minute: 60 steps (i.e. seconds)
	- Hour: 60 steps (i.e. minutes)
	- Day: 24 steps (i.e. hours)
	- Week: 7 steps (i.e. days)
	- Month: 4 steps (i.e. weeks)
	- Quarter: 3 steps (i.e. months)
	- Year: 12 steps (i.e. months)
- Trigger: This output sends a 10V trigger signal at the beginning of each time unit cycle. The trigger is activated when the progress is less than or equal to 1/10th of the reciprocal of the time unit's duration. For instance, in the case of hours, the trigger would be activated when the progress is less than or equal to 1/240 (0.1/24). This output is useful for triggering events or resets at the start of each time unit.
- Gate: This output provides a 10V gate signal for the first half of the time unit cycle and 0V for the second half. The gate is active (10V) when the progress is less than 0.5 (50% of the time unit) and inactive (0V) otherwise. This output can be used to create sustained events or behaviors during the first half of each time unit cycle.
- Inverse Gate: This output is the inverse of the gate output. It provides a 0V signal for the first half of the time unit cycle and a 10V signal for the second half. The reverse gate is active (10V) when the progress is greater than or equal to 0.5 (50% of the time unit) and inactive (0V) otherwise. This output can be used to create sustained events or behaviors during the second half of each time unit cycle.

---

# Seed

![Seed](screenshots/seed.png)

Seed is a random voltage generator with 16 outputs and a polyphonic output, providing random values based on a given input voltage as the seed. When the input seed changes, the module generates 16 random numbers normalized between 0.0V and 10.0V. These random values are output individually across the 16 numbered outputs and as a polyphonic signal through the polyphonic output.

- 16 individual random voltage outputs
- Polyphonic output with 16 channels
- Random voltage generation based on the input seed
- Regenerates random values only when the seed changes
- Alt Mode: Snap outputs to 0v or 10v - useful for generating a gate pattern.

## Inputs and Outputs

- Seed: Input voltage used as the seed for random number generation. If no input is connected, initializes to a random seed.
- Poly Out: Polyphonic output with 16 channels containing random voltages.
- Out 1 - Out 16: Individual outputs for each of the 16 random voltages generated.

---

# Ouroboros

![Ouroboros](screenshots/ouroboros.png)

Ouroboros steps through polyphonic channels to turn it into a sequence.

- Step through a polyphonic signal as a monophonic sequence
- CV controlled sequence length
- Reset and Clock inputs for precise control over sequence timing
- Alt Mode: Output the average of current and next step

## Inputs and Outputs

- Poly Input: A polyphonic input for connecting the source polyphonic signal to step through.
- Clock: An input for connecting an external clock source to control the stepping through the sequence.
- Reset: An input for connecting a reset trigger signal to reset the sequence to the first step. Uses Grids style: if Clock is low, arm the Reset for next Clock. If Clock is high, Reset immediately.
- Length: An input for controlling the sequence length using an external CV signal (0V: one step, 10V: all steps).
- Out: An output for the resulting monophonic sequence.

---

# Append

![Append](screenshots/append.png)

Takes multiple mono or poly input signals and combines them into a single polyphonic output with a selectable range of channels. Control the output width and rotation to create a customizable polyphonic subset of voltages from the input signals.

1. Connect up to 16 signals to the input jacks (Signal 1 to Signal 16). The module collects all channels from all connected signals into an internal buffer, in input order.
2. Use the Width input to control the number of channels in the output. The range goes from 1 channel up to the total number of connected input channels (or 16, whichever is lower). A 0V signal corresponds to a 1-channel output, and a 10V signal corresponds to the maximum number of channels.
3. Use the Rotation input to control the starting point of the output channels. A 0V signal corresponds to the first channel from the first input signal, and a 10V signal corresponds to the last channel from the last input signal.
4. Connect the Poly Out output to the desired destination. The module will output a polyphonic subset of voltages from the inputs, as specified by the Width and Rotation controls.

## Inputs and Outputs

- Signal 1-16 Inputs: Connect up to 16 mono or poly input signals.
- Width: Control voltage input to set the output width (number of channels).
- Rotation: Control voltage input to set the output starting point.
- Out: The polyphonic output containing the selected voltages from the input signals.

---

![Sight](screenshots/sight.png)

# Sight
A real-time, logarithmic scope. Time is shown non-linearly, letting you see a both short term and long term structures in the signal. Less precise than a normal scope; this is intended to just "keep an eye" on a signal and form an intuition for what it's up to. Scope range is -10 to +10.

## Inputs / Outputs
- **Voltage Input**: Connect any voltage source to this input to display its waveform on the scope.

---

# Spellbook (PREVIEW)

![Spellbook](screenshots/spellbook.png)

Spellbook is a module to sequence pitch and control voltage (CV) patterns in a eurorack-style environment using the plain text [RhythML syntax](RhythML.md). It has 16 outputs (and a polyphonic output which combines all of them), each of which outputs a voltage determined by the corresponding column in RhythML-formatted text input. Spellbook is not yet in the VCV Rack Library version of this plugin; check [Releases](https://github.com/Jadael/TMT/releases) to [install the preview](https://vcvrack.com/manual/Installing#Installing-Rack-plugins).

## Inputs & Outputs

- **Step Forward**: Advances to the next step in the sequence on the rising edge of a trigger.
- **Step Backward**: Advances to the prior step in the sequence on the rising edge of a trigger.
- **Reset Input**: Resets the sequence to the first step on the rising edge of the input signal.
- **Index Input**: Set the current step to a specific index, where 0v is the first step through to 10v for the last step, like a Phasor

- **Index Mode Toggle**: Toggle the Index to "absolute address" mode, where 1v is step one, 2v is step two, etc.

- **Poly Out**: Outputs all 16 voltages as a polyphonic signal.
- **Out 1 - Out 16**: Individual outputs for each column specified in the RhythML sequence.
- **Relative Index Out**: Outputs the current step as 0v = step 1, through to 10v = last step.
- **Absolute Index Out**: Outputs the current step as a voltage, e.g. step 3 outputs 3.0v.

## Sequencing with RhythML

Spellbook sequences are programmed using the RhythML format, a syntax to define pitch and CV patterns in plain text. Each line in the text input represents a sequence step, triggered sequentially by the clock input. Columns in the text represent the 16 outputs, allowing for complex configurations across multiple hardware modules.

### RhythML Features
**Voltages and Gates**
- **Decimal Voltages**: Directly specify voltage outputs by writing decimal numbers.
- **Percentages**: Numbers ending in `%` (e.g. `50%` or `12.5%`), are translated so that 0% = 0.0v and 100% = 10.0v.
- **Gate and Trigger Commands**: Use `X` or `_` for a gate-with-retrigger (guarantees a rising edge), `T` or `^` for a 1ms trigger pulse (guarantees a rising edge), and `W` or `|` for a full-width gate (no rising edge); as shorthand for rhythmic sequences.

**Pitch Representations**
These are all parsed and translated into 1v/Octave. Decimals are allowed for all of them, but microtones may not be supported by all things you send those signals to.
- **Scientific Pitch Names**: Specify pitches using standard note names (e.g., `C4`, `G#3`).
- **MIDI numbers**: Numbers prefixed with `m` (e.g. `m60`) are parsed as MIDI note numbers. 60 = C4.
- **Semitones**: Numbers prefixed with `S` (e.g. `s7`) are parsed as semitones relative to C4.
- **Cents**: Numbers ending with `ct` are parsed as cents relative to C4.
- **Hertz**: Numbers ending with `Hz` are parsed as frequencies.


Refer to [RhythML Syntax Specification](RhythML.md) for comprehensive guidelines on writing sequences for Spellbook.

### Usage Example

Here's a simple RhythML sequence to get started:

```
E4 ? Pitch, X ? Gate, 100% ? Velocity
C5        , X       , 80%
D5        , X       , 60%
B4        , X       , 50%
```

In this example, each step sets a pitch in column one, uses column two for gating, and controls a velocity CV in column three. The sequence plays a C major arpeggio, gradually lowering the velocity on each note. The labels, like `? Pitch`, are ignored because of the `?`.

Here's the same arpeggio, but with a little more rhythmic variation:

```
E4 ? Pitch, X ? Gate, 100% ? Velocity
          , |       , 
          , |       , 
C5        , X       , 80%
D5        , X       , 60%
          ,         , 
B4        , X       , 50%
          ,         , 
```

Notice the way this pattern holds notes by using consecutive gates over several steps, using Retriggers to ensure a new rising edge when the step begins (even after a full width gate). The first note is held for three steps.

Watch a brief demonstration here:

[![YouTube Demo](https://img.youtube.com/vi/vhQHPlpJW-Q/0.jpg)](https://www.youtube.com/watch?v=vhQHPlpJW-Q)

---

# Stats (PREVIEW)

![Stats](screenshots/stats.png)

Stats is a statistical function module for VCV Rack. It computes and outputs various statistical metrics from the signals of a polyphonic input cable. Stats is not yet in the VCV Rack Library version of this plugin; check [Releases](https://github.com/Jadael/TMT/releases) to [install the preview](https://vcvrack.com/manual/Installing#Installing-Rack-plugins).

## Inputs & Outputs

- **Toggle Audio Rate**: By default, Stats runs at step rate (~10-60hz). Toggle this to run at audio rate, which is pretty CPU heavy.

- **Polyphonic Input**: Receives the polyphonic signals to analyze.

- **Mean Output**: Outputs the average voltage of the input signals.
- **Median Output**: Outputs the median voltage.
- **Mode Output**: Outputs the most frequent voltage(s) as a polyphonic signals.
	- If multiple modes are found, it lists all of them as a polyphonic signal.
	- If no modes are found, outputs 0.
- **Geometric Mean Output**: Outputs the geometric mean of the input voltages.
- **Product Output**: Outputs the product of all input voltages.
	- !!! WARNING !!! THis can produced extremely large voltages (e.g. quickly rises to greater than a million volts ), think about safety precautions!
- **Count Output**: Outputs the number of active channels in the input as an integer voltage.
	- !!! WARNING !!! The output range for this port is 0.0v to 16.0v.
- **Sum Output**: Outputs the sum of all input voltages.
- **Ascending Output**: Outputs the input voltages sorted in ascending order.
- **Distinct Output**: Outputs one of each distinct voltage from the input, ignoring very close values (+/- 0.0001v) as not distinct.

---

## License

This project is licensed under the MIT License. See the [license.txt](license.txt) file for more information.