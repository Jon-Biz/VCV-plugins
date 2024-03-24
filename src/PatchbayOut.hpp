#include "Patchbay.hpp"
#include "plugin.hpp"

// these have to be forward-declared here to make the implementation of step() possible, see cpp for details
struct PatchbayOutPortWidget;
struct PatchbayOutPortTooltip : ui::Tooltip {
	PatchbayOutPortWidget* portWidget;
	void step() override;
};

std::map<std::string, Patchbay*> Patchbay::destinations = {};
