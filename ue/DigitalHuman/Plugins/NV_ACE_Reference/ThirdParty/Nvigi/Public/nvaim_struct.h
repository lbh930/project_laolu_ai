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

#include <stdint.h>
#include <type_traits>

namespace nvaim
{

//! GUID
struct alignas(8) UID
{
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t  data4[8];

    inline bool operator==(const UID& rhs) const 
    { 
        for (int i = 0; i < 4; i++) if (data4[i] != rhs.data4[i]) return false;
        return data1 == rhs.data1 && data2 == rhs.data2 && data3 == rhs.data3; 
    }
    inline bool operator!=(const UID& rhs) const { return !operator==(rhs); }
};

static_assert(std::is_standard_layout<UID>::value, "NVAIM structure must have standard layout");
static_assert(std::alignment_of<UID>::value == 8, "NVAIM structure must have alignment of 8");

//! Plugin ID
struct alignas(8) PluginID
{
    UID id;
    uint32_t crc24;  // 24bit id - can be used as the magic value to generate unique results, see nvaim_result.h for details

    inline operator UID() const { return id; };
    inline operator uint32_t() const { return crc24; };
    inline bool operator<(const PluginID& rhs) const { return crc24 < rhs.crc24; }
    inline bool operator==(const PluginID& rhs) const { return !operator!=(rhs); }
    inline bool operator!=(const PluginID& rhs) const { return id.operator!=(rhs.id) && crc24 != rhs.crc24; }
};

static_assert(std::is_standard_layout<PluginID>::value, "NVAIM structure must have standard layout");
static_assert(std::alignment_of<PluginID>::value == 8, "NVAIM structure must have alignment of 8");

//! NVAIM is using typed and versioned structures, always aligned to 8 bytes, which can be chained together if needed.
//! 
//! --- OPTION 1 ---
//! 
//! New members must be added at the end and version needs to be increased:
//! 
//! struct alignas(8) S1 
//! {
//!     NVAIM_UID(GUID1, kStructVersion1);
//!     A
//!     B
//!     C
//! }
//! 
//! struct alignas(8) S1 
//! {
//!     NVAIM_UID(GUID1, kStructVersion2); // Note that version was bumped
//!
//!     // V1
//!     A
//!     B
//!     C
//! 
//!     //! V2 - new members always go at the end!
//!     D
//!     E
//! }
//! 
//! Here is one example on how to check for version and handle backwards compatibility:
//! 
//! void func(const S1* input)
//! {
//!     // Access A, B, C as needed
//!     ...
//!     if (input->structVersion >= kStructVersion2)
//!     {
//!         // Safe to access D, E
//!     }
//! }
//!  
//! --- OPTION 2 ---
//! 
//! New members are optional, represent new sub-feature or logical unit in which case they are added to a new struct which is then chained as needed:
//! 
//! struct alignas(8) S1 
//! {
//!     NVAIM_UID(GUID1, kStructVersion1);
//!     A
//!     B
//!     C
//! }
//! 
//! struct alignas(8) S2 
//! {
//!     NVAIM_UID(GUID2, kStructVersion1); // New GUID, v1
//!     D
//!     E
//! }
//! 
//! S1 s1;
//! S2 s2
//! s1.chain(s2); // optional parameters in S2

//! IMPORTANT: 
//! 
//! * New members in the structure always go at the end!
//! * Never add NVAIM structure as an embedded member of another NVAIM structure - this breaks ABI compatiblity! Use pointers or chaining instead.

//! Structure versions
constexpr uint32_t kStructVersion1 = 1;
constexpr uint32_t kStructVersion2 = 2;
constexpr uint32_t kStructVersion3 = 3;
constexpr uint32_t kStructVersion4 = 4;
constexpr uint32_t kStructVersion5 = 5;
constexpr uint32_t kStructVersion6 = 6;
constexpr uint32_t kStructVersion7 = 7;
constexpr uint32_t kStructVersion8 = 8;

struct alignas(8) BaseStructure
{
    void* next{};
    UID type{};
    uint32_t version;
};

//! Same underlying logic but separating by use case for easier code reading
using NVAIMInterface = BaseStructure;
using NVAIMParameter = BaseStructure;

#define NVAIM_UID(guid, v)                                                                                             \
    BaseStructure _base = {nullptr, guid, v};                                                                         \
    uint32_t getVersion() { return _base.version;}                                                                    \
    UID getType() { return _base.type;}                                                                               \
    inline operator BaseStructure* () { return &_base;}                                                               \
    inline operator const BaseStructure* () const { return &_base;}                                                   \
    inline void chain(BaseStructure* next) { if(_base.next) {next->next = _base.next;}; _base.next = next;}              \
    constexpr static UID s_type = guid;

#define NVAIM_VALIDATE_STRUCT(S)                                                                                       \
    static_assert(std::is_standard_layout<S>::value, "NVAIM structure must have standard layout");                     \
    static_assert(std::alignment_of<S>::value == 8, "NVAIM structure must have alignment of 8");

NVAIM_VALIDATE_STRUCT(BaseStructure)

template<typename T>
bool isOfType(const BaseStructure* base)
{
    return base && base->type == T::s_type ? true : false;
}

template<typename T>
T* castTo(BaseStructure* base)
{
    return base && base->type == T::s_type ? (T*)(base) : nullptr;
}

template<typename T>
const T* castTo(const BaseStructure* base)
{
    return base && base->type == T::s_type ? (T*)(base) : nullptr;
}

template<typename T>
const T* findStruct(const BaseStructure* base)
{
    while (base && base->type != T::s_type)
    {
        base = static_cast<const BaseStructure*>(base->next);
    }
    return (T*)base;
}

template<typename T>
T* findStruct(BaseStructure* base)
{
    while (base && base->type != T::s_type)
    {
        base = static_cast<BaseStructure*>(base->next);
    }
    return (T*)base;
}

//! Find a struct of type T, but stop the search if we find a struct of type S
template<typename T, typename S>
const T* findStruct(const BaseStructure* base)
{
    while (base && base->type != T::s_type)
    {
        base = static_cast<const BaseStructure*>(base->next);

        // If we find a struct of type S, we know should stop the search
        if (base->type == S::s_type)
        {
            return nullptr;
        }
    }
    return (T*)base;
}

template<typename T>
T* findStruct(const void** ptr, uint32_t count)
{
    const BaseStructure* base{};
    for (uint32_t i = 0; base == nullptr && i < count; i++)
    {
        base = static_cast<const BaseStructure*>(ptr[i]);
        while (base && base->type != T::s_type)
        {
            base = static_cast<const BaseStructure*>(base->next);
        }
    }
    return (T*)base;
}

} // namespace nvaim
