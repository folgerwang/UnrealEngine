#if defined _WIN32 || defined _WIN64
#define DLLIMPORT __declspec(dllimport)
#elif defined __linux__
#define DLLIMPORT __attribute__((visibility("default")))
#else
#define DLLIMPORT
#endif

DLLIMPORT void ExampleLibraryFunction();
