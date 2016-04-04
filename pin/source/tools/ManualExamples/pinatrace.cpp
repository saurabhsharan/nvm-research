#include <string>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <streambuf>
#include <stdio.h>
#include <syscall.h>
#include "pin.H"
#include <iostream>
#include <inttypes.h>
#include <map>
#include "json.h"

// for some reason, the type CACHE_STATS is used in pin_cache.H even though it isn't typedef'd in that file, so we just typedef it here
typedef UINT64 CACHE_STATS;
#include "pin_cache.H"

FILE * trace;

PIN_LOCK lock;

// cat /sys/devices/system/cpu/cpu0/cache to get cache stats
typedef CACHE_DIRECT_MAPPED(KILO, CACHE_ALLOC::STORE_ALLOCATE) L1Cache;
typedef CACHE_ROUND_ROBIN(8192, 16, CACHE_ALLOC::STORE_ALLOCATE) L3Cache;

L1Cache *dl1cache = NULL;
L3Cache *dl3cache = NULL;

struct thread_data
{
public:
  thread_data() {}
  std::map<uint64_t, uint64_t> page_count_read_with_cache;
  std::map<uint64_t, uint64_t> page_count_read_without_cache;
  std::map<uint64_t, uint64_t> page_count_write_with_cache;
  std::map<uint64_t, uint64_t> page_count_write_without_cache;

  void record_mem_read(void *ip, void *addr, bool cache_hit) {
    uint64_t pageno = ((uint64_t)(addr)) / 4096;
    if (page_count_read_without_cache.find(pageno) == page_count_read_without_cache.end()) {
      page_count_read_without_cache[pageno] = 0;
    }
    page_count_read_without_cache[pageno]++;

    if (!cache_hit) {
      if (page_count_read_with_cache.find(pageno) == page_count_read_with_cache.end()) {
        page_count_read_with_cache[pageno] = 0;
      }
      page_count_read_with_cache[pageno]++;
    }
  }

  void record_mem_write(void *ip, void *addr, bool cache_hit) {
    uint64_t pageno = ((uint64_t)(addr)) / 4096;
    if (page_count_write_without_cache.find(pageno) == page_count_write_without_cache.end()) {
      page_count_write_without_cache[pageno] = 0;
    }
    page_count_write_without_cache[pageno]++;

    if (!cache_hit) {
      if (page_count_write_with_cache.find(pageno) == page_count_write_with_cache.end()) {
        page_count_write_with_cache[pageno] = 0;
      }
      page_count_write_with_cache[pageno]++;
    }
  }
};

TLS_KEY tls_key;

std::vector<thread_data *> all_thread_data;

thread_data *get_tls(THREADID threadid)
{
    return static_cast<thread_data *>(PIN_GetThreadData(tls_key, threadid));
}

// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr, THREADID threadid)
{
    PIN_GetLock(&lock, 0);
    bool dl1hit = dl1cache->AccessSingleLine((ADDRINT)addr, CACHE_BASE::ACCESS_TYPE_LOAD);
    bool dl3hit = dl3cache->AccessSingleLine((ADDRINT)addr, CACHE_BASE::ACCESS_TYPE_LOAD);
    PIN_ReleaseLock(&lock);

    thread_data *td = get_tls(threadid);
    td->record_mem_read(ip, addr, dl1hit || dl3hit);
}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr, THREADID threadid)
{
    PIN_GetLock(&lock, 0);
    bool dl1hit = dl1cache->AccessSingleLine((ADDRINT)addr, CACHE_BASE::ACCESS_TYPE_STORE);
    bool dl3hit = dl3cache->AccessSingleLine((ADDRINT)addr, CACHE_BASE::ACCESS_TYPE_STORE);
    PIN_ReleaseLock(&lock);

    thread_data *td = get_tls(threadid);
    td->record_mem_write(ip, addr, dl1hit || dl3hit);
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP
    // prefixed instructions appear as predicated instructions in Pin.
    UINT32 memOperands = INS_MemoryOperandCount(ins);

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            // TODO(saurabh): register different function based on whether we want to use cache or not (run-time configuration flag)
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_THREAD_ID,
                IARG_END);
        }
        // Note that in some architectures a single memory operand can be
        // both read and written (for instance incl (%eax) on IA-32)
        // In that case we instrument it once for read and once for write.
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            // TODO(saurabh): register different function based on whether we want to use cache or not (run-time configuration flag)
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_THREAD_ID,
                IARG_END);
        }
    }
}

thread_data aggregate_thread_data()
{
  thread_data result;

  for (size_t i = 0; i < all_thread_data.size(); i++) {
    thread_data *td = all_thread_data[i];

    for (std::map<uint64_t, uint64_t>::iterator it = td->page_count_read_with_cache.begin(); it != td->page_count_read_with_cache.end(); ++it) {
      result.page_count_read_with_cache[it->first] += it->second;
    }

    for (std::map<uint64_t, uint64_t>::iterator it = td->page_count_read_without_cache.begin(); it != td->page_count_read_without_cache.end(); ++it) {
      result.page_count_read_without_cache[it->first] += it->second;
    }

    for (std::map<uint64_t, uint64_t>::iterator it = td->page_count_write_with_cache.begin(); it != td->page_count_write_with_cache.end(); ++it) {
      result.page_count_write_with_cache[it->first] += it->second;
    }

    for (std::map<uint64_t, uint64_t>::iterator it = td->page_count_write_without_cache.begin(); it != td->page_count_write_without_cache.end(); ++it) {
      result.page_count_write_without_cache[it->first] += it->second;
    }
  }

  return result;
}

Json::Value convert_uint64_map_to_json_value(std::map<uint64_t, uint64_t> m)
{
    Json::Value result(Json::objectValue);

    for (std::map<uint64_t, uint64_t>::iterator it = m.begin(); it != m.end(); ++it) {
      // convert uint64_t to string
      std::ostringstream o, o2;
      o << it->first;
      o2 << it->second;

      result[o.str()] = o2.str();
    }

    return result;
}

VOID Fini(INT32 code, VOID *v)
{
    Json::Value cache_data(Json::arrayValue);
    Json::Value no_cache_data(Json::arrayValue);

    for (size_t i = 0; i < all_thread_data.size(); i++) {
      Json::Value threadData_Cache(Json::objectValue);
      threadData_Cache["reads"] = convert_uint64_map_to_json_value(all_thread_data[i]->page_count_read_with_cache);
      threadData_Cache["writes"] = convert_uint64_map_to_json_value(all_thread_data[i]->page_count_write_with_cache);
      cache_data.append(threadData_Cache);

      Json::Value threadData_noCache(Json::objectValue);
      threadData_noCache["reads"] = convert_uint64_map_to_json_value(all_thread_data[i]->page_count_read_without_cache);
      threadData_noCache["writes"] = convert_uint64_map_to_json_value(all_thread_data[i]->page_count_write_without_cache);
      no_cache_data.append(threadData_noCache);
    }

    Json::Value root(Json::objectValue);
    root["cache"] = cache_data;
    root["no_cache"] = no_cache_data;

    ofstream ofs(std::getenv("PINATRACE_OUTPUT_FILENAME"), ofstream::out);
    ofs << root << endl;
    ofs.close();
}

VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    PIN_GetLock(&lock, 0);

    thread_data *td = new thread_data;

    PIN_SetThreadData(tls_key, td, threadid);

    all_thread_data.push_back(td);

    PIN_ReleaseLock(&lock);
}

VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 flags, VOID *v)
{
    // TODO(saurabh): (debug) log that thread finished
}

INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n"
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

int main(int argc, char *argv[])
{
    PIN_InitLock(&lock);

    if (PIN_Init(argc, argv)) return Usage();

    PIN_InitLock(&lock);

    tls_key = PIN_CreateThreadDataKey(0);

    dl1cache = new L1Cache("L1 Data Cache", 64 * KILO, 64, 1);
    dl3cache = new L3Cache("L3 Unified Cache", /* size = */ 8192 * KILO, /* block size = */ 64, /* associativity = */ 16);

    std::srand(std::time(0));

    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);

    INS_AddInstrumentFunction(Instruction, 0);

    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}


// include source for json library
#include "json.cpp.inc"
