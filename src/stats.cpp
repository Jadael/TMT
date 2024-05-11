#include "plugin.hpp"
#include "ports.hpp"

#define GRID_SNAP 10.16 // A 2hp grid in millimeters. 1 GRID_SNAP is just the right spacing for adjacent ports on the module

struct Timer {
	// There's probably something in dsp which could handle this better,
	// it was just easier to conceptualize as a simple "time since reset" which I can check however I want
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
	
	float time() {// Return seconds since timer start
		return timePassed;
	}
	
	bool check(float seconds) { // Return whether it's been at least <seconds> since the timer started
		return timePassed >= seconds;
	}
};

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
	
	Timer timeSinceUpdate;

	Stats() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		// Configure the toggle switch parameter with a label and a tooltip for additional detail
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Alt Mode");

		// Configure the main polyphonic input
		configInput(POLY_INPUT, "Polyphonic Input");
		inputInfos[POLY_INPUT]->description = "The main input for the module. Connect a polyphonic signal here to compute various statistics based on the voltages of each channel.";

		// Configure the outputs and their detailed descriptions
		configOutput(MEAN_OUTPUT, "Mean");
		outputInfos[MEAN_OUTPUT]->description = "Outputs the arithmetic mean (average) of the voltages connected to the 'Polyphonic Input'.\nThe mean is the sum of all input voltages divided by the number of channels.";

		configOutput(MEDIAN_OUTPUT, "Median");
		outputInfos[MEDIAN_OUTPUT]->description = "Outputs the median of the voltages connected to the 'Polyphonic Input'.\nThe median is the middle value that separates the higher half from the lower half of the data set.";

		configOutput(MODE_OUTPUT, "Mode(s)");
		outputInfos[MODE_OUTPUT]->description = "Outputs the mode(s) of the voltages connected to the 'Polyphonic Input'.\nThe mode is the value that appears most frequently in the data set.\nIf multiple values are equally frequent, all are considered modes and result will be polyphonic.";

		configOutput(GEOMETRIC_MEAN_OUTPUT, "Geometric Mean");
		outputInfos[GEOMETRIC_MEAN_OUTPUT]->description = "Outputs the geometric mean of the voltages connected to the 'Polyphonic Input'.\nThe geometric mean is the nth root of the product of all the input voltages, where n is the number of channels.\nNote, because this involves multiplication, if ANY input channel is 0, result will be zero.";

		configOutput(PRODUCT_OUTPUT, "Product");
		outputInfos[PRODUCT_OUTPUT]->description = "Outputs the product of the voltages connected to the 'Polyphonic Input'.\nThis is the result of multiplying all the input voltages together.\nNote, because this involves multiplication, if ANY input channel is 0, result will be zero.\n!!! WARNING: THIS CAN OUTPUT EXTREMELY LARGE VOLTAGES !!!";

		configOutput(COUNT_OUTPUT, "Count");
		outputInfos[COUNT_OUTPUT]->description = "Outputs the number of active channels in the 'Polyphonic Input'.\nThis is the count of how many input channels are currently providing a signal, where 1 channel is 1.0v, 16 channels is 16.0v.\n!!! WARNING: THIS CAN OUTPUT >10v !!!";

		configOutput(SUM_OUTPUT, "Sum");
		outputInfos[SUM_OUTPUT]->description = "Outputs the sum of all the voltages connected to the 'Polyphonic Input'.\nThis is the total of all input voltages added together.\n!!! WARNING: THIS CAN OUTPUT LARGE VOLTAGES !!!";

		configOutput(ASCENDING_OUTPUT, "Ascending");
		outputInfos[ASCENDING_OUTPUT]->description = "Outputs the voltages from the 'Polyphonic Input' sorted in ascending order.\nEach channel corresponds to one of the input voltages, sorted from smallest to largest.";

		configOutput(DISTINCT_OUTPUT, "Distinct");
		outputInfos[DISTINCT_OUTPUT]->description = "Outputs the distinct voltages from the 'Polyphonic Input', removing any duplicates.\nVoltages are considered duplicates if they are within 0.001 volts.";

		// Reset the timer for the initial state
		timeSinceUpdate.reset();
	}
	
	bool isDistinct(float newVoltage, float lastVoltage, float tolerance = 0.001) {
		if (newVoltage == lastVoltage) return false;
		return fabs(newVoltage - lastVoltage) > tolerance;
	}
	
	
	void process(const ProcessArgs& args) override {
		timeSinceUpdate.update(args.sampleTime); // Advance the timer
		
		if (!inputs[POLY_INPUT].isConnected()) {
			return; // Break early if there's no input
		}
		
		// Check the timer/toggle for processing rate
		if (!timeSinceUpdate.check(0.01f) && params[TOGGLE_SWITCH].getValue() < 0.5f) {
			return; // Break early if it's been less than 10ms, unless we're in Alt mode
		}
		
		timeSinceUpdate.reset(); // Reset the timer, since we're about to re-process all the stats
		
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
		
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*1.5)), module, Stats::POLY_INPUT));
		// The input will be the upper leftmost port, and I think we could fit a column of 10 below it, with either labels to the right, or a second column of outputs if we can think of that many different stats to output
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*3)), module, Stats::MEAN_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*4)), module, Stats::MEDIAN_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*5)), module, Stats::MODE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*6)), module, Stats::GEOMETRIC_MEAN_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*7)), module, Stats::PRODUCT_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*8)), module, Stats::COUNT_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*9)), module, Stats::SUM_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*10)), module, Stats::ASCENDING_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*11)), module, Stats::DISTINCT_OUTPUT));
	}
};


Model* modelStats = createModel<Stats, StatsWidget>("Stats");