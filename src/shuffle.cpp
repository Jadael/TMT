/*
T's Musical Tools (TMT) - A collection of esoteric modules for VCV Rack, focused on manipulating RNG and polyphonic signals.
Copyright (C) 2024  T

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "plugin.hpp"
#include "ports.hpp"
#include <array>

struct Shuffle : Module {
	enum ParamId {
		TOGGLE_SWITCH,
		PARAMS_LEN
	};
	enum InputId {
		TRIGGER_INPUT,
		POLYPHONIC_PITCH_INPUT,
		SEED_INPUT,
		OUTPUT_CHANNELS_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		REORDERED_PITCH_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Shuffle() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Alt Mode: Allow duplicate channels");

		configInput(TRIGGER_INPUT, "Shuffle Trigger");
		inputInfos[TRIGGER_INPUT]->description = "- Triggers a re-shuffle of input channels whenever a rising edge is detected on this input.";

		configInput(POLYPHONIC_PITCH_INPUT, "Polyphonic Input");
		inputInfos[POLYPHONIC_PITCH_INPUT]->description = "- The main polyphonic input for the pitch voltages that you want to shuffle. \n- Accepts up to 16 channels.";

		configInput(SEED_INPUT, "Seed");
		inputInfos[SEED_INPUT]->description = "- Optional input for a voltage that determines the random seed used for shuffling. \n- A stable voltage leads to a consistent shuffling pattern.";

		configInput(OUTPUT_CHANNELS_INPUT, "Output Channels Control");
		inputInfos[OUTPUT_CHANNELS_INPUT]->description = "- Controls the number of active output channels. \n- Expect voltages from 0V (one output) to 10V (all inputs are used as outputs).";

		configOutput(REORDERED_PITCH_OUTPUT, "Polyphonic Output");
		outputInfos[REORDERED_PITCH_OUTPUT]->description = "- The output after shuffling the input pitch voltages. \n- The number of active channels here is set by the 'Output Channels Control' input.";
	}
	
	dsp::SchmittTrigger trigger;
	std::random_device rd;
	std::mt19937 rng{rd()};
	std::array<int, 16> reorder = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
	size_t inputChannels = 12;
	size_t priorInputChannels = inputChannels;
	size_t outputChannels = inputChannels;
	size_t FinalSize = 12;
	std::array<float, 16> defaultVoltages = {0.f/12, 1.f/12, 2.f/12, 3.f/12, 4.f/12, 5.f/12, 6.f/12, 7.f/12, 8.f/12, 9.f/12, 10.f/12, 11.f/12, 12.f/12, 13.f/12, 14.f/12, 15.f/12};
	std::array<float, 16> inputVoltages = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
	size_t maxSize = 16;
	int seed;
	float germ;
	bool priorToggle;

	void process(const ProcessArgs& args) override {
		// Check for alt-mode toggle
		bool currentToggle = params[TOGGLE_SWITCH].getValue() > 0.5f;
		// Restrict the number of output channels based on the OUTPUT_CHANNELS_INPUT: 1 channel at 0.00 volts, half the input channels at 5.00 volts, all input channels at 10.00 volts, etc. (Clamp this input to 0.00v-10.00v)
		if (inputs[POLYPHONIC_PITCH_INPUT].isConnected()) {
			inputChannels = inputs[POLYPHONIC_PITCH_INPUT].getChannels();
			for (size_t i = 0; i < std::min(inputChannels, maxSize); i++) {
				inputVoltages[i] = inputs[POLYPHONIC_PITCH_INPUT].getVoltage(i);
			}
		} else {
			inputChannels = 12;
			inputVoltages = defaultVoltages;
		}
		
		if (inputs[OUTPUT_CHANNELS_INPUT].isConnected()) {
			float outputChannelsVoltage = clamp(inputs[OUTPUT_CHANNELS_INPUT].getVoltage(), 0.f, 10.f);
			outputChannels = std::round(rescale(outputChannelsVoltage, 0.f, 10.f, 1.f, inputChannels));
		} else {
			outputChannels = inputChannels;
		}
		
		if (trigger.process(inputs[TRIGGER_INPUT].getVoltage())) {
			// If there was a new trigger on TRIGGER_INPUT...
			// Randomly re-order the list; use the voltage from SEED_INPUT for a deterministic result, or use a random seed otherwise
			if (inputs[SEED_INPUT].isConnected()) {
				germ  = (inputs[SEED_INPUT].getVoltage() + 10.f) / 20.f;
				seed = static_cast<int>(germ * std::numeric_limits<int>::max());
				rng.seed(seed);
			} else {
				rng.seed(rd());
			}
			// Create a new shuffling using that seed, sized to current input
			// Reset the shuffle
			for (size_t i = 0; i < reorder.size(); i++) {
				reorder[i] = i;
			}
			// Randomize the shuffle
			if (params[TOGGLE_SWITCH].getValue() < 0.5f) {
				// Random "shuffle", with no duplicates
				if (inputChannels > 1) {
					// Shuffle within size of input channels
					std::shuffle(reorder.begin(), reorder.begin() + inputChannels, rng);
				}
				else {
					// If there's only one input channel, just pass it through
					reorder[0] = 0;
				}
			} else {
				// Random "selection", with potential duplicates
				for (size_t i = 0; i < reorder.size(); ++i) {
					int randomIndex = std::uniform_int_distribution<size_t>(0, inputChannels - 1)(rng);
					reorder[i] = randomIndex;
				}
			}
		}
		// And/Or, if the number of input channels changed, randomize again, re-using the current seed
		if (currentToggle != priorToggle or inputChannels != priorInputChannels) {
			// Detect and list inputs
			for (size_t i = 0; i < inputChannels; i++) {
				reorder[i] = i;
			}
			// Reset the random seed
			rng.seed(seed); // Set the seed to be used
			// Randomize the reordering
			if (params[TOGGLE_SWITCH].getValue() < 0.5f) {
				// Random "shuffle", with no duplicates
				std::shuffle(reorder.begin(), reorder.begin() + inputChannels, rng);
			} else {
				// Random "selection", with potential duplicates
				for (size_t i = 0; i < inputChannels; ++i) {
					int randomIndex = std::uniform_int_distribution<size_t>(0, inputChannels - 1)(rng);
					reorder[i] = randomIndex;
				}
			}
			priorToggle = currentToggle;
			priorInputChannels = inputChannels;
		}
		// Pass through the current incoming polyphonic signal, sorted according to the most recent "shuffling"
		// Constrained to "FinalSize" which is the lesser of the current Output Channels and current Reorder size
		FinalSize = std::min(inputChannels, outputChannels);
		
		for (size_t i = 0; i < FinalSize; i++) {
			outputs[REORDERED_PITCH_OUTPUT].setVoltage(inputVoltages[reorder[i]], i);
		}
		outputs[REORDERED_PITCH_OUTPUT].setChannels(FinalSize);
	}// End process()
};

struct ShuffleDiagram : LightWidget {
	Shuffle* module;

	ShuffleDiagram(Shuffle* module) : module(module) {}

	void drawLight(const DrawArgs& args) override {
		if (!module) return;
		// Set up the drawing context
		nvgSave(args.vg);
		nvgStrokeColor(args.vg, nvgRGBA(254, 201, 1, 255));
		nvgStrokeWidth(args.vg, 1.0);
		// Draw the input and output dots
		float xInput = 10;
		float xOutput = 60;
		float yOffset = 30;
		float ySpacing = 120.0 / module->inputChannels-1;
		//int CappedInput std::min(module->inputChannels,module->FinalSize);
		for (size_t i = 0; i < module->inputChannels; i++) {
			nvgFillColor(args.vg, nvgRGBA(254, 201, 1, 255));
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, xInput, yOffset + i * ySpacing, 1.5);
			nvgFill(args.vg);
			if (i < module->outputChannels) {
				nvgFillColor(args.vg, nvgRGBA(254, 201, 1, 255));
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, xOutput, yOffset + i * ySpacing, 1.5);
				nvgFill(args.vg);
			}
		}
		// Draw the lines connecting input and output channels (up to current FinalSize, to avoid drawing undefined outputs)
		for (size_t i = 0; i < module->FinalSize; i++) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, xInput, yOffset + module->reorder[i] * ySpacing);
			nvgLineTo(args.vg, xOutput, yOffset + i * ySpacing);
			nvgStroke(args.vg);
		}
		// Restore the drawing context
		nvgRestore(args.vg);
	}
};

struct ShuffleWidget : ModuleWidget {
	ShuffleWidget(Shuffle* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/shuffle.svg")));
		
		ShuffleDiagram* diagram = new ShuffleDiagram(module);
		diagram->box.pos = Vec(10, 10);
		diagram->box.size = Vec(50, 200);
		addChild(diagram);

		addParam(createParamCentered<BrassToggle>(mm2px(Vec(15, 6)), module, Shuffle::TOGGLE_SWITCH));

		addInput(createInputCentered<BrassPort>(mm2px(Vec(8.625, 65.012)), module, Shuffle::TRIGGER_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(8.625, 76.981)), module, Shuffle::POLYPHONIC_PITCH_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(8.625, 88.949)), module, Shuffle::SEED_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(8.625, 100.918)), module, Shuffle::OUTPUT_CHANNELS_INPUT));

		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(8.625, 112.887)), module, Shuffle::REORDERED_PITCH_OUTPUT));
	}
};

Model* modelShuffle = createModel<Shuffle, ShuffleWidget>("Shuffle");
