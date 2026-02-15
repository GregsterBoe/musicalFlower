#include "ofApp.h"
#include <cmath>
#include <thread>
#include <sstream>
#include <array>

using namespace essentia;
using namespace essentia::standard;

//--------------------------------------------------------------
void ofApp::setup(){
	// Initialize Essentia
	essentia::init();
	ofLogNotice("Essentia") << "Essentia initialized successfully!";

	// Create Essentia algorithms
	AlgorithmFactory& factory = AlgorithmFactory::instance();

	windowing = factory.create("Windowing",
		"type", "hann",
		"zeroPadding", 0);

	spectrum = factory.create("Spectrum",
		"size", kFrameSize);

	pitchYinFFT = factory.create("PitchYinFFT",
		"frameSize", kFrameSize,
		"sampleRate", (Real)kSampleRate);

	// Pre-allocate buffers
	frame.resize(kFrameSize, 0.0f);
	windowedFrame.resize(kFrameSize, 0.0f);
	spectrumValues.resize(kFrameSize / 2 + 1, 0.0f);
	displaySpectrum.resize(kFrameSize / 2 + 1, 0.0f);
	audioInputBuffer.reserve(kSampleRate);

	// Setup audio input
	ofSoundStreamSettings settings;
	settings.setInListener(this);
	settings.sampleRate = kSampleRate;
	settings.numInputChannels = 1;
	settings.numOutputChannels = 0;
	settings.bufferSize = 512;
	soundStream.setup(settings);

	// Auto-route audio from active playback (runs in background thread)
	std::thread([this](){
		std::this_thread::sleep_for(std::chrono::seconds(1));
		autoRouteAudio();
	}).detach();

	// Setup flower field
	flowerField.setup(300);

	ofSetFrameRate(60);
}

//--------------------------------------------------------------
void ofApp::audioIn(ofSoundBuffer& input){
	float sumSquares = 0.0f;
	std::lock_guard<std::mutex> lock(audioMutex);
	size_t nFrames = input.getNumFrames();
	for(size_t i = 0; i < nFrames; i++){
		float sample = input[i];
		audioInputBuffer.push_back(sample);
		sumSquares += sample * sample;
	}
	rmsVolume = std::sqrt(sumSquares / nFrames);
}

//--------------------------------------------------------------
void ofApp::update(){
	// Grab audio samples from the audio thread
	{
		std::lock_guard<std::mutex> lock(audioMutex);
		processingBuffer.insert(processingBuffer.end(),
			audioInputBuffer.begin(), audioInputBuffer.end());
		audioInputBuffer.clear();
	}

	// Process if we have enough samples for a frame
	if((int)processingBuffer.size() >= kFrameSize){
		// Take the most recent kFrameSize samples
		size_t offset = processingBuffer.size() - kFrameSize;
		for(int i = 0; i < kFrameSize; i++){
			frame[i] = processingBuffer[offset + i];
		}

		// Discard old samples, keep last kFrameSize for overlap
		if(processingBuffer.size() > (size_t)kFrameSize * 2){
			processingBuffer.erase(processingBuffer.begin(),
				processingBuffer.begin() + processingBuffer.size() - kFrameSize);
		}

		// Essentia pipeline: frame -> Windowing -> Spectrum -> PitchYinFFT
		windowing->input("frame").set(frame);
		windowing->output("frame").set(windowedFrame);
		windowing->compute();

		spectrum->input("frame").set(windowedFrame);
		spectrum->output("spectrum").set(spectrumValues);
		spectrum->compute();

		pitchYinFFT->input("spectrum").set(spectrumValues);
		pitchYinFFT->output("pitch").set(currentPitch);
		pitchYinFFT->output("pitchConfidence").set(currentPitchConfidence);
		pitchYinFFT->compute();

		// Copy spectrum for visualization
		displaySpectrum.assign(spectrumValues.begin(), spectrumValues.end());

		// Smooth pitch and confidence for display
		// Higher alpha = faster reaction, less smoothing
		float alpha = 0.6f;
		if(currentPitchConfidence > 0.15f && currentPitch > 50.0f){
			smoothedPitch = smoothedPitch * (1.0f - alpha) + currentPitch * alpha;
			smoothedConfidence = smoothedConfidence * (1.0f - alpha) + currentPitchConfidence * alpha;
		} else {
			smoothedConfidence *= 0.95f;
		}

		// Compute spectral fullness: fraction of bins with significant energy
		int activeBins = 0;
		int totalBins = (int)spectrumValues.size();
		for(int i = 0; i < totalBins; i++){
			float db = 20.0f * std::log10(std::max(spectrumValues[i], 1e-10f));
			if(db > -65.0f) activeBins++;
		}
		float rawFullness = (totalBins > 0) ? (float)activeBins / totalBins : 0.0f;
		// Boost with power curve so typical music lands around 0.4-0.7
		spectralFullness = std::pow(rawFullness, 0.4f);

		// Add to melody history
		melodyHistory.push_back({smoothedPitch, smoothedConfidence});
		if(melodyHistory.size() > 400){
			melodyHistory.pop_front();
		}
	}

	// Update flower field with audio data
	flowerField.update(rmsVolume, smoothedPitch, smoothedConfidence, spectralFullness);
}

//--------------------------------------------------------------
void ofApp::draw(){
	if(debugMode){
		drawDebug();
	} else {
		drawMain();
	}
}

//--------------------------------------------------------------
void ofApp::drawMain(){
	ofBackground(0);

	// Draw flower field
	ofEnableAlphaBlending();
	flowerField.draw();
	ofDisableAlphaBlending();

	// Mode hint
	ofSetColor(50);
	if (flowerField.isReactiveMode()) {
		ofSetColor(0, 180, 120);
		ofDrawBitmapString("REACTIVE", 10, ofGetHeight() - 10);
		ofSetColor(50);
		ofDrawBitmapString("[D] debug  [SPACE] reactive mode", 100, ofGetHeight() - 10);
	} else {
		ofDrawBitmapString("[D] debug  [SPACE] reactive mode", 10, ofGetHeight() - 10);
	}
}

//--------------------------------------------------------------
void ofApp::drawDebug(){
	ofBackground(20);
	float w = ofGetWidth();
	float h = ofGetHeight();

	// --- Spectrum visualization (bottom third) ---
	float specY = h * 0.65f;
	float specH = h * 0.30f;
	int specSize = displaySpectrum.size();
	if(specSize > 0){
		int numBars = std::min(specSize, 512);
		float barW = w / (float)numBars;
		for(int i = 0; i < numBars; i++){
			float mag = displaySpectrum[i];
			float db = 20.0f * std::log10(std::max(mag, 1e-10f));
			float normalized = ofClamp((db + 80.0f) / 80.0f, 0.0f, 1.0f);
			float barH = normalized * specH;

			ofColor c;
			c.setHsb((int)(170 - normalized * 120) % 255, 220, 50 + normalized * 205);
			ofSetColor(c);
			ofDrawRectangle(i * barW, specY + specH - barH, barW - 1, barH);
		}
	}

	ofSetColor(150);
	ofDrawBitmapString("SPECTRUM", 10, specY - 5);

	// --- Melody trail (middle section) ---
	float melodyY = h * 0.20f;
	float melodyH = h * 0.40f;

	// Draw pitch range guides
	ofSetColor(40);
	float noteFreqs[] = {65.41f, 130.81f, 261.63f, 523.25f, 1046.50f, 2093.0f};
	const char* noteLabels[] = {"C2", "C3", "C4", "C5", "C6", "C7"};
	float minLog = std::log2(50.0f);
	float maxLog = std::log2(2500.0f);
	for(int i = 0; i < 6; i++){
		float logFreq = std::log2(noteFreqs[i]);
		float yPos = melodyY + melodyH - ((logFreq - minLog) / (maxLog - minLog)) * melodyH;
		ofDrawLine(0, yPos, w, yPos);
		ofSetColor(80);
		ofDrawBitmapString(noteLabels[i], w - 35, yPos - 3);
		ofSetColor(40);
	}

	// Draw melody trail — always draw if pitch is valid, use confidence for opacity
	if(melodyHistory.size() > 1){
		float stepX = w / 400.0f;
		for(size_t i = 1; i < melodyHistory.size(); i++){
			float pitch0 = melodyHistory[i-1].first;
			float conf0 = melodyHistory[i-1].second;
			float pitch1 = melodyHistory[i].first;
			float conf1 = melodyHistory[i].second;

			if(pitch0 > 50.0f && pitch1 > 50.0f && (conf0 > 0.05f || conf1 > 0.05f)){
				float logP0 = std::log2(pitch0);
				float logP1 = std::log2(pitch1);
				float y0 = melodyY + melodyH - ((logP0 - minLog) / (maxLog - minLog)) * melodyH;
				float y1 = melodyY + melodyH - ((logP1 - minLog) / (maxLog - minLog)) * melodyH;
				float x0 = (i - 1) * stepX;
				float x1 = i * stepX;
				// Confidence drives opacity: low confidence = faded, high = bright
				float avgConf = (conf0 + conf1) * 0.5f;
				int opacity = (int)(ofClamp(avgConf * 1.5f, 0.05f, 1.0f) * 255);
				ofSetColor(0, 255, 180, opacity);
				ofSetLineWidth(2);
				ofDrawLine(x0, y0, x1, y1);
			}
		}
		ofSetLineWidth(1);
	}

	ofSetColor(150);
	ofDrawBitmapString("MELODY", 10, melodyY - 5);

	// --- Pitch info (top area) ---
	float infoY = 25;

	if(smoothedConfidence > 0.1f && smoothedPitch > 50.0f){
		std::string noteName = pitchToNoteName(smoothedPitch);
		ofSetColor(0, 255, 180);
		ofDrawBitmapString(noteName, 10, infoY);
		ofSetColor(180);
		ofDrawBitmapString(ofToString(smoothedPitch, 1) + " Hz", 60, infoY);
	} else {
		ofSetColor(100);
		ofDrawBitmapString("--", 10, infoY);
	}

	// Confidence bar
	ofSetColor(100);
	ofDrawBitmapString("Conf:", 160, infoY);
	ofSetColor(60);
	ofDrawRectangle(210, infoY - 12, 100, 14);
	ofSetColor(0, 200, 150);
	ofDrawRectangle(210, infoY - 12, smoothedConfidence * 100, 14);

	// Volume bar
	ofSetColor(100);
	ofDrawBitmapString("Vol:", 330, infoY);
	ofSetColor(60);
	ofDrawRectangle(370, infoY - 12, 100, 14);
	float volDisplay = ofClamp(rmsVolume * 5.0f, 0.0f, 1.0f);
	ofSetColor(255, 180, 0);
	ofDrawRectangle(370, infoY - 12, volDisplay * 100, 14);

	// FPS and mode hint
	ofSetColor(80);
	ofDrawBitmapString("FPS: " + ofToString(ofGetFrameRate(), 0), w - 80, infoY);
	ofDrawBitmapString("[D] main mode  |  DEBUG", 10, h - 10);
}

//--------------------------------------------------------------
std::string ofApp::pitchToNoteName(float freqHz){
	if(freqHz <= 0.0f) return "--";
	const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	int midiNote = (int)std::round(69.0f + 12.0f * std::log2(freqHz / 440.0f));
	if(midiNote < 0 || midiNote > 127) return "--";
	int noteIndex = midiNote % 12;
	int octave = (midiNote / 12) - 1;
	return std::string(noteNames[noteIndex]) + ofToString(octave);
}

//--------------------------------------------------------------
void ofApp::autoRouteAudio(){
	// Helper to run a command and capture output
	auto exec = [](const char* cmd) -> std::string {
		std::array<char, 256> buffer;
		std::string result;
		FILE* pipe = popen(cmd, "r");
		if(!pipe) return "";
		while(fgets(buffer.data(), buffer.size(), pipe)){
			result += buffer.data();
		}
		pclose(pipe);
		return result;
	};

	// Find a sink that has active playback (sink-inputs)
	// Format: "index sink_index ..."
	std::string sinkInputs = exec("pactl list short sink-inputs 2>/dev/null");
	if(sinkInputs.empty()){
		ofLogWarning("AudioRoute") << "No active playback streams found";
		return;
	}

	// Parse the first sink-input to get the sink index it plays to
	std::istringstream iss(sinkInputs);
	int inputIdx, sinkIdx;
	if(!(iss >> inputIdx >> sinkIdx)){
		ofLogWarning("AudioRoute") << "Could not parse sink-inputs";
		return;
	}

	// Get the sink name for that index
	std::string cmd = "pactl list short sinks 2>/dev/null | awk '$1 == " + std::to_string(sinkIdx) + " {print $2}'";
	std::string sinkName = exec(cmd.c_str());
	// Trim whitespace
	while(!sinkName.empty() && (sinkName.back() == '\n' || sinkName.back() == '\r'))
		sinkName.pop_back();

	if(sinkName.empty()){
		ofLogWarning("AudioRoute") << "Could not find sink name for index " << sinkIdx;
		return;
	}

	std::string monitorName = sinkName + ".monitor";
	ofLogNotice("AudioRoute") << "Found active playback on: " << sinkName;
	ofLogNotice("AudioRoute") << "Routing input from: " << monitorName;

	// Find our source-outputs (recording streams from this process)
	std::string pid = std::to_string(getpid());
	std::string sourceOutputs = exec("pactl list short source-outputs 2>/dev/null");

	std::istringstream soStream(sourceOutputs);
	std::string line;
	while(std::getline(soStream, line)){
		std::istringstream lineStream(line);
		int soIdx;
		if(lineStream >> soIdx){
			std::string moveCmd = "pactl move-source-output " + std::to_string(soIdx)
				+ " " + monitorName + " 2>/dev/null";
			int ret = system(moveCmd.c_str());
			if(ret == 0){
				ofLogNotice("AudioRoute") << "Redirected source-output " << soIdx << " to " << monitorName;
			}
		}
	}
}

//--------------------------------------------------------------
void ofApp::exit(){
	soundStream.close();

	if(windowing) AlgorithmFactory::free(windowing);
	if(spectrum) AlgorithmFactory::free(spectrum);
	if(pitchYinFFT) AlgorithmFactory::free(pitchYinFFT);

	essentia::shutdown();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	if(key == 'd' || key == 'D'){
		debugMode = !debugMode;
	}
	if(key == ' '){
		flowerField.setReactiveMode(!flowerField.isReactiveMode());
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY){
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){
}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){
}
