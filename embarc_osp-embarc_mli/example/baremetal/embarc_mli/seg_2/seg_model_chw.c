/*
* Copyright 2019, Synopsys, Inc.
* All rights reserved.
*
* This source code is licensed under the BSD-3-Clause license found in
* the LICENSE file in the root directory of this source tree.
*
*/

//TODO 
//intermediate data buffer 
//print all the layers output ir_tensor y(next, output) x(largest, input)
//print all model layer (kernal , pad, stride, ...) fc
//do the upsample
#include "seg_model.h"
#include "embARC.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mli_api.h"
#include "mli_types.h"
#include "mli_config.h"

#include "seg_constants.h"

#if (MODEL_BIT_DEPTH == MODEL_FX_8)
#define D_EL_TYPE (MLI_EL_FX_8)
#else
#define D_EL_TYPE (MLI_EL_FX_16)
#endif

//==============================================================
//
//
// Data related to the Module
//
//
//==============================================================

// Intermediate data buffers (enough size for max intermediate results)
//==============================
#if defined(MODEL_BIG)
#define IR_BUF_SZ_MOST (12*60*80)//(32*32*32) //32768 //1,382,400
#define IR_BUF_SZ_NEXT (24*30*40)//(32*16*16)  //8192  //576,000
/*#else
#define IR_BUF_SZ_MOST (64*120*160)//(32*32*32) //32768 //1,382,400 need smaller image?
#define IR_BUF_SZ_NEXT (64*120*160)*/
#endif
static d_type  _Z    x_mem_buf[IR_BUF_SZ_MOST];
static d_type  _Y    y_mem_buf[IR_BUF_SZ_NEXT];
static d_type  _X    z_mem_buf[IR_BUF_SZ_NEXT];

// Module Input/Output tensors and their's external interface
//============================================================
static mli_tensor input = {
    .data = (void *)x_mem_buf,
    .capacity = sizeof(d_type) * IN_POINTS,
    .shape = {60,89, 3}, //.shape = {120,160, 3},
    .rank = 3,
    .el_type = D_EL_TYPE,
    .el_params.fx.frac_bits = 7,
};

static mli_tensor output = {
    .data = (void *)y_mem_buf,
    .capacity = sizeof(d_type) * OUT_POINTS,
    .shape = {2,30,40}, //.shape = {19,60,80}, //.shape = {3,120,160},
    .rank = 3,
    .el_type = D_EL_TYPE,
    .el_params.fx.frac_bits = 0,
};

// Interface variables: Available to user via main model header
//===========================================================
mli_tensor * const seg_net_input = &input;
mli_tensor * const seg_net_output = &output;


//==============================================================
//  Model description and configuration
//==============================================================

// Configuration objects for layers
//===============================================

static const mli_permute_cfg permute_hwc2chw_cfg = {
        .perm_dim = {2, 0, 1}
};

static const mli_conv2d_cfg shared_conv_cfg = {      // stride = 1
    .stride_height = 1, .stride_width = 1,            
    .padding_bottom = 1, .padding_top = 1,           // TODO padding
    .padding_left = 1, .padding_right = 1,
    .relu.type = MLI_RELU_GEN                        //relu
};

static const mli_conv2d_cfg shared_conv_cfg2 = {     // stride = 2
    .stride_height = 2, .stride_width = 2,
    .padding_bottom = 1, .padding_top = 0,           // TODO
    .padding_left = 0, .padding_right = 1,
    .relu.type = MLI_RELU_GEN
};

static const mli_conv2d_cfg shared_conv_cfg3 = {     // for fully convolution layer
    .stride_height = 1, .stride_width = 1,
    .padding_bottom = 0, .padding_top = 0,           // TODO
    .padding_left = 0, .padding_right = 0,
    .relu.type = MLI_RELU_NONE                       // no relu
};

static const mli_pool_cfg shared_pool_cfg = {
    .kernel_height = 3,	.kernel_width = 3,
    .stride_height = 2, .stride_width = 2,
    .padding_bottom = 1, .padding_top = 0,
    .padding_left = 0, .padding_right = 1
};

//static const mli_relu_cfg relu_cfg = {.type = MLI_RELU_GEN};

// Conv 1 Layer related tensors
//===================================
static const mli_tensor L1_conv_wt = {
    .data = (void *)L1_conv_wt_buf,
    .capacity = CONV1_W_ELEMENTS * sizeof(w_type),
    .shape = CONV1_W_SHAPE,
    .rank = CONV1_W_RANK,
    .el_type = W_EL_TYPE,
    .el_params.fx.frac_bits = CONV1_W_FRAQ,
};


static const mli_tensor L1_conv_bias = {
    .data = (void *)L1_conv_bias_buf,
    .capacity = CONV1_B_ELEMENTS * sizeof(w_type),
    .shape = CONV1_B_SHAPE,
    .rank = CONV1_B_RANK,
    .el_type = W_EL_TYPE,
    .el_params.fx.frac_bits = CONV1_B_FRAQ,
};


// Conv 2 Layer related data
//===================================
static mli_tensor L2_conv_wt = {
    .data = (void *)L2_conv_wt_buf,
    .capacity = CONV2_W_ELEMENTS * sizeof(w_type),
    .shape = CONV2_W_SHAPE,
    .rank = CONV2_W_RANK,
    .el_type = W_EL_TYPE,
    .el_params.fx.frac_bits = CONV2_W_FRAQ,
};

static mli_tensor L2_conv_bias = {
    .data = (void *)L2_conv_bias_buf,
    .capacity = CONV2_B_ELEMENTS * sizeof(w_type),
    .shape = CONV2_B_SHAPE,
    .rank = CONV2_B_RANK,
    .el_type = W_EL_TYPE,
    .el_params.fx.frac_bits = CONV2_B_FRAQ,
};


// Conv 3 Layer related data
//===================================
static mli_tensor L3_conv_wt = {
    .data = (void *)L3_conv_wt_buf,
    .capacity = CONV3_W_ELEMENTS * sizeof(w_type),
    .shape = CONV3_W_SHAPE,
    .rank = CONV3_W_RANK,
    .el_type = W_EL_TYPE,
    .el_params.fx.frac_bits = CONV3_W_FRAQ,
};

static mli_tensor L3_conv_bias = {
    .data = (void *)L3_conv_bias_buf,
    .capacity = CONV3_B_ELEMENTS * sizeof(w_type),
    .shape = CONV3_B_SHAPE,
    .rank = CONV3_B_RANK,
    .el_type = W_EL_TYPE,
    .el_params.fx.frac_bits = CONV3_B_FRAQ,
};

// FC7 Layer related data
//===================================
static mli_tensor L7_fc_wt = {
    .data = (void *)L7_fc_wt_buf,
    .capacity = FC7_W_ELEMENTS * sizeof(w_type),
    .shape = FC7_W_SHAPE,
    .rank = FC7_W_RANK,
    .el_type = W_EL_TYPE,
    .el_params.fx.frac_bits = FC7_W_FRAQ,
};

static mli_tensor L7_fc_bias = {
    .data = (void *)L7_fc_bias_buf,
    .capacity = FC7_B_ELEMENTS * sizeof(w_type),
    .shape = FC7_B_SHAPE,
    .rank = FC7_B_RANK,
    .el_type = W_EL_TYPE,
    .el_params.fx.frac_bits = FC7_B_FRAQ,
};


// Intermediate result tensors
//===============================================
static mli_tensor ir_tensor_X = {
    .data = (void *)x_mem_buf,
    .capacity = sizeof(x_mem_buf),
    .shape = {0, 0, 0, 0},
    .rank = 4,
    .el_type = D_EL_TYPE,
    .el_params.fx.frac_bits = FRQ_BITS(0, d_type),
};

static mli_tensor ir_tensor_Y = {
    .data = (void *)y_mem_buf,
    .capacity = sizeof(y_mem_buf),
    .shape = {0, 0, 0, 0},
    .rank = 4,
    .el_type = D_EL_TYPE,
    .el_params.fx.frac_bits = FRQ_BITS(0, d_type),
};

static mli_tensor ir_tensor_Z = {
    .data = (void *)z_mem_buf,
    .capacity = sizeof(z_mem_buf),
    .shape = {0, 0, 0, 0},
    .rank = 4,
    .el_type = D_EL_TYPE,
    .el_params.fx.frac_bits = FRQ_BITS(0, d_type),
};


//==============================================================
//  Wrappers on MLI calls to deal with various
//  bit depth configurable in compile time
//==============================================================
static inline mli_status maxpool_chw(const mli_tensor *in, const mli_pool_cfg *cfg, mli_tensor *out);

//static inline mli_status avepool_chw(const mli_tensor *in, const mli_pool_cfg *cfg, mli_tensor *out);

static inline mli_status elwise_add(const mli_tensor *in1, const mli_tensor *in2, mli_tensor *out);

static inline mli_status softmax(const mli_tensor *in,	mli_tensor *out);

static inline mli_status relu(const mli_tensor *in, const mli_relu_cfg *cfg, mli_tensor *out);

static inline mli_status mli_krn_permute_fx(const mli_tensor *in, const mli_permute_cfg *cfg, mli_tensor *out);

static inline mli_status conv2d_chw(       //stride = 1 & 2
        const mli_tensor *in,
        const mli_tensor *weights,
        const mli_tensor *bias,
        const mli_conv2d_cfg *cfg,
        mli_tensor *out);

//==============================================================
//
//  CIFAR10 graph based on Caffe example.
//  Layer-by-Layer execution for CHW layput
//
//==============================================================
void seg_net(void) {
        // Version A: Pure implementation: without return status checking and profiling wrappers
        //========================================================================================
        // LAYER 0: Change RGB Image layout
        //=======================================
        #if defined(MODEL_BIG)
        mli_krn_permute_fx(&input, &permute_hwc2chw_cfg, &ir_tensor_Y);  //input use x_mem

        // LAYER 1
        //=======================================
        ir_tensor_X.el_params.fx.frac_bits = CONV1_OUT_FRAQ;
        conv2d_chw(&ir_tensor_Y, &L1_conv_wt, &L1_conv_bias, &shared_conv_cfg, &ir_tensor_X);
        //relu(&ir_tensor_X, &relu_cfg, &ir_tensor_Y)
        maxpool_chw(&ir_tensor_X, &shared_pool_cfg, &ir_tensor_Z);   //define frac_bit??

        // LAYER 2
        //=======================================
        ir_tensor_Y.el_params.fx.frac_bits = CONV2_OUT_FRAQ;
        conv2d_chw(&ir_tensor_X, &L2_conv_wt, &L2_conv_bias, &shared_conv_cfg2, &ir_tensor_Y);

        // LAYER 3
        //=======================================
        ir_tensor_X.el_params.fx.frac_bits = CONV3_OUT_FRAQ;
        conv2d_chw(&ir_tensor_Y, &L3_conv_wt, &L3_conv_bias, &shared_conv_cfg, &ir_tensor_X);
        elwise_add(&ir_tensor_X, &ir_tensor_Z, &ir_tensor_Y);

        // LAYER 7
        //=======================================
        ir_tensor_X.el_params.fx.frac_bits = FC7_OUT_FRAQ;
        conv2d_chw(&ir_tensor_Y, &L7_fc_wt, &L7_fc_bias, &shared_conv_cfg3, &ir_tensor_X);

        softmax(&ir_tensor_X, &output); //output use y_mem &ir_tensor_X
        #endif
}

//========================================================================================
//  MLI Functions wrappers: Kernels w/o weights
//========================================================================================
#if (MODEL_BIT_DEPTH != MODEL_FX_8)
static inline mli_status elwise_add(const mli_tensor *in1, const mli_tensor *in2, mli_tensor *out) {
    return mli_krn_eltwise_add_fx16(in1, in2, out);
}

static inline mli_status maxpool_chw(const mli_tensor *in, const mli_pool_cfg *cfg, mli_tensor *out) {
    return mli_krn_maxpool_chw_fx16_k3x3_krnpad(in, cfg, out);
}

/*static inline mli_status avepool_chw(const mli_tensor *in, const mli_pool_cfg *cfg, mli_tensor *out) {
    return mli_krn_avepool_chw_fx16_k3x3_krnpad(in, cfg, out);
}*/

static inline mli_status softmax(const mli_tensor *in,	mli_tensor *out) {
    return mli_krn_softmax_fx16(in, out);
}

static inline mli_status relu(const mli_tensor *in, const mli_relu_cfg *cfg, mli_tensor *out) {
    return mli_krn_relu_fx16(in, cfg, out);
}

static inline mli_status mli_krn_permute_fx(const mli_tensor *in, const mli_permute_cfg *cfg, mli_tensor *out) {
    return mli_krn_permute_fx16(in, cfg, out);
}

#else // MODEL_BIT_DEPTH == (MODEL_FX_8W16D || MODEL_FX_8W16D)
static inline mli_status elwise_add(const mli_tensor *in1, const mli_tensor *in2, mli_tensor *out) {
    return mli_krn_eltwise_add_fx8(in1, in2, out);
}

static inline mli_status maxpool_chw(const mli_tensor *in, const mli_pool_cfg *cfg, mli_tensor *out) {
    return mli_krn_maxpool_chw_fx8_generic(in, cfg, out);
    //
    //mli_krn_maxpool_chw_fx8_generic
}

/*static inline mli_status avepool_chw(const mli_tensor *in, const mli_pool_cfg *cfg, mli_tensor *out) {
    return mli_krn_avepool_chw_fx8_k3x3_krnpad(in, cfg, out);
}*/

static inline mli_status softmax(const mli_tensor *in,	mli_tensor *out) {
    return mli_krn_softmax_fx8(in, out);
}

static inline mli_status relu(const mli_tensor *in, const mli_relu_cfg *cfg, mli_tensor *out) {
    return mli_krn_relu_fx8(in, cfg, out);
}

static inline mli_status mli_krn_permute_fx(const mli_tensor *in, const mli_permute_cfg *cfg, mli_tensor *out) {
    return mli_krn_permute_fx8(in, cfg, out);
}

#endif

//========================================================================================
//  MLI Functions wrappers: Kernels with weights
//========================================================================================
#if (MODEL_BIT_DEPTH == MODEL_FX_8)
static inline mli_status conv2d_chw(
        const mli_tensor *in,
        const mli_tensor *weights,
        const mli_tensor *bias,
        const mli_conv2d_cfg *cfg,
        mli_tensor *out) {
    return mli_krn_conv2d_chw_fx8_generic(in, weights, bias, cfg, out);
    //return mli_krn_conv2d_chw_fx8_k3x3_krnpad(in, weights, bias, cfg, out);
}


#elif (MODEL_BIT_DEPTH == MODEL_FX_16)
static inline mli_status conv2d_chw(
        const mli_tensor *in,
        const mli_tensor *weights,
        const mli_tensor *bias,
        const mli_conv2d_cfg *cfg,
        mli_tensor *out) {
    return mli_krn_conv2d_chw_fx16_k5x5_str1_krnpad(in, weights, bias, cfg, out);
}


#else // MODEL_BIT_DEPTH == MODEL_FX_8W16D
static inline mli_status conv2d_chw(
        const mli_tensor *in,
        const mli_tensor *weights,
        const mli_tensor *bias,
        const mli_conv2d_cfg *cfg,
        mli_tensor *out) {
    return mli_krn_conv2d_chw_fx8w16d(in, weights, bias, cfg, out);
}

#endif
