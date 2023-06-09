#include "plugin.hpp"
#include "ports.hpp"
#include <deque>
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
    const int bufferSize = 1024;
    std::deque<float> voltageBuffer;

    SightScope(Sight* module) {
        this->module = module;
        box.size = Vec(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
        voltageBuffer.resize(bufferSize, 0.f);
    }

    void advanceBuffer(float inputVoltage) {
        voltageBuffer.pop_back();
        voltageBuffer.push_front(inputVoltage);
    }

    float xPosition(float index) {
        // Use a logarithmic scaling factor to compress the waveform towards the "new signal" end
        float scalingFactor = std::log(index + 1) / std::log(bufferSize);
        return box.size.x - scalingFactor * box.size.x;
    }

    void drawLight(const DrawArgs& args) override {
        if (!module) {
            return;
        }

        // Advance Voltage buffer one sample
        advanceBuffer(module->inputs[Sight::VOLTAGE_INPUT].getVoltage());
		
		// Draw dotted lines at 1 volt increments
		nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 128));
		nvgStrokeWidth(args.vg, 1);
		for (float voltage = -10.f; voltage <= 10.f; voltage += 1.f) {
			float y = box.size.y - rescale(voltage, -10.f, 10.f, 0.f, box.size.y);
			for (float x = 0.f; x < box.size.x; x += 10.f) {
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, x, y);
				nvgLineTo(args.vg, x + 5, y);
				nvgStroke(args.vg);
				nvgClosePath(args.vg);
			}
		}
		
		// Draw a horizontal black line at the zero crossing
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, box.size.y / 2);
		nvgLineTo(args.vg, box.size.x, box.size.y / 2);
		nvgStrokeColor(args.vg, nvgRGBA(0, 0, 0, 255));
		nvgStrokeWidth(args.vg, 1);
		nvgStroke(args.vg);
		nvgClosePath(args.vg);

        // Draw the input voltages in the buffer
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, xPosition(0), box.size.y - rescale(voltageBuffer[0], -10.f, 10.f, 0.f, box.size.y));
        for (int i = 1; i < bufferSize; i++) {
            float height = rescale(voltageBuffer[i], -10.f, 10.f, 0.f, box.size.y);
            nvgLineTo(args.vg, xPosition(i), box.size.y - height);
        }
        nvgStrokeColor(args.vg, nvgRGBA(254, 201, 1, 255));
        nvgStrokeWidth(args.vg, 1);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
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
