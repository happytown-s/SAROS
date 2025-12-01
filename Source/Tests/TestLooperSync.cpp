#include <iostream>
#include <vector>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../LooperAudio.h"

// A simple test harness that links against LooperAudio and JUCE
// This test is intended to be run in an environment where JUCE is available.

int main() {
    std::cout << "Starting TestLooperSync..." << std::endl;

    // 1. Create Looper with 44100Hz sample rate and generous buffer
    LooperAudio looper(44100.0, 44100 * 10);

    // 2. Setup Track 1 (Master)
    looper.addTrack(1);
    looper.startRecording(1);

    // Create input with 100 samples of DC 1.0
    juce::AudioBuffer<float> input(1, 100);
    for (int i=0; i<100; ++i) input.setSample(0, i, 1.0f);

    juce::AudioBuffer<float> output(1, 100);

    // Record first track to establish master loop length of 100 samples
    looper.processBlock(output, input);
    looper.stopRecording(1);
    looper.startPlaying(1);

    // Now masterLoopLength is 100.

    // 3. Setup Track 2 (Slave)
    looper.addTrack(2);
    looper.startRecording(2);

    // 4. Create input larger than master loop length
    // Master loop is 100. Input block is 200.
    juce::AudioBuffer<float> input2(1, 200);
    for (int i=0; i<100; ++i) input2.setSample(0, i, 0.5f); // First 100 samples are 0.5
    for (int i=100; i<200; ++i) input2.setSample(0, i, 0.9f); // Next 100 samples are 0.9

    juce::AudioBuffer<float> output2(1, 200);

    // This call should record only the first 100 samples (0.5) and stop.
    // Before the fix, it would overwrite with the next 100 samples (0.9).
    looper.processBlock(output2, input2);

    // 5. Verify the content of Track 2
    // We process another block to read back what was recorded in Track 2.
    juce::AudioBuffer<float> input3(1, 100); // Silence input
    juce::AudioBuffer<float> output3(1, 100);

    looper.processBlock(output3, input3);

    // Track 1 is playing 1.0f.
    // Track 2 should be playing 0.5f (if correct) or something else (if buggy).
    // Expected Output: 1.0 + 0.5 = 1.5.

    float sample0 = output3.getSample(0, 0);
    std::cout << "Output sample 0: " << sample0 << std::endl;

    if (std::abs(sample0 - 1.5f) < 0.001f) {
        std::cout << "Test Passed: Correctly recorded first 100 samples." << std::endl;
        return 0;
    } else {
        std::cout << "Test Failed: Expected 1.5, got " << sample0 << std::endl;
        return 1;
    }
}
