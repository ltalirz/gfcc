
#include "gfcc/contrib/cd_ccsd_os_ann.hpp"

#include <filesystem>
namespace fs = std::filesystem;

void ccsd_driver();
std::string filename;

int main( int argc, char* argv[] )
{
    if(argc<2){
        std::cout << "Please provide an input file!" << std::endl;
        return 1;
    }

    filename = std::string(argv[1]);
    std::ifstream testinput(filename); 
    if(!testinput){
        std::cout << "Input file provided [" << filename << "] does not exist!" << std::endl;
        return 1;
    }

    tamm::initialize(argc, argv);

    ccsd_driver();

    tamm::finalize();

    return 0;
}

void ccsd_driver() {

    // std::cout << "Input file provided = " << filename << std::endl;

    using T = double;

    ProcGroup pg = ProcGroup::create_coll(GA_MPI_Comm());
    ExecutionContext ec{pg, DistributionKind::nw, MemoryManagerKind::ga};
    auto rank = ec.pg().rank();

    auto [sys_data, hf_energy, shells, shell_tile_map, C_AO, F_AO, C_beta_AO, F_beta_AO, AO_opt, AO_tis,scf_conv]  
                    = hartree_fock_driver<T>(ec,filename);

    CCSDOptions& ccsd_options = sys_data.options_map.ccsd_options;
    debug = ccsd_options.debug;
    if(rank == 0) ccsd_options.print();

    if(rank==0) cout << endl << "#occupied, #virtual = " << sys_data.nocc << ", " << sys_data.nvir << endl;
    
    auto [MO,total_orbitals] = setupMOIS(sys_data);

    std::string out_fp = sys_data.output_file_prefix+"."+ccsd_options.basis;
    std::string files_dir = out_fp+"_files/"+sys_data.options_map.scf_options.scf_type;
    std::string files_prefix = /*out_fp;*/ files_dir+"/"+out_fp;
    std::string f1file = files_prefix+".f1_mo";
    std::string t1file = files_prefix+".t1amp";
    std::string t2file = files_prefix+".t2amp";
    std::string v2file = files_prefix+".cholv2";
    std::string cholfile = files_prefix+".cholcount";
    std::string ccsdstatus = files_prefix+".ccsdstatus";

    const bool is_rhf = sys_data.is_restricted;
    
    bool ccsd_restart = ccsd_options.readt || 
        ( (fs::exists(t1file) && fs::exists(t2file)     
        && fs::exists(f1file) && fs::exists(v2file)) );

    //deallocates F_AO, C_AO
    auto [cholVpr,d_f1,lcao,chol_count, max_cvecs, CI] = cd_svd_ga_driver<T>
                        (sys_data, ec, MO, AO_opt, C_AO, F_AO, C_beta_AO, F_beta_AO, shells, shell_tile_map,
                                ccsd_restart, cholfile);
    free_tensors(lcao);

    if(ccsd_options.writev) ccsd_options.writet = true;

    TiledIndexSpace N = MO("all");

    std::vector<T> p_evl_sorted;
    Tensor<T> d_r1, d_r2, d_t1, d_t2;
    std::vector<Tensor<T>> d_r1s, d_r2s, d_t1s, d_t2s;

    if(is_rhf) 
        std::tie(p_evl_sorted,d_t1,d_t2,d_r1,d_r2, d_r1s, d_r2s, d_t1s, d_t2s)
                = setupTensors_cs(ec,MO,d_f1,ccsd_options.ndiis,ccsd_restart && fs::exists(ccsdstatus) && scf_conv);
    else
        std::tie(p_evl_sorted,d_t1,d_t2,d_r1,d_r2, d_r1s, d_r2s, d_t1s, d_t2s)
                = setupTensors(ec,MO,d_f1,ccsd_options.ndiis,ccsd_restart && fs::exists(ccsdstatus) && scf_conv);

    if(ccsd_restart) {
        read_from_disk(d_f1,f1file);
        if(fs::exists(t1file) && fs::exists(t2file)) {
            read_from_disk(d_t1,t1file);
            read_from_disk(d_t2,t2file);
        }
        read_from_disk(cholVpr,v2file);
        ec.pg().barrier();
        p_evl_sorted = tamm::diagonal(d_f1);
    }
    
    else if(ccsd_options.writet) {
        // fs::remove_all(files_dir); 
        if(!fs::exists(files_dir)) fs::create_directories(files_dir);

        write_to_disk(d_f1,f1file);
        write_to_disk(cholVpr,v2file);

        if(rank==0){
          std::ofstream out(cholfile, std::ios::out);
          if(!out) cerr << "Error opening file " << cholfile << endl;
          out << chol_count << std::endl;
          out.close();
        }        
    }

    if(rank==0 && debug){
      print_vector(p_evl_sorted, files_prefix+".eigen_values.txt");
      cout << "Eigen values written to file: " << files_prefix+".eigen_values.txt" << endl << endl;
    }
    
    ec.pg().barrier();

    auto cc_t1 = std::chrono::high_resolution_clock::now();

    ExecutionHW ex_hw = ExecutionHW::CPU;
    #ifdef USE_TALSH
    ex_hw = ExecutionHW::GPU;
    const bool has_gpu = ec.has_gpu();
    TALSH talsh_instance;
    if(has_gpu) talsh_instance.initialize(ec.gpu_devid(),rank.value());
    #endif

    ccsd_restart = ccsd_restart && fs::exists(ccsdstatus) && scf_conv;

    std::string fullV2file = files_prefix+".fullV2";
    t1file = files_prefix+".fullT1amp";
    t2file = files_prefix+".fullT2amp";

    bool  computeTData = ccsd_options.computeTData;
    if(ccsd_options.writev)
        computeTData = computeTData && !fs::exists(fullV2file)
                && !fs::exists(t1file) && !fs::exists(t2file);

    if(computeTData && is_rhf)
      setup_full_t1t2(ec,MO,dt1_full,dt2_full);

    double residual=0, corr_energy=0;

    if(is_rhf)
        std::tie(residual, corr_energy) = cd_ccsd_cs_driver<T>(
            sys_data, ec, MO, CI, d_t1, d_t2, d_f1, 
            d_r1,d_r2, d_r1s, d_r2s, d_t1s, d_t2s, 
            p_evl_sorted, 
            cholVpr, ccsd_restart, files_prefix,
            computeTData);
    else
        std::tie(residual, corr_energy) = cd_ccsd_os_driver<T>(
            sys_data, ec, MO, CI, d_t1, d_t2, d_f1, 
            d_r1,d_r2, d_r1s, d_r2s, d_t1s, d_t2s, 
            p_evl_sorted, 
            cholVpr, ccsd_restart, files_prefix,
            computeTData);

    if(computeTData && is_rhf) {
        if(ccsd_options.writev) {
            write_to_disk(dt1_full,t1file);
            write_to_disk(dt2_full,t2file); 
            free_tensors(dt1_full, dt2_full);
        }
    }

    ccsd_stats(ec, hf_energy,residual,corr_energy,ccsd_options.threshold);

    if(ccsd_options.writet && !fs::exists(ccsdstatus)) {
        // write_to_disk(d_t1,t1file);
        // write_to_disk(d_t2,t2file);
        if(rank==0){
          std::ofstream out(ccsdstatus, std::ios::out);
          if(!out) cerr << "Error opening file " << ccsdstatus << endl;
          out << 1 << std::endl;
          out.close();
        }                
    }


    auto cc_t2 = std::chrono::high_resolution_clock::now();
    double ccsd_time = 
        std::chrono::duration_cast<std::chrono::duration<double>>((cc_t2 - cc_t1)).count();
    if(rank == 0) { 
      if(is_rhf)
        std::cout << std::endl << "Time taken for Closed Shell Cholesky CCSD: " << ccsd_time << " secs" << std::endl;
      else
        std::cout << std::endl << "Time taken for Open Shell Cholesky CCSD: " << ccsd_time << " secs" << std::endl;
    }

    double printtol=ccsd_options.printtol;
    if (rank == 0 && debug) {
        std::cout << std::endl << "Threshold for printing amplitudes set to: " << printtol << std::endl;
        std::cout << "T1, T2 amplitudes written to files: " << files_prefix+".print_t1amp.txt" 
                  << ", " << files_prefix+".print_t2amp.txt" << std::endl << std::endl;
        print_max_above_threshold(d_t1,printtol,files_prefix+".print_t1amp.txt");
        print_max_above_threshold(d_t2,printtol,files_prefix+".print_t2amp.txt");
    }

    if(!ccsd_restart) {
        free_tensors(d_r1,d_r2);
        free_vec_tensors(d_r1s, d_r2s, d_t1s, d_t2s);
    }

    if(is_rhf) free_tensors(d_t1, d_t2, d_f1);
    else free_tensors(d_f1);

    Tensor<T> d_v2;
    if(computeTData) {
        d_v2 = setupV2<T>(ec,MO,CI,cholVpr,chol_count, ex_hw);
        if(ccsd_options.writev) {
          write_to_disk(d_v2,fullV2file,true);
          Tensor<T>::deallocate(d_v2);
        }
    }

    free_tensors(cholVpr);

    #ifdef USE_TALSH
    //talshStats();
    if(has_gpu) talsh_instance.shutdown();
    #endif

    if(computeTData) {
        if(!is_rhf) {
          dt1_full = d_t1;
          dt2_full = d_t2;
        }
        if(rank==0) {
          cout << endl << "Retile T1,T2,V2 ... " << endl;
        }

        auto [MO1,total_orbitals1] = setupMOIS(sys_data,true);
        TiledIndexSpace N1 = MO1("all");
        TiledIndexSpace O1 = MO1("occ");
        TiledIndexSpace V1 = MO1("virt");

        // Tensor<T> t_d_f1{{N1,N1},{1,1}};
        Tensor<T> t_d_t1{{V1,O1},{1,1}};
        Tensor<T> t_d_t2{{V1,V1,O1,O1},{2,2}};
        Tensor<T> t_d_v2{{N1,N1,N1,N1},{2,2}};
        Tensor<T>::allocate(&ec,t_d_t1,t_d_t2,t_d_v2);

        Scheduler{ec}
        // (t_d_f1() = 0)
        (t_d_t1() = 0)
        (t_d_t2() = 0)
        (t_d_v2() = 0)
        .execute();

        TiledIndexSpace O = MO("occ");
        TiledIndexSpace V = MO("virt");

        if(ccsd_options.writev) {
          // Tensor<T> wd_f1{{N,N},{1,1}};
          Tensor<T> wd_t1{{V,O},{1,1}};
          Tensor<T> wd_t2{{V,V,O,O},{2,2}};
          Tensor<T> wd_v2{{N,N,N,N},{2,2}};

          // read_from_disk(t_d_f1,f1file,false,wd_f1);
          read_from_disk(t_d_t1,t1file,false,wd_t1);
          read_from_disk(t_d_t2,t2file,false,wd_t2);
          read_from_disk(t_d_v2,fullV2file,false,wd_v2);

          ec.pg().barrier();
          // write_to_disk(t_d_f1,f1file);
          write_to_disk(t_d_t1,t1file);
          write_to_disk(t_d_t2,t2file);
          write_to_disk(t_d_v2,fullV2file);
        }

        else {
          retile_tamm_tensor(dt1_full,t_d_t1);
          retile_tamm_tensor(dt2_full,t_d_t2);
          if(is_rhf) free_tensors(dt1_full, dt2_full);
          retile_tamm_tensor(d_v2,t_d_v2,"V2");
          free_tensors(d_v2);
        }

        free_tensors(t_d_t1, t_d_t2, t_d_v2);
    }
    
    if(!is_rhf) free_tensors(d_t1, d_t2);

    ec.flush_and_sync();
    // delete ec;

}
