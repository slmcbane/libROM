/******************************************************************************
 *
 * Copyright (c) 2013-2019, Lawrence Livermore National Security, LLC
 * and other libROM project developers. See the top-level COPYRIGHT
 * file for details.
 *
 * SPDX-License-Identifier: (Apache-2.0 OR MIT)
 *
 *****************************************************************************/

// Description: A class that reads basis vectors from a file.

#include "BasisReader.h"
#include "HDFDatabase.h"
#include "Matrix.h"
#include "mpi.h"

namespace CAROM {

BasisReader::BasisReader(
   const std::string& base_file_name,
   Database::formats db_format) :
   d_spatial_basis_vectors(NULL),
   d_temporal_basis_vectors(0),
   d_singular_values(0),
   d_last_basis_idx(-1)
{
   CAROM_ASSERT(!base_file_name.empty());

   int mpi_init;
   MPI_Initialized(&mpi_init);
   int rank;
   if (mpi_init) {
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
   }
   else {
      rank = 0;
   }
   char tmp[100];
   sprintf(tmp, ".%06d", rank);
   std::string full_file_name = base_file_name + tmp;
   if (db_format == Database::HDF5) {
      d_database = new HDFDatabase();
   }
   d_database->open(full_file_name);
   int num_time_intervals;
   d_database->getInteger("num_time_intervals", num_time_intervals);
   d_time_interval_start_times.resize(num_time_intervals);
   for (int i = 0; i < num_time_intervals; ++i) {
      sprintf(tmp, "time_%06d", i);
      d_database->getDouble(tmp, d_time_interval_start_times[i]);
   }
}

BasisReader::~BasisReader()
{
   delete d_spatial_basis_vectors;
   delete d_temporal_basis_vectors;
   delete d_singular_values;
   d_database->close();
   delete d_database;
}

void
BasisReader::readBasis(
   const std::string& base_file_name,
   Database::formats db_format)
{
   CAROM_ASSERT(!base_file_name.empty());

   int mpi_init;
   MPI_Initialized(&mpi_init);
   int rank;
   if (mpi_init) {
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
   }
   else {
      rank = 0;
   }
   char tmp[100];
   sprintf(tmp, ".%06d", rank);
   std::string full_file_name = base_file_name + tmp;
   if (db_format == Database::HDF5) {
      delete d_database;
      d_database = new HDFDatabase();
   }
   d_database->open(full_file_name);
   int num_time_intervals;
   double foo;
   d_database->getDouble("num_time_intervals", foo);
   num_time_intervals = static_cast<int>(foo);
   d_time_interval_start_times.resize(num_time_intervals);
   for (int i = 0; i < num_time_intervals; ++i) {
      sprintf(tmp, "time_%06d", i);
      d_database->getDouble(tmp, d_time_interval_start_times[i]);
   }
}

const Matrix*
BasisReader::getSpatialBasis(
   double time)
{
   CAROM_ASSERT(0 < numTimeIntervals());
   CAROM_ASSERT(0 <= time);
   int num_time_intervals = numTimeIntervals();
   int i;
   for (i = 0; i < num_time_intervals-1; ++i) {
      if (d_time_interval_start_times[i] <= time &&
          time < d_time_interval_start_times[i+1]) {
         break;
      }
   }
   d_last_basis_idx = i;
   char tmp[100];
   int num_rows;
   sprintf(tmp, "spatial_basis_num_rows_%06d", i);
   d_database->getInteger(tmp, num_rows);
   int num_cols;
   sprintf(tmp, "spatial_basis_num_cols_%06d", i);
   d_database->getInteger(tmp, num_cols);
   if (d_spatial_basis_vectors) {
      delete d_spatial_basis_vectors;
   }
   d_spatial_basis_vectors = new Matrix(num_rows, num_cols, true);
   sprintf(tmp, "spatial_basis_%06d", i);
   d_database->getDoubleArray(tmp,
                              &d_spatial_basis_vectors->item(0, 0),
                              num_rows*num_cols);
   return d_spatial_basis_vectors;
}

const Matrix*
BasisReader::getTemporalBasis(
   double time)
{
   CAROM_ASSERT(0 < numTimeIntervals());
   CAROM_ASSERT(0 <= time);
   int num_time_intervals = numTimeIntervals();
   int i;
   for (i = 0; i < num_time_intervals-1; ++i) {
      if (d_time_interval_start_times[i] <= time &&
          time < d_time_interval_start_times[i+1]) {
         break;
      }
   }
   d_last_basis_idx = i;
   char tmp[100];
   int num_rows;
   sprintf(tmp, "temporal_basis_num_rows_%06d", i);
   d_database->getInteger(tmp, num_rows);
   int num_cols;
   sprintf(tmp, "temporal_basis_num_cols_%06d", i);
   d_database->getInteger(tmp, num_cols);
   if (d_temporal_basis_vectors) {
      delete d_temporal_basis_vectors;
   }
   d_temporal_basis_vectors = new Matrix(num_rows, num_cols, true);
   sprintf(tmp, "temporal_basis_%06d", i);
   d_database->getDoubleArray(tmp,
                              &d_temporal_basis_vectors->item(0, 0),
                              num_rows*num_cols);
   return d_temporal_basis_vectors;
}

const Matrix*
BasisReader::getSingularValues(
   double time)
{
   CAROM_ASSERT(0 < numTimeIntervals());
   CAROM_ASSERT(0 <= time);
   int num_time_intervals = numTimeIntervals();
   int i;
   for (i = 0; i < num_time_intervals-1; ++i) {
      if (d_time_interval_start_times[i] <= time &&
          time < d_time_interval_start_times[i+1]) {
         break;
      }
   }
   d_last_basis_idx = i;
   char tmp[100];
   int size;
   sprintf(tmp, "singular_value_size_%06d", i);
   d_database->getInteger(tmp, size);
   if (d_singular_values) {
      delete d_singular_values;
   }
   d_singular_values = new Matrix(size, size, true);
   sprintf(tmp, "singular_value_%06d", i);
   d_database->getDoubleArray(tmp,
                              &d_singular_values->item(0, 0),
                              size*size);
   return d_singular_values;
}

Matrix
BasisReader::getMatlabBasis(
   double time)
{
   CAROM_ASSERT(0 < numTimeIntervals());
   CAROM_ASSERT(0 <= time);
   int num_time_intervals = numTimeIntervals();
   int i;
   for (i = 0; i < num_time_intervals-1; ++i) {
      if (d_time_interval_start_times[i] <= time &&
          time < d_time_interval_start_times[i+1]) {
         break;
      }
   }
   d_last_basis_idx = i;
   char tmp[100];
   int num_rows;
   double foo;
   sprintf(tmp, "num_rows_%06d", i);
   d_database->getDouble(tmp, foo);
   num_rows = static_cast<int>(foo);
   int num_cols;
   sprintf(tmp, "num_cols_%06d", i);
   d_database->getDouble(tmp, foo);
   num_cols = static_cast<int>(foo);
   if (d_spatial_basis_vectors) {
      delete d_spatial_basis_vectors;
   }
   d_spatial_basis_vectors = new Matrix(num_rows, num_cols, true);
   sprintf(tmp, "matlab_spatial_basis_%06d", i);
   d_database->getDoubleArray(tmp,
                              &d_spatial_basis_vectors->item(0, 0),
                              num_rows*num_cols);
   return *d_spatial_basis_vectors;
}
}
