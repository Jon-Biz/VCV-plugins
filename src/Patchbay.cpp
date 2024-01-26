#include "Patchbay.hpp"
#include "Widgets.hpp"
#include "Util.hpp"

/////////////
// modules //
/////////////

//TODO: undo history for label/source change

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
			l = randomString(EditableTextBox::defaultMaxTextLength);
		} while(sourceExists(l)); // if the label exists, regenerate
		return l;
	}

	// Change the label of this input, if the label doesn't exist already.
	// Return whether the label was updated.
	bool updateLabel(std::string lbl) {
		if(lbl.empty() || sourceExists(lbl)) {
			return false;
		}
		sources.erase(label); //TODO: mutex for this and erase() calls below?
		label = lbl;
		addSource(this);
		return true;
	}

	PatchbayInModule() : Patchbay(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		assert(NUM_INPUTS == NUM_PATCHBAY_INPUTS);
		for(int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
			configInput(i, string::f("Port %d", i + 1));
		}
		label = getLabel();
		addSource(this);
	}

	~PatchbayInModule() {
		sources.erase(label);
	}

	// process() is not needed for a Patchbay source, values are read directly from inputs by Patchbay out

	json_t* dataToJson() override {
		json_t *data = json_object();
		json_object_set_new(data, "label", json_string(label.c_str()));
		return data;
	}

	void dataFromJson(json_t* root) override {
		json_t *label_json = json_object_get(root, "label");
		if(json_is_string(label_json)) {
			// remove previous label randomly generated in constructor
			sources.erase(label);
			label = std::string(json_string_value(label_json));
			if(sourceExists(label)) {
				// Label already exists in sources, this means that dataFromJson()
				// was called due to duplication instead of loading from file.
				// Generate new label.
				label = getLabel();
			}
		} else {
			// label couldn't be read from json for some reason, generate new one
			label = getLabel();
		}

		addSource(this);

	}

};

struct PatchbayOutModule : Patchbay {

	bool sourceIsValid;

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
		}
		if(sources.size() > 0) {
			if(sourceExists(lastInsertedKey)) {
				label = lastInsertedKey;
			} else {
				// the lastly added input doesn't exist anymore,
				// pick first input in alphabetical order
				label = sources.begin()->first;
			}
			sourceIsValid = true;
		} else {
			label = "";
			sourceIsValid = false;
		}
	}

	void process(const ProcessArgs &args) override {

		if(sourceExists(label)){
			PatchbayInModule *src = sources[label];
			for(int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
				Input input = src->inputs[PatchbayInModule::INPUT_1 + i];
				const int channels = input.getChannels();
				outputs[OUTPUT_1 + i].setChannels(channels);
				for(int c = 0; c < channels; c++) {
					outputs[OUTPUT_1 + i].setVoltage(input.getVoltage(c), c);
				}
				lights[OUTPUT_1_LIGHTG + 2*i].setBrightness( input.isConnected());
				lights[OUTPUT_1_LIGHTR + 2*i].setBrightness(!input.isConnected());
			}
			sourceIsValid = true;
		} else {
			for(int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
				outputs[i].setChannels(1);
				outputs[OUTPUT_1 + i].setVoltage(0.f);
				lights[OUTPUT_1_LIGHTG + 2*i].setBrightness(0.f);
				lights[OUTPUT_1_LIGHTR + 2*i].setBrightness(0.f);
			}
			sourceIsValid = false;
		}
	};

	json_t* dataToJson() override {
		json_t *data = json_object();
		json_object_set_new(data, "label", json_string(label.c_str()));
		return data;
	}

	void dataFromJson(json_t* root) override {
		json_t *label_json = json_object_get(root, "label");
		if(json_is_string(label_json)) {
			label = json_string_value(label_json);
		}
	}
};

void Patchbay::addSource(PatchbayInModule *t) {
	std::string key = t->label;
	sources[key] = t; //TODO: mutex?
	lastInsertedKey = key;
}


////////////////////////////////////
// some Patchbay-specific widgets //
////////////////////////////////////

struct PatchbayLabelDisplay {
	NVGcolor errorTextColor = nvgRGB(0xd8, 0x0, 0x0);
};

struct EditablePatchbayLabelTextbox : EditableTextBox, PatchbayLabelDisplay {
	PatchbayInModule *module;
	std::string errorText = "!err";
	GUITimer errorDisplayTimer;
	float errorDuration = 3.f;

	EditablePatchbayLabelTextbox(PatchbayInModule *m): EditableTextBox() {
		assert(errorText.size() <= maxTextLength);
		module = m;
	}

	void onDeselect(const event::Deselect &e) override {
		if(module->updateLabel(TextField::text) || module->label.compare(TextField::text) == 0) {
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
			HoverableTextBox::setText(module->label);
			if(!isFocused) {
				TextField::setText(module->label);
			}
		}
	}

};

struct PatchbayLabelMenuItem : MenuItem {
	PatchbayOutModule *module;
	std::string label;
	void onAction(const event::Action &e) override {
		module->label = label;
	}
};

struct PatchbaySourceSelectorTextBox : HoverableTextBox, PatchbayLabelDisplay {
	PatchbayOutModule *module;

	PatchbaySourceSelectorTextBox() : HoverableTextBox() {}

	void onAction(const event::Action &e) override {
		// based on AudioDeviceChoice::onAction in src/app/AudioWidget.cpp
		Menu *menu = createMenu();
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Select source"));

		{
			PatchbayLabelMenuItem *item = new PatchbayLabelMenuItem();
			item->module = module;
			item->label = "";
			item->text = "(none)";
			item->rightText = CHECKMARK(module->label.empty());
			menu->addChild(item);
		}

		if(!module->sourceIsValid && !module->label.empty()) {
			// the source of the module doesn't exist, it shouldn't appear in sources, so display it as unavailable
			PatchbayLabelMenuItem *item = new PatchbayLabelMenuItem();
			item->module = module;
			item->label = module->label;
			item->text = module->label;
			item->text += " (missing)";
			item->rightText = CHECKMARK("true");
			menu->addChild(item);
		}

		auto src = module->sources;
		for(auto it = src.begin(); it != src.end(); it++) {
			PatchbayLabelMenuItem *item = new PatchbayLabelMenuItem();
			item->module = module;
			item->label = it->first;
			item->text = it->first;
			item->rightText = CHECKMARK(item->label == module->label);
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
		setText(module->label);
		textColor = module->sourceIsValid ? defaultTextColor : errorTextColor;
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
		PatchbayOutModule* mod = dynamic_cast<PatchbayOutModule*>(portWidget->module);
		PatchbayInModule* inputPatchbay = NULL;
		if(mod && mod->sourceExists(mod->label)) {
			inputPatchbay = mod->sources[mod->label];
		}

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
		for (widget::Widget* w : APP->scene->rack->getCableContainer()->children) {
			CableWidget* cw = dynamic_cast<CableWidget*>(w);

			if(!cw->isComplete())
				continue;

			if(cw->outputPort == portWidget) {
				// we've found a cable that is outgoing from this port
				// we know that the portWidget is always an output, so otherPw will be the cable input port.
				PortWidget* otherPw = cw->inputPort;
				if(!otherPw)
					continue;

				cableText += "\n";
				// This widget is always instantiated on an output, so always say "To"
				cableText += "To ";
				cableText += otherPw->module->model->getFullName();
				cableText += ": ";
				cableText += otherPw->getPortInfo()->getName();
				cableText += " ";
				cableText += "input";

			} else if(inputPatchbay
			   && cw->inputPort
			   && cw->outputPort
			   && cw->inputPort->module == inputPatchbay
			   && cw->inputPort->portId == portWidget->portId
			  ) {
				// cable is incoming to the other end of the corresponding
				// Patchbay input, snag the label from it
				labelText += "\n";
				labelText += "Patchbaying from ";
				labelText += cw->outputPort->module->model->getFullName();
				labelText += ": ";
				labelText += cw->outputPort->getPortInfo()->getName();
				labelText += " ";
				labelText += "output";

			} else {
				continue;
			}

		}

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

	virtual void addLabelDisplay(HoverableTextBox *disp) {
		disp->font_size = 14;
		disp->box.size = Vec(30, 14);
		disp->textOffset.x = disp->box.size.x * 0.5f;
		disp->box.pos = Vec(7.5f, RACK_GRID_WIDTH + 7.0f);
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
		addLabelDisplay(new EditablePatchbayLabelTextbox(module));
		for(int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
			addInput(createInputCentered<PJ301MPort>(Vec(22.5, getPortYCoord(i)), module, PatchbayInModule::INPUT_1 + i));
		}
	}

};


struct PatchbayOutModuleWidget : PatchbayModuleWidget {
	PatchbaySourceSelectorTextBox *labelDisplay;

	PatchbayOutModuleWidget(PatchbayOutModule *module) : PatchbayModuleWidget(module, "res/PatchbayOut.svg") {
		labelDisplay = new PatchbaySourceSelectorTextBox();
		labelDisplay->module = module;
		addLabelDisplay(labelDisplay);

		for(int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
			float y = getPortYCoord(i);
			addOutput(createOutputCentered<PatchbayOutPortWidget>(Vec(22.5, y), module, PatchbayOutModule::OUTPUT_1 + i));
			addChild(createTinyLightForPort<GreenRedLight>(Vec(22.5, y), module, PatchbayOutModule::OUTPUT_1_LIGHTG + 2*i));
		}
	}

};


Model *modelPatchbayInModule = createModel<PatchbayInModule, PatchbayInModuleWidget>("PatchbayIn");
Model *modelPatchbayOutModule = createModel<PatchbayOutModule, PatchbayOutModuleWidget>("PatchbayOut");
