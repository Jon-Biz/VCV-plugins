#include <vector>
#include <map>

#include "plugin.hpp"
#include "Widgets.hpp"
#include "Util.hpp"

#define NUM_PATCHBAY_INPUTS 8

struct PatchbayInModule;
struct Patchbay : Module {
	std::string label[NUM_PATCHBAY_INPUTS];
	Patchbay(int numParams, int numInputs, int numOutputs, int numLights = 0) {
		config(numParams, numInputs, numOutputs, numLights);
	}

	// This static map is used for keeping track of all existing Patchbay instances.
	// We're using a map instead of a set because it's easier to search.
	static std::map<std::string, PatchbayInModule*> sources;

	void addSource(PatchbayInModule *t);

	inline bool sourceExists(std::string lbl) {
		return sources.find(lbl) != sources.end();
	}
};

struct PatchbayInModule : Patchbay {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		INPUT_1,
		INPUT_2,
		INPUT_3,
		INPUT_4,
		INPUT_5,
		INPUT_6,
		INPUT_7,
		INPUT_8,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	// Generate random, unique label for this Patchbay endpoint. Don't modify the sources map.
	std::string getLabel() {
		std::string l;
		do {
			l = randomString(EditableTextBox::defaultTextLength);
		} while(sourceExists(l)); // if the label exists, regenerate
		return l;
	}

	int getInputIdx(std::string lbl) {
		for (int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
			if (lbl.compare(label[i]) == 0) {
				return i;
			}
		}

		return 0;
	}

	// Change the label of this input, if the label doesn't exist already.
	// Return whether the label was updated.
	bool updateLabel(std::string lbl, int idx = 0) {
		if(lbl.empty() || sourceExists(lbl)) {
			return false;
		}

		std::string oldLabel = label[idx];
		sources.erase(oldLabel); //TODO: mutex for this and erase() calls below?
		label[idx] = lbl;
		addSource(this);

		return true;
	}

	PatchbayInModule() : Patchbay(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		assert(NUM_INPUTS == NUM_PATCHBAY_INPUTS);
		for(int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
			DEBUG("herein %i", i);
			configInput(i, string::f("Port %d", i + 1));
			DEBUG("herein2");
			label[i] = getLabel();
		}
		DEBUG("herein3");
		
		addSource(this);
	}

	~PatchbayInModule() {
		sources.erase(label[0]);
	}

	// process() is not needed for a Patchbay source, values are read directly from inputs by Patchbay out

	json_t* dataToJson() override {
		json_t *data = json_object();

		for(int i=0; i  < NUM_PATCHBAY_INPUTS; i++) {
			// Create a character array to hold the concatenated string
			char buffer[16]; // Adjust the size as needed

			// Use snprintf to format the combination
			snprintf(buffer, sizeof(buffer), "label%d", i);

			// The buffer now contains the concatenated C-style string
			const char* key = buffer;

			json_object_set_new(data, key, json_string(label[i].c_str()));
		}

		return data;
	}

	void dataFromJson(json_t* root) override {

		for(int i=0; i  < NUM_PATCHBAY_INPUTS; i++) {
			// Create a character array to hold the concatenated string
			char buffer[16]; // Adjust the size as needed

			// Use snprintf to format the combination
			snprintf(buffer, sizeof(buffer), "label%d", i);

			// The buffer now contains the concatenated C-style string
			const char* key = buffer;

			json_t *label_json = json_object_get(root, key);
			if(json_is_string(label_json)) {
				// remove previous label randomly generated in constructor
				sources.erase(label[i]);
				label[i] = std::string(json_string_value(label_json));

				if(sourceExists(label[i])) {
					// Label already exists in sources, this means that dataFromJson()
					// was called due to duplication instead of loading from file.
					// Generate new label.
					label[i] = getLabel();
				}
			} else {
				// label couldn't be read from json for some reason, generate new one
				label[i] = getLabel();
			}
		}

		addSource(this);
	}

	void onRemove (const RemoveEvent & e) override {
		for(int i=0; i < NUM_PATCHBAY_INPUTS; i++) {
			sources.erase(label[i]);
		}
	}
};

struct PatchbayModuleWidget : ModuleWidget {
	HoverableTextBox *labelDisplay;
	Patchbay *module;

	virtual void addLabelDisplay(HoverableTextBox *disp, int idx) {
		disp->font_size = 12;
		disp->box.size = Vec(50,12);
		disp->textOffset.x = disp->box.size.x * 0.5f;
		disp->box.pos = Vec(5.0f, getLabelYCoord(idx));
		labelDisplay = disp;
		addChild(labelDisplay);
	}

	float getPortYCoord(int i) {
		return 54.0f + (RACK_GRID_WIDTH + 27.0f) * (i);
	}

	float getLabelYCoord(int i) {
	
		return 30.0f + ((RACK_GRID_WIDTH + 27.0f) * (i));
	}

	PatchbayModuleWidget(Patchbay *module, std::string panelFilename) {
		setModule(module);
		this->module = module;
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, panelFilename)));

		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH * 0.5f, 0)));
		// addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH * 0.5f, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		// addChild(createWidget<ScrewSilver>(Vec(45.0f - (RACK_GRID_WIDTH * 0.5f), 0)));
		// addChild(createWidget<ScrewSilver>(Vec(45.0f - (RACK_GRID_WIDTH * 0.5f), RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	}
};

struct PatchbayLabelDisplay {
	NVGcolor errorTextColor = nvgRGB(0xd8, 0x0, 0x0);
};

struct EditablePatchbayLabelTextbox : EditableTextBox, PatchbayLabelDisplay {
	PatchbayInModule *module;
	int idx;
	std::string errorText = "!err";
	GUITimer errorDisplayTimer;
	float errorDuration = 3.f;

	EditablePatchbayLabelTextbox(PatchbayInModule *m, int idx): EditableTextBox() {
		assert(errorText.size() <= maxTextLength);
		this->idx = idx;
		module = m;
	}

	void onDeselect(const event::Deselect &e) override {
		if(module->updateLabel(TextField::text, idx) || module->label[idx].compare(TextField::text) == 0) {
			errorDisplayTimer.reset();
		} else {
			errorDisplayTimer.trigger(errorDuration);
		}

		isFocused = false;
		e.consume(NULL);
	}

	void step() override {
		EditableTextBox::step();
		if(!module) return;
		if(errorDisplayTimer.process()) {
			textColor = isFocused ? defaultTextColor : errorTextColor;
			HoverableTextBox::setText(errorText);
		} else {
			textColor = defaultTextColor;
			HoverableTextBox::setText(module->label[idx]);
			if(!isFocused) {
				TextField::setText(module->label[idx]);
			}
		}
	}

};


struct PatchbayInModuleWidget : PatchbayModuleWidget {
	PatchbayInModuleWidget(PatchbayInModule *module) : PatchbayModuleWidget(module, "res/PB-O.svg") {
		for(int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
			addLabelDisplay(new EditablePatchbayLabelTextbox(module, i), i);
			addInput(createInputCentered<PJ301MPort>(Vec(30, getPortYCoord(i)), module, PatchbayInModule::INPUT_1 + i));
		}
	}

};



// these have to be forward-declared here to make the implementation of step() possible, see cpp for details
struct PatchbayOutPortWidget;
struct PatchbayOutPortTooltip : ui::Tooltip {
	PatchbayOutPortWidget* portWidget;
	void step() override;
};

std::map<std::string, PatchbayInModule*> Patchbay::sources = {};

Model *modelPatchbayInModule = createModel<PatchbayInModule, PatchbayInModuleWidget>("PatchbayIn");
