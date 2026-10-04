"""Offline lighting-path oracle. No UE imports, scene edits or production renderer hook.

Centimetres, rigid frames, elliptical apertures, finite rays, opaque oriented
solids and sliced surface quads. Deliberately not a GI/BRDF integrator.
"""
from dataclasses import asdict, dataclass
import argparse
import json
import math
from pathlib import Path


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def mul(a, k):
    return tuple(x * k for x in a)


def sub(a, b):
    return add(a, mul(b, -1))


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def finite(a):
    return len(a) == 3 and all(math.isfinite(x) for x in a)


@dataclass(frozen=True)
class Frame:
    origin: tuple = (0., 0., 0.)
    axes: tuple = ((1., 0., 0.), (0., 1., 0.), (0., 0., 1.))

    def __post_init__(self):
        if not finite(self.origin) or len(self.axes) != 3 or not all(map(finite, self.axes)):
            raise ValueError('Non-finite frame')
        for i in range(3):
            for j in range(3):
                if abs(dot(self.axes[i], self.axes[j]) - (i == j)) > 1.e-10:
                    raise ValueError('Only orthonormal rigid frames are supported')
        a, b, c = self.axes
        cross = (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
        if dot(cross, c) < 1. - 1.e-10:
            raise ValueError('Reflected/scaled frames are unsupported')

    def vector_to_local(self, v):
        return tuple(dot(v, axis) for axis in self.axes)

    def vector_to_world(self, v):
        result = (0., 0., 0.)
        for axis, value in zip(self.axes, v):
            result = add(result, mul(axis, value))
        return result

    def local(self, p):
        return self.vector_to_local(sub(p, self.origin))

    def world(self, p):
        return add(self.origin, self.vector_to_world(p))


def mapped_vector(v, entry, exit):
    x, y, z = entry.vector_to_local(v)
    # Exact local Z half-turn, matching InteriorPortalMath::Rotation.
    return exit.vector_to_world((-x, -y, z))


@dataclass(frozen=True)
class Endpoint:
    identity: str
    target: str
    support: str
    frame: Frame
    half_width: float = 65.
    half_height: float = 115.

    def __post_init__(self):
        if not self.identity or not self.target or not self.support:
            raise ValueError('Missing geometry identity')
        if not all(math.isfinite(x) and x > 0 for x in (self.half_width, self.half_height)):
            raise ValueError('Invalid aperture')

    def cylinder_interval(self, origin, direction):
        """Ray interval inside the normal-extruded aperture (convex quadric)."""
        _, y, z = self.frame.local(origin)
        _, dy, dz = self.frame.vector_to_local(direction)
        a = (dy/self.half_width)**2 + (dz/self.half_height)**2
        b = 2*(y*dy/self.half_width**2 + z*dz/self.half_height**2)
        c = (y/self.half_width)**2 + (z/self.half_height)**2 - 1
        if a == 0:
            return (-math.inf, math.inf) if c <= 0 else None
        determinant = b*b - 4*a*c
        if determinant < 0:
            return None
        root = math.sqrt(determinant)
        # Stable roots for oblique rays far from the aperture.
        q = -.5*(b + math.copysign(root, b))
        roots = (-b/(2*a),)*2 if q == 0 else (q/a, c/q)
        return tuple(sorted(roots))

    def crossing(self, origin, direction, limit):
        x = self.frame.local(origin)[0]
        dx = self.frame.vector_to_local(direction)[0]
        if x < 0 or dx >= 0:
            return None  # Back-wall rays cannot acquire a portal connection.
        distance = -x/dx
        interval = self.cylinder_interval(origin, direction)
        if 0 <= distance < limit and interval and interval[0] <= distance <= interval[1]:
            return distance
        return None


@dataclass(frozen=True)
class Box:
    identity: str
    frame: Frame
    half_extent: tuple

    def __post_init__(self):
        if not self.identity or not finite(self.half_extent) or min(self.half_extent) <= 0:
            raise ValueError('Invalid solid')

    def interval(self, origin, direction, limit):
        start, end = 0., limit
        for p, d, h in zip(self.frame.local(origin), self.frame.vector_to_local(direction), self.half_extent):
            if d == 0:
                if abs(p) > h:
                    return None
            else:
                a, b = sorted(((-h-p)/d, (h-p)/d))
                start, end = max(start, a), min(end, b)
        return (start, end) if start <= end and start < limit else None


@dataclass(frozen=True)
class SurfaceQuad:
    identity: str
    frame: Frame
    half_width: float
    half_height: float
    slice_frame: Frame = None

    def __post_init__(self):
        if not self.identity or not all(math.isfinite(x) and x > 0 for x in (self.half_width, self.half_height)):
            raise ValueError('Invalid surface')

    def interval(self, origin, direction, limit):
        x = self.frame.local(origin)[0]
        dx = self.frame.vector_to_local(direction)[0]
        if dx == 0:
            return None
        distance = -x/dx
        if not 0 <= distance < limit:
            return None
        point = add(origin, mul(direction, distance))
        _, y, z = self.frame.local(point)
        if abs(y) > self.half_width or abs(z) > self.half_height:
            return None
        if self.slice_frame and self.slice_frame.local(point)[0] < 0:
            return None
        # A material mask discards an existing face. It creates no slice cap.
        return (distance, distance)


@dataclass(frozen=True)
class Segment:
    origin: tuple
    direction: tuple
    distance: float
    endpoint: str = None


@dataclass(frozen=True)
class Result:
    status: str
    hit: str
    segments: tuple
    travelled: float
    # Ideal rigid transport preserves radiance, even through multiple portals.
    radiance_weight: float = 1.


class Scene:
    def __init__(self, endpoints=(), solids=()):
        self.endpoints = {e.identity: e for e in endpoints}
        self.solids = tuple(solids)
        if len(self.endpoints) != len(endpoints) or len({s.identity for s in solids}) != len(solids):
            raise ValueError('Duplicate identity')
        for e in endpoints:
            target = self.endpoints.get(e.target)
            if not target or target.target != e.identity or target is e:
                raise ValueError('Connection must be reciprocal')
            if (e.half_width, e.half_height) != (target.half_width, target.half_height):
                raise ValueError('Scaled portals are unsupported')
            if e.support not in {s.identity for s in solids}:
                raise ValueError('Unknown support identity')

    def trace(self, origin, direction, max_distance, max_hops=4):
        if not finite(origin) or not finite(direction) or not math.isfinite(max_distance) or max_distance <= 0:
            raise ValueError('Invalid finite ray')
        length = math.sqrt(dot(direction, direction))
        if length == 0 or max_hops not in range(5):
            raise ValueError('Invalid ray direction / light hop budget')
        direction = mul(direction, 1/length)
        remaining, travelled, segments, departed = max_distance, 0., [], None
        for hop in range(max_hops + 1):
            crossings = [(t, e.identity, e) for e in self.endpoints.values()
                         if (t := e.crossing(origin, direction, remaining)) is not None]
            candidate = min(crossings, default=None, key=lambda item: item[:2])
            boundary = candidate[0] if candidate else remaining
            holes = []
            if candidate:
                holes.append(candidate[2])
            if departed:
                holes.append(departed)
            hits = []
            for solid in self.solids:
                interval = solid.interval(origin, direction, remaining)
                if not interval:
                    continue
                start, end = interval
                # Subtract only this support's aperture volume, never other solids.
                cuts = sorted(cut for e in holes if solid.identity == e.support
                              and (cut := e.cylinder_interval(origin, direction)))
                removed = False
                for low, high in cuts:
                    if high < start:
                        continue
                    if low > start:
                        break
                    if high >= end:
                        removed = True
                        break
                    start = high
                if not removed and start <= boundary and start < remaining:
                    hits.append((start, solid.identity))
            if hits:
                distance, identity = min(hits)
                segments.append(Segment(origin, direction, distance))
                return Result('blocked', identity, tuple(segments), travelled + distance)
            if not candidate:
                segments.append(Segment(origin, direction, remaining))
                return Result('range_complete', None, tuple(segments), max_distance)
            distance, identity, entry = candidate
            segments.append(Segment(origin, direction, distance, identity))
            travelled += distance
            remaining -= distance
            if hop == max_hops:
                return Result('unresolved_hop_budget', None, tuple(segments), travelled)
            exit = self.endpoints[entry.target]
            intersection = add(origin, mul(direction, distance))
            origin = add(exit.frame.origin, mapped_vector(sub(intersection, entry.frame.origin), entry.frame, exit.frame))
            direction = mapped_vector(direction, entry.frame, exit.frame)
            departed = exit
        raise AssertionError('Unreachable')


def demonstration():
    entry = Endpoint('blue', 'orange', 'wall_blue', Frame())
    exit = Endpoint('orange', 'blue', 'wall_orange', Frame((1000., 0., 0.)))
    supports = (Box('wall_blue', entry.frame, (6., 200., 200.)),
                Box('wall_orange', exit.frame, (6., 200., 200.)))
    blocker = Box('exit_blocker', Frame((1006.15, 0., 0.)), (.05, 20., 20.))
    origin, direction = (50., 0., 0.), (-1., 0., 0.)
    stock = Scene(solids=supports + (blocker,)).trace(origin, direction, 100.)
    connected = Scene((entry, exit), supports + (blocker,)).trace(origin, direction, 100.)
    return {'kind': 'offline_reference_not_UE_GPU_or_visual_acceptance',
            'stock_world': asdict(stock), 'connected_world': asdict(connected)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    report = demonstration()
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
