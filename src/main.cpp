#include <tadah.mlip/cli/tadah_cli.h>

#include <cstdio>
#include <cstring>
#include <exception>
#include <iostream>

#ifdef TADAH_HPO_MPI
#include <mpi.h>

#include <chrono>
#include <thread>

namespace {
/* A rank that ends with an error must not leave its peers blocked in a
 * collective: MPI_Finalize would then wait for them forever and the job
 * would burn its allocation instead of exiting non-zero. In a TADAH_HPO_MPI
 * run that follows a failure confined to one rank outside the phases whose
 * ending the ranks agree on (HPO run_stop.h, agree_on_rank_stop): a
 * NOISE_CALIBRATION evaluation that fails, or that a signal stops, on one
 * rank leaves the others in the driver's next collective. Only a rank that
 * returns from cli.run() gets here: one that is itself blocked in a
 * collective never does, so this cannot end such a job. Every rank that
 * reaches the end of main() enters a
 * barrier on a communicator of its own -- so it can never be matched
 * against a collective a stuck peer sits in on MPI_COMM_WORLD. When all
 * ranks arrive (all succeeded, or all failed together: each has closed its
 * own journal) they finalize normally. A failing rank whose peers have not
 * all arrived within kExitGrace aborts the job with its status: they are
 * blocked. Its own journal is already closed (hpo_run writes RESULT before
 * the error leaves it). */
MPI_Comm exit_comm = MPI_COMM_NULL;
constexpr std::chrono::seconds kExitGrace{30};

void create_exit_comm()
{
  MPI_Comm_dup(MPI_COMM_WORLD, &exit_comm);
}

void exit_handshake(int ret)
{
  int world = 1;
  MPI_Comm_size(MPI_COMM_WORLD, &world);
  if (world < 2 || exit_comm == MPI_COMM_NULL) return;
  /* Nonblocking on every rank: a blocking MPI_Barrier would not match. */
  MPI_Request req = MPI_REQUEST_NULL;
  MPI_Ibarrier(exit_comm, &req);
  if (ret == 0) {
    /* A peer that failed waits for this rank; if one is stuck, a failing
     * rank aborts the job (a successful rank never does). */
    MPI_Wait(&req, MPI_STATUS_IGNORE);
    return;
  }
  const auto deadline = std::chrono::steady_clock::now() + kExitGrace;
  int arrived = 0;
  for (;;) {
    MPI_Test(&req, &arrived, MPI_STATUS_IGNORE);
    if (arrived || std::chrono::steady_clock::now() >= deadline) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  if (arrived) return;
  std::cerr << "ERROR: exiting with status " << ret << "; not every MPI rank "
            << "reached the end within " << kExitGrace.count()
            << " s (a rank is blocked) -- aborting the job" << std::endl;
  std::cout.flush();
  std::fflush(nullptr);
  MPI_Abort(MPI_COMM_WORLD, ret);
}
}  // namespace
#endif

/**
 * @brief Main entry point.
 *
 * Instantiates the TadahCLI and parses command-line arguments. Uncaught
 * exceptions from tasks surface as a clean one-line ERROR instead of a
 * std::terminate core dump.
 */
/* PC_ISOLATION worker-process entry (HPO/src/pc_worker_main.cpp). Declared
 * here directly (not via header) to keep main.cpp's include surface minimal. */
namespace tadah { namespace hpo { int pc_worker_main(int argc, char** argv); } }

int main(int argc, char** argv) {
  /* PC_ISOLATION: a re-exec'd pc_lammps worker child. MUST be intercepted
   * before ANY other init -- in particular before MPI_Init_thread (a
   * fork+exec'd child of an MPI parent must never touch MPI; the
   * TADAH_HPO_MPI build rejects PROCESS mode at config time and
   * pc_worker_main double-guards). The worker's
   * socket is inherited on fd 3. */
  if (argc >= 2 && std::strcmp(argv[1], "--tadah-pc-worker") == 0)
    return tadah::hpo::pc_worker_main(argc, argv);

#ifdef TADAH_HPO_MPI
  /* The HPO parallel-annealing drivers (TADAH:ANNEAL_PT / ANNEAL_PA)
   * distribute replicas over ranks while every rank's evaluation
   * threads run their own LAMMPS instances on MPI_COMM_SELF -- those
   * instances make MPI calls concurrently, so MPI_THREAD_MULTIPLE is
   * required (Cray MPICH additionally needs
   * `export MPICH_MAX_THREAD_SAFETY=multiple`). */
  {
    int provided = 0;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
      int rank = 0;
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      if (rank == 0)
        std::cerr << "ERROR: this MPI library provides thread level "
                  << provided << " < MPI_THREAD_MULTIPLE, which the "
                  << "TADAH_HPO_MPI build requires (concurrent LAMMPS "
                  << "instances on MPI_COMM_SELF). On Cray MPICH set "
                  << "MPICH_MAX_THREAD_SAFETY=multiple; otherwise use an "
                  << "MPI built with full thread support." << std::endl;
      MPI_Finalize();
      return 1;
    }
    /* Some MPI implementations initialise MCA frameworks LAZILY on
     * first use -- OpenMPI's topo framework, for instance, comes up on
     * the first MPI_Cart_create, which LAMMPS calls during domain
     * decomposition. That lazy path is NOT thread-safe even under
     * MPI_THREAD_MULTIPLE, and the parallel annealing drivers create
     * their first LAMMPS instances from concurrent worker threads.
     * Touch it once here, single-threaded. */
    {
      int dims[1] = {1}, periods[1] = {0};
      MPI_Comm cart = MPI_COMM_NULL;
      MPI_Cart_create(MPI_COMM_SELF, 1, dims, periods, 0, &cart);
      if (cart != MPI_COMM_NULL) MPI_Comm_free(&cart);
    }
    create_exit_comm();
  }
#endif

  int ret = 0;
  try {
    tadah::mlip::cli::TadahCLI cli;
    ret = cli.run(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    ret = 1;
  }

#ifdef TADAH_HPO_MPI
  exit_handshake(ret);   // may MPI_Abort (see above)
  MPI_Finalize();
#endif

  return ret;
}

