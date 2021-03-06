#!/usr/bin/env python3
#
# formatted with black

from pybindlibs import cpp

import unittest, os, pyphare.pharein as ph
from datetime import datetime, timezone
from ddt import ddt, data
from tests.diagnostic import dump_all_diags
from tests.simulator import NoOverwriteDict, populate_simulation
from pyphare.simulator.simulator import Simulator,startMPI
from pyphare.core.box import Box

out = "phare_outputs/valid/refinement_boxes/"
diags = {"diag_options": {"format": "phareh5", "options": {"dir": out, "mode":"overwrite" }}}


@ddt
class SimulatorRefineBoxInputs(unittest.TestCase):


    def __init__(self, *args, **kwargs):
        super(SimulatorRefineBoxInputs, self).__init__(*args, **kwargs)
        self.simulator = None


    def dup(dic):
        dic = NoOverwriteDict(dic)
        dic.update(diags.copy())
        dic.update({"diags_fn": lambda model: dump_all_diags(model.populations)})
        return dic

    """
      The first set of boxes "B0": [(10,), (14,)]
      Are configured to force there to be a single patch on L0
      This creates a case with MPI that there are an unequal number of
      Patches across MPI domains. This case must be handled and not hang due
      to collective calls not being handled properly.
    """
    valid1D = [
        dup({"cells":[65], "refinement_boxes": {"L0": {"B0": [(10,), (14,)]}}}),
        dup({"cells":[65], "refinement_boxes": {"L0": {"B0": [(5,), (55,)]}}}),
        dup({"cells":[65], "refinement_boxes": {"L0": {"B0": Box(5, 55)}}}),
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 55)]}}),
        dup({"cells":[65], "refinement_boxes": {  0 : [Box(5, 55)]}}),
        dup({"cells":[65], "refinement_boxes": {  0 : [Box(0, 55)]}}),
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 14), Box(15, 25)]}}),
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 25)], "L1": [Box(12, 48)], "L2": [Box(60, 64)]}}),
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 25)], "L1": [Box(12, 48)]}}),
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 25)], "L1": [Box(20, 30)]}}),
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 25)], "L1": [Box(11, 49)]}, "nesting_buffer": 1}),
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 25)], "L1": [Box(10, 50)]}}),
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 25)], "L1": [Box(15, 49)]}}),

        dup({"cells":[65], "refinement_boxes": None, "smallest_patch_size": 20, "largest_patch_size": 20, "nesting_buffer": 10}),

    ]

    invalid1D = [
        # finer box outside lower
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 24)], "L1": [Box(9, 30)]}}),
        # finer box outside upper
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 24)], "L1": [Box(15, 50)]}}),
        # overlapping boxes
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 15), Box(15, 25)]}}),
        # box.upper outside domain
        dup({"cells": [55], "refinement_boxes": {"L0": {"B0": [(5,), (65,)]}}}),
        # largest_patch_size > smallest_patch_size
        dup({"smallest_patch_size": 100, "largest_patch_size": 64,}),
        # refined_particle_nbr doesn't exist
        dup({"refined_particle_nbr": 1}),
        # L2 box incompatible with L1 box due to nesting buffer
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 25)], "L1": [Box(11, 49)]}, "nesting_buffer": 2}),
        # negative nesting buffer
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 25)], "L1": [Box(11, 49)]}, "nesting_buffer": -1}),
        # too large nesting buffer
        dup({"cells":[65], "refinement_boxes": {"L0": [Box(5, 25)], "L1": [Box(11, 49)]}, "nesting_buffer": 33}),
        dup({"cells":[65], "refinement_boxes": None, "largest_patch_size": 20, "nesting_buffer": 46}),
    ]

    def tearDown(self):
        if self.simulator is not None:
            self.simulator.reset()



    def _do_dim(self, dim, input, valid: bool = False):
        for interp in range(1, 4):

            try:
                self.simulator = Simulator(populate_simulation(dim, interp, **input))
                self.simulator.initialize()

                self.assertTrue(valid)

                self.simulator.dump(self.simulator.currentTime(), self.simulator.timeStep())

                self.simulator = None

            except ValueError as e:
                self.assertTrue(not valid)

    @data(*valid1D)
    def test_1d_valid(self, input):
        self._do_dim(1, input, True)

    @data(*invalid1D)
    def test_1d_invalid(self, input):
        self._do_dim(1, input)


if __name__ == "__main__":
    unittest.main()
