#include "plugin.hpp"
#include "ports.hpp"

#define GRID_SNAP 10.16 // A 2hp grid in millimeters. 1 GRID_SNAP is just the right spacing for adjacent ports on the module

struct Blankt : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Blankt() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
	}
	
	void process(const ProcessArgs& args) override {}
};

struct BlanktWidget : ModuleWidget {
	BlanktWidget(Blankt* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/blank.svg")));
	}
};


Model* modelBlankt = createModel<Blankt, BlanktWidget>("Blankt");