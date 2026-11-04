// Old project files the stress test loads on the board (Applications/Synth/StressTest.cpp): a version 1
// file with presets that do not exist (from tests/core_test.cpp) and DEMO-2 of the 0.1.0 release, format 6,
// as a user sent it (tests/groove_ui_test.cpp kReleaseProject).
// clang-format off
constexpr const char* kStressFixtureV1 = R"FIXTURE(CARDENZA_PROJECT 1
BUILD CardenzaSynth 1
UI 1 -1 2 3 1 9
TRACK 0 1 "factory.1" "user.1" "Brass" ""
PARAM 1
"global.feedback" 5
TRACK 1 2 "factory.2" "" "" ""
PARAM 0
TRACK 2 3 "factory.3" "" "" "kick.wav"
PARAM 0
TRACK 3 4 "kit.1" "" "" ""
PARAM 0
SEQ 128
T 2 1 110 24 1 0
NOTES 2
0 0 60 100 24 0
1 4 64 90 48 2
T 1 0 100 48 1 1
NOTES 0
T 1 0 110 24 0 0
NOTES 0
T 1 0 110 24 1 0
NOTES 0
END
)FIXTURE";
constexpr const char* kStressFixtureRelease = R"FIXTURE(CARDENZA_PROJECT 6
BUILD CardenzaSynth 1
UI 0 -1 4 0 1 15
MASTER 100
MIX 0 100 0 0
MIX 1 100 0 0
MIX 2 100 0 0
MIX 3 100 0 0
SYSTEM 180
FX 12 7 56 72 100 76 52 36 100 0 64 64 0
KEYS 0 110
PLAYFX 0 0 0 0 3 1 50 0
PLAYFX 1 0 0 0 3 1 50 0
PLAYFX 2 0 0 0 3 1 50 0
PLAYFX 3 0 0 0 3 1 50 0
TRACK 0 4 "drums.808" "drums.808" "808 Kit" ""
PARAM 16
"clap.level" 100
"closed_hat.level" 100
"cowbell.level" 100
"fx.chorus" 2
"fx.crush" 0
"fx.delay" 0
"fx.drive" 12
"fx.filter" 64
"fx.resonance" 20
"fx.reverb" 40
"kick.level" 127
"maraca.level" 100
"master.level" 100
"open_hat.level" 100
"snare.level" 100
"tom.level" 100
TRACK 1 2 "juno.a68" "juno.a68" "Synth Pad" ""
PARAM 29
"dco.lfo" 5
"dco.noise" 0
"dco.pulse" 1
"dco.pwm" 43
"dco.pwm_mode" 0
"dco.range" 1
"dco.saw" 0
"dco.sub" 37
"env.attack" 0
"env.decay" 65
"env.release" 70
"env.sustain" 22
"fx.chorus" 2
"fx.crush" 0
"fx.delay" 0
"fx.drive" 0
"fx.filter" 64
"fx.resonance" 20
"fx.reverb" 0
"lfo.delay" 0
"lfo.rate" 56
"vca.level" 127
"vca.mode" 0
"vcf.cutoff" 37
"vcf.env" 84
"vcf.keyboard" 127
"vcf.lfo" 0
"vcf.polarity" 0
"vcf.resonance" 0
TRACK 2 2 "juno.b78" "juno.b78" "Brassy Organ" ""
PARAM 29
"dco.lfo" 0
"dco.noise" 0
"dco.pulse" 1
"dco.pwm" 68
"dco.pwm_mode" 0
"dco.range" 0
"dco.saw" 1
"dco.sub" 56
"env.attack" 0
"env.decay" 36
"env.release" 0
"env.sustain" 109
"fx.chorus" 1
"fx.crush" 0
"fx.delay" 0
"fx.drive" 0
"fx.filter" 64
"fx.resonance" 20
"fx.reverb" 0
"lfo.delay" 0
"lfo.rate" 16
"vca.level" 127
"vca.mode" 0
"vcf.cutoff" 8
"vcf.env" 87
"vcf.keyboard" 52
"vcf.lfo" 0
"vcf.polarity" 0
"vcf.resonance" 0
TRACK 3 3 "sampler.empty" "sampler.empty" "Empty Slot" ""
PARAM 18
"amp.level" 96
"env.attack" 4
"env.release" 18
"filter.cutoff" 96
"filter.resonance" 18
"fx.chorus" 0
"fx.crush" 0
"fx.delay" 0
"fx.drive" 0
"fx.filter" 64
"fx.resonance" 20
"fx.reverb" 0
"sample.end" 127
"sample.gain" 96
"sample.loop" 0
"sample.root" 60
"sample.start" 0
"sample.tune" 0
SEQ 100 50
T 3 2 110 24 1 0 16
NOTES 14
0 0 36 127 96 0
0 4 42 110 28 0
0 10 36 110 31 2
0 12 42 110 24 0
1 0 36 110 20 1
1 1 36 110 22 2
1 6 39 110 25 2
1 8 36 110 24 0
1 12 42 110 24 0
2 0 36 110 24 0
2 4 39 110 24 0
2 8 36 110 24 0
2 10 36 110 24 0
2 12 39 110 24 0
T 4 2 110 24 1 0 16
NOTES 28
0 0 73 110 72 0
0 2 80 110 24 0
0 4 76 110 24 0
0 6 73 110 24 0
0 10 75 110 24 0
0 12 76 110 24 0
0 14 68 110 36 0
1 0 69 110 72 0
1 2 80 110 24 0
1 4 76 110 24 0
1 6 73 110 24 0
1 10 76 110 24 0
1 12 78 110 24 0
1 14 73 110 36 0
2 0 66 110 72 0
2 2 80 110 24 0
2 4 76 110 24 0
2 6 73 110 24 0
2 10 78 110 24 0
2 12 80 110 24 0
2 14 73 110 36 0
3 0 74 110 72 0
3 2 78 110 24 0
3 4 81 110 24 0
3 6 83 110 24 0
3 10 78 110 24 0
3 12 81 110 24 0
3 14 83 110 36 0
T 1 0 110 24 1 0 16
NOTES 0
T 1 0 110 24 1 0 16
NOTES 0
SONG 4 1
R 2 0 0 0 2
R 2 1 0 0 2
R 2 2 0 0 2
R 2 3 0 0 2
END
)FIXTURE";
// clang-format on
