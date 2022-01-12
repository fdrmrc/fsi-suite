

#include <tools/parsed_preconditioner/ilu.h>

#ifdef DEAL_II_WITH_TRILINOS

using namespace dealii;

namespace Tools
{
  ParsedILUPreconditioner::ParsedILUPreconditioner(const std::string & name,
                                                   const unsigned int &ilu_fill,
                                                   const double &      ilu_atol,
                                                   const double &      ilu_rtol,
                                                   const unsigned int &overlap)
    : ParameterAcceptor(name)
    , PreconditionILU()
    , ilu_fill(ilu_fill)
    , ilu_atol(ilu_atol)
    , ilu_rtol(ilu_rtol)
    , overlap(overlap)
  {
    add_parameters();
  }

  void
  ParsedILUPreconditioner::add_parameters()
  {
    add_parameter("Fill-in", ilu_fill, "Additional fill-in.");

    add_parameter("ILU atol",
                  ilu_atol,
                  "The amount of perturbation to add to diagonal entries.");

    add_parameter("ILU rtol", ilu_rtol, "Scaling factor for diagonal entries.");

    add_parameter("Overlap", overlap, "Overlap between processors.");
  }

  template <typename Matrix>
  void
  ParsedILUPreconditioner::initialize_preconditioner(const Matrix &matrix)
  {
    TrilinosWrappers::PreconditionILU::AdditionalData data;

    data.ilu_fill = ilu_fill;
    data.ilu_atol = ilu_atol;
    data.ilu_rtol = ilu_rtol;
    data.overlap  = overlap;
    this->initialize(matrix, data);
  }
} // namespace Tools

template void
Tools::ParsedILUPreconditioner::initialize_preconditioner<
  dealii::TrilinosWrappers::SparseMatrix>(
  const dealii::TrilinosWrappers::SparseMatrix &);

#endif