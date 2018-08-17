#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoogleVRDummyTest, "GoogleVR.DummyTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGoogleVRDummyTest::RunTest(const FString& Parameters)
{
          return true;
}
