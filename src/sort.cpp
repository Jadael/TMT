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
	// Put some sort of "three column" data structure here we can update and manipulate extremely efficiently (i.e. hopefully at audio-rate >=48kHz)

	Sort() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Alt Mode: Process at audio rate (CPU heavy)");
		configInput(DATA_INPUT, "Polyphonic Data");
		configInput(SORT_INPUT, "Ordering");
		configInput(SELECT_INPUT, "Selection");
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
				outputs[i].setChannels(1);
				outputs[i].setVoltage(0.0f, 0);
			}
			return; // Exit early if there's no input
		}
		
		if (!timeSinceUpdate.check(0.01f) && params[TOGGLE_SWITCH].getValue() < 0.5f) {
			return; // Break early if we haven't reached our throttle time, unless we're in Alt mode
		}
		
		timeSinceUpdate.reset(); // Reset the timer

		int maxChannels = std::max({inputs[DATA_INPUT].getChannels(), inputs[SORT_INPUT].getChannels(), inputs[SELECT_INPUT].getChannels()});
		
		std::vector<float> dataValues(maxChannels);
		std::vector<float> sortValues(maxChannels);
		std::vector<float> selectValues(maxChannels);

		for (int i = 0; i < maxChannels; i++) {
			dataValues[i] = inputs[DATA_INPUT].getVoltage(i);
			sortValues[i] = inputs[SORT_INPUT].isConnected() ? inputs[SORT_INPUT].getVoltage(i) : 0.0f;
			selectValues[i] = inputs[SELECT_INPUT].isConnected() ? inputs[SELECT_INPUT].getVoltage(i) : 0.0f;
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

		// Apply selection
		std::vector<float> selectedData;
		std::vector<int> selectedIndex;
		for (int i = 0; i < maxChannels; i++) {
			if (selectValues[i] > 5.0f) {
				selectedData.push_back(dataValues[i]);
				selectedIndex.push_back(index[i]);
			}
		}

		// Sort selected data
		std::vector<float> sortedSelectedData(selectedIndex.size());
		for (size_t i = 0; i < selectedIndex.size(); i++) {
			sortedSelectedData[i] = dataValues[selectedIndex[i]];
		}
		std::stable_sort(sortedSelectedData.begin(), sortedSelectedData.end());

		// Ascending and Descending sorting of dataValues
		std::vector<float> ascendingData = dataValues;
		std::sort(ascendingData.begin(), ascendingData.end());
		std::vector<float> descendingData = ascendingData; // Copy already sorted data
		std::reverse(descendingData.begin(), descendingData.end()); // Reverse for descending order

		// Outputs
		outputs[PASSTHRU_OUTPUT].setChannels(maxChannels);
		outputs[SORTED_OUTPUT].setChannels(maxChannels);
		outputs[SELECTED_OUTPUT].setChannels(selectedData.size());
		outputs[SORTED_AND_SELECTED_OUTPUT].setChannels(selectedData.size());
		outputs[SELECTED_AND_SORTED_OUTPUT].setChannels(sortedSelectedData.size());
		outputs[ASCENDING_OUTPUT].setChannels(maxChannels);
		outputs[DESCENDING_OUTPUT].setChannels(maxChannels);

		for (int i = 0; i < maxChannels; i++) {
			outputs[PASSTHRU_OUTPUT].setVoltage(dataValues[i], i);
			outputs[SORTED_OUTPUT].setVoltage(sortedData[i], i);
			outputs[ASCENDING_OUTPUT].setVoltage(ascendingData[i], i);
			outputs[DESCENDING_OUTPUT].setVoltage(descendingData[i], i);
		}
		for (size_t i = 0; i < selectedData.size(); i++) {
			outputs[SELECTED_OUTPUT].setVoltage(selectedData[i], i);
			outputs[SORTED_AND_SELECTED_OUTPUT].setVoltage(selectedData[i], i);
		}
		for (size_t i = 0; i < sortedSelectedData.size(); i++) {
			outputs[SELECTED_AND_SORTED_OUTPUT].setVoltage(sortedSelectedData[i], i);
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
		// The polyphonic cable we are treating as our "index" or "ordering key column"
		
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*3.5)), module, Sort::SELECT_INPUT));
		// The polyphonic cable we are treating as our "crtieria" or "mask column"
		
		// OUTPUTS --------
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*5)), module, Sort::PASSTHRU_OUTPUT));
		// Pass through DATA_INPUT as-is
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*6)), module, Sort::SORTED_OUTPUT));
		// Use as the "index" or "sort key" when sorting data (i.e. for an Excel style rank() index)
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*7)), module, Sort::SELECTED_OUTPUT));
		// As as a "criteria" or "mask" for data - unselected channells are removed from the output set, not just muted (i.e. for an Excel style filter() criteria, where these voltages are treated like booleans)
		
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