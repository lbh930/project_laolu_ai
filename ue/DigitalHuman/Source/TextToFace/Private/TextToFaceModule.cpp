// Source/TextToFace/Private/TextToFaceModule.cpp
#include "Modules/ModuleManager.h"

class FTextToFaceModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FTextToFaceModule, TextToFace)
