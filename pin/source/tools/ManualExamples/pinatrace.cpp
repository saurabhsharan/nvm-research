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

// for some reason, the type CACHE_STATS is used in pin_cache.H even though it isn't typedef'd in that file, so we just typedef it here
typedef UINT64 CACHE_STATS;
#include "pin_cache.H"

FILE * trace;
FILE * addrs;
int firstInstr = 0;

PIN_LOCK lock;

// cat /sys/devices/system/cpu/cpu0/cache
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

VOID PrintProcAddrs()
{
  std::ifstream t("/proc/self/maps");
  std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
  // fprintf(addrs, "%zu\n%s\n", str.length(), str.c_str());
  fprintf(addrs, "%s\n\n\n", str.c_str());
}

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
    // if (std::rand() < (RAND_MAX / 3)) {
      // fprintf(trace,"M %p R %p %s %s\n", ip, addr, dl1hit ? "H" : "M", dl3hit ? "H" : "M");
    // }
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
    // if (std::rand() < (RAND_MAX / 500)) {
      // fprintf(trace,"M %p W %p %s %s\n", ip, addr, dl1hit ? "H" : "M", dl3hit ? "H" : "M");
    // }
    PIN_ReleaseLock(&lock);

  thread_data *td = get_tls(threadid);
  td->record_mem_write(ip, addr, dl1hit || dl3hit);
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
  // if (firstInstr == 0) {
    // fprintf(addrs, "0x%lx %s\n", INS_Address(ins), INS_Disassemble(ins).c_str());
    // PrintProcAddrs();
    // firstInstr = 1;
  // }

  // if (std::rand() < RAND_MAX / 20) {
    // PrintProcAddrs();
    // fprintf(trace, "ADDR\n");
  // }

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
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_THREAD_ID,
                IARG_END);
        }
    }
}

// Print syscall number and arguments
VOID SysBefore(ADDRINT ip, ADDRINT num, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5)
{
#if defined(TARGET_LINUX) && defined(TARGET_IA32)
  // On ia32 Linux, there are only 5 registers for passing system call arguments,
  // but mmap needs 6. For mmap on ia32, the first argument to the system call
  // is a pointer to an array of the 6 arguments
  if (num == SYS_mmap)
  {
    ADDRINT * mmapArgs = reinterpret_cast<ADDRINT *>(arg0);
    arg0 = mmapArgs[0];
    arg1 = mmapArgs[1];
    arg2 = mmapArgs[2];
    arg3 = mmapArgs[3];
    arg4 = mmapArgs[4];
    arg5 = mmapArgs[5];
  }
#endif

  // PrintProcAddrs();
  /*
  fprintf(trace,"S 0x%lx %ld 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx ",
      (unsigned long)ip,
      (long)num,
      (unsigned long)arg0,
      (unsigned long)arg1,
      (unsigned long)arg2,
      (unsigned long)arg3,
      (unsigned long)arg4,
      (unsigned long)arg5);
  */
}

VOID SyscallEntry(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *v)
{
  SysBefore(PIN_GetContextReg(ctxt, REG_INST_PTR),
      PIN_GetSyscallNumber(ctxt, std),
      PIN_GetSyscallArgument(ctxt, std, 0),
      PIN_GetSyscallArgument(ctxt, std, 1),
      PIN_GetSyscallArgument(ctxt, std, 2),
      PIN_GetSyscallArgument(ctxt, std, 3),
      PIN_GetSyscallArgument(ctxt, std, 4),
      PIN_GetSyscallArgument(ctxt, std, 5));
}

// Print the return value of the system call
VOID SysAfter(ADDRINT ret)
{
      // fprintf(trace,"0x%lx\n", (unsigned long)ret);
          // fflush(trace);
          PrintProcAddrs();
}


VOID SyscallExit(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *v)
{
      SysAfter(PIN_GetSyscallReturn(ctxt, std));
}

VOID Fini(INT32 code, VOID *v)
{
    // fclose(addrs);
    // cout << "fini" << endl;

    std::map<uint64_t, uint64_t> aggregate_page_counts_read;
    std::map<uint64_t, uint64_t> aggregate_page_counts_write;
    std::map<uint64_t, uint64_t> aggregate_page_counts_total;

    for (size_t i = 0; i < all_thread_data.size(); i++) {
      thread_data *td = all_thread_data[i];

      std::map<uint64_t, uint64_t> &td_read = td->page_count_read_with_cache;
      std::map<uint64_t, uint64_t> &td_write = td->page_count_write_with_cache;

      for (std::map<uint64_t, uint64_t>::iterator it = td_read.begin(); it != td_read.end(); ++it) {
        uint64_t pageno = it->first;
        uint64_t count = it->second;

        if (aggregate_page_counts_read.find(pageno) == aggregate_page_counts_read.end()) {
          aggregate_page_counts_read[pageno] = 0;
        }

        if (aggregate_page_counts_total.find(pageno) == aggregate_page_counts_total.end()) {
          aggregate_page_counts_total[pageno] = 0;
        }

        aggregate_page_counts_read[pageno] += count;
        aggregate_page_counts_total[pageno] += count;
      }

      for (std::map<uint64_t, uint64_t>::iterator it = td_write.begin(); it != td_write.end(); ++it) {
        uint64_t pageno = it->first;
        uint64_t count = it->second;

        if (aggregate_page_counts_write.find(pageno) == aggregate_page_counts_write.end()) {
          aggregate_page_counts_write[pageno] = 0;
        }

        if (aggregate_page_counts_total.find(pageno) == aggregate_page_counts_total.end()) {
          aggregate_page_counts_total[pageno] = 0;
        }

        aggregate_page_counts_write[pageno] += count;
        aggregate_page_counts_total[pageno] += count;
      }
    }

    cout << endl << endl << endl << endl << endl;
    // fprintf(trace, "\n\n\n\n\n\n\n\n\n\n\n\n");

    cout << "==== READS ====" << endl;
    fprintf(trace, "## READS\n");
    for (std::map<uint64_t, uint64_t>::iterator it = aggregate_page_counts_read.begin(); it != aggregate_page_counts_read.end(); ++it) {
      cout << it->first << ": " << it->second << endl;
      fprintf(trace, "%lu: %lu\n", it->first, it->second);
    }

    cout << endl << endl;

    cout << "==== WRITES ====" << endl;
    fprintf(trace, "## WRITES\n");
    for (std::map<uint64_t, uint64_t>::iterator it = aggregate_page_counts_write.begin(); it != aggregate_page_counts_write.end(); ++it) {
      cout << it->first << ": " << it->second << endl;
      fprintf(trace, "%lu: %lu\n", it->first, it->second);
    }

    cout << endl << endl;

    cout << "==== TOTAL ====" << endl;
    fprintf(trace, "## TOTAL\n");
    for (std::map<uint64_t, uint64_t>::iterator it = aggregate_page_counts_total.begin(); it != aggregate_page_counts_total.end(); ++it) {
      cout << it->first << ": " << it->second << endl;
      fprintf(trace, "%lu: %lu\n", it->first, it->second);
    }

    fclose(trace);
}

VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    PIN_GetLock(&lock, 0);

    // std::cout << "Starting thread " << threadid << endl;

    thread_data *td = new thread_data;
    PIN_SetThreadData(tls_key, td, threadid);

    all_thread_data.push_back(td);

    PIN_ReleaseLock(&lock);
}

// VOID ThreadFini(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 flags, VOID *v)
{
    PIN_GetLock(&lock, 0);

    // std::cout << "Ending thread " << threadid << endl;

    PIN_ReleaseLock(&lock);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n"
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    PIN_InitLock(&lock);

    if (PIN_Init(argc, argv)) return Usage();

    PIN_InitLock(&lock);

    tls_key = PIN_CreateThreadDataKey(0);

    dl1cache = new L1Cache("L1 Data Cache", 64 * KILO, 64, 1);
    dl3cache = new L3Cache("L3 Unified Cache", /* size = */ 8192 * KILO, /* block size = */ 64, /* associativity = */ 16);

    trace = fopen(std::getenv("PINATRACE_OUTPUT_FILENAME"), "w");
    // addrs = fopen("pinaaddrs.out", "w");

    std::srand(std::time(0));

    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);

    // PIN_AddSyscallEntryFunction(SyscallEntry, 0);
    // PIN_AddSyscallExitFunction(SyscallExit, 0);
    INS_AddInstrumentFunction(Instruction, 0);

    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
