#include <vector>
#include <map>

#include "plugin.hpp"
#include "PatchbayIn.hpp"

#define NUM_PATCHBAY_INPUTS 8

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
			configInput(i, string::f("Port %d", i + 1));
			label[i] = getLabel();
		}
		
		addSource(this);
	}

	~PatchbayInModule() {
		eraseInputs();
	}

	void addSource(Patchbay *t) {
		for(int i=0; i  < NUM_PATCHBAY_INPUTS; i++) {
			std::string key = t->label[i];
			sources[key] = t;
		}
	}

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
		eraseInputs();
	}

	void eraseInputs() {
		for(int i=0; i < NUM_PATCHBAY_INPUTS; i++) {
			sources.erase(label[i]);
		}
	}
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

Model *modelPatchbayInModule = createModel<PatchbayInModule, PatchbayInModuleWidget>("PatchbayIn");
