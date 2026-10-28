#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "gpu_packets.hpp"
namespace Gpu {
struct ChannelExecution;
using EngineHandler=DecodeResult (*)(const MethodWrite&,ChannelExecution&);
enum class ProfileId { MaxwellReference, AmpereResearch };
enum class Generation { Maxwell, Ampere };
enum class EngineKind { Channel, Copy, Graphics3D, Compute, Inline, TwoD };
enum class ClassSupport { DeclaredOnly, HostControl, NoOperationOnly, BoundedCopy };
enum class PacketAbi { Unvalidated, B06FReference };
struct ClassInfo { uint32_t id;std::string_view name;Generation generation;EngineKind engine;ClassSupport support;EngineHandler handler=nullptr; };
struct ProfileInfo { ProfileId id;std::string_view name;PacketAbi packets;std::span<const uint32_t> classes; };
std::span<const ClassInfo> classRegistry();
const ClassInfo* findClass(uint32_t id);
const ProfileInfo* findProfile(ProfileId id);
const ProfileInfo* findProfile(std::string_view name);
bool profileAllows(ProfileId profile,uint32_t classId);
}
