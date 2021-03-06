/*******************************************************************************
* Copyright 2016-2018 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#ifndef CPU_JIT_AVX512_CORE_U8S8S32X_CONVOLUTION_HPP
#define CPU_JIT_AVX512_CORE_U8S8S32X_CONVOLUTION_HPP

#include "c_types_map.hpp"
#include "cpu_convolution_pd.hpp"
#include "cpu_engine.hpp"
#include "jit_transpose_src_utils.hpp"
#include "cpu_reducer.hpp"
#include "cpu_barrier.hpp"

#include "jit_avx512_core_u8s8s32x_conv_kernel.hpp"

namespace mkldnn {
namespace impl {
namespace cpu {

template <bool with_relu, impl::data_type_t dst_type>
struct _jit_avx512_core_u8s8s32x_convolution_fwd_t : public cpu_primitive_t {
    struct pd_t : public _cpu_convolution_fwd_pd_t<with_relu> {
        pd_t(engine_t *engine, const typename pd_t::base_desc_t *adesc,
                const primitive_attr_t *attr,
                const typename pd_t::base_class *hint_fwd_pd)
            : _cpu_convolution_fwd_pd_t<with_relu>(engine, adesc, attr,
                    hint_fwd_pd)
            , jcp_({})
        {
        }
        DECLARE_COMMON_PD_T(
                JIT_IMPL_NAME_HELPER("jit:", avx512_core, ""),
                _jit_avx512_core_u8s8s32x_convolution_fwd_t<with_relu,
                dst_type>);

        virtual status_t init() override
        {
            using namespace prop_kind;
            assert(this->engine()->kind() == engine_kind::cpu);
            bool ok = true
                    && utils::one_of(this->cdesc_().prop_kind, forward_training,
                               forward_inference)
                    && this->cdesc_().alg_kind == alg_kind::convolution_direct
                    && this->cdesc_().dst_desc.data_type == dst_type
                    && utils::implication(this->with_bias(), utils::one_of(
                            this->cdesc_().bias_desc.data_type, data_type::f32,
                            data_type::s32, data_type::s8, data_type::u8))
                    && this->cdesc_().accum_data_type == data_type::s32;
            if (!ok)
                return status::unimplemented;

            return jit_avx512_core_u8s8s32x_fwd_kernel::init_conf(
                    jcp_, this->cdesc_(), this->src_pd_, this->weights_pd_,
                    this->dst_pd_,this->bias_pd_, *this->attr(),
                    with_relu, this->negative_slope());
        }

        jit_conv_conf_t jcp_;
    };

    _jit_avx512_core_u8s8s32x_convolution_fwd_t(const pd_t *pd,
            const input_vector &inputs, const output_vector &outputs)
        : cpu_primitive_t(&conf_, inputs, outputs), conf_(*pd)
    {
        kernel_ = new jit_avx512_core_u8s8s32x_fwd_kernel(conf_.jcp_,
                    *conf_.attr());

        const int nthreads = omp_get_max_threads();
        ws_per_thread_ = conf_.jcp_.oh * conf_.jcp_.ow * conf_.jcp_.oc_block
                            * conf_.jcp_.nb_oc_blocking;
        ws_ = (acc_data_t *)malloc(
                nthreads * ws_per_thread_ * sizeof(acc_data_t), 64);
    }

    ~_jit_avx512_core_u8s8s32x_convolution_fwd_t() {
        free(ws_);
        delete kernel_;
    };

    typedef typename prec_traits<data_type::u8>::type src_data_t;
    typedef typename prec_traits<data_type::s8>::type wei_data_t;
    typedef typename prec_traits<dst_type>::type dst_data_t;
    typedef typename prec_traits<data_type::s32>::type acc_data_t;

    virtual void execute(event_t *e)
    {
        execute_forward();
        e->set_state(event_t::ready);
    }

private:
    void execute_forward();
    pd_t conf_;
    jit_avx512_core_u8s8s32x_fwd_kernel *kernel_;
    size_t ws_per_thread_;
    acc_data_t *ws_;
};

template <impl::data_type_t dst_type>
using jit_avx512_core_u8s8s32x_convolution_fwd_t =
    _jit_avx512_core_u8s8s32x_convolution_fwd_t<false, dst_type>;

template <impl::data_type_t dst_type>
using jit_avx512_core_u8s8s32x_convolution_relu_t =
    _jit_avx512_core_u8s8s32x_convolution_fwd_t<true, dst_type>;

}
}
}

#endif

// vim: et ts=4 sw=4 cindent cino^=l0,\:0,N-s
