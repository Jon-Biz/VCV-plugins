#include "Patchbay.hpp"
#include "Widgets.hpp"
#include "Util.hpp"

/////////////
// modules //
/////////////

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
 
 	// Generate random, unique label for this Patchbay endpoint. Don't modify the sources map.
	std::string getOtherLabel() {
		std::string l;
		do {
			l = randomString(EditableTextBox::maxTextLength);
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
			configInput(i, string::f("Port %d", i + 1));
			label[i] = getLabel();
		}
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

};

struct PatchbayOutModule : Patchbay {

	bool sourceIsValid[NUM_PATCHBAY_INPUTS];

	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_1,
		OUTPUT_2,
		OUTPUT_3,
		OUTPUT_4,
		OUTPUT_5,
		OUTPUT_6,
		OUTPUT_7,
		OUTPUT_8,
		NUM_OUTPUTS
	};
	enum LightIds {
		OUTPUT_1_LIGHTG,
		OUTPUT_1_LIGHTR,
		OUTPUT_2_LIGHTG,
		OUTPUT_2_LIGHTR,
		OUTPUT_3_LIGHTG,
		OUTPUT_3_LIGHTR,
		OUTPUT_4_LIGHTG,
		OUTPUT_4_LIGHTR,
		OUTPUT_5_LIGHTG,
		OUTPUT_5_LIGHTR,
		OUTPUT_6_LIGHTG,
		OUTPUT_6_LIGHTR,
		OUTPUT_7_LIGHTG,
		OUTPUT_7_LIGHTR,
		OUTPUT_8_LIGHTG,
		OUTPUT_8_LIGHTR,
		NUM_LIGHTS
	};

	PatchbayOutModule() : Patchbay(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		assert(NUM_OUTPUTS == NUM_PATCHBAY_INPUTS);
		for(int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
			configOutput(i, string::f("Port %d", i + 1));
			label[i] = "";
			sourceIsValid[i] = false;
		}
	}

	void process(const ProcessArgs &args) override {
		for(int i=0; i  < NUM_PATCHBAY_INPUTS; i++) {
			std::string key = label[i];

			if(sourceExists(key)){
				PatchbayInModule *src = sources[label[i]];
				int idx = src->getInputIdx(label[i]);
				Input input = src->inputs[idx];
				
				const int channels = input.getChannels();
				outputs[OUTPUT_1 + i].setChannels(channels);
				for(int c = 0; c < channels; c++) {
					outputs[OUTPUT_1 + i].setVoltage(input.getVoltage(c), c);
				}
				lights[OUTPUT_1_LIGHTG + 2*i].setBrightness( input.isConnected());
				lights[OUTPUT_1_LIGHTR + 2*i].setBrightness(!input.isConnected());	
				sourceIsValid[i] = true;
			} else {
				outputs[i].setChannels(1);
				outputs[OUTPUT_1 + i].setVoltage(0.f);
				lights[OUTPUT_1_LIGHTG + 2*i].setBrightness(0.f);
				lights[OUTPUT_1_LIGHTR + 2*i].setBrightness(0.f);
				sourceIsValid[i] = false;
			}			
		}
	};

	json_t* dataToJson() override {
		json_t *data = json_object();

		for(int i=0; i  < NUM_PATCHBAY_INPUTS; i++) {
			// Create a character array to hold the concatenated string
			char buffer[6]; // Adjust the size as needed

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
			char buffer[6]; // Adjust the size as needed

			// Use snprintf to format the combination
			snprintf(buffer, sizeof(buffer), "label%d", i);

			// The buffer now contains the concatenated C-style string
			const char* key = buffer;

			json_t *label_json = json_object_get(root, key);
			if(json_is_string(label_json)) {
				label[i] = json_string_value(label_json);
			}
		}

	}
};

void Patchbay::addSource(PatchbayInModule *t) {
	for(int i=0; i  < NUM_PATCHBAY_INPUTS; i++) {
		std::string key = t->label[i];
		sources[key] = t; //TODO: mutex?
	}
}


////////////////////////////////////
// some Patchbay-specific widgets //
////////////////////////////////////

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

struct PatchbayLabelMenuItem : MenuItem {
	PatchbayOutModule *module;
	std::string label;
	int idx;
	void onAction(const event::Action &e) override {
		module->label[idx] = label;
	}
};

struct PatchbaySourceSelectorTextBox : HoverableTextBox, PatchbayLabelDisplay {
	PatchbayOutModule *module;
	int idx;

	PatchbaySourceSelectorTextBox() : HoverableTextBox() {}

	void onAction(const event::Action &e) override {
		// based on AudioDeviceChoice::onAction in src/app/AudioWidget.cpp
		Menu *menu = createMenu();
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Select source"));

		{
			PatchbayLabelMenuItem *item = new PatchbayLabelMenuItem();
			item->module = module;
			item->idx = idx;
			item->label = "";		
			item->text = "(none)";
			item->rightText = CHECKMARK(module->label[idx].empty());
			menu->addChild(item);
		}

		for (int i=0; i < NUM_PATCHBAY_INPUTS; i++) {
			if(!module->sourceIsValid[i] && !module->label[i].empty()) {
				// the source of the module doesn't exist, it shouldn't appear in sources, so display it as unavailable
				PatchbayLabelMenuItem *item = new PatchbayLabelMenuItem();
				item->module = module;
				item->idx = idx;
				item->label = module->label[i];
				item->text = module->label[i];
				item->text += " (missing)";
				item->rightText = CHECKMARK("true");
				menu->addChild(item);
			}

		}

		auto src = module->sources;

		for(auto it = src.begin(); it != src.end(); it++) {
			int idx = it->second->getInputIdx(it->first);

			PatchbayLabelMenuItem *item = new PatchbayLabelMenuItem();
			
			item->module = module;
			item->idx = idx;
			item->label = it->first;
			item->text = it->first;
			item->rightText = CHECKMARK(item->label == module->label[idx]);
			menu->addChild(item);
		}
	}

	void onButton(const event::Button &e) override {
		HoverableTextBox::onButton(e);
		bool l = e.button == GLFW_MOUSE_BUTTON_LEFT;
		bool r = e.button == GLFW_MOUSE_BUTTON_RIGHT;
		if(e.action == GLFW_RELEASE && (l || r)) {
			event::Action eAction;
			onAction(eAction);
			e.consume(this);
		}
	}

	void step() override {
		HoverableTextBox::step();
		if(!module) return;
		setText(module->label[idx]);
		textColor = module->sourceIsValid[idx] ? defaultTextColor : errorTextColor;
	}

};

// Custom PortWidget for Patchbay outputs, with custom tooltip behavior
struct PatchbayOutPortWidget : PJ301MPort {
	PatchbayOutPortTooltip* customTooltip = NULL;

	void createTooltip() {
		// same as PortWidget::craeteTooltip(), but internal->tooltip replaced with customTooltip
		if (!settings::tooltips)
			return;
		if (customTooltip)
			return;
		if (!module)
			return;

		PatchbayOutPortTooltip* tooltip = new PatchbayOutPortTooltip;
		tooltip->portWidget = this;
		APP->scene->addChild(tooltip);
		customTooltip = tooltip;
	}

	void destroyTooltip() {
		if(!customTooltip)
			return;
		APP->scene->removeChild(customTooltip);
		delete customTooltip;
		customTooltip = NULL;
	}

	// createTooltip cannot be overridden, so we have to manually override and
	// reimplement all the methods that call createTooltip.
	void onEnter(const EnterEvent& e) override {
		createTooltip();
		// don't call superclass onEnter, it calls its own createTooltip()
	}

	void onLeave(const LeaveEvent& e) override {
		destroyTooltip();
		// don't call superclass onLeave, it calls its own destroyTooltip()
	}

	void onDragDrop(const DragDropEvent& e) override {
		if(e.origin == this) {
			createTooltip();
		}
		DragDropEvent e2 = e;
		// HACK: this depends on the implementation detail that the superclass
		// onDragDrop will call createTooltip() if the origin is not null. Same
		// for onDragEnter.
		e2.origin = NULL;
		PJ301MPort::onDragDrop(e2);
	}

	void onDragEnter(const DragEnterEvent& e) override {
		PortWidget* pw = dynamic_cast<PortWidget*>(e.origin);
		if (pw) {
			createTooltip();
		}
		DragEnterEvent e2 = e;
		e2.origin = NULL;
		PJ301MPort::onDragEnter(e2);
	}

	void onDragLeave(const DragLeaveEvent& e) override {
		destroyTooltip();
		PJ301MPort::onDragLeave(e);
	}

	~PatchbayOutPortWidget() {
		destroyTooltip();
	}

};

void PatchbayOutPortTooltip::step() {
	// Based on PortTooltip::step(), but reworked to display also the label of
	// the incoming signal at the other end of the Patchbay if applicable.

	if (portWidget->module) {

		// The final tooltip text is going to have these four parts.
		std::string labelText = "";
		std::string description = ""; // Note: PatchbayOutPortWidget doesn't actually have a description, but this is here for completeness anyway.
		std::string voltageText = "";
		std::string cableText = "";

		// find out the corresponding Patchbay input
		// PatchbayOutModule* mod = dynamic_cast<PatchbayOutModule*>(portWidget->module);
		// PatchbayInModule* inputPatchbay = NULL;
		// if(mod && mod->sourceExists(mod->label[portWidget->idx])) {
		// 	inputPatchbay = mod->sources[mod->label[portWidget->idx]];
		// }

		engine::Port* port = portWidget->getPort();
		engine::PortInfo* portInfo = portWidget->getPortInfo();

		description = portInfo->getDescription();

		// Get voltage text based on the number of channels
		int channels = port->getChannels();
		for (int i = 0; i < channels; i++) {
			float v = port->getVoltage(i);
			// Add newline or comma
			voltageText += "\n";
			if (channels > 1)
				voltageText += string::f("%d: ", i + 1);
			voltageText += string::f("% .3fV", math::normalizeZero(v));
		}

		labelText = portInfo->getFullName();

		// Find the relevant cables: the cable going out of this port and the
		// cable of the corresponding port on the other end of the Patchbay. We
		// iterate over all cables, but that's fine, getCompleteCablesOnPort
		// would do that anyway.
		// for (widget::Widget* w : APP->scene->rack->getCableContainer()->children) {
		// 	CableWidget* cw = dynamic_cast<CableWidget*>(w);

		// 	if(!cw->isComplete())
		// 		continue;

			// if(cw->outputPort == portWidget) {
			// 	// we've found a cable that is outgoing from this port
			// 	// we know that the portWidget is always an output, so otherPw will be the cable input port.
			// 	PortWidget* otherPw = cw->inputPort;
			// 	if(!otherPw)
			// 		continue;

			// 	cableText += "\n";
			// 	// This widget is always instantiated on an output, so always say "To"
			// 	cableText += "To ";
			// 	cableText += otherPw->module->model->getFullName();
			// 	cableText += ": ";
			// 	cableText += otherPw->getPortInfo()->getName();
			// 	cableText += " ";
			// 	cableText += "input";

	// 		} else if(inputPatchbay
	// 		   && cw->inputPort
	// 		   && cw->outputPort
	// 		   && cw->inputPort->module == inputPatchbay
	// 		   && cw->inputPort->portId == portWidget->portId
	// 		  ) {
	// 			// cable is incoming to the other end of the corresponding
	// 			// Patchbay input, snag the label from it
	// 			labelText += "\n";
	// 			labelText += "Patchbaying from ";
	// 			labelText += cw->outputPort->module->model->getFullName();
	// 			labelText += ": ";
	// 			labelText += cw->outputPort->getPortInfo()->getName();
	// 			labelText += " ";
	// 			labelText += "output";

	// 		} else {
	// 			continue;
	// 		}

		// }

		// Assemble the final tooltip text.
		text = labelText;

		if(description != "") {
			text += "\n";
			text += description;
		}

		if(voltageText != "") {
			// voltageText already starts with newline
			text += voltageText;
		}

		if(cableText != "") {
			// cableText already starts with newline
			text += cableText;
		}

	}

	Tooltip::step();
	// Position at bottom-right of parameter
	box.pos = portWidget->getAbsoluteOffset(portWidget->box.size).round();
	// Fit inside parent (copied from Tooltip.cpp)
	assert(parent);
	box = box.nudge(parent->box.zeroPos());
};




////////////////////
// module widgets //
////////////////////

struct PatchbayModuleWidget : ModuleWidget {
	HoverableTextBox *labelDisplay;
	Patchbay *module;

	virtual void addLabelDisplay(HoverableTextBox *disp, int idx) {
		disp->font_size = 13;
		disp->box.size = Vec(70,16);
		disp->textOffset.x = disp->box.size.x * 0.5f;
		disp->box.pos = Vec(7.5f, (RACK_GRID_WIDTH + 21.0f) * (idx + 1));
		labelDisplay = disp;
		addChild(labelDisplay);
	}

	float getPortYCoord(int i) {
		return 57.f + 37.f * i;
	}

	PatchbayModuleWidget(Patchbay *module, std::string panelFilename) {
		setModule(module);
		this->module = module;
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, panelFilename)));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	}
};


struct PatchbayInModuleWidget : PatchbayModuleWidget {

	PatchbayInModuleWidget(PatchbayInModule *module) : PatchbayModuleWidget(module, "res/PatchbayIn.svg") {
		for(int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
			addLabelDisplay(new EditablePatchbayLabelTextbox(module, i), i);
			addInput(createInputCentered<PJ301MPort>(Vec(22.5, getPortYCoord(i)), module, PatchbayInModule::INPUT_1 + i));
		}
	}

};


struct PatchbayOutModuleWidget : PatchbayModuleWidget {
	PatchbaySourceSelectorTextBox *labelDisplay;

	PatchbayOutModuleWidget(PatchbayOutModule *module) : PatchbayModuleWidget(module, "res/PatchbayOut.svg") {
		for(int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
			labelDisplay = new PatchbaySourceSelectorTextBox();
			labelDisplay->module = module;
			labelDisplay->idx = i;
			addLabelDisplay(labelDisplay, i);

			float y = getPortYCoord(i);
			addOutput(createOutputCentered<PatchbayOutPortWidget>(Vec(22.5, y), module, PatchbayOutModule::OUTPUT_1 + i));
			addChild(createTinyLightForPort<GreenRedLight>(Vec(22.5, y), module, PatchbayOutModule::OUTPUT_1_LIGHTG + 2*i));
		}
	}

};


Model *modelPatchbayInModule = createModel<PatchbayInModule, PatchbayInModuleWidget>("PatchbayIn");
Model *modelPatchbayOutModule = createModel<PatchbayOutModule, PatchbayOutModuleWidget>("PatchbayOut");
