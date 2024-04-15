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
	
	Spellbook() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(CLOCK_INPUT, "Clock In");
		configOutput(POLY_OUTPUT, "16 voltages from columns");
		configOutput(OUT01_OUTPUT, "1st voltage");
		configOutput(OUT09_OUTPUT, "9th voltage");
		configOutput(OUT02_OUTPUT, "2nd voltage");
		configOutput(OUT10_OUTPUT, "10th voltage");
		configOutput(OUT03_OUTPUT, "3rd voltage");
		configOutput(OUT11_OUTPUT, "11th voltage");
		configOutput(OUT04_OUTPUT, "4th voltage");
		configOutput(OUT12_OUTPUT, "12th voltage");
		configOutput(OUT05_OUTPUT, "5th voltage");
		configOutput(OUT13_OUTPUT, "13th voltage");
		configOutput(OUT06_OUTPUT, "6th voltage");
		configOutput(OUT14_OUTPUT, "14th voltage");
		configOutput(OUT07_OUTPUT, "7th voltage");
		configOutput(OUT15_OUTPUT, "15th voltage");
		configOutput(OUT08_OUTPUT, "8th voltage");
		configOutput(OUT16_OUTPUT, "16th voltage");
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
		return it == s.end() && s.size() > minSize;  // True if all characters are digits or one decimal
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
		if (!steps.empty() && clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			currentStep = (currentStep + 1) % steps.size();  // Move to the next step, wrap around
		}

		if (!steps.empty()) {
			std::vector<float> &currentValues = steps[currentStep];
			int numChannels = std::min((int)currentValues.size(), 16); // Ensure we do not exceed 16 channels

			// Set number of channels for polyphonic output
			outputs[POLY_OUTPUT].setChannels(numChannels);
			
			for (int i = 0; i < numChannels; i++) {
				outputs[OUT01_OUTPUT + i].setVoltage(currentValues[i]);
				outputs[POLY_OUTPUT].setVoltage(currentValues[i], i);
			}

			// Zero the rest of the outputs
			for (int i = numChannels; i < 16; i++) {
				outputs[OUT01_OUTPUT + i].setVoltage(0.0f);
				outputs[POLY_OUTPUT].setVoltage(0.0f, i);
			}
		} else {
			// Zero all outputs if there are no steps
			for (int i = 0; i < 16; i++) {
				outputs[OUT01_OUTPUT + i].setVoltage(0.0f);
				outputs[POLY_OUTPUT].setVoltage(0.0f, i);
			}
			outputs[POLY_OUTPUT].setChannels(16);  // Default to 16 channels, all zeroed
		}
	}

};

struct SpellbookTextField : LedDisplayTextField {
    Spellbook* module;

    // This constructor ensures the text field is initialized with the current text.
    SpellbookTextField() {
        if (module) {
            setText(module->text);
        }
    }

    void step() override {
        LedDisplayTextField::step();
        // Update the text field only if it's dirty and the text is different
        // to avoid unnecessary updates that can cause cursor or scroll issues.
        if (module && module->dirty && getText() != module->text) {
            setText(module->text);
            module->dirty = false;
        }
    }

	void onChange(const ChangeEvent& e) override {
		if (module) {
			std::string cleanedText;
			std::string originalText = getText();

			// Remove all unwanted characters
			for (char &c : originalText) {
				if (c >= 32 && c <= 126) { // ASCII printable characters
					cleanedText += c;
				} else if (c == '\n' || c == '\r') { // Convert all types of newlines to just '\n'
					cleanedText += '\n';
				}
				// You can add more conditions here to handle other types of characters
			}

			module->text = cleanedText;
			module->dirty = true;
			setText(cleanedText); // Update the text field to show the cleaned text
		}
    }
};

struct SpellbookWidget : ModuleWidget {
	SpellbookWidget(Spellbook* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/spellbook.svg")));
		
		addInput(createInputCentered<BrassPort>(mm2px(Vec(11.331, 14.933)), module, Spellbook::CLOCK_INPUT));
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 14.933)), module, Spellbook::POLY_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 27.166)), module, Spellbook::OUT01_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 27.166)), module, Spellbook::OUT09_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 39.399)), module, Spellbook::OUT02_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 39.399)), module, Spellbook::OUT10_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 51.632)), module, Spellbook::OUT03_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 51.632)), module, Spellbook::OUT11_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 63.866)), module, Spellbook::OUT04_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 63.866)), module, Spellbook::OUT12_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 76.099)), module, Spellbook::OUT05_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 76.099)), module, Spellbook::OUT13_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 88.332)), module, Spellbook::OUT06_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 88.332)), module, Spellbook::OUT14_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 100.566)), module, Spellbook::OUT07_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 100.566)), module, Spellbook::OUT15_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 112.799)), module, Spellbook::OUT08_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 112.799)), module, Spellbook::OUT16_OUTPUT));
		
        // Create the text field widget.
        SpellbookTextField* textField = createWidget<SpellbookTextField>(mm2px(Vec(33.992, 0)));
        textField->box.size = mm2px(Vec(209.848, RACK_GRID_HEIGHT));
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