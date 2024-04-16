#include "plugin.hpp"
#include "ports.hpp"
#include <random>
#include <string>
#include <sstream>
#include <vector>
#include <array>
#include <map>

// Define a map for note to semitone conversion relative to C4
std::map<std::string, int> noteToSemitone = {
    {"C", 0}, {"C#", 1}, {"Db", 1}, {"D", 2}, {"D#", 3}, {"Eb", 3}, {"E", 4},
    {"F", 5}, {"F#", 6}, {"Gb", 6}, {"G", 7}, {"G#", 8}, {"Ab", 8},
    {"A", 9}, {"A#", 10}, {"Bb", 10}, {"B", 11}
};

struct Spellbook : Module {
    enum ParamId {
        TOGGLE_SWITCH,
        PARAMS_LEN
    };
    enum InputId {
        CLOCK_INPUT,
		RESET_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        POLY_OUTPUT,
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
	
    dsp::SchmittTrigger clockTrigger;  // Schmitt trigger for handling clock input
    std::vector<std::vector<float>> steps;  // Parsed CSV data, each inner vector is one step
    int currentStep = 0;  // Current step index
    std::string text;  // Stored text from the user input
    bool dirty = false;  // Flag to re-parse text when changed
	// Initialize the last values for each channel to 0.
	std::array<float, 16> lastValues = {};
	// Flag to check initialization
    bool fullyInitialized = false;
	
	Spellbook() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(CLOCK_INPUT, "Clock / Next Step");
		configInput(RESET_INPUT, "Reset - UNUSED");
		configOutput(POLY_OUTPUT, "16 voltages from columns");
		configOutput(OUT01_OUTPUT, "Column 1");
		configOutput(OUT09_OUTPUT, "Column 9");
		configOutput(OUT02_OUTPUT, "Column 2");
		configOutput(OUT10_OUTPUT, "Column 10");
		configOutput(OUT03_OUTPUT, "Column 3");
		configOutput(OUT11_OUTPUT, "Column 11");
		configOutput(OUT04_OUTPUT, "Column 4");
		configOutput(OUT12_OUTPUT, "Column 12");
		configOutput(OUT05_OUTPUT, "Column 5");
		configOutput(OUT13_OUTPUT, "Column 13");
		configOutput(OUT06_OUTPUT, "Column 6");
		configOutput(OUT14_OUTPUT, "Column 14");
		configOutput(OUT07_OUTPUT, "Column 7");
		configOutput(OUT15_OUTPUT, "Column 15");
		configOutput(OUT08_OUTPUT, "Column 8");
		configOutput(OUT16_OUTPUT, "Column 16");
		
        // Set outputs to a safe initial state
		outputs[POLY_OUTPUT].setChannels(16);
        for (int i = 0; i < 16; ++i) {
            outputs[OUT01_OUTPUT + i].setVoltage(0.0f);
        }
        fullyInitialized = true; // Mark as fully initialized at the end of constructor
    }
	

    void onReset() override {
        text = "";
        dirty = true;
    }

    void fromJson(json_t* rootJ) override {
        Module::fromJson(rootJ);
        json_t* textJ = json_object_get(rootJ, "text");
        if (textJ)
            text = json_string_value(textJ);
        dirty = true;
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "text", json_stringn(text.c_str(), text.size()));
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        json_t* textJ = json_object_get(rootJ, "text");
        if (textJ)
            text = json_string_value(textJ);
        dirty = true;
    }
	
	bool isDecimal(const std::string& s) {
		std::string::const_iterator it = s.begin();
		bool decimalPoint = false;
		int minSize = 0;
		if(!s.empty() && (s.front() == '-' || s.front() == '+')) {
			it++;
			minSize++;
		}
		while (it != s.end()) {
			if (*it == '.') {
				if (!decimalPoint) decimalPoint = true;
				else break;  // More than one decimal point found
			} else if (!isdigit(*it)) {
				break;  // Non-digit found
			}
			++it;
		}
		return it == s.end() && s.size() > static_cast<std::string::size_type>(minSize);  // True if all characters are digits or one decimal
	}
	
	float noteNameToVoltage(const std::string& noteName, int octave) {
		// C4 is considered 0V, and each octave up or down is a change of 1V.
		int semitoneOffsetFromC4 = noteToSemitone.at(noteName) + (octave - 4) * 12;
		return static_cast<float>(semitoneOffsetFromC4) / 12.0f;
	}

	bool isValidNoteName(const std::string& note) {
		return noteToSemitone.find(note) != noteToSemitone.end();
	}

	bool tryParseOctave(const std::string& text, int& octaveOut) {
		try {
			octaveOut = std::stoi(text);
			return true;
		} catch (...) {
			return false;
		}
	}
	
	float parsePitch(const std::string& cell) {
		// Iterate over the noteToSemitone map and try to find a matching note name
		for (const auto& notePair : noteToSemitone) {
			const std::string& noteName = notePair.first;
			// Check if the cell starts with the note name
			if (cell.rfind(noteName, 0) == 0) {
				std::string octavePart = cell.substr(noteName.size());
				int octave;
				// Try parsing the remaining part of the cell as the octave
				if (tryParseOctave(octavePart, octave)) {
					return noteNameToVoltage(noteName, octave);
				}
			}
		}
		return 0.0f; // Return default value if parsing fails
	}

	void parseText() {
		steps.clear();
		std::istringstream ss(text);
		std::string line;

		while (getline(ss, line)) {
			std::vector<float> stepData(16, 0.0f); // Default to 0 for all 16 channels
			std::istringstream lineStream(line);
			std::string cell;
			int index = 0;
			while (getline(lineStream, cell, ',') && index < 16) {
				cell.erase(std::remove_if(cell.begin(), cell.end(), ::isspace), cell.end());
				
				try {
					if (cell == "X") {
						stepData[index] = 10.0f;
					} else if (isDecimal(cell)) {  // Check if it's a decimal
						try {
							stepData[index] = std::stof(cell);
						} catch (...) {
							stepData[index] = 0.0f;  // Default to 0 if parsing fails
						}
					} else {
						try {
							stepData[index] = parsePitch(cell);
						} catch (...) {
							stepData[index] = 0.0f;  // Default to 0 if parsing fails
						}
					}
				} catch (...) {
					stepData[index] = 0.0f;  // Default to 0 if parsing fails
				}
				index++;
			}
			steps.push_back(stepData);
		}

		if (steps.empty()) {
			steps.push_back(std::vector<float>(16, 0.0f));  // Add a default step if no data present
		}

		currentStep = 0; // Reset step position after re-parsing
	}

	void process(const ProcessArgs& args) override {
		if (dirty) {
			parseText();
			dirty = false;
		}

		if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			if (!steps.empty()) {
				// Only update the step if we have steps available
				currentStep = (currentStep + 1) % steps.size();
			}
		}

		// Make sure polyphonic output has 16 channels
		outputs[POLY_OUTPUT].setChannels(16);

		if (!steps.empty()) {
			std::vector<float>& currentValues = steps[currentStep];
			for (int i = 0; i < 16; i++) {
				// If the current step has a value for this channel, update lastValues
				if (i < (int)currentValues.size() && !std::isnan(currentValues[i])) {
					lastValues[i] = currentValues[i];
				}
				// Update the output voltages to the last known values
				outputs[OUT01_OUTPUT + i].setVoltage(lastValues[i]);
				outputs[POLY_OUTPUT].setVoltage(lastValues[i], i);
			}
		} else {
			// If there are no steps, we do nothing, retaining the last known values.
			// This is safe because we initialize lastValues to 0 and never let them be NaN.
			for (int i = 0; i < 16; i++) {
				outputs[OUT01_OUTPUT + i].setVoltage(lastValues[i]);
				outputs[POLY_OUTPUT].setVoltage(lastValues[i], i);
			}
		}
	}

};

struct SpellbookTextField : LedDisplayTextField {
    Spellbook* module;
    NVGcolor goldColor = nvgRGB(255, 215, 0); // Gold color for the dot

    SpellbookTextField() {
    }

    void draw(const DrawArgs& args) override {
        LedDisplayTextField::draw(args);

        if (!module || !module->fullyInitialized) return;

        // Calculate the y-position of the dot based on the current step and line height
        float lineHeight = 12.0f;  // Assuming each line of text is roughly 12 pixels high, adjust as necessary
        float yPos = (module->currentStep * lineHeight);

        // Draw a gold dot at the calculated position
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, 3, yPos+lineHeight+3, 3); // Draw circle at x=5px, calculated yPos, radius=5px
        nvgFillColor(args.vg, goldColor);
        nvgFill(args.vg);
    }

    void step() override {
        if (!module || !module->fullyInitialized) return;

        LedDisplayTextField::step();
        if (module->dirty && getText() != module->text) {
            setText(module->text);
            module->dirty = false;
        }
    }

	void onChange(const ChangeEvent& e) override {
		if (!module || !module->fullyInitialized) return; // Prevent changes if module not ready

		std::string originalText = getText();
		std::istringstream ss(originalText);
		std::string line;
		std::vector<std::vector<std::string>> rows;
		std::vector<size_t> columnWidths;

		// Parse the input text into rows and columns, computing column widths
		while (std::getline(ss, line)) {
			std::istringstream lineStream(line);
			std::string cell;
			std::vector<std::string> cells;
			size_t columnIndex = 0;

			while (std::getline(lineStream, cell, ',')) {
				cell.erase(cell.find_last_not_of(" \n\r\t") + 1); // Trim trailing whitespace
				cell.erase(0, cell.find_first_not_of(" \n\r\t")); // Trim leading whitespace
				cells.push_back(cell);

				if (columnWidths.size() <= columnIndex) {
					columnWidths.push_back(cell.size());
				} else {
					columnWidths[columnIndex] = std::max(columnWidths[columnIndex], cell.size());
				}
				++columnIndex;
			}
			rows.push_back(cells);
		}

		// Construct the cleaned text with proper padding and commas
		std::string cleanedText;
		for (size_t rowIdx = 0; rowIdx < rows.size(); ++rowIdx) {
			auto& row = rows[rowIdx];
			for (size_t i = 0; i < row.size(); ++i) {
				cleanedText += row[i];
				if (i < columnWidths.size() - 1) { // Add padding spaces
					cleanedText += std::string(columnWidths[i] - row[i].size(), ' ');
				}
				if (i < row.size() - 1) { // Add comma and space
					cleanedText += ", ";
				}
			}
			if (rowIdx < rows.size() - 1) { // Add newline if it's not the last row
				cleanedText += '\n';
			}
		}

		module->text = cleanedText;
		module->dirty = true;
		setText(cleanedText); // Update the text field to show the cleaned text
	}
};

struct SpellbookWidget : ModuleWidget {
	SpellbookWidget(Spellbook* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/spellbook.svg")));
		
		addInput(createInputCentered<BrassPort>(mm2px(Vec(11.331, 14.933)), module, Spellbook::CLOCK_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(11.331, 27.166)), module, Spellbook::RESET_INPUT));
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654+209.848-(11.331/2), 14.933)), module, Spellbook::POLY_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331+209.848, 27.166)), module, Spellbook::OUT01_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654+209.848, 27.166)), module, Spellbook::OUT09_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331+209.848, 39.399)), module, Spellbook::OUT02_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654+209.848, 39.399)), module, Spellbook::OUT10_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331+209.848, 51.632)), module, Spellbook::OUT03_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654+209.848, 51.632)), module, Spellbook::OUT11_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331+209.848, 63.866)), module, Spellbook::OUT04_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654+209.848, 63.866)), module, Spellbook::OUT12_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331+209.848, 76.099)), module, Spellbook::OUT05_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654+209.848, 76.099)), module, Spellbook::OUT13_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331+209.848, 88.332)), module, Spellbook::OUT06_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654+209.848, 88.332)), module, Spellbook::OUT14_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331+209.848, 100.566)), module, Spellbook::OUT07_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654+209.848, 100.566)), module, Spellbook::OUT15_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331+209.848, 112.799)), module, Spellbook::OUT08_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654+209.848, 112.799)), module, Spellbook::OUT16_OUTPUT));
		
        // Create the text field widget.
        SpellbookTextField* textField = createWidget<SpellbookTextField>(mm2px(Vec(33.992, 0)));
        textField->box.size = mm2px(Vec(209.848-20.654-11.331, RACK_GRID_HEIGHT));
        textField->multiline = true;
        textField->module = module;
        // If the module is loaded, set the text to the current content of the module's text buffer.
        if (module) {
            textField->setText(module->text);
        }
        addChild(textField);
    }
};


Model* modelSpellbook = createModel<Spellbook, SpellbookWidget>("Spellbook");