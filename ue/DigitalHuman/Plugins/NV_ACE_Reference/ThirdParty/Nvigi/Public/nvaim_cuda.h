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

typedef int CUdevice_v1;                                     /**< CUDA device */
typedef CUdevice_v1 CUdevice;                                /**< CUDA device */
typedef struct CUctx_st* CUcontext;                          /**< CUDA context */
typedef struct CUstream_st* CUstream;                        /**< CUDA stream */

namespace nvaim
{

//! Temporary solution, do we really need to ship all this?
//! 
enum CudaImplementation
{
    eTRT,
    eGGML,
    eNative
};

struct alignas(8) CudaParameters {
    CudaParameters() {}; 
    NVAIM_UID(UID({ 0xfab2bd3f, 0x8a3e, 0x41ab,{ 0x88, 0xde, 0xd6, 0xcb, 0x2b, 0x65, 0xc5, 0x54 } }), kStructVersion1);
    CudaImplementation impl{}; // remove at some point
    CUdevice device{};
    CUcontext context{};
    CUstream stream{};
};

NVAIM_VALIDATE_STRUCT(CudaParameters);

//! Interface CudaData
//!
//! {DEE43A64-2622-492E-8737-9AAD6BE1D634}
struct alignas(8) CudaData {
    CudaData() {}; 
    NVAIM_UID(UID({ 0xdee43a64, 0x2622, 0x492e,{ 0x87, 0x37, 0x9a, 0xad, 0x6b, 0xe1, 0xd6, 0x34 } }), kStructVersion1);
    //! Data buffer
    const void* buffer{};
    //! Number of bytes in the buffer
    size_t sizeInBytes{};
    //! NEW MEMBERS GO HERE, REMEMBER TO BUMP THE VERSION!
};

NVAIM_VALIDATE_STRUCT(CudaData)


}
