#include "serial_poisson.h"

#include <deal.II/lac/linear_operator_tools.h>

#include <deal.II/numerics/error_estimator.h>


using namespace dealii;
namespace PDEs
{
  template <int dim, int spacedim>
  SerialPoisson<dim, spacedim>::SerialPoisson()
    : ParameterAcceptor("/Serial Poisson/")
    , grid_generator("/Serial Poisson/Grid")
    , grid_refinement("/Serial Poisson/Grid/Refinement")
    , finite_element("/Serial Poisson/", "u", "FE_Q(1)")
    , dof_handler(triangulation)
    , inverse_operator("/Serial Poisson/Solver")
    , constants("/Serial Poisson/Constants",
                {"kappa"},
                {1.0},
                {"Diffusion coefficient"})
    , forcing_term("/Serial Poisson/Functions",
                   "kappa*sin(2*pi*x)*sin(2*pi*y)",
                   "Forcing term")
    , exact_solution("/Serial Poisson/Functions",
                     "sin(2*pi*x)*sin(2*pi*y)",
                     "Exact solution")
    , boundary_conditions("/Serial Poisson/Boundary conditions")
  {
    add_parameter("Output filename", output_filename);
    add_parameter("Number of refinement cycles", n_refinement_cycles);

    enter_subsection("Error table");
    enter_my_subsection(this->prm);
    error_table.add_parameters(this->prm);
    leave_my_subsection(this->prm);
    leave_subsection();
  }



  template <int dim, int spacedim>
  void
  SerialPoisson<dim, spacedim>::setup_system()
  {
    boundary_conditions.update_substitution_map(constants);
    exact_solution().update_user_substitution_map(constants);
    forcing_term().update_user_substitution_map(constants);

    dof_handler.distribute_dofs(finite_element);

    constraints.clear();
    DoFTools::make_hanging_node_constraints(dof_handler, constraints);
    boundary_conditions.check_consistency(triangulation);
    boundary_conditions.apply_boundary_conditions(dof_handler, constraints);
    constraints.close();


    DynamicSparsityPattern dsp(dof_handler.n_dofs());
    DoFTools::make_sparsity_pattern(dof_handler, dsp, constraints, false);
    sparsity_pattern.copy_from(dsp);
    system_matrix.reinit(sparsity_pattern);
    solution.reinit(dof_handler.n_dofs());
    system_rhs.reinit(dof_handler.n_dofs());
  }



  template <int dim, int spacedim>
  void
  SerialPoisson<dim, spacedim>::assemble_system()
  {
    QGauss<dim>     quadrature_formula(finite_element().degree + 1);
    QGauss<dim - 1> face_quadrature_formula(finite_element().degree + 1);

    FEValues<dim, spacedim> fe_values(finite_element,
                                      quadrature_formula,
                                      update_values | update_gradients |
                                        update_quadrature_points |
                                        update_JxW_values);

    FEFaceValues<dim, spacedim> fe_face_values(finite_element,
                                               face_quadrature_formula,
                                               update_values |
                                                 update_quadrature_points |
                                                 update_JxW_values);

    const unsigned int dofs_per_cell = finite_element().n_dofs_per_cell();
    FullMatrix<double> cell_matrix(dofs_per_cell, dofs_per_cell);
    Vector<double>     cell_rhs(dofs_per_cell);
    std::vector<types::global_dof_index> local_dof_indices(dofs_per_cell);
    for (const auto &cell : dof_handler.active_cell_iterators())
      {
        fe_values.reinit(cell);
        cell_matrix = 0;
        cell_rhs    = 0;
        for (const unsigned int q_index : fe_values.quadrature_point_indices())
          {
            for (const unsigned int i : fe_values.dof_indices())
              for (const unsigned int j : fe_values.dof_indices())
                cell_matrix(i, j) +=
                  (fe_values.shape_grad(i, q_index) * // grad phi_i(x_q)
                   fe_values.shape_grad(j, q_index) * // grad phi_j(x_q)
                   fe_values.JxW(q_index));           // dx
            for (const unsigned int i : fe_values.dof_indices())
              cell_rhs(i) += (fe_values.shape_value(i, q_index) * // phi_i(x_q)
                              forcing_term().value(
                                fe_values.quadrature_point(q_index)) * // f(x_q)
                              fe_values.JxW(q_index));                 // dx
          }

        // if (cell->at_boundary())
        //   //  for(const auto face: cell->face_indices())
        //   for (const unsigned int f : cell->face_indices())
        //     if (neumann_ids.find(cell->face(f)->boundary_id()) !=
        //         neumann_ids.end())
        //       {
        //         fe_face_values.reinit(cell, f);
        //         for (const unsigned int q_index :
        //              fe_face_values.quadrature_point_indices())
        //           for (const unsigned int i : fe_face_values.dof_indices())
        //             cell_rhs(i) += fe_face_values.shape_value(i, q_index) *
        //                            neumann_boundary_condition.value(
        //                              fe_face_values.quadrature_point(q_index))
        //                              *
        //                            fe_face_values.JxW(q_index);
        //       }

        cell->get_dof_indices(local_dof_indices);
        constraints.distribute_local_to_global(
          cell_matrix, cell_rhs, local_dof_indices, system_matrix, system_rhs);
      }
  }



  template <int dim, int spacedim>
  void
  SerialPoisson<dim, spacedim>::solve()
  {
    const auto A    = linear_operator<Vector<double>>(system_matrix);
    const auto Ainv = inverse_operator(A, PreconditionIdentity());
    solution        = Ainv * system_rhs;
    constraints.distribute(solution);
  }



  template <int dim, int spacedim>
  void
  SerialPoisson<dim, spacedim>::output_results(const unsigned cycle) const
  {
    DataOut<dim, spacedim> data_out;
    data_out.attach_dof_handler(dof_handler);
    data_out.add_data_vector(solution, "solution");
    data_out.build_patches();
    std::string fname = output_filename + "_" + std::to_string(cycle) + ".vtu";
    std::ofstream output(fname);
    data_out.write_vtu(output);
  }



  template <int dim, int spacedim>
  void
  SerialPoisson<dim, spacedim>::run()
  {
    grid_generator.generate(triangulation);
    for (unsigned int cycle = 0; cycle < n_refinement_cycles; ++cycle)
      {
        setup_system();
        assemble_system();
        solve();
        error_table.error_from_exact(dof_handler, solution, exact_solution());
        output_results(cycle);
        if (cycle < n_refinement_cycles - 1)
          {
            Vector<float> estimated_error_per_cell(
              triangulation.n_active_cells());
            KellyErrorEstimator<dim, spacedim>::estimate(
              dof_handler,
              QGauss<dim - 1>(finite_element().degree + 1),
              {},
              solution,
              estimated_error_per_cell);
            grid_refinement.mark_cells(estimated_error_per_cell, triangulation);
            triangulation.execute_coarsening_and_refinement();
          }
      }
    error_table.output_table(std::cout);
  }

  template class SerialPoisson<2, 2>;
  template class SerialPoisson<3, 3>;
} // namespace PDEs