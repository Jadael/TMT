#include "plugin.hpp"
#include "ports.hpp"
#include <chrono>
#include <ctime>

constexpr int timeUnits[] = {4, 60, 24, 7, 4, 12, 4, 1}; // Traditional divisions of each time unit

//Define the module
struct Calendar : Module {
	//Define parameters, inputs, outputs, and lights
	enum ParamId {
		TOGGLE_SWITCH,
		PARAMS_LEN
	};//end parameters
	enum InputId {
		INPUTS_LEN
	};//end inputs
	enum OutputId {
		SECOND_SMOOTH_OUTPUT,
		SECOND_STEPPED_OUTPUT,
		SECOND_TRIGGER_OUTPUT,
		SECOND_GATE_OUTPUT,
		SECOND_IGATE_OUTPUT,
		MINUTE_SMOOTH_OUTPUT,
		MINUTE_STEPPED_OUTPUT,
		MINUTE_TRIGGER_OUTPUT,
		MINUTE_GATE_OUTPUT,
		MINUTE_IGATE_OUTPUT,
		HOUR_SMOOTH_OUTPUT,
		HOUR_STEPPED_OUTPUT,
		HOUR_TRIGGER_OUTPUT,
		HOUR_GATE_OUTPUT,
		HOUR_IGATE_OUTPUT,
		DAY_SMOOTH_OUTPUT,
		DAY_STEPPED_OUTPUT,
		DAY_TRIGGER_OUTPUT,
		DAY_GATE_OUTPUT,
		DAY_IGATE_OUTPUT,
		WEEK_SMOOTH_OUTPUT,
		WEEK_STEPPED_OUTPUT,
		WEEK_TRIGGER_OUTPUT,
		WEEK_GATE_OUTPUT,
		WEEK_IGATE_OUTPUT,
		MONTH_SMOOTH_OUTPUT,
		MONTH_STEPPED_OUTPUT,
		MONTH_TRIGGER_OUTPUT,
		MONTH_GATE_OUTPUT,
		MONTH_IGATE_OUTPUT,
		SEASON_SMOOTH_OUTPUT,
		SEASON_STEPPED_OUTPUT,
		SEASON_TRIGGER_OUTPUT,
		SEASON_GATE_OUTPUT,
		SEASON_IGATE_OUTPUT,
		YEAR_SMOOTH_OUTPUT,
		YEAR_STEPPED_OUTPUT,
		YEAR_TRIGGER_OUTPUT,
		YEAR_GATE_OUTPUT,
		YEAR_IGATE_OUTPUT,
		OUTPUTS_LEN
	};//end outputs	
	enum LightId {
		LIGHTS_LEN
	};//end lights
	//Configure module
	Calendar() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Use UTC time instead of Local");
		configOutput(SECOND_SMOOTH_OUTPUT, "1 second linear ramp");
		configOutput(SECOND_STEPPED_OUTPUT, "1 second stepped ramp, 4 steps");
		configOutput(SECOND_TRIGGER_OUTPUT, "1 second trigger");
		configOutput(SECOND_GATE_OUTPUT, "1 second gate (high during first half)");
		configOutput(SECOND_IGATE_OUTPUT, "1 second inverted gate (high during second half)");
		configOutput(MINUTE_SMOOTH_OUTPUT, "1 minute linear ramp");
		configOutput(MINUTE_STEPPED_OUTPUT, "1 minute stepped ramp, 60 steps");
		configOutput(MINUTE_TRIGGER_OUTPUT, "1 minute trigger");
		configOutput(MINUTE_GATE_OUTPUT, "1 minute gate (high on first half)");
		configOutput(MINUTE_IGATE_OUTPUT, "1 minute inverted gate (high during second half)");
		configOutput(HOUR_SMOOTH_OUTPUT, "1 hour linear ramp");
		configOutput(HOUR_STEPPED_OUTPUT, "1 hour stepped ramp, 60 steps");
		configOutput(HOUR_TRIGGER_OUTPUT, "1 hour trigger");
		configOutput(HOUR_GATE_OUTPUT, "1 hour gate (high during first half)");
		configOutput(HOUR_IGATE_OUTPUT, "1 hour inverted gate (high during second half)");
		configOutput(DAY_SMOOTH_OUTPUT, "1 day linear ramp");
		configOutput(DAY_STEPPED_OUTPUT, "1 day stepped ramp, 24 steps");
		configOutput(DAY_TRIGGER_OUTPUT, "1 day trigger");
		configOutput(DAY_GATE_OUTPUT, "1 day gate (high during first half)");
		configOutput(DAY_IGATE_OUTPUT, "1 day inverted gate (high during second half)");
		configOutput(WEEK_SMOOTH_OUTPUT, "1 week linear ramp");
		configOutput(WEEK_STEPPED_OUTPUT, "1 week stepped ramp, 7 steps");
		configOutput(WEEK_TRIGGER_OUTPUT, "1 week trigger");
		configOutput(WEEK_GATE_OUTPUT, "1 week gate (high during first half)");
		configOutput(WEEK_IGATE_OUTPUT, "1 week inverted gate (high during second half)");
		configOutput(MONTH_SMOOTH_OUTPUT, "1 month linear ramp");
		configOutput(MONTH_STEPPED_OUTPUT, "1 month stepped ramp, 4 steps");
		configOutput(MONTH_TRIGGER_OUTPUT, "1 month trigger");
		configOutput(MONTH_GATE_OUTPUT, "1 month gate (high during first half)");
		configOutput(MONTH_IGATE_OUTPUT, "1 month inverted gate (high during second half)");
		configOutput(SEASON_SMOOTH_OUTPUT, "1 season linear ramp");
		configOutput(SEASON_STEPPED_OUTPUT, "1 season stepped ramp, 3 steps");
		configOutput(SEASON_TRIGGER_OUTPUT, "1 season trigger");
		configOutput(SEASON_GATE_OUTPUT, "1 season gate (high during first half)");
		configOutput(SEASON_IGATE_OUTPUT, "1 season inverted gate (high during second half)");
		configOutput(YEAR_SMOOTH_OUTPUT, "1 year linear ramp");
		configOutput(YEAR_STEPPED_OUTPUT, "1 year stepped ramp, 12 steps");
		configOutput(YEAR_TRIGGER_OUTPUT, "1 year trigger");
		configOutput(YEAR_GATE_OUTPUT, "1 year gate (high during first half)");
		configOutput(YEAR_IGATE_OUTPUT, "1 year inverted gate (high during second half)");
	}//end configuration
	
	float getTimeProgress(int timeUnitIndex, bool useUTC = false) {
		/**
		 * @brief Calculate the progress of the given time unit as a float from 0.0 to 1.0.
		 * 
		 * This function calculates the progress of the specified time unit by taking into account
		 * the current system time and milliseconds, providing a smooth linear increase.
		 * 
		 * @param timeUnitIndex An integer representing the time unit, where:
		 *                      0 - second
		 *                      1 - minute
		 *                      2 - hour
		 *                      3 - day
		 *                      4 - week
		 *                      5 - month
		 *                      6 - quarter
		 *                      7 - year
		 * @param useUTC Optional flag to use UTC time instead of local time (default is false).
		 * @return A float value representing the progress of the specified time unit, from 0.0 to 1.0.
		 *         If an invalid timeUnitIndex is provided, the function will return 0.0f.
		 * 
		 * Special concerns or shortcomings:
		 * - Assumes an average month duration of 30 days and an average year duration of 365 days.
		 * - The function may be affected by leap years and daylight saving time changes.
		 * - This function uses the C++11 standard library and the <chrono> library for time handling.
		 * - The function has been tested with g++ and the -std=c++11 flag.
		 */

		auto now = std::chrono::system_clock::now();
		std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
		std::tm timeInfo = useUTC ? *std::gmtime(&currentTime) : *std::localtime(&currentTime);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

		float currentValue, maxValue;
		float currentMillisecond = static_cast<float>(ms.count())/1000;
		float currentSecond = static_cast<float>(timeInfo.tm_sec);
		float currentMinute = static_cast<float>(timeInfo.tm_min);
		float currentHour = static_cast<float>(timeInfo.tm_hour);
		float currentWeekday = static_cast<float>(timeInfo.tm_wday);
		float currentMonthday = static_cast<float>(timeInfo.tm_mday);
		float currentQuartermonth = static_cast<float>(timeInfo.tm_mon % 3);
		float currentYearday = static_cast<float>(timeInfo.tm_yday);
		if (timeUnitIndex == 0) { // second
			currentValue = currentMillisecond;
			maxValue = 1.0f; // 1 decimal second
		} else if (timeUnitIndex == 1) { // minute
			currentValue = currentSecond + currentMillisecond;
			maxValue = 60.0f; // 60 decimal seconds
		} else if (timeUnitIndex == 2) { // hour
			currentValue = currentMinute * 60 + currentSecond + currentMillisecond;
			maxValue = 60.0f * 60; // sixty minutes per hour, converted to seconds, converted to ms
		} else if (timeUnitIndex == 3) { // day
			currentValue = currentHour * 60 * 60 + currentMinute * 60 + currentSecond + currentMillisecond;
			maxValue = 24.0f * 60 * 60;
		} else if (timeUnitIndex == 4) { // week
			currentValue = currentWeekday * 24 * 60 * 60  + currentHour * 60 * 60 + currentMinute * 60 + currentSecond + currentMillisecond;
			maxValue = 7.0f * 24 * 60 * 60;
		} else if (timeUnitIndex == 5) { // month
			currentValue = currentMonthday * 24 * 60 * 60 + currentHour * 60 * 60 + currentMinute * 6 + currentSecond + currentMillisecond;
			maxValue = 30.0f * 24 * 60 * 60;
		} else if (timeUnitIndex == 6) { // quarter
			currentValue = currentQuartermonth * 30 * 24 * 60 * 60  + currentMonthday * 24 * 60 * 60 + currentHour * 60 * 60 + currentMinute * 60 + currentSecond + currentMillisecond;
			maxValue = 3.0f * 30 * 24 * 60 * 60;
		} else if (timeUnitIndex == 7) { // year
			currentValue = currentYearday * 24 * 60 * 60 + currentHour * 60 * 60 + currentMinute * 60 + currentSecond + currentMillisecond;
			maxValue = 365.0f * 24 * 60 * 60;
		} else { // invalid index
			return 0.0f;
		}

		return currentValue / maxValue;
	}
	//Main loop
	void process(const ProcessArgs& args) override {
		//Limit to updating once per millisecond
		static auto lastUpdateTime = std::chrono::steady_clock::now();
		auto currentTime = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastUpdateTime).count() < 5) return;
		lastUpdateTime = currentTime;
		bool useUTC = params[TOGGLE_SWITCH].getValue() > 0.5f;
		
		// Update outputs
		for (int timeUnitIndex = 0; timeUnitIndex < 8; timeUnitIndex++) {
			float progress = getTimeProgress(timeUnitIndex, useUTC);
			float smooth = progress * 10.0f;
			float stepped = (std::floor(progress * timeUnits[timeUnitIndex]) / timeUnits[timeUnitIndex]) * 10.0f;
			float trigger = progress <= 0.1f / timeUnits[timeUnitIndex] ? 10.0f : 0.0f;
			float gate = progress < 0.5f ? 10.0f : 0.0f;
			float reverseGate = progress >= 0.5f ? 10.0f : 0.0f;
				for (int i = 0; i < 5; i++) {
				OutputId outId = static_cast<OutputId>(timeUnitIndex * 5 + i);
				switch (i) {
					case 0:
						outputs[outId].setVoltage(smooth);
						break;
					case 1:
						outputs[outId].setVoltage(stepped);
						break;
					case 2:
						outputs[outId].setVoltage(trigger);
						break;
					case 3:
						outputs[outId].setVoltage(gate);
						break;
					case 4:
						outputs[outId].setVoltage(reverseGate);
						break;
				}
			}
		}
	}//end main loop
};//end Calendar : Module

//Define the "panel" (e.g. the widget and visuals of the module)
struct CalendarWidget : ModuleWidget {
	CalendarWidget(Calendar* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/calendar.svg")));
		
		addParam(createParamCentered<BrassToggle>(mm2px(Vec(30, 6)), module, Calendar::TOGGLE_SWITCH));

		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 21.925)), module, Calendar::SECOND_SMOOTH_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.918, 21.925)), module, Calendar::SECOND_STEPPED_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(30.506, 21.925)), module, Calendar::SECOND_TRIGGER_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(40.093, 21.925)), module, Calendar::SECOND_GATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(49.681, 21.925)), module, Calendar::SECOND_IGATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 34.925)), module, Calendar::MINUTE_SMOOTH_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.918, 34.925)), module, Calendar::MINUTE_STEPPED_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(30.506, 34.925)), module, Calendar::MINUTE_TRIGGER_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(40.093, 34.925)), module, Calendar::MINUTE_GATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(49.681, 34.925)), module, Calendar::MINUTE_IGATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 47.925)), module, Calendar::HOUR_SMOOTH_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.918, 47.925)), module, Calendar::HOUR_STEPPED_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(30.506, 47.925)), module, Calendar::HOUR_TRIGGER_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(40.093, 47.925)), module, Calendar::HOUR_GATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(49.681, 47.925)), module, Calendar::HOUR_IGATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 60.925)), module, Calendar::DAY_SMOOTH_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.918, 60.925)), module, Calendar::DAY_STEPPED_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(30.506, 60.925)), module, Calendar::DAY_TRIGGER_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(40.093, 60.925)), module, Calendar::DAY_GATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(49.681, 60.925)), module, Calendar::DAY_IGATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 73.925)), module, Calendar::WEEK_SMOOTH_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.918, 73.925)), module, Calendar::WEEK_STEPPED_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(30.506, 73.925)), module, Calendar::WEEK_TRIGGER_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(40.093, 73.925)), module, Calendar::WEEK_GATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(49.681, 73.925)), module, Calendar::WEEK_IGATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 86.925)), module, Calendar::MONTH_SMOOTH_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.918, 86.925)), module, Calendar::MONTH_STEPPED_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(30.506, 86.925)), module, Calendar::MONTH_TRIGGER_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(40.093, 86.925)), module, Calendar::MONTH_GATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(49.681, 86.925)), module, Calendar::MONTH_IGATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 99.925)), module, Calendar::SEASON_SMOOTH_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.918, 99.925)), module, Calendar::SEASON_STEPPED_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(30.506, 99.925)), module, Calendar::SEASON_TRIGGER_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(40.093, 99.925)), module, Calendar::SEASON_GATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(49.681, 99.925)), module, Calendar::SEASON_IGATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(11.331, 112.925)), module, Calendar::YEAR_SMOOTH_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(20.918, 112.925)), module, Calendar::YEAR_STEPPED_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(30.506, 112.925)), module, Calendar::YEAR_TRIGGER_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(40.093, 112.925)), module, Calendar::YEAR_GATE_OUTPUT));
		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(49.681, 112.925)), module, Calendar::YEAR_IGATE_OUTPUT));
	}//end main widget
};//end panel

//Initialize the module
Model* modelCalendar = createModel<Calendar, CalendarWidget>("Calendar");