// AMY's Gamma9001 drum sample banks for a test build (CARDENZA_AMY_BENCH,
// Library/Synth/Audio/Amy/AmyBench.h): the file tools/audio/gamma9001_blob.py makes, int16 samples
// read in place from flash. Never in a release.
    .section .rodata.cardenza_gamma9001, "a"
    .balign 4
    .global cardenza_gamma9001_pcm
    .type cardenza_gamma9001_pcm, @object
cardenza_gamma9001_pcm:
    .incbin CARDENZA_GAMMA9001_BIN
    .size cardenza_gamma9001_pcm, . - cardenza_gamma9001_pcm
