// Per-frame dispatch: picks each strand's render mode from the latest packet.
#pragma once

#include <Arduino.h>
#include "LuminaProtocol.h"

void render_strand(uint8_t s, const AudioLedPacket &p, bool newBeat);
