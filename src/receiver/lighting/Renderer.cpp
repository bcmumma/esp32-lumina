#include "Renderer.h"
#include "Effects.h"
#include "LedOutput.h"

void render_strand(uint8_t s, const AudioLedPacket &p, bool newBeat) {
  if (s >= MAX_STRANDS || !configured[s] || !strips[s] || count_for(s) == 0) return;

  StrandSettings st = p.strand[s];

  // Hardware is configured separately by apply_hardware_from_packet().
  // Rendering uses the currently configured local strip.
  st.ledCount = count_for(s);

  if (FORCE_VISIBLE_IF_BAD_SETTINGS) {
    if (st.enabled == 0) st.enabled = 1;
    if (st.brightness == 0) st.brightness = SAFE_BRIGHTNESS;
    // Saturation 0 is intentional white. Never replace it with 255; the old
    // failsafe converted Test White into hue-0 red.
    if (st.mode == 0) st.mode = 10;
  }

  if (!st.enabled) {
    render_off(s);
    return;
  }

  switch (st.mode) {
    case 0: render_off(s); break;
    case 1: render_space_pulse(s, p, st); break;
    case 2: render_beat_strobe(s, p, st, newBeat); break;
    case 3: render_bass_comet(s, p, st); break;
    case 4: render_twinkle(s, p, st, newBeat); break;
    case 5: render_spectrum(s, p, st); break;
    case 6: render_vu(s, p, st); break;
    case 7: render_glitter_kick(s, p, st, newBeat); break;
    case 8: render_scanner(s, p, st); break;
    case 9: render_solid(s, p, st); break;
    case 10: render_debug_chase(s, p); break;
    case 11: render_band_vu(s, p, st, VU_SOURCE_BASS); break;
    case 12: render_band_vu(s, p, st, VU_SOURCE_MID); break;
    case 13: render_band_vu(s, p, st, VU_SOURCE_TREBLE); break;
    case 14: render_beat_tracker(s, p, st, newBeat); break;
    case 15: render_aurora_drift(s, p, st); break;
    case 16: render_starfield(s, p, st); break;
    case 17: render_neon_scanner(s, p, st); break;
    case 18: render_laser_sweep(s, p, st); break;
    case 19: render_rave_chase(s, p, st); break;
    case 20: render_bass_cannons(s, p, st); break;
    case 21: render_dnb_chopper(s, p, st); break;
    case 22: render_acid_chase(s, p, st); break;
    case 23: render_portal_pulse(s, p, st); break;
    case 24: render_spectrum_sparks(s, p, st); break;
    case 25: render_hyper_strobe(s, p, st); break;
    case 26: render_cyber_tunnel(s, p, st); break;
    case 27: render_white_lightning(s, p, st); break;
    case 28: render_dnb_split(s, p, st); break;
    case 29: render_rave_tiles(s, p, st); break;
    case 30: render_pulse_train(s, p, st); break;
    case 31: render_void_bloom(s, p, st); break;
    case 32: render_stereo_vu(s, p, st); break;
    case 33: render_kick_pump(s, p, st, newBeat); break;
    case 34: render_kick_ripple(s, p, st, newBeat); break;
    case 35: render_break_chopper(s, p, st, newBeat); break;
    case 36: render_acid_squelch(s, p, st); break;
    case 37: render_strobe(s, p, st); break;
    case 38: render_drop_strobe(s, p, st, newBeat); break;
    case CELLS_MODE: render_cells(s, p, st); break;
    default: render_solid(s, p, st); break;
  }
  apply_kick_flash(s, p);
}
