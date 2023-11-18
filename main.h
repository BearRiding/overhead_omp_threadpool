#include <stdlib.h>
#include <assert.h>
#include <mpi.h>
#include <utofu.h>
#include <mpi-ext.h>  // Include header file
#include <algorithm>
#include <unistd.h>
#include "memory.h"
#include <omp.h>
#include <pthread.h>
#include <atomic>
#include <vector>
#include <string.h>
#include <random>

using namespace std;

#define DATA_LENGTH 32768
#define THREAD_NUM 11
#define T_THREAD  12
#define NUMA_NUM  4
#define ATOMIC_NUM  12
#define iteration  10000

#define LEN_NUM 12

class Threadpool_s {
public:
  int nprocs;
  int me;
  int numa_id; // fugaku has 4 numas, so we have to get the numa_id to band the cpu core for each thread

  double start, end;

  double buffer[DATA_LENGTH];

  void run();

  ~Threadpool_s(){
    is_shutdown_ = true;
  }

  pthread_t thread[THREAD_NUM];
  atomic<uint64_t> is_excute;

  bool is_shutdown_ = false;

  void execute(uint64_t cmd) {
    is_excute = cmd;
  };

  void single_thread(); // test single thread

  void omp_thread(); // test openmp

  void setcpu();

  void setCurMy() {
    curMy = this;  
  };

  void parral_xmit_fun(int);

  void start_pthread();

  void control_pthread();


  static Threadpool_s* curMy;

  static void* callback(void* arg){  
    Threadpool_s::curMy->parral_xmit_fun(int(arg));  
    return NULL;  
  };

  int memlen[LEN_NUM] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};

};
