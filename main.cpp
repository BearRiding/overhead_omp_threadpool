#include "main.h"

Threadpool_s* Threadpool_s::curMy = NULL; 

int iter = 0;

// test single thread
void Threadpool_s::single_thread() {
  double start, end;

  for(int _len = 0; _len < LEN_NUM; _len++) {
    start = MPI_Wtime();
    for(iter = 0; iter < iteration; iter++) {
      for(int i = 0; i < memlen[_len]; i++) {
        buffer[i] = i * _len * iter;
      }
    }
    end = MPI_Wtime() - start;
    printf("single_thread len %d, time %f\n", memlen[_len], end);fflush(stdout);
  }
}

// test omp, note that omp cannot test with threadpool, because they will contend with each other
void Threadpool_s::omp_thread() {
  double start, end;

  for(int _len = 0; _len < LEN_NUM; _len++) {
    start = MPI_Wtime();
    int len = memlen[_len];
    for(iter = 0; iter < iteration; iter++) {
      
      #pragma omp parallel for
      for(int i = 0; i < len; i++) {
        buffer[i] = i * _len * iter;
      }
    }
    end = MPI_Wtime() - start;
    printf("omp_thread len %d, time %f\n", len, end);fflush(stdout);
  }
}

// threadpool create pthread 
void Threadpool_s::start_pthread() {
  int no, res;
  int i, j;
  is_excute = 0;
  setCurMy();

  for(int i = 0; i < THREAD_NUM; i++) {
    pthread_create(&thread[i], NULL, Threadpool_s::callback, (void*)i);
  }
  setcpu();

  return ;    
}

// band cpu for each thread
void Threadpool_s::setcpu(){
  int num, ret ;
  num = sysconf(_SC_NPROCESSORS_CONF);

  int numa_id = me % 4;
  int cpuid[12];

  // printf(" node cpu  :%d  numa_id %d\n", num, numa_id); fflush(stdout);

  for(int i = 0; i < T_THREAD; i++) {
    cpu_set_t cpu_get;
    cpu_set_t cpu_set;

    cpuid[i] = 12 + numa_id * 12 + i;

    CPU_ZERO(&cpu_set);
    CPU_SET(cpuid[i], &cpu_set);

    if(i < THREAD_NUM) {
      ret = pthread_setaffinity_np(thread[i], sizeof(cpu_set_t), &cpu_set);
    } else {
      ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);
    }

    if(ret != 0) {
      printf(" fail to set affinity cpu  %d\n", i); 
      continue;
    }

    if(i < THREAD_NUM) {
      ret = pthread_getaffinity_np(thread[i], sizeof(cpu_get), &cpu_get);
    } else {
      ret = pthread_getaffinity_np(pthread_self(), sizeof(cpu_get), &cpu_get);
    }

    if(ret != 0) {
      printf(" fail to get affinity cpu  %d\n", i); 
      continue;
    }

    for (int j = 0; j < num; j++) {
      if (CPU_ISSET(j, &cpu_get)) {
        // printf("this thread tid  %d is running processor : %d\n", i, j); fflush(stdout);
      }
    }
  }
}

// use atomic variable to control each thread
void Threadpool_s::control_pthread() {
  int no, res;

  printf("begin pthread function \n");fflush(stdout);

  for(int _len = 0; _len < LEN_NUM; _len++) {
    uint64_t cmd;
    cmd   = (_len+1);
    cmd   |= cmd << 5;
    cmd   |= cmd << 10;
    cmd   |= cmd << 20;
    cmd   |= cmd << 15;

    start = MPI_Wtime();
    int len = memlen[_len];
    for( iter = 0; iter < iteration; iter++) {
      // master thread set the atomic variable
      execute(cmd);
      int idelta = 1 + len / T_THREAD;
      int ifrom = 11 * idelta;
      int ito = ((ifrom + idelta) > len) ? len : ifrom + idelta;

      for(int i = ifrom; i < ito; i++) {
        buffer[i] = i * _len * iter;
      }
      // slave threads clear the atomic variable
      while(is_excute){};
    }

    end = MPI_Wtime() - start;
    printf("threadpool len %d, time %f\n", len, end);fflush(stdout);
  }
}

// threadpool slave function 
void Threadpool_s::parral_xmit_fun(int tid) {
  int i, j;
  uint64_t tmp_ex, _len;
  int finish_ptr;
  int bit_tid = 1 << tid;

  while(1){
    // get the atomic variable
    tmp_ex = is_excute;
    // if the atomic variable is not zero
    tmp_ex = (tmp_ex >> (tid * 5)) & 0x1full;
    if (tmp_ex) {

      _len = tmp_ex - 1;
      // calculate each thread caculation Range
      int idelta = 1 + memlen[_len] / T_THREAD;
      int ifrom = tid * idelta;
      int ito = ((ifrom + idelta) > memlen[_len]) ? memlen[_len] : ifrom + idelta;

      // printf("tid %d from %d to %d  len %d delta %d\n", tid, ifrom, ito, memlen[_len], idelta);fflush(stdout);

      // multiplying iter is to Increase computational complexity . 
      // otherwise the circulation may be eliminate by the compiler

      for(int i = ifrom; i < ito; i++) {
        buffer[i] = i * _len * iter;
      }
      // after finishing task, clear the atomic variable
      is_excute &= ~(0x1full << (tid * 5));
    } else if (is_shutdown_) {
      break;
    }
  }
}

int  main(int argc, char *argv[]) {
  Threadpool_s threadpool;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &threadpool.me);

  
  threadpool.numa_id = threadpool.me % 4;

  threadpool.single_thread();

  //**************************************************************
  // test omp
  // compile command  mpic++ -O3 main.cpp -o main -pthread -fopenmp
  //**************************************************************
  // threadpool.omp_thread();

  //**************************************************************
  // test threadpool
  // compile command  mpic++ -O3 main.cpp -o main -pthread
  //**************************************************************
  threadpool.start_pthread();
  threadpool.control_pthread();
  

  
  return 0;
}