#ifndef PATCHBUILDER_H
#define PATCHBUILDER_H

#include <string>
#include "DBProvider/DBProvider.h"

using namespace std;

class PatchBuilder {
public:
  PatchBuilder(string pPatchListDirectory);
  ~PatchBuilder();
  void buildPatch(string directory);

private:
	string patchListDirectory;

	void createInstallPocket(string directory);
	void createObjectList(string directory);
	void createInstallScript(string directory);
	void createDependencyList(string directory);
};
#endif