#include "ga/macdecls.h"
#include "mpi.h"
#include <chrono>
#include <tamm/tamm.hpp>

using namespace tamm;

template<typename T>
void test_2_dim_mult_op(Scheduler& sch, size_t N, Tile tilesize, ExecutionHW ex_hw, bool profile) {
    TiledIndexSpace tis1{IndexSpace{range(N)}, tilesize};

    auto [i, j, k] = tis1.labels<3>("all");

    Tensor<T> A{i, k};
    Tensor<T> B{k, j};
    Tensor<T> C{i, j};

    sch.allocate(A, B, C).execute();
    sch(A() = 21.0)(B() = 2.0)(C() = 0.0).execute();

    const auto timer_start = std::chrono::high_resolution_clock::now();

    sch(C(j, i) += A(i, k) * B(k, j)).execute(ex_hw, profile);

    const auto timer_end = std::chrono::high_resolution_clock::now();

    auto mult_time = std::chrono::duration_cast<std::chrono::duration<double>>(
                         (timer_end - timer_start))
                         .count();

    if (sch.ec().pg().rank() == 0)
      std::cout << "2-D Tensor contraction with " << N << " indices tiled with "
                << tilesize << " : " << mult_time << std::endl;
    
    sch.deallocate(A, B, C).execute();

    // 2D dense case
    // ExecutionContext ec{pg, DistributionKind::dense, MemoryManagerKind::ga};
    // using Matrix   = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

    // TiledIndexSpace AO{IndexSpace{range(N)}, 50};
    // TiledIndexSpace AO{IndexSpace{range(N)}, {2,2,1}};
    // auto [mu,nu,ku] = AO.labels<3>("all");
    // Tensor<T> X1{AO,AO};
    // Tensor<T> X2{AO,AO};
    // Tensor<T> X3{AO,AO};
    // X1.set_dense();
    // X2.set_dense();
    // X3.set_dense();
    // Scheduler{ec}.allocate(X1,X2,X3)
    // (X2()=3.2)
    // (X3()=2.4)
    // (X1(mu,nu) = X2(mu,ku) * X3(ku,nu)).execute();
    
    // if(pg.rank()==0) {
    //   Matrix X1_e(N,N);
    //   std::cout << "----------------" << std::endl;
    //   X1_e.setRandom();
    //   std::cout << X1_e << std::endl;
    //   eigen_to_tamm_tensor<double>(X1,X1_e);
    // }
    // Tensor<T> X1_b = tensor_block(X1, {0,0}, {2,2});
    // Tensor<T> X1_bp = permute_tensor(X1_b,{1,0});

    // if(pg.rank()==0) {
    //   std::cout << "----------------" << std::endl;
    //   Matrix X_eig = tamm_to_eigen_matrix<double>(X1_b);
    //   std::cout << X_eig << std::endl;
    //   X_eig = tamm_to_eigen_matrix<double>(X1_bp);
    //   std::cout << "----------------" << std::endl;
    //   std::cout << X_eig << std::endl;
    // }

    // sch.deallocate(X1,X2,X3,X1_b,X1_bp).execute();  
    // X1 = {AO,AO,AO};
    // X2 = {AO,AO,AO};
    // X3 = {AO,AO,AO};
    // Tensor<T>::set_dense(X1,X2,X3);
    // sch.allocate(X1,X2,X3).execute();  
    // sch(X2()=3.2)
    // (X3()=2.3)
    // (X1(mu,nu,ku) = X2(mu,nu,lu)* X3(lu,nu,ku)).execute();    


    // if(pg.rank()==0) {
    //   std::cout << "----------------" << std::endl;
    //   Tensor3D X1_e(N,N,N);
    //   X1_e.setRandom();
    //   eigen_to_tamm_tensor<double>(X1,X1_e);
    //   std::cout << X1_e << std::endl;
    // }

    // X1_b = tensor_block(X1, {0,0,0}, {2,1,2});
    // if(pg.rank() == 0) std::cout << "X1_b is BC: " << X1_b.is_block_cyclic() << std::endl;
    // Tensor<T> X1_bp3 = permute_tensor(X1_b,{2,0,1});
    // if(pg.rank()==0) {
    //   std::cout << "----------------" << std::endl;
    //   Tensor3D X_eig = tamm_to_eigen_tensor<double,3>(X1_b);
    //   std::cout << X_eig << std::endl; 
    //   std::cout << "----------------" << std::endl;
    //   X_eig = tamm_to_eigen_tensor<double,3>(X1_bp3);
    //   std::cout << X_eig.dimensions() << std::endl;       
    //   std::cout << X_eig << std::endl;       
    // }           

    // std::random_device rd;
    // std::mt19937                     e2(rd());
    // std::uniform_real_distribution<> dist(0, 25);
    // double* fptr = F_BC.access_local_buf();
    // for(int i=0;i<F_BC.local_buf_size();i++)
    //  fptr[i] = dist(e2);
    
    // std::cout << "rank " << scalapack_info.pg.rank().value() << std::endl;
    // for(int i=0;i<F_BC.local_buf_size();i++) {
    //  if(i%11==0) std::cout << "\n";
    //  std::cout << fptr[i] << "\t";
    // }
    // std::cout << "\n";

    // // print_tensor(F_BC);
    // // if(scalapack_info.pg.rank() == 0) print_tensor(F_BC);
    // Tensor<T> F1 = from_block_cyclic_tensor(F_BC);
    // if(scalapack_info.pg.rank() == 0) std::cout << "f1 pg = " << F1.proc_grid()[0].value() << "," << F1.proc_grid()[1].value() << "\n";
    // if(scalapack_info.pg.rank() == 0) print_tensor(F1);
    // // eigen_to_tamm_tensor<double>(F1,X1_e);

    // Tensor<T> X1_bp3 = tensor_block(F1, {8,0}, {11,11}, {1,0});
    // if(scalapack_info.pg.rank() == 0) {
    //   std::cout << X1_e << "\n";
    //   std::cout << std::string(16,'-') << "\n";
    //   X1_e = tamm_to_eigen_matrix<double>(X1_bp3);
    //   std::cout << X1_e << "\n";
    // }
}

template <typename T>
void test_3_dim_mult_op(Scheduler& sch, size_t N, Tile tilesize, ExecutionHW ex_hw, bool profile) {
  TiledIndexSpace tis1{IndexSpace{range(N)}, tilesize};

  auto [i, j, k, l, m] = tis1.labels<5>("all");

  Tensor<T> A{i, j, l};
  Tensor<T> B{l, m, k};
  Tensor<T> C{i, j, k};

  sch.allocate(A, B, C)(A() = 21.0)(B() = 2.0)(C() = 0.0).execute();

  const auto timer_start = std::chrono::high_resolution_clock::now();

  sch(C(j, i, k) += A(i, j, l) * B(l, m, k)).execute(ex_hw, profile);

  const auto timer_end = std::chrono::high_resolution_clock::now();

  auto mult_time = std::chrono::duration_cast<std::chrono::duration<double>>(
                       (timer_end - timer_start))
                       .count();

  if (sch.ec().pg().rank() == 0)
    std::cout << "3-D Tensor contraction with " << N << " indices tiled with "
              << tilesize << " : " << mult_time << std::endl;

  sch.deallocate(A, B, C).execute();
}

template <typename T>
void test_4_dim_mult_op(Scheduler& sch, size_t N, Tile tilesize, ExecutionHW ex_hw, bool profile) {
  TiledIndexSpace tis1{IndexSpace{range(N)}, tilesize};

  auto [i, j, k, l, m, o] = tis1.labels<6>("all");

  Tensor<T> A{i, j, m, o};
  Tensor<T> B{m, o, k, l};
  Tensor<T> C{i, j, k, l};

  sch.allocate(A, B, C)(A() = 21.0)(B() = 2.0)(C() = 0.0).execute();

  const auto timer_start = std::chrono::high_resolution_clock::now();

  sch//.exact_copy(A(i, j, m, o), B(m, o, k, l))
  (C(j, i, k, l) += A(i, j, m, o) * B(m, o, k, l)).execute(ex_hw, profile);

  const auto timer_end = std::chrono::high_resolution_clock::now();

  auto mult_time = std::chrono::duration_cast<std::chrono::duration<double>>(
                       (timer_end - timer_start))
                       .count();

  if (sch.ec().pg().rank() == 0)
    std::cout << "4-D Tensor contraction with " << N << " indices tiled with "
              << tilesize << " : " << mult_time << std::endl;

  sch.deallocate(A, B, C).execute();
}

template <typename T>
void test_4_dim_mult_op_last_unit(Scheduler& sch, size_t N, Tile tilesize, ExecutionHW ex_hw, bool profile) {
  TiledIndexSpace tis1{IndexSpace{range(N)}, tilesize};
  size_t size = N / 10 > 0 ? N / 10 : 1;
  TiledIndexSpace tis2{IndexSpace{range(size)}};

  auto [i, j, k, l] = tis1.labels<4>("all");
  auto [m, o] = tis2.labels<2>("all");

  Tensor<T> A{m, o};
  Tensor<T> B{i, j, k, m};
  Tensor<T> C{i, j, k, m};

  sch.allocate(A, B, C)(A() = 21.0)(B() = 2.0)(C() = 0.0).execute();

  const auto timer_start = std::chrono::high_resolution_clock::now();

  sch(C(j, i, k, o) += A(m, o) * B(i, j, k, m)).execute(ex_hw, profile);

  const auto timer_end = std::chrono::high_resolution_clock::now();

  auto mult_time = std::chrono::duration_cast<std::chrono::duration<double>>(
                       (timer_end - timer_start))
                       .count();

  if (sch.ec().pg().rank() == 0)
    std::cout << "4-D Tensor contraction with 2-D unit tiled matrix " << N
              << " indices tiled with " << tilesize
              << " last index unit tiled : " << mult_time << std::endl;
}

template <typename T>
void test_4_dim_mult_op_first_unit(Scheduler& sch, size_t N, Tile tilesize, ExecutionHW ex_hw, bool profile) {
  TiledIndexSpace tis1{IndexSpace{range(N)}, tilesize};
  size_t size = N / 10 > 0 ? N / 10 : 1;
  TiledIndexSpace tis2{IndexSpace{range(size)}};
  TiledIndexSpace tis3{IndexSpace{range(tilesize)}, tilesize};

  auto [i, j, k, l, m, o] = tis1.labels<6>("all");
  auto [t1, t2] = tis3.labels<2>("all");
  // auto [m, o] = tis2.labels<2>("all");

  Tensor<T> A{m, t1};
  Tensor<T> B{t1, m};
  Tensor<T> C{m, i};

  sch.allocate(A, B, C)(A() = 21.0)(B() = 2.0)(C() = 0.0).execute();

  const auto timer_start = std::chrono::high_resolution_clock::now();

  sch(C(m, i) += A(m, t1) * B(t1, i)).execute(ex_hw, profile);

  const auto timer_end = std::chrono::high_resolution_clock::now();

  auto mult_time = std::chrono::duration_cast<std::chrono::duration<double>>(
                       (timer_end - timer_start))
                       .count();

  if (sch.ec().pg().rank() == 0)
    std::cout << "4-D Tensor contraction with 2-D unit tiled matrix " << N
              << " indices tiled with " << tilesize
              << " first index unit tiled : " << mult_time << std::endl;
}

int main(int argc, char* argv[]) {

  tamm::initialize(argc, argv);

  if(argc < 3) {
    tamm_terminate("Please provide an index space size and tile size");
  }

  size_t is_size   = atoi(argv[1]);
  Tile   tile_size = atoi(argv[2]);

  if(is_size < tile_size) tile_size = is_size;

  ProcGroup pg = ProcGroup::create_coll(GA_MPI_Comm());
  ExecutionContext ec{pg, DistributionKind::nw, MemoryManagerKind::ga};

  ExecutionHW ex_hw = ExecutionHW::CPU;
  #ifdef USE_DPCPP
  ex_hw = ExecutionHW::GPU;
  #endif
  #ifdef USE_TALSH
  ex_hw = ExecutionHW::GPU;
  const bool has_gpu = ec.has_gpu();
  TALSH talsh_instance;
  if(has_gpu) talsh_instance.initialize(ec.gpu_devid(),ec.pg().rank().value());
  #endif

  Scheduler sch{ec};

  if(ec.pg().rank() == 0) std::cout << "tilesize = " << tile_size << std::endl;

  const bool profile = true;
  test_2_dim_mult_op<double>(sch, is_size, tile_size, ex_hw, profile);
  test_3_dim_mult_op<double>(sch, is_size, tile_size, ex_hw, profile);
  test_4_dim_mult_op<double>(sch, is_size, tile_size, ex_hw, profile);
  // test_4_dim_mult_op_last_unit<double>(sch, is_size, tile_size);
  // test_4_dim_mult_op_first_unit<double>(sch, is_size, tile_size);

  #ifdef USE_TALSH
  //talshStats();
  if(has_gpu) talsh_instance.shutdown();
  #endif

  if(profile) {
    std::string profile_csv = "multops_profile.csv";
    std::ofstream pds(profile_csv, std::ios::out);
    if(!pds) std::cerr << "Error opening file " << profile_csv << std::endl;
    std::string header = "ID;Level;OP;total_op_time_min;total_op_time_max;total_op_time_avg;";
    header += "get_time_min;get_time_max;get_time_avg;gemm_time_min;";
    header += "gemm_time_max;gemm_time_avg;acc_time_min;acc_time_max;acc_time_avg";
    pds << header << std::endl;
    pds << ec.get_profile_data().str() << std::endl;
    pds.close();
  }

  tamm::finalize();

  return 0;
}
