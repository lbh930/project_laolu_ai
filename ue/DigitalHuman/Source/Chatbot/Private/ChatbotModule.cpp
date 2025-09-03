#include "Modules/ModuleManager.h"

class FChatbotModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogTemp, Display, TEXT("[Chatbot] StartupModule"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogTemp, Display, TEXT("[Chatbot] ShutdownModule"));
	}
};

IMPLEMENT_MODULE(FChatbotModule, Chatbot)
