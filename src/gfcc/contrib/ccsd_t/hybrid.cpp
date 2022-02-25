/*------------------------------------------hybrid execution------------*/
/* $Id$ */

#include <assert.h>
///#define NUM_DEVICES 1
static long long device_id=-1;
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <iomanip>
#include "ccsd_t_common.hpp"
#include "mpi.h"
#include "ga/ga.h"
#include "ga/ga-mpi.h"
#include "ga/typesf2c.h"

//
int util_my_smp_index(){
  auto ppn = GA_Cluster_nprocs(0);
  return GA_Nodeid()%ppn;
}

//
//
//
#define NUM_RANKS_PER_GPU   1

std::string check_memory_req(const int nDevices, const int cc_t_ts, const int nbf) {
  int dev_count_check = 0;
  if(nDevices <= 0) return "";
  double global_gpu_mem = 0;
  std::string errmsg = "";
  
  #if defined(USE_CUDA)
  CUDA_SAFE(cudaGetDeviceCount(&dev_count_check));
  if(dev_count_check < nDevices){
    errmsg = "ERROR: Please check whether you have " + std::to_string(nDevices)
     + " cuda devices per node and set the ngpu option accordingly";
  }
  cudaDeviceProp gpu_properties;
  CUDA_SAFE(cudaGetDeviceProperties(&gpu_properties,0));
  global_gpu_mem = gpu_properties.totalGlobalMem;

  #elif defined(USE_HIP)
  HIP_SAFE(hipGetDeviceCount(&dev_count_check));
  if(dev_count_check < nDevices){
    errmsg = "ERROR: Please check whether you have " + std::to_string(nDevices)
     + " hip devices per node and set the ngpu option accordingly";
  }
  hipDeviceProp_t gpu_properties;
  HIP_SAFE(hipGetDeviceProperties(&gpu_properties,0));
  global_gpu_mem = gpu_properties.totalGlobalMem;  

  #elif defined(USE_DPCPP)
  {
    sycl::platform platform(sycl::gpu_selector{});
    auto const& gpu_devices = platform.get_devices(sycl::info::device_type::gpu);
    for (int i = 0; i < gpu_devices.size(); i++) {
      if(gpu_devices[i].get_info<sycl::info::device::partition_max_sub_devices>() > 0) {
        auto SubDevicesDomainNuma = gpu_devices[i].create_sub_devices<sycl::info::partition_property::partition_by_affinity_domain>(
                                                                                                                                    sycl::info::partition_affinity_domain::numa);
        dev_count_check += SubDevicesDomainNuma.size();
      }
      else {
        dev_count_check++;
      }
    }
    if(dev_count_check < nDevices){
      errmsg = "ERROR: Please check whether you have " + std::to_string(nDevices)
        + " sycl devices per node and set the ngpu option accordingly";
    }

    if (gpu_devices[0].is_gpu()) {
      if(gpu_devices[0].get_info<sycl::info::device::partition_max_sub_devices>() > 0) {
        auto SubDevicesDomainNuma = gpu_devices[0].create_sub_devices<sycl::info::partition_property::partition_by_affinity_domain>(
          sycl::info::partition_affinity_domain::numa);
        global_gpu_mem = SubDevicesDomainNuma[0].get_info<sycl::info::device::global_mem_size>();
      }
      else {
        global_gpu_mem = gpu_devices[0].get_info<sycl::info::device::global_mem_size>();
      }
    }
  }  
  #endif

  const double gpu_mem_req = (9.0 * (std::pow(cc_t_ts, 2) + std::pow(cc_t_ts, 4) + 2*2*nbf*std::pow(cc_t_ts, 3)) * 8);
  int gpu_mem_check=0;
  if(gpu_mem_req >= global_gpu_mem) gpu_mem_check = 1;
  if(gpu_mem_check) {
    const double gib = 1024*1024*1024.0;
    errmsg = "ERROR: GPU memory not sufficient for (T) calculation, available memory per gpu: "
     + std::to_string(global_gpu_mem/gib) + " GiB, required: " + std::to_string(gpu_mem_req/gib)
     + " GiB. Please set a smaller tilesize and retry";
  }

  return errmsg;

}

int check_device(long iDevice) {
  /* Check whether this process is associated with a GPU */
  // printf ("[%s] util_my_smp_index(): %d\n", __func__, util_my_smp_index());
  if((util_my_smp_index()) < iDevice * NUM_RANKS_PER_GPU) return 1;
  return 0;
}

int device_init(
#if defined(USE_DPCPP)
		const std::vector<sycl::queue*> iDevice_syclQueue,
		sycl::queue **syclQue,
#endif
                long iDevice,int *gpu_device_number) {

  /* Set device_id */
  int dev_count_check = 0;
#if defined(USE_CUDA)
  CUDA_SAFE(cudaGetDeviceCount(&dev_count_check));
#elif defined(USE_HIP)
  HIP_SAFE(hipGetDeviceCount(&dev_count_check));
#elif defined(USE_DPCPP)
  sycl::platform platform(sycl::gpu_selector{});
  auto const& gpu_devices = platform.get_devices(sycl::info::device_type::gpu);
  for (const auto &dev : gpu_devices) {
    if(dev.get_info<sycl::info::device::partition_max_sub_devices>() > 0) {
      auto SubDevicesDomainNuma = dev.create_sub_devices<sycl::info::partition_property::partition_by_affinity_domain>(
                                                                                                                       sycl::info::partition_affinity_domain::numa);
      dev_count_check += SubDevicesDomainNuma.size();
    }
    else {
      dev_count_check++;
    }
  }
#endif

  //
  device_id = util_my_smp_index();
  int actual_device_id = device_id % dev_count_check;

  // printf ("[%s] device_id: %lld (%d), dev_count_check: %d, iDevice: %ld\n", __func__, device_id, actual_device_id, dev_count_check, iDevice);

  if(dev_count_check < iDevice) {
    printf("Warning: Please check whether you have %ld devices per node\n",iDevice);
    fflush(stdout);
    *gpu_device_number = 30;
  }
  else {
#if defined(USE_CUDA)
    // cudaSetDevice(device_id);
    CUDA_SAFE(cudaSetDevice(actual_device_id));
#elif defined(USE_HIP)
    // hipSetDevice(device_id);
    HIP_SAFE(hipSetDevice(actual_device_id));
#elif defined(USE_DPCPP)
    *syclQue = iDevice_syclQueue[actual_device_id];
#endif
  }
  return 1;
}
