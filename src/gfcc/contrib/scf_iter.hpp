
#pragma once

#include "scf_guess.hpp"

//TODO: UHF,ROHF,diis,3c,dft

template<typename TensorType>
void diis(ExecutionContext& ec, const TiledIndexSpace& tAO, Tensor<TensorType> D, Tensor<TensorType> F, Tensor<TensorType> err_mat, 
          int iter, int max_hist, const SCFVars& scf_vars, const int n_lindep,
          std::vector<Tensor<TensorType>>& diis_hist, std::vector<Tensor<TensorType>>& fock_hist);

template<typename TensorType>
void energy_diis(ExecutionContext& ec, const TiledIndexSpace& tAO, int iter, int max_hist, 
                 Tensor<TensorType> D, Tensor<TensorType> F, std::vector<Tensor<TensorType>>& D_hist, 
                 std::vector<Tensor<TensorType>>& fock_hist, std::vector<Tensor<TensorType>>& ehf_tamm_hist);



template<typename TensorType>
std::tuple<TensorType,TensorType> scf_iter_body(ExecutionContext& ec, 
      ScalapackInfo& scalapack_info,
      const int& iter, const SystemData& sys_data, SCFVars& scf_vars,
      TAMMTensors& ttensors, EigenTensors& etensors, bool ediis,
      #if defined(USE_GAUXC)
      GauXC::XCIntegrator<Matrix>& gauxc_integrator, 
      #endif
      bool scf_restart=false){

      const bool is_uhf = sys_data.is_unrestricted;
      const bool is_rhf = sys_data.is_restricted;
      const bool is_ks  = sys_data.is_ks;

      Tensor<TensorType>& H1                = ttensors.H1;
      Tensor<TensorType>& S1                = ttensors.S1;
      Tensor<TensorType>& D_diff            = ttensors.D_diff;
      Tensor<TensorType>& ehf_tmp           = ttensors.ehf_tmp;
      Tensor<TensorType>& ehf_tamm          = ttensors.ehf_tamm; 

      Matrix& C_alpha     = etensors.C; 
      Matrix& D_alpha     = etensors.D;
      Matrix& C_occ       = etensors.C_occ; 
      Tensor<TensorType>& F_alpha           = ttensors.F_alpha;
      Tensor<TensorType>& F_alpha_tmp       = ttensors.F_alpha_tmp;
      Tensor<TensorType>& FD_alpha_tamm     = ttensors.FD_tamm;
      Tensor<TensorType>& FDS_alpha_tamm    = ttensors.FDS_tamm;
      Tensor<TensorType>& D_alpha_tamm      = ttensors.D_tamm;
      Tensor<TensorType>& D_last_alpha_tamm = ttensors.D_last_tamm;
      
      Matrix& C_beta      = etensors.C_beta; 
      Matrix& D_beta      = etensors.D_beta;      
      Tensor<TensorType>& F_beta            = ttensors.F_beta;
      Tensor<TensorType>& F_beta_tmp        = ttensors.F_beta_tmp;
      Tensor<TensorType>& FD_beta_tamm      = ttensors.FD_beta_tamm;
      Tensor<TensorType>& FDS_beta_tamm     = ttensors.FDS_beta_tamm; 
      Tensor<TensorType>& D_beta_tamm       = ttensors.D_beta_tamm;
      Tensor<TensorType>& D_last_beta_tamm  = ttensors.D_last_beta_tamm;
      
      // DFT only
      Tensor<TensorType>& VXC       = ttensors.VXC;

      Scheduler sch{ec};

      const int64_t N = sys_data.nbf_orig;
      const TiledIndexSpace& tAO = scf_vars.tAO;

      auto rank  = ec.pg().rank(); 
      auto debug = sys_data.options_map.scf_options.debug;
      auto [mu, nu, ku]  = tAO.labels<3>("all");
      const int max_hist = sys_data.options_map.scf_options.diis_hist; 

      sch
        (F_alpha()  = H1())
        (F_alpha() += F_alpha_tmp())
        .execute();
      
      if(is_uhf) {
        sch
          (F_beta()   = H1())
          (F_beta()  += F_beta_tmp())
          .execute();
      }

      double ehf = 0.0;

      if(is_rhf) {
        sch
          (ehf_tmp(mu,nu)  = H1(mu,nu))
          (ehf_tmp(mu,nu) += F_alpha(mu,nu))
          (ehf_tamm()      = 0.5 * D_last_alpha_tamm() * ehf_tmp())
          .execute();
      }

      if(is_uhf) {
        sch
          (ehf_tmp(mu,nu)  = H1(mu,nu))
          (ehf_tmp(mu,nu) += F_alpha(mu,nu))
          (ehf_tamm()      = 0.5 * D_last_alpha_tamm() * ehf_tmp())
          (ehf_tmp(mu,nu)  = H1(mu,nu))
          (ehf_tmp(mu,nu) += F_beta(mu,nu))
          (ehf_tamm()     += 0.5 * D_last_beta_tamm()  * ehf_tmp())
          .execute();
      }
       
      ehf = get_scalar(ehf_tamm);
      
      #if defined(USE_GAUXC)
      double gauxc_exc = 0;
      if(is_ks) {
        const auto xcf_start = std::chrono::high_resolution_clock::now();
        gauxc_exc = gauxc_util::compute_xcf<TensorType>( ec, ttensors, etensors, gauxc_integrator );

        const auto xcf_stop = std::chrono::high_resolution_clock::now();
        const auto xcf_time =
        std::chrono::duration_cast<std::chrono::duration<double>>((xcf_stop - xcf_start)).count();
        if(rank == 0 && debug) std::cout << "xcf:" << xcf_time << "s, ";
      }

      ehf += gauxc_exc;

      //TODO: uks not implemented
      if(is_ks) {
        sch
          (F_alpha() += VXC())
          .execute();
      }
      #endif

      Tensor<TensorType> err_mat_alpha_tamm{tAO, tAO};
      Tensor<TensorType> err_mat_beta_tamm{tAO, tAO};
      Tensor<TensorType>::allocate(&ec, err_mat_alpha_tamm);
      if(is_uhf) Tensor<TensorType>::allocate(&ec, err_mat_beta_tamm);

      sch
        (FD_alpha_tamm(mu,nu)       = F_alpha(mu,ku)      * D_last_alpha_tamm(ku,nu))
        (FDS_alpha_tamm(mu,nu)      = FD_alpha_tamm(mu,ku) * S1(ku,nu))
        (err_mat_alpha_tamm(mu,nu)  = FDS_alpha_tamm(mu,nu))
        (err_mat_alpha_tamm(mu,nu) -= FDS_alpha_tamm(nu,mu))
        .execute();
      
      if(is_uhf) {
        sch
          (FD_beta_tamm(mu,nu)        = F_beta(mu,ku)       * D_last_beta_tamm(ku,nu))
          (FDS_beta_tamm(mu,nu)       = FD_beta_tamm(mu,ku)  * S1(ku,nu))    
          (err_mat_beta_tamm(mu,nu)   = FDS_beta_tamm(mu,nu))
          (err_mat_beta_tamm(mu,nu)  -= FDS_beta_tamm(nu,mu))
          .execute();
      }

      if(iter >= 1 && !ediis) {
        if(is_rhf) {
          ++scf_vars.idiis;
          diis(ec, tAO, D_alpha_tamm, F_alpha, err_mat_alpha_tamm, iter, max_hist,
            scf_vars, sys_data.n_lindep, ttensors.diis_hist, ttensors.fock_hist);
        }
        if(is_uhf) {
          ++scf_vars.idiis;
          Tensor<TensorType> F1_T{tAO, tAO};
          Tensor<TensorType> F1_S{tAO, tAO};
          Tensor<TensorType> D_T{tAO, tAO};
          Tensor<TensorType> D_S{tAO, tAO};
          sch
            .allocate(F1_T,F1_S,D_T,D_S)
            (F1_T()  = F_alpha())
            (F1_T() += F_beta())
            (F1_S()  = F_alpha())
            (F1_S() -= F_beta())
            (D_T()   = D_alpha_tamm())
            (D_T()  += D_beta_tamm())
            (D_S()   = D_alpha_tamm())
            (D_S()  -= D_beta_tamm())
            .execute();
          diis(ec, tAO, D_T, F1_T, err_mat_alpha_tamm, iter, max_hist, scf_vars,
               sys_data.n_lindep, ttensors.diis_hist, ttensors.fock_hist);
          diis(ec, tAO, D_S, F1_S,  err_mat_beta_tamm,  iter, max_hist, scf_vars,
               sys_data.n_lindep, ttensors.diis_beta_hist, ttensors.fock_beta_hist);

          sch
            (F_alpha()  = 0.5 * F1_T())
            (F_alpha() += 0.5 * F1_S())
            (F_beta()   = 0.5 * F1_T())
            (F_beta()  -= 0.5 * F1_S())
            (D_alpha_tamm()  = 0.5 * D_T())
            (D_alpha_tamm() += 0.5 * D_S())
            (D_beta_tamm()   = 0.5 * D_T())
            (D_beta_tamm()  -= 0.5 * D_S())
            .deallocate(F1_T,F1_S,D_T,D_S)
            .execute();
        }
      }
      
      

      if(!scf_restart){
        auto do_t1 = std::chrono::high_resolution_clock::now();

        scf_diagonalize<TensorType>(sch, sys_data, scalapack_info, ttensors, etensors);

        // compute density
        if( rank == 0 ) {
          if(is_rhf) {
            C_occ   = C_alpha.leftCols(sys_data.nelectrons_alpha);
            D_alpha = 2.0 * C_occ * C_occ.transpose();
            // X_a     = C_alpha;
            eigen_to_tamm_tensor(ttensors.X_alpha, C_alpha);
          }
          if(is_uhf) {
            C_occ   = C_alpha.leftCols(sys_data.nelectrons_alpha);
            D_alpha = C_occ * C_occ.transpose();
            // X_a     = C_alpha;
            C_occ   = C_beta.leftCols(sys_data.nelectrons_beta);
            D_beta  = C_occ * C_occ.transpose();
            // X_b     = C_beta;
            eigen_to_tamm_tensor(ttensors.X_alpha, C_alpha);
            eigen_to_tamm_tensor(ttensors.X_beta, C_beta);
          }
        }

        auto do_t2 = std::chrono::high_resolution_clock::now();
        auto do_time =
        std::chrono::duration_cast<std::chrono::duration<double>>((do_t2 - do_t1)).count();

        if(rank == 0 && debug) std::cout << std::fixed << std::setprecision(2) << "diagonalize: " << do_time << "s, " << std::endl; 
      } // end scf_restart 

      if(rank == 0) {
        eigen_to_tamm_tensor(D_alpha_tamm,D_alpha);
        if(is_uhf) {
          eigen_to_tamm_tensor(D_beta_tamm, D_beta);
        }
      }

      ec.pg().broadcast(D_alpha.data(), D_alpha.size(), 0 );
      if(is_uhf) ec.pg().broadcast(D_beta.data(), D_beta.size(), 0 );


      
      // if(ediis) {
      //   compute_2bf<TensorType>(ec, sys_data, scf_vars, obs, do_schwarz_screen, shell2bf, SchwarzK,
      //                           max_nprim4,shells, ttensors, etensors, false, do_density_fitting);
      //   Tensor<TensorType>  Dcopy{tAO,tAO};
      //   Tensor<TensorType>  Fcopy{tAO, tAO};
      //   Tensor<TensorType>  ehf_tamm_copy{};
      //   Tensor<TensorType>::allocate(&ec,Dcopy,Fcopy,ehf_tamm_copy);
      //   sch
      //     (Dcopy() = D_alpha_tamm())
      //     (Fcopy() = F_alpha_tmp())
      //     (ehf_tmp(mu,nu) = F_alpha_tmp(mu,nu))
      //     (ehf_tamm_copy()     = 0.5 * D_alpha_tamm() * ehf_tmp())
      //     // (ehf_tamm_copy() = ehf_tamm())
      //     .execute();
      //   ttensors.D_hist.push_back(Dcopy);
      //   ttensors.fock_hist.push_back(Fcopy);
      //   ttensors.ehf_tamm_hist.push_back(ehf_tamm_copy);
      //   if(rank==0) cout << "iter: " << iter << "," << (int)ttensors.D_hist.size() << endl;
      //   energy_diis(ec, tAO, iter, max_hist, D_alpha_tamm,
      //               ttensors.D_hist, ttensors.fock_hist, ttensors.ehf_tamm_hist);
      // }

      double rmsd = 0.0;
      sch
        (D_diff()  = D_alpha_tamm())
        (D_diff() -= D_last_alpha_tamm())
        .execute();
      rmsd = norm(D_diff)/(double)(1.0*N);

      if(is_uhf) {
        sch
          (D_diff()  = D_beta_tamm())
          (D_diff() -= D_last_beta_tamm())
          .execute();
        rmsd += norm(D_diff)/(double)(1.0*N);        
      }
        
      double alpha = sys_data.options_map.scf_options.alpha;
      if(rmsd < 1e-6) {
        scf_vars.switch_diis = true;
      } //rmsd check

      // D = alpha*D + (1.0-alpha)*D_last;
      if(is_rhf) {
        tamm::scale_ip(D_alpha_tamm(),alpha);
        sch(D_alpha_tamm() += (1.0-alpha)*D_last_alpha_tamm()).execute();
        tamm_to_eigen_tensor(D_alpha_tamm,D_alpha);
      }
      if(is_uhf) {
        tamm::scale_ip(D_alpha_tamm(),alpha);
        sch(D_alpha_tamm() += (1.0-alpha) * D_last_alpha_tamm()).execute();
        tamm_to_eigen_tensor(D_alpha_tamm,D_alpha);
        tamm::scale_ip(D_beta_tamm(),alpha);
        sch(D_beta_tamm()  += (1.0-alpha) * D_last_beta_tamm()).execute();
        tamm_to_eigen_tensor(D_beta_tamm, D_beta);
      }

      return std::make_tuple(ehf,rmsd);       
}

template<typename TensorType>
std::tuple<std::vector<int>,std::vector<int>,std::vector<int>> compute_2bf_taskinfo
      (ExecutionContext& ec, const SystemData& sys_data, const SCFVars& scf_vars, const libint2::BasisSet& obs,
      const bool do_schwarz_screen, const std::vector<size_t>& shell2bf,
      const Matrix& SchwarzK, 
      const size_t& max_nprim4, libint2::BasisSet& shells,
      TAMMTensors& ttensors, EigenTensors& etensors, const bool cs1s2=false){

      Matrix& D      = etensors.D; 
      Tensor<TensorType>& F_dummy       = ttensors.F_dummy;

      double fock_precision = std::min(sys_data.options_map.scf_options.tol_int, 1e-3 * sys_data.options_map.scf_options.conve);
      auto   rank           = ec.pg().rank();

      Matrix D_shblk_norm =  compute_shellblock_norm(obs, D);  // matrix of infty-norms of shell blocks
      
      std::vector<int> s1vec;
      std::vector<int> s2vec;
      std::vector<int> ntask_vec;

        auto comp_2bf_lambda = [&](IndexVector blockid) {

          auto s1 = blockid[0];
          auto sp12_iter = scf_vars.obs_shellpair_data.at(s1).begin();

          auto s2 = blockid[1];
          auto s2spl = scf_vars.obs_shellpair_list.at(s1);
          auto s2_itr = std::find(s2spl.begin(),s2spl.end(),s2);
          if(s2_itr == s2spl.end()) return;
          auto s2_pos = std::distance(s2spl.begin(),s2_itr);

          std::advance(sp12_iter,s2_pos);
          // const auto* sp12 = sp12_iter->get();
        
          const auto Dnorm12 = do_schwarz_screen ? D_shblk_norm(s1, s2) : 0.;

          size_t taskid = 0;          

          for (decltype(s1) s3 = 0; s3 <= s1; ++s3) {

            const auto Dnorm123 =
                do_schwarz_screen
                    ? std::max(D_shblk_norm(s1, s3),
                      std::max(D_shblk_norm(s2, s3), Dnorm12))
                    : 0.;

            auto sp34_iter = scf_vars.obs_shellpair_data.at(s3).begin();

            const auto s4_max = (s1 == s3) ? s2 : s3;
            for (const auto& s4 : scf_vars.obs_shellpair_list.at(s3)) {
              if (s4 > s4_max)
                break;  // for each s3, s4 are stored in monotonically increasing
                        // order

              // must update the iter even if going to skip s4
              // const auto* sp34 = sp34_iter->get();
              ++sp34_iter;

              const auto Dnorm1234 =
                  do_schwarz_screen
                      ? std::max(D_shblk_norm(s1, s4),
                        std::max(D_shblk_norm(s2, s4),
                        std::max(D_shblk_norm(s3, s4), Dnorm123)))
                      : 0.;

              if (do_schwarz_screen &&
                  Dnorm1234 * SchwarzK(s1, s2) * SchwarzK(s3, s4) < fock_precision)
                continue;

              taskid++;
            }
          }
          // taskinfo << rank.value() << ", " << s1 << ", " << s2 << ", " << taskid << "\n";
          s1vec.push_back(s1);
          s2vec.push_back(s2);
          ntask_vec.push_back(taskid);
        };

        block_for(ec, F_dummy(), comp_2bf_lambda);
        return std::make_tuple(s1vec,s2vec,ntask_vec);
}

template<typename TensorType>
void compute_2bf(ExecutionContext& ec, const SystemData& sys_data, const SCFVars& scf_vars,
      const libint2::BasisSet& obs, const bool do_schwarz_screen, const std::vector<size_t>& shell2bf,
      const Matrix& SchwarzK, const size_t& max_nprim4, libint2::BasisSet& shells,
      TAMMTensors& ttensors, EigenTensors& etensors, bool& is_3c_init, const bool do_density_fitting=false, double xHF = 1.) {

      using libint2::Operator;

      const bool is_uhf = sys_data.is_unrestricted;
      const bool is_rhf = sys_data.is_restricted;

      Matrix& G      = etensors.G;
      Matrix& D      = etensors.D; 
      Matrix& G_beta = etensors.G_beta;
      Matrix& D_beta = etensors.D_beta;

      // Tensor<TensorType>& F_dummy  = ttensors.F_dummy;
      Tensor<TensorType>& F_alpha_tmp = ttensors.F_alpha_tmp;
      Tensor<TensorType>& F_beta_tmp  = ttensors.F_beta_tmp;
      Tensor<TensorType>& Zxy_tamm    = ttensors.Zxy_tamm;
      Tensor<TensorType>& xyK_tamm    = ttensors.xyK_tamm;

      double fock_precision = std::min(sys_data.options_map.scf_options.tol_int, 1e-3 * sys_data.options_map.scf_options.conve);
      auto   rank           = ec.pg().rank();
      auto   N              = sys_data.nbf_orig; 
      auto   debug          = sys_data.options_map.scf_options.debug;

    const std::vector<Tile>& AO_tiles = scf_vars.AO_tiles;
    const std::vector<size_t>& shell_tile_map = scf_vars.shell_tile_map;

      auto do_t1 = std::chrono::high_resolution_clock::now();
      Matrix D_shblk_norm =  compute_shellblock_norm(obs, D);  // matrix of infty-norms of shell blocks
      
      //TODO: Revisit
      double engine_precision = fock_precision;
      
      if(rank == 0)
        assert(engine_precision > max_engine_precision &&
          "using precomputed shell pair data limits the max engine precision"
          " ... make max_engine_precision smaller and recompile");

      // construct the 2-electron repulsion integrals engine pool
      using libint2::Engine;
      Engine engine(Operator::coulomb, obs.max_nprim(), obs.max_l(), 0);

      engine.set_precision(engine_precision);
      const auto& buf = engine.results();

      #if 1
        auto comp_2bf_lambda = [&](IndexVector blockid) {

          auto s1 = blockid[0];
          auto bf1_first = shell2bf[s1]; 
          auto n1 = obs[s1].size();
          auto sp12_iter = scf_vars.obs_shellpair_data.at(s1).begin();

          auto s2 = blockid[1];
          auto s2spl = scf_vars.obs_shellpair_list.at(s1);
          auto s2_itr = std::find(s2spl.begin(),s2spl.end(),s2);
          if(s2_itr == s2spl.end()) return;
          auto s2_pos = std::distance(s2spl.begin(),s2_itr);
          auto bf2_first = shell2bf[s2];
          auto n2 = obs[s2].size();

          std::advance(sp12_iter,s2_pos);
          const auto* sp12 = sp12_iter->get();
        
          const auto Dnorm12 = do_schwarz_screen ? D_shblk_norm(s1, s2) : 0.;

          for (decltype(s1) s3 = 0; s3 <= s1; ++s3) {
            auto bf3_first = shell2bf[s3];
            auto n3 = obs[s3].size();

            const auto Dnorm123 =
                do_schwarz_screen
                    ? std::max(D_shblk_norm(s1, s3),
                      std::max(D_shblk_norm(s2, s3), Dnorm12))
                    : 0.;

            auto sp34_iter = scf_vars.obs_shellpair_data.at(s3).begin();

            const auto s4_max = (s1 == s3) ? s2 : s3;
            for (const auto& s4 : scf_vars.obs_shellpair_list.at(s3)) {
              if (s4 > s4_max)
                break;  // for each s3, s4 are stored in monotonically increasing
                        // order

              // must update the iter even if going to skip s4
              const auto* sp34 = sp34_iter->get();
              ++sp34_iter;

              const auto Dnorm1234 =
                  do_schwarz_screen
                      ? std::max(D_shblk_norm(s1, s4),
                        std::max(D_shblk_norm(s2, s4),
                        std::max(D_shblk_norm(s3, s4), Dnorm123)))
                      : 0.;

              if (do_schwarz_screen &&
                  Dnorm1234 * SchwarzK(s1, s2) * SchwarzK(s3, s4) < fock_precision)
                continue;

              auto bf4_first = shell2bf[s4];
              auto n4 = obs[s4].size();

              // compute the permutational degeneracy (i.e. # of equivalents) of
              // the given shell set
              auto s12_deg = (s1 == s2) ? 1 : 2;
              auto s34_deg = (s3 == s4) ? 1 : 2;
              auto s12_34_deg = (s1 == s3) ? (s2 == s4 ? 1 : 2) : 2;
              auto s1234_deg = s12_deg * s34_deg * s12_34_deg;

              engine.compute2<Operator::coulomb, libint2::BraKet::xx_xx, 0>(
                obs[s1], obs[s2], obs[s3], obs[s4], sp12, sp34); 
                
              const auto* buf_1234 = buf[0];
              if (buf_1234 == nullptr)
                continue; // if all integrals screened out, skip to next quartet

              // 1) each shell set of integrals contributes up to 6 shell sets of
              // the Fock matrix:
              //    F(a,b) += 1/2 * (ab|cd) * D(c,d)
              //    F(c,d) += 1/2 * (ab|cd) * D(a,b)
              //    F(b,d) -= 1/8 * (ab|cd) * D(a,c)
              //    F(b,c) -= 1/8 * (ab|cd) * D(a,d)
              //    F(a,c) -= 1/8 * (ab|cd) * D(b,d)
              //    F(a,d) -= 1/8 * (ab|cd) * D(b,c)
              // 2) each permutationally-unique integral (shell set) must be
              // scaled by its degeneracy,
              //    i.e. the number of the integrals/sets equivalent to it
              // 3) the end result must be symmetrized
              for (decltype(n1) f1 = 0, f1234 = 0; f1 != n1; ++f1) {
                const auto bf1 = f1 + bf1_first;
                for (decltype(n2) f2 = 0; f2 != n2; ++f2) {
                  const auto bf2 = f2 + bf2_first;
                  for (decltype(n3) f3 = 0; f3 != n3; ++f3) {
                    const auto bf3 = f3 + bf3_first;
                    for (decltype(n4) f4 = 0; f4 != n4; ++f4, ++f1234) {
                      const auto bf4 = f4 + bf4_first;
  
                      const auto value = buf_1234[f1234];
                      const auto value_scal_by_deg = value * s1234_deg;

                      if(is_uhf) {
                        //alpha_part
                        G(bf1, bf2)      += 0.5   * D(bf3, bf4) * value_scal_by_deg;
                        G(bf3, bf4)      += 0.5   * D(bf1, bf2) * value_scal_by_deg;
                        G(bf1, bf2)      += 0.5   * D_beta(bf3, bf4) * value_scal_by_deg;
                        G(bf3, bf4)      += 0.5   * D_beta(bf1, bf2) * value_scal_by_deg;
                        G(bf1, bf3)      -= xHF*0.25  * D(bf2, bf4) * value_scal_by_deg;
                        G(bf2, bf4)      -= xHF*0.25  * D(bf1, bf3) * value_scal_by_deg;
                        G(bf1, bf4)      -= xHF*0.25  * D(bf2, bf3) * value_scal_by_deg;
                        G(bf2, bf3)      -= xHF*0.25  * D(bf1, bf4) * value_scal_by_deg;
                        //beta_part
                        G_beta(bf1, bf2) += 0.5   * D_beta(bf3, bf4) * value_scal_by_deg;
                        G_beta(bf3, bf4) += 0.5   * D_beta(bf1, bf2) * value_scal_by_deg;
                        G_beta(bf1, bf2) += 0.5   * D(bf3, bf4) * value_scal_by_deg;
                        G_beta(bf3, bf4) += 0.5   * D(bf1, bf2) * value_scal_by_deg;
                        G_beta(bf1, bf3) -= xHF*0.25  * D_beta(bf2, bf4) * value_scal_by_deg;
                        G_beta(bf2, bf4) -= xHF*0.25  * D_beta(bf1, bf3) * value_scal_by_deg;
                        G_beta(bf1, bf4) -= xHF*0.25  * D_beta(bf2, bf3) * value_scal_by_deg;
                        G_beta(bf2, bf3) -= xHF*0.25  * D_beta(bf1, bf4) * value_scal_by_deg;
                      }
                      if(is_rhf) {
                        G(bf1, bf2)      += 0.5   * D(bf3, bf4) * value_scal_by_deg;
                        G(bf3, bf4)      += 0.5   * D(bf1, bf2) * value_scal_by_deg;
                        G(bf1, bf3)      -= xHF*0.125 * D(bf2, bf4) * value_scal_by_deg;
                        G(bf2, bf4)      -= xHF*0.125 * D(bf1, bf3) * value_scal_by_deg;
                        G(bf1, bf4)      -= xHF*0.125 * D(bf2, bf3) * value_scal_by_deg;
                        G(bf2, bf3)      -= xHF*0.125 * D(bf1, bf4) * value_scal_by_deg;
                      }
                    }
                  }
                }
              }
            }
          }
        };
      #endif

    decltype(do_t1) do_t2;
    double do_time;

      if(!do_density_fitting){
        G.setZero(N,N);
        if(is_uhf) G_beta.setZero(N,N);
        //block_for(ec, F_dummy(), comp_2bf_lambda);
        for (Eigen::Index i1=0;i1<etensors.taskmap.rows();i1++)
        for (Eigen::Index j1=0;j1<etensors.taskmap.cols();j1++) {
          if(etensors.taskmap(i1,j1)==-1 || etensors.taskmap(i1,j1) != rank) continue;
          IndexVector blockid{(tamm::Index)i1,(tamm::Index)j1};
          comp_2bf_lambda(blockid);
        }
        ec.pg().barrier();        
        //symmetrize G
        Matrix Gt = 0.5*(G + G.transpose());
        G = Gt;
        if(is_uhf) {
          Gt = 0.5*(G_beta + G_beta.transpose());
          G_beta = Gt;
        }
        Gt.resize(0,0);

        do_t2 = std::chrono::high_resolution_clock::now();
        do_time =
        std::chrono::duration_cast<std::chrono::duration<double>>((do_t2 - do_t1)).count();

        if(rank == 0 && debug) std::cout << std::fixed << std::setprecision(2) << "Fock build: " << do_time << "s, ";
        
        eigen_to_tamm_tensor_acc(F_alpha_tmp, G);
        if(is_uhf) eigen_to_tamm_tensor_acc(F_beta_tmp, G_beta);

        // ec.pg().barrier();

      }
      else {
        #if 1
          using libint2::Operator;
          using libint2::BraKet;
          using libint2::Engine;

          // const auto n = obs.nbf();
          const auto ndf = sys_data.ndf;
          const libint2::BasisSet& dfbs = scf_vars.dfbs;
          const std::vector<Tile>& dfAO_tiles = scf_vars.dfAO_tiles;
          const std::vector<size_t>& df_shell_tile_map = scf_vars.df_shell_tile_map;

          auto mu = scf_vars.mu, nu = scf_vars.nu, ku = scf_vars.ku;
          auto d_mu = scf_vars.d_mu, d_nu = scf_vars.d_nu, d_ku = scf_vars.d_ku;
          const tamm::TiledIndexLabel& dCocc_til = scf_vars.dCocc_til;

          // using first time? compute 3-center ints and transform to inv sqrt
          // representation
          if (!is_3c_init) {
            is_3c_init = true;
          // const auto nshells = obs.size();
          // const auto nshells_df = dfbs.size();  
            const auto& unitshell = libint2::Shell::unit();
          
            auto engine = libint2::Engine(libint2::Operator::coulomb,
                                        std::max(obs.max_nprim(), dfbs.max_nprim()),
                                        std::max(obs.max_l(), dfbs.max_l()), 0);
            engine.set(libint2::BraKet::xs_xx);

            auto shell2bf = obs.shell2bf();
            auto shell2bf_df = dfbs.shell2bf();
            const auto& results = engine.results();

            // Tensor<TensorType>::allocate(&ec, Zxy_tamm);

            #if 1
              //TODO: Screening?
              auto compute_2body_fock_dfC_lambda = [&](const IndexVector& blockid) {

                auto bi0 = blockid[0];
                auto bi1 = blockid[1];
                auto bi2 = blockid[2];

                const TAMM_SIZE size = Zxy_tamm.block_size(blockid);
                auto block_dims   = Zxy_tamm.block_dims(blockid);
                std::vector<TensorType> dbuf(size);

                auto bd1 = block_dims[1];
                auto bd2 = block_dims[2];

                auto s0range_end = df_shell_tile_map[bi0];
                decltype(s0range_end) s0range_start = 0l;

                if (bi0>0) s0range_start = df_shell_tile_map[bi0-1]+1;
              
                for (auto s0 = s0range_start; s0 <= s0range_end; ++s0) {
                // auto n0 = dfbs[s0].size();
                auto s1range_end = shell_tile_map[bi1];
                decltype(s1range_end) s1range_start = 0l;
                if (bi1>0) s1range_start = shell_tile_map[bi1-1]+1;
              
              for (auto s1 = s1range_start; s1 <= s1range_end; ++s1) {
                // auto n1 = shells[s1].size();

                auto s2range_end = shell_tile_map[bi2];
                decltype(s2range_end) s2range_start = 0l;
                if (bi2>0) s2range_start = shell_tile_map[bi2-1]+1;

                for (auto s2 = s2range_start; s2 <= s2range_end; ++s2) {
                  //// if (s2>s1) continue;
                  // auto n2 = shells[s2].size();
                  // auto n123 = n0*n1*n2;
                  // std::vector<TensorType> tbuf(n123);
                  engine.compute2<Operator::coulomb, BraKet::xs_xx, 0>(
                      dfbs[s0], unitshell, obs[s1], obs[s2]);
                  const auto* buf = results[0];
                  if (buf == nullptr) continue;     
                  // std::copy(buf, buf + n123, tbuf.begin());         
                  
                      tamm::Tile curshelloffset_i = 0U;
                      tamm::Tile curshelloffset_j = 0U;
                      tamm::Tile curshelloffset_k = 0U;
                      for(auto x=s1range_start;x<s1;x++) curshelloffset_i += AO_tiles[x];
                      for(auto x=s2range_start;x<s2;x++) curshelloffset_j += AO_tiles[x];
                      for(auto x=s0range_start;x<s0;x++) curshelloffset_k += dfAO_tiles[x];

                      size_t c = 0;
                      auto dimi =  curshelloffset_i + AO_tiles[s1];
                      auto dimj =  curshelloffset_j + AO_tiles[s2];
                      auto dimk =  curshelloffset_k + dfAO_tiles[s0];

                      for(auto k = curshelloffset_k; k < dimk; k++) 
                      for(auto i = curshelloffset_i; i < dimi; i++) 
                      for(auto j = curshelloffset_j; j < dimj; j++, c++) 
                          dbuf[(k*bd1+i)*bd2+j] = buf[c]; //tbuf[c]
                    } //s2
                  } //s1
                } //s0
                Zxy_tamm.put(blockid,dbuf);
              };

              do_t1 = std::chrono::high_resolution_clock::now();
              block_for(ec, Zxy_tamm(), compute_2body_fock_dfC_lambda);

              do_t2 = std::chrono::high_resolution_clock::now();
              do_time =
              std::chrono::duration_cast<std::chrono::duration<double>>((do_t2 - do_t1)).count();
              if(rank == 0 && debug) std::cout << "2BF-DFC:" << do_time << "s, ";
            #endif  
        
            Tensor<TensorType> K_tamm{scf_vars.tdfAO, scf_vars.tdfAO}; //ndf,ndf
            Tensor<TensorType>::allocate(&ec, K_tamm);

            /*** ============================== ***/
            /*** compute 2body-2index integrals ***/
            /*** ============================== ***/

            engine =
                Engine(libint2::Operator::coulomb, dfbs.max_nprim(), dfbs.max_l(), 0);
            engine.set(BraKet::xs_xs);
        
            const auto& buf2 = engine.results();

            auto compute_2body_2index_ints_lambda = [&](const IndexVector& blockid) {

              auto bi0 = blockid[0];
              auto bi1 = blockid[1];

              const TAMM_SIZE size = K_tamm.block_size(blockid);
              auto block_dims   = K_tamm.block_dims(blockid);
              std::vector<TensorType> dbuf(size);

              auto bd1 = block_dims[1];
              auto s1range_end = df_shell_tile_map[bi0];
              decltype(s1range_end) s1range_start = 0l;

              if (bi0>0) s1range_start = df_shell_tile_map[bi0-1]+1;
              
              for (auto s1 = s1range_start; s1 <= s1range_end; ++s1) {
                auto n1 = dfbs[s1].size();

                auto s2range_end = df_shell_tile_map[bi1];
                decltype(s2range_end) s2range_start = 0l;
                if (bi1>0) s2range_start = df_shell_tile_map[bi1-1]+1;

                for (auto s2 = s2range_start; s2 <= s2range_end; ++s2) {
                // if (s2>s1) continue;          
                // if(s2>s1){ TODO: screening doesnt work - revisit
                //   auto s2spl = scf_vars.dfbs_shellpair_list.at(s2);
                //   if(std::find(s2spl.begin(),s2spl.end(),s1) == s2spl.end()) continue;
                // }
                // else{
                //   auto s2spl = scf_vars.dfbs_shellpair_list.at(s1);
                //   if(std::find(s2spl.begin(),s2spl.end(),s2) == s2spl.end()) continue;
                // }

                  auto n2 = dfbs[s2].size();

                  std::vector<TensorType> tbuf(n1*n2);
                  engine.compute(dfbs[s1], dfbs[s2]);
                  if (buf2[0] == nullptr) continue;
                  Eigen::Map<const Matrix> buf_mat(buf2[0], n1, n2);
                  Eigen::Map<Matrix>(&tbuf[0],n1,n2) = buf_mat;

                  tamm::Tile curshelloffset_i = 0U;
                  tamm::Tile curshelloffset_j = 0U;
                  for(decltype(s1) x=s1range_start;x<s1;x++) curshelloffset_i += dfAO_tiles[x];
                  for(decltype(s2) x=s2range_start;x<s2;x++) curshelloffset_j += dfAO_tiles[x];

                  size_t c = 0;
                  auto dimi =  curshelloffset_i + dfAO_tiles[s1];
                  auto dimj =  curshelloffset_j + dfAO_tiles[s2];
                  for(auto i = curshelloffset_i; i < dimi; i++) 
                  for(auto j = curshelloffset_j; j < dimj; j++, c++) 
                          dbuf[i*bd1+j] = tbuf[c];

                //TODO: not needed if screening works
                // if (s1 != s2)  // if s1 >= s2, copy {s1,s2} to the corresponding {s2,s1}
                //              // block, note the transpose!
                // result.block(bf2, bf1, n2, n1) = buf_mat.transpose();                            
                }
              }
              K_tamm.put(blockid,dbuf);
            };

            block_for(ec, K_tamm(), compute_2body_2index_ints_lambda);

            Matrix V(ndf,ndf);
            V.setZero(); 
            tamm_to_eigen_tensor(K_tamm,V);
          
            #if 1
              auto ig1 = std::chrono::high_resolution_clock::now();

              std::vector<TensorType> eps(ndf);
              lapack::syevd( lapack::Job::Vec, lapack::Uplo::Lower, ndf, V.data(), ndf, eps.data() );

              Matrix Vp = V;

              for (size_t j=0; j<ndf; ++j) {
                double  tmp=1.0/sqrt(eps[j]);
                blas::scal( ndf, tmp, Vp.data() + j*ndf,   1 );
              }

              Matrix ke(ndf,ndf);
              blas::gemm(blas::Layout::ColMajor, blas::Op::NoTrans, blas::Op::Trans, ndf, ndf, ndf,
                          1, Vp.data(), ndf, V.data(), ndf, 0, ke.data(), ndf);
              eigen_to_tamm_tensor(K_tamm,ke);

              // Scheduler{ec}
              // (k_tmp(d_mu,d_nu) = V_tamm(d_ku,d_mu) * V_tamm(d_ku,d_nu))
              // (K_tamm(d_mu,d_nu) = eps_tamm(d_mu,d_ku) * k_tmp(d_ku,d_nu)) .execute();

              //Tensor<TensorType>::deallocate(k_tmp, eps_tamm, V_tamm);

              auto ig2 = std::chrono::high_resolution_clock::now();
              auto igtime =
              std::chrono::duration_cast<std::chrono::duration<double>>((ig2 - ig1)).count();

              if(rank == 0 && debug) std::cout << "V^-1/2:" << igtime << "s, ";

            #endif


          // contract(1.0, Zxy, {1, 2, 3}, K, {1, 4}, 0.0, xyK, {2, 3, 4});
          // Tensor3D xyK = Zxy.contract(K,aidx_00); 
            Scheduler{ec}
            (xyK_tamm(mu,nu,d_nu) = Zxy_tamm(d_mu,mu,nu) * K_tamm(d_mu,d_nu)).execute();
            Tensor<TensorType>::deallocate(K_tamm); //release memory Zxy_tamm

          }  // if (!is_3c_init)
      
          Scheduler sch{ec};
          auto ig1 = std::chrono::high_resolution_clock::now();
          auto tig1 = ig1;   

          Tensor<TensorType> Jtmp_tamm{scf_vars.tdfAO}; //ndf
          Tensor<TensorType> xiK_tamm{scf_vars.tAO,scf_vars.tdfCocc,scf_vars.tdfAO}; //n, nocc, ndf
          Tensor<TensorType>& C_occ_tamm = ttensors.C_occ_tamm;
      
          eigen_to_tamm_tensor(C_occ_tamm,etensors.C_occ);

        auto ig2 = std::chrono::high_resolution_clock::now();
        auto igtime =
        std::chrono::duration_cast<std::chrono::duration<double>>((ig2 - ig1)).count();
        ec.pg().barrier();
        if(rank == 0 && debug) std::cout << " C_occ_tamm <- C_occ:" << igtime << "s, ";

        ig1 = std::chrono::high_resolution_clock::now();
        // contract(1.0, xyK, {1, 2, 3}, Co, {2, 4}, 0.0, xiK, {1, 4, 3});
        sch.allocate(xiK_tamm,Jtmp_tamm)
        (xiK_tamm(mu,dCocc_til,d_mu) = xyK_tamm(mu,nu,d_mu) * C_occ_tamm(nu, dCocc_til)).execute();

         ig2 = std::chrono::high_resolution_clock::now();
         igtime =
        std::chrono::duration_cast<std::chrono::duration<double>>((ig2 - ig1)).count();
        if(rank == 0 && debug) std::cout << " xiK_tamm:" << igtime << "s, ";

        ig1 = std::chrono::high_resolution_clock::now();
        // compute Coulomb
        // contract(1.0, xiK, {1, 2, 3}, Co, {1, 2}, 0.0, Jtmp, {3});
        // Jtmp = xiK.contract(Co,idx_0011); 
        sch(Jtmp_tamm(d_mu) = xiK_tamm(mu,dCocc_til,d_mu) * C_occ_tamm(mu,dCocc_til)).execute();

         ig2 = std::chrono::high_resolution_clock::now();
         igtime =
        std::chrono::duration_cast<std::chrono::duration<double>>((ig2 - ig1)).count();
        if(rank == 0 && debug) std::cout << " Jtmp_tamm:" << igtime << "s, ";

        ig1 = std::chrono::high_resolution_clock::now();
        // contract(1.0, xiK, {1, 2, 3}, xiK, {4, 2, 3}, 0.0, G, {1, 4});
        // Tensor2D K_ret = xiK.contract(xiK,idx_1122); 
        // xiK.resize(0, 0, 0);
        sch
        (F_alpha_tmp(mu,ku) += -xHF * xiK_tamm(mu,dCocc_til,d_mu) * xiK_tamm(ku,dCocc_til,d_mu))
        .deallocate(xiK_tamm).execute();

         ig2 = std::chrono::high_resolution_clock::now();
         igtime =
        std::chrono::duration_cast<std::chrono::duration<double>>((ig2 - ig1)).count();
        if(rank == 0 && debug) std::cout << " F_alpha_tmp:" << igtime << "s, ";

        ig1 = std::chrono::high_resolution_clock::now();
        //contract(2.0, xyK, {1, 2, 3}, Jtmp, {3}, -1.0, G, {1, 2});
        // Tensor2D J_ret = xyK.contract(Jtmp,aidx_20);
        sch
        (F_alpha_tmp(mu,nu) += 2.0 * xyK_tamm(mu,nu,d_mu) * Jtmp_tamm(d_mu))
        .deallocate(Jtmp_tamm).execute();
        // (F_alpha_tmp(mu,nu) = 2.0 * J_ret_tamm(mu,nu))
        // (F_alpha_tmp(mu,nu) += -1.0 * K_ret_tamm(mu,nu)).execute();

         ig2 = std::chrono::high_resolution_clock::now();
         igtime =        
        std::chrono::duration_cast<std::chrono::duration<double>>((ig2 - ig1)).count();
        if(rank == 0 && debug) std::cout << " F_alpha_tmp:" << igtime << "s, ";
        
      auto tig2 = std::chrono::high_resolution_clock::now();
      auto tigtime =
      std::chrono::duration_cast<std::chrono::duration<double>>((tig2 - tig1)).count();  

      if(rank == 0 && debug) std::cout << "3c contractions:" << tigtime << "s, ";
        
        #endif
      } //end density fitting
}

template<typename TensorType>
void energy_diis(ExecutionContext& ec, const TiledIndexSpace& tAO, int iter, int max_hist, 
                 Tensor<TensorType> D, Tensor<TensorType> F, Tensor<TensorType> ehf_tamm,
                 std::vector<Tensor<TensorType>>& D_hist, 
                 std::vector<Tensor<TensorType>>& fock_hist, std::vector<Tensor<TensorType>>& ehf_tamm_hist) {

  tamm::Scheduler sch{ec};

  auto rank = ec.pg().rank().value();

  // if(rank == 0) cout << "contructing pulay matrix" << endl;
  
  int64_t idim = std::min((int)D_hist.size(), max_hist);
  if(idim < 2) return;

  int64_t info = -1;
  int64_t N    = idim+1;
  std::vector<double> X(N, 0.);

  Tensor<TensorType> dhi_trace{};
  auto [mu, nu] = tAO.labels<2>("all");
  Tensor<TensorType>::allocate(&ec,dhi_trace); 

  // ----- Construct Pulay matrix -----
  Matrix A = Matrix::Zero(idim + 1, idim + 1);
  Tensor<TensorType> dFij{tAO, tAO};
  Tensor<TensorType> dDij{tAO, tAO};
  Tensor<TensorType>::allocate(&ec,dFij,dDij);

  for(int i = 0; i < idim; i++) {
    for(int j = i+1; j < idim; j++) {
      sch
        (dFij()       = fock_hist[i]())
        (dFij()      -= fock_hist[j]())
        (dDij()       = D_hist[i]())
        (dDij()      -= D_hist[j]())
        (dhi_trace()  = dFij(nu,mu) * dDij(mu,nu))
        .execute();
      double dhi = get_scalar(dhi_trace); 
      A(i+1,j+1) = dhi;
    }
  }
  Tensor<TensorType>::deallocate(dFij,dDij);

  for(int i = 1; i <= idim; i++) {
    for(int j = i+1; j <= idim; j++) { 
      A(j, i) = A(i, j); 
    }
  }

  for(int i = 1; i <= idim; i++) {
    A(i, 0) = -1.0;
    A(0, i) = -1.0;
  }

  // if(rank == 0) cout << std::setprecision(8) << "in ediis, A: " << endl << A << endl;

  while(info!=0) {

    N  = idim+1;
    std::vector<double> AC( N*(N+1)/2 );
    std::vector<double> AF( AC.size() );
    std::vector<double> B(N, 0.); B.front() = -1.0;
    // if(rank == 0) cout << std::setprecision(8) << "in ediis, B ini: " << endl << B << endl;
    for(int i = 1; i < N; i++) {
      double dhi = get_scalar(ehf_tamm_hist[i-1]);
      // if(rank==0) cout << "i,N,dhi: " << i << "," << N << "," << dhi << endl;
      B[i] = dhi;
    }
    X.resize(N);

    if(rank == 0) cout << std::setprecision(8) << "in ediis, B: " << endl << B << endl;

    int ac_i = 0;
    for(int i = 0; i <= idim; i++) {
      for(int j = i; j <= idim; j++) { 
        AC[ac_i] = A(j, i); ac_i++; 
      }
    }

    TensorType RCOND;
    std::vector<int64_t> IPIV(N);
    std::vector<double>  BERR(1), FERR(1); // NRHS

    info = lapack::spsvx( lapack::Factored::NotFactored, lapack::Uplo::Lower, N, 1, AC.data(), AF.data(), 
                          IPIV.data(), B.data(), N, X.data(), N, &RCOND, FERR.data(), BERR.data() );
      
    if(info!=0) {
      // if(rank==0) cout << "<E-DIIS> Singularity in Pulay matrix. Density and Fock difference matrices removed." << endl;
      fock_hist.erase(fock_hist.begin());
      D_hist.erase(D_hist.begin());
      idim--;
      if(idim==1) return;
      Matrix A_pl = A.block(2,2,idim,idim);
      Matrix A_new  = Matrix::Zero(idim + 1, idim + 1);

      for(int i = 1; i <= idim; i++) {
          A_new(i, 0) = -1.0;
          A_new(0, i) = -1.0;
      }      
      A_new.block(1,1,idim,idim) = A_pl;
      A=A_new;
    }

  } //while

  Tensor<TensorType>::deallocate(dhi_trace); 

  // if(rank == 0) cout << std::setprecision(8) << "in ediis, X: " << endl << X << endl;

  std::vector<double> X_final(N);
  //Reordering [0...N] -> [1...N,0] to match eigen's lu solve
  X_final.back() = X.front();
  std::copy ( X.begin()+1, X.end(), X_final.begin() );

  if(rank == 0) cout << std::setprecision(8) << "in ediis, X_reordered: " << endl << X_final << endl;

  sch
    (D() = 0)
    (F() = 0)
    (ehf_tamm() = 0)
    .execute();
  for(int j = 0; j < idim; j++) { 
    sch
      (D() += X_final[j] * D_hist[j]())
      (F() += X_final[j] * fock_hist[j]())
      (ehf_tamm() += X_final[j] * ehf_tamm_hist[j]())
      ; 
  }
  sch.execute();

  // if(rank == 0) cout << "end of ediis" << endl;

}

template<typename TensorType>
void diis(ExecutionContext& ec, const TiledIndexSpace& tAO, Tensor<TensorType> D, Tensor<TensorType> F, 
          Tensor<TensorType> err_mat, int iter, int max_hist, const SCFVars& scf_vars, const int n_lindep,
          std::vector<Tensor<TensorType>>& diis_hist, std::vector<Tensor<TensorType>>& fock_hist) {
  
  using Vector =
      Eigen::Matrix<TensorType, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

  tamm::Scheduler sch{ec};

  auto rank = ec.pg().rank().value();
  auto ndiis = scf_vars.idiis;

  if(ndiis > max_hist) {
    auto maxe = 0;
    if(!scf_vars.switch_diis) {
      std::vector<TensorType> max_err(diis_hist.size());
      for (size_t i=0; i<diis_hist.size(); i++) {
        max_err[i] = tamm::norm(diis_hist[i]);
      }

      maxe = std::distance(max_err.begin(), 
             std::max_element(max_err.begin(),max_err.end()));
    }
    diis_hist.erase(diis_hist.begin()+maxe);
    fock_hist.erase(fock_hist.begin()+maxe);      
  }
  else{
    if(ndiis == (int)(max_hist/2) && n_lindep > 1) {
      diis_hist.clear();
      fock_hist.clear();
    }
  }

  Tensor<TensorType> Fcopy{tAO, tAO};
  Tensor<TensorType>::allocate(&ec,Fcopy);
  sch(Fcopy() = F()).execute();

  diis_hist.push_back(err_mat);
  fock_hist.push_back(Fcopy);

  Matrix A;
  Vector b;
  int  idim = std::min((int)diis_hist.size(), max_hist);
  int64_t info = -1;
  int64_t N = idim+1;
  std::vector<TensorType> X;

  // Tensor<TensorType> dhi_trans{tAO, tAO};
  Tensor<TensorType> dhi_trace{};
  auto [mu, nu] = tAO.labels<2>("all");
  Tensor<TensorType>::allocate(&ec,dhi_trace); //dhi_trans

  // ----- Construct Pulay matrix -----
  A       = Matrix::Zero(idim + 1, idim + 1);

  for(int i = 0; i < idim; i++) {
      for(int j = i; j < idim; j++) {
          //A(i, j) = (diis_hist[i].transpose() * diis_hist[j]).trace();
          sch(dhi_trace() = diis_hist[i](nu,mu) * diis_hist[j](mu,nu)).execute();
          TensorType dhi = get_scalar(dhi_trace); //dhi_trace.trace();
          A(i+1,j+1) = dhi;
      }
  }

  for(int i = 1; i <= idim; i++) {
      for(int j = i; j <= idim; j++) { A(j, i) = A(i, j); }
  }

  for(int i = 1; i <= idim; i++) {
      A(i, 0) = -1.0;
      A(0, i) = -1.0;
  }

  while(info!=0) {

    if(idim==1) return;

    N  = idim+1;
    std::vector<TensorType> AC( N*(N+1)/2 );
    std::vector<TensorType> AF( AC.size() );
    std::vector<TensorType> B(N, 0.); B.front() = -1.0;
    X.resize(N);

    int ac_i = 0;
    for(int i = 0; i <= idim; i++) {
        for(int j = i; j <= idim; j++) { AC[ac_i] = A(j, i); ac_i++; }
    }

    TensorType RCOND;
    std::vector<int64_t> IPIV(N);
    std::vector<TensorType>  BERR(1), FERR(1); // NRHS
    info = lapack::spsvx( lapack::Factored::NotFactored, lapack::Uplo::Lower, N, 1, AC.data(), AF.data(), 
                          IPIV.data(), B.data(), N, X.data(), N, &RCOND, FERR.data(), BERR.data() );
      
    if(info!=0) {
      if(rank==0) cout << "<DIIS> Singularity in Pulay matrix detected." /*Error and Fock matrices removed." */ << endl;
      diis_hist.erase(diis_hist.begin());
      fock_hist.erase(fock_hist.begin());            
      idim--;
      if(idim==1) return;
      Matrix A_pl = A.block(2,2,idim,idim);
      Matrix A_new  = Matrix::Zero(idim + 1, idim + 1);

      for(int i = 1; i <= idim; i++) {
          A_new(i, 0) = -1.0;
          A_new(0, i) = -1.0;
      }      
      A_new.block(1,1,idim,idim) = A_pl;
      A=A_new;
    }

  } //while

  Tensor<TensorType>::deallocate(dhi_trace); //dhi_trans

  std::vector<TensorType> X_final(N);
  //Reordering [0...N] -> [1...N,0] to match eigen's lu solve
  X_final.back() = X.front();
  std::copy ( X.begin()+1, X.end(), X_final.begin() );

  //if(rank==0 && scf_options.debug) 
  // std::cout << "diis weights sum, vector: " << std::setprecision(5) << std::accumulate(X.begin(),X.end(),0.0d) << std::endl << X << std::endl << X_final << std::endl;

  sch(F() = 0).execute();
  for(int j = 0; j < idim; j++) { 
    sch(F() += X_final[j] * fock_hist[j]()); 
  }
  // sch(F() += 0.5 * D()); //level shift
  sch.execute();

}
