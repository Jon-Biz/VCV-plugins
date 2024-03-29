#pragma once

#include <vector>
#include <map>

#include "plugin.hpp"
#include "Widgets.hpp"
#include "Util.hpp"

#define NUM_PATCHBAY_INPUTS 8
struct Patchbay : Module {
	std::string label[NUM_PATCHBAY_INPUTS];
	Patchbay(int numParams, int numInputs, int numOutputs, int numLights = 0) {
		config(numParams, numInputs, numOutputs, numLights);
	}

	// This static map is used for keeping track of all existing Patchbay instances.
	// We're using a map instead of a set because it's easier to search.
	static std::map<std::string, Patchbay*> sources;
	static std::map<std::string, Patchbay*> destinations;

	// Generate random, unique label for this Patchbay endpoint. Don't modify the sources map.
	std::string getLabel() {
		std::string l;
		do {
			l = randomString(EditableTextBox::defaultTextLength);
		} while(sourceExists(l) || destinationExists(l)); // if the label exists, regenerate
		return l;
	}

	inline bool sourceExists(std::string lbl) {
		return (
			sources.find(lbl) != sources.end()
		);
	}


	inline bool destinationExists(std::string lbl) {
		return (
			destinations.find(lbl) != destinations.end()
		);
	}

	int getIOIdx(std::string lbl) {
		for (int i = 0; i < NUM_PATCHBAY_INPUTS; i++) {
			if (lbl.compare(label[i]) == 0) {
				return i;
			}
		}

		return 0;
	}

	void setInput(int idx, Patchbay* pbIn, int input_Idx) {
	}

	void removeInput(int idx) {
	}
};

struct PatchbayLabelDisplay {
	NVGcolor errorTextColor = nvgRGB(0xd8, 0x0, 0x0);
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

