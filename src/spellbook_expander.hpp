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

#pragma once

#define SPELLBOOK_BASE_COLUMNS 16 // Columns handled by base module
#define MAX_EXPANDER_COLUMNS 512  // Support up to 512 columns total (16 base + 31 Pages x 16 = 512)

// Expander message structure shared between Spellbook and Page modules
// Spellbook sends pre-calculated voltages for all columns to Page expanders
struct SpellbookExpanderMessage {
    int64_t baseID = -1;           // ID of the base Spellbook module
    int position = 0;              // Position in the chain (1=first Page, 2=second, etc.)
    int currentStep = 0;           // Current step in the sequence
    int totalSteps = 0;            // Total number of steps in the sequence
    int totalColumns = 0;          // Total number of columns in current step
    float outputVoltages[MAX_EXPANDER_COLUMNS];  // Pre-calculated output voltages for all columns
};
