#include "plugin.hpp"
#include "ports.hpp"
#include <sstream>
#include <vector>
#include <map>
#include <iomanip>
#define GRID_SNAP 10.16

struct StepData {
    float voltage;
    char type;  // 'N' for normal, 'T' for trigger, 'R' for retrigger
};

struct Timer {
    float timeLeft = 0.0f;  // Time since timer start in seconds

    // Reset to 1 ms
    void reset() {
        timeLeft = 0.0f;  // Start timer at 0 on resets
    }
	
	void set(float seconds) {
		timeLeft = seconds;
	}

    // Update the timer and check if the period has expired
    void update(float deltaTime) {
        timeLeft += deltaTime;
    }
	
	// Return seconds since timer start
	float time(float deltaTime) {
        update(deltaTime);
		return timeLeft;
	}
	
	// Check whether it's been at least `seconds` since the timer started
	bool check(float deltaTime, float seconds) {
		update(deltaTime);
		return timeLeft >= seconds;
	}
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

    dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;
    std::vector<std::vector<StepData>> steps;
	Timer triggerTimer;
    std::vector<StepData> lastValues;
    int currentStep = 0;
    std::string text;
    bool dirty = false;
    bool fullyInitialized = false;
    
	Spellbook() : lastValues(16, {0.0f, 'N'}) {  // Initialize lastValues with 16 zeros
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configInput(CLOCK_INPUT, "Clock / Next Step");
        configInput(RESET_INPUT, "Reset");
        configOutput(POLY_OUTPUT, "16 voltages from columns");

        for (int i = 0; i < 16; ++i) {
            configOutput(OUT01_OUTPUT + i, "Column " + std::to_string(i + 1));
            outputs[OUT01_OUTPUT + i].setVoltage(0.0f);
        }
        outputs[POLY_OUTPUT].setChannels(16);
        fullyInitialized = true;
    }

    void onReset() override {
        dirty = true;
    }

	void fromJson(json_t* rootJ) override {
		Module::fromJson(rootJ);
		// In <1.0, module used "text" property at root level.
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

	// Map to convert note names into semitone offsets relative to C4
	std::map<std::string, int> noteToSemitone = {
		{"C", 0}, {"C#", 1}, {"Db", 1}, {"D", 2}, {"D#", 3}, {"Eb", 3},
		{"E", 4}, {"F", 5}, {"F#", 6}, {"Gb", 6}, {"G", 7}, {"G#", 8},
		{"Ab", 8}, {"A", 9}, {"A#", 10}, {"Bb", 10}, {"B", 11}
	};

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
				int octave = 4;
				if (tryParseOctave(octavePart, octave)) {
					return noteNameToVoltage(noteName, octave);
				} else {
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
			std::vector<StepData> stepData(16, StepData{0.0f, 'E'});  // Default all steps to 0.0 volts, empty type
			std::istringstream lineStream(line);
			std::string cell;
			int index = 0;
			while (getline(lineStream, cell, ',') && index < 16) {
				size_t commentPos = cell.find('?');
				if (commentPos != std::string::npos) {
					cell = cell.substr(0, commentPos);  // Remove the comment part
				}
				std::transform(cell.begin(), cell.end(), cell.begin(),
							   [](unsigned char c) { return std::toupper(c); });  // Convert to upper case
				cell.erase(std::remove_if(cell.begin(), cell.end(), ::isspace), cell.end());  // Clean cell from spaces
				
				if (!cell.empty()) {
					if (cell == "X") {
						stepData[index].voltage = 10.0f;  // Treat 'X' as a gate signal (10 volts)
						stepData[index].type = 'G';  // Gate
					} else if (cell == "T") {
						stepData[index].voltage = 10.0f;
						stepData[index].type = 'T';  // 1ms Trigger signal
					} else if (cell == "R") {
						stepData[index].voltage = 10.0f;
						stepData[index].type = 'R';  // Retrigger signal (0 for 10ms at start of step)
					} else if (isDecimal(cell)) {
						stepData[index].voltage = std::stof(cell);
						stepData[index].type = 'N';
					} else {
						stepData[index].voltage = parsePitch(cell);
						stepData[index].type = 'N';
					}
				}
					
				index++;
			}
			steps.push_back(stepData);
		}

		if (steps.empty()) {
			steps.push_back(std::vector<StepData>(16, StepData{0.0f, 'N'}));
		}

		currentStep = currentStep % steps.size();
	}

    void process(const ProcessArgs& args) override {
        if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
            currentStep = 0;
            triggerTimer.reset();
            dirty = true;
        }

        if (dirty) {
            parseText();
            dirty = false;
        }

        if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
            currentStep = (currentStep + 1) % steps.size();
            triggerTimer.reset();  // Reset timer at new step
        }

        outputs[POLY_OUTPUT].setChannels(16);
        std::vector<StepData>& currentValues = steps.empty() ? lastValues : steps[currentStep];

        for (int i = 0; i < 16; i++) {
            StepData& step = currentValues[i];
            float outputValue = step.voltage;  // Start with the last known voltage

            switch (step.type) {
                case 'T':  // Trigger
                    if (triggerTimer.check(args.sampleTime, 0.002f)) {
                        outputValue = 0.0f;  // Output zero if it's been more than 2ms
                    } else if (triggerTimer.check(args.sampleTime, 0.001f)) {
                        outputValue = 10.0f;  // Otherwise, output 10v if it's been at least 1ms
                    } else {
                        outputValue = 0.0f;  // Otherwise, output 0v
                    }
                    break;
                case 'R':  // Retrigger
                    if (!triggerTimer.check(args.sampleTime, 0.001f)) {
                        outputValue = 00.0f;  // Keep low for the first 1ms
                    } else {
                        outputValue = 10.0f;  // Output 10V thereafter
                    }
                    break;
                case 'N':  // Normal pitch or CV
                    //if (step.voltage != 0.0f || step.voltage == lastValues[i].voltage) {
                        outputValue = step.voltage;  // Normal pitch
                    //}
                    break;
                case 'E':  // Empty cells, check to see if we need to end a gate
                    if (lastValues[i].type == 'G' || lastValues[i].type == 'T' || lastValues[i].type == 'R' ) {
                        outputValue = 0.0f;  // Zero on empty cells, if last value was a gate or trigger
                    } else {
						outputValue = lastValues[i].voltage; // Otherwise continue last voltage
					}
                    break;
                default:
                    // If there's no specific type and the cell was empty, keep last voltage (do nothing)
                    break;  // Leave outputValue as last known voltage if not explicitly set
            }

            outputs[OUT01_OUTPUT + i].setVoltage(outputValue);
            outputs[POLY_OUTPUT].setVoltage(outputValue, i);
            lastValues[i].voltage = outputValue;  // Update last known values
            lastValues[i].type = step.type;       // Update last known type
        }
    }
};

struct StepIndicatorField : LedDisplayTextField {
    Spellbook* module;

    StepIndicatorField() {
        this->multiline = true;  // Allow multiple lines
        this->color = nvgRGB(255, 215, 0);  // Gold text color
    }

    void updateStepText() {
        if (!module) return;

        int totalSteps = module->steps.size();
        int maxDigits = std::to_string(totalSteps).length();  // Maximum digits in the largest step number

        std::ostringstream oss;
        for (int i = 0; i < totalSteps; i++) {
            std::string stepNumber = std::to_string(i + 1);  // Convert step number to string
            std::string paddedStepNumber = std::string(maxDigits - stepNumber.length(), ' ') + stepNumber;  // Pad with spaces

            if (i == module->currentStep) {
                oss << ">" << paddedStepNumber << "|\n";  // Mark the current step
            } else {
                oss << " " << paddedStepNumber << "|\n";
            }
        }
        this->setText(oss.str());
    }

    void step() override {
        Widget::step();
        updateStepText();  // Update the step numbers every frame
    }
};

struct SpellbookTextField : LedDisplayTextField {
    Spellbook* module;
	float textHeight;
	StepIndicatorField* stepField;
	float minY = 0.0f, maxY = 0.0f;
	const float lineHeight = 12.0f;

/* 	int getTextPosition(math::Vec mousePos) override {
		return bndTextFieldTextPosition(APP->window->vg, 0, 0, box.size.x, box.size.y, -1, text.c_str(), mousePos.x, mousePos.y);
		// Replace this function with something better for our purposes with all the weird text manipulation- all this functions needs to do is somehow decide which position in the text string to return as being "under" the mouse cursor, in a way that aligns with the user's expectations as they visually interact with the module.
		// We know we're using nanoVG to draw, we know the box of the widget, and we know the teftOffset. We should be able to properly request the actual line height and character width in pxiels that we need from somewhere, instead of guessing we like we are now.
	} */

    SpellbookTextField() {
        this->multiline = true;  // Allow multiple lines
		this->color = nvgRGB(255, 215, 0);  // Gold text color
		//this->box.pos = Vec(0, 0);
		//this->box.size = Vec(0, RACK_GRID_HEIGHT); // Initial height
    }
	
    void setScrollLimits(float contentHeight, float viewportHeight) {
		maxY = 0.0f;  // Top edge can never move down past 0
        if (contentHeight > viewportHeight) {
            minY = viewportHeight - contentHeight;  // Content is taller, allow scrolling up
        } else {
            minY = 0.0f;  // Content is shorter, no need to scroll
        }
    }
	
    void onHoverScroll(const event::HoverScroll &e) override {
        Widget::onHoverScroll(e);
        float delta = e.scrollDelta.y * 1.0f; // Adjust scroll speed if necessary
        float newY = clamp(textOffset.y + delta, minY, maxY);
		// Offset the text
		//box.pos.y = newY;
		textOffset.y = newY;
		// Offset stepField to match
		if (stepField) {
			//stepField->box.pos.y = newY;
			stepField->textOffset.y = newY;
		}
        e.consume(this);
    }
	
	
    void updateSizeAndOffset() {
        std::string text = getText();
        size_t lineCount = std::count(text.begin(), text.end(), '\n') + 1;
        float contentHeight = lineCount * lineHeight;
        
        textHeight = contentHeight;
		
		// Refresh scrolling
		setScrollLimits(contentHeight, RACK_GRID_HEIGHT);
    }

	void onDeselect(const DeselectEvent& e) override {
		std::string originalText = getText();
		std::string cleanedText = cleanAndPadText(originalText);
		
		if (module) {
			module->text = cleanedText;
			module->dirty = true;
		}

		setText(cleanedText);  // This should also trigger the widget to update its display
		updateSizeAndOffset();
		//updateSizeAndPosition();  // Update size and position after text update
	}

	std::string cleanAndPadText(const std::string& originalText) {
		std::istringstream ss(originalText);
		std::string line;
		std::vector<std::vector<std::string>> rows;
		std::vector<size_t> columnWidths;

		size_t maxColumns = 0;

		// First pass: fill rows and find maximum column widths and the maximum number of columns
		while (std::getline(ss, line)) {
			std::istringstream lineStream(line);
			std::string cell;
			std::vector<std::string> cells;
			size_t columnIndex = 0;

			while (std::getline(lineStream, cell, ',')) {
				cell.erase(cell.find_last_not_of(" \n\r\t") + 1); // Trim trailing whitespace
				cell.erase(0, cell.find_first_not_of(" \n\r\t")); // Trim leading whitespace
				cells.push_back(cell);

				if (columnIndex >= columnWidths.size()) {
					columnWidths.push_back(cell.size());
				} else {
					columnWidths[columnIndex] = std::max(columnWidths[columnIndex], cell.size());
				}
				columnIndex++;
			}
			maxColumns = std::max(maxColumns, cells.size());
			rows.push_back(cells);
		}

		// Normalize the number of columns in all rows
		for (auto& row : rows) {
			while (row.size() < maxColumns) {
				row.push_back("");  // Add empty strings for missing columns
			}
		}

		// Second pass: construct the cleaned text with proper padding and commas
		std::string cleanedText;
		for (auto& row : rows) {
			for (size_t i = 0; i < row.size(); ++i) {
				cleanedText += row[i];
				if (i < row.size() - 1) {
					// Pad with spaces if not the last column
					cleanedText += std::string(columnWidths[i] - row[i].size(), ' ');
					cleanedText += ", ";
				} else {
					// Ensure even the last column in each row is right-padded if necessary
					if (row.size() < columnWidths.size()) {
						cleanedText += std::string(columnWidths[i] - row[i].size(), ' ');
					}
				}
			}
			cleanedText += '\n';
		}

		return cleanedText;
	}
};

struct SpellbookWidget : ModuleWidget {
	SpellbookWidget(Spellbook* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/spellbook.svg")));
		
		// GRID_SNAP is a 2hp grid; 10.16mm.
		// Module is 48hp wide, with 4hp of space on the left side and and right sides for ports
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*1.5)), module, Spellbook::CLOCK_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*3)), module, Spellbook::RESET_INPUT));
		
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*22.5, GRID_SNAP*1)), module, Spellbook::POLY_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*22, GRID_SNAP*2)), module, Spellbook::OUT01_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*23, GRID_SNAP*2)), module, Spellbook::OUT09_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*22, GRID_SNAP*3)), module, Spellbook::OUT02_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*23, GRID_SNAP*3)), module, Spellbook::OUT10_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*22, GRID_SNAP*4)), module, Spellbook::OUT03_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*23, GRID_SNAP*4)), module, Spellbook::OUT11_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*22, GRID_SNAP*5)), module, Spellbook::OUT04_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*23, GRID_SNAP*5)), module, Spellbook::OUT12_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*22, GRID_SNAP*6)), module, Spellbook::OUT05_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*23, GRID_SNAP*6)), module, Spellbook::OUT13_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*22, GRID_SNAP*7)), module, Spellbook::OUT06_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*23, GRID_SNAP*7)), module, Spellbook::OUT14_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*22, GRID_SNAP*8)), module, Spellbook::OUT07_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*23, GRID_SNAP*8)), module, Spellbook::OUT15_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*22, GRID_SNAP*9)), module, Spellbook::OUT08_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP*23, GRID_SNAP*9)), module, Spellbook::OUT16_OUTPUT));
		
		
        // Main text field for patch notes
        SpellbookTextField* textField = createWidget<SpellbookTextField>(mm2px(Vec(GRID_SNAP*4, 0)));
        textField->setSize(Vec(mm2px(GRID_SNAP*17), RACK_GRID_HEIGHT));
        textField->module = module;
        addChild(textField);

        // Step indicator field
        StepIndicatorField* stepField = createWidget<StepIndicatorField>(Vec(mm2px(GRID_SNAP*2),0));
        stepField->setSize(Vec(mm2px(GRID_SNAP*2), RACK_GRID_HEIGHT));
        stepField->module = module;
		textField->stepField = stepField; // Give textField a reference to stepField
        addChild(stepField);

        // Ensure text field is populated with current module text
        if (module) {
            textField->setText(module->text);
        }
		
		textField->updateSizeAndOffset();
    }
};

Model* modelSpellbook = createModel<Spellbook, SpellbookWidget>("Spellbook");