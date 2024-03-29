#pragma once

#include <iostream>
#include <thread>
#include <chrono>

#include "plugin.hpp"
#include "Patchbay.hpp"
#include "PatchbayIn.hpp"
/////////////
// modules //
/////////////

struct PatchbayOut : Patchbay {

	bool sourceIsValid[NUM_PATCHBAY_INPUTS];
	
	PatchbayIn* inputs[NUM_PATCHBAY_INPUTS];
	int inputIdx[NUM_PATCHBAY_INPUTS];

	std::string moduleId;

	bool isGreen = false;
	bool isRed = false;
	
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

	PatchbayOut() : Patchbay(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
		assert(NUM_OUTPUTS == NUM_PATCHBAY_INPUTS);

		for(int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
			configOutput(i, string::f("Port %d", i + 1));
			label[i] = "";
			sourceIsValid[i] = false;
			attachInputs();
			addDestination();
		}
	}

	rack::engine::Input getPort(std::string key) {
		Patchbay *src = sources[key];
		int idx = src->getIOIdx(key);

		return src->inputs[idx];
	}

	int setChannels(rack::engine::Input &input, rack::engine::Output &output) { 
		const int channels = input.getChannels();
		const int outChannels = output.getChannels();

		if (channels != outChannels) {
			// if the number of channels is different, set the number of channels
			// of the output to the number of channels of the input
			output.setChannels(channels);
		}

		return channels;
	}

	void setLights(rack::engine::Input &input, int idx) {
		bool isConnected = input.isConnected();

		if (isConnected && !isGreen) {
			lights[OUTPUT_1_LIGHTG + 2*idx].setBrightness( true );
			lights[OUTPUT_1_LIGHTR + 2*idx].setBrightness(false);
			isGreen = true;
			isRed = false;
		} else if (!isConnected && isGreen){
			lights[OUTPUT_1_LIGHTG + 2*idx].setBrightness(false);
			lights[OUTPUT_1_LIGHTR + 2*idx].setBrightness(true);
			isGreen = false;
			isRed = true;
		}
	}

	void process(const ProcessArgs &args) override {
		for(int i=0; i  < NUM_PATCHBAY_INPUTS; i++) {
			if (sourceIsValid[i]) {
				std::string key = label[i];
				PatchbayIn* pb = inputs[i];
				int inputIndex = inputIdx[i];

				rack::engine::Input input = pb->inputs[inputIndex];

				int channels = input.getChannels();

				for(int c = 0; c < channels; c++) {
					int voltage = input.getVoltage(c);
					outputs[i].setVoltage(voltage, c);
				}
				
				setLights(input, i);
			}
		}
	};

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
				label[i] = json_string_value(label_json);
			}
		}

		attachInputs();
		addDestination();
	}

	void attachInputs() {
		for(int i=0; i  < NUM_PATCHBAY_INPUTS; i++) {
			std::string key = label[i];

			if(sourceExists(key)){
				PatchbayIn* input = dynamic_cast<PatchbayIn*>(sources[key]);
				int input_Idx = input->getIOIdx(key);
				setInput(i, input, input_Idx);
			}
		}
	}

	void setInput(int idx, PatchbayIn* pbIn, int input_Idx) {
		inputs[idx] = pbIn;
		inputIdx[idx] = input_Idx;
		sourceIsValid[idx] = true;

		rack::engine::Input in = pbIn->inputs[input_Idx];
		setChannels(in, outputs[idx]);
	}

	void addDestination() {
		for(int i=0; i  < NUM_PATCHBAY_INPUTS; i++) {
			// each PatchbayOut is a destination with it's own unique key
			std::string key = getLabel();
			moduleId = key;
			destinations[key] = this; 
		}
	}

	void removeInput(int idx) {
		sourceIsValid[idx] = false;

		outputs[OUTPUT_1 + idx].setChannels(0);

		isGreen = false;
		isRed = false;

		lights[OUTPUT_1_LIGHTG + 2*idx].setBrightness(0.f);
		lights[OUTPUT_1_LIGHTR + 2*idx].setBrightness(0.f);
	}
};

// these have to be forward-declared here to make the implementation of step() possible, see cpp for details
struct PatchbayOutPortWidget;
struct PatchbayOutPortTooltip : ui::Tooltip {
	PatchbayOutPortWidget* portWidget;
	void step() override;
};

std::map<std::string, Patchbay*> Patchbay::destinations = {};

struct PatchbayLabelMenuItem : MenuItem {
	PatchbayOut *module;
	std::string label;
	int idx;
	void onAction(const event::Action &e) override {
		module->label[idx] = label;
		module->attachInputs();
	}
};

struct PatchbaySourceSelectorTextBox : HoverableTextBox, PatchbayLabelDisplay {
	PatchbayOut *module;
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
			// int idx = it->second->getInputIdx(it->first);

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
		// PatchbayOut* mod = dynamic_cast<PatchbayOut*>(portWidget->module);
		// PatchbayIn* inputPatchbay = NULL;
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

struct PatchbayOutWidget : PatchbayModuleWidget {
	PatchbaySourceSelectorTextBox *labelDisplay;

	PatchbayOutWidget(PatchbayOut *module) : PatchbayModuleWidget(module, "res/PB-I.svg") {
		for(int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
			labelDisplay = new PatchbaySourceSelectorTextBox();
			labelDisplay->module = module;
			labelDisplay->idx = i;
			addLabelDisplay(labelDisplay, i);

			addOutput(createOutputCentered<PatchbayOutPortWidget>(Vec(30, getPortYCoord(i)), module, PatchbayOut::OUTPUT_1 + i));
			addChild(createTinyLightForPort<GreenRedLight>(Vec(44, 11.0f + getLabelYCoord(i)), module, PatchbayOut::OUTPUT_1_LIGHTG + 2*i));
		}
	}
};

// Model *modelPatchbayOutModule = createModel<PatchbayOut, PatchbayOutWidget>("PatchbayOut");

