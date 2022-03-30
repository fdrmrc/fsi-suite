// ---------------------------------------------------------------------
//
// Copyright (C) 2022 by Luca Heltai
//
// This file is part of the FSI-suite platform, based on the deal.II library.
//
// The FSI-suite platform is free software; you can use it, redistribute it,
// and/or modify it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 3.0 of the License,
// or (at your option) any later version. The full text of the license can be
// found in the file LICENSE at the top level of the FSI-suite platform
// distribution.
//
// ---------------------------------------------------------------------


#include <deal.II/base/config.h>

#include <deal.II/base/function_lib.h>
#include <deal.II/base/quadrature.h>

#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/mapping.h>
#include <deal.II/fe/mapping_q1.h>

#include <deal.II/grid/grid_tools.h>
#include <deal.II/grid/grid_tools_cache.h>
#include <deal.II/grid/tria.h>

#include <deal.II/lac/sparse_matrix.h>

#ifdef DEAL_II_WITH_CGAL


#  include "assemble_coupling_exact.h"


using namespace dealii;
namespace dealii::NonMatching
{
  template <int dim0, int dim1, int spacedim, typename Matrix>
  void
  assemble_coupling_exact(
    const DoFHandler<dim0, spacedim> &                    space_dh,
    const DoFHandler<dim1, spacedim> &                    immersed_dh,
    const std::vector<std::tuple<
      typename dealii::Triangulation<dim0, spacedim>::active_cell_iterator,
      typename dealii::Triangulation<dim1, spacedim>::active_cell_iterator,
      dealii::Quadrature<spacedim>>> &                    cells_and_quads,
    Matrix &                                              matrix,
    const AffineConstraints<typename Matrix::value_type> &space_constraints,
    const ComponentMask &                                 space_comps,
    const ComponentMask &                                 immersed_comps,
    const Mapping<dim0, spacedim> &                       space_mapping,
    const Mapping<dim1, spacedim> &                       immersed_mapping,
    const AffineConstraints<typename Matrix::value_type> &immersed_constraints)
  {
    AssertDimension(matrix.m(), space_dh.n_dofs());
    AssertDimension(matrix.n(), immersed_dh.n_dofs());
    Assert(dim1 <= dim0,
           ExcMessage("This function can only work if dim1<=dim0"));
    // Assert((dynamic_cast<
    //           const parallel::distributed::Triangulation<dim1, spacedim> *>(
    //           &immersed_dh.get_triangulation()) == nullptr),
    //        ExcNotImplemented());



    const auto &space_fe    = space_dh.get_fe();
    const auto &immersed_fe = immersed_dh.get_fe();

    const unsigned int dofs_per_space_cell    = space_fe.n_dofs_per_cell();
    const unsigned int dofs_per_immersed_cell = immersed_fe.n_dofs_per_cell();

    FullMatrix<double> local_cell_matrix(dofs_per_space_cell,
                                         dofs_per_immersed_cell);
    // DoF indices
    std::vector<types::global_dof_index> local_space_dof_indices(
      dofs_per_space_cell);
    std::vector<types::global_dof_index> local_immersed_dof_indices(
      dofs_per_immersed_cell);



    // Loop over vector of tuples, and gather everything together

    for (const auto &infos : cells_and_quads)
      {
        const auto &[first_cell, second_cell, quad_formula] = infos;



        local_cell_matrix = 0.;

        const unsigned int           n_quad_pts = quad_formula.size();
        const auto &                 real_qpts  = quad_formula.get_points();
        std::vector<Point<spacedim>> ref_pts_space(n_quad_pts);
        std::vector<Point<dim1>>     ref_pts_immersed(n_quad_pts);

        space_mapping.transform_points_real_to_unit_cell(first_cell,
                                                         real_qpts,
                                                         ref_pts_space);
        immersed_mapping.transform_points_real_to_unit_cell(second_cell,
                                                            real_qpts,
                                                            ref_pts_immersed);
        const auto &JxW = quad_formula.get_weights();
        for (unsigned int q = 0; q < n_quad_pts; ++q)
          {
            for (unsigned int i = 0; i < dofs_per_space_cell; ++i)
              {
                for (unsigned int j = 0; j < dofs_per_immersed_cell; ++j)
                  {
                    local_cell_matrix(i, j) +=
                      space_fe.shape_value(i, ref_pts_space[q]) *
                      immersed_fe.shape_value(j, ref_pts_immersed[q]) * JxW[q];
                  }
              }
          }
        typename DoFHandler<dim0, spacedim>::cell_iterator space_cell_dh(
          *first_cell, &space_dh);
        typename DoFHandler<dim1, spacedim>::cell_iterator immersed_cell_dh(
          *second_cell, &immersed_dh);
        space_cell_dh->get_dof_indices(local_space_dof_indices);
        immersed_cell_dh->get_dof_indices(local_immersed_dof_indices);



        space_constraints.distribute_local_to_global(local_cell_matrix,
                                                     local_space_dof_indices,
                                                     immersed_constraints,
                                                     local_immersed_dof_indices,
                                                     matrix);
      }
  }

} // namespace dealii::NonMatching



#else



template <int dim0, int dim1, int spacedim, typename Matrix>
void
dealii::NonMatching::assemble_coupling_exact(
  const DoFHandler<dim0, spacedim> &,
  const DoFHandler<dim1, spacedim> &,
  const std::vector<std::tuple<
    typename dealii::Triangulation<dim0, spacedim>::active_cell_iterator,
    typename dealii::Triangulation<dim1, spacedim>::active_cell_iterator,
    dealii::Quadrature<spacedim>>> &,
  Matrix &,
  const AffineConstraints<typename Matrix::value_type> &,
  const ComponentMask &,
  const ComponentMask &,
  const Mapping<dim0, spacedim> &,
  const Mapping<dim1, spacedim> &,
  const AffineConstraints<typename Matrix::value_type> &)
{
  Assert(false,
         ExcMessage("This function needs CGAL to be installed, "
                    "but cmake could not find it."));
  return {};
}


#endif



template void
dealii::NonMatching::assemble_coupling_exact(
  const DoFHandler<2, 2> &,
  const DoFHandler<1, 2> &,
  const std::vector<
    std::tuple<typename dealii::Triangulation<2, 2>::active_cell_iterator,
               typename dealii::Triangulation<1, 2>::active_cell_iterator,
               dealii::Quadrature<2>>> &,
  dealii::SparseMatrix<double> &,
  const AffineConstraints<dealii::SparseMatrix<double>::value_type> &,
  const ComponentMask &,
  const ComponentMask &,
  const Mapping<2, 2> &space_mapping,
  const Mapping<1, 2> &immersed_mapping,
  const AffineConstraints<dealii::SparseMatrix<double>::value_type> &);

template void
dealii::NonMatching::assemble_coupling_exact(
  const DoFHandler<2, 2> &,
  const DoFHandler<2, 2> &,
  const std::vector<
    std::tuple<typename dealii::Triangulation<2, 2>::active_cell_iterator,
               typename dealii::Triangulation<2, 2>::active_cell_iterator,
               dealii::Quadrature<2>>> &,
  dealii::SparseMatrix<double> &,
  const AffineConstraints<dealii::SparseMatrix<double>::value_type> &,
  const ComponentMask &,
  const ComponentMask &,
  const Mapping<2, 2> &space_mapping,
  const Mapping<2, 2> &immersed_mapping,
  const AffineConstraints<dealii::SparseMatrix<double>::value_type> &);



template void
dealii::NonMatching::assemble_coupling_exact(
  const DoFHandler<3, 3> &,
  const DoFHandler<2, 3> &,
  const std::vector<
    std::tuple<typename dealii::Triangulation<3, 3>::active_cell_iterator,
               typename dealii::Triangulation<2, 3>::active_cell_iterator,
               dealii::Quadrature<3>>> &,
  dealii::SparseMatrix<double> &,
  const AffineConstraints<dealii::SparseMatrix<double>::value_type> &,
  const ComponentMask &,
  const ComponentMask &,
  const Mapping<3, 3> &space_mapping,
  const Mapping<2, 3> &immersed_mapping,
  const AffineConstraints<dealii::SparseMatrix<double>::value_type> &);


template void
dealii::NonMatching::assemble_coupling_exact(
  const DoFHandler<3, 3> &,
  const DoFHandler<3, 3> &,
  const std::vector<
    std::tuple<typename dealii::Triangulation<3, 3>::active_cell_iterator,
               typename dealii::Triangulation<3, 3>::active_cell_iterator,
               dealii::Quadrature<3>>> &,
  dealii::SparseMatrix<double> &,
  const AffineConstraints<typename dealii::SparseMatrix<double>::value_type> &,
  const ComponentMask &,
  const ComponentMask &,
  const Mapping<3, 3> &space_mapping,
  const Mapping<3, 3> &immersed_mapping,
  const AffineConstraints<typename dealii::SparseMatrix<double>::value_type> &);
