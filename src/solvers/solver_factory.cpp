#include "solvers/solver_factory.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "boost/algorithm/string.hpp"
#include "errors/solve_error.hpp"
#include "problem/problem.hpp"
#include "solvers/csp/csp_solver.hpp"
#include "solvers/dlx/dlx_solver.hpp"
#include "solvers/ilp/cbc_wrapper.hpp"
#include "solvers/ilp/ilp_solver.hpp"
#include "solvers/ilp/ilp_utils.hpp"
#include "solvers/ilp/ilp_wrapper.hpp"
#include "solvers/sat/sat_amk_solver.hpp"
#include "solvers/sat/sat_amo_ordered_solver.hpp"
#include "solvers/sat/sat_amo_solver.hpp"
#include "solvers/sat/symmetry_breaker.hpp"
#include "solvers/simple/simple_solver.hpp"
#include "solvers/solver.hpp"

#ifdef CADICAL
#include "solvers/sat/cadical_wrapper.hpp"
#endif

#ifdef CRYPTOMINISAT
#include "solvers/sat/cryptominisat_wrapper.hpp"
#endif

#if defined(CADICAL) || defined(CRYPTOMINISAT)
#include "pb2cnf.h"
#include "solvers/sat/pblib_wrapper.hpp"
#endif

#ifdef BREAKID
#include "solvers/sat/breakid_wrapper.hpp"
#endif

#ifdef GUROBI
#include "solvers/ilp/gurobi_wrapper.hpp"
#endif

#ifdef DLX
#include "solvers/dlx/dlx_wrapper.hpp"
#endif

#ifdef MINIZINC
#include "solvers/csp/minizinc_wrapper.hpp"
#endif

std::vector<std::string> solver_factory::get_solver_names() {
    using namespace solver_factory;

    std::vector<std::string> solver_names;

    // simple
    solver_names.push_back(kSimpleSolver);

    // sat
    for (std::string sat_wrapper_name : kSatWrapperNames) {
        for (std::string symmetry_breaker_name : kSatSymmetryBreakerNames) {
            for (std::string pblib_wrapper_name : kSatPBLibWrapperNames) {
                std::vector<std::string> words{kSatPrefix, sat_wrapper_name, symmetry_breaker_name,
                                               pblib_wrapper_name};
                solver_names.push_back(boost::algorithm::join(words, "_"));
            }
        }
    }

    // ilp
    for (std::string ilp_wrapper_name : kIlpWrapperNames) {
        for (std::string params_name : kIlpParamsNames) {
            for (std::string objective_name : kIlpObjectiveNames) {
                std::vector<std::string> words{kIlpPrefix, ilp_wrapper_name, params_name,
                                               objective_name};
                solver_names.push_back(boost::algorithm::join(words, "_"));
            }
        }
    }

    // dlx
    solver_names.push_back(kDlxSolver);

    // csp
    solver_names.push_back(kCspSolver);

    return solver_names;
}

std::unique_ptr<Solver> solver_factory::create(const std::string& solver_name,
                                               const Problem& problem) {
    using namespace solver_factory;

    // actually, the error should never be thrown if the solver_name is from get_solver_names()
    auto SolverNotFound = SolveError("Solver " + solver_name + " is not available.");

    auto solver_names = get_solver_names();
    if (std::find(solver_names.begin(), solver_names.end(), solver_name) == solver_names.end()) {
        throw SolverNotFound;
    }

    std::vector<std::string> words;
    boost::split(words, solver_name, boost::is_any_of("_"));

    // simple
    if (words.size() == 1 && words[0] == kSimpleSolver) {
        return std::make_unique<SimpleSolver>(problem);
    }

    // sat
    if (words.size() == 4 && words[0] == kSatPrefix) {
        using namespace AMO_ENCODER;
        using namespace AMK_ENCODER;

        std::string sat_wrapper_name = words[1];
        std::unique_ptr<SatWrapper> sat_wrapper;
#ifdef CADICAL
        if (sat_wrapper_name == kSatCadical) {
            sat_wrapper = std::make_unique<CadicalWrapper>();
        }
#endif
#ifdef CRYPTOMINISAT
        if (sat_wrapper_name == kSatCryptominisat) {
            sat_wrapper = std::make_unique<CryptominisatWrapper>();
        }
#endif
        if (sat_wrapper.get() == nullptr) {
            throw SolverNotFound;
        }

        std::string symmetry_breaker_name = words[2];
        std::unique_ptr<SymmetryBreaker> symmetry_breaker;
        if (symmetry_breaker_name == kSatNoSymmetryBreaker) {
            symmetry_breaker = std::make_unique<SymmetryBreaker>();
        }
#ifdef BREAKID
        if (symmetry_breaker_name == kSatBreakid) {
            symmetry_breaker = std::make_unique<BreakIDWrapper>();
        }
#endif
        if (symmetry_breaker.get() == nullptr) {
            throw SolverNotFound;
        }

        std::string pblib_wrapper_name = words[3];
        if (pblib_wrapper_name == kSatAmoAuto) {
            return std::make_unique<SatAmoSolver>(
                    problem, std::move(sat_wrapper), std::move(symmetry_breaker),
                    PBLibWrapper(PB2CNF_AMO_Encoder::BEST, PB2CNF_AMK_Encoder::BEST));
        }
        if (pblib_wrapper_name == kSatAmoNested) {
            return std::make_unique<SatAmoSolver>(
                    problem, std::move(sat_wrapper), std::move(symmetry_breaker),
                    PBLibWrapper(PB2CNF_AMO_Encoder::NESTED, PB2CNF_AMK_Encoder::BEST));
        }
        if (pblib_wrapper_name == kSatAmoBDD) {
            return std::make_unique<SatAmoSolver>(
                    problem, std::move(sat_wrapper), std::move(symmetry_breaker),
                    PBLibWrapper(PB2CNF_AMO_Encoder::BDD, PB2CNF_AMK_Encoder::BEST));
        }
        if (pblib_wrapper_name == kSatAmoBimander) {
            return std::make_unique<SatAmoSolver>(
                    problem, std::move(sat_wrapper), std::move(symmetry_breaker),
                    PBLibWrapper(PB2CNF_AMO_Encoder::BIMANDER, PB2CNF_AMK_Encoder::BEST));
        }
        if (pblib_wrapper_name == kSatAmoCommander) {
            return std::make_unique<SatAmoSolver>(
                    problem, std::move(sat_wrapper), std::move(symmetry_breaker),
                    PBLibWrapper(PB2CNF_AMO_Encoder::COMMANDER, PB2CNF_AMK_Encoder::BEST));
        }
        if (pblib_wrapper_name == kSatAmoKProduct) {
            return std::make_unique<SatAmoSolver>(
                    problem, std::move(sat_wrapper), std::move(symmetry_breaker),
                    PBLibWrapper(PB2CNF_AMO_Encoder::KPRODUCT, PB2CNF_AMK_Encoder::BEST));
        }
        if (pblib_wrapper_name == kSatAmoBinary) {
            return std::make_unique<SatAmoSolver>(
                    problem, std::move(sat_wrapper), std::move(symmetry_breaker),
                    PBLibWrapper(PB2CNF_AMO_Encoder::BINARY, PB2CNF_AMK_Encoder::BEST));
        }
        if (pblib_wrapper_name == kSatAmoPairwise) {
            return std::make_unique<SatAmoSolver>(
                    problem, std::move(sat_wrapper), std::move(symmetry_breaker),
                    PBLibWrapper(PB2CNF_AMO_Encoder::PAIRWISE, PB2CNF_AMK_Encoder::BEST));
        }
        if (pblib_wrapper_name == kSatAmoOrdered) {
            return std::make_unique<SatAmoOrderedSolver>(
                    problem, std::move(sat_wrapper), std::move(symmetry_breaker),
                    PBLibWrapper(PB2CNF_AMO_Encoder::BDD, PB2CNF_AMK_Encoder::BEST));
        }
        if (pblib_wrapper_name == kSatAmkAuto) {
            return std::make_unique<SatAmkSolver>(
                    problem, std::move(sat_wrapper), std::move(symmetry_breaker),
                    PBLibWrapper(PB2CNF_AMO_Encoder::BEST, PB2CNF_AMK_Encoder::BEST));
        }
        if (pblib_wrapper_name == kSatAmkBDD) {
            return std::make_unique<SatAmkSolver>(
                    problem, std::move(sat_wrapper), std::move(symmetry_breaker),
                    PBLibWrapper(PB2CNF_AMO_Encoder::BEST, PB2CNF_AMK_Encoder::BDD));
        }
        if (pblib_wrapper_name == kSatAmkCard) {
            return std::make_unique<SatAmkSolver>(
                    problem, std::move(sat_wrapper), std::move(symmetry_breaker),
                    PBLibWrapper(PB2CNF_AMO_Encoder::BEST, PB2CNF_AMK_Encoder::CARD));
        }
        throw SolverNotFound;
    }

    // ilp
    if (words.size() == 4 && words[0] == kIlpPrefix) {
        using namespace ilp_utils;

        std::string params_name = words[2];
        bool adjusted_params;
        if (params_name == kIlpDefaultParams) {
            adjusted_params = false;
        } else if (params_name == kIlpAdjustedParams) {
            adjusted_params = true;
        } else {
            throw SolverNotFound;
        }

        std::string ilp_wrapper_name = words[1];
        std::unique_ptr<IlpWrapper> ilp_wrapper;
        if (ilp_wrapper_name == kIlpCbc) {
            ilp_wrapper = std::make_unique<CbcWrapper>(adjusted_params);
        }
#ifdef GUROBI
        if (ilp_wrapper_name == kIlpGurobi) {
            ilp_wrapper = std::make_unique<GurobiWrapper>(adjusted_params);
        }
#endif
        if (ilp_wrapper.get() == nullptr) {
            throw SolverNotFound;
        }

        std::string objective_name = words[3];
        if (objective_name == kIlpEqIgnore) {
            return std::make_unique<IlpSolver>(problem, std::move(ilp_wrapper),
                                               ConstraintSense::kEq, ObjectiveSense::kIgnore);
        }
        if (objective_name == kIlpEqMinimize) {
            return std::make_unique<IlpSolver>(problem, std::move(ilp_wrapper),
                                               ConstraintSense::kEq, ObjectiveSense::kMinimize);
        }
        if (objective_name == kIlpGeqMinimize) {
            return std::make_unique<IlpSolver>(problem, std::move(ilp_wrapper),
                                               ConstraintSense::kGeq, ObjectiveSense::kMinimize);
        }
        if (objective_name == kIlpLeqMaximize) {
            return std::make_unique<IlpSolver>(problem, std::move(ilp_wrapper),
                                               ConstraintSense::kLeq, ObjectiveSense::kMaximize);
        }
        throw SolverNotFound;
    }

    // dlx
#ifdef DLX
    if (words.size() == 1 && words[0] == kDlxSolver) {
        return std::make_unique<DlxSolver>(problem, DlxWrapper{});
    }
#endif

    // csp
#ifdef MINIZINC
    if (words.size() == 1 && words[0] == kCspSolver) {
        return std::make_unique<CspSolver>(problem, MinizincWrapper{});
    }
#endif

    throw SolverNotFound;
}
