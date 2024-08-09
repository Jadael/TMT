/*
T's Musical Tools (TMT) - A collection of esoteric modules for VCV Rack, focused on manipulating RNG and polyphonic signals.
Copyright (C) 2024  T

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "plugin.hpp"
#include "ports.hpp"
#include <chrono>
#include <ctime>
#include <array>

// Constants for time unit divisions
constexpr int timeUnits[] = {4, 60, 24, 7, 4, 12, 3, 12}; // Subdivisions of each time unit
constexpr int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; // Days in each month

// Define the module
struct Calendar : Module {
    enum ParamId {
        TOGGLE_SWITCH,
        PARAMS_LEN
    };
    enum InputId {
        INPUTS_LEN
    };
    enum OutputId {
        SECOND_SMOOTH_OUTPUT, SECOND_STEPPED_OUTPUT, SECOND_TRIGGER_OUTPUT, SECOND_GATE_OUTPUT, SECOND_IGATE_OUTPUT,
        MINUTE_SMOOTH_OUTPUT, MINUTE_STEPPED_OUTPUT, MINUTE_TRIGGER_OUTPUT, MINUTE_GATE_OUTPUT, MINUTE_IGATE_OUTPUT,
        HOUR_SMOOTH_OUTPUT, HOUR_STEPPED_OUTPUT, HOUR_TRIGGER_OUTPUT, HOUR_GATE_OUTPUT, HOUR_IGATE_OUTPUT,
        DAY_SMOOTH_OUTPUT, DAY_STEPPED_OUTPUT, DAY_TRIGGER_OUTPUT, DAY_GATE_OUTPUT, DAY_IGATE_OUTPUT,
        WEEK_SMOOTH_OUTPUT, WEEK_STEPPED_OUTPUT, WEEK_TRIGGER_OUTPUT, WEEK_GATE_OUTPUT, WEEK_IGATE_OUTPUT,
        MONTH_SMOOTH_OUTPUT, MONTH_STEPPED_OUTPUT, MONTH_TRIGGER_OUTPUT, MONTH_GATE_OUTPUT, MONTH_IGATE_OUTPUT,
        SEASON_SMOOTH_OUTPUT, SEASON_STEPPED_OUTPUT, SEASON_TRIGGER_OUTPUT, SEASON_GATE_OUTPUT, SEASON_IGATE_OUTPUT,
        YEAR_SMOOTH_OUTPUT, YEAR_STEPPED_OUTPUT, YEAR_TRIGGER_OUTPUT, YEAR_GATE_OUTPUT, YEAR_IGATE_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        LIGHTS_LEN
    };

    // Time keeping
    float lastProgress[8] = {};  // Last calculated progress for each time unit
    double lastUpdateTime = 0.0;  // Last update in real time seconds
    std::tm timeInfo = {};
    std::time_t lastLocalTimeUpdate = 0; // Last update time for local time in seconds
    const std::time_t localTimeUpdateInterval = 1; // Update local time every 1 second

    Calendar() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Alt Mode: Use UTC time instead of Local");
        for (int i = 0; i < 8; ++i) {
            std::string unit;
            switch (i) {
                case 0: unit = "Second"; break;
                case 1: unit = "Minute"; break;
                case 2: unit = "Hour"; break;
                case 3: unit = "Day"; break;
                case 4: unit = "Week"; break;
                case 5: unit = "Month"; break;
                case 6: unit = "Season"; break;
                case 7: unit = "Year"; break;
            }
            configOutput(i * 5 + 0, unit + " smooth ramp");
            configOutput(i * 5 + 1, unit + " stepped ramp");
            configOutput(i * 5 + 2, unit + " trigger");
            configOutput(i * 5 + 3, unit + " gate (high during first half)");
            configOutput(i * 5 + 4, unit + " inverted gate (high during second half)");
        }
    }

    void updateLocalTime() {
        std::time_t currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        if (currentTime - lastLocalTimeUpdate >= localTimeUpdateInterval) {
            lastLocalTimeUpdate = currentTime;
#ifdef _POSIX_VERSION
            localtime_r(&currentTime, &timeInfo);
#else
            timeInfo = *localtime(&currentTime);
#endif
        }
    }

    void updateUTCTime() {
        std::time_t currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
#ifdef _POSIX_VERSION
        gmtime_r(&currentTime, &timeInfo);
#else
        timeInfo = *gmtime(&currentTime);
#endif
    }

    float getCurrentProgress(int unitIndex, float timeFraction) {
        switch (unitIndex) {
            case 0: return (timeInfo.tm_sec % 1 + timeFraction) / 1.0f;
            case 1: return (timeInfo.tm_sec + timeFraction) / 60.0f;
            case 2: return (timeInfo.tm_min * 60 + timeInfo.tm_sec + timeFraction) / 3600.0f;
            case 3: return (timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec + timeFraction) / 86400.0f;
            case 4: return ((timeInfo.tm_wday) * 86400 + timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec + timeFraction) / (7 * 86400.0f);
            case 5: {
                int daysInCurrentMonth = daysInMonth[timeInfo.tm_mon] + (timeInfo.tm_mon == 1 && (timeInfo.tm_year % 4 == 0 && (timeInfo.tm_year % 100 != 0 || timeInfo.tm_year % 400 == 0)));
                return ((timeInfo.tm_mday - 1) * 86400 + timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec + timeFraction) / (daysInCurrentMonth * 86400.0f);
            }
            case 6: {
                int seasonStartMonth = (timeInfo.tm_mon / 3) * 3;
                float seasonProgress = 0;
                for (int m = seasonStartMonth; m < timeInfo.tm_mon; ++m) {
                    seasonProgress += daysInMonth[m] * 86400;  // Average month length
                }
                return (seasonProgress + (timeInfo.tm_mday - 1) * 86400 + timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec + timeFraction) / (3 * 30 * 86400.0f);
            }
            case 7: return ((timeInfo.tm_yday * 86400) + timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec + timeFraction) / (365 * 86400.0f);
        }
        return 0.0f;
    }

    void updateOutputs(int unitIndex, float currentProgress, const ProcessArgs& args) {
        if (outputs[unitIndex * 5 + 0].isConnected())
            outputs[unitIndex * 5 + 0].setVoltage(currentProgress * 10.0f);
        if (outputs[unitIndex * 5 + 1].isConnected())
            outputs[unitIndex * 5 + 1].setVoltage(std::floor(currentProgress * timeUnits[unitIndex]) / timeUnits[unitIndex] * 10.0f);
        if (outputs[unitIndex * 5 + 2].isConnected())
            outputs[unitIndex * 5 + 2].setVoltage(currentProgress < (0.01f) ? 10.0f : 0.0f);
        if (outputs[unitIndex * 5 + 3].isConnected())
            outputs[unitIndex * 5 + 3].setVoltage(currentProgress < 0.5f ? 10.0f : 0.0f);
        if (outputs[unitIndex * 5 + 4].isConnected())
            outputs[unitIndex * 5 + 4].setVoltage(currentProgress >= 0.5f ? 10.0f : 0.0f);
    }

    void process(const ProcessArgs& args) override {
        bool useUTC = params[TOGGLE_SWITCH].getValue() > 0.5f;
        if (useUTC) {
            updateUTCTime();
        } else {
            updateLocalTime();
        }

        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        float timeFraction = ms.count() / 1000.0f;

        for (int i = 0; i < 8; ++i) {
            if (std::any_of(outputs.begin() + i * 5, outputs.begin() + (i * 5 + 5), [](Output& o) { return o.isConnected(); })) {
                float currentProgress = getCurrentProgress(i, timeFraction);
                updateOutputs(i, currentProgress, args);
            }
        }
    }
};

// Define the "panel" (e.g. the widget and visuals of the module)
struct CalendarWidget : ModuleWidget {
    CalendarWidget(Calendar* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/calendar.svg")));

        addParam(createParamCentered<BrassToggle>(mm2px(Vec(30, 6)), module, Calendar::TOGGLE_SWITCH));

        for (int i = 0; i < 8; ++i) {
            float y = 21.925 + i * 13;
            addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, y)), module, Calendar::SECOND_SMOOTH_OUTPUT + i * 5));
            addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.918, y)), module, Calendar::SECOND_STEPPED_OUTPUT + i * 5));
            addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(30.506, y)), module, Calendar::SECOND_TRIGGER_OUTPUT + i * 5));
            addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(40.093, y)), module, Calendar::SECOND_GATE_OUTPUT + i * 5));
            addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(49.681, y)), module, Calendar::SECOND_IGATE_OUTPUT + i * 5));
        }
    }
};

// Initialize the module
Model* modelCalendar = createModel<Calendar, CalendarWidget>("Calendar");