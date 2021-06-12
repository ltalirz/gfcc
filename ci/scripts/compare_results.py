#!/usr/bin/env python

import sys
import os
import json
import math

def isclose(a, b, rel_tol=1e-09, abs_tol=0):
  return abs(a-b) <= max(rel_tol * max(abs(a), abs(b)), abs_tol)

if len(sys.argv) < 2:
    print("\nUsage: python3 compare_results.py reference_results_path current_results_path")
    sys.exit(1)

ref_res_path = os.path.abspath(str(sys.argv[1]))
cur_res_path = os.path.abspath(str(sys.argv[2]))

#check if above paths exist
if not os.path.exists(ref_res_path): 
    print("ERROR: " + ref_res_path + " does not exist!")
    sys.exit(1)

if not os.path.exists(cur_res_path): 
    print("ERROR: " + cur_res_path + " does not exist!")
    sys.exit(1)

ref_files = os.listdir(ref_res_path)
cur_files = os.listdir(cur_res_path)

for ref_file in ref_files:
    if ref_file not in cur_files:
        print("ERROR: " + ref_file + " not available in " + cur_res_path)
        sys.exit(1)
    
    with open(ref_res_path+"/"+ref_file) as ref_json_file:
        ref_data = json.load(ref_json_file)

    with open(cur_res_path+"/"+ref_file) as cur_json_file:
        cur_data = json.load(cur_json_file)    

    ref_scf_energy = ref_data["output"]["SCF"]["final_energy"]
    cur_scf_energy = cur_data["output"]["SCF"]["final_energy"]

    ref_ccsd_energy = ref_data["output"]["CCSD"]["final_energy"]["correlation"]
    cur_ccsd_energy = cur_data["output"]["CCSD"]["final_energy"]["correlation"]

    if not isclose(ref_scf_energy, cur_scf_energy):
        print("ERROR: SCF energy does not match. reference: " + str(ref_scf_energy) + ", current: " + str(cur_scf_energy))
        sys.exit(1)

    ccsd_threshold = ref_data["input"]["CCSD"]["threshold"]
    if not isclose(ref_ccsd_energy, cur_ccsd_energy, ccsd_threshold):
        print("ERROR: CCSD correlation energy does not match. reference: " + str(ref_ccsd_energy) + ", current: " + str(cur_ccsd_energy))
        sys.exit(1)

    ref_gfcc_data = ref_data["output"]["GFCCSD"]["retarded_alpha"]
    cur_gfcc_data = cur_data["output"]["GFCCSD"]["retarded_alpha"]

    ref_nlevels = ref_gfcc_data["nlevels"]
    cur_nlevels = cur_gfcc_data["nlevels"]

    if ref_nlevels != cur_nlevels:
        print("ERROR: number of levels in GFCCSD calculation does not match. reference: " + str(ref_nlevels) + ", current: " + str(cur_nlevels))
        sys.exit(1)
    
    gf_threshold = ref_data["input"]["GFCCSD"]["gf_threshold"]

    for lvl in range(1,ref_nlevels+1):
        lvl_str = "level"+str(lvl)

        ref_omega_npts = ref_gfcc_data[lvl_str]["omega_npts"]
        cur_omega_npts = cur_gfcc_data[lvl_str]["omega_npts"]

        if ref_omega_npts != cur_omega_npts:
            print("ERROR in " + lvl_str + ": number of frequency points in GFCCSD calculation does not match. reference: " + str(ref_omega_npts) + ", current: " + str(cur_omega_npts))
            sys.exit(1)
    
    
        for ni in range(0,ref_omega_npts):
            ref_w = ref_gfcc_data[lvl_str][str(ni)]["omega"]
            cur_w = cur_gfcc_data[lvl_str][str(ni)]["omega"]
            ref_A = ref_gfcc_data[lvl_str][str(ni)]["A_a"]
            cur_A = cur_gfcc_data[lvl_str][str(ni)]["A_a"]

            # if(isclose(ref_w,0.0)): cur_w = 0.0
            if (not isclose(ref_w, cur_w)) or (not isclose(ref_A, cur_A, gf_threshold)):
                print("GFCC ERROR in " + lvl_str + ": omega, A_a mismatch. reference (w, A0): (" + str(ref_w) + "," + str(ref_A) +
                "), current (w, A0): (" + str(cur_w) + "," + str(cur_A) + ")")
                sys.exit(1)        

