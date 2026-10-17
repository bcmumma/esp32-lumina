#include "CellFrame.h"

#include <string.h>

namespace cells {

void reset(Frame &f) { memset(&f, 0, sizeof(f)); }

uint8_t encode(const uint8_t *rgb, uint8_t total, uint16_t frameId, CellPacket *out, uint16_t *bytes, uint8_t maxPackets) {
  if (total == 0 || total > MAX_CELLS) return 0;
  uint8_t n = 0;
  for (uint8_t first = 0; first < total && n < maxPackets; first = (uint8_t)(first + CELLS_PER_PACKET)) {
    const uint8_t count = (uint8_t)((total - first) < CELLS_PER_PACKET ? (total - first) : CELLS_PER_PACKET);
    CellPacket &p = out[n];
    p.magic = CELL_PACKET_MAGIC;
    p.version = CELL_PACKET_VERSION;
    p.totalCells = total;
    p.firstCell = first;
    p.count = count;
    p.frameId = frameId;
    memcpy(p.rgb, rgb + first * 3, count * 3U);
    bytes[n] = cell_packet_bytes(count);
    n++;
  }
  return n;
}

bool accept(Frame &f, const uint8_t *data, int len) {
  if (len < CELL_HEADER_BYTES) return false;
  CellPacket p;
  memcpy(&p, data, len > (int)sizeof(p) ? sizeof(p) : (size_t)len);
  if (p.magic != CELL_PACKET_MAGIC || p.version != CELL_PACKET_VERSION) return false;
  if (p.totalCells == 0 || p.totalCells > MAX_CELLS || p.count == 0 || p.count > CELLS_PER_PACKET) return false;
  if ((uint16_t)p.firstCell + p.count > p.totalCells) return false;
  if (len != cell_packet_bytes(p.count)) return false;
  if (p.totalCells != f.total) memset(f.rgb, 0, sizeof(f.rgb));  // layout changed: start clean
  f.total = p.totalCells;
  f.frameId = p.frameId;
  memcpy(f.rgb + p.firstCell * 3, p.rgb, p.count * 3U);
  f.parts++;
  return true;
}

void sample(const Frame &f, uint16_t i, uint16_t n, bool smooth, uint8_t &r, uint8_t &g, uint8_t &b) {
  r = g = b = 0;
  if (f.total == 0 || n == 0 || i >= n) return;
  // Position of LED i's centre in cell units, x256: ((i + 0.5) * total / n) * 256.
  const uint32_t pos = ((2U * i + 1U) * f.total * 256U) / (2U * n);
  if (!smooth || f.total == 1) {
    uint16_t c = (uint16_t)(pos >> 8);
    if (c >= f.total) c = f.total - 1;
    r = f.rgb[c * 3]; g = f.rgb[c * 3 + 1]; b = f.rgb[c * 3 + 2];
    return;
  }
  // Blend between cell centres (cell c's centre is at c + 0.5).
  int32_t x = (int32_t)pos - 128;
  if (x < 0) x = 0;
  uint16_t c0 = (uint16_t)(x >> 8);
  if (c0 >= f.total - 1) {
    c0 = f.total - 1;
    r = f.rgb[c0 * 3]; g = f.rgb[c0 * 3 + 1]; b = f.rgb[c0 * 3 + 2];
    return;
  }
  const uint16_t t = (uint16_t)(x & 0xFF);
  const uint8_t *a = f.rgb + c0 * 3, *z = a + 3;
  r = (uint8_t)((a[0] * (256U - t) + z[0] * t) >> 8);
  g = (uint8_t)((a[1] * (256U - t) + z[1] * t) >> 8);
  b = (uint8_t)((a[2] * (256U - t) + z[2] * t) >> 8);
}

}  // namespace cells
