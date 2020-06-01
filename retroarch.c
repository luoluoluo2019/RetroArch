/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#if defined(DEBUG) && defined(HAVE_DRMINGW)
#include "exchndl.h"
#endif
#endif

#if defined(DINGUX)
#include <sys/types.h>
#include <unistd.h>
#endif

#if (defined(__linux__) || defined(__unix__) || defined(DINGUX)) && !defined(EMSCRIPTEN)
#include <signal.h>
#endif

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)
#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#include <objbase.h>
#include <process.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <math.h>
#include <locale.h>

#include <boolean.h>
#include <clamping.h>
#include <string/stdstring.h>
#include <streams/stdin_stream.h>
#include <dynamic/dylib.h>
#include <file/config_file.h>
#include <lists/string_list.h>
#include <retro_math.h>
#include <retro_timers.h>
#include <encodings/utf.h>

#include <gfx/scaler/pixconv.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <libretro.h>
#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>

#include <features/features_cpu.h>

#include <compat/strl.h>
#include <compat/strcasestr.h>
#include <compat/getopt.h>
#include <audio/conversion/float_to_s16.h>
#include <audio/conversion/s16_to_float.h>
#ifdef HAVE_AUDIOMIXER
#include <audio/audio_mixer.h>
#endif
#include <audio/dsp_filter.h>
#include <compat/posix_string.h>
#include <streams/file_stream.h>
#include <streams/interface_stream.h>
#include <file/file_path.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <queues/message_queue.h>
#include <queues/task_queue.h>
#include <lists/dir_list.h>
#ifdef HAVE_NETWORKING
#include <net/net_http.h>
#endif

#ifdef WIIU
#include <wiiu/os/energy.h>
#endif

#ifdef HAVE_LIBNX
#include <switch.h>
#include "switch_performance_profiles.h"
#endif

#ifdef HAVE_DISCORD
#include "deps/discord-rpc/include/discord_rpc.h"
#endif

#include "config.def.h"
#include "config.def.keybinds.h"

#include "runtime_file.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#include <net/net_socket.h>
#endif

#include <audio/audio_resampler.h>

#include "gfx/gfx_animation.h"
#include "gfx/gfx_display.h"

#include "input/input_osk.h"

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#include "menu/menu_input.h"
#include "menu/widgets/menu_dialog.h"
#include "menu/widgets/menu_input_bind_dialog.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "menu/menu_shader.h"
#endif
#endif

#ifdef HAVE_GFX_WIDGETS
#include "gfx/gfx_widgets.h"
#endif

#include "input/input_mapper.h"
#include "input/input_keymaps.h"
#include "input/input_remapping.h"

#ifdef HAVE_CHEEVOS
#include "cheevos/cheevos.h"
#include "cheevos/fixup.h"
#endif

#ifdef HAVE_TRANSLATE
#include <encodings/base64.h>
#include <formats/rbmp.h>
#include <formats/rpng.h>
#include "translation_defines.h"
#endif

#ifdef HAVE_DISCORD
#include "network/discord.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#include "network/netplay/netplay_private.h"
#include "network/netplay/netplay_discovery.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#if defined(HAVE_OPENGL)
#include "gfx/common/gl_common.h"
#elif defined(HAVE_OPENGL_CORE)
#include "gfx/common/gl_core_common.h"
#endif

#include "autosave.h"
#include "command.h"
#include "config.features.h"
#include "cores/internal_cores.h"
#include "content.h"
#include "core_type.h"
#include "core_info.h"
#include "dynamic.h"
#include "defaults.h"
#include "driver.h"
#include "msg_hash.h"
#include "paths.h"
#include "file_path_special.h"
#include "ui/ui_companion_driver.h"
#include "verbosity.h"

#include "frontend/frontend_driver.h"
#ifdef HAVE_THREADS
#include "gfx/video_thread_wrapper.h"
#endif
#include "gfx/video_display_server.h"
#include "gfx/video_crt_switch.h"
#include "wifi/wifi_driver.h"
#include "led/led_driver.h"
#include "midi/midi_driver.h"
#include "core.h"
#include "configuration.h"
#include "list_special.h"
#include "managers/core_option_manager.h"
#include "managers/cheat_manager.h"
#include "managers/state_manager.h"
#ifdef HAVE_AUDIOMIXER
#include "tasks/task_audio_mixer.h"
#endif
#include "tasks/task_content.h"
#include "tasks/tasks_internal.h"
#include "performance_counters.h"

#include "version.h"
#include "version_git.h"

#include "retroarch.h"

#ifdef HAVE_ACCESSIBILITY
#include "accessibility.h"
#endif

#ifdef HAVE_THREADS
#include "audio/audio_thread_wrapper.h"
#endif

/* DRIVERS */

audio_driver_t audio_null = {
   NULL, /* init */
   NULL, /* write */
   NULL, /* stop */
   NULL, /* start */
   NULL, /* alive */
   NULL, /* set_nonblock_state */
   NULL, /* free */
   NULL, /* use_float */
   "null",
   NULL,
   NULL,
   NULL, /* write_avail */
   NULL
};

static const audio_driver_t *audio_drivers[] = {
#ifdef HAVE_ALSA
   &audio_alsa,
#if !defined(__QNX__) && defined(HAVE_THREADS)
   &audio_alsathread,
#endif
#endif
#ifdef HAVE_TINYALSA
	&audio_tinyalsa,
#endif
#if defined(HAVE_AUDIOIO)
   &audio_audioio,
#endif
#if defined(HAVE_OSS) || defined(HAVE_OSS_BSD)
   &audio_oss,
#endif
#ifdef HAVE_RSOUND
   &audio_rsound,
#endif
#ifdef HAVE_COREAUDIO
   &audio_coreaudio,
#endif
#ifdef HAVE_COREAUDIO3
   &audio_coreaudio3,
#endif
#ifdef HAVE_AL
   &audio_openal,
#endif
#ifdef HAVE_SL
   &audio_opensl,
#endif
#ifdef HAVE_ROAR
   &audio_roar,
#endif
#ifdef HAVE_JACK
   &audio_jack,
#endif
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &audio_sdl,
#endif
#ifdef HAVE_XAUDIO
   &audio_xa,
#endif
#ifdef HAVE_DSOUND
   &audio_dsound,
#endif
#ifdef HAVE_WASAPI
   &audio_wasapi,
#endif
#ifdef HAVE_PULSE
   &audio_pulse,
#endif
#ifdef __CELLOS_LV2__
   &audio_ps3,
#endif
#ifdef XENON
   &audio_xenon360,
#endif
#ifdef GEKKO
   &audio_gx,
#endif
#ifdef WIIU
   &audio_ax,
#endif
#ifdef EMSCRIPTEN
   &audio_rwebaudio,
#endif
#if defined(PSP) || defined(VITA) || defined(ORBIS)
  &audio_psp,
#endif
#if defined(PS2)
  &audio_ps2,
#endif
#ifdef _3DS
   &audio_ctr_csnd,
   &audio_ctr_dsp,
#endif
#ifdef SWITCH
   &audio_switch,
   &audio_switch_thread,
#ifdef HAVE_LIBNX
   &audio_switch_libnx_audren,
   &audio_switch_libnx_audren_thread,
#endif
#endif
   &audio_null,
   NULL,
};

static const video_display_server_t dispserv_null = {
   NULL, /* init */
   NULL, /* destroy */
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   NULL, /* set_resolution */
   NULL, /* get_resolution_list */
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   NULL, /* get_flags */
   "null"
};

static void *video_null_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   *input       = NULL;
   *input_data = NULL;

   return (void*)-1;
}

static bool video_null_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   return true;
}

static void video_null_free(void *data) { }
static void video_null_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }
static bool video_null_alive(void *data) { return true; }
static bool video_null_focus(void *data) { return true; }
static bool video_null_has_windowed(void *data) { return true; }
static bool video_null_suppress_screensaver(void *data, bool enable) { return false; }
static bool video_null_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

static video_driver_t video_null = {
   video_null_init,
   video_null_frame,
   video_null_set_nonblock_state,
   video_null_alive,
   video_null_focus,
   video_null_suppress_screensaver,
   video_null_has_windowed,
   video_null_set_shader,
   video_null_free,
   "null",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
   NULL,
#endif
  NULL, /* get_poke_interface */
};

static const video_driver_t *video_drivers[] = {
#ifdef __PSL1GHT__
   &video_gcm,
#endif
#ifdef HAVE_VITA2D
   &video_vita2d,
#endif
#ifdef HAVE_OPENGL
   &video_gl2,
#endif
#if defined(HAVE_OPENGL_CORE)
   &video_gl_core,
#endif
#ifdef HAVE_OPENGL1
   &video_gl1,
#endif
#ifdef HAVE_VULKAN
   &video_vulkan,
#endif
#ifdef HAVE_METAL
   &video_metal,
#endif
#ifdef XENON
   &video_xenon360,
#endif
#if defined(HAVE_D3D12)
   &video_d3d12,
#endif
#if defined(HAVE_D3D11)
   &video_d3d11,
#endif
#if defined(HAVE_D3D10)
   &video_d3d10,
#endif
#if defined(HAVE_D3D9)
   &video_d3d9,
#endif
#if defined(HAVE_D3D8)
   &video_d3d8,
#endif
#ifdef PSP
   &video_psp1,
#endif
#ifdef PS2
   &video_ps2,
#endif
#ifdef _3DS
   &video_ctr,
#endif
#ifdef SWITCH
   &video_switch,
#endif
#ifdef HAVE_ODROIDGO2
   &video_oga,
#endif
#if defined(HAVE_SDL) && !defined(HAVE_SDL_DINGUX)
   &video_sdl,
#endif
#ifdef HAVE_SDL2
   &video_sdl2,
#endif
#ifdef HAVE_SDL_DINGUX
   &video_sdl_dingux,
#endif
#ifdef HAVE_XVIDEO
   &video_xvideo,
#endif
#ifdef GEKKO
   &video_gx,
#endif
#ifdef WIIU
   &video_wiiu,
#endif
#ifdef HAVE_VG
   &video_vg,
#endif
#ifdef HAVE_OMAP
   &video_omap,
#endif
#ifdef HAVE_EXYNOS
   &video_exynos,
#endif
#ifdef HAVE_DISPMANX
   &video_dispmanx,
#endif
#ifdef HAVE_SUNXI
   &video_sunxi,
#endif
#ifdef HAVE_PLAIN_DRM
   &video_drm,
#endif
#ifdef HAVE_XSHM
   &video_xshm,
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
   &video_gdi,
#endif
#endif
#ifdef DJGPP
   &video_vga,
#endif
#ifdef HAVE_FPGA
   &video_fpga,
#endif
#ifdef HAVE_SIXEL
   &video_sixel,
#endif
#ifdef HAVE_CACA
   &video_caca,
#endif
#ifdef HAVE_NETWORK_VIDEO
   &video_network,
#endif
   &video_null,
   NULL,
};

static const gfx_ctx_driver_t *gfx_ctx_drivers[] = {
#if defined(ORBIS)
   &orbis_ctx,
#endif
#if defined(VITA)
   &vita_ctx,
#endif
#if defined(HAVE_LIBNX) && defined(HAVE_OPENGL)
   &switch_ctx,
#endif
#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
   &gfx_ctx_ps3,
#endif
#if defined(HAVE_VIDEOCORE)
   &gfx_ctx_videocore,
#endif
#if defined(HAVE_MALI_FBDEV)
   &gfx_ctx_mali_fbdev,
#endif
#if defined(HAVE_VIVANTE_FBDEV)
   &gfx_ctx_vivante_fbdev,
#endif
#if defined(HAVE_OPENDINGUX_FBDEV)
   &gfx_ctx_opendingux_fbdev,
#endif
#if defined(_WIN32) && !defined(__WINRT__) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE) || defined(HAVE_VULKAN))
   &gfx_ctx_wgl,
#endif
#if defined(__WINRT__) && defined(HAVE_OPENGLES)
   &gfx_ctx_uwp,
#endif
#if defined(HAVE_WAYLAND)
   &gfx_ctx_wayland,
#endif
#if defined(HAVE_X11) && !defined(HAVE_OPENGLES)
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE) || defined(HAVE_VULKAN)
   &gfx_ctx_x,
#endif
#endif
#if defined(HAVE_X11) && defined(HAVE_OPENGL) && defined(HAVE_EGL)
   &gfx_ctx_x_egl,
#endif
#if defined(HAVE_KMS)
#if defined(HAVE_ODROIDGO2)
   &gfx_ctx_go2_drm,
#else
   &gfx_ctx_drm,
#endif
#endif
#if defined(ANDROID)
   &gfx_ctx_android,
#endif
#if defined(__QNX__)
   &gfx_ctx_qnx,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA_METAL)
   &gfx_ctx_cocoagl,
#endif
#if defined(__APPLE__) && !defined(TARGET_IPHONE_SIMULATOR) && !defined(TARGET_OS_IPHONE)
   &gfx_ctx_cgl,
#endif
#if (defined(HAVE_SDL) || defined(HAVE_SDL2)) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE))
   &gfx_ctx_sdl_gl,
#endif
#ifdef HAVE_OSMESA
   &gfx_ctx_osmesa,
#endif
#ifdef EMSCRIPTEN
   &gfx_ctx_emscripten,
#endif
#if defined(HAVE_VULKAN) && defined(HAVE_VULKAN_DISPLAY)
   &gfx_ctx_khr_display,
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
   &gfx_ctx_gdi,
#endif
#endif
#ifdef HAVE_SIXEL
   &gfx_ctx_sixel,
#endif
#ifdef HAVE_NETWORK_VIDEO
   &gfx_ctx_network,
#endif
#if defined(HAVE_FPGA)
   &gfx_ctx_fpga,
#endif
   &gfx_ctx_null,
   NULL
};

static input_driver_t input_null = {
   NULL, /* init */
   NULL, /* poll */
   NULL, /* input_state */
   NULL, /* free */
   NULL, /* set_sensor_state */
   NULL,
   NULL, /* get_capabilities */
   "null",
   NULL, /* grab_mouse */
   NULL,
   NULL, /* set_rumble */
   NULL,
   NULL,
   false,
};

static input_driver_t *input_drivers[] = {
#ifdef ORBIS
   &input_ps4,
#endif
#ifdef __CELLOS_LV2__
   &input_ps3,
#endif
#if defined(SN_TARGET_PSP2) || defined(PSP) || defined(VITA)
   &input_psp,
#endif
#if defined(PS2)
   &input_ps2,
#endif
#if defined(_3DS)
   &input_ctr,
#endif
#if defined(SWITCH)
   &input_switch,
#endif
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &input_sdl,
#endif
#ifdef HAVE_DINPUT
   &input_dinput,
#endif
#ifdef HAVE_X11
   &input_x,
#endif
#ifdef __WINRT__
   &input_uwp,
#endif
#ifdef XENON
   &input_xenon360,
#endif
#if defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1) || defined(__WINRT__)
   &input_xinput,
#endif
#ifdef GEKKO
   &input_gx,
#endif
#ifdef WIIU
   &input_wiiu,
#endif
#ifdef ANDROID
   &input_android,
#endif
#ifdef HAVE_UDEV
   &input_udev,
#endif
#if defined(__linux__) && !defined(ANDROID)
   &input_linuxraw,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA_METAL)
   &input_cocoa,
#endif
#ifdef __QNX__
   &input_qnx,
#endif
#ifdef EMSCRIPTEN
   &input_rwebinput,
#endif
#ifdef DJGPP
   &input_dos,
#endif
#if defined(_WIN32) && !defined(_XBOX) && _WIN32_WINNT >= 0x0501 && !defined(__WINRT__)
   /* winraw only available since XP */
   &input_winraw,
#endif
   &input_null,
   NULL,
};

static input_device_driver_t null_joypad = {
   NULL, /* init */
   NULL, /* query_pad */
   NULL, /* destroy */
   NULL, /* button */
   NULL, /* get_buttons */
   NULL, /* axis */
   NULL, /* poll */
   NULL,
   NULL, /* name */
   "null",
};

static input_device_driver_t *joypad_drivers[] = {
#ifdef __CELLOS_LV2__
   &ps3_joypad,
#endif
#ifdef HAVE_XINPUT
   &xinput_joypad,
#endif
#ifdef GEKKO
   &gx_joypad,
#endif
#ifdef WIIU
   &wiiu_joypad,
#endif
#ifdef _XBOX
   &xdk_joypad,
#endif
#if defined(ORBIS)
   &ps4_joypad,
#endif
#if defined(PSP) || defined(VITA)
   &psp_joypad,
#endif
#if defined(PS2)
   &ps2_joypad,
#endif
#ifdef _3DS
   &ctr_joypad,
#endif
#ifdef SWITCH
   &switch_joypad,
#endif
#ifdef HAVE_DINPUT
   &dinput_joypad,
#endif
#ifdef HAVE_UDEV
   &udev_joypad,
#endif
#if defined(__linux) && !defined(ANDROID)
   &linuxraw_joypad,
#endif
#ifdef HAVE_PARPORT
   &parport_joypad,
#endif
#ifdef ANDROID
   &android_joypad,
#endif
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &sdl_joypad,
#endif
#ifdef __QNX__
   &qnx_joypad,
#endif
#ifdef HAVE_MFI
   &mfi_joypad,
#endif
#ifdef DJGPP
   &dos_joypad,
#endif
/* Selecting the HID gamepad driver disables the Wii U gamepad. So while
 * we want the HID code to be compiled & linked, we don't want the driver
 * to be selectable in the UI. */
#if defined(HAVE_HID) && !defined(WIIU)
   &hid_joypad,
#endif
#ifdef EMSCRIPTEN
   &rwebpad_joypad,
#endif
   &null_joypad,
   NULL,
};

#ifdef HAVE_HID
static hid_driver_t null_hid = {
   NULL, /* init */
   NULL, /* joypad_query */
   NULL, /* free */
   NULL, /* button */
   NULL, /* get_buttons */
   NULL, /* axis */
   NULL, /* poll */
   NULL, /* rumble */
   NULL, /* joypad_name */
   "null",
};

static hid_driver_t *hid_drivers[] = {
#if defined(HAVE_BTSTACK)
   &btstack_hid,
#endif
#if defined(__APPLE__) && defined(HAVE_IOHIDMANAGER)
   &iohidmanager_hid,
#endif
#if defined(HAVE_LIBUSB) && defined(HAVE_THREADS)
   &libusb_hid,
#endif
#ifdef HW_RVL
   &wiiusb_hid,
#endif
   &null_hid,
   NULL,
};
#endif

static wifi_driver_t wifi_null = {
   NULL, /* init */
   NULL, /* free */
   NULL, /* start */
   NULL, /* stop */
   NULL, /* scan */
   NULL, /* get_ssids */
   NULL, /* ssid_is_online */
   NULL, /* connect_ssid */
   NULL, /* tether_start_stop */
   "null",
};

static const wifi_driver_t *wifi_drivers[] = {
#ifdef HAVE_LAKKA
   &wifi_connmanctl,
#endif
   &wifi_null,
   NULL,
};

static location_driver_t location_null = {
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   "null",
};

static const location_driver_t *location_drivers[] = {
#ifdef ANDROID
   &location_android,
#endif
   &location_null,
   NULL,
};

static ui_companion_driver_t ui_companion_null = {
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   "null",
};


static const ui_companion_driver_t *ui_companion_drivers[] = {
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   &ui_companion_win32,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
   &ui_companion_cocoa,
#endif
#ifdef HAVE_COCOATOUCH
   &ui_companion_cocoatouch,
#endif
   &ui_companion_null,
   NULL
};

static const record_driver_t record_null = {
   NULL, /* new */
   NULL, /* free */
   NULL, /* push_video */
   NULL, /* push_audio */
   NULL, /* finalize */
   "null",
};

static const record_driver_t *record_drivers[] = {
#ifdef HAVE_FFMPEG
   &record_ffmpeg,
#endif
   &record_null,
   NULL,
};

extern midi_driver_t midi_winmm;
extern midi_driver_t midi_alsa;

static void null_midi_free(void *p) { }
static void *null_midi_init(const char *input, const char *output) { return (void*)-1; }
static bool null_midi_get_avail_inputs(struct string_list *inputs) { union string_list_elem_attr attr = {0}; return string_list_append(inputs, "Null", attr); }
static bool null_midi_get_avail_outputs(struct string_list *outputs) { union string_list_elem_attr attr = {0}; return string_list_append(outputs, "Null", attr); }
static bool null_midi_set_input(void *p, const char *input) { return input == NULL || string_is_equal(input, "Null"); }
static bool null_midi_set_output(void *p, const char *output) { return output == NULL || string_is_equal(output, "Null"); }
static bool null_midi_read(void *p, midi_event_t *event) { return false; }
static bool null_midi_write(void *p, const midi_event_t *event) { return true; }
static bool null_midi_flush(void *p) { return true; }

static midi_driver_t midi_null = {
   "null",
   null_midi_get_avail_inputs,
   null_midi_get_avail_outputs,
   null_midi_init,
   null_midi_free,
   null_midi_set_input,
   null_midi_set_output,
   null_midi_read,
   null_midi_write,
   null_midi_flush
};

static midi_driver_t *midi_drivers[] = {
#if defined(HAVE_ALSA) && !defined(HAVE_HAKCHI) && !defined(HAVE_SEGAM)
   &midi_alsa,
#endif
#ifdef HAVE_WINMM
   &midi_winmm,
#endif
   &midi_null
};

static void *nullcamera_init(const char *device, uint64_t caps,
      unsigned width, unsigned height) { return (void*)-1; }
static void nullcamera_free(void *data) { }
static void nullcamera_stop(void *data) { }
static bool nullcamera_start(void *data) { return true; }
static bool nullcamera_poll(void *a,
      retro_camera_frame_raw_framebuffer_t b,
      retro_camera_frame_opengl_texture_t c) { return true; }

static camera_driver_t camera_null = {
   nullcamera_init,
   nullcamera_free,
   nullcamera_start,
   nullcamera_stop,
   nullcamera_poll,
   "null",
};

static const camera_driver_t *camera_drivers[] = {
#ifdef HAVE_V4L2
   &camera_v4l2,
#endif
#ifdef EMSCRIPTEN
   &camera_rwebcam,
#endif
#ifdef ANDROID
   &camera_android,
#endif
   &camera_null,
   NULL,
};

/* MAIN GLOBAL VARIABLES */

#define DRIVERS_CMD_ALL \
      ( DRIVER_AUDIO_MASK \
      | DRIVER_VIDEO_MASK \
      | DRIVER_INPUT_MASK \
      | DRIVER_CAMERA_MASK \
      | DRIVER_LOCATION_MASK \
      | DRIVER_MENU_MASK \
      | DRIVERS_VIDEO_INPUT_MASK \
      | DRIVER_WIFI_MASK \
      | DRIVER_LED_MASK \
      | DRIVER_MIDI_MASK )

#define DRIVERS_CMD_ALL_BUT_MENU \
      ( DRIVER_AUDIO_MASK \
      | DRIVER_VIDEO_MASK \
      | DRIVER_INPUT_MASK \
      | DRIVER_CAMERA_MASK \
      | DRIVER_LOCATION_MASK \
      | DRIVERS_VIDEO_INPUT_MASK \
      | DRIVER_WIFI_MASK \
      | DRIVER_LED_MASK \
      | DRIVER_MIDI_MASK )


#define _PSUPP(var, name, desc) printf("  %s:\n\t\t%s: %s\n", name, desc, var ? "yes" : "no")

#define FAIL_CPU(simd_type) do { \
   RARCH_ERR(simd_type " code is compiled in, but CPU does not support this feature. Cannot continue.\n"); \
   retroarch_fail(1, "validate_cpu_features()"); \
}while(0)

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

#define SHADER_FILE_WATCH_DELAY_MSEC 500
#define HOLD_START_DELAY_SEC 2

#define QUIT_DELAY_USEC 3 * 1000000 /* 3 seconds */

#define DEBUG_INFO_FILENAME "debug_info.txt"

#define BSV_MAGIC          0x42535631

#define MAGIC_INDEX        0
#define SERIALIZER_INDEX   1
#define CRC_INDEX          2
#define STATE_SIZE_INDEX   3

#define BSV_MOVIE_IS_PLAYBACK_ON() (p_rarch->bsv_movie_state_handle && p_rarch->bsv_movie_state.movie_playback)
#define BSV_MOVIE_IS_PLAYBACK_OFF() (p_rarch->bsv_movie_state_handle && !p_rarch->bsv_movie_state.movie_playback)

#define MEASURE_FRAME_TIME_SAMPLES_COUNT (2 * 1024)

#define TIME_TO_FPS(last_time, new_time, frames) ((1000000.0f * (frames)) / ((new_time) - (last_time)))

#define AUDIO_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)

#define MENU_SOUND_FORMATS "ogg|mod|xm|s3m|mp3|flac"

#define MIDI_DRIVER_BUF_SIZE 4096

/**
 * db_to_gain:
 * @db          : Decibels.
 *
 * Converts decibels to voltage gain.
 *
 * Returns: voltage gain value.
 **/
#define db_to_gain(db) (powf(10.0f, (db) / 20.0f))

#define DEFAULT_NETWORK_GAMEPAD_PORT 55400
#define UDP_FRAME_PACKETS 16

#ifdef HAVE_OVERLAY
#define OVERLAY_GET_KEY(state, key) (((state)->keys[(key) / 32] >> ((key) % 32)) & 1)
#define OVERLAY_SET_KEY(state, key) (state)->keys[(key) / 32] |= 1 << ((key) % 32)

#define MAX_VISIBILITY 32
#endif

#define DECLARE_BIND(x, bind, desc) { true, 0, #x, desc, bind }
#define DECLARE_META_BIND(level, x, bind, desc) { true, level, #x, desc, bind }

#define DEFAULT_NETWORK_CMD_PORT 55355
#define STDIN_BUF_SIZE           4096

#ifdef HAVE_THREADS
#define video_driver_is_threaded_internal() ((!video_driver_is_hw_context() && p_rarch->video_driver_threaded) ? true : false)
#else
#define video_driver_is_threaded_internal() (false)
#endif

#ifdef HAVE_THREADS
#define video_driver_lock() \
   if (p_rarch->display_lock) \
      slock_lock(p_rarch->display_lock)

#define video_driver_unlock() \
   if (p_rarch->display_lock) \
      slock_unlock(p_rarch->display_lock)

#define video_driver_context_lock() \
   if (p_rarch->context_lock) \
      slock_lock(p_rarch->context_lock)

#define video_driver_context_unlock() \
   if (p_rarch->context_lock) \
      slock_unlock(p_rarch->context_lock)

#define video_driver_lock_free() \
   slock_free(p_rarch->display_lock); \
   slock_free(p_rarch->context_lock); \
   p_rarch->display_lock = NULL; \
   p_rarch->context_lock = NULL

#define video_driver_threaded_lock(is_threaded) \
   if (is_threaded) \
      video_driver_lock()

#define video_driver_threaded_unlock(is_threaded) \
   if (is_threaded) \
      video_driver_unlock()
#else
#define video_driver_lock()            ((void)0)
#define video_driver_unlock()          ((void)0)
#define video_driver_lock_free()       ((void)0)
#define video_driver_threaded_lock(is_threaded)   ((void)0)
#define video_driver_threaded_unlock(is_threaded) ((void)0)
#define video_driver_context_lock()    ((void)0)
#define video_driver_context_unlock()  ((void)0)
#endif

#ifdef HAVE_THREADS
#define video_driver_get_ptr_internal(force) ((video_driver_is_threaded_internal() && !force) ? video_thread_get_ptr(NULL) : p_rarch->video_driver_data)
#else
#define video_driver_get_ptr_internal(force) (p_rarch->video_driver_data)
#endif

#define video_driver_get_hw_context_internal() (&p_rarch->hw_render)

#ifdef HAVE_THREADS
#define runloop_msg_queue_lock() slock_lock(p_rarch->runloop_msg_queue_lock)
#define runloop_msg_queue_unlock() slock_unlock(p_rarch->runloop_msg_queue_lock)
#else
#define runloop_msg_queue_lock()
#define runloop_msg_queue_unlock()
#endif

#define BSV_MOVIE_IS_EOF() (p_rarch->bsv_movie_state.movie_end && p_rarch->bsv_movie_state.eof_exit)

/* Time to exit out of the main loop?
 * Reasons for exiting:
 * a) Shutdown environment callback was invoked.
 * b) Quit key was pressed.
 * c) Frame count exceeds or equals maximum amount of frames to run.
 * d) Video driver no longer alive.
 * e) End of BSV movie and BSV EOF exit is true. (TODO/FIXME - explain better)
 */
#define TIME_TO_EXIT(quit_key_pressed) (p_rarch->runloop_shutdown_initiated || quit_key_pressed || !is_alive || BSV_MOVIE_IS_EOF() || ((p_rarch->runloop_max_frames != 0) && (frame_count >= p_rarch->runloop_max_frames)) || runloop_exec)

/* Depends on ASCII character values */
#define ISPRINT(c) (((int)(c) >= ' ' && (int)(c) <= '~') ? 1 : 0)

#define input_config_bind_map_get(i) ((const struct input_bind_map*)&input_config_bind_map[(i)])

#define video_has_focus() (p_rarch->current_video_context.has_focus ? p_rarch->current_video_context.has_focus(p_rarch->video_context_data) : p_rarch->current_video->focus ? (p_rarch->current_video && p_rarch->current_video->focus && p_rarch->current_video->focus(p_rarch->video_driver_data)) : true)

#if HAVE_DYNAMIC
#define runahead_run_secondary() \
   if (!secondary_core_run_use_last_input()) \
      runahead_secondary_core_available = false
#endif

#define runahead_resume_video() \
   if (runahead_video_driver_is_active) \
      p_rarch->video_driver_active = true; \
   else \
      p_rarch->video_driver_active = false

#define _PSUPP_BUF(buf, var, name, desc) \
   strlcat(buf, "  ", sizeof(buf)); \
   strlcat(buf, name, sizeof(buf)); \
   strlcat(buf, ":\n\t\t", sizeof(buf)); \
   strlcat(buf, desc, sizeof(buf)); \
   strlcat(buf, ": ", sizeof(buf)); \
   strlcat(buf, var ? "yes\n" : "no\n", sizeof(buf))

#define HOTKEY_CHECK(cmd1, cmd2, cond, cond2) \
   { \
      static bool old_pressed                   = false; \
      bool pressed                              = BIT256_GET(current_bits, cmd1); \
      if (pressed && !old_pressed) \
         if (cond) \
            command_event(cmd2, cond2); \
      old_pressed                               = pressed; \
   }

#define HOTKEY_CHECK3(cmd1, cmd2, cmd3, cmd4, cmd5, cmd6) \
   { \
      static bool old_pressed                   = false; \
      static bool old_pressed2                  = false; \
      static bool old_pressed3                  = false; \
      bool pressed                              = BIT256_GET(current_bits, cmd1); \
      bool pressed2                             = BIT256_GET(current_bits, cmd3); \
      bool pressed3                             = BIT256_GET(current_bits, cmd5); \
      if (pressed && !old_pressed) \
         command_event(cmd2, (void*)(intptr_t)0); \
      else if (pressed2 && !old_pressed2) \
         command_event(cmd4, (void*)(intptr_t)0); \
      else if (pressed3 && !old_pressed3) \
         command_event(cmd6, (void*)(intptr_t)0); \
      old_pressed                               = pressed; \
      old_pressed2                              = pressed2; \
      old_pressed3                              = pressed3; \
   }

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
#define input_remote_key_pressed(key, port) (p_rarch->remote_st_ptr.buttons[(port)] & (UINT64_C(1) << (key)))
#endif

/**
 * check_input_driver_block_hotkey:
 *
 * Checks if 'hotkey enable' key is pressed.
 *
 * If we haven't bound anything to this,
 * always allow hotkeys.

 * If we hold ENABLE_HOTKEY button, block all libretro input to allow
 * hotkeys to be bound to same keys as RetroPad.
 **/
#define check_input_driver_block_hotkey(normal_bind, autoconf_bind) \
( \
         (((normal_bind)->key      != RETROK_UNKNOWN) \
      || ((normal_bind)->mbutton   != NO_BTN) \
      || ((normal_bind)->joykey    != NO_BTN) \
      || ((normal_bind)->joyaxis   != AXIS_NONE) \
      || ((autoconf_bind)->key     != RETROK_UNKNOWN) \
      || ((autoconf_bind)->joykey  != NO_BTN) \
      || ((autoconf_bind)->joyaxis != AXIS_NONE)) \
)


#define inherit_joyaxis(binds) (((binds)[x_plus].joyaxis == (binds)[x_minus].joyaxis) || (  (binds)[y_plus].joyaxis == (binds)[y_minus].joyaxis))

/**
 * input_pop_analog_dpad:
 * @binds                          : Binds to modify.
 *
 * Restores binds temporarily overridden by input_push_analog_dpad().
 **/
#define input_pop_analog_dpad(binds) \
{ \
   unsigned j; \
   for (j = RETRO_DEVICE_ID_JOYPAD_UP; j <= RETRO_DEVICE_ID_JOYPAD_RIGHT; j++) \
      (binds)[j].joyaxis = (binds)[j].orig_joyaxis; \
}

/**
 * input_push_analog_dpad:
 * @binds                          : Binds to modify.
 * @mode                           : Which analog stick to bind D-Pad to.
 *                                   E.g:
 *                                   ANALOG_DPAD_LSTICK
 *                                   ANALOG_DPAD_RSTICK
 *
 * Push analog to D-Pad mappings to binds.
 **/
#define input_push_analog_dpad(binds, mode) \
{ \
   unsigned k; \
   unsigned x_plus      =  RARCH_ANALOG_RIGHT_X_PLUS; \
   unsigned y_plus      =  RARCH_ANALOG_RIGHT_Y_PLUS; \
   unsigned x_minus     =  RARCH_ANALOG_RIGHT_X_MINUS; \
   unsigned y_minus     =  RARCH_ANALOG_RIGHT_Y_MINUS; \
   if ((mode) == ANALOG_DPAD_LSTICK) \
   { \
      x_plus            =  RARCH_ANALOG_LEFT_X_PLUS; \
      y_plus            =  RARCH_ANALOG_LEFT_Y_PLUS; \
      x_minus           =  RARCH_ANALOG_LEFT_X_MINUS; \
      y_minus           =  RARCH_ANALOG_LEFT_Y_MINUS; \
   } \
   for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++) \
      (binds)[k].orig_joyaxis = (binds)[k].joyaxis; \
   if (!inherit_joyaxis(binds)) \
   { \
      unsigned j = x_plus + 3; \
      /* Inherit joyaxis from analogs. */ \
      for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++) \
         (binds)[k].joyaxis = (binds)[j--].joyaxis; \
   } \
}

#ifdef HAVE_DYNAMIC
#define SYMBOL(x) do { \
   function_t func = dylib_proc(lib_handle_local, #x); \
   memcpy(&current_core->x, &func, sizeof(func)); \
   if (!current_core->x) { RARCH_ERR("Failed to load symbol: \"%s\"\n", #x); retroarch_fail(1, "init_libretro_symbols()"); } \
} while (0)
#else
#define SYMBOL(x) current_core->x = x
#endif

#define SYMBOL_DUMMY(x) current_core->x = libretro_dummy_##x

#ifdef HAVE_FFMPEG
#define SYMBOL_FFMPEG(x) current_core->x = libretro_ffmpeg_##x
#endif

#ifdef HAVE_MPV
#define SYMBOL_MPV(x) current_core->x = libretro_mpv_##x
#endif

#ifdef HAVE_IMAGEVIEWER
#define SYMBOL_IMAGEVIEWER(x) current_core->x = libretro_imageviewer_##x
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
#define SYMBOL_NETRETROPAD(x) current_core->x = libretro_netretropad_##x
#endif

#if defined(HAVE_VIDEOPROCESSOR)
#define SYMBOL_VIDEOPROCESSOR(x) current_core->x = libretro_videoprocessor_##x
#endif

#ifdef HAVE_GONG
#define SYMBOL_GONG(x) current_core->x = libretro_gong_##x
#endif

#define CORE_SYMBOLS(x) \
            x(retro_init); \
            x(retro_deinit); \
            x(retro_api_version); \
            x(retro_get_system_info); \
            x(retro_get_system_av_info); \
            x(retro_set_environment); \
            x(retro_set_video_refresh); \
            x(retro_set_audio_sample); \
            x(retro_set_audio_sample_batch); \
            x(retro_set_input_poll); \
            x(retro_set_input_state); \
            x(retro_set_controller_port_device); \
            x(retro_reset); \
            x(retro_run); \
            x(retro_serialize_size); \
            x(retro_serialize); \
            x(retro_unserialize); \
            x(retro_cheat_reset); \
            x(retro_cheat_set); \
            x(retro_load_game); \
            x(retro_load_game_special); \
            x(retro_unload_game); \
            x(retro_get_region); \
            x(retro_get_memory_data); \
            x(retro_get_memory_size);

#define FFMPEG_RECORD_ARG "r:"

#ifdef HAVE_DYNAMIC
#define DYNAMIC_ARG "L:"
#else
#define DYNAMIC_ARG
#endif

#ifdef HAVE_NETWORKING
#define NETPLAY_ARG "HC:F:"
#else
#define NETPLAY_ARG
#endif

#ifdef HAVE_CONFIGFILE
#define CONFIG_FILE_ARG "c:"
#else
#define CONFIG_FILE_ARG
#endif

#define BSV_MOVIE_ARG "P:R:M:"

#ifdef HAVE_LIBNX
#define LIBNX_SWKBD_LIMIT 500 /* enforced by HOS */
#endif

/* Griffin hack */
#ifdef HAVE_QT
#ifndef HAVE_MAIN
#define HAVE_MAIN
#endif
#endif

#ifdef _WIN32
#define PERF_LOG_FMT "[PERF]: Avg (%s): %I64u ticks, %I64u runs.\n"
#else
#define PERF_LOG_FMT "[PERF]: Avg (%s): %llu ticks, %llu runs.\n"
#endif

/* Descriptive names for options without short variant.
 *
 * Please keep the name in sync with the option name.
 * Order does not matter. */
enum
{
   RA_OPT_MENU = 256, /* must be outside the range of a char */
   RA_OPT_STATELESS,
   RA_OPT_CHECK_FRAMES,
   RA_OPT_PORT,
   RA_OPT_SPECTATE,
   RA_OPT_NICK,
   RA_OPT_COMMAND,
   RA_OPT_APPENDCONFIG,
   RA_OPT_BPS,
   RA_OPT_IPS,
   RA_OPT_NO_PATCH,
   RA_OPT_RECORDCONFIG,
   RA_OPT_SUBSYSTEM,
   RA_OPT_SIZE,
   RA_OPT_FEATURES,
   RA_OPT_VERSION,
   RA_OPT_EOF_EXIT,
   RA_OPT_LOG_FILE,
   RA_OPT_MAX_FRAMES,
   RA_OPT_MAX_FRAMES_SCREENSHOT,
   RA_OPT_MAX_FRAMES_SCREENSHOT_PATH,
   RA_OPT_SET_SHADER,
   RA_OPT_ACCESSIBILITY
};

enum  runloop_state
{
   RUNLOOP_STATE_ITERATE = 0,
   RUNLOOP_STATE_POLLED_AND_SLEEP,
   RUNLOOP_STATE_MENU_ITERATE,
   RUNLOOP_STATE_END,
   RUNLOOP_STATE_QUIT
};

enum rarch_movie_type
{
   RARCH_MOVIE_PLAYBACK = 0,
   RARCH_MOVIE_RECORD
};

enum cmd_source_t
{
   CMD_NONE = 0,
   CMD_STDIN,
   CMD_NETWORK
};

enum poll_type_override_t
{
   POLL_TYPE_OVERRIDE_DONTCARE = 0,
   POLL_TYPE_OVERRIDE_EARLY,
   POLL_TYPE_OVERRIDE_NORMAL,
   POLL_TYPE_OVERRIDE_LATE
};

typedef struct runloop_ctx_msg_info
{
   const char *msg;
   unsigned prio;
   unsigned duration;
   bool flush;
} runloop_ctx_msg_info_t;

typedef struct
{
   char str[128];
   unsigned priority;
   float duration;
   bool set;
} runloop_core_status_msg_t;

struct rarch_dir_list
{
   struct string_list *list;
   size_t ptr;
};

struct bsv_state
{
   bool movie_start_recording;
   bool movie_start_playback;
   bool movie_playback;
   bool eof_exit;
   bool movie_end;

   /* Movie playback/recording support. */
   char movie_path[PATH_MAX_LENGTH];
   /* Immediate playback/recording. */
   char movie_start_path[PATH_MAX_LENGTH];
};

struct bsv_movie
{
   intfstream_t *file;

   /* A ring buffer keeping track of positions
    * in the file for each frame. */
   size_t *frame_pos;
   size_t frame_mask;
   size_t frame_ptr;

   size_t min_file_pos;

   size_t state_size;
   uint8_t *state;

   bool playback;
   bool first_rewind;
   bool did_rewind;
};

typedef struct video_pixel_scaler
{
   struct scaler_ctx *scaler;
   void *scaler_out;
} video_pixel_scaler_t;

typedef struct
{
   enum gfx_ctx_api api;
   struct string_list *list;
} gfx_api_gpu_map;

struct remote_message
{
   int port;
   int device;
   int index;
   int id;
   uint16_t state;
};

struct input_remote
{
   bool state[RARCH_BIND_LIST_END];
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   int net_fd[MAX_USERS];
#endif
};

typedef struct bsv_movie bsv_movie_t;

typedef struct input_remote input_remote_t;

typedef struct input_remote_state
{
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4][MAX_USERS];
   /* This is a bitmask of (1 << key_bind_id). */
   uint64_t buttons[MAX_USERS];
} input_remote_state_t;

typedef struct input_list_element_t
{
   unsigned port;
   unsigned device;
   unsigned index;
   int16_t *state;
   unsigned int state_size;
} input_list_element;

typedef void *(*constructor_t)(void);
typedef void  (*destructor_t )(void*);

typedef struct my_list_t
{
   void **data;
   int capacity;
   int size;
   constructor_t constructor;
   destructor_t destructor;
} my_list;

#ifdef HAVE_OVERLAY
typedef struct input_overlay_state
{
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4];
   uint32_t keys[RETROK_LAST / 32 + 1];
   /* This is a bitmask of (1 << key_bind_id). */
   input_bits_t buttons;
} input_overlay_state_t;

struct input_overlay
{
   enum overlay_status state;

   bool enable;
   bool blocked;
   bool alive;

   unsigned next_index;

   size_t index;
   size_t size;

   struct overlay *overlays;
   const struct overlay *active;
   void *iface_data;
   const video_overlay_interface_t *iface;

   input_overlay_state_t overlay_state;
};
#endif

struct cmd_map
{
   const char *str;
   unsigned id;
};

#if defined(HAVE_COMMAND)
struct cmd_action_map
{
   const char *str;
   bool (*action)(const char *arg);
   const char *arg_desc;
};
#endif

struct command
{
   bool stdin_enable;
   bool state[RARCH_BIND_LIST_END];
#ifdef HAVE_STDIN_CMD
   char stdin_buf[STDIN_BUF_SIZE];
   size_t stdin_buf_ptr;
#endif
#ifdef HAVE_NETWORK_CMD
   int net_fd;
#endif
};

/* Input config. */
struct input_bind_map
{
   bool valid;

   /* Meta binds get input as prefix, not input_playerN".
    * 0 = libretro related.
    * 1 = Common hotkey.
    * 2 = Uncommon/obscure hotkey.
    */
   uint8_t meta;

   const char *base;
   enum msg_hash_enums desc;
   uint8_t retro_key;
};

typedef struct turbo_buttons turbo_buttons_t;

/* Turbo support. */
struct turbo_buttons
{
   bool frame_enable[MAX_USERS];
   uint16_t enable[MAX_USERS];
   bool mode1_enable[MAX_USERS];
   int32_t turbo_pressed[MAX_USERS];
   unsigned count;
};

struct input_keyboard_line
{
   char *buffer;
   size_t ptr;
   size_t size;

   /** Line complete callback.
    * Calls back after return is
    * pressed with the completed line.
    * Line can be NULL.
    **/
   input_keyboard_line_complete_t cb;
   void *userdata;
};

#ifdef HAVE_RUNAHEAD
typedef bool(*runahead_load_state_function)(const void*, size_t);
#endif

struct rarch_state
{
   enum rarch_core_type current_core_type;
   enum rarch_core_type explicit_current_core_type;
   enum rotation initial_screen_orientation;
   enum rotation current_screen_orientation;
   enum retro_pixel_format video_driver_pix_fmt;
#if defined(HAVE_COMMAND)
   enum cmd_source_t lastcmd_source;
#endif
#if defined(HAVE_RUNAHEAD)
   enum rarch_core_type last_core_type;
#endif
   enum rarch_display_type video_driver_display_type;
   enum poll_type_override_t core_poll_type_override;
#ifdef HAVE_OVERLAY
   enum overlay_visibility *overlay_visibility;
#endif

   bool has_set_username;
   bool rarch_error_on_init;
   bool rarch_force_fullscreen;
   bool has_set_core;
   bool has_set_verbosity;
   bool has_set_libretro;
   bool has_set_libretro_directory;
   bool has_set_save_path;
   bool has_set_state_path;
   bool has_set_ups_pref;
   bool has_set_bps_pref;
   bool has_set_ips_pref;
#ifdef HAVE_QT
   bool qt_is_inited;
#endif
   bool has_set_log_to_file;
   bool rarch_is_inited;
   bool rarch_is_switching_display_mode;
   bool rarch_is_sram_load_disabled;
   bool rarch_is_sram_save_disabled;
   bool rarch_use_sram;
   bool rarch_ups_pref;
   bool rarch_bps_pref;
   bool rarch_ips_pref;
   bool rarch_patch_blocked;
   bool runloop_missing_bios;
   bool runloop_force_nonblock;
   bool runloop_paused;
   bool runloop_idle;
   bool runloop_slowmotion;
   bool runloop_fastmotion;
   bool runloop_shutdown_initiated;
   bool runloop_core_shutdown_initiated;
   bool runloop_core_running;
   bool runloop_perfcnt_enable;
   bool video_driver_window_title_update;

   /**
    * dynamic.c:dynamic_request_hw_context will try to set 
    * flag data when the context
    * is in the middle of being rebuilt; in these cases we will save flag
    * data and set this to true.
    * When the context is reinit, it checks this, reads from
    * deferred_flag_data and cleans it.
    *
    * TODO - Dirty hack, fix it better
    */
   bool deferred_video_context_driver_set_flags;
   bool ignore_environment_cb;
   bool core_set_shared_context;

   /* Graphics driver requires RGBA byte order data (ABGR on little-endian)
    * for 32-bit.
    * This takes effect for overlay and shader cores that wants to load
    * data into graphics driver. Kinda hackish to place it here, it is only
    * used for GLES.
    * TODO: Refactor this better. */
   bool video_driver_use_rgba;

   /* If set during context deinit, the driver should keep
    * graphics context alive to avoid having to reset all
    * context state. */
   bool video_driver_cache_context;

   /* Set to true by driver if context caching succeeded. */
   bool video_driver_cache_context_ack;

#ifdef HAVE_GFX_WIDGETS
   bool gfx_widgets_paused;
   bool gfx_widgets_fast_forward;
   bool gfx_widgets_rewinding;
#endif
#ifdef HAVE_ACCESSIBILITY
   /* Is text-to-speech accessibility turned on? */
   bool accessibility_enabled;
#endif
#ifdef HAVE_CONFIGFILE
   bool rarch_block_config_read;
   bool runloop_overrides_active;
   bool runloop_remaps_core_active;
   bool runloop_remaps_game_active;
   bool runloop_remaps_content_dir_active;
#endif
   bool runloop_game_options_active;
   bool runloop_autosave;
   bool runloop_max_frames_screenshot;
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   bool cli_shader_disable;
#endif

   bool location_driver_active;
   bool wifi_driver_active;
   bool video_driver_active;
   bool audio_driver_active;
   bool camera_driver_active;
   bool video_driver_state_out_rgb32;
   bool video_driver_crt_switching_active;
   bool video_driver_crt_dynamic_super_width;
   bool video_driver_threaded;

   bool video_started_fullscreen;

   bool audio_driver_control;
   bool audio_driver_mute_enable;
   bool audio_driver_use_float;

   bool audio_suspended;

#ifdef HAVE_RUNAHEAD
   bool has_variable_update;
   bool runahead_save_state_size_known;
   bool request_fast_savestate;
   bool hard_disable_audio;

   bool input_is_dirty;
#endif

#if defined(HAVE_NETWORKING)
   bool has_set_netplay_mode;
   bool has_set_netplay_ip_address;
   bool has_set_netplay_ip_port;
   bool has_set_netplay_stateless_mode;
   bool has_set_netplay_check_frames;
#endif

   bool input_driver_keyboard_linefeed_enable;

   bool input_driver_block_hotkey;
   bool input_driver_block_libretro_input;
   bool input_driver_nonblock_state;

#ifdef HAVE_MENU
   bool menu_input_dialog_keyboard_display;
   /* Is the menu driver still running? */
   bool menu_driver_alive;
   /* Are we binding a button inside the menu? */
   bool menu_driver_is_binding;
#endif

   bool recording_enable;
   bool streaming_enable;

   bool midi_drv_input_enabled;
   bool midi_drv_output_enabled;

   bool midi_drv_output_pending;

   bool main_ui_companion_is_on_foreground;

#ifdef HAVE_AUDIOMIXER
   bool audio_driver_mixer_mute_enable;
   bool audio_mixer_active;
#endif
   bool *load_no_content_hook;

#if defined(HAVE_COMMAND)
#ifdef HAVE_NETWORK_CMD
   int lastcmd_net_fd;
#endif
#endif

#ifdef HAVE_TRANSLATE
   int ai_service_auto;
#endif

#if defined(HAVE_RUNAHEAD)
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   int port_map[16];
#endif
#endif

#if defined(HAVE_ACCESSIBILITY) && defined(HAVE_TRANSLATE)
   int ai_gamepad_state[16];
#endif

   uint8_t *video_driver_record_gpu_buffer;
   uint8_t *midi_drv_input_buffer;
   uint8_t *midi_drv_output_buffer;

   uint16_t input_config_vid[MAX_USERS];
   uint16_t input_config_pid[MAX_USERS];

   size_t runloop_msg_queue_size;
   size_t recording_gpu_width;
   size_t recording_gpu_height;

   size_t frame_cache_pitch;

   size_t audio_driver_chunk_size;
   size_t audio_driver_chunk_nonblock_size;
   size_t audio_driver_chunk_block_size;

   size_t audio_driver_rewind_ptr;
   size_t audio_driver_rewind_size;
   size_t audio_driver_buffer_size;
   size_t audio_driver_data_ptr;

#ifdef HAVE_RUNAHEAD
   size_t runahead_save_state_size;
#endif

   unsigned runloop_pending_windowed_scale;
   unsigned runloop_max_frames;
   unsigned fastforward_after_frames;

#ifdef HAVE_MENU
   unsigned menu_input_dialog_keyboard_type;
   unsigned menu_input_dialog_keyboard_idx;
#endif

   unsigned recording_width;
   unsigned recording_height;

   unsigned video_driver_state_scale;
   unsigned video_driver_state_out_bpp;
   unsigned frame_cache_width;
   unsigned frame_cache_height;
   unsigned video_driver_width;
   unsigned video_driver_height;
   unsigned osk_last_codepoint;
   unsigned osk_last_codepoint_len;
   unsigned input_driver_flushing_input;
   unsigned input_driver_max_users;
#ifdef HAVE_ACCESSIBILITY
   unsigned gamepad_input_override;
#endif

#ifdef HAVE_MENU
   unsigned char menu_keyboard_key_state[RETROK_LAST];
#endif
   unsigned audio_driver_free_samples_buf[
      AUDIO_BUFFER_FREE_SAMPLES_COUNT];
   unsigned perf_ptr_rarch;
   unsigned perf_ptr_libretro;

   /* Opaque handles to currently running window.
    * Used by e.g. input drivers which bind to a window.
    * Drivers are responsible for setting these if an input driver
    * could potentially make use of this. */
   uintptr_t video_driver_display_userdata;
   uintptr_t video_driver_display;
   uintptr_t video_driver_window;

   float video_driver_core_hz;
   float video_driver_aspect_ratio;

#ifdef HAVE_AUDIOMIXER
   float audio_driver_mixer_volume_gain;
#endif

   float audio_driver_rate_control_delta;
   float audio_driver_input;
   float audio_driver_volume_gain;

   float input_driver_axis_threshold;

   float *audio_driver_input_data;
   float *audio_driver_output_samples_buf;

   retro_time_t frame_limit_minimum_time;
   retro_time_t frame_limit_last_time;
   retro_time_t libretro_core_runtime_last;
   retro_time_t libretro_core_runtime_usec;
   retro_time_t video_driver_frame_time_samples[
      MEASURE_FRAME_TIME_SAMPLES_COUNT];

   retro_usec_t runloop_frame_time_last;

   uint64_t audio_driver_free_samples_count;

#ifdef HAVE_RUNAHEAD
   uint64_t runahead_last_frame_count;
#endif

   uint64_t video_driver_frame_time_count;
   uint64_t video_driver_frame_count;

   double audio_source_ratio_original;
   double audio_source_ratio_current;

   char cached_video_driver[32];
   char video_driver_title_buf[64];
   char video_driver_gpu_device_string[128];
   char video_driver_gpu_api_version_string[128];
   char error_string[255];
#ifdef HAVE_MENU
   char menu_input_dialog_keyboard_label_setting[256];
   char menu_input_dialog_keyboard_label[256];
#endif
   char video_driver_window_title[512];
   char current_library_name[1024];
   char current_library_version[1024];
   char current_valid_extensions[1024];
   char launch_arguments[4096];
   char path_main_basename[8192];
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   char cli_shader[PATH_MAX_LENGTH];
   char runtime_shader_preset[PATH_MAX_LENGTH];
#endif
   char runloop_max_frames_screenshot_path[PATH_MAX_LENGTH];
   char runtime_content_path[PATH_MAX_LENGTH];
   char runtime_core_path[PATH_MAX_LENGTH];
   char subsystem_path[PATH_MAX_LENGTH];
   char path_default_shader_preset[PATH_MAX_LENGTH];
   char path_content[PATH_MAX_LENGTH];
   char path_libretro[PATH_MAX_LENGTH];
   char path_config_file[PATH_MAX_LENGTH];
   char path_config_append_file[PATH_MAX_LENGTH];
   char path_core_options_file[PATH_MAX_LENGTH];
   char dir_system[PATH_MAX_LENGTH];
   char dir_savefile[PATH_MAX_LENGTH];
   char current_savefile_dir[PATH_MAX_LENGTH];
   char current_savestate_dir[PATH_MAX_LENGTH];
   char dir_savestate[PATH_MAX_LENGTH];

   char input_device_display_names[MAX_INPUT_DEVICES][64];
   char input_device_config_names [MAX_INPUT_DEVICES][64];
   char input_device_config_paths [MAX_INPUT_DEVICES][64];

#if defined(HAVE_RUNAHEAD)
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   char *secondary_library_path;
#endif
#endif

   struct retro_perf_counter *perf_counters_rarch[MAX_COUNTERS];
   struct retro_perf_counter *perf_counters_libretro[MAX_COUNTERS];

   const struct retro_keybind *libretro_input_binds[MAX_USERS];

   input_keyboard_press_t keyboard_press_cb;

   turbo_buttons_t input_driver_turbo_btns;

#ifdef HAVE_DYNAMIC
   dylib_t lib_handle;
#endif

#if defined(HAVE_RUNAHEAD)
   retro_ctx_load_content_info_t *load_content_info;

#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   dylib_t secondary_module;
   struct retro_core_t secondary_core;
   struct retro_callbacks secondary_callbacks;
#endif
#endif

   struct rarch_dir_list dir_shader_list;

#ifdef HAVE_MENU
   /* Since these are static/global, they are initialised to zero */
   menu_input_pointer_hw_state_t menu_input_pointer_hw_state;
   menu_input_t menu_input_state;
#endif

   struct retro_camera_callback camera_cb;

   midi_event_t midi_drv_input_event;
   midi_event_t midi_drv_output_event;

   gfx_ctx_driver_t current_video_context;

   struct retro_system_av_info video_driver_av_info;

   struct bsv_state bsv_movie_state;

   struct retro_hw_render_callback hw_render;

   retro_input_state_t input_state_callback_original;

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   input_remote_state_t remote_st_ptr;
#endif

   /**
    * dynamic.c:dynamic_request_hw_context will try to set flag data when the context
    * is in the middle of being rebuilt; in these cases we will save flag
    * data and set this to true.
    * When the context is reinit, it checks this, reads from
    * deferred_flag_data and cleans it.
    *
    * TODO - Dirty hack, fix it better
    */
   gfx_ctx_flags_t deferred_flag_data;

   retro_bits_t has_set_libretro_device;

   rarch_system_info_t runloop_system;
   struct retro_frame_time_callback runloop_frame_time;

#if defined(HAVE_COMMAND)
#ifdef HAVE_NETWORK_CMD
   struct sockaddr_storage lastcmd_net_source;
   socklen_t lastcmd_net_source_len;
#endif
#endif

#ifdef HAVE_THREAD_STORAGE
   sthread_tls_t rarch_tls;
#endif

#ifdef HAVE_AUDIOMIXER
   struct audio_mixer_stream
      audio_mixer_streams[AUDIO_MIXER_MAX_SYSTEM_STREAMS];
#endif

#ifdef HAVE_MENU
   const char **menu_input_dialog_keyboard_buffer;
#endif

   struct retro_audio_callback audio_callback;
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   rarch_timer_t shader_delay_timer;
#endif

   struct retro_callbacks     retro_ctx;
   struct retro_core_t        current_core;
   struct global              g_extern;

   jmp_buf error_sjlj_context;

   struct retro_subsystem_rom_info 
      subsystem_data_roms[SUBSYSTEM_MAX_SUBSYSTEMS]
      [SUBSYSTEM_MAX_SUBSYSTEM_ROMS];

   retro_keyboard_event_t runloop_key_event;
   retro_keyboard_event_t runloop_frontend_key_event;
   core_option_manager_t *runloop_core_options;
   msg_queue_t *runloop_msg_queue;

   struct string_list *subsystem_fullpaths;

   const record_driver_t *recording_driver;
   void *recording_data;

#ifdef HAVE_THREADS
   slock_t *runloop_msg_queue_lock;
#endif

   const camera_driver_t *camera_driver;
   void *camera_data;

   void *midi_drv_data;
   struct string_list *midi_drv_inputs;
   struct string_list *midi_drv_outputs;

   const ui_companion_driver_t *ui_companion;
   void *ui_companion_data;

#ifdef HAVE_QT
   void *ui_companion_qt_data;
#endif

   const location_driver_t *location_driver;
   void *location_data;

   const wifi_driver_t *wifi_driver;
   void *wifi_data;

   void *current_display_server_data;

   bsv_movie_t     *bsv_movie_state_handle;

   rarch_softfilter_t *video_driver_state_filter;
   void               *video_driver_state_buffer;

   const void *frame_cache_data;

   void *video_driver_data;
   video_driver_t *current_video;

   /* Interface for "poking". */
   const video_poke_interface_t *video_driver_poke;

   /* Used for 15-bit -> 16-bit conversions that take place before
    * being passed to video driver. */
   video_pixel_scaler_t *video_driver_scaler_ptr;

   const struct
      retro_hw_render_context_negotiation_interface *
      hw_render_context_negotiation;

   video_driver_frame_t frame_bak;

#ifdef HAVE_THREADS
   slock_t *display_lock;
   slock_t *context_lock;
#endif

   void *video_context_data;

   int16_t *audio_driver_rewind_buf;
   int16_t *audio_driver_output_samples_conv_buf;

   retro_dsp_filter_t *audio_driver_dsp;
   struct string_list *audio_driver_devices_list;
   const retro_resampler_t *audio_driver_resampler;

   void *audio_driver_resampler_data;
   const audio_driver_t *current_audio;
   void *audio_driver_context_audio_data;

#ifdef HAVE_RUNAHEAD
   my_list *runahead_save_state_list;
   my_list *input_state_list;

   function_t retro_reset_callback_original;
   runahead_load_state_function
      retro_unserialize_callback_original;

   function_t original_retro_deinit;
   function_t original_retro_unload;
#endif

#ifdef HAVE_OVERLAY
   input_overlay_t *overlay_ptr;
#endif

   pad_connection_listener_t *pad_connection_listener;

   input_keyboard_line_t *keyboard_line;

   void *keyboard_press_data;

#ifdef HAVE_COMMAND
   command_t *input_driver_command;
#endif
#ifdef HAVE_NETWORKGAMEPAD
   input_remote_t *input_driver_remote;
#endif
   input_mapper_t *input_driver_mapper;
   input_driver_t *current_input;
   void *current_input_data;

#ifdef HAVE_HID
   const void *hid_data;
#endif
   settings_t *configuration_settings;
};

static struct rarch_state         rarch_st;

#ifdef HAVE_THREAD_STORAGE
const void *MAGIC_POINTER                                        = (void*)(uintptr_t)0x0DEFACED;
#endif

static runloop_core_status_msg_t runloop_core_status_msg         =
{
   "",
   0,
   0.0f,
   false
};

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static bool shader_presets_need_reload                           = true;
#endif
#ifdef HAVE_RUNAHEAD
static bool runahead_video_driver_is_active                      = true;
static bool runahead_available                                   = true;
static bool runahead_secondary_core_available                    = true;
static bool runahead_force_input_dirty                           = true;
#endif


#ifdef HAVE_LIBNX
/* TODO/FIXME - public global variable */
extern u32 __nx_applet_type;
#endif

static midi_driver_t *midi_drv                                   = &midi_null;
static const video_display_server_t *current_display_server      = &dispserv_null;

struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END] = {
   { "4:3",           1.3333f },
   { "16:9",          1.7778f },
   { "16:10",         1.6f },
   { "16:15",         16.0f / 15.0f },
   { "21:9",          21.0f / 9.0f },
   { "1:1",           1.0f },
   { "2:1",           2.0f },
   { "3:2",           1.5f },
   { "3:4",           0.75f },
   { "4:1",           4.0f },
   { "9:16",          0.5625f },
   { "5:4",           1.25f },
   { "6:5",           1.2f },
   { "7:9",           0.7777f },
   { "8:3",           2.6666f },
   { "8:7",           1.1428f },
   { "19:12",         1.5833f },
   { "19:14",         1.3571f },
   { "30:17",         1.7647f },
   { "32:9",          3.5555f },
   { "Config",        0.0f },
   { "Square pixel",  1.0f },
   { "Core provided", 1.0f },
   { "Custom",        0.0f }
};

static gfx_api_gpu_map gpu_map[] = {
   { GFX_CTX_VULKAN_API,     NULL },
   { GFX_CTX_DIRECT3D10_API, NULL },
   { GFX_CTX_DIRECT3D11_API, NULL },
   { GFX_CTX_DIRECT3D12_API, NULL }
};

/* TODO/FIXME - turn these into static global variable */
const struct input_bind_map input_config_bind_map[RARCH_BIND_LIST_END_NULL] = {
      DECLARE_BIND(b,         RETRO_DEVICE_ID_JOYPAD_B,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B),
      DECLARE_BIND(y,         RETRO_DEVICE_ID_JOYPAD_Y,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y),
      DECLARE_BIND(select,    RETRO_DEVICE_ID_JOYPAD_SELECT, MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT),
      DECLARE_BIND(start,     RETRO_DEVICE_ID_JOYPAD_START,  MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START),
      DECLARE_BIND(up,        RETRO_DEVICE_ID_JOYPAD_UP,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP),
      DECLARE_BIND(down,      RETRO_DEVICE_ID_JOYPAD_DOWN,   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN),
      DECLARE_BIND(left,      RETRO_DEVICE_ID_JOYPAD_LEFT,   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT),
      DECLARE_BIND(right,     RETRO_DEVICE_ID_JOYPAD_RIGHT,  MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT),
      DECLARE_BIND(a,         RETRO_DEVICE_ID_JOYPAD_A,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A),
      DECLARE_BIND(x,         RETRO_DEVICE_ID_JOYPAD_X,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X),
      DECLARE_BIND(l,         RETRO_DEVICE_ID_JOYPAD_L,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L),
      DECLARE_BIND(r,         RETRO_DEVICE_ID_JOYPAD_R,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R),
      DECLARE_BIND(l2,        RETRO_DEVICE_ID_JOYPAD_L2,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2),
      DECLARE_BIND(r2,        RETRO_DEVICE_ID_JOYPAD_R2,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2),
      DECLARE_BIND(l3,        RETRO_DEVICE_ID_JOYPAD_L3,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3),
      DECLARE_BIND(r3,        RETRO_DEVICE_ID_JOYPAD_R3,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3),
      DECLARE_BIND(l_x_plus,  RARCH_ANALOG_LEFT_X_PLUS,      MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS),
      DECLARE_BIND(l_x_minus, RARCH_ANALOG_LEFT_X_MINUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS),
      DECLARE_BIND(l_y_plus,  RARCH_ANALOG_LEFT_Y_PLUS,      MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS),
      DECLARE_BIND(l_y_minus, RARCH_ANALOG_LEFT_Y_MINUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS),
      DECLARE_BIND(r_x_plus,  RARCH_ANALOG_RIGHT_X_PLUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS),
      DECLARE_BIND(r_x_minus, RARCH_ANALOG_RIGHT_X_MINUS,    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS),
      DECLARE_BIND(r_y_plus,  RARCH_ANALOG_RIGHT_Y_PLUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS),
      DECLARE_BIND(r_y_minus, RARCH_ANALOG_RIGHT_Y_MINUS,    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS),

      DECLARE_BIND( gun_trigger,			RARCH_LIGHTGUN_TRIGGER,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER ),
      DECLARE_BIND( gun_offscreen_shot,RARCH_LIGHTGUN_RELOAD,	      MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD ),
      DECLARE_BIND( gun_aux_a,			RARCH_LIGHTGUN_AUX_A,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A ),
      DECLARE_BIND( gun_aux_b,			RARCH_LIGHTGUN_AUX_B,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B ),
      DECLARE_BIND( gun_aux_c,			RARCH_LIGHTGUN_AUX_C,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C ),
      DECLARE_BIND( gun_start,			RARCH_LIGHTGUN_START,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START ),
      DECLARE_BIND( gun_select,			RARCH_LIGHTGUN_SELECT,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT ),
      DECLARE_BIND( gun_dpad_up,			RARCH_LIGHTGUN_DPAD_UP,			MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP ),
      DECLARE_BIND( gun_dpad_down,		RARCH_LIGHTGUN_DPAD_DOWN,		MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN ),
      DECLARE_BIND( gun_dpad_left,		RARCH_LIGHTGUN_DPAD_LEFT,		MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT ),
      DECLARE_BIND( gun_dpad_right,		RARCH_LIGHTGUN_DPAD_RIGHT,		MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT ),

      DECLARE_BIND( turbo,             RARCH_TURBO_ENABLE,           MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE),

      DECLARE_META_BIND(1, toggle_fast_forward,   RARCH_FAST_FORWARD_KEY,      MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY),
      DECLARE_META_BIND(2, hold_fast_forward,     RARCH_FAST_FORWARD_HOLD_KEY, MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY),
      DECLARE_META_BIND(1, toggle_slowmotion,     RARCH_SLOWMOTION_KEY,        MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY),
      DECLARE_META_BIND(2, hold_slowmotion,       RARCH_SLOWMOTION_KEY,        MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY),
      DECLARE_META_BIND(1, load_state,            RARCH_LOAD_STATE_KEY,        MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY),
      DECLARE_META_BIND(1, save_state,            RARCH_SAVE_STATE_KEY,        MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY),
      DECLARE_META_BIND(2, toggle_fullscreen,     RARCH_FULLSCREEN_TOGGLE_KEY, MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY),
#ifdef HAVE_LAKKA
      DECLARE_META_BIND(2, exit_emulator,         RARCH_QUIT_KEY,              MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY),
#else
      DECLARE_META_BIND(2, exit_emulator,         RARCH_QUIT_KEY,              MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY),
#endif
      DECLARE_META_BIND(2, state_slot_increase,   RARCH_STATE_SLOT_PLUS,       MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS),
      DECLARE_META_BIND(2, state_slot_decrease,   RARCH_STATE_SLOT_MINUS,      MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS),
      DECLARE_META_BIND(1, rewind,                RARCH_REWIND,                MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND),
      DECLARE_META_BIND(2, movie_record_toggle,   RARCH_BSV_RECORD_TOGGLE,     MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE),
      DECLARE_META_BIND(2, pause_toggle,          RARCH_PAUSE_TOGGLE,          MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE),
      DECLARE_META_BIND(2, frame_advance,         RARCH_FRAMEADVANCE,          MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE),
      DECLARE_META_BIND(2, reset,                 RARCH_RESET,                 MENU_ENUM_LABEL_VALUE_INPUT_META_RESET),
      DECLARE_META_BIND(2, shader_next,           RARCH_SHADER_NEXT,           MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT),
      DECLARE_META_BIND(2, shader_prev,           RARCH_SHADER_PREV,           MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV),
      DECLARE_META_BIND(2, cheat_index_plus,      RARCH_CHEAT_INDEX_PLUS,      MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS),
      DECLARE_META_BIND(2, cheat_index_minus,     RARCH_CHEAT_INDEX_MINUS,     MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS),
      DECLARE_META_BIND(2, cheat_toggle,          RARCH_CHEAT_TOGGLE,          MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE),
      DECLARE_META_BIND(2, screenshot,            RARCH_SCREENSHOT,            MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT),
      DECLARE_META_BIND(2, audio_mute,            RARCH_MUTE,                  MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE),
      DECLARE_META_BIND(2, osk_toggle,            RARCH_OSK,                   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK),
      DECLARE_META_BIND(2, fps_toggle,            RARCH_FPS_TOGGLE,            MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE),
      DECLARE_META_BIND(2, send_debug_info,       RARCH_SEND_DEBUG_INFO,       MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO),
      DECLARE_META_BIND(2, netplay_host_toggle,   RARCH_NETPLAY_HOST_TOGGLE,   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE),
      DECLARE_META_BIND(2, netplay_game_watch,    RARCH_NETPLAY_GAME_WATCH,    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH),
      DECLARE_META_BIND(2, enable_hotkey,         RARCH_ENABLE_HOTKEY,         MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY),
      DECLARE_META_BIND(2, volume_up,             RARCH_VOLUME_UP,             MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP),
      DECLARE_META_BIND(2, volume_down,           RARCH_VOLUME_DOWN,           MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN),
      DECLARE_META_BIND(2, overlay_next,          RARCH_OVERLAY_NEXT,          MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT),
      DECLARE_META_BIND(2, disk_eject_toggle,     RARCH_DISK_EJECT_TOGGLE,     MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE),
      DECLARE_META_BIND(2, disk_next,             RARCH_DISK_NEXT,             MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT),
      DECLARE_META_BIND(2, disk_prev,             RARCH_DISK_PREV,             MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV),
      DECLARE_META_BIND(2, grab_mouse_toggle,     RARCH_GRAB_MOUSE_TOGGLE,     MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE),
      DECLARE_META_BIND(2, game_focus_toggle,     RARCH_GAME_FOCUS_TOGGLE,     MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE),
      DECLARE_META_BIND(2, desktop_menu_toggle,   RARCH_UI_COMPANION_TOGGLE,   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE),
#ifdef HAVE_MENU
      DECLARE_META_BIND(1, menu_toggle,           RARCH_MENU_TOGGLE,           MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE),
#endif
      DECLARE_META_BIND(2, recording_toggle,      RARCH_RECORDING_TOGGLE,      MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE),
      DECLARE_META_BIND(2, streaming_toggle,      RARCH_STREAMING_TOGGLE,      MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE),
      DECLARE_META_BIND(2, ai_service,            RARCH_AI_SERVICE,            MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE),
};
#ifdef HAVE_DISCORD
bool discord_is_inited                                          = false;
#endif
uint64_t lifecycle_state                                        = 0;
unsigned subsystem_current_count                                = 0;
char        input_device_names        [MAX_INPUT_DEVICES][64];
struct retro_keybind input_config_binds[MAX_USERS][RARCH_BIND_LIST_END];
struct retro_keybind input_autoconf_binds[MAX_USERS][RARCH_BIND_LIST_END];
struct retro_subsystem_info subsystem_data[SUBSYSTEM_MAX_SUBSYSTEMS];

/* Forward declarations */
static void retroarch_fail(int error_code, const char *error);
static void retroarch_core_options_intl_init(const struct 
      retro_core_options_intl *core_options_intl);
static void ui_companion_driver_toggle(bool force);

static const void *location_driver_find_handle(int idx);
static const void *audio_driver_find_handle(int idx);
static const void *video_driver_find_handle(int idx);
static const void *record_driver_find_handle(int idx);
static const void *wifi_driver_find_handle(int idx);
static const void *camera_driver_find_handle(int idx);
static const void *input_driver_find_handle(int idx);
static const void *joypad_driver_find_handle(int idx);
#ifdef HAVE_LIBNX
void libnx_apply_overclock(void);
#endif
#ifdef HAVE_HID
static const void *hid_driver_find_handle(int idx);
#endif
#ifdef HAVE_ACCESSIBILITY
#ifdef HAVE_TRANSLATE
static bool is_narrator_running(void);
#endif
static bool accessibility_startup_message(void);
#endif

static void retroarch_deinit_drivers(void);

static bool command_set_shader(const char *arg);

static bool midi_driver_read(uint8_t *byte);
static bool midi_driver_write(uint8_t byte, uint32_t delta_time);
static bool midi_driver_output_enabled(void);
static bool midi_driver_input_enabled(void);
static bool midi_driver_set_all_sounds_off(void);
static const void *midi_driver_find_handle(int index);
static bool midi_driver_flush(void);

static void retroarch_deinit_core_options(void);
static void retroarch_init_core_variables(const struct retro_variable *vars);
static void rarch_init_core_options(
      const struct retro_core_option_definition *option_defs);
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
static bool secondary_core_create(void);
#endif
static int16_t input_state_get_last(unsigned port,
      unsigned device, unsigned index, unsigned id);
static int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id);
static void video_driver_frame(const void *data, unsigned width,
      unsigned height, size_t pitch);
static void retro_frame_null(const void *data, unsigned width,
      unsigned height, size_t pitch);
static void retro_run_null(void);
static void retro_input_poll_null(void);

static void uninit_libretro_symbols(struct retro_core_t *current_core);
static bool init_libretro_symbols(enum rarch_core_type type,
      struct retro_core_t *current_core);

static void ui_companion_driver_deinit(void);

static bool audio_driver_stop(void);
static bool audio_driver_start(bool is_shutdown);

static bool recording_init(void);
static bool recording_deinit(void);

#ifdef HAVE_OVERLAY
static void retroarch_overlay_init(void);
static void retroarch_overlay_deinit(void);
static void input_overlay_set_alpha_mod(input_overlay_t *ol, float mod);
static void input_overlay_set_scale_factor(input_overlay_t *ol, float scale);
static void input_overlay_load_active(input_overlay_t *ol, float opacity);
static void input_overlay_auto_rotate_(input_overlay_t *ol);
#endif

#ifdef HAVE_AUDIOMIXER
static void audio_mixer_play_stop_sequential_cb(
      audio_mixer_sound_t *sound, unsigned reason);
static void audio_mixer_play_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason);
static void audio_mixer_menu_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason);
#endif

static void video_driver_gpu_record_deinit(void);
static retro_proc_address_t video_driver_get_proc_address(const char *sym);
static uintptr_t video_driver_get_current_framebuffer(void);
static bool video_driver_find_driver(void);

static void bsv_movie_deinit(void);
static bool bsv_movie_init(void);
static bool bsv_movie_check(void);

static void driver_uninit(int flags);
static void drivers_init(int flags);

#if defined(HAVE_RUNAHEAD)
static void core_free_retro_game_info(struct retro_game_info *dest);
#endif
static bool core_load(unsigned poll_type_behavior);
static bool core_unload_game(void);

static bool rarch_environment_cb(unsigned cmd, void *data);

static bool driver_location_get_position(double *lat, double *lon,
      double *horiz_accuracy, double *vert_accuracy);
static void driver_location_set_interval(unsigned interval_msecs,
      unsigned interval_distance);
static void driver_location_stop(void);
static bool driver_location_start(void);
static void driver_camera_stop(void);
static bool driver_camera_start(void);

static void log_counters(struct retro_perf_counter **counters, unsigned num)
{
   unsigned i;
   for (i = 0; i < num; i++)
   {
      if (counters[i]->call_cnt)
      {
         RARCH_LOG(PERF_LOG_FMT,
               counters[i]->ident,
               (uint64_t)counters[i]->total /
               (uint64_t)counters[i]->call_cnt,
               (uint64_t)counters[i]->call_cnt);
      }
   }
}

static void rarch_perf_log(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   RARCH_LOG("[PERF]: Performance counters (RetroArch):\n");
   log_counters(p_rarch->perf_counters_rarch, p_rarch->perf_ptr_rarch);
}

static void retro_perf_log(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   RARCH_LOG("[PERF]: Performance counters (libretro):\n");
   log_counters(p_rarch->perf_counters_libretro, p_rarch->perf_ptr_libretro);
}


struct retro_perf_counter **retro_get_perf_counter_rarch(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->perf_counters_rarch;
}

struct retro_perf_counter **retro_get_perf_counter_libretro(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->perf_counters_libretro;
}

unsigned retro_get_perf_count_rarch(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->perf_ptr_rarch;
}

unsigned retro_get_perf_count_libretro(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->perf_ptr_libretro;
}

void rarch_perf_register(struct retro_perf_counter *perf)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (
            !rarch_ctl(RARCH_CTL_IS_PERFCNT_ENABLE, NULL)
         || perf->registered
         || p_rarch->perf_ptr_rarch >= MAX_COUNTERS
      )
      return;

   p_rarch->perf_counters_rarch[p_rarch->perf_ptr_rarch++] = perf;
   perf->registered = true;
}

void performance_counter_register(struct retro_perf_counter *perf)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (perf->registered || p_rarch->perf_ptr_libretro >= MAX_COUNTERS)
      return;

   p_rarch->perf_counters_libretro[p_rarch->perf_ptr_libretro++] = perf;
   perf->registered = true;
}

void performance_counters_clear(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->perf_ptr_libretro  = 0;
   memset(p_rarch->perf_counters_libretro, 0,
         sizeof(p_rarch->perf_counters_libretro));
}

void rarch_timer_tick(rarch_timer_t *timer, retro_time_t current_time)
{
   if (!timer)
      return;
   timer->current    = current_time;
   timer->timeout_us = (timer->timeout_end - timer->current);
}

int rarch_timer_get_timeout(rarch_timer_t *timer)
{
   if (!timer)
      return 0;
   return (int)(timer->timeout_us / 1000000);
}

bool rarch_timer_is_running(rarch_timer_t *timer)
{
   if (!timer)
      return false;
   return timer->timer_begin;
}

bool rarch_timer_has_expired(rarch_timer_t *timer)
{
   if (!timer || timer->timeout_us <= 0)
      return true;
   return false;
}

void rarch_timer_end(rarch_timer_t *timer)
{
   if (!timer)
      return;
   timer->timer_end   = true;
   timer->timer_begin = false;
   timer->timeout_end = 0;
}

void rarch_timer_begin_new_time_us(rarch_timer_t *timer, uint64_t usec)
{
   if (!timer)
      return;
   timer->timeout_us  = usec;
   timer->current     = cpu_features_get_time_usec();
   timer->timeout_end = timer->current + timer->timeout_us;
}

struct string_list *dir_list_new_special(const char *input_dir,
      enum dir_list_type type, const char *filter,
      bool show_hidden_files)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   char ext_shaders[255];
#endif
   char ext_name[255];
   const char *exts                  = NULL;
   bool recursive                    = false;

   switch (type)
   {
      case DIR_LIST_AUTOCONFIG:
         exts = filter;
         break;
      case DIR_LIST_CORES:
         {
            ext_name[0]         = '\0';

            if (!frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
               return NULL;

            exts = ext_name;
         }
         break;
      case DIR_LIST_RECURSIVE:
         recursive = true;
         /* fall-through */
      case DIR_LIST_CORE_INFO:
         {
            core_info_list_t *list = NULL;
            core_info_get_list(&list);

            if (list)
               exts = list->all_ext;
         }
         break;
      case DIR_LIST_SHADERS:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            union string_list_elem_attr attr;
            struct string_list *str_list     = string_list_new();

            if (!str_list)
               return NULL;

            ext_shaders[0]                   = '\0';

            attr.i = 0;

            if (video_shader_is_supported(RARCH_SHADER_CG))
            {
               string_list_append(str_list, "cgp", attr);
               string_list_append(str_list, "cg", attr);
            }

            if (video_shader_is_supported(RARCH_SHADER_GLSL))
            {
               string_list_append(str_list, "glslp", attr);
               string_list_append(str_list, "glsl", attr);
            }

            if (video_shader_is_supported(RARCH_SHADER_SLANG))
            {
               string_list_append(str_list, "slangp", attr);
               string_list_append(str_list, "slang", attr);
            }

            string_list_join_concat(ext_shaders, sizeof(ext_shaders), str_list, "|");
            string_list_free(str_list);
            exts = ext_shaders;
         }
         break;
#else
         return NULL;
#endif
      case DIR_LIST_COLLECTIONS:
         exts = "lpl";
         break;
      case DIR_LIST_DATABASES:
         exts = "rdb";
         break;
      case DIR_LIST_PLAIN:
         exts = filter;
         break;
      case DIR_LIST_NONE:
      default:
         return NULL;
   }

   return dir_list_new(input_dir, exts, false,
         show_hidden_files,
         type == DIR_LIST_CORE_INFO, recursive);
}

struct string_list *string_list_new_special(enum string_list_type type,
      void *data, unsigned *len, size_t *list_size)
{
   union string_list_elem_attr attr;
   unsigned i;
   core_info_list_t *core_info_list = NULL;
   const core_info_t *core_info     = NULL;
   struct string_list *s            = string_list_new();

   if (!s || !len)
      goto error;

   attr.i = 0;
   *len   = 0;

   switch (type)
   {
      case STRING_LIST_MENU_DRIVERS:
#ifdef HAVE_MENU
         for (i = 0; menu_driver_find_handle(i); i++)
         {
            const char *opt  = menu_driver_find_ident(i);
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
#endif
      case STRING_LIST_CAMERA_DRIVERS:
         for (i = 0; camera_driver_find_handle(i); i++)
         {
            const char *opt  = camera_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_WIFI_DRIVERS:
#ifdef HAVE_WIFI
         for (i = 0; wifi_driver_find_handle(i); i++)
         {
            const char *opt  = wifi_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
#endif
      case STRING_LIST_LOCATION_DRIVERS:
         for (i = 0; location_driver_find_handle(i); i++)
         {
            const char *opt  = location_drivers[i]->ident;
            *len            += strlen(opt) + 1;
            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_AUDIO_DRIVERS:
         for (i = 0; audio_driver_find_handle(i); i++)
         {
            const char *opt  = audio_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_AUDIO_RESAMPLER_DRIVERS:
         for (i = 0; audio_resampler_driver_find_handle(i); i++)
         {
            const char *opt  = audio_resampler_driver_find_ident(i);
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_VIDEO_DRIVERS:
         for (i = 0; video_driver_find_handle(i); i++)
         {
            const char *opt  = video_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_INPUT_DRIVERS:
         for (i = 0; input_driver_find_handle(i); i++)
         {
            const char *opt  = input_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_INPUT_HID_DRIVERS:
#ifdef HAVE_HID
         for (i = 0; hid_driver_find_handle(i); i++)
         {
            const char *opt  = hid_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
#endif
         break;
      case STRING_LIST_INPUT_JOYPAD_DRIVERS:
         for (i = 0; joypad_driver_find_handle(i); i++)
         {
            const char *opt  = joypad_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_RECORD_DRIVERS:
         for (i = 0; record_driver_find_handle(i); i++)
         {
            const char *opt  = record_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_MIDI_DRIVERS:
         for (i = 0; midi_driver_find_handle(i); i++)
         {
            const char *opt  = midi_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_SUPPORTED_CORES_PATHS:
         core_info_get_list(&core_info_list);

         core_info_list_get_supported_cores(core_info_list,
               (const char*)data, &core_info, list_size);

         if (!core_info)
            goto error;

         if (*list_size == 0)
            goto error;

         for (i = 0; i < *list_size; i++)
         {
            const core_info_t *info = (const core_info_t*)&core_info[i];
            const char *opt = info->path;

            if (!opt)
               goto error;

            *len += strlen(opt) + 1;
            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_SUPPORTED_CORES_NAMES:
         core_info_get_list(&core_info_list);
         core_info_list_get_supported_cores(core_info_list,
               (const char*)data, &core_info, list_size);

         if (!core_info)
            goto error;

         if (*list_size == 0)
            goto error;

         for (i = 0; i < *list_size; i++)
         {
            core_info_t *info = (core_info_t*)&core_info[i];
            const char  *opt  = info->display_name;

            if (!opt)
               goto error;

            *len            += strlen(opt) + 1;
            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_NONE:
      default:
         goto error;
   }

   return s;

error:
   string_list_free(s);
   s    = NULL;
   return NULL;
}

const char *char_list_new_special(enum string_list_type type, void *data)
{
   unsigned len = 0;
   size_t list_size;
   struct string_list *s = string_list_new_special(type, data, &len, &list_size);
   char         *options = (len > 0) ? (char*)calloc(len, sizeof(char)): NULL;

   if (options && s)
      string_list_join_concat(options, len, s, "|");

   string_list_free(s);
   s = NULL;

   return options;
}

static void path_set_redirect(void)
{
   size_t path_size                            = PATH_MAX_LENGTH * sizeof(char);
   char *new_savefile_dir                      = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   char *new_savestate_dir                     = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   struct rarch_state *p_rarch                 = &rarch_st;
   global_t   *global                          = &p_rarch->g_extern;
   const char *old_savefile_dir                = p_rarch->dir_savefile;
   const char *old_savestate_dir               = p_rarch->dir_savestate;
   struct retro_system_info *system            = &p_rarch->runloop_system.info;
   settings_t *settings                        = p_rarch->configuration_settings;
   bool sort_savefiles_enable                  = settings->bools.sort_savefiles_enable;
   bool sort_savestates_enable                 = settings->bools.sort_savestates_enable;
   bool savefiles_in_content_dir               = settings->bools.savefiles_in_content_dir;
   bool savestates_in_content_dir              = settings->bools.savestates_in_content_dir;
   new_savefile_dir[0] = new_savestate_dir[0]  = '\0';

   /* Initialize current save directories
    * with the values from the config. */
   strlcpy(new_savefile_dir,  old_savefile_dir,  path_size);
   strlcpy(new_savestate_dir, old_savestate_dir, path_size);

   if (system && !string_is_empty(system->library_name))
   {
#ifdef HAVE_MENU
      if (!string_is_equal(system->library_name,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)))
#endif
      {
         /* per-core saves: append the library_name to the save location */
         if (sort_savefiles_enable
               && !string_is_empty(old_savefile_dir))
         {
            fill_pathname_join(
                  new_savefile_dir,
                  old_savefile_dir,
                  system->library_name,
                  path_size);

            /* If path doesn't exist, try to create it,
             * if everything fails revert to the original path. */
            if (!path_is_directory(new_savefile_dir))
               if (!path_mkdir(new_savefile_dir))
               {
                  RARCH_LOG("%s %s\n",
                        msg_hash_to_str(MSG_REVERTING_SAVEFILE_DIRECTORY_TO),
                        old_savefile_dir);

                  strlcpy(new_savefile_dir, old_savefile_dir, path_size);
               }
         }

         /* per-core states: append the library_name to the save location */
         if (sort_savestates_enable
               && !string_is_empty(old_savestate_dir))
         {
            fill_pathname_join(
                  new_savestate_dir,
                  old_savestate_dir,
                  system->library_name,
                  path_size);

            /* If path doesn't exist, try to create it.
             * If everything fails, revert to the original path. */
            if (!path_is_directory(new_savestate_dir))
               if (!path_mkdir(new_savestate_dir))
               {
                  RARCH_LOG("%s %s\n",
                        msg_hash_to_str(MSG_REVERTING_SAVESTATE_DIRECTORY_TO),
                        old_savestate_dir);
                  strlcpy(new_savestate_dir,
                        old_savestate_dir,
                        path_size);
               }
         }
      }
   }

   /* Set savefile directory if empty to content directory */
   if (string_is_empty(new_savefile_dir) || savefiles_in_content_dir)
   {
      strlcpy(new_savefile_dir, p_rarch->path_main_basename,
            path_size);
      path_basedir(new_savefile_dir);
   }

   /* Set savestate directory if empty based on content directory */
   if (string_is_empty(new_savestate_dir) || savestates_in_content_dir)
   {
      strlcpy(new_savestate_dir, p_rarch->path_main_basename,
            path_size);
      path_basedir(new_savestate_dir);
   }

   if (global)
   {
      if (path_is_directory(new_savefile_dir))
         strlcpy(global->name.savefile, new_savefile_dir,
               sizeof(global->name.savefile));

      if (path_is_directory(new_savestate_dir))
         strlcpy(global->name.savestate, new_savestate_dir,
               sizeof(global->name.savestate));

      if (path_is_directory(global->name.savefile))
      {
         fill_pathname_dir(global->name.savefile,
               !string_is_empty(p_rarch->path_main_basename) 
               ? p_rarch->path_main_basename 
               : system && !string_is_empty(system->library_name) 
               ? system->library_name 
               : "",
               file_path_str(FILE_PATH_SRM_EXTENSION),
               sizeof(global->name.savefile));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
               global->name.savefile);
      }

      if (path_is_directory(global->name.savestate))
      {
         fill_pathname_dir(global->name.savestate,
               !string_is_empty(p_rarch->path_main_basename) 
               ? p_rarch->path_main_basename 
               : system && !string_is_empty(system->library_name) 
               ? system->library_name 
               : "",
               file_path_str(FILE_PATH_STATE_EXTENSION),
               sizeof(global->name.savestate));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
               global->name.savestate);
      }

      if (path_is_directory(global->name.cheatfile))
      {
         /* FIXME: Should this optionally use system->library_name like the others? */
         fill_pathname_dir(global->name.cheatfile,
               !string_is_empty(p_rarch->path_main_basename) 
               ? p_rarch->path_main_basename 
               : "",
               file_path_str(FILE_PATH_CHT_EXTENSION),
               sizeof(global->name.cheatfile));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_CHEATFILE_TO),
               global->name.cheatfile);
      }
   }

   dir_set(RARCH_DIR_CURRENT_SAVEFILE,  new_savefile_dir);
   dir_set(RARCH_DIR_CURRENT_SAVESTATE, new_savestate_dir);
   free(new_savefile_dir);
   free(new_savestate_dir);
}

static void path_set_basename(const char *path)
{
   char *dst                   = NULL;
   struct rarch_state *p_rarch = &rarch_st;

   path_set(RARCH_PATH_CONTENT,  path);
   path_set(RARCH_PATH_BASENAME, path);

#ifdef HAVE_COMPRESSION
   /* Removing extension is a bit tricky for compressed files.
    * Basename means:
    * /file/to/path/game.extension should be:
    * /file/to/path/game
    *
    * Two things to consider here are: /file/to/path/ is expected
    * to be a directory and "game" is a single file. This is used for
    * states and srm default paths.
    *
    * For compressed files we have:
    *
    * /file/to/path/comp.7z#game.extension and
    * /file/to/path/comp.7z#folder/game.extension
    *
    * The choice I take here is:
    * /file/to/path/game as basename. We might end up in a writable
    * directory then and the name of srm and states are meaningful.
    *
    */
   path_basedir_wrapper(p_rarch->path_main_basename);
   fill_pathname_dir(p_rarch->path_main_basename, path, "", sizeof(p_rarch->path_main_basename));
#endif

   if ((dst = strrchr(p_rarch->path_main_basename, '.')))
      *dst = '\0';
}

struct string_list *path_get_subsystem_list(void)
{
   struct rarch_state       *p_rarch = &rarch_st;
   return p_rarch->subsystem_fullpaths;
}

void path_set_special(char **argv, unsigned num_content)
{
   unsigned i;
   char str[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   struct string_list *subsystem_paths = NULL;
   struct rarch_state         *p_rarch = &rarch_st;
   global_t   *global                  = &p_rarch->g_extern;

   /* First content file is the significant one. */
   path_set_basename(argv[0]);

   p_rarch->subsystem_fullpaths       = string_list_new();
   subsystem_paths = string_list_new();
   retro_assert(p_rarch->subsystem_fullpaths);

   attr.i = 0;

   for (i = 0; i < num_content; i++)
   {
      string_list_append(p_rarch->subsystem_fullpaths, argv[i], attr);
      strlcpy(str, argv[i], sizeof(str));
      path_remove_extension(str);
      string_list_append(subsystem_paths, path_basename(str), attr);
   }
   str[0] = '\0';
   string_list_join_concat(str, sizeof(str), subsystem_paths, " + ");

   /* We defer SRAM path updates until we can resolve it.
    * It is more complicated for special content types. */
   if (global)
   {
      struct rarch_state *p_rarch = &rarch_st;
      const char *savestate_dir   = p_rarch->current_savestate_dir;

      if (path_is_directory(savestate_dir))
         strlcpy(global->name.savestate, savestate_dir,
               sizeof(global->name.savestate));
      if (path_is_directory(global->name.savestate))
      {
         fill_pathname_dir(global->name.savestate,
               str,
               ".state",
               sizeof(global->name.savestate));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
               global->name.savestate);
      }
   }

   if (subsystem_paths)
      string_list_free(subsystem_paths);
}

static bool path_init_subsystem(void)
{
   unsigned i, j;
   const struct retro_subsystem_info *info = NULL;
   struct rarch_state             *p_rarch = &rarch_st;
   global_t   *global                      = &p_rarch->g_extern;
   rarch_system_info_t             *system = &p_rarch->runloop_system;
   bool subsystem_path_empty               = path_is_empty(RARCH_PATH_SUBSYSTEM);

   if (!system || subsystem_path_empty)
      return false;
   /* For subsystems, we know exactly which RAM types are supported. */

   info = libretro_find_subsystem_info(
         system->subsystem.data,
         system->subsystem.size,
         path_get(RARCH_PATH_SUBSYSTEM));

   /* We'll handle this error gracefully later. */
   if (info)
   {
      unsigned num_content = MIN(info->num_roms,
            subsystem_path_empty ?
            0 : (unsigned)p_rarch->subsystem_fullpaths->size);

      for (i = 0; i < num_content; i++)
      {
         for (j = 0; j < info->roms[i].num_memory; j++)
         {
            union string_list_elem_attr attr;
            char ext[32];
            char savename[PATH_MAX_LENGTH];
            size_t path_size = PATH_MAX_LENGTH * sizeof(char);
            char *path       = (char*)malloc(
                  PATH_MAX_LENGTH * sizeof(char));
            const struct retro_subsystem_memory_info *mem =
               (const struct retro_subsystem_memory_info*)
               &info->roms[i].memory[j];

            path[0] = ext[0] = '\0';

            snprintf(ext, sizeof(ext), ".%s", mem->extension);
            strlcpy(savename, p_rarch->subsystem_fullpaths->elems[i].data, sizeof(savename));
            path_remove_extension(savename);

            {
               const char    *savefile_dir = p_rarch->current_savefile_dir;

               if (path_is_directory(savefile_dir))
               {
                  /* Use SRAM dir */
                  /* Redirect content fullpath to save directory. */
                  strlcpy(path, savefile_dir, path_size);
                  fill_pathname_dir(path,
                        savename, ext,
                        path_size);
               }
               else
                  fill_pathname(path, savename, ext, path_size);
            }

            RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
               path);

            attr.i = mem->type;
            string_list_append((struct string_list*)savefile_ptr_get(), path, attr);
            free(path);
         }
      }
   }

   if (global)
   {
      /* Let other relevant paths be inferred from the main SRAM location. */
      if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL))
         fill_pathname_noext(global->name.savefile,
               p_rarch->path_main_basename,
               ".srm",
               sizeof(global->name.savefile));

      if (path_is_directory(global->name.savefile))
      {
         fill_pathname_dir(global->name.savefile,
               p_rarch->path_main_basename,
               ".srm",
               sizeof(global->name.savefile));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
               global->name.savefile);
      }
   }

   return true;
}

static void path_init_savefile(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   bool    should_sram_be_used = p_rarch->rarch_use_sram
      && !p_rarch->rarch_is_sram_save_disabled;

   p_rarch->rarch_use_sram     = should_sram_be_used;

   if (!p_rarch->rarch_use_sram)
   {
      RARCH_LOG("%s\n",
            msg_hash_to_str(MSG_SRAM_WILL_NOT_BE_SAVED));
      return;
   }

   command_event(CMD_EVENT_AUTOSAVE_INIT, NULL);
}

static void path_init_savefile_internal(void)
{
   struct rarch_state *p_rarch = &rarch_st;

   path_deinit_savefile();

   path_init_savefile_new();

   if (!path_init_subsystem())
   {
      global_t   *global = &p_rarch->g_extern;
      path_init_savefile_rtc(global->name.savefile);
   }
}

static void path_fill_names(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   global_t            *global = &p_rarch->g_extern;

   path_init_savefile_internal();

   if (global)
      strlcpy(p_rarch->bsv_movie_state.movie_path,
            global->name.savefile,
            sizeof(p_rarch->bsv_movie_state.movie_path));

   if (string_is_empty(p_rarch->path_main_basename))
      return;

   if (global)
   {
      if (string_is_empty(global->name.ups))
         fill_pathname_noext(global->name.ups,
               p_rarch->path_main_basename,
               ".ups",
               sizeof(global->name.ups));

      if (string_is_empty(global->name.bps))
         fill_pathname_noext(global->name.bps,
               p_rarch->path_main_basename,
               ".bps",
               sizeof(global->name.bps));

      if (string_is_empty(global->name.ips))
         fill_pathname_noext(global->name.ips,
               p_rarch->path_main_basename,
               ".ips",
               sizeof(global->name.ips));
   }
}

char *path_get_ptr(enum rarch_path_type type)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (type)
   {
      case RARCH_PATH_CONTENT:
         return p_rarch->path_content;
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         return p_rarch->path_default_shader_preset;
      case RARCH_PATH_BASENAME:
         return p_rarch->path_main_basename;
      case RARCH_PATH_CORE_OPTIONS:
         if (!path_is_empty(RARCH_PATH_CORE_OPTIONS))
            return p_rarch->path_core_options_file;
         break;
      case RARCH_PATH_SUBSYSTEM:
         return p_rarch->subsystem_path;
      case RARCH_PATH_CONFIG:
         if (!path_is_empty(RARCH_PATH_CONFIG))
            return p_rarch->path_config_file;
         break;
      case RARCH_PATH_CONFIG_APPEND:
         if (!path_is_empty(RARCH_PATH_CONFIG_APPEND))
            return p_rarch->path_config_append_file;
         break;
      case RARCH_PATH_CORE:
         return p_rarch->path_libretro;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }

   return NULL;
}

const char *path_get(enum rarch_path_type type)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (type)
   {
      case RARCH_PATH_CONTENT:
         return p_rarch->path_content;
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         return p_rarch->path_default_shader_preset;
      case RARCH_PATH_BASENAME:
         return p_rarch->path_main_basename;
      case RARCH_PATH_CORE_OPTIONS:
         if (!path_is_empty(RARCH_PATH_CORE_OPTIONS))
            return p_rarch->path_core_options_file;
         break;
      case RARCH_PATH_SUBSYSTEM:
         return p_rarch->subsystem_path;
      case RARCH_PATH_CONFIG:
         if (!path_is_empty(RARCH_PATH_CONFIG))
            return p_rarch->path_config_file;
         break;
      case RARCH_PATH_CONFIG_APPEND:
         if (!path_is_empty(RARCH_PATH_CONFIG_APPEND))
            return p_rarch->path_config_append_file;
         break;
      case RARCH_PATH_CORE:
         return p_rarch->path_libretro;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }

   return NULL;
}

size_t path_get_realsize(enum rarch_path_type type)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (type)
   {
      case RARCH_PATH_CONTENT:
         return sizeof(p_rarch->path_content);
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         return sizeof(p_rarch->path_default_shader_preset);
      case RARCH_PATH_BASENAME:
         return sizeof(p_rarch->path_main_basename);
      case RARCH_PATH_CORE_OPTIONS:
         return sizeof(p_rarch->path_core_options_file);
      case RARCH_PATH_SUBSYSTEM:
         return sizeof(p_rarch->subsystem_path);
      case RARCH_PATH_CONFIG:
         return sizeof(p_rarch->path_config_file);
      case RARCH_PATH_CONFIG_APPEND:
         return sizeof(p_rarch->path_config_append_file);
      case RARCH_PATH_CORE:
         return sizeof(p_rarch->path_libretro);
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }

   return 0;
}

static void path_set_names(const char *path)
{
   struct rarch_state *p_rarch = &rarch_st;
   global_t            *global = &p_rarch->g_extern;

   path_set_basename(path);

   if (global)
   {
      if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL))
         fill_pathname_noext(global->name.savefile,
               p_rarch->path_main_basename,
               ".srm", sizeof(global->name.savefile));

      if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL))
         fill_pathname_noext(global->name.savestate,
               p_rarch->path_main_basename,
               ".state", sizeof(global->name.savestate));

      fill_pathname_noext(global->name.cheatfile,
            p_rarch->path_main_basename,
            ".cht", sizeof(global->name.cheatfile));
   }

   path_set_redirect();
}

bool path_set(enum rarch_path_type type, const char *path)
{
   struct rarch_state *p_rarch = &rarch_st;

   if (!path)
      return false;

   switch (type)
   {
      case RARCH_PATH_BASENAME:
         strlcpy(p_rarch->path_main_basename, path,
               sizeof(p_rarch->path_main_basename));
         break;
      case RARCH_PATH_NAMES:
         path_set_names(path);
         break;
      case RARCH_PATH_CORE:
         strlcpy(p_rarch->path_libretro, path,
               sizeof(p_rarch->path_libretro));
         break;
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         strlcpy(p_rarch->path_default_shader_preset, path,
               sizeof(p_rarch->path_default_shader_preset));
         break;
      case RARCH_PATH_CONFIG_APPEND:
         strlcpy(p_rarch->path_config_append_file, path,
               sizeof(p_rarch->path_config_append_file));
         break;
      case RARCH_PATH_CONFIG:
         strlcpy(p_rarch->path_config_file, path,
               sizeof(p_rarch->path_config_file));
         break;
      case RARCH_PATH_SUBSYSTEM:
         strlcpy(p_rarch->subsystem_path, path,
               sizeof(p_rarch->subsystem_path));
         break;
      case RARCH_PATH_CORE_OPTIONS:
         strlcpy(p_rarch->path_core_options_file, path,
               sizeof(p_rarch->path_core_options_file));
         break;
      case RARCH_PATH_CONTENT:
         strlcpy(p_rarch->path_content, path,
               sizeof(p_rarch->path_content));
         break;
      case RARCH_PATH_NONE:
         break;
   }

   return true;
}

bool path_is_empty(enum rarch_path_type type)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (type)
   {
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         if (string_is_empty(p_rarch->path_default_shader_preset))
            return true;
         break;
      case RARCH_PATH_SUBSYSTEM:
         if (string_is_empty(p_rarch->subsystem_path))
            return true;
         break;
      case RARCH_PATH_CONFIG:
         if (string_is_empty(p_rarch->path_config_file))
            return true;
         break;
      case RARCH_PATH_CORE_OPTIONS:
         if (string_is_empty(p_rarch->path_core_options_file))
            return true;
         break;
      case RARCH_PATH_CONFIG_APPEND:
         if (string_is_empty(p_rarch->path_config_append_file))
            return true;
         break;
      case RARCH_PATH_CONTENT:
         if (string_is_empty(p_rarch->path_content))
            return true;
         break;
      case RARCH_PATH_CORE:
         if (string_is_empty(p_rarch->path_libretro))
            return true;
         break;
      case RARCH_PATH_BASENAME:
         if (string_is_empty(p_rarch->path_main_basename))
            return true;
         break;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }

   return false;
}

void path_clear(enum rarch_path_type type)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (type)
   {
      case RARCH_PATH_SUBSYSTEM:
         *p_rarch->subsystem_path = '\0';
         break;
      case RARCH_PATH_CORE:
         *p_rarch->path_libretro = '\0';
         break;
      case RARCH_PATH_CONFIG:
         *p_rarch->path_config_file = '\0';
         break;
      case RARCH_PATH_CONTENT:
         *p_rarch->path_content = '\0';
         break;
      case RARCH_PATH_BASENAME:
         *p_rarch->path_main_basename = '\0';
         break;
      case RARCH_PATH_CORE_OPTIONS:
         *p_rarch->path_core_options_file = '\0';
         break;
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         *p_rarch->path_default_shader_preset = '\0';
         break;
      case RARCH_PATH_CONFIG_APPEND:
         *p_rarch->path_config_append_file = '\0';
         break;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }
}

static void path_clear_all(void)
{
   path_clear(RARCH_PATH_CONTENT);
   path_clear(RARCH_PATH_CONFIG);
   path_clear(RARCH_PATH_CONFIG_APPEND);
   path_clear(RARCH_PATH_CORE_OPTIONS);
   path_clear(RARCH_PATH_BASENAME);
}

enum rarch_content_type path_is_media_type(const char *path)
{
   char ext_lower[128];

   ext_lower[0] = '\0';

   strlcpy(ext_lower, path_get_extension(path), sizeof(ext_lower));

   string_to_lower(ext_lower);

   /* hack, to detect livestreams so the ffmpeg core can be started */
   if (string_starts_with(path, "udp://") ||
       string_starts_with(path, "http://") ||
       string_starts_with(path, "https://") ||
       string_starts_with(path, "tcp://") ||
       string_starts_with(path, "rtmp://") ||
       string_starts_with(path, "rtp://"))
      return RARCH_CONTENT_MOVIE;

   switch (msg_hash_to_file_type(msg_hash_calculate(ext_lower)))
   {
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      case FILE_TYPE_OGM:
      case FILE_TYPE_MKV:
      case FILE_TYPE_AVI:
      case FILE_TYPE_MP4:
      case FILE_TYPE_FLV:
      case FILE_TYPE_WEBM:
      case FILE_TYPE_3GP:
      case FILE_TYPE_3G2:
      case FILE_TYPE_F4F:
      case FILE_TYPE_F4V:
      case FILE_TYPE_MOV:
      case FILE_TYPE_WMV:
      case FILE_TYPE_MPG:
      case FILE_TYPE_MPEG:
      case FILE_TYPE_VOB:
      case FILE_TYPE_ASF:
      case FILE_TYPE_DIVX:
      case FILE_TYPE_M2P:
      case FILE_TYPE_M2TS:
      case FILE_TYPE_PS:
      case FILE_TYPE_TS:
      case FILE_TYPE_MXF:
         return RARCH_CONTENT_MOVIE;
      case FILE_TYPE_WMA:
      case FILE_TYPE_OGG:
      case FILE_TYPE_MP3:
      case FILE_TYPE_M4A:
      case FILE_TYPE_FLAC:
      case FILE_TYPE_WAV:
         return RARCH_CONTENT_MUSIC;
#endif
#ifdef HAVE_IMAGEVIEWER
      case FILE_TYPE_JPEG:
      case FILE_TYPE_PNG:
      case FILE_TYPE_TGA:
      case FILE_TYPE_BMP:
         return RARCH_CONTENT_IMAGE;
#endif
#ifdef HAVE_IBXM
      case FILE_TYPE_MOD:
      case FILE_TYPE_S3M:
      case FILE_TYPE_XM:
         return RARCH_CONTENT_MUSIC;
#endif
#ifdef HAVE_GONG
      case FILE_TYPE_GONG:
         return RARCH_CONTENT_GONG;
#endif

      case FILE_TYPE_NONE:
      default:
         break;
   }

   return RARCH_CONTENT_NONE;
}

void path_deinit_subsystem(void)
{
   struct rarch_state     *p_rarch = &rarch_st;
   if (p_rarch->subsystem_fullpaths)
      string_list_free(p_rarch->subsystem_fullpaths);
   p_rarch->subsystem_fullpaths = NULL;
}

static bool dir_free_shader(void)
{
   struct rarch_state     *p_rarch = &rarch_st;
   struct rarch_dir_list *dir_list =
      (struct rarch_dir_list*)&p_rarch->dir_shader_list;

   dir_list_free(dir_list->list);
   dir_list->list = NULL;
   dir_list->ptr  = 0;

   return true;
}


#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static bool dir_init_shader(const char *path_dir_shader,
      bool show_hidden_files)
{
   unsigned i;
   struct rarch_dir_list *dir_list = NULL;
   struct rarch_state     *p_rarch = &rarch_st;
   struct string_list *new_list    = dir_list_new_special(path_dir_shader,
         DIR_LIST_SHADERS, NULL, show_hidden_files);

   if (!new_list)
      return false;
   if (new_list->size == 0)
   {
      string_list_free(new_list);
      return false;
   }

   dir_list_sort(new_list, false);

   for (i = 0; i < new_list->size; i++)
      RARCH_LOG("%s \"%s\"\n",
            msg_hash_to_str(MSG_FOUND_SHADER),
            new_list->elems[i].data);

   dir_list       = (struct rarch_dir_list*)&p_rarch->dir_shader_list;
   dir_list->list = new_list;
   dir_list->ptr  = 0;

   return true;
}
#endif

/* check functions */

/**
 * dir_check_shader:
 * @pressed_next         : was next shader key pressed?
 * @pressed_previous     : was previous shader key pressed?
 *
 * Checks if any one of the shader keys has been pressed for this frame:
 * a) Next shader index.
 * b) Previous shader index.
 *
 * Will also immediately apply the shader.
 **/
static void dir_check_shader(bool pressed_next, bool pressed_prev)
{
   struct rarch_state     *p_rarch = &rarch_st;
   struct rarch_dir_list *dir_list = (struct rarch_dir_list*)&p_rarch->dir_shader_list;
   static bool change_triggered = false;

   if (!dir_list || !dir_list->list)
      return;

   if (pressed_next)
   {
      if (change_triggered)
         dir_list->ptr = (dir_list->ptr + 1) %
            dir_list->list->size;
   }
   else if (pressed_prev)
   {
      if (dir_list->ptr == 0)
         dir_list->ptr = dir_list->list->size - 1;
      else
         dir_list->ptr--;
   }
   else
      return;
   change_triggered = true;

   command_set_shader(dir_list->list->elems[dir_list->ptr].data);
}

/* get size functions */

size_t dir_get_size(enum rarch_dir_type type)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (type)
   {
      case RARCH_DIR_SYSTEM:
         return sizeof(p_rarch->dir_system);
      case RARCH_DIR_SAVESTATE:
         return sizeof(p_rarch->dir_savestate);
      case RARCH_DIR_CURRENT_SAVESTATE:
         return sizeof(p_rarch->current_savestate_dir);
      case RARCH_DIR_SAVEFILE:
         return sizeof(p_rarch->dir_savefile);
      case RARCH_DIR_CURRENT_SAVEFILE:
         return sizeof(p_rarch->current_savefile_dir);
      case RARCH_DIR_NONE:
         break;
   }

   return 0;
}

/* clear functions */

void dir_clear(enum rarch_dir_type type)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         *p_rarch->dir_savefile = '\0';
         break;
      case RARCH_DIR_CURRENT_SAVEFILE:
         *p_rarch->current_savefile_dir = '\0';
         break;
      case RARCH_DIR_SAVESTATE:
         *p_rarch->dir_savestate = '\0';
         break;
      case RARCH_DIR_CURRENT_SAVESTATE:
         *p_rarch->current_savestate_dir = '\0';
         break;
      case RARCH_DIR_SYSTEM:
         *p_rarch->dir_system = '\0';
         break;
      case RARCH_DIR_NONE:
         break;
   }
}

static void dir_clear_all(void)
{
   dir_clear(RARCH_DIR_SYSTEM);
   dir_clear(RARCH_DIR_SAVEFILE);
   dir_clear(RARCH_DIR_SAVESTATE);
}

/* get ptr functions */

char *dir_get_ptr(enum rarch_dir_type type)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         return p_rarch->dir_savefile;
      case RARCH_DIR_CURRENT_SAVEFILE:
         return p_rarch->current_savefile_dir;
      case RARCH_DIR_SAVESTATE:
         return p_rarch->dir_savestate;
      case RARCH_DIR_CURRENT_SAVESTATE:
         return p_rarch->current_savestate_dir;
      case RARCH_DIR_SYSTEM:
         return p_rarch->dir_system;
      case RARCH_DIR_NONE:
         break;
   }

   return NULL;
}

void dir_set(enum rarch_dir_type type, const char *path)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (type)
   {
      case RARCH_DIR_CURRENT_SAVEFILE:
         strlcpy(p_rarch->current_savefile_dir, path,
               sizeof(p_rarch->current_savefile_dir));
         break;
      case RARCH_DIR_SAVEFILE:
         strlcpy(p_rarch->dir_savefile, path,
               sizeof(p_rarch->dir_savefile));
         break;
      case RARCH_DIR_CURRENT_SAVESTATE:
         strlcpy(p_rarch->current_savestate_dir, path,
               sizeof(p_rarch->current_savestate_dir));
         break;
      case RARCH_DIR_SAVESTATE:
         strlcpy(p_rarch->dir_savestate, path,
               sizeof(p_rarch->dir_savestate));
         break;
      case RARCH_DIR_SYSTEM:
         strlcpy(p_rarch->dir_system, path,
               sizeof(p_rarch->dir_system));
         break;
      case RARCH_DIR_NONE:
         break;
   }
}

void dir_check_defaults(void)
{
   unsigned i;
   /* early return for people with a custom folder setup
      so it doesn't create unnecessary directories
    */
#if defined(ORBIS) || defined(ANDROID)
   if (path_is_valid("host0:app/custom.ini"))
#elif defined(__WINRT__)
   char path[MAX_PATH];
   fill_pathname_expand_special(path, "~\\custom.ini", MAX_PATH);
   if (path_is_valid(path))
#else
   if (path_is_valid("custom.ini"))
#endif
      return;

   for (i = 0; i < DEFAULT_DIR_LAST; i++)
   {
      char       *new_path = NULL;
      const char *dir_path = g_defaults.dirs[i];

      if (string_is_empty(dir_path))
         continue;

      new_path = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));

      if (!new_path)
         continue;

      new_path[0] = '\0';
      fill_pathname_expand_special(new_path,
            dir_path,
            PATH_MAX_LENGTH * sizeof(char));

      if (!path_is_directory(new_path))
         path_mkdir(new_path);

      free(new_path);
   }
}

#ifdef HAVE_ACCESSIBILITY
bool is_accessibility_enabled(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   bool accessibility_enable   = settings->bools.accessibility_enable;
   bool accessibility_enabled  = p_rarch->accessibility_enabled;
   if (accessibility_enabled || accessibility_enable)
      return true;
   return false;
}
#endif

bool gfx_widgets_ready(void)
{
#ifdef HAVE_GFX_WIDGETS
   return gfx_widgets_active();
#else
   return false;
#endif
}

#ifdef HAVE_MENU

static void menu_input_search_cb(void *userdata, const char *str)
{
   size_t idx = 0;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   if (!selection_buf)
      return;

   if (str && *str && file_list_search(selection_buf, str, &idx))
   {
      menu_navigation_set_selection(idx);
      menu_driver_navigation_set(true);
   }

   menu_input_dialog_end();
}

const char *menu_input_dialog_get_label_buffer(void)
{
   struct rarch_state *p_rarch                           = &rarch_st;
   return p_rarch->menu_input_dialog_keyboard_label;
}

const char *menu_input_dialog_get_label_setting_buffer(void)
{
   struct rarch_state *p_rarch                           = &rarch_st;
   return p_rarch->menu_input_dialog_keyboard_label_setting;
}

void menu_input_dialog_end(void)
{
   struct rarch_state *p_rarch                           = &rarch_st;
   p_rarch->menu_input_dialog_keyboard_type              = 0;
   p_rarch->menu_input_dialog_keyboard_idx               = 0;
   p_rarch->menu_input_dialog_keyboard_display           = false;
   p_rarch->menu_input_dialog_keyboard_label[0]          = '\0';
   p_rarch->menu_input_dialog_keyboard_label_setting[0]  = '\0';

   /* Avoid triggering tates on pressing return. */
   input_driver_set_flushing_input();
}

const char *menu_input_dialog_get_buffer(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!(*p_rarch->menu_input_dialog_keyboard_buffer))
      return "";
   return *p_rarch->menu_input_dialog_keyboard_buffer;
}

unsigned menu_input_dialog_get_kb_idx(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->menu_input_dialog_keyboard_idx;
}

bool menu_input_dialog_start_search(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   menu_handle_t         *menu = menu_driver_get_ptr();

   if (!menu)
      return false;

   p_rarch->menu_input_dialog_keyboard_display = true;
   strlcpy(p_rarch->menu_input_dialog_keyboard_label,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH),
         sizeof(p_rarch->menu_input_dialog_keyboard_label));

   input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_LINE_FREE, NULL);

#ifdef HAVE_ACCESSIBILITY
   if (is_accessibility_enabled())
      accessibility_speak_priority((char*)
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH), 10);
#endif

   p_rarch->menu_input_dialog_keyboard_buffer   =
      input_keyboard_start_line(menu, menu_input_search_cb);

   return true;
}

bool menu_input_dialog_start(menu_input_ctx_line_t *line)
{
   struct rarch_state *p_rarch = &rarch_st;
   menu_handle_t    *menu      = menu_driver_get_ptr();
   if (!line || !menu)
      return false;

   p_rarch->menu_input_dialog_keyboard_display = true;

   /* Only copy over the menu label and setting if they exist. */
   if (line->label)
      strlcpy(p_rarch->menu_input_dialog_keyboard_label,
            line->label,
            sizeof(p_rarch->menu_input_dialog_keyboard_label));
   if (line->label_setting)
      strlcpy(p_rarch->menu_input_dialog_keyboard_label_setting,
            line->label_setting,
            sizeof(p_rarch->menu_input_dialog_keyboard_label_setting));

   p_rarch->menu_input_dialog_keyboard_type   = line->type;
   p_rarch->menu_input_dialog_keyboard_idx    = line->idx;

   input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_LINE_FREE, NULL);

#ifdef HAVE_ACCESSIBILITY
   accessibility_speak_priority("Keyboard input:", 10);
#endif

   p_rarch->menu_input_dialog_keyboard_buffer =
      input_keyboard_start_line(menu, line->cb);

   return true;
}

bool menu_input_dialog_get_display_kb(void)
{
   struct rarch_state *p_rarch = &rarch_st;
#ifdef HAVE_LIBNX
   SwkbdConfig kbd;
   Result rc;
   /* Indicates that we are "typing" from the swkbd
    * result to RetroArch with repeated calls to input_keyboard_event
    * This prevents input_keyboard_event from calling back
    * menu_input_dialog_get_display_kb, looping indefinintely */
   static bool typing = false;

   if (typing)
      return false;


   /* swkbd only works on "real" titles */
   if (     __nx_applet_type != AppletType_Application
         && __nx_applet_type != AppletType_SystemApplication)
      return p_rarch->menu_input_dialog_keyboard_display;

   if (!p_rarch->menu_input_dialog_keyboard_display)
      return false;

   rc = swkbdCreate(&kbd, 0);

   if (R_SUCCEEDED(rc))
   {
      unsigned i;
      char buf[LIBNX_SWKBD_LIMIT] = {'\0'};
      swkbdConfigMakePresetDefault(&kbd);

      swkbdConfigSetGuideText(&kbd,
            p_rarch->menu_input_dialog_keyboard_label);

      rc = swkbdShow(&kbd, buf, sizeof(buf));

      swkbdClose(&kbd);

      /* RetroArch uses key-by-key input
         so we need to simulate it */
      typing = true;
      for (i = 0; i < LIBNX_SWKBD_LIMIT; i++)
      {
         /* In case a previous "Enter" press closed the keyboard */
         if (!p_rarch->menu_input_dialog_keyboard_display)
            break;

         if (buf[i] == '\n' || buf[i] == '\0')
            input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
         else
         {
            /* input_keyboard_line_append expects a null-terminated
               string, so just make one (yes, the touch keyboard is
               a list of "null-terminated characters") */
            char oldchar = buf[i+1];
            buf[i+1]     = '\0';
            input_keyboard_line_append(&buf[i]);
            buf[i+1]     = oldchar;
         }
      }

      /* fail-safe */
      if (p_rarch->menu_input_dialog_keyboard_display)
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);

      typing = false;
      libnx_apply_overclock();
      return false;
   }
   libnx_apply_overclock();
#endif
   return p_rarch->menu_input_dialog_keyboard_display;
}

/* Checks if the menu is still running */
bool menu_driver_is_alive(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->menu_driver_alive;
}

void menu_driver_set_binding_state(bool on)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->menu_driver_is_binding = on;
}
#endif

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static void retroarch_set_runtime_shader_preset(const char *arg)
{
   struct rarch_state *p_rarch       = &rarch_st;
   if (!string_is_empty(arg))
      strlcpy(p_rarch->runtime_shader_preset,
            arg,
            sizeof(p_rarch->runtime_shader_preset));
   else
      p_rarch->runtime_shader_preset[0] = '\0';
}

static void retroarch_unset_runtime_shader_preset(void)
{
   struct rarch_state *p_rarch       = &rarch_st;
   p_rarch->runtime_shader_preset[0] = '\0';
}
#endif

#if defined(HAVE_RUNAHEAD)
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
static char *strcpy_alloc(const char *src)
{
   char *result = NULL;
   size_t   len = src ? strlen(src) : 0;

   if (len == 0)
      return NULL;

   result = (char*)malloc(len + 1);
   strcpy(result, src);
   return result;
}

static char *strcpy_alloc_force(const char *src)
{
   char *result = strcpy_alloc(src);
   if (!result)
      return (char*)calloc(1, 1);
   return result;
}

#endif
#endif

/* GLOBAL POINTER GETTERS */
struct retro_hw_render_callback *video_driver_get_hw_context(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return video_driver_get_hw_context_internal();
}

struct retro_system_av_info *video_viewport_get_system_av_info(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return &p_rarch->video_driver_av_info;
}

settings_t *config_get_ptr(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->configuration_settings;
}

global_t *global_get_ptr(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return &p_rarch->g_extern;
}

input_driver_t *input_get_ptr(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->current_input;
}

/**
 * video_driver_get_ptr:
 *
 * Use this if you need the real video driver
 * and driver data pointers.
 *
 * Returns: video driver's userdata.
 **/
void *video_driver_get_ptr(bool force_nonthreaded_data)
{
   struct rarch_state *p_rarch = &rarch_st;
   return video_driver_get_ptr_internal(force_nonthreaded_data);
}

/* MESSAGE QUEUE */

static void retroarch_msg_queue_deinit(void)
{
   struct rarch_state *p_rarch = &rarch_st;

   runloop_msg_queue_lock();

   if (!p_rarch->runloop_msg_queue)
      return;

   msg_queue_free(p_rarch->runloop_msg_queue);

   runloop_msg_queue_unlock();
#ifdef HAVE_THREADS
   slock_free(p_rarch->runloop_msg_queue_lock);
   p_rarch->runloop_msg_queue_lock = NULL;
#endif

   p_rarch->runloop_msg_queue      = NULL;
   p_rarch->runloop_msg_queue_size = 0;
}

static void retroarch_msg_queue_init(void)
{
   struct rarch_state       *p_rarch = &rarch_st;

   retroarch_msg_queue_deinit();
   p_rarch->runloop_msg_queue        = msg_queue_new(8);

#ifdef HAVE_THREADS
   p_rarch->runloop_msg_queue_lock   = slock_new();
#endif
}

#ifdef HAVE_THREADS
static void retroarch_autosave_deinit(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   const bool rarch_use_sram   = p_rarch->rarch_use_sram;
   if (rarch_use_sram)
      autosave_deinit();
}
#endif

/* COMMAND */

#if defined(HAVE_COMMAND)
#if (defined(HAVE_STDIN_CMD) || defined(HAVE_NETWORK_CMD))
static void command_reply(const char * data, size_t len)
{
   struct rarch_state            *p_rarch = &rarch_st;
   const enum cmd_source_t lastcmd_source = p_rarch->lastcmd_source;

   switch (lastcmd_source)
   {
      case CMD_STDIN:
#ifdef HAVE_STDIN_CMD
         fwrite(data, 1,len, stdout);
#endif
         break;
      case CMD_NETWORK:
#ifdef HAVE_NETWORK_CMD
         sendto(p_rarch->lastcmd_net_fd, data, len, 0,
               (struct sockaddr*)&p_rarch->lastcmd_net_source,
               p_rarch->lastcmd_net_source_len);
#endif
         break;
      case CMD_NONE:
      default:
         break;
   }
}

#endif
static bool command_version(const char* arg)
{
   char reply[256] = {0};

   snprintf(reply, sizeof(reply), "%s\n", PACKAGE_VERSION);
#if (defined(HAVE_STDIN_CMD) || defined(HAVE_NETWORK_CMD))
   command_reply(reply, strlen(reply));
#endif

   return true;
}

static bool command_get_status(const char* arg)
{
   char reply[4096]            = {0};
   bool contentless            = false;
   bool is_inited              = false;
   struct rarch_state *p_rarch = &rarch_st;

   content_get_status(&contentless, &is_inited);
    
   if (!is_inited)
       snprintf(reply, sizeof(reply), "GET_STATUS CONTENTLESS");
   else
   {
       /* add some content info */
       const char *status       = "PLAYING";
       const char *content_name = path_basename(path_get(RARCH_PATH_BASENAME));  /* filename only without ext */
       int content_crc32        = content_get_crc();
       const char* system_id    = NULL;
       core_info_t *core_info   = NULL;

       core_info_get_current_core(&core_info);

       if (p_rarch->runloop_paused)
          status                = "PAUSED";
       if (core_info)
          system_id             = core_info->system_id;
       if (!system_id)
          system_id             = p_rarch->runloop_system.info.library_name;
       
       snprintf(reply, sizeof(reply), "GET_STATUS %s %s,%s,crc32=%x\n", status, system_id, content_name, content_crc32);
   }

   command_reply(reply, strlen(reply));

   return true;
}


static bool command_show_osd_msg(const char* arg)
{
    runloop_msg_queue_push(arg, 1, 180, false, NULL,
          MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
    return true;    
}


static bool command_get_config_param(const char* arg)
{
   char reply[8192]             = {0};
   struct rarch_state  *p_rarch = &rarch_st;
   const char      *value       = "unsupported";
   settings_t       *settings   = p_rarch->configuration_settings;
   bool       video_fullscreen  = settings->bools.video_fullscreen;
   const char *dir_runtime_log  = settings->paths.directory_runtime_log;
   const char *log_dir          = settings->paths.log_dir;
   const char *directory_cache  = settings->paths.directory_cache;
   const char *directory_system = settings->paths.directory_system;
   const char *path_username    = settings->paths.username;

   if (string_is_equal(arg, "video_fullscreen"))
   {
      if (video_fullscreen)
         value = "true";
      else
         value = "false";
   }
   else if (string_is_equal(arg, "savefile_directory"))
      value = p_rarch->dir_savefile;
   else if (string_is_equal(arg, "savestate_directory"))
      value = p_rarch->dir_savestate;
   else if (string_is_equal(arg, "runtime_log_directory"))
      value = dir_runtime_log;
   else if (string_is_equal(arg, "log_dir"))
      value = log_dir;
   else if (string_is_equal(arg, "cache_directory"))
      value = directory_cache;
   else if (string_is_equal(arg, "system_directory"))
      value = directory_system;
   else if (string_is_equal(arg, "netplay_nickname"))
      value = path_username;
   /* TODO: query any string */

   snprintf(reply, sizeof(reply), "GET_CONFIG_PARAM %s %s\n", arg, value);
   command_reply(reply, strlen(reply));
   return true;
}

#if defined(HAVE_CHEEVOS)
static bool command_read_ram(const char *arg);
static bool command_write_ram(const char *arg);
#endif

static const struct cmd_action_map action_map[] = {
   { "SET_SHADER",       command_set_shader,       "<shader path>" },
   { "VERSION",          command_version,          "No argument"},
   { "GET_STATUS",       command_get_status,       "No argument" },
   { "GET_CONFIG_PARAM", command_get_config_param, "<param name>" },
   { "SHOW_MSG",         command_show_osd_msg,     "No argument" },
#if defined(HAVE_CHEEVOS)
   { "READ_CORE_RAM",   command_read_ram,    "<address> <number of bytes>" },
   { "WRITE_CORE_RAM",  command_write_ram,   "<address> <byte1> <byte2> ..." },
#endif
};

static const struct cmd_map map[] = {
   { "FAST_FORWARD",           RARCH_FAST_FORWARD_KEY },
   { "FAST_FORWARD_HOLD",      RARCH_FAST_FORWARD_HOLD_KEY },
   { "SLOWMOTION",             RARCH_SLOWMOTION_KEY },
   { "SLOWMOTION_HOLD",        RARCH_SLOWMOTION_HOLD_KEY },
   { "LOAD_STATE",             RARCH_LOAD_STATE_KEY },
   { "SAVE_STATE",             RARCH_SAVE_STATE_KEY },
   { "FULLSCREEN_TOGGLE",      RARCH_FULLSCREEN_TOGGLE_KEY },
   { "QUIT",                   RARCH_QUIT_KEY },
   { "STATE_SLOT_PLUS",        RARCH_STATE_SLOT_PLUS },
   { "STATE_SLOT_MINUS",       RARCH_STATE_SLOT_MINUS },
   { "REWIND",                 RARCH_REWIND },
   { "BSV_RECORD_TOGGLE",      RARCH_BSV_RECORD_TOGGLE },
   { "PAUSE_TOGGLE",           RARCH_PAUSE_TOGGLE },
   { "FRAMEADVANCE",           RARCH_FRAMEADVANCE },
   { "RESET",                  RARCH_RESET },
   { "SHADER_NEXT",            RARCH_SHADER_NEXT },
   { "SHADER_PREV",            RARCH_SHADER_PREV },
   { "CHEAT_INDEX_PLUS",       RARCH_CHEAT_INDEX_PLUS },
   { "CHEAT_INDEX_MINUS",      RARCH_CHEAT_INDEX_MINUS },
   { "CHEAT_TOGGLE",           RARCH_CHEAT_TOGGLE },
   { "SCREENSHOT",             RARCH_SCREENSHOT },
   { "MUTE",                   RARCH_MUTE },
   { "OSK",                    RARCH_OSK },
   { "FPS_TOGGLE",             RARCH_FPS_TOGGLE },
   { "SEND_DEBUG_INFO",        RARCH_SEND_DEBUG_INFO },
   { "NETPLAY_HOST_TOGGLE",    RARCH_NETPLAY_HOST_TOGGLE },
   { "NETPLAY_GAME_WATCH",     RARCH_NETPLAY_GAME_WATCH },
   { "VOLUME_UP",              RARCH_VOLUME_UP },
   { "VOLUME_DOWN",            RARCH_VOLUME_DOWN },
   { "OVERLAY_NEXT",           RARCH_OVERLAY_NEXT },
   { "DISK_EJECT_TOGGLE",      RARCH_DISK_EJECT_TOGGLE },
   { "DISK_NEXT",              RARCH_DISK_NEXT },
   { "DISK_PREV",              RARCH_DISK_PREV },
   { "GRAB_MOUSE_TOGGLE",      RARCH_GRAB_MOUSE_TOGGLE },
   { "UI_COMPANION_TOGGLE",    RARCH_UI_COMPANION_TOGGLE },
   { "GAME_FOCUS_TOGGLE",      RARCH_GAME_FOCUS_TOGGLE },
   { "MENU_TOGGLE",            RARCH_MENU_TOGGLE },
   { "RECORDING_TOGGLE",       RARCH_RECORDING_TOGGLE },
   { "STREAMING_TOGGLE",       RARCH_STREAMING_TOGGLE },
   { "MENU_UP",                RETRO_DEVICE_ID_JOYPAD_UP },
   { "MENU_DOWN",              RETRO_DEVICE_ID_JOYPAD_DOWN },
   { "MENU_LEFT",              RETRO_DEVICE_ID_JOYPAD_LEFT },
   { "MENU_RIGHT",             RETRO_DEVICE_ID_JOYPAD_RIGHT },
   { "MENU_A",                 RETRO_DEVICE_ID_JOYPAD_A },
   { "MENU_B",                 RETRO_DEVICE_ID_JOYPAD_B },
   { "AI_SERVICE",             RARCH_AI_SERVICE },
};
#endif

bool retroarch_apply_shader(enum rarch_shader_type type, const char *preset_path, bool message)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   char msg[256];
   struct rarch_state  *p_rarch = &rarch_st;
   settings_t      *settings    = p_rarch->configuration_settings;
   const char      *core_name   = p_rarch->runloop_system.info.library_name;
   bool ret                     = false;
   const char      *preset_file = NULL;

   /* disallow loading shaders when no core is loaded */
   if (string_is_empty(core_name))
      return false;

   if (!string_is_empty(preset_path))
      preset_file = path_basename(preset_path);

   if (p_rarch->current_video->set_shader)
      ret = p_rarch->current_video->set_shader(
            p_rarch->video_driver_data, type, preset_path);

   if (ret)
   {
      configuration_set_bool(settings, settings->bools.video_shader_enable, true);
      retroarch_set_runtime_shader_preset(preset_path);

#ifdef HAVE_MENU
      /* reflect in shader manager */
      if (menu_shader_manager_set_preset(menu_shader_get(), type, preset_path, false))
         if (!string_is_empty(preset_path))
            menu_shader_get()->modified = false;
#endif

      if (message)
      {
         /* Display message */
         if (preset_file)
            snprintf(msg, sizeof(msg),
                  "%s: \"%s\"",
                  msg_hash_to_str(MSG_SHADER),
                  preset_file);
         else
            snprintf(msg, sizeof(msg),
                  "%s: %s",
                  msg_hash_to_str(MSG_SHADER),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE));
#ifdef HAVE_GFX_WIDGETS
         if (gfx_widgets_active())
            gfx_widget_set_message(msg);
         else
#endif
            runloop_msg_queue_push(msg, 1, 120, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }

      RARCH_LOG("%s \"%s\".\n",
            msg_hash_to_str(MSG_APPLYING_SHADER),
            preset_path ? preset_path : "null");
   }
   else
   {
      retroarch_set_runtime_shader_preset(NULL);

#ifdef HAVE_MENU
      /* reflect in shader manager */
      menu_shader_manager_set_preset(menu_shader_get(), type, NULL, false);
#endif

      /* Display error message */
      fill_pathname_join_delim(msg,
            msg_hash_to_str(MSG_FAILED_TO_APPLY_SHADER_PRESET),
            preset_file ? preset_file : "null",
            ' ',
            sizeof(msg));

      runloop_msg_queue_push(
            msg, 1, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
   }

   return ret;
#else
   return false;
#endif
}

static bool command_set_shader(const char *arg)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   enum  rarch_shader_type type = video_shader_parse_type(arg);
   struct rarch_state  *p_rarch = &rarch_st;
   settings_t  *settings        = p_rarch->configuration_settings;

   if (!string_is_empty(arg))
   {
      if (!video_shader_is_supported(type))
         return false;

      /* rebase on shader directory */
      if (!path_is_absolute(arg))
      {
         char abs_arg[PATH_MAX_LENGTH];
         const char *ref_path = settings->paths.directory_video_shader;
         fill_pathname_join(abs_arg,
               ref_path, arg, sizeof(abs_arg));
         arg = abs_arg;
      }
   }

   if (retroarch_apply_shader(type, arg, true))
      return true;
#endif
   return false;
}

#if defined(HAVE_COMMAND)
#if defined(HAVE_CHEEVOS)
static bool command_read_ram(const char *arg)
{
   unsigned i;
   char *reply             = NULL;
   const uint8_t  *data    = NULL;
   char *reply_at          = NULL;
   unsigned int nbytes     = 0;
   unsigned int alloc_size = 0;
   unsigned int addr       = -1;
   unsigned int len        = 0;

   if (sscanf(arg, "%x %u", &addr, &nbytes) != 2)
      return true;
   /* We allocate more than needed, saving 20 bytes is not really relevant */
   alloc_size              = 40 + nbytes * 3;
   reply                   = (char*)malloc(alloc_size);
   reply[0]                = '\0';
   reply_at                = reply + snprintf(
         reply, alloc_size - 1, "READ_CORE_RAM" " %x", addr);

   if ((data = rcheevos_patch_address(addr, rcheevos_get_console())))
   {
      for (i = 0; i < nbytes; i++)
         snprintf(reply_at + 3 * i, 4, " %.2X", data[i]);
      reply_at[3 * nbytes] = '\n';
      len                  = reply_at + 3 * nbytes + 1 - reply;
   }
   else
   {
      strlcpy(reply_at, " -1\n", sizeof(reply) - strlen(reply));
      len                  = reply_at + STRLEN_CONST(" -1\n") - reply;
   }
   command_reply(reply, len);
   free(reply);
   return true;
}

static bool command_write_ram(const char *arg)
{
   unsigned int addr    = strtoul(arg, (char**)&arg, 16);
   uint8_t *data        = (uint8_t *)rcheevos_patch_address(
         addr, rcheevos_get_console());

   if (!data)
      return false;

   while (*arg)
   {
      *data = strtoul(arg, (char**)&arg, 16);
      data++;
   }
   return true;
}
#endif

#ifdef HAVE_NETWORK_CMD
static bool command_get_arg(const char *tok,
      const char **arg, unsigned *index)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(map); i++)
   {
      if (string_is_equal(tok, map[i].str))
      {
         if (arg)
            *arg = NULL;

         if (index)
            *index = i;

         return true;
      }
   }

   for (i = 0; i < ARRAY_SIZE(action_map); i++)
   {
      const char *str = strstr(tok, action_map[i].str);
      if (str == tok)
      {
         const char *argument = str + strlen(action_map[i].str);
         if (*argument != ' ' && *argument != '\0')
            return false;

         if (arg)
            *arg = argument + 1;

         if (index)
            *index = i;

         return true;
      }
   }

   return false;
}

static bool command_network_init(command_t *handle, uint16_t port)
{
   struct addrinfo *res  = NULL;
   int fd                = socket_init((void**)&res, port,
         NULL, SOCKET_TYPE_DATAGRAM);

   RARCH_LOG("%s %hu.\n",
         msg_hash_to_str(MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT),
         (unsigned short)port);

   if (fd < 0)
      goto error;

   handle->net_fd = fd;

   if (!socket_nonblock(handle->net_fd))
      goto error;

   if (!socket_bind(handle->net_fd, (void*)res))
   {
      RARCH_ERR("%s.\n",
            msg_hash_to_str(MSG_FAILED_TO_BIND_SOCKET));
      goto error;
   }

   freeaddrinfo_retro(res);
   return true;

error:
   if (res)
      freeaddrinfo_retro(res);
   return false;
}

static bool command_verify(const char *cmd)
{
   unsigned i;

   if (command_get_arg(cmd, NULL, NULL))
      return true;

   RARCH_ERR("Command \"%s\" is not recognized by the program.\n", cmd);
   RARCH_ERR("\tValid commands:\n");
   for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
      RARCH_ERR("\t\t%s\n", map[i].str);

   for (i = 0; i < sizeof(action_map) / sizeof(action_map[0]); i++)
      RARCH_ERR("\t\t%s %s\n", action_map[i].str, action_map[i].arg_desc);

   return false;
}

static bool command_network_send(const char *cmd_)
{
   bool ret            = false;
   char *command       = NULL;
   char *save          = NULL;
   const char *cmd     = NULL;
   const char *host    = NULL;
   const char *port_   = NULL;
   uint16_t port       = DEFAULT_NETWORK_CMD_PORT;

   if (!network_init())
      return false;

   if (!(command = strdup(cmd_)))
      return false;

   cmd = strtok_r(command, ";", &save);
   if (cmd)
      host = strtok_r(NULL, ";", &save);
   if (host)
      port_ = strtok_r(NULL, ";", &save);

   if (!host)
   {
#ifdef _WIN32
      host = "127.0.0.1";
#else
      host = "localhost";
#endif
   }

   if (port_)
      port = strtoul(port_, NULL, 0);

   if (cmd)
   {
      RARCH_LOG("%s: \"%s\" to %s:%hu\n",
            msg_hash_to_str(MSG_SENDING_COMMAND),
            cmd, host, (unsigned short)port);

      ret = command_verify(cmd) && udp_send_packet(host, port, cmd);
   }
   free(command);

   if (ret)
      return true;
   return false;
}
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORK_CMD) && defined(HAVE_COMMAND)
static void command_parse_sub_msg(command_t *handle, const char *tok)
{
   const char *arg = NULL;
   unsigned index  = 0;

   if (command_get_arg(tok, &arg, &index))
   {
      if (arg)
      {
         if (!action_map[index].action(arg))
            RARCH_ERR("Command \"%s\" failed.\n", arg);
      }
      else
         handle->state[map[index].id] = true;
   }
   else
      RARCH_WARN("%s \"%s\" %s.\n",
            msg_hash_to_str(MSG_UNRECOGNIZED_COMMAND),
            tok,
            msg_hash_to_str(MSG_RECEIVED));
}

static void command_parse_msg(command_t *handle,
      char *buf, enum cmd_source_t source)
{
   char                            *save  = NULL;
   const char                        *tok = strtok_r(buf, "\n", &save);
   struct rarch_state            *p_rarch = &rarch_st;

   p_rarch->lastcmd_source                = source;

   while (tok)
   {
      command_parse_sub_msg(handle, tok);
      tok = strtok_r(NULL, "\n", &save);
   }

   p_rarch->lastcmd_source = CMD_NONE;
}

static void command_network_poll(command_t *handle)
{
   fd_set fds;
   struct timeval       tmp_tv = {0};
   struct rarch_state *p_rarch = &rarch_st;

   if (handle->net_fd < 0)
      return;

   FD_ZERO(&fds);
   FD_SET(handle->net_fd, &fds);

   if (socket_select(handle->net_fd + 1, &fds, NULL, NULL, &tmp_tv) <= 0)
      return;

   if (!FD_ISSET(handle->net_fd, &fds))
      return;

   for (;;)
   {
      ssize_t ret;
      char buf[1024];

      buf[0] = '\0';

      p_rarch->lastcmd_net_fd         = handle->net_fd;
      p_rarch->lastcmd_net_source_len = sizeof(p_rarch->lastcmd_net_source);
      ret                             = recvfrom(handle->net_fd, buf,
            sizeof(buf) - 1, 0,
            (struct sockaddr*)&p_rarch->lastcmd_net_source,
            &p_rarch->lastcmd_net_source_len);

      if (ret <= 0)
         break;

      buf[ret] = '\0';

      command_parse_msg(handle, buf, CMD_NETWORK);
   }
}
#endif

static bool command_free(command_t *handle)
{
#ifdef HAVE_NETWORK_CMD
   if (handle && handle->net_fd >= 0)
      socket_close(handle->net_fd);
#endif

   free(handle);

   return true;
}

#ifdef HAVE_STDIN_CMD
static bool command_stdin_init(command_t *handle)
{
#ifndef _WIN32
#ifdef HAVE_NETWORKING
   if (!socket_nonblock(STDIN_FILENO))
      return false;
#endif
#endif

   handle->stdin_enable = true;
   return true;
}

static void command_stdin_poll(command_t *handle)
{
   ptrdiff_t msg_len;
   char *last_newline = NULL;
   ssize_t        ret = read_stdin(
         handle->stdin_buf + handle->stdin_buf_ptr,
         STDIN_BUF_SIZE - handle->stdin_buf_ptr - 1);

   if (ret == 0)
      return;

   handle->stdin_buf_ptr                    += ret;
   handle->stdin_buf[handle->stdin_buf_ptr]  = '\0';

   last_newline                              =
      strrchr(handle->stdin_buf, '\n');

   if (!last_newline)
   {
      /* We're receiving bogus data in pipe
       * (no terminating newline), flush out the buffer. */
      if (handle->stdin_buf_ptr + 1 >= STDIN_BUF_SIZE)
      {
         handle->stdin_buf_ptr = 0;
         handle->stdin_buf[0]  = '\0';
      }

      return;
   }

   *last_newline++ = '\0';
   msg_len         = last_newline - handle->stdin_buf;

#if defined(HAVE_NETWORKING)
   command_parse_msg(handle, handle->stdin_buf, CMD_STDIN);
#endif

   memmove(handle->stdin_buf, last_newline,
         handle->stdin_buf_ptr - msg_len);
   handle->stdin_buf_ptr -= msg_len;
}
#endif

static bool command_network_new(
      command_t *handle,
      bool stdin_enable,
      bool network_enable,
      uint16_t port)
{
#ifdef HAVE_NETWORK_CMD
   handle->net_fd = -1;
   if (network_enable && !command_network_init(handle, port))
      goto error;
#endif

#ifdef HAVE_STDIN_CMD
   handle->stdin_enable = stdin_enable;
   if (stdin_enable && !command_stdin_init(handle))
      goto error;
#endif

   return true;

#if defined(HAVE_NETWORK_CMD) || defined(HAVE_STDIN_CMD)
error:
   command_free(handle);
   return false;
#endif
}
#endif

/* TRANSLATION */
#ifdef HAVE_TRANSLATE
static bool task_auto_translate_callback(void)
{
   struct rarch_state       *p_rarch = &rarch_st;
   bool was_paused                   = p_rarch->runloop_paused;
   command_event(CMD_EVENT_AI_SERVICE_CALL, &was_paused);
   return true;
}

/* TODO/FIXME - Doesn't currently work.  Fix this. */
static bool is_ai_service_speech_running(void)
{
#ifdef HAVE_AUDIOMIXER
   enum audio_mixer_state res = audio_driver_mixer_get_stream_state(10);
   bool ret = (res == AUDIO_STREAM_STATE_NONE) || (res == AUDIO_STREAM_STATE_STOPPED);
   if (!ret)
      return true;
#endif
   return false;
}

static bool ai_service_speech_stop(void)
{
#ifdef HAVE_AUDIOMIXER
   audio_driver_mixer_stop_stream(10);
   audio_driver_mixer_remove_stream(10);
#endif
   return false;
}

static void task_auto_translate_handler(retro_task_t *task)
{
   int               *mode_ptr = (int*)task->user_data;
   struct rarch_state *p_rarch = &rarch_st;

   if (task_get_cancelled(task))
      goto task_finished;

   switch (*mode_ptr)
   {
      case 1: /* Speech   Mode */
#ifdef HAVE_AUDIOMIXER
         if (!is_ai_service_speech_running())
            goto task_finished;
#endif
         break;
      case 2: /* Narrator Mode */
#ifdef HAVE_ACCESSIBILITY
         if (!is_narrator_running())
            goto task_finished;
#endif
         break;
      default:
         break;
   }

   return;

task_finished:
   if (p_rarch->ai_service_auto == 1)
      p_rarch->ai_service_auto = 2;

   task_set_finished(task, true);

   if (*mode_ptr == 1 || *mode_ptr == 2)
       task_auto_translate_callback();
   if (task->user_data)
       free(task->user_data);
}

static bool call_auto_translate_task(bool* was_paused)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t        *settings = p_rarch->configuration_settings;
   int        ai_service_mode  = settings->uints.ai_service_mode;

   /*Image Mode*/
   if (ai_service_mode == 0)
   {
      if (p_rarch->ai_service_auto == 1)
         p_rarch->ai_service_auto = 2;

      command_event(CMD_EVENT_AI_SERVICE_CALL, was_paused);
      return true;
   }
   else /* Speech or Narrator Mode */
   {
      retro_task_t  *t                   = NULL;
      int* mode                          = (int*)malloc(sizeof(int));
      *mode = ai_service_mode;
      t = task_init();
      if (!t)
         return false;

      t->handler = task_auto_translate_handler;
      t->user_data = mode;
      t->mute = true;
      task_queue_push(t);
   }
   return true;
}

static void handle_translation_cb(
      retro_task_t *task, void *task_data, void *user_data, const char *error)
{
   size_t pitch;
   char curr;
   unsigned width, height;
   unsigned image_width, image_height;
   char* body_copy                   = NULL;
   uint8_t* raw_output_data          = NULL;
   char* raw_image_file_data         = NULL;
   struct scaler_ctx* scaler         = NULL;
   http_transfer_data_t *data        = (http_transfer_data_t*)task_data;
   int new_image_size                = 0;
#ifdef HAVE_AUDIOMIXER
   int new_sound_size                = 0;
#endif
   const void* dummy_data            = NULL;
   void* raw_image_data              = NULL;
   void* raw_image_data_alpha        = NULL;
   void* raw_sound_data              = NULL;
   int retval                        = 0;
   int i                             = 0;
   int start                         = -1;
   char* found_string                = NULL;
   char* err_string                  = NULL;
   char* text_string                 = NULL;
   char* auto_string                 = NULL;
   char* key_string                  = NULL;
   int curr_state                    = 0;
   struct rarch_state *p_rarch       = &rarch_st;
   settings_t* settings              = p_rarch->configuration_settings;
   bool was_paused                   = p_rarch->runloop_paused;
   const enum retro_pixel_format 
      video_driver_pix_fmt           = p_rarch->video_driver_pix_fmt;
   bool gfx_widgets_paused           = p_rarch->gfx_widgets_paused;

#ifdef GFX_MENU_WIDGETS
   if (gfx_widgets_ai_service_overlay_get_state() != 0 
       && p_rarch->ai_service_auto == 2)
   {
      /* When auto mode is on, we turn off the overlay
       * once we have the result for the next call.*/
      gfx_widgets_ai_service_overlay_unload();
   }
#endif

#ifdef DEBUG
   if (p_rarch->ai_service_auto != 2)
      RARCH_LOG("RESULT FROM AI SERVICE...\n");
#endif

   if (!data || error)
      goto finish;
   
   data->data = (char*)realloc(data->data, data->len + 1);
   if (!data->data)
      goto finish;

   data->data[data->len] = '\0';

   /* Parse JSON body for the image and sound data */
   body_copy = strdup(data->data);

   for (;;)
   {
      curr = (char)*(body_copy+i);
      if (curr == '\0')
          break;
      if (curr == '\"')
      {
         if (start == -1)
            start = i;
         else
         {
            size_t found_string_len;
            found_string = (char*)malloc(i-start+1);
            strlcpy(found_string, body_copy+start+1, i-start);

            found_string_len = strlen(found_string);

            if (curr_state == 1)/*image*/
            {
               raw_image_file_data = (char*)unbase64(found_string,
                    found_string_len,
                    &new_image_size);
               curr_state = 0;
            }
#ifdef HAVE_AUDIOMIXER
            else if (curr_state == 2)
            {
               raw_sound_data = (void*)unbase64(found_string,
                    found_string_len, &new_sound_size);
               curr_state = 0;
            }
#endif
            else if (curr_state == 3)
            {
               text_string = (char*)malloc(i-start+1);
               strlcpy(text_string, body_copy+start+1, i-start);
               curr_state = 0;
            }
            else if (curr_state == 4)
            {
               err_string = (char*)malloc(i-start+1);
               strlcpy(err_string, body_copy+start+1, i-start);
               curr_state = 0;
            }
            else if (curr_state == 5)
            {
               auto_string = (char*)malloc(i-start+1);
               strlcpy(auto_string, body_copy+start+1, i-start);
               curr_state = 0;
            }
            else if (curr_state == 6)
            {
               key_string = (char*)malloc(i-start+1);
               strlcpy(key_string, body_copy+start+1, i-start);
               curr_state = 0;
            }
            else if (string_is_equal(found_string, "image"))
            {
               curr_state = 1;
               free(found_string);
            }
            else if (string_is_equal(found_string, "sound"))
            {
               curr_state = 2;
               free(found_string);
            }
            else if (string_is_equal(found_string, "text"))
            {
               curr_state = 3;
               free(found_string);
            }
            else if (string_is_equal(found_string, "error"))
            {
               curr_state = 4;
               free(found_string);
            }
            else if (string_is_equal(found_string, "auto"))
            {
               curr_state = 5;
               free(found_string);
            }
            else if (string_is_equal(found_string, "press"))
            {
               curr_state = 6;
               free(found_string);
            }
            else
            {
              curr_state = 0;
              free(found_string);
            }
            start = -1;
         }
      }
      i++;
   }

   if (string_is_equal(err_string, "No text found."))
   {
#ifdef DEBUG
      RARCH_LOG("No text found...\n");
#endif
      if (!text_string)
         text_string = (char*)malloc(15);

      strlcpy(text_string, err_string, 15);
#ifdef HAVE_GFX_WIDGETS
      if (gfx_widgets_paused)
      {
         /* In this case we have to unpause and then repause for a frame */
         gfx_widgets_ai_service_overlay_set_state(2);
         command_event(CMD_EVENT_UNPAUSE, NULL);
      }
#endif
   }

   if (     !raw_image_file_data 
         && !raw_sound_data 
         && !text_string 
         && (p_rarch->ai_service_auto != 2)
         && !key_string)
   {
      error = "Invalid JSON body.";
      goto finish;
   }

   if (raw_image_file_data)
   {
      /* Get the video frame dimensions reference */
      video_driver_cached_frame_get(&dummy_data, &width, &height, &pitch);

      /* try two different modes for text display *
       * In the first mode, we use display widget overlays, but they require
       * the video poke interface to be able to load image buffers.
       *
       * The other method is to draw to the video buffer directly, which needs
       * a software core to be running. */
#ifdef HAVE_GFX_WIDGETS
      if (p_rarch->video_driver_poke
          && p_rarch->video_driver_poke->load_texture 
          && p_rarch->video_driver_poke->unload_texture)
      {
         bool ai_res;
         enum image_type_enum image_type;
         /* Write to overlay */
         if (  raw_image_file_data[0] == 'B' && 
               raw_image_file_data[1] == 'M')
             image_type = IMAGE_TYPE_BMP;
         else if (raw_image_file_data[1] == 'P' &&
                  raw_image_file_data[2] == 'N' &&
                  raw_image_file_data[3] == 'G')
            image_type = IMAGE_TYPE_PNG;
         else
         {
            RARCH_LOG("Invalid image type returned from server.\n");
            goto finish;
         }

         ai_res = gfx_widgets_ai_service_overlay_load(
                     raw_image_file_data, (unsigned) new_image_size,
                     image_type);

         if (!ai_res)
         {
            RARCH_LOG("Video driver not supported for AI Service.");
            runloop_msg_queue_push(
               /* msg_hash_to_str(MSG_VIDEO_DRIVER_NOT_SUPPORTED), */
               "Video driver not supported.",
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         else if (gfx_widgets_paused)
         {
            /* In this case we have to unpause and then repause for a frame */
#ifdef HAVE_TRANSLATE
            gfx_widgets_ai_service_overlay_set_state(2);/* Unpausing state */
#endif
            command_event(CMD_EVENT_UNPAUSE, NULL);
         }
      }
      else
#endif
      /* Can't use display widget overlays, so try writing to video buffer */
      {
         /* Write to video buffer directly (software cores only) */
         if (raw_image_file_data[0] == 'B' && raw_image_file_data[1] == 'M')
         {
            /* This is a BMP file coming back. */
            /* Get image data (24 bit), and convert to the emulated pixel format */
            image_width    =
               ((uint32_t) ((uint8_t)raw_image_file_data[21]) << 24) +
               ((uint32_t) ((uint8_t)raw_image_file_data[20]) << 16) +
               ((uint32_t) ((uint8_t)raw_image_file_data[19]) << 8) +
               ((uint32_t) ((uint8_t)raw_image_file_data[18]) << 0);

            image_height   =
               ((uint32_t) ((uint8_t)raw_image_file_data[25]) << 24) +
               ((uint32_t) ((uint8_t)raw_image_file_data[24]) << 16) +
               ((uint32_t) ((uint8_t)raw_image_file_data[23]) << 8) +
               ((uint32_t) ((uint8_t)raw_image_file_data[22]) << 0);
            raw_image_data = (void*)malloc(image_width*image_height*3*sizeof(uint8_t));
            memcpy(raw_image_data,
                   raw_image_file_data+54*sizeof(uint8_t),
                   image_width*image_height*3*sizeof(uint8_t));
         }
         else if (raw_image_file_data[1] == 'P' && raw_image_file_data[2] == 'N' &&
                  raw_image_file_data[3] == 'G')
         {
            rpng_t *rpng = NULL;
            /* PNG coming back from the url */
            image_width  =
                ((uint32_t) ((uint8_t)raw_image_file_data[16]) << 24)+
                ((uint32_t) ((uint8_t)raw_image_file_data[17]) << 16)+
                ((uint32_t) ((uint8_t)raw_image_file_data[18]) << 8)+
                ((uint32_t) ((uint8_t)raw_image_file_data[19]) << 0);
            image_height =
                ((uint32_t) ((uint8_t)raw_image_file_data[20]) << 24)+
                ((uint32_t) ((uint8_t)raw_image_file_data[21]) << 16)+
                ((uint32_t) ((uint8_t)raw_image_file_data[22]) << 8)+
                ((uint32_t) ((uint8_t)raw_image_file_data[23]) << 0);
            rpng = rpng_alloc();

            if (!rpng)
            {
               error = "Can't allocate memory.";
               goto finish;
            }

            rpng_set_buf_ptr(rpng, raw_image_file_data, new_image_size);
            rpng_start(rpng);
            while (rpng_iterate_image(rpng));

            do
            {
               retval = rpng_process_image(rpng, &raw_image_data_alpha, new_image_size, &image_width, &image_height);
            }while(retval == IMAGE_PROCESS_NEXT);

            /* Returned output from the png processor is an upside down RGBA
             * image, so we have to change that to RGB first.  This should
             * probably be replaced with a scaler call.*/
            {
               unsigned ui;
               int d,tw,th,tc;
               d=0;
               raw_image_data = (void*)malloc(image_width*image_height*3*sizeof(uint8_t));
               for (ui = 0; ui < image_width * image_height * 4; ui++)
               {
                  if (ui % 4 != 3)
                  {
                     tc = d%3;
                     th = image_height-d / (3*image_width)-1;
                     tw = (d%(image_width*3)) / 3;
                     ((uint8_t*) raw_image_data)[tw*3+th*3*image_width+tc] = ((uint8_t *)raw_image_data_alpha)[ui];
                     d+=1;
                  }
               }
            }
            rpng_free(rpng);
         }
         else
         {
            RARCH_LOG("Output from URL not a valid file type, or is not supported.\n");
            goto finish;
         }

         scaler = (struct scaler_ctx*)calloc(1, sizeof(struct scaler_ctx));
         if (!scaler)
            goto finish;

         if (dummy_data == RETRO_HW_FRAME_BUFFER_VALID)
         {
            /*
               In this case, we used the viewport to grab the image
               and translate it, and we have the translated image in
               the raw_image_data buffer.
            */
            RARCH_LOG("Hardware frame buffer core, but selected video driver isn't supported.\n");
            goto finish;
         }

         /* The assigned pitch may not be reliable.  The width of
            the video frame can change during run-time, but the
            pitch may not, so we just assign it as the width
            times the byte depth.
         */

         if (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
         {
            raw_output_data    = (uint8_t*)malloc(width * height * 4 * sizeof(uint8_t));
            scaler->out_fmt    = SCALER_FMT_ARGB8888;
            pitch              = width * 4;
            scaler->out_stride = width * 4;
         }
         else
         {
            raw_output_data    = (uint8_t*)malloc(width * height * 2 * sizeof(uint8_t));
            scaler->out_fmt    = SCALER_FMT_RGB565;
            pitch              = width * 2;
            scaler->out_stride = width * 1;
         }

         if (!raw_output_data)
            goto finish;

         scaler->in_fmt        = SCALER_FMT_BGR24;
         scaler->in_width      = image_width;
         scaler->in_height     = image_height;
         scaler->out_width     = width;
         scaler->out_height    = height;
         scaler->scaler_type   = SCALER_TYPE_POINT;
         scaler_ctx_gen_filter(scaler);
         scaler->in_stride     = -1 * width * 3;

         scaler_ctx_scale_direct(scaler, raw_output_data,
               (uint8_t*)raw_image_data + (image_height - 1) * width * 3);
         video_driver_frame(raw_output_data, image_width, image_height, pitch);
      }
   }

#ifdef HAVE_AUDIOMIXER
   if (raw_sound_data)
   {
      audio_mixer_stream_params_t params;
      nbio_buf_t *task_data       = (nbio_buf_t*)calloc(1, sizeof(nbio_buf_t));
      nbio_buf_t *img             = (nbio_buf_t*)task_data;

      task_data->buf              = raw_sound_data;
      task_data->bufsize          = new_sound_size;
      task_data->path             = NULL;

      if (!img)
         return;

      params.volume               = 1.0f;
      params.slot_selection_type  = AUDIO_MIXER_SLOT_SELECTION_MANUAL; /* user->slot_selection_type; */
      params.slot_selection_idx   = 10;
      params.stream_type          = AUDIO_STREAM_TYPE_SYSTEM; /* user->stream_type; */
      params.type                 = AUDIO_MIXER_TYPE_WAV;
      params.state                = AUDIO_STREAM_STATE_PLAYING;
      params.buf                  = img->buf;
      params.bufsize              = img->bufsize;
      params.cb                   = NULL;
      params.basename             = NULL;

      audio_driver_mixer_add_stream(&params);

      if (img->path)
         free(img->path);
      if (params.basename)
         free(params.basename);
      free(img);
   }
#endif

   if (key_string)
   {
      char key[8];
      int length = strlen(key_string);
      int i      = 0;
      int start  = 0;
      char t     = ' ';

      for (i = 1; i < length; i++)
      {
         t = key_string[i];
         if (i == length-1 || t == ' ' || t == ',')
         {
            if (i == length-1 && t != ' ' && t!= ',')
               i++;

            if (i-start > 7)
            {
               start = i;
               continue;
            }
            
            strncpy(key, key_string+start, i-start);
            key[i-start] = '\0';

#ifdef HAVE_ACCESSIBILITY
#ifdef HAVE_TRANSLATE
            if (string_is_equal(key, "b"))
               p_rarch->ai_gamepad_state[0] = 2;
            if (string_is_equal(key, "y"))
               p_rarch->ai_gamepad_state[1] = 2;
            if (string_is_equal(key, "select"))
               p_rarch->ai_gamepad_state[2] = 2;
            if (string_is_equal(key, "start"))
               p_rarch->ai_gamepad_state[3] = 2;

            if (string_is_equal(key, "up"))
               p_rarch->ai_gamepad_state[4] = 2;
            if (string_is_equal(key, "down"))
               p_rarch->ai_gamepad_state[5] = 2;
            if (string_is_equal(key, "left"))
               p_rarch->ai_gamepad_state[6] = 2;
            if (string_is_equal(key, "right"))
               p_rarch->ai_gamepad_state[7] = 2;

            if (string_is_equal(key, "a"))
               p_rarch->ai_gamepad_state[8] = 2;
            if (string_is_equal(key, "x"))
               p_rarch->ai_gamepad_state[9] = 2;
            if (string_is_equal(key, "l"))
               p_rarch->ai_gamepad_state[10] = 2;
            if (string_is_equal(key, "r"))
               p_rarch->ai_gamepad_state[11] = 2;

            if (string_is_equal(key, "l2"))
               p_rarch->ai_gamepad_state[12] = 2;
            if (string_is_equal(key, "r2"))
               p_rarch->ai_gamepad_state[13] = 2;
            if (string_is_equal(key, "l3"))
               p_rarch->ai_gamepad_state[14] = 2;
            if (string_is_equal(key, "r3"))
               p_rarch->ai_gamepad_state[15] = 2;
#endif
#endif

            if (string_is_equal(key, "pause"))
               command_event(CMD_EVENT_PAUSE, NULL);
            if (string_is_equal(key, "unpause"))
               command_event(CMD_EVENT_UNPAUSE, NULL);

            start = i+1;
         }
      }
   }

#ifdef HAVE_ACCESSIBILITY
   if (text_string && is_accessibility_enabled())
      accessibility_speak_priority(text_string, 10);
#endif

finish:
   if (error)
      RARCH_ERR("%s: %s\n", msg_hash_to_str(MSG_DOWNLOAD_FAILED), error);

   if (data)
   {
      if (data->data)
         free(data->data);
      free(data);
   }
   if (user_data)
      free(user_data);

   if (body_copy)
      free(body_copy);
   if (raw_image_file_data)
      free(raw_image_file_data);
   if (raw_image_data_alpha)
       free(raw_image_data_alpha);
   if (raw_image_data)
      free(raw_image_data);
   if (scaler)
      free(scaler);
   if (err_string)
      free(err_string);
   if (text_string)
      free(text_string);
   if (raw_output_data)
      free(raw_output_data);

   if (string_is_equal(auto_string, "auto"))
   {
      if (     (p_rarch->ai_service_auto != 0)
            && !settings->bools.ai_service_pause)
         call_auto_translate_task(&was_paused);
   }
   if (auto_string)
      free(auto_string);
   if (key_string)
      free(key_string);
}

static const char *ai_service_get_str(enum translation_lang id)
{
   switch (id)
   {
      case TRANSLATION_LANG_EN:
         return "en";
      case TRANSLATION_LANG_ES:
         return "es";
      case TRANSLATION_LANG_FR:
         return "fr";
      case TRANSLATION_LANG_IT:
         return "it";
      case TRANSLATION_LANG_DE:
         return "de";
      case TRANSLATION_LANG_JP:
         return "ja";
      case TRANSLATION_LANG_NL:
         return "nl";
      case TRANSLATION_LANG_CS:
         return "cs";
      case TRANSLATION_LANG_DA:
         return "da";
      case TRANSLATION_LANG_SV:
         return "sv";
      case TRANSLATION_LANG_HR:
         return "hr";
      case TRANSLATION_LANG_KO:
         return "ko";
      case TRANSLATION_LANG_ZH_CN:
         return "zh-CN";
      case TRANSLATION_LANG_ZH_TW:
         return "zh-TW";
      case TRANSLATION_LANG_CA:
         return "ca";
      case TRANSLATION_LANG_BG:
         return "bg";
      case TRANSLATION_LANG_BN:
         return "bn";
      case TRANSLATION_LANG_EU:
         return "eu";
      case TRANSLATION_LANG_AZ:
         return "az";
      case TRANSLATION_LANG_AR:
         return "ar";
      case TRANSLATION_LANG_SQ:
         return "sq";
      case TRANSLATION_LANG_AF:
         return "af";
      case TRANSLATION_LANG_EO:
         return "eo";
      case TRANSLATION_LANG_ET:
         return "et";
      case TRANSLATION_LANG_TL:
         return "tl";
      case TRANSLATION_LANG_FI:
         return "fi";
      case TRANSLATION_LANG_GL:
         return "gl";
      case TRANSLATION_LANG_KA:
         return "ka";
      case TRANSLATION_LANG_EL:
         return "el";
      case TRANSLATION_LANG_GU:
         return "gu";
      case TRANSLATION_LANG_HT:
         return "ht";
      case TRANSLATION_LANG_IW:
         return "iw";
      case TRANSLATION_LANG_HI:
         return "hi";
      case TRANSLATION_LANG_HU:
         return "hu";
      case TRANSLATION_LANG_IS:
         return "is";
      case TRANSLATION_LANG_ID:
         return "id";
      case TRANSLATION_LANG_GA:
         return "ga";
      case TRANSLATION_LANG_KN:
         return "kn";
      case TRANSLATION_LANG_LA:
         return "la";
      case TRANSLATION_LANG_LV:
         return "lv";
      case TRANSLATION_LANG_LT:
         return "lt";
      case TRANSLATION_LANG_MK:
         return "mk";
      case TRANSLATION_LANG_MS:
         return "ms";
      case TRANSLATION_LANG_MT:
         return "mt";
      case TRANSLATION_LANG_NO:
         return "no";
      case TRANSLATION_LANG_FA:
         return "fa";
      case TRANSLATION_LANG_PL:
         return "pl";
      case TRANSLATION_LANG_PT:
         return "pt";
      case TRANSLATION_LANG_RO:
         return "ro";
      case TRANSLATION_LANG_RU:
         return "ru";
      case TRANSLATION_LANG_SR:
         return "sr";
      case TRANSLATION_LANG_SK:
         return "sk";
      case TRANSLATION_LANG_SL:
         return "sl";
      case TRANSLATION_LANG_SW:
         return "sw";
      case TRANSLATION_LANG_TA:
         return "ta";
      case TRANSLATION_LANG_TE:
         return "te";
      case TRANSLATION_LANG_TH:
         return "th";
      case TRANSLATION_LANG_TR:
         return "tr";
      case TRANSLATION_LANG_UK:
         return "uk";
      case TRANSLATION_LANG_UR:
         return "ur";
      case TRANSLATION_LANG_VI:
         return "vi";
      case TRANSLATION_LANG_CY:
         return "cy";
      case TRANSLATION_LANG_YI:
         return "yi";
      case TRANSLATION_LANG_DONT_CARE:
      case TRANSLATION_LANG_LAST:
         break;
   }

   return "";
}


/*
   This function does all the stuff needed to translate the game screen,
   using the URL given in the settings.  Once the image from the frame
   buffer is sent to the server, the callback will write the translated
   image to the screen.

   Supported client/services (thus far)
   -VGTranslate client ( www.gitlab.com/spherebeaker/vg_translate )
   -Ztranslate client/service ( www.ztranslate.net/docs/service )

   To use a client, download the relevant code/release, configure
   them, and run them on your local machine, or network.  Set the
   retroarch configuration to point to your local client (usually
   listening on localhost:4404 ) and enable translation service.

   If you don't want to run a client, you can also use a service,
   which is basically like someone running a client for you.  The
   downside here is that your retroarch device will have to have
   an internet connection, and you may have to sign up for it.

   To make your own server, it must listen for a POST request, which
   will consist of a JSON body, with the "image" field as a base64
   encoded string of a 24bit-BMP/PNG that the will be translated.
   The server must output the translated image in the form of a
   JSON body, with the "image" field also as a base64 encoded
   24bit-BMP, or as an alpha channel png.

  "paused" boolean is passed in to indicate if the current call
   was made during a paused frame.  Due to how the menu widgets work,
   if the ai service is called in "auto" mode, then this call will
   be made while the menu widgets unpause the core for a frame to update
   the on-screen widgets.  To tell the ai service what the pause
   mode is honestly, we store the runloop_paused variable from before
   the handle_translation_cb wipes the widgets, and pass that in here.
*/

static bool run_translation_service(bool paused)
{
   struct video_viewport vp;
   uint8_t header[54];
   size_t pitch;
   unsigned width, height;
   const void *data                      = NULL;
   uint8_t *bit24_image                  = NULL;
   uint8_t *bit24_image_prev             = NULL;
   struct rarch_state *p_rarch           = &rarch_st;
   settings_t *settings                  = p_rarch->configuration_settings;
   struct scaler_ctx *scaler             = (struct scaler_ctx*)
      calloc(1, sizeof(struct scaler_ctx));
   bool error                            = false;
   bool playlist_fuzzy_archive_match     = settings->bools.playlist_fuzzy_archive_match;

   uint8_t *bmp_buffer                   = NULL;
   uint64_t buffer_bytes                 = 0;
   char *bmp64_buffer                    = NULL;
   char *json_buffer                     = NULL;

   int out_length                        = 0;
   int json_length                       = 0;
   const char *rf1                       = "{\"image\": \"";
   const char *rf2                       = "\"}\0";
   char *rf3                             = NULL;
   char *state_son                       = NULL;
   int state_son_length                  = 0;
   int curr_length                       = 0;
   bool TRANSLATE_USE_BMP                = false;
   bool use_overlay                      = false;

   const char *label                     = NULL;
   char* system_label                    = NULL;
   core_info_t *core_info                = NULL;
   const enum retro_pixel_format 
      video_driver_pix_fmt               = p_rarch->video_driver_pix_fmt;

#ifdef HAVE_GFX_WIDGETS
   if (  (gfx_widgets_ai_service_overlay_get_state() != 0)
         && (p_rarch->ai_service_auto == 1))
   {
      /* For the case when ai service pause is disabled. */
      gfx_widgets_ai_service_overlay_unload();
      goto finish;
   }
#endif

#ifdef HAVE_GFX_WIDGETS
   if (     p_rarch->video_driver_poke
         && p_rarch->video_driver_poke->load_texture
         && p_rarch->video_driver_poke->unload_texture)
      use_overlay = true;
#endif

   /* get the core info here so we can pass long the game name */
   core_info_get_current_core(&core_info);

   if (core_info)
   {
      size_t label_len;
      const char *system_id               = core_info->system_id
         ? core_info->system_id : "core";
      size_t system_id_len                = strlen(system_id);
      const struct playlist_entry *entry  = NULL;
      playlist_t *current_playlist        = playlist_get_cached();

      if (current_playlist)
      {
         playlist_get_index_by_path(
            current_playlist, path_get(RARCH_PATH_CONTENT), &entry,
            playlist_fuzzy_archive_match);

         if (entry && !string_is_empty(entry->label))
            label = entry->label;
      }

      if (!label)
         label     = path_basename(path_get(RARCH_PATH_BASENAME));
      label_len    = strlen(label);
      system_label = (char*)malloc(label_len + system_id_len + 3);
      memcpy(system_label, system_id, system_id_len);
      memcpy(system_label + system_id_len, "__", 2);
      memcpy(system_label + 2 + system_id_len, label, label_len);
      system_label[system_id_len + 2 + label_len] = '\0';
   }

   if (!scaler)
      goto finish;

   video_driver_cached_frame_get(&data, &width, &height, &pitch);

   if (!data)
      goto finish;

   if (data == RETRO_HW_FRAME_BUFFER_VALID)
   {
      /*
        The direct frame capture didn't work, so try getting it
        from the viewport instead.  This isn't as good as the
        raw frame buffer, since the viewport may us bilinear
        filtering, or other shaders that will completely trash
        the OCR, but it's better than nothing.
      */
      vp.x                           = 0;
      vp.y                           = 0;
      vp.width                       = 0;
      vp.height                      = 0;
      vp.full_width                  = 0;
      vp.full_height                 = 0;

      video_driver_get_viewport_info(&vp);

      if (!vp.width || !vp.height)
         goto finish;

      bit24_image_prev = (uint8_t*)malloc(vp.width * vp.height * 3);
      bit24_image = (uint8_t*)malloc(width * height * 3);

      if (!bit24_image_prev || !bit24_image)
         goto finish;

      if (!video_driver_read_viewport(bit24_image_prev, false))
      {
         RARCH_LOG("Could not read viewport for translation service...\n");
         goto finish;
      }

      /* TODO: Rescale down to regular resolution */
      scaler->in_fmt      = SCALER_FMT_BGR24;
      scaler->out_fmt     = SCALER_FMT_BGR24;
      scaler->scaler_type = SCALER_TYPE_POINT;
      scaler->in_width    = vp.width;
      scaler->in_height   = vp.height;
      scaler->out_width   = width;
      scaler->out_height  = height;
      scaler_ctx_gen_filter(scaler);

      scaler->in_stride   = vp.width*3;
      scaler->out_stride  = width*3;
      scaler_ctx_scale_direct(scaler, bit24_image, bit24_image_prev);
      scaler_ctx_gen_reset(scaler);
   }
   else
   {
      /* This is a software core, so just change the pixel format to 24-bit. */
      bit24_image = (uint8_t*)malloc(width * height * 3);
      if (!bit24_image)
          goto finish;

      if (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
         scaler->in_fmt = SCALER_FMT_ARGB8888;
      else
         scaler->in_fmt = SCALER_FMT_RGB565;
      video_frame_convert_to_bgr24(
         scaler,
         (uint8_t *)bit24_image,
         (const uint8_t*)data + ((int)height - 1)*pitch,
         width, height,
         -pitch);

      scaler_ctx_gen_reset(scaler);
   }

   if (!bit24_image)
   {
      error = true;
      goto finish;
   }

   if (TRANSLATE_USE_BMP)
   {
      /*
        At this point, we should have a screenshot in the buffer, so allocate
        an array to contain the BMP image along with the BMP header as bytes,
        and then covert that to a b64 encoded array for transport in JSON.
      */

      form_bmp_header(header, width, height, false);
      bmp_buffer = (uint8_t*)malloc(width * height * 3+54);
      if (!bmp_buffer)
         goto finish;

      memcpy(bmp_buffer, header, 54*sizeof(uint8_t));
      memcpy(bmp_buffer+54, bit24_image, width * height * 3 * sizeof(uint8_t));
      buffer_bytes = sizeof(uint8_t)*(width*height*3+54);
   }
   else
   {
      pitch      = width * 3;
      bmp_buffer = rpng_save_image_bgr24_string(bit24_image+width*(height-1)*3, width, height, -pitch, &buffer_bytes);
   }

   bmp64_buffer = base64((void *)bmp_buffer, sizeof(uint8_t)*buffer_bytes,
         &out_length);

   if (!bmp64_buffer)
      goto finish;

   /* Form request... */
   if (system_label)
   {
      unsigned i;
      size_t system_label_len = strlen(system_label);

      /* include game label if provided */
      rf3 = (char *)malloc(15 + system_label_len);
      memcpy(rf3, ", \"label\": \"", 12*sizeof(uint8_t));
      memcpy(rf3 + 12, system_label, system_label_len);
      memcpy(rf3 + 12 + system_label_len, "\"}\0", 3*sizeof(uint8_t));
      for (i = 12; i < system_label_len + 12; i++)
      {
         if (rf3[i] == '\"')
            rf3[i] = ' ';
      }
      json_length = 11 + out_length + 15 + system_label_len;
   }
   else
      json_length = 11 + out_length + 1;

   {
      state_son_length = 177;
      state_son        = (char*)malloc(state_son_length);

      memcpy(state_son, ", \"state\": {\"paused\": 0, \"a\": 0, \"b\": 0, \"select\": 0, \"start\": 0, \"up\": 0, \"down\": 0, \"left\": 0, \"right\": 0, \"x\": 0, \"y\": 0, \"l\": 0, \"r\":0, \"l2\": 0, \"r2\": 0, \"l3\":0, \"r3\": 0}}\0", state_son_length*sizeof(uint8_t));

      if (paused)
         state_son[22] = '1';

#ifdef HAVE_ACCESSIBILITY
#ifdef HAVE_TRANSLATE
      if (p_rarch->ai_gamepad_state[8]) /* a */
         state_son[30] = '1';
      if (p_rarch->ai_gamepad_state[0]) /* b */
         state_son[38] = '1';
      if (p_rarch->ai_gamepad_state[2]) /* select */
         state_son[51] = '1';
      if (p_rarch->ai_gamepad_state[3]) /* start */
         state_son[63] = '1';

      if (p_rarch->ai_gamepad_state[4]) /* up */
         state_son[72] = '1';
      if (p_rarch->ai_gamepad_state[5]) /* down */
         state_son[83] = '1';
      if (p_rarch->ai_gamepad_state[6]) /* left */
         state_son[94] = '1';
      if (p_rarch->ai_gamepad_state[7]) /* right */
         state_son[106] = '1';

      if (p_rarch->ai_gamepad_state[9]) /* x */
         state_son[114] = '1';
      if (p_rarch->ai_gamepad_state[1]) /* y */
         state_son[122] = '1';
      if (p_rarch->ai_gamepad_state[10]) /* l */
         state_son[130] = '1';
      if (p_rarch->ai_gamepad_state[11]) /* r */
         state_son[138] = '1';

      if (p_rarch->ai_gamepad_state[12]) /* l2 */
         state_son[147] = '1';
      if (p_rarch->ai_gamepad_state[13]) /* r2 */
         state_son[156] = '1';
      if (p_rarch->ai_gamepad_state[14]) /* l3 */
         state_son[165] = '1';
      if (p_rarch->ai_gamepad_state[15]) /* r3 */
         state_son[174] = '1';
#endif
#endif

      json_length+=state_son_length;
   }

   json_buffer = (char*)malloc(json_length);
   if (!json_buffer)
      goto finish;
    
   /* Image data */
   memcpy(json_buffer, (const void*)rf1, 11 * sizeof(uint8_t));
   memcpy(json_buffer + 11, bmp64_buffer, out_length * sizeof(uint8_t));
   memcpy(json_buffer + 11 + out_length, "\"", 1 * sizeof(uint8_t));
   curr_length = 11 + out_length + 1;

   /* State data */
   memcpy(json_buffer+curr_length, state_son, state_son_length*sizeof(uint8_t));
   curr_length += state_son_length;

   /* System Label */
   if (rf3)
   {
      size_t system_label_len = strlen(system_label);
      memcpy(json_buffer + curr_length, (const void*)rf3, (15 + system_label_len) * sizeof(uint8_t));
      curr_length += 15 + system_label_len;
   }
   else
   {
      memcpy(json_buffer + curr_length, (const void*)rf2, 3 * sizeof(uint8_t));
      curr_length += 3;
   }

#ifdef DEBUG
   if (p_rarch->ai_service_auto != 2)
      RARCH_LOG("Request size: %d\n", out_length);
#endif
   {
      char separator  = '?';
      char new_ai_service_url[PATH_MAX_LENGTH];
      unsigned ai_service_source_lang = settings->uints.ai_service_source_lang;
      unsigned ai_service_target_lang = settings->uints.ai_service_target_lang;
      const char *ai_service_url      = settings->arrays.ai_service_url;

      strlcpy(new_ai_service_url, ai_service_url, sizeof(new_ai_service_url));

      /* if query already exists in url, then use &'s instead */
      if (strrchr(new_ai_service_url, '?'))
          separator = '&';

      /* source lang */
      if (ai_service_source_lang != TRANSLATION_LANG_DONT_CARE)
      {
         const char *lang_source = ai_service_get_str(
               (enum translation_lang)ai_service_source_lang);

         if (!string_is_empty(lang_source))
         {
            char temp_string[PATH_MAX_LENGTH];
            snprintf(temp_string,
                  sizeof(temp_string),
                  "%csource_lang=%s", separator, lang_source);
            separator = '&';
            strlcat(new_ai_service_url, temp_string, sizeof(new_ai_service_url));
         }
      }

      /* target lang */
      if (ai_service_target_lang != TRANSLATION_LANG_DONT_CARE)
      {
         const char *lang_target = ai_service_get_str(
               (enum translation_lang)ai_service_target_lang);

         if (!string_is_empty(lang_target))
         {
            char temp_string[PATH_MAX_LENGTH];
            snprintf(temp_string,
                  sizeof(temp_string),
                  "%ctarget_lang=%s", separator, lang_target);
            separator = '&';

            strlcat(new_ai_service_url, temp_string,
                  sizeof(new_ai_service_url));
         }
      }

      /* mode */
      {
         char temp_string[PATH_MAX_LENGTH];
         const char *mode_chr                    = NULL;
         unsigned ai_service_mode                = settings->uints.ai_service_mode;
         /*"image" is included for backwards compatability with
          * vgtranslate < 1.04 */

         temp_string[0] = '\0';

         switch (ai_service_mode)
         {
            case 0:
               if (use_overlay)
                  mode_chr = "image,png,png-a";
               else
                  mode_chr = "image,png";
               break;
            case 1:
               mode_chr = "sound,wav";
               break;
            case 2:
               mode_chr = "text";
               break;
            case 3:
               if (use_overlay)
                  mode_chr = "image,png,png-a,sound,wav";
               else
                  mode_chr = "image,png,sound,wav";
               break;
            default:
               break;
         }

         snprintf(temp_string,
               sizeof(temp_string),
               "%coutput=%s", separator, mode_chr);
         separator = '&';

         strlcat(new_ai_service_url, temp_string,
                 sizeof(new_ai_service_url));
      }
#ifdef DEBUG
      if (p_rarch->ai_service_auto != 2)
         RARCH_LOG("SENDING... %s\n", new_ai_service_url);
#endif
      task_push_http_post_transfer(new_ai_service_url,
            json_buffer, true, NULL, handle_translation_cb, NULL);
   }

   error = false;
finish:
   if (bit24_image_prev)
      free(bit24_image_prev);
   if (bit24_image)
      free(bit24_image);

   if (scaler)
      free(scaler);

   if (bmp_buffer)
      free(bmp_buffer);

   if (bmp64_buffer)
      free(bmp64_buffer);
   if (rf3)
      free(rf3);
   if (system_label)
      free(system_label);
   if (json_buffer)
      free(json_buffer);
   return !error;
}
#endif

/**
 * command_event_disk_control_append_image:
 * @path                 : Path to disk image.
 *
 * Appends disk image to disk image list.
 **/
static bool command_event_disk_control_append_image(const char *path)
{
   struct rarch_state *p_rarch   = &rarch_st;
   rarch_system_info_t *sys_info = &p_rarch->runloop_system;

   if (!sys_info)
      return false;

   if (!disk_control_append_image(&sys_info->disk_control, path))
      return false;

#ifdef HAVE_THREADS
   retroarch_autosave_deinit();
#endif

   /* TODO: Need to figure out what to do with subsystems case. */
   if (path_is_empty(RARCH_PATH_SUBSYSTEM))
   {
      /* Update paths for our new image.
       * If we actually use append_image, we assume that we
       * started out in a single disk case, and that this way
       * of doing it makes the most sense. */
      path_set(RARCH_PATH_NAMES, path);
      path_fill_names();
   }

   command_event(CMD_EVENT_AUTOSAVE_INIT, NULL);

   return true;
}

/**
 * event_set_volume:
 * @gain      : amount of gain to be applied to current volume level.
 *
 * Adjusts the current audio volume level.
 *
 **/
static void command_event_set_volume(float gain)
{
   char msg[128];
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   float new_volume            = settings->floats.audio_volume + gain;

   new_volume                  = MAX(new_volume, -80.0f);
   new_volume                  = MIN(new_volume, 12.0f);

   configuration_set_float(settings, settings->floats.audio_volume, new_volume);

   snprintf(msg, sizeof(msg), "%s: %.1f dB",
         msg_hash_to_str(MSG_AUDIO_VOLUME),
         new_volume);

#if defined(HAVE_GFX_WIDGETS)
   if (gfx_widgets_active())
      gfx_widget_volume_update_and_show(new_volume,
            p_rarch->audio_driver_mute_enable);
   else
#endif
      runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   RARCH_LOG("%s\n", msg);

   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, new_volume);
}

/**
 * event_set_mixer_volume:
 * @gain      : amount of gain to be applied to current volume level.
 *
 * Adjusts the current audio volume level.
 *
 **/
static void command_event_set_mixer_volume(float gain)
{
   char msg[128];
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   float new_volume            = settings->floats.audio_mixer_volume + gain;

   new_volume                  = MAX(new_volume, -80.0f);
   new_volume                  = MIN(new_volume, 12.0f);

   configuration_set_float(settings, settings->floats.audio_mixer_volume, new_volume);

   snprintf(msg, sizeof(msg), "%s: %.1f dB",
         msg_hash_to_str(MSG_AUDIO_VOLUME),
         new_volume);
   runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s\n", msg);

   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, new_volume);
}

/**
 * command_event_init_controllers:
 *
 * Initialize libretro controllers.
 **/
static void command_event_init_controllers(void)
{
   unsigned i;
   struct rarch_state *p_rarch   = &rarch_st;
   rarch_system_info_t *info     = &p_rarch->runloop_system;

   for (i = 0; i < MAX_USERS; i++)
   {
      retro_ctx_controller_info_t pad;
      const char *ident                               = NULL;
      bool set_controller                             = false;
      const struct retro_controller_description *desc = NULL;
      unsigned device                                 = input_config_get_device(i);

      if (info)
      {
         if (i < info->ports.size)
            desc = libretro_find_controller_description(
                  &info->ports.data[i], device);
      }

      if (desc)
         ident = desc->desc;

      if (!ident)
      {
         /* If we're trying to connect a completely unknown device,
          * revert back to JOYPAD. */

         if (device != RETRO_DEVICE_JOYPAD && device != RETRO_DEVICE_NONE)
         {
            /* Do not fix device,
             * because any use of dummy core will reset this,
             * which is not a good idea. */
            RARCH_WARN("Input device ID %u is unknown to this "
                  "libretro implementation. Using RETRO_DEVICE_JOYPAD.\n",
                  device);
            device = RETRO_DEVICE_JOYPAD;
         }
         ident = "Joypad";
      }

      switch (device)
      {
         case RETRO_DEVICE_NONE:
            RARCH_LOG("%s %u.\n",
                  msg_hash_to_str(MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT),
                  i + 1);
            set_controller = true;
            break;
         case RETRO_DEVICE_JOYPAD:
            /* Ideally these checks shouldn't be required but if we always
             * call core_set_controller_port_device input won't work on
             * cores that don't set port information properly */
            if (info && info->ports.size != 0)
               set_controller = true;
            break;
         default:
            /* Some cores do not properly range check port argument.
             * This is broken behavior of course, but avoid breaking
             * cores needlessly. */
            RARCH_LOG("%s %u: %s (ID: %u).\n",
                    msg_hash_to_str(MSG_CONNECTING_TO_PORT),
                    device, ident, i+1);
            set_controller = true;
            break;
      }

      if (set_controller && info && i < info->ports.size)
      {
         pad.device     = device;
         pad.port       = i;
         core_set_controller_port_device(&pad);
      }
   }
}

#ifdef HAVE_CONFIGFILE
static void command_event_disable_overrides(void)
{
   struct rarch_state       *p_rarch = &rarch_st;

   /* reload the original config */
   config_unload_override();
   p_rarch->runloop_overrides_active = false;
}
#endif

static void command_event_deinit_core(bool reinit)
{
   struct rarch_state *p_rarch = &rarch_st;

#ifdef HAVE_CHEEVOS
   rcheevos_unload();
#endif

   RARCH_LOG("Unloading game..\n");
   core_unload_game();

   RARCH_LOG("Unloading core..\n");

   video_driver_set_cached_frame_ptr(NULL);

   if (p_rarch->current_core.inited)
      p_rarch->current_core.retro_deinit();

   RARCH_LOG("Unloading core symbols..\n");
   uninit_libretro_symbols(&p_rarch->current_core);
   p_rarch->current_core.symbols_inited = false;

   if (reinit)
      driver_uninit(DRIVERS_CMD_ALL);

#ifdef HAVE_CONFIGFILE
   if (p_rarch->runloop_overrides_active)
      command_event_disable_overrides();
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   retroarch_unset_runtime_shader_preset();
#endif

#ifdef HAVE_CONFIGFILE
   if (     p_rarch->runloop_remaps_core_active
         || p_rarch->runloop_remaps_content_dir_active
         || p_rarch->runloop_remaps_game_active
      )
      input_remapping_set_defaults(true);
#endif
}

static void command_event_init_cheats(void)
{
   struct rarch_state *p_rarch   = &rarch_st;
   settings_t     *settings      = p_rarch->configuration_settings;
   bool        allow_cheats      = true;
   bool apply_cheats_after_load  = settings->bools.apply_cheats_after_load;
   const char *path_cheat_db     = settings->paths.path_cheat_database;
#ifdef HAVE_NETWORKING
   allow_cheats                 &= !netplay_driver_ctl(
         RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL);
#endif
   allow_cheats                 &= !(p_rarch->bsv_movie_state_handle != NULL);

   if (!allow_cheats)
      return;

   cheat_manager_alloc_if_empty();
   cheat_manager_load_game_specific_cheats(path_cheat_db);

   if (apply_cheats_after_load)
      cheat_manager_apply_cheats();
}

static void command_event_load_auto_state(void)
{
   bool ret                        = false;
   char *savestate_name_auto       = NULL;
   size_t savestate_name_auto_size = PATH_MAX_LENGTH * sizeof(char);
   struct rarch_state *p_rarch     = &rarch_st;
   settings_t *settings            = p_rarch->configuration_settings;
   global_t   *global              = &p_rarch->g_extern;
   bool savestate_auto_load        = settings->bools.savestate_auto_load;

   if (!global || !savestate_auto_load)
      return;
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active)
      return;
#endif
#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
      return;
#endif

   savestate_name_auto             = (char*)calloc(PATH_MAX_LENGTH,
         sizeof(*savestate_name_auto));

   fill_pathname_noext(savestate_name_auto, global->name.savestate,
         ".auto", savestate_name_auto_size);

   if (!path_is_valid(savestate_name_auto))
   {
      free(savestate_name_auto);
      return;
   }

   ret = content_load_state(savestate_name_auto, false, true);

   RARCH_LOG("%s: %s\n%s \"%s\" %s.\n",
         msg_hash_to_str(MSG_FOUND_AUTO_SAVESTATE_IN),
         savestate_name_auto,
         msg_hash_to_str(MSG_AUTOLOADING_SAVESTATE_FROM),
         savestate_name_auto, ret ? "succeeded" : "failed"
         );

   free(savestate_name_auto);
}

static void command_event_set_savestate_auto_index(void)
{
   size_t i;
   char *state_dir                   = NULL;
   char *state_base                  = NULL;

   size_t state_size                 = PATH_MAX_LENGTH * sizeof(char);
   struct string_list *dir_list      = NULL;
   unsigned max_idx                  = 0;
   struct rarch_state *p_rarch       = &rarch_st;
   settings_t *settings              = p_rarch->configuration_settings;
   global_t   *global                = &p_rarch->g_extern;
   bool savestate_auto_index         = settings->bools.savestate_auto_index;
   bool show_hidden_files            = settings->bools.show_hidden_files;

   if (!global || !savestate_auto_index)
      return;

   state_dir                         = (char*)calloc(PATH_MAX_LENGTH, sizeof(*state_dir));

   /* Find the file in the same directory as global->savestate_name
    * with the largest numeral suffix.
    *
    * E.g. /foo/path/content.state, will try to find
    * /foo/path/content.state%d, where %d is the largest number available.
    */
   fill_pathname_basedir(state_dir, global->name.savestate,
         state_size);

   dir_list = dir_list_new_special(state_dir, DIR_LIST_PLAIN, NULL,
         show_hidden_files);

   free(state_dir);

   if (!dir_list)
      return;

   state_base                        = (char*)calloc(PATH_MAX_LENGTH, sizeof(*state_base));

   fill_pathname_base(state_base, global->name.savestate,
         state_size);

   for (i = 0; i < dir_list->size; i++)
   {
      unsigned idx;
      char elem_base[128]             = {0};
      const char *end                 = NULL;
      const char *dir_elem            = dir_list->elems[i].data;

      fill_pathname_base(elem_base, dir_elem, sizeof(elem_base));

      if (strstr(elem_base, state_base) != elem_base)
         continue;

      end = dir_elem + strlen(dir_elem);
      while ((end > dir_elem) && isdigit((int)end[-1]))
         end--;

      idx = (unsigned)strtoul(end, NULL, 0);
      if (idx > max_idx)
         max_idx = idx;
   }

   dir_list_free(dir_list);
   free(state_base);

   configuration_set_int(settings, settings->ints.state_slot, max_idx);

   RARCH_LOG("%s: #%d\n",
         msg_hash_to_str(MSG_FOUND_LAST_STATE_SLOT),
         max_idx);
}

static bool event_init_content(void)
{
   bool contentless                             = false;
   bool is_inited                               = false;
   struct rarch_state *p_rarch                  = &rarch_st;
   const enum rarch_core_type current_core_type = p_rarch->current_core_type;

   content_get_status(&contentless, &is_inited);

   /* TODO/FIXME - just because we have a contentless core does not
    * necessarily mean there should be no SRAM, try to find a solution here */
   p_rarch->rarch_use_sram   = (current_core_type == CORE_TYPE_PLAIN)
      && !contentless;

   /* No content to be loaded for dummy core,
    * just successfully exit. */
   if (current_core_type == CORE_TYPE_DUMMY)
      return true;

   content_set_subsystem_info();

   content_get_status(&contentless, &is_inited);

   if (!contentless)
      path_fill_names();

   if (!content_init())
   {
      p_rarch->runloop_core_running = false;
      return false;
   }

   command_event_set_savestate_auto_index();

   if (event_load_save_files(p_rarch->rarch_is_sram_load_disabled))
      RARCH_LOG("%s.\n",
            msg_hash_to_str(MSG_SKIPPING_SRAM_LOAD));

/*
   Since the operations are asynchronous we can't
   guarantee users will not use auto_load_state to cheat on
   achievements so we forbid auto_load_state from happening
   if cheevos_enable and cheevos_hardcode_mode_enable
   are true.
*/
#ifdef HAVE_CHEEVOS
   {
      settings_t *settings              = p_rarch->configuration_settings;
      bool cheevos_enable               = settings->bools.cheevos_enable;
      bool cheevos_hardcore_mode_enable = settings->bools.cheevos_hardcore_mode_enable;
      if (!cheevos_enable || !cheevos_hardcore_mode_enable)
         command_event_load_auto_state();
   }
#else
   command_event_load_auto_state();
#endif

   bsv_movie_deinit();
   bsv_movie_init();
   command_event(CMD_EVENT_NETPLAY_INIT, NULL);

   return true;
}

static void update_runtime_log(bool log_per_core)
{
   struct rarch_state  *p_rarch = &rarch_st;
   settings_t  *settings        = p_rarch->configuration_settings;
   const char  *dir_runtime_log = settings->paths.directory_runtime_log;
   const char  *dir_playlist    = settings->paths.directory_playlist;

   /* Initialise runtime log file */
   runtime_log_t *runtime_log   = runtime_log_init(
         p_rarch->runtime_content_path,
         p_rarch->runtime_core_path,
         dir_runtime_log,
         dir_playlist,
         log_per_core);

   if (!runtime_log)
      return;

   /* Add additional runtime */
   runtime_log_add_runtime_usec(runtime_log,
         p_rarch->libretro_core_runtime_usec);

   /* Update 'last played' entry */
   runtime_log_set_last_played_now(runtime_log);

   /* Save runtime log file */
   runtime_log_save(runtime_log);

   /* Clean up */
   free(runtime_log);
}


static void command_event_runtime_log_deinit(void)
{
   char log[PATH_MAX_LENGTH]    = {0};
   unsigned hours               = 0;
   unsigned minutes             = 0;
   unsigned seconds             = 0;
   int n                        = 0;
   struct rarch_state  *p_rarch = &rarch_st;

   runtime_log_convert_usec2hms(
         p_rarch->libretro_core_runtime_usec,
         &hours, &minutes, &seconds);

   n = snprintf(log, sizeof(log),
         "Content ran for a total of: %02u hours, %02u minutes, %02u seconds.",
         hours, minutes, seconds);
   if ((n < 0) || (n >= PATH_MAX_LENGTH))
      n = 0; /* Just silence any potential gcc warnings... */
   RARCH_LOG("%s\n",log);

   /* Only write to file if content has run for a non-zero length of time */
   if (p_rarch->libretro_core_runtime_usec > 0)
   {
      settings_t *settings               = p_rarch->configuration_settings;
      bool content_runtime_log           = settings->bools.content_runtime_log;
      bool content_runtime_log_aggregate = settings->bools.content_runtime_log_aggregate;

      /* Per core logging */
      if (content_runtime_log)
         update_runtime_log(true);

      /* Aggregate logging */
      if (content_runtime_log_aggregate)
         update_runtime_log(false);
   }

   /* Reset runtime + content/core paths, to prevent any
    * possibility of duplicate logging */
   p_rarch->libretro_core_runtime_usec = 0;
   memset(p_rarch->runtime_content_path, 0, sizeof(p_rarch->runtime_content_path));
   memset(p_rarch->runtime_core_path, 0, sizeof(p_rarch->runtime_core_path));
}

static void command_event_runtime_log_init(void)
{
   struct rarch_state         *p_rarch = &rarch_st;
   const char *content_path            = path_get(RARCH_PATH_CONTENT);
   const char *core_path               = path_get(RARCH_PATH_CORE);

   p_rarch->libretro_core_runtime_last = cpu_features_get_time_usec();
   p_rarch->libretro_core_runtime_usec = 0;

   /* Have to cache content and core path here, otherwise
    * logging fails if new content is loaded without
    * closing existing content
    * i.e. RARCH_PATH_CONTENT and RARCH_PATH_CORE get
    * updated when the new content is loaded, which
    * happens *before* command_event_runtime_log_deinit
    * -> using RARCH_PATH_CONTENT and RARCH_PATH_CORE
    *    directly in command_event_runtime_log_deinit
    *    can therefore lead to the runtime of the currently
    *    loaded content getting written to the *new*
    *    content's log file... */
   memset(p_rarch->runtime_content_path, 0, sizeof(p_rarch->runtime_content_path));
   memset(p_rarch->runtime_core_path,    0, sizeof(p_rarch->runtime_core_path));

   if (!string_is_empty(content_path))
      strlcpy(p_rarch->runtime_content_path,
            content_path,
            sizeof(p_rarch->runtime_content_path));

   if (!string_is_empty(core_path))
      strlcpy(p_rarch->runtime_core_path,
            core_path,
            sizeof(p_rarch->runtime_core_path));
}

static void retroarch_set_frame_limit(float fastforward_ratio_orig)
{
   struct rarch_state          *p_rarch = &rarch_st;
   struct retro_system_av_info *av_info = &p_rarch->video_driver_av_info;
   float fastforward_ratio              = (fastforward_ratio_orig == 0.0f) 
      ? 1.0f : fastforward_ratio_orig;

   p_rarch->frame_limit_last_time       = cpu_features_get_time_usec();
   p_rarch->frame_limit_minimum_time    = (retro_time_t)roundf(1000000.0f
         / (av_info->timing.fps * fastforward_ratio));
}

static bool command_event_init_core(enum rarch_core_type type)
{
   struct rarch_state *p_rarch     = &rarch_st;
   settings_t *settings            = p_rarch->configuration_settings;
#ifdef HAVE_CONFIGFILE
   bool auto_overrides_enable      = settings->bools.auto_overrides_enable;
   bool auto_remaps_enable         = settings->bools.auto_remaps_enable;
   const char *dir_input_remapping = settings->paths.directory_input_remapping;
#endif
   unsigned poll_type_behavior     = settings->uints.input_poll_type_behavior;
   float fastforward_ratio         = settings->floats.fastforward_ratio;
   rarch_system_info_t *sys_info   = &p_rarch->runloop_system;

   if (!init_libretro_symbols(type, &p_rarch->current_core))
      return false;
   if (!p_rarch->current_core.retro_run)
      p_rarch->current_core.retro_run   = retro_run_null;
   p_rarch->current_core.symbols_inited = true;

   p_rarch->current_core.retro_get_system_info(&sys_info->info);

   if (!sys_info->info.library_name)
      sys_info->info.library_name = msg_hash_to_str(MSG_UNKNOWN);
   if (!sys_info->info.library_version)
      sys_info->info.library_version = "v0";

   fill_pathname_join_concat_noext(
         p_rarch->video_driver_title_buf,
         msg_hash_to_str(MSG_PROGRAM),
         " ",
         sys_info->info.library_name,
         sizeof(p_rarch->video_driver_title_buf));
   strlcat(p_rarch->video_driver_title_buf, " ",
         sizeof(p_rarch->video_driver_title_buf));
   strlcat(p_rarch->video_driver_title_buf,
         sys_info->info.library_version,
         sizeof(p_rarch->video_driver_title_buf));

   strlcpy(sys_info->valid_extensions,
         sys_info->info.valid_extensions ?
         sys_info->info.valid_extensions : DEFAULT_EXT,
         sizeof(sys_info->valid_extensions));

#ifdef HAVE_CONFIGFILE
   if (auto_overrides_enable)
      p_rarch->runloop_overrides_active = 
         config_load_override(&p_rarch->runloop_system);
#endif

   /* Load auto-shaders on the next occasion */
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   shader_presets_need_reload              = true;
   p_rarch->shader_delay_timer.timer_begin = false; /* not initialized */
   p_rarch->shader_delay_timer.timer_end   = false; /* not expired */
#endif

   /* reset video format to libretro's default */
   p_rarch->video_driver_pix_fmt = RETRO_PIXEL_FORMAT_0RGB1555;

   p_rarch->current_core.retro_set_environment(rarch_environment_cb);

#ifdef HAVE_CONFIGFILE
   if (auto_remaps_enable)
      config_load_remap(dir_input_remapping, &p_rarch->runloop_system);
#endif

   /* Per-core saves: reset redirection paths */
   path_set_redirect();

   video_driver_set_cached_frame_ptr(NULL);

   p_rarch->current_core.retro_init();
   p_rarch->current_core.inited          = true;

   /* Attempt to set initial disk index */
   disk_control_set_initial_index(
         &sys_info->disk_control,
         path_get(RARCH_PATH_CONTENT),
         p_rarch->current_savefile_dir);

   if (!event_init_content())
      return false;

   /* Verify that initial disk index was set correctly */
   disk_control_verify_initial_index(&sys_info->disk_control);

   if (!core_load(poll_type_behavior))
      return false;

   retroarch_set_frame_limit(fastforward_ratio);
   command_event_runtime_log_init();
   return true;
}

static bool command_event_save_auto_state(void)
{
   bool ret                    = false;
   char *savestate_name_auto   = NULL;
   size_t
      savestate_name_auto_size = PATH_MAX_LENGTH * sizeof(char);
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   global_t   *global          = &p_rarch->g_extern;
   bool savestate_auto_save    = settings->bools.savestate_auto_save;
   const enum rarch_core_type 
      current_core_type        = p_rarch->current_core_type;

   if (!global || !savestate_auto_save)
      return false;
   if (current_core_type == CORE_TYPE_DUMMY)
      return false;

   if (string_is_empty(path_basename(path_get(RARCH_PATH_BASENAME))))
      return false;

#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active)
      return false;
#endif

   savestate_name_auto         = (char*)
      calloc(PATH_MAX_LENGTH, sizeof(*savestate_name_auto));

   fill_pathname_noext(savestate_name_auto, global->name.savestate,
         ".auto", savestate_name_auto_size);

   ret = content_save_state((const char*)savestate_name_auto, true, true);
   RARCH_LOG("%s \"%s\" %s.\n",
         msg_hash_to_str(MSG_AUTO_SAVE_STATE_TO),
         savestate_name_auto, ret ?
         "succeeded" : "failed");

   free(savestate_name_auto);
   return true;
}

#ifdef HAVE_CONFIGFILE
static bool command_event_save_config(
      const char *config_path,
      char *s, size_t len)
{
   char log[PATH_MAX_LENGTH];
   bool path_exists = !string_is_empty(config_path);
   const char *str  = path_exists ? config_path :
      path_get(RARCH_PATH_CONFIG);

   if (path_exists && config_save_file(config_path))
   {
      snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_SAVED_NEW_CONFIG_TO),
            config_path);

      strlcpy(log, "[config] ", sizeof(log));
      strlcat(log, s, sizeof(log));
      RARCH_LOG("%s\n", log);
      return true;
   }

   if (!string_is_empty(str))
   {
      snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_SAVING_CONFIG_TO),
            str);

      strlcpy(log, "[config] ", sizeof(log));
      strlcat(log, s, sizeof(log));
      RARCH_ERR("%s\n", log);
   }

   return false;
}

/**
 * command_event_save_core_config:
 *
 * Saves a new (core) configuration to a file. Filename is based
 * on heuristics to avoid typing.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
static bool command_event_save_core_config(const char *dir_menu_config)
{
   char msg[128];
   bool found_path                 = false;
   bool overrides_active           = false;
   const char *core_path           = NULL;
   char *config_name               = NULL;
   char *config_path               = NULL;
   char *config_dir                = NULL;
   size_t config_size              = PATH_MAX_LENGTH * sizeof(char);
   struct rarch_state     *p_rarch = &rarch_st;

   msg[0]                          = '\0';

   if (!string_is_empty(dir_menu_config))
      config_dir = strdup(dir_menu_config);
   else if (!path_is_empty(RARCH_PATH_CONFIG)) /* Fallback */
   {
      config_dir                   = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
      config_dir[0]                = '\0';
      fill_pathname_basedir(config_dir, path_get(RARCH_PATH_CONFIG),
            config_size);
   }

   if (string_is_empty(config_dir))
   {
      runloop_msg_queue_push(msg_hash_to_str(MSG_CONFIG_DIRECTORY_NOT_SET), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_ERR("[config] %s\n", msg_hash_to_str(MSG_CONFIG_DIRECTORY_NOT_SET));
      free (config_dir);
      return false;
   }

   core_path                       = path_get(RARCH_PATH_CORE);
   config_name                     = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   config_path                     = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   config_name[0]                  = '\0';
   config_path[0]                  = '\0';

   /* Infer file name based on libretro core. */
   if (path_is_valid(core_path))
   {
      unsigned i;
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_USING_CORE_NAME_FOR_NEW_CONFIG));

      /* In case of collision, find an alternative name. */
      for (i = 0; i < 16; i++)
      {
         char tmp[64]   = {0};

         fill_pathname_base_noext(
               config_name,
               core_path,
               config_size);

         fill_pathname_join(config_path, config_dir, config_name,
               config_size);

         if (i)
            snprintf(tmp, sizeof(tmp), "-%u", i);

         strlcat(tmp, ".cfg", sizeof(tmp));
         strlcat(config_path, tmp, config_size);

         if (!path_is_valid(config_path))
         {
            found_path = true;
            break;
         }
      }
   }

   if (!found_path)
   {
      /* Fallback to system time... */
      RARCH_WARN("[config] %s\n",
            msg_hash_to_str(MSG_CANNOT_INFER_NEW_CONFIG_PATH));
      fill_dated_filename(config_name, ".cfg", config_size);
      fill_pathname_join(config_path, config_dir, config_name,
            config_size);
   }

   if (p_rarch->runloop_overrides_active)
   {
      /* Overrides block config file saving,
       * make it appear as overrides weren't enabled
       * for a manual save. */
      p_rarch->runloop_overrides_active = false;
      overrides_active                  = true;
   }

#ifdef HAVE_CONFIGFILE
   command_event_save_config(config_path, msg, sizeof(msg));
#endif

   if (!string_is_empty(msg))
      runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   p_rarch->runloop_overrides_active = overrides_active;

   free(config_dir);
   free(config_name);
   free(config_path);
   return true;
}

/**
 * event_save_current_config:
 *
 * Saves current configuration file to disk, and (optionally)
 * autosave state.
 **/
static void command_event_save_current_config(enum override_type type)
{
   char msg[128];
   struct rarch_state *p_rarch = &rarch_st;

   msg[0] = '\0';

   switch (type)
   {
      case OVERRIDE_NONE:
         if (path_is_empty(RARCH_PATH_CONFIG))
            strlcpy(msg, "[config] Config directory not set, cannot save configuration.",
                  sizeof(msg));
         else
            command_event_save_config(path_get(RARCH_PATH_CONFIG), msg, sizeof(msg));
         break;
      case OVERRIDE_GAME:
      case OVERRIDE_CORE:
      case OVERRIDE_CONTENT_DIR:
         if (config_save_overrides(type, &p_rarch->runloop_system))
         {
            strlcpy(msg, msg_hash_to_str(MSG_OVERRIDES_SAVED_SUCCESSFULLY), sizeof(msg));
            RARCH_LOG("[config] [overrides] %s\n", msg);

            /* set overrides to active so the original config can be
               restored after closing content */
            p_rarch->runloop_overrides_active = true;
         }
         else
         {
            strlcpy(msg, msg_hash_to_str(MSG_OVERRIDES_ERROR_SAVING), sizeof(msg));
            RARCH_ERR("[config] [overrides] %s\n", msg);
         }
         break;
   }

   if (!string_is_empty(msg))
      runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}
#endif

static void command_event_undo_save_state(char *s, size_t len)
{
   if (content_undo_save_buf_is_empty())
   {
      strlcpy(s,
         msg_hash_to_str(MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET), len);
      return;
   }

   if (!content_undo_save_state())
   {
      strlcpy(s,
         msg_hash_to_str(MSG_FAILED_TO_UNDO_SAVE_STATE), len);
      return;
   }

   strlcpy(s,
         msg_hash_to_str(MSG_UNDOING_SAVE_STATE), len);
}

static void command_event_undo_load_state(char *s, size_t len)
{

   if (content_undo_load_buf_is_empty())
   {
      strlcpy(s,
         msg_hash_to_str(MSG_NO_STATE_HAS_BEEN_LOADED_YET),
         len);
      return;
   }

   if (!content_undo_load_state())
   {
      snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_TO_UNDO_LOAD_STATE),
            "RAM");
      return;
   }

#ifdef HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, NULL);
#endif

   strlcpy(s,
         msg_hash_to_str(MSG_UNDID_LOAD_STATE), len);
}

static bool command_event_main_state(unsigned cmd)
{
   retro_ctx_size_info_t info;
   char msg[128];
   size_t state_path_size      = 16384 * sizeof(char);
   char *state_path            = (char*)malloc(state_path_size);
   struct rarch_state *p_rarch = &rarch_st;
   const global_t *global      = &p_rarch->g_extern;
   settings_t *settings        = p_rarch->configuration_settings;
   bool ret                    = false;
   bool push_msg               = true;

   state_path[0] = msg[0]      = '\0';

   if (global)
   {
      int state_slot             = settings->ints.state_slot;
      const char *name_savestate = global->name.savestate;

      if (state_slot > 0)
         snprintf(state_path, state_path_size, "%s%d",
               name_savestate, state_slot);
      else if (state_slot < 0)
         fill_pathname_join_delim(state_path,
               name_savestate, "auto", '.', state_path_size);
      else
         strlcpy(state_path, name_savestate, state_path_size);
   }

   core_serialize_size(&info);

   if (info.size)
   {
      switch (cmd)
      {
         case CMD_EVENT_SAVE_STATE:
            content_save_state(state_path, true, false);
            {
               bool frame_time_counter_reset_after_save_state = 
                  settings->bools.frame_time_counter_reset_after_save_state;
               if (frame_time_counter_reset_after_save_state)
                  p_rarch->video_driver_frame_time_count = 0;
            }
            ret      = true;
            push_msg = false;
            break;
         case CMD_EVENT_LOAD_STATE:
            if (content_load_state(state_path, false, false))
            {
#ifdef HAVE_CHEEVOS
               if (rcheevos_hardcore_active)
                  rcheevos_state_loaded_flag = true;
#endif
               ret = true;
#ifdef HAVE_NETWORKING
               netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, NULL);
#endif
               {
                  bool frame_time_counter_reset_after_load_state = 
                     settings->bools.frame_time_counter_reset_after_load_state;
                  if (frame_time_counter_reset_after_load_state)
                     p_rarch->video_driver_frame_time_count = 0;
               }
            }
            push_msg = false;
            break;
         case CMD_EVENT_UNDO_LOAD_STATE:
            command_event_undo_load_state(msg, sizeof(msg));
            ret = true;
            break;
         case CMD_EVENT_UNDO_SAVE_STATE:
            command_event_undo_save_state(msg, sizeof(msg));
            ret = true;
            break;
      }
   }
   else
      strlcpy(msg, msg_hash_to_str(
               MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES), sizeof(msg));

   if (push_msg)
      runloop_msg_queue_push(msg, 2, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s\n", msg);

   free(state_path);
   return ret;
}

static bool command_event_resize_windowed_scale(void)
{
   unsigned                idx = 0;
   struct rarch_state *p_rarch = &rarch_st;
   settings_t      *settings   = p_rarch->configuration_settings;
   unsigned      window_scale  = p_rarch->runloop_pending_windowed_scale;
   bool      video_fullscreen  = settings->bools.video_fullscreen;

   if (window_scale == 0)
      return false;

   configuration_set_float(settings, settings->floats.video_scale, (float)window_scale);

   if (!video_fullscreen)
      command_event(CMD_EVENT_REINIT, NULL);

   rarch_ctl(RARCH_CTL_SET_WINDOWED_SCALE, &idx);

   return true;
}

static void command_event_reinit(const int flags)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
#ifdef HAVE_MENU
   bool video_fullscreen       = settings->bools.video_fullscreen;
   bool adaptive_vsync         = settings->bools.video_adaptive_vsync;
   unsigned swap_interval      = settings->uints.video_swap_interval;
#endif

   video_driver_reinit(flags);
   /* Poll input to avoid possibly stale data to corrupt things. */
   if (p_rarch->current_input && p_rarch->current_input->poll)
      p_rarch->current_input->poll(p_rarch->current_input_data);
   command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, (void*)(intptr_t)-1);

#ifdef HAVE_MENU
   gfx_display_set_framebuffer_dirty_flag();
   if (video_fullscreen)
      video_driver_hide_mouse();
   if (p_rarch->menu_driver_alive && p_rarch->current_video->set_nonblock_state)
      p_rarch->current_video->set_nonblock_state(
            p_rarch->video_driver_data, false,
            video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC) &&
            adaptive_vsync,
            swap_interval);
#endif
}

static void retroarch_pause_checks(void)
{
#ifdef HAVE_DISCORD
   discord_userdata_t userdata;
#endif
   struct rarch_state
      *p_rarch                    = &rarch_st;
   bool is_paused                 = p_rarch->runloop_paused;
   bool is_idle                   = p_rarch->runloop_idle;
#if defined(HAVE_GFX_WIDGETS)
   bool widgets_active            = gfx_widgets_active();

   if (widgets_active)
      p_rarch->gfx_widgets_paused = is_paused;
#endif

   if (is_paused)
   {
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_PAUSED));

#if defined(HAVE_GFX_WIDGETS)
      if (!widgets_active)
#endif
         runloop_msg_queue_push(msg_hash_to_str(MSG_PAUSED), 1,
               1, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);


      if (!is_idle)
         video_driver_cached_frame();

#ifdef HAVE_DISCORD
      userdata.status = DISCORD_PRESENCE_GAME_PAUSED;
      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
#endif
   }
   else
   {
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_UNPAUSED));
   }

#if defined(HAVE_TRANSLATE) && defined(HAVE_GFX_WIDGETS)
   if (gfx_widgets_ai_service_overlay_get_state() == 1)
      gfx_widgets_ai_service_overlay_unload();
#endif
}

static void retroarch_frame_time_free(void)
{
   struct rarch_state       *p_rarch = &rarch_st;

   memset(&p_rarch->runloop_frame_time, 0,
         sizeof(struct retro_frame_time_callback));
   p_rarch->runloop_frame_time_last  = 0;
   p_rarch->runloop_max_frames       = 0;
}

static void retroarch_system_info_free(void)
{
   struct rarch_state            *p_rarch = &rarch_st;
   rarch_system_info_t        *sys_info   = &p_rarch->runloop_system;

   if (sys_info->subsystem.data)
      free(sys_info->subsystem.data);
   sys_info->subsystem.data                           = NULL;
   sys_info->subsystem.size                           = 0;

   if (sys_info->ports.data)
      free(sys_info->ports.data);
   sys_info->ports.data                               = NULL;
   sys_info->ports.size                               = 0;

   if (sys_info->mmaps.descriptors)
      free((void *)sys_info->mmaps.descriptors);
   sys_info->mmaps.descriptors                        = NULL;
   sys_info->mmaps.num_descriptors                    = 0;

   p_rarch->runloop_key_event                         = NULL;
   p_rarch->runloop_frontend_key_event                = NULL;

   p_rarch->audio_callback.callback                   = NULL;
   p_rarch->audio_callback.set_state                  = NULL;

   sys_info->info.library_name                        = NULL;
   sys_info->info.library_version                     = NULL;
   sys_info->info.valid_extensions                    = NULL;
   sys_info->info.need_fullpath                       = false;
   sys_info->info.block_extract                       = false;

   memset(&p_rarch->runloop_system, 0, sizeof(rarch_system_info_t));
}

static bool libretro_get_system_info(const char *path,
      struct retro_system_info *info, bool *load_no_content);

static bool input_driver_grab_mouse(void)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (!p_rarch->current_input || !p_rarch->current_input->grab_mouse)
      return false;

   p_rarch->current_input->grab_mouse(p_rarch->current_input_data, true);
   return true;
}

static bool input_driver_ungrab_mouse(void)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (!p_rarch->current_input || !p_rarch->current_input->grab_mouse)
      return false;

   p_rarch->current_input->grab_mouse(p_rarch->current_input_data, false);
   return true;
}


/**
 * command_event:
 * @cmd                  : Event command index.
 *
 * Performs program event command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool command_event(enum event_command cmd, void *data)
{
   bool boolean                = false;
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;

   switch (cmd)
   {
      case CMD_EVENT_SAVE_FILES:
         event_save_files(p_rarch->rarch_use_sram);
         break;
      case CMD_EVENT_OVERLAY_DEINIT:
#ifdef HAVE_OVERLAY
         retroarch_overlay_deinit();
#endif
#if defined(HAVE_TRANSLATE) && defined(HAVE_GFX_WIDGETS)
         if (gfx_widgets_ai_service_overlay_get_state() != 0)
         {
            /* Because the overlay is a display widget, 
             * it's going to be written
             * over the menu, so we unset it here. */
            gfx_widgets_ai_service_overlay_unload();
         }
#endif
         break;
      case CMD_EVENT_OVERLAY_INIT:
#ifdef HAVE_OVERLAY
         retroarch_overlay_init();
#endif
         break;
      case CMD_EVENT_CHEAT_INDEX_PLUS:
         cheat_manager_index_next();
         break;
      case CMD_EVENT_CHEAT_INDEX_MINUS:
         cheat_manager_index_prev();
         break;
      case CMD_EVENT_CHEAT_TOGGLE:
         cheat_manager_toggle();
         break;
      case CMD_EVENT_SHADER_NEXT:
         dir_check_shader(true, false);
         break;
      case CMD_EVENT_SHADER_PREV:
         dir_check_shader(false, true);
         break;
      case CMD_EVENT_BSV_RECORDING_TOGGLE:
         if (!recording_is_enabled())
            command_event(CMD_EVENT_RECORD_INIT, NULL);
         else
            command_event(CMD_EVENT_RECORD_DEINIT, NULL);
         bsv_movie_check();
         break;
      case CMD_EVENT_AI_SERVICE_TOGGLE:
      {
#ifdef HAVE_TRANSLATE
         bool ai_service_pause     = settings->bools.ai_service_pause;

         if (!settings->bools.ai_service_enable)
            break;

         if (ai_service_pause)
         {
            /* pause on call, unpause on second press. */
            if (!p_rarch->runloop_paused)
            {
               command_event(CMD_EVENT_PAUSE, NULL);
               command_event(CMD_EVENT_AI_SERVICE_CALL, NULL);
            }
            else
            {
#ifdef HAVE_ACCESSIBILITY
               if (is_accessibility_enabled())
                   accessibility_speak_priority((char*) msg_hash_to_str(MSG_UNPAUSED), 10);
#endif
               command_event(CMD_EVENT_UNPAUSE, NULL);
            }
         }
         else
         {
           /* Don't pause - useful for Text-To-Speech since
            * the audio can't currently play while paused.
            * Also useful for cases when users don't want the
            * core's sound to stop while translating. 
            *
            * Also, this mode is required for "auto" translation
            * packages, since you don't want to pause for that.   
            */ 
            if (p_rarch->ai_service_auto == 2)
            {
               /* Auto mode was turned on, but we pressed the
                * toggle button, so turn it off now. */
               p_rarch->ai_service_auto = 0;
#ifdef HAVE_MENU_WIDGETS
               gfx_widgets_ai_service_overlay_unload();
#endif
            }
            else
               command_event(CMD_EVENT_AI_SERVICE_CALL, NULL);
         }
#endif
         break;
      }
      case CMD_EVENT_STREAMING_TOGGLE:
         if (streaming_is_enabled())
            command_event(CMD_EVENT_RECORD_DEINIT, NULL);
         else
         {
            streaming_set_state(true);
            command_event(CMD_EVENT_RECORD_INIT, NULL);
         }
         break;
      case CMD_EVENT_RECORDING_TOGGLE:
         if (recording_is_enabled())
            command_event(CMD_EVENT_RECORD_DEINIT, NULL);
         else
            command_event(CMD_EVENT_RECORD_INIT, NULL);
         break;
      case CMD_EVENT_OSK_TOGGLE:
         if (p_rarch->input_driver_keyboard_linefeed_enable)
            p_rarch->input_driver_keyboard_linefeed_enable = false;
         else
            p_rarch->input_driver_keyboard_linefeed_enable = true;
         break;
      case CMD_EVENT_SET_PER_GAME_RESOLUTION:
#if defined(GEKKO)
         {
            unsigned width = 0, height = 0;

            command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);

            if (video_driver_get_video_output_size(&width, &height))
            {
               char msg[128] = {0};

               video_driver_set_video_mode(width, height, true);

               if (width == 0 || height == 0)
                  snprintf(msg, sizeof(msg), "%s: DEFAULT",
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION));
               else
                  snprintf(msg, sizeof(msg),"%s: %dx%d",
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION),
                        width, height);
               runloop_msg_queue_push(msg, 1, 100, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
         }
#endif
         break;
      case CMD_EVENT_LOAD_CORE_PERSIST:
         {
            core_info_ctx_find_t info_find;
            rarch_system_info_t *system_info = &p_rarch->runloop_system;
            struct retro_system_info *system = &system_info->info;
            const char *core_path            = path_get(RARCH_PATH_CORE);

#if defined(HAVE_DYNAMIC)
            if (string_is_empty(core_path))
               return false;
#endif

            if (!libretro_get_system_info(
                     core_path,
                     system,
                     &system_info->load_no_content))
               return false;
            info_find.path = core_path;

            if (!core_info_load(&info_find))
            {
#ifdef HAVE_DYNAMIC
               return false;
#endif
            }
         }
         break;
      case CMD_EVENT_LOAD_CORE:
         {
            bool success            = false;
            subsystem_current_count = 0;
            content_clear_subsystem();
            success = command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
            (void)success;

#ifndef HAVE_DYNAMIC
            command_event(CMD_EVENT_QUIT, NULL);
#else
            if (!success)
               return false;
#endif
            break;
         }
      case CMD_EVENT_LOAD_STATE:
         /* Immutable - disallow savestate load when
          * we absolutely cannot change game state. */
         if (p_rarch->bsv_movie_state_handle)
            return false;

#ifdef HAVE_CHEEVOS
         if (rcheevos_hardcore_active)
            return false;
#endif
         if (!command_event_main_state(cmd))
            return false;
         break;
      case CMD_EVENT_UNDO_LOAD_STATE:
         if (!command_event_main_state(cmd))
            return false;
         break;
      case CMD_EVENT_UNDO_SAVE_STATE:
         if (!command_event_main_state(cmd))
            return false;
         break;
      case CMD_EVENT_RESIZE_WINDOWED_SCALE:
         if (!command_event_resize_windowed_scale())
            return false;
         break;
      case CMD_EVENT_MENU_TOGGLE:
#ifdef HAVE_MENU
         if (p_rarch->menu_driver_alive)
            retroarch_menu_running_finished(false);
         else
            retroarch_menu_running();
#endif
         break;
      case CMD_EVENT_RESET:
#ifdef HAVE_CHEEVOS
         rcheevos_state_loaded_flag = false;
         rcheevos_hardcore_paused = false;
#endif
         RARCH_LOG("%s.\n", msg_hash_to_str(MSG_RESET));
         runloop_msg_queue_push(msg_hash_to_str(MSG_RESET), 1, 120, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         core_reset();
#ifdef HAVE_CHEEVOS
         rcheevos_reset_game();
#endif
#if HAVE_NETWORKING
         netplay_driver_ctl(RARCH_NETPLAY_CTL_RESET, NULL);
#endif
         return false;
      case CMD_EVENT_SAVE_STATE:
         {
            bool savestate_auto_index = settings->bools.savestate_auto_index;
            int state_slot            = settings->ints.state_slot;

            if (savestate_auto_index)
            {
               int new_state_slot = state_slot + 1;
               configuration_set_int(settings, settings->ints.state_slot, new_state_slot);
            }
         }
         if (!command_event_main_state(cmd))
            return false;
         break;
      case CMD_EVENT_SAVE_STATE_DECREMENT:
         {
            int state_slot            = settings->ints.state_slot;

            /* Slot -1 is (auto) slot. */
            if (state_slot >= 0)
            {
               int new_state_slot = state_slot - 1;
               configuration_set_int(settings, settings->ints.state_slot, new_state_slot);
            }
         }
         break;
      case CMD_EVENT_SAVE_STATE_INCREMENT:
         {
            int new_state_slot        = settings->ints.state_slot + 1;
            configuration_set_int(settings, settings->ints.state_slot, new_state_slot);
         }
         break;
      case CMD_EVENT_TAKE_SCREENSHOT:
         {
            const char *dir_screenshot = settings->paths.directory_screenshot;
            if (!take_screenshot(dir_screenshot,
                     path_get(RARCH_PATH_BASENAME), false,
                     video_driver_cached_frame_has_valid_framebuffer(), false, true))
               return false;
         }
         break;
      case CMD_EVENT_UNLOAD_CORE:
         {
            bool contentless                = false;
            bool is_inited                  = false;
            content_ctx_info_t content_info = {0};
            rarch_system_info_t *sys_info   = &p_rarch->runloop_system;

            content_get_status(&contentless, &is_inited);

            p_rarch->runloop_core_running   = false;

            /* Save last selected disk index, if required */
            if (sys_info)
               disk_control_save_image_index(&sys_info->disk_control);

            command_event_runtime_log_deinit();
            command_event_save_auto_state();
#ifdef HAVE_CONFIGFILE
            if (p_rarch->runloop_overrides_active)
               command_event_disable_overrides();
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            retroarch_unset_runtime_shader_preset();
#endif

            if (p_rarch->cached_video_driver[0])
            {
               configuration_set_string(settings, 
               settings->arrays.video_driver, p_rarch->cached_video_driver);

               p_rarch->cached_video_driver[0] = 0;
               RARCH_LOG("[Video]: Restored video driver to \"%s\".\n",
                     settings->arrays.video_driver);
            }

#ifdef HAVE_CONFIGFILE
            if (     p_rarch->runloop_remaps_core_active
                  || p_rarch->runloop_remaps_content_dir_active
                  || p_rarch->runloop_remaps_game_active
               )
               input_remapping_set_defaults(true);
#endif

            if (is_inited)
            {
               if (!task_push_start_dummy_core(&content_info))
                  return false;
            }
#ifdef HAVE_DISCORD
            if (discord_is_inited)
            {
               discord_userdata_t userdata;
               userdata.status = DISCORD_PRESENCE_NETPLAY_NETPLAY_STOPPED;
               command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
               userdata.status = DISCORD_PRESENCE_MENU;
               command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
            }
#endif
#ifdef HAVE_DYNAMIC
            path_clear(RARCH_PATH_CORE);
            retroarch_system_info_free();
#endif
            if (is_inited)
            {
               subsystem_current_count = 0;
               content_clear_subsystem();
            }
         }
         break;
      case CMD_EVENT_QUIT:
         if (!retroarch_main_quit())
            return false;
         break;
      case CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE:
#ifdef HAVE_CHEEVOS
         rcheevos_toggle_hardcore_mode();
#endif
         break;
      case CMD_EVENT_REINIT_FROM_TOGGLE:
         p_rarch->rarch_force_fullscreen = false;
         /* this fallthrough is on purpose, it should do
            a CMD_EVENT_REINIT too */
      case CMD_EVENT_REINIT:
         command_event_reinit(data ? *(const int*)data : DRIVERS_CMD_ALL);
         break;
      case CMD_EVENT_CHEATS_APPLY:
         cheat_manager_apply_cheats();
         break;
      case CMD_EVENT_REWIND_DEINIT:
#ifdef HAVE_CHEEVOS
         if (rcheevos_hardcore_active)
            return false;
#endif
         state_manager_event_deinit();
         break;
      case CMD_EVENT_REWIND_INIT:
         {
            bool rewind_enable        = settings->bools.rewind_enable;
            unsigned rewind_buf_size  = settings->sizes.rewind_buffer_size;
#ifdef HAVE_CHEEVOS
            if (rcheevos_hardcore_active)
               return false;
#endif
            if (rewind_enable)
            {
#ifdef HAVE_NETWORKING
               /* Only enable state manager if netplay is not underway
                  TODO/FIXME: Add a setting for these tweaks */
               if (!netplay_driver_ctl(
                        RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
#endif
               {
                  state_manager_event_init((unsigned)rewind_buf_size);
               }
            }
         }
         break;
      case CMD_EVENT_REWIND_TOGGLE:
         {
            bool rewind_enable        = settings->bools.rewind_enable;
            if (rewind_enable)
               command_event(CMD_EVENT_REWIND_INIT, NULL);
            else
               command_event(CMD_EVENT_REWIND_DEINIT, NULL);
         }
         break;
      case CMD_EVENT_AUTOSAVE_INIT:
#ifdef HAVE_THREADS
         retroarch_autosave_deinit();
         {
#ifdef HAVE_NETWORKING
            unsigned autosave_interval = 
               settings->uints.autosave_interval;
            /* Only enable state manager if netplay is not underway
               TODO/FIXME: Add a setting for these tweaks */
            if (      (autosave_interval != 0)
                  && !netplay_driver_ctl(
                     RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
#endif
               p_rarch->runloop_autosave = autosave_init();
         }
#endif
         break;
      case CMD_EVENT_AUDIO_STOP:
         midi_driver_set_all_sounds_off();
         if (!audio_driver_stop())
            return false;
         break;
      case CMD_EVENT_AUDIO_START:
         if (!audio_driver_start(p_rarch->runloop_shutdown_initiated))
            return false;
         break;
      case CMD_EVENT_AUDIO_MUTE_TOGGLE:
         {
            bool audio_mute_enable             = 
               *(audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE));
            const char *msg                    = !audio_mute_enable ?
               msg_hash_to_str(MSG_AUDIO_MUTED):
               msg_hash_to_str(MSG_AUDIO_UNMUTED);

            p_rarch->audio_driver_mute_enable  = 
               !p_rarch->audio_driver_mute_enable;

#if defined(HAVE_GFX_WIDGETS)
            if (gfx_widgets_active())
               gfx_widget_volume_update_and_show(
                     settings->floats.audio_volume,
                     p_rarch->audio_driver_mute_enable);
            else
#endif
               runloop_msg_queue_push(msg, 1, 180, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
      case CMD_EVENT_SEND_DEBUG_INFO:
         break;
      case CMD_EVENT_FPS_TOGGLE:
         settings->bools.video_fps_show = !(settings->bools.video_fps_show);
         break;
      case CMD_EVENT_OVERLAY_NEXT:
         /* Switch to the next available overlay screen. */
#ifdef HAVE_OVERLAY
         {
            bool *check_rotation           = (bool*)data;
            bool inp_overlay_auto_rotate   = settings->bools.input_overlay_auto_rotate;
            float input_overlay_opacity    = settings->floats.input_overlay_opacity;
            if (!p_rarch->overlay_ptr)
               return false;

            p_rarch->overlay_ptr->index    = p_rarch->overlay_ptr->next_index;
            p_rarch->overlay_ptr->active   = &p_rarch->overlay_ptr->overlays[
               p_rarch->overlay_ptr->index];

            input_overlay_load_active(p_rarch->overlay_ptr, input_overlay_opacity);

            p_rarch->overlay_ptr->blocked    = true;
            p_rarch->overlay_ptr->next_index = (unsigned)((p_rarch->overlay_ptr->index + 1) % p_rarch->overlay_ptr->size);

            /* Check orientation, if required */
            if (inp_overlay_auto_rotate)
               if (check_rotation)
                  if (*check_rotation)
                     input_overlay_auto_rotate_(p_rarch->overlay_ptr);
         }
#endif
         break;
      case CMD_EVENT_DSP_FILTER_INIT:
         {
            const char *path_audio_dsp_plugin = settings->paths.path_audio_dsp_plugin;
            audio_driver_dsp_filter_free();
            if (string_is_empty(path_audio_dsp_plugin))
               break;
            if (!audio_driver_dsp_filter_init(path_audio_dsp_plugin))
            {
               RARCH_ERR("[DSP]: Failed to initialize DSP filter \"%s\".\n",
                     path_audio_dsp_plugin);
            }
         }
         break;
      case CMD_EVENT_RECORD_DEINIT:
         p_rarch->recording_enable = false;
         streaming_set_state(false);
         if (!recording_deinit())
            return false;
         break;
      case CMD_EVENT_RECORD_INIT:
         p_rarch->recording_enable = true;
         if (!recording_init())
         {
            command_event(CMD_EVENT_RECORD_DEINIT, NULL);
            return false;
         }
         break;
      case CMD_EVENT_HISTORY_DEINIT:
         if (g_defaults.content_history)
         {
            bool playlist_use_old_format = settings->bools.playlist_use_old_format;
            bool playlist_compression    = settings->bools.playlist_compression;
            playlist_write_file(g_defaults.content_history,
                  playlist_use_old_format, playlist_compression);
            playlist_free(g_defaults.content_history);
         }
         g_defaults.content_history = NULL;

         if (g_defaults.music_history)
         {
            bool playlist_use_old_format = settings->bools.playlist_use_old_format;
            bool playlist_compression    = settings->bools.playlist_compression;
            playlist_write_file(g_defaults.music_history,
                  playlist_use_old_format, playlist_compression);
            playlist_free(g_defaults.music_history);
         }
         g_defaults.music_history = NULL;

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
         if (g_defaults.video_history)
         {
            bool playlist_use_old_format = settings->bools.playlist_use_old_format;
            bool playlist_compression    = settings->bools.playlist_compression;
            playlist_write_file(g_defaults.video_history,
                  playlist_use_old_format, playlist_compression);
            playlist_free(g_defaults.video_history);
         }
         g_defaults.video_history = NULL;
#endif

#ifdef HAVE_IMAGEVIEWER
         if (g_defaults.image_history)
         {
            bool playlist_use_old_format = settings->bools.playlist_use_old_format;
            bool playlist_compression    = settings->bools.playlist_compression;
            playlist_write_file(g_defaults.image_history,
                  playlist_use_old_format, playlist_compression);
            playlist_free(g_defaults.image_history);
         }
         g_defaults.image_history = NULL;
#endif
         break;
      case CMD_EVENT_HISTORY_INIT:
         {
            unsigned content_history_size    = settings->uints.content_history_size;
            bool history_list_enable         = settings->bools.history_list_enable;
            const char *path_content_history = settings->paths.path_content_history;
            const char *path_content_music_history = settings->paths.path_content_music_history;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            const char *path_content_video_history = settings->paths.path_content_video_history;
#endif
#ifdef HAVE_IMAGEVIEWER
            const char *path_content_image_history = settings->paths.path_content_image_history;
#endif

            command_event(CMD_EVENT_HISTORY_DEINIT, NULL);

            if (!history_list_enable)
               return false;

            /* Note: Sorting is disabled by default for
             * all content history playlists */

            RARCH_LOG("%s: [%s].\n",
                  msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
                  path_content_history);
            g_defaults.content_history = playlist_init(
                  path_content_history,
                  content_history_size);
            playlist_set_sort_mode(
                  g_defaults.content_history, PLAYLIST_SORT_MODE_OFF);

            RARCH_LOG("%s: [%s].\n",
                  msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
                  path_content_music_history);
            g_defaults.music_history = playlist_init(
                  path_content_music_history,
                  content_history_size);
            playlist_set_sort_mode(
                  g_defaults.music_history, PLAYLIST_SORT_MODE_OFF);

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            RARCH_LOG("%s: [%s].\n",
                  msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
                  path_content_video_history);
            g_defaults.video_history = playlist_init(
                  path_content_video_history,
                  content_history_size);
            playlist_set_sort_mode(
                  g_defaults.video_history, PLAYLIST_SORT_MODE_OFF);
#endif

#ifdef HAVE_IMAGEVIEWER
            RARCH_LOG("%s: [%s].\n",
                  msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
                  path_content_image_history);
            g_defaults.image_history = playlist_init(
                  path_content_image_history,
                  content_history_size);
            playlist_set_sort_mode(
                  g_defaults.image_history, PLAYLIST_SORT_MODE_OFF);
#endif
         }
         break;
      case CMD_EVENT_CORE_INFO_DEINIT:
         core_info_deinit_list();
         core_info_free_current_core();
         break;
      case CMD_EVENT_CORE_INFO_INIT:
         {
            char ext_name[255];
            const char *dir_libretro       = settings->paths.directory_libretro;
            const char *path_libretro_info = settings->paths.path_libretro_info;
            bool show_hidden_files         = settings->bools.show_hidden_files;

            ext_name[0]                    = '\0';

            command_event(CMD_EVENT_CORE_INFO_DEINIT, NULL);

            if (!frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
               return false;

            if (!string_is_empty(dir_libretro))
               core_info_init_list(path_libretro_info,
                     dir_libretro,
                     ext_name,
                     show_hidden_files
                     );
         }
         break;
      case CMD_EVENT_CORE_DEINIT:
         {
            struct retro_hw_render_callback *hwr = NULL;
            rarch_system_info_t *sys_info        = &p_rarch->runloop_system;

            /* Save last selected disk index, if required */
            if (sys_info)
               disk_control_save_image_index(&sys_info->disk_control);

            command_event_runtime_log_deinit();
            content_reset_savestate_backups();
            hwr = video_driver_get_hw_context_internal();
            command_event_deinit_core(true);

            if (hwr)
               memset(hwr, 0, sizeof(*hwr));

            break;
         }
      case CMD_EVENT_CORE_INIT:
         content_reset_savestate_backups();
         {
            enum rarch_core_type *type = (enum rarch_core_type*)data;
            if (!type || !command_event_init_core(*type))
               return false;
         }
         break;
      case CMD_EVENT_VIDEO_APPLY_STATE_CHANGES:
         video_driver_apply_state_changes();
         break;
      case CMD_EVENT_VIDEO_SET_BLOCKING_STATE:
         {
            bool adaptive_vsync       = settings->bools.video_adaptive_vsync;
            unsigned swap_interval    = settings->uints.video_swap_interval;

            if (p_rarch->current_video->set_nonblock_state)
               p_rarch->current_video->set_nonblock_state(
                     p_rarch->video_driver_data, false,
                     video_driver_test_all_flags(
                        GFX_CTX_FLAGS_ADAPTIVE_VSYNC) &&
                     adaptive_vsync, swap_interval);
         }
         break;
      case CMD_EVENT_VIDEO_SET_ASPECT_RATIO:
         video_driver_set_aspect_ratio();
         break;
      case CMD_EVENT_OVERLAY_SET_SCALE_FACTOR:
#ifdef HAVE_OVERLAY
         {
            float input_overlay_scale   = settings->floats.input_overlay_scale;
            input_overlay_set_scale_factor(p_rarch->overlay_ptr, input_overlay_scale);
         }
#endif
         break;
      case CMD_EVENT_OVERLAY_SET_ALPHA_MOD:
         /* Sets a modulating factor for alpha channel. Default is 1.0.
          * The alpha factor is applied for all overlays. */
#ifdef HAVE_OVERLAY
         {
            float input_overlay_opacity = settings->floats.input_overlay_opacity;
            input_overlay_set_alpha_mod(p_rarch->overlay_ptr, input_overlay_opacity);
         }
#endif
         break;
      case CMD_EVENT_AUDIO_REINIT:
         driver_uninit(DRIVER_AUDIO_MASK);
         drivers_init(DRIVER_AUDIO_MASK);
         break;
      case CMD_EVENT_SHUTDOWN:
#if defined(__linux__) && !defined(ANDROID)
         runloop_msg_queue_push(msg_hash_to_str(MSG_VALUE_SHUTTING_DOWN), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
         command_event(CMD_EVENT_QUIT, NULL);
         system("shutdown -P now");
#endif
         break;
      case CMD_EVENT_REBOOT:
#if defined(__linux__) && !defined(ANDROID)
         runloop_msg_queue_push(msg_hash_to_str(MSG_VALUE_REBOOTING), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
         command_event(CMD_EVENT_QUIT, NULL);
         system("shutdown -r now");
#endif
         break;
      case CMD_EVENT_RESUME:
         retroarch_menu_running_finished(false);
         if (p_rarch->main_ui_companion_is_on_foreground)
            ui_companion_driver_toggle(false);
         break;
      case CMD_EVENT_ADD_TO_FAVORITES:
         {
            struct string_list *str_list = (struct string_list*)data;

            /* Check whether favourties playlist is at capacity */
            if (playlist_size(g_defaults.content_favorites) >=
                playlist_capacity(g_defaults.content_favorites))
            {
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_ADD_TO_FAVORITES_FAILED), 1, 180, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
               return false;
            }

            if (str_list)
            {
               if (str_list->size >= 6)
               {
                  struct playlist_entry entry       = {0};
                  bool playlist_use_old_format      = settings->bools.playlist_use_old_format;
                  bool playlist_compression         = settings->bools.playlist_compression;
                  bool playlist_sort_alphabetical   = settings->bools.playlist_sort_alphabetical;
                  bool playlist_fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;

                  entry.path      = str_list->elems[0].data; /* content_path */
                  entry.label     = str_list->elems[1].data; /* content_label */
                  entry.core_path = str_list->elems[2].data; /* core_path */
                  entry.core_name = str_list->elems[3].data; /* core_name */
                  entry.crc32     = str_list->elems[4].data; /* crc32 */
                  entry.db_name   = str_list->elems[5].data; /* db_name */

                  /* Write playlist entry */
                  if (playlist_push(g_defaults.content_favorites, &entry,
                           playlist_fuzzy_archive_match))
                  {
                     enum playlist_sort_mode current_sort_mode =
                           playlist_get_sort_mode(g_defaults.content_favorites);

                     /* New addition - need to resort if option is enabled */
                     if ((playlist_sort_alphabetical && (current_sort_mode == PLAYLIST_SORT_MODE_DEFAULT)) ||
                         (current_sort_mode == PLAYLIST_SORT_MODE_ALPHABETICAL))
                        playlist_qsort(g_defaults.content_favorites);

                     playlist_write_file(g_defaults.content_favorites,
                           playlist_use_old_format, playlist_compression);
                     runloop_msg_queue_push(msg_hash_to_str(MSG_ADDED_TO_FAVORITES), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  }
               }
            }

            break;
         }
      case CMD_EVENT_RESET_CORE_ASSOCIATION:
         {
            const char *core_name          = "DETECT";
            const char *core_path          = "DETECT";
            size_t *playlist_index         = (size_t*)data;
            struct playlist_entry entry    = {0};
            bool playlist_use_old_format   = settings->bools.playlist_use_old_format;
            bool playlist_compression      = settings->bools.playlist_compression;

            /* the update function reads our entry as const,
             * so these casts are safe */
            entry.core_path                = (char*)core_path;
            entry.core_name                = (char*)core_name;

            command_playlist_update_write(
                  NULL,
                  *playlist_index,
                  &entry,
                  playlist_use_old_format,
                  playlist_compression);

            runloop_msg_queue_push(msg_hash_to_str(MSG_RESET_CORE_ASSOCIATION), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            break;

         }
      case CMD_EVENT_RESTART_RETROARCH:
         if (!frontend_driver_set_fork(FRONTEND_FORK_RESTART))
            return false;
#ifndef HAVE_DYNAMIC
         command_event(CMD_EVENT_QUIT, NULL);
#endif
         break;
      case CMD_EVENT_MENU_RESET_TO_DEFAULT_CONFIG:
         config_set_defaults(&p_rarch->g_extern);
         break;
      case CMD_EVENT_MENU_SAVE_CURRENT_CONFIG:
#ifdef HAVE_CONFIGFILE
         command_event_save_current_config(OVERRIDE_NONE);
#endif
         break;
      case CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
#ifdef HAVE_CONFIGFILE
         command_event_save_current_config(OVERRIDE_CORE);
#endif
         break;
      case CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR:
#ifdef HAVE_CONFIGFILE
         command_event_save_current_config(OVERRIDE_CONTENT_DIR);
#endif
         break;
      case CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
#ifdef HAVE_CONFIGFILE
         command_event_save_current_config(OVERRIDE_GAME);
#endif
         break;
      case CMD_EVENT_MENU_SAVE_CONFIG:
#ifdef HAVE_CONFIGFILE
         if (!command_event_save_core_config(
                  settings->paths.directory_menu_config))
            return false;
#endif
         break;
      case CMD_EVENT_SHADER_PRESET_LOADED:
         ui_companion_event_command(cmd);
         break;
      case CMD_EVENT_SHADERS_APPLY_CHANGES:
#ifdef HAVE_MENU
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         menu_shader_manager_apply_changes(menu_shader_get(),
               settings->paths.directory_video_shader,
               settings->paths.directory_menu_config
               );
#endif
#endif
         ui_companion_event_command(cmd);
         break;
      case CMD_EVENT_PAUSE_TOGGLE:
         boolean        = p_rarch->runloop_paused;
         boolean        = !boolean;

#ifdef HAVE_ACCESSIBILITY
         if (is_accessibility_enabled())
         {
            if (boolean)
               accessibility_speak_priority((char*) msg_hash_to_str(MSG_PAUSED), 10);
            else
               accessibility_speak_priority((char*) msg_hash_to_str(MSG_UNPAUSED), 10);
         }
#endif

         p_rarch->runloop_paused = boolean;
         retroarch_pause_checks();
         break;
      case CMD_EVENT_UNPAUSE:
         boolean                 = false;
         p_rarch->runloop_paused = boolean;
         retroarch_pause_checks();
         break;
      case CMD_EVENT_PAUSE:
         boolean                 = true;
         p_rarch->runloop_paused = boolean;
         retroarch_pause_checks();
         break;
      case CMD_EVENT_MENU_PAUSE_LIBRETRO:
#ifdef HAVE_MENU
         if (p_rarch->menu_driver_alive)
         {
            bool menu_pause_libretro  = settings->bools.menu_pause_libretro;
            if (menu_pause_libretro)
               command_event(CMD_EVENT_AUDIO_STOP, NULL);
            else
               command_event(CMD_EVENT_AUDIO_START, NULL);
         }
         else
         {
            bool menu_pause_libretro  = settings->bools.menu_pause_libretro;
            if (menu_pause_libretro)
               command_event(CMD_EVENT_AUDIO_START, NULL);
         }
#endif
         break;
#ifdef HAVE_NETWORKING
      case CMD_EVENT_NETPLAY_GAME_WATCH:
         netplay_driver_ctl(RARCH_NETPLAY_CTL_GAME_WATCH, NULL);
         break;
      case CMD_EVENT_NETPLAY_DEINIT:
         deinit_netplay();
         break;
      case CMD_EVENT_NETWORK_INIT:
         network_init();
         break;
         /* init netplay manually */
      case CMD_EVENT_NETPLAY_INIT:
         {
            char       *hostname       = (char*)data;
            const char *netplay_server = settings->paths.netplay_server;
            unsigned netplay_port      = settings->uints.netplay_port;

            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

            if (!init_netplay(NULL, hostname ? hostname :
                     netplay_server, netplay_port))
            {
               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
               return false;
            }

            /* Disable rewind & SRAM autosave if it was enabled
             * TODO/FIXME: Add a setting for these tweaks */
            state_manager_event_deinit();
#ifdef HAVE_THREADS
            autosave_deinit();
#endif
         }
         break;
         /* Initialize netplay via lobby when content is loaded */
      case CMD_EVENT_NETPLAY_INIT_DIRECT:
         {
            /* buf is expected to be address|port */
            static struct string_list *hostname = NULL;
            char *buf                           = (char *)data;
            unsigned netplay_port               = settings->uints.netplay_port;

            hostname                            = string_split(buf, "|");

            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

            RARCH_LOG("[Netplay] connecting to %s:%d (direct)\n",
                  hostname->elems[0].data, !string_is_empty(hostname->elems[1].data)
                  ? atoi(hostname->elems[1].data) 
                  : netplay_port);

            if (!init_netplay(NULL, hostname->elems[0].data,
                     !string_is_empty(hostname->elems[1].data)
                     ? atoi(hostname->elems[1].data) 
                     : netplay_port))
            {
               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
               string_list_free(hostname);
               return false;
            }

            string_list_free(hostname);

            /* Disable rewind if it was enabled
               TODO/FIXME: Add a setting for these tweaks */
            state_manager_event_deinit();
#ifdef HAVE_THREADS
            autosave_deinit();
#endif
         }
         break;
         /* init netplay via lobby when content is not loaded */
      case CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED:
         {
            static struct string_list *hostname = NULL;
            /* buf is expected to be address|port */
            char *buf                           = (char *)data;
            unsigned netplay_port               = settings->uints.netplay_port;

            hostname = string_split(buf, "|");

            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

            RARCH_LOG("[Netplay] connecting to %s:%d (deferred)\n",
                  hostname->elems[0].data, !string_is_empty(hostname->elems[1].data)
                  ? atoi(hostname->elems[1].data) 
                  : netplay_port);

            if (!init_netplay_deferred(hostname->elems[0].data,
                     !string_is_empty(hostname->elems[1].data)
                     ? atoi(hostname->elems[1].data) 
                     : netplay_port))
            {
               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
               string_list_free(hostname);
               return false;
            }

            string_list_free(hostname);

            /* Disable rewind if it was enabled
             * TODO/FIXME: Add a setting for these tweaks */
            state_manager_event_deinit();
#ifdef HAVE_THREADS
            autosave_deinit();
#endif
         }
         break;
      case CMD_EVENT_NETPLAY_ENABLE_HOST:
         {
#ifdef HAVE_MENU
            bool contentless  = false;
            bool is_inited    = false;

            content_get_status(&contentless, &is_inited);

            if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
            netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);

            /* If we haven't yet started, this will load on its own */
            if (!is_inited)
            {
               runloop_msg_queue_push(
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED),
                     1, 480, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               return false;
            }

            /* Enable Netplay itself */
            if (!command_event(CMD_EVENT_NETPLAY_INIT, NULL))
               return false;
#endif
            break;
         }
      case CMD_EVENT_NETPLAY_DISCONNECT:
         {
            netplay_driver_ctl(RARCH_NETPLAY_CTL_DISCONNECT, NULL);
            netplay_driver_ctl(RARCH_NETPLAY_CTL_DISABLE, NULL);

            {
               bool rewind_enable                  = settings->bools.rewind_enable;
               unsigned autosave_interval          = settings->uints.autosave_interval;

               /* Re-enable rewind if it was enabled
                * TODO/FIXME: Add a setting for these tweaks */
               if (rewind_enable)
                  command_event(CMD_EVENT_REWIND_INIT, NULL);
               if (autosave_interval != 0)
                  command_event(CMD_EVENT_AUTOSAVE_INIT, NULL);
            }

            break;
         }
      case CMD_EVENT_NETPLAY_HOST_TOGGLE:
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
               netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL))
            command_event(CMD_EVENT_NETPLAY_DISCONNECT, NULL);
         else if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
               !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL) &&
               netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_CONNECTED, NULL))
            command_event(CMD_EVENT_NETPLAY_DISCONNECT, NULL);
         else
            command_event(CMD_EVENT_NETPLAY_ENABLE_HOST, NULL);

         break;
#else
      case CMD_EVENT_NETPLAY_DEINIT:
      case CMD_EVENT_NETWORK_INIT:
      case CMD_EVENT_NETPLAY_INIT:
      case CMD_EVENT_NETPLAY_INIT_DIRECT:
      case CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED:
      case CMD_EVENT_NETPLAY_HOST_TOGGLE:
      case CMD_EVENT_NETPLAY_DISCONNECT:
      case CMD_EVENT_NETPLAY_ENABLE_HOST:
      case CMD_EVENT_NETPLAY_GAME_WATCH:
         return false;
#endif
      case CMD_EVENT_FULLSCREEN_TOGGLE:
         {
            bool *userdata            = (bool*)data;
            bool video_fullscreen     = settings->bools.video_fullscreen;
            bool ra_is_forced_fs      = retroarch_is_forced_fullscreen();
            bool new_fullscreen_state = !video_fullscreen && !ra_is_forced_fs;

            if (!video_driver_has_windowed())
               return false;

            p_rarch->rarch_is_switching_display_mode = true;

            /* we toggled manually, write the new value to settings */
            configuration_set_bool(settings, settings->bools.video_fullscreen,
                  new_fullscreen_state);
            /* Need to grab this setting's value again */
            video_fullscreen = new_fullscreen_state;

            /* we toggled manually, the CLI arg is irrelevant now */
            if (ra_is_forced_fs)
               p_rarch->rarch_force_fullscreen = false;

            /* If we go fullscreen we drop all drivers and
             * reinitialize to be safe. */
            command_event(CMD_EVENT_REINIT, NULL);
            if (video_fullscreen)
               video_driver_hide_mouse();
            else
               video_driver_show_mouse();

            p_rarch->rarch_is_switching_display_mode = false;

            if (userdata && *userdata == true)
               video_driver_cached_frame();
         }
         break;
      case CMD_EVENT_LOG_FILE_DEINIT:
         retro_main_log_file_deinit();
         break;
      case CMD_EVENT_DISK_APPEND_IMAGE:
         {
            const char *path = (const char*)data;
            if (string_is_empty(path))
               return false;
            if (!command_event_disk_control_append_image(path))
               return false;
         }
         break;
      case CMD_EVENT_DISK_EJECT_TOGGLE:
         {
            rarch_system_info_t *sys_info = &p_rarch->runloop_system;
            bool *show_msg                = (bool*)data;

            if (!sys_info)
               return false;

            if (disk_control_enabled(&sys_info->disk_control))
            {
               bool eject   = !disk_control_get_eject_state(&sys_info->disk_control);
               bool verbose = true;
               bool refresh = false;

               if (show_msg)
                  verbose = *show_msg;

               disk_control_set_eject_state(&sys_info->disk_control, eject, verbose);

#if defined(HAVE_MENU)
               /* It is necessary to refresh the disk options
                * menu when toggling the tray state */
               menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
               menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
#endif
            }
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                     1, 120, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
      case CMD_EVENT_DISK_NEXT:
         {
            rarch_system_info_t *sys_info = &p_rarch->runloop_system;
            bool *show_msg                = (bool*)data;

            if (!sys_info)
               return false;

            if (disk_control_enabled(&sys_info->disk_control))
            {
               bool verbose = true;

               if (show_msg)
                  verbose = *show_msg;

               disk_control_set_index_next(&sys_info->disk_control, verbose);
            }
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                     1, 120, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
      case CMD_EVENT_DISK_PREV:
         {
            rarch_system_info_t *sys_info = &p_rarch->runloop_system;
            bool *show_msg                = (bool*)data;

            if (!sys_info)
               return false;

            if (disk_control_enabled(&sys_info->disk_control))
            {
               bool verbose = true;

               if (show_msg)
                  verbose = *show_msg;

               disk_control_set_index_prev(&sys_info->disk_control, verbose);
            }
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                     1, 120, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
      case CMD_EVENT_DISK_INDEX:
         {
            rarch_system_info_t *sys_info = &p_rarch->runloop_system;
            unsigned *index               = (unsigned*)data;

            if (!sys_info || !index)
               return false;

            /* Note: Menu itself provides visual feedback - no
             * need to print info message to screen */
            if (disk_control_enabled(&sys_info->disk_control))
               disk_control_set_index(&sys_info->disk_control, *index, false);
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                     1, 120, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
      case CMD_EVENT_RUMBLE_STOP:
         {
            unsigned i;
            for (i = 0; i < MAX_USERS; i++)
            {
               input_driver_set_rumble_state(i, RETRO_RUMBLE_STRONG, 0);
               input_driver_set_rumble_state(i, RETRO_RUMBLE_WEAK, 0);
            }
         }
         break;
      case CMD_EVENT_GRAB_MOUSE_TOGGLE:
         {
            bool ret = false;
            static bool grab_mouse_state  = false;

            grab_mouse_state = !grab_mouse_state;

            if (grab_mouse_state)
               ret = input_driver_grab_mouse();
            else
               ret = input_driver_ungrab_mouse();

            if (!ret)
               return false;

            RARCH_LOG("%s: %s.\n",
                  msg_hash_to_str(MSG_GRAB_MOUSE_STATE),
                  grab_mouse_state ? "yes" : "no");

            if (grab_mouse_state)
               video_driver_hide_mouse();
            else
               video_driver_show_mouse();
         }
         break;
      case CMD_EVENT_UI_COMPANION_TOGGLE:
         ui_companion_driver_toggle(true);
         break;
      case CMD_EVENT_GAME_FOCUS_TOGGLE:
         {
            static bool game_focus_state  = false;
            intptr_t                 mode = (intptr_t)data;

            /* mode = -1: restores current game focus state
             * mode =  1: force set game focus, instead of toggling
             * any other: toggle
             */
            if (mode == 1)
               game_focus_state = true;
            else if (mode != -1)
               game_focus_state = !game_focus_state;

            RARCH_LOG("%s: %s.\n",
                  "Game focus is: ",
                  game_focus_state ? "on" : "off");

            if (game_focus_state)
            {
               input_driver_grab_mouse();
               video_driver_hide_mouse();
               p_rarch->input_driver_block_hotkey               = true;
               p_rarch->current_input->keyboard_mapping_blocked = true;
               if (mode != -1)
                  runloop_msg_queue_push(msg_hash_to_str(MSG_GAME_FOCUS_ON),
                        1, 120, true,
                        NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
            else
            {
               input_driver_ungrab_mouse();
               video_driver_show_mouse();
               p_rarch->input_driver_block_hotkey               = false;
               p_rarch->current_input->keyboard_mapping_blocked = false;
               if (mode != -1)
                  runloop_msg_queue_push(msg_hash_to_str(MSG_GAME_FOCUS_OFF),
                        1, 120, true,
                        NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }

         }
         break;
      case CMD_EVENT_VOLUME_UP:
         command_event_set_volume(0.5f);
         break;
      case CMD_EVENT_VOLUME_DOWN:
         command_event_set_volume(-0.5f);
         break;
      case CMD_EVENT_MIXER_VOLUME_UP:
         command_event_set_mixer_volume(0.5f);
         break;
      case CMD_EVENT_MIXER_VOLUME_DOWN:
         command_event_set_mixer_volume(-0.5f);
         break;
      case CMD_EVENT_SET_FRAME_LIMIT:
         retroarch_set_frame_limit(
               settings->floats.fastforward_ratio);
         break;
      case CMD_EVENT_DISCORD_INIT:
#ifdef HAVE_DISCORD
         {
            bool discord_enable        = settings ? settings->bools.discord_enable : false;
            const char *discord_app_id = settings ? settings->arrays.discord_app_id : NULL;

            if (!settings)
               return false;
            if (!discord_enable)
               return false;
            if (discord_is_ready())
               return true;
            discord_init(discord_app_id,
                  p_rarch->launch_arguments);
         }
#endif
         break;
      case CMD_EVENT_DISCORD_UPDATE:
#ifdef HAVE_DISCORD
         if (!data || !discord_is_ready())
            return false;

         {
            bool playlist_fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
            discord_userdata_t *userdata      = (discord_userdata_t*)data;

            discord_update(userdata->status, playlist_fuzzy_archive_match);
         }
#endif
         break;

      case CMD_EVENT_AI_SERVICE_CALL:
      {
#ifdef HAVE_TRANSLATE
         unsigned ai_service_mode = settings->uints.ai_service_mode;
         if (ai_service_mode == 1 && is_ai_service_speech_running())
         {
            ai_service_speech_stop();
#ifdef HAVE_ACCESSIBILITY
            if (is_accessibility_enabled())
               accessibility_speak_priority("stopped.", 10);
#endif
         }
#ifdef HAVE_ACCESSIBILITY
         else if (is_accessibility_enabled() && 
                  ai_service_mode == 2 &&
                  is_narrator_running())
            accessibility_speak_priority("stopped.", 10);
#endif
         else
         {
            bool paused = p_rarch->runloop_paused;
            if (data!=NULL)
               paused = *((bool*)data);

            if (p_rarch->ai_service_auto == 0 && !settings->bools.ai_service_pause)
               p_rarch->ai_service_auto = 1;
            if (p_rarch->ai_service_auto != 2)
               RARCH_LOG("AI Service Called...\n");
            run_translation_service(paused);
         }
#endif
         break;
      }
      case CMD_EVENT_NONE:
         return false;
   }

   return true;
}

/* FRONTEND */

void retroarch_override_setting_set(
      enum rarch_override_setting enum_idx, void *data)
{
   struct rarch_state            *p_rarch = &rarch_st;

   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned bit = *val;
               BIT256_SET(p_rarch->has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         p_rarch->has_set_verbosity = true;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         p_rarch->has_set_libretro = true;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         p_rarch->has_set_libretro_directory = true;
         break;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         p_rarch->has_set_save_path = true;
         break;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         p_rarch->has_set_state_path = true;
         break;
#ifdef HAVE_NETWORKING
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         p_rarch->has_set_netplay_mode = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         p_rarch->has_set_netplay_ip_address = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         p_rarch->has_set_netplay_ip_port = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         p_rarch->has_set_netplay_stateless_mode = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         p_rarch->has_set_netplay_check_frames = true;
         break;
#endif
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         p_rarch->has_set_ups_pref = true;
         break;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         p_rarch->has_set_bps_pref = true;
         break;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         p_rarch->has_set_ips_pref = true;
         break;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         p_rarch->has_set_log_to_file = true;
         break;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }
}

void retroarch_override_setting_unset(enum rarch_override_setting enum_idx, void *data)
{
   struct rarch_state            *p_rarch = &rarch_st;

   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned bit = *val;
               BIT256_CLEAR(p_rarch->has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         p_rarch->has_set_verbosity = false;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         p_rarch->has_set_libretro = false;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         p_rarch->has_set_libretro_directory = false;
         break;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         p_rarch->has_set_save_path = false;
         break;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         p_rarch->has_set_state_path = false;
         break;
#ifdef HAVE_NETWORKING
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         p_rarch->has_set_netplay_mode = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         p_rarch->has_set_netplay_ip_address = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         p_rarch->has_set_netplay_ip_port = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         p_rarch->has_set_netplay_stateless_mode = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         p_rarch->has_set_netplay_check_frames = false;
         break;
#endif
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         p_rarch->has_set_ups_pref = false;
         break;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         p_rarch->has_set_bps_pref = false;
         break;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         p_rarch->has_set_ips_pref = false;
         break;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         p_rarch->has_set_log_to_file = false;
         break;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }
}

static void retroarch_override_setting_free_state(void)
{
   unsigned i;
   for (i = 0; i < RARCH_OVERRIDE_SETTING_LAST; i++)
   {
      if (i == RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE)
      {
         unsigned j;
         for (j = 0; j < MAX_USERS; j++)
            retroarch_override_setting_unset(
                  (enum rarch_override_setting)(i), &j);
      }
      else
         retroarch_override_setting_unset(
               (enum rarch_override_setting)(i), NULL);
   }
}

static void global_free(void)
{
   global_t            *global = NULL;
   struct rarch_state *p_rarch = &rarch_st;

   content_deinit();

   path_deinit_subsystem();
   command_event(CMD_EVENT_RECORD_DEINIT, NULL);
   command_event(CMD_EVENT_LOG_FILE_DEINIT, NULL);

   p_rarch->rarch_is_sram_load_disabled  = false;
   p_rarch->rarch_is_sram_save_disabled  = false;
   p_rarch->rarch_use_sram               = false;
   rarch_ctl(RARCH_CTL_UNSET_BPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_IPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_UPS_PREF, NULL);
   p_rarch->rarch_patch_blocked               = false;
#ifdef HAVE_CONFIGFILE
   p_rarch->rarch_block_config_read           = false;
   p_rarch->runloop_overrides_active          = false;
   p_rarch->runloop_remaps_core_active        = false;
   p_rarch->runloop_remaps_game_active        = false;
   p_rarch->runloop_remaps_content_dir_active = false;
#endif

   p_rarch->current_core.has_set_input_descriptors = false;
   p_rarch->current_core.has_set_subsystems        = false;

   global                                          = &p_rarch->g_extern;
   path_clear_all();
   dir_clear_all();

   if (global)
   {
      if (!string_is_empty(global->name.remapfile))
         free(global->name.remapfile);
      memset(global, 0, sizeof(struct global));
   }
   retroarch_override_setting_free_state();
}

/**
 * main_exit:
 *
 * Cleanly exit RetroArch.
 *
 * Also saves configuration files to disk,
 * and (optionally) autosave state.
 **/
void main_exit(void *args)
{
   struct rarch_state *p_rarch  = &rarch_st;
   settings_t     *settings     = p_rarch->configuration_settings;
   bool     config_save_on_exit = settings->bools.config_save_on_exit;

   if (p_rarch->cached_video_driver[0])
   {
      configuration_set_string(settings, 
            settings->arrays.video_driver, p_rarch->cached_video_driver);

      p_rarch->cached_video_driver[0] = 0;
      RARCH_LOG("[Video]: Restored video driver to \"%s\".\n",
            settings->arrays.video_driver);
   }

   if (config_save_on_exit)
      command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);

#if defined(HAVE_GFX_WIDGETS)
   /* Do not want display widgets to live any more. */
   gfx_widgets_set_persistence(false);
#endif
#ifdef HAVE_MENU
   /* Do not want menu context to live any more. */
   menu_driver_ctl(RARCH_MENU_CTL_UNSET_OWN_DRIVER, NULL);
#endif
   rarch_ctl(RARCH_CTL_MAIN_DEINIT, NULL);

   if (p_rarch->runloop_perfcnt_enable)
      rarch_perf_log();

#if defined(HAVE_LOGGER) && !defined(ANDROID)
   logger_shutdown();
#endif

   frontend_driver_deinit(args);
   frontend_driver_exitspawn(
         path_get_ptr(RARCH_PATH_CORE),
         path_get_realsize(RARCH_PATH_CORE),
         p_rarch->launch_arguments);

   p_rarch->has_set_username        = false;
   p_rarch->rarch_is_inited         = false;
   p_rarch->rarch_error_on_init     = false;
#ifdef HAVE_CONFIGFILE
   p_rarch->rarch_block_config_read = false;
#endif

   retroarch_msg_queue_deinit();
   driver_uninit(DRIVERS_CMD_ALL);
   command_event(CMD_EVENT_LOG_FILE_DEINIT, NULL);

   rarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
   global_free();
   task_queue_deinit();

   if (p_rarch->configuration_settings)
      free(p_rarch->configuration_settings);
   p_rarch->configuration_settings = NULL;

   ui_companion_driver_deinit();

   frontend_driver_shutdown(false);

   retroarch_deinit_drivers();
   ui_companion_driver_free();
   frontend_driver_free();

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   CoUninitialize();
#endif
}

/**
 * main_entry:
 *
 * Main function of RetroArch.
 *
 * If HAVE_MAIN is not defined, will contain main loop and will not
 * be exited from until we exit the program. Otherwise, will
 * just do initialization.
 *
 * Returns: varies per platform.
 **/
int rarch_main(int argc, char *argv[], void *data)
{
   struct rarch_state *p_rarch  = &rarch_st;
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   if (FAILED(CoInitialize(NULL)))
   {
      RARCH_ERR("FATAL: Failed to initialize the COM interface\n");
      return 1;
   }
#endif

   libretro_free_system_info(&p_rarch->runloop_system.info);
   command_event(CMD_EVENT_HISTORY_DEINIT, NULL);
   rarch_favorites_deinit();

   p_rarch->configuration_settings = (settings_t*)calloc(1, sizeof(settings_t));

   retroarch_deinit_drivers();
   rarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
   global_free();

   frontend_driver_init_first(data);

   if (p_rarch->rarch_is_inited)
      driver_uninit(DRIVERS_CMD_ALL);

#ifdef HAVE_THREAD_STORAGE
   sthread_tls_create(&p_rarch->rarch_tls);
   sthread_tls_set(&p_rarch->rarch_tls, MAGIC_POINTER);
#endif
   p_rarch->video_driver_active = true;
   p_rarch->audio_driver_active = true;

   {
      uint8_t i;
      for (i = 0; i < MAX_USERS; i++)
         input_config_set_device(i, RETRO_DEVICE_JOYPAD);
   }
   retroarch_msg_queue_init();

   if (frontend_driver_is_inited())
   {
      content_ctx_info_t info;

      info.argc            = argc;
      info.argv            = argv;
      info.args            = data;
      info.environ_get     = frontend_driver_environment_get_ptr();

      if (!task_push_load_content_from_cli(
               NULL,
               NULL,
               &info,
               CORE_TYPE_PLAIN,
               NULL,
               NULL))
         return 1;
   }

   ui_companion_driver_init_first();

#if !defined(HAVE_MAIN) || defined(HAVE_QT)
   for (;;)
   {
      int ret;
      bool app_exit     = false;
#ifdef HAVE_QT
      ui_companion_qt.application->process_events();
#endif
      ret = runloop_iterate();

      task_queue_check();

#ifdef HAVE_QT
      app_exit = ui_companion_qt.application->exiting;
#endif

      if (ret == -1 || app_exit)
      {
#ifdef HAVE_QT
         ui_companion_qt.application->quit();
#endif
         break;
      }
   }

   main_exit(data);
#endif

   return 0;
}

#ifndef HAVE_MAIN
#ifdef __cplusplus
extern "C"
#endif
int main(int argc, char *argv[])
{
   return rarch_main(argc, argv, NULL);
}
#endif

/* CORE OPTIONS */
static bool core_option_manager_parse_variable(
      core_option_manager_t *opt, size_t idx,
      const struct retro_variable *var,
      config_file_t *config_src)
{
   const char *val_start      = NULL;
   char *value                = NULL;
   char *desc_end             = NULL;
   char *config_val           = NULL;
   struct core_option *option = (struct core_option*)&opt->opts[idx];

   /* All options are visible by default */
   option->visible = true;

   if (!string_is_empty(var->key))
      option->key             = strdup(var->key);
   if (!string_is_empty(var->value))
      value                   = strdup(var->value);

   if (!string_is_empty(value))
      desc_end                = strstr(value, "; ");

   if (!desc_end)
      goto error;

   *desc_end    = '\0';

   if (!string_is_empty(value))
      option->desc    = strdup(value);

   val_start          = desc_end + 2;
   option->vals       = string_split(val_start, "|");

   if (!option->vals)
      goto error;

   /* Legacy core option interface has no concept of
    * value labels - use actual values for display purposes */
   option->val_labels = string_list_clone(option->vals);

   if (!option->val_labels)
      goto error;

   /* Legacy core option interface always uses first
    * defined value as the default */
   option->default_index = 0;
   option->index         = 0;

   /* Set current config value */
   if (config_get_string(config_src ? config_src : opt->conf, option->key, &config_val))
   {
      size_t i;

      for (i = 0; i < option->vals->size; i++)
      {
         if (string_is_equal(option->vals->elems[i].data, config_val))
         {
            option->index = i;
            break;
         }
      }

      free(config_val);
   }

   free(value);

   return true;

error:
   free(value);
   return false;
}

static bool core_option_manager_parse_option(
      core_option_manager_t *opt, size_t idx,
      const struct retro_core_option_definition *option_def,
      config_file_t *config_src)
{
   size_t i;
   union string_list_elem_attr attr;
   size_t num_vals                              = 0;
   char *config_val                             = NULL;
   struct core_option *option                   = (struct core_option*)&opt->opts[idx];
   const struct retro_core_option_value *values = option_def->values;

   /* All options are visible by default */
   option->visible = true;

   if (!string_is_empty(option_def->key))
      option->key             = strdup(option_def->key);

   if (!string_is_empty(option_def->desc))
      option->desc            = strdup(option_def->desc);

   if (!string_is_empty(option_def->info))
      option->info            = strdup(option_def->info);

   /* Get number of values */
   for (;;)
   {
      if (string_is_empty(values[num_vals].value))
         break;
      num_vals++;
   }

   if (num_vals < 1)
      return false;

   /* Initialise string lists */
   attr.i             = 0;
   option->vals       = string_list_new();
   option->val_labels = string_list_new();

   if (!option->vals || !option->val_labels)
      return false;

   /* Initialse default value */
   option->default_index = 0;
   option->index         = 0;

   /* Extract value/label pairs */
   for (i = 0; i < num_vals; i++)
   {
      /* We know that 'value' is valid */
      string_list_append(option->vals, values[i].value, attr);

      /* Value 'label' may be NULL */
      if (!string_is_empty(values[i].label))
         string_list_append(option->val_labels, values[i].label, attr);
      else
         string_list_append(option->val_labels, values[i].value, attr);

      /* Check whether this value is the default setting */
      if (!string_is_empty(option_def->default_value))
      {
         if (string_is_equal(option_def->default_value, values[i].value))
         {
            option->default_index = i;
            option->index         = i;
         }
      }
   }

   /* Set current config value */
   if (config_get_string(config_src ? config_src : opt->conf, option->key, &config_val))
   {
      for (i = 0; i < option->vals->size; i++)
      {
         if (string_is_equal(option->vals->elems[i].data, config_val))
         {
            option->index = i;
            break;
         }
      }

      free(config_val);
   }

   return true;
}

/**
 * core_option_manager_free:
 * @opt              : options manager handle
 *
 * Frees core option manager handle.
 **/
static void core_option_manager_free(core_option_manager_t *opt)
{
   size_t i;

   if (!opt)
      return;

   for (i = 0; i < opt->size; i++)
   {
      if (opt->opts[i].desc)
         free(opt->opts[i].desc);
      if (opt->opts[i].info)
         free(opt->opts[i].info);
      if (opt->opts[i].key)
         free(opt->opts[i].key);

      if (opt->opts[i].vals)
         string_list_free(opt->opts[i].vals);
      if (opt->opts[i].val_labels)
         string_list_free(opt->opts[i].val_labels);

      opt->opts[i].desc = NULL;
      opt->opts[i].info = NULL;
      opt->opts[i].key  = NULL;
      opt->opts[i].vals = NULL;
   }

   if (opt->conf)
      config_file_free(opt->conf);
   free(opt->opts);
   free(opt);
}

/**
 * core_option_manager_new_vars:
 * @conf_path        : Filesystem path to write core option config file to.
 * @src_conf_path    : Filesystem path from which to load initial config settings.
 * @vars             : Pointer to variable array handle.
 *
 * Legacy version of core_option_manager_new().
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle, otherwise NULL.
 **/
static core_option_manager_t *core_option_manager_new_vars(
      const char *conf_path, const char *src_conf_path,
      const struct retro_variable *vars)
{
   const struct retro_variable *var;
   size_t size                       = 0;
   core_option_manager_t *opt        = (core_option_manager_t*)
      calloc(1, sizeof(*opt));
   config_file_t *config_src         = NULL;

   if (!opt)
      return NULL;

   if (!string_is_empty(conf_path))
      if (!(opt->conf = config_file_new_from_path_to_string(conf_path)))
         if (!(opt->conf = config_file_new_alloc()))
            goto error;

   strlcpy(opt->conf_path, conf_path, sizeof(opt->conf_path));

   /* Load source config file, if required */
   if (!string_is_empty(src_conf_path))
      config_src = config_file_new_from_path_to_string(src_conf_path);

   for (var = vars; var->key && var->value; var++)
      size++;

   if (size == 0)
      goto error;

   opt->opts = (struct core_option*)calloc(size, sizeof(*opt->opts));
   if (!opt->opts)
      goto error;

   opt->size = size;
   size      = 0;

   for (var = vars; var->key && var->value; size++, var++)
   {
      if (!core_option_manager_parse_variable(opt, size, var, config_src))
         goto error;
   }

   if (config_src)
      config_file_free(config_src);

   return opt;

error:
   if (config_src)
      config_file_free(config_src);
   core_option_manager_free(opt);
   return NULL;
}

/**
 * core_option_manager_new:
 * @conf_path        : Filesystem path to write core option config file to.
 * @src_conf_path    : Filesystem path from which to load initial config settings.
 * @option_defs      : Pointer to variable array handle.
 *
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle, otherwise NULL.
 **/
static core_option_manager_t *core_option_manager_new(
      const char *conf_path, const char *src_conf_path,
      const struct retro_core_option_definition *option_defs)
{
   const struct retro_core_option_definition *option_def;
   size_t size                       = 0;
   core_option_manager_t *opt        = (core_option_manager_t*)
      calloc(1, sizeof(*opt));
   config_file_t *config_src         = NULL;

   if (!opt)
      return NULL;

   if (!string_is_empty(conf_path))
      if (!(opt->conf = config_file_new_from_path_to_string(conf_path)))
         if (!(opt->conf = config_file_new_alloc()))
            goto error;

   strlcpy(opt->conf_path, conf_path, sizeof(opt->conf_path));

   /* Load source config file, if required */
   if (!string_is_empty(src_conf_path))
      config_src = config_file_new_from_path_to_string(src_conf_path);

   /* Note: 'option_def->info == NULL' is valid */
   for (option_def = option_defs;
        option_def->key && option_def->desc && option_def->values[0].value;
        option_def++)
      size++;

   if (size == 0)
      goto error;

   opt->opts = (struct core_option*)calloc(size, sizeof(*opt->opts));
   if (!opt->opts)
      goto error;

   opt->size = size;
   size      = 0;

   /* Note: 'option_def->info == NULL' is valid */
   for (option_def = option_defs;
        option_def->key && option_def->desc && option_def->values[0].value;
        size++, option_def++)
   {
      if (!core_option_manager_parse_option(opt, size, option_def, config_src))
         goto error;
   }

   if (config_src)
      config_file_free(config_src);

   return opt;

error:
   if (config_src)
      config_file_free(config_src);
   core_option_manager_free(opt);
   return NULL;
}

/**
 * core_option_manager_flush:
 * @opt              : options manager handle
 *
 * Writes core option key-pair values to file.
 **/
static void core_option_manager_flush(
      config_file_t *conf,
      core_option_manager_t *opt)
{
   size_t i;

   for (i = 0; i < opt->size; i++)
   {
      struct core_option *option = (struct core_option*)&opt->opts[i];

      if (option)
         config_set_string(conf, option->key,
               opt->opts[i].vals->elems[opt->opts[i].index].data);
   }
}

/**
 * core_option_manager_get_desc:
 * @opt              : options manager handle
 * @index            : index identifier of the option
 *
 * Gets description for an option.
 *
 * Returns: Description for an option.
 **/
const char *core_option_manager_get_desc(
      core_option_manager_t *opt, size_t idx)
{
   if (!opt)
      return NULL;
   if (idx >= opt->size)
      return NULL;
   return opt->opts[idx].desc;
}

/**
 * core_option_manager_get_info:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets information text for an option.
 *
 * Returns: Information text for an option.
 **/
const char *core_option_manager_get_info(
      core_option_manager_t *opt, size_t idx)
{
   if (!opt)
      return NULL;
   if (idx >= opt->size)
      return NULL;
   return opt->opts[idx].info;
}

/**
 * core_option_manager_get_val:
 * @opt              : options manager handle
 * @index            : index identifier of the option
 *
 * Gets value for an option.
 *
 * Returns: Value for an option.
 **/
const char *core_option_manager_get_val(core_option_manager_t *opt, size_t idx)
{
   struct core_option *option = NULL;
   if (!opt)
      return NULL;
   if (idx >= opt->size)
      return NULL;
   option = (struct core_option*)&opt->opts[idx];
   return option->vals->elems[option->index].data;
}

/**
 * core_option_manager_get_val_label:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets value label for an option.
 *
 * Returns: Value label for an option.
 **/
const char *core_option_manager_get_val_label(core_option_manager_t *opt, size_t idx)
{
   struct core_option *option = NULL;
   if (!opt)
      return NULL;
   if (idx >= opt->size)
      return NULL;
   option = (struct core_option*)&opt->opts[idx];
   return option->val_labels->elems[option->index].data;
}

/**
 * core_option_manager_get_visible:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets whether option should be visible when displaying
 * core options in the frontend
 *
 * Returns: 'true' if option should be displayed by the frontend.
 **/
bool core_option_manager_get_visible(core_option_manager_t *opt,
      size_t idx)
{
   if (!opt)
      return false;
   if (idx >= opt->size)
      return false;
   return opt->opts[idx].visible;
}

void core_option_manager_set_val(core_option_manager_t *opt,
      size_t idx, size_t val_idx)
{
   struct core_option *option= NULL;

   if (!opt)
      return;
   if (idx >= opt->size)
      return;

   option        = (struct core_option*)&opt->opts[idx];
   option->index = val_idx % option->vals->size;

   opt->updated  = true;
}

/**
 * core_option_manager_set_default:
 * @opt                   : pointer to core option manager object.
 * @idx                   : index of core option to be reset to defaults.
 *
 * Reset core option specified by @idx and sets default value for option.
 **/
void core_option_manager_set_default(core_option_manager_t *opt, size_t idx)
{
   if (!opt)
      return;
   if (idx >= opt->size)
      return;

   opt->opts[idx].index = opt->opts[idx].default_index;
   opt->updated         = true;
}

static struct retro_core_option_definition *core_option_manager_get_definitions(
      const struct retro_core_options_intl *core_options_intl)
{
   size_t i;
   size_t num_options                                     = 0;
   struct retro_core_option_definition *option_defs_us    = NULL;
   struct retro_core_option_definition *option_defs_local = NULL;
   struct retro_core_option_definition *option_defs       = NULL;

   if (!core_options_intl)
      return NULL;

   option_defs_us    = core_options_intl->us;
   option_defs_local = core_options_intl->local;

   if (!option_defs_us)
      return NULL;

   /* Determine number of options */
   for (;;)
   {
      if (string_is_empty(option_defs_us[num_options].key))
         break;
      num_options++;
   }

   if (num_options < 1)
      return NULL;

   /* Allocate output option_defs array
    * > One extra entry required for terminating NULL entry
    * > Note that calloc() sets terminating NULL entry and
    *   correctly 'nullifies' each values array */
   option_defs = (struct retro_core_option_definition *)calloc(
         num_options + 1, sizeof(struct retro_core_option_definition));

   if (!option_defs)
      return NULL;

   /* Loop through options... */
   for (i = 0; i < num_options; i++)
   {
      size_t j;
      size_t num_values                            = 0;
      const char *key                              = option_defs_us[i].key;
      const char *local_desc                       = NULL;
      const char *local_info                       = NULL;
      struct retro_core_option_value *local_values = NULL;

      /* Key is always taken from us english defs */
      option_defs[i].key = key;

      /* Default value is always taken from us english defs */
      option_defs[i].default_value = option_defs_us[i].default_value;

      /* Try to find corresponding entry in local defs array */
      if (option_defs_local)
      {
         size_t index = 0;

         for (;;)
         {
            const char *local_key = option_defs_local[index].key;

            if (string_is_empty(local_key))
               break;

            if (string_is_equal(key, local_key))
            {
               local_desc   = option_defs_local[index].desc;
               local_info   = option_defs_local[index].info;
               local_values = option_defs_local[index].values;
               break;
            }

            index++;
         }
      }

      /* Set desc and info strings */
      option_defs[i].desc = string_is_empty(local_desc) ? option_defs_us[i].desc : local_desc;
      option_defs[i].info = string_is_empty(local_info) ? option_defs_us[i].info : local_info;

      /* Determine number of values
       * (always taken from us english defs) */
      for (;;)
      {
         if (string_is_empty(option_defs_us[i].values[num_values].value))
            break;
         num_values++;
      }

      /* Copy values */
      for (j = 0; j < num_values; j++)
      {
         const char *value       = option_defs_us[i].values[j].value;
         const char *local_label = NULL;

         /* Value string is always taken from us english defs */
         option_defs[i].values[j].value = value;

         /* Try to find corresponding entry in local defs values array */
         if (local_values)
         {
            size_t value_index = 0;

            for (;;)
            {
               const char *local_value = local_values[value_index].value;

               if (string_is_empty(local_value))
                  break;

               if (string_is_equal(value, local_value))
               {
                  local_label = local_values[value_index].label;
                  break;
               }

               value_index++;
            }
         }

         /* Set value label string */
         option_defs[i].values[j].label = string_is_empty(local_label) ?
               option_defs_us[i].values[j].label : local_label;
      }
   }

   return option_defs;
}

static void core_option_manager_set_display(core_option_manager_t *opt,
      const char *key, bool visible)
{
   size_t i;

   if (!opt || string_is_empty(key))
      return;

   for (i = 0; i < opt->size; i++)
   {
      if (string_is_empty(opt->opts[i].key))
         continue;

      if (string_is_equal(opt->opts[i].key, key))
      {
         opt->opts[i].visible = visible;
         return;
      }
   }
}

/* DYNAMIC LIBRETRO CORE  */

const struct retro_subsystem_info *libretro_find_subsystem_info(
      const struct retro_subsystem_info *info, unsigned num_info,
      const char *ident)
{
   unsigned i;
   for (i = 0; i < num_info; i++)
   {
      if (string_is_equal(info[i].ident, ident))
         return &info[i];
      else if (string_is_equal(info[i].desc, ident))
         return &info[i];
   }

   return NULL;
}

/**
 * libretro_find_controller_description:
 * @info                         : Pointer to controller info handle.
 * @id                           : Identifier of controller to search
 *                                 for.
 *
 * Search for a controller of type @id in @info.
 *
 * Returns: controller description of found controller on success,
 * otherwise NULL.
 **/
const struct retro_controller_description *
libretro_find_controller_description(
      const struct retro_controller_info *info, unsigned id)
{
   unsigned i;

   for (i = 0; i < info->num_types; i++)
   {
      if (info->types[i].id != id)
         continue;

      return &info->types[i];
   }

   return NULL;
}

/**
 * libretro_free_system_info:
 * @info                         : Pointer to system info information.
 *
 * Frees system information.
 **/
void libretro_free_system_info(struct retro_system_info *info)
{
   if (!info)
      return;

   free((void*)info->library_name);
   free((void*)info->library_version);
   free((void*)info->valid_extensions);
   memset(info, 0, sizeof(*info));
}

static bool environ_cb_get_system_info(unsigned cmd, void *data)
{
   struct rarch_state *p_rarch  = &rarch_st;
   rarch_system_info_t *system  = &p_rarch->runloop_system;

   switch (cmd)
   {
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
         *p_rarch->load_no_content_hook = *(const bool*)data;
         break;
      case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
      {
         unsigned i, j, size;
         const struct retro_subsystem_info *info =
            (const struct retro_subsystem_info*)data;
         settings_t *settings    = p_rarch->configuration_settings;
         unsigned log_level      = settings->uints.libretro_log_level;

         subsystem_current_count = 0;

         RARCH_LOG("[Environ]: SET_SUBSYSTEM_INFO.\n");

         for (i = 0; info[i].ident; i++)
         {
            if (log_level != RETRO_LOG_DEBUG)
               continue;

            RARCH_LOG("Subsystem ID: %d\nSpecial game type: %s\n  Ident: %s\n  ID: %u\n  Content:\n",
                  i,
                  info[i].desc,
                  info[i].ident,
                  info[i].id
                  );
            for (j = 0; j < info[i].num_roms; j++)
            {
               RARCH_LOG("    %s (%s)\n",
                     info[i].roms[j].desc, info[i].roms[j].required ?
                     "required" : "optional");
            }
         }

         size = i;

         if (log_level == RETRO_LOG_DEBUG)
         {
            RARCH_LOG("Subsystems: %d\n", i);
            if (size > SUBSYSTEM_MAX_SUBSYSTEMS)
               RARCH_WARN("Subsystems exceed subsystem max, clamping to %d\n", SUBSYSTEM_MAX_SUBSYSTEMS);
         }

         if (system)
         {
            for (i = 0; i < size && i < SUBSYSTEM_MAX_SUBSYSTEMS; i++)
            {
               /* Nasty, but have to do it like this since
                * the pointers are const char *
                * (if we don't free them, we get a memory leak) */
               if (!string_is_empty(subsystem_data[i].desc))
                  free((char *)subsystem_data[i].desc);
               if (!string_is_empty(subsystem_data[i].ident))
                  free((char *)subsystem_data[i].ident);
               subsystem_data[i].desc     = strdup(info[i].desc);
               subsystem_data[i].ident    = strdup(info[i].ident);
               subsystem_data[i].id       = info[i].id;
               subsystem_data[i].num_roms = info[i].num_roms;

               if (log_level == RETRO_LOG_DEBUG)
                  if (subsystem_data[i].num_roms > SUBSYSTEM_MAX_SUBSYSTEM_ROMS)
                     RARCH_WARN("Subsystems exceed subsystem max roms, clamping to %d\n", SUBSYSTEM_MAX_SUBSYSTEM_ROMS);

               for (j = 0; j < subsystem_data[i].num_roms && j < SUBSYSTEM_MAX_SUBSYSTEM_ROMS; j++)
               {
                  /* Nasty, but have to do it like this since
                   * the pointers are const char *
                   * (if we don't free them, we get a memory leak) */
                  if (!string_is_empty(p_rarch->subsystem_data_roms[i][j].desc))
                     free((char *)p_rarch->subsystem_data_roms[i][j].desc);
                  if (!string_is_empty(p_rarch->subsystem_data_roms[i][j].valid_extensions))
                     free((char *)p_rarch->subsystem_data_roms[i][j].valid_extensions);
                  p_rarch->subsystem_data_roms[i][j].desc             = strdup(info[i].roms[j].desc);
                  p_rarch->subsystem_data_roms[i][j].valid_extensions = strdup(info[i].roms[j].valid_extensions);
                  p_rarch->subsystem_data_roms[i][j].required         = info[i].roms[j].required;
                  p_rarch->subsystem_data_roms[i][j].block_extract    = info[i].roms[j].block_extract;
                  p_rarch->subsystem_data_roms[i][j].need_fullpath    = info[i].roms[j].need_fullpath;
               }

               subsystem_data[i].roms               = p_rarch->subsystem_data_roms[i];
            }

            subsystem_current_count =
               size <= SUBSYSTEM_MAX_SUBSYSTEMS
               ? size
               : SUBSYSTEM_MAX_SUBSYSTEMS;
         }
         break;
      }
      default:
         return false;
   }

   return true;
}

static bool dynamic_request_hw_context(enum retro_hw_context_type type,
      unsigned minor, unsigned major)
{
   switch (type)
   {
      case RETRO_HW_CONTEXT_NONE:
         RARCH_LOG("Requesting no HW context.\n");
         break;

      case RETRO_HW_CONTEXT_VULKAN:
#ifdef HAVE_VULKAN
         RARCH_LOG("Requesting Vulkan context.\n");
         break;
#else
         RARCH_ERR("Requesting Vulkan context, but RetroArch is not compiled against Vulkan. Cannot use HW context.\n");
         return false;
#endif

#if defined(HAVE_OPENGLES)

#if (defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3))
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
         RARCH_LOG("Requesting OpenGLES%u context.\n",
               type == RETRO_HW_CONTEXT_OPENGLES2 ? 2 : 3);
         break;

#if defined(HAVE_OPENGLES3)
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         RARCH_LOG("Requesting OpenGLES%u.%u context.\n",
               major, minor);
         break;
#endif

#endif
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
         RARCH_ERR("Requesting OpenGL context, but RetroArch "
               "is compiled against OpenGLES. Cannot use HW context.\n");
         return false;

#elif defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
         RARCH_ERR("Requesting OpenGLES%u context, but RetroArch "
               "is compiled against OpenGL. Cannot use HW context.\n",
               type == RETRO_HW_CONTEXT_OPENGLES2 ? 2 : 3);
         return false;

      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         RARCH_ERR("Requesting OpenGLES%u.%u context, but RetroArch "
               "is compiled against OpenGL. Cannot use HW context.\n",
               major, minor);
         return false;

      case RETRO_HW_CONTEXT_OPENGL:
         RARCH_LOG("Requesting OpenGL context.\n");
         break;

      case RETRO_HW_CONTEXT_OPENGL_CORE:
         /* TODO/FIXME - we should do a check here to see if
          * the requested core GL version is supported */
         RARCH_LOG("Requesting core OpenGL context (%u.%u).\n",
               major, minor);
         break;
#endif

#if defined(HAVE_D3D9) || defined(HAVE_D3D11)
      case RETRO_HW_CONTEXT_DIRECT3D:
         switch (major)
         {
#ifdef HAVE_D3D9
            case 9:
               RARCH_LOG("Requesting D3D9 context.\n");
               break;
#endif
#ifdef HAVE_D3D11
            case 11:
               RARCH_LOG("Requesting D3D11 context.\n");
               break;
#endif
            default:
               RARCH_LOG("Requesting unknown context.\n");
               return false;
         }
         break;
#endif

      default:
         RARCH_LOG("Requesting unknown context.\n");
         return false;
   }

   return true;
}

static bool dynamic_verify_hw_context(enum retro_hw_context_type type,
      unsigned minor, unsigned major)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t   *settings      = p_rarch->configuration_settings;
   const char   *video_ident   = settings->arrays.video_driver;
   bool   driver_switch_enable = settings->bools.driver_switch_enable;

   if (driver_switch_enable)
      return true;

   switch (type)
   {
      case RETRO_HW_CONTEXT_VULKAN:
         if (!string_is_equal(video_ident, "vulkan"))
            return false;
         break;
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
         if (!string_is_equal(video_ident, "gl") &&
             !string_is_equal(video_ident, "glcore"))
            return false;
         break;
      case RETRO_HW_CONTEXT_DIRECT3D:
         if (!(string_is_equal(video_ident, "d3d11") && major == 11))
            return false;
         break;
      default:
         break;
   }

   return true;
}

static void rarch_log_libretro(enum retro_log_level level,
      const char *fmt, ...)
{
   va_list vp;
   struct rarch_state *p_rarch = &rarch_st;
   settings_t        *settings = p_rarch->configuration_settings;
   unsigned libretro_log_level = settings->uints.libretro_log_level;

   if ((unsigned)level < libretro_log_level)
      return;

   if (!verbosity_is_enabled())
      return;

   va_start(vp, fmt);

   switch (level)
   {
      case RETRO_LOG_DEBUG:
         RARCH_LOG_V("[libretro DEBUG]", fmt, vp);
         break;

      case RETRO_LOG_INFO:
         RARCH_LOG_OUTPUT_V("[libretro INFO]", fmt, vp);
         break;

      case RETRO_LOG_WARN:
         RARCH_WARN_V("[libretro WARN]", fmt, vp);
         break;

      case RETRO_LOG_ERROR:
         RARCH_ERR_V("[libretro ERROR]", fmt, vp);
         break;

      default:
         break;
   }

   va_end(vp);
}

static void core_performance_counter_start(struct retro_perf_counter *perf)
{
   struct rarch_state *p_rarch = &rarch_st;
   bool runloop_perfcnt_enable = p_rarch->runloop_perfcnt_enable;

   if (runloop_perfcnt_enable)
   {
      perf->call_cnt++;
      perf->start      = cpu_features_get_perf_counter();
   }
}

static void core_performance_counter_stop(struct retro_perf_counter *perf)
{
   struct rarch_state *p_rarch = &rarch_st;
   bool runloop_perfcnt_enable = p_rarch->runloop_perfcnt_enable;

   if (runloop_perfcnt_enable)
      perf->total += cpu_features_get_perf_counter() - perf->start;
}

static size_t mmap_add_bits_down(size_t n)
{
   n |= n >>  1;
   n |= n >>  2;
   n |= n >>  4;
   n |= n >>  8;
   n |= n >> 16;

   /* double shift to avoid warnings on 32bit (it's dead code, 
    * but compilers suck) */
   if (sizeof(size_t) > 4)
      n |= n >> 16 >> 16;

   return n;
}

static size_t mmap_inflate(size_t addr, size_t mask)
{
    while (mask)
   {
      size_t tmp = (mask - 1) & ~mask;

      /* to put in an 1 bit instead, OR in tmp+1 */
      addr       = ((addr & ~tmp) << 1) | (addr & tmp);
      mask       = mask & (mask - 1);
   }

   return addr;
}

static size_t mmap_reduce(size_t addr, size_t mask)
{
   while (mask)
   {
      size_t tmp = (mask - 1) & ~mask;
      addr       = (addr & tmp) | ((addr >> 1) & ~tmp);
      mask       = (mask & (mask - 1)) >> 1;
   }

   return addr;
}

static size_t mmap_highest_bit(size_t n)
{
   n = mmap_add_bits_down(n);
   return n ^ (n >> 1);
}


static bool mmap_preprocess_descriptors(
      rarch_memory_descriptor_t *first, unsigned count)
{
   size_t                      top_addr = 1;
   rarch_memory_descriptor_t *desc      = NULL;
   const rarch_memory_descriptor_t *end = first + count;

   for (desc = first; desc < end; desc++)
   {
      if (desc->core.select != 0)
         top_addr |= desc->core.select;
      else
         top_addr |= desc->core.start + desc->core.len - 1;
   }

   top_addr = mmap_add_bits_down(top_addr);

   for (desc = first; desc < end; desc++)
   {
      if (desc->core.select == 0)
      {
         if (desc->core.len == 0)
            return false;

         if ((desc->core.len & (desc->core.len - 1)) != 0)
            return false;

         desc->core.select = top_addr & ~mmap_inflate(mmap_add_bits_down(desc->core.len - 1),
               desc->core.disconnect);
      }

      if (desc->core.len == 0)
         desc->core.len = mmap_add_bits_down(mmap_reduce(top_addr & ~desc->core.select,
                  desc->core.disconnect)) + 1;

      if (desc->core.start & ~desc->core.select)
         return false;

      while (mmap_reduce(top_addr & ~desc->core.select, desc->core.disconnect) >> 1 > desc->core.len - 1)
         desc->core.disconnect |= mmap_highest_bit(top_addr & ~desc->core.select & ~desc->core.disconnect);

      desc->disconnect_mask = mmap_add_bits_down(desc->core.len - 1);
      desc->core.disconnect &= desc->disconnect_mask;

      while ((~desc->disconnect_mask) >> 1 & desc->core.disconnect)
      {
         desc->disconnect_mask >>= 1;
         desc->core.disconnect &= desc->disconnect_mask;
      }
   }

   return true;
}

static bool rarch_clear_all_thread_waits(unsigned clear_threads, void *data)
{
   if ( clear_threads > 0)
      audio_driver_start(false);
   else
      audio_driver_stop();

   return true;
}

static void runloop_core_msg_queue_push(const struct retro_message_ext *msg)
{
   double fps;
   unsigned duration_frames;
   enum message_queue_category category;
   struct rarch_state          *p_rarch = &rarch_st;
   struct retro_system_av_info *av_info = &p_rarch->video_driver_av_info;

   /* Assign category */
   switch (msg->level)
   {
      case RETRO_LOG_WARN:
         category = MESSAGE_QUEUE_CATEGORY_WARNING;
         break;
      case RETRO_LOG_ERROR:
         category = MESSAGE_QUEUE_CATEGORY_ERROR;
         break;
      case RETRO_LOG_INFO:
      case RETRO_LOG_DEBUG:
      default:
         category = MESSAGE_QUEUE_CATEGORY_INFO;
         break;
   }

   /* Get duration in frames */
   fps             = av_info ? av_info->timing.fps : 60.0;
   duration_frames = (unsigned)((fps * (float)msg->duration / 1000.0f) + 0.5f);

   runloop_msg_queue_push(msg->msg,
         msg->priority, duration_frames,
         true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
         category);
}

/**
 * rarch_environment_cb:
 * @cmd                          : Identifier of command.
 * @data                         : Pointer to data.
 *
 * Environment callback function implementation.
 *
 * Returns: true (1) if environment callback command could
 * be performed, otherwise false (0).
 **/
static bool rarch_environment_cb(unsigned cmd, void *data)
{
   unsigned p;
   struct rarch_state *p_rarch  = &rarch_st;
   settings_t         *settings = p_rarch->configuration_settings;
   rarch_system_info_t *system  = &p_rarch->runloop_system;
   bool ignore_environment_cb   = p_rarch->ignore_environment_cb;

   if (ignore_environment_cb)
      return false;

   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_OVERSCAN:
         {
            bool video_crop_overscan = settings->bools.video_crop_overscan;
            *(bool*)data = !video_crop_overscan;
            RARCH_LOG("[Environ]: GET_OVERSCAN: %u\n",
                  (unsigned)!video_crop_overscan);
         }
         break;

      case RETRO_ENVIRONMENT_GET_CAN_DUPE:
         *(bool*)data = true;
         RARCH_LOG("[Environ]: GET_CAN_DUPE: true\n");
         break;

      case RETRO_ENVIRONMENT_GET_VARIABLE:
         {
            unsigned log_level         = settings->uints.libretro_log_level;
            struct retro_variable *var = (struct retro_variable*)data;

            if (!var)
               return true;

            var->value = NULL;

            if (!p_rarch->runloop_core_options)
            {
               RARCH_LOG("[Environ]: GET_VARIABLE %s: not implemented.\n",
                     var->key);
               return true;
            }

            {
               size_t i;

#ifdef HAVE_RUNAHEAD
               if (p_rarch->runloop_core_options->updated)
                  p_rarch->has_variable_update = true;
#endif

               p_rarch->runloop_core_options->updated   = false;

               for (i = 0; i < p_rarch->runloop_core_options->size; i++)
               {
                  if (!string_is_empty(p_rarch->runloop_core_options->opts[i].key))
                     if (string_is_equal(
                              p_rarch->runloop_core_options->opts[i].key, var->key))
                     {
                        var->value = p_rarch->runloop_core_options->opts[i].vals->elems[
                           p_rarch->runloop_core_options->opts[i].index].data;
                        break;
                     }
               }
            }

            if (log_level == RETRO_LOG_DEBUG)
            {
               char s[128];
               s[0] = '\0';

               snprintf(s, sizeof(s), "[Environ]: GET_VARIABLE %s:\n\t%s\n", var->key, var->value ? var->value :
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
               RARCH_LOG(s);
            }
         }

         break;

      case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
         if (p_rarch->runloop_core_options)
            *(bool*)data = p_rarch->runloop_core_options->updated;
         else
            *(bool*)data = false;
         break;

      /* SET_VARIABLES: Legacy path */
      case RETRO_ENVIRONMENT_SET_VARIABLES:
         RARCH_LOG("[Environ]: SET_VARIABLES.\n");

         if (p_rarch->runloop_core_options)
            retroarch_deinit_core_options();
         retroarch_init_core_variables((const struct retro_variable *)data);

         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
         RARCH_LOG("[Environ]: SET_CORE_OPTIONS.\n");

         if (p_rarch->runloop_core_options)
            retroarch_deinit_core_options();
         rarch_init_core_options(
               (const struct retro_core_option_definition*)data);

         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
         RARCH_LOG("[Environ]: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL.\n");

         if (p_rarch->runloop_core_options)
            retroarch_deinit_core_options();
         retroarch_core_options_intl_init((const struct 
                  retro_core_options_intl *)data);

         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
         RARCH_LOG("[Environ]: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY.\n");

         {
            const struct retro_core_option_display *core_options_display = (const struct retro_core_option_display *)data;

            if (p_rarch->runloop_core_options && core_options_display)
               core_option_manager_set_display(
                     p_rarch->runloop_core_options,
                     core_options_display->key,
                     core_options_display->visible);
         }

         break;

      case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
         /* Current API version is 1 */
         *(unsigned *)data = 1;
         break;

      case RETRO_ENVIRONMENT_SET_MESSAGE:
      {
         const struct retro_message *msg = (const struct retro_message*)data;
         RARCH_LOG("[Environ]: SET_MESSAGE: %s\n", msg->msg);
#if defined(HAVE_GFX_WIDGETS)
         if (gfx_widgets_active())
            gfx_widget_set_libretro_message(msg->msg,
                  roundf((float)msg->frames / 60.0f * 1000.0f));
         else
#endif
            runloop_msg_queue_push(msg->msg, 3, msg->frames,
                  true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
                  MESSAGE_QUEUE_CATEGORY_INFO);
         break;
      }

      case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
      {
         const struct retro_message_ext *msg = 
            (const struct retro_message_ext*)data;

         /* Log message, if required */
         if (msg->target != RETRO_MESSAGE_TARGET_OSD)
         {
            settings_t *settings = p_rarch->configuration_settings;
            unsigned log_level   = settings->uints.frontend_log_level;
            switch (msg->level)
            {
               case RETRO_LOG_DEBUG:
                  if (log_level == RETRO_LOG_DEBUG)
                     RARCH_LOG("[Environ]: SET_MESSAGE_EXT: %s\n", msg->msg);
                  break;
               case RETRO_LOG_WARN:
                  RARCH_WARN("[Environ]: SET_MESSAGE_EXT: %s\n", msg->msg);
                  break;
               case RETRO_LOG_ERROR:
                  RARCH_ERR("[Environ]: SET_MESSAGE_EXT: %s\n", msg->msg);
                  break;
               case RETRO_LOG_INFO:
               default:
                  RARCH_LOG("[Environ]: SET_MESSAGE_EXT: %s\n", msg->msg);
                  break;
            }
         }

         /* Display message via OSD, if required */
         if (msg->target != RETRO_MESSAGE_TARGET_LOG)
         {
            switch (msg->type)
            {
               /* Handle 'status' messages */
               case RETRO_MESSAGE_TYPE_STATUS:

                  /* Note: We need to lock a mutex here. Strictly
                   * speaking, runloop_core_status_msg is not part
                   * of the message queue, but:
                   * - It may be implemented as a queue in the future
                   * - It seems unnecessary to create a new slock_t
                   *   object for this type of message when
                   *   _runloop_msg_queue_lock is already available
                   * We therefore just call runloop_msg_queue_lock()/
                   * runloop_msg_queue_unlock() in this case */
                  runloop_msg_queue_lock();

                  /* If a message is already set, only overwrite
                   * it if the new message has the same or higher
                   * priority */
                  if (!runloop_core_status_msg.set ||
                      (runloop_core_status_msg.priority <= msg->priority))
                  {
                     if (!string_is_empty(msg->msg))
                     {
                        strlcpy(runloop_core_status_msg.str, msg->msg,
                              sizeof(runloop_core_status_msg.str));

                        runloop_core_status_msg.duration = (float)msg->duration;
                        runloop_core_status_msg.set      = true;
                     }
                     else
                     {
                        /* Ensure sane behaviour if core sends an
                         * empty message */
                        runloop_core_status_msg.str[0] = '\0';
                        runloop_core_status_msg.priority = 0;
                        runloop_core_status_msg.duration = 0.0f;
                        runloop_core_status_msg.set      = false;
                     }
                  }

                  runloop_msg_queue_unlock();
                  break;

#if defined(HAVE_GFX_WIDGETS)
               /* Handle 'alternate' non-queued notifications */
               case RETRO_MESSAGE_TYPE_NOTIFICATION_ALT:

                  if (gfx_widgets_active())
                     gfx_widget_set_libretro_message(msg->msg, msg->duration);
                  else
                     runloop_core_msg_queue_push(msg);

                  break;

               /* Handle 'progress' messages
                * TODO/FIXME: At present, we also display messages
                * of type RETRO_MESSAGE_TYPE_PROGRESS via
                * gfx_widget_set_libretro_message(). We need to
                * implement a separate 'progress bar' widget to
                * handle these correctly */
               case RETRO_MESSAGE_TYPE_PROGRESS:

                  if (gfx_widgets_active())
                     gfx_widget_set_libretro_message(msg->msg, msg->duration);
                  else
                     runloop_core_msg_queue_push(msg);

                  break;
#endif
               /* Handle standard (queued) notifications */
               case RETRO_MESSAGE_TYPE_NOTIFICATION:
               default:
                  runloop_core_msg_queue_push(msg);
                  break;
            }
         }

         break;
      }

      case RETRO_ENVIRONMENT_SET_ROTATION:
      {
         unsigned rotation       = *(const unsigned*)data;
         bool video_allow_rotate = settings->bools.video_allow_rotate;

         RARCH_LOG("[Environ]: SET_ROTATION: %u\n", rotation);
         if (!video_allow_rotate)
            break;

         if (system)
            system->rotation = rotation;

         if (!video_driver_set_rotation(rotation))
            return false;
         break;
      }

      case RETRO_ENVIRONMENT_SHUTDOWN:
         RARCH_LOG("[Environ]: SHUTDOWN.\n");

         /* This case occurs when a core (internally) requests
          * a shutdown event. Must save runtime log file here,
          * since normal command.c CMD_EVENT_CORE_DEINIT event
          * will not occur until after the current content has
          * been cleared (causing log to be skipped) */
         command_event_runtime_log_deinit();

         p_rarch->runloop_shutdown_initiated      = true;
         p_rarch->runloop_core_shutdown_initiated = true;
         break;

      case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
         if (system)
         {
            system->performance_level = *(const unsigned*)data;
            RARCH_LOG("[Environ]: PERFORMANCE_LEVEL: %u.\n",
                  system->performance_level);
         }
         break;

      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
         {
            const char *dir_system          = settings->paths.directory_system;
            bool systemfiles_in_content_dir = settings->bools.systemfiles_in_content_dir;
            if (string_is_empty(dir_system) || systemfiles_in_content_dir)
            {
               const char *fullpath = path_get(RARCH_PATH_CONTENT);
               if (!string_is_empty(fullpath))
               {
                  size_t path_size = PATH_MAX_LENGTH * sizeof(char);
                  char *temp_path  = (char*)malloc(PATH_MAX_LENGTH 
                        * sizeof(char));
                  temp_path[0]     = '\0';

                  if (string_is_empty(dir_system))
                     RARCH_WARN("SYSTEM DIR is empty, assume CONTENT DIR %s\n",
                           fullpath);
                  fill_pathname_basedir(temp_path, fullpath, path_size);
                  dir_set(RARCH_DIR_SYSTEM, temp_path);
                  free(temp_path);
               }

               *(const char**)data = dir_get_ptr(RARCH_DIR_SYSTEM);
               RARCH_LOG("[Environ]: SYSTEM_DIRECTORY: \"%s\".\n",
                     dir_system);
            }
            else
            {
               *(const char**)data = dir_system;
               RARCH_LOG("[Environ]: SYSTEM_DIRECTORY: \"%s\".\n",
                     dir_system);
            }
         }
         break;

      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         *(const char**)data = p_rarch->current_savefile_dir;
         break;

      case RETRO_ENVIRONMENT_GET_USERNAME:
         *(const char**)data = *settings->paths.username ?
            settings->paths.username : NULL;
         RARCH_LOG("[Environ]: GET_USERNAME: \"%s\".\n",
               settings->paths.username);
         break;

      case RETRO_ENVIRONMENT_GET_LANGUAGE:
#ifdef HAVE_LANGEXTRA
         {
            unsigned user_lang = *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);
            *(unsigned *)data  = user_lang;
            RARCH_LOG("[Environ]: GET_LANGUAGE: \"%u\".\n", user_lang);
         }
#endif
         break;

      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      {
         enum retro_pixel_format pix_fmt =
            *(const enum retro_pixel_format*)data;

         switch (pix_fmt)
         {
            case RETRO_PIXEL_FORMAT_0RGB1555:
               RARCH_LOG("[Environ]: SET_PIXEL_FORMAT: 0RGB1555.\n");
               break;

            case RETRO_PIXEL_FORMAT_RGB565:
               RARCH_LOG("[Environ]: SET_PIXEL_FORMAT: RGB565.\n");
               break;
            case RETRO_PIXEL_FORMAT_XRGB8888:
               RARCH_LOG("[Environ]: SET_PIXEL_FORMAT: XRGB8888.\n");
               break;
            default:
               return false;
         }

         p_rarch->video_driver_pix_fmt = pix_fmt;
         break;
      }

      case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
      {
         static const char *libretro_btn_desc[]    = {
            "B (bottom)", "Y (left)", "Select", "Start",
            "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
            "A (right)", "X (up)",
            "L", "R", "L2", "R2", "L3", "R3",
         };

         if (system)
         {
            unsigned retro_id;
            const struct retro_input_descriptor *desc = NULL;
            memset((void*)&system->input_desc_btn, 0,
                  sizeof(system->input_desc_btn));

            desc = (const struct retro_input_descriptor*)data;

            for (; desc->description; desc++)
            {
               unsigned retro_port = desc->port;

               retro_id            = desc->id;

               if (desc->port >= MAX_USERS)
                  continue;

               /* Ignore all others for now. */
               if (
                     desc->device != RETRO_DEVICE_JOYPAD  &&
                     desc->device != RETRO_DEVICE_ANALOG)
                  continue;

               if (desc->id >= RARCH_FIRST_CUSTOM_BIND)
                  continue;

               if (desc->device == RETRO_DEVICE_ANALOG)
               {
                  switch (retro_id)
                  {
                     case RETRO_DEVICE_ID_ANALOG_X:
                        switch (desc->index)
                        {
                           case RETRO_DEVICE_INDEX_ANALOG_LEFT:
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_LEFT_X_PLUS]  = desc->description;
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_LEFT_X_MINUS] = desc->description;
                              break;
                           case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_RIGHT_X_PLUS] = desc->description;
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_RIGHT_X_MINUS] = desc->description;
                              break;
                        }
                        break;
                     case RETRO_DEVICE_ID_ANALOG_Y:
                        switch (desc->index)
                        {
                           case RETRO_DEVICE_INDEX_ANALOG_LEFT:
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_LEFT_Y_PLUS] = desc->description;
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_LEFT_Y_MINUS] = desc->description;
                              break;
                           case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_RIGHT_Y_PLUS] = desc->description;
                              system->input_desc_btn[retro_port]
                                 [RARCH_ANALOG_RIGHT_Y_MINUS] = desc->description;
                              break;
                        }
                        break;
                  }
               }
               else
                  system->input_desc_btn[retro_port]
                     [retro_id] = desc->description;
            }

            RARCH_LOG("[Environ]: SET_INPUT_DESCRIPTORS:\n");

            {
               unsigned log_level      = settings->uints.libretro_log_level;

               if (log_level == RETRO_LOG_DEBUG)
               {
                  unsigned input_driver_max_users = 
                     p_rarch->input_driver_max_users;
                  for (p = 0; p < input_driver_max_users; p++)
                  {
                     for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND; retro_id++)
                     {
                        const char *description = system->input_desc_btn[p][retro_id];

                        if (!description)
                           continue;

                        RARCH_LOG("\tRetroPad, Port %u, Button \"%s\" => \"%s\"\n",
                              p + 1, libretro_btn_desc[retro_id], description);
                     }
                  }
               }
            }

            p_rarch->current_core.has_set_input_descriptors = true;
         }

         break;
      }

      case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
      {
         const struct retro_keyboard_callback *info =
            (const struct retro_keyboard_callback*)data;
         retro_keyboard_event_t *frontend_key_event = &p_rarch->runloop_frontend_key_event;
         retro_keyboard_event_t *key_event          = &p_rarch->runloop_key_event;

         RARCH_LOG("[Environ]: SET_KEYBOARD_CALLBACK.\n");
         if (key_event)
            *key_event                  = info->callback;

         if (frontend_key_event && key_event)
            *frontend_key_event         = *key_event;
         break;
      }

      case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION:
         /* Current API version is 1 */
         *(unsigned *)data = 1;
         break;

      case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
         {
            const struct retro_disk_control_callback *control_cb =
                  (const struct retro_disk_control_callback*)data;

            if (system)
            {
               RARCH_LOG("[Environ]: SET_DISK_CONTROL_INTERFACE.\n");
               disk_control_set_callback(&system->disk_control, control_cb);
            }
         }
         break;

      case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
         {
            const struct retro_disk_control_ext_callback *control_cb =
                  (const struct retro_disk_control_ext_callback*)data;

            if (system)
            {
               RARCH_LOG("[Environ]: SET_DISK_CONTROL_EXT_INTERFACE.\n");
               disk_control_set_ext_callback(&system->disk_control, control_cb);
            }
         }
         break;

      case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
      {
         unsigned *cb = (unsigned*)data;
         settings_t *settings          = p_rarch->configuration_settings;
         const char *video_driver_name = settings->arrays.video_driver;
         bool driver_switch_enable     = settings->bools.driver_switch_enable;

         RARCH_LOG("[Environ]: GET_PREFERRED_HW_RENDER.\n");

         if (!driver_switch_enable)
             return false;

         if (string_is_equal(video_driver_name, "glcore"))
             *cb = RETRO_HW_CONTEXT_OPENGL_CORE;
         else if (string_is_equal(video_driver_name, "gl"))
             *cb = RETRO_HW_CONTEXT_OPENGL;
         else if (string_is_equal(video_driver_name, "vulkan"))
             *cb = RETRO_HW_CONTEXT_VULKAN;
         else if (!strncmp(video_driver_name, "d3d", 3))
             *cb = RETRO_HW_CONTEXT_DIRECT3D;
         else
             *cb = RETRO_HW_CONTEXT_NONE;
         break;
      }

      case RETRO_ENVIRONMENT_SET_HW_RENDER:
      case RETRO_ENVIRONMENT_SET_HW_RENDER | RETRO_ENVIRONMENT_EXPERIMENTAL:
      {
         struct retro_hw_render_callback *cb =
            (struct retro_hw_render_callback*)data;
         struct retro_hw_render_callback *hwr =
            video_driver_get_hw_context_internal();

         RARCH_LOG("[Environ]: SET_HW_RENDER.\n");

         if (!dynamic_request_hw_context(
                  cb->context_type, cb->version_minor, cb->version_major))
            return false;

         if (!dynamic_verify_hw_context(
                  cb->context_type, cb->version_minor, cb->version_major))
            return false;

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)
         if (!gl_set_core_context(cb->context_type)) { }
#endif

         cb->get_current_framebuffer = video_driver_get_current_framebuffer;
         cb->get_proc_address        = video_driver_get_proc_address;

         /* Old ABI. Don't copy garbage. */
         if (cmd & RETRO_ENVIRONMENT_EXPERIMENTAL)
         {
            memcpy(hwr,
                  cb, offsetof(struct retro_hw_render_callback, stencil));
            memset((uint8_t*)hwr + offsetof(struct retro_hw_render_callback, stencil),
               0, sizeof(*cb) - offsetof(struct retro_hw_render_callback, stencil));
         }
         else
            memcpy(hwr, cb, sizeof(*cb));
         break;
      }

      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
      {
         bool state = *(const bool*)data;
         RARCH_LOG("[Environ]: SET_SUPPORT_NO_GAME: %s.\n", state ? "yes" : "no");

         if (state)
            content_set_does_not_need_content();
         else
            content_unset_does_not_need_content();
         break;
      }

      case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
      {
         const char **path = (const char**)data;
#ifdef HAVE_DYNAMIC
         *path = path_get(RARCH_PATH_CORE);
#else
         *path = NULL;
#endif
         break;
      }

      case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK:
#ifdef HAVE_THREADS
      {
         const struct retro_audio_callback *cb = (const struct retro_audio_callback*)data;
         RARCH_LOG("[Environ]: SET_AUDIO_CALLBACK.\n");
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            return false;
#endif
         if (p_rarch->recording_data) /* A/V sync is a must. */
            return false;
         if (cb)
            p_rarch->audio_callback = *cb;
	 return true;
      }
#endif
      return false;

      case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
      {
         const struct retro_frame_time_callback *info =
            (const struct retro_frame_time_callback*)data;

         RARCH_LOG("[Environ]: SET_FRAME_TIME_CALLBACK.\n");
#ifdef HAVE_NETWORKING
         /* retro_run() will be called in very strange and
          * mysterious ways, have to disable it. */
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            return false;
#endif
         p_rarch->runloop_frame_time = *info;
         break;
      }

      case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
      {
         struct retro_rumble_interface *iface =
            (struct retro_rumble_interface*)data;

         RARCH_LOG("[Environ]: GET_RUMBLE_INTERFACE.\n");
         iface->set_rumble_state = input_driver_set_rumble_state;
         break;
      }

      case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
      {
         uint64_t *mask = (uint64_t*)data;

         RARCH_LOG("[Environ]: GET_INPUT_DEVICE_CAPABILITIES.\n");
         if (!p_rarch->current_input->get_capabilities || !p_rarch->current_input_data)
            return false;
         *mask = input_driver_get_capabilities();
         break;
      }

      case RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE:
      {
         struct retro_sensor_interface *iface =
            (struct retro_sensor_interface*)data;

         RARCH_LOG("[Environ]: GET_SENSOR_INTERFACE.\n");
         iface->set_sensor_state = input_sensor_set_state;
         iface->get_sensor_input = input_sensor_get_input;
         break;
      }
      case RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE:
      {
         struct retro_camera_callback *cb =
            (struct retro_camera_callback*)data;

         RARCH_LOG("[Environ]: GET_CAMERA_INTERFACE.\n");
         cb->start                        = driver_camera_start;
         cb->stop                         = driver_camera_stop;

         p_rarch->camera_cb               = *cb;

         if (cb->caps != 0)
            p_rarch->camera_driver_active = true;
         else
            p_rarch->camera_driver_active = false;
         break;
      }

      case RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE:
      {
         struct retro_location_callback *cb =
            (struct retro_location_callback*)data;

         RARCH_LOG("[Environ]: GET_LOCATION_INTERFACE.\n");
         cb->start                       = driver_location_start;
         cb->stop                        = driver_location_stop;
         cb->get_position                = driver_location_get_position;
         cb->set_interval                = driver_location_set_interval;

         if (system)
            system->location_cb          = *cb;

         p_rarch->location_driver_active = false;
         break;
      }

      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
      {
         struct retro_log_callback *cb = (struct retro_log_callback*)data;

         RARCH_LOG("[Environ]: GET_LOG_INTERFACE.\n");
         cb->log = rarch_log_libretro;
         break;
      }

      case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
      {
         struct retro_perf_callback *cb = (struct retro_perf_callback*)data;

         RARCH_LOG("[Environ]: GET_PERF_INTERFACE.\n");
         cb->get_time_usec    = cpu_features_get_time_usec;
         cb->get_cpu_features = cpu_features_get;
         cb->get_perf_counter = cpu_features_get_perf_counter;

         cb->perf_register    = performance_counter_register;
         cb->perf_start       = core_performance_counter_start;
         cb->perf_stop        = core_performance_counter_stop;
         cb->perf_log         = retro_perf_log;
         break;
      }

      case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
      {
         const char **dir            = (const char**)data;
         const char *dir_core_assets = settings->paths.directory_core_assets;

         *dir = *dir_core_assets ?
            dir_core_assets : NULL;
         RARCH_LOG("[Environ]: CORE_ASSETS_DIRECTORY: \"%s\".\n",
               dir_core_assets);
         break;
      }

      case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
      /**
       * Update the system Audio/Video information.
       * Will reinitialize audio/video drivers if needed.
       * Used by RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO.
       **/
      {
         const struct retro_system_av_info **info = (const struct retro_system_av_info**)&data;
         struct retro_system_av_info *av_info     = &p_rarch->video_driver_av_info;
         if (data)
         {
            settings_t *settings                  = p_rarch->configuration_settings;
            unsigned crt_switch_resolution        = settings->uints.crt_switch_resolution;
            bool video_fullscreen                 = settings->bools.video_fullscreen;
            const bool no_video_reinit            = (
                     crt_switch_resolution == 0 
                  && data
                  && ((*info)->geometry.max_width  == av_info->geometry.max_width)
                  && ((*info)->geometry.max_height == av_info->geometry.max_height));
            /* When not doing video reinit, we also must not do input and menu
             * reinit, otherwise the input driver crashes and the menu gets
             * corrupted. */
            int reinit_flags = no_video_reinit ?
                    DRIVERS_CMD_ALL & ~(DRIVER_VIDEO_MASK | DRIVER_INPUT_MASK | DRIVER_MENU_MASK)
                  : DRIVERS_CMD_ALL;

            RARCH_LOG("[Environ]: SET_SYSTEM_AV_INFO.\n");

            memcpy(av_info, *info, sizeof(*av_info));
            command_event(CMD_EVENT_REINIT, &reinit_flags);
            if (no_video_reinit)
               video_driver_set_aspect_ratio();

            /* Cannot continue recording with different parameters.
             * Take the easiest route out and just restart the recording. */
            if (p_rarch->recording_data)
            {
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT),
                     2, 180, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               command_event(CMD_EVENT_RECORD_DEINIT, NULL);
               command_event(CMD_EVENT_RECORD_INIT, NULL);
            }

            /* Hide mouse cursor in fullscreen after
             * a RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO call. */
            if (video_fullscreen)
               video_driver_hide_mouse();

            return true;
         }
         return false;
      }

      case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
      {
         unsigned i;
         const struct retro_subsystem_info *info =
            (const struct retro_subsystem_info*)data;
         unsigned log_level   = settings->uints.libretro_log_level;

         if (log_level == RETRO_LOG_DEBUG)
            RARCH_LOG("[Environ]: SET_SUBSYSTEM_INFO.\n");

         for (i = 0; info[i].ident; i++)
         {
            unsigned j;
            if (log_level != RETRO_LOG_DEBUG)
               continue;

            RARCH_LOG("Special game type: %s\n  Ident: %s\n  ID: %u\n  Content:\n",
                  info[i].desc,
                  info[i].ident,
                  info[i].id
                  );
            for (j = 0; j < info[i].num_roms; j++)
            {
               RARCH_LOG("    %s (%s)\n",
                     info[i].roms[j].desc, info[i].roms[j].required ?
                     "required" : "optional");
            }
         }

         if (system)
         {
            struct retro_subsystem_info *info_ptr = NULL;
            free(system->subsystem.data);
            system->subsystem.data = NULL;
            system->subsystem.size = 0;

            info_ptr = (struct retro_subsystem_info*)
               malloc(i * sizeof(*info_ptr));

            if (!info_ptr)
               return false;

            system->subsystem.data = info_ptr;

            memcpy(system->subsystem.data, info,
                  i * sizeof(*system->subsystem.data));
            system->subsystem.size                   = i;
            p_rarch->current_core.has_set_subsystems = true;
         }
         break;
      }

      case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
      {
         unsigned i, j;
         const struct retro_controller_info *info =
            (const struct retro_controller_info*)data;
         unsigned log_level      = settings->uints.libretro_log_level;

         RARCH_LOG("[Environ]: SET_CONTROLLER_INFO.\n");

         for (i = 0; info[i].types; i++)
         {
            if (log_level != RETRO_LOG_DEBUG)
               continue;

            RARCH_LOG("Controller port: %u\n", i + 1);
            for (j = 0; j < info[i].num_types; j++)
               RARCH_LOG("   %s (ID: %u)\n", info[i].types[j].desc,
                     info[i].types[j].id);
         }

         if (system)
         {
            struct retro_controller_info *info_ptr = NULL;

            free(system->ports.data);
            system->ports.data = NULL;
            system->ports.size = 0;

            info_ptr = (struct retro_controller_info*)calloc(i, sizeof(*info_ptr));
            if (!info_ptr)
               return false;

            system->ports.data = info_ptr;
            memcpy(system->ports.data, info,
                  i * sizeof(*system->ports.data));
            system->ports.size = i;
         }
         break;
      }

      case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
      {
         if (system)
         {
            unsigned i;
            const struct retro_memory_map *mmaps        =
               (const struct retro_memory_map*)data;
            rarch_memory_descriptor_t *descriptors = NULL;

            RARCH_LOG("[Environ]: SET_MEMORY_MAPS.\n");
            free((void*)system->mmaps.descriptors);
            system->mmaps.descriptors     = 0;
            system->mmaps.num_descriptors = 0;
            descriptors = (rarch_memory_descriptor_t*)
               calloc(mmaps->num_descriptors,
                     sizeof(*descriptors));

            if (!descriptors)
               return false;

            system->mmaps.descriptors     = descriptors;
            system->mmaps.num_descriptors = mmaps->num_descriptors;

            for (i = 0; i < mmaps->num_descriptors; i++)
               system->mmaps.descriptors[i].core = mmaps->descriptors[i];

            mmap_preprocess_descriptors(descriptors, mmaps->num_descriptors);

            if (sizeof(void *) == 8)
               RARCH_LOG("   ndx flags  ptr              offset   start    select   disconn  len      addrspace\n");
            else
               RARCH_LOG("   ndx flags  ptr          offset   start    select   disconn  len      addrspace\n");

            for (i = 0; i < system->mmaps.num_descriptors; i++)
            {
               const rarch_memory_descriptor_t *desc =
                  &system->mmaps.descriptors[i];
               char flags[7];

               flags[0] = 'M';
               if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_8) == RETRO_MEMDESC_MINSIZE_8)
                  flags[1] = '8';
               else if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_4) == RETRO_MEMDESC_MINSIZE_4)
                  flags[1] = '4';
               else if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_2) == RETRO_MEMDESC_MINSIZE_2)
                  flags[1] = '2';
               else
                  flags[1] = '1';

               flags[2] = 'A';
               if ((desc->core.flags & RETRO_MEMDESC_ALIGN_8) == RETRO_MEMDESC_ALIGN_8)
                  flags[3] = '8';
               else if ((desc->core.flags & RETRO_MEMDESC_ALIGN_4) == RETRO_MEMDESC_ALIGN_4)
                  flags[3] = '4';
               else if ((desc->core.flags & RETRO_MEMDESC_ALIGN_2) == RETRO_MEMDESC_ALIGN_2)
                  flags[3] = '2';
               else
                  flags[3] = '1';

               flags[4] = (desc->core.flags & RETRO_MEMDESC_BIGENDIAN) ? 'B' : 'b';
               flags[5] = (desc->core.flags & RETRO_MEMDESC_CONST) ? 'C' : 'c';
               flags[6] = 0;

               RARCH_LOG("   %03u %s %p %08X %08X %08X %08X %08X %s\n",
                     i + 1, flags, desc->core.ptr, desc->core.offset, desc->core.start,
                     desc->core.select, desc->core.disconnect, desc->core.len,
                     desc->core.addrspace ? desc->core.addrspace : "");
            }
         }
         else
         {
            RARCH_WARN("[Environ]: SET_MEMORY_MAPS, but system pointer not initialized..\n");
         }

         break;
      }

      case RETRO_ENVIRONMENT_SET_GEOMETRY:
      {
         struct retro_system_av_info *av_info      = &p_rarch->video_driver_av_info;
         struct retro_game_geometry  *geom         = (struct retro_game_geometry*)&av_info->geometry;
         const struct retro_game_geometry *in_geom = (const struct retro_game_geometry*)data;

         if (!geom)
            return false;

         /* Can potentially be called every frame,
          * don't do anything unless required. */
         if (  (geom->base_width   != in_geom->base_width)  ||
               (geom->base_height  != in_geom->base_height) ||
               (geom->aspect_ratio != in_geom->aspect_ratio))
         {
            geom->base_width   = in_geom->base_width;
            geom->base_height  = in_geom->base_height;
            geom->aspect_ratio = in_geom->aspect_ratio;

            RARCH_LOG("SET_GEOMETRY: %ux%u, aspect: %.3f.\n",
                  geom->base_width, geom->base_height, geom->aspect_ratio);

            /* Forces recomputation of aspect ratios if
             * using core-dependent aspect ratios. */
            video_driver_set_aspect_ratio();

            /* TODO: Figure out what to do, if anything, with recording. */
         }
         else
         {
            RARCH_LOG("[Environ]: SET_GEOMETRY.\n");

         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER:
      {
         struct retro_framebuffer *fb = (struct retro_framebuffer*)data;
         if (
                  p_rarch->video_driver_poke
               && p_rarch->video_driver_poke->get_current_software_framebuffer
               && p_rarch->video_driver_poke->get_current_software_framebuffer(
                  p_rarch->video_driver_data, fb))
            return true;

         return false;
      }

      case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE:
      {
         const struct retro_hw_render_interface **iface = (const struct retro_hw_render_interface **)data;
         if (
                  p_rarch->video_driver_poke
               && p_rarch->video_driver_poke->get_hw_render_interface
               && p_rarch->video_driver_poke->get_hw_render_interface(
                  p_rarch->video_driver_data, iface))
            return true;

         return false;
      }

      case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
#ifdef HAVE_CHEEVOS
         {
            bool state = *(const bool*)data;
            RARCH_LOG("[Environ]: SET_SUPPORT_ACHIEVEMENTS: %s.\n", state ? "yes" : "no");
            rcheevos_set_support_cheevos(state);
         }
#endif
         break;

      case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE:
      {
         const struct retro_hw_render_context_negotiation_interface *iface =
            (const struct retro_hw_render_context_negotiation_interface*)data;
         RARCH_LOG("[Environ]: SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE.\n");
         p_rarch->hw_render_context_negotiation = iface;
         break;
      }

      case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
      {
         uint64_t *quirks = (uint64_t *) data;
         p_rarch->current_core.serialization_quirks_v = *quirks;
         break;
      }

      case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT:
#ifdef HAVE_LIBNX
         /* TODO/FIXME - Force this off for now for Switch
          * until shared HW context can work there */
         return false;
#else
         p_rarch->core_set_shared_context = true;
#endif
         break;

      case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
      {
         const uint32_t supported_vfs_version = 3;
         static struct retro_vfs_interface vfs_iface =
         {
            /* VFS API v1 */
            retro_vfs_file_get_path_impl,
            retro_vfs_file_open_impl,
            retro_vfs_file_close_impl,
            retro_vfs_file_size_impl,
            retro_vfs_file_tell_impl,
            retro_vfs_file_seek_impl,
            retro_vfs_file_read_impl,
            retro_vfs_file_write_impl,
            retro_vfs_file_flush_impl,
            retro_vfs_file_remove_impl,
            retro_vfs_file_rename_impl,
            /* VFS API v2 */
            retro_vfs_file_truncate_impl,
            /* VFS API v3 */
            retro_vfs_stat_impl,
            retro_vfs_mkdir_impl,
            retro_vfs_opendir_impl,
            retro_vfs_readdir_impl,
            retro_vfs_dirent_get_name_impl,
            retro_vfs_dirent_is_dir_impl,
            retro_vfs_closedir_impl
         };

         struct retro_vfs_interface_info *vfs_iface_info = (struct retro_vfs_interface_info *) data;
         if (vfs_iface_info->required_interface_version <= supported_vfs_version)
         {
            RARCH_LOG("Core requested VFS version >= v%d, providing v%d\n", vfs_iface_info->required_interface_version, supported_vfs_version);
            vfs_iface_info->required_interface_version = supported_vfs_version;
            vfs_iface_info->iface                      = &vfs_iface;
            system->supports_vfs = true;
         }
         else
         {
            RARCH_WARN("Core requested VFS version v%d which is higher than what we support (v%d)\n", vfs_iface_info->required_interface_version, supported_vfs_version);
            return false;
         }

         break;
      }

      case RETRO_ENVIRONMENT_GET_LED_INTERFACE:
      {
         struct retro_led_interface *ledintf =
            (struct retro_led_interface *)data;
         if (ledintf)
            ledintf->set_led_state = led_driver_set_led;
      }
      break;

      case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
      {
         int result = 0;
         if ( !p_rarch->audio_suspended && 
               p_rarch->audio_driver_active)
            result |= 2;
         if (p_rarch->video_driver_active 
               && !(p_rarch->current_video->frame == video_null.frame))
            result |= 1;
#ifdef HAVE_RUNAHEAD
         if (p_rarch->request_fast_savestate)
            result |= 4;
         if (p_rarch->hard_disable_audio)
            result |= 8;
#endif
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_REPLAYING, NULL))
            result &= ~(1|2);
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            result |= 4;
#endif
         if (data)
         {
            int* result_p = (int*)data;
            *result_p = result;
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_MIDI_INTERFACE:
      {
         struct retro_midi_interface *midi_interface =
               (struct retro_midi_interface *)data;

         if (midi_interface)
         {
            midi_interface->input_enabled  = midi_driver_input_enabled;
            midi_interface->output_enabled = midi_driver_output_enabled;
            midi_interface->read           = midi_driver_read;
            midi_interface->write          = midi_driver_write;
            midi_interface->flush          = midi_driver_flush;
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
         *(bool *)data = p_rarch->runloop_fastmotion;
         break;

      case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
         /* Just falldown, the function will return true */
         break;

      case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
         /* Current API version is 1 */
         *(unsigned *)data = 1;
         break;

      case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
      {
         /* Try to use the polled refresh rate first.  */
         float target_refresh_rate = video_driver_get_refresh_rate();
         float video_refresh_rate  = settings ? settings->floats.video_refresh_rate : 0.0;

         /* If the above function failed [possibly because it is not
          * implemented], use the refresh rate set in the config instead. */
         if (target_refresh_rate == 0.0 && video_refresh_rate != 0.0)
            target_refresh_rate = video_refresh_rate;

         *(float *)data = target_refresh_rate;
         break;
      }

      /* Private environment callbacks.
       *
       * Should all be properly addressed in version 2.
       * */

      case RETRO_ENVIRONMENT_POLL_TYPE_OVERRIDE:
         {
            const unsigned *poll_type_data = (const unsigned*)data;

            if (poll_type_data)
               p_rarch->core_poll_type_override = (enum poll_type_override_t)*poll_type_data;
         }
         break;

      case RETRO_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB:
         *(retro_environment_t *)data = rarch_clear_all_thread_waits;
         break;

      case RETRO_ENVIRONMENT_SET_SAVE_STATE_IN_BACKGROUND:
         {
            bool state = *(const bool*)data;
            RARCH_LOG("[Environ]: SET_SAVE_STATE_IN_BACKGROUND: %s.\n", state ? "yes" : "no");

            set_save_state_in_background(state);

         }
      break;


      default:
         RARCH_LOG("[Environ]: UNSUPPORTED (#%u).\n", cmd);
         return false;
   }

   return true;
}

#ifdef HAVE_DYNAMIC
/**
 * libretro_get_environment_info:
 * @func                         : Function pointer for get_environment_info.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Sets environment callback in order to get statically known
 * information from it.
 *
 * Fetched via environment callbacks instead of
 * retro_get_system_info(), as this info is part of extensions.
 *
 * Should only be called once right after core load to
 * avoid overwriting the "real" environ callback.
 *
 * For statically linked cores, pass retro_set_environment as argument.
 */
static void libretro_get_environment_info(void (*func)(retro_environment_t),
      bool *load_no_content)
{
   struct rarch_state *p_rarch   = &rarch_st;

   p_rarch->load_no_content_hook = load_no_content;

   /* load_no_content gets set in this callback. */
   func(environ_cb_get_system_info);

   /* It's possible that we just set get_system_info callback
    * to the currently running core.
    *
    * Make sure we reset it to the actual environment callback.
    * Ignore any environment callbacks here in case we're running
    * on the non-current core. */
   p_rarch->ignore_environment_cb = true;
   func(rarch_environment_cb);
   p_rarch->ignore_environment_cb = false;
}

static bool load_dynamic_core(const char *path, char *buf, size_t size)
{
   struct rarch_state *p_rarch   = &rarch_st;

   /* Can't lookup symbols in itself on UWP */
#if !(defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
   if (dylib_proc(NULL, "retro_init"))
   {
      /* Try to verify that -lretro was not linked in from other modules
       * since loading it dynamically and with -l will fail hard. */
      RARCH_ERR("Serious problem. RetroArch wants to load libretro cores"
            " dynamically, but it is already linked.\n");
      RARCH_ERR("This could happen if other modules RetroArch depends on "
            "link against libretro directly.\n");
      RARCH_ERR("Proceeding could cause a crash. Aborting ...\n");
      retroarch_fail(1, "init_libretro_symbols()");
   }
#endif

   /* Need to use absolute path for this setting. It can be
    * saved to content history, and a relative path would
    * break in that scenario. */
   path_resolve_realpath(buf, size, true);
   if ((p_rarch->lib_handle = dylib_load(path)))
      return true;
   return false;
}

static dylib_t libretro_get_system_info_lib(const char *path,
      struct retro_system_info *info, bool *load_no_content)
{
   dylib_t lib = dylib_load(path);
   void (*proc)(struct retro_system_info*);

   if (!lib)
      return NULL;

   proc = (void (*)(struct retro_system_info*))
      dylib_proc(lib, "retro_get_system_info");

   if (!proc)
   {
      dylib_close(lib);
      return NULL;
   }

   proc(info);

   if (load_no_content)
   {
      void (*set_environ)(retro_environment_t);
      *load_no_content = false;
      set_environ = (void (*)(retro_environment_t))
         dylib_proc(lib, "retro_set_environment");

      if (set_environ)
         libretro_get_environment_info(set_environ, load_no_content);
   }

   return lib;
}
#endif

/**
 * libretro_get_system_info:
 * @path                         : Path to libretro library.
 * @info                         : Pointer to system info information.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Gets system info from an arbitrary lib.
 * The struct returned must be freed as strings are allocated dynamically.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool libretro_get_system_info(const char *path,
      struct retro_system_info *info, bool *load_no_content)
{
   struct retro_system_info dummy_info;
#ifdef HAVE_DYNAMIC
   dylib_t lib;
#endif
   struct rarch_state *p_rarch  = &rarch_st;

   dummy_info.library_name      = NULL;
   dummy_info.library_version   = NULL;
   dummy_info.valid_extensions  = NULL;
   dummy_info.need_fullpath     = false;
   dummy_info.block_extract     = false;

#ifdef HAVE_DYNAMIC
   lib                         = libretro_get_system_info_lib(
         path, &dummy_info, load_no_content);

   if (!lib)
   {
      RARCH_ERR("%s: \"%s\"\n",
            msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE),
            path);
      RARCH_ERR("Error(s): %s\n", dylib_error());
      return false;
   }
#else
   if (load_no_content)
   {
      p_rarch->load_no_content_hook = load_no_content;

      /* load_no_content gets set in this callback. */
      retro_set_environment(environ_cb_get_system_info);

      /* It's possible that we just set get_system_info callback
       * to the currently running core.
       *
       * Make sure we reset it to the actual environment callback.
       * Ignore any environment callbacks here in case we're running
       * on the non-current core. */
      p_rarch->ignore_environment_cb = true;
      retro_set_environment(rarch_environment_cb);
      p_rarch->ignore_environment_cb = false;
   }

   retro_get_system_info(&dummy_info);
#endif

   memcpy(info, &dummy_info, sizeof(*info));

   p_rarch->current_library_name[0]    = '\0';
   p_rarch->current_library_version[0] = '\0';
   p_rarch->current_valid_extensions[0] = '\0';

   if (!string_is_empty(dummy_info.library_name))
      strlcpy(p_rarch->current_library_name,
            dummy_info.library_name, sizeof(p_rarch->current_library_name));
   if (!string_is_empty(dummy_info.library_version))
      strlcpy(p_rarch->current_library_version,
            dummy_info.library_version, sizeof(p_rarch->current_library_version));
   if (dummy_info.valid_extensions)
      strlcpy(p_rarch->current_valid_extensions,
            dummy_info.valid_extensions, sizeof(p_rarch->current_valid_extensions));

   info->library_name     = p_rarch->current_library_name;
   info->library_version  = p_rarch->current_library_version;
   info->valid_extensions = p_rarch->current_valid_extensions;

#ifdef HAVE_DYNAMIC
   dylib_close(lib);
#endif
   return true;
}

/**
 * load_symbols:
 * @type                        : Type of core to be loaded.
 *                                If CORE_TYPE_DUMMY, will
 *                                load dummy symbols.
 *
 * Setup libretro callback symbols. Returns true on success,
 * or false if symbols could not be loaded.
 **/
static bool init_libretro_symbols_custom(enum rarch_core_type type,
      struct retro_core_t *current_core, const char *lib_path, void *_lib_handle_p)
{
#ifdef HAVE_DYNAMIC
   /* the library handle for use with the SYMBOL macro */
   dylib_t lib_handle_local;
   struct rarch_state *p_rarch = &rarch_st;
#endif

   switch (type)
   {
      case CORE_TYPE_PLAIN:
         {
#ifdef HAVE_DYNAMIC
#ifdef HAVE_RUNAHEAD
            dylib_t *lib_handle_p = (dylib_t*)_lib_handle_p;
            if (!lib_path || !lib_handle_p)
#endif
            {
               const char *path = path_get(RARCH_PATH_CORE);

               if (string_is_empty(path))
               {
                  RARCH_ERR("Frontend is built for dynamic libretro cores, but "
                        "path is not set. Cannot continue.\n");
                  retroarch_fail(1, "init_libretro_symbols()");
               }

               RARCH_LOG("Loading dynamic libretro core from: \"%s\"\n",
                     path);

               if (!load_dynamic_core(
                        path,
                        path_get_ptr(RARCH_PATH_CORE),
                        path_get_realsize(RARCH_PATH_CORE)
                        ))
               {
                  RARCH_ERR("Failed to open libretro core: \"%s\"\nError(s): %s\n", path, dylib_error());
                  runloop_msg_queue_push(msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE),
                        1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  return false;
               }
               lib_handle_local = p_rarch->lib_handle;
            }
#ifdef HAVE_RUNAHEAD
            else
            {
               /* for a secondary core, we already have a
                * primary library loaded, so we can skip
                * some checks and just load the library */
               retro_assert(lib_path != NULL && lib_handle_p != NULL);
               lib_handle_local = dylib_load(lib_path);

               if (!lib_handle_local)
                  return false;
               *lib_handle_p = lib_handle_local;
            }
#endif
#endif

            CORE_SYMBOLS(SYMBOL);
         }
         break;
      case CORE_TYPE_DUMMY:
         CORE_SYMBOLS(SYMBOL_DUMMY);
         break;
      case CORE_TYPE_FFMPEG:
#ifdef HAVE_FFMPEG
         CORE_SYMBOLS(SYMBOL_FFMPEG);
#endif
         break;
      case CORE_TYPE_MPV:
#ifdef HAVE_MPV
         CORE_SYMBOLS(SYMBOL_MPV);
#endif
         break;
      case CORE_TYPE_IMAGEVIEWER:
#ifdef HAVE_IMAGEVIEWER
         CORE_SYMBOLS(SYMBOL_IMAGEVIEWER);
#endif
         break;
      case CORE_TYPE_NETRETROPAD:
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
         CORE_SYMBOLS(SYMBOL_NETRETROPAD);
#endif
         break;
      case CORE_TYPE_VIDEO_PROCESSOR:
#if defined(HAVE_VIDEOPROCESSOR)
         CORE_SYMBOLS(SYMBOL_VIDEOPROCESSOR);
#endif
         break;
      case CORE_TYPE_GONG:
#ifdef HAVE_GONG
         CORE_SYMBOLS(SYMBOL_GONG);
#endif
         break;
   }

   return true;
}

/**
 * init_libretro_symbols:
 * @type                        : Type of core to be loaded.
 *                                If CORE_TYPE_DUMMY, will
 *                                load dummy symbols.
 *
 * Initializes libretro symbols and
 * setups environment callback functions. Returns true on success,
 * or false if symbols could not be loaded.
 **/
static bool init_libretro_symbols(enum rarch_core_type type,
      struct retro_core_t *current_core)
{
   /* Load symbols */
   if (!init_libretro_symbols_custom(type, current_core, NULL, NULL))
      return false;

#ifdef HAVE_RUNAHEAD
   {
      /* remember last core type created, so creating a
       * secondary core will know what core type to use. */
      struct rarch_state *p_rarch = &rarch_st;
      p_rarch->last_core_type = type;
   }
#endif
   return true;
}

bool libretro_get_shared_context(void)
{
   struct rarch_state *p_rarch  = &rarch_st;
   bool core_set_shared_context = p_rarch->core_set_shared_context;
   return core_set_shared_context;
}

/**
 * uninit_libretro_sym:
 *
 * Frees libretro core.
 *
 * Frees all core options,
 * associated state, and
 * unbind all libretro callback symbols.
 **/
static void uninit_libretro_symbols(struct retro_core_t *current_core)
{
   struct rarch_state *p_rarch = &rarch_st;

#ifdef HAVE_DYNAMIC
   if (p_rarch->lib_handle)
      dylib_close(p_rarch->lib_handle);
   p_rarch->lib_handle = NULL;
#endif

   memset(current_core, 0, sizeof(struct retro_core_t));

   p_rarch->core_set_shared_context   = false;

   if (p_rarch->runloop_core_options)
      retroarch_deinit_core_options();
   retroarch_system_info_free();
   retroarch_frame_time_free();
   p_rarch->camera_driver_active      = false;
   p_rarch->location_driver_active    = false;

   /* Performance counters no longer valid. */
   performance_counters_clear();
}

#if defined(HAVE_RUNAHEAD)
static void free_retro_ctx_load_content_info(struct
      retro_ctx_load_content_info *dest)
{
   if (!dest)
      return;

   core_free_retro_game_info(dest->info);
   string_list_free((struct string_list*)dest->content);
   if (dest->info)
      free(dest->info);

   dest->info    = NULL;
   dest->content = NULL;
}

static struct retro_game_info* clone_retro_game_info(const
      struct retro_game_info *src)
{
   struct retro_game_info *dest = (struct retro_game_info*)calloc(1,
         sizeof(struct retro_game_info));

   if (!dest)
      return NULL;

   if (src->size && src->data)
   {
      void *data = malloc(src->size);

      if (data)
      {
         memcpy(data, src->data, src->size);
         dest->data = data;
      }
   }

   if (!string_is_empty(src->path))
      dest->path = strdup(src->path);
   if (!string_is_empty(src->meta))
      dest->meta = strdup(src->meta);

   dest->size    = src->size;

   return dest;
}


static struct retro_ctx_load_content_info
*clone_retro_ctx_load_content_info(
      const struct retro_ctx_load_content_info *src)
{
   struct retro_ctx_load_content_info *dest = NULL;
   if (!src || src->special)
      return NULL;   /* refuse to deal with the Special field */

   dest          = (struct retro_ctx_load_content_info*)
      calloc(1, sizeof(*dest));

   if (!dest)
      return NULL;

   if (src->info)
      dest->info    = clone_retro_game_info(src->info);
   if (src->content)
      dest->content = string_list_clone(src->content);

   return dest;
}

static void set_load_content_info(const retro_ctx_load_content_info_t *ctx)
{
   struct rarch_state *p_rarch = &rarch_st;

   free_retro_ctx_load_content_info(p_rarch->load_content_info);
   free(p_rarch->load_content_info);
   p_rarch->load_content_info = clone_retro_ctx_load_content_info(ctx);
}

/* RUNAHEAD - SECONDARY CORE  */
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
static void strcat_alloc(char **dst, const char *s)
{
   size_t len1;
   char *src  = *dst;

   if (!src)
   {
      src     = strcpy_alloc_force(s);
      *dst    = src;
      return;
   }

   if (!s)
      return;

   len1       = strlen(src);
   src        = (char*)realloc(src, len1 + strlen(s) + 1);

   if (!src)
      return;

   *dst       = src;
   strcpy(src + len1, s);
}

static void secondary_core_destroy(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->secondary_module)
      return;

   /* unload game from core */
   if (p_rarch->secondary_core.retro_unload_game)
      p_rarch->secondary_core.retro_unload_game();
   p_rarch->core_poll_type_override = POLL_TYPE_OVERRIDE_DONTCARE;

   /* deinit */
   if (p_rarch->secondary_core.retro_deinit)
      p_rarch->secondary_core.retro_deinit();
   memset(&p_rarch->secondary_core, 0, sizeof(struct retro_core_t));

   dylib_close(p_rarch->secondary_module);
   p_rarch->secondary_module = NULL;
   filestream_delete(p_rarch->secondary_library_path);
   if (p_rarch->secondary_library_path)
      free(p_rarch->secondary_library_path);
   p_rarch->secondary_library_path = NULL;
}

static bool secondary_core_ensure_exists(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->secondary_module)
      if (!secondary_core_create())
         return false;
   return true;
}

#if defined(HAVE_RUNAHEAD) && defined(HAVE_DYNAMIC)
static bool secondary_core_deserialize(const void *buffer, int size)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (secondary_core_ensure_exists())
      return p_rarch->secondary_core.retro_unserialize(buffer, size);
   secondary_core_destroy();
   return false;
}
#endif

static void remember_controller_port_device(long port, long device)
{
   struct rarch_state *p_rarch = &rarch_st;

   if (port >= 0 && port < 16)
      p_rarch->port_map[port] = (int)device;
   if (p_rarch->secondary_module && p_rarch->secondary_core.retro_set_controller_port_device)
      p_rarch->secondary_core.retro_set_controller_port_device((unsigned)port, (unsigned)device);
}

static void clear_controller_port_map(void)
{
   unsigned port;
   struct rarch_state *p_rarch = &rarch_st;

   for (port = 0; port < 16; port++)
      p_rarch->port_map[port] = -1;
}

static char *get_temp_directory_alloc(const char *override_dir)
{
   const char *src        = NULL;
   char *path             = NULL;
#ifdef _WIN32
#ifdef LEGACY_WIN32
   DWORD path_length      = GetTempPath(0, NULL) + 1;

   if (!(path = (char*)malloc(path_length * sizeof(char))))
      return NULL;

   path[path_length - 1]   = 0;
   GetTempPath(path_length, path);
#else
   DWORD path_length = GetTempPathW(0, NULL) + 1;
   wchar_t *wide_str = (wchar_t*)malloc(path_length * sizeof(wchar_t));

   if (!wide_str)
      return NULL;

   wide_str[path_length - 1] = 0;
   GetTempPathW(path_length, wide_str);

   path = utf16_to_utf8_string_alloc(wide_str);
   free(wide_str);
#endif
#else
#if defined ANDROID
   src     = override_dir;
#else
   if (getenv("TMPDIR"))
      src  = getenv("TMPDIR");
   else 
      src  = "/tmp";
#endif
   path = strcpy_alloc_force(src);
#endif
   return path;
}

static bool write_file_with_random_name(char **temp_dll_path,
      const char *retroarch_temp_path, const void* data, ssize_t dataSize)
{
   unsigned i;
   char number_buf[32];
   bool okay                = false;
   const char *prefix       = "tmp";
   time_t time_value        = time(NULL);
   unsigned number_value    = (unsigned)time_value;
   char *ext                = strcpy_alloc_force(
         path_get_extension(*temp_dll_path));
   int ext_len              = (int)strlen(ext);

   if (ext_len > 0)
   {
      strcat_alloc(&ext, ".");
      memmove(ext + 1, ext, ext_len);
      ext[0] = '.';
      ext_len++;
   }

   /* Try up to 30 'random' filenames before giving up */
   for (i = 0; i < 30; i++)
   {
      int number;
      number_value = number_value * 214013 + 2531011;
      number       = (number_value >> 14) % 100000;

      snprintf(number_buf, sizeof(number_buf), "%05d", number);

      if (*temp_dll_path)
         free(*temp_dll_path);
      *temp_dll_path = NULL;

      strcat_alloc(temp_dll_path, retroarch_temp_path);
      strcat_alloc(temp_dll_path, prefix);
      strcat_alloc(temp_dll_path, number_buf);
      strcat_alloc(temp_dll_path, ext);

      if (filestream_write_file(*temp_dll_path, data, dataSize))
      {
         okay = true;
         break;
      }
   }

   if (ext)
      free(ext);
   ext = NULL;
   return okay;
}

static char *copy_core_to_temp_file(void)
{
   bool  failed                = false;
   char  *temp_directory       = NULL;
   char  *retroarch_temp_path  = NULL;
   char  *temp_dll_path        = NULL;
   void  *dll_file_data        = NULL;
   int64_t  dll_file_size      = 0;
   const char  *core_path      = path_get(RARCH_PATH_CORE);
   const char  *core_base_name = path_basename(core_path);
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   const char  *dir_libretro   = settings->paths.directory_libretro;

   if (strlen(core_base_name) == 0)
      return NULL;

   temp_directory             = get_temp_directory_alloc(dir_libretro);
   if (!temp_directory)
      return NULL;

   strcat_alloc(&retroarch_temp_path, temp_directory);
   strcat_alloc(&retroarch_temp_path, path_default_slash());
   strcat_alloc(&retroarch_temp_path, "retroarch_temp");
   strcat_alloc(&retroarch_temp_path, path_default_slash());

   if (!path_mkdir(retroarch_temp_path))
   {
      failed = true;
      goto end;
   }

   if (!filestream_read_file(core_path, &dll_file_data, &dll_file_size))
   {
      failed = true;
      goto end;
   }

   strcat_alloc(&temp_dll_path, retroarch_temp_path);
   strcat_alloc(&temp_dll_path, core_base_name);

   if (!filestream_write_file(temp_dll_path, dll_file_data, dll_file_size))
   {
      /* try other file names */
      if (!write_file_with_random_name(&temp_dll_path,
               retroarch_temp_path, dll_file_data, dll_file_size))
         failed = true;
   }

end:
   if (temp_directory)
      free(temp_directory);
   if (retroarch_temp_path)
      free(retroarch_temp_path);
   if (dll_file_data)
      free(dll_file_data);

   temp_directory      = NULL;
   retroarch_temp_path = NULL;
   dll_file_data       = NULL;

   if (!failed)
      return temp_dll_path;

   if (temp_dll_path)
      free(temp_dll_path);

   temp_dll_path     = NULL;

   return NULL;
}

static bool rarch_environment_secondary_core_hook(unsigned cmd, void *data)
{
   struct rarch_state *p_rarch = &rarch_st;
   bool                 result = rarch_environment_cb(cmd, data);

   if (p_rarch->has_variable_update)
   {
      if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE)
      {
         bool *bool_p                 = (bool*)data;
         *bool_p                      = true;
         p_rarch->has_variable_update = false;
         return true;
      }
      else if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE)
         p_rarch->has_variable_update = false;
   }
   return result;
}

static bool secondary_core_create(void)
{
   long port, device;
   bool contentless            = false;
   bool is_inited              = false;
   struct rarch_state *p_rarch = &rarch_st;
   const enum rarch_core_type 
      last_core_type           = p_rarch->last_core_type;

   if (   last_core_type != CORE_TYPE_PLAIN          ||
         !p_rarch->load_content_info                 ||
          p_rarch->load_content_info->special)
      return false;

   if (p_rarch->secondary_library_path)
      free(p_rarch->secondary_library_path);
   p_rarch->secondary_library_path = NULL;
   p_rarch->secondary_library_path = copy_core_to_temp_file();

   if (!p_rarch->secondary_library_path)
      return false;

   /* Load Core */
   if (!init_libretro_symbols_custom(
            CORE_TYPE_PLAIN, &p_rarch->secondary_core,
            p_rarch->secondary_library_path, &p_rarch->secondary_module))
      return false;

   p_rarch->secondary_core.symbols_inited = true;
   p_rarch->secondary_core.retro_set_environment(
         rarch_environment_secondary_core_hook);
#ifdef HAVE_RUNAHEAD
   p_rarch->has_variable_update  = true;
#endif

   p_rarch->secondary_core.retro_init();

   content_get_status(&contentless, &is_inited);
   p_rarch->secondary_core.inited = is_inited;

   /* Load Content */
   /* disabled due to crashes */
   if ( !p_rarch->load_content_info || 
         p_rarch->load_content_info->special)
      return false;

   if ( (p_rarch->load_content_info->content->size > 0) && 
         p_rarch->load_content_info->content->elems[0].data)
   {
      p_rarch->secondary_core.game_loaded = p_rarch->secondary_core.retro_load_game(
            p_rarch->load_content_info->info);
      if (!p_rarch->secondary_core.game_loaded)
         goto error;
   }
   else if (contentless)
   {
      p_rarch->secondary_core.game_loaded = p_rarch->secondary_core.retro_load_game(NULL);
      if (!p_rarch->secondary_core.game_loaded)
         goto error;
   }
   else
      p_rarch->secondary_core.game_loaded = false;

   if (!p_rarch->secondary_core.inited)
      goto error;

   core_set_default_callbacks(&p_rarch->secondary_callbacks);
   p_rarch->secondary_core.retro_set_video_refresh(p_rarch->secondary_callbacks.frame_cb);
   p_rarch->secondary_core.retro_set_audio_sample(p_rarch->secondary_callbacks.sample_cb);
   p_rarch->secondary_core.retro_set_audio_sample_batch(p_rarch->secondary_callbacks.sample_batch_cb);
   p_rarch->secondary_core.retro_set_input_state(p_rarch->secondary_callbacks.state_cb);
   p_rarch->secondary_core.retro_set_input_poll(p_rarch->secondary_callbacks.poll_cb);

   for (port = 0; port < 16; port++)
   {
      device = p_rarch->port_map[port];
      if (device >= 0)
         p_rarch->secondary_core.retro_set_controller_port_device(
               (unsigned)port, (unsigned)device);
   }
   clear_controller_port_map();

   return true;

error:
   secondary_core_destroy();
   return false;
}

static void secondary_core_input_poll_null(void) { }

static bool secondary_core_run_use_last_input(void)
{
   retro_input_poll_t old_poll_function;
   retro_input_state_t old_input_function;
   struct rarch_state *p_rarch = &rarch_st;

   if (!secondary_core_ensure_exists())
   {
      secondary_core_destroy();
      return false;
   }

   old_poll_function                     = p_rarch->secondary_callbacks.poll_cb;
   old_input_function                    = p_rarch->secondary_callbacks.state_cb;

   p_rarch->secondary_callbacks.poll_cb  = secondary_core_input_poll_null;
   p_rarch->secondary_callbacks.state_cb = input_state_get_last;

   p_rarch->secondary_core.retro_set_input_poll(p_rarch->secondary_callbacks.poll_cb);
   p_rarch->secondary_core.retro_set_input_state(p_rarch->secondary_callbacks.state_cb);

   p_rarch->secondary_core.retro_run();

   p_rarch->secondary_callbacks.poll_cb  = old_poll_function;
   p_rarch->secondary_callbacks.state_cb = old_input_function;

   p_rarch->secondary_core.retro_set_input_poll(p_rarch->secondary_callbacks.poll_cb);
   p_rarch->secondary_core.retro_set_input_state(p_rarch->secondary_callbacks.state_cb);

   return true;
}
#else
static void secondary_core_destroy(void) { }
static void remember_controller_port_device(long port, long device) { }
static void clear_controller_port_map(void) { }
#endif

#endif

/* WIFI DRIVER  */

/**
 * wifi_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to wifi driver at index. Can be NULL
 * if nothing found.
 **/
static const void *wifi_driver_find_handle(int idx)
{
   const void *drv = wifi_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * config_get_wifi_driver_options:
 *
 * Get an enumerated list of all wifi driver names,
 * separated by '|'.
 *
 * Returns: string listing of all wifi driver names,
 * separated by '|'.
 **/
const char* config_get_wifi_driver_options(void)
{
   return char_list_new_special(STRING_LIST_WIFI_DRIVERS, NULL);
}

void driver_wifi_scan(void)
{
   struct rarch_state       *p_rarch = &rarch_st;
   p_rarch->wifi_driver->scan();
}

void driver_wifi_get_ssids(struct string_list* ssids)
{
   struct rarch_state       *p_rarch = &rarch_st;
   p_rarch->wifi_driver->get_ssids(ssids);
}

bool driver_wifi_ssid_is_online(unsigned i)
{
   struct rarch_state       *p_rarch = &rarch_st;
   return p_rarch->wifi_driver->ssid_is_online(i);
}

bool driver_wifi_connect_ssid(unsigned i, const char* passphrase)
{
   struct rarch_state       *p_rarch = &rarch_st;
   return p_rarch->wifi_driver->connect_ssid(i, passphrase);
}

void driver_wifi_tether_start_stop(bool start, char* configfile)
{
   struct rarch_state       *p_rarch = &rarch_st;
   p_rarch->wifi_driver->tether_start_stop(start, configfile);
}

bool wifi_driver_ctl(enum rarch_wifi_ctl_state state, void *data)
{
   struct rarch_state     *p_rarch  = &rarch_st;
   settings_t             *settings = p_rarch->configuration_settings;

   switch (state)
   {
      case RARCH_WIFI_CTL_DESTROY:
         p_rarch->wifi_driver_active   = false;
         p_rarch->wifi_driver          = NULL;
         p_rarch->wifi_data            = NULL;
         break;
      case RARCH_WIFI_CTL_SET_ACTIVE:
         p_rarch->wifi_driver_active   = true;
         break;
      case RARCH_WIFI_CTL_FIND_DRIVER:
         {
            int i;
            driver_ctx_info_t drv;

            drv.label = "wifi_driver";
            drv.s     = settings->arrays.wifi_driver;

            driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

            i = (int)drv.len;

            if (i >= 0)
               p_rarch->wifi_driver = (const wifi_driver_t*)wifi_driver_find_handle(i);
            else
            {
               if (verbosity_is_enabled())
               {
                  unsigned d;
                  RARCH_ERR("Couldn't find any wifi driver named \"%s\"\n",
                        settings->arrays.wifi_driver);
                  RARCH_LOG_OUTPUT("Available wifi drivers are:\n");
                  for (d = 0; wifi_driver_find_handle(d); d++)
                     RARCH_LOG_OUTPUT("\t%s\n", wifi_drivers[d]->ident);

                  RARCH_WARN("Going to default to first wifi driver...\n");
               }

               p_rarch->wifi_driver = (const wifi_driver_t*)wifi_driver_find_handle(0);

               if (!p_rarch->wifi_driver)
                  retroarch_fail(1, "find_wifi_driver()");
            }
         }
         break;
      case RARCH_WIFI_CTL_UNSET_ACTIVE:
         p_rarch->wifi_driver_active = false;
         break;
      case RARCH_WIFI_CTL_IS_ACTIVE:
        return p_rarch->wifi_driver_active;
      case RARCH_WIFI_CTL_DEINIT:
        if (p_rarch->wifi_data && p_rarch->wifi_driver)
        {
           if (p_rarch->wifi_driver->free)
              p_rarch->wifi_driver->free(p_rarch->wifi_data);
        }

        p_rarch->wifi_data = NULL;
        break;
      case RARCH_WIFI_CTL_STOP:
        if (     p_rarch->wifi_driver
              && p_rarch->wifi_driver->stop
              && p_rarch->wifi_data)
           p_rarch->wifi_driver->stop(p_rarch->wifi_data);
        break;
      case RARCH_WIFI_CTL_START:
        if (     p_rarch->wifi_driver 
              && p_rarch->wifi_data
              && p_rarch->wifi_driver->start)
        {
           bool wifi_allow      = settings->bools.wifi_allow;
           if (wifi_allow)
              return p_rarch->wifi_driver->start(p_rarch->wifi_data);
        }
        return false;
      case RARCH_WIFI_CTL_SET_CB:
        {
           /*struct retro_wifi_callback *cb =
              (struct retro_wifi_callback*)data;
           wifi_cb          = *cb;*/
        }
        break;
      case RARCH_WIFI_CTL_INIT:
        /* Resource leaks will follow if wifi is initialized twice. */
        if (p_rarch->wifi_data)
           return false;

        wifi_driver_ctl(RARCH_WIFI_CTL_FIND_DRIVER, NULL);

        p_rarch->wifi_data = p_rarch->wifi_driver->init();

        if (!p_rarch->wifi_data)
        {
           RARCH_ERR("Failed to initialize wifi driver. Will continue without wifi.\n");
           wifi_driver_ctl(RARCH_WIFI_CTL_UNSET_ACTIVE, NULL);
        }

        /*if (wifi_cb.initialized)
           wifi_cb.initialized();*/
        break;
      default:
         break;
   }

   return false;
}

/* UI COMPANION */

void ui_companion_set_foreground(unsigned enable)
{
   struct rarch_state     *p_rarch = &rarch_st;
   p_rarch->main_ui_companion_is_on_foreground = enable;
}

bool ui_companion_is_on_foreground(void)
{
   struct rarch_state     *p_rarch = &rarch_st;
   return p_rarch->main_ui_companion_is_on_foreground;
}

void ui_companion_event_command(enum event_command action)
{
   struct rarch_state     *p_rarch = &rarch_st;
#ifdef HAVE_QT
   bool qt_is_inited               = p_rarch->qt_is_inited;
#endif
   const ui_companion_driver_t *ui = p_rarch->ui_companion;

   if (ui && ui->event_command)
      ui->event_command(p_rarch->ui_companion_data, action);
#ifdef HAVE_QT
   if (ui_companion_qt.toggle && qt_is_inited)
      ui_companion_qt.event_command(
            p_rarch->ui_companion_qt_data, action);
#endif
}

static void ui_companion_driver_deinit(void)
{
   struct rarch_state     *p_rarch = &rarch_st;
#ifdef HAVE_QT
   bool qt_is_inited               = p_rarch->qt_is_inited;
#endif
   const ui_companion_driver_t *ui = p_rarch->ui_companion;

   if (!ui)
      return;
   if (ui->deinit)
      ui->deinit(p_rarch->ui_companion_data);

#ifdef HAVE_QT
   if (qt_is_inited)
   {
      ui_companion_qt.deinit(p_rarch->ui_companion_qt_data);
      p_rarch->ui_companion_qt_data = NULL;
   }
#endif
   p_rarch->ui_companion_data = NULL;
}

void ui_companion_driver_init_first(void)
{
   struct rarch_state     *p_rarch = &rarch_st;
   settings_t      *settings       = p_rarch->configuration_settings;
#ifdef HAVE_QT
   bool desktop_menu_enable        = settings->bools.desktop_menu_enable;
   bool ui_companion_toggle        = settings->bools.ui_companion_toggle;
#endif

   p_rarch->ui_companion           = (ui_companion_driver_t*)ui_companion_drivers[0];

#ifdef HAVE_QT
   if (desktop_menu_enable && ui_companion_toggle)
   {
      p_rarch->ui_companion_qt_data  = ui_companion_qt.init();
      p_rarch->qt_is_inited          = true;
   }
#endif

   if (p_rarch->ui_companion)
   {
      unsigned ui_companion_start_on_boot = 
         settings->bools.ui_companion_start_on_boot;

      if (ui_companion_start_on_boot)
      {
         if (p_rarch->ui_companion->init)
            p_rarch->ui_companion_data = p_rarch->ui_companion->init();

         ui_companion_driver_toggle(false);
      }
   }
}

static void ui_companion_driver_toggle(bool force)
{
   struct rarch_state *p_rarch  = &rarch_st;
#ifdef HAVE_QT
   settings_t     *settings     = p_rarch->configuration_settings;
   bool     desktop_menu_enable = settings->bools.desktop_menu_enable;
   bool     ui_companion_toggle = settings->bools.ui_companion_toggle;
#endif

   if (p_rarch->ui_companion && p_rarch->ui_companion->toggle)
      p_rarch->ui_companion->toggle(p_rarch->ui_companion_data, false);

#ifdef HAVE_QT
   if (desktop_menu_enable)
   {
      if ((ui_companion_toggle || force) && !p_rarch->qt_is_inited)
      {
         p_rarch->ui_companion_qt_data   = ui_companion_qt.init();
         p_rarch->qt_is_inited           = true;
      }

      if (ui_companion_qt.toggle && p_rarch->qt_is_inited)
         ui_companion_qt.toggle(p_rarch->ui_companion_qt_data, force);
   }
#endif
}

void ui_companion_driver_notify_refresh(void)
{
   struct rarch_state *p_rarch     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
#ifdef HAVE_QT
   settings_t      *settings       = p_rarch->configuration_settings;
   bool desktop_menu_enable        = settings->bools.desktop_menu_enable;
   bool qt_is_inited               = p_rarch->qt_is_inited;
#endif

   if (!ui)
      return;
   if (ui->notify_refresh)
      ui->notify_refresh(p_rarch->ui_companion_data);

#ifdef HAVE_QT
   if (desktop_menu_enable)
      if (ui_companion_qt.notify_refresh && qt_is_inited)
         ui_companion_qt.notify_refresh(p_rarch->ui_companion_qt_data);
#endif
}

void ui_companion_driver_notify_list_loaded(
      file_list_t *list, file_list_t *menu_list)
{
   struct rarch_state *p_rarch     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (ui && ui->notify_list_loaded)
      ui->notify_list_loaded(p_rarch->ui_companion_data, list, menu_list);
}

void ui_companion_driver_notify_content_loaded(void)
{
   struct rarch_state *p_rarch     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (ui && ui->notify_content_loaded)
      ui->notify_content_loaded(p_rarch->ui_companion_data);
}

void ui_companion_driver_free(void)
{
   struct rarch_state *p_rarch     = &rarch_st;

   p_rarch->ui_companion = NULL;
}

const ui_msg_window_t *ui_companion_driver_get_msg_window_ptr(void)
{
   struct rarch_state *p_rarch     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (!ui)
      return NULL;
   return ui->msg_window;
}

const ui_window_t *ui_companion_driver_get_window_ptr(void)
{
   struct rarch_state *p_rarch     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (!ui)
      return NULL;
   return ui->window;
}

const ui_browser_window_t *ui_companion_driver_get_browser_window_ptr(void)
{
   struct rarch_state *p_rarch     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (!ui)
      return NULL;
   return ui->browser_window;
}

static void ui_companion_driver_msg_queue_push(
      const char *msg, unsigned priority, unsigned duration, bool flush)
{
   struct rarch_state 
      *p_rarch                     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;

   if (ui && ui->msg_queue_push)
      ui->msg_queue_push(p_rarch->ui_companion_data, msg, priority, duration, flush);

#ifdef HAVE_QT
   {
      settings_t *settings     = p_rarch->configuration_settings;
      bool qt_is_inited        = p_rarch->qt_is_inited;
      bool desktop_menu_enable = settings->bools.desktop_menu_enable;

      if (desktop_menu_enable)
         if (ui_companion_qt.msg_queue_push && qt_is_inited)
            ui_companion_qt.msg_queue_push(
                  p_rarch->ui_companion_qt_data,
                  msg, priority, duration, flush);
   }
#endif
}

void *ui_companion_driver_get_main_window(void)
{
   struct rarch_state 
      *p_rarch                     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (!ui || !ui->get_main_window)
      return NULL;
   return ui->get_main_window(p_rarch->ui_companion_data);
}

const char *ui_companion_driver_get_ident(void)
{
   struct rarch_state 
      *p_rarch                     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (!ui)
      return "null";
   return ui->ident;
}

void ui_companion_driver_log_msg(const char *msg)
{
#ifdef HAVE_QT
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   bool qt_is_inited           = p_rarch->qt_is_inited;
   bool desktop_menu_enable    = settings->bools.desktop_menu_enable;

   if (desktop_menu_enable)
      if (p_rarch->ui_companion_qt_data && qt_is_inited)
         ui_companion_qt.log_msg(p_rarch->ui_companion_qt_data, msg);
#endif
}

/* RECORDING */

/**
 * record_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to record driver at index. Can be NULL
 * if nothing found.
 **/
static const void *record_driver_find_handle(int idx)
{
   const void *drv = record_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * config_get_record_driver_options:
 *
 * Get an enumerated list of all record driver names, separated by '|'.
 *
 * Returns: string listing of all record driver names, separated by '|'.
 **/
const char* config_get_record_driver_options(void)
{
   return char_list_new_special(STRING_LIST_RECORD_DRIVERS, NULL);
}

#if 0
/* TODO/FIXME - not used apparently */
static void find_record_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   settings_t *settings = p_rarch->configuration_settings;

   drv.label = "record_driver";
   drv.s     = settings->arrays.record_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i = (int)drv.len;

   if (i >= 0)
      p_rarch->recording_driver = (const record_driver_t*)record_driver_find_handle(i);
   else
   {
      if (verbosity_is_enabled())
      {
         unsigned d;

         RARCH_ERR("[recording] Couldn't find any record driver named \"%s\"\n",
               settings->arrays.record_driver);
         RARCH_LOG_OUTPUT("Available record drivers are:\n");
         for (d = 0; record_driver_find_handle(d); d++)
            RARCH_LOG_OUTPUT("\t%s\n", record_drivers[d].ident);
         RARCH_WARN("[recording] Going to default to first record driver...\n");
      }

      p_rarch->recording_driver = (const record_driver_t*)record_driver_find_handle(0);

      if (!p_rarch->recording_driver)
         retroarch_fail(1, "find_record_driver()");
   }
}

/**
 * ffemu_find_backend:
 * @ident                   : Identifier of driver to find.
 *
 * Finds a recording driver with the name @ident.
 *
 * Returns: recording driver handle if successful, otherwise
 * NULL.
 **/
static const record_driver_t *ffemu_find_backend(const char *ident)
{
   unsigned i;

   for (i = 0; record_drivers[i]; i++)
   {
      if (string_is_equal(record_drivers[i]->ident, ident))
         return record_drivers[i];
   }

   return NULL;
}

static void recording_driver_free_state(void)
{
   /* TODO/FIXME - this is not being called anywhere */
   p_rarch->recording_gpu_width      = 0;
   p_rarch->recording_gpu_height     = 0;
   p_rarch->recording_width          = 0;
   p_rarch->recording_height         = 0;
}
#endif

/**
 * gfx_ctx_init_first:
 * @backend                 : Recording backend handle.
 * @data                    : Recording data handle.
 * @params                  : Recording info parameters.
 *
 * Finds first suitable recording context driver and initializes.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool record_driver_init_first(
      const record_driver_t **backend, void **data,
      const struct record_params *params)
{
   unsigned i;

   for (i = 0; record_drivers[i]; i++)
   {
      void *handle = record_drivers[i]->init(params);

      if (!handle)
         continue;

      *backend = record_drivers[i];
      *data = handle;
      return true;
   }

   return false;
}

static void recording_dump_frame(const void *data, unsigned width,
      unsigned height, size_t pitch, bool is_idle)
{
   struct record_video_data ffemu_data;
   struct rarch_state *p_rarch = &rarch_st;

   ffemu_data.data     = data;
   ffemu_data.width    = width;
   ffemu_data.height   = height;
   ffemu_data.pitch    = (int)pitch;
   ffemu_data.is_dupe  = false;

   if (p_rarch->video_driver_record_gpu_buffer)
   {
      struct video_viewport vp;

      vp.x                        = 0;
      vp.y                        = 0;
      vp.width                    = 0;
      vp.height                   = 0;
      vp.full_width               = 0;
      vp.full_height              = 0;

      video_driver_get_viewport_info(&vp);

      if (!vp.width || !vp.height)
      {
         RARCH_WARN("[recording] %s \n",
               msg_hash_to_str(MSG_VIEWPORT_SIZE_CALCULATION_FAILED));
         video_driver_gpu_record_deinit();
         recording_dump_frame(data, width, height, pitch, is_idle);
         return;
      }

      /* User has resized. We kinda have a problem now. */
      if (  vp.width  != p_rarch->recording_gpu_width ||
            vp.height != p_rarch->recording_gpu_height)
      {
         RARCH_WARN("[recording] %s\n",
               msg_hash_to_str(MSG_RECORDING_TERMINATED_DUE_TO_RESIZE));

         runloop_msg_queue_push(
               msg_hash_to_str(MSG_RECORDING_TERMINATED_DUE_TO_RESIZE),
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         command_event(CMD_EVENT_RECORD_DEINIT, NULL);
         return;
      }

      /* Big bottleneck.
       * Since we might need to do read-backs asynchronously,
       * it might take 3-4 times before this returns true. */
      if (!video_driver_read_viewport(p_rarch->video_driver_record_gpu_buffer, is_idle))
         return;

      ffemu_data.pitch  = (int)(p_rarch->recording_gpu_width * 3);
      ffemu_data.width  = (unsigned)p_rarch->recording_gpu_width;
      ffemu_data.height = (unsigned)p_rarch->recording_gpu_height;
      ffemu_data.data   = p_rarch->video_driver_record_gpu_buffer + (ffemu_data.height - 1) * ffemu_data.pitch;

      ffemu_data.pitch  = -ffemu_data.pitch;
   }
   else
      ffemu_data.is_dupe = !data;

   p_rarch->recording_driver->push_video(p_rarch->recording_data, &ffemu_data);
}

static bool recording_deinit(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->recording_data || !p_rarch->recording_driver)
      return false;

   if (p_rarch->recording_driver->finalize)
      p_rarch->recording_driver->finalize(p_rarch->recording_data);

   if (p_rarch->recording_driver->free)
      p_rarch->recording_driver->free(p_rarch->recording_data);

   p_rarch->recording_data            = NULL;
   p_rarch->recording_driver          = NULL;

   video_driver_gpu_record_deinit();

   return true;
}

bool recording_is_enabled(void)
{
   struct rarch_state *p_rarch          = &rarch_st;
   return p_rarch->recording_enable;
}

bool streaming_is_enabled(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->streaming_enable;
}

void streaming_set_state(bool state)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->streaming_enable = state;
}

static bool video_driver_gpu_record_init(unsigned size)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->video_driver_record_gpu_buffer = (uint8_t*)malloc(size);
   if (!p_rarch->video_driver_record_gpu_buffer)
      return false;
   return true;
}

static void video_driver_gpu_record_deinit(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->video_driver_record_gpu_buffer)
      free(p_rarch->video_driver_record_gpu_buffer);
   p_rarch->video_driver_record_gpu_buffer = NULL;
}

/**
 * recording_init:
 *
 * Initializes recording.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool recording_init(void)
{
   char output[PATH_MAX_LENGTH];
   char buf[PATH_MAX_LENGTH];
   struct record_params params          = {0};
   struct rarch_state *p_rarch          = &rarch_st;
   struct retro_system_av_info *av_info = &p_rarch->video_driver_av_info;
   settings_t *settings                 = p_rarch->configuration_settings;
   global_t *global                     = &p_rarch->g_extern;
   bool video_gpu_record                = settings->bools.video_gpu_record;
   bool video_force_aspect              = settings->bools.video_force_aspect;
   const enum rarch_core_type 
      current_core_type                 = p_rarch->current_core_type;
   const enum retro_pixel_format 
      video_driver_pix_fmt              = p_rarch->video_driver_pix_fmt;
   bool recording_enable                = p_rarch->recording_enable;

   if (!recording_enable)
      return false;

   output[0] = '\0';

   if (current_core_type == CORE_TYPE_DUMMY)
   {
      RARCH_WARN("[recording] %s\n",
            msg_hash_to_str(MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED));
      return false;
   }

   if (!video_gpu_record && video_driver_is_hw_context())
   {
      RARCH_WARN("[recording] %s.\n",
            msg_hash_to_str(MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING));
      return false;
   }

   RARCH_LOG("[recording] %s: FPS: %.4f, Sample rate: %.4f\n",
         msg_hash_to_str(MSG_CUSTOM_TIMING_GIVEN),
         (float)av_info->timing.fps,
         (float)av_info->timing.sample_rate);

   if (!string_is_empty(global->record.path))
      strlcpy(output, global->record.path, sizeof(output));
   else
   {
      const char *stream_url        = settings->paths.path_stream_url;
      unsigned video_record_quality = settings->uints.video_record_quality;
      unsigned video_stream_port    = settings->uints.video_stream_port;
      if (p_rarch->streaming_enable)
         if (!string_is_empty(stream_url))
            strlcpy(output, stream_url, sizeof(output));
         else
            /* Fallback, stream locally to 127.0.0.1 */
            snprintf(output, sizeof(output), "udp://127.0.0.1:%u",
                  video_stream_port);
      else
      {
         const char *game_name = path_basename(path_get(RARCH_PATH_BASENAME));
         if (video_record_quality < RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST)
         {
            fill_str_dated_filename(buf, game_name,
                     "mkv", sizeof(buf));
            fill_pathname_join(output, global->record.output_dir, buf, sizeof(output));
         }
         else if (video_record_quality >= RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST
               && video_record_quality < RECORD_CONFIG_TYPE_RECORDING_GIF)
         {
            fill_str_dated_filename(buf, game_name,
                     "webm", sizeof(buf));
            fill_pathname_join(output, global->record.output_dir, buf, sizeof(output));
         }
         else if (video_record_quality >= RECORD_CONFIG_TYPE_RECORDING_GIF
               && video_record_quality < RECORD_CONFIG_TYPE_RECORDING_APNG)
         {
            fill_str_dated_filename(buf, game_name,
                     "gif", sizeof(buf));
            fill_pathname_join(output, global->record.output_dir, buf, sizeof(output));
         }
         else
         {
            fill_str_dated_filename(buf, game_name,
                     "png", sizeof(buf));
            fill_pathname_join(output, global->record.output_dir, buf, sizeof(output));
         }
      }
   }

   params.audio_resampler           = settings->arrays.audio_resampler;
   params.video_gpu_record          = settings->bools.video_gpu_record;
   params.video_record_scale_factor = settings->uints.video_record_scale_factor;
   params.video_stream_scale_factor = settings->uints.video_stream_scale_factor;
   params.video_record_threads      = settings->uints.video_record_threads;
   params.streaming_mode            = settings->uints.streaming_mode;

   params.out_width                 = av_info->geometry.base_width;
   params.out_height                = av_info->geometry.base_height;
   params.fb_width                  = av_info->geometry.max_width;
   params.fb_height                 = av_info->geometry.max_height;
   params.channels                  = 2;
   params.filename                  = output;
   params.fps                       = av_info->timing.fps;
   params.samplerate                = av_info->timing.sample_rate;
   params.pix_fmt                   = 
      (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
      ? FFEMU_PIX_ARGB8888
      : FFEMU_PIX_RGB565;
   params.config                    = NULL;

   if (!string_is_empty(global->record.config))
      params.config                 = global->record.config;
   else
   {
      if (p_rarch->streaming_enable)
      {
         params.config = settings->paths.path_stream_config;
         params.preset = (enum record_config_type)
            settings->uints.video_stream_quality;
      }
      else
      {
         params.config = settings->paths.path_record_config;
         params.preset = (enum record_config_type)
            settings->uints.video_record_quality;
      }
   }

   if (settings->bools.video_gpu_record
      && p_rarch->current_video->read_viewport)
   {
      unsigned gpu_size;
      struct video_viewport vp;

      vp.x                        = 0;
      vp.y                        = 0;
      vp.width                    = 0;
      vp.height                   = 0;
      vp.full_width               = 0;
      vp.full_height              = 0;

      video_driver_get_viewport_info(&vp);

      if (!vp.width || !vp.height)
      {
         RARCH_ERR("[recording] Failed to get viewport information from video driver. "
               "Cannot start recording ...\n");
         return false;
      }

      params.out_width                    = vp.width;
      params.out_height                   = vp.height;
      params.fb_width                     = next_pow2(vp.width);
      params.fb_height                    = next_pow2(vp.height);

      if (video_force_aspect &&
            (p_rarch->video_driver_aspect_ratio > 0.0f))
         params.aspect_ratio              = p_rarch->video_driver_aspect_ratio;
      else
         params.aspect_ratio              = (float)vp.width / vp.height;

      params.pix_fmt                      = FFEMU_PIX_BGR24;
      p_rarch->recording_gpu_width        = vp.width;
      p_rarch->recording_gpu_height       = vp.height;

      RARCH_LOG("[recording] %s %u x %u\n", msg_hash_to_str(MSG_DETECTED_VIEWPORT_OF),
            vp.width, vp.height);

      gpu_size = vp.width * vp.height * 3;
      if (!video_driver_gpu_record_init(gpu_size))
         return false;
   }
   else
   {
      if (p_rarch->recording_width || p_rarch->recording_height)
      {
         params.out_width  = p_rarch->recording_width;
         params.out_height = p_rarch->recording_height;
      }

      if (video_force_aspect &&
            (p_rarch->video_driver_aspect_ratio > 0.0f))
         params.aspect_ratio = p_rarch->video_driver_aspect_ratio;
      else
         params.aspect_ratio = (float)params.out_width / params.out_height;

      if (settings->bools.video_post_filter_record
            && !!p_rarch->video_driver_state_filter)
      {
         unsigned max_width  = 0;
         unsigned max_height = 0;

         params.pix_fmt      = FFEMU_PIX_RGB565;

         if (p_rarch->video_driver_state_out_rgb32)
            params.pix_fmt = FFEMU_PIX_ARGB8888;

         rarch_softfilter_get_max_output_size(
               p_rarch->video_driver_state_filter,
               &max_width, &max_height);
         params.fb_width  = next_pow2(max_width);
         params.fb_height = next_pow2(max_height);
      }
   }

   RARCH_LOG("[recording] %s %s @ %ux%u. (FB size: %ux%u pix_fmt: %u)\n",
         msg_hash_to_str(MSG_RECORDING_TO),
         output,
         params.out_width, params.out_height,
         params.fb_width, params.fb_height,
         (unsigned)params.pix_fmt);

   if (!record_driver_init_first(
            &p_rarch->recording_driver, &p_rarch->recording_data, &params))
   {
      RARCH_ERR("[recording] %s\n", msg_hash_to_str(MSG_FAILED_TO_START_RECORDING));
      video_driver_gpu_record_deinit();

      return false;
   }

   return true;
}

void recording_driver_update_streaming_url(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t     *settings    = p_rarch->configuration_settings;
   const char     *youtube_url = "rtmp://a.rtmp.youtube.com/live2/";
   const char     *twitch_url  = "rtmp://live.twitch.tv/app/";

   if (!settings)
      return;

   switch (settings->uints.streaming_mode)
   {
      case STREAMING_MODE_TWITCH:
         if (!string_is_empty(settings->arrays.twitch_stream_key))
            snprintf(settings->paths.path_stream_url,
                  sizeof(settings->paths.path_stream_url),
                  "%s%s", twitch_url, settings->arrays.twitch_stream_key);
         break;
      case STREAMING_MODE_YOUTUBE:
         if (!string_is_empty(settings->arrays.youtube_stream_key))
            snprintf(settings->paths.path_stream_url,
                  sizeof(settings->paths.path_stream_url),
                  "%s%s", youtube_url, settings->arrays.youtube_stream_key);
         break;
      case STREAMING_MODE_LOCAL:
         /* TODO: figure out default interface and bind to that instead */
         snprintf(settings->paths.path_stream_url, sizeof(settings->paths.path_stream_url),
            "udp://%s:%u", "127.0.0.1", settings->uints.video_stream_port);
         break;
      case STREAMING_MODE_CUSTOM:
      default:
         /* Do nothing, let the user input the URL */
         break;
   }
}

/* BSV MOVIE */
static bool bsv_movie_init_playback(
      bsv_movie_t *handle, const char *path)
{
   uint32_t state_size       = 0;
   uint32_t content_crc      = 0;
   uint32_t header[4]        = {0};
   intfstream_t *file        = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open BSV file for playback, path : \"%s\".\n", path);
      return false;
   }

   handle->file              = file;
   handle->playback          = true;

   intfstream_read(handle->file, header, sizeof(uint32_t) * 4);
   /* Compatibility with old implementation that
    * used incorrect documentation. */
   if (swap_if_little32(header[MAGIC_INDEX]) != BSV_MAGIC
         && swap_if_big32(header[MAGIC_INDEX]) != BSV_MAGIC)
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE));
      return false;
   }

   content_crc               = content_get_crc();

   if (content_crc != 0)
      if (swap_if_big32(header[CRC_INDEX]) != content_crc)
         RARCH_WARN("%s.\n", msg_hash_to_str(MSG_CRC32_CHECKSUM_MISMATCH));

   state_size = swap_if_big32(header[STATE_SIZE_INDEX]);

#if 0
   RARCH_ERR("----- debug %u -----\n", header[0]);
   RARCH_ERR("----- debug %u -----\n", header[1]);
   RARCH_ERR("----- debug %u -----\n", header[2]);
   RARCH_ERR("----- debug %u -----\n", header[3]);
#endif

   if (state_size)
   {
      retro_ctx_size_info_t info;
      retro_ctx_serialize_info_t serial_info;
      uint8_t *buf       = (uint8_t*)malloc(state_size);

      if (!buf)
         return false;

      handle->state      = buf;
      handle->state_size = state_size;
      if (intfstream_read(handle->file,
               handle->state, state_size) != state_size)
      {
         RARCH_ERR("%s\n", msg_hash_to_str(MSG_COULD_NOT_READ_STATE_FROM_MOVIE));
         return false;
      }

      core_serialize_size( &info);

      if (info.size == state_size)
      {
         serial_info.data_const = handle->state;
         serial_info.size       = state_size;
         core_unserialize(&serial_info);
      }
      else
         RARCH_WARN("%s\n",
               msg_hash_to_str(MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION));
   }

   handle->min_file_pos = sizeof(header) + state_size;

   return true;
}

static bool bsv_movie_init_record(
      bsv_movie_t *handle, const char *path)
{
   retro_ctx_size_info_t info;
   uint32_t state_size       = 0;
   uint32_t content_crc      = 0;
   uint32_t header[4]        = {0};
   intfstream_t *file        = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open BSV file for recording, path : \"%s\".\n", path);
      return false;
   }

   handle->file             = file;

   content_crc              = content_get_crc();

   /* This value is supposed to show up as
    * BSV1 in a HEX editor, big-endian. */
   header[MAGIC_INDEX]      = swap_if_little32(BSV_MAGIC);
   header[CRC_INDEX]        = swap_if_big32(content_crc);

   core_serialize_size(&info);

   state_size               = (unsigned)info.size;

   header[STATE_SIZE_INDEX] = swap_if_big32(state_size);

   intfstream_write(handle->file, header, 4 * sizeof(uint32_t));

   handle->min_file_pos     = sizeof(header) + state_size;
   handle->state_size       = state_size;

   if (state_size)
   {
      retro_ctx_serialize_info_t serial_info;
      uint8_t *st      = (uint8_t*)malloc(state_size);

      if (!st)
         return false;

      handle->state    = st;

      serial_info.data = handle->state;
      serial_info.size = state_size;

      core_serialize(&serial_info);

      intfstream_write(handle->file,
            handle->state, state_size);
   }

   return true;
}

static void bsv_movie_free(bsv_movie_t *handle)
{
   if (!handle)
      return;

   intfstream_close(handle->file);
   free(handle->file);

   free(handle->state);
   free(handle->frame_pos);
   free(handle);
}

static bsv_movie_t *bsv_movie_init_internal(const char *path,
      enum rarch_movie_type type)
{
   size_t *frame_pos   = NULL;
   bsv_movie_t *handle = (bsv_movie_t*)calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   if (type == RARCH_MOVIE_PLAYBACK)
   {
      if (!bsv_movie_init_playback(handle, path))
         goto error;
   }
   else if (!bsv_movie_init_record(handle, path))
      goto error;

   /* Just pick something really large
    * ~1 million frames rewind should do the trick. */
   if (!(frame_pos = (size_t*)calloc((1 << 20), sizeof(size_t))))
      goto error;

   handle->frame_pos       = frame_pos;

   handle->frame_pos[0]    = handle->min_file_pos;
   handle->frame_mask      = (1 << 20) - 1;

   return handle;

error:
   bsv_movie_free(handle);
   return NULL;
}

void bsv_movie_frame_rewind(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   bsv_movie_t         *handle = p_rarch->bsv_movie_state_handle;

   if (!handle)
      return;

   handle->did_rewind = true;

   if (     (handle->frame_ptr <= 1)
         && (handle->frame_pos[0] == handle->min_file_pos))
   {
      /* If we're at the beginning... */
      handle->frame_ptr = 0;
      intfstream_seek(handle->file, (int)handle->min_file_pos, SEEK_SET);
   }
   else
   {
      /* First time rewind is performed, the old frame is simply replayed.
       * However, playing back that frame caused us to read data, and push
       * data to the ring buffer.
       *
       * Sucessively rewinding frames, we need to rewind past the read data,
       * plus another. */
      handle->frame_ptr = (handle->frame_ptr -
            (handle->first_rewind ? 1 : 2)) & handle->frame_mask;
      intfstream_seek(handle->file,
            (int)handle->frame_pos[handle->frame_ptr], SEEK_SET);
   }

   if (intfstream_tell(handle->file) <= (long)handle->min_file_pos)
   {
      /* We rewound past the beginning. */

      if (!handle->playback)
      {
         retro_ctx_serialize_info_t serial_info;

         /* If recording, we simply reset
          * the starting point. Nice and easy. */

         intfstream_seek(handle->file, 4 * sizeof(uint32_t), SEEK_SET);

         serial_info.data = handle->state;
         serial_info.size = handle->state_size;

         core_serialize(&serial_info);

         intfstream_write(handle->file, handle->state, handle->state_size);
      }
      else
         intfstream_seek(handle->file, (int)handle->min_file_pos, SEEK_SET);
   }
}

static bool bsv_movie_init_handle(const char *path,
      enum rarch_movie_type type)
{
   struct rarch_state     *p_rarch = &rarch_st;
   bsv_movie_t *state              = bsv_movie_init_internal(path, type);
   if (!state)
      return false;

   p_rarch->bsv_movie_state_handle = state;
   return true;
}

static bool bsv_movie_init(void)
{
   bool        set_granularity = false;
   struct rarch_state *p_rarch = &rarch_st;

   if (p_rarch->bsv_movie_state.movie_start_playback)
   {
      if (!bsv_movie_init_handle(p_rarch->bsv_movie_state.movie_start_path,
                  RARCH_MOVIE_PLAYBACK))
      {
         RARCH_ERR("%s: \"%s\".\n",
               msg_hash_to_str(MSG_FAILED_TO_LOAD_MOVIE_FILE),
               p_rarch->bsv_movie_state.movie_start_path);
         return false;
      }

      p_rarch->bsv_movie_state.movie_playback = true;
      runloop_msg_queue_push(msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK),
            2, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s.\n", msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK));

      set_granularity = true;
   }
   else if (p_rarch->bsv_movie_state.movie_start_recording)
   {
      if (!bsv_movie_init_handle(p_rarch->bsv_movie_state.movie_start_path,
                  RARCH_MOVIE_RECORD))
      {
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD),
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_ERR("%s.\n",
               msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
         return false;
      }

      {
         char msg[8192];
         snprintf(msg, sizeof(msg),
               "%s \"%s\".",
               msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
               p_rarch->bsv_movie_state.movie_start_path);

         runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
               p_rarch->bsv_movie_state.movie_start_path);
      }

      set_granularity = true;
   }

   if (set_granularity)
   {
      settings_t *settings    = p_rarch->configuration_settings;
      configuration_set_uint(settings,
            settings->uints.rewind_granularity, 1);
   }

   return true;
}

static void bsv_movie_deinit(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->bsv_movie_state_handle)
      bsv_movie_free(p_rarch->bsv_movie_state_handle);
   p_rarch->bsv_movie_state_handle = NULL;
}

static bool runloop_check_movie_init(void)
{
   char msg[16384], path[8192];
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   int state_slot              = settings->ints.state_slot;

   msg[0] = path[0]            = '\0';

   configuration_set_uint(settings, settings->uints.rewind_granularity, 1);

   if (state_slot > 0)
      snprintf(path, sizeof(path), "%s%d.bsv",
            p_rarch->bsv_movie_state.movie_path,
            state_slot);
   else
   {
      strlcpy(path, p_rarch->bsv_movie_state.movie_path, sizeof(path));
      strlcat(path, ".bsv", sizeof(path));
   }


   snprintf(msg, sizeof(msg), "%s \"%s\".",
         msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
         path);

   bsv_movie_init_handle(path, RARCH_MOVIE_RECORD);

   if (!p_rarch->bsv_movie_state_handle)
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD),
            2, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
      return false;
   }

   runloop_msg_queue_push(msg, 2, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s \"%s\".\n",
         msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
         path);

   return true;
}

static bool bsv_movie_check(void)
{
   struct rarch_state *p_rarch = &rarch_st;

   if (!p_rarch->bsv_movie_state_handle)
      return runloop_check_movie_init();

   if (p_rarch->bsv_movie_state.movie_playback)
   {
      /* Checks if movie is being played back. */
      if (!p_rarch->bsv_movie_state.movie_end)
         return false;
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED), 2, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED));

      bsv_movie_deinit();

      p_rarch->bsv_movie_state.movie_end      = false;
      p_rarch->bsv_movie_state.movie_playback = false;

      return true;
   }

   /* Checks if movie is being recorded. */
   if (!p_rarch->bsv_movie_state_handle)
      return false;

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED), 2, 180, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED));

   bsv_movie_deinit();

   return true;
}

/* INPUT OVERLAY */

#ifdef HAVE_OVERLAY
static bool video_driver_overlay_interface(
      const video_overlay_interface_t **iface);

/**
 * input_overlay_add_inputs:
 * @ol : pointer to overlay
 * @port : the user to show the inputs of
 *
 * Adds inputs from current_input to the overlay, so it's displayed
 * returns true if an input that is pressed will change the overlay
 */
static bool input_overlay_add_inputs_inner(overlay_desc_t *desc,
      unsigned port, unsigned analog_dpad_mode)
{
   switch(desc->type)
   {
      case OVERLAY_TYPE_BUTTONS:
         {
            unsigned i;
            bool all_buttons_pressed        = false;

            /*Check each bank of the mask*/
            for (i = 0; i < ARRAY_SIZE(desc->button_mask.data); ++i)
            {
               /*Get bank*/
               uint32_t bank_mask = BITS_GET_ELEM(desc->button_mask,i);
               unsigned        id = i * 32;

               /*Worth pursuing? Have we got any bits left in here?*/
               while (bank_mask)
               {
                  /*If this bit is set then we need to query the pad
                   *The button must be pressed.*/
                  if (bank_mask & 1)
                  {
                     /* Light up the button if pressed */
                     if (!input_state(port, RETRO_DEVICE_JOYPAD, 0, id))
                     {
                        /* We need ALL of the inputs to be active,
                         * abort. */
                        desc->updated    = false;
                        return false;
                     }

                     all_buttons_pressed = true;
                     desc->updated       = true;
                  }

                  bank_mask >>= 1;
                  ++id;
               }
            }

            return all_buttons_pressed;
         }

      case OVERLAY_TYPE_ANALOG_LEFT:
      case OVERLAY_TYPE_ANALOG_RIGHT:
         {
            unsigned int index = (desc->type == OVERLAY_TYPE_ANALOG_RIGHT) ?
               RETRO_DEVICE_INDEX_ANALOG_RIGHT : RETRO_DEVICE_INDEX_ANALOG_LEFT;

            float analog_x     = input_state(port, RETRO_DEVICE_ANALOG,
                  index, RETRO_DEVICE_ID_ANALOG_X);
            float analog_y     = input_state(port, RETRO_DEVICE_ANALOG,
                  index, RETRO_DEVICE_ID_ANALOG_Y);
            float dx           = (analog_x/0x8000)*(desc->range_x/2);
            float dy           = (analog_y/0x8000)*(desc->range_y/2);

            desc->delta_x      = dx;
            desc->delta_y      = dy;

            /*Maybe use some option here instead of 0, only display
              changes greater than some magnitude.
              */
            if ((dx * dx) > 0 || (dy*dy) > 0)
               return true;
         }
         break;

      case OVERLAY_TYPE_KEYBOARD:
         if (input_state(port, RETRO_DEVICE_KEYBOARD, 0, desc->retro_key_idx))
         {
            desc->updated  = true;
            return true;
         }
         break;

      default:
         break;
   }

   return false;
}

static bool input_overlay_add_inputs(input_overlay_t *ol,
      unsigned port, unsigned analog_dpad_mode)
{
   unsigned i;
   bool button_pressed             = false;
   input_overlay_state_t *ol_state = &ol->overlay_state;

   if (!ol_state)
      return false;

   for (i = 0; i < ol->active->size; i++)
   {
      overlay_desc_t *desc  = &(ol->active->descs[i]);
      button_pressed       |= input_overlay_add_inputs_inner(desc,
            port, analog_dpad_mode);
   }

   return button_pressed;
}
/**
 * input_overlay_scale:
 * @ol                    : Overlay handle.
 * @scale                 : Scaling factor.
 *
 * Scales overlay and all its associated descriptors
 * by a given scaling factor (@scale).
 **/
static void input_overlay_scale(struct overlay *ol, float scale)
{
   size_t i;

   if (ol->block_scale)
      scale = 1.0f;

   ol->scale = scale;
   ol->mod_w = ol->w * scale;
   ol->mod_h = ol->h * scale;
   ol->mod_x = ol->center_x +
      (ol->x - ol->center_x) * scale;
   ol->mod_y = ol->center_y +
      (ol->y - ol->center_y) * scale;

   for (i = 0; i < ol->size; i++)
   {
      struct overlay_desc *desc = &ol->descs[i];
      float scale_w             = ol->mod_w * desc->range_x;
      float scale_h             = ol->mod_h * desc->range_y;
      float adj_center_x        = ol->mod_x + desc->x * ol->mod_w;
      float adj_center_y        = ol->mod_y + desc->y * ol->mod_h;

      desc->mod_w               = 2.0f * scale_w;
      desc->mod_h               = 2.0f * scale_h;
      desc->mod_x               = adj_center_x - scale_w;
      desc->mod_y               = adj_center_y - scale_h;
   }
}

static void input_overlay_set_vertex_geom(input_overlay_t *ol)
{
   size_t i;

   if (ol->active->image.pixels)
      ol->iface->vertex_geom(ol->iface_data, 0,
            ol->active->mod_x, ol->active->mod_y,
            ol->active->mod_w, ol->active->mod_h);

   if (ol->iface->vertex_geom)
      for (i = 0; i < ol->active->size; i++)
      {
         struct overlay_desc *desc = &ol->active->descs[i];

         if (!desc->image.pixels)
            continue;

         ol->iface->vertex_geom(ol->iface_data, desc->image_index,
               desc->mod_x, desc->mod_y, desc->mod_w, desc->mod_h);
      }
}

/**
 * input_overlay_set_scale_factor:
 * @ol                    : Overlay handle.
 * @scale                 : Factor of scale to apply.
 *
 * Scales the overlay by a factor of scale.
 **/
static void input_overlay_set_scale_factor(input_overlay_t *ol, float scale)
{
   size_t i;

   if (!ol)
      return;

   for (i = 0; i < ol->size; i++)
      input_overlay_scale(&ol->overlays[i], scale);

   input_overlay_set_vertex_geom(ol);
}

void input_overlay_free_overlay(struct overlay *overlay)
{
   size_t i;

   if (!overlay)
      return;

   for (i = 0; i < overlay->size; i++)
      image_texture_free(&overlay->descs[i].image);

   if (overlay->load_images)
      free(overlay->load_images);
   overlay->load_images = NULL;
   if (overlay->descs)
      free(overlay->descs);
   overlay->descs       = NULL;
   image_texture_free(&overlay->image);
}

static void input_overlay_free_overlays(input_overlay_t *ol)
{
   size_t i;

   if (!ol || !ol->overlays)
      return;

   for (i = 0; i < ol->size; i++)
      input_overlay_free_overlay(&ol->overlays[i]);

   free(ol->overlays);
   ol->overlays = NULL;
}

static enum overlay_visibility input_overlay_get_visibility(int overlay_idx)
{
   struct rarch_state         *p_rarch = &rarch_st;
   enum overlay_visibility *visibility = p_rarch->overlay_visibility;;

    if (!visibility)
       return OVERLAY_VISIBILITY_DEFAULT;
    if ((overlay_idx < 0) || (overlay_idx >= MAX_VISIBILITY))
       return OVERLAY_VISIBILITY_DEFAULT;
    return visibility[overlay_idx];
}


static bool input_overlay_is_hidden(int overlay_idx)
{
    return (input_overlay_get_visibility(overlay_idx)
          == OVERLAY_VISIBILITY_HIDDEN);
}

/**
 * input_overlay_set_alpha_mod:
 * @ol                    : Overlay handle.
 * @mod                   : New modulating factor to apply.
 *
 * Sets a modulating factor for alpha channel. Default is 1.0.
 * The alpha factor is applied for all overlays.
 **/
static void input_overlay_set_alpha_mod(input_overlay_t *ol, float mod)
{
   unsigned i;

   if (!ol)
      return;

   for (i = 0; i < ol->active->load_images_size; i++)
   {
      if (input_overlay_is_hidden(i))
          ol->iface->set_alpha(ol->iface_data, i, 0.0);
      else
          ol->iface->set_alpha(ol->iface_data, i, mod);
   }
}


static void input_overlay_load_active(input_overlay_t *ol, float opacity)
{
   if (ol->iface->load)
      ol->iface->load(ol->iface_data, ol->active->load_images,
            ol->active->load_images_size);

   input_overlay_set_alpha_mod(ol, opacity);
   input_overlay_set_vertex_geom(ol);

   if (ol->iface->full_screen)
      ol->iface->full_screen(ol->iface_data, ol->active->full_screen);
}

/* Attempts to automatically rotate the specified overlay.
 * Depends upon proper naming conventions in overlay
 * config file. */
static void input_overlay_auto_rotate_(input_overlay_t *ol)
{
   size_t i;
   enum overlay_orientation screen_orientation         = OVERLAY_ORIENTATION_NONE;
   enum overlay_orientation active_overlay_orientation = OVERLAY_ORIENTATION_NONE;
   struct rarch_state                         *p_rarch = &rarch_st;
   settings_t *settings                                = p_rarch->configuration_settings;
   bool input_overlay_enable                           = settings->bools.input_overlay_enable;
   bool next_overlay_found                             = false;
   bool tmp                                            = false;
   unsigned next_overlay_index                         = 0;

   /* Sanity check */
   if (!ol)
      return;

   if (!ol->alive || !input_overlay_enable)
      return;

   /* Get current screen orientation */
   if (p_rarch->video_driver_width > p_rarch->video_driver_height)
      screen_orientation = OVERLAY_ORIENTATION_LANDSCAPE;
   else
      screen_orientation = OVERLAY_ORIENTATION_PORTRAIT;

   /* Get orientation of active overlay */
   if (!string_is_empty(ol->active->name))
   {
      if (strstr(ol->active->name, "landscape"))
         active_overlay_orientation = OVERLAY_ORIENTATION_LANDSCAPE;
      else if (strstr(ol->active->name, "portrait"))
         active_overlay_orientation = OVERLAY_ORIENTATION_PORTRAIT;
   }

   /* Sanity check */
   if (active_overlay_orientation == OVERLAY_ORIENTATION_NONE)
      return;

   /* If screen and overlay have the same orientation,
    * no action is required */
   if (screen_orientation == active_overlay_orientation)
      return;

   /* Attempt to find index of overlay corresponding
    * to opposite orientation */
   for (i = 0; i < p_rarch->overlay_ptr->active->size; i++)
   {
      overlay_desc_t *desc = &p_rarch->overlay_ptr->active->descs[i];

      if (!desc)
         continue;

      if (!string_is_empty(desc->next_index_name))
      {
         if (active_overlay_orientation == OVERLAY_ORIENTATION_LANDSCAPE)
            next_overlay_found = (strstr(desc->next_index_name, "portrait") != 0);
         else
            next_overlay_found = (strstr(desc->next_index_name, "landscape") != 0);

         if (next_overlay_found)
         {
            next_overlay_index = desc->next_index;
            break;
         }
      }
   }

   /* Sanity check */
   if (!next_overlay_found)
      return;

   /* We have a valid target overlay
    * > Trigger 'overly next' command event
    * Note: tmp == false. This prevents CMD_EVENT_OVERLAY_NEXT
    * from calling input_overlay_auto_rotate_() again */
   ol->next_index = next_overlay_index;
   command_event(CMD_EVENT_OVERLAY_NEXT, &tmp);
}

/**
 * inside_hitbox:
 * @desc                  : Overlay descriptor handle.
 * @x                     : X coordinate value.
 * @y                     : Y coordinate value.
 *
 * Check whether the given @x and @y coordinates of the overlay
 * descriptor @desc is inside the overlay descriptor's hitbox.
 *
 * Returns: true (1) if X, Y coordinates are inside a hitbox,
 * otherwise false (0).
 **/
static bool inside_hitbox(const struct overlay_desc *desc, float x, float y)
{
   if (!desc)
      return false;

   switch (desc->hitbox)
   {
      case OVERLAY_HITBOX_RADIAL:
      {
         /* Ellipsis. */
         float x_dist  = (x - desc->x) / desc->range_x_mod;
         float y_dist  = (y - desc->y) / desc->range_y_mod;
         float sq_dist = x_dist * x_dist + y_dist * y_dist;
         return (sq_dist <= 1.0f);
      }

      case OVERLAY_HITBOX_RECT:
         return
            (fabs(x - desc->x) <= desc->range_x_mod) &&
            (fabs(y - desc->y) <= desc->range_y_mod);
   }

   return false;
}

/**
 * input_overlay_poll:
 * @out                   : Polled output data.
 * @norm_x                : Normalized X coordinate.
 * @norm_y                : Normalized Y coordinate.
 *
 * Polls input overlay.
 *
 * @norm_x and @norm_y are the result of
 * input_translate_coord_viewport().
 **/
static void input_overlay_poll(
      input_overlay_t *ol,
      input_overlay_state_t *out,
      int16_t norm_x, int16_t norm_y)
{
   size_t i;

   /* norm_x and norm_y is in [-0x7fff, 0x7fff] range,
    * like RETRO_DEVICE_POINTER. */
   float x = (float)(norm_x + 0x7fff) / 0xffff;
   float y = (float)(norm_y + 0x7fff) / 0xffff;

   x -= ol->active->mod_x;
   y -= ol->active->mod_y;
   x /= ol->active->mod_w;
   y /= ol->active->mod_h;

   for (i = 0; i < ol->active->size; i++)
   {
      float x_dist, y_dist;
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!inside_hitbox(desc, x, y))
         continue;

      desc->updated = true;
      x_dist        = x - desc->x;
      y_dist        = y - desc->y;

      switch (desc->type)
      {
         case OVERLAY_TYPE_BUTTONS:
            {
               bits_or_bits(out->buttons.data,
                     desc->button_mask.data,
                     ARRAY_SIZE(desc->button_mask.data));

               if (BIT256_GET(desc->button_mask, RARCH_OVERLAY_NEXT))
                  ol->next_index = desc->next_index;
            }
            break;
         case OVERLAY_TYPE_KEYBOARD:
            if (desc->retro_key_idx < RETROK_LAST)
               OVERLAY_SET_KEY(out, desc->retro_key_idx);
            break;
         default:
            {
               float x_val           = x_dist / desc->range_x;
               float y_val           = y_dist / desc->range_y;
               float x_val_sat       = x_val / desc->analog_saturate_pct;
               float y_val_sat       = y_val / desc->analog_saturate_pct;

               unsigned int base     =
                  (desc->type == OVERLAY_TYPE_ANALOG_RIGHT)
                  ? 2 : 0;

               out->analog[base + 0] = clamp_float(x_val_sat, -1.0f, 1.0f)
                  * 32767.0f;
               out->analog[base + 1] = clamp_float(y_val_sat, -1.0f, 1.0f)
                  * 32767.0f;
            }
            break;
      }

      if (desc->movable)
      {
         desc->delta_x = clamp_float(x_dist, -desc->range_x, desc->range_x)
            * ol->active->mod_w;
         desc->delta_y = clamp_float(y_dist, -desc->range_y, desc->range_y)
            * ol->active->mod_h;
      }
   }

   if (!bits_any_set(out->buttons.data, ARRAY_SIZE(out->buttons.data)))
      ol->blocked = false;
   else if (ol->blocked)
      memset(out, 0, sizeof(*out));
}

/**
 * input_overlay_update_desc_geom:
 * @ol                    : overlay handle.
 * @desc                  : overlay descriptors handle.
 *
 * Update input overlay descriptors' vertex geometry.
 **/
static void input_overlay_update_desc_geom(input_overlay_t *ol,
      struct overlay_desc *desc)
{
   if (!desc->image.pixels || !desc->movable)
      return;

   if (ol->iface->vertex_geom)
      ol->iface->vertex_geom(ol->iface_data, desc->image_index,
            desc->mod_x + desc->delta_x, desc->mod_y + desc->delta_y,
            desc->mod_w, desc->mod_h);

   desc->delta_x = 0.0f;
   desc->delta_y = 0.0f;
}

/**
 * input_overlay_post_poll:
 *
 * Called after all the input_overlay_poll() calls to
 * update the range modifiers for pressed/unpressed regions
 * and alpha mods.
 **/
static void input_overlay_post_poll(input_overlay_t *ol, float opacity)
{
   size_t i;

   input_overlay_set_alpha_mod(ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      desc->range_x_mod = desc->range_x;
      desc->range_y_mod = desc->range_y;

      if (desc->updated)
      {
         /* If pressed this frame, change the hitbox. */
         desc->range_x_mod *= desc->range_mod;
         desc->range_y_mod *= desc->range_mod;

         if (desc->image.pixels)
         {
            if (ol->iface->set_alpha)
               ol->iface->set_alpha(ol->iface_data, desc->image_index,
                     desc->alpha_mod * opacity);
         }
      }

      input_overlay_update_desc_geom(ol, desc);
      desc->updated = false;
   }
}

/**
 * input_overlay_poll_clear:
 * @ol                    : overlay handle
 *
 * Call when there is nothing to poll. Allows overlay to
 * clear certain state.
 **/
static void input_overlay_poll_clear(input_overlay_t *ol, float opacity)
{
   size_t i;

   ol->blocked = false;

   input_overlay_set_alpha_mod(ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      desc->range_x_mod = desc->range_x;
      desc->range_y_mod = desc->range_y;
      desc->updated     = false;

      desc->delta_x     = 0.0f;
      desc->delta_y     = 0.0f;
      input_overlay_update_desc_geom(ol, desc);
   }
}


/**
 * input_overlay_free:
 * @ol                    : Overlay handle.
 *
 * Frees overlay handle.
 **/
static void input_overlay_free(input_overlay_t *ol)
{
   if (!ol)
      return;

   input_overlay_free_overlays(ol);

   if (ol->iface->enable)
      ol->iface->enable(ol->iface_data, false);

   free(ol);
}

/* task_data = overlay_task_data_t* */
static void input_overlay_loaded(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   size_t i;
   struct rarch_state            *p_rarch = &rarch_st;
   overlay_task_data_t              *data = (overlay_task_data_t*)task_data;
   input_overlay_t                    *ol = NULL;
   const video_overlay_interface_t *iface = NULL;
   settings_t *settings                   = p_rarch->configuration_settings;
   bool input_overlay_show_mouse_cursor   = settings->bools.input_overlay_show_mouse_cursor;
   bool inp_overlay_auto_rotate           = settings->bools.input_overlay_auto_rotate;

   if (err)
      return;

#ifdef HAVE_MENU
   /* We can't display when the menu is up */
   if (data->hide_in_menu && p_rarch->menu_driver_alive)
   {
      if (data->overlay_enable)
         goto abort_load;
   }
#endif

   if (  !data->overlay_enable                   ||
         !video_driver_overlay_interface(&iface) ||
         !iface)
   {
      RARCH_ERR("Overlay interface is not present in video driver,"
            " or not enabled.\n");
      goto abort_load;
   }

   ol             = (input_overlay_t*)calloc(1, sizeof(*ol));
   ol->overlays   = data->overlays;
   ol->size       = data->size;
   ol->active     = data->active;
   ol->iface      = iface;
   ol->iface_data = video_driver_get_ptr_internal(true);

   input_overlay_load_active(ol, data->overlay_opacity);

   /* Enable or disable the overlay. */
   ol->enable = data->overlay_enable;

   if (ol->iface->enable)
      ol->iface->enable(ol->iface_data, data->overlay_enable);

   input_overlay_set_scale_factor(ol, data->overlay_scale);

   ol->next_index = (unsigned)((ol->index + 1) % ol->size);
   ol->state      = OVERLAY_STATUS_NONE;
   ol->alive      = true;

   /* Due to the asynchronous nature of overlay loading
    * it is possible for overlay_ptr to be non-NULL here
    * > Ensure it is free()'d before assigning new pointer */
   if (p_rarch->overlay_ptr)
   {
      input_overlay_free_overlays(p_rarch->overlay_ptr);
      free(p_rarch->overlay_ptr);
   }
   p_rarch->overlay_ptr = ol;

   free(data);

   if (!input_overlay_show_mouse_cursor)
      video_driver_hide_mouse();

   /* Attempt to automatically rotate overlay, if required */
   if (inp_overlay_auto_rotate)
      input_overlay_auto_rotate_(p_rarch->overlay_ptr);

   return;

abort_load:
   for (i = 0; i < data->size; i++)
      input_overlay_free_overlay(&data->overlays[i]);

   free(data->overlays);
   free(data);
}

void input_overlay_set_visibility(int overlay_idx,
      enum overlay_visibility vis)
{
   struct rarch_state         *p_rarch = &rarch_st;
   input_overlay_t                 *ol = p_rarch->overlay_ptr;

   if (!p_rarch->overlay_visibility)
   {
      unsigned i;
      p_rarch->overlay_visibility = (enum overlay_visibility *)calloc(
            MAX_VISIBILITY, sizeof(enum overlay_visibility));

      for (i = 0; i < MAX_VISIBILITY; i++)
         p_rarch->overlay_visibility[i] = OVERLAY_VISIBILITY_DEFAULT;
   }

   p_rarch->overlay_visibility[overlay_idx] = vis;

   if (!ol)
      return;
   if (vis == OVERLAY_VISIBILITY_HIDDEN)
      ol->iface->set_alpha(ol->iface_data, overlay_idx, 0.0);
}

bool input_overlay_key_pressed(input_overlay_t *ol, unsigned key)
{
   input_overlay_state_t *ol_state  = ol ? &ol->overlay_state : NULL;
   if (!ol)
      return false;
   return (BIT256_GET(ol_state->buttons, key));
}

/*
 * input_poll_overlay:
 *
 * Poll pressed buttons/keys on currently active overlay.
 **/
static void input_poll_overlay(input_overlay_t *ol, float opacity,
      unsigned analog_dpad_mode,
      float axis_threshold)
{
   rarch_joypad_info_t joypad_info;
   input_overlay_state_t old_key_state;
   unsigned i, j, device;
   uint16_t key_mod                                 = 0;
   bool polled                                      = false;
   bool button_pressed                              = false;
   struct rarch_state *p_rarch                      = &rarch_st;
   void *input_data                                 = p_rarch->current_input_data;
   input_overlay_state_t *ol_state                  = &ol->overlay_state;
   input_driver_t *input_ptr                        = p_rarch->current_input;
   settings_t *settings                             = p_rarch->configuration_settings;
   bool input_overlay_show_physical_inputs          = settings->bools.input_overlay_show_physical_inputs;
   unsigned input_overlay_show_physical_inputs_port = settings->uints.input_overlay_show_physical_inputs_port;

   if (!ol_state)
      return;

   joypad_info.joy_idx             = 0;
   joypad_info.auto_binds          = NULL;
   joypad_info.axis_threshold      = 0.0f;

   memcpy(old_key_state.keys, ol_state->keys,
         sizeof(ol_state->keys));
   memset(ol_state, 0, sizeof(*ol_state));

   device = ol->active->full_screen ?
      RARCH_DEVICE_POINTER_SCREEN : RETRO_DEVICE_POINTER;

   for (i = 0;
         input_ptr->input_state(input_data, &joypad_info,
            NULL,
            0, device, i, RETRO_DEVICE_ID_POINTER_PRESSED);
         i++)
   {
      input_overlay_state_t polled_data;
      int16_t x = input_ptr->input_state(input_data, &joypad_info,
            NULL,
            0, device, i, RETRO_DEVICE_ID_POINTER_X);
      int16_t y = input_ptr->input_state(input_data, &joypad_info,
            NULL,
            0, device, i, RETRO_DEVICE_ID_POINTER_Y);

      memset(&polled_data, 0, sizeof(struct input_overlay_state));

      if (ol->enable)
         input_overlay_poll(ol, &polled_data, x, y);
      else
         ol->blocked = false;

      bits_or_bits(ol_state->buttons.data,
            polled_data.buttons.data,
            ARRAY_SIZE(polled_data.buttons.data));

      for (j = 0; j < ARRAY_SIZE(ol_state->keys); j++)
         ol_state->keys[j] |= polled_data.keys[j];

      /* Fingers pressed later take priority and matched up
       * with overlay poll priorities. */
      for (j = 0; j < 4; j++)
         if (polled_data.analog[j])
            ol_state->analog[j] = polled_data.analog[j];

      polled = true;
   }

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LSHIFT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RSHIFT))
      key_mod |= RETROKMOD_SHIFT;

   if (OVERLAY_GET_KEY(ol_state, RETROK_LCTRL) ||
       OVERLAY_GET_KEY(ol_state, RETROK_RCTRL))
      key_mod |= RETROKMOD_CTRL;

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LALT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RALT))
      key_mod |= RETROKMOD_ALT;

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LMETA) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RMETA))
      key_mod |= RETROKMOD_META;

   /* CAPSLOCK SCROLLOCK NUMLOCK */
   for (i = 0; i < ARRAY_SIZE(ol_state->keys); i++)
   {
      if (ol_state->keys[i] != old_key_state.keys[i])
      {
         uint32_t orig_bits = old_key_state.keys[i];
         uint32_t new_bits  = ol_state->keys[i];

         for (j = 0; j < 32; j++)
            if ((orig_bits & (1 << j)) != (new_bits & (1 << j)))
               input_keyboard_event(new_bits & (1 << j),
                     i * 32 + j, 0, key_mod, RETRO_DEVICE_POINTER);
      }
   }

   /* Map "analog" buttons to analog axes like regular input drivers do. */
   for (j = 0; j < 4; j++)
   {
      unsigned bind_plus  = RARCH_ANALOG_LEFT_X_PLUS + 2 * j;
      unsigned bind_minus = bind_plus + 1;

      if (ol_state->analog[j])
         continue;

      if ((BIT256_GET(ol->overlay_state.buttons, bind_plus)))
         ol_state->analog[j] += 0x7fff;
      if ((BIT256_GET(ol->overlay_state.buttons, bind_minus)))
         ol_state->analog[j] -= 0x7fff;
   }

   /* Check for analog_dpad_mode.
    * Map analogs to d-pad buttons when configured. */
   switch (analog_dpad_mode)
   {
      case ANALOG_DPAD_LSTICK:
      case ANALOG_DPAD_RSTICK:
      {
         float analog_x, analog_y;
         unsigned analog_base = 2;

         if (analog_dpad_mode == ANALOG_DPAD_LSTICK)
            analog_base = 0;

         analog_x = (float)ol_state->analog[analog_base + 0] / 0x7fff;
         analog_y = (float)ol_state->analog[analog_base + 1] / 0x7fff;

         if (analog_x <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_LEFT);
         if (analog_x >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         if (analog_y <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_UP);
         if (analog_y >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_DOWN);
         break;
      }

      default:
         break;
   }

   if (input_overlay_show_physical_inputs)
      button_pressed = input_overlay_add_inputs(ol,
            input_overlay_show_physical_inputs_port,
            analog_dpad_mode);

   if (button_pressed || polled)
      input_overlay_post_poll(ol, opacity);
   else
      input_overlay_poll_clear(ol, opacity);
}

static void retroarch_overlay_deinit(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   input_overlay_free(p_rarch->overlay_ptr);
   p_rarch->overlay_ptr = NULL;
}

static void retroarch_overlay_init(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   bool input_overlay_enable   = settings->bools.input_overlay_enable;
   const char *path_overlay    = settings->paths.path_overlay;
   float overlay_opacity       = settings->floats.input_overlay_opacity;
   float overlay_scale         = settings->floats.input_overlay_scale;
   bool load_enabled           = input_overlay_enable;
#ifdef HAVE_MENU
   bool overlay_hide_in_menu   = settings->bools.input_overlay_hide_in_menu;
#else
   bool overlay_hide_in_menu   = false;
#endif
#if defined(GEKKO)
   /* Avoid a crash at startup or even when toggling overlay in rgui */
   uint64_t memory_free        = frontend_driver_get_free_memory();
   if (memory_free < (3 * 1024 * 1024))
      return;
#endif

   retroarch_overlay_deinit();

#ifdef HAVE_MENU
   /* Cancel load if 'hide_in_menu' is enabled and
    * menu is currently active */
   if (overlay_hide_in_menu)
      load_enabled = load_enabled && !p_rarch->menu_driver_alive;
#endif

   if (load_enabled)
      task_push_overlay_load_default(input_overlay_loaded,
            path_overlay,
            overlay_hide_in_menu,
            input_overlay_enable,
            overlay_opacity,
            overlay_scale,
            NULL);
}
#endif

/* INPUT REMOTE */

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
static bool input_remote_init_network(input_remote_t *handle,
      uint16_t port, unsigned user)
{
   int fd;
   struct addrinfo *res  = NULL;

   port = port + user;

   if (!network_init())
      return false;

   RARCH_LOG("Bringing up remote interface on port %hu.\n",
         (unsigned short)port);

   fd = socket_init((void**)&res, port, NULL, SOCKET_TYPE_DATAGRAM);

   if (fd < 0)
      goto error;

   handle->net_fd[user] = fd;

   if (!socket_nonblock(handle->net_fd[user]))
      goto error;

   if (!socket_bind(handle->net_fd[user], res))
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_BIND_SOCKET));
      goto error;
   }

   freeaddrinfo_retro(res);
   return true;

error:
   if (res)
      freeaddrinfo_retro(res);
   return false;
}

static void input_remote_free(input_remote_t *handle, unsigned max_users)
{
   unsigned user;
   for (user = 0; user < max_users; user ++)
      socket_close(handle->net_fd[user]);

   free(handle);
}

static input_remote_t *input_remote_new(uint16_t port, unsigned max_users)
{
   unsigned user;
   struct rarch_state *p_rarch = &rarch_st;
   settings_t        *settings = p_rarch->configuration_settings;
   input_remote_t      *handle = (input_remote_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   for (user = 0; user < max_users; user ++)
   {
      handle->net_fd[user] = -1;
      if (settings->bools.network_remote_enable_user[user])
         if (!input_remote_init_network(handle, port, user))
         {
            input_remote_free(handle, max_users);
            return NULL;
         }
   }

   return handle;
}

static void input_remote_parse_packet(struct remote_message *msg, unsigned user)
{
   struct rarch_state *p_rarch        = &rarch_st;
   input_remote_state_t *input_state  = &p_rarch->remote_st_ptr;

   /* Parse message */
   switch (msg->device)
   {
      case RETRO_DEVICE_JOYPAD:
         input_state->buttons[user] &= ~(1 << msg->id);
         if (msg->state)
            input_state->buttons[user] |= 1 << msg->id;
         break;
      case RETRO_DEVICE_ANALOG:
         input_state->analog[msg->index * 2 + msg->id][user] = msg->state;
         break;
   }
}
#endif

/* INPUT */

void set_connection_listener(pad_connection_listener_t *listener)
{
   struct rarch_state *p_rarch      = &rarch_st;
   p_rarch->pad_connection_listener = listener;
}

static void fire_connection_listener(
      unsigned port, input_device_driver_t *driver)
{
   struct rarch_state *p_rarch      = &rarch_st;
   if (p_rarch->pad_connection_listener)
      p_rarch->pad_connection_listener->connected(port, driver);
}

/**
 * input_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to input driver at index. Can be NULL
 * if nothing found.
 **/
static const void *input_driver_find_handle(int idx)
{
   const void *drv = input_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * config_get_input_driver_options:
 *
 * Get an enumerated list of all input driver names, separated by '|'.
 *
 * Returns: string listing of all input driver names, separated by '|'.
 **/
const char* config_get_input_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_DRIVERS, NULL);
}

void *input_get_data(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->current_input_data;
}

/**
 * input_driver_set_rumble_state:
 * @port               : User number.
 * @effect             : Rumble effect.
 * @strength           : Strength of rumble effect.
 *
 * Sets the rumble state.
 * Used by RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE.
 **/
bool input_driver_set_rumble_state(unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->current_input || !p_rarch->current_input->set_rumble)
      return false;
   return p_rarch->current_input->set_rumble(p_rarch->current_input_data,
         port, effect, strength);
}

const input_device_driver_t *input_driver_get_joypad_driver(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->current_input || !p_rarch->current_input->get_joypad_driver)
      return NULL;
   return p_rarch->current_input->get_joypad_driver(p_rarch->current_input_data);
}

const input_device_driver_t *input_driver_get_sec_joypad_driver(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->current_input || !p_rarch->current_input->get_sec_joypad_driver)
      return NULL;
   return p_rarch->current_input->get_sec_joypad_driver(p_rarch->current_input_data);
}

uint64_t input_driver_get_capabilities(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->current_input || !p_rarch->current_input->get_capabilities)
      return 0;
   return p_rarch->current_input->get_capabilities(p_rarch->current_input_data);
}

/**
 * input_sensor_set_state:
 * @port               : User number.
 * @effect             : Sensor action.
 * @rate               : Sensor rate update.
 *
 * Sets the sensor state.
 * Used by RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE.
 **/
bool input_sensor_set_state(unsigned port,
      enum retro_sensor_action action, unsigned rate)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->current_input_data &&
         p_rarch->current_input->set_sensor_state)
      return p_rarch->current_input->set_sensor_state(p_rarch->current_input_data,
            port, action, rate);
   return false;
}

float input_sensor_get_input(unsigned port, unsigned id)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->current_input_data &&
         p_rarch->current_input->get_sensor_input)
      return p_rarch->current_input->get_sensor_input(p_rarch->current_input_data,
            port, id);
   return 0.0f;
}

/**
 * input_poll:
 *
 * Input polling callback function.
 **/
static void input_driver_poll(void)
{
   size_t i;
   rarch_joypad_info_t joypad_info[MAX_USERS];
   struct rarch_state    *p_rarch = &rarch_st;
   settings_t *settings           = p_rarch->configuration_settings;
#ifdef HAVE_OVERLAY
   float input_overlay_opacity    = settings->floats.input_overlay_opacity;
#endif
   bool input_remap_binds_enable  = settings->bools.input_remap_binds_enable;
   uint8_t max_users              = (uint8_t)p_rarch->input_driver_max_users;
   input_bits_t current_inputs[MAX_USERS];

   p_rarch->current_input->poll(p_rarch->current_input_data);

   p_rarch->input_driver_turbo_btns.count++;

   for (i = 0; i < max_users; i++)
      p_rarch->input_driver_turbo_btns.frame_enable[i] = 0;

   if (p_rarch->input_driver_block_libretro_input)
      return;

   for (i = 0; i < max_users; i++)
   {
      joypad_info[i].axis_threshold              = p_rarch->input_driver_axis_threshold;
      joypad_info[i].joy_idx                     = settings->uints.input_joypad_map[i];
      joypad_info[i].auto_binds                  = input_autoconf_binds[joypad_info[i].joy_idx];
      if (!p_rarch->libretro_input_binds[i][RARCH_TURBO_ENABLE].valid)
         continue;

      p_rarch->input_driver_turbo_btns.frame_enable[i] = p_rarch->current_input->input_state(
            p_rarch->current_input_data, &joypad_info[i], p_rarch->libretro_input_binds,
            (unsigned)i, RETRO_DEVICE_JOYPAD, 0, RARCH_TURBO_ENABLE);
   }

#ifdef HAVE_OVERLAY
   if (p_rarch->overlay_ptr && p_rarch->overlay_ptr->alive)
      input_poll_overlay(
            p_rarch->overlay_ptr,
            input_overlay_opacity,
            settings->uints.input_analog_dpad_mode[0],
            p_rarch->input_driver_axis_threshold);
#endif

#ifdef HAVE_MENU
   if (!p_rarch->menu_driver_alive)
#endif
   if (input_remap_binds_enable && p_rarch->input_driver_mapper)
   {
      for (i = 0; i < max_users; i++)
      {
         unsigned device = settings->uints.input_libretro_device[i] & RETRO_DEVICE_MASK;

         switch (device)
         {
            case RETRO_DEVICE_KEYBOARD:
            case RETRO_DEVICE_JOYPAD:
            case RETRO_DEVICE_ANALOG:
               BIT256_CLEAR_ALL_PTR(&current_inputs[i]);
               {
                  unsigned k, j;
                  const input_device_driver_t *joypad_driver   = input_driver_get_joypad_driver();
                  input_bits_t *p_new_state                    = (input_bits_t*)&current_inputs[i];

                  if (joypad_driver)
                  {
                     int16_t ret = 0;
                     if (p_rarch->current_input && p_rarch->current_input->input_state)
                        ret = p_rarch->current_input->input_state(
                              p_rarch->current_input_data,
                              &joypad_info[i],
                              p_rarch->libretro_input_binds,
                              (unsigned)i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);

                     for (k = 0; k < RARCH_FIRST_CUSTOM_BIND; k++)
                     {
                        if (ret & (1 << k))
                        {
                           int16_t      val = input_joypad_analog(
                                 joypad_driver, &joypad_info[i], (unsigned)i,
                                 RETRO_DEVICE_INDEX_ANALOG_BUTTON, k,
                                 p_rarch->libretro_input_binds[i]);

                           BIT256_SET_PTR(p_new_state, k);

                           if (val)
                              p_new_state->analog_buttons[k] = val;
                        }
                     }

                     for (k = 0; k < 2; k++)
                     {
                        for (j = 0; j < 2; j++)
                        {
                           unsigned offset = 0 + (k * 4) + (j * 2);
                           int16_t     val = input_joypad_analog(joypad_driver,
                                 &joypad_info[i], (unsigned)i, k, j,
                                 p_rarch->libretro_input_binds[i]);

                           if (val >= 0)
                              p_new_state->analogs[offset]   = val;
                           else
                              p_new_state->analogs[offset+1] = val;
                        }
                     }
                  }
               }
               break;
            default:
               break;
         }

      }
      input_mapper_poll(p_rarch->input_driver_mapper,
#ifdef HAVE_OVERLAY
            p_rarch->overlay_ptr,
#else
            NULL,
#endif
            settings,
            &current_inputs,
            max_users,
#ifdef HAVE_OVERLAY
            (p_rarch->overlay_ptr && p_rarch->overlay_ptr->alive)
#else
            false
#endif
            );
   }

#ifdef HAVE_COMMAND
   if (p_rarch->input_driver_command)
   {
      memset(p_rarch->input_driver_command->state,
            0, sizeof(p_rarch->input_driver_command->state));
#if defined(HAVE_NETWORK_CMD) && defined(HAVE_COMMAND)
      command_network_poll(p_rarch->input_driver_command);
#endif

#ifdef HAVE_STDIN_CMD
      if (p_rarch->input_driver_command->stdin_enable)
         command_stdin_poll(p_rarch->input_driver_command);
#endif
   }
#endif

#ifdef HAVE_NETWORKGAMEPAD
   /* Poll remote */
   if (p_rarch->input_driver_remote)
   {
      unsigned user;

      for (user = 0; user < max_users; user++)
      {
         if (settings->bools.network_remote_enable_user[user])
         {
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
            struct remote_message msg;
            ssize_t ret;
            fd_set fds;

            if (p_rarch->input_driver_remote->net_fd[user] < 0)
               return;

            FD_ZERO(&fds);
            FD_SET(p_rarch->input_driver_remote->net_fd[user], &fds);

            ret = recvfrom(p_rarch->input_driver_remote->net_fd[user], (char*)&msg,
                  sizeof(msg), 0, NULL, NULL);

            if (ret == sizeof(msg))
               input_remote_parse_packet(&msg, user);
            else if ((ret != -1) || ((errno != EAGAIN) && (errno != ENOENT)))
#endif
            {
               input_remote_state_t *input_state  = &p_rarch->remote_st_ptr;
               input_state->buttons[user]         = 0;
               input_state->analog[0][user]       = 0;
               input_state->analog[1][user]       = 0;
               input_state->analog[2][user]       = 0;
               input_state->analog[3][user]       = 0;
            }
         }
      }
   }
#endif
}

static int16_t input_state_device(
      int16_t ret,
      unsigned port, unsigned device,
      unsigned idx, unsigned id,
      bool button_mask)
{
#ifdef HAVE_NETWORKGAMEPAD
   bool remote_input             = false;
#endif
   int16_t res                   = 0;
   struct rarch_state   *p_rarch = &rarch_st;
   settings_t *settings          = p_rarch->configuration_settings;
   bool input_remap_binds_enable = settings->bools.input_remap_binds_enable;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:

#ifdef HAVE_NETWORKGAMEPAD
         if (p_rarch->input_driver_remote)
         {
            if (input_remote_key_pressed(id, port))
            {
               res |= 1;
               remote_input = true;
            }
         }
#endif

         if (id < RARCH_FIRST_META_KEY
#ifdef HAVE_NETWORKGAMEPAD
               /* Don't process binds if input is coming from Remote RetroPad */
               && !remote_input
#endif
            )
         {
            bool bind_valid = p_rarch->libretro_input_binds[port]
               && p_rarch->libretro_input_binds[port][id].valid;

            if (input_remap_binds_enable &&
                  id != settings->uints.input_remap_ids[port][id])
               res = 0;
            else if (bind_valid)
            {
               if (button_mask)
               {
                  res = 0;
                  if (ret & (1 << id))
                     res |= (1 << id);
               }
               else
                  res = ret;

#ifdef HAVE_OVERLAY
               {
                  int16_t res_overlay  = 0;
                  if (     p_rarch->overlay_ptr 
                        && port == 0 
                        && p_rarch->overlay_ptr->alive)
                  {
                     if ((BIT256_GET(p_rarch->overlay_ptr->overlay_state.buttons, id)))
                        res_overlay |= 1;
                     if (p_rarch->overlay_ptr->alive)
                        res |= res_overlay;
                  }
               }
#endif
            }
         }

         if (input_remap_binds_enable && p_rarch->input_driver_mapper)
            if (BIT256_GET(p_rarch->input_driver_mapper->buttons[port], id))
               res = 1;

         /* Don't allow turbo for D-pad. */
         if ((id < RETRO_DEVICE_ID_JOYPAD_UP || id > RETRO_DEVICE_ID_JOYPAD_RIGHT))
         {
            /*
             * Apply turbo button if activated.
             */
            if (settings->uints.input_turbo_mode == 1)
            {
               /* Pressing turbo button toggles turbo mode on or off.
                * Holding the button will
                * pass through, else the pressed state will be modulated by a
                * periodic pulse defined by the configured duty cycle.
                */

               /* Avoid detecting the turbo button being held as multiple toggles */
               if (!p_rarch->input_driver_turbo_btns.frame_enable[port])
                  p_rarch->input_driver_turbo_btns.turbo_pressed[port] &= ~(1 << 31);
               else if (p_rarch->input_driver_turbo_btns.turbo_pressed[port]>=0)
               {
                  p_rarch->input_driver_turbo_btns.turbo_pressed[port] |= (1 << 31);
                  /* Toggle turbo for selected buttons. */
                  if (!p_rarch->input_driver_turbo_btns.enable[port])
                  {
                     static const int button_map[]={
                        RETRO_DEVICE_ID_JOYPAD_B,
                        RETRO_DEVICE_ID_JOYPAD_Y,
                        RETRO_DEVICE_ID_JOYPAD_A,
                        RETRO_DEVICE_ID_JOYPAD_X,
                        RETRO_DEVICE_ID_JOYPAD_L,
                        RETRO_DEVICE_ID_JOYPAD_R,
                        RETRO_DEVICE_ID_JOYPAD_L2,
                        RETRO_DEVICE_ID_JOYPAD_R2,
                        RETRO_DEVICE_ID_JOYPAD_L3,
                        RETRO_DEVICE_ID_JOYPAD_R3};
                     p_rarch->input_driver_turbo_btns.enable[port] = 1 << button_map[
                        MIN(
                              sizeof(button_map)/sizeof(button_map[0])-1,
                              settings->uints.input_turbo_default_button)];
                  }
                  p_rarch->input_driver_turbo_btns.mode1_enable[port] ^= 1;
               }

               if (p_rarch->input_driver_turbo_btns.turbo_pressed[port] & (1 << 31))
               {
                  /* Avoid detecting buttons being held as multiple toggles */
                  if (!res)
                     p_rarch->input_driver_turbo_btns.turbo_pressed[port] &= ~(1 << id);
                  else if (!(p_rarch->input_driver_turbo_btns.turbo_pressed[port] & (1 << id)))
                  {
                     uint16_t enable_new;
                     p_rarch->input_driver_turbo_btns.turbo_pressed[port] |= 1 << id;
                     /* Toggle turbo for pressed button but make
                      * sure at least one button has turbo */
                     enable_new = p_rarch->input_driver_turbo_btns.enable[port] ^ (1 << id);
                     if (enable_new)
                        p_rarch->input_driver_turbo_btns.enable[port] = enable_new;
                  }
               }

               if (!res && p_rarch->input_driver_turbo_btns.mode1_enable[port] &&
                     p_rarch->input_driver_turbo_btns.enable[port] & (1 << id))
               {
                  /* if turbo button is enabled for this key ID */
                  res = ((p_rarch->input_driver_turbo_btns.count
                           % settings->uints.input_turbo_period)
                        < settings->uints.input_turbo_duty_cycle);
               }
            }
            else
            {
               /* If turbo button is held, all buttons pressed except
                * for D-pad will go into a turbo mode. Until the button is
                * released again, the input state will be modulated by a
                * periodic pulse defined by the configured duty cycle.
                */
               if (res && p_rarch->input_driver_turbo_btns.frame_enable[port])
                  p_rarch->input_driver_turbo_btns.enable[port] |= (1 << id);
               else if (!res)
                  p_rarch->input_driver_turbo_btns.enable[port] &= ~(1 << id);

               if (p_rarch->input_driver_turbo_btns.enable[port] & (1 << id))
               {
                  /* if turbo button is enabled for this key ID */
                  res = res && ((p_rarch->input_driver_turbo_btns.count
                           % settings->uints.input_turbo_period)
                        < settings->uints.input_turbo_duty_cycle);
               }
            }
         }

         break;

      case RETRO_DEVICE_MOUSE:

         if (id < RARCH_FIRST_META_KEY)
         {
            bool bind_valid = p_rarch->libretro_input_binds[port]
               && p_rarch->libretro_input_binds[port][id].valid;

            if (bind_valid)
            {
               if (button_mask)
               {
                  res = 0;
                  if (ret & (1 << id))
                     res |= (1 << id);
               }
               else
                  res = ret;
            }
         }

         break;

      case RETRO_DEVICE_KEYBOARD:

         res = ret;

#ifdef HAVE_OVERLAY
         if (p_rarch->overlay_ptr && port == 0)
         {
            int16_t res_overlay  = 0;
            if (id < RETROK_LAST)
            {
               input_overlay_state_t *ol_state = &p_rarch->overlay_ptr->overlay_state;
               if (OVERLAY_GET_KEY(ol_state, id))
                  res_overlay |= 1;
            }

            if (p_rarch->overlay_ptr->alive)
               res |= res_overlay;
         }
#endif

         if (input_remap_binds_enable && p_rarch->input_driver_mapper)
            if (id < RETROK_LAST)
               if (MAPPER_GET_KEY(p_rarch->input_driver_mapper, id))
                  res |= 1;

         break;

      case RETRO_DEVICE_LIGHTGUN:

         if (id < RARCH_FIRST_META_KEY)
         {
            bool bind_valid = p_rarch->libretro_input_binds[port]
               && p_rarch->libretro_input_binds[port][id].valid;

            if (bind_valid)
            {
               if (button_mask)
               {
                  res = 0;
                  if (ret & (1 << id))
                     res |= (1 << id);
               }
               else
                  res = ret;
            }
         }

         break;

      case RETRO_DEVICE_ANALOG:
         {
#ifdef HAVE_OVERLAY
            int16_t res_overlay     = 0;
            if (p_rarch->overlay_ptr && port == 0)
            {
               unsigned                   base = 0;
               input_overlay_state_t *ol_state = 
                  &p_rarch->overlay_ptr->overlay_state;

               if (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                  base = 2;
               if (id == RETRO_DEVICE_ID_ANALOG_Y)
                  base += 1;
               if (ol_state && ol_state->analog[base])
                  res_overlay = ol_state->analog[base];
            }
#endif

#ifdef HAVE_NETWORKGAMEPAD
            if (p_rarch->input_driver_remote)
            {
               input_remote_state_t *input_state  = &p_rarch->remote_st_ptr;

               if (input_state)
               {
                  unsigned base = 0;
                  if (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                     base = 2;
                  if (id == RETRO_DEVICE_ID_ANALOG_Y)
                     base += 1;
                  if (input_state->analog[base][port])
                  {
                     res = input_state->analog[base][port];
                     remote_input = true;
                  }
               }
            }
#endif

            if (id < RARCH_FIRST_META_KEY
#ifdef HAVE_NETWORKGAMEPAD
                  && !remote_input
#endif
                )
            {
               bool bind_valid         = p_rarch->libretro_input_binds[port]
                  && p_rarch->libretro_input_binds[port][id].valid;

               if (bind_valid)
               {
                  /* reset_state - used to reset input state of a button
                   * when the gamepad mapper is in action for that button*/
                  bool reset_state        = false;
                  if (input_remap_binds_enable)
                  {
                     if (idx < 2 && id < 2)
                     {
                        unsigned offset = RARCH_FIRST_CUSTOM_BIND +
                           (idx * 4) + (id * 2);

                        if (settings->uints.input_remap_ids
                              [port][offset]   != offset)
                           reset_state = true;
                        else if (settings->uints.input_remap_ids
                              [port][offset+1] != (offset+1))
                           reset_state = true;
                     }
                  }

                  if (!reset_state)
                  {
                     res = ret;

#ifdef HAVE_OVERLAY
                     if (  p_rarch->overlay_ptr        && 
                           p_rarch->overlay_ptr->alive && port == 0)
                        res |= res_overlay;
#endif
                  }
                  else
                     res = 0;
               }
            }

            if (input_remap_binds_enable && p_rarch->input_driver_mapper)
            {
               if (idx < 2 && id < 2)
               {
                  int         val = 0;
                  unsigned offset = 0 + (idx * 4) + (id * 2);
                  int        val1 = p_rarch->input_driver_mapper->analog_value[port][offset];
                  int        val2 = p_rarch->input_driver_mapper->analog_value[port][offset+1];

                  if (val1)
                     val          = val1;
                  else if (val2)
                     val          = val2;

                  if (val1 || val2)
                     res        |= val;
               }
            }
         }
         break;

      case RETRO_DEVICE_POINTER:

         if (id < RARCH_FIRST_META_KEY)
         {
            bool bind_valid = p_rarch->libretro_input_binds[port]
               && p_rarch->libretro_input_binds[port][id].valid;

            if (bind_valid)
            {
               if (button_mask)
               {
                  res = 0;
                  if (ret & (1 << id))
                     res |= (1 << id);
               }
               else
                  res = ret;
            }
         }

         break;
   }

   return res;
}

/**
 * input_state:
 * @port                 : user number.
 * @device               : device identifier of user.
 * @idx                  : index value of user.
 * @id                   : identifier of key pressed by user.
 *
 * Input state callback function.
 *
 * Returns: Non-zero if the given key (identified by @id)
 * was pressed by the user (assigned to @port).
 **/
static int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   rarch_joypad_info_t joypad_info;
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   int16_t result              = 0;
   int16_t ret                 = 0;

   joypad_info.axis_threshold  = p_rarch->input_driver_axis_threshold;
   joypad_info.joy_idx         = settings->uints.input_joypad_map[port];
   joypad_info.auto_binds      = input_autoconf_binds[joypad_info.joy_idx];

   if (BSV_MOVIE_IS_PLAYBACK_ON())
   {
      int16_t bsv_result;
      if (intfstream_read(p_rarch->bsv_movie_state_handle->file, &bsv_result, 2) == 2)
         return swap_if_big16(bsv_result);
      p_rarch->bsv_movie_state.movie_end = true;
   }

   device &= RETRO_DEVICE_MASK;
   ret     = p_rarch->current_input->input_state(
         p_rarch->current_input_data, &joypad_info,
         p_rarch->libretro_input_binds, port, device, idx, id);

   if (     (p_rarch->input_driver_flushing_input == 0)
         && !p_rarch->input_driver_block_libretro_input)
   {
      if (  (device == RETRO_DEVICE_JOYPAD) &&
            (id == RETRO_DEVICE_ID_JOYPAD_MASK))
      {
         unsigned i;

         {
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               if (input_state_device(ret, port, device, idx, i, true))
                  result |= (1 << i);
         }
      }
      else
         result = input_state_device(ret, port, device, idx, id, false);
   }

   if (BSV_MOVIE_IS_PLAYBACK_OFF())
   {
      result = swap_if_big16(result);
      intfstream_write(p_rarch->bsv_movie_state_handle->file, &result, 2);
   }

   return result;
}

static INLINE bool input_keys_pressed_other_sources(unsigned i,
      input_bits_t* p_new_state)
{
   struct rarch_state   *p_rarch = &rarch_st;

#ifdef HAVE_OVERLAY
   if (p_rarch->overlay_ptr &&
         ((BIT256_GET(p_rarch->overlay_ptr->overlay_state.buttons, i))))
      return true;
#endif

#ifdef HAVE_COMMAND
   if (p_rarch->input_driver_command)
      return ((i < RARCH_BIND_LIST_END)
         && p_rarch->input_driver_command->state[i]);
#endif

#ifdef HAVE_NETWORKGAMEPAD
   /* Only process key presses related to game input if using Remote RetroPad */
   if (i < RARCH_CUSTOM_BIND_LIST_END &&
         p_rarch->input_driver_remote &&
            input_remote_key_pressed(i, 0))
      return true;
#endif

   return false;
}

static int16_t input_joypad_axis(const input_device_driver_t *drv,
      unsigned port, uint32_t joyaxis, float normal_mag)
{
   struct rarch_state  *p_rarch   = &rarch_st;
   settings_t *settings           = p_rarch->configuration_settings;
   float input_analog_deadzone    = settings->floats.input_analog_deadzone;
   float input_analog_sensitivity = settings->floats.input_analog_sensitivity;
   int16_t val                    = drv->axis(port, joyaxis);

   if (input_analog_deadzone)
   {
      /* if analog value is below the deadzone, ignore it
       * normal magnitude is calculated radially for analog sticks
       * and linearly for analog buttons */
      if (normal_mag <= input_analog_deadzone)
         return 0;

      /* due to the way normal_mag is calculated differently for buttons and
       * sticks, this results in either a radial scaled deadzone for sticks
       * or linear scaled deadzone for analog buttons */
      val = val * MAX(1.0f,(1.0f / normal_mag)) * MIN(1.0f,((normal_mag - input_analog_deadzone)
          / (1.0f - input_analog_deadzone)));
   }

   if (input_analog_sensitivity != 1.0f)
   {
      float normalized = (1.0f / 0x7fff) * val;
      int      new_val = 0x7fff * normalized  *
         input_analog_sensitivity;

      if (new_val > 0x7fff)
         return 0x7fff;
      else if (new_val < -0x7fff)
         return -0x7fff;

      return new_val;
   }

   return val;
}

/* MENU INPUT */
#ifdef HAVE_MENU
/* Must be called inside menu_driver_toggle()
 * Prevents phantom input when using an overlay to
 * toggle menu ON if overlays are disabled in-menu */

static void menu_input_driver_toggle(bool on)
{
#ifdef HAVE_OVERLAY
   struct rarch_state *p_rarch  = &rarch_st;
   settings_t *settings         = p_rarch->configuration_settings;

   if (on)
   {
      bool overlay_hide_in_menu = settings->bools.input_overlay_hide_in_menu;
      bool input_overlay_enable = settings->bools.input_overlay_enable;
      /* If an overlay was displayed before the toggle
       * and overlays are disabled in menu, need to
       * inhibit 'select' input */
      if (overlay_hide_in_menu)
         if (  input_overlay_enable && 
               p_rarch->overlay_ptr && 
               p_rarch->overlay_ptr->alive)
            menu_input_set_pointer_inhibit(true);
   }
   else
#endif
      menu_input_set_pointer_inhibit(false);
}

int16_t menu_input_read_mouse_hw(enum menu_input_mouse_hw_id id)
{
   rarch_joypad_info_t joypad_info;
   unsigned type                   = 0;
   unsigned device                 = RETRO_DEVICE_MOUSE;
   struct rarch_state *p_rarch     = &rarch_st;

   joypad_info.joy_idx             = 0;
   joypad_info.auto_binds          = NULL;
   joypad_info.axis_threshold      = 0.0f;

   switch (id)
   {
      case MENU_MOUSE_X_AXIS:
         device = RARCH_DEVICE_MOUSE_SCREEN;
         type = RETRO_DEVICE_ID_MOUSE_X;
         break;
      case MENU_MOUSE_Y_AXIS:
         device = RARCH_DEVICE_MOUSE_SCREEN;
         type = RETRO_DEVICE_ID_MOUSE_Y;
         break;
      case MENU_MOUSE_LEFT_BUTTON:
         type = RETRO_DEVICE_ID_MOUSE_LEFT;
         break;
      case MENU_MOUSE_RIGHT_BUTTON:
         type = RETRO_DEVICE_ID_MOUSE_RIGHT;
         break;
      case MENU_MOUSE_WHEEL_UP:
         type = RETRO_DEVICE_ID_MOUSE_WHEELUP;
         break;
      case MENU_MOUSE_WHEEL_DOWN:
         type = RETRO_DEVICE_ID_MOUSE_WHEELDOWN;
         break;
      case MENU_MOUSE_HORIZ_WHEEL_UP:
         type = RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP;
         break;
      case MENU_MOUSE_HORIZ_WHEEL_DOWN:
         type = RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN;
         break;
   }

   return p_rarch->current_input->input_state(
         p_rarch->current_input_data, &joypad_info,
         NULL, 0, device, 0, type);
}

static void menu_input_get_mouse_hw_state(
      menu_input_pointer_hw_state_t *hw_state)
{
   struct rarch_state *p_rarch     = &rarch_st;
   settings_t *settings            = p_rarch->configuration_settings;
   static int16_t last_x           = 0;
   static int16_t last_y           = 0;
   static bool last_select_pressed = false;
   static bool last_cancel_pressed = false;
   bool mouse_enabled              = settings->bools.menu_mouse_enable;
   menu_handle_t *menu_data        = menu_driver_get_ptr();
   bool menu_has_fb                =
      (menu_data && 
       menu_data->driver_ctx && 
       menu_data->driver_ctx->set_texture);
#ifdef HAVE_OVERLAY
   bool overlay_enable             = settings->bools.input_overlay_enable;
   /* Menu pointer controls are ignored when overlays are enabled. */
   bool overlay_active             = overlay_enable && p_rarch->overlay_ptr 
      && p_rarch->overlay_ptr->alive;
   if (overlay_active)
      mouse_enabled = false;
#endif

   /* Easiest to set inactive by default, and toggle
    * when input is detected */
   hw_state->active  = false;


   if (!mouse_enabled)
   {
      hw_state->x              = 0;
      hw_state->y              = 0;
      hw_state->select_pressed = false;
      hw_state->cancel_pressed = false;
      hw_state->up_pressed     = false;
      hw_state->down_pressed   = false;
      hw_state->left_pressed   = false;
      hw_state->right_pressed  = false;
      return;
   }

   /* X pos */
   hw_state->x = menu_input_read_mouse_hw(MENU_MOUSE_X_AXIS);
   if (hw_state->x != last_x)
      hw_state->active = true;
   last_x = hw_state->x;

   /* Y pos */
   hw_state->y = menu_input_read_mouse_hw(MENU_MOUSE_Y_AXIS);
   if (hw_state->y != last_y)
      hw_state->active = true;
   last_y = hw_state->y;

   /* > X/Y adjustment */
   if (menu_has_fb)
   {
      /* RGUI uses a framebuffer texture + custom viewports,
       * which means we have to convert from screen space to
       * menu space... */
      size_t fb_pitch;
      unsigned fb_width, fb_height;
      struct video_viewport vp = {0};

      /* Read display/framebuffer info */
      gfx_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);
      video_driver_get_viewport_info(&vp);

      /* Adjust X pos */
      hw_state->x = (int16_t)(((float)(hw_state->x - vp.x) / (float)vp.width) * (float)fb_width);
      hw_state->x = hw_state->x <  0        ? 0            : hw_state->x;
      hw_state->x = hw_state->x >= fb_width ? fb_width - 1 : hw_state->x;

      /* Adjust Y pos */
      hw_state->y = (int16_t)(((float)(hw_state->y - vp.y) / (float)vp.height) * (float)fb_height);
      hw_state->y = hw_state->y <  0         ? 0             : hw_state->y;
      hw_state->y = hw_state->y >= fb_height ? fb_height - 1 : hw_state->y;
   }

   /* Select (LMB)
    * Note that releasing select also counts as activity */
   hw_state->select_pressed = (bool)menu_input_read_mouse_hw(MENU_MOUSE_LEFT_BUTTON);
   if (hw_state->select_pressed || (hw_state->select_pressed != last_select_pressed))
      hw_state->active = true;
   last_select_pressed = hw_state->select_pressed;

   /* Cancel (RMB)
    * Note that releasing cancel also counts as activity */
   hw_state->cancel_pressed = (bool)menu_input_read_mouse_hw(MENU_MOUSE_RIGHT_BUTTON);
   if (hw_state->cancel_pressed || (hw_state->cancel_pressed != last_cancel_pressed))
      hw_state->active = true;
   last_cancel_pressed = hw_state->cancel_pressed;

   /* Up (mouse wheel up) */
   hw_state->up_pressed = (bool)menu_input_read_mouse_hw(MENU_MOUSE_WHEEL_UP);
   if (hw_state->up_pressed)
      hw_state->active = true;

   /* Down (mouse wheel down) */
   hw_state->down_pressed = (bool)menu_input_read_mouse_hw(MENU_MOUSE_WHEEL_DOWN);
   if (hw_state->down_pressed)
      hw_state->active = true;

   /* Left (mouse wheel horizontal left) */
   hw_state->left_pressed = (bool)menu_input_read_mouse_hw(MENU_MOUSE_HORIZ_WHEEL_DOWN);
   if (hw_state->left_pressed)
      hw_state->active = true;

   /* Right (mouse wheel horizontal right) */
   hw_state->right_pressed = (bool)menu_input_read_mouse_hw(MENU_MOUSE_HORIZ_WHEEL_UP);
   if (hw_state->right_pressed)
      hw_state->active = true;
}

static void menu_input_get_touchscreen_hw_state(
      menu_input_pointer_hw_state_t *hw_state)
{
   rarch_joypad_info_t joypad_info;
   int pointer_x, pointer_y;
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   struct rarch_state *p_rarch                  = &rarch_st;
   settings_t *settings                         = p_rarch->configuration_settings;
   const struct retro_keybind *binds[MAX_USERS] = {NULL};
   menu_handle_t *menu_data                     = menu_driver_get_ptr();
   /* Is a background texture set for the current menu driver?
    * Checks if the menu framebuffer is set.
    * This would usually only return true
    * for framebuffer-based menu drivers, like RGUI. */
   int pointer_device                           =
         (menu_data && menu_data->driver_ctx && menu_data->driver_ctx->set_texture) ?
               RETRO_DEVICE_POINTER : RARCH_DEVICE_POINTER_SCREEN;
   static int16_t last_x                        = 0;
   static int16_t last_y                        = 0;
   static bool last_select_pressed              = false;
   static bool last_cancel_pressed              = false;
   bool overlay_active                          = false;
   bool pointer_enabled                         = settings->bools.menu_pointer_enable;

   /* Easiest to set inactive by default, and toggle
    * when input is detected */
   hw_state->active        = false;

   /* Touch screens don't have mouse wheels, so these
    * are always disabled */
   hw_state->up_pressed    = false;
   hw_state->down_pressed  = false;
   hw_state->left_pressed  = false;
   hw_state->right_pressed = false;

#ifdef HAVE_OVERLAY
   /* Menu pointer controls are ignored when overlays are enabled. */
   overlay_active          = settings->bools.input_overlay_enable 
      && p_rarch->overlay_ptr && p_rarch->overlay_ptr->alive;
   if (overlay_active)
      pointer_enabled = false;
#endif

   /* If touchscreen is disabled, ignore all input */
   if (!pointer_enabled)
   {
      hw_state->x              = 0;
      hw_state->y              = 0;
      hw_state->select_pressed = false;
      hw_state->cancel_pressed = false;
      return;
   }

   gfx_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);

   joypad_info.joy_idx        = 0;
   joypad_info.auto_binds     = NULL;
   joypad_info.axis_threshold = 0.0f;

   /* X pos */
   pointer_x                  = p_rarch->current_input->input_state(
         p_rarch->current_input_data, &joypad_info, binds,
         0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_X);
   hw_state->x = ((pointer_x + 0x7fff) * (int)fb_width) / 0xFFFF;

   /* > An annoyance - we get different starting positions
    *   depending upon whether pointer_device is
    *   RETRO_DEVICE_POINTER or RARCH_DEVICE_POINTER_SCREEN,
    *   so different 'activity' checks are required to prevent
    *   false positives on first run */
   if (pointer_device == RARCH_DEVICE_POINTER_SCREEN)
   {
      if (hw_state->x != last_x)
         hw_state->active = true;
      last_x = hw_state->x;
   }
   else
   {
      if (pointer_x != last_x)
         hw_state->active = true;
      last_x = pointer_x;
   }

   /* Y pos */
   pointer_y = p_rarch->current_input->input_state(
         p_rarch->current_input_data, &joypad_info, binds,
         0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_Y);
   hw_state->y = ((pointer_y + 0x7fff) * (int)fb_height) / 0xFFFF;

   if (pointer_device == RARCH_DEVICE_POINTER_SCREEN)
   {
      if (hw_state->y != last_y)
         hw_state->active = true;
      last_y = hw_state->y;
   }
   else
   {
      if (pointer_y != last_y)
         hw_state->active = true;
      last_y = pointer_y;
   }

   /* Select (touch screen contact)
    * Note that releasing select also counts as activity */
   hw_state->select_pressed = (bool)p_rarch->current_input->input_state(
         p_rarch->current_input_data, &joypad_info, binds,
         0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
   if (hw_state->select_pressed || (hw_state->select_pressed != last_select_pressed))
      hw_state->active = true;
   last_select_pressed = hw_state->select_pressed;

   /* Cancel (touch screen 'back' - don't know what is this, but whatever...)
    * Note that releasing cancel also counts as activity */
   hw_state->cancel_pressed = (bool)p_rarch->current_input->input_state(
         p_rarch->current_input_data, &joypad_info, binds,
         0, pointer_device, 0, RARCH_DEVICE_ID_POINTER_BACK);
   if (hw_state->cancel_pressed || (hw_state->cancel_pressed != last_cancel_pressed))
      hw_state->active = true;
   last_cancel_pressed = hw_state->cancel_pressed;
}

/*
 * This function gets called in order to process all input events
 * for the current frame.
 *
 * Sends input code to menu for one frame.
 *
 * It uses as input the local variables 'input' and 'trigger_input'.
 *
 * Mouse and touch input events get processed inside this function.
 *
 * NOTE: 'input' and 'trigger_input' is sourced from the keyboard and/or
 * the gamepad. It does not contain input state derived from the mouse
 * and/or touch - this gets dealt with separately within this function.
 *
 * TODO/FIXME - maybe needs to be overhauled so we can send multiple
 * events per frame if we want to, and we shouldn't send the
 * entire button state either but do a separate event per button
 * state.
 */
static unsigned menu_event(
      input_bits_t *p_input,
      input_bits_t *p_trigger_input,
      bool display_kb)
{
   /* Used for key repeat */
   static float delay_timer                        = 0.0f;
   static float delay_count                        = 0.0f;
   static bool initial_held                        = true;
   static bool first_held                          = false;
   static unsigned ok_old                          = 0;
   unsigned ret                                    = MENU_ACTION_NOOP;
   bool set_scroll                                 = false;
   size_t new_scroll_accel                         = 0;
   struct rarch_state                   *p_rarch   = &rarch_st;
   menu_input_t *menu_input                        = &p_rarch->menu_input_state;
   menu_input_pointer_hw_state_t *pointer_hw_state = &p_rarch->menu_input_pointer_hw_state;
   settings_t *settings                            = p_rarch->configuration_settings;
   bool menu_mouse_enable                          = settings->bools.menu_mouse_enable;
   bool menu_pointer_enable                        = settings->bools.menu_pointer_enable;
   bool swap_ok_cancel_btns                        = settings->bools.input_menu_swap_ok_cancel_buttons;
   bool menu_scroll_fast                           = settings->bools.menu_scroll_fast;
   bool input_swap_override                        = input_autoconfigure_get_swap_override();
   unsigned menu_ok_btn                            =
         (!input_swap_override && swap_ok_cancel_btns) ?
               RETRO_DEVICE_ID_JOYPAD_B : RETRO_DEVICE_ID_JOYPAD_A;
   unsigned menu_cancel_btn                        =
         (!input_swap_override && swap_ok_cancel_btns) ?
               RETRO_DEVICE_ID_JOYPAD_A : RETRO_DEVICE_ID_JOYPAD_B;
   unsigned ok_current                             = BIT256_GET_PTR(p_input, menu_ok_btn);
   unsigned ok_trigger                             = ok_current & ~ok_old;
#ifdef HAVE_RGUI
   menu_handle_t *menu_data                        = menu_driver_get_ptr();
   bool menu_has_fb                                =
         (menu_data && 
          menu_data->driver_ctx && 
          menu_data->driver_ctx->set_texture);
#else
   bool menu_has_fb                                = false;
#endif

   ok_old                                          = ok_current;

   if (bits_any_set(p_input->data, ARRAY_SIZE(p_input->data)))
   {
      if (!first_held)
      {
         /* don't run anything first frame, only capture held inputs
          * for old_input_state. */

         first_held  = true;
         if (menu_scroll_fast)
            delay_timer = initial_held ? 200 : 100;
         else
            delay_timer = initial_held ? 400 : 20;
         delay_count = 0;
      }

      if (delay_count >= delay_timer)
      {
         uint32_t input_repeat = 0;
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_UP);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_DOWN);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_LEFT);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_L);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_R);

         set_scroll           = true;
         first_held           = false;
         p_trigger_input->data[0] |= p_input->data[0] & input_repeat;

         menu_driver_ctl(MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL,
               &new_scroll_accel);

         if (menu_scroll_fast)
            new_scroll_accel = MIN(new_scroll_accel + 1, 64);
         else
            new_scroll_accel = MIN(new_scroll_accel + 1, 5);
      }

      initial_held  = false;
   }
   else
   {
      set_scroll   = true;
      first_held   = false;
      initial_held = true;
   }

   if (set_scroll)
      menu_driver_ctl(MENU_NAVIGATION_CTL_SET_SCROLL_ACCEL,
            &new_scroll_accel);

   delay_count += gfx_animation_get_delta_time();

   if (display_kb)
   {
      input_event_osk_iterate();

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
      {
         int old_osk_ptr = input_event_get_osk_ptr();
         if (old_osk_ptr < 33)
            input_event_set_osk_ptr(old_osk_ptr + OSK_CHARS_PER_LINE);
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_UP))
      {
         int old_osk_ptr = input_event_get_osk_ptr();
         if (old_osk_ptr >= OSK_CHARS_PER_LINE)
            input_event_set_osk_ptr(old_osk_ptr
                  - OSK_CHARS_PER_LINE);
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_RIGHT))
      {
         int old_osk_ptr = input_event_get_osk_ptr();
         if (old_osk_ptr < 43)
            input_event_set_osk_ptr(old_osk_ptr + 1);
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_LEFT))
      {
         int old_osk_ptr = input_event_get_osk_ptr();
         if (old_osk_ptr >= 1)
            input_event_set_osk_ptr(old_osk_ptr - 1);
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_L))
      {
         enum osk_type osk_type_idx = input_event_get_osk_idx();
         if (osk_type_idx > OSK_TYPE_UNKNOWN + 1)
            input_event_set_osk_idx((enum osk_type)(
                     osk_type_idx - 1));
         else
            input_event_set_osk_idx((enum osk_type)(menu_has_fb 
                     ? OSK_SYMBOLS_PAGE1 
                     : OSK_TYPE_LAST - 1));
      }

      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_R))
      {
         enum osk_type osk_type_idx = input_event_get_osk_idx();
         if (osk_type_idx < (menu_has_fb 
                  ? OSK_SYMBOLS_PAGE1 
                  : OSK_TYPE_LAST - 1))
            input_event_set_osk_idx((enum osk_type)(
                     osk_type_idx + 1));
         else
            input_event_set_osk_idx((enum osk_type)(OSK_TYPE_UNKNOWN + 1));
      }

      if (BIT256_GET_PTR(p_trigger_input, menu_ok_btn))
      {
         int ptr = input_event_get_osk_ptr();
         if (ptr >= 0)
            input_event_osk_append(ptr, menu_has_fb);
      }

      if (BIT256_GET_PTR(p_trigger_input, menu_cancel_btn))
         input_keyboard_event(true, '\x7f', '\x7f',
               0, RETRO_DEVICE_KEYBOARD);

      /* send return key to close keyboard input window */
      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_START))
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);

      BIT256_CLEAR_ALL_PTR(p_trigger_input);
   }
   else
   {
      if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_UP))
         ret = MENU_ACTION_UP;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
         ret = MENU_ACTION_DOWN;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_LEFT))
         ret = MENU_ACTION_LEFT;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_RIGHT))
         ret = MENU_ACTION_RIGHT;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_L))
         ret = MENU_ACTION_SCROLL_UP;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_R))
         ret = MENU_ACTION_SCROLL_DOWN;
      else if (ok_trigger)
         ret = MENU_ACTION_OK;
      else if (BIT256_GET_PTR(p_trigger_input, menu_cancel_btn))
         ret = MENU_ACTION_CANCEL;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_X))
         ret = MENU_ACTION_SEARCH;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_Y))
         ret = MENU_ACTION_SCAN;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_START))
         ret = MENU_ACTION_START;
      else if (BIT256_GET_PTR(p_trigger_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
         ret = MENU_ACTION_INFO;
      else if (BIT256_GET_PTR(p_trigger_input, RARCH_MENU_TOGGLE))
         ret = MENU_ACTION_TOGGLE;
   }

   /* Get pointer (mouse + touchscreen) input */

   /* > If pointer input is disabled, do nothing */
   if (!menu_mouse_enable && !menu_pointer_enable)
      menu_input->pointer.type = MENU_POINTER_DISABLED;
   else
   {
      menu_input_pointer_hw_state_t mouse_hw_state       = {0};
      menu_input_pointer_hw_state_t touchscreen_hw_state = {0};

      /* Read mouse */
      if (menu_mouse_enable)
         menu_input_get_mouse_hw_state(&mouse_hw_state);

      /* Read touchscreen
       * Note: Could forgo this if mouse is currently active,
       * but this is 'cleaner' code... (if performance is a
       * concern - and it isn't - user can just disable touch
       * screen support) */
      if (menu_pointer_enable)
         menu_input_get_touchscreen_hw_state(&touchscreen_hw_state);

      /* Mouse takes precedence */
      if (mouse_hw_state.active)
         menu_input->pointer.type = MENU_POINTER_MOUSE;
      else if (touchscreen_hw_state.active)
         menu_input->pointer.type = MENU_POINTER_TOUCHSCREEN;

      /* Copy input from the current device */
      if (menu_input->pointer.type == MENU_POINTER_MOUSE)
         memcpy(pointer_hw_state, &mouse_hw_state, sizeof(menu_input_pointer_hw_state_t));
      else if (menu_input->pointer.type == MENU_POINTER_TOUCHSCREEN)
         memcpy(pointer_hw_state, &touchscreen_hw_state, sizeof(menu_input_pointer_hw_state_t));
   }

   /* Populate menu_input_state
    * Note: dx, dy, ptr, y_accel, etc. entries are set elsewhere */
   if (menu_input->select_inhibit || menu_input->cancel_inhibit)
   {
      menu_input->pointer.active  = false;
      menu_input->pointer.pressed = false;
      menu_input->pointer.x       = 0;
      menu_input->pointer.y       = 0;
   }
   else
   {
      menu_input->pointer.active  = pointer_hw_state->active;
      menu_input->pointer.pressed = pointer_hw_state->select_pressed;
      menu_input->pointer.x       = pointer_hw_state->x;
      menu_input->pointer.y       = pointer_hw_state->y;
   }

   return ret;
}

bool menu_input_pointer_check_vector_inside_hitbox(menu_input_ctx_hitbox_t *hitbox)
{
   struct rarch_state                   *p_rarch   = &rarch_st;
   menu_input_pointer_hw_state_t *pointer_hw_state = &p_rarch->menu_input_pointer_hw_state;
   int16_t x                                       = pointer_hw_state->x;
   int16_t y                                       = pointer_hw_state->y;
   bool inside_hitbox                              =
         (x >= hitbox->x1) &&
         (x <= hitbox->x2) &&
         (y >= hitbox->y1) &&
         (y <= hitbox->y2);

   return inside_hitbox;
}

void menu_input_get_pointer_state(menu_input_pointer_t *pointer)
{
   struct rarch_state  *p_rarch   = &rarch_st;
   menu_input_t       *menu_input = &p_rarch->menu_input_state;

   if (!pointer)
      return;

   /* Copy parameters from global menu_input_state
    * (i.e. don't pass by reference)
    * This is a fast operation */
   memcpy(pointer, &menu_input->pointer, sizeof(menu_input_pointer_t));
}

unsigned menu_input_get_pointer_selection(void)
{
   struct rarch_state  *p_rarch   = &rarch_st;
   menu_input_t       *menu_input = &p_rarch->menu_input_state;
   return menu_input->ptr;
}

void menu_input_set_pointer_selection(unsigned selection)
{
   struct rarch_state  *p_rarch   = &rarch_st;
   menu_input_t       *menu_input = &p_rarch->menu_input_state;

   menu_input->ptr                = selection;
}

void menu_input_set_pointer_y_accel(float y_accel)
{
   struct rarch_state  *p_rarch   = &rarch_st;
   menu_input_t    *menu_input    = &p_rarch->menu_input_state;

   menu_input->pointer.y_accel    = y_accel;
}

void menu_input_set_pointer_inhibit(bool inhibit)
{
   struct rarch_state  *p_rarch   = &rarch_st;
   menu_input_t     *menu_input   = &p_rarch->menu_input_state;

   menu_input->select_inhibit     = inhibit;
   menu_input->cancel_inhibit     = inhibit;
}

void menu_input_reset(void)
{
   struct rarch_state                   *p_rarch   = &rarch_st;
   menu_input_t *menu_input                        = &p_rarch->menu_input_state;
   menu_input_pointer_hw_state_t *pointer_hw_state = &p_rarch->menu_input_pointer_hw_state;

   memset(menu_input, 0, sizeof(menu_input_t));
   memset(pointer_hw_state, 0, sizeof(menu_input_pointer_hw_state_t));
}

static void menu_input_set_pointer_visibility(retro_time_t current_time)
{
   bool show_cursor                                = false;
   static bool cursor_shown                        = false;
   bool hide_cursor                                = false;
   static bool cursor_hidden                       = false;
   static retro_time_t end_time                    = 0;
   struct rarch_state                   *p_rarch   = &rarch_st;
   menu_input_t *menu_input                        = &p_rarch->menu_input_state;
   menu_input_pointer_hw_state_t *pointer_hw_state = &p_rarch->menu_input_pointer_hw_state;

   /* Ensure that mouse cursor is hidden when not in use */
   if ((menu_input->pointer.type == MENU_POINTER_MOUSE) && pointer_hw_state->active)
   {
      if ((current_time > end_time) && !cursor_shown)
         show_cursor = true;

      end_time = current_time + MENU_INPUT_HIDE_CURSOR_DELAY;
   }
   else
   {
      if ((current_time > end_time) && !cursor_hidden)
         hide_cursor = true;
   }

   if (show_cursor)
   {
      menu_ctx_environment_t menu_environ;
      menu_environ.type = MENU_ENVIRON_ENABLE_MOUSE_CURSOR;
      menu_environ.data = NULL;

      menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
      cursor_shown  = true;
      cursor_hidden = false;
   }

   if (hide_cursor)
   {
      menu_ctx_environment_t menu_environ;
      menu_environ.type = MENU_ENVIRON_DISABLE_MOUSE_CURSOR;
      menu_environ.data = NULL;

      menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
      cursor_shown  = false;
      cursor_hidden = true;
   }
}

static float menu_input_get_dpi(void)
{
   static unsigned last_video_width  = 0;
   static unsigned last_video_height = 0;
   static float dpi                  = 0.0f;
   static bool dpi_cached            = false;
   bool menu_has_fb                  = false;
   struct rarch_state       *p_rarch = &rarch_st;
   menu_handle_t *menu_data          = menu_driver_get_ptr();

   if (!menu_data)
      return 0.0f;

   menu_has_fb                       = menu_data->driver_ctx 
      && menu_data->driver_ctx->set_texture;

   /* Regardless of menu driver, need 'actual' screen DPI
    * Note: DPI is a fixed hardware property. To minimise performance
    * overheads we therefore only call video_context_driver_get_metrics()
    * on first run, or when the current video resolution changes */
   if (!dpi_cached ||
       (p_rarch->video_driver_width  != last_video_width) ||
       (p_rarch->video_driver_height != last_video_height))
   {
      gfx_ctx_metrics_t metrics;

      metrics.type  = DISPLAY_METRIC_DPI;
      metrics.value = &dpi;

      /* Note: If video_context_driver_get_metrics() fails,
       * we don't know what happened to dpi - so ensure it
       * is reset to a sane value */
      if (!video_context_driver_get_metrics(&metrics))
         dpi = 0.0f;

      dpi_cached        = true;
      last_video_width  = p_rarch->video_driver_width;
      last_video_height = p_rarch->video_driver_height;
   }

   /* RGUI uses a framebuffer texture, which means we
    * operate in menu space, not screen space.
    * DPI in a traditional sense is therefore meaningless,
    * so generate a substitute value based upon framebuffer
    * dimensions */
   if ((dpi > 0.0f) && menu_has_fb)
   {
      size_t fb_pitch;
      unsigned fb_width, fb_height;

      /* Read framebuffer info */
      gfx_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);

      /* Rationale for current 'DPI' determination method:
       * - Divide screen height by DPI, to get number of vertical
       *   '1 inch' squares
       * - Divide RGUI framebuffer height by number of vertical
       *   '1 inch' squares to get number of menu space pixels
       *   per inch
       * This is crude, but should be sufficient... */
      return ((float)fb_height / (float)p_rarch->video_driver_height) * dpi;
   }

   return dpi;
}

/* Used to close an active message box (help or info)
 * TODO/FIXME: The way that message boxes are handled
 * is complete garbage. generic_menu_iterate() and
 * message boxes in general need a total rewrite.
 * I consider this current 'close_messagebox' a hack,
 * but at least it prevents undefined/dangerous
 * behaviour... */
static void menu_input_pointer_close_messagebox(void)
{
   const char *label = NULL;
   bool pop_stack    = false;

   /* Determine whether this is a help or info
    * message box */
   menu_entries_get_last_stack(NULL, &label, NULL, NULL, NULL);

   /* > Info box */
   if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_INFO_SCREEN)))
      pop_stack = true;
   /* > Help box */
   if (string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP)) ||
       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_CONTROLS)) ||
       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE)) ||
       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_LOADING_CONTENT)) ||
       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_SCANNING_CONTENT)) ||
       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD)) ||
       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING)) ||
       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HELP_SEND_DEBUG_INFO)) ||
       string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_CHEEVOS_DESCRIPTION)))
      pop_stack = true;

   /* Pop stack, if required */
   if (pop_stack)
   {
      size_t selection     = menu_navigation_get_selection();
      size_t new_selection = selection;

      menu_entries_pop_stack(&new_selection, 0, 0);
      menu_navigation_set_selection(selection);
   }
}

static int menu_input_pointer_post_iterate(
      retro_time_t current_time,
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   static retro_time_t start_time                  = 0;
   static int16_t start_x                          = 0;
   static int16_t start_y                          = 0;
   static int16_t last_x                           = 0;
   static int16_t last_y                           = 0;
   static uint16_t dx_start_right_max              = 0;
   static uint16_t dx_start_left_max               = 0;
   static uint16_t dy_start_up_max                 = 0;
   static uint16_t dy_start_down_max               = 0;
   static bool last_select_pressed                 = false;
   static bool last_cancel_pressed                 = false;
   static bool last_left_pressed                   = false;
   static bool last_right_pressed                  = false;
   static retro_time_t last_left_action_time       = 0;
   static retro_time_t last_right_action_time      = 0;
   static retro_time_t last_press_direction_time   = 0;
   bool attenuate_y_accel                          = true;
   bool osk_active                                 = menu_input_dialog_get_display_kb();
   bool messagebox_active                          = false;
   int ret                                         = 0;
   struct rarch_state *p_rarch                     = &rarch_st;
   menu_input_pointer_hw_state_t *pointer_hw_state = &p_rarch->menu_input_pointer_hw_state;
   menu_input_t *menu_input                        = &p_rarch->menu_input_state;
   menu_handle_t *menu_data                        = menu_driver_get_ptr();

   /* Check whether a message box is currently
    * being shown
    * > Note: This ignores input bind dialogs,
    *   since input binding overrides normal input
    *   and must be handled separately... */
   if (menu_data)
      messagebox_active = BIT64_GET(menu_data->state, MENU_STATE_RENDER_MESSAGEBOX) &&
            !string_is_empty(menu_data->menu_state_msg);

   /* If onscreen keyboard is shown and we currently have
    * active mouse input, highlight key under mouse cursor */
   if (osk_active &&
       (menu_input->pointer.type == MENU_POINTER_MOUSE) &&
       pointer_hw_state->active)
   {
      menu_ctx_pointer_t point;

      point.x       = pointer_hw_state->x;
      point.y       = pointer_hw_state->y;
      point.ptr     = 0;
      point.cbs     = NULL;
      point.entry   = NULL;
      point.action  = 0;
      point.gesture = MENU_INPUT_GESTURE_NONE;
      point.retcode = 0;

      menu_driver_ctl(RARCH_MENU_CTL_OSK_PTR_AT_POS, &point);
      if (point.retcode > -1)
         input_event_set_osk_ptr(point.retcode);
   }

   /* Select + X/Y position */
   if (!menu_input->select_inhibit)
   {
      if (pointer_hw_state->select_pressed)
      {
         int16_t x           = pointer_hw_state->x;
         int16_t y           = pointer_hw_state->y;
         static float accel0 = 0.0f;
         static float accel1 = 0.0f;

         /* Transition from select unpressed to select pressed */
         if (!last_select_pressed)
         {
            menu_ctx_pointer_t point;

            /* Initialise variables */
            start_time                = current_time;
            start_x                   = x;
            start_y                   = y;
            last_x                    = x;
            last_y                    = y;
            dx_start_right_max        = 0;
            dx_start_left_max         = 0;
            dy_start_up_max           = 0;
            dy_start_down_max         = 0;
            accel0                    = 0.0f;
            accel1                    = 0.0f;
            last_press_direction_time = 0;

            /* If we are not currently showing the onscreen
             * keyboard or a message box, trigger a 'pointer
             * down' event */
            if (!osk_active && !messagebox_active)
            {
               point.x       = x;
               point.y       = y;
               /* Note: menu_input->ptr is meaningless here when
                * using a touchscreen... */
               point.ptr     = menu_input->ptr;
               point.cbs     = cbs;
               point.entry   = entry;
               point.action  = action;
               point.gesture = MENU_INPUT_GESTURE_NONE;

               menu_driver_ctl(RARCH_MENU_CTL_POINTER_DOWN, &point);
               ret = point.retcode;
            }
         }
         else
         {
            /* Pointer is being held down
             * (i.e. for more than one frame) */
            float dpi = menu_input_get_dpi();

            /* > Update deltas + acceleration & detect press direction
             *   Note: We only do this if the pointer has moved above
             *   a certain threshold - this requires dpi info */
            if (dpi > 0.0f)
            {
               uint16_t dpi_threshold_drag =
                     (uint16_t)((dpi * MENU_INPUT_DPI_THRESHOLD_DRAG) + 0.5f);

               int16_t dx_start            = x - start_x;
               int16_t dy_start            = y - start_y;
               uint16_t dx_start_abs       = dx_start < 0 ? dx_start * -1 : dx_start;
               uint16_t dy_start_abs       = dy_start < 0 ? dy_start * -1 : dy_start;

               if ((dx_start_abs > dpi_threshold_drag) ||
                   (dy_start_abs > dpi_threshold_drag))
               {
                  uint16_t dpi_threshold_press_direction_min     =
                        (uint16_t)((dpi * MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_MIN) + 0.5f);
                  uint16_t dpi_threshold_press_direction_max     =
                        (uint16_t)((dpi * MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_MAX) + 0.5f);
                  uint16_t dpi_threshold_press_direction_tangent =
                        (uint16_t)((dpi * MENU_INPUT_DPI_THRESHOLD_PRESS_DIRECTION_TANGENT) + 0.5f);

                  enum menu_input_pointer_press_direction
                        press_direction                          = MENU_INPUT_PRESS_DIRECTION_NONE;
                  float press_direction_amplitude                = 0.0f;
                  retro_time_t press_direction_delay             = MENU_INPUT_PRESS_DIRECTION_DELAY_MAX;

                  /* Pointer has moved a sufficient distance to
                   * trigger a 'dragged' state */
                  menu_input->pointer.dragged = true;

                  /* Here we diverge:
                   * > If onscreen keyboard or a message box is
                   *   active, pointer deltas, acceleration and
                   *   press direction must be inhibited
                   * > If not, input is processed normally */
                  if (osk_active || messagebox_active)
                  {
                     /* Inhibit normal pointer input */
                     menu_input->pointer.dx              = 0;
                     menu_input->pointer.dy              = 0;
                     menu_input->pointer.y_accel         = 0.0f;
                     menu_input->pointer.press_direction = MENU_INPUT_PRESS_DIRECTION_NONE;
                     accel0                              = 0.0f;
                     accel1                              = 0.0f;
                     attenuate_y_accel                   = false;
                  }
                  else
                  {
                     /* Assign current deltas */
                     menu_input->pointer.dx = x - last_x;
                     menu_input->pointer.dy = y - last_y;

                     /* Update maximum start->current deltas */
                     if (dx_start > 0)
                        dx_start_right_max = (dx_start_abs > dx_start_right_max) ?
                              dx_start_abs : dx_start_right_max;
                     else
                        dx_start_left_max = (dx_start_abs > dx_start_left_max) ?
                              dx_start_abs : dx_start_left_max;

                     if (dy_start > 0)
                        dy_start_down_max = (dy_start_abs > dy_start_down_max) ?
                              dy_start_abs : dy_start_down_max;
                     else
                        dy_start_up_max = (dy_start_abs > dy_start_up_max) ?
                              dy_start_abs : dy_start_up_max;

                     /* Magic numbers... */
                     menu_input->pointer.y_accel = (accel0 + accel1 + (float)menu_input->pointer.dy) / 3.0f;
                     accel0                      = accel1;
                     accel1                      = menu_input->pointer.y_accel;

                     /* Acceleration decays over time - but if the value
                      * has been set on this frame, attenuation should
                      * be skipped */
                     attenuate_y_accel = false;

                     /* Check if pointer is being held in a particular
                      * direction */
                     menu_input->pointer.press_direction = MENU_INPUT_PRESS_DIRECTION_NONE;

                     /* > Press directions are actually triggered as a pulse train,
                      *   since a continuous direction prevents fine control in the
                      *   context of menu actions (i.e. it would be the same
                      *   as always holding down a cursor key all the time - too fast
                      *   to control). We therefore apply a low pass filter, with
                      *   a variable frequency based upon the distance the user has
                      *   dragged the pointer */

                     /* > Horizontal */
                     if ((dx_start_abs >= dpi_threshold_press_direction_min) &&
                         (dy_start_abs <  dpi_threshold_press_direction_tangent))
                     {
                        press_direction = (dx_start > 0) ?
                              MENU_INPUT_PRESS_DIRECTION_RIGHT : MENU_INPUT_PRESS_DIRECTION_LEFT;

                        /* Get effective amplitude of press direction offset */
                        press_direction_amplitude =
                              (float)(dx_start_abs - dpi_threshold_press_direction_min) /
                              (float)(dpi_threshold_press_direction_max - dpi_threshold_press_direction_min);
                     }
                     /* > Vertical */
                     else if ((dy_start_abs >= dpi_threshold_press_direction_min) &&
                              (dx_start_abs <  dpi_threshold_press_direction_tangent))
                     {
                        press_direction = (dy_start > 0) ?
                              MENU_INPUT_PRESS_DIRECTION_DOWN : MENU_INPUT_PRESS_DIRECTION_UP;

                        /* Get effective amplitude of press direction offset */
                        press_direction_amplitude =
                              (float)(dy_start_abs - dpi_threshold_press_direction_min) /
                              (float)(dpi_threshold_press_direction_max - dpi_threshold_press_direction_min);
                     }

                     if (press_direction != MENU_INPUT_PRESS_DIRECTION_NONE)
                     {
                        /* > Update low pass filter frequency */
                        if (press_direction_amplitude > 1.0f)
                           press_direction_delay = MENU_INPUT_PRESS_DIRECTION_DELAY_MIN;
                        else
                           press_direction_delay = MENU_INPUT_PRESS_DIRECTION_DELAY_MIN +
                                 ((MENU_INPUT_PRESS_DIRECTION_DELAY_MAX - MENU_INPUT_PRESS_DIRECTION_DELAY_MIN)*
                                  (1.0f - press_direction_amplitude));

                        /* > Apply low pass filter */
                        if (current_time - last_press_direction_time > press_direction_delay)
                        {
                           menu_input->pointer.press_direction = press_direction;
                           last_press_direction_time = current_time;
                        }
                     }
                  }
               }
               else
               {
                  /* Pointer is stationary */
                  menu_input->pointer.dx              = 0;
                  menu_input->pointer.dy              = 0;
                  menu_input->pointer.press_direction = MENU_INPUT_PRESS_DIRECTION_NONE;

                  /* Standard behaviour (on Android, at least) is to stop
                   * scrolling when the user touches the screen. We should
                   * therefore 'reset' y acceleration whenever the pointer
                   * is stationary - with two caveats:
                   * - We only disable scrolling if the pointer *remains*
                   *   stationary. If the pointer is held down then
                   *   subsequently moves, normal scrolling should resume
                   * - Halting the scroll immediately produces a very
                   *   unpleasant 'jerky' user experience. To avoid this,
                   *   we add a small delay between detecting a pointer
                   *   down event and forcing y acceleration to zero
                   * NOTE: Of course, we must also 'reset' y acceleration
                   * whenever the onscreen keyboard or a message box is
                   * shown */
                  if ((!menu_input->pointer.dragged &&
                        (menu_input->pointer.press_duration > MENU_INPUT_Y_ACCEL_RESET_DELAY)) ||
                      (osk_active || messagebox_active))
                  {
                     menu_input->pointer.y_accel = 0.0f;
                     accel0                      = 0.0f;
                     accel1                      = 0.0f;
                     attenuate_y_accel           = false;
                  }
               }
            }
            else
            {
               /* No dpi info - just fallback to zero... */
               menu_input->pointer.dx              = 0;
               menu_input->pointer.dy              = 0;
               menu_input->pointer.y_accel         = 0.0f;
               menu_input->pointer.press_direction = MENU_INPUT_PRESS_DIRECTION_NONE;
               accel0                              = 0.0f;
               accel1                              = 0.0f;
               attenuate_y_accel                   = false;
            }

            /* > Update remaining variables */
            menu_input->pointer.press_duration = current_time - start_time;
            last_x                             = x;
            last_y                             = y;
         }
      }
      else if (last_select_pressed)
      {
         /* Transition from select pressed to select unpressed */
         int16_t x;
         int16_t y;
         menu_ctx_pointer_t point;

         if (menu_input->pointer.dragged)
         {
            /* Pointer has moved.
             * When using a touchscreen, releasing a press
             * resets the x/y position - so cannot use
             * current hardware x/y values. Instead, use
             * previous position from last time that a
             * press was active */
            x = last_x;
            y = last_y;
         }
         else
         {
            /* Pointer is considered stationary,
             * so use start position */
            x = start_x;
            y = start_y;
         }

         point.x       = x;
         point.y       = y;
         point.ptr     = menu_input->ptr;
         point.cbs     = cbs;
         point.entry   = entry;
         point.action  = action;
         point.gesture = MENU_INPUT_GESTURE_NONE;

         /* On screen keyboard overrides normal menu input... */
         if (osk_active)
         {
            /* If pointer has been 'dragged', then it counts as
             * a miss. Only register 'release' event if pointer
             * has remained stationary */
            if (!menu_input->pointer.dragged)
            {
               menu_driver_ctl(RARCH_MENU_CTL_OSK_PTR_AT_POS, &point);
               if (point.retcode > -1)
               {
                  menu_handle_t *menu_data      = menu_driver_get_ptr();
                  bool menu_has_fb              =
                     (menu_data && 
                      menu_data->driver_ctx && 
                      menu_data->driver_ctx->set_texture);

                  input_event_set_osk_ptr(point.retcode);
                  input_event_osk_append(point.retcode,
                        menu_has_fb);
               }
            }
         }
         /* Message boxes override normal menu input...
          * > If a message box is shown, any kind of pointer
          *   gesture should close it */
         else if (messagebox_active)
            menu_input_pointer_close_messagebox();
         /* Normal menu input */
         else
         {
            /* Detect gesture type */
            if (!menu_input->pointer.dragged)
            {
               /* Pointer hasn't moved - check press duration */
               if (menu_input->pointer.press_duration < MENU_INPUT_PRESS_TIME_SHORT)
                  point.gesture = MENU_INPUT_GESTURE_TAP;
               else if (menu_input->pointer.press_duration < MENU_INPUT_PRESS_TIME_LONG)
                  point.gesture = MENU_INPUT_GESTURE_SHORT_PRESS;
               else
                  point.gesture = MENU_INPUT_GESTURE_LONG_PRESS;
            }
            else
            {
               /* Pointer has moved - check if this is a swipe */
               float dpi = menu_input_get_dpi();

               if ((dpi > 0.0f) && (menu_input->pointer.press_duration < MENU_INPUT_SWIPE_TIMEOUT))
               {
                  uint16_t dpi_threshold_swipe         =
                        (uint16_t)((dpi * MENU_INPUT_DPI_THRESHOLD_SWIPE) + 0.5f);
                  uint16_t dpi_threshold_swipe_tangent =
                        (uint16_t)((dpi * MENU_INPUT_DPI_THRESHOLD_SWIPE_TANGENT) + 0.5f);

                  int16_t dx_start                     = x - start_x;
                  int16_t dy_start                     = y - start_y;
                  uint16_t dx_start_right_final        = 0;
                  uint16_t dx_start_left_final         = 0;
                  uint16_t dy_start_up_final           = 0;
                  uint16_t dy_start_down_final         = 0;

                  /* Get final deltas */
                  if (dx_start > 0)
                     dx_start_right_final = (uint16_t)dx_start;
                  else
                     dx_start_left_final  = (uint16_t)(dx_start * -1);

                  if (dy_start > 0)
                     dy_start_down_final  = (uint16_t)dy_start;
                  else
                     dy_start_up_final    = (uint16_t)(dy_start * -1);

                  /* Swipe right */
                  if ((dx_start_right_final > dpi_threshold_swipe) &&
                      (dx_start_left_max    < dpi_threshold_swipe_tangent) &&
                      (dy_start_up_max      < dpi_threshold_swipe_tangent) &&
                      (dy_start_down_max    < dpi_threshold_swipe_tangent))
                     point.gesture = MENU_INPUT_GESTURE_SWIPE_RIGHT;
                  /* Swipe left */
                  else if ((dx_start_right_max  < dpi_threshold_swipe_tangent) &&
                           (dx_start_left_final > dpi_threshold_swipe) &&
                           (dy_start_up_max     < dpi_threshold_swipe_tangent) &&
                           (dy_start_down_max   < dpi_threshold_swipe_tangent))
                     point.gesture = MENU_INPUT_GESTURE_SWIPE_LEFT;
                  /* Swipe up */
                  else if ((dx_start_right_max < dpi_threshold_swipe_tangent) &&
                           (dx_start_left_max  < dpi_threshold_swipe_tangent) &&
                           (dy_start_up_final  > dpi_threshold_swipe) &&
                           (dy_start_down_max  < dpi_threshold_swipe_tangent))
                     point.gesture = MENU_INPUT_GESTURE_SWIPE_UP;
                  /* Swipe down */
                  else if ((dx_start_right_max  < dpi_threshold_swipe_tangent) &&
                           (dx_start_left_max   < dpi_threshold_swipe_tangent) &&
                           (dy_start_up_max     < dpi_threshold_swipe_tangent) &&
                           (dy_start_down_final > dpi_threshold_swipe))
                     point.gesture = MENU_INPUT_GESTURE_SWIPE_DOWN;
               }
            }

            /* Trigger a 'pointer up' event */
            menu_driver_ctl(RARCH_MENU_CTL_POINTER_UP, &point);
            ret = point.retcode;
         }

         /* Reset variables */
         start_x                             = 0;
         start_y                             = 0;
         last_x                              = 0;
         last_y                              = 0;
         dx_start_right_max                  = 0;
         dx_start_left_max                   = 0;
         dy_start_up_max                     = 0;
         dy_start_down_max                   = 0;
         last_press_direction_time           = 0;
         menu_input->pointer.press_duration  = 0;
         menu_input->pointer.press_direction = MENU_INPUT_PRESS_DIRECTION_NONE;
         menu_input->pointer.dx              = 0;
         menu_input->pointer.dy              = 0;
         menu_input->pointer.dragged         = false;
      }
   }
   last_select_pressed = pointer_hw_state->select_pressed;

   /* Adjust acceleration
    * > If acceleration has not been set on this frame,
    *   apply normal attenuation */
   if (attenuate_y_accel)
      menu_input->pointer.y_accel *= MENU_INPUT_Y_ACCEL_DECAY_FACTOR;

   /* If select has been released, disable any existing
    * select inhibit */
   if (!pointer_hw_state->select_pressed)
      menu_input->select_inhibit = false;

   /* Cancel */
   if (!menu_input->cancel_inhibit &&
       pointer_hw_state->cancel_pressed &&
       !last_cancel_pressed)
   {
      /* If currently showing a message box, close it */
      if (messagebox_active)
         menu_input_pointer_close_messagebox();
      /* ...otherwise, invoke standard MENU_ACTION_CANCEL
       * action */
      else
      {
         size_t selection = menu_navigation_get_selection();
         ret = menu_entry_action(entry, selection, MENU_ACTION_CANCEL);
      }
   }
   last_cancel_pressed = pointer_hw_state->cancel_pressed;

   /* If cancel has been released, disable any existing
    * cancel inhibit */
   if (!pointer_hw_state->cancel_pressed)
      menu_input->cancel_inhibit = false;

   /* Up/Down
    * > Note 1: These always correspond to a mouse wheel, which
    *   handles differently from other inputs - i.e. we don't
    *   want a 'last pressed' check
    * > Note 2: If a message box is currently shown, must
    *   inhibit input */

   /* > Up */
   if (!messagebox_active && pointer_hw_state->up_pressed)
   {
      size_t selection = menu_navigation_get_selection();
      ret = menu_entry_action(entry, selection, MENU_ACTION_UP);
   }

   /* > Down */
   if (!messagebox_active && pointer_hw_state->down_pressed)
   {
      size_t selection = menu_navigation_get_selection();
      ret = menu_entry_action(entry, selection, MENU_ACTION_DOWN);
   }

   /* Left/Right
    * > Note 1: These also always correspond to a mouse wheel...
    *   In this case, it's a mouse wheel *tilt* operation, which
    *   is incredibly annoying because holding a tilt direction
    *   rapidly toggles the input state. The repeat speed is so
    *   high that any sort of useable control is impossible - so
    *   we have to apply a 'low pass' filter by ignoring inputs
    *   that occur below a certain frequency...
    * > Note 2: If a message box is currently shown, must
    *   inhibit input */

   /* > Left */
   if (!messagebox_active &&
       pointer_hw_state->left_pressed &&
       !last_left_pressed)
   {
      if (current_time - last_left_action_time > MENU_INPUT_HORIZ_WHEEL_DELAY)
      {
         size_t selection      = menu_navigation_get_selection();
         last_left_action_time = current_time;
         ret = menu_entry_action(entry, selection, MENU_ACTION_LEFT);
      }
   }
   last_left_pressed = pointer_hw_state->left_pressed;

   /* > Right */
   if (!messagebox_active &&
       pointer_hw_state->right_pressed &&
       !last_right_pressed)
   {
      if (current_time - last_right_action_time > MENU_INPUT_HORIZ_WHEEL_DELAY)
      {
         size_t selection       = menu_navigation_get_selection();
         last_right_action_time = current_time;
         ret = menu_entry_action(entry, selection, MENU_ACTION_RIGHT);
      }
   }
   last_right_pressed = pointer_hw_state->right_pressed;

   menu_input_set_pointer_visibility(current_time);

   return ret;
}

void menu_input_post_iterate(int *ret, unsigned action)
{
   struct rarch_state *p_rarch   = &rarch_st;
   menu_input_t     *menu_input  = &p_rarch->menu_input_state;
   retro_time_t     current_time = cpu_features_get_time_usec();

   /* If pointer devices are disabled, just ensure mouse
    * cursor is hidden */
   if (menu_input->pointer.type == MENU_POINTER_DISABLED)
   {
      /* Note: We have to call menu_input_set_pointer_visibility()
       * here, otherwise the cursor state gets muddled up when
       * toggling mouse/touchscreen support...
       * It's a very light function, however, so there should
       * be no performance impact */
      menu_input_set_pointer_visibility(current_time);
      *ret = 0;
   }
   else
   {
      menu_entry_t entry;
      file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
      size_t selection           = menu_navigation_get_selection();
      menu_file_list_cbs_t *cbs  = selection_buf ?
         (menu_file_list_cbs_t*)selection_buf->list[selection].actiondata
         : NULL;

      menu_entry_init(&entry);
      /* Note: If menu_input_pointer_post_iterate() is
       * modified, will have to verify that these
       * parameters remain unused... */
      entry.rich_label_enabled   = false;
      entry.value_enabled        = false;
      entry.sublabel_enabled     = false;
      menu_entry_get(&entry, 0, selection, NULL, false);

      *ret = menu_input_pointer_post_iterate(current_time, cbs, &entry, action);
   }
}

/**
 * input_menu_keys_pressed:
 *
 * Grab an input sample for this frame. We exclude
 * keyboard input here.
 *
 * Returns: Input sample containing a mask of all pressed keys.
 */
static void input_menu_keys_pressed(input_bits_t *p_new_state,
      const struct retro_keybind **binds,
      rarch_joypad_info_t *joypad_info)
{
   unsigned i, port;
   struct rarch_state                  *p_rarch = &rarch_st;
   settings_t     *settings                     = p_rarch->configuration_settings;
   bool input_all_users_control_menu            = settings->bools.input_all_users_control_menu;
   uint8_t max_users                            = (uint8_t)p_rarch->input_driver_max_users;
   uint8_t port_max                             = input_all_users_control_menu
      ? max_users : 1;

   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *auto_binds          = input_autoconf_binds[i];
      struct retro_keybind *general_binds       = input_config_binds[i];
      binds[i]                                  = input_config_binds[i];

      input_push_analog_dpad(auto_binds, ANALOG_DPAD_LSTICK);
      input_push_analog_dpad(general_binds, ANALOG_DPAD_LSTICK);
   }

   for (port = 0; port < port_max; port++)
   {
      const struct retro_keybind *binds_norm = &input_config_binds[port][RARCH_ENABLE_HOTKEY];
      const struct retro_keybind *binds_auto = &input_autoconf_binds[port][RARCH_ENABLE_HOTKEY];

      joypad_info->joy_idx                    = settings->uints.input_joypad_map[port];
      joypad_info->auto_binds                 = input_autoconf_binds[joypad_info->joy_idx];
      joypad_info->axis_threshold             = p_rarch->input_driver_axis_threshold;

      if (check_input_driver_block_hotkey(binds_norm, binds_auto))
      {
         const struct retro_keybind *htkey = &input_config_binds[port][RARCH_ENABLE_HOTKEY];

         if (htkey->valid
               && p_rarch->current_input->input_state(p_rarch->current_input_data, joypad_info,
                  &binds[0], port, RETRO_DEVICE_JOYPAD, 0, RARCH_ENABLE_HOTKEY))
         {
            p_rarch->input_driver_block_libretro_input = true;
            break;
         }
         else
         {
            p_rarch->input_driver_block_hotkey         = true;
            break;
         }
      }
   }

   {
      int16_t ret[MAX_USERS];
      /* Check the libretro input first */
      for (port = 0; port < port_max; port++)
      {
         joypad_info->joy_idx              = settings->uints.input_joypad_map[port];
         joypad_info->auto_binds           = input_autoconf_binds[joypad_info->joy_idx];
         joypad_info->axis_threshold       = p_rarch->input_driver_axis_threshold;
         ret[port]                         = p_rarch->current_input->input_state(
               p_rarch->current_input_data,
               joypad_info, &binds[0], port, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_MASK);
      }

      for (i = 0; i < RARCH_FIRST_META_KEY; i++)
      {
         bool bit_pressed    = false;

         if (!p_rarch->input_driver_block_libretro_input)
         {
            for (port = 0; port < port_max; port++)
            {
               if (binds[port][i].valid && ret[port] & (UINT64_C(1) << i))
               {
                  bit_pressed = true;
                  break;
               }
            }
         }

         if (bit_pressed || input_keys_pressed_other_sources(i, p_new_state))
         {
            BIT256_SET_PTR(p_new_state, i);
         }
      }

      /* Check the hotkeys */
      for (i = RARCH_FIRST_META_KEY; i < RARCH_BIND_LIST_END; i++)
      {
         bool bit_pressed    = false;

         if (!p_rarch->input_driver_block_hotkey)
         {
            for (port = 0; port < port_max; port++)
            {
               const struct retro_keybind *mtkey = &input_config_binds[port][i];
               if (!mtkey->valid)
                  continue;
               joypad_info->joy_idx              = settings->uints.input_joypad_map[port];
               joypad_info->auto_binds           = input_autoconf_binds[joypad_info->joy_idx];
               joypad_info->axis_threshold       = p_rarch->input_driver_axis_threshold;

               if (p_rarch->current_input->input_state(
                        p_rarch->current_input_data, joypad_info,
                        &binds[0], port, RETRO_DEVICE_JOYPAD, 0, i))
               {
                  bit_pressed = true;
                  break;
               }
            }
         }

         if (bit_pressed || BIT64_GET(lifecycle_state, i) || input_keys_pressed_other_sources(i, p_new_state))
         {
            BIT256_SET_PTR(p_new_state, i);
         }
      }
   }

   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *auto_binds    = input_autoconf_binds[i];
      struct retro_keybind *general_binds = input_config_binds[i];
      input_pop_analog_dpad(auto_binds);
      input_pop_analog_dpad(general_binds);
   }
}
#endif

/**
 * input_keys_pressed:
 *
 * Grab an input sample for this frame.
 *
 * Returns: Input sample containing a mask of all pressed keys.
 */
static void input_keys_pressed(input_bits_t *p_new_state,
      rarch_joypad_info_t *joypad_info)
{
   unsigned i, port                       = 0;
   struct rarch_state            *p_rarch = &rarch_st;
   settings_t              *settings      = p_rarch->configuration_settings;
   const struct retro_keybind *binds      = input_config_binds[0];
   const struct retro_keybind *binds_norm = &input_config_binds[port][RARCH_ENABLE_HOTKEY];
   const struct retro_keybind *binds_auto = &input_autoconf_binds[port][RARCH_ENABLE_HOTKEY];

   joypad_info->joy_idx                   = settings->uints.input_joypad_map[port];
   joypad_info->auto_binds                = input_autoconf_binds[joypad_info->joy_idx];
   joypad_info->axis_threshold            = p_rarch->input_driver_axis_threshold;

   if (check_input_driver_block_hotkey(binds_norm, binds_auto))
   {
      const struct retro_keybind *enable_hotkey    =
         &input_config_binds[port][RARCH_ENABLE_HOTKEY];

      if (     enable_hotkey && enable_hotkey->valid
            && p_rarch->current_input->input_state(
               p_rarch->current_input_data, joypad_info,
               &binds, port,
               RETRO_DEVICE_JOYPAD, 0, RARCH_ENABLE_HOTKEY))
         p_rarch->input_driver_block_libretro_input = true;
      else
         p_rarch->input_driver_block_hotkey         = true;
   }

   if (binds[RARCH_GAME_FOCUS_TOGGLE].valid)
   {
      const struct retro_keybind *focus_binds_auto =
         &input_autoconf_binds[port][RARCH_GAME_FOCUS_TOGGLE];
      const struct retro_keybind *focus_normal     =
         &binds[RARCH_GAME_FOCUS_TOGGLE];

      /* Allows rarch_focus_toggle hotkey to still work
       * even though every hotkey is blocked */
      if (check_input_driver_block_hotkey(
               focus_normal, focus_binds_auto))
      {
         if (p_rarch->current_input->input_state(
                  p_rarch->current_input_data,
                  joypad_info,
                  &binds, port,
                  RETRO_DEVICE_JOYPAD, 0, RARCH_GAME_FOCUS_TOGGLE))
            p_rarch->input_driver_block_hotkey = false;
      }
   }

   /* Check the libretro input first */
   {
      int16_t ret = p_rarch->current_input->input_state(
            p_rarch->current_input_data,
            joypad_info, &binds, 0, RETRO_DEVICE_JOYPAD, 0,
            RETRO_DEVICE_ID_JOYPAD_MASK);
      for (i = 0; i < RARCH_FIRST_META_KEY; i++)
      {
         bool bit_pressed = !p_rarch->input_driver_block_libretro_input
            && binds[i].valid && (ret & (UINT64_C(1) <<  i));
         if (bit_pressed || input_keys_pressed_other_sources(i, p_new_state))
         {
            BIT256_SET_PTR(p_new_state, i);
         }
      }
   }

   /* Check the hotkeys */
   for (i = RARCH_FIRST_META_KEY; i < RARCH_BIND_LIST_END; i++)
   {
      bool bit_pressed = !p_rarch->input_driver_block_hotkey && binds[i].valid
         && p_rarch->current_input->input_state(p_rarch->current_input_data, joypad_info,
               &binds, 0, RETRO_DEVICE_JOYPAD, 0, i);
      if (     bit_pressed
            || BIT64_GET(lifecycle_state, i)
            || input_keys_pressed_other_sources(i, p_new_state))
      {
         BIT256_SET_PTR(p_new_state, i);
      }
   }
}

void *input_driver_get_data(void)
{
   struct rarch_state  *p_rarch   = &rarch_st;
   return p_rarch->current_input_data;
}

static bool input_driver_init(void)
{
   struct rarch_state  *p_rarch   = &rarch_st;
   if (p_rarch->current_input)
   {
      settings_t *settings        = p_rarch->configuration_settings;
      p_rarch->current_input_data = p_rarch->current_input->init(
            settings->arrays.input_joypad_driver);
   }

   if (!p_rarch->current_input_data)
      return false;
   return true;
}

static bool input_driver_find_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   struct rarch_state  *p_rarch   = &rarch_st;
   settings_t           *settings = p_rarch->configuration_settings;

   drv.label                      = "input_driver";
   drv.s                          = settings->arrays.input_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i                              = (int)drv.len;

   if (i >= 0)
      p_rarch->current_input = (input_driver_t*)input_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any input driver named \"%s\"\n",
            settings->arrays.input_driver);
      RARCH_LOG_OUTPUT("Available input drivers are:\n");
      for (d = 0; input_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", input_drivers[d]->ident);
      RARCH_WARN("Going to default to first input driver...\n");

      p_rarch->current_input = (input_driver_t*)input_driver_find_handle(0);

      if (!p_rarch->current_input)
      {
         retroarch_fail(1, "find_input_driver()");
         return false;
      }
   }

   return true;
}

void input_driver_set_flushing_input(void)
{
   struct rarch_state          *p_rarch = &rarch_st;
   /* Inhibits input for 2 frames
    * > Required, since input is ignored for 1 frame
    *   after certain events - e.g. closing the OSK */
   p_rarch->input_driver_flushing_input = 2;
}

bool input_driver_is_libretro_input_blocked(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->input_driver_block_libretro_input;
}

void input_driver_set_nonblock_state(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->input_driver_nonblock_state = true;
}

void input_driver_unset_nonblock_state(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->input_driver_nonblock_state = false;
}

static bool input_driver_init_command(void)
{
#ifdef HAVE_COMMAND
   struct rarch_state   *p_rarch = &rarch_st;
   settings_t *settings          = p_rarch->configuration_settings;
   bool input_stdin_cmd_enable   = settings->bools.stdin_cmd_enable;
   bool input_network_cmd_enable = settings->bools.network_cmd_enable;
   unsigned network_cmd_port     = settings->uints.network_cmd_port;
   bool grab_stdin               = p_rarch->current_input->grab_stdin && 
      p_rarch->current_input->grab_stdin(p_rarch->current_input_data);

   if (!input_stdin_cmd_enable && !input_network_cmd_enable)
      return false;

   if (input_stdin_cmd_enable && grab_stdin)
   {
      RARCH_WARN("stdin command interface is desired, but input driver has already claimed stdin.\n"
            "Cannot use this command interface.\n");
   }

   p_rarch->input_driver_command = (command_t*)
      calloc(1, sizeof(*p_rarch->input_driver_command));

   if (p_rarch->input_driver_command)
      if (command_network_new(
               p_rarch->input_driver_command,
               input_stdin_cmd_enable && !grab_stdin,
               input_network_cmd_enable,
               network_cmd_port))
         return true;

   RARCH_ERR("Failed to initialize command interface.\n");
#endif
   return false;
}

static void input_driver_deinit_command(void)
{
#ifdef HAVE_COMMAND
   struct rarch_state   *p_rarch = &rarch_st;
   if (p_rarch->input_driver_command)
      command_free(p_rarch->input_driver_command);
   p_rarch->input_driver_command = NULL;
#endif
}

static void input_driver_deinit_remote(void)
{
#ifdef HAVE_NETWORKGAMEPAD
   struct rarch_state       *p_rarch = &rarch_st;
   if (p_rarch->input_driver_remote)
      input_remote_free(p_rarch->input_driver_remote,
            p_rarch->input_driver_max_users);
   p_rarch->input_driver_remote = NULL;
#endif
}

static void input_driver_deinit_mapper(void)
{
   struct rarch_state   *p_rarch = &rarch_st;
   if (p_rarch->input_driver_mapper)
      free(p_rarch->input_driver_mapper);
   p_rarch->input_driver_mapper = NULL;
}

static bool input_driver_init_remote(void)
{
#ifdef HAVE_NETWORKGAMEPAD
   struct rarch_state       *p_rarch = &rarch_st;
   settings_t *settings              = p_rarch->configuration_settings;
   bool network_remote_enable        = settings->bools.network_remote_enable;
   unsigned network_remote_base_port = settings->uints.network_remote_base_port;

   if (!network_remote_enable)
      return false;

   p_rarch->input_driver_remote      = input_remote_new(network_remote_base_port,
         p_rarch->input_driver_max_users);

   if (p_rarch->input_driver_remote)
      return true;

   RARCH_ERR("Failed to initialize remote gamepad interface.\n");
#endif
   return false;
}

static bool input_driver_init_mapper(void)
{
   input_mapper_t *handle        = NULL;
   struct rarch_state   *p_rarch = &rarch_st;
   settings_t *settings          = p_rarch->configuration_settings;
   bool input_remap_binds_enable = settings->bools.input_remap_binds_enable;

   if (!input_remap_binds_enable)
      return false;

   handle = (input_mapper_t*)calloc(1, sizeof(*p_rarch->input_driver_mapper));

   if (!handle)
   {
      RARCH_ERR("Failed to initialize input mapper.\n");
      return false;
   }

   p_rarch->input_driver_mapper = handle;

   return true;
}

float *input_driver_get_float(enum input_action action)
{
   struct rarch_state           *p_rarch = &rarch_st;

   switch (action)
   {
      case INPUT_ACTION_AXIS_THRESHOLD:
         return &p_rarch->input_driver_axis_threshold;
      default:
      case INPUT_ACTION_NONE:
         break;
   }

   return NULL;
}

unsigned *input_driver_get_uint(enum input_action action)
{
   struct rarch_state           *p_rarch = &rarch_st;

   switch (action)
   {
      case INPUT_ACTION_MAX_USERS:
         return &p_rarch->input_driver_max_users;
      default:
      case INPUT_ACTION_NONE:
         break;
   }

   return NULL;
}

/**
 * joypad_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to joypad driver at index. Can be NULL
 * if nothing found.
 **/
static const void *joypad_driver_find_handle(int idx)
{
   const void *drv = joypad_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * config_get_joypad_driver_options:
 *
 * Get an enumerated list of all joypad driver names, separated by '|'.
 *
 * Returns: string listing of all joypad driver names, separated by '|'.
 **/
const char* config_get_joypad_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_JOYPAD_DRIVERS, NULL);
}

/**
 * input_joypad_init_first:
 *
 * Finds first suitable joypad driver and initializes.
 *
 * Returns: joypad driver if found, otherwise NULL.
 **/
static const input_device_driver_t *input_joypad_init_first(void *data)
{
   unsigned i;

   for (i = 0; joypad_drivers[i]; i++)
   {
      if (joypad_drivers[i]->init(data))
      {
         RARCH_LOG("[Joypad]: Found joypad driver: \"%s\".\n",
               joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return NULL;
}

/**
 * input_joypad_init_driver:
 * @ident                           : identifier of driver to initialize.
 *
 * Initialize a joypad driver of name @ident.
 *
 * If ident points to NULL or a zero-length string,
 * equivalent to calling input_joypad_init_first().
 *
 * Returns: joypad driver if found, otherwise NULL.
 **/
const input_device_driver_t *input_joypad_init_driver(
      const char *ident, void *data)
{
   unsigned i;
   if (!ident || !*ident)
      return input_joypad_init_first(data);

   for (i = 0; joypad_drivers[i]; i++)
   {
      if (string_is_equal(ident, joypad_drivers[i]->ident)
            && joypad_drivers[i]->init
            && joypad_drivers[i]->init(data))
      {
         RARCH_LOG("[Joypad]: Found joypad driver: \"%s\".\n",
               joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return input_joypad_init_first(data);
}

/**
 * input_joypad_set_rumble:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @effect                  : Rumble effect to set.
 * @strength                : Strength of rumble effect.
 *
 * Sets rumble effect @effect with strength @strength.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_joypad_set_rumble(const input_device_driver_t *drv,
      unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t  *settings       = p_rarch->configuration_settings;
   unsigned  joy_idx           = settings->uints.input_joypad_map[port];

   if (!drv || !drv->set_rumble || joy_idx >= MAX_USERS)
      return false;

   return drv->set_rumble(joy_idx, effect, strength);
}

/**
 * input_joypad_analog:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @idx                     : Analog key index.
 *                            E.g.:
 *                            - RETRO_DEVICE_INDEX_ANALOG_LEFT
 *                            - RETRO_DEVICE_INDEX_ANALOG_RIGHT
 * @ident                   : Analog key identifier.
 *                            E.g.:
 *                            - RETRO_DEVICE_ID_ANALOG_X
 *                            - RETRO_DEVICE_ID_ANALOG_Y
 * @binds                   : Binds of user.
 *
 * Gets analog value of analog key identifiers @idx and @ident
 * from user with number @port with provided keybinds (@binds).
 *
 * Returns: analog value on success, otherwise 0.
 **/
int16_t input_joypad_analog(const input_device_driver_t *drv,
      rarch_joypad_info_t *joypad_info,
      unsigned port, unsigned idx, unsigned ident,
      const struct retro_keybind *binds)
{
   int16_t res = 0;
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   float input_analog_deadzone = settings->floats.input_analog_deadzone;

   if (idx == RETRO_DEVICE_INDEX_ANALOG_BUTTON)
   {
      /* A RETRO_DEVICE_JOYPAD button?
       * Otherwise, not a suitable button */
      if (ident < RARCH_FIRST_CUSTOM_BIND)
      {
         uint32_t axis = 0;
         const struct retro_keybind *bind = &binds[ ident ];

         if (!bind->valid)
            return 0;

         axis = (bind->joyaxis == AXIS_NONE)
            ? joypad_info->auto_binds[ident].joyaxis
            : bind->joyaxis;

         /* Analog button. */
         if (drv->axis)
         {
            float normal_mag = 0.0f;
            if (input_analog_deadzone)
               normal_mag = fabs((1.0f / 0x7fff) * drv->axis(
                        joypad_info->joy_idx, axis));
            res = abs(input_joypad_axis(drv, 
                     joypad_info->joy_idx, axis, normal_mag));
         }
         /* If the result is zero, it's got a digital button
          * attached to it instead */
         if (res == 0)
         {
            uint16_t key = (bind->joykey == NO_BTN)
               ? joypad_info->auto_binds[ident].joykey
               : bind->joykey;

            if (drv->button(joypad_info->joy_idx, key))
               res = 0x7fff;
         }
      }
   }
   else
   {
      /* Analog sticks. Either RETRO_DEVICE_INDEX_ANALOG_LEFT
       * or RETRO_DEVICE_INDEX_ANALOG_RIGHT */

      unsigned ident_minus                     = 0;
      unsigned ident_plus                      = 0;
      unsigned ident_x_minus                   = 0;
      unsigned ident_x_plus                    = 0;
      unsigned ident_y_minus                   = 0;
      unsigned ident_y_plus                    = 0;
      const struct retro_keybind *bind_minus   = NULL;
      const struct retro_keybind *bind_plus    = NULL;
      const struct retro_keybind *bind_x_minus = NULL;
      const struct retro_keybind *bind_x_plus  = NULL;
      const struct retro_keybind *bind_y_minus = NULL;
      const struct retro_keybind *bind_y_plus  = NULL;

      input_conv_analog_id_to_bind_id(idx, ident, ident_minus, ident_plus);

      bind_minus                             = &binds[ident_minus];
      bind_plus                              = &binds[ident_plus];

      if (!bind_minus->valid || !bind_plus->valid)
         return 0;

      input_conv_analog_id_to_bind_id(idx, RETRO_DEVICE_ID_ANALOG_X, ident_x_minus, ident_x_plus);

      bind_x_minus = &binds[ident_x_minus];
      bind_x_plus  = &binds[ident_x_plus];

      if (!bind_x_minus->valid || !bind_x_plus->valid)
         return 0;

      input_conv_analog_id_to_bind_id(idx, RETRO_DEVICE_ID_ANALOG_Y, ident_y_minus, ident_y_plus);

      bind_y_minus = &binds[ident_y_minus];
      bind_y_plus  = &binds[ident_y_plus];

      if (!bind_y_minus->valid || !bind_y_plus->valid)
         return 0;

      if (drv->axis)
      {
         uint32_t axis_minus      = (bind_minus->joyaxis   == AXIS_NONE)
            ? joypad_info->auto_binds[ident_minus].joyaxis
            : bind_minus->joyaxis;
         uint32_t axis_plus       = (bind_plus->joyaxis    == AXIS_NONE)
            ? joypad_info->auto_binds[ident_plus].joyaxis
            : bind_plus->joyaxis;
         int16_t pressed_minus    = 0;
         int16_t pressed_plus     = 0;
         float normal_mag         = 0.0f;

         /* normalized magnitude of stick actuation, needed for scaled
          * radial deadzone */
         if (input_analog_deadzone)
         {
            uint32_t x_axis_minus    = (bind_x_minus->joyaxis == AXIS_NONE)
               ? joypad_info->auto_binds[ident_x_minus].joyaxis
               : bind_x_minus->joyaxis;
            uint32_t x_axis_plus     = (bind_x_plus->joyaxis  == AXIS_NONE)
               ? joypad_info->auto_binds[ident_x_plus].joyaxis
               : bind_x_plus->joyaxis;
            uint32_t y_axis_minus    = (bind_y_minus->joyaxis == AXIS_NONE)
               ? joypad_info->auto_binds[ident_y_minus].joyaxis
               : bind_y_minus->joyaxis;
            uint32_t y_axis_plus     = (bind_y_plus->joyaxis  == AXIS_NONE)
               ? joypad_info->auto_binds[ident_y_plus].joyaxis
               : bind_y_plus->joyaxis;
            /* normalized magnitude for radial scaled analog deadzone */
            float x                  = drv->axis(
                  joypad_info->joy_idx, x_axis_plus)
               + drv->axis(joypad_info->joy_idx, x_axis_minus);
            float y                  = drv->axis(
                  joypad_info->joy_idx, y_axis_plus)
               + drv->axis(joypad_info->joy_idx, y_axis_minus);
            normal_mag = (1.0f / 0x7fff) * sqrt(x * x + y * y);
         }

         pressed_minus = abs(
               input_joypad_axis(drv, joypad_info->joy_idx,
                  axis_minus, normal_mag));
         pressed_plus  = abs(
               input_joypad_axis(drv, joypad_info->joy_idx,
                  axis_plus, normal_mag));
         res           = pressed_plus - pressed_minus;
      }

      if (res == 0)
      {
         uint16_t key_minus    = (bind_minus->joykey == NO_BTN)
            ? joypad_info->auto_binds[ident_minus].joykey
            : bind_minus->joykey;
         uint16_t key_plus     = (bind_plus->joykey  == NO_BTN)
            ? joypad_info->auto_binds[ident_plus].joykey
            : bind_plus->joykey;
         int16_t digital_left  = drv->button(joypad_info->joy_idx, key_minus)
            ? -0x7fff : 0;
         int16_t digital_right = drv->button(joypad_info->joy_idx, key_plus)
            ? 0x7fff  : 0;

         return digital_right + digital_left;
      }
   }

   return res;
}

/**
 * input_mouse_button_raw:
 * @port                    : Mouse number.
 * @button                  : Identifier of key (libretro mouse constant).
 *
 * Checks if key (@button) was being pressed by user
 * with mouse number @port.
 *
 * Returns: true (1) if key was pressed, otherwise
 * false (0).
 **/
bool input_mouse_button_raw(unsigned port, unsigned id)
{
   rarch_joypad_info_t joypad_info;
   struct rarch_state       *p_rarch = &rarch_st;
   settings_t              *settings = p_rarch->configuration_settings;

   /*ignore axes*/
   if (id == RETRO_DEVICE_ID_MOUSE_X || id == RETRO_DEVICE_ID_MOUSE_Y)
      return false;

   joypad_info.axis_threshold        = p_rarch->input_driver_axis_threshold;
   joypad_info.joy_idx               = settings->uints.input_joypad_map[port];
   joypad_info.auto_binds            = input_autoconf_binds[joypad_info.joy_idx];

   if (p_rarch->current_input->input_state(p_rarch->current_input_data,
         &joypad_info, p_rarch->libretro_input_binds, port, RETRO_DEVICE_MOUSE, 0, id))
      return true;
   return false;
}

void input_pad_connect(unsigned port, input_device_driver_t *driver)
{
   if (port >= MAX_USERS || !driver)
   {
      RARCH_ERR("[input]: input_pad_connect: bad parameters\n");
      return;
   }

   fire_connection_listener(port, driver);

   input_autoconfigure_connect(driver->name(port), NULL, driver->ident,
          port, 0, 0);
}

#ifdef HAVE_HID
static const void *hid_driver_find_handle(int idx)
{
   const void *drv = hid_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

const void *hid_driver_get_data(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->hid_data;
}

/* This is only to be called after we've invoked free() on the
 * HID driver; the memory will have already been freed, so we need to
 * reset the pointer.
 */
void hid_driver_reset_data(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->hid_data = NULL;
}

/**
 * config_get_hid_driver_options:
 *
 * Get an enumerated list of all HID driver names, separated by '|'.
 *
 * Returns: string listing of all HID driver names, separated by '|'.
 **/
const char* config_get_hid_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_HID_DRIVERS, NULL);
}

/**
 * input_hid_init_first:
 *
 * Finds first suitable HID driver and initializes.
 *
 * Returns: HID driver if found, otherwise NULL.
 **/
const hid_driver_t *input_hid_init_first(void)
{
   unsigned i;
   struct rarch_state *p_rarch = &rarch_st;

   for (i = 0; hid_drivers[i]; i++)
   {
      p_rarch->hid_data = hid_drivers[i]->init();

      if (p_rarch->hid_data)
      {
         RARCH_LOG("Found HID driver: \"%s\".\n",
               hid_drivers[i]->ident);
         return hid_drivers[i];
      }
   }

   return NULL;
}
#endif

static void osk_update_last_codepoint(const char *word)
{
   const char *letter                    = word;
   const char    *pos                    = letter;
   struct rarch_state           *p_rarch = &rarch_st;

   for (;;)
   {
      unsigned codepoint                 = utf8_walk(&letter);
      unsigned       len                 = (unsigned)(letter - pos);

      if (letter[0] == 0)
      {
         p_rarch->osk_last_codepoint     = codepoint;
         p_rarch->osk_last_codepoint_len = len;
         break;
      }

      pos                                = letter;
   }
}

/**
 * input_keyboard_line_event:
 * @state                    : Input keyboard line handle.
 * @character                : Inputted character.
 *
 * Called on every keyboard character event.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
static bool input_keyboard_line_event(
      input_keyboard_line_t *state, uint32_t character)
{
   char array[2];
   bool            ret         = false;
   const char            *word = NULL;
   struct rarch_state *p_rarch = &rarch_st;
   char            c           = character >= 128 ? '?' : character;

   /* Treat extended chars as ? as we cannot support
    * printable characters for unicode stuff. */

   if (c == '\r' || c == '\n')
   {
      state->cb(state->userdata, state->buffer);

      array[0] = c;
      array[1] = 0;

      word     = array;
      ret      = true;
   }
   else if (c == '\b' || c == '\x7f') /* 0x7f is ASCII for del */
   {
      if (state->ptr)
      {
         unsigned i;

         for (i = 0; i < p_rarch->osk_last_codepoint_len; i++)
         {
            memmove(state->buffer + state->ptr - 1,
                  state->buffer + state->ptr,
                  state->size - state->ptr + 1);
            state->ptr--;
            state->size--;
         }

         word     = state->buffer;
      }
   }
   else if (ISPRINT(c))
   {
      /* Handle left/right here when suitable */
      char *newbuf = (char*)
         realloc(state->buffer, state->size + 2);
      if (!newbuf)
         return false;

      memmove(newbuf + state->ptr + 1,
            newbuf + state->ptr,
            state->size - state->ptr + 1);
      newbuf[state->ptr] = c;
      state->ptr++;
      state->size++;
      newbuf[state->size] = '\0';

      state->buffer = newbuf;

      array[0] = c;
      array[1] = 0;

      word     = array;
   }

   if (word)
   {
      /* OSK - update last character */
      if (word[0] == 0)
      {
         p_rarch->osk_last_codepoint     = 0;
         p_rarch->osk_last_codepoint_len = 0;
      }
      else
         osk_update_last_codepoint(word);
   }

   return ret;
}

bool input_keyboard_line_append(const char *word)
{
   struct rarch_state *p_rarch = &rarch_st;
   unsigned i                  = 0;
   unsigned len                = (unsigned)strlen(word);
   char *newbuf                = (char*)realloc(
         p_rarch->keyboard_line->buffer,
         p_rarch->keyboard_line->size + len*2);

   if (!newbuf)
      return false;

   memmove(newbuf + p_rarch->keyboard_line->ptr + len,
         newbuf + p_rarch->keyboard_line->ptr,
         p_rarch->keyboard_line->size - p_rarch->keyboard_line->ptr + len);

   for (i = 0; i < len; i++)
   {
      newbuf[p_rarch->keyboard_line->ptr] = word[i];
      p_rarch->keyboard_line->ptr++;
      p_rarch->keyboard_line->size++;
   }

   newbuf[p_rarch->keyboard_line->size]      = '\0';

   p_rarch->keyboard_line->buffer            = newbuf;

   if (word[0] == 0)
   {
      p_rarch->osk_last_codepoint     = 0;
      p_rarch->osk_last_codepoint_len = 0;
   }
   else
      osk_update_last_codepoint(word);

   return false;
}

/**
 * input_keyboard_start_line:
 * @userdata                 : Userdata.
 * @cb                       : Line complete callback function.
 *
 * Sets function pointer for keyboard line handle.
 *
 * The underlying buffer can be reallocated at any time
 * (or be NULL), but the pointer to it remains constant
 * throughout the objects lifetime.
 *
 * Returns: underlying buffer of the keyboard line.
 **/
const char **input_keyboard_start_line(void *userdata,
      input_keyboard_line_complete_t cb)
{
   struct rarch_state  *p_rarch = &rarch_st;
   input_keyboard_line_t *state = (input_keyboard_line_t*)
      calloc(1, sizeof(*state));
   if (!state)
      return NULL;

   p_rarch->keyboard_line           = state;
   p_rarch->keyboard_line->cb       = cb;
   p_rarch->keyboard_line->userdata = userdata;

   /* While reading keyboard line input, we have to block all hotkeys. */
   p_rarch->current_input->keyboard_mapping_blocked = true;

   return (const char**)&p_rarch->keyboard_line->buffer;
}

/**
 * input_keyboard_event:
 * @down                     : Keycode was pressed down?
 * @code                     : Keycode.
 * @character                : Character inputted.
 * @mod                      : TODO/FIXME: ???
 *
 * Keyboard event utils. Called by drivers when keyboard events are fired.
 * This interfaces with the global system driver struct and libretro callbacks.
 **/
void input_keyboard_event(bool down, unsigned code,
      uint32_t character, uint16_t mod, unsigned device)
{
   static bool deferred_wait_keys;
   struct rarch_state *p_rarch   = &rarch_st;

#ifdef HAVE_ACCESSIBILITY
#ifdef HAVE_MENU
   if (menu_input_dialog_get_display_kb()
         && down && is_accessibility_enabled())
   {
      if (code != 303 && code != 0)
      {
         char* say_char = (char*)malloc(sizeof(char)+1);

         if (say_char)
         {
            char c    = (char) character;
            *say_char = c;

            if (character == 127)
               accessibility_speak_priority("backspace", 10);
            else if (c == '`')
               accessibility_speak_priority("left quote", 10);
            else if (c == '`')
               accessibility_speak_priority("tilde", 10);
            else if (c == '!')
               accessibility_speak_priority("exclamation point", 10);
            else if (c == '@')
               accessibility_speak_priority("at sign", 10);
            else if (c == '#')
               accessibility_speak_priority("hash sign", 10);
            else if (c == '$')
               accessibility_speak_priority("dollar sign", 10);
            else if (c == '%')
               accessibility_speak_priority("percent sign", 10);
            else if (c == '^')
               accessibility_speak_priority("carrot", 10);
            else if (c == '&')
               accessibility_speak_priority("ampersand", 10);
            else if (c == '*')
               accessibility_speak_priority("asterisk", 10);
            else if (c == '(')
               accessibility_speak_priority("left bracket", 10);
            else if (c == ')')
               accessibility_speak_priority("right bracket", 10);
            else if (c == '-')
               accessibility_speak_priority("minus", 10);
            else if (c == '_')
               accessibility_speak_priority("underscore", 10);
            else if (c == '=')
               accessibility_speak_priority("equals", 10);
            else if (c == '+')
               accessibility_speak_priority("plus", 10);
            else if (c == '[')
               accessibility_speak_priority("left square bracket", 10);
            else if (c == '{')
               accessibility_speak_priority("left curl bracket", 10);
            else if (c == ']')
               accessibility_speak_priority("right square bracket", 10);
            else if (c == '}')
               accessibility_speak_priority("right curl bracket", 10);
            else if (c == '\\')
               accessibility_speak_priority("back slash", 10);
            else if (c == '|')
               accessibility_speak_priority("pipe", 10);
            else if (c == ';')
               accessibility_speak_priority("semicolon", 10);
            else if (c == ':')
               accessibility_speak_priority("colon", 10);
            else if (c == '\'')
               accessibility_speak_priority("single quote", 10);
            else if (c  == '\"')
               accessibility_speak_priority("double quote", 10);
            else if (c == ',')
               accessibility_speak_priority("comma", 10);
            else if (c == '<')
               accessibility_speak_priority("left angle bracket", 10);
            else if (c == '.')
               accessibility_speak_priority("period", 10);
            else if (c == '>')
               accessibility_speak_priority("right angle bracket", 10);
            else if (c == '/')
               accessibility_speak_priority("front slash", 10);
            else if (c == '?')
               accessibility_speak_priority("question mark", 10);
            else if (c == ' ')
               accessibility_speak_priority("space", 10);
            else if (character != 0)
               accessibility_speak_priority(say_char, 10);
            free(say_char);
         }
      }
   }
#endif
#endif

   if (deferred_wait_keys)
   {
      if (down)
         return;

      p_rarch->keyboard_press_cb                       = NULL;
      p_rarch->keyboard_press_data                     = NULL;
      p_rarch->current_input->keyboard_mapping_blocked = false;
      deferred_wait_keys                               = false;
   }
   else if (p_rarch->keyboard_press_cb)
   {
      if (!down || code == RETROK_UNKNOWN)
         return;
      if (p_rarch->keyboard_press_cb(p_rarch->keyboard_press_data, code))
         return;
      deferred_wait_keys = true;
   }
   else if (p_rarch->keyboard_line)
   {
      if (!down)
         return;

      switch (device)
      {
         case RETRO_DEVICE_POINTER:
            if (code != 0x12d)
               character = (char)code;
            /* fall-through */
         default:
            if (!input_keyboard_line_event(p_rarch->keyboard_line, character))
               return;
            break;
      }

      /* Line is complete, can free it now. */
      if (p_rarch->keyboard_line)
      {
         if (p_rarch->keyboard_line->buffer)
            free(p_rarch->keyboard_line->buffer);
         free(p_rarch->keyboard_line);
      }
      p_rarch->keyboard_line                           = NULL;

      /* Unblock all hotkeys. */
      p_rarch->current_input->keyboard_mapping_blocked = false;
   }
   else
   {
      retro_keyboard_event_t *key_event = &p_rarch->runloop_key_event;

      if (*key_event)
         (*key_event)(down, code, character, mod);
   }
}

bool input_keyboard_ctl(
      enum rarch_input_keyboard_ctl_state state, void *data)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (state)
   {
      case RARCH_INPUT_KEYBOARD_CTL_LINE_FREE:
         if (p_rarch->keyboard_line)
         {
            if (p_rarch->keyboard_line->buffer)
               free(p_rarch->keyboard_line->buffer);
            free(p_rarch->keyboard_line);
         }
         p_rarch->keyboard_line = NULL;
         break;
      case RARCH_INPUT_KEYBOARD_CTL_START_WAIT_KEYS:
         {
            input_keyboard_ctx_wait_t *keys      = (input_keyboard_ctx_wait_t*)data;

            if (!keys)
               return false;

            p_rarch->keyboard_press_cb           = keys->cb;
            p_rarch->keyboard_press_data         = keys->userdata;
         }

         /* While waiting for input, we have to block all hotkeys. */
         p_rarch->current_input->keyboard_mapping_blocked = true;
         break;
      case RARCH_INPUT_KEYBOARD_CTL_CANCEL_WAIT_KEYS:
         p_rarch->keyboard_press_cb                       = NULL;
         p_rarch->keyboard_press_data                     = NULL;
         p_rarch->current_input->keyboard_mapping_blocked = false;
         break;
      case RARCH_INPUT_KEYBOARD_CTL_IS_LINEFEED_ENABLED:
         return p_rarch->input_driver_keyboard_linefeed_enable;
      case RARCH_INPUT_KEYBOARD_CTL_NONE:
      default:
         break;
   }

   return true;
}

static bool input_config_bind_map_get_valid(unsigned i)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)input_config_bind_map_get(i);
   if (!keybind)
      return false;
   return keybind->valid;
}

unsigned input_config_bind_map_get_meta(unsigned i)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)input_config_bind_map_get(i);
   if (!keybind)
      return 0;
   return keybind->meta;
}

const char *input_config_bind_map_get_base(unsigned i)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)input_config_bind_map_get(i);
   if (!keybind)
      return NULL;
   return keybind->base;
}

const char *input_config_bind_map_get_desc(unsigned i)
{
   const struct input_bind_map *keybind =
      (const struct input_bind_map*)input_config_bind_map_get(i);
   if (!keybind)
      return NULL;
   return msg_hash_to_str(keybind->desc);
}

static void input_config_parse_key(
      config_file_t *conf,
      const char *prefix, const char *btn,
      struct retro_keybind *bind)
{
   char tmp[64];
   char key[64];

   tmp[0] = key[0] = '\0';

   fill_pathname_join_delim(key, prefix, btn, '_', sizeof(key));

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
      bind->key = input_config_translate_str_to_rk(tmp);
}

static const char *input_config_get_prefix(unsigned user, bool meta)
{
   static const char *bind_user_prefix[MAX_USERS] = {
      "input_player1",
      "input_player2",
      "input_player3",
      "input_player4",
      "input_player5",
      "input_player6",
      "input_player7",
      "input_player8",
      "input_player9",
      "input_player10",
      "input_player11",
      "input_player12",
      "input_player13",
      "input_player14",
      "input_player15",
      "input_player16",
   };
   const char *prefix = bind_user_prefix[user];

   if (user == 0)
      return meta ? "input" : prefix;

   if (!meta)
      return prefix;

   /* Don't bother with meta bind for anyone else than first user. */
   return NULL;
}

/**
 * input_config_translate_str_to_rk:
 * @str                            : String to translate to key ID.
 *
 * Translates tring representation to key identifier.
 *
 * Returns: key identifier.
 **/
enum retro_key input_config_translate_str_to_rk(const char *str)
{
   size_t i;
   if (strlen(str) == 1 && isalpha((int)*str))
      return (enum retro_key)(RETROK_a + (tolower((int)*str) - (int)'a'));
   for (i = 0; input_config_key_map[i].str; i++)
   {
      if (string_is_equal_noncase(input_config_key_map[i].str, str))
         return input_config_key_map[i].key;
   }

   RARCH_WARN("Key name %s not found.\n", str);
   return RETROK_UNKNOWN;
}

/**
 * input_config_translate_str_to_bind_id:
 * @str                            : String to translate to bind ID.
 *
 * Translate string representation to bind ID.
 *
 * Returns: Bind ID value on success, otherwise
 * RARCH_BIND_LIST_END on not found.
 **/
unsigned input_config_translate_str_to_bind_id(const char *str)
{
   unsigned i;

   for (i = 0; input_config_bind_map[i].valid; i++)
      if (string_is_equal(str, input_config_bind_map[i].base))
         return i;

   return RARCH_BIND_LIST_END;
}

static void parse_hat(struct retro_keybind *bind, const char *str)
{
   uint16_t hat_dir = 0;
   char        *dir = NULL;
   uint16_t     hat = strtoul(str, &dir, 0);

   if (!dir)
   {
      RARCH_WARN("Found invalid hat in config!\n");
      return;
   }

   if (     dir[0] == 'u'
         && dir[1] == 'p'
         && dir[2] == '\0'
      )
      hat_dir = HAT_UP_MASK;
   else if (     dir[0] == 'd'
              && dir[1] == 'o'
              && dir[2] == 'w'
              && dir[3] == 'n'
              && dir[4] == '\0'
      )
      hat_dir = HAT_DOWN_MASK;
   else if (     dir[0] == 'l'
              && dir[1] == 'e'
              && dir[2] == 'f'
              && dir[3] == 't'
              && dir[4] == '\0'
      )
      hat_dir = HAT_LEFT_MASK;
   else if (     dir[0] == 'r'
              && dir[1] == 'i'
              && dir[2] == 'g'
              && dir[3] == 'h'
              && dir[4] == 't'
              && dir[5] == '\0'
      )
      hat_dir = HAT_RIGHT_MASK;

   if (hat_dir)
      bind->joykey = HAT_MAP(hat, hat_dir);
}

static void input_config_parse_joy_button(
      config_file_t *conf, const char *prefix,
      const char *btn, struct retro_keybind *bind)
{
   char str[256];
   char tmp[64];
   char key[64];
   char key_label[64];
   char *tmp_a              = NULL;

   str[0] = tmp[0] = key[0] = key_label[0] = '\0';

   fill_pathname_join_delim(str, prefix, btn,
         '_', sizeof(str));
   fill_pathname_join_delim(key, str,
         "btn", '_', sizeof(key));
   fill_pathname_join_delim(key_label, str,
         "btn_label", '_', sizeof(key_label));

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      btn = tmp;
      if (     btn[0] == 'n'
            && btn[1] == 'u'
            && btn[2] == 'l'
            && btn[3] == '\0'
         )
         bind->joykey = NO_BTN;
      else
      {
         if (*btn == 'h')
         {
            const char *str = btn + 1;
            if (bind && str && isdigit((int)*str))
               parse_hat(bind, str);
         }
         else
            bind->joykey = strtoull(tmp, NULL, 0);
      }
   }

   if (bind && config_get_string(conf, key_label, &tmp_a))
   {
      if (!string_is_empty(bind->joykey_label))
         free(bind->joykey_label);

      bind->joykey_label = strdup(tmp_a);
      free(tmp_a);
   }
}

static void input_config_parse_joy_axis(
      config_file_t *conf, const char *prefix,
      const char *axis, struct retro_keybind *bind)
{
   char str[256];
   char       tmp[64];
   char       key[64];
   char key_label[64];
   char        *tmp_a       = NULL;

   str[0] = tmp[0] = key[0] = key_label[0] = '\0';

   fill_pathname_join_delim(str, prefix, axis,
         '_', sizeof(str));
   fill_pathname_join_delim(key, str,
         "axis", '_', sizeof(key));
   fill_pathname_join_delim(key_label, str,
         "axis_label", '_', sizeof(key_label));

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      if (     tmp[0] == 'n'
            && tmp[1] == 'u'
            && tmp[2] == 'l'
            && tmp[3] == '\0'
         )
         bind->joyaxis = AXIS_NONE;
      else if (strlen(tmp) >= 2 && (*tmp == '+' || *tmp == '-'))
      {
         int i_axis = (int)strtol(tmp + 1, NULL, 0);
         if (*tmp == '+')
            bind->joyaxis = AXIS_POS(i_axis);
         else
            bind->joyaxis = AXIS_NEG(i_axis);
      }

      /* Ensure that D-pad emulation doesn't screw this over. */
      bind->orig_joyaxis = bind->joyaxis;
   }

   if (config_get_string(conf, key_label, &tmp_a))
   {
      if (bind->joyaxis_label &&
            !string_is_empty(bind->joyaxis_label))
         free(bind->joyaxis_label);
      bind->joyaxis_label = strdup(tmp_a);
      free(tmp_a);
   }
}

static void input_config_parse_mouse_button(
      config_file_t *conf, const char *prefix,
      const char *btn, struct retro_keybind *bind)
{
   int val;
   char str[256];
   char tmp[64];
   char key[64];

   str[0] = tmp[0] = key[0] = '\0';

   fill_pathname_join_delim(str, prefix, btn,
         '_', sizeof(str));
   fill_pathname_join_delim(key, str,
         "mbtn", '_', sizeof(key));

   if (bind && config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      bind->mbutton = NO_BTN;

      if (tmp[0]=='w')
      {
         switch (tmp[1])
         {
            case 'u':
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_WHEELUP;
               break;
            case 'd':
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_WHEELDOWN;
               break;
            case 'h':
               switch (tmp[2])
               {
                  case 'u':
                     bind->mbutton = RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP;
                     break;
                  case 'd':
                     bind->mbutton = RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN;
                     break;
               }
               break;
         }
      }
      else
      {
         val = atoi(tmp);
         switch (val)
         {
            case 1:
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_LEFT;
               break;
            case 2:
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_RIGHT;
               break;
            case 3:
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_MIDDLE;
               break;
            case 4:
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_BUTTON_4;
               break;
            case 5:
               bind->mbutton = RETRO_DEVICE_ID_MOUSE_BUTTON_5;
               break;
         }
      }
   }
}

static void input_config_get_bind_string_joykey(
      char *buf, const char *prefix,
      const struct retro_keybind *bind, size_t size)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t  *settings       = p_rarch->configuration_settings;
   bool  label_show            = settings->bools.input_descriptor_label_show;

   if (GET_HAT_DIR(bind->joykey))
   {
      if (bind->joykey_label &&
            !string_is_empty(bind->joykey_label) && label_show)
         fill_pathname_join_delim_concat(buf, prefix,
               bind->joykey_label, ' ', " (hat)", size);
      else
      {
         const char *dir = "?";

         switch (GET_HAT_DIR(bind->joykey))
         {
            case HAT_UP_MASK:
               dir = "up";
               break;
            case HAT_DOWN_MASK:
               dir = "down";
               break;
            case HAT_LEFT_MASK:
               dir = "left";
               break;
            case HAT_RIGHT_MASK:
               dir = "right";
               break;
            default:
               break;
         }
         snprintf(buf, size, "%sHat #%u %s (%s)", prefix,
               (unsigned)GET_HAT(bind->joykey), dir,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
      }
   }
   else
   {
      if (bind->joykey_label &&
            !string_is_empty(bind->joykey_label) && label_show)
         fill_pathname_join_delim_concat(buf, prefix,
               bind->joykey_label, ' ', " (btn)", size);
      else
         snprintf(buf, size, "%s%u (%s)", prefix, (unsigned)bind->joykey,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
   }
}

static void input_config_get_bind_string_joyaxis(char *buf, const char *prefix,
      const struct retro_keybind *bind, size_t size)
{
   struct rarch_state      *p_rarch = &rarch_st;
   settings_t *settings             = p_rarch->configuration_settings;
   bool input_descriptor_label_show = settings->bools.input_descriptor_label_show;

   if (bind->joyaxis_label &&
         !string_is_empty(bind->joyaxis_label)
         && input_descriptor_label_show)
      fill_pathname_join_delim_concat(buf, prefix,
            bind->joyaxis_label, ' ', " (axis)", size);
   else
   {
      unsigned axis        = 0;
      char dir             = '\0';
      if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
      {
         dir = '-';
         axis = AXIS_NEG_GET(bind->joyaxis);
      }
      else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
      {
         dir = '+';
         axis = AXIS_POS_GET(bind->joyaxis);
      }
      snprintf(buf, size, "%s%c%u (%s)", prefix, dir, axis,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
   }
}

void input_config_get_bind_string(char *buf, const struct retro_keybind *bind,
      const struct retro_keybind *auto_bind, size_t size)
{
   int delim = 0;

   *buf = '\0';

   if (bind->joykey != NO_BTN)
      input_config_get_bind_string_joykey(buf, "", bind, size);
   else if (bind->joyaxis != AXIS_NONE)
      input_config_get_bind_string_joyaxis(buf, "", bind, size);
   else if (auto_bind && auto_bind->joykey != NO_BTN)
      input_config_get_bind_string_joykey(buf, "Auto: ", auto_bind, size);
   else if (auto_bind && auto_bind->joyaxis != AXIS_NONE)
      input_config_get_bind_string_joyaxis(buf, "Auto: ", auto_bind, size);

   if (*buf)
      delim = 1;

#ifndef RARCH_CONSOLE
   {
      char key[64];
      key[0] = '\0';

      input_keymaps_translate_rk_to_str(bind->key, key, sizeof(key));
      if (     key[0] == 'n'
            && key[1] == 'u'
            && key[2] == 'l'
            && key[3] == '\0'
         )
         *key = '\0';
      /*empty?*/
      if (*key != '\0')
      {
         char keybuf[64];

         keybuf[0] = '\0';

         if (delim)
            strlcat(buf, ", ", size);
         snprintf(keybuf, sizeof(keybuf),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_KEY), key);
         strlcat(buf, keybuf, size);
         delim = 1;
      }
   }
#endif

   if (bind->mbutton != NO_BTN)
   {
      int tag = 0;
      switch (bind->mbutton)
      {
         case RETRO_DEVICE_ID_MOUSE_LEFT:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT;
            break;
         case RETRO_DEVICE_ID_MOUSE_RIGHT:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT;
            break;
         case RETRO_DEVICE_ID_MOUSE_MIDDLE:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE;
            break;
         case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4;
            break;
         case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5;
            break;
         case RETRO_DEVICE_ID_MOUSE_WHEELUP:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP;
            break;
         case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN;
            break;
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP;
            break;
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN;
            break;
      }

      if (tag != 0)
      {
         if (delim)
            strlcat(buf, ", ", size);
         strlcat(buf, msg_hash_to_str((enum msg_hash_enums)tag), size);
      }
   }

   /*completely empty?*/
   if (*buf == '\0')
      strlcat(buf, "---", size);
}

unsigned input_config_get_device_count(void)
{
   unsigned num_devices;
   for (num_devices = 0; num_devices < MAX_INPUT_DEVICES; ++num_devices)
   {
      if (string_is_empty(input_device_names[num_devices]))
         break;
   }
   return num_devices;
}

const char *input_config_get_device_name(unsigned port)
{
   if (string_is_empty(input_device_names[port]))
      return NULL;
   return input_device_names[port];
}

const char *input_config_get_device_display_name(unsigned port)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (string_is_empty(p_rarch->input_device_display_names[port]))
      return NULL;
   return p_rarch->input_device_display_names[port];
}

const char *input_config_get_device_config_path(unsigned port)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (string_is_empty(p_rarch->input_device_config_paths[port]))
      return NULL;
   return p_rarch->input_device_config_paths[port];
}

const char *input_config_get_device_config_name(unsigned port)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (string_is_empty(p_rarch->input_device_config_names[port]))
      return NULL;
   return p_rarch->input_device_config_names[port];
}

void input_config_set_device_name(unsigned port, const char *name)
{
   if (string_is_empty(name))
      return;

   strlcpy(input_device_names[port],
         name,
         sizeof(input_device_names[port]));

   input_autoconfigure_joypad_reindex_devices();
}

void input_config_set_device_config_path(unsigned port, const char *path)
{
   if (!string_is_empty(path))
   {
      char parent_dir_name[128];
      struct rarch_state *p_rarch = &rarch_st;

      parent_dir_name[0] = '\0';

      if (fill_pathname_parent_dir_name(parent_dir_name,
               path, sizeof(parent_dir_name)))
         fill_pathname_join(p_rarch->input_device_config_paths[port],
               parent_dir_name, path_basename(path),
               sizeof(p_rarch->input_device_config_paths[port]));
   }
}

void input_config_set_device_config_name(unsigned port, const char *name)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!string_is_empty(name))
      strlcpy(p_rarch->input_device_config_names[port],
            name,
            sizeof(p_rarch->input_device_config_names[port]));
}

void input_config_set_device_display_name(unsigned port, const char *name)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!string_is_empty(name))
      strlcpy(p_rarch->input_device_display_names[port],
            name,
            sizeof(p_rarch->input_device_display_names[port]));
}

void input_config_clear_device_name(unsigned port)
{
   input_device_names[port][0] = '\0';
   input_autoconfigure_joypad_reindex_devices();
}

void input_config_clear_device_display_name(unsigned port)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->input_device_display_names[port][0] = '\0';
}

void input_config_clear_device_config_path(unsigned port)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->input_device_config_paths[port][0] = '\0';
}

void input_config_clear_device_config_name(unsigned port)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->input_device_config_names[port][0] = '\0';
}

unsigned *input_config_get_device_ptr(unsigned port)
{
   struct rarch_state      *p_rarch = &rarch_st;
   settings_t             *settings = p_rarch->configuration_settings;
   return &settings->uints.input_libretro_device[port];
}

unsigned input_config_get_device(unsigned port)
{
   struct rarch_state      *p_rarch = &rarch_st;
   settings_t             *settings = p_rarch->configuration_settings;
   return settings->uints.input_libretro_device[port];
}

void input_config_set_device(unsigned port, unsigned id)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t        *settings = p_rarch->configuration_settings;

   if (settings)
      configuration_set_uint(settings,
      settings->uints.input_libretro_device[port], id);
}


const struct retro_keybind *input_config_get_bind_auto(
      unsigned port, unsigned id)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t        *settings = p_rarch->configuration_settings;
   unsigned        joy_idx     = settings->uints.input_joypad_map[port];

   if (joy_idx < MAX_USERS)
      return &input_autoconf_binds[joy_idx][id];
   return NULL;
}

void input_config_set_pid(unsigned port, uint16_t pid)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->input_config_pid[port] = pid;
}

uint16_t input_config_get_pid(unsigned port)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->input_config_pid[port];
}

void input_config_set_vid(unsigned port, uint16_t vid)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->input_config_vid[port] = vid;
}

uint16_t input_config_get_vid(unsigned port)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->input_config_vid[port];
}

void input_config_reset(void)
{
   unsigned i, j;
   struct rarch_state *p_rarch = &rarch_st;

   retro_assert(sizeof(input_config_binds[0]) >= sizeof(retro_keybinds_1));
   retro_assert(sizeof(input_config_binds[1]) >= sizeof(retro_keybinds_rest));

   memcpy(input_config_binds[0], retro_keybinds_1, sizeof(retro_keybinds_1));

   for (i = 1; i < MAX_USERS; i++)
      memcpy(input_config_binds[i], retro_keybinds_rest,
            sizeof(retro_keybinds_rest));

   for (i = 0; i < MAX_USERS; i++)
   {
      p_rarch->input_config_vid[i]     = 0;
      p_rarch->input_config_pid[i]     = 0;
      p_rarch->libretro_input_binds[i] = input_config_binds[i];

      for (j = 0; j < 64; j++)
         input_device_names[i][j]      = 0;
   }
}

void config_read_keybinds_conf(void *data)
{
   unsigned i;
   config_file_t *conf = (config_file_t*)data;

   if (!conf)
      return;

   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned j;

      for (j = 0; input_config_bind_map_get_valid(j); j++)
      {
         struct retro_keybind *bind = &input_config_binds[i][j];
         const char *prefix         = input_config_get_prefix(i, input_config_bind_map_get_meta(j));
         const char *btn            = input_config_bind_map_get_base(j);

         if (!bind->valid)
            continue;
         if (!input_config_bind_map_get_valid(j))
            continue;
         if (!btn || !prefix)
            continue;

         input_config_parse_key(conf, prefix, btn, bind);
         input_config_parse_joy_button(conf, prefix, btn, bind);
         input_config_parse_joy_axis(conf, prefix, btn, bind);
         input_config_parse_mouse_button(conf, prefix, btn, bind);
      }
   }
}

void input_autoconfigure_joypad_conf(void *data,
      struct retro_keybind *binds)
{
   unsigned i;
   config_file_t *conf = (config_file_t*)data;

   if (!conf)
      return;

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      input_config_parse_joy_button(conf, "input",
            input_config_bind_map_get_base(i), &binds[i]);
      input_config_parse_joy_axis(conf, "input",
            input_config_bind_map_get_base(i), &binds[i]);
   }
}

/**
 * input_config_save_keybinds_user:
 * @conf               : pointer to config file object
 * @user               : user number
 *
 * Save the current keybinds of a user (@user) to the config file (@conf).
 */
void input_config_save_keybinds_user(void *data, unsigned user)
{
   unsigned i = 0;
   config_file_t *conf = (config_file_t*)data;

   for (i = 0; input_config_bind_map_get_valid(i); i++)
   {
      char key[64];
      char btn[64];
      const char *prefix               = input_config_get_prefix(user,
            input_config_bind_map_get_meta(i));
      const struct retro_keybind *bind = &input_config_binds[user][i];
      const char                 *base = input_config_bind_map_get_base(i);

      if (!prefix || !bind->valid)
         continue;

      key[0] = btn[0]  = '\0';

      fill_pathname_join_delim(key, prefix, base, '_', sizeof(key));

      input_keymaps_translate_rk_to_str(bind->key, btn, sizeof(btn));
      config_set_string(conf, key, btn);

      input_config_save_keybind(conf, prefix, base, bind, true);
   }
}

static void save_keybind_hat(config_file_t *conf, const char *key,
      const struct retro_keybind *bind)
{
   char config[16];
   unsigned hat     = (unsigned)GET_HAT(bind->joykey);
   const char *dir  = NULL;

   config[0]        = '\0';

   switch (GET_HAT_DIR(bind->joykey))
   {
      case HAT_UP_MASK:
         dir = "up";
         break;

      case HAT_DOWN_MASK:
         dir = "down";
         break;

      case HAT_LEFT_MASK:
         dir = "left";
         break;

      case HAT_RIGHT_MASK:
         dir = "right";
         break;

      default:
         break;
   }

   snprintf(config, sizeof(config), "h%u%s", hat, dir);
   config_set_string(conf, key, config);
}

static void save_keybind_joykey(config_file_t *conf,
      const char *prefix,
      const char *base,
      const struct retro_keybind *bind, bool save_empty)
{
   char key[64];

   key[0] = '\0';

   fill_pathname_join_delim_concat(key, prefix,
         base, '_', "_btn", sizeof(key));

   if (bind->joykey == NO_BTN)
   {
       if (save_empty)
         config_set_string(conf, key, "nul");
   }
   else if (GET_HAT_DIR(bind->joykey))
      save_keybind_hat(conf, key, bind);
   else
      config_set_uint64(conf, key, bind->joykey);
}

static void save_keybind_axis(config_file_t *conf,
      const char *prefix,
      const char *base,
      const struct retro_keybind *bind, bool save_empty)
{
   char key[64];
   unsigned axis   = 0;
   char dir        = '\0';

   key[0] = '\0';

   fill_pathname_join_delim_concat(key,
         prefix, base, '_',
         "_axis",
         sizeof(key));

   if (bind->joyaxis == AXIS_NONE)
   {
      if (save_empty)
         config_set_string(conf, key, "nul");
   }
   else if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '-';
      axis = AXIS_NEG_GET(bind->joyaxis);
   }
   else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '+';
      axis = AXIS_POS_GET(bind->joyaxis);
   }

   if (dir)
   {
      char config[16];

      config[0] = '\0';

      snprintf(config, sizeof(config), "%c%u", dir, axis);
      config_set_string(conf, key, config);
   }
}

static void save_keybind_mbutton(config_file_t *conf,
      const char *prefix,
      const char *base,
      const struct retro_keybind *bind, bool save_empty)
{
   char key[64];

   key[0] = '\0';

   fill_pathname_join_delim_concat(key, prefix,
      base, '_', "_mbtn", sizeof(key));

   switch (bind->mbutton)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         config_set_uint64(conf, key, 1);
         break;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         config_set_uint64(conf, key, 2);
         break;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         config_set_uint64(conf, key, 3);
         break;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         config_set_uint64(conf, key, 4);
         break;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         config_set_uint64(conf, key, 5);
         break;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         config_set_string(conf, key, "wu");
         break;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         config_set_string(conf, key, "wd");
         break;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         config_set_string(conf, key, "whu");
         break;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         config_set_string(conf, key, "whd");
         break;
      default:
         if (save_empty)
            config_set_string(conf, key, "nul");
         break;
   }
}

/**
 * input_config_save_keybind:
 * @conf               : pointer to config file object
 * @prefix             : prefix name of keybind
 * @base               : base name   of keybind
 * @bind               : pointer to key binding object
 * @kb                 : save keyboard binds
 *
 * Save a key binding to the config file.
 */
void input_config_save_keybind(void *data, const char *prefix,
      const char *base, const struct retro_keybind *bind,
      bool save_empty)
{
   config_file_t *conf = (config_file_t*)data;

   save_keybind_joykey (conf, prefix, base, bind, save_empty);
   save_keybind_axis   (conf, prefix, base, bind, save_empty);
   save_keybind_mbutton(conf, prefix, base, bind, save_empty);
}

/* MIDI */

static midi_driver_t *midi_driver_find_driver(const char *ident)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(midi_drivers); ++i)
   {
      if (string_is_equal(midi_drivers[i]->ident, ident))
         return midi_drivers[i];
   }

   RARCH_ERR("[MIDI]: Unknown driver \"%s\", falling back to \"null\" driver.\n", ident);

   return &midi_null;
}

static const void *midi_driver_find_handle(int index)
{
   if (index < 0 || index >= ARRAY_SIZE(midi_drivers))
      return NULL;

   return midi_drivers[index];
}

struct string_list *midi_driver_get_avail_inputs(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->midi_drv_inputs;
}

struct string_list *midi_driver_get_avail_outputs(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->midi_drv_outputs;
}

static bool midi_driver_set_all_sounds_off(void)
{
   midi_event_t event;
   uint8_t i;
   uint8_t data[3]     = { 0xB0, 120, 0 };
   bool result         = true;
   struct rarch_state 
      *p_rarch         = &rarch_st;

   if (!p_rarch->midi_drv_data || !p_rarch->midi_drv_output_enabled)
      return false;

   event.data       = data;
   event.data_size  = sizeof(data);
   event.delta_time = 0;

   for (i = 0; i < 16; ++i)
   {
      data[0] = 0xB0 | i;

      if (!midi_drv->write(p_rarch->midi_drv_data, &event))
         result = false;
   }

   if (!midi_drv->flush(p_rarch->midi_drv_data))
      result = false;

   if (!result)
      RARCH_ERR("[MIDI]: All sounds off failed.\n");

   return result;
}

bool midi_driver_set_volume(unsigned volume)
{
   midi_event_t event;
   struct rarch_state *p_rarch = &rarch_st;
   uint8_t         data[8]     = {
      0xF0, 0x7F, 0x7F, 0x04, 0x01, 0, 0, 0xF7};

   if (!p_rarch->midi_drv_data || !p_rarch->midi_drv_output_enabled)
      return false;

   volume           = (unsigned)(163.83 * volume + 0.5);
   if (volume > 16383)
      volume        = 16383;

   data[5]          = (uint8_t)(volume & 0x7F);
   data[6]          = (uint8_t)(volume >> 7);

   event.data       = data;
   event.data_size  = sizeof(data);
   event.delta_time = 0;

   if (!midi_drv->write(p_rarch->midi_drv_data, &event))
   {
      RARCH_ERR("[MIDI]: Volume change failed.\n");
      return false;
   }

   return true;
}

static bool midi_driver_init_io_buffers(void)
{
   struct rarch_state     *p_rarch          = &rarch_st;
   uint8_t *midi_drv_input_buffer           = (uint8_t*)malloc(MIDI_DRIVER_BUF_SIZE);
   uint8_t *midi_drv_output_buffer          = (uint8_t*)malloc(MIDI_DRIVER_BUF_SIZE);

   if (!midi_drv_input_buffer || !midi_drv_output_buffer)
      return false;
   
   p_rarch->midi_drv_input_buffer           = midi_drv_input_buffer;
   p_rarch->midi_drv_output_buffer          = midi_drv_output_buffer;

   p_rarch->midi_drv_input_event.data       = midi_drv_input_buffer;
   p_rarch->midi_drv_input_event.data_size  = 0;

   p_rarch->midi_drv_output_event.data      = midi_drv_output_buffer;
   p_rarch->midi_drv_output_event.data_size = 0;

   return true;
}

static void midi_driver_free(void)
{
   struct rarch_state *p_rarch = &rarch_st;

   if (p_rarch->midi_drv_data)
   {
      midi_drv->free(p_rarch->midi_drv_data);
      p_rarch->midi_drv_data = NULL;
   }

   if (p_rarch->midi_drv_inputs)
   {
      string_list_free(p_rarch->midi_drv_inputs);
      p_rarch->midi_drv_inputs = NULL;
   }

   if (p_rarch->midi_drv_outputs)
   {
      string_list_free(p_rarch->midi_drv_outputs);
      p_rarch->midi_drv_outputs = NULL;
   }

   if (p_rarch->midi_drv_input_buffer)
   {
      free(p_rarch->midi_drv_input_buffer);
      p_rarch->midi_drv_input_buffer = NULL;
   }

   if (p_rarch->midi_drv_output_buffer)
   {
      free(p_rarch->midi_drv_output_buffer);
      p_rarch->midi_drv_output_buffer = NULL;
   }

   p_rarch->midi_drv_input_enabled  = false;
   p_rarch->midi_drv_output_enabled = false;
}

static bool midi_driver_init(void)
{
   struct rarch_state       *p_rarch = &rarch_st;
   settings_t *settings              = p_rarch->configuration_settings;
   union string_list_elem_attr attr  = {0};
   const char *err_str               = NULL;

   p_rarch->midi_drv_inputs          = string_list_new();
   p_rarch->midi_drv_outputs         = string_list_new();

   RARCH_LOG("[MIDI]: Initializing ...\n");

   if (!settings)
      err_str = "settings unavailable";
   else if (!p_rarch->midi_drv_inputs || !p_rarch->midi_drv_outputs)
      err_str = "string_list_new failed";
   else if (!string_list_append(p_rarch->midi_drv_inputs, "Off", attr) ||
            !string_list_append(p_rarch->midi_drv_outputs, "Off", attr))
      err_str = "string_list_append failed";
   else
   {
      char * input  = NULL;
      char * output = NULL;

      midi_drv = midi_driver_find_driver(settings->arrays.midi_driver);
      if (strcmp(midi_drv->ident, settings->arrays.midi_driver))
      {
         configuration_set_string(settings,
               settings->arrays.midi_driver, midi_drv->ident);
      }

      if (!midi_drv->get_avail_inputs(p_rarch->midi_drv_inputs))
         err_str = "list of input devices unavailable";
      else if (!midi_drv->get_avail_outputs(p_rarch->midi_drv_outputs))
         err_str = "list of output devices unavailable";
      else
      {
         if (string_is_not_equal(settings->arrays.midi_input, "Off"))
         {
            if (string_list_find_elem(p_rarch->midi_drv_inputs, settings->arrays.midi_input))
               input = settings->arrays.midi_input;
            else
            {
               RARCH_WARN("[MIDI]: Input device \"%s\" unavailable.\n",
                     settings->arrays.midi_input);
               configuration_set_string(settings,
                     settings->arrays.midi_input, "Off");
            }
         }

         if (string_is_not_equal(settings->arrays.midi_output, "Off"))
         {
            if (string_list_find_elem(p_rarch->midi_drv_outputs, settings->arrays.midi_output))
               output = settings->arrays.midi_output;
            else
            {
               RARCH_WARN("[MIDI]: Output device \"%s\" unavailable.\n",
                     settings->arrays.midi_output);
               configuration_set_string(settings,
                     settings->arrays.midi_output, "Off");
            }
         }

         p_rarch->midi_drv_data = midi_drv->init(input, output);
         if (!p_rarch->midi_drv_data)
            err_str = "driver init failed";
         else
         {
            p_rarch->midi_drv_input_enabled  = (input  != NULL);
            p_rarch->midi_drv_output_enabled = (output != NULL);

            if (!midi_driver_init_io_buffers())
               err_str = "out of memory";
            else
            {
               if (input)
                  RARCH_LOG("[MIDI]: Input device \"%s\".\n", input);
               else
                  RARCH_LOG("[MIDI]: Input disabled.\n");

               if (output)
               {
                  RARCH_LOG("[MIDI]: Output device \"%s\".\n", output);
                  midi_driver_set_volume(settings->uints.midi_volume);
               }
               else
                  RARCH_LOG("[MIDI]: Output disabled.\n");
            }
         }
      }
   }

   if (err_str)
   {
      midi_driver_free();
      RARCH_ERR("[MIDI]: Initialization failed (%s).\n", err_str);
   }
   else
      RARCH_LOG("[MIDI]: Initialized \"%s\" driver.\n", midi_drv->ident);

   return err_str == NULL;
}

bool midi_driver_set_input(const char *input)
{
   struct rarch_state *p_rarch = &rarch_st;

   if (!p_rarch->midi_drv_data)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_set_input called on uninitialized driver.\n");
#endif
      return false;
   }

   if (string_is_equal(input, "Off"))
      input = NULL;

   if (!midi_drv->set_input(p_rarch->midi_drv_data, input))
   {
      if (input)
         RARCH_ERR("[MIDI]: Failed to change input device to \"%s\".\n", input);
      else
         RARCH_ERR("[MIDI]: Failed to disable input.\n");
      return false;
   }

   if (input)
      RARCH_LOG("[MIDI]: Input device changed to \"%s\".\n", input);
   else
      RARCH_LOG("[MIDI]: Input disabled.\n");

   p_rarch->midi_drv_input_enabled = input != NULL;

   return true;
}

bool midi_driver_set_output(const char *output)
{
   struct rarch_state *p_rarch = &rarch_st;

   if (!p_rarch->midi_drv_data)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_set_output called on uninitialized driver.\n");
#endif
      return false;
   }

   if (string_is_equal(output, "Off"))
      output = NULL;

   if (!midi_drv->set_output(p_rarch->midi_drv_data, output))
   {
      if (output)
         RARCH_ERR("[MIDI]: Failed to change output device to \"%s\".\n", output);
      else
         RARCH_ERR("[MIDI]: Failed to disable output.\n");
      return false;
   }

   if (output)
   {
      settings_t *settings             = p_rarch->configuration_settings;

      p_rarch->midi_drv_output_enabled = true;
      RARCH_LOG("[MIDI]: Output device changed to \"%s\".\n", output);

      if (settings)
         midi_driver_set_volume(settings->uints.midi_volume);
      else
         RARCH_ERR("[MIDI]: Volume change failed (settings unavailable).\n");
   }
   else
   {
      p_rarch->midi_drv_output_enabled = false;
      RARCH_LOG("[MIDI]: Output disabled.\n");
   }

   return true;
}

static bool midi_driver_input_enabled(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->midi_drv_input_enabled;
}

static bool midi_driver_output_enabled(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->midi_drv_output_enabled;
}

static bool midi_driver_read(uint8_t *byte)
{
   static int i;
   struct rarch_state *p_rarch = &rarch_st;

   if (!p_rarch->midi_drv_data || !p_rarch->midi_drv_input_enabled || !byte)
   {
#ifdef DEBUG
      if (!p_rarch->midi_drv_data)
         RARCH_ERR("[MIDI]: midi_driver_read called on uninitialized driver.\n");
      else if (!p_rarch->midi_drv_input_enabled)
         RARCH_ERR("[MIDI]: midi_driver_read called when input is disabled.\n");
      else
         RARCH_ERR("[MIDI]: midi_driver_read called with null pointer.\n");
#endif
      return false;
   }

   if (i == p_rarch->midi_drv_input_event.data_size)
   {
      p_rarch->midi_drv_input_event.data_size = MIDI_DRIVER_BUF_SIZE;
      if (!midi_drv->read(p_rarch->midi_drv_data,
               &p_rarch->midi_drv_input_event))
      {
         p_rarch->midi_drv_input_event.data_size = i;
         return false;
      }

      i = 0;

#ifdef DEBUG
      if (p_rarch->midi_drv_input_event.data_size == 1)
         RARCH_LOG("[MIDI]: In [0x%02X].\n",
               p_rarch->midi_drv_input_event.data[0]);
      else if (p_rarch->midi_drv_input_event.data_size == 2)
         RARCH_LOG("[MIDI]: In [0x%02X, 0x%02X].\n",
               p_rarch->midi_drv_input_event.data[0],
               p_rarch->midi_drv_input_event.data[1]);
      else if (p_rarch->midi_drv_input_event.data_size == 3)
         RARCH_LOG("[MIDI]: In [0x%02X, 0x%02X, 0x%02X].\n",
               p_rarch->midi_drv_input_event.data[0],
               p_rarch->midi_drv_input_event.data[1],
               p_rarch->midi_drv_input_event.data[2]);
      else
         RARCH_LOG("[MIDI]: In [0x%02X, ...], size %u.\n",
               p_rarch->midi_drv_input_event.data[0],
               p_rarch->midi_drv_input_event.data_size);
#endif
   }

   *byte = p_rarch->midi_drv_input_event.data[i++];

   return true;
}

static bool midi_driver_write(uint8_t byte, uint32_t delta_time)
{
   static int event_size;
   struct rarch_state *p_rarch = &rarch_st;

   if (!p_rarch->midi_drv_data || !p_rarch->midi_drv_output_enabled)
   {
#ifdef DEBUG
      if (!p_rarch->midi_drv_data)
         RARCH_ERR("[MIDI]: midi_driver_write called on uninitialized driver.\n");
      else
         RARCH_ERR("[MIDI]: midi_driver_write called when output is disabled.\n");
#endif
      return false;
   }

   if (byte >= 0x80)
   {
      if (p_rarch->midi_drv_output_event.data_size &&
            p_rarch->midi_drv_output_event.data[0] == 0xF0)
      {
         if (byte == 0xF7)
            event_size = (int)p_rarch->midi_drv_output_event.data_size + 1;
         else
         {
            if (!midi_drv->write(p_rarch->midi_drv_data,
                     &p_rarch->midi_drv_output_event))
               return false;

#ifdef DEBUG
            switch (p_rarch->midi_drv_output_event.data_size)
            {
               case 1:
                  RARCH_LOG("[MIDI]: Out [0x%02X].\n",
                        p_rarch->midi_drv_output_event.data[0]);
                  break;
               case 2:
                  RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X].\n",
                        p_rarch->midi_drv_output_event.data[0],
                        p_rarch->midi_drv_output_event.data[1]);
                  break;
               case 3:
                  RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X, 0x%02X].\n",
                        p_rarch->midi_drv_output_event.data[0],
                        p_rarch->midi_drv_output_event.data[1],
                        p_rarch->midi_drv_output_event.data[2]);
                  break;
               default:
                  RARCH_LOG("[MIDI]: Out [0x%02X, ...], size %u.\n",
                        p_rarch->midi_drv_output_event.data[0],
                        p_rarch->midi_drv_output_event.data_size);
                  break;
            }
#endif

            p_rarch->midi_drv_output_pending          = true;
            event_size                                = (int)midi_driver_get_event_size(byte);
            p_rarch->midi_drv_output_event.data_size  = 0;
            p_rarch->midi_drv_output_event.delta_time = 0;
         }
      }
      else
      {
         event_size                                   = (int)midi_driver_get_event_size(byte);
         p_rarch->midi_drv_output_event.data_size     = 0;
         p_rarch->midi_drv_output_event.delta_time    = 0;
      }
   }

   if (p_rarch->midi_drv_output_event.data_size < MIDI_DRIVER_BUF_SIZE)
   {
      p_rarch->midi_drv_output_event.data[p_rarch->midi_drv_output_event.data_size] = byte;
      ++p_rarch->midi_drv_output_event.data_size;
      p_rarch->midi_drv_output_event.delta_time += delta_time;
   }
   else
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: Output event dropped.\n");
#endif
      return false;
   }

   if (p_rarch->midi_drv_output_event.data_size == event_size)
   {
      if (!midi_drv->write(p_rarch->midi_drv_data,
               &p_rarch->midi_drv_output_event))
         return false;

#ifdef DEBUG
      switch (p_rarch->midi_drv_output_event.data_size)
      {
         case 1:
            RARCH_LOG("[MIDI]: Out [0x%02X].\n",
                  p_rarch->midi_drv_output_event.data[0]);
            break;
         case 2:
            RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X].\n",
                  p_rarch->midi_drv_output_event.data[0],
                  p_rarch->midi_drv_output_event.data[1]);
            break;
         case 3:
            RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X, 0x%02X].\n",
                  p_rarch->midi_drv_output_event.data[0],
                  p_rarch->midi_drv_output_event.data[1],
                  p_rarch->midi_drv_output_event.data[2]);
            break;
         default:
            RARCH_LOG("[MIDI]: Out [0x%02X, ...], size %u.\n",
                  p_rarch->midi_drv_output_event.data[0],
                  p_rarch->midi_drv_output_event.data_size);
            break;
      }
#endif

      p_rarch->midi_drv_output_pending             = true;
      p_rarch->midi_drv_output_event.data_size     = 0;
      p_rarch->midi_drv_output_event.delta_time    = 0;
   }

   return true;
}

static bool midi_driver_flush(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->midi_drv_data)
      return false;

   if (p_rarch->midi_drv_output_pending)
      p_rarch->midi_drv_output_pending = 
         !midi_drv->flush(p_rarch->midi_drv_data);

   return !p_rarch->midi_drv_output_pending;
}

size_t midi_driver_get_event_size(uint8_t status)
{
   static const uint8_t midi_drv_ev_sizes[128]                     =
   {
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      0, 2, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
   };

   if (status < 0x80)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_get_event_size called with invalid status.\n");
#endif
      return 0;
   }

   return midi_drv_ev_sizes[status - 0x80];
}

/* AUDIO */

static enum resampler_quality audio_driver_get_resampler_quality(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t     *settings    = p_rarch->configuration_settings;

   if (!settings)
      return RESAMPLER_QUALITY_DONTCARE;

   return (enum resampler_quality)settings->uints.audio_resampler_quality;
}

#ifdef HAVE_AUDIOMIXER
audio_mixer_stream_t *audio_driver_mixer_get_stream(unsigned i)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (i > (AUDIO_MIXER_MAX_SYSTEM_STREAMS-1))
      return NULL;
   return &p_rarch->audio_mixer_streams[i];
}

const char *audio_driver_mixer_get_stream_name(unsigned i)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (i > (AUDIO_MIXER_MAX_SYSTEM_STREAMS-1))
      return "N/A";
   if (!string_is_empty(p_rarch->audio_mixer_streams[i].name))
      return p_rarch->audio_mixer_streams[i].name;
   return "N/A";
}

static void audio_driver_mixer_deinit(void)
{
   unsigned i;
   struct rarch_state *p_rarch = &rarch_st;

   p_rarch->audio_mixer_active = false;

   for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
   {
      audio_driver_mixer_stop_stream(i);
      audio_driver_mixer_remove_stream(i);
   }

   audio_mixer_done();
}
#endif

/**
 * audio_compute_buffer_statistics:
 *
 * Computes audio buffer statistics.
 *
 **/
static bool audio_compute_buffer_statistics(audio_statistics_t *stats)
{
   unsigned i, low_water_size, high_water_size, avg, stddev;
   uint64_t accum                = 0;
   uint64_t accum_var            = 0;
   unsigned low_water_count      = 0;
   unsigned high_water_count     = 0;
   struct rarch_state   *p_rarch = &rarch_st;
   unsigned samples              = MIN(
         (unsigned)p_rarch->audio_driver_free_samples_count,
         AUDIO_BUFFER_FREE_SAMPLES_COUNT);

   if (!stats || samples < 3)
      return false;

   stats->samples                = (unsigned)
      p_rarch->audio_driver_free_samples_count;

#ifdef WARPUP
   /* uint64 to double not implemented, fair chance
    * signed int64 to double doesn't exist either */
   /* https://forums.libretro.com/t/unsupported-platform-help/13903/ */
   (void)stddev;
#elif defined(_MSC_VER) && _MSC_VER <= 1200
   /* FIXME: error C2520: conversion from unsigned __int64
    * to double not implemented, use signed __int64 */
   (void)stddev;
#else
   for (i = 1; i < samples; i++)
      accum += p_rarch->audio_driver_free_samples_buf[i];

   avg = (unsigned)accum / (samples - 1);

   for (i = 1; i < samples; i++)
   {
      int diff     = avg - p_rarch->audio_driver_free_samples_buf[i];
      accum_var   += diff * diff;
   }

   stddev                                = (unsigned)
      sqrt((double)accum_var / (samples - 2));

   stats->average_buffer_saturation      = (1.0f - (float)avg
         / p_rarch->audio_driver_buffer_size) * 100.0;
   stats->std_deviation_percentage       = ((float)stddev
         / p_rarch->audio_driver_buffer_size)  * 100.0;
#endif

   low_water_size  = (unsigned)(p_rarch->audio_driver_buffer_size * 3 / 4);
   high_water_size = (unsigned)(p_rarch->audio_driver_buffer_size     / 4);

   for (i = 1; i < samples; i++)
   {
      if (p_rarch->audio_driver_free_samples_buf[i] >= low_water_size)
         low_water_count++;
      else if (p_rarch->audio_driver_free_samples_buf[i] <= high_water_size)
         high_water_count++;
   }

   stats->close_to_underrun      = (100.0 * low_water_count)  / (samples - 1);
   stats->close_to_blocking      = (100.0 * high_water_count) / (samples - 1);

   return true;
}

static void report_audio_buffer_statistics(void)
{
   audio_statistics_t audio_stats = {0.0f};
   if (!audio_compute_buffer_statistics(&audio_stats))
      return;

#ifdef DEBUG
   RARCH_LOG("[Audio]: Average audio buffer saturation: %.2f %%,"
         " standard deviation (percentage points): %.2f %%.\n"
         "[Audio]: Amount of time spent close to underrun: %.2f %%."
         " Close to blocking: %.2f %%.\n",
         audio_stats.average_buffer_saturation,
         audio_stats.std_deviation_percentage,
         audio_stats.close_to_underrun,
         audio_stats.close_to_blocking);
#endif
}

/**
 * audio_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to audio driver at index. Can be NULL
 * if nothing found.
 **/
static const void *audio_driver_find_handle(int idx)
{
   const void *drv = audio_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * config_get_audio_driver_options:
 *
 * Get an enumerated list of all audio driver names, separated by '|'.
 *
 * Returns: string listing of all audio driver names, separated by '|'.
 **/
const char *config_get_audio_driver_options(void)
{
   return char_list_new_special(STRING_LIST_AUDIO_DRIVERS, NULL);
}

static void audio_driver_deinit_resampler(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->audio_driver_resampler && p_rarch->audio_driver_resampler_data)
      p_rarch->audio_driver_resampler->free(p_rarch->audio_driver_resampler_data);
   p_rarch->audio_driver_resampler      = NULL;
   p_rarch->audio_driver_resampler_data = NULL;
}


static bool audio_driver_deinit_internal(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   bool audio_enable           = settings->bools.audio_enable;

   if (p_rarch->current_audio && p_rarch->current_audio->free)
   {
      if (p_rarch->audio_driver_context_audio_data)
         p_rarch->current_audio->free(
               p_rarch->audio_driver_context_audio_data);
      p_rarch->audio_driver_context_audio_data = NULL;
   }

   if (p_rarch->audio_driver_output_samples_conv_buf)
      free(p_rarch->audio_driver_output_samples_conv_buf);
   p_rarch->audio_driver_output_samples_conv_buf     = NULL;

   p_rarch->audio_driver_data_ptr           = 0;

   if (p_rarch->audio_driver_rewind_buf)
      free(p_rarch->audio_driver_rewind_buf);
   p_rarch->audio_driver_rewind_buf         = NULL;

   p_rarch->audio_driver_rewind_size        = 0;

   if (!audio_enable)
   {
      p_rarch->audio_driver_active          = false;
      return false;
   }

   audio_driver_deinit_resampler();

   if (p_rarch->audio_driver_input_data)
      free(p_rarch->audio_driver_input_data);
   p_rarch->audio_driver_input_data         = NULL;

   if (p_rarch->audio_driver_output_samples_buf)
      free(p_rarch->audio_driver_output_samples_buf);
   p_rarch->audio_driver_output_samples_buf = NULL;

   audio_driver_dsp_filter_free();
   report_audio_buffer_statistics();

   return true;
}

static bool audio_driver_free_devices_list(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->current_audio || !p_rarch->current_audio->device_list_free
         || !p_rarch->audio_driver_context_audio_data)
      return false;
   p_rarch->current_audio->device_list_free(
         p_rarch->audio_driver_context_audio_data,
         p_rarch->audio_driver_devices_list);
   p_rarch->audio_driver_devices_list = NULL;
   return true;
}

static bool audio_driver_deinit(void)
{
#ifdef HAVE_AUDIOMIXER
   audio_driver_mixer_deinit();
#endif
   audio_driver_free_devices_list();

   if (!audio_driver_deinit_internal())
      return false;
   return true;
}

static bool audio_driver_find_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   struct rarch_state *p_rarch = &rarch_st;
   settings_t     *settings    = p_rarch->configuration_settings;

   drv.label                   = "audio_driver";
   drv.s                       = settings->arrays.audio_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i                           = (int)drv.len;

   if (i >= 0)
      p_rarch->current_audio = (const audio_driver_t*)
         audio_driver_find_handle(i);
   else
   {
      if (verbosity_is_enabled())
      {
         unsigned d;
         RARCH_ERR("Couldn't find any audio driver named \"%s\"\n",
               settings->arrays.audio_driver);
         RARCH_LOG_OUTPUT("Available audio drivers are:\n");
         for (d = 0; audio_driver_find_handle(d); d++)
            RARCH_LOG_OUTPUT("\t%s\n", audio_drivers[d]->ident);
         RARCH_WARN("Going to default to first audio driver...\n");
      }

      p_rarch->current_audio = (const audio_driver_t*)
         audio_driver_find_handle(0);

      if (!p_rarch->current_audio)
         retroarch_fail(1, "audio_driver_find()");
   }

   return true;
}

static bool audio_driver_init_internal(bool audio_cb_inited)
{
   unsigned new_rate       = 0;
   float   *aud_inp_data   = NULL;
   float  *samples_buf     = NULL;
   int16_t *rewind_buf     = NULL;
   size_t max_bufsamples   = AUDIO_CHUNK_SIZE_NONBLOCKING * 2;
   struct rarch_state 
      *p_rarch             = &rarch_st;
   settings_t *settings    = p_rarch->configuration_settings;
   bool audio_enable       = settings->bools.audio_enable;
   bool audio_sync         = settings->bools.audio_sync;
   bool audio_rate_control = settings->bools.audio_rate_control;
   float slowmotion_ratio  = settings->floats.slowmotion_ratio;
   /* Accomodate rewind since at some point we might have two full buffers. */
   size_t outsamples_max   = AUDIO_CHUNK_SIZE_NONBLOCKING * 2 * AUDIO_MAX_RATIO *
      slowmotion_ratio;
   int16_t *conv_buf       = (int16_t*)malloc(outsamples_max
         * sizeof(int16_t));

   convert_s16_to_float_init_simd();
   convert_float_to_s16_init_simd();

   /* Used for recording even if audio isn't enabled. */
   retro_assert(conv_buf != NULL);

   if (!conv_buf)
      goto error;

   p_rarch->audio_driver_output_samples_conv_buf = conv_buf;
   p_rarch->audio_driver_chunk_block_size        = AUDIO_CHUNK_SIZE_BLOCKING;
   p_rarch->audio_driver_chunk_nonblock_size     = AUDIO_CHUNK_SIZE_NONBLOCKING;
   p_rarch->audio_driver_chunk_size              = p_rarch->audio_driver_chunk_block_size;

   /* Needs to be able to hold full content of a full max_bufsamples
    * in addition to its own. */
   rewind_buf = (int16_t*)malloc(max_bufsamples * sizeof(int16_t));
   retro_assert(rewind_buf != NULL);

   if (!rewind_buf)
      goto error;

   p_rarch->audio_driver_rewind_buf              = rewind_buf;
   p_rarch->audio_driver_rewind_size             = max_bufsamples;

   if (!audio_enable)
   {
      p_rarch->audio_driver_active = false;
      return false;
   }

   audio_driver_find_driver();

   if (!p_rarch->current_audio || !p_rarch->current_audio->init)
   {
      RARCH_ERR("Failed to initialize audio driver. Will continue without audio.\n");
      p_rarch->audio_driver_active = false;
      return false;
   }

#ifdef HAVE_THREADS
   if (audio_cb_inited)
   {
      RARCH_LOG("[Audio]: Starting threaded audio driver ...\n");
      if (!audio_init_thread(
               &p_rarch->current_audio,
               &p_rarch->audio_driver_context_audio_data,
               *settings->arrays.audio_device
               ? settings->arrays.audio_device : NULL,
               settings->uints.audio_out_rate, &new_rate,
               settings->uints.audio_latency,
               settings->uints.audio_block_frames,
               p_rarch->current_audio))
      {
         RARCH_ERR("Cannot open threaded audio driver ... Exiting ...\n");
         retroarch_fail(1, "audio_driver_init_internal()");
      }
   }
   else
#endif
   {
      p_rarch->audio_driver_context_audio_data =
         p_rarch->current_audio->init(*settings->arrays.audio_device ?
               settings->arrays.audio_device : NULL,
               settings->uints.audio_out_rate,
               settings->uints.audio_latency,
               settings->uints.audio_block_frames,
               &new_rate);
   }

   if (new_rate != 0)
      configuration_set_int(settings, settings->uints.audio_out_rate, new_rate);

   if (!p_rarch->audio_driver_context_audio_data)
   {
      RARCH_ERR("Failed to initialize audio driver. Will continue without audio.\n");
      p_rarch->audio_driver_active    = false;
   }

   p_rarch->audio_driver_use_float    = false;
   if (     p_rarch->audio_driver_active
         && p_rarch->current_audio->use_float(
            p_rarch->audio_driver_context_audio_data))
      p_rarch->audio_driver_use_float = true;

   if (!audio_sync && p_rarch->audio_driver_active)
   {
      if (p_rarch->audio_driver_active && 
            p_rarch->audio_driver_context_audio_data)
         p_rarch->current_audio->set_nonblock_state(
               p_rarch->audio_driver_context_audio_data, true);

      p_rarch->audio_driver_chunk_size = 
         p_rarch->audio_driver_chunk_nonblock_size;
   }

   if (p_rarch->audio_driver_input <= 0.0f)
   {
      /* Should never happen. */
      RARCH_WARN("Input rate is invalid (%.3f Hz)."
            " Using output rate (%u Hz).\n",
            p_rarch->audio_driver_input, settings->uints.audio_out_rate);

      p_rarch->audio_driver_input = settings->uints.audio_out_rate;
   }

   p_rarch->audio_source_ratio_original   = 
      p_rarch->audio_source_ratio_current =
      (double)settings->uints.audio_out_rate / p_rarch->audio_driver_input;

   if (!retro_resampler_realloc(
            &p_rarch->audio_driver_resampler_data,
            &p_rarch->audio_driver_resampler,
            settings->arrays.audio_resampler,
            audio_driver_get_resampler_quality(),
            p_rarch->audio_source_ratio_original))
   {
      RARCH_ERR("Failed to initialize resampler \"%s\".\n",
            settings->arrays.audio_resampler);
      p_rarch->audio_driver_active = false;
   }

   aud_inp_data = (float*)malloc(max_bufsamples * sizeof(float));
   retro_assert(aud_inp_data != NULL);

   if (!aud_inp_data)
      goto error;

   p_rarch->audio_driver_input_data = aud_inp_data;
   p_rarch->audio_driver_data_ptr   = 0;

   retro_assert(settings->uints.audio_out_rate <
         p_rarch->audio_driver_input * AUDIO_MAX_RATIO);

   samples_buf = (float*)malloc(outsamples_max * sizeof(float));

   retro_assert(samples_buf != NULL);

   if (!samples_buf)
      goto error;

   p_rarch->audio_driver_output_samples_buf = (float*)samples_buf;
   p_rarch->audio_driver_control            = false;

   if (
         !audio_cb_inited
         && p_rarch->audio_driver_active
         && audio_rate_control
         )
   {
      /* Audio rate control requires write_avail
       * and buffer_size to be implemented. */
      if (p_rarch->current_audio->buffer_size)
      {
         p_rarch->audio_driver_buffer_size =
            p_rarch->current_audio->buffer_size(
                  p_rarch->audio_driver_context_audio_data);
         p_rarch->audio_driver_control     = true;
      }
      else
         RARCH_WARN("Audio rate control was desired, but driver does not support needed features.\n");
   }

   command_event(CMD_EVENT_DSP_FILTER_INIT, NULL);

   p_rarch->audio_driver_free_samples_count = 0;

#ifdef HAVE_AUDIOMIXER
   audio_mixer_init(settings->uints.audio_out_rate);
#endif

   /* Threaded driver is initially stopped. */
   if (
         p_rarch->audio_driver_active
         && audio_cb_inited
         )
      audio_driver_start(false);

   return true;

error:
   return audio_driver_deinit();
}

/**
 * audio_driver_flush:
 * @data                 : pointer to audio buffer.
 * @right                : amount of samples to write.
 *
 * Writes audio samples to audio driver. Will first
 * perform DSP processing (if enabled) and resampling.
 **/
static void audio_driver_flush(const int16_t *data, size_t samples,
      bool is_slowmotion, bool is_fastmotion)
{
   struct resampler_data src_data;
   struct rarch_state       *p_rarch = &rarch_st;
   settings_t       *settings        = p_rarch->configuration_settings;
   float slowmotion_ratio            = settings->floats.slowmotion_ratio;
   bool audio_fastforward_mute       = settings->bools.audio_fastforward_mute;
   float audio_volume_gain           = (p_rarch->audio_driver_mute_enable ||
         (audio_fastforward_mute && is_fastmotion)) ?
               0.0f : p_rarch->audio_driver_volume_gain;

   src_data.data_out                 = NULL;
   src_data.output_frames            = 0;

   convert_s16_to_float(p_rarch->audio_driver_input_data, data, samples,
         audio_volume_gain);

   src_data.data_in                  = p_rarch->audio_driver_input_data;
   src_data.input_frames             = samples >> 1;

   if (p_rarch->audio_driver_dsp)
   {
      struct retro_dsp_data dsp_data;

      dsp_data.input                 = NULL;
      dsp_data.input_frames          = 0;
      dsp_data.output                = NULL;
      dsp_data.output_frames         = 0;

      dsp_data.input                 = p_rarch->audio_driver_input_data;
      dsp_data.input_frames          = (unsigned)(samples >> 1);

      retro_dsp_filter_process(p_rarch->audio_driver_dsp, &dsp_data);

      if (dsp_data.output)
      {
         src_data.data_in            = dsp_data.output;
         src_data.input_frames       = dsp_data.output_frames;
      }
   }

   src_data.data_out                 = p_rarch->audio_driver_output_samples_buf;

   if (p_rarch->audio_driver_control)
   {
      /* Readjust the audio input rate. */
      int      half_size           = 
         (int)(p_rarch->audio_driver_buffer_size / 2);
      int      avail               =
         (int)p_rarch->current_audio->write_avail(
               p_rarch->audio_driver_context_audio_data);
      int      delta_mid           = avail - half_size;
      double   direction           = (double)delta_mid / half_size;
      double   adjust              = 1.0 + 
         p_rarch->audio_driver_rate_control_delta * direction;
      unsigned write_idx           = 
         p_rarch->audio_driver_free_samples_count++ &
         (AUDIO_BUFFER_FREE_SAMPLES_COUNT - 1);

      p_rarch->audio_driver_free_samples_buf
         [write_idx]                        = avail;
      p_rarch->audio_source_ratio_current   =
         p_rarch->audio_source_ratio_original * adjust;

#if 0
      if (verbosity_is_enabled())
      {
         RARCH_LOG_OUTPUT("[Audio]: Audio buffer is %u%% full\n",
               (unsigned)(100 - (avail * 100) / 
                  p_rarch->audio_driver_buffer_size));
         RARCH_LOG_OUTPUT("[Audio]: New rate: %lf, Orig rate: %lf\n",
               p_rarch->audio_source_ratio_current,
               p_rarch->audio_source_ratio_original);
      }
#endif
   }

   src_data.ratio           = p_rarch->audio_source_ratio_current;

   if (is_slowmotion)
      src_data.ratio       *= slowmotion_ratio;

   /* Note: Ideally we would divide by the user-configured
    * 'fastforward_ratio' when fast forward is enabled,
    * but in practice this doesn't work:
    * - 'fastforward_ratio' is only a limit. If the host
    *   cannot push frames fast enough, the actual ratio
    *   will be lower - and crackling will ensue
    * - Most of the time 'fastforward_ratio' will be
    *   zero (unlimited)
    * So what we would need to do is measure the time since
    * the last audio flush operation, and calculate a 'real'
    * fast-forward ratio - but this doesn't work either.
    * The measurement is inaccurate and the frame-by-frame
    * fluctuations are too large, so crackling is unavoidable.
    * Since it's going to crackle anyway, there's no point
    * trying to do anything. Just leave the ratio as-is,
    * and hope for the best... */

   p_rarch->audio_driver_resampler->process(
         p_rarch->audio_driver_resampler_data, &src_data);

#ifdef HAVE_AUDIOMIXER
   if (p_rarch->audio_mixer_active)
   {
      bool override                       = true;
      float mixer_gain                    = 0.0f;
      bool audio_driver_mixer_mute_enable = 
         p_rarch->audio_driver_mixer_mute_enable;

      if (!audio_driver_mixer_mute_enable)
      {
         if (p_rarch->audio_driver_mixer_volume_gain == 1.0f)
            override                      = false; 
         mixer_gain                       = 
            p_rarch->audio_driver_mixer_volume_gain;
      }
      audio_mixer_mix(
            p_rarch->audio_driver_output_samples_buf,
            src_data.output_frames, mixer_gain, override);
   }
#endif

   {
      const void *output_data = p_rarch->audio_driver_output_samples_buf;
      unsigned output_frames  = (unsigned)src_data.output_frames;

      if (p_rarch->audio_driver_use_float)
         output_frames  *= sizeof(float);
      else
      {
         convert_float_to_s16(p_rarch->audio_driver_output_samples_conv_buf,
               (const float*)output_data, output_frames * 2);

         output_data     = p_rarch->audio_driver_output_samples_conv_buf;
         output_frames  *= sizeof(int16_t);
      }

      if (p_rarch->current_audio->write(
               p_rarch->audio_driver_context_audio_data,
               output_data, output_frames * 2) < 0)
         p_rarch->audio_driver_active = false;
   }
}

/**
 * audio_driver_sample:
 * @left                 : value of the left audio channel.
 * @right                : value of the right audio channel.
 *
 * Audio sample render callback function.
 **/
static void audio_driver_sample(int16_t left, int16_t right)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->audio_suspended)
      return;

   p_rarch->audio_driver_output_samples_conv_buf[p_rarch->audio_driver_data_ptr++] = left;
   p_rarch->audio_driver_output_samples_conv_buf[p_rarch->audio_driver_data_ptr++] = right;

   if (p_rarch->audio_driver_data_ptr < p_rarch->audio_driver_chunk_size)
      return;

   if (  p_rarch->recording_data     && 
         p_rarch->recording_driver   &&
         p_rarch->recording_driver->push_audio)
   {
      struct record_audio_data ffemu_data;

      ffemu_data.data                    = p_rarch->audio_driver_output_samples_conv_buf;
      ffemu_data.frames                  = p_rarch->audio_driver_data_ptr / 2;

      p_rarch->recording_driver->push_audio(p_rarch->recording_data, &ffemu_data);
   }

   if (!(p_rarch->runloop_paused           ||
		   !p_rarch->audio_driver_active     ||
		   !p_rarch->audio_driver_input_data ||
		   !p_rarch->audio_driver_output_samples_buf))
      audio_driver_flush(
            p_rarch->audio_driver_output_samples_conv_buf,
            p_rarch->audio_driver_data_ptr,
            p_rarch->runloop_slowmotion,
            p_rarch->runloop_fastmotion);

   p_rarch->audio_driver_data_ptr = 0;
}

#ifdef HAVE_MENU
static void audio_driver_menu_sample(void)
{
   static int16_t samples_buf[1024]       = {0};
   struct rarch_state          *p_rarch   = &rarch_st;
   struct retro_system_av_info *av_info   = &p_rarch->video_driver_av_info;
   const struct retro_system_timing *info =
      (const struct retro_system_timing*)&av_info->timing;
   unsigned sample_count                  = (info->sample_rate / info->fps) * 2;
   bool check_flush                       = !(
         p_rarch->runloop_paused           ||
         !p_rarch->audio_driver_active     ||
         !p_rarch->audio_driver_input_data ||
         !p_rarch->audio_driver_output_samples_buf);

   while (sample_count > 1024)
   {
      if (  p_rarch->recording_data   && 
            p_rarch->recording_driver &&
            p_rarch->recording_driver->push_audio)
      {
         struct record_audio_data ffemu_data;

         ffemu_data.data                    = samples_buf;
         ffemu_data.frames                  = 1024 / 2;

         p_rarch->recording_driver->push_audio(p_rarch->recording_data, &ffemu_data);
      }
      if (check_flush)
         audio_driver_flush(samples_buf, 1024,
               p_rarch->runloop_slowmotion,
               p_rarch->runloop_fastmotion);
      sample_count -= 1024;
   }
   if (  p_rarch->recording_data   && 
         p_rarch->recording_driver &&
         p_rarch->recording_driver->push_audio)
   {
      struct record_audio_data ffemu_data;

      ffemu_data.data                    = samples_buf;
      ffemu_data.frames                  = sample_count / 2;

      p_rarch->recording_driver->push_audio(p_rarch->recording_data, &ffemu_data);
   }
   if (check_flush)
      audio_driver_flush(samples_buf, sample_count,
            p_rarch->runloop_slowmotion,
            p_rarch->runloop_fastmotion);
}
#endif

/**
 * audio_driver_sample_batch:
 * @data                 : pointer to audio buffer.
 * @frames               : amount of audio frames to push.
 *
 * Batched audio sample render callback function.
 *
 * Returns: amount of frames sampled. Will be equal to @frames
 * unless @frames exceeds (AUDIO_CHUNK_SIZE_NONBLOCKING / 2).
 **/
static size_t audio_driver_sample_batch(const int16_t *data, size_t frames)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (frames > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
      frames = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;

   if (p_rarch->audio_suspended)
      return frames;

   if (  p_rarch->recording_data   &&
         p_rarch->recording_driver &&
         p_rarch->recording_driver->push_audio)
   {
      struct record_audio_data ffemu_data;

      ffemu_data.data                    = data;
      ffemu_data.frames                  = (frames << 1) / 2;

      p_rarch->recording_driver->push_audio(p_rarch->recording_data, &ffemu_data);
   }

   if (!(
         p_rarch->runloop_paused           ||
         !p_rarch->audio_driver_active     ||
         !p_rarch->audio_driver_input_data ||
         !p_rarch->audio_driver_output_samples_buf))
      audio_driver_flush(data, frames << 1,
            p_rarch->runloop_slowmotion,
            p_rarch->runloop_fastmotion);

   return frames;
}

/**
 * audio_driver_sample_rewind:
 * @left                 : value of the left audio channel.
 * @right                : value of the right audio channel.
 *
 * Audio sample render callback function (rewind version).
 * This callback function will be used instead of
 * audio_driver_sample when rewinding is activated.
 **/
static void audio_driver_sample_rewind(int16_t left, int16_t right)
{
   struct rarch_state *p_rarch   = &rarch_st;
   if (p_rarch->audio_driver_rewind_ptr == 0)
      return;

   p_rarch->audio_driver_rewind_buf[--p_rarch->audio_driver_rewind_ptr] = right;
   p_rarch->audio_driver_rewind_buf[--p_rarch->audio_driver_rewind_ptr] = left;
}

/**
 * audio_driver_sample_batch_rewind:
 * @data                 : pointer to audio buffer.
 * @frames               : amount of audio frames to push.
 *
 * Batched audio sample render callback function (rewind version).
 *
 * This callback function will be used instead of
 * audio_driver_sample_batch when rewinding is activated.
 *
 * Returns: amount of frames sampled. Will be equal to @frames
 * unless @frames exceeds (AUDIO_CHUNK_SIZE_NONBLOCKING / 2).
 **/
static size_t audio_driver_sample_batch_rewind(
      const int16_t *data, size_t frames)
{
   size_t i;
   struct rarch_state *p_rarch   = &rarch_st;
   size_t              samples   = frames << 1;

   for (i = 0; i < samples; i++)
   {
      if (p_rarch->audio_driver_rewind_ptr > 0)
         p_rarch->audio_driver_rewind_buf[--p_rarch->audio_driver_rewind_ptr] = data[i];
   }

   return frames;
}

void audio_driver_dsp_filter_free(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->audio_driver_dsp)
      retro_dsp_filter_free(p_rarch->audio_driver_dsp);
   p_rarch->audio_driver_dsp = NULL;
}

bool audio_driver_dsp_filter_init(const char *device)
{
   struct rarch_state *p_rarch       = &rarch_st;
   struct string_list *plugs         = NULL;
#if defined(HAVE_DYLIB) && !defined(HAVE_FILTERS_BUILTIN)
   char *basedir                     = (char*)
      calloc(PATH_MAX_LENGTH, sizeof(*basedir));
   char *ext_name                    = (char*)
      calloc(PATH_MAX_LENGTH, sizeof(*ext_name));
   size_t str_size                   = PATH_MAX_LENGTH * sizeof(char);

   fill_pathname_basedir(basedir, device, str_size);

   if (!frontend_driver_get_core_extension(ext_name, str_size))
   {
      free(ext_name);
      free(basedir);
      return false;
   }

   plugs = dir_list_new(basedir, ext_name, false, true, false, false);
   free(ext_name);
   free(basedir);
   if (!plugs)
      return false;
#endif
   p_rarch->audio_driver_dsp = retro_dsp_filter_new(
         device, plugs, p_rarch->audio_driver_input);
   if (!p_rarch->audio_driver_dsp)
      return false;

   return true;
}

void audio_driver_set_buffer_size(size_t bufsize)
{
   struct rarch_state *p_rarch       = &rarch_st;
   p_rarch->audio_driver_buffer_size = bufsize;
}

static void audio_driver_monitor_adjust_system_rates(void)
{
   float timing_skew;
   struct rarch_state            *p_rarch = &rarch_st;
   settings_t *settings                   = p_rarch->configuration_settings;
   bool vrr_runloop_enable                = settings->bools.vrr_runloop_enable;
   const float target_video_sync_rate     =
      settings->floats.video_refresh_rate / settings->uints.video_swap_interval;
   float max_timing_skew                  = settings->floats.audio_max_timing_skew;
   struct retro_system_av_info *av_info   = &p_rarch->video_driver_av_info;
   const struct retro_system_timing *info =
      (const struct retro_system_timing*)&av_info->timing;

   if (info->sample_rate <= 0.0)
      return;

   timing_skew                            = 
      fabs(1.0f - info->fps / target_video_sync_rate);
   p_rarch->audio_driver_input            = info->sample_rate;

   if (timing_skew <= max_timing_skew && !vrr_runloop_enable)
      p_rarch->audio_driver_input        *= target_video_sync_rate / info->fps;

   RARCH_LOG("[Audio]: Set audio input rate to: %.2f Hz.\n",
         p_rarch->audio_driver_input);
}

void audio_driver_setup_rewind(void)
{
   unsigned i;
   struct rarch_state *p_rarch      = &rarch_st;

   /* Push audio ready to be played. */
   p_rarch->audio_driver_rewind_ptr = p_rarch->audio_driver_rewind_size;

   for (i = 0; i < p_rarch->audio_driver_data_ptr; i += 2)
   {
      if (p_rarch->audio_driver_rewind_ptr > 0)
         p_rarch->audio_driver_rewind_buf[
            --p_rarch->audio_driver_rewind_ptr] =
            p_rarch->audio_driver_output_samples_conv_buf[i + 1];

      if (p_rarch->audio_driver_rewind_ptr > 0)
         p_rarch->audio_driver_rewind_buf[--p_rarch->audio_driver_rewind_ptr] =
            p_rarch->audio_driver_output_samples_conv_buf[i + 0];
   }

   p_rarch->audio_driver_data_ptr = 0;
}


bool audio_driver_get_devices_list(void **data)
{
   struct rarch_state *p_rarch = &rarch_st;
   struct string_list**ptr     = (struct string_list**)data;
   if (!ptr)
      return false;
   *ptr = p_rarch->audio_driver_devices_list;
   return true;
}

#ifdef HAVE_AUDIOMIXER
bool audio_driver_mixer_extension_supported(const char *ext)
{
   union string_list_elem_attr attr;
   unsigned i;
   bool ret                      = false;
   struct string_list *str_list  = string_list_new();

   attr.i = 0;

#ifdef HAVE_STB_VORBIS
   string_list_append(str_list, "ogg", attr);
#endif
#ifdef HAVE_IBXM
   string_list_append(str_list, "mod", attr);
   string_list_append(str_list, "s3m", attr);
   string_list_append(str_list, "xm", attr);
#endif
#ifdef HAVE_DR_FLAC
   string_list_append(str_list, "flac", attr);
#endif
#ifdef HAVE_DR_MP3
   string_list_append(str_list, "mp3", attr);
#endif
   string_list_append(str_list, "wav", attr);

   for (i = 0; i < str_list->size; i++)
   {
      const char *str_ext = str_list->elems[i].data;
      if (string_is_equal_noncase(str_ext, ext))
      {
         ret = true;
         break;
      }
   }

   string_list_free(str_list);

   return ret;
}

static int audio_mixer_find_index(audio_mixer_sound_t *sound)
{
   unsigned i;
   struct rarch_state *p_rarch = &rarch_st;

   for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
   {
      audio_mixer_sound_t *handle = p_rarch->audio_mixer_streams[i].handle;
      if (handle == sound)
         return i;
   }
   return -1;
}

static void audio_mixer_play_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason)
{
   int                     idx = audio_mixer_find_index(sound);
   struct rarch_state *p_rarch = &rarch_st;

   switch (reason)
   {
      case AUDIO_MIXER_SOUND_FINISHED:
         audio_mixer_destroy(sound);

         if (idx >= 0)
         {
            unsigned i = (unsigned)idx;

            if (!string_is_empty(p_rarch->audio_mixer_streams[i].name))
               free(p_rarch->audio_mixer_streams[i].name);

            p_rarch->audio_mixer_streams[i].name    = NULL;
            p_rarch->audio_mixer_streams[i].state   = AUDIO_STREAM_STATE_NONE;
            p_rarch->audio_mixer_streams[i].volume  = 0.0f;
            p_rarch->audio_mixer_streams[i].buf     = NULL;
            p_rarch->audio_mixer_streams[i].stop_cb = NULL;
            p_rarch->audio_mixer_streams[i].handle  = NULL;
            p_rarch->audio_mixer_streams[i].voice   = NULL;
         }
         break;
      case AUDIO_MIXER_SOUND_STOPPED:
         break;
      case AUDIO_MIXER_SOUND_REPEATED:
         break;
   }
}

static void audio_mixer_menu_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason)
{
   int                     idx = audio_mixer_find_index(sound);
   struct rarch_state *p_rarch = &rarch_st;

   switch (reason)
   {
      case AUDIO_MIXER_SOUND_FINISHED:
         if (idx >= 0)
         {
            unsigned i                              = (unsigned)idx;
            p_rarch->audio_mixer_streams[i].state   = AUDIO_STREAM_STATE_STOPPED;
            p_rarch->audio_mixer_streams[i].volume  = 0.0f;
         }
         break;
      case AUDIO_MIXER_SOUND_STOPPED:
         break;
      case AUDIO_MIXER_SOUND_REPEATED:
         break;
   }
}

static void audio_mixer_play_stop_sequential_cb(
      audio_mixer_sound_t *sound, unsigned reason)
{
   int                     idx = audio_mixer_find_index(sound);
   struct rarch_state *p_rarch = &rarch_st;

   switch (reason)
   {
      case AUDIO_MIXER_SOUND_FINISHED:
         audio_mixer_destroy(sound);

         if (idx >= 0)
         {
            unsigned i = (unsigned)idx;

            if (!string_is_empty(p_rarch->audio_mixer_streams[i].name))
               free(p_rarch->audio_mixer_streams[i].name);

            if (i < AUDIO_MIXER_MAX_STREAMS)
               p_rarch->audio_mixer_streams[i].stream_type = AUDIO_STREAM_TYPE_USER;
            else
               p_rarch->audio_mixer_streams[i].stream_type = AUDIO_STREAM_TYPE_SYSTEM;

            p_rarch->audio_mixer_streams[i].name           = NULL;
            p_rarch->audio_mixer_streams[i].state          = AUDIO_STREAM_STATE_NONE;
            p_rarch->audio_mixer_streams[i].volume         = 0.0f;
            p_rarch->audio_mixer_streams[i].buf            = NULL;
            p_rarch->audio_mixer_streams[i].stop_cb        = NULL;
            p_rarch->audio_mixer_streams[i].handle         = NULL;
            p_rarch->audio_mixer_streams[i].voice          = NULL;

            i++;

            for (; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
            {
               if (p_rarch->audio_mixer_streams[i].state 
                     == AUDIO_STREAM_STATE_STOPPED)
               {
                  audio_driver_mixer_play_stream_sequential(i);
                  break;
               }
            }
         }
         break;
      case AUDIO_MIXER_SOUND_STOPPED:
         break;
      case AUDIO_MIXER_SOUND_REPEATED:
         break;
   }
}

static bool audio_driver_mixer_get_free_stream_slot(unsigned *id, enum audio_mixer_stream_type type)
{
   unsigned                  i = AUDIO_MIXER_MAX_STREAMS;
   unsigned              count = AUDIO_MIXER_MAX_SYSTEM_STREAMS;
   struct rarch_state *p_rarch = &rarch_st;

   if (type == AUDIO_STREAM_TYPE_USER)
   {
      i     = 0;
      count = AUDIO_MIXER_MAX_STREAMS;
   }

   for (; i < count; i++)
   {
      if (p_rarch->audio_mixer_streams[i].state == AUDIO_STREAM_STATE_NONE)
      {
         *id = i;
         return true;
      }
   }

   return false;
}

bool audio_driver_mixer_add_stream(audio_mixer_stream_params_t *params)
{
   struct rarch_state *p_rarch   = &rarch_st;
   unsigned free_slot            = 0;
   audio_mixer_voice_t *voice    = NULL;
   audio_mixer_sound_t *handle   = NULL;
   audio_mixer_stop_cb_t stop_cb = audio_mixer_play_stop_cb;
   bool looped                   = false;
   void *buf                     = NULL;

   if (params->stream_type == AUDIO_STREAM_TYPE_NONE)
      return false;

   switch (params->slot_selection_type)
   {
      case AUDIO_MIXER_SLOT_SELECTION_MANUAL:
         free_slot = params->slot_selection_idx;
         break;
      case AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC:
      default:
         if (!audio_driver_mixer_get_free_stream_slot(
                  &free_slot, params->stream_type))
            return false;
         break;
   }

   if (params->state == AUDIO_STREAM_STATE_NONE)
      return false;

   buf = malloc(params->bufsize);

   if (!buf)
      return false;

   memcpy(buf, params->buf, params->bufsize);

   switch (params->type)
   {
      case AUDIO_MIXER_TYPE_WAV:
         handle = audio_mixer_load_wav(buf, (int32_t)params->bufsize);
         break;
      case AUDIO_MIXER_TYPE_OGG:
         handle = audio_mixer_load_ogg(buf, (int32_t)params->bufsize);
         break;
      case AUDIO_MIXER_TYPE_MOD:
         handle = audio_mixer_load_mod(buf, (int32_t)params->bufsize);
         break;
      case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_DR_FLAC
         handle = audio_mixer_load_flac(buf, (int32_t)params->bufsize);
#endif
         break;
      case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_DR_MP3
         handle = audio_mixer_load_mp3(buf, (int32_t)params->bufsize);
#endif
         break;
      case AUDIO_MIXER_TYPE_NONE:
         break;
   }

   if (!handle)
   {
      free(buf);
      return false;
   }

   switch (params->state)
   {
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
         looped = true;
         voice = audio_mixer_play(handle, looped, params->volume, stop_cb);
         break;
      case AUDIO_STREAM_STATE_PLAYING:
         voice = audio_mixer_play(handle, looped, params->volume, stop_cb);
         break;
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         stop_cb = audio_mixer_play_stop_sequential_cb;
         voice = audio_mixer_play(handle, looped, params->volume, stop_cb);
         break;
      default:
         break;
   }

   p_rarch->audio_mixer_active                         = true;

   p_rarch->audio_mixer_streams[free_slot].name        = 
      !string_is_empty(params->basename) ? strdup(params->basename) : NULL;
   p_rarch->audio_mixer_streams[free_slot].buf         = buf;
   p_rarch->audio_mixer_streams[free_slot].handle      = handle;
   p_rarch->audio_mixer_streams[free_slot].voice       = voice;
   p_rarch->audio_mixer_streams[free_slot].stream_type = params->stream_type;
   p_rarch->audio_mixer_streams[free_slot].type        = params->type;
   p_rarch->audio_mixer_streams[free_slot].state       = params->state;
   p_rarch->audio_mixer_streams[free_slot].volume      = params->volume;
   p_rarch->audio_mixer_streams[free_slot].stop_cb     = stop_cb;

   return true;
}

enum audio_mixer_state audio_driver_mixer_get_stream_state(unsigned i)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return AUDIO_STREAM_STATE_NONE;

   return p_rarch->audio_mixer_streams[i].state;
}

static void audio_driver_mixer_play_stream_internal(unsigned i, unsigned type)
{
   bool set_state              = false;
   struct rarch_state *p_rarch = &rarch_st;

   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   switch (p_rarch->audio_mixer_streams[i].state)
   {
      case AUDIO_STREAM_STATE_STOPPED:
         p_rarch->audio_mixer_streams[i].voice = 
            audio_mixer_play(p_rarch->audio_mixer_streams[i].handle,
               (type == AUDIO_STREAM_STATE_PLAYING_LOOPED) ? true : false,
               1.0f, p_rarch->audio_mixer_streams[i].stop_cb);
         set_state = true;
         break;
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }

   if (set_state)
      p_rarch->audio_mixer_streams[i].state   = (enum audio_mixer_state)type;
}

static void audio_driver_load_menu_bgm_callback(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   bool contentless = false;
   bool is_inited   = false;

   content_get_status(&contentless, &is_inited);

   if (!is_inited)
      audio_driver_mixer_play_menu_sound_looped(AUDIO_MIXER_SYSTEM_SLOT_BGM);
}

void audio_driver_load_menu_sounds(void)
{
   struct rarch_state *p_rarch       = &rarch_st;
   settings_t *settings              = p_rarch->configuration_settings;
   const char *dir_assets            = settings->paths.directory_assets;
   bool audio_enable_menu_ok         = settings->bools.audio_enable_menu_ok;
   bool audio_enable_menu_cancel     = settings->bools.audio_enable_menu_cancel;
   bool audio_enable_menu_notice     = settings->bools.audio_enable_menu_notice;
   bool audio_enable_menu_bgm        = settings->bools.audio_enable_menu_bgm;
   const char *path_ok               = NULL;
   const char *path_cancel           = NULL;
   const char *path_notice           = NULL;
   const char *path_bgm              = NULL;
   struct string_list *list          = NULL;
   struct string_list *list_fallback = NULL;
   unsigned i                        = 0;
   char *sounds_path                 = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));
   char *sounds_fallback_path        = (char*)
      malloc(PATH_MAX_LENGTH * sizeof(char));

   sounds_path[0] = sounds_fallback_path[0] = '\0';

   fill_pathname_join(
         sounds_fallback_path,
         dir_assets,
         "sounds",
         PATH_MAX_LENGTH * sizeof(char));

   fill_pathname_application_special(
         sounds_path,
         PATH_MAX_LENGTH * sizeof(char),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_SOUNDS);

   list = dir_list_new(sounds_path, MENU_SOUND_FORMATS, false, false, false, false);
   list_fallback = dir_list_new(sounds_fallback_path, MENU_SOUND_FORMATS, false, false, false, false);

   if (!list)
   {
      list = list_fallback;
      list_fallback = NULL;
   }

   if (!list || list->size == 0)
      goto end;

   if (list_fallback && list_fallback->size > 0)
   {
      for (i = 0; i < list_fallback->size; i++)
      {
         if (list->size == 0 || !string_list_find_elem(list, list_fallback->elems[i].data))
         {
            union string_list_elem_attr attr = {0};
            string_list_append(list, list_fallback->elems[i].data, attr);
         }
      }
   }

   for (i = 0; i < list->size; i++)
   {
      const char *path = list->elems[i].data;
      const char *ext = path_get_extension(path);
      char basename_noext[PATH_MAX_LENGTH];

      basename_noext[0] = '\0';

      fill_pathname_base_noext(basename_noext, path, sizeof(basename_noext));

      if (audio_driver_mixer_extension_supported(ext))
      {
         if (string_is_equal_noncase(basename_noext, "ok"))
            path_ok = path;
         if (string_is_equal_noncase(basename_noext, "cancel"))
            path_cancel = path;
         if (string_is_equal_noncase(basename_noext, "notice"))
            path_notice = path;
         if (string_is_equal_noncase(basename_noext, "bgm"))
            path_bgm = path;
      }
   }

   if (path_ok && audio_enable_menu_ok)
      task_push_audio_mixer_load(path_ok, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_OK);
   if (path_cancel && audio_enable_menu_cancel)
      task_push_audio_mixer_load(path_cancel, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_CANCEL);
   if (path_notice && audio_enable_menu_notice)
      task_push_audio_mixer_load(path_notice, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_NOTICE);
   if (path_bgm && audio_enable_menu_bgm)
      task_push_audio_mixer_load(path_bgm, audio_driver_load_menu_bgm_callback, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_BGM);

end:
   if (list)
      string_list_free(list);
   if (list_fallback)
      string_list_free(list_fallback);
   if (sounds_path)
      free(sounds_path);
   if (sounds_fallback_path)
      free(sounds_fallback_path);
}

void audio_driver_mixer_play_stream(unsigned i)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->audio_mixer_streams[i].stop_cb = audio_mixer_play_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING);
}

void audio_driver_mixer_play_menu_sound_looped(unsigned i)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->audio_mixer_streams[i].stop_cb = audio_mixer_menu_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING_LOOPED);
}

void audio_driver_mixer_play_menu_sound(unsigned i)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->audio_mixer_streams[i].stop_cb = audio_mixer_menu_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING);
}

void audio_driver_mixer_play_stream_looped(unsigned i)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->audio_mixer_streams[i].stop_cb = audio_mixer_play_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING_LOOPED);
}

void audio_driver_mixer_play_stream_sequential(unsigned i)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->audio_mixer_streams[i].stop_cb = audio_mixer_play_stop_sequential_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL);
}

float audio_driver_mixer_get_stream_volume(unsigned i)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return 0.0f;

   return p_rarch->audio_mixer_streams[i].volume;
}

void audio_driver_mixer_set_stream_volume(unsigned i, float vol)
{
   audio_mixer_voice_t *voice             = NULL;
   struct rarch_state *p_rarch            = &rarch_st;

   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   p_rarch->audio_mixer_streams[i].volume = vol;

   voice                                  = 
      p_rarch->audio_mixer_streams[i].voice;

   if (voice)
      audio_mixer_voice_set_volume(voice, db_to_gain(vol));
}

void audio_driver_mixer_stop_stream(unsigned i)
{
   bool set_state                         = false;
   struct rarch_state *p_rarch            = &rarch_st;

   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   switch (p_rarch->audio_mixer_streams[i].state)
   {
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         set_state = true;
         break;
      case AUDIO_STREAM_STATE_STOPPED:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }

   if (set_state)
   {
      audio_mixer_voice_t *voice     = p_rarch->audio_mixer_streams[i].voice;

      if (voice)
         audio_mixer_stop(voice);
      p_rarch->audio_mixer_streams[i].state   = AUDIO_STREAM_STATE_STOPPED;
      p_rarch->audio_mixer_streams[i].volume  = 1.0f;
   }
}

void audio_driver_mixer_remove_stream(unsigned i)
{
   bool destroy                = false;
   struct rarch_state *p_rarch = &rarch_st;

   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   switch (p_rarch->audio_mixer_streams[i].state)
   {
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         audio_driver_mixer_stop_stream(i);
         destroy = true;
         break;
      case AUDIO_STREAM_STATE_STOPPED:
         destroy = true;
         break;
      case AUDIO_STREAM_STATE_NONE:
         break;
   }

   if (destroy)
   {
      audio_mixer_sound_t *handle = p_rarch->audio_mixer_streams[i].handle;
      if (handle)
         audio_mixer_destroy(handle);

      if (!string_is_empty(p_rarch->audio_mixer_streams[i].name))
         free(p_rarch->audio_mixer_streams[i].name);

      p_rarch->audio_mixer_streams[i].state   = AUDIO_STREAM_STATE_NONE;
      p_rarch->audio_mixer_streams[i].stop_cb = NULL;
      p_rarch->audio_mixer_streams[i].volume  = 0.0f;
      p_rarch->audio_mixer_streams[i].handle  = NULL;
      p_rarch->audio_mixer_streams[i].voice   = NULL;
      p_rarch->audio_mixer_streams[i].name    = NULL;
   }
}
#endif

bool audio_driver_enable_callback(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->audio_callback.callback)
      return false;
   if (p_rarch->audio_callback.set_state)
      p_rarch->audio_callback.set_state(true);
   return true;
}

bool audio_driver_disable_callback(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->audio_callback.callback)
      return false;

   if (p_rarch->audio_callback.set_state)
      p_rarch->audio_callback.set_state(false);
   return true;
}

/* Sets audio monitor rate to new value. */
static void audio_driver_monitor_set_rate(void)
{
   struct rarch_state *p_rarch          = &rarch_st;
   settings_t *settings                 = p_rarch->configuration_settings;
   unsigned audio_out_rate              = settings->uints.audio_out_rate;
   double new_src_ratio                 = (double)audio_out_rate / 
      p_rarch->audio_driver_input;

   p_rarch->audio_source_ratio_original = new_src_ratio;
   p_rarch->audio_source_ratio_current  = new_src_ratio;
}

bool audio_driver_callback(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->audio_callback.callback)
      return false;

   if (p_rarch->audio_callback.callback)
      p_rarch->audio_callback.callback();

   return true;
}

bool audio_driver_has_callback(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->audio_callback.callback)
	   return true;
   return false;
}

#ifdef HAVE_AUDIOMIXER
bool audio_driver_mixer_toggle_mute(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->audio_driver_mixer_mute_enable  = 
      !p_rarch->audio_driver_mixer_mute_enable;
   return true;
}
#endif

static INLINE bool audio_driver_alive(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (     p_rarch->current_audio
         && p_rarch->current_audio->alive
         && p_rarch->audio_driver_context_audio_data)
      return p_rarch->current_audio->alive(p_rarch->audio_driver_context_audio_data);
   return false;
}

static bool audio_driver_start(bool is_shutdown)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->current_audio || !p_rarch->current_audio->start
         || !p_rarch->audio_driver_context_audio_data)
      goto error;
   if (!p_rarch->current_audio->start(
            p_rarch->audio_driver_context_audio_data, is_shutdown))
      goto error;

   return true;

error:
   RARCH_ERR("%s\n",
         msg_hash_to_str(MSG_FAILED_TO_START_AUDIO_DRIVER));
   p_rarch->audio_driver_active = false;
   return false;
}

static bool audio_driver_stop(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->current_audio || !p_rarch->current_audio->stop
         || !p_rarch->audio_driver_context_audio_data)
      return false;
   if (!audio_driver_alive())
      return false;
   return p_rarch->current_audio->stop(
         p_rarch->audio_driver_context_audio_data);
}

void audio_driver_frame_is_reverse(void)
{
   struct rarch_state            *p_rarch = &rarch_st;
   /* We just rewound. Flush rewind audio buffer. */
   if (  p_rarch->recording_data   && 
         p_rarch->recording_driver &&
         p_rarch->recording_driver->push_audio)
   {
      struct record_audio_data ffemu_data;

      ffemu_data.data                    = p_rarch->audio_driver_rewind_buf + 
         p_rarch->audio_driver_rewind_ptr;
      ffemu_data.frames                  = (p_rarch->audio_driver_rewind_size - 
            p_rarch->audio_driver_rewind_ptr) / 2;

      p_rarch->recording_driver->push_audio(p_rarch->recording_data, &ffemu_data);
   }

   if (!(
          p_rarch->runloop_paused          ||
         !p_rarch->audio_driver_active     ||
         !p_rarch->audio_driver_input_data ||
         !p_rarch->audio_driver_output_samples_buf))
      audio_driver_flush(
            p_rarch->audio_driver_rewind_buf  + 
            p_rarch->audio_driver_rewind_ptr,
            p_rarch->audio_driver_rewind_size - 
            p_rarch->audio_driver_rewind_ptr,
            p_rarch->runloop_slowmotion,
            p_rarch->runloop_fastmotion);
}

void audio_set_float(enum audio_action action, float val)
{
   struct rarch_state *p_rarch                    = &rarch_st;

   switch (action)
   {
      case AUDIO_ACTION_VOLUME_GAIN:
         p_rarch->audio_driver_volume_gain        = db_to_gain(val);
         break;
      case AUDIO_ACTION_MIXER_VOLUME_GAIN:
#ifdef HAVE_AUDIOMIXER
         p_rarch->audio_driver_mixer_volume_gain  = db_to_gain(val);
#endif
         break;
      case AUDIO_ACTION_RATE_CONTROL_DELTA:
         p_rarch->audio_driver_rate_control_delta = val;
         break;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }
}

float *audio_get_float_ptr(enum audio_action action)
{
   struct rarch_state *p_rarch     = &rarch_st;

   switch (action)
   {
      case AUDIO_ACTION_RATE_CONTROL_DELTA:
         return &p_rarch->audio_driver_rate_control_delta;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }

   return NULL;
}

bool *audio_get_bool_ptr(enum audio_action action)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (action)
   {
      case AUDIO_ACTION_MIXER_MUTE_ENABLE:
#ifdef HAVE_AUDIOMIXER
         return &p_rarch->audio_driver_mixer_mute_enable;
#else
         break;
#endif
      case AUDIO_ACTION_MUTE_ENABLE:
         return &p_rarch->audio_driver_mute_enable;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }

   return NULL;
}

/* VIDEO */
const char *video_display_server_get_ident(void)
{
   if (!current_display_server)
      return "null";
   return current_display_server->ident;
}

void* video_display_server_init(enum rarch_display_type type)
{
   struct rarch_state            *p_rarch = &rarch_st;

   video_display_server_destroy();

   switch (type)
   {
      case RARCH_DISPLAY_WIN32:
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
         current_display_server = &dispserv_win32;
#endif
         break;
      case RARCH_DISPLAY_X11:
#if defined(HAVE_X11)
         current_display_server = &dispserv_x11;
#endif
         break;
      default:
#if defined(ANDROID)
         current_display_server = &dispserv_android;
#else
         current_display_server = &dispserv_null;
#endif
         break;
   }

   if (current_display_server && current_display_server->init)
       p_rarch->current_display_server_data = current_display_server->init();

   RARCH_LOG("[Video]: Found display server: %s\n",
		   current_display_server->ident);

   p_rarch->initial_screen_orientation = video_display_server_get_screen_orientation();
   p_rarch->current_screen_orientation = p_rarch->initial_screen_orientation;

   return p_rarch->current_display_server_data;
}

void video_display_server_destroy(void)
{
   struct rarch_state                    *p_rarch = &rarch_st;
   const enum rotation initial_screen_orientation = p_rarch->initial_screen_orientation;
   const enum rotation current_screen_orientation = p_rarch->current_screen_orientation;

   if (initial_screen_orientation != current_screen_orientation)
      video_display_server_set_screen_orientation(initial_screen_orientation);

   if (current_display_server)
      if (p_rarch->current_display_server_data)
         current_display_server->destroy(p_rarch->current_display_server_data);
}

bool video_display_server_set_window_opacity(unsigned opacity)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (current_display_server && current_display_server->set_window_opacity)
      return current_display_server->set_window_opacity(
            p_rarch->current_display_server_data, opacity);
   return false;
}

bool video_display_server_set_window_progress(int progress, bool finished)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (current_display_server && current_display_server->set_window_progress)
      return current_display_server->set_window_progress(
            p_rarch->current_display_server_data, progress, finished);
   return false;
}

bool video_display_server_set_window_decorations(bool on)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (current_display_server && current_display_server->set_window_decorations)
      return current_display_server->set_window_decorations(
            p_rarch->current_display_server_data, on);
   return false;
}

bool video_display_server_set_resolution(unsigned width, unsigned height,
      int int_hz, float hz, int center, int monitor_index, int xoffset)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (current_display_server && current_display_server->set_resolution)
      return current_display_server->set_resolution(
            p_rarch->current_display_server_data, width, height, int_hz,
            hz, center, monitor_index, xoffset);
   return false;
}

bool video_display_server_has_resolution_list(void)
{
   return (current_display_server 
         && current_display_server->get_resolution_list);
}

void *video_display_server_get_resolution_list(unsigned *size)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (video_display_server_has_resolution_list())
      return current_display_server->get_resolution_list(
            p_rarch->current_display_server_data, size);
   return NULL;
}

const char *video_display_server_get_output_options(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (current_display_server && current_display_server->get_output_options)
      return current_display_server->get_output_options(p_rarch->current_display_server_data);
   return NULL;
}

void video_display_server_set_screen_orientation(enum rotation rotation)
{
   if (current_display_server && current_display_server->set_screen_orientation)
   {
      struct rarch_state            *p_rarch = &rarch_st;

      RARCH_LOG("[Video]: Setting screen orientation to %d.\n", rotation);
      p_rarch->current_screen_orientation    = rotation;
      current_display_server->set_screen_orientation(rotation);
   }
}

bool video_display_server_can_set_screen_orientation(void)
{
   if (current_display_server && current_display_server->set_screen_orientation)
      return true;
   return false;
}

enum rotation video_display_server_get_screen_orientation(void)
{
   if (current_display_server && current_display_server->get_screen_orientation)
      return current_display_server->get_screen_orientation();
   return ORIENTATION_NORMAL;
}

bool video_display_server_get_flags(gfx_ctx_flags_t *flags)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!current_display_server || !current_display_server->get_flags)
      return false;
   if (!flags)
      return false;

   flags->flags = current_display_server->get_flags(
         p_rarch->current_display_server_data);
   return true;
}

bool video_driver_started_fullscreen(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->video_started_fullscreen;
}

/* Stub functions */

static void update_window_title_null(void *data) { }
static void swap_buffers_null(void *data) { }
static bool get_metrics_null(void *data, enum display_metric_types type,
      float *value) { return false; }
static bool set_resize_null(void *a, unsigned b, unsigned c) { return false; }

/**
 * video_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to video driver at index. Can be NULL
 * if nothing found.
 **/
static const void *video_driver_find_handle(int idx)
{
   const void *drv = video_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * config_get_video_driver_options:
 *
 * Get an enumerated list of all video driver names, separated by '|'.
 *
 * Returns: string listing of all video driver names, separated by '|'.
 **/
const char* config_get_video_driver_options(void)
{
   return char_list_new_special(STRING_LIST_VIDEO_DRIVERS, NULL);
}

bool video_driver_is_threaded(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return video_driver_is_threaded_internal();
}

#ifdef HAVE_VULKAN
static bool hw_render_context_is_vulkan(enum retro_hw_context_type type)
{
   return type == RETRO_HW_CONTEXT_VULKAN;
}
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)
static bool hw_render_context_is_gl(enum retro_hw_context_type type)
{
   switch (type)
   {
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         return true;
      default:
         break;
   }

   return false;
}
#endif

bool *video_driver_get_threaded(void)
{
   struct rarch_state *p_rarch = &rarch_st;
#if defined(__MACH__) && defined(__APPLE__)
   /* TODO/FIXME - force threaded video to disabled on Apple for now
    * until NSWindow/UIWindow concurrency issues are taken care of */
   p_rarch->video_driver_threaded = false;
#endif
   return &p_rarch->video_driver_threaded;
}

void video_driver_set_threaded(bool val)
{
   struct rarch_state *p_rarch = &rarch_st;
#if defined(__MACH__) && defined(__APPLE__)
   /* TODO/FIXME - force threaded video to disabled on Apple for now
    * until NSWindow/UIWindow concurrency issues are taken care of */
   p_rarch->video_driver_threaded = false;
#else
   p_rarch->video_driver_threaded = val;
#endif
}

const char *video_driver_get_ident(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->current_video)
      return NULL;

#ifdef HAVE_THREADS
   if (video_driver_is_threaded_internal())
      return video_thread_get_ident();
#endif

   return p_rarch->current_video->ident;
}

static void video_context_driver_reset(void)
{
   struct rarch_state  *p_rarch   = &rarch_st;

   if (!p_rarch->current_video_context.get_metrics)
      p_rarch->current_video_context.get_metrics         = get_metrics_null;

   if (!p_rarch->current_video_context.update_window_title)
      p_rarch->current_video_context.update_window_title = update_window_title_null;

   if (!p_rarch->current_video_context.set_resize)
      p_rarch->current_video_context.set_resize          = set_resize_null;

   if (!p_rarch->current_video_context.swap_buffers)
      p_rarch->current_video_context.swap_buffers        = swap_buffers_null;
}

bool video_context_driver_set(const gfx_ctx_driver_t *data)
{
   struct rarch_state     *p_rarch = &rarch_st;

   if (!data)
      return false;
   p_rarch->current_video_context = *data;
   video_context_driver_reset();
   return true;
}

void video_context_driver_destroy(void)
{
   struct rarch_state     *p_rarch                           = &rarch_st;

   p_rarch->current_video_context.init                       = NULL;
   p_rarch->current_video_context.bind_api                   = NULL;
   p_rarch->current_video_context.swap_interval              = NULL;
   p_rarch->current_video_context.set_video_mode             = NULL;
   p_rarch->current_video_context.get_video_size             = NULL;
   p_rarch->current_video_context.get_video_output_size      = NULL;
   p_rarch->current_video_context.get_video_output_prev      = NULL;
   p_rarch->current_video_context.get_video_output_next      = NULL;
   p_rarch->current_video_context.get_metrics                = get_metrics_null;
   p_rarch->current_video_context.translate_aspect           = NULL;
   p_rarch->current_video_context.update_window_title        = update_window_title_null;
   p_rarch->current_video_context.check_window               = NULL;
   p_rarch->current_video_context.set_resize                 = set_resize_null;
   p_rarch->current_video_context.suppress_screensaver       = NULL;
   p_rarch->current_video_context.swap_buffers               = swap_buffers_null;
   p_rarch->current_video_context.input_driver               = NULL;
   p_rarch->current_video_context.get_proc_address           = NULL;
   p_rarch->current_video_context.image_buffer_init          = NULL;
   p_rarch->current_video_context.image_buffer_write         = NULL;
   p_rarch->current_video_context.show_mouse                 = NULL;
   p_rarch->current_video_context.ident                      = NULL;
   p_rarch->current_video_context.get_flags                  = NULL;
   p_rarch->current_video_context.set_flags                  = NULL;
   p_rarch->current_video_context.bind_hw_render             = NULL;
   p_rarch->current_video_context.get_context_data           = NULL;
   p_rarch->current_video_context.make_current               = NULL;
}

/**
 * video_driver_get_current_framebuffer:
 *
 * Gets pointer to current hardware renderer framebuffer object.
 * Used by RETRO_ENVIRONMENT_SET_HW_RENDER.
 *
 * Returns: pointer to hardware framebuffer object, otherwise 0.
 **/
static uintptr_t video_driver_get_current_framebuffer(void)
{
   struct rarch_state          *p_rarch = &rarch_st;
   if (     p_rarch->video_driver_poke 
         && p_rarch->video_driver_poke->get_current_framebuffer)
      return p_rarch->video_driver_poke->get_current_framebuffer(
            p_rarch->video_driver_data);
   return 0;
}

static retro_proc_address_t video_driver_get_proc_address(const char *sym)
{
   struct rarch_state          *p_rarch = &rarch_st;
   if (     p_rarch->video_driver_poke 
         && p_rarch->video_driver_poke->get_proc_address)
      return p_rarch->video_driver_poke->get_proc_address(
            p_rarch->video_driver_data, sym);
   return NULL;
}

static void video_driver_filter_free(void)
{
   struct rarch_state          *p_rarch = &rarch_st;

   if (p_rarch->video_driver_state_filter)
      rarch_softfilter_free(p_rarch->video_driver_state_filter);
   p_rarch->video_driver_state_filter   = NULL;

   if (p_rarch->video_driver_state_buffer)
   {
#ifdef _3DS
      linearFree(p_rarch->video_driver_state_buffer);
#else
      free(p_rarch->video_driver_state_buffer);
#endif
   }
   p_rarch->video_driver_state_buffer    = NULL;

   p_rarch->video_driver_state_scale     = 0;
   p_rarch->video_driver_state_out_bpp   = 0;
   p_rarch->video_driver_state_out_rgb32 = false;
}

static void video_driver_init_filter(enum retro_pixel_format colfmt_int)
{
   unsigned pow2_x, pow2_y, maxsize;
   void *buf                            = NULL;
   struct rarch_state          *p_rarch = &rarch_st;
   settings_t *settings                 = p_rarch->configuration_settings;
   struct retro_game_geometry *geom     = &p_rarch->video_driver_av_info.geometry;
   unsigned width                       = geom->max_width;
   unsigned height                      = geom->max_height;
   /* Deprecated format. Gets pre-converted. */
   enum retro_pixel_format colfmt       =
      (colfmt_int == RETRO_PIXEL_FORMAT_0RGB1555) ?
      RETRO_PIXEL_FORMAT_RGB565 : colfmt_int;

   if (video_driver_is_hw_context())
   {
      RARCH_WARN("Cannot use CPU filters when hardware rendering is used.\n");
      return;
   }

   p_rarch->video_driver_state_filter   = rarch_softfilter_new(
         settings->paths.path_softfilter_plugin,
         RARCH_SOFTFILTER_THREADS_AUTO, colfmt, width, height);

   if (!p_rarch->video_driver_state_filter)
   {
      RARCH_ERR("[Video]: Failed to load filter.\n");
      return;
   }

   rarch_softfilter_get_max_output_size(
         p_rarch->video_driver_state_filter,
         &width, &height);

   pow2_x                              = next_pow2(width);
   pow2_y                              = next_pow2(height);
   maxsize                             = MAX(pow2_x, pow2_y);

#ifdef _3DS
   /* On 3DS, video is disabled if the output resolution
    * exceeds 2048x2048. To avoid the user being presented
    * with a black screen, we therefore have to check that
    * the filter upscaling buffer fits within this limit. */
   if (maxsize >= 2048)
   {
      RARCH_ERR("[Video]: Softfilter initialization failed."
            " Upscaling buffer exceeds hardware limitations.\n");
      video_driver_filter_free();
      return;
   }
#endif

   p_rarch->video_driver_state_scale     = maxsize / RARCH_SCALE_BASE;
   p_rarch->video_driver_state_out_rgb32 = rarch_softfilter_get_output_format(
         p_rarch->video_driver_state_filter) == RETRO_PIXEL_FORMAT_XRGB8888;

   p_rarch->video_driver_state_out_bpp   = 
      p_rarch->video_driver_state_out_rgb32 ?
      sizeof(uint32_t)             :
      sizeof(uint16_t);

   /* TODO: Aligned output. */
#ifdef _3DS
   buf = linearMemAlign(
         width * height * p_rarch->video_driver_state_out_bpp, 0x80);
#else
   buf = malloc(
         width * height * p_rarch->video_driver_state_out_bpp);
#endif
   if (!buf)
   {
      RARCH_ERR("[Video]: Softfilter initialization failed.\n");
      video_driver_filter_free();
      return;
   }

   p_rarch->video_driver_state_buffer    = buf;
}

static void video_driver_init_input(input_driver_t *tmp)
{
   struct rarch_state *p_rarch = &rarch_st;
   input_driver_t      **input = &p_rarch->current_input;
   if (*input)
      return;

   /* Video driver didn't provide an input driver,
    * so we use configured one. */
   RARCH_LOG("[Video]: Graphics driver did not initialize an input driver."
         " Attempting to pick a suitable driver.\n");

   if (tmp)
      *input = tmp;
   else
      input_driver_find_driver();

   /* This should never really happen as tmp (driver.input) is always
    * found before this in find_driver_input(), or we have aborted
    * in a similar fashion anyways. */
   if (!p_rarch->current_input || !input_driver_init())
   {
      RARCH_ERR("[Video]: Cannot initialize input driver. Exiting ...\n");
      retroarch_fail(1, "video_driver_init_input()");
   }
}

/**
 * video_driver_monitor_compute_fps_statistics:
 *
 * Computes monitor FPS statistics.
 **/
static void video_driver_monitor_compute_fps_statistics(void)
{
   double        avg_fps       = 0.0;
   double        stddev        = 0.0;
   unsigned        samples     = 0;
   struct rarch_state *p_rarch = &rarch_st;

   if (p_rarch->video_driver_frame_time_count <
         (2 * MEASURE_FRAME_TIME_SAMPLES_COUNT))
   {
      RARCH_LOG(
            "[Video]: Does not have enough samples for monitor refresh rate"
            " estimation. Requires to run for at least %u frames.\n",
            2 * MEASURE_FRAME_TIME_SAMPLES_COUNT);
      return;
   }

   if (video_monitor_fps_statistics(&avg_fps, &stddev, &samples))
   {
      RARCH_LOG("[Video]: Average monitor Hz: %.6f Hz. (%.3f %% frame time"
            " deviation, based on %u last samples).\n",
            avg_fps, 100.0 * stddev, samples);
   }
}

static void video_driver_pixel_converter_free(void)
{
   struct rarch_state *p_rarch = &rarch_st;

   scaler_ctx_gen_reset(p_rarch->video_driver_scaler_ptr->scaler);

   if (p_rarch->video_driver_scaler_ptr->scaler)
      free(p_rarch->video_driver_scaler_ptr->scaler);

   if (p_rarch->video_driver_scaler_ptr->scaler_out)
      free(p_rarch->video_driver_scaler_ptr->scaler_out);
   if (p_rarch->video_driver_scaler_ptr)
      free(p_rarch->video_driver_scaler_ptr);

   p_rarch->video_driver_scaler_ptr->scaler     = NULL;
   p_rarch->video_driver_scaler_ptr->scaler_out = NULL;
   p_rarch->video_driver_scaler_ptr             = NULL;
}

static void video_driver_free_hw_context(void)
{
   struct rarch_state *p_rarch = &rarch_st;

   video_driver_context_lock();

   if (p_rarch->hw_render.context_destroy)
      p_rarch->hw_render.context_destroy();

   memset(&p_rarch->hw_render, 0, sizeof(p_rarch->hw_render));

   video_driver_context_unlock();

   p_rarch->hw_render_context_negotiation = NULL;
}

static void video_driver_free_internal(void)
{
   struct rarch_state *p_rarch = &rarch_st;
#ifdef HAVE_THREADS
   bool        is_threaded     = video_driver_is_threaded_internal();
#endif

#ifdef HAVE_VIDEO_LAYOUT
   video_layout_deinit();
#endif

   command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);

   if (!video_driver_is_video_cache_context())
      video_driver_free_hw_context();

   if (!(p_rarch->current_input_data == p_rarch->video_driver_data))
   {
      if (p_rarch->current_input && p_rarch->current_input->free)
         p_rarch->current_input->free(p_rarch->current_input_data);
      p_rarch->current_input->keyboard_mapping_blocked = false;
      p_rarch->current_input_data                      = NULL;
   }

   if (p_rarch->video_driver_data
         && p_rarch->current_video 
         && p_rarch->current_video->free)
      p_rarch->current_video->free(p_rarch->video_driver_data);

   if (p_rarch->video_driver_scaler_ptr)
      video_driver_pixel_converter_free();
   video_driver_filter_free();
   dir_free_shader();

#ifdef HAVE_THREADS
   if (is_threaded)
      return;
#endif

   video_driver_monitor_compute_fps_statistics();
}

static bool video_driver_pixel_converter_init(unsigned size)
{
   struct rarch_state          *p_rarch = &rarch_st;
   struct retro_hw_render_callback *hwr =
      video_driver_get_hw_context_internal();
   void *scalr_out                      = NULL;
   video_pixel_scaler_t          *scalr = NULL;
   struct scaler_ctx        *scalr_ctx  = NULL;
   const enum retro_pixel_format 
      video_driver_pix_fmt              = p_rarch->video_driver_pix_fmt;

   /* If pixel format is not 0RGB1555, we don't need to do
    * any internal pixel conversion. */
   if (video_driver_pix_fmt != RETRO_PIXEL_FORMAT_0RGB1555)
      return true;

   /* No need to perform pixel conversion for HW rendering contexts. */
   if (hwr && hwr->context_type != RETRO_HW_CONTEXT_NONE)
      return true;

   RARCH_WARN("0RGB1555 pixel format is deprecated,"
         " and will be slower. For 15/16-bit, RGB565"
         " format is preferred.\n");

   scalr = (video_pixel_scaler_t*)calloc(1, sizeof(*scalr));

   if (!scalr)
      goto error;

   p_rarch->video_driver_scaler_ptr         = scalr;

   scalr_ctx = (struct scaler_ctx*)calloc(1, sizeof(*scalr_ctx));

   if (!scalr_ctx)
      goto error;

   p_rarch->video_driver_scaler_ptr->scaler              = scalr_ctx;
   p_rarch->video_driver_scaler_ptr->scaler->scaler_type = SCALER_TYPE_POINT;
   p_rarch->video_driver_scaler_ptr->scaler->in_fmt      = SCALER_FMT_0RGB1555;

   /* TODO: Pick either ARGB8888 or RGB565 depending on driver. */
   p_rarch->video_driver_scaler_ptr->scaler->out_fmt     = SCALER_FMT_RGB565;

   if (!scaler_ctx_gen_filter(scalr_ctx))
      goto error;

   scalr_out = calloc(sizeof(uint16_t), size * size);

   if (!scalr_out)
      goto error;

   p_rarch->video_driver_scaler_ptr->scaler_out          = scalr_out;

   return true;

error:
   if (p_rarch->video_driver_scaler_ptr)
      video_driver_pixel_converter_free();
   video_driver_filter_free();

   return false;
}

static void video_driver_set_viewport_config(void)
{
   struct rarch_state  *p_rarch   = &rarch_st;
   settings_t       *settings     = p_rarch->configuration_settings;
   float       video_aspect_ratio = settings->floats.video_aspect_ratio;

   if (video_aspect_ratio < 0.0f)
   {
      struct retro_game_geometry *geom = &p_rarch->video_driver_av_info.geometry;

      if (geom->aspect_ratio > 0.0f &&
            settings->bools.video_aspect_ratio_auto)
         aspectratio_lut[ASPECT_RATIO_CONFIG].value = geom->aspect_ratio;
      else
      {
         unsigned base_width  = geom->base_width;
         unsigned base_height = geom->base_height;

         /* Get around division by zero errors */
         if (base_width == 0)
            base_width = 1;
         if (base_height == 0)
            base_height = 1;
         aspectratio_lut[ASPECT_RATIO_CONFIG].value =
            (float)base_width / base_height; /* 1:1 PAR. */
      }
   }
   else
      aspectratio_lut[ASPECT_RATIO_CONFIG].value = video_aspect_ratio;
}

static void video_driver_set_viewport_square_pixel(void)
{
   unsigned len, highest, i, aspect_x, aspect_y;
   struct rarch_state     *p_rarch   = &rarch_st;
   struct retro_game_geometry *geom  = &p_rarch->video_driver_av_info.geometry;
   unsigned width                    = geom->base_width;
   unsigned height                   = geom->base_height;
   unsigned int rotation             = retroarch_get_rotation();

   if (width == 0 || height == 0)
      return;

   len      = MIN(width, height);
   highest  = 1;

   for (i = 1; i < len; i++)
   {
      if ((width % i) == 0 && (height % i) == 0)
         highest = i;
   }

   if (rotation % 2)
   {
      aspect_x = height / highest;
      aspect_y = width / highest;
   }
   else
   {
      aspect_x = width / highest;
      aspect_y = height / highest;
   }

   snprintf(aspectratio_lut[ASPECT_RATIO_SQUARE].name,
         sizeof(aspectratio_lut[ASPECT_RATIO_SQUARE].name),
         "1:1 PAR (%u:%u DAR)", aspect_x, aspect_y);

   aspectratio_lut[ASPECT_RATIO_SQUARE].value = (float)aspect_x / aspect_y;
}

static bool video_driver_init_internal(bool *video_is_threaded)
{
   video_info_t video;
   unsigned max_dim, scale, width, height;
   video_viewport_t *custom_vp            = NULL;
   input_driver_t *tmp                    = NULL;
   static uint16_t dummy_pixels[32]       = {0};
   struct rarch_state            *p_rarch = &rarch_st;
   settings_t *settings                   = p_rarch->configuration_settings;
   struct retro_game_geometry *geom       = &p_rarch->video_driver_av_info.geometry;
   const char *path_softfilter_plugin     = settings->paths.path_softfilter_plugin;
   const enum retro_pixel_format 
      video_driver_pix_fmt                = p_rarch->video_driver_pix_fmt;

   if (!string_is_empty(path_softfilter_plugin))
      video_driver_init_filter(video_driver_pix_fmt);

   max_dim   = MAX(geom->max_width, geom->max_height);
   scale     = next_pow2(max_dim) / RARCH_SCALE_BASE;
   scale     = MAX(scale, 1);

   if (p_rarch->video_driver_state_filter)
      scale  = p_rarch->video_driver_state_scale;

   /* Update core-dependent aspect ratio values. */
   video_driver_set_viewport_square_pixel();
   video_driver_set_viewport_core();
   video_driver_set_viewport_config();

   /* Update CUSTOM viewport. */
   custom_vp = video_viewport_get_custom();

   if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      float default_aspect = aspectratio_lut[ASPECT_RATIO_CORE].value;
      aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
         (custom_vp->width && custom_vp->height) ?
         (float)custom_vp->width / custom_vp->height : default_aspect;
   }

   {
      /* Guard against aspect ratio index possibly being out of bounds */
      unsigned new_aspect_idx = settings->uints.video_aspect_ratio_idx;
      if (new_aspect_idx > ASPECT_RATIO_END)
         new_aspect_idx = settings->uints.video_aspect_ratio_idx = 0;

      video_driver_set_aspect_ratio_value(
            aspectratio_lut[new_aspect_idx].value);
   }

   if (settings->bools.video_fullscreen|| retroarch_is_forced_fullscreen())
   {
      width  = settings->uints.video_fullscreen_x;
      height = settings->uints.video_fullscreen_y;
   }
   else
   {
      /* TODO: remove when the new window resizing core is hooked */
      if (settings->bools.video_window_save_positions &&
         (settings->uints.window_position_width ||
          settings->uints.window_position_height))
      {
         width  = settings->uints.window_position_width;
         height = settings->uints.window_position_height;
      }
      else
      {
         float video_scale = settings->floats.video_scale;
         if (settings->bools.video_force_aspect)
         {
            /* Do rounding here to simplify integer scale correctness. */
            unsigned base_width =
               roundf(geom->base_height * p_rarch->video_driver_aspect_ratio);
            width  = roundf(base_width * video_scale);
         }
         else
            width  = roundf(geom->base_width   * video_scale);
         height    = roundf(geom->base_height  * video_scale);
      }
   }

   if (width && height)
      RARCH_LOG("[Video]: Video @ %ux%u\n", width, height);
   else
      RARCH_LOG("[Video]: Video @ fullscreen\n");

   video_driver_display_type_set(RARCH_DISPLAY_NONE);
   video_driver_display_set(0);
   video_driver_display_userdata_set(0);
   video_driver_window_set(0);

   if (!video_driver_pixel_converter_init(RARCH_SCALE_BASE * scale))
   {
      RARCH_ERR("[Video]: Failed to initialize pixel converter.\n");
      goto error;
   }

   video.width                       = width;
   video.height                      = height;
   video.fullscreen                  = settings->bools.video_fullscreen || 
                                       retroarch_is_forced_fullscreen();
   video.vsync                       = settings->bools.video_vsync && 
      !p_rarch->runloop_force_nonblock;
   video.force_aspect                = settings->bools.video_force_aspect;
   video.font_enable                 = settings->bools.video_font_enable;
   video.swap_interval               = settings->uints.video_swap_interval;
   video.adaptive_vsync              = settings->bools.video_adaptive_vsync;
#ifdef GEKKO
   video.viwidth                     = settings->uints.video_viwidth;
   video.vfilter                     = settings->bools.video_vfilter;
#endif
   video.smooth                      = settings->bools.video_smooth;
#ifdef HAVE_ODROIDGO2
   video.ctx_scaling                 = settings->bools.video_ctx_scaling;
#endif
   video.input_scale                 = scale;
   video.font_size                   = settings->floats.video_font_size;
   video.path_font                   = settings->paths.path_font;
   video.rgb32                       = 
      p_rarch->video_driver_state_filter ?
      p_rarch->video_driver_state_out_rgb32 :
      (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);
   video.parent                      = 0;

   p_rarch->video_started_fullscreen = video.fullscreen;

   /* Reset video frame count */
   p_rarch->video_driver_frame_count = 0;

   tmp                               = p_rarch->current_input;
   /* Need to grab the "real" video driver interface on a reinit. */
   video_driver_find_driver();

#ifdef HAVE_THREADS
   video.is_threaded                 = video_driver_is_threaded_internal();
   *video_is_threaded                = video.is_threaded;

   if (video.is_threaded)
   {
      bool ret;
      /* Can't do hardware rendering with threaded driver currently. */
      RARCH_LOG("[Video]: Starting threaded video driver ...\n");

      ret = video_init_thread((const video_driver_t**)&p_rarch->current_video,
               &p_rarch->video_driver_data,
               &p_rarch->current_input,
               (void**)&p_rarch->current_input_data,
               p_rarch->current_video,
               video);
      if (!ret)
      {
         RARCH_ERR("[Video]: Cannot open threaded video driver ... Exiting ...\n");
         goto error;
      }
   }
   else
#endif
      p_rarch->video_driver_data = p_rarch->current_video->init(
            &video,
            &p_rarch->current_input,
            (void**)&p_rarch->current_input_data);

   if (!p_rarch->video_driver_data)
   {
      RARCH_ERR("[Video]: Cannot open video driver ... Exiting ...\n");
      goto error;
   }

   p_rarch->video_driver_poke = NULL;
   if (p_rarch->current_video->poke_interface)
      p_rarch->current_video->poke_interface(
            p_rarch->video_driver_data, &p_rarch->video_driver_poke);

   if (p_rarch->current_video->viewport_info &&
         (!custom_vp->width  ||
          !custom_vp->height))
   {
      /* Force custom viewport to have sane parameters. */
      custom_vp->width = width;
      custom_vp->height = height;

      video_driver_get_viewport_info(custom_vp);
   }

   video_driver_set_rotation(retroarch_get_rotation() % 4);

   p_rarch->current_video->suppress_screensaver(p_rarch->video_driver_data,
         settings->bools.ui_suspend_screensaver_enable);

   video_driver_init_input(tmp);

#ifdef HAVE_OVERLAY
   retroarch_overlay_deinit();
   retroarch_overlay_init();
#endif

#ifdef HAVE_VIDEO_LAYOUT
   if (settings->bools.video_layout_enable)
   {
      video_layout_init(p_rarch->video_driver_data,
            video_driver_layout_render_interface());
      video_layout_load(settings->paths.path_video_layout);
      video_layout_view_select(settings->uints.video_layout_selected_view);
   }
#endif

   if (!p_rarch->current_core.game_loaded)
      video_driver_cached_frame_set(&dummy_pixels, 4, 4, 8);

#if defined(PSP)
   video_driver_set_texture_frame(&dummy_pixels, false, 1, 1, 1.0f);
#endif

   video_context_driver_reset();

   video_display_server_init(p_rarch->video_driver_display_type);

   if ((enum rotation)settings->uints.screen_orientation != ORIENTATION_NORMAL)
      video_display_server_set_screen_orientation((enum rotation)settings->uints.screen_orientation);

   dir_free_shader();

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   {
      bool dir_list_is_free              = true;
      bool show_hidden_files             = settings->bools.show_hidden_files;
      const char *directory_video_shader = settings->paths.directory_video_shader;
      const char *directory_menu_config  = settings->paths.directory_menu_config;

      if (!string_is_empty(directory_video_shader))
         dir_list_is_free = !dir_init_shader(
               directory_video_shader,
               show_hidden_files);

      if (dir_list_is_free && 
            !string_is_empty(directory_menu_config))
         dir_list_is_free = !dir_init_shader(
               directory_menu_config,
               show_hidden_files);

      if (dir_list_is_free && 
            !path_is_empty(RARCH_PATH_CONFIG))
      {
         char *config_file_directory = strdup(path_get(RARCH_PATH_CONFIG));
         path_basedir(config_file_directory);

         if (config_file_directory)
         {
            dir_init_shader(
                  config_file_directory,
                  settings->bools.show_hidden_files);
            free(config_file_directory);
         }
      }
   }
#endif

   return true;

error:
   retroarch_fail(1, "init_video()");
   return false;
}

bool video_driver_set_viewport(unsigned width, unsigned height,
      bool force_fullscreen, bool allow_rotate)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (!p_rarch->current_video || !p_rarch->current_video->set_viewport)
      return false;
   p_rarch->current_video->set_viewport(
         p_rarch->video_driver_data, width, height,
         force_fullscreen, allow_rotate);
   return true;
}

bool video_driver_set_rotation(unsigned rotation)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (!p_rarch->current_video || !p_rarch->current_video->set_rotation)
      return false;
   p_rarch->current_video->set_rotation(p_rarch->video_driver_data, rotation);
   return true;
}

bool video_driver_set_video_mode(unsigned width,
      unsigned height, bool fullscreen)
{
   gfx_ctx_mode_t mode;
   struct rarch_state            *p_rarch = &rarch_st;

   if (  p_rarch->video_driver_poke && 
         p_rarch->video_driver_poke->set_video_mode)
   {
      p_rarch->video_driver_poke->set_video_mode(p_rarch->video_driver_data,
            width, height, fullscreen);
      return true;
   }

   mode.width      = width;
   mode.height     = height;
   mode.fullscreen = fullscreen;

   return video_context_driver_set_video_mode(&mode);
}

bool video_driver_get_video_output_size(unsigned *width, unsigned *height)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (!p_rarch->video_driver_poke || !p_rarch->video_driver_poke->get_video_output_size)
      return false;
   p_rarch->video_driver_poke->get_video_output_size(p_rarch->video_driver_data,
         width, height);
   return true;
}

void video_driver_set_osd_msg(const char *msg, const void *data, void *font)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (p_rarch->video_driver_poke && p_rarch->video_driver_poke->set_osd_msg)
      p_rarch->video_driver_poke->set_osd_msg(p_rarch->video_driver_data,
            msg, data, font);
}

void video_driver_set_texture_enable(bool enable, bool fullscreen)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (p_rarch->video_driver_poke && p_rarch->video_driver_poke->set_texture_enable)
      p_rarch->video_driver_poke->set_texture_enable(p_rarch->video_driver_data,
            enable, fullscreen);
}

void video_driver_set_texture_frame(const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (p_rarch->video_driver_poke && 
         p_rarch->video_driver_poke->set_texture_frame)
      p_rarch->video_driver_poke->set_texture_frame(p_rarch->video_driver_data,
            frame, rgb32, width, height, alpha);
}

#ifdef HAVE_OVERLAY
static bool video_driver_overlay_interface(
      const video_overlay_interface_t **iface)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (!p_rarch->current_video || !p_rarch->current_video->overlay_interface)
      return false;
   p_rarch->current_video->overlay_interface(p_rarch->video_driver_data, iface);
   return true;
}
#endif

#ifdef HAVE_VIDEO_LAYOUT
const video_layout_render_interface_t *video_driver_layout_render_interface(void)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (  !p_rarch->current_video || 
         !p_rarch->current_video->video_layout_render_interface)
      return NULL;

   return p_rarch->current_video->video_layout_render_interface(
         p_rarch->video_driver_data);
}
#endif

void *video_driver_read_frame_raw(unsigned *width,
   unsigned *height, size_t *pitch)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (!p_rarch->current_video || !p_rarch->current_video->read_frame_raw)
      return NULL;
   return p_rarch->current_video->read_frame_raw(
         p_rarch->video_driver_data, width,
         height, pitch);
}

void video_driver_set_filtering(unsigned index, bool smooth, bool ctx_scaling)
{
   struct rarch_state            *p_rarch = &rarch_st;
   if (p_rarch->video_driver_poke && p_rarch->video_driver_poke->set_filtering)
      p_rarch->video_driver_poke->set_filtering(p_rarch->video_driver_data,
            index, smooth, ctx_scaling);
}

void video_driver_cached_frame_set(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   struct rarch_state *p_rarch  = &rarch_st;

   if (data)
      p_rarch->frame_cache_data = data;

   p_rarch->frame_cache_width   = width;
   p_rarch->frame_cache_height  = height;
   p_rarch->frame_cache_pitch   = pitch;
}

void video_driver_cached_frame_get(const void **data, unsigned *width,
      unsigned *height, size_t *pitch)
{
   struct rarch_state *p_rarch  = &rarch_st;
   if (data)
      *data    = p_rarch->frame_cache_data;
   if (width)
      *width   = p_rarch->frame_cache_width;
   if (height)
      *height  = p_rarch->frame_cache_height;
   if (pitch)
      *pitch   = p_rarch->frame_cache_pitch;
}

void video_driver_get_size(unsigned *width, unsigned *height)
{
   struct rarch_state *p_rarch = &rarch_st;
#ifdef HAVE_THREADS
   bool            is_threaded = video_driver_is_threaded_internal();

   video_driver_threaded_lock(is_threaded);
#endif
   if (width)
      *width  = p_rarch->video_driver_width;
   if (height)
      *height = p_rarch->video_driver_height;
#ifdef HAVE_THREADS
   video_driver_threaded_unlock(is_threaded);
#endif
}

void video_driver_set_size(unsigned width, unsigned height)
{
   struct rarch_state *p_rarch   = &rarch_st;
#ifdef HAVE_THREADS
   bool            is_threaded   = video_driver_is_threaded_internal();

   video_driver_threaded_lock(is_threaded);
#endif
   p_rarch->video_driver_width   = width;
   p_rarch->video_driver_height  = height;

#ifdef HAVE_THREADS
   video_driver_threaded_unlock(is_threaded);
#endif
}

/**
 * video_monitor_set_refresh_rate:
 * @hz                 : New refresh rate for monitor.
 *
 * Sets monitor refresh rate to new value.
 **/
void video_monitor_set_refresh_rate(float hz)
{
   char msg[128];
   struct rarch_state *p_rarch = &rarch_st;
   settings_t        *settings = p_rarch->configuration_settings;

   snprintf(msg, sizeof(msg),
         "Setting refresh rate to: %.3f Hz.", hz);
   runloop_msg_queue_push(msg, 1, 180, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s\n", msg);

   configuration_set_float(settings,
         settings->floats.video_refresh_rate,
         hz);
}

/**
 * video_monitor_fps_statistics
 * @refresh_rate       : Monitor refresh rate.
 * @deviation          : Deviation from measured refresh rate.
 * @sample_points      : Amount of sampled points.
 *
 * Gets the monitor FPS statistics based on the current
 * runtime.
 *
 * Returns: true (1) on success.
 * false (0) if:
 * a) threaded video mode is enabled
 * b) less than 2 frame time samples.
 * c) FPS monitor enable is off.
 **/
bool video_monitor_fps_statistics(double *refresh_rate,
      double *deviation, unsigned *sample_points)
{
   unsigned i;
   retro_time_t accum          = 0;
   retro_time_t avg            = 0;
   retro_time_t accum_var      = 0;
   unsigned samples            = 0;
   struct rarch_state *p_rarch = &rarch_st;

#ifdef HAVE_THREADS
   if (video_driver_is_threaded_internal())
      return false;
#endif

   samples = MIN(MEASURE_FRAME_TIME_SAMPLES_COUNT,
         (unsigned)p_rarch->video_driver_frame_time_count);

   if (samples < 2)
      return false;

   /* Measure statistics on frame time (microsecs), *not* FPS. */
   for (i = 0; i < samples; i++)
   {
      accum += p_rarch->video_driver_frame_time_samples[i];
#if 0
      RARCH_LOG("[Video]: Interval #%u: %d usec / frame.\n",
            i, (int)frame_time_samples[i]);
#endif
   }

   avg = accum / samples;

   /* Drop first measurement. It is likely to be bad. */
   for (i = 0; i < samples; i++)
   {
      retro_time_t diff = p_rarch->video_driver_frame_time_samples[i] - avg;
      accum_var         += diff * diff;
   }

   *deviation        = sqrt((double)accum_var / (samples - 1)) / avg;

   if (refresh_rate)
      *refresh_rate  = 1000000.0 / avg;

   if (sample_points)
      *sample_points = samples;

   return true;
}

float video_driver_get_aspect_ratio(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->video_driver_aspect_ratio;
}

void video_driver_set_aspect_ratio_value(float value)
{
   struct rarch_state        *p_rarch = &rarch_st;
   p_rarch->video_driver_aspect_ratio = value;
}

enum retro_pixel_format video_driver_get_pixel_format(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->video_driver_pix_fmt;
}

/**
 * video_driver_cached_frame:
 *
 * Renders the current video frame.
 **/
void video_driver_cached_frame(void)
{
   struct rarch_state *p_rarch  = &rarch_st;
   void             *recording  = p_rarch->recording_data;
   struct retro_callbacks *cbs  = &p_rarch->retro_ctx;

   /* Cannot allow recording when pushing duped frames. */
   p_rarch->recording_data      = NULL;

   if (p_rarch->current_core.inited)
      cbs->frame_cb(
            (p_rarch->frame_cache_data != RETRO_HW_FRAME_BUFFER_VALID)
            ? p_rarch->frame_cache_data : NULL,
            p_rarch->frame_cache_width,
            p_rarch->frame_cache_height,
            p_rarch->frame_cache_pitch);

   p_rarch->recording_data      = recording;
}

static void video_driver_monitor_adjust_system_rates(void)
{
   struct rarch_state            *p_rarch = &rarch_st;
   float timing_skew                      = 0.0f;
   settings_t *settings                   = p_rarch->configuration_settings;
   const struct retro_system_timing *info = (const struct retro_system_timing*)
      &p_rarch->video_driver_av_info.timing;
   float video_refresh_rate               = settings->floats.video_refresh_rate;
   bool vrr_runloop_enable                = settings->bools.vrr_runloop_enable;
   float audio_max_timing_skew            = settings->floats.audio_max_timing_skew;
   float timing_skew_hz                   = video_refresh_rate;

   if (!info || info->fps <= 0.0)
      return;

   p_rarch->video_driver_core_hz           = info->fps;

   if (p_rarch->video_driver_crt_switching_active)
      timing_skew_hz                       = p_rarch->video_driver_core_hz;
   timing_skew                             = fabs(
         1.0f - info->fps / timing_skew_hz);

   if (!vrr_runloop_enable)
   {
      /* We don't want to adjust pitch too much. If we have extreme cases,
       * just don't readjust at all. */
      if (timing_skew <= audio_max_timing_skew)
         return;

      RARCH_LOG("[Video]: Timings deviate too much. Will not adjust."
            " (Display = %.2f Hz, Game = %.2f Hz)\n",
            video_refresh_rate,
            (float)info->fps);
   }

   if (info->fps <= timing_skew_hz)
      return;

   /* We won't be able to do VSync reliably when game FPS > monitor FPS. */
   p_rarch->runloop_force_nonblock = true;
   RARCH_LOG("[Video]: Game FPS > Monitor FPS. Cannot rely on VSync.\n");
}

static void video_driver_lock_new(void)
{
   struct rarch_state *p_rarch = &rarch_st;

   video_driver_lock_free();
#ifdef HAVE_THREADS
   if (!p_rarch->display_lock)
      p_rarch->display_lock = slock_new();
   retro_assert(p_rarch->display_lock);

   if (!p_rarch->context_lock)
      p_rarch->context_lock = slock_new();
   retro_assert(p_rarch->context_lock);
#endif
}

void video_driver_set_cached_frame_ptr(const void *data)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (data)
      p_rarch->frame_cache_data = data;
}

void video_driver_set_stub_frame(void)
{
   struct rarch_state *p_rarch   = &rarch_st;

   p_rarch->frame_bak            = p_rarch->current_video->frame;
   p_rarch->current_video->frame = video_null.frame;
}

void video_driver_unset_stub_frame(void)
{
   struct rarch_state *p_rarch      = &rarch_st;

   if (p_rarch->frame_bak)
      p_rarch->current_video->frame = p_rarch->frame_bak;

   p_rarch->frame_bak               = NULL;
}

bool video_driver_supports_viewport_read(void)
{
   struct rarch_state *p_rarch      = &rarch_st;

   return p_rarch->current_video->read_viewport 
      && p_rarch->current_video->viewport_info;
}

bool video_driver_prefer_viewport_read(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   bool video_gpu_screenshot   = settings->bools.video_gpu_screenshot;

   return video_gpu_screenshot ||
      (video_driver_is_hw_context() && 
       !p_rarch->current_video->read_frame_raw);
}

bool video_driver_supports_read_frame_raw(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->current_video->read_frame_raw)
      return true;
   return false;
}

void video_driver_set_viewport_core(void)
{
   struct rarch_state        *p_rarch   = &rarch_st;
   struct retro_game_geometry *geom     = &p_rarch->video_driver_av_info.geometry;

   if (!geom || geom->base_width <= 0.0f || geom->base_height <= 0.0f)
      return;

   /* Fallback to 1:1 pixel ratio if none provided */
   if (geom->aspect_ratio > 0.0f)
      aspectratio_lut[ASPECT_RATIO_CORE].value = geom->aspect_ratio;
   else
      aspectratio_lut[ASPECT_RATIO_CORE].value =
         (float)geom->base_width / geom->base_height;
}

void video_driver_reset_custom_viewport(void)
{
   struct video_viewport *custom_vp = video_viewport_get_custom();

   custom_vp->width  = 0;
   custom_vp->height = 0;
   custom_vp->x      = 0;
   custom_vp->y      = 0;
}

void video_driver_set_rgba(void)
{
   struct rarch_state *p_rarch    = &rarch_st;
   video_driver_lock();
   p_rarch->video_driver_use_rgba = true;
   video_driver_unlock();
}

void video_driver_unset_rgba(void)
{
   struct rarch_state *p_rarch    = &rarch_st;
   video_driver_lock();
   p_rarch->video_driver_use_rgba = false;
   video_driver_unlock();
}

bool video_driver_supports_rgba(void)
{
   bool tmp;
   struct rarch_state *p_rarch = &rarch_st;
   video_driver_lock();
   tmp = p_rarch->video_driver_use_rgba;
   video_driver_unlock();
   return tmp;
}

bool video_driver_get_next_video_out(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->video_driver_poke)
      return false;

   if (!p_rarch->video_driver_poke->get_video_output_next)
      return video_context_driver_get_video_output_next();
   p_rarch->video_driver_poke->get_video_output_next(p_rarch->video_driver_data);
   return true;
}

bool video_driver_get_prev_video_out(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->video_driver_poke)
      return false;

   if (!p_rarch->video_driver_poke->get_video_output_prev)
      return video_context_driver_get_video_output_prev();
   p_rarch->video_driver_poke->get_video_output_prev(p_rarch->video_driver_data);
   return true;
}

void video_driver_monitor_reset(void)
{
   struct rarch_state            *p_rarch = &rarch_st;
   p_rarch->video_driver_frame_time_count = 0;
}

void video_driver_set_aspect_ratio(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t  *settings       = p_rarch->configuration_settings;
   unsigned  aspect_ratio_idx  = settings->uints.video_aspect_ratio_idx;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         video_driver_set_viewport_square_pixel();
         break;

      case ASPECT_RATIO_CORE:
         video_driver_set_viewport_core();
         break;

      case ASPECT_RATIO_CONFIG:
         video_driver_set_viewport_config();
         break;

      default:
         break;
   }

   video_driver_set_aspect_ratio_value(
            aspectratio_lut[aspect_ratio_idx].value);

   if (  p_rarch->video_driver_poke && 
         p_rarch->video_driver_poke->set_aspect_ratio)
      p_rarch->video_driver_poke->set_aspect_ratio(
            p_rarch->video_driver_data, aspect_ratio_idx);
}

void video_driver_update_viewport(
      struct video_viewport* vp, bool force_full, bool keep_aspect)
{
   struct rarch_state *p_rarch     = &rarch_st;
   float            device_aspect  = (float)vp->full_width / vp->full_height;
   settings_t *settings            = p_rarch->configuration_settings;
   bool video_scale_integer        = settings->bools.video_scale_integer;
   unsigned video_aspect_ratio_idx = settings->uints.video_aspect_ratio_idx;

   if (     p_rarch->video_context_data
         && p_rarch->current_video_context.translate_aspect)
      device_aspect = p_rarch->current_video_context.translate_aspect(
            p_rarch->video_context_data, vp->full_width, vp->full_height);

   vp->x      = 0;
   vp->y      = 0;
   vp->width  = vp->full_width;
   vp->height = vp->full_height;

   if (video_scale_integer && !force_full)
      video_viewport_get_scaled_integer(
            vp,
            vp->full_width,
            vp->full_height,
            p_rarch->video_driver_aspect_ratio, keep_aspect);
   else if (keep_aspect && !force_full)
   {
      float desired_aspect = p_rarch->video_driver_aspect_ratio;

#if defined(HAVE_MENU)
      if (video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const struct video_viewport* custom = video_viewport_get_custom();

         vp->x      = custom->x;
         vp->y      = custom->y;
         vp->width  = custom->width;
         vp->height = custom->height;
      }
      else
#endif
      {
         float delta;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta      = (desired_aspect / device_aspect - 1.0f)
               / 2.0f + 0.5f;
            vp->x      = (int)roundf(vp->full_width * (0.5f - delta));
            vp->width  = (unsigned)roundf(2.0f * vp->full_width * delta);
            vp->y      = 0;
            vp->height = vp->full_height;
         }
         else
         {
            vp->x      = 0;
            vp->width  = vp->full_width;
            delta      = (device_aspect / desired_aspect - 1.0f)
               / 2.0f + 0.5f;
            vp->y      = (int)roundf(vp->full_height * (0.5f - delta));
            vp->height = (unsigned)roundf(2.0f * vp->full_height * delta);
         }
      }
   }

#if defined(RARCH_MOBILE)
   /* In portrait mode, we want viewport to gravitate to top of screen. */
   if (device_aspect < 1.0f)
      vp->y = 0;
#endif
}

void video_driver_show_mouse(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->video_driver_poke && p_rarch->video_driver_poke->show_mouse)
      p_rarch->video_driver_poke->show_mouse(p_rarch->video_driver_data, true);
}

void video_driver_hide_mouse(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->video_driver_poke && p_rarch->video_driver_poke->show_mouse)
      p_rarch->video_driver_poke->show_mouse(p_rarch->video_driver_data, false);
}

static bool video_driver_find_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   struct rarch_state *p_rarch             = &rarch_st;
   settings_t          *settings           = p_rarch->configuration_settings;

   if (video_driver_is_hw_context())
   {
      struct retro_hw_render_callback *hwr =
         video_driver_get_hw_context_internal();
      p_rarch->current_video               = NULL;

      (void)hwr;

#if defined(HAVE_VULKAN)
      if (hwr && hw_render_context_is_vulkan(hwr->context_type))
      {
         RARCH_LOG("[Video]: Using HW render, Vulkan driver forced.\n");
         if (!string_is_equal(settings->arrays.video_driver, "vulkan"))
         {
            RARCH_LOG("[Video]: \"%s\" saved as cached driver.\n", settings->arrays.video_driver);
            strlcpy(p_rarch->cached_video_driver,
                  settings->arrays.video_driver,
                  sizeof(p_rarch->cached_video_driver));
            configuration_set_string(settings,
                  settings->arrays.video_driver,
                  "vulkan");
         }
         p_rarch->current_video = &video_vulkan;
      }
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)
      if (hwr && hw_render_context_is_gl(hwr->context_type))
      {
         RARCH_LOG("[Video]: Using HW render, OpenGL driver forced.\n");

         /* If we have configured one of the HW render capable GL drivers, go with that. */
         if (  !string_is_equal(settings->arrays.video_driver, "gl") &&
               !string_is_equal(settings->arrays.video_driver, "glcore"))
         {
            RARCH_LOG("[Video]: \"%s\" saved as cached driver.\n", settings->arrays.video_driver);
            strlcpy(p_rarch->cached_video_driver,
                  settings->arrays.video_driver,
                  sizeof(p_rarch->cached_video_driver));
#if defined(HAVE_OPENGL_CORE)
            RARCH_LOG("[Video]: Forcing \"glcore\" driver.\n");
            configuration_set_string(settings,
                  settings->arrays.video_driver, "glcore");
            p_rarch->current_video = &video_gl_core;
#else
            RARCH_LOG("[Video]: Forcing \"gl\" driver.\n");
            configuration_set_string(settings,
                  settings->arrays.video_driver, "gl");
            p_rarch->current_video = &video_gl2;
#endif
         }
         else
         {
            RARCH_LOG("[Video]: Using configured \"%s\" driver for GL HW render.\n",
                  settings->arrays.video_driver);
         }
      }
#endif

      if (p_rarch->current_video)
         return true;
   }

   if (frontend_driver_has_get_video_driver_func())
   {
      p_rarch->current_video = (video_driver_t*)
         frontend_driver_get_video_driver();

      if (p_rarch->current_video)
         return true;
      RARCH_WARN("Frontend supports get_video_driver() but did not specify one.\n");
   }

   drv.label = "video_driver";
   drv.s     = settings->arrays.video_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i = (int)drv.len;

   if (i >= 0)
      p_rarch->current_video = (video_driver_t*)video_driver_find_handle(i);
   else
   {
      if (verbosity_is_enabled())
      {
         unsigned d;
         RARCH_ERR("Couldn't find any video driver named \"%s\"\n",
               settings->arrays.video_driver);
         RARCH_LOG_OUTPUT("Available video drivers are:\n");
         for (d = 0; video_driver_find_handle(d); d++)
            RARCH_LOG_OUTPUT("\t%s\n", video_drivers[d]->ident);
         RARCH_WARN("Going to default to first video driver...\n");
      }

      p_rarch->current_video = (video_driver_t*)video_driver_find_handle(0);

      if (!p_rarch->current_video)
         retroarch_fail(1, "find_video_driver()");
   }
   return true;
}

void video_driver_apply_state_changes(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (  p_rarch->video_driver_poke &&
         p_rarch->video_driver_poke->apply_state_changes)
      p_rarch->video_driver_poke->apply_state_changes(
            p_rarch->video_driver_data);
}

bool video_driver_read_viewport(uint8_t *buffer, bool is_idle)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (     p_rarch->current_video->read_viewport
         && p_rarch->current_video->read_viewport(
            p_rarch->video_driver_data, buffer, is_idle))
      return true;
   return false;
}

void video_driver_default_settings(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   global_t           *global  = &p_rarch->g_extern;

   if (!global)
      return;

   global->console.screen.gamma_correction       = DEFAULT_GAMMA;
   global->console.flickerfilter_enable          = false;
   global->console.softfilter_enable             = false;

   global->console.screen.resolutions.current.id = 0;
}

void video_driver_load_settings(config_file_t *conf)
{
   bool               tmp_bool = false;
   struct rarch_state *p_rarch = &rarch_st;
   global_t            *global = &p_rarch->g_extern;

   if (!conf)
      return;

   CONFIG_GET_INT_BASE(conf, global,
         console.screen.gamma_correction, "gamma_correction");

   if (config_get_bool(conf, "flicker_filter_enable",
         &tmp_bool))
      global->console.flickerfilter_enable = tmp_bool;

   if (config_get_bool(conf, "soft_filter_enable",
         &tmp_bool))
      global->console.softfilter_enable = tmp_bool;

   CONFIG_GET_INT_BASE(conf, global,
         console.screen.soft_filter_index,
         "soft_filter_index");
   CONFIG_GET_INT_BASE(conf, global,
         console.screen.resolutions.current.id,
         "current_resolution_id");
   CONFIG_GET_INT_BASE(conf, global,
         console.screen.flicker_filter_index,
         "flicker_filter_index");
}

void video_driver_save_settings(config_file_t *conf)
{
   struct rarch_state *p_rarch = &rarch_st;
   global_t            *global = &p_rarch->g_extern;
   if (!conf)
      return;

   config_set_int(conf, "gamma_correction",
         global->console.screen.gamma_correction);
   config_set_bool(conf, "flicker_filter_enable",
         global->console.flickerfilter_enable);
   config_set_bool(conf, "soft_filter_enable",
         global->console.softfilter_enable);

   config_set_int(conf, "soft_filter_index",
         global->console.screen.soft_filter_index);
   config_set_int(conf, "current_resolution_id",
         global->console.screen.resolutions.current.id);
   config_set_int(conf, "flicker_filter_index",
         global->console.screen.flicker_filter_index);
}

static void video_driver_reinit_context(int flags)
{
   /* RARCH_DRIVER_CTL_UNINIT clears the callback struct so we
    * need to make sure to keep a copy */
   struct retro_hw_render_callback hwr_copy;
   struct rarch_state          *p_rarch = &rarch_st;
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context_internal();
   const struct retro_hw_render_context_negotiation_interface *iface =
      video_driver_get_context_negotiation_interface();
   memcpy(&hwr_copy, hwr, sizeof(hwr_copy));

   driver_uninit(flags);

   memcpy(hwr, &hwr_copy, sizeof(*hwr));
   p_rarch->hw_render_context_negotiation = iface;

   drivers_init(flags);
}

void video_driver_reinit(int flags)
{
   struct rarch_state          *p_rarch    = &rarch_st;
   struct retro_hw_render_callback *hwr    =
      video_driver_get_hw_context_internal();

   p_rarch->video_driver_cache_context     = (hwr->cache_context != false);
   p_rarch->video_driver_cache_context_ack = false;
   video_driver_reinit_context(flags);
   p_rarch->video_driver_cache_context     = false;
}

bool video_driver_is_hw_context(void)
{
   bool            is_hw_context = false;
   struct rarch_state   *p_rarch = &rarch_st;

   video_driver_context_lock();
   is_hw_context      = (p_rarch->hw_render.context_type != RETRO_HW_CONTEXT_NONE);
   video_driver_context_unlock();

   return is_hw_context;
}

const struct retro_hw_render_context_negotiation_interface *
   video_driver_get_context_negotiation_interface(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->hw_render_context_negotiation;
}

bool video_driver_is_video_cache_context(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->video_driver_cache_context;
}

void video_driver_set_video_cache_context_ack(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->video_driver_cache_context_ack = true;
}

bool video_driver_get_viewport_info(struct video_viewport *viewport)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->current_video || !p_rarch->current_video->viewport_info)
      return false;
   p_rarch->current_video->viewport_info(
         p_rarch->video_driver_data, viewport);
   return true;
}

/**
 * video_viewport_get_scaled_integer:
 * @vp            : Viewport handle
 * @width         : Width.
 * @height        : Height.
 * @aspect_ratio  : Aspect ratio (in float).
 * @keep_aspect   : Preserve aspect ratio?
 *
 * Gets viewport scaling dimensions based on
 * scaled integer aspect ratio.
 **/
void video_viewport_get_scaled_integer(struct video_viewport *vp,
      unsigned width, unsigned height,
      float aspect_ratio, bool keep_aspect)
{
   int padding_x                   = 0;
   int padding_y                   = 0;
   struct rarch_state     *p_rarch = &rarch_st;
   settings_t *settings            = p_rarch->configuration_settings;
   unsigned video_aspect_ratio_idx = settings->uints.video_aspect_ratio_idx;

   if (video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      struct video_viewport *custom = video_viewport_get_custom();

      if (custom)
      {
         padding_x = width - custom->width;
         padding_y = height - custom->height;
         width     = custom->width;
         height    = custom->height;
      }
   }
   else
   {
      unsigned base_width;
      /* Use system reported sizes as these define the
       * geometry for the "normal" case. */
      unsigned base_height  = p_rarch->video_driver_av_info.geometry.base_height;
      unsigned int rotation = retroarch_get_rotation();
      
      if (rotation % 2)
         base_height = p_rarch->video_driver_av_info.geometry.base_width;

      if (base_height == 0)
         base_height = 1;

      /* Account for non-square pixels.
       * This is sort of contradictory with the goal of integer scale,
       * but it is desirable in some cases.
       *
       * If square pixels are used, base_height will be equal to
       * system->av_info.base_height. */
      base_width = (unsigned)roundf(base_height * aspect_ratio);

      /* Make sure that we don't get 0x scale ... */
      if (width >= base_width && height >= base_height)
      {
         if (keep_aspect)
         {
            /* X/Y scale must be same. */
            unsigned max_scale = MIN(width / base_width,
                  height / base_height);
            padding_x          = width - base_width * max_scale;
            padding_y          = height - base_height * max_scale;
         }
         else
         {
            /* X/Y can be independent, each scaled as much as possible. */
            padding_x = width % base_width;
            padding_y = height % base_height;
         }
      }

      width     -= padding_x;
      height    -= padding_y;
   }

   vp->width  = width;
   vp->height = height;
   vp->x      = padding_x / 2;
   vp->y      = padding_y / 2;
}

struct video_viewport *video_viewport_get_custom(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t        *settings = p_rarch->configuration_settings;
   return &settings->video_viewport_custom;
}

unsigned video_pixel_get_alignment(unsigned pitch)
{
   if (pitch & 1)
      return 1;
   if (pitch & 2)
      return 2;
   if (pitch & 4)
      return 4;
   return 8;
}

/**
 * video_driver_frame:
 * @data                 : pointer to data of the video frame.
 * @width                : width of the video frame.
 * @height               : height of the video frame.
 * @pitch                : pitch of the video frame.
 *
 * Video frame render callback function.
 **/
static void video_driver_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   char fps_text[128];
   static char video_driver_msg[256];
   static retro_time_t curr_time;
   static retro_time_t fps_time;
   static float last_fps, frame_time;
   retro_time_t new_time;
   video_frame_info_t video_info;
#if defined(HAVE_GFX_WIDGETS)
   bool widgets_active;
#endif
   struct rarch_state *p_rarch  = &rarch_st;
   const enum retro_pixel_format 
      video_driver_pix_fmt      = p_rarch->video_driver_pix_fmt;
   bool runloop_idle            = p_rarch->runloop_idle;
   bool video_driver_active     = p_rarch->video_driver_active;
   settings_t *settings         = p_rarch->configuration_settings;

   fps_text[0]                  = '\0';
   video_driver_msg[0]          = '\0';

   if (!video_driver_active)
      return;

   new_time                     = cpu_features_get_time_usec();
#if defined(HAVE_GFX_WIDGETS)
   widgets_active               = gfx_widgets_active();
#endif

   if (data)
      p_rarch->frame_cache_data = data;
   p_rarch->frame_cache_width   = width;
   p_rarch->frame_cache_height  = height;
   p_rarch->frame_cache_pitch   = pitch;

   if (
            p_rarch->video_driver_scaler_ptr
         && data
         && (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555)
         && (data != RETRO_HW_FRAME_BUFFER_VALID)
         && video_pixel_frame_scale(
            p_rarch->video_driver_scaler_ptr->scaler,
            p_rarch->video_driver_scaler_ptr->scaler_out,
            data, width, height, pitch)
      )
   {
      data                = p_rarch->video_driver_scaler_ptr->scaler_out;
      pitch               = p_rarch->video_driver_scaler_ptr->scaler->out_stride;
   }

   video_driver_build_info(&video_info);

   /* Get the amount of frames per seconds. */
   if (p_rarch->video_driver_frame_count)
   {
      unsigned fps_update_interval                 = 
         settings->uints.fps_update_interval;
      size_t buf_pos                               = 1;
      /* set this to 1 to avoid an offset issue */
      unsigned write_index                         =
         p_rarch->video_driver_frame_time_count++ &
         (MEASURE_FRAME_TIME_SAMPLES_COUNT - 1);
      frame_time                                   = new_time - fps_time;
      p_rarch->video_driver_frame_time_samples
         [write_index]                             = frame_time;
      fps_time                                     = new_time;

      if (video_info.fps_show)
         buf_pos = snprintf(
               fps_text, sizeof(fps_text),
               "FPS: %6.2f", last_fps);

      if (video_info.framecount_show)
      {
         char frames_text[64];
         if (fps_text[buf_pos-1] != '\0')
            strlcat(fps_text, " || ", sizeof(fps_text));
         snprintf(frames_text,
               sizeof(frames_text),
               "%s: %" PRIu64, msg_hash_to_str(MSG_FRAMES),
               (uint64_t)p_rarch->video_driver_frame_count);
         buf_pos = strlcat(fps_text, frames_text, sizeof(fps_text));
      }

      if (video_info.memory_show)
      {
         char mem[128];
         uint64_t mem_bytes_used  = frontend_driver_get_free_memory();
         uint64_t mem_bytes_total = frontend_driver_get_total_memory();

         mem[0] = '\0';
         snprintf(
               mem, sizeof(mem), "MEM: %.2f/%.2fMB", mem_bytes_used / (1024.0f * 1024.0f),
               mem_bytes_total / (1024.0f * 1024.0f));
         if (fps_text[buf_pos-1] != '\0')
            strlcat(fps_text, " || ", sizeof(fps_text));
         strlcat(fps_text, mem, sizeof(fps_text));
      }

      if ((p_rarch->video_driver_frame_count % fps_update_interval) == 0)
      {
         last_fps = TIME_TO_FPS(curr_time, new_time,
               fps_update_interval);

         strlcpy(p_rarch->video_driver_window_title,
               p_rarch->video_driver_title_buf,
               sizeof(p_rarch->video_driver_window_title));

         if (!string_is_empty(fps_text))
         {
            strlcat(p_rarch->video_driver_window_title,
                  " || ", sizeof(p_rarch->video_driver_window_title));
            strlcat(p_rarch->video_driver_window_title,
                  fps_text, sizeof(p_rarch->video_driver_window_title));
         }

         curr_time                                 = new_time;
         p_rarch->video_driver_window_title_update = true;
      }
   }
   else
   {
      curr_time = fps_time = new_time;

      strlcpy(p_rarch->video_driver_window_title,
            p_rarch->video_driver_title_buf,
            sizeof(p_rarch->video_driver_window_title));

      if (video_info.fps_show)
         strlcpy(fps_text,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               sizeof(fps_text));

      p_rarch->video_driver_window_title_update = true;
   }

   /* Add core status message to 'fps_text' string
    * TODO/FIXME: fps_text is used for several status
    * parameters, not just FPS. It should probably be
    * renamed to reflect this... */
   if (video_info.core_status_msg_show)
   {
      /* Note: We need to lock a mutex here. Strictly
       * speaking, runloop_core_status_msg is not part
       * of the message queue, but:
       * - It may be implemented as a queue in the future
       * - It seems unnecessary to create a new slock_t
       *   object for this type of message when
       *   _runloop_msg_queue_lock is already available
       * We therefore just call runloop_msg_queue_lock()/
       * runloop_msg_queue_unlock() in this case */
      runloop_msg_queue_lock();

      /* Check whether duration timer has elapsed */
      runloop_core_status_msg.duration -= gfx_animation_get_delta_time();

      if (runloop_core_status_msg.duration < 0.0f)
      {
         runloop_core_status_msg.str[0]   = '\0';
         runloop_core_status_msg.priority = 0;
         runloop_core_status_msg.duration = 0.0f;
         runloop_core_status_msg.set      = false;
      }
      else
      {
         /* If 'fps_text' is already set, add status
          * message at the end */
         if (!string_is_empty(fps_text))
         {
            strlcat(fps_text,
                  " || ", sizeof(fps_text));
            strlcat(fps_text,
                  runloop_core_status_msg.str, sizeof(fps_text));
         }
         else
            strlcpy(fps_text, runloop_core_status_msg.str,
                  sizeof(fps_text));
      }

      runloop_msg_queue_unlock();
   }

   /* Slightly messy code,
    * but we really need to do processing before blocking on VSync
    * for best possible scheduling.
    */
   if (
         (
             !p_rarch->video_driver_state_filter
          || !video_info.post_filter_record
          || !data
          || p_rarch->video_driver_record_gpu_buffer
         ) && p_rarch->recording_data
           && p_rarch->recording_driver 
           && p_rarch->recording_driver->push_video)
      recording_dump_frame(data, width, height,
            pitch, runloop_idle);

   if (data && p_rarch->video_driver_state_filter)
   {
      unsigned output_width                             = 0;
      unsigned output_height                            = 0;
      unsigned output_pitch                             = 0;

      rarch_softfilter_get_output_size(p_rarch->video_driver_state_filter,
            &output_width, &output_height, width, height);

      output_pitch = (output_width) * p_rarch->video_driver_state_out_bpp;

      rarch_softfilter_process(p_rarch->video_driver_state_filter,
            p_rarch->video_driver_state_buffer, output_pitch,
            data, width, height, pitch);

      if (video_info.post_filter_record 
            && p_rarch->recording_data
            && p_rarch->recording_driver
            && p_rarch->recording_driver->push_video)
         recording_dump_frame(p_rarch->video_driver_state_buffer,
               output_width, output_height, output_pitch,
               runloop_idle);

      data   = p_rarch->video_driver_state_buffer;
      width  = output_width;
      height = output_height;
      pitch  = output_pitch;
   }

   if (p_rarch->runloop_msg_queue_size > 0)
   {
      /* If widgets are currently enabled, then
       * messages were pushed to the queue before
       * widgets were initialised - in this case, the
       * first item in the message queue should be
       * extracted and pushed to the widget message
       * queue instead */
#if defined(HAVE_GFX_WIDGETS)
      if (widgets_active)
      {
         bool msg_found = false;
         msg_queue_entry_t msg_entry;

         runloop_msg_queue_lock();
         msg_found                       = msg_queue_extract(
               p_rarch->runloop_msg_queue, &msg_entry);
         p_rarch->runloop_msg_queue_size = msg_queue_size(
               p_rarch->runloop_msg_queue);
         runloop_msg_queue_unlock();

         if (msg_found)
            gfx_widgets_msg_queue_push(
                  NULL,
                  msg_entry.msg,
                  roundf((float)msg_entry.duration / 60.0f * 1000.0f),
                  msg_entry.title,
                  msg_entry.icon,
                  msg_entry.category,
                  msg_entry.prio,
                  false,
#ifdef HAVE_MENU
                  p_rarch->menu_driver_alive
#else
                  false
#endif
            );
      }
      /* ...otherwise, just output message via
       * regular OSD notification text (if enabled) */
      else if (video_info.font_enable)
#else
      if (video_info.font_enable)
#endif
      {
         const char *msg                 = NULL;
         runloop_msg_queue_lock();
         msg                             = msg_queue_pull(p_rarch->runloop_msg_queue);
         p_rarch->runloop_msg_queue_size = msg_queue_size(p_rarch->runloop_msg_queue);
         if (msg)
            strlcpy(video_driver_msg, msg, sizeof(video_driver_msg));
         runloop_msg_queue_unlock();
      }
   }

   if (video_info.statistics_show)
   {
      audio_statistics_t audio_stats         = {0.0f};
      double stddev                          = 0.0;
      struct retro_system_av_info *av_info   = &p_rarch->video_driver_av_info;
      unsigned red                           = 255;
      unsigned green                         = 255;
      unsigned blue                          = 255;
      unsigned alpha                         = 255;

      video_monitor_fps_statistics(NULL, &stddev, NULL);

      video_info.osd_stat_params.x           = 0.010f;
      video_info.osd_stat_params.y           = 0.950f;
      video_info.osd_stat_params.scale       = 1.0f;
      video_info.osd_stat_params.full_screen = true;
      video_info.osd_stat_params.drop_x      = -2;
      video_info.osd_stat_params.drop_y      = -2;
      video_info.osd_stat_params.drop_mod    = 0.3f;
      video_info.osd_stat_params.drop_alpha  = 1.0f;
      video_info.osd_stat_params.color       = COLOR_ABGR(
            red, green, blue, alpha);

      audio_compute_buffer_statistics(&audio_stats);

      snprintf(video_info.stat_text,
            sizeof(video_info.stat_text),
            "Video Statistics:\n -Frame rate: %6.2f fps\n -Frame time: %6.2f ms\n -Frame time deviation: %.3f %%\n"
            " -Frame count: %" PRIu64"\n -Viewport: %d x %d x %3.2f\n"
            "Audio Statistics:\n -Average buffer saturation: %.2f %%\n -Standard deviation: %.2f %%\n -Time spent close to underrun: %.2f %%\n -Time spent close to blocking: %.2f %%\n -Sample count: %d\n"
            "Core Geometry:\n -Size: %u x %u\n -Max Size: %u x %u\n -Aspect: %3.2f\nCore Timing:\n -FPS: %3.2f\n -Sample Rate: %6.2f\n",
            last_fps,
            frame_time / 1000.0f,
            100.0 * stddev,
            p_rarch->video_driver_frame_count,
            video_info.width,
            video_info.height,
            video_info.refresh_rate,
            audio_stats.average_buffer_saturation,
            audio_stats.std_deviation_percentage,
            audio_stats.close_to_underrun,
            audio_stats.close_to_blocking,
            audio_stats.samples,
            av_info->geometry.base_width,
            av_info->geometry.base_height,
            av_info->geometry.max_width,
            av_info->geometry.max_height,
            av_info->geometry.aspect_ratio,
            av_info->timing.fps,
            av_info->timing.sample_rate);

      /* TODO/FIXME - add OSD chat text here */
   }

   if (p_rarch->current_video && p_rarch->current_video->frame)
      p_rarch->video_driver_active = p_rarch->current_video->frame(
            p_rarch->video_driver_data, data, width, height,
            p_rarch->video_driver_frame_count,
            (unsigned)pitch, video_driver_msg, &video_info);

   p_rarch->video_driver_frame_count++;

   /* Display the FPS, with a higher priority. */
   if (     video_info.fps_show
         || video_info.framecount_show
         || video_info.memory_show
         || video_info.core_status_msg_show
         )
   {
#if defined(HAVE_GFX_WIDGETS)
      if (widgets_active)
         gfx_widgets_set_fps_text(fps_text);
      else
#endif
      {
         runloop_msg_queue_push(fps_text, 2, 1, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

   /* trigger set resolution*/
   if (video_info.crt_switch_resolution)
   {
      p_rarch->video_driver_crt_switching_active          = true;

      switch (video_info.crt_switch_resolution_super)
      {
         case 2560:
         case 3840:
         case 1920:
            width                                         = 
               video_info.crt_switch_resolution_super;
            p_rarch->video_driver_crt_dynamic_super_width = false;
            break;
         case 1:
            p_rarch->video_driver_crt_dynamic_super_width = true;
            break;
         default:
            p_rarch->video_driver_crt_dynamic_super_width = false;
            break;
      }

      crt_switch_res_core(
            width,
            height,
            p_rarch->video_driver_core_hz,
            video_info.crt_switch_resolution,
            video_info.crt_switch_center_adjust,
            video_info.monitor_index,
            p_rarch->video_driver_crt_dynamic_super_width);
   }
   else if (!video_info.crt_switch_resolution)
      p_rarch->video_driver_crt_switching_active = false;
}

void crt_switch_driver_reinit(void)
{
   video_driver_reinit(DRIVERS_CMD_ALL);
}

void video_driver_display_type_set(enum rarch_display_type type)
{
   struct rarch_state            *p_rarch = &rarch_st;
   p_rarch->video_driver_display_type     = type;
}

uintptr_t video_driver_display_get(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->video_driver_display;
}

uintptr_t video_driver_display_userdata_get(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->video_driver_display_userdata;
}

void video_driver_display_userdata_set(uintptr_t idx)
{
   struct rarch_state            *p_rarch = &rarch_st;
   p_rarch->video_driver_display_userdata = idx;
}

void video_driver_display_set(uintptr_t idx)
{
   struct rarch_state   *p_rarch = &rarch_st;
   p_rarch->video_driver_display = idx;
}

enum rarch_display_type video_driver_display_type_get(void)
{
   struct rarch_state            *p_rarch = &rarch_st;
   return p_rarch->video_driver_display_type;
}

void video_driver_window_set(uintptr_t idx)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->video_driver_window = idx;
}

uintptr_t video_driver_window_get(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->video_driver_window;
}

bool video_driver_texture_load(void *data,
      enum texture_filter_type  filter_type,
      uintptr_t *id)
{
   struct rarch_state *p_rarch = &rarch_st;
#ifdef HAVE_THREADS
   bool            is_threaded = video_driver_is_threaded_internal();
#endif
   if (!id || !p_rarch->video_driver_poke || !p_rarch->video_driver_poke->load_texture)
      return false;

#ifdef HAVE_THREADS
   if (is_threaded)
      if (p_rarch->current_video_context.make_current)
         p_rarch->current_video_context.make_current(false);
#endif

   *id = p_rarch->video_driver_poke->load_texture(
         p_rarch->video_driver_data, data,
         video_driver_is_threaded_internal(),
         filter_type);

   return true;
}

bool video_driver_texture_unload(uintptr_t *id)
{
   struct rarch_state *p_rarch = &rarch_st;
#ifdef HAVE_THREADS
   bool            is_threaded = video_driver_is_threaded_internal();
#endif
   if (!p_rarch->video_driver_poke || !p_rarch->video_driver_poke->unload_texture)
      return false;

#ifdef HAVE_THREADS
   if (is_threaded)
      if (p_rarch->current_video_context.make_current)
         p_rarch->current_video_context.make_current(false);
#endif

   p_rarch->video_driver_poke->unload_texture(p_rarch->video_driver_data, *id);
   *id = 0;
   return true;
}

void video_driver_build_info(video_frame_info_t *video_info)
{
   video_viewport_t *custom_vp             = NULL;
   struct rarch_state       *p_rarch       = &rarch_st;
   settings_t *settings                    = p_rarch->configuration_settings;
#ifdef HAVE_THREADS
   bool is_threaded                        = 
      video_driver_is_threaded_internal();

   video_driver_threaded_lock(is_threaded);
#endif
   custom_vp                               = &settings->video_viewport_custom;
   video_info->refresh_rate                = settings->floats.video_refresh_rate;
   video_info->crt_switch_resolution       = settings->uints.crt_switch_resolution;
   video_info->crt_switch_resolution_super = settings->uints.crt_switch_resolution_super;
   video_info->crt_switch_center_adjust    = settings->ints.crt_switch_center_adjust;
   video_info->black_frame_insertion       = settings->bools.video_black_frame_insertion;
   video_info->hard_sync                   = settings->bools.video_hard_sync;
   video_info->hard_sync_frames            = settings->uints.video_hard_sync_frames;
   video_info->fps_show                    = settings->bools.video_fps_show;
   video_info->memory_show                 = settings->bools.video_memory_show;
   video_info->statistics_show             = settings->bools.video_statistics_show;
   video_info->framecount_show             = settings->bools.video_framecount_show;
   video_info->core_status_msg_show        = runloop_core_status_msg.set;
   video_info->aspect_ratio_idx            = settings->uints.video_aspect_ratio_idx;
   video_info->post_filter_record          = settings->bools.video_post_filter_record;
   video_info->input_menu_swap_ok_cancel_buttons    = settings->bools.input_menu_swap_ok_cancel_buttons;
   video_info->max_swapchain_images        = settings->uints.video_max_swapchain_images;
   video_info->windowed_fullscreen         = settings->bools.video_windowed_fullscreen;
   video_info->fullscreen                  = settings->bools.video_fullscreen 
      || retroarch_is_forced_fullscreen();
   video_info->menu_mouse_enable           = settings->bools.menu_mouse_enable;
   video_info->monitor_index               = settings->uints.video_monitor_index;

   video_info->font_enable                 = settings->bools.video_font_enable;
   video_info->font_msg_pos_x              = settings->floats.video_msg_pos_x;
   video_info->font_msg_pos_y              = settings->floats.video_msg_pos_y;
   video_info->font_msg_color_r            = settings->floats.video_msg_color_r;
   video_info->font_msg_color_g            = settings->floats.video_msg_color_g;
   video_info->font_msg_color_b            = settings->floats.video_msg_color_b;
   video_info->custom_vp_x                 = custom_vp->x;
   video_info->custom_vp_y                 = custom_vp->y;
   video_info->custom_vp_width             = custom_vp->width;
   video_info->custom_vp_height            = custom_vp->height;
   video_info->custom_vp_full_width        = custom_vp->full_width;
   video_info->custom_vp_full_height       = custom_vp->full_height;

#if defined(HAVE_GFX_WIDGETS)
   video_info->widgets_is_paused           = p_rarch->gfx_widgets_paused;
   video_info->widgets_is_fast_forwarding  = p_rarch->gfx_widgets_fast_forward;
   video_info->widgets_is_rewinding        = p_rarch->gfx_widgets_rewinding;
#else
   video_info->widgets_is_paused           = false;
   video_info->widgets_is_fast_forwarding  = false;
   video_info->widgets_is_rewinding        = false;
#endif

   video_info->width                       = p_rarch->video_driver_width;
   video_info->height                      = p_rarch->video_driver_height;

   video_info->use_rgba                    = p_rarch->video_driver_use_rgba;

   video_info->libretro_running            = false;
   video_info->msg_bgcolor_enable          = 
      settings->bools.video_msg_bgcolor_enable;

#ifdef HAVE_MENU
   video_info->menu_is_alive               = p_rarch->menu_driver_alive;
   video_info->menu_footer_opacity         = settings->floats.menu_footer_opacity;
   video_info->menu_header_opacity         = settings->floats.menu_header_opacity;
   video_info->materialui_color_theme      = settings->uints.menu_materialui_color_theme;
   video_info->ozone_color_theme           = settings->uints.menu_ozone_color_theme;
   video_info->menu_shader_pipeline        = settings->uints.menu_xmb_shader_pipeline;
   video_info->xmb_theme                   = settings->uints.menu_xmb_theme;
   video_info->xmb_color_theme             = settings->uints.menu_xmb_color_theme;
   video_info->timedate_enable             = settings->bools.menu_timedate_enable;
   video_info->battery_level_enable        = settings->bools.menu_battery_level_enable;
   video_info->xmb_shadows_enable          =
      settings->bools.menu_xmb_shadows_enable;
   video_info->xmb_alpha_factor            =
      settings->uints.menu_xmb_alpha_factor;
   video_info->menu_wallpaper_opacity      =
      settings->floats.menu_wallpaper_opacity;
   video_info->menu_framebuffer_opacity    =
      settings->floats.menu_framebuffer_opacity;

   video_info->libretro_running            = p_rarch->current_core.game_loaded;
#else
   video_info->menu_is_alive               = false;
   video_info->menu_footer_opacity         = 0.0f;
   video_info->menu_header_opacity         = 0.0f;
   video_info->materialui_color_theme      = 0;
   video_info->menu_shader_pipeline        = 0;
   video_info->xmb_color_theme             = 0;
   video_info->xmb_theme                   = 0;
   video_info->timedate_enable             = false;
   video_info->battery_level_enable        = false;
   video_info->xmb_shadows_enable          = false;
   video_info->xmb_alpha_factor            = 0.0f;
   video_info->menu_framebuffer_opacity    = 0.0f;
   video_info->menu_wallpaper_opacity      = 0.0f;
#endif

   video_info->runloop_is_paused           = p_rarch->runloop_paused;
   video_info->runloop_is_slowmotion       = p_rarch->runloop_slowmotion;

   video_info->input_driver_nonblock_state = p_rarch->input_driver_nonblock_state;
   video_info->context_data                = p_rarch->video_context_data;
   video_info->cb_swap_buffers             = p_rarch->current_video_context.swap_buffers;
   video_info->cb_set_resize               = p_rarch->current_video_context.set_resize;

   video_info->userdata                    = video_driver_get_ptr_internal(false);

#ifdef HAVE_THREADS
   video_driver_threaded_unlock(is_threaded);
#endif
}

/**
 * video_driver_translate_coord_viewport:
 * @mouse_x                        : Pointer X coordinate.
 * @mouse_y                        : Pointer Y coordinate.
 * @res_x                          : Scaled  X coordinate.
 * @res_y                          : Scaled  Y coordinate.
 * @res_screen_x                   : Scaled screen X coordinate.
 * @res_screen_y                   : Scaled screen Y coordinate.
 *
 * Translates pointer [X,Y] coordinates into scaled screen
 * coordinates based on viewport info.
 *
 * Returns: true (1) if successful, false if video driver doesn't support
 * viewport info.
 **/
bool video_driver_translate_coord_viewport(
      struct video_viewport *vp,
      int mouse_x,           int mouse_y,
      int16_t *res_x,        int16_t *res_y,
      int16_t *res_screen_x, int16_t *res_screen_y)
{
   int scaled_screen_x, scaled_screen_y, scaled_x, scaled_y;
   int norm_vp_width         = (int)vp->width;
   int norm_vp_height        = (int)vp->height;
   int norm_full_vp_width    = (int)vp->full_width;
   int norm_full_vp_height   = (int)vp->full_height;

   if (norm_vp_width <= 0 ||
       norm_vp_height <= 0 ||
       norm_full_vp_width <= 0 ||
       norm_full_vp_height <= 0)
      return false;

   if (mouse_x >= 0 && mouse_x <= norm_full_vp_width)
      scaled_screen_x = ((2 * mouse_x * 0x7fff)
            / norm_full_vp_width)  - 0x7fff;
   else
      scaled_screen_x = -0x8000; /* OOB */

   if (mouse_y >= 0 && mouse_y <= norm_full_vp_height)
      scaled_screen_y = ((2 * mouse_y * 0x7fff)
            / norm_full_vp_height) - 0x7fff;
   else
      scaled_screen_y = -0x8000; /* OOB */

   mouse_x           -= vp->x;
   mouse_y           -= vp->y;

   if (mouse_x >= 0 && mouse_x <= norm_vp_width)
      scaled_x        = ((2 * mouse_x * 0x7fff)
            / norm_vp_width) - 0x7fff;
   else
      scaled_x        = -0x8000; /* OOB */

   if (mouse_y >= 0 && mouse_y <= norm_vp_height)
      scaled_y        = ((2 * mouse_y * 0x7fff)
            / norm_vp_height) - 0x7fff;
   else
      scaled_y        = -0x8000; /* OOB */

   *res_x             = scaled_x;
   *res_y             = scaled_y;
   *res_screen_x      = scaled_screen_x;
   *res_screen_y      = scaled_screen_y;

   return true;
}

bool video_driver_has_focus(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return video_has_focus();
}

void video_driver_get_window_title(char *buf, unsigned len)
{
   struct rarch_state *p_rarch  = &rarch_st;

   if (buf && p_rarch->video_driver_window_title_update)
   {
      struct rarch_state *p_rarch = &rarch_st;
      strlcpy(buf, p_rarch->video_driver_window_title, len);
      p_rarch->video_driver_window_title_update = false;
   }
}

/**
 * find_video_context_driver_driver_index:
 * @ident                      : Identifier of resampler driver to find.
 *
 * Finds graphics context driver index by @ident name.
 *
 * Returns: graphics context driver index if driver was found, otherwise
 * -1.
 **/
static int find_video_context_driver_index(const char *ident)
{
   unsigned i;
   for (i = 0; gfx_ctx_drivers[i]; i++)
      if (string_is_equal_noncase(ident, gfx_ctx_drivers[i]->ident))
         return i;
   return -1;
}

/**
 * find_prev_context_driver:
 *
 * Finds previous driver in graphics context driver array.
 **/
bool video_context_driver_find_prev_driver(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t        *settings = p_rarch->configuration_settings;
   int                       i = find_video_context_driver_index(
         settings->arrays.video_context_driver);

   if (i > 0)
   {
      configuration_set_string(settings,
      settings->arrays.video_context_driver,
            gfx_ctx_drivers[i - 1]->ident);
      return true;
   }

   RARCH_WARN("Couldn't find any previous video context driver.\n");
   return false;
}

/**
 * find_next_context_driver:
 *
 * Finds next driver in graphics context driver array.
 **/
bool video_context_driver_find_next_driver(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t        *settings = p_rarch->configuration_settings;
   int                       i = find_video_context_driver_index(
         settings->arrays.video_context_driver);

   if (i >= 0 && gfx_ctx_drivers[i + 1])
   {
      configuration_set_string(settings,
      settings->arrays.video_context_driver,
            gfx_ctx_drivers[i + 1]->ident);
      return true;
   }

   RARCH_WARN("Couldn't find any next video context driver.\n");
   return false;
}

/**
 * video_context_driver_init:
 * @data                    : Input data.
 * @ctx                     : Graphics context driver to initialize.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Initialize graphics context driver.
 *
 * Returns: graphics context driver if successfully initialized,
 * otherwise NULL.
 **/
static const gfx_ctx_driver_t *video_context_driver_init(
      void *data,
      const gfx_ctx_driver_t *ctx,
      const char *ident,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx,
      void **ctx_data)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t       *settings  = p_rarch->configuration_settings;
   bool  video_shared_context  = settings->bools.video_shared_context;

   if (!ctx->bind_api(data, api, major, minor))
   {
      RARCH_WARN("Failed to bind API (#%u, version %u.%u)"
            " on context driver \"%s\".\n",
            (unsigned)api, major, minor, ctx->ident);

      return NULL;
   }

   if (!(*ctx_data = ctx->init(data)))
      return NULL;

   if (ctx->bind_hw_render)
      ctx->bind_hw_render(*ctx_data,
            video_shared_context && hw_render_ctx);

   return ctx;
}

/**
 * video_context_driver_init_first:
 * @data                    : Input data.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Finds first suitable graphics context driver and initializes.
 *
 * Returns: graphics context driver if found, otherwise NULL.
 **/
const gfx_ctx_driver_t *video_context_driver_init_first(void *data,
      const char *ident, enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx, void **ctx_data)
{
   struct rarch_state *p_rarch = &rarch_st;
   int                       i = find_video_context_driver_index(ident);

   if (i >= 0)
   {
      const gfx_ctx_driver_t *ctx = video_context_driver_init(data, gfx_ctx_drivers[i], ident,
            api, major, minor, hw_render_ctx, ctx_data);
      if (ctx)
      {
         p_rarch->video_context_data = *ctx_data;
         return ctx;
      }
   }

   for (i = 0; gfx_ctx_drivers[i]; i++)
   {
      const gfx_ctx_driver_t *ctx =
         video_context_driver_init(data, gfx_ctx_drivers[i], ident,
            api, major, minor, hw_render_ctx, ctx_data);

      if (ctx)
      {
         p_rarch->video_context_data = *ctx_data;
         return ctx;
      }
   }

   return NULL;
}

bool video_context_driver_write_to_image_buffer(gfx_ctx_image_t *img)
{
   struct rarch_state   *p_rarch = &rarch_st;
   if (
            p_rarch->current_video_context.image_buffer_write
         && p_rarch->current_video_context.image_buffer_write(
            p_rarch->video_context_data,
            img->frame, img->width, img->height, img->pitch,
            img->rgb32, img->index, img->handle))
      return true;
   return false;
}

bool video_context_driver_get_video_output_prev(void)
{
   struct rarch_state   *p_rarch = &rarch_st;
   if (!p_rarch->current_video_context.get_video_output_prev)
      return false;
   p_rarch->current_video_context.get_video_output_prev(
         p_rarch->video_context_data);
   return true;
}

bool video_context_driver_get_video_output_next(void)
{
   struct rarch_state   *p_rarch = &rarch_st;
   if (!p_rarch->current_video_context.get_video_output_next)
      return false;
   p_rarch->current_video_context.get_video_output_next(
         p_rarch->video_context_data);
   return true;
}

void video_context_driver_translate_aspect(gfx_ctx_aspect_t *aspect)
{
   struct rarch_state   *p_rarch = &rarch_st;
   if (!p_rarch->video_context_data || !aspect || !p_rarch->current_video_context.translate_aspect)
      return;
   *aspect->aspect = p_rarch->current_video_context.translate_aspect(
         p_rarch->video_context_data, aspect->width, aspect->height);
}

void video_context_driver_free(void)
{
   struct rarch_state   *p_rarch = &rarch_st;
   if (p_rarch->current_video_context.destroy)
      p_rarch->current_video_context.destroy(p_rarch->video_context_data);
   video_context_driver_destroy();
   p_rarch->video_context_data    = NULL;
}

bool video_context_driver_get_video_output_size(gfx_ctx_size_t *size_data)
{
   struct rarch_state   *p_rarch = &rarch_st;
   if (!size_data)
      return false;
   if (!p_rarch->current_video_context.get_video_output_size)
      return false;
   p_rarch->current_video_context.get_video_output_size(
         p_rarch->video_context_data,
         size_data->width, size_data->height);
   return true;
}

bool video_context_driver_get_metrics(gfx_ctx_metrics_t *metrics)
{
   struct rarch_state *p_rarch   = &rarch_st;
   if (
         p_rarch->current_video_context.get_metrics(
            p_rarch->video_context_data,
            metrics->type,
            metrics->value))
      return true;
   return false;
}

bool video_context_driver_get_refresh_rate(float *refresh_rate)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->current_video_context.get_refresh_rate || !refresh_rate)
      return false;
   if (!p_rarch->video_context_data)
      return false;

   if (!p_rarch->video_driver_crt_switching_active)
   {
      if (refresh_rate)
         *refresh_rate =
             p_rarch->current_video_context.get_refresh_rate(
                   p_rarch->video_context_data);
   }
   else
   {
      float refresh_holder      = 0;
      if (refresh_rate)
         refresh_holder         =
             p_rarch->current_video_context.get_refresh_rate(
                   p_rarch->video_context_data);

      /* Fix for incorrect interlacing detection -- 
       * HARD SET VSNC TO REQUIRED REFRESH FOR CRT*/
      if (refresh_holder != p_rarch->video_driver_core_hz)
         *refresh_rate          = p_rarch->video_driver_core_hz;
   }

   return true;
}

bool video_context_driver_input_driver(gfx_ctx_input_t *inp)
{
   struct rarch_state   *p_rarch = &rarch_st;
   settings_t *settings          = p_rarch->configuration_settings;
   const char *joypad_name       = settings->arrays.input_joypad_driver;

   if (!p_rarch->current_video_context.input_driver)
      return false;
   p_rarch->current_video_context.input_driver(
         p_rarch->video_context_data, joypad_name,
         inp->input, inp->input_data);
   return true;
}

bool video_context_driver_get_ident(gfx_ctx_ident_t *ident)
{
   struct rarch_state      *p_rarch = &rarch_st;
   if (!ident)
      return false;
   ident->ident = p_rarch->current_video_context.ident;
   return true;
}

bool video_context_driver_set_video_mode(gfx_ctx_mode_t *mode_info)
{
   struct rarch_state      *p_rarch = &rarch_st;
   if (!p_rarch->current_video_context.set_video_mode)
      return false;
   return p_rarch->current_video_context.set_video_mode(
         p_rarch->video_context_data, mode_info->width,
         mode_info->height, mode_info->fullscreen);
}

bool video_context_driver_get_video_size(gfx_ctx_mode_t *mode_info)
{
   struct rarch_state      *p_rarch = &rarch_st;
   if (!p_rarch->current_video_context.get_video_size)
      return false;
   p_rarch->current_video_context.get_video_size(p_rarch->video_context_data,
         &mode_info->width, &mode_info->height);
   return true;
}

bool video_context_driver_get_flags(gfx_ctx_flags_t *flags)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->current_video_context.get_flags)
      return false;

   if (p_rarch->deferred_video_context_driver_set_flags)
   {
      flags->flags                                     = 
         p_rarch->deferred_flag_data.flags;
      p_rarch->deferred_video_context_driver_set_flags = false;
      return true;
   }

   flags->flags = p_rarch->current_video_context.get_flags(
         p_rarch->video_context_data);
   return true;
}

bool video_driver_get_flags(gfx_ctx_flags_t *flags)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->video_driver_poke || !p_rarch->video_driver_poke->get_flags)
      return false;
   flags->flags = p_rarch->video_driver_poke->get_flags(p_rarch->video_driver_data);
   return true;
}

/**
 * video_driver_test_all_flags:
 * @testflag          : flag to test
 *
 * Poll both the video and context driver's flags and test
 * whether @testflag is set or not.
 **/
bool video_driver_test_all_flags(enum display_flags testflag)
{
   gfx_ctx_flags_t flags;

   if (video_driver_get_flags(&flags))
      if (BIT32_GET(flags.flags, testflag))
         return true;

   if (video_context_driver_get_flags(&flags))
      if (BIT32_GET(flags.flags, testflag))
         return true;

   return false;
}

bool video_context_driver_set_flags(gfx_ctx_flags_t *flags)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!flags)
      return false;

   if (!p_rarch->current_video_context.set_flags)
   {
      p_rarch->deferred_flag_data.flags                = flags->flags;
      p_rarch->deferred_video_context_driver_set_flags = true;
      return false;
   }

   p_rarch->current_video_context.set_flags(
         p_rarch->video_context_data, flags->flags);
   return true;
}

enum gfx_ctx_api video_context_driver_get_api(void)
{
   struct rarch_state      *p_rarch = &rarch_st;
   enum gfx_ctx_api         ctx_api = p_rarch->video_context_data ?
      p_rarch->current_video_context.get_api(
            p_rarch->video_context_data) : GFX_CTX_NONE;

   if (ctx_api == GFX_CTX_NONE)
   {
      const char *video_ident  = (p_rarch->current_video) 
         ? p_rarch->current_video->ident 
         : NULL;
      if (string_is_equal(video_ident, "d3d9"))
         return GFX_CTX_DIRECT3D9_API;
      else if (string_is_equal(video_ident, "d3d10"))
         return GFX_CTX_DIRECT3D10_API;
      else if (string_is_equal(video_ident, "d3d11"))
         return GFX_CTX_DIRECT3D11_API;
      else if (string_is_equal(video_ident, "d3d12"))
         return GFX_CTX_DIRECT3D12_API;
      else if (string_is_equal(video_ident, "gx2"))
         return GFX_CTX_GX2_API;
      else if (string_is_equal(video_ident, "gx"))
         return GFX_CTX_GX_API;
      else if (string_is_equal(video_ident, "gl"))
         return GFX_CTX_OPENGL_API;
      else if (string_is_equal(video_ident, "gl1"))
         return GFX_CTX_OPENGL_API;
      else if (string_is_equal(video_ident, "glcore"))
         return GFX_CTX_OPENGL_API;
      else if (string_is_equal(video_ident, "vulkan"))
         return GFX_CTX_VULKAN_API;
      else if (string_is_equal(video_ident, "metal"))
         return GFX_CTX_METAL_API;
      else if (string_is_equal(video_ident, "network"))
         return GFX_CTX_NETWORK_VIDEO_API;

      return GFX_CTX_NONE;
   }

   return ctx_api;
}

bool video_driver_has_windowed(void)
{
#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
   struct rarch_state      *p_rarch = &rarch_st;
   if (p_rarch->video_driver_data && p_rarch->current_video->has_windowed)
      return p_rarch->current_video->has_windowed(p_rarch->video_driver_data);
   else if (p_rarch->video_context_data)
      return p_rarch->current_video_context.has_windowed;
#endif
   return false;
}

bool video_driver_cached_frame_has_valid_framebuffer(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->frame_cache_data)
      return (p_rarch->frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID);
   return false;
}


bool video_shader_driver_get_current_shader(video_shader_ctx_t *shader)
{
   struct rarch_state              *p_rarch = &rarch_st;
   void *video_driver                       = video_driver_get_ptr_internal(true);
   const video_poke_interface_t *video_poke = p_rarch->video_driver_poke;

   shader->data = NULL;
   if (!video_poke || !video_driver || !video_poke->get_current_shader)
      return false;
   shader->data = video_poke->get_current_shader(video_driver);
   return true;
}

float video_driver_get_refresh_rate(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (p_rarch->video_driver_poke && p_rarch->video_driver_poke->get_refresh_rate)
      return p_rarch->video_driver_poke->get_refresh_rate(p_rarch->video_driver_data);

   return 0.0f;
}

#if defined(HAVE_GFX_WIDGETS)
static bool video_driver_has_widgets(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->current_video && p_rarch->current_video->gfx_widgets_enabled
      && p_rarch->current_video->gfx_widgets_enabled(p_rarch->video_driver_data);
}
#endif

void video_driver_set_gpu_device_string(const char *str)
{
   struct rarch_state *p_rarch = &rarch_st;
   strlcpy(p_rarch->video_driver_gpu_device_string, str,
         sizeof(p_rarch->video_driver_gpu_device_string));
}

const char* video_driver_get_gpu_device_string(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->video_driver_gpu_device_string;
}

void video_driver_set_gpu_api_version_string(const char *str)
{
   struct rarch_state *p_rarch = &rarch_st;
   strlcpy(p_rarch->video_driver_gpu_api_version_string, str,
         sizeof(p_rarch->video_driver_gpu_api_version_string));
}

const char* video_driver_get_gpu_api_version_string(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->video_driver_gpu_api_version_string;
}

/* string list stays owned by the caller and must be available at
 * all times after the video driver is inited */
void video_driver_set_gpu_api_devices(
      enum gfx_ctx_api api, struct string_list *list)
{
   int i;

   for (i = 0; i < ARRAY_SIZE(gpu_map); i++)
   {
      if (api == gpu_map[i].api)
      {
         gpu_map[i].list = list;
         break;
      }
   }
}

struct string_list* video_driver_get_gpu_api_devices(enum gfx_ctx_api api)
{
   int i;

   for (i = 0; i < ARRAY_SIZE(gpu_map); i++)
   {
      if (api == gpu_map[i].api)
         return gpu_map[i].list;
   }

   return NULL;
}


/* LOCATION */

/**
 * location_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to location driver at index. Can be NULL
 * if nothing found.
 **/
static const void *location_driver_find_handle(int idx)
{
   const void *drv = location_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * config_get_location_driver_options:
 *
 * Get an enumerated list of all location driver names,
 * separated by '|'.
 *
 * Returns: string listing of all location driver names,
 * separated by '|'.
 **/
const char* config_get_location_driver_options(void)
{
   return char_list_new_special(STRING_LIST_LOCATION_DRIVERS, NULL);
}

static void find_location_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   struct rarch_state  *p_rarch = &rarch_st;
   settings_t         *settings = p_rarch->configuration_settings;

   drv.label                    = "location_driver";
   drv.s                        = settings->arrays.location_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i                            = (int)drv.len;

   if (i >= 0)
      p_rarch->location_driver  = (const location_driver_t*)location_driver_find_handle(i);
   else
   {

      if (verbosity_is_enabled())
      {
         unsigned d;
         RARCH_ERR("Couldn't find any location driver named \"%s\"\n",
               settings->arrays.location_driver);
         RARCH_LOG_OUTPUT("Available location drivers are:\n");
         for (d = 0; location_driver_find_handle(d); d++)
            RARCH_LOG_OUTPUT("\t%s\n", location_drivers[d]->ident);

         RARCH_WARN("Going to default to first location driver...\n");
      }

      p_rarch->location_driver = (const location_driver_t*)location_driver_find_handle(0);

      if (!p_rarch->location_driver)
         retroarch_fail(1, "find_location_driver()");
   }
}

/**
 * driver_location_start:
 *
 * Starts location driver interface..
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool driver_location_start(void)
{
   struct rarch_state  *p_rarch = &rarch_st;
   if (     p_rarch->location_driver 
         && p_rarch->location_data
         && p_rarch->location_driver->start)
   {
      settings_t *settings = p_rarch->configuration_settings;
      bool location_allow  = settings->bools.location_allow;
      if (location_allow)
         return p_rarch->location_driver->start(p_rarch->location_data);

      runloop_msg_queue_push("Location is explicitly disabled.\n",
            1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
            MESSAGE_QUEUE_CATEGORY_INFO);
   }
   return false;
}

/**
 * driver_location_stop:
 *
 * Stops location driver interface..
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static void driver_location_stop(void)
{
   struct rarch_state  *p_rarch = &rarch_st;
   if (     p_rarch->location_driver 
         && p_rarch->location_driver->stop
         && p_rarch->location_data)
      p_rarch->location_driver->stop(p_rarch->location_data);
}

/**
 * driver_location_set_interval:
 * @interval_msecs     : Interval time in milliseconds.
 * @interval_distance  : Distance at which to update.
 *
 * Sets interval update time for location driver interface.
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 **/
static void driver_location_set_interval(unsigned interval_msecs,
      unsigned interval_distance)
{
   struct rarch_state  *p_rarch = &rarch_st;
   if (     p_rarch->location_driver 
         && p_rarch->location_driver->set_interval
         && p_rarch->location_data)
      p_rarch->location_driver->set_interval(p_rarch->location_data,
            interval_msecs, interval_distance);
}

/**
 * driver_location_get_position:
 * @lat                : Latitude of current position.
 * @lon                : Longitude of current position.
 * @horiz_accuracy     : Horizontal accuracy.
 * @vert_accuracy      : Vertical accuracy.
 *
 * Gets current positioning information from
 * location driver interface.
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 *
 * Returns: bool (1) if successful, otherwise false (0).
 **/
static bool driver_location_get_position(double *lat, double *lon,
      double *horiz_accuracy, double *vert_accuracy)
{
   struct rarch_state  *p_rarch = &rarch_st;
   if (     p_rarch->location_driver 
         && p_rarch->location_driver->get_position
         && p_rarch->location_data)
      return p_rarch->location_driver->get_position(p_rarch->location_data,
            lat, lon, horiz_accuracy, vert_accuracy);

   *lat            = 0.0;
   *lon            = 0.0;
   *horiz_accuracy = 0.0;
   *vert_accuracy  = 0.0;
   return false;
}

static void init_location(void)
{
   struct rarch_state  *p_rarch = &rarch_st;
   rarch_system_info_t *system  = &p_rarch->runloop_system;

   /* Resource leaks will follow if location interface is initialized twice. */
   if (p_rarch->location_data)
      return;

   find_location_driver();

   p_rarch->location_data = p_rarch->location_driver->init();

   if (!p_rarch->location_data)
   {
      RARCH_ERR("Failed to initialize location driver. Will continue without location.\n");
      p_rarch->location_driver_active = false;
   }

   if (system->location_cb.initialized)
      system->location_cb.initialized();
}

static void uninit_location(void)
{
   struct rarch_state  *p_rarch = &rarch_st;
   rarch_system_info_t  *system = &p_rarch->runloop_system;

   if (p_rarch->location_data && p_rarch->location_driver)
   {
      if (system->location_cb.deinitialized)
         system->location_cb.deinitialized();

      if (p_rarch->location_driver->free)
         p_rarch->location_driver->free(p_rarch->location_data);
   }

   p_rarch->location_data = NULL;
}

/* CAMERA */

/**
 * camera_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to camera driver at index. Can be NULL
 * if nothing found.
 **/
static const void *camera_driver_find_handle(int idx)
{
   const void *drv = camera_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * config_get_camera_driver_options:
 *
 * Get an enumerated list of all camera driver names,
 * separated by '|'.
 *
 * Returns: string listing of all camera driver names,
 * separated by '|'.
 **/
const char *config_get_camera_driver_options(void)
{
   return char_list_new_special(STRING_LIST_CAMERA_DRIVERS, NULL);
}

static bool driver_camera_start(void)
{
   struct rarch_state  *p_rarch = &rarch_st;
   if (  p_rarch->camera_driver && 
         p_rarch->camera_data   &&
         p_rarch->camera_driver->start)
   {
      settings_t *settings = p_rarch->configuration_settings;
      bool camera_allow    = settings->bools.camera_allow;
      if (camera_allow)
         return p_rarch->camera_driver->start(p_rarch->camera_data);

      runloop_msg_queue_push(
            "Camera is explicitly disabled.\n", 1, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
   return true;
}

static void driver_camera_stop(void)
{
   struct rarch_state  *p_rarch = &rarch_st;
   if (     p_rarch->camera_driver
         && p_rarch->camera_driver->stop
         && p_rarch->camera_data)
      p_rarch->camera_driver->stop(p_rarch->camera_data);
}

static void camera_driver_find_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   struct rarch_state  *p_rarch = &rarch_st;
   settings_t         *settings = p_rarch->configuration_settings;

   drv.label = "camera_driver";
   drv.s     = settings->arrays.camera_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i         = (int)drv.len;

   if (i >= 0)
      p_rarch->camera_driver = (const camera_driver_t*)camera_driver_find_handle(i);
   else
   {
      if (verbosity_is_enabled())
      {
         unsigned d;
         RARCH_ERR("Couldn't find any camera driver named \"%s\"\n",
               settings->arrays.camera_driver);
         RARCH_LOG_OUTPUT("Available camera drivers are:\n");
         for (d = 0; camera_driver_find_handle(d); d++)
            RARCH_LOG_OUTPUT("\t%s\n", camera_drivers[d]->ident);

         RARCH_WARN("Going to default to first camera driver...\n");
      }

      p_rarch->camera_driver = (const camera_driver_t*)camera_driver_find_handle(0);

      if (!p_rarch->camera_driver)
         retroarch_fail(1, "find_camera_driver()");
   }
}

/* DRIVERS */

/**
 * find_driver_nonempty:
 * @label              : string of driver type to be found.
 * @i                  : index of driver.
 * @str                : identifier name of the found driver
 *                       gets written to this string.
 * @len                : size of @str.
 *
 * Find driver based on @label.
 *
 * Returns: NULL if no driver based on @label found, otherwise
 * pointer to driver.
 **/
static const void *find_driver_nonempty(const char *label, int i,
      char *s, size_t len)
{
   if (string_is_equal(label, "camera_driver"))
   {
      if (camera_driver_find_handle(i))
      {
         strlcpy(s, camera_drivers[i]->ident, len);
         return camera_drivers[i];
      }
   }
   else if (string_is_equal(label, "location_driver"))
   {
      if (location_driver_find_handle(i))
      {
         strlcpy(s, location_drivers[i]->ident, len);
         return location_drivers[i];
      }
   }
#ifdef HAVE_MENU
   else if (string_is_equal(label, "menu_driver"))
   {
      if (menu_driver_find_handle(i))
      {
         strlcpy(s, menu_driver_find_ident(i), len);
         return menu_driver_find_handle(i);
      }
   }
#endif
   else if (string_is_equal(label, "input_driver"))
   {
      if (input_driver_find_handle(i))
      {
         strlcpy(s, input_drivers[i]->ident, len);
         return input_drivers[i];
      }
   }
   else if (string_is_equal(label, "input_joypad_driver"))
   {
      if (joypad_driver_find_handle(i))
      {
         strlcpy(s, joypad_drivers[i]->ident, len);
         return joypad_drivers[i];
      }
   }
   else if (string_is_equal(label, "video_driver"))
   {
      if (video_driver_find_handle(i))
      {
         strlcpy(s, video_drivers[i]->ident, len);
         return video_drivers[i];
      }
   }
   else if (string_is_equal(label, "audio_driver"))
   {
      if (audio_driver_find_handle(i))
      {
         strlcpy(s, audio_drivers[i]->ident, len);
         return audio_drivers[i];
      }
   }
   else if (string_is_equal(label, "record_driver"))
   {
      if (record_driver_find_handle(i))
      {
         strlcpy(s, record_drivers[i]->ident, len);
         return record_drivers[i];
      }
   }
   else if (string_is_equal(label, "midi_driver"))
   {
      if (midi_driver_find_handle(i))
      {
         strlcpy(s, midi_drivers[i]->ident, len);
         return midi_drivers[i];
      }
   }
   else if (string_is_equal(label, "audio_resampler_driver"))
   {
      if (audio_resampler_driver_find_handle(i))
      {
         strlcpy(s, audio_resampler_driver_find_ident(i), len);
         return audio_resampler_driver_find_handle(i);
      }
   }
   else if (string_is_equal(label, "wifi_driver"))
   {
      if (wifi_driver_find_handle(i))
      {
         strlcpy(s, wifi_drivers[i]->ident, len);
         return wifi_drivers[i];
      }
   }

   return NULL;
}

/**
 * driver_find_index:
 * @label              : string of driver type to be found.
 * @drv                : identifier of driver to be found.
 *
 * Find index of the driver, based on @label.
 *
 * Returns: -1 if no driver based on @label and @drv found, otherwise
 * index number of the driver found in the array.
 **/
static int driver_find_index(const char * label, const char *drv)
{
   unsigned i;
   char str[256];

   str[0] = '\0';

   for (i = 0;
         find_driver_nonempty(label, i, str, sizeof(str)) != NULL; i++)
   {
      if (string_is_empty(str))
         break;
      if (string_is_equal_noncase(drv, str))
         return i;
   }

   return -1;
}

/**
 * driver_find_last:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find last driver in driver array.
 **/
static bool driver_find_last(const char *label, char *s, size_t len)
{
   unsigned i;

   for (i = 0;
         find_driver_nonempty(label, i, s, len) != NULL; i++)
   {}

   if (i)
      i = i - 1;
   else
      i = 0;

   find_driver_nonempty(label, i, s, len);
   return true;
}

/**
 * driver_find_prev:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find previous driver in driver array.
 **/
static bool driver_find_prev(const char *label, char *s, size_t len)
{
   int i = driver_find_index(label, s);

   if (i > 0)
   {
      find_driver_nonempty(label, i - 1, s, len);
      return true;
   }

   RARCH_WARN(
         "Couldn't find any previous driver (current one: \"%s\").\n", s);
   return false;
}

/**
 * driver_find_next:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find next driver in driver array.
 **/
static bool driver_find_next(const char *label, char *s, size_t len)
{
   int i = driver_find_index(label, s);

   if (i >= 0 && string_is_not_equal(s, "null"))
   {
      find_driver_nonempty(label, i + 1, s, len);
      return true;
   }

   RARCH_WARN("%s (current one: \"%s\").\n",
         msg_hash_to_str(MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER),
         s);
   return false;
}

static void driver_adjust_system_rates(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;

   audio_driver_monitor_adjust_system_rates();

   p_rarch->runloop_force_nonblock = false;

   video_driver_monitor_adjust_system_rates();

   if (!video_driver_get_ptr_internal(false))
      return;

   if (p_rarch->runloop_force_nonblock)
   {
      bool video_adaptive_vsync    = settings->bools.video_adaptive_vsync;
      unsigned video_swap_interval = settings->uints.video_swap_interval;

      if (p_rarch->current_video->set_nonblock_state)
         p_rarch->current_video->set_nonblock_state(p_rarch->video_driver_data, true,
               video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC) &&
               video_adaptive_vsync,
               video_swap_interval
               );
   }
   else
      driver_set_nonblock_state();
}

/**
 * driver_set_nonblock_state:
 *
 * Sets audio and video drivers to nonblock state (if enabled).
 *
 * If nonblock state is false, sets
 * blocking state for both audio and video drivers instead.
 **/
void driver_set_nonblock_state(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   bool                 enable = p_rarch->input_driver_nonblock_state;
   settings_t       *settings  = p_rarch->configuration_settings;
   bool audio_sync             = settings->bools.audio_sync;
   bool video_vsync            = settings->bools.video_vsync;
   bool adaptive_vsync         = settings->bools.video_adaptive_vsync;
   unsigned swap_interval      = settings->uints.video_swap_interval;
   bool video_driver_active    = p_rarch->video_driver_active;
   bool audio_driver_active    = p_rarch->audio_driver_active;
   bool runloop_force_nonblock = p_rarch->runloop_force_nonblock;

   /* Only apply non-block-state for video if we're using vsync. */
   if (video_driver_active && video_driver_get_ptr_internal(false))
   {
      if (p_rarch->current_video->set_nonblock_state)
      {
         bool video_nonblock        = enable;
         if (!video_vsync || runloop_force_nonblock)
            video_nonblock = true;
         p_rarch->current_video->set_nonblock_state(p_rarch->video_driver_data,
               video_nonblock,
               video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC) &&
               adaptive_vsync, swap_interval);
      }
   }

   if (audio_driver_active && p_rarch->audio_driver_context_audio_data)
      p_rarch->current_audio->set_nonblock_state(
            p_rarch->audio_driver_context_audio_data,
            audio_sync ? enable : true);

   p_rarch->audio_driver_chunk_size = enable
      ? p_rarch->audio_driver_chunk_nonblock_size
      : p_rarch->audio_driver_chunk_block_size;
}

/**
 * drivers_init:
 * @flags              : Bitmask of drivers to initialize.
 *
 * Initializes drivers.
 * @flags determines which drivers get initialized.
 **/
static void drivers_init(int flags)
{
   struct rarch_state *p_rarch = &rarch_st;
   bool video_is_threaded      = video_driver_is_threaded_internal();
   settings_t *settings        = p_rarch->configuration_settings;
#if defined(HAVE_GFX_WIDGETS)
   bool menu_enable_widgets    = settings->bools.menu_enable_widgets;

   /* By default, we want display widgets to persist through driver reinits. */
   gfx_widgets_set_persistence(true);
#endif

#ifdef HAVE_MENU
   /* By default, we want the menu to persist through driver reinits. */
   menu_driver_ctl(RARCH_MENU_CTL_SET_OWN_DRIVER, NULL);
#endif

   if (flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK))
      driver_adjust_system_rates();

   /* Initialize video driver */
   if (flags & DRIVER_VIDEO_MASK)
   {
      struct retro_hw_render_callback *hwr   =
         video_driver_get_hw_context_internal();

      p_rarch->video_driver_frame_time_count = 0;

      video_driver_lock_new();
      video_driver_filter_free();
      video_driver_set_cached_frame_ptr(NULL);
      video_driver_init_internal(&video_is_threaded);

      if (!p_rarch->video_driver_cache_context_ack
            && hwr->context_reset)
         hwr->context_reset();
      p_rarch->video_driver_cache_context_ack = false;
      p_rarch->runloop_frame_time_last        = 0;
   }

   /* Initialize audio driver */
   if (flags & DRIVER_AUDIO_MASK)
   {
      audio_driver_init_internal(p_rarch->audio_callback.callback != NULL);
      if (  p_rarch->current_audio && 
            p_rarch->current_audio->device_list_new &&
            p_rarch->audio_driver_context_audio_data)
         p_rarch->audio_driver_devices_list = (struct string_list*)
            p_rarch->current_audio->device_list_new(
                  p_rarch->audio_driver_context_audio_data);
   }

   if (flags & DRIVER_CAMERA_MASK)
   {
      /* Only initialize camera driver if we're ever going to use it. */
      if (p_rarch->camera_driver_active)
      {
         /* Resource leaks will follow if camera is initialized twice. */
         if (!p_rarch->camera_data)
         {
            camera_driver_find_driver();

            if (p_rarch->camera_driver)
            {
               p_rarch->camera_data = p_rarch->camera_driver->init(
                     *settings->arrays.camera_device ?
                     settings->arrays.camera_device : NULL,
                     p_rarch->camera_cb.caps,
                     settings->uints.camera_width ?
                     settings->uints.camera_width : p_rarch->camera_cb.width,
                     settings->uints.camera_height ?
                     settings->uints.camera_height : p_rarch->camera_cb.height);

               if (!p_rarch->camera_data)
               {
                  RARCH_ERR("Failed to initialize camera driver. Will continue without camera.\n");
                  p_rarch->camera_driver_active = false;
               }

               if (p_rarch->camera_cb.initialized)
                  p_rarch->camera_cb.initialized();
            }
         }
      }
   }

   if (flags & DRIVER_LOCATION_MASK)
   {
      /* Only initialize location driver if we're ever going to use it. */
      if (p_rarch->location_driver_active)
         init_location();
   }

   core_info_init_current_core();

#if defined(HAVE_GFX_WIDGETS)
   if (menu_enable_widgets && video_driver_has_widgets())
   {
      bool rarch_force_fullscreen = p_rarch->rarch_force_fullscreen;
      bool video_is_fullscreen    = settings->bools.video_fullscreen ||
            rarch_force_fullscreen;

      gfx_widgets_init(video_is_threaded,
            p_rarch->video_driver_width,
            p_rarch->video_driver_height,
            video_is_fullscreen,
            settings->paths.directory_assets,
            settings->paths.path_font);
   }
   else
#endif
   {
      gfx_display_init_first_driver(video_is_threaded);
   }

#ifdef HAVE_MENU
   if (flags & DRIVER_VIDEO_MASK)
   {
      /* Initialize menu driver */
      if (flags & DRIVER_MENU_MASK)
         if (!menu_driver_init(video_is_threaded))
             RARCH_ERR("Unable to init menu driver.\n");
   }

   /* Initialising the menu driver will also initialise
    * core info - if we are not initialising the menu
    * driver, must initialise core info 'by hand' */
   if (!(flags & DRIVER_VIDEO_MASK) ||
       !(flags & DRIVER_MENU_MASK))
   {
      command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
      command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
   }

#else
   /* Qt uses core info, even if the menu is disabled */
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
   command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
#endif

   if (flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK))
   {
      /* Keep non-throttled state as good as possible. */
      if (p_rarch->input_driver_nonblock_state)
         driver_set_nonblock_state();
   }

   /* Initialize LED driver */
   if (flags & DRIVER_LED_MASK)
      led_driver_init(settings->arrays.led_driver);

   /* Initialize MIDI  driver */
   if (flags & DRIVER_MIDI_MASK)
      midi_driver_init();
}

/**
 * Driver ownership - set this to true if the platform in question needs to 'own'
 * the respective handle and therefore skip regular RetroArch
 * driver teardown/reiniting procedure.
 *
 * If  to true, the 'free' function will get skipped. It is
 * then up to the driver implementation to properly handle
 * 'reiniting' inside the 'init' function and make sure it
 * returns the existing handle instead of allocating and
 * returning a pointer to a new handle.
 *
 * Typically, if a driver intends to make use of this, it should
 * set this to true at the end of its 'init' function.
 **/
static void driver_uninit(int flags)
{
   struct rarch_state      *p_rarch = &rarch_st;

   core_info_deinit_list();
   core_info_free_current_core();

#if defined(HAVE_GFX_WIDGETS)
   /* This absolutely has to be done before video_driver_free_internal()
    * is called/completes, otherwise certain menu drivers
    * (e.g. Vulkan) will segfault */
   gfx_widgets_deinit();
#endif

#ifdef HAVE_MENU
   if (flags & DRIVER_MENU_MASK)
   {
      menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL);
   }
#endif

   if ((flags & DRIVER_LOCATION_MASK))
      uninit_location();

   if ((flags & DRIVER_CAMERA_MASK))
   {
      if (p_rarch->camera_data && p_rarch->camera_driver)
      {
         if (p_rarch->camera_cb.deinitialized)
            p_rarch->camera_cb.deinitialized();

         if (p_rarch->camera_driver->free)
            p_rarch->camera_driver->free(p_rarch->camera_data);
      }

      p_rarch->camera_data = NULL;
   }

   if ((flags & DRIVER_WIFI_MASK))
      wifi_driver_ctl(RARCH_WIFI_CTL_DEINIT, NULL);

   if (flags & DRIVER_LED)
      led_driver_free();

   if (flags & DRIVERS_VIDEO_INPUT)
   {
      video_driver_free_internal();
      video_driver_lock_free();
      p_rarch->video_driver_data = NULL;
      video_driver_set_cached_frame_ptr(NULL);
   }

   if (flags & DRIVER_AUDIO_MASK)
      audio_driver_deinit();

   if ((flags & DRIVER_VIDEO_MASK))
      p_rarch->video_driver_data = NULL;

   if ((flags & DRIVER_INPUT_MASK))
      p_rarch->current_input_data = NULL;

   if ((flags & DRIVER_AUDIO_MASK))
      p_rarch->audio_driver_context_audio_data = NULL;

   if (flags & DRIVER_MIDI_MASK)
      midi_driver_free();
}

static void retroarch_deinit_drivers(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   struct retro_callbacks *cbs = &p_rarch->retro_ctx;

#if defined(HAVE_GFX_WIDGETS)
   /* Tear down display widgets no matter what
    * in case the handle is lost in the threaded
    * video driver in the meantime
    * (breaking video_driver_has_widgets) */
   gfx_widgets_deinit();
#endif

   /* Video */
   video_display_server_destroy();
   crt_video_restore();

   p_rarch->video_driver_use_rgba                   = false;
   p_rarch->video_driver_active                     = false;
   p_rarch->video_driver_cache_context              = false;
   p_rarch->video_driver_cache_context_ack          = false;
   p_rarch->video_driver_record_gpu_buffer          = NULL;
   p_rarch->current_video                           = NULL;
   video_driver_set_cached_frame_ptr(NULL);

   /* Audio */
   p_rarch->audio_driver_active                     = false;
   p_rarch->current_audio                           = NULL;

   /* Input */
   p_rarch->input_driver_keyboard_linefeed_enable   = false;
   p_rarch->input_driver_block_hotkey               = false;
   p_rarch->input_driver_block_libretro_input       = false;
   p_rarch->input_driver_nonblock_state             = false;
   p_rarch->input_driver_flushing_input             = 0;
   memset(&p_rarch->input_driver_turbo_btns, 0, sizeof(turbo_buttons_t));
   p_rarch->current_input                           = NULL;

#ifdef HAVE_MENU
   menu_driver_destroy();
   p_rarch->menu_driver_alive                       = false;
#endif
   p_rarch->location_driver_active                  = false;
   p_rarch->location_driver                         = NULL;

   /* Camera */
   p_rarch->camera_driver_active                    = false;
   p_rarch->camera_driver                           = NULL;
   p_rarch->camera_data                             = NULL;

   wifi_driver_ctl(RARCH_WIFI_CTL_DESTROY, NULL);

   cbs->frame_cb                                    = retro_frame_null;
   cbs->poll_cb                                     = retro_input_poll_null;
   cbs->sample_cb                                   = NULL;
   cbs->sample_batch_cb                             = NULL;
   cbs->state_cb                                    = NULL;

   p_rarch->current_core.inited                     = false;
}

bool driver_ctl(enum driver_ctl_state state, void *data)
{
   switch (state)
   {
      case RARCH_DRIVER_CTL_SET_REFRESH_RATE:
         {
            float *hz = (float*)data;
            video_monitor_set_refresh_rate(*hz);
            audio_driver_monitor_set_rate();
            driver_adjust_system_rates();
         }
         break;
      case RARCH_DRIVER_CTL_FIND_FIRST:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            find_driver_nonempty(drv->label, 0, drv->s, drv->len);
         }
         break;
      case RARCH_DRIVER_CTL_FIND_LAST:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            return driver_find_last(drv->label, drv->s, drv->len);
         }
      case RARCH_DRIVER_CTL_FIND_PREV:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            return driver_find_prev(drv->label, drv->s, drv->len);
         }
      case RARCH_DRIVER_CTL_FIND_NEXT:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            return driver_find_next(drv->label, drv->s, drv->len);
         }
      case RARCH_DRIVER_CTL_FIND_INDEX:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            drv->len = driver_find_index(drv->label, drv->s);
         }
         break;
      case RARCH_DRIVER_CTL_NONE:
      default:
         break;
   }

   return true;
}

/* RUNAHEAD */

#ifdef HAVE_RUNAHEAD
static void mylist_resize(my_list *list, int new_size, bool run_constructor)
{
   int i;
   int new_capacity;
   int old_size;
   void *element    = NULL;
   if (new_size < 0)
      new_size  = 0;
   if (!list)
      return;
   new_capacity = new_size;
   old_size     = list->size;

   if (new_size == old_size)
      return;

   if (new_size > list->capacity)
   {
      if (new_capacity < list->capacity * 2)
         new_capacity = list->capacity * 2;

      /* try to realloc */
      list->data = (void**)realloc(
            (void*)list->data, new_capacity * sizeof(void*));

      for (i = list->capacity; i < new_capacity; i++)
         list->data[i] = NULL;

      list->capacity = new_capacity;
   }

   if (new_size <= list->size)
   {
      for (i = new_size; i < list->size; i++)
      {
         element = list->data[i];

         if (element)
         {
            list->destructor(element);
            list->data[i] = NULL;
         }
      }
   }
   else
   {
      for (i = list->size; i < new_size; i++)
      {
         list->data[i] = NULL;
         if (run_constructor)
            list->data[i] = list->constructor();
      }
   }

   list->size = new_size;
}

static void *mylist_add_element(my_list *list)
{
   int old_size;

   if (!list)
      return NULL;

   old_size = list->size;
   mylist_resize(list, old_size + 1, true);
   return list->data[old_size];
}

static void mylist_destroy(my_list **list_p)
{
   my_list *list = NULL;
   if (!list_p)
      return;

   list = *list_p;

   if (list)
   {
      mylist_resize(list, 0, false);
      free(list->data);
      free(list);
      *list_p = NULL;
   }
}

static void mylist_create(my_list **list_p, int initial_capacity,
      constructor_t constructor, destructor_t destructor)
{
   my_list *list        = NULL;

   if (!list_p)
      return;

   if (initial_capacity < 0)
      initial_capacity = 0;

   list                = *list_p;
   if (list)
      mylist_destroy(list_p);

   list               = (my_list*)malloc(sizeof(my_list));
   *list_p            = list;
   list->size         = 0;
   list->constructor  = constructor;
   list->destructor   = destructor;

   if (initial_capacity > 0)
   {
      list->data      = (void**)calloc(initial_capacity, sizeof(void*));
      list->capacity  = initial_capacity;
   }
   else
   {
      list->data      = NULL;
      list->capacity  = 0;
   }
}

static void *input_list_element_constructor(void)
{
   void *ptr                   = calloc(1, sizeof(input_list_element));
   input_list_element *element = (input_list_element*)ptr;

   element->state_size         = 256;
   element->state              = (int16_t*)calloc(
         element->state_size, sizeof(int16_t));

   return ptr;
}

static void input_list_element_realloc(
      input_list_element *element,
      unsigned int new_size)
{
   if (new_size > element->state_size)
   {
      element->state = (int16_t*)realloc(element->state,
            new_size * sizeof(int16_t));
      memset(&element->state[element->state_size], 0,
            (new_size - element->state_size) * sizeof(int16_t));
      element->state_size = new_size;
   }
}

static void input_list_element_expand(
      input_list_element *element, unsigned int new_index)
{
   unsigned int new_size = element->state_size;
   if (new_size == 0)
      new_size = 32;
   while (new_index >= new_size)
      new_size *= 2;
   input_list_element_realloc(element, new_size);
}

static void input_list_element_destructor(void* element_ptr)
{
   input_list_element *element = (input_list_element*)element_ptr;
   if (!element)
      return;

   free(element->state);
   free(element_ptr);
}

static void input_state_set_last(unsigned port, unsigned device,
      unsigned index, unsigned id, int16_t value)
{
   unsigned i;
   input_list_element *element = NULL;
   struct rarch_state *p_rarch = &rarch_st;

   if (!p_rarch->input_state_list)
      mylist_create(&p_rarch->input_state_list, 16,
            input_list_element_constructor,
            input_list_element_destructor);

   /* Find list item */
   for (i = 0; i < (unsigned)p_rarch->input_state_list->size; i++)
   {
      element = (input_list_element*)p_rarch->input_state_list->data[i];
      if (  (element->port   == port)   &&
            (element->device == device) &&
            (element->index  == index)
         )
      {
         if (id >= element->state_size)
            input_list_element_expand(element, id);
         element->state[id] = value;
         return;
      }
   }

   element            = (input_list_element*)
      mylist_add_element(p_rarch->input_state_list);
   element->port      = port;
   element->device    = device;
   element->index     = index;
   if (id >= element->state_size)
      input_list_element_expand(element, id);
   element->state[id] = value;
}

static int16_t input_state_get_last(unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   unsigned i;
   struct rarch_state      *p_rarch = &rarch_st;

   if (!p_rarch->input_state_list)
      return 0;

   /* find list item */
   for (i = 0; i < (unsigned)p_rarch->input_state_list->size; i++)
   {
      input_list_element *element =
         (input_list_element*)p_rarch->input_state_list->data[i];

      if (  (element->port   == port)   &&
            (element->device == device) &&
            (element->index  == index))
      {
         if (id < element->state_size)
            return element->state[id];
         return 0;
      }
   }
   return 0;
}

static int16_t input_state_with_logging(unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   struct rarch_state      *p_rarch = &rarch_st;

   if (p_rarch->input_state_callback_original)
   {
      int16_t result              = p_rarch->input_state_callback_original(
            port, device, index, id);
      int16_t last_input          = 
         input_state_get_last(port, device, index, id);
      if (result != last_input)
         p_rarch->input_is_dirty  = true;
      /*arbitrary limit of up to 65536 elements in state array*/
      if (id < 65536)
         input_state_set_last(port, device, index, id, result);

      return result;
   }
   return 0;
}

static void reset_hook(void)
{
   struct rarch_state     *p_rarch = &rarch_st;

   p_rarch->input_is_dirty         = true;

   if (p_rarch->retro_reset_callback_original)
      p_rarch->retro_reset_callback_original();
}

static bool unserialize_hook(const void *buf, size_t size)
{
   struct rarch_state     *p_rarch = &rarch_st;

   p_rarch->input_is_dirty         = true;

   if (p_rarch->retro_unserialize_callback_original)
      return p_rarch->retro_unserialize_callback_original(buf, size);
   return false;
}

static void add_input_state_hook(void)
{  
   struct rarch_state      *p_rarch = &rarch_st;
   struct retro_callbacks *cbs      = &p_rarch->retro_ctx;

   if (!p_rarch->input_state_callback_original)
   {
      p_rarch->input_state_callback_original = cbs->state_cb;
      cbs->state_cb                          = input_state_with_logging;
      p_rarch->current_core.retro_set_input_state(cbs->state_cb);
   }

   if (!p_rarch->retro_reset_callback_original)
   {
      p_rarch->retro_reset_callback_original = p_rarch->current_core.retro_reset;
      p_rarch->current_core.retro_reset      = reset_hook;
   }

   if (!p_rarch->retro_unserialize_callback_original)
   {
      p_rarch->retro_unserialize_callback_original = p_rarch->current_core.retro_unserialize;
      p_rarch->current_core.retro_unserialize      = unserialize_hook;
   }
}

static void remove_input_state_hook(void)
{
   struct rarch_state      *p_rarch = &rarch_st;
   struct retro_callbacks *cbs      = &p_rarch->retro_ctx;

   if (p_rarch->input_state_callback_original)
   {
      cbs->state_cb                 = p_rarch->input_state_callback_original;
      p_rarch->current_core.retro_set_input_state(cbs->state_cb);
      p_rarch->input_state_callback_original = NULL;
      mylist_destroy(&p_rarch->input_state_list);
   }

   if (p_rarch->retro_reset_callback_original)
   {
      p_rarch->current_core.retro_reset               = 
         p_rarch->retro_reset_callback_original;
      p_rarch->retro_reset_callback_original          = NULL;
   }

   if (p_rarch->retro_unserialize_callback_original)
   {
      p_rarch->current_core.retro_unserialize                = 
         p_rarch->retro_unserialize_callback_original;
      p_rarch->retro_unserialize_callback_original           = NULL;
   }
}

static void *runahead_save_state_alloc(void)
{
   struct rarch_state *p_rarch           = &rarch_st;
   retro_ctx_serialize_info_t *savestate = (retro_ctx_serialize_info_t*)
      malloc(sizeof(retro_ctx_serialize_info_t));

   if (!savestate)
      return NULL;

   savestate->data          = NULL;
   savestate->data_const    = NULL;
   savestate->size          = 0;

   if (  (p_rarch->runahead_save_state_size > 0) && 
         p_rarch->runahead_save_state_size_known)
   {
      savestate->data       = malloc(p_rarch->runahead_save_state_size);
      savestate->data_const = savestate->data;
      savestate->size       = p_rarch->runahead_save_state_size;
   }

   return savestate;
}

static void runahead_save_state_free(void *data)
{
   retro_ctx_serialize_info_t *savestate = (retro_ctx_serialize_info_t*)data;
   if (!savestate)
      return;
   free(savestate->data);
   free(savestate);
}

static void runahead_save_state_list_init(size_t save_state_size)
{
   struct rarch_state             *p_rarch = &rarch_st;

   p_rarch->runahead_save_state_size       = save_state_size;
   p_rarch->runahead_save_state_size_known = true;

   mylist_create(&p_rarch->runahead_save_state_list, 16,
         runahead_save_state_alloc, runahead_save_state_free);
}

/* Hooks - Hooks to cleanup, and add dirty input hooks */
static void runahead_remove_hooks(void)
{
   struct rarch_state *p_rarch       = &rarch_st;

   if (p_rarch->original_retro_deinit)
   {
      p_rarch->current_core.retro_deinit = p_rarch->original_retro_deinit;
      p_rarch->original_retro_deinit     = NULL;
   }

   if (p_rarch->original_retro_unload)
   {
      p_rarch->current_core.retro_unload_game = p_rarch->original_retro_unload;
      p_rarch->original_retro_unload          = NULL;
   }
   remove_input_state_hook();
}

static void runahead_clear_variables(void)
{
   struct rarch_state *p_rarch                = &rarch_st;

   p_rarch->runahead_save_state_size          = 0;
   p_rarch->runahead_save_state_size_known    = false;
   runahead_video_driver_is_active            = true;
   runahead_available                         = true;
   runahead_secondary_core_available          = true;
   runahead_force_input_dirty                 = true;
   p_rarch->runahead_last_frame_count         = 0;
}

static void runahead_destroy(void)
{
   struct rarch_state *p_rarch                = &rarch_st;

   mylist_destroy(&p_rarch->runahead_save_state_list);
   runahead_remove_hooks();
   runahead_clear_variables();
}

static void unload_hook(void)
{
   struct rarch_state *p_rarch = &rarch_st;

   runahead_remove_hooks();
   runahead_destroy();
   secondary_core_destroy();
   if (p_rarch->current_core.retro_unload_game)
      p_rarch->current_core.retro_unload_game();
   p_rarch->core_poll_type_override = POLL_TYPE_OVERRIDE_DONTCARE;
}

static void runahead_deinit_hook(void)
{
   struct rarch_state *p_rarch     = &rarch_st;

   runahead_remove_hooks();
   runahead_destroy();
   secondary_core_destroy();
   if (p_rarch->current_core.retro_deinit)
      p_rarch->current_core.retro_deinit();
}

static void runahead_add_hooks(void)
{
   struct rarch_state *p_rarch = &rarch_st;

   if (!p_rarch->original_retro_deinit)
   {
      p_rarch->original_retro_deinit     = p_rarch->current_core.retro_deinit;
      p_rarch->current_core.retro_deinit = runahead_deinit_hook;
   }

   if (!p_rarch->original_retro_unload)
   {
      p_rarch->original_retro_unload          = p_rarch->current_core.retro_unload_game;
      p_rarch->current_core.retro_unload_game = unload_hook;
   }
   add_input_state_hook();
}

/* Runahead Code */

static void runahead_error(void)
{
   struct rarch_state *p_rarch             = &rarch_st;

   runahead_available                      = false;
   mylist_destroy(&p_rarch->runahead_save_state_list);
   runahead_remove_hooks();
   p_rarch->runahead_save_state_size       = 0;
   p_rarch->runahead_save_state_size_known = true;
}

static bool runahead_create(void)
{
   /* get savestate size and allocate buffer */
   retro_ctx_size_info_t info;
   struct rarch_state *p_rarch     = &rarch_st;

   p_rarch->request_fast_savestate = true;
   core_serialize_size(&info);
   p_rarch->request_fast_savestate = false;

   runahead_save_state_list_init(info.size);
   runahead_video_driver_is_active = p_rarch->video_driver_active;

   if (  (p_rarch->runahead_save_state_size == 0) || 
         !p_rarch->runahead_save_state_size_known)
   {
      runahead_error();
      return false;
   }

   runahead_add_hooks();
   runahead_force_input_dirty = true;
   mylist_resize(p_rarch->runahead_save_state_list, 1, true);
   return true;
}

static bool runahead_save_state(void)
{
   retro_ctx_serialize_info_t *serialize_info;
   struct rarch_state     *p_rarch = &rarch_st;
   bool okay                       = false;

   if (!p_rarch->runahead_save_state_list)
      return false;

   serialize_info                  =
      (retro_ctx_serialize_info_t*)p_rarch->runahead_save_state_list->data[0];

   p_rarch->request_fast_savestate = true;
   okay                            = core_serialize(serialize_info);
   p_rarch->request_fast_savestate = false;

   if (okay)
      return true;

   runahead_error();
   return false;
}

static bool runahead_load_state(void)
{
   struct rarch_state                *p_rarch = &rarch_st;
   bool okay                                  = false;
   retro_ctx_serialize_info_t *serialize_info = (retro_ctx_serialize_info_t*)
      p_rarch->runahead_save_state_list->data[0];
   bool last_dirty                            = p_rarch->input_is_dirty;

   p_rarch->request_fast_savestate            = true;
   /* calling core_unserialize has side effects with
    * netplay (it triggers transmitting your save state)
      call retro_unserialize directly from the core instead */
   okay = p_rarch->current_core.retro_unserialize(
         serialize_info->data_const, serialize_info->size);

   p_rarch->request_fast_savestate            = false;
   p_rarch->input_is_dirty                    = last_dirty;

   if (!okay)
      runahead_error();

   return okay;
}

#if HAVE_DYNAMIC
static bool runahead_load_state_secondary(void)
{
   struct rarch_state                *p_rarch = &rarch_st;
   bool okay                                  = false;
   retro_ctx_serialize_info_t *serialize_info =
      (retro_ctx_serialize_info_t*)p_rarch->runahead_save_state_list->data[0];

   p_rarch->request_fast_savestate            = true;
   okay                                       = secondary_core_deserialize(
         serialize_info->data_const, (int)serialize_info->size);
   p_rarch->request_fast_savestate            = false;

   if (!okay)
   {
      runahead_secondary_core_available = false;
      runahead_error();
      return false;
   }

   return true;
}
#endif

static bool runahead_core_run_use_last_input(void)
{
   struct rarch_state *p_rarch            = &rarch_st;
   struct retro_callbacks *cbs            = &p_rarch->retro_ctx;
   retro_input_poll_t old_poll_function   = cbs->poll_cb;
   retro_input_state_t old_input_function = cbs->state_cb;

   cbs->poll_cb                           = retro_input_poll_null;
   cbs->state_cb                          = input_state_get_last;

   p_rarch->current_core.retro_set_input_poll(cbs->poll_cb);
   p_rarch->current_core.retro_set_input_state(cbs->state_cb);

   p_rarch->current_core.retro_run();

   cbs->poll_cb                           = old_poll_function;
   cbs->state_cb                          = old_input_function;

   p_rarch->current_core.retro_set_input_poll(cbs->poll_cb);
   p_rarch->current_core.retro_set_input_state(cbs->state_cb);

   return true;
}

static void do_runahead(int runahead_count, bool use_secondary)
{
   int frame_number        = 0;
   bool last_frame         = false;
   bool suspended_frame    = false;
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   const bool have_dynamic = true;
#else
   const bool have_dynamic = false;
#endif
   struct rarch_state 
      *p_rarch             = &rarch_st;
   uint64_t frame_count    = p_rarch->video_driver_frame_count;

   if (runahead_count <= 0 || !runahead_available)
      goto force_input_dirty;

   if (!p_rarch->runahead_save_state_size_known)
   {
      if (!runahead_create())
      {
         settings_t *settings        = p_rarch->configuration_settings;
         bool runahead_hide_warnings = settings->bools.run_ahead_hide_warnings;

         if (!runahead_hide_warnings)
            runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES), 0, 2 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         goto force_input_dirty;
      }
   }

   /* Check for GUI */
   /* Hack: If we were in the GUI, force a resync. */
   if (frame_count != p_rarch->runahead_last_frame_count + 1)
      runahead_force_input_dirty = true;

   p_rarch->runahead_last_frame_count = frame_count;

   if (!use_secondary || !have_dynamic || !runahead_secondary_core_available)
   {
      /* TODO: multiple savestates for higher performance
       * when not using secondary core */
      for (frame_number = 0; frame_number <= runahead_count; frame_number++)
      {
         last_frame      = frame_number == runahead_count;
         suspended_frame = !last_frame;

         if (suspended_frame)
         {
            p_rarch->audio_suspended     = true;
            p_rarch->video_driver_active = false;
         }

         if (frame_number == 0)
            core_run();
         else
            runahead_core_run_use_last_input();

         if (suspended_frame)
         {
            runahead_resume_video();
            p_rarch->audio_suspended     = false;
         }

         if (frame_number == 0)
         {
            if (!runahead_save_state())
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_SAVE_STATE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               return;
            }
         }

         if (last_frame)
         {
            if (!runahead_load_state())
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_LOAD_STATE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               return;
            }
         }
      }
   }
   else
   {
#if HAVE_DYNAMIC
      if (!secondary_core_ensure_exists())
      {
         secondary_core_destroy();
         runahead_secondary_core_available = false;
         runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         goto force_input_dirty;
      }

      /* run main core with video suspended */
      p_rarch->video_driver_active     = false;
      core_run();
      runahead_resume_video();

      if (p_rarch->input_is_dirty || runahead_force_input_dirty)
      {
         p_rarch->input_is_dirty       = false;

         if (!runahead_save_state())
         {
            runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_SAVE_STATE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            return;
         }

         if (!runahead_load_state_secondary())
         {
            runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_LOAD_STATE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            return;
         }

         for (frame_number = 0; frame_number < runahead_count - 1; frame_number++)
         {
            p_rarch->video_driver_active = false;
            p_rarch->audio_suspended     = true;
            p_rarch->hard_disable_audio  = true;
            runahead_run_secondary();
            p_rarch->hard_disable_audio  = false;
            p_rarch->audio_suspended     = false;
            runahead_resume_video();
         }
      }
      p_rarch->audio_suspended           = true;
      p_rarch->hard_disable_audio        = true;
      runahead_run_secondary();
      p_rarch->hard_disable_audio        = false;
      p_rarch->audio_suspended           = false;
#endif
   }
   runahead_force_input_dirty = false;
   return;

force_input_dirty:
   core_run();
   runahead_force_input_dirty = true;
}
#endif

static retro_time_t rarch_core_runtime_tick(retro_time_t current_time)
{
   struct rarch_state          *p_rarch = &rarch_st;
   retro_time_t frame_time              =
      (1.0 / p_rarch->video_driver_av_info.timing.fps) * 1000000;
   bool runloop_slowmotion              = p_rarch->runloop_slowmotion;
   bool runloop_fastmotion              = p_rarch->runloop_fastmotion;

   /* Account for slow motion */
   if (runloop_slowmotion)
   {
      settings_t *settings              = p_rarch->configuration_settings;
      float slowmotion_ratio            = settings->floats.slowmotion_ratio;
      return (retro_time_t)((double)frame_time * slowmotion_ratio);
   }

   /* Account for fast forward */
   if (runloop_fastmotion)
   {
      /* Doing it this way means we miss the first frame after
       * turning fast forward on, but it saves the overhead of
       * having to do:
       *    retro_time_t current_usec = cpu_features_get_time_usec();
       *    libretro_core_runtime_last = current_usec;
       * every frame when fast forward is off. */
      retro_time_t current_usec           = current_time;
      retro_time_t potential_frame_time   = current_usec -
         p_rarch->libretro_core_runtime_last;
      p_rarch->libretro_core_runtime_last = current_usec;

      if (potential_frame_time < frame_time)
         return potential_frame_time;
   }

   return frame_time;
}

static void retroarch_print_features(void)
{
   char buf[2048];
   buf[0] = '\0';
   frontend_driver_attach_console();

   strlcat(buf, "\nFeatures:\n", sizeof(buf));

   _PSUPP_BUF(buf, SUPPORTS_LIBRETRODB,      "LibretroDB",      "LibretroDB support");
   _PSUPP_BUF(buf, SUPPORTS_COMMAND,         "Command",         "Command interface support");
   _PSUPP_BUF(buf, SUPPORTS_NETWORK_COMMAND, "Network Command", "Network Command interface "
         "support");
   _PSUPP_BUF(buf, SUPPORTS_SDL,             "SDL",             "SDL input/audio/video drivers");
   _PSUPP_BUF(buf, SUPPORTS_SDL2,            "SDL2",            "SDL2 input/audio/video drivers");
   _PSUPP_BUF(buf, SUPPORTS_X11,             "X11",             "X11 input/video drivers");
   _PSUPP_BUF(buf, SUPPORTS_WAYLAND,         "wayland",         "Wayland input/video drivers");
   _PSUPP_BUF(buf, SUPPORTS_THREAD,          "Threads",         "Threading support");
   _PSUPP_BUF(buf, SUPPORTS_VULKAN,          "Vulkan",          "Vulkan video driver");
   _PSUPP_BUF(buf, SUPPORTS_METAL,           "Metal",           "Metal video driver");
   _PSUPP_BUF(buf, SUPPORTS_OPENGL,          "OpenGL",          "OpenGL   video driver support");
   _PSUPP_BUF(buf, SUPPORTS_OPENGLES,        "OpenGL ES",       "OpenGLES video driver support");
   _PSUPP_BUF(buf, SUPPORTS_XVIDEO,          "XVideo",          "Video driver");
   _PSUPP_BUF(buf, SUPPORTS_UDEV,            "UDEV",            "UDEV/EVDEV input driver support");
   _PSUPP_BUF(buf, SUPPORTS_EGL,             "EGL",             "Video context driver");
   _PSUPP_BUF(buf, SUPPORTS_KMS,             "KMS",             "Video context driver");
   _PSUPP_BUF(buf, SUPPORTS_VG,              "OpenVG",          "Video context driver");
   _PSUPP_BUF(buf, SUPPORTS_COREAUDIO,       "CoreAudio",       "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_COREAUDIO3,      "CoreAudioV3",     "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_ALSA,            "ALSA",            "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_OSS,             "OSS",             "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_JACK,            "Jack",            "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_RSOUND,          "RSound",          "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_ROAR,            "RoarAudio",       "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_PULSE,           "PulseAudio",      "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_DSOUND,          "DirectSound",     "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_WASAPI,          "WASAPI",     "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_XAUDIO,          "XAudio2",         "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_AL,              "OpenAL",          "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_SL,              "OpenSL",          "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_7ZIP,            "7zip",            "7zip extraction support");
   _PSUPP_BUF(buf, SUPPORTS_ZLIB,            "zlib",            ".zip extraction support");
   _PSUPP_BUF(buf, SUPPORTS_DYLIB,           "External",        "External filter and plugin support");
   _PSUPP_BUF(buf, SUPPORTS_CG,              "Cg",              "Fragment/vertex shader driver");
   _PSUPP_BUF(buf, SUPPORTS_GLSL,            "GLSL",            "Fragment/vertex shader driver");
   _PSUPP_BUF(buf, SUPPORTS_HLSL,            "HLSL",            "Fragment/vertex shader driver");
   _PSUPP_BUF(buf, SUPPORTS_SDL_IMAGE,       "SDL_image",       "SDL_image image loading");
   _PSUPP_BUF(buf, SUPPORTS_RPNG,            "rpng",            "PNG image loading/encoding");
   _PSUPP_BUF(buf, SUPPORTS_RJPEG,            "rjpeg",           "JPEG image loading");
   _PSUPP_BUF(buf, SUPPORTS_DYNAMIC,         "Dynamic",         "Dynamic run-time loading of "
                                              "libretro library");
   _PSUPP_BUF(buf, SUPPORTS_FFMPEG,          "FFmpeg",          "On-the-fly recording of gameplay "
                                              "with libavcodec");
   _PSUPP_BUF(buf, SUPPORTS_FREETYPE,        "FreeType",        "TTF font rendering driver");
   _PSUPP_BUF(buf, SUPPORTS_CORETEXT,        "CoreText",        "TTF font rendering driver ");
   _PSUPP_BUF(buf, SUPPORTS_NETPLAY,         "Netplay",         "Peer-to-peer netplay");
   _PSUPP_BUF(buf, SUPPORTS_PYTHON,          "Python",          "Script support in shaders");
   _PSUPP_BUF(buf, SUPPORTS_LIBUSB,          "Libusb",          "Libusb support");
   _PSUPP_BUF(buf, SUPPORTS_COCOA,           "Cocoa",           "Cocoa UI companion support "
                                              "(for OSX and/or iOS)");
   _PSUPP_BUF(buf, SUPPORTS_QT,              "Qt",              "Qt UI companion support");
   _PSUPP_BUF(buf, SUPPORTS_V4L2,            "Video4Linux2",    "Camera driver");

   puts(buf);
}

static void retroarch_print_version(void)
{
   char str[255];
   frontend_driver_attach_console();
   str[0] = '\0';

   fprintf(stderr, "%s: %s -- v%s",
         msg_hash_to_str(MSG_PROGRAM),
         msg_hash_to_str(MSG_LIBRETRO_FRONTEND),
         PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
   printf(" -- %s --\n", retroarch_git_version);
#else
   printf("\n");
#endif
   retroarch_get_capabilities(RARCH_CAPABILITIES_COMPILER, str, sizeof(str));
   strlcat(str, " Built: " __DATE__, sizeof(str));
   fprintf(stdout, "%s\n", str);
}

/**
 * retroarch_print_help:
 *
 * Prints help message explaining the program's commandline switches.
 **/
static void retroarch_print_help(const char *arg0)
{
   frontend_driver_attach_console();
   puts("===================================================================");
   retroarch_print_version();
   puts("===================================================================");

   printf("Usage: %s [OPTIONS]... [FILE]\n", arg0);

   {
      char buf[2148];
      buf[0] = '\0';

      strlcpy(buf, "  -h, --help            Show this help message.\n", sizeof(buf));
      strlcat(buf, "  -v, --verbose         Verbose logging.\n",        sizeof(buf));
      strlcat(buf, "      --log-file FILE   Log messages to FILE.\n",   sizeof(buf));
      strlcat(buf, "      --version         Show version.\n",           sizeof(buf));
      strlcat(buf, "      --features        Prints available features compiled into "
            "program.\n", sizeof(buf));

#ifdef HAVE_MENU
      strlcat(buf, "      --menu            Do not require content or libretro core to "
            "be loaded,\n"
            "                        starts directly in menu. If no arguments "
            "are passed to\n"
            "                        the program, it is equivalent to using "
            "--menu as only argument.\n", sizeof(buf));
#endif

      strlcat(buf, "  -s, --save=PATH       Path for save files (*.srm). (DEPRECATED, use --appendconfig and savefile_directory)\n", sizeof(buf));
      strlcat(buf, "  -S, --savestate=PATH  Path for the save state files (*.state). (DEPRECATED, use --apendconfig and savestate_directory)\n", sizeof(buf));
      strlcat(buf, "      --set-shader PATH Path to a shader (preset) that will be loaded each time content is loaded.\n"
            "                        Effectively overrides automatic shader presets.\n"
            "                        An empty argument \"\" will disable automatic shader presets.\n", sizeof(buf));
      strlcat(buf, "  -f, --fullscreen      Start the program in fullscreen regardless "
            "of config settings.\n", sizeof(buf));
#ifdef HAVE_CONFIGFILE
#ifdef _WIN32
      strlcat(buf, "  -c, --config=FILE     Path for config file."
            "\n\t\tDefaults to retroarch.cfg in same directory as retroarch.exe."
            "\n\t\tIf a default config is not found, the program will attempt to "
            "create one.\n"
            , sizeof(buf));
#else
      strlcat(buf, "  -c, --config=FILE     Path for config file."
            "\n\t\tBy default looks for config in $XDG_CONFIG_HOME/retroarch/"
            "retroarch.cfg,\n\t\t$HOME/.config/retroarch/retroarch.cfg,\n\t\t"
            "and $HOME/.retroarch.cfg.\n\t\tIf a default config is not found, "
            "the program will attempt to create one based on the \n\t\t"
            "skeleton config (" GLOBAL_CONFIG_DIR "/retroarch.cfg). \n"
            , sizeof(buf));
#endif
#endif
      strlcat(buf, "      --appendconfig=FILE\n"
            "                        Extra config files are loaded in, "
            "and take priority over\n"
            "                        config selected in -c (or default). "
            "Multiple configs are\n"
            "                        delimited by '|'.\n", sizeof(buf));
#ifdef HAVE_DYNAMIC
      strlcat(buf, "  -L, --libretro=FILE   Path to libretro implementation. "
            "Overrides any config setting.\n", sizeof(buf));
#endif
      strlcat(buf, "      --subsystem=NAME  Use a subsystem of the libretro core. "
            "Multiple content\n"
            "                        files are loaded as multiple arguments. "
            "If a content\n"
            "                        file is skipped, use a blank (\"\") "
            "command line argument.\n"
            "                        Content must be loaded in an order "
            "which depends on the\n"
            "                        particular subsystem used. See verbose "
            "log output to learn\n"
            "                        how a particular subsystem wants content "
            "to be loaded.\n", sizeof(buf));
      puts(buf);
   }

   printf("  -N, --nodevice=PORT\n"
          "                        Disconnects controller device connected "
          "to PORT (1 to %d).\n", MAX_USERS);
   printf("  -A, --dualanalog=PORT\n"
          "                        Connect a DualAnalog controller to PORT "
          "(1 to %d).\n", MAX_USERS);
   printf("  -d, --device=PORT:ID\n"
          "                        Connect a generic device into PORT of "
          "the device (1 to %d).\n", MAX_USERS);

   {
      char buf[2148];
      buf[0] = '\0';
      strlcpy(buf, "                        Format is PORT:ID, where ID is a number "
            "corresponding to the particular device.\n", sizeof(buf));
      strlcat(buf, "  -P, --bsvplay=FILE    Playback a BSV movie file.\n", sizeof(buf));
      strlcat(buf, "  -R, --bsvrecord=FILE  Start recording a BSV movie file from "
            "the beginning.\n", sizeof(buf));
      strlcat(buf, "      --eof-exit        Exit upon reaching the end of the "
            "BSV movie file.\n", sizeof(buf));
      strlcat(buf, "  -M, --sram-mode=MODE  SRAM handling mode. MODE can be "
            "'noload-nosave',\n"
            "                        'noload-save', 'load-nosave' or "
            "'load-save'.\n"
            "                        Note: 'noload-save' implies that "
            "save files *WILL BE OVERWRITTEN*.\n", sizeof(buf));
#ifdef HAVE_NETWORKING
      strlcat(buf, "  -H, --host            Host netplay as user 1.\n", sizeof(buf));
      strlcat(buf, "  -C, --connect=HOST    Connect to netplay server as user 2.\n", sizeof(buf));
      strlcat(buf, "      --port=PORT       Port used to netplay. Default is 55435.\n", sizeof(buf));
      strlcat(buf, "      --stateless       Use \"stateless\" mode for netplay\n", sizeof(buf));
      strlcat(buf, "                        (requires a very fast network).\n", sizeof(buf));
      strlcat(buf, "      --check-frames=NUMBER\n"
            "                        Check frames when using netplay.\n", sizeof(buf));
#ifdef HAVE_NETWORK_CMD
      strlcat(buf, "      --command         Sends a command over UDP to an already "
            "running program process.\n", sizeof(buf));
      strlcat(buf, "      Available commands are listed if command is invalid.\n", sizeof(buf));
#endif

#endif

      strlcat(buf, "      --nick=NICK       Picks a username (for use with netplay). "
            "Not mandatory.\n", sizeof(buf));
      strlcat(buf, "  -r, --record=FILE     Path to record video file.\n        "
            "Using .mkv extension is recommended.\n", sizeof(buf));
      strlcat(buf, "      --recordconfig    Path to settings used during recording.\n", sizeof(buf));
      strlcat(buf, "      --size=WIDTHxHEIGHT\n"
            "                        Overrides output video size when recording.\n", sizeof(buf));
      strlcat(buf, "  -U, --ups=FILE        Specifies path for UPS patch that will be "
            "applied to content.\n", sizeof(buf));
      strlcat(buf, "      --bps=FILE        Specifies path for BPS patch that will be "
            "applied to content.\n", sizeof(buf));
      strlcat(buf, "      --ips=FILE        Specifies path for IPS patch that will be "
            "applied to content.\n", sizeof(buf));
      strlcat(buf, "      --no-patch        Disables all forms of content patching.\n", sizeof(buf));
      strlcat(buf, "  -D, --detach          Detach program from the running console. "
            "Not relevant for all platforms.\n", sizeof(buf));
      strlcat(buf, "      --max-frames=NUMBER\n"
            "                        Runs for the specified number of frames, "
            "then exits.\n", sizeof(buf));
      strlcat(buf, "      --max-frames-ss\n"
            "                        Takes a screenshot at the end of max-frames.\n", sizeof(buf));
      strlcat(buf, "      --max-frames-ss-path=FILE\n"
            "                        Path to save the screenshot to at the end of max-frames.\n", sizeof(buf));
      puts(buf);
   }
   printf("      --accessibility\n"
          "                        Enables accessibilty for blind users using text-to-speech.\n");
}

/**
 * retroarch_parse_input_and_config:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Parses (commandline) arguments passed to program and loads the config file,
 * with command line options overriding the config file.
 *
 **/
static void retroarch_parse_input_and_config(int argc, char *argv[])
{
   unsigned i;
   static bool           first_run = true;
   const char           *optstring = NULL;
   bool           explicit_menu    = false;
   struct rarch_state     *p_rarch = &rarch_st;
   global_t            *global     = &p_rarch->g_extern;

   const struct option opts[] = {
#ifdef HAVE_DYNAMIC
      { "libretro",           1, NULL, 'L' },
#endif
      { "menu",               0, NULL, RA_OPT_MENU },
      { "help",               0, NULL, 'h' },
      { "save",               1, NULL, 's' },
      { "fullscreen",         0, NULL, 'f' },
      { "record",             1, NULL, 'r' },
      { "recordconfig",       1, NULL, RA_OPT_RECORDCONFIG },
      { "size",               1, NULL, RA_OPT_SIZE },
      { "verbose",            0, NULL, 'v' },
#ifdef HAVE_CONFIGFILE
      { "config",             1, NULL, 'c' },
      { "appendconfig",       1, NULL, RA_OPT_APPENDCONFIG },
#endif
      { "nodevice",           1, NULL, 'N' },
      { "dualanalog",         1, NULL, 'A' },
      { "device",             1, NULL, 'd' },
      { "savestate",          1, NULL, 'S' },
      { "set-shader",         1, NULL, RA_OPT_SET_SHADER },
      { "bsvplay",            1, NULL, 'P' },
      { "bsvrecord",          1, NULL, 'R' },
      { "sram-mode",          1, NULL, 'M' },
#ifdef HAVE_NETWORKING
      { "host",               0, NULL, 'H' },
      { "connect",            1, NULL, 'C' },
      { "stateless",          0, NULL, RA_OPT_STATELESS },
      { "check-frames",       1, NULL, RA_OPT_CHECK_FRAMES },
      { "port",               1, NULL, RA_OPT_PORT },
#ifdef HAVE_NETWORK_CMD
      { "command",            1, NULL, RA_OPT_COMMAND },
#endif
#endif
      { "nick",               1, NULL, RA_OPT_NICK },
      { "ups",                1, NULL, 'U' },
      { "bps",                1, NULL, RA_OPT_BPS },
      { "ips",                1, NULL, RA_OPT_IPS },
      { "no-patch",           0, NULL, RA_OPT_NO_PATCH },
      { "detach",             0, NULL, 'D' },
      { "features",           0, NULL, RA_OPT_FEATURES },
      { "subsystem",          1, NULL, RA_OPT_SUBSYSTEM },
      { "max-frames",         1, NULL, RA_OPT_MAX_FRAMES },
      { "max-frames-ss",      0, NULL, RA_OPT_MAX_FRAMES_SCREENSHOT },
      { "max-frames-ss-path", 1, NULL, RA_OPT_MAX_FRAMES_SCREENSHOT_PATH },
      { "eof-exit",           0, NULL, RA_OPT_EOF_EXIT },
      { "version",            0, NULL, RA_OPT_VERSION },
      { "log-file",           1, NULL, RA_OPT_LOG_FILE },
      { "accessibility",      0, NULL, RA_OPT_ACCESSIBILITY},
      { NULL, 0, NULL, 0 }
   };

   if (first_run)
   {
      /* Copy the args into a buffer so launch arguments can be reused */
      for (i = 0; i < (unsigned)argc; i++)
      {
         strlcat(p_rarch->launch_arguments,
               argv[i], sizeof(p_rarch->launch_arguments));
         strlcat(p_rarch->launch_arguments, " ",
               sizeof(p_rarch->launch_arguments));
      }
      string_trim_whitespace_left(p_rarch->launch_arguments);
      string_trim_whitespace_right(p_rarch->launch_arguments);

      first_run = false;
   }

   /* Handling the core type is finicky. Based on the arguments we pass in,
    * we handle it differently.
    * Some current cases which track desired behavior and how it is supposed to work:
    *
    * Dynamically linked RA:
    * ./retroarch                            -> CORE_TYPE_DUMMY
    * ./retroarch -v                         -> CORE_TYPE_DUMMY + verbose
    * ./retroarch --menu                     -> CORE_TYPE_DUMMY
    * ./retroarch --menu -v                  -> CORE_TYPE_DUMMY + verbose
    * ./retroarch -L contentless-core        -> CORE_TYPE_PLAIN
    * ./retroarch -L content-core            -> CORE_TYPE_PLAIN + FAIL (This currently crashes)
    * ./retroarch [-L content-core] ROM      -> CORE_TYPE_PLAIN
    * ./retroarch <-L or ROM> --menu         -> FAIL
    *
    * The heuristic here seems to be that if we use the -L CLI option or
    * optind < argc at the end we should set CORE_TYPE_PLAIN.
    * To handle --menu, we should ensure that CORE_TYPE_DUMMY is still set
    * otherwise, fail early, since the CLI options are non-sensical.
    * We could also simply ignore --menu in this case to be more friendly with
    * bogus arguments.
    */

   if (!p_rarch->has_set_core)
      retroarch_set_current_core_type(CORE_TYPE_DUMMY, false);

   path_clear(RARCH_PATH_SUBSYSTEM);

   retroarch_override_setting_free_state();

   p_rarch->has_set_username             = false;
   rarch_ctl(RARCH_CTL_UNSET_UPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_IPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_BPS_PREF, NULL);
   *global->name.ups                     = '\0';
   *global->name.bps                     = '\0';
   *global->name.ips                     = '\0';

#ifdef HAVE_CONFIGFILE
   p_rarch->runloop_overrides_active     = false;
#endif

   /* Make sure we can call retroarch_parse_input several times ... */
   optind    = 0;
   optstring = "hs:fvS:A:U:DN:d:"
      BSV_MOVIE_ARG NETPLAY_ARG DYNAMIC_ARG FFMPEG_RECORD_ARG CONFIG_FILE_ARG;

#ifdef ORBIS
   argv      = &(argv[2]);
   argc      = argc - 2;
#endif

#ifndef HAVE_MENU
   if (argc == 1)
   {
      printf("%s\n", msg_hash_to_str(MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN));
      retroarch_print_help(argv[0]);
      exit(0);
   }
#endif

   /* First pass: Read the config file path and any directory overrides, so
    * they're in place when we load the config */
   if (argc)
   {
      for (;;)
      {
         int c = getopt_long(argc, argv, optstring, opts, NULL);

#if 0
         fprintf(stderr, "c is: %c (%d), optarg is: [%s]\n", c, c, string_is_empty(optarg) ? "" : optarg);
#endif

         if (c == -1)
            break;

         switch (c)
         {
            case 'h':
               retroarch_print_help(argv[0]);
               exit(0);

#ifdef HAVE_CONFIGFILE
            case 'c':
               path_set(RARCH_PATH_CONFIG, optarg);
               break;
            case RA_OPT_APPENDCONFIG:
               path_set(RARCH_PATH_CONFIG_APPEND, optarg);
               break;
#endif

            case 's':
               strlcpy(global->name.savefile, optarg,
                     sizeof(global->name.savefile));
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL);
               break;

            case 'S':
               strlcpy(global->name.savestate, optarg,
                     sizeof(global->name.savestate));
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
               break;

            /* Must handle '?' otherwise you get an infinite loop */
            case '?':
               retroarch_print_help(argv[0]);
               retroarch_fail(1, "retroarch_parse_input()");
               break;
            /* All other arguments are handled in the second pass */
         }
      }
   }

   /* Flush out some states that could have been set
    * by core environment variables. */
   p_rarch->current_core.has_set_input_descriptors = false;
   p_rarch->current_core.has_set_subsystems        = false;

   /* Load the config file now that we know what it is */
#ifdef HAVE_CONFIGFILE
   if (!p_rarch->rarch_block_config_read)
#endif
      config_load(&p_rarch->g_extern);

   /* Second pass: All other arguments override the config file */
   optind = 1;

   if (argc)
   {
      for (;;)
      {
         int c = getopt_long(argc, argv, optstring, opts, NULL);

         if (c == -1)
            break;

         switch (c)
         {
            case 'd':
               {
                  unsigned new_port;
                  unsigned id              = 0;
                  struct string_list *list = string_split(optarg, ":");
                  int    port              = 0;

                  if (list && list->size == 2)
                  {
                     port = (int)strtol(list->elems[0].data, NULL, 0);
                     id   = (unsigned)strtoul(list->elems[1].data, NULL, 0);
                  }
                  string_list_free(list);

                  if (port < 1 || port > MAX_USERS)
                  {
                     RARCH_ERR("%s\n", msg_hash_to_str(MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT));
                     retroarch_print_help(argv[0]);
                     retroarch_fail(1, "retroarch_parse_input()");
                  }
                  new_port = port -1;

                  input_config_set_device(new_port, id);

                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
               }
               break;

            case 'A':
               {
                  unsigned new_port;
                  int port = (int)strtol(optarg, NULL, 0);

                  if (port < 1 || port > MAX_USERS)
                  {
                     RARCH_ERR("Connect dualanalog to a valid port.\n");
                     retroarch_print_help(argv[0]);
                     retroarch_fail(1, "retroarch_parse_input()");
                  }
                  new_port = port - 1;

                  input_config_set_device(new_port, RETRO_DEVICE_ANALOG);
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
               }
               break;

            case 'f':
               p_rarch->rarch_force_fullscreen = true;
               break;

            case 'v':
               verbosity_enable();
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_VERBOSITY, NULL);
               break;

            case 'N':
               {
                  unsigned new_port;
                  int port = (int)strtol(optarg, NULL, 0);

                  if (port < 1 || port > MAX_USERS)
                  {
                     RARCH_ERR("%s\n",
                           msg_hash_to_str(MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT));
                     retroarch_print_help(argv[0]);
                     retroarch_fail(1, "retroarch_parse_input()");
                  }
                  new_port = port - 1;
                  input_config_set_device(port - 1, RETRO_DEVICE_NONE);
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
               }
               break;

            case 'r':
               strlcpy(global->record.path, optarg,
                     sizeof(global->record.path));
               if (p_rarch->recording_enable)
                  p_rarch->recording_enable = true;
               break;

            case RA_OPT_SET_SHADER:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
               /* disable auto-shaders */
               if (string_is_empty(optarg))
               {
                  p_rarch->cli_shader_disable = true;
                  break;
               }

               /* rebase on shader directory */
               if (!path_is_absolute(optarg))
               {
                  settings_t *settings = p_rarch->configuration_settings;
                  char       *ref_path = settings->paths.directory_video_shader;
                  fill_pathname_join(p_rarch->cli_shader,
                        ref_path, optarg, sizeof(p_rarch->cli_shader));
                  break;
               }

               strlcpy(p_rarch->cli_shader, optarg, sizeof(p_rarch->cli_shader));
#endif
               break;

   #ifdef HAVE_DYNAMIC
            case 'L':
               {
                  int path_stats = path_stat(optarg);

                  if ((path_stats & RETRO_VFS_STAT_IS_DIRECTORY) != 0)
                  {
                     settings_t *settings = p_rarch->configuration_settings;

                     path_clear(RARCH_PATH_CORE);

                     configuration_set_string(settings,
                     settings->paths.directory_libretro, optarg);

                     retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
                     retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY, NULL);
                     RARCH_WARN("Using old --libretro behavior. "
                           "Setting libretro_directory to \"%s\" instead.\n",
                           optarg);
                  }
                  else if ((path_stats & RETRO_VFS_STAT_IS_VALID) != 0)
                  {
                     path_set(RARCH_PATH_CORE, optarg);
                     retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);

                     /* We requested explicit core, so use PLAIN core type. */
                     retroarch_set_current_core_type(CORE_TYPE_PLAIN, false);
                  }
                  else
                  {
                     RARCH_WARN("--libretro argument \"%s\" is neither a file nor directory. Ignoring.\n",
                           optarg);
                  }
               }
               break;
   #endif
            case 'P':
               strlcpy(p_rarch->bsv_movie_state.movie_start_path, optarg,
                     sizeof(p_rarch->bsv_movie_state.movie_start_path));

               p_rarch->bsv_movie_state.movie_start_playback  = true;
               p_rarch->bsv_movie_state.movie_start_recording = false;
               break;
            case 'R':
               strlcpy(p_rarch->bsv_movie_state.movie_start_path, optarg,
                     sizeof(p_rarch->bsv_movie_state.movie_start_path));

               p_rarch->bsv_movie_state.movie_start_playback  = false;
               p_rarch->bsv_movie_state.movie_start_recording = true;
               break;

            case 'M':
               if (string_is_equal(optarg, "noload-nosave"))
               {
                  p_rarch->rarch_is_sram_load_disabled = true;
                  p_rarch->rarch_is_sram_save_disabled = true;
               }
               else if (string_is_equal(optarg, "noload-save"))
                  p_rarch->rarch_is_sram_load_disabled = true;
               else if (string_is_equal(optarg, "load-nosave"))
                  p_rarch->rarch_is_sram_save_disabled = true;
               else if (string_is_not_equal(optarg, "load-save"))
               {
                  RARCH_ERR("Invalid argument in --sram-mode.\n");
                  retroarch_print_help(argv[0]);
                  retroarch_fail(1, "retroarch_parse_input()");
               }
               break;

#ifdef HAVE_NETWORKING
            case 'H':
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_MODE, NULL);
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);
               break;

            case 'C':
               {
                  settings_t *settings = p_rarch->configuration_settings;
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_MODE, NULL);
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS, NULL);
                  netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);
                  configuration_set_string(settings,
                  settings->paths.netplay_server, optarg);
               }
               break;

            case RA_OPT_STATELESS:
               {
                  settings_t *settings = p_rarch->configuration_settings;

                  configuration_set_bool(settings,
                        settings->bools.netplay_stateless_mode, true);

                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE, NULL);
               }
               break;

            case RA_OPT_CHECK_FRAMES:
               {
                  settings_t *settings = p_rarch->configuration_settings;
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES, NULL);

                  configuration_set_int(settings,
                        settings->ints.netplay_check_frames,
                        (int)strtoul(optarg, NULL, 0));
               }
               break;

            case RA_OPT_PORT:
               {
                  settings_t *settings = p_rarch->configuration_settings;
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT, NULL);
                  configuration_set_uint(settings,
                        settings->uints.netplay_port,
                        (int)strtoul(optarg, NULL, 0));
               }
               break;

#ifdef HAVE_NETWORK_CMD
            case RA_OPT_COMMAND:
#ifdef HAVE_COMMAND
               if (command_network_send((const char*)optarg))
                  exit(0);
               else
                  retroarch_fail(1, "network_cmd_send()");
#endif
               break;
#endif

#endif

            case RA_OPT_BPS:
               strlcpy(global->name.bps, optarg,
                     sizeof(global->name.bps));
               p_rarch->rarch_bps_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_BPS_PREF, NULL);
               break;

            case 'U':
               strlcpy(global->name.ups, optarg,
                     sizeof(global->name.ups));
               p_rarch->rarch_ups_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_UPS_PREF, NULL);
               break;

            case RA_OPT_IPS:
               strlcpy(global->name.ips, optarg,
                     sizeof(global->name.ips));
               p_rarch->rarch_ips_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_IPS_PREF, NULL);
               break;

            case RA_OPT_NO_PATCH:
               p_rarch->rarch_patch_blocked = true;
               break;

            case 'D':
               frontend_driver_detach_console();
               break;

            case RA_OPT_MENU:
               explicit_menu = true;
               break;

            case RA_OPT_NICK:
               {
                  settings_t *settings      = p_rarch->configuration_settings;

                  p_rarch->has_set_username = true;

                  configuration_set_string(settings,
                  settings->paths.username, optarg);
               }
               break;

            case RA_OPT_SIZE:
               if (sscanf(optarg, "%ux%u",
                        &p_rarch->recording_width,
                        &p_rarch->recording_height) != 2)
               {
                  RARCH_ERR("Wrong format for --size.\n");
                  retroarch_print_help(argv[0]);
                  retroarch_fail(1, "retroarch_parse_input()");
               }
               break;

            case RA_OPT_RECORDCONFIG:
               strlcpy(global->record.config, optarg,
                     sizeof(global->record.config));
               break;

            case RA_OPT_MAX_FRAMES:
               p_rarch->runloop_max_frames  = (unsigned)strtoul(optarg, NULL, 10);
               break;

            case RA_OPT_MAX_FRAMES_SCREENSHOT:
               p_rarch->runloop_max_frames_screenshot = true;
               break;

            case RA_OPT_MAX_FRAMES_SCREENSHOT_PATH:
               strlcpy(p_rarch->runloop_max_frames_screenshot_path,
                     optarg,
                     sizeof(p_rarch->runloop_max_frames_screenshot_path));
               break;

            case RA_OPT_SUBSYSTEM:
               path_set(RARCH_PATH_SUBSYSTEM, optarg);
               break;

            case RA_OPT_FEATURES:
               retroarch_print_features();
               exit(0);

            case RA_OPT_EOF_EXIT:
               p_rarch->bsv_movie_state.eof_exit = true;
               break;

            case RA_OPT_VERSION:
               retroarch_print_version();
               exit(0);

            case RA_OPT_LOG_FILE:
               /* Enable 'log to file' */
               configuration_set_bool(p_rarch->configuration_settings,
                     p_rarch->configuration_settings->bools.log_to_file, true);

               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_LOG_TO_FILE, NULL);

               /* Cache log file path override */
               rarch_log_file_set_override(optarg);
               break;

            case 'h':
#ifdef HAVE_CONFIGFILE
            case 'c':
            case RA_OPT_APPENDCONFIG:
#endif
            case 's':
            case 'S':
               break; /* Handled in the first pass */

            case '?':
               retroarch_print_help(argv[0]);
               retroarch_fail(1, "retroarch_parse_input()");
            case RA_OPT_ACCESSIBILITY:
#ifdef HAVE_ACCESSIBILITY
               p_rarch->accessibility_enabled = true;
#endif
               break;
            default:
               RARCH_ERR("%s\n", msg_hash_to_str(MSG_ERROR_PARSING_ARGUMENTS));
               retroarch_fail(1, "retroarch_parse_input()");
         }
      }
   }

   if (verbosity_is_enabled())
      rarch_log_file_init(
            p_rarch->configuration_settings->bools.log_to_file,
            p_rarch->configuration_settings->bools.log_to_file_timestamp,
            p_rarch->configuration_settings->paths.log_dir);

#ifdef HAVE_GIT_VERSION
   RARCH_LOG("RetroArch %s (Git %s)\n",
         PACKAGE_VERSION, retroarch_git_version);
#endif

   if (explicit_menu)
   {
      if (optind < argc)
      {
         RARCH_ERR("--menu was used, but content file was passed as well.\n");
         retroarch_fail(1, "retroarch_parse_input()");
      }
#ifdef HAVE_DYNAMIC
      else
      {
         /* Allow stray -L arguments to go through to workaround cases
          * where it's used as "config file".
          *
          * This seems to still be the case for Android, which
          * should be properly fixed. */
         retroarch_set_current_core_type(CORE_TYPE_DUMMY, false);
      }
#endif
   }

   if (optind < argc)
   {
      bool subsystem_path_is_empty = path_is_empty(RARCH_PATH_SUBSYSTEM);

      /* We requested explicit ROM, so use PLAIN core type. */
      retroarch_set_current_core_type(CORE_TYPE_PLAIN, false);

      if (subsystem_path_is_empty)
         path_set(RARCH_PATH_NAMES, (const char*)argv[optind]);
      else
         path_set_special(argv + optind, argc - optind);
   }

   /* Copy SRM/state dirs used, so they can be reused on reentrancy. */
   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL) &&
         path_is_directory(global->name.savefile))
      dir_set(RARCH_DIR_SAVEFILE, global->name.savefile);

   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL) &&
         path_is_directory(global->name.savestate))
      dir_set(RARCH_DIR_SAVESTATE, global->name.savestate);
}

static bool retroarch_validate_per_core_options(char *s,
      size_t len, bool mkdir,
      const char *core_name, const char *game_name)
{
   char *config_directory                 = NULL;
   size_t str_size                        = PATH_MAX_LENGTH * sizeof(char);

   config_directory                       = (char*)malloc(str_size);
   config_directory[0]                    = '\0';

   fill_pathname_application_special(config_directory,
         str_size, APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   fill_pathname_join_special_ext(s,
         config_directory, core_name, game_name,
         ".opt", len);

   /* No need to make a directory if file already exists... */
   if (mkdir && !path_is_valid(s))
   {
      char *new_path          = (char*)malloc(str_size);
      new_path[0]             = '\0';

      fill_pathname_join(new_path,
            config_directory, core_name, str_size);

      if (!path_is_directory(new_path))
         path_mkdir(new_path);

      free(new_path);
   }

   free(config_directory);
   return true;
}

static bool retroarch_validate_game_options(char *s, size_t len, bool mkdir)
{
   struct rarch_state            *p_rarch = &rarch_st;
   const char *core_name                  = p_rarch->runloop_system.info.library_name;
   const char *game_name                  = path_basename(path_get(RARCH_PATH_BASENAME));

   if (string_is_empty(core_name) || string_is_empty(game_name))
      return false;

   return retroarch_validate_per_core_options(s, len, mkdir,
   core_name, game_name);
}

/* Validates CPU features for given processor architecture.
 * Make sure we haven't compiled for something we cannot run.
 * Ideally, code would get swapped out depending on CPU support,
 * but this will do for now. */
static void retroarch_validate_cpu_features(void)
{
   uint64_t cpu = cpu_features_get();
   (void)cpu;

#ifdef __MMX__
   if (!(cpu & RETRO_SIMD_MMX))
      FAIL_CPU("MMX");
#endif
#ifdef __SSE__
   if (!(cpu & RETRO_SIMD_SSE))
      FAIL_CPU("SSE");
#endif
#ifdef __SSE2__
   if (!(cpu & RETRO_SIMD_SSE2))
      FAIL_CPU("SSE2");
#endif
#ifdef __AVX__
   if (!(cpu & RETRO_SIMD_AVX))
      FAIL_CPU("AVX");
#endif
}

/**
 * retroarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Initializes the program.
 *
 * Returns: true on success, otherwise false if there was an error.
 **/
bool retroarch_main_init(int argc, char *argv[])
{
#if defined(DEBUG) && defined(HAVE_DRMINGW)
   char log_file_name[128];
#endif
   bool           init_failed   = false;
   struct rarch_state *p_rarch  = &rarch_st;
   global_t            *global  = &p_rarch->g_extern;

   p_rarch->video_driver_active = true;
   p_rarch->audio_driver_active = true;

   if (setjmp(p_rarch->error_sjlj_context) > 0)
   {
      RARCH_ERR("%s: \"%s\"\n",
            msg_hash_to_str(MSG_FATAL_ERROR_RECEIVED_IN), p_rarch->error_string);
      return false;
   }

   p_rarch->rarch_error_on_init = true;

   /* Have to initialise non-file logging once at the start... */
   retro_main_log_file_init(NULL, false);

   retroarch_parse_input_and_config(argc, argv);

#ifdef HAVE_ACCESSIBILITY
   if (is_accessibility_enabled())
      accessibility_startup_message();
#endif

   if (verbosity_is_enabled())
   {
      {
         char str_output[256];
         const char *cpu_model  = NULL;
         str_output[0] = '\0';

         cpu_model     = frontend_driver_get_cpu_model_name();

         strlcat(str_output,
               "=== Build =======================================\n",
               sizeof(str_output));

         if (!string_is_empty(cpu_model))
         {
            strlcat(str_output, FILE_PATH_LOG_INFO " CPU Model Name: ", sizeof(str_output));
            strlcat(str_output, cpu_model, sizeof(str_output));
            strlcat(str_output, "\n", sizeof(str_output));
         }

         RARCH_LOG_OUTPUT(str_output);
      }
      {
         char str_output[256];
         char str[128];
         str[0]        = str_output[0] = '\0';

         retroarch_get_capabilities(RARCH_CAPABILITIES_CPU, str, sizeof(str));

         strlcat(str_output, msg_hash_to_str(MSG_CAPABILITIES),
               sizeof(str_output));
         strlcat(str_output, ": ", sizeof(str_output));
         strlcat(str_output, str,  sizeof(str_output));
         strlcat(str_output, "\n" FILE_PATH_LOG_INFO " Built: " __DATE__ "\n" FILE_PATH_LOG_INFO " Version: " PACKAGE_VERSION "\n", sizeof(str_output));
#ifdef HAVE_GIT_VERSION
         strlcat(str_output, FILE_PATH_LOG_INFO " Git: ", sizeof(str_output));
         strlcat(str_output, retroarch_git_version, sizeof(str_output));
         strlcat(str_output, "\n", sizeof(str_output));
#endif

         strlcat(str_output, FILE_PATH_LOG_INFO " =================================================\n", sizeof(str_output));
         RARCH_LOG_OUTPUT(str_output);
      }
   }

#if defined(DEBUG) && defined(HAVE_DRMINGW)
   RARCH_LOG_OUTPUT("Initializing Dr.MingW Exception handler\n");
   fill_str_dated_filename(log_file_name, "crash",
         "log", sizeof(log_file_name));
   ExcHndlInit();
   ExcHndlSetLogFileNameA(log_file_name);
#endif

   retroarch_validate_cpu_features();
   retroarch_init_task_queue();

   {
      const char    *fullpath  = path_get(RARCH_PATH_CONTENT);

      if (!string_is_empty(fullpath))
      {
         settings_t *settings              = p_rarch->configuration_settings;
         enum rarch_content_type cont_type = path_is_media_type(fullpath);
#ifdef HAVE_IMAGEVIEWER
         bool builtin_imageviewer          = settings ? settings->bools.multimedia_builtin_imageviewer_enable : false;
#endif
         bool builtin_mediaplayer          = settings ? settings->bools.multimedia_builtin_mediaplayer_enable : false;

         switch (cont_type)
         {
            case RARCH_CONTENT_MOVIE:
            case RARCH_CONTENT_MUSIC:
               if (builtin_mediaplayer)
               {
                  /* TODO/FIXME - it needs to become possible to
                   * switch between FFmpeg and MPV at runtime */
#if defined(HAVE_MPV)
                  retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
                  retroarch_set_current_core_type(CORE_TYPE_MPV, false);
#elif defined(HAVE_FFMPEG)
                  retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
                  retroarch_set_current_core_type(CORE_TYPE_FFMPEG, false);
#endif
               }
               break;
#ifdef HAVE_IMAGEVIEWER
            case RARCH_CONTENT_IMAGE:
               if (builtin_imageviewer)
               {
                  retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
                  retroarch_set_current_core_type(CORE_TYPE_IMAGEVIEWER, false);
               }
               break;
#endif
#ifdef HAVE_GONG
            case RARCH_CONTENT_GONG:
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
               retroarch_set_current_core_type(CORE_TYPE_GONG, false);
               break;
#endif
            default:
               break;
         }
      }
   }

   /* Pre-initialize all drivers
    * Attempts to find a default driver for
    * all driver types.
    */
   audio_driver_find_driver();
   video_driver_find_driver();
   input_driver_find_driver();
   camera_driver_find_driver();
   wifi_driver_ctl(RARCH_WIFI_CTL_FIND_DRIVER, NULL);
   find_location_driver();
#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_FIND_DRIVER, NULL);
#endif

   /* Attempt to initialize core */
   if (p_rarch->has_set_core)
   {
      p_rarch->has_set_core = false;
      if (!command_event(CMD_EVENT_CORE_INIT,
               &p_rarch->explicit_current_core_type))
         init_failed = true;
   }
   else if (!command_event(CMD_EVENT_CORE_INIT,
            &p_rarch->current_core_type))
      init_failed = true;

   /* Handle core initialization failure */
   if (init_failed)
   {
      /* Check if menu was active prior to core initialization */
      if (!content_launched_from_cli()
#ifdef HAVE_MENU
          || p_rarch->menu_driver_alive
#endif
         )
      {
         /* Attempt initializing dummy core */
         p_rarch->current_core_type = CORE_TYPE_DUMMY;
         if (!command_event(CMD_EVENT_CORE_INIT, &p_rarch->current_core_type))
            goto error;
      }
      else
      {
         /* Fall back to regular error handling */
         goto error;
      }
   }

   cheat_manager_state_free();
   command_event_init_cheats();
   drivers_init(DRIVERS_CMD_ALL);
   input_driver_deinit_command();
   input_driver_init_command();
   input_driver_deinit_remote();
   input_driver_init_remote();
   input_driver_deinit_mapper();
   input_driver_init_mapper();
   command_event(CMD_EVENT_REWIND_INIT, NULL);
   command_event_init_controllers();
   if (!string_is_empty(global->record.path))
      command_event(CMD_EVENT_RECORD_INIT, NULL);

   path_init_savefile();

   command_event(CMD_EVENT_SET_PER_GAME_RESOLUTION, NULL);

   p_rarch->rarch_error_on_init     = false;
   p_rarch->rarch_is_inited         = true;

#ifdef HAVE_DISCORD
   if (command_event(CMD_EVENT_DISCORD_INIT, NULL))
      discord_is_inited = true;

   if (discord_is_inited)
   {
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_MENU;

      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
   }
#endif

#if defined(HAVE_MENU) && defined(HAVE_AUDIOMIXER)
   if (p_rarch->configuration_settings->bools.audio_enable_menu)
      audio_driver_load_menu_sounds();
#endif

   return true;

error:
   command_event(CMD_EVENT_CORE_DEINIT, NULL);
   p_rarch->rarch_is_inited         = false;

   return false;
}

#if 0
static bool retroarch_is_on_main_thread(void)
{
#ifdef HAVE_THREAD_STORAGE
   struct rarch_state *p_rarch = &rarch_st;
   if (sthread_tls_get(&p_rarch->rarch_tls) != MAGIC_POINTER)
      return false;
#endif
   return true;
}
#endif

#ifdef HAVE_MENU
/* This callback gets triggered by the keyboard whenever
 * we press or release a keyboard key. When a keyboard
 * key is being pressed down, 'down' will be true. If it
 * is being released, 'down' will be false.
 */
static void menu_input_key_event(bool down, unsigned keycode,
      uint32_t character, uint16_t mod)
{
   struct rarch_state *p_rarch = &rarch_st;
   enum retro_key          key = (enum retro_key)keycode;

   if (key == RETROK_UNKNOWN)
   {
      unsigned i;

      for (i = 0; i < RETROK_LAST; i++)
         p_rarch->menu_keyboard_key_state[i] = 
            (p_rarch->menu_keyboard_key_state[(enum retro_key)i] & 1) << 1;
   }
   else
      p_rarch->menu_keyboard_key_state[key]  = 
         ((p_rarch->menu_keyboard_key_state[key] & 1) << 1) | down;
}

/* Gets called when we want to toggle the menu.
 * If the menu is already running, it will be turned off.
 * If the menu is off, then the menu will be started.
 */
static void menu_driver_toggle(bool on)
{
   /* TODO/FIXME - retroarch_main_quit calls menu_driver_toggle -
    * we might have to redesign this to avoid EXXC_BAD_ACCESS errors
    * on OSX - for now we work around this by checking if the settings
    * struct is NULL 
    */
   struct rarch_state                *p_rarch = &rarch_st;
   retro_keyboard_event_t *key_event          = &p_rarch->runloop_key_event;
   retro_keyboard_event_t *frontend_key_event = &p_rarch->runloop_frontend_key_event;
   settings_t                 *settings       = p_rarch->configuration_settings;
   bool pause_libretro                        = settings ?
      settings->bools.menu_pause_libretro : false;
#ifdef HAVE_AUDIOMIXER
   bool audio_enable_menu                     = settings ? settings->bools.audio_enable_menu : false;
#if 0
   bool audio_enable_menu_bgm                 = settings ? settings->bools.audio_enable_menu_bgm : false;
#endif
#endif
   bool runloop_shutdown_initiated            = p_rarch->runloop_shutdown_initiated;
   menu_handle_t *menu_data                   = menu_driver_get_ptr();

   if (!menu_data)
      return;

   if (menu_data->driver_ctx && menu_data->driver_ctx->toggle)
      menu_data->driver_ctx->toggle(menu_data->userdata, on);

   p_rarch->menu_driver_alive                 = on;

   /* Apply any required menu pointer input inhibits
    * (i.e. prevent phantom input when using an overlay
    * to toggle the menu on) */
   menu_input_driver_toggle(on);

   if (p_rarch->menu_driver_alive)
   {
      bool refresh = false;

#ifdef WIIU
      /* Enable burn-in protection menu is running */
      IMEnableDim();
#endif

      menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

      /* Menu should always run with vsync on. */
      if (p_rarch->current_video->set_nonblock_state)
         p_rarch->current_video->set_nonblock_state(p_rarch->video_driver_data, false,
               video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC) &&
               settings->bools.video_adaptive_vsync,
               settings->uints.video_swap_interval
               );
      /* Stop all rumbling before entering the menu. */
      command_event(CMD_EVENT_RUMBLE_STOP, NULL);

      if (pause_libretro && !audio_enable_menu)
         command_event(CMD_EVENT_AUDIO_STOP, NULL);

#if 0
     if (audio_enable_menu && audio_enable_menu_bgm)
         audio_driver_mixer_play_menu_sound_looped(AUDIO_MIXER_SYSTEM_SLOT_BGM);
#endif

      /* Override keyboard callback to redirect to menu instead.
       * We'll use this later for something ... */

      if (key_event && frontend_key_event)
      {
         *frontend_key_event              = *key_event;
         *key_event                       = menu_input_key_event;

         p_rarch->runloop_frame_time_last = 0;
      }
   }
   else
   {
#ifdef WIIU
      /* Disable burn-in protection while core is running; this is needed
       * because HID inputs don't count for the purpose of Wii U
       * power-saving. */
      IMDisableDim();
#endif

      if (!runloop_shutdown_initiated)
         driver_set_nonblock_state();

      if (pause_libretro && !audio_enable_menu)
         command_event(CMD_EVENT_AUDIO_START, NULL);

#if 0
      if (audio_enable_menu && audio_enable_menu_bgm)
         audio_driver_mixer_stop_stream(AUDIO_MIXER_SYSTEM_SLOT_BGM);
#endif

      /* Restore libretro keyboard callback. */
      if (key_event && frontend_key_event)
         *key_event = *frontend_key_event;
   }
}
#endif

void retroarch_menu_running(void)
{
   struct rarch_state *p_rarch     = &rarch_st;
#if defined(HAVE_MENU) || defined(HAVE_OVERLAY)
   settings_t *settings            = p_rarch->configuration_settings;
#endif
#ifdef HAVE_OVERLAY
   bool input_overlay_hide_in_menu = settings->bools.input_overlay_hide_in_menu;
#endif
#ifdef HAVE_AUDIOMIXER
   bool audio_enable_menu          = settings->bools.audio_enable_menu;
   bool audio_enable_menu_bgm      = settings->bools.audio_enable_menu_bgm;
#endif

#ifdef HAVE_MENU
   menu_driver_toggle(true);

   /* Prevent stray input (for a single frame) */
   p_rarch->input_driver_flushing_input = 1;

#ifdef HAVE_AUDIOMIXER
   if (audio_enable_menu && audio_enable_menu_bgm)
      audio_driver_mixer_play_menu_sound_looped(AUDIO_MIXER_SYSTEM_SLOT_BGM);
#endif
#endif

#ifdef HAVE_OVERLAY
   if (input_overlay_hide_in_menu)
      command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
#endif
}

void retroarch_menu_running_finished(bool quit)
{
   struct rarch_state *p_rarch          = &rarch_st;
#if defined(HAVE_MENU) || defined(HAVE_OVERLAY)
   settings_t *settings                 = p_rarch->configuration_settings;
#endif
#ifdef HAVE_MENU
   menu_driver_toggle(false);

   /* Prevent stray input
    * (for a single frame) */
   p_rarch->input_driver_flushing_input = 1;

#ifdef HAVE_AUDIOMIXER
   if (!quit)
      /* Stop menu background music before we exit the menu */
      if (  settings && 
            settings->bools.audio_enable_menu && 
            settings->bools.audio_enable_menu_bgm
         )
         audio_driver_mixer_stop_stream(AUDIO_MIXER_SYSTEM_SLOT_BGM);
#endif

#endif
   video_driver_set_texture_enable(false, false);
#ifdef HAVE_OVERLAY
   if (!quit)
      if (settings && settings->bools.input_overlay_hide_in_menu)
         retroarch_overlay_init();
#endif
}

/**
 * rarch_game_specific_options:
 *
 * Returns: true (1) if a game specific core
 * options path has been found,
 * otherwise false (0).
 **/
static bool rarch_game_specific_options(char **output)
{
   size_t game_path_size = PATH_MAX_LENGTH * sizeof(char);
   char *game_path       = (char*)malloc(game_path_size);

   game_path[0] ='\0';

   if (!retroarch_validate_game_options(game_path,
            game_path_size, false) || !path_is_valid(game_path))
   {
      free(game_path);
      return false;
   }

   RARCH_LOG("%s %s\n",
         msg_hash_to_str(MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT),
         game_path);
   *output = strdup(game_path);
   free(game_path);
   return true;
}

static void runloop_task_msg_queue_push(
      retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration,
      bool flush)
{
#if defined(HAVE_GFX_WIDGETS)
   struct rarch_state *p_rarch = &rarch_st;

   if (gfx_widgets_active() && task->title && !task->mute)
   {
      runloop_msg_queue_lock();
      ui_companion_driver_msg_queue_push(msg,
            prio, task ? duration : duration * 60 / 1000, flush);
#ifdef HAVE_ACCESSIBILITY
      if (is_accessibility_enabled())
         accessibility_speak_priority((char*)msg, 0);
#endif
      gfx_widgets_msg_queue_push(task, msg, duration, NULL, (enum message_queue_icon)MESSAGE_QUEUE_CATEGORY_INFO, (enum message_queue_category)MESSAGE_QUEUE_ICON_DEFAULT, prio, flush,
#ifdef HAVE_MENU
            p_rarch->menu_driver_alive
#else
            false
#endif
            );
      runloop_msg_queue_unlock();
   }
   else
#endif
      runloop_msg_queue_push(msg, prio, duration, flush, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

/* Fetches core options path for current core/content
 * - path: path from which options should be read
 *   from/saved to
 * - src_path: in the event that 'path' file does not
 *   yet exist, provides source path from which initial
 *   options should be extracted
 *  */
static void rarch_init_core_options_path(
      char *path, size_t len,
      char *src_path, size_t src_len)
{
   char *game_options_path        = NULL;
   struct rarch_state    *p_rarch = &rarch_st;
   settings_t *settings           = p_rarch->configuration_settings;
   bool game_specific_options     = settings->bools.game_specific_options;

   /* Ensure that 'input' strings are null terminated */
   if (len > 0)
      path[0]     = '\0';
   if (src_len > 0)
      src_path[0] = '\0';

   /* Check whether game-specific options exist */
   if (game_specific_options &&
         rarch_game_specific_options(&game_options_path))
   {
      /* Notify system that we have a valid core options
       * override */
      path_set(RARCH_PATH_CORE_OPTIONS, game_options_path);
      p_rarch->runloop_game_options_active = true;

      /* Copy options path */
      strlcpy(path, game_options_path, len);

      free(game_options_path);
   }
   else
   {
      char global_options_path[PATH_MAX_LENGTH];
      char per_core_options_path[PATH_MAX_LENGTH];
      bool per_core_options_exist   = false;
      bool per_core_options         = !settings->bools.global_core_options;
      const char *path_core_options = settings->paths.path_core_options;

      global_options_path[0]        = '\0';
      per_core_options_path[0]      = '\0';

      if (per_core_options)
      {
         const char *core_name      = p_rarch->runloop_system.info.library_name;
         /* Get core-specific options path
          * > if retroarch_validate_per_core_options() returns
          *   false, then per-core options are disabled (due to
          *   unknown system errors...) */

         if (string_is_empty(core_name))
            per_core_options = false;
         else
            per_core_options = retroarch_validate_per_core_options(
                  per_core_options_path, sizeof(per_core_options_path), true,
                  core_name, core_name
                  );

         /* If we can use per-core options, check whether an options
          * file already exists */
         if (per_core_options)
            per_core_options_exist = path_is_valid(per_core_options_path);
      }

      /* If not using per-core options, or if a per-core options
       * file does not yet exist, must fetch 'global' options path */
      if (!per_core_options || !per_core_options_exist)
      {
         const char *options_path   = path_core_options;

         if (!string_is_empty(options_path))
            strlcpy(global_options_path,
                  options_path, sizeof(global_options_path));
         else if (!path_is_empty(RARCH_PATH_CONFIG))
         {
            fill_pathname_resolve_relative(
                  global_options_path, path_get(RARCH_PATH_CONFIG),
                  "retroarch-core-options.cfg", sizeof(global_options_path));
         }
      }

      /* Allocate correct path/src_path strings */
      if (per_core_options)
      {
         strlcpy(path, per_core_options_path, len);

         if (!per_core_options_exist)
            strlcpy(src_path, global_options_path, src_len);
      }
      else
         strlcpy(path, global_options_path, len);

      /* Notify system that we *do not* have a valid core options
       * options override */
      p_rarch->runloop_game_options_active = false;
   }
}

static void rarch_init_core_options(
      const struct retro_core_option_definition *option_defs)
{
   char options_path[PATH_MAX_LENGTH];
   char src_options_path[PATH_MAX_LENGTH];
   struct rarch_state    *p_rarch = &rarch_st;

   options_path[0]                = '\0';
   src_options_path[0]            = '\0';

   /* Get core options file path */
   rarch_init_core_options_path(
      options_path, sizeof(options_path),
      src_options_path, sizeof(src_options_path));

   if (!string_is_empty(options_path))
      p_rarch->runloop_core_options =
            core_option_manager_new(options_path, src_options_path, option_defs);
}

void retroarch_init_task_queue(void)
{
#ifdef HAVE_THREADS
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   bool threaded_enable        = settings->bools.threaded_data_runloop_enable;
#else
   bool threaded_enable        = false;
#endif

   task_queue_deinit();
   task_queue_init(threaded_enable, runloop_task_msg_queue_push);
}

static void retroarch_core_options_intl_init(const struct 
      retro_core_options_intl *core_options_intl)
{
   /* Parse core_options_intl to create option definitions array */
   struct retro_core_option_definition *option_defs =
      core_option_manager_get_definitions(core_options_intl);

   if (option_defs)
   {
      /* Initialise core options */
      rarch_init_core_options(option_defs);

      /* Clean up */
      free(option_defs);
   }
}

bool rarch_ctl(enum rarch_ctl_state state, void *data)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch(state)
   {
      case RARCH_CTL_HAS_SET_SUBSYSTEMS:
         return (p_rarch->current_core.has_set_subsystems);
      case RARCH_CTL_CORE_IS_RUNNING:
         return p_rarch->runloop_core_running;
      case RARCH_CTL_BSV_MOVIE_IS_INITED:
         return (p_rarch->bsv_movie_state_handle != NULL);
      case RARCH_CTL_IS_PATCH_BLOCKED:
         return p_rarch->rarch_patch_blocked;
      case RARCH_CTL_IS_BPS_PREF:
         return p_rarch->rarch_bps_pref;
      case RARCH_CTL_UNSET_BPS_PREF:
         p_rarch->rarch_bps_pref = false;
         break;
      case RARCH_CTL_IS_UPS_PREF:
         return p_rarch->rarch_ups_pref;
      case RARCH_CTL_UNSET_UPS_PREF:
         p_rarch->rarch_ups_pref = false;
         break;
      case RARCH_CTL_IS_IPS_PREF:
         return p_rarch->rarch_ips_pref;
      case RARCH_CTL_UNSET_IPS_PREF:
         p_rarch->rarch_ips_pref = false;
         break;
      case RARCH_CTL_IS_DUMMY_CORE:
         return (p_rarch->current_core_type == CORE_TYPE_DUMMY);
      case RARCH_CTL_HAS_SET_USERNAME:
         return p_rarch->has_set_username;
      case RARCH_CTL_IS_INITED:
         return p_rarch->rarch_is_inited;
      case RARCH_CTL_MAIN_DEINIT:
         if (!p_rarch->rarch_is_inited)
            return false;
         command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
         input_driver_deinit_command();
         input_driver_deinit_remote();
         input_driver_deinit_mapper();

#ifdef HAVE_THREADS
         retroarch_autosave_deinit();
#endif

         command_event(CMD_EVENT_RECORD_DEINIT, NULL);

         command_event(CMD_EVENT_SAVE_FILES, NULL);

         command_event(CMD_EVENT_REWIND_DEINIT, NULL);
         cheat_manager_state_free();
         bsv_movie_deinit();

         command_event(CMD_EVENT_CORE_DEINIT, NULL);

         content_deinit();

         path_deinit_subsystem();
         path_deinit_savefile();

         p_rarch->rarch_is_inited         = false;

#ifdef HAVE_THREAD_STORAGE
         sthread_tls_delete(&p_rarch->rarch_tls);
#endif
         break;
#ifdef HAVE_CONFIGFILE
      case RARCH_CTL_SET_BLOCK_CONFIG_READ:
         p_rarch->rarch_block_config_read = true;
         break;
      case RARCH_CTL_UNSET_BLOCK_CONFIG_READ:
         p_rarch->rarch_block_config_read = false;
         break;
#endif
      case RARCH_CTL_GET_CORE_OPTION_SIZE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            if (p_rarch->runloop_core_options)
               *idx = (unsigned)p_rarch->runloop_core_options->size;
            else
               *idx = 0;
         }
         break;
      case RARCH_CTL_HAS_CORE_OPTIONS:
         return (p_rarch->runloop_core_options != NULL);
      case RARCH_CTL_CORE_OPTIONS_LIST_GET:
         {
            core_option_manager_t **coreopts = (core_option_manager_t**)data;
            if (!coreopts)
               return false;
            *coreopts = p_rarch->runloop_core_options;
         }
         break;
#ifdef HAVE_CONFIGFILE
      case RARCH_CTL_IS_OVERRIDES_ACTIVE:
         return p_rarch->runloop_overrides_active;
      case RARCH_CTL_SET_REMAPS_CORE_ACTIVE:
         p_rarch->runloop_remaps_core_active = true;
         break;
      case RARCH_CTL_UNSET_REMAPS_CORE_ACTIVE:
         p_rarch->runloop_remaps_core_active = false;
         break;
      case RARCH_CTL_IS_REMAPS_CORE_ACTIVE:
         return p_rarch->runloop_remaps_core_active;
      case RARCH_CTL_SET_REMAPS_GAME_ACTIVE:
         p_rarch->runloop_remaps_game_active = true;
         break;
      case RARCH_CTL_UNSET_REMAPS_GAME_ACTIVE:
         p_rarch->runloop_remaps_game_active = false;
         break;
      case RARCH_CTL_IS_REMAPS_GAME_ACTIVE:
         return p_rarch->runloop_remaps_game_active;
      case RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE:
         p_rarch->runloop_remaps_content_dir_active = true;
         break;
      case RARCH_CTL_UNSET_REMAPS_CONTENT_DIR_ACTIVE:
         p_rarch->runloop_remaps_content_dir_active = false;
         break;
      case RARCH_CTL_IS_REMAPS_CONTENT_DIR_ACTIVE:
         return p_rarch->runloop_remaps_content_dir_active;
#endif
      case RARCH_CTL_SET_MISSING_BIOS:
         p_rarch->runloop_missing_bios = true;
         break;
      case RARCH_CTL_UNSET_MISSING_BIOS:
         p_rarch->runloop_missing_bios = false;
         break;
      case RARCH_CTL_IS_MISSING_BIOS:
         return p_rarch->runloop_missing_bios;
      case RARCH_CTL_IS_GAME_OPTIONS_ACTIVE:
         return p_rarch->runloop_game_options_active;
      case RARCH_CTL_GET_PERFCNT:
         {
            bool **perfcnt = (bool**)data;
            if (!perfcnt)
               return false;
            *perfcnt = &p_rarch->runloop_perfcnt_enable;
         }
         break;
      case RARCH_CTL_SET_PERFCNT_ENABLE:
         p_rarch->runloop_perfcnt_enable = true;
         break;
      case RARCH_CTL_UNSET_PERFCNT_ENABLE:
         p_rarch->runloop_perfcnt_enable = false;
         break;
      case RARCH_CTL_IS_PERFCNT_ENABLE:
         return p_rarch->runloop_perfcnt_enable;
      case RARCH_CTL_SET_WINDOWED_SCALE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            p_rarch->runloop_pending_windowed_scale = *idx;
         }
         break;
      case RARCH_CTL_STATE_FREE:
         p_rarch->runloop_perfcnt_enable   = false;
         p_rarch->runloop_idle             = false;
         p_rarch->runloop_paused           = false;
         p_rarch->runloop_slowmotion       = false;
#ifdef HAVE_CONFIGFILE
         p_rarch->runloop_overrides_active = false;
#endif
         p_rarch->runloop_autosave         = false;
         retroarch_frame_time_free();
         break;
      case RARCH_CTL_IS_IDLE:
         return p_rarch->runloop_idle;
      case RARCH_CTL_SET_IDLE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            p_rarch->runloop_idle = *ptr;
         }
         break;
      case RARCH_CTL_SET_PAUSED:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            p_rarch->runloop_paused = *ptr;
         }
         break;
      case RARCH_CTL_IS_PAUSED:
         return p_rarch->runloop_paused;
      case RARCH_CTL_SET_SHUTDOWN:
         p_rarch->runloop_shutdown_initiated = true;
         break;
      case RARCH_CTL_CORE_OPTION_PREV:
         /*
          * Get previous value for core option specified by @idx.
          * Options wrap around.
          */
         {
            struct core_option *option        = NULL;
            unsigned              *idx        = (unsigned*)data;
            if (!idx || !p_rarch->runloop_core_options)
               return false;
            option                            = (struct core_option*)
               &p_rarch->runloop_core_options->opts[*idx];

            if (option)
            {
               option->index                  =
                  (option->index + option->vals->size - 1) %
                  option->vals->size;
               p_rarch->runloop_core_options->updated  = true;
            }
         }
         break;
      case RARCH_CTL_CORE_OPTION_NEXT:
         /*
          * Get next value for core option specified by @idx.
          * Options wrap around.
          */
         {
            struct core_option *option       = NULL;
            unsigned *idx                    = (unsigned*)data;

            if (!idx || !p_rarch->runloop_core_options)
               return false;

            option                           =
               (struct core_option*)&p_rarch->runloop_core_options->opts[*idx];

            if (option)
            {
               option->index                          =
                  (option->index + 1) % option->vals->size;
               p_rarch->runloop_core_options->updated = true;
            }
         }
         break;


      case RARCH_CTL_NONE:
      default:
         return false;
   }

   return true;
}

static void retroarch_deinit_core_options(void)
{
   struct rarch_state    *p_rarch = &rarch_st;

   /* Check whether game-specific options file is being used */
   if (!path_is_empty(RARCH_PATH_CORE_OPTIONS))
   {
      const char *path        = path_get(RARCH_PATH_CORE_OPTIONS);
      config_file_t *conf_tmp = NULL;

      /* We only need to save configuration settings for
       * the current core
       * > If game-specific options file exists, have
       *   to read it (to ensure file only gets written
       *   if config values change)
       * > Otherwise, create a new, empty config_file_t
       *   object */
      if (path_is_valid(path))
         conf_tmp = config_file_new_from_path_to_string(path);

      if (!conf_tmp)
         conf_tmp = config_file_new_alloc();

      if (conf_tmp)
      {
         core_option_manager_flush(
               conf_tmp,
               p_rarch->runloop_core_options);
         RARCH_LOG("[Core Options]: Saved game-specific core options to \"%s\"\n", path);
         config_file_write(conf_tmp, path, true);
         config_file_free(conf_tmp);
         conf_tmp = NULL;
      }
      path_clear(RARCH_PATH_CORE_OPTIONS);
   }
   else
   {
      const char *path = p_rarch->runloop_core_options->conf_path;
      core_option_manager_flush(
            p_rarch->runloop_core_options->conf,
            p_rarch->runloop_core_options);
      RARCH_LOG("[Core Options]: Saved core options file to \"%s\"\n", path);
      config_file_write(p_rarch->runloop_core_options->conf, path, true);
   }

   if (p_rarch->runloop_game_options_active)
      p_rarch->runloop_game_options_active = false;

   if (p_rarch->runloop_core_options)
      core_option_manager_free(p_rarch->runloop_core_options);
   p_rarch->runloop_core_options           = NULL;
}

static void retroarch_init_core_variables(const struct retro_variable *vars)
{
   char options_path[PATH_MAX_LENGTH];
   char src_options_path[PATH_MAX_LENGTH];
   struct rarch_state *p_rarch = &rarch_st;

   options_path[0]     = '\0';
   src_options_path[0] = '\0';

   /* Get core options file path */
   rarch_init_core_options_path(
         options_path, sizeof(options_path),
         src_options_path, sizeof(src_options_path));

   if (!string_is_empty(options_path))
      p_rarch->runloop_core_options =
         core_option_manager_new_vars(options_path, src_options_path, vars);
}

bool retroarch_is_forced_fullscreen(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->rarch_force_fullscreen;
}

bool retroarch_is_switching_display_mode(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->rarch_is_switching_display_mode;
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static bool retroarch_load_shader_preset_internal(
      const char *shader_directory,
      const char *core_name,
      const char *special_name)
{
   unsigned i;
   char *shader_path = (char*)malloc(PATH_MAX_LENGTH);

   static enum rarch_shader_type types[] =
   {
      /* Shader preset priority, highest to lowest
       * only important for video drivers with multiple shader backends */
      RARCH_SHADER_GLSL, RARCH_SHADER_SLANG, RARCH_SHADER_CG, RARCH_SHADER_HLSL
   };

   for (i = 0; i < ARRAY_SIZE(types); i++)
   {
      if (!video_shader_is_supported(types[i]))
         continue;

      /* Concatenate strings into full paths */
      if (!string_is_empty(core_name))
      {
         fill_pathname_join_special_ext(shader_path,
               shader_directory, core_name,
               special_name,
               video_shader_get_preset_extension(types[i]),
               PATH_MAX_LENGTH);
      }
      else
      {
         if (string_is_empty(special_name))
            break;

         fill_pathname_join(shader_path, shader_directory, special_name, PATH_MAX_LENGTH);
         strlcat(shader_path,
               video_shader_get_preset_extension(types[i]),
               PATH_MAX_LENGTH);
      }

      if (!path_is_valid(shader_path))
         continue;

      /* Shader preset exists, load it. */
      RARCH_LOG("[Shaders]: Specific shader preset found at %s.\n",
            shader_path);
      retroarch_set_runtime_shader_preset(shader_path);

      free(shader_path);
      return true;
   }

   free(shader_path);
   return false;
}

/**
 * retroarch_load_shader_preset:
 *
 * Tries to load a supported core-, game-, folder-specific or global
 * shader preset from its respective location:
 *
 * global:          $CONFIG_DIR/global.$PRESET_EXT
 * core-specific:   $CONFIG_DIR/$CORE_NAME/$CORE_NAME.$PRESET_EXT
 * folder-specific: $CONFIG_DIR/$CORE_NAME/$FOLDER_NAME.$PRESET_EXT
 * game-specific:   $CONFIG_DIR/$CORE_NAME/$GAME_NAME.$PRESET_EXT
 *
 * $CONFIG_DIR is expected to be Menu Config directory, or failing that, the
 * directory where retroarch.cfg is stored.
 *
 * For compatibility purposes with versions 1.8.7 and older, the presets
 * subdirectory on the Video Shader path is used as a fallback directory.
 *
 * Note: Uses video_shader_is_supported() which only works after
 *       context driver initialization.
 *
 * Returns: false if there was an error or no action was performed.
 */
static bool retroarch_load_shader_preset(void)
{
   struct rarch_state *p_rarch        = &rarch_st;
   settings_t *settings               = p_rarch->configuration_settings;
   const char *video_shader_directory = settings->paths.directory_video_shader;
   const char *menu_config_directory  = settings->paths.directory_menu_config;
   const char *core_name              = p_rarch->runloop_system.info.library_name;
   const char *rarch_path_basename    = path_get(RARCH_PATH_BASENAME);

   const char *game_name              = path_basename(rarch_path_basename);
   char *config_file_directory        = NULL;
   char *old_presets_directory        = NULL;

   const char *dirs[3]                = {0};
   size_t i                           = 0;

   bool ret                           = false;
   char *content_dir_name             = (char*)malloc(PATH_MAX_LENGTH);
   if (!content_dir_name)
      return false;

   config_file_directory = (char*)malloc(PATH_MAX_LENGTH);
   if (!config_file_directory)
      goto end;

   old_presets_directory = (char*)malloc(PATH_MAX_LENGTH);
   if (!old_presets_directory)
      goto end;

   content_dir_name[0] = '\0';

   if (!string_is_empty(rarch_path_basename))
      fill_pathname_parent_dir_name(content_dir_name,
            rarch_path_basename, PATH_MAX_LENGTH);

   config_file_directory[0] = '\0';

   if (!path_is_empty(RARCH_PATH_CONFIG))
      fill_pathname_basedir(config_file_directory,
            path_get(RARCH_PATH_CONFIG), PATH_MAX_LENGTH);

   old_presets_directory[0] = '\0';

   if (!string_is_empty(video_shader_directory))
      fill_pathname_join(old_presets_directory,
         video_shader_directory, "presets", PATH_MAX_LENGTH);

   dirs[0] = menu_config_directory;
   dirs[1] = config_file_directory;
   dirs[2] = old_presets_directory;

   for (i = 0; i < ARRAY_SIZE(dirs); i++)
   {
      if (string_is_empty(dirs[i]))
         continue;

      RARCH_LOG("[Shaders]: preset directory: %s\n", dirs[i]);

      ret = retroarch_load_shader_preset_internal(dirs[i], core_name,
         game_name);

      if (ret)
      {
         RARCH_LOG("[Shaders]: game-specific shader preset found.\n");
         break;
      }

      ret = retroarch_load_shader_preset_internal(dirs[i], core_name,
            content_dir_name);

      if (ret)
      {
         RARCH_LOG("[Shaders]: folder-specific shader preset found.\n");
         break;
      }
   
      ret = retroarch_load_shader_preset_internal(dirs[i], core_name,
         core_name);

      if (ret)
      {
         RARCH_LOG("[Shaders]: core-specific shader preset found.\n");
         break;
      }

      ret = retroarch_load_shader_preset_internal(dirs[i], NULL,
         "global");

      if (ret)
      {
         RARCH_LOG("[Shaders]: global shader preset found.\n");
         break;
      }
   }

end:
   free(content_dir_name);
   free(config_file_directory);
   free(old_presets_directory);
   return ret;
}
#endif

/* get the name of the current shader preset */
const char* retroarch_get_shader_preset(void)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   const char *core_name       = p_rarch->runloop_system.info.library_name;
   bool video_shader_enable    = settings->bools.video_shader_enable;
   unsigned video_shader_delay = settings->uints.video_shader_delay;
   bool auto_shaders_enable    = settings->bools.auto_shaders_enable;
   bool cli_shader_disable     = p_rarch->cli_shader_disable;

   if (!video_shader_enable)
      return NULL;

   if (video_shader_delay && !p_rarch->shader_delay_timer.timer_end)
      return NULL;

   /* disallow loading auto-shaders when no core is loaded */
   if (string_is_empty(core_name))
      return NULL;

   if (!string_is_empty(p_rarch->runtime_shader_preset))
      return p_rarch->runtime_shader_preset;

   /* load auto-shader once, --set-shader works like a global auto-shader */
   if (shader_presets_need_reload && !cli_shader_disable)
   {
      shader_presets_need_reload = false;
      if (video_shader_is_supported(video_shader_parse_type(p_rarch->cli_shader)))
         strlcpy(p_rarch->runtime_shader_preset,
               p_rarch->cli_shader,
               sizeof(p_rarch->runtime_shader_preset));
      else
         if (auto_shaders_enable)
            retroarch_load_shader_preset(); /* sets runtime_shader_preset */
      return p_rarch->runtime_shader_preset;
   }
#endif

   return NULL;
}

bool retroarch_override_setting_is_set(enum rarch_override_setting enum_idx, void *data)
{
   struct rarch_state            *p_rarch = &rarch_st;

   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned bit = *val;
               return BIT256_GET(p_rarch->has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         return p_rarch->has_set_verbosity;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         return p_rarch->has_set_libretro;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         return p_rarch->has_set_libretro_directory;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         return p_rarch->has_set_save_path;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         return p_rarch->has_set_state_path;
#ifdef HAVE_NETWORKING
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         return p_rarch->has_set_netplay_mode;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         return p_rarch->has_set_netplay_ip_address;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         return p_rarch->has_set_netplay_ip_port;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         return p_rarch->has_set_netplay_stateless_mode;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         return p_rarch->has_set_netplay_check_frames;
#endif
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         return p_rarch->has_set_ups_pref;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         return p_rarch->has_set_bps_pref;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         return p_rarch->has_set_ips_pref;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         return p_rarch->has_set_log_to_file;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }

   return false;
}

int retroarch_get_capabilities(enum rarch_capabilities type,
      char *s, size_t len)
{
   switch (type)
   {
      case RARCH_CAPABILITIES_CPU:
         {
            uint64_t cpu     = cpu_features_get();

            if (cpu & RETRO_SIMD_MMX)
               strlcat(s, " MMX", len);
            if (cpu & RETRO_SIMD_MMXEXT)
               strlcat(s, " MMXEXT", len);
            if (cpu & RETRO_SIMD_SSE)
               strlcat(s, " SSE", len);
            if (cpu & RETRO_SIMD_SSE2)
               strlcat(s, " SSE2", len);
            if (cpu & RETRO_SIMD_SSE3)
               strlcat(s, " SSE3", len);
            if (cpu & RETRO_SIMD_SSSE3)
               strlcat(s, " SSSE3", len);
            if (cpu & RETRO_SIMD_SSE4)
               strlcat(s, " SSE4", len);
            if (cpu & RETRO_SIMD_SSE42)
               strlcat(s, " SSE4.2", len);
            if (cpu & RETRO_SIMD_AES)
               strlcat(s, " AES", len);
            if (cpu & RETRO_SIMD_AVX)
               strlcat(s, " AVX", len);
            if (cpu & RETRO_SIMD_AVX2)
               strlcat(s, " AVX2", len);
            if (cpu & RETRO_SIMD_NEON)
               strlcat(s, " NEON", len);
            if (cpu & RETRO_SIMD_VFPV3)
               strlcat(s, " VFPv3", len);
            if (cpu & RETRO_SIMD_VFPV4)
               strlcat(s, " VFPv4", len);
            if (cpu & RETRO_SIMD_VMX)
               strlcat(s, " VMX", len);
            if (cpu & RETRO_SIMD_VMX128)
               strlcat(s, " VMX128", len);
            if (cpu & RETRO_SIMD_VFPU)
               strlcat(s, " VFPU", len);
            if (cpu & RETRO_SIMD_PS)
               strlcat(s, " PS", len);
            if (cpu & RETRO_SIMD_ASIMD)
               strlcat(s, " ASIMD", len);
         }
         break;
      case RARCH_CAPABILITIES_COMPILER:
#if defined(_MSC_VER)
         snprintf(s, len, "%s: MSVC (%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               _MSC_VER, (unsigned)
               (CHAR_BIT * sizeof(size_t)));
#elif defined(__SNC__)
         snprintf(s, len, "%s: SNC (%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __SN_VER__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(_WIN32) && defined(__GNUC__)
         snprintf(s, len, "%s: MinGW (%d.%d.%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
               (CHAR_BIT * sizeof(size_t)));
#elif defined(__clang__)
         snprintf(s, len, "%s: Clang/LLVM (%s) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __clang_version__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(__GNUC__)
         snprintf(s, len, "%s: GCC (%d.%d.%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
               (CHAR_BIT * sizeof(size_t)));
#else
         snprintf(s, len, "%s %u-bit",
               msg_hash_to_str(MSG_UNKNOWN_COMPILER),
               (unsigned)(CHAR_BIT * sizeof(size_t)));
#endif
         break;
      default:
      case RARCH_CAPABILITIES_NONE:
         break;
   }

   return 0;
}

void retroarch_set_current_core_type(enum rarch_core_type type, bool explicitly_set)
{
   struct rarch_state *p_rarch = &rarch_st;

   if (p_rarch->has_set_core)
      return;

   if (explicitly_set)
   {
      p_rarch->has_set_core                = true;
      p_rarch->explicit_current_core_type  = type;
   }
   p_rarch->current_core_type              = type;
}

/**
 * retroarch_fail:
 * @error_code  : Error code.
 * @error       : Error message to show.
 *
 * Sanely kills the program.
 **/
static void retroarch_fail(int error_code, const char *error)
{
   struct rarch_state *p_rarch = &rarch_st;
   /* We cannot longjmp unless we're in retroarch_main_init().
    * If not, something went very wrong, and we should
    * just exit right away. */
   retro_assert(p_rarch->rarch_error_on_init);

   strlcpy(p_rarch->error_string,
         error, sizeof(p_rarch->error_string));
   longjmp(p_rarch->error_sjlj_context, error_code);
}

bool retroarch_main_quit(void)
{
   struct rarch_state *p_rarch = &rarch_st;
#ifdef HAVE_DISCORD
   if (discord_is_inited)
   {
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_SHUTDOWN;
      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
   }
   if (discord_is_ready())
      discord_shutdown();
   discord_is_inited          = false;
#endif

   if (!p_rarch->runloop_shutdown_initiated)
   {
      command_event_save_auto_state();
#ifdef HAVE_CONFIGFILE
      if (p_rarch->runloop_overrides_active)
         command_event_disable_overrides();
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
      retroarch_unset_runtime_shader_preset();
#endif

#ifdef HAVE_CONFIGFILE
      if (     p_rarch->runloop_remaps_core_active
            || p_rarch->runloop_remaps_content_dir_active
            || p_rarch->runloop_remaps_game_active
         )
         input_remapping_set_defaults(true);
#endif
   }

   p_rarch->runloop_shutdown_initiated = true;
   retroarch_menu_running_finished(true);

   return true;
}

void runloop_msg_queue_push(const char *msg,
      unsigned prio, unsigned duration,
      bool flush,
      char *title,
      enum message_queue_icon icon,
      enum message_queue_category category)
{
   struct rarch_state *p_rarch = &rarch_st;

   runloop_msg_queue_lock();
#ifdef HAVE_ACCESSIBILITY
   if (is_accessibility_enabled())
      accessibility_speak_priority((char*) msg, 0);
#endif
#if defined(HAVE_GFX_WIDGETS)
   if (gfx_widgets_active())
   {
      gfx_widgets_msg_queue_push(NULL, msg,
            roundf((float)duration / 60.0f * 1000.0f),
            title, icon, category, prio, flush,
#ifdef HAVE_MENU
            p_rarch->menu_driver_alive
#else
            false
#endif
            );
      duration = duration * 60 / 1000;
   }
   else
#endif
   {
      if (flush)
         msg_queue_clear(p_rarch->runloop_msg_queue);

      if (p_rarch->runloop_msg_queue)
         msg_queue_push(p_rarch->runloop_msg_queue, msg,
               prio, duration,
               title, icon, category);

      p_rarch->runloop_msg_queue_size = msg_queue_size(p_rarch->runloop_msg_queue);
   }

   ui_companion_driver_msg_queue_push(msg,
         prio, duration, flush);

   runloop_msg_queue_unlock();
}

void runloop_get_status(bool *is_paused, bool *is_idle,
      bool *is_slowmotion, bool *is_perfcnt_enable)
{
   struct rarch_state *p_rarch = &rarch_st;
   *is_paused                  = p_rarch->runloop_paused;
   *is_idle                    = p_rarch->runloop_idle;
   *is_slowmotion              = p_rarch->runloop_slowmotion;
   *is_perfcnt_enable          = p_rarch->runloop_perfcnt_enable;
}

#ifdef HAVE_MENU
static bool input_driver_toggle_button_combo(
      unsigned mode,
      retro_time_t current_time,
      input_bits_t* p_input)
{
   switch (mode)
   {
      case INPUT_TOGGLE_DOWN_Y_L_R:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_Y))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_L3_R3:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R3))
            return false;
         break;
      case INPUT_TOGGLE_L1_R1_START_SELECT:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_START_SELECT:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         break;
      case INPUT_TOGGLE_L3_R:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_L_R:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_DOWN_SELECT:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         break;
      case INPUT_TOGGLE_HOLD_START:
      {
         static rarch_timer_t timer = {0};

         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
         {
            /* timer only runs while start is held down */
            rarch_timer_end(&timer);
            return false;
         }

         /* user started holding down the start button, start the timer */
         if (!rarch_timer_is_running(&timer))
         {
            rarch_timer_begin_new_time_us(&timer,
                  HOLD_START_DELAY_SEC * 1000000);
            timer.timer_begin = true;
            timer.timer_end   = false;
         }

         rarch_timer_tick(&timer, current_time);

         if (!timer.timer_end && rarch_timer_has_expired(&timer))
         {
            /* start has been held down long enough, stop timer and enter menu */
            rarch_timer_end(&timer);
            return true;
         }

         return false;
      }
      default:
      case INPUT_TOGGLE_NONE:
         return false;
   }

   return true;
}
#endif

#if defined(HAVE_MENU)
static bool menu_display_libretro_running(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   bool menu_pause_libretro    = settings->bools.menu_pause_libretro;
   bool rarch_is_inited        = p_rarch->rarch_is_inited;
   bool check                  = !menu_pause_libretro
      && rarch_is_inited
      && (p_rarch->current_core_type != CORE_TYPE_DUMMY);
   return check;
}

/* Display the libretro core's framebuffer onscreen. */
static bool menu_display_libretro(retro_time_t current_time)
{
   struct rarch_state *p_rarch = &rarch_st;
   bool runloop_idle           = p_rarch->runloop_idle;

   if (  p_rarch->video_driver_poke && 
         p_rarch->video_driver_poke->set_texture_enable)
      p_rarch->video_driver_poke->set_texture_enable(
            p_rarch->video_driver_data,
            true, false);

   if (menu_display_libretro_running())
   {
      if (!p_rarch->input_driver_block_libretro_input)
         p_rarch->input_driver_block_libretro_input = true;

      core_run();
      p_rarch->libretro_core_runtime_usec        += 
         rarch_core_runtime_tick(current_time);
      p_rarch->input_driver_block_libretro_input  = false;

      return true;
   }

   if (runloop_idle)
   {
#ifdef HAVE_DISCORD
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_GAME_PAUSED;

      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
#endif
      return false; /* Return false here for indication of idleness */
   }

   video_driver_cached_frame();
   return true;
}
#endif

static void update_savestate_slot(void)
{
   char msg[128];
   struct rarch_state *p_rarch = &rarch_st;
   settings_t        *settings = p_rarch->configuration_settings;
   int        state_slot       = settings->ints.state_slot;

   msg[0] = '\0';

   snprintf(msg, sizeof(msg), "%s: %d",
         msg_hash_to_str(MSG_STATE_SLOT),
         state_slot);

   runloop_msg_queue_push(msg, 2, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   RARCH_LOG("%s\n", msg);
}

#if defined(HAVE_GFX_WIDGETS)
/* Display the fast forward state to the user, if needed. */
static void update_fastforwarding_state(void)
{
   struct rarch_state *p_rarch       = &rarch_st;
   bool runloop_fastmotion           = p_rarch->runloop_fastmotion;
   settings_t            *settings   = p_rarch->configuration_settings;

   p_rarch->gfx_widgets_fast_forward = runloop_fastmotion;

   if (!runloop_fastmotion)
      if (settings->bools.frame_time_counter_reset_after_fastforwarding)
         p_rarch->video_driver_frame_time_count = 0;
}
#endif

static enum runloop_state runloop_check_state(retro_time_t current_time)
{
   input_bits_t current_bits;
   rarch_joypad_info_t joypad_info;
#ifdef HAVE_MENU
   static input_bits_t last_input      = {{0}};
#endif
   static bool old_focus               = true;
   struct rarch_state *p_rarch         = &rarch_st;
   struct retro_callbacks *cbs         = &p_rarch->retro_ctx;
   settings_t *settings                = p_rarch->configuration_settings;
   bool is_focused                     = false;
   bool is_alive                       = false;
   uint64_t frame_count                = 0;
   bool focused                        = true;
   bool rarch_is_initialized           = p_rarch->rarch_is_inited;
   bool runloop_paused                 = p_rarch->runloop_paused;
   float fastforward_ratio             = settings->floats.fastforward_ratio;
   bool pause_nonactive                = settings->bools.pause_nonactive;
#ifdef HAVE_MENU
   unsigned menu_toggle_gamepad_combo  = settings->uints.input_menu_toggle_gamepad_combo;
   bool menu_driver_binding_state      = p_rarch->menu_driver_is_binding;
   bool menu_is_alive                  = p_rarch->menu_driver_alive;
   bool display_kb                     = menu_input_dialog_get_display_kb();
#endif
#if defined(HAVE_GFX_WIDGETS)
   bool widgets_active                 = gfx_widgets_active();
#endif

#if defined(HAVE_TRANSLATE) && defined(HAVE_GFX_WIDGETS)
   if (gfx_widgets_ai_service_overlay_get_state() == 3)
   {
      command_event(CMD_EVENT_PAUSE, NULL);
      gfx_widgets_ai_service_overlay_set_state(1);
   }
#endif

#ifdef HAVE_LIBNX
   /* Should be called once per frame */
   if (!appletMainLoop())
      return RUNLOOP_STATE_QUIT;
#endif

   BIT256_CLEAR_ALL_PTR(&current_bits);

   p_rarch->input_driver_block_libretro_input   = false;
   p_rarch->input_driver_block_hotkey           = false;

   if (p_rarch->current_input->keyboard_mapping_blocked)
      p_rarch->input_driver_block_hotkey        = true;

   joypad_info.joy_idx                          = 0;
   joypad_info.auto_binds                       = NULL;

#ifdef HAVE_MENU
   if (menu_is_alive && !(settings->bools.menu_unified_controls && !display_kb))
   {
      const struct retro_keybind *binds[MAX_USERS] = {NULL};
      input_menu_keys_pressed(&current_bits, &binds[0], &joypad_info);

      if (!display_kb)
      {
         unsigned i;
         unsigned ids[][2] =
         {
            {RETROK_SPACE,     RETRO_DEVICE_ID_JOYPAD_START   },
            {RETROK_SLASH,     RETRO_DEVICE_ID_JOYPAD_X       },
            {RETROK_RSHIFT,    RETRO_DEVICE_ID_JOYPAD_SELECT  },
            {RETROK_RIGHT,     RETRO_DEVICE_ID_JOYPAD_RIGHT   },
            {RETROK_LEFT,      RETRO_DEVICE_ID_JOYPAD_LEFT    },
            {RETROK_DOWN,      RETRO_DEVICE_ID_JOYPAD_DOWN    },
            {RETROK_UP,        RETRO_DEVICE_ID_JOYPAD_UP      },
            {RETROK_PAGEUP,    RETRO_DEVICE_ID_JOYPAD_L       },
            {RETROK_PAGEDOWN,  RETRO_DEVICE_ID_JOYPAD_R       },
            {0,                RARCH_QUIT_KEY                 },
            {0,                RARCH_FULLSCREEN_TOGGLE_KEY    },
            {RETROK_BACKSPACE, RETRO_DEVICE_ID_JOYPAD_B      },
            {RETROK_RETURN,    RETRO_DEVICE_ID_JOYPAD_A      },
            {RETROK_DELETE,    RETRO_DEVICE_ID_JOYPAD_Y      },
            {0,                RARCH_UI_COMPANION_TOGGLE     },
            {0,                RARCH_FPS_TOGGLE              },
            {0,                RARCH_SEND_DEBUG_INFO         },
            {0,                RARCH_NETPLAY_HOST_TOGGLE     },
            {0,                RARCH_MENU_TOGGLE             },
         };

         ids[9][0]  = input_config_binds[0][RARCH_QUIT_KEY].key;
         ids[10][0] = input_config_binds[0][RARCH_FULLSCREEN_TOGGLE_KEY].key;
         ids[14][0] = input_config_binds[0][RARCH_UI_COMPANION_TOGGLE].key;
         ids[15][0] = input_config_binds[0][RARCH_FPS_TOGGLE].key;
         ids[16][0] = input_config_binds[0][RARCH_SEND_DEBUG_INFO].key;
         ids[17][0] = input_config_binds[0][RARCH_NETPLAY_HOST_TOGGLE].key;
         ids[18][0] = input_config_binds[0][RARCH_MENU_TOGGLE].key;

         if (settings->bools.input_menu_swap_ok_cancel_buttons)
         {
            ids[11][1] = RETRO_DEVICE_ID_JOYPAD_A;
            ids[12][1] = RETRO_DEVICE_ID_JOYPAD_B;
         }

         for (i = 0; i < ARRAY_SIZE(ids); i++)
         {
            if (p_rarch->current_input->input_state(
                     p_rarch->current_input_data,
                     &joypad_info, binds, 0,
                     RETRO_DEVICE_KEYBOARD, 0, ids[i][0]))
               BIT256_SET_PTR(&current_bits, ids[i][1]);
         }
      }
   }
   else
#endif
   {
      input_keys_pressed(&current_bits, &joypad_info);
#ifdef HAVE_ACCESSIBILITY
#ifdef HAVE_TRANSLATE
      if (settings->bools.ai_service_enable)
      {
         unsigned i;
         reset_gamepad_input_override();
      
         for (i = 0; i < 16; i++)
         {
            if (p_rarch->ai_gamepad_state[i] == 2)
               set_gamepad_input_override(i, true);
            p_rarch->ai_gamepad_state[i] = 0;
         }
      }      
#endif
#endif
   }

#ifdef HAVE_MENU
   last_input                       = current_bits;
   if (
         ((menu_toggle_gamepad_combo != INPUT_TOGGLE_NONE) &&
          input_driver_toggle_button_combo(
             menu_toggle_gamepad_combo, current_time,
             &last_input)))
      BIT256_SET(current_bits, RARCH_MENU_TOGGLE);
#endif

   if (p_rarch->input_driver_flushing_input > 0)
   {
      bool input_active = bits_any_set(current_bits.data, ARRAY_SIZE(current_bits.data));

      p_rarch->input_driver_flushing_input = input_active 
         ? p_rarch->input_driver_flushing_input 
         : (p_rarch->input_driver_flushing_input - 1);

      if (input_active || (p_rarch->input_driver_flushing_input > 0))
      {
         BIT256_CLEAR_ALL(current_bits);
         if (runloop_paused)
            BIT256_SET(current_bits, RARCH_PAUSE_TOGGLE);
      }
   }

   if (!video_driver_is_threaded_internal())
   {
      const ui_application_t *application = p_rarch->ui_companion
         ? p_rarch->ui_companion->application 
         : NULL;
      if (application)
         application->process_events();
   }

   frame_count = p_rarch->video_driver_frame_count;
   is_alive    = p_rarch->current_video 
      ? p_rarch->current_video->alive(p_rarch->video_driver_data) 
      : true;
   is_focused  = video_has_focus();

#ifdef HAVE_MENU
   if (menu_driver_binding_state)
      BIT256_CLEAR_ALL(current_bits);
#endif

   /* Check fullscreen toggle */
   {
      bool fullscreen_toggled = !runloop_paused
#ifdef HAVE_MENU
         || menu_is_alive;
#else
      ;
#endif
      HOTKEY_CHECK(RARCH_FULLSCREEN_TOGGLE_KEY, CMD_EVENT_FULLSCREEN_TOGGLE,
            fullscreen_toggled, NULL);
   }

   /* Check mouse grab toggle */
   HOTKEY_CHECK(RARCH_GRAB_MOUSE_TOGGLE, CMD_EVENT_GRAB_MOUSE_TOGGLE, true, NULL);

#ifdef HAVE_OVERLAY
   if (settings->bools.input_overlay_enable)
   {
      static char prev_overlay_restore = false;
      static unsigned last_width       = 0;
      static unsigned last_height      = 0;
      bool check_next_rotation         = true;
      bool input_overlay_hide_in_menu  = settings->bools.input_overlay_hide_in_menu;
      bool input_overlay_auto_rotate   = settings->bools.input_overlay_auto_rotate;

      /* Check next overlay */
      HOTKEY_CHECK(RARCH_OVERLAY_NEXT, CMD_EVENT_OVERLAY_NEXT, true, &check_next_rotation);

      /* Ensure overlay is restored after displaying osk */
      if (p_rarch->input_driver_keyboard_linefeed_enable)
         prev_overlay_restore = true;
      else if (prev_overlay_restore)
      {
         if (!input_overlay_hide_in_menu)
            retroarch_overlay_init();
         prev_overlay_restore = false;
      }

      /* If video aspect ratio has changed, check overlay
       * rotation (if required) */
      if (input_overlay_auto_rotate)
      {
         if ((p_rarch->video_driver_width  != last_width) ||
             (p_rarch->video_driver_height != last_height))
         {
            input_overlay_auto_rotate_(p_rarch->overlay_ptr);
            last_width  = p_rarch->video_driver_width;
            last_height = p_rarch->video_driver_height;
         }
      }
   }
#endif

   /* Check quit key */
   {
      bool trig_quit_key, quit_press_twice;
      static bool quit_key     = false;
      static bool old_quit_key = false;
      static bool runloop_exec = false;
      quit_key                 = BIT256_GET(
            current_bits, RARCH_QUIT_KEY);
      trig_quit_key            = quit_key && !old_quit_key;
      old_quit_key             = quit_key;
      quit_press_twice         = settings->bools.quit_press_twice;

      /* Check double press if enabled */
      if (trig_quit_key && quit_press_twice)
      {
         static retro_time_t quit_key_time   = 0;
         retro_time_t cur_time               = current_time;
         trig_quit_key                       = (cur_time - quit_key_time < QUIT_DELAY_USEC);
         quit_key_time                       = cur_time;

         if (!trig_quit_key)
         {
            float target_hz = 0.0;

            rarch_environment_cb(
                  RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE, &target_hz);

            runloop_msg_queue_push(msg_hash_to_str(MSG_PRESS_AGAIN_TO_QUIT), 1,
                  QUIT_DELAY_USEC * target_hz / 1000000,
                  true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
      }

      if (TIME_TO_EXIT(trig_quit_key))
      {
         bool quit_runloop = false;

         if ((p_rarch->runloop_max_frames != 0) 
               && (frame_count >= p_rarch->runloop_max_frames)
               && p_rarch->runloop_max_frames_screenshot)
         {
            const char *screenshot_path = NULL;
            bool fullpath               = false;

            if (string_is_empty(p_rarch->runloop_max_frames_screenshot_path))
               screenshot_path          = path_get(RARCH_PATH_BASENAME);
            else
            {
               fullpath                 = true;
               screenshot_path          = p_rarch->runloop_max_frames_screenshot_path;
            }

            RARCH_LOG("Taking a screenshot before exiting...\n");

            /* Take a screenshot before we exit. */
            if (!take_screenshot(settings->paths.directory_screenshot,
                     screenshot_path, false,
                     video_driver_cached_frame_has_valid_framebuffer(), fullpath, false))
            {
               RARCH_ERR("Could not take a screenshot before exiting.\n");
            }
         }

         if (runloop_exec)
            runloop_exec = false;

         if (p_rarch->runloop_core_shutdown_initiated &&
               settings->bools.load_dummy_on_core_shutdown)
         {
            content_ctx_info_t content_info;

            content_info.argc               = 0;
            content_info.argv               = NULL;
            content_info.args               = NULL;
            content_info.environ_get        = NULL;

            if (task_push_start_dummy_core(&content_info))
            {
               /* Loads dummy core instead of exiting RetroArch completely.
                * Aborts core shutdown if invoked. */
               p_rarch->runloop_shutdown_initiated      = false;
               p_rarch->runloop_core_shutdown_initiated = false;
            }
            else
               quit_runloop              = true;
         }
         else
            quit_runloop                 = true;

         p_rarch->runloop_core_running   = false;

         if (quit_runloop)
         {
            old_quit_key                 = quit_key;
            retroarch_main_quit();
            return RUNLOOP_STATE_QUIT;
         }
      }
   }

#if defined(HAVE_MENU) || defined(HAVE_GFX_WIDGETS)
   gfx_animation_update(
         current_time,
         settings->bools.menu_timedate_enable,
         settings->floats.menu_ticker_speed,
         p_rarch->video_driver_width,
         p_rarch->video_driver_height);

#if defined(HAVE_GFX_WIDGETS)
   if (widgets_active)
   {
      bool rarch_force_fullscreen = p_rarch->rarch_force_fullscreen;
      bool video_is_fullscreen    = settings->bools.video_fullscreen ||
            rarch_force_fullscreen;

      runloop_msg_queue_lock();
      gfx_widgets_iterate(
            p_rarch->video_driver_width,
            p_rarch->video_driver_height,
            video_is_fullscreen,
            settings->paths.directory_assets,
            settings->paths.path_font,
            video_driver_is_threaded_internal());
      runloop_msg_queue_unlock();
   }
#endif

#ifdef HAVE_MENU
   if (menu_is_alive)
   {
      enum menu_action action;
      menu_ctx_iterate_t iter;
      static input_bits_t old_input = {{0}};
      static enum menu_action
         old_action                 = MENU_ACTION_CANCEL;
      bool focused                  = false;
      input_bits_t trigger_input    = current_bits;
      global_t *global              = &p_rarch->g_extern;

      cbs->poll_cb();

      bits_clear_bits(trigger_input.data, old_input.data,
            ARRAY_SIZE(trigger_input.data));
      action                    = (enum menu_action)menu_event(&current_bits, &trigger_input, display_kb);
      focused                   = pause_nonactive ? is_focused : true;
      focused                   = focused && 
         !p_rarch->main_ui_companion_is_on_foreground;

      iter.action               = action;

      if (global)
      {
         if (action == old_action)
         {
            retro_time_t press_time           = current_time;

            if (action == MENU_ACTION_NOOP)
               global->menu.noop_press_time   = press_time - global->menu.noop_start_time;
            else
               global->menu.action_press_time = press_time - global->menu.action_start_time;
         }
         else
         {
            if (action == MENU_ACTION_NOOP)
            {
               global->menu.noop_start_time      = current_time;
               global->menu.noop_press_time      = 0;

               if (global->menu.prev_action == old_action)
                  global->menu.action_start_time = global->menu.prev_start_time;
               else
                  global->menu.action_start_time = current_time;
            }
            else
            {
               if (  global->menu.prev_action == action &&
                     global->menu.noop_press_time < 200000) /* 250ms */
               {
                  global->menu.action_start_time = global->menu.prev_start_time;
                  global->menu.action_press_time = current_time - global->menu.action_start_time;
               }
               else
               {
                  global->menu.prev_start_time   = current_time;
                  global->menu.prev_action       = action;
                  global->menu.action_press_time = 0;
               }
            }
         }
      }

      if (!menu_driver_iterate(&iter, current_time))
         retroarch_menu_running_finished(false);

      if (focused || !p_rarch->runloop_idle)
      {
         bool libretro_running    = menu_display_libretro_running();
         menu_handle_t *menu_data = menu_driver_get_ptr();

         if (menu_data)
         {
            if (BIT64_GET(menu_data->state, MENU_STATE_RENDER_FRAMEBUFFER)
                  != BIT64_GET(menu_data->state, MENU_STATE_RENDER_MESSAGEBOX))
               BIT64_SET(menu_data->state, MENU_STATE_RENDER_FRAMEBUFFER);

            if (BIT64_GET(menu_data->state, MENU_STATE_RENDER_FRAMEBUFFER))
               gfx_display_set_framebuffer_dirty_flag();

            if (BIT64_GET(menu_data->state, MENU_STATE_RENDER_MESSAGEBOX)
                  && !string_is_empty(menu_data->menu_state_msg))
            {
               if (menu_data->driver_ctx->render_messagebox)
                  menu_data->driver_ctx->render_messagebox(
                        menu_data->userdata,
                        menu_data->menu_state_msg);

               if (p_rarch->main_ui_companion_is_on_foreground)
               {
                  if (  p_rarch->ui_companion && 
                        p_rarch->ui_companion->render_messagebox)
                     p_rarch->ui_companion->render_messagebox(menu_data->menu_state_msg);
               }
            }

            if (BIT64_GET(menu_data->state, MENU_STATE_BLIT))
            {
               if (menu_data->driver_ctx->render)
                  menu_data->driver_ctx->render(
                        menu_data->userdata,
                        p_rarch->video_driver_width,
                        p_rarch->video_driver_height,
                        p_rarch->runloop_idle);
            }

            if (p_rarch->menu_driver_alive && !p_rarch->runloop_idle)
               menu_display_libretro(current_time);

            if (menu_data->driver_ctx->set_texture)
               menu_data->driver_ctx->set_texture();

            menu_data->state               = 0;
         }

         if (settings->bools.audio_enable_menu &&
               !libretro_running)
            audio_driver_menu_sample();
      }

      old_input                 = current_bits;
      old_action                = action;

      if (!focused || p_rarch->runloop_idle)
         return RUNLOOP_STATE_POLLED_AND_SLEEP;
   }
   else
#endif
#endif
   {
      if (p_rarch->runloop_idle)
      {
         cbs->poll_cb();
         return RUNLOOP_STATE_POLLED_AND_SLEEP;
      }
   }

   /* Check game focus toggle */
   HOTKEY_CHECK(RARCH_GAME_FOCUS_TOGGLE, CMD_EVENT_GAME_FOCUS_TOGGLE, true, NULL);
   /* Check if we have pressed the UI companion toggle button */
   HOTKEY_CHECK(RARCH_UI_COMPANION_TOGGLE, CMD_EVENT_UI_COMPANION_TOGGLE, true, NULL);

#ifdef HAVE_MENU
   /* Check if we have pressed the menu toggle button */
   {
      static bool old_pressed = false;
      char *menu_driver       = settings->arrays.menu_driver;
      bool pressed            = BIT256_GET(
            current_bits, RARCH_MENU_TOGGLE) &&
         !string_is_equal(menu_driver, "null");
      bool core_type_is_dummy = p_rarch->current_core_type == CORE_TYPE_DUMMY;

      if (p_rarch->menu_keyboard_key_state[RETROK_F1] == 1)
      {
         if (p_rarch->menu_driver_alive)
         {
            if (rarch_is_initialized && !core_type_is_dummy)
            {
               retroarch_menu_running_finished(false);
               p_rarch->menu_keyboard_key_state[RETROK_F1] = 
                  ((p_rarch->menu_keyboard_key_state[RETROK_F1] & 1) << 1) | false;
            }
         }
      }
      else if ((!p_rarch->menu_keyboard_key_state[RETROK_F1] &&
               (pressed && !old_pressed)) ||
            core_type_is_dummy)
      {
         if (p_rarch->menu_driver_alive)
         {
            if (rarch_is_initialized && !core_type_is_dummy)
               retroarch_menu_running_finished(false);
         }
         else
         {
            retroarch_menu_running();
         }
      }
      else
         p_rarch->menu_keyboard_key_state[RETROK_F1] = 
            ((p_rarch->menu_keyboard_key_state[RETROK_F1] & 1) << 1) | false;

      old_pressed             = pressed;
   }

   /* Check if we have pressed the FPS toggle button */
   HOTKEY_CHECK(RARCH_FPS_TOGGLE, CMD_EVENT_FPS_TOGGLE, true, NULL);

   /* Check if we have pressed the netplay host toggle button */
   HOTKEY_CHECK(RARCH_NETPLAY_HOST_TOGGLE, CMD_EVENT_NETPLAY_HOST_TOGGLE, true, NULL);

   if (p_rarch->menu_driver_alive)
   {
      if (!settings->bools.menu_throttle_framerate && !fastforward_ratio)
         return RUNLOOP_STATE_MENU_ITERATE;

      return RUNLOOP_STATE_END;
   }
#endif

   if (pause_nonactive)
      focused                = is_focused;

   /* Check if we have pressed the screenshot toggle button */
   HOTKEY_CHECK(RARCH_SCREENSHOT, CMD_EVENT_TAKE_SCREENSHOT, true, NULL);

   /* Check if we have pressed the audio mute toggle button */
   HOTKEY_CHECK(RARCH_MUTE, CMD_EVENT_AUDIO_MUTE_TOGGLE, true, NULL);

   /* Check if we have pressed the OSK toggle button */
   HOTKEY_CHECK(RARCH_OSK, CMD_EVENT_OSK_TOGGLE, true, NULL);

   /* Check if we have pressed the recording toggle button */
   HOTKEY_CHECK(RARCH_RECORDING_TOGGLE, CMD_EVENT_RECORDING_TOGGLE, true, NULL);

   /* Check if we have pressed the AI Service toggle button */
   HOTKEY_CHECK(RARCH_AI_SERVICE, CMD_EVENT_AI_SERVICE_TOGGLE, true, NULL);

   /* Check if we have pressed the streaming toggle button */
   HOTKEY_CHECK(RARCH_STREAMING_TOGGLE, CMD_EVENT_STREAMING_TOGGLE, true, NULL);

   if (BIT256_GET(current_bits, RARCH_VOLUME_UP))
      command_event(CMD_EVENT_VOLUME_UP, NULL);
   else if (BIT256_GET(current_bits, RARCH_VOLUME_DOWN))
      command_event(CMD_EVENT_VOLUME_DOWN, NULL);

#ifdef HAVE_NETWORKING
   /* Check Netplay */
   HOTKEY_CHECK(RARCH_NETPLAY_GAME_WATCH, CMD_EVENT_NETPLAY_GAME_WATCH, true, NULL);
#endif

   /* Check if we have pressed the pause button */
   {
      static bool old_frameadvance  = false;
      static bool old_pause_pressed = false;
      bool frameadvance_pressed     = BIT256_GET(
            current_bits, RARCH_FRAMEADVANCE);
      bool pause_pressed            = BIT256_GET(
            current_bits, RARCH_PAUSE_TOGGLE);
      bool trig_frameadvance        = frameadvance_pressed && !old_frameadvance;

      /* Check if libretro pause key was pressed. If so, pause or
       * unpause the libretro core. */

      /* FRAMEADVANCE will set us into pause mode. */
      pause_pressed                |= !p_rarch->runloop_paused 
         && trig_frameadvance;

      if (focused && pause_pressed && !old_pause_pressed)
         command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
      else if (focused && !old_focus)
         command_event(CMD_EVENT_UNPAUSE, NULL);
      else if (!focused && old_focus)
         command_event(CMD_EVENT_PAUSE, NULL);

      old_focus           = focused;
      old_pause_pressed   = pause_pressed;
      old_frameadvance    = frameadvance_pressed;

      if (p_rarch->runloop_paused)
      {
         bool toggle = !p_rarch->runloop_idle ? true : false;

         HOTKEY_CHECK(RARCH_FULLSCREEN_TOGGLE_KEY,
               CMD_EVENT_FULLSCREEN_TOGGLE, true, &toggle);

         /* Check if it's not oneshot */
         if (!(trig_frameadvance || BIT256_GET(current_bits, RARCH_REWIND)))
            focused = false;
      }
   }

#ifdef HAVE_ACCESSIBILITY
#ifdef HAVE_TRANSLATE
   /* Copy over the retropad state to a buffer for the translate service
      to send off if it's run. */
   if (settings->bools.ai_service_enable)
   {
      p_rarch->ai_gamepad_state[0]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_B);
      p_rarch->ai_gamepad_state[1]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_Y);
      p_rarch->ai_gamepad_state[2]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_SELECT);
      p_rarch->ai_gamepad_state[3]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_START);

      p_rarch->ai_gamepad_state[4]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_UP);
      p_rarch->ai_gamepad_state[5]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_DOWN);
      p_rarch->ai_gamepad_state[6]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_LEFT);
      p_rarch->ai_gamepad_state[7]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_RIGHT);

      p_rarch->ai_gamepad_state[8]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_A);
      p_rarch->ai_gamepad_state[9]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_X);
      p_rarch->ai_gamepad_state[10] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_L);
      p_rarch->ai_gamepad_state[11] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_R);

      p_rarch->ai_gamepad_state[12] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_L2);
      p_rarch->ai_gamepad_state[13] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_R2);
      p_rarch->ai_gamepad_state[14] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_L3);
      p_rarch->ai_gamepad_state[15] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_R3);
   }
#endif
#endif

   if (!focused)
   {
      cbs->poll_cb();
      return RUNLOOP_STATE_POLLED_AND_SLEEP;
   }

   /* Check if we have pressed the fast forward button */
   /* To avoid continous switching if we hold the button down, we require
    * that the button must go from pressed to unpressed back to pressed
    * to be able to toggle between then.
    */
   {
      static bool old_button_state      = false;
      static bool old_hold_button_state = false;
      bool new_button_state             = BIT256_GET(
            current_bits, RARCH_FAST_FORWARD_KEY);
      bool new_hold_button_state        = BIT256_GET(
            current_bits, RARCH_FAST_FORWARD_HOLD_KEY);
      bool check1                       = !new_hold_button_state;
      bool check2                       = new_button_state && !old_button_state;

      if (check2)
         check1                         = p_rarch->input_driver_nonblock_state;
      else
         check2                         = old_hold_button_state != new_hold_button_state;

      if (check2)
      {
         if (check1)
         {
            p_rarch->input_driver_nonblock_state = false;
            p_rarch->runloop_fastmotion          = false;
            p_rarch->fastforward_after_frames    = 1;
         }
         else
         {
            p_rarch->input_driver_nonblock_state = true;
            p_rarch->runloop_fastmotion          = true;
         }
         driver_set_nonblock_state();
#if defined(HAVE_GFX_WIDGETS)
         if (widgets_active)
            update_fastforwarding_state();
#endif
      }

      old_button_state                  = new_button_state;
      old_hold_button_state             = new_hold_button_state;

      /* Show the fast-forward OSD for 1 frame every frame if 
       * display widgets are disabled */
#if defined(HAVE_GFX_WIDGETS)
      if (!widgets_active && p_rarch->runloop_fastmotion)
#else
      if (p_rarch->runloop_fastmotion)
#endif
      {
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAST_FORWARD), 1, 1, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

   /* Check if we have pressed any of the state slot buttons */
   {
      static bool old_should_slot_increase = false;
      static bool old_should_slot_decrease = false;
      bool should_slot_increase            = BIT256_GET(
            current_bits, RARCH_STATE_SLOT_PLUS);
      bool should_slot_decrease            = BIT256_GET(
            current_bits, RARCH_STATE_SLOT_MINUS);
      bool check1                          = true;
      bool check2                          = should_slot_increase && !old_should_slot_increase;
      int addition                         = 1;
      int state_slot                       = settings->ints.state_slot;

      if (!check2)
      {
         check2                            = should_slot_decrease && !old_should_slot_decrease;
         check1                            = state_slot > 0;
         addition                          = -1;
      }

      /* Checks if the state increase/decrease keys have been pressed
       * for this frame. */
      if (check2)
      {
         int cur_state_slot                = state_slot;
         if (check1)
            configuration_set_int(settings, settings->ints.state_slot,
                  cur_state_slot + addition);
         update_savestate_slot();
      }

      old_should_slot_increase = should_slot_increase;
      old_should_slot_decrease = should_slot_decrease;
   }

   /* Check if we have pressed any of the savestate buttons */
   HOTKEY_CHECK(RARCH_SAVE_STATE_KEY, CMD_EVENT_SAVE_STATE, true, NULL);
   HOTKEY_CHECK(RARCH_LOAD_STATE_KEY, CMD_EVENT_LOAD_STATE, true, NULL);

#ifdef HAVE_CHEEVOS
   rcheevos_hardcore_active = settings->bools.cheevos_enable
      && settings->bools.cheevos_hardcore_mode_enable
      && !rcheevos_hardcore_paused;

   if (rcheevos_hardcore_active && rcheevos_state_loaded_flag)
   {
      rcheevos_hardcore_paused = true;
      runloop_msg_queue_push(msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_DISABLED), 0, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   if (!rcheevos_hardcore_active)
#endif
   {
      char s[128];
      bool rewinding = false;
      unsigned t     = 0;

      s[0]           = '\0';

      rewinding      = state_manager_check_rewind(
            BIT256_GET(current_bits, RARCH_REWIND),
            settings->uints.rewind_granularity,
            p_rarch->runloop_paused,
            s, sizeof(s), &t);

#if defined(HAVE_GFX_WIDGETS)
      if (widgets_active)
         p_rarch->gfx_widgets_rewinding = rewinding;
      else
#endif
      {
         if (rewinding)
            runloop_msg_queue_push(s, 0, t, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

   /* Checks if slowmotion toggle/hold was being pressed and/or held. */
#ifdef HAVE_CHEEVOS
   if (!rcheevos_hardcore_active)
#endif
   {
      static bool old_slowmotion_button_state      = false;
      static bool old_slowmotion_hold_button_state = false;
      bool new_slowmotion_button_state             = BIT256_GET(
            current_bits, RARCH_SLOWMOTION_KEY);
      bool new_slowmotion_hold_button_state        = BIT256_GET(
            current_bits, RARCH_SLOWMOTION_HOLD_KEY);

      if (new_slowmotion_button_state && !old_slowmotion_button_state)
         p_rarch->runloop_slowmotion = !p_rarch->runloop_slowmotion;
      else if (old_slowmotion_hold_button_state != new_slowmotion_hold_button_state)
         p_rarch->runloop_slowmotion = new_slowmotion_hold_button_state;

      if (p_rarch->runloop_slowmotion)
      {
         if (settings->bools.video_black_frame_insertion)
            if (!p_rarch->runloop_idle)
               video_driver_cached_frame();

#if defined(HAVE_GFX_WIDGETS)
         if (!widgets_active)
#endif
         {
            if (state_manager_frame_is_reversed())
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_SLOW_MOTION_REWIND), 1, 1, false, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_SLOW_MOTION), 1, 1, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
      }

      old_slowmotion_button_state                  = new_slowmotion_button_state;
      old_slowmotion_hold_button_state             = new_slowmotion_hold_button_state;
   }

   /* Check movie record toggle */
   HOTKEY_CHECK(RARCH_BSV_RECORD_TOGGLE, CMD_EVENT_BSV_RECORDING_TOGGLE, true, NULL);

   /* Check shader prev/next */
   HOTKEY_CHECK(RARCH_SHADER_NEXT, CMD_EVENT_SHADER_NEXT, true, NULL);
   HOTKEY_CHECK(RARCH_SHADER_PREV, CMD_EVENT_SHADER_PREV, true, NULL);

   /* Check if we have pressed any of the disk buttons */
   HOTKEY_CHECK3(
         RARCH_DISK_EJECT_TOGGLE, CMD_EVENT_DISK_EJECT_TOGGLE,
         RARCH_DISK_NEXT,         CMD_EVENT_DISK_NEXT,
         RARCH_DISK_PREV,         CMD_EVENT_DISK_PREV);

   /* Check if we have pressed the reset button */
   HOTKEY_CHECK(RARCH_RESET, CMD_EVENT_RESET, true, NULL);

   /* Check cheats */
   HOTKEY_CHECK3(
         RARCH_CHEAT_INDEX_PLUS,  CMD_EVENT_CHEAT_INDEX_PLUS,
         RARCH_CHEAT_INDEX_MINUS, CMD_EVENT_CHEAT_INDEX_MINUS,
         RARCH_CHEAT_TOGGLE,      CMD_EVENT_CHEAT_TOGGLE);


#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   if (settings->bools.video_shader_watch_files)
   {
      static rarch_timer_t timer = {0};
      static bool need_to_apply = false;

      if (video_shader_check_for_changes())
      {
         need_to_apply = true;

         if (!rarch_timer_is_running(&timer))
         {
            rarch_timer_begin_new_time_us(&timer,
                  SHADER_FILE_WATCH_DELAY_MSEC * 1000);
            timer.timer_begin = true;
            timer.timer_end   = false;
         }
      }

      /* If a file is modified atomically (moved/renamed from a different file),
       * we have no idea how long that might take.
       * If we're trying to re-apply shaders immediately after changes are made
       * to the original file(s), the filesystem might be in an in-between
       * state where the new file hasn't been moved over yet and the original
       * file was already deleted. This leaves us no choice but to wait an
       * arbitrary amount of time and hope for the best.
       */
      if (need_to_apply)
      {
         rarch_timer_tick(&timer, current_time);

         if (!timer.timer_end && rarch_timer_has_expired(&timer))
         {
            rarch_timer_end(&timer);
            need_to_apply = false;
            command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
         }
      }
   }

   if (   settings->uints.video_shader_delay && 
         !p_rarch->shader_delay_timer.timer_end)
   {
      if (!rarch_timer_is_running(&p_rarch->shader_delay_timer))
      {
         rarch_timer_begin_new_time_us(&p_rarch->shader_delay_timer,
               settings->uints.video_shader_delay * 1000);
         p_rarch->shader_delay_timer.timer_begin = true;
         p_rarch->shader_delay_timer.timer_end   = false;
      }
      else
      {
         rarch_timer_tick(&p_rarch->shader_delay_timer, current_time);

         if (rarch_timer_has_expired(&p_rarch->shader_delay_timer))
         {
            rarch_timer_end(&p_rarch->shader_delay_timer);

            {
               const char *preset = retroarch_get_shader_preset();
               enum rarch_shader_type type = video_shader_parse_type(preset);
               retroarch_apply_shader(type, preset, false);
            }
         }
      }
   }
#endif

   return RUNLOOP_STATE_ITERATE;
}

/**
 * runloop_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on success, 1 if we have to wait until
 * button input in order to wake up the loop,
 * -1 if we forcibly quit out of the RetroArch iteration loop.
 **/
int runloop_iterate(void)
{
   unsigned i;
   struct rarch_state                  *p_rarch = &rarch_st;
   settings_t *settings                         = p_rarch->configuration_settings;
   float fastforward_ratio                      = settings->floats.fastforward_ratio;
   unsigned video_frame_delay                   = settings->uints.video_frame_delay;
   bool vrr_runloop_enable                      = settings->bools.vrr_runloop_enable;
   unsigned max_users                           = p_rarch->input_driver_max_users;
   retro_time_t current_time                    = cpu_features_get_time_usec();

#ifdef HAVE_DISCORD
   if (discord_is_inited)
      Discord_RunCallbacks();
#endif

   if (p_rarch->runloop_frame_time.callback)
   {
      /* Updates frame timing if frame timing callback is in use by the core.
       * Limits frame time if fast forward ratio throttle is enabled. */
      retro_usec_t runloop_last_frame_time = p_rarch->runloop_frame_time_last;
      retro_time_t current                 = current_time;
      bool is_locked_fps                   = (p_rarch->runloop_paused 
            || p_rarch->input_driver_nonblock_state)
            | !!p_rarch->recording_data;
      retro_time_t delta                   = (!runloop_last_frame_time || is_locked_fps) 
         ? p_rarch->runloop_frame_time.reference
         : (current - runloop_last_frame_time);

      if (is_locked_fps)
         p_rarch->runloop_frame_time_last  = 0;
      else
      {
         float slowmotion_ratio            = 
            settings->floats.slowmotion_ratio;

         p_rarch->runloop_frame_time_last  = current;

         if (p_rarch->runloop_slowmotion)
            delta /= slowmotion_ratio;
      }

      p_rarch->runloop_frame_time.callback(delta);
   }

   switch ((enum runloop_state)runloop_check_state(current_time))
   {
      case RUNLOOP_STATE_QUIT:
         p_rarch->frame_limit_last_time = 0.0;
         p_rarch->runloop_core_running  = false;
         command_event(CMD_EVENT_QUIT, NULL);
         return -1;
      case RUNLOOP_STATE_POLLED_AND_SLEEP:
#ifdef HAVE_NETWORKING
         /* FIXME: This is an ugly way to tell Netplay this... */
         netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL);
#endif
#if defined(HAVE_COCOATOUCH)
         if (!p_rarch->main_ui_companion_is_on_foreground)
#endif
            retro_sleep(10);
         return 1;
      case RUNLOOP_STATE_END:
#ifdef HAVE_NETWORKING
         /* FIXME: This is an ugly way to tell Netplay this... */
         if (settings->bools.menu_pause_libretro &&
               netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL)
            )
            netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL);
#endif
         goto end;
      case RUNLOOP_STATE_MENU_ITERATE:
#ifdef HAVE_NETWORKING
         /* FIXME: This is an ugly way to tell Netplay this... */
         netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL);
#endif
         return 0;
      case RUNLOOP_STATE_ITERATE:
         p_rarch->runloop_core_running = true;
         break;
   }

#ifdef HAVE_THREADS
   if (p_rarch->runloop_autosave)
      autosave_lock();
#endif

   /* Used for rewinding while playback/record. */
   if (p_rarch->bsv_movie_state_handle)
      p_rarch->bsv_movie_state_handle->frame_pos[p_rarch->bsv_movie_state_handle->frame_ptr]
         = intfstream_tell(p_rarch->bsv_movie_state_handle->file);

   if (  p_rarch->camera_cb.caps && 
         p_rarch->camera_driver  &&
         p_rarch->camera_driver->poll &&
         p_rarch->camera_data)
      p_rarch->camera_driver->poll(p_rarch->camera_data,
            p_rarch->camera_cb.frame_raw_framebuffer,
            p_rarch->camera_cb.frame_opengl_texture);

   /* Update binds for analog dpad modes. */
   for (i = 0; i < max_users; i++)
   {
      enum analog_dpad_mode dpad_mode     = (enum analog_dpad_mode)settings->uints.input_analog_dpad_mode[i];

      if (dpad_mode != ANALOG_DPAD_NONE)
      {
         struct retro_keybind *general_binds = input_config_binds[i];
         struct retro_keybind *auto_binds    = input_autoconf_binds[i];
         input_push_analog_dpad(general_binds, dpad_mode);
         input_push_analog_dpad(auto_binds,    dpad_mode);
      }
   }

   if ((video_frame_delay > 0) && !p_rarch->input_driver_nonblock_state)
      retro_sleep(video_frame_delay);

   {
#ifdef HAVE_RUNAHEAD
      unsigned run_ahead_num_frames = settings->uints.run_ahead_frames;
      /* Run Ahead Feature replaces the call to core_run in this loop */
      bool want_runahead            = settings->bools.run_ahead_enabled && run_ahead_num_frames > 0;
#ifdef HAVE_NETWORKING
      want_runahead                 = want_runahead && !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL);
#endif

      if (want_runahead)
         do_runahead(run_ahead_num_frames, settings->bools.run_ahead_secondary_instance);
      else
#endif
         core_run();
   }

   /* Increment runtime tick counter after each call to
    * core_run() or run_ahead() */
   p_rarch->libretro_core_runtime_usec += rarch_core_runtime_tick(current_time);

#ifdef HAVE_CHEEVOS
   if (settings->bools.cheevos_enable && rcheevos_loaded)
      rcheevos_test();
#endif
   cheat_manager_apply_retro_cheats();

#ifdef HAVE_DISCORD
   if (discord_is_inited && discord_is_ready())
      discord_update(DISCORD_PRESENCE_GAME, settings->bools.playlist_fuzzy_archive_match);
#endif

   for (i = 0; i < max_users; i++)
   {
      enum analog_dpad_mode dpad_mode     = (enum analog_dpad_mode)settings->uints.input_analog_dpad_mode[i];

      if (dpad_mode != ANALOG_DPAD_NONE)
      {
         struct retro_keybind *general_binds = input_config_binds[i];
         struct retro_keybind *auto_binds    = input_autoconf_binds[i];

         input_pop_analog_dpad(general_binds);
         input_pop_analog_dpad(auto_binds);
      }
   }

   if (p_rarch->bsv_movie_state_handle)
   {
      p_rarch->bsv_movie_state_handle->frame_ptr    =
         (p_rarch->bsv_movie_state_handle->frame_ptr + 1)
         & p_rarch->bsv_movie_state_handle->frame_mask;

      p_rarch->bsv_movie_state_handle->first_rewind =
         !p_rarch->bsv_movie_state_handle->did_rewind;
      p_rarch->bsv_movie_state_handle->did_rewind   = false;
   }

#ifdef HAVE_THREADS
   if (p_rarch->runloop_autosave)
      autosave_unlock();
#endif

   /* Condition for max speed x0.0 when vrr_runloop is off to skip that part */
   if (!(fastforward_ratio || vrr_runloop_enable))
      return 0;

end:
   if (vrr_runloop_enable)
   {
      struct retro_system_av_info *av_info = &p_rarch->video_driver_av_info;
      bool audio_sync                      = settings->bools.audio_sync;

      /* Sync on video only, block audio later. */
      if (p_rarch->fastforward_after_frames && audio_sync)
      {
         if (p_rarch->fastforward_after_frames == 1)
         {
            /* Nonblocking audio */
            if (p_rarch->audio_driver_active && 
                  p_rarch->audio_driver_context_audio_data)
               p_rarch->current_audio->set_nonblock_state(
                     p_rarch->audio_driver_context_audio_data, true);
            p_rarch->audio_driver_chunk_size = 
               p_rarch->audio_driver_chunk_nonblock_size;
         }

         p_rarch->fastforward_after_frames++;

         if (p_rarch->fastforward_after_frames == 6)
         {
            /* Blocking audio */
            if (p_rarch->audio_driver_active && 
                  p_rarch->audio_driver_context_audio_data)
               p_rarch->current_audio->set_nonblock_state(
                     p_rarch->audio_driver_context_audio_data,
                     audio_sync ? false : true);

            p_rarch->audio_driver_chunk_size  = 
               p_rarch->audio_driver_chunk_block_size;
            p_rarch->fastforward_after_frames = 0;
         }
      }

      /* Fast Forward for max speed x0.0 */
      if (!fastforward_ratio && p_rarch->runloop_fastmotion)
         return 0;

      p_rarch->frame_limit_minimum_time =
         (retro_time_t)roundf(1000000.0f / (av_info->timing.fps *
                  (p_rarch->runloop_fastmotion 
                   ? fastforward_ratio : 1.0f)));
   }

   {
      retro_time_t to_sleep_ms  = (
            (p_rarch->frame_limit_last_time + p_rarch->frame_limit_minimum_time)
            - cpu_features_get_time_usec()) / 1000;

      if (to_sleep_ms > 0)
      {
         unsigned               sleep_ms = (unsigned)to_sleep_ms;

         /* Combat jitter a bit. */
         p_rarch->frame_limit_last_time += p_rarch->frame_limit_minimum_time;

         if (sleep_ms > 0)
#if defined(HAVE_COCOATOUCH)
            if (!p_rarch->main_ui_companion_is_on_foreground)
#endif
               retro_sleep(sleep_ms);
         return 1;
      }
   }

   p_rarch->frame_limit_last_time  = cpu_features_get_time_usec();

   return 0;
}

rarch_system_info_t *runloop_get_system_info(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return &p_rarch->runloop_system;
}

struct retro_system_info *runloop_get_libretro_system_info(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return &p_rarch->runloop_system.info;
}

void retroarch_force_video_driver_fallback(const char *driver)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;
   ui_msg_window_t *msg_window = NULL;

   configuration_set_string(settings,
         settings->arrays.video_driver,
         driver);

   command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && !defined(WINAPI_FAMILY)
   /* UI companion driver is not inited yet, just call into it directly */
   msg_window = &ui_msg_window_win32;
#endif

   if (msg_window)
   {
      char text[PATH_MAX_LENGTH];
      ui_msg_window_state window_state;
      char *title          = strdup(msg_hash_to_str(MSG_ERROR));

      text[0]              = '\0';

      snprintf(text, sizeof(text),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK),
            driver);

      window_state.buttons = UI_MSG_WINDOW_OK;
      window_state.text    = strdup(text);
      window_state.title   = title;
      window_state.window  = NULL;

      msg_window->error(&window_state);

      free(title);
   }
   exit(1);
}

enum retro_language rarch_get_language_from_iso(const char *iso639)
{
   unsigned i;
   enum retro_language lang = RETRO_LANGUAGE_ENGLISH;

   struct lang_pair
   {
      const char *iso639;
      enum retro_language lang;
   };

   const struct lang_pair pairs[] =
   {
      {"en", RETRO_LANGUAGE_ENGLISH},
      {"ja", RETRO_LANGUAGE_JAPANESE},
      {"fr", RETRO_LANGUAGE_FRENCH},
      {"es", RETRO_LANGUAGE_SPANISH},
      {"de", RETRO_LANGUAGE_GERMAN},
      {"it", RETRO_LANGUAGE_ITALIAN},
      {"nl", RETRO_LANGUAGE_DUTCH},
      {"pt_BR", RETRO_LANGUAGE_PORTUGUESE_BRAZIL},
      {"pt_PT", RETRO_LANGUAGE_PORTUGUESE_PORTUGAL},
      {"pt", RETRO_LANGUAGE_PORTUGUESE_PORTUGAL},
      {"ru", RETRO_LANGUAGE_RUSSIAN},
      {"ko", RETRO_LANGUAGE_KOREAN},
      {"zh_CN", RETRO_LANGUAGE_CHINESE_SIMPLIFIED},
      {"zh_SG", RETRO_LANGUAGE_CHINESE_SIMPLIFIED},
      {"zh_HK", RETRO_LANGUAGE_CHINESE_TRADITIONAL},
      {"zh_TW", RETRO_LANGUAGE_CHINESE_TRADITIONAL},
      {"zh", RETRO_LANGUAGE_CHINESE_SIMPLIFIED},
      {"eo", RETRO_LANGUAGE_ESPERANTO},
      {"pl", RETRO_LANGUAGE_POLISH},
      {"vi", RETRO_LANGUAGE_VIETNAMESE},
      {"ar", RETRO_LANGUAGE_ARABIC},
      {"el", RETRO_LANGUAGE_GREEK},
   };

   if (string_is_empty(iso639))
      return lang;

   for (i = 0; i < ARRAY_SIZE(pairs); i++)
   {
      if (strcasestr(iso639, pairs[i].iso639))
      {
         lang = pairs[i].lang;
         break;
      }
   }

   return lang;
}

void rarch_favorites_init(void)
{
   struct rarch_state *p_rarch        = &rarch_st;
   settings_t *settings               = p_rarch->configuration_settings;
   int content_favorites_size         = settings ? settings->ints.content_favorites_size : 0;
   const char *path_content_favorites = settings ? settings->paths.path_content_favorites : NULL;
   bool playlist_sort_alphabetical    = settings ? settings->bools.playlist_sort_alphabetical : false;
   enum playlist_sort_mode current_sort_mode;

   if (!settings)
      return;

   if (content_favorites_size < 0)
      content_favorites_size = COLLECTION_SIZE;

   rarch_favorites_deinit();

   RARCH_LOG("%s: [%s].\n",
         msg_hash_to_str(MSG_LOADING_FAVORITES_FILE),
         path_content_favorites);
   g_defaults.content_favorites = playlist_init(
         path_content_favorites,
         (unsigned)content_favorites_size);

   /* Get current per-playlist sort mode */
   current_sort_mode = playlist_get_sort_mode(g_defaults.content_favorites);

   /* Ensure that playlist is sorted alphabetically,
    * if required */
   if ((playlist_sort_alphabetical && (current_sort_mode == PLAYLIST_SORT_MODE_DEFAULT)) ||
       (current_sort_mode == PLAYLIST_SORT_MODE_ALPHABETICAL))
      playlist_qsort(g_defaults.content_favorites);
}

void rarch_favorites_deinit(void)
{
   if (g_defaults.content_favorites)
   {
      struct rarch_state *p_rarch  = &rarch_st;
      settings_t         *settings = p_rarch->configuration_settings;
      bool playlist_use_old_format = settings->bools.playlist_use_old_format;
      bool playlist_compression    = settings->bools.playlist_compression;

      playlist_write_file(g_defaults.content_favorites,
            playlist_use_old_format, playlist_compression);
      playlist_free(g_defaults.content_favorites);
      g_defaults.content_favorites = NULL;
   }
}

/* Libretro core loader */

static void retro_run_null(void) { }
static void retro_frame_null(const void *data, unsigned width,
      unsigned height, size_t pitch) { }
static void retro_input_poll_null(void) { }

static int16_t core_input_state_poll_late(unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   struct rarch_state *p_rarch = &rarch_st;
   if (!p_rarch->current_core.input_polled)
      input_driver_poll();
   p_rarch->current_core.input_polled = true;
   
   return input_state(port, device, idx, id);
}

static retro_input_state_t core_input_state_poll_return_cb(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   const enum poll_type_override_t 
      core_poll_type_override  = p_rarch->core_poll_type_override;
   unsigned new_poll_type      = (core_poll_type_override > POLL_TYPE_OVERRIDE_DONTCARE)
      ? (core_poll_type_override - 1)
      : p_rarch->current_core.poll_type;
   if (new_poll_type == POLL_TYPE_LATE)
      return core_input_state_poll_late;
   return input_state;
}

static void core_input_state_poll_maybe(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   const enum poll_type_override_t 
      core_poll_type_override  = p_rarch->core_poll_type_override;
   unsigned new_poll_type      = (core_poll_type_override > POLL_TYPE_OVERRIDE_DONTCARE)
      ? (core_poll_type_override - 1)
      : p_rarch->current_core.poll_type;
   if (new_poll_type == POLL_TYPE_NORMAL)
      input_driver_poll();
}

/**
 * core_init_libretro_cbs:
 * @data           : pointer to retro_callbacks object
 *
 * Initializes libretro callbacks, and binds the libretro callbacks
 * to default callback functions.
 **/
static bool core_init_libretro_cbs(struct retro_callbacks *cbs)
{
   struct rarch_state *p_rarch  = &rarch_st;
   retro_input_state_t state_cb = core_input_state_poll_return_cb();

   p_rarch->current_core.retro_set_video_refresh(video_driver_frame);
   p_rarch->current_core.retro_set_audio_sample(audio_driver_sample);
   p_rarch->current_core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   p_rarch->current_core.retro_set_input_state(state_cb);
   p_rarch->current_core.retro_set_input_poll(core_input_state_poll_maybe);

   core_set_default_callbacks(cbs);

#ifdef HAVE_NETWORKING
   if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      return true;

   core_set_netplay_callbacks();
#endif

   return true;
}

/**
 * core_set_default_callbacks:
 * @data           : pointer to retro_callbacks object
 *
 * Binds the libretro callbacks to default callback functions.
 **/
bool core_set_default_callbacks(struct retro_callbacks *cbs)
{
   retro_input_state_t state_cb = core_input_state_poll_return_cb();

   cbs->frame_cb        = video_driver_frame;
   cbs->sample_cb       = audio_driver_sample;
   cbs->sample_batch_cb = audio_driver_sample_batch;
   cbs->state_cb        = state_cb;
   cbs->poll_cb         = input_driver_poll;

   return true;
}

/**
 * core_set_rewind_callbacks:
 *
 * Sets the audio sampling callbacks based on whether or not
 * rewinding is currently activated.
 **/
bool core_set_rewind_callbacks(void)
{
   struct rarch_state *p_rarch  = &rarch_st;

   if (state_manager_frame_is_reversed())
   {
      p_rarch->current_core.retro_set_audio_sample(audio_driver_sample_rewind);
      p_rarch->current_core.retro_set_audio_sample_batch(audio_driver_sample_batch_rewind);
   }
   else
   {
      p_rarch->current_core.retro_set_audio_sample(audio_driver_sample);
      p_rarch->current_core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   }
   return true;
}

#ifdef HAVE_NETWORKING
/**
 * core_set_netplay_callbacks:
 *
 * Set the I/O callbacks to use netplay's interceding callback system. Should
 * only be called while initializing netplay.
 **/
bool core_set_netplay_callbacks(void)
{
   struct rarch_state *p_rarch  = &rarch_st;

   /* Force normal poll type for netplay. */
   p_rarch->current_core.poll_type = POLL_TYPE_NORMAL;

   /* And use netplay's interceding callbacks */
   p_rarch->current_core.retro_set_video_refresh(video_frame_net);
   p_rarch->current_core.retro_set_audio_sample(audio_sample_net);
   p_rarch->current_core.retro_set_audio_sample_batch(audio_sample_batch_net);
   p_rarch->current_core.retro_set_input_state(input_state_net);

   return true;
}

/**
 * core_unset_netplay_callbacks
 *
 * Unset the I/O callbacks from having used netplay's interceding callback
 * system. Should only be called while uninitializing netplay.
 */
bool core_unset_netplay_callbacks(void)
{
   struct retro_callbacks cbs;
   struct rarch_state *p_rarch  = &rarch_st;

   if (!core_set_default_callbacks(&cbs))
      return false;

   p_rarch->current_core.retro_set_video_refresh(cbs.frame_cb);
   p_rarch->current_core.retro_set_audio_sample(cbs.sample_cb);
   p_rarch->current_core.retro_set_audio_sample_batch(cbs.sample_batch_cb);
   p_rarch->current_core.retro_set_input_state(cbs.state_cb);

   return true;
}
#endif

bool core_set_cheat(retro_ctx_cheat_info_t *info)
{
   struct rarch_state *p_rarch  = &rarch_st;
   p_rarch->current_core.retro_cheat_set(info->index, info->enabled, info->code);
   return true;
}

bool core_reset_cheat(void)
{
   struct rarch_state *p_rarch  = &rarch_st;
   p_rarch->current_core.retro_cheat_reset();
   return true;
}

bool core_set_poll_type(unsigned type)
{
   struct rarch_state *p_rarch  = &rarch_st;
   p_rarch->current_core.poll_type = type;
   return true;
}

bool core_set_controller_port_device(retro_ctx_controller_info_t *pad)
{
   struct rarch_state *p_rarch  = &rarch_st;
   if (!pad)
      return false;

#ifdef HAVE_RUNAHEAD
   remember_controller_port_device(pad->port, pad->device);
#endif

   p_rarch->current_core.retro_set_controller_port_device(pad->port, pad->device);
   return true;
}

bool core_get_memory(retro_ctx_memory_info_t *info)
{
   struct rarch_state *p_rarch  = &rarch_st;
   if (!info)
      return false;
   info->size  = p_rarch->current_core.retro_get_memory_size(info->id);
   info->data  = p_rarch->current_core.retro_get_memory_data(info->id);
   return true;
}

bool core_load_game(retro_ctx_load_content_info_t *load_info)
{
   bool             contentless = false;
   bool             is_inited   = false;
   bool             game_loaded = false;
   struct rarch_state *p_rarch  = &rarch_st;

   video_driver_set_cached_frame_ptr(NULL);

#ifdef HAVE_RUNAHEAD
   set_load_content_info(load_info);
   clear_controller_port_map();
#endif

   content_get_status(&contentless, &is_inited);
   set_save_state_in_background(false);

   if (load_info && load_info->special)
      game_loaded = p_rarch->current_core.retro_load_game_special(
            load_info->special->id, load_info->info, load_info->content->size);
   else if (load_info && !string_is_empty(load_info->content->elems[0].data))
      game_loaded = p_rarch->current_core.retro_load_game(load_info->info);
   else if (contentless)
      game_loaded = p_rarch->current_core.retro_load_game(NULL);

   p_rarch->current_core.game_loaded = game_loaded;

   return game_loaded;
}

bool core_get_system_info(struct retro_system_info *system)
{
   struct rarch_state *p_rarch  = &rarch_st;
   if (!system)
      return false;
   p_rarch->current_core.retro_get_system_info(system);
   return true;
}

bool core_unserialize(retro_ctx_serialize_info_t *info)
{
   struct rarch_state *p_rarch  = &rarch_st;
   if (!info || !p_rarch->current_core.retro_unserialize(info->data_const, info->size))
      return false;

#if HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, info);
#endif

   return true;
}

bool core_serialize(retro_ctx_serialize_info_t *info)
{
   struct rarch_state *p_rarch  = &rarch_st;
   if (!info || !p_rarch->current_core.retro_serialize(info->data, info->size))
      return false;
   return true;
}

bool core_serialize_size(retro_ctx_size_info_t *info)
{
   struct rarch_state *p_rarch  = &rarch_st;
   if (!info)
      return false;
   info->size = p_rarch->current_core.retro_serialize_size();
   return true;
}

uint64_t core_serialization_quirks(void)
{
   struct rarch_state *p_rarch  = &rarch_st;
   return p_rarch->current_core.serialization_quirks_v;
}

bool core_reset(void)
{
   struct rarch_state *p_rarch  = &rarch_st;

   video_driver_set_cached_frame_ptr(NULL);

   p_rarch->current_core.retro_reset();
   return true;
}

static bool core_unload_game(void)
{
   struct rarch_state *p_rarch = &rarch_st;

   video_driver_free_hw_context();

   video_driver_set_cached_frame_ptr(NULL);

   if (p_rarch->current_core.game_loaded)
   {
      p_rarch->current_core.retro_unload_game();
      p_rarch->core_poll_type_override  = POLL_TYPE_OVERRIDE_DONTCARE;
      p_rarch->current_core.game_loaded = false;
   }

   audio_driver_stop();

   return true;
}

bool core_run(void)
{
   struct rarch_state 
      *p_rarch                 = &rarch_st;
   struct retro_core_t *
      current_core             = &p_rarch->current_core;
   const enum poll_type_override_t 
      core_poll_type_override  = p_rarch->core_poll_type_override;
   unsigned new_poll_type      = (core_poll_type_override != POLL_TYPE_OVERRIDE_DONTCARE)
      ? (core_poll_type_override - 1)
      : current_core->poll_type;
   bool early_polling          = new_poll_type == POLL_TYPE_EARLY;
   bool late_polling           = new_poll_type == POLL_TYPE_LATE;
#ifdef HAVE_NETWORKING
   bool netplay_preframe       = netplay_driver_ctl(
         RARCH_NETPLAY_CTL_PRE_FRAME, NULL);

   if (!netplay_preframe)
   {
      /* Paused due to netplay. We must poll and display something so that a
       * netplay peer pausing doesn't just hang. */
      input_driver_poll();
      video_driver_cached_frame();
      return true;
   }
#endif

   if (early_polling)
      input_driver_poll();
   else if (late_polling)
      current_core->input_polled = false;

   current_core->retro_run();

   if (late_polling && !current_core->input_polled)
      input_driver_poll();

#ifdef HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_POST_FRAME, NULL);
#endif

   return true;
}

static bool core_verify_api_version(void)
{
   struct rarch_state 
      *p_rarch                 = &rarch_st;
   unsigned api_version        = p_rarch->current_core.retro_api_version();

   RARCH_LOG("%s: %u\n%s %s: %u\n",
         msg_hash_to_str(MSG_VERSION_OF_LIBRETRO_API),
         api_version,
         FILE_PATH_LOG_INFO,
         msg_hash_to_str(MSG_COMPILED_AGAINST_API),
         RETRO_API_VERSION
         );

   if (api_version != RETRO_API_VERSION)
   {
      RARCH_WARN("%s\n", msg_hash_to_str(MSG_LIBRETRO_ABI_BREAK));
      return false;
   }
   return true;
}

static bool core_load(unsigned poll_type_behavior)
{
   struct rarch_state *p_rarch = &rarch_st;

   p_rarch->current_core.poll_type      = poll_type_behavior;

   if (!core_verify_api_version())
      return false;
   if (!core_init_libretro_cbs(&p_rarch->retro_ctx))
      return false;

   p_rarch->current_core.retro_get_system_av_info(&p_rarch->video_driver_av_info);

   return true;
}

bool core_has_set_input_descriptor(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->current_core.has_set_input_descriptors;
}

#if defined(HAVE_RUNAHEAD)
static void core_free_retro_game_info(struct retro_game_info *dest)
{
   if (!dest)
      return;
   if (dest->path)
      free((void*)dest->path);
   if (dest->data)
      free((void*)dest->data);
   if (dest->meta)
      free((void*)dest->meta);
   dest->path = NULL;
   dest->data = NULL;
   dest->meta = NULL;
}
#endif

unsigned int retroarch_get_rotation(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t     *settings    = p_rarch->configuration_settings;
   unsigned     video_rotation = settings->uints.video_rotation;
   return video_rotation + p_rarch->runloop_system.rotation;
}

bool is_input_keyboard_display_on(void)
{
#ifdef HAVE_MENU
   return menu_input_dialog_get_display_kb();
#else
   return false;
#endif
}

#ifdef HAVE_ACCESSIBILITY
bool accessibility_speak_priority(const char* speak_text, int priority)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = p_rarch->configuration_settings;

   RARCH_LOG("Spoke: %s\n", speak_text);

   if (is_accessibility_enabled())
   {
      frontend_ctx_driver_t *frontend = frontend_get_ptr();
      if (frontend && frontend->accessibility_speak)
      {
         int speed = settings->uints.accessibility_narrator_speech_speed;
         return frontend->accessibility_speak(speed, speak_text,
               priority);
      }

      RARCH_LOG("Platform not supported for accessibility.\n");
      /* The following method is a fallback for other platforms to use the
         AI Service url to do the TTS.  However, since the playback is done
         via the audio mixer, which only processes the audio while the
         core is running, this playback method won't work.  When the audio
         mixer can handle playing streams while the core is paused, then
         we can use this. */
#if 0
#if defined(HAVE_NETWORKING)
      return accessibility_speak_ai_service(speak_text, voice, priority);
#endif
#endif
   }

   return true;
}

#ifdef HAVE_TRANSLATE
static bool is_narrator_running(void)
{
   if (is_accessibility_enabled())
   {
      frontend_ctx_driver_t *frontend = frontend_get_ptr();
      if (frontend && frontend->is_narrator_running)
         return frontend->is_narrator_running();
   }
   return true;
}
#endif

static bool accessibility_startup_message(void)
{
   /* State that the narrator is on, and also include the first menu
      item we're on at startup. */
   accessibility_speak_priority(
         "RetroArch accessibility on.  Main Menu Load Core.",
         10);
   return true;
}

unsigned get_gamepad_input_override(void)
{
   struct rarch_state *p_rarch        = &rarch_st;
   return p_rarch->gamepad_input_override;
}

void set_gamepad_input_override(unsigned i, bool val)
{
   struct rarch_state *p_rarch        = &rarch_st;

   if (val)
      p_rarch->gamepad_input_override = p_rarch->gamepad_input_override | (1 << i);
   else
      p_rarch->gamepad_input_override = p_rarch->gamepad_input_override & ((1 << i) ^ (~0));
}

void reset_gamepad_input_override(void)
{
   struct rarch_state *p_rarch     = &rarch_st;
   p_rarch->gamepad_input_override = 0;
}
#endif

/* creates folder and core options stub file for subsequent runs */
bool create_folder_and_core_options(void)
{
   char game_path[PATH_MAX_LENGTH];
   config_file_t *conf             = NULL;

   game_path[0] = '\0';

   if (!retroarch_validate_game_options(game_path, sizeof(game_path), true))
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_ERROR_SAVING_CORE_OPTIONS_FILE),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      return false;
   }

   if (!(conf = config_file_new_from_path_to_string(game_path)))
      if (!(conf = config_file_new_alloc()))
         return false;

   if (config_file_write(conf, game_path, true))
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      path_set(RARCH_PATH_CORE_OPTIONS, game_path);
   }
   config_file_free(conf);

   return true;
}

void menu_content_environment_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   struct rarch_state       *p_rarch = &rarch_st;
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)params_data;
   rarch_system_info_t *sys_info     = &p_rarch->runloop_system;

   if (!wrap_args)
      return;

   wrap_args->no_content             = sys_info->load_no_content;

   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_VERBOSITY, NULL))
      wrap_args->verbose       = verbosity_is_enabled();

   wrap_args->touched          = true;
   wrap_args->config_path      = NULL;
   wrap_args->sram_path        = NULL;
   wrap_args->state_path       = NULL;
   wrap_args->content_path     = NULL;

   if (!path_is_empty(RARCH_PATH_CONFIG))
      wrap_args->config_path   = path_get(RARCH_PATH_CONFIG);
   if (!string_is_empty(p_rarch->dir_savefile))
      wrap_args->sram_path     = p_rarch->dir_savefile;
   if (!string_is_empty(p_rarch->dir_savestate))
      wrap_args->state_path    = p_rarch->dir_savestate;
   if (!path_is_empty(RARCH_PATH_CONTENT))
      wrap_args->content_path  = path_get(RARCH_PATH_CONTENT);
   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL))
      wrap_args->libretro_path = string_is_empty(path_get(RARCH_PATH_CORE)) ? NULL :
         path_get(RARCH_PATH_CORE);
}
