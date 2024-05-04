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
			// Set all outputs to a single channel of 0v if no Data input is present
			// TODO: Skip processing if the input hasn't *changed*? Otherwise even this sits here zeroing every output every sample on idle
			for (int i = 0; i < OUTPUTS_LEN; ++i) {
				outputs[i].setChannels(1);
				outputs[i].setVoltage(0.0f, 0);
			}
			return; // Exit early if there's no input
		}
		
		// Check the timer/toggle for processing rate
		if (!timeSinceUpdate.check(0.01f) && params[TOGGLE_SWITCH].getValue() < 0.5f) {
			return; // Break early if we haven't reached our throttle time, unless we're in Alt mode
		}
		
		timeSinceUpdate.reset(); // Reset the timer, since we're about to re-process
		
		// Logic here --------
		// Be wary: any inputs might be disconnected, or have different numbers of channels
		// The "SORT" and "SELECT" columns should be padded with repeats to reach the size of longer DATA inputs, or extra items can be ignored for shorter ones
		// Getting or setting channels that don't exist can cause crashes
		// However, we DO want to SAFELY grow/shrink outputs to match the final sizes of the data sets that output is being given
		
		// Get inputs, safely handle their sizes, and read them into our three "columns"
		
		// Output the appropriate "Excel style queries" needed for each output
		
		// Update and size each output, safely and efficiently
	}
};

struct SortWidget : ModuleWidget {
	SortWidget(Sort* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/stats.svg")));
		
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