package rtaudiowrapper

import (
	"os"
	"encoding/binary"
	"fmt"
)
// writeWavFile writes audio data to a WAV file
func WriteWavFile(filename string, data *RecordingData, sampleRate uint32, bitsPerSample uint32) error {
	file, err := os.Create(filename)
	if err != nil {
		return fmt.Errorf("failed to create file: %w", err)
	}
	defer file.Close()

	channels := uint32(data.channels)

	// Calculate sizes based on actual frames recorded
	dataSize := uint32(data.frameCounter) * channels * (bitsPerSample / 8)

	// Write RIFF header
	file.Write([]byte("RIFF"))
	binary.Write(file, binary.LittleEndian, uint32(36+dataSize)) // ChunkSize
	file.Write([]byte("WAVE"))

	// Write fmt subchunk
	file.Write([]byte("fmt "))
	binary.Write(file, binary.LittleEndian, uint32(16))                                    // Subchunk1Size (PCM)
	binary.Write(file, binary.LittleEndian, uint16(1))                                     // AudioFormat (PCM)
	binary.Write(file, binary.LittleEndian, uint16(channels))                              // NumChannels
	binary.Write(file, binary.LittleEndian, uint32(sampleRate))                            // SampleRate
	binary.Write(file, binary.LittleEndian, uint32(sampleRate*channels*(bitsPerSample/8))) // ByteRate
	binary.Write(file, binary.LittleEndian, uint16(channels*(bitsPerSample/8)))            // BlockAlign
	binary.Write(file, binary.LittleEndian, uint16(bitsPerSample))                         // BitsPerSample

	// Write data subchunk
	file.Write([]byte("data"))
	binary.Write(file, binary.LittleEndian, uint32(dataSize)) // Subchunk2Size

	// Write audio data from Go slice
	totalSamples := data.frameCounter * data.channels
	for i := 0; i < totalSamples; i++ {
		if err := binary.Write(file, binary.LittleEndian, data.buffer[i]); err != nil {
			return fmt.Errorf("failed to write sample: %w", err)
		}
	}

	return nil
}
