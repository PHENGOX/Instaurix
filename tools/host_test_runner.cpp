#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
struct Handle {
    HANDLE value=nullptr;
    ~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
};
std::wstring quote(const std::wstring& value){
    std::wstring out=L"\"";size_t slashes=0;
    for(const auto c:value){
        if(c==L'\\'){++slashes;continue;}
        out.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;
        if(c==L'"')out+=L'\\';out+=c;
    }
    out.append(slashes*2,L'\\');out+=L'"';return out;
}
std::string json(const std::string& value){
    std::ostringstream out;out<<'"';
    for(const unsigned char c:value){
        if(c=='"' || c=='\\')out<<'\\'<<char(c);
        else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;
        else out<<char(c);
    }
    return out.str()+'"';
}
std::string utf8(const std::wstring& value){
    if(value.empty())return {};
    const int size=WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);
    std::string out(size,'\0');WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),out.data(),size,nullptr,nullptr);return out;
}
}

int wmain(int argc,wchar_t** argv){
    // Usage: runner report-directory test-name --expect-success|--expect-failure executable [arguments...].
    if(argc<5)return 2;
    const bool expectFailure=std::wstring(argv[3])==L"--expect-failure";
    if(!expectFailure && std::wstring(argv[3])!=L"--expect-success")return 2;
    const std::wstring testName=argv[2];
    if(testName.empty() || testName==L"." || testName==L"..")return 2;
    for(const auto c:testName)if(!((c>=L'a' && c<=L'z') || (c>=L'A' && c<=L'Z') ||
       (c>=L'0' && c<=L'9') || c==L'_' || c==L'-' || c==L'.'))return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    SYSTEMTIME utc{};GetSystemTime(&utc);
    std::ostringstream timestamp;timestamp<<std::setfill('0')<<std::setw(4)<<utc.wYear<<'-'
        <<std::setw(2)<<utc.wMonth<<'-'<<std::setw(2)<<utc.wDay<<'T'<<std::setw(2)<<utc.wHour
        <<':'<<std::setw(2)<<utc.wMinute<<':'<<std::setw(2)<<utc.wSecond<<'Z';
    const auto start=std::chrono::steady_clock::now();
    const auto folder=std::filesystem::path(argv[1])/(std::wstring(argv[2])+L"-"+std::to_wstring(GetCurrentProcessId()));
    std::error_code error;std::filesystem::create_directories(folder,error);
    if(error){std::cerr<<"HOST_DIAGNOSTIC_FAILURE: cannot create report directory\n";return 2;}
    const auto report=folder/L"report.json",output=folder/L"output.log";
    std::wstring command;
    for(int i=4;i<argc;++i){if(i>4)command+=L' ';command+=quote(argv[i]);}
    uint64_t peakWorking=0,peakPrivate=0,minimumRam=UINT64_MAX,minimumCommit=UINT64_MAX;
    uint64_t initialRam=0,initialCommit=0;size_t samples=0;DWORD exitCode=0,launchError=0,childPid=0;
    const auto sample=[&](HANDLE process){
        MEMORYSTATUSEX status{};status.dwLength=sizeof(status);
        if(GlobalMemoryStatusEx(&status)){
            if(!samples){initialRam=status.ullAvailPhys;initialCommit=status.ullAvailPageFile;}
            minimumRam=(std::min)(minimumRam,uint64_t(status.ullAvailPhys));
            minimumCommit=(std::min)(minimumCommit,uint64_t(status.ullAvailPageFile));
        }
        if(process){PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
            if(GetProcessMemoryInfo(process,reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))){
                peakWorking=(std::max)(peakWorking,uint64_t(memory.PeakWorkingSetSize));
                peakPrivate=(std::max)(peakPrivate,uint64_t(memory.PrivateUsage));
            }
        }
        ++samples;
    };
    const auto save=[&](const char* stage){
        const auto temporary=folder/L"report.tmp";
        {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
            out<<"{\n  \"test\": "<<json(utf8(argv[2]))<<",\n  \"stage\": "<<json(stage)
               <<",\n  \"started_utc\": "<<json(timestamp.str())<<",\n  \"child_process_id\": "<<childPid
               <<",\n  \"expected_nonzero_exit\": "<<(expectFailure?"true":"false")
               <<",\n  \"command\": "<<json(utf8(command))
               <<",\n  \"elapsed_ms\": "<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()
               <<",\n  \"exit_code\": "<<exitCode<<",\n  \"exit_hex\": \"0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<exitCode<<std::dec
               <<"\",\n  \"launch_error\": "<<launchError<<",\n  \"peak_working_set_bytes\": "<<peakWorking
               <<",\n  \"peak_sampled_private_bytes\": "<<peakPrivate<<",\n  \"initial_available_ram_bytes\": "<<initialRam
               <<",\n  \"minimum_available_ram_bytes\": "<<(minimumRam==UINT64_MAX?0:minimumRam)
               <<",\n  \"initial_available_commit_bytes\": "<<initialCommit
               <<",\n  \"minimum_available_commit_bytes\": "<<(minimumCommit==UINT64_MAX?0:minimumCommit)
               <<",\n  \"samples\": "<<samples<<"\n}\n";
            out.flush();if(!out)return false;
        }
        return MoveFileExW(temporary.c_str(),report.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    };
    sample(nullptr);
    if(!save("launching")){std::cerr<<"HOST_DIAGNOSTIC_FAILURE: cannot write report\n";return 2;}
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    Handle log{CreateFileW(output.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};
    Handle input{CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    Handle job{CreateJobObjectW(nullptr,nullptr)};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(log.value==INVALID_HANDLE_VALUE || input.value==INVALID_HANDLE_VALUE || !job.value ||
       !SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits))){
        launchError=GetLastError();save("monitor_setup_failed");std::cerr<<"HOST_DIAGNOSTIC_FAILURE: monitor setup failed\n";return 2;
    }
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdInput=input.value;startup.hStdOutput=log.value;startup.hStdError=log.value;
    PROCESS_INFORMATION info{};
    if(!CreateProcessW(argv[4],command.data(),nullptr,nullptr,TRUE,CREATE_SUSPENDED|CREATE_NO_WINDOW,nullptr,nullptr,&startup,&info)){
        launchError=GetLastError();save("launch_failed");std::cerr<<"HOST_DIAGNOSTIC_FAILURE: launch failed; report: "<<utf8(report.wstring())<<'\n';return 2;
    }
    childPid=info.dwProcessId;
    Handle process{info.hProcess},thread{info.hThread};
    if(!AssignProcessToJobObject(job.value,process.value)){
        launchError=GetLastError();TerminateProcess(process.value,2);WaitForSingleObject(process.value,INFINITE);
        save("monitor_setup_failed");std::cerr<<"HOST_DIAGNOSTIC_FAILURE: cannot supervise child\n";return 2;
    }
    if(ResumeThread(thread.value)==DWORD(-1)){
        launchError=GetLastError();TerminateJobObject(job.value,2);WaitForSingleObject(process.value,INFINITE);
        save("launch_failed");std::cerr<<"HOST_DIAGNOSTIC_FAILURE: cannot resume child\n";return 2;
    }
    bool timedOut=false,monitorFailed=false;sample(process.value);save("running");
    auto nextReport=start+std::chrono::seconds(1);
    for(;;){
        const auto waited=WaitForSingleObject(process.value,50);sample(process.value);
        if(waited==WAIT_OBJECT_0)break;
        if(waited==WAIT_FAILED){monitorFailed=true;launchError=GetLastError();TerminateJobObject(job.value,2);WaitForSingleObject(process.value,INFINITE);break;}
        const auto now=std::chrono::steady_clock::now();
        if(now-start>std::chrono::seconds(240)){timedOut=true;TerminateJobObject(job.value,0xC000013A);WaitForSingleObject(process.value,INFINITE);break;}
        if(now>=nextReport){if(!save("running"))monitorFailed=true;nextReport=now+std::chrono::seconds(1);}
    }
    GetExitCodeProcess(process.value,&exitCode);sample(process.value);
    const bool nativeFailure=(exitCode&0x80000000U)!=0 || exitCode==0x40000015U;
    const char* stage=timedOut?"timeout":monitorFailed?"monitor_failed":nativeFailure?"native_failure":exitCode?"nonzero_exit":"completed";
    if(!save(stage))monitorFailed=true;
    FlushFileBuffers(log.value);
    {std::ifstream captured(output,std::ios::binary);std::cout<<captured.rdbuf();}
    if(exitCode || timedOut || monitorFailed)std::cerr<<"Host diagnostic report: "<<utf8(report.wstring())<<'\n';
    if(nativeFailure || timedOut || monitorFailed)std::cerr<<"HOST_DIAGNOSTIC_FAILURE: "<<stage<<'\n';
    if(monitorFailed)return 2;
    if(nativeFailure || timedOut)return static_cast<int>(exitCode);
    if(expectFailure)return exitCode?0:1;
    return static_cast<int>(exitCode);
}
