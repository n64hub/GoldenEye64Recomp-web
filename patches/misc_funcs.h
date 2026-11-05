#ifndef __RECOMP_FUNCS_H__
#define __RECOMP_FUNCS_H__

#include "patch_helpers.h"

DECLARE_FUNC(void, recomp_yield_self_1ms, void);
DECLARE_FUNC(void, recomp_load_overlays, u32 rom, void* ram, u32 size);
DECLARE_FUNC(void, recomp_puts, const char* data, u32 size);
DECLARE_FUNC(void, recomp_exit);
DECLARE_FUNC(void, recomp_handle_quicksave_actions, OSMesgQueue* enter_mq, OSMesgQueue* exit_mq);
DECLARE_FUNC(void, recomp_handle_quicksave_actions_main, OSMesgQueue* enter_mq, OSMesgQueue* exit_mq);
DECLARE_FUNC(u16, recomp_get_pending_warp);
DECLARE_FUNC(u32, recomp_get_pending_set_time);
DECLARE_FUNC(s32, recomp_autosave_enabled);
DECLARE_FUNC(void, recomp_load_overlays, u32 rom, void* ram, u32 size);
DECLARE_FUNC(s32, osPiStartDma_recomp, OSIoMesg* mb, s32 priority, s32 direction, uintptr_t devAddr, void* vAddr,
             size_t nbytes, OSMesgQueue* mq);
DECLARE_FUNC(s32, osPfsInit_recomp, OSMesgQueue*, OSPfs*, int);
DECLARE_FUNC(s32, osMotorInit_recomp, OSMesgQueue*, OSPfs*, int);
DECLARE_FUNC(s32, recomp_get_harness_stage);
DECLARE_FUNC(s32, recomp_get_harness_look);
DECLARE_FUNC(s32, recomp_get_harness_skip);
DECLARE_FUNC(s32, recomp_get_harness_dbg);
DECLARE_FUNC(s32, recomp_get_debug_menu_enabled);
DECLARE_FUNC(s32, recomp_get_debug_log_enabled);
DECLARE_FUNC(void, recomp_get_mouse_deltas, f32* x, f32* y);
DECLARE_FUNC(u32, recomp_get_mouse_actions);
DECLARE_FUNC(s32, recomp_get_mouse_invert_y);
DECLARE_FUNC(s32, recomp_mouse_playing);
DECLARE_FUNC(s32, recomp_mouse_aim_held);
DECLARE_FUNC(void, recomp_mouse_menu_shown);
DECLARE_FUNC(void, recomp_mouse_driving_tank, s32 driving);
DECLARE_FUNC(void, recomp_ext_playing, s32 playing);
#endif
