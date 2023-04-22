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
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Average current and next step");
		configInput(POLY_SEQUENCE_INPUT, "Source polyphonic signal to step through");
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(LENGTH_INPUT, "Sequence length (0v: one step, 10v: all steps)");
		configOutput(MONO_SEQUENCE_OUTPUT, "Mono sequence");
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

struct SequenceDisplay : Widget {
    Ouroboros* module;
    const Vec topLeft = Vec(10, 30);
    const Vec bottomRight = Vec(60, 120);

    SequenceDisplay(Ouroboros* module) {
        this->module = module;
        box.size = Vec(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
    }

    void draw(const DrawArgs& args) override {
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
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Ouroboros.svg")));
		
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