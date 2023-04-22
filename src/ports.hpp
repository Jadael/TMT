struct BrassPort : app::SvgPort {
	BrassPort() {
		shadow->opacity = 0.0;
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/port.svg")));
	}
};

struct BrassPortOut : app::SvgPort {
	BrassPortOut() {
		shadow->opacity = 0.0;
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/port_out.svg")));
	}
};

struct BrassToggle : app::SvgSwitch {
	BrassToggle() {
		shadow->opacity = 0.0;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/toggleswitch_on.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/toggleswitch_off.svg")));
	}
};