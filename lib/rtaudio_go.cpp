// This file exists to allow cgo to compile the C++ RtAudio library
// It should be compiled as C++ and linked with the Go code

// dlopen loaders must come before RtAudio.cpp so their macros intercept
// all jack_* and pa_* call sites inside RtAudio's backend implementations.
#include "jack_loader.h"
#include "pulse_loader.h"

#include "RtAudio.cpp"
#include "rtaudio_c.cpp"
