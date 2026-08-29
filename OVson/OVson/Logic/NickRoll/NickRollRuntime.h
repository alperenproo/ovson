#pragma once

namespace OVson::NickRoll {

// Watches the open /nick book, scores each distinct generated name, and emits
// configured notifications. No dataset is ever written.
//
// When auto-reroll is on it also presses TRY AGAIN for a name that misses the
// threshold, by sending the command the page itself puts behind that button --
// read, never assembled. It stops the moment a name passes, which is the whole
// point: the sound and the stop happen together.
void tick();
void shutdown();

} // namespace OVson::NickRoll
