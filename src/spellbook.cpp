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
	
	// Check whether it's been at least <seconds> since the timer started
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
		INDEX_INPUT,
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
	Timer triggerTimer; // General purpose stopwatch, used by Triggers and Retriggers
	Timer resetIgnoreTimer; // Timer to ignore Clock input shortly after Reset
    std::vector<StepData> lastValues;
    int currentStep = 0;
    std::string text = "0 ?Col1, 0 ?Col2, 0 ?Col3, 0 ?Col4\n\n\n\n";
    bool dirty = false;
    bool fullyInitialized = false;
    
	Spellbook() : lastValues(16, {0.0f, 'N'}) {  // Initialize lastValues with 16 zeros
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configInput(CLOCK_INPUT, "Clock / Next Step");
        configInput(RESET_INPUT, "Reset");
		configInput(INDEX_INPUT, "Index");
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
		resetIgnoreTimer.set(0.01); // Set the timer to ignore clock inputs for 10ms after reset
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
	// b (flat) , ♭ (flat), ♯ (sharp), # (sharp)
    std::vector<std::pair<std::string, int>> noteToSemitone = {
		{"C#", 1}, {"Db", 1}, {"D#", 3}, {"Eb", 3}, {"F#", 6}, {"Gb", 6}, {"G#", 8}, {"Ab", 8}, {"A#", 10}, {"Bb", 10},
        {"C♯", 1}, {"D♭", 1}, {"D♯", 3}, {"E♭", 3}, {"F♯", 6}, {"G♭", 6}, {"G♯", 8}, {"A♭", 8}, {"A♯", 10}, {"B♭", 10},
		{"B", 11}, {"D", 2}, {"G", 7}, {"E", 4}, {"F", 5}, {"A", 9}, {"C", 0}
    };

	// Converts a note name and octave to a voltage based on Eurorack 1V/oct standard
	float noteNameToVoltage(const std::string& noteName, int octave) {
		for (const auto& notePair : noteToSemitone) {
			if (notePair.first == noteName) {
				int semitoneOffsetFromC4 = notePair.second + (octave - 4) * 12;
				return static_cast<float>(semitoneOffsetFromC4) / 12.0f;
			}
		}
		return 0.0f;  // Return 0.0 volts if the noteName is not found (optional: handle this case more gracefully)
	}
	
	// Parses pitch from a cell with various formats
	float parsePitch(const std::string& cell) {
		if (cell.empty()) {
			return 0.0f;  // Return default voltage for empty cells
		}

		// Handling for semitone offset input (e.g., "S7" should be interpreted as 7 semitones above C4)
		if (cell[0] == 'S') {
			try {
				float semitoneOffset = std::stof(cell.substr(1));
				return semitoneOffset / 12.0f;  // Convert semitone offset to voltage
			} catch (...) {
				return 0.0f;  // Return default voltage if parsing fails
			}
		}

		// Handling for MIDI note number input (e.g., "M60" is MIDI note number 60, equivalent to C4)
		if (cell[0] == 'M') {
			try {
				float midiNoteNumber = std::stof(cell.substr(1));
				return (midiNoteNumber - 60) / 12.0f;  // Convert MIDI note number to voltage, offset by C4 (MIDI 60)
			} catch (...) {
				return 0.0f;  // Return default voltage if parsing fails
			}
		}

		// Handling for percentage-based input (e.g., "100%" should convert to 10.0 volts)
		if (cell.back() == '%') {
			try {
				float percentage = std::stof(cell.substr(0, cell.size() - 1));
				return percentage / 10.0f;  // Convert percentage to voltage
			} catch (...) {
				return 0.0f;  // Return default voltage if parsing fails
			}
		}

		// Parse note name and octave (e.g., "C4")
        for (const auto& notePair : noteToSemitone) {
            const std::string& noteName = notePair.first;
            if (cell.rfind(noteName, 0) == 0) { // Ensure the cell starts with the note name
                std::string octavePart = cell.substr(noteName.length());
                int octave = 4; // Default octave
                if (tryParseOctave(octavePart, octave)) {
                    return noteNameToVoltage(noteName, octave);
                } else {
                    return noteNameToVoltage(noteName, 4); // Fallback if octave parsing fails
                }
            }
        }

		// If no format is matched, assume it's a decimal voltage value directly
		if (isDecimal(cell)) {
			return std::stof(cell);
		}

		return 0.0f;  // Default value if no format matches and parsing fails
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
					if (cell == "G" || cell == "X" || cell == "|") {
						stepData[index].voltage = 10.0f;  // Treat 'X' as a gate signal (10 volts)
						stepData[index].type = 'G';  // Gate
					} else if (cell == "T" || cell == "^") {
						stepData[index].voltage = 10.0f;
						stepData[index].type = 'T';  // 1ms Trigger signal
					} else if (cell == "R" || cell == "_") {
						stepData[index].voltage = 10.0f;
						stepData[index].type = 'R';  // Retrigger signal (0 for 10ms at start of step)
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
			currentStep = 0;  // Reset the current step index to 0
			triggerTimer.reset();  // Reset the timer
			resetIgnoreTimer.reset(); // Reset the post-reset-clock-ignore period
			dirty = true;  // Mark the state as needing re-evaluation
		}
		
		bool resetHigh = inputs[RESET_INPUT].getVoltage() >= 5.0f;
		bool ignoreClock = !resetIgnoreTimer.check(args.sampleTime, 0.01f);

		if (dirty) {
			parseText();  // Reparse the text into steps
			dirty = false;
			if (steps.empty()) return;  // If still empty after parsing, skip processing
		}

		if (steps.empty()) return;  // Ensure not to proceed if there are no steps

/* 		if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
			if (!steps.empty()) {  // Only process clock input if there are steps defined
				currentStep = (currentStep + 1) % steps.size();
				triggerTimer.reset();  // Reset timer at new step
			}
		} */
		
		if (!inputs[INDEX_INPUT].isConnected() && !resetHigh && !ignoreClock && !steps.empty()) {
			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
				currentStep = (currentStep + 1) % steps.size();
				triggerTimer.reset();
			}
		} else if (inputs[INDEX_INPUT].isConnected()) {
			float indexVoltage = inputs[INDEX_INPUT].getVoltage();
			int numSteps = steps.size();
			int lastStep = currentStep;
			currentStep = clamp((int)(indexVoltage / 10.0f * numSteps), 0, numSteps - 1);
			if (currentStep != lastStep) {
				triggerTimer.reset();
			}
		}

		outputs[POLY_OUTPUT].setChannels(16);
		std::vector<StepData>& currentValues = steps[currentStep];

		for (int i = 0; i < 16; i++) {
			StepData& step = currentValues[i];
			float outputValue = step.voltage;  // Start with the last known voltage

			switch (step.type) {
				case 'T':  // Trigger
					if (triggerTimer.check(args.sampleTime, 0.002f)) {
						outputValue = 0.0f;
					} else if (triggerTimer.check(args.sampleTime, 0.001f)) {
						outputValue = 10.0f;
					} else {
						outputValue = 0.0f;
					}
					break;
				case 'R':  // Retrigger
					if (!triggerTimer.check(args.sampleTime, 0.001f)) {
						outputValue = 0.0f;
					} else {
						outputValue = 10.0f;
					}
					break;
				case 'N':  // Normal pitch or CV
					outputValue = step.voltage;
					break;
				case 'E':  // Empty cells
					outputValue = (lastValues[i].type == 'G' || lastValues[i].type == 'T' || lastValues[i].type == 'R') ? 0.0f : lastValues[i].voltage;
					break;
				default:
					break;
			}

			outputs[OUT01_OUTPUT + i].setVoltage(outputValue);
			outputs[POLY_OUTPUT].setVoltage(outputValue, i);
			lastValues[i].voltage = outputValue;
			lastValues[i].type = step.type;
		}
	}
};

struct SpellbookTextField : LedDisplayTextField {
    Spellbook* module;
    float textHeight;
    float minY = 0.0f, maxY = 0.0f; // Vertical scroll limits
    const float lineHeight = 12.0f;
    math::Vec mousePos;  // To track the mouse position within the widget
    int lastTextPosition = 0; // To store the last calculated text position for display in the debug info
    float lastMouseX = 0.0f, lastMouseY = 0.0f; // To store the exact mouse coordinates passed to the text positioning function
	bool focused = false;

    SpellbookTextField() {
        this->color = nvgRGB(255, 215, 0);  // Gold text color
        this->textOffset = Vec(0,0);
		//this->fontPath = asset::plugin(pluginInstance, "/res/dum1thin.ttf");
    }
	
	void scrollToCursor() {
		std::string text = getText();
		int cursorLine = 0;
		for (size_t i = 0; i < (size_t)cursor; ++i) {
			if (text[i] == '\n') {
				cursorLine++;
			}
		}
		textOffset.y = clamp(-(cursorLine * lineHeight - box.size.y / 2 + lineHeight / 2), minY, maxY);
	}
	
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1 || !module) return;  // Only draw on the correct layer, and only if active
		
		if (!focused) {
			// Autoscroll logic
			float targetY = -(module->currentStep * lineHeight - box.size.y / 2 + lineHeight / 2);
			textOffset.y = clamp(targetY, minY, maxY);
		}

		// Extend the scissor box to include the gutter area
		nvgScissor(args.vg, args.clipBox.pos.x - GRID_SNAP * 5, args.clipBox.pos.y, 
				   args.clipBox.size.x + GRID_SNAP * 5, args.clipBox.size.y);

		// Configure font
		std::shared_ptr<window::Font> font = APP->window->loadFont(fontPath);
		if (!font) return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 12);  // Example font size
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

		// Brute force a 12px by 6px grid.
		float lineHeight = 12;
		float charWidth = 6;

		// Variables for text drawing
		float x = textOffset.x;  // Horizontal text start - typically a small indent
		float y = textOffset.y;  // Vertical scroll offset
		std::string text = getText();
		std::istringstream lines(text);
		std::string line;
		int currentPos = 0;  // Current character position in the overall text
		int selectionStart = std::min(cursor, selection);
		int selectionEnd = std::max(cursor, selection);

		int lineIndex = 0;  // Line index to match with steps
		
		if (focused) {		
			// Draw an all-black backdrop, with plenty of bleed
			nvgBeginPath(args.vg);
			nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 200));
			nvgRect(args.vg, 0, 0 - lineHeight*4, box.size.x, box.size.y + lineHeight*4);
			nvgFill(args.vg);
		} else {
			// Draw column backgrounds
			std::getline(lines, line); // Assume the first line gives the column layout
			std::vector<float> columnWidths;
			size_t startPos = 0;
			while (startPos < line.length()) {
				size_t nextComma = line.find(',', startPos);
				if (nextComma == std::string::npos) {
					nextComma = line.length();
				}
				size_t columnLength = nextComma - startPos + 1; // Include comma space in the width calculation
				columnWidths.push_back(columnLength * charWidth);
				startPos = nextComma + 1; // Skip comma
			}

			float columnStart = x;
			float totalWidth = 0;
			for (size_t i = 0; i < columnWidths.size(); ++i) {
				nvgBeginPath(args.vg);
				nvgFillColor(args.vg, i % 2 == 0 ? nvgRGBA(0, 0, 0, 128) : nvgRGBA(16, 16, 16, 128));  // Alternate colors
				nvgRect(args.vg, columnStart, 0-lineHeight*4, columnWidths[i], box.size.y+lineHeight*4);
				nvgFill(args.vg);
				columnStart += columnWidths[i];
				totalWidth += columnWidths[i];
			}

			// Calculate remaining width and draw dummy column if there's remaining space
			float remainingWidth = box.size.x - totalWidth;
			if (remainingWidth > 0) {
				nvgBeginPath(args.vg);
				nvgFillColor(args.vg, (columnWidths.size() % 2 == 0) ? nvgRGBA(16, 16, 16, 128) : nvgRGBA(0, 0, 0, 128));  // Invert so the alternation so the last real column is "continued"
				nvgRect(args.vg, columnStart, 0-lineHeight*4, remainingWidth, box.size.y+lineHeight*4);
				nvgFill(args.vg);
			}

			// Reset stream to start drawing text
			lines.clear();
			lines.seekg(0, std::ios::beg);
		}

		// Draw each line of text
		NVGcolor textColor = nvgRGB(255, 215, 0); // Bright gold text
		NVGcolor commaColor = nvgRGB(155, 131, 0); // Dark gold commas
		NVGcolor commentColor = nvgRGB(158, 80, 191); // Purple comments
		NVGcolor activeColor = textColor;
		
		while (std::getline(lines, line)) {
			if (y + lineHeight < 0) {
				y += lineHeight;
				currentPos += line.size() + 1;
				lineIndex++;
				continue;
			}
			if (y > box.size.y+lineHeight) break;
			
			for (size_t i = 0; i < line.length(); ++i) {
				float charX = x + i * charWidth;  // X position of the character

				// Draw selection background for this character if within selection bounds
				if (static_cast<size_t>(currentPos + i) >= static_cast<size_t>(selectionStart) && static_cast<size_t>(currentPos + i) < static_cast<size_t>(selectionEnd)) {
					nvgBeginPath(args.vg);
					nvgFillColor(args.vg, nvgRGBA(73, 3, 103, 200));  // Selection color
					nvgRect(args.vg, charX, y, charWidth, lineHeight);
					nvgFill(args.vg);
				}

				// Draw the character
				
				char str[2] = {line[i], 0};  // Temporary string for character
				if (line[i] == ',') {
					nvgFillColor(args.vg, commaColor); // Dark gold commas
					nvgText(args.vg, charX, y, str, NULL); // draw the comma
					activeColor = textColor; // Reset to default color after commas
				} else {
					if (line[i] == '?') {
						activeColor = commentColor; // "Snap" to comment color if we encounter a ?, which will be rest after the enxt comma
					}
					nvgFillColor(args.vg, activeColor);
					nvgText(args.vg, charX, y, str, NULL); // draw the text
				}
			}
			activeColor = textColor; // Reset active color at the end of the line.

			// Draw cursor if within this line
			if (cursor >= currentPos && cursor < currentPos + (int)line.length() + 1) {
				float cursorX = x + (cursor - currentPos) * charWidth;
				nvgBeginPath(args.vg);
				nvgFillColor(args.vg, nvgRGB(158, 80, 191));  // Cursor color
				nvgRect(args.vg, cursorX, y, 1, lineHeight);
				nvgFill(args.vg);
			}

			// Draw step numbers in the gutter
			std::string stepNumber = std::to_string(lineIndex + 1) + "| ";
			if (module->currentStep == lineIndex) {
				stepNumber = "@> "+ stepNumber;
			}
			float stepTextWidth = nvgTextBounds(args.vg, 0, 0, stepNumber.c_str(), NULL, NULL);
			float stepX = -(GRID_SNAP * 5) + (GRID_SNAP * 5 - stepTextWidth);  // Right-align in gutter
			nvgFillColor(args.vg, (module->currentStep == lineIndex) ? nvgRGB(158, 80, 191) : nvgRGB(155, 131, 0));  // Current step in purple, others in gold
			nvgText(args.vg, stepX, y, stepNumber.c_str(), NULL);

			y += lineHeight;
			currentPos += line.length() + 1;
			lineIndex++;
		}

		nvgResetScissor(args.vg);
	}
	
	int getTextPosition(math::Vec mousePos) override {
		std::shared_ptr<window::Font> font = APP->window->loadFont(fontPath);
		if (!font || font->handle == -1)
			return 0;

		nvgFontFaceId(APP->window->vg, font->handle);
		nvgFontSize(APP->window->vg, 12);
		nvgTextAlign(APP->window->vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		
		// Brute force a 12px by 6px grid.
		float lineHeight = 12;
		float charWidth = 6;

		mousePos.x -= textOffset.x;
		mousePos.y -= textOffset.y;

		std::string text = getText();
		std::istringstream lines(text);
		std::string line;
		int textPosition = 0;
		float y = 0;

		while (std::getline(lines, line)) {
			if (mousePos.y < y) break;
			if (mousePos.y <= y + lineHeight) {
				int charIndex = (int)((mousePos.x) / charWidth);  // Calculate character index from x position
				charIndex = std::min(charIndex, (int)line.length());  // Clamp within line length
				return textPosition + charIndex;
			}
			y += lineHeight;
			textPosition += line.length() + 1;
		}

		return textPosition;
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
		textOffset.y = newY;
        e.consume(this);
    }
	
    void updateSizeAndOffset() {
        std::string text = getText();
        size_t lineCount = std::count(text.begin(), text.end(), '\n') + 1;
        float contentHeight = lineCount * lineHeight;
        
        textHeight = contentHeight;
		
		// Refresh scrolling
		setScrollLimits(contentHeight, box.size.y);
    }

	void onDeselect(const DeselectEvent& e) override {
		focused = false;
		cleanup();
		LedDisplayTextField::onDeselect(e);
	}
	
	void onSelect(const SelectEvent& e) override {
		focused = true;
		LedDisplayTextField::onSelect(e);
	}
	
	void cleanup() {
		std::string originalText = getText();
		std::string cleanedText = cleanAndPadText(originalText);
		
		if (module) {
			module->text = cleanedText;
			module->dirty = true;
		}

		setText(cleanedText);  // This should also trigger the widget to update its display
		updateSizeAndOffset();
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
	
	void onSelectKey(const SelectKeyEvent& e) override {
		if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
			if (e.key == GLFW_KEY_ENTER) {
				std::string text = getText();
				std::string beforeCursor = text.substr(0, cursor);
				std::string afterCursor = text.substr(cursor);
				setText(beforeCursor + "\n" + afterCursor);
				cursor = beforeCursor.length() + 1;  // Set cursor right after the new line

				// Ensure cursor does not go out of bounds
				if (cursor > text.length()) {
					cursor = text.length();
				}
				
				selection = cursor;  // Reset selection to cursor position
				module->dirty = true;

				// Recalculate text box scrolling
				updateSizeAndOffset();
				
				e.consume(this);
				return;
			} else if (e.key == GLFW_KEY_UP || e.key == GLFW_KEY_DOWN) {
				std::string text = getText();
				std::vector<int> lineBreaks = {-1};  // Start before the first line
				for (int i = 0; i < (int)text.length(); i++) {
					if (text[i] == '\n') lineBreaks.push_back(i);
				}
				lineBreaks.push_back(text.length());  // End after the last line

				int currentLine = 0;
				while (currentLine < (int)lineBreaks.size() - 1 && lineBreaks[currentLine + 1] < cursor) {
					currentLine++;
				}

				int lineStart = lineBreaks[currentLine] + 1;
				int posInLine = cursor - lineStart;

				if (e.key == GLFW_KEY_UP && currentLine > 0) {
					int prevLineStart = lineBreaks[currentLine - 1] + 1;
					int prevLineEnd = lineBreaks[currentLine];
					cursor = std::min(prevLineStart + posInLine, prevLineEnd);
				} else if (e.key == GLFW_KEY_DOWN && currentLine < (int)lineBreaks.size() - 2) {
					int nextLineStart = lineBreaks[currentLine + 1] + 1;
					int nextLineEnd = lineBreaks[currentLine + 2];
					cursor = std::min(nextLineStart + posInLine, nextLineEnd);
				}

				if (!(e.mods & GLFW_MOD_SHIFT)) {
					selection = cursor;
				}
				updateSizeAndOffset();  // Always update size and offset after moving cursor
				e.consume(this);
				return;
			}
		}
		scrollToCursor(); // Scroll to the cursor after any keypress
		LedDisplayTextField::onSelectKey(e);  // Delegate other keys to the base class
	}


    std::vector<std::string> split(const std::string &s, char delim) {
        std::vector<std::string> elems;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim)) {
            elems.push_back(item);
        }
        return elems;
    }
};

struct SpellbookWidget : ModuleWidget {
	SpellbookWidget(Spellbook* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/spellbook.svg")));
		
		// GRID_SNAP is a 2hp grid; 10.16mm.
		// Module is 48hp wide, with 4hp of space on the left side and and right sides for ports
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*1.5)), module, Spellbook::CLOCK_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*3.0)), module, Spellbook::RESET_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*4.5)), module, Spellbook::INDEX_INPUT));
		
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
		// GRID_SNAP is derived from RACK_GRID_WIDTH, which is 1hp. One GRID_SNAP is 2hp in milimeters.
        SpellbookTextField* textField = createWidget<SpellbookTextField>(mm2px(Vec(GRID_SNAP*3, GRID_SNAP*0.25)));
        textField->setSize(Vec(mm2px(GRID_SNAP*18), RACK_GRID_HEIGHT-mm2px(GRID_SNAP*0.5)));
        textField->module = module;
        addChild(textField);

        // Step indicator field
/*         StepIndicatorField* stepField = createWidget<StepIndicatorField>(mm2px(Vec(GRID_SNAP*2, GRID_SNAP*0.25)));
        stepField->setSize(Vec(mm2px(GRID_SNAP*2), RACK_GRID_HEIGHT-mm2px(GRID_SNAP*0.5)));
        stepField->module = module;
		textField->stepField = stepField; // Give textField a reference to stepField
        addChild(stepField); */
		
        // Ensure text field is populated with current module text
        if (module) {
            textField->setText(module->text);
			textField->cleanup();
        }
		
    }
};

Model* modelSpellbook = createModel<Spellbook, SpellbookWidget>("Spellbook");