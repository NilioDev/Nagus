# 3D models (`.hob`)

The combat view's ships, stations, weapons and effects are `.hob` files,
little-endian, with 32-bit numbers and floats. A ship class's model is
`<picture>_a.hob` (its picture name from `shiplist.sst`; `_b` and `_c` are
coarser versions) and its texture `<picture>_a.gif`, a 512×512 GIF (see
[images](images.md)). Some effect models (phasers, shields) have no
texture; their texture places are not numbers. What follows is what Nagus
reads; the rest of the file (parts and their positions, animation, lights)
is not understood.

## Header

| Offset | Size | What |
| --- | --- | --- |
| 0 | 4 | File size |
| 16 | 4 | Number of corners |
| 20 | 4 | Number of points |
| 60 | 4 | Offset of the root node |
| 64 | 4 | Offset of the corners |
| 68 | 4 | Offset of the points |
| 80 | 24 | Two corners of a box around the model (not used) |

## Points

20 bytes each: x, y, z (floats), then two numbers (the part the point
belongs to, and 0). +z is the model's front, +y up.

## Corners

40 bytes each, one for each corner of each face:

| Offset | Size | What |
| --- | --- | --- |
| 0 | 4 | Flags |
| 4 | 12 | Normal of the face (floats) |
| 16 | 8 | Place on the texture, u and v from 0 to 1 (floats) |
| 24 | 4 | Part |
| 28 | 4 | Index of the point |
| 32 | 8 | Not understood |

## Nodes and faces

The faces hang in a tree of 16-byte nodes: a type, a pointer, and two
offsets of further nodes (0 for none). Every node reachable from the root
is visited once; a node is never past the end of the file.

- Type 4 carries a face to draw.
- Type 5 carries a face that only splits space (a plane through the model,
  drawn in no colour), with the nodes on either side.
- Types 1, 13 and 14 only link other nodes.

A face:

| Offset | Size | What |
| --- | --- | --- |
| 0 | 4 | Kind |
| 4 | 4 | Flags (`0x100` for most faces) |
| 8 | 4 | See-through amount (float) |
| 12 | 4 | Material |
| 16 | 12 | Normal (floats) |
| 28 | 4 | Distance of the plane from the origin (float) |
| 32 | 12 | Not understood |
| 44 | 4 | Number of corners *n* |
| 48 | 4·*n* | The corners, each as its index times 68 |
| 48 + 4·*n* | 4 | Index of the first corner |

Faces of fewer than three corners (lights) are not drawn.

The reader refuses points that are not numbers, tables and nodes past the
end of the file, corners past their table and faces of more than 64
corners; texture places and normals that are not numbers count as 0.
