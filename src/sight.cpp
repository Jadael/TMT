#include "plugin.hpp"
#include "ports.hpp"
#include <deque>
#include <vector>
#include <cmath>

struct Sight : Module {
    enum ParamId {
        PARAMS_LEN
    };
    enum InputId {
        VOLTAGE_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        OUTPUTS_LEN
    };
    enum LightId {
        LIGHTS_LEN
    };

    Sight() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configInput(VOLTAGE_INPUT, "Voltage");
    }

    void process(const ProcessArgs& args) override {
    }
};

struct SightScope : LightWidget {
    Sight* module;
    const Vec topLeft = Vec(15, 10);
    const Vec bottomRight = Vec(240, 260);
    const int bufferSize = 512;
    std::vector<std::deque<float>> voltageBuffers;
	int channels = 1;

    SightScope(Sight* module) {
        this->module = module;
        box.size = Vec(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
        voltageBuffers.resize(16, std::deque<float>(bufferSize, 0.f));
    }

	void advanceBuffer(int channel, float inputVoltage) {
		if (channel >= (int)voltageBuffers.size())
			return;

		auto& buffer = voltageBuffers[channel];

		// Generate a biased random index
		float randomValue = (float)rand() / RAND_MAX;  // Uniform random value between 0 and 1
		int discardIndex = (int)(bufferSize * randomValue);  // Choose an index

		// Shift all elements past the discard index one position to the left
		// TODO: CHange the buffer to something that lets us just MOVE a random column to the end to be overwritten, to avoid all the shifting.
		for (int i = discardIndex; i < bufferSize - 1; i++) {
			buffer[i] = buffer[i + 1];
		}

		// Place the new sample at the end of the buffer
		buffer[bufferSize - 1] = inputVoltage;
	}
	
	void step() override {
        if (!module) {
            return;
        }
		// Handle polyphonic inputs
		channels = module->inputs[Sight::VOLTAGE_INPUT].getChannels(); // double check channels
		for (int c = 0; c < channels; c++) {
			advanceBuffer(c, module->inputs[Sight::VOLTAGE_INPUT].getVoltage(c));
		}
		LightWidget::step();
	}

    void drawLight(const DrawArgs& args) override {
        if (!module) {
            return;
        }
		channels = module->inputs[Sight::VOLTAGE_INPUT].getChannels(); // double check channels
        nvgScissor(args.vg, RECT_ARGS(args.clipBox));
        for (int c = 0; c < channels; c++) {
            auto& buffer = voltageBuffers[c];
            for (int i = 0; i < bufferSize; i++) {
                float height = rescale(buffer[i], -10.f, 10.f, 0.f, box.size.y);
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, i * (box.size.x / bufferSize), box.size.y - height, 0.75);
                nvgFillColor(args.vg, nvgRGBA(255, 215, 0, 255)); // Color modified per channel if desired
                nvgFill(args.vg);
                nvgClosePath(args.vg);
            }
        }
        nvgResetScissor(args.vg);
    }

};

struct SightWidget : ModuleWidget {
    SightWidget(Sight* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/sight.svg")));
        
        SightScope* diagram = new SightScope(module);
        diagram->box.pos = Vec(15, 30);
        diagram->box.size = Vec(240, 260);
        addChild(diagram);

        addInput(createInputCentered<BrassPort>(mm2px(Vec(45.72, 112.842)), module, Sight::VOLTAGE_INPUT));
    }
};

Model* modelSight = createModel<Sight, SightWidget>("Sight");