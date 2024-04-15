#pragma once
#include <rack.hpp>

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelShuffle;
extern Model* modelCalendar;
extern Model* modelSeed;
extern Model* modelOuroboros;
extern Model* modelAppend;
extern Model* modelSight;
extern Model* modelSpellbook;