#include "plugin.hpp"
#include "ports.hpp"
#include <sstream>
#include <vector>
#include <map>
#include <iomanip>
#include <regex>

#define GRID_SNAP 10.16 // 10.16mm grid for placing components
#define SPELLBOOK_DEFAULT_WIDTH 48
#define SPELLBOOK_MIN_WIDTH 18
#define SPELLBOOK_MAX_WIDTH 96
#define SPELLBOOK_MIN_LINEHEIGHT 4.0f
#define SPELLBOOK_MAX_LINEHEIGHT 128.0f

struct StepData {
    float voltage;
    char type;  // 'N' for normal, 'T' for trigger, 'R' for retrigger, 'G' for gate, 'E' for empty, 'U' for unused
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
        STEPFWD_INPUT,
        RESET_INPUT,
		INDEX_INPUT,
		STEPBAK_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        POLY_OUTPUT,
        OUT01_OUTPUT, OUT02_OUTPUT, OUT03_OUTPUT, OUT04_OUTPUT,
        OUT05_OUTPUT, OUT06_OUTPUT, OUT07_OUTPUT, OUT08_OUTPUT,
        OUT09_OUTPUT, OUT10_OUTPUT, OUT11_OUTPUT, OUT12_OUTPUT,
        OUT13_OUTPUT, OUT14_OUTPUT, OUT15_OUTPUT, OUT16_OUTPUT,
		RELATIVE_OUTPUT, ABSOLUTE_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        LIGHTS_LEN
    };

    dsp::SchmittTrigger stepForwardTrigger;
	dsp::SchmittTrigger stepBackTrigger;
	dsp::SchmittTrigger resetTrigger;
    std::vector<std::vector<StepData>> steps;
	std::vector<std::string> firstRowComments; // Fill in whenever we check row 1
	std::vector<std::string> currentStepComments; // Continually update as we go, but only if and when we encounter comments, so they're sticky
	Timer triggerTimer; // General purpose stopwatch, used by Triggers and Retriggers
	Timer resetIgnoreTimer; // Timer to ignore Clock input briefly after Reset triggers
    std::vector<StepData> lastValues;
    int currentStep = 0;
	int width = SPELLBOOK_DEFAULT_WIDTH; // Default width for the module is 48hp
	// Map of accidentals and their offsets

    //std::string text = "0 ?Column 1, 0 ?Column 2, 0 ?Column 3, 0 ?Column 4\n0, 0, 0, 0\n0, 0, 0, 0\n0, 0, 0, 0"; // A default sequence that outputs four labelled 0s for 4 steps
	
	// Default text is a little tutorial
	std::string text = R"~(0 ? Decimal                                         , T ? Trigger
1.0 ? text after ? is ignored (for comments)!       , X ? Gate with retrigger
-1 ? row 1 comments become output labels            , W ? Full width gate
1 ? (sorry no row 0 / header row... yet!)                  , | ? alternate full width gate
                                                    , |
? Empty cells don't change the output...            , ? ...except after gates/triggers
                                                    , 
C4 ? Also parses note names like `C4` to 1v/oct...  , X
C ? (octave 4 is the default if left out)           , X
m60 ? ...or MIDI note numbers like `m60`...         , X
s7 ? ...or semitones from C4 like `s7`.             , X
10% ? ...or percentages! (useful for velocity)      , X
? Important !!! This is not a Tracker !!!               , 
C4 ? Pitches do NOT automatically create triggers..., ? ...you need a trigger column
                                                    , X ? or triggers from somewhere else
? Or use columns for ANY CV                         , | ? Think modular!)~";

	std::string defaultText = text;

    bool dirty = false;
    bool fullyInitialized = false;
	float lineHeight = 12;
    
	Spellbook() : lastValues(16, {0.0f, 'N'}) {  // Some RhythML commands act differently based on the prior voltage of each channel, so assume all 0s for "before time began"
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configInput(STEPFWD_INPUT, "Step Forward");
		configInput(STEPBAK_INPUT, "Step Backward");
        configInput(RESET_INPUT, "Reset");
		configInput(INDEX_INPUT, "Index");
        configOutput(POLY_OUTPUT, "16 voltages from columns"); // This poly output will always be exactly 16 channels.
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Toggle relative / absolute indexing");
		configOutput(RELATIVE_OUTPUT, "Relative Index");
		configOutput(ABSOLUTE_OUTPUT, "Absolute Index");

        for (int i = 0; i < 16; ++i) { 
            configOutput(OUT01_OUTPUT + i, "Column " + std::to_string(i + 1));
            outputs[OUT01_OUTPUT + i].setVoltage(0.0f);
        }
        outputs[POLY_OUTPUT].setChannels(16);
			// TODO: The compiler keeps complaining about array bounds, because we're basically just pinky-promising ourselves to never change the number of channels and a lot of loops just ASSUME 16 channels. Need to change something about how we distribute all the right values to all the right channels to avoid that awkwardness.
        width = SPELLBOOK_DEFAULT_WIDTH; // Not sure this is needed, I just feel safer with it here.
		fullyInitialized = true;
    }
	
	
	void updateLabels(std::vector<std::string> labels) {
		// Frist default all the labels
        for (int i = 0; i < 16; ++i) { 
            configOutput(OUT01_OUTPUT + i, "Column " + std::to_string(i + 1));
            outputs[OUT01_OUTPUT + i].setVoltage(0.0f);
        }
		// Config the output using comments from Row 1 as labels:
		for (size_t i = 0; i < labels.size(); ++i) {
			configOutput(OUT01_OUTPUT + i, labels[i]);
		}
	}

    void onReset() override {
		resetIgnoreTimer.set(0.01); // Set the timer to ignore clock inputs for 10ms after reset
		text = defaultText;
        dirty = true;
    }

	void fromJson(json_t* rootJ) override {
		Module::fromJson(rootJ);
		// In <1.0, module used "text" property at root level.
		json_t* textJ = json_object_get(rootJ, "text");
		if (textJ)
			text = json_string_value(textJ);
		
		json_t* lineHeightJ = json_object_get(rootJ, "lineHeight");
		if (lineHeightJ) {
			lineHeight = clamp(json_number_value(lineHeightJ),SPELLBOOK_MIN_LINEHEIGHT,SPELLBOOK_MAX_LINEHEIGHT);
		}
		
		json_t* widthJ = json_object_get(rootJ, "width");
		if (widthJ) {
			width = clamp(json_number_value(widthJ),SPELLBOOK_MIN_WIDTH,SPELLBOOK_MAX_WIDTH); 
		}
		
		dirty = true;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "text", json_stringn(text.c_str(), text.size()));
		json_object_set_new(rootJ, "lineHeight", json_real(lineHeight));
		json_object_set_new(rootJ, "width", json_real(width));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		// Get text buffer
		json_t* textJ = json_object_get(rootJ, "text");
		if (textJ)
			text = json_string_value(textJ);
		
		// Get lineHeight (effectively text size / zoom level)
		json_t* lineHeightJ = json_object_get(rootJ, "lineHeight");
		if (lineHeightJ) {
			lineHeight = clamp(json_number_value(lineHeightJ),SPELLBOOK_MIN_LINEHEIGHT,SPELLBOOK_MAX_LINEHEIGHT);
		}

		json_t* widthJ = json_object_get(rootJ, "width");
		if (widthJ) {
			width = clamp(json_number_value(widthJ),SPELLBOOK_MIN_WIDTH,SPELLBOOK_MAX_WIDTH); 
		}

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
	
	// Map of accidental symbols to semitone shifts
	std::map<std::string, float> accidentalToShift = {
		{"#", 1.f}, {"B", -1.f}, {"D",-0.5f}, {"$", 0.5f}, {"~",-0.25f}, {"`",0.25f}
	};
	
	// Computes semitone offset from C for a given note letter and accidentals
	float letterAccidentalsToSemitone(char letter, const std::string& accidentals) {
		float baseSemitone = 0.f;

		switch (letter) {
			case 'C': baseSemitone = 0.f; break;
			case 'D': baseSemitone = 2.f; break;
			case 'E': baseSemitone = 4.f; break;
			case 'F': baseSemitone = 5.f; break;
			case 'G': baseSemitone = 7.f; break;
			case 'A': baseSemitone = 9.f; break;
			case 'B': baseSemitone = 11.f; break;
			default: return 0.f;  // Error case
		}

		float accidentalShift = 0.f;
		for (const char& acc : accidentals) {
			std::string accStr(1, acc);
			if (accidentalToShift.count(accStr)) {
				accidentalShift += accidentalToShift[accStr];
			}
		}

		return baseSemitone + accidentalShift;
	}

	// Converts a note name and octave to a voltage
	float noteNameToVoltage(const std::string& noteName, int octave) {
		if (noteName.empty()) return 0.0f;
		
		char noteLetter = noteName[0];
		std::string accidentals = noteName.substr(1);

		float semitoneOffsetFromC4 = letterAccidentalsToSemitone(noteLetter, accidentals) + (octave - 4) * 12;

		return static_cast<float>(semitoneOffsetFromC4) / 12.0f;
	}
	
	// Scale Hertz to 1v/Octave
	float frequencyToVoltage(float frequency) {
		return std::log2(frequency / 261.63f);  // Converts Hz to 1V/oct standard with C4 = 261.63 Hz
	}

	// Scale Cents to 1v/Octave
	float parseCents(const std::string& centsPart) {
		try {
			float cents = std::stof(centsPart);
			return cents / 1200.0f;  // Convert cents to 1V/oct relative to C4
		} catch (...) {
			return 0.0f;  // Return default voltage if parsing fails
		}
	}
	
	// Parses pitch from a cell with various formats
	float parsePitch(const std::string& cell) {
		if (cell.empty()) {
			return 0.0f;  // Return default voltage for empty cells
		}
		
		// Decimal values
		if (isDecimal(cell)) {
			return std::stof(cell);
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
		
		// Hz format parsing
		if (cell.find("HZ") != std::string::npos) {
			try {
				float frequency = std::stof(cell.substr(0, cell.find("HZ")));
				return frequencyToVoltage(frequency);  // Convert frequency directly to voltage
			} catch (...) {
				return 0.0f;  // Return default voltage if parsing fails
			}
		}

		// Cents notation parsing
		if (cell.find("CT") != std::string::npos) {
			try {
				return parseCents(cell.substr(0, cell.find("CT")));
			} catch (...) {
				return 0.0f;  // Return default voltage if parsing fails
			}
		}
		
		for (size_t i = 0; i < cell.size(); ++i) {
			if (isdigit(cell[i]) || cell[i] == '-' || cell[i] == '+') {
				std::string notePart = cell.substr(0, i);
				std::string octavePart = cell.substr(i);

				int octave = 4;
				if (tryParseOctave(octavePart, octave)) {
					return noteNameToVoltage(notePart, octave);
				} else {
					return noteNameToVoltage(notePart, 4);
				}
			}
		}
		
		try {
			std::string notePart = cell;
			return noteNameToVoltage(notePart, 4);
		} catch (...) {
			// Do nothing
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
			std::vector<StepData> stepData(16, StepData{0.0f, 'U'});  // Default all steps to 0.0 volts, "Unused" type
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
					if (cell == "W" || cell == "|") {
						stepData[index].voltage = 10.0f;
						stepData[index].type = 'G';  // Full Width Gate (stay 10v the entire step)
					} else if (cell == "T" || cell == "^") {
						stepData[index].voltage = 10.0f;
						stepData[index].type = 'T';  // Trigger (1ms pulse)
					} else if (cell == "X" || cell == "R" || cell == "_") {
						stepData[index].voltage = 10.0f;
						stepData[index].type = 'R';  // Gate with Retrigger (0v for 1ms at start of step, then 10v after)
					} else {
						stepData[index].voltage = parsePitch(cell);
						stepData[index].type = 'N'; // Normal, anything that translates to a simple voltage/pitch
					}
				} else {
						stepData[index].voltage = 0.0f;
						stepData[index].type = 'E';  // Empty (but "active")
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
		
		int stepCount = steps.size();
		int lastStep = currentStep;
		
		if (!inputs[INDEX_INPUT].isConnected() && !resetHigh && !ignoreClock && !steps.empty()) {
			// Forward step
			if (stepForwardTrigger.process(inputs[STEPFWD_INPUT].getVoltage())) {
				currentStep = (currentStep + 1) % stepCount;
				triggerTimer.reset();
			}
			
			// Backward step
			if (stepBackTrigger.process(inputs[STEPBAK_INPUT].getVoltage())) {
				currentStep = (currentStep - 1 + stepCount) % stepCount;
				triggerTimer.reset();
			}
			
		} else if (inputs[INDEX_INPUT].isConnected()) {
			float indexVoltage = inputs[INDEX_INPUT].getVoltage();
			//int numSteps = steps.size();
			if (params[TOGGLE_SWITCH].getValue() > 0) { 
				currentStep = clamp((int)indexVoltage % stepCount,0,stepCount-1); // Absolute mode (alt)
				//configInput(INDEX_INPUT, "Index (Absolute address, 1v/step)");
			} else {
				float percentage = indexVoltage/10.f; // Treat 10.v as "1.0" for "100%"
				
				float unboundedIndex = percentage * stepCount; // Get the index that is <percentage> through the array
				
				//unboundedIndex -= 0.0001f;
				
				int targetIndex = (int)unboundedIndex % stepCount;
				
				if (targetIndex==0 && std::fabs(unboundedIndex)>1) targetIndex=stepCount;
				
				if (targetIndex < 0) targetIndex+=stepCount;

				currentStep = clamp(targetIndex, 0, stepCount-1); // Relative mode (default)
				//configInput(INDEX_INPUT, "Index (Relative / Phasor-like)");
			}
			if (currentStep != lastStep) {
				triggerTimer.reset();
			}
		}
		
		float rowCount = (float)stepCount;
		float relativeIndex = currentStep / (rowCount-1) * 10.f;
		float absoluteIndex = (float)currentStep + 1.f;
		outputs[RELATIVE_OUTPUT].setVoltage( relativeIndex );
		outputs[ABSOLUTE_OUTPUT].setVoltage( absoluteIndex );

		outputs[POLY_OUTPUT].setChannels(16);
		std::vector<StepData>& currentValues = steps[currentStep];

		for (int i = 0; i < 16; i++) { // Use PORT_MAX_CHANNELS instead of 16?
			StepData& step = currentValues[i];
			float outputValue = 0.f;  // Default 0v

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
				case 'U': // Unused cells
					outputValue = 0.f;
					break;
				default:
					outputValue = 0.f;
					break;
			}
			outputs[OUT01_OUTPUT + i].setVoltage(outputValue);
			outputs[POLY_OUTPUT].setVoltage(outputValue, i);
			// To-do: Maybe setChannels() to removed unused columns?
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
	std::string old_text, new_text;
	int old_width, new_width;

	SpellbookUndoRedoAction(int64_t id, std::string oldText, std::string newText) : old_text{oldText}, new_text{newText} {
		moduleId = id;
		name = "Spellbook text edit";
		old_width = new_width = -1; // flag as "not a resize"
	}
	
	SpellbookUndoRedoAction(int64_t id, int oldWidth, int newWidth) : old_width{oldWidth}, new_width{newWidth} {
		moduleId = id;
		name = "Spellbook panel resize";
	}

	void undo() override {
		Spellbook *module = dynamic_cast<Spellbook*>(APP->engine->getModule(moduleId));
		if (module) {
			if (old_width < 0) {// This must have been a text edit
				module->overrideText(this->old_text);
			} else {
				module->width = old_width;
			}
		}
	}

	void redo() override {
	Spellbook *module = dynamic_cast<Spellbook*>(APP->engine->getModule(moduleId));
		if (module) {
			if (new_width < 0) {// This must have been a text edit
				module->overrideText(this->new_text);
			} else {
				module->width = new_width;
			}
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
	float charWidth = lineHeight*0.5; // Text is almost always drawn character by character, stepping by this amount, instead of 
	
	NVGcolor textColor = nvgRGB(255, 215, 0); // Bright gold text
	NVGcolor commaColor = nvgRGB(155, 131, 0); // Dark gold commas
	NVGcolor commentColor = nvgRGB(158, 80, 191); // Purple comments
	NVGcolor commentCharColor = nvgRGB(121, 8, 170); // Dark purple for `?`
	NVGcolor selectionColor = nvgRGB(39, 1, 52); // Darkest purple for selection highlight
	NVGcolor currentStepColor = nvgRGB(255, 255, 255); // White current step when autoscrolling
	NVGcolor cursorColor = nvgRGBA(158, 80, 191,192); // Light translucent purple for cursor
	NVGcolor lineColor = textColor;
	NVGcolor activeColor = textColor;
	
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
	
	void cursorToPrevCell() {
		size_t pos = text.rfind(',', std::max(cursor - 2, 0));
		if (pos == std::string::npos)
			cursor = 0;
		else
			cursor = std::min((int) pos + 1, (int) text.size());
		
		if (text[cursor-1] == ',') cursor--;
	}

	void cursorToNextCell() {
		size_t pos = text.find(',', std::min(cursor + 1, (int) text.size()));
		if (pos == std::string::npos)
			pos = text.size();
		cursor = pos;
	}
	
	Vec getCursorPosition(int cursor) {
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
		float cursorY = cursorLine * lineHeight + 0.5f;
		float cursorX = cursorPos * charWidth + 0.5f;
		
		return Vec(cursorX,cursorY);
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
	
	void startParse() {
		if (module) {
			std::string priorText = module->text;
			cleanAndPublishText();
			if (text != priorText) {
				// Push an undo action if we made a real change (post cleaning)
				APP->history->push( new SpellbookUndoRedoAction(module->id, priorText, text) );
			}
		}
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
	
	void resizeText(float delta) { // Resize relative to current size
		float target = lineHeight + delta;
		lineHeight = clamp(target, SPELLBOOK_MIN_LINEHEIGHT, SPELLBOOK_MAX_LINEHEIGHT);
		charWidth = lineHeight * 0.5;
		module->lineHeight = lineHeight;
		module->dirty = true;
	}
	
	void sizeText(float size) { // Set an absolute size
		lineHeight = clamp(size, SPELLBOOK_MIN_LINEHEIGHT, SPELLBOOK_MAX_LINEHEIGHT);
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
		clampCursor(); // Safety rail, probably not needed
		if (e.action == GLFW_PRESS || e.action == GLFW_REPEAT) {
			// Jump Left
			if (e.key == GLFW_KEY_LEFT && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				cursorToPrevCell();
				if (!(e.mods & GLFW_MOD_SHIFT)) {
					selection = cursor;
				}
				e.consume(this);
			} else if (e.key == GLFW_KEY_RIGHT && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				cursorToNextCell();
				if (!(e.mods & GLFW_MOD_SHIFT)) {
					selection = cursor;
				}
				e.consume(this);
			} else if (e.key == GLFW_KEY_ENTER) {
				if ((e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
					Vec cursorPos = getCursorPosition(std::min(cursor,selection));
					startParse();
					cursor = getTextPosition(Vec(cursorPos.x,cursorPos.y));
					selection = cursor;
					clampCursor();
					e.consume(this);
					return;
				} else {
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
					//updateSizeAndOffset();
					
					e.consume(this);
					return;
				}
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
				e.consume(this);
				return;
			} else if (e.keyName == "]" && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				resizeText(1);
			} else if (e.keyName == "[" && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				resizeText(-1);
			} else {
				LedDisplayTextField::onSelectKey(e);  // Delegate other keys to the base class
			}
		}
		clampCursor(); // Safety rail, probably not needed
		updateSizeAndOffset();  // Validate size and offset after any keypress
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
	
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;  // Only draw on the text layer
		
		// Textfield backdrop
		nvgBeginPath(args.vg);
		nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 200));
		nvgRect(args.vg, -2, -2, box.size.x+4, box.size.y+4);
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, textColor);  // White color with full opacity
		nvgStrokeWidth(args.vg, 1.0);  // Set the width of the stroke
		nvgStroke(args.vg);  // Apply the stroke to the path
		
		if (!module) return;  // Only proceed if module is active
				
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

		// Slightly oversized box so we can draw a bordered backdrop for the textfield
		nvgScissor(args.vg, args.clipBox.pos.x-2, args.clipBox.pos.y-2, args.clipBox.size.x+4, args.clipBox.size.y+4);
		


		// Make sure the scissor matches our box... for now.
		nvgScissor(args.vg, args.clipBox.pos.x, args.clipBox.pos.y, args.clipBox.size.x, args.clipBox.size.y);

		// Configure font
		std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/Hack-Regular.ttf"));
		//std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, "res/BravuraText.otf"));
		if (!font) { // Use app font as a backup
			font = APP->window->loadFont(fontPath);
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
		while (std::getline(lines, line)) {
			nvgFontSize(args.vg, lineHeight);  // Brute force match lineHeight
			
			if (y + lineHeight < 0) { // +lineHeight lets it draw one extra line, for bleed
				y += lineHeight;
				currentPos += line.size() + 1;
				lineIndex++; // Only increment if not a `??` comment row? Would that work? This seems to only be used to determine what Step we're on, not wheere we're drawing
				continue;
			}
			
			if (y > box.size.y+lineHeight*2) {
				break; // Stop once we've drawn two extra lines, for bleed
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
				stepNumber = ""+ stepNumber;
			}
			//float stepSize = std::min(lineHeight,14.f);
			//float centerOffset = stepSize / lineHeight;
			float stepSize = std::min(lineHeight,14.f); // step numbers max out at a smaller size or it looks bad
			float stepY = 0 + (lineHeight - stepSize)*0.5; // center them to their row, looks better when they're smaller
			nvgFontSize(args.vg, stepSize); 
			float stepTextWidth = nvgTextBounds(args.vg, 0, 0, stepNumber.c_str(), NULL, NULL); // So we can move it left by one text-length
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
};

struct SpellbookResizeHandle : OpaqueWidget {
	Vec dragPos;
	Rect originalBox;
	Spellbook* module;
	bool right = false;  // True for one on the right side.

	SpellbookResizeHandle() {
	// One hole wide and full length tall.
		box.size = Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
	}

	void onDragStart(const DragStartEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;

		dragPos = APP->scene->rack->getMousePos();
		ModuleWidget* mw = getAncestorOfType<ModuleWidget>();
		assert(mw);
		originalBox = mw->box;
	}

	void onDragMove(const DragMoveEvent& e) override {
		ModuleWidget* mw = getAncestorOfType<ModuleWidget>();
		assert(mw);
		int original_width = module->width;

		Vec newDragPos = APP->scene->rack->getMousePos();
		float deltaX = newDragPos.x - dragPos.x;

		Rect newBox = originalBox;
		Rect oldBox = mw->box;
		// Minimum and maximum number of holes we allow the module to be.
		const float minWidth = SPELLBOOK_MIN_WIDTH * RACK_GRID_WIDTH;
		const float maxWidth = SPELLBOOK_MAX_WIDTH * RACK_GRID_WIDTH;
		if (right) {
			newBox.size.x += deltaX;
			newBox.size.x = std::fmax(newBox.size.x, minWidth);
			newBox.size.x = std::fmin(newBox.size.x, maxWidth);
			newBox.size.x = std::round(newBox.size.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH;
		} else {
			newBox.size.x -= deltaX;
			newBox.size.x = std::fmax(newBox.size.x, minWidth);
			newBox.size.x = std::fmin(newBox.size.x, maxWidth);
			newBox.size.x = std::round(newBox.size.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH;
			newBox.pos.x = originalBox.pos.x + originalBox.size.x - newBox.size.x;
		}
		// Set box and test whether it's valid.
		mw->box = newBox;
		if (!APP->scene->rack->requestModulePos(mw, newBox.pos)) {
			mw->box = oldBox;
		}
		module->width = std::round(mw->box.size.x / RACK_GRID_WIDTH); // Storing it here lets all the widgets see it
		
		if (original_width != module->width) { // Move to onDragEnd()?
			// Make resizing an undo/redo action. If I don't do this, undoing a
			// different module's move will cause them to overlap (aka, a
			// transporter malfunction).
			APP->history->push(new SpellbookUndoRedoAction(module->id, original_width, module->width));
		}
	}
};

struct SpellbookWidget : ModuleWidget {
	SpellbookResizeHandle* rightHandle;
	SvgWidget* rightBrass;
	BrassPortOut* polyOutput;
    BrassPortOut* out01Output;
    BrassPortOut* out09Output;
    BrassPortOut* out02Output;
    BrassPortOut* out10Output;
    BrassPortOut* out03Output;
    BrassPortOut* out11Output;
    BrassPortOut* out04Output;
    BrassPortOut* out12Output;
    BrassPortOut* out05Output;
    BrassPortOut* out13Output;
    BrassPortOut* out06Output;
    BrassPortOut* out14Output;
    BrassPortOut* out07Output;
    BrassPortOut* out15Output;
    BrassPortOut* out08Output;
    BrassPortOut* out16Output;
    BrassPortOut* relativeOutput;
    BrassPortOut* absoluteOutput;
	SpellbookTextField* textField;
	
	int width = SPELLBOOK_DEFAULT_WIDTH; // Default width of Spellbook
	
	SpellbookWidget(Spellbook* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/spellbook.svg")));
		
		// We have to manually resize immediately because the SVG must be the width of the max size panel, ready to be cropped
		if (module) {
			int oldWidth = module->width;
			int newWidth = oldWidth;
			box.size.x = module->width * RACK_GRID_WIDTH; // Match width from module
			
			while (newWidth >= SPELLBOOK_MIN_WIDTH && !APP->scene->rack->requestModulePos(this, box.pos)) {
				newWidth--; // Shrink until we either hit a valid box position, or min size
				box.size.x = newWidth * RACK_GRID_WIDTH;
			}
			
			if (newWidth != oldWidth) {
				module->width = newWidth; // Push that back to the module
			}
		} else {
			box.size.x = SPELLBOOK_DEFAULT_WIDTH * RACK_GRID_WIDTH; // Default width (i.e. for browser
		}
		
		addParam(createParamCentered<BrassToggle>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*6)), module, Spellbook::TOGGLE_SWITCH));
		
		// GRID_SNAP gives us a 10.16mm grid.
		// GRID_SNAP is derived from RACK_GRID_WIDTH, which is 1hp. One GRID_SNAP is 2hp in milimeters, because 2hp is just right for port spacing.
		// Module is 48hp wide, with 4hp of space on the left side and and right sides for ports
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1.5, GRID_SNAP*1.5)), module, Spellbook::STEPFWD_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*0.75, GRID_SNAP*1.5)), module, Spellbook::STEPBAK_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*3.0)), module, Spellbook::RESET_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(GRID_SNAP*1, GRID_SNAP*4.5)), module, Spellbook::INDEX_INPUT));
		

		
        // Main text field
        textField = createWidget<SpellbookTextField>(mm2px(Vec(GRID_SNAP*3, GRID_SNAP*0.25)));
        textField->setSize(Vec(mm2px(GRID_SNAP*18), RACK_GRID_HEIGHT-mm2px(GRID_SNAP*0.5)));
        textField->module = module;
        addChild(textField);
		
        // Ensure text field is populated with current module text and lineHeight, and puts a pointer in the module for Undos
        if (module) {
            textField->setText(module->text);
			textField->sizeText(module->lineHeight);
			textField->cleanAndPublishText();
        }
		
		// Resize bar on right.
		//SpellbookResizeHandle* rightHandle = createWidget<SpellbookResizeHandle>(Vec(box.size.x - RACK_GRID_WIDTH, 0));
		rightHandle = new SpellbookResizeHandle;
		rightHandle->box.pos.x = box.size.x - RACK_GRID_WIDTH; // Scoot to the right edge minus 1hp;
		rightHandle->right = true;
		rightHandle->module = module;
		// Make sure the handle is correctly placed if drawing for the module
		// browser.
		
		addChild(rightHandle);
		
        // Load and position right brass element
        rightBrass = new SvgWidget();
        rightBrass->setSvg(Svg::load(asset::plugin(pluginInstance, "res/brass_right_spellbook.svg")));
        rightBrass->box.pos = Vec(box.size.x - rightBrass->box.size.x, 0); // Initially position; adjust y as needed
        addChild(rightBrass);
		
		// Right hand output ports
		// store this port as a class member so we can move it around later, then add it as an output like normal;
        // Create output ports
        polyOutput = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 22.5, GRID_SNAP * 1)), module, Spellbook::POLY_OUTPUT);
        out01Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 22, GRID_SNAP * 2)), module, Spellbook::OUT01_OUTPUT);
        out09Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 23, GRID_SNAP * 2)), module, Spellbook::OUT09_OUTPUT);
        out02Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 22, GRID_SNAP * 3)), module, Spellbook::OUT02_OUTPUT);
        out10Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 23, GRID_SNAP * 3)), module, Spellbook::OUT10_OUTPUT);
        out03Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 22, GRID_SNAP * 4)), module, Spellbook::OUT03_OUTPUT);
        out11Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 23, GRID_SNAP * 4)), module, Spellbook::OUT11_OUTPUT);
        out04Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 22, GRID_SNAP * 5)), module, Spellbook::OUT04_OUTPUT);
        out12Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 23, GRID_SNAP * 5)), module, Spellbook::OUT12_OUTPUT);
        out05Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 22, GRID_SNAP * 6)), module, Spellbook::OUT05_OUTPUT);
        out13Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 23, GRID_SNAP * 6)), module, Spellbook::OUT13_OUTPUT);
        out06Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 22, GRID_SNAP * 7)), module, Spellbook::OUT06_OUTPUT);
        out14Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 23, GRID_SNAP * 7)), module, Spellbook::OUT14_OUTPUT);
        out07Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 22, GRID_SNAP * 8)), module, Spellbook::OUT07_OUTPUT);
        out15Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 23, GRID_SNAP * 8)), module, Spellbook::OUT15_OUTPUT);
        out08Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 22, GRID_SNAP * 9)), module, Spellbook::OUT08_OUTPUT);
        out16Output = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 23, GRID_SNAP * 9)), module, Spellbook::OUT16_OUTPUT);
        relativeOutput = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 22, GRID_SNAP * 10.5)), module, Spellbook::RELATIVE_OUTPUT);
        absoluteOutput = createOutputCentered<BrassPortOut>(mm2px(Vec(GRID_SNAP * 23, GRID_SNAP * 10.5)), module, Spellbook::ABSOLUTE_OUTPUT);

        // Add outputs to module
        addOutput(polyOutput);
        addOutput(out01Output);
        addOutput(out09Output);
        addOutput(out02Output);
        addOutput(out10Output);
        addOutput(out03Output);
        addOutput(out11Output);
        addOutput(out04Output);
        addOutput(out12Output);
        addOutput(out05Output);
        addOutput(out13Output);
        addOutput(out06Output);
        addOutput(out14Output);
        addOutput(out07Output);
        addOutput(out15Output);
        addOutput(out08Output);
        addOutput(out16Output);
        addOutput(relativeOutput);
        addOutput(absoluteOutput);
    }
	
	void step() override {
		// This resize system is borrowed with love from Fermata by mahlenmorris
		
		Spellbook* module = dynamic_cast<Spellbook*>(this->module);
		// While this is really only useful to call when the width changes,
		// I don't think it's currently worth the effort to ONLY call it then.
		// And maybe the *first* time step() is called.
		
		// This whole section is exactly what the main widget also does when the module is created
		if (module) { // If the module is loaded
			int oldWidth = module->width;
			int newWidth = oldWidth;
			box.size.x = module->width * RACK_GRID_WIDTH; // Match width from module
			
			while (newWidth >= SPELLBOOK_MIN_WIDTH && !APP->scene->rack->requestModulePos(this, box.pos)) {
				newWidth--; // Shrink until we either hit a valid box position, or min size
				box.size.x = newWidth * RACK_GRID_WIDTH;
			}
			
			if (newWidth != oldWidth) {
				module->width = newWidth; // Push that back to the module
			}
		} else { // module is not loaded, like when showing the module in the module browser
			box.size.x = SPELLBOOK_DEFAULT_WIDTH * RACK_GRID_WIDTH; // default
		}
		
		
		// This continually moves the handle and ports relative to the right edge of the panel
		if (rightHandle && module && textField) {
			float rightEdge = box.size.x;
			
			rightHandle->box.pos.x = rightEdge - rightHandle->box.size.x;
			
			// Resize the text field
			textField->box.size.x = rightEdge - mm2px(GRID_SNAP*3) - textField->box.pos.x;
			
			// Also move the ports (scary! never let them go out of bounds, probably?)
			float portOffset = polyOutput->box.size.x / 2;
			float leftColumn = mm2px(GRID_SNAP*3);
			float rightColumn = mm2px(GRID_SNAP*2);
			float portMin = mm2px(RACK_GRID_WIDTH);
			float portMax = box.size.x - mm2px(RACK_GRID_WIDTH);
			
			// Centered poly port
			polyOutput->box.pos.x = clamp(box.size.x - mm2px(GRID_SNAP*2.5) + portOffset,portMin,portMax);
			
			// Left colummn
			out01Output->box.pos.x = clamp(rightEdge - leftColumn + portOffset,portMin,portMax);
			out02Output->box.pos.x = clamp(rightEdge - leftColumn + portOffset,portMin,portMax);
			out03Output->box.pos.x = clamp(rightEdge - leftColumn + portOffset,portMin,portMax);
			out04Output->box.pos.x = clamp(rightEdge - leftColumn + portOffset,portMin,portMax);
			out05Output->box.pos.x = clamp(rightEdge - leftColumn + portOffset,portMin,portMax);
			out06Output->box.pos.x = clamp(rightEdge - leftColumn + portOffset,portMin,portMax);
			out07Output->box.pos.x = clamp(rightEdge - leftColumn + portOffset,portMin,portMax);
			out08Output->box.pos.x = clamp(rightEdge - leftColumn + portOffset,portMin,portMax);
			
			relativeOutput->box.pos.x = clamp(rightEdge - leftColumn + portOffset,portMin,portMax);
			
			// Right column
			out09Output->box.pos.x = clamp(rightEdge - rightColumn + portOffset,portMin,portMax);
			out10Output->box.pos.x = clamp(rightEdge - rightColumn + portOffset,portMin,portMax);
			out11Output->box.pos.x = clamp(rightEdge - rightColumn + portOffset,portMin,portMax);
			out12Output->box.pos.x = clamp(rightEdge - rightColumn + portOffset,portMin,portMax);
			out13Output->box.pos.x = clamp(rightEdge - rightColumn + portOffset,portMin,portMax);
			out14Output->box.pos.x = clamp(rightEdge - rightColumn + portOffset,portMin,portMax);
			out15Output->box.pos.x = clamp(rightEdge - rightColumn + portOffset,portMin,portMax);
			out16Output->box.pos.x = clamp(rightEdge - rightColumn + portOffset,portMin,portMax);
			
			absoluteOutput->box.pos.x = clamp(rightEdge - rightColumn + portOffset,portMin,portMax);
		}
		
		if (rightBrass && module) {
			rightBrass->box.pos.x = box.size.x - rightBrass->box.size.x;
		}
		
		ModuleWidget::step();
	}
};

Model* modelSpellbook = createModel<Spellbook, SpellbookWidget>("Spellbook");