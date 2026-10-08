/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - rdp_core.c                                              *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2014 Bobby Smiles                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "rdp_core.h"

#include <string.h>

#include "device/memory/m64p_memory.h"
#include "device/rcp/mi/mi_controller.h"
#include "device/rcp/rsp/rsp_core.h"
#include "plugin/plugin.h"
#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "device/r4300/cp0.h"
#include "device/r4300/interrupt.h"
#include "device/r4300/r4300_core.h"

#if defined(HAVE_PARALLEL)
/* ParaLLEl-RDP's deferred sync (mupen64plus-video-paraLLEl/parallel.h). */
extern int parallel_take_deferred_interrupt(void);
extern void parallel_wait_deferred_sync(void);
#endif

static struct rdp_core* rsp_dp;  /* the RDP the RSP submits to (init_rdp) */

static void update_dpc_status(struct rdp_core* dp, uint32_t w)
{
    /* clear / set xbus_dmem_dma */
    if (w & DPC_CLR_XBUS_DMEM_DMA) dp->dpc_regs[DPC_STATUS_REG] &= ~DPC_STATUS_XBUS_DMEM_DMA;
    if (w & DPC_SET_XBUS_DMEM_DMA) dp->dpc_regs[DPC_STATUS_REG] |= DPC_STATUS_XBUS_DMEM_DMA;

    /* clear / set freeze */
    if (w & DPC_CLR_FREEZE)
    {
        dp->dpc_regs[DPC_STATUS_REG] &= ~DPC_STATUS_FREEZE;

        if (dp->do_on_unfreeze & DELAY_DP_INT)
        {
#if defined(HAVE_PARALLEL)
            parallel_wait_deferred_sync();
#endif
            signal_rcp_interrupt(dp->mi, MI_INTR_DP);
        }
        if (dp->do_on_unfreeze & DELAY_UPDATESCREEN)
            gfx.updateScreen();
        dp->do_on_unfreeze = 0;
    }
    if (w & DPC_SET_FREEZE) dp->dpc_regs[DPC_STATUS_REG] |= DPC_STATUS_FREEZE;

    /* clear / set flush */
    if (w & DPC_CLR_FLUSH) dp->dpc_regs[DPC_STATUS_REG] &= ~DPC_STATUS_FLUSH;
    if (w & DPC_SET_FLUSH) dp->dpc_regs[DPC_STATUS_REG] |= DPC_STATUS_FLUSH;

    /* clear clock counter */
    if (w & DPC_CLR_CLOCK_CTR) dp->dpc_regs[DPC_CLOCK_REG] = 0;
}


void init_rdp(struct rdp_core* dp,
              struct rsp_core* sp,
              struct mi_controller* mi,
              struct memory* mem,
              struct rdram* rdram,
              struct r4300_core* r4300)
{
    dp->sp = sp;
    dp->mi = mi;
    rsp_dp = dp;

    init_fb(&dp->fb, mem, rdram, r4300);
}

void poweron_rdp(struct rdp_core* dp)
{
    memset(dp->dpc_regs, 0, DPC_REGS_COUNT*sizeof(uint32_t));
    memset(dp->dps_regs, 0, DPS_REGS_COUNT*sizeof(uint32_t));
    dp->dpc_regs[DPC_STATUS_REG] |= DPC_STATUS_START_GCLK;

    dp->do_on_unfreeze = 0;

    poweron_fb(&dp->fb);
}


void read_dpc_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct rdp_core* dp = (struct rdp_core*)opaque;
    uint32_t reg = dpc_reg(address);

    *value = dp->dpc_regs[reg];
}

void write_dpc_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct rdp_core* dp = (struct rdp_core*)opaque;
    uint32_t reg = dpc_reg(address);

    switch(reg)
    {
    case DPC_STATUS_REG:
        update_dpc_status(dp, value & mask);
    case DPC_CURRENT_REG:
    case DPC_CLOCK_REG:
    case DPC_BUFBUSY_REG:
    case DPC_PIPEBUSY_REG:
    case DPC_TMEM_REG:
        return;
    }

    masked_write(&dp->dpc_regs[reg], value, mask);

    switch(reg)
    {
    case DPC_START_REG:
        dp->dpc_regs[DPC_CURRENT_REG] = dp->dpc_regs[DPC_START_REG];
        break;
    case DPC_END_REG:
        unprotect_framebuffers(&dp->fb);
        gfx.processRDPList();
        protect_framebuffers(&dp->fb);
#if defined(HAVE_PARALLEL)
        /* A full sync whose GPU work runs on: its DP interrupt at the next frame
         * (rdp_pull_deferred_interrupt). */
        if (parallel_take_deferred_interrupt())
        {
            cp0_update_count(dp->mi->r4300);
            add_interrupt_event(&dp->mi->r4300->cp0, DP_INT, RDP_DEFERRED_DP_DELAY);
            break;
        }
#endif
        signal_rcp_interrupt(dp->mi, MI_INTR_DP);
        break;
    }
}


void read_dps_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct rdp_core* dp = (struct rdp_core*)opaque;
    uint32_t reg = dps_reg(address);

    *value = dp->dps_regs[reg];
}

void write_dps_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct rdp_core* dp = (struct rdp_core*)opaque;
    uint32_t reg = dps_reg(address);

    masked_write(&dp->dps_regs[reg], value, mask);
}

void rdp_process_list_from_rsp(void)
{
    gfx.processRDPList();
#if defined(HAVE_PARALLEL)
    if (rsp_dp && parallel_take_deferred_interrupt())
    {
        cp0_update_count(rsp_dp->mi->r4300);
        add_interrupt_event(&rsp_dp->mi->r4300->cp0, DP_INT, RDP_DEFERRED_DP_DELAY);
    }
#endif
}

void rdp_pull_deferred_interrupt(void)
{
    struct cp0* cp0;
    const unsigned int* at;
    unsigned int ahead;

    if (!rsp_dp)
        return;
    cp0 = &rsp_dp->mi->r4300->cp0;
    at = get_event(&cp0->q, DP_INT);
    if (!at)
        return;
    /* Only a deferred one is this far ahead (others are a few thousand cycles). */
    ahead = *at - r4300_cp0_regs(cp0)[CP0_COUNT_REG];
    if (ahead < RDP_DEFERRED_DP_DELAY / 2 || ahead > RDP_DEFERRED_DP_DELAY)
        return;
    remove_event(&cp0->q, DP_INT);
    add_interrupt_event(cp0, DP_INT, 4000);
}

void rdp_interrupt_event(void* opaque)
{
    struct rdp_core* dp = (struct rdp_core*)opaque;

#if defined(HAVE_PARALLEL)
    parallel_wait_deferred_sync();
#endif
    raise_rcp_interrupt(dp->mi, MI_INTR_DP);
}

