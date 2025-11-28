#pragma once

// Detect if we are building the DLL
#if defined(BLVCKWINPIPE_DLL)
    #if defined(BLVCKWINPIPE_EXPORTS)
        // Building DLL
        #define BLVCKWINPIPE_API __declspec(dllexport)
    #else
        // Using DLL
        #define BLVCKWINPIPE_API __declspec(dllimport)
    #endif
#else
    // Static library or header-only
    #define BLVCKWINPIPE_API
#endif