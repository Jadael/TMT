#include "plugin.hpp"
#include "ports.hpp"

struct Append : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		SIGNAL01_INPUT,
		SIGNAL02_INPUT,
		SIGNAL03_INPUT,
		SIGNAL04_INPUT,
		SIGNAL05_INPUT,
		SIGNAL06_INPUT,
		SIGNAL07_INPUT,
		SIGNAL08_INPUT,
		SIGNAL09_INPUT,
		SIGNAL10_INPUT,
		SIGNAL11_INPUT,
		SIGNAL12_INPUT,
		SIGNAL13_INPUT,
		SIGNAL14_INPUT,
		SIGNAL15_INPUT,
		SIGNAL16_INPUT,
		WIDTH_INPUT,
		ROTATION_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		POLY_OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Append() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(SIGNAL01_INPUT, "Signal 1");
		configInput(SIGNAL02_INPUT, "Signal 2");
		configInput(SIGNAL03_INPUT, "Signal 3");
		configInput(SIGNAL04_INPUT, "Signal 4");
		configInput(SIGNAL05_INPUT, "Signal 5");
		configInput(SIGNAL06_INPUT, "Signal 6");
		configInput(SIGNAL07_INPUT, "Signal 7");
		configInput(SIGNAL08_INPUT, "Signal 8");
		configInput(SIGNAL09_INPUT, "Signal 9");
		configInput(SIGNAL10_INPUT, "Signal 10");
		configInput(SIGNAL11_INPUT, "Signal 11");
		configInput(SIGNAL12_INPUT, "Signal 12");
		configInput(SIGNAL13_INPUT, "Signal 13");
		configInput(SIGNAL14_INPUT, "Signal 14");
		configInput(SIGNAL15_INPUT, "Signal 15");
		configInput(SIGNAL16_INPUT, "Signal 16");
		configInput(WIDTH_INPUT, "Output set width (0v: 1 channel, 10v: max channels");
		configInput(ROTATION_INPUT, "Output set starting point (0v: first channel from first signal, 10v: last channel from last signal)");
		configOutput(POLY_OUT_OUTPUT, "Polyphonic subset of voltages from inputs");
	}

	void process(const ProcessArgs& args) override {
		std::vector<float> buffer;
		buffer.reserve(16 * 16);

		// Gather all input signals into the buffer
		for (int i = SIGNAL01_INPUT; i <= SIGNAL16_INPUT; i++) {
			int channels = inputs[i].getChannels();
			for (int ch = 0; ch < channels; ch++) {
				buffer.push_back(inputs[i].getVoltage(ch));
			}
		}

		int buffer_size = buffer.size();

		if (buffer_size == 0) {
			int width = clamp(inputs[WIDTH_INPUT].isConnected() ? (int)rescale(inputs[WIDTH_INPUT].getVoltage(), 0.f, 10.f, 1, 17) : 16, 1, 16);
			for (int ch = 0; ch < width; ch++) {
				outputs[POLY_OUT_OUTPUT].setVoltage(0.f, ch);
			}
			outputs[POLY_OUT_OUTPUT].setChannels(width);
			return;
		}

		float width_input = inputs[WIDTH_INPUT].isConnected() ? clamp(inputs[WIDTH_INPUT].getVoltage(), 0.f, 10.f) : 10.f;
		float rotation_input = inputs[ROTATION_INPUT].isConnected() ? clamp(inputs[ROTATION_INPUT].getVoltage(), 0.f, 10.f) : 0.f;

		int output_channels = rescale(width_input, 0.f, 10.f, 1, buffer_size);
		output_channels = std::min(output_channels, 16);

		int rotation = rescale(rotation_input, 0.f, 10.f, 0, buffer_size - 1);

		// Fill the output with the selected voltages
		for (int ch = 0; ch < output_channels; ch++) {
			int index = (ch + rotation) % buffer_size;
			outputs[POLY_OUT_OUTPUT].setVoltage(buffer[index], ch);
		}
		outputs[POLY_OUT_OUTPUT].setChannels(output_channels);
	}

};


struct AppendWidget : ModuleWidget {
	AppendWidget(Append* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/append.svg")));

		addInput(createInputCentered<BrassPort>(mm2px(Vec(10.579, 13.37)), module, Append::SIGNAL01_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(19.901, 13.37)), module, Append::SIGNAL09_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(10.579, 24.545)), module, Append::SIGNAL02_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(19.901, 24.545)), module, Append::SIGNAL10_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(10.579, 35.72)), module, Append::SIGNAL03_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(19.901, 35.72)), module, Append::SIGNAL11_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(10.579, 46.895)), module, Append::SIGNAL04_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(19.901, 46.895)), module, Append::SIGNAL12_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(10.579, 58.07)), module, Append::SIGNAL05_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(19.901, 58.07)), module, Append::SIGNAL13_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(10.579, 69.245)), module, Append::SIGNAL06_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(19.901, 69.245)), module, Append::SIGNAL14_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(10.579, 80.42)), module, Append::SIGNAL07_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(19.901, 80.42)), module, Append::SIGNAL15_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(10.579, 91.595)), module, Append::SIGNAL08_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(19.901, 91.595)), module, Append::SIGNAL16_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(8.096, 106.388)), module, Append::WIDTH_INPUT));
		addInput(createInputCentered<BrassPort>(mm2px(Vec(22.384, 106.388)), module, Append::ROTATION_INPUT));

		addOutput(createOutputCentered<BrassPortOut>(mm2px(Vec(15.24, 112.842)), module, Append::POLY_OUT_OUTPUT));
	}
};


Model* modelAppend = createModel<Append, AppendWidget>("Append");
