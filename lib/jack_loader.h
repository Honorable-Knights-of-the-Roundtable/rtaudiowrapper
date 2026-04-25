// jack_loader.h — Runtime dlopen loader for JACK
//
// Include this BEFORE RtAudio.cpp. It pulls in <jack/jack.h> for types, then
// replaces every jack_* call site with a function-pointer dereference via macros.
// The binary therefore has no hard link dependency on libjack.so.
//
// Call rtaudio_jack_load() before using the JACK backend. Returns true if the
// library was found and all required symbols resolved.
#pragma once
#ifdef __UNIX_JACK__

#include <cstdio>
#include <dlfcn.h>
#include <jack/jack.h>

// ─── Function pointer variables ───────────────────────────────────────────────
// Named with a leading _ so they don't clash with the jack.h declarations.

typedef jack_client_t* (*_jack_client_open_fn_t)(const char*, jack_options_t, jack_status_t*, ...);

static _jack_client_open_fn_t         _jack_client_open                                       = nullptr;
static int          (*_jack_client_close)(jack_client_t*)                                      = nullptr;
static int          (*_jack_activate)(jack_client_t*)                                          = nullptr;
static int          (*_jack_deactivate)(jack_client_t*)                                        = nullptr;
static jack_nframes_t (*_jack_get_sample_rate)(jack_client_t*)                                 = nullptr;
static jack_nframes_t (*_jack_get_buffer_size)(jack_client_t*)                                 = nullptr;
static const char** (*_jack_get_ports)(jack_client_t*, const char*, const char*, unsigned long) = nullptr;
static jack_port_t* (*_jack_port_register)(jack_client_t*, const char*, const char*, unsigned long, unsigned long) = nullptr;
static int          (*_jack_port_unregister)(jack_client_t*, jack_port_t*)                     = nullptr;
static int          (*_jack_connect)(jack_client_t*, const char*, const char*)                 = nullptr;
static void*        (*_jack_port_get_buffer)(jack_port_t*, jack_nframes_t)                     = nullptr;
static const char*  (*_jack_port_name)(const jack_port_t*)                                     = nullptr;
static jack_port_t* (*_jack_port_by_name)(jack_client_t*, const char*)                        = nullptr;
static int          (*_jack_set_process_callback)(jack_client_t*, JackProcessCallback, void*) = nullptr;
static int          (*_jack_set_xrun_callback)(jack_client_t*, JackXRunCallback, void*)       = nullptr;
static void         (*_jack_on_shutdown)(jack_client_t*, JackShutdownCallback, void*)         = nullptr;
static int          (*_jack_set_client_registration_callback)(jack_client_t*, JackClientRegistrationCallback, void*) = nullptr;
static void         (*_jack_set_error_function)(void (*)(const char*))                        = nullptr;
static void         (*_jack_port_get_latency_range)(jack_port_t*, jack_latency_callback_mode_t, jack_latency_range_t*) = nullptr;
static jack_nframes_t (*_jack_port_get_latency)(jack_port_t*)                                  = nullptr;  // deprecated

// ─── Redirect all call sites through the pointers ─────────────────────────────

#define jack_client_open                      _jack_client_open
#define jack_client_close                     _jack_client_close
#define jack_activate                         _jack_activate
#define jack_deactivate                       _jack_deactivate
#define jack_get_sample_rate                  _jack_get_sample_rate
#define jack_get_buffer_size                  _jack_get_buffer_size
#define jack_get_ports                        _jack_get_ports
#define jack_port_register                    _jack_port_register
#define jack_port_unregister                  _jack_port_unregister
#define jack_connect                          _jack_connect
#define jack_port_get_buffer                  _jack_port_get_buffer
#define jack_port_name                        _jack_port_name
#define jack_port_by_name                     _jack_port_by_name
#define jack_set_process_callback             _jack_set_process_callback
#define jack_set_xrun_callback                _jack_set_xrun_callback
#define jack_on_shutdown                      _jack_on_shutdown
#define jack_set_client_registration_callback _jack_set_client_registration_callback
#define jack_set_error_function               _jack_set_error_function
#define jack_port_get_latency_range           _jack_port_get_latency_range
#define jack_port_get_latency                 _jack_port_get_latency

// ─── Loader ───────────────────────────────────────────────────────────────────

static void* _jack_lib_handle = nullptr;

// PipeWire ships its own libjack at a non-standard path. On systems where both
// jackd2 and pipewire-jack are installed, dlopen("libjack.so.0") finds jackd2
// first but its server isn't running. We detect that by doing a test connection
// after loading symbols — if it fails we try PipeWire's path instead.
static const char* _jack_candidate_paths[] = {
  "libjack.so.0",
  "libjack.so",
  "/usr/lib/x86_64-linux-gnu/pipewire-0.3/jack/libjack.so.0",
  "/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack/libjack.so.0",
  "/usr/lib/pipewire-0.3/jack/libjack.so.0",
  nullptr
};

// Attempt to load all required symbols from handle into the _* pointers.
// Returns false and clears handle on any missing symbol.
static bool _jack_load_syms(void* handle) {
#define _JACK_SYM(name) \
  _##name = (decltype(_##name))dlsym(handle, #name); \
  if (!_##name) { fprintf(stderr, "[jack_loader]   missing symbol: " #name "\n"); return false; }

  _JACK_SYM(jack_client_open)
  _JACK_SYM(jack_client_close)
  _JACK_SYM(jack_activate)
  _JACK_SYM(jack_deactivate)
  _JACK_SYM(jack_get_sample_rate)
  _JACK_SYM(jack_get_buffer_size)
  _JACK_SYM(jack_get_ports)
  _JACK_SYM(jack_port_register)
  _JACK_SYM(jack_port_unregister)
  _JACK_SYM(jack_connect)
  _JACK_SYM(jack_port_get_buffer)
  _JACK_SYM(jack_port_name)
  _JACK_SYM(jack_port_by_name)
  _JACK_SYM(jack_set_process_callback)
  _JACK_SYM(jack_set_xrun_callback)
  _JACK_SYM(jack_on_shutdown)
  _JACK_SYM(jack_set_client_registration_callback)
  _JACK_SYM(jack_set_error_function)
  _JACK_SYM(jack_port_get_latency_range)
#undef _JACK_SYM
  // Deprecated in newer JACK — tolerate its absence
  _jack_port_get_latency = (decltype(_jack_port_get_latency))dlsym(handle, "jack_port_get_latency");
  return true;
}

// Returns true if JACK is available at runtime. Safe to call multiple times.
static bool rtaudio_jack_load() {
  if (_jack_lib_handle) return true;

  fprintf(stderr, "[jack_loader] rtaudio_jack_load() called\n");

  for (int i = 0; _jack_candidate_paths[i]; ++i) {
    fprintf(stderr, "[jack_loader] trying: %s\n", _jack_candidate_paths[i]);
    void* handle = dlopen(_jack_candidate_paths[i], RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
      fprintf(stderr, "[jack_loader]   dlopen failed: %s\n", dlerror());
      continue;
    }
    fprintf(stderr, "[jack_loader]   dlopen OK\n");

    if (!_jack_load_syms(handle)) {
      fprintf(stderr, "[jack_loader]   symbol load failed\n");
      dlclose(handle);
      continue;
    }
    fprintf(stderr, "[jack_loader]   symbols OK, probing server\n");

    // Verify a server is actually reachable before committing to this library.
    // Use JackNoStartServer so we don't accidentally launch a daemon.
    jack_status_t status;
    jack_client_t* probe = _jack_client_open("rtaudio_probe",
                                              (jack_options_t)0x01 /*JackNoStartServer*/,
                                              &status);
    if (!probe) {
      fprintf(stderr, "[jack_loader]   probe failed (status=0x%x), trying next\n", (unsigned)status);
      // This library loaded fine but has no live server — try the next candidate.
      dlclose(handle);
      continue;
    }
    fprintf(stderr, "[jack_loader]   probe OK — using this library\n");
    _jack_client_close(probe);

    _jack_lib_handle = handle;
    return true;
  }

  fprintf(stderr, "[jack_loader] no working JACK library found\n");
  return false;
}

#endif  // __UNIX_JACK__
