"""Physical counterexamples for the offline portal lighting reference (not PIE)."""
import json
import math
from pathlib import Path
import sys
import unittest

from portal_light_transport_reference import Box, Endpoint, Frame, Scene, SurfaceQuad, add, mul, demonstration


class LightPaths(unittest.TestCase):
    def setUp(self):
        self.a = Endpoint('blue', 'orange', 'wall_blue', Frame())
        self.b = Endpoint('orange', 'blue', 'wall_orange', Frame((1000., 0., 0.)))
        self.walls = (Box('wall_blue', self.a.frame, (6., 200., 200.)),
                      Box('wall_orange', self.b.frame, (6., 200., 200.)))

    def scene(self, *extra, endpoints=None):
        return Scene((self.a, self.b) if endpoints is None else endpoints, self.walls + extra)

    def test_thick_support_and_subcentimetre_exit_occluder(self):
        report = demonstration()
        self.assertEqual(report['stock_world']['hit'], 'wall_blue')
        path = report['connected_world']
        self.assertEqual(path['hit'], 'exit_blocker')
        self.assertAlmostEqual(path['travelled'], 56.1)
        self.assertEqual(len(path['segments']), 2)
        # Wall thickness is not a step past the nearby distinct occluder.
        self.assertAlmostEqual(path['segments'][1]['distance'], 6.1)

    def test_occluder_inside_exit_wall_volume_still_blocks(self):
        blocker = Box('thin_other_body', Frame((1000.15, 0., 0.)), (.05, 10., 10.))
        path = self.scene(blocker).trace((50., 0., 0.), (-1., 0., 0.), 100.)
        self.assertEqual(path.hit, 'thin_other_body')
        self.assertAlmostEqual(path.travelled, 50.1)

    def test_foreground_wins_before_portal(self):
        blocker = Box('foreground', Frame((25., 0., 0.)), (1., 10., 10.))
        result = self.scene(blocker).trace((50., 0., 0.), (-1., 0., 0.), 100.)
        self.assertEqual(result.hit, 'foreground')
        self.assertAlmostEqual(result.travelled, 24.)
        self.assertEqual(len(result.segments), 1)

    def test_back_wall_and_ellipse_corner_are_opaque(self):
        scene = self.scene()
        back = scene.trace((-50., 0., 0.), (1., 0., 0.), 100.)
        self.assertEqual(back.hit, 'wall_blue')
        # Inside enclosing rectangle, outside the actual ellipse.
        corner = scene.trace((50., 60., 110.), (-1., 0., 0.), 100.)
        self.assertEqual(corner.hit, 'wall_blue')

    def test_oblique_ray_hits_rim_in_entry_thickness(self):
        # At logical plane y=60 is legal. At first wall contact y=66 is outside.
        result = self.scene().trace((50., 110., 0.), (-1., -1., 0.), 150.)
        self.assertEqual(result.hit, 'wall_blue')
        self.assertAlmostEqual(result.travelled, 44.*math.sqrt(2))

    def test_oblique_ray_hits_rim_in_exit_thickness(self):
        # Entry wall spans negative X: approach stays within the aperture.
        walls = (Box('wall_blue', Frame((-6., 0., 0.)), (6., 200., 200.)), self.walls[1])
        scene = Scene((self.a, self.b), walls)
        result = scene.trace((50., 10., 0.), (-1., 1., 0.), 150.)
        self.assertEqual(result.hit, 'wall_orange')
        self.assertAlmostEqual(result.travelled, 55.*math.sqrt(2))

    def test_path_length_not_world_gap_and_no_radiance_gain(self):
        target = Box('target', Frame((1020., 0., 0.)), (1., 10., 10.))
        result = self.scene(target).trace((50., 0., 0.), (-1., 0., 0.), 100.)
        self.assertEqual(result.hit, 'target')
        self.assertAlmostEqual(result.travelled, 69.)
        self.assertEqual(result.radiance_weight, 1.)
        self.assertAlmostEqual(sum(s.distance for s in result.segments), result.travelled)

    def test_reciprocal_path_and_non_axis_aligned_exit(self):
        rotated = Endpoint('orange', 'blue', 'wall_orange',
                           Frame((1000., 0., 0.), ((0., 1., 0.), (-1., 0., 0.), (0., 0., 1.))))
        walls = (self.walls[0], Box('wall_orange', rotated.frame, (6., 200., 200.)))
        scene = Scene((self.a, rotated), walls)
        forward = scene.trace((50., 10., 5.), (-1., 0., 0.), 100.)
        endpoint = add(forward.segments[-1].origin, mul(forward.segments[-1].direction, 50.))
        reverse = scene.trace(endpoint, mul(forward.segments[-1].direction, -1), 100.)
        self.assertEqual(forward.status, 'range_complete')
        self.assertEqual(reverse.status, 'range_complete')
        end = add(reverse.segments[-1].origin, mul(reverse.segments[-1].direction, reverse.segments[-1].distance))
        for value, expected in zip(end, (50., 10., 5.)):
            self.assertAlmostEqual(value, expected)

    def test_material_slice_discards_only_illegal_half(self):
        cut = Frame((1010., 0., 0.))
        near = SurfaceQuad('traveller_near_face', Frame((1005., 0., 0.)), 10., 10., cut)
        far = SurfaceQuad('traveller_far_face', Frame((1015., 0., 0.)), 10., 10., cut)
        result = self.scene(near, far).trace((50., 0., 0.), (-1., 0., 0.), 100.)
        self.assertEqual(result.hit, 'traveller_far_face')
        self.assertAlmostEqual(result.travelled, 65.)  # No synthetic cap at x=1010.

    def test_large_world_translation_preserves_path(self):
        offset = (1.e9, -1.e9, 1.e9)
        a = Endpoint('blue', 'orange', 'wall_blue', Frame(offset))
        b = Endpoint('orange', 'blue', 'wall_orange', Frame(add(offset, (1000., 0., 0.))))
        solids = (Box('wall_blue', a.frame, (6., 200., 200.)),
                  Box('wall_orange', b.frame, (6., 200., 200.)),
                  Box('target', Frame(add(offset, (1020., 0., 0.))), (1., 10., 10.)))
        result = Scene((a, b), solids).trace(add(offset, (50., 0., 0.)), (-1., 0., 0.), 100.)
        self.assertEqual(result.hit, 'target')
        self.assertAlmostEqual(result.travelled, 69.)

    def test_finite_range_and_zero_length_directed_seam(self):
        before = self.scene().trace((50., 0., 0.), (-1., 0., 0.), 20.)
        self.assertEqual(before.status, 'range_complete')
        self.assertEqual(len(before.segments), 1)
        exact = self.scene().trace((0., 0., 0.), (-1., 0., 0.), 20.)
        self.assertEqual(exact.status, 'range_complete')
        self.assertEqual(exact.segments[0].distance, 0.)
        self.assertEqual(exact.segments[1].origin, (1000., 0., 0.))

    def test_bounded_repeated_connection_is_unresolved(self):
        # Exiting orange +X meets blue at x=20 facing -X, creating a repeated path.
        a = Endpoint('blue', 'orange', 'wall_blue', Frame((20., 0., 0.), ((-1., 0., 0.), (0., -1., 0.), (0., 0., 1.))))
        b = Endpoint('orange', 'blue', 'wall_orange', Frame())
        walls = (Box('wall_blue', a.frame, (1., 200., 200.)), Box('wall_orange', b.frame, (1., 200., 200.)))
        for budget in range(5):
            result = Scene((a, b), walls).trace((10., 0., 0.), (1., 0., 0.), 200., budget)
            self.assertEqual(result.status, 'unresolved_hop_budget')
            self.assertEqual(len(result.segments), budget + 1)
            self.assertAlmostEqual(result.travelled, 10. + 20.*budget)
            self.assertEqual(result.radiance_weight, 1.)

    def test_order_independence_and_disconnect_restore_wall(self):
        one = Scene((self.a, self.b), self.walls).trace((50., 0., 0.), (-1., 0., 0.), 100.)
        two = Scene((self.b, self.a), self.walls[::-1]).trace((50., 0., 0.), (-1., 0., 0.), 100.)
        self.assertEqual(one, two)
        disconnected = Scene(solids=self.walls).trace((50., 0., 0.), (-1., 0., 0.), 100.)
        self.assertEqual(disconnected.hit, 'wall_blue')

    def test_invalid_inputs_rejected(self):
        with self.assertRaises(ValueError):
            Frame(axes=((2., 0., 0.), (0., 1., 0.), (0., 0., 1.)))
        with self.assertRaises(ValueError):
            self.scene().trace((50., 0., 0.), (float('nan'), 0., 0.), 100.)
        with self.assertRaises(ValueError):
            self.scene(endpoints=(self.a,))
        with self.assertRaises(ValueError):
            self.scene().trace((50., 0., 0.), (-1., 0., 0.), 100., 5)
        with self.assertRaises(ValueError):
            self.scene(endpoints=(self.a, Endpoint('orange', 'blue', 'wall_orange', self.b.frame, 40., 115.)))


if __name__ == '__main__':
    report = None
    if '--report' in sys.argv:
        index = sys.argv.index('--report')
        report = Path(sys.argv[index + 1])
        del sys.argv[index:index + 2]
    outcome = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(LightPaths))
    if report:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps({'kind': 'offline_reference_not_GPU_or_PIE', 'tests': outcome.testsRun,
                                     'failures': len(outcome.failures), 'errors': len(outcome.errors),
                                     'successful': outcome.wasSuccessful()}, indent=2), encoding='utf-8')
    sys.exit(0 if outcome.wasSuccessful() else 1)
