#include "gpu_classes.hpp"
#include "gpu_maxwell_3d.hpp"
#include "gpu_maxwell_copy.hpp"
#include <array>
namespace Gpu {
namespace {
constexpr std::array<ClassInfo,6> classes{{
    {0xB06F,"MAXWELL_CHANNEL_GPFIFO_A",Generation::Maxwell,EngineKind::Channel,ClassSupport::HostControl},
    {0xB197,"MAXWELL_B",Generation::Maxwell,EngineKind::Graphics3D,ClassSupport::NoOperationOnly,executeMaxwell3D},
    {0xB0B5,"MAXWELL_DMA_COPY_A",Generation::Maxwell,EngineKind::Copy,ClassSupport::BoundedCopy,executeMaxwellCopy},
    {0xC6B5,"AMPERE_DMA_COPY_A",Generation::Ampere,EngineKind::Copy,ClassSupport::DeclaredOnly},
    {0xC7B5,"AMPERE_DMA_COPY_B",Generation::Ampere,EngineKind::Copy,ClassSupport::DeclaredOnly},
    {0xC797,"AMPERE_B",Generation::Ampere,EngineKind::Graphics3D,ClassSupport::DeclaredOnly}
}};
constexpr std::array<uint32_t,3> maxwell{0xB06F,0xB197,0xB0B5},ampere{0xC6B5,0xC7B5,0xC797};
const std::array<ProfileInfo,2> profiles{{
    {ProfileId::MaxwellReference,"maxwell-reference",PacketAbi::B06FReference,maxwell},
    {ProfileId::AmpereResearch,"ampere-research",PacketAbi::Unvalidated,ampere}
}};
}
std::span<const ClassInfo> classRegistry(){return classes;}
const ClassInfo* findClass(uint32_t id){for(const auto& entry:classes)if(entry.id==id)return &entry;return nullptr;}
const ProfileInfo* findProfile(ProfileId id){for(const auto& entry:profiles)if(entry.id==id)return &entry;return nullptr;}
const ProfileInfo* findProfile(std::string_view name){for(const auto& entry:profiles)if(entry.name==name)return &entry;return nullptr;}
bool profileAllows(ProfileId profile,uint32_t classId){const auto entry=findProfile(profile);if(!entry)return false;for(const auto id:entry->classes)if(id==classId)return true;return false;}
}
