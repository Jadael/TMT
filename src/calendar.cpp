#include "plugin.hpp"
#include "ports.hpp"
#include <chrono>
#include <ctime>
#include <time.h> // Include this for gmtime_r

constexpr int timeUnits[] = {4, 60, 24, 7, 4, 12, 3, 12}; // Traditional subdivisions of each time unit

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
		configParam(TOGGLE_SWITCH, 0.f, 1.f, 0.f, "Alt Mode: Use UTC time instead of Local");
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
	
  // Time keeping
  float lastProgress[8] = {};  // Last calculated progress for each time unit
  double lastUpdateTime = 0.0;  // Last update in real time seconds
  struct tm timeInfo;
  time_t lastLocalTimeUpdate = 0; // Last update time for local time in seconds
  const time_t localTimeUpdateInterval = 1; // Update local time every 1 second

  void process(const ProcessArgs& args) override {
      using namespace std::chrono;
      bool useUTC = params[TOGGLE_SWITCH].getValue() > 0.5f;

      // Current time in system_clock, then convert to time_t
      auto now = system_clock::now();
      time_t currentTime = system_clock::to_time_t(now);
      auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

      if (useUTC) {
      #ifdef _POSIX_VERSION
          gmtime_r(&currentTime, &timeInfo); // Use thread-safe version if available
      #else
          timeInfo = *gmtime(&currentTime);  // Fallback to standard gmtime
      #endif
      } else {
          // Only update local time if the current time exceeds the last update interval
          if (currentTime - lastLocalTimeUpdate >= localTimeUpdateInterval) {
              lastLocalTimeUpdate = currentTime;
          #ifdef _POSIX_VERSION
              localtime_r(&currentTime, &timeInfo); // Use thread-safe version if available
          #else
              timeInfo = *localtime(&currentTime);  // Fallback to standard localtime
          #endif
          }
      }

      // Precompute time step progress
      float timeStepProgress[8];
      for (int i = 0; i < 8; i++) {
          timeStepProgress[i] = args.sampleTime * timeUnits[i] / (timeUnits[i] == 1 ? 365 * 86400 : timeUnits[i] * (i == 5 ? 30 * 86400 : 86400));
      }

      float currentProgress[8];
      for (int i = 0; i < 8; i++) {
          if (std::any_of(outputs.begin() + i * 5, outputs.begin() + (i * 5 + 5), [](Output& o) { return o.isConnected(); })) {
              switch (i) {
                  case 0: {
                      currentProgress[i] = (timeInfo.tm_sec % 4 + ms.count() / 1000.0f) / 4.0f;
                  } break;
                  case 1: {
                      currentProgress[i] = (timeInfo.tm_sec + ms.count() / 1000.0f) / 60.0f;
                  } break;
                  case 2: {
                      currentProgress[i] = (timeInfo.tm_min * 60 + timeInfo.tm_sec + ms.count() / 1000.0f) / 3600.0f;
                  } break;
                  case 3: {
                      currentProgress[i] = (timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec + ms.count() / 1000.0f) / 86400.0f;
                  } break;
                  case 4: {
                      currentProgress[i] = ((timeInfo.tm_wday) * 86400 + timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec + ms.count() / 1000.0f) / (7 * 86400.0f);
                  } break;
                  case 5: {
                      int daysInMonth = 30;
                      switch (timeInfo.tm_mon) {
                          case 0: case 2: case 4: case 6: case 7: case 9: case 11: daysInMonth = 31; break;
                          case 1: daysInMonth = (timeInfo.tm_year % 4 == 0 && (timeInfo.tm_year % 100 != 0 || timeInfo.tm_year % 400 == 0)) ? 29 : 28; break;
                          case 3: case 5: case 8: case 10: daysInMonth = 30; break;
                      }
                      currentProgress[i] = ((timeInfo.tm_mday - 1) * 86400 + timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec + ms.count() / 1000.0f) / (daysInMonth * 86400.0f);
                  } break;
                  case 6: {
                      int month = timeInfo.tm_mon;
                      int seasonStartMonth = (month / 3) * 3;
                      float seasonProgress = 0;
                      for (int m = seasonStartMonth; m < month; m++) {
                          seasonProgress += 30 * 86400;  // Simplified average month length
                      }
                      currentProgress[i] = (seasonProgress + (timeInfo.tm_mday - 1) * 86400 + timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec + ms.count() / 1000.0f) / (3 * 30 * 86400.0f);
                  } break;
                  case 7: {
                      currentProgress[i] = ((timeInfo.tm_yday * 86400) + timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec + ms.count() / 1000.0f) / (365 * 86400.0f);
                  } break;
              }
              currentProgress[i] = fmod(currentProgress[i] + timeStepProgress[i], 1.0f);

              // Update outputs
              if (outputs[i * 5].isConnected())
                  outputs[i * 5].setVoltage(currentProgress[i] * 10.0f);
              if (outputs[i * 5 + 1].isConnected())
                  outputs[i * 5 + 1].setVoltage(floor(currentProgress[i] * timeUnits[i]) / timeUnits[i] * 10.0f);
              if (outputs[i * 5 + 2].isConnected())
                  outputs[i * 5 + 2].setVoltage(currentProgress[i] < (1.0f / args.sampleRate) ? 10.0f : 0.0f);
              if (outputs[i * 5 + 3].isConnected())
                  outputs[i * 5 + 3].setVoltage(currentProgress[i] < 0.5f ? 10.0f : 0.0f);
              if (outputs[i * 5 + 4].isConnected())
                  outputs[i * 5 + 4].setVoltage(currentProgress[i] >= 0.5f ? 10.0f : 0.0f);
          }
      }
  }
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