/*
T's Musical Tools (TMT) - A collection of esoteric modules for VCV Rack, focused on manipulating RNG and polyphonic signals.
Copyright (C) 2024  T

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "plugin.hpp"
#include "ports.hpp"
#include <deque>
#include <cmath>
#include <mutex>

#define SIGHT_BUFFER_SIZE 8192

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

struct Sight : Module {
	enum ParamId {
		TOGGLE_SWITCH,
		PARAMS_LEN
	};
	enum InputId {
		VOLTAGE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	std::deque<float> voltageBuffer;
    std::deque<float> voltageBufferCopy; // Copy of voltageBuffer for safe access
    const int bufferSize = SIGHT_BUFFER_SIZE;
    bool bufferNeedsUpdate = true; // Flag to indicate if the buffer needs update
	Timer timeSinceUpdate;

	Sight() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Alt Mode: Process at audio rate (CPU heavy)");
		configInput(VOLTAGE_INPUT, "Voltage");

        // Initialize both buffers with zeros
        voltageBuffer.resize(bufferSize, 0.f);
        voltageBufferCopy.resize(bufferSize, 0.f);
	}

    void advanceBuffer(float inputVoltage) {
        voltageBuffer.pop_back();
        voltageBuffer.push_front(inputVoltage);
        bufferNeedsUpdate = true; // Mark buffer as needing update
    }

	void process(const ProcessArgs& args) override {
		timeSinceUpdate.update(args.sampleTime); // Advance the timer

		if (!inputs[VOLTAGE_INPUT].isConnected()) {
			return; // Break early if there's no input
		}

		// Check the timer/toggle for processing rate
		if (!timeSinceUpdate.check(0.001f) && params[TOGGLE_SWITCH].getValue() < 0.5f) {
			return; // Break early if not enough time has passed
		}

		timeSinceUpdate.reset(); // Reset the timer, since we're about to process

		advanceBuffer(inputs[VOLTAGE_INPUT].getVoltage());

        // Copy the voltageBuffer for safe access by SightScope widget
        if (bufferNeedsUpdate) {
            std::lock_guard<std::mutex> lock(bufferMutex);
            voltageBufferCopy = voltageBuffer;
            bufferNeedsUpdate = false;
        }
	}

    // Mutex for thread safety
    std::mutex bufferMutex;
};

struct SightScope : LightWidget {
    Sight* module;
    int bufferSize = SIGHT_BUFFER_SIZE;
    std::deque<float> voltageBuffer;
    std::vector<float> scalingFactors; // Store precalculated scaling factors
	std::vector<float> precomputedPositions; // Store precalculated scaled positions
	bool dirty = true;

    SightScope(Sight* module) {
        this->module = module;
        voltageBuffer.resize(bufferSize, 0.f); // Match module
        bufferSize = voltageBuffer.size(); // Match module
		dirty = true;
    }

	void precomputePositions() {
		scalingFactors.resize(bufferSize);
		precomputedPositions.resize(bufferSize);
		for (int i = 0; i < bufferSize; ++i) {
			scalingFactors[i] = std::log2(i + 1) / std::log2(bufferSize);
			precomputedPositions[i] = (box.size.x - scalingFactors[i] * box.size.x) * 1.5f;
		}
		dirty = false;
	}

	void step() override {

	}


    void drawLight(const DrawArgs& args) override {
        if (!module) {
            return;
        }

		if (dirty) precomputePositions();

        // Safely access the voltageBufferCopy from Sight module
        {
            std::lock_guard<std::mutex> lock(module->bufferMutex);
            voltageBuffer = module->voltageBufferCopy;
        }

		nvgScissor(args.vg, args.clipBox.pos.x, args.clipBox.pos.y, args.clipBox.size.x, args.clipBox.size.y);



		// Draw the input voltages in the buffer
		for (int i = 0; i < bufferSize - 1; i++) {
			// Calculate the thickness for this segment and the next segment
			float thicknessStart = 3.f * (1.f - scalingFactors[i]);
			float thicknessEnd = 3.f * (1.f - scalingFactors[i + 1]);
			float position1 = precomputedPositions[i];
			float position2 = precomputedPositions[i + 1];

			// Begin a new path for each segment
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, position1, box.size.y - rescale(voltageBuffer[i], -10.f, 10.f, 0.f, box.size.y));
			nvgLineTo(args.vg, position2, box.size.y - rescale(voltageBuffer[i + 1], -10.f, 10.f, 0.f, box.size.y));

			// Set the line thickness for this segment
			nvgStrokeWidth(args.vg, thicknessStart);

			// Stroke the segment
			nvgStrokeColor(args.vg, nvgRGBA(254, 201, 1, 255));
			nvgStroke(args.vg);

			// Set the line thickness for the next segment
			nvgStrokeWidth(args.vg, thicknessEnd);

			// Draw a dot at the start of the current segment
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, position1, box.size.y - rescale(voltageBuffer[i], -10.f, 10.f, 0.f, box.size.y), thicknessStart / 2);
			nvgFillColor(args.vg, nvgRGBA(254, 201, 1, 255));
			nvgFill(args.vg);
		}

		float position3 = precomputedPositions[bufferSize - 1];

		// Draw the last segment (to ensure the thickness is applied)
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, position3, box.size.y - rescale(voltageBuffer[bufferSize - 1], -10.f, 10.f, 0.f, box.size.y));
		nvgLineTo(args.vg, position3, box.size.y - rescale(voltageBuffer[bufferSize - 1], -10.f, 10.f, 0.f, box.size.y));
		nvgStroke(args.vg);

		// Draw a dot at the last sample
		nvgBeginPath(args.vg);
		float thicknessLast = 4.f * (1.f - scalingFactors[bufferSize - 1]);
		nvgCircle(args.vg, position3, box.size.y - rescale(voltageBuffer[bufferSize - 1], -10.f, 10.f, 0.f, box.size.y), thicknessLast / 2);
		nvgFillColor(args.vg, nvgRGBA(254, 201, 1, 255));
		nvgFill(args.vg);

		nvgResetScissor(args.vg);
	}
};

struct SightWidget : ModuleWidget {
	SightWidget(Sight* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/sight.svg")));

		addParam(createParamCentered<BrassToggle>(Vec(box.size.x/2, mm2px(6)), module, Sight::TOGGLE_SWITCH));

		SightScope* diagram = new SightScope(module);
		diagram->box.pos = Vec(15, 30);
		diagram->box.size = Vec(240, 260);
		addChild(diagram);

		addInput(createInputCentered<BrassPort>(mm2px(Vec(45.72, 112.842)), module, Sight::VOLTAGE_INPUT));
	}
};

Model* modelSight = createModel<Sight, SightWidget>("Sight");
