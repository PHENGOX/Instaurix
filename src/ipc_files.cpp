#include "ipc.hpp"
#include "bus.hpp"
#include "memory.hpp"
#include <algorithm>

namespace Ipc {
uint32_t dispatchFiles(const Buffer& request,Bus& bus,uint64_t ipcAddress,
    const ReadOnlyFiles* files,const ReadOnlyFiles::Bytes* file,Transfer& transfer,FileRequest& plan)
{
    Layout layout;
    auto status=parseBuffers(request,&bus,ipcAddress,layout);
    if(status) return status;
    FileRequest staged;
    Transfer output;
    auto read=[&](size_t at) { uint64_t v=0; for(size_t i=0;i<8;++i) v|=uint64_t(request[at+i])<<(8*i); return v; };
    if(!file)
    {
        if(!files) staged.result=FilesDisabled;
        else if(layout.command!=0) staged.result=UnknownCommand;
        else if(layout.sends!=1 || layout.receives || layout.payloadSize) staged.result=InvalidArguments;
        else if(layout.sourceSize<2 || layout.sourceSize>65) staged.result=InvalidPath;
        else {
            std::string name;
            for(size_t i=0;i<layout.sourceSize;++i) name+=char(bus.getMemory().read8(layout.source+i));
            if(name.back()!=0) staged.result=InvalidPath;
            else {
                name.pop_back();
                if(!ReadOnlyFiles::validName(name)) staged.result=InvalidPath;
                else { staged.opened=files->find(name); if(!staged.opened) staged.result=FileNotFound; }
            }
        }
    }
    else if(layout.command==1) // GetSize
    {
        if(layout.sends || layout.receives || layout.payloadSize) staged.result=InvalidArguments;
        else { staged.hasValue=true; staged.value=file->size(); }
    }
    else if(layout.command==0) // Read(offset, length), positional and without a mutable cursor.
    {
        if(layout.sends || layout.receives!=1 || layout.payloadSize!=16) staged.result=InvalidArguments;
        else {
            const auto offset=read(layout.payload), count=read(layout.payload+8);
            if(count>layout.destinationSize || count>MaxTransfer) staged.result=InvalidArguments;
            else {
                const size_t size=offset>=file->size() ? 0 : size_t(std::min<uint64_t>(count,file->size()-offset));
                output.address=layout.destination;
                if(size) output.bytes.assign(file->begin()+size_t(offset),file->begin()+size_t(offset)+size);
                staged.hasValue=true; staged.value=size;
            }
        }
    }
    else staged.result=UnknownCommand; // Includes every attempted write operation.
    plan=std::move(staged); transfer=std::move(output);
    return 0;
}
}
