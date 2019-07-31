#include "PatchInstaller/FileParser.h"
#include "PatchInstaller/PatchInstaller.h"

#include <iostream>
#include <fstream>
#include <string>

FileParser::FileParser() {}
FileParser::~FileParser() {}

DBObjects FileParser::getResultOfParsing(std::string nameOfFile) 
{
	FileParser parser;
	return parser.parse(nameOfFile);
}



DBObjects FileParser::parse(std::string nameOfFile)
{
	DBObjects objectParametersFromFile;
	std::ifstream dependencies(nameOfFile, std::ios::in);

	std::string buffer("");
	std::string scheme("");
	std::string objectName("");
	std::string objectType("");

	//Try to read first string from file
	dependencies >> scheme >>objectName >> objectType;
	if ((objectName != "") && (objectType != "")) {
		while (getline(dependencies, buffer)) {
			objectParametersFromFile.emplace_back(scheme, objectName, objectType);
			dependencies >> scheme >> objectName >> objectType;
		}
	}
	objectParametersFromFile.emplace_back(scheme, objectName, objectType);
	dependencies.close();
	return objectParametersFromFile;
}


