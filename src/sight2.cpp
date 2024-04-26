#include "plugin.hpp"
#include "ports.hpp"
#include <deque>
#include <vector>
#include <cmath>

struct Sight2 : Module {
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

    Sight2() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configInput(VOLTAGE_INPUT, "Voltage");
    }

    void process(const ProcessArgs& args) override {
    }
};

struct Sight2Scope : LightWidget {
    Sight2* module;
    Vec topLeft = Vec(15, 10);
    Vec bottomRight = Vec(240, 260);
    int bufferSize = 512;
    std::vector<std::vector<float>> voltageBuffers;
    int channels = 1;

    Sight2Scope(Sight2* module) {
        this->module = module;
        box.size = Vec(bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
        voltageBuffers.resize(16, std::vector<float>(bufferSize, 0.f));
    }

    void advanceBuffer(int channel, float inputVoltage) {
        if (channel >= (int)voltageBuffers.size())
            return;

        std::vector<float>& buffer = voltageBuffers[channel];

        // New strategy for compression: Remove one sample based on some logic
        //int skipIndex = (int)(bufferSize * 0.99); // Arbitrary skip index logic; can be improved
		int skipIndex = rand() % (bufferSize - 1);  // Random skip index

        // Rescale buffer content to fit in the reduced size, skipping the selected index
        std::vector<float> newBuffer(bufferSize - 1, 0.f);
        for (int i = 0, j = 0; i < bufferSize; i++) {
            if (i != skipIndex) {
                newBuffer[j++] = buffer[i];
            }
        }

        // Add the new sample at the end
        newBuffer.push_back(inputVoltage);

        // Update the original buffer with new content
        buffer = newBuffer;
    }
    
    void step() override {
        if (!module) return;

        channels = module->inputs[Sight2::VOLTAGE_INPUT].getChannels();
        for (int c = 0; c < channels; c++) {
            advanceBuffer(c, module->inputs[Sight2::VOLTAGE_INPUT].getVoltage(c));
        }
        LightWidget::step();
    }

    void drawLight(const DrawArgs& args) override {
        if (!module) return;

        nvgScissor(args.vg, RECT_ARGS(args.clipBox));
        for (int c = 0; c < channels; c++) {
            auto& buffer = voltageBuffers[c];
            for (int i = 0; i < buffer.size(); i++) {
                float height = rescale(buffer[i], -10.f, 10.f, 0.f, box.size.y);
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, i * (box.size.x / buffer.size()), box.size.y - height, 0.75);
                nvgFillColor(args.vg, nvgRGBA(255, 215, 0, 255));
                nvgFill(args.vg);
                nvgClosePath(args.vg);
            }
        }
        nvgResetScissor(args.vg);
    }
};

struct Sight2Widget : ModuleWidget {
    Sight2Widget(Sight2* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/sight.svg")));
        
        Sight2Scope* diagram = new Sight2Scope(module);
        diagram->box.pos = Vec(15, 30);
        diagram->box.size = Vec(240, 260);
        addChild(diagram);

        addInput(createInputCentered<BrassPort>(mm2px(Vec(45.72, 112.842)), module, Sight2::VOLTAGE_INPUT));
    }
};

Model* modelSight2 = createModel<Sight2, Sight2Widget>("Sight2");