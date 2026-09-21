#if defined(__aarch64__) && defined(SOUNDTRACK_USE_ALSA)
#include "third_party/splayerapi-v5/src/audio_output_alsa.c"
#else
#include "third_party/splayerapi-v5/src/audio_output_dummy.c"
#endif
