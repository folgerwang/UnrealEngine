#include "IMagicLeapControllerPlugin.h"
#include "MagicLeapController.h"
#include "Framework/Application/SlateApplication.h"

class FMagicLeapControllerPlugin : public IMagicLeapControllerPlugin
{
public:

	virtual void StartupModule() override
	{
		IMagicLeapControllerPlugin::StartupModule();

		// HACK: Generic Application might not be instantiated at this point so we create the input device with a 
		// dummy message handler. When the Generic Application creates the input device it passes a valid message
		// handler to it which is further on used for all the controller events. This hack fixes issues caused by
		// using a custom input device before the Generic Application has instantiated it. Eg. within BeginPlay()
		//
		// This also fixes the warnings that pop up on the custom input keys when the blueprint loads. Those
		// warnings are caused because Unreal loads the bluerints before the input device has been instantiated
		// and has added its keys, thus leading Unreal to believe that those keys don't exist. This hack causes
		// an earlier instantiation of the input device, and consequently, the custom keys.
		TSharedPtr<FGenericApplicationMessageHandler> DummyMessageHandler(new FGenericApplicationMessageHandler());
		CreateInputDevice(DummyMessageHandler.ToSharedRef());

		// Ideally, we should be able to query GetDefault<UMagicLeapSettings>()->bEnableZI directly.
		// Unfortunately, the UObject system hasn't finished initialization when this module has been loaded.
		bool bIsVDZIEnabled = false;
		GConfig->GetBool(TEXT("/Script/MagicLeap.MagicLeapSettings"), TEXT("bEnableZI"), bIsVDZIEnabled, GEngineIni);

		APISetup.Startup(bIsVDZIEnabled);
#if WITH_MLSDK
		APISetup.LoadDLL(TEXT("ml_input"));
#endif
	}

	virtual void ShutdownModule() override
	{
		APISetup.Shutdown();
		IMagicLeapControllerPlugin::ShutdownModule();
	}

	virtual TSharedPtr<class IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override
	{
		if (!InputDevice.IsValid())
		{
			TSharedPtr<IInputDevice> MagicLeapController(new FMagicLeapController(InMessageHandler));
			InputDevice = MagicLeapController;
			return InputDevice;
		}
		else
		{
			InputDevice.Get()->SetMessageHandler(InMessageHandler);
			return InputDevice;
		}
		return nullptr;
	}

	virtual TSharedPtr<IInputDevice> GetInputDevice() override
	{
		if (!InputDevice.IsValid())
		{
			InputDevice = CreateInputDevice(FSlateApplication::Get().GetPlatformApplication()->GetMessageHandler());
		}
		return InputDevice;
	}

private:
	FMagicLeapAPISetup APISetup;
	TSharedPtr<IInputDevice> InputDevice;
};

IMPLEMENT_MODULE(FMagicLeapControllerPlugin, MagicLeapController);

