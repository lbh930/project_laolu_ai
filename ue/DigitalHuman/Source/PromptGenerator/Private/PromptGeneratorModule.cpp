#include "Modules/ModuleManager.h"

class FPromptGeneratorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FPromptGeneratorModule, PromptGenerator)
