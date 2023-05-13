#include "plugin.hpp"
#include "ports.hpp"
#include <random>

// A description of the module!

struct Seed : Module {
	enum ParamId {
		TOGGLE_SWITCH,
		PARAMS_LEN
	};
	enum InputId {
		SEED_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		POLY_OUT_OUTPUT,
		OUT01_OUTPUT,
		OUT02_OUTPUT,
		OUT03_OUTPUT,
		OUT04_OUTPUT,
		OUT05_OUTPUT,
		OUT06_OUTPUT,
		OUT07_OUTPUT,
		OUT08_OUTPUT,
		OUT09_OUTPUT,
		OUT10_OUTPUT,
		OUT11_OUTPUT,
		OUT12_OUTPUT,
		OUT13_OUTPUT,
		OUT14_OUTPUT,
		OUT15_OUTPUT,
		OUT16_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Seed() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Snap voltages to 0v/10v");
		configInput(SEED_INPUT, "Seed");
		configOutput(POLY_OUT_OUTPUT, "16 random voltages from seed");
		configOutput(OUT01_OUTPUT, "1st random voltage from seed");
		configOutput(OUT09_OUTPUT, "9th random voltage from seed");
		configOutput(OUT02_OUTPUT, "2nd random voltage from seed");
		configOutput(OUT10_OUTPUT, "10th random voltage from seed");
		configOutput(OUT03_OUTPUT, "3rd random voltage from seed");
		configOutput(OUT11_OUTPUT, "11th random voltage from seed");
		configOutput(OUT04_OUTPUT, "4th random voltage from seed");
		configOutput(OUT12_OUTPUT, "12th random voltage from seed");
		configOutput(OUT05_OUTPUT, "5th random voltage from seed");
		configOutput(OUT13_OUTPUT, "13th random voltage from seed");
		configOutput(OUT06_OUTPUT, "6th random voltage from seed");
		configOutput(OUT14_OUTPUT, "14th random voltage from seed");
		configOutput(OUT07_OUTPUT, "7th random voltage from seed");
		configOutput(OUT15_OUTPUT, "15th random voltage from seed");
		configOutput(OUT08_OUTPUT, "8th random voltage from seed");
		configOutput(OUT16_OUTPUT, "16th random voltage from seed");
	}
	
	float lastSeed = -1.0f;
	std::mt19937 gen;
	std::uniform_real_distribution<float> dist{0.0f, 10.0f};
	float currentSeed = dist(gen);
	float updateCounter = 0.0f;
	const float updateRate = 100.0f;
		
	void process(const ProcessArgs& args) override {
		currentSeed = inputs[SEED_INPUT].isConnected() ? inputs[SEED_INPUT].getVoltage() * 10000 : currentSeed;
		
		updateCounter += args.sampleTime;
		
		if (updateCounter >= (1.0f / updateRate)) {
			updateCounter -= (1.0f / updateRate);
			
			//bool toggleMode = params[TOGGLE_MODE_PARAM].getValue() > 0.5f;

			gen.seed(static_cast<unsigned int>(currentSeed));
			
			outputs[POLY_OUT_OUTPUT].setChannels(16);
			
			float randomNumbers[16];
			for (int i = 0; i < 16; ++i) {
				int outputIndex = OUT01_OUTPUT + i;
				if (params[TOGGLE_SWITCH].getValue() < 0.5f) { // Uniform distribution when the toggle switch is off.
					randomNumbers[i] = dist(gen);
				} else {
					randomNumbers[i] = (gen() % 2 == 0) ? 0.0f : 10.0f;
				}
				outputs[outputIndex].setVoltage(randomNumbers[i]);
			}
			
			for (int i = 0; i < 16; ++i) {
				outputs[POLY_OUT_OUTPUT].setVoltage(randomNumbers[i], i);
			}
			
			lastSeed = currentSeed;
		}
	}

};

struct SeedWidget : ModuleWidget {
	SeedWidget(Seed* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/seed.svg")));
		
		addParam(createParamCentered<BrassToggle>(mm2px(Vec(15, 6)), module, Seed::TOGGLE_SWITCH));
		
		addInput(createInputCentered<BrassPort>(mm2px(Vec(11.331, 14.933)), module, Seed::SEED_INPUT));
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 14.933)), module, Seed::POLY_OUT_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 27.166)), module, Seed::OUT01_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 27.166)), module, Seed::OUT09_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 39.399)), module, Seed::OUT02_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 39.399)), module, Seed::OUT10_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 51.632)), module, Seed::OUT03_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 51.632)), module, Seed::OUT11_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 63.866)), module, Seed::OUT04_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 63.866)), module, Seed::OUT12_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 76.099)), module, Seed::OUT05_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 76.099)), module, Seed::OUT13_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 88.332)), module, Seed::OUT06_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 88.332)), module, Seed::OUT14_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 100.566)), module, Seed::OUT07_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 100.566)), module, Seed::OUT15_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 112.799)), module, Seed::OUT08_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 112.799)), module, Seed::OUT16_OUTPUT));
	}
};


Model* modelSeed = createModel<Seed, SeedWidget>("Seed");