#include "solvers/sat_solver.hpp"

#include <memory>
#include <vector>

#include "print.hpp"
#include "problem/problem.hpp"
#include "problem/region.hpp"
#include "problem/tile.hpp"
#include "solution/placed_region.hpp"
#include "solution/solution.hpp"
#include "solvers/sat_utils/sat_utils.hpp"
#include "solvers/sat_utils/sat_wrapper.hpp"
#include "utils.hpp"

SatSolver::SatSolver(Problem problem, std::unique_ptr<SatWrapper> sat_wrapper,
                     PBLibWrapper pblib_wrapper)
        : problem_(problem),
          sat_wrapper_(std::move(sat_wrapper)),
          pblib_wrapper_(std::move(pblib_wrapper)) {
    int board_size = problem_.board_.get_size();
    for (Tile& tile : problem_.tiles_) {
        tile.limit_count(board_size / tile.get_size());
    }
}

// For every possible position and orientation of every available piece of every tile, there is
// one logical variable representing "this piece is placed on the board on this position with this
// orietation".
// For each piece of each tile, there is a set of clauses that guarantee that at most one of the
// piece's variables is true (ie. each piece is placed on the board at most once).
// For each cell of the board, there is a set of clauses that guarantee that exactly one of the tile
// pieces covers this cell.
// Put together, the CNF formula is satisfiable if and only if the board can be tiled.
// There are some symmetries that should be broken in the future (eg. the order of pieces) to
// improve the SAT solving performance.
Solution SatSolver::solve(bool print_stats) {
    using sat_utils::Lit, sat_utils::Clause;

    int w = problem_.board_.get_width();
    int h = problem_.board_.get_height();
    std::vector<Clause> tile_clauses;
    std::vector<std::vector<Clause>> cell_clauses(h, std::vector<Clause>(w, Clause{}));
    std::vector<PlacedRegion> placed_regions;
    for (const Tile& tile : problem_.tiles_) {
        for (int i = 0; i < tile.get_count(); i++) {
            Clause tile_clause;
            for (auto [bx, by] : problem_.board_.get_cells()) {
                for (const Region& region : tile) {
                    int sx = bx - region.get_top_left_x();
                    int sy = by - region.get_top_left_y();
                    if (!problem_.board_.has_subregion(sx, sy, region)) continue;
                    placed_regions.push_back({sx, sy, region});
                    Lit lit = sat_wrapper_->new_lit();
                    tile_clause.push_back(lit);
                    for (auto [rx, ry] : region.get_cells()) {
                        cell_clauses[sy + ry][sx + rx].push_back(lit);
                    }
                }
            }
            tile_clauses.push_back(tile_clause);
        }
    }

    for (const Clause& tile_clause : tile_clauses) {
        pblib_wrapper_.at_most_one_of(tile_clause, sat_wrapper_);
    }
    for (auto [x, y] : problem_.board_.get_cells()) {
        if (cell_clauses[y][x].size() == 0) {
            // the cell cannot be covered, the problem is unsolvable
            return {};
        }
        pblib_wrapper_.at_most_one_of(cell_clauses[y][x], sat_wrapper_, true);
    }

    if (print_stats) {
        print::stats() << sat_wrapper_->get_var_count() << " variables\n"
                       << sat_wrapper_->get_clause_count() << " clauses\n"
                       << sat_wrapper_->get_lit_count() << " literals\n";
    }

    bool result = sat_wrapper_->solve();
    if (!result) return {};

    Solution solution;
    std::vector<bool> model = sat_wrapper_->get_model();
    for (int i = 0; i < static_cast<int>(placed_regions.size()); i++) {
        if (!model[i]) continue;
        solution.push_back(placed_regions[i]);
    }
    return solution;
}
