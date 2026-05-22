// Copied from MIMUW course.
#ifndef MIM_ERR_H
#define MIM_ERR_H

#include <stdnoreturn.h>
#include <cstdint>

// Print information about a system error and quits.
[[noreturn]] void syserr(uint8_t verbosity, const char* fmt, ...);

// Print information about an error and quits.
[[noreturn]] void fatal(uint8_t verbosity, const char* fmt, ...);

// Print information about an error and return.
void error(uint8_t verbosity, const char* fmt, ...);

#endif
