# T's Musical Tools

## License

This project is licensed under the MIT License. See the [license.txt](license.txt) file for more information.

# Shuffle
Shuffle is a polyphonic VCV Rack module that shuffles a set of incoming polyphonic voltages and outputs the reordered voltages. The module allows you to control the number of output channels and provides a trigger input to initiate the shuffle. The shuffle is deterministic, meaning that the same seed will always give the same shuffle. If no seed is provided, the module will generate and use a new (unpredictable) seed on every trigger input.

## Features
- Polyphonic input and output
- Control over the number of output channels
- Trigger input to initiate shuffle
- Seed input to control deterministic randomness of shuffle
- Outputs track the inputs as they change, while retaining the current shuffle

## Inputs & Outputs

### Shuffle Trigger
Triggers the shuffle of the polyphonic voltages.

### Poly In (optional)
Polyphonic voltages to be shuffled. If no cable is connected, a one-octave chromatic scale will be used as the default input.

### Seed (optional)
Provide a random seed for the shuffle. The same seed will always gives the same shuffle. If no cable is connected, a new (unpredictable) seed will be generated on every trigger input.

### Number of Output Channels (optional)
Controls the number of output channels (one channel at 0.00 volts, half the input channels at 5.00 volts, all input channels at 10.00 volts). If no cable is connected, all input channels will be output.

### Poly Out
The reordered polyphonic voltages.

## Usage
- Connect a set of polyphonic voltages to the Poly In input (optional).
- Connect a trigger input to the Shuffle Trigger input.
- Optionally, connect a seed input to the Seed input to control the deterministic randomness of the shuffle.
- Optionally, connect a voltage source to the Number of Output Channels input to control the number of output channels.
- The reordered polyphonic voltages will be available at the Poly Out output.

---

# Calendar

Calendar is a VCV Rack module that provides a set of very slow LFOs representing the progress through various major time/calendar units. The module outputs voltages ranging from 0 to 10V, where each voltage corresponds to the current "progress" through the respective time unit.

## Features

- Outputs for seconds, minutes, hours, days, months, quarters, and years
- Voltages range from 0 to 10V
- Can be used for slowly evolving modulation, generative patching, or time-based sequencing

### Outputs
Each time unit has a row of five related outputs:

- smooth - This output represents a smooth, continuous voltage ranging from 0 to 10V. It is calculated by taking the current progress (a value between 0 and 1) of the time unit and scaling it by a factor of 10. As the progress of the time unit increases, the output voltage increases linearly.
- stepped - This output is a stepped version of the smooth output. It divides the progress into equal intervals based on how that time unit is usually subdivided. This output can be useful for creating distinct voltage steps or quantized changes in a patch.
	- Second: Four steps
	- Minute: 60 steps (i.e. seconds)
	- Hour: 60 steps (i.e. minutes)
	- Day: 24 steps (i.e. hours)
	- Week: 7 steps (i.e. days)
	- Month: 4 steps (i.e. weeks)
	- Quarter: 3 steps (i.e. months)
	- Year: 12 steps (i.e. months)
- trigger - This output sends a 10V trigger signal at the beginning of each time unit cycle. The trigger is activated when the progress is less than or equal to 1/10th of the reciprocal of the time unit's duration. For instance, in the case of hours, the trigger would be activated when the progress is less than or equal to 1/240 (0.1/24). This output is useful for triggering events or resets at the start of each time unit.
- gate - This output provides a 10V gate signal for the first half of the time unit cycle and 0V for the second half. The gate is active (10V) when the progress is less than 0.5 (50% of the time unit) and inactive (0V) otherwise. This output can be used to create sustained events or behaviors during the first half of each time unit cycle.
- reverseGate - This output is the inverse of the gate output. It provides a 0V signal for the first half of the time unit cycle and a 10V signal for the second half. The reverse gate is active (10V) when the progress is greater than or equal to 0.5 (50% of the time unit) and inactive (0V) otherwise. This output can be used to create sustained events or behaviors during the second half of each time unit cycle.

## Usage

1. Add the Calendar Progress module to your VCV Rack patch.
2. Connect the desired time unit outputs (e.g., seconds, minutes, hours, days, months, or years) to the inputs of other modules in your patch.
3. Observe the slow voltage changes as the module represents the progress of the connected time units.

---

# Seed

Seed is a random voltage generator with 16 outputs and a polyphonic output, providing random values based on a given input voltage as the seed. When the input seed changes, the module generates 16 random numbers normalized between 0.0V and 10.0V. These random values are output individually across the 16 numbered outputs and as a polyphonic signal through the polyphonic output.

## Features

- 16 individual random voltage outputs
- Polyphonic output with 16 channels
- Random voltage generation based on the input seed
- Regenerates random values only when the seed changes

## Inputs

- **Seed**: Input voltage used as the seed for random number generation. If no input is connected, the seed defaults to 0.0V.

## Outputs

- **Poly Out**: Polyphonic output with 16 channels containing random voltages.
- **Out 1 - Out 16**: Individual outputs for each of the 16 random voltages generated.
