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
		// Configures the toggle switch parameter that determines the processing mode of the module.
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Alt Mode: Process at audio rate (CPU heavy)");
		
		// Configures the inputs and outputs with descriptions for each port.
		
		// Data Input - This is the main data input for the module, where you send the signal you want to process.
		configInput(DATA_INPUT, "Data");
		inputInfos[DATA_INPUT]->description = "- Main polyphonic input for the data you want to sort or select.\n- Connect the signal that contains the data you wish to manipulate, treating the channels like an 'array' or 'column'.";

		// Sort Input - This port takes the sort key used to determine the order of data.
		configInput(SORT_INPUT, "Sort Key");
		inputInfos[SORT_INPUT]->description = "- Polyphonic input for the sort key.\n- Connect a signal here to determine the order in which data is sorted.\n- Data connected to 'Data Input' will be sorted based on the values from this input.\n- This is similar to the Excel rank() function, sorting one array by another.";

		// Select Input - This port reads the select key which determines which data points are outputted.
		configInput(SELECT_INPUT, "Select Key");
		inputInfos[SELECT_INPUT]->description = "- Polyphonic input for the select key.\n- Connect a signal here to determine which data points are included in the output.\n- Data points with corresponding 'Select Key' values of 1.0v or higher will be considered 'selected'.\n- This is similar to the Excel filter() function, with the Select Key being treated like an array true/false booleans.";

		// Passthrough Output - Directly outputs the data received at the Data Input without any modifications.
		configOutput(PASSTHRU_OUTPUT, "Passthrough Output");
		outputInfos[PASSTHRU_OUTPUT]->description = "- Outputs the data received at the 'Data Input' directly without any modifications.";

		// Sorted Output - Outputs data sorted based on the values of the Sort Key.
		configOutput(SORTED_OUTPUT, "Sorted Output");
		outputInfos[SORTED_OUTPUT]->description = "- Outputs data sorted based on the 'Sort Key'.\n- The data from 'Data Input' is rearranged into a new order determined by the values from 'Sort Key', sorted from lowest to highest.";

		// Selected Output - Outputs only the data points that are 'selected' by the Select Key.
		configOutput(SELECTED_OUTPUT, "Selected Output");
		outputInfos[SELECTED_OUTPUT]->description = "- Outputs only the data points from 'Data Input' that are 'selected' by the 'Select Key'.\n- A data point is included in this output if its corresponding 'Select Key' value is 1.0v or higher.";

		// Sorted and Selected Output - Data is first sorted by the Sort Key, then filtered by the Select Key.
		configOutput(SORTED_AND_SELECTED_OUTPUT, "Sorted, then Selected Output");
		outputInfos[SORTED_AND_SELECTED_OUTPUT]->description = "- Outputs data that is first sorted by the 'Sort Key' and then filtered by the 'Select Key'.\n- The data is first arranged based on the sorting key, and then only the selected data (where 'Select Key' >= 1.0v) is output.";

		// Selected and Sorted Output - Data is first filtered by the Select Key, then sorted by the Sort Key.
		configOutput(SELECTED_AND_SORTED_OUTPUT, "Selected, then Sorted Output");
		outputInfos[SELECTED_AND_SORTED_OUTPUT]->description = "- Outputs data that is first filtered by the 'Select Key' and then sorted by the 'Sort Key'.\n- The data is first reduced to only include the selected channels, and then that subset is sorted based on the sorting key.";

		// Ascending Output - Outputs data sorted in ascending order, ignoring the Sort Key.
		configOutput(ASCENDING_OUTPUT, "Ascending Output");
		outputInfos[ASCENDING_OUTPUT]->description = "- Outputs data sorted in ascending order based on its own values, ignoring the 'Sort Key'.\n- This is a simple ascending sort of the 'Data Input'.";

		// Descending Output - Outputs data sorted in descending order, ignoring the Sort Key.
		configOutput(DESCENDING_OUTPUT, "Descending Output");
		outputInfos[DESCENDING_OUTPUT]->description = "- Outputs data sorted in descending order based on its own values, ignoring the 'Sort Key'.\n- This is a simple descending sort of the 'Data Input'.";

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