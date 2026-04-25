package rtaudiowrapper

import (
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"os"
	"time"
)

// Speaker plays a WAV file through the named output device.
// If deviceName is empty, the system default output device is used.
func Speaker(wavFilePath string, deviceName string) error {
	// Open the WAV file
	file, err := os.Open(wavFilePath)
	if err != nil {
		return fmt.Errorf("unable to find or open file: %w", err)
	}
	defer file.Close()

	// Read WAV header
	channels, sampleRate, bitsPerSample, dataSize, err := readWavHeader(file)
	if err != nil {
		return fmt.Errorf("failed to read WAV header: %w", err)
	}

	// Verify it's 16-bit format
	if bitsPerSample != 16 {
		return fmt.Errorf("only 16-bit WAV files are currently supported, got %d-bit", bitsPerSample)
	}

	fmt.Printf("Playing WAV file: %s\n", wavFilePath)
	fmt.Printf("Channels: %d, Sample Rate: %d Hz, Bits per Sample: %d\n", channels, sampleRate, bitsPerSample)

	// Create RtAudio instance
	audio, err := Create(APIUnspecified)
	if err != nil {
		return fmt.Errorf("failed to create audio interface: %w", err)
	}
	defer audio.Destroy()

	// Check for available devices
	devices, err := audio.Devices()
	if err != nil {
		return fmt.Errorf("failed to get devices: %w", err)
	}

	if len(devices) < 1 {
		return fmt.Errorf("no audio devices found")
	}

	// Resolve device ID: use the named device if provided, fall back to default.
	deviceID := audio.DefaultOutputDeviceId()
	outputDev := audio.DefaultOutputDevice()
	if deviceName != "" {
		for _, d := range devices {
			if d.Name == deviceName {
				outputDev = d
				deviceID = d.ID
				break
			}
		}
	}

	// Calculate total frames
	bytesPerSample := bitsPerSample / 8
	totalFrames := dataSize / (channels * bytesPerSample)

	// Initialize output data
	data := &OutputData{
		file:         file,
		channels:     channels,
		frameCounter: 0,
		totalFrames:  totalFrames,
	}


	// Set up stream parameters for output
	bufferFrames := uint(512)
	params := StreamParams{
		DeviceID:     uint(deviceID),
		NumChannels:  uint(outputDev.NumOutputChannels),
		FirstChannel: 0,
	}

	// Output callback function
  cb := func(out, in Buffer, dur time.Duration, status StreamStatus) int {
      outputData := out.Int16()
      if outputData == nil {
          return 0
      }

      nFrames := out.Len()
      outChannels := int(params.NumChannels)

      samplesToRead := nFrames * data.channels
      buffer := make([]int16, samplesToRead)

      err := binary.Read(data.file, binary.LittleEndian, buffer)
      if err != nil {
          if errors.Is(err, os.ErrClosed) || errors.Is(err, io.EOF) || errors.Is(err, io.ErrUnexpectedEOF) {
              clear(outputData)
              return 1
          }
          clear(outputData)
          return 1
      }

      // Expand channels: e.g. mono -> stereo, or pass through if counts match
      if data.channels == outChannels {
          copy(outputData, buffer)
      } else {
          for f := 0; f < nFrames; f++ {
              for ch := 0; ch < outChannels; ch++ {
                  srcCh := ch % data.channels  // cycles mono across all output channels
                  outputData[f*outChannels+ch] = buffer[f*data.channels+srcCh]
              }
          }
      }

      data.frameCounter += nFrames

      if status&StatusOutputUnderflow != 0 {
          fmt.Println("\nWARNING: Output underflow detected!")
      }

      return 0
  }

	err = audio.Open(&params, nil, FormatInt16, uint(sampleRate), bufferFrames, cb, nil)
	if err != nil {
		return fmt.Errorf("failed to open audio stream: %w", err)
	}
	defer audio.Close()

	fmt.Printf("Starting playback (buffer frames = %d)...\n", bufferFrames)

	err = audio.Start()
	if err != nil {
		return fmt.Errorf("failed to start audio stream: %w", err)
	}

	// Wait for playback to complete
	for audio.IsRunning() {
		time.Sleep(100 * time.Millisecond)
		progress := float64(data.frameCounter) / float64(data.totalFrames) * 100.0
		fmt.Printf("\rProgress: %.1f%% (%d / %d frames)", progress, data.frameCounter, data.totalFrames)
	}

	fmt.Printf("\n\nPlayback complete. Played %d frames.\n", data.frameCounter)

	return nil
}
