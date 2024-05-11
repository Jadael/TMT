#include "plugin.hpp"
#include "ports.hpp"

#define GRID_SNAP 10.16 // A 2hp grid in millimeters. 1 GRID_SNAP is just the right spacing for adjacent ports on the module

struct Timer {
	// There's probably something in dsp which could handle this better,
	// it was just easier to conceptualize as a simple "time since reset" which I can check however I want
	float timePassed = 0.0f;  // Time since timer start in seconds

	void reset() { timePassed = 0.0f; } // Set timer at 0 on resets

	void set(float seconds) { timePassed = seconds; } // Set the timer to something specific

	void update(float deltaTime) { timePassed += deltaTime; } // Update the timer and check if the period has expired

	float time() { return timePassed; } // Return seconds since timer start

	bool check(float seconds) { return timePassed >= seconds; } // Return whether it's been at least <seconds> since the timer started
};

struct Sort : Module {
	enum ParamId {
		TOGGLE_SWITCH,
		PARAMS_LEN
	};
	enum InputId {
		DATA_INPUT,
		SORT_INPUT,
		SELECT_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		PASSTHRU_OUTPUT,
		SORTED_OUTPUT,
		SELECTED_OUTPUT,
		SORTED_AND_SELECTED_OUTPUT,
		SELECTED_AND_SORTED_OUTPUT,
		ASCENDING_OUTPUT,
		DESCENDING_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
	
	Timer timeSinceUpdate;

	Sort() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Alt Mode: Process at audio rate (CPU heavy)");
		configInput(DATA_INPUT, "Polyphonic Data");
		configInput(SORT_INPUT, "Polyphonic Sort Key");
		inputInfos[SORT_INPUT]->description = "Data will be sorted based on the values of the sort key.";
		configInput(SELECT_INPUT, "Polyphonic Select Key");
		configOutput(PASSTHRU_OUTPUT, "Pass through data");
		configOutput(SORTED_OUTPUT, "Sorted data");
		configOutput(SELECTED_OUTPUT, "Selected data");
		configOutput(SORTED_AND_SELECTED_OUTPUT, "Sorted then selected data");
		configOutput(SELECTED_AND_SORTED_OUTPUT, "Selected then sorted data");
		configOutput(ASCENDING_OUTPUT, "Ascending sorted data");
		configOutput(DESCENDING_OUTPUT, "Descending sorted data");
		timeSinceUpdate.reset();
	}
	
	void process(const ProcessArgs& args) override {
		timeSinceUpdate.update(args.sampleTime); // Advance the timer
		
		if (!inputs[DATA_INPUT].isConnected()) {
			for (int i = 0; i < OUTPUTS_LEN; ++i) {
				outputs[i].setChannels(0); // Ensure no output
			}
			return; // Exit early if there's no input
		}
		
		if (!timeSinceUpdate.check(0.01f) && params[TOGGLE_SWITCH].getValue() < 0.5f) {
			return; // Break early if we haven't reached our throttle time, unless we're in Alt mode
		}
		
		timeSinceUpdate.reset(); // Reset the timer

		int maxChannels = inputs[DATA_INPUT].getChannels();
		
		std::vector<float> dataValues(maxChannels);
		std::vector<float> sortValues(maxChannels);
		std::vector<float> selectValues(maxChannels);

		for (int i = 0; i < maxChannels; i++) {
			dataValues[i] = inputs[DATA_INPUT].getVoltage(i);
			sortValues[i] = inputs[SORT_INPUT].isConnected() ? inputs[SORT_INPUT].getVoltage(i) : 0.0f;
			selectValues[i] = inputs[SELECT_INPUT].isConnected() ? inputs[SELECT_INPUT].getVoltage(i) : 0.0f;
		}

		// Ascending and Descending sorting of dataValues
		std::vector<float> ascendingData = dataValues;
		std::sort(ascendingData.begin(), ascendingData.end());
		std::vector<float> descendingData = ascendingData; // Copy already sorted data
		std::reverse(descendingData.begin(), descendingData.end()); // Reverse for descending order

		// Outputs
		outputs[PASSTHRU_OUTPUT].setChannels(maxChannels);
		outputs[ASCENDING_OUTPUT].setChannels(maxChannels);
		outputs[DESCENDING_OUTPUT].setChannels(maxChannels);

		for (int i = 0; i < maxChannels; i++) {
			outputs[PASSTHRU_OUTPUT].setVoltage(dataValues[i], i);
			outputs[ASCENDING_OUTPUT].setVoltage(ascendingData[i], i);
			outputs[DESCENDING_OUTPUT].setVoltage(descendingData[i], i);
		}

		// Create a sorting index based on sortValues
		std::vector<int> index(maxChannels);
		std::iota(index.begin(), index.end(), 0);
		std::stable_sort(index.begin(), index.end(), [&sortValues](int i, int j) { return sortValues[i] < sortValues[j]; });

		// Apply sort to dataValues
		std::vector<float> sortedData(maxChannels);
		for (int i = 0; i < maxChannels; i++) {
			sortedData[i] = dataValues[index[i]];
		}
		
		outputs[SORTED_OUTPUT].setChannels(maxChannels);
		for (int i = 0; i < maxChannels; i++) {
			outputs[SORTED_OUTPUT].setVoltage(sortedData[i], i);
		}

		// Apply selection
		std::vector<float> selectedData;
		for (int i = 0; i < maxChannels; i++) {
			if (selectValues[i] >= 1.0f) {
				selectedData.push_back(dataValues[i]);
			}
		}

		outputs[SELECTED_OUTPUT].setChannels(selectedData.size());
		for (size_t i = 0; i < selectedData.size(); i++) {
			outputs[SELECTED_OUTPUT].setVoltage(selectedData[i], i);
		}

		// Sort the data, then select
		std::vector<float> sortedSelectedData;
		for (int i = 0; i < maxChannels; i++) {
			if (selectValues[index[i]] >= 1.0f) {
				sortedSelectedData.push_back(sortedData[i]);
			}
		}

		outputs[SORTED_AND_SELECTED_OUTPUT].setChannels(sortedSelectedData.size());
		for (size_t i = 0; i < sortedSelectedData.size(); i++) {
			outputs[SORTED_AND_SELECTED_OUTPUT].setVoltage(sortedSelectedData[i], i);
		}

		// Select data, then sort it
		std::vector<float> selectedAndSortedData = selectedData;
		std::sort(selectedAndSortedData.begin(), selectedAndSortedData.end());

		outputs[SELECTED_AND_SORTED_OUTPUT].setChannels(selectedAndSortedData.size());
		for (size_t i = 0; i < selectedAndSortedData.size(); i++) {
			outputs[SELECTED_AND_SORTED_OUTPUT].setVoltage(selectedAndSortedData[i], i);
		}
	}

};

struct SortWidget : ModuleWidget {
	SortWidget(Sort* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/sort.svg")));
		
	// CONTROLS --------
		addParam(createParamCentered<BrassToggle>(mm2px(Vec(15, 6)), module, Sort::TOGGLE_SWITCH));
		// Toggle between sample rate and throttled rate
		
	// INPUTS --------
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*1.5)), module, Sort::DATA_INPUT));
		// The polyphonic cable we are treating like our "data column"
		
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*2.5)), module, Sort::SORT_INPUT));
		// The polyphonic cable we are treating as our "sort key' (as in "index" or "ordering")
		
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*3.5)), module, Sort::SELECT_INPUT));
		// The polyphonic cable we are treating as our "select key" (as in "mask" or database-style "filter")
		
	// OUTPUTS --------
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*5)), module, Sort::PASSTHRU_OUTPUT));
		// Pass through DATA_INPUT as-is
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*6)), module, Sort::SORTED_OUTPUT));
		// Use as the "index" or "key" when sorting data (like an Excel style rank() )
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*7)), module, Sort::SELECTED_OUTPUT));
		// Unselected channels are removed from the output set, not just muted, with the select key treated like booleans)
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*8)), module, Sort::SORTED_AND_SELECTED_OUTPUT));
		// Sort the data according to the key, THEN apply the selection, and output a set of channels size to that new set
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*9)), module, Sort::SELECTED_AND_SORTED_OUTPUT));
		// Apply the selection, THEN sort the data according to the key, and output a set of channels size to that new set
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*10)), module, Sort::ASCENDING_OUTPUT));
		// The data sorted by its own ascending order, for convenience
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*11)), module, Sort::DESCENDING_OUTPUT));
		// The data sorted by its own descending order, for convenience
	}
};


Model* modelSort = createModel<Sort, SortWidget>("Sort");