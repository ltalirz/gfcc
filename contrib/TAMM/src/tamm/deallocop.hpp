#ifndef TAMM_DEALLOC_OP_
#define TAMM_DEALLOC_OP_

#include <algorithm>
#include <chrono>
#include <memory>

#include "tamm/boundvec.hpp"
#include "tamm/errors.hpp"
#include "tamm/labeled_tensor.hpp"
#include "tamm/runtime_engine.hpp"
#include "tamm/tensor.hpp"
#include "tamm/types.hpp"
#include "tamm/utils.hpp"

namespace tamm {
template<typename TensorType>
class DeallocOp : public Op {
public:
    DeallocOp(TensorType tensor) : tensor_{tensor} {}

    DeallocOp(const DeallocOp<TensorType>&) = default;

    TensorType tensor() const { return tensor_; }

    OpList canonicalize() const override { return OpList{(*this)}; }

    OpType op_type() const override { return OpType::dealloc; }

    std::shared_ptr<Op> clone() const override {
        return std::shared_ptr<Op>(new DeallocOp{*this});
    }

    void execute(ExecutionContext& ec, ExecutionHW hw = ExecutionHW::CPU) override { tensor_.deallocate(); }

    TensorBase* writes() const {
        return tensor_.base_ptr();
    }

    std::vector<TensorBase*> reads() const {
        return {};
    }

    TensorBase* accumulates() const {
        return {};
    }

    bool is_memory_barrier() const {
        return false;
    }
    std::string opstr_;
    
protected:
    TensorType tensor_;
    
}; // class AllocOp
}  // namespace tamm

#endif // TAMM_DEALLOC_OP_
