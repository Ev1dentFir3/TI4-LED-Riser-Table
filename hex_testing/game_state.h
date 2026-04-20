#pragma once

#include <vector>
#include <queue>
#include <algorithm>

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

// Edge hexes in clockwise order starting from hex 0
const int EDGE_HEX_ORDER[] = {
  0, 10, 11, 25, 26, 42, 43, 55, 56, 57, 58, 59, 60, 
  50, 49, 35, 34, 18, 17, 5, 4, 3, 2, 1
};
const int NUM_EDGE_HEXES = 24;

// =============================================================================
// Helper structure to store hex/direction pairs for LED control
// =============================================================================
struct HexDirection {
  int hex;        // Hex ID (0-60)
  int direction;  // Direction (0-5) where the edge is
};

// =============================================================================
// Basic Neighbor Functions
// =============================================================================

// Returns the neighbor hex id in a given direction, or -1.
inline int hexNeighbor(int hex, int dir) {
  if (hex < 0 || hex >= 61 || dir < 0 || dir > 5) return -1;
  return HEX_NEIGHBORS[hex][dir];
}

// Check if a hex is on the edge (has at least one -1 neighbor)
inline bool isEdgeHex(int hexId) {
  if (hexId < 0 || hexId >= 61) return false;
  for (int i = 0; i < 6; i++) {
    if (HEX_NEIGHBORS[hexId][i] == -1) return true;
  }
  return false;
}

// Get all valid neighbors of a hex
inline std::vector<int> getValidNeighbors(int hexId) {
  std::vector<int> validNeighbors;
  if (hexId < 0 || hexId >= 61) return validNeighbors;
  
  for (int i = 0; i < 6; i++) {
    if (HEX_NEIGHBORS[hexId][i] != -1) {
      validNeighbors.push_back(HEX_NEIGHBORS[hexId][i]);
    }
  }
  return validNeighbors;
}

// =============================================================================
// Edge Finding Functions
// =============================================================================

// Find the index of a hex in the edge order array (-1 if not an edge hex)
inline int getEdgeIndex(int hexId) {
  for (int i = 0; i < NUM_EDGE_HEXES; i++) {
    if (EDGE_HEX_ORDER[i] == hexId) return i;
  }
  return -1;
}

// Find all closest edge hexes to a source hex using BFS
inline std::vector<int> findClosestEdgeHexes(int sourceHex) {
  std::vector<int> closestEdges;
  if (sourceHex < 0 || sourceHex >= 61) return closestEdges;
  
  // If source is already on edge, return it
  if (isEdgeHex(sourceHex)) {
    closestEdges.push_back(sourceHex);
    return closestEdges;
  }
  
  // BFS to find closest edge hexes
  std::vector<int> distances(61, -1);
  std::queue<int> q;
  q.push(sourceHex);
  distances[sourceHex] = 0;
  
  int minDist = 999;
  
  while (!q.empty()) {
    int current = q.front();
    q.pop();
    
    // Check if this is an edge hex
    if (isEdgeHex(current) && current != sourceHex) {
      if (distances[current] < minDist) {
        minDist = distances[current];
        closestEdges.clear();
        closestEdges.push_back(current);
      } else if (distances[current] == minDist) {
        closestEdges.push_back(current);
      }
    }
    
    // Explore neighbors
    std::vector<int> neighbors = getValidNeighbors(current);
    for (int neighbor : neighbors) {
      if (distances[neighbor] == -1) {
        distances[neighbor] = distances[current] + 1;
        q.push(neighbor);
      }
    }
  }
  
  return closestEdges;
}

// Find the "middle" hex from a list of edge hexes based on clockwise order
inline int findMiddleEdgeHex(const std::vector<int>& edgeHexes) {
  if (edgeHexes.empty()) return -1;
  if (edgeHexes.size() == 1) return edgeHexes[0];
  
  // Get indices of all edge hexes in the clockwise order
  std::vector<int> indices;
  for (int hex : edgeHexes) {
    int idx = getEdgeIndex(hex);
    if (idx != -1) {
      indices.push_back(idx);
    }
  }
  
  if (indices.empty()) return -1;
  
  // Sort the indices
  std::sort(indices.begin(), indices.end());
  
  // Handle wrap-around case (e.g., indices like {0, 1, 23})
  int maxGap = 0;
  int gapStartIdx = 0;
  
  for (size_t i = 0; i < indices.size(); i++) {
    int next = (i + 1) % indices.size();
    int gap = (indices[next] - indices[i] + NUM_EDGE_HEXES) % NUM_EDGE_HEXES;
    if (gap > maxGap) {
      maxGap = gap;
      gapStartIdx = next;
    }
  }
  
  // If the max gap is large, we have wrap-around - rotate to make contiguous
  if (maxGap > (int)indices.size()) {
    std::rotate(indices.begin(), indices.begin() + gapStartIdx, indices.end());
  }
  
  // Return the middle index
  int middleIdx = indices[indices.size() / 2];
  return EDGE_HEX_ORDER[middleIdx];
}

// Main function: Find the center edge hex closest to the source
inline int findCenterEdgeHex(int sourceHex) {
  std::vector<int> closestEdges = findClosestEdgeHexes(sourceHex);
  return findMiddleEdgeHex(closestEdges);
}

// =============================================================================
// Edge Direction Functions (for LED control)
// =============================================================================

// Get all edge directions (where neighbor is -1) for a specific hex
inline std::vector<int> getEdgeDirections(int hexId) {
  std::vector<int> edgeDirs;
  if (hexId < 0 || hexId >= 61) return edgeDirs;
  
  for (int dir = 0; dir < 6; dir++) {
    if (HEX_NEIGHBORS[hexId][dir] == -1) {
      edgeDirs.push_back(dir);
    }
  }
  return edgeDirs;
}

// Get all hex/direction pairs for the center edge hex and its immediate neighbors
// This creates a "strip" of edge faces for LED highlighting
inline std::vector<HexDirection> getEdgeHighlightStrip(int sourceHex) {
  std::vector<HexDirection> strip;
  
  // Find the center edge hex
  int centerEdgeHex = findCenterEdgeHex(sourceHex);
  if (centerEdgeHex == -1) return strip;
  
  // Get the edge index to find neighbors in the edge order
  int centerIdx = getEdgeIndex(centerEdgeHex);
  if (centerIdx == -1) return strip;
  
  // Find the two adjacent edge hexes in clockwise order
  int prevIdx = (centerIdx - 1 + NUM_EDGE_HEXES) % NUM_EDGE_HEXES;
  int nextIdx = (centerIdx + 1) % NUM_EDGE_HEXES;
  
  int prevEdgeHex = EDGE_HEX_ORDER[prevIdx];
  int nextEdgeHex = EDGE_HEX_ORDER[nextIdx];
  
  // Collect all edge directions from these three hexes
  std::vector<int> hexesToProcess = {prevEdgeHex, centerEdgeHex, nextEdgeHex};
  
  for (int hex : hexesToProcess) {
    std::vector<int> edgeDirs = getEdgeDirections(hex);
    for (int dir : edgeDirs) {
      strip.push_back({hex, dir});
    }
  }
  
  return strip;
}

// =============================================================================
// Multi-Player Edge Zone Functions
// =============================================================================

// Structure to hold each player's edge zone
struct PlayerEdgeZone {
  int playerIndex;              // Which player (use -1 for unused)
  std::vector<int> edgeHexes;   // Edge hexes closest to this player
};

// Helper: BFS distance from one hex to another
inline int findHexDistance(int fromHex, int toHex) {
  if (fromHex == toHex) return 0;
  if (fromHex < 0 || fromHex >= 61 || toHex < 0 || toHex >= 61) return 999;
  
  std::vector<int> distances(61, -1);
  std::queue<int> q;
  q.push(fromHex);
  distances[fromHex] = 0;
  
  while (!q.empty()) {
    int current = q.front();
    q.pop();
    
    if (current == toHex) {
      return distances[toHex];
    }
    
    std::vector<int> neighbors = getValidNeighbors(current);
    for (int neighbor : neighbors) {
      if (distances[neighbor] == -1) {
        distances[neighbor] = distances[current] + 1;
        q.push(neighbor);
      }
    }
  }
  
  return 999; // No path found
}

// Divide the entire edge perimeter among multiple players based on proximity
// homeHexes: array of home hex IDs (e.g., {9, 27, 33, 51} for 4 players)
// numPlayers: how many players (4-8)
// Returns a vector of zones, one per player
inline std::vector<PlayerEdgeZone> divideEdgeByPlayers(const int* homeHexes, int numPlayers) {
  std::vector<PlayerEdgeZone> zones;
  
  if (numPlayers < 1 || numPlayers > 8) return zones;
  
  // Initialize a zone for each player
  for (int i = 0; i < numPlayers; i++) {
    zones.push_back({i, {}});
  }
  
  // For each edge hex, find which home hex is closest
  for (int edgeIdx = 0; edgeIdx < NUM_EDGE_HEXES; edgeIdx++) {
    int edgeHex = EDGE_HEX_ORDER[edgeIdx];
    
    int closestPlayer = 0;
    int minDistance = 999;
    
    // Check distance from this edge hex to each player's home hex
    for (int p = 0; p < numPlayers; p++) {
      int dist = findHexDistance(homeHexes[p], edgeHex);
      
      if (dist < minDistance) {
        minDistance = dist;
        closestPlayer = p;
      }
    }
    
    // Assign this edge hex to the closest player's zone
    zones[closestPlayer].edgeHexes.push_back(edgeHex);
  }
  
  return zones;
}

// Get all LED segments for a specific player's edge zone
// playerIndex: which player (0-7)
// zones: the result from divideEdgeByPlayers()
inline std::vector<HexDirection> getPlayerEdgeStrip(int playerIndex, 
                                                      const std::vector<PlayerEdgeZone>& zones) {
  std::vector<HexDirection> strip;
  
  if (playerIndex < 0) return strip;
  
  // Find this player's zone
  for (size_t z = 0; z < zones.size(); z++) {
    if (zones[z].playerIndex != playerIndex) continue;
    
    // Get all edge directions for all hexes in this player's zone
    for (size_t h = 0; h < zones[z].edgeHexes.size(); h++) {
      int edgeHex = zones[z].edgeHexes[h];
      std::vector<int> edgeDirs = getEdgeDirections(edgeHex);
      
      for (size_t d = 0; d < edgeDirs.size(); d++) {
        strip.push_back({edgeHex, edgeDirs[d]});
      }
    }
    break;
  }
  
  return strip;
}