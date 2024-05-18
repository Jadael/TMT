#include "plugin.hpp"
#include "ports.hpp"

#define GRID_SNAP 10.16 // A 2hp grid in millimeters. 1 GRID_SNAP is just the right spacing for adjacent ports on the module

struct Timer {
    float timePassed = 0.0f;  // Time since timer start in seconds

    void reset() { // Start timer at 0 on resets
        timePassed = 0.0f; 
    }

    void set(float seconds) { // Set the timer to something specific
        timePassed = seconds;
    }

    void update(float deltaTime) { // Update the timer and check if the period has expired
        timePassed += deltaTime;
    }

    float time() { // Return seconds since timer start
        return timePassed;
    }

    bool check(float seconds) { // Return whether it's been at least <seconds> since the timer started
        return timePassed >= seconds;
    }
};

struct Spine : Module {
    enum ParamId {
        TOGGLE_SWITCH,
        PARAMS_LEN
    };
    enum InputId {
        X_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        ADD_1V_OUTPUT,
        SUB_1V_OUTPUT,
        ADD_5V_OUTPUT,
        SUB_5V_OUTPUT,
        ADD_10V_OUTPUT,
        SUB_10V_OUTPUT,
        INVERSE_OUTPUT,
        REVERSE_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        LIGHTS_LEN
    };

    Timer timeSinceUpdate;

    Spine() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        // Configure the toggle switch parameter with a label and a tooltip for additional detail
        configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Alt Mode: Process at audio rate (CPU heavy)");

        // Configure the main polyphonic input
        configInput(X_INPUT, "x");

        // Configure the outputs and their detailed descriptions
        configOutput(ADD_1V_OUTPUT, "x + 1v");
        configOutput(SUB_1V_OUTPUT, "x - 1v");
        configOutput(ADD_5V_OUTPUT, "x + 5v");
        configOutput(SUB_5V_OUTPUT, "x - 5v");
        configOutput(ADD_10V_OUTPUT, "x + 10v");
        configOutput(SUB_10V_OUTPUT, "x - 10v");
        configOutput(INVERSE_OUTPUT, "x * -1"); 
        configOutput(REVERSE_OUTPUT, "10v - x"); 

        // Reset the timer for the initial state
        timeSinceUpdate.reset();
    }

    void process(const ProcessArgs& args) override {
        timeSinceUpdate.update(args.sampleTime); // Advance the timer

        // Check the timer/toggle for processing rate
        if (!timeSinceUpdate.check(0.01f) && params[TOGGLE_SWITCH].getValue() < 0.5f) {
            return; // Break early if it's been less than 10ms, unless we're in Alt mode
        }

        timeSinceUpdate.reset(); // Reset the timer, since we're about to re-process all the stats

        // Get the input voltage, and how many channels it is
        int inputChannels = std::max(inputs[X_INPUT].getChannels(),1);
        
        // Update the number of channels on all outputs to match the input
        for (int i = 0; i < OUTPUTS_LEN; i++) {
            outputs[i].setChannels(inputChannels);
        }
        
        // Process each channel separately
        for (int c = 0; c < inputChannels; c++) {
          float x = 0.f;
          if (inputs[X_INPUT].isConnected()) {
            x = inputs[X_INPUT].getVoltage(c);
          }
          // ADD_1V_OUTPUT: x + 1v
          outputs[ADD_1V_OUTPUT].setVoltage(x + 1.0f, c);
          // SUB_1V_OUTPUT: x - 1v
          outputs[SUB_1V_OUTPUT].setVoltage(x - 1.0f, c);
          // ADD_5V_OUTPUT: x + 5v
          outputs[ADD_5V_OUTPUT].setVoltage(x + 5.0f, c);
          // SUB_5V_OUTPUT: x - 5v
          outputs[SUB_5V_OUTPUT].setVoltage(x - 5.0f, c);
          // ADD_10V_OUTPUT: x + 10v
          outputs[ADD_10V_OUTPUT].setVoltage(x + 10.0f, c);
          // SUB_10V_OUTPUT: x - 10v
          outputs[SUB_10V_OUTPUT].setVoltage(x - 10.0f, c);
          // INVERSE_OUTPUT: x * -1
          outputs[INVERSE_OUTPUT].setVoltage(x * -1.0f, c);
          // REVERSE_OUTPUT: 10v - x
          outputs[REVERSE_OUTPUT].setVoltage(10.0f - x, c);
        }
    }
};

struct SpineWidget : ModuleWidget {
    SpineWidget(Spine* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/spine.svg")));

        addParam(createParamCentered<BrassToggle>(mm2px(Vec(15, 6)), module, Spine::TOGGLE_SWITCH));

        addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP * 1, GRID_SNAP * 1.5)), module, Spine::X_INPUT));

        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 1, GRID_SNAP * 4)), module, Spine::ADD_1V_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 1, GRID_SNAP * 5)), module, Spine::SUB_1V_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 1, GRID_SNAP * 6)), module, Spine::ADD_5V_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 1, GRID_SNAP * 7)), module, Spine::SUB_5V_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 1, GRID_SNAP * 8)), module, Spine::ADD_10V_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 1, GRID_SNAP * 9)), module, Spine::SUB_10V_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 1, GRID_SNAP * 10)), module, Spine::INVERSE_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 1, GRID_SNAP * 11)), module, Spine::REVERSE_OUTPUT));
    }
};

Model* modelSpine = createModel<Spine, SpineWidget>("Spine");