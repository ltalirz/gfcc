#pragma once

#include "ccsd_t_common.hpp"

//
// created by tc_gen_definition()
#define FUSION_SIZE_SLICE_1_H3 	4
#define FUSION_SIZE_SLICE_1_H2 	4
#define FUSION_SIZE_SLICE_1_H1 	4
#define FUSION_SIZE_SLICE_1_P6 	4
#define FUSION_SIZE_SLICE_1_P5 	4
#define FUSION_SIZE_SLICE_1_P4 	4
#define FUSION_SIZE_SLICE_1_H7 	16

#define FUSION_SIZE_SLICE_2_H3 	4
#define FUSION_SIZE_SLICE_2_H2 	4
#define FUSION_SIZE_SLICE_2_H1 	4
#define FUSION_SIZE_SLICE_2_P6 	4
#define FUSION_SIZE_SLICE_2_P5 	4
#define FUSION_SIZE_SLICE_2_P4 	4
#define FUSION_SIZE_SLICE_2_H7 	16

#define FUSION_SIZE_INT_UNIT 	FUSION_SIZE_SLICE_1_H7

#define FUSION_SIZE_TB_1_X 	FUSION_SIZE_SLICE_1_H3 * FUSION_SIZE_SLICE_1_H2
#define FUSION_SIZE_TB_1_Y 	FUSION_SIZE_SLICE_1_P6 * FUSION_SIZE_SLICE_1_H1
#define FUSION_SIZE_REG_1_X 	FUSION_SIZE_SLICE_1_P5
#define FUSION_SIZE_REG_1_Y 	FUSION_SIZE_SLICE_1_P4

#define FUSION_SIZE_TB_2_X 	FUSION_SIZE_SLICE_2_H3 * FUSION_SIZE_SLICE_2_H2
#define FUSION_SIZE_TB_2_Y 	FUSION_SIZE_SLICE_2_P4 * FUSION_SIZE_SLICE_2_H1
#define FUSION_SIZE_REG_2_X 	FUSION_SIZE_SLICE_2_P5
#define FUSION_SIZE_REG_2_Y 	FUSION_SIZE_SLICE_2_P6

#define CEIL(a, b)      	(((a) + (b) - 1) / (b))

#define NUM_IA6_LOOPS           9
#define NUM_D1_EQUATIONS        9
#define NUM_D2_EQUATIONS        9
#define NUM_S1_EQUATIONS        9
#define NUM_D1_INDEX            7
#define NUM_D2_INDEX            7
#define NUM_S1_INDEX            6
#define NUM_ENERGIES            2

#define MAX_NOAB		30
#define MAX_NVAB 		120

using localAcc = sycl::accessor<double, 2, sycl::access_mode::read_write, sycl::target::local>;

// 64 KB = 65536 bytes = 16384 (int) = 8192 (size_t)
// 9 * 9 * noab = 81 * noab

//
// 	|constant memory| = sizeof(int) * {(6 + 9) + ((7 + 9) * MAX_NOAB) + ((7 + 9) * MAX_NVAB)}
// 										= 4 bytes * (15 + 16 * 20 + 16 * 80) = 8 bytes * (15 + 320 + 1280) = 1615 * 4 bytes = 6460 bytes (6.30 KB)
//

void revised_jk_ccsd_t_fully_fused_kernel(int size_noab, int size_nvab,
                                          // 	common
                                          int size_max_dim_s1_t1, int size_max_dim_s1_v2,
                                          int size_max_dim_d1_t2, int size_max_dim_d1_v2,
                                          int size_max_dim_d2_t2, int size_max_dim_d2_v2,
                                          //
                                          double* df_dev_d1_t2_all, double* df_dev_d1_v2_all,
                                          double* df_dev_d2_t2_all, double* df_dev_d2_v2_all,
                                          double* df_dev_s1_t1_all, double* df_dev_s1_v2_all,
                                          //  energies
                                          double* dev_evl_sorted_h1b,
                                          double* dev_evl_sorted_h2b,
                                          double* dev_evl_sorted_h3b,
                                          double* dev_evl_sorted_p4b,
                                          double* dev_evl_sorted_p5b,
                                          double* dev_evl_sorted_p6b,
                                          // 	not-fully reduced results
                                          double* reduced_energy,
                                          //  common
                                          int num_blks_h3b, int num_blks_h2b, int num_blks_h1b,
                                          int num_blks_p6b, int num_blks_p5b, int num_blks_p4b,
                                          //
                                          int base_size_h1b, int base_size_h2b, int base_size_h3b,
                                          int base_size_p4b, int base_size_p5b, int base_size_p6b,
                                          sycl::nd_item<2>& item,
                                          int* const_df_s1_size, int* const_df_s1_exec,
                                          int* const_df_d1_size, int* const_df_d1_exec,
                                          int* const_df_d2_size, int* const_df_d2_exec,
                                          localAcc sm_a,
                                          localAcc sm_b)
{
    size_t threadIdx_x = item.get_local_id(1);
    size_t threadIdx_y = item.get_local_id(0);
    size_t blockIdx_x = item.get_group(1);

    int internal_upperbound = 0;
    int internal_offset;

    // should support for non-full tiles
    int idx_h3 = threadIdx_x % FUSION_SIZE_SLICE_1_H3;
    int idx_h2 = threadIdx_x / FUSION_SIZE_SLICE_1_H3;
    int idx_p6 = threadIdx_y % FUSION_SIZE_SLICE_1_P6;
    int idx_h1 = threadIdx_y / FUSION_SIZE_SLICE_1_P6;

    int blk_idx_p4b = blockIdx_x /
        (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b * num_blks_p5b);
    int tmp_blkIdx = blockIdx_x %
        (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b * num_blks_p5b);
    int blk_idx_p5b = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b);
    tmp_blkIdx      = (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b);
    int blk_idx_p6b = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b * num_blks_h1b);
    tmp_blkIdx      = (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b * num_blks_h1b);
    int blk_idx_h1b = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b);
    tmp_blkIdx      = (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b);
    int blk_idx_h2b = (tmp_blkIdx) / (num_blks_h3b);
    int blk_idx_h3b = blockIdx_x % (num_blks_h3b);

    int str_blk_idx_h3 = blk_idx_h3b * FUSION_SIZE_SLICE_1_H3;
    int str_blk_idx_h2 = blk_idx_h2b * FUSION_SIZE_SLICE_1_H2;
    int str_blk_idx_h1 = blk_idx_h1b * FUSION_SIZE_SLICE_1_H1;
    int str_blk_idx_p6 = blk_idx_p6b * FUSION_SIZE_SLICE_1_P6;
    int str_blk_idx_p5 = blk_idx_p5b * FUSION_SIZE_SLICE_1_P5;
    int str_blk_idx_p4 = blk_idx_p4b * FUSION_SIZE_SLICE_1_P4;

    //
    int rng_h3, rng_h2, rng_h1, rng_p6, rng_p5, rng_p4;
    int energy_rng_h3, energy_rng_h2, energy_rng_h1, energy_rng_p6, energy_rng_p5, energy_rng_p4;
    if ((base_size_h3b - (str_blk_idx_h3)) >= FUSION_SIZE_SLICE_1_H3)
    {
        energy_rng_h3 = FUSION_SIZE_SLICE_1_H3;
    }
    else
    {
        energy_rng_h3 = base_size_h3b % FUSION_SIZE_SLICE_1_H3;
    }

    if ((base_size_h2b - (str_blk_idx_h2)) >= FUSION_SIZE_SLICE_1_H2)
    {
        energy_rng_h2 = FUSION_SIZE_SLICE_1_H2;
    }
    else
    {
        energy_rng_h2 = base_size_h2b % FUSION_SIZE_SLICE_1_H2;
    }

    if ((base_size_h1b - (str_blk_idx_h1)) >= FUSION_SIZE_SLICE_1_H1)
    {
        energy_rng_h1 = FUSION_SIZE_SLICE_1_H1;
    }
    else
    {
        energy_rng_h1 = base_size_h1b % FUSION_SIZE_SLICE_1_H1;
    }

    if ((base_size_p6b - (str_blk_idx_p6)) >= FUSION_SIZE_SLICE_1_P6)
    {
        energy_rng_p6 = FUSION_SIZE_SLICE_1_P6;
    }
    else
    {
        energy_rng_p6 = base_size_p6b % FUSION_SIZE_SLICE_1_P6;
    }

    if ((base_size_p5b - (str_blk_idx_p5)) >= FUSION_SIZE_SLICE_1_P5)
    {
        energy_rng_p5 = FUSION_SIZE_SLICE_1_P5;
    }
    else
    {
        energy_rng_p5 = base_size_p5b % FUSION_SIZE_SLICE_1_P5;
    }

    if ((base_size_p4b - (str_blk_idx_p4)) >= FUSION_SIZE_SLICE_1_P4)
    {
        energy_rng_p4 = FUSION_SIZE_SLICE_1_P4;
    }
    else
    {
        energy_rng_p4 = base_size_p4b % FUSION_SIZE_SLICE_1_P4;
    }

    //
    double temp_av{0};
    double temp_bv[4] = {0};
    double reg_tile[4][4] = {{0}};
    double reg_singles[4][4] = {{0}};

    int base_size_h7b, base_size_p7b;

    int energy_str_blk_idx_p4 = str_blk_idx_p4;
    int energy_str_blk_idx_p5 = str_blk_idx_p5;
    double eval_h3 = dev_evl_sorted_h3b[str_blk_idx_h3 + idx_h3];
    double eval_h2 = dev_evl_sorted_h2b[str_blk_idx_h2 + idx_h2];
    double eval_p6 = dev_evl_sorted_p6b[str_blk_idx_p6 + idx_p6];
    double eval_h1 = dev_evl_sorted_h1b[str_blk_idx_h1 + idx_h1];

    double partial_inner_factor = eval_h3 + eval_h2 + eval_h1 - eval_p6;

    //
    //  energies
    //
    double energy_1 = 0.0;
    double energy_2 = 0.0;

#pragma unroll 1
    for (int iter_noab = 0; iter_noab < size_noab; iter_noab++)
    {
        //
        int flag_d1_1 = const_df_d1_exec[0 + (iter_noab) * NUM_D1_EQUATIONS];
        int flag_d1_2 = const_df_d1_exec[1 + (iter_noab) * NUM_D1_EQUATIONS];
        int flag_d1_3 = const_df_d1_exec[2 + (iter_noab) * NUM_D1_EQUATIONS];

        //
        base_size_h1b = const_df_d1_size[0 + (iter_noab) * NUM_D1_INDEX];
        base_size_h2b = const_df_d1_size[1 + (iter_noab) * NUM_D1_INDEX];
        base_size_h3b = const_df_d1_size[2 + (iter_noab) * NUM_D1_INDEX];
        base_size_h7b = const_df_d1_size[3 + (iter_noab) * NUM_D1_INDEX];
        base_size_p4b = const_df_d1_size[4 + (iter_noab) * NUM_D1_INDEX];
        base_size_p5b = const_df_d1_size[5 + (iter_noab) * NUM_D1_INDEX];
        base_size_p6b = const_df_d1_size[6 + (iter_noab) * NUM_D1_INDEX];

        //
        num_blks_h1b = CEIL(base_size_h1b, FUSION_SIZE_SLICE_1_H1);
        num_blks_h2b = CEIL(base_size_h2b, FUSION_SIZE_SLICE_1_H2);
        num_blks_h3b = CEIL(base_size_h3b, FUSION_SIZE_SLICE_1_H3);
        num_blks_p4b = CEIL(base_size_p4b, FUSION_SIZE_SLICE_1_P4);
        num_blks_p5b = CEIL(base_size_p5b, FUSION_SIZE_SLICE_1_P5);
        num_blks_p6b = CEIL(base_size_p6b, FUSION_SIZE_SLICE_1_P6);

        // 	(2) blk_idx_h/p*b
        blk_idx_p4b =
            blockIdx_x / (num_blks_h3b * num_blks_h2b * num_blks_h1b *
                                     num_blks_p6b * num_blks_p5b);
        tmp_blkIdx =
            blockIdx_x % (num_blks_h3b * num_blks_h2b * num_blks_h1b *
                                     num_blks_p6b * num_blks_p5b);
        blk_idx_p5b     = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b);
        tmp_blkIdx  	= (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b);
        blk_idx_p6b     = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b * num_blks_h1b);
        tmp_blkIdx 		= (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b * num_blks_h1b);
        blk_idx_h1b     = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b);
        tmp_blkIdx 		= (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b);
        blk_idx_h2b 	= (tmp_blkIdx) / (num_blks_h3b);
        blk_idx_h3b = blockIdx_x % (num_blks_h3b);

        // 	(3) str_blk_idx_h/p*
        str_blk_idx_h3 	= blk_idx_h3b * FUSION_SIZE_SLICE_1_H3;
        str_blk_idx_h2 	= blk_idx_h2b * FUSION_SIZE_SLICE_1_H2;
        str_blk_idx_h1 	= blk_idx_h1b * FUSION_SIZE_SLICE_1_H1;
        str_blk_idx_p6 	= blk_idx_p6b * FUSION_SIZE_SLICE_1_P6;
        str_blk_idx_p5 	= blk_idx_p5b * FUSION_SIZE_SLICE_1_P5;
        str_blk_idx_p4 	= blk_idx_p4b * FUSION_SIZE_SLICE_1_P4;

        // 	(4) rng_h/p*
        if ((base_size_h3b - (str_blk_idx_h3)) >= FUSION_SIZE_SLICE_1_H3)
            rng_h3 = FUSION_SIZE_SLICE_1_H3;
        else
            rng_h3 = base_size_h3b % FUSION_SIZE_SLICE_1_H3;

        if ((base_size_h2b - (str_blk_idx_h2)) >= FUSION_SIZE_SLICE_1_H2)
            rng_h2 = FUSION_SIZE_SLICE_1_H2;
        else
            rng_h2 = base_size_h2b % FUSION_SIZE_SLICE_1_H2;

        if ((base_size_h1b - (str_blk_idx_h1)) >= FUSION_SIZE_SLICE_1_H1)
            rng_h1 = FUSION_SIZE_SLICE_1_H1;
        else
            rng_h1 = base_size_h1b % FUSION_SIZE_SLICE_1_H1;

        if ((base_size_p6b - (str_blk_idx_p6)) >= FUSION_SIZE_SLICE_1_P6)
            rng_p6 = FUSION_SIZE_SLICE_1_P6;
        else
            rng_p6 = base_size_p6b % FUSION_SIZE_SLICE_1_P6;

        if ((base_size_p5b - (str_blk_idx_p5)) >= FUSION_SIZE_SLICE_1_P5)
            rng_p5 = FUSION_SIZE_SLICE_1_P5;
        else
            rng_p5 = base_size_p5b % FUSION_SIZE_SLICE_1_P5;

        if ((base_size_p4b - (str_blk_idx_p4)) >= FUSION_SIZE_SLICE_1_P4)
            rng_p4 = FUSION_SIZE_SLICE_1_P4;
        else
            rng_p4 = base_size_p4b % FUSION_SIZE_SLICE_1_P4;

        //  sd1_1
        if (flag_d1_1 >= 0)
        {
            //
            double* tmp_dev_d1_t2 = df_dev_d1_t2_all + size_max_dim_d1_t2 * flag_d1_1;
            double* tmp_dev_d1_v2 = df_dev_d1_v2_all + size_max_dim_d1_v2 * flag_d1_1;

            //
            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_h7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_h7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_p4 && idx_h1 < rng_h1 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_a[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_2_Y] =
                            tmp_dev_d1_t2[(str_blk_idx_p4 + idx_p6 +
                                           (str_blk_idx_p5 + ll +
                                            (str_blk_idx_h1 + idx_h1) * base_size_p5b) *
                                           base_size_p4b) *
                                          base_size_h7b +
                                          (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_h3 < rng_h3 && idx_h2 < rng_h2 &&
                    threadIdx_y < FUSION_SIZE_INT_UNIT - internal_upperbound) {
                    for (int ll = 0; ll < rng_p6; ll++)
                    {
                        sm_b[threadIdx_y][threadIdx_x + ll * FUSION_SIZE_TB_2_X] =
                            tmp_dev_d1_v2[str_blk_idx_h3 + idx_h3 +
                                          (str_blk_idx_h2 + idx_h2 +
                                           (str_blk_idx_p6 + ll +
                                            (threadIdx_y + l) * base_size_p6b) *
                                           base_size_h2b) *
                                          base_size_h3b];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: -1
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_b[ll][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_2_H3 + 0];
                    temp_bv[1] = sm_b[ll][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_2_H3 + 16];
                    temp_bv[2] = sm_b[ll][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_2_H3 + 32];
                    temp_bv[3] = sm_b[ll][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_2_H3 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_a[ll][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6 + (xx * 16)];

                        reg_tile[0][xx] -= temp_av * temp_bv[0];
                        reg_tile[1][xx] -= temp_av * temp_bv[1];
                        reg_tile[2][xx] -= temp_av * temp_bv[2];
                        reg_tile[3][xx] -= temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        //  sd1_2
        if (flag_d1_2 >= 0)
        {
            //
            double* tmp_dev_d1_t2 = df_dev_d1_t2_all + size_max_dim_d1_t2 * flag_d1_2;
            double* tmp_dev_d1_v2 = df_dev_d1_v2_all + size_max_dim_d1_v2 * flag_d1_2;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_h7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_h7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_p4 && idx_h1 < rng_h2 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound) {
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_a[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_2_Y] =
                            tmp_dev_d1_t2[(str_blk_idx_p4 + idx_p6 +
                                           (str_blk_idx_p5 + ll +
                                            (str_blk_idx_h2 + idx_h1) * base_size_p5b) *
                                           base_size_p4b) *
                                          base_size_h7b +
                                          (threadIdx_x + l)];
                    }
                }

                // Load Input Tensor to Shared Memory
                if (idx_h3 < rng_h3 && idx_h2 < rng_h1 &&
                    threadIdx_y < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p6; ll++)
                    {
                        sm_b[threadIdx_y][threadIdx_x + ll * FUSION_SIZE_TB_2_X] =
                            tmp_dev_d1_v2[str_blk_idx_h3 + idx_h3 +
                                          (str_blk_idx_h1 + idx_h2 +
                                           (str_blk_idx_p6 + ll +
                                            (threadIdx_y + l) * base_size_p6b) *
                                           base_size_h1b) *
                                          base_size_h3b];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: -1
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_b[ll][idx_h3 + (idx_h1) * FUSION_SIZE_SLICE_2_H3 + 0];
                    temp_bv[1] = sm_b[ll][idx_h3 + (idx_h1) * FUSION_SIZE_SLICE_2_H3 + 16];
                    temp_bv[2] = sm_b[ll][idx_h3 + (idx_h1) * FUSION_SIZE_SLICE_2_H3 + 32];
                    temp_bv[3] = sm_b[ll][idx_h3 + (idx_h1) * FUSION_SIZE_SLICE_2_H3 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_a[ll][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_2_P4 + (xx * 16)];

                        reg_tile[0][xx] += temp_av * temp_bv[0];
                        reg_tile[1][xx] += temp_av * temp_bv[1];
                        reg_tile[2][xx] += temp_av * temp_bv[2];
                        reg_tile[3][xx] += temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        //  sd1_3
        if (flag_d1_3 >= 0)
        {
            //
            double* tmp_dev_d1_t2 = df_dev_d1_t2_all + size_max_dim_d1_t2 * flag_d1_3;
            double* tmp_dev_d1_v2 = df_dev_d1_v2_all + size_max_dim_d1_v2 * flag_d1_3;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_h7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_h7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_p4 && idx_h1 < rng_h3 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_a[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_2_Y] =
                            tmp_dev_d1_t2[(str_blk_idx_p4 + idx_p6 +
                                           (str_blk_idx_p5 + ll +
                                            (str_blk_idx_h3 + idx_h1) * base_size_p5b) *
                                           base_size_p4b) *
                                          base_size_h7b +
                                          (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_h3 < rng_h2 && idx_h2 < rng_h1 &&
                    threadIdx_y < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p6; ll++)
                    {
                        sm_b[threadIdx_y][threadIdx_x + ll * FUSION_SIZE_TB_2_X] =
                            tmp_dev_d1_v2[(str_blk_idx_h2 + idx_h3 +
                                           (str_blk_idx_h1 + idx_h2 +
                                            (str_blk_idx_p6 + ll +
                                             (threadIdx_y + l) * base_size_p6b) *
                                            base_size_h1b) *
                                           base_size_h2b)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: -1
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_b[ll][idx_h2 + (idx_h1) * FUSION_SIZE_SLICE_2_H2 + 0];
                    temp_bv[1] = sm_b[ll][idx_h2 + (idx_h1) * FUSION_SIZE_SLICE_2_H2 + 16];
                    temp_bv[2] = sm_b[ll][idx_h2 + (idx_h1) * FUSION_SIZE_SLICE_2_H2 + 32];
                    temp_bv[3] = sm_b[ll][idx_h2 + (idx_h1) * FUSION_SIZE_SLICE_2_H2 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_a[ll][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_2_P4 + (xx * 16)];

                        reg_tile[0][xx] -= temp_av * temp_bv[0];
                        reg_tile[1][xx] -= temp_av * temp_bv[1];
                        reg_tile[2][xx] -= temp_av * temp_bv[2];
                        reg_tile[3][xx] -= temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }
    }

    //  d2-top: sd2_7, 8 and 9
#pragma unroll 1
    for (int iter_nvab = 0; iter_nvab < size_nvab; iter_nvab++)
    {
        //
        int flag_d2_7 = const_df_d2_exec[6 + (iter_nvab) * NUM_D2_EQUATIONS];
        int flag_d2_8 = const_df_d2_exec[7 + (iter_nvab) * NUM_D2_EQUATIONS];
        int flag_d2_9 = const_df_d2_exec[8 + (iter_nvab) * NUM_D2_EQUATIONS];

        //
        base_size_h1b = const_df_d2_size[0 + (iter_nvab) * NUM_D2_INDEX];
        base_size_h2b = const_df_d2_size[1 + (iter_nvab) * NUM_D2_INDEX];
        base_size_h3b = const_df_d2_size[2 + (iter_nvab) * NUM_D2_INDEX];
        base_size_p4b = const_df_d2_size[3 + (iter_nvab) * NUM_D2_INDEX];
        base_size_p5b = const_df_d2_size[4 + (iter_nvab) * NUM_D2_INDEX];
        base_size_p6b = const_df_d2_size[5 + (iter_nvab) * NUM_D2_INDEX];
        base_size_p7b = const_df_d2_size[6 + (iter_nvab) * NUM_D2_INDEX];

        //
        num_blks_h1b = CEIL(base_size_h1b, FUSION_SIZE_SLICE_1_H1);
        num_blks_h2b = CEIL(base_size_h2b, FUSION_SIZE_SLICE_1_H2);
        num_blks_h3b = CEIL(base_size_h3b, FUSION_SIZE_SLICE_1_H3);
        num_blks_p4b = CEIL(base_size_p4b, FUSION_SIZE_SLICE_1_P4);
        num_blks_p5b = CEIL(base_size_p5b, FUSION_SIZE_SLICE_1_P5);
        num_blks_p6b = CEIL(base_size_p6b, FUSION_SIZE_SLICE_1_P6);

        // 	(2) blk_idx_h/p*b
        blk_idx_p4b =
            blockIdx_x / (num_blks_h3b * num_blks_h2b * num_blks_h1b *
                                     num_blks_p6b * num_blks_p5b);
        tmp_blkIdx =
            blockIdx_x % (num_blks_h3b * num_blks_h2b * num_blks_h1b *
                                     num_blks_p6b * num_blks_p5b);
        blk_idx_p5b     = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b);
        tmp_blkIdx  	= (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b);
        blk_idx_p6b     = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b * num_blks_h1b);
        tmp_blkIdx 		= (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b * num_blks_h1b);
        blk_idx_h1b     = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b);
        tmp_blkIdx 		= (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b);
        blk_idx_h2b 	= (tmp_blkIdx) / (num_blks_h3b);
        blk_idx_h3b = blockIdx_x % (num_blks_h3b);

        // 	(3) str_blk_idx_h/p*
        str_blk_idx_h3 	= blk_idx_h3b * FUSION_SIZE_SLICE_1_H3;
        str_blk_idx_h2 	= blk_idx_h2b * FUSION_SIZE_SLICE_1_H2;
        str_blk_idx_h1 	= blk_idx_h1b * FUSION_SIZE_SLICE_1_H1;
        str_blk_idx_p6 	= blk_idx_p6b * FUSION_SIZE_SLICE_1_P6;
        str_blk_idx_p5 	= blk_idx_p5b * FUSION_SIZE_SLICE_1_P5;
        str_blk_idx_p4 	= blk_idx_p4b * FUSION_SIZE_SLICE_1_P4;

        // 	(4) rng_h/p*
        if ((base_size_h3b - (str_blk_idx_h3)) >= FUSION_SIZE_SLICE_1_H3)
            rng_h3 = FUSION_SIZE_SLICE_1_H3;
        else
            rng_h3 = base_size_h3b % FUSION_SIZE_SLICE_1_H3;

        if ((base_size_h2b - (str_blk_idx_h2)) >= FUSION_SIZE_SLICE_1_H2)
            rng_h2 = FUSION_SIZE_SLICE_1_H2;
        else
            rng_h2 = base_size_h2b % FUSION_SIZE_SLICE_1_H2;

        if ((base_size_h1b - (str_blk_idx_h1)) >= FUSION_SIZE_SLICE_1_H1)
            rng_h1 = FUSION_SIZE_SLICE_1_H1;
        else
            rng_h1 = base_size_h1b % FUSION_SIZE_SLICE_1_H1;

        if ((base_size_p6b - (str_blk_idx_p6)) >= FUSION_SIZE_SLICE_1_P6)
            rng_p6 = FUSION_SIZE_SLICE_1_P6;
        else
            rng_p6 = base_size_p6b % FUSION_SIZE_SLICE_1_P6;

        if ((base_size_p5b - (str_blk_idx_p5)) >= FUSION_SIZE_SLICE_1_P5)
            rng_p5 = FUSION_SIZE_SLICE_1_P5;
        else
            rng_p5 = base_size_p5b % FUSION_SIZE_SLICE_1_P5;

        if ((base_size_p4b - (str_blk_idx_p4)) >= FUSION_SIZE_SLICE_1_P4)
            rng_p4 = FUSION_SIZE_SLICE_1_P4;
        else
            rng_p4 = base_size_p4b % FUSION_SIZE_SLICE_1_P4;

        //	sd2_7
        if (flag_d2_7 >= 0)
        {
            //
            double* tmp_dev_d2_t2_7 = df_dev_d2_t2_all + size_max_dim_d2_t2 * flag_d2_7;//const_list_d2_flags_offset[local_offset];
            double* tmp_dev_d2_v2_7 = df_dev_d2_v2_all + size_max_dim_d2_v2 * flag_d2_7;//const_list_d2_flags_offset[local_offset];

            //	sd2_7
            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_p7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_p7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_h1 && idx_h1 < rng_h2 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p6; ll++)
                    {
                        sm_a[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_2_Y] =
                            tmp_dev_d2_t2_7[(blk_idx_p6b * FUSION_SIZE_SLICE_2_P6 + ll +
                                             (str_blk_idx_h1 + idx_p6 +
                                              (str_blk_idx_h2 + idx_h1) * base_size_h1b) *
                                             base_size_p6b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_p6 < rng_h3 && idx_h1 < rng_p4 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_b[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_2_Y] =
                            tmp_dev_d2_v2_7[(str_blk_idx_h3 + idx_p6 +
                                             (str_blk_idx_p5 + ll +
                                              (str_blk_idx_p4 + idx_h1) * base_size_p5b) *
                                             base_size_h3b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: 16
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_a[ll][idx_h1 + (idx_h2) * FUSION_SIZE_SLICE_2_H1 + 0];
                    temp_bv[1] = sm_a[ll][idx_h1 + (idx_h2) * FUSION_SIZE_SLICE_2_H1 + 16];
                    temp_bv[2] = sm_a[ll][idx_h1 + (idx_h2) * FUSION_SIZE_SLICE_2_H1 + 32];
                    temp_bv[3] = sm_a[ll][idx_h1 + (idx_h2) * FUSION_SIZE_SLICE_2_H1 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_b[ll][idx_h3 + (idx_p6) * FUSION_SIZE_SLICE_2_H3 + (xx * 16)];

                        reg_tile[0][xx] -= temp_av * temp_bv[0];
                        reg_tile[1][xx] -= temp_av * temp_bv[1];
                        reg_tile[2][xx] -= temp_av * temp_bv[2];
                        reg_tile[3][xx] -= temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        // 	sd2_8
        if (flag_d2_8 >= 0)
        {
            //
            double* tmp_dev_d2_t2_8 = df_dev_d2_t2_all + size_max_dim_d2_t2 * flag_d2_8;//const_list_d2_flags_offset[local_offset];
            double* tmp_dev_d2_v2_8 = df_dev_d2_v2_all + size_max_dim_d2_v2 * flag_d2_8;//const_list_d2_flags_offset[local_offset];

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_p7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                // internal_offset = (l + FUSION_SIZE_INT_UNIT) - size_internal;
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_p7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_h2 && idx_h1 < rng_h3 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p6; ll++)
                    {
                        sm_a[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_2_Y] =
                            tmp_dev_d2_t2_8[(str_blk_idx_p6 + ll +
                                             (str_blk_idx_h2 + idx_p6 +
                                              (str_blk_idx_h3 + idx_h1) * base_size_h2b) *
                                             base_size_p6b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_p6 < rng_h1 && idx_h1 < rng_p4 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_b[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_2_Y] =
                            tmp_dev_d2_v2_8[(str_blk_idx_h1 + idx_p6 +
                                             (str_blk_idx_p5 + ll +
                                              (str_blk_idx_p4 + idx_h1) * base_size_p5b) *
                                             base_size_h1b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: 16
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_a[ll][idx_h2 + (idx_h3) * FUSION_SIZE_SLICE_2_H2 + 0];
                    temp_bv[1] = sm_a[ll][idx_h2 + (idx_h3) * FUSION_SIZE_SLICE_2_H2 + 16];
                    temp_bv[2] = sm_a[ll][idx_h2 + (idx_h3) * FUSION_SIZE_SLICE_2_H2 + 32];
                    temp_bv[3] = sm_a[ll][idx_h2 + (idx_h3) * FUSION_SIZE_SLICE_2_H2 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_b[ll][idx_h1 + (idx_p6) * FUSION_SIZE_SLICE_2_H1 + (xx * 16)];

                        reg_tile[0][xx] -= temp_av * temp_bv[0];
                        reg_tile[1][xx] -= temp_av * temp_bv[1];
                        reg_tile[2][xx] -= temp_av * temp_bv[2];
                        reg_tile[3][xx] -= temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        // 	sd2_9
        if (flag_d2_9 >= 0)
        {
            //
            double* tmp_dev_d2_t2_9 = df_dev_d2_t2_all + size_max_dim_d2_t2 * flag_d2_9;//const_list_d2_flags_offset[local_offset];
            double* tmp_dev_d2_v2_9 = df_dev_d2_v2_all + size_max_dim_d2_v2 * flag_d2_9;//const_list_d2_flags_offset[local_offset];

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_p7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                // internal_offset = (l + FUSION_SIZE_INT_UNIT) - size_internal;
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_p7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_h1 && idx_h1 < rng_h3 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p6; ll++)
                    {
                        sm_a[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_2_Y] =
                            tmp_dev_d2_t2_9[(str_blk_idx_p6 + ll +
                                             (str_blk_idx_h1 + idx_p6 +
                                              (str_blk_idx_h3 + idx_h1) * base_size_h1b) *
                                             base_size_p6b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_p6 < rng_h2 && idx_h1 < rng_p4 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_b[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_2_Y] =
                            tmp_dev_d2_v2_9[(str_blk_idx_h2 + idx_p6 +
                                             (str_blk_idx_p5 + ll +
                                              (str_blk_idx_p4 + idx_h1) * base_size_p5b) *
                                             base_size_h2b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: 16
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_a[ll][idx_h1 + (idx_h3) * FUSION_SIZE_SLICE_2_H1 + 0];
                    temp_bv[1] = sm_a[ll][idx_h1 + (idx_h3) * FUSION_SIZE_SLICE_2_H1 + 16];
                    temp_bv[2] = sm_a[ll][idx_h1 + (idx_h3) * FUSION_SIZE_SLICE_2_H1 + 32];
                    temp_bv[3] = sm_a[ll][idx_h1 + (idx_h3) * FUSION_SIZE_SLICE_2_H1 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_b[ll][idx_h2 + (idx_p6) * FUSION_SIZE_SLICE_2_H2 + (xx * 16)];

                        reg_tile[0][xx] += temp_av * temp_bv[0];
                        reg_tile[1][xx] += temp_av * temp_bv[1];
                        reg_tile[2][xx] += temp_av * temp_bv[2];
                        reg_tile[3][xx] += temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }
    }

    //
    //  Register Transpose (top - bottom)
    //
    {
        if (threadIdx_y < 4) // 0, 1, 2, 3
        {
            // sm_a[16][64] <-- (4 x 16) x (4 x 4) = (16 x 64)		  'y''x'
            sm_a[0 + threadIdx_y * 4][threadIdx_x] = reg_tile[0][0];
            sm_a[1 + threadIdx_y * 4][threadIdx_x] = reg_tile[1][0];
            sm_a[2 + threadIdx_y * 4][threadIdx_x] = reg_tile[2][0];
            sm_a[3 + threadIdx_y * 4][threadIdx_x] = reg_tile[3][0];

            sm_a[0 + threadIdx_y * 4][threadIdx_x + 16] = reg_tile[0][1];
            sm_a[1 + threadIdx_y * 4][threadIdx_x + 16] = reg_tile[1][1];
            sm_a[2 + threadIdx_y * 4][threadIdx_x + 16] = reg_tile[2][1];
            sm_a[3 + threadIdx_y * 4][threadIdx_x + 16] = reg_tile[3][1];

            sm_a[0 + threadIdx_y * 4][threadIdx_x + 32] = reg_tile[0][2];
            sm_a[1 + threadIdx_y * 4][threadIdx_x + 32] = reg_tile[1][2];
            sm_a[2 + threadIdx_y * 4][threadIdx_x + 32] = reg_tile[2][2];
            sm_a[3 + threadIdx_y * 4][threadIdx_x + 32] = reg_tile[3][2];

            sm_a[0 + threadIdx_y * 4][threadIdx_x + 48] = reg_tile[0][3];
            sm_a[1 + threadIdx_y * 4][threadIdx_x + 48] = reg_tile[1][3];
            sm_a[2 + threadIdx_y * 4][threadIdx_x + 48] = reg_tile[2][3];
            sm_a[3 + threadIdx_y * 4][threadIdx_x + 48] = reg_tile[3][3];
        }

        if (threadIdx_y >= 4 && threadIdx_y < 8) // 4, 5, 6, 7
        {
            sm_b[0 + (threadIdx_y - 4) * 4][threadIdx_x] = reg_tile[0][0];
            sm_b[1 + (threadIdx_y - 4) * 4][threadIdx_x] = reg_tile[1][0];
            sm_b[2 + (threadIdx_y - 4) * 4][threadIdx_x] = reg_tile[2][0];
            sm_b[3 + (threadIdx_y - 4) * 4][threadIdx_x] = reg_tile[3][0];

            sm_b[0 + (threadIdx_y - 4) * 4][threadIdx_x + 16] = reg_tile[0][1];
            sm_b[1 + (threadIdx_y - 4) * 4][threadIdx_x + 16] = reg_tile[1][1];
            sm_b[2 + (threadIdx_y - 4) * 4][threadIdx_x + 16] = reg_tile[2][1];
            sm_b[3 + (threadIdx_y - 4) * 4][threadIdx_x + 16] = reg_tile[3][1];

            sm_b[0 + (threadIdx_y - 4) * 4][threadIdx_x + 32] = reg_tile[0][2];
            sm_b[1 + (threadIdx_y - 4) * 4][threadIdx_x + 32] = reg_tile[1][2];
            sm_b[2 + (threadIdx_y - 4) * 4][threadIdx_x + 32] = reg_tile[2][2];
            sm_b[3 + (threadIdx_y - 4) * 4][threadIdx_x + 32] = reg_tile[3][2];

            sm_b[0 + (threadIdx_y - 4) * 4][threadIdx_x + 48] = reg_tile[0][3];
            sm_b[1 + (threadIdx_y - 4) * 4][threadIdx_x + 48] = reg_tile[1][3];
            sm_b[2 + (threadIdx_y - 4) * 4][threadIdx_x + 48] = reg_tile[2][3];
            sm_b[3 + (threadIdx_y - 4) * 4][threadIdx_x + 48] = reg_tile[3][3];
        }
        item.barrier(sycl::access::fence_space::local_space);

        if (threadIdx_y < 4) // 0, 1, 2, 3
        {
            reg_tile[0][0] = sm_a[threadIdx_y + 0][(threadIdx_x)];
            reg_tile[1][0] = sm_a[threadIdx_y + 4][(threadIdx_x)];
            reg_tile[2][0] = sm_a[threadIdx_y + 8][(threadIdx_x)];
            reg_tile[3][0] = sm_a[threadIdx_y + 12][(threadIdx_x)];

            reg_tile[0][1] = sm_a[threadIdx_y + 0][(threadIdx_x) + 16];
            reg_tile[1][1] = sm_a[threadIdx_y + 4][(threadIdx_x) + 16];
            reg_tile[2][1] = sm_a[threadIdx_y + 8][(threadIdx_x) + 16];
            reg_tile[3][1] = sm_a[threadIdx_y + 12][(threadIdx_x) + 16];

            reg_tile[0][2] = sm_a[threadIdx_y + 0][(threadIdx_x) + 32];
            reg_tile[1][2] = sm_a[threadIdx_y + 4][(threadIdx_x) + 32];
            reg_tile[2][2] = sm_a[threadIdx_y + 8][(threadIdx_x) + 32];
            reg_tile[3][2] = sm_a[threadIdx_y + 12][(threadIdx_x) + 32];

            reg_tile[0][3] = sm_a[threadIdx_y + 0][(threadIdx_x) + 48];
            reg_tile[1][3] = sm_a[threadIdx_y + 4][(threadIdx_x) + 48];
            reg_tile[2][3] = sm_a[threadIdx_y + 8][(threadIdx_x) + 48];
            reg_tile[3][3] = sm_a[threadIdx_y + 12][(threadIdx_x) + 48];
        }

        if (threadIdx_y >= 4 && threadIdx_y < 8) // 4, 5, 6, 7
        {
            reg_tile[0][0] = sm_b[(threadIdx_y - 4) + 0][(threadIdx_x)];
            reg_tile[1][0] = sm_b[(threadIdx_y - 4) + 4][(threadIdx_x)];
            reg_tile[2][0] = sm_b[(threadIdx_y - 4) + 8][(threadIdx_x)];
            reg_tile[3][0] = sm_b[(threadIdx_y - 4) + 12][(threadIdx_x)];

            reg_tile[0][1] = sm_b[(threadIdx_y - 4) + 0][(threadIdx_x) + 16];
            reg_tile[1][1] = sm_b[(threadIdx_y - 4) + 4][(threadIdx_x) + 16];
            reg_tile[2][1] = sm_b[(threadIdx_y - 4) + 8][(threadIdx_x) + 16];
            reg_tile[3][1] = sm_b[(threadIdx_y - 4) + 12][(threadIdx_x) + 16];

            reg_tile[0][2] = sm_b[(threadIdx_y - 4) + 0][(threadIdx_x) + 32];
            reg_tile[1][2] = sm_b[(threadIdx_y - 4) + 4][(threadIdx_x) + 32];
            reg_tile[2][2] = sm_b[(threadIdx_y - 4) + 8][(threadIdx_x) + 32];
            reg_tile[3][2] = sm_b[(threadIdx_y - 4) + 12][(threadIdx_x) + 32];

            reg_tile[0][3] = sm_b[(threadIdx_y - 4) + 0][(threadIdx_x) + 48];
            reg_tile[1][3] = sm_b[(threadIdx_y - 4) + 4][(threadIdx_x) + 48];
            reg_tile[2][3] = sm_b[(threadIdx_y - 4) + 8][(threadIdx_x) + 48];
            reg_tile[3][3] = sm_b[(threadIdx_y - 4) + 12][(threadIdx_x) + 48];
        }
        item.barrier(sycl::access::fence_space::local_space);

        if (threadIdx_y >= 8 && threadIdx_y < 12) // 8, 9, 10, 11
        {
            sm_a[0 + (threadIdx_y - 8) * 4][threadIdx_x] = reg_tile[0][0];
            sm_a[1 + (threadIdx_y - 8) * 4][threadIdx_x] = reg_tile[1][0];
            sm_a[2 + (threadIdx_y - 8) * 4][threadIdx_x] = reg_tile[2][0];
            sm_a[3 + (threadIdx_y - 8) * 4][threadIdx_x] = reg_tile[3][0];

            sm_a[0 + (threadIdx_y - 8) * 4][threadIdx_x + 16] = reg_tile[0][1];
            sm_a[1 + (threadIdx_y - 8) * 4][threadIdx_x + 16] = reg_tile[1][1];
            sm_a[2 + (threadIdx_y - 8) * 4][threadIdx_x + 16] = reg_tile[2][1];
            sm_a[3 + (threadIdx_y - 8) * 4][threadIdx_x + 16] = reg_tile[3][1];

            sm_a[0 + (threadIdx_y - 8) * 4][threadIdx_x + 32] = reg_tile[0][2];
            sm_a[1 + (threadIdx_y - 8) * 4][threadIdx_x + 32] = reg_tile[1][2];
            sm_a[2 + (threadIdx_y - 8) * 4][threadIdx_x + 32] = reg_tile[2][2];
            sm_a[3 + (threadIdx_y - 8) * 4][threadIdx_x + 32] = reg_tile[3][2];

            sm_a[0 + (threadIdx_y - 8) * 4][threadIdx_x + 48] = reg_tile[0][3];
            sm_a[1 + (threadIdx_y - 8) * 4][threadIdx_x + 48] = reg_tile[1][3];
            sm_a[2 + (threadIdx_y - 8) * 4][threadIdx_x + 48] = reg_tile[2][3];
            sm_a[3 + (threadIdx_y - 8) * 4][threadIdx_x + 48] = reg_tile[3][3];
        }

        if (threadIdx_y >= 12) // 12, 13, 14, 15
        {
            sm_b[0 + (threadIdx_y - 12) * 4][threadIdx_x] = reg_tile[0][0];
            sm_b[1 + (threadIdx_y - 12) * 4][threadIdx_x] = reg_tile[1][0];
            sm_b[2 + (threadIdx_y - 12) * 4][threadIdx_x] = reg_tile[2][0];
            sm_b[3 + (threadIdx_y - 12) * 4][threadIdx_x] = reg_tile[3][0];

            sm_b[0 + (threadIdx_y - 12) * 4][threadIdx_x + 16] = reg_tile[0][1];
            sm_b[1 + (threadIdx_y - 12) * 4][threadIdx_x + 16] = reg_tile[1][1];
            sm_b[2 + (threadIdx_y - 12) * 4][threadIdx_x + 16] = reg_tile[2][1];
            sm_b[3 + (threadIdx_y - 12) * 4][threadIdx_x + 16] = reg_tile[3][1];

            sm_b[0 + (threadIdx_y - 12) * 4][threadIdx_x + 32] = reg_tile[0][2];
            sm_b[1 + (threadIdx_y - 12) * 4][threadIdx_x + 32] = reg_tile[1][2];
            sm_b[2 + (threadIdx_y - 12) * 4][threadIdx_x + 32] = reg_tile[2][2];
            sm_b[3 + (threadIdx_y - 12) * 4][threadIdx_x + 32] = reg_tile[3][2];

            sm_b[0 + (threadIdx_y - 12) * 4][threadIdx_x + 48] = reg_tile[0][3];
            sm_b[1 + (threadIdx_y - 12) * 4][threadIdx_x + 48] = reg_tile[1][3];
            sm_b[2 + (threadIdx_y - 12) * 4][threadIdx_x + 48] = reg_tile[2][3];
            sm_b[3 + (threadIdx_y - 12) * 4][threadIdx_x + 48] = reg_tile[3][3];
        }
        item.barrier(sycl::access::fence_space::local_space);

        if (threadIdx_y >= 8 && threadIdx_y < 12) // 8, 9, 10, 11
        {
            reg_tile[0][0] = sm_a[(threadIdx_y - 8) + 0][(threadIdx_x)];
            reg_tile[1][0] = sm_a[(threadIdx_y - 8) + 4][(threadIdx_x)];
            reg_tile[2][0] = sm_a[(threadIdx_y - 8) + 8][(threadIdx_x)];
            reg_tile[3][0] = sm_a[(threadIdx_y - 8) + 12][(threadIdx_x)];

            reg_tile[0][1] = sm_a[(threadIdx_y - 8) + 0][(threadIdx_x) + 16];
            reg_tile[1][1] = sm_a[(threadIdx_y - 8) + 4][(threadIdx_x) + 16];
            reg_tile[2][1] = sm_a[(threadIdx_y - 8) + 8][(threadIdx_x) + 16];
            reg_tile[3][1] = sm_a[(threadIdx_y - 8) + 12][(threadIdx_x) + 16];

            reg_tile[0][2] = sm_a[(threadIdx_y - 8) + 0][(threadIdx_x) + 32];
            reg_tile[1][2] = sm_a[(threadIdx_y - 8) + 4][(threadIdx_x) + 32];
            reg_tile[2][2] = sm_a[(threadIdx_y - 8) + 8][(threadIdx_x) + 32];
            reg_tile[3][2] = sm_a[(threadIdx_y - 8) + 12][(threadIdx_x) + 32];

            reg_tile[0][3] = sm_a[(threadIdx_y - 8) + 0][(threadIdx_x) + 48];
            reg_tile[1][3] = sm_a[(threadIdx_y - 8) + 4][(threadIdx_x) + 48];
            reg_tile[2][3] = sm_a[(threadIdx_y - 8) + 8][(threadIdx_x) + 48];
            reg_tile[3][3] = sm_a[(threadIdx_y - 8) + 12][(threadIdx_x) + 48];
        }

        if (threadIdx_y >= 12) // 12, 13, 14, 15
        {
            reg_tile[0][0] = sm_b[(threadIdx_y - 12) + 0][(threadIdx_x)];
            reg_tile[1][0] = sm_b[(threadIdx_y - 12) + 4][(threadIdx_x)];
            reg_tile[2][0] = sm_b[(threadIdx_y - 12) + 8][(threadIdx_x)];
            reg_tile[3][0] = sm_b[(threadIdx_y - 12) + 12][(threadIdx_x)];

            reg_tile[0][1] = sm_b[(threadIdx_y - 12) + 0][(threadIdx_x) + 16];
            reg_tile[1][1] = sm_b[(threadIdx_y - 12) + 4][(threadIdx_x) + 16];
            reg_tile[2][1] = sm_b[(threadIdx_y - 12) + 8][(threadIdx_x) + 16];
            reg_tile[3][1] = sm_b[(threadIdx_y - 12) + 12][(threadIdx_x) + 16];

            reg_tile[0][2] = sm_b[(threadIdx_y - 12) + 0][(threadIdx_x) + 32];
            reg_tile[1][2] = sm_b[(threadIdx_y - 12) + 4][(threadIdx_x) + 32];
            reg_tile[2][2] = sm_b[(threadIdx_y - 12) + 8][(threadIdx_x) + 32];
            reg_tile[3][2] = sm_b[(threadIdx_y - 12) + 12][(threadIdx_x) + 32];

            reg_tile[0][3] = sm_b[(threadIdx_y - 12) + 0][(threadIdx_x) + 48];
            reg_tile[1][3] = sm_b[(threadIdx_y - 12) + 4][(threadIdx_x) + 48];
            reg_tile[2][3] = sm_b[(threadIdx_y - 12) + 8][(threadIdx_x) + 48];
            reg_tile[3][3] = sm_b[(threadIdx_y - 12) + 12][(threadIdx_x) + 48];
        }
        item.barrier(sycl::access::fence_space::local_space);
    }   // 	End of Register Transpose

    //
    // 	based on "noab"
    //  d1-bottom: sd1_4, 5 , 6 , 7 , 8 and 9.
    //
#pragma unroll 1
    for (int iter_noab = 0; iter_noab < size_noab; iter_noab++)
    {
        // 	flags
        int flag_d1_4 = const_df_d1_exec[3 + (iter_noab) * NUM_D1_EQUATIONS];
        int flag_d1_5 = const_df_d1_exec[4 + (iter_noab) * NUM_D1_EQUATIONS];
        int flag_d1_6 = const_df_d1_exec[5 + (iter_noab) * NUM_D1_EQUATIONS];
        int flag_d1_7 = const_df_d1_exec[6 + (iter_noab) * NUM_D1_EQUATIONS];
        int flag_d1_8 = const_df_d1_exec[7 + (iter_noab) * NUM_D1_EQUATIONS];
        int flag_d1_9 = const_df_d1_exec[8 + (iter_noab) * NUM_D1_EQUATIONS];

        base_size_h1b = const_df_d1_size[0 + (iter_noab) * NUM_D1_INDEX];
        base_size_h2b = const_df_d1_size[1 + (iter_noab) * NUM_D1_INDEX];
        base_size_h3b = const_df_d1_size[2 + (iter_noab) * NUM_D1_INDEX];
        base_size_h7b = const_df_d1_size[3 + (iter_noab) * NUM_D1_INDEX];
        base_size_p4b = const_df_d1_size[4 + (iter_noab) * NUM_D1_INDEX];
        base_size_p5b = const_df_d1_size[5 + (iter_noab) * NUM_D1_INDEX];
        base_size_p6b = const_df_d1_size[6 + (iter_noab) * NUM_D1_INDEX];

        //
        num_blks_h1b = CEIL(base_size_h1b, FUSION_SIZE_SLICE_1_H1);
        num_blks_h2b = CEIL(base_size_h2b, FUSION_SIZE_SLICE_1_H2);
        num_blks_h3b = CEIL(base_size_h3b, FUSION_SIZE_SLICE_1_H3);
        num_blks_p4b = CEIL(base_size_p4b, FUSION_SIZE_SLICE_1_P4);
        num_blks_p5b = CEIL(base_size_p5b, FUSION_SIZE_SLICE_1_P5);
        num_blks_p6b = CEIL(base_size_p6b, FUSION_SIZE_SLICE_1_P6);

        // 	(2) blk_idx_h/p*b
        blk_idx_p4b =
            blockIdx_x / (num_blks_h3b * num_blks_h2b * num_blks_h1b *
                                     num_blks_p6b * num_blks_p5b);
        tmp_blkIdx =
            blockIdx_x % (num_blks_h3b * num_blks_h2b * num_blks_h1b *
                                     num_blks_p6b * num_blks_p5b);
        blk_idx_p5b = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b);
        tmp_blkIdx  = (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b);
        blk_idx_p6b = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b * num_blks_h1b);
        tmp_blkIdx  = (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b * num_blks_h1b);
        blk_idx_h1b = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b);
        tmp_blkIdx  = (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b);
        blk_idx_h2b = (tmp_blkIdx) / (num_blks_h3b);
        blk_idx_h3b = blockIdx_x % (num_blks_h3b);

        // 	(3) str_blk_idx_h/p*
        str_blk_idx_h3 = blk_idx_h3b * FUSION_SIZE_SLICE_1_H3;
        str_blk_idx_h2 = blk_idx_h2b * FUSION_SIZE_SLICE_1_H2;
        str_blk_idx_h1 = blk_idx_h1b * FUSION_SIZE_SLICE_1_H1;
        str_blk_idx_p6 = blk_idx_p6b * FUSION_SIZE_SLICE_1_P6;
        str_blk_idx_p5 = blk_idx_p5b * FUSION_SIZE_SLICE_1_P5;
        str_blk_idx_p4 = blk_idx_p4b * FUSION_SIZE_SLICE_1_P4;

        // 	(4) rng_h/p*
        if ((base_size_h3b - (str_blk_idx_h3)) >= FUSION_SIZE_SLICE_1_H3)
            rng_h3 = FUSION_SIZE_SLICE_1_H3;
        else
            rng_h3 = base_size_h3b % FUSION_SIZE_SLICE_1_H3;

        if ((base_size_h2b - (str_blk_idx_h2)) >= FUSION_SIZE_SLICE_1_H2)
            rng_h2 = FUSION_SIZE_SLICE_1_H2;
        else
            rng_h2 = base_size_h2b % FUSION_SIZE_SLICE_1_H2;

        if ((base_size_h1b - (str_blk_idx_h1)) >= FUSION_SIZE_SLICE_1_H1)
            rng_h1 = FUSION_SIZE_SLICE_1_H1;
        else
            rng_h1 = base_size_h1b % FUSION_SIZE_SLICE_1_H1;

        if ((base_size_p6b - (str_blk_idx_p6)) >= FUSION_SIZE_SLICE_1_P6)
            rng_p6 = FUSION_SIZE_SLICE_1_P6;
        else
            rng_p6 = base_size_p6b % FUSION_SIZE_SLICE_1_P6;

        if ((base_size_p5b - (str_blk_idx_p5)) >= FUSION_SIZE_SLICE_1_P5)
            rng_p5 = FUSION_SIZE_SLICE_1_P5;
        else
            rng_p5 = base_size_p5b % FUSION_SIZE_SLICE_1_P5;

        if ((base_size_p4b - (str_blk_idx_p4)) >= FUSION_SIZE_SLICE_1_P4)
            rng_p4 = FUSION_SIZE_SLICE_1_P4;
        else
            rng_p4 = base_size_p4b % FUSION_SIZE_SLICE_1_P4;

        // 	sd1_4
        if (flag_d1_4 >= 0)
        {
            //
            double* tmp_dev_d1_t2_4 = df_dev_d1_t2_all + size_max_dim_d1_t2 * flag_d1_4;
            double* tmp_dev_d1_v2_4 = df_dev_d1_v2_all + size_max_dim_d1_v2 * flag_d1_4;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_h7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_h7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_p6 && idx_h1 < rng_h1 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_a[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d1_t2_4[(str_blk_idx_p5 + ll +
                                             (str_blk_idx_p6 + idx_p6 +
                                              (str_blk_idx_h1 + idx_h1) * base_size_p6b) *
                                             base_size_p5b) *
                                            base_size_h7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_h3 < rng_h3 && idx_h2 < rng_h2 &&
                    threadIdx_y < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p4; ll++)
                    {
                        sm_b[threadIdx_y][threadIdx_x + ll * FUSION_SIZE_TB_1_X] =
                            tmp_dev_d1_v2_4[(str_blk_idx_h3 + idx_h3 +
                                             (str_blk_idx_h2 + idx_h2 +
                                              (str_blk_idx_p4 + ll +
                                               (threadIdx_y + l) * base_size_p4b) *
                                              base_size_h2b) *
                                             base_size_h3b)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: -1
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_b[ll][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_H3 + 0];
                    temp_bv[1] = sm_b[ll][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_H3 + 16];
                    temp_bv[2] = sm_b[ll][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_H3 + 32];
                    temp_bv[3] = sm_b[ll][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_H3 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_a[ll][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6 + (xx * 16)];

                        reg_tile[0][xx] -= temp_av * temp_bv[0];
                        reg_tile[1][xx] -= temp_av * temp_bv[1];
                        reg_tile[2][xx] -= temp_av * temp_bv[2];
                        reg_tile[3][xx] -= temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        // 	sd1_5
        if (flag_d1_5 >= 0)
        {
            //
            double* tmp_dev_d1_t2_5 = df_dev_d1_t2_all + size_max_dim_d1_t2 * flag_d1_5;
            double* tmp_dev_d1_v2_5 = df_dev_d1_v2_all + size_max_dim_d1_v2 * flag_d1_5;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_h7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                // internal_offset = (l + FUSION_SIZE_INT_UNIT) - size_internal;
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_h7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of Internal Indices: 1
                if (idx_p6 < rng_p6 && idx_h1 < rng_h2 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_a[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d1_t2_5[(str_blk_idx_p5 + ll +
                                             (str_blk_idx_p6 + idx_p6 +
                                              (str_blk_idx_h2 + idx_h1) * base_size_p6b) *
                                             base_size_p5b) *
                                            base_size_h7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_h3 < rng_h3 && idx_h2 < rng_h1 &&
                    threadIdx_y < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p4; ll++)
                    {
                        sm_b[threadIdx_y][threadIdx_x + ll * FUSION_SIZE_TB_1_X] =
                            tmp_dev_d1_v2_5[(str_blk_idx_h3 + idx_h3 +
                                             (str_blk_idx_h1 + idx_h2 +
                                              (str_blk_idx_p4 + ll +
                                               (threadIdx_y + l) * base_size_p4b) *
                                              base_size_h1b) *
                                             base_size_h3b)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: -1
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_b[ll][idx_h3 + (idx_h1) * FUSION_SIZE_SLICE_1_H3 + 0];
                    temp_bv[1] = sm_b[ll][idx_h3 + (idx_h1) * FUSION_SIZE_SLICE_1_H3 + 16];
                    temp_bv[2] = sm_b[ll][idx_h3 + (idx_h1) * FUSION_SIZE_SLICE_1_H3 + 32];
                    temp_bv[3] = sm_b[ll][idx_h3 + (idx_h1) * FUSION_SIZE_SLICE_1_H3 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_a[ll][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6 + (xx * 16)];

                        reg_tile[0][xx] += temp_av * temp_bv[0];
                        reg_tile[1][xx] += temp_av * temp_bv[1];
                        reg_tile[2][xx] += temp_av * temp_bv[2];
                        reg_tile[3][xx] += temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        // 	sd1_6
        if (flag_d1_6 >= 0)
        {
            //
            double* tmp_dev_d1_t2_6 = df_dev_d1_t2_all + size_max_dim_d1_t2 * flag_d1_6;
            double* tmp_dev_d1_v2_6 = df_dev_d1_v2_all + size_max_dim_d1_v2 * flag_d1_6;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_h7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                // internal_offset = (l + FUSION_SIZE_INT_UNIT) - size_internal;
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_h7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of Internal Indices: 1 //63, 21
                if (idx_p6 < rng_p6 && idx_h1 < rng_h3 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_a[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d1_t2_6[(str_blk_idx_p5 + ll +
                                             (str_blk_idx_p6 + idx_p6 +
                                              (str_blk_idx_h3 + idx_h1) * base_size_p6b) *
                                             base_size_p5b) *
                                            base_size_h7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_h3 < rng_h2 && idx_h2 < rng_h1 &&
                    threadIdx_y < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p4; ll++)
                    {
                        sm_b[threadIdx_y][threadIdx_x + ll * FUSION_SIZE_TB_1_X] =
                            tmp_dev_d1_v2_6[(str_blk_idx_h2 + idx_h3 +
                                             (str_blk_idx_h1 + idx_h2 +
                                              (str_blk_idx_p4 + ll +
                                               (threadIdx_y + l) * base_size_p4b) *
                                              base_size_h1b) *
                                             base_size_h2b)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: -1
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_b[ll][idx_h2 + (idx_h1) * FUSION_SIZE_SLICE_1_H2 + 0];
                    temp_bv[1] = sm_b[ll][idx_h2 + (idx_h1) * FUSION_SIZE_SLICE_1_H2 + 16];
                    temp_bv[2] = sm_b[ll][idx_h2 + (idx_h1) * FUSION_SIZE_SLICE_1_H2 + 32];
                    temp_bv[3] = sm_b[ll][idx_h2 + (idx_h1) * FUSION_SIZE_SLICE_1_H2 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_a[ll][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6 + (xx * 16)];

                        reg_tile[0][xx] -= temp_av * temp_bv[0];
                        reg_tile[1][xx] -= temp_av * temp_bv[1];
                        reg_tile[2][xx] -= temp_av * temp_bv[2];
                        reg_tile[3][xx] -= temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        // 	sd1_7
        if (flag_d1_7 >= 0)
        {
            //
            double* tmp_dev_d1_t2_7 = df_dev_d1_t2_all + size_max_dim_d1_t2 * flag_d1_7;
            double* tmp_dev_d1_v2_7 = df_dev_d1_v2_all + size_max_dim_d1_v2 * flag_d1_7;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_h7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_h7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of Internal Indices: 1
                if (idx_p6 < rng_p6 && idx_h1 < rng_h1 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p4; ll++)
                    {
                        sm_a[threadIdx_x][threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d1_t2_7[(str_blk_idx_p4 + ll +
                                             (str_blk_idx_p6 + idx_p6 +
                                              (str_blk_idx_h1 + idx_h1) * base_size_p6b) *
                                             base_size_p4b) *
                                            base_size_h7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_h3 < rng_h3 && idx_h2 < rng_h2 &&
                    threadIdx_y < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_b[threadIdx_y][threadIdx_x + ll * FUSION_SIZE_TB_1_X] =
                            tmp_dev_d1_v2_7[(str_blk_idx_h3 + idx_h3 +
                                             (str_blk_idx_h2 + idx_h2 +
                                              (str_blk_idx_p5 + ll +
                                               (threadIdx_y + l) * base_size_p5b) *
                                              base_size_h2b) *
                                             base_size_h3b)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: -1
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_a[ll][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6 + 0];
                    temp_bv[1] = sm_a[ll][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6 + 16];
                    temp_bv[2] = sm_a[ll][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6 + 32];
                    temp_bv[3] = sm_a[ll][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_b[ll][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_H3 + (xx * 16)];

                        reg_tile[0][xx] += temp_av * temp_bv[0];
                        reg_tile[1][xx] += temp_av * temp_bv[1];
                        reg_tile[2][xx] += temp_av * temp_bv[2];
                        reg_tile[3][xx] += temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        // 	sd1_8
        if (flag_d1_8 >= 0)
        {
            //
            double* tmp_dev_d1_t2_8 = df_dev_d1_t2_all + size_max_dim_d1_t2 * flag_d1_8;
            double* tmp_dev_d1_v2_8 = df_dev_d1_v2_all + size_max_dim_d1_v2 * flag_d1_8;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_h7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_h7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of Internal Indices: 1
                if (idx_p6 < rng_p6 && idx_h1 < rng_h2 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p4; ll++)
                    {
                        sm_a[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d1_t2_8[(str_blk_idx_p4 + ll +
                                             (str_blk_idx_p6 + idx_p6 +
                                              (str_blk_idx_h2 + idx_h1) * base_size_p6b) *
                                             base_size_p4b) *
                                            base_size_h7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_h3 < rng_h3 && idx_h2 < rng_h1 &&
                    threadIdx_y < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_b[threadIdx_y][threadIdx_x +
                                                       ll * FUSION_SIZE_TB_1_X] =
                            tmp_dev_d1_v2_8[(str_blk_idx_h3 + idx_h3 +
                                             (str_blk_idx_h1 + idx_h2 +
                                              (str_blk_idx_p5 + ll +
                                               (threadIdx_y + l) * base_size_p5b) *
                                              base_size_h1b) *
                                             base_size_h3b)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: -1
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_a[ll][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6 + 0];
                    temp_bv[1] = sm_a[ll][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6 + 16];
                    temp_bv[2] = sm_a[ll][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6 + 32];
                    temp_bv[3] = sm_a[ll][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_b[ll][idx_h3 + (idx_h1) * FUSION_SIZE_SLICE_1_H3 + (xx * 16)];

                        reg_tile[0][xx] -= temp_av * temp_bv[0];
                        reg_tile[1][xx] -= temp_av * temp_bv[1];
                        reg_tile[2][xx] -= temp_av * temp_bv[2];
                        reg_tile[3][xx] -= temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        // 	sd1_9
        if (flag_d1_9 >= 0)
        {
            //
            double* tmp_dev_d1_t2_9 = df_dev_d1_t2_all + size_max_dim_d1_t2 * flag_d1_9;
            double* tmp_dev_d1_v2_9 = df_dev_d1_v2_all + size_max_dim_d1_v2 * flag_d1_9;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_h7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_h7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of Internal Indices: 1
                if (idx_p6 < rng_p6 && idx_h1 < rng_h3 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p4; ll++)
                    {
                        sm_a[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d1_t2_9[(str_blk_idx_p4 + ll +
                                             (str_blk_idx_p6 + idx_p6 +
                                              (str_blk_idx_h3 + idx_h1) * base_size_p6b) *
                                             base_size_p4b) *
                                            base_size_h7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_h3 < rng_h2 && idx_h2 < rng_h1 &&
                    threadIdx_y < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_b[threadIdx_y][threadIdx_x +
                                                       ll * FUSION_SIZE_TB_1_X] =
                            tmp_dev_d1_v2_9[(str_blk_idx_h2 + idx_h3 +
                                             (str_blk_idx_h1 + idx_h2 +
                                              (str_blk_idx_p5 + ll +
                                               (threadIdx_y + l) * base_size_p5b) *
                                              base_size_h1b) *
                                             base_size_h2b)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: -1
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_a[ll][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6 + 0];
                    temp_bv[1] = sm_a[ll][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6 + 16];
                    temp_bv[2] = sm_a[ll][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6 + 32];
                    temp_bv[3] = sm_a[ll][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_b[ll][idx_h2 + (idx_h1) * FUSION_SIZE_SLICE_1_H2 + (xx * 16)];

                        reg_tile[0][xx] += temp_av * temp_bv[0];
                        reg_tile[1][xx] += temp_av * temp_bv[1];
                        reg_tile[2][xx] += temp_av * temp_bv[2];
                        reg_tile[3][xx] += temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }
    }

    //  d2-bottom: sd2_1, 2, 3, 4, 5 and 6.
#pragma unroll 1
    for (int iter_nvab = 0; iter_nvab < size_nvab; iter_nvab++)
    {
        //
        int flag_d2_1 = const_df_d2_exec[0 + (iter_nvab) * NUM_D2_EQUATIONS];
        int flag_d2_2 = const_df_d2_exec[1 + (iter_nvab) * NUM_D2_EQUATIONS];
        int flag_d2_3 = const_df_d2_exec[2 + (iter_nvab) * NUM_D2_EQUATIONS];
        int flag_d2_4 = const_df_d2_exec[3 + (iter_nvab) * NUM_D2_EQUATIONS];
        int flag_d2_5 = const_df_d2_exec[4 + (iter_nvab) * NUM_D2_EQUATIONS];
        int flag_d2_6 = const_df_d2_exec[5 + (iter_nvab) * NUM_D2_EQUATIONS];

        //
        base_size_h1b = const_df_d2_size[0 + (iter_nvab) * NUM_D2_INDEX];
        base_size_h2b = const_df_d2_size[1 + (iter_nvab) * NUM_D2_INDEX];
        base_size_h3b = const_df_d2_size[2 + (iter_nvab) * NUM_D2_INDEX];
        base_size_p4b = const_df_d2_size[3 + (iter_nvab) * NUM_D2_INDEX];
        base_size_p5b = const_df_d2_size[4 + (iter_nvab) * NUM_D2_INDEX];
        base_size_p6b = const_df_d2_size[5 + (iter_nvab) * NUM_D2_INDEX];
        base_size_p7b = const_df_d2_size[6 + (iter_nvab) * NUM_D2_INDEX];

        //
        num_blks_h1b = CEIL(base_size_h1b, FUSION_SIZE_SLICE_1_H1);
        num_blks_h2b = CEIL(base_size_h2b, FUSION_SIZE_SLICE_1_H2);
        num_blks_h3b = CEIL(base_size_h3b, FUSION_SIZE_SLICE_1_H3);
        num_blks_p4b = CEIL(base_size_p4b, FUSION_SIZE_SLICE_1_P4);
        num_blks_p5b = CEIL(base_size_p5b, FUSION_SIZE_SLICE_1_P5);
        num_blks_p6b = CEIL(base_size_p6b, FUSION_SIZE_SLICE_1_P6);

        // 	(2) blk_idx_h/p*b
        blk_idx_p4b =
            blockIdx_x / (num_blks_h3b * num_blks_h2b * num_blks_h1b *
                                     num_blks_p6b * num_blks_p5b);
        tmp_blkIdx =
            blockIdx_x % (num_blks_h3b * num_blks_h2b * num_blks_h1b *
                                     num_blks_p6b * num_blks_p5b);
        blk_idx_p5b     = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b);
        tmp_blkIdx  	= (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b);
        blk_idx_p6b     = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b * num_blks_h1b);
        tmp_blkIdx 		= (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b * num_blks_h1b);
        blk_idx_h1b     = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b);
        tmp_blkIdx 		= (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b);
        blk_idx_h2b 	= (tmp_blkIdx) / (num_blks_h3b);
        blk_idx_h3b = blockIdx_x % (num_blks_h3b);

        // 	(3) str_blk_idx_h/p*
        str_blk_idx_h3 	= blk_idx_h3b * FUSION_SIZE_SLICE_1_H3;
        str_blk_idx_h2 	= blk_idx_h2b * FUSION_SIZE_SLICE_1_H2;
        str_blk_idx_h1 	= blk_idx_h1b * FUSION_SIZE_SLICE_1_H1;
        str_blk_idx_p6 	= blk_idx_p6b * FUSION_SIZE_SLICE_1_P6;
        str_blk_idx_p5 	= blk_idx_p5b * FUSION_SIZE_SLICE_1_P5;
        str_blk_idx_p4 	= blk_idx_p4b * FUSION_SIZE_SLICE_1_P4;

        // 	(4) rng_h/p*
        if ((base_size_h3b - (str_blk_idx_h3)) >= FUSION_SIZE_SLICE_1_H3)
            rng_h3 = FUSION_SIZE_SLICE_1_H3;
        else
            rng_h3 = base_size_h3b % FUSION_SIZE_SLICE_1_H3;

        if ((base_size_h2b - (str_blk_idx_h2)) >= FUSION_SIZE_SLICE_1_H2)
            rng_h2 = FUSION_SIZE_SLICE_1_H2;
        else
            rng_h2 = base_size_h2b % FUSION_SIZE_SLICE_1_H2;

        if ((base_size_h1b - (str_blk_idx_h1)) >= FUSION_SIZE_SLICE_1_H1)
            rng_h1 = FUSION_SIZE_SLICE_1_H1;
        else
            rng_h1 = base_size_h1b % FUSION_SIZE_SLICE_1_H1;

        if ((base_size_p6b - (str_blk_idx_p6)) >= FUSION_SIZE_SLICE_1_P6)
            rng_p6 = FUSION_SIZE_SLICE_1_P6;
        else
            rng_p6 = base_size_p6b % FUSION_SIZE_SLICE_1_P6;

        if ((base_size_p5b - (str_blk_idx_p5)) >= FUSION_SIZE_SLICE_1_P5)
            rng_p5 = FUSION_SIZE_SLICE_1_P5;
        else
            rng_p5 = base_size_p5b % FUSION_SIZE_SLICE_1_P5;

        if ((base_size_p4b - (str_blk_idx_p4)) >= FUSION_SIZE_SLICE_1_P4)
            rng_p4 = FUSION_SIZE_SLICE_1_P4;
        else
            rng_p4 = base_size_p4b % FUSION_SIZE_SLICE_1_P4;

        //  sd2_1
        if (flag_d2_1 >= 0)
        {
            //
            double* tmp_dev_d2_t2_1 = df_dev_d2_t2_all + size_max_dim_d2_t2 * flag_d2_1;
            double* tmp_dev_d2_v2_1 = df_dev_d2_v2_all + size_max_dim_d2_v2 * flag_d2_1;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_p7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                // internal_offset = (l + FUSION_SIZE_INT_UNIT) - size_internal;
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_p7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_h1 && idx_h1 < rng_h2 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p4; ll++)
                    {
                        sm_a[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d2_t2_1[(str_blk_idx_p4 + ll +
                                             (str_blk_idx_h1 + idx_p6 +
                                              (str_blk_idx_h2 + idx_h1) * base_size_h1b) *
                                             base_size_p4b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_p6 < rng_h3 && idx_h1 < rng_p6 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_b[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d2_v2_1[(str_blk_idx_h3 + idx_p6 +
                                             (str_blk_idx_p6 + idx_h1 +
                                              (str_blk_idx_p5 + ll) * base_size_p6b) *
                                             base_size_h3b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: 16
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_a[ll][idx_h1 + (idx_h2) * FUSION_SIZE_SLICE_1_H1 + 0];
                    temp_bv[1] = sm_a[ll][idx_h1 + (idx_h2) * FUSION_SIZE_SLICE_1_H1 + 16];
                    temp_bv[2] = sm_a[ll][idx_h1 + (idx_h2) * FUSION_SIZE_SLICE_1_H1 + 32];
                    temp_bv[3] = sm_a[ll][idx_h1 + (idx_h2) * FUSION_SIZE_SLICE_1_H1 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_b[ll][idx_h3 + (idx_p6) * FUSION_SIZE_SLICE_1_H3 + (xx * 16)];

                        reg_tile[0][xx] -= temp_av * temp_bv[0];
                        reg_tile[1][xx] -= temp_av * temp_bv[1];
                        reg_tile[2][xx] -= temp_av * temp_bv[2];
                        reg_tile[3][xx] -= temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        // 	sd2_2
        if (flag_d2_2 >= 0)
        {
            //
            double* tmp_dev_d2_t2_2 = df_dev_d2_t2_all + size_max_dim_d2_t2 * flag_d2_2;
            double* tmp_dev_d2_v2_2 = df_dev_d2_v2_all + size_max_dim_d2_v2 * flag_d2_2;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_p7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_p7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_h2 && idx_h1 < rng_h3 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p4; ll++)
                    {
                        sm_a[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d2_t2_2[(str_blk_idx_p4 + ll +
                                             (str_blk_idx_h2 + idx_p6 +
                                              (str_blk_idx_h3 + idx_h1) * base_size_h2b) *
                                             base_size_p4b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_p6 < rng_h1 && idx_h1 < rng_p6 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_b[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d2_v2_2[(str_blk_idx_h1 + idx_p6 +
                                             (str_blk_idx_p6 + idx_h1 +
                                              (str_blk_idx_p5 + ll) * base_size_p6b) *
                                             base_size_h1b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: 16
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_a[ll][idx_h2 + (idx_h3) * FUSION_SIZE_SLICE_1_H2 + 0];
                    temp_bv[1] = sm_a[ll][idx_h2 + (idx_h3) * FUSION_SIZE_SLICE_1_H2 + 16];
                    temp_bv[2] = sm_a[ll][idx_h2 + (idx_h3) * FUSION_SIZE_SLICE_1_H2 + 32];
                    temp_bv[3] = sm_a[ll][idx_h2 + (idx_h3) * FUSION_SIZE_SLICE_1_H2 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_b[ll][idx_h1 + (idx_p6) * FUSION_SIZE_SLICE_1_H1 + (xx * 16)];

                        reg_tile[0][xx] -= temp_av * temp_bv[0];
                        reg_tile[1][xx] -= temp_av * temp_bv[1];
                        reg_tile[2][xx] -= temp_av * temp_bv[2];
                        reg_tile[3][xx] -= temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        // 	sd2_3
        if (flag_d2_3 >= 0)
        {
            //
            double* tmp_dev_d2_t2_3 = df_dev_d2_t2_all + size_max_dim_d2_t2 * flag_d2_3;
            double* tmp_dev_d2_v2_3 = df_dev_d2_v2_all + size_max_dim_d2_v2 * flag_d2_3;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_p7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_p7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_h1 && idx_h1 < rng_h3 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p4; ll++)
                    {
                        sm_a[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d2_t2_3[(str_blk_idx_p4 + ll +
                                             (str_blk_idx_h1 + idx_p6 +
                                              (str_blk_idx_h3 + idx_h1) * base_size_h1b) *
                                             base_size_p4b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_p6 < rng_h2 && idx_h1 < rng_p6 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_b[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d2_v2_3[(str_blk_idx_h2 + idx_p6 +
                                             (str_blk_idx_p6 + idx_h1 +
                                              (str_blk_idx_p5 + ll) * base_size_p6b) *
                                             base_size_h2b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: 16
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_a[ll][idx_h1 + (idx_h3) * FUSION_SIZE_SLICE_1_H1 + 0];
                    temp_bv[1] = sm_a[ll][idx_h1 + (idx_h3) * FUSION_SIZE_SLICE_1_H1 + 16];
                    temp_bv[2] = sm_a[ll][idx_h1 + (idx_h3) * FUSION_SIZE_SLICE_1_H1 + 32];
                    temp_bv[3] = sm_a[ll][idx_h1 + (idx_h3) * FUSION_SIZE_SLICE_1_H1 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_b[ll][idx_h2 + (idx_p6) * FUSION_SIZE_SLICE_1_H2 + (xx * 16)];

                        reg_tile[0][xx] += temp_av * temp_bv[0];
                        reg_tile[1][xx] += temp_av * temp_bv[1];
                        reg_tile[2][xx] += temp_av * temp_bv[2];
                        reg_tile[3][xx] += temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        // 	sd2_4
        if (flag_d2_4 >= 0)
        {
            //
            double* tmp_dev_d2_t2_4 = df_dev_d2_t2_all + size_max_dim_d2_t2 * flag_d2_4;
            double* tmp_dev_d2_v2_4 = df_dev_d2_v2_all + size_max_dim_d2_v2 * flag_d2_4;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_p7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_p7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_h1 && idx_h1 < rng_h2 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_a[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d2_t2_4[(str_blk_idx_p5 + ll +
                                             (str_blk_idx_h1 + idx_p6 +
                                              (str_blk_idx_h2 + idx_h1) * base_size_h1b) *
                                             base_size_p5b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_p6 < rng_h3 && idx_h1 < rng_p6 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p4; ll++)
                    {
                        sm_b[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d2_v2_4[(str_blk_idx_h3 + idx_p6 +
                                             (str_blk_idx_p6 + idx_h1 +
                                              (str_blk_idx_p4 + ll) * base_size_p6b) *
                                             base_size_h3b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: 16
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_b[ll][idx_h3 + (idx_p6) * FUSION_SIZE_SLICE_1_H1 + 0];
                    temp_bv[1] = sm_b[ll][idx_h3 + (idx_p6) * FUSION_SIZE_SLICE_1_H1 + 16];
                    temp_bv[2] = sm_b[ll][idx_h3 + (idx_p6) * FUSION_SIZE_SLICE_1_H1 + 32];
                    temp_bv[3] = sm_b[ll][idx_h3 + (idx_p6) * FUSION_SIZE_SLICE_1_H1 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_a[ll][idx_h1 + (idx_h2) * FUSION_SIZE_SLICE_1_H1 + (xx * 16)];

                        reg_tile[0][xx] += temp_av * temp_bv[0];
                        reg_tile[1][xx] += temp_av * temp_bv[1];
                        reg_tile[2][xx] += temp_av * temp_bv[2];
                        reg_tile[3][xx] += temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        // 	sd2_5
        if (flag_d2_5 >= 0)
        {
            //
            double* tmp_dev_d2_t2_5 = df_dev_d2_t2_all + size_max_dim_d2_t2 * flag_d2_5;
            double* tmp_dev_d2_v2_5 = df_dev_d2_v2_all + size_max_dim_d2_v2 * flag_d2_5;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_p7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_p7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_h2 && idx_h1 < rng_h3 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_a[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d2_t2_5[(str_blk_idx_p5 + ll +
                                             (str_blk_idx_h2 + idx_p6 +
                                              (str_blk_idx_h3 + idx_h1) * base_size_h2b) *
                                             base_size_p5b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_p6 < rng_h1 && idx_h1 < rng_p6 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p4; ll++)
                    {
                        sm_b[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d2_v2_5[(str_blk_idx_h1 + idx_p6 +
                                             (str_blk_idx_p6 + idx_h1 +
                                              (str_blk_idx_p4 + ll) * base_size_p6b) *
                                             base_size_h1b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: 16
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_b[ll][idx_h1 + (idx_p6) * FUSION_SIZE_SLICE_1_H1 + 0];
                    temp_bv[1] = sm_b[ll][idx_h1 + (idx_p6) * FUSION_SIZE_SLICE_1_H1 + 16];
                    temp_bv[2] = sm_b[ll][idx_h1 + (idx_p6) * FUSION_SIZE_SLICE_1_H1 + 32];
                    temp_bv[3] = sm_b[ll][idx_h1 + (idx_p6) * FUSION_SIZE_SLICE_1_H1 + 48];

                    for (int xx = 0 ; xx < 4; xx++)
                    {
                        temp_av = sm_a[ll][idx_h2 + (idx_h3) * FUSION_SIZE_SLICE_1_H2 + (xx * 16)];

                        reg_tile[0][xx] += temp_av * temp_bv[0];
                        reg_tile[1][xx] += temp_av * temp_bv[1];
                        reg_tile[2][xx] += temp_av * temp_bv[2];
                        reg_tile[3][xx] += temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }

        // 	sd2_6
        if (flag_d2_6 >= 0)
        {
            //
            double* tmp_dev_d2_t2_6 = df_dev_d2_t2_all + size_max_dim_d2_t2 * flag_d2_6;
            double* tmp_dev_d2_v2_6 = df_dev_d2_v2_all + size_max_dim_d2_v2 * flag_d2_6;

            internal_upperbound = 0;
#pragma unroll 1
            for (int l = 0; l < base_size_p7b; l+= FUSION_SIZE_INT_UNIT)
            {
                // Part: Generalized Contraction Index (p7b)
                internal_offset = (l + FUSION_SIZE_INT_UNIT) - base_size_p7b;
                if (internal_offset > 0) internal_upperbound = internal_offset;

                // Load Input Tensor to Shared Memory: 16:16
                // # of size_internal Indices: 1
                if (idx_p6 < rng_h1 && idx_h1 < rng_h3 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p5; ll++)
                    {
                        sm_a[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d2_t2_6[(blk_idx_p5b * FUSION_SIZE_SLICE_1_P6 + ll +
                                             (str_blk_idx_h1 + idx_p6 +
                                              (str_blk_idx_h3 + idx_h1) * base_size_h1b) *
                                             base_size_p5b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }

                // Load Input Tensor to Shared Memory
                if (idx_p6 < rng_h2 && idx_h1 < rng_p6 &&
                    threadIdx_x < FUSION_SIZE_INT_UNIT - internal_upperbound)
                    for (int ll = 0; ll < rng_p4; ll++)
                    {
                        sm_b[threadIdx_x]
                            [threadIdx_y + ll * FUSION_SIZE_TB_1_Y] =
                            tmp_dev_d2_v2_6[(str_blk_idx_h2 + idx_p6 +
                                             (str_blk_idx_p6 + idx_h1 +
                                              (str_blk_idx_p4 + ll) * base_size_p6b) *
                                             base_size_h2b) *
                                            base_size_p7b +
                                            (threadIdx_x + l)];
                    }
                item.barrier(sycl::access::fence_space::local_space);

                // Cross-Product: 16
                // Part: Generalized Threads
                for (int ll = 0; ll < FUSION_SIZE_INT_UNIT - internal_upperbound; ll++)
                {
                    temp_bv[0] = sm_b[ll][idx_h2 + (idx_p6) * FUSION_SIZE_SLICE_1_H2 + 0];
                    temp_bv[1] = sm_b[ll][idx_h2 + (idx_p6) * FUSION_SIZE_SLICE_1_H2 + 16];
                    temp_bv[2] = sm_b[ll][idx_h2 + (idx_p6) * FUSION_SIZE_SLICE_1_H2 + 32];
                    temp_bv[3] = sm_b[ll][idx_h2 + (idx_p6) * FUSION_SIZE_SLICE_1_H2 + 48];

                    for (int xx = 0; xx < 4; xx++)	// 4 -> rng_p4: Local Transactions...
                    {
                        temp_av = sm_a[ll][idx_h1 + (idx_h3) * FUSION_SIZE_SLICE_1_H1 + (xx * 16)];

                        reg_tile[0][xx] -= temp_av * temp_bv[0];
                        reg_tile[1][xx] -= temp_av * temp_bv[1];
                        reg_tile[2][xx] -= temp_av * temp_bv[2];
                        reg_tile[3][xx] -= temp_av * temp_bv[3];
                    }
                }
                item.barrier(sycl::access::fence_space::local_space);
            }
        }
    }

    //
    // 	>>> s1 <<<
    // 		- if
    //
    //  singles (s1)
    {
        base_size_h1b = const_df_s1_size[0];
        base_size_h2b = const_df_s1_size[1];
        base_size_h3b = const_df_s1_size[2];
        base_size_p4b = const_df_s1_size[3];
        base_size_p5b = const_df_s1_size[4];
        base_size_p6b = const_df_s1_size[5];

        num_blks_h1b = CEIL(base_size_h1b, FUSION_SIZE_SLICE_1_H1);
        num_blks_h2b = CEIL(base_size_h2b, FUSION_SIZE_SLICE_1_H2);
        num_blks_h3b = CEIL(base_size_h3b, FUSION_SIZE_SLICE_1_H3);
        num_blks_p4b = CEIL(base_size_p4b, FUSION_SIZE_SLICE_1_P4);
        num_blks_p5b = CEIL(base_size_p5b, FUSION_SIZE_SLICE_1_P5);
        num_blks_p6b = CEIL(base_size_p6b, FUSION_SIZE_SLICE_1_P6);

        // 	(2) blk_idx_h/p*b
        blk_idx_p4b =
            blockIdx_x / (num_blks_h3b * num_blks_h2b * num_blks_h1b *
                                     num_blks_p6b * num_blks_p5b);
        tmp_blkIdx =
            blockIdx_x % (num_blks_h3b * num_blks_h2b * num_blks_h1b *
                                     num_blks_p6b * num_blks_p5b);
        blk_idx_p5b     = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b);
        tmp_blkIdx  	= (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b * num_blks_h1b * num_blks_p6b);
        blk_idx_p6b     = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b * num_blks_h1b);
        tmp_blkIdx 		= (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b * num_blks_h1b);
        blk_idx_h1b     = (tmp_blkIdx) / (num_blks_h3b * num_blks_h2b);
        tmp_blkIdx 		= (tmp_blkIdx) % (num_blks_h3b * num_blks_h2b);
        blk_idx_h2b 	= (tmp_blkIdx) / (num_blks_h3b);
        blk_idx_h3b = blockIdx_x % (num_blks_h3b);

        // 	(3) str_blk_idx_h/p*
        str_blk_idx_h3 	= blk_idx_h3b * FUSION_SIZE_SLICE_1_H3;
        str_blk_idx_h2 	= blk_idx_h2b * FUSION_SIZE_SLICE_1_H2;
        str_blk_idx_h1 	= blk_idx_h1b * FUSION_SIZE_SLICE_1_H1;
        str_blk_idx_p6 	= blk_idx_p6b * FUSION_SIZE_SLICE_1_P6;
        str_blk_idx_p5 	= blk_idx_p5b * FUSION_SIZE_SLICE_1_P5;
        str_blk_idx_p4 	= blk_idx_p4b * FUSION_SIZE_SLICE_1_P4;

        // 	(4) rng_h/p*
        if ((base_size_h3b - (str_blk_idx_h3)) >= FUSION_SIZE_SLICE_1_H3)
            rng_h3 = FUSION_SIZE_SLICE_1_H3;
        else
            rng_h3 = base_size_h3b % FUSION_SIZE_SLICE_1_H3;

        if ((base_size_h2b - (str_blk_idx_h2)) >= FUSION_SIZE_SLICE_1_H2)
            rng_h2 = FUSION_SIZE_SLICE_1_H2;
        else
            rng_h2 = base_size_h2b % FUSION_SIZE_SLICE_1_H2;

        if ((base_size_h1b - (str_blk_idx_h1)) >= FUSION_SIZE_SLICE_1_H1)
            rng_h1 = FUSION_SIZE_SLICE_1_H1;
        else
            rng_h1 = base_size_h1b % FUSION_SIZE_SLICE_1_H1;

        if ((base_size_p6b - (str_blk_idx_p6)) >= FUSION_SIZE_SLICE_1_P6)
            rng_p6 = FUSION_SIZE_SLICE_1_P6;
        else
            rng_p6 = base_size_p6b % FUSION_SIZE_SLICE_1_P6;

        if ((base_size_p5b - (str_blk_idx_p5)) >= FUSION_SIZE_SLICE_1_P5)
            rng_p5 = FUSION_SIZE_SLICE_1_P5;
        else
            rng_p5 = base_size_p5b % FUSION_SIZE_SLICE_1_P5;

        if ((base_size_p4b - (str_blk_idx_p4)) >= FUSION_SIZE_SLICE_1_P4)
            rng_p4 = FUSION_SIZE_SLICE_1_P4;
        else
            rng_p4 = base_size_p4b % FUSION_SIZE_SLICE_1_P4;


        // 	flags
        int flag_s1_1 = const_df_s1_exec[0];
        int flag_s1_2 = const_df_s1_exec[1];
        int flag_s1_3 = const_df_s1_exec[2];
        int flag_s1_4 = const_df_s1_exec[3];
        int flag_s1_5 = const_df_s1_exec[4];
        int flag_s1_6 = const_df_s1_exec[5];
        int flag_s1_7 = const_df_s1_exec[6];
        int flag_s1_8 = const_df_s1_exec[7];
        int flag_s1_9 = const_df_s1_exec[8];

        //                                        "x"         "x"
        //  >> s1_1:   t3[h3,h2,h1,p6,p5,p4] -= t2[p4,h1] * v2[h3,h2,p6,p5]
        //
        if (flag_s1_1 >= 0)	// these if-conditions make 100 ms..
        {
            //
            double* tmp_dev_s1_t1_1 = df_dev_s1_t1_all + size_max_dim_s1_t1 * flag_s1_1;
            double* tmp_dev_s1_v2_1 = df_dev_s1_v2_all + size_max_dim_s1_v2 * flag_s1_1;

            if (idx_h3 < rng_p4 && idx_h2 < rng_h1 && idx_p6 == 0 && idx_h1 == 0)
                sm_a[0][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_P4] = tmp_dev_s1_t1_1[str_blk_idx_p4 + idx_h3 + (str_blk_idx_h1 + idx_h2) * base_size_p4b];

            if (idx_h3 < rng_h3 && idx_h2 < rng_h2 && idx_p6 < rng_p6 && idx_h1 < rng_p5)
                sm_b[idx_h1][idx_h3 + (idx_h2 + (idx_p6) * 4) * 4] = tmp_dev_s1_v2_1[blk_idx_h3b * 4 + idx_h3 + (blk_idx_h2b * 4 + idx_h2 + (blk_idx_p6b * 4 + idx_p6 + (blk_idx_p5b * 4 + idx_h1) * base_size_p6b) * base_size_h2b) * base_size_h3b];
            item.barrier(sycl::access::fence_space::local_space);

            //  "p4"
            temp_av = sm_a[0][0 + (idx_h1) * 4];

            //  "p5"
            temp_bv[0] = sm_b[0][idx_h3 + (idx_h2 + (idx_p6) * 4) * 4];
            temp_bv[1] = sm_b[1][idx_h3 + (idx_h2 + (idx_p6) * 4) * 4];
            temp_bv[2] = sm_b[2][idx_h3 + (idx_h2 + (idx_p6) * 4) * 4];
            temp_bv[3] = sm_b[3][idx_h3 + (idx_h2 + (idx_p6) * 4) * 4];

            //  "p4 x p5"
            reg_singles[0][0] += temp_av * temp_bv[0];
            reg_singles[0][1] += temp_av * temp_bv[1];
            reg_singles[0][2] += temp_av * temp_bv[2];
            reg_singles[0][3] += temp_av * temp_bv[3];

            temp_av = sm_a[0][1 + (idx_h1) * 4];

            reg_singles[1][0] += temp_av * temp_bv[0];
            reg_singles[1][1] += temp_av * temp_bv[1];
            reg_singles[1][2] += temp_av * temp_bv[2];
            reg_singles[1][3] += temp_av * temp_bv[3];

            temp_av = sm_a[0][2 + (idx_h1) * 4];

            reg_singles[2][0] += temp_av * temp_bv[0];
            reg_singles[2][1] += temp_av * temp_bv[1];
            reg_singles[2][2] += temp_av * temp_bv[2];
            reg_singles[2][3] += temp_av * temp_bv[3];

            temp_av = sm_a[0][3 + (idx_h1) * 4];

            reg_singles[3][0] += temp_av * temp_bv[0];
            reg_singles[3][1] += temp_av * temp_bv[1];
            reg_singles[3][2] += temp_av * temp_bv[2];
            reg_singles[3][3] += temp_av * temp_bv[3];
            item.barrier(sycl::access::fence_space::local_space);
        }

        //                                        "x1,x2"     "x1,x2,x3,y1"
        //  >> s1_2:   t3[h3,h2,h1,p6,p5,p4] -= t2[p4,h2] * v2[h3,h1,p6,p5] (h3,h2,p6), (h1)
        //
        if (flag_s1_2 >= 0)	// these if-conditions make 100 ms..
        {
            //
            double* tmp_dev_s1_t1_2 = df_dev_s1_t1_all + size_max_dim_s1_t1 * flag_s1_2;
            double* tmp_dev_s1_v2_2 = df_dev_s1_v2_all + size_max_dim_s1_v2 * flag_s1_2;

            if (idx_h3 < rng_p4 && idx_h2 < rng_h2 && idx_p6 == 0 && idx_h1 == 0)
                sm_a[0][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_P4] = tmp_dev_s1_t1_2[str_blk_idx_p4 + idx_h3 + (str_blk_idx_h2 + idx_h2) * base_size_p4b];

            if (idx_h3 < rng_h3 && idx_h2 < rng_h1 && idx_p6 < rng_p6 && idx_h1 < rng_p5)
                sm_b[idx_h1][idx_h3 + (idx_h2 + (idx_p6) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3] = tmp_dev_s1_v2_2[str_blk_idx_h3 + idx_h3 + (str_blk_idx_h1 + idx_h2 + (str_blk_idx_p6 + idx_p6 + (str_blk_idx_p5 + idx_h1) * base_size_p6b) * base_size_h1b) * base_size_h3b];
            item.barrier(sycl::access::fence_space::local_space);

            //  "p4"
            temp_av = sm_a[0][0 + (idx_h2) * 4];

            //  "p5"
            temp_bv[0] = sm_b[0][idx_h3 + (idx_h1 + (idx_p6) * 4) * 4];
            temp_bv[1] = sm_b[1][idx_h3 + (idx_h1 + (idx_p6) * 4) * 4];
            temp_bv[2] = sm_b[2][idx_h3 + (idx_h1 + (idx_p6) * 4) * 4];
            temp_bv[3] = sm_b[3][idx_h3 + (idx_h1 + (idx_p6) * 4) * 4];

            //  "p4 x p5"
            reg_singles[0][0] -= temp_av * temp_bv[0];
            reg_singles[0][1] -= temp_av * temp_bv[1];
            reg_singles[0][2] -= temp_av * temp_bv[2];
            reg_singles[0][3] -= temp_av * temp_bv[3];

            temp_av = sm_a[0][1 + (idx_h2) * 4];

            reg_singles[1][0] -= temp_av * temp_bv[0];
            reg_singles[1][1] -= temp_av * temp_bv[1];
            reg_singles[1][2] -= temp_av * temp_bv[2];
            reg_singles[1][3] -= temp_av * temp_bv[3];

            temp_av = sm_a[0][2 + (idx_h2) * 4];

            reg_singles[2][0] -= temp_av * temp_bv[0];
            reg_singles[2][1] -= temp_av * temp_bv[1];
            reg_singles[2][2] -= temp_av * temp_bv[2];
            reg_singles[2][3] -= temp_av * temp_bv[3];

            temp_av = sm_a[0][3 + (idx_h2) * 4];

            reg_singles[3][0] -= temp_av * temp_bv[0];
            reg_singles[3][1] -= temp_av * temp_bv[1];
            reg_singles[3][2] -= temp_av * temp_bv[2];
            reg_singles[3][3] -= temp_av * temp_bv[3];
            item.barrier(sycl::access::fence_space::local_space);
        }

        //
        //  >> s1_3:   t3[h3,h2,h1,p6,p5,p4] -= t2[p4,h1] * v2[h3,h2,p6,p5] >> t3[h3,h2,h1,p6,p5,p4] += t2[p4,h3] * v2[h2,h1,p6,p5]
        //
        if (flag_s1_3 >= 0)	// these if-conditions make 100 ms..
        {
            //
            double* tmp_dev_s1_t1_3 = df_dev_s1_t1_all + size_max_dim_s1_t1 * flag_s1_3;
            double* tmp_dev_s1_v2_3 = df_dev_s1_v2_all + size_max_dim_s1_v2 * flag_s1_3;

            if (idx_h3 < rng_p4 && idx_h2 < rng_h3 && idx_p6 == 0 && idx_h1 == 0)
                sm_a[0][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_P4] = tmp_dev_s1_t1_3[str_blk_idx_p4 + idx_h3 + (str_blk_idx_h3 + idx_h2) * base_size_p4b];

            if (idx_h3 < rng_h2 && idx_h2 < rng_h1 && idx_p6 < rng_p6 && idx_h1 < rng_p5)
                sm_b[idx_h1][idx_h3 + (idx_h2 + (idx_p6) * 4) * 4] = tmp_dev_s1_v2_3[blk_idx_h2b * 4 + idx_h3 + (blk_idx_h1b * 4 + idx_h2 + (blk_idx_p6b * 4 + idx_p6 + (blk_idx_p5b * 4 + idx_h1) * base_size_p6b) * base_size_h1b) * base_size_h2b];
            item.barrier(sycl::access::fence_space::local_space);

            //  "p4"
            temp_av = sm_a[0][0 + (idx_h3) * 4];

            //  "p5"
            temp_bv[0] = sm_b[0][idx_h2 + (idx_h1 + (idx_p6) * 4) * 4];
            temp_bv[1] = sm_b[1][idx_h2 + (idx_h1 + (idx_p6) * 4) * 4];
            temp_bv[2] = sm_b[2][idx_h2 + (idx_h1 + (idx_p6) * 4) * 4];
            temp_bv[3] = sm_b[3][idx_h2 + (idx_h1 + (idx_p6) * 4) * 4];

            //  "p4 x p5"
            reg_singles[0][0] += temp_av * temp_bv[0];
            reg_singles[0][1] += temp_av * temp_bv[1];
            reg_singles[0][2] += temp_av * temp_bv[2];
            reg_singles[0][3] += temp_av * temp_bv[3];

            temp_av = sm_a[0][1 + (idx_h3) * 4];

            reg_singles[1][0] += temp_av * temp_bv[0];
            reg_singles[1][1] += temp_av * temp_bv[1];
            reg_singles[1][2] += temp_av * temp_bv[2];
            reg_singles[1][3] += temp_av * temp_bv[3];

            temp_av = sm_a[0][2 + (idx_h3) * 4];

            reg_singles[2][0] += temp_av * temp_bv[0];
            reg_singles[2][1] += temp_av * temp_bv[1];
            reg_singles[2][2] += temp_av * temp_bv[2];
            reg_singles[2][3] += temp_av * temp_bv[3];

            temp_av = sm_a[0][3 + (idx_h3) * 4];

            reg_singles[3][0] += temp_av * temp_bv[0];
            reg_singles[3][1] += temp_av * temp_bv[1];
            reg_singles[3][2] += temp_av * temp_bv[2];
            reg_singles[3][3] += temp_av * temp_bv[3];
            item.barrier(sycl::access::fence_space::local_space);
        }

        //
        //  >> s1_4:   t3[h3,h2,h1,p6,p5,p4] -= t2[p5,h1] * v2[h3,h2,p6,p4] (h3,h2,p6), (h1)
        //
        if (flag_s1_4 >= 0)	// these if-conditions make 100 ms..
        {
            double* tmp_dev_s1_t1_4 = df_dev_s1_t1_all + size_max_dim_s1_t1 * flag_s1_4;
            double* tmp_dev_s1_v2_4 = df_dev_s1_v2_all + size_max_dim_s1_v2 * flag_s1_4;

            if (idx_h3 < rng_p5 && idx_h2 < rng_h1 && idx_p6 == 0 && idx_h1 == 0)
                sm_a[0][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_P5] = tmp_dev_s1_t1_4[str_blk_idx_p5 + idx_h3 + (str_blk_idx_h1 + idx_h2) * base_size_p5b];

            if (idx_h3 < rng_h3 && idx_h2 < rng_h2 && idx_p6 < rng_p6 && idx_h1 < rng_p4)
                sm_b[idx_h1][idx_h3 + (idx_h2 + (idx_p6) * 4) * 4] = tmp_dev_s1_v2_4[str_blk_idx_h3 + idx_h3 + (str_blk_idx_h2 + idx_h2 + (str_blk_idx_p6 + idx_p6 + (str_blk_idx_p4 + idx_h1) * base_size_p6b) * base_size_h2b) * base_size_h3b];
            item.barrier(sycl::access::fence_space::local_space);

            //  "p5"
            temp_av = sm_a[0][0 + (idx_h1) * 4];

            //  "p4"
            temp_bv[0] = sm_b[0][idx_h3 + (idx_h2 + (idx_p6) * 4) * 4];
            temp_bv[1] = sm_b[1][idx_h3 + (idx_h2 + (idx_p6) * 4) * 4];
            temp_bv[2] = sm_b[2][idx_h3 + (idx_h2 + (idx_p6) * 4) * 4];
            temp_bv[3] = sm_b[3][idx_h3 + (idx_h2 + (idx_p6) * 4) * 4];

            //  "p4 x p5"
            reg_singles[0][0] -= temp_av * temp_bv[0];
            reg_singles[1][0] -= temp_av * temp_bv[1];
            reg_singles[2][0] -= temp_av * temp_bv[2];
            reg_singles[3][0] -= temp_av * temp_bv[3];

            temp_av = sm_a[0][1 + (idx_h1) * 4];

            reg_singles[0][1] -= temp_av * temp_bv[0];
            reg_singles[1][1] -= temp_av * temp_bv[1];
            reg_singles[2][1] -= temp_av * temp_bv[2];
            reg_singles[3][1] -= temp_av * temp_bv[3];

            temp_av = sm_a[0][2 + (idx_h1) * 4];

            reg_singles[0][2] -= temp_av * temp_bv[0];
            reg_singles[1][2] -= temp_av * temp_bv[1];
            reg_singles[2][2] -= temp_av * temp_bv[2];
            reg_singles[3][2] -= temp_av * temp_bv[3];

            temp_av = sm_a[0][3 + (idx_h1) * 4];

            reg_singles[0][3] -= temp_av * temp_bv[0];
            reg_singles[1][3] -= temp_av * temp_bv[1];
            reg_singles[2][3] -= temp_av * temp_bv[2];
            reg_singles[3][3] -= temp_av * temp_bv[3];
            item.barrier(sycl::access::fence_space::local_space);
        }

        //
        //  >> s1_5:   t3[h3,h2,h1,p6,p5,p4] -= t2[p5,h2] * v2[h3,h1,p6,p4] (h3,h2,p6), (h1)
        //
        if (flag_s1_5 >= 0)	// these if-conditions make 100 ms..
        {
            //
            double* tmp_dev_s1_t1_5 = df_dev_s1_t1_all + size_max_dim_s1_t1 * flag_s1_5;
            double* tmp_dev_s1_v2_5 = df_dev_s1_v2_all + size_max_dim_s1_v2 * flag_s1_5;

            if (idx_h3 < rng_p5 && idx_h2 < rng_h2 && idx_p6 == 0 && idx_h1 == 0)
                sm_a[0][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_P5] = tmp_dev_s1_t1_5[str_blk_idx_p5 + idx_h3 + (str_blk_idx_h2 + idx_h2) * base_size_p5b];

            if (idx_h3 < rng_h3 && idx_h2 < rng_h1 && idx_p6 < rng_p6 && idx_h1 < rng_p4)
                sm_b[idx_h1][idx_h3 + (idx_h2 + (idx_p6) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3] = tmp_dev_s1_v2_5[str_blk_idx_h3 + idx_h3 + (str_blk_idx_h1 + idx_h2 + (str_blk_idx_p6 + idx_p6 + (str_blk_idx_p4 + idx_h1) * base_size_p6b) * base_size_h1b) * base_size_h3b];
            item.barrier(sycl::access::fence_space::local_space);

            //  "p5"
            temp_av = sm_a[0][0 + (idx_h2) * 4];

            //  "p4"
            temp_bv[0] = sm_b[0][idx_h3 + (idx_h1 + (idx_p6) * 4) * 4];
            temp_bv[1] = sm_b[1][idx_h3 + (idx_h1 + (idx_p6) * 4) * 4];
            temp_bv[2] = sm_b[2][idx_h3 + (idx_h1 + (idx_p6) * 4) * 4];
            temp_bv[3] = sm_b[3][idx_h3 + (idx_h1 + (idx_p6) * 4) * 4];

            //  "p4 x p5"
            reg_singles[0][0] += temp_av * temp_bv[0];
            reg_singles[1][0] += temp_av * temp_bv[1];
            reg_singles[2][0] += temp_av * temp_bv[2];
            reg_singles[3][0] += temp_av * temp_bv[3];

            temp_av = sm_a[0][1 + (idx_h2) * 4];

            reg_singles[0][1] += temp_av * temp_bv[0];
            reg_singles[1][1] += temp_av * temp_bv[1];
            reg_singles[2][1] += temp_av * temp_bv[2];
            reg_singles[3][1] += temp_av * temp_bv[3];

            temp_av = sm_a[0][2 + (idx_h2) * 4];

            reg_singles[0][2] += temp_av * temp_bv[0];
            reg_singles[1][2] += temp_av * temp_bv[1];
            reg_singles[2][2] += temp_av * temp_bv[2];
            reg_singles[3][2] += temp_av * temp_bv[3];

            temp_av = sm_a[0][3 + (idx_h2) * 4];

            reg_singles[0][3] += temp_av * temp_bv[0];
            reg_singles[1][3] += temp_av * temp_bv[1];
            reg_singles[2][3] += temp_av * temp_bv[2];
            reg_singles[3][3] += temp_av * temp_bv[3];
            item.barrier(sycl::access::fence_space::local_space);
        }

        //
        //  >> s1_6:   t3[h3,h2,h1,p6,p5,p4] -= t2[p5,h3] * v2[h2,h1,p6,p4] (h3,h2,p6), (h1)
        //
        if (flag_s1_6 >= 0)	// these if-conditions make 100 ms..
        {
            //
            double* tmp_dev_s1_t1_6 = df_dev_s1_t1_all + size_max_dim_s1_t1 * flag_s1_6;
            double* tmp_dev_s1_v2_6 = df_dev_s1_v2_all + size_max_dim_s1_v2 * flag_s1_6;

            if (idx_h3 < rng_p5 && idx_h2 < rng_h3 && idx_p6 == 0 && idx_h1 == 0)
                sm_a[0][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_P5] = tmp_dev_s1_t1_6[str_blk_idx_p5 + idx_h3 + (str_blk_idx_h3 + idx_h2) * base_size_p5b];

            if (idx_h3 < rng_h2 && idx_h2 < rng_h1 && idx_p6 < rng_p6 && idx_h1 < rng_p4)
                sm_b[idx_h1][idx_h3 + (idx_h2 + (idx_p6) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2] = tmp_dev_s1_v2_6[str_blk_idx_h2 + idx_h3 + (str_blk_idx_h1 + idx_h2 + (str_blk_idx_p6 + idx_p6 + (str_blk_idx_p4 + idx_h1) * base_size_p6b) * base_size_h1b) * base_size_h2b];
            item.barrier(sycl::access::fence_space::local_space);

            //  "p5"
            temp_av = sm_a[0][0 + (idx_h3) * FUSION_SIZE_SLICE_1_P5];

            //  "p4"
            temp_bv[0] = sm_b[0][idx_h2 + (idx_h1 + (idx_p6) * 4) * 4];
            temp_bv[1] = sm_b[1][idx_h2 + (idx_h1 + (idx_p6) * 4) * 4];
            temp_bv[2] = sm_b[2][idx_h2 + (idx_h1 + (idx_p6) * 4) * 4];
            temp_bv[3] = sm_b[3][idx_h2 + (idx_h1 + (idx_p6) * 4) * 4];

            //  "p4 x p5"
            reg_singles[0][0] -= temp_av * temp_bv[0];
            reg_singles[1][0] -= temp_av * temp_bv[1];
            reg_singles[2][0] -= temp_av * temp_bv[2];
            reg_singles[3][0] -= temp_av * temp_bv[3];

            temp_av = sm_a[0][1 + (idx_h3) * FUSION_SIZE_SLICE_1_P5];

            reg_singles[0][1] -= temp_av * temp_bv[0];
            reg_singles[1][1] -= temp_av * temp_bv[1];
            reg_singles[2][1] -= temp_av * temp_bv[2];
            reg_singles[3][1] -= temp_av * temp_bv[3];

            temp_av = sm_a[0][2 + (idx_h3) * FUSION_SIZE_SLICE_1_P5];

            reg_singles[0][2] -= temp_av * temp_bv[0];
            reg_singles[1][2] -= temp_av * temp_bv[1];
            reg_singles[2][2] -= temp_av * temp_bv[2];
            reg_singles[3][2] -= temp_av * temp_bv[3];

            temp_av = sm_a[0][3 + (idx_h3) * FUSION_SIZE_SLICE_1_P5];

            reg_singles[0][3] -= temp_av * temp_bv[0];
            reg_singles[1][3] -= temp_av * temp_bv[1];
            reg_singles[2][3] -= temp_av * temp_bv[2];
            reg_singles[3][3] -= temp_av * temp_bv[3];
            item.barrier(sycl::access::fence_space::local_space);
        }

        //
        //  >> s1_7:   t3[h3,h2,h1,p6,p5,p4] -= t2[p6,h1] * v2[h3,h2,p5,p4] (h3,h2,p6), (h1)
        //
        if (flag_s1_7 >= 0)	// these if-conditions make 100 ms..
        {
            //
            double* tmp_dev_s1_t1_7 = df_dev_s1_t1_all + size_max_dim_s1_t1 * flag_s1_7;
            double* tmp_dev_s1_v2_7 = df_dev_s1_v2_all + size_max_dim_s1_v2 * flag_s1_7;

            if (idx_h3 < rng_p6 && idx_h2 < rng_h1 && idx_p6 == 0 && idx_h1 == 0)
                sm_a[0][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] = tmp_dev_s1_t1_7[str_blk_idx_p6 + idx_h3 + (str_blk_idx_h1 + idx_h2) * base_size_p6b];

            if (idx_h3 < rng_h3 && idx_h2 < rng_h2 && idx_p6 < rng_p5 && idx_h1 < rng_p4)
                sm_b[idx_h1][idx_h3 + (idx_h2 + (idx_p6) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3] = tmp_dev_s1_v2_7[str_blk_idx_h3 + idx_h3 + (str_blk_idx_h2 + idx_h2 + (str_blk_idx_p5 + idx_p6 + (str_blk_idx_p4 + idx_h1) * base_size_p5b) * base_size_h2b) * base_size_h3b];
            item.barrier(sycl::access::fence_space::local_space);

            //  "p4" x "p5"
            reg_singles[0][0] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[0][idx_h3 + (idx_h2 + (0) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[0][1] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[0][idx_h3 + (idx_h2 + (1) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[0][2] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[0][idx_h3 + (idx_h2 + (2) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[0][3] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[0][idx_h3 + (idx_h2 + (3) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];

            reg_singles[1][0] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[1][idx_h3 + (idx_h2 + (0) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[1][1] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[1][idx_h3 + (idx_h2 + (1) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[1][2] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[1][idx_h3 + (idx_h2 + (2) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[1][3] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[1][idx_h3 + (idx_h2 + (3) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];

            reg_singles[2][0] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[2][idx_h3 + (idx_h2 + (0) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[2][1] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[2][idx_h3 + (idx_h2 + (1) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[2][2] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[2][idx_h3 + (idx_h2 + (2) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[2][3] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[2][idx_h3 + (idx_h2 + (3) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];

            reg_singles[3][0] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[3][idx_h3 + (idx_h2 + (0) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[3][1] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[3][idx_h3 + (idx_h2 + (1) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[3][2] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[3][idx_h3 + (idx_h2 + (2) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[3][3] += sm_a[0][idx_p6 + (idx_h1) * FUSION_SIZE_SLICE_1_P6] * sm_b[3][idx_h3 + (idx_h2 + (3) * FUSION_SIZE_SLICE_1_H2) * FUSION_SIZE_SLICE_1_H3];
            item.barrier(sycl::access::fence_space::local_space);
        }

        //
        //  >> s1_8:   t3[h3,h2,h1,p6,p5,p4] -= t2[p6,h2] * v2[h3,h1,p5,p4] (h3,h2,p6), (h1)
        //
        if (flag_s1_8 >= 0)	// these if-conditions make 100 ms..
        {
            //
            double* tmp_dev_s1_t1_8 = df_dev_s1_t1_all + size_max_dim_s1_t1 * flag_s1_8;
            double* tmp_dev_s1_v2_8 = df_dev_s1_v2_all + size_max_dim_s1_v2 * flag_s1_8;

            if (idx_h3 < rng_p6 && idx_h2 < rng_h2 && idx_p6 == 0 && idx_h1 == 0)
                sm_a[0][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] = tmp_dev_s1_t1_8[str_blk_idx_p6 + idx_h3 + (str_blk_idx_h2 + idx_h2) * base_size_p6b];

            if (idx_h3 < rng_h3 && idx_h2 < rng_h1 && idx_p6 < rng_p5 && idx_h1 < rng_p4)
                sm_b[idx_h1][idx_h3 + (idx_h2 + (idx_p6) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3] = tmp_dev_s1_v2_8[str_blk_idx_h3 + idx_h3 + (str_blk_idx_h1 + idx_h2 + (str_blk_idx_p5 + idx_p6 + (str_blk_idx_p4 + idx_h1) * base_size_p5b) * base_size_h1b) * base_size_h3b];
            item.barrier(sycl::access::fence_space::local_space);

            //  "p4" x "p5"
            reg_singles[0][0] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[0][idx_h3 + (idx_h1 + (0) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[0][1] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[0][idx_h3 + (idx_h1 + (1) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[0][2] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[0][idx_h3 + (idx_h1 + (2) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[0][3] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[0][idx_h3 + (idx_h1 + (3) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];

            reg_singles[1][0] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[1][idx_h3 + (idx_h1 + (0) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[1][1] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[1][idx_h3 + (idx_h1 + (1) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[1][2] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[1][idx_h3 + (idx_h1 + (2) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[1][3] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[1][idx_h3 + (idx_h1 + (3) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];

            reg_singles[2][0] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[2][idx_h3 + (idx_h1 + (0) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[2][1] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[2][idx_h3 + (idx_h1 + (1) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[2][2] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[2][idx_h3 + (idx_h1 + (2) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[2][3] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[2][idx_h3 + (idx_h1 + (3) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];

            reg_singles[3][0] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[3][idx_h3 + (idx_h1 + (0) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[3][1] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[3][idx_h3 + (idx_h1 + (1) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[3][2] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[3][idx_h3 + (idx_h1 + (2) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            reg_singles[3][3] -= sm_a[0][idx_p6 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] * sm_b[3][idx_h3 + (idx_h1 + (3) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H3];
            item.barrier(sycl::access::fence_space::local_space);
        }

        //
        //  >> s1_9:   t3[h3,h2,h1,p6,p5,p4] -= t2[p6,h3] * v2[h2,h1,p5,p4] (h3,h2,p6), (h1)
        //
        if (flag_s1_9 >= 0)	// these if-conditions make 100 ms..
        {
            //
            double* tmp_dev_s1_t1_9 = df_dev_s1_t1_all + size_max_dim_s1_t1 * flag_s1_9;
            double* tmp_dev_s1_v2_9 = df_dev_s1_v2_all + size_max_dim_s1_v2 * flag_s1_9;

            if (idx_h3 < rng_p6 && idx_h2 < rng_h3 && idx_p6 == 0 && idx_h1 == 0)
                sm_a[0][idx_h3 + (idx_h2) * FUSION_SIZE_SLICE_1_P6] = tmp_dev_s1_t1_9[str_blk_idx_p6 + idx_h3 + (str_blk_idx_h3 + idx_h2) * base_size_p6b];

            if (idx_h3 < rng_h2 && idx_h2 < rng_h1 && idx_p6 < rng_p5 && idx_h1 < rng_p4)
                sm_b[idx_h1][idx_h3 + (idx_h2 + (idx_p6) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2] = tmp_dev_s1_v2_9[str_blk_idx_h2 + idx_h3 + (str_blk_idx_h1 + idx_h2 + (str_blk_idx_p5 + idx_p6 + (str_blk_idx_p4 + idx_h1) * base_size_p5b) * base_size_h1b) * base_size_h2b];
            item.barrier(sycl::access::fence_space::local_space);

            //  "p4" x "p5"
            reg_singles[0][0] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[0][idx_h2 + (idx_h1 + (0) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            reg_singles[0][1] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[0][idx_h2 + (idx_h1 + (1) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            reg_singles[0][2] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[0][idx_h2 + (idx_h1 + (2) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            reg_singles[0][3] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[0][idx_h2 + (idx_h1 + (3) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];

            reg_singles[1][0] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[1][idx_h2 + (idx_h1 + (0) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            reg_singles[1][1] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[1][idx_h2 + (idx_h1 + (1) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            reg_singles[1][2] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[1][idx_h2 + (idx_h1 + (2) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            reg_singles[1][3] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[1][idx_h2 + (idx_h1 + (3) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];

            reg_singles[2][0] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[2][idx_h2 + (idx_h1 + (0) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            reg_singles[2][1] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[2][idx_h2 + (idx_h1 + (1) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            reg_singles[2][2] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[2][idx_h2 + (idx_h1 + (2) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            reg_singles[2][3] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[2][idx_h2 + (idx_h1 + (3) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];

            reg_singles[3][0] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[3][idx_h2 + (idx_h1 + (0) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            reg_singles[3][1] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[3][idx_h2 + (idx_h1 + (1) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            reg_singles[3][2] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[3][idx_h2 + (idx_h1 + (2) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            reg_singles[3][3] += sm_a[0][idx_p6 + (idx_h3) * FUSION_SIZE_SLICE_1_P6] * sm_b[3][idx_h2 + (idx_h1 + (3) * FUSION_SIZE_SLICE_1_H1) * FUSION_SIZE_SLICE_1_H2];
            item.barrier(sycl::access::fence_space::local_space);
        }
    }


    //
    if (idx_h3 < energy_rng_h3 && idx_h2 < energy_rng_h2 && idx_p6 < energy_rng_p6 && idx_h1 < energy_rng_h1)
    {
        for (int i = 0; i < FUSION_SIZE_SLICE_1_P5; i++)
        {
            for (int j = 0; j < FUSION_SIZE_SLICE_1_P4; j++)
            {
                if (i < energy_rng_p5 && j < energy_rng_p4)
                {
                    //
                    double inner_factor = partial_inner_factor - dev_evl_sorted_p5b[i + (energy_str_blk_idx_p5)] - dev_evl_sorted_p4b[j + (energy_str_blk_idx_p4)];
                    // double inner_factor = partial_inner_factor - const_df_evl_sorted_p5b[i + (energy_str_blk_idx_p5)] - const_df_evl_sorted_p4b[j + (energy_str_blk_idx_p4)];

                    //
                    energy_1 += (reg_tile[j][i] * reg_tile[j][i]) / inner_factor;
                    energy_2 += (reg_tile[j][i] * (reg_tile[j][i] + reg_singles[j][i])) / inner_factor;
                }
            }
        }
    }
    item.barrier(sycl::access::fence_space::local_space);

    //
    //  to partially reduce the energies--- E(4) and E(5)
    //  a warp: 32 -(1)-> 16 -(2)-> 8 -(3)-> 4 -(4)-> 2
    //
#if 0
    sycl::ONEAPI::sub_group SG = item.get_sub_group();
    for (int offset = 32/2; offset > 0; offset /= 2)
    {
        energy_1 += SG.shuffle_down(energy_1, offset);
        energy_2 += SG.shuffle_down(energy_2, offset);
    }

    if (threadIdx_x == 0 && threadIdx_y % 2 == 0)
    {
        sm_a[0][threadIdx_y / 2] = energy_1;
        sm_b[0][threadIdx_y / 2] = energy_2;
    }
    item.barrier(sycl::access::fence_space::local_space);

    double final_energy_1 = 0.0;
    double final_energy_2 = 0.0;
    if (threadIdx_x == 0 && threadIdx_y == 0)
    {
        for (int i = 0; i < 8; i++)
        {
            final_energy_1 += sm_a[0][i];
            final_energy_2 += sm_b[0][i];
        }
        reduced_energy[blockIdx_x] = final_energy_1;
        reduced_energy[blockIdx_x + item.get_group_range(1)] = final_energy_2;
    }
#else
    // sm_a[16][64]
    // sm_b[16][64]
    sm_a[threadIdx_y][threadIdx_x] = energy_1;
    sm_b[threadIdx_y][threadIdx_x] = energy_2;
    item.barrier(sycl::access::fence_space::local_space);

    double final_energy_1 = 0.0;
    double final_energy_2 = 0.0;
    if (threadIdx_x == 0 && threadIdx_y == 0)
    {
      //if (blockIdx.x == 0) printf ("[%s] called\n", __func__);

      for (int i = 0; i < 16; i++)
	for (int j = 0; j < 16; j++)
	{
	  final_energy_1 += sm_a[j][i];
	  final_energy_2 += sm_b[j][i];
	}

      reduced_energy[blockIdx_x] = final_energy_1;
      reduced_energy[blockIdx_x + item.get_group_range(1)] = final_energy_2;
    }
#endif
}

void fully_fused_ccsd_t_gpu(sycl::queue *stream_id, size_t num_blocks,
                            size_t base_size_h1b, size_t base_size_h2b, size_t base_size_h3b,
                            size_t base_size_p4b, size_t base_size_p5b, size_t base_size_p6b,
                            //
                            double* df_dev_d1_t2_all, double* df_dev_d1_v2_all,
                            double* df_dev_d2_t2_all, double* df_dev_d2_v2_all,
                            double* df_dev_s1_t1_all, double* df_dev_s1_v2_all,
                            //
                            size_t size_d1_t2_all, size_t size_d1_v2_all, size_t size_d2_t2_all,
                            size_t size_d2_v2_all, size_t size_s1_t1_all, size_t size_s1_v2_all,
                            //
                            int* host_d1_size, int* host_d1_exec, // used
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
                            //
                            double* partial_energies)
{
    // 	to handle constant memories
    sycl::buffer<int, 1> const_df_s1_size(host_s1_size, sycl::range<1>{6});
    sycl::buffer<int, 1> const_df_s1_exec(host_s1_exec, sycl::range<1>{9});
    sycl::buffer<int, 1> const_df_d1_size(host_d1_size, sycl::range<1>{7 * size_noab});
    sycl::buffer<int, 1> const_df_d1_exec(host_d1_exec, sycl::range<1>{9 * size_noab});
    sycl::buffer<int, 1> const_df_d2_size(host_d2_size, sycl::range<1>{7 * size_nvab});
    sycl::buffer<int, 1> const_df_d2_exec(host_d2_exec, sycl::range<1>{9 * size_nvab});

    //
    // 	Depends on # of Fused Kernel
    //
    sycl::range<2> gridsize(1, num_blocks);
    sycl::range<2> blocksize(FUSION_SIZE_TB_1_Y, FUSION_SIZE_TB_1_X);

    //
    // 	to call the fused kernel for singles, doubles and energies.
    //
    // jk_ccsd_t_fully_fused_kernel_associative
    try {
      stream_id->submit([&](sycl::handler &cgh)
      {
	auto const_df_s1_size_acc = const_df_s1_size.get_access<sycl::access_mode::read, sycl::target::constant_buffer>(cgh);
	auto const_df_s1_exec_acc = const_df_s1_exec.get_access<sycl::access_mode::read, sycl::target::constant_buffer>(cgh);
	auto const_df_d1_size_acc = const_df_d1_size.get_access<sycl::access_mode::read, sycl::target::constant_buffer>(cgh);
	auto const_df_d1_exec_acc = const_df_d1_exec.get_access<sycl::access_mode::read, sycl::target::constant_buffer>(cgh);
	auto const_df_d2_size_acc = const_df_d2_size.get_access<sycl::access_mode::read, sycl::target::constant_buffer>(cgh);
	auto const_df_d2_exec_acc = const_df_d2_exec.get_access<sycl::access_mode::read, sycl::target::constant_buffer>(cgh);

	// allocate local/shared memory
	sycl::range<2> sm_a_range(16, 65 /*64 + 1*/);
	sycl::range<2> sm_b_range(16, 65 /*64 + 1*/);
	localAcc sm_a_acc(sm_a_range, cgh);
	localAcc sm_b_acc(sm_b_range, cgh);

	auto global_range = gridsize * blocksize;

	cgh.parallel_for(sycl::nd_range<2>(global_range, blocksize),
			 [=](sycl::nd_item<2> item) [[intel::reqd_sub_group_size(8)]]
			 {
			   revised_jk_ccsd_t_fully_fused_kernel(size_noab, size_nvab,
								size_max_dim_s1_t1,
								size_max_dim_s1_v2,
								size_max_dim_d1_t2,
								size_max_dim_d1_v2,
								size_max_dim_d2_t2,
								size_max_dim_d2_v2,
								df_dev_d1_t2_all, df_dev_d1_v2_all,
								df_dev_d2_t2_all, df_dev_d2_v2_all,
								df_dev_s1_t1_all, df_dev_s1_v2_all,
								dev_evl_sorted_h1b,
								dev_evl_sorted_h2b,
								dev_evl_sorted_h3b,
								dev_evl_sorted_p4b,
								dev_evl_sorted_p5b,
								dev_evl_sorted_p6b,
								partial_energies,
								CEIL(base_size_h3b, FUSION_SIZE_SLICE_1_H3),
								CEIL(base_size_h2b, FUSION_SIZE_SLICE_1_H2),
								CEIL(base_size_h1b, FUSION_SIZE_SLICE_1_H1),
								CEIL(base_size_p6b, FUSION_SIZE_SLICE_1_P6),
								CEIL(base_size_p5b, FUSION_SIZE_SLICE_1_P5),
								CEIL(base_size_p4b, FUSION_SIZE_SLICE_1_P4),
								base_size_h1b,
								base_size_h2b,
								base_size_h3b,
								base_size_p4b,
								base_size_p5b,
								base_size_p6b,
								item,
								const_df_s1_size_acc.get_pointer(), const_df_s1_exec_acc.get_pointer(),
								const_df_d1_size_acc.get_pointer(), const_df_d1_exec_acc.get_pointer(),
								const_df_d2_size_acc.get_pointer(), const_df_d2_exec_acc.get_pointer(),
								sm_a_acc, sm_b_acc);
			 });
      });
    } catch (const sycl::exception &e) {
      std::cout << "Caught synchronous SYCL exception:\n" << e.what() << std::endl;
    }
}
