/******************************************************************************
 *
 * Copyright (c) 2013-2019, Lawrence Livermore National Security, LLC
 * and other libROM project developers. See the top-level COPYRIGHT
 * file for details.
 *
 * SPDX-License-Identifier: (Apache-2.0 OR MIT)
 *
 *****************************************************************************/

// Description: The class that determines the next time at which a sample
//              should be taken for basis generation using an incremental SVD
//              approach.

#include "SVDSampler.h"
#include "IncrementalSVDSampler.h"
#include "IncrementalSVDStandard.h"
#include "IncrementalSVDFastUpdate.h"

#include "mpi.h"

#include <cmath>

namespace CAROM {

IncrementalSVDSampler::IncrementalSVDSampler(
   int dim,
   double linearity_tol,
   bool skip_linearly_dependent,
   bool fast_update,
   int max_basis_dimension,
   double initial_dt,
   int samples_per_time_interval,
   double sampling_tol,
   double max_time_between_samples,
   const std::string& basis_file_name,
   bool save_state,
   bool restore_state,
   bool updateRightSV,
   double min_sampling_time_step_scale,
   double sampling_time_step_scale,
   double max_sampling_time_step_scale,
   bool debug_algorithm) :
   d_tol(sampling_tol),
   d_max_time_between_samples(max_time_between_samples),
   d_min_sampling_time_step_scale(min_sampling_time_step_scale),
   d_sampling_time_step_scale(sampling_time_step_scale),
   d_max_sampling_time_step_scale(max_sampling_time_step_scale),
   d_dt(initial_dt),
   d_next_sample_time(0.0)
{
   CAROM_ASSERT(initial_dt > 0.0);
   CAROM_ASSERT(sampling_tol > 0.0);
   CAROM_ASSERT(max_time_between_samples > 0.0);
   CAROM_ASSERT(min_sampling_time_step_scale >= 0.0);
   CAROM_ASSERT(sampling_time_step_scale >= 0.0);
   CAROM_ASSERT(max_sampling_time_step_scale >= 0.0);
   CAROM_ASSERT(min_sampling_time_step_scale <= max_sampling_time_step_scale);

   d_updateRightSV = updateRightSV;

   if (fast_update) {
      d_svd.reset(
         new IncrementalSVDFastUpdate(dim,
            linearity_tol,
            skip_linearly_dependent,
            max_basis_dimension,
            samples_per_time_interval,
            basis_file_name,
            save_state,
            restore_state,
            updateRightSV,
            debug_algorithm));
   }
   else {
      d_svd.reset(
         new IncrementalSVDStandard(dim,
            linearity_tol,
            skip_linearly_dependent,
            max_basis_dimension,
            samples_per_time_interval,
            basis_file_name,
            save_state,
            restore_state,
            updateRightSV,
            debug_algorithm));
   }

   // Get the number of processors.
   int mpi_init;
   MPI_Initialized(&mpi_init);
   if (mpi_init) {
     MPI_Comm_size(MPI_COMM_WORLD, &d_num_procs);
   }
   else {
      d_num_procs = 1;
   }
}

IncrementalSVDSampler::~IncrementalSVDSampler()
{
}

bool
IncrementalSVDSampler::isNextSample(
   double time)
{
   if(d_updateRightSV)
     return true;
   else
     return time >= d_next_sample_time;
}

double
IncrementalSVDSampler::computeNextSampleTime(
   double* u_in,
   double* rhs_in,
   double time)
{
   CAROM_ASSERT(u_in != 0);
   CAROM_ASSERT(rhs_in != 0);
   CAROM_ASSERT(time >= 0.0);

   // Check that u_in is not non-zero.
   int dim = d_svd->getDim();
   Vector u_vec(u_in, dim, true);
   if (u_vec.norm() == 0.0) {
      return d_next_sample_time;
   }

   // Get the current basis vectors.
   const Matrix* basis = getSpatialBasis();

   // Compute l = basis' * u
   Vector* l = basis->transposeMult(u_vec);

   // basisl = basis * l
   Vector* basisl = basis->mult(l);

   // Compute u - basisl.
   Vector* eta = u_vec.minus(basisl);

   delete l;
   delete basisl;

   // Compute l = basis' * rhs
   Vector rhs_vec(rhs_in, dim, true);
   l = basis->transposeMult(rhs_vec);

   // basisl = basis * l
   basisl = basis->mult(l);

   // Compute rhs - basisl.
   Vector* eta_dot = rhs_vec.minus(basisl);

   delete l;
   delete basisl;

   // Compute the l-inf norm of eta + d_dt*eta_dot.
   double global_norm;
   double local_norm = 0.0;
   for (int i = 0; i < dim; ++i) {
      double val = fabs(eta->item(i) + d_dt*eta_dot->item(i));
      if (val > local_norm) {
         local_norm = val;
      }
   }
   delete eta;
   delete eta_dot;
   if (d_num_procs == 1) {
      global_norm = local_norm;
   }
   else {
     MPI_Allreduce(&local_norm,
        &global_norm,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        MPI_COMM_WORLD);
   }

   // Compute dt from this norm.
   double tmp = d_sampling_time_step_scale*sqrt(d_tol/global_norm);
   if (tmp < d_min_sampling_time_step_scale) {
      d_dt *= d_min_sampling_time_step_scale;
   }
   else if (tmp > d_max_sampling_time_step_scale) {
      d_dt *= d_max_sampling_time_step_scale;
   }
   else {
      d_dt *= tmp;
   }
   if (d_dt < 0) {
      d_dt = 0.0;
   }
   else if (d_dt > d_max_time_between_samples) {
      d_dt = d_max_time_between_samples;
   }

   // Return next sample time.
   d_next_sample_time = time + d_dt;
   return d_next_sample_time;
}

void
IncrementalSVDSampler::resetDt(
   double new_dt)
{
   d_dt = new_dt;
}

}
