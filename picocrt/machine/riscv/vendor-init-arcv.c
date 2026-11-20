/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2026 MIPS Holding, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdbool.h>

#define CSR_NUM_MSTATUS                  0x300
#define MSTATUS_VS_OFFSET                  0x9
#define MSTATUS_VS_MASK                    0x3

#define CSR_NUM_ARCV_MCACHE_CTRL         0x7C8
#define ARCV_MCACHE_CTRL_IC_EN_OFFSET      0x0
#define ARCV_MCACHE_CTRL_DC_EN_OFFSET      0x8
#define ARCV_MCACHE_CTRL_DC_L0_EN_OFFSET   0xC
#define ARCV_MCACHE_CTRL_L2_EN_OFFSET     0x10

#define CSR_NUM_ARCV_RVV_BUILD           0xFC4
#define ARCV_RVV_BUILD_V_OPTION_OFFSET     0x0
#define ARCV_RVV_BUILD_V_OPTION_MASK       0x7

#define CSR_NUM_MARCHID                  0xF12
#define ARCV_MARCHID_RMX700_MASK         0x3FF
#define ARCV_MARCHID_RMX700_VALUE        0x105
#define ARCV_MARCHID_RHX100_MASK         0x0FF
#define ARCV_MARCHID_RHX100_VALUE        0x002
#define ARCV_MARCHID_RPX100_MASK         0x0FF
#define ARCV_MARCHID_RPX100_VALUE        0x003

/*
 * Cores which support arcv_mexec_ctrl register:
 *
 *     RMX-700 (fused)
 *     RHX-100 (fused instruction, dual issue)
 *     RPX-100 (fused instruction, dual issue)
 */
#define CSR_NUM_ARCV_MEXEC_CTRL          0x7C5
#define ARCV_MEXEC_DUAL_ISSUE_OFFSET       0x0
#define ARCV_MEXEC_FUSION_OFFSET           0x2
#define ARCV_MEXEC_FUSION_CTRL_OFFSET      0x8
#define ARCV_MEXEC_FUSION_CTRL_MASK     0x3FFF

extern void __picocrt_vendor_init(void);

#define _arcv_csr_read(csr)                       \
    ({                                            \
        unsigned long __arcv_csr_r;               \
        __asm__ __volatile__("csrr %0, %1"        \
                             : "=r"(__arcv_csr_r) \
                             : "i"(csr));         \
        __arcv_csr_r;                             \
    })

#define _arcv_csr_write(csr, val) \
    __asm__ __volatile__("csrw %0, %1" ::"i"(csr), "r"(val))

static inline void __attribute__((always_inline, target("arch=+zicsr")))
_arcv_cache_enable(void)
{
    unsigned long mcache = _arcv_csr_read(CSR_NUM_ARCV_MCACHE_CTRL);
    mcache |= (1 << ARCV_MCACHE_CTRL_IC_EN_OFFSET) | (1 << ARCV_MCACHE_CTRL_DC_EN_OFFSET)
        | (1 << ARCV_MCACHE_CTRL_DC_L0_EN_OFFSET) | (1 << ARCV_MCACHE_CTRL_L2_EN_OFFSET);
    _arcv_csr_write(CSR_NUM_ARCV_MCACHE_CTRL, mcache);
}

/*
 * misa.V is set only when the full V extension is present. If a target
 * implements only a subset of V, this bit is not set and it cannot be
 * used to decide whether mstatus.VS must be set. Use the ARC-V build
 * register for RVV features instead: if its first 3 bits are not 0,
 * then vector extensions are present.
 */
static inline void __attribute__((always_inline, target("arch=+zicsr")))
_arcv_vector_enable(void)
{
    unsigned long rvv_build = _arcv_csr_read(CSR_NUM_ARCV_RVV_BUILD);
    unsigned long v_option
        = (rvv_build >> ARCV_RVV_BUILD_V_OPTION_OFFSET) & ARCV_RVV_BUILD_V_OPTION_MASK;

    if (v_option != 0) {
        unsigned long mstatus = _arcv_csr_read(CSR_NUM_MSTATUS);
        mstatus |= (1 << MSTATUS_VS_OFFSET);
        _arcv_csr_write(CSR_NUM_MSTATUS, mstatus);
    }
}

static inline bool __attribute__((always_inline, target("arch=+zicsr")))
_arcv_marchid_is_rmx700(unsigned long marchid)
{
    return (marchid & ARCV_MARCHID_RMX700_MASK) == ARCV_MARCHID_RMX700_VALUE;
}

static inline bool __attribute__((always_inline, target("arch=+zicsr")))
_arcv_marchid_is_rhx100(unsigned long marchid)
{
    return (marchid & ARCV_MARCHID_RHX100_MASK) == ARCV_MARCHID_RHX100_VALUE;
}

static inline bool __attribute__((always_inline, target("arch=+zicsr")))
_arcv_marchid_is_rpx100(unsigned long marchid)
{
    return (marchid & ARCV_MARCHID_RPX100_MASK) == ARCV_MARCHID_RPX100_VALUE;
}

static inline bool __attribute__((always_inline, target("arch=+zicsr")))
_arcv_marchid_supports_fusion(void)
{
    unsigned long marchid = _arcv_csr_read(CSR_NUM_MARCHID);

    return _arcv_marchid_is_rmx700(marchid)
        || _arcv_marchid_is_rhx100(marchid)
        || _arcv_marchid_is_rpx100(marchid);
}

static inline bool __attribute__((always_inline, target("arch=+zicsr")))
_arcv_marchid_supports_dual_issue(void)
{
    unsigned long marchid = _arcv_csr_read(CSR_NUM_MARCHID);

    return _arcv_marchid_is_rhx100(marchid)
        || _arcv_marchid_is_rpx100(marchid);
}

/*
 * To enable dual issue and instruction fusion features, respective
 * bits in arcv_mexec_ctrl CSR must be zeroed.
 */

static inline void __attribute__((always_inline, target("arch=+zicsr")))
_arcv_dual_issue_enable()
{
    unsigned long mexec;

    if (!_arcv_marchid_supports_dual_issue())
        return;

    mexec = _arcv_csr_read(CSR_NUM_ARCV_MEXEC_CTRL);
    mexec = ~mexec;
    mexec |= 1 << ARCV_MEXEC_DUAL_ISSUE_OFFSET;
    mexec = ~mexec;

    _arcv_csr_write(CSR_NUM_ARCV_MEXEC_CTRL, mexec);
}

static inline void __attribute__((always_inline, target("arch=+zicsr")))
_arcv_fusion_enable(void)
{
    unsigned long mexec;

    if (!_arcv_marchid_supports_fusion())
        return;

    mexec = _arcv_csr_read(CSR_NUM_ARCV_MEXEC_CTRL);
    mexec = ~mexec;
    mexec |= 1 << ARCV_MEXEC_FUSION_OFFSET;
    mexec |= ARCV_MEXEC_FUSION_CTRL_MASK << ARCV_MEXEC_FUSION_CTRL_OFFSET;
    mexec = ~mexec;

    _arcv_csr_write(CSR_NUM_ARCV_MEXEC_CTRL, mexec);
}

void __attribute__((target("arch=+zicsr")))
__picocrt_vendor_init(void)
{
    _arcv_cache_enable();
    _arcv_vector_enable();
    _arcv_dual_issue_enable();
    _arcv_fusion_enable();
}
