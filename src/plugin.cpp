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

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelShuffle);
	p->addModel(modelCalendar);
	p->addModel(modelSeed);
	p->addModel(modelOuroboros);
	p->addModel(modelAppend);
	p->addModel(modelSight);
	p->addModel(modelSpellbook);
	p->addModel(modelPage);
	p->addModel(modelStats);
	p->addModel(modelBlankt);
	p->addModel(modelSort);
	p->addModel(modelSpine);
	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
