#ifndef CONFIG_RESCORER_H__
#define CONFIG_RESCORER_H__

#include <string>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <travatar/config-base.h>

namespace travatar {

class ConfigRescorer : public ConfigBase {

public:

    ConfigRescorer() : ConfigBase() {
        minArgs_ = 0;
        maxArgs_ = 0;

        SetUsage(
"~~~ rescorer ~~~\n"
"  by Graham Neubig\n"
"\n"
"Rescores an n-best list according to some criterion\n"
"  Usage: travatar -nbest NBEST\n"
);

        AddConfigEntry("nbest", "", "File containing the n-best list (blank for stdin)");
        AddConfigEntry("debug", "0", "What level of debugging output to print");
        AddConfigEntry("weight_in", "", "File of initial weights");

    }
	
};

}

#endif