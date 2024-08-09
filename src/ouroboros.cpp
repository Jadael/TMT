/*
T's Musical Tools (TMT) - A collection of esoteric modules for VCV Rack, focused on manipulating RNG and polyphonic signals.
Copyright (C) 2024  T

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "plugin.hpp"
#include "ports.hpp"

struct Ouroboros : Module {
	enum ParamId {
		TOGGLE_SWITCH,
		PARAMS_LEN
	};
	enum InputId {
		POLY_SEQUENCE_INPUT,
		CLOCK_INPUT,
		RESET_INPUT,
		LENGTH_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		MONO_SEQUENCE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Ouroboros() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		// Toggle Switch Parameter
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Alt Mode: Output average of current and next step");

		// Polyphonic Sequence Input
		configInput(POLY_SEQUENCE_INPUT, "Polyphonic Sequence Input");
		inputInfos[POLY_SEQUENCE_INPUT]->description = "- This polyphonic input accepts multiple channels, each representing a step in the sequence.\n- The module steps through these channels based on the clock input.";

		// Clock Input
		configInput(CLOCK_INPUT, "Clock Input");
		inputInfos[CLOCK_INPUT]->description = "- This input expects a clock signal.\n- On each rising edge of this signal, the module advances to the next step in the sequence.";

		// Reset Input
		configInput(RESET_INPUT, "Reset Input");
		inputInfos[RESET_INPUT]->description = "- A rising edge on this input resets the sequence to the first step.\n- If a rising edge is received while the clock input is high, the reset will occur on the next clock's rising edge.";

		// Sequence Length Input
		configInput(LENGTH_INPUT, "Sequence Length Input");
		inputInfos[LENGTH_INPUT]->description = "- This input controls the number of active steps in the sequence.\n- A voltage of 0V means only the first step is active.\n- A voltage of 10V means ALL connected channels.\n- Intermediate voltages scale linearly between 1 and the number of connected channels.";

		// Mono Sequence Output
		configOutput(MONO_SEQUENCE_OUTPUT, "Mono Sequence Output");
		outputInfos[MONO_SEQUENCE_OUTPUT]->description = "- Outputs the voltage of the current step.\n- In Alt Mode, this output will be the average of the current and next step voltages.";
	}
	
    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger resetTrigger;
    int step = 0;
	bool waitForNextClock = false;
	//bool movingAverageMode = false;

	void process(const ProcessArgs& args) override {
		// Get inputs
		int channels = inputs[POLY_SEQUENCE_INPUT].getChannels();
		float clockInput = inputs[CLOCK_INPUT].getVoltage();
		float resetInput = inputs[RESET_INPUT].getVoltage();
		float lengthInput = inputs[LENGTH_INPUT].isConnected() ? inputs[LENGTH_INPUT].getVoltage() : 10.f;
		//movingAverageMode = params[TOGGLE_SWITCH].getValue() > 0.5f;

        // Reset the step if the reset input is high
        if (resetTrigger.process(rescale(resetInput, 0.1f, 2.f, 0.f, 1.f))) {
            if (clockInput <= 0.1f) { // If Clock input is "low"
                waitForNextClock = true; // Arm the reset
            } else { // If Clock input is "high"
                step = 0; // Reset immediately
                waitForNextClock = false; // de-arm the reset
            }
        }

		// Calculate the sequence length based on the length input voltage (0V - 10V)
		int length = std::max(1, (int)std::round(rescale(lengthInput, 0.f, 10.f, 1.f, (float)channels)));

        // Clock input processing
        if (clockTrigger.process(rescale(clockInput, 0.1f, 2.f, 0.f, 1.f))) {
            if (waitForNextClock) {
                step = 0; // Reset on the next clock's rising edge
                waitForNextClock = false; // de-arm the reset
            } else {
                step = (step + 1) % length; // Proceed to next step
            }
        }

		// Output the current step from the polyphonic input
		//float monoSignal = inputs[POLY_SEQUENCE_INPUT].getVoltage(step);
		//outputs[MONO_SEQUENCE_OUTPUT].setVoltage(monoSignal);
        if (params[TOGGLE_SWITCH].getValue() > 0.5f) {
            float currentStepVoltage = inputs[POLY_SEQUENCE_INPUT].getVoltage(step);
            float nextStepVoltage = inputs[POLY_SEQUENCE_INPUT].getVoltage((step + 1) % length);
            outputs[MONO_SEQUENCE_OUTPUT].setVoltage((currentStepVoltage + nextStepVoltage) / 2.0f);
        } else {
            outputs[MONO_SEQUENCE_OUTPUT].setVoltage(inputs[POLY_SEQUENCE_INPUT].getVoltage(step));
        }
	}
};

struct SequenceDisplay : LightWidget {
    Ouroboros* module;
    const Vec topLeft = Vec(10, 30);
    const Vec bottomRight = Vec(60, 120);

    SequenceDisplay(Ouroboros* module) {
        this->module = module;
        box.size = Vec(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
    }

    void drawLight(const DrawArgs& args) override {
		if (!module) {
			return;
		}
		
        nvgSave(args.vg);
        nvgTranslate(args.vg, topLeft.x, topLeft.y);
		
        // Draw a thin vertical line at the zero-crossing
        float zeroX = rescale(0.f, -10.f, 10.f, 0.f, box.size.x);
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, zeroX, 0);
        nvgLineTo(args.vg, zeroX, box.size.y);
        nvgStrokeColor(args.vg, nvgRGBA(254, 201, 1, 255));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);

        // Get the total number of rows (equal to the number of channels in the polyphonic input signal)
        int rows = module->inputs[Ouroboros::POLY_SEQUENCE_INPUT].getChannels();

        // Draw the yellow dot for each step
        for (int i = 0; i < rows; i++) {
            float voltage = module->inputs[Ouroboros::POLY_SEQUENCE_INPUT].getVoltage(i);
            float x = rescale(voltage, -10.f, 10.f, 0.f, box.size.x);
            float y = i * (box.size.y / rows) + (box.size.y / rows) / 2;

            nvgFillColor(args.vg, nvgRGBA(254, 201, 1, 255));
            nvgBeginPath(args.vg);
            nvgCircle(args.vg, x, y, 1.5f);
            nvgFill(args.vg);
        }

        // Draw a thin horizontal line bisecting the current step's dot
        float lineY = module->step * (box.size.y / rows) + (box.size.y / rows) / 2;
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0, lineY);
        nvgLineTo(args.vg, box.size.x, lineY);
        nvgStrokeColor(args.vg, nvgRGBA(254, 201, 1, 255));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);

        nvgRestore(args.vg);
    }
};

struct OuroborosWidget : ModuleWidget {
	OuroborosWidget(Ouroboros* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ouroboros.svg")));
		
		SequenceDisplay* diagram = new SequenceDisplay(module);
		diagram->box.pos = Vec(10, 10);
		diagram->box.size = Vec(50, 120);
		addChild(diagram);
		
		addParam(createParamCentered<BrassToggle>(mm2px(Vec(15, 6)), module, Ouroboros::TOGGLE_SWITCH));

		addInput(createInputCentered<BrassPort>(mm2px(Vec(8.625, 65.012)), module, Ouroboros::POLY_SEQUENCE_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(8.625, 76.981)), module, Ouroboros::CLOCK_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(8.625, 88.949)), module, Ouroboros::RESET_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(8.625, 100.918)), module, Ouroboros::LENGTH_INPUT));

		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(8.625, 112.887)), module, Ouroboros::MONO_SEQUENCE_OUTPUT));
	}
};


Model* modelOuroboros = createModel<Ouroboros, OuroborosWidget>("Ouroboros");
