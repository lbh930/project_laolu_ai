// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: LicenseRef-NvidiaProprietary
//
// NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
// property and proprietary rights in and to this material, related
// documentation and any modifications thereto. Any use, reproduction,
// disclosure or distribution of this material and related documentation
// without an express license agreement from NVIDIA CORPORATION or
// its affiliates is strictly prohibited.

#pragma once

#include "nvaim_struct.h"

namespace nvaim
{

// {6E145BB2-8B36-4467-B745-255EEFD8D823}
struct alignas(8) CPUParameters {
    CPUParameters() {}; 
    NVAIM_UID(UID({ 0x6e145bb2, 0x8b36, 0x4467,{ 0xb7, 0x45, 0x25, 0x5e, 0xef, 0xd8, 0xd8, 0x23 } }), kStructVersion1);
    bool arm_fma{};
    bool avx{};
    bool avx_vnni{};
    bool avx2{};
    bool avx512{};
    bool avx512_vbmi{};
    bool avx512_vnni{};
    bool fma{};
    bool neon{};
    bool f16c{};
    bool fp16_va{};
    bool wasm_simd{};
    bool sse3{};
    bool vsx{};
};

NVAIM_VALIDATE_STRUCT(CPUParameters);

//! Interface CpuData
//!
//! {A8197FE3-FC9B-4730-BC85-CB9F755C111C}
struct alignas(8) CpuData {
    CpuData() {};
    NVAIM_UID(UID({ 0xa8197fe3, 0xfc9b, 0x4730,{ 0xbc, 0x85, 0xcb, 0x9f, 0x75, 0x5c, 0x11, 0x1c } }), kStructVersion1);
    CpuData(size_t _count, const void* _buffer) : sizeInBytes(_count) { buffer = _buffer; };
    //! Data buffer
    const void* buffer{};
    //! Number of bytes in the buffer
    size_t sizeInBytes{};

    //! NEW MEMBERS GO HERE, BUMP THE VERSION!
};

NVAIM_VALIDATE_STRUCT(CpuData)

}
