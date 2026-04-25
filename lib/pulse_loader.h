// pulse_loader.h — Runtime dlopen loader for PulseAudio
//
// Include this BEFORE RtAudio.cpp. It pulls in PulseAudio headers for types,
// then replaces every pa_* call site with a function-pointer dereference via
// macros. The binary therefore has no hard link dependency on libpulse.so or
// libpulse-simple.so.
//
// Call rtaudio_pulse_load() before using the PulseAudio backend. Returns true
// if both libraries were found and all required symbols resolved.
#pragma once
#ifdef __LINUX_PULSE__

#include <dlfcn.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

// ─── Function pointer variables ───────────────────────────────────────────────

// libpulse — async / mainloop API
static pa_mainloop*      (*_pa_mainloop_new)(void)                                                    = nullptr;
static pa_mainloop_api*  (*_pa_mainloop_get_api)(pa_mainloop*)                                        = nullptr;
static int               (*_pa_mainloop_run)(pa_mainloop*, int*)                                      = nullptr;
static void              (*_pa_mainloop_free)(pa_mainloop*)                                           = nullptr;

static pa_context*       (*_pa_context_new)(pa_mainloop_api*, const char*)                            = nullptr;
static pa_context*       (*_pa_context_new_with_proplist)(pa_mainloop_api*, const char*, pa_proplist*) = nullptr;
static int               (*_pa_context_connect)(pa_context*, const char*, pa_context_flags_t, const pa_spawn_api*) = nullptr;
static void              (*_pa_context_unref)(pa_context*)                                            = nullptr;
static pa_context_state_t (*_pa_context_get_state)(const pa_context*)                                 = nullptr;
static void              (*_pa_context_set_state_callback)(pa_context*, pa_context_notify_cb_t, void*) = nullptr;
static pa_operation*     (*_pa_context_get_server_info)(pa_context*, pa_server_info_cb_t, void*)      = nullptr;
static pa_operation*     (*_pa_context_get_sink_info_list)(pa_context*, pa_sink_info_cb_t, void*)     = nullptr;
static pa_operation*     (*_pa_context_get_source_info_list)(pa_context*, pa_source_info_cb_t, void*) = nullptr;
static int               (*_pa_context_errno)(const pa_context*)                                      = nullptr;

static const char*       (*_pa_strerror)(int)                                                         = nullptr;
static void              (*_pa_xfree)(void*)                                                          = nullptr;
static const char*       (*_pa_proplist_gets)(pa_proplist*, const char*)                              = nullptr;

// libpulse-simple — synchronous stream API
static pa_simple*  (*_pa_simple_new)(const char*, const char*, pa_stream_direction_t, const char*,
                                     const char*, const pa_sample_spec*, const pa_channel_map*,
                                     const pa_buffer_attr*, int*)                                     = nullptr;
static void        (*_pa_simple_free)(pa_simple*)                                                     = nullptr;
static int         (*_pa_simple_read)(pa_simple*, void*, size_t, int*)                               = nullptr;
static int         (*_pa_simple_write)(pa_simple*, const void*, size_t, int*)                        = nullptr;
static int         (*_pa_simple_drain)(pa_simple*, int*)                                             = nullptr;
static int         (*_pa_simple_flush)(pa_simple*, int*)                                             = nullptr;

// ─── Redirect all call sites through the pointers ─────────────────────────────

#define pa_mainloop_new                   _pa_mainloop_new
#define pa_mainloop_get_api               _pa_mainloop_get_api
#define pa_mainloop_run                   _pa_mainloop_run
#define pa_mainloop_free                  _pa_mainloop_free
#define pa_context_new                    _pa_context_new
#define pa_context_new_with_proplist      _pa_context_new_with_proplist
#define pa_context_connect                _pa_context_connect
#define pa_context_unref                  _pa_context_unref
#define pa_context_get_state              _pa_context_get_state
#define pa_context_set_state_callback     _pa_context_set_state_callback
#define pa_context_get_server_info        _pa_context_get_server_info
#define pa_context_get_sink_info_list     _pa_context_get_sink_info_list
#define pa_context_get_source_info_list   _pa_context_get_source_info_list
#define pa_context_errno                  _pa_context_errno
#define pa_strerror                       _pa_strerror
#define pa_xfree                          _pa_xfree
#define pa_proplist_gets                  _pa_proplist_gets
#define pa_simple_new                     _pa_simple_new
#define pa_simple_free                    _pa_simple_free
#define pa_simple_read                    _pa_simple_read
#define pa_simple_write                   _pa_simple_write
#define pa_simple_drain                   _pa_simple_drain
#define pa_simple_flush                   _pa_simple_flush

// ─── Loader ───────────────────────────────────────────────────────────────────

static void* _pulse_lib_handle        = nullptr;
static void* _pulse_simple_lib_handle = nullptr;

// Returns true if PulseAudio is available at runtime. Safe to call multiple times.
static bool rtaudio_pulse_load() {
  if (_pulse_lib_handle && _pulse_simple_lib_handle) return true;

  _pulse_lib_handle = dlopen("libpulse.so.0", RTLD_LAZY | RTLD_LOCAL);
  if (!_pulse_lib_handle)
    _pulse_lib_handle = dlopen("libpulse.so", RTLD_LAZY | RTLD_LOCAL);
  if (!_pulse_lib_handle)
    return false;

  _pulse_simple_lib_handle = dlopen("libpulse-simple.so.0", RTLD_LAZY | RTLD_LOCAL);
  if (!_pulse_simple_lib_handle)
    _pulse_simple_lib_handle = dlopen("libpulse-simple.so", RTLD_LAZY | RTLD_LOCAL);
  if (!_pulse_simple_lib_handle) {
    dlclose(_pulse_lib_handle);
    _pulse_lib_handle = nullptr;
    return false;
  }

#define _PULSE_SYM(handle, name) \
  _##name = (decltype(_##name))dlsym(handle, #name); \
  if (!_##name) { \
    dlclose(_pulse_simple_lib_handle); _pulse_simple_lib_handle = nullptr; \
    dlclose(_pulse_lib_handle);        _pulse_lib_handle = nullptr; \
    return false; \
  }

  _PULSE_SYM(_pulse_lib_handle, pa_mainloop_new)
  _PULSE_SYM(_pulse_lib_handle, pa_mainloop_get_api)
  _PULSE_SYM(_pulse_lib_handle, pa_mainloop_run)
  _PULSE_SYM(_pulse_lib_handle, pa_mainloop_free)
  _PULSE_SYM(_pulse_lib_handle, pa_context_new)
  _PULSE_SYM(_pulse_lib_handle, pa_context_new_with_proplist)
  _PULSE_SYM(_pulse_lib_handle, pa_context_connect)
  _PULSE_SYM(_pulse_lib_handle, pa_context_unref)
  _PULSE_SYM(_pulse_lib_handle, pa_context_get_state)
  _PULSE_SYM(_pulse_lib_handle, pa_context_set_state_callback)
  _PULSE_SYM(_pulse_lib_handle, pa_context_get_server_info)
  _PULSE_SYM(_pulse_lib_handle, pa_context_get_sink_info_list)
  _PULSE_SYM(_pulse_lib_handle, pa_context_get_source_info_list)
  _PULSE_SYM(_pulse_lib_handle, pa_context_errno)
  _PULSE_SYM(_pulse_lib_handle, pa_strerror)
  _PULSE_SYM(_pulse_lib_handle, pa_xfree)
  _PULSE_SYM(_pulse_lib_handle, pa_proplist_gets)
  _PULSE_SYM(_pulse_simple_lib_handle, pa_simple_new)
  _PULSE_SYM(_pulse_simple_lib_handle, pa_simple_free)
  _PULSE_SYM(_pulse_simple_lib_handle, pa_simple_read)
  _PULSE_SYM(_pulse_simple_lib_handle, pa_simple_write)
  _PULSE_SYM(_pulse_simple_lib_handle, pa_simple_drain)
  _PULSE_SYM(_pulse_simple_lib_handle, pa_simple_flush)
#undef _PULSE_SYM

  return true;
}

#endif  // __LINUX_PULSE__
