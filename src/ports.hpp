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