#ifndef PARALLEL_H__
#define PARALLEL_H__

#include <volk.h>
#include <libretro.h>
#include <libretro_vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

bool parallel_init(const struct retro_hw_render_interface_vulkan *vulkan);
void parallel_deinit(void);
bool parallel_frame_is_valid(void);
unsigned parallel_frame_width(void);
unsigned parallel_frame_height(void);
void parallel_begin_frame(void);
void parallel_set_synchronous_rdp(bool enable);
/* Deferred sync (with synchronous RDP): at a full sync the CPU goes on and the core raises the
 * DP interrupt `parallel_deferred_delay` count cycles later, waiting for the GPU then; at the
 * end of every frame and around savestates the GPU is waited for too. Deterministic (the same
 * cycles everywhere), unlike asynchronous RDP, with the GPU's work overlapping the CPU's. */
void parallel_set_deferred_sync(bool enable);
int parallel_take_deferred_interrupt(void);
void parallel_wait_deferred_sync(void);

void parallel_set_divot_filter(bool enable);
void parallel_set_gamma_dither(bool enable);
void parallel_set_vi_aa(bool enable);
void parallel_set_vi_scale(bool enable);
void parallel_set_dither_filter(bool enable);
void parallel_set_interlacing(bool enable);

void parallel_set_upscaling(unsigned factor);
void parallel_set_downscaling_steps(unsigned steps);
void parallel_set_native_texture_lod(bool enable);
void parallel_set_native_tex_rect(bool enable);

unsigned parallel_get_upscaling();

void parallel_set_overscan_crop(unsigned pixels);

void parallel_profile_video_refresh_begin(void);
void parallel_profile_video_refresh_end(void);

const VkApplicationInfo *parallel_get_application_info(void);
bool parallel_create_device(struct retro_vulkan_context *context,
      VkInstance instance,
      VkPhysicalDevice gpu,
      VkSurfaceKHR surface,
      PFN_vkGetInstanceProcAddr get_instance_proc_addr,
      const char **required_device_extensions,
      unsigned num_required_device_extensions,
      const char **required_device_layers,
      unsigned num_required_device_layers,
      const VkPhysicalDeviceFeatures *required_features);

#ifdef __cplusplus
}
#endif

#endif
