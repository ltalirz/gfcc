
#ifndef TAMM_TENSOR_IMPL_HPP_
#define TAMM_TENSOR_IMPL_HPP_

#include "ga/ga.h"
#include "tamm/distribution.hpp"
#include "tamm/execution_context.hpp"
#include "tamm/index_loop_nest.hpp"
#include "tamm/index_space.hpp"
#include "tamm/memory_manager_local.hpp"
#include "tamm/tensor_base.hpp"
#include "tamm/blockops_cpu.hpp"
#include <functional>
#include <gsl/span>
#include <type_traits>

namespace tamm {

using gsl::span;

/**
 * @ingroup tensors
 * @brief Implementation class for TensorBase class
 *
 * @tparam T Element type of Tensor
 */
template<typename T>
class TensorImpl : public TensorBase {
public:
    using TensorBase::TensorBase;
    // Ctors
    TensorImpl() = default;

    /**
     * @brief Construct a new TensorImpl object using a vector of
     * TiledIndexSpace objects for each mode of the tensor
     *
     * @param [in] tis vector of TiledIndexSpace objects for each
     * mode used to construct the tensor
     */
    TensorImpl(const TiledIndexSpaceVec& tis) : TensorBase{tis} {
        has_spin_symmetry_    = false;
        has_spatial_symmetry_ = false;
        kind_                 = TensorBase::TensorKind::normal;
    }

    /**
     * @brief Construct a new TensorImpl object using a vector of
     * TiledIndexSpace objects for each mode of the tensor
     *
     * @param [in] lbls vector of tiled index labels used for extracting
     * corresponding TiledIndexSpace objects for each mode used to construct
     * the tensor
     */
    TensorImpl(const std::vector<TiledIndexLabel>& lbls) : TensorBase{lbls} {
        has_spin_symmetry_    = false;
        has_spatial_symmetry_ = false;
        kind_                 = TensorBase::TensorKind::normal;
    }

    /**
     * @brief Construct a new TensorBase object recursively with a set of
     * TiledIndexSpace objects followed by a lambda expression
     *
     * @tparam Ts variadic template for rest of the arguments
     * @param [in] tis TiledIndexSpace object used as a mode
     * @param [in] rest remaining part of the arguments
     */
    template<class... Ts>
    TensorImpl(const TiledIndexSpace& tis, Ts... rest) :
      TensorBase{tis, rest...} {
        num_modes_ = block_indices_.size();
        construct_dep_map();
        has_spin_symmetry_    = false;
        has_spatial_symmetry_ = false;
        kind_                 = TensorBase::TensorKind::normal;
    }

    // SpinTensor related constructors
    /**
     * @brief Construct a new SpinTensorImpl object using set of TiledIndexSpace
     * objects and Spin attribute mask
     *
     * @param [in] t_spaces
     * @param [in] spin_mask
     */
    TensorImpl(TiledIndexSpaceVec t_spaces, SpinMask spin_mask) :
      TensorBase(t_spaces) {
        EXPECTS(t_spaces.size() == spin_mask.size());

        // for(const auto& tis : t_spaces) { EXPECTS(tis.has_spin()); }

        spin_mask_            = spin_mask;
        has_spin_symmetry_    = true;
        has_spatial_symmetry_ = false;
        // spin_total_        = calculate_spin();
        kind_ = TensorBase::TensorKind::spin;
    }

    /**
     * @brief Construct a new SpinTensorImpl object using set of TiledIndexLabel
     * objects and Spin attribute mask
     *
     * @param [in] t_labels
     * @param [in] spin_mask
     */
    TensorImpl(IndexLabelVec t_labels, SpinMask spin_mask) :
      TensorBase(t_labels) {
        EXPECTS(t_labels.size() == spin_mask.size());
        // for(const auto& tlbl : t_labels) {
        //     EXPECTS(tlbl.tiled_index_space().has_spin());
        // }
        spin_mask_            = spin_mask;
        has_spin_symmetry_    = true;
        has_spatial_symmetry_ = false;
        // spin_total_        = calculate_spin();
        kind_ = TensorBase::TensorKind::spin;
    }

    /**
     * @brief Construct a new SpinTensorImpl object using set of TiledIndexSpace
     * objects and Spin attribute mask
     *
     * @param [in] t_spaces
     * @param [in] spin_mask
     */
    TensorImpl(TiledIndexSpaceVec t_spaces, std::vector<size_t> spin_sizes) :
      TensorBase(t_spaces) {
        // EXPECTS(t_spaces.size() == spin_mask.size());
        EXPECTS(spin_sizes.size() > 0);
        // for(const auto& tis : t_spaces) { EXPECTS(tis.has_spin()); }
        SpinMask spin_mask;
        size_t upper = spin_sizes[0];
        size_t lower =
          spin_sizes.size() > 1 ? spin_sizes[1] : t_spaces.size() - upper;
        size_t ignore = spin_sizes.size() > 2 ?
                          spin_sizes[1] :
                          t_spaces.size() - (upper + lower);

        for(size_t i = 0; i < upper; i++) {
            spin_mask.push_back(SpinPosition::upper);
        }

        for(size_t i = 0; i < lower; i++) {
            spin_mask.push_back(SpinPosition::lower);
        }

        for(size_t i = 0; i < upper; i++) {
            spin_mask.push_back(SpinPosition::ignore);
        }

        spin_mask_            = spin_mask;
        has_spin_symmetry_    = true;
        has_spatial_symmetry_ = false;
        // spin_total_        = calculate_spin();
        kind_ = TensorBase::TensorKind::spin;
    }

    /**
     * @brief Construct a new SpinTensorImpl object using set of TiledIndexLabel
     * objects and Spin attribute mask
     *
     * @param [in] t_labels
     * @param [in] spin_mask
     */
    TensorImpl(IndexLabelVec t_labels, std::vector<size_t> spin_sizes) :
      TensorBase(t_labels) {
        // EXPECTS(t_labels.size() == spin_mask.size());
        EXPECTS(spin_sizes.size() > 0);
        // for(const auto& tlbl : t_labels) {
        //     EXPECTS(tlbl.tiled_index_space().has_spin());
        // }

        SpinMask spin_mask;
        size_t upper = spin_sizes[0];
        size_t lower =
          spin_sizes.size() > 1 ? spin_sizes[1] : t_labels.size() - upper;
        size_t ignore = spin_sizes.size() > 2 ?
                          spin_sizes[1] :
                          t_labels.size() - (upper + lower);

        for(size_t i = 0; i < upper; i++) {
            spin_mask.push_back(SpinPosition::upper);
        }

        for(size_t i = 0; i < lower; i++) {
            spin_mask.push_back(SpinPosition::lower);
        }

        for(size_t i = 0; i < upper; i++) {
            spin_mask.push_back(SpinPosition::ignore);
        }

        spin_mask_            = spin_mask;
        has_spin_symmetry_    = true;
        has_spatial_symmetry_ = false;
        // spin_total_        = calculate_spin();
        kind_ = TensorBase::TensorKind::spin;
    }

    // Copy/Move Ctors and Assignment Operators
    TensorImpl(TensorImpl&&)      = default;
    TensorImpl(const TensorImpl&) = delete;
    TensorImpl& operator=(TensorImpl&&) = default;
    TensorImpl& operator=(const TensorImpl&) = delete;

    // Dtor
    ~TensorImpl() {
        if(mpb_ != nullptr) {
            mpb_->allocation_status_ = AllocationStatus::orphaned;
        }
    }

    /**
     * @brief Virtual method for deallocating a Tensor
     *
     */
    virtual void deallocate() {
        EXPECTS(allocation_status_ == AllocationStatus::created);
        EXPECTS(mpb_);
        ec_->unregister_for_dealloc(mpb_);
        mpb_->dealloc_coll();
        delete mpb_;
        mpb_ = nullptr;
        update_status(AllocationStatus::deallocated);
    }

    /**
     * @brief Virtual method for allocating a Tensor using an ExecutionContext
     *
     * @param [in] ec ExecutionContext to be used for allocation
     */
    virtual void allocate(ExecutionContext* ec) {
        {
            TimerGuard tg_total{&memTime1};
            EXPECTS(allocation_status_ == AllocationStatus::invalid);
            auto defd                  = ec->get_default_distribution();
            Distribution* distribution = ec->distribution(
              defd->get_tensor_base(), defd->get_dist_proc()); // defd->kind());
            // Distribution* distribution    =
            // ec->distribution(defd.tensor_base(), nproc );
            MemoryManager* memory_manager = ec->memory_manager();
            EXPECTS(distribution != nullptr);
            EXPECTS(memory_manager != nullptr);
            ec_ = ec;
            // distribution_ =
            // DistributionFactory::make_distribution(*distribution, this,
            // pg.size());
            {
                TimerGuard tg_total{&memTime2};
                distribution_ = std::shared_ptr<Distribution>(
                  distribution->clone(this, memory_manager->pg().size()));
            }
#if 0
        auto rank = memory_manager->pg().rank();
        auto buf_size = distribution_->buf_size(rank);
        auto eltype = tensor_element_type<T>();
        EXPECTS(buf_size >= 0);
        mpb_ = memory_manager->alloc_coll(eltype, buf_size);
#else
            auto eltype = tensor_element_type<T>();
            mpb_        = memory_manager->alloc_coll_balanced(
              eltype, distribution_->max_proc_buf_size());
#endif
            EXPECTS(mpb_ != nullptr);
            ec_->register_for_dealloc(mpb_);
            update_status(AllocationStatus::created);
        }
    }

    virtual const Distribution &distribution() const {
      return *distribution_.get();
    }

    // Tensor Accessors
    /**
     * @brief Tensor accessor method for getting values from a set of
     * indices to specified memory span
     *
     * @tparam T type of the values hold on the tensor object
     * @param [in] idx_vec a vector of indices to fetch the values
     * @param [in] buff_span memory span where to put the fetched values
     */

    virtual void get(const IndexVector& idx_vec, span<T> buff_span) const {
        EXPECTS(allocation_status_ != AllocationStatus::invalid);

        if(!is_non_zero(idx_vec)) {
            Size size = block_size(idx_vec);
            EXPECTS(size <= buff_span.size());
            for(size_t i = 0; i < size; i++) { buff_span[i] = (T)0; }
            return;
        }

        Proc proc;
        Offset offset;
        std::tie(proc, offset) = distribution_->locate(idx_vec);
        Size size              = block_size(idx_vec);
        EXPECTS(size <= buff_span.size());
        mpb_->mgr().get(*mpb_, proc, offset, Size{size}, buff_span.data());
    }

    /**
     * @brief Tensor accessor method for getting values in nonblocking fashion
     * from a set of indices to specified memory span
     *
     * @tparam T type of the values hold on the tensor object
     * @param [in] idx_vec a vector of indices to fetch the values
     * @param [in] buff_span memory span where to put the fetched values
     */

    virtual void nb_get(const IndexVector& idx_vec, span<T> buff_span,
                        DataCommunicationHandlePtr data_comm_handle) const {
        EXPECTS(allocation_status_ != AllocationStatus::invalid);

        if(!is_non_zero(idx_vec)) {
            Size size = block_size(idx_vec);
            EXPECTS(size <= buff_span.size());
            for(size_t i = 0; i < size; i++) { buff_span[i] = (T)0; }
            return;
        }

        Proc proc;
        Offset offset;
        std::tie(proc, offset) = distribution_->locate(idx_vec);
        Size size              = block_size(idx_vec);
        EXPECTS(size <= buff_span.size());
        mpb_->mgr().nb_get(*mpb_, proc, offset, Size{size}, buff_span.data(),
                           data_comm_handle);
    }

    /**
     * @brief Tensor accessor method for putting values to a set of indices
     * with the specified memory span
     *
     * @tparam T type of the values hold on the tensor object
     * @param [in] idx_vec a vector of indices to put the values
     * @param [in] buff_span buff_span memory span for the values to put
     */

    virtual void put(const IndexVector& idx_vec, span<T> buff_span) {
        EXPECTS(allocation_status_ != AllocationStatus::invalid);

        if(!is_non_zero(idx_vec)) { return; }

        Proc proc;
        Offset offset;
        std::tie(proc, offset) = distribution_->locate(idx_vec);
        Size size              = block_size(idx_vec);
        EXPECTS(size <= buff_span.size());
        mpb_->mgr().put(*mpb_, proc, offset, Size{size}, buff_span.data());
    }

    /**
     * @brief Tensor accessor method for putting values in nonblocking fashion
     * to a set of indices with the specified memory span
     *
     * @tparam T type of the values hold on the tensor object
     * @param [in] idx_vec a vector of indices to put the values
     * @param [in] buff_span buff_span memory span for the values to put
     */

    virtual void nb_put(const IndexVector& idx_vec, span<T> buff_span,
                        DataCommunicationHandlePtr data_comm_handle) {
        EXPECTS(allocation_status_ != AllocationStatus::invalid);

        if(!is_non_zero(idx_vec)) { return; }

        Proc proc;
        Offset offset;
        std::tie(proc, offset) = distribution_->locate(idx_vec);
        Size size              = block_size(idx_vec);
        EXPECTS(size <= buff_span.size());
        mpb_->mgr().nb_put(*mpb_, proc, offset, Size{size}, buff_span.data(),
                           data_comm_handle);
    }

    /**
     * @brief Tensor accessor method for adding svalues to a set of indices
     * with the specified memory span
     *
     * @tparam T type of the values hold on the tensor object
     * @param [in] idx_vec a vector of indices to put the values
     * @param [in] buff_span buff_span memory span for the values to put
     */

    virtual void add(const IndexVector& idx_vec, span<T> buff_span) {
        EXPECTS(allocation_status_ != AllocationStatus::invalid);

        if(!is_non_zero(idx_vec)) { return; }

        Proc proc;
        Offset offset;
        std::tie(proc, offset) = distribution_->locate(idx_vec);
        Size size              = block_size(idx_vec);
        EXPECTS(size <= buff_span.size());
        mpb_->mgr().add(*mpb_, proc, offset, Size{size}, buff_span.data());
    }

    /**
     * @brief Tensor accessor method for adding svalues in nonblocking fashion
     * to a set of indices with the specified memory span
     *
     * @tparam T type of the values hold on the tensor object
     * @param [in] idx_vec a vector of indices to put the values
     * @param [in] buff_span buff_span memory span for the values to put
     */

    virtual void nb_add(const IndexVector& idx_vec, span<T> buff_span,
                        DataCommunicationHandlePtr data_comm_handle) {
        EXPECTS(allocation_status_ != AllocationStatus::invalid);

        if(!is_non_zero(idx_vec)) { return; }

        Proc proc;
        Offset offset;
        std::tie(proc, offset) = distribution_->locate(idx_vec);
        Size size              = block_size(idx_vec);
        EXPECTS(size <= buff_span.size());
        mpb_->mgr().nb_add(*mpb_, proc, offset, Size{size}, buff_span.data(),
                           data_comm_handle);
    }

    /**
     * @brief Virtual method for getting the diagonal values in a Tensor
     *
     * @returns a vector with the diagonal values
     * @warning available for tensors with 2 modes
     */
    // virtual std::vector<T> diagonal() {
    //     EXPECTS(num_modes() == 2);
    //     std::vector<T> dest;
    //     for(const IndexVector& blockid : loop_nest()) {
    //         if(blockid[0] == blockid[1]) {
    //             const TAMM_SIZE size = block_size(blockid);
    //             std::vector<T> buf(size);
    //             get(blockid, buf);
    //             auto block_dims1  = block_dims(blockid);
    //             auto block_offset = block_offsets(blockid);
    //             auto dim          = block_dims1[0];
    //             auto offset       = block_offset[0];
    //             size_t i          = 0;
    //             for(auto p = offset; p < offset + dim; p++, i++) {
    //                 dest.push_back(buf[i * dim + i]);
    //             }
    //         }
    //     }
    //     return dest;
    // }

    virtual int ga_handle() {
        const MemoryRegionGA& mr = static_cast<const MemoryRegionGA&>(*mpb_);
        return mr.ga();
    }

    virtual T* access_local_buf() {
        EXPECTS(mpb_);
        return static_cast<T*>(mpb_->mgr().access(*mpb_, Offset{0}));
    }

    virtual const T* access_local_buf() const {
        EXPECTS(mpb_);
        return static_cast<T*>(mpb_->mgr().access(*mpb_, Offset{0}));
    }

    virtual size_t local_buf_size() const {
        EXPECTS(mpb_);
        return mpb_->local_nelements().value();
    }

    virtual size_t total_buf_size(Proc proc) const {
        EXPECTS(distribution_);
        return distribution_->buf_size(proc).value();
    }

    MemoryRegion* memory_region() const {
        EXPECTS(mpb_);
        return mpb_;
    }

    virtual int64_t size() const {
      EXPECTS(distribution_ != nullptr);
      return distribution_->total_size().value();
    }

    bool is_allocated() const {
      return (allocation_status_== AllocationStatus::created);
    }
protected:
    std::shared_ptr<Distribution>
      distribution_; /**< shared pointer to associated Distribution */
    MemoryRegion* mpb_ =
      nullptr; /**< Raw pointer memory region (default null) */

}; // TensorImpl

/**
 * @ingroup tensors
 * @brief Implementation class for TensorBase with Lambda function construction
 *
 * @tparam T Element type of Tensor
 */
template<typename T>
class LambdaTensorImpl : public TensorImpl<T> {
public:
    using TensorBase::setKind;
    using TensorBase::block_indices_;

    /// @brief Function signature for the Lambda method
    using Func = std::function<void(const IndexVector&, span<T>)>;
    // Ctors
    LambdaTensorImpl() = default;

    // Copy/Move Ctors and Assignment Operators
    LambdaTensorImpl(LambdaTensorImpl&&)      = default;
    LambdaTensorImpl(const LambdaTensorImpl&) = delete;
    LambdaTensorImpl& operator=(LambdaTensorImpl&&) = default;
    LambdaTensorImpl& operator=(const LambdaTensorImpl&) = delete;

    /**
     * @brief Construct a new LambdaTensorImpl object using a Lambda function
     *
     * @param [in] tis_vec vector of TiledIndexSpace objects for each mode of
     * the Tensor
     * @param [in] lambda a function for constructing the Tensor
     */
    LambdaTensorImpl(const TiledIndexSpaceVec& tis_vec, Func lambda) :
      TensorImpl<T>(tis_vec), lambda_{lambda} {
        setKind(TensorBase::TensorKind::lambda);
    }

    LambdaTensorImpl(const IndexLabelVec& til_vec, Func lambda) :
      TensorImpl<T>(til_vec), lambda_{lambda} {
        setKind(TensorBase::TensorKind::lambda);
    }

    LambdaTensorImpl(const IndexLabelVec &til_vec, Tensor<T> ref_tensor,
                     const IndexLabelVec &ref_labels,
                     const IndexLabelVec &use_labels,
                     const std::vector<TranslateFunc> &translate_func_vec)
        : TensorImpl<T>(til_vec), lambda_{construct_lambda(
                                      til_vec, ref_tensor, ref_labels,
                                      use_labels, translate_func_vec)} {}

    // Dtor
    ~LambdaTensorImpl() = default;

    void deallocate() override {
        // NO_OP();
    }

    void allocate(ExecutionContext* ec) override {
        // NO_OP();
    }

    void get(const IndexVector& idx_vec, span<T> buff_span) const override {
        lambda_(idx_vec, buff_span);
    }

    void put(const IndexVector& idx_vec, span<T> buff_span) override {
        NOT_ALLOWED();
    }

    void add(const IndexVector& idx_vec, span<T> buff_span) override {
        NOT_ALLOWED();
    }

    T* access_local_buf() override { NOT_ALLOWED(); }

    const T* access_local_buf() const override { NOT_ALLOWED(); }

    size_t local_buf_size() const override { NOT_ALLOWED(); }

    int64_t size() const override {

      int64_t res = 1;
      for (const auto &tis : block_indices_) {
        res *= tis.max_num_indices();
      }
      return res;
    }

protected:
    Func lambda_; /**< Lambda function for the Tensor */

    Func
    construct_lambda(const IndexLabelVec &til_vec, Tensor<T> ref_tensor,
                     const IndexLabelVec &ref_labels,
                     const IndexLabelVec &use_labels,
                     const std::vector<TranslateFunc> &translate_func_vec) {
      auto ip_gen_loop_builder_ptr =
          std::make_shared<blockops::cpu::IpGenLoopBuilder<2>>(
              std::array<IndexLabelVec, 2>{use_labels, ref_labels});

      auto perm_dup_map_compute = [](const IndexLabelVec &from,
                                     const IndexLabelVec &to) {
        std::vector<int> ret(from.size(), -1);
        for (size_t i = 0; i < from.size(); i++) {
          auto it = std::find(to.begin(), to.end(), from[i]);
          if (it != to.end()) {
            ret[i] = it - to.begin();
          }
        }
        return ret;
      };

      auto perm_map = perm_dup_map_compute(use_labels, ref_labels);
      EXPECTS(perm_map.size() == use_labels.size());

      auto lambda = [til_vec, ref_tensor, ip_gen_loop_builder_ptr,
                     translate_func_vec,
                     perm_map](const IndexVector &blockid, span<T> buff) {
        EXPECTS(blockid.size() == til_vec.size());
        EXPECTS(translate_func_vec.size() == blockid.size());

        IndexVector ref_blockid;
        for (size_t i = 0; i < blockid.size(); i++) {
          auto translated_id = translate_func_vec[i](blockid[i]);
          if(translated_id != -1)
            ref_blockid.push_back(translated_id);
        }

        auto ref_buff_size = ref_tensor.block_size(ref_blockid);
        auto ref_block_dims = ref_tensor.block_dims(ref_blockid);
        std::vector<T> ref_buf(ref_buff_size);
        BlockSpan ref_span(ref_buf.data(), ref_block_dims);

        std::vector<size_t> use_block_dims;
        for (size_t i = 0; i < perm_map.size(); i++) {
          use_block_dims.push_back(ref_block_dims[perm_map[i]]);
        }

        BlockSpan lhs_span(buff.data(), use_block_dims);
        EXPECTS(lhs_span.num_elements() == buff.size());

        ref_tensor.get(ref_blockid, ref_buf);

        blockops::cpu::flat_set(lhs_span, 0);

        ip_gen_loop_builder_ptr->update_plan(lhs_span, ref_span);
        blockops::cpu::ipgen_loop_assign(
            lhs_span.buf(), ip_gen_loop_builder_ptr->u2ald()[0], ref_span.buf(),
            ip_gen_loop_builder_ptr->u2ald()[1],
            ip_gen_loop_builder_ptr->unique_label_dims());
      };

      return lambda;
    }
};                // class LambdaTensorImpl

/**
 * @ingroup tensors
 * @brief Implementation class for TensorBase with dense multidimensional GA
 *
 * @tparam T Element type of Tensor
 */
template<typename T>
class DenseTensorImpl : public TensorImpl<T> {
public:
    using TensorImpl<T>::TenorBase::setKind;
    using TensorImpl<T>::TensorBase::ec_;
    using TensorImpl<T>::TensorBase::allocation_status_;

    using TensorImpl<T>::block_size;
    using TensorImpl<T>::block_dims;
    using TensorImpl<T>::block_offsets;
    using TensorImpl<T>::TensorBase::tindices;
    using TensorImpl<T>::TensorBase::num_modes;
    using TensorImpl<T>::TensorBase::update_status;

    // Ctors
    DenseTensorImpl() = default;

    // Copy/Move Ctors and Assignment Operators
    DenseTensorImpl(DenseTensorImpl&&)      = default;
    DenseTensorImpl(const DenseTensorImpl&) = delete;
    DenseTensorImpl& operator=(DenseTensorImpl&&) = default;
    DenseTensorImpl& operator=(const DenseTensorImpl&) = delete;

    /**
     * @brief Construct a new LambdaTensorImpl object using a Lambda function
     *
     * @param [in] tis_vec vector of TiledIndexSpace objects for each mode of
     * the Tensor
     * @param [in] lambda a function for constructing the Tensor
     */
    DenseTensorImpl(const TiledIndexSpaceVec& tis_vec, const ProcGrid proc_grid,
                    const bool scalapack_distribution = false) :
      TensorImpl<T>(tis_vec),
      proc_grid_{proc_grid},
      scalapack_distribution_{scalapack_distribution} {
        // check no dependences
        // for(auto& tis : tis_vec) { EXPECTS(!tis.is_dependent()); }

        // check index spaces are dense
        setKind(TensorBase::TensorKind::dense);
    }

    DenseTensorImpl(const IndexLabelVec& til_vec, const ProcGrid proc_grid,
                    const bool scalapack_distribution = false) :
      TensorImpl<T>(til_vec),
      proc_grid_{proc_grid},
      scalapack_distribution_{scalapack_distribution} {
        // check no dependences
        // check index spaces are dense
        // for(auto& til : til_vec) { EXPECTS(!til.is_dependent()); }
        setKind(TensorBase::TensorKind::dense);
    }

    // Dtor
    ~DenseTensorImpl() {
        // EXPECTS(allocation_status_ == AllocationStatus::deallocated ||
        //         allocation_status_ == AllocationStatus::invalid);
    }

    void deallocate() {
        NGA_Destroy(ga_);
        update_status(AllocationStatus::deallocated);
    }

    void allocate(ExecutionContext* ec) {
        EXPECTS(allocation_status_ == AllocationStatus::invalid);

        ec_             = ec;
        ga_             = NGA_Create_handle();
        const int ndims = num_modes();

        auto tis_dims = tindices();
        std::vector<int64_t> dims;
        for(auto tis : tis_dims)
            dims.push_back(tis.index_space().num_indices());

        NGA_Set_data64(ga_, ndims, &dims[0], ga_eltype_);

        const bool is_irreg_tis1 = !tis_dims[0].input_tile_sizes().empty();
        const bool is_irreg_tis2 = !tis_dims[1].input_tile_sizes().empty();
        // std::cout << "#dims 0,1 = " << dims[0] << "," << dims[1] <<
        // std::endl;

        if(scalapack_distribution_) {
            // EXPECTS(ndims == 2);
            std::vector<int64_t> bsize(2);
            std::vector<int64_t> pgrid(2);
            {
                // TODO: grid_factor doesnt work
                if(proc_grid_.empty()) {
                    int idx, idy;
                    grid_factor((*ec).pg().size().value(), &idx, &idy);
                    proc_grid_.push_back(idx);
                    proc_grid_.push_back(idy);
                }

                // Cannot provide list of irreg tile sizes
                EXPECTS(!is_irreg_tis1 && !is_irreg_tis2);

                bsize[0] = tis_dims[0].input_tile_size();
                bsize[1] = tis_dims[1].input_tile_size();

                pgrid[0] = proc_grid_[0].value();
                pgrid[1] = proc_grid_[1].value();
            }
            // blocks_ = block sizes for scalapack distribution
            GA_Set_block_cyclic_proc_grid64(ga_, &bsize[0], &pgrid[0]);
        } else {
            // only needed when irreg tile sizes are provided
            if(is_irreg_tis1 || is_irreg_tis2) {
                std::vector<Tile> tiles1 =
                  is_irreg_tis1 ?
                    tis_dims[0].input_tile_sizes() :
                    std::vector<Tile>{tis_dims[0].input_tile_size()};
                std::vector<Tile> tiles2 =
                  is_irreg_tis2 ?
                    tis_dims[1].input_tile_sizes() :
                    std::vector<Tile>{tis_dims[1].input_tile_size()};

                int64_t size_map;
                int64_t nblock[ndims];

                int nranks = (*ec).pg().size().value();
                int ranks_list[nranks];
                for(int i = 0; i < nranks; i++) ranks_list[i] = i;

                int idx, idy;
                grid_factor((*ec).pg().size().value(), &idx, &idy);

                nblock[0] = is_irreg_tis1 ?
                              tiles1.size() :
                              std::ceil(dims[0] * 1.0 / tiles1[0]);
                nblock[1] = is_irreg_tis2 ?
                              tiles2.size() :
                              std::ceil(dims[1] * 1.0 / tiles2[0]);

                // int max_t1 = is_irreg_tis1? *max_element(tiles1.begin(),
                // tiles1.end()) : tiles1[0]; int max_t2 = is_irreg_tis2?
                // *max_element(tiles2.begin(), tiles2.end()) : tiles2[0]; int
                // new_t1 = std::ceil(nblock[0]/idx)*max_t1; int new_t2 =
                // std::ceil(nblock[1]/idy)*max_t2;
                nblock[0] = (int64_t)idx;
                nblock[1] = (int64_t)idy;

                size_map = nblock[0] + nblock[1];

                // std::cout << "#blocks 0,1 = " << nblock[0] << "," <<
                // nblock[1] << std::endl;

                // create map
                std::vector<int64_t> k_map(size_map);
                {
                    auto mi = 0;
                    for(auto idim = 0; idim < ndims; idim++) {
                        auto size_blk =
                          std::ceil(1.0 * dims[idim] / nblock[idim]);
                        // regular tile size
                        for(auto i = 0; i < nblock[idim]; i++) {
                            k_map[mi] = size_blk * i;
                            mi++;
                        }
                    }
                    // k_map[mi] = 0;
                }
                GA_Set_irreg_distr64(ga_, &k_map[0], nblock);
            } else {
                // fixed tilesize for both dims
                int64_t chunk[2] = {tis_dims[0].input_tile_size(),
                                    tis_dims[1].input_tile_size()};
                GA_Set_chunk64(ga_, chunk);
            }
        }
        GA_Set_pgroup(ga_, ec->pg().ga_pg());
        NGA_Allocate(ga_);
        update_status(AllocationStatus::created);
    }

    void get(const IndexVector& blockid, span<T> buff_span) const {
        std::vector<int64_t> lo = compute_lo(blockid);
        std::vector<int64_t> hi = compute_hi(blockid);
        std::vector<int64_t> ld = compute_ld(blockid);
        EXPECTS(block_size(blockid) <= buff_span.size());
        NGA_Get64(ga_, &lo[0], &hi[0], buff_span.data(), &ld[0]);
    }

    void put(const IndexVector& blockid, span<T> buff_span) {
        std::vector<int64_t> lo = compute_lo(blockid);
        std::vector<int64_t> hi = compute_hi(blockid);
        std::vector<int64_t> ld = compute_ld(blockid);

        EXPECTS(block_size(blockid) <= buff_span.size());
        NGA_Put64(ga_, &lo[0], &hi[0], buff_span.data(), &ld[0]);
    }

    void add(const IndexVector& blockid, span<T> buff_span) {
        std::vector<int64_t> lo = compute_lo(blockid);
        std::vector<int64_t> hi = compute_hi(blockid);
        std::vector<int64_t> ld = compute_ld(blockid);
        EXPECTS(block_size(blockid) <= buff_span.size());
        void* alpha;
        switch(from_ga_eltype(ga_eltype_)) {
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
            default: UNREACHABLE();
        }
        NGA_Acc64(ga_, &lo[0], &hi[0],
                  reinterpret_cast<void*>(buff_span.data()), &ld[0], alpha);
    }

    int ga_handle() { return ga_; }

    /// @todo Should this be GA_Nodeid() or GA_Proup_nodeid(GA_Get_pgroup(ga_))
    T* access_local_buf() override {
        T* ptr;
        int64_t len;
        NGA_Access_block_segment64(ga_, GA_Pgroup_nodeid(GA_Get_pgroup(ga_)),
                                   reinterpret_cast<void*>(&ptr), &len);
        return ptr;
    }

    /// @todo Should this be GA_Nodeid() or GA_Proup_nodeid(GA_Get_pgroup(ga_))
    const T* access_local_buf() const override {
        T* ptr;
        int64_t len;
        NGA_Access_block_segment64(ga_, GA_Pgroup_nodeid(GA_Get_pgroup(ga_)),
                                   reinterpret_cast<void*>(&ptr), &len);
        return ptr;
    }

    /// @todo Check for a GA method to get the local buf size?
    size_t local_buf_size() const override {
        T* ptr;
        int64_t len;
        NGA_Access_block_segment64(ga_, GA_Pgroup_nodeid(GA_Get_pgroup(ga_)),
                                   reinterpret_cast<void*>(&ptr), &len);
        size_t res = (size_t)len;
        return res;
    }

    /// @todo implement accordingly
    int64_t size() const override {
        NOT_IMPLEMENTED();
    }

protected:
    std::vector<int64_t> compute_lo(const IndexVector& blockid) const {
        std::vector<int64_t> retv;
        std::vector<size_t> off = block_offsets(blockid);
        for(const auto& i : off) { retv.push_back(static_cast<int64_t>(i)); }
        return retv;
    }

    std::vector<int64_t> compute_hi(const IndexVector& blockid) const {
        std::vector<int64_t> retv;
        std::vector<size_t> boff  = block_offsets(blockid);
        std::vector<size_t> bdims = block_dims(blockid);
        for(size_t i = 0; i < boff.size(); i++) {
            retv.push_back(static_cast<int64_t>(boff[i] + bdims[i] - 1));
        }
        return retv;
    }

    std::vector<int64_t> compute_ld(const IndexVector& blockid) const {
        std::vector<size_t> bdims = block_dims(blockid);
        std::vector<int64_t> retv(bdims.size() - 1, 1);
        size_t ri = 0;
        for(size_t i = bdims.size() - 1; i > 0; i--) {
            retv[ri] = (int64_t)bdims[i];
            ri++;
        }
        return retv;
    }

    int ga_;
    ProcGrid proc_grid_;
    // true only when a ProcGrid is explicity passed to Tensor constructor
    bool scalapack_distribution_ = false;

    // constants for NGA_Acc call
    float sp_alpha          = 1.0;
    double dp_alpha         = 1.0;
    SingleComplex scp_alpha = {1, 0};
    DoubleComplex dcp_alpha = {1, 0};

    int ga_eltype_ = to_ga_eltype(tensor_element_type<T>());

    /**
     * Factor p processors into 2D processor grid of dimensions px, py
     */
    void grid_factor(int p, int* idx, int* idy) {
        int i, j;
        const int MAX_FACTOR = 512;
        int ip, ifac, pmax, prime[MAX_FACTOR];
        int fac[MAX_FACTOR];
        int ix, iy, ichk;

        i = 1;
        /**
         *   factor p completely
         *   first, find all prime numbers, besides 1, less than or equal to
         *   the square root of p
         */
        ip   = (int)(sqrt((double)p)) + 1;
        pmax = 0;
        for(i = 2; i <= ip; i++) {
            ichk = 1;
            for(j = 0; j < pmax; j++) {
                if(i % prime[j] == 0) {
                    ichk = 0;
                    break;
                }
            }
            if(ichk) {
                pmax = pmax + 1;
                if(pmax > MAX_FACTOR) printf("Overflow in grid_factor\n");
                prime[pmax - 1] = i;
            }
        }
        /**
         *   find all prime factors of p
         */
        ip   = p;
        ifac = 0;
        for(i = 0; i < pmax; i++) {
            while(ip % prime[i] == 0) {
                ifac          = ifac + 1;
                fac[ifac - 1] = prime[i];
                ip            = ip / prime[i];
            }
        }
        /**
         *  p is prime
         */
        if(ifac == 0) {
            ifac++;
            fac[0] = p;
        }
        /**
         *    find two factors of p of approximately the
         *    same size
         */
        *idx = 1;
        *idy = 1;
        for(i = ifac - 1; i >= 0; i--) {
            ix = *idx;
            iy = *idy;
            if(ix <= iy) {
                *idx = fac[i] * (*idx);
            } else {
                *idy = fac[i] * (*idy);
            }
        }
    }

}; // class DenseTensorImpl

template <typename T> class ViewTensorImpl : public TensorImpl<T> {
public:
  using TensorImpl<T>::distribution_;
  using TensorImpl<T>::TensorBase::update_status;
  using TensorImpl<T>::TensorBase::setKind;
  using TensorImpl<T>::TensorBase::block_size;
  using TensorImpl<T>::TensorBase::block_dims;
  using TensorImpl<T>::TensorBase::ec_;
  using Func = std::function<IndexVector(const IndexVector&)>;
  using CopyFunc = std::function<void(const BlockSpan<T>&, BlockSpan<T>&, const IndexVector&)>;

  // Ctors
  ViewTensorImpl() = default;
  ViewTensorImpl(Tensor<T> ref_tensor, const TiledIndexSpaceVec &tis_vec,
                 Func ref_map_func)
      : TensorImpl<T>(tis_vec), ref_tensor_{ref_tensor}, ref_map_func_{
                                                             ref_map_func} {
    setKind(TensorBase::TensorKind::view);
    if (ref_tensor_.is_allocated()) {
      distribution_ = std::make_shared<ViewDistribution>(
          &ref_tensor_.distribution(), ref_map_func_);
      update_status(AllocationStatus::created);
    }
    EXPECTS(ref_tensor_.is_allocated());
    ec_ = ref_tensor.execution_context();
  }

  ViewTensorImpl(Tensor<T> ref_tensor, const IndexLabelVec &labels,
                 Func ref_map_func)
      : TensorImpl<T>(labels), ref_tensor_{ref_tensor}, ref_map_func_{
                                                            ref_map_func} {
    setKind(TensorBase::TensorKind::view);

    if (ref_tensor_.is_allocated()) {
      distribution_ = std::make_shared<ViewDistribution>(
          &ref_tensor_.distribution(), ref_map_func_);
      update_status(AllocationStatus::created);
    }
    EXPECTS(ref_tensor_.is_allocated());
    ec_ = ref_tensor.execution_context();

  }

  ViewTensorImpl(Tensor<T> ref_tensor, const IndexLabelVec &labels,
                 Func ref_map_func, CopyFunc get_func, CopyFunc put_func)
      : TensorImpl<T>(labels), ref_tensor_{ref_tensor},
        ref_map_func_{ref_map_func}, get_func_{get_func}, put_func_{put_func} {
    setKind(TensorBase::TensorKind::view);

    if (ref_tensor_.is_allocated()) {
      distribution_ = std::make_shared<ViewDistribution>(
          &ref_tensor_.distribution(), ref_map_func_);
      update_status(AllocationStatus::created);
    }
    EXPECTS(ref_tensor_.is_allocated());
    ec_ = ref_tensor.execution_context();
  }

  // Copy/Move Ctors and Assignment Operators
  ViewTensorImpl(ViewTensorImpl &&) = default;
  ViewTensorImpl(const ViewTensorImpl &) = delete;
  ViewTensorImpl &operator=(ViewTensorImpl &&) = default;
  ViewTensorImpl &operator=(const ViewTensorImpl &) = delete;

  // Dtor
  ~ViewTensorImpl() = default;

  /**
   * @brief Virtual method for deallocating a Tensor
   *
   */
  void deallocate() override {
    // No-op
  }

  /**
   * @brief Virtual method for allocating a Tensor using an ExecutionContext
   *
   * @param [in] ec ExecutionContext to be used for allocation
   */
  void allocate(ExecutionContext *ec) override {
    // No-op
    // EXPECTS(ref_tensor_.is_allocated());
    // distribution_ = std::make_shared<ViewDistribution>(
    //     &ref_tensor_.distribution(), ref_map_func_);
    // update_status(AllocationStatus::created);
  }

  const Distribution &distribution() const override {
    // return ref_tensor_.distribution();
    return *distribution_.get();
  }

  // Tensor Accessors
  /**
   * @brief Tensor accessor method for getting values from a set of
   * indices to specified memory span
   *
   * @tparam T type of the values hold on the tensor object
   * @param [in] idx_vec a vector of indices to fetch the values
   * @param [in] buff_span memory span where to put the fetched values
   */

  void get(const IndexVector &view_vec, span<T> buff_span) const override {
    // Call Reference tensors get
    const auto &idx_vec = ref_map_func_(view_vec);
    auto ref_buf_size = ref_tensor_.block_size(idx_vec);
    auto ref_blck_dims = ref_tensor_.block_dims(idx_vec);
    std::vector<T> ref_vec(ref_buf_size, 0);
    const BlockSpan ref_span{ref_vec.data(), ref_blck_dims};
    // @todo we might not need to do a get depending on if the tile has non-zero values
    ref_tensor_.get(idx_vec, ref_vec);

    auto blck_dims = block_dims(view_vec);
    BlockSpan blck_span{buff_span.data(), blck_dims};

    get_func_(ref_span, blck_span, view_vec);
  }

  /**
   * @brief Tensor accessor method for getting values in nonblocking fashion
   * from a set of indices to specified memory span
   *
   * @tparam T type of the values hold on the tensor object
   * @param [in] idx_vec a vector of indices to fetch the values
   * @param [in] buff_span memory span where to put the fetched values
   *
   * @bug should fix handle return
   */

  void nb_get(const IndexVector &view_vec, span<T> buff_span,
              DataCommunicationHandlePtr data_comm_handle) const override {
    // Call Reference tensors nb_get
    const auto &idx_vec = ref_map_func_(view_vec);
    auto ref_buf_size = ref_tensor_.block_size(idx_vec);
    auto ref_blck_dims = ref_tensor_.block_dims(idx_vec);
    std::vector<T> ref_vec(ref_buf_size, 0);
    const BlockSpan ref_span{ref_vec.data(), ref_blck_dims};

    ref_tensor_.nb_get(idx_vec, ref_vec, data_comm_handle);

    auto blck_dims = block_dims(view_vec);
    BlockSpan blck_span{buff_span.data(), blck_dims};

    get_func_(ref_span, blck_span, view_vec);
  }

  /**
   * @brief Tensor accessor method for putting values to a set of indices
   * with the specified memory span
   *
   * @tparam T type of the values hold on the tensor object
   * @param [in] idx_vec a vector of indices to put the values
   * @param [in] buff_span buff_span memory span for the values to put
   */

  void put(const IndexVector &view_vec, span<T> buff_span) override {
    NOT_ALLOWED();
    // Call Reference tensors put
    const auto &idx_vec = ref_map_func_(view_vec);
    auto ref_buf_size = ref_tensor_.block_size(idx_vec);
    auto ref_blck_dims = ref_tensor_.block_dims(idx_vec);
    std::vector<T> ref_vec(ref_buf_size, 0);
    BlockSpan ref_span{ref_vec.data(), ref_blck_dims};

    auto blck_dims = block_dims(view_vec);
    const BlockSpan blck_span{buff_span.data(), blck_dims};

    put_func_(blck_span, ref_span, view_vec);

    ref_tensor_.put(idx_vec, ref_vec);
  }

  /**
   * @brief Tensor accessor method for putting values in nonblocking fashion
   * to a set of indices with the specified memory span
   *
   * @tparam T type of the values hold on the tensor object
   * @param [in] idx_vec a vector of indices to put the values
   * @param [in] buff_span buff_span memory span for the values to put
   */

  void nb_put(const IndexVector &view_vec, span<T> buff_span,
              DataCommunicationHandlePtr data_comm_handle) override {
    NOT_ALLOWED();
    // Call Reference tensors nb_put
    const auto &idx_vec = ref_map_func_(view_vec);
    auto ref_buf_size = ref_tensor_.block_size(idx_vec);
    auto ref_blck_dims = ref_tensor_.block_dims(idx_vec);
    std::vector<T> ref_vec(ref_buf_size, 0);
    BlockSpan ref_span{ref_vec.data(), ref_blck_dims};

    auto blck_dims = block_dims(view_vec);
    const BlockSpan blck_span{buff_span.data(), blck_dims};

    put_func_(blck_span, ref_span, view_vec);
    ref_tensor_.nb_put(idx_vec, ref_vec, data_comm_handle);
  }

  /**
   * @brief Tensor accessor method for adding svalues to a set of indices
   * with the specified memory span
   *
   * @tparam T type of the values hold on the tensor object
   * @param [in] idx_vec a vector of indices to put the values
   * @param [in] buff_span buff_span memory span for the values to put
   */

  void add(const IndexVector &view_vec, span<T> buff_span) override {
    // Call Reference tensors nb_put
    const auto &idx_vec = ref_map_func_(view_vec);
    ref_tensor_.add(idx_vec, buff_span);
  }

  /**
   * @brief Tensor accessor method for adding svalues in nonblocking fashion
   * to a set of indices with the specified memory span
   *
   * @tparam T type of the values hold on the tensor object
   * @param [in] idx_vec a vector of indices to put the values
   * @param [in] buff_span buff_span memory span for the values to put
   */

  void nb_add(const IndexVector &view_vec, span<T> buff_span,
              DataCommunicationHandlePtr data_comm_handle) override {
    // Call Reference tensors nb_put
    const auto &idx_vec = ref_map_func_(view_vec);
    ref_tensor_.nb_add(idx_vec, buff_span, data_comm_handle);
  }

  int ga_handle() override {
    // Call Reference tensor
    return ref_tensor_.ga_handle();
  }

  T *access_local_buf() override { NOT_ALLOWED(); }

  const T *access_local_buf() const override { NOT_ALLOWED(); }

protected:
  Func ref_map_func_;
  CopyFunc get_func_;
  CopyFunc put_func_;
  Tensor<T> ref_tensor_;
}; // class ViewTensorImpl
} // namespace tamm

#endif // TENSOR_IMPL_HPP_
