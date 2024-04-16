#include "plugin.hpp"
#include "ports.hpp"
#include <sstream>
#include <vector>
#include <array>
#include <map>

// Map to convert note names into semitone offsets relative to C4
std::map<std::string, int> noteToSemitone = {
    {"C", 0}, {"C#", 1}, {"Db", 1}, {"D", 2}, {"D#", 3}, {"Eb", 3},
    {"E", 4}, {"F", 5}, {"F#", 6}, {"Gb", 6}, {"G", 7}, {"G#", 8},
    {"Ab", 8}, {"A", 9}, {"A#", 10}, {"Bb", 10}, {"B", 11}
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
        OUT01_OUTPUT, OUT02_OUTPUT, OUT03_OUTPUT, OUT04_OUTPUT,
        OUT05_OUTPUT, OUT06_OUTPUT, OUT07_OUTPUT, OUT08_OUTPUT,
        OUT09_OUTPUT, OUT10_OUTPUT, OUT11_OUTPUT, OUT12_OUTPUT,
        OUT13_OUTPUT, OUT14_OUTPUT, OUT15_OUTPUT, OUT16_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        LIGHTS_LEN
    };

    dsp::SchmittTrigger clockTrigger;  // Trigger for handling clock input
    std::vector<std::vector<float>> steps;  // Stores parsed CSV data, each vector represents a step
    std::array<float, 16> lastValues = {};  // Stores last values for each channel to maintain state between steps
    int currentStep = 0;  // Index of the current step being processed
    std::string text;  // Text buffer for user input
    bool dirty = false;  // Flag to indicate when the text buffer needs re-parsing
    bool fullyInitialized = false;  // Flag to check if the module has been fully initialized

    Spellbook() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configInput(CLOCK_INPUT, "Clock / Next Step");
        configInput(RESET_INPUT, "Reset - UNUSED");
        configOutput(POLY_OUTPUT, "16 voltages from columns");

        // Configure individual outputs for each column
        for (int i = 0; i < 16; ++i) {
            configOutput(OUT01_OUTPUT + i, "Column " + std::to_string(i + 1));
            outputs[OUT01_OUTPUT + i].setVoltage(0.0f);
        }
        outputs[POLY_OUTPUT].setChannels(16);
        fullyInitialized = true; // Mark initialization as complete
    }

    void onReset() override {
        text = "";
        dirty = true;
    }

    void fromJson(json_t* rootJ) override {
        json_t* textJ = json_object_get(rootJ, "text");
        if (textJ) {
            text = json_string_value(textJ);
            dirty = true;
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "text", json_stringn(text.c_str(), text.size()));
        return rootJ;
    }

    // Checks if a string represents a decimal number
    bool isDecimal(const std::string& s) {
        bool decimalPoint = false;
        auto it = s.begin();
        if (!s.empty() && (s.front() == '-' || s.front() == '+')) {
            it++; // Skip the sign for checking digits
        }
        while (it != s.end()) {
            if (*it == '.') {
                if (decimalPoint) break; // Invalid if more than one decimal point
                decimalPoint = true;
            } else if (!isdigit(*it)) {
                break; // Invalid if non-digit characters found
            }
            ++it;
        }
        return it == s.end() && s.size() > (s.front() == '-' || s.front() == '+' ? 1 : 0);
    }

    // Converts a note name and octave to a voltage based on Eurorack 1V/oct standard
    float noteNameToVoltage(const std::string& noteName, int octave) {
        int semitoneOffsetFromC4 = noteToSemitone.at(noteName) + (octave - 4) * 12;
        return static_cast<float>(semitoneOffsetFromC4) / 12.0f;
    }

    // Parses pitch from a cell in the format "NoteNameOctave", e.g., "C4"
    float parsePitch(const std::string& cell) {
        for (const auto& notePair : noteToSemitone) {
            const std::string& noteName = notePair.first;
            if (cell.rfind(noteName, 0) == 0) { // Check if the cell starts with the note name
                std::string octavePart = cell.substr(noteName.size());
                int octave;
                if (tryParseOctave(octavePart, octave)) {
                    return noteNameToVoltage(noteName, octave);
                }
            }
        }
        return 0.0f; // Default value if parsing fails
    }

    // Attempts to parse a string to an integer, safely handling exceptions
    bool tryParseOctave(const std::string& text, int& octaveOut) {
        try {
            octaveOut = std::stoi(text);
            return true;
        } catch (...) {
            return false;
        }
    }

    // Parses the text input to update the steps data structure
    void parseText() {
        steps.clear();
        std::istringstream ss(text);
        std::string line;
        while (getline(ss, line)) {
            std::vector<float> stepData(16, 0.0f);
            std::istringstream lineStream(line);
            std::string cell;
            int index = 0;
            while (getline(lineStream, cell, ',') && index < 16) {
                cell.erase(std::remove_if(cell.begin(), cell.end(), ::isspace), cell.end());
                float value = 0.0f;
                if (cell == "X") {
                    value = 10.0f;
                } else if (isDecimal(cell)) {
                    value = std::stof(cell);
                } else {
                    value = parsePitch(cell);
                }
                stepData[index++] = value;
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
                currentStep = (currentStep + 1) % steps.size();
            }
        }

        std::vector<float>& currentValues = steps.empty() ? lastValues : steps[currentStep];
        for (int i = 0; i < 16; i++) {
            float outputValue = (i < (int)currentValues.size()) ? currentValues[i] : lastValues[i];
            outputs[OUT01_OUTPUT + i].setVoltage(outputValue);
            outputs[POLY_OUTPUT].setVoltage(outputValue, i);
            lastValues[i] = outputValue;
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