#include "plugin.hpp"
#include <vector>
#include <map>

#define NUM_PATCHBAY_INPUTS 8

struct PatchbayInModule;

struct Patchbay : Module {
	std::string label;
	Patchbay(int numParams, int numInputs, int numOutputs, int numLights = 0) {
		config(numParams, numInputs, numOutputs, numLights);
	}

	// This static map is used for keeping track of all existing Patchbay instances.
	// We're using a map instead of a set because it's easier to search.
	static std::map<std::string, PatchbayInModule*> sources;
	static std::string lastInsertedKey; // this is used to assign the label of an output initially

	void addSource(PatchbayInModule *t);

	inline bool sourceExists(std::string lbl) {
		return sources.find(lbl) != sources.end();
	}
};


// these have to be forward-declared here to make the implementation of step() possible, see cpp for details
struct PatchbayOutPortWidget;
struct PatchbayOutPortTooltip : ui::Tooltip {
	PatchbayOutPortWidget* portWidget;
	void step() override;
};


std::map<std::string, PatchbayInModule*> Patchbay::sources = {};
std::string Patchbay::lastInsertedKey = "";
