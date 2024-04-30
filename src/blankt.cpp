#include "plugin.hpp"
#include "ports.hpp"

#define GRID_SNAP 10.16 // A 2hp grid in millimeters. 1 GRID_SNAP is just the right spacing for adjacent ports on the module

struct Blankt : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
	
	float width = 6; // Width in hp (RACK_GRID_WIDTH)

	Blankt() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		width = 6;
	}
	
	void fromJson(json_t* rootJ) override {
		Module::fromJson(rootJ);
		// In <1.0, module used "text" property at root level.
		json_t* widthJ = json_object_get(rootJ, "width");
		if (widthJ) {
			width = json_number_value(widthJ);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "width", json_real(width));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* widthJ = json_object_get(rootJ, "width");
		if (widthJ) {
			width = json_number_value(widthJ); 
		}
	}
	
	void process(const ProcessArgs& args) override {}
};

struct BlanktUndoRedoAction : history::ModuleAction {
	float old_width;
	float new_width;

	BlanktUndoRedoAction(int64_t id, float oldWidth, float newWidth) : old_width{oldWidth}, new_width{newWidth} {
		moduleId = id;
		name = "Blankt resize";
	}

	void undo() override {
		Blankt *module = dynamic_cast<Blankt*>(APP->engine->getModule(moduleId));
		if (module) {
			module->width = this->old_width;
		}
	}

	void redo() override {
	Blankt *module = dynamic_cast<Blankt*>(APP->engine->getModule(moduleId));
		if (module) {
			module->width = this->new_width;
		}
	}
};

struct BlanktResizeHandle : OpaqueWidget {
	Vec dragPos;
	Rect originalBox;
	Blankt* module;
	bool right = false;  // True for one on the right side.

	BlanktResizeHandle() {
	// One hole wide and full length tall.
		box.size = Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
	}

	void onDragStart(const DragStartEvent& e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;

		dragPos = APP->scene->rack->getMousePos();
		ModuleWidget* mw = getAncestorOfType<ModuleWidget>();
		assert(mw);
		originalBox = mw->box;
	}

	void onDragMove(const DragMoveEvent& e) override {
		ModuleWidget* mw = getAncestorOfType<ModuleWidget>();
		assert(mw);
		int original_width = module->width;

		Vec newDragPos = APP->scene->rack->getMousePos();
		float deltaX = newDragPos.x - dragPos.x;

		Rect newBox = originalBox;
		Rect oldBox = mw->box;
		// Minimum and maximum number of holes we allow the module to be.
		const float minWidth = 2 * RACK_GRID_WIDTH;
		const float maxWidth = 96 * RACK_GRID_WIDTH;
		if (right) {
			newBox.size.x += deltaX;
			newBox.size.x = std::fmax(newBox.size.x, minWidth);
			newBox.size.x = std::fmin(newBox.size.x, maxWidth);
			newBox.size.x = std::round(newBox.size.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH;
		} else {
			newBox.size.x -= deltaX;
			newBox.size.x = std::fmax(newBox.size.x, minWidth);
			newBox.size.x = std::fmin(newBox.size.x, maxWidth);
			newBox.size.x = std::round(newBox.size.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH;
			newBox.pos.x = originalBox.pos.x + originalBox.size.x - newBox.size.x;
		}
		// Set box and test whether it's valid.
		mw->box = newBox;
		if (!APP->scene->rack->requestModulePos(mw, newBox.pos)) {
			mw->box = oldBox;
		}
		module->width = std::round(mw->box.size.x / RACK_GRID_WIDTH); // Storing it here lets all the widgets see it
		
		if (original_width != module->width) { // Move to onDragEnd()?
			// Make resizing an undo/redo action. If I don't do this, undoing a
			// different module's move will cause them to overlap (aka, a
			// transporter malfunction).
			APP->history->push(new BlanktUndoRedoAction(module->id, original_width, module->width));
		}
	}
};

struct BlanktWidget : ModuleWidget {
	BlanktResizeHandle* rightHandle;
	SvgWidget* leftBrass;
	SvgWidget* rightBrass;
	
	BlanktWidget(Blankt* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/blank.svg")));
		box.size.x = 6 * RACK_GRID_WIDTH; // Default width (i.e. for browser
		// We have to manually resize immediately because the SVG is the width of the max size panel, ready to be cropped
		
		if (module) {
			box.size.x = module->width * RACK_GRID_WIDTH; // Storing width here lets all the widgets and Undo see it
		} else {
			box.size.x = 6 * RACK_GRID_WIDTH; // Default width (i.e. for browser
		}
		
        // Load and position left brass element
        leftBrass = new SvgWidget();
        leftBrass->setSvg(Svg::load(asset::plugin(pluginInstance, "res/brass_left.svg")));
        leftBrass->box.pos = Vec(0, 0); // Adjust position as needed
        addChild(leftBrass);

        // Load and position right brass element
        rightBrass = new SvgWidget();
        rightBrass->setSvg(Svg::load(asset::plugin(pluginInstance, "res/brass_right.svg")));
        rightBrass->box.pos = Vec(box.size.x - rightBrass->box.size.x, 0); // Initially position; adjust y as needed
        addChild(rightBrass);
		
		// Resize bar on right.
		//BlanktResizeHandle* rightHandle = createWidget<BlanktResizeHandle>(Vec(box.size.x - RACK_GRID_WIDTH, 0));
		rightHandle = new BlanktResizeHandle;
		rightHandle->box.pos.x = box.size.x - RACK_GRID_WIDTH; // Scoot to the right edge minus 1hp;
		rightHandle->right = true;
		rightHandle->module = module;
		// Make sure the handle is correctly placed if drawing for the module
		// browser.
		
		addChild(rightHandle);
	}
	
	void step() override {
		Blankt* module = dynamic_cast<Blankt*>(this->module);
		// While this is really only useful to call when the width changes,
		// I don't think it's currently worth the effort to ONLY call it then.
		// And maybe the *first* time step() is called.
		
		if (module) { // If the module is loaded
			box.size.x = module->width * RACK_GRID_WIDTH; // Get width from module
		} else { // Like when showing the module in the module browser.
			box.size.x = 6 * RACK_GRID_WIDTH; // Otherwise, default to 6hp
		}
		
		if (rightHandle && module) {
			rightHandle->box.pos.x = box.size.x - rightHandle->box.size.x;
		}
		
		if (rightBrass && module) {
			rightBrass->box.pos.x = box.size.x - rightBrass->box.size.x;
		}
		
		ModuleWidget::step();
	}
};

Model* modelBlankt = createModel<Blankt, BlanktWidget>("Blankt");