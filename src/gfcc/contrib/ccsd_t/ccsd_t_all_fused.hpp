#pragma once

#include "fused_common.hpp"
#if defined(USE_DPCPP)
  #include "ccsd_t_all_fused_sycl.hpp"
#endif

#if defined(USE_HIP)
  #include "ccsd_t_all_fused_hip.hpp"
#endif


void initmemmodule();
void dev_mem_s(size_t,size_t,size_t,size_t,size_t,size_t);
void dev_mem_d(size_t,size_t,size_t,size_t,size_t,size_t);

#define TEST_NEW_KERNEL
#define TEST_NEW_THREAD

#define STREAM_REDUCE

#define CEIL(a, b)  (((a) + (b) - 1) / (b))

void hostEnergyReduce(void* data) {
  hostEnergyReduceData_t* data_t = (hostEnergyReduceData_t*) data;
  const size_t num_blocks = data_t->num_blocks;
  double* host_energies = data_t->host_energies;
  double* res = data_t->result_energy;
  double factor = data_t->factor;

  double final_energy_1 = 0.0;
  double final_energy_2 = 0.0; 
  for (size_t i = 0; i < num_blocks; i++)
  {
    final_energy_1 += host_energies[i];
    final_energy_2 += host_energies[i + num_blocks];
  }

  res[0] += final_energy_1 * factor;
  res[1] += final_energy_2 * factor;
}


// driver for the fully-fused kernel (FP64)
void fully_fused_ccsd_t_gpu(gpuStream_t* stream_id, size_t num_blocks,
	size_t base_size_h1b, size_t base_size_h2b, size_t base_size_h3b,
	size_t base_size_p4b, size_t base_size_p5b, size_t base_size_p6b,
	//
	double* df_dev_d1_t2_all, double* df_dev_d1_v2_all,
	double* df_dev_d2_t2_all, double* df_dev_d2_v2_all,
	double* df_dev_s1_t1_all, double* df_dev_s1_v2_all,
	//
	size_t size_d1_t2_all, size_t size_d1_v2_all,
	size_t size_d2_t2_all, size_t size_d2_v2_all,
	size_t size_s1_t1_all, size_t size_s1_v2_all,
	//
	int* host_d1_size, int* host_d1_exec, 	// used
	int* host_d2_size, int* host_d2_exec,
	int* host_s1_size, int* host_s1_exec,
	//
	size_t size_noab, size_t size_max_dim_d1_t2, size_t size_max_dim_d1_v2,
	size_t size_nvab, size_t size_max_dim_d2_t2, size_t size_max_dim_d2_v2,
	size_t size_max_dim_s1_t1, size_t size_max_dim_s1_v2,
	//
	double factor,
	//
	double* dev_evl_sorted_h1b, double* dev_evl_sorted_h2b, double* dev_evl_sorted_h3b,
	double* dev_evl_sorted_p4b, double* dev_evl_sorted_p5b, double* dev_evl_sorted_p6b,
	double* partial_energies
  #if defined(USE_CUDA)
	, gpuEvent_t done_compute, gpuEvent_t done_copy
  #endif
  );

#if defined(USE_NV_TC)
// driver for fully-fused kernel for 3rd gen. tensor core (FP64)
void ccsd_t_fully_fused_nvidia_tc_fp64(cudaStream_t* stream_id, size_t numBlks, 
	size_t size_h3, size_t size_h2, size_t size_h1, 
	size_t size_p6, size_t size_p5, size_t size_p4, 
	// 
	double* dev_s1_t1_all, double* dev_s1_v2_all, 
	double* dev_d1_t2_all, double* dev_d1_v2_all, 
	double* dev_d2_t2_all, double* dev_d2_v2_all, 
	// 
											int* host_size_d1_h7b, 	int* host_size_d2_p7b,
	int* host_exec_s1, 	int* host_exec_d1, 			int* host_exec_d2, 
	// 
	size_t size_noab, size_t size_nvab, 
	size_t size_max_dim_s1_t1, size_t size_max_dim_s1_v2, 
	size_t size_max_dim_d1_t2, size_t size_max_dim_d1_v2, 
	size_t size_max_dim_d2_t2, size_t size_max_dim_d2_v2, 
	// 
	double factor, 
	double* dev_evl_sorted_h1b, double* dev_evl_sorted_h2b, double* dev_evl_sorted_h3b, 
	double* dev_evl_sorted_p4b, double* dev_evl_sorted_p5b, double* dev_evl_sorted_p6b, 
	double* dev_final_energies,
	gpuEvent_t done_compute, gpuEvent_t done_copy);
#endif

// #define OPT_ALL_TIMING
// #define OPT_KERNEL_TIMING

// #define TEMP_ENABLED_OLD

template<typename T>
void ccsd_t_fully_fused_none_df_none_task(bool is_restricted, 
#if defined(USE_DPCPP)
  sycl::queue* syclQue,
#endif
  const Index noab, const Index nvab, int64_t rank,
  std::vector<int>& k_spin,
  std::vector<size_t>& k_range,
  std::vector<size_t>& k_offset,
  Tensor<T>& d_t1, Tensor<T>& d_t2, Tensor<T>& d_v2,
  std::vector<T>& k_evl_sorted,
  //
  T* df_host_pinned_s1_t1, T* df_host_pinned_s1_v2,
  T* df_host_pinned_d1_t2, T* df_host_pinned_d1_v2,
  T* df_host_pinned_d2_t2, T* df_host_pinned_d2_v2,
  T* host_energies,
  // for new fully-fused kernel
  int* host_d1_size_h7b, int* host_d2_size_p7b, 
  //
  int* df_simple_s1_size, int* df_simple_d1_size, int* df_simple_d2_size,
  int* df_simple_s1_exec, int* df_simple_d1_exec, int* df_simple_d2_exec,
  //
  T* df_dev_s1_t1_all, T* df_dev_s1_v2_all,
  T* df_dev_d1_t2_all, T* df_dev_d1_v2_all,
  T* df_dev_d2_t2_all, T* df_dev_d2_v2_all,
  T* dev_energies,
  //
  size_t t_h1b, size_t t_h2b, size_t t_h3b,
  size_t t_p4b, size_t t_p5b, size_t t_p6b,
  double factor, size_t taskid,
  size_t max_d1_kernels_pertask, size_t max_d2_kernels_pertask,
  //
  size_t size_T_s1_t1, size_t size_T_s1_v2,
  size_t size_T_d1_t2, size_t size_T_d1_v2,
  size_t size_T_d2_t2, size_t size_T_d2_v2,
  //
  std::vector<double>& energy_l,
  #if defined(USE_CUDA)
  hostEnergyReduceData_t* reduceData,
  #endif
  LRUCache<Index,std::vector<T>>& cache_s1t, LRUCache<Index,std::vector<T>>& cache_s1v,
  LRUCache<Index,std::vector<T>>& cache_d1t, LRUCache<Index,std::vector<T>>& cache_d1v,
  LRUCache<Index,std::vector<T>>& cache_d2t, LRUCache<Index,std::vector<T>>& cache_d2v
  #if defined(USE_CUDA)
  , gpuEvent_t done_compute, gpuEvent_t done_copy
  #endif
  )
{
#ifdef OPT_KERNEL_TIMING
  long double total_num_ops_s1 = 0;
  long double total_num_ops_d1 = 0;
  long double total_num_ops_d2 = 0;
#endif

#ifdef OPT_ALL_TIMING
#if defined(USE_CUDA) //|| defined(USE_HIP)
  gpuEvent_t start_init,               stop_init;
  gpuEvent_t start_fused_kernel,       stop_fused_kernel;
  gpuEvent_t start_pre_processing,     stop_pre_processing;
  gpuEvent_t start_post_processing,    stop_post_processing;
  gpuEvent_t start_collecting_data,    stop_collecting_data;

  cudaEventCreate(&start_init);             cudaEventCreate(&stop_init);
  cudaEventCreate(&start_fused_kernel);     cudaEventCreate(&stop_fused_kernel);
  cudaEventCreate(&start_pre_processing);   cudaEventCreate(&stop_pre_processing);
  cudaEventCreate(&start_post_processing);  cudaEventCreate(&stop_post_processing);
  cudaEventCreate(&start_collecting_data);  cudaEventCreate(&stop_collecting_data);

  //
  float time_ms_init              = 0.0;
  float time_ms_fused_kernel      = 0.0;
  float time_ms_pre_processing    = 0.0;
  float time_ms_post_processing   = 0.0;
  float time_ms_collecting_data   = 0.0;

  //
  cudaEventRecord(start_init);
#endif
#endif //OPT_ALL_TIMING

// create and assign streams
#if defined(USE_CUDA)|| defined(USE_HIP) || defined(USE_DPCPP)
    gpuStream_t stream;
#endif
#if defined(USE_CUDA)
  CUDA_SAFE(cudaStreamCreate(&stream));
#elif defined(USE_HIP)
  HIP_SAFE(hipStreamCreate(&stream));
#elif defined(USE_DPCPP)
    stream = *syclQue; // abb: does this need std::move(*syclQue) ?
#endif

  // Index p4b,p5b,p6b,h1b,h2b,h3b;
  const size_t max_dim_s1_t1 = size_T_s1_t1 / 9;
  const size_t max_dim_s1_v2 = size_T_s1_v2 / 9;
  const size_t max_dim_d1_t2 = size_T_d1_t2 / max_d1_kernels_pertask;
  const size_t max_dim_d1_v2 = size_T_d1_v2 / max_d1_kernels_pertask;
  const size_t max_dim_d2_t2 = size_T_d2_t2 / max_d2_kernels_pertask;
  const size_t max_dim_d2_v2 = size_T_d2_v2 / max_d2_kernels_pertask;

  //
  int df_num_s1_enabled, df_num_d1_enabled, df_num_d2_enabled; 

  // 
  size_t base_size_h1b = k_range[t_h1b];
  size_t base_size_h2b = k_range[t_h2b];
  size_t base_size_h3b = k_range[t_h3b];
  size_t base_size_p4b = k_range[t_p4b];
  size_t base_size_p5b = k_range[t_p5b];
  size_t base_size_p6b = k_range[t_p6b];

  // 
  double* host_evl_sorted_h1b = &k_evl_sorted[k_offset[t_h1b]];
  double* host_evl_sorted_h2b = &k_evl_sorted[k_offset[t_h2b]];
  double* host_evl_sorted_h3b = &k_evl_sorted[k_offset[t_h3b]];
  double* host_evl_sorted_p4b = &k_evl_sorted[k_offset[t_p4b]];
  double* host_evl_sorted_p5b = &k_evl_sorted[k_offset[t_p5b]];
  double* host_evl_sorted_p6b = &k_evl_sorted[k_offset[t_p6b]];

#if defined(USE_CUDA) || defined(USE_HIP)
  double* dev_evl_sorted_h1b = (double*)getGpuMem(sizeof(double) * base_size_h1b);
  double* dev_evl_sorted_h2b = (double*)getGpuMem(sizeof(double) * base_size_h2b);
  double* dev_evl_sorted_h3b = (double*)getGpuMem(sizeof(double) * base_size_h3b);
  double* dev_evl_sorted_p4b = (double*)getGpuMem(sizeof(double) * base_size_p4b);
  double* dev_evl_sorted_p5b = (double*)getGpuMem(sizeof(double) * base_size_p5b);
  double* dev_evl_sorted_p6b = (double*)getGpuMem(sizeof(double) * base_size_p6b);
#elif defined(USE_DPCPP)
  double* dev_evl_sorted_h1b = (double*)getGpuMem(stream, sizeof(double) * base_size_h1b);
  double* dev_evl_sorted_h2b = (double*)getGpuMem(stream, sizeof(double) * base_size_h2b);
  double* dev_evl_sorted_h3b = (double*)getGpuMem(stream, sizeof(double) * base_size_h3b);
  double* dev_evl_sorted_p4b = (double*)getGpuMem(stream, sizeof(double) * base_size_p4b);
  double* dev_evl_sorted_p5b = (double*)getGpuMem(stream, sizeof(double) * base_size_p5b);
  double* dev_evl_sorted_p6b = (double*)getGpuMem(stream, sizeof(double) * base_size_p6b);
#endif

#if defined(USE_CUDA)
#ifdef OPT_ALL_TIMING
  cudaEventRecord(stop_init);
  cudaEventSynchronize(stop_init);
  cudaEventRecord(start_collecting_data);
#endif

#ifdef STREAM_REDUCE
  cudaEventSynchronize(done_copy);
#endif
#endif

  // resets
  std::fill(df_simple_s1_exec, df_simple_s1_exec + (9), -1);
  std::fill(df_simple_d1_exec, df_simple_d1_exec + (9 * noab), -1);
  std::fill(df_simple_d2_exec, df_simple_d2_exec + (9 * nvab), -1);

  //
  ccsd_t_data_s1_new(is_restricted,noab,nvab,k_spin,
    d_t1,d_t2,d_v2,
    k_evl_sorted,k_range,
    t_h1b,t_h2b,t_h3b,
    t_p4b,t_p5b,t_p6b,
    //
    size_T_s1_t1,         size_T_s1_v2,
    df_simple_s1_size,    df_simple_s1_exec,
    df_host_pinned_s1_t1, df_host_pinned_s1_v2,
    &df_num_s1_enabled,
    //
    cache_s1t,cache_s1v);

  //
  ccsd_t_data_d1_new(is_restricted,noab,nvab,k_spin,
    d_t1,d_t2,d_v2,
    k_evl_sorted,k_range,
    t_h1b,t_h2b,t_h3b,t_p4b,t_p5b,t_p6b,
    max_d1_kernels_pertask,
    //
    size_T_d1_t2,         size_T_d1_v2,
    df_host_pinned_d1_t2, df_host_pinned_d1_v2,
    // 
    host_d1_size_h7b, 
    //  
    df_simple_d1_size,    df_simple_d1_exec,
    &df_num_d1_enabled,
    //
    cache_d1t,cache_d1v);

  //
  ccsd_t_data_d2_new(is_restricted,noab,nvab,k_spin,
    d_t1,d_t2,d_v2,
    k_evl_sorted,k_range,
    t_h1b,t_h2b,t_h3b,t_p4b,t_p5b,t_p6b,
    max_d2_kernels_pertask,
    //
    size_T_d2_t2,           size_T_d2_v2,
    df_host_pinned_d2_t2,   df_host_pinned_d2_v2,
    //  
    host_d2_size_p7b, 
    // 
    df_simple_d2_size,      df_simple_d2_exec,
    &df_num_d2_enabled,
    //
    cache_d2t, cache_d2v);

#if defined(USE_CUDA)
#ifdef OPT_ALL_TIMING
  cudaEventRecord(stop_collecting_data);
  cudaEventSynchronize(stop_collecting_data);
  cudaEventRecord(start_pre_processing);
#endif

#ifdef STREAM_REDUCE
  cudaEventSynchronize(done_compute);
#endif
#endif


#if defined(USE_CUDA)
  // this is not pinned memory.
  cudaMemcpyAsync(dev_evl_sorted_h1b, host_evl_sorted_h1b, sizeof(double) * base_size_h1b, cudaMemcpyHostToDevice, stream);
  cudaMemcpyAsync(dev_evl_sorted_h2b, host_evl_sorted_h2b, sizeof(double) * base_size_h2b, cudaMemcpyHostToDevice, stream);
  cudaMemcpyAsync(dev_evl_sorted_h3b, host_evl_sorted_h3b, sizeof(double) * base_size_h3b, cudaMemcpyHostToDevice, stream);
  cudaMemcpyAsync(dev_evl_sorted_p4b, host_evl_sorted_p4b, sizeof(double) * base_size_p4b, cudaMemcpyHostToDevice, stream);
  cudaMemcpyAsync(dev_evl_sorted_p5b, host_evl_sorted_p5b, sizeof(double) * base_size_p5b, cudaMemcpyHostToDevice, stream);
  cudaMemcpyAsync(dev_evl_sorted_p6b, host_evl_sorted_p6b, sizeof(double) * base_size_p6b, cudaMemcpyHostToDevice, stream);

  //  new tensors
  cudaMemcpyAsync(df_dev_s1_t1_all, df_host_pinned_s1_t1, sizeof(double) * (max_dim_s1_t1 * df_num_s1_enabled), cudaMemcpyHostToDevice, stream);
  cudaMemcpyAsync(df_dev_s1_v2_all, df_host_pinned_s1_v2, sizeof(double) * (max_dim_s1_v2 * df_num_s1_enabled), cudaMemcpyHostToDevice, stream);
  cudaMemcpyAsync(df_dev_d1_t2_all, df_host_pinned_d1_t2, sizeof(double) * (max_dim_d1_t2 * df_num_d1_enabled), cudaMemcpyHostToDevice, stream);
  cudaMemcpyAsync(df_dev_d1_v2_all, df_host_pinned_d1_v2, sizeof(double) * (max_dim_d1_v2 * df_num_d1_enabled), cudaMemcpyHostToDevice, stream);
  cudaMemcpyAsync(df_dev_d2_t2_all, df_host_pinned_d2_t2, sizeof(double) * (max_dim_d2_t2 * df_num_d2_enabled), cudaMemcpyHostToDevice, stream);
  cudaMemcpyAsync(df_dev_d2_v2_all, df_host_pinned_d2_v2, sizeof(double) * (max_dim_d2_v2 * df_num_d2_enabled), cudaMemcpyHostToDevice, stream);
#elif defined(USE_HIP)
  // this is not pinned memory.
  HIP_SAFE(hipMemcpyHtoDAsync(dev_evl_sorted_h1b, host_evl_sorted_h1b, sizeof(double) * base_size_h1b, stream));
  HIP_SAFE(hipMemcpyHtoDAsync(dev_evl_sorted_h2b, host_evl_sorted_h2b, sizeof(double) * base_size_h2b, stream));
  HIP_SAFE(hipMemcpyHtoDAsync(dev_evl_sorted_h3b, host_evl_sorted_h3b, sizeof(double) * base_size_h3b, stream));
  HIP_SAFE(hipMemcpyHtoDAsync(dev_evl_sorted_p4b, host_evl_sorted_p4b, sizeof(double) * base_size_p4b, stream));
  HIP_SAFE(hipMemcpyHtoDAsync(dev_evl_sorted_p5b, host_evl_sorted_p5b, sizeof(double) * base_size_p5b, stream));
  HIP_SAFE(hipMemcpyHtoDAsync(dev_evl_sorted_p6b, host_evl_sorted_p6b, sizeof(double) * base_size_p6b, stream));

  //  new tensors
  HIP_SAFE(hipMemcpyHtoDAsync(df_dev_s1_t1_all, df_host_pinned_s1_t1, sizeof(double) * (max_dim_s1_t1 * df_num_s1_enabled), stream));
  HIP_SAFE(hipMemcpyHtoDAsync(df_dev_s1_v2_all, df_host_pinned_s1_v2, sizeof(double) * (max_dim_s1_v2 * df_num_s1_enabled), stream));
  HIP_SAFE(hipMemcpyHtoDAsync(df_dev_d1_t2_all, df_host_pinned_d1_t2, sizeof(double) * (max_dim_d1_t2 * df_num_d1_enabled), stream));
  HIP_SAFE(hipMemcpyHtoDAsync(df_dev_d1_v2_all, df_host_pinned_d1_v2, sizeof(double) * (max_dim_d1_v2 * df_num_d1_enabled), stream));
  HIP_SAFE(hipMemcpyHtoDAsync(df_dev_d2_t2_all, df_host_pinned_d2_t2, sizeof(double) * (max_dim_d2_t2 * df_num_d2_enabled), stream));
  HIP_SAFE(hipMemcpyHtoDAsync(df_dev_d2_v2_all, df_host_pinned_d2_v2, sizeof(double) * (max_dim_d2_v2 * df_num_d2_enabled), stream));
#elif defined(USE_DPCPP)
  // this is not pinned memory.
  stream.memcpy(dev_evl_sorted_h1b, host_evl_sorted_h1b, sizeof(double) * base_size_h1b);
  stream.memcpy(dev_evl_sorted_h2b, host_evl_sorted_h2b, sizeof(double) * base_size_h2b);
  stream.memcpy(dev_evl_sorted_h3b, host_evl_sorted_h3b, sizeof(double) * base_size_h3b);
  stream.memcpy(dev_evl_sorted_p4b, host_evl_sorted_p4b, sizeof(double) * base_size_p4b);
  stream.memcpy(dev_evl_sorted_p5b, host_evl_sorted_p5b, sizeof(double) * base_size_p5b);
  stream.memcpy(dev_evl_sorted_p6b, host_evl_sorted_p6b, sizeof(double) * base_size_p6b);

  //  new tensors
  stream.memcpy(df_dev_s1_t1_all, df_host_pinned_s1_t1, sizeof(double) * (max_dim_s1_t1 * df_num_s1_enabled));
  stream.memcpy(df_dev_s1_v2_all, df_host_pinned_s1_v2, sizeof(double) * (max_dim_s1_v2 * df_num_s1_enabled));
  stream.memcpy(df_dev_d1_t2_all, df_host_pinned_d1_t2, sizeof(double) * (max_dim_d1_t2 * df_num_d1_enabled));
  stream.memcpy(df_dev_d1_v2_all, df_host_pinned_d1_v2, sizeof(double) * (max_dim_d1_v2 * df_num_d1_enabled));
  stream.memcpy(df_dev_d2_t2_all, df_host_pinned_d2_t2, sizeof(double) * (max_dim_d2_t2 * df_num_d2_enabled));
  stream.memcpy(df_dev_d2_v2_all, df_host_pinned_d2_v2, sizeof(double) * (max_dim_d2_v2 * df_num_d2_enabled));
#endif

  //
#ifdef OPT_ALL_TIMING
  cudaEventRecord(stop_pre_processing);
  cudaEventSynchronize(stop_pre_processing);
  cudaEventRecord(start_fused_kernel);
#endif

  //
  size_t num_blocks = CEIL(base_size_h3b,4) * CEIL(base_size_h2b,4) * CEIL(base_size_h1b,4) *
                      CEIL(base_size_p6b,4) * CEIL(base_size_p5b,4) * CEIL(base_size_p4b,4);

#ifdef OPT_KERNEL_TIMING
  //
  long double task_num_ops_s1 = 0;
  long double task_num_ops_d1 = 0;
  long double task_num_ops_d2 = 0;
  long double task_num_ops_total = 0;

  //
  helper_calculate_num_ops(noab, nvab,
    df_simple_s1_size, df_simple_d1_size, df_simple_d2_size,
    df_simple_s1_exec, df_simple_d1_exec, df_simple_d2_exec,
    task_num_ops_s1, task_num_ops_d1, task_num_ops_d2,
    total_num_ops_s1, total_num_ops_d1, total_num_ops_d2);

  //
  task_num_ops_total = task_num_ops_s1 + task_num_ops_d1 + task_num_ops_d2;
#endif

#ifdef OPT_KERNEL_TIMING
#if defined(USE_CUDA) || defined(USE_HIP)
  gpuEvent_t start_kernel_only, stop_kernel_only;
  cudaEventCreate(&start_kernel_only);
  cudaEventCreate(&stop_kernel_only);

  //
  cudaEventRecord(start_kernel_only);
#endif
#endif //OPT_KERNEL_TIMING
  
#if !defined(USE_NV_TC)
    // printf ("[%s] called the old kernel\n", __func__);
    fully_fused_ccsd_t_gpu(&stream, num_blocks,
      k_range[t_h1b],k_range[t_h2b],
      k_range[t_h3b],k_range[t_p4b],
      k_range[t_p5b],k_range[t_p6b],
      //
      df_dev_d1_t2_all, df_dev_d1_v2_all,
      df_dev_d2_t2_all, df_dev_d2_v2_all,
      df_dev_s1_t1_all, df_dev_s1_v2_all,
      //
      size_T_d1_t2, size_T_d1_v2,
      size_T_d2_t2, size_T_d2_v2,
      size_T_s1_t1, size_T_s1_v2,
      //
      //  for constant memory
      //
      df_simple_d1_size, df_simple_d1_exec,
      df_simple_d2_size, df_simple_d2_exec,
      df_simple_s1_size, df_simple_s1_exec,
      //
      noab, max_dim_d1_t2, max_dim_d1_v2,
      nvab, max_dim_d2_t2, max_dim_d2_v2,
            max_dim_s1_t1, max_dim_s1_v2,
      //
      factor,
      //
      dev_evl_sorted_h1b, dev_evl_sorted_h2b, dev_evl_sorted_h3b,
      dev_evl_sorted_p4b, dev_evl_sorted_p5b, dev_evl_sorted_p6b,
      //
      dev_energies
      #if defined(USE_CUDA)
      , done_compute, done_copy
      #endif
      );
  #else
    // printf ("[%s] called the new kernel\n", __func__);
    ccsd_t_fully_fused_nvidia_tc_fp64(&stream,num_blocks,
      k_range[t_h3b],k_range[t_h2b],k_range[t_h1b],
      k_range[t_p6b],k_range[t_p5b],k_range[t_p4b],
      //
      df_dev_s1_t1_all,df_dev_s1_v2_all,
      df_dev_d1_t2_all,df_dev_d1_v2_all,
      df_dev_d2_t2_all,df_dev_d2_v2_all,
      //
      //  for constant memory
      //
                        host_d1_size_h7b,host_d2_size_p7b, 
      df_simple_s1_exec,df_simple_d1_exec,df_simple_d2_exec,
      //
      noab, nvab, 
      max_dim_s1_t1, max_dim_s1_v2,
      max_dim_d1_t2, max_dim_d1_v2,
      max_dim_d2_t2, max_dim_d2_v2,
      //
      factor,
      //
      dev_evl_sorted_h1b, dev_evl_sorted_h2b, dev_evl_sorted_h3b,
      dev_evl_sorted_p4b, dev_evl_sorted_p5b, dev_evl_sorted_p6b,
      //
      dev_energies,
      done_compute, done_copy);
  #endif
  //
#ifdef OPT_KERNEL_TIMING
  cudaEventRecord(stop_kernel_only);
  cudaEventSynchronize(stop_kernel_only);

  float ms_time_kernel_only = 0.0;
  cudaEventElapsedTime(&ms_time_kernel_only, start_kernel_only, stop_kernel_only);
  if (rank == 0)
  {
      // printf ("[%s] s1: %lu, d1: %lu, d2: %lu >> total: %lu\n", __func__, task_num_ops_s1, task_num_ops_d1, task_num_ops_d2, task_num_ops_total);
      printf ("[ms_time_kernel_only] time: %f (ms) >> # of ops: %Lf >> %Lf GFLOPS\n", ms_time_kernel_only, task_num_ops_total, task_num_ops_total / (ms_time_kernel_only * 1000000));
  }
#endif

#if defined(USE_CUDA)
#ifdef OPT_ALL_TIMING
  CUDA_SAFE(cudaEventRecord(stop_fused_kernel));
  CUDA_SAFE(cudaEventSynchronize(stop_fused_kernel));
  CUDA_SAFE(cudaEventRecord(start_post_processing));
#endif
#endif

  // 
#if defined(USE_CUDA)
  CUDA_SAFE(cudaMemcpyAsync(host_energies, dev_energies, num_blocks * 2 * sizeof(double), cudaMemcpyDeviceToHost, stream));
#ifndef STREAM_REDUCE
  CUDA_SAFE(cudaDeviceSynchronize());
#endif
#elif defined(USE_HIP)
  HIP_SAFE(hipMemcpyHtoDAsync(host_energies, dev_energies, num_blocks * 2 * sizeof(double), stream));
  HIP_SAFE(hipStreamSynchronize(stream));
#elif defined(USE_DPCPP)
  stream.memcpy(host_energies, dev_energies, num_blocks * 2 * sizeof(double));
  stream.wait_and_throw();
#endif

  //
// #ifdef TEMP_ENABLED_OLD
  if (0) { 
    double final_energy_1 = 0.0;
    double final_energy_2 = 0.0;
    for (size_t i = 0; i < num_blocks; i++)
    {
      final_energy_1 += host_energies[i];
      final_energy_2 += host_energies[i + num_blocks];
    }

    //
    energy_l[0] += final_energy_1 * factor;
    energy_l[1] += final_energy_2 * factor;
  } else { 
// #else
  #if !defined(USE_CUDA) //#ifndef STREAM_REDUCE
    // 
    double final_energy_1 = 0.0;
    double final_energy_2 = 0.0; 
    for (size_t i = 0; i < num_blocks; i++)
    {
      final_energy_1 += host_energies[i];
      final_energy_2 += host_energies[i + num_blocks];
    }

    energy_l[0] += final_energy_1 * factor;
    energy_l[1] += final_energy_2 * factor;
  #else
    reduceData->num_blocks = num_blocks;
    reduceData->host_energies = host_energies;
    reduceData->result_energy = energy_l.data();
    reduceData->factor = factor;
    cudaLaunchHostFunc(stream, hostEnergyReduce, reduceData);
    cudaEventRecord(done_compute);
  #endif
  }
// #endif
  
  //
  //  free device and host mem. for a task.
  //
#if defined(USE_CUDA) || defined(USE_HIP)
  freeGpuMem(dev_evl_sorted_h1b); freeGpuMem(dev_evl_sorted_h2b); freeGpuMem(dev_evl_sorted_h3b);
  freeGpuMem(dev_evl_sorted_p4b); freeGpuMem(dev_evl_sorted_p5b); freeGpuMem(dev_evl_sorted_p6b);
#elif defined(USE_DPCPP)
  freeGpuMem(stream, dev_evl_sorted_h1b);
  freeGpuMem(stream, dev_evl_sorted_h2b);
  freeGpuMem(stream, dev_evl_sorted_h3b);
  freeGpuMem(stream, dev_evl_sorted_p4b);
  freeGpuMem(stream, dev_evl_sorted_p5b);
  freeGpuMem(stream, dev_evl_sorted_p6b);
#endif

#if defined(USE_CUDA)
    CUDA_SAFE(cudaStreamDestroy(stream));
#elif defined(USE_HIP)
    HIP_SAFE(hipStreamDestroy(stream));
#elif defined(USE_DPCPP)
    // DONT delete sycl::queue here, instead done in ccsd_t_fused_driver.hpp
#endif

#ifdef OPT_ALL_TIMING
  cudaEventRecord(stop_post_processing);
  cudaEventSynchronize(stop_post_processing);

  cudaEventElapsedTime(&time_ms_init,             start_init,             stop_init);
  cudaEventElapsedTime(&time_ms_pre_processing,   start_pre_processing,   stop_pre_processing);
  cudaEventElapsedTime(&time_ms_fused_kernel,     start_fused_kernel,     stop_fused_kernel);
  cudaEventElapsedTime(&time_ms_collecting_data,  start_collecting_data,  stop_collecting_data);
  cudaEventElapsedTime(&time_ms_post_processing,  start_post_processing,  stop_post_processing);

  // if (rank == 0)
  // {
  //   int tmp_dev_id = 0;
  //   cudaGetDevice(&tmp_dev_id);
  //   printf ("[%s] performed by rank: %d with dev-id: %d ----------------------\n", __func__, rank, tmp_dev_id);
  //   printf ("[%s][df-based] time-init             : %f (ms)\n", __func__, time_ms_init);
  //   printf ("[%s][df-based] time-pre-processing   : %f (ms)\n", __func__, time_ms_pre_processing);
  //   printf ("[%s][df-based] time-fused-kernel     : %f (ms)\n", __func__, time_ms_fused_kernel);
  //   printf ("[%s][df-based] time-collecting-data  : %f (ms)\n", __func__, time_ms_collecting_data);
  //   printf ("[%s][df-based] time-post-processing  : %f (ms)\n", __func__, time_ms_post_processing);
  //   printf ("[%s] ------------------------------------------------------------\n", __func__);
  // }
  double task_memcpy_time = time_ms_init + time_ms_pre_processing + time_ms_post_processing;
  double total_task_time =  task_memcpy_time + time_ms_fused_kernel + time_ms_collecting_data;
  //6dtaskid-142563,kernel,memcpy,data,total
  cout << std::fixed << std::setprecision(2) << t_h1b << "-" << t_p4b << "-" << t_h2b << "-" << t_p5b << "-" << t_p6b << "-" << t_h3b
        << ", " << time_ms_fused_kernel/1e3 << "," << task_memcpy_time/1e3 << ","
        << time_ms_collecting_data/1e3 << "," << total_task_time/1e3 << endl;
#endif
}
