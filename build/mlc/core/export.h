#pragma once

#ifdef _MSC_VER
#define MLC_API __declspec(dllexport)
#else
#define MLC_API __attribute__((visibility("default")))
#endif