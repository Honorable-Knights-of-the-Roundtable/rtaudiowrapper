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
static void         (*_jack_get_latency_range)(jack_port_t*, jack_latency_callback_mode_t, jack_latency_range_t*) = nullptr;
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
#define jack_get_latency_range                _jack_get_latency_range
#define jack_port_get_latency                 _jack_port_get_latency

// ─── Loader ───────────────────────────────────────────────────────────────────

static void* _jack_lib_handle = nullptr;

// Returns true if JACK is available at runtime. Safe to call multiple times.
static bool rtaudio_jack_load() {
  if (_jack_lib_handle) return true;

  _jack_lib_handle = dlopen("libjack.so.0", RTLD_LAZY | RTLD_LOCAL);
  if (!_jack_lib_handle)
    _jack_lib_handle = dlopen("libjack.so", RTLD_LAZY | RTLD_LOCAL);
  if (!_jack_lib_handle)
    return false;

// Load a required symbol; bail if missing.
#define _JACK_SYM(name) \
  _##name = (decltype(_##name))dlsym(_jack_lib_handle, #name); \
  if (!_##name) { dlclose(_jack_lib_handle); _jack_lib_handle = nullptr; return false; }

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
  _JACK_SYM(jack_get_latency_range)
#undef _JACK_SYM

  // Deprecated in newer JACK versions — tolerate its absence
  _jack_port_get_latency = (decltype(_jack_port_get_latency))dlsym(_jack_lib_handle, "jack_port_get_latency");

  return true;
}

#endif  // __UNIX_JACK__
