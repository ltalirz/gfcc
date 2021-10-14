#ifndef TAMM_MEMORY_MANAGER_GA_H_
#define TAMM_MEMORY_MANAGER_GA_H_

#include "tamm/memory_manager.hpp"
#include "armci.h"
#include "ga/ga.h"
#include "ga/ga-mpi.h"

#include <vector>
#include <string>
#include <numeric>

//@todo Check visibility: public, private, protected

//@todo implement attach()

//@todo make MemoryManager also has alloc/dealloc semantics

namespace tamm {

class MemoryManagerGA;

/**
 * @ingroup memory_management
 * @brief Memory region that allocates memory using Global Arrays
 */
class MemoryRegionGA : public MemoryRegionImpl<MemoryManagerGA> {
 public:
  MemoryRegionGA(MemoryManagerGA& mgr)
      : MemoryRegionImpl<MemoryManagerGA>(mgr) {}

  /**
   * Access the underying global arrays
   * @return Handle to underlying global array
   */
  int ga() const {
    return ga_;
  }

 private:
  int ga_;
  ElementType eltype_;
  std::vector<int64_t> map_;

  friend class MemoryManagerGA;
}; // class MemoryRegionGA


/**
 * @ingroup memory_management
 * @brief Memory manager that wraps Glocal Arrays API
 */
class MemoryManagerGA : public MemoryManager {
 public:
  /**
   * @brief Collectively create a MemoryManagerGA.
   *
   * Creation of a GA memory manager involves creation of a GA process group.
   * To make this explicit, the call is static with an _coll suffix.
   * @param pg Process group in which to create GA memory manager
   * @return Constructed GA memory manager
   */
  static MemoryManagerGA* create_coll(ProcGroup pg) {
    return new MemoryManagerGA{pg};
  }

  /**
   * @brief Collectively destroy a GA meomry manager
   * @param mmga
   */
  static void destroy_coll(MemoryManagerGA* mmga) {
    delete mmga;
  }

  /**
   * @copydoc MemoryManager::attach_coll
   */
  MemoryRegion* alloc_coll(ElementType eltype, Size local_nelements) override {
    MemoryRegionGA* pmr;
    {
     TimerGuard tg_total{&memTime3};
     pmr = new MemoryRegionGA(*this);

      int ga_pg_default = GA_Pgroup_get_default();
      GA_Pgroup_set_default(ga_pg_);
    int nranks = pg_.size().value();
    int ga_eltype = to_ga_eltype(eltype);

    pmr->map_.resize(nranks + 1);
    pmr->eltype_ = eltype;
    pmr->local_nelements_ = local_nelements;
    int64_t nels = local_nelements.value();

    int64_t nelements_min, nelements_max;

    GA_Pgroup_set_default(ga_pg_);
    {
       TimerGuard tg_total{&memTime5};
       nelements_min = pg_.allreduce(&nels, ReduceOp::min);
       nelements_max = pg_.allreduce(&nels, ReduceOp::max);
    }
    std::string array_name{"array_name"+std::to_string(++ga_counter_)};

    if (nelements_min == nels && nelements_max == nels) {
      int64_t dim = nranks * nels, chunk = -1;
      pmr->ga_ = NGA_Create64(ga_eltype, 1, &dim, const_cast<char*>(array_name.c_str()), &chunk);
      pmr->map_[0] = 0;
      std::fill_n(pmr->map_.begin()+1, nranks-1, nels);
      std::partial_sum(pmr->map_.begin(), pmr->map_.begin()+nranks, pmr->map_.begin());
    } else {
      int64_t dim, block = nranks;
      {
      TimerGuard tg_total{&memTime5};
      dim = pg_.allreduce(&nels, ReduceOp::sum);
      }
      {
      TimerGuard tg_total{&memTime6};
      pg_.allgather(&nels, &pmr->map_[1]);
      }
      pmr->map_[0] = 0; // @note this is not set by MPI_Exscan
     {
       TimerGuard tg_total{&memTime7};      
       std::partial_sum(pmr->map_.begin(), pmr->map_.begin()+nranks, pmr->map_.begin());
     }
      
      for(block = nranks; block>0 && static_cast<int64_t>(pmr->map_[block-1]) == dim; --block) {
        //no-op
      }

      int64_t* map_start = &pmr->map_[0];
      for(int i=0; i<nranks && *(map_start+1)==0; ++i, ++map_start, --block) {
        //no-op
      }
      
           {
       TimerGuard tg_total{&memTime9};
      pmr->ga_ = NGA_Create_irreg64(ga_eltype, 1, &dim, const_cast<char*>(array_name.c_str()), &block, map_start);
           }
           memTime4+=memTime9;
           memTime9=0;
      
    }

    GA_Pgroup_set_default(ga_pg_default);

    int64_t lo, hi;//, ld;
     {
       TimerGuard tg_total{&memTime8};    
       NGA_Distribution64(pmr->ga_, pg_.rank().value(), &lo, &hi);
     }
    EXPECTS(nels<=0 || lo == static_cast<int64_t>(pmr->map_[pg_.rank().value()]));
    EXPECTS(nels<=0 || hi == static_cast<int64_t>(pmr->map_[pg_.rank().value()]) + nels - 1);
    pmr->set_status(AllocationStatus::created);
          }
    return pmr;
  }

  MemoryRegion* alloc_coll_balanced(ElementType eltype,
                                    Size max_nelements) override {

    MemoryRegionGA* pmr = nullptr;
     {
       TimerGuard tg_total{&memTime3}; 
    pmr = new MemoryRegionGA{*this};
    int nranks = pg_.size().value();
    int ga_eltype = to_ga_eltype(eltype);

    pmr->map_.resize(nranks + 1);
    pmr->eltype_ = eltype;
    pmr->local_nelements_ = max_nelements;
    int64_t nels = max_nelements.value();

    std::string array_name{"array_name" + std::to_string(++ga_counter_)};

    int64_t dim = nranks * nels, chunk = nels;
    pmr->ga_ = NGA_Create_handle();
    NGA_Set_data64(pmr->ga_, 1, &dim, ga_eltype);
    GA_Set_chunk64(pmr->ga_, &chunk);
    GA_Set_pgroup(pmr->ga_, pg().ga_pg());

        {
    TimerGuard tg_total{&memTime9};
    NGA_Allocate(pmr->ga_);
        }
    memTime4+=memTime9;
    memTime9=0;

    pmr->map_[0] = 0;
     {
       TimerGuard tg_total{&memTime7};     
    std::fill_n(pmr->map_.begin() + 1, nranks - 1, nels);
    std::partial_sum(pmr->map_.begin(), pmr->map_.begin() + nranks,
                     pmr->map_.begin());
     }
    pmr->set_status(AllocationStatus::created);
     }
    return pmr;
  }

  /**
   * @copydoc MemoryManager::attach_coll
   */
  MemoryRegion* attach_coll(MemoryRegion& mrb) override {
    MemoryRegionGA& mr_rhs = static_cast<MemoryRegionGA&>(mrb);
    MemoryRegionGA* pmr = new MemoryRegionGA(*this);

    pmr->map_ = mr_rhs.map_;
    pmr->eltype_ = mr_rhs.eltype_;
    pmr->local_nelements_ = mr_rhs.local_nelements_;
    pmr->ga_ = mr_rhs.ga_;
    pmr->set_status(AllocationStatus::attached);
    return pmr;
  }

  /**
   * @brief Fence all operations on this memory region
   * @param mr Memory region to be fenced
   *
   * @todo Use a possibly more efficient fence
   */
  void fence(MemoryRegion& mr) override {
    ARMCI_AllFence();
  }

  protected:
  explicit MemoryManagerGA(ProcGroup pg)
      : MemoryManager{pg, MemoryManagerKind::ga} {
    EXPECTS(pg.is_valid());
    pg_ = pg;
    ga_pg_ = pg.ga_pg();
  }

  ~MemoryManagerGA() = default;

 public:
  /**
   * @copydoc MemoryManager::dealloc_coll
   */
  void dealloc_coll(MemoryRegion& mrb) override {
    MemoryRegionGA& mr = static_cast<MemoryRegionGA&>(mrb);
    NGA_Destroy(mr.ga_);
    mr.ga_ = -1;
  }

  /**
   * @copydoc MemoryManager::detach_coll
   */
  void detach_coll(MemoryRegion& mrb) override {
    MemoryRegionGA& mr = static_cast<MemoryRegionGA&>(mrb);
    mr.ga_ = -1;
  }

  /**
   * @copydoc MemoryManager::access
   */
  const void* access(const MemoryRegion& mrb, Offset off) const override {
    const MemoryRegionGA& mr = static_cast<const MemoryRegionGA&>(mrb);
    Proc proc{pg_.rank()};
    TAMM_SIZE nels{1};
    TAMM_SIZE ioffset{mr.map_[proc.value()] + off.value()};
    int64_t lo = ioffset, hi = ioffset + nels-1, ld = -1;
    void* buf;
    NGA_Access64(mr.ga_, &lo, &hi, reinterpret_cast<void*>(&buf), &ld);
    return buf;
  }

  /**
   * @copydoc MemoryManager::get
   */
  void get(MemoryRegion& mrb, Proc proc, Offset off, Size nelements, void* to_buf) override {
    const MemoryRegionGA& mr = static_cast<const MemoryRegionGA&>(mrb);
    TAMM_SIZE ioffset{mr.map_[proc.value()] + off.value()};
    int64_t lo = ioffset, hi = ioffset + nelements.value()-1, ld = -1;
    NGA_Get64(mr.ga_, &lo, &hi, to_buf, &ld);
  }

    /**
   * @copydoc MemoryManager::nb_get
   */
  void nb_get(MemoryRegion& mrb, Proc proc, Offset off, Size nelements, void* to_buf, DataCommunicationHandlePtr data_comm_handle) override {
    const MemoryRegionGA& mr = static_cast<const MemoryRegionGA&>(mrb);
    TAMM_SIZE ioffset{mr.map_[proc.value()] + off.value()};
    int64_t lo = ioffset, hi = ioffset + nelements.value()-1, ld = -1;
    data_comm_handle->resetCompletionStatus();
    NGA_NbGet64(mr.ga_, &lo, &hi, to_buf, &ld, data_comm_handle->getDataHandlePtr());
  }

  /**
   * @copydoc MemoryManager::put
   */
  void put(MemoryRegion& mrb, Proc proc, Offset off, Size nelements, const void* from_buf) override {
    const MemoryRegionGA& mr = static_cast<const MemoryRegionGA&>(mrb);

    TAMM_SIZE ioffset{mr.map_[proc.value()] + off.value()};
    int64_t lo = ioffset, hi = ioffset + nelements.value()-1, ld = -1;
    NGA_Put64(mr.ga_, &lo, &hi, const_cast<void*>(from_buf), &ld);
  }

  void nb_put(MemoryRegion& mrb, Proc proc, Offset off, Size nelements, const void* from_buf, DataCommunicationHandlePtr data_comm_handle) override {
    const MemoryRegionGA& mr = static_cast<const MemoryRegionGA&>(mrb);

    TAMM_SIZE ioffset{mr.map_[proc.value()] + off.value()};
    int64_t lo = ioffset, hi = ioffset + nelements.value()-1, ld = -1;
    data_comm_handle->resetCompletionStatus();
    NGA_NbPut64(mr.ga_, &lo, &hi, const_cast<void*>(from_buf), &ld, data_comm_handle->getDataHandlePtr());
  }

  /**
   * @copydoc MemoryManager::add
   */
  void add(MemoryRegion& mrb, Proc proc, Offset off, Size nelements, const void* from_buf) override {
    const MemoryRegionGA& mr = static_cast<const MemoryRegionGA&>(mrb);
    TAMM_SIZE ioffset{mr.map_[proc.value()] + off.value()};
    int64_t lo = ioffset, hi = ioffset + nelements.value()-1, ld = -1;
    void *alpha;
    switch(mr.eltype_) {
      case ElementType::single_precision:
        alpha = reinterpret_cast<void*>(&sp_alpha);
        break;
      case ElementType::double_precision:
        alpha = reinterpret_cast<void*>(&dp_alpha);
        break;
      case ElementType::single_complex:
        alpha = reinterpret_cast<void*>(&scp_alpha);
        break;
      case ElementType::double_complex:
        alpha = reinterpret_cast<void*>(&dcp_alpha);
        break;
      case ElementType::invalid:
      default:
        UNREACHABLE();
    }
    NGA_Acc64(mr.ga_, &lo, &hi, const_cast<void*>(from_buf), &ld, alpha);
  }

  /**
   * @copydoc MemoryManager::nb_add
   */
  void nb_add(MemoryRegion& mrb, Proc proc, Offset off, Size nelements, const void* from_buf, DataCommunicationHandlePtr data_comm_handle) override {
    const MemoryRegionGA& mr = static_cast<const MemoryRegionGA&>(mrb);
    TAMM_SIZE ioffset{mr.map_[proc.value()] + off.value()};
    int64_t lo = ioffset, hi = ioffset + nelements.value()-1, ld = -1;
    void *alpha;
    switch(mr.eltype_) {
      case ElementType::single_precision:
        alpha = reinterpret_cast<void*>(&sp_alpha);
        break;
      case ElementType::double_precision:
        alpha = reinterpret_cast<void*>(&dp_alpha);
        break;
      case ElementType::single_complex:
        alpha = reinterpret_cast<void*>(&scp_alpha);
        break;
      case ElementType::double_complex:
        alpha = reinterpret_cast<void*>(&dcp_alpha);
        break;
      case ElementType::invalid:
      default:
        UNREACHABLE();
    }
    data_comm_handle->resetCompletionStatus();
    NGA_NbAcc64(mr.ga_, &lo, &hi, const_cast<void*>(from_buf), &ld, alpha, data_comm_handle->getDataHandlePtr());
  }
  
  /**
   * @copydoc MemoryManager::print_coll
   */
  void print_coll(const MemoryRegion& mrb, std::ostream& os) override {
    const MemoryRegionGA& mr = static_cast<const MemoryRegionGA&>(mrb);
    GA_Print(mr.ga_);
  }

 private:
  ProcGroup pg_;       /**< Underlying ProcGroup */
  int ga_pg_;          /**< GA pgroup underlying pg_ */
  int ga_counter_ = 0; /**< GA counter to name GAs in create call */

  // constants for NGA_Acc call
  float sp_alpha = 1.0;
  double dp_alpha = 1.0;
  SingleComplex scp_alpha = {1, 0};
  DoubleComplex dcp_alpha = {1, 0};
  friend class ExecutionContext;
};  // class MemoryManagerGA

}  // namespace tamm

#endif // TAMM_MEMORY_MANAGER_GA_H_
