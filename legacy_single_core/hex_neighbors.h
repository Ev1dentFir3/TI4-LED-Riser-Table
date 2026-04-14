#pragma once

// =============================================================================
// TI4 Hex Riser - Hex Neighbor Table
// =============================================================================
// HEX_NEIGHBORS[hex][direction] — all 61 hexes, 6 directions each.
// -1 means no neighbor in that direction (edge of grid).
//
// Directions (clockwise from top, pointy-top orientation):
//   0 = top          3 = bottom
//   1 = top-right    4 = bottom-left
//   2 = bottom-right 5 = top-left
//
// Grid layout (9 columns, left→right):
//   Col 0:  5 hexes  (0– 4) top→bottom
//   Col 1:  6 hexes  (5–10) bottom→top
//   Col 2:  7 hexes (11–17) top→bottom
//   Col 3:  8 hexes (18–25) bottom→top
//   Col 4:  9 hexes (26–34) top→bottom  ← center column
//   Col 5:  8 hexes (35–42) bottom→top
//   Col 6:  7 hexes (43–49) top→bottom
//   Col 7:  6 hexes (50–55) bottom→top
//   Col 8:  5 hexes (56–60) top→bottom
//
// Neighbor derivation: visual y-coordinate system (adjacent cols offset by 0.5
// hex heights). Hexes in adjacent cols are neighbors when their visual-y coords
// differ by ±0.5 units.
// =============================================================================

const int HEX_NEIGHBORS[61][6] = {
  // {top, top-right, bottom-right, bottom, bottom-left, top-left}
  {-1, 10,  9,  1, -1, -1},  // hex  0  col0 y=2
  { 0,  9,  8,  2, -1, -1},  // hex  1  col0 y=3
  { 1,  8,  7,  3, -1, -1},  // hex  2  col0 y=4
  { 2,  7,  6,  4, -1, -1},  // hex  3  col0 y=5
  { 3,  6,  5, -1, -1, -1},  // hex  4  col0 y=6
  { 6, 16, 17, -1, -1,  4},  // hex  5  col1 y=6.5
  { 7, 15, 16,  5,  4,  3},  // hex  6  col1 y=5.5
  { 8, 14, 15,  6,  3,  2},  // hex  7  col1 y=4.5
  { 9, 13, 14,  7,  2,  1},  // hex  8  col1 y=3.5
  {10, 12, 13,  8,  1,  0},  // hex  9  col1 y=2.5
  {-1, 11, 12,  9,  0, -1},  // hex 10  col1 y=1.5
  {-1, 25, 24, 12, 10, -1},  // hex 11  col2 y=1
  {11, 24, 23, 13,  9, 10},  // hex 12  col2 y=2
  {12, 23, 22, 14,  8,  9},  // hex 13  col2 y=3
  {13, 22, 21, 15,  7,  8},  // hex 14  col2 y=4
  {14, 21, 20, 16,  6,  7},  // hex 15  col2 y=5
  {15, 20, 19, 17,  5,  6},  // hex 16  col2 y=6
  {16, 19, 18, -1, -1,  5},  // hex 17  col2 y=7
  {19, 33, 34, -1, -1, 17},  // hex 18  col3 y=7.5
  {20, 32, 33, 18, 17, 16},  // hex 19  col3 y=6.5
  {21, 31, 32, 19, 16, 15},  // hex 20  col3 y=5.5
  {22, 30, 31, 20, 15, 14},  // hex 21  col3 y=4.5
  {23, 29, 30, 21, 14, 13},  // hex 22  col3 y=3.5
  {24, 28, 29, 22, 13, 12},  // hex 23  col3 y=2.5
  {25, 27, 28, 23, 12, 11},  // hex 24  col3 y=1.5
  {-1, 26, 27, 24, 11, -1},  // hex 25  col3 y=0.5
  {-1, -1, 42, 27, 25, -1},  // hex 26  col4 y=0   (top of center col)
  {26, 42, 41, 28, 24, 25},  // hex 27  col4 y=1
  {27, 41, 40, 29, 23, 24},  // hex 28  col4 y=2
  {28, 40, 39, 30, 22, 23},  // hex 29  col4 y=3
  {29, 39, 38, 31, 21, 22},  // hex 30  col4 y=4   ← CENTER HEX
  {30, 38, 37, 32, 20, 21},  // hex 31  col4 y=5
  {31, 37, 36, 33, 19, 20},  // hex 32  col4 y=6
  {32, 36, 35, 34, 18, 19},  // hex 33  col4 y=7
  {33, 35, -1, -1, -1, 18},  // hex 34  col4 y=8   (bottom of center col)
  {36, 49, -1, -1, 34, 33},  // hex 35  col5 y=7.5
  {37, 48, 49, 35, 33, 32},  // hex 36  col5 y=6.5
  {38, 47, 48, 36, 32, 31},  // hex 37  col5 y=5.5
  {39, 46, 47, 37, 31, 30},  // hex 38  col5 y=4.5
  {40, 45, 46, 38, 30, 29},  // hex 39  col5 y=3.5
  {41, 44, 45, 39, 29, 28},  // hex 40  col5 y=2.5
  {42, 43, 44, 40, 28, 27},  // hex 41  col5 y=1.5
  {-1, -1, 43, 41, 27, 26},  // hex 42  col5 y=0.5
  {-1, -1, 55, 44, 41, 42},  // hex 43  col6 y=1
  {43, 55, 54, 45, 40, 41},  // hex 44  col6 y=2
  {44, 54, 53, 46, 39, 40},  // hex 45  col6 y=3
  {45, 53, 52, 47, 38, 39},  // hex 46  col6 y=4
  {46, 52, 51, 48, 37, 38},  // hex 47  col6 y=5
  {47, 51, 50, 49, 36, 37},  // hex 48  col6 y=6
  {48, 50, -1, -1, 35, 36},  // hex 49  col6 y=7
  {51, 60, -1, -1, 49, 48},  // hex 50  col7 y=6.5
  {52, 59, 60, 50, 48, 47},  // hex 51  col7 y=5.5
  {53, 58, 59, 51, 47, 46},  // hex 52  col7 y=4.5
  {54, 57, 58, 52, 46, 45},  // hex 53  col7 y=3.5
  {55, 56, 57, 53, 45, 44},  // hex 54  col7 y=2.5
  {-1, -1, 56, 54, 44, 43},  // hex 55  col7 y=1.5
  {-1, -1, -1, 57, 54, 55},  // hex 56  col8 y=2
  {56, -1, -1, 58, 53, 54},  // hex 57  col8 y=3
  {57, -1, -1, 59, 52, 53},  // hex 58  col8 y=4
  {58, -1, -1, 60, 51, 52},  // hex 59  col8 y=5
  {59, -1, -1, -1, 50, 51},  // hex 60  col8 y=6
};

// Returns the neighbor hex id in a given direction, or -1.
// Defined after HEX_NEIGHBORS so the compiler has seen the array.
inline int hexNeighbor(int hex, int dir) {
  if (hex < 0 || hex >= 61 || dir < 0 || dir > 5) return -1;
  return HEX_NEIGHBORS[hex][dir];
}
