// SPDX-License-Identifier: AGPL-3.0-only
// Run against the original STEP fixture; no fixture is redistributed with this test.
const assert = require("node:assert/strict");
const { spawnSync } = require("node:child_process");
const [exe, input, expectedFaces] = process.argv.slice(2);

function findTinyIsolatedOpenings(positions, indices, tolerance = 1e-4) {
  const welded = new Uint32Array(positions.length / 3);
  const points = [];
  const buckets = new Map();
  const scale = 1 / tolerance;
  for (let i = 0; i < welded.length; ++i) {
    const point = Array.from(positions.subarray(i * 3, i * 3 + 3));
    const key = point.map((value) => Math.round(value * scale)).join(",");
    let id = buckets.get(key);
    if (id === undefined) {
      id = points.length;
      buckets.set(key, id);
      points.push(point);
    }
    welded[i] = id;
  }

  const edgeCounts = new Map();
  for (let i = 0; i < indices.length; i += 3) {
    const triangle = [welded[indices[i]], welded[indices[i + 1]], welded[indices[i + 2]]];
    for (let j = 0; j < 3; ++j) {
      const a = triangle[j],
        b = triangle[(j + 1) % 3];
      if (a === b) continue;
      const key = a < b ? `${a},${b}` : `${b},${a}`;
      edgeCounts.set(key, (edgeCounts.get(key) || 0) + 1);
    }
  }

  const adjacency = new Map();
  for (const [key, count] of edgeCounts) {
    if (count !== 1) continue;
    const [a, b] = key.split(",").map(Number);
    if (!adjacency.has(a)) adjacency.set(a, []);
    if (!adjacency.has(b)) adjacency.set(b, []);
    adjacency.get(a).push(b);
    adjacency.get(b).push(a);
  }

  const openings = [];
  const seen = new Set();
  for (const start of adjacency.keys()) {
    if (seen.has(start)) continue;
    const stack = [start],
      component = [];
    seen.add(start);
    while (stack.length) {
      const current = stack.pop();
      component.push(current);
      for (const next of adjacency.get(current) || []) {
        if (!seen.has(next)) {
          seen.add(next);
          stack.push(next);
        }
      }
    }
    const min = [Infinity, Infinity, Infinity],
      max = [-Infinity, -Infinity, -Infinity];
    for (const id of component) {
      for (let axis = 0; axis < 3; ++axis) {
        min[axis] = Math.min(min[axis], points[id][axis]);
        max[axis] = Math.max(max[axis], points[id][axis]);
      }
    }
    const span = max.map((value, axis) => value - min[axis]);
    if (component.length >= 20 && Math.max(...span) < 0.5) {
      openings.push({ vertices: component.length, min, max });
    }
  }
  return openings;
}

assert(
  exe && input,
  "Usage: node step_regression.cjs <gmesh.exe> <input.stp> [face-count]",
);
for (const [name, linear, angular] of [
  ["lowest", 0.05, 0.5],
  ["very_low", 0.025, 0.25],
  ["low", 0.01, 0.1],
  ["medium", 0.005, 0.05],
  ["high", 0.001, 0.02],
]) {
  const result = spawnSync(
    exe,
    [
      "convert",
      "--input",
      input,
      "--format",
      "step",
      "--output-stdio",
      "--linear-deflection-type",
      "bounding_box_ratio",
      "--linear-deflection",
      String(linear),
      "--angular-deflection",
      String(angular),
      "--parallel",
    ],
    { maxBuffer: 1024 ** 3, timeout: 120000 },
  );
  assert.ifError(result.error);
  assert.equal(result.status, 0, `${name}: ${result.stderr.toString()}`);
  let report,
    info,
    ended = false;
  const lengths = { 4: 0, 5: 0, 6: 0 };
  const chunks = { 4: [], 5: [], 6: [] };
  for (let offset = 0; offset < result.stdout.length; ) {
    const bytes = result.stdout;
    assert(offset + 24 <= bytes.length, "truncated frame header");
    assert.equal(bytes.toString("ascii", offset, offset + 4), "RMIP");
    const type = bytes.readUInt16LE(offset + 6);
    const size = Number(bytes.readBigUInt64LE(offset + 16));
    assert(size <= 64 * 1024 ** 2 && offset + 24 + size <= bytes.length);
    const payload = bytes.subarray(offset + 24, offset + 24 + size);
    if (type === 3) info = JSON.parse(payload.toString());
    if (type === 8) report = JSON.parse(payload.toString());
    if (type === 100) assert.fail(payload.toString());
    if (type in lengths) {
      lengths[type] += size;
      chunks[type].push(payload);
    }
    if (type === 9) ended = true;
    offset += 24 + size;
  }
  assert(ended && info && report && report.ok);
  assert(report.tessellatedFaceCount > 0, "missing face coverage diagnostics");
  assert.equal(
    report.tessellatedFaceCount + report.degenerateFaceCount,
    report.faceCount,
    "a non-degenerate CAD face was silently omitted",
  );
  if (expectedFaces) assert.equal(report.faceCount, Number(expectedFaces));
  assert.equal(lengths[4], report.vertexCount * 12);
  assert.equal(lengths[5], lengths[4]);
  assert.equal(lengths[6], report.triangleCount * 12);
  const positionsBuffer = Buffer.concat(chunks[4]);
  const normalsBuffer = Buffer.concat(chunks[5]);
  const indicesBuffer = Buffer.concat(chunks[6]);
  const positions = new Float32Array(
    positionsBuffer.buffer,
    positionsBuffer.byteOffset,
    positionsBuffer.length / 4,
  );
  const normals = new Float32Array(
    normalsBuffer.buffer,
    normalsBuffer.byteOffset,
    normalsBuffer.length / 4,
  );
  const indices = new Uint32Array(
    indicesBuffer.buffer,
    indicesBuffer.byteOffset,
    indicesBuffer.length / 4,
  );
  const longEdges = new Map();
  let holePatchArea = 0;
  let holePatchTriangles = 0;
  for (let i = 0; i < indices.length; i += 3) {
    const ids = [indices[i], indices[i + 1], indices[i + 2]];
    const [a, b, c] = ids.map((id) => id * 3);
    const x1 = positions[b] - positions[a],
      y1 = positions[b + 1] - positions[a + 1],
      z1 = positions[b + 2] - positions[a + 2];
    const x2 = positions[c] - positions[a],
      y2 = positions[c + 1] - positions[a + 1],
      z2 = positions[c + 2] - positions[a + 2];
    const nx = y1 * z2 - z1 * y2,
      ny = z1 * x2 - x1 * z2,
      nz = x1 * y2 - y1 * x2;
    if (
      ids.every((id) => {
        const offset = id * 3;
        return (
          positions[offset] >= 87.46 &&
          positions[offset] <= 87.61 &&
          positions[offset + 1] >= 18.29 &&
          positions[offset + 1] <= 18.32 &&
          positions[offset + 2] >= 26.82 &&
          positions[offset + 2] <= 26.89
        );
      })
    ) {
      holePatchArea += Math.hypot(nx, ny, nz) * 0.5;
      holePatchTriangles++;
    }
    const dot =
      nx * (normals[a] + normals[b] + normals[c]) +
      ny * (normals[a + 1] + normals[b + 1] + normals[c + 1]) +
      nz * (normals[a + 2] + normals[b + 2] + normals[c + 2]);
    assert(
      dot >= -Math.hypot(nx, ny, nz) * 0.3 - 1e-14,
      `${name}: triangle winding opposes smooth normals`,
    );
    for (let j = 0; j < 3; ++j) {
      const a = Math.min(ids[j], ids[(j + 1) % 3]),
        b = Math.max(ids[j], ids[(j + 1) % 3]);
      const length = Math.hypot(
        positions[a * 3] - positions[b * 3],
        positions[a * 3 + 1] - positions[b * 3 + 1],
        positions[a * 3 + 2] - positions[b * 3 + 2],
      );
      if (length <= 10) continue;
      const key = `${a},${b}`;
      const edge = longEdges.get(key) || { a, b, count: 0 };
      edge.count++;
      longEdges.set(key, edge);
    }
  }
  if (Number(expectedFaces) === 593) {
    // The original 27 mm slit remains detectable when two sides have different subdivisions.
    const boundary = [...longEdges.values()].filter((edge) => edge.count === 1);
    const point = (id) => Array.from(positions.subarray(id * 3, id * 3 + 3));
    let maxGap = 0;
    for (let i = 0; i < boundary.length; ++i) {
      const a = point(boundary[i].a),
        b = point(boundary[i].b);
      for (const t of [0.25, 0.5, 0.75]) {
        const sample = a.map((v, k) => v + (b[k] - v) * t);
        let nearest = Infinity;
        for (let j = 0; j < boundary.length; ++j) {
          if (i === j) continue;
          const p = point(boundary[j].a),
            q = point(boundary[j].b);
          const direction = p.map((v, k) => q[k] - v);
          const denominator = direction.reduce((s, v) => s + v * v, 0);
          const parameter = Math.max(
            0,
            Math.min(
              1,
              direction.reduce((s, v, k) => s + v * (sample[k] - p[k]), 0) /
                denominator,
            ),
          );
          nearest = Math.min(
            nearest,
            Math.hypot(
              ...sample.map((v, k) => v - p[k] - direction[k] * parameter),
            ),
          );
        }
        maxGap = Math.max(maxGap, nearest);
      }
    }
    assert(maxGap < 1e-4, `${name}: visible long seam ${maxGap} mm`);
    if (name === "medium") {
      assert(
        holePatchTriangles > 2 && holePatchArea > 0.001,
        `${name}: curved hole patch collapsed (${holePatchTriangles} triangles, ${holePatchArea} mm^2)`,
      );
      assert.deepEqual(
        findTinyIsolatedOpenings(positions, indices),
        [],
        `${name}: visible tiny isolated opening`,
      );
    }
  }
  console.log(
    `PASS ${name}: ${report.faceCount} faces, ${report.recoveredFaceCount} recovered, ` +
      `${report.degenerateFaceCount} degenerate, ${report.triangleCount} triangles`,
  );
}
