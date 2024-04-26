#include "plugin.hpp"
#include "ports.hpp"

#define GRID_SNAP 10.16 // A 2hp grid in millimeters. 1 GRID_SNAP is just the right spacing for adjacent ports on the module

struct Stats : Module {
	enum ParamId {
		TOGGLE_SWITCH,
		PARAMS_LEN
	};
	enum InputId {
		POLY_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		MEAN_OUTPUT,
		MEDIAN_OUTPUT,
		MODE_OUTPUT,
		GEOMETRIC_MEAN_OUTPUT,
		PRODUCT_OUTPUT,
		COUNT_OUTPUT,
		SUM_OUTPUT,
		ASCENDING_OUTPUT,
		DISTINCT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Stats() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Limit Process Rate to 10ms");
		configInput(POLY_INPUT, "Polyphonic Input");
		configOutput(MEAN_OUTPUT, "Average (mean)");
		configOutput(MEDIAN_OUTPUT, "Median");
		configOutput(MODE_OUTPUT, "Mode");
		configOutput(GEOMETRIC_MEAN_OUTPUT, "Geometric Mean");
		configOutput(PRODUCT_OUTPUT, "Product");
		configOutput(COUNT_OUTPUT, "Count");
		configOutput(SUM_OUTPUT, "Sum");
		configOutput(ASCENDING_OUTPUT, "Ascending");
		configOutput(DISTINCT_OUTPUT, "Distinct");
	}
	
	bool isDistinct(float newVoltage, float lastVoltage, float tolerance = 0.001) {
		if (newVoltage == lastVoltage) return false;
		return fabs(newVoltage - lastVoltage) > tolerance;
	}
	
	
	void process(const ProcessArgs& args) override {
		
		if (!inputs[POLY_INPUT].isConnected()) { // Break early if there's no input
			return;
		}
		
		int numChannels = inputs[POLY_INPUT].getChannels();
		outputs[COUNT_OUTPUT].setVoltage((float)numChannels);
		
		std::vector<float> voltages;
		float sum = 0.0f;
		float product = 1.0f;

		for (int i = 0; i < numChannels; i++) {
			float voltage = inputs[POLY_INPUT].getVoltage(i);
			sum += voltage;
			product *= voltage;
			voltages.push_back(voltage);
		}
		
		outputs[SUM_OUTPUT].setVoltage(sum);
		outputs[PRODUCT_OUTPUT].setVoltage(product);

		if (outputs[MEAN_OUTPUT].isConnected()) {
			float average = (numChannels > 0) ? sum / numChannels : 0.0f;
			outputs[MEAN_OUTPUT].setVoltage(average);
		}
		
		if (outputs[GEOMETRIC_MEAN_OUTPUT].isConnected()) {
			float geometricMean = (numChannels > 0) ? pow(product, 1.0f / numChannels) : 0.0f;
			outputs[GEOMETRIC_MEAN_OUTPUT].setVoltage(geometricMean);
		}
		
		if (outputs[MODE_OUTPUT].isConnected()
			|| outputs[MEDIAN_OUTPUT].isConnected()
			|| outputs[ASCENDING_OUTPUT].isConnected()
			|| outputs[DISTINCT_OUTPUT].isConnected()
			) {
			
			// All need a sorted list
			std::sort(voltages.begin(), voltages.end());
			
			// Ascending sort
			if (outputs[ASCENDING_OUTPUT].isConnected()) {
				outputs[ASCENDING_OUTPUT].setChannels(numChannels);
				
				for (int i = 0; i < numChannels; i++) {
					outputs[ASCENDING_OUTPUT].setVoltage(voltages[i], i);
				}
			}
			
			// Distinct
			if (outputs[DISTINCT_OUTPUT].isConnected()) {
				std::vector<float> distinctVoltages;
				
				// Start with the first value, since we're always going to include it anyway
				float lastVoltage = voltages[0];
				distinctVoltages.push_back(lastVoltage);
				
				for (float voltage : voltages) {
					if (isDistinct(voltage,lastVoltage)) {
						distinctVoltages.push_back(voltage);
						lastVoltage = voltage;
					}
				}
				outputs[DISTINCT_OUTPUT].setChannels(distinctVoltages.size());
				for (int i = 0; i < (int)distinctVoltages.size(); i++) {
					outputs[DISTINCT_OUTPUT].setVoltage(distinctVoltages[i], i);
				}
			}

			// Median
			if (outputs[MEDIAN_OUTPUT].isConnected()) {
				float median = (numChannels % 2 == 0) ?
					(voltages[numChannels / 2 - 1] + voltages[numChannels / 2]) / 2.0f :
					voltages[numChannels / 2];
				outputs[MEDIAN_OUTPUT].setVoltage(median);
			}

			// Mode
			if (outputs[MODE_OUTPUT].isConnected()) {
				float currentVoltage = voltages[0];
				int maxCount = 0;
				int currentCount = 1;
				std::vector<float> modes;  // To store all modes

				for (int i = 1; i < numChannels; i++) {
					if (voltages[i] == currentVoltage) {
						currentCount++;
					} else {
						if (currentCount > maxCount && currentCount > 1) {
							modes.clear();
							maxCount = currentCount;
							modes.push_back(currentVoltage);
						} else if (currentCount == maxCount && currentCount > 1) {
							modes.push_back(currentVoltage);
						}
						currentVoltage = voltages[i];
						currentCount = 1;
					}
				}
				// Check the last group of values
				if (currentCount > maxCount && currentCount > 1) {
					modes.clear();
					modes.push_back(currentVoltage);
				} else if (currentCount == maxCount && currentCount > 1) {
					modes.push_back(currentVoltage);
				}

				// Set the modes as polyphonic output
				int modeChannels = modes.size();
				outputs[MODE_OUTPUT].setChannels(modeChannels);
				for (int i = 0; i < modeChannels; i++) {
					outputs[MODE_OUTPUT].setVoltage(modes[i], i);
				}
			}
		}
	}
};

struct StatsWidget : ModuleWidget {
	StatsWidget(Stats* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/stats.svg")));
		
		addParam(createParamCentered<BrassToggle>(mm2px(Vec(15, 6)), module, Stats::TOGGLE_SWITCH));
		// Right now this doesn't do anything, but presumably we'll encounter something fun and esoteric for this module's "B Side" to do
		
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*1.5)), module, Stats::POLY_INPUT));
		// The input will be the upper leftmost port, and I think we could fit a column of 10 below it, with either labels to the right, or a second column of outputs if we can think of that many different stats to output
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*2.5)), module, Stats::MEAN_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*3.5)), module, Stats::MEDIAN_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*4.5)), module, Stats::MODE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*5.5)), module, Stats::GEOMETRIC_MEAN_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*6.5)), module, Stats::PRODUCT_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*7.5)), module, Stats::COUNT_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*8.5)), module, Stats::SUM_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*9.5)), module, Stats::ASCENDING_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*10.5)), module, Stats::DISTINCT_OUTPUT));
	}
};


Model* modelStats = createModel<Stats, StatsWidget>("Stats");