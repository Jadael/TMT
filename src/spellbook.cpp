#include "plugin.hpp"
#include "ports.hpp"
#include <sstream>
#include <vector>
#include <map>
#include <iomanip>
#define GRID_SNAP 10.16

struct StepData {
    float voltage;
    char type;  // 'N' for normal, 'T' for trigger, 'R' for retrigger, 'G' for gate, 'E' for empty
};

struct Timer {
	// There's probably something in dsp which could handle this better,
	// it was just easier to conceptualize as a simple "time since start of step" which I can check however I want
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
	std::vector<std::string> firstRowComments; // Fill in whenever we check row 1
	std::vector<std::string> currentStepComments; // Continually update as we go, but only if and when we encounter comments, so they're sticky
	Timer triggerTimer; // General purpose stopwatch, used by Triggers and Retriggers
	Timer resetIgnoreTimer; // Timer to ignore Clock input briefly after Reset triggers
    std::vector<StepData> lastValues;
    int currentStep = 0;
    std::string text = "0 ?Col1, 0 ?Col2, 0 ?Col3, 0 ?Col4\n\n\n\n"; // A default sequence that outputs four labelled 0s for 4 steps
    bool dirty = false;
    bool fullyInitialized = false;
	float lineHeight = 12;
    
	Spellbook() : lastValues(16, {0.0f, 'N'}) {  // Some RhythML commands act differently based on the prior voltage of each channel, so assume all 0s for "before time began"
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configInput(CLOCK_INPUT, "Clock / Next Step");
        configInput(RESET_INPUT, "Reset");
		configInput(INDEX_INPUT, "Index");
        configOutput(POLY_OUTPUT, "16 voltages from columns"); // This poly output will always be exactly 16 channels.
			// TODO: The compiler keeps complaining about array bounds, because we're basically just pinky-promising ourselves to never change the number of channels and a lot of loops just ASSUME 16 channels. Need to change something about how we distribute all the right values to all the right channels to avoid that awkwardness.

        for (int i = 0; i < 16; ++i) { 
            configOutput(OUT01_OUTPUT + i, "Column " + std::to_string(i + 1));
            outputs[OUT01_OUTPUT + i].setVoltage(0.0f);
        }
        outputs[POLY_OUTPUT].setChannels(16);
        fullyInitialized = true;
    }
	
	
	void updateLabels(std::vector<std::string> labels) {
		// Config the output using comments from Row 1 as labels:
		for (size_t i = 0; i < labels.size(); ++i) {
			configOutput(OUT01_OUTPUT + i, labels[i]);
		}
	}

    void onReset() override {
		resetIgnoreTimer.set(0.01); // Set the timer to ignore clock inputs for 10ms after reset
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
		json_object_set_new(rootJ, "lineHeight", json_real(lineHeight));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// Get text buffer
		json_t* textJ = json_object_get(rootJ, "text");
		if (textJ)
			text = json_string_value(textJ);
		
		// Get lineHeight (effectively text size / zoom level)
		json_t* lineHeightJ = json_object_get(rootJ, "lineHeight");
		if (lineHeightJ)
			lineHeight = json_number_value(lineHeightJ); 

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
	// TODO: Maybe changes to a syntax like <Note Letter>[0+ Accidentals]<Octave Number>, so we can count and handle double sharps like C##4, etc.?
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

		// Handling for semitone offset input (e.g., "S0" should become 0.0 / C4, "S7" should be interpreted as 7 semitones above C4, etc.)
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
					if (cell == "G" || cell == "|") {
						stepData[index].voltage = 10.0f;  // Treat 'X' as a gate signal (10 volts)
						stepData[index].type = 'G';  // Gate
					} else if (cell == "T" || cell == "^") {
						stepData[index].voltage = 10.0f;
						stepData[index].type = 'T';  // 1ms Trigger signal
					} else if (cell == "X" || cell == "R" || cell == "_") {
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
		// Advance the timers
		resetIgnoreTimer.update(args.sampleTime);
		triggerTimer.update(args.sampleTime);
		
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			currentStep = 0;  // Reset the current step index to 0
			triggerTimer.reset();  // Reset the timer
			resetIgnoreTimer.reset(); // Reset the post-reset-clock-ignore period
			dirty = true;  // Mark the state as needing re-evaluation
		}
		
		bool resetHigh = inputs[RESET_INPUT].getVoltage() >= 5.0f;
		bool ignoreClock = !resetIgnoreTimer.check(0.01f);

		if (dirty) {
			parseText();  // Reparse the text into steps
			dirty = false;
		}

		if (steps.empty()) return;  // If still empty after parsing, skip processing
		
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
					if (triggerTimer.check(0.002f)) {
						outputValue = 0.0f;
					} else if (triggerTimer.check(0.001f)) {
						outputValue = 10.0f;
					} else {
						outputValue = 0.0f;
					}
					break;
				case 'R':  // Retrigger
					if (!triggerTimer.check(0.001f)) {
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
	
	void overrideText(std::string newText) {
		// Update our text and trust the TextField to notice it
		text = newText;
		dirty = true;
	}
};

// Undo struct holding the two things (prior/next text state) it needs to be able to give back to the module
struct SpellbookUndoRedoAction : history::ModuleAction {
	std::string old_text;
	std::string new_text;

	SpellbookUndoRedoAction(int64_t id, std::string oldText, std::string newText) : old_text{oldText}, new_text{newText} {
		moduleId = id;
		name = "Spellbook text edit";
	}

	void undo() override {
		Spellbook *module = dynamic_cast<Spellbook*>(APP->engine->getModule(moduleId));
		if (module) {
			module->overrideText(this->old_text);
		}
	}

	void redo() override {
	Spellbook *module = dynamic_cast<Spellbook*>(APP->engine->getModule(moduleId));
		if (module) {
			module->overrideText(this->new_text);
		}
	}
};

struct SpellbookTextField : LedDisplayTextField {
    Spellbook* module; // Reference to the object which is is "the module"
    float textHeight; // Not sure if this is used, honestly. Probably does something in the TextField class.
    float minY = 0.0f, maxY = 0.0f; // Vertical scroll limits
    math::Vec mousePos;  // To track the mouse position within the widget
    int lastTextPosition = 0; // To store the last calculated text position for display in the debug info
    float lastMouseX = 0.0f, lastMouseY = 0.0f; // To store the exact mouse coordinates passed to the text positioning function
	bool focused = false; // Whether we're in text editing (focused) mode or playing mode
	
	// Brute force a 2:1 monospaced grid.
    float lineHeight = 12.0f; // This also gets used as the font size
	float charWidth = lineHeight*0.5; // Text is almost always drawn character by character, stepping by this amount
	
    SpellbookTextField() {
        this->textOffset = Vec(0,0);
    }
	
	void scrollToCursor() {
		std::string text = getText();
		int cursorLine = 0;
		int cursorPos = 0;
		int maxLineLength = 0;
		for (size_t i = 0; i < (size_t)cursor; ++i) {
			cursorPos++;
			if (text[i] == '\n') {
				cursorLine++;
				if (cursorPos > maxLineLength) maxLineLength = cursorPos;
				cursorPos = 0;
			}
		}
		// Cursor position relative to box
		float cursorY = cursorLine * lineHeight;
		float cursorX = cursorPos * charWidth;
		
		// Only scroll vertically if the cursor would be out of view (or we're in 'playing' mode), to minimize jumpiness
		if (cursorY+textOffset.y < 0 || cursorY+textOffset.y > box.size.y || !focused) {
			textOffset.y = clamp(-(cursorY - box.size.y * 0.5 + lineHeight * 0.5), minY, maxY);
		}
		// Handling axes independantly feels nicer
		// Only scroll horizontally if the cursor would be out of view, to minimize jumpiness
		if (cursorX+textOffset.x < 0 || cursorX+textOffset.x > box.size.x) {
			textOffset.x = clamp( -(cursorX - box.size.x * 0.5 + charWidth), -(maxLineLength * charWidth), 0.f);
		}
	}
	
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1 || !module) return;  // Only draw on the correct layer, and only if active
				
		if (!focused) {
			// Autoscroll logic
			float targetY = -(module->currentStep * lineHeight - box.size.y / 2 + lineHeight / 2);
			textOffset.y = clamp(targetY, minY, maxY);
			
			// Check for fresh text in the module, such as from an undo, and bring it in as if the user had typed it in
			if (text != module->text) {
				setText(module->text);
				cleanAndPublishText();
			}
		}

		// Make sure the scissor matches our box... for now.
		nvgScissor(args.vg, args.clipBox.pos.x, args.clipBox.pos.y, args.clipBox.size.x, args.clipBox.size.y);

		// Configure font
		std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/Hack-Regular.ttf"));
		if (!font) { // Use app font as a backup
			std::shared_ptr<window::Font> font = APP->window->loadFont(fontPath);
		}
		if (!font) return;
		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

		// Brute force a 12px by 6px grid.
		//float lineHeight = 14;
		//float charWidth = 7;

		// Variables for text drawing
		float x = textOffset.x;  // Horizontal text start - typically a small indent
		float y = textOffset.y;  // Vertical scroll offset
		std::string text = getText();
		text += "\n"; // Add back in a trailing newline for drawing, else std::string gets confused about line count
		std::istringstream lines(text);
		std::string line;
		int currentPos = 0;  // Current character position in the overall text
		int selectionStart = std::min(cursor, selection);
		int selectionEnd = std::max(cursor, selection);

		int lineIndex = 0;  // Line index to match with steps
		
		if (focused) {		
			// Draw an all-black backdrop, with plenty of bleed
			nvgBeginPath(args.vg);
			nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 140));
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
				nvgFillColor(args.vg, i % 2 == 0 ? nvgRGBA(0, 0, 0, 140) : nvgRGBA(16, 16, 16, 140));  // Alternate colors
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
		NVGcolor commentCharColor = nvgRGB(121, 8, 170); // Dark purple for `?`
		NVGcolor selectionColor = nvgRGB(39, 1, 52); // Darkest purple for selection highlight
		NVGcolor currentStepColor = nvgRGB(255, 255, 255); // White current step when autoscrolling
		NVGcolor cursorColor = nvgRGBA(158, 80, 191,192); // Light translucent purple for cursor
		NVGcolor lineColor = textColor;
		NVGcolor activeColor = textColor;
		
		while (std::getline(lines, line)) {
			nvgFontSize(args.vg, lineHeight);  // Brute force match lineHeight
			
			if (y + lineHeight < 0) {
				y += lineHeight;
				currentPos += line.size() + 1;
				lineIndex++;
				continue;
			}
			
			if (y > box.size.y+lineHeight*2) {
				break; // Stop once we've drawn two extra lines.
			}
			
			// Use brighter color if current step and defocused (playing)
			if (module->currentStep == lineIndex && !focused) {
				lineColor = currentStepColor;
			} else {
				lineColor = textColor;
			}
			
			activeColor = lineColor;
			
			for (size_t i = 0; i < line.length(); ++i) {
				float charX = x + i * charWidth;  // X position of the character
				
				// If focused, draw selection background for this character if within selection bounds
				if (focused && static_cast<size_t>(currentPos + i) >= static_cast<size_t>(selectionStart) && static_cast<size_t>(currentPos + i) < static_cast<size_t>(selectionEnd)) {
					nvgBeginPath(args.vg);
					nvgFillColor(args.vg, selectionColor);  // Selection color
					nvgRect(args.vg, charX+0.5, y+0.5, charWidth-1, lineHeight-1);
					nvgFill(args.vg);
				}

				// Draw the character
				char str[2] = {line[i], 0};  // Temporary string for character
				if (line[i] == ',') {
					nvgFillColor(args.vg, commaColor); // Dark gold commas
					nvgText(args.vg, charX, y, str, NULL); // draw the comma
					activeColor = lineColor; // Reset to line color after commas
				} else {
					if (line[i] == '?') {
						nvgFillColor(args.vg, commentCharColor); // Set the (maybe new) activeColor
						nvgText(args.vg, charX, y, str, NULL); // draw the character
						activeColor = commentColor; // "Snap" to comment color after a ?, which will be untouched until after the next comma
					} else {
						nvgFillColor(args.vg, activeColor); // Set the (maybe new) activeColor
						nvgText(args.vg, charX, y, str, NULL); // draw the character
					}
				}
			}
			// Reset active and line color at the end of the line.
			activeColor = textColor;
			lineColor = textColor;

			// If focused, draw cursor if within this line
			if (focused && cursor >= currentPos && cursor < currentPos + (int)line.length() + 1) {
				float cursorX = x + (cursor - currentPos) * charWidth;
				nvgBeginPath(args.vg);
				nvgFillColor(args.vg, cursorColor); 
				nvgRect(args.vg, cursorX, y, charWidth*0.125f, lineHeight);
				nvgFill(args.vg);
			}

			// Extend the scissor box into a gutter area
				// Kinda rude, this should probably just be an area within this widget's box, but it does mean you can think of the x/y coordinates as belonging to the TEXT, ignoring the step labels.
			nvgScissor(args.vg, args.clipBox.pos.x - GRID_SNAP * 4, args.clipBox.pos.y, 
					   args.clipBox.size.x + GRID_SNAP * 4, args.clipBox.size.y);

			// Draw step numbers in the gutter
			std::string stepNumber = std::to_string(lineIndex + 1)+"┃";
			if (module->currentStep == lineIndex) {
				stepNumber = " "+ stepNumber;
			}
			//float stepSize = std::min(lineHeight,14.f);
			//float centerOffset = stepSize / lineHeight;
			float stepSize = std::min(lineHeight,14.f);
			float stepY = 0 + (lineHeight - stepSize)*0.5;
			nvgFontSize(args.vg, stepSize);  // step numbers max out at a smaller size or it looks bad
			//TODO: make littler labels center to their bigger row 
			float stepTextWidth = nvgTextBounds(args.vg, 0, 0, stepNumber.c_str(), NULL, NULL); // This ends up averaging their widths to get back to monospace
			float stepX = -stepTextWidth - 2;  // Right-align in gutter, with constant padding
			nvgFillColor(args.vg, (module->currentStep == lineIndex) ? nvgRGB(158, 80, 191) : nvgRGB(155, 131, 0));  // Current step in purple, others in gold
			nvgText(args.vg, stepX, y+stepY, stepNumber.c_str(), NULL);
			
			// Back out of the gutter
			nvgScissor(args.vg, args.clipBox.pos.x, args.clipBox.pos.y, args.clipBox.size.x, args.clipBox.size.y);

			y += lineHeight;
			currentPos += line.length() + 1;
			lineIndex++;
		}

		nvgResetScissor(args.vg);
	}
	
	int getTextPosition(math::Vec mousePos) override {
		// The Core TextFIeld class tries to actually draw some text to a box so it can decide where all the characters are, but since we brute force a fixed-width grid for characters we just re-use that.

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
		textPosition = clamp(textPosition,0,text.length());
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
        size_t lineCount = std::count(text.begin(), text.end(),'\n')+1; // Need the extra 1 because we don't have a trailing newline
        float contentHeight = lineCount * lineHeight;
        
        textHeight = contentHeight;
		
		// Refresh scrolling
		setScrollLimits(contentHeight, box.size.y);
    }

	void onDeselect(const DeselectEvent& e) override {
		focused = false;
		if (module) {
			std::string priorText = module->text;
			cleanAndPublishText();
			if (text != priorText) {
				// Push an undo action if we made a real change (post cleaning)
				APP->history->push( new SpellbookUndoRedoAction(module->id, priorText, text) );
			}
		}
		LedDisplayTextField::onDeselect(e);
	}
	
	void onSelect(const SelectEvent& e) override {
		focused = true;
		// Ensure cursor is not out of bounds
		if (cursor > (int)text.length()) {
			cursor = (int)text.length()-1;
		} else if (cursor < 0) {
			cursor = 0;
		}
		LedDisplayTextField::onSelect(e);
		// Can't scrollToCursor() here, because you might move while the mouse button is pressed and make a selection.
	}
	
	void cleanAndPublishText() {
		std::string cleanedText = cleanAndPadText(getText());
		
		if (module) {
			module->text = cleanedText;
			module->dirty = true;
		}
		setText(cleanedText);  // Make sure to update the text within this widget too
			// This happens whether or not we successfully updated a module, but we don't exist otherwise, so that's okay?
		updateSizeAndOffset();
	}

	std::string cleanAndPadText(const std::string& originalText) {
		std::istringstream ss(originalText);
		std::string line;
		std::vector<std::vector<std::string>> rows;
		std::vector<size_t> columnWidths;
		std::vector<std::string> columnLabels;
		bool firstRow = true;

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
				
				size_t commentStart = cell.find('?');
				std::string comment;
				if (commentStart != std::string::npos) {
					comment = cell.substr(commentStart + 1);
				}

				if (columnIndex >= columnWidths.size()) {
					columnWidths.push_back(cell.size());
				} else {
					columnWidths[columnIndex] = std::max(columnWidths[columnIndex], cell.size());
				}
				
				if (firstRow && !comment.empty()) {
					columnLabels.push_back(comment);
				} else if (firstRow) {
					columnLabels.push_back("Column " + std::to_string(columnIndex + 1));
				}
				columnIndex++;
			}

			// Remove trailing empty cells
			while (!cells.empty() && cells.back().empty()) {
				cells.pop_back();
			}

			maxColumns = std::max(maxColumns, cells.size());
			rows.push_back(cells);
			firstRow = false;
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
		
		// Update column labels in case we found any
		module->updateLabels(columnLabels);
		
		// Trim trailing newline, for nicer copy & pasting, scrolling, and cursor handling.
		cleanedText = "" + cleanedText.erase(cleanedText.find_last_not_of("\n") + 1);
		return cleanedText;
	}
	
	void resizeText(float delta) {
		float target = lineHeight + delta;
		lineHeight = clamp(target, 4.f, 128.f);
		charWidth = lineHeight * 0.5;
		module->lineHeight = lineHeight;
		module->dirty = true;
	}
	
	void sizeText(float size) {
		lineHeight = clamp(size, 4.f, 128.f);
		charWidth = lineHeight * 0.5;
		module->lineHeight = lineHeight;
		module->dirty = true;
	}
	
	void clampCursor() {
		// Ensure cursor and selection is not out of bounds
		if (cursor > (int)text.length()) {
			cursor = (int)text.length();
		} else if (cursor < 0) {
			cursor = 0;
		}
		
		if (selection > (int)text.length()) {
			selection = (int)text.length();
		} else if (selection < 0) {
			selection = 0;
		}
	}
	
	void onSelectKey(const SelectKeyEvent& e) override {
		clampCursor();
		if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
			if (e.key == GLFW_KEY_ENTER) {
				std::string text = getText();
				std::string beforeCursor = text.substr(0, cursor);
				std::string afterCursor = text.substr(cursor);
				setText(beforeCursor + "\n" + afterCursor);
				// Trailing blank lines get trimmed away unless you add a 0 or a comma or something,
				// but not sure what an intuitive way to convey and/or avoid that would be
				
				cursor = beforeCursor.length() + 1;  // Set cursor right after the new line
				
				clampCursor();
				
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
			} else if (e.keyName == "]" && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				resizeText(1);
			} else if (e.keyName == "[" && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				resizeText(-1);
			}
		}
		clampCursor();
		LedDisplayTextField::onSelectKey(e);  // Delegate other keys to the base class
		clampCursor();
		scrollToCursor(); // Scroll to the cursor after any keypress
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
		
		// GRID_SNAP gives us a 10.16mm grid.
		// GRID_SNAP is derived from RACK_GRID_WIDTH, which is 1hp. One GRID_SNAP is 2hp in milimeters, because 2hp is just right for port spacing.
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
		
		
        // Main text field
        SpellbookTextField* textField = createWidget<SpellbookTextField>(mm2px(Vec(GRID_SNAP*3, GRID_SNAP*0.25)));
        textField->setSize(Vec(mm2px(GRID_SNAP*18), RACK_GRID_HEIGHT-mm2px(GRID_SNAP*0.5)));
        textField->module = module;
        addChild(textField);
		
        // Ensure text field is populated with current module text and lineHeight, and puts a pointer in the module for Undos
        if (module) {
            textField->setText(module->text);
			textField->sizeText(module->lineHeight);
			textField->cleanAndPublishText();
        }	
    }
	
/* 	void appendContextMenu(Menu* menu) override {
		Spellbook* module = dynamic_cast<Spellbook*>(this->module);

		menu->addChild(new MenuSeparator);

		// Controls int Module::mode
		menu->addChild(createIndexPtrSubmenuItem("Mode",
			{"Hi-fi", "Mid-fi", "Lo-fi"},
			&module->mode
		));
	} */
};

Model* modelSpellbook = createModel<Spellbook, SpellbookWidget>("Spellbook");