#include <vector>
#include <fstream>

#include "loader_lsd.h"


namespace lava {

using namespace boost::spirit;
using namespace boost::spirit::classic;

static rule<scanner_t> skipParser = blank_p | comment_p('#') | eol_p;

LoaderLSD::LoaderLSD(): LoaderBase() {
	mpSyntax = new SyntaxLSD(mpIface);
}

LoaderLSD::~LoaderLSD() {
	delete mpSyntax;
}

void LoaderLSD::parseLine(const std::string& line) {
	auto it = line.begin();
	//parse_info<iterator_t> pInfo 
	bool p = qi::parse(it, line.end(), *mpSyntax, skipParser);

	if (!pInfo.full) {
		if (pInfo.hit)
            std::cout << "Erreur d'analyse (offset " << pInfo.length << ") a la ligne: \"";
        else
            std::cout << "Erreur d'analyse (offset 0) a la ligne: \"";

		// Write error on standard output
        char_t currentChar = *pInfo.stop;
        bool isNewLine = currentChar == '\n' || currentChar == '\r';
        while (currentChar != 0 && !isNewLine) {
            std::cout << currentChar;
            ++pInfo.stop;

            currentChar = *pInfo.stop;
            isNewLine = currentChar == '\n' || currentChar == '\r';
        }
        std::cout << "\"" << std::endl;
	}
}

}  // namespace lava