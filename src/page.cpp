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
#include "spellbook_expander.hpp"

struct Page : Module {
    enum ParamId {
        PARAMS_LEN
    };
    enum InputId {
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

    int position = 0;
    int64_t baseID = -1;
    int lastConfiguredPosition = -1;  // Track when we last updated output labels

    // Expander message buffers (static allocation to avoid DLL issues)
    // Allocate BOTH sides: Page receives from Spellbook (left) and sends to next Page (right)
    SpellbookExpanderMessage leftMessages[2];   // To RECEIVE from Spellbook
    SpellbookExpanderMessage rightMessages[2];  // To SEND to next Page

    Page() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configOutput(POLY_OUTPUT, "Polyphonic voltages from columns");

        for (int i = 0; i < 16; ++i) {
            configOutput(OUT01_OUTPUT + i, "Column " + std::to_string(i + 1));
            outputs[OUT01_OUTPUT + i].setVoltage(0.0f);
        }
        outputs[POLY_OUTPUT].setChannels(16);

        // Set up BOTH expander message buffers
        // VCV Rack Engine will connect adjacent modules' buffers
        leftExpander.producerMessage = &leftMessages[0];
        leftExpander.consumerMessage = &leftMessages[1];
        rightExpander.producerMessage = &rightMessages[0];
        rightExpander.consumerMessage = &rightMessages[1];

        // Initialize message data to defaults
        for (int i = 0; i < 2; i++) {
            leftMessages[i].baseID = -1;
            leftMessages[i].position = 0;
            leftMessages[i].currentStep = 0;
            leftMessages[i].totalSteps = 0;
            leftMessages[i].totalColumns = 0;
            for (int j = 0; j < MAX_EXPANDER_COLUMNS; j++) {
                leftMessages[i].outputVoltages[j] = 0.0f;
            }
        }
        for (int i = 0; i < 2; i++) {
            rightMessages[i].baseID = -1;
            rightMessages[i].position = 0;
            rightMessages[i].currentStep = 0;
            rightMessages[i].totalSteps = 0;
            rightMessages[i].totalColumns = 0;
            for (int j = 0; j < MAX_EXPANDER_COLUMNS; j++) {
                rightMessages[i].outputVoltages[j] = 0.0f;
            }
        }
    }

    void process(const ProcessArgs& args) override {
        // Read message from left module (either Spellbook or another Page)
        if (leftExpander.module && leftExpander.consumerMessage) {
            // Verify it's a Spellbook or Page module by checking the message
            SpellbookExpanderMessage* message = (SpellbookExpanderMessage*)leftExpander.consumerMessage;

            // Update our position and base ID
            baseID = message->baseID;
            position = message->position;

            if (message->totalColumns > 0) {
                // Calculate which columns this expander handles
                // Position 1 = columns 17-32 (indices 16-31)
                // Position 2 = columns 33-48 (indices 32-47)
                // etc.
                int startColumn = SPELLBOOK_BASE_COLUMNS + (position - 1) * 16;

                // Only update output labels when position changes (not every process call!)
                if (position != lastConfiguredPosition) {
                    std::string positionLabel = " (Page " + std::to_string(position) + ")";
                    for (int i = 0; i < 16; ++i) {
                        int columnIndex = startColumn + i;
                        configOutput(OUT01_OUTPUT + i, "Column " + std::to_string(columnIndex + 1) + positionLabel);
                    }
                    lastConfiguredPosition = position;
                }

                int activeChannels = 0;

                // Output the pre-calculated voltages for this expander's 16 columns
                for (int i = 0; i < 16; i++) {
                    int columnIndex = startColumn + i;
                    float outputValue = 0.0f;

                    // Only process if this column exists
                    if (columnIndex < message->totalColumns && columnIndex < MAX_EXPANDER_COLUMNS) {
                        // Simply read the pre-calculated voltage from Spellbook
                        outputValue = message->outputVoltages[columnIndex];
                        activeChannels = i + 1;
                    }

                    outputs[OUT01_OUTPUT + i].setVoltage(outputValue);
                    outputs[POLY_OUTPUT].setVoltage(outputValue, i);
                }

                outputs[POLY_OUTPUT].setChannels(activeChannels);

                // Forward message to right expander (if any)
                if (rightExpander.module && rightExpander.module->leftExpander.consumerMessage) {
                    SpellbookExpanderMessage* rightMessage = (SpellbookExpanderMessage*)rightExpander.module->leftExpander.consumerMessage;
                    rightMessage->baseID = baseID;
                    rightMessage->position = position + 1;  // Increment position
                    rightMessage->currentStep = message->currentStep;
                    rightMessage->totalSteps = message->totalSteps;
                    rightMessage->totalColumns = message->totalColumns;

                    // Copy the voltage array
                    for (int i = 0; i < MAX_EXPANDER_COLUMNS; i++) {
                        rightMessage->outputVoltages[i] = message->outputVoltages[i];
                    }

                    rightExpander.module->leftExpander.messageFlipRequested = true;
                }
            } else {
                // No data from left module - output zeros
                for (int i = 0; i < 16; i++) {
                    outputs[OUT01_OUTPUT + i].setVoltage(0.0f);
                    outputs[POLY_OUTPUT].setVoltage(0.0f, i);
                }
            }
        } else {
            // Not connected to anything - output zeros
            position = 0;
            baseID = -1;
            for (int i = 0; i < 16; i++) {
                outputs[OUT01_OUTPUT + i].setVoltage(0.0f);
                outputs[POLY_OUTPUT].setVoltage(0.0f, i);
            }
        }
    }
};

struct PageWidget : ModuleWidget {
    PageWidget(Page* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/page.svg")));

        // Poly output centered at top, then 16 outputs in two columns
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(15.993, 14.933)), module, Page::POLY_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 27.166)), module, Page::OUT01_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 27.166)), module, Page::OUT09_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 39.399)), module, Page::OUT02_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 39.399)), module, Page::OUT10_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 51.632)), module, Page::OUT03_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 51.632)), module, Page::OUT11_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 63.866)), module, Page::OUT04_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 63.866)), module, Page::OUT12_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 76.099)), module, Page::OUT05_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 76.099)), module, Page::OUT13_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 88.332)), module, Page::OUT06_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 88.332)), module, Page::OUT14_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 100.566)), module, Page::OUT07_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 100.566)), module, Page::OUT15_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 112.799)), module, Page::OUT08_OUTPUT));
        addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.654, 112.799)), module, Page::OUT16_OUTPUT));
    }
};

Model* modelPage = createModel<Page, PageWidget>("Page");
